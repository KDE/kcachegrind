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
 * Call Views
 */


#include "callview.h"

#include <Qt3Support/Q3PopupMenu>

#include "globalconfig.h"
#include "callitem.h"


//
// CallView
//


CallView::CallView(bool showCallers, TraceItemView* parentView,
		   QWidget* parent, const char* name)
  : Q3ListView(parent, name), TraceItemView(parentView)
{
    _showCallers = showCallers;

    addColumn( tr( "Cost" ) );
    addColumn( tr( "Cost 2" ) );
    if (_showCallers) {
	addColumn( tr( "Count" ) );
	addColumn( tr( "Caller" ) );
    }
    else {
	addColumn( tr( "Count" ) );
	addColumn( tr( "Callee" ) );
    }

    setSorting(0,false);
    setColumnAlignment(0, Qt::AlignRight);
    setColumnAlignment(1, Qt::AlignRight);
    setColumnAlignment(2, Qt::AlignRight);
    setAllColumnsShowFocus(true);
    setResizeMode(Q3ListView::LastColumn);
    setMinimumHeight(50);

    connect( this,
	     SIGNAL( selectionChanged(Q3ListViewItem*) ),
	     SLOT( selectedSlot(Q3ListViewItem*) ) );

    connect( this,
	     SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint &, int)),
	     SLOT(context(Q3ListViewItem*, const QPoint &, int)));

    connect(this,
	    SIGNAL(doubleClicked(Q3ListViewItem*)),
	    SLOT(activatedSlot(Q3ListViewItem*)));

    connect(this,
	    SIGNAL(returnPressed(Q3ListViewItem*)),
	    SLOT(activatedSlot(Q3ListViewItem*)));

    this->setWhatsThis( whatsThis() );
}

QString CallView::whatsThis() const
{
    return _showCallers ?
	tr( "<b>List of direct Callers</b>"
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
	tr( "<b>List of direct Callees</b>"
	      "<p>This list shows all functions called by the "
	      "current selected one directly, together with "
	      "a call count and the cost spent in this function "
	      "while being called from the selected function.</p>"
	      "<p>Selecting a function makes it the current selected "
	      "one of this information panel. "
	      "If there are two panels (Split mode), the "
	      "function of the other panel is changed instead.</p>");
}


void CallView::context(Q3ListViewItem* i, const QPoint & p, int col)
{
  Q3PopupMenu popup;

  // Menu entry:
  TraceCall* c = i ? ((CallItem*) i)->call() : 0;
  TraceFunction *f = 0, *cycle = 0;

  if (c) {
    QString name  = _showCallers ? c->callerName(true) : c->calledName(true);
    f = _showCallers ? c->caller(true) : c->called(true);
    cycle = f->cycle();

    popup.insertItem(tr("Go to '%1'")
		     .arg(GlobalConfig::shortenSymbol(name)), 93);

    if (cycle) {
	name = GlobalConfig::shortenSymbol(cycle->prettyName());
	popup.insertItem(tr("Go to '%1'").arg(name), 94);
    }

    popup.insertSeparator();
  }

  if ((col == 0) || (col == 1)) {
    addEventTypeMenu(&popup);
    popup.insertSeparator();
  }
  addGoMenu(&popup);

  int r = popup.exec(p);
  if (r == 93) activated(f);
  else if (r == 94) activated(cycle);
}

void CallView::selectedSlot(Q3ListViewItem * i)
{
  if (!i) return;
  TraceCall* c = ((CallItem*) i)->call();
  // Should we skip cycles here?
  TraceItem* f = _showCallers ? c->caller(false) : c->called(false);

  _selectedItem = f;
  selected(f);
}

void CallView::activatedSlot(Q3ListViewItem * i)
{
  if (!i) return;
  TraceCall* c = ((CallItem*) i)->call();
  // skip cycles: use the context menu to get to the cycle...
  TraceItem* f = _showCallers ? c->caller(true) : c->called(true);

  activated(f);
}

TraceItem* CallView::canShow(TraceItem* i)
{
  ProfileContext::Type t = i ? i->type() : ProfileContext::InvalidType;

  switch(t) {
  case ProfileContext::Function:
  case ProfileContext::FunctionCycle:
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

	CallItem* ci = (CallItem*) Q3ListView::selectedItem();
	TraceCall* c;
	TraceItem* ti;
	if (ci) {
	    c = ci->call();
	    ti = _showCallers ? c->caller() : c->called();
	    if (ti == _selectedItem) return;
	}

	Q3ListViewItem *item;
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
	Q3ListViewItem *item;
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
    setColumnWidth(1, _eventType2 ? 50:0);
    setColumnWidth(2, 50);
    if (_eventType)
      setColumnText(0, _eventType->name());
    if (_eventType2)
      setColumnText(1, _eventType2->name());

    if (!_data || !_activeItem) return;

    TraceFunction* f = activeFunction();
    if (!f) return;

    TraceCall* call;
    // In the call lists, we skip cycles to show the real call relations
    TraceCallList l = _showCallers ? f->callers(true) : f->callings(true);

    // Allow resizing of column 1
    setColumnWidthMode(1, Q3ListView::Maximum);

    for (call=l.first();call;call=l.next())
	if (call->subCost(_eventType)>0)
	    new CallItem(this, this, call);

    if (!_eventType2) {
      setColumnWidthMode(1, Q3ListView::Manual);
      setColumnWidth(1, 0);
    }
}

#include "callview.moc"
