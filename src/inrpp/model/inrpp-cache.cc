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
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-route.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppCache");



NS_OBJECT_ENSURE_REGISTERED (InrppCache);

CachedPacket::CachedPacket (Ptr<const Packet> p, Ptr<Ipv4Route> route)
{
	NS_LOG_FUNCTION (this);
	m_packet = p;
	m_route = route;
}
CachedPacket::~CachedPacket ()
{
	NS_LOG_FUNCTION (this);
}

Ptr<const Packet>
CachedPacket::GetPacket()
{
	return m_packet;
}

Ptr<Ipv4Route>
CachedPacket::GetRoute()
{
	return m_route;
}


TypeId 
InrppCache::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppCache")
    .SetParent<Object> ()
    .AddAttribute ("MaxCacheSize",
                   "The size of the queue for packets pending an arp reply.",
                   UintegerValue (10000000),
                   MakeUintegerAccessor (&InrppCache::GetMaxSize,
                           	   	   	   	 &InrppCache::SetMaxSize),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("HighThresholdCacheSize",
				   "The size of the queue for packets pending an arp reply.",
				   UintegerValue (8000000),
				   MakeUintegerAccessor (&InrppCache::m_highSizeTh),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("LowThresholdCacheSize",
				   "The size of the queue for packets pending an arp reply.",
				   UintegerValue (5000000),
				   MakeUintegerAccessor (&InrppCache::m_lowSizeTh),
				   MakeUintegerChecker<uint32_t> ())
	.AddTraceSource ("Size",
					 "Remote side's flow control window",
					 MakeTraceSourceAccessor (&InrppCache::m_size),
					 "ns3::TracedValue::Uint32Callback")
  ;
  return tid;
}

InrppCache::InrppCache ():
m_size(0),
m_packets(0)
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

bool
InrppCache::Insert(Ptr<InrppInterface> iface,Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet)
{
	NS_LOG_FUNCTION(this<<m_size);
	if(m_size.Get()+packet->GetSize()<=m_maxCacheSize)
	{

	    if ((m_size.Get()+m_packets>m_highSizeTh)&&!m_hTh)
		{
		  NS_LOG_LOGIC ("Queue reaching full " << this);
		  if(!m_highTh.IsNull())m_highTh (m_size.Get());
		  m_hTh = true;
		  m_lTh = false;

		}

		Ptr<CachedPacket>p = CreateObject<CachedPacket> (packet,rtentry);
		m_InrppCache.insert(PairCache(iface,p));
		m_size+=packet->GetSize();
		std::map<Ptr<InrppInterface>,uint32_t>::iterator it = m_ifaceSize.find(iface);
		if(it!=m_ifaceSize.end())
		{
		  NS_LOG_LOGIC("Size found " << it->second);
		  it->second += packet->GetSize();
		} else {
			m_ifaceSize.insert(std::pair<Ptr<InrppInterface>,uint32_t>(iface,packet->GetSize()));
		}
		return true;
	} else {
		return false;
	}

}

bool
InrppCache::InsertFirst(Ptr<InrppInterface> iface,Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet)
{
	NS_LOG_FUNCTION(this<<m_size);
	if(m_size.Get()+packet->GetSize()<=m_maxCacheSize)
	{

	    if ((m_size.Get()+m_packets>m_highSizeTh)&&!m_hTh)
		{
		  NS_LOG_LOGIC ("Queue reaching full " << this);
		  if(!m_highTh.IsNull())m_highTh (m_size.Get());
		  m_hTh = true;
		  m_lTh = false;

		}

		Ptr<CachedPacket>p = CreateObject<CachedPacket> (packet,rtentry);
		m_InrppCache.insert(m_InrppCache.begin(),PairCache(iface,p));
		m_size+=packet->GetSize();
		std::map<Ptr<InrppInterface>,uint32_t>::iterator it = m_ifaceSize.find(iface);
		if(it!=m_ifaceSize.end())
		{
		  NS_LOG_LOGIC("Size found " << it->second);
		  it->second += packet->GetSize();
		} else {
			m_ifaceSize.insert(std::pair<Ptr<InrppInterface>,uint32_t>(iface,packet->GetSize()));
		}
		return true;
	} else {
		return false;
	}

}

Ptr<CachedPacket>
InrppCache::GetPacket(Ptr<InrppInterface> iface)
{
	Ptr<CachedPacket> p;
	CacheIter it = m_InrppCache.find(iface);
	if(it!=m_InrppCache.end())
	{

		p = it->second;
		m_InrppCache.erase (it);
		m_size-=p->GetPacket()->GetSize();

	    if ((m_size.Get()<=m_lowSizeTh)&&(!m_lTh&&m_hTh))
		{
		  NS_LOG_LOGIC ("Queue uncongested " << this);
		  if(!m_lowTh.IsNull())m_lowTh (m_size.Get());
		  m_lTh = true;
		  m_hTh = false;
		}

		std::map<Ptr<InrppInterface>,uint32_t>::iterator it = m_ifaceSize.find(iface);
		if(it!=m_ifaceSize.end())
		{
		  NS_LOG_LOGIC("Size found " << it->second);
		  it->second-=p->GetPacket()->GetSize();
		}


		return p;
	}

	return NULL;

}

void
InrppCache::SetMaxSize (uint32_t size)
{
  NS_LOG_FUNCTION(this <<size);
  m_maxCacheSize = size;
  NS_LOG_LOGIC("Cache " <<m_maxCacheSize);

}

uint32_t
InrppCache::GetMaxSize (void) const
{
  NS_LOG_INFO(this);
  return m_maxCacheSize;
}

uint32_t
InrppCache::GetSize (Ptr<InrppInterface> iface)
{
  NS_LOG_INFO(this);
  uint32_t m_size = 0;
  std::map<Ptr<InrppInterface>,uint32_t>::iterator it = m_ifaceSize.find(iface);
  if(it!=m_ifaceSize.end())
  {
	  m_size = it->second;
  }
  return m_size;
}

uint32_t
InrppCache::GetSize ()
{
	return m_size;
}
void
InrppCache::SetHighThCallback(HighThCallback cb)
{
	NS_LOG_FUNCTION (this << &cb);
	m_highTh = cb;
}

void
InrppCache::SetLowThCallback(LowThCallback cb)
{
	m_lowTh = cb;

}

bool
InrppCache::IsFull()
{
	return (m_size.Get() >= m_highSizeTh);
}

void
InrppCache::AddPacket()
{
	m_packets++;
}

void
InrppCache::RemovePacket()
{
	m_packets--;
}

uint32_t
InrppCache::GetThreshold()
{
	return m_lowSizeTh;
}


} // namespace ns3

