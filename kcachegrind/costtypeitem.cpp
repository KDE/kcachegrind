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
 * Items of cost type view.
 */

#include <qpixmap.h>
#include <klocale.h>

#include "configuration.h"
#include "listutils.h"
#include "costtypeitem.h"


// CostTypeItem


CostTypeItem::CostTypeItem(QListView* parent, TraceCostItem* costItem,
                           TraceCostType* ct, TraceCost::CostType gt)
  :QListViewItem(parent)
{
  _costItem = costItem;
  _costType = ct;
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
      setText(0, i18n("Unknown Type"));
  }
  update();
}

void CostTypeItem::setGroupType(TraceCost::CostType gt)
{
    if (_groupType == gt) return;

    _groupType = gt;
    update();
}

void CostTypeItem::update()
{
  TraceData* d = _costItem ? _costItem->data() : 0;
  double total = d ? ((double)d->subCost(_costType)) : 0.0;

  if (total == 0.0) {
    setText(1, "-");
    setPixmap(1, QPixmap());
    setText(2, "-");
    setPixmap(2, QPixmap());
    return;
  }

  TraceFunction* f = (_costItem->type()==TraceCost::Function) ?
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
  if (_costItem->type()==TraceCost::FunctionCycle) {
      f = (TraceFunction*)_costItem;
      selfTotalCost = f->data();
  }

  double selfTotal = selfTotalCost->subCost(_costType);

  // for all cost items there's a self cost
  _pure = _costItem ? _costItem->subCost(_costType) : SubCost(0);
  double pure  = 100.0 * _pure / selfTotal;
  if (Configuration::showPercentage()) {
    setText(2, QString("%1")
            .arg(pure, 0, 'f', Configuration::percentPrecision()));
  }
  else
    setText(2, _costItem->prettySubCost(_costType));

  setPixmap(2, costPixmap(_costType, _costItem, selfTotal, false));

  if (!f) {
    setText(1, "-");
    setPixmap(1, QPixmap());
    return;
  }

  _sum = f->inclusive()->subCost(_costType);
  double sum  = 100.0 * _sum / total;
  if (Configuration::showPercentage()) {
    setText(1, QString("%1")
            .arg(sum, 0, 'f', Configuration::percentPrecision()));
  }
  else
    setText(1, _sum.pretty());

  setPixmap(1, costPixmap(_costType, f->inclusive(), total, false));
}


int CostTypeItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  CostTypeItem* fi = (CostTypeItem*) i;
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
  return QListViewItem::compare(i, col, ascending);
}


