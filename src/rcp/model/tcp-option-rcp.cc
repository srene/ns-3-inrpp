/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Adrian Sai-wah Tam
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
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#include "tcp-option-rcp.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpOptionRcp");

NS_OBJECT_ENSURE_REGISTERED (TcpOptionRcp);

TcpOptionRcp::TcpOptionRcp ()
  : TcpOption (),
	m_flag (0)
{
}

TcpOptionRcp::~TcpOptionRcp ()
{
}


TypeId
TcpOptionRcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionRcp")
    .SetParent<TcpOption> ()
    .SetGroupName ("Rcp")
    .AddConstructor<TcpOptionRcp> ()
  ;
  return tid;
}

TypeId
TcpOptionRcp::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
TcpOptionRcp::Print (std::ostream &os) const
{
  os << (uint32_t)m_flag;
}

uint32_t
TcpOptionRcp::GetSerializedSize (void) const
{
  return 11;
}

void
TcpOptionRcp::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (GetKind ()); // Kind
  i.WriteU8 (11); // Length
  i.WriteU8 (m_flag); // Flag
  i.WriteHtonU32(m_rcpRate);
  i.WriteHtonU32(m_pktsRcvd);
}

uint32_t
TcpOptionRcp::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  uint8_t readKind = i.ReadU8 ();
  NS_LOG_LOGIC("Kind " << (uint32_t)readKind);
  if (readKind != GetKind ())
    {
      NS_LOG_WARN ("Malformed Inrpp option");
      return 0;
    }

  uint8_t size = i.ReadU8 ();
  if (size != 11)
    {
      NS_LOG_WARN ("Malformed Inrpp option");
      return 0;
    }
  m_flag = i.ReadU8();
  m_rcpRate = i.ReadNtohU32();
  m_pktsRcvd = i.ReadNtohU32();
  return GetSerializedSize ();
}

uint8_t
TcpOptionRcp::GetKind (void) const
{
  return TcpOption::RCP;
}

uint8_t
TcpOptionRcp::GetFlag (void) const
{
  return m_flag;
}

void
TcpOptionRcp::SetFlag (uint8_t flag)
{
	m_flag = flag;
}

uint32_t
TcpOptionRcp::GetReceivedPackets (void) const
{
	return m_pktsRcvd;
}

void
TcpOptionRcp::SetReceivedPackets (uint32_t pkts)
{
	m_pktsRcvd = pkts;
}

uint32_t
TcpOptionRcp::GetRate (void) const
{
	return m_rcpRate;
}

void
TcpOptionRcp::SetRate (uint32_t rate)
{
	m_rcpRate = rate;
}


} // namespace ns3
