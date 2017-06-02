/* This file is part of KCachegrind.
   Copyright (c) 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Tab View, enclosing detailed views for one trace item in
 * two tab widgets, separated by a splitter
 */

#ifndef TABVIEW_H
#define TABVIEW_H

#include <QWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QMouseEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QSplitter>
#include <QList>

#include "traceitemview.h"

class QLabel;
class TabView;

/**
 * Subclass of QTabBar to enable context menu on tabs
 */
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    TabBar(TabView*, QTabWidget* parent);
protected:
    void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;

private:
    void context(QWidget*, const QPoint &);

    QTabWidget* _tabWidget;
    TabView* _tabView;
};


/**
 * Own Splitter:
 * Call checkVisiblity for all TabWidget children of the splitter
 * on a MoveEvent. This typically is produced when collapsing the widget
 * inside of another splitter.
 */
class Splitter: public QSplitter
{
    Q_OBJECT

public:
    explicit Splitter(Qt::Orientation o, QWidget* parent = 0);
    void checkVisiblity();

protected:
    void moveEvent(QMoveEvent *) Q_DECL_OVERRIDE;
};


/**
 * Own TabView:
 * - A QTabWidget able to track its visible rect via resizeEvents.
 *   This is needed to track if this widget is collapsed in a QSplitter.
 * - Use own TabBar for context menu
 */
class TabWidget: public QTabWidget
{
    Q_OBJECT

public:

    explicit TabWidget(TabView*, QWidget* parent = 0);

    bool hasVisibleRect() { return _hasVisibleRect; }
    void checkVisibility();

signals:
    void visibleRectChanged(TabWidget*);

protected:
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *) Q_DECL_OVERRIDE;

private:
    bool _hasVisibleRect;
};



class TabView : public QWidget, public TraceItemView
{
    Q_OBJECT

public:

    explicit TabView( TraceItemView* parentView,
                      QWidget* parent = 0 );

    QWidget* widget() Q_DECL_OVERRIDE { return this; }
    QString whatsThis() const Q_DECL_OVERRIDE;
    void setData(TraceData*) Q_DECL_OVERRIDE;
    bool isViewVisible() Q_DECL_OVERRIDE { return !_isCollapsed; }
    void selected(TraceItemView*, CostItem*) Q_DECL_OVERRIDE;
    bool active() const { return _active; }
    void setActive(bool);

    /**
     * Rearrange tabs
     * if <w> == 0, move hidden tabs
     */
    void moveTab(QWidget* w, Position, bool wholeArea = false);

    Position tabPosition(QWidget*);
    int visibleTabs();
    int visibleAreas();

    void saveLayout(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;
    void restoreLayout(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;
    void saveOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;
    void restoreOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;

public slots:
    void tabChanged(int);
    void visibleRectChangedSlot(TabWidget*);

signals:
    void tabActivated(TabView*);

protected:
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    bool eventFilter(QObject*, QEvent*) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;

private:
    TraceItemView* addTab(const QString&, TraceItemView*);
    void addTop(TraceItemView*);
    void addBottom(TraceItemView*);
    TabWidget* tabWidget(Position);
    void updateVisibility();
    void doUpdate(int, bool) Q_DECL_OVERRIDE;
    void updateNameLabel(const QString &n = QString());
    void installFocusFilters();
    void tabCounts(int&, int&, int&, int&);

    // this is true if width or height <= 1, and no child updates are done
    bool _isCollapsed;

    QLabel* _nameLabel;
    QString _nameLabelText, _nameLabelTooltip;
    int _textWidth;

    QSplitter *_mainSplitter, *_leftSplitter, *_bottomSplitter;
    TabWidget *_topTW, *_leftTW, *_bottomTW, *_rightTW;
    QList<TraceItemView*> _tabs;

    QWidget* _lastFocus;
    bool _active;
};

#endif
