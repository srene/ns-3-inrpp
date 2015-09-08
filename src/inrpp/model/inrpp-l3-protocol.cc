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
#include "inrpp-l3-protocol.h"

#include "inrpp-header.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/object-vector.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/pointer.h"
#include "ns3/string.h"

#include "ns3/loopback-net-device.h"
#include "ns3/ipv4-interface.h"
#include "ns3/inrpp-interface.h"

#include "ns3/inrpp-tail-queue.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/icmpv4-l4-protocol.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/arp-header.h"
#include "ns3/ipv4-raw-socket-impl.h"
#include "inrpp-tag.h"
#include "inrpp-backp-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppL3Protocol");

//const uint8_t InrppL3Protocol::PROT_NUMBER = 25;
const uint16_t InrppL3Protocol::PROT_NUMBER = 0x00FD;

NS_OBJECT_ENSURE_REGISTERED (InrppL3Protocol);



TypeId 
InrppL3Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppL3Protocol")
    .SetParent<Ipv4L3Protocol> ()
    .AddConstructor<InrppL3Protocol> ()
    .SetGroupName ("Internet")
    /*.AddTraceSource ("Drop",
                     "Packet dropped because not enough room "
                     "in pending queue for a specific cache entry.",
                     MakeTraceSourceAccessor (&InrppL3Protocol::m_dropTrace),
                     "ns3::Packet::TracedCallback")*/
  ;
  return tid;
}

InrppL3Protocol::InrppL3Protocol ()
{
  NS_LOG_FUNCTION (this);

  Ptr<InrppCache> m_cache = CreateObject<InrppCache> ();

}

InrppL3Protocol::~InrppL3Protocol ()
{
  NS_LOG_FUNCTION (this);

}




/*void
InrppL3Protocol::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_node = node;
  SetNetDevices();
}*/

/*
 * This method is called by AddAgregate and completes the aggregation
 * by setting the node in the ipv4 stack
 */
/*void
InrppL3Protocol::NotifyNewAggregate ()
{
  NS_LOG_FUNCTION (this);
  if (m_node == 0)
    {
      Ptr<Node>node = this->GetObject<Node> ();
      //verify that it's a valid node and that
      //the node was not set before
      if (node != 0)
        {
          this->SetNode (node);
        }
    }
  Object::NotifyNewAggregate ();
}*/

void 
InrppL3Protocol::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_node = 0;
  Object::DoDispose ();
}


uint32_t
InrppL3Protocol::AddInterface (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);

  Ptr<Node> node = GetObject<Node> ();
  node->RegisterProtocolHandler (MakeCallback (&InrppL3Protocol::Receive, this),
                                 Ipv4L3Protocol::PROT_NUMBER, device);
  node->RegisterProtocolHandler (MakeCallback (&ArpL3Protocol::Receive, PeekPointer (GetObject<ArpL3Protocol> ())),
                                 ArpL3Protocol::PROT_NUMBER, device);

  node->RegisterProtocolHandler (MakeCallback (&InrppL3Protocol::InrppReceive, this),
                                 InrppL3Protocol::PROT_NUMBER, device);

  Ptr<InrppInterface> interface = CreateObject<InrppInterface> ();
  interface->SetNode (m_node);
  interface->SetDevice (device);
  interface->SetForwarding (m_ipForward);
  interface->SetCache(m_cache);

  return Ipv4L3Protocol::AddIpv4Interface (interface);
  //return Ipv4L3Protocol::AddInterface(device);
}

/*void
InrppL3Protocol::Receive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                        const Address &to, NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION (this << device << p->GetSize () << protocol << from << to << packetType);

  Ptr<Ipv4Interface> ipv4Interface;
  uint32_t interface = 0;

   for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin ();
        i != m_interfaces.end ();
        i++, interface++)
     {
       ipv4Interface = *i;

       NS_LOG_LOGIC("Interface " << interface << " " <<*i);
     }

  Ipv4L3Protocol::Receive(device,p,protocol,from,to,packetType);



}*/


void
InrppL3Protocol::SendInrppInfo (Ptr<InrppInterface> iface, Ptr<NetDevice> device, Ipv4Address infoAddress)
{
  NS_LOG_FUNCTION (this<<device<<iface->GetResidual());

  Ptr<Packet> packet = Create<Packet> ();
 // InrppHeader inrpp;
 // packet->AddHeader(inrpp);
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  Ipv4InterfaceAddress address = ipv4->GetAddress(ipv4->GetInterfaceForDevice (device),0);
  NS_LOG_LOGIC(address);
//  Ipv4Header header;
//  header.SetDestination (address.GetBroadcast());
//  header.SetSource(address.GetLocal());
//  header.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
//  ArpHeader arp;
//  arp.SetReply (device->GetAddress (), address.GetLocal(), device->GetBroadcast(), address.GetBroadcast());
  InrppHeader inrpp;
  inrpp.SetInrpp (device->GetAddress (), address.GetLocal(), device->GetBroadcast(), address.GetBroadcast(),infoAddress,iface->GetResidual());
  packet->AddHeader(inrpp);
  device->Send (packet, device->GetBroadcast (), InrppL3Protocol::PROT_NUMBER);

  Simulator::Schedule (Seconds (1.0),&InrppL3Protocol::SendInrppInfo,this,iface,device,infoAddress);

}

void
InrppL3Protocol::InrppReceive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                        const Address &to, NetDevice::PacketType packetType)
{
	NS_LOG_FUNCTION(this<<device<<p<<protocol<<from<<to<<packetType);
	Ptr<Packet> packet = p->Copy ();
	InrppHeader inrpp;
	packet->RemoveHeader (inrpp);

	NS_LOG_LOGIC("Inrpp header detour " << inrpp.GetAddress() << " " << inrpp.GetResidual());

	m_residualList.insert(std::pair<Ipv4Address, uint32_t>(inrpp.GetAddress(),inrpp.GetResidual()));
}

void
InrppL3Protocol::IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
	  NS_LOG_FUNCTION (this << rtentry << p << header);
	  NS_LOG_LOGIC ("Forwarding logic for node: " << m_node->GetId ());
	  NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()<< " " << rtentry->GetSource() << " " << rtentry->GetOutputDevice());
	  // Forwarding

	  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	  DataRate bps = rtentry->GetOutputDevice ()->GetObject<PointToPointNetDevice>()->GetDataRate();
	  NS_ASSERT (interface >= 0);
	  Ptr<InrppInterface> outInterface = GetInterface (interface)->GetObject<InrppInterface>();
	  Ptr<InrppRoute> route = outInterface->GetDetour();
	  outInterface->CalculateFlow(p);

	  int deltaRate = outInterface->GetFlow() - bps.GetBitRate();

	  NS_LOG_LOGIC("Flow " << outInterface << " " << outInterface->GetFlow() << " BW " << outInterface->GetBW() << " DeltaRate " << deltaRate << " " << outInterface->GetState());
	//  switch(outInterface->GetState())
	//  {
//	  case DETOUR:
	  if(outInterface->GetState()!=N0_DETOUR)
	  {
		  m_cache->Insert(outInterface,p);
		  if(route){
			  NS_LOG_LOGIC("DETOUR PATH");
			  int32_t iface2 = GetInterfaceForDevice (route->GetOutputDevice ());
			  Ptr<InrppInterface> detourIface2 = GetInterface (iface2)->GetObject<InrppInterface>();
			  NS_LOG_LOGIC("Iface 1 " << outInterface->GetResidual() << " " << detourIface2->GetResidual());

			  std::map<Ipv4Address,uint32_t>::iterator it = m_residualList.find(rtentry->GetGateway());


			  if(it != m_residualList.end())
			  {
				  int32_t iface = GetInterfaceForDevice (route->GetOutputDevice ());
				  Ptr<InrppInterface> detourIface = GetInterface (iface)->GetObject<InrppInterface>();
				  uint32_t residual = std::min(detourIface->GetResidual(),it->second);
				  NS_LOG_LOGIC("Residual capacity " << residual << " deltaRate " << deltaRate);
				  deltaRate=deltaRate-residual;
				  if(residual>0&&outInterface->GetBW()>bps.GetBitRate())
				  {
					  InrppTag tag;
					  tag.SetAddress (rtentry->GetGateway());
					  p->AddPacketTag (tag);
					  rtentry->SetGateway(route->GetDetour());
					  rtentry->SetOutputDevice(route->GetOutputDevice());
				  }

			  }
		  }

		  NS_LOG_LOGIC("DeltaRate " << deltaRate);


		  if(deltaRate>1000)
		  {
			  outInterface->SetState(BACKPRESSURE);
			  outInterface->SetDeltaRate(deltaRate);
		  }

	  }



	/*	  break;
	  case N0_DETOUR:
	  case BACKPRESSURE:
	  case UP_BACKPRESSURE:
	  case UP_AND_PROP_BACKPRESSURE:
	  default:

		  break;

	  }*/

	  Ipv4Header ipHeader = header;
	  Ptr<Packet> packet = p->Copy ();
	 // int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	  NS_LOG_FUNCTION (this << interface);
	  ipHeader.SetTtl (ipHeader.GetTtl () - 1);
	  if (ipHeader.GetTtl () == 0)
	      {
	        // Do not reply to ICMP or to multicast/broadcast IP address
	        if (ipHeader.GetProtocol () != Icmpv4L4Protocol::PROT_NUMBER &&
	            ipHeader.GetDestination ().IsBroadcast () == false &&
	            ipHeader.GetDestination ().IsMulticast () == false)
	          {
	            Ptr<Icmpv4L4Protocol> icmp = GetIcmp ();
	            icmp->SendTimeExceededTtl (ipHeader, packet);
	          }
	        NS_LOG_WARN ("TTL exceeded.  Drop.");
	        m_dropTrace (header, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4> (), interface);
	        return;
	      }
	    m_unicastForwardTrace (ipHeader, packet, interface);
	    SendRealOut (rtentry, packet, ipHeader);
}

void
InrppL3Protocol::Receive ( Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                          const Address &to, NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION (this << device << p << protocol << from << to << packetType);

  NS_LOG_LOGIC ("Packet from " << from << " received on node " <<
                m_node->GetId ());

  uint32_t interface = 0;
  Ptr<Packet> packet = p->Copy ();

  InrppBackpressureTag backpTagCopy;
     if(p->PeekPacketTag (backpTagCopy))
     {
   	  NS_LOG_LOGIC("Backpressure tag " << (uint32_t)backpTagCopy.GetFlag() << " " << backpTagCopy.GetNonce() << " " << backpTagCopy.GetDeltaRate());

     }
  // read the tag from the packet copy
  InrppTag tagCopy;
  if(p->PeekPacketTag (tagCopy))
  {
	  NS_LOG_LOGIC("Remove tag " << tagCopy.GetAddress());
	  Ptr<Ipv4Interface> ipv4Interface;
	  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	  {
		  Ptr<Ipv4Interface> iface = *i;
		  for(uint32_t add = 0;add<iface->GetNAddresses();add++)
		  {
			  if(iface->GetAddress(add).GetLocal()==tagCopy.GetAddress())
			  {
				  packet->RemovePacketTag(tagCopy);
			  	  break;
			  }
		  }
	  }
  }


  int32_t ifaceNumber = GetInterfaceForDevice (device);
  Ptr<InrppInterface> iface = GetInterface (ifaceNumber)->GetObject<InrppInterface>();


  if(iface->GetState()==BACKPRESSURE)
  {
	  NS_LOG_LOGIC("Start backpressure");
	  InrppBackpressureTag tag;
	  tag.SetNonce(iface->GetNonce());
	  tag.SetDeltaRate(iface->GetDeltaRate());
	  tag.SetFlag(1);
	  packet->AddPacketTag (tag);

  }

  Ptr<Ipv4Interface> ipv4Interface;
  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin ();
       i != m_interfaces.end ();
       i++, interface++)
    {
      ipv4Interface = *i;
      if (ipv4Interface->GetDevice () == device)
        {
          if (ipv4Interface->IsUp ())
            {
              m_rxTrace (packet, m_node->GetObject<Ipv4> (), interface);
              break;
            }
          else
            {
              NS_LOG_LOGIC ("Dropping received packet -- interface is down");
              Ipv4Header ipHeader;
              packet->RemoveHeader (ipHeader);
              m_dropTrace (ipHeader, packet, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4> (), interface);
              return;
            }
        }
    }

  Ipv4Header ipHeader;
  if (Node::ChecksumEnabled ())
    {
      ipHeader.EnableChecksum ();
    }
  packet->RemoveHeader (ipHeader);

  // Trim any residual frame padding from underlying devices
  if (ipHeader.GetPayloadSize () < packet->GetSize ())
    {
      packet->RemoveAtEnd (packet->GetSize () - ipHeader.GetPayloadSize ());
    }

  if (!ipHeader.IsChecksumOk ())
    {
      NS_LOG_LOGIC ("Dropping received packet -- checksum not ok");
      m_dropTrace (ipHeader, packet, DROP_BAD_CHECKSUM, m_node->GetObject<Ipv4> (), interface);
      return;
    }

  for (SocketList::iterator i = m_sockets.begin (); i != m_sockets.end (); ++i)
    {
      NS_LOG_LOGIC ("Forwarding to raw socket");
      Ptr<Ipv4RawSocketImpl> socket = *i;
      socket->ForwardUp (packet, ipHeader, ipv4Interface);
    }

  NS_ASSERT_MSG (m_routingProtocol != 0, "Need a routing protocol object to process packets");
  NS_LOG_LOGIC ("Route input");


  if (!m_routingProtocol->RouteInput (packet, ipHeader, device,
                                      MakeCallback (&InrppL3Protocol::IpForward, this),
                                      MakeCallback (&Ipv4L3Protocol::IpMulticastForward, this),
                                      MakeCallback (&Ipv4L3Protocol::LocalDeliver, this),
                                      MakeCallback (&Ipv4L3Protocol::RouteInputError, this)
                                      ))
    {
      NS_LOG_WARN ("No route found for forwarding packet.  Drop.");
      m_dropTrace (ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4> (), interface);
    }
}

void
InrppL3Protocol::SetDetourRoute(Ptr<NetDevice> netdevice, Ptr<InrppRoute> route)
{

	  Ptr<InrppInterface> iface = GetInterface(GetInterfaceForDevice (netdevice))->GetObject<InrppInterface>();
	  iface->SetDetour(route);

}

void
InrppL3Protocol::SendDetourInfo(Ptr<NetDevice> devSource, Ptr<NetDevice> devDestination, Ipv4Address address)
{
	  int32_t interface = GetInterfaceForDevice (devSource);
	  NS_ASSERT (interface >= 0);
	  Ptr<InrppInterface> inrppInface = GetInterface (interface)->GetObject<InrppInterface>();

	  Simulator::Schedule (Seconds (1.0),&InrppL3Protocol::SendInrppInfo,this,inrppInface,devDestination,address);


}


} // namespace ns3
