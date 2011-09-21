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

#ifndef COSTLISTITEM_H
#define COSTLISTITEM_H

#include <QTreeWidget>

#include "tracedata.h"

class CostListItem: public QTreeWidgetItem
{
public:
  CostListItem(QTreeWidget* parent, TraceCostItem* cost,
               EventType* et, int size = -1);
  // entry with multiple skipped items
  CostListItem(QTreeWidget* parent, int skipped, TraceCostItem* cost,
               EventType* et);

  bool operator< ( const QTreeWidgetItem & other ) const;
  TraceCostItem* costItem() { return (_skipped) ? 0 : _costItem; }
  void setEventType(EventType* et);
  void update();
  void setSize(int s);

private:
  void updateName();

  SubCost _pure;
  EventType* _eventType;
  TraceCostItem* _costItem;
  // >0 only for last item in list, if items are skipped 
  int _skipped;
  // number of items in group, is put in parenthesis after name
  int _groupSize;
};

#endif
