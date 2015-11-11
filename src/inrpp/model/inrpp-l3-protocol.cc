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
#include "tcp-option-inrpp-back.h"

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
  ;
  return tid;
}

InrppL3Protocol::InrppL3Protocol ():
//m_cacheFull(false),
m_mustCache(false)
//m_initCache(true)
//m_back(false)
{
  NS_LOG_FUNCTION (this);

  m_cache = CreateObject<InrppCache> ();
  m_cache->SetHighThCallback (MakeCallback (&InrppL3Protocol::HighTh,this));
  m_cache->SetLowThCallback (MakeCallback (&InrppL3Protocol::LowTh,this));

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
  return Ipv4L3Protocol::AddIpv4Interface (interface);
}


void
InrppL3Protocol::SendInrppInfo (Ptr<InrppInterface> iface, Ptr<NetDevice> device, Ipv4Address infoAddress)
{
  NS_LOG_FUNCTION (this<<device<<iface->GetResidual());

  Ptr<Packet> packet = Create<Packet> ();
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  Ipv4InterfaceAddress address = ipv4->GetAddress(ipv4->GetInterfaceForDevice (device),0);
  NS_LOG_LOGIC(address);
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

	Ptr<InrppInterface> inrppInface = GetInterface (GetInterfaceForDevice (device))->GetObject<InrppInterface>();

	inrppInface->UpdateResidual(inrpp.GetAddress(),inrpp.GetResidual());
}

void
InrppL3Protocol::IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
	NS_LOG_FUNCTION (this << rtentry << p << header);
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
		if(m_mustCache)outInterface->SetInitCache(false);
		NS_LOG_LOGIC("Detour State Cache " << outInterface->GetState() << " " << m_mustCache << " " << outInterface->GetInitCache() << " " << m_cache->GetSize());
		if(tcpHeader.GetFlags () & TcpHeader::SYN)
		{
			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			NS_LOG_LOGIC("Send packet "<< rtentry->GetDestination());
			SendData(rtentry,packet);
		} else if ((outInterface->GetState()!=NO_DETOUR)&&(m_mustCache||outInterface->GetInitCache()))
		{
			if(outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DETOUR||outInterface->GetState()==DISABLE_BACK)
			//if(outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DISABLE_BACK)
			{
				if(AddOptionInrpp(tcpHeader,3,outInterface->GetNonce()))
				{
					uint32_t size = ipHeader.GetPayloadSize();
					ipHeader.SetPayloadSize(size+12);
				}
			}

			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			if(!m_cache->Insert(outInterface,rtentry,packet)){
				NS_LOG_LOGIC("CACHE FULL");
			} else {
				//NS_LOG_LOGIC("Cached at " << outInterface->GetRate());
				outInterface->SendPacket();
			}
			/*} else if(outInterface->GetState()==DETOUR||outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DISABLE_BACK)
		{
			packet->AddHeader(tcpHeader);
			packet->AddHeader(ipHeader);
			NS_LOG_LOGIC("Push packet");
			outInterface->PushPacket(packet,rtentry);

			if(!m_secondCache->Insert(outInterface,rtentry,packet)){
				NS_LOG_LOGIC("CACHE FULL");
			} else {
				//NS_LOG_LOGIC("Cached at " << outInterface->GetRate());
				outInterface->SendPacket();
			}*/

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

	  /*TcpHeader tcpHeader;
	  if(packet->RemoveHeader(tcpHeader))
	 	 {

	 		 NS_LOG_LOGIC ("InrppL3Protocol " << this
	 		                                 << " receiving seq " << tcpHeader.GetSequenceNumber ()
	 		                                 << " ack " << tcpHeader.GetAckNumber ()
	 		                                 << " flags "<< std::hex << (int)tcpHeader.GetFlags () << std::dec
	 		                                 << " data size " << packet->GetSize ());
	 	 }
	  packet->AddHeader(tcpHeader);*/
	  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	  NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()
			  << " " << rtentry->GetSource() << " " << interface
			  << " " << p->GetSize() << " " << Simulator::Now().GetSeconds()-t.GetSeconds());
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
	  m_routeList.insert(std::make_pair(packet,rtentry));

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

  int32_t ifaceNumber = GetInterfaceForDevice (device);
  Ptr<InrppInterface> iface = GetInterface (ifaceNumber)->GetObject<InrppInterface>();

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
		//m_back = false;
		if(iface->GetState()==BACKPRESSURE)
		{
			//m_back = true;
			//Ptr<InrppInterface> iface2 = FindDetourIface(iface);
			//m_rate = iface->GetRate();
			//if(iface2)m_rate+=iface2->GetResidual();
			//NS_LOG_LOGIC("Backpressure rate " << m_rate << " " << iface->GetFlow());
			if(AddOptionInrpp(tcpHeader,1,iface->GetNonce()))
			{
				uint32_t size = ipHeader.GetPayloadSize();
				ipHeader.SetPayloadSize(size+12);
			}
		} else if (iface->GetState()==DISABLE_BACK)
		{
	       //NS_LOG_LOGIC("Disable backpressure enabled");
			if(AddOptionInrpp(tcpHeader,2,iface->GetNonce()))
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

	Ptr<InrppInterface> iface = GetInterface(GetInterfaceForDevice (netdevice))->GetObject<InrppInterface>();
	Ptr<InrppInterface> iface2 = GetInterface(GetInterfaceForDevice (route->GetOutputDevice()))->GetObject<InrppInterface>();
	iface2->SetDetouredIface(iface,route->GetDestination());
	iface2->SetDetour(route);

}

void
InrppL3Protocol::SendDetourInfo(Ptr<NetDevice> devSource, Ptr<NetDevice> devDestination, Ipv4Address address)
{
	int32_t interface = GetInterfaceForDevice (devSource);
	NS_ASSERT (interface >= 0);
	Ptr<InrppInterface> inrppInface = GetInterface (interface)->GetObject<InrppInterface>();
	Simulator::Schedule (Seconds (1.0),&InrppL3Protocol::SendInrppInfo,this,inrppInface,devDestination,address);

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
          //if(iface2->GetState()==BACKPRESSURE&&iface2->GetDisable())
		  if(iface2->GetState()==BACKPRESSURE)
		  	  iface2->SetState(DISABLE_BACK);

	  }
	}

}

bool
InrppL3Protocol::AddOptionInrpp (TcpHeader& header,uint8_t flag,uint32_t nonce)
{
  NS_LOG_FUNCTION (this << header << flag);

  bool add = false;
  if (!header.HasOption (TcpOption::INRPP_BACK))
  {
	  if(flag==3)
	  {
		  Ptr<TcpOptionInrppBack> option = CreateObject<TcpOptionInrppBack> ();
		  option->SetFlag(flag);
		  option->SetNonce (nonce);
		  header.AppendOption (option);
		  add = true;
	  } else //if (flag==1)
	  {
		  Ptr<TcpOptionInrppBack> option = CreateObject<TcpOptionInrppBack> ();
		  option->SetFlag(flag);
		  option->SetNonce (0);
		  header.AppendOption (option);
		  add = true;
	  }
  } else {
	  Ptr<TcpOptionInrppBack> ts = DynamicCast<TcpOptionInrppBack> (header.GetOption (TcpOption::INRPP_BACK));
	  ts->SetFlag(flag);
	  ts->SetNonce(nonce);
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
		  iface->CalculatePacing(1496);
	  if(ts->GetFlag()==1){
		  //if(ts->GetNonce()!=0)iface->CalculatePacing(1496);

		  if(iface->GetState()==NO_DETOUR&&m_cache->GetSize()<m_cache->GetThreshold())
		  {
			  iface->SetState(UP_BACKPRESSURE);
		  } else if(iface->GetState()==NO_DETOUR)
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

	  if(ts->GetFlag()==3)
	  {
		  NS_LOG_LOGIC("nonce received");

		  if(std::find(m_noncesList.begin(), m_noncesList.end(), ts->GetNonce())!=m_noncesList.end())
		  {
			  NS_LOG_LOGIC("First nonce received");
			  m_mustCache = true;
			 // iface->SetInitCache(false);
			  //m_initCache = false;
		  } else {
			  m_mustCache = false;
		  }

		  if(IsNonceFromInterface(ts->GetNonce()))
		  {
			  NS_LOG_LOGIC("Nonce from interface nonce received");
			  m_mustCache=true;
			  //iface->SetInitCache(false);
			  //m_initCache = false;
		  }

	  }
	} else {
		  m_mustCache = false;
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
    // int32_t ifaceNumber = GetInterfaceForDevice (device);

     Ptr<InrppTailQueue> q = device->GetObject <PointToPointNetDevice>()->GetQueue()->GetObject<InrppTailQueue>();

    NS_LOG_LOGIC("Detour State Cache " << iface->GetState() << " " << m_mustCache << " " << iface->GetInitCache() << " " << m_cache->GetSize() << " " << q->GetNBytes());

     std::map <Ptr<const Packet>, Ptr<Ipv4Route> >::iterator it = m_routeList.find(packet);
     if(it==m_routeList.end())NS_LOG_LOGIC("ROUTE NOT FOUND");
    if(p->RemoveHeader(tcpHeader))
	{
    		NS_LOG_LOGIC(tcpHeader);
			if(AddOptionInrpp(tcpHeader,3,iface->GetNonce()))
			{
				uint32_t size = ipHeader.GetPayloadSize();
				ipHeader.SetPayloadSize(size+12);
			}

			//ProcessInrppOption(tcpHeader,iface);
			NS_LOG_LOGIC("TTL " << (uint32_t)ipHeader.GetTtl());
			ipHeader.SetTtl (ipHeader.GetTtl () + 1);
			NS_LOG_LOGIC("TTL " << (uint32_t)ipHeader.GetTtl());

			p->AddHeader(tcpHeader);
			p->AddHeader(ipHeader);


			if(!m_cache->InsertFirst(iface,it->second,p)){
				NS_LOG_LOGIC("CACHE FULL");
			}

	 }

}

void
InrppL3Protocol::Discard(Ptr<const Packet> packet)
{
	m_routeList.erase(packet);
}

} // namespace ns3
