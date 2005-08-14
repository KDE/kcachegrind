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
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#include <qtimer.h>
#include <qlistview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlineedit.h>

#include <kdebug.h>

#include "stackbrowser.h"
#include "stackselection.h"
#include "stackitem.h"

StackSelection::StackSelection( QWidget* parent, const char* name)
  : StackSelectionBase(parent, name)
{
  _data = 0;
  _browser = new StackBrowser();
  _item = 0;
  _function = 0;
  _costType = 0;
  _costType2 = 0;
  _groupType = TraceItem::Function;

  stackList->setSorting(-1);
  stackList->setAllColumnsShowFocus(true);
  stackList->setResizeMode(QListView::LastColumn);
  stackList->setColumnAlignment(0, Qt::AlignRight);
  stackList->setColumnAlignment(1, Qt::AlignRight);
  stackList->setColumnAlignment(2, Qt::AlignRight);
  stackList->setColumnWidth(0, 50);
  // 2nd cost column hidden at first (_costType2 == 0)
  stackList->setColumnWidth(1, 0);
  stackList->setColumnWidth(2, 50);

  connect(stackList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(stackSelected(QListViewItem*)));
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

  //kdDebug() << "StackSelection::setFunction " << f->name() << endl;

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
  stackList->setColumnWidth(1, _costType2 ? 50:0);
  stackList->setColumnWidth(2, 50);
  if (!item || !item->stack()) return;

  TraceFunction* top = item->stack()->top();
  if (!top) return;

  stackList->setColumnWidthMode(1, QListView::Maximum);

  TraceCallList l = item->stack()->calls();
  TraceCall* call;
  for (call=l.last();call;call=l.prev())
    new StackItem(this, stackList, call);

  new StackItem(this, stackList, top);

  // select current function
  QListViewItem* i = stackList->firstChild();
  for (;i;i=i->nextSibling())
    if (((StackItem*)i)->function() == item->function())
      break;

  if (i) {
    // this calls stackFunctionSelected()
    stackList->setCurrentItem(i);
    stackList->ensureItemVisible(i);
  }

  if (!_costType2) {
    stackList->setColumnWidthMode(1, QListView::Manual);
    stackList->setColumnWidth(1, 0);
  }
}

void StackSelection::stackSelected(QListViewItem* i)
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
  QListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setCostType(TraceCostType* ct)
{
  if (ct == _costType) return;
  _costType = ct;

  stackList->setColumnWidth(0, 50);
  if (_costType)
    stackList->setColumnText(0, _costType->name());

  QListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setCostType2(TraceCostType* ct)
{
  if (ct == _costType2) return;
  _costType2 = ct;

  stackList->setColumnWidth(1, 50);
  stackList->setColumnWidthMode(1, QListView::Maximum);
  if (_costType2)
    stackList->setColumnText(1, _costType2->name());

  QListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();

  if (!_costType2) {
    stackList->setColumnWidthMode(1, QListView::Manual);
    stackList->setColumnWidth(1, 0);
  }
}

void StackSelection::setGroupType(TraceItem::CostType gt)
{
  if (_groupType == gt) return;
  _groupType = gt;

  QListViewItem* item  = stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateGroup();
}

#include "stackselection.moc"
