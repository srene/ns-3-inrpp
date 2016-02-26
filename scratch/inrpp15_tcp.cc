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

uint32_t i;
bool tracing,tracing2;
std::string folder;
Time t;
uint32_t active_flows;
std::map<Ptr<PacketSink> ,uint32_t> flows;
uint32_t n;
uint32_t m;
std::vector<Ptr<PacketSink> > sink;
std::map<Ptr<PacketSink> ,uint32_t> data;
std::map<Ptr<PacketSink> ,uint32_t> rx;
uint32_t 		maxBytes;
uint32_t cache=0;

std::map<Ptr<NetDevice>,double> tr;
std::map<Ptr<NetDevice>,double>                  m_lastSampleBW4;           //!< Last bandwidth sample
std::map<Ptr<NetDevice>,double>                  m_lastBW4;                 //!< Last bandwidth sample after being filtered
std::map<Ptr<NetDevice>,Time>                    t4;
std::map<Ptr<NetDevice>,uint32_t> data4;

Ptr<UdpServer> udp;

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
RttTracer (Ptr<OutputStreamWrapper> stream,Time oldval, Time newval)
{

  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad);

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev);
void StopFlow(Ptr<PacketSink> p);
void LogState(Ptr<InrppInterface> iface,uint32_t state);
void TxRx(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p, Ptr<NetDevice> dev1 ,  Ptr<NetDevice> dev2,  Time tr, Time rcv);
void LogCache(Ptr<InrppL3Protocol> inrpp)
{
	AsciiTraceHelper asciiTraceHelper;
	std::ostringstream osstr21;
	osstr21 << folder << "/netcache_"<<cache<<".bf";
	Ptr<OutputStreamWrapper> streamtr21 = asciiTraceHelper.CreateFileStream (osstr21.str());
    inrpp->GetCache()->TraceConnectWithoutContext ("Size", MakeBoundCallback (&BufferChange, streamtr21));
    cache++;

}


int
main (int argc, char *argv[])
{
	  t = Simulator::Now();
	  i=0;
	  tracing = true;
	  tracing2 = true;
	  maxBytes = 100000;
	  uint32_t    	maxPackets = 40;

	  uint32_t 		stop = 100;
	  n = 5;
	  uint32_t as = 3;
	  double 		time = 0.1;
	  bool 			detour=true;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("tracing2", "Flag to enable/disable tracing", tracing2);

  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("n","number of flows", n);
  cmd.AddValue ("m","number of flows", m);
  cmd.AddValue ("time","interflow time",time);
  cmd.AddValue ("detour","interflow time",detour);
  cmd.AddValue ("stop","stop time",stop);

  cmd.Parse (argc, argv);

  if(detour){
  std::ostringstream st;
  st << "tcptest_fl" <<n+m<<"_int"<<time;
  folder = st.str();
  }
  else{
	  std::ostringstream st;
	  st << "test_nod_fl" <<n<<"_int"<<time;
	  folder = st.str();
  }

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (10000000));
  Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes " << n << " and " << m);

  NodeContainer core;
  NodeContainer edges;
  NodeContainer routers;
  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::DropTailQueue",
 						   "MaxBytes", UintegerValue(maxPackets*1500));
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  uint32_t net1 = 0;

  for(uint32_t i=0;i<as;i++)
  {
	  NodeContainer nodes;
	  nodes.Create(6);

	  NS_LOG_LOGIC("AS " << i);
	  std::vector<NetDeviceContainer> devs;
	  NetDeviceContainer devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
	  devs.push_back(devices0);
	  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
	  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
	  devs.push_back(devices1);
	  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get(1),nodes.Get(3));
	  devs.push_back(devices2);
	  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get(2),nodes.Get(3));
	  devs.push_back(devices3);
	  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
	  NetDeviceContainer devices4 = pointToPoint.Install (nodes.Get(2),nodes.Get(4));
	  devs.push_back(devices4);
	  NetDeviceContainer devices5 = pointToPoint.Install (nodes.Get(3),nodes.Get(5));
	  devs.push_back(devices5);

	  core.Add(nodes.Get(0));
	  edges.Add(nodes.Get(4));
	  edges.Add(nodes.Get(5));

	  routers.Add(nodes);
	  InternetStackHelper inrpp;
	  inrpp.Install (nodes);

	  uint32_t num1 = 0;
	  for(uint32_t j=0;j<devs.size();j++)
		{
			NS_LOG_INFO ("Assign IP Addresses.");
			std::stringstream netAddr;
			netAddr << "10." << net1 << "." << num1 << ".0";
			Ipv4AddressHelper ipv4;
			std::string str = netAddr.str();
			  NS_LOG_LOGIC("Set up address " << str);
			ipv4.SetBase(str.c_str(), "255.255.255.0");
			Ipv4InterfaceContainer i0 = ipv4.Assign (devs[j]);
			num1++;

			  AsciiTraceHelper ascii;
			  std::ostringstream osstr;
			  osstr << folder << "/inrpp15_tcp.tr";
			  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (osstr.str());
			  pointToPoint.EnableAscii (stream, devs[j].Get(0));
		}
	  net1++;
  }

  NodeContainer servers;
  servers.Create(2);
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  NetDeviceContainer serversDev = pointToPoint.Install (servers.Get(0),servers.Get(1));
  InternetStackHelper inrpp;
  inrpp.Install (servers);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("12.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (serversDev);
  core.Add(servers.Get(0));

  routers.Add(servers);

  std::vector<NetDeviceContainer> cores;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  NetDeviceContainer coreDevice0 = pointToPoint.Install (core.Get(0),core.Get(1));
  cores.push_back(coreDevice0);
  NetDeviceContainer coreDevice1 = pointToPoint.Install (core.Get(1),core.Get(2));
  cores.push_back(coreDevice1);
  NetDeviceContainer coreDevice2 = pointToPoint.Install (core.Get(2),servers.Get(0));
  cores.push_back(coreDevice2);
  NetDeviceContainer coreDevice3 = pointToPoint.Install (servers.Get(0),core.Get(0));
  cores.push_back(coreDevice3);
  NetDeviceContainer coreDevice4 = pointToPoint.Install (core.Get(1),servers.Get(0));
  cores.push_back(coreDevice4);
  std::vector<Ipv4InterfaceContainer> coreIfaces;

  uint32_t net2=0;
  for(uint32_t j=0;j<cores.size();j++)
	{
		NS_LOG_INFO ("Assign Core IP Addresses.");
		std::stringstream netAddr;
		netAddr << "11.0." << net2 << ".0";
		Ipv4AddressHelper ipv4;
		std::string str = netAddr.str();
		ipv4.SetBase(str.c_str(), "255.255.255.0");
		  NS_LOG_LOGIC("Set up address " << str);
		Ipv4InterfaceContainer i0 = ipv4.Assign (cores[j]);
		coreIfaces.push_back(i0);

		net2++;
	}

  NS_LOG_INFO ("Create channels.");

  net2=0;
  uint32_t num2=0;

  std::vector<Ipv4InterfaceContainer> ifaces;
  std::vector<NetDeviceContainer> clientDevs;

  NodeContainer clients;
  NodeContainer allSenders;

  std::vector<NetDeviceContainer> sourceLinks;
  for(uint32_t i=0;i<edges.GetN();i++)
  {

	NodeContainer senders;
	uint32_t nclient = 0;
//  if(i % 2== 0 )
//  {
	Config::SetDefault ("ns3::UniformRandomVariable::Min", DoubleValue (1));
	Config::SetDefault ("ns3::UniformRandomVariable::Max", DoubleValue (n));
	Ptr<UniformRandomVariable> urng = CreateObject<UniformRandomVariable> ();
	nclient = urng->GetInteger();
	//lastClients = nclient;
	//senders.Create(nclient);
	senders.Create(n);
    NS_LOG_LOGIC("Number of clients in edge " << i << " is: " << nclient);
  for(uint32_t j=0;j<senders.GetN();j++)
	{
		Ptr<Node> client =  CreateObject<Node>();
		InternetStackHelper inrpp;
		inrpp.Install (client);
		pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Kbps"));
		NetDeviceContainer clientDev = pointToPoint.Install (servers.Get(1),client);
		clients.Add(client);
		std::stringstream netAddr;
		netAddr << "13." << net2 << "." << num2 << ".0";
		Ipv4AddressHelper ipv4;
		std::string str = netAddr.str();
		ipv4.SetBase(str.c_str(), "255.255.255.0");
		NS_LOG_LOGIC("Set up address " << str);
		Ipv4InterfaceContainer i0 = ipv4.Assign (clientDev);
		ifaces.push_back(i0);
		clientDevs.push_back(clientDev);
		  num2++;
		  if(num2==255)
		  {
			  num2=0;
			  net2++;
		  }
		  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Kbps"));
		  NetDeviceContainer sourceLink = pointToPoint.Install (senders.Get(j),edges.Get(i));
		  sourceLinks.push_back(sourceLink);

	}
     net2++;
     num2=0;
	  allSenders.Add(senders);

  }

  inrpp.Install (allSenders);
  uint32_t net = 0;
 // uint32_t nserv = 0;
  //uint32_t lastClients = 0 ;


	  int num = 0;

	//  }
	//  else{
	//	  nclient = (n*2)-lastClients;
	//	  senders.Create(nclient);
	// }

	 //

	  for(uint32_t j=0;j<allSenders.GetN();j++)
	  {

		 // pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));



		  std::stringstream netAddr;
		  netAddr << "14." << net << "." << num++ << ".0";
		  Ipv4AddressHelper ipv4;
		  std::string str = netAddr.str();
		  NS_LOG_LOGIC("Set up address " << str);
		  ipv4.SetBase(str.c_str(), "255.255.255.0");
		  Ipv4InterfaceContainer iSource = ipv4.Assign (sourceLinks[j]);

		  /*if(tracing2)
		  {
			AsciiTraceHelper asciiTraceHelper;
			PointerValue ptr3;
			sourceLink.Get(0)->GetAttribute ("TxQueue", ptr3);
			Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
			std::ostringstream osstr3;
			osstr3 << folder << "/netdevice_" <<j<<".bf";
			Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
			txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));
		  }*/
		  //txQueue3->TraceConnectWithoutContext ("Drop", MakeCallback (&Drop));
		  //NS_LOG_INFO ("Create Applications.");

		  uint16_t port = 9000+j;  // well-known echo port number

		  BulkSendHelper source ("ns3::TcpSocketFactory",
								 InetSocketAddress (iSource.GetAddress (0), port));
		  // Set the amount of data to send in bytes.  Zero is unlimited.
		  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
		  ApplicationContainer sourceApps = source.Install (clients.Get(j));
		  sourceApps.Start (Seconds (1.0));
		  sourceApps.Stop (Seconds (stop));

		  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
		  bulk->SetCallback(MakeCallback(&StartLog));
		  bulk->SetNetDevice(clientDevs[j].Get(1));

		  PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
							   InetSocketAddress (Ipv4Address::GetAny (), port));
		  ApplicationContainer sinkApps = sink1.Install (allSenders.Get(j));
		  sinkApps.Start (Seconds (1.0));
		  sinkApps.Stop (Seconds (stop));
		  Ptr<PacketSink> psink = DynamicCast<PacketSink> (sinkApps.Get (0));
		  psink->SetCallback(MakeCallback(&StopFlow));
		  //psink->TraceConnectWithoutContext("Rx", MakeBoundCallback (&StopFlow, psink));
		  sink.push_back(psink);

		//  nserv++;

		 // Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
		 // std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;

		   if(tracing2)
		  {
			  AsciiTraceHelper asciiTraceHelper;
			  std::ostringstream osstr;
			  osstr << folder << "/netdeviceRx_"<<j<<".tr";
			  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
			  DynamicCast<PacketSink> (sinkApps.Get (0))->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr));
		  }

			  if(num==255)
			  {
				  num=0;
				  net++;
			  }

	   }


		  if (tracing)
			{
			  std::ostringstream osstr;
			  osstr << folder << "/inrpp12-client";
			  //pointToPoint.EnablePcap(osstr.str(),nodes, false);
			  pointToPoint.EnablePcap(osstr.str(),allSenders, false);

			}

//
// Create one udpServer applications on node one.
//
  uint16_t port = 9000+(n*6)+1;  // well-known echo port number
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (core.Get (2));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (100.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1470;
  Time interPacketInterval = Seconds (0.0007);
  uint32_t maxPacketCount = 1000000;
  UdpClientHelper client (coreIfaces[2].GetAddress(0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  ApplicationContainer apps2 = client.Install (servers.Get (0));
  apps2.Start (Seconds (5.0));
  apps2.Stop (Seconds (20.0));

  udp = DynamicCast<UdpServer> (apps.Get (0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  //InrppGlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing)
 	{
 	 // std::ostringstream osstr;
 	 // osstr << folder << "/inrpp12";
 	 //pointToPoint.EnablePcap(osstr.str(),nodes, false);
 	 // pointToPoint.EnablePcap(osstr.str(),routers, false);

 	}


  for(uint32_t i=0;i<routers.GetN();i++)
  {


	  if(tracing2)
	  {
		  for(uint32_t j=0;j<routers.Get(i)->GetNDevices();j++)
		  {
			  if(j>3)break;
			  if(routers.Get(i)->GetDevice(j)->IsPointToPoint())
			  {
				  AsciiTraceHelper asciiTraceHelper;
				  PointerValue ptr3;
				  routers.Get(i)->GetDevice(j)->GetAttribute ("TxQueue", ptr3);
				  Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
				  std::ostringstream osstr3;
				  osstr3 << folder << "/router" << i << "_" << j << ".bf";
				  Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
				  txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));

				  std::ostringstream osstr4;
				  osstr4 << folder << "/router" << i << "_" << j << ".otr";
				  Ptr<OutputStreamWrapper> streamtr4 = asciiTraceHelper.CreateFileStream (osstr4.str());
				  data4.insert(std::make_pair(routers.Get(i)->GetDevice(j),0));
				  m_lastSampleBW4.insert(std::make_pair(routers.Get(i)->GetDevice(j),0));
				  m_lastBW4.insert(std::make_pair(routers.Get(i)->GetDevice(j),0));
				  t4.insert(std::make_pair(routers.Get(i)->GetDevice(j),0));
				  Ptr<PointToPointChannel> ch = routers.Get(i)->GetDevice(j)->GetChannel()->GetObject<PointToPointChannel>();
				  ch->TraceConnectWithoutContext ("TxRxPointToPoint", MakeBoundCallback (&TxRx,streamtr4));

			  }
		  }
	  }
  }
  if (tracing)
	{
	  std::ostringstream osstr;
	  osstr << folder << "/inrpp12-server";
	 //pointToPoint.EnablePcap(osstr.str(),nodes, false);
	  pointToPoint.EnablePcap(osstr.str(),clients, false);

	}
  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stop));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  int fl = 0;
  double avg_ct=0;
  while (!sink.empty())
  {
    std::map<Ptr<PacketSink>,uint32_t>::iterator it = flows.find(sink.back());
    double ct =  sink.back()->GetCompletionTime().GetSeconds();
	NS_LOG_LOGIC("Flow " << fl << " bytes received " << sink.back()->GetTotalRx() << " using " << ct << " sec " << " with " << it->second << " active flows.");
    sink.pop_back();
    flows.erase(it);
    fl++;
    avg_ct+=ct;
  }

  NS_LOG_LOGIC("UDP Lost packets " << udp->GetLost() << " received " << udp->GetReceived());
  NS_LOG_LOGIC("Average flow completion time " << avg_ct/(allSenders.GetN()));

  return 0;

}


void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev)
{
	active_flows++;
	NS_LOG_LOGIC("Start flow " << active_flows);

	socket->BindToNetDevice(netDev);

	if(active_flows==n)
	{
		for (std::vector<Ptr<PacketSink> >::iterator it = sink.begin() ; it != sink.end(); ++it)
		{
			Ptr<PacketSink> p = *it;
			p->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&Sink,p));
			data.insert(std::make_pair(p,0));

		}
	}
	  //socket->GetObject<TcpInrpp>()->SetRate(10000000);
	if(tracing2)
	  {
		  AsciiTraceHelper asciiTraceHelper;
		  std::ostringstream osstr;
		  osstr << folder << "/netdevice_"<<i<<".tr";
		  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
		  socket->GetObject<TcpNewReno>()->TraceConnectWithoutContext ("Throughput", MakeBoundCallback (&BwChange, streamtr));
		  i++;

		  std::ostringstream oss2;
		  oss2 << folder << "/netdevice_"<<i<<".rtt";
		  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (oss2.str());
		  socket->TraceConnectWithoutContext("RTT", MakeBoundCallback (&RttTracer, stream2));
  }


}

void StopFlow(Ptr<PacketSink> p)
{

	NS_LOG_LOGIC("Flow ended " <<active_flows);
	flows.insert(std::make_pair(p,active_flows));

	//if(active_flows==n)
	//{
		//NS_LOG_LOGIC("Ended");
		double sum=0;
		double powersum=0;
		for (std::map<Ptr<PacketSink>,uint32_t>::iterator it=data.begin(); it!=data.end(); ++it)
		{
			NS_LOG_LOGIC("PacketSink " << it->first << " data rx " << (it->second/1000));
			sum+=(it->second/1000);
			uint32_t powsum = pow((it->second/1000),2);
			powersum+=powsum;

		}
		double fairness = (double)pow(sum,2) / (data.size() * powersum);
		NS_LOG_LOGIC("Fairness index " << fairness);
	//}
	active_flows--;
	data.erase(p);




}

void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad)
{
	std::map<Ptr<PacketSink>,uint32_t>::iterator it = data.find(psink);
	uint32_t rx = it->second;
	rx+=p->GetSize();
	data.erase(it);
	data.insert(std::make_pair(psink,rx));

}

void LogState(Ptr<InrppInterface> iface,uint32_t state){

	NS_LOG_LOGIC("Inrpp state changed " << iface << " to state " << state);
}

void TxRx(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p, Ptr<NetDevice> dev1, Ptr<NetDevice> dev2,  Time tr, Time rcv)
{
	//NS_LOG_LOGIC("TXRX " << dev1 << " " << p->GetSize());
	std::map<Ptr<NetDevice>,uint32_t>::iterator it;
	it = data4.find(dev1);

	if(it!=data4.end())
	{

			uint32_t tdata = it->second;
			tdata+= p->GetSize() * 8;

	        Time tt;
			std::map<Ptr<NetDevice>,Time>::iterator it2 = t4.find(dev1);
			tt = it2->second;
			std::map<Ptr<NetDevice>,double>::iterator it3 = m_lastSampleBW4.find(dev1);
			std::map<Ptr<NetDevice>,double>::iterator it4 = m_lastBW4.find(dev1);

			if(Simulator::Now().GetSeconds()-tt.GetSeconds()>0.1)
			{
			  double t_currentBW = tdata / (Simulator::Now().GetSeconds()-tt.GetSeconds());
			  tdata = 0;
			  double alpha = 0.6;
			  double sample_bwe = t_currentBW;
			  t_currentBW = (alpha * it4->second) + ((1 - alpha) * ((sample_bwe + it3->second) / 2));
			  if(it!=data4.end())
			  m_lastSampleBW4.erase(it3);
			  m_lastSampleBW4.insert(std::make_pair(it->first,sample_bwe));
			  m_lastBW4.erase(it4);
			  m_lastBW4.insert(std::make_pair(it->first,t_currentBW));
			  t4.erase(it2);
			  t4.insert(std::make_pair(it->first,Simulator::Now()));
			 // NS_LOG_LOGIC("Detoured data " << it->first << " "<< tdata << " "<< p->GetSize()*8 << " "<< (Simulator::Now().GetSeconds()-tt.GetSeconds()) << " " << t_currentBW);
			  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << t_currentBW << std::endl;
			}


			data4.erase(it);
			data4.insert(std::make_pair(it->first,tdata));
	}
}

