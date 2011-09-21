/* This file is part of KCachegrind.
   Copyright (C) 2002-2011 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <QPixmap>

#include "listutils.h"
#include "coverage.h"
#include "globalguiconfig.h"


// CostListItem


CostListItem::CostListItem(QTreeWidget* parent, TraceCostItem* costItem,
                           EventType* et, int size)
  :QTreeWidgetItem(parent)
{
  _groupSize = size;
  _skipped = 0;
  _costItem = costItem;
  setEventType(et);

  setTextAlignment(0, Qt::AlignRight);

  if (costItem) {
    updateName();
    setIcon(1, colorPixmap(10, 10,
                           GlobalGUIConfig::groupColor(_costItem)));
  }
}

CostListItem::CostListItem(QTreeWidget* parent, int skipped,
                           TraceCostItem* costItem, EventType* et)
  :QTreeWidgetItem(parent)
{
  _skipped = skipped;
  _costItem = costItem;
  setEventType(et);

  setTextAlignment(0, Qt::AlignRight);

  setText(1, QObject::tr("(%n item(s) skipped)", "", _skipped));
}

void CostListItem::setEventType(EventType* et)
{
  _eventType = et;
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

  double total = d->subCost(_eventType);
  if (total == 0.0) {
    setText(0, QString("---"));
    setIcon(0, QPixmap());
    return;
  }

  _pure = _costItem->subCost(_eventType);
  double pure  = 100.0 * _pure / total;
  QString str;
  if (GlobalConfig::showPercentage())
    str = QString("%1").arg(pure, 0, 'f', GlobalConfig::percentPrecision());
  else
    str = _costItem->prettySubCost(_eventType);

  if (_skipped) {
    // special handling for skip entries...
    setText(0, QString("< %1").arg(str));
    return;
  }

  setText(0, str);
  setIcon(0, costPixmap(_eventType, _costItem, total, false));
}

bool CostListItem::operator< ( const QTreeWidgetItem & other ) const
{
  const CostListItem* fi1 = this;
  const CostListItem* fi2 = (CostListItem*) &other;
  int col = treeWidget()->sortColumn();

  // a skip entry is always sorted last
  if (fi1->_skipped) return true;
  if (fi2->_skipped) return false;

  if (col==0)
      return (fi1->_pure < fi2->_pure);

  return QTreeWidgetItem::operator <(other);
}
