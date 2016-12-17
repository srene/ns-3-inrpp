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

   //Cached packet object along the route and the nonce information
   CachedPacket (Ptr<const Packet> p, Ptr<Ipv4Route> route);
   CachedPacket (Ptr<const Packet> p, Ptr<Ipv4Route> route, uint32_t nonce);
   ~CachedPacket ();

   Ptr<const Packet> GetPacket();
   Ptr<Ipv4Route> GetRoute();
   uint32_t GetNonce();

private:

	Ptr<const Packet> m_packet;
	Ptr<Ipv4Route> m_route;
	  uint32_t m_nonce;

};

typedef std::pair<Ptr<InrppInterface>,uint32_t> PairKey;
typedef std::pair<PairKey,Ptr<CachedPacket> > PairCache;
typedef std::multimap<PairKey,Ptr<CachedPacket> > Cache;

typedef std::multimap<PairKey,Ptr<CachedPacket> >::iterator CacheIter;


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

  //Set callback when cache is full
  void SetHighThCallback(HighThCallback cb);
  //Set callback when cache is empty again
  void SetLowThCallback(LowThCallback cb);
  //Flush cache
  void Flush ();
  //Insert packet to the cache
  bool Insert(Ptr<InrppInterface> iface, uint32_t flag,Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet);
  //Insert packet at the beginning of the cache after a drop or when packet not sent
  bool InsertFirst(Ptr<InrppInterface> iface,uint32_t flag, Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet);

  bool Insert(Ptr<InrppInterface> iface,uint32_t flag, Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet, uint32_t nonce);
  Ptr<CachedPacket>  GetPacket(Ptr<InrppInterface> iface,uint32_t flag);
  //Insert packet at the beginning of the cache after a drop or when packet not sent
  bool InsertFirst(Ptr<InrppInterface> iface,uint32_t flag, Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet,uint32_t nonce);

  //Set the max size of the cache
  void SetMaxSize (uint32_t size);
  //Get the max size of the cache
  uint32_t GetMaxSize (void) const;
  //Get the cache occupancy x iface and slot
  uint32_t GetSize (Ptr<InrppInterface> iface,uint32_t slot);
  //Get the occupancy of the whole cache
  uint32_t GetSize();

  //Return true when the cache occupancy is over the high_tr
  bool IsFull();

  //Increase the packet count
  void AddPacket();
  //Decrease the packet count
  void RemovePacket();
  //Return the low threshold of the cache occupancy
  uint32_t GetThreshold();

private:

  Cache m_InrppCache;
  TracedValue<uint32_t> m_size;
  uint32_t m_maxCacheSize;
  std::map<PairKey,uint32_t> m_ifaceSize;
  uint32_t m_highSizeTh;
  uint32_t m_lowSizeTh;

  uint32_t m_redSizeTh;
  std::map<uint32_t,uint32_t> m_nonces;


  HighThCallback m_highTh;
  LowThCallback m_lowTh;
  bool m_hTh;
  bool m_lTh;

  uint32_t m_packets;

  uint32_t m_split,m_round;


  bool red;
};


} // namespace ns3

#endif /* INRPP_CACHE_H */
