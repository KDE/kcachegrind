/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Coverage Views
 */


#include "coverageview.h"

#include <QAction>
#include <QMenu>
#include <QHeaderView>
#include <QKeyEvent>

#include "globalconfig.h"
#include "coverageitem.h"
#include "coverage.h"


//
// CoverageView
//


CoverageView::CoverageView(bool showCallers, TraceItemView* parentView, QWidget* parent)
    : QTreeWidget(parent), TraceItemView(parentView)
{
    _showCallers = showCallers;

    QStringList labels;
    labels  << tr( "Incl." );
    if (_showCallers) {
        setColumnCount(4);
        labels  << tr( "Distance" ),
                labels  << tr( "Called" ),
                labels  << tr( "Caller" );
    }
    else {
        setColumnCount(5);
        labels  << tr( "Self" ),
                labels  << tr( "Distance" ),
                labels  << tr( "Calling" ),
                labels  << tr( "Callee" );
    }
    setHeaderLabels(labels);

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
             this, &CoverageView::selectedSlot );

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect( this,
             &QWidget::customContextMenuRequested,
             this, &CoverageView::context);

    connect(this,
            &QTreeWidget::itemDoubleClicked,
            this, &CoverageView::activatedSlot);

    connect(header(), &QHeaderView::sectionClicked,
            this, &CoverageView::headerClicked);
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

void CoverageView::context(const QPoint & p)
{
    int c = columnAt(p.x());
    QTreeWidgetItem* i = itemAt(p);
    QMenu popup;

    TraceFunction* f = nullptr;
    if (i) {
        f = _showCallers ?
                ((CallerCoverageItem*)i)->function() :
                ((CalleeCoverageItem*)i)->function();
    }

    QAction* activateFunctionAction = nullptr;
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

    QAction* a = popup.exec(mapToGlobal(p + QPoint(0,header()->height())));
    if (a == activateFunctionAction)
        TraceItemView::activated(f);
}

void CoverageView::selectedSlot(QTreeWidgetItem* i, QTreeWidgetItem*)
{
    TraceFunction* f = nullptr;
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

void CoverageView::activatedSlot(QTreeWidgetItem* i, int)
{
    TraceFunction* f = nullptr;
    if (i) {
        f = _showCallers ?
                ((CallerCoverageItem*)i)->function() :
                ((CalleeCoverageItem*)i)->function();
    }

    if (f) TraceItemView::activated(f);
}

void CoverageView::headerClicked(int col)
{
    // distance and name columns should be sortable in both ways
    if (_showCallers) {
        if ((col == 1) || (col==3)) return;
    }
    else {
        if ((col == 2) || (col==4)) return;
    }

    // all others only descending
    sortByColumn(col, Qt::DescendingOrder);
}

void CoverageView::keyPressEvent(QKeyEvent* event)
{
    QTreeWidgetItem *item = currentItem();
    if (item && ((event->key() == Qt::Key_Return) ||
                 (event->key() == Qt::Key_Space)))
    {
        TraceFunction* f;
        f = _showCallers ?
                ((CallerCoverageItem*)item)->function() :
                ((CalleeCoverageItem*)item)->function();
        TraceItemView::activated(f);
    }
    QTreeView::keyPressEvent(event);
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
    return nullptr;
}

void CoverageView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == selectedItemChanged) {

        if (!_selectedItem) {
            clearSelection();
            return;
        }

        TraceFunction* f = nullptr;
        QTreeWidgetItem* i = currentItem();
        if (i) {
            f = _showCallers ?
                    ((CallerCoverageItem*)i)->function() :
                    ((CalleeCoverageItem*)i)->function();
        }
        if (f == _selectedItem) return;

        QTreeWidgetItem *item;
        for (int i=0; i<topLevelItemCount(); i++) {
            item = this->topLevelItem(i);
            f = _showCallers ?
                    ((CallerCoverageItem*)item)->function() :
                    ((CalleeCoverageItem*)item)->function();
            if (f == _selectedItem) {
                scrollToItem(item);
                setCurrentItem(item);
                break;
            }
        }
        return;
    }

    if (changeType == groupTypeChanged) {
        QTreeWidgetItem *item;
        for (int i=0; i<topLevelItemCount();i++) {
            item = topLevelItem(i);
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

    if (!_data || !_activeItem) return;

    ProfileContext::Type t = _activeItem->type();
    TraceFunction* f = nullptr;
    if (t == ProfileContext::Function) f = (TraceFunction*) _activeItem;
    if (t == ProfileContext::FunctionCycle) f = (TraceFunction*) _activeItem;
    if (!f) return;



    _hc.clear(GlobalConfig::maxListCount());
    SubCost realSum = f->inclusive()->subCost(_eventType);

    TraceFunctionList l;
    if (_showCallers)
        l = Coverage::coverage(f, Coverage::Caller, _eventType);
    else
        l = Coverage::coverage(f, Coverage::Called, _eventType);

    foreach(TraceFunction* f2, l) {
        Coverage* c = (Coverage*) f2->association(Coverage::Rtti);
        if (c && (c->inclusive()>0.0))
            _hc.addCost(f2, SubCost(realSum * c->inclusive()));
    }

    QList<QTreeWidgetItem*> items;
    QTreeWidgetItem* item;
    TraceFunction* ff;
    for(int i=0;i<_hc.realCount();i++) {
        ff = (TraceFunction*) _hc[i];
        Coverage* c = (Coverage*) ff->association(Coverage::Rtti);
        if (_showCallers)
            item = new CallerCoverageItem(nullptr, c, f, _eventType, _groupType);
        else
            item = new CalleeCoverageItem(nullptr, c, f, _eventType, _groupType);
        items.append(item);
    }
    if (_hc.hasMore()) {
        // a placeholder for all the functions skipped ...
        ff = (TraceFunction*) _hc[_hc.maxSize()-1];
        Coverage* c = (Coverage*) ff->association(Coverage::Rtti);
        if (_showCallers)
            item = new CallerCoverageItem(nullptr, _hc.count() - _hc.maxSize(),
                                          c, f, _eventType, _groupType);
        else
            item = new CalleeCoverageItem(nullptr, _hc.count() - _hc.maxSize(),
                                          c, f, _eventType, _groupType);
        items.append(item);
    }

    // when inserting, switch off sorting for performance reason
    setSortingEnabled(false);
    addTopLevelItems(items);
    setSortingEnabled(true);
    // enabling sorting switches on the indicator, but we want it off
    header()->setSortIndicatorShown(false);
    // resize to content now (section size still can be interactively changed)
    header()->resizeSections(QHeaderView::ResizeToContents);

    if (_data->maxCallCount() == 0) {
        // hide call count
        setColumnWidth(_showCallers ? 2 : 3, 0);
    }
}

