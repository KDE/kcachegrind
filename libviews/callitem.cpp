/* This file is part of KCachegrind.
   Copyright (c) 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Items for caller/callee view.
 */

#include "callitem.h"

#include <QFontMetrics>

#include "globalguiconfig.h"
#include "listutils.h"
#include "callview.h"


// CallItem


CallItem::CallItem(CallView* view, QTreeWidget* parent, TraceCall* c)
    : QTreeWidgetItem(parent)
{
    for (int i = 0 ; i < 5; ++i)
        setTextAlignment(i, Qt::AlignRight);

  _call = c;
  _view = view;

  _active = _view->activeFunction();
  bool baseIsCycle = (_active && (_active == _active->cycle()));

  QString fName;
  if (_view->showCallers()) {
      _shown = _call->caller(true);
      fName  = c->callerName(!baseIsCycle);
  }
  else {
      _shown = _call->called(true);
      fName = c->calledName(!baseIsCycle);
  }

  _shown->addPrettyLocation(fName);

  setText(5, fName);
  updateGroup();
  updateCost();
}

void  CallItem::updateGroup()
{
  QColor c = GlobalGUIConfig::functionColor(_view->groupType(), _shown);
  setIcon(5, colorPixmap(10, 10, c));
}

void CallItem::updateCost()
{
    bool sameCycle = _shown->cycle() && (_active->cycle() == _shown->cycle());
    bool shownIsCycle = (_shown == _shown->cycle());
    bool selectedIsCycle = (_active == _active->cycle());
    if (_call->isRecursion()) sameCycle=true;

    QString cStr;
    if ((selectedIsCycle || shownIsCycle) && sameCycle)
	cStr = QStringLiteral("-");
    else {
	_cc  = _call->callCount();
	if (_cc == 0)
	    cStr = QObject::tr("(active)");
	else
	    cStr = _call->prettyCallCount();
    }
    setText(4, cStr);

    ProfileCostArray* totalCost;
    if (GlobalConfig::showExpanded()) {
      if (_active->cycle())
        totalCost = _active->cycle()->inclusive();
      else
	totalCost = _active->inclusive();
    }
    else
	totalCost = _active->data();

    EventType* ct = _view->eventType();
    _sum = _call->subCost(ct);
    double total = totalCost->subCost(ct);

    if (total == 0.0) {
	QString str = QStringLiteral("-");

	setText(0, str);
        setIcon(0, QIcon());
    }
    else {
	double sum  = 100.0 * _sum / total;

	if (GlobalConfig::showPercentage())
	    setText(0, QStringLiteral("%1")
		    .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
	else {
	    setText(0, _call->prettySubCost(ct));
        }
        setIcon(0, costPixmap(ct, _call, total, false));
        setText(1, _call->prettySubCostPerCall(ct, _cc));
    }

    // Cost Type 2
    EventType* ct2 = _view->eventType2();
    if (ct2) {
      _sum2 = _call->subCost(ct2);
      double total = totalCost->subCost(ct2);

      if (total == 0.0) {
	QString str = QStringLiteral("-");

	setText(2, str);
        setIcon(2, QIcon());
      }
      else {
	double sum  = 100.0 * _sum2 / total;
	
	if (GlobalConfig::showPercentage())
	  setText(2, QStringLiteral("%1")
		  .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
	else {
	  setText(2, _call->prettySubCost(ct2));
        }
        setIcon(2, costPixmap(ct2, _call, total, false));
	setText(3, _call->prettySubCostPerCall(ct2, _cc));
      }
    }

    QIcon p;
    if (sameCycle && !selectedIsCycle && !shownIsCycle) {
        QFontMetrics fm(font(4));
        p = QIcon::fromTheme(QStringLiteral("edit-undo")).pixmap(fm.height());
    }
    setIcon(4, p);
}


bool CallItem::operator<(const QTreeWidgetItem& other) const
{
  int col = treeWidget()->sortColumn();
  const CallItem* ci1 = this;
  const CallItem* ci2 = (CallItem*) &other;

  if (col==0)
    return ci1->_sum < ci2->_sum;

  if (col==1) {
    uint64 cc1 = ci1->_cc;
    uint64 cc2 = ci2->_cc;
    if (cc1 == 0) cc1 = 1;
    if (cc2 == 0) cc2 = 1;
    return (ci1->_sum / cc1) < (ci2->_sum / cc2);
  }

  if (col==2)
    return ci1->_sum2 < ci2->_sum2;

  if (col==3) {
    uint64 cc1 = ci1->_cc;
    uint64 cc2 = ci2->_cc;
    if (cc1 == 0) cc1 = 1;
    if (cc2 == 0) cc2 = 1;
    return (ci1->_sum2 / cc1) < (ci2->_sum2 / cc2);
  }

  if (col==4)
   return ci1->_cc < ci2->_cc;

  return QTreeWidgetItem::operator <(other);
}

