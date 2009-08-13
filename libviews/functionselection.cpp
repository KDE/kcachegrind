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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * For function selection, to be put into a QDockWindow
 */

#include "functionselection.h"

#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QRegExp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <Qt3Support/Q3ListView>
#include <Qt3Support/Q3PopupMenu>
#include <Qt3Support/Q3Header>
#include <QDebug>

#include "traceitemview.h"
#include "stackbrowser.h"
#include "functionitem.h"
#include "costlistitem.h"
#include "globalconfig.h"


FunctionSelection::FunctionSelection( TopLevelBase* top,
				      QWidget* parent)
    : QWidget(parent), TraceItemView(0, top)
{
  _group = 0;
  _inSetGroup = false;
  _inSetFunction = false;

  setTitle(tr("Function Profile"));

  // first row with search label and group type combo
  QHBoxLayout* hboxLayout = new QHBoxLayout();
  hboxLayout->setSpacing(6);
  hboxLayout->setMargin(0);

  searchLabel = new QLabel(this);
  searchLabel->setText(tr("&Search:"));
  searchLabel->setWordWrap(false);
  hboxLayout->addWidget(searchLabel);

  searchEdit = new QLineEdit(this);
  searchLabel->setBuddy(searchEdit);
  hboxLayout->addWidget(searchEdit);

  groupBox = new QComboBox(this);
  hboxLayout->addWidget(groupBox);

  // vertical layout: first row, group list, function list
  QVBoxLayout* vboxLayout = new QVBoxLayout(this);
  vboxLayout->setSpacing(6);
  vboxLayout->setMargin(3);
  vboxLayout->addLayout(hboxLayout);

  groupList = new Q3ListView(this);
  groupList->addColumn(tr("Self"));
  groupList->addColumn(tr("Group"));
  groupList->header()->setClickEnabled(true);
  groupList->header()->setResizeEnabled(true);
  groupList->setMaximumHeight(150);
  vboxLayout->addWidget(groupList);

  functionList = new Q3ListView(this);
  functionList->addColumn(tr("Incl."));
  functionList->addColumn(tr("Self"));
  functionList->addColumn(tr("Called"));
  functionList->addColumn(tr("Function"));
  functionList->addColumn(tr("Location"));
  functionList->header()->setClickEnabled(true);
  functionList->header()->setResizeEnabled(true);
  vboxLayout->addWidget(functionList);

  // order has to match mapping in groupTypeSelected()
  QStringList args;
  args << tr("(No Grouping)")
       << ProfileContext::i18nTypeName(ProfileContext::Object)
       << ProfileContext::i18nTypeName(ProfileContext::File)
       << ProfileContext::i18nTypeName(ProfileContext::Class)
       << ProfileContext::i18nTypeName(ProfileContext::FunctionCycle);
  groupBox->addItems(args);
  connect(groupBox, SIGNAL(activated(int)),
	  this, SLOT(groupTypeSelected(int)));

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
  functionList->setColumnWidthMode(3, Q3ListView::Manual);
  functionList->setColumnWidth(3, 200);
  functionList->setColumnWidthMode(4, Q3ListView::Manual);
  functionList->setColumnWidth(4, 200);

  groupList->setSorting(0,false);
  groupList->setColumnAlignment(0, Qt::AlignRight);
  groupList->setAllColumnsShowFocus(true);
  // groupList->setShowSortIndicator(true);
  groupList->setResizeMode(Q3ListView::LastColumn);

#if 0
  // single click press activation
  connect(functionList, SIGNAL(selectionChanged(Q3ListViewItem*)),
          this, SLOT(functionActivated(Q3ListViewItem*)));
  connect(functionList,
	  SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint &, int)),
          this, SLOT(functionContext(Q3ListViewItem*, const QPoint &, int)));
#else
  // single click release activation
  connect(functionList, SIGNAL(selectionChanged(Q3ListViewItem*)),
          this, SLOT(functionSelected(Q3ListViewItem*)));
  connect(functionList, SIGNAL(clicked(Q3ListViewItem*)),
	  this, SLOT(functionActivated(Q3ListViewItem*)));
  connect(functionList, SIGNAL(returnPressed(Q3ListViewItem*)),
	  this, SLOT(functionActivated(Q3ListViewItem*)));
  connect(functionList,
	  SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint &, int)),
          this, SLOT(functionContext(Q3ListViewItem*, const QPoint &, int)));
#endif

  connect(groupList, SIGNAL(selectionChanged(Q3ListViewItem*)),
          this, SLOT(groupSelected(Q3ListViewItem*)));
  connect(groupList, SIGNAL(doubleClicked(Q3ListViewItem*)),
	  this, SLOT(groupDoubleClicked(Q3ListViewItem*)));
  connect(groupList, SIGNAL(returnPressed(Q3ListViewItem*)),
	  this, SLOT(groupDoubleClicked(Q3ListViewItem*)));
  connect(groupList,
	  SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint &, int)),
          this, SLOT(groupContext(Q3ListViewItem*, const QPoint &, int)));

  // start hidden
  groupList->hide();

  setWhatsThis(whatsThis());
}

QString FunctionSelection::whatsThis() const
{
    return tr(
	// xgettext: no-c-format
	"<b>The Flat Profile</b>"
	"<p>The flat profile contains a group and a function "
	"selection list. The group list contains all groups "
	"where costs "
	"are spent in, depending on the chosen group type. "
	"The group list is hidden when group type 'Function' "
	"is selected.</p>"
	"<p>The function list contains the functions of the "
	"selected group (or all for 'Function' group type), "
	"ordered by the costs spent therein. Functions with "
	"costs less than 1% are hidden on default.</p>");
}

void FunctionSelection::searchReturnPressed()
{
  query(searchEdit->text());

  Q3ListViewItem* item;
  if (_groupType != ProfileContext::Function) {
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

void FunctionSelection::functionContext(Q3ListViewItem* i,
					const QPoint & p, int c)
{
  Q3PopupMenu popup;
  TraceFunction* f = 0;

	if (i) {
		f = ((FunctionItem*) i)->function();
		if (f) {
		    popup.insertItem(tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(f->prettyName())), 93);
		    popup.insertSeparator();
		}
	}

  if ((c == 0) || (c == 1)) {
    addEventTypeMenu(&popup,false);
    popup.insertSeparator();
  }
  addGroupMenu(&popup);  
  popup.insertSeparator();
  addGoMenu(&popup);

  int r = popup.exec(p);
  if (r == 93) activated(f);
}

void FunctionSelection::groupContext(Q3ListViewItem* /*i*/,
				     const QPoint & p, int c)
{
  Q3PopupMenu popup;

#if 0
  TraceCostItem* g = 0;
  if (i) {
      g = ((CostListItem*) i)->costItem();
      if (!g) {
	popup.insertItem(tr("Show All Items"), 93);
	popup.insertSeparator();
      }
  }
#endif
  if (c == 0) {
    addEventTypeMenu(&popup,false);
    popup.insertSeparator();
  }
  addGroupMenu(&popup);  
  popup.insertSeparator();
  addGoMenu(&popup);

  popup.exec(p);
}


void FunctionSelection::addGroupMenu(Q3PopupMenu* popup)
{
  Q3PopupMenu *popup1 = new Q3PopupMenu(popup);
  popup1->setCheckable(true);

  if (_groupType != ProfileContext::Function) {
    popup1->insertItem(tr("No Grouping"),0);
    popup1->insertSeparator();
  }
  popup1->insertItem(ProfileContext::i18nTypeName(ProfileContext::Object),1);
  popup1->insertItem(ProfileContext::i18nTypeName(ProfileContext::File),2);
  popup1->insertItem(ProfileContext::i18nTypeName(ProfileContext::Class),3);
  popup1->insertItem(ProfileContext::i18nTypeName(ProfileContext::FunctionCycle),4);
  switch(_groupType) {
  case ProfileContext::Object:        popup1->setItemChecked(1, true); break;
  case ProfileContext::File:          popup1->setItemChecked(2, true); break;
  case ProfileContext::Class:         popup1->setItemChecked(3, true); break;
  case ProfileContext::FunctionCycle: popup1->setItemChecked(4, true); break;
  default: break;
  }
  connect(popup1, SIGNAL(activated(int)),
	  this, SLOT(groupTypeSelected(int)));

  popup->insertItem(tr("Grouping"), popup1);
}    


void FunctionSelection::groupTypeSelected(int cg)
{
    switch(cg) {
	case 0: selectedGroupType( ProfileContext::Function ); break;
	case 1: selectedGroupType( ProfileContext::Object ); break;
	case 2: selectedGroupType( ProfileContext::File ); break;
	case 3: selectedGroupType( ProfileContext::Class ); break;
	case 4: selectedGroupType( ProfileContext::FunctionCycle ); break;
	default: break;
    }
}


CostItem* FunctionSelection::canShow(CostItem* i)
{
    ProfileContext::Type t = i ? i->type() : ProfileContext::InvalidType;

    switch(t) {
    case ProfileContext::Function:
    case ProfileContext::FunctionCycle:
    case ProfileContext::Object:
    case ProfileContext::File:
    case ProfileContext::Class:
	break;

    case ProfileContext::Instr:
	i = ((TraceInstr*)i)->function();
	break;

    case ProfileContext::Line:
	i = ((TraceLine*)i)->functionSource()->function();
	break;

    default:
	i = 0;
	break;
  }
  return i;
}


void FunctionSelection::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) return;

    // we do not show cost 2 at all...
    if (changeType == eventType2Changed) return;

    if (changeType == activeItemChanged) {
	if (_activeItem ==0) {
	    functionList->clearSelection();
	    return;
	}
	switch(_activeItem->type()) {
	case ProfileContext::Object:
	case ProfileContext::File:
	case ProfileContext::Class:
	    setGroup((TraceCostItem*)_activeItem);
	    return;
	default: break;
	}

	// active item is a function
	TraceFunction* f = (TraceFunction*) _activeItem;

	// if already current, nothing to do
	Q3ListViewItem* i = functionList->currentItem();
	if (i && (((FunctionItem*)i)->function() == f)) {
	    functionList->setSelected(i,true);
	    return;	    
	}

	// reset searchEdit (as not activated from this view)
	_searchString = QString();
	query(QString::null);	//krazy:exclude=nullstrassign for old broken gcc

	// select cost item group of function
	switch(_groupType) {
	case ProfileContext::Object: setGroup(f->object()); break;
	case ProfileContext::Class:  setGroup(f->cls()); break;
	case ProfileContext::File:   setGroup(f->file());  break;
	case ProfileContext::FunctionCycle: setGroup(f->cycle());  break;
	default:
	    break;
	}

	Q3ListViewItem* item  = functionList->firstChild();
	for (;item;item = item->nextSibling())
	    if (((FunctionItem*)item)->function() == f)
		break;

	if (!item)
	    item = new FunctionItem(functionList, f, _eventType, _groupType);

	functionList->ensureItemVisible(item);
	// prohibit signalling of a function selection
	_inSetFunction = true;
	functionList->setSelected(item, true);
	_inSetFunction = false;

	return;
    }

    if (changeType & groupTypeChanged) {
	if (_activeItem && (_activeItem->type() == ProfileContext::Function)) {
	    TraceFunction* f = (TraceFunction*) _activeItem;

	    // select cost item group of function
	    switch(_groupType) {
	    case ProfileContext::Object: _group = f->object(); break;
	    case ProfileContext::Class:  _group = f->cls(); break;
	    case ProfileContext::File:   _group = f->file();  break;
	    case ProfileContext::FunctionCycle: _group = f->cycle();  break;
	    default:
		_group = 0;
		break;
	    }
	}

	int id;
	switch(_groupType) {
	case ProfileContext::Object: id = 1; break;
	case ProfileContext::File:   id = 2; break;
	case ProfileContext::Class:  id = 3; break;
	case ProfileContext::FunctionCycle: id = 4; break;
	default: id = 0; break;
	}
	groupBox->setCurrentIndex(id);

	if (_groupType == ProfileContext::Function)
	    groupList->hide();
	else
	    groupList->show();
    }

    // reset searchEdit
    _searchString = QString();
    query(QString::null);	//krazy:exclude=nullstrassign for old broken gcc

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

  Q3ListViewItem* item  = groupList->firstChild();
  for (;item;item = item->nextSibling())
    if (((CostListItem*)item)->costItem() == g)
      break;

  if (item) {
    groupList->ensureItemVisible(item);
    // prohibit signalling of a group selection
    _inSetGroup = true;
    _group = 0;
    groupList->setSelected(item, true);
    _inSetGroup = false;
  }
  else
    groupList->clearSelection();
}


void FunctionSelection::refresh()
{
    groupList->clear();

    // make cost columns as small as possible:
    // the new functions make them as wide as needed
    groupList->setColumnWidth(0, 50);

    groupList->setColumnText(1, ProfileContext::i18nTypeName(_groupType));

    if (!_data || _data->parts().count()==0) {
	functionList->clear();

	// this clears all other lists
	functionList->setSelected(functionList->firstChild(), true);
	return;
    }

  TraceObjectMap::Iterator oit;
  TraceClassMap::Iterator cit;
  TraceFileMap::Iterator fit;
  Q3ListViewItem *i = 0, *item = 0, *fitem = 0;

  // Fill up group list.
  // Always show group of current function, even if cost below low limit.
  //

  _hc.clear(GlobalConfig::maxListCount());

  TraceCostItem *group;

  // update group from _activeItem if possible
  if (_activeItem && (_activeItem->type() == _groupType))
    _group = (TraceCostItem*) _activeItem;

  switch(_groupType) {
  case ProfileContext::Object:

    for ( oit = _data->objectMap().begin();
          oit != _data->objectMap().end(); ++oit )
      _hc.addCost(&(*oit), (*oit).subCost(_eventType));
    break;

  case ProfileContext::Class:

    for ( cit = _data->classMap().begin();
          cit != _data->classMap().end(); ++cit )
      _hc.addCost(&(*cit), (*cit).subCost(_eventType));
    break;

  case ProfileContext::File:

    for ( fit = _data->fileMap().begin();
          fit != _data->fileMap().end(); ++fit )
      _hc.addCost(&(*fit), (*fit).subCost(_eventType));
    break;

  case ProfileContext::FunctionCycle:
    {
      // add all cycles
      TraceFunctionCycleList l =  _data->functionCycles();
      for (group=l.first();group;group=l.next())
	_hc.addCost(group, group->subCost(_eventType));
    }

  break;

  default:
    {
      Q3ListViewItem* oldItem = functionList->selectedItem();
      TraceFunction* oldFunction = 0;
      int oldPos = 0;
      if (oldItem) {
	oldFunction = ((FunctionItem*)oldItem)->function();
	oldPos = oldItem->itemPos();
	oldPos -= functionList->contentsY();
	if (oldPos < 0 || oldPos > functionList->height())
	  oldFunction = 0;
      }

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
	_hc.addCost(&(*it), (*it).inclusive()->subCost(_eventType));

      TraceFunctionCycleList l =  _data->functionCycles();
      for (f=l.first();f;f=l.next())
	_hc.addCost(f, f->inclusive()->subCost(_eventType));

      if (_activeItem &&
	  ((_activeItem->type() == ProfileContext::Function) ||
	   (_activeItem->type() == ProfileContext::FunctionCycle)))
	fitem = new FunctionItem(functionList, (TraceFunction*)_activeItem,
				 _eventType, _groupType);

      for(int i=0;i<_hc.realCount();i++) {
	f = (TraceFunction*)_hc[i];
	if (f == _activeItem) continue;
	new FunctionItem(functionList, f, _eventType, _groupType);
      }
      if (_hc.hasMore()) {
	// a placeholder for all the cost items skipped ...
	new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
			 (TraceFunction*)_hc[_hc.maxSize()-1], _eventType);
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

      return;
    }
  }

  // we always put group of active item in list, even if
  // it would be skipped because of small costs
  if (_group)
    item = new CostListItem(groupList, _group, _eventType);

  for(int i=0;i<_hc.realCount();i++) {
    group = (TraceCostItem*)_hc[i];
    // do not put group of active item twice into list
    if (group == _group) continue;
    new CostListItem(groupList, group, _eventType);
  }
  if (_hc.hasMore()) {
      // a placeholder for all the cost items skipped ...
    new CostListItem(groupList, _hc.count() - _hc.maxSize(),
		     (TraceCostItem*)_hc[_hc.maxSize()-1], _eventType);
  }
  groupList->sort();
  if (item) {
    groupList->ensureItemVisible(item);
    _inSetGroup = true;
    _group = 0;
    groupList->setSelected(item, true);
    _inSetGroup = false;
  }
  else
    groupList->clearSelection();
}


void FunctionSelection::groupSelected(Q3ListViewItem* i)
{
  if (!i) return;
  if (!_data) return;

  TraceCostItem* g = ((CostListItem*) i)->costItem();
  if (!g) return;
  if (g == _group) return;
  _group = g;

  TraceFunctionList list;

  switch(g->type()) {
  case ProfileContext::Object:
    list = ((TraceObject*)g)->functions();
    break;
  case ProfileContext::Class:
    list = ((TraceClass*)g)->functions();
    break;
  case ProfileContext::File:
    list = ((TraceFile*)g)->functions();
    break;
  case ProfileContext::FunctionCycle:
    list = ((TraceFunctionCycle*)g)->members();
    break;
  default:
    return;
  }

  functionList->clear();
  setCostColumnWidths();

  double total;
  if (GlobalConfig::showExpanded())
      total = (double) g->subCost(_eventType);
  else
      total = (double) _data->subCost(_eventType);

  QRegExp re(_searchString, false, true);

  FunctionItem* fitem = 0;
  TraceFunction *f;
  _hc.clear(GlobalConfig::maxListCount());
  for (f=list.first();f;f=list.next()) {
    if (re.search(f->prettyName())<0) continue;

    _hc.addCost(f, f->inclusive()->subCost(_eventType));
    if (_activeItem == f)
      fitem = new FunctionItem(functionList, (TraceFunction*)_activeItem,
			       _eventType, _groupType);
  }

  for(int i=0;i<_hc.realCount();i++) {
    if (_activeItem == (TraceFunction*)_hc[i]) continue;
    new FunctionItem(functionList, (TraceFunction*)_hc[i],
		     _eventType, _groupType);
  }

  if (_hc.hasMore()) {
    // a placeholder for all the functions skipped ...
    new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
		     (TraceFunction*)_hc[_hc.maxSize()-1], _eventType);
  }
  functionList->sort();

  if (fitem) {
    functionList->ensureItemVisible(fitem);
    _inSetFunction = true;
    functionList->setSelected(fitem, true);
    _inSetFunction = false;
  }

  // Do not emit signal if cost item was changed programatically
  if (!_inSetGroup) {
      if (_topLevel)
	  _topLevel->setGroupDelayed(g);
  }
}

void FunctionSelection::groupDoubleClicked(Q3ListViewItem* i)
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
  Q3ListViewItem *item;
  item  = groupList->firstChild();
  for(;item;item = item->nextSibling())
    if (((CostListItem*)item)->costItem()->name() == s)
      return ((CostListItem*)item)->costItem();

  return 0;
}



void FunctionSelection::functionSelected(Q3ListViewItem* i)
{
  if (!i) return;
  if (!_data) return;

  TraceFunction* f = ((FunctionItem*) i)->function();
  if (!f) return;

  //qDebug("FunctionSelection::functionSelected %s", f->name().ascii());

  // Do not emit signal if function was changed programatically
  if (!_inSetFunction) {
      _selectedItem = f;
      selected(f);
  }
}

void FunctionSelection::functionActivated(Q3ListViewItem* i)
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
  Q3ListViewItem* item  = groupList->firstChild();
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
  if(!_data)
     return;  
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

  _hc.clear(GlobalConfig::maxListCount());

  TraceFunctionMap::Iterator it;
  for ( it = _data->functionMap().begin();
	it != _data->functionMap().end(); ++it ) {
    f = &(*it);
    if (re.search(f->prettyName())>=0) {
      if (_group) {
	if (_groupType==ProfileContext::Object) {
	  if (_groupSize.contains(f->object()))
	    _groupSize[f->object()]++;
	  else
	    _groupSize[f->object()] = 1;
	  if (f->object() != _group) continue;
	}
	else if (_groupType==ProfileContext::Class) {
	  if (_groupSize.contains(f->cls()))
	    _groupSize[f->cls()]++;
	  else
	    _groupSize[f->cls()] = 1;
	  if (f->cls() != _group) continue;
	}
	else if (_groupType==ProfileContext::File) {
	  if (_groupSize.contains(f->file()))
	    _groupSize[f->file()]++;
	  else
	    _groupSize[f->file()] = 1;
	  if (f->file() != _group) continue;
	}
	else if (_groupType==ProfileContext::FunctionCycle) {
	  if (_groupSize.contains(f->cycle()))
	    _groupSize[f->cycle()]++;
	  else
	    _groupSize[f->cycle()] = 1;
	  if (f->cycle() != _group) continue;
	}
      }
      _hc.addCost(f, f->inclusive()->subCost(_eventType));
    }
  }
  updateGroupSizes(true);

  FunctionItem *fi, *item = 0;

  functionList->clear();
  setCostColumnWidths();
  for(int i=0;i<_hc.realCount();i++) {
      fi = new FunctionItem(functionList, (TraceFunction*)_hc[i],
			    _eventType, _groupType);
      if (_activeItem == f) item = fi;
  }
  if (_hc.hasMore()) {
      // a placeholder for all the functions skipped ...
      new FunctionItem(functionList, _hc.count() - _hc.maxSize(),
		       (TraceFunction*)_hc[_hc.maxSize()-1], _eventType);
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
  Q3ListViewItem* i = functionList->firstChild();
  // this emits a function selection
  functionList->setSelected(i, true);
  functionActivated(i);
  return i!=0;
}

void FunctionSelection::setCostColumnWidths()
{
  if (_eventType && (_eventType->subCost(_data->callMax())>0) ) {
    functionList->setColumnWidthMode(0, Q3ListView::Maximum);
    functionList->setColumnWidth(0,50);
    functionList->setColumnWidthMode(2, Q3ListView::Maximum);
    functionList->setColumnWidth(2,50);
  }
  else {
    functionList->setColumnWidthMode(0, Q3ListView::Manual);
    functionList->setColumnWidth(0,0);
    functionList->setColumnWidthMode(2, Q3ListView::Manual);
    functionList->setColumnWidth(2,0);
  }

  functionList->setColumnWidth(1, 50);
}

#include "functionselection.moc"
