/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Part View
 */


#include "partview.h"

#include <QAction>
#include <QMenu>
#include <QTreeWidget>
#include <QHeaderView>
#include <QKeyEvent>

#include "partlistitem.h"
#include "toplevelbase.h"


//
// PartView
//


PartView::PartView(TraceItemView* parentView, QWidget* parent)
    : QTreeWidget(parent), TraceItemView(parentView)
{
    _inSelectionUpdate = false;

    QStringList headerLabels;
    headerLabels << tr( "Profile Part" )
                 << tr( "Incl." )
                 << tr( "Self" )
                 << tr( "Called" )
                 << tr( "Comment" );
    setHeaderLabels(headerLabels);

    setAllColumnsShowFocus(true);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    // sorting will be enabled after refresh()
    sortByColumn(0, Qt::DescendingOrder);
    setMinimumHeight(50);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect( this, &QTreeWidget::itemSelectionChanged,
             this, &PartView::selectionChangedSlot );

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect( this,
             &QWidget::customContextMenuRequested,
             this, &PartView::context);

    connect(header(), &QHeaderView::sectionClicked,
            this, &PartView::headerClicked);

    setWhatsThis( whatsThis() );
}

QString PartView::whatsThis() const
{
    return tr( "<b>Trace Part List</b>"
               "<p>This list shows all trace parts of the loaded "
               "trace. For each part, the "
               "self/inclusive cost of the current selected "
               "function, spent in the part, is shown; "
               "percentage costs are always relative to the "
               "total cost <em>of the part</em> (not to the whole "
               "trace as in the Trace Part Overview). "
               "Also shown are the calls happening to/from the "
               "current function inside of the trace part.</p>"
               "<p>By choosing one or more trace parts from the "
               "list, the costs shown all over KCachegrind will "
               "only be the ones spent in the selected part(s). "
               "If no list selection is shown, in fact all trace "
               "parts are selected implicitly.</p>"
               "<p>This is a multi-selection list. You can select "
               "ranges by dragging the mouse or use SHIFT/CTRL "
               "modifiers. "
               "Selection/Deselection of trace parts can also be "
               "done by using the Trace Part Overview Dockable. "
               "This one also supports multiple selection.</p>"
               "<p>Note that the list is hidden if only one trace "
               "part is loaded.</p>");
}


void PartView::context(const QPoint & p)
{
    QMenu popup;
    addGoMenu(&popup);

    // p is in local coordinates
    popup.exec(mapToGlobal(p + QPoint(0,header()->height())));
}


void PartView::selectionChangedSlot()
{
    if (_inSelectionUpdate) return;

    TracePartList l;
    QList<QTreeWidgetItem*> sItems = selectedItems();
    foreach(QTreeWidgetItem* item, sItems)
        l.append( ((PartListItem*)item)->part() );

    // nothing selected means all
    if (l.isEmpty()) l = _data->parts();

    partsSelected(l);
}

void PartView::headerClicked(int col)
{
    // name columns should be sortable in both ways
    if ((col == 0) || (col == 4)) return;

    // all others only descending
    sortByColumn(col, Qt::DescendingOrder);
}

CostItem* PartView::canShow(CostItem* i)
{
    if (!TraceItemView::data()) return nullptr;
    if (TraceItemView::data()->parts().count()>1) return i;
    return nullptr;
}

void PartView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == eventType2Changed) return;
    if (changeType == selectedItemChanged) return;

    if (changeType == groupTypeChanged) {
        QTreeWidgetItem *item;
        for (int i=0; i<topLevelItemCount(); i++){
            item = topLevelItem(i);
            ((PartListItem*)item)->setGroupType(_groupType);
        }
        return;
    }

    if (changeType == eventTypeChanged) {
#if QT_VERSION >= 0x050000
        header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
#else
        header()->setResizeMode(1, QHeaderView::ResizeToContents);
        header()->setResizeMode(2, QHeaderView::ResizeToContents);
#endif
        // need to disable sorting! Otherwise each change of shown cost
        // can reorders list and change order returned by topLevelItem()
        setSortingEnabled(false);
        for (int i=0; i< topLevelItemCount(); i++) {
            PartListItem* item = (PartListItem*) topLevelItem(i);
            item->setEventType(_eventType);
        }
#if QT_VERSION >= 0x050000
        header()->setSectionResizeMode(1, QHeaderView::Interactive);
        header()->setSectionResizeMode(2, QHeaderView::Interactive);
#else
        header()->setResizeMode(1, QHeaderView::Interactive);
        header()->setResizeMode(2, QHeaderView::Interactive);
#endif
        setSortingEnabled(true);
        header()->setSortIndicatorShown(false);

        return;
    }

    if (changeType == partsChanged) {

        TracePart* part;

        _inSelectionUpdate = true;
        for (int i=0; i< topLevelItemCount(); i++) {
            PartListItem* item = (PartListItem*) topLevelItem(i);
            part = ((PartListItem*)item)->part();

            if (_partList.contains(part)) {
                item->setSelected(true);
                scrollToItem(item);
            }
            else
                item->setSelected(false);
        }
        _inSelectionUpdate = false;

        return;
    }

    refresh();
}

void PartView::refresh()
{
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);

    if (!_data || !_activeItem) {
        clear();
        return;
    }

    ProfileContext::Type t = _activeItem->type();
    TraceFunction* f = nullptr;
    if (t == ProfileContext::Function) f = (TraceFunction*) _activeItem;
    if (!f) return;

    TracePartList hidden;
    if (_topLevel)
        hidden = _topLevel->hiddenParts();

    _inSelectionUpdate = true;
    clear();

    QList<QTreeWidgetItem*> items;
    QTreeWidgetItem* item;
    foreach(TracePart* part, _data->parts()) {
        if (hidden.contains(part)) continue;
        item = new PartListItem(nullptr, f, _eventType, _groupType, part);
        items.append(item);
    }
    setSortingEnabled(false);
    addTopLevelItems(items);
    setSortingEnabled(true);
    header()->setSortIndicatorShown(false);
    header()->resizeSections(QHeaderView::ResizeToContents);

    foreach(item, items) {
        TracePart* part = ((PartListItem*)item)->part();
        if (hidden.contains(part)) continue;
        if (part->isActive()) {
            item->setSelected(true);
            scrollToItem(item);
        }
    }

    _inSelectionUpdate = false;
}

#include "moc_partview.cpp"
