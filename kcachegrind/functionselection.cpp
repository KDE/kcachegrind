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
#include "toplevel.h"

FunctionSelection::FunctionSelection( TopLevel* top,
				      QWidget* parent, const char* name)
    : FunctionSelectionBase(parent, name), TraceItemView(0, top)
{
  _group = 0;
  _inSetGroup = false;
  _inSetFunction = false;

  QStringList args;
  args << i18n("(No Grouping)")
       << TraceCost::i18nTypeName(TraceItem::Object)
       << TraceCost::i18nTypeName(TraceItem::File)
       << TraceCost::i18nTypeName(TraceItem::Class)
       << TraceCost::i18nTypeName(TraceItem::FunctionCycle);
  
  groupBox->insertStringList(args);
  // this needs same order of grouptype actionlist!
  connect(groupBox, SIGNAL(activated(int)),
	  top, SLOT(groupTypeSelected(int)));

  // search while typing...
  connect(searchEdit, SIGNAL(textChanged(const QString&)),
	  this, SLOT(searchChanged(const QString&)));
  connect(&_searchTimer, SIGNAL(timeout()),
	  this, SLOT(queryDelayed()));
  // select first matching group/function on return
  connect(searchEdit, SIGNAL(returnPressed()),
	  this, SLOT(searchReturnPressed()));
  searchEdit->setMinimumWidth(50);

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

#if 0
  // single click press activation
  connect(functionList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(functionActivated(QListViewItem*)));
  connect(functionList,
	  SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
          this, SLOT(functionContext(QListViewItem*, const QPoint &, int)));
#else
  // single click release activation
  connect(functionList, SIGNAL(selectionChanged(QListViewItem*)),
          this, SLOT(functionSelected(QListViewItem*)));
  connect(functionList, SIGNAL(clicked(QListViewItem*)),
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

void FunctionSelection::searchReturnPressed()
{
  query(searchEdit->text());

  QListViewItem* item;
  if (_groupType != TraceItem::Function) {
      // if current group not matching, select first matching group
      item  = groupList->currentItem();
      if (!item || !item->isVisible()) {
	  item  = groupList->firstChild();
	  for (;item;item = item->nextSibling())
	      if (item->isVisible()) break;
	  if (!item) return;

	  setGroup(((CostListItem*)item)->costItem());
	  return;
      }
  }

  functionActivated(functionList->firstChild());
}

// trigger the query after some delay, dependent on length
void FunctionSelection::searchChanged(const QString& q)
{
  _searchDelayed = q;
  int ms = 100;
  if (q.length()<5) ms = 200;
  if (q.length()<2) ms = 300;
  _searchTimer.start(ms,true);
}

void FunctionSelection::queryDelayed()
{
  query(_searchDelayed);
}

void FunctionSelection::functionContext(QListViewItem* i,
					const QPoint & p, int c)
{
  QPopupMenu popup;
  TraceFunction* f = 0;

  if (i) {
      f = ((FunctionItem*) i)->function();
      if (f) {
	  popup.insertItem(i18n("Go to %1").arg(f->prettyName()), 93);
	  popup.insertSeparator();
      }
  }

  if ((c == 0) || (c == 1)) {
    addCostMenu(&popup,false);
    popup.insertSeparator();
  }
  addGroupMenu(&popup);  
  popup.insertSeparator();
  addGoMenu(&popup);

  int r = popup.exec(p);
  if (r == 93) activated(f);
}

void FunctionSelection::groupContext(QListViewItem* /*i*/,
				     const QPoint & p, int c)
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
  if (c == 0) {
    addCostMenu(&popup,false);
    popup.insertSeparator();
  }
  addGroupMenu(&popup);  
  popup.insertSeparator();
  addGoMenu(&popup);

  popup.exec(p);
}


void FunctionSelection::addGroupMenu(QPopupMenu* popup)
{
  QPopupMenu *popup1 = new QPopupMenu(popup);
  popup1->setCheckable(true);

  if (_groupType != TraceItem::Function) {
    popup1->insertItem(i18n("No Grouping"),0);
    popup1->insertSeparator();
  }
  popup1->insertItem(TraceCost::i18nTypeName(TraceItem::Object),1);
  popup1->insertItem(TraceCost::i18nTypeName(TraceItem::File),2);
  popup1->insertItem(TraceCost::i18nTypeName(TraceItem::Class),3);
  popup1->insertItem(TraceCost::i18nTypeName(TraceItem::FunctionCycle),4);
  switch(_groupType) {
  case TraceItem::Object:        popup1->setItemChecked(1, true); break;
  case TraceItem::File:          popup1->setItemChecked(2, true); break;
  case TraceItem::Class:         popup1->setItemChecked(3, true); break;
  case TraceItem::FunctionCycle: popup1->setItemChecked(4, true); break;
  default: break;
  }
  connect(popup1,SIGNAL(activated(int)),
	  _topLevel,SLOT(groupTypeSelected(int)));

  popup->insertItem(i18n("Grouping"), popup1);
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

    // we don't show cost 2 at all...
    if (changeType == costType2Changed) return;

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

	// if already current, nothing to do
	QListViewItem* i = functionList->currentItem();
	if (i && (((FunctionItem*)i)->function() == f)) {
	    functionList->setSelected(i,true);
	    return;	    
	}

	// reset searchEdit (as not activated from this view)
	_searchString = QString::null;
	query(QString::null);

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

	int id;
	switch(_groupType) {
	case TraceItem::Object: id = 1; break;
	case TraceItem::File:   id = 2; break;
	case TraceItem::Class:  id = 3; break;
	case TraceItem::FunctionCycle: id = 4; break;
	default: id = 0; break;
	}
	groupBox->setCurrentItem(id);

	if (_groupType == TraceItem::Function)
	    groupList->hide();
	else
	    groupList->show();
    }

    // reset searchEdit
    _searchString = QString::null;
    query(QString::null);

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
    _inSetGroup = true;
    groupList->setSelected(item, true);
    _inSetGroup = false;
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

    if (!_data || _data->parts().count()==0) {
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

      // switching off QListView updates is buggy with some QT versions...
      //functionList->setUpdatesEnabled(false);
      functionList->clear();
      setCostColumnWidths();

      if (0) qDebug("Function %s at %d, Item %p",
		    oldFunction ? oldFunction->name().ascii() : "-",
		    oldPos, (void*)oldItem);

      TraceFunctionMap::Iterator it;
      TraceFunction *f;
      i = 0;
      fitem = 0;
      for ( it = _data->functionMap().begin();
	    it != _data->functionMap().end(); ++it )
	_hc.addCost(&(*it), (*it).inclusive()->subCost(_costType));

      TraceFunctionCycleList l =  _data->functionCycles();
      for (f=l.first();f;f=l.next())
	_hc.addCost(f, f->inclusive()->subCost(_costType));

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
        _inSetFunction = true;
	functionList->setSelected(fitem, true);
        _inSetFunction = false;
	int newPos = functionList->itemPos(fitem) - functionList->contentsY();
	functionList->scrollBy(0, newPos-oldPos);
      }
      else if (fitem) {
	functionList->ensureItemVisible(fitem);
        _inSetFunction = true;
	functionList->setSelected(fitem, true);
        _inSetFunction = false;
      }
      else
	functionList->clearSelection();

      //functionList->setUpdatesEnabled(true);
      //functionList->repaint();
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
    _inSetGroup = true;
    groupList->setSelected(item, true);
    _inSetGroup = false;
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

  // switching off QListView updates is buggy with some QT versions...
  //functionList->setUpdatesEnabled(false);

  functionList->clear();
  setCostColumnWidths();

  double total;
  if (Configuration::showExpanded())
      total = (double) g->subCost(_costType);
  else
      total = (double) _data->subCost(_costType);
#if 0
  if (total == 0.0) {
      functionList->setUpdatesEnabled(true);
      functionList->repaint();
      return;
  }
#endif

  QRegExp re(_searchString, false, true);

  FunctionItem* fitem = 0;
  TraceFunction *f;
  _hc.clear(Configuration::maxListCount());
  for (f=list.first();f;f=list.next()) {
    if (re.search(f->prettyName())<0) continue;

    _hc.addCost(f, f->inclusive()->subCost(_costType));
    if (_activeItem == f)
      fitem = new FunctionItem(functionList, (TraceFunction*)_activeItem,
			       _costType, _groupType);
  }

  for(int i=0;i<_hc.realCount();i++) {
    if (_activeItem == (TraceFunction*)_hc[i]) continue;
    new FunctionItem(functionList, (TraceFunction*)_hc[i],
		     _costType, _groupType);
  }

  if (_hc.hasMore()) {
    // a placeholder for all the functions skipped ...
    new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
		     (TraceFunction*)_hc[_hc.maxSize()-1], _costType);
  }
  functionList->sort();

  if (fitem) {
    functionList->ensureItemVisible(fitem);
    _inSetFunction = true;
    functionList->setSelected(fitem, true);
    _inSetFunction = false;
  }

  //functionList->setUpdatesEnabled(true);
  //functionList->repaint();

  // Don't emit signal if cost item was changed programatically
  if (!_inSetGroup) {
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

  if (!_inSetFunction)
    activated(f);
}

void FunctionSelection::updateGroupSizes(bool hideEmpty)
{
  QListViewItem* item  = groupList->firstChild();
  for (;item;item = item->nextSibling()) {
    CostListItem* i = (CostListItem*)item;
    int size = (_groupSize.contains(i->costItem())) ?
	_groupSize[i->costItem()] : -1;    
    i->setSize(size);
    i->setVisible(!hideEmpty || (size>0));
  }
}

void FunctionSelection::query(QString query)
{
  if (searchEdit->text() != query)
    searchEdit->setText(query);
  if (_searchString == query) {
    // when resetting query, get rid of group sizes
    if (query.isEmpty()) {
      _groupSize.clear();
      updateGroupSizes(false);
    }
    return;
  }
  _searchString = query;

  QRegExp re(query, false, true);
  _groupSize.clear();

  TraceFunction* f = 0;
  TraceFunctionList list2;

  _hc.clear(Configuration::maxListCount());

  TraceFunctionMap::Iterator it;
  for ( it = _data->functionMap().begin();
	it != _data->functionMap().end(); ++it ) {
    f = &(*it);
    if (re.search(f->prettyName())>=0) {
      if (_group) {
	if (_groupType==TraceItem::Object) {
	  if (_groupSize.contains(f->object()))
	    _groupSize[f->object()]++;
	  else
	    _groupSize[f->object()] = 1;
	  if (f->object() != _group) continue;
	}
	else if (_groupType==TraceItem::Class) {
	  if (_groupSize.contains(f->cls()))
	    _groupSize[f->cls()]++;
	  else
	    _groupSize[f->cls()] = 1;
	  if (f->cls() != _group) continue;
	}
	else if (_groupType==TraceItem::File) {
	  if (_groupSize.contains(f->file()))
	    _groupSize[f->file()]++;
	  else
	    _groupSize[f->file()] = 1;
	  if (f->file() != _group) continue;
	}
	else if (_groupType==TraceItem::FunctionCycle) {
	  if (_groupSize.contains(f->cycle()))
	    _groupSize[f->cycle()]++;
	  else
	    _groupSize[f->cycle()] = 1;
	  if (f->cycle() != _group) continue;
	}
      }
      _hc.addCost(f, f->inclusive()->subCost(_costType));
    }
  }

  updateGroupSizes(true);

  FunctionItem *fi, *item = 0;

  functionList->clear();
  setCostColumnWidths();

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
    _inSetFunction = true;
    functionList->setSelected(item, true);
    _inSetFunction = false;
  }
  else {
    // this emits a function selection
    functionList->setSelected(functionList->firstChild(), true);
  }
}

bool FunctionSelection::setTopFunction()
{
  QListViewItem* i = functionList->firstChild();
  // this emits a function selection
  functionList->setSelected(i, true);
  functionActivated(i);
  return i!=0;
}

void FunctionSelection::setCostColumnWidths()
{
  if (_costType && (_costType->subCost(_data->callMax())>0) ) {
    functionList->setColumnWidthMode(0, QListView::Maximum);
    functionList->setColumnWidth(0,50);
    functionList->setColumnWidthMode(2, QListView::Maximum);
    functionList->setColumnWidth(2,50);
  }
  else {
    functionList->setColumnWidthMode(0, QListView::Manual);
    functionList->setColumnWidth(0,0);
    functionList->setColumnWidthMode(2, QListView::Manual);
    functionList->setColumnWidth(2,0);
  }

  functionList->setColumnWidth(1, 50);
}



#include "functionselection.moc"
