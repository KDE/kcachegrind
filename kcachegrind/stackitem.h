/* This file is part of KCachegrind.
   Copyright (C) 2003, 2004
   Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

class StackSelection;


// for the stack browser

class StackItem: public QListViewItem
{
public:
  // for top
  StackItem(StackSelection* ss, QListView* parent, TraceFunction* f);
  StackItem(StackSelection* ss, QListView* parent, TraceCall* c);

  TraceFunction* function() { return _function; }
  TraceCall* call() { return _call; }
  void updateGroup();
  void updateCost();

private:
  StackSelection* _view;
  SubCost _sum;
  TraceFunction* _function;
  TraceCall* _call;
};



#endif
