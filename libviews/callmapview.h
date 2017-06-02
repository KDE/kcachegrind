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

    QWidget* widget() Q_DECL_OVERRIDE { return this; }
    QString whatsThis() const Q_DECL_OVERRIDE;
    void setData(TraceData*) Q_DECL_OVERRIDE;

    void restoreOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;
    void saveOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;

    bool showCallers() const { return _showCallers; }
    ProfileCostArray* totalCost();
    QString tipString(TreeMapItem*) const Q_DECL_OVERRIDE;
    QColor groupColor(TraceFunction*) const;

private slots:
    void context(TreeMapItem*,const QPoint &);
    void selectedSlot(TreeMapItem*, bool);
    void activatedSlot(TreeMapItem*);
    void mapItemTriggered(QAction*);
    void drawingDepthTriggered(QAction*);
    void stopFunctionTriggered(QAction*);
    void areaLimitTriggered(QAction*);
    void borderWidthTriggered(QAction*);

private:
    CostItem* canShow(CostItem*) Q_DECL_OVERRIDE;
    void doUpdate(int, bool) Q_DECL_OVERRIDE;

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
    int maxLines(int) const Q_DECL_OVERRIDE;
    bool allowBreak(int) const Q_DECL_OVERRIDE;
    bool allowTruncation(int) const Q_DECL_OVERRIDE;
    Position position(int) const Q_DECL_OVERRIDE;
};

class CallMapRootItem: public CallMapItemBase
{
public:
    CallMapRootItem();

    void setFunction(TraceFunction* f);
    TraceFunction* function() { return _f; }
    int rtti() const Q_DECL_OVERRIDE { return 1; }
    double sum() const Q_DECL_OVERRIDE;
    double value() const Q_DECL_OVERRIDE;
    bool isMarked(int) const Q_DECL_OVERRIDE;
    QString text(int) const Q_DECL_OVERRIDE;
    QPixmap pixmap(int) const Q_DECL_OVERRIDE;
    TreeMapItemList* children() Q_DECL_OVERRIDE;
    QColor backColor() const Q_DECL_OVERRIDE;

private:
    TraceFunction* _f;
};


class CallMapCallingItem: public CallMapItemBase
{
public:
    CallMapCallingItem(double factor, TraceCall* c);
    void init();
    int rtti() const Q_DECL_OVERRIDE { return 2; }
    int borderWidth() const Q_DECL_OVERRIDE { return widget()->borderWidth(); }
    TraceFunction* function() { return _c->called(); }
    double value() const Q_DECL_OVERRIDE;
    double sum() const Q_DECL_OVERRIDE;
    bool isMarked(int) const Q_DECL_OVERRIDE;
    QString text(int) const Q_DECL_OVERRIDE;
    QPixmap pixmap(int) const Q_DECL_OVERRIDE;
    TreeMapItemList* children() Q_DECL_OVERRIDE;
    QColor backColor() const Q_DECL_OVERRIDE;

private:
    TraceCall* _c;
    double _factor;
};

class CallMapCallerItem: public CallMapItemBase
{
public:
    CallMapCallerItem(double factor, TraceCall* c);
    int rtti() const Q_DECL_OVERRIDE { return 3; }
    int borderWidth() const Q_DECL_OVERRIDE { return widget()->borderWidth(); }
    TraceFunction* function() { return _c->caller(); }
    double value() const Q_DECL_OVERRIDE;
    bool isMarked(int) const Q_DECL_OVERRIDE;
    QString text(int i) const Q_DECL_OVERRIDE;
    QPixmap pixmap(int) const Q_DECL_OVERRIDE;
    TreeMapItemList* children() Q_DECL_OVERRIDE;
    QColor backColor() const Q_DECL_OVERRIDE;

private:
    TraceCall* _c;
    double _factor;
};


#endif
