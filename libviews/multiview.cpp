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

  foreach(TabView* tv, _views)
    tv->setData(d);
}

void MultiView::setChildCount(int n)
{
    while(n< _views.count()) removeView();
    while(n> _views.count()) appendView();
}

void MultiView::appendView()
{
    int n = _views.count()+1;

    TabView* tv = new TabView(this, this);
    tv->setObjectName(QString("TabView-%1").arg(n));
    connect(tv, SIGNAL(tabActivated(TabView*)),
	    this, SLOT(tabActivated(TabView*)) );
    _views.append(tv);
    tv->show();

    // no need to waste time with update merging
    tv->setMergeUpdates(false);

    // set same attributes as in active view
    tv->set(0, _data, _eventType, _eventType2,
	    _groupType, _partList, _activeItem, 0);

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

    _views.removeAll(last);
    delete last;

    if (0) qDebug() << "MultiView::removeView, now "
		     << _views.count();
}


void MultiView::tabActivated(TabView* newActiveTab)
{
    if (_active == newActiveTab) return;

    if (0) qDebug() << "MultiView::tabActivated "
                     << newActiveTab->objectName();

    CostItem* oldActiveItem = 0;
    if (_active) {
	oldActiveItem = _active->activeItem();
	_active->setActive(false);
    }
    _active = newActiveTab;

    // make the active item of the new TabView active
    if (_active && (oldActiveItem != _active->activeItem()))
	TraceItemView::activated(_active->activeItem());
}

void MultiView::selected(TraceItemView* sender, CostItem* i)
{
    if (0) qDebug() << "MultiView::selected " << i->name()
                     << ", sender " << sender->widget()->objectName();

     // we react only on selection changes of the active TabView
    if (sender != (TraceItemView*)_active) return;

    int idx = _views.indexOf(_active);
    idx++;
    if (idx == _views.count()) idx = 0;
    TabView* next = _views.at(idx);

    // do not change item of active tab
    if (next == _active) return;

    next->activate(i);
}

void MultiView::activated(TraceItemView* sender, CostItem* i)
{
    if (0) qDebug() << "MultiView::activated " << i->name()
                     << ", sender " << sender->widget()->objectName();

    // we react only on selection changes of the active TabView
    if (sender != (TraceItemView*)_active) return;

    TraceItemView::activated(sender,i);
}

void MultiView::doUpdate(int changeType, bool force)
{
    foreach(TabView* tv, _views) {
	tv->set(changeType, _data, _eventType, _eventType2,
		_groupType, _partList,
		(tv == _active) ? _activeItem : tv->activeItem(),
		tv->selectedItem());
	tv->notifyChange(changeType);
	if (tv->isViewVisible())
	    tv->updateView(force);
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

    TabView* activeTV = 0;
    foreach(TabView* tv, _views) {
        if (tv->objectName() == active) activeTV=tv;
        tv->restoreLayout( QString("%1-%2").arg(prefix).arg(tv->objectName()),
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
    g->setValue("ActivePanel",
                _active ? QString(_active->objectName()) : QString("none"));
    delete g;

    foreach(TabView* tv, _views)
        tv->saveLayout(QString("%1-%2").arg(prefix).arg(tv->objectName()),
		       postfix);
}



void MultiView::restoreOptions(const QString& prefix, const QString& postfix)
{
    foreach(TabView* tv, _views)
        tv->restoreOptions(QString("%1-%2").arg(prefix).arg(tv->objectName()),
			   postfix);
}

void MultiView::saveOptions(const QString& prefix, const QString& postfix)
{
    foreach(TabView* tv, _views)
        tv->saveOptions(QString("%1-%2").arg(prefix).arg(tv->objectName()),
			postfix);
}


#include "multiview.moc"
