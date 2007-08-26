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

#include <q3listview.h>
//Added by qt3to4:
#include <Q3MemArray>
#include "traceitemview.h"

class SourceItem;

class SourceView : public Q3ListView, public TraceItemView
{
  friend class SourceItem;

  Q_OBJECT

public:
  explicit SourceView(TraceItemView* parentView,
		      QWidget* parent = 0, const char* name = 0);

  QWidget* widget() { return this; }
  QString whatsThis() const;

protected:
  int arrowLevels() { return _arrowLevels; }
  void paintEmptyArea( QPainter *, const QRect & );

private slots:
  void context(Q3ListViewItem*, const QPoint &, int);
  void selectedSlot(Q3ListViewItem *);
  void activatedSlot(Q3ListViewItem *);

private:
  TraceItem* canShow(TraceItem*);
  void doUpdate(int);
  void refresh();
  void updateJumpArray(uint,SourceItem*,bool,bool);
  void fillSourceFile(TraceFunctionSource*, int);
  void updateSourceItems();

  bool _inSelectionUpdate;

  // arrows
  int _arrowLevels;
  // temporary needed on creation...
  Q3MemArray<TraceLineJump*> _jump;
  TraceLineJumpList _lowList, _highList;
};

#endif
