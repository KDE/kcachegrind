/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

   KCachegrind is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * TracePart Graph
 */

#ifndef PARTGRAPH_H
#define PARTGRAPH_H

#include "treemap.h"
#include "tracedata.h"

class PartAreaWidget: public TreeMapWidget
{
  Q_OBJECT

public:
  // Visualisation inside of trace parts
  enum VisualisationMode { NoVisualisation, Partitioning, Cumulative };

  PartAreaWidget(QWidget* parent=0, const char* name=0);

  void setData(TraceData* d);
  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType gt);
  void setVisualisation(VisualisationMode);
  void setZoomFunction(bool zoomFunction);
  void setCallLevels(int callLevels);
  void setFunction(TraceFunction* f);

  TraceCostType* costType() { return _costType; }
  TraceCost::CostType groupType() { return _groupType; }
  TraceFunction* function() { return _function; }
  VisualisationMode visualisation() { return _visualisation; }
  bool zoomFunction() { return _zoomFunction; }
  int callLevels() { return _callLevels; }

  QColor groupColor(TraceFunction*);
  QString tipString(TreeMapItem*);

  void changeHidden(const TracePartList& list);
  bool isHidden(TracePart*);

private:
  void refreshParts();

  TraceData* _data;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  TraceFunction* _function;
  VisualisationMode _visualisation;
  bool _zoomFunction;
  int _callLevels;

  TracePartList _hiddenParts;
};

class BasePartItem: public TreeMapItem
{
public:
  BasePartItem();

  void setData(TraceData* d);

  int rtti() { return 1; }
  double value();
  QString text(int);
  int borderWidth() { return 0; }
  TreeMapItemList* children();
  QColor backColor();

private:
  TraceData* _data;
};

class PartItem: public TreeMapItem
{
public:
  PartItem(TracePart* p);
  int rtti() { return 2; }
  TracePart* part() { return _p; }
  double value();
  double sum();
  int borderWidth() { return 0; }
  QString text(int);
  QPixmap pixmap(int);
  TreeMapItemList* children();
  QColor backColor();

private:
  TracePart* _p;
  unsigned int _factor;
};

class SubPartItem: public TreeMapItem
{
public:
  SubPartItem(TraceCost*);
  int rtti() { return 3; }
  TraceCost* partCostItem() { return _partCostItem; }
  double value();
  double sum();
  SplitMode splitMode() { return Vertical; }
  QString text(int);
  QPixmap pixmap(int);
  TreeMapItemList* children();
  QColor backColor();

private:
  TraceCost* _partCostItem;
  unsigned int _factor;
};


#endif
