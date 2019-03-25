/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "udp-app-client.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <bitset>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpAppClientApplication");

NS_OBJECT_ENSURE_REGISTERED (UdpAppClient);

TypeId
UdpAppClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpAppClient")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<UdpAppClient> ()
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (12000),
                   MakeUintegerAccessor (&UdpAppClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&UdpAppClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&UdpAppClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&UdpAppClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of packets generated",
                    UintegerValue (1100),
                    MakeUintegerAccessor (&UdpAppClient::m_size),
                    MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UdpAppClient::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&UdpAppClient::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UdpAppClient::m_txTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&UdpAppClient::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

// Constructors
UdpAppClient::UdpAppClient ()
{
  NS_LOG_FUNCTION (this);
  m_sent_l = 0;
  m_sent_h = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;
  uint8_t packets[6144000] = {};
  packets[0] = 0;
  if (packets[0] == 0) {

  }
  uint8_t lowPackets[1024] = {};
  for (int i = 0; i < 1024; i++) {
    lowPackets[i] = 0;
  }
  if (lowPackets[0] == 0 ) {
    std::cout << "in constructor\n";
  }
  //uint8_t * packets2;

//  std::cout << "location of packets: " << &packets << "\n";
    
}

UdpAppClient::~UdpAppClient()
{

  NS_LOG_FUNCTION (this);
  m_socket = 0;

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
}

void 
UdpAppClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
UdpAppClient::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

void
UdpAppClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
UdpAppClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  std::thread::id this_id = std::this_thread::get_id();
  std::cout << "thread ID: " << this_id << "\n";

  FillInPacketArray(packets);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else if (Inet6SocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else
        {
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
        }
    }

  std::cout << "Start first send.\n";
  m_socket->SetRecvCallback (MakeCallback (&UdpAppClient::HandleRead, this));
  m_socket->SetAllowBroadcast (true);
  ScheduleTransmit (Seconds (0.));
}

void 
UdpAppClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  std::cout << "Stopping client.\n";
  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void 
UdpAppClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
UdpAppClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
UdpAppClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpAppClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpAppClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpAppClient::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &UdpAppClient::Send, this);
}

void 
UdpAppClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  std::thread::id this_id = std::this_thread::get_id();
  if (m_dataSize)
    {
      //std::cout << "m_dataSize\n";

      // MEMCPY the correct portion of packets to mdata
    	delete [] m_data;
      	m_data = new uint8_t [m_dataSize];
      	m_dataSize = m_dataSize;

      	int i = m_sent_l - 6000;
      	i = 1024 * i;

      	std::copy(packets + i, packets + i + 1024, m_data);

      	//std::cout << m_data << "\n";

      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "UdpAppClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "UdpAppClient::Send(): m_dataSize but no m_data");
      // std::cout << "Reached max packets: " << (m_sent_l == m_count) << "\n";
      p = Create<Packet> (m_data, m_dataSize);
      //p = Create<Packet> (m_size);
    }
  else
    {
    	// so the high entropy packets aren't longer b/c of additional copying
    	delete [] m_data;
      	m_data = new uint8_t [1024];
      	int i = m_sent_l * 1024;     	
      	std::copy(packets + i, packets + i + 1024, m_data);

      //
      // If m_dataSize is zero, the client has indicated that it doesn't care
      // about the data itself either by specifying the data size by setting
      // the corresponding attribute or by not calling a SetFill function.  In
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      
      //std::copy(lowPackets + 0, lowPackets + 1024, m_data);
      p = Create<Packet> (lowPackets, 1024);


      //p = Create<Packet> (m_size);
    }
  Address localAddress;
  m_socket->GetSockName (localAddress);
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  m_socket->Send (p);
  ++m_sent_l;
  // std::cout << "Sent packet " << m_sent_l << "\n"; // Print to check # of packets sent
  // if (m_sent_l < m_count)
  //   {
  //     ++m_sent_l;
  //   }
  // else
  //   {
  //     ++m_sent_h;
  //     // std::cout << "Sent high entropy packet " << m_sent_h << "\n"; // Print to check # of packets sent
  //   }
  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (InetSocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (m_peerAddress).GetPort ());
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetPort ());
    }

  if (m_sent_l <= m_count ) // from 1 to 
    {
      ScheduleTransmit (m_interval);
    }
  else if (m_sent_l == m_count + 1) // THE FIRST HIGH ENTROPY PACKET
    {
      std::cout << this_id << " sent " << m_sent_l << " low entropy packets. Send the next high entropy packets.\n";
      std::cout << m_count << " m_count\n";
      std::cout << "CLIENT starting wait\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      std::cout << "CLIENT done waiting, sending 1st high entropy\n";
      //TODO critical point
      m_dataSize = 1024;
      ScheduleTransmit (m_interval);  
      // ScheduleTransmit (Seconds (20.0));
    }
  else if (m_sent_l <= m_count*2) 
    {
      // std::cout << "Sending dummy message " << m_sent_h << "\n";
      ScheduleTransmit (m_interval);
    }
}

void
UdpAppClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
    }
}

void
UdpAppClient::FillInPacketArray(uint8_t packets[6144000])
{ 
  std::cout << "Random fill running";

    int i = 0;
//    int j;
    std::ifstream randomfile("randomfile");
    if (!randomfile.good()) { // if file does not already exist
        std::ofstream randomoutfile("randomfile"); // makes file and fills it in
        uint8_t buffer[768000]; // 128*6000 random 8-bit #s
        int fd = open("/dev/random", O_RDONLY);
        int size = read(fd, buffer, 768000); // put it in buffer
        size ++;
        //buffer now contains the random data
        close(fd);
        for (i = 0; i < 6000; ++i) { // 6000 packets: 1 line per
            for (int j = 0; j < 128; j++) { // 128 numbers * 8 bits = 1024
                randomoutfile << std::bitset<8>(buffer[(128 * i) + j]);
            }
            randomoutfile << "\n";
        }
        randomoutfile.close();
    }
    std::string line;
    i = 0;
    // reads file in a line at a time as a string
    // converts string to uint8_t* and puts in packets array
    while (std::getline(randomfile, line))
    {
        //std::istringstream iss(line);
        //cout << line;
        //cout << "\n";
        //cout << sizeof(line);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(line.c_str());
        for (int x = 0; x < 1024; x++) {
            packets[(i*1024) + x] = p[x];
        }


       //std::cout << sizeof(line) << "\n";
        //if (i > 2) {
        //    int x = i - 1;
        //    std::cout << i << " " << sizeof(&packets[i]) <<" " << packets[i] << "\n";
        //    std::cout << x << " " << sizeof(&packets[i-1]) <<" " << packets[i-1] << "\n";
        //    std::cout << i-2 << " " << packets[i-2] << "\n\n";
        //}
        i++;
    }
        

    //std::randomfile.close();
    //TODO ??

}
} // Namespace ns3
