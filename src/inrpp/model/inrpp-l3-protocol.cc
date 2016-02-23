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

#include "inrpp-l3-protocol.h"
#include <vector>
#include <algorithm>
#include "inrpp-header.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/object-vector.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/ppp-header.h"

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
//#include <random>
#include <iostream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppL3Protocol");

const uint16_t InrppL3Protocol::PROT_NUMBER = 0x00FD;

NS_OBJECT_ENSURE_REGISTERED (InrppL3Protocol);



TypeId 
InrppL3Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppL3Protocol")
    .SetParent<Ipv4L3Protocol> ()
    .AddConstructor<InrppL3Protocol> ()
    .SetGroupName ("Inrpp")
	.AddAttribute ("NumSlot",
					   "The size of the queue for packets pending an arp reply.",
					   UintegerValue (100),
					   MakeUintegerAccessor (&InrppL3Protocol::m_numSlot),
					   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("PacketSize",
					   "The size of the queue for packets pending an arp reply.",
					   UintegerValue (1500),
					   MakeUintegerAccessor (&InrppL3Protocol::m_packetSize),
					   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

InrppL3Protocol::InrppL3Protocol ():
//m_cacheFull(false),
m_mustCache(false)//,
//m_initCache(true)
//m_back(false)
//m_cacheFlag(0)
{
  NS_LOG_FUNCTION (this);

  m_cache = CreateObject<InrppCache> ();
  m_cache->SetHighThCallback (MakeCallback (&InrppL3Protocol::HighTh,this));
  m_cache->SetLowThCallback (MakeCallback (&InrppL3Protocol::LowTh,this));
  //m_flagEvent = Simulator::Schedule(Seconds(0.1),&InrppL3Protocol::ChangeFlag,this);

}

InrppL3Protocol::~InrppL3Protocol ()
{
  NS_LOG_FUNCTION (this);

}


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
  NS_LOG_LOGIC ("Add interface " << interface);

  interface->SetNode (m_node);
  interface->SetDevice (device);
  interface->SetForwarding (m_ipForward);
  interface->SetCache(m_cache);
  interface->SetRate(device->GetObject<PointToPointNetDevice>()->GetDataRate());
  interface->SetInrppL3Protocol(this);
  interface->SetNumSlot(m_numSlot);
  //interface->SetWeights(m_weights);
  return Ipv4L3Protocol::AddIpv4Interface (interface);
}


void
//InrppL3Protocol::SendInrppInfo (Ptr<InrppInterface> sourceIface, Ptr<NetDevice> destDevice, Ipv4Address infoAddress)
InrppL3Protocol::SendInrppInfo (Ptr<InrppInterface> sourceIface, Ptr<InrppInterface> destIface, Ipv4Address infoAddress)
{
  //NS_LOG_FUNCTION (this<<destDevice<<sourceIface<<sourceIface->GetResidual()<<sourceIface->GetAddress(0).GetLocal());

  NS_LOG_FUNCTION (this);
  Ptr<Packet> packet = Create<Packet> ();
  //Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  //Ipv4InterfaceAddress address = ipv4->GetAddress(ipv4->GetInterfaceForDevice (destDevice),0);
  //NS_LOG_LOGIC(address);
  Ptr<NetDevice> destDevice = destIface->GetDevice();
  Ipv4InterfaceAddress address = destIface->GetAddress(0);
  InrppHeader inrpp;

  NS_LOG_LOGIC("Local ad: " << address.GetLocal() << " info ad: " << infoAddress << " " << sourceIface->GetAddress(0).GetLocal() << " residual: " << sourceIface->GetResidual(infoAddress));
  inrpp.SetInrpp (destDevice->GetAddress (), address.GetLocal(), destDevice->GetBroadcast(), address.GetBroadcast(),infoAddress,sourceIface->GetResidual(infoAddress));
  packet->AddHeader(inrpp);
  destDevice->Send (packet, destDevice->GetBroadcast (), InrppL3Protocol::PROT_NUMBER);

  Simulator::Schedule (Seconds (0.01),&InrppL3Protocol::SendInrppInfo,this,sourceIface,destIface,infoAddress);
  //Simulator::Schedule (Seconds (0.01),&InrppL3Protocol::SendInrppInfo,this,sourceIface,destDevice,infoAddress);

}

void
InrppL3Protocol::InrppReceive (Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                        const Address &to, NetDevice::PacketType packetType)
{
	NS_LOG_FUNCTION(this<<device<<p<<protocol<<from<<to<<packetType);
	Ptr<Packet> packet = p->Copy ();
	InrppHeader inrpp;
	packet->RemoveHeader (inrpp);

	Ptr<InrppInterface> inrppInface = GetInterface (GetInterfaceForDevice (device))->GetObject<InrppInterface>();
	NS_LOG_LOGIC("Inrpp header detour " << inrppInface->GetAddress(0).GetLocal() << " " << inrpp.GetAddress() << " " << inrpp.GetResidual());

	inrppInface->UpdateResidual(inrpp.GetAddress(),inrpp.GetResidual());
}

void
InrppL3Protocol::IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
	NS_LOG_FUNCTION (this << rtentry << p << p->GetSize() << header);
	NS_LOG_LOGIC ("Forwarding logic for node: " << m_node->GetId ());
	NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()<< " " << rtentry->GetSource() << " " << rtentry->GetOutputDevice());

	Ipv4Header ipHeader = header;

	// Forwarding
	int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	NS_ASSERT (interface >= 0);
	Ptr<InrppInterface> outInterface = GetInterface (interface)->GetObject<InrppInterface>();
	outInterface->CalculateFlow(p);

	NS_LOG_LOGIC ("Interface " << interface << " " << outInterface);
	Ptr<Packet> packet = p->Copy();

	TcpHeader tcpHeader;

	if(packet->RemoveHeader(tcpHeader))
	{
		NS_LOG_FUNCTION(tcpHeader);

		InrppTag tag;
		if(packet->PeekPacketTag(tag))
		{
			NS_LOG_LOGIC("Detoured traffic from " << tag.GetAddress() << " through " << outInterface->GetAddress(0).GetLocal());
			outInterface->CalculateDetour(tag.GetAddress(),p);
		}
//		if(m_mustCache)outInterface->SetInitCache(false);
		NS_LOG_LOGIC("Detour State Cache " << outInterface->GetState() << " " << m_mustCache << " " << outInterface->GetInitCache() << " " << m_cache->GetSize());
		/*if(tcpHeader.GetFlags () & TcpHeader::SYN)
		{
			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			NS_LOG_LOGIC("Send packet "<< rtentry->GetDestination());
			SendData(rtentry,packet);
		} else*/
		//if (outInterface->GetState()!=NO_DETOUR&&m_mustCache)
		//if (outInterface->GetState()!=NO_DETOUR)
		//{
		if(outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DETOUR||outInterface->GetState()==DISABLE_BACK||
				(outInterface->GetState()==UP_BACKPRESSURE&&m_mustCache)||(outInterface->GetState()==PROP_BACKPRESSURE&&m_mustCache))
		{
			//if(outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DISABLE_BACK)
		//	{
			//	uint8_t rand;
				//std::random_device rd;
		//		st/d::mt19937 gen(rd());
		//		std::uniform_int_distribution<> dis(0, m_numSlot-1);
		//		if(AddOptionInrpp(tcpHeader,dis(gen),outInterface->GetNonce()))
				if(AddOptionInrpp(tcpHeader,outInterface->GetNonce()))
				{
					uint32_t size = ipHeader.GetPayloadSize();
					ipHeader.SetPayloadSize(size+12);
				}
		//	}

			uint32_t flag = 0;
			if(tcpHeader.HasOption(TcpOption::INRPP))
			{
				Ptr<TcpOptionInrpp> inrpp = DynamicCast<TcpOptionInrpp> (tcpHeader.GetOption (TcpOption::INRPP));
				flag = inrpp->GetFlag();
				NS_LOG_LOGIC("Insert at slot " << this << " " << flag << " " << inrpp->GetFlag() << " " << inrpp->GetNonce()<< " " << tcpHeader.GetDestinationPort() << " " << m_cache->GetSize(outInterface,flag) << " " << m_cache->GetSize());
			}

			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			NS_LOG_LOGIC("Insert at slot " << flag << " " << m_numSlot);
			if(!m_cache->Insert(outInterface,flag,rtentry,packet)){
			//if(!m_cache->Insert(outInterface,m_cacheFlag,rtentry,packet)){
				NS_LOG_LOGIC("CACHE FULL");
			} else {
				//NS_LOG_LOGIC("Cached at " << outInterface->GetRate());
				outInterface->SendPacket();
			}

		} else
		{
			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			NS_LOG_LOGIC("Send packet "<< rtentry->GetDestination());
			SendData(rtentry,packet);
		}
	} else {
		packet->AddHeader(ipHeader);
		NS_LOG_LOGIC("Send packet");
		SendData(rtentry,packet);
	}

}

void
InrppL3Protocol::SendData (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p)
{
	  //m_route = rtentry;
	  Ptr<Packet> packet = p->Copy ();
	  Ipv4Header ipHeader;
	  packet->RemoveHeader(ipHeader);

	  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	 // NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()
	//		  << " " << rtentry->GetSource() << " " << interface
	//		  << " " << p->GetSize() << " " << Simulator::Now().GetSeconds()-t.GetSeconds());
	  NS_LOG_FUNCTION (this << interface << p->GetSize());
	  if(interface==1)t = Simulator::Now();

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
		m_dropTrace (ipHeader, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4> (), interface);
		return;
	  }
	//  NS_LOG_LOGIC("Routelist insert " << packet << " " << m_routeList.size());

	  std::map <Ptr<const Packet>, Ptr<Ipv4Route> >::iterator it =  m_routeList.find(packet);
	  if(it==m_routeList.end())m_routeList.insert(std::make_pair(packet,rtentry));

	  m_unicastForwardTrace (ipHeader, packet, interface);
	  SendRealOut (rtentry, packet, ipHeader);
}

void
InrppL3Protocol::Receive ( Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                          const Address &to, NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION (this << device << p  << p->GetSize() << protocol << from << to << packetType);

 // NS_LOG_LOGIC ("Packet from " << from << " received on node " <<
 //               m_node->GetId ());

  uint32_t interface = 0;
  Ptr<Packet> packet = p->Copy ();

  int32_t ifaceNumber = GetInterfaceForDevice (device);
  Ptr<InrppInterface> iface = GetInterface (ifaceNumber)->GetObject<InrppInterface>();

  // read the tag from the packet copy
  InrppTag tagCopy;
  if(p->PeekPacketTag (tagCopy))
  {
	  //NS_LOG_LOGIC("Remove tag " << tagCopy.GetAddress());
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

	 TcpHeader tcpHeader;
	 if(packet->RemoveHeader(tcpHeader))
	 {

		 NS_LOG_LOGIC ("InrppL3Protocol " << this
		                                 << " receiving seq " << tcpHeader.GetSequenceNumber ()
		                                 << " ack " << tcpHeader.GetAckNumber ()
		                                 << " flags "<< std::hex << (int)tcpHeader.GetFlags () << std::dec
		                                 << " data size " << packet->GetSize ());

		ProcessInrppOption(tcpHeader,iface);
		if(iface->GetState()==BACKPRESSURE)
		{
			if(AddOptionInrppBack(tcpHeader,1,iface->GetNonce()))
			{
				uint32_t size = ipHeader.GetPayloadSize();
				ipHeader.SetPayloadSize(size+12);
			}
		} else if (iface->GetState()==DISABLE_BACK)
		{
	       //NS_LOG_LOGIC("Disable backpressure enabled");
			if(AddOptionInrppBack(tcpHeader,2,iface->GetNonce()))
			{
				uint32_t size = ipHeader.GetPayloadSize();
				ipHeader.SetPayloadSize(size+12);
			}
		}

		packet->AddHeader(tcpHeader);

	 }

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
	NS_LOG_LOGIC("NetDev " <<  m_node->GetId() << " " << m_node->GetObject<Ipv4> ()->GetAddress(m_node->GetObject<Ipv4> ()->GetInterfaceForDevice (netdevice),0).GetLocal());
	NS_LOG_LOGIC("InrppRoute " <<  route->GetDestination() << " " << route->GetDetour() << " " << m_node->GetObject<Ipv4> ()->GetAddress(m_node->GetObject<Ipv4> ()->GetInterfaceForDevice (route->GetOutputDevice()),0).GetLocal());

	/*int32_t interface = 0;
    for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin ();
	   i != m_interfaces.end ();
	   i++, interface++)
	{

		  NS_LOG_LOGIC("Iface " << GetInterface(interface)->GetAddress(0).GetLocal()<< " " <<(*i)->GetDevice ());

	}*/

	Ptr<InrppInterface> iface = GetInterface(GetInterfaceForDevice (netdevice))->GetObject<InrppInterface>();

	/*interface = 0;
    for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin ();
	   i != m_interfaces.end ();
	   i++, interface++)
	{

		  NS_LOG_LOGIC("Iface2 " << GetInterface(interface)->GetAddress(0).GetLocal() << " " << (*i)->GetDevice ());

	}*/

	Ptr<InrppInterface> iface2 = GetInterface(GetInterfaceForDevice (route->GetOutputDevice()))->GetObject<InrppInterface>();
	if(iface2 == NULL || iface == NULL)
	{
		if(!iface)
			NS_LOG_LOGIC("iface is NULL in SetDetourRoute()");
		if(!iface2)
			NS_LOG_LOGIC("iface2 is NULL in SetDetourRoute()");
		return;
	}
	NS_LOG_LOGIC("Set detour route to " << route->GetDestination() << " of " << iface << " " << iface->GetAddress(0).GetLocal()<< " via " << iface2 << " " << iface2->GetAddress(0).GetLocal());

	iface2->SetDetouredIface(iface,route->GetDestination());
	iface2->SetDetour(route);

}

void
InrppL3Protocol::SendDetourInfo(uint32_t sourceIface, uint32_t destIface, Ipv4Address address)
{
	//int32_t interface = GetInterfaceForDevice (devSource);
	//NS_ASSERT (interface >= 0);
	NS_LOG_LOGIC("Interface "<<  GetInterface (sourceIface) << " destiface " << GetInterface (destIface));

	//Ptr<InrppInterface> inrppInface = GetInterface (sourceIface)->GetObject<InrppInterface>();
	Ptr<InrppInterface> sourceInterface = GetInterface (sourceIface)->GetObject<InrppInterface>();
	Ptr<InrppInterface> destInterface = GetInterface (destIface)->GetObject<InrppInterface>();

	sourceInterface->OneMoreDetour(address);
	NS_LOG_LOGIC("Interface "<< sourceInterface->GetAddress(0).GetLocal() << " " << destInterface->GetAddress(0).GetLocal() << " " << address);
	Simulator::Schedule (Seconds (0.01),&InrppL3Protocol::SendInrppInfo,this,sourceInterface,destInterface,address);
	//Simulator::Schedule (Seconds (0.01),&InrppL3Protocol::SendInrppInfo,this,inrppInface,m_node->GetDevice(destIface),address);

}

void
InrppL3Protocol::HighTh( uint32_t packets)
{
	NS_LOG_FUNCTION(this<<packets);

	for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	{
	  Ptr<Ipv4Interface> iface = *i;
	  Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
	  if(!loop)
	  {
		  Ptr<InrppInterface> iface2 = iface->GetObject<InrppInterface>();
		  if(iface2->GetState()==DETOUR)
			  iface2->SetState(BACKPRESSURE);
		  else if(iface2->GetState()==UP_BACKPRESSURE)
		  {
			  iface2->SetState(PROP_BACKPRESSURE);
		  } else if(iface2->GetState()==DISABLE_BACK)
		  {
			  iface2->SetState(BACKPRESSURE);
		  }
	  }
	}
}

void
InrppL3Protocol::LowTh(uint32_t packets)
{
	NS_LOG_FUNCTION(this<<packets);

	for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	{
	  Ptr<Ipv4Interface> iface = *i;
	  Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
	  if(!loop)
	  {
		  Ptr<InrppInterface> iface2 = iface->GetObject<InrppInterface>();
		  if(iface2->GetState()==PROP_BACKPRESSURE)
			  iface2->SetState(UP_BACKPRESSURE);
		  if(iface2->GetState()==BACKPRESSURE)
		  	  iface2->SetState(DISABLE_BACK);

	  }
	}

}

bool
InrppL3Protocol::AddOptionInrpp (TcpHeader& header, uint32_t nonce)
{
  NS_LOG_FUNCTION (this << header << header.GetDestinationPort()%m_numSlot);

  bool add = false;
  if (!header.HasOption (TcpOption::INRPP))
  {
	  uint32_t flag = header.GetDestinationPort()%m_numSlot;
	  NS_LOG_LOGIC("AddHeader " << flag);
	  Ptr<TcpOptionInrpp> option = CreateObject<TcpOptionInrpp> ();
	  option->SetFlag(flag);
	  //option->SetFlag(m_cacheFlag);
	  option->SetNonce (nonce);
	  header.AppendOption (option);
	  //NS_LOG_FUNCTION("Create option inrpp " <<header);
	  NS_LOG_FUNCTION (this << option->GetFlag());

	  add = true;
  }


 // NS_LOG_FUNCTION (this << header);
  return add;

}

bool
InrppL3Protocol::AddOptionInrppBack (TcpHeader& header,uint8_t flag,uint32_t nonce)
{
  NS_LOG_FUNCTION (this << header << flag);

  bool add = false;

  if(header.HasOption (TcpOption::INRPP))
  {
	  Ptr<TcpOptionInrpp> option = DynamicCast<TcpOptionInrpp> (header.GetOption (TcpOption::INRPP));
	  header.ClearOption();
	  header.AppendOption(option);
  }
  if (!header.HasOption (TcpOption::INRPP_BACK))
  {
	  Ptr<TcpOptionInrppBack> option = CreateObject<TcpOptionInrppBack> ();
	  option->SetFlag(flag);
	  option->SetNonce (nonce);
	  header.AppendOption (option);
	  add = true;
  }

  return add;

}

void
InrppL3Protocol::ProcessInrppOption(TcpHeader& tcpHeader,Ptr<InrppInterface> iface)
{
	NS_LOG_FUNCTION (this << tcpHeader << tcpHeader.GetLength());

	if (tcpHeader.HasOption (TcpOption::INRPP_BACK))
	{

	  Ptr<TcpOptionInrppBack> ts = DynamicCast<TcpOptionInrppBack> (tcpHeader.GetOption (TcpOption::INRPP_BACK));

	  NS_LOG_INFO (m_node->GetId () << " Got InrppBack flag=" <<
				   (uint32_t) ts->GetFlag()<< " and nonce="     << ts->GetNonce () << " cache="<<m_cache->GetSize());

	  if(iface->GetState()==PROP_BACKPRESSURE||iface->GetState()==UP_BACKPRESSURE)
		  iface->CalculatePacing(m_packetSize);
	  if(ts->GetFlag()==1){
		  if(iface->GetState()==NO_DETOUR&&m_cache->GetSize()<m_cache->GetThreshold())
		  {
			  iface->SetState(UP_BACKPRESSURE);
		  } else if(iface->GetState()==NO_DETOUR&&m_cache->GetSize()>=m_cache->GetThreshold())
		  {
			  iface->SetState(PROP_BACKPRESSURE);
		  }
		  if(iface->GetState()==UP_BACKPRESSURE)
		  {

			  ts->SetFlag(0);
			  if(ts->GetNonce()!=0)
			  {
			  m_noncesList.push_back(ts->GetNonce());
			  }
		  }
		  if(iface->GetState()==PROP_BACKPRESSURE)
		  {
			  NS_LOG_LOGIC("propagate backpressure");
			  if(ts->GetNonce()!=0)
			  {
			  m_noncesList.push_back(ts->GetNonce());
			  }

		  }

	  }

	  if(ts->GetFlag()==2)
	  {
		  if(iface->GetState()==PROP_BACKPRESSURE||iface->GetState()==UP_BACKPRESSURE)
		  {
			  m_noncesList.erase(std::remove(m_noncesList.begin(), m_noncesList.end(), ts->GetNonce()), m_noncesList.end());
			  iface->SetState(NO_DETOUR);
		  }

	  }



	/*  if(ts->GetFlag()==3)
	  {

		  if(std::find(m_noncesList.begin(), m_noncesList.end(), ts->GetNonce())!=m_noncesList.end())
		  {
			  m_mustCache = true;

		  } else {
			  m_mustCache = false;
		  }

		  if(IsNonceFromInterface(ts->GetNonce()))
		  {
			  m_mustCache=true;
		  }

	  }*/
	} /*else {
		  m_mustCache = false;
	}*/
	if (tcpHeader.HasOption (TcpOption::INRPP))
	{
		  Ptr<TcpOptionInrpp> ts = DynamicCast<TcpOptionInrpp> (tcpHeader.GetOption (TcpOption::INRPP));
		  if(std::find(m_noncesList.begin(), m_noncesList.end(), ts->GetNonce())!=m_noncesList.end())
		  {
			  m_mustCache=true;
		  } else {
			  m_mustCache=false;
		  }
	} else {
		m_mustCache=false;
	}


}

Ptr<InrppInterface>
InrppL3Protocol::FindDetourIface(Ptr<InrppInterface> interface)
{
	NS_LOG_FUNCTION(this<< interface);
	Ptr<InrppInterface> detouredIface;
	  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
		  {
			  Ptr<Ipv4Interface> iface = *i;
			  Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
			  if(!loop)
			  {
				  Ptr<InrppInterface> iface2 = iface->GetObject<InrppInterface>();
				  if(iface2->GetDetouredIface() == interface)
					  detouredIface = iface2;
			  }

		  }
	  return detouredIface;
}

bool
InrppL3Protocol::IsNonceFromInterface(uint32_t nonce)
{
	bool found = false;

      for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	  {
		  Ptr<Ipv4Interface> iface = *i;
		  Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
		  if(!loop)
		  {
			  Ptr<InrppInterface> iface2 = iface->GetObject<InrppInterface>();
			  if(iface2->GetNonce() == nonce)
				  found = true;
		  }

	  }

	return found;
}

Ptr<InrppCache>
InrppL3Protocol::GetCache()
{
	return m_cache;
}

void
InrppL3Protocol::LostPacket(Ptr<const Packet> packet, Ptr<InrppInterface> iface,Ptr<NetDevice> device)
{
	NS_LOG_FUNCTION(this<<packet->GetSize()<<iface<<device<<m_routeList.size());
	Ptr<Packet> p = packet->Copy();
	PppHeader ppp;
	p->RemoveHeader (ppp);
	TcpHeader tcpHeader;
    Ipv4Header ipHeader;
    p->RemoveHeader(ipHeader);

    Ptr<InrppTailQueue> q = device->GetObject <PointToPointNetDevice>()->GetQueue()->GetObject<InrppTailQueue>();
    //NS_LOG_LOGIC("Detour State Cache " << iface->GetState() << " " << m_mustCache << " " << iface->GetInitCache() << " " << m_cache->GetSize() << " " << q->GetNBytes());

    std::map <Ptr<const Packet>, Ptr<Ipv4Route> >::iterator it = m_routeList.find(packet);
    //if(it==m_routeList.end())NS_LOG_LOGIC("ROUTE NOT FOUND");
    NS_ASSERT_MSG(it!=m_routeList.end(),"Route not found");

    if(p->RemoveHeader(tcpHeader))
	{
    	    //NS_LOG_LOGIC(tcpHeader);
			//if(AddOptionInrpp(tcpHeader,3,iface->GetNonce()))
			//{
			//	uint32_t size = ipHeader.GetPayloadSize();
			//	ipHeader.SetPayloadSize(size+12);
			//}

			ipHeader.SetTtl (ipHeader.GetTtl () + 1);
			p->AddHeader(tcpHeader);
			p->AddHeader(ipHeader);

			if(!m_cache->InsertFirst(iface,it->second,p)){
				NS_LOG_LOGIC("CACHE FULL");
			}

	 }

 	m_routeList.erase(packet);


}

void
InrppL3Protocol::Discard(Ptr<const Packet> packet)
{
	NS_LOG_FUNCTION(this<<packet<<m_routeList.size());
	m_routeList.erase(packet);
}

void
InrppL3Protocol::NotifyState(Ptr<InrppInterface> iface, uint32_t state)
{
	if(!m_cb.IsNull())m_cb(iface,state);
}

void
InrppL3Protocol::SetCallback(Callback<void,Ptr<InrppInterface>,uint32_t > cb)
{
	m_cb = cb;
}

void
InrppL3Protocol::ChangeFlag()
{
	NS_LOG_FUNCTION(this);
	m_cacheFlag++;
	m_cacheFlag=m_cacheFlag%m_numSlot;
	m_flagEvent = Simulator::Schedule(Seconds(0.1),&InrppL3Protocol::ChangeFlag,this);


	/*for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	{
		Ptr<Ipv4Interface> iface = *i;
		Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
			if(!loop)
			{

				if(iface->GetObject<InrppInterface>()->GetState()!=NO_DETOUR)
				{
					for(uint32_t i = 0; i<m_numSlot;++i)
					{
						NS_LOG_LOGIC("Cache size slot " << i << ": " << m_cache->GetSize(iface->GetObject<InrppInterface>(),i) << " " << m_cache->GetSize());
						uint32_t weight = round(m_cache->GetSize(iface->GetObject<InrppInterface>(),i)/1494);
						NS_LOG_LOGIC("Cache size slot " << i << ": " << weight << " " << round(weight*100/(m_cache->GetSize()/1494)));
						weight = round(weight*100/(m_cache->GetSize()/1494));
						m_weights.insert(std::make_pair(i,weight));
						iface->GetObject<InrppInterface>()->SetWeights(m_weights);
					}

				}
			}
		}*/



}
} // namespace ns3
