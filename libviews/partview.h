/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
    explicit PartView(TraceItemView* parentView, QWidget* parent=nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;

    void refresh();

private Q_SLOTS:
    void context(const QPoint &);
    void selectionChangedSlot();
    void headerClicked(int);

private:
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;

    bool _inSelectionUpdate;
};

#endif
