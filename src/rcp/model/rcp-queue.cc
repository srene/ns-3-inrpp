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
#include "tcp-option-rcp.h"

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

  if (!header.HasOption (TcpOption::RCP))
  {
	  Ptr<TcpOptionRcp> ts = DynamicCast<TcpOptionRcp> (header.GetOption (TcpOption::RCP));

	  uint32_t request = ts->GetRate();

	  if (request < 0 || request > flow_rate_)
		  ts->SetRate(flow_rate_);
  }
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

void RcpQueue::Timeout()
{
  double temp;
  double datarate_fact;
  double estN1;
  double estN2;
  int Q_pkts;
  char clip;
  //int Q_target_;

  double ratio;
  double input_traffic_devider_;
  //double queueing_delay_;

  double virtual_link_capacity; // bytes per second

  last_load_ = act_input_traffic_/Tq_; // bytes per second

  Q_ = byteLength();
  Q_pkts = length();

  input_traffic_ = last_load_;
  if (input_traffic_rtt_ > 0)
    this_Tq_rtt_numPkts_ = this_Tq_rtt_sum_/input_traffic_rtt_;

  /*
  if (this_Tq_rtt_numPkts_ >= avg_rtt_)
      rtt_moving_gain_ = (flow_rate_/link_capacity_);
  else
      rtt_moving_gain_ = (flow_rate_/link_capacity_)*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);
   */
   if (this_Tq_rtt_numPkts_ >= avg_rtt_)
        rtt_moving_gain_ = (Tq_/avg_rtt_);
   else
        rtt_moving_gain_ = (flow_rate_/link_capacity_)*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);

  avg_rtt_ = RunningAvg(this_Tq_rtt_numPkts_, avg_rtt_, rtt_moving_gain_);

//  if (Q_ == 0)
//	  input_traffic_ = PHI*link_capacity_;
//  else
//	 input_traffic_ = link_capacity_ + (Q_ - Q_last)/Tq_;

//  queueing_delay_ = (Q_ ) / (link_capacity_ );
//  if ( avg_rtt_ > queueing_delay_ ){
//    propag_rtt_ = avg_rtt_ - queueing_delay_;
//  } else {
//    propag_rtt_ = avg_rtt_;
//  }


  estN1 = input_traffic_ / flow_rate_;
  estN2 = link_capacity_ / flow_rate_;

  if ( rate_fact_mode_ == 0) { // Masayoshi .. for Nandita's RCP

   virtual_link_capacity = gamma_ * link_capacity_;

    /* Estimate # of active flows with  estN2 = (link_capacity_/flow_rate_) */
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(virtual_link_capacity - input_traffic_) - beta_*(Q_/avg_rtt_)))/virtual_link_capacity);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 1) { // Masayoshi .. for fixed rate fact
    /* Fixed Rate Mode */
    temp = link_capacity_ * fixed_rate_fact_;

  } else if ( rate_fact_mode_ == 2) {

    /* Estimate # of active flows with  estN1 = (input_traffic_/flow_rate_) */

    if (input_traffic_ == 0.0 ){
      input_traffic_devider_ = link_capacity_/1000000.0;
    } else {
      input_traffic_devider_ = input_traffic_;
    }
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/avg_rtt_)))/input_traffic_devider_);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 3) {
    //if (input_traffic_ == 0.0 ){
    //input_traffic_devider_ = link_capacity_/1000000.0;
    //    } else {
    //input_traffic_devider_ = input_traffic_;
    //}
    ratio =  (1 + ((Tq_/propag_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/propag_rtt_)))/link_capacity_);
    temp = flow_rate_ * ratio;
  } else  if ( rate_fact_mode_ == 4) { // Masayoshi .. Experimental
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/avg_rtt_)))/link_capacity_);
    temp = flow_rate_ * ratio;
    // link_capacity_ : byte/sec
  } else  if ( rate_fact_mode_ == 5) {
    // temp = - link_capacity_ * ( Q_/(link_capacity_* avg_rtt_) - 1.0);
    //temp = flow_rate_ +  link_capacity_ * (alpha_ * (1.0 - input_traffic_/link_capacity_) - beta_ * ( Q_/(avg_rtt_*link_capacity_) )) * Tq_;
    temp = link_capacity_ * exp ( - Q_/(link_capacity_* avg_rtt_));
  } else  if ( rate_fact_mode_ == 6) {
    temp = link_capacity_ * exp ( - Q_/(link_capacity_* propag_rtt_));
  } else  if ( rate_fact_mode_ == 7) {
     temp = - link_capacity_ * ( Q_/(link_capacity_* propag_rtt_) - 1.0);
  } else  if ( rate_fact_mode_ == 8) {
    temp = flow_rate_ +  link_capacity_ * (alpha_ * (1.0 - input_traffic_/link_capacity_) - beta_/avg_rtt_ * ( Q_/(propag_rtt_*link_capacity_) - 0.8 )) * Tq_;
  }


  if ( rate_fact_mode_ != 4) { // Masayoshi .. Experimental
    if (temp < min_pprtt_ * (1000/avg_rtt_) ){     // Masayoshi
      flow_rate_ = min_pprtt_ * (1000/avg_rtt_) ; // min pkt per rtt
      clip  = 'L';
    } else if (temp > virtual_link_capacity){
      flow_rate_ = virtual_link_capacity;
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
  }else{
    if (temp < 16000.0 ){    // Masayoshi 16 KB/sec = 128 Kbps
      flow_rate_ = 16000.0;
      clip  = 'L';
    } else if (temp > link_capacity_){
      flow_rate_ = link_capacity_;
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
  }


//  else if (temp < 0 )
//	 flow_rate_ = 1000/avg_rtt_; // 1 pkt per rtt

  datarate_fact = flow_rate_/link_capacity_;

//   if (print_status_ == 1)
//   if (routerId_ == 0)
//      	   fprintf(stdout, "%f %d %f %f %f\n", now(), length(), datarate_fact, output_traffic_/(link_capacity_*Tq_), avg_rtt_);
//	fprintf(stdout, "%s %f %d %d %f %f %f\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_);
     //	fprintf(stdout, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(link_capacity_ - input_traffic_)/link_capacity_, (Q_/propag_rtt_)/link_capacity_,ratio,estN1,estN2,clip);

 //    if(channel_ != NULL){
 //      char buf[2048];
//       sprintf(buf, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(link_capacity_ - input_traffic_)/link_capacity_, (Q_/avg_rtt_)/link_capacity_,ratio,estN1,estN2,clip);
	   // sprintf(buf, "%f %d %.5lf %f %f %f %f %f %c\n", now(), byteLength(), datarate_fact, avg_rtt_, this_Tq_rtt_, this_Tq_rtt_numPkts_, last_load_, (link_capacity_ - input_traffic_)/link_capacity_, clip);
//       sprintf(buf, "%f r_ %f estN1_ %f estN2_ %f \n", now(), datarate_fact, estN1, estN2);
//	   (void)Tcl_Write(channel_, buf, strlen(buf));
   //  }

// fflush(stdout);

  Tq_ = min(avg_rtt_, upd_timeslot_);
  this_Tq_rtt_ = 0;
  this_Tq_rtt_sum_ = 0;
  input_traffic_rtt_ = 0;
  Q_last = Q_;
  act_input_traffic_ = traffic_spill_;
  traffic_spill_ = 0;
  output_traffic_ = 0.0;
  end_slot_ = now() + Tq_;
  queue_timer_.resched(Tq_);
}

double RcpQueue::RunningAvg(double var_sample, double var_last_avg, double gain)
{
	double avg;
	if (gain < 0)
	  exit(3);
	avg = gain * var_sample + ( 1 - gain) * var_last_avg;
	return avg;
}

} // namespace ns3

