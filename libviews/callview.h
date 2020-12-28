/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Call Views
 */

#ifndef CALLVIEW_H
#define CALLVIEW_H

#include <QTreeWidget>
#include "tracedata.h"
#include "traceitemview.h"

class CallView: public QTreeWidget, public TraceItemView
{
    Q_OBJECT

public:
    CallView(bool showCallers, TraceItemView* parentView,
             QWidget* parent=nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    bool showCallers() const { return _showCallers; }

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
    void setCostColumnWidths();

    bool _showCallers;
};

#endif
