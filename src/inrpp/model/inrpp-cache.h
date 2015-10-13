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
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-route.h"

namespace ns3 {

class NetDevice;
class Ipv4Interface;
class InrppInterface;

class CachedPacket : public Object
{

public:

   CachedPacket (Ptr<const Packet> p, Ptr<Ipv4Route> route);
   ~CachedPacket ();

   Ptr<const Packet> GetPacket();
   Ptr<Ipv4Route> GetRoute();

private:

	Ptr<const Packet> m_packet;
	Ptr<Ipv4Route> m_route;
};


typedef std::pair<Ptr<InrppInterface>,Ptr<CachedPacket> > PairCache;
typedef std::multimap<Ptr<InrppInterface>,Ptr<CachedPacket> > Cache;
typedef std::multimap<Ptr<InrppInterface>,Ptr<CachedPacket> >::iterator CacheIter;


class InrppCache : public Object
{

	typedef Callback< void, uint32_t> HighThCallback;
	typedef Callback< void, uint32_t> LowThCallback;

public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  InrppCache ();
  ~InrppCache ();

  void SetHighThCallback(HighThCallback cb);

  void SetLowThCallback(LowThCallback cb);

  void Flush ();

  bool Insert(Ptr<InrppInterface> iface,Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet);

  Ptr<CachedPacket>  GetPacket(Ptr<InrppInterface> iface);

  void SetMaxSize (uint32_t size);

  uint32_t GetMaxSize (void) const;

  uint32_t GetSize (Ptr<InrppInterface> iface);

  uint32_t GetSize();

  bool IsFull();

private:

  Cache m_InrppCache;
  TracedValue<uint32_t> m_size;
  uint32_t m_maxCacheSize;
  std::map<Ptr<InrppInterface>,uint32_t> m_ifaceSize;
  uint32_t m_highSizeTh;
  uint32_t m_lowSizeTh;

  HighThCallback m_highTh;
  LowThCallback m_lowTh;
  bool m_hTh;
  bool m_lTh;
};


} // namespace ns3

#endif /* INRPP_CACHE_H */
