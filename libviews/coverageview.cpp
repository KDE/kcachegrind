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
 * Coverage Views
 */


#include "coverageview.h"

#include <QAction>
#include <QMenu>

#include "globalconfig.h"
#include "coverageitem.h"
#include "coverage.h"


//
// CoverageView
//


CoverageView::CoverageView(bool showCallers, TraceItemView* parentView,
			   QWidget* parent, const char* name)
  : Q3ListView(parent, name), TraceItemView(parentView)
{
    _showCallers = showCallers;


    addColumn( tr( "Incl." ) );
    if (_showCallers) {
	addColumn( tr( "Distance" ) );
	addColumn( tr( "Called" ) );
	addColumn( tr( "Caller" ) );
    }
    else {
	addColumn( tr( "Self" ) );
	addColumn( tr( "Distance" ) );
	addColumn( tr( "Calling" ) );
	addColumn( tr( "Callee" ) );
	setColumnAlignment(3, Qt::AlignRight);
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

QString CoverageView::whatsThis() const
{
    return _showCallers ?
	tr( "<b>List of all Callers</b>"
	      "<p>This list shows all functions calling the "
	      "current selected one, either directly or with "
	      "several functions in-between on the stack; the "
	      "number of functions in-between plus one "
	      "is called the <em>Distance</em> (e.g. "
	      "for function A,B,C there exists a call from "
	      "A to C when A calls B and B calls C, i.e. "
	      "A => B => C. The distance here is 2).</p>"

	      "<p>Absolute cost shown is the cost spent in the "
	      "selected function while a listed function is active; "
	      "relative cost is the percentage of all cost spent in "
	      "the selected function while the listed one is "
	      "active. The cost graphic shows logarithmic "
	      "percentage with a different color for each "
	      "distance.</p>"

	      "<p>As there can be many calls from the same function, "
	      "the distance column sometimes shows "
	      "the range of distances for all "
	      "calls happening; then, in parentheses, there is the "
	      "medium distance, i.e. the distance where most of the "
	      "call costs happened.</p>"

	      "<p>Selecting a function makes it the current selected "
	      "one of this information panel. "
	      "If there are two panels (Split mode), the "
	      "function of the other panel is changed instead.</p>") :

	tr( "<b>List of all Callees</b>"
	      "<p>This list shows all functions called by the "
	      "current selected one, either directly or with "
	      "several function in-between on the stack; the "
	      "number of function in-between plus one "
	      "is called the <em>Distance</em> (e.g. "
	      "for function A,B,C there exists a call from "
	      "A to C when A calls B and B calls C, i.e. "
	      "A => B => C. The distance here is 2).</p>"

	      "<p>Absolute cost shown is the cost spent in the "
	      "listed function while the selected is active; "
	      "relative cost is the percentage of all cost spent in "
	      "the listed function while the selected one is active. "
	      "The cost graphic always shows logarithmic "
	      "percentage with a different color for each "
	      "distance.</p>"

	      "<p>As there can be many calls to the same function, "
	      "the distance column sometimes shows "
	      "the range of distances for all "
	      "calls happening; then, in parentheses, there is the "
	      "medium distance, i.e. the distance where most of the "
	      "call costs happened.</p>"

	      "<p>Selecting a function makes it the current selected "
	      "one of this information panel. "
	      "If there are two panels (Split mode), the "
	      "function of the other panel is changed instead.</p>");
}

void CoverageView::context(Q3ListViewItem* i, const QPoint & p, int c)
{
  QMenu popup;

  TraceFunction* f = 0;
  if (i) {
      f = _showCallers ?
	  ((CallerCoverageItem*)i)->function() :
	  ((CalleeCoverageItem*)i)->function();
  }

  QAction* activateFunctionAction = 0;
  if (f) {
      QString menuText = tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(f->prettyName()));
      activateFunctionAction = popup.addAction(menuText);
      popup.addSeparator();
  }

   if ((c == 0) || (!_showCallers && c == 1)) {
    addEventTypeMenu(&popup, false);
    popup.addSeparator();
  }
  addGoMenu(&popup); 

  QAction* a = popup.exec(p);
  if (a == activateFunctionAction)
      activated(f);
}

void CoverageView::selectedSlot(Q3ListViewItem * i)
{
  TraceFunction* f = 0;
  if (i) {
      f = _showCallers ?
	  ((CallerCoverageItem*)i)->function() :
	  ((CalleeCoverageItem*)i)->function();
  }

  if (f) {
      _selectedItem = f;
      selected(f);
  }
}

void CoverageView::activatedSlot(Q3ListViewItem * i)
{
  TraceFunction* f = 0;
  if (i) {
      f = _showCallers ?
	  ((CallerCoverageItem*)i)->function() :
	  ((CalleeCoverageItem*)i)->function();
  }

  if (f) activated(f);
}

CostItem* CoverageView::canShow(CostItem* i)
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

void CoverageView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) {

	if (!_selectedItem) {
	    clearSelection();
	    return;
	}

	TraceFunction* f = 0;
	Q3ListViewItem* i = Q3ListView::selectedItem();
	if (i) {
	    f = _showCallers ?
		((CallerCoverageItem*)i)->function() :
		((CalleeCoverageItem*)i)->function();
	}
	if (f == _selectedItem) return;

	Q3ListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling()) {
	    f = _showCallers ?
		((CallerCoverageItem*)item)->function() :
		((CalleeCoverageItem*)item)->function();
	    if (f == _selectedItem) {
		ensureItemVisible(item);
		setCurrentItem(item);
		break;
	    }
	}
	return;
    }

    if (changeType == groupTypeChanged) {
	Q3ListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling()) {
	    if (_showCallers)
		((CallerCoverageItem*)item)->setGroupType(_groupType);
	    else
		((CalleeCoverageItem*)item)->setGroupType(_groupType);
	}
	return;
    }

    refresh();
}

void CoverageView::refresh()
{
    clear();
    setColumnWidth(0, 50);
    if (!_showCallers)
	setColumnWidth(1, 50);

    if (!_data || !_activeItem) return;

    ProfileContext::Type t = _activeItem->type();
    TraceFunction* f = 0;
    if (t == ProfileContext::Function) f = (TraceFunction*) _activeItem;
    if (t == ProfileContext::FunctionCycle) f = (TraceFunction*) _activeItem;
    if (!f) return;

    TraceFunction* ff;
    TraceFunctionList l;

    _hc.clear(GlobalConfig::maxListCount());
    SubCost realSum = f->inclusive()->subCost(_eventType);

    if (_showCallers)
      l = Coverage::coverage(f, Coverage::Caller, _eventType);
    else
      l = Coverage::coverage(f, Coverage::Called, _eventType);

    for (ff=l.first();ff;ff=l.next()) {
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (c && (c->inclusive()>0.0))
	_hc.addCost(ff, SubCost(realSum * c->inclusive()));
    }

    for(int i=0;i<_hc.realCount();i++) {
      ff = (TraceFunction*) _hc[i];
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (_showCallers)
	new CallerCoverageItem(this, c, f, _eventType, _groupType);
      else
	new CalleeCoverageItem(this, c, f, _eventType, _groupType);
    }
    if (_hc.hasMore()) {
      // a placeholder for all the functions skipped ...
      ff = (TraceFunction*) _hc[_hc.maxSize()-1];
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (_showCallers)
	new CallerCoverageItem(this, _hc.count() - _hc.maxSize(),
			       c, f, _eventType, _groupType);
      else
	new CalleeCoverageItem(this, _hc.count() - _hc.maxSize(),
			       c, f, _eventType, _groupType);
    }
}

#include "coverageview.moc"
