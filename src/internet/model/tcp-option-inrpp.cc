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

#include "tcp-option-inrpp.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpOptionInrpp");

NS_OBJECT_ENSURE_REGISTERED (TcpOptionInrpp);

TcpOptionInrpp::TcpOptionInrpp ()
  : TcpOption (),
	m_nonce (0)
{
}

TcpOptionInrpp::~TcpOptionInrpp ()
{
}


TypeId
TcpOptionInrpp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionInrpp")
    .SetParent<TcpOption> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpOptionInrpp> ()
  ;
  return tid;
}

TypeId
TcpOptionInrpp::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
TcpOptionInrpp::Print (std::ostream &os) const
{
  os << (uint32_t)m_nonce;
}

uint32_t
TcpOptionInrpp::GetSerializedSize (void) const
{
  return 6;
}

void
TcpOptionInrpp::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (GetKind ()); // Kind
  i.WriteU8 (GetSerializedSize()); // Length
  i.WriteHtonU32 (m_nonce); // Nonce
}

uint32_t
TcpOptionInrpp::Deserialize (Buffer::Iterator start)
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
  if (size != GetSerializedSize())
    {
      NS_LOG_WARN ("Malformed Inrpp option");
      return 0;
    }
  m_nonce = i.ReadNtohU32 ();
  return GetSerializedSize ();
}

uint8_t
TcpOptionInrpp::GetKind (void) const
{
  return TcpOption::INRPP;
}

uint32_t
TcpOptionInrpp::GetNonce(void) const
{
  return m_nonce;
}


void
TcpOptionInrpp::SetNonce (uint32_t nonce)
{
  m_nonce = nonce;
}


} // namespace ns3
