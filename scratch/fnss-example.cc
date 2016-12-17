/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/fnss-simulation.h"
#include "ns3/udp-echo-client.h"
#include "ns3/fnss-event.h"
#include "ns3/traffic-matrix-sequence.h"
#include "ns3/parser.h"

#include <string>
#include <iostream>
#include <fstream>
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
#include "ns3/tcp-rcp.h"


NS_LOG_COMPONENT_DEFINE ("inrpp_fnss");
using namespace ns3;
uint32_t packetSize;

int main (int argc, char *argv[])
{
   std::map<uint32_t, uint32_t> node_to_indx;
   std::map<uint32_t, Address> node_to_dst_addr;
   packetSize = 1500;
	uint32_t 		stop = 1000;
	uint32_t 		start = 0;
   uint32_t duration = 2; //duration of each TM in sequence
   std::string protocol = "t";
   uint32_t      bneck = 1000000000;
   //uint32_t    mean_n_pkts = (0.015*bneck)/(8*packetSize);

   uint32_t      maxPackets = (bneck * 0.05)/(8);

   uint32_t      maxTh = maxPackets;
   uint32_t      minTh = maxPackets/2;

   //uint32_t    hCacheTh  = bneck * 10/8;
   //uint32_t    lCacheTh  = hCacheTh/2;
   //uint32_t    maxCacheSize = hCacheTh*2;
   //double load = 0.8;
   std::string topologyFile = "src/fnss/examples/res/topology.xml";
   std::string trafficFile = "src/fnss/examples/res/stationary-traffic-matrix.xml";
   std::string eventsFile = "src/fnss/examples/res/eventschedule.xml";
 
   LogComponentEnable("FNSSSimulation", LOG_LEVEL_INFO);
   LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
   LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
   LogComponentEnable("FNSSEvent", LOG_LEVEL_INFO);
   
   CommandLine cmd;
   cmd.AddValue ("protocol","protocol",protocol);
   cmd.Parse (argc, argv);

   fnss::Topology topo = fnss::Parser::parseTopology(topologyFile);
   FNSSSimulation sim(topo);
   sim.assignIPv4Addresses(Ipv4Address("10.0.0.0"));
   //sim.scheduleEvents(eventsFile);
   fnss::TrafficMatrixSequence tms = fnss::Parser::parseTrafficMatrixSequence(trafficFile);
   
   PointToPointHelper pointToPoint = sim.getP2PHHelper();

   // Add all the router nodes into a single container
   NodeContainer nodes;
   FNSSSimulation::NodesMap nodesMap = sim.getNodesMap();
   for(FNSSSimulation::NodesMap::iterator it = sim.getNodesMap().begin(); it != sim.getNodesMap().end(); it++) {
      nodes.Add(it->second.m_ptr);
   }

   if(protocol=="t"){
      pointToPoint.SetQueue ("ns3::DropTailQueue",
                        "MaxBytes", UintegerValue(maxPackets));
      InternetStackHelper inrpp;
      inrpp.Install (nodes);
   } else if (protocol=="i") {

      pointToPoint.SetQueue ("ns3::InrppTailQueue",
                        "LowerThBytes", UintegerValue (minTh),
                        "HigherThBytes", UintegerValue (maxTh),
                        "MaxBytes", UintegerValue(maxPackets));

      InrppStackHelper inrpp;
      inrpp.Install (nodes);
   } 
   
   // Attach a sender and a receiver node to each router
   NodeContainer senders;
   NodeContainer receivers;
   senders.Create(nodes.GetN());
   receivers.Create(nodes.GetN());
   NetDeviceContainer sourceLink;
   NetDeviceContainer destLink;
   uint32_t indx;
	int num = 0;
	int net = 0;
   for(indx = 0; indx < nodes.GetN(); indx++)
   {
      if(protocol =="i")
      {
         pointToPoint.SetQueue ("ns3::InrppTailQueue",
                           "LowerThBytes", UintegerValue (minTh),
                           "HigherThBytes", UintegerValue (maxTh),
                           "MaxBytes", UintegerValue(maxPackets));

         sourceLink = pointToPoint.Install(nodes.Get(indx), senders.Get(indx));
         destLink = pointToPoint.Install(nodes.Get(indx), receivers.Get(indx));


         InternetStackHelper internet; 
         internet.Install (senders.Get(indx));
         internet.Install (receivers.Get(indx));
      }
      else 
      {
         pointToPoint.SetQueue ("ns3::DropTailQueue",
								   "MaxBytes", UintegerValue(maxPackets));

         sourceLink = pointToPoint.Install(nodes.Get(indx), senders.Get(indx));
         destLink = pointToPoint.Install(nodes.Get(indx), receivers.Get(indx));
         InternetStackHelper internet;
         internet.Install (senders.Get(indx));
         internet.Install (receivers.Get(indx));
      }
      node_to_indx.insert(std::make_pair(nodes.Get(indx)->GetId(), indx));
      
      std::stringstream netAddr;
      netAddr << "11." << net << "." << (num) << ".0";
      Ipv4AddressHelper ipv4;
      std::string str = netAddr.str();
      ipv4.SetBase(str.c_str(), "255.255.255.0");
      Ipv4InterfaceContainer iSource = ipv4.Assign (sourceLink);
      
      std::stringstream netAddr2;
      netAddr2 << "12." << net << "." << (num) << ".0";
      str = netAddr2.str();
      ipv4.SetBase(str.c_str(), "255.255.255.0");
      Ipv4InterfaceContainer iDest = ipv4.Assign (destLink);

      //Start the sink appps
		uint16_t port = 9000+indx;  // well-known echo port number
      node_to_dst_addr.insert(std::make_pair(nodes.Get(indx)->GetId(), InetSocketAddress(iDest.GetAddress(1), port)));
        
		PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
					   InetSocketAddress (Ipv4Address::GetAny (), port));
		ApplicationContainer sinkApps = sink1.Install (receivers.Get(indx));
		sinkApps.Start (Seconds (0.0));
		sinkApps.Stop (Seconds (stop));
   }
   
   start = 0;
   //Start the on/off client apps
   for(indx = 0; indx < tms.size(); indx++)
   {
      fnss::TrafficMatrix tm = tms.getMatrix(indx);
      std::set<std::pair<std::string, std::string> > pairs = tm.getPairs();
      std::set<std::pair<std::string, std::string> >::iterator it;
      for(it = pairs.begin(); it != pairs.end(); it++)
      {
         fnss::Quantity q = tm.getFlow(it->first, it->second); 
         Ptr<Node> src = sim.getNode(it->first);
         Ptr<Node> dst = sim.getNode(it->second);
         std::map<uint32_t, uint32_t>::iterator it_src = node_to_indx.find(src->GetId());
         std::map<uint32_t, uint32_t>::iterator it_dst = node_to_indx.find(dst->GetId());
         if(it_src != node_to_indx.end() && it_dst != node_to_indx.end())
         {
            std::map<uint32_t, Address>::iterator it_addr = node_to_dst_addr.find(dst->GetId());
            if(it_addr == node_to_dst_addr.end())
            {
               //Error:
               std::cout<<"node to addr mapping failed!\n";
               exit(1);
            }
            Address addr = it_addr->second;
            OnOffHelper onoffclient("ns3::TcpSocketFactory", addr); 
            onoffclient.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
            onoffclient.SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            ApplicationContainer sourceApps = onoffclient.Install(src);
            sourceApps.Start (Seconds (start));
            sourceApps.Stop (Seconds (start+duration));
         }
         else
         {
            //Error
         }
         start += duration;
         break; //XXX Remove this to test more than one tm
      }
   }

	NS_LOG_INFO ("Run Simulation.");
	Simulator::Stop (Seconds (stop));
   Simulator::Run ();
   Simulator::Destroy ();
   return 0;
}


