/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of call view.
 */

#ifndef CALLITEM_H
#define CALLITEM_H

#include <QTreeWidget>
#include "tracedata.h"

class CallView;

class CallItem: public QTreeWidgetItem
{
public:
    CallItem(CallView*, QTreeWidget*, TraceCall* c);

    bool operator<(const QTreeWidgetItem& other) const override;
    TraceCall* call() { return _call; }
    CallView* view() { return _view; }
    void updateCost();
    void updateGroup();

private:
    SubCost _sum, _sum2;
    SubCost _cc;
    TraceCall* _call;
    CallView* _view;
    TraceFunction *_active, *_shown;
};

#endif
