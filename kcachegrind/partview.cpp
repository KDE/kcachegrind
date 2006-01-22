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
 * Part View
 */

#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <qheader.h>
#include <klocale.h>

#include "configuration.h"
#include "partlistitem.h"
#include "toplevel.h"
#include "partview.h"



//
// PartView
//


PartView::PartView(TraceItemView* parentView,
		   QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
    _inSelectionUpdate = false;

    addColumn( i18n( "Profile Part" ) );
    addColumn( i18n( "Incl." ) );
    addColumn( i18n( "Self" ) );
    addColumn( i18n( "Called" ) );
    //addColumn( i18n( "Fixed" ) );
    addColumn( i18n( "Comment" ) );

    setAllColumnsShowFocus(true);
    setColumnAlignment(1, Qt::AlignRight);
    setColumnAlignment(2, Qt::AlignRight);
    setColumnAlignment(3, Qt::AlignRight);
    setMinimumHeight(50);
    setSelectionMode(Extended);

    connect( this,
	     SIGNAL( selectionChanged() ),
	     SLOT( selectionChangedSlot() ) );

    connect( this,
	     SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
	     SLOT(context(QListViewItem*, const QPoint &, int)));

    QWhatsThis::add( this, whatsThis() );
}

QString PartView::whatsThis() const
{
    return i18n( "<b>Trace Part List</b>"
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


void PartView::context(QListViewItem* i, const QPoint & pos, int)
{
  QPopupMenu popup;

  TracePart* p = i ? ((PartListItem*) i)->part() : 0;

  if (p) {
    popup.insertItem(i18n("Select '%1'").arg(p->name()), 93);
    popup.insertItem(i18n("Hide '%1'").arg(p->name()), 94);
    popup.insertSeparator();
  }

  popup.insertItem(i18n("Hide Selected"), 95);
  popup.insertItem(i18n("Show All"), 96);
  popup.insertSeparator();

  addGoMenu(&popup);

  int r = popup.exec(pos);
  if (r == 95) {
    ;
  }

  // TODO: ...
}

void PartView::selectionChangedSlot()
{
    if (_inSelectionUpdate) return;

    TracePartList l;
    QListViewItem* item  = firstChild();
    for(;item;item = item->nextSibling())
      if (item->isSelected())
        l.append( ((PartListItem*)item)->part() );

    selected(l);
}


TraceItem* PartView::canShow(TraceItem* i)
{
  if (!data()) return 0;
  if (data()->parts().count()>1) return i;
  return 0;
}

void PartView::doUpdate(int changeType)
{
    // Special case ?
    if (changeType == costType2Changed) return;
    if (changeType == selectedItemChanged) return;

    if (changeType == groupTypeChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	    ((PartListItem*)item)->setGroupType(_groupType);

	return;
    }

    if (changeType == costTypeChanged) {
	QListViewItem *item;
	for (item = firstChild();item;item = item->nextSibling())
	  ((PartListItem*)item)->setCostType(_costType);

	return;
    }

    if (changeType == partsChanged) {

      TracePart* part;

      QListViewItem* item;
      _inSelectionUpdate = true;
      item  = firstChild();
      for(;item;item = item->nextSibling()) {
        part = ((PartListItem*)item)->part();

        if (_partList.containsRef(part)>0) {
          setSelected(item, true);
          ensureItemVisible(item);
        }
        else
          setSelected(item, false);
      }
      _inSelectionUpdate = false;

      return;
    }

    refresh();
}

void PartView::refresh()
{
    clear();
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);

    if (!_data || !_activeItem) return;

    TraceItem::CostType t = _activeItem->type();
    TraceFunction* f = 0;
    if (t == TraceItem::Function) f = (TraceFunction*) _activeItem;
    if (!f) return;

    TracePart* part;
    TracePartList hidden;
    if (_topLevel)
	hidden = _topLevel->hiddenParts();

    TracePartList allParts = _data->parts();

    _inSelectionUpdate = true;

    QListViewItem* item = 0;
    for (part = allParts.first(); part; part = allParts.next()) {
	if (hidden.findRef(part)>=0) continue;
	item = new PartListItem(this, f, _costType, _groupType, part);

	if (part->isActive()) {
	    setSelected(item, true);
	    ensureItemVisible(item);
	}
    }

    _inSelectionUpdate = false;

    if (item) {
	int headerHeight = header()->height();
	int itemHeight = item->height();
	setMinimumHeight(headerHeight + 2*itemHeight + 2);
    }
}

#include "partview.moc"
