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
 * Cost Type View
 */

#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <klocale.h>

#include "configuration.h"
#include "costtypeitem.h"
#include "costtypeview.h"
#include "toplevel.h"


//
// CostTypeView
//


CostTypeView::CostTypeView(TraceItemView* parentView,
			   QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
    addColumn( i18n( "Event Type" ) );
    addColumn( i18n( "Incl." ) );
    addColumn( i18n( "Self" ) );
    addColumn( i18n( "Short" ) );
    addColumn( QString::null );
    addColumn( i18n( "Formula" ) );

    setSorting(-1);
    setAllColumnsShowFocus(true);
    setColumnAlignment(1, Qt::AlignRight);
    setColumnAlignment(2, Qt::AlignRight);
    setColumnAlignment(3, Qt::AlignRight);
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

    connect(this,
	    SIGNAL(itemRenamed(QListViewItem*,int,const QString&)),
	    SLOT(renamedSlot(QListViewItem*,int,const QString&)));

    QWhatsThis::add( this, whatsThis() );
}

QString CostTypeView::whatsThis() const
{
    return i18n( "<b>Cost Types List</b>"
		 "<p>This list shows all cost types available "
		 "and what the self/inclusive cost of the "
		 "current selected function is for that cost type.</p>"
		 "<p>By choosing a cost type from the list, "
		 "you change the cost type of costs shown "
		 "all over KCachegrind to be the selected one.</p>");
}


void CostTypeView::context(QListViewItem* i, const QPoint & p, int)
{
  QPopupMenu popup;

  TraceCostType* ct = i ? ((CostTypeItem*) i)->costType() : 0;

  if (ct)
    popup.insertItem(i18n("Set Secondary Event Type"), 99);
  if (_costType2)
    popup.insertItem(i18n("Remove Secondary Event Type"), 98);
  if (popup.count()>0)
    popup.insertSeparator();

  if (ct && !ct->isReal()) {
      popup.insertItem(i18n("Edit Long Name"), 93);
      popup.insertItem(i18n("Edit Short Name"), 94);
      popup.insertItem(i18n("Edit Formula"), 95);
      popup.insertItem(i18n("Remove"), 96);
      popup.insertSeparator();
  }

  addGoMenu(&popup);

  popup.insertSeparator();
  popup.insertItem(i18n("New Cost Type ..."), 97);

  int r = popup.exec(p);
  if (r == 98) selectedCostType2(0);
  else if (r == 99) selectedCostType2(ct);
  else if (r == 93) i->startRename(0);
  else if (r == 94) i->startRename(3);
  else if (r == 95) i->startRename(5);
  else if (r == 96) {

    // search for a previous type 
    TraceCostType* prev = 0, *ct = 0;
    TraceCostMapping* m = _data->mapping();
    for (int i=0;i<m->realCount();i++) {
	ct = m->realType(i);
	if (ct) prev = ct;
    }
    for (int i=0;i<m->virtualCount();i++) {
	ct = m->virtualType(i);
	if (ct == _costType) break;
	if (ct) prev = ct;
    }

    if (_data->mapping()->remove(ct)) {
      // select previous cost type
      selectedCostType(prev);
      if (_costType2 == ct)
	selectedCostType2(prev);
      refresh();
    }
  }
  else if (r == 97) {
    int i = 1;
    while(1) {
      if (!TraceCostType::knownVirtualType(i18n("New%1").arg(i)))
	break;
      i++;
    }
    // add same new cost type to this mapping and to known types
    QString shortName = i18n("New%1").arg(i);
    QString longName  = i18n("New Cost Type %1").arg(i);
    TraceCostType::add(new TraceCostType(shortName, longName, "0"));
    _data->mapping()->add(new TraceCostType(shortName, longName, "0"));
    refresh();
  }
}

void CostTypeView::selectedSlot(QListViewItem * i)
{
    TraceCostType* ct = i ? ((CostTypeItem*) i)->costType() : 0;
    if (ct)
	selectedCostType(ct);
}

void CostTypeView::activatedSlot(QListViewItem * i)
{
  TraceCostType* ct = i ? ((CostTypeItem*) i)->costType() : 0;
  if (ct)
      selectedCostType2(ct);
}

TraceItem* CostTypeView::canShow(TraceItem* i)
{
    if (!i) return 0;

    switch(i->type()) {
    case TraceCost::Object:
    case TraceCost::Class:
    case TraceCost::File:
    case TraceCost::Call:
    case TraceCost::FunctionCycle:
    case TraceCost::Function:
	break;
    default:
	return 0;
    }
    return i;
}

void CostTypeView::doUpdate(int changeType)
{
    // Special case ?
    if (changeType == selectedItemChanged) return;

    if (changeType == costType2Changed) return;

    if (changeType == groupTypeChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((CostTypeItem*)item)->setGroupType(_groupType);

	return;
    }

    if (changeType == costTypeChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    if ( ((CostTypeItem*)item)->costType() == _costType) {
		setSelected(item, true);
		ensureItemVisible(item);
		break;
	    }

	return;
    }

    if (changeType == partsChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((CostTypeItem*)item)->update();

	return;
    }


    refresh();
}

void CostTypeView::refresh()
{
    clear();
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);

    if (!_data || !_activeItem) return;
    switch(_activeItem->type()) {
    case TraceCost::Object:
    case TraceCost::Class:
    case TraceCost::File:
    case TraceCost::FunctionCycle:
    case TraceCost::Function:
	break;
    default:
	return;
    }
    TraceCostItem* c = (TraceCostItem*) _activeItem;

    TraceCostType* ct =0 ;
    QListViewItem* item = 0;
    QString sumStr, pureStr;
    QListViewItem* costItem=0;

    TraceCostMapping* m = _data->mapping();
    for (int i=m->virtualCount()-1;i>=0;i--) {
	ct = m->virtualType(i);
	if (!ct) continue;
	item = new CostTypeItem(this, c, ct, _groupType);
	if (ct == _costType) costItem = item;
    }
    for (int i=m->realCount()-1;i>=0;i--) {
	ct = m->realType(i);
	item = new CostTypeItem(this, c, ct, _groupType);
	if (ct == _costType) costItem = item;
    }

    if (costItem) {
	setSelected(costItem, true);
	ensureItemVisible(costItem);
    }

    if (item) setMinimumHeight(3*item->height());
}


void CostTypeView::renamedSlot(QListViewItem* item,int c,const QString& t)
{
  TraceCostType* ct = item ? ((CostTypeItem*) item)->costType() : 0;
  if (!ct || ct->isReal()) return;

  // search for matching known Type
  int knownCount = TraceCostType::knownTypeCount();
  TraceCostType* known = 0;
  for (int i=0; i<knownCount; i++) {
      known = TraceCostType::knownType(i);
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

#include "costtypeview.moc"
