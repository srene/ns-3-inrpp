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


#include "inrpp-tag.h"

#include <stdint.h>
#include "ns3/ipv4-address.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppTag");


TypeId
InrppTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppTag")
    .SetParent<Tag> ()
    .AddConstructor<InrppTag> ()
  ;
  return tid;
}
TypeId
InrppTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t
InrppTag::GetSerializedSize (void) const
{
  return 4;
}
void
InrppTag::Serialize (TagBuffer i) const
{
	  NS_LOG_FUNCTION (this << &i);
	  uint8_t buf[4];
	  m_addr.Serialize (buf);
	  i.Write (buf, 4);
}
void
InrppTag::Deserialize (TagBuffer i)
{
	 NS_LOG_FUNCTION (this<< &i);
	  uint8_t buf[4];
	  i.Read (buf, 4);
	  m_addr = Ipv4Address::Deserialize (buf);
}
void
InrppTag::Print (std::ostream &os) const
{
	 NS_LOG_FUNCTION (this << &os);
	  os << "Ipv4 PKTINFO [DestAddr: " << m_addr;
	  os << "] ";}

void
InrppTag::SetAddress (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_addr = addr;
}
Ipv4Address
InrppTag::GetAddress (void) const
{
  NS_LOG_FUNCTION (this);
  return m_addr;
}

} // namespace ns3

