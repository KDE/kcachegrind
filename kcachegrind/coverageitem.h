/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

/*
 * Items of coverage view.
 */

#ifndef COVERAGEITEM_H
#define COVERAGEITEM_H

#include <q3listview.h>
#include "tracedata.h"

class Coverage;

class CallerCoverageItem: public Q3ListViewItem
{
public:
  CallerCoverageItem(Q3ListView* parent, Coverage* c, TraceFunction* base,
                     TraceEventType* ct, TraceCost::CostType gt);
  CallerCoverageItem(Q3ListView* parent, int skipped, Coverage* c, TraceFunction* base,
                     TraceEventType* ct, TraceCost::CostType gt);

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(TraceEventType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

private:
  float _pSum;
  SubCost _sum;
  TraceEventType* _costType;
  TraceCost::CostType _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};


class CalleeCoverageItem: public Q3ListViewItem
{
public:
  CalleeCoverageItem(Q3ListView* parent, Coverage* c, TraceFunction* base,
                     TraceEventType* ct, TraceCost::CostType gt);
  CalleeCoverageItem(Q3ListView* parent, int skipped, Coverage* c, TraceFunction* base,
                     TraceEventType* ct, TraceCost::CostType gt);

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(TraceEventType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

private:
  float _pSum, _pSelf;
  SubCost _sum, _self;
  TraceEventType* _costType;
  TraceCost::CostType _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};

#endif
