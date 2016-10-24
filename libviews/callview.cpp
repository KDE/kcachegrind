/* This file is part of KCachegrind.
   Copyright (c) 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <QAction>
#include <QMenu>
#include <QTreeWidget>
#include <QHeaderView>
#include <QKeyEvent>

#include "globalconfig.h"
#include "callitem.h"


//
// CallView
//


CallView::CallView(bool showCallers, TraceItemView* parentView, QWidget* parent)
    : QTreeWidget(parent), TraceItemView(parentView)
{
    _showCallers = showCallers;

    QStringList headerLabels;
    headerLabels << tr( "Cost" )
                 << tr( "Cost per call" )
                 << tr( "Cost 2" )
                 << tr( "Cost 2 per call" )
                 << tr( "Count" )
                 << ((_showCallers) ? tr( "Caller" ) : tr( "Callee" ));
    setHeaderLabels(headerLabels);

    // forbid scaling icon pixmaps to smaller size
    setIconSize(QSize(99,99));
    setAllColumnsShowFocus(true);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    // sorting will be enabled after refresh()
    sortByColumn(0, Qt::DescendingOrder);
    setMinimumHeight(50);

    this->setWhatsThis( whatsThis() );

    connect( this,
             &QTreeWidget::currentItemChanged,
             this, &CallView::selectedSlot );

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect( this,
             &QWidget::customContextMenuRequested,
             this, &CallView::context);

    connect(this,
            &QTreeWidget::itemDoubleClicked,
            this, &CallView::activatedSlot);

    connect(header(), &QHeaderView::sectionClicked,
            this, &CallView::headerClicked);
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


void CallView::context(const QPoint & p)
{
  QMenu popup;

  // p is in local coordinates
  int col = columnAt(p.x());
  QTreeWidgetItem* i = itemAt(p);
  TraceCall* c = i ? ((CallItem*) i)->call() : 0;
  TraceFunction *f = 0, *cycle = 0;

  QAction* activateFunctionAction = 0;
  QAction* activateCycleAction = 0;
  if (c) {
    QString name  = _showCallers ? c->callerName(true) : c->calledName(true);
    f = _showCallers ? c->caller(true) : c->called(true);
    cycle = f->cycle();

    QString menuText = tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(name));
    activateFunctionAction = popup.addAction(menuText);

    if (cycle) {
	name = GlobalConfig::shortenSymbol(cycle->prettyName());
        QString menuText = tr("Go to '%1'").arg(name);
        activateCycleAction = popup.addAction(menuText);
    }

    popup.addSeparator();
  }

  // add menu items to select event type if column displays cost for a type
  if (col < 4) {
    addEventTypeMenu(&popup);
    popup.addSeparator();
  }
  addGoMenu(&popup);

  QAction* a = popup.exec(mapToGlobal(p + QPoint(0,header()->height())));
  if (a == activateFunctionAction)
      TraceItemView::activated(f);
  else if (a == activateCycleAction)
      TraceItemView::activated(cycle);
}

void CallView::selectedSlot(QTreeWidgetItem * i, QTreeWidgetItem *)
{
  if (!i) return;
  TraceCall* c = ((CallItem*) i)->call();
  // Should we skip cycles here?
  CostItem* f = _showCallers ? c->caller(false) : c->called(false);

  _selectedItem = f;
  selected(f);
}

void CallView::activatedSlot(QTreeWidgetItem* i,int)
{
  if (!i) return;

  TraceCall* c = ((CallItem*) i)->call();
  // skip cycles: use the context menu to get to the cycle...
  CostItem* f = _showCallers ? c->caller(true) : c->called(true);

  TraceItemView::activated(f);
}

void CallView::headerClicked(int col)
{
    // name columns should be sortable in both ways
    if (col == 5) return;

    // all others only descending
    sortByColumn(col, Qt::DescendingOrder);
}

void CallView::keyPressEvent(QKeyEvent* event)
{
    QTreeWidgetItem *item = currentItem();
    if (item && ((event->key() == Qt::Key_Return) ||
                 (event->key() == Qt::Key_Space)))
    {
        TraceCall* c = ((CallItem*) item)->call();
        CostItem* f = _showCallers ? c->caller(false) : c->called(false);

        TraceItemView::activated(f);
    }
    QTreeView::keyPressEvent(event);
}

CostItem* CallView::canShow(CostItem* i)
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

void CallView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) {

	if (!_selectedItem) {
	    clearSelection();
	    return;
	}

        CallItem* ci = (CallItem*) currentItem();
	TraceCall* c;
	CostItem* ti;
	if (ci) {
	    c = ci->call();
	    ti = _showCallers ? c->caller() : c->called();
	    if (ti == _selectedItem) return;
	}

        QTreeWidgetItem *item = 0;
        for (int i=0; i<topLevelItemCount();i++) {
            item = topLevelItem(i);
	    c = ((CallItem*) item)->call();
	    ti = _showCallers ? c->caller() : c->called();
	    if (ti == _selectedItem) {
                scrollToItem(item);
                setCurrentItem(item);
		break;
	    }
	}
        if (!item && ci) clearSelection();
	return;
    }
    
    if (changeType == groupTypeChanged) {
        QTreeWidgetItem *item;
        for (int i=0; i<topLevelItemCount(); i++){
            item = topLevelItem(i);
	    ((CallItem*)item)->updateGroup();
        }
	return;
    }

    refresh();
}

void CallView::refresh()
{
    clear();
    setColumnWidth(1, _eventType2 ? 50:0);

    if (_eventType) {
        headerItem()->setText(0, _eventType->name());
        headerItem()->setText(1, tr("%1 per call").arg(_eventType->name()));
    }
    if (_eventType2) {
        headerItem()->setText(2, _eventType2->name());
        headerItem()->setText(3, tr("%1 per call").arg(_eventType2->name()));
    }

    if (!_data || !_activeItem) return;

    TraceFunction* f = activeFunction();
    if (!f) return;

    // In the call lists, we skip cycles to show the real call relations
    TraceCallList l = _showCallers ? f->callers(true) : f->callings(true);

    QList<QTreeWidgetItem*> items;
    foreach(TraceCall* call, l)
	if (call->subCost(_eventType)>0)
            items.append(new CallItem(this, 0, call));

    // when inserting, switch off sorting for performance reason
    setSortingEnabled(false);
    addTopLevelItems(items);
    setSortingEnabled(true);
    // enabling sorting switches on the indicator, but we want it off
    header()->setSortIndicatorShown(false);
    // resize to content now (section size still can be interactively changed)
    setCostColumnWidths();
}

void CallView::setCostColumnWidths()
{
    // inclusive cost columns
    resizeColumnToContents(0);
    if (_eventType2)
        resizeColumnToContents(2);
    else
        setColumnWidth(2, 0);

    if (_data->maxCallCount() == 0) {
        // hide "per call" columns and call count column
        setColumnWidth(1, 0);
        setColumnWidth(3, 0);
        setColumnWidth(4, 0);
    }
    else {
        resizeColumnToContents(1);
        if (_eventType2)
            resizeColumnToContents(3);
        else
            setColumnWidth(3, 0);
        resizeColumnToContents(4);
    }
}
