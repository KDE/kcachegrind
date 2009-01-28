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
#include <QVBoxLayout>
#include <Qt3Support/Q3ListView>

#include "stackbrowser.h"
#include "stackitem.h"


StackSelection::StackSelection(QWidget* parent)
  : QWidget(parent)
{
  _data = 0;
  _browser = new StackBrowser();
  _item = 0;
  _function = 0;
  _eventType = 0;
  _eventType2 = 0;
  _groupType = ProfileContext::Function;

  setWindowTitle(tr("Stack Selection"));

  QVBoxLayout* vboxLayout = new QVBoxLayout(this);
  vboxLayout->setSpacing(6);
  vboxLayout->setMargin(3);

  _stackList = new Q3ListView(this);
  _stackList->addColumn(tr("Cost"));
  _stackList->addColumn(tr("Cost2"));
  _stackList->addColumn(tr("Calls"));
  _stackList->addColumn(tr("Function"));
  //_stackList->header()->setClickEnabled(true);
  vboxLayout->addWidget(_stackList);

  _stackList->setSorting(-1);
  _stackList->setAllColumnsShowFocus(true);
  _stackList->setResizeMode(Q3ListView::LastColumn);
  _stackList->setColumnAlignment(0, Qt::AlignRight);
  _stackList->setColumnAlignment(1, Qt::AlignRight);
  _stackList->setColumnAlignment(2, Qt::AlignRight);
  _stackList->setColumnWidth(0, 50);
  // 2nd cost column hidden at first (_eventType2 == 0)
  _stackList->setColumnWidth(1, 0);
  _stackList->setColumnWidth(2, 50);

  connect(_stackList, SIGNAL(selectionChanged(Q3ListViewItem*)),
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

  _stackList->clear();
   delete _browser;
  _browser = new StackBrowser();
  _function = 0;
}


void StackSelection::setFunction(TraceFunction* f)
{
  if (_function == f) return;
  _function = f;

  if (!_data || !_function) return;

  //qDebug() << "StackSelection::setFunction " << f->name();

  HistoryItem* item = _browser->current();
  if (!item || item->function() != f) {
    _browser->select(f);
    rebuildStackList();
  }
}


void StackSelection::rebuildStackList()
{
  HistoryItem* item = _browser->current();
  _stackList->clear();
  _stackList->setColumnWidth(0, 50);
  _stackList->setColumnWidth(1, _eventType2 ? 50:0);
  _stackList->setColumnWidth(2, 50);
  if (!item || !item->stack()) return;

  TraceFunction* top = item->stack()->top();
  if (!top) return;

  _stackList->setColumnWidthMode(1, Q3ListView::Maximum);

  TraceCallList l = item->stack()->calls();
  TraceCall* call;
  for (call=l.last();call;call=l.prev())
    new StackItem(this, _stackList, call);

  new StackItem(this, _stackList, top);

  // select current function
  Q3ListViewItem* i = _stackList->firstChild();
  for (;i;i=i->nextSibling())
    if (((StackItem*)i)->function() == item->function())
      break;

  if (i) {
    // this calls stackFunctionSelected()
    _stackList->setCurrentItem(i);
    _stackList->ensureItemVisible(i);
  }

  if (!_eventType2) {
    _stackList->setColumnWidthMode(1, Q3ListView::Manual);
    _stackList->setColumnWidth(1, 0);
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
  Q3ListViewItem* item  = _stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setEventType(TraceEventType* ct)
{
  if (ct == _eventType) return;
  _eventType = ct;

  _stackList->setColumnWidth(0, 50);
  if (_eventType)
    _stackList->setColumnText(0, _eventType->name());

  Q3ListViewItem* item  = _stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();
}

void StackSelection::setEventType2(TraceEventType* ct)
{
  if (ct == _eventType2) return;
  _eventType2 = ct;

  _stackList->setColumnWidth(1, 50);
  _stackList->setColumnWidthMode(1, Q3ListView::Maximum);
  if (_eventType2)
    _stackList->setColumnText(1, _eventType2->name());

  Q3ListViewItem* item  = _stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateCost();

  if (!_eventType2) {
    _stackList->setColumnWidthMode(1, Q3ListView::Manual);
    _stackList->setColumnWidth(1, 0);
  }
}

void StackSelection::setGroupType(ProfileContext::Type gt)
{
  if (_groupType == gt) return;
  _groupType = gt;

  Q3ListViewItem* item  = _stackList->firstChild();
  for(;item;item = item->nextSibling())
    ((StackItem*)item)->updateGroup();
}

#include "stackselection.moc"
