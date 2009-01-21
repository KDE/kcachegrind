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
 * MultiView, enclosing multiple TabView's with a user choosable
 * active view (i.e. focus), separated by a splitter.
 * Selection of the active view is shown in the next to the right view
 * (with wrap around).
 */

#include "multiview.h"

#include <QObject>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <QDebug>

#include "config.h"
#include "tabview.h"


//
// MultiView
//

MultiView::MultiView(TopLevelBase* top, QWidget* parent)
    : QSplitter(parent), TraceItemView(0, top)
{
  // default
  setOrientation(Qt::Horizontal);

    appendView();
    _active = _views.first();
    _active->setActive(true);
}

void MultiView::setData(TraceData* d)
{
  TraceItemView::setData(d);

  TabView* tv;
  for(tv=_views.first(); tv; tv=_views.next())
    tv->setData(d);
}

void MultiView::setChildCount(int n)
{
    while(n< (int)_views.count()) removeView();
    while(n> (int)_views.count()) appendView();
}

void MultiView::appendView()
{
    int n = _views.count()+1;

    TabView* tv = new TabView(this, this,
			      QString("TabView-%1").arg(n).ascii());
    connect(tv, SIGNAL(tabActivated(TabView*)),
	    this, SLOT(tabActivated(TabView*)) );
    _views.append(tv);
    tv->show();

    // set same attributes as in active view
    tv->set(0, _data, _eventType, _eventType2,
	    _groupType, _partList, _activeItem, 0);
    tv->updateView();

    if (0) qDebug() << "MultiView::appendView, now "
		     << _views.count();
}

void MultiView::removeView()
{
    if (_views.count()<=1) return;

    TabView* last = _views.last();

    // if last tab is active, make first active
    if (last == _active) {
	TabView* newActive = _views.first();
	newActive->setActive(true);
	tabActivated(newActive);
    }

    _views.removeRef(last);
    delete last;

    if (0) qDebug() << "MultiView::removeView, now "
		     << _views.count();
}


void MultiView::tabActivated(TabView* newActiveTab)
{
    if (_active == newActiveTab) return;

    if (0) qDebug() << "MultiView::tabActivated "
		     << newActiveTab->name();

    TraceItem* oldActiveItem = 0;
    if (_active) {
	oldActiveItem = _active->activeItem();
	_active->setActive(false);
    }
    _active = newActiveTab;

    // make the active item of the new TabView active
    if (_active && (oldActiveItem != _active->activeItem()))
	TraceItemView::activated(_active->activeItem());
}

void MultiView::selected(TraceItemView* sender, TraceItem* i)
{
    if (0) qDebug() << "MultiView::selected " << i->name()
		     << ", sender " << sender->widget()->name();

     // we react only on selection changes of the active TabView
    if (sender != (TraceItemView*)_active) return;

    _views.findRef(_active);
    TabView* next = _views.next();
    if (!next) next = _views.first();

    // do not change item of active tab
    if (next == _active) return;

    next->activate(i);
    next->updateView();
}

void MultiView::activated(TraceItemView* sender, TraceItem* i)
{
    if (0) qDebug() << "MultiView::activated " << i->name()
		     << ", sender " << sender->widget()->name();

    // we react only on selection changes of the active TabView
    if (sender != (TraceItemView*)_active) return;

    TraceItemView::activated(sender,i);
}

void MultiView::doUpdate(int changeType)
{
    TabView* tv;
    for(tv=_views.first(); tv; tv=_views.next()) {
	tv->set(changeType, _data, _eventType, _eventType2,
		_groupType, _partList,
		(tv == _active) ? _activeItem : tv->activeItem(),
		tv->selectedItem());
	tv->notifyChange(changeType);
	if (tv->isViewVisible())
	    tv->updateView();
    }
}


void MultiView::restoreLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    int panelCount = g->value("Panels", 1).toInt();;
    QString o      = g->value("Orientation", QString("Vertical")).toString();
    QString active = g->value("ActivePanel", QString()).toString();

    setChildCount(panelCount);
    setOrientation( o == QString("Horizontal") ?
		    Qt::Horizontal : Qt::Vertical );
    if ( panelCount>1 ) {
	QList<int> sizes = toIntList(g->value("PanelSizes", QStringList()).toStringList());
	setSizes(sizes);
    }
    delete g;

    TabView* tv, *activeTV = 0;
    for(tv=_views.first();tv;tv=_views.next()) {
	if (tv->name() == active) activeTV=tv;
	tv->restoreLayout( QString("%1-%2").arg(prefix).arg(tv->name()),
			   postfix);
    }

    // activate panel after restoring
    if (!activeTV) activeTV = _views.first();

    if (_active == activeTV)
	TraceItemView::activated(_active->activeItem());
    else
	activeTV->setActive(true);
}

void MultiView::saveLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue("Panels", childCount());
    g->setValue("Orientation",
		QString( (orientation() == Qt::Horizontal) ?
			 "Horizontal" : "Vertical"),
		QString("Vertical"));

    g->setValue("PanelSizes", toStringList(sizes()));
    g->setValue("ActivePanel", _active ? _active->name() : QString("none"));
    delete g;

    TabView* tv;
    for(tv=_views.first();tv;tv=_views.next())
	tv->saveLayout(QString("%1-%2").arg(prefix).arg(tv->name()),
		       postfix);
}



void MultiView::readViewConfig(KConfig* c,
                               const QString& prefix, const QString& postfix)
{
  if (0) qDebug("%s::readConfig(%s%s)", name(),
		prefix.ascii(), postfix.ascii());

  TabView* tv;
  for(tv=_views.first();tv;tv=_views.next()) {
      tv->readViewConfig(c, QString("%1-%2").arg(prefix).arg(tv->name()),
			 postfix);
  }
}

void MultiView::saveViewConfig(KConfig* c,
                               const QString& prefix, const QString& postfix)
{
  TabView* tv;
  for(tv=_views.first();tv;tv=_views.next())
    tv->saveViewConfig(c, QString("%1-%2").arg(prefix).arg(tv->name()),
		       postfix);
}


#include "multiview.moc"
