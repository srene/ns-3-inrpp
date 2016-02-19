/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
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
 * Authors: 
 *  Mathieu Lacage <mathieu.lacage@sophia.inria.fr>,
 *  Tom Henderson <tomh@tomh.org>
 */
#ifndef INRPP_INTERFACE_H
#define INRPP_INTERFACE_H

#include <queue>
#include <list>
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "inrpp-route.h"
#include "ns3/simulator.h"
#include "ns3/traced-value.h"
#include "inrpp-cache.h"
#include "ns3/data-rate.h"
#include "inrpp-l3-protocol.h"
#include "ns3/ipv4-route.h"
#include <map>

namespace ns3 {

class NetDevice;
class Packet;
class Node;
class InrppCache;
class InrppL3Protocol;



/**
 * \brief Names of the INRPP states
 */
enum InrppState{
	NO_DETOUR,       // 0
	DETOUR,       // 1
	BACKPRESSURE,     // 2
	UP_BACKPRESSURE,     // 3
	PROP_BACKPRESSURE,  // 4
	DISABLE_BACK   // 5
	 } ;

/**
 * \brief The INRPP representation of a network interface
 *
 * This class roughly corresponds to the struct in_device
 * of Linux; the main purpose is to provide address-family
 * specific information (addresses) about an interface.
 *
 * By default, Ipv4 interface are created in the "down" state
 * no IP addresses.  Before becoming usable, the user must
 * add an address of some type and invoke Setup on them.
 */
class InrppInterface  : public Ipv4Interface
{


public:

  /**
   * \brief Get the type ID
   * \return type ID
   */
  static TypeId GetTypeId (void);

  InrppInterface ();
  virtual ~InrppInterface();


  /**
   * \brief High queue occupancy threshold callback
   *
   * \param IpV4 Interface
   * \returns
   */
//   void HighTh(Ptr<Ipv4Interface> iface, uint32_t packets);
   void HighTh(uint32_t packets,Ptr<NetDevice> dev);

  /**
   * \brief Low queue occupancy threshold callback
   *
   * \param IpV4 Interface
   * \returns
   */
  void LowTh(uint32_t packets,Ptr<NetDevice> dev);

  Ptr<InrppRoute> GetDetour(void);

  void SetDetour(Ptr<InrppRoute> route);

  /**
  * \brief Set the NetDevice.
  * \param device NetDevice
  */
 void SetDevice (Ptr<NetDevice> device);

 InrppState GetState(void);

 void SetState(InrppState state);

 uint32_t GetResidual();

 uint32_t GetResidual(Ipv4Address address);

 uint32_t GetBW();

 uint32_t GetFlow();

 uint32_t GetDetoured();

 void CalculateFlow(Ptr<const Packet> p);

 void SetDeltaRate(uint32_t deltaRate);
 uint32_t GetDeltaRate(void);
 uint32_t GetNonce(void);

 void SetCache(Ptr<InrppCache> cache);

 void SetRate(DataRate bps);

 void SendPacket();

 void SetInrppL3Protocol(Ptr<InrppL3Protocol> inrpp);

 void SetDetouredIface(Ptr<InrppInterface> interface,Ipv4Address address);

 Ptr<InrppInterface> GetDetouredIface(void);

 void UpdateResidual(Ipv4Address address, uint32_t residual);

 void CalculatePacing(uint32_t bytes);

 void SetInitCache(bool);

 bool GetInitCache(void);


 bool GetDisable(void);

 void SetNumSlot(uint32_t numSlot);

 void OneMoreDetour(Ipv4Address ip);

 void CalculateDetour(Ipv4Address ip, Ptr<const Packet> p);
// void SetWeights(std::map<uint32_t,uint32_t> weights);
private:

  void TxRx(Ptr<const Packet> p, Ptr<NetDevice> dev1 ,  Ptr<NetDevice> dev2,  Time tr, Time rcv);
  void SendResidual();
  void Drop(Ptr<const Packet> p);

  TracedValue<double>    m_currentBW;              //!< Current value of the estimated BW
  double                 m_lastSampleBW;           //!< Last bandwidth sample
  double                 m_lastBW;                 //!< Last bandwidth sample after being filtered
  Time t1;
  uint32_t data;

  TracedValue<double>    m_currentBW2;              //!< Current value of the estimated BW
  double                 m_lastSampleBW2;           //!< Last bandwidth sample
  double                 m_lastBW2;                 //!< Last bandwidth sample after being filtered
  Time t2;
  uint32_t data2;

  TracedValue<double>    m_currentBW3;              //!< Current value of the estimated BW
  double                 m_lastSampleBW3;           //!< Last bandwidth sample
  double                 m_lastBW3;                 //!< Last bandwidth sample after being filtered
  Time t3;
  uint32_t data3;

  //std::map<Ipv4Address,TracedValue<double> >    m_currentBW4;              //!< Current value of the estimated BW
  std::map<Ipv4Address,double>                  m_lastSampleBW4;           //!< Last bandwidth sample
  std::map<Ipv4Address,double>                  m_lastBW4;                 //!< Last bandwidth sample after being filtered
  std::map<Ipv4Address,Time>                    t4;
  std::map<Ipv4Address,uint32_t> data4;

  Ptr<InrppRoute> m_detourRoute;
  InrppState m_state;
  uint32_t m_nonce;
  uint32_t m_deltaRate;
  Ptr<InrppCache> m_cache;
  DataRate m_bps;
  EventId m_txEvent;       //!< Transmit cached packet event
  EventId m_txResidualEvent;
  EventId m_txAckEvent;
  Ptr<InrppL3Protocol> m_inrpp;
  std::map <Ptr<InrppInterface>, Ipv4Address> m_detouredIfaces;
  std::map <Ipv4Address, uint32_t> m_residualList;
  Ptr<InrppInterface> m_detouredIface;
  TracedValue<uint32_t>  m_residualMin;
  bool m_disable;
  uint32_t m_ackRate;
  uint32_t packetSize;
  bool m_initCache;

  uint32_t m_slot;
  uint32_t m_numSlot;
  //std::map<uint32_t,uint32_t> m_weights;

  uint32_t m_nDetour;

  std::list<Ipv4Address> m_adList;
  //uint32_t m_lastSlot;

  double m_refresh;
};

} // namespace ns3

#endif
