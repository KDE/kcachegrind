/* This file is part of KCachegrind.
   Copyright (C) 2003-2011 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "partlistitem.h"

#include <math.h>

#include <QPixmap>

#include "listutils.h"
#include "coverage.h"
#include "globalconfig.h"


// PartListItem

PartListItem::PartListItem(QTreeWidget* parent, TraceCostItem* costItem,
			   EventType* et, ProfileContext::Type gt,
			   TracePart* part)
  :QTreeWidgetItem(parent)
{
  _partCostItem = costItem->findDepFromPart(part);
  _part = part;
  _groupType = gt;
  _eventType = et;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  setText(0, _part->prettyName());

  if (_part->trigger().isEmpty())
    setText(4, QObject::tr("(none)"));
  else
    setText(4, _part->trigger());

  update();
}

void PartListItem::setEventType(EventType* et)
{
    if (_eventType == et) return;

    _eventType = et;
    update();
}

void PartListItem::setGroupType(ProfileContext::Type gt)
{
    if (_groupType == gt) return;

    _groupType = gt;
    update();
}

void PartListItem::update()
{
  TracePartFunction* pf;
  pf = !_partCostItem ? 0 :
       (_partCostItem->type()==ProfileContext::PartFunction) ?
       ((TracePartFunction*)_partCostItem) : 0;

  double total = _part->subCost(_eventType);

  ProfileCostArray* selfTotalCost = _part;
  if (pf && GlobalConfig::showExpanded()) {
      switch(_groupType) {
      case ProfileContext::Object: selfTotalCost = pf->partObject(); break;
      case ProfileContext::Class:  selfTotalCost = pf->partClass(); break;
      case ProfileContext::File:   selfTotalCost = pf->partFile(); break;
      default: break;
      }
  }
  double selfTotal = selfTotalCost->subCost(_eventType);

  _pure = _partCostItem ? _partCostItem->subCost(_eventType) : SubCost(0);
  _sum = pf ? pf->inclusive()->subCost(_eventType) : SubCost(0);

  if (selfTotal == 0 || !_partCostItem) {
    setText(2, QString("-"));
    setIcon(2, QPixmap());
  }
  else {
    double pure  = 100.0 * _pure / selfTotal;
    if (GlobalConfig::showPercentage()) {
      setText(2, QString("%1")
              .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
    }
    else
      setText(2, _partCostItem->prettySubCost(_eventType));

    setIcon(2, costPixmap(_eventType, _partCostItem, selfTotal, false));
  }

  if (total == 0 || !pf) {
    setText(1, QString("-"));
    setIcon(1, QPixmap());
  }
  else {
    double sum  = 100.0 * _sum / total;
    if (GlobalConfig::showPercentage()) {
      setText(1, QString("%1")
              .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
    }
    else
      setText(1, _sum.pretty());

    setIcon(1, costPixmap(_eventType, pf->inclusive(), total, false));
  }

  if (!pf) {
    setText(3, QString("-"));
    _callCount = 0;
    return;
  }

  SubCost callCount;
  QString str;

  callCount = 0;
  foreach(TracePartCall* pc, pf->partCallers())
    callCount += pc->callCount();

  if ((callCount == 0) && (pf->calledContexts()>0))
    str = QObject::tr("(active)");
  else
    str = callCount.pretty();

  _callCount = callCount;
  setText(3, str);
}

bool PartListItem::operator<(const QTreeWidgetItem& other) const
{
    const PartListItem* pi1 = this;
    const PartListItem* pi2 = (PartListItem*) &other;
    int col = treeWidget()->sortColumn();

    if (col==0)
        return (pi1->_part < pi2->_part);

    if (col==1)
        return (pi1->_sum < pi2->_sum);

    if (col==2)
        return (pi1->_pure < pi2->_pure);

    if (col==3)
        return (pi1->_callCount < pi2->_callCount);

    return QTreeWidgetItem::operator <(other);
}
