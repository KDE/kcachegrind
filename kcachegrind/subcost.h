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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef SUBCOST_H
#define SUBCOST_H

#include "utils.h"

typedef unsigned long long uint64;

/**
 * Cost event counter, simple wrapper around a 64bit entity
 */
class SubCost
{
 public:
    SubCost() {}
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
    QString pretty();

    uint64 v;
};

class SubCostAllocator;

/*
 * Consumer of a subcost array.
 *
 * The array can be arbitrarily resized when a
 * non-fixed allocator is used.
 * When resizing, the allocator can trigger a
 * defragmentation sweep.
 */
class SubCostOwner
{
 public:
  SubCostOwner(SubCostAllocator*, int count);
  void realloc(SubCostAllocator*, SubCostAllocator*, int count);

  int count() { return _count; }

 protected:
  SubCost* _cost;
  int _count;
};

/**
 * Allocator for multiple arrays of SubCosts.
 * If the allocator is 'fixed', the allocator itself and
 * SubCost arrays enclosed can not be resized.
 * Otherwise, the allocator compactizes and resizes
 * itself as needed (never more than 20% fragmentation).
 */
class SubCostAllocator
{
 public:
  SubCostAllocator(int size, bool fixed);

  SubCost* allocate(SubCostOwner*, int n);
  void free(SubCostOwner*);

 private:
  void defragment();

  int _size, _used;
  bool _fixed;

  /*
   * The data array for fixed allocators is the
   * compact concatenation of SubCost arrays.
   * For non-fixed, the arrays are seperated by
   * pointers to the SubCostOwner. When resizing
   * an array, the old array is freed, and the
   * pointer points to the next array start.
   * These free parts are squweezed in defragment().
   */
  char* _data;

  char* _defragStart;
};

#endif
