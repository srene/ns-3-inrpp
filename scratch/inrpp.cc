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
/*
										   n4
									   2.2/   \ 4.1
								 1 Mbps	 /	   \  1 Mbps
								   1ms	/       \   1ms
									2.1/         \
		0.1		    0.2 1.1 	1.2	  /			  \4.2 5.1     5.2
       n0 ----------- n1-----------n2------------n3-------------n5
            1 Mbps  	   1 Mbps   3.1  500 Kbps 3.2   1 Mbps
             1 ms           1 ms          1 ms          1 ms


*/
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBulkSendExample");

int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t maxBytes = 0;

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (6);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices0;
  NetDeviceContainer devices1;
  NetDeviceContainer devices2;
  NetDeviceContainer devices3;
  NetDeviceContainer devices4;
  NetDeviceContainer devices5;

  devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
  devices1 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
  devices2 = pointToPoint.Install (nodes.Get(2),nodes.Get(4));
  devices4 = pointToPoint.Install (nodes.Get(4),nodes.Get(3));
  devices5 = pointToPoint.Install (nodes.Get(4),nodes.Get(5));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  devices3 = pointToPoint.Install (nodes.Get(2),nodes.Get(3));


  // NixHelper to install nix-vector routing
  // on all nodes
  /*Ipv4NixVectorHelper nixRouting;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (nixRouting, 10);*/
//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  //internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (devices0);
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ipv4.Assign (devices1);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (devices2);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = ipv4.Assign (devices3);
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i4 = ipv4.Assign (devices4);
  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i5 = ipv4.Assign (devices5);
  NS_LOG_INFO ("Create Applications.");

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//
// Create a BulkSendApplication and install it on node 0
//
  uint16_t port = 9000;  // well-known echo port number


  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i5.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

//
// Create a PacketSinkApplication and install it on node 1
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (5));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

//
// Set up tracing if enabled
//
  if (tracing)
    {
     //AsciiTraceHelper ascii;
    //  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("inrpp.tr"));
      pointToPoint.EnablePcapAll ("inrpp", false);
    }


  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("inrpp.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (10.0), routingStream);

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
}
