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

#ifndef PARTLISTITEM_H
#define PARTLISTITEM_H

#include <qlistview.h>
#include "tracedata.h"

/**
 * For info tab, trace part list.
 * Needs update on
 * - cost type change
 *
 * Note: on a cost item / percentage change, the list is rebuild
 */
class PartListItem: public QListViewItem
{
public:
  PartListItem(QListView* parent, TraceCostItem* costItem,
               TraceCostType* ct, TraceCost::CostType gt, TracePart* part);

  int compare(QListViewItem * i, int col, bool ascending ) const;
  TraceCost* partCostItem() { return _partCostItem; }
  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType);
  TracePart* part() { return _part; }
  void update();

private:
  SubCost _sum, _pure;
  SubCost _callers;
  TraceCostType* _costType;
  TraceCost* _partCostItem;
  TracePart* _part;
  TraceCost::CostType _groupType;
};

#endif
