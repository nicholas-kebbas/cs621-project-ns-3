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
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/ppp-header.h"
#include <chrono>

#include "udp-app-server.h"

using std::chrono::high_resolution_clock;
// using std::chrono::system_clock;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpAppServerApplication");

NS_OBJECT_ENSURE_REGISTERED (UdpAppServer);

TypeId
UdpAppServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpAppServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<UdpAppServer> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&UdpAppServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will receive.",
                   UintegerValue (6000),
                   MakeUintegerAccessor (&UdpAppServer::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&UdpAppServer::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&UdpAppServer::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("PacketSize", "Size of packets generated",
                      UintegerValue (100),
                      MakeUintegerAccessor (&UdpAppServer::m_size),
                      MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

UdpAppServer::UdpAppServer ()
{
  NS_LOG_FUNCTION (this);
  std::chrono::system_clock::time_point m_first_t;
  std::chrono::system_clock::time_point m_last_t;
  m_received_l = 0;
  m_received_h = 0;
  m_duration_l = 0;
  m_duration_h = 0;
  receivedAllLowEntropy = false;
}

UdpAppServer::~UdpAppServer()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
}

void
UdpAppServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
UdpAppServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      if (m_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      if (m_socket6->Bind (local6) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      if (addressUtils::IsMulticast (local6))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket6);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, local6);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  // std::cout << "Init contact with client...\n"; 
    // m_first_t = high_resolution_clock::now(); // Start clock when get first packet
  m_socket->SetRecvCallback (MakeCallback (&UdpAppServer::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&UdpAppServer::HandleRead, this));
}

void 
UdpAppServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  // typedef std::chrono::milliseconds ms;
  // typedef std::chrono::duration<float> fsec;
  // fsec fs = m_last_t - m_first_t;
  // m_duration_l = fs.count();
  // auto d = std::chrono::duration_cast<ms>(fs);
  std::cout << "low entropy:  " << m_duration_l << "ms\n"; // Take duration between first and last packets in seconds
  // std::cout << d.count() << "ms\n"; // Take duration between first and last packets in milliseconds
  std::cout << "high entropy: " << m_duration_h << "ms\n";
  if(m_duration_h - m_duration_l > 100) 
    {
      std::cout << "∆t_H − ∆t_L = " << m_duration_h - m_duration_l << "ms. Compression detected!\n";
    }
  else
    {
      std::cout << "∆t_H − ∆t_L = " << m_duration_h - m_duration_l << "ms. No compression detected.\n";
    }

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_socket6 != 0) 
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void 
UdpAppServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
  // std::cout << m_received_l << "\n";
  if (!receivedAllLowEntropy)
    {
      ++m_received_l;
      // std::cout << "Receieved le packet " << m_received_l << "\n";
      if(m_received_l == 1) 
        {
          std::cout << "Got first low entropy packet " << m_received_l << "\n";
          m_first_t = high_resolution_clock::now(); // Start clock when get first packet
        }
      else if (m_received_l < m_count)
        {
          m_last_t = high_resolution_clock::now();
          
        }
      else if (m_received_l == m_count)
        {
          m_last_t = high_resolution_clock::now();
          std::cout << "Got last low entropy packet " << m_received_l << "\n";
          typedef std::chrono::milliseconds ms;
          typedef std::chrono::duration<float> fsec;
          fsec fs = m_last_t - m_first_t;
          auto d = std::chrono::duration_cast<ms>(fs);
          m_duration_l = d.count();
          receivedAllLowEntropy = true;
        }
    }
  else
    {
      ++m_received_h;
      // std::cout << "Receieved he packet " << m_received_h << "\n";
      if(m_received_h == 1) 
        {
          std::cout << "Got first high entropy packet " << m_received_h << "\n";
          m_first_t = high_resolution_clock::now(); // Start clock when get first packet
        }
      else if (m_received_h < m_count)
        {
          m_last_t = high_resolution_clock::now();
        }
      else if(m_received_h == m_count)
        {
          m_last_t = high_resolution_clock::now();
          std::cout << "Got last high entropy packet " << m_received_h << "\n";
          typedef std::chrono::milliseconds ms;
          typedef std::chrono::duration<float> fsec;
          fsec fs = m_last_t - m_first_t;
          auto d = std::chrono::duration_cast<ms>(fs);
          m_duration_h = d.count();            
        }
    }
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      NS_LOG_LOGIC ("Responding...");
      Ptr<Packet> p = Create<Packet>(m_size);
      socket->SendTo (p, 0, from);

      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }
    }
    // std::cout << "Finished stuff: " << m_received_l << "\n";
}

} // Namespace ns3
