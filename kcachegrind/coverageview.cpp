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
 * Coverage Views
 */

#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <klocale.h>

#include "configuration.h"
#include "coverageitem.h"
#include "coverage.h"
#include "coverageview.h"



//
// CoverageView
//


CoverageView::CoverageView(bool showCallers, TraceItemView* parentView,
			   QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
    _showCallers = showCallers;


    addColumn( i18n( "Incl." ) );
    if (_showCallers) {
	addColumn( i18n( "Distance" ) );
	addColumn( i18n( "Called" ) );
	addColumn( i18n( "Caller" ) );
    }
    else {
	addColumn( i18n( "Self" ) );
	addColumn( i18n( "Distance" ) );
	addColumn( i18n( "Calling" ) );
	addColumn( i18n( "Callee" ) );
	setColumnAlignment(3, Qt::AlignRight);
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

QString CoverageView::whatsThis() const
{
    return _showCallers ?
	i18n( "<b>List of all Callers</b>"
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

	i18n( "<b>List of all Callees</b>"
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

void CoverageView::context(QListViewItem* i, const QPoint & p, int c)
{
  QPopupMenu popup;

  TraceFunction* f = 0;
  if (i) {
      f = _showCallers ?
	  ((CallerCoverageItem*)i)->function() :
	  ((CalleeCoverageItem*)i)->function();
  }

  if (f) {
    QString name = f->name();
    if ((int)name.length()>Configuration::maxSymbolLength())
	name = name.left(Configuration::maxSymbolLength()) + "...";
    popup.insertItem(i18n("Go to '%1'").arg(name), 93);
    popup.insertSeparator();
  }

   if ((c == 0) || (!_showCallers && c == 1)) {
    addCostMenu(&popup, false);
    popup.insertSeparator();
  }
  addGoMenu(&popup); 

  int r = popup.exec(p);
  if (r == 93) activated(f);
}

void CoverageView::selectedSlot(QListViewItem * i)
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

void CoverageView::activatedSlot(QListViewItem * i)
{
  TraceFunction* f = 0;
  if (i) {
      f = _showCallers ?
	  ((CallerCoverageItem*)i)->function() :
	  ((CalleeCoverageItem*)i)->function();
  }

  if (f) activated(f);
}

TraceItem* CoverageView::canShow(TraceItem* i)
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

void CoverageView::doUpdate(int changeType)
{
    // Special case ?
    if (changeType == selectedItemChanged) {

	if (!_selectedItem) {
	    clearSelection();
	    return;
	}

	TraceFunction* f = 0;
	QListViewItem* i = QListView::selectedItem();
	if (i) {
	    f = _showCallers ?
		((CallerCoverageItem*)i)->function() :
		((CalleeCoverageItem*)i)->function();
	}
	if (f == _selectedItem) return;

	QListViewItem *item;
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
	QListViewItem *item;
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

    TraceItem::CostType t = _activeItem->type();
    TraceFunction* f = 0;
    if (t == TraceItem::Function) f = (TraceFunction*) _activeItem;
    if (t == TraceItem::FunctionCycle) f = (TraceFunction*) _activeItem;
    if (!f) return;

    TraceFunction* ff;
    TraceFunctionList l;

    _hc.clear(Configuration::maxListCount());
    SubCost realSum = f->inclusive()->subCost(_costType);

    if (_showCallers)
      l = Coverage::coverage(f, Coverage::Caller, _costType);
    else
      l = Coverage::coverage(f, Coverage::Called, _costType);

    for (ff=l.first();ff;ff=l.next()) {
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (c && (c->inclusive()>0.0))
	_hc.addCost(ff, SubCost(realSum * c->inclusive()));
    }

    for(int i=0;i<_hc.realCount();i++) {
      ff = (TraceFunction*) _hc[i];
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (_showCallers)
	new CallerCoverageItem(this, c, f, _costType, _groupType);
      else
	new CalleeCoverageItem(this, c, f, _costType, _groupType);
    }
    if (_hc.hasMore()) {
      // a placeholder for all the functions skipped ...
      ff = (TraceFunction*) _hc[_hc.maxSize()-1];
      Coverage* c = (Coverage*) ff->assoziation(Coverage::Rtti);
      if (_showCallers)
	new CallerCoverageItem(this, _hc.count() - _hc.maxSize(),
			       c, f, _costType, _groupType);
      else
	new CalleeCoverageItem(this, _hc.count() - _hc.maxSize(),
			       c, f, _costType, _groupType);
    }
}

#include "coverageview.moc"
