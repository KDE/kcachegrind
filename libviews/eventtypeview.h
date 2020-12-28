/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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

private Q_SLOTS:
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
