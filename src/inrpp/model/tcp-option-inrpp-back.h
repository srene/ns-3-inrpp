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
  /**
   * \brief Get the timestamp echo stored in the Option
   * \return the timestamp echo
   */
  uint32_t GetDeltaRate (void) const;

  uint8_t GetFlag (void) const;
  /**
   * \brief Set the timestamp stored in the Option
   * \param ts the timestamp
   */
  void SetNonce (uint32_t nonce);
  /**
   * \brief Set the timestamp echo stored in the Option
   * \param ts the timestamp echo
   */
  void SetDeltaRate (uint32_t deltaRate);

  void SetFlag (uint8_t flag);

  /**
    * \brief Return an uint32_t value which represent "now"
    *
    * The value returned is usually used as Timestamp option for the
    * TCP header; when the value will be echoed back, calculating the RTT
    * will be an easy matter.
    *
    * The RFC does not mention any units for this value; following what
    * is implemented in OS, we use milliseconds. Any change to this must be
    * reflected to EstimateRttFromTs.
    *
    * \see EstimateRttFromTs
    * \return The Timestamp value to use
    */
  //static uint32_t NowToTsValue ();

  /**
   * \brief Estimate the Time elapsed from a TS echo value
   *
   * The echoTime should be a value returned from NowToTsValue.
   *
   * \param echoTime Echoed value from other side
   * \see NowToTsValue
   * \return The measured RTT
   */
  //static Time ElapsedTimeFromTsValue (uint32_t echoTime);

protected:
  uint8_t m_flag;     //!< Header destination address
  uint32_t m_nonce;
  uint32_t m_deltaRate;
};

} // namespace ns3

#endif /* TCP_OPTION_INRPP_BACK_H */
