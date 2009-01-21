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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * For function selection, to be put into a QDockWindow
 */

#ifndef FUNCTIONSELECTION_H
#define FUNCTIONSELECTION_H

#include <QTimer>

#include "ui_functionselectionbase.h"
#include "traceitemview.h"
#include "tracedata.h"
#include "listutils.h"
#include "toplevel.h"

class Q3PopupMenu;



class FunctionSelectionBase : public QWidget, public Ui::FunctionSelectionBase
{
public:
  FunctionSelectionBase( QWidget *parent ) : QWidget( parent ) {
    setupUi( this );
  }
};


class FunctionSelection : public FunctionSelectionBase, public TraceItemView
{
  Q_OBJECT

public:
  explicit FunctionSelection(TopLevel*, QWidget* parent = 0);
  ~FunctionSelection();

  TraceCostItem* group(QString);
  void setGroup(TraceCostItem*);
  void query(QString);
  bool setTopFunction();

  QWidget* widget() { return this; }

  void addGroupMenu(Q3PopupMenu*);

public slots:
  void searchReturnPressed();
  void searchChanged(const QString&);
  void queryDelayed();
  void groupDoubleClicked( Q3ListViewItem* );
  void functionActivated( Q3ListViewItem* );
  void groupSelected( Q3ListViewItem* );
  void functionSelected( Q3ListViewItem* );
  void functionContext(Q3ListViewItem*, const QPoint &, int);
  void groupContext(Q3ListViewItem*, const QPoint &, int);

private:
  TraceItem* canShow(TraceItem* i);
  void doUpdate(int);
  void selectFunction();
  void refresh();
  void setCostColumnWidths();
  void updateGroupSizes(bool hideEmpty);

  TraceCostItem* _group;

  QString _searchString, _searchDelayed;
  QTimer _searchTimer;
  QMap<TraceCostItem*,int> _groupSize;

  HighestCostList _hc;
  // when setting a
  bool _inSetGroup, _inSetFunction;
};

#endif
