/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of stack dockable.
 */

#ifndef STACKITEM_H
#define STACKITEM_H

#include <QTreeWidget>

#include "tracedata.h"

class StackSelection;


// for the stack browser

class StackItem: public QTreeWidgetItem
{
public:
    // for top
    StackItem(StackSelection* ss, QTreeWidget* parent, TraceFunction* f);
    StackItem(StackSelection* ss, QTreeWidget* parent, TraceCall* c);

    TraceFunction* function() { return _function; }
    TraceCall* call() { return _call; }
    void updateGroup();
    void updateCost();

private:
    StackSelection* _view;
    SubCost _sum;
    TraceFunction* _function;
    TraceCall* _call;
};

#endif // STACKITEM_H
