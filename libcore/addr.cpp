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

#include "addr.h"

//---------------------------------------------------
// Addr

bool Addr::set(FixString& s)
{
  return s.stripUInt64(_v);
}

int Addr::set(const char *s)
{
  int n = 0;
  _v = 0;

  while((n<16) && *s) {
    if ((*s>='0') && (*s<='9'))
      _v = 16*_v + (*s-'0');
    else if ((*s>='a') && (*s<='f'))
      _v = 16*_v + 10 + (*s-'a');
    else if ((*s>='A') && (*s<='F'))
      _v = 16*_v + 10 + (*s-'A');
    else break;
    s++;
    n++;
  }

  return n;
}


QString Addr::toString() const
{
  if (_v == 0) return QString("0");

  uint64 n = _v;
  QString hex;
  hex.reserve(16);

  while(n>0) {
    int d = (n & 15);
    hex = QChar((d<10) ? ('0'+d) : ('A'-10+d)) + hex;
    n /= 16;
  }

  return hex;
}

QString Addr::pretty() const
{
  if (_v == 0) return QString("0");

  uint64 n = _v;
  int p = 0;
  QString hex;
  hex.reserve(20);

  while(n>0) {
    int d = (n & 15);
    if ((p>0) && ((p%4)==0)) hex = ' ' + hex;
    hex = QChar((d<10) ? ('0'+d) : ('A'-10+d)) + hex;
    n /= 16;
    p++;
  }

  return hex;
}

bool Addr::isInRange(Addr a, int distance)
{
  uint64 diff = (a._v > _v) ? (a._v - _v) : (_v - a._v);
  uint64 dist = (distance<0) ? distance : -distance;
  return (diff < dist);
}
