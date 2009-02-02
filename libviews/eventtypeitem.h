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
               EventType* ct, ProfileContext::Type gt);

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;
  void setGroupType(ProfileContext::Type);
  TraceCostItem* costItem() { return _costItem; }
  EventType* eventType() { return _eventType; }
  void update();

private:
  SubCost _sum, _pure;
  EventType* _eventType;
  TraceCostItem* _costItem;
  ProfileContext::Type _groupType;
};

#endif // EVENTTYPEITEM_H
