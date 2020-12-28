/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
