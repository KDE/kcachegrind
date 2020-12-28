/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of event type view.
 */

#ifndef EVENTTYPEITEM_H
#define EVENTTYPEITEM_H

#include <QTreeWidgetItem>

#include "tracedata.h"

class EventTypeItem: public QTreeWidgetItem
{
public:
    EventTypeItem(TraceCostItem* costItem,
                  EventType* ct, ProfileContext::Type gt);

    bool operator<(const QTreeWidgetItem &other) const override;
    void setGroupType(ProfileContext::Type);
    TraceCostItem* costItem() { return _costItem; }
    EventType* eventType() { return _eventType; }
    void update();

    QVariant data(int column, int role) const override;

private:
    SubCost _sum, _pure;
    EventType* _eventType;
    TraceCostItem* _costItem;
    ProfileContext::Type _groupType;
};

#endif // EVENTTYPEITEM_H
