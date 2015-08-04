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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "inrpp-interface.h"
#include "ns3/inrpp-tail-queue.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/point-to-point-net-device.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppInterface");

NS_OBJECT_ENSURE_REGISTERED (InrppInterface);



TypeId 
InrppInterface::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppInterface")
    .SetParent<Ipv4Interface> ()

  ;
  ;
  return tid;
}

/** 
 * By default, Ipv4 interface are created in the "down" state
 *  with no IP addresses.  Before becoming useable, the user must 
 * invoke SetUp on them once an Ipv4 address and mask have been set.
 */
InrppInterface::InrppInterface ()
{
  NS_LOG_FUNCTION (this);
}

InrppInterface::~InrppInterface ()
{
  NS_LOG_FUNCTION (this);
}

void
InrppInterface::HighTh( uint32_t packets,Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION(this<<packets<<dev);
	m_state = DETOUR;
}

void
InrppInterface::LowTh(uint32_t packets,Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION(this<<packets<<dev);
	m_state = N0_DETOUR;


}

Ptr<InrppRoute>
InrppInterface::GetDetour(void)
{

	  return m_detourRoute;

}

void
InrppInterface::SetDetour(Ptr<InrppRoute> route)
{
	m_detourRoute = route;
}


InrppState
InrppInterface::GetState(void)
{
	return m_state;
}
void
InrppInterface::SetDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  Ptr<InrppTailQueue> q = device->GetObject <PointToPointNetDevice>()->GetQueue()->GetObject<InrppTailQueue>();
  q->SetNetDevice(device);
  q->SetHighThCallback (MakeCallback (&InrppInterface::HighTh,this));
  q->SetLowThCallback (MakeCallback (&InrppInterface::LowTh,this));
  Ipv4Interface::SetDevice(device);
}

} // namespace ns3

