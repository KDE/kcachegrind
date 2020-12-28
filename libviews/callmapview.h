/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Call Map View
 */

#ifndef CALLMAPVIEW_H
#define CALLMAPVIEW_H

#include <QPixmap>

#include "treemap.h"
#include "tracedata.h"
#include "traceitemview.h"

class QAction;
class QMenu;

class CallMapView: public TreeMapWidget, public TraceItemView
{
    Q_OBJECT

public:

    CallMapView(bool showCallers, TraceItemView* parentView,
                QWidget* parent, const QString& name);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    void setData(TraceData*) override;

    void restoreOptions(const QString& prefix, const QString& postfix) override;
    void saveOptions(const QString& prefix, const QString& postfix) override;

    bool showCallers() const { return _showCallers; }
    ProfileCostArray* totalCost();
    QString tipString(TreeMapItem*) const override;
    QColor groupColor(TraceFunction*) const;

private Q_SLOTS:
    void context(TreeMapItem*,const QPoint &);
    void selectedSlot(TreeMapItem*, bool);
    void activatedSlot(TreeMapItem*);
    void mapItemTriggered(QAction*);
    void drawingDepthTriggered(QAction*);
    void stopFunctionTriggered(QAction*);
    void areaLimitTriggered(QAction*);
    void borderWidthTriggered(QAction*);

private:
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;

    // context menu builders
    void addItemListMenu(QMenu*,TreeMapItem*);
    QAction* addDrawingDepthAction(QMenu*, const QString&, int);
    void addDrawingDepthMenu(QMenu*, TreeMapItem*, const QString&);
    QAction* addStopFunctionAction(QMenu*, const QString&, const QString&);
    void addStopFunctionMenu(QMenu*, TreeMapItem*);
    QAction* addAreaLimitAction(QMenu*, const QString&, int);
    void addAreaLimitMenu(QMenu*, TreeMapItem*, const QString&);
    QAction* addBorderWidthAction(QMenu*, const QString&, int);

    bool _showCallers;
};



// Subitems in CallMapView

// text field indexes: lower indexes get priority if space is not enough
#define IDX_COST      0
#define IDX_FUNCNAME  1
#define IDX_LOCATION  2
#define IDX_CALLCOUNT 3

// base class providing same layout configuration
class CallMapItemBase: public TreeMapItem
{
public:
    int maxLines(int) const override;
    bool allowBreak(int) const override;
    bool allowTruncation(int) const override;
    Position position(int) const override;
};

class CallMapRootItem: public CallMapItemBase
{
public:
    CallMapRootItem();

    void setFunction(TraceFunction* f);
    TraceFunction* function() { return _f; }
    int rtti() const override { return 1; }
    double sum() const override;
    double value() const override;
    bool isMarked(int) const override;
    QString text(int) const override;
    QPixmap pixmap(int) const override;
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    TraceFunction* _f;
};


class CallMapCallingItem: public CallMapItemBase
{
public:
    CallMapCallingItem(double factor, TraceCall* c);
    void init();
    int rtti() const override { return 2; }
    int borderWidth() const override { return widget()->borderWidth(); }
    TraceFunction* function() { return _c->called(); }
    double value() const override;
    double sum() const override;
    bool isMarked(int) const override;
    QString text(int) const override;
    QPixmap pixmap(int) const override;
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    TraceCall* _c;
    double _factor;
};

class CallMapCallerItem: public CallMapItemBase
{
public:
    CallMapCallerItem(double factor, TraceCall* c);
    int rtti() const override { return 3; }
    int borderWidth() const override { return widget()->borderWidth(); }
    TraceFunction* function() { return _c->caller(); }
    double value() const override;
    bool isMarked(int) const override;
    QString text(int i) const override;
    QPixmap pixmap(int) const override;
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    TraceCall* _c;
    double _factor;
};


#endif
