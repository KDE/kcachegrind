/* This file is part of KCachegrind.
   Copyright (C) 2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
