/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef COSTLISTITEM_H
#define COSTLISTITEM_H

#include <QTreeWidget>

#include "tracedata.h"

class CostListItem: public QTreeWidgetItem
{
public:
    CostListItem(QTreeWidget* parent, TraceCostItem* cost,
                 EventType* et, int size = -1);
    // entry with multiple skipped items
    CostListItem(QTreeWidget* parent, int skipped, TraceCostItem* cost,
                 EventType* et);

    bool operator< ( const QTreeWidgetItem & other ) const override;
    TraceCostItem* costItem() { return (_skipped) ? nullptr : _costItem; }
    void setEventType(EventType* et);
    void update();
    void setSize(int s);

private:
    void updateName();

    SubCost _pure;
    EventType* _eventType;
    TraceCostItem* _costItem;
    // >0 only for last item in list, if items are skipped
    int _skipped;
    // number of items in group, is put in parenthesis after name
    int _groupSize;
};

#endif
