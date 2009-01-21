/* This file is part of KCachegrind.
   Copyright (C) 2002, 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "costlistitem.h"

#include <math.h>

#include <QPainter>
#include <QRegExp>
#include <QPixmap>

#include "listutils.h"
#include "coverage.h"
#include "globalconfig.h"


// CostListItem


CostListItem::CostListItem(Q3ListView* parent, TraceCostItem* costItem,
                           TraceEventType* ct, int size)
  :Q3ListViewItem(parent)
{
  _groupSize = size;
  _skipped = 0;
  _costItem = costItem;
  setCostType(ct);

  if (costItem) {
    updateName();
    setPixmap(1, colorPixmap(10, 10,
			     GlobalConfig::groupColor(_costItem)));
  }
}

CostListItem::CostListItem(Q3ListView* parent, int skipped,
			   TraceCostItem* costItem, TraceEventType* ct)
  :Q3ListViewItem(parent)
{
  _skipped = skipped;
  _costItem = costItem;
  setCostType(ct);

  setText(1, QObject::tr("(%n item(s) skipped)", "", _skipped));
}

void CostListItem::setCostType(TraceEventType* ct)
{
  _costType = ct;
  update();
}

void CostListItem::updateName()
{
  if (!_costItem) return;

  QString n = _costItem->prettyName();
  if (_groupSize>=0) n += QString(" (%1)").arg(_groupSize);

  setText(1, n);
}

void CostListItem::setSize(int s)
{
  _groupSize = s;
  updateName();
}

void CostListItem::update()
{
  if (!_costItem) return;
  TraceData* d = _costItem->data();

  double total = d->subCost(_costType);
  if (total == 0.0) {
    setText(0, QString("---"));
    setPixmap(0, QPixmap());
    return;
  }

  _pure = _costItem->subCost(_costType);
  double pure  = 100.0 * _pure / total;
  QString str;
  if (GlobalConfig::showPercentage())
    str = QString("%1").arg(pure, 0, 'f', GlobalConfig::percentPrecision());
  else
    str = _costItem->prettySubCost(_costType);

  if (_skipped) {
    // special handling for skip entries...
    setText(0, QString("< %1").arg(str));
    return;
  }

  setText(0, str);
  setPixmap(0, costPixmap(_costType, _costItem, total, false));
}

int CostListItem::compare(Q3ListViewItem * i, int col, bool ascending ) const
{
  const CostListItem* fi1 = this;
  const CostListItem* fi2 = (CostListItem*) i;

  // we always want descending order
  if (ascending) {
    fi1 = fi2;
    fi2 = this;
  }

  // a skip entry is always sorted last
  if (fi1->_skipped) return -1;
  if (fi2->_skipped) return 1;

  if (col==0) {
    if (fi1->_pure < fi2->_pure) return -1;
    if (fi1->_pure > fi2->_pure) return 1;
    return 0;
  }
  return Q3ListViewItem::compare(i, col, ascending);
}
