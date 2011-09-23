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
 * Part View
 */

#ifndef PARTVIEW_H
#define PARTVIEW_H

#include <QTreeWidget>

#include "tracedata.h"
#include "traceitemview.h"

class PartView: public QTreeWidget, public TraceItemView
{
  Q_OBJECT

public:
  explicit PartView(TraceItemView* parentView, QWidget* parent=0);

  virtual QWidget* widget() { return this; }
  QString whatsThis() const;

  void refresh();

private slots:
  void context(const QPoint &);
  void selectionChangedSlot();
  void headerClicked(int);

private:
  CostItem* canShow(CostItem*);
  void doUpdate(int, bool);

  bool _inSelectionUpdate;
};

#endif
