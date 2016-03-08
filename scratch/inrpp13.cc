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
 * 							  n7		n6
  	  	  	  	  	  	  	  \		    |
 	 	 	 	 	 	 	   \		|
 	 	 	 	 	 	 	 	\		|
 							    n1------n5
			  			  1.2 /   \ 2.1 |
	                   500k  /	   \    |
		    		 1ms    /       \   |
		 		 	   1.1 /         \  |
		  1.5Mbps    1.5Mb/			  \2.2  1.5Mb
		nx--------n4-----n0------------n2--------n3--------nx
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




int
main (int argc, char *argv[])
{

	  uint32_t 		maxBytes = 10000000;
	  uint32_t    	maxPackets = 5000;
	  uint32_t      minTh = 2500;
	  uint32_t      maxTh = 4000;
	  uint32_t 		stop = 100;
	  uint32_t as = 3;
	  double 		time = 0.1;
	  bool 			detour=true;
	  uint32_t n = 5;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("time","interflow time",time);
  cmd.AddValue ("detour","interflow time",detour);
  cmd.AddValue ("stop","stop time",stop);
  cmd.AddValue ("n","number of flows", n);
  cmd.Parse (argc, argv);



  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpInrpp::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (1200000000));
  Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (600000000));
  Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (300000000));
  Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes " << n);

  NodeContainer core;
  NodeContainer edges;

  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::InrppTailQueue",
                           "LowerThBytes", UintegerValue (minTh*1500),
                           "HigherThBytes", UintegerValue (maxTh*1500),
 						   "MaxBytes", UintegerValue(maxPackets*1500));
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("2Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));


  for(uint32_t i=0;i<as;i++)
  {
	  NodeContainer nodes;
	  nodes.Create(6);

	  NS_LOG_LOGIC("AS " << i);
	  std::vector<NetDeviceContainer> devs;
	  NetDeviceContainer devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
	  devs.push_back(devices0);
	  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
	  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
	  devs.push_back(devices1);
	  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get(1),nodes.Get(3));
	  devs.push_back(devices2);
	  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get(2),nodes.Get(3));
	  devs.push_back(devices3);
	  NetDeviceContainer devices4 = pointToPoint.Install (nodes.Get(2),nodes.Get(4));
	  devs.push_back(devices4);
	  NetDeviceContainer devices5 = pointToPoint.Install (nodes.Get(3),nodes.Get(5));
	  devs.push_back(devices5);

	  core.Add(nodes.Get(0));
	  edges.Add(nodes.Get(4));
	  edges.Add(nodes.Get(5));

	  InrppStackHelper inrpp;
	  inrpp.Install (nodes);


  }

  NodeContainer servers;
  servers.Create(2);
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  NetDeviceContainer serversDev = pointToPoint.Install (servers.Get(0),servers.Get(1));
  InrppStackHelper inrpp;
  inrpp.Install (servers);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("12.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (serversDev);
  core.Add(servers.Get(0));

  std::vector<NetDeviceContainer> cores;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  NetDeviceContainer coreDevice0 = pointToPoint.Install (core.Get(0),core.Get(1));
  cores.push_back(coreDevice0);
  NetDeviceContainer coreDevice1 = pointToPoint.Install (core.Get(1),core.Get(2));
  cores.push_back(coreDevice1);
  NetDeviceContainer coreDevice2 = pointToPoint.Install (core.Get(2),core.Get(3));
  cores.push_back(coreDevice2);
  NetDeviceContainer coreDevice3 = pointToPoint.Install (core.Get(3),core.Get(0));
  cores.push_back(coreDevice3);
  NetDeviceContainer coreDevice4 = pointToPoint.Install (core.Get(1),core.Get(3));
  cores.push_back(coreDevice4);


  NS_LOG_INFO ("Create channels.");

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));

  std::vector<Ipv4InterfaceContainer> ifaces;
  std::vector<NetDeviceContainer> clientDevs;

  NodeContainer clients;

  for(uint32_t j=0;j<(n*edges.GetN());j++)
	{
		Ptr<Node> client =  CreateObject<Node>();
		InternetStackHelper inrpp;
		inrpp.Install (client);
		pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
		NetDeviceContainer clientDev = pointToPoint.Install (servers.Get(1),client);
		clients.Add(client);

	}


  NodeContainer allSenders;



  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stop));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;


}



