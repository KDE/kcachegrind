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

#include "traceitemview.h"
#include "toplevel.h"

#define TRACE_UPDATES 1

TraceItemView::TraceItemView(TraceItemView* parentView, TopLevel* top)
{
  _parentView = parentView;
  _topLevel = top ? top : parentView->topLevel();

  _data = _newData = 0;
  _activeItem = _newActiveItem = 0;
  _selectedItem = _newSelectedItem = 0;
  _costType = _newCostType = 0;
  _groupType = _newGroupType = TraceItem::NoCostType;

  _status = nothingChanged;
  _inUpdate = false;
  _pos = Hidden;
}

QString TraceItemView::whatsThis()
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

void TraceItemView::readViewConfig(KConfig*, QString, QString)
{}

#if 1
void TraceItemView::saveViewConfig(KConfig*, QString, QString)
{}
#else
void TraceItemView::saveViewConfig(KConfig* c, QString prefix, QString postfix)
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
			TraceCostType* t ,TraceItem::CostType g,
                        TraceItem* a, TraceItem* s)
{
  _status |= changeType;
  _newData = d;
  _newGroupType = g;
  _newCostType = t;
  _newSelectedItem = s;
  _newActiveItem = canShow(a);
  if (!_newActiveItem) {
      _newSelectedItem = 0;
      return false;
  }

  return true;
}

QWidget* TraceItemView::widget() const
{
  return 0;
}

bool TraceItemView::isViewVisible()
{
  QWidget* w = widget();
  if (w)
    return w->isVisible();
  return false;
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
  qDebug("%s::doUpdate ( %s%s%s)",
         widget() ? widget()->name() : "TraceItemView",
	 (_status & partsChanged)        ? "part ":"",
         (_status & dataChanged)         ? "data ":"",
         (_status & configChanged)       ? "config ":"");

  if (_status & costTypeChanged)
      qDebug( "  Cost type %s", _costType ? _costType->name().ascii() : "?");
  if (_status & groupTypeChanged)
      qDebug( "  Group type %s", TraceItem::typeName(_groupType).ascii());
  if (_status & activeItemChanged)
      qDebug( "  Active: %s",
	      _activeItem ? _activeItem->fullName().ascii() : "?");
  if (_status & selectedItemChanged)
      qDebug( "  Selected: %s",
	      _selectedItem ? _selectedItem->fullName().ascii() : "?");

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


void TraceItemView::selected(TraceItemView*, TraceItem* i)
{
  if (_parentView) _parentView->selected(this, i);
}

void TraceItemView::activated(TraceItemView*, TraceItem* i)
{
  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selected(TraceItemView*, TraceCostType* t)
{
  if (_parentView)
      _parentView->selected(this, t);
  else
      if (_topLevel) _topLevel->setCostTypeDelayed(t);
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

void TraceItemView::activated(TraceItem* i)
{
  if (_parentView)
      _parentView->activated(this, i);
  else
      if (_topLevel) _topLevel->setTraceItemDelayed(i);
}

void TraceItemView::selected(TraceCostType* t)
{
  if (_parentView)
      _parentView->selected(this, t);
  else
      if (_topLevel) _topLevel->setCostTypeDelayed(t);
}

void TraceItemView::activated(Direction d)
{
  if (_parentView)
      _parentView->activated(this, d);
  else
      if (_topLevel) _topLevel->setDirectionDelayed(d);
}
