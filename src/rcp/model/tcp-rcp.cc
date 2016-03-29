/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Sergi Rene
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
 *
 * Author: Sergi Rene <s.rene@ucl.ac.uk>
 */

#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

#include "tcp-rcp.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/tcp-option-inrpp.h"
#include "ns3/tcp-option-inrpp-back.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-end-point.h"
#include "tcp-option-rcp.h"
#include "ns3/tcp-option-ts.h"

#define RCP_HDR_BYTES 62

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpRcp");

NS_OBJECT_ENSURE_REGISTERED (TcpRcp);

TypeId
TcpRcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpRcp")
    .SetParent<TcpSocketBase> ()
    .SetGroupName ("Rcp")
    .AddConstructor<TcpRcp> ()
    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpRcp::m_retxThresh),
                    MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LimitedTransmit", "Enable limited transmit",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcpRcp::m_limitedTx),
                   MakeBooleanChecker ())
//	.AddAttribute ("Rate", "Tcp rate",
//					UintegerValue (1400000),
//					MakeUintegerAccessor (&TcpRcp::m_initialRate),
//					MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpRcp::m_cWnd),
                     "ns3::TracedValue::Uint32Callback")
	.AddTraceSource("Throughput", "The estimated bandwidth",
					 MakeTraceSourceAccessor(&TcpRcp::m_currentBW),
					 "ns3::TracedValue::DoubleCallback")
	.AddAttribute ("Refresh",
					   "Moving average refresh value.",
					   DoubleValue (0.1),
					   MakeDoubleAccessor (&TcpRcp::m_refresh),
					   MakeDoubleChecker<double> ());
    /*.AddTraceSource ("SlowStartThreshold",
                     "TCP slow start threshold (bytes)",
                     MakeTraceSourceAccessor (&TcpRcp::m_ssThresh),
                     "ns3::TracedValue::Uint32Callback")*/
 ;
  return tid;
}

TcpRcp::TcpRcp (void)
  : m_retxThresh (3), // mute valgrind, actual value set by the attribute system
    m_inFastRec (false),
    m_limitedTx (false), // mute valgrind, actual value set by the attribute system
	m_nonce(0),
	m_flag(2),
	m_rate(0),
	m_back(false),
	m_lastSeq(0),
	m_currentBW(0),
    m_lastSampleBW(0),
	m_lastBW(0),
	data(0),
	m_slot(0),
	m_tcpRate(1000000)
	//m_ackInterval(0)
{
  NS_LOG_FUNCTION (this);
  //m_tcpRate = m_initialRate;
  t1 = Simulator::Now();
  NS_LOG_LOGIC("Endpoint " << m_endPoint);

}

TcpRcp::TcpRcp (const TcpRcp& sock)
  : TcpSocketBase (sock),
   // m_cWnd (sock.m_cWnd),
   // m_ssThresh (sock.m_ssThresh),
    m_initialCWnd (sock.m_initialCWnd),
    m_initialSsThresh (sock.m_initialSsThresh),
    m_retxThresh (sock.m_retxThresh),
    m_inFastRec (false),
    m_limitedTx (sock.m_limitedTx),
	m_nonce(sock.m_nonce),
//	m_rate(sock.m_rate),
	m_flag(sock.m_flag),
	m_back(sock.m_back),
	m_slot(sock.m_slot)

{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpRcp::~TcpRcp (void)
{
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpRcp::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpRcp::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  //m_tcpRate = m_endPoint->GetBoundNetDevice()->GetObject<PointToPointNetDevice>()->GetDataRate().GetBitRate();
 // t = Simulator::Now();
  return TcpSocketBase::Connect (address);
}

/* Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpRcp::Window (void)
{
  NS_LOG_FUNCTION (this);
  //return std::min (m_rWnd.Get (), m_cWnd.Get ());
  return std::min (m_rWnd.Get (), m_segmentSize);
}

Ptr<TcpSocketBase>
TcpRcp::Fork (void)
{
  return CopyObject<TcpRcp> (this);
}

/* New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpRcp::NewAck (const SequenceNumber32& seq)
{

	m_lastSeq = seq.GetValue();


  NS_LOG_FUNCTION (this << seq);
 // NS_LOG_LOGIC ("TcpRcp received ACK for seq " << seq <<
  //    /          " cwnd " << m_cWnd <<
    //            " ssthresh " << m_ssThresh);

  // Check for exit condition of fast recovery
  if (m_inFastRec && seq < m_recover)
    { // Partial ACK, partial window deflation (RFC2582 sec.3 bullet #5 paragraph 3)
    //  m_cWnd += m_segmentSize - (seq - m_txBuffer->HeadSequence ());
    //  NS_LOG_INFO ("Partial ACK for seq " << seq << " in fast recovery: cwnd set to " << m_cWnd);
      m_txBuffer->DiscardUpTo(seq);  //Bug 1850:  retransmit before newack
      DoRetransmit (); // Assume the next seq is lost. Retransmit lost packet
      TcpSocketBase::NewAck (seq); // update m_nextTxSequence and send new data if allowed by window
      return;
    }
  else if (m_inFastRec && seq >= m_recover)
    { // Full ACK (RFC2582 sec.3 bullet #5 paragraph 2, option 1)
    //  m_cWnd = std::min (m_ssThresh.Get (), BytesInFlight () + m_segmentSize);
      m_inFastRec = false;
  //    NS_LOG_INFO ("Received full ACK for seq " << seq <<". Leaving fast recovery with cwnd set to " << m_cWnd);
    }

  // Increase of cwnd based on current phase (slow start or congestion avoidance)
//  if (m_cWnd < m_ssThresh)
 //   { // Slow start mode, add one segSize to cWnd. Default m_ssThresh is 65535. (RFC2001, sec.1)
   //   m_cWnd += m_segmentSize;
 //     NS_LOG_INFO ("In SlowStart, ACK of seq " << seq << "; update cwnd to " << m_cWnd << "; ssthresh " << m_ssThresh);
//    }
 // else
 //   { // Congestion avoidance mode, increase by (segSize*segSize)/cwnd. (RFC2581, sec.3.1)
      // To increase cwnd for one segSize per RTT, it should be (ackBytes*segSize)/cwnd
 //     double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_cWnd.Get ();
  //    adder = std::max (1.0, adder);
    //  m_cWnd += static_cast<uint32_t> (adder);
 //     NS_LOG_INFO ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
 //   }

  // Complete newAck processing
  // Try to send more data
 // if (!m_sendPendingDataEvent.IsRunning ())
 //   {
 //     m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
 //   }
  TcpSocketBase::NewAck (seq);
}

/* Cut cwnd and enter fast recovery mode upon triple dupack */
void
TcpRcp::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << count);
/*	m_cWnd += m_segmentSize;
	  if (!m_sendPendingDataEvent.IsRunning ())
	    {
	      m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
	    }
*/
}

/* Retransmit timeout */
void
TcpRcp::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
 // m_inFastRec = false;

  // If erroneous timeout in closed/timed-wait state, just return
 // if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
 // if (m_state <= ESTABLISHED && m_txBuffer->HeadSequence () >= m_highTxMark) return;

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start
  //m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
  //m_cWnd = m_segmentSize;
 // m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
 // NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
  //             ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
 // DoRetransmit ();                          // Retransmit the packet
}

void
TcpRcp::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpRcp::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void
TcpRcp::SetInitialSSThresh (uint32_t threshold)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpRcp::SetSSThresh() cannot change initial ssThresh after connection started.");
  m_initialSsThresh = threshold;
}

uint32_t
TcpRcp::GetInitialSSThresh (void) const
{
  return m_initialSsThresh;
}

void
TcpRcp::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpRcp::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
TcpRcp::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

void 
TcpRcp::InitializeCwnd (void)
{
  /*
   * Initialize congestion window, default to 1 MSS (RFC2001, sec.1) and must
   * not be larger than 2 MSS (RFC2581, sec.3.1). Both m_initiaCWnd and
   * m_segmentSize are set by the attribute system in ns3::TcpSocket.
   */
 // m_cWnd = m_initialCWnd * m_segmentSize;
  //m_cWnd = 100000;
  //m_ssThresh = m_initialSsThresh;
}

void
TcpRcp::ScaleSsThresh (uint8_t scaleFactor)
{
  //m_ssThresh <<= scaleFactor;
}

void
TcpRcp::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
	NS_LOG_FUNCTION(this);

	  NS_LOG_LOGIC("INRPP_BACK Flag " << (uint32_t)m_flag << " received");

	  if(m_flag==1){
		  m_back = true;
		  NS_LOG_LOGIC("Start backpressure");

	  }else if(m_flag==0||m_flag==2){
		  m_cWnd = 0;
		  NS_LOG_LOGIC("Full rate again");
		  m_back = false;
		 // m_tcpRate = m_initialRate;
	  }

	TcpSocketBase::ReceivedAck (packet,tcpHeader);
}


bool
TcpRcp::SendPendingData (bool withAck)
{
	  NS_LOG_FUNCTION (this << m_back << withAck << m_txBuffer->SizeFromSequence (m_nextTxSequence) << m_tcpRate);
	  if (m_txBuffer->Size () == 0)
		{
		  return false;                           // Nothing to send
		}
	  if (m_endPoint == 0 && m_endPoint6 == 0)
		{
		  NS_LOG_INFO ("TcpSocketBase::SendPendingData: No endpoint; m_shutdownSend=" << m_shutdownSend);
		  return false; // Is this the right way to handle this condition?
		}
	  uint32_t nPacketsSent = 0;


		  if(m_txBuffer->SizeFromSequence (m_nextTxSequence) > 0)
		  {
			  NS_LOG_LOGIC("NextSeq " << m_nextTxSequence);
			  uint32_t sz = SendDataPacket (m_nextTxSequence, m_segmentSize, withAck);
			  nPacketsSent++;                             // Count sent this loop
			  m_nextTxSequence += sz;                     // Advance next tx sequence

		 	  data+= sz * 8;
		 	  if(Simulator::Now().GetSeconds()-t1.GetSeconds()>0.1){
		 		  m_currentBW = data / (Simulator::Now().GetSeconds()-t1.GetSeconds());
		 		  data = 0;
		 		  double alpha = 0.4;
		 		  double   sample_bwe = m_currentBW;
		 		  m_currentBW = (alpha * m_lastBW) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW) / 2));
		 		  m_lastSampleBW = sample_bwe;
		 		  m_lastBW = m_currentBW;
		 		  t1 = Simulator::Now();

		 	  }

			  NS_LOG_LOGIC("Sent " << sz << " bytes");
		  } else if(m_closeOnEmpty)
		  {
			  return false;
		  }

		  NS_LOG_LOGIC ("SendPendingData sent " << nPacketsSent << " packets " << m_tcpRate << " " << " rate");
		  return (nPacketsSent > 0);

}

void
TcpRcp::AddOptions (TcpHeader& tcpHeader)
{
	 NS_LOG_FUNCTION (this << tcpHeader << m_nonce);

	 Ptr<TcpOptionRcp> option = CreateObject<TcpOptionRcp> ();
	 option->SetFlag(m_slot);
	 //option->SetNonce (m_nonce);
	 tcpHeader.AppendOption (option);

	 TcpSocketBase::AddOptions (tcpHeader);
}

void
TcpRcp::SetRate(uint32_t rate)
{
	//m_initialRate =  rate;
	m_tcpRate = rate;
}

double
TcpRcp::RCP_desired_rate()
{
	return -1;
}


void
TcpRcp::Timeout()
{
	if (RCP_state == RCP_RUNNING || RCP_state == RCP_RUNNING_WREF) {
		if (num_sent_ < numpkts_ - 1) {
		    m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
		    m_rcpTimeout = Simulator::Schedule (Seconds(interval_), &TcpRcp::Timeout, this);

		} else {

			//sendlast();
		    m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
			RCP_state = RCP_FINSENT;
			NS_LOG_LOGIC("RCP_FINSENT");

			if(m_rcpTimeout.IsRunning())
				m_rcpTimeout.Cancel();
			if(m_refTransmit.IsRunning())
				m_refTransmit.Cancel();
			if(m_rxTimeout.IsRunning())
				m_rxTimeout.Cancel();
		}

	} else if (RCP_state == RCP_RETRANSMIT) {

          if (num_pkts_resent_ < num_Pkts_to_retransmit_ - 1) {
  		      m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
  		      if(m_rcpTimeout.IsRunning())
  		    	  m_rcpTimeout.Cancel();
  		      m_rcpTimeout = Simulator::Schedule (Seconds(interval_), &TcpRcp::Timeout, this);
          } else if (num_pkts_resent_ == num_Pkts_to_retransmit_ - 1) {
  		      m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpRcp::SendPendingData, this, m_connected);
  			if(m_rcpTimeout.IsRunning())
  				m_rcpTimeout.Cancel();
  			if(m_refTransmit.IsRunning())
  				m_refTransmit.Cancel();
  			if(m_rxTimeout.IsRunning())
  				m_rxTimeout.Cancel();
  			m_rxTimeout =  Simulator::Schedule (Seconds(2*rtt_), &TcpRcp::RtxTimeout, this);
          }
    }
}

void
TcpRcp::RtxTimeout()
{
    RCP_state = RCP_RETRANSMIT;
    num_enter_retransmit_mode_++;

    num_pkts_resent_ = 0;
    num_Pkts_to_retransmit_ = numpkts_ - num_dataPkts_acked_by_receiver_;

    NS_LOG_LOGIC("Entered retransmission mode " << num_enter_retransmit_mode_ << " num_Pkts_to_retransmit_ " << num_Pkts_to_retransmit_);
   // fprintf(stdout, "%lf %s Entered retransmission mode %d, num_Pkts_to_retransmit_ %d \n", Scheduler::instance().clock(), this->name(), num_enter_retransmit_mode_, num_Pkts_to_retransmit_);

    if (num_Pkts_to_retransmit_ > 0)
    {
    	if(m_rcpTimeout.IsRunning())
    		m_rcpTimeout.Cancel();
    	m_rcpTimeout = Simulator::Schedule (Seconds(interval_), &TcpRcp::Timeout, this);
    }
}

/* Nandita Rui
 * This function has been changed.
 */
void
TcpRcp::ReadOptions (const TcpHeader& header)
{

	if (header.HasOption (TcpOption::RCP))
	{
		Ptr<TcpOptionRcp> rcp = DynamicCast<TcpOptionRcp> (header.GetOption (TcpOption::RCP));

		if((rcp->GetFlag()==TcpOptionRcp::RCP_SYN)||(RCP_state != RCP_INACT)) {

			switch (rcp->GetFlag()) {

			case TcpOptionRcp::RCP_SYNACK:
				if (header.HasOption (TcpOption::TS))
				  {
					Ptr<TcpOptionTS> ts = DynamicCast<TcpOptionTS> (header.GetOption (TcpOption::TS));
					rtt_ = TcpOptionTS::ElapsedTimeFromTsValue (ts->GetEcho ()).GetSeconds();
				  }

				if (min_rtt_ > rtt_)
					min_rtt_ = rtt_;

				if (rcp->GetRate() > 0) {
					interval_ = (m_segmentSize+RCP_HDR_BYTES)/(rcp->GetRate());

		//#ifdef MASAYOSHI_DEBUG
		//			fprintf(stdout,"%lf recv_synack..rate_change_1st %s %lf %lf\n",now,this->name(),interval_,((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));
		//#endif

					//if (RCP_state == RCP_SYNSENT)
						//start();

				} else {
					if (rcp->GetRate() < 0)
						NS_LOG_LOGIC("Error: RCP rate < 0: " << rcp->GetRate());

					//if (Random::uniform(0,1)<QUIT_PROB) { //sender decides to stop
						//				RCP_state = RCP_DONE;
						// RCP_state = RCP_QUITTING; // Masayoshi
					//	pause();
					//	NS_LOG_LOGIC("LOG: quit by QUIT_PROB");
					//}
					//else {
						RCP_state = RCP_CONGEST;
						//can do exponential backoff or probalistic stopping here.
					//}

				}
				break;

	case TcpOptionRcp::RCP_REFACK:
		//		if ( rh->seqno() < ref_seqno_ && RCP_state != RCP_INACT) /* Added  by Masayoshi */
		// if ( rh->seqno() < seqno_ && RCP_state != RCP_INACT) /* Added  by Masayoshi */
		if ( (header.GetSequenceNumber() <= m_nextTxSequence)  && RCP_state != RCP_INACT){ /* Added  by Masayoshi */
			numOutRefs_--;
			if (numOutRefs_ < 0) {
				fprintf(stderr, "Extra REF_ACK received! \n");
				{
					if(RCP_state == RCP_INACT)
						NS_LOG_LOGIC("RCP_INACT");
					if(RCP_state == RCP_SYNSENT)
						NS_LOG_LOGIC("RCP_SYNSENT");
					if(RCP_state == RCP_RUNNING)
						NS_LOG_LOGIC("RCP_RUNNING");
					if(RCP_state == RCP_RUNNING_WREF)
						NS_LOG_LOGIC("RCP_RUNNING_WREF");
					if(RCP_state == RCP_CONGEST)
						NS_LOG_LOGIC("RCP_CONGEST");
				}
				exit(1);
			}

			if (header.HasOption (TcpOption::TS))
			{
				Ptr<TcpOptionTS> ts = DynamicCast<TcpOptionTS> (header.GetOption (TcpOption::TS));
				rtt_ = TcpOptionTS::ElapsedTimeFromTsValue (ts->GetEcho ()).GetSeconds();
			}

			if (min_rtt_ > rtt_)
				min_rtt_ = rtt_;

			if (rcp->GetRate()> 0) {
				double new_interval = (m_segmentSize+RCP_HDR_BYTES)/(rcp->GetRate());
				if( new_interval != interval_ ){
					interval_ = new_interval;
					if (RCP_state == RCP_CONGEST){
						//start();
					}else
						RateChange();
				}

			}
			else {
				if (rcp->GetRate() < 0)
					NS_LOG_LOGIC("Error: RCP rate < 0: "<<rcp->GetRate());
				//fprintf(stderr, "Error: RCP rate < 0: %f\n",rcp->GetRate());
				//rcp_timer_.force_cancel();
				m_rcpTimeout.Cancel();

				RCP_state = RCP_CONGEST; //can do exponential backoff or probalistic stopping here.
			}
		}
		break;

	case TcpOptionRcp::RCP_ACK:

         num_dataPkts_acked_by_receiver_ = rcp->GetReceivedPackets();
        if (num_dataPkts_acked_by_receiver_ == numpkts_) {
            // fprintf(stdout, "%lf %d RCP_ACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
            Stop();
        }

		if (header.HasOption (TcpOption::TS))
		{
			Ptr<TcpOptionTS> ts = DynamicCast<TcpOptionTS> (header.GetOption (TcpOption::TS));
			rtt_ = TcpOptionTS::ElapsedTimeFromTsValue (ts->GetEcho ()).GetSeconds();
		}

		if (min_rtt_ > rtt_)
			min_rtt_ = rtt_;

		if (rcp->GetRate() > 0) {
			double new_interval = (m_segmentSize+62+RCP_HDR_BYTES)/(rcp->GetRate());
			if( new_interval != interval_ ){
				interval_ = new_interval;
				if (RCP_state == RCP_CONGEST){
					//start();
				}else
					RateChange();
			}
		}
		else {
			NS_LOG_LOGIC("Error: RCP rate < 0: "<< rcp->GetRate());
			//fprintf(stderr, "Error: RCP rate < 0: %f\n",rcp->GetRate());
			RCP_state = RCP_CONGEST; //can do exponential backoff or probalistic stopping here.
		}
		break;

	case TcpOptionRcp::RCP_FIN:
		{double copy_rate;
        num_dataPkts_received_++; // because RCP_FIN is piggybacked on the last packet of flow

		copy_rate = rcp->GetRate();
		// Can modify the rate here.
		/*send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_FINACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();

		target_->recv(send_pkt, (Handler*)0);
		Tcl::instance().evalf("%s fin-received", this->name()); */
		/* Added by Masayoshi */
		break;
		}


	case TcpOptionRcp::RCP_SYN:
		{double copy_rate;

		copy_rate = rcp->GetRate();
		// Can modify the rate here.
		/*send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_SYNACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();

		target_->recv(send_pkt, (Handler*)0);
        RCP_state = RCP_RUNNING; // Only the receiver changes state here
		*/
		break;}

	case TcpOptionRcp::RCP_FINACK:
        num_dataPkts_acked_by_receiver_ = rcp->GetReceivedPackets();

	    if (num_dataPkts_acked_by_receiver_ == numpkts_){
           // fprintf(stdout, "%lf %d RCP_FINACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
            Stop();
        }
		break;

	case TcpOptionRcp::RCP_REF:
		{
		double copy_rate;

		copy_rate = rcp->GetRate();
		// Can modify the rate here.
		/*send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_REFACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();

		target_->recv(send_pkt, (Handler*)0);
		*/
		break;}

	case TcpOptionRcp::RCP_DATA:
		{
		double copy_rate;
        num_dataPkts_received_++;


		copy_rate = rcp->GetRate();
		// Can modify the rate here.
		/*send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_ACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();

		target_->recv(send_pkt, (Handler*)0);
		*/
		break;}

	case TcpOptionRcp::RCP_OTHER:
		NS_LOG_LOGIC("received RCP_OTHER");
		exit(1);
		break;

	default:
		NS_LOG_LOGIC("Unknown RCP packet type!");
		exit(1);
		break;
    }
		}

	}
	TcpSocketBase::ReadOptions (header);

}

void
TcpRcp::RateChange()
{
	if (RCP_state == RCP_RUNNING || RCP_state == RCP_RUNNING_WREF) {
		//rcp_timer_.force_cancel();
		m_rcpTimeout.Cancel();
		double t = lastpkttime_ + interval_;

		Time now = Simulator::Now();

		if ( t > now) {
			NS_LOG_LOGIC("rate_change " << interval_ << " " << ((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));
			rcp_timer_.resched(t - now);

			if( (t - lastpkttime_) > REF_INTVAL * min_rtt_ && RCP_state != RCP_RUNNING_WREF ){
			// the inter-packet time > min_rtt and not in REF mode. Enter REF MODE.
				RCP_state = RCP_RUNNING_WREF;
				NS_LOG_LOGIC("RCP_RUNNING -> RCP_RUNNING_WREF at start");
				if( lastpkttime_ + REF_INTVAL * min_rtt_ > now ){
					ref_timer_.resched(lastpkttime_ + REF_INTVAL * min_rtt_ - now);
				} else {
					ref_timeout();  // send ref packet now.
				}
			}else if ((t-lastpkttime_)<= REF_INTVAL * min_rtt_ &&
				  RCP_state == RCP_RUNNING_WREF ){
			// the inter-packet time <= min_rtt and in REF mode.  Exit REF MODE
				RCP_state = RCP_RUNNING;
				NS_LOG_LOGIC("RCP_RUNNING_WREF -> RCP_RUNNING at start");
				m_refTransmit.Cancel();
			}

		} else {
			fprintf(stdout,"%lf rate_change_sync %s %lf %lf\n",now,this->name(),interval_,((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));

			// sendpkt();
			// rcp_timer_.resched(interval_);
            RateChange(); // send a packet immediately and reschedule timer


			if( interval_ > REF_INTVAL * min_rtt_ && RCP_state != RCP_RUNNING_WREF ){
			// the next packet sendingtime > min_rtt and not in REF mode. Enter REF MODE.
				RCP_state = RCP_RUNNING_WREF;
				NS_LOG_LOGIC("RCP_RUNNINGF -> RCP_RUNNING_WREF at start");
				ref_timer_.resched(REF_INTVAL * min_rtt_);
			}else if ( interval_ <= REF_INTVAL * min_rtt_ && RCP_state == RCP_RUNNING_WREF ){
			// the next packet sending time <= min_rtt and in REF mode.  Exit REF MODE
				RCP_state = RCP_RUNNING;
				NS_LOG_LOGIC("RCP_RUNNINGF_WREF -> RCP_RUNNING at start");
				ref_timer_.force_cancel();
			}
		}
	}

}

void
TcpRcp::Stop()
{

}


} // namespace ns3
