/* This file is part of KCachegrind.
   Copyright (C) 2002, 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * For function selection, to be put into a QDockWindow
 */

#ifndef FUNCTIONSELECTION_H
#define FUNCTIONSELECTION_H

#include "functionselectionbase.h"
#include "traceitemview.h"
#include "tracedata.h"
#include "listutils.h"

class TraceFunction;
class TraceData;
class StackBrowser;
class NestedAreaItem;

class FunctionSelection : public FunctionSelectionBase, public TraceItemView
{
  Q_OBJECT

public:
  FunctionSelection( TopLevel*, QWidget* parent = 0, const char* name = 0);
  ~FunctionSelection();

  TraceCostItem* group(QString);
  void setGroup(TraceCostItem*);
  void query(QString);
  bool setTopFunction();

  QWidget* widget() { return this; }

public slots:
  void groupDoubleClicked( QListViewItem* );
  void functionActivated( QListViewItem* );
  void groupSelected( QListViewItem* );
  void functionSelected( QListViewItem* );
  void functionContext(QListViewItem*, const QPoint &, int);
  void groupContext(QListViewItem*, const QPoint &, int);

private:
  TraceItem* canShow(TraceItem* i);
  void doUpdate(int);
  void selectFunction();
  void refresh();

  TraceCostItem* _group;

  HighestCostList _hc;
  // when setting a
  bool _inSetCostItem, _inSetFunction;
};

#endif
