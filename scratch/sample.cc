//* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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

// for Rocketfuel topology reading
#include "ns3/rocketfuel-map-reader.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("inrpp6");

/*uint32_t i;
bool tracing,tracing2;
std::string folder;
Time t;
std::map<Ptr<PacketSink> ,uint32_t> flows;
uint32_t n;
std::vector<Ptr<PacketSink> > sink;*/
std::map<uint16_t,Time> data;
std::map<Ptr<NetDevice>,DataRate> rate;
std::string folder;
uint32_t packetSize;

uint32_t active_flows;
Ptr<OutputStreamWrapper> flowstream,utilstream;

static void
BufferChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;

}


static void
<<<<<<< HEAD
//BwChange (DataRate dr, double oldCwnd, double newCwnd)
BwChange (Ptr<NetDevice> netDev, double oldCwnd, double newCwnd)
{
	std::map<Ptr<NetDevice>,DataRate>::iterator it = rate.find(netDev);
	if(it!=rate.end())
	  *utilstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << netDev<<"\t"<<newCwnd/(it->second).GetBitRate()<< std::endl;
=======
BwChange (DataRate dr, double oldCwnd, double newCwnd)
{
  //*stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
	//NS_LOG_INFO ("Link utilization "<<Simulator::Now ().GetSeconds () << "\t" << newCwnd << "\t" << dr.GetBitRate() <<"\t"<<newCwnd/dr.GetBitRate());
	  *utilstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd/dr.GetBitRate() << std::endl;
>>>>>>> 088b301b09dc5c3e6e1ceacd08c182e06e5d1b04

}

/*
void LogCache(Ptr<InrppL3Protocol> inrpp)
{
	AsciiTraceHelper asciiTraceHelper;
	std::ostringstream osstr21;
	osstr21 << folder << "/netcache_0.bf";
	Ptr<OutputStreamWrapper> streamtr21 = asciiTraceHelper.CreateFileStream (osstr21.str());
    inrpp->GetCache()->TraceConnectWithoutContext ("Size", MakeBoundCallback (&BufferChange, streamtr21));

}*/

//void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad);

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev,  uint16_t port);
void StopFlow(Ptr<PacketSink> p, uint16_t port,uint32_t size);
//void LogState(Ptr<InrppInterface> iface,uint32_t state);

int
main (int argc, char *argv[])
{

	packetSize = 1500;
	//t = Simulator::Now();
	std::string topo_file_name = "3257.pop.cch";
	std::string protocol = "t";
	//std::string topo_file_name = "4755.pop.cch";
	//i=0;
	//tracing = true;
	//tracing2 = true;
	//uint32_t 		maxBytes = 10000000;
	bool pcap_tracing=false;
	bool pcap_tracing2=false;
	uint32_t 		stop = 1000;
	uint32_t n = 1;
	//double 		time = 0.01;
	std::string b2b_bottleneck="1Gbps";
	std::string b2g_bottleneck = "1Gbps";
	std::string c2g_bottleneck = "100Mbps";

	uint32_t 	  bneck = 1000000000;
	uint32_t 	  mean_n_pkts = (0.015*bneck)/(8*packetSize);

	uint32_t      maxPackets = (bneck * 0.005)/(8);

	//uint32_t      maxTh = maxPackets;
	//uint32_t      minTh = maxPackets/2;

	uint32_t	  hCacheTh  = bneck * 10/8;
	uint32_t	  lCacheTh  = hCacheTh/2;
	uint32_t	  maxCacheSize = hCacheTh*2;
	double load = 0.8;

	//
	// Allow the user to override any of the defaults at
	// run-time, via command-line arguments
	//
	CommandLine cmd;
	//cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
	//cmd.AddValue ("tracing2", "Flag to enable/disable tracing", tracing2);

	cmd.AddValue ("maxBytes",
				"Total number of bytes for application to send", mean_n_pkts);
	cmd.AddValue ("n","number of flows", n);
	cmd.AddValue ("protocol","protocol",protocol);
	//cmd.AddValue ("time","interflow time",time);
	cmd.AddValue ("stop","stop time",stop);
	//cmd.AddValue ("bottleneck","bottleneck",bottleneck);
	cmd.AddValue ("load","load",load);
	cmd.AddValue ("topo_file_name","topo_file_name",topo_file_name);

	cmd.Parse (argc, argv);

	std::ostringstream st;
	st << protocol << "rockettest_fl" <<n;
	folder = st.str();

    if (mean_n_pkts < 30)
		mean_n_pkts = 100;

	if(protocol=="t"){
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1458));

	} else if(protocol=="r"){
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpRcp::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1434));
		Config::SetDefault ("ns3::TcpSocketBase::Timestamp", BooleanValue (true));
		Config::SetDefault ("ns3::RcpQueue::upd_timeslot_", DoubleValue(0.005));

	} else if(protocol=="i"){
		//maxPackets = maxPackets*2;
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpInrpp::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
		Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (maxCacheSize));
		Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (hCacheTh));
		Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (lCacheTh));

		Config::SetDefault ("ns3::InrppInterface::Refresh", DoubleValue (0.01));
	}

	Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (10000000));
	Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (10000000));

	Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

   //Read RocketFuel Topology
	RocketfuelParams params;
	params.averageRtt = 2.0;
	params.clientNodeDegrees = 2;
	params.minb2bDelay = "1ms";
	params.minb2bBandwidth = b2b_bottleneck;
	params.maxb2bDelay = "1ms";
	params.maxb2bBandwidth = b2b_bottleneck;
	params.minb2gDelay = "1ms";
	params.minb2gBandwidth = b2g_bottleneck;
	params.maxb2gDelay = "1ms";
	params.maxb2gBandwidth = b2g_bottleneck;
	params.ming2cDelay = "1ms";
	params.ming2cBandwidth = c2g_bottleneck;
	params.maxg2cDelay = "1ms";
	params.maxg2cBandwidth = c2g_bottleneck;

	RocketfuelMapReader topo_reader(topo_file_name, 10); //the second paramater has to do with visualizing
	topo_reader.SetFileName(topo_file_name);
	NS_LOG_INFO ("Read Topology.");
	NodeContainer nodes = topo_reader.Read(params, true, false); //Onur: Set the last parameter to true, if you need a fully-connected backbone

	// We have there kinds of nodes: BackBone, GateWay and Customer (i.e., edge)
	// Customer nodes are the ones whose degree is less than params.clientNodeDegrees
	// A Node is a Gateway node if it is connected to at least one customer node
	// A node is a BackBone node if it is not connected to a customer node
	const NodeContainer& BackBoneRouters = topo_reader.GetBackboneRouters();
	const NodeContainer& GatewayRouters = topo_reader.GetGatewayRouters();
	const NodeContainer& CustomerRouters = topo_reader.GetCustomerRouters();

	if(protocol=="i")
	{
		Config::SetDefault ("ns3::InrppL3Protocol::NumSlot", UintegerValue ((CustomerRouters.GetN()*n)+9000));
		Config::SetDefault ("ns3::TcpInrpp::NumSlot", UintegerValue ((CustomerRouters.GetN()*n)+9000));
	}

	NS_LOG_INFO("Number of customer (edge) routers: "<<CustomerRouters.GetN());
	NS_LOG_INFO("Number of gateway routers: "<<GatewayRouters.GetN());
	NS_LOG_INFO("Number of backbone (core) routers: "<<BackBoneRouters.GetN());

	//
	// Explicitly create the point-to-point link required by the topology (shown above).
	//

    Ipv4NixVectorHelper nixRouting;
	Ipv4StaticRoutingHelper staticRouting;

	Ipv4ListRoutingHelper list;
	list.Add (staticRouting, 0);
	list.Add (nixRouting, 10);

	PointToPointHelper pointToPoint;

    InternetStackHelper internet;
	InrppStackHelper inrpp;
	if(protocol=="t"||protocol=="r"){
		internet.SetRoutingHelper (list); // has effect on the next Install ()
		internet.Install (nodes);
	} else if (protocol=="i") {
		//inrpp.SetRoutingHelper (list); // has effect on the next Install ()
		inrpp.Install (nodes);
	}

	int num = 0;
	int net = 0;

	AsciiTraceHelper asciiTraceHelper;

	std::ostringstream osstr13;
	osstr13 << folder << "/util.tr";
	utilstream = asciiTraceHelper.CreateFileStream (osstr13.str());

	NS_LOG_INFO ("Create channels.");
	//pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bottleneck));
	//pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));
	//Iterate through all the TopologyReader::Link objects and form the "real" links
    std::list<TopologyReader::Link> links = topo_reader.GetLinks();
    uint32_t l=1;

        uint32_t l=1;
	for (std::list<TopologyReader::Link>::iterator it = links.begin(); it != links.end(); it++) {
		Ptr<Node> fromNode = it->GetFromNode();
		Ptr<Node> toNode = it->GetToNode();
	//	NS_LOG_LOGIC("Delay " << it->GetAttribute("Delay"));
	    DataRate bitrate(it->GetAttribute("DataRate"));
	    NS_LOG_LOGIC("Data rate " << bitrate.GetBitRate());

		pointToPoint.SetDeviceAttribute ("DataRate", StringValue (it->GetAttribute("DataRate")));
		pointToPoint.SetChannelAttribute ("Delay", StringValue (it->GetAttribute("Delay")));
		//pointToPoint.SetQueue ("ns3::DropTailQueue",
		//					   "MaxBytes", UintegerValue(maxPackets));
		if(protocol=="t"){
			pointToPoint.SetQueue ("ns3::DropTailQueue",
								   "MaxBytes", UintegerValue(maxPackets*100));
		} else if (protocol=="i") {

			pointToPoint.SetQueue ("ns3::InrppTailQueue",
								 //  "LowerThBytes", UintegerValue (minTh),
								 //  "HigherThBytes", UintegerValue (maxTh),
								   "MaxBytes", UintegerValue(maxPackets*10));
		} else if (protocol=="r"){

			pointToPoint.SetQueue ("ns3::RcpQueue",
			"MaxBytes", UintegerValue(maxPackets*100),
			"DataRate", StringValue (it->GetAttribute("DataRate")));
		}

		NetDeviceContainer devices = pointToPoint.Install (fromNode, toNode);
		//
		// We've got the "hardware" in place.  Now we need to add IP addresses.
		//
		NS_LOG_INFO ("Assign IP Addresses: "<<l);
<<<<<<< HEAD
		rate.insert(std::make_pair(devices.Get(0),bitrate));
		rate.insert(std::make_pair(devices.Get(1),bitrate));
		*utilstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << devices.Get(0)<<"\t"<<0 << std::endl;
		*utilstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << devices.Get(1)<<"\t"<<0 << std::endl;
		//std::ostringstream devosstr;
		//devosstr << folder << "/p2pdevice_0.tr";
		//Ptr<OutputStreamWrapper> streamtrdev = asciiTraceHelper.CreateFileStream (devosstr.str());
		devices.Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, devices.Get(0)));
		devices.Get(1)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, devices.Get(1)));
=======

		//std::ostringstream devosstr;
		//devosstr << folder << "/p2pdevice_0.tr";
		//Ptr<OutputStreamWrapper> streamtrdev = asciiTraceHelper.CreateFileStream (devosstr.str());
		devices.Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, bitrate.GetBitRate()));
>>>>>>> 088b301b09dc5c3e6e1ceacd08c182e06e5d1b04

		std::stringstream netAddr;
		netAddr << "10." << net << "." << (num) << ".0";
		Ipv4AddressHelper ipv4;
		std::string str = netAddr.str();
		ipv4.SetBase(str.c_str(), "255.255.255.0");
		Ipv4InterfaceContainer i0 = ipv4.Assign (devices);
		num++;
		if(num==256)
		{
			num=0;
			net++;
		}
		l++;
	}

	num = 0;
	net = 0;
	NodeContainer senders;
	NodeContainer receivers;

	Ptr<ExponentialRandomVariable> m_rv_flow_intval;
	Ptr<RandomVariableStream> m_rv_npkts;

	DataRate dr(c2g_bottleneck);

    uint32_t num_customers = CustomerRouters.GetN();
	senders.Create(n*(num_customers/2));
	receivers.Create(n*(num_customers/2));

    NS_LOG_LOGIC("Senders " << senders.GetN());
    NS_LOG_LOGIC("Receivers " << receivers.GetN());

    uint32_t source_net = 11;
    uint32_t dest_net = 110;
    //uint32_t dest_net2 = 210;

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    x->SetAttribute ("Min", DoubleValue (0));
    x->SetAttribute ("Max", DoubleValue (num_customers-1));

    uint32_t num_pair = 0;
	for(uint32_t pair = 0; pair < num_customers/2; pair++)
    //for(uint32_t pair = 0; pair < num_customers; pair++)
	{

		Ptr<Node> fromNode = CustomerRouters.Get(pair);
		Ptr<Node> toNode = CustomerRouters.Get((pair+(num_customers/2)));
		//Ptr<Node> fromNode = CustomerRouters.Get(pair);
		//uint32_t dest = x->GetInteger();
		//while(dest==pair)dest = x->GetInteger();
		NS_LOG_LOGIC("From node " << pair << " to node " << pair+1);
		//Ptr<Node> toNode = CustomerRouters.Get(dest);
	    double time = 1.0;
	    double lambda = ((dr.GetBitRate() * load) / ((mean_n_pkts) * 1500 * 8.0));
		m_rv_flow_intval= CreateObject<ExponentialRandomVariable> ();

		m_rv_flow_intval->SetAttribute("Mean", DoubleValue(1.0/lambda));
		m_rv_flow_intval->SetAttribute("Stream", IntegerValue(2));

		m_rv_npkts = CreateObject<ParetoRandomVariable> ();
		m_rv_npkts->SetAttribute("Mean", DoubleValue(mean_n_pkts*packetSize));
		m_rv_npkts->SetAttribute("Shape", DoubleValue(1.2));
		m_rv_npkts->SetAttribute("Stream", IntegerValue(-1));

	    for(uint32_t i=0;i<n;i++)
        {
			NetDeviceContainer sourceLink;
			NetDeviceContainer destLink;
			pointToPoint.SetDeviceAttribute ("DataRate", StringValue (c2g_bottleneck));
			pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));
			//pointToPoint.SetQueue ("ns3::DropTailQueue","MaxBytes", UintegerValue(maxPackets*100));
			NS_LOG_LOGIC("Pair " << num_pair);

			if(protocol=="t"){
							pointToPoint.SetQueue ("ns3::DropTailQueue",
											   "MaxBytes", UintegerValue(maxPackets*100));

			} else if (protocol=="i") {

				pointToPoint.SetQueue ("ns3::InrppTailQueue",
								   //"LowerThBytes", UintegerValue (minTh),
								   //"HigherThBytes", UintegerValue (maxTh),
								   "MaxBytes", UintegerValue(maxPackets*100));


			} else if (protocol=="r"){

				pointToPoint.SetQueue ("ns3::RcpQueue",
				"MaxBytes", UintegerValue(maxPackets*100),
				"DataRate", StringValue (c2g_bottleneck));

			}
			sourceLink = pointToPoint.Install (fromNode, senders.Get(num_pair));
			destLink = pointToPoint.Install (toNode, receivers.Get(num_pair));

			//InternetStackHelper inrpp;
			//if(protocol!="i"){
			internet.Install (senders.Get(num_pair));
			internet.Install (receivers.Get(num_pair));
			//} else {
			//	inrpp.Install (senders.Get(num_pair));
			//	inrpp.Install (receivers.Get(num_pair));
			//}

			std::stringstream netAddr;
			netAddr << source_net << "." << net << "." << (num) << ".0";
			Ipv4AddressHelper ipv4;
			std::string str = netAddr.str();
			ipv4.SetBase(str.c_str(), "255.255.255.0");
			Ipv4InterfaceContainer iSource = ipv4.Assign (sourceLink);

			std::stringstream netAddr2;
			netAddr2 << dest_net << "." << net << "." << (num) << ".0";
			str = netAddr2.str();
			ipv4.SetBase(str.c_str(), "255.255.255.0");
			Ipv4InterfaceContainer iDest = ipv4.Assign (destLink);


			uint16_t port = 9000+num_pair;  // well-known echo port number

			NS_LOG_LOGIC("Ip " << iSource.GetAddress(0));
			NS_LOG_LOGIC("Ip 2 " << iDest.GetAddress(1));
			//NS_LOG_LOGIC("Ip 3 " << iDest2.GetAddress(1));

			uint32_t packets = m_rv_npkts->GetInteger();


			BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address(iDest.GetAddress (1)), port));
			source.SetAttribute("MaxBytes", UintegerValue(packets));
			ApplicationContainer sourceApps = source.Install (senders.Get(num_pair));
			time+=m_rv_flow_intval->GetValue();


			//BulkSendHelper source ("ns3::TcpSocketFactory",
			//			 InetSocketAddress (iDest.GetAddress (1), port));
			// Set the amount of data to send in bytes.  Zero is unlimited.
			//source.SetAttribute ("MaxBytes", UintegerValue (packets));
			//ApplicationContainer sourceApps = source.Install (senders.Get(pair*n + i));
			//time+=m_rv_flow_intval->GetValue();

			NS_LOG_LOGIC("New flow at " << time << " " << packets << " pkts long");
			sourceApps.Start (Seconds (time));
			sourceApps.Stop (Seconds (stop));

			PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
			//ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
			ApplicationContainer sinkApps = sink.Install (receivers.Get(num_pair));
			sinkApps.Start(Seconds(0.0));
			sinkApps.Stop(Seconds(stop));

			Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
			bulk->SetCallback(MakeCallback(&StartLog));
			bulk->SetNetDevice(sourceLink.Get(1));
			Ptr<PacketSink> psink = DynamicCast<PacketSink> (sinkApps.Get (0));
			psink->SetCallback(MakeCallback(&StopFlow));

			num++;
			if(num==256)
			{
				num=0;
				net++;
			}
			if(net==256)
			{
				net=0;
				source_net++;
				dest_net++;
				//dest_net2++;
			}
			   num_pair++;

		} // end of i's
	} // end of pairs

	if(pcap_tracing)
	{
		std::ostringstream osstr3;
		osstr3 << folder << "/inrpp2";
		//pointToPoint.EnablePcap(osstr.str(),nodes, false);
		pointToPoint.EnablePcap(osstr3.str(),senders, false);
		pointToPoint.EnablePcap(osstr3.str(),receivers, false);
	}

	if(protocol=="i")InrppGlobalRoutingHelper::PopulateRoutingTables ();


	std::ostringstream osstr3;
	osstr3 << folder << "/flows.tr";
	flowstream = asciiTraceHelper.CreateFileStream (osstr3.str());

<<<<<<< HEAD
=======
	std::ostringstream osstr13;
	osstr13 << folder << "/util.tr";
	utilstream = asciiTraceHelper.CreateFileStream (osstr13.str());
>>>>>>> 088b301b09dc5c3e6e1ceacd08c182e06e5d1b04

	NS_LOG_INFO ("Run Simulation.");
	Simulator::Stop (Seconds (stop));
	Simulator::Run ();
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");

}


void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev,  uint16_t port)
{
	active_flows++;
	NS_LOG_LOGIC("Start flow " << port << " " << active_flows);
	socket->BindToNetDevice(netDev);
	data.insert(std::make_pair(port,Simulator::Now()));

}

void StopFlow(Ptr<PacketSink> p, uint16_t port, uint32_t size)
{
	active_flows--;
	NS_LOG_LOGIC("Flow ended " << port << " " << active_flows << " " << size);

	std::map<uint16_t,Time>::iterator it = data.find(port);
	if(it!=data.end())
		*flowstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << port << "\t" << Simulator::Now ().GetSeconds ()-it->second.GetSeconds() << "\t" << size/packetSize << "\t" << active_flows <<  std::endl;

}


/*void LogState(Ptr<InrppInterface> iface,uint32_t state){

	NS_LOG_LOGIC("Inrpp state changed " << iface << " to state " << state);
}*/


