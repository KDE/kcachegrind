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

#include "callitem.h"

#include <QPixmap>

#include "globalguiconfig.h"
#include "listutils.h"
#include "callview.h"


// CallItem


CallItem::CallItem(CallView* view, QTreeWidget* parent, TraceCall* c)
    : QTreeWidgetItem(parent)
{
    setTextAlignment(0, Qt::AlignRight);
    setTextAlignment(1, Qt::AlignRight);
    setTextAlignment(2, Qt::AlignRight);

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
  QColor c = GlobalGUIConfig::functionColor(_view->groupType(), _shown);
  setIcon(3, colorPixmap(10, 10, c));
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
	    cStr = QObject::tr("(active)");
	else
	    cStr = _call->prettyCallCount();
    }
    setText(2, cStr);

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
	QString str = "-";

	setText(0, str);
        setIcon(0, QPixmap());
    }
    else {
	double sum  = 100.0 * _sum / total;

	if (GlobalConfig::showPercentage())
	    setText(0, QString("%1")
		    .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
	else
	    setText(0, _call->prettySubCost(ct));

        setIcon(0, costPixmap(ct, _call, total, false));
    }

    // Cost Type 2
    EventType* ct2 = _view->eventType2();
    if (ct2) {
      _sum2 = _call->subCost(ct2);
      double total = totalCost->subCost(ct2);

      if (total == 0.0) {
	QString str = "-";

	setText(1, str);
        setIcon(1, QPixmap());
      }
      else {
	double sum  = 100.0 * _sum2 / total;
	
	if (GlobalConfig::showPercentage())
	  setText(1, QString("%1")
		  .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
	else
	  setText(1, _call->prettySubCost(ct2));

        setIcon(1, costPixmap(ct2, _call, total, false));
      }
    }

    QPixmap p;
    if (sameCycle && !selectedIsCycle && !shownIsCycle) {

	QString icon = "edit-undo";
#if 0 // TODO
	KIconLoader* loader = KIconLoader::global();
	p= loader->loadIcon(icon, KIconLoader::Small, 0,
			    KIconLoader::DefaultState, QStringList(), 0, true);
#endif
    }
    setIcon(2, p);
}


bool CallItem::operator<(const QTreeWidgetItem& other) const
{
  int col = treeWidget()->sortColumn();
  const CallItem* ci1 = this;
  const CallItem* ci2 = (CallItem*) &other;

  if (col==0)
    return ci1->_sum < ci2->_sum;

  if (col==1)
    return ci1->_sum2 < ci2->_sum2;

  if (col==2)
   return ci1->_cc < ci2->_cc;

  return QTreeWidgetItem::operator <(other);
}

