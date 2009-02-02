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

#ifndef PARTLISTITEM_H
#define PARTLISTITEM_H

#include <q3listview.h>
#include "tracedata.h"

/**
 * For info tab, trace part list.
 * Needs update on
 * - cost type change
 *
 * Note: on a cost item / percentage change, the list is rebuild
 */
class PartListItem: public Q3ListViewItem
{
public:
  PartListItem(Q3ListView* parent, TraceCostItem* costItem,
               EventType* ct, ProfileContext::Type gt, TracePart* part);

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;
  ProfileCostArray* partCostItem() { return _partCostItem; }
  void setCostType(EventType* ct);
  void setGroupType(ProfileContext::Type);
  TracePart* part() { return _part; }
  void update();

private:
  SubCost _sum, _pure;
  SubCost _callers;
  EventType* _costType;
  ProfileCostArray* _partCostItem;
  TracePart* _part;
  ProfileContext::Type _groupType;
};

#endif
