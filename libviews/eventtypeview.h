/* This file is part of KCachegrind.
   Copyright (c) 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Event Type View
 */

#ifndef EVENTTYPEVIEW_H
#define EVENTTYPEVIEW_H

#include <QTreeWidget>

#include "tracedata.h"
#include "traceitemview.h"

class EventTypeView: public QTreeWidget, public TraceItemView
{
    Q_OBJECT

public:
    explicit EventTypeView(TraceItemView* parentView,
                           QWidget* parent, const QString& name);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;

private slots:
    void context(const QPoint&);
    void currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void itemDoubleClicked(QTreeWidgetItem*, int);
    void itemChanged(QTreeWidgetItem*, int);

private:
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;
    void refresh();
};

#endif
