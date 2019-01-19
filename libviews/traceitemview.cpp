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
 * Trace Item View
 */

#include "traceitemview.h"

#include <QtGlobal>
#include <QWidget>

#include "toplevelbase.h"

#define TRACE_UPDATES 0


// TraceItemViewUpdateTimer

TraceItemViewUpdateTimer::TraceItemViewUpdateTimer(TraceItemView* view)
{
    _view = view;
    setSingleShot(true);
    connect(this, &QTimer::timeout, this, &TraceItemViewUpdateTimer::timeoutTriggered);
}

void TraceItemViewUpdateTimer::timeoutTriggered()
{
    _view->triggerUpdate(false);
}

// TraceItemView

TraceItemView::TraceItemView(TraceItemView* parentView, TopLevelBase* top)
{
    _parentView = parentView;
    _topLevel = top ? top : parentView->topLevel();

    _data = _newData = nullptr;
    // _partList and _newPartList is empty
    _activeItem = _newActiveItem = nullptr;
    _selectedItem = _newSelectedItem = nullptr;
    _eventType = _newEventType = nullptr;
    _eventType2 = _newEventType2 = nullptr;
    _groupType = _newGroupType = ProfileContext::InvalidType;

    _status = nothingChanged;
    _needsUpdate = false;
    _pos = Hidden;

    _mergeUpdates = true;
    _updateTimer = new TraceItemViewUpdateTimer(this);
}

TraceItemView::~TraceItemView()
{
    delete _updateTimer;
}

QString TraceItemView::whatsThis() const
{
    return QObject::tr("No description available");
}

void TraceItemView::select(CostItem* i)
{
    if (_selectedItem == i) return;

    _newSelectedItem = i;
    updateView();
}

void TraceItemView::saveLayout(const QString&, const QString&)
{
#if 0
    qDebug("In '%s', saveLayout not implemented",
           widget()->name());
#endif
}

void TraceItemView::saveOptions(const QString&, const QString&)
{
#if 0
    qDebug("In '%s', saveOptions not implemented",
           widget()->name());
#endif
}

void TraceItemView::restoreLayout(const QString&, const QString&)
{}

void TraceItemView::restoreOptions(const QString&, const QString&)
{}


bool TraceItemView::activate(CostItem* i)
{
    _newActiveItem = canShow(i);
    if (_activeItem != _newActiveItem) {
        // new item activated, start with empty selection
        _newSelectedItem = nullptr;
        updateView();
    }

    return (_newActiveItem != nullptr);
}

TraceFunction* TraceItemView::activeFunction()
{
    if (!_activeItem) return nullptr;

    ProfileContext::Type t = _activeItem->type();
    switch(t) {
    case ProfileContext::Function:
    case ProfileContext::FunctionCycle:
        return (TraceFunction*) _activeItem;
    default:
        break;
    }
    return nullptr;
}

bool TraceItemView::set(int changeType, TraceData* d,
                        EventType* t1, EventType* t2,
                        ProfileContext::Type g, const TracePartList& l,
                        CostItem* a, CostItem* s)
{
    _status |= changeType;
    _newData = d;
    _newGroupType = g;
    _newEventType = t1;
    _newEventType2 = t2;
    _newPartList = l;
    _newSelectedItem = s;
    _newActiveItem = canShow(a);
    if (_activeItem != _newActiveItem) {
        // new item activated, start with empty selection
        _newSelectedItem = nullptr;
    }
    updateView();

    return (_newActiveItem != nullptr);
}


bool TraceItemView::isViewVisible()
{
    QWidget* w = widget();
    if (w)
        return w->isVisible();
    return false;
}

void TraceItemView::setData(TraceData* d)
{
    if (_data == d) return;
    _newData = d;

    // invalidate all pointers to old data
    _activeItem = _newActiveItem = nullptr;
    _selectedItem = _newSelectedItem = nullptr;
    _eventType = _newEventType = nullptr;
    _eventType2 = _newEventType2 = nullptr;
    _groupType = _newGroupType = ProfileContext::InvalidType;
    _partList.clear();
    _newPartList.clear();

    // updateView will change this to dataChanged
    _status = nothingChanged;
    updateView();
}

// force: update immediately even if invisible and no change was detected
void TraceItemView::updateView(bool force)
{
    if (!_mergeUpdates || force) {
        _needsUpdate = true;
        _updateTimer->stop();
        triggerUpdate(force);
        return;
    }

    // if _needsUpdate is true, an update is already scheduled
    if (_needsUpdate) return;

    _needsUpdate = true;
    _updateTimer->start(1);
}


void TraceItemView::triggerUpdate(bool force)
{
    // If already updated no need to update again.
    // This can happen if a update by timer is scheduled, but a "force"d
    // update is done before the timer triggers
    if (!_needsUpdate) return;
    _needsUpdate = false;

    if (!force) {
        if (!isViewVisible()) return;
    }

    if (_newData != _data) {
        _status |= dataChanged;
        _data = _newData;
    }
    else {
        _status &= ~dataChanged;

        // if there is no data change and data is 0, no update needed
        if (!_data) return;
    }

    if (!(_newPartList == _partList)) {
        _status |= partsChanged;
        _partList = _newPartList;
    }
    else
        _status &= ~partsChanged;

    if (_newActiveItem != _activeItem) {

        // when setting a new active item, there is no selection
        _selectedItem = nullptr;

        _status |= activeItemChanged;
        _activeItem = _newActiveItem;
    }
    else
        _status &= ~activeItemChanged;

    if (_newEventType != _eventType) {
        _status |= eventTypeChanged;
        _eventType = _newEventType;
    }
    else
        _status &= ~eventTypeChanged;

    if (_newEventType2 != _eventType2) {
        _status |= eventType2Changed;
        _eventType2 = _newEventType2;
    }
    else
        _status &= ~eventType2Changed;

    if (_newGroupType != _groupType) {
        _status |= groupTypeChanged;
        _groupType = _newGroupType;
    }
    else
        _status &= ~groupTypeChanged;


    if (_newSelectedItem != _selectedItem) {
        _status |= selectedItemChanged;
        _selectedItem = _newSelectedItem;
    }
    else
        _status &= ~selectedItemChanged;


    if (!force && (_status == nothingChanged)) return;

#if TRACE_UPDATES
    qDebug("%s::doUpate",
           widget() ? qPrintable(widget()->objectName()) : "TraceItemView");

    if (_status & dataChanged)
        qDebug("  data changed");

    if (_status & configChanged)
        qDebug("  config changed");

    if (_status & partsChanged)
        qDebug("  parts changed: %s", qPrintable(_partList[0]->name()) );

    if (_status & eventTypeChanged)
        qDebug("  event type 1 changed: %s",
               _eventType ? qPrintable( _eventType->name() ) : "(None)");

    if (_status & eventType2Changed)
        qDebug("  event type 2 changed: %s",
               _eventType2 ? qPrintable( _eventType2->name() ) : "(None)");

    if (_status & groupTypeChanged)
        qDebug("  group type changed: %s",
               qPrintable(ProfileContext::typeName(_groupType)));

    if (_status & activeItemChanged)
        qDebug("  active item changed: %s",
               _activeItem ? qPrintable( _activeItem->fullName() ) : "(none)");

    if (_status & selectedItemChanged)
        qDebug("  selected item changed: %s",
               _selectedItem ? qPrintable( _selectedItem->fullName() ) : "(none)");
#endif

    int st = _status;
    _status = nothingChanged;
    doUpdate(st, force);
}


void TraceItemView::selected(TraceItemView* /*sender*/, CostItem* i)
{
#if 0 // TRACE_UPDATES
    qDebug("%s::selected( item %s, sender %s )",
           widget() ? qPrintable(widget()->objectName()) : "TraceItemView",
           i ? qPrintable( i->name() ): "(none)",
           qPrintable(sender->widget()->objectName()) );
#endif

    if (_parentView) _parentView->selected(this, i);
}

void TraceItemView::partsSelected(TraceItemView* /*sender*/, const TracePartList& l)
{
#if 0 // TRACE_UPDATES
    qDebug() << (widget() ? widget()->name() : "TraceItemView")
             << "::selected "
             << l.names()
             << ", sender "
             << sender->widget()->name();
#endif

    if (_parentView)
        _parentView->partsSelected(this, l);
    else
        if (_topLevel) _topLevel->activePartsChangedSlot(l);
}

void TraceItemView::activated(TraceItemView* /*sender*/, CostItem* i)
{
#if 0 // TRACE_UPDATES
    qDebug("%s::activated( item %s, sender %s )",
           widget() ? qPrintable(widget()->objectName()) : "TraceItemView",
           i ? qPrintable( i->name() ): "(none)",
           qPrintable(sender->widget()->objectName()) );
#endif

    if (_parentView)
        _parentView->activated(this, i);
    else
        if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedEventType(TraceItemView*, EventType* t)
{
    if (_parentView)
        _parentView->selectedEventType(this, t);
    else
        if (_topLevel) _topLevel->setEventTypeDelayed(t);
}

void TraceItemView::selectedEventType2(TraceItemView*, EventType* t)
{
    if (_parentView)
        _parentView->selectedEventType2(this, t);
    else
        if (_topLevel) _topLevel->setEventType2Delayed(t);
}

void TraceItemView::selectedGroupType(TraceItemView*, ProfileContext::Type t)
{
    if (_parentView)
        _parentView->selectedGroupType(this, t);
    else
        if (_topLevel) _topLevel->setGroupTypeDelayed(t);
}


void TraceItemView::directionActivated(TraceItemView*, TraceItemView::Direction d)
{
    if (_parentView)
        _parentView->directionActivated(this, d);
    else
        if (_topLevel) _topLevel->setDirectionDelayed(d);
}

void TraceItemView::doUpdate(int, bool)
{
}

void TraceItemView::selected(CostItem* i)
{
#if TRACE_UPDATES
    qDebug("%s::selected( item %s )",
           widget() ? qPrintable(widget()->objectName()) : "TraceItemView",
           i ? qPrintable( i->name() ): "(none)" );
#endif

    if (_parentView)
        _parentView->selected(this, i);

}

void TraceItemView::partsSelected(const TracePartList& l)
{
    if (_parentView)
        _parentView->partsSelected(this, l);
    else
        if (_topLevel) _topLevel->activePartsChangedSlot(l);
}

void TraceItemView::activated(CostItem* i)
{
#if TRACE_UPDATES
    qDebug("%s::activated( item %s )",
           widget() ? qPrintable(widget()->objectName()) : "TraceItemView",
           i ? qPrintable( i->name() ): "(none)" );
#endif

    if (_parentView)
        _parentView->activated(this, i);
    else
        if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedEventType(EventType* t)
{
    if (_parentView)
        _parentView->selectedEventType(this, t);
    else
        if (_topLevel) _topLevel->setEventTypeDelayed(t);
}

void TraceItemView::selectedEventType2(EventType* t)
{
    if (_parentView)
        _parentView->selectedEventType2(this, t);
    else
        if (_topLevel) _topLevel->setEventType2Delayed(t);
}

void TraceItemView::selectedGroupType(ProfileContext::Type t)
{
    if (_parentView)
        _parentView->selectedGroupType(this, t);
    else
        if (_topLevel) _topLevel->setGroupTypeDelayed(t);
}

void TraceItemView::directionActivated(TraceItemView::Direction d)
{
    if (_parentView)
        _parentView->directionActivated(this, d);
    else
        if (_topLevel) _topLevel->setDirectionDelayed(d);
}

void TraceItemView::addEventTypeMenu(QMenu* p, bool withCost2)
{
    if (_topLevel) _topLevel->addEventTypeMenu(p, withCost2);
}

void TraceItemView::addGoMenu(QMenu* p)
{
    if (_topLevel) _topLevel->addGoMenu(p);
}

