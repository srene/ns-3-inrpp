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
#include "ns3/tcp-rcp.h"

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

std::string folder;

uint32_t active_flows;
Ptr<OutputStreamWrapper> flowstream;

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


void LogCache(Ptr<InrppL3Protocol> inrpp)
{
	AsciiTraceHelper asciiTraceHelper;
	std::ostringstream osstr21;
	osstr21 << folder << "/netcache_0.bf";
	Ptr<OutputStreamWrapper> streamtr21 = asciiTraceHelper.CreateFileStream (osstr21.str());
    inrpp->GetCache()->TraceConnectWithoutContext ("Size", MakeBoundCallback (&BufferChange, streamtr21));

}
void Sink(Ptr<PacketSink> psink, Ptr<const Packet> p,const Address &ad);

void StartLog(Ptr<Socket> socket,Ptr<NetDevice> netDev,  uint16_t port);
void StopFlow(Ptr<PacketSink> p, uint16_t port);
void LogState(Ptr<InrppInterface> iface,uint32_t state);

int
main (int argc, char *argv[])
{
	//t = Simulator::Now();
	std::string protocol = "i";
	//i=0;
	//tracing = true;
	//tracing2 = true;
	//uint32_t 		maxBytes = 10000000;
    bool pcap_tracing=true;
	uint32_t 		stop = 300;
	uint32_t n = 1000;
	//double 		time = 0.01;
	std::string bottleneck="1Gbps";
	uint32_t bneck = 1000000000;
	uint32_t mean_n_pkts = (0.015*bneck)/(8*1500);

	uint32_t      maxPackets = (bneck * 0.05)/(8);

	uint32_t      maxTh = maxPackets;
	uint32_t      minTh = maxPackets/2;

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
	//cmd.AddValue ("time","interflow time",time);
	cmd.AddValue ("stop","stop time",stop);
	cmd.AddValue ("protocol","protocol",protocol);
	cmd.AddValue ("bottleneck","bottleneck",bottleneck);
	cmd.AddValue ("load","load",load);

	cmd.Parse (argc, argv);

	std::ostringstream st;
	st << protocol << "test_fl" <<n;
	folder = st.str();

    if (mean_n_pkts < 30)
		mean_n_pkts = 30;


	if(protocol=="t"){
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1458));

	} else if(protocol=="r"){
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpRcp::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));

	} else if(protocol=="i"){
		maxPackets = maxPackets*2;
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpInrpp::GetTypeId ()));
		Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
		Config::SetDefault ("ns3::InrppCache::MaxCacheSize", UintegerValue (maxCacheSize));
		Config::SetDefault ("ns3::InrppCache::HighThresholdCacheSize", UintegerValue (hCacheTh));
		Config::SetDefault ("ns3::InrppCache::LowThresholdCacheSize", UintegerValue (lCacheTh));
		Config::SetDefault ("ns3::InrppL3Protocol::NumSlot", UintegerValue (n));
		Config::SetDefault ("ns3::InrppInterface::Refresh", DoubleValue (0.01));

	}

	Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (10000000));
	Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (10000000));

	Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

	//
	// Explicitly create the nodes required by the topology (shown above).
	//
	NS_LOG_INFO ("Create nodes.");
	NodeContainer nodes;
	nodes.Create ((2*n)+2);

	NS_LOG_INFO ("Create channels.");

	//
	// Explicitly create the point-to-point link required by the topology (shown above).
	//
	PointToPointHelper pointToPoint;

	if(protocol=="t"){
		pointToPoint.SetQueue ("ns3::DropTailQueue",
							   "MaxBytes", UintegerValue(maxPackets));
		InternetStackHelper inrpp;
		inrpp.Install (nodes.Get(0));
		inrpp.Install (nodes.Get(1));
	} else if (protocol=="i") {

		pointToPoint.SetQueue ("ns3::InrppTailQueue",
							   "LowerThBytes", UintegerValue (minTh),
							   "HigherThBytes", UintegerValue (maxTh),
							   "MaxBytes", UintegerValue(maxPackets));

		InrppStackHelper inrpp;
		inrpp.Install (nodes.Get(0));
		inrpp.Install (nodes.Get(1));
	} else if (protocol=="r"){

		pointToPoint.SetQueue ("ns3::RcpQueue",
		"MaxBytes", UintegerValue(maxPackets),
		"DataRate", StringValue (bottleneck));

		InternetStackHelper inrpp;
		inrpp.Install (nodes.Get(0));
		inrpp.Install (nodes.Get(1));
	}

	pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bottleneck));
	pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));
	NetDeviceContainer devices = pointToPoint.Install (nodes.Get(0),nodes.Get(1));



	//
	// We've got the "hardware" in place.  Now we need to add IP addresses.
	//
    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i0 = ipv4.Assign (devices);

	int num = 0;
	int net = 0;
	NodeContainer senders;
	NodeContainer receivers;

	DataRate dr(bottleneck);

	double lambda = ((dr.GetBitRate() * load) / ((mean_n_pkts) * 1500 * 8.0));


	Ptr<ExponentialRandomVariable> m_rv_flow_intval = CreateObject<ExponentialRandomVariable> ();
	m_rv_flow_intval->SetAttribute("Mean", DoubleValue(1.0/lambda));
	m_rv_flow_intval->SetAttribute("Stream", IntegerValue(2));

	Ptr<RandomVariableStream> m_rv_npkts = CreateObject<ParetoRandomVariable> ();
	m_rv_npkts->SetAttribute("Mean", DoubleValue(mean_n_pkts*1500));
	m_rv_npkts->SetAttribute("Shape", DoubleValue(1.2));
	m_rv_npkts->SetAttribute("Stream", IntegerValue(-1));


	double time = 1.0;

	for(uint32_t i=0;i<n;i++)
	{

		NetDeviceContainer sourceLink;
		NetDeviceContainer destLink;

		if(protocol=="t"){
			pointToPoint.SetQueue ("ns3::DropTailQueue",
								   "MaxBytes", UintegerValue(maxPackets));
			sourceLink = pointToPoint.Install (nodes.Get(2+i),nodes.Get(0));
			destLink = pointToPoint.Install (nodes.Get(1),nodes.Get(2+n+i));

			InternetStackHelper inrpp;
			inrpp.Install (nodes.Get(2+i));
			inrpp.Install (nodes.Get(2+n+i));
		} else if (protocol=="i") {

			pointToPoint.SetQueue ("ns3::InrppTailQueue",
								   "LowerThBytes", UintegerValue (minTh),
								   "HigherThBytes", UintegerValue (maxTh),
								   "MaxBytes", UintegerValue(maxPackets));

			sourceLink = pointToPoint.Install (nodes.Get(2+i),nodes.Get(0));
			destLink = pointToPoint.Install (nodes.Get(1),nodes.Get(2+n+i));

			InrppStackHelper inrpp;
			inrpp.Install (nodes.Get(2+i));
			inrpp.Install (nodes.Get(2+n+i));
		} else if (protocol=="r"){

			pointToPoint.SetQueue ("ns3::RcpQueue",
			"MaxBytes", UintegerValue(maxPackets),
			"DataRate", StringValue (bottleneck));

			sourceLink = pointToPoint.Install (nodes.Get(2+i),nodes.Get(0));
			destLink = pointToPoint.Install (nodes.Get(1),nodes.Get(2+n+i));

			InternetStackHelper inrpp;
			inrpp.Install (nodes.Get(2+i));
			inrpp.Install (nodes.Get(2+n+i));
		}


		senders.Add(nodes.Get(2+i));

		receivers.Add(nodes.Get(2+n+i));

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

		uint16_t port = 9000+i;  // well-known echo port number

		NS_LOG_LOGIC("Ip " << iSource.GetAddress(0));
		NS_LOG_LOGIC("Ip 2 " << iDest.GetAddress(1));



		uint32_t packets = m_rv_npkts->GetInteger();

		BulkSendHelper source ("ns3::TcpSocketFactory",
						 InetSocketAddress (iDest.GetAddress (1), port));
		// Set the amount of data to send in bytes.  Zero is unlimited.
		source.SetAttribute ("MaxBytes", UintegerValue (packets));
		ApplicationContainer sourceApps = source.Install (nodes.Get(2+i));
		time+=m_rv_flow_intval->GetValue();

		NS_LOG_LOGIC("New flow at " << time << " " << packets << " pkts long");
		sourceApps.Start (Seconds (time));
		sourceApps.Stop (Seconds (stop));

		PacketSinkHelper sink1 ("ns3::TcpSocketFactory",
					   InetSocketAddress (Ipv4Address::GetAny (), port));
		ApplicationContainer sinkApps = sink1.Install (nodes.Get(2+n+i));
		sinkApps.Start (Seconds (0.0));
		sinkApps.Stop (Seconds (stop));

		Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (sourceApps.Get (0));
		bulk->SetCallback(MakeCallback(&StartLog));
		bulk->SetNetDevice(sourceLink.Get(0));
		Ptr<PacketSink> psink = DynamicCast<PacketSink> (sinkApps.Get (0));
		psink->SetCallback(MakeCallback(&StopFlow));


		AsciiTraceHelper asciiTraceHelper;
		std::ostringstream osstr;
		osstr << folder << "/netdeviceRx_"<<i<<".tr";
		Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
		DynamicCast<PacketSink> (sinkApps.Get (0))->TraceConnectWithoutContext ("EstimatedBW", MakeBoundCallback (&BwChange, streamtr));


		num++;
		if(num==256)
		{
		num=0;
		net++;
		}

	}


	if(pcap_tracing)
	{
		std::ostringstream osstr3;
		osstr3 << folder << "/inrpp2";
		//pointToPoint.EnablePcap(osstr.str(),nodes, false);
		pointToPoint.EnablePcap(osstr3.str(),senders, false);
		pointToPoint.EnablePcap(osstr3.str(),receivers, false);
	}

	if(protocol=="i"){
		InrppGlobalRoutingHelper::PopulateRoutingTables ();
		Ptr<InrppL3Protocol> ip = nodes.Get(0)->GetObject<InrppL3Protocol> ();
		ip->SetCallback(MakeCallback(&LogState));
		Simulator::Schedule(Seconds(1.0),&LogCache,ip);


	}else
		Ipv4GlobalRoutingHelper::PopulateRoutingTables ();



	AsciiTraceHelper asciiTraceHelper;
	PointerValue ptr;
	devices.Get(0)->GetAttribute ("TxQueue", ptr);
	Ptr<Queue> txQueue = ptr.Get<Queue> ();
	std::ostringstream osstr;
	osstr << folder << "/netdevice_0.bf";
	Ptr<OutputStreamWrapper> streamtr = asciiTraceHelper.CreateFileStream (osstr.str());
	txQueue->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr));

	PointerValue ptr2;
	devices.Get(1)->GetAttribute ("TxQueue", ptr2);
	Ptr<Queue> txQueue2 = ptr2.Get<Queue> ();
	std::ostringstream osstr2;
	osstr2 << folder << "/netdevice_1.bf";
	Ptr<OutputStreamWrapper> streamtr2 = asciiTraceHelper.CreateFileStream (osstr2.str());
	txQueue2->GetObject<DropTailQueue>()->TraceConnectWithoutContext ("BytesQueue", MakeBoundCallback (&BufferChange, streamtr2));


	std::ostringstream osstr3;
	osstr3 << folder << "/flows.tr";
	flowstream = asciiTraceHelper.CreateFileStream (osstr3.str());
	//
	// Now, do the actual simulation.
	//
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

void StopFlow(Ptr<PacketSink> p, uint16_t port)
{
	active_flows--;
	NS_LOG_LOGIC("Flow ended " << port << " " << active_flows);

	std::map<uint16_t,Time>::iterator it = data.find(port);
	if(it!=data.end())
		*flowstream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << port << "\t" << Simulator::Now ().GetSeconds ()-it->second.GetSeconds() << "\t" << active_flows <<  std::endl;

}


void LogState(Ptr<InrppInterface> iface,uint32_t state){

	NS_LOG_LOGIC("Inrpp state changed " << iface << " to state " << state);
}
