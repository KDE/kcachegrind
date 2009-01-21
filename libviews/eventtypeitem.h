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
 * Items of event type view.
 */

#ifndef EVENTTYPEITEM_H
#define EVENTTYPEITEM_H

#include <q3listview.h>
#include "tracedata.h"

class EventTypeItem: public Q3ListViewItem
{
public:
  EventTypeItem(Q3ListView* parent, TraceCostItem* costItem,
               TraceEventType* ct, TraceCost::CostType gt);

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;
  void setGroupType(TraceCost::CostType);
  TraceCostItem* costItem() { return _costItem; }
  TraceEventType* eventType() { return _eventType; }
  void update();

private:
  SubCost _sum, _pure;
  TraceEventType* _eventType;
  TraceCostItem* _costItem;
  TraceCost::CostType _groupType;
};

#endif // EVENTTYPEITEM_H
