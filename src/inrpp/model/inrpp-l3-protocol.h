/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University College of London
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

#ifndef INRPP_L3_PROTOCOL_H
#define INRPP_L3_PROTOCOL_H

#include <list>

#include "inrpp-cache.h"
#include "inrpp-route.h"
#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/address.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-l3-protocol.h"
#include "inrpp-interface.h"
#include "ns3/tcp-header.h"
#include "ns3/tcp-option-inrpp.h"
#include "ns3/tcp-option-inrpp-back.h"

namespace ns3 {

class NetDevice;
class Node;
class Packet;
class Ipv4Interface;
class InrppInterface;
class InrppCache;

/**
 * \ingroup internet
 * \defgroup arp Arp
 *
 * This is an overview of Arp capabilities (write me).
 */
/**
 * \ingroup arp
 * \brief An implementation of the ARP protocol
 */
class InrppL3Protocol : public Ipv4L3Protocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
//  static const uint8_t PROT_NUMBER; //!< INRPP protocol number (0x00FD)
  static const uint16_t PROT_NUMBER; //!< INRPP protocol number (0x00FD)
  InrppL3Protocol ();
  virtual ~InrppL3Protocol ();

  void SetL3Protocol (Ptr<Ipv4L3Protocol> ipv4);


  void InrppReceive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from, const Address &to,
                NetDevice::PacketType packetType);

  void SetDetourRoute(Ptr<NetDevice> netdevice, Ptr<InrppRoute> route);
  void SendDetourInfo(uint32_t sourceIface, uint32_t destIface, Ipv4Address address);
  void SendData (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p);
  //void SetDetourRoute(Ipv4Address address, Ptr<InrppRoute> route);
  Ptr<InrppCache> GetCache();
  void LostPacket(Ptr<const Packet> packet, Ptr<InrppInterface> iface,Ptr<NetDevice> device);

  void Discard(Ptr<const Packet> packet);


  void NotifyState(Ptr<InrppInterface> iface, uint32_t state);

  void SetCallback(Callback<void,Ptr<InrppInterface>,uint32_t > cb);

  void ChangeFlag();
protected:
  virtual void DoDispose (void);
  /*
   * This function will notify other components connected to the node that a new stack member is now connected
   * This will be used to notify Layer 3 protocol of layer 4 protocol stack to connect them together.
   */
//  virtual void NotifyNewAggregate ();
private:
  typedef std::list<Ptr<ArpCache> > CacheList; //!< container of the ARP caches
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \param o
   */
  InrppL3Protocol (const InrppL3Protocol &o);
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \param o
   * \returns
   */
  InrppL3Protocol &operator = (const InrppL3Protocol &o);


  /**
   * \brief Configure the callbacks for the NetDevice
   */
  //void SetNetDevices (Ptr<NetDevice> device);


  /**
   * \brief Send an ARP request to an host
   * \param cache the ARP cache to use
   * \param to the destination IP
   */
  //void SendInrppInfo (Ptr<InrppInterface> iface, Ptr<NetDevice> device, Ipv4Address address);
  void SendInrppInfo (Ptr<InrppInterface> sourceIface, Ptr<InrppInterface> destIface, Ipv4Address infoAddress);

  uint32_t AddInterface (Ptr<NetDevice> device);

  void IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header);

  void Receive ( Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                            const Address &to, NetDevice::PacketType packetType);

  void HighTh( uint32_t packets);

  void LowTh( uint32_t packets);

  bool AddOptionInrpp (TcpHeader& header,uint32_t nonce);

  bool AddOptionInrppBack (TcpHeader& header,uint8_t flag,uint32_t nonce);

  void ProcessInrppOption(TcpHeader& header,Ptr<InrppInterface> iface);

  //Ptr<InrppInterface> FindDetourIface(Ptr<InrppInterface> iface);

  bool IsNonceFromInterface(uint32_t nonce);

  Ptr<InrppCache> m_cache; //!< ARP cache container

 // Ptr<InrppCache> m_secondCache; //!< ARP cache container

  bool m_cacheFull;
  //std::map <Ipv4Address, uint32_t> m_residualList;
  std::vector <uint32_t> m_noncesList;
  bool m_mustCache;
  uint32_t m_initCache;
  uint32_t m_rate;
  uint32_t m_back;
  //Time t;
  //Ptr<Ipv4Route> m_route;
  std::map <Ptr<const Packet>, Ptr<Ipv4Route> > m_routeList;

  Callback<void,Ptr<InrppInterface>,uint32_t > m_cb;

  uint8_t m_cacheFlag;

  EventId m_flagEvent;       //!< Transmit cached packet event

  uint32_t m_numSlot;

  uint32_t m_packetSize;
  //std::map<uint32_t,uint32_t> m_weights;

};

} // namespace ns3


#endif /* ARP_L3_PROTOCOL_H */
