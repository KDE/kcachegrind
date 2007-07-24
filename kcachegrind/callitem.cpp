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

/*
 * Items for caller/callee view.
 */

#include <qpixmap.h>
#include <klocale.h>
#include <kiconloader.h>

#include "configuration.h"
#include "listutils.h"
#include "callitem.h"
#include "callview.h"
#include "toplevel.h"

// CallItem


CallItem::CallItem(CallView* view, Q3ListView* parent, TraceCall* c)
    : Q3ListViewItem(parent)
{
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

  setText(3, fName);
  updateGroup();
  updateCost();
}

void  CallItem::updateGroup()
{
  QColor c = Configuration::functionColor(_view->groupType(), _shown);
  setPixmap(3, colorPixmap(10, 10, c));
}

void CallItem::updateCost()
{
    bool sameCycle = _shown->cycle() && (_active->cycle() == _shown->cycle());
    bool shownIsCycle = (_shown == _shown->cycle());
    bool selectedIsCycle = (_active == _active->cycle());
    if (_call->isRecursion()) sameCycle=true;

    QString cStr;
    if ((selectedIsCycle || shownIsCycle) && sameCycle)
	cStr = "-";
    else {
	_cc  = _call->callCount();
	if (_cc == 0)
	    cStr = i18n("(active)");
	else
	    cStr = _call->prettyCallCount();
    }
    setText(2, cStr);

    TraceCost* totalCost;
    if (_view->topLevel()->showExpanded()) {
      if (_active->cycle())
        totalCost = _active->cycle()->inclusive();
      else
	totalCost = _active->inclusive();
    }
    else
	totalCost = _active->data();

    TraceEventType* ct = _view->eventType();
    _sum = _call->subCost(ct);
    double total = totalCost->subCost(ct);

    if (total == 0.0) {
	QString str = "-";

	setText(0, str);
	setPixmap(0, QPixmap());
    }
    else {
	double sum  = 100.0 * _sum / total;

	if (_view->topLevel()->showPercentage())
	    setText(0, QString("%1")
		    .arg(sum, 0, 'f', Configuration::percentPrecision()));
	else
	    setText(0, _call->prettySubCost(ct));

	setPixmap(0, costPixmap(ct, _call, total, false));
    }

    // Cost Type 2
    TraceEventType* ct2 = _view->eventType2();
    if (ct2) {
      _sum2 = _call->subCost(ct2);
      double total = totalCost->subCost(ct2);

      if (total == 0.0) {
	QString str = "-";

	setText(1, str);
	setPixmap(1, QPixmap());
      }
      else {
	double sum  = 100.0 * _sum2 / total;
	
	if (_view->topLevel()->showPercentage())
	  setText(1, QString("%1")
		  .arg(sum, 0, 'f', Configuration::percentPrecision()));
	else
	  setText(1, _call->prettySubCost(ct2));

	setPixmap(1, costPixmap(ct2, _call, total, false));
      }
    }

    QPixmap p;
    if (sameCycle && !selectedIsCycle && !shownIsCycle) {

	QString icon = "edit-undo";
	KIconLoader* loader = KIconLoader::global();
	p= loader->loadIcon(icon, K3Icon::Small, 0,
			    K3Icon::DefaultState, QStringList(), 0, true);
    }
    setPixmap(2, p);
}


int CallItem::compare(Q3ListViewItem * i, int col, bool ascending ) const
{
  const CallItem* ci1 = this;
  const CallItem* ci2 = (CallItem*) i;

  // we always want descending order
  if (ascending) {
    ci1 = ci2;
    ci2 = this;
  }

  if (col==0) {
    if (ci1->_sum < ci2->_sum) return -1;
    if (ci1->_sum > ci2->_sum) return 1;
    return 0;
  }
  if (col==1) {
    if (ci1->_sum2 < ci2->_sum2) return -1;
    if (ci1->_sum2 > ci2->_sum2) return 1;
    return 0;
  }
  if (col==2) {
    if (ci1->_cc < ci2->_cc) return -1;
    if (ci1->_cc > ci2->_cc) return 1;
    return 0;
  }
  return Q3ListViewItem::compare(i, col, ascending);
}

