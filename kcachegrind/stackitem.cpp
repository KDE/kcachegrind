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
 * Items of stack dockable.
 */

#include <qpixmap.h>
#include <klocale.h>

#include "configuration.h"
#include "listutils.h"
#include "stackitem.h"



// StackItem


StackItem::StackItem(QListView* parent, TraceFunction* f,
                     TraceCostType* ct,
                     TraceCost::CostType gt)
  :QListViewItem(parent)
{
  _function = f;
  _call = 0;
  _groupType = TraceCost::NoCostType;

  setCostType(ct);
  setGroupType(gt);
  setText(0, QString("-- "));
  setText(1, QString("-- "));
  setText(2, f->prettyName());
}

StackItem::StackItem(QListView* parent, TraceCall* call,
                     TraceCostType* ct,
                     TraceCost::CostType gt)
  :QListViewItem(parent)
{
  _call = call;
  _function = call->called();
  _groupType = TraceCost::NoCostType;

  setCostType(ct);
  setGroupType(gt);
  setText(2, _function->prettyName());
}


void StackItem::setCostType(TraceCostType* ct)
{
  _costType = ct;
  update();
}

void StackItem::setGroupType(TraceCost::CostType gt)
{
  if (_groupType == gt) return;
  _groupType = gt;

  QColor c = Configuration::functionColor(_groupType, _function);
  setPixmap(2, colorPixmap(10, 10, c));
}

void StackItem::update()
{
  if (!_call) return;

  setText(1, _call->prettyCallCount());

  _sum = _call->subCost(_costType);
  double total = _call->called()->data()->subCost(_costType);
  if (total == 0.0) {
    setText(0, "-");
    setPixmap(0, QPixmap());
    return;
  }

  double sum  = 100.0 * _sum / total;

  if (Configuration::showPercentage())
    setText(0, QString("%1")
            .arg(sum, 0, 'f', Configuration::percentPrecision()));
  else
    setText(0, _call->prettySubCost(_costType));

  setPixmap(1, costPixmap(_costType, _call, total));
}
