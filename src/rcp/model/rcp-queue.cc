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
#include "ns3/tcp-option-ts.h"

//#include <random>
//#include <iostream>

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
	.AddAttribute ("PHI",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.95),
				   MakeDoubleAccessor (&RcpQueue::m_phi),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("RTT",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.2),
				   MakeDoubleAccessor (&RcpQueue::m_rtt),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("RTT_GAIN",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.02),
				   MakeDoubleAccessor (&RcpQueue::m_rttGain),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("Alpha",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.4),
				   MakeDoubleAccessor (&RcpQueue::m_alpha),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("Beta",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.4),
				   MakeDoubleAccessor (&RcpQueue::m_beta),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("Gamma",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (1),
				   MakeDoubleAccessor (&RcpQueue::m_gamma),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("min_pprtt_",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.01),
				   MakeDoubleAccessor (&RcpQueue::min_pprtt_),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("init_rate_fact_",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.05),
				   MakeDoubleAccessor (&RcpQueue::init_rate_fact_),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("fixed_rate_fact_",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue ( 0.05),
				   MakeDoubleAccessor (&RcpQueue::fixed_rate_fact_),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("propag_rtt_",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (1.0),
				   MakeDoubleAccessor (&RcpQueue::propag_rtt_),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("upd_timeslot_",
				   "The lower threshold number of bytes accepted by this RcpQueue.",
				   DoubleValue (0.01),
				   MakeDoubleAccessor (&RcpQueue::upd_timeslot_),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("rate_fact_mode_",
				   "The higher threshold number of packets accepted by this RcpQueue.",
				   UintegerValue (0),
				   MakeUintegerAccessor (&RcpQueue::rate_fact_mode_),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("DataRate",
				   "The default data rate for point to point links",
				   DataRateValue (DataRate ("32768b/s")),
				   MakeDataRateAccessor (&RcpQueue::m_bps),
				   MakeDataRateChecker ())
  ;

  return tid;
}

RcpQueue::RcpQueue () :
  DropTailQueue (),
  m_hTh(false),
  m_lTh(false),
  m_init(true)
  /*m_packets (),
  m_bytesInQueue (0)*/
{
  NS_LOG_FUNCTION (this);

 // double T;
  Init();

 /* Tq_ = std::min(m_rtt, upd_timeslot_);  // Tq_ has to be initialized  after binding of upd_timeslot_
  //
  // fprintf(stdout,"LOG-RCPQueue: alpha_ %f beta_ %f\n",alpha_,beta_);

  // Scheduling queue_timer randommly so that routers are not synchronized
  //T = Random::normal(Tq_, 0.2*Tq_);
  T = Tq_;
  //std::default_random_engine generator;
  //std::normal_distribution<double> distribution(Tq_,0.2*Tq_);
  //T = distribution(generator);
  //if (T < 0.004) { T = 0.004; } // Not sure why Dina did this...

  end_slot_ = T;
  NS_LOG_LOGIC("Timeout at " << T << " " << m_rtt << " " << upd_timeslot_);
  queue_timer_ = Simulator::Schedule (Seconds(T), &RcpQueue::Timeout, this);*/

  //queue_timer_.sched(T);
}

RcpQueue::~RcpQueue ()
{
  NS_LOG_FUNCTION (this);
}

void
RcpQueue::Init()
{
  //link_capacity_ = -1;
	input_traffic_ = 0.0;
	act_input_traffic_ = 0.0;
	output_traffic_ = 0.0;
	last_load_ = 0;
	traffic_spill_ = 0;
	avg_rtt_ = 0.2;
	this_Tq_rtt_sum_ = 0;
	this_Tq_rtt_     = 0;
	this_Tq_rtt_numPkts_ = 0;
	input_traffic_rtt_   = 0;
	rtt_moving_gain_ = 0.02;
	//    Tq_ = RTT;
	//    Tq_ = min(RTT, TIMESLOT);
	Q_ = 0;
	Q_last = 0;

	double T;
	//NS_LOG_LOGIC("Timeout at " << m_rtt << " " << upd_timeslot_);
	//Tq_ = std::min(m_rtt, upd_timeslot_);  // Tq_ has to be initialized  after binding of upd_timeslot_
	Tq_ = 0.01;
	T = Tq_;
	end_slot_ = T;
	//NS_LOG_LOGIC("Timeout at " << T << " " << flow_rate_ << " " << m_bps.GetBitRate());
	queue_timer_ = Simulator::Schedule (Seconds(T), &RcpQueue::Timeout, this);

}

Ptr<Packet>
RcpQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this<<m_rtt);

  Ptr<Packet> p = DropTailQueue::DoDequeue();
  if (p != 0)
	  DoBeforePacketDeparture(p);

  return p;

}

bool 
RcpQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << m_packets.size () << p->GetSize());
  DoOnPacketArrival(p);
  return DropTailQueue::DoEnqueue(p);
}

void
RcpQueue::DoBeforePacketDeparture(Ptr<Packet> p)
{
	NS_LOG_FUNCTION (this<<m_rtt);
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

	if (tcpHeader.HasOption (TcpOption::RCP))
	{
		Ptr<TcpOptionRcp> ts = DynamicCast<TcpOptionRcp> (tcpHeader.GetOption (TcpOption::RCP));

		if (tcpHeader.GetFlags () & TcpHeader::SYN)
		{
			FillInFeedback(tcpHeader);

		} else if (ts->GetFlag() & TcpOptionRcp::RCP_REF || ts->GetFlag() == TcpOptionRcp::RCP_DATA )
		{
			FillInFeedback(tcpHeader);
		}
	}
}

void
RcpQueue::FillInFeedback(const TcpHeader &header)
{
  NS_LOG_FUNCTION(this<<flow_rate_);
  if (header.HasOption (TcpOption::RCP))
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
  NS_LOG_FUNCTION (this<<m_rtt);
  // Taking input traffic statistics
  uint32_t size = p->GetSize();
  double pkt_time_ = (double)size/m_bps.GetBitRate();
  double end_time = Simulator::Now().GetSeconds()+pkt_time_;
  double part1, part2;
  NS_LOG_LOGIC("Packet time " << pkt_time_ << " end time " << end_time);

  NS_LOG_LOGIC("Packet time " << pkt_time_ << " end time " << end_time);
  Ptr<Packet> packet = p->Copy();
  PppHeader ppp;
  packet->RemoveHeader (ppp);
  TcpHeader tcpHeader;
  Ipv4Header ipHeader;
  packet->RemoveHeader(ipHeader);
  packet->PeekHeader(tcpHeader);

  double this_rtt=0;
  if (tcpHeader.HasOption (TcpOption::TS))
  {
	Ptr<TcpOptionTS> ts;
	ts = DynamicCast<TcpOptionTS> (tcpHeader.GetOption (TcpOption::TS));
	this_rtt = TcpOptionTS::ElapsedTimeFromTsValue (ts->GetEcho ()).GetSeconds();
  }

  if (this_rtt > 0) {
       this_Tq_rtt_sum_ += (this_rtt * size);
       input_traffic_rtt_ += size;
       NS_LOG_LOGIC("Rtt " << this_Tq_rtt_sum_ << " " << input_traffic_rtt_<< " " << this_rtt << " " << flow_rate_ << " " << m_bps.GetBitRate());
       this_Tq_rtt_ = RunningAvg(this_rtt, this_Tq_rtt_, flow_rate_/m_bps.GetBitRate());
  }

  if (end_time <= end_slot_)
    act_input_traffic_ += size;
  else {
    part2 = size * (end_time - end_slot_)/pkt_time_;
    part1 = size - part2;
    act_input_traffic_ += part1;
    traffic_spill_ += part2;
  }

  NS_LOG_LOGIC("Input traffic " << act_input_traffic_);

}

void RcpQueue::Timeout()
{
  NS_LOG_FUNCTION (this<<m_rtt);
  if(m_init)
  {
	  flow_rate_ = m_bps.GetBitRate() * 0.05;
	  m_init = false;
  }
  NS_LOG_LOGIC ("Flowrate " << flow_rate_ << " " << (flow_rate_/m_bps.GetBitRate()));

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

  Q_ = GetNBytes();
  Q_pkts = GetNPackets();

  input_traffic_ = last_load_;
  if (input_traffic_rtt_ > 0)
    this_Tq_rtt_numPkts_ = this_Tq_rtt_sum_/input_traffic_rtt_;

  /*
  if (this_Tq_rtt_numPkts_ >= avg_rtt_)
      rtt_moving_gain_ = (flow_rate_/m_bps.GetBitRate());
  else
      rtt_moving_gain_ = (flow_rate_/m_bps.GetBitRate())*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);
   */
   if (this_Tq_rtt_numPkts_ >= avg_rtt_)
        rtt_moving_gain_ = (Tq_/avg_rtt_);
   else
        rtt_moving_gain_ = (flow_rate_/m_bps.GetBitRate())*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);

  avg_rtt_ = RunningAvg(this_Tq_rtt_numPkts_, avg_rtt_, rtt_moving_gain_);

//  if (Q_ == 0)
//	  input_traffic_ = PHI*m_bps.GetBitRate();
//  else
//	 input_traffic_ = m_bps.GetBitRate() + (Q_ - Q_last)/Tq_;

//  queueing_delay_ = (Q_ ) / (m_bps.GetBitRate() );
//  if ( avg_rtt_ > queueing_delay_ ){
//    propag_rtt_ = avg_rtt_ - queueing_delay_;
//  } else {
//    propag_rtt_ = avg_rtt_;
//  }


  estN1 = input_traffic_ / flow_rate_;
  estN2 = m_bps.GetBitRate() / flow_rate_;

  if ( rate_fact_mode_ == 0) { // Masayoshi .. for Nandita's RCP

   virtual_link_capacity = m_gamma * m_bps.GetBitRate();

    /* Estimate # of active flows with  estN2 = (m_bps.GetBitRate()/flow_rate_) */
    ratio = (1 + ((Tq_/avg_rtt_)*(m_alpha*(virtual_link_capacity - input_traffic_) - m_beta*(Q_/avg_rtt_)))/virtual_link_capacity);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 1) { // Masayoshi .. for fixed rate fact
    /* Fixed Rate Mode */
    temp = m_bps.GetBitRate() * fixed_rate_fact_;

  } else if ( rate_fact_mode_ == 2) {

    /* Estimate # of active flows with  estN1 = (input_traffic_/flow_rate_) */

    if (input_traffic_ == 0.0 ){
      input_traffic_devider_ = m_bps.GetBitRate()/1000000.0;
    } else {
      input_traffic_devider_ = input_traffic_;
    }
    ratio = (1 + ((Tq_/avg_rtt_)*(m_alpha*(m_bps.GetBitRate() - input_traffic_) - m_beta*(Q_/avg_rtt_)))/input_traffic_devider_);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 3) {
    //if (input_traffic_ == 0.0 ){
    //input_traffic_devider_ = m_bps.GetBitRate()/1000000.0;
    //    } else {
    //input_traffic_devider_ = input_traffic_;
    //}
    ratio =  (1 + ((Tq_/propag_rtt_)*(m_alpha*(m_bps.GetBitRate() - input_traffic_) - m_beta*(Q_/propag_rtt_)))/m_bps.GetBitRate());
    temp = flow_rate_ * ratio;
  } else  if ( rate_fact_mode_ == 4) { // Masayoshi .. Experimental
    ratio = (1 + ((Tq_/avg_rtt_)*(m_alpha*(m_bps.GetBitRate() - input_traffic_) - m_beta*(Q_/avg_rtt_)))/m_bps.GetBitRate());
    temp = flow_rate_ * ratio;
    // m_bps.GetBitRate() : byte/sec
  } else  if ( rate_fact_mode_ == 5) {
    // temp = - m_bps.GetBitRate() * ( Q_/(m_bps.GetBitRate()* avg_rtt_) - 1.0);
    //temp = flow_rate_ +  m_bps.GetBitRate() * (alpha_ * (1.0 - input_traffic_/m_bps.GetBitRate()) - beta_ * ( Q_/(avg_rtt_*m_bps.GetBitRate()) )) * Tq_;
    temp = m_bps.GetBitRate() * exp ( - Q_/(m_bps.GetBitRate()* avg_rtt_));
  } else  if ( rate_fact_mode_ == 6) {
    temp = m_bps.GetBitRate() * exp ( - Q_/(m_bps.GetBitRate()* propag_rtt_));
  } else  if ( rate_fact_mode_ == 7) {
     temp = - m_bps.GetBitRate() * ( Q_/(m_bps.GetBitRate()* propag_rtt_) - 1.0);
  } else  if ( rate_fact_mode_ == 8) {
    temp = flow_rate_ +  m_bps.GetBitRate() * (m_alpha * (1.0 - input_traffic_/m_bps.GetBitRate()) - m_beta/avg_rtt_ * ( Q_/(propag_rtt_*m_bps.GetBitRate()) - 0.8 )) * Tq_;
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
    } else if (temp > m_bps.GetBitRate()){
      flow_rate_ = m_bps.GetBitRate();
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
  }


//  else if (temp < 0 )
//	 flow_rate_ = 1000/avg_rtt_; // 1 pkt per rtt

  datarate_fact = flow_rate_/m_bps.GetBitRate();

//   if (print_status_ == 1)
//   if (routerId_ == 0)
//      	   fprintf(stdout, "%f %d %f %f %f\n", now(), length(), datarate_fact, output_traffic_/(m_bps.GetBitRate()*Tq_), avg_rtt_);
//	fprintf(stdout, "%s %f %d %d %f %f %f\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_);
     //	fprintf(stdout, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(m_bps.GetBitRate() - input_traffic_)/m_bps.GetBitRate(), (Q_/propag_rtt_)/m_bps.GetBitRate(),ratio,estN1,estN2,clip);

 //    if(channel_ != NULL){
 //      char buf[2048];
//       sprintf(buf, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(m_bps.GetBitRate() - input_traffic_)/m_bps.GetBitRate(), (Q_/avg_rtt_)/m_bps.GetBitRate(),ratio,estN1,estN2,clip);
	   // sprintf(buf, "%f %d %.5lf %f %f %f %f %f %c\n", now(), byteLength(), datarate_fact, avg_rtt_, this_Tq_rtt_, this_Tq_rtt_numPkts_, last_load_, (m_bps.GetBitRate() - input_traffic_)/m_bps.GetBitRate(), clip);
//       sprintf(buf, "%f r_ %f estN1_ %f estN2_ %f \n", now(), datarate_fact, estN1, estN2);
//	   (void)Tcl_Write(channel_, buf, strlen(buf));
   //  }

// fflush(stdout);

  NS_LOG_FUNCTION(this<<GetNBytes()<<Q_pkts<<datarate_fact<<last_load_<<avg_rtt_<<(m_bps.GetBitRate() - input_traffic_)/m_bps.GetBitRate()<<(Q_/avg_rtt_)/m_bps.GetBitRate()<<ratio<<estN1<<estN2<<clip);

  Tq_ = std::min(avg_rtt_, upd_timeslot_);
  this_Tq_rtt_ = 0;
  this_Tq_rtt_sum_ = 0;
  input_traffic_rtt_ = 0;
  Q_last = Q_;
  act_input_traffic_ = traffic_spill_;
  traffic_spill_ = 0;
  output_traffic_ = 0.0;
  end_slot_ = Simulator::Now().GetSeconds() + Tq_;
  queue_timer_ = Simulator::Schedule (Seconds(Tq_), &RcpQueue::Timeout, this);

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

