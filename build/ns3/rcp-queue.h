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
#include "ns3/event-id.h"
#include "ns3/data-rate.h"


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
  void Init();
  virtual bool DoEnqueue (Ptr<Packet> p);
  double RunningAvg(double var_sample, double var_last_avg, double gain);
  void Timeout();
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

  // RCPQTimer        queue_timer_;
   double          Tq_;

   // Rui: link_capacity_ is set by tcl script.
   // Rui: must call set-link-capacity after building topology and before running simulation
   double link_capacity_;
   double input_traffic_;       // traffic in Tq_
   double act_input_traffic_;
   double output_traffic_;
   double traffic_spill_;  // parts of packets that should fall in next slot
   double last_load_;
   double end_slot_; // end time of the current time slot
   double avg_rtt_;
   double this_Tq_rtt_sum_;
   double this_Tq_rtt_;
   double this_Tq_rtt_numPkts_;
   int input_traffic_rtt_;
   double rtt_moving_gain_;
   int Q_;
   int Q_last;
   double flow_rate_;
   bool m_init;
   double m_alpha;  // Masayoshi
   double m_beta;   // Masayoshi
   double m_gamma;
   double min_pprtt_;   // Masayoshi minimum packet per rtt
   double init_rate_fact_;    // Masayoshi
   int    print_status_;      // Masayoshi
   int    rate_fact_mode_;    // Masayoshi
   double fixed_rate_fact_;   // Masayoshi
   double propag_rtt_ ;       // Masayoshi (experimental, used with rate_fact_mode_ = 3)
   double upd_timeslot_ ;       // Masayoshi

  // Tcl_Channel   channel_;      // Masayoshi

   double m_phi,m_rtt,m_rttGain;

   EventId queue_timer_;

   DataRate       m_bps;


};

} // namespace ns3

#endif /* RCPQUEUE_H */
