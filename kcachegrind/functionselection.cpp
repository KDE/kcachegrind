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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * For function selection, to be put into a QDockWindow
 */

#include <qtimer.h>
#include <qlistview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qregexp.h>
#include <qpopupmenu.h>

#include <klocale.h>

#include "traceitemview.h"
#include "stackbrowser.h"
#include "functionselection.h"
#include "partgraph.h"
#include "functionitem.h"
#include "costlistitem.h"
#include "configuration.h"

FunctionSelection::FunctionSelection( TopLevel* top,
				      QWidget* parent, const char* name)
    : FunctionSelectionBase(parent, name), TraceItemView(0, top)
{
  _group = 0;
  _inSetCostItem = false;
  _inSetFunction = false;

  // we start with desending cost sorting
  functionList->setSorting(0,false);
  functionList->setColumnAlignment(0, Qt::AlignRight);
  functionList->setColumnAlignment(1, Qt::AlignRight);
  functionList->setColumnAlignment(2, Qt::AlignRight);
  functionList->setAllColumnsShowFocus(true);
  // functionList->setShowSortIndicator(true);
  // we can have very long function and location names
  functionList->setColumnWidthMode(3, QListView::Manual);
  functionList->setColumnWidth(3, 200);
  functionList->setColumnWidthMode(4, QListView::Manual);
  functionList->setColumnWidth(4, 200);

  groupList->setSorting(0,false);
  groupList->setColumnAlignment(0, Qt::AlignRight);
  groupList->setAllColumnsShowFocus(true);
  // groupList->setShowSortIndicator(true);
  groupList->setResizeMode(QListView::LastColumn);

#if 1
  // single click activation
  connect(functionList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(functionActivated(QListViewItem*)));
#else
  connect(functionList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(functionSelected(QListViewItem*)));
  connect(functionList, SIGNAL(doubleClicked(QListViewItem*)),
	  this, SLOT(functionActivated(QListViewItem*)));
  connect(functionList, SIGNAL(returnPressed(QListViewItem*)),
	  this, SLOT(functionActivated(QListViewItem*)));
  connect(functionList,
	  SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
          this, SLOT(functionContext(QListViewItem*, const QPoint &, int)));
#endif

  connect(groupList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(groupSelected(QListViewItem*)));
  connect(groupList, SIGNAL(doubleClicked(QListViewItem*)),
	  this, SLOT(groupDoubleClicked(QListViewItem*)));
  connect(groupList, SIGNAL(returnPressed(QListViewItem*)),
	  this, SLOT(groupDoubleClicked(QListViewItem*)));
  connect(groupList,
	  SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
          this, SLOT(groupContext(QListViewItem*, const QPoint &, int)));

  // start hidden
  groupList->hide();
}

FunctionSelection::~FunctionSelection()
{
}

void FunctionSelection::functionContext(QListViewItem* i,
					const QPoint & p, int)
{
  QPopupMenu popup;
  TraceFunction* f = 0;

  if (i) {
      f = ((FunctionItem*) i)->function();
      popup.insertItem(i18n("Go to %1").arg(f->prettyName()), 93);
      popup.insertSeparator();
  }
  popup.insertItem(i18n("Go Back"), 90);
  popup.insertItem(i18n("Go Forward"), 91);
  popup.insertItem(i18n("Go Up"), 92);

  int r = popup.exec(p);
  if      (r == 90) activated(Back);
  else if (r == 91) activated(Forward);
  else if (r == 92) activated(Up);
  else if (r == 93) activated(f);
}

void FunctionSelection::groupContext(QListViewItem* /*i*/,
				     const QPoint & p, int)
{
  QPopupMenu popup;

#if 0
  TraceCostItem* g = 0;
  if (i) {
      g = ((CostListItem*) i)->costItem();
      if (!g) {
	popup.insertItem(i18n("Show All Items"), 93);
	popup.insertSeparator();
      }
  }
#endif
  popup.insertItem(i18n("Go Back"), 90);
  popup.insertItem(i18n("Go Forward"), 91);
  popup.insertItem(i18n("Go Up"), 92);

  int r = popup.exec(p);
  if      (r == 90) activated(Back);
  else if (r == 91) activated(Forward);
  else if (r == 92) activated(Up);
}

TraceItem* FunctionSelection::canShow(TraceItem* i)
{
    TraceItem::CostType t = i ? i->type() : TraceItem::NoCostType;

    switch(t) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
    case TraceItem::Object:
    case TraceItem::File:
    case TraceItem::Class:
	break;

    case TraceItem::Instr:
	i = ((TraceInstr*)i)->function();
	break;

    case TraceItem::Line:
	i = ((TraceLine*)i)->functionSource()->function();
	break;

    default:
	i = 0;
	break;
  }
  return i;
}


void FunctionSelection::doUpdate(int changeType)
{
    // Special case ?
    if (changeType == selectedItemChanged) return;

    if (changeType == activeItemChanged) {
	if (_activeItem ==0) {
	    functionList->clearSelection();
	    return;
	}
	switch(_activeItem->type()) {
	case TraceItem::Object:
	case TraceItem::File:
	case TraceItem::Class:
	    setGroup((TraceCostItem*)_activeItem);
	    return;
	default: break;
	}

	// active item is a function
	TraceFunction* f = (TraceFunction*) _activeItem;

	// select cost item group of function
	switch(_groupType) {
	case TraceItem::Object: setGroup(f->object()); break;
	case TraceItem::Class:  setGroup(f->cls()); break;
	case TraceItem::File:   setGroup(f->file());  break;
	case TraceItem::FunctionCycle: setGroup(f->cycle());  break;
	default:
	    break;
	}

	QListViewItem* item  = functionList->firstChild();
	for (;item;item = item->nextSibling())
	    if (((FunctionItem*)item)->function() == f)
		break;

	if (!item)
	    item = new FunctionItem(functionList, f, _costType, _groupType);

	functionList->ensureItemVisible(item);
	// prohibit signalling of a function selection
	_inSetFunction = true;
	functionList->setSelected(item, true);
	_inSetFunction = false;

	return;
    }

    if (changeType & groupTypeChanged) {
	if (_activeItem && (_activeItem->type() == TraceItem::Function)) {
	    TraceFunction* f = (TraceFunction*) _activeItem;

	    // select cost item group of function
	    switch(_groupType) {
	    case TraceItem::Object: _group = f->object(); break;
	    case TraceItem::Class:  _group = f->cls(); break;
	    case TraceItem::File:   _group = f->file();  break;
	    case TraceItem::FunctionCycle: _group = f->cycle();  break;
	    default:
		_group = 0;
		break;
	    }
	}

	if (_groupType == TraceItem::Function)
	    groupList->hide();
	else
	    groupList->show();
    }

    refresh();
}


/*
 * This set/selects a group of the set available within the
 * current group type
 */
void FunctionSelection::setGroup(TraceCostItem* g)
{
  if (!g) return;
  if (g->type() != _groupType) return;
  if (g == _group) return;
  _group = g;

  QListViewItem* item  = groupList->firstChild();
  for (;item;item = item->nextSibling())
    if (((CostListItem*)item)->costItem() == g)
      break;

  if (item) {
    groupList->ensureItemVisible(item);
    // prohibit signalling of a group selection
    _inSetCostItem = true;
    groupList->setSelected(item, true);
    _inSetCostItem = false;
  }
  else
    groupList->clearSelection();
}


void FunctionSelection::refresh()
{
    groupList->setUpdatesEnabled(false);
    groupList->clear();

    // make cost columns as small as possible:
    // the new functions make them as wide as needed
    groupList->setColumnWidth(0, 50);

    groupList->setColumnText(1, TraceItem::i18nTypeName(_groupType));

    double total = _data ? ((double)_data->subCost(_costType)) : 0.0;

    if (!_data || (total==0.0) || _data->parts().count()==0) {
	functionList->clear();
	groupList->setUpdatesEnabled(true);
	groupList->repaint();

	// this clears all other lists
	functionList->setSelected(functionList->firstChild(), true);
	return;
    }

  /*
  qDebug("FunctionSelection::fillLists (%s)",
         _data->command().ascii());
  */

  TraceObjectMap::Iterator oit;
  TraceClassMap::Iterator cit;
  TraceFileMap::Iterator fit;
  QListViewItem *i = 0, *item = 0, *fitem = 0;

  // Fill up group list.
  // Always show group of current function, even if cost below low limit.
  //

  _hc.clear(Configuration::maxListCount());

  TraceCostItem *group;

  // update group from _activeItem if possible
  if (_activeItem && (_activeItem->type() == _groupType))
    _group = (TraceCostItem*) _activeItem;
  
  switch(_groupType) {
  case TraceItem::Object:
    
    for ( oit = _data->objectMap().begin();
          oit != _data->objectMap().end(); ++oit )
      _hc.addCost(&(*oit), (*oit).subCost(_costType));
    break;
    
  case TraceItem::Class:
    
    for ( cit = _data->classMap().begin();
          cit != _data->classMap().end(); ++cit )
      _hc.addCost(&(*cit), (*cit).subCost(_costType));
    break;
    
  case TraceItem::File:
    
    for ( fit = _data->fileMap().begin();
          fit != _data->fileMap().end(); ++fit )
      _hc.addCost(&(*fit), (*fit).subCost(_costType));
    break;
    
  case TraceItem::FunctionCycle:
    {    
      // add all cycles
      TraceFunctionCycleList l =  _data->functionCycles();
      for (group=l.first();group;group=l.next())
	_hc.addCost(group, group->subCost(_costType));
    }

  break;
  
  default:
    {
      QListViewItem* oldItem = functionList->selectedItem();
      TraceFunction* oldFunction = 0;
      int oldPos = 0;
      if (oldItem) {
	oldFunction = ((FunctionItem*)oldItem)->function();
	oldPos = oldItem->itemPos();
	oldPos -= functionList->contentsY();
	if (oldPos < 0 || oldPos > functionList->height())
	  oldFunction = 0;
      }
      
      functionList->setUpdatesEnabled(false);
      functionList->clear();
      functionList->setColumnWidth(0, 50);
      functionList->setColumnWidth(1, 50);
      functionList->setColumnWidth(2, 50);

      if (0) qDebug("Function %s at %d, Item %p",
		    oldFunction ? oldFunction->name().ascii() : "-",
		    oldPos, (void*)oldItem);
      
      TraceFunctionMap::Iterator it;
      TraceFunction *f;
      i = 0;
      fitem = 0;
      for ( it = _data->functionMap().begin();
	    it != _data->functionMap().end(); ++it )
	_hc.addCost(&(*it), (*it).cumulative()->subCost(_costType));
      
      TraceFunctionCycleList l =  _data->functionCycles();
      for (f=l.first();f;f=l.next())
	_hc.addCost(f, f->cumulative()->subCost(_costType));
      
      if (_activeItem && 
	  ((_activeItem->type() == TraceItem::Function) ||
	   (_activeItem->type() == TraceItem::FunctionCycle)))
	fitem = new FunctionItem(functionList, (TraceFunction*)_activeItem,
				 _costType, _groupType);
      
      for(int i=0;i<_hc.realCount();i++) {
	f = (TraceFunction*)_hc[i];
	if (f == _activeItem) continue;
	new FunctionItem(functionList, f, _costType, _groupType);
      }
      if (_hc.hasMore()) {
	// a placeholder for all the cost items skipped ...
	new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
			 (TraceFunction*)_hc[_hc.maxSize()-1], _costType);
      }
      functionList->sort();
      
      if (fitem && oldFunction) {
	functionList->setSelected(fitem, true);
	int newPos = functionList->itemPos(fitem) - functionList->contentsY();
	functionList->scrollBy(0, newPos-oldPos);
      }
      else if (fitem) {
	functionList->ensureItemVisible(fitem);
	functionList->setSelected(fitem, true);
      }
      else
	functionList->clearSelection();
      
      functionList->setUpdatesEnabled(true);
      functionList->repaint();
      //functionList->triggerUpdate();
      groupList->setUpdatesEnabled(true);
      groupList->repaint();
      return;
    }
  }
  
  // we always put group of active item in list, even if
  // it would be skipped because of small costs
  if (_group) 
    item = new CostListItem(groupList, _group, _costType);

  for(int i=0;i<_hc.realCount();i++) {
    group = (TraceCostItem*)_hc[i];
    // don't put group of active item twice into list
    if (group == _group) continue;
    new CostListItem(groupList, group, _costType);
  }
  if (_hc.hasMore()) {
      // a placeholder for all the cost items skipped ...
    new CostListItem(groupList, _hc.count() - _hc.maxSize(),
		     (TraceCostItem*)_hc[_hc.maxSize()-1], _costType);
  }
  groupList->sort();
  if (item) {
    groupList->ensureItemVisible(item);
    groupList->setSelected(item, true);
  }
  else
    groupList->clearSelection();

  groupList->setUpdatesEnabled(true);
  groupList->repaint();
}


void FunctionSelection::groupSelected(QListViewItem* i)
{
  if (!i) return;
  if (!_data) return;

  TraceCostItem* g = ((CostListItem*) i)->costItem();
  if (!g) return;

  _group = g;

  TraceFunctionList list;

  switch(g->type()) {
  case TraceItem::Object:
    list = ((TraceObject*)g)->functions();
    break;
  case TraceItem::Class:
    list = ((TraceClass*)g)->functions();
    break;
  case TraceItem::File:
    list = ((TraceFile*)g)->functions();
    break;
  case TraceItem::FunctionCycle:
    list = ((TraceFunctionCycle*)g)->members();
    break;
  default:
    return;
  }

  functionList->setUpdatesEnabled(false);

  functionList->clear();
  functionList->setColumnWidth(0, 50);
  functionList->setColumnWidth(1, 50);
  functionList->setColumnWidth(2, 50);

  double total;
  if (Configuration::showExpanded())
      total = (double) g->subCost(_costType);
  else
      total = (double) _data->subCost(_costType);
  if (total == 0.0) {
      functionList->setUpdatesEnabled(true);
      functionList->repaint();
      return;
  }

  TraceFunction *f;
  _hc.clear(Configuration::maxListCount());
  for (f=list.first();f;f=list.next())
    _hc.addCost(f, f->cumulative()->subCost(_costType));
            
  for(int i=0;i<_hc.realCount();i++)
    new FunctionItem(functionList, (TraceFunction*)_hc[i], _costType, _groupType);
  if (_hc.hasMore()) {
    // a placeholder for all the functions skipped ...
    new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
		     (TraceFunction*)_hc[_hc.maxSize()-1], _costType);
  }
  functionList->sort();

  functionList->setUpdatesEnabled(true);
  functionList->repaint();

  // Don't emit signal if cost item was changed programatically
  if (!_inSetCostItem) {
      _selectedItem = g;
      selected(g);
  }
}

void FunctionSelection::groupDoubleClicked(QListViewItem* i)
{
  if (!i) return;
  if (!_data) return;
  TraceCostItem* g = ((CostListItem*) i)->costItem();

  if (!g) return;
  // group must be selected first
  if (g != _group) return;

  activated(g);
}


TraceCostItem* FunctionSelection::group(QString s)
{
  QListViewItem *item;
  item  = groupList->firstChild();
  for(;item;item = item->nextSibling())
    if (((CostListItem*)item)->costItem()->name() == s)
      return ((CostListItem*)item)->costItem();

  return 0;
}



void FunctionSelection::functionSelected(QListViewItem* i)
{
  if (!i) return;
  if (!_data) return;

  TraceFunction* f = ((FunctionItem*) i)->function();
  if (!f) return;

  //qDebug("FunctionSelection::functionSelected %s", f->name().ascii());

  // Don't emit signal if function was changed programatically
  if (!_inSetFunction) {
      _selectedItem = f;
      selected(f);
  }
}

void FunctionSelection::functionActivated(QListViewItem* i)
{
  if (!i) return;
  if (!_data) return;
  TraceFunction* f = ((FunctionItem*) i)->function();

  if (!f) return;

  activated(f);
}


void FunctionSelection::query(QString query)
{
  QRegExp re(query, false, true);
  
  TraceFunctionList list;
  switch(_groupType) {
  case TraceItem::Object:
    list = ((TraceObject*)_group)->functions();
    break;
  case TraceItem::Class:
    list = ((TraceClass*)_group)->functions();
    break;
  case TraceItem::File:
    list = ((TraceFile*)_group)->functions();
    break;
  case TraceItem::FunctionCycle:
    list = ((TraceFunctionCycle*)_group)->members();
    break;
  default:
    break;
  }

  TraceFunction* f;
  TraceFunctionList list2;

  _hc.clear(Configuration::maxListCount());

  if (_groupType == TraceItem::Function) {
    TraceFunctionMap::Iterator it;
    for ( it = _data->functionMap().begin();
          it != _data->functionMap().end(); ++it ) {
	f = &(*it);
	if (re.search(f->prettyName())>=0)
	    _hc.addCost(f, f->cumulative()->subCost(_costType));
    }

    TraceFunctionCycleList l =  _data->functionCycles();
    for (f=l.first();f;f=l.next()) {
      if (re.search(f->prettyName())>=0)
	  _hc.addCost(f, f->cumulative()->subCost(_costType));
    }
  }
  else {
    for (f=list.first();f;f=list.next()) {
      if (re.search(f->name())>=0)
	  _hc.addCost(f, f->cumulative()->subCost(_costType));
    }
  }


  FunctionItem *fi, *item = 0;

  functionList->clear();
  functionList->setColumnWidth(0, 50);
  functionList->setColumnWidth(1, 50);
  functionList->setColumnWidth(2, 50);

  for(int i=0;i<_hc.realCount();i++) {
      fi = new FunctionItem(functionList, (TraceFunction*)_hc[i],
			    _costType, _groupType);
      if (_activeItem == f) item = fi;
  }
  if (_hc.hasMore()) {
      // a placeholder for all the functions skipped ...
      new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
		       (TraceFunction*)_hc[_hc.maxSize()-1], _costType);
  }

  functionList->sort();

  if (item) {
    functionList->ensureItemVisible(item);
    functionList->setSelected(item, true);
  }
  else
    functionList->setSelected(functionList->firstChild(), true);
}

bool FunctionSelection::setTopFunction()
{
  QListViewItem* i = functionList->firstChild();
  functionList->setSelected(i, true);
  return i!=0;
}


#include "functionselection.moc"
