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
  QString whatsThis() const;
  void setData(TraceData*);

  void readViewConfig(KConfig*, QString prefix, QString postfix, bool);
  void saveViewConfig(KConfig*, QString prefix, QString postfix, bool);

  bool showCallers() const { return _showCallers; }
  TraceCost* totalCost();
  QString tipString(TreeMapItem*) const;
  QColor groupColor(TraceFunction*) const;

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
  int rtti() const { return 1; }
  double sum() const;
  double value() const ;
  bool isMarked(int) const;
  QString text(int) const;
  QPixmap pixmap(int) const;
  TreeMapItemList* children();
  QColor backColor() const;

private:
  TraceFunction* _f;
};


class CallMapCallingItem: public TreeMapItem
{
public:
  CallMapCallingItem(double factor, TraceCall* c);
  void init();
  int rtti() const { return 2; }
  int borderWidth() const { return widget()->borderWidth(); }
  TraceFunction* function() { return _c->called(); }
  double value() const;
  double sum() const;
  bool isMarked(int) const;
  QString text(int) const;
  QPixmap pixmap(int) const;
  TreeMapItemList* children();
  QColor backColor() const;

private:
  TraceCall* _c;
  double _factor;
};

class CallMapCallerItem: public TreeMapItem
{
public:
  CallMapCallerItem(double factor, TraceCall* c);
  int rtti() const { return 3; }
  int borderWidth() const { return widget()->borderWidth(); }
  TraceFunction* function() { return _c->caller(); }
  double value() const;
  bool isMarked(int) const;
  QString text(int) const;
  QPixmap pixmap(int) const;
  TreeMapItemList* children();
  QColor backColor() const;

private:
  TraceCall* _c;
  double _factor;
};


#endif
