/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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
 */

#include "inrpp-route.h"

#include "ns3/net-device.h"
#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppRoute");

InrppRoute::InrppRoute ()
{
  NS_LOG_FUNCTION (this);
}

void
InrppRoute::SetDestination (Ipv4Address dest)
{
  NS_LOG_FUNCTION (this << dest);
  m_destAddress = dest;
}

Ipv4Address
InrppRoute::GetDestination (void) const
{
  NS_LOG_FUNCTION (this);
  return m_destAddress;
}

void
InrppRoute::SetDetour (Ipv4Address src)
{
  NS_LOG_FUNCTION (this << src);
  m_detourAddress = src;
}

Ipv4Address
InrppRoute::GetDetour(void) const
{
  NS_LOG_FUNCTION (this);
  return m_detourAddress;
}



void
InrppRoute::SetOutputDevice (Ptr<NetDevice> outputDevice)
{
  NS_LOG_FUNCTION (this << outputDevice);
  m_detourDevice = outputDevice;
}

Ptr<NetDevice>
InrppRoute::GetOutputDevice (void) const
{
  NS_LOG_FUNCTION (this);
  return m_detourDevice;
}

std::ostream& operator<< (std::ostream& os, InrppRoute const& route)
{
  os << "source=" << route.GetDetour () << " dest="<< route.GetDestination () <<" gw=" << route.GetOutputDevice ();
  return os;
}


} // namespace ns3
