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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <math.h>

#include <qpainter.h>
#include <qregexp.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kapplication.h>

#include "listutils.h"
#include "costlistitem.h"
#include "coverage.h"
#include "configuration.h"

// CostListItem


CostListItem::CostListItem(QListView* parent, TraceCostItem* costItem,
                           TraceCostType* ct, int size)
  :QListViewItem(parent)
{
  _groupSize = size;
  _skipped = 0;
  _costItem = costItem;
  setCostType(ct);

  if (costItem) {
    updateName();
    setPixmap(1, colorPixmap(10, 10,
			     Configuration::groupColor(_costItem)));
  }
}

CostListItem::CostListItem(QListView* parent, int skipped,
			   TraceCostItem* costItem, TraceCostType* ct)
  :QListViewItem(parent)
{
  _skipped = skipped;
  _costItem = costItem;
  setCostType(ct);

  setText(1, i18n("(%n item skipped)", "(%n items skipped)", _skipped));
}

void CostListItem::setCostType(TraceCostType* ct)
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
  if (Configuration::showPercentage())
    str = QString("%1").arg(pure, 0, 'f', Configuration::percentPrecision());
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

int CostListItem::compare(QListViewItem * i, int col, bool ascending ) const
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
  return QListViewItem::compare(i, col, ascending);
}
