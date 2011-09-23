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
#include <QAction>
#include <QMenu>
#include <QDebug>
#include <QTreeView>
#include <QHeaderView>

#include "traceitemview.h"
#include "stackbrowser.h"
#include "costlistitem.h"
#include "globalconfig.h"
#include "functionlistmodel.h"


FunctionSelection::FunctionSelection( TopLevelBase* top,
				      QWidget* parent)
    : QWidget(parent), TraceItemView(0, top)
{
  _group = 0;
  _inSetGroup = false;
  _inSetFunction = false;
  _functionListSortOrder = Qt::DescendingOrder;

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

  groupList = new QTreeWidget(this);
  QStringList groupHeader;
  groupHeader << tr("Self") << tr("Group");
  groupList->setHeaderLabels(groupHeader);
  groupList->header()->setClickable(true);
  groupList->header()->setSortIndicatorShown(false);
  groupList->header()->stretchLastSection();
  groupList->setIconSize(QSize(99,99));
  groupList->setMaximumHeight(150);
  groupList->setRootIsDecorated(false);
  groupList->setUniformRowHeights(true);
  groupList->sortByColumn(0, Qt::AscendingOrder);
  vboxLayout->addWidget(groupList);

  functionListModel = new FunctionListModel();
  functionListModel->setMaxCount(GlobalConfig::maxListCount());

  functionList = new QTreeView(this);
  functionList->setRootIsDecorated(false);
  functionList->setAllColumnsShowFocus(true);
  functionList->setContextMenuPolicy(Qt::CustomContextMenu);
  functionList->setUniformRowHeights(true);
  functionList->header()->setClickable(true);
  functionList->header()->setSortIndicatorShown(false);
  functionList->header()->setSortIndicator(0, Qt::DescendingOrder);
  functionList->header()->setResizeMode(QHeaderView::Interactive);
  // for columns 3 and 4 (all others get resized)
  functionList->header()->setDefaultSectionSize(200);
  functionList->setModel(functionListModel);
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

  // single click release activation
  connect(functionList, SIGNAL(clicked(QModelIndex)),
          this, SLOT(functionActivated(QModelIndex)));
  connect(functionList, SIGNAL(activated(QModelIndex)),
          this, SLOT(functionActivated(QModelIndex)));
  connect(functionList, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(functionContext(const QPoint &)));
  connect(functionList->header(), SIGNAL(sectionClicked(int)),
          this, SLOT(functionHeaderClicked(int)));

  connect(groupList,
          SIGNAL( currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
          this, SLOT( groupSelected(QTreeWidgetItem*,QTreeWidgetItem*) ) );
  connect(groupList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
          this, SLOT(groupDoubleClicked(QTreeWidgetItem*,int)));

  groupList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(groupList,
          SIGNAL(customContextMenuRequested(const QPoint &) ),
          this, SLOT(groupContext(const QPoint &)));
  connect(groupList->header(),
          SIGNAL(sectionClicked(int)), this, SLOT(groupHeaderClicked(int)));

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

void FunctionSelection::setData(TraceData* d)
{
  TraceItemView::setData(d);

  _group = 0;
  _groupSize.clear();
  _hc.clear(GlobalConfig::maxListCount());
  groupList->clear();
  functionListModel->resetModelData(d, 0, QString(), 0);
}

void FunctionSelection::searchReturnPressed()
{
  query(searchEdit->text());

  QTreeWidgetItem* item;
  if (_groupType != ProfileContext::Function) {
      // if current group not matching, select first matching group
      item  = groupList->currentItem();
      if (!item || item->isHidden()) {
          int i;
          item = 0;
          for (i=0; i<groupList->topLevelItemCount(); i++) {
              item = groupList->topLevelItem(i);
              if (!item->isHidden()) break;
          }
          if (!item) return;

          setGroup(((CostListItem*)item)->costItem());
          return;
      }
  }
  // activate top function in functionList
  selectTopFunction();
  functionList->setFocus();
}

// trigger the query after some delay, dependent on length
void FunctionSelection::searchChanged(const QString& q)
{
  _searchDelayed = q;
  int ms = 100;
  if (q.length()<5) ms = 200;
  if (q.length()<2) ms = 300;
  _searchTimer.setSingleShot(true);
  _searchTimer.start(ms);
}

void FunctionSelection::queryDelayed()
{
  query(_searchDelayed);
}

void FunctionSelection::functionContext(const QPoint & p)
{
    QMenu popup;
    TraceFunction* f = 0;

    QAction* activateFunctionAction = 0;
    QModelIndex i = functionList->indexAt(p);
    if (i.isValid()) {
        f = functionListModel->function(i);
	if (f) {
	    QString menuText = tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(f->prettyName()));
	    activateFunctionAction = popup.addAction(menuText);
	    popup.addSeparator();
	}
        if ((i.column() == 0) || (i.column() == 1)) {
            addEventTypeMenu(&popup,false);
            popup.addSeparator();
        }
    }

    addGroupMenu(&popup);
    popup.addSeparator();
    addGoMenu(&popup);

    QPoint pDiff = QPoint(0, functionList->header()->height());
    QAction* a = popup.exec(functionList->mapToGlobal(p + pDiff));
    if (a == activateFunctionAction)
	activated(f);
}

void FunctionSelection::groupContext(const QPoint & p)
{
    QMenu popup;

    int c = groupList->columnAt(p.x());
    if (c == 0) {
        addEventTypeMenu(&popup,false);
        popup.addSeparator();
    }
    addGroupMenu(&popup);
    popup.addSeparator();
    addGoMenu(&popup);

    QPoint headerSize = QPoint(0, groupList->header()->height());
    popup.exec(groupList->mapToGlobal(p + headerSize));
}

void FunctionSelection::addGroupAction(QMenu* m,
				       ProfileContext::Type v,
				       const QString& s)
{
    QAction* a;
    if (s.isEmpty())
	a = m->addAction(ProfileContext::i18nTypeName(v));
    else
	a = m->addAction(s);
    a->setData((int)v);
    a->setCheckable(true);
    a->setChecked(_groupType == v);
}

void FunctionSelection::addGroupMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(tr("Grouping"));

    if (_groupType != ProfileContext::Function) {
	addGroupAction(m,  ProfileContext::Function, tr("No Grouping"));
	m->addSeparator();
    }
    addGroupAction(m, ProfileContext::Object);
    addGroupAction(m, ProfileContext::File);
    addGroupAction(m, ProfileContext::Class);
    addGroupAction(m, ProfileContext::FunctionCycle);

    connect(m, SIGNAL(triggered(QAction*)),
	  this, SLOT(groupTypeSelected(QAction*)));
}    


void FunctionSelection::groupTypeSelected(QAction* a)
{
    selectedGroupType( (ProfileContext::Type) a->data().toInt() );
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


void FunctionSelection::selectFunction(TraceFunction* f,
                                       bool ensureVisible)
{
    QModelIndex i = functionListModel->indexForFunction(f, true);
    if (!i.isValid()) return;

    if (ensureVisible)
        functionList->scrollTo(i, QAbstractItemView::EnsureVisible);

    _inSetFunction = true;
    QModelIndex last = functionListModel->index(i.row(), 4);
    QItemSelection s(i, last);
    functionList->selectionModel()->select(s, QItemSelectionModel::ClearAndSelect);
    _inSetFunction = false;
}

void FunctionSelection::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) return;

    // we do not show cost 2 at all...
    if (changeType == eventType2Changed) return;

    if (changeType == eventTypeChanged) {
        int i;
        groupList->header()->setResizeMode(0, QHeaderView::ResizeToContents);
        // need to disable sorting! Otherwise each change of shown cost
        // reorders list and changes order returned by topLevelItem()
        groupList->setSortingEnabled(false);
        for (i=0; i<groupList->topLevelItemCount(); i++) {
            CostListItem* item = (CostListItem*) groupList->topLevelItem(i);
            item->setEventType(_eventType);
        }
        groupList->header()->setResizeMode(0, QHeaderView::Interactive);
        groupList->setSortingEnabled(true);
        groupList->header()->setSortIndicatorShown(false);

        functionListModel->setEventType(_eventType);
        // previous line resets the model: reselect active item
        selectFunction(dynamic_cast<TraceFunction*>(_activeItem));
        setCostColumnWidths();
        return;
    }

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
        QModelIndex i = functionList->currentIndex();
        if (functionListModel->function(i) == f) {
	    return;	    
	}

        // reset search (as not activated from this view)
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

        selectFunction(f);
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

  QTreeWidgetItem* item = 0;
  int i;
  for (i=0; i < groupList->topLevelItemCount(); i++) {
      item = groupList->topLevelItem(i);
      if (((CostListItem*)item)->costItem() == g)
          break;
  }

  if (item) {
    groupList->scrollToItem(item);
    // prohibit signalling of a group selection
    _inSetGroup = true;
    _group = 0;
    groupList->setCurrentItem(item);
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
    groupList->headerItem()->setText(1, ProfileContext::i18nTypeName(_groupType));

    functionListModel->setMaxCount(GlobalConfig::maxListCount());

    if (!_data || _data->parts().count()==0) {
        functionListModel->resetModelData(0, 0, QString(), 0);
        selectTopFunction();
	return;
    }


  TraceObjectMap::Iterator oit;
  TraceClassMap::Iterator cit;
  TraceFileMap::Iterator fit;

  // Fill up group list.
  // Always show group of current function, even if cost below low limit.
  //

  _hc.clear(GlobalConfig::maxListCount());

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
      foreach(TraceCostItem *group, _data->functionCycles())
	_hc.addCost(group, group->subCost(_eventType));
    }

  break;

  default:
    {
      _group = 0;
      functionListModel->resetModelData(_data, _group, _searchString, _eventType);
      selectFunction(dynamic_cast<TraceFunction*>(_activeItem));
      setCostColumnWidths();
      return;
    }
  }

  // update group from _activeItem if possible
  if (_activeItem && (_activeItem->type() == _groupType))
    _group = (TraceCostItem*) _activeItem;

  QTreeWidgetItem *item = 0, *activeItem = 0;
  QList<QTreeWidgetItem*> items;

  // we always put group of active item in list, even if
  // it would be skipped because of small costs
  if (_group) {
    activeItem = new CostListItem(groupList, _group, _eventType);
    items.append(activeItem);
  }

  for(int i=0; i<_hc.realCount(); i++) {
    TraceCostItem *group = (TraceCostItem*)_hc[i];
    // do not put group of active item twice into list
    if (group == _group) continue;
    item = new CostListItem(groupList, group, _eventType);
    items.append(item);
  }
  if (_hc.hasMore()) {
      // a placeholder for all the cost items skipped ...
      item = new CostListItem(groupList, _hc.count() - _hc.maxSize(),
                              (TraceCostItem*)_hc[_hc.maxSize()-1], _eventType);
      items.append(item);
  }

  groupList->header()->setResizeMode(0, QHeaderView::ResizeToContents);
  groupList->setSortingEnabled(false);
  groupList->addTopLevelItems(items);
  groupList->setSortingEnabled(true);
  // always reset to cost sorting
  groupList->sortByColumn(0, Qt::DescendingOrder);
  groupList->header()->setSortIndicatorShown(false);
  groupList->header()->setResizeMode(0, QHeaderView::Interactive);

  if (activeItem) {
    groupList->scrollToItem(activeItem);
    _inSetGroup = true;
    _group = 0;
    groupList->setCurrentItem(activeItem);
    _inSetGroup = false;
  }
  else
    groupList->clearSelection();
}


void FunctionSelection::groupSelected(QTreeWidgetItem* i, QTreeWidgetItem*)
{
  if (!i) return;
  if (!_data) return;

  TraceCostItem* g = ((CostListItem*) i)->costItem();
  if (!g) return;
  if (g == _group) return;
  _group = g;

  functionListModel->resetModelData(_data, g, _searchString, _eventType);
  selectFunction(dynamic_cast<TraceFunction*>(_activeItem));
  setCostColumnWidths();

  // Do not emit signal if cost item was changed programatically
  if (!_inSetGroup) {
      if (_topLevel)
	  _topLevel->setGroupDelayed(g);
  }
}

void FunctionSelection::groupDoubleClicked(QTreeWidgetItem* i, int)
{
  if (!i) return;
  if (!_data) return;
  TraceCostItem* g = ((CostListItem*) i)->costItem();

  if (!g) return;
  // group must be selected first
  if (g != _group) return;

  activated(g);
}

void FunctionSelection::groupHeaderClicked(int col)
{
    groupList->sortByColumn(col, Qt::DescendingOrder);
}

TraceCostItem* FunctionSelection::group(QString s)
{
    int i;
    for (i=0; i<groupList->topLevelItemCount(); i++) {
        CostListItem* item = (CostListItem*) groupList->topLevelItem(i);
        if (item->costItem()->name() == s)
          return item->costItem();
    }

    return 0;
}


void FunctionSelection::functionActivated(const QModelIndex& i)
{
  if (!_data) return;

  TraceFunction* f = functionListModel->function(i);
  if (!f) return;

  if (!_inSetFunction)
    activated(f);
}

void FunctionSelection::updateGroupSizes(bool hideEmpty)
{
    int i;
    for (i=0; i<groupList->topLevelItemCount(); i++) {
        CostListItem* item = (CostListItem*) groupList->topLevelItem(i);
        int size = (_groupSize.contains(item->costItem())) ?
                   _groupSize[item->costItem()] : -1;
        item->setSize(size);
        item->setHidden(hideEmpty && (size<0));
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

  QRegExp re(query, Qt::CaseInsensitive, QRegExp::Wildcard);
  _groupSize.clear();

  TraceFunction* f = 0;
  TraceFunctionList list2;

  _hc.clear(GlobalConfig::maxListCount());

  TraceFunctionMap::Iterator it;
  for ( it = _data->functionMap().begin();
	it != _data->functionMap().end(); ++it ) {
    f = &(*it);
    if (re.indexIn(f->prettyName())>=0) {
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

  functionListModel->resetModelData(_data, _group, _searchString, _eventType);
  selectFunction(dynamic_cast<TraceFunction*>(_activeItem));
  setCostColumnWidths();
}

bool FunctionSelection::selectTopFunction()
{
    QModelIndex i = functionListModel->index(0,0);
    TraceFunction* f = functionListModel->function(i);

    // pre-select before activation to not trigger a refresh of this view
    _activeItem = f;
    selectFunction(f);

    functionActivated(i);

    return (f!=0);
}

void FunctionSelection::setCostColumnWidths()
{
    functionList->resizeColumnToContents(1);

    if (_eventType && (_eventType->subCost(_data->callMax())>0) ) {
        functionList->resizeColumnToContents(0);
        functionList->resizeColumnToContents(2);
    }
    else {
        functionList->header()->resizeSection(0, 0);
        functionList->header()->resizeSection(2, 0);
    }
}

void FunctionSelection::functionHeaderClicked(int col)
{
    if ((_functionListSortOrder== Qt::AscendingOrder) || (col<3))
        _functionListSortOrder = Qt::DescendingOrder;
    else
        _functionListSortOrder = Qt::AscendingOrder;

    functionList->sortByColumn(col, _functionListSortOrder);
    selectFunction(dynamic_cast<TraceFunction*>(_activeItem), false);
    setCostColumnWidths();
}



#include "functionselection.moc"
