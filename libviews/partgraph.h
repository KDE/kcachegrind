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
 * TracePart Graph
 */

#ifndef PARTGRAPH_H
#define PARTGRAPH_H

#include <QPixmap>

#include "treemap.h"
#include "tracedata.h"

class PartAreaWidget: public TreeMapWidget
{
    Q_OBJECT

public:
    // Visualization inside of trace parts
    enum VisualizationMode { NoVisualization, Partitioning, Inclusive };

    explicit PartAreaWidget(QWidget* parent=nullptr);

    void setData(TraceData* d);
    void setEventType(EventType* ct);
    void setGroupType(ProfileContext::Type gt);
    void setVisualization(VisualizationMode);
    void setZoomFunction(bool zoomFunction);
    void setCallLevels(int callLevels);
    void setFunction(TraceFunction* f);

    EventType* eventType() const { return _eventType; }
    ProfileContext::Type groupType() const { return _groupType; }
    TraceFunction* function() const { return _function; }
    VisualizationMode visualization() const { return _visualization; }
    bool zoomFunction() const { return _zoomFunction; }
    int callLevels() const { return _callLevels; }

    QColor groupColor(TraceFunction*) const;
    QString tipString(TreeMapItem*) const override;

    void changeHidden(const TracePartList& list);
    bool isHidden(TracePart*) const;

private:
    void refreshParts();

    TraceData* _data;
    EventType* _eventType;
    ProfileContext::Type _groupType;
    TraceFunction* _function;
    VisualizationMode _visualization;
    bool _zoomFunction;
    int _callLevels;

    TracePartList _hiddenParts;
};

class BasePartItem: public TreeMapItem
{
public:
    BasePartItem();

    void setData(TraceData* d);

    int rtti() const override { return 1; }
    double value() const override;
    QString text(int) const override;
    int borderWidth() const override { return 0; }
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    TraceData* _data;
};

class PartItem: public TreeMapItem
{
public:
    explicit PartItem(TracePart* p);
    int rtti() const override { return 2; }
    TracePart* part() { return _p; }
    double value() const override;
    double sum() const override;
    int borderWidth() const override { return 0; }
    QString text(int) const override;
    QPixmap pixmap(int) const override;
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    TracePart* _p;
    unsigned int _factor;
};

class SubPartItem: public TreeMapItem
{
public:
    explicit SubPartItem(ProfileCostArray*);
    int rtti() const override { return 3; }
    ProfileCostArray* partCostItem() { return _partCostItem; }
    double value() const override;
    double sum() const override;
    SplitMode splitMode() const override { return Vertical; }
    QString text(int) const override;
    QPixmap pixmap(int) const override;
    TreeMapItemList* children() override;
    QColor backColor() const override;

private:
    ProfileCostArray* _partCostItem;
    unsigned int _factor;
};


#endif
