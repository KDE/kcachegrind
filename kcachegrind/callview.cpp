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
 * Call Views
 */

#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <klocale.h>

#include "configuration.h"
#include "callitem.h"
#include "callview.h"



//
// CallView
//


CallView::CallView(bool showCallers, TraceItemView* parentView,
		   QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
    _showCallers = showCallers;

    addColumn( i18n( "Cost" ) );
    addColumn( i18n( "Cost 2" ) );
    if (_showCallers) {
	addColumn( i18n( "Count" ) );
	addColumn( i18n( "Caller" ) );
    }
    else {
	addColumn( i18n( "Count" ) );
	addColumn( i18n( "Callee" ) );
    }

    setSorting(0,false);
    setColumnAlignment(0, Qt::AlignRight);
    setColumnAlignment(1, Qt::AlignRight);
    setColumnAlignment(2, Qt::AlignRight);
    setAllColumnsShowFocus(true);
    setResizeMode(QListView::LastColumn);
    setMinimumHeight(50);

    connect( this,
	     SIGNAL( selectionChanged(QListViewItem*) ),
	     SLOT( selectedSlot(QListViewItem*) ) );

    connect( this,
	     SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
	     SLOT(context(QListViewItem*, const QPoint &, int)));

    connect(this,
	    SIGNAL(doubleClicked(QListViewItem*)),
	    SLOT(activatedSlot(QListViewItem*)));

    connect(this,
	    SIGNAL(returnPressed(QListViewItem*)),
	    SLOT(activatedSlot(QListViewItem*)));

    QWhatsThis::add( this, whatsThis() );
}

QString CallView::whatsThis() const
{
    return _showCallers ?
	i18n( "<b>List of direct Callers</b>"
	      "<p>This list shows all functions calling the "
	      "current selected one directly, together with "
	      "a call count and the cost spent in the current "
	      "selected function while being called from the "
	      "function from the list.</p>"
	      "<p>An icon instead of an inclusive cost specifies "
	      "that this is a call inside of a recursive cycle. "
	      "An inclusive cost makes no sense here.</p>"
	      "<p>Selecting a function makes it the current selected "
	      "one of this information panel. "
	      "If there are two panels (Split mode), the "
	      "function of the other panel is changed instead.</p>") :
	i18n( "<b>List of direct Callees</b>"
	      "<p>This list shows all functions called by the "
	      "current selected one directly, together with "
	      "a call count and the cost spent in this function "
	      "while being called from the selected function.</p>"
	      "<p>Selecting a function makes it the current selected "
	      "one of this information panel. "
	      "If there are two panels (Split mode), the "
	      "function of the other panel is changed instead.</p>");
}


void CallView::context(QListViewItem* i, const QPoint & p, int col)
{
  QPopupMenu popup;

  // Menu entry:
  TraceCall* c = i ? ((CallItem*) i)->call() : 0;
  TraceFunction *f = 0, *cycle = 0;

  if (c) {
    QString name  = _showCallers ? c->callerName(true) : c->calledName(true);
    f = _showCallers ? c->caller(true) : c->called(true);
    cycle = f->cycle();

    popup.insertItem(i18n("Go to '%1'")
		     .arg(Configuration::shortenSymbol(name)), 93);

    if (cycle) {
	name = Configuration::shortenSymbol(cycle->prettyName());
	popup.insertItem(i18n("Go to '%1'").arg(name), 94);
    }

    popup.insertSeparator();
  }

  if ((col == 0) || (col == 1)) {
    addCostMenu(&popup);
    popup.insertSeparator();
  }
  addGoMenu(&popup);

  int r = popup.exec(p);
  if (r == 93) activated(f);
  else if (r == 94) activated(cycle);
}

void CallView::selectedSlot(QListViewItem * i)
{
  if (!i) return;
  TraceCall* c = ((CallItem*) i)->call();
  // Should we skip cycles here?
  TraceItem* f = _showCallers ? c->caller(false) : c->called(false);

  _selectedItem = f;
  selected(f);
}

void CallView::activatedSlot(QListViewItem * i)
{
  if (!i) return;
  TraceCall* c = ((CallItem*) i)->call();
  // skip cycles: use the context menu to get to the cycle...
  TraceItem* f = _showCallers ? c->caller(true) : c->called(true);

  activated(f);
}

TraceItem* CallView::canShow(TraceItem* i)
{
  TraceItem::CostType t = i ? i->type() : TraceItem::NoCostType;

  switch(t) {
  case TraceItem::Function:
  case TraceItem::FunctionCycle:
      return i;
  default:
      break;
  }
  return 0;
}

void CallView::doUpdate(int changeType)
{
    // Special case ?
    if (changeType == selectedItemChanged) {

	if (!_selectedItem) {
	    clearSelection();
	    return;
	}

	CallItem* ci = (CallItem*) QListView::selectedItem();
	TraceCall* c;
	TraceItem* ti;
	if (ci) {
	    c = ci->call();
	    ti = _showCallers ? c->caller() : c->called();
	    if (ti == _selectedItem) return;
	}

	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling()) {
	    c = ((CallItem*) item)->call();
	    ti = _showCallers ? c->caller() : c->called();
	    if (ti == _selectedItem) {
		ensureItemVisible(item);
		setSelected(item,  true);
		break;
	    }
	}
        if (!item && ci) clearSelection();
	return;
    }
    
    if (changeType == groupTypeChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((CallItem*)item)->updateGroup();
	return;
    }

    refresh();
}

void CallView::refresh()
{
    clear();
    setColumnWidth(0, 50);
    setColumnWidth(1, _costType2 ? 50:0);
    setColumnWidth(2, 50);
    if (_costType)
      setColumnText(0, _costType->name());
    if (_costType2)
      setColumnText(1, _costType2->name());

    if (!_data || !_activeItem) return;

    TraceFunction* f = activeFunction();
    if (!f) return;

    TraceCall* call;
    // In the call lists, we skip cycles to show the real call relations
    TraceCallList l = _showCallers ? f->callers(true) : f->callings(true);

    // Allow resizing of column 1
    setColumnWidthMode(1, QListView::Maximum);

    for (call=l.first();call;call=l.next())
	if (call->subCost(_costType)>0)
	    new CallItem(this, this, call);

    if (!_costType2) {
      setColumnWidthMode(1, QListView::Manual);
      setColumnWidth(1, 0);
    }
}

#include "callview.moc"
