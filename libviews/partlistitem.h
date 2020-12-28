/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef PARTLISTITEM_H
#define PARTLISTITEM_H

#include <QTreeWidget>

#include "tracedata.h"

/**
 * For info tab, trace part list.
 * Needs update on
 * - cost type change
 *
 * Note: on a cost item / percentage change, the list is rebuild
 */
class PartListItem: public QTreeWidgetItem
{
public:
    PartListItem(QTreeWidget* parent, TraceCostItem* costItem,
                 EventType* ct, ProfileContext::Type gt, TracePart* part);

    bool operator<(const QTreeWidgetItem& other) const override;
    ProfileCostArray* partCostItem() { return _partCostItem; }
    void setEventType(EventType* ct);
    void setGroupType(ProfileContext::Type);
    TracePart* part() { return _part; }
    void update();

private:
    SubCost _sum, _pure;
    SubCost _callCount;
    EventType* _eventType;
    ProfileCostArray* _partCostItem;
    TracePart* _part;
    ProfileContext::Type _groupType;
};

#endif // PARTLISTITEM_H
