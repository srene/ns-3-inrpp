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
			 n6				   n1
			  \			  1.2 /   \ 2.1
	    500k   \       500k  /	   \  500 kbps
		    	\	 1ms    /       \   1ms
		 		 \	   1.1 /         \
		  1.5Mbps \  1.5Mb/			  \2.2  1.5Mb
		n5--------n4-----n0------------n2--------n3
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
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("inrpp6");

uint32_t i;
static void
Drop (Ptr<const Packet> p)
{
	  NS_LOG_INFO ("Drop "<< p);

}

/*static void
CwndTracer (const Ipv4Header &iph, Ptr<const Packet> p, uint32_t interface)
{
	  NS_LOG_INFO ("Forward "<< iph << " " << interface);

}*/

static void
BufferChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;

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


void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev);

int
main (int argc, char *argv[])
{
  i=0;
  bool tracing = false;
  uint32_t 		maxBytes = 10000000;
  uint32_t    	maxPackets = 50;
  uint32_t      minTh = 25;
  uint32_t      maxTh = 40;
  uint32_t 		stop = 200;
  uint32_t 		n = 3;
  uint32_t 		time = 5;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpInrpp::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (10000000));
  //Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (12500000));
  Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (60000000));
  //Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (5000000));
  //Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (5000000));
  Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (3000000));
  Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("n","number of flows", n);
  cmd.AddValue ("time","interflow time",time);
  cmd.AddValue ("stop","stop time",stop);

  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create ((2*n)+5);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::InrppTailQueue",
                           "LowerThBytes", UintegerValue (minTh*1500),
                           "HigherThBytes", UintegerValue (maxTh*1500),
						   "MaxBytes", UintegerValue(maxPackets*1500));
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices0;
  NetDeviceContainer devices1;
  NetDeviceContainer devices2;
  NetDeviceContainer devices3;
  NetDeviceContainer devices4;

  devices3 = pointToPoint.Install (nodes.Get(2),nodes.Get(3));
  devices4 = pointToPoint.Install (nodes.Get(4),nodes.Get(0));


  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  devices2 = pointToPoint.Install (nodes.Get(0),nodes.Get(2));
  devices1 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
  devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));

  //
  // Install the internet stack on the nodes with INRPP
  //
    InrppStackHelper inrpp;
    inrpp.Install (nodes.Get(0));
    inrpp.Install (nodes.Get(1));
    inrpp.Install (nodes.Get(2));
    inrpp.Install (nodes.Get(3));
    inrpp.Install (nodes.Get(4));

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

    std::vector<Ptr<PacketSink> > sink;

  for(uint32_t i=0;i<n;i++)
  {

	  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
	  NetDeviceContainer sourceLink = pointToPoint.Install (nodes.Get(5+i),nodes.Get(4));
	  NetDeviceContainer destLink = pointToPoint.Install (nodes.Get(3),nodes.Get(5+n+i));

	  InternetStackHelper internet;
	  internet.Install (nodes.Get(5+i));
	  internet.Install (nodes.Get(5+n+i));

      std::stringstream netAddr;
      netAddr << "10.0." << (i+5) << ".0";
      Ipv4AddressHelper ipv4;
      std::string str = netAddr.str();
      ipv4.SetBase(str.c_str(), "255.255.255.0");
	  Ipv4InterfaceContainer iSource = ipv4.Assign (sourceLink);
      std::stringstream netAddr2;
      netAddr2 << "10.0." << (i+5+n) << ".0";
      str = netAddr2.str();
      ipv4.SetBase(str.c_str(), "255.255.255.0");
	  Ipv4InterfaceContainer iDest = ipv4.Assign (destLink);

	  // Create a PacketSinkApplication and install it on node 1

	  AsciiTraceHelper asciiTraceHelper;
	  PointerValue ptr3;
	  destLink.Get(0)->GetAttribute ("TxQueue", ptr3);
	  Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
	  std::ostringstream osstr3;
	  osstr3 << "netdevice_" <<5+i<<".bf";
	  Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
	  txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));
	  txQueue3->TraceConnectWithoutContext ("Drop", MakeCallback (&Drop));
	  NS_LOG_INFO ("Create Applications.");

	  uint16_t port = 9000+i;  // well-known echo port number

	  BulkSendHelper source ("ns3::TcpSocketFactory",
	                         InetSocketAddress (iDest.GetAddress (1), port));
	  // Set the amount of data to send in bytes.  Zero is unlimited.
	  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
	  ApplicationContainer sourceApps = source.Install (nodes.Get(5+i));
	  sourceApps.Start (Seconds (1.0+(i*time)));
	  sourceApps.Stop (Seconds (stop));

	  PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
						   InetSocketAddress (Ipv4Address::GetAny (), port));
	  ApplicationContainer sinkApps = sink1.Install (nodes.Get(5+n+i));
	  sinkApps.Start (Seconds (0.0));
	  sinkApps.Stop (Seconds (stop));

	  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
	  bulk->SetCallback(MakeCallback(&StartLog));

	 // Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
	 // std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
	  sink.push_back(DynamicCast<PacketSink> (sinkApps.Get (0)));
  }

//
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//Configure detour path at n0
  Ptr<InrppL3Protocol> ip = nodes.Get(0)->GetObject<InrppL3Protocol> ();
  Ptr<InrppRoute> rtentry = Create<InrppRoute> ();
  rtentry->SetDestination (Ipv4Address ("10.0.0.2"));
  /// \todo handle multi-address case
  rtentry->SetDetour (Ipv4Address ("10.0.2.1"));
  rtentry->SetOutputDevice (devices0.Get(0));
  ip->SetDetourRoute(devices2.Get(0),rtentry);

  Ptr<InrppL3Protocol> ip2 = nodes.Get(1)->GetObject<InrppL3Protocol> ();
  //ip2->SendDetourInfo(devices1.Get(0),devices0.Get(1),Ipv4Address ("10.0.0.2"));

  PointerValue ptr;
  devices0.Get(0)->GetAttribute ("TxQueue", ptr);
  Ptr<Queue> txQueue = ptr.Get<Queue> ();
  AsciiTraceHelper asciiTraceHelper;
  std::ostringstream osstr1;
  osstr1 << "netdevice_2.bf";
  Ptr<OutputStreamWrapper> streamtr1 = asciiTraceHelper.CreateFileStream (osstr1.str());
  txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr1));

  PointerValue ptr2;
  devices2.Get(0)->GetAttribute ("TxQueue", ptr2);
  Ptr<Queue> txQueue2 = ptr2.Get<Queue> ();
  std::ostringstream osstr2;
  osstr2 << "netdevice_1.bf";
  Ptr<OutputStreamWrapper> streamtr2 = asciiTraceHelper.CreateFileStream (osstr2.str());
  txQueue2->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr2));

  PointerValue ptr3;
  devices4.Get(0)->GetAttribute ("TxQueue", ptr3);
  Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
  std::ostringstream osstr3;
  osstr3 << "netdevice_3.bf";
  Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
  txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));


  PointerValue ptr4;
  devices2.Get(1)->GetAttribute ("TxQueue", ptr4);
  Ptr<Queue> txQueue4 = ptr4.Get<Queue> ();
  std::ostringstream osstr13;
  osstr13 << "netdevice_4.bf";
  Ptr<OutputStreamWrapper> streamtr13 = asciiTraceHelper.CreateFileStream (osstr13.str());
  txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr13));


  txQueue2->TraceConnectWithoutContext ("Drop", MakeCallback (&Drop));
  //txQueue3->TraceConnectWithoutContext ("Drop", MakeCallback (&Drop));


  std::ostringstream osstr4;
  osstr4 << "netdevice_1.bw";
  Ptr<OutputStreamWrapper> streamtr4 = asciiTraceHelper.CreateFileStream (osstr4.str());
  uint32_t iface = ip->GetInterfaceForDevice(devices2.Get(0));
  ip->GetInterface(iface)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr4));

  std::ostringstream osstr6;
  osstr6 << "netdevice_5.bw";
  Ptr<OutputStreamWrapper> streamtr6 = asciiTraceHelper.CreateFileStream (osstr6.str());
  uint32_t iface2 = ip->GetInterfaceForDevice(devices0.Get(0));
  ip->GetInterface(iface2)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("DetouredFlow", MakeBoundCallback (&BwChange, streamtr6));

  std::ostringstream osstr7;
  osstr7 << "netdevice_3.bw";
  Ptr<OutputStreamWrapper> streamtr7 = asciiTraceHelper.CreateFileStream (osstr7.str());
  ip->GetInterface(iface)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("EstimatedFlow", MakeBoundCallback (&BwChange, streamtr7));

  std::ostringstream osstr10;
  osstr10 << "netdevice_4.bw";
  Ptr<OutputStreamWrapper> streamtr10 = asciiTraceHelper.CreateFileStream (osstr10.str());
  Ptr<InrppL3Protocol> ip3 = nodes.Get(4)->GetObject<InrppL3Protocol> ();
  uint32_t iface3 = ip3->GetInterfaceForDevice(devices4.Get(0));
  ip3->GetInterface(iface3)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("EstimatedFlow", MakeBoundCallback (&BwChange, streamtr10));


  std::ostringstream osstr5;
  osstr5 << "netdevice_2.bw";
  Ptr<OutputStreamWrapper> streamtr5 = asciiTraceHelper.CreateFileStream (osstr5.str());
  ip3->GetInterface(iface3)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr5));

  std::ostringstream oss1;
  oss1 << "node0.cwnd";
  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (oss1.str());
  Config::ConnectWithoutContext ("/NodeList/4/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback (&CwndChange, stream1));


//
// Set up tracing if enabled
//
  if (tracing)
    {
      pointToPoint.EnablePcapAll ("inrpp2", false);
    }

  //Ptr<Ipv4L3Protocol> ipa = nodes.Get(0)->GetObject<Ipv4L3Protocol> ();
  //ipa->TraceConnectWithoutContext ("UnicastForward", MakeCallback (&CwndTracer));

  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("inrpp2.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (100.0), routingStream);

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stop));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  int fl = 0;
  while (!sink.empty())
  {
    NS_LOG_LOGIC("Flow " << fl << " bytes received " << sink.back()->GetTotalRx() << " using " << sink.back()->GetCompletionTime().GetSeconds() << " sec.");
    sink.pop_back();
    fl++;
  }

}

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev)
{
	  socket->GetObject<TcpInrpp>()->SetRate(9900000);
	  AsciiTraceHelper asciiTraceHelper;
	  std::ostringstream osstr;
	  osstr << "netdevice_"<<i<<".tr";
	  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
	  socket->GetObject<TcpInrpp>()->TraceConnectWithoutContext ("Throughput", MakeBoundCallback (&BwChange, streamtr));
	  i++;

}


