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
 * Call Views
 */

#ifndef CALLVIEW_H
#define CALLVIEW_H

#include <qlistview.h>
#include "tracedata.h"
#include "traceitemview.h"

class CallView: public QListView, public TraceItemView
{
  Q_OBJECT

public:
  CallView(bool showCallers, TraceItemView* parentView,
	   QWidget* parent=0, const char* name=0);

  virtual QWidget* widget() { return this; }
  QString whatsThis() const;
  bool showCallers() const { return _showCallers; }

private slots:
  void context(QListViewItem*,const QPoint &, int);
  void selectedSlot(QListViewItem*);
  void activatedSlot(QListViewItem*);

private:
  TraceItem* canShow(TraceItem*);
  void doUpdate(int);
  void refresh();

  bool _showCallers;
};

#endif
