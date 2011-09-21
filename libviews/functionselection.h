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
#include <QWidget>
#include <QModelIndex>

#include "tracedata.h"
#include "traceitemview.h"
#include "listutils.h"
#include "toplevelbase.h"

class QAction;
class QMenu;
class QLabel;
class QComboBox;
class QLineEdit;
class QTreeView;
class QTreeWidget;
class QTreeWidgetItem;
class FunctionListModel;

class FunctionSelection: public QWidget, public TraceItemView
{
  Q_OBJECT

public:
  explicit FunctionSelection(TopLevelBase*, QWidget* parent = 0);

  TraceCostItem* group() { return _group; }
  TraceCostItem* group(QString);
  void setGroup(TraceCostItem*);
  void query(QString);
  bool selectTopFunction();

  QWidget* widget() { return this; }
  QString whatsThis() const;
  void setData(TraceData*);

  void addGroupMenu(QMenu*);

public slots:
  void searchReturnPressed();
  void searchChanged(const QString&);
  void queryDelayed();

  void groupTypeSelected(QAction*);
  void groupTypeSelected(int);
  void groupDoubleClicked(QTreeWidgetItem*, int);
  void groupSelected(QTreeWidgetItem*, QTreeWidgetItem*);
  void groupContext(const QPoint &);
  void groupHeaderClicked(int);

  void functionActivated(const QModelIndex&);
  void functionContext(const QPoint &);
  void functionHeaderClicked(int);

private:
  CostItem* canShow(CostItem* i);
  void doUpdate(int, bool);
  void selectFunction();
  void refresh();
  void setCostColumnWidths();
  void updateGroupSizes(bool hideEmpty);
  void addGroupAction(QMenu*, ProfileContext::Type,
		      const QString& s = QString());
  void selectFunction(TraceFunction* f, bool ensureVisible = true);

  TraceCostItem* _group;

  QString _searchString, _searchDelayed;
  QTimer _searchTimer;
  QMap<TraceCostItem*,int> _groupSize;

  HighestCostList _hc;
  // when setting a
  bool _inSetGroup, _inSetFunction;

  QLabel *searchLabel;
  QLineEdit *searchEdit;
  QComboBox *groupBox;
  QTreeWidget *groupList;
  QTreeView *functionList;
  FunctionListModel* functionListModel;

  Qt::SortOrder _functionListSortOrder;
};

#endif
