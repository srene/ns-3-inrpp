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
#include "ns3/channel.h"
#include "inrpp-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppInterface");

NS_OBJECT_ENSURE_REGISTERED (InrppInterface);


TypeId 
InrppInterface::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppInterface")
    .SetParent<Ipv4Interface> ()
    .AddTraceSource("EstimatedBW", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW),
				     "ns3::TracedValue::DoubleCallback")
    .AddTraceSource("EstimatedFlow", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW2),
				     "ns3::TracedValue::DoubleCallback")
    .AddTraceSource("DetouredFlow", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW3),
				     "ns3::TracedValue::DoubleCallback");
  ;
  return tid;
}

/** 
 * By default, Ipv4 interface are created in the "down" state
 *  with no IP addresses.  Before becoming useable, the user must 
 * invoke SetUp on them once an Ipv4 address and mask have been set.
 */
InrppInterface::InrppInterface ()
:	m_currentBW(0),
	m_lastSampleBW(0),
	m_lastBW(0),
    data(0),
	m_currentBW2(0),
	m_lastSampleBW2(0),
	m_lastBW2(0),
    data2(0),
	m_currentBW3(0),
	m_lastSampleBW3(0),
	m_lastBW3(0),
    data3(0),
	m_nonce(rand())
{
  NS_LOG_FUNCTION (this);
  t1 = Simulator::Now();
  t2 = Simulator::Now();
  t3 = Simulator::Now();

}

InrppInterface::~InrppInterface ()
{
  NS_LOG_FUNCTION (this);
}

void
InrppInterface::HighTh( uint32_t packets,Ptr<NetDevice> dev)
{

	if(m_state==NO_DETOUR)
	{
		NS_LOG_FUNCTION(this<<packets<<dev<<m_state);
		SetState(DETOUR);
		SendPacket(m_bps.GetBitRate());
	}


}

void
InrppInterface::LowTh(uint32_t packets,Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION(this<<packets<<dev<<m_currentBW2<<m_bps.GetBitRate());

	if(m_state!=NO_DETOUR&&m_currentBW2<m_bps.GetBitRate())
	{
		NS_LOG_FUNCTION(this<<packets<<dev<<m_currentBW2<<m_bps.GetBitRate());
		SetState(NO_DETOUR);
	}
	//m_txEvent.Cancel();


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
InrppInterface::SetState(InrppState state)
{
	NS_LOG_FUNCTION(this<<state);
	m_state = state;


}
void
InrppInterface::TxRx(Ptr<const Packet> p, Ptr<NetDevice> dev1 ,  Ptr<NetDevice> dev2,  Time tr, Time rcv)
{
	NS_LOG_LOGIC(this);

  // read the tag from the packet copy
  InrppTag tag;
  if(!p->PeekPacketTag (tag))
  {
	  data+= p->GetSize() * 8;
	  if(Simulator::Now().GetSeconds()-t1.GetSeconds()>0.01){
		  NS_LOG_LOGIC("Data " << data << " "<< p->GetSize()*8);
		  m_currentBW = data / (Simulator::Now().GetSeconds()-t1.GetSeconds());
		  data = 0;
		  double alpha = 0.1;
		  double   sample_bwe = m_currentBW;
		  m_currentBW = (alpha * m_lastBW) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW) / 2));
		  m_lastSampleBW = sample_bwe;
		  m_lastBW = m_currentBW;
		  t1 = Simulator::Now();

	  }
  } else {

	  data3+= p->GetSize() * 8;
	  if(Simulator::Now().GetSeconds()-t3.GetSeconds()>0.01){
		  NS_LOG_LOGIC("Data3 " << data3 << " "<< p->GetSize()*8);
		  m_currentBW3 = data3 / (Simulator::Now().GetSeconds()-t3.GetSeconds());
		  data3 = 0;
		  double alpha = 0.1;
		  double   sample_bwe = m_currentBW3;
		  m_currentBW3 = (alpha * m_lastBW3) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW3) / 2));
		  m_lastSampleBW3 = sample_bwe;
		  m_lastBW3 = m_currentBW3;
		  t3 = Simulator::Now();
	  }

  }

}

void
InrppInterface::CalculateFlow(Ptr<const Packet> p)
{
  NS_LOG_LOGIC(this);

  data2+= p->GetSize() * 8;
  if(Simulator::Now().GetSeconds()-t2.GetSeconds()>0.01){
	  m_currentBW2 = data2 / (Simulator::Now().GetSeconds()-t2.GetSeconds());
	  NS_LOG_LOGIC("Data2 " << data2 << " "<< p->GetSize()*8 << " "<< (Simulator::Now().GetSeconds()-t2.GetSeconds()) << " " << m_currentBW2);
	  data2 = 0;
	  double alpha = 0.1;
	  double sample_bwe = m_currentBW2;
	  m_currentBW2 = (alpha * m_lastBW2) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW2) / 2));
	  m_lastSampleBW2 = sample_bwe;
	  m_lastBW2 = m_currentBW2;
	  t2 = Simulator::Now();

  }

}

uint32_t
InrppInterface::GetFlow()
{
	return m_currentBW2;
}

uint32_t
InrppInterface::GetBW()
{
	return m_currentBW;
}

uint32_t
InrppInterface::GetDetoured()
{
	return m_currentBW3;
}

uint32_t
InrppInterface::GetResidual()
{
	uint32_t residual;
	NS_LOG_LOGIC("Bitrate " << (uint32_t)m_bps.GetBitRate() << " bw " << m_currentBW.Get());
	if((uint32_t)m_bps.GetBitRate()<m_currentBW.Get())
		residual = 0;
	else
		residual = (uint32_t)m_bps.GetBitRate()-m_currentBW.Get();

	return residual;
}

void
InrppInterface::SetDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  Ptr<InrppTailQueue> q = device->GetObject <PointToPointNetDevice>()->GetQueue()->GetObject<InrppTailQueue>();
  q->SetNetDevice(device);
  q->SetHighThCallback (MakeCallback (&InrppInterface::HighTh,this));
  q->SetLowThCallback (MakeCallback (&InrppInterface::LowTh,this));

  Ptr<PointToPointChannel> ch = device->GetChannel()->GetObject<PointToPointChannel>();
  ch->TraceConnectWithoutContext ("TxRxPointToPoint", MakeCallback (&InrppInterface::TxRx,this));
  Ipv4Interface::SetDevice(device);
}

void
InrppInterface::SetDeltaRate(uint32_t deltaRate)
{
	m_deltaRate = deltaRate;
}

uint32_t
InrppInterface::GetDeltaRate(void)
{
	return m_deltaRate;
}

uint32_t
InrppInterface::GetNonce(void)
{
	return m_nonce;
}

void
InrppInterface::SetCache(Ptr<InrppCache> cache)
{
	m_cache = cache;
}

void
InrppInterface::SetRate(DataRate bps)
{
	m_bps = bps;
}

uint32_t
InrppInterface::GetRate()
{
	return m_bps.GetBitRate();
}

void
InrppInterface::SetInrppL3Protocol(Ptr<InrppL3Protocol> inrpp)
{
    m_inrpp = inrpp;
}

void
InrppInterface::SendPacket(uint32_t rate)
{
	if(!m_txEvent.IsRunning()){
		NS_LOG_FUNCTION(this);

		Ptr<CachedPacket> c = m_cache->GetPacket(this);
		if(c){
			Ptr<Ipv4Route> rtentry = c->GetRoute();
			Ptr<const Packet> packet = c->GetPacket();
			m_inrpp->Send(rtentry,packet);
			uint32_t sendingRate = std::min(rate,(uint32_t)m_bps.GetBitRate());
			Time t = Seconds(((double)(packet->GetSize()*8)+60)/sendingRate);
			NS_LOG_LOGIC("Time " << t.GetSeconds() << packet->GetSize()*8 << " " << rate);
			m_txEvent = Simulator::Schedule(t,&InrppInterface::SendPacket,this,sendingRate);
		}

	}
}

void
InrppInterface::SetDetouredIface(Ptr<InrppInterface> interface,Ipv4Address address)
{
	NS_LOG_FUNCTION(this<<interface);
	//m_detouredIfaces.insert(std::pair<Ptr<InrppInterface>, Ipv4Address>(interface,address));
	m_detouredIface= interface;
}

Ptr<InrppInterface>
InrppInterface::GetDetouredIface(void)
{
	return m_detouredIface;
}


void
InrppInterface::UpdateResidual(Ipv4Address address, uint32_t residual)
{
	m_residualMin = std::min(GetResidual(),residual);
	if(!m_txResidualEvent.IsRunning())
	{
		NS_LOG_FUNCTION(this);
		SendResidual();
	}

}

void
InrppInterface::SendResidual()
{

	if(!m_txEvent.IsRunning())
	{
		NS_LOG_FUNCTION(this<<m_detouredIface);

		if(m_cache->GetSize(m_detouredIface)>0){
			Ptr<CachedPacket> c = m_cache->GetPacket(m_detouredIface);
			if(c)
			{
				Ptr<Ipv4Route> rtentry = c->GetRoute();
				Ptr<const Packet> packet = c->GetPacket();
				  InrppTag tag;
				  tag.SetAddress (rtentry->GetGateway());
				  packet->AddPacketTag (tag);
				  rtentry->SetGateway(m_detourRoute->GetDetour());
				  rtentry->SetOutputDevice(m_detourRoute->GetOutputDevice());
				  m_inrpp->Send(rtentry,packet);
				Time t = Seconds(((double)packet->GetSize()*8)/m_residualMin);
				NS_LOG_LOGIC("Time " << t.GetSeconds() << packet->GetSize()*8 << " " << m_residualMin);
				m_txEvent = Simulator::Schedule(t,&InrppInterface::SendResidual,this);
			}
		}
	}

}


} // namespace ns3

