/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
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

namespace ns3 {

class ArpCache;
class NetDevice;
class Node;
class Packet;
class Ipv4Interface;


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

  /**
   * \brief Set the node the ARP L3 protocol is associated with
   * \param node the node
   */
//  void SetNode (Ptr<Node> node);

  /**
   * \brief Create an ARP cache for the device/interface
   * \param device the NetDevice
   * \param interface the Ipv4Interface
   * \returns a smart pointer to the ARP cache
   */
  Ptr<InrppCache> CreateCache (Ptr<NetDevice> device, Ptr<Ipv4Interface> interface);

  /**
   * \brief Receive a packet
   * \param device the source NetDevice
   * \param p the packet
   * \param protocol the protocol
   * \param from the source address
   * \param to the destination address
   * \param packetType type of packet (i.e., unicast, multicast, etc.)
   */
//  void Receive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from, const Address &to,
//                NetDevice::PacketType packetType);


  void InrppReceive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from, const Address &to,
                NetDevice::PacketType packetType);

  void SetDetourRoute(Ptr<NetDevice> netdevice, Ptr<InrppRoute> route);
  void SendDetourInfo(Ptr<NetDevice> devSource, Ptr<NetDevice> devDestination, Ipv4Address infoAddress);
  //void SetDetourRoute(Ipv4Address address, Ptr<InrppRoute> route);
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
  void SendInrppInfo (Ptr<InrppInterface> iface, Ptr<NetDevice> device, Ipv4Address address);


  uint32_t AddInterface (Ptr<NetDevice> device);

  void IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header);
  void Receive ( Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                            const Address &to, NetDevice::PacketType packetType);
  Ptr<InrppCache> m_cache; //!< ARP cache container

  std::map <Ptr<Ipv4Interface>, uint32_t> m_intList;


};

} // namespace ns3


#endif /* ARP_L3_PROTOCOL_H */
