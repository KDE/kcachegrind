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
 * Trace Item View
 */

#include <qwidget.h>
#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>

#include "traceitemview.h"
#include "toplevel.h"

#define TRACE_UPDATES 0

TraceItemView::TraceItemView(TraceItemView* parentView, TopLevel* top)
{
  _parentView = parentView;
  _topLevel = top ? top : parentView->topLevel();

  _data = _newData = 0;
  // _partList and _newPartList is empty
  _activeItem = _newActiveItem = 0;
  _selectedItem = _newSelectedItem = 0;
  _costType = _newCostType = 0;
  _costType2 = _newCostType2 = 0;
  _groupType = _newGroupType = TraceItem::NoCostType;

  _status = nothingChanged;
  _inUpdate = false;
  _pos = Hidden;
}

QString TraceItemView::whatsThis() const
{
    return i18n("No description available");
}

void TraceItemView::select(TraceItem* i)
{
    _newSelectedItem = i;
}

KConfigGroup* TraceItemView::configGroup(KConfig* c,
					 QString group, QString post)
{
    QStringList gList = c->groupList();
    if (gList.contains((group+post).ascii()) ) group += post;
    return new KConfigGroup(c, group);
}

void TraceItemView::writeConfigEntry(KConfigBase* c, const char* pKey,
				     QString value, const char* def, bool bNLS)
{
    if (!c) return;
    if ((value.isEmpty() && ((def == 0) || (*def == 0))) ||
	(value == QString(def)))
	c->deleteEntry(pKey);
    else
	c->writeEntry(pKey, value, true, false, bNLS);
}

void TraceItemView::writeConfigEntry(KConfigBase* c, const char* pKey,
				     int value, int def)
{
    if (!c) return;
    if (value == def)
	c->deleteEntry(pKey);
    else
	c->writeEntry(pKey, value);
}

void TraceItemView::writeConfigEntry(KConfigBase* c, const char* pKey,
				     double value, double def)
{
    if (!c) return;
    if (value == def)
	c->deleteEntry(pKey);
    else
	c->writeEntry(pKey, value);
}

void TraceItemView::writeConfigEntry(KConfigBase* c, const char* pKey,
				     bool value, bool def)
{
    if (!c) return;
    if (value == def)
	c->deleteEntry(pKey);
    else
	c->writeEntry(pKey, value);
}

void TraceItemView::readViewConfig(KConfig*, QString, QString, bool)
{}

#if 1
void TraceItemView::saveViewConfig(KConfig*, QString, QString, bool)
{}
#else
void TraceItemView::saveViewConfig(KConfig* c, 
				   QString prefix, QString postfix, bool)
{
    // write a dummy config entry to see missing virtual functions
    KConfigGroup g(c, (prefix+postfix).ascii());
    g.writeEntry("SaveNotImplemented", true);
}
#endif

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
			TraceCostType* t1, TraceCostType* t2,
			TraceItem::CostType g, const TracePartList& l,
                        TraceItem* a, TraceItem* s)
{
  _status |= changeType;
  _newData = d;
  _newGroupType = g;
  _newCostType = t1;
  _newCostType2 = t2;
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
  _costType = _newCostType = 0;
  _costType2 = _newCostType2 = 0;
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

    // if there's no data change and data is 0, no update needed
    if (!_data) return;
  }

  if (!(_newPartList == _partList)) {
    _status |= partsChanged;
    _partList = _newPartList;
  }
  else
    _status &= ~partsChanged;

  if (_newActiveItem != _activeItem) {

      // when setting a new active item, there's no selection
      _selectedItem = 0;

      _status |= activeItemChanged;
      _activeItem = _newActiveItem;
  }
  else
    _status &= ~activeItemChanged;

  if (_newCostType != _costType) {
    _status |= costTypeChanged;
    _costType = _newCostType;
  }
  else
    _status &= ~costTypeChanged;

  if (_newCostType2 != _costType2) {
    _status |= costType2Changed;
    _costType2 = _newCostType2;
  }
  else
    _status &= ~costType2Changed;

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
  kdDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::doUpdate ( "
            << ((_status & dataChanged) ? "data ":"")
            << ((_status & configChanged) ? "config ":"")
            << ")" << endl;

  if (_status & partsChanged)
    kdDebug() << "  Part List "
              << _partList.names()
              << endl;

  if (_status & costTypeChanged)
    kdDebug() << "  Cost type "
              << (_costType ? _costType->name().ascii() : "?")
              << endl;

  if (_status & costType2Changed)
    kdDebug() << "  Cost type 2 "
              << (_costType2 ? _costType2->name().ascii() : "?")
              << endl;

  if (_status & groupTypeChanged)
    kdDebug() << "  Group type "
              << TraceItem::typeName(_groupType)
              << endl;

  if (_status & activeItemChanged)
    kdDebug() << "  Active: "
              << (_activeItem ? _activeItem->fullName().ascii() : "?")
              << endl;

  if (_status & selectedItemChanged)
    kdDebug() << "  Selected: "
              << (_selectedItem ? _selectedItem->fullName().ascii() : "?")
              << endl;
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
  kdDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::selected "
            << (i ? i->name().ascii(): "(nil)")
            << ", sender "
            << sender->widget()->name() << endl;
#endif

  if (_parentView) _parentView->selected(this, i);
}

void TraceItemView::selected(TraceItemView* /*sender*/, const TracePartList& l)
{
#if TRACE_UPDATES
  kdDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::selected "
            << l.names()
            << ", sender "
            << sender->widget()->name() << endl;
#endif

  if (_parentView)
    _parentView->selected(this, l);
  else
    if (_topLevel) _topLevel->activePartsChangedSlot(l);
}

void TraceItemView::activated(TraceItemView* /*sender*/, TraceItem* i)
{
#if TRACE_UPDATES
  kdDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::activated "
            << (i ? i->name().ascii(): "(nil)")
            << ", sender "
            << sender->widget()->name() << endl;
#endif

  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedCostType(TraceItemView*, TraceCostType* t)
{
  if (_parentView)
      _parentView->selectedCostType(this, t);
  else
      if (_topLevel) _topLevel->setCostTypeDelayed(t);
}

void TraceItemView::selectedCostType2(TraceItemView*, TraceCostType* t)
{
  if (_parentView)
      _parentView->selectedCostType2(this, t);
  else
      if (_topLevel) _topLevel->setCostType2Delayed(t);
}

void TraceItemView::activated(TraceItemView*, Direction d)
{
  if (_parentView)
      _parentView->activated(this, d);
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

void TraceItemView::selected(const TracePartList& l)
{
  if (_parentView)
      _parentView->selected(this, l);
  else
      if (_topLevel) _topLevel->activePartsChangedSlot(l);
}

void TraceItemView::activated(TraceItem* i)
{
#if TRACE_UPDATES
  kdDebug() << (widget() ? widget()->name() : "TraceItemView")
            << "::activated "
            << (i ? i->name().ascii(): "(nil)") << endl;
#endif

  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selectedCostType(TraceCostType* t)
{
  if (_parentView)
      _parentView->selectedCostType(this, t);
  else
      if (_topLevel) _topLevel->setCostTypeDelayed(t);
}

void TraceItemView::selectedCostType2(TraceCostType* t)
{
  if (_parentView)
      _parentView->selectedCostType2(this, t);
  else
      if (_topLevel) _topLevel->setCostType2Delayed(t);
}

void TraceItemView::activated(Direction d)
{
  if (_parentView)
      _parentView->activated(this, d);
  else
      if (_topLevel) _topLevel->setDirectionDelayed(d);
}

void TraceItemView::addCostMenu(QPopupMenu* p, bool withCost2)
{
  if (_topLevel) _topLevel->addCostMenu(p, withCost2);
}

void TraceItemView::addGoMenu(QPopupMenu* p)
{
  if (_topLevel) _topLevel->addGoMenu(p);
}
