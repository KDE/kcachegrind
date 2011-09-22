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

/*
 * Source View
 */

#ifndef SOURCEVIEW_H
#define SOURCEVIEW_H

#include <QTreeWidget>
#include "traceitemview.h"

class SourceItem;

class SourceView : public QTreeWidget, public TraceItemView
{
  friend class SourceItem;

  Q_OBJECT

public:
  explicit SourceView(TraceItemView* parentView,
		      QWidget* parent = 0);

  QWidget* widget() { return this; }
  QString whatsThis() const;
  int arrowLevels() { return _arrowLevels; }

protected slots:
  void context(const QPoint &);
  void selectedSlot(QTreeWidgetItem*, QTreeWidgetItem*);
  void activatedSlot(QTreeWidgetItem*,int);
  void headerClicked(int);

protected:
  void keyPressEvent(QKeyEvent* event);

private:
  CostItem* canShow(CostItem*);
  void doUpdate(int, bool);
  void refresh();
  void updateJumpArray(uint,SourceItem*,bool,bool);
  bool searchFile(QString&, TraceFunctionSource*);
  void fillSourceFile(TraceFunctionSource*, int);

  bool _inSelectionUpdate;

  // arrows
  int _arrowLevels;
  // temporary needed on creation...
  QVector<TraceLineJump*> _jump;
  TraceLineJumpList _lowList, _highList;
  TraceLineJumpList::iterator _lowListIter, _highListIter;
};

#endif
