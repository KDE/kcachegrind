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
 * Call Map View
 */

#ifndef CALLMAPVIEW_H
#define CALLMAPVIEW_H

#include "treemap.h"
#include "tracedata.h"
#include "traceitemview.h"

class CallMapView: public TreeMapWidget, public TraceItemView
{
  Q_OBJECT

public:

  CallMapView(bool showCallers, TraceItemView* parentView,
              QWidget* parent=0, const char* name=0);

  QWidget* widget() { return this; }
  QString whatsThis();

  void readViewConfig(KConfig*, QString prefix, QString postfix);
  void saveViewConfig(KConfig*, QString prefix, QString postfix);

  bool showCallers() { return _showCallers; }
  TraceCost* totalCost();
  QString tipString(TreeMapItem*);
  QColor groupColor(TraceFunction*);

private slots:
  void context(TreeMapItem*,const QPoint &);
  void selectedSlot(TreeMapItem*, bool);
  void activatedSlot(TreeMapItem*);

private:
  TraceItem* canShow(TraceItem*);
  void doUpdate(int);

  bool _showCallers;
};



// Subitems of CallMap

class CallMapBaseItem: public TreeMapItem
{
public:
  CallMapBaseItem();

  void setFunction(TraceFunction* f);
  TraceFunction* function() { return _f; }
  int rtti() { return 1; }
  double sum();
  double value();
  bool isMarked(int);
  QString text(int);
  QPixmap pixmap(int);
  TreeMapItemList* children();
  QColor backColor();

private:
  TraceFunction* _f;
};


class CallMapCallingItem: public TreeMapItem
{
public:
  CallMapCallingItem(double factor, TraceCall* c);
  void init();
  int rtti() { return 2; }
  int borderWidth() { return widget()->borderWidth(); }
  TraceFunction* function() { return _c->called(); }
  double value();
  double sum();
  bool isMarked(int);
  QString text(int);
  QPixmap pixmap(int);
  TreeMapItemList* children();
  QColor backColor();

private:
  TraceCall* _c;
  double _factor;
};

class CallMapCallerItem: public TreeMapItem
{
public:
  CallMapCallerItem(double factor, TraceCall* c);
  int rtti() { return 3; }
  int borderWidth() { return widget()->borderWidth(); }
  TraceFunction* function() { return _c->caller(); }
  double value();
  bool isMarked(int);
  QString text(int);
  QPixmap pixmap(int);
  TreeMapItemList* children();
  QColor backColor();

private:
  TraceCall* _c;
  double _factor;
};


#endif
