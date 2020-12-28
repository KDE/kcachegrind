/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
    void mousePressEvent(QMouseEvent *e) override;

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
    explicit Splitter(Qt::Orientation o, QWidget* parent = nullptr);
    void checkVisiblity();

protected:
    void moveEvent(QMoveEvent *) override;
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

    explicit TabWidget(TabView*, QWidget* parent = nullptr);

    bool hasVisibleRect() { return _hasVisibleRect; }
    void checkVisibility();

Q_SIGNALS:
    void visibleRectChanged(TabWidget*);

protected:
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void moveEvent(QMoveEvent *) override;

private:
    bool _hasVisibleRect;
};



class TabView : public QWidget, public TraceItemView
{
    Q_OBJECT

public:

    explicit TabView( TraceItemView* parentView,
                      QWidget* parent = nullptr );

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    void setData(TraceData*) override;
    bool isViewVisible() override { return !_isCollapsed; }
    void selected(TraceItemView*, CostItem*) override;
    bool active() const { return _active; }
    void setActive(bool);

    /**
     * Rearrange tabs
     * if @p w == 0, move hidden tabs
     * @param w the widget
     * @param p position
     * @param wholeArea whether to move the whole area
     */
    void moveTab(QWidget* w, Position, bool wholeArea = false);

    Position tabPosition(QWidget*);
    int visibleTabs();
    int visibleAreas();

    void saveLayout(const QString& prefix, const QString& postfix) override;
    void restoreLayout(const QString& prefix, const QString& postfix) override;
    void saveOptions(const QString& prefix, const QString& postfix) override;
    void restoreOptions(const QString& prefix, const QString& postfix) override;

public Q_SLOTS:
    void tabChanged(int);
    void visibleRectChangedSlot(TabWidget*);

Q_SIGNALS:
    void tabActivated(TabView*);

protected:
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject*, QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    TraceItemView* addTab(const QString&, TraceItemView*);
    void addTop(TraceItemView*);
    void addBottom(TraceItemView*);
    TabWidget* tabWidget(Position);
    void updateVisibility();
    void doUpdate(int, bool) override;
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
