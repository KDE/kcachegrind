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

#include "globalconfig.h"
#include "listutils.h"


// EventTypeItem


EventTypeItem::EventTypeItem(TraceCostItem* costItem,
			     EventType* ct, ProfileContext::Type gt)
{
  _costItem = costItem;
  _eventType = ct;
  _groupType = gt;

  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);
  setTextAlignment(3, Qt::AlignRight);
  setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  if (ct) {
      setText(0, ct->longName());
      setText(3, ct->name());
      setText(5, ct->parsedFormula());
      QString formula = ct->formula();
      if (!ct->isReal()) {
	  setText(4, "=");
	  // we have a virtual type: allow editing
	  // FIXME: How to enable this only for columns 0,3,5 ?!
	  setFlags(flags() | Qt::ItemIsEditable);
      } 
  }
  else {
      setText(0, QObject::tr("Unknown Type"));
  }
  update();
}

void EventTypeItem::setGroupType(ProfileContext::Type gt)
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
    setIcon(1, QIcon());
    setText(2, "-");
    setIcon(2, QIcon());
    return;
  }

  TraceFunction* f = (_costItem && _costItem->type()==ProfileContext::Function) ?
                     (TraceFunction*)_costItem : 0;

  ProfileCostArray* selfTotalCost = f ? f->data() : d;
  if (f && GlobalConfig::showExpanded()) {
      ProfileCostArray* parent = 0;
      switch(_groupType) {
      case ProfileContext::Object:        parent = f->object(); break;
      case ProfileContext::Class:         parent = f->cls(); break;
      case ProfileContext::File:          parent = f->file(); break;
      case ProfileContext::FunctionCycle: parent = f->cycle(); break;
      default: break;
      }
      if (parent) selfTotalCost = parent;
  }
  if (_costItem && _costItem->type()==ProfileContext::FunctionCycle) {
      f = (TraceFunction*)_costItem;
      selfTotalCost = f->data();
  }

  double selfTotal = selfTotalCost->subCost(_eventType);

  // for all cost items there is a self cost
  _pure = _costItem ? _costItem->subCost(_eventType) : SubCost(0);
  double pure  = 100.0 * _pure / selfTotal;
  if (GlobalConfig::showPercentage()) {
    setText(2, QString("%1")
            .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
  }
  else if (_costItem)
    setText(2, _costItem->prettySubCost(_eventType));

  setIcon(2, QIcon(costPixmap(_eventType, _costItem, selfTotal, false)));

  if (!f) {
    setText(1, "-");
    setIcon(1, QIcon());
    return;
  }

  _sum = f->inclusive()->subCost(_eventType);
  double sum  = 100.0 * _sum / total;
  if (GlobalConfig::showPercentage()) {
    setText(1, QString("%1")
            .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
  }
  else
    setText(1, _sum.pretty());

  setIcon(1, QIcon(costPixmap(_eventType, f->inclusive(), total, false)));
}

bool EventTypeItem::operator<(const QTreeWidgetItem &other) const
{
    int col = treeWidget()->sortColumn();
    EventTypeItem* o = (EventTypeItem*) &other;
    if (col==0)
	return _sum < o->_sum;
    if (col==1)
	return _pure < o->_pure;

    return QTreeWidgetItem::operator<(other);
}

QVariant EventTypeItem::data(int column, int role) const
{
    if ((column == 5) && (role == Qt::EditRole))
        return QVariant(_eventType->formula());
    return QTreeWidgetItem::data(column, role);
}


