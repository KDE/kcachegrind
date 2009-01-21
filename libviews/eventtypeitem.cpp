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
 * Items of event type view.
 */

#include "eventtypeitem.h"

#include <QPixmap>

#include "configuration.h"
#include "listutils.h"


// EventTypeItem


EventTypeItem::EventTypeItem(Q3ListView* parent, TraceCostItem* costItem,
                           TraceEventType* ct, TraceCost::CostType gt)
  :Q3ListViewItem(parent)
{
  _costItem = costItem;
  _eventType = ct;
  _groupType = gt;

  if (ct) {
      setText(0, ct->longName());
      setText(3, ct->name());
      QString formula = ct->formula();
      setText(5, formula);
      if (!formula.isEmpty()) {
	  setText(4, "=");
	  // we have a virtual type: allow editing
	  setRenameEnabled(0, true);
	  setRenameEnabled(3, true);
	  setRenameEnabled(5, true);
      } 
  }
  else {
      setText(0, QObject::tr("Unknown Type"));
  }
  update();
}

void EventTypeItem::setGroupType(TraceCost::CostType gt)
{
    if (_groupType == gt) return;

    _groupType = gt;
    update();
}

void EventTypeItem::update()
{
  TraceData* d = _costItem ? _costItem->data() : 0;
  double total = d ? ((double)d->subCost(_eventType)) : 0.0;

  if (total == 0.0) {
    setText(1, "-");
    setPixmap(1, QPixmap());
    setText(2, "-");
    setPixmap(2, QPixmap());
    return;
  }

  TraceFunction* f = (_costItem && _costItem->type()==TraceCost::Function) ?
                     (TraceFunction*)_costItem : 0;

  TraceCost* selfTotalCost = f ? f->data() : d;
  if (f && Configuration::showExpanded()) {
      switch(_groupType) {
      case TraceCost::Object: selfTotalCost = f->object(); break;
      case TraceCost::Class:  selfTotalCost = f->cls(); break;
      case TraceCost::File:   selfTotalCost = f->file(); break;
      case TraceCost::FunctionCycle: selfTotalCost = f->cycle(); break;
      default: break;
      }
  }
  if (_costItem && _costItem->type()==TraceCost::FunctionCycle) {
      f = (TraceFunction*)_costItem;
      selfTotalCost = f->data();
  }

  double selfTotal = selfTotalCost->subCost(_eventType);

  // for all cost items there is a self cost
  _pure = _costItem ? _costItem->subCost(_eventType) : SubCost(0);
  double pure  = 100.0 * _pure / selfTotal;
  if (Configuration::showPercentage()) {
    setText(2, QString("%1")
            .arg(pure, 0, 'f', Configuration::percentPrecision()));
  }
  else if (_costItem)
    setText(2, _costItem->prettySubCost(_eventType));

  setPixmap(2, costPixmap(_eventType, _costItem, selfTotal, false));

  if (!f) {
    setText(1, "-");
    setPixmap(1, QPixmap());
    return;
  }

  _sum = f->inclusive()->subCost(_eventType);
  double sum  = 100.0 * _sum / total;
  if (Configuration::showPercentage()) {
    setText(1, QString("%1")
            .arg(sum, 0, 'f', Configuration::percentPrecision()));
  }
  else
    setText(1, _sum.pretty());

  setPixmap(1, costPixmap(_eventType, f->inclusive(), total, false));
}


int EventTypeItem::compare(Q3ListViewItem * i, int col, bool ascending ) const
{
  EventTypeItem* fi = (EventTypeItem*) i;
  if (col==0) {
    if (_sum < fi->_sum) return -1;
    if (_sum > fi->_sum) return 1;
    return 0;
  }
  if (col==1) {
    if (_pure < fi->_pure) return -1;
    if (_pure > fi->_pure) return 1;
    return 0;
  }
  return Q3ListViewItem::compare(i, col, ascending);
}


