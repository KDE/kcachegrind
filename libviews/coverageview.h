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
 * Coverage Views
 */

#ifndef COVERAGEVIEW_H
#define COVERAGEVIEW_H

#include <QTreeWidget>

#include "tracedata.h"
#include "traceitemview.h"
#include "listutils.h"

class CoverageView: public QTreeWidget, public TraceItemView
{
    Q_OBJECT

public:
    CoverageView(bool showCallers, TraceItemView* parentView,
                 QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;

protected Q_SLOTS:
    void context(const QPoint &);
    void selectedSlot(QTreeWidgetItem*, QTreeWidgetItem*);
    void activatedSlot(QTreeWidgetItem*, int);
    void headerClicked(int);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;
    void refresh();

    HighestCostList _hc;
    bool _showCallers;
};

#endif
