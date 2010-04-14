/* This file is part of KCachegrind.
   Copyright (C) 2002-2004 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#ifndef SUBCOST_H
#define SUBCOST_H

#include <QVector>
#include <QList>

#include "utils.h"

typedef unsigned long long uint64;

/**
 * Cost event counter, simple wrapper around a 64bit entity
 */
class SubCost
{
 public:
    SubCost() { v=0; }
    SubCost(uint64 i) { v=i; }
    SubCost(unsigned i) { v=i; }
    SubCost(int i) { v=(unsigned)i; }
    SubCost(double d) { v= (uint64)(d + .5); }

    SubCost& operator=(uint64 i) { v = i; return *this; }
    SubCost& operator=(unsigned i) { v = i; return *this; }
    SubCost& operator=(int i) { v = i; return *this; }
    SubCost& operator=(double d) { v = (uint64)(d + .5); return *this; }

    bool set(const char** s);
    bool set(FixString& s) { return s.stripUInt64(v); }

    operator uint64&() { return v; }

    bool operator==(unsigned i) const { return v == i; }
    bool operator==(int i) const { return v == (unsigned)i; }
    bool operator<(unsigned i) const { return v < i; }
    bool operator<(int i) const { return v < (unsigned)i; }
    bool operator<(const SubCost& s) const { return v < s.v; }
    bool operator>(unsigned i) const { return v > i; }
    bool operator>(int i) const { return v > (unsigned)i; }
    bool operator>(const SubCost& s) const { return v > s.v; }

    /**
     * Convert SubCost value into a QString,
     * spaced every 3 digits.
     */
    QString pretty(char sep = ' ') const;

    uint64 v;
};

class ProfileCostArray;
class EventType;
typedef QList<ProfileCostArray*> TraceCostList;

/**
 * A class to calculate the <maxSize> ProfileCostArray items
 * with highest cost.
 */

class HighestCostList
{
 public:
    HighestCostList();

    void clear(int maxSize);
    void addCost(ProfileCostArray*, SubCost);
    int count() { return _count; }
    int realCount() { return (_count > _maxSize) ? _maxSize:_count; }
    int maxSize() { return _maxSize; }
    bool hasMore() { return _count > _maxSize; }
    ProfileCostArray* operator[] (int i)
	{ return (i>=0 && i<_count && i<_maxSize) ? _item[i] : 0; }

 private:
    TraceCostList _list;
    int _maxSize, _count;
    EventType* _costType;
    QVector<ProfileCostArray*> _item;
    QVector<SubCost> _cost;
};


#endif
