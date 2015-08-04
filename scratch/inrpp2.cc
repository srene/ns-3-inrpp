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
			   n1
          1.2/   \ 2.1
  1 Mbps    /	  \  1 Mbps
     1ms   /       \   1ms
	   1.1/         \
		 /			 \2.2
  n4-----n0------------n2--------n3
  4.2  4.1 0.1 500 Kbps  0.2 3.1      3.2
             1 ms


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
#include "ns3/inrpp-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("inrpp2");


static void
Drop (Ptr<const Packet> p)
{
	  NS_LOG_INFO ("Drop "<< p);

}

static void
CwndTracer (const Ipv4Header &iph, Ptr<const Packet> p, uint32_t interface)
{
	  NS_LOG_INFO ("Forward "<< iph << " " << interface);

}

static void
BufferChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd/1500 << std::endl;

}

static void
BwChange (Ptr<OutputStreamWrapper> stream, double oldCwnd, double newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;

}
static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_INFO ("CwndChange");
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;

}

void StartLog(Ptr<Socket> socket);


int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t 		maxBytes = 0;
  uint32_t    	maxPackets = 50;
  uint32_t      minTh = 25;
  uint32_t      maxTh = 40;

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
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
  nodes.Create (5);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::InrppTailQueue",
                           "LowerThPackets", UintegerValue (minTh),
                           "HigherThPackets", UintegerValue (maxTh),
						   "MaxPackets", UintegerValue(maxPackets));
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices0;
  NetDeviceContainer devices1;
  NetDeviceContainer devices2;
  NetDeviceContainer devices3;
  NetDeviceContainer devices4;

  devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
  devices1 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
  devices3 = pointToPoint.Install (nodes.Get(2),nodes.Get(3));
  devices4 = pointToPoint.Install (nodes.Get(0),nodes.Get(4));


  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  devices2 = pointToPoint.Install (nodes.Get(0),nodes.Get(2));


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
  InrppStackHelper internet;
  //InternetStackHelper internet;
  //internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (devices2);
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ipv4.Assign (devices0);
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (devices1);
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = ipv4.Assign (devices3);
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i4 = ipv4.Assign (devices4);

//
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//Configure each node to support INRPP protocol
 for(uint32_t i=0; i<nodes.GetN(); ++i)
  {

	  NS_LOG_INFO("Ip object " << nodes.Get(i));

	 /*  Ptr<InrppL3Protocol> inrpp = CreateObject<InrppL3Protocol> ();
 	  inrpp->SetL3Protocol(ip->GetObject<Ipv4L3Protocol>());
	  nodes.Get(i)->AggregateObject (inrpp);*/


  }

  Ptr<InrppL3Protocol> ip = nodes.Get(0)->GetObject<InrppL3Protocol> ();
  Ptr<InrppRoute> rtentry = Create<InrppRoute> ();
  rtentry->SetDestination (Ipv4Address ("10.0.0.2"));
  /// \todo handle multi-address case
  rtentry->SetDetour (Ipv4Address ("10.0.2.1"));
  rtentry->SetOutputDevice (devices0.Get(0));
  ip->SetDetourRoute(devices2.Get(0),rtentry);

  PointerValue ptr;
  devices4.Get(1)->GetAttribute ("TxQueue", ptr);
  Ptr<Queue> txQueue = ptr.Get<Queue> ();
  AsciiTraceHelper asciiTraceHelper;
  std::ostringstream osstr1;
  osstr1 << "netdevice_0.bf";
  Ptr<OutputStreamWrapper> streamtr1 = asciiTraceHelper.CreateFileStream (osstr1.str());
  txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr1));

  PointerValue ptr2;
  devices2.Get(0)->GetAttribute ("TxQueue", ptr2);
  Ptr<Queue> txQueue2 = ptr2.Get<Queue> ();
  std::ostringstream osstr2;
  osstr2 << "netdevice_1.bf";
  Ptr<OutputStreamWrapper> streamtr2 = asciiTraceHelper.CreateFileStream (osstr2.str());
  txQueue2->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr2));

  txQueue2->TraceConnectWithoutContext ("Drop", MakeCallback (&Drop));

  PointerValue ptr4;
  devices3.Get(0)->GetAttribute ("TxQueue", ptr4);
  Ptr<Queue> txQueue4 = ptr4.Get<Queue> ();
  std::ostringstream osstr4;
  osstr4 << "netdevice_2.bf";
  Ptr<OutputStreamWrapper> streamtr4 = asciiTraceHelper.CreateFileStream (osstr4.str());
  txQueue4->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr4));


  std::ostringstream osstr5;
  osstr5 << "netdevice_1.bw";
  Ptr<OutputStreamWrapper> streamtr5 = asciiTraceHelper.CreateFileStream (osstr5.str());
  devices2.Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr5));


  NS_LOG_INFO ("Create Applications.");

  uint16_t port = 9000;  // well-known echo port number

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i3.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (4));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  std::ostringstream oss1;
  oss1 << "node0.cwnd";
  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (oss1.str());
  Config::ConnectWithoutContext ("/NodeList/4/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback (&CwndChange, stream1));
  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
  bulk->SetCallback(MakeCallback(&StartLog));

// Create a PacketSinkApplication and install it on node 1
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

//
// Set up tracing if enabled
//
  if (tracing)
    {
     //AsciiTraceHelper ascii;
    //  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("inrpp.tr"));
      pointToPoint.EnablePcapAll ("inrpp2", false);
    }

  Ptr<Ipv4L3Protocol> ipa = nodes.Get(0)->GetObject<Ipv4L3Protocol> ();

  ipa->TraceConnectWithoutContext ("UnicastForward", MakeCallback (&CwndTracer));

  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("inrpp2.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (100.0), routingStream);

  Ptr<Node> n2 = nodes.Get (2);
  Ptr<Ipv4> ipv02 = n2->GetObject<Ipv4> ();
  // The first ifIndex is 0 for loopback, then the first p2p is numbered 1,
  // then the next p2p is numbered 2
//  uint32_t ipv4ifIndex1 = 1;

//  Simulator::Schedule (Seconds (2),&Ipv4::SetDown,ipv02, ipv4ifIndex1);
//  Simulator::Schedule (Seconds (4),&Ipv4::SetUp,ipv02, ipv4ifIndex1);

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
}

void StartLog(Ptr<Socket> socket)
{
	  std::ostringstream oss1;
	  oss1 << "node0.cwnd";
	  AsciiTraceHelper asciiTraceHelper;
	  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (oss1.str());
	  NS_LOG_LOGIC("Bulk " << socket);
	  socket->TraceConnectWithoutContext("CongestionWindow",MakeBoundCallback (&CwndChange, stream1));
}
