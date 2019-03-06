/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

// Network topology
//
// (S)-----(R1)[compression]<--->compression-link<---->[decompression](R2)-----(R)
//
// - UDP flows from S to the R1 P2P IP Link where compression may occur
//       R1 then relays packets, compressed or not, to R2 who then relays them to
//       receiver node R.

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/project1-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include <nlohmann/json.hpp>
#include <iomanip>

using namespace ns3;
using json = nlohmann::json;

NS_LOG_COMPONENT_DEFINE ("UdpClientServerExample");

int
main (int argc, char *argv[])
{
//
// Enable logging for UdpClient and
//
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  bool useV6 = false;
  bool compressionEnabled = false;
  uint16_t maxBandwidth = 0;
  Address udpServerInterfaces;
  Address p2pInterfaces;

// command line interface. Allow for additional parameters to change application.
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.AddValue ("maxBandwidth", "Maximum bandwidth", maxBandwidth);
  cmd.AddValue ("compressionEnabled", "Enable compression", compressionEnabled);
  cmd.Parse (argc, argv);
  printf("Specified maximum bandwidth: %d\n", maxBandwidth);

// Read config file; take inputstream from the file and put it all in json j
  std::ifstream jsonIn("config.json");
  json j;
  jsonIn >> j;
// Print the pretty json to the terminal
  std::cout << std::setw(4) << j << std::endl;
  std::string protocol = "";

// Get the string value from protocolsToCompress and print it
  protocol = j["protocolsToCompress"].get<std::string>();
  std::cout << protocol << "\n";

// Set data rate for point to point
  std::string dataRate (std::to_string(maxBandwidth));
  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  // Setup p2p nodes for the IP link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate + "Mbps"));
  pointToPoint.SetDeviceAttribute ("Compression", BooleanValue (compressionEnabled));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer udpNodes;
  udpNodes.Create (2);

// p2pNetDevice container
  NetDeviceContainer p2pDevices = pointToPoint.Install (p2pNodes);

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer udpContainer = csma.Install (udpNodes);  // Install UDP nodes

  // Internet
  InternetStackHelper internet;
  internet.Install (udpNodes);
  internet.Install (p2pNodes);

  // CsmaHelper csmaServer;
  // csmaServer.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  // csmaServer.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  // csmaServer.SetDeviceAttribute ("Mtu", UintegerValue (1400));
  // NetDeviceContainer udpContainer = csmaClient.Install (udpNodes);  // Install UDP nodes

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  if (useV6 == false)
    {
      Ipv4AddressHelper ipv4;
      // P2P nodes
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer p2pIp = ipv4.Assign (p2pDevices);
      // std::cout << "p2p address=" << p2pIp.GetAddress (1) << "\n";
      p2pInterfaces = Address (p2pIp.GetAddress (1));
      // UDP nodes
      ipv4.SetBase ("10.1.2.0", "255.255.255.0");
      Ipv4InterfaceContainer udpIp = ipv4.Assign (udpContainer);
      // Set clients address to the first udp node
      udpServerInterfaces = Address (udpIp.GetAddress (1));
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
      Ipv6InterfaceContainer i6 = ipv6.Assign (udpContainer);
      udpServerInterfaces = Address(i6.GetAddress (1,1));
    }

  NS_LOG_INFO ("Create Applications.");
//
// Create one udpServer applications on node one.
//
  uint16_t port = 4000;
  uint32_t maxPacketCount = 6000;
  UdpAppServerHelper server (port);
  server.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  ApplicationContainer udpApps = server.Install (udpNodes.Get (1));
  udpApps.Start (Seconds (1.0));
  udpApps.Stop (Seconds (300.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.015);
  UdpAppClientHelper appClient (udpServerInterfaces, port);
  // UdpAppClientHelper appClient (p2pInterfaces, port);
  appClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  appClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  appClient.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  // Install udp client node into the app
  std::cout << "First round\n";
  udpApps = appClient.Install (udpNodes.Get (0));
  udpApps.Start (Seconds (2.0));
  udpApps.Stop (Seconds (300.0));
  // udpApps.Start (Seconds (120.1));
  // udpApps.Stop (Seconds (240.0));
  uint8_t fill[] = { 0, 1, 0, 1, 1, 0 };
  appClient.SetFill (udpApps.Get (0), fill, sizeof(fill), 1024);
  // Install p2p nodes into the app
  ApplicationContainer p2pClient = appClient.Install (p2pNodes.Get (0));
  p2pClient.Start (Seconds (2.0));
  p2pClient.Stop (Seconds (300.0));


  // std::cout << "Second round\n";

  // udpApps = server.Install (udpNodes.Get (1));
  // udpApps.Start (Seconds (1.0));
  // udpApps.Stop (Seconds (120.0));
  // udpApps = appClient.Install (udpNodes.Get (0));
  // udpApps.Start (Seconds (2.0));
  // udpApps.Stop (Seconds (120.0));
  // p2pClient = appClient.Install (p2pNodes.Get (0));
  // p2pClient.Start (Seconds (2.0));
  // p2pClient.Stop (Seconds (120.0));
// #if 0
// set fill for packet data
// #endif

  // Init routers (PointToPoint devices)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  AsciiTraceHelper ascii;;
  csma.EnableAsciiAll (ascii.CreateFileStream ("udp-app-l.tr"));
  csma.EnablePcapAll ("udp-app-l", false);
  pointToPoint.EnablePcapAll ("udp-p2p-l", false);

  // udpApps.Start (Seconds (120.5));
  // udpApps.Stop (Seconds (120.0));
//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  std::cout << "Done 1.\n";

  // udpApps = server.Install (udpNodes.Get (1));
  // udpApps.Start (Seconds (1.0));
  // udpApps.Stop (Seconds (120.0));

  // // udpApps = appClient.Install (udpNodes.Get (0));
  // udpApps.Start (Seconds (1.0));
  // udpApps.Stop (Seconds (120.0));

  // // p2pClient = appClient.Install (p2pNodes.Get (0));
  // p2pClient.Start (Seconds (2.0));
  // p2pClient.Stop (Seconds (120.0));

  // csma.EnableAsciiAll (ascii.CreateFileStream ("udp-app-h.tr"));
  // csma.EnablePcapAll ("udp-app-h", false);
  // pointToPoint.EnablePcapAll ("udp-p2p-h", false);

  // NS_LOG_INFO ("Run Simulation.");
  // Simulator::Run ();
  // Simulator::Destroy ();
  // NS_LOG_INFO ("Done.");
  // std::cout << "Done 2.\n";
}
