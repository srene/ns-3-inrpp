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
#include "tcp-option-inrpp-back.h"

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
  ;
  return tid;
}

InrppL3Protocol::InrppL3Protocol ():
//m_cacheFull(false),
m_mustCache(false)
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
	//m_residualList.insert(std::pair<Ipv4Address, uint32_t>(inrpp.GetAddress(),inrpp.GetResidual()));
}

void
InrppL3Protocol::IpForward (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
	  NS_LOG_FUNCTION (this << rtentry << p << header);
	  NS_LOG_LOGIC ("Forwarding logic for node: " << m_node->GetId ());
	  NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()<< " " << rtentry->GetSource() << " " << rtentry->GetOutputDevice());

	  // Forwarding
	  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
	 // DataRate bps = rtentry->GetOutputDevice ()->GetObject<PointToPointNetDevice>()->GetDataRate();
	  NS_ASSERT (interface >= 0);
	  Ptr<InrppInterface> outInterface = GetInterface (interface)->GetObject<InrppInterface>();
	  outInterface->CalculateFlow(p);

	  //int deltaRate = outInterface->GetFlow() - bps.GetBitRate();

	 // NS_LOG_LOGIC("Flow " << outInterface << " " << outInterface->GetFlow() << " BW " << outInterface->GetBW() << " DeltaRate " << deltaRate << " " << outInterface->GetState());

	  Ptr<Packet> packet = p->Copy();
	  packet->AddHeader(header);
	  //NS_LOG_LOGIC("Cache size "<< m_cache->GetSize(outInterface) << " " << m_cache->GetSize() << " " << m_mustCache << " " << outInterface->GetState());


	  if(outInterface->GetState()==DETOUR||outInterface->GetState()==BACKPRESSURE||outInterface->GetState()==DISABLE_BACK||(outInterface->GetState()==NO_DETOUR&&m_cache->GetSize(outInterface)>0))
	  {
	//	 NS_LOG_LOGIC("DETOUR STATE");
		 if(!m_cache->Insert(outInterface,rtentry,packet,0)){
			 NS_LOG_LOGIC("CACHE FULL");
		 } else {
			 outInterface->SendPacket(outInterface->GetRate());
		 }
	  } else if((outInterface->GetState()==UP_BACKPRESSURE&&m_mustCache)||(outInterface->GetState()==PROP_BACKPRESSURE&&m_mustCache)){

	   //  NS_LOG_LOGIC("UPSTREAM BACKPRESSURE STATE");

		 if(!m_cache->Insert(outInterface,rtentry,packet,0)){
			 NS_LOG_LOGIC("CACHE FULL");
		 } else {
		     //NS_LOG_LOGIC("Cache size "<< m_cache->GetSize(outInterface) << " " << m_cache->GetSize() << " " << m_rate);
		     //uint32_t rate = outInterface->GetBW()*m_rate/100;
		     //NS_LOG_LOGIC("Backpressure rate " << rate << " " << m_rate << " " << outInterface->GetBW());
			 //outInterface->SendPacket(rate);
			 outInterface->SendPacket(m_rate);
		 }

	  } else {
		  NS_LOG_LOGIC("Send packet ");
		  Send(rtentry,packet);
	  }


}

void
InrppL3Protocol::Send (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p)
{

	  //Ipv4Header ipHeader = header;
	  NS_LOG_LOGIC ("Route " << rtentry->GetDestination() << " " << rtentry->GetGateway()<< " " << rtentry->GetSource() << " " << rtentry->GetOutputDevice());

	  Ptr<Packet> packet = p->Copy ();
	  Ipv4Header ipHeader;
	  packet->RemoveHeader(ipHeader);

	  int32_t interface = GetInterfaceForDevice (rtentry->GetOutputDevice ());
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
		m_dropTrace (ipHeader, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4> (), interface);
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

		if(iface->GetState()==BACKPRESSURE)
		{
		   Ptr<InrppInterface> iface2 = FindDetourIface(iface);
		   uint32_t rate = iface->GetRate();
		   if(iface2)rate+=iface2->GetResidual();
		   //uint32_t newRate = ((double)rate/iface->GetFlow())*100;
		   uint32_t newRate = rate;
		   NS_LOG_LOGIC("Backpressure rate " << rate << " " << iface->GetFlow() << " " << newRate);
		   AddOptionInrpp(tcpHeader,1,iface->GetNonce(),newRate);
		   uint32_t size = ipHeader.GetPayloadSize();
		   ipHeader.SetPayloadSize(size+12);
		} else if (iface->GetState()==DISABLE_BACK)
		{
	       //NS_LOG_LOGIC("Disable backpressure enabled");
		   AddOptionInrpp(tcpHeader,2,iface->GetNonce(),0);
		   uint32_t size = ipHeader.GetPayloadSize();
		   ipHeader.SetPayloadSize(size+12);
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
	//m_cacheFull = true;
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
			  }
		  }

	  }
}

void
InrppL3Protocol::LowTh(uint32_t packets)
{
	NS_LOG_FUNCTION(this<<packets);
	//m_cacheFull = false;
	  for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin (); i != m_interfaces.end (); i++)
	  {
		  Ptr<Ipv4Interface> iface = *i;
		  Ptr<LoopbackNetDevice> loop = iface->GetDevice()->GetObject<LoopbackNetDevice>();
		  if(!loop)
		  {
			  Ptr<InrppInterface> iface2 = iface->GetObject<InrppInterface>();
			  if(iface2->GetState()==PROP_BACKPRESSURE&&iface2->GetFlow()<iface2->GetRate())
				  iface2->SetState(UP_BACKPRESSURE);

		  }
	  }

}

void
InrppL3Protocol::AddOptionInrpp (TcpHeader& header,uint8_t flag,uint32_t nonce, uint32_t rate)
{
  NS_LOG_FUNCTION (this << header);

  Ptr<TcpOptionInrppBack> option = CreateObject<TcpOptionInrppBack> ();
  option->SetFlag(flag);
  option->SetNonce (nonce);
  option->SetDeltaRate (rate);

  header.AppendOption (option);

}

void
InrppL3Protocol::ProcessInrppOption(TcpHeader& tcpHeader,Ptr<InrppInterface> iface)
{
	NS_LOG_FUNCTION (this << tcpHeader);

	if (tcpHeader.HasOption (TcpOption::INRPP_BACK))
	{

	  Ptr<TcpOptionInrppBack> ts = DynamicCast<TcpOptionInrppBack> (tcpHeader.GetOption (TcpOption::INRPP_BACK));

	  NS_LOG_INFO (m_node->GetId () << " Got InrppBack flag=" <<
				   (uint32_t) ts->GetFlag()<< " and nonce="     << ts->GetNonce () << " and rate=" << ts->GetDeltaRate());

	  if(ts->GetFlag()==1){
		  if(iface->GetState()==NO_DETOUR)
		  {
			  iface->SetState(UP_BACKPRESSURE);
		  }
		  if(iface->GetState()==UP_BACKPRESSURE)
		  {
			  ts->SetFlag(0);
			  m_noncesList.push_back(ts->GetNonce());
		  }
		  if(iface->GetState()==PROP_BACKPRESSURE)
		  {
			  NS_LOG_LOGIC("propagate backpressure");
			  //uint32_t n;
			  std::map<uint32_t,uint32_t>::iterator it = m_nonceCounter.find(ts->GetNonce());
			  if(it!= m_nonceCounter.end()){
				  NS_LOG_LOGIC("propagate backpressure " << it->second << " paths");
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
	  if(ts->GetFlag()==3)
	  {

		  if(std::find(m_noncesList.begin(), m_noncesList.end(), ts->GetNonce())!=m_noncesList.end())
		  {
			  m_mustCache = true;
			  m_rate = ts->GetDeltaRate();
		  } else {
			  m_mustCache = false;
		  }

		  std::map <uint32_t, std::vector<Ptr<InrppInterface> > >::iterator it;

		  it = m_nonceIfaces.find(ts->GetNonce());

		  if (it != m_nonceIfaces.end())
		  {
			  std::vector<Ptr<InrppInterface> > ifVector = it->second;
			  std::vector<Ptr<InrppInterface> >::iterator it2;
			  it2 = find (ifVector.begin(), ifVector.end(), iface);

			  if(it2== ifVector.end()){
				  ifVector.push_back(iface);
				  std::map <uint32_t, uint32_t >::iterator it3;
				  it3 = m_nonceCounter.find(ts->GetNonce());
				  if(it3!= m_nonceCounter.end())
				  {
					  it3->second++;
				  } else {
					  m_nonceCounter.insert(std::pair<uint32_t,uint32_t>(ts->GetNonce(),1));
				  }
			  }

		  } else {
			  std::vector<Ptr<InrppInterface> > ifaces;
			  ifaces.push_back(iface);
			  m_nonceIfaces.insert(std::pair<uint32_t,std::vector<Ptr<InrppInterface> > >(ts->GetNonce(),ifaces));
			  m_nonceCounter.insert(std::pair<uint32_t,uint32_t>(ts->GetNonce(),1));

		  }


	  }
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

} // namespace ns3
