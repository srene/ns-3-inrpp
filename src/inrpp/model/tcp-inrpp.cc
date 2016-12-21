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

#include "tcp-inrpp.h"
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
#include "inrpp-tag2.h"
#include "ns3/callback.h"
#include "ns3/traced-value.h"
#include "ns3/tcp-socket.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-interface.h"
#include "ns3/event-id.h"


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
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpInrpp::m_cWnd),
                     "ns3::TracedValue::Uint32Callback")
	.AddTraceSource("Throughput", "The estimated bandwidth",
					 MakeTraceSourceAccessor(&TcpInrpp::m_currentBW),
					 "ns3::TracedValue::DoubleCallback")
	.AddAttribute ("NumSlot",
					   "The size of the queue for packets pending an arp reply.",
					   UintegerValue (100),
					   MakeUintegerAccessor (&TcpInrpp::m_numSlot),
					   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("Refresh",
					   "Moving average refresh value.",
					   DoubleValue (0.1),
					   MakeDoubleAccessor (&TcpInrpp::m_refresh),
					   MakeDoubleChecker<double> ());
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
	m_lastSeq(0),
	m_currentBW(0),
    m_lastSampleBW(0),
	m_lastBW(0),
	data(0),
	//m_slot(0),
	m_tcpRate(1000000)
	//m_ackInterval(0)
{
  NS_LOG_FUNCTION (this);
  //m_tcpRate = m_initialRate;
  t1 = Simulator::Now();
  NS_LOG_LOGIC("Endpoint " << m_endPoint);

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
	m_back(sock.m_back),
	m_slot(sock.m_slot)

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
  return TcpSocketBase::Connect (address);
}

/* Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpInrpp::Window (void)
{
  NS_LOG_FUNCTION (this);
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
		//m_cWnd+= (seq.GetValue()-m_lastSeq);
		m_cWnd += m_segmentSize;
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
    {
      m_txBuffer->DiscardUpTo(seq);  //Bug 1850:  retransmit before newack
      DoRetransmit (); // Assume the next seq is lost. Retransmit lost packet
      TcpSocketBase::NewAck (seq); // update m_nextTxSequence and send new data if allowed by window
      return;
    }
  else if (m_inFastRec && seq >= m_recover)
    { // Full ACK (RFC2582 sec.3 bullet #5 paragraph 2, option 1)
      m_inFastRec = false;
  //    NS_LOG_INFO ("Received full ACK for seq " << seq <<". Leaving fast recovery with cwnd set to " << m_cWnd);
    }

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
	m_cWnd += m_segmentSize;
	  if (!m_sendPendingDataEvent.IsRunning ())
	    {
	      m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpInrpp::SendPendingData, this, m_connected);
	    }

}

/* Retransmit timeout */
void
TcpInrpp::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
                    // Retransmit the packet
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
TcpInrpp::ReadOptions (const TcpHeader& header)
{
	if (header.HasOption (TcpOption::INRPP_BACK))
	{

	  Ptr<TcpOptionInrppBack> inrpp = DynamicCast<TcpOptionInrppBack> (header.GetOption (TcpOption::INRPP_BACK));
	  m_flag = inrpp->GetFlag();
	  m_nonce =  inrpp->GetNonce ();
	  NS_LOG_INFO (m_node->GetId () << " Got InrppBack flag=" <<
				   (uint32_t) inrpp->GetFlag()<< " and nonce="     << inrpp->GetNonce () << " " << m_tcpRate);
	}

	if(m_flag==2)
	{
		NS_LOG_LOGIC("Clear nonce");
		m_nonce = 0;
	}


	TcpSocketBase::ReadOptions (header);

}
void
TcpInrpp::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader)
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
	  }

	TcpSocketBase::ReceivedAck (packet,tcpHeader);
}

int
TcpInrpp::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC("Endpoint " << m_endPoint << " " << m_boundnetdevice->GetObject<PointToPointNetDevice>()->GetDataRate().GetBitRate());
  m_tcpRate = m_boundnetdevice->GetObject<PointToPointNetDevice>()->GetDataRate().GetBitRate();
  NS_ABORT_MSG_IF (flags, "use of flags is not supported in TcpSocketBase::Send()");
  if (m_state == ESTABLISHED || m_state == SYN_SENT || m_state == CLOSE_WAIT)
    {
      // Store the packet into Tx buffer
      if (!m_txBuffer->Add (p))
        { // TxBuffer overflow, send failed
          m_errno = ERROR_MSGSIZE;
          return -1;
        }
      if (m_shutdownSend)
        {
          m_errno = ERROR_SHUTDOWN;
          return -1;
        }
      // Submit the data to lower layers
      NS_LOG_LOGIC ("txBufSize=" << m_txBuffer->Size () << " state " << TcpStateName[m_state]);
      if (m_state == ESTABLISHED || m_state == CLOSE_WAIT)
        { // Try to send the data out
          if (!m_sendPendingDataEvent.IsRunning ())
            {
              m_sendPendingDataEvent = Simulator::Schedule ( TimeStep (1), &TcpInrpp::SendPendingData, this, m_connected);
            }
        }
      return p->GetSize ();
    }
  else
    { // Connection not established yet
      m_errno = ERROR_NOTCONN;
      return -1; // Send failure
    }
}


bool
TcpInrpp::SendPendingData (bool withAck)
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

		 	  data+= sz * 8;
		 	  if(Simulator::Now().GetSeconds()-t1.GetSeconds()>m_refresh){
		 		  m_currentBW = data / (Simulator::Now().GetSeconds()-t1.GetSeconds());
		 		  data = 0;
		 		  double alpha = 0.4;
		 		  double   sample_bwe = m_currentBW;
		 		  m_currentBW = (alpha * m_lastBW) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW) / 2));
		 		  m_lastSampleBW = sample_bwe;
		 		  m_lastBW = m_currentBW;
		 		  t1 = Simulator::Now();

		 	  }

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

		 	  data+= sz * 8;

		 	  NS_LOG_LOGIC("Data " << data);
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

		  // Try to send more data
		  if (!m_sendPendingDataEvent.IsRunning ())
		    {
			 Time t = Seconds(((double)(m_segmentSize+54)*8)/m_tcpRate);
			 NS_LOG_LOGIC("Schedule next packet at " << t.GetSeconds());
		      m_sendPendingDataEvent = Simulator::Schedule (t, &TcpInrpp::SendPendingData, this, m_connected);
		    }

		  NS_LOG_LOGIC ("SendPendingData sent " << nPacketsSent << " packets " << m_tcpRate << " " << " rate");
		  return (nPacketsSent > 0);
	  }


}

void
TcpInrpp::AddOptions (TcpHeader& tcpHeader)
{
	// NS_LOG_FUNCTION (this << tcpHeader << m_nonce);

	 if(m_nonce!=0)
	 {
		 Ptr<TcpOptionInrpp> option = CreateObject<TcpOptionInrpp> ();
		 option->SetNonce (m_nonce);
		 tcpHeader.AppendOption (option);
	 }
	 TcpSocketBase::AddOptions (tcpHeader);
}

void
TcpInrpp::SetRate(uint32_t rate)
{
	m_tcpRate = rate;
}

} // namespace ns3
