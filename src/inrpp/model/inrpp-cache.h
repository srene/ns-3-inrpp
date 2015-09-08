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
#ifndef INRPP_CACHE_H
#define INRPP_CACHE_H

#include <stdint.h>
#include <list>
#include <map>
#include "ns3/simulator.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-address.h"
#include "ns3/address.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/output-stream-wrapper.h"
#include "inrpp-interface.h"

namespace ns3 {

class NetDevice;
class Ipv4Interface;
class InrppInterface;
/**
 * \ingroup arp
 * \brief An ARP cache
 *
 * A cached lookup table for translating layer 3 addresses to layer 2.
 * This implementation does lookups from IPv4 to a MAC address
 */
class InrppCache : public Object
{

public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  InrppCache ();
  ~InrppCache ();

  void Flush ();

  void Insert(Ptr<InrppInterface> iface,Ptr<const Packet> packet);

  Ptr<const Packet> GetPacket(Ptr<InrppInterface> iface);
private:

  typedef std::pair<Ptr<InrppInterface>,Ptr<const Packet> > PairCache;
  typedef std::multimap<Ptr<InrppInterface>,Ptr<const Packet> > Cache;
  typedef std::multimap<Ptr<InrppInterface>,Ptr<const Packet> >::iterator CacheIter;

  Cache m_InrppCache; //!< the ARP cache
};


} // namespace ns3

#endif /* INRPP_CACHE_H */
