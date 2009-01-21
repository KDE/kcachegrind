/* This file is part of KCachegrind.
   Copyright (C) 2003-2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <QWidget>

#include "toplevelbase.h"


#define TRACE_UPDATES 0

TraceItemView::TraceItemView(TraceItemView* parentView, TopLevelBase* top)
{
  _parentView = parentView;
  _topLevel = top ? top : parentView->topLevel();

  _data = _newData = 0;
  // _partList and _newPartList is empty
  _activeItem = _newActiveItem = 0;
  _selectedItem = _newSelectedItem = 0;
  _eventType = _newEventType = 0;
  _eventType2 = _newEventType2 = 0;
  _groupType = _newGroupType = TraceItem::NoCostType;

  _status = nothingChanged;
  _inUpdate = false;
  _pos = Hidden;
}

TraceItemView::~TraceItemView()
{}

QString TraceItemView::whatsThis() const
{
    return QObject::tr("No description available");
}

void TraceItemView::select(TraceItem* i)
{
    _newSelectedItem = i;
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


bool TraceItemView::activate(TraceItem* i)
{
    i = canShow(i);
    _newActiveItem = i;

    return (i != 0);
}

TraceFunction* TraceItemView::activeFunction()
{
    TraceItem::CostType t = _activeItem->type();
    switch(t) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
	return (TraceFunction*) _activeItem;
    default:
	break;
    }
    return 0;
}

bool TraceItemView::set(int changeType, TraceData* d,
			TraceEventType* t1, TraceEventType* t2,
			TraceItem::CostType g, const TracePartList& l,
                        TraceItem* a, TraceItem* s)
{
  _status |= changeType;
  _newData = d;
  _newGroupType = g;
  _newEventType = t1;
  _newEventType2 = t2;
  _newPartList = l;
  _newSelectedItem = s;
  _newActiveItem = canShow(a);
  if (!_newActiveItem) {
      _newSelectedItem = 0;
      return false;
  }

  return true;
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
  _newData = d;

  // invalidate all pointers to old data
  _activeItem = _newActiveItem = 0;
  _selectedItem = _newSelectedItem = 0;
  _eventType = _newEventType = 0;
  _eventType2 = _newEventType2 = 0;
  _groupType = _newGroupType = TraceItem::NoCostType;
  _partList.clear();
  _newPartList.clear();

  // updateView will change this to dataChanged
  _status = nothingChanged;
}

void TraceItemView::updateView(bool force)
{
  if (!force && !isViewVisible()) return;

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
      _selectedItem = 0;

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
  qDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::doUpdate ( "
            << ((_status & dataChanged) ? "data ":"")
            << ((_status & configChanged) ? "config ":"")
            << ")";

  if (_status & partsChanged)
    qDebug() << "  Part List "
	     << _partList.names();

  if (_status & eventTypeChanged)
    qDebug() << "  Cost type "
	     << (_eventType ? qPrintable( _eventType->name() ) : "?");

  if (_status & eventType2Changed)
    qDebug() << "  Cost type 2 "
	     << (_eventType2 ? qPrintable( _eventType2->name() ) : "?");

  if (_status & groupTypeChanged)
    qDebug() << "  Group type "
	     << TraceItem::typeName(_groupType);

  if (_status & activeItemChanged)
    qDebug() << "  Active: "
	     << (_activeItem ? qPrintable( _activeItem->fullName() ) : "?");

  if (_status & selectedItemChanged)
    qDebug() << "  Selected: "
	     << (_selectedItem ? qPrintable( _selectedItem->fullName() ) : "?");
#endif

  int st = _status;
  _status = nothingChanged;
  doUpdate(st);
  return;

  if (_inUpdate) return;
  _inUpdate = true;
  doUpdate(_status);
  _inUpdate = false;
}


void TraceItemView::selected(TraceItemView* /*sender*/, TraceItem* i)
{
#if TRACE_UPDATES
  qDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::selected "
            << (i ? qPrintable( i->name() ): "(nil)")
            << ", sender "
            << sender->widget()->name() << endl;
#endif

  if (_parentView) _parentView->selected(this, i);
}

void TraceItemView::partsSelected(TraceItemView* /*sender*/, const TracePartList& l)
{
#if TRACE_UPDATES
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

void TraceItemView::activated(TraceItemView* /*sender*/, TraceItem* i)
{
#if TRACE_UPDATES
  qDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::activated "
            << (i ? qPrintable( i->name() ) : "(nil)")
            << ", sender "
            << sender->widget()->name();
#endif

  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedEventType(TraceItemView*, TraceEventType* t)
{
  if (_parentView)
      _parentView->selectedEventType(this, t);
  else
      if (_topLevel) _topLevel->setEventTypeDelayed(t);
}

void TraceItemView::selectedEventType2(TraceItemView*, TraceEventType* t)
{
  if (_parentView)
      _parentView->selectedEventType2(this, t);
  else
      if (_topLevel) _topLevel->setEventType2Delayed(t);
}

void TraceItemView::selectedGroupType(TraceItemView*, TraceItem::CostType t)
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

void TraceItemView::doUpdate(int)
{
}

void TraceItemView::selected(TraceItem* i)
{
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

void TraceItemView::activated(TraceItem* i)
{
#if TRACE_UPDATES
  qDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::activated "
            << (i ? qPrintable( i->name() ): "(nil)");
#endif

  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedEventType(TraceEventType* t)
{
  if (_parentView)
      _parentView->selectedEventType(this, t);
  else
      if (_topLevel) _topLevel->setEventTypeDelayed(t);
}

void TraceItemView::selectedEventType2(TraceEventType* t)
{
  if (_parentView)
      _parentView->selectedEventType2(this, t);
  else
      if (_topLevel) _topLevel->setEventType2Delayed(t);
}

void TraceItemView::selectedGroupType(TraceItem::CostType t)
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
