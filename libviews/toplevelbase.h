/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2008-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Abstract base class for top level windows
 * - provides services from the top level main window such as action
 *   bindings for popup menus
 * - consumes changes to visualization state
 */

#ifndef TOPLEVELBASE_H
#define TOPLEVELBASE_H

#include "tracedata.h"
#include "traceitemview.h"

class QMenu;

class TopLevelBase
{
public:
    virtual ~TopLevelBase();

    /* notify about changes in the visualization state from profile views */
    virtual void activePartsChangedSlot(const TracePartList& list) = 0;
    virtual void setTraceItemDelayed(CostItem*) = 0;
    virtual void setEventTypeDelayed(EventType*) = 0;
    virtual void setEventType2Delayed(EventType*) = 0;
    virtual void setGroupTypeDelayed(ProfileContext::Type) = 0;
    virtual void setGroupDelayed(TraceCostItem*) = 0;
    virtual void setDirectionDelayed(TraceItemView::Direction) = 0;
    virtual void configChanged() = 0;

    virtual TracePartList hiddenParts() = 0;

    virtual void addEventTypeMenu(QMenu*,bool) = 0;
    virtual void addGoMenu(QMenu*) = 0;
    virtual void showMessage(const QString&, int msec) = 0;
};

#endif // TOPLEVELBASE_H
