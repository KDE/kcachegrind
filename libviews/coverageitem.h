/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of coverage view.
 */

#ifndef COVERAGEITEM_H
#define COVERAGEITEM_H

#include <QTreeWidget>
#include "tracedata.h"

class Coverage;

class CallerCoverageItem: public QTreeWidgetItem
{
public:
    CallerCoverageItem(QTreeWidget* parent, Coverage* c,
                       TraceFunction* base,
                       EventType* ct, ProfileContext::Type gt);
    CallerCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
                       TraceFunction* base,
                       EventType* ct, ProfileContext::Type gt);

    bool operator< ( const QTreeWidgetItem & other ) const override;
    TraceFunction* function() { return (_skipped) ? nullptr : _function; }
    void setCostType(EventType* ct);
    void setGroupType(ProfileContext::Type);
    void update();

private:
    float _pSum;
    SubCost _sum;
    EventType* _costType;
    ProfileContext::Type _groupType;
    SubCost _cc;
    int _distance, _skipped;
    TraceFunction *_function, *_base;
    Coverage* _coverage;
};


class CalleeCoverageItem: public QTreeWidgetItem
{
public:
    CalleeCoverageItem(QTreeWidget* parent, Coverage* c,
                       TraceFunction* base,
                       EventType* ct, ProfileContext::Type gt);
    CalleeCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
                       TraceFunction* base,
                       EventType* ct, ProfileContext::Type gt);

    bool operator< ( const QTreeWidgetItem & other ) const override;
    TraceFunction* function() { return (_skipped) ? nullptr : _function; }
    void setCostType(EventType* ct);
    void setGroupType(ProfileContext::Type);
    void update();

private:
    float _pSum, _pSelf;
    SubCost _sum, _self;
    EventType* _costType;
    ProfileContext::Type _groupType;
    SubCost _cc;
    int _distance, _skipped;
    TraceFunction *_function, *_base;
    Coverage* _coverage;
};

#endif
