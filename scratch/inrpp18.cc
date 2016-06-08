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
#include "ns3/tcp-rcp.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("inrpp6");

uint32_t q;
bool tracing,tracing2;
std::string folder;
Time t;
uint32_t active_flows;
std::map<Ptr<PacketSink> ,uint32_t> flows;
uint32_t n;
uint32_t m;
std::vector<Ptr<PacketSink> > sink;
std::map<Ptr<PacketSink> ,uint32_t> data;
std::map<Ptr<PacketSink> ,uint32_t> data2;
Ptr<OutputStreamWrapper> logstream;

//std::map<Ptr<PacketSink> ,Ptr<OutputStreamWrapper> > jit1;
//std::map<Ptr<PacketSink> ,DelayJitterEstimation> jit2;

std::map<Ptr<PacketSink> ,Time> arrival;
std::map<uint16_t,Time> fct;
Ptr<OutputStreamWrapper> flowstream;
uint32_t segmentSize = 1446;
std::string protocol;


uint32_t 		maxBytes;
uint32_t cache=0;

Ptr<UdpServer> udp;

void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad);
//void TxPacket (Ptr<PacketSink> sink, Ptr<const Packet> p);
void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev, uint16_t port);
//void StopFlow(Ptr<PacketSink> p, Ptr<const Packet> packet, const Address &);
void StopFlow(Ptr<PacketSink> p, uint16_t port, uint32_t size);

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


void LogCache(Ptr<InrppL3Protocol> inrpp)
{
	AsciiTraceHelper asciiTraceHelper;
	std::ostringstream osstr21;
	osstr21 << folder << "/netcache_"<<cache<<".bf";
	Ptr<OutputStreamWrapper> streamtr21 = asciiTraceHelper.CreateFileStream (osstr21.str());
    inrpp->GetCache()->TraceConnectWithoutContext ("Size", MakeBoundCallback (&BufferChange, streamtr21));
    cache++;

}

void LogState(Ptr<Node> node, Ptr<InrppInterface> iface,uint32_t state);


int
main (int argc, char *argv[])
{
	  uint32_t 	packetSize = 1500;
	  t = Simulator::Now();
	  q=0;
	  tracing = false;
	  tracing2 = true;
	  std::string   bottleneck="10Mbps";
      uint32_t 	    bneck = 10000000;
	  std::string   usersbw="1Mbps";
	  uint32_t 	    mean_n_pkts = 500;
	  uint32_t 		maxBytes = 1000000;
	  uint32_t      maxPackets = (bneck * 0.05)/(8);
	  uint32_t      maxTh = maxPackets;
	  uint32_t      minTh = maxPackets/2;
	  uint32_t	    hCacheTh  = bneck * 12.5;
	  uint32_t	    lCacheTh  = hCacheTh/2;
	  uint32_t	    maxCacheSize = hCacheTh*2;
	  uint32_t 		stop = 100;
	  n = 5;
	  uint32_t as = 3;
	  double 		time = 0.1;
	  protocol = "i";
	  double load = 0.9;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("tracing2", "Flag to enable/disable tracing", tracing2);
  cmd.AddValue ("protocol","protocol",protocol);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", mean_n_pkts);
  cmd.AddValue ("n","number of flows", n);
  cmd.AddValue ("time","interflow time",time);
  cmd.AddValue ("stop","stop time",stop);

  cmd.Parse (argc, argv);

  std::ostringstream st;
  st << protocol << "test4_fl"<<n;
  folder = st.str();

	if(protocol=="t"){
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1458));

	} else if(protocol=="r"){
		 Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpRcp::GetTypeId ()));
		  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1434));
		Config::SetDefault ("ns3::TcpSocketBase::Timestamp", BooleanValue (true));
		Config::SetDefault ("ns3::RcpQueue::upd_timeslot_", DoubleValue(0.005));


	} else if(protocol=="i"){
		maxPackets = maxPackets*2;
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpInrpp::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
		Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (maxCacheSize));
		Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (hCacheTh));
		Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (lCacheTh));
		Config::SetDefault ("ns3::InrppL3Protocol::NumSlot", UintegerValue (6*n));
		Config::SetDefault ("ns3::InrppInterface::Refresh", DoubleValue (0.01));

	}

	Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (maxBytes));
	Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (maxBytes));

	Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));



//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes " << n);

  NodeContainer core;
  NodeContainer edges;
  NodeContainer routers;
  PointToPointHelper pointToPoint;

	pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bottleneck));
	pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

	if(protocol=="t"){
		pointToPoint.SetQueue ("ns3::DropTailQueue",
							   "MaxBytes", UintegerValue(maxPackets));
	} else if (protocol=="i") {

		pointToPoint.SetQueue ("ns3::InrppTailQueue",
							   "LowerThBytes", UintegerValue (minTh),
							   "HigherThBytes", UintegerValue (maxTh),
							   "MaxBytes", UintegerValue(maxPackets));
	} else if (protocol=="r"){

		pointToPoint.SetQueue ("ns3::RcpQueue",
		"MaxBytes", UintegerValue(maxPackets*1000),
		"DataRate", StringValue (bottleneck));
	}
  uint32_t net1 = 0;

  AsciiTraceHelper asciiTraceHelper;


  uint32_t k = 0;
  for(uint32_t i=0;i<as;i++)
  {
	  NodeContainer nodes;
	  nodes.Create(3);

	  NS_LOG_LOGIC("AS " << i);
	  std::vector<NetDeviceContainer> devs;
	  NetDeviceContainer devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
	  devs.push_back(devices0);
	  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get(0),nodes.Get(2));
	  devs.push_back(devices1);
	  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
	  devs.push_back(devices2);

	  for (unsigned i=0; i < devs.size(); i++) {

		  std::ostringstream devosstr;
		  devosstr << folder << "/p2pdevice_"<<k<<".tr";
		  Ptr<OutputStreamWrapper> streamtrdev = asciiTraceHelper.CreateFileStream (devosstr.str());
		  devs[i].Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtrdev));


		  PointerValue ptr;
		  devs[i].Get(0)->GetAttribute ("TxQueue", ptr);
		  Ptr<Queue> txQueue = ptr.Get<Queue> ();
		  std::ostringstream osstrbf;
		  osstrbf << folder << "/link_"<<k<<".bf";
		  Ptr<OutputStreamWrapper> streamtrbf = asciiTraceHelper.CreateFileStream (osstrbf.str());
		  txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtrbf));

		  k++;
	  }

	  core.Add(nodes.Get(0));
	  edges.Add(nodes.Get(1));
	  edges.Add(nodes.Get(2));

	  routers.Add(nodes);

	  if (protocol=="i") {
		InrppStackHelper inrpp;
		inrpp.Install (nodes);
	  } else
	  {
		InternetStackHelper inrpp;
		inrpp.Install (nodes);
	  }

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
		}
	  net1++;
  }

  NodeContainer servers;
  servers.Create(2);
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
	if(protocol=="t"){
	pointToPoint.SetQueue ("ns3::DropTailQueue",
					   "MaxBytes", UintegerValue(maxPackets*1000));
	} else if (protocol=="i") {

	pointToPoint.SetQueue ("ns3::InrppTailQueue",
			"LowerThBytes", UintegerValue (minTh*1000),
			"HigherThBytes", UintegerValue (maxTh*1000),
			"MaxBytes", UintegerValue(maxPackets*1000));
	} else if (protocol=="r"){

	pointToPoint.SetQueue ("ns3::RcpQueue",
	"MaxBytes", UintegerValue(maxPackets*100000),
	"DataRate", StringValue ("10Gbps"));
	}

  NetDeviceContainer serversDev = pointToPoint.Install (servers.Get(0),servers.Get(1));

	if (protocol=="i") {
		InrppStackHelper inrpp;
		inrpp.Install (servers);
	} else
	{
		InternetStackHelper inrpp;
		inrpp.Install (servers);
	}

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("12.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign (serversDev);
  core.Add(servers.Get(0));

  routers.Add(servers);

  std::vector<NetDeviceContainer> cores;
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

  uint32_t net2=0;
  std::vector<Ipv4InterfaceContainer> coreIfaces;

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

  std::vector<NetDeviceContainer> sourceLinks;


  std::vector<NodeContainer> senders;
  std::vector<NodeContainer> receivers;

  for(uint32_t i=0;i<edges.GetN();i++)
  {
	NodeContainer clients;
 	clients.Create(n);

	NodeContainer server;
	server.Create(n);

	NS_LOG_LOGIC("Number of clients in edge " << i << " is: " << clients.GetN());

	for(uint32_t j=0;j<clients.GetN();j++)
	{
		//Ptr<Node> server =  CreateObject<Node>();
		pointToPoint.SetDeviceAttribute ("DataRate", StringValue (usersbw));

		if(protocol=="t"){
				pointToPoint.SetQueue ("ns3::DropTailQueue",
									   "MaxBytes", UintegerValue(maxPackets));
		} else if (protocol=="i") {
			pointToPoint.SetQueue ("ns3::InrppTailQueue",
									   "LowerThBytes", UintegerValue (minTh),
									   "HigherThBytes", UintegerValue (maxTh),
									   "MaxBytes", UintegerValue(maxPackets));
		} else if (protocol=="r"){

			pointToPoint.SetQueue ("ns3::RcpQueue",
			"MaxBytes", UintegerValue(maxPackets*10000),
			"DataRate", StringValue ("1Mbps"));
		}
	    InternetStackHelper internet;
	    internet.Install (server.Get(j));
		NetDeviceContainer serverDev = pointToPoint.Install (servers.Get(1),server.Get(j));
		std::stringstream netAddr;
		netAddr << "13." << net2 << "." << num2 << ".0";
		Ipv4AddressHelper ipv4;
		std::string str = netAddr.str();
		ipv4.SetBase(str.c_str(), "255.255.255.0");
		NS_LOG_LOGIC("Set up address " << str);
		Ipv4InterfaceContainer i0 = ipv4.Assign (serverDev);
		ifaces.push_back(i0);
		sourceLinks.push_back(serverDev);
		num2++;

		if(num2==255)
		{
		  num2=0;
		  net2++;
		}

		NetDeviceContainer sourceLink = pointToPoint.Install (clients.Get(j),edges.Get(i));
		clientDevs.push_back(sourceLink);

	 }
     net2++;
     num2=0;

	 InternetStackHelper internet;
     internet.Install (clients);

     senders.push_back(server);
     receivers.push_back(clients);

  }


  uint32_t net = 0;
  int num = 0;



  for(uint32_t i=0;i<senders.size();i++)
  {


	  double lambda = ((bneck * load) / (packetSize * mean_n_pkts * 8.0));

	  NS_LOG_LOGIC("Lambda " << lambda);

	  Ptr<ExponentialRandomVariable> m_rv_flow_intval = CreateObject<ExponentialRandomVariable> ();
	  m_rv_flow_intval->SetAttribute("Mean", DoubleValue(1.0/lambda));
	  m_rv_flow_intval->SetAttribute("Stream", IntegerValue(2));

	  Ptr<RandomVariableStream> m_rv_npkts = CreateObject<ParetoRandomVariable> ();
	  m_rv_npkts->SetAttribute("Mean", DoubleValue(mean_n_pkts*packetSize));
	  m_rv_npkts->SetAttribute("Shape", DoubleValue(1.2));
	  m_rv_npkts->SetAttribute("Stream", IntegerValue(-1));

	  double start = 1.0;

	  for(uint32_t j=0;j<senders[i].GetN();j++)
	  {

		  uint32_t nuser = (i*n)+j;
		  std::stringstream netAddr;
		  netAddr << "14." << net << "." << num++ << ".0";
		  Ipv4AddressHelper ipv4;
		  std::string str = netAddr.str();
		  NS_LOG_LOGIC("Set up address " << str);
		  ipv4.SetBase(str.c_str(), "255.255.255.0");
		  Ipv4InterfaceContainer iSource = ipv4.Assign (clientDevs[nuser]);

		  uint16_t port = 9000+nuser;  // well-known echo port number

		  uint32_t bytes = m_rv_npkts->GetInteger();


		  BulkSendHelper source ("ns3::TcpSocketFactory",
								 InetSocketAddress (iSource.GetAddress (0), port));
		  // Set the amount of data to send in bytes.  Zero is unlimited.
		  source.SetAttribute ("MaxBytes", UintegerValue (bytes));
		  ApplicationContainer sourceApps = source.Install (senders[i].Get(j));
		  //Ptr<ExponentialRandomVariable> exp = CreateObject<ExponentialRandomVariable> ();
		  //exp->SetAttribute ("Mean", DoubleValue (0.1));
		  //lastTime = lastTime + exp->GetValue();

		  if(j>0)start+=m_rv_flow_intval->GetValue();


		  sourceApps.Start (Seconds(start));
		  sourceApps.Stop (Seconds (stop));

		  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
		  bulk->SetCallback(MakeCallback(&StartLog));
		  bulk->SetNetDevice(sourceLinks[nuser].Get(1));

		  PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
							   InetSocketAddress (Ipv4Address::GetAny (), port));
		  ApplicationContainer sinkApps = sink1.Install (receivers[i].Get(j));
		  sinkApps.Start (Seconds (0.0));
		  sinkApps.Stop (Seconds (stop));
		  Ptr<PacketSink> psink = DynamicCast<PacketSink> (sinkApps.Get (0));
		  psink->SetCallback(MakeCallback(&StopFlow));
		  //psink->TraceConnectWithoutContext("Rx", MakeBoundCallback (&StopFlow, psink));

		  sink.push_back(psink);


		  if(tracing2)
		  {

			  AsciiTraceHelper asciiTraceHelper;
			  std::ostringstream osstr;
			  osstr << folder << "/netdeviceRx_"<<nuser<<".tr";
			  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
			  DynamicCast<PacketSink> (sinkApps.Get (0))->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr));

			  /*bulk->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxPacket,psink));
			  DelayJitterEstimation jitter = DelayJitterEstimation();
			  std::ostringstream osstr2;
			  osstr2 << folder << "/jitter_"<<nuser<<".tr";
			  Ptr<OutputStreamWrapper> streamjitter = asciiTraceHelper.CreateFileStream (osstr2.str());
			  jit1.insert(std::make_pair(psink,streamjitter));
			  jit2.insert(std::make_pair(psink,jitter));
			  arrival.insert(std::make_pair(psink,Simulator::Now()));*/
		  }

		  if(num==255)
		  {
			  num=0;
			  net++;
		  }

	   }

  }

   if(protocol=="i"){
	   InrppGlobalRoutingHelper::PopulateRoutingTables ();
   } else {
	   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
   }


/*  if (tracing)
	{
	  std::ostringstream osstr;
	  osstr << folder << "/inrpp17-client";
	  //pointToPoint.EnablePcap(osstr.str(),nodes, false);
	  pointToPoint.EnablePcap(osstr.str(),allClients, false);

	  std::ostringstream osstr2;
	  osstr2 << folder << "/inrpp17-server";
	 //pointToPoint.EnablePcap(osstr.str(),nodes, false);
	  pointToPoint.EnablePcap(osstr2.str(),senders, false);

	}*/

  if(tracing2)
  {
	  if(protocol=="i"){
		  std::ostringstream osstrlog;
		  osstrlog << folder << "/logcache.tr";
		  logstream = asciiTraceHelper.CreateFileStream (osstrlog.str());

		  for(uint32_t j = 0; j < routers.GetN(); j++)
		  {
			  NS_LOG_LOGIC("Logstate node " << j);
			  Ptr<InrppL3Protocol> ip = routers.Get(j)->GetObject<InrppL3Protocol> ();
			  ip->SetCallback(MakeBoundCallback(&LogState,routers.Get(j)));

		  }
	  }


  }
	std::ostringstream osstrfct;
	osstrfct << folder << "/flows.tr";
	flowstream = asciiTraceHelper.CreateFileStream (osstrfct.str());

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stop));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");


}


void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev, uint16_t port)
{
	active_flows++;
	NS_LOG_LOGIC("Start flow " << active_flows << " " << port);

	socket->BindToNetDevice(netDev);

	fct.insert(std::make_pair(port,Simulator::Now()));

	if(tracing2)
	  {
		  AsciiTraceHelper asciiTraceHelper;
		  std::ostringstream osstr;
		  osstr << folder << "/netdevice_"<<q<<".tr";
		  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
		  if(socket->GetObject<TcpInrpp>())socket->GetObject<TcpInrpp>()->TraceConnectWithoutContext ("Throughput", MakeBoundCallback (&BwChange, streamtr));
		  q++;

		  std::ostringstream oss2;
		  oss2 << folder << "/netdevice_"<<q<<".rtt";
		  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (oss2.str());
		  socket->TraceConnectWithoutContext("RTT", MakeBoundCallback (&RttTracer, stream2));
  }


}

//void StopFlow(Ptr<PacketSink> p, Ptr<const Packet> packet, const Address &)
void StopFlow(Ptr<PacketSink> p, uint16_t port, uint32_t size)
{

	NS_LOG_LOGIC("Flow ended " <<active_flows);
	flows.insert(std::make_pair(p,active_flows));

	active_flows--;

	std::map<uint16_t,Time>::iterator it = fct.find(port);
	if(it!=fct.end())
		*flowstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << port << "\t" << Simulator::Now ().GetSeconds ()-it->second.GetSeconds() << "\t" << size/segmentSize << "\t" << active_flows <<  std::endl;

	data.erase(p);
	sink.erase(std::remove(sink.begin(), sink.end(), p), sink.end());


}

void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad)
{
	std::map<Ptr<PacketSink>,uint32_t>::iterator it = data.find(psink);
	uint32_t rx = it->second;
	rx+=p->GetSize();
	if(it!=data.end()){
		data.erase(it);
		data.insert(std::make_pair(psink,rx));
	}

}


/*void
TxPacket (Ptr<PacketSink> sink, Ptr<const Packet> p)
{

	std::map<Ptr<PacketSink>,DelayJitterEstimation>::iterator it2;
	it2=jit2.find(sink);
	DelayJitterEstimation jitter = it2->second;
	jitter.PrepareTx(p);
	jit2.erase(it2);
	jit2.insert(std::make_pair(sink,jitter));
}*/

void LogState(Ptr<Node> node, Ptr<InrppInterface> iface,uint32_t state){

	NS_LOG_LOGIC("Inrpp state changed " << iface << " at node " << node->GetId() << " to state " << state);
	*logstream->GetStream () << Simulator::Now ().GetSeconds () << " " << iface << " at node " << node->GetId() << " to state " << state << std::endl;

}



