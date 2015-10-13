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

#ifndef TCP_INRPP_H
#define TCP_INRPP_H

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
class TcpInrpp : public TcpSocketBase
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * Create an unbound tcp socket.
   */
  TcpInrpp (void);
  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpInrpp (const TcpInrpp& sock);
  virtual ~TcpInrpp (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address);
  virtual int Listen (void);
  void SetRate (uint32_t rate );

protected:
  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpInrpp> to clone me
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
  void UpdateRate();

private:
  /**
   * \brief Set the congestion window when connection starts
   */
  void InitializeCwnd (void);

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
  uint32_t 				 m_tcpRate;
  EventId m_updateEvent;       //!< Transmit cached packet event
  bool m_back;
  uint32_t m_lastSeq;
};

} // namespace ns3

#endif /* TCP_NEWRENO_H */
