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
#include "ns3/ppp-header.h"

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
	m_state(NO_DETOUR),
    data3(0),
	m_nonce(rand()),
	m_disable(false),
	m_ackRate(0),
	packetSize(0),
	m_initCache(true),
	m_slot(0)//,
	//m_lastSlot(0)

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
	NS_LOG_FUNCTION (this<<m_state);
	if(m_state==NO_DETOUR||m_state==DISABLE_BACK)
	{
		NS_LOG_FUNCTION(this<<packets<<dev<<m_state);
		SetState(DETOUR);
		SendPacket();
	}
	m_disable=false;
}

void
InrppInterface::LowTh(uint32_t packets,Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION(this<<packets<<dev<<m_currentBW2<<m_bps.GetBitRate()<<m_cache->GetSize()<<m_cache->GetThreshold());

	if(m_cache->GetSize()<m_cache->GetThreshold()){
		SetState(DISABLE_BACK);
	}
	m_disable = true;
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
	m_inrpp->NotifyState(this,state);
	if(m_state==NO_DETOUR)
	{
		m_disable=false;
		m_initCache=true;
	}

}

void
InrppInterface::TxRx(Ptr<const Packet> p, Ptr<NetDevice> dev1 ,  Ptr<NetDevice> dev2,  Time tr, Time rcv)
{
	NS_LOG_FUNCTION(this<<p);

	if(GetDevice()!=dev1)return;
	m_inrpp->Discard(p);
  // read the tag from the packet copy
  InrppTag tag;

  if(!p->PeekPacketTag (tag))
  {
	  data+= p->GetSize() * 8;
	  if(Simulator::Now().GetSeconds()-t1.GetSeconds()>0.01){
		  NS_LOG_LOGIC("Data " << data << " "<< p->GetSize()*8);
		  m_currentBW = data / (Simulator::Now().GetSeconds()-t1.GetSeconds());
		  data = 0;
		  double alpha = 0.6;
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
		  double alpha = 0.6;
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
  if(Simulator::Now().GetSeconds()-t2.GetSeconds()>0.01)
  {
	  m_currentBW2 = data2 / (Simulator::Now().GetSeconds()-t2.GetSeconds());
	  NS_LOG_LOGIC("Data2 " << data2 << " "<< p->GetSize()*8 << " "<< (Simulator::Now().GetSeconds()-t2.GetSeconds()) << " " << m_currentBW2);
	  data2 = 0;
	  double alpha = 0.6;
	  double sample_bwe = m_currentBW2;
	  m_currentBW2 = (alpha * m_lastBW2) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW2) / 2));
	  m_lastSampleBW2 = sample_bwe;
	  m_lastBW2 = m_currentBW2;
	  t2 = Simulator::Now();

  }

  if(m_state==DISABLE_BACK&&m_cache->GetSize()==0)
  {
	  if(m_disable)SetState(NO_DETOUR);
	  else SetState(DETOUR);
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
  q->SetDropCallback (MakeCallback (&InrppInterface::Drop,this));

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


void
InrppInterface::SetInrppL3Protocol(Ptr<InrppL3Protocol> inrpp)
{
    m_inrpp = inrpp;
}

void
InrppInterface::SendPacket()
{

	if(!m_txEvent.IsRunning()){
		NS_LOG_FUNCTION(this<<m_ackRate<<packetSize<<m_state<<m_cache->GetSize(this,m_slot)<<m_cache->GetSize());
		if(((m_state==UP_BACKPRESSURE||m_state==PROP_BACKPRESSURE)&&m_ackRate>=packetSize)||(m_state!=UP_BACKPRESSURE&&m_state!=PROP_BACKPRESSURE))
		{
			uint32_t loop=0;
			NS_LOG_FUNCTION(this<<m_slot);
			Ptr<CachedPacket> c;
			do{
				NS_LOG_FUNCTION(this<<m_slot<<loop<<m_cache->GetSize(this,m_slot));
				//std::map<uint32_t,uint32_t>::iterator it = m_weights.find(m_slot);
				//if(it->second==m_lastSlot)
				//{
				//	m_slot++;
				//	m_lastSlot=0;
				//}
				m_slot=m_slot%m_numSlot;
			//	NS_LOG_FUNCTION(this<<m_slot<<it->second<<m_lastSlot);
				c = m_cache->GetPacket(this,m_slot);
				m_slot++;
				NS_LOG_LOGIC("Packet " << c << " " << m_slot);
				loop++;
			}while(!c&&loop<=m_numSlot);
			if(c){
				//m_lastSlot++;
				Ptr<Ipv4Route> rtentry = c->GetRoute();
				Ptr<const Packet> packet = c->GetPacket();
				m_inrpp->SendData(rtentry,packet);
				if(m_state==UP_BACKPRESSURE||m_state==PROP_BACKPRESSURE)m_ackRate-=packetSize;
					packetSize=packet->GetSize();
				Time t = Seconds(((double)(packet->GetSize()*8)+16)/(uint32_t)m_bps.GetBitRate());
				NS_LOG_LOGIC("Time " << t.GetSeconds() << " " << packet->GetSize() << " " << m_ackRate);
				m_txEvent = Simulator::Schedule(t,&InrppInterface::SendPacket,this);
			}
		} else if (m_cache->GetSize()>0)
		{
			Time t = Seconds(((double)(packetSize*8)+16)/(uint32_t)m_bps.GetBitRate());
			m_txEvent = Simulator::Schedule(t,&InrppInterface::SendPacket,this);
		}
	}

}

void
InrppInterface::SetDetouredIface(Ptr<InrppInterface> interface,Ipv4Address address)
{
	NS_LOG_FUNCTION(this<<interface);
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

	if(!m_txResidualEvent.IsRunning())
	{
		NS_LOG_FUNCTION(this<<m_ackRate<<packetSize<<m_state<<m_cache->GetSize(m_detouredIface,m_slot));

		if(m_cache->GetSize()>0)
		{
			NS_LOG_FUNCTION(this<<m_slot);
			uint32_t loop=0;
			Ptr<CachedPacket> c;
			do{
				NS_LOG_FUNCTION(this<<m_slot<<loop<<m_cache->GetSize(this,m_slot));
			//	std::map<uint32_t,uint32_t>::iterator it = m_weights.find(m_slot);
			//	if(it->second==m_lastSlot)
			//	{
		//			m_slot++;
			//		m_lastSlot=0;
			//	}
				m_slot=m_slot%m_numSlot;
				//NS_LOG_FUNCTION(this<<m_slot<<it->second<<m_lastSlot);
				c = m_cache->GetPacket(m_detouredIface,m_slot);
				m_slot++;
				NS_LOG_LOGIC("Packet " << c << " " << m_slot);
				loop++;
			}while(!c&&loop<=m_numSlot);
			if(c)
			{
				//m_lastSlot++;
				Ptr<Ipv4Route> rtentry = c->GetRoute();
				Ptr<const Packet> packet = c->GetPacket();
				InrppTag tag;
				tag.SetAddress (rtentry->GetGateway());
				packet->AddPacketTag (tag);
				rtentry->SetGateway(m_detourRoute->GetDetour());
				rtentry->SetOutputDevice(m_detourRoute->GetOutputDevice());
				m_inrpp->SendData(rtentry,packet);
				Time t = Seconds(((double)(packet->GetSize()+16)*8)/m_residualMin);
				NS_LOG_LOGIC("Time " << t.GetSeconds() << (packet->GetSize()+16)*8 << " " << m_residualMin);
				m_txResidualEvent = Simulator::Schedule(t,&InrppInterface::SendResidual,this);
			}
		}
	}

}

/*void
InrppInterface::PushPacket(Ptr<Packet> p,Ptr<Ipv4Route> route)
{
	m_queue.push(std::make_pair(p,route));
	m_cache->AddPacket();
}*/

void
InrppInterface::CalculatePacing(uint32_t bytes)
{
	NS_LOG_FUNCTION(this);
	m_ackRate+=bytes;

}

void
InrppInterface::SetInitCache(bool init)
{
	m_initCache = init;
}

bool
InrppInterface::GetInitCache(void)
{
	return m_initCache;
}

bool
InrppInterface::GetDisable(void)
{
	return m_disable;
}

void
InrppInterface::Drop (Ptr<const Packet> p)
{
	NS_LOG_FUNCTION(this<<p);
	m_inrpp->LostPacket(p,this,GetDevice());
}

void
InrppInterface::SetNumSlot(uint32_t numSlot)
{
	m_numSlot = numSlot;
}

/*void
InrppInterface::SetWeights(std::map<uint32_t,uint32_t> weights)
{
	m_weights = weights;
}*/
} // namespace ns3

