/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hajime Tazaki
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
 * Authors: Hajime Tazaki <tazaki@sfc.wide.ad.jp>
 */


#include "inrpp-backp-tag.h"

#include <stdint.h>
#include "ns3/ipv4-address.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppBackpressureTag");


TypeId
InrppBackpressureTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppBackpressureTag")
    .SetParent<Tag> ()
    .AddConstructor<InrppBackpressureTag> ()
  ;
  return tid;
}
TypeId
InrppBackpressureTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t
InrppBackpressureTag::GetSerializedSize (void) const
{
  return 10;
}
void
InrppBackpressureTag::Serialize (TagBuffer i) const
{
	  NS_LOG_FUNCTION (this << &i);
//	  m_nonce.Serialize (buf);
	  i.WriteU8 (m_flag);
	  i.WriteU32 (m_nonce);
	  i.WriteU32(m_deltaRate);

}
void
InrppBackpressureTag::Deserialize (TagBuffer i)
{
	 NS_LOG_FUNCTION (this<< &i);
	 m_flag = i.ReadU8 ();
	 m_nonce = i.ReadU32 ();
	 m_deltaRate = i.ReadU32();
//	  m_nonce = Ipv4Address::Deserialize (buf);
}
void
InrppBackpressureTag::Print (std::ostream &os) const
{
	 NS_LOG_FUNCTION (this << &os);
	  os << "Ipv4 PKTINFO [DestAddr: " << m_flag;
	  os << "] ";}

void
InrppBackpressureTag::SetFlag (uint8_t flag)
{
  NS_LOG_FUNCTION (this << flag);
  m_flag = flag;
}
uint8_t
InrppBackpressureTag::GetFlag (void) const
{
  NS_LOG_FUNCTION (this);
  return m_flag;
}

uint32_t
InrppBackpressureTag::GetNonce (void) const
{
	  NS_LOG_FUNCTION (this);
	  return m_nonce;
}

void
InrppBackpressureTag::SetNonce (uint32_t nonce)
{
  NS_LOG_FUNCTION (this << nonce);
  m_nonce = nonce;
}

uint32_t
InrppBackpressureTag::GetDeltaRate (void) const
{
	  NS_LOG_FUNCTION (this);
	  return m_deltaRate;
}

void
InrppBackpressureTag::SetDeltaRate (uint32_t deltaRate)
{
  NS_LOG_FUNCTION (this << deltaRate);
  m_deltaRate = deltaRate;
}

} // namespace ns3

