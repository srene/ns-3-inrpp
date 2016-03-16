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

#ifndef RCPQUEUE_H
#define RCPQUEUE_H

#include <queue>
#include "ns3/packet.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/net-device.h"
#include "ns3/tcp-header.h"
namespace ns3 {

class TraceContainer;

typedef Callback< void, uint32_t,Ptr<NetDevice> > HighThCallback;
typedef Callback< void, uint32_t,Ptr<NetDevice> > LowThCallback;
typedef Callback< void, Ptr<const Packet> > DropCallback;

/**
 * \ingroup queue
 *
 * \brief A FIFO packet queue that drops tail-end packets on overflow
 */
class RcpQueue : public DropTailQueue {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief DropTailQueue Constructor
   *
   * Creates a droptail queue with a maximum size of 100 packets by default
   */
  RcpQueue ();

  virtual ~RcpQueue();

 // void SetHighThCallback(HighThCallback cb);

 // void SetLowThCallback(LowThCallback cb);

 // void SetDropCallback(DropCallback cb);

 // void SetNetDevice(Ptr<NetDevice> dev);

 // uint32_t GetNBytes();
private:
  virtual bool DoEnqueue (Ptr<Packet> p);

  void DoOnPacketArrival(Ptr<Packet> p);
  void DoBeforePacketDeparture(Ptr<Packet> p);
  void FillInFeedback(const TcpHeader &header);
  virtual Ptr<Packet> DoDequeue (void);
  /* virtual Ptr<const Packet> DoPeek (void) const;

  std::queue<Ptr<Packet> > m_packets; //!< the packets in the queue
  uint32_t m_bytesInQueue;            //!< actual bytes in the queue
  QueueMode m_mode;                   //!< queue mode (packets or bytes limited)
  */

  uint32_t m_highPacketsTh;              //!< higher threshold packets in the queue
  uint32_t m_highBytesTh;                //!< higher threshold bytes in the queue
  uint32_t m_lowPacketsTh;              //!< lower threshold packets in the queue
  uint32_t m_lowBytesTh;                //!< lower threshold bytes in the queue

  HighThCallback m_highTh;
  LowThCallback m_lowTh;
  DropCallback m_drop;
  Ptr<NetDevice> m_netDevice;
  bool m_hTh;
  bool m_lTh;

  double m_linkCapacity;

};

} // namespace ns3

#endif /* RCPQUEUE_H */
