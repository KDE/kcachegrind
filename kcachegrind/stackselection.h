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
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#ifndef STACKSELECTION_H
#define STACKSELECTION_H

#include "stackselectionbase.h"
#include "tracedata.h"

class TraceFunction;
class TraceData;
class StackBrowser;
class NestedAreaItem;

class StackSelection : public StackSelectionBase
{
  Q_OBJECT

public:
  StackSelection( QWidget* parent = 0, const char* name = 0);
  ~StackSelection();

  TraceData* data() const { return _data; }
  void setData(TraceData*);
  StackBrowser* browser() const { return _browser; }
  TraceCostType* costType() { return _costType; }
  TraceCostType* costType2() { return _costType2; }
  TraceItem::CostType groupType() { return _groupType; }

signals:
  void functionSelected(TraceItem*);

public slots:
  void setFunction(TraceFunction*);
  void setCostType(TraceCostType*);
  void setCostType2(TraceCostType*);
  void setGroupType(TraceItem::CostType);

  void stackSelected( QListViewItem* );
  void browserBack();
  void browserForward();
  void browserUp();
  void browserDown();
  void refresh();
  void rebuildStackList();

private:
  void selectFunction();

  TraceData* _data;
  StackBrowser* _browser;
  QListViewItem* _item;
  TraceFunction* _function;
  TraceCostType* _costType;
  TraceCostType* _costType2;
  TraceItem::CostType _groupType;
};

#endif
