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
 * PartSelection for KCachegrind
 * For part file selection, to be put into a QDockWindow
 */

#ifndef PARTSELECTION_H
#define PARTSELECTION_H

#include <qobject.h>

#include "partselectionbase.h"
#include "partgraph.h"
#include "tracedata.h"

class KConfigGroup;
class TraceFunction;
class TraceData;
class TreeMapItem;

class PartSelection: public PartSelectionBase
{
  Q_OBJECT

public:
  PartSelection( QWidget* parent = 0, const char* name = 0);
  ~PartSelection();

  TraceData* data() { return _data; }
  void setData(TraceData*);

  PartAreaWidget* graph() { return partAreaWidget; }

  void readVisualisationConfig(KConfigGroup*);
  void saveVisualisationConfig(KConfigGroup*);

signals:
  void activePartsChanged(const TracePartList& list);
  void partsHideSelected();
  void partsUnhideAll();
  void groupChanged(TraceCostItem*);
  void functionChanged(TraceItem*);
  void showMessage(const QString&, int);
  void goBack();

public slots:
  void selectionChanged();
  void doubleClicked(TreeMapItem*);
  void contextMenuRequested(TreeMapItem*, const QPoint &);
  void currentChangedSlot(TreeMapItem*, bool);

  void setPart(TracePart*);
  void setCostType(TraceCostType*);
  void setCostType2(TraceCostType*);
  void setGroupType(TraceItem::CostType);
  void setGroup(TraceCostItem*);
  void setFunction(TraceFunction*);
  void activePartsChangedSlot(const TracePartList& list);
  void hiddenPartsChangedSlot(const TracePartList& list);
  void refresh();
  void showInfo(bool);

private:
  void fillInfo();

  TraceData* _data;
  TraceCostType *_costType, *_costType2;
  TraceItem::CostType _groupType;
  TraceCostItem* _group;
  TraceFunction* _function;
  bool _showInfo;
  bool _diagramMode;
  bool _drawFrames;

  bool _inSelectionUpdate;
};

#endif
