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
#include "inrpp-cache.h"
#include "ns3/assert.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/names.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppCache");

NS_OBJECT_ENSURE_REGISTERED (InrppCache);

TypeId 
InrppCache::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppCache")
    .SetParent<Object> ()
  ;
  return tid;
}

InrppCache::InrppCache ()
{
  NS_LOG_FUNCTION (this);
}

InrppCache::~InrppCache ()
{
  NS_LOG_FUNCTION (this);
}

void
InrppCache::Flush (void)
{

}

void
InrppCache::Insert(Ptr<InrppInterface> iface,Ptr<const Packet> packet)
{

	m_InrppCache.insert(PairCache(iface,packet));
}

Ptr<const Packet>
InrppCache::GetPacket(Ptr<InrppInterface> iface)
{
	CacheIter it = m_InrppCache.find(iface);
	m_InrppCache.erase (it);

	return it->second;

}

} // namespace ns3

