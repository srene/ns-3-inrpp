/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Faker Moatamri <faker.moatamri@sophia.inria.fr>
 */

#include "ns3/inrpp-stack-helper.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/names.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/config.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/core-config.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-extension.h"
#include "ns3/ipv6-extension-demux.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/global-router-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppStackHelper");


InrppStackHelper::InrppStackHelper ()
  : InternetStackHelper()

{
}

InrppStackHelper::~InrppStackHelper ()
{
	//  delete m_routing;
	 // delete m_routingv6;
}

void 
InrppStackHelper::Install (NodeContainer c) const
{
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Install (*i);
    }
}

void 
InrppStackHelper::InstallAll (void) const
{
  Install (NodeContainer::GetGlobal ());
}


void
InrppStackHelper::Install (Ptr<Node> node) const
{
NS_LOG_INFO("Install");
  if (m_ipv4Enabled)
    {
	  NS_LOG_INFO("Install ipv4");

      if (node->GetObject<Ipv4> () != 0)
        {
          NS_FATAL_ERROR ("InrppStackHelper::Install (): Aggregating "
                          "an InternetStack to a node with an existing Ipv4 object");
          return;
        }

      CreateAndAggregateObjectFromTypeId (node, "ns3::ArpL3Protocol");
      CreateAndAggregateObjectFromTypeId (node, "ns3::InrppL3Protocol");
      CreateAndAggregateObjectFromTypeId (node, "ns3::Icmpv4L4Protocol");
      if (m_ipv4ArpJitterEnabled == false)
        {
          Ptr<ArpL3Protocol> arp = node->GetObject<ArpL3Protocol> ();
          NS_ASSERT (arp);
          arp->SetAttribute ("RequestJitter", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
        }
      // Set routing
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<Ipv4RoutingProtocol> ipv4Routing = m_routing->Create (node);
      ipv4->SetRoutingProtocol (ipv4Routing);
    }

  if (m_ipv6Enabled)
    {
	  NS_LOG_INFO("Install ipv6");

      /* IPv6 stack */
      if (node->GetObject<Ipv6> () != 0)
        {
          NS_FATAL_ERROR ("InrppStackHelper::Install (): Aggregating "
                          "an InternetStack to a node with an existing Ipv6 object");
          return;
        }

      CreateAndAggregateObjectFromTypeId (node, "ns3::Ipv6L3Protocol");
      CreateAndAggregateObjectFromTypeId (node, "ns3::Icmpv6L4Protocol");
      if (m_ipv6NsRsJitterEnabled == false)
        {
          Ptr<Icmpv6L4Protocol> icmpv6l4 = node->GetObject<Icmpv6L4Protocol> ();
          NS_ASSERT (icmpv6l4);
          icmpv6l4->SetAttribute ("SolicitationJitter", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
        }
      // Set routing
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      Ptr<Ipv6RoutingProtocol> ipv6Routing = m_routingv6->Create (node);
      ipv6->SetRoutingProtocol (ipv6Routing);

      /* register IPv6 extensions and options */
      ipv6->RegisterExtensions ();
      ipv6->RegisterOptions ();
    }

  if (m_ipv4Enabled || m_ipv6Enabled)
    {
      CreateAndAggregateObjectFromTypeId (node, "ns3::UdpL4Protocol");
      node->AggregateObject (m_tcpFactory.Create<Object> ());
      Ptr<PacketSocketFactory> factory = CreateObject<PacketSocketFactory> ();
      node->AggregateObject (factory);
    }
}

void
InrppStackHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  Install (node);
}

/**
 * \brief Sync function for IPv4 packet - Pcap output
 * \param p smart pointer to the packet
 * \param ipv4 smart pointer to the node's IPv4 stack
 * \param interface incoming interface
 */


} // namespace ns3
