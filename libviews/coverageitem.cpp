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
 * Items of coverage view.
 */

#include "coverageitem.h"


#include "globalguiconfig.h"
#include "listutils.h"
#include "coverage.h"


// CallerCoverageItem


CallerCoverageItem::CallerCoverageItem(QTreeWidget* parent, Coverage* c,
                                       TraceFunction* base,
                                       EventType* ct,
                                       ProfileContext::Type gt)
    : QTreeWidgetItem(parent)
{
  _skipped = 0;
  _coverage = c;
  _function = c->function();
  _base = base;
  _groupType = ProfileContext::InvalidType;

  setText(3, _function->prettyNameWithLocation());
  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);
  setCostType(ct);
  setGroupType(gt);
}

CallerCoverageItem::CallerCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
                                       TraceFunction* base,
                                       EventType* ct,
                                       ProfileContext::Type gt)
    : QTreeWidgetItem(parent)
{
  _skipped = skipped;
  _coverage = c;
  _function = c->function();
  _base = base;
  _groupType = ProfileContext::InvalidType;

  setText(3, QObject::tr("(%n function(s) skipped)", "", _skipped));
  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);
  setCostType(ct);
  setGroupType(gt);
}

void CallerCoverageItem::setGroupType(ProfileContext::Type gt)
{
  if (_skipped) return;
  if (_groupType == gt) return;
  _groupType = gt;

  QColor c = GlobalGUIConfig::functionColor(_groupType, _function);
  setIcon(3, colorPixmap(10, 10, c));
}

void CallerCoverageItem::setCostType(EventType* ct)
{
  _costType = ct;
  update();
}

void CallerCoverageItem::update()
{
  if (!_coverage) {
    setText(0, QString());
    setText(1, QString());
    return;
  }

  _pSum = 100.0 * _coverage->inclusive();
  SubCost realSum = _base->inclusive()->subCost(_costType);
  _sum = SubCost(realSum * _coverage->inclusive());
  QString str;
  if (GlobalConfig::showPercentage())
    str = QString("%1").arg(_pSum, 0, 'f', GlobalConfig::percentPrecision());
  else
    str = _sum.pretty();

  if (_skipped) {
    setText(0, QString("< %1").arg(str));
    return;
  }

  setText(0, str);
  setIcon(0, partitionPixmap(25, 10, _coverage->inclusiveHistogram(), 0,
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


bool CallerCoverageItem::operator<( const QTreeWidgetItem & other ) const
{
  const CallerCoverageItem* ci1 = this;
  const CallerCoverageItem* ci2 = (CallerCoverageItem*) &other;
  int col = treeWidget()->sortColumn();

  // a skip entry is always sorted last
  if (ci1->_skipped) return true;
  if (ci2->_skipped) return false;

  if (col==0) {
    if (ci1->_pSum < ci2->_pSum) return true;
    if (ci1->_pSum > ci2->_pSum) return false;

    // for same percentage (e.g. all 100%), use distance info
    return ci1->_distance < ci2->_distance;
  }

  if (col==1) {
    return ci1->_distance < ci2->_distance;
  }

  if (col==2) {
    return ci1->_cc < ci2->_cc;
  }

  return QTreeWidgetItem::operator <(other);
}



// CalleeCoverageItem

CalleeCoverageItem::CalleeCoverageItem(QTreeWidget* parent, Coverage* c,
                                       TraceFunction* base,
                                       EventType* ct,
                                       ProfileContext::Type gt)
    : QTreeWidgetItem(parent)
{
  _skipped = 0;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = ProfileContext::InvalidType;

  if ( _function )
     setText(4, _function->prettyNameWithLocation());

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);
  setTextAlignment(3, Qt::AlignRight);

  setCostType(ct);
  setGroupType(gt);
}

CalleeCoverageItem::CalleeCoverageItem(QTreeWidget* parent, int skipped, Coverage* c,
                                       TraceFunction* base,
                                       EventType* ct,
                                       ProfileContext::Type gt)
    : QTreeWidgetItem(parent)
{
  _skipped = skipped;
  _coverage = c;
  _function = c ? c->function() : 0;
  _base = base;
  _groupType = ProfileContext::InvalidType;

  setText(4, QObject::tr("(%n function(s) skipped)", "", _skipped));

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);
  setTextAlignment(3, Qt::AlignRight);

  setCostType(ct);
  setGroupType(gt);
}

void CalleeCoverageItem::setGroupType(ProfileContext::Type gt)
{
  if (_skipped) return;
  if (_groupType == gt) return;
  _groupType = gt;

  QColor c = GlobalGUIConfig::functionColor(_groupType, _function);
  setIcon(4, colorPixmap(10, 10, c));
}

void CalleeCoverageItem::setCostType(EventType* ct)
{
  _costType = ct;
  update();
}

void CalleeCoverageItem::update()
{
  if (!_coverage) {
    setText(0, QString());
    setText(1, QString());
    setText(2, QString());
    return;
  }

  _pSum = 100.0 * _coverage->inclusive();

  // pSum/pSelf are percentages of inclusive cost of base
  SubCost realSum = _base->inclusive()->subCost(_costType);
  _sum = SubCost(realSum * _coverage->inclusive());


  QString str;
  if (GlobalConfig::showPercentage())
    str = QString("%1").arg(_pSum, 0, 'f', GlobalConfig::percentPrecision());
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

 if (GlobalConfig::showPercentage()) {
    setText(1, QString("%1")
            .arg(_pSelf, 0, 'f', GlobalConfig::percentPrecision()));
  }
  else {
    setText(1, _self.pretty());
  }

  setIcon(0, partitionPixmap(25, 10, _coverage->inclusiveHistogram(), 0,
                               Coverage::maxHistogramDepth, false));
  setIcon(1, partitionPixmap(25, 10, _coverage->selfHistogram(), 0,
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


bool CalleeCoverageItem::operator<( const QTreeWidgetItem & other ) const
{
  CalleeCoverageItem* ci = (CalleeCoverageItem*) &other;
  int col = treeWidget()->sortColumn();
  // a skip entry is always sorted last
  if (_skipped) return true;
  if (ci->_skipped) return false;

  if (col==0) {
    if (_pSum < ci->_pSum) return true;
    if (_pSum > ci->_pSum) return false;

    // for same percentage (e.g. all 100%), use distance info
    return _distance < ci->_distance;

  }

  if (col==1) {
    if (_pSelf < ci->_pSelf) return true;
    if (_pSelf > ci->_pSelf) return false;

    // for same percentage (e.g. all 100%), use distance info
    return _distance < ci->_distance;
  }

  if (col==2) {
    // we want to sort the distance in contra direction to costs
    return _distance < ci->_distance;
  }

  if (col==3) {
    return _cc < ci->_cc;
  }
  return QTreeWidgetItem::operator <(other);
}


