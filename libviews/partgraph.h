/* This file is part of KCachegrind.
   Copyright (C) 2003,2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

  PartAreaWidget(QWidget* parent=0);

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
  QString tipString(TreeMapItem*) const;

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

  int rtti() const { return 1; }
  double value() const;
  QString text(int) const;
  int borderWidth() const { return 0; }
  TreeMapItemList* children();
  QColor backColor() const;

private:
  TraceData* _data;
};

class PartItem: public TreeMapItem
{
public:
  PartItem(TracePart* p);
  int rtti() const { return 2; }
  TracePart* part() { return _p; }
  double value() const;
  double sum() const;
  int borderWidth() const { return 0; }
  QString text(int) const;
  QPixmap pixmap(int) const;
  TreeMapItemList* children();
  QColor backColor() const;

private:
  TracePart* _p;
  unsigned int _factor;
};

class SubPartItem: public TreeMapItem
{
public:
  SubPartItem(ProfileCostArray*);
  int rtti() const { return 3; }
  ProfileCostArray* partCostItem() { return _partCostItem; }
  double value() const;
  double sum() const;
  SplitMode splitMode() const { return Vertical; }
  QString text(int) const;
  QPixmap pixmap(int) const;
  TreeMapItemList* children();
  QColor backColor() const;

private:
  ProfileCostArray* _partCostItem;
  unsigned int _factor;
};


#endif
