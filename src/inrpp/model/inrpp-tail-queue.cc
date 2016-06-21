/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "inrpp-tail-queue.h"

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/inrpp-header.h"
#include "ns3/ppp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppTailQueue");

NS_OBJECT_ENSURE_REGISTERED (InrppTailQueue);

TypeId InrppTailQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppTailQueue")
    .SetParent<DropTailQueue> ()
    .SetGroupName("Inrpp")
    .AddConstructor<InrppTailQueue> ()
    .AddAttribute ("HigherThPackets",
                   "The higher threshold number of packets accepted by this InrppTailQueue.",
                   UintegerValue (80),
                   MakeUintegerAccessor (&InrppTailQueue::m_highPacketsTh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("HigherThBytes",
                   "The higher threshold number of bytes accepted by this InrppTailQueue.",
                   UintegerValue (80 * 65535),
                   MakeUintegerAccessor (&InrppTailQueue::m_highBytesTh),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("LowerThPackets",
				   "The lower threshold number of packets accepted by this InrppTailQueue.",
				   UintegerValue (50),
				   MakeUintegerAccessor (&InrppTailQueue::m_lowPacketsTh),
				   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("LowerThBytes",
				   "The lower threshold number of bytes accepted by this InrppTailQueue.",
				   UintegerValue (50 * 65535),
				   MakeUintegerAccessor (&InrppTailQueue::m_lowBytesTh),
				   MakeUintegerChecker<uint32_t> ())
	/*.AddTraceSource ("HigherThreshold", "Drop a packet stored in the queue.",
					 MakeTraceSourceAccessor (&InrppTailQueue::m_highTh),
					 "ns3::InrppTailQueue::HighThTracedCallback")
	.AddTraceSource ("LowerThreshold", "Drop a packet stored in the queue.",
					 MakeTraceSourceAccessor (&InrppTailQueue::m_lowTh),
					 "ns3::InrppTailQueue::LowThTracedCallback")*/
  ;

  return tid;
}

InrppTailQueue::InrppTailQueue () :
  DropTailQueue (),
  m_hTh(false),
  m_lTh(false)
  /*m_packets (),
  m_bytesInQueue (0)*/
{
  NS_LOG_FUNCTION (this);
}

InrppTailQueue::~InrppTailQueue ()
{
  NS_LOG_FUNCTION (this);
}

void
InrppTailQueue::SetHighThCallback(HighThCallback cb)
{
	NS_LOG_FUNCTION (this << &cb);
	m_highTh = cb;
}

void
InrppTailQueue::SetLowThCallback(LowThCallback cb)
{
	m_lowTh = cb;

}

void
InrppTailQueue::SetNetDevice(Ptr<NetDevice> dev)
{
	NS_LOG_FUNCTION (this << dev);
	m_netDevice = dev;
}

bool 
InrppTailQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << m_packets.size () << p->GetSize());
  //p->Print (std::cout);
	Ptr<Packet> packet = p->Copy ();
	PppHeader h;
	InrppHeader inrpp;
	if(packet->RemoveHeader (h))NS_LOG_LOGIC("Send p2p Info");
	//if(packet->PeekHeader (inrpp))NS_LOG_LOGIC("Send Inrpp Info");
  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () >= m_maxBytes) && !packet->PeekHeader (inrpp))
    {
	  if(!m_drop.IsNull())m_drop(p);
    }
  if (m_mode == QUEUE_MODE_PACKETS && (m_packets.size () >= m_highPacketsTh)&&!m_hTh)
    {
      NS_LOG_LOGIC ("Queue reaching full " << this);
      if(!m_highTh.IsNull())m_highTh (m_packets.size (),m_netDevice);
      m_hTh = true;
      m_lTh = false;

    }

  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () >= m_highBytesTh)&&!m_hTh)
    {
      NS_LOG_LOGIC ("Queue reaching full " << this);
      if(!m_highTh.IsNull())m_highTh (m_packets.size (),m_netDevice);
      m_hTh = true;
      m_lTh = false;

    }

  if (m_mode == QUEUE_MODE_PACKETS && (m_packets.size () <= m_lowPacketsTh)&&(!m_lTh&&m_hTh))
    {
      NS_LOG_LOGIC ("Queue uncongested " << this);
      if(!m_lowTh.IsNull())m_lowTh (m_packets.size (),m_netDevice);
      m_lTh = true;
      m_hTh = false;
    }

  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () <= m_lowBytesTh)&&(!m_lTh&&m_hTh))
    {
      NS_LOG_LOGIC ("Queue uncongested " << this);
      if(!m_lowTh.IsNull())m_lowTh (m_packets.size (),m_netDevice);
      m_lTh = true;
      m_hTh = false;
    }

  //DropTailQueue::DoEnqueue(p);

  if (m_mode == QUEUE_MODE_PACKETS && (m_packets.size () >= m_maxPackets)&& !packet->PeekHeader (inrpp))
    {
      NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
      Drop (p);
      return false;
    }

  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () >= m_maxBytes) && !packet->PeekHeader (inrpp))
    {
      NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
      Drop (p);
      return false;
    }

  m_bytesInQueue += p->GetSize ();
  m_packets.push (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return true;
}
void
InrppTailQueue::SetDropCallback(DropCallback cb)
{
	NS_LOG_FUNCTION (this << &cb);
	m_drop = cb;
}

uint32_t
InrppTailQueue::GetNBytes()
{
	return m_bytesInQueue;
}

} // namespace ns3

