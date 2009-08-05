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

#include <Qt3Support/Q3PopupMenu>

#include "eventtypeitem.h"
#include "toplevelbase.h"


//
// EventTypeView
//


EventTypeView::EventTypeView(TraceItemView* parentView,
			   QWidget* parent, const char* name)
  : Q3ListView(parent, name), TraceItemView(parentView)
{
    addColumn( tr( "Event Type" ) );
    addColumn( tr( "Incl." ) );
    addColumn( tr( "Self" ) );
    addColumn( tr( "Short" ) );
    addColumn( QString::null );	//krazy:exclude=nullstrassign for old broken gcc
    addColumn( tr( "Formula" ) );

    setSorting(-1);
    setAllColumnsShowFocus(true);
    setColumnAlignment(1, Qt::AlignRight);
    setColumnAlignment(2, Qt::AlignRight);
    setColumnAlignment(3, Qt::AlignRight);
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

    connect(this,
	    SIGNAL(itemRenamed(Q3ListViewItem*,int,const QString&)),
	    SLOT(renamedSlot(Q3ListViewItem*,int,const QString&)));

    this->setWhatsThis( whatsThis() );
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

void EventTypeView::context(Q3ListViewItem* i, const QPoint & p, int)
{
  Q3PopupMenu popup;

  EventType* ct = i ? ((EventTypeItem*) i)->eventType() : 0;

  if (ct)
    popup.insertItem(tr("Set Secondary Event Type"), 99);
  if (_eventType2)
    popup.insertItem(tr("Remove Secondary Event Type"), 98);
  if (popup.count()>0)
    popup.insertSeparator();

  if (ct && !ct->isReal()) {
      popup.insertItem(tr("Edit Long Name"), 93);
      popup.insertItem(tr("Edit Short Name"), 94);
      popup.insertItem(tr("Edit Formula"), 95);
      popup.insertItem(tr("Remove"), 96);
      popup.insertSeparator();
  }

  addGoMenu(&popup);

  if( _data) 
  {
    popup.insertSeparator();
    popup.insertItem(tr("New Event Type ..."), 97);
  }
  int r = popup.exec(p);
  if (r == 98) selectedEventType2(0);
  else if (r == 99) selectedEventType2(ct);
  else if (r == 93) i->startRename(0);
  else if (r == 94) i->startRename(3);
  else if (r == 95) i->startRename(5);
  else if (r == 96) {

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
  else if (r == 97) {
    int i = 1;
    while(1) {
	if (!EventType::knownDerivedType(tr("New%1").arg(i)))
	break;
      i++;
    }
    // add same new cost type to this set and to known types
    QString shortName = tr("New%1").arg(i);
    QString longName  = tr("New Event Type %1").arg(i);
    EventType::add(new EventType(shortName, longName, "0"));
    _data->eventTypes()->add(new EventType(shortName, longName, "0"));
    refresh();
  }
}

void EventTypeView::selectedSlot(Q3ListViewItem * i)
{
    EventType* ct = i ? ((EventTypeItem*) i)->eventType() : 0;
    if (ct)
	selectedEventType(ct);
}

void EventTypeView::activatedSlot(Q3ListViewItem * i)
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
	Q3ListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((EventTypeItem*)item)->setGroupType(_groupType);

	return;
    }

    if (changeType == eventTypeChanged) {
	Q3ListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    if ( ((EventTypeItem*)item)->eventType() == _eventType) {
		setSelected(item, true);
		ensureItemVisible(item);
		break;
	    }

	return;
    }

    if (changeType == partsChanged) {
	Q3ListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((EventTypeItem*)item)->update();

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

    EventType* ct =0 ;
    Q3ListViewItem* item = 0;
    QString sumStr, pureStr;
    Q3ListViewItem* costItem=0;

    EventTypeSet* m = _data->eventTypes();
    for (int i=m->derivedCount()-1;i>=0;i--) {
	ct = m->derivedType(i);
	if (!ct) continue;
	item = new EventTypeItem(this, c, ct, _groupType);
	if (ct == _eventType) costItem = item;
    }
    for (int i=m->realCount()-1;i>=0;i--) {
	ct = m->realType(i);
	item = new EventTypeItem(this, c, ct, _groupType);
	if (ct == _eventType) costItem = item;
    }

    if (costItem) {
	setSelected(costItem, true);
	ensureItemVisible(costItem);
    }

    if (item) setMinimumHeight(3*item->height());
}


void EventTypeView::renamedSlot(Q3ListViewItem* item,int c,const QString& t)
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
