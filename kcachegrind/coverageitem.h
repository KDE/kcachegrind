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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Items of coverage view.
 */

#ifndef COVERAGEITEM_H
#define COVERAGEITEM_H

#include <qlistview.h>
#include "tracedata.h"

class Coverage;

class CallerCoverageItem: public QListViewItem
{
public:
  CallerCoverageItem(QListView* parent, Coverage* c, TraceFunction* base,
                     TraceCostType* ct, TraceCost::CostType gt);
  CallerCoverageItem(QListView* parent, int skipped, Coverage* c, TraceFunction* base,
                     TraceCostType* ct, TraceCost::CostType gt);

  int compare(QListViewItem * i, int col, bool ascending ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

private:
  float _pSum;
  SubCost _sum;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};


class CalleeCoverageItem: public QListViewItem
{
public:
  CalleeCoverageItem(QListView* parent, Coverage* c, TraceFunction* base,
                     TraceCostType* ct, TraceCost::CostType gt);
  CalleeCoverageItem(QListView* parent, int skipped, Coverage* c, TraceFunction* base,
                     TraceCostType* ct, TraceCost::CostType gt);

  int compare(QListViewItem * i, int col, bool ascending ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

private:
  float _pSum, _pSelf;
  SubCost _sum, _self;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};

#endif
