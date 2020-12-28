/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#ifndef STACKSELECTION_H
#define STACKSELECTION_H

#include <QWidget>
#include "tracedata.h"

class QTreeWidget;
class QTreeWidgetItem;
class TraceFunction;
class TraceData;
class StackBrowser;


class StackSelection : public QWidget
{
    Q_OBJECT

public:
    explicit StackSelection(QWidget* parent = nullptr);
    ~StackSelection() override;

    TraceData* data() const { return _data; }
    void setData(TraceData*);
    StackBrowser* browser() const { return _browser; }
    EventType* eventType() { return _eventType; }
    EventType* eventType2() { return _eventType2; }
    ProfileContext::Type groupType() { return _groupType; }

Q_SIGNALS:
    void functionSelected(CostItem*);

public Q_SLOTS:
    void setFunction(TraceFunction*);
    void setEventType(EventType*);
    void setEventType2(EventType*);
    void setGroupType(ProfileContext::Type);

    void stackSelected(QTreeWidgetItem*,QTreeWidgetItem*);
    void browserBack();
    void browserForward();
    void browserUp();
    void browserDown();
    void refresh();
    void rebuildStackList();

private:
    void selectFunction();

    TraceData* _data;
    StackBrowser* _browser;
    TraceFunction* _function;
    EventType* _eventType;
    EventType* _eventType2;
    ProfileContext::Type _groupType;

    QTreeWidget* _stackList;
};

#endif
