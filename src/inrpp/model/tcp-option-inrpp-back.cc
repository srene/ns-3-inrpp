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

#include "tcp-option-inrpp-back.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpOptionInrppBack");

NS_OBJECT_ENSURE_REGISTERED (TcpOptionInrppBack);

TcpOptionInrppBack::TcpOptionInrppBack ()
  : TcpOption (),
	m_flag (0),
	m_nonce (0)
{
}

TcpOptionInrppBack::~TcpOptionInrppBack ()
{
}

TypeId
TcpOptionInrppBack::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionInrppBack")
    .SetParent<TcpOption> ()
    .SetGroupName ("Inrpp")
    .AddConstructor<TcpOptionInrppBack> ()
  ;
  return tid;
}

TypeId
TcpOptionInrppBack::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
TcpOptionInrppBack::Print (std::ostream &os) const
{
  os << (uint32_t)m_flag << ";" << m_nonce;
}

uint32_t
TcpOptionInrppBack::GetSerializedSize (void) const
{
  return 7;
}

void
TcpOptionInrppBack::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (GetKind ()); // Kind
  i.WriteU8 (7); // Length
  i.WriteU8 (m_flag); // Flag
  i.WriteHtonU32 (m_nonce); // Nonce
}

uint32_t
TcpOptionInrppBack::Deserialize (Buffer::Iterator start)
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
  if (size != 7)
    {
      NS_LOG_WARN ("Malformed Inrpp option");
      return 0;
    }
  m_flag = i.ReadU8();
  m_nonce = i.ReadNtohU32 ();
  return GetSerializedSize ();
}

uint8_t
TcpOptionInrppBack::GetKind (void) const
{
  return TcpOption::INRPP_BACK;
}

uint32_t
TcpOptionInrppBack::GetNonce(void) const
{
  return m_nonce;
}

uint8_t
TcpOptionInrppBack::GetFlag (void) const
{
  return m_flag;
}

void
TcpOptionInrppBack::SetNonce (uint32_t nonce)
{
  m_nonce = nonce;
}


void
TcpOptionInrppBack::SetFlag (uint8_t flag)
{
  m_flag = flag;
}


} // namespace ns3
