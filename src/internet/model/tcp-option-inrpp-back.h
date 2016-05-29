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

#ifndef TCP_OPTION_INRPP_BACK_H
#define TCP_OPTION_INRPP_BACK_H

#include "ns3/tcp-option.h"
#include "ns3/timer.h"

namespace ns3 {

/**
 * Defines the TCP option of kind 8 (timestamp option) as in \RFC{1323}
 */

class TcpOptionInrppBack : public TcpOption
{
public:
  TcpOptionInrppBack ();
  virtual ~TcpOptionInrppBack ();

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

  /**
   * \brief Get the timestamp stored in the Option
   * \return the timestamp
   */
  uint32_t GetNonce(void) const;

  uint32_t GetFlag (void) const;
  /**
   * \brief Set the timestamp stored in the Option
   * \param ts the timestamp
   */
  void SetNonce (uint32_t nonce);

  void SetFlag (uint32_t flag);



protected:
  uint32_t m_flag;     //!< Header destination address
  uint32_t m_nonce;
};

} // namespace ns3

#endif /* TCP_OPTION_INRPP_BACK_H */
