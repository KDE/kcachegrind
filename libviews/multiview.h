/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * MultiView, enclosing multiple (default: 2) TabView's with a user
 * choosable active view (i.e. focus). This is a splitter itself.
 * Selection of the active view is shown in the next to the right view
 * (with wrap around).
 */

#ifndef MULTIVIEW_H
#define MULTIVIEW_H

#include <QSplitter>
#include <QList>

#include "traceitemview.h"

class TabView;

class MultiView : public QSplitter, public TraceItemView
{
    Q_OBJECT

public:
    explicit MultiView(TopLevelBase* top, QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    TabView* activeTabView() const { return _active; }
    void setData(TraceData*) override;

    void appendView();
    void removeView();
    void setChildCount(int);
    int childCount() { return _views.count(); }

    void selected(TraceItemView*, CostItem*) override;
    void activated(TraceItemView*, CostItem*) override;

    void saveLayout(const QString& prefix, const QString& postfix) override;
    void restoreLayout(const QString& prefix, const QString& postfix) override;
    void saveOptions(const QString& prefix, const QString& postfix) override;
    void restoreOptions(const QString& prefix, const QString& postfix) override;

public Q_SLOTS:
    void tabActivated(TabView*);

private:
    void doUpdate(int, bool) override;

    TabView* _active;
    QList<TabView*> _views;
};

#endif
