/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
 *
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

#include "rcp-queue.h"

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/inrpp-header.h"
#include "ns3/ppp-header.h"
#include "ns3/simulator.h"
#include "ns3/ppp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RcpQueue");

NS_OBJECT_ENSURE_REGISTERED (RcpQueue);

TypeId RcpQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RcpQueue")
    .SetParent<DropTailQueue> ()
    .SetGroupName("Rcp")
    .AddConstructor<RcpQueue> ()
    /*.AddAttribute ("Mode",
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&DropTailQueue::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("HigherThPackets",
                   "The higher threshold number of packets accepted by this RcpQueue.",
                   UintegerValue (80),
                   MakeUintegerAccessor (&RcpQueue::m_highPacketsTh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("HigherThBytes",
                   "The higher threshold number of bytes accepted by this RcpQueue.",
                   UintegerValue (80 * 65535),
                   MakeUintegerAccessor (&RcpQueue::m_highBytesTh),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("LowerThPackets",
				   "The lower threshold number of packets accepted by this RcpQueue.",
				   UintegerValue (50),
				   MakeUintegerAccessor (&RcpQueue::m_lowPacketsTh),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("LowerThBytes",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   UintegerValue (50 * 65535),
				   MakeUintegerAccessor (&RcpQueue::m_lowBytesTh),
				   MakeUintegerChecker<uint32_t> ())*/
	.AddAttribute ("LinkCapacity",
					   "Moving average refresh value.",
					   DoubleValue (100000000),
					   MakeDoubleAccessor (&RcpQueue::m_linkCapacity),
					   MakeDoubleChecker<double> ())
	/*.AddTraceSource ("HigherThreshold", "Drop a packet stored in the queue.",
					 MakeTraceSourceAccessor (&RcpQueue::m_highTh),
					 "ns3::RcpQueue::HighThTracedCallback")
	.AddTraceSource ("LowerThreshold", "Drop a packet stored in the queue.",
					 MakeTraceSourceAccessor (&RcpQueue::m_lowTh),
					 "ns3::RcpQueue::LowThTracedCallback")*/
  ;

  return tid;
}

RcpQueue::RcpQueue () :
  DropTailQueue (),
  m_hTh(false),
  m_lTh(false)
  /*m_packets (),
  m_bytesInQueue (0)*/
{
  NS_LOG_FUNCTION (this);
}

RcpQueue::~RcpQueue ()
{
  NS_LOG_FUNCTION (this);
}

bool 
RcpQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << m_packets.size () << p->GetSize());

  DoOnPacketArrival(p);
  DropTailQueue::DoEnqueue(p);

  return true;
}

Ptr<Packet>
RcpQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p = DropTailQueue::DoDequeue();
  DoBeforePacketDeparture(p);
  return p;
}

void
RcpQueue::DoBeforePacketDeparture(Ptr<Packet> p)
{
	output_traffic_ += p->GetSize();
	Ptr<Packet> packet = p->Copy();
	PppHeader ppp;
	packet->RemoveHeader (ppp);
	TcpHeader tcpHeader;
	Ipv4Header ipHeader;
	packet->RemoveHeader(ipHeader);
	//UdpHeader udpHeader;
	// NS_LOG_LOGIC("IP " << ipHeader.GetSource() << " " << ipHeader.GetDestination());
	packet->PeekHeader(tcpHeader);


	/*if (tcpHeader.GetFlags () & TcpHeader::SYN)
	{
		FillInFeedback(tcpHeader);

	} else if ( hdr->RCP_pkt_type() == RCP_REF || hdr->RCP_pkt_type() == RCP_DATA )
	{
		FillInFeedback(tcpHeader);
	}*/
}

void
RcpQueue::FillInFeedback(const TcpHeader &header)
{

  hdr_rcp * hdr = hdr_rcp::access(p);
  double request = hdr->RCP_request_rate();

  // update avg_rtt_ here
  // double this_rtt = hdr->rtt();

  /*
  if (this_rtt > 0) {
    avg_rtt_ = running_avg(this_rtt, avg_rtt_, RTT_GAIN);
  }
  */

  if (request < 0 || request > flow_rate_)
    hdr->set_RCP_rate(flow_rate_);
}

void
RcpQueue::DoOnPacketArrival(Ptr<Packet> p)
{
  // Taking input traffic statistics
  uint32_t size = p->GetSize();
  uint32_t pkt_time = size/m_linkCapacity;
  double end_time = Simulator::Now().GetSeconds()+pkt_time;
  double part1, part2;

  /*int size = hdr_cmn::access(pkt)->size();

  hdr_rcp * hdr = hdr_rcp::access(pkt);
  double pkt_time_ = packet_time(pkt);
  double end_time = now() + pkt_time_;
  double part1, part2;

  // update avg_rtt_ here
  double this_rtt = hdr->rtt();

  if (this_rtt > 0) {

       this_Tq_rtt_sum_ += (this_rtt * size);
       input_traffic_rtt_ += size;
       this_Tq_rtt_ = running_avg(this_rtt, this_Tq_rtt_, flow_rate_/link_capacity_);

//     rtt_moving_gain_ = flow_rate_/link_capacity_;
//     avg_rtt_ = running_avg(this_rtt, avg_rtt_, rtt_moving_gain_);
  }

  if (end_time <= end_slot_)
    act_input_traffic_ += size;
  else {
    part2 = size * (end_time - end_slot_)/pkt_time_;
    part1 = size - part2;
   act_input_traffic_ += part1;
    traffic_spill_ += part2;
  }*/

  // Can do some measurement of queue length here
  // length() in packets and byteLength() in bytes

  /* Can read the flow size from a last packet here */
}


} // namespace ns3

