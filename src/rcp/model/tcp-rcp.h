/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Adrian Sai-wah Tam
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
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#ifndef TCP_RCP_H
#define TCP_RCP_H

#include "ns3/tcp-socket-base.h"

namespace ns3 {

/**
 * \ingroup socket
 * \ingroup tcp
 *
 * \brief An implementation of a stream socket using TCP.
 *
 * This class contains the NewReno implementation of TCP, as of \RFC{2582}.
 */
class TcpRcp : public TcpSocketBase
{
public:

	typedef enum
	{RCP_INACT,
	 RCP_SYNSENT,
	 RCP_CONGEST,
	 RCP_RUNNING,
	 RCP_RUNNING_WREF,
	 RCP_FINSENT,
	 RCP_RETRANSMIT} States2_t;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * Create an unbound tcp socket.
   */
  TcpRcp (void);
  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpRcp (const TcpRcp& sock);
  virtual ~TcpRcp (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address);
  virtual int Listen (void);
  void SetRate (uint32_t rate );
  //virtual int Send (Ptr<Packet> p, uint32_t flags);

protected:
  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpRcp> to clone me
  virtual void NewAck (SequenceNumber32 const& seq); // Inc cwnd and call NewAck() of parent
  virtual void DupAck (const TcpHeader& t, uint32_t count);  // Halving cwnd and reset nextTxSequence
  virtual void Retransmit (void); // Exit fast recovery upon retransmit timeout
  virtual void ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader);
  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSegSize (uint32_t size);
  virtual void     SetInitialSSThresh (uint32_t threshold);
  virtual uint32_t GetInitialSSThresh (void) const;
  virtual void     SetInitialCwnd (uint32_t cwnd);
  virtual uint32_t GetInitialCwnd (void) const;
  virtual void ScaleSsThresh (uint8_t scaleFactor);
  bool SendPendingData (bool withAck);
  void AddOptions (TcpHeader& tcpHeader);
  virtual void ReadOptions (const TcpHeader& tcpHeader);
  //void UpdateRate();
 // void ReceivedData (Ptr<Packet> p, const TcpHeader& tcpHeader);
private:
  /**
   * \brief Set the congestion window when connection starts
   */
  void InitializeCwnd (void);
  //void SendAck(void);
  double RCP_desired_rate();
  void Timeout();
  void RtxTimeout();
  void RateChange();

protected:
  TracedValue<uint32_t>  m_cWnd;         //!< Congestion window
  //TracedValue<uint32_t>  m_ssThresh;     //!< Slow Start Threshold
  uint32_t               m_initialCWnd;  //!< Initial cWnd value
  uint32_t               m_initialSsThresh;  //!< Initial Slow Start Threshold value
  SequenceNumber32       m_recover;      //!< Previous highest Tx seqnum for fast recovery
  uint32_t               m_retxThresh;   //!< Fast Retransmit threshold
  bool                   m_inFastRec;    //!< currently in fast recovery
  bool                   m_limitedTx;    //!< perform limited transmit
  uint32_t 				 m_nonce;
  uint8_t 				 m_flag;
  uint32_t 				 m_rate;
  uint32_t 				 m_initialRate;
  bool m_back;
  uint32_t m_lastSeq;

  TracedValue<double>    m_currentBW;              //!< Current value of the estimated BW
  double                 m_lastSampleBW;           //!< Last bandwidth sample
  double                 m_lastBW;                 //!< Last bandwidth sample after being filtered
  Time t1;
  uint32_t data;
  uint32_t m_slot;
  uint32_t 				 m_tcpRate;
  double 				 m_refresh;
  //EventId m_ackEvent;       //!< Transmit cached packet event
  //Time t;
  //double m_ackInterval;

private:
	double lastpkttime_;
	double rtt_;
	double min_rtt_;
	int seqno_;
	int ref_seqno_; /* Masayoshi */
	int init_refintv_fix_; /* Masayoshi */
	double interval_;
	double numpkts_;
	int num_sent_;
	int RCP_state;
	int numOutRefs_;

	/* for retransmissions */
	int num_dataPkts_acked_by_receiver_;   // number of packets acked by receiver
	int num_dataPkts_received_;            // Receiver keeps track of number of packets it received
	int num_Pkts_to_retransmit_;           // Number of data packets to retransmit
	int num_pkts_resent_;                  // Number retransmitted since last RTO
	int num_enter_retransmit_mode_;        // Number of times we are entering retransmission mode

	EventId m_rcpTimeout;
	EventId m_refTransmit;
	EventId m_rxTimeout;

};

} // namespace ns3

#endif /* TCP_RCP_H */
