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
 * Items of stack dockable.
 */

#include "stackitem.h"

#include <QPixmap>

#include "globalguiconfig.h"
#include "listutils.h"
#include "stackselection.h"


// StackItem

StackItem::StackItem(StackSelection* ss, 
                     QTreeWidget* parent, TraceFunction* f)
  :QTreeWidgetItem(parent)
{
  _view = ss;
  _function = f;
  _call = 0;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  updateGroup();
  updateCost();

  setText(2, QString("-- "));
  setText(3, f->prettyName());
}

StackItem::StackItem(StackSelection* ss,
                     QTreeWidget* parent, TraceCall* call)
  :QTreeWidgetItem(parent)
{
  _view = ss;
  _call = call;
  _function = call->called();

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  updateGroup();
  updateCost();

  setText(3, _function->prettyName());
}


void StackItem::updateGroup()
{
  QColor c = GlobalGUIConfig::functionColor(_view->groupType(),
                                            _function);
  setIcon(3, colorPixmap(10, 10, c));
}

void StackItem::updateCost()
{
  if (!_call) return;

  setText(2, _call->prettyCallCount());

  EventType* ct = _view->eventType();
  _sum = _call->subCost(ct);
  double total = _call->called()->data()->subCost(ct);
  if (total == 0.0) {
    setText(0, "-");
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

  // if _eventType2 is 0, column1 is hidden, no change needed
  EventType* ct2 = _view->eventType2();
  if (!ct2) return;

  _sum = _call->subCost(ct2);
  total = _call->called()->data()->subCost(ct2);
  if (total == 0.0) {
    setText(1, "-");
    setIcon(1, QPixmap());
  }
  else {
    double sum  = 100.0 * _sum / total;

    if (GlobalConfig::showPercentage())
      setText(1, QString("%1")
	      .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
    else
      setText(1, _call->prettySubCost(ct2));
    
    setIcon(1, costPixmap(ct2, _call, total, false));
  }
}
