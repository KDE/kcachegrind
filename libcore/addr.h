/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

   KCachegrind is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef ADDR_H
#define ADDR_H

#include <QString>

#include "utils.h"

/**
 * Addresses are 64bit values like costs to be able
 * to always load profile data produced on 64bit
 * architectures.
 */
class Addr
{
 public:
  Addr() { _v=0; }
  Addr(uint64 v) { _v = v; }

  // Interpretes char data at s as hex (without "0x" prefix)
  // and return number of interpreted chars.
  int set(const char *s);
  bool set(FixString& s);
  QString toString() const;
  // similar to toString(), but adds a space every 4 digits
  QString pretty() const;

  // returns true if this address is in [a-distance;a+distance]
  bool isInRange(Addr a, int distance);

  bool operator==(const Addr& a) const { return (_v == a._v); }
  bool operator!=(const Addr& a) const { return (_v != a._v); }
  bool operator>(const Addr& a) const { return _v > a._v; }
  bool operator>=(const Addr& a) const { return _v >= a._v; }
  bool operator<(const Addr& a) const { return _v < a._v; }
  bool operator<=(const Addr& a) const { return _v <= a._v; }

  Addr operator+(int d) const { return Addr(_v + d); }
  Addr operator-(int d) const { return Addr(_v - d); }

 private:
  uint64 _v;
};

#endif // ADDR_H
