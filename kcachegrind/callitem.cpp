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

/*
 * Items for caller/callee view.
 */

#include <qpixmap.h>
#include <klocale.h>
#include <kapplication.h>
#include <kiconloader.h>

#include "configuration.h"
#include "listutils.h"
#include "callitem.h"
#include "callview.h"
#include "toplevel.h"

// CallItem


CallItem::CallItem(CallView* view, QListView* parent, TraceCall* c)
    : QListViewItem(parent)
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

  if (_shown->object() && _shown->object()->name() != QString("???"))
      fName += QString(" (%1)").arg(_shown->object()->shortName());

  setText(2, fName);
  updateGroup();
  updateCost();
}

void  CallItem::updateGroup()
{
  QColor c = Configuration::functionColor(_view->groupType(), _shown);
  setPixmap(2, colorPixmap(10, 10, c));
}

void CallItem::updateCost()
{
    TraceCostType* ct = _view->costType();
    _sum = _call->subCost(ct);

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
    setText(1, cStr);

    TraceCost* totalCost;
    if (_view->topLevel()->showExpanded()) {
      if (_active->cycle())
        totalCost = _active->cycle()->cumulative();
      else
	totalCost = _active->cumulative();
    }
    else
	totalCost = _active->data();

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

	setPixmap(0, costPixmap(ct, _call, total));
    }

    QPixmap p;
    if (sameCycle && !selectedIsCycle && !shownIsCycle) {

	QString icon = "undo";
	KIconLoader* loader = KApplication::kApplication()->iconLoader();
	p= loader->loadIcon(icon, KIcon::Small, 0,
			    KIcon::DefaultState, 0, true);
    }
    setPixmap(1, p);
}


int CallItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  CallItem* ci = (CallItem*) i;
  if (col==0) {
    if (_sum < ci->_sum) return -1;
    if (_sum > ci->_sum) return 1;
    return 0;
  }
  if (col==1) {
    if (_cc < ci->_cc) return -1;
    if (_cc > ci->_cc) return 1;
    return 0;
  }
  return QListViewItem::compare(i, col, ascending);
}

