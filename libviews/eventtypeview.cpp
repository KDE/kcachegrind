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
 * Event Type View
 */


#include "eventtypeview.h"

#include <QAction>
#include <QMenu>
#include <QHeaderView>

#include "eventtypeitem.h"
#include "toplevelbase.h"


//
// EventTypeView
//

EventTypeView::EventTypeView(TraceItemView* parentView,
			     QWidget* parent, const char* name)
  : QTreeWidget(parent), TraceItemView(parentView)
{
    setObjectName(name);
    // forbid scaling icon pixmaps to smaller size
    setIconSize(QSize(99,99));
    setColumnCount(6);
    QStringList labels;
    labels  << tr( "Event Type" )
	    << tr( "Incl." )
	    << tr( "Self" )
	    << tr( "Short" )
	    << QString()
	    << tr( "Formula" );
    setHeaderLabels(labels);
    // reduce minimum width for '=' column
    header()->setMinimumSectionSize(10);

    setRootIsDecorated(false);
    setSortingEnabled(false);
    setAllColumnsShowFocus(true);
    setMinimumHeight(50);


    setContextMenuPolicy(Qt::CustomContextMenu);
    connect( this,
	     SIGNAL(customContextMenuRequested(const QPoint &)),
	     SLOT(context(const QPoint &)));

    // FIXME: Endless jumping among 2 types possible!
    connect( this,
	     SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
	     SLOT(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) );

    connect(this,
	    SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
	    SLOT(itemDoubleClicked(QTreeWidgetItem*,int)));

    connect(this,
	    SIGNAL(itemChanged(QTreeWidgetItem*,int)),
	    SLOT(itemChanged(QTreeWidgetItem*,int)));

    setWhatsThis( whatsThis() );
}

QString EventTypeView::whatsThis() const
{
    return tr( "<b>Cost Types List</b>"
		 "<p>This list shows all cost types available "
		 "and what the self/inclusive cost of the "
		 "current selected function is for that cost type.</p>"
		 "<p>By choosing a cost type from the list, "
		 "you change the cost type of costs shown "
		 "all over KCachegrind to be the selected one.</p>");
}


void EventTypeView::context(const QPoint & p)
{
  QMenu popup;

  QTreeWidgetItem* i = itemAt(p);
  EventType* ct = i ? ((EventTypeItem*) i)->eventType() : 0;

  QAction* selectType2Action = 0;
  QAction* hideType2Action = 0;
  if (ct)
    selectType2Action = popup.addAction(tr("Set as Secondary Event Type"));
  if (_eventType2)
    hideType2Action = popup.addAction(tr("Hide Secondary Event Type"));
  if (!popup.isEmpty())
    popup.addSeparator();

  QAction* editLongNameAction = 0;
  QAction* editShortNameAction = 0;
  QAction* editFormulaAction = 0;
  QAction* removeTypeAction = 0;
  if (ct && !ct->isReal()) {
      editLongNameAction = popup.addAction(tr("Edit Long Name"));
      editShortNameAction = popup.addAction(tr("Edit Short Name"));
      editFormulaAction = popup.addAction(tr("Edit Formula"));
      removeTypeAction = popup.addAction(tr("Remove"));
      popup.addSeparator();
  }

  addGoMenu(&popup);

  QAction* newTypeAction = 0;
  if( _data) {
      popup.addSeparator();
      newTypeAction = popup.addAction(tr("New Event Type..."));
  }

  QAction* a = popup.exec(viewport()->mapToGlobal(p));
  if (a == hideType2Action) selectedEventType2(0);
  else if (a == selectType2Action) selectedEventType2(ct);
  else if (a == editLongNameAction) editItem(i, 0);
  else if (a == editShortNameAction) editItem(i, 3);
  else if (a == editFormulaAction) editItem(i, 5);
  else if (a == removeTypeAction) {

    // search for a previous type 
    EventType* prev = 0, *ct = 0;
    EventTypeSet* m = _data->eventTypes();
    for (int i=0;i<m->realCount();i++) {
	ct = m->realType(i);
	if (ct) prev = ct;
    }
    for (int i=0;i<m->derivedCount();i++) {
	ct = m->derivedType(i);
	if (ct == _eventType) break;
	if (ct) prev = ct;
    }

    if (_data->eventTypes()->remove(ct)) {
      // select previous cost type
      selectedEventType(prev);
      if (_eventType2 == ct)
	selectedEventType2(prev);
      refresh();
    }
  }
  else if (a == newTypeAction) {
    int i = 1;
    while(1) {
	if (!EventType::knownDerivedType(tr("New%1").arg(i)))
	break;
      i++;
    }
    // add same new cost type to this set and to known types
    QString shortName = tr("New%1").arg(i);
    QString longName  = tr("New Event Type %1").arg(i);
    EventType* et;
    et = new EventType(shortName, longName);
    et->setFormula(QString()); // event is derived
    EventType::add(et);
    // EventType::add() took ownership, need new object
    et = new EventType(shortName, longName);
    et->setFormula(QString()); // event is derived
    _data->eventTypes()->add(et);
    refresh();
  }
}


void EventTypeView::currentItemChanged(QTreeWidgetItem* i, QTreeWidgetItem*)
{
    EventType* ct = i ? ((EventTypeItem*) i)->eventType() : 0;
    if (ct)
	selectedEventType(ct);
}

void EventTypeView::itemDoubleClicked(QTreeWidgetItem* i, int)
{
  EventType* ct = i ? ((EventTypeItem*) i)->eventType() : 0;
  if (ct)
      selectedEventType2(ct);
}

CostItem* EventTypeView::canShow(CostItem* i)
{
    if (!i) return 0;

    switch(i->type()) {
    case ProfileContext::Object:
    case ProfileContext::Class:
    case ProfileContext::File:
    case ProfileContext::Call:
    case ProfileContext::FunctionCycle:
    case ProfileContext::Function:
	break;
    default:
	return 0;
    }
    return i;
}

void EventTypeView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) return;

    if (changeType == eventType2Changed) return;

    if (changeType == groupTypeChanged) {
	for(int i = 0; i < topLevelItemCount(); i++)
	    ((EventTypeItem*)topLevelItem(i))->setGroupType(_groupType);

	return;
    }

    if (changeType == eventTypeChanged) {
	for(int i = 0; i < topLevelItemCount(); i++) {
	    EventTypeItem* item = (EventTypeItem*)topLevelItem(i);
	    if ( item->eventType() == _eventType) {
		setCurrentItem(item);
		scrollToItem(item);
		break;
	    }
	}

	return;
    }

    if (changeType == partsChanged) {
	for(int i = 0; i < topLevelItemCount(); i++)
	    ((EventTypeItem*)topLevelItem(i))->update();

	return;
    }


    refresh();
}

void EventTypeView::refresh()
{
    clear();
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);

    if (!_data || !_activeItem) return;
    switch(_activeItem->type()) {
    case ProfileContext::Object:
    case ProfileContext::Class:
    case ProfileContext::File:
    case ProfileContext::FunctionCycle:
    case ProfileContext::Function:
	break;
    default:
	return;
    }
    TraceCostItem* c = (TraceCostItem*) _activeItem;

    EventType* ct =0;
    QTreeWidgetItem* item = 0;
    QTreeWidgetItem* selected = 0;
    QList<QTreeWidgetItem*> items;
    QString sumStr, pureStr;

    EventTypeSet* m = _data->eventTypes();
    for (int i=0; i<m->realCount();i++) {
	ct = m->realType(i);
	item = new EventTypeItem(c, ct, _groupType);
	if (ct == _eventType) selected = item;
	items.append(item);
    }
    for (int i=0; i<m->derivedCount();i++) {
	ct = m->derivedType(i);
	if (!ct) continue;
	item = new EventTypeItem(c, ct, _groupType);
	if (ct == _eventType) selected = item;
	items.append(item);
    }
    insertTopLevelItems(0,items);

    if (selected) {
	setCurrentItem(selected);
	scrollToItem(selected);
    }

    for(int c = 0; c<6; c++)
	resizeColumnToContents(c);
}

void EventTypeView::itemChanged(QTreeWidgetItem* item, int c)
{
  EventType* ct = item ? ((EventTypeItem*) item)->eventType() : 0;
  if (!ct || ct->isReal()) return;

  // search for matching known Type
  int knownCount = EventType::knownTypeCount();
  EventType* known = 0;
  for (int i=0; i<knownCount; i++) {
      known = EventType::knownType(i);
      if (known->name() == ct->name()) break;
  }

  QString t = item->text(c);
  if (c == 0) {
      ct->setLongName(t);
      if (known) known->setLongName(t);
  }
  else if (c == 3) {
      ct->setName(t);
      if (known) known->setName(t);
  }
  else if (c == 5) {
      ct->setFormula(t);
      if (known) known->setFormula(t);
  }
  else return;

  if (_topLevel) _topLevel->configChanged();
  refresh();
}

#include "eventtypeview.moc"
