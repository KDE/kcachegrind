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

    bool operator<(const QTreeWidgetItem& other) const;
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
