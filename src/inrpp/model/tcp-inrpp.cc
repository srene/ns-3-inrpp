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

#include "../../inrpp/model/tcp-inrpp.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/packet.h"
//#include "ns3/inrpp-backp-tag.h"
#include "tcp-option-inrpp-back.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpInrpp");

NS_OBJECT_ENSURE_REGISTERED (TcpInrpp);

TypeId
TcpInrpp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpInrpp")
    .SetParent<TcpSocketBase> ()
    .SetGroupName ("Inrpp")
    .AddConstructor<TcpInrpp> ()
    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpInrpp::m_retxThresh),
                    MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LimitedTransmit", "Enable limited transmit",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcpInrpp::m_limitedTx),
                   MakeBooleanChecker ())
	.AddAttribute ("Rate", "Tcp rate",
					UintegerValue (1400000),
					MakeUintegerAccessor (&TcpInrpp::m_initialRate),
					MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpInrpp::m_cWnd),
                     "ns3::TracedValue::Uint32Callback")
    /*.AddTraceSource ("SlowStartThreshold",
                     "TCP slow start threshold (bytes)",
                     MakeTraceSourceAccessor (&TcpInrpp::m_ssThresh),
                     "ns3::TracedValue::Uint32Callback")*/
 ;
  return tid;
}

TcpInrpp::TcpInrpp (void)
  : m_retxThresh (3), // mute valgrind, actual value set by the attribute system
    m_inFastRec (false),
    m_limitedTx (false), // mute valgrind, actual value set by the attribute system
	m_nonce(0),
	m_flag(2),
	m_rate(0),
	m_back(false),
	m_lastSeq(0)
{
  NS_LOG_FUNCTION (this<<m_initialRate);
  m_tcpRate = m_initialRate;
}

TcpInrpp::TcpInrpp (const TcpInrpp& sock)
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
	m_back(sock.m_back)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpInrpp::~TcpInrpp (void)
{
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpInrpp::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/* We initialize m_cWnd from this function, after attributes initialized */
int
TcpInrpp::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  m_tcpRate = m_initialRate;
  return TcpSocketBase::Connect (address);
}

/* Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpInrpp::Window (void)
{
  NS_LOG_FUNCTION (this);
  //return std::min (m_rWnd.Get (), m_cWnd.Get ());
  return std::min (m_rWnd.Get (), m_segmentSize);
}

Ptr<TcpSocketBase>
TcpInrpp::Fork (void)
{
  return CopyObject<TcpInrpp> (this);
}

/* New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpInrpp::NewAck (const SequenceNumber32& seq)
{
	if(m_back)
	{
		m_cWnd += (seq.GetValue()-m_lastSeq);
	} else
	{
		m_cWnd = 0;
	}
	m_lastSeq = seq.GetValue();


  NS_LOG_FUNCTION (this << seq);
 // NS_LOG_LOGIC ("TcpInrpp received ACK for seq " << seq <<
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
  if (!m_sendPendingDataEvent.IsRunning ())
    {
      m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpInrpp::SendPendingData, this, m_connected);
    }
  TcpSocketBase::NewAck (seq);
}

/* Cut cwnd and enter fast recovery mode upon triple dupack */
void
TcpInrpp::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << count);

}

/* Retransmit timeout */
void
TcpInrpp::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  m_inFastRec = false;

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer->HeadSequence () >= m_highTxMark) return;

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start
  //m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
  //m_cWnd = m_segmentSize;
  m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
 // NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
  //             ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
  DoRetransmit ();                          // Retransmit the packet
}

void
TcpInrpp::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpInrpp::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void
TcpInrpp::SetInitialSSThresh (uint32_t threshold)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpInrpp::SetSSThresh() cannot change initial ssThresh after connection started.");
  m_initialSsThresh = threshold;
}

uint32_t
TcpInrpp::GetInitialSSThresh (void) const
{
  return m_initialSsThresh;
}

void
TcpInrpp::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpInrpp::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
TcpInrpp::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

void 
TcpInrpp::InitializeCwnd (void)
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
TcpInrpp::ScaleSsThresh (uint8_t scaleFactor)
{
  //m_ssThresh <<= scaleFactor;
}

void
TcpInrpp::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
	NS_LOG_FUNCTION(this);
	if (tcpHeader.HasOption (TcpOption::INRPP_BACK))
	{

	  Ptr<TcpOptionInrppBack> inrpp = DynamicCast<TcpOptionInrppBack> (tcpHeader.GetOption (TcpOption::INRPP_BACK));
	  m_flag = inrpp->GetFlag();
	  m_nonce =  inrpp->GetNonce ();

	  NS_LOG_LOGIC("Flag " << (uint32_t)m_flag << " received");

	  if(m_flag==1){
		  m_back = true;
		  NS_LOG_LOGIC("Start backpressure");
		  //m_tcpRate = std::min(m_pacingRate,m_initialRate);
		  //m_tcpRate = 400000;
		  /*if(!m_updateEvent.IsRunning()){
			  m_tcpRate = m_tcpRate*m_rate/100;
			  NS_LOG_LOGIC("Rate " << m_rate << " " << m_tcpRate);
			  m_updateEvent = Simulator::Schedule(m_lastRtt,&TcpInrpp::UpdateRate,this);
		  }*/
	  }else if(m_flag==0||m_flag==2){
		//  m_updateEvent.Cancel();
		  m_back = false;
		  m_tcpRate = m_initialRate;
	  }
//	  m_timestampToEcho = ts->GetTimestamp ();

	  NS_LOG_INFO (m_node->GetId () << " Got InrppBack flag=" <<
				   (uint32_t) inrpp->GetFlag()<< " and nonce="     << inrpp->GetNonce () << " " << m_tcpRate);
	}
//	NS_LOG_LOGIC("Update rate " << m_lastRtt);

	TcpSocketBase::ReceivedAck (packet,tcpHeader);
}

bool
TcpInrpp::SendPendingData (bool withAck)
{
	  NS_LOG_FUNCTION (this << withAck << m_txBuffer->SizeFromSequence (m_nextTxSequence) << m_tcpRate << m_initialRate);
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

	  if(m_back){
		  uint32_t nPacketsSent = 0;
		   while (m_txBuffer->SizeFromSequence (m_nextTxSequence))
		     {
		       uint32_t w = m_cWnd; // Get available window size
		       // Stop sending if we need to wait for a larger Tx window (prevent silly window syndrome)
		       NS_LOG_LOGIC("Window " <<  w << " SizefromSeq " << m_txBuffer->SizeFromSequence (m_nextTxSequence));
		       if (w < m_segmentSize && m_txBuffer->SizeFromSequence (m_nextTxSequence) > w)
		         {
		           NS_LOG_LOGIC ("Preventing Silly Window Syndrome. Wait to send.");
		           break; // No more
		         }
		       // Nagle's algorithm (RFC896): Hold off sending if there is unacked data
		       // in the buffer and the amount of data to send is less than one segment
		       if (!m_noDelay && UnAckDataCount () > 0
		           && m_txBuffer->SizeFromSequence (m_nextTxSequence) < m_segmentSize)
		         {
		           NS_LOG_LOGIC ("Invoking Nagle's algorithm. Wait to send.");
		           break;
		         }
		       NS_LOG_LOGIC ("TcpSocketBase " << this << " SendPendingData" <<
		                     " w " << w <<
		                     " rxwin " << m_rWnd <<
		                     " segsize " << m_segmentSize <<
		                     " nextTxSeq " << m_nextTxSequence <<
		                     " highestRxAck " << m_txBuffer->HeadSequence () <<
		                     " pd->Size " << m_txBuffer->Size () <<
		                     " pd->SFS " << m_txBuffer->SizeFromSequence (m_nextTxSequence));
		       uint32_t s = std::min (w, m_segmentSize);  // Send no more than window
		       uint32_t sz = SendDataPacket (m_nextTxSequence, s, withAck);
		       nPacketsSent++;                             // Count sent this loop
		       m_nextTxSequence += sz;                     // Advance next tx sequence
		       m_cWnd-=sz;
		     }
		   NS_LOG_LOGIC ("SendPendingData sent " << nPacketsSent << " packets");
		   return (nPacketsSent > 0);
	  } else
	  {
		  NS_LOG_LOGIC("Full rate at "<< m_tcpRate);

		  if(m_txBuffer->SizeFromSequence (m_nextTxSequence) > 0)
		  {
			  NS_LOG_LOGIC("NextSeq " << m_nextTxSequence);
			  uint32_t sz = SendDataPacket (m_nextTxSequence, m_segmentSize, withAck);
			  nPacketsSent++;                             // Count sent this loop
			  m_nextTxSequence += sz;                     // Advance next tx sequence
			  NS_LOG_LOGIC("Sent " << sz << " bytes");
		  } else if(m_closeOnEmpty)
		  {
			  return false;
		  }

		  // Try to send more data
		  if (!m_sendPendingDataEvent.IsRunning ())
		    {
			 Time t = Seconds(((double)(m_segmentSize+60)*8)/m_tcpRate);
			 NS_LOG_LOGIC("Schedule next packet at " << Simulator::Now().GetSeconds()+t.GetSeconds());
		      m_sendPendingDataEvent = Simulator::Schedule (t, &TcpInrpp::SendPendingData, this, m_connected);
		    }

		  NS_LOG_LOGIC ("SendPendingData sent " << nPacketsSent << " packets " << m_tcpRate << " " << " rate");
		  return (nPacketsSent > 0);
	  }


}

void
TcpInrpp::AddOptions (TcpHeader& tcpHeader)
{
	 NS_LOG_FUNCTION (this << tcpHeader);

	 if((m_flag==0||m_flag==1||m_flag==3)&&(m_nonce!=0))
	 {
		 Ptr<TcpOptionInrppBack> option = CreateObject<TcpOptionInrppBack> ();
		 option->SetFlag(3);
		 option->SetNonce (m_nonce);
	     tcpHeader.AppendOption (option);
	 }

	 TcpSocketBase::AddOptions (tcpHeader);
}

void
TcpInrpp::SetRate(uint32_t rate)
{
	m_initialRate =  rate;
	m_tcpRate = m_initialRate;
}

void
TcpInrpp::UpdateRate()
{
	NS_LOG_FUNCTION(this<<m_lastRtt);
	  m_tcpRate = m_tcpRate*m_rate/100;

	 m_updateEvent = Simulator::Schedule(m_lastRtt,&TcpInrpp::UpdateRate,this);
}

} // namespace ns3
