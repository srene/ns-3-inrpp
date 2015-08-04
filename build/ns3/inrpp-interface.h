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

#include <list>
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "inrpp-route.h"

namespace ns3 {

class NetDevice;
class Packet;
class Node;
class ArpCache;

/**
 * \brief Names of the INRPP states
 */
enum InrppState{
	N0_DETOUR,       // 0
	DETOUR,       // 1
	BACKPRESSURE,     // 2
	UP_BACKPRESSURE,     // 3
	UP_AND_PROP_BACKPRESSURE  // 4
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

private:



  //std::map <Ipv4Address, Ptr<InrppRoute> > m_routeList;
  Ptr<InrppRoute> m_detourRoute;
  InrppState m_state;

};

} // namespace ns3

#endif
