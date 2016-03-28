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

#ifndef TCP_OPTION_RCP_H
#define TCP_OPTION_RCP_H

#include "ns3/tcp-option.h"
#include "ns3/timer.h"

namespace ns3 {

/**
 * Defines the TCP option of kind 8 (timestamp option) as in \RFC{1323}
 */

class TcpOptionRcp : public TcpOption
{
public:
  TcpOptionRcp ();
  virtual ~TcpOptionRcp ();

  typedef enum
  {
	  RCP_OTHER = 0,   //!< No flags
	  RCP_REF  = 1,   //!< Reset
	  RCP_REFACK  = 2,   //!< Push
	  RCP_DATA  = 4,  //!< Ack
	  RCP_ACK  = 8,  //!< Urgent
	  RCP_SYN = 16,
	  RCP_SYNACK = 32,
	  RCP_FIN = 64,
	  RCP_FINACK = 128
  } Flags_t;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

  virtual uint8_t GetKind (void) const;
  virtual uint32_t GetSerializedSize (void) const;

  uint8_t GetFlag (void) const;

  void SetFlag (uint8_t flag);

  uint32_t GetReceivedPackets (void) const;

  void SetReceivedPackets (uint32_t pkts);

  uint32_t GetRate (void) const;

  void SetRate (uint32_t rate);

protected:
  uint8_t m_flag;     //!< Header destination address
  uint32_t m_rcpRate;  // in bytes per second
  uint32_t m_pktsRcvd;
};

} // namespace ns3

#endif /* TCP_OPTION_RCP_H */
