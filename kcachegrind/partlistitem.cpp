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

#include <math.h>

#include <qpainter.h>
#include <qregexp.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kapplication.h>

#include "listutils.h"
#include "partlistitem.h"
#include "coverage.h"
#include "configuration.h"


// PartListItem

PartListItem::PartListItem(QListView* parent, TraceCostItem* costItem,
                           TraceCostType* ct, TraceCost::CostType gt,
			   TracePart* part)
  :QListViewItem(parent)
{
  _partCostItem = costItem->findDepFromPart(part);
  _part = part;
  _groupType = gt;
  _costType = ct;

#if 0
  QString partName = QString::number(part->partNumber());
  if (part->data()->maxThreadID() >1)
      partName += i18n(" (Thread %1)").arg(part->threadID());
  setText(0, partName);
#else
  setText(0, _part->prettyName());
#endif

  if (_part->trigger().isEmpty())
    setText(4,i18n("(none)"));
  else
    setText(4, _part->trigger());

  update();
}

void PartListItem::setCostType(TraceCostType* ct)
{
    if (_costType == ct) return;

    _costType = ct;
    update();
}

void PartListItem::setGroupType(TraceCost::CostType gt)
{
    if (_groupType == gt) return;

    _groupType = gt;
    update();
}

void PartListItem::update()
{
  TracePartFunction* pf;
  pf = !_partCostItem ? 0 :
       (_partCostItem->type()==TraceCost::PartFunction) ?
       ((TracePartFunction*)_partCostItem) : 0;

  double total = _part->subCost(_costType);

  TraceCost* selfTotalCost = _part;
  if (pf && Configuration::showExpanded()) {
      switch(_groupType) {
      case TraceCost::Object: selfTotalCost = pf->partObject(); break;
      case TraceCost::Class:  selfTotalCost = pf->partClass(); break;
      case TraceCost::File:   selfTotalCost = pf->partFile(); break;
      default: break;
      }
  }
  double selfTotal = selfTotalCost->subCost(_costType);

  _pure = _partCostItem ? _partCostItem->subCost(_costType) : SubCost(0);
  _sum = pf ? pf->inclusive()->subCost(_costType) : SubCost(0);

  if (selfTotal == 0 || !_partCostItem) {
    setText(2, QString("-"));
    setPixmap(2, QPixmap());
  }
  else {
    double pure  = 100.0 * _pure / selfTotal;
    if (Configuration::showPercentage()) {
      setText(2, QString("%1")
              .arg(pure, 0, 'f', Configuration::percentPrecision()));
    }
    else
      setText(2, _partCostItem->prettySubCost(_costType));

    setPixmap(2, costPixmap(_costType, _partCostItem, selfTotal, false));
  }

  if (total == 0 || !pf) {
    setText(1, QString("-"));
    setPixmap(1, QPixmap());
  }
  else {
    double sum  = 100.0 * _sum / total;
    if (Configuration::showPercentage()) {
      setText(1, QString("%1")
              .arg(sum, 0, 'f', Configuration::percentPrecision()));
    }
    else
      setText(1, _sum.pretty());

    setPixmap(1, costPixmap(_costType, pf->inclusive(), total, false));
  }

  if (!pf) {
    setText(3, QString("-"));
    _callers = 0;
    return;
  }

  TracePartCall* pc;
  TracePartCallList pl;
  SubCost callers, callees;
  QString str;

  callers = 0;
  pl = pf->partCallers();
  for (pc=pl.first();pc;pc=pl.next()) {
    callers += pc->callCount();
  }

  if ((callers == 0) && (pf->calledContexts()>0))
    str = i18n("(active)");
  else
    str = callers.pretty();

  _callers = callers;
  setText(3, str);
}


int PartListItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  PartListItem* fi = (PartListItem*) i;
  if (col==0) {
      int mTID = _part->data()->maxThreadID()+1;
      int mNum = _part->data()->maxPartNumber()+1;

      return
	  (_part->processID()  - fi->_part->processID()) * mTID * mNum +
	  (_part->partNumber() - fi->_part->partNumber()) * mTID +
	  (_part->threadID()   - fi->_part->threadID());
  }
  if (col==1) {
    if (_sum < fi->_sum) return -1;
    if (_sum > fi->_sum) return 1;
    return 0;
  }
  if (col==2) {
    if (_pure < fi->_pure) return -1;
    if (_pure > fi->_pure) return 1;
    return 0;
  }
  if (col==3) {
    if (_callers < fi->_callers) return -1;
    if (_callers > fi->_callers) return 1;
    return 0;
  }
  return QListViewItem::compare(i, col, ascending);
}
