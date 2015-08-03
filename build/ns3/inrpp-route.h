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
#ifndef INRPP_ROUTE_H
#define INRPP_ROUTE_H

#include <list>
#include <map>
#include <ostream>

#include "ns3/simple-ref-count.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

class NetDevice;

/**
 * \ingroup ipv4Routing
 *
 *\brief Ipv4 route cache entry (similar to Linux struct rtable)
 *
 * This is a reference counted object.  In the future, we will add other 
 * entries from struct dst_entry, struct rtable, and struct dst_ops as needed.
 */
class InrppRoute : public SimpleRefCount<InrppRoute>
{
public:
  InrppRoute ();

  /**
   * \param dest Destination Ipv4Address
   */
  void SetDestination (Ipv4Address dest);
  /**
   * \return Destination Ipv4Address of the route
   */
  Ipv4Address GetDestination (void) const;


  /**
   * \param gw Gateway (next hop) Ipv4Address
   */
  void SetDetour (Ipv4Address gw);
  /**
   * \return Ipv4Address of the gateway (next hop) 
   */
  Ipv4Address GetDetour (void) const;

  /**
   * Equivalent in Linux to dst_entry.dev
   *
   * \param outputDevice pointer to NetDevice for outgoing packets
   */
  void SetOutputDevice (Ptr<NetDevice> outputDevice);
  /**
   * \return pointer to NetDevice for outgoing packets
   */
  Ptr<NetDevice> GetOutputDevice (void) const;


private:
  Ipv4Address m_destAddress;
  Ipv4Address m_detourAddress;
  Ptr<NetDevice> m_detourDevice;

#ifdef NOTYET
  uint32_t m_inputIfIndex;
#endif
};


} // namespace ns3

#endif /* INRPP_ROUTE_H */
