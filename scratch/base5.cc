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

//global variables
uint32_t i;
bool tracing,tracing2;
std::string folder;
Time t;
uint32_t active_flows;
std::map<Ptr<PacketSink> ,uint32_t> flows;
uint32_t n;
std::vector<Ptr<PacketSink> > sink;
std::map<Ptr<PacketSink> ,uint32_t> data;
uint32_t cache=0;
Ptr<OutputStreamWrapper> logstream;
Ptr<OutputStreamWrapper> fairstream;
Ptr<OutputStreamWrapper> linkstream;
uint32_t segmentSize = 1446;
std::map<Ipv4Address, Time> tmap;
//std::map<Ipv4Address,DelayJitterEstimation> jit;
std::map<uint16_t,Time> fct;
Ptr<OutputStreamWrapper> flowstream;
std::string protocol;
std::vector<NetDeviceContainer> dev;
Ipv4Address udpDest;
bool cross;
uint32_t stop;
//std::map<uint16_t,Ipv4Address> pMap;
//log methods
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
SeqChange (Ptr<OutputStreamWrapper> stream, SequenceNumber32 oldCwnd, SequenceNumber32 newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd.GetValue() << std::endl;

}

static void
RxPacket (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p , const Address &add)
{
   uint32_t size = p->GetSize();

   while(size>0){
	   std::map<Ipv4Address,Time>::iterator it = tmap.find(InetSocketAddress::ConvertFrom(add).GetIpv4 ());
	//   NS_LOG_LOGIC("Packet size " << size << " " << InetSocketAddress::ConvertFrom(add).GetIpv4 ());
	   *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << Simulator::Now().GetSeconds() - it->second.GetSeconds() << " " << std::endl;
	   tmap.erase(InetSocketAddress::ConvertFrom(add).GetIpv4 ());
	   tmap.insert(std::make_pair(InetSocketAddress::ConvertFrom(add).GetIpv4 (),Simulator::Now()));
	   if(size<segmentSize)break;
	   size-=segmentSize;

//	   std::map<Ipv4Address,DelayJitterEstimation>::iterator it2 = jit.find(InetSocketAddress::ConvertFrom(add).GetIpv4 ());
   }

}

static void
RttTracer (Ptr<OutputStreamWrapper> stream,Time oldval, Time newval)
{

  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad);

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev, uint16_t port);
void StopFlow(Ptr<PacketSink> p, uint16_t port, uint32_t size);
void LogState(Ptr<Node> node, Ptr<InrppInterface> iface,uint32_t state);
void TxPacket (Ipv4Address add, Ptr<const Packet> p);
void LogCache(Ptr<InrppL3Protocol> inrpp);
void LogCacheFlow(Ptr<OutputStreamWrapper> streamtr, Ptr<InrppL3Protocol> ip,Ptr<InrppInterface> iface,uint32_t flow);
void LogFairness();
void LogUtilization();
void GenerateTraffic(double startTime, double stopTime, Ptr<NetDevice> netDev, Ptr<Node> nodeSrc, Ptr<Node> nodeDst, uint32_t bw1, uint32_t bw2, Ipv4Address dest);

void
ChangeBW(Ptr<PointToPointNetDevice> netdev, DataRate dr)
{

	//netdev->SetDataRate(dr);
    Ptr<InrppL3Protocol> ip = netdev->GetNode()->GetObject<InrppL3Protocol> ();
	Ptr<InrppInterface> iface = ip->GetInterface(ip->GetInterfaceForDevice (netdev))->GetObject<InrppInterface>();
	iface->SetRate(dr);

}

int
main (int argc, char *argv[])
{
	  t = Simulator::Now();
	  i=0;
	  tracing = false;
	  tracing2 = true;
	  uint32_t 		maxBytes = 10000000;
	  stop = 1000;
	  n = 10;
	  std::string   bottleneck="10Mbps";
      uint32_t 	    bneck = 10000000;
	  double 		time = 1;
	  uint32_t      maxPackets = (bneck * 0.05)/(8);
	  //uint32_t		maxPackets = 10000;
	  //std::string   congestion="ns3::TcpMpTcp";
	  cross = false;
	  uint32_t      maxTh = maxPackets;
	  uint32_t      minTh = maxPackets/2;
	  uint32_t	    hCacheTh  = bneck * 12.5;
	  uint32_t	    lCacheTh  = hCacheTh/2;
	  uint32_t	    maxCacheSize = hCacheTh*2;
	  protocol = "i";
	 // double load = 0.8;
//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("tracing2", "Flag to enable/disable tracing", tracing2);
  cmd.AddValue ("protocol","protocol",protocol);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("n","number of flows", n);
  cmd.AddValue ("time","interflow time",time);
  cmd.AddValue ("stop","stop time",stop);
  cmd.AddValue ("cross","cross traffic", cross);
  cmd.Parse (argc, argv);

  std::ostringstream st;
  st << protocol << "test2_fl" <<n;
  folder = st.str();

  AsciiTraceHelper asciiTraceHelper;

  NS_LOG_LOGIC("Cache " << hCacheTh);

 	if(protocol=="t"){
 		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
 		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1458));

 	} else if(protocol=="r"){
 		 Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpRcp	::GetTypeId ()));
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
 		Config::SetDefault ("ns3::InrppL3Protocol::NumSlot", UintegerValue (n));
 		Config::SetDefault ("ns3::InrppInterface::Refresh", DoubleValue (0.001));

 	}

 	Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (maxBytes));
 	Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (maxBytes));

 	Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes " << n);
  NodeContainer nodes;
  nodes.Create ((2*n)+5);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
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
		"MaxBytes", UintegerValue(maxPackets),
		"DataRate", StringValue (bottleneck));
	}

  NetDeviceContainer devices0;
  NetDeviceContainer devices1;
  NetDeviceContainer devices2;
  NetDeviceContainer devices3;
  NetDeviceContainer devices4;
  NetDeviceContainer devices5;
  NetDeviceContainer devices6;
  NetDeviceContainer devices7;

  devices0 = pointToPoint.Install (nodes.Get(0),nodes.Get(2));
  dev.push_back(devices0);
  devices1 = pointToPoint.Install (nodes.Get(0),nodes.Get(1));
  dev.push_back(devices1);
  devices2 = pointToPoint.Install (nodes.Get(1),nodes.Get(2));
  dev.push_back(devices2);
  devices3 = pointToPoint.Install (nodes.Get(0),nodes.Get(3));
  dev.push_back(devices3);
  devices4 = pointToPoint.Install (nodes.Get(3),nodes.Get(1));
  dev.push_back(devices4);
  devices5 = pointToPoint.Install (nodes.Get(1),nodes.Get(4));
  dev.push_back(devices5);
  devices6 = pointToPoint.Install (nodes.Get(3),nodes.Get(4));
  dev.push_back(devices6);
  devices7 = pointToPoint.Install (nodes.Get(2),nodes.Get(4));
  dev.push_back(devices7);


  for(uint32_t i = 0; i<5;i++)
  {

		if (protocol=="i") {
			InrppStackHelper inrpp;
			inrpp.Install (nodes.Get(i));
		} else
		{
			InternetStackHelper inrpp;
			inrpp.Install (nodes.Get(i));
		}

  }
  //
  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
    NS_LOG_INFO ("Assign IP Addresses.");


  for(uint32_t i = 0; i<8;i++)
  {
    	std::stringstream netAddr;
	    netAddr << "10.0." << i << ".0";
	    Ipv4AddressHelper ipv4;
	    std::string str = netAddr.str();
	    ipv4.SetBase(str.c_str(), "255.255.255.0");
	    Ipv4InterfaceContainer i0 = ipv4.Assign (dev[i]);
	    if(i==0) udpDest = i0.GetAddress(1);
   }



   NodeContainer senders;
   NodeContainer receivers;

  // double lambda = ((bneck * load) / (maxBytes * 8.0));

		  int num = 0;
		  int net = 0;
   Ptr<ExponentialRandomVariable> m_rv_flow_intval = CreateObject<ExponentialRandomVariable> ();
	m_rv_flow_intval->SetAttribute("Mean", DoubleValue(time));
	m_rv_flow_intval->SetAttribute("Stream", IntegerValue(2));


	double start = 1.0;
	for(uint32_t i=0;i<n;i++)
	{

		NetDeviceContainer sourceLink,destLink;

		pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
		pointToPoint.SetQueue ("ns3::DropTailQueue",
								   "MaxBytes", UintegerValue(maxPackets*100));

		if(protocol=="t"){
			pointToPoint.SetQueue ("ns3::DropTailQueue",
								   "MaxBytes", UintegerValue(maxPackets*100));
		} else if (protocol=="i") {
			pointToPoint.SetQueue ("ns3::InrppTailQueue",
									   "LowerThBytes", UintegerValue (minTh*100),
									   "HigherThBytes", UintegerValue (maxTh*100),
									   "MaxBytes", UintegerValue(maxPackets*100));
		} else if (protocol=="r"){

			pointToPoint.SetQueue ("ns3::RcpQueue",
			"MaxBytes", UintegerValue(maxPackets*100),
			"DataRate", StringValue ("100Mbps"));
		}

		//if(i%2==0)
		//{
			sourceLink = pointToPoint.Install (nodes.Get(5+i),nodes.Get(0));
			destLink = pointToPoint.Install (nodes.Get(2),nodes.Get(5+n+i));
		//} else
		//{
		//	sourceLink = pointToPoint.Install (nodes.Get(5+i),nodes.Get(3));
		//	destLink = pointToPoint.Install (nodes.Get(4),nodes.Get(5+n+i));
		//}



		InternetStackHelper inrpp;
		inrpp.Install (nodes.Get(5+i));
		inrpp.Install (nodes.Get(5+n+i));

	  senders.Add(nodes.Get(5+i));
	  receivers.Add(nodes.Get(5+n+i));

	  std::stringstream netAddr;
	  netAddr << "11." << net << "." << num << ".0";
	  Ipv4AddressHelper ipv4;
	  std::string str = netAddr.str();
	  ipv4.SetBase(str.c_str(), "255.255.255.0");
	  Ipv4InterfaceContainer iSource = ipv4.Assign (sourceLink);

	  std::stringstream netAddr2;
	  netAddr2 << "12." << net << "." << num << ".0";
	  str = netAddr2.str();
	  ipv4.SetBase(str.c_str(), "255.255.255.0");
	  Ipv4InterfaceContainer iDest = ipv4.Assign (destLink);

	  if(tracing2)
	  {
	  	PointerValue ptr3;
	  	sourceLink.Get(0)->GetAttribute ("TxQueue", ptr3);
	  	Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
	  	std::ostringstream osstr3;
	  	osstr3 << folder << "/netdevice_" <<5+i<<".bf";
	  	Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
	  	txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));

		PointerValue ptr33;
		sourceLink.Get(1)->GetAttribute ("TxQueue", ptr33);
		Ptr<Queue> txQueue33 = ptr33.Get<Queue> ();
		std::ostringstream osstr33;
		osstr33 << folder << "/netdevice2_" <<5+i<<".bf";
		Ptr<OutputStreamWrapper> streamtr33 = asciiTraceHelper.CreateFileStream (osstr33.str());
		txQueue33->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr33));

	  	tmap.insert(std::make_pair(iSource.GetAddress(0),Simulator::Now()));
	  }
	  //txQueue3->Trt 	aceConnectWithoutContext ("Drop", MakeCallback (&Drop));
	  //NS_LOG_INFO ("Create Applications.");


	  uint16_t port = 9000+i;  // well-known echo port number

	  if(i>0)start+=m_rv_flow_intval->GetValue();

	  /*  NS_LOG_LOGIC("Start flow " << i);
	  BulkSendHelper source ("ns3::TcpSocketFactory",
	                         InetSocketAddress (iDest.GetAddress (1), port));
	  // Set the amount of data to send in bytes.  Zero is unlimited.
	  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
	  ApplicationContainer sourceApps = source.Install (nodes.Get(5+i));
	  sourceApps.Start (Seconds (start));
	  sourceApps.Stop (Seconds (stop));*/
	  BulkSendHelper source ("ns3::TcpSocketFactory",
	                          InetSocketAddress (iDest.GetAddress (1)	, port));
	   // Set the amount of data to send in bytes.  Zero is unlimited.
	   source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
	   ApplicationContainer sourceApps = source.Install (nodes.Get(5+i));
	   sourceApps.Start (Seconds (start));
	   sourceApps.Stop (Seconds (stop));


	  // pMap.insert(std::make_pair(port,iDest2.GetAddress (1)));

	  /*PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
						   InetSocketAddress (Ipv4Address::GetAny (), port));
	  ApplicationContainer sinkApps = sink1.Install (nodes.Get(5+n+i));
	  sinkApps.Start (Seconds (0.0));
	  sinkApps.Stop (Seconds (stop));*/

	  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
	  bulk->SetCallback(MakeCallback(&StartLog));
	  bulk->SetNetDevice(sourceLink.Get(0));

	  PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
	  	                          InetSocketAddress (Ipv4Address::GetAny (), port));
	  ApplicationContainer sinkApps = sink1.Install (nodes.Get(5+n+i));
	  sinkApps.Start (Seconds (start));
	  sinkApps.Stop (Seconds (stop));

      Ptr<PacketSink> psink = DynamicCast<PacketSink> (sinkApps.Get (0));
      psink->SetCallback(MakeCallback(&StopFlow));
	 // Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
	 // std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
	  sink.push_back(psink);

   	  if(tracing2)
	  {
		  std::ostringstream osstr;
		  osstr << folder << "/netdeviceRx_"<<i<<".tr";
		  Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
		  psink->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr));


		  std::ostringstream osstr0;
		  osstr0 << folder << "/netdeviceRx_"<<i<<".seq";
		  Ptr<OutputStreamWrapper> streamtr0 = asciiTraceHelper.CreateFileStream (osstr0.str());
		  psink->TraceConnectWithoutContext ("TotalRx", MakeBoundCallback (&BufferChange, streamtr0));

		  std::ostringstream osstr1;
		  osstr1 << folder << "/jitter_"<<i<<".tr";
		  Ptr<OutputStreamWrapper> streamtr1 = asciiTraceHelper.CreateFileStream (osstr1.str());
		  psink->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxPacket, streamtr1));


	  }

	  num++;
	  if(num==256)
	  {
		  num=0;
		  net++;
	  }

//	  Ipv4StaticRoutingHelper ipv4RoutingHelper;
//	  Ptr<Ipv4> ipv4B = nodes.Get(5+n+i)->GetObject<Ipv4> ();
//	  Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting (ipv4B);
//	  staticRoutingB->AddHostRouteTo (iSource.GetAddress(0), iDest.GetAddress(0),1);
//	  staticRoutingB->AddHostRouteTo (iSource2.GetAddress(0), iDest2.GetAddress(0),2);
  }

  if(cross)GenerateTraffic(5.0,15.0,devices0.Get(0),nodes.Get(0),nodes.Get(2),bneck/2,bneck,udpDest);

  std::ostringstream osstrfair;
  osstrfair << folder << "/fairness.tr";
  fairstream = asciiTraceHelper.CreateFileStream (osstrfair.str());
  Simulator::Schedule(Seconds(1.0),&LogFairness);

  std::ostringstream osstrlink;
  osstrlink << folder << "/linkutil.tr";
  linkstream = asciiTraceHelper.CreateFileStream (osstrlink.str());
  Simulator::Schedule(Seconds(1.0),&LogUtilization);

  if(protocol=="i")
 	  InrppGlobalRoutingHelper::PopulateRoutingTables ();
   else
 	  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /*Ptr<GlobalRouter> gr = nodes.Get(0)->GetObject<GlobalRouter>();
  gr->InjectRoute(Ipv4Address("12.0.0.0"),Ipv4Mask("255.255.0.0"));

  Ptr<GlobalRouter> gr1 = nodes.Get(2)->GetObject<GlobalRouter>();
  gr1->InjectRoute(Ipv4Address("11.0.0.0"),Ipv4Mask("255.255.0.0"));

  Ptr<GlobalRouter> gr2 = nodes.Get(3)->GetObject<GlobalRouter>();
  gr2->InjectRoute(Ipv4Address("14.0.0.0"),Ipv4Mask("255.255.0.0"));

  Ptr<GlobalRouter> gr3 = nodes.Get(4)->GetObject<GlobalRouter>();
  gr3->InjectRoute(Ipv4Address("13.0.0.0"),Ipv4Mask("255.255.0.0"));



  Ipv4GlobalRoutingHelper::RecomputeRoutingTables();*/


  if(tracing2)
  {
	/*  PointerValue ptr;
	  devices0.Get(0)->GetAttribute ("TxQueue", ptr);
	  Ptr<Queue> txQueue = ptr.Get<Queue> ();
	  AsciiTraceHelper asciiTraceHelper;
	  std::ostringstream osstr1;
	  osstr1 << folder << "/router2.bf";
	  Ptr<OutputStreamWrapper> streamtr1 = asciiTraceHelper.CreateFileStream (osstr1.str());
	  txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr1));

	  PointerValue ptr2;
	  devices2.Get(0)->GetAttribute ("TxQueue", ptr2);
	  Ptr<Queue> txQueue2 = ptr2.Get<Queue> ();
	  std::ostringstream osstr2;
	  osstr2 << folder << "/router1.bf";
	  Ptr<OutputStreamWrapper> streamtr2 = asciiTraceHelper.CreateFileStream (osstr2.str());
	  txQueue2->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr2));

	  PointerValue ptr3;
	  devices4.Get(0)->GetAttribute ("TxQueue", ptr3);
	  Ptr<Queue> txQueue3 = ptr3.Get<Queue> ();
	  std::ostringstream osstr3;
	  osstr3 << folder << "/router3.bf";
	  Ptr<OutputStreamWrapper> streamtr3 = asciiTraceHelper.CreateFileStream (osstr3.str());
	  txQueue3->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr3));


	  PointerValue ptr4;
	  devices2.Get(1)->GetAttribute ("TxQueue", ptr4);
	  Ptr<Queue> txQueue4 = ptr4.Get<Queue> ();
	  std::ostringstream osstr13;
	  osstr13 << folder << "/router4.bf";
	  Ptr<OutputStreamWrapper> streamtr13 = asciiTraceHelper.CreateFileStream (osstr13.str());
	  txQueue4->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr13));

	  PointerValue ptr5;
	  devices3.Get(1)->GetAttribute ("TxQueue", ptr5);
	  Ptr<Queue> txQueue5 = ptr5.Get<Queue> ();
	  std::ostringstream osstr14;
	  osstr14 << folder << "/router5.bf";
	  Ptr<OutputStreamWrapper> streamtr14 = asciiTraceHelper.CreateFileStream (osstr14.str());
	  txQueue5->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr14));
*/
	  if(protocol=="i")
	  {
		  Ptr<InrppL3Protocol> ip = nodes.Get(0)->GetObject<InrppL3Protocol> ();
		  Ptr<InrppL3Protocol> ip2 = nodes.Get(1)->GetObject<InrppL3Protocol> ();
		  Ptr<InrppL3Protocol> ip3 = nodes.Get(4)->GetObject<InrppL3Protocol> ();
		  Ptr<InrppL3Protocol> ip4 = nodes.Get(2)->GetObject<InrppL3Protocol> ();
		  Ptr<InrppL3Protocol> ip5 = nodes.Get(3)->GetObject<InrppL3Protocol> ();

		  std::ostringstream osstrlog;
		  osstrlog << folder << "/logcache.tr";
		  logstream = asciiTraceHelper.CreateFileStream (osstrlog.str());

		  Simulator::Schedule(Seconds(1.0),&LogCache,ip);
		  Simulator::Schedule(Seconds(1.0),&LogCache,ip2);
		  Simulator::Schedule(Seconds(1.0),&LogCache,ip3);
		  Simulator::Schedule(Seconds(1.0),&LogCache,ip4);
		  Simulator::Schedule(Seconds(1.0),&LogCache,ip5);

		  ip->SetCallback(MakeBoundCallback(&LogState,nodes.Get(0)));
		  ip2->SetCallback(MakeBoundCallback(&LogState,nodes.Get(1)));
		  ip3->SetCallback(MakeBoundCallback(&LogState,nodes.Get(4)));
		  ip4->SetCallback(MakeBoundCallback(&LogState,nodes.Get(2)));
		  ip5->SetCallback(MakeBoundCallback(&LogState,nodes.Get(3)));

		  std::ostringstream osstr4;
		  osstr4 << folder << "/router_1_i0.otr";
		  Ptr<OutputStreamWrapper> streamtr4 = asciiTraceHelper.CreateFileStream (osstr4.str());
		  uint32_t iface = ip->GetInterfaceForDevice(devices0.Get(0));
		  ip->GetInterface(iface)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("OutputThroughput", MakeBoundCallback (&BwChange, streamtr4));

		  std::ostringstream osstr7;
		  osstr7 << folder << "/router_1_i0.itr";
		  Ptr<OutputStreamWrapper> streamtr7 = asciiTraceHelper.CreateFileStream (osstr7.str());
		  ip->GetInterface(iface)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("InputThroughput", MakeBoundCallback (&BwChange, streamtr7));

		  std::ostringstream osstr6;
		  osstr6 << folder << "/router_1_i1.dtr";
		  Ptr<OutputStreamWrapper> streamtr6 = asciiTraceHelper.CreateFileStream (osstr6.str());
		  uint32_t iface2 = ip->GetInterfaceForDevice(devices1.Get(0));
		  ip->GetInterface(iface2)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("DetouredThroughput", MakeBoundCallback (&BwChange, streamtr6));

		  std::ostringstream osstr10;
		  osstr10 << folder << "/router_0_i0.itr";
		  Ptr<OutputStreamWrapper> streamtr10 = asciiTraceHelper.CreateFileStream (osstr10.str());
		  uint32_t iface3 = ip5->GetInterfaceForDevice(devices6.Get(0));
		  ip5->GetInterface(iface3)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("InputThroughput", MakeBoundCallback (&BwChange, streamtr10));

		  std::ostringstream osstr5;
		  osstr5 << folder << "/router_0_i0.otr";
		  Ptr<OutputStreamWrapper> streamtr5 = asciiTraceHelper.CreateFileStream (osstr5.str());
		  ip5->GetInterface(iface3)->GetObject<InrppInterface>()->TraceConnectWithoutContext ("OutputThroughput", MakeBoundCallback (&BwChange, streamtr5));
	  }


	  for (unsigned i=0; i < dev.size(); i++) {
		  std::ostringstream devosstr;
		  devosstr << folder << "/p2pdevice_"<<i<<".tr";
		  Ptr<OutputStreamWrapper> streamtrdev = asciiTraceHelper.CreateFileStream (devosstr.str());
		  dev[i].Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtrdev));


		  PointerValue ptr;
		  dev[i].Get(0)->GetAttribute ("TxQueue", ptr);
		  Ptr<Queue> txQueue = ptr.Get<Queue> ();
		  std::ostringstream osstrbf;
		  osstrbf << folder << "/link_"<<i<<".bf";
		  Ptr<OutputStreamWrapper> streamtrbf = asciiTraceHelper.CreateFileStream (osstrbf.str());
		  txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtrbf));

	  }
  }

  if (tracing)
  {
	  std::ostringstream osstr;
	  osstr << folder << "/inrpp2";
//	  pointToPoint.EnablePcap(osstr.str(),nodes, false);
  pointToPoint.EnablePcap(osstr.str(),senders, false);
	  pointToPoint.EnablePcap(osstr.str(),receivers, false);
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

void LogCache(Ptr<InrppL3Protocol> inrpp)
{
	AsciiTraceHelper asciiTraceHelper;
	std::ostringstream osstr21;
	osstr21 << folder << "/netcache_"<<cache<<".bf";
	Ptr<OutputStreamWrapper> streamtr21 = asciiTraceHelper.CreateFileStream (osstr21.str());
    inrpp->GetCache()->TraceConnectWithoutContext ("Size", MakeBoundCallback (&BufferChange, streamtr21));
    cache++;

}

void LogCacheFlow(Ptr<OutputStreamWrapper> streamtr, Ptr<InrppL3Protocol> ip,Ptr<InrppInterface> iface,uint32_t flow)
{

	//NS_LOG_LOGIC("Cache flow " << flow << " " << ip->GetCache() << " " << ip->GetCache()->GetSize(iface,flow));
	*streamtr->GetStream () << Simulator::Now ().GetSeconds () << "\t" << ip->GetCache()->GetSize(iface,flow) << std::endl;
	Simulator::Schedule(Seconds(0.01),&LogCacheFlow,streamtr,ip,iface,flow);

}

void LogState(Ptr<Node> node, Ptr<InrppInterface> iface,uint32_t state){

	NS_LOG_LOGIC("Inrpp state changed " << iface << " at node " << node->GetId() << " to state " << state);
	*logstream->GetStream () << Simulator::Now ().GetSeconds () << " " << iface << " at node " << node->GetId() << " to state " << state << std::endl;

}

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev, uint16_t port)
{
	active_flows++;
	NS_LOG_LOGIC("Start flow " << active_flows);

	socket->BindToNetDevice(netDev);

	fct.insert(std::make_pair(port,Simulator::Now()));

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


		  std::ostringstream oss2;
		  oss2 << folder << "/netdevice_"<<i<<".rtt";
		  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (oss2.str());
		  socket->TraceConnectWithoutContext("RTT", MakeBoundCallback (&RttTracer, stream2));


		  std::ostringstream oss3;
		  oss3 << folder << "/netdevice_"<<i<<".seq";
		  Ptr<OutputStreamWrapper> stream3 = asciiTraceHelper.CreateFileStream (oss3.str());
		  socket->TraceConnectWithoutContext("NextTxSequence", MakeBoundCallback (&SeqChange, stream3));


		  i++;

  }

}

void StopFlow(Ptr<PacketSink> p, uint16_t port, uint32_t size)
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
	data.erase(it);
	data.insert(std::make_pair(psink,rx));

}


void LogFairness()
{

	double fairness=1.0;
	double num = 0.0;
	double den = 0.0;
	uint32_t n = 0;

	for (std::vector<Ptr<PacketSink> >::iterator it = sink.begin() ; it != sink.end(); ++it)
	{
		Ptr<PacketSink> p = *it;
		if(p->GetTr()>0)
		{
		//	NS_LOG_LOGIC("GetTr " << p->GetTr());
			n++;
			num += p->GetTr();
			den += pow(p->GetTr(),2);

		}
	}
	if(n>0)
		fairness = pow(num,2)/(den*n);

	*fairstream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << fairness << std::endl;

	if(sink.size()>0)Simulator::Schedule(Seconds(0.01),&LogFairness);

}

void LogUtilization()
{

	double util=1.0;
	double num = 0.0;

	for (unsigned i=0; i < dev.size(); i++)
	{
		num += dev[i].Get(0)->GetObject<PointToPointNetDevice>()->GetLinkUtilization();
	}
	util = num/dev.size();

	*linkstream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << util << std::endl;

	Simulator::Schedule(Seconds(0.01),&LogUtilization);

}

void
GenerateTraffic(double duration, double startTime, Ptr<NetDevice> netDev, Ptr<Node> nodeSrc, Ptr<Node> nodeDst, uint32_t bw1, uint32_t bw2, Ipv4Address dest)
{
// Create one udpServer applications on node one.
//
  uint16_t udpport = 4000;
  UdpServerHelper server (udpport);
  ApplicationContainer apps = server.Install (nodeDst);
  apps.Start (Seconds (startTime));
  apps.Stop (Seconds (startTime+duration));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1470;
  uint32_t maxPacketCount = 1000000000;

  double interval = (double)((MaxPacketSize+30)*8)/(double)(bw1);

  for(double t = startTime; t < stop; t+=startTime)
  {
	  NS_LOG_LOGIC("Udp Interval " << interval << " " << duration << " " << t);
  UdpClientHelper client (dest, udpport);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (Seconds(interval)));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  apps = client.Install (nodeSrc);
  apps.Start (Seconds (t));
  apps.Stop (Seconds (t+duration));

	if(protocol=="i")
	{
		DataRate datarate(bw1);
		Simulator::Schedule(Seconds(t),&ChangeBW,netDev->GetObject<PointToPointNetDevice>(),datarate);
		DataRate datarate2(bw2);
		Simulator::Schedule(Seconds(t+duration),&ChangeBW,netDev->GetObject<PointToPointNetDevice>(),datarate2);
	}
  }
 // Simulator::Schedule(Seconds(startTime),&GenerateTraffic,duration,startTime,netDev,nodeSrc,nodeDst,bw1,bw2,dest);

}

