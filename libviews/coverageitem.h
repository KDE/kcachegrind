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
 * Items of coverage view.
 */

#ifndef COVERAGEITEM_H
#define COVERAGEITEM_H

#include <QTreeWidget>
#include "tracedata.h"

class Coverage;

class CallerCoverageItem: public QTreeWidgetItem
{
public:
  CallerCoverageItem(QTreeWidget* parent, Coverage* c,
		     TraceFunction* base,
                     EventType* ct, ProfileContext::Type gt);
  CallerCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
		     TraceFunction* base,
                     EventType* ct, ProfileContext::Type gt);

  bool operator< ( const QTreeWidgetItem & other ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(EventType* ct);
  void setGroupType(ProfileContext::Type);
  void update();

private:
  float _pSum;
  SubCost _sum;
  EventType* _costType;
  ProfileContext::Type _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};


class CalleeCoverageItem: public QTreeWidgetItem
{
public:
  CalleeCoverageItem(QTreeWidget* parent, Coverage* c,
		     TraceFunction* base,
                     EventType* ct, ProfileContext::Type gt);
  CalleeCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
		     TraceFunction* base,
                     EventType* ct, ProfileContext::Type gt);

  bool operator< ( const QTreeWidgetItem & other ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(EventType* ct);
  void setGroupType(ProfileContext::Type);
  void update();

private:
  float _pSum, _pSelf;
  SubCost _sum, _self;
  EventType* _costType;
  ProfileContext::Type _groupType;
  SubCost _cc;
  int _distance, _skipped;
  TraceFunction *_function, *_base;
  Coverage* _coverage;
};

#endif
