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
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#include "stackselection.h"

#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <Qt3Support/Q3ListView>

#include <kdebug.h>

#include "stackbrowser.h"
#include "stackitem.h"


StackSelection::StackSelection( QWidget* parent)
  : StackSelectionBase(parent)
{
  _data = 0;
  _browser = new StackBrowser();
  _item = 0;
  _function = 0;
  _eventType = 0;
  _eventType2 = 0;
  _groupType = TraceItem::Function;

  stackList->setSorting(-1);
  stackList->setAllColumnsShowFocus(true);
  stackList->setResizeMode(Q3ListView::LastColumn);
  stackList->setColumnAlignment(0, Qt::AlignRight);
  stackList->setColumnAlignment(1, Qt::AlignRight);
  stackList->setColumnAlignment(2, Qt::AlignRight);
  stackList->setColumnWidth(0, 50);
  // 2nd cost column hidden at first (_eventType2 == 0)
  stackList->setColumnWidth(1, 0);
  stackList->setColumnWidth(2, 50);

  connect(stackList, SIGNAL(selectionChanged(Q3ListViewItem*)),
          this, SLOT(stackSelected(Q3ListViewItem*)));
}

StackSelection::~StackSelection()
{
  delete _browser;
}

void StackSelection::setData(TraceData* data)
{
  if (_data == data) return;

  _data = data;

  stackList->clear();
   delete _browser;
  _browser = new StackBrowser();
  _function = 0;
}


void StackSelection::setFunction(TraceFunction* f)
{
  if (_function == f) return;
  _function = f;

  if (!_data || !_function) return;

  //kDebug() << "StackSelection::setFunction " << f->name();

  HistoryItem* item = _browser->current();
  if (!item || item->function() != f) {
    _browser->select(f);
    rebuildStackList();
  }
}


void StackSelection::rebuildStackList()
{
  HistoryItem* item = _browser->current();
  stackList->clear();
  stackList->setColumnWidth(0, 50);
  stackList->setColumnWidth(1, _eventType2 ? 50:0);
  stackList->setColumnWidth(2, 50);
  if (!item || !item->stack()) return;

  TraceFunction* top = item->stack()->top();
  if (!top) return;

  stackList->setColumnWidthMode(1, Q3ListView::Maximum);

  TraceCallList l = item->stack()->calls();
  TraceCall* call;
  for (call=l.last();call;call=l.prev())
    new StackItem(this, stackList, call);

  new StackItem(this, stackList, top);

  // select current function
  Q3ListViewItem* i = stackList->firstChild();
  for (;i;i=i->nextSibling())
    if (((StackItem*)i)->function() == item->function())
      break;

  if (i) {
    // this calls stackFunctionSelected()
    stackList->setCurrentItem(i);
    stackList->ensureItemVisible(i);
  }

  if (!_eventType2) {
    stackList->setColumnWidthMode(1, Q3ListView::Manual);
    stackList->setColumnWidth(1, 0);
  }
}

void StackSelection::stackSelected(Q3ListViewItem* i)
{
  if (!i) return;

  TraceFunction* f = ((StackItem*)i)->function();
  emit functionSelected(f);
}


void StackSelection::browserBack()
{
  if (_browser && _browser->canGoBack()) {
    _browser->goBack();
    rebuildStackList();
  }
}

void StackSelection::browserForward()
{
  if (_browser && _browser->canGoForward()) {
    _browser->goForward();
    rebuildStackList();
  }
}

void StackSelection::browserUp()
{
  if (_browser) {
    _browser->goUp();
    rebuildStackList();
  }
}

void StackSelection::browserDown()
{
  if (_browser) {
    _browser->goDown();
    rebuildStackList();
  }
}

void StackSelection::refresh()
{
  Q3ListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setEventType(TraceEventType* ct)
{
  if (ct == _eventType) return;
  _eventType = ct;

  stackList->setColumnWidth(0, 50);
  if (_eventType)
    stackList->setColumnText(0, _eventType->name());

  Q3ListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setEventType2(TraceEventType* ct)
{
  if (ct == _eventType2) return;
  _eventType2 = ct;

  stackList->setColumnWidth(1, 50);
  stackList->setColumnWidthMode(1, Q3ListView::Maximum);
  if (_eventType2)
    stackList->setColumnText(1, _eventType2->name());

  Q3ListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();

  if (!_eventType2) {
    stackList->setColumnWidthMode(1, Q3ListView::Manual);
    stackList->setColumnWidth(1, 0);
  }
}

void StackSelection::setGroupType(TraceItem::CostType gt)
{
  if (_groupType == gt) return;
  _groupType = gt;

  Q3ListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateGroup();
}

#include "stackselection.moc"
