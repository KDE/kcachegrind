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
 * Items of coverage view.
 */

#include <qpixmap.h>
#include <klocale.h>

#include "configuration.h"
#include "listutils.h"
#include "coverage.h"
#include "coverageitem.h"


// CallerCoverageItem


CallerCoverageItem::CallerCoverageItem(QListView* parent, Coverage* c,
                                       TraceFunction* base,
                                       TraceCostType* ct,
                                       TraceCost::CostType gt)
    : QListViewItem(parent)
{
  _skipped = 0;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = TraceCost::NoCostType;

  setText(3, _function->prettyNameWithLocation());

  setCostType(ct);
  setGroupType(gt);
}

CallerCoverageItem::CallerCoverageItem(QListView* parent, int skipped, Coverage* c,
                                       TraceFunction* base,
                                       TraceCostType* ct,
                                       TraceCost::CostType gt)
    : QListViewItem(parent)
{
  _skipped = skipped;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = TraceCost::NoCostType;

  setText(3, i18n("(%n function skipped)", "(%n functions skipped)", _skipped));

  setCostType(ct);
  setGroupType(gt);
}

void CallerCoverageItem::setGroupType(TraceCost::CostType gt)
{
  if (_skipped) return;
  if (_groupType == gt) return;
  _groupType = gt;

  QColor c = Configuration::functionColor(_groupType, _function);
  setPixmap(3, colorPixmap(10, 10, c));
}

void CallerCoverageItem::setCostType(TraceCostType* ct)
{
  _costType = ct;
  update();
}

void CallerCoverageItem::update()
{
  if (!_coverage) {
    setText(0, QString::null);
    setText(1, QString::null);
    return;
  }

  _pSum = 100.0 * _coverage->inclusive();
  SubCost realSum = _base->inclusive()->subCost(_costType);
  _sum = SubCost(realSum * _coverage->inclusive());
  QString str;
  if (Configuration::showPercentage())
    str = QString("%1").arg(_pSum, 0, 'f', Configuration::percentPrecision());
  else
    str = _sum.pretty();

  if (_skipped) {
    setText(0, QString("< %1").arg(str));
    return;
  }

  setText(0, str);
  setPixmap(0, partitionPixmap(25, 10, _coverage->inclusiveHistogram(), 0,
                               Coverage::maxHistogramDepth, false));

  // call count
  _cc  = SubCost(_coverage->callCount());
  setText(2, _cc ? _cc.pretty() : QString("(0)"));

  // distance (min/max/median)
  _distance = _coverage->inclusiveMedian();
  QString distString;
  if (_coverage->minDistance() == _coverage->maxDistance())
    distString = QString::number(_distance);
  else
    distString = QString("%1-%2 (%3)")
                 .arg(_coverage->minDistance())
                 .arg(_coverage->maxDistance())
                 .arg(_distance);
  setText(1, distString);
}


int CallerCoverageItem::compare(QListViewItem * i,
                                int col, bool ascending ) const
{
  const CallerCoverageItem* ci1 = this;
  const CallerCoverageItem* ci2 = (CallerCoverageItem*) i;

  // we always want descending order
  if (ascending) {
    ci1 = ci2;
    ci2 = this;
  }

  // a skip entry is always sorted last
  if (ci1->_skipped) return -1;
  if (ci2->_skipped) return 1;

  if (col==0) {
    if (ci1->_pSum < ci2->_pSum) return -1;
    if (ci1->_pSum > ci2->_pSum) return 1;

    // for same percentage (e.g. all 100%), use distance info
    if (ci1->_distance < ci2->_distance) return -1;
    if (ci1->_distance > ci2->_distance) return 1;
    return 0;
  }

  if (col==1) {
    if (ci1->_distance < ci2->_distance) return -1;
    if (ci1->_distance > ci2->_distance) return 1;
    return 0;
  }

  if (col==2) {
    if (ci1->_cc < ci2->_cc) return -1;
    if (ci1->_cc > ci2->_cc) return 1;
    return 0;
  }
  return QListViewItem::compare(i, col, ascending);
}


// CalleeCoverageItem


CalleeCoverageItem::CalleeCoverageItem(QListView* parent, Coverage* c,
                                       TraceFunction* base,
                                       TraceCostType* ct,
                                       TraceCost::CostType gt)
    : QListViewItem(parent)
{
  _skipped = 0;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = TraceCost::NoCostType;

  setText(4, _function->prettyNameWithLocation());

  setCostType(ct);
  setGroupType(gt);
}

CalleeCoverageItem::CalleeCoverageItem(QListView* parent, int skipped, Coverage* c,
                                       TraceFunction* base,
                                       TraceCostType* ct,
                                       TraceCost::CostType gt)
    : QListViewItem(parent)
{
  _skipped = skipped;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = TraceCost::NoCostType;

  setText(4, i18n("(%n function skipped)", "(%n functions skipped)", _skipped));

  setCostType(ct);
  setGroupType(gt);
}

void CalleeCoverageItem::setGroupType(TraceCost::CostType gt)
{
  if (_skipped) return;
  if (_groupType == gt) return;
  _groupType = gt;

  QColor c = Configuration::functionColor(_groupType, _function);
  setPixmap(4, colorPixmap(10, 10, c));
}

void CalleeCoverageItem::setCostType(TraceCostType* ct)
{
  _costType = ct;
  update();
}

void CalleeCoverageItem::update()
{
  if (!_coverage) {
    setText(0, QString::null);
    setText(1, QString::null);
    setText(2, QString::null);
    return;
  }

  _pSum = 100.0 * _coverage->inclusive();

  // pSum/pSelf are percentages of inclusive cost of base
  SubCost realSum = _base->inclusive()->subCost(_costType);
  _sum = SubCost(realSum * _coverage->inclusive());


  QString str;
  if (Configuration::showPercentage())
    str = QString("%1").arg(_pSum, 0, 'f', Configuration::percentPrecision());
  else
    str = _sum.pretty();

  if (_skipped) {
    str = QString("< %1").arg(str);
    setText(0, str);
    setText(1, str);
    return;
  }
 setText(0, str);

 _pSelf = 100.0 * _coverage->self();
 _self = SubCost(realSum * _coverage->self());

 if (Configuration::showPercentage()) {
    setText(1, QString("%1")
            .arg(_pSelf, 0, 'f', Configuration::percentPrecision()));
  }
  else {
    setText(1, _self.pretty());
  }

  setPixmap(0, partitionPixmap(25, 10, _coverage->inclusiveHistogram(), 0,
                               Coverage::maxHistogramDepth, false));
  setPixmap(1, partitionPixmap(25, 10, _coverage->selfHistogram(), 0,
                               Coverage::maxHistogramDepth, false));


  _cc  = SubCost(_coverage->callCount());
  setText(3, _cc ? _cc.pretty() : QString("(0)"));

  // for comparations
  _distance = _coverage->inclusiveMedian();
  QString distString;
  if (_coverage->minDistance() == _coverage->maxDistance())
    distString = QString::number(_distance);
  else {
    int sMed = _coverage->selfMedian();
    QString med;
    if (_distance == sMed)
      med = QString::number(_distance);
    else
      med = QString("%1/%2").arg(_distance).arg(sMed);

    distString = QString("%1-%2 (%3)")
                 .arg(_coverage->minDistance())
                 .arg(_coverage->maxDistance())
                 .arg(med);
  }
  setText(2, distString);
}


int CalleeCoverageItem::compare(QListViewItem * i,
                                int col, bool ascending ) const
{
  CalleeCoverageItem* ci = (CalleeCoverageItem*) i;

  // a skip entry is always sorted last
  if (_skipped) return -1;
  if (ci->_skipped) return 1;

  if (col==0) {
    if (_pSum < ci->_pSum) return -1;
    if (_pSum > ci->_pSum) return 1;

    // for same percentage (e.g. all 100%), use distance info
    if (_distance < ci->_distance) return -1;
    if (_distance > ci->_distance) return 1;
    return 0;
  }

  if (col==1) {
    if (_pSelf < ci->_pSelf) return -1;
    if (_pSelf > ci->_pSelf) return 1;

    // for same percentage (e.g. all 100%), use distance info
    if (_distance < ci->_distance) return -1;
    if (_distance > ci->_distance) return 1;
    return 0;
  }

  if (col==2) {
    // we want to sort the distance in contra direction to costs
    if (_distance < ci->_distance) return  1;
    if (_distance > ci->_distance) return -1;
    return 0;
  }

  if (col==3) {
    if (_cc < ci->_cc) return -1;
    if (_cc > ci->_cc) return 1;
    return 0;
  }
  return QListViewItem::compare(i, col, ascending);
}


