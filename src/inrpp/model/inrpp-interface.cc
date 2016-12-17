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
    .AddTraceSource("OutputThroughput", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW),
				     "ns3::TracedValue::DoubleCallback")
    .AddTraceSource("InputThroughput", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW2),
				     "ns3::TracedValue::DoubleCallback")
    .AddTraceSource("DetouredThroughput", "The estimated bandwidth",
	     			 MakeTraceSourceAccessor(&InrppInterface::m_currentBW3),
				     "ns3::TracedValue::DoubleCallback")
	.AddAttribute ("Refresh",
					   "Moving average refresh value.",
					   DoubleValue (0.1),
					   MakeDoubleAccessor (&InrppInterface::m_refresh),
					   MakeDoubleChecker<double> ());
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
    data3(0),
	m_state(NO_DETOUR),
	m_nonce(rand()),
	m_disable(false),
	packetSize(0),
	m_initCache(true),
	m_slot(0),
	m_nDetour(0),
	m_lastDetoured(0),
	m_enabled(false)
{
  NS_LOG_FUNCTION (this);

  //Initialize the timers used to calculate the throughputs
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
	NS_LOG_FUNCTION (this<<GetState());
	//Update state when the link buffer is full
	if(GetState()!=DETOUR&&GetState()!=BACKPRESSURE)
	{
		SetState(DETOUR);
		SendPacket();
	}
	m_disable=false;
}

void
InrppInterface::LowTh(uint32_t packets,Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION(this<<packets<<dev<<m_currentBW2<<m_bps.GetBitRate()<<m_cache->GetSize()<<m_cache->GetThreshold());
	//Update state when the link buffer is empty
	if((GetState()==DETOUR||GetState()==BACKPRESSURE)&&m_cache->GetSize()<m_cache->GetThreshold()){
		SetState(DISABLE_BACK);
	}
	m_disable = true;
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
	//m_ackRate = 0;
	if(m_state==NO_DETOUR)
	{
		m_disable=false;
		m_initCache=true;
	}
	if(m_state==DETOUR)m_enabled=true;
	if(m_state==DISABLE_BACK)m_enabled=false;

}

void
InrppInterface::TxRx(Ptr<const Packet> p, Ptr<NetDevice> dev1 ,  Ptr<NetDevice> dev2,  Time tr, Time rcv)
{
  if(GetDevice()!=dev1)return;
  m_inrpp->Discard(p);
  InrppTag tag;

  if(!p->PeekPacketTag (tag))
  {
	  data+= p->GetSize() * 8;
	  if(Simulator::Now().GetSeconds()-t1.GetSeconds()>m_refresh){
		  double   sample_bwe = data / (Simulator::Now().GetSeconds()-t1.GetSeconds());
		  data = 0;
		  double alpha = 0.6;
		  m_currentBW = (alpha * m_lastBW) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW) / 2));
		  m_lastSampleBW = sample_bwe;
		  m_lastBW = m_currentBW;
		  t1 = Simulator::Now();
	  }
	  m_refreshEvent.Cancel();
	  m_refreshEvent = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh,this);
  } else {

	  data3+= p->GetSize() * 8;
	  if(Simulator::Now().GetSeconds()-t3.GetSeconds()>m_refresh){
		  double   sample_bwe = data3 / (Simulator::Now().GetSeconds()-t3.GetSeconds());
		  data3 = 0;
		  double alpha = 0.6;
		  m_currentBW3 = (alpha * m_lastBW3) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW3) / 2));
		  m_lastSampleBW3 = sample_bwe;
		  m_lastBW3 = m_currentBW3;
		  t3 = Simulator::Now();
	  }

	  m_refreshEvent3.Cancel();
	  m_refreshEvent3 = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh3,this);

  }

}

void
InrppInterface::CalculateFlow(Ptr<const Packet> p)
{
  data2+= p->GetSize() * 8;
  if(Simulator::Now().GetSeconds()-t2.GetSeconds()>m_refresh)
  {
	  double sample_bwe = data2 / (Simulator::Now().GetSeconds()-t2.GetSeconds());
	  data2 = 0;
	  double alpha = 0.6;
	  m_currentBW2 = (alpha * m_lastBW2) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW2) / 2));
	  m_lastSampleBW2 = sample_bwe;
	  m_lastBW2 = m_currentBW2;
	  t2 = Simulator::Now();

  }
  m_refreshEvent2.Cancel();
  m_refreshEvent2 = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh2,this);

  if(GetState()==DISABLE_BACK&&m_cache->GetSize()==0)
  {
	  if(m_disable)SetState(NO_DETOUR);
	  else SetState(DETOUR);
  }

}


void
InrppInterface::CalculateDetour(Ipv4Address ip, Ptr<const Packet> p)
{

  for(std::map<Ipv4Address,uint32_t>::iterator it = data4.begin();it!=data4.end();it++)
  {
		NS_LOG_LOGIC("Ip " << it->first << " data " << it->second << " ip " << ip);
		uint32_t tdata = it->second;

		if(it->first == ip)
		{
		  tdata+= p->GetSize() * 8;
		  data4.erase(it);
		  data4.insert(std::make_pair(it->first,tdata));
		}

        Time tt;
		std::map<Ipv4Address,Time>::iterator it2 = t4.find(it->first);
		if(it2!=t4.end())
		{
		  tt = it2->second;

		}

		std::map<Ipv4Address,double>::iterator it3 = m_lastSampleBW4.find(it->first);
		std::map<Ipv4Address,double>::iterator it4 = m_lastBW4.find(it->first);

		if(Simulator::Now().GetSeconds()-tt.GetSeconds()>m_refresh)
		{
		  double t_currentBW = tdata / (Simulator::Now().GetSeconds()-tt.GetSeconds());
		  tdata = 0;
		  data4.erase(it);
		  data4.insert(std::make_pair(it->first,tdata));
		  double alpha = 0.6;
		  double sample_bwe = t_currentBW;
		  t_currentBW = (alpha * it4->second) + ((1 - alpha) * ((sample_bwe + it3->second) / 2));
		  if(it!=data4.end())
		  m_lastSampleBW4.erase(it3);
		  m_lastSampleBW4.insert(std::make_pair(it->first,sample_bwe));
		  m_lastBW4.erase(it4);
		  m_lastBW4.insert(std::make_pair(it->first,t_currentBW));
		  t4.erase(it2);
		  t4.insert(std::make_pair(it->first,Simulator::Now()));
		  NS_LOG_LOGIC("Detoured data " << it->first << " "<< tdata << " "<< p->GetSize()*8 << " "<< (Simulator::Now().GetSeconds()-tt.GetSeconds()) << " " << t_currentBW);
		}

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
	if(((uint32_t)m_bps.GetBitRate()<m_currentBW.Get())||(GetState()!=NO_DETOUR&&GetState()!=DISABLE_BACK))
		residual = 0;
	else
		residual = (uint32_t)m_bps.GetBitRate()-m_currentBW.Get();

	return residual;
}

uint32_t
InrppInterface::GetResidual(Ipv4Address address)
{
	NS_LOG_FUNCTION(this<<address);

	uint32_t residual = GetResidual();
	int av=0;
	for (std::map<Ipv4Address,double>::iterator it=m_lastBW4.begin(); it!=m_lastBW4.end(); ++it)
	{
	    av+=it->second;
	}

    av=residual-av;
	uint32_t th=0;


	std::map<Ipv4Address,double>::iterator it=m_lastBW4.find(address);
	if(it!=m_lastBW4.end())
	{
		th=it->second;
	}


	if(av<0)
	{
		return (uint32_t)GetResidual()/m_nDetour;
	}
	else
	{
		return (uint32_t)(th)+(av/m_nDetour);
	}
}

void
InrppInterface::SetDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  Ptr<InrppTailQueue> q = device->GetObject <PointToPointNetDevice>()->GetQueue()->GetObject<InrppTailQueue>();
  NS_LOG_FUNCTION (this << q);
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

DataRate
InrppInterface::GetRate()
{
	return m_bps;
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

		if (m_cache->GetSize()>0)
		{
			uint32_t loop=0;
			NS_LOG_FUNCTION(this<<m_slot);
			Ptr<CachedPacket> c;
			do{
				m_slot=m_slot%m_numSlot;
				c = m_cache->GetPacket(this,m_slot);

				if(c)
				{
					uint32_t nonce = c->GetNonce();
					NS_LOG_LOGIC("NOnce " << nonce << " " << m_nonce);
					if(nonce!=m_nonce){
						NS_LOG_LOGIC("Upstream node");
						std::map<uint32_t,uint32_t>::iterator it = m_ackRate.find(nonce);
						NS_LOG_LOGIC("Cached nonce " << nonce << " " << it->second << " " << packetSize);
						if(it!=m_ackRate.end()&&it->second>=packetSize)
						{
							NS_LOG_LOGIC("Ack pacing OK " << it->second);
							it->second-=packetSize;
						} else {
							NS_LOG_LOGIC("Ack pacing KO " << it->second);
							m_cache->InsertFirst(this,m_slot,c->GetRoute(),c->GetPacket(),c->GetNonce());
							c = 0;
						}
					}
				} else {
					NS_LOG_LOGIC("No packet at flow " << m_slot);
				}
				m_slot++;
				loop++;

			}while(!c&&loop<=m_numSlot);
			if(c){
				NS_LOG_LOGIC("Send data packet");
				Ptr<Ipv4Route> rtentry = c->GetRoute();
				Ptr<const Packet> packet = c->GetPacket();
				InrppTag tag;
				Ptr<Packet> p = packet->Copy();
				if(p->PeekPacketTag(tag))
					p->RemovePacketTag(tag);
				m_inrpp->SendData(rtentry,p);
				packetSize=p->GetSize();
				Time t = Seconds((double)((p->GetSize()+3)*8)/m_bps.GetBitRate());

				NS_LOG_LOGIC("Time " << t.GetSeconds() << " " << p->GetSize() << " " << (double)((p->GetSize()+2)*8)/m_bps.GetBitRate() << " "<< m_bps.GetBitRate());
				m_txEvent = Simulator::Schedule(t,&InrppInterface::SendPacket,this);
			} else {
				NS_LOG_LOGIC("No packet sent");

			}
		}
	}

}

void
InrppInterface::SetDetouredIface(Ptr<InrppInterface> interface,Ptr<InrppRoute> route)
{
	NS_LOG_FUNCTION(this<<interface << route->GetDestination());
	m_detouredIface.push_back(std::make_pair(interface,route));
}

void
InrppInterface::UpdateResidual(Ipv4Address address, uint32_t residual)
{
	NS_LOG_LOGIC("Residual " << GetResidual() << " " << residual);
	m_residualList.insert(std::make_pair(address,residual));

	uint32_t totalResidual = 0;
	std::map <Ipv4Address, uint32_t>::iterator it = m_residualList.find(address);
	if(it!=m_residualList.end()) m_residualList.erase(it);
	m_residualList.insert(std::make_pair(address,residual));

	for(it=m_residualList.begin();it!=m_residualList.end();++it)
		totalResidual+=it->second;

	m_residualMin = std::min(GetResidual(),totalResidual);
	NS_LOG_LOGIC("Residual "<<  GetAddress(0).GetLocal() << " " << address << " "<< m_residualMin.Get());

	if(!m_txResidualEvent.IsRunning()&&m_residualMin.Get()>0)
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
		//NS_LOG_FUNCTION(this<<m_ackRate<<packetSize<<GetState()<<m_cache->GetSize(m_detouredIface[m_lastDetoured].first,m_slot)<<m_residualMin);

		if(m_cache->GetSize()>0)
		{
			NS_LOG_FUNCTION(this<<m_slot);
			uint32_t loop=0;
			Ptr<CachedPacket> c;
			do{
				m_slot=m_slot%m_numSlot;
				uint32_t tries=0;
				while(!m_detouredIface[m_lastDetoured].first->GetEnabled()&&tries<=m_detouredIface.size()){
					m_lastDetoured++;
					if(m_detouredIface.size()==m_lastDetoured)m_lastDetoured=0;
					tries++;
				}
				c = m_cache->GetPacket(m_detouredIface[m_lastDetoured].first,m_slot);
				m_slot++;
				loop++;
			}while(!c&&loop<=m_numSlot);
			if(c)
			{
				NS_LOG_LOGIC(this << " Interface " << m_detouredIface[m_lastDetoured].first->GetAddress(0).GetLocal() << " is sending through " << GetAddress(0).GetLocal() << " " << m_detouredIface[m_lastDetoured].second->GetDetour() << " " << m_inrpp->GetInterface(m_inrpp->GetInterfaceForDevice (m_detouredIface[m_lastDetoured].second->GetOutputDevice()))->GetObject<InrppInterface>());
				Ptr<Ipv4Route> rtentry = c->GetRoute();
				Ptr<const Packet> packet = c->GetPacket();
				InrppTag tag;
				tag.SetAddress (rtentry->GetGateway());
				if(!packet->PeekPacketTag(tag))
					packet->AddPacketTag (tag);
				rtentry->SetGateway(m_detouredIface[m_lastDetoured].second->GetDetour());
				rtentry->SetOutputDevice(m_detouredIface[m_lastDetoured].second->GetOutputDevice());
				m_inrpp->SendData(rtentry,packet);
				m_lastDetoured++;
				if(m_detouredIface.size()==m_lastDetoured)m_lastDetoured=0;
				if(m_residualMin.Get()>0)
				{
					Time t = Seconds(((double)(packet->GetSize()+3)*8)/m_residualMin.Get());
					NS_LOG_LOGIC(this<<" Time " << t.GetSeconds() << (packet->GetSize()+2)*8 << " " << m_residualMin.Get());
					m_txResidualEvent = Simulator::Schedule(t,&InrppInterface::SendResidual,this);
				}
			}
		}
	}

}

void
InrppInterface::CalculatePacing(uint32_t bytes, uint32_t nonce)
{

	std::map<uint32_t,uint32_t>::iterator it = m_ackRate.find(nonce);
	if(it!=m_ackRate.end())
	{
		it->second+=bytes;
	} else
	{
		m_ackRate.insert(std::make_pair(nonce,bytes));
	}
	SendPacket();
	it = m_ackRate.find(nonce);
	NS_LOG_LOGIC("ACK Rate " << it->second << " " << nonce);

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
	NS_LOG_FUNCTION(this<<p<<p->GetSize());

	m_inrpp->LostPacket(p,this,GetDevice());
}

void
InrppInterface::SetNumSlot(uint32_t numSlot)
{
	m_numSlot = numSlot;
}

void
InrppInterface::OneMoreDetour(Ipv4Address ip)
{
	NS_LOG_FUNCTION(this<<ip);
	//m_currentBW4.insert(std::make_pair(ip,0));
	m_lastSampleBW4.insert(std::make_pair(ip,0));
	m_lastBW4.insert(std::make_pair(ip,0));
	t4.insert(std::make_pair(ip,Seconds(0)));
	data4.insert(std::make_pair(ip,0));
	m_nDetour++;
	  for(std::map<Ipv4Address,uint32_t>::iterator it = data4.begin();it!=data4.end();it++)
			NS_LOG_LOGIC("Ip " << it->first << " data " << it->second << " ip " << ip);
}

void
InrppInterface::SetEnabled(bool enable)
{
	m_enabled = enable;
}

bool
InrppInterface::GetEnabled(void)
{
	return m_enabled;
}

void
InrppInterface::Refresh(void)
{
	  double sample_bwe = 0;
	  double alpha = 0.6;
	  m_currentBW = (alpha * m_lastBW) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW) / 2));
	  m_lastSampleBW = sample_bwe;
	  m_lastBW = m_currentBW;
	  t1 = Simulator::Now();
	  m_refreshEvent = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh,this);
}

void
InrppInterface::Refresh2(void)
{
	  double   sample_bwe = 0;
	  double alpha = 0.6;
	  m_currentBW2 = (alpha * m_lastBW2) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW2) / 2));
	  m_lastSampleBW2 = sample_bwe;
	  m_lastBW2 = m_currentBW2;
	  t2 = Simulator::Now();
	  m_refreshEvent2 = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh2,this);
}

void
InrppInterface::Refresh3(void)
{
	  double sample_bwe = 0;
	  double alpha = 0.6;
	  m_currentBW3 = (alpha * m_lastBW3) + ((1 - alpha) * ((sample_bwe + m_lastSampleBW3) / 2));
	  m_lastSampleBW3 = sample_bwe;
	  m_lastBW3 = m_currentBW3;
	  t3 = Simulator::Now();
	  m_refreshEvent3 = Simulator::Schedule(Seconds(m_refresh),&InrppInterface::Refresh3,this);
}
}
// namespace ns3

