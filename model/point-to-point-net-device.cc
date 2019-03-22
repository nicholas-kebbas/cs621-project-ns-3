/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/net-device-queue-interface.h"
#include "point-to-point-net-device.h"
#include "point-to-point-channel.h"
#include "ppp-header.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cstdlib>
extern "C"{
#include "zlib.h"
}

using json = nlohmann::json;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointNetDevice");

NS_OBJECT_ENSURE_REGISTERED (PointToPointNetDevice);

TypeId 
PointToPointNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointNetDevice")
    .SetParent<NetDevice> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointNetDevice> ()
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&PointToPointNetDevice::SetMtu,
                                         &PointToPointNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&PointToPointNetDevice::m_address),
                   MakeMac48AddressChecker ())
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("32768b/s")),
                   MakeDataRateAccessor (&PointToPointNetDevice::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("InterframeGap", 
                   "The time to wait between packet (frame) transmissions",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PointToPointNetDevice::m_tInterframeGap),
                   MakeTimeChecker ())
    .AddAttribute ("Compression", 
                   "Should the application run compression on valid packets",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PointToPointNetDevice::compressionEnabled),
                   MakeBooleanChecker ())

    //
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    //
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_queue),
                   MakePointerChecker<Queue<Packet> > ())

    //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived "
                     "for transmission by this device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped "
                     "by the device before transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxDropTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macPromiscRxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was dropped "
                     "before being forwarded up the stack",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxDropTrace),
                     "ns3::Packet::TracedCallback")
#endif
    //
    // Trace sources at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun "
                     "transmitting over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been "
                     "completely transmitted over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxDropTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun "
                     "being received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxBeginTrace),
                     "ns3::Packet::TracedCallback")
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been "
                     "completely received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during reception",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxDropTrace),
                     "ns3::Packet::TracedCallback")

    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    // Note that there is really no difference between promiscuous and 
    // non-promiscuous traces in a point-to-point link.
    //
    .AddTraceSource ("Sniffer", 
                    "Trace source simulating a non-promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_snifferTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_promiscSnifferTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


PointToPointNetDevice::PointToPointNetDevice () 
  :
    m_txMachineState (READY),
    m_channel (0),
    m_linkUp (false),
    m_currentPkt (0)
{
  NS_LOG_FUNCTION (this);
  m_protocol = 0;
}

PointToPointNetDevice::~PointToPointNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
PointToPointNetDevice::AddHeader (Ptr<Packet> p, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << p << protocolNumber);
  PppHeader ppp;
  ppp.SetProtocol (EtherToPpp (protocolNumber));
  p->AddHeader (ppp);
}

bool
PointToPointNetDevice::ProcessHeader (Ptr<Packet> p, uint16_t& param)
{
  NS_LOG_FUNCTION (this << p << param);
  PppHeader ppp;
  p->RemoveHeader (ppp);
  param = PppToEther (ppp.GetProtocol ());
  return true;
}

void
PointToPointNetDevice::DoInitialize (void)
{
  if (m_queueInterface)
    {
      NS_ASSERT_MSG (m_queue != 0, "A Queue object has not been attached to the device");
      // connect the traced callbacks of m_queue to the static methods provided by
      // the NetDeviceQueue class to support flow control and dynamic queue limits.
      // This could not be done in NotifyNewAggregate because at that time we are
      // not guaranteed that a queue has been attached to the netdevice
      m_queueInterface->ConnectQueueTraces (m_queue, 0);
    }
  if (compressionEnabled)
    {
      std::cout << "Compression is enabled!\n";
    }
// Read config file; take inputstream from the file and put it all in json j
  std::ifstream jsonIn("./config.json");
  json j;
  jsonIn >> j;
// Print the pretty json to the terminal
  std::cout << std::setw(4) << j << std::endl;
  std::string protocol = "";

// Get the string value from protocolsToCompress and print it
  protocol = j["protocolsToCompress"].get<std::string>();
  std::istringstream buffer(protocol);
  buffer >> std::hex >> m_protocol;
  std::cout << "Protocol to compress: 0x" << std::hex << m_protocol << "\n";
  m_protocol = PppToEther(m_protocol);

  NetDevice::DoInitialize ();
}

void
PointToPointNetDevice::NotifyNewAggregate (void)
{
  NS_LOG_FUNCTION (this);
  if (m_queueInterface == 0)
    {
      Ptr<NetDeviceQueueInterface> ndqi = this->GetObject<NetDeviceQueueInterface> ();
      //verify that it's a valid netdevice queue interface and that
      //the netdevice queue interface was not set before
      if (ndqi != 0)
        {
          m_queueInterface = ndqi;
        }
    }
  NetDevice::NotifyNewAggregate ();
}

void
PointToPointNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_node = 0;
  m_channel = 0;
  m_receiveErrorModel = 0;
  m_currentPkt = 0;
  m_queue = 0;
  m_queueInterface = 0;
  NetDevice::DoDispose ();
}

void
PointToPointNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION (this);
  m_bps = bps;
}

void
PointToPointNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION (this << t.GetSeconds ());
  m_tInterframeGap = t;
}

bool
PointToPointNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  //
  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  //
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;
  m_currentPkt = p;
  m_phyTxBeginTrace (m_currentPkt);

  Time txTime = m_bps.CalculateBytesTxTime (p->GetSize ());
  Time txCompleteTime = txTime + m_tInterframeGap;

  NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds () << "sec");
  Simulator::Schedule (txCompleteTime, &PointToPointNetDevice::TransmitComplete, this);

  bool result = m_channel->TransmitStart (p, this, txTime);
  if (result == false)
    {
      m_phyTxDropTrace (p);
    }
  return result;
}

void
PointToPointNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION (this);

  //
  // This function is called to when we're all done transmitting a packet.
  // We try and pull another packet off of the transmit queue.  If the queue
  // is empty, we are done, otherwise we need to start transmitting the
  // next packet.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "PointToPointNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);
  m_currentPkt = 0;

  Ptr<Packet> p = m_queue->Dequeue ();
  if (p == 0)
    {
      NS_LOG_LOGIC ("No pending packets in device queue after tx complete");
      return;
    }

  //
  // Got another packet off of the queue, so start the transmit process again.
  //
  m_snifferTrace (p);
  m_promiscSnifferTrace (p);
  TransmitStart (p);
}

bool
PointToPointNetDevice::Attach (Ptr<PointToPointChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;
  m_channel->Attach (this);

  //
  // This device is up whenever it is attached to a channel.  A better plan
  // would be to have the link come up when both devices are attached, but this
  // is not done for now.
  //
  NotifyLinkUp ();
  return true;
}

void
PointToPointNetDevice::SetQueue (Ptr<Queue<Packet> > q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

void
PointToPointNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (this << em);
  m_receiveErrorModel = em;
}

void
PointToPointNetDevice::Receive (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
  uint16_t protocol = 0;
  // m_count++;
  // std::cout << "Receive packet " << m_count << "\n";
  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) ) 
    {
      // 
      // If we have an error model and it indicates that it is time to lose a
      // corrupted packet, don't forward this packet up, let it go.
      //
      m_phyRxDropTrace (packet);
    }
  else 
    {
      // 
      // Hit the trace hooks.  All of these hooks are in the same place in this 
      // device because it is so simple, but this is not usually the case in
      // more complicated devices.
      //
      m_snifferTrace (packet);
      m_promiscSnifferTrace (packet);
      m_phyRxEndTrace (packet);

      //
      // Trace sinks will expect complete packets, not packets without some of the
      // headers.
      //
      Ptr<Packet> originalPacket = packet->Copy ();

      if (compressionEnabled)
        {
          // Ptr<Packet> pCopy = originalPacket->Copy();
          // Packet checking stuff
          PppHeader header;
          // Packet p = packet.operator*();  // Get the packet from the Ptr
          // /*uint32_t packetBytes =*/ 
          packet->PeekHeader(header); // Get the header from the packet
          uint16_t currentProtocol = header.GetProtocol();
          // std::cout << "recv: got packet: 0x" << std::hex << currentProtocol << "\n";
          switch (currentProtocol)
            {
              case 0x4021:  // LZS
                {
                  std::cout << "recv: Got LZS packet: 0x" << std::hex << currentProtocol << "\n";

                  /* Decompress the compressed packet */
                  // Ether to PPP header
                  uint32_t packetSize = packet->GetSize();
                  std::cout << "Received As String 1: " << packet -> ToString() << "\n";
                  /* Allocate enough memory to the buffer so we don't get a seg fault */
                 //  uint32_t adjustedPacketSize = packetSize + 8;
                  uint8_t* buffer = new uint8_t[packetSize];
                  uint8_t* newBuffer = new uint8_t[packetSize];
                  packet -> CopyData(buffer, packetSize);
                  printf ("Size of Packet in Decompress: %d  \n", packetSize);
                  /* If we successfully serialized, do the next thing */
                  uint8_t* decompressedBuffer = Decompress(buffer, newBuffer, packetSize);
                  /* Create the new, decompressed packet. Change packet to point to that. */
                  packet = Create<Packet>(decompressedBuffer, packetSize);
                  std::cout << "Adding 0x0021\n";
                  AddHeader(packet, 0x0021);
                  std::cout << "Received As String 2: " << packet -> ToString() << "\n";
                  break;
                }
            }

          //
          // Strip off the point-to-point protocol header and forward this packet
          // up the protocol stack.  Since this is a simple point-to-point link,
          // there is no difference in what the promisc callback sees and what the
          // normal receive callback sees.
          //

          ProcessHeader (packet, protocol);

          if (!m_promiscCallback.IsNull ())
            {
              m_macPromiscRxTrace (originalPacket);
              m_promiscCallback (this, packet, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
            }

          m_macRxTrace (originalPacket);
          // m_rxCallback (this, pCopy, protocol, GetRemote ());
          m_rxCallback (this, packet, protocol, GetRemote ());
        } 
      else
        {
          //
          // Strip off the point-to-point protocol header and forward this packet
          // up the protocol stack.  Since this is a simple point-to-point link,
          // there is no difference in what the promisc callback sees and what the
          // normal receive callback sees.
          //
          // ProcessHeader (packet, protocol);
          ProcessHeader (packet, protocol);


          if (!m_promiscCallback.IsNull ())
            {
              m_macPromiscRxTrace (originalPacket);
              m_promiscCallback (this, packet, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
            }

          m_macRxTrace (originalPacket);
          m_rxCallback (this, packet, protocol, GetRemote ());
        }
    }
}

Ptr<Queue<Packet> >
PointToPointNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION (this);
  return m_queue;
}

void
PointToPointNetDevice::NotifyLinkUp (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

void
PointToPointNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (this);
  m_ifIndex = index;
}

uint32_t
PointToPointNetDevice::GetIfIndex (void) const
{
  return m_ifIndex;
}

Ptr<Channel>
PointToPointNetDevice::GetChannel (void) const
{
  return m_channel;
}

//
// This is a point-to-point device, so we really don't need any kind of address
// information.  However, the base class NetDevice wants us to define the
// methods to get and set the address.  Rather than be rude and assert, we let
// clients get and set the address, but simply ignore them.

void
PointToPointNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION (this << address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address
PointToPointNetDevice::GetAddress (void) const
{
  return m_address;
}

bool
PointToPointNetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkUp;
}

void
PointToPointNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

//
// This is a point-to-point device, so every transmission is a broadcast to
// all of the devices on the network.
//
bool
PointToPointNetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

//
// We don't really need any addressing information since this is a 
// point-to-point device.  The base class NetDevice wants us to return a
// broadcast address, so we make up something reasonable.
//
Address
PointToPointNetDevice::GetBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
PointToPointNetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address
PointToPointNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("01:00:5e:00:00:00");
}

Address
PointToPointNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address ("33:33:00:00:00:00");
}

bool
PointToPointNetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

bool
PointToPointNetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

bool
PointToPointNetDevice::Send (
  Ptr<Packet> packet, 
  const Address &dest, 
  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << dest << protocolNumber);
  NS_LOG_LOGIC ("p=" << packet << ", dest=" << &dest);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  // std::cout << "protocol: " << protocolNumber << "\n";
  // std::cout << "Sending packet=" << packet << "to dest=" << &dest << "\n";
  if (compressionEnabled)
    {

      if (protocolNumber == m_protocol)  // IPv4
        {

          std::cout << "sending: packet with 0x" << std::hex << protocolNumber << "\n";

          if (IsLinkUp () == false)
            {
              m_macTxDropTrace (packet);
              return false;
            }

          // Stick a point to point protocol header on the packet in preparation for
          // shoving it out the door.

          // AddHeader (packet, 0x4021);  // Ether to PPP header
          // TODO uncomment this
          // std::cout << "Converting to 0x4021\n";

          uint32_t packetSize = packet->GetSize();
          printf ("Size of Packet in Compress: %d  \n", packetSize);
          uint32_t adjustedPacketSize = packetSize + 8;
          printf ("Adjusted PacketSize: %d  \n", adjustedPacketSize);
          std::cout << "M_Protocol is: " << m_protocol <<"\n";
          std::cout << "Protocol Number is: " << protocolNumber <<"\n";

          /* Allocate enough memory to the buffer so we don't get a seg fault. 
          Add 8 to house the header code since a header is 8 bytes. */
          uint8_t* buffer = new uint8_t[adjustedPacketSize];
          uint8_t* newBuffer = new uint8_t[adjustedPacketSize];
          packet -> CopyData(buffer, adjustedPacketSize);

          /* So it's serealized, now append the old header protocol i guess, then 
          compress it, and add it back into the packet. */

          /* Adding the old header to the buffer. Then I think we can compress the buffer.
          I think this is probably the wrong way to do it. */

          printf("Packet: %d \n ", packetSize);
          std::cout << "Sent As String: " << packet -> ToString() << "\n";
          // packet -> AddAtEnd();
          /* Get contents of packet */
          // uint8_t* thirdBuffer = new uint8_t[adjustedPacketSize];
          packet -> CopyData(newBuffer, adjustedPacketSize);
          uint8_t* outputData = new uint8_t[adjustedPacketSize];
          /* Compress the data test. This seems to be working */
          Compress(newBuffer, outputData, adjustedPacketSize);
          // Decompress(outputData, thirdBuffer, adjustedPacketSize);

          /** EXAMPLE. Get this working first */
          // uint8_t a[50] = "Hello Hello Hello Hello Hello Hello!"; 
          // uint8_t b[50];
          // uint8_t c[50];
          // CompressExample(adjustedPacketSize, a, b);
          // DecompressExample(adjustedPacketSize, b, c);
          /* Create new packet with compressed buffer and size of buffer memory allocation */

          /* Might be missing the "append old header" step */
           Ptr<Packet> newPacket = Create<Packet>(outputData, adjustedPacketSize);
           /* Add the  correct header before sending it */
           AddHeader (newPacket, 0x4021);
           /* This is 1054 instead of 1062 for some reason */
           printf("Exiting Packet Size: %d \n", newPacket -> GetSize());
           // newPacket -> CopyData(buffer, adjustedPacketSize);
           std::cout << "Sent As String 2: " << newPacket -> ToString() << "\n";          
           /* Now send newPacket */
          m_macTxTrace (newPacket);

          //
          // We should enqueue and dequeue the packet to hit the tracing hooks.
          //
          if (m_queue->Enqueue (newPacket))
            {
              //
              // If the channel is ready for transition we send the packet right now
              // 
              if (m_txMachineState == READY)
                {
                  packet = m_queue->Dequeue ();
                  m_snifferTrace (newPacket);
                  m_promiscSnifferTrace (newPacket);
                  bool ret = TransmitStart (newPacket);
                  return ret;
                }
              return true;
            }
          // Enqueue may fail (overflow)
          m_macTxDropTrace (newPacket);
          return false;
        }

      if (IsLinkUp () == false)
        {
          m_macTxDropTrace (packet);
          return false;
        }

      //
      // Stick a point to point protocol header on the packet in preparation for
      // shoving it out the door.
      //
      AddHeader (packet, protocolNumber);
      std::cout << "Adding header again with" << protocolNumber << "\n";

      m_macTxTrace (packet);

      //
      // We should enqueue and dequeue the packet to hit the tracing hooks.
      //
      if (m_queue->Enqueue (packet))
        {
          //
          // If the channel is ready for transition we send the packet right now
          // 
          if (m_txMachineState == READY)
            {
              packet = m_queue->Dequeue ();
              m_snifferTrace (packet);
              m_promiscSnifferTrace (packet);
              bool ret = TransmitStart (packet);
              return ret;
            }
          return true;
        }

      // Enqueue may fail (overflow)

      m_macTxDropTrace (packet);
      return false;
    } 
  else
    {
    //
    // If IsLinkUp() is false it means there is no channel to send any packet 
    // over so we just hit the drop trace on the packet and return an error.
    //
    if (IsLinkUp () == false)
      {
        m_macTxDropTrace (packet);
        return false;
      }

    //
    // Stick a point to point protocol header on the packet in preparation for
    // shoving it out the door.
    //
    AddHeader (packet, protocolNumber);

    m_macTxTrace (packet);

    //
    // We should enqueue and dequeue the packet to hit the tracing hooks.
    //
    if (m_queue->Enqueue (packet))
      {
        //
        // If the channel is ready for transition we send the packet right now
        // 
        if (m_txMachineState == READY)
          {
            packet = m_queue->Dequeue ();
            m_snifferTrace (packet);
            m_promiscSnifferTrace (packet);
            bool ret = TransmitStart (packet);
            return ret;
          }
        return true;
      }

    // Enqueue may fail (overflow)

    m_macTxDropTrace (packet);
    return false;
  }
}

bool
PointToPointNetDevice::SendFrom (Ptr<Packet> packet, 
                                 const Address &source, 
                                 const Address &dest, 
                                 uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << source << dest << protocolNumber);
  return false;
}

Ptr<Node>
PointToPointNetDevice::GetNode (void) const
{
  return m_node;
}

void
PointToPointNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  m_node = node;
}

bool
PointToPointNetDevice::NeedsArp (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
PointToPointNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
PointToPointNetDevice::SupportsSendFrom (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointNetDevice::DoMpiReceive (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  Receive (p);
}

Address 
PointToPointNetDevice::GetRemote (void) const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_channel->GetNDevices () == 2);
  for (std::size_t i = 0; i < m_channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> tmp = m_channel->GetDevice (i);
      if (tmp != this)
        {
          return tmp->GetAddress ();
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return Address ();
}

bool
PointToPointNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;
  return true;
}

uint16_t
PointToPointNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mtu;
}

uint16_t
PointToPointNetDevice::PppToEther (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case 0x0021: return 0x0800;   //IPv4
    case 0x4021: return 0x4021;   //LZS
    case 0x0057: return 0x86DD;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

uint16_t
PointToPointNetDevice::EtherToPpp (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case 0x0800: return 0x0021;   //IPv4
    case 0x4021: return 0x4021;   //LZS
    case 0x86DD: return 0x0057;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

// might just need to take packet as input and modify it within the function
/* Will need to store the original data somewhere to confirm data is the same */
/* Big issues with these. Need to deal with pointers to arrays correctly. */
uint8_t*
PointToPointNetDevice::Compress (uint8_t* packetData, uint8_t* outputData, uint32_t size)
{
  // p_packet
  // Ptr<Packet> p = p_packet->Copy ();
  // original string len = 36
  
  printf("Packet Data In Compress: %s\n", packetData);
  printf("Size in Compress: %d \n", size);

  // zlib struct
  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;

  defstream.avail_in = (uInt)strlen((char*)packetData)+1; // size of input, string + terminator
  defstream.next_in = (Bytef *)packetData; // input char array
  defstream.avail_out = (uInt)(size); // size of output
  defstream.next_out = (Bytef *)outputData; // output char array

  // compress
  deflateInit(&defstream, Z_BEST_COMPRESSION);
  deflate(&defstream, Z_FINISH);
  deflateEnd(&defstream);

  printf("Compressed data: %s\n", outputData);
  return outputData;
}

//figure out return type
uint8_t*
PointToPointNetDevice::Decompress (uint8_t* packetData, uint8_t* outputData, uint32_t size)
{

    uint8_t* c = new uint8_t[size];
    printf("Size in Decompress: %d \n", size);
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    
    // need to determine avail_in still
    // Don't know what to do for avail_in
    infstream.avail_in = (uInt)(size-strlen((char*)packetData)); // size of input
    infstream.next_in = (Bytef *)packetData; // input char array
    infstream.avail_out = (uInt)(size); // size of output
    infstream.next_out = (Bytef *)outputData; // output char array
     
    // decompress
    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);

    printf("Decompressed data: %s\n", outputData);
    
    return c;
}

uint8_t*
PointToPointNetDevice::CompressExample (uint32_t size, uint8_t* a, uint8_t* b)
{
     

    printf("Uncompressed size is: %lu\n", strlen((char*)a));
    printf("Uncompressed string is: %s\n", a);


    printf("\n----------\n\n");

    // STEP 1.
    // deflate a into b. (that is, compress a into b)
    
    // zlib struct
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    // setup "a" as the input and "b" as the compressed output
    defstream.avail_in = (uInt)strlen((char*)a)+1; // size of input, string + terminator
    defstream.next_in = (Bytef *)a; // input char array
    defstream.avail_out = (uInt)(size); // size of output
    defstream.next_out = (Bytef *)b; // output char array
    
    // the actual compression work.
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
     
    // This is one way of getting the size of the output
    printf("Compressed size is: %lu\n", strlen((char*)b));
    printf("Compressed string is: %s\n", b);
    

    printf("\n----------\n\n");
    return b;
}

//figure out return type
uint8_t*
PointToPointNetDevice::DecompressExample (uint32_t size, uint8_t* b, uint8_t* c)
{

    printf("Input size is: %lu\n", strlen((char*)b));
    printf("Input string is: %s\n", (char*)b);


    printf("\n----------\n\n");

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    // setup "b" as the input and "c" as the compressed output
    // changes this line 
    infstream.avail_in = (uInt)(size-strlen((char*)b)); // size of input
    infstream.next_in = (Bytef *)b; // input char array
    infstream.avail_out = (uInt)(size); // size of output
    infstream.next_out = (Bytef *)c; // output char array
     
    // the actual DE-compression work.
    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);
     
    printf("Uncompressed size is: %lu\n", strlen((char*)c));
    printf("Uncompressed string is: %s\n", c);
    return c;
}

} // namespace ns3
