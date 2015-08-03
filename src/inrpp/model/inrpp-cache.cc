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
#include "inrpp-cache.h"
#include "ns3/assert.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/names.h"

#include "ns3/arp-header.h"
#include "ns3/ipv4-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InrppCache");

NS_OBJECT_ENSURE_REGISTERED (InrppCache);

TypeId 
InrppCache::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InrppCache")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddAttribute ("AliveTimeout",
                   "When this timeout expires, "
                   "the matching cache entry needs refreshing",
                   TimeValue (Seconds (120)),
                   MakeTimeAccessor (&InrppCache::m_aliveTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("DeadTimeout",
                   "When this timeout expires, "
                   "a new attempt to resolve the matching entry is made",
                   TimeValue (Seconds (100)),
                   MakeTimeAccessor (&InrppCache::m_deadTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("WaitReplyTimeout",
                   "When this timeout expires, "
                   "the cache entries will be scanned and "
                   "entries in WaitReply state will resend ArpRequest "
                   "unless MaxRetries has been exceeded, "
                   "in which case the entry is marked dead",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&InrppCache::m_waitReplyTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MaxRetries",
                   "Number of retransmissions of ArpRequest "
                   "before marking dead",
                   UintegerValue (3),
                   MakeUintegerAccessor (&InrppCache::m_maxRetries),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("PendingQueueSize",
                   "The size of the queue for packets pending an arp reply.",
                   UintegerValue (3),
                   MakeUintegerAccessor (&InrppCache::m_pendingQueueSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Drop",
                     "Packet dropped due to InrppCache entry "
                     "in WaitReply expiring.",
                     MakeTraceSourceAccessor (&InrppCache::m_dropTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

InrppCache::InrppCache ()
  : m_device (0), 
    m_interface (0)
{
  NS_LOG_FUNCTION (this);
}

InrppCache::~InrppCache ()
{
  NS_LOG_FUNCTION (this);
}

void 
InrppCache::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Flush ();
  m_device = 0;
  m_interface = 0;
  if (!m_waitReplyTimer.IsRunning ())
    {
      Simulator::Remove (m_waitReplyTimer);
    }
  Object::DoDispose ();
}

void
InrppCache::SetDevice (Ptr<NetDevice> device, Ptr<Ipv4Interface> interface)
{
  NS_LOG_FUNCTION (this << device << interface);
  m_device = device;
  m_interface = interface;
}

Ptr<NetDevice>
InrppCache::GetDevice (void) const
{
  NS_LOG_FUNCTION (this);
  return m_device;
}

Ptr<Ipv4Interface>
InrppCache::GetInterface (void) const
{
  NS_LOG_FUNCTION (this);
  return m_interface;
}

void 
InrppCache::SetAliveTimeout (Time aliveTimeout)
{
  NS_LOG_FUNCTION (this << aliveTimeout);
  m_aliveTimeout = aliveTimeout;
}
void 
InrppCache::SetDeadTimeout (Time deadTimeout)
{
  NS_LOG_FUNCTION (this << deadTimeout);
  m_deadTimeout = deadTimeout;
}
void 
InrppCache::SetWaitReplyTimeout (Time waitReplyTimeout)
{
  NS_LOG_FUNCTION (this << waitReplyTimeout);
  m_waitReplyTimeout = waitReplyTimeout;
}

Time
InrppCache::GetAliveTimeout (void) const
{
  NS_LOG_FUNCTION (this);
  return m_aliveTimeout;
}
Time
InrppCache::GetDeadTimeout (void) const
{
  NS_LOG_FUNCTION (this);
  return m_deadTimeout;
}
Time
InrppCache::GetWaitReplyTimeout (void) const
{
  NS_LOG_FUNCTION (this);
  return m_waitReplyTimeout;
}

void 
InrppCache::SetArpRequestCallback (Callback<void, Ptr<const InrppCache>,
                                          Ipv4Address> arpRequestCallback)
{
  NS_LOG_FUNCTION (this << &arpRequestCallback);
  m_arpRequestCallback = arpRequestCallback;
}

void 
InrppCache::StartWaitReplyTimer (void)
{
  NS_LOG_FUNCTION (this);
  if (!m_waitReplyTimer.IsRunning ())
    {
      NS_LOG_LOGIC ("Starting WaitReplyTimer at " << Simulator::Now () << " for " <<
                    m_waitReplyTimeout);
      m_waitReplyTimer = Simulator::Schedule (m_waitReplyTimeout, 
                                              &InrppCache::HandleWaitReplyTimeout, this);
    }
}

void
InrppCache::HandleWaitReplyTimeout (void)
{
  NS_LOG_FUNCTION (this);
  InrppCache::Entry* entry;
  bool restartWaitReplyTimer = false;
  for (CacheI i = m_InrppCache.begin (); i != m_InrppCache.end (); i++)
    {
      entry = (*i).second;
      if (entry != 0 && entry->IsWaitReply ())
        {
          if (entry->GetRetries () < m_maxRetries)
            {
              NS_LOG_LOGIC ("node="<< m_device->GetNode ()->GetId () <<
                            ", ArpWaitTimeout for " << entry->GetIpv4Address () <<
                            " expired -- retransmitting arp request since retries = " <<
                            entry->GetRetries ());
              m_arpRequestCallback (this, entry->GetIpv4Address ());
              restartWaitReplyTimer = true;
              entry->IncrementRetries ();
            }
          else
            {
              NS_LOG_LOGIC ("node="<<m_device->GetNode ()->GetId () <<
                            ", wait reply for " << entry->GetIpv4Address () <<
                            " expired -- drop since max retries exceeded: " <<
                            entry->GetRetries ());
              entry->MarkDead ();
              entry->ClearRetries ();
              Ptr<Packet> pending = entry->DequeuePending ();
              while (pending != 0)
                {
                  m_dropTrace (pending);
                  pending = entry->DequeuePending ();
                }
            }
        }

    }
  if (restartWaitReplyTimer)
    {
      NS_LOG_LOGIC ("Restarting WaitReplyTimer at " << Simulator::Now ().GetSeconds ());
      m_waitReplyTimer = Simulator::Schedule (m_waitReplyTimeout, 
                                              &InrppCache::HandleWaitReplyTimeout, this);
    }
}

void 
InrppCache::Flush (void)
{
  NS_LOG_FUNCTION (this);
  for (CacheI i = m_InrppCache.begin (); i != m_InrppCache.end (); i++)
    {
      delete (*i).second;
    }
  m_InrppCache.erase (m_InrppCache.begin (), m_InrppCache.end ());
  if (m_waitReplyTimer.IsRunning ())
    {
      NS_LOG_LOGIC ("Stopping WaitReplyTimer at " << Simulator::Now ().GetSeconds () << " due to InrppCache flush");
      m_waitReplyTimer.Cancel ();
    }
}

void
InrppCache::PrintInrppCache (Ptr<OutputStreamWrapper> stream)
{
  NS_LOG_FUNCTION (this << stream);
  std::ostream* os = stream->GetStream ();

  for (CacheI i = m_InrppCache.begin (); i != m_InrppCache.end (); i++)
    {
      *os << i->first << " dev ";
      std::string found = Names::FindName (m_device);
      if (Names::FindName (m_device) != "")
        {
          *os << found;
        }
      else
        {
          *os << static_cast<int> (m_device->GetIfIndex ());
        }

      *os << " lladdr " << i->second->GetMacAddress ();

      if (i->second->IsAlive ())
        {
          *os << " REACHABLE\n";
        }
      else if (i->second->IsWaitReply ())
        {
          *os << " DELAY\n";
        }
      else
        {
          *os << " STALE\n";
        }
    }
}

InrppCache::Entry *
InrppCache::Lookup (Ipv4Address to)
{
  NS_LOG_FUNCTION (this << to);
  if (m_InrppCache.find (to) != m_InrppCache.end ())
    {
      InrppCache::Entry *entry = m_InrppCache[to];
      return entry;
    }
  return 0;
}

InrppCache::Entry *
InrppCache::Add (Ipv4Address to)
{
  NS_LOG_FUNCTION (this << to);
  NS_ASSERT (m_InrppCache.find (to) == m_InrppCache.end ());

  InrppCache::Entry *entry = new InrppCache::Entry (this);
  m_InrppCache[to] = entry;
  entry->SetIpv4Address (to);
  return entry;
}

InrppCache::Entry::Entry (InrppCache *arp)
  : m_arp (arp),
    m_state (ALIVE),
    m_retries (0)
{
  NS_LOG_FUNCTION (this << arp);
}


bool 
InrppCache::Entry::IsDead (void)
{
  NS_LOG_FUNCTION (this);
  return (m_state == DEAD) ? true : false;
}
bool 
InrppCache::Entry::IsAlive (void)
{
  NS_LOG_FUNCTION (this);
  return (m_state == ALIVE) ? true : false;
}
bool
InrppCache::Entry::IsWaitReply (void)
{
  NS_LOG_FUNCTION (this);
  return (m_state == WAIT_REPLY) ? true : false;
}


void 
InrppCache::Entry::MarkDead (void)
{
  NS_LOG_FUNCTION (this);
  m_state = DEAD;
  ClearRetries ();
  UpdateSeen ();
}
void
InrppCache::Entry::MarkAlive (Address macAddress)
{
  NS_LOG_FUNCTION (this << macAddress);
  NS_ASSERT (m_state == WAIT_REPLY);
  m_macAddress = macAddress;
  m_state = ALIVE;
  ClearRetries ();
  UpdateSeen ();
}

bool
InrppCache::Entry::UpdateWaitReply (Ptr<Packet> waiting)
{
  NS_LOG_FUNCTION (this << waiting);
  NS_ASSERT (m_state == WAIT_REPLY);
  /* We are already waiting for an answer so
   * we dump the previously waiting packet and
   * replace it with this one.
   */
  if (m_pending.size () >= m_arp->m_pendingQueueSize)
    {
      return false;
    }
  m_pending.push_back (waiting);
  return true;
}
void 
InrppCache::Entry::MarkWaitReply (Ptr<Packet> waiting)
{
  NS_LOG_FUNCTION (this << waiting);
  NS_ASSERT (m_state == ALIVE || m_state == DEAD);
  NS_ASSERT (m_pending.empty ());
  m_state = WAIT_REPLY;
  m_pending.push_back (waiting);
  UpdateSeen ();
  m_arp->StartWaitReplyTimer ();
}

Address
InrppCache::Entry::GetMacAddress (void) const
{
  NS_LOG_FUNCTION (this);
  return m_macAddress;
}
Ipv4Address 
InrppCache::Entry::GetIpv4Address (void) const
{
  NS_LOG_FUNCTION (this);
  return m_ipv4Address;
}
void 
InrppCache::Entry::SetIpv4Address (Ipv4Address destination)
{
  NS_LOG_FUNCTION (this << destination);
  m_ipv4Address = destination;
}
Time
InrppCache::Entry::GetTimeout (void) const
{
  NS_LOG_FUNCTION (this);
  switch (m_state) {
    case InrppCache::Entry::WAIT_REPLY:
      return m_arp->GetWaitReplyTimeout ();
    case InrppCache::Entry::DEAD:
      return m_arp->GetDeadTimeout ();
    case InrppCache::Entry::ALIVE:
      return m_arp->GetAliveTimeout ();
    default:
      NS_ASSERT (false);
      return Seconds (0);
      /* NOTREACHED */
    }
}
bool 
InrppCache::Entry::IsExpired (void) const
{
  NS_LOG_FUNCTION (this);
  Time timeout = GetTimeout ();
  Time delta = Simulator::Now () - m_lastSeen;
  NS_LOG_DEBUG ("delta=" << delta.GetSeconds () << "s");
  if (delta > timeout) 
    {
      return true;
    } 
  return false;
}
Ptr<Packet> 
InrppCache::Entry::DequeuePending (void)
{
  NS_LOG_FUNCTION (this);
  if (m_pending.empty ())
    {
      return 0;
    }
  else
    {
      Ptr<Packet> p = m_pending.front ();
      m_pending.pop_front ();
      return p;
    }
}
void 
InrppCache::Entry::UpdateSeen (void)
{
  NS_LOG_FUNCTION (this);
  m_lastSeen = Simulator::Now ();
}
uint32_t
InrppCache::Entry::GetRetries (void) const
{
  NS_LOG_FUNCTION (this);
  return m_retries;
}
void
InrppCache::Entry::IncrementRetries (void)
{
  NS_LOG_FUNCTION (this);
  m_retries++;
  UpdateSeen ();
}
void
InrppCache::Entry::ClearRetries (void)
{
  NS_LOG_FUNCTION (this);
  m_retries = 0;
}

} // namespace ns3

