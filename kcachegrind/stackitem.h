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
 * Items of stack dockable.
 */

#ifndef STACKITEM_H
#define STACKITEM_H

#include <qlistview.h>
#include "tracedata.h"


// for the stack browser

class StackItem: public QListViewItem
{
public:
  StackItem(QListView* parent, TraceFunction* f,
            TraceCostType* ct, TraceCost::CostType); // for top
  StackItem(QListView* parent, TraceCall* c,
            TraceCostType* ct, TraceCost::CostType);

  TraceFunction* function() { return _function; }
  TraceCall* call() { return _call; }
  void setCostType(TraceCostType*);
  void setGroupType(TraceCost::CostType);
  void update();

private:
  SubCost _sum;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  TraceFunction* _function;
  TraceCall* _call;
};



#endif
