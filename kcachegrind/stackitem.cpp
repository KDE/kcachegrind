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
 * Items of stack dockable.
 */

#include <qpixmap.h>
#include <klocale.h>

#include "configuration.h"
#include "listutils.h"
#include "stackitem.h"
#include "stackselection.h"

// StackItem

StackItem::StackItem(StackSelection* ss, 
		     QListView* parent, TraceFunction* f)
  :QListViewItem(parent)
{
  _view = ss;
  _function = f;
  _call = 0;

  updateGroup();
  updateCost();

  setText(2, QString("-- "));
  setText(3, f->prettyName());
}

StackItem::StackItem(StackSelection* ss,
		     QListView* parent, TraceCall* call)
  :QListViewItem(parent)
{
  _view = ss;
  _call = call;
  _function = call->called();

  updateGroup();
  updateCost();

  setText(3, _function->prettyName());
}


void StackItem::updateGroup()
{
  QColor c = Configuration::functionColor(_view->groupType(),
					  _function);
  setPixmap(3, colorPixmap(10, 10, c));
}

void StackItem::updateCost()
{
  if (!_call) return;

  setText(2, _call->prettyCallCount());

  TraceCostType* ct = _view->costType();
  _sum = _call->subCost(ct);
  double total = _call->called()->data()->subCost(ct);
  if (total == 0.0) {
    setText(0, "-");
    setPixmap(0, QPixmap());
  }
  else {
    double sum  = 100.0 * _sum / total;

    if (Configuration::showPercentage())
      setText(0, QString("%1")
	      .arg(sum, 0, 'f', Configuration::percentPrecision()));
    else
      setText(0, _call->prettySubCost(ct));
    
    setPixmap(0, costPixmap(ct, _call, total, false));
  }

  // if _costType2 is 0, column1 is hidden, no change needed
  TraceCostType* ct2 = _view->costType2();
  if (!ct2) return;

  _sum = _call->subCost(ct2);
  total = _call->called()->data()->subCost(ct2);
  if (total == 0.0) {
    setText(1, "-");
    setPixmap(1, QPixmap());
  }
  else {
    double sum  = 100.0 * _sum / total;

    if (Configuration::showPercentage())
      setText(1, QString("%1")
	      .arg(sum, 0, 'f', Configuration::percentPrecision()));
    else
      setText(1, _call->prettySubCost(ct2));
    
    setPixmap(1, costPixmap(ct2, _call, total, false));
  }
}
