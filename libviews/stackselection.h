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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#ifndef STACKSELECTION_H
#define STACKSELECTION_H

#include <QWidget>
#include "tracedata.h"

class Q3ListView;
class Q3ListViewItem;
class TraceFunction;
class TraceData;
class StackBrowser;


class StackSelection : public QWidget
{
  Q_OBJECT

public:
  StackSelection(QWidget* parent = 0);
  ~StackSelection();

  TraceData* data() const { return _data; }
  void setData(TraceData*);
  StackBrowser* browser() const { return _browser; }
  TraceEventType* eventType() { return _eventType; }
  TraceEventType* eventType2() { return _eventType2; }
  ProfileContext::Type groupType() { return _groupType; }

signals:
  void functionSelected(TraceItem*);

public slots:
  void setFunction(TraceFunction*);
  void setEventType(TraceEventType*);
  void setEventType2(TraceEventType*);
  void setGroupType(ProfileContext::Type);

  void stackSelected(Q3ListViewItem*);
  void browserBack();
  void browserForward();
  void browserUp();
  void browserDown();
  void refresh();
  void rebuildStackList();

private:
  void selectFunction();

  TraceData* _data;
  StackBrowser* _browser;
  Q3ListViewItem* _item;
  TraceFunction* _function;
  TraceEventType* _eventType;
  TraceEventType* _eventType2;
  ProfileContext::Type _groupType;

  Q3ListView* _stackList;
};

#endif
