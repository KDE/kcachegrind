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

#ifndef COSTLISTITEM_H
#define COSTLISTITEM_H

#include <qlistview.h>
#include "tracedata.h"

class CostListItem: public QListViewItem
{
public:
  CostListItem(QListView* parent, TraceCostItem* cost,
               TraceCostType* ct);
  // entry with multiple skipped items
  CostListItem(QListView* parent, int skipped, TraceCostItem* cost,
               TraceCostType* ct);

  int compare(QListViewItem * i, int col, bool ascending ) const;
  TraceCostItem* costItem() { return (_skipped) ? 0 : _costItem; }
  void setCostType(TraceCostType* ct);
  void update();

private:
  SubCost _pure;
  TraceCostType* _costType;
  TraceCostItem* _costItem;
  int _skipped;
};

#endif
