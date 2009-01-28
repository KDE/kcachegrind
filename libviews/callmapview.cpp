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
 * Call Map View
 */


#include "callmapview.h"

#include <QPixmap>
#include <Qt3Support/Q3PopupMenu>

#include "config.h"
#include "globalconfig.h"
#include "listutils.h"
#include "toplevelbase.h"


//
// CallMapView
//


// defaults
#define DEFAULT_SPLITMODE    "Rows"
#define DEFAULT_DRAWNAME     true
#define DEFAULT_DRAWCOST     true
#define DEFAULT_DRAWLOCATION false
#define DEFAULT_DRAWCALLS    false
#define DEFAULT_FORCESTRINGS false
#define DEFAULT_ROTATION     true
#define DEFAULT_SHADING      true
#define DEFAULT_STOPNAME     ""
#define DEFAULT_MAXDEPTH     -1
#define DEFAULT_MAXAREA      100


CallMapView::CallMapView(bool showCallers, TraceItemView* parentView,
                         QWidget* parent, const char* name)
  : TreeMapWidget(new CallMapBaseItem(), parent),  TraceItemView(parentView)
{
  setObjectName(name);
  _showCallers = showCallers;

  setFieldType(0, tr("A thing's name", "Name" ));
  setFieldType(1, tr( "Cost" ));
  setFieldType(2, tr( "Location" ));
  setFieldPosition(2, TreeMapItem::TopLeft);
  setFieldType(3, tr( "Calls" ));
  setFieldPosition(3, TreeMapItem::TopRight);

  setSplitMode(DEFAULT_SPLITMODE);
  setFieldVisible(0, DEFAULT_DRAWNAME);
  setFieldVisible(1, DEFAULT_DRAWCOST);
  setFieldVisible(2, DEFAULT_DRAWLOCATION);
  setFieldVisible(3, DEFAULT_DRAWCALLS);
  setFieldForced(0, DEFAULT_FORCESTRINGS);
  setFieldForced(1, DEFAULT_FORCESTRINGS);
  setFieldForced(2, DEFAULT_FORCESTRINGS);
  setFieldForced(3, DEFAULT_FORCESTRINGS);
  setAllowRotation(DEFAULT_ROTATION);
  setShadingEnabled(DEFAULT_SHADING);
  setMinimalArea(DEFAULT_MAXAREA);

  connect(this,
          SIGNAL(doubleClicked(TreeMapItem*)),
          SLOT(activatedSlot(TreeMapItem*)));
  connect(this,
          SIGNAL(returnPressed(TreeMapItem*)),
          SLOT(activatedSlot(TreeMapItem*)));
  connect(this,
          SIGNAL(currentChanged(TreeMapItem*, bool)),
          SLOT(selectedSlot(TreeMapItem*, bool)));
  connect(this,
          SIGNAL(contextMenuRequested(TreeMapItem*,const QPoint &)),
          SLOT(context(TreeMapItem*,const QPoint &)));

  this->setWhatsThis( whatsThis());
}

QString CallMapView::whatsThis() const
{
  QString s = _showCallers ?
              tr( "<b>Caller Map</b>"
                    "<p>This graph shows the nested hierarchy of "
                    "all callers of the current activated function. "
                    "Each colored rectangle represents a function; "
                    "its size tries to be proportional to the cost spent "
                    "therein while the active function is running "
                    "(however, there are drawing constrains).</p>") :
              tr("<b>Call Map</b>"
                   "<p>This graph shows the nested hierarchy of "
                   "all callees of the current activated function. "
                   "Each colored rectangle represents a function; "
                   "its size tries to be proportional to the cost spent "
                   "therein while the active function is running "
                   "(however, there are drawing constrains).</p>");

  s += tr( "<p>Appearance options can be found in the "
             "in the context menu. To get exact size proportions, "
             "choose 'Hide incorrect borders'. As this mode can be "
             "<em>very</em> time consuming, you may want to limit "
             "the maximum drawn nesting level before. "
             "'Best' determinates the split direction for children "
             "from the aspect ratio of the parent. "
             "'Always Best' decides on remaining space for each "
             "sibling. "
             "'Ignore Proportions' takes space for function name "
             "drawing <em>before</em> drawing children. Note that "
             "size proportions can get <em>heavily</em> wrong.</p>"

             "<p>This is a <em>TreeMap</em> widget. "
             "Keyboard navigation is available with the left/right arrow "
             "keys for traversing siblings, and up/down arrow keys "
             "to go a nesting level up/down. "
             "<em>Return</em> activates the current item.</p>");

  return s;
}

void CallMapView::setData(TraceData* d)
{
  TraceItemView::setData(d);

  ((CallMapBaseItem*)base())->setFunction(0);
}

void CallMapView::context(TreeMapItem* i,const QPoint & p)
{
  if (!i) return;

  Q3PopupMenu popup;
  Q3PopupMenu fpopup; // select function subpopup
  Q3PopupMenu vpopup; // visualization subpopup
  Q3PopupMenu dpopup; // split direction
  Q3PopupMenu bpopup; // border subpopup
  Q3PopupMenu l1popup; // depth limit subpopup
  Q3PopupMenu l2popup; // function limit subpopup
  Q3PopupMenu l3popup; // area limit subpopup

  TreeMapItem* item = i;
  int count;

  QString shortCurrentName;
  if (i) {
    shortCurrentName = i->text(0);
    if ((int)shortCurrentName.length() > GlobalConfig::maxSymbolLength())
      shortCurrentName =
        shortCurrentName.left(GlobalConfig::maxSymbolLength()) + "...";
  }

  if (item) {
    popup.insertItem(tr("Go To"), &fpopup, 100);
    count = 0;
    while (count<GlobalConfig::maxSymbolCount() && item) {
      QString name = item->text(0);
      fpopup.insertItem(GlobalConfig::shortenSymbol(name), 101+count);
      item = item->parent();
      count++;
    }
    popup.insertSeparator();
  }

  addGoMenu(&popup);
  popup.insertSeparator();

  l1popup.setCheckable(true);
  popup.insertItem(tr("Stop at Depth"), &l1popup, 12);

  int maxDepth = maxDrawingDepth();
  l1popup.insertItem(tr("No Depth Limit"), 50);
  l1popup.setItemChecked(50, maxDepth==-1);
  l1popup.insertSeparator();
  l1popup.insertItem(tr("Depth 10"), 51);
  l1popup.setItemChecked(51, maxDepth==10);
  l1popup.insertItem(tr("Depth 15"), 52);
  l1popup.setItemChecked(52, maxDepth==15);
  l1popup.insertItem(tr("Depth 20"), 53);
  l1popup.setItemChecked(53, maxDepth==20);
  if (i) {
    l1popup.insertSeparator();
    l1popup.insertItem(tr("Depth of '%1' (%2)")
		       .arg(shortCurrentName).arg(i->depth()), 55);
    l1popup.setItemChecked(55, maxDepth == i->depth());
  }
  if (maxDepth>0) {
    l1popup.insertSeparator();
    l1popup.insertItem(tr("Decrement Depth (to %1)").arg(maxDepth-1), 56);
    l1popup.insertItem(tr("Increment Depth (to %1)").arg(maxDepth+1), 57);
  }

  l2popup.setCheckable(true);
  popup.insertItem(tr("Stop at Function"), &l2popup, 13);
  l2popup.insertItem(tr("No Function Limit"), 200);
  l2popup.setItemChecked(200, fieldStop(0).isEmpty());
  bool foundStopName = false;
  item = i;
  if (i) {
    l2popup.insertSeparator();
    count = 0;
    while (count<GlobalConfig::maxSymbolCount() && item) {
      QString name = item->text(0);
      if ((int)name.length()>GlobalConfig::maxSymbolLength())
        name = name.left(GlobalConfig::maxSymbolLength()) + "...";
      l2popup.insertItem(name, 201+count);
      if (item->text(0) == fieldStop(0)) {
        l2popup.setItemChecked(201+count, true);
        foundStopName = true;
      }
      item = item->parent();
      count++;
    }
  }
  if (!foundStopName && !fieldStop(0).isEmpty()) {
    l2popup.insertSeparator();
    QString name = fieldStop(0);
    if ((int)name.length()>GlobalConfig::maxSymbolLength())
      name = name.left(GlobalConfig::maxSymbolLength()) + "...";
    l2popup.insertItem(name, 199);
    l2popup.setItemChecked(199, true);
  }

  l3popup.setCheckable(true);
  popup.insertItem(tr("Stop at Area"), &l3popup, 14);

  int mArea = minimalArea();
  l3popup.insertItem(tr("No Area Limit"), 60);
  l3popup.setItemChecked(60, mArea ==-1);
  l3popup.insertSeparator();
  l3popup.insertItem(tr("50 Pixels"), 63);
  l3popup.setItemChecked(63, mArea==50);
  l3popup.insertItem(tr("100 Pixels"), 64);
  l3popup.setItemChecked(64, mArea==100);
  l3popup.insertItem(tr("200 Pixels"), 65);
  l3popup.setItemChecked(65, mArea==200);
  l3popup.insertItem(tr("500 Pixels"), 66);
  l3popup.setItemChecked(66, mArea==500);
  int currentArea = 0;
  if (i) {
    currentArea = i->width() * i->height();
    l3popup.insertSeparator();
    l3popup.insertItem(tr("Area of '%1' (%2)")
		       .arg(shortCurrentName).arg(currentArea), 67);
    l3popup.setItemChecked(67, mArea == currentArea);
  }
  if (mArea>0) {
    l3popup.insertSeparator();
    l3popup.insertItem(tr("Double Area Limit (to %1)")
		       .arg(mArea*2), 68);
    l3popup.insertItem(tr("Half Area Limit (to %1)")
		       .arg(mArea/2), 69);
  }

  popup.insertSeparator();

  vpopup.setCheckable(true);
  popup.insertItem(tr("Visualization"), &vpopup, 10);

  Q3PopupMenu splitpopup;
  addSplitDirectionItems(&splitpopup, 1001);
  vpopup.insertItem(tr("Split Direction"), &splitpopup, 1000);

  vpopup.insertItem(tr("Skip Incorrect Borders"), 40);
  vpopup.setItemEnabled(40, !_showCallers);
  vpopup.setItemChecked(40, skipIncorrectBorder());

  bpopup.setCheckable(true);
  vpopup.insertItem(tr("Border Width"), &bpopup, 41);
  bpopup.insertItem(tr("Border 0"), 42);
  bpopup.setItemEnabled(42, !_showCallers);
  bpopup.setItemChecked(42, borderWidth()==0);
  bpopup.insertItem(tr("Border 1"), 43);
  bpopup.setItemChecked(43, borderWidth()==1);
  bpopup.insertItem(tr("Border 2"), 44);
  bpopup.setItemChecked(44, borderWidth()==2);
  bpopup.insertItem(tr("Border 3"), 45);
  bpopup.setItemChecked(45, borderWidth()==3);

  vpopup.insertSeparator();

  vpopup.insertItem(tr("Draw Symbol Names"), 20);
  vpopup.insertItem(tr("Draw Cost"), 21);
  vpopup.insertItem(tr("Draw Location"), 22);
  vpopup.insertItem(tr("Draw Calls"), 23);
  vpopup.insertSeparator();

  vpopup.insertItem(tr("Ignore Proportions"), 24);
  vpopup.insertItem(tr("Allow Rotation"), 25);
  if (!fieldVisible(0) &&
      !fieldVisible(1) &&
      !fieldVisible(2) &&
      !fieldVisible(3)) {
    vpopup.setItemEnabled(24, false);
    vpopup.setItemEnabled(25, false);
  }
  else {
    vpopup.setItemChecked(20,fieldVisible(0));
    vpopup.setItemChecked(21,fieldVisible(1));
    vpopup.setItemChecked(22,fieldVisible(2));
    vpopup.setItemChecked(23,fieldVisible(3));
    vpopup.setItemChecked(24,fieldForced(0));
    vpopup.setItemChecked(25,allowRotation());
  }

  vpopup.insertItem(tr("Shading"), 26);
  vpopup.setItemChecked(26,isShadingEnabled());

  int r = popup.exec(mapToGlobal(p));

  if (r>100 && r<150) {
    r -= 100;
    while (i && (r>1)) {
      i=i->parent();
      r--;
    }
    activatedSlot(i);
    return;
  }

  if (r>200 && r<250) {
    r -= 200;
    while (i && (r>1)) {
      i=i->parent();
      r--;
    }
    if (i)
      setFieldStop(0, i->text(0));

    return;
  }

  bool newSetting;

  switch(r) {
  case 20:
    setFieldVisible(0, !fieldVisible(0));
    break;

  case 21:
    setFieldVisible(1, !fieldVisible(1));
    break;

  case 22:
    setFieldVisible(2, !fieldVisible(2));
    break;

  case 23:
    setFieldVisible(3, !fieldVisible(3));
    break;

  case 24:
    newSetting = !fieldForced(0);
    setFieldForced(0, newSetting);
    setFieldForced(1, newSetting);
    setFieldForced(2, newSetting);
    setFieldForced(3, newSetting);
    break;

  case 25: setAllowRotation(!allowRotation()); break;
  case 26: setShadingEnabled(!isShadingEnabled()); break;

  case 40:
    setSkipIncorrectBorder(!skipIncorrectBorder());
    break;

  case 42: setBorderWidth(0); break;
  case 43: setBorderWidth(1); break;
  case 44: setBorderWidth(2); break;
  case 45: setBorderWidth(3); break;

  case 50: setMaxDrawingDepth(-1); break;
  case 51: setMaxDrawingDepth(10); break;
  case 52: setMaxDrawingDepth(15); break;
  case 53: setMaxDrawingDepth(20); break;
  case 55: setMaxDrawingDepth(i->depth()); break;
  case 56: setMaxDrawingDepth(maxDepth-1); break;
  case 57: setMaxDrawingDepth(maxDepth+1); break;

  case 200: setFieldStop(0, QString()); break;

  case 60: setMinimalArea(-1); break;
  case 61: setMinimalArea(10); break;
  case 62: setMinimalArea(20); break;
  case 63: setMinimalArea(50); break;
  case 64: setMinimalArea(100); break;
  case 65: setMinimalArea(200); break;
  case 66: setMinimalArea(500); break;
  case 67: setMinimalArea(currentArea); break;
  case 68: setMinimalArea(mArea*2); break;
  case 69: setMinimalArea(mArea/2); break;
  }
}

void CallMapView::activatedSlot(TreeMapItem* item)
{
  if (!item) return;

  if (item->rtti() == 1) {
    CallMapBaseItem* bi = (CallMapBaseItem*)item;
    activated(bi->function());
  }
  else if (item->rtti() == 2) {
    CallMapCallingItem* ci = (CallMapCallingItem*)item;
    activated(ci->function());
  }
  else if (item->rtti() == 3) {
    CallMapCallerItem* ci = (CallMapCallerItem*)item;
    activated(ci->function());
  }
}

void CallMapView::selectedSlot(TreeMapItem* item, bool kbd)
{
  if (!item) return;
  if (item->text(0).isEmpty()) return;

  if (kbd) {
      QString msg = tr("Call Map: Current is '%1'").arg(item->text(0));
      if (_topLevel)
	  _topLevel->showMessage(msg, 5000);
  }

  TraceFunction* f = 0;

  if (item->rtti() == 1) {
    CallMapBaseItem* bi = (CallMapBaseItem*)item;
    f = bi->function();
  }
  else if (item->rtti() == 2) {
    CallMapCallingItem* ci = (CallMapCallingItem*)item;
    f = ci->function();
  }
  else if (item->rtti() == 3) {
    CallMapCallerItem* ci = (CallMapCallerItem*)item;
    f = ci->function();
  }
  if (f) {
      // this avoids marking
      _selectedItem = f;
      selected(f);
  }
}

TraceItem* CallMapView::canShow(TraceItem* i)
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

void CallMapView::doUpdate(int changeType)
{
    if (changeType == eventType2Changed) return;

    // if there is a selected item, always draw marking...
    if (changeType & selectedItemChanged) {
	TraceFunction* f = 0;

	if (_selectedItem) {
	    switch(_selectedItem->type()) {
	    case ProfileContext::Function:
	    case ProfileContext::FunctionCycle:
		f = (TraceFunction*)_selectedItem;
		break;
	    default:
		break;
	    }
	}
	// if this is the only change...
	if (changeType == selectedItemChanged) {
	    setMarked(f ? 1:0, true);
	    return;
	}
	setMarked(f ? 1:0, false);
    }


  if (changeType & activeItemChanged) {
      TraceFunction* f = 0;

      if (_activeItem) {
	  switch(_activeItem->type()) {
	  case ProfileContext::Function:
	  case ProfileContext::FunctionCycle:
	      f = (TraceFunction*)_activeItem;
	      break;
	  default:
	      break;
	  }
      }
      ((CallMapBaseItem*)base())->setFunction(f);
  }
  else if ( ((changeType & partsChanged) && GlobalConfig::showCycles()) ||
            (changeType & dataChanged) ||
            (changeType & configChanged)) {
    /* regenerates the treemap because traceitems were added/removed */
    base()->refresh();
  }
  else if ((changeType & partsChanged) ||
           (changeType & eventTypeChanged)) {
    /* we need to do the draw order sorting again as the values change */
    resort();
    redraw();
  }
  else
    redraw();
}



QColor CallMapView::groupColor(TraceFunction* f) const
{
  if (!f)
      return palette().color( QPalette::Button );

  return GlobalConfig::functionColor(_groupType, f);
}


QString CallMapView::tipString(TreeMapItem* i) const
{
  QString tip, itemTip;
  int count = 0;

  //qDebug("CallMapView::tipString for '%s'", i->text(0).ascii());

  // first, SubPartItem's
  while (i && count<GlobalConfig::maxSymbolCount()) {
    itemTip = i->text(0);
    if ((int)itemTip.length()>GlobalConfig::maxSymbolLength())
      itemTip = itemTip.left(GlobalConfig::maxSymbolLength()) + "...";

    if (!i->text(1).isEmpty())
      itemTip += " (" + i->text(1) + ')';

    if (!tip.isEmpty()) tip += '\n';

    tip += itemTip;
    i = i->parent();
    count++;
  }
  if (count == GlobalConfig::maxSymbolCount()) tip += "\n...";

  return tip;
}


TraceCost* CallMapView::totalCost()
{
  TraceFunction* f = ((CallMapBaseItem*)base())->function();
  if (!f) return 0;

  return GlobalConfig::showExpanded() ? f->inclusive() : f->data();
}




// CallMapBaseItem

CallMapBaseItem::CallMapBaseItem()
{
  _f = 0;
}

void CallMapBaseItem::setFunction(TraceFunction* f)
{
  if (f == _f) return;

  _f = f;
  refresh();
}


QString CallMapBaseItem::text(int textNo) const
{
  if (textNo == 0) {
    if (!_f)
      return QObject::tr("(no function)");

    return _f->prettyName();
  }

  if (!_f) return QString();

  if (textNo == 2) return _f->prettyLocation();
  if (textNo == 3) return _f->calledCount().pretty();
  if (textNo != 1) return QString();

  TraceEventType* ct = ((CallMapView*)widget())->eventType();
  TraceCost* t      = ((CallMapView*)widget())->totalCost();

  if (GlobalConfig::showPercentage()) {
      double sum, total = t->subCost(ct);
      if (total == 0.0)
	  sum = 100.0;
      else
	  sum = 100.0 * _f->inclusive()->subCost(ct) / total;

      return QString("%1 %")
	  .arg(sum, 0, 'f', GlobalConfig::percentPrecision());
  }
  return _f->inclusive()->prettySubCost(ct);
}

QPixmap CallMapBaseItem::pixmap(int i) const
{
    if ((i != 1) || !_f) return QPixmap();

    TraceEventType* ct = ((CallMapView*)widget())->eventType();
    TraceCost* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _f->inclusive(), (double) (t->subCost(ct)), true);
}


double CallMapBaseItem::value() const
{
  if (!_f) return 0.0;

  TraceEventType* ct;
  ct = ((CallMapView*)widget())->eventType();
  return (double) _f->inclusive()->subCost(ct);
}


double CallMapBaseItem::sum() const
{
  if (!_f) return 0.0;

  CallMapView* w = (CallMapView*)widget();

  if (w->showCallers())
    return 0.0;
  else
    return (double) _f->inclusive()->subCost(w->eventType());
}


bool CallMapBaseItem::isMarked(int) const
{
    return ((CallMapView*)widget())->selectedItem() == _f;
}

TreeMapItemList* CallMapBaseItem::children()
{
  if (_f && !initialized()) {
    CallMapView* w = (CallMapView*)widget();

    if (0) qDebug("Create Function %s (%s)",
		  w->showCallers() ? "Callers":"Callees",
		  text(0).ascii());

    TraceCall* call;

    setSorting(-1);
    if (w->showCallers()) {
      TraceCallList l = _f->callers();
      for (call=l.first();call;call=l.next()) {

        // do not show calls inside of a cycle
        if (call->inCycle()>0) continue;
        if (call->isRecursion()) continue;

        addItem(new CallMapCallerItem(1.0, call));
      }

      setSum(0);
    }
    else {
      TraceCallList l = _f->callings();
      for (call=l.first();call;call=l.next()) {

        // do not show calls inside of a cycle
        if (call->inCycle()>0) continue;
        if (call->isRecursion()) continue;

        CallMapCallingItem* i = new CallMapCallingItem(1.0, call);
        i->init();
        addItem(i);
      }

      setSum(_f->inclusive()->subCost(w->eventType()));
    }
    setSorting(-2, false);
  }

  return _children;
}

QColor CallMapBaseItem::backColor() const
{
  return ((CallMapView*)widget())->groupColor(_f);
}



// CallMapCallingItems

CallMapCallingItem::CallMapCallingItem(double factor, TraceCall* c)
{
  _factor = factor;
  _c = c;
}

void CallMapCallingItem::init()
{
#if 0
  // create assoziation: if not possible, i.e. an ass. already exists
  // for the function, we need to draw the recursive version
  _recursive = !setFunction(_c->called());
  _valid = true;
#endif
}

QString CallMapCallingItem::text(int textNo) const
{
  if (textNo == 0) {
    if (!_c)
      return QObject::tr("(no call)");

    return _c->calledName();
  }

  if (textNo == 2) return _c->called()->prettyLocation();
  if (textNo == 3) return SubCost(_factor * _c->callCount()).pretty();
  if (textNo != 1) return QString();

  TraceEventType* ct;
  ct = ((CallMapView*)widget())->eventType();

  SubCost val = SubCost(_factor * _c->subCost(ct));
  if (GlobalConfig::showPercentage()) {
    // percentage relative to function cost
    TraceCost* t = ((CallMapView*)widget())->totalCost();
    double p  = 100.0 * _factor * _c->subCost(ct) / t->subCost(ct);
    return QString("%1 %")
      .arg(p, 0, 'f', GlobalConfig::percentPrecision());
  }
  return val.pretty();
}

QPixmap CallMapCallingItem::pixmap(int i) const
{
    if (i != 1) return QPixmap();

    // Cost pixmap
    TraceEventType* ct = ((CallMapView*)widget())->eventType();
    TraceCost* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _c, t->subCost(ct) / _factor, true);
}


double CallMapCallingItem::value() const
{
  TraceEventType* ct;
  ct = ((CallMapView*)widget())->eventType();
  return _factor * _c->subCost(ct);
}

double CallMapCallingItem::sum() const
{
  return value();
}

bool CallMapCallingItem::isMarked(int) const
{
    return ((CallMapView*)widget())->selectedItem() == _c->called();
}


TreeMapItemList* CallMapCallingItem::children()
{
  if (!initialized()) {
    if (0) qDebug("Create Calling subitems (%s)", path(0).join("/").ascii());

    TraceEventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    // same as sum()
    SubCost s = _c->called()->inclusive()->subCost(ct);
    SubCost v = _c->subCost(ct);
    if (v>s) {
      qDebug("Warning: CallingItem subVal %u > Sum %u (%s)",
             (unsigned)v, (unsigned)s, _c->called()->prettyName().ascii());
      v = s;
    }
    double newFactor = _factor * v / s;

#if 0
    qDebug("CallingItem: Subitems of %s => %s, factor %f * %d/%d => %f",
               _c->caller()->prettyName().ascii(),
               _c->called()->prettyName().ascii(),
               _factor, v, s, newFactor);
#endif
    setSorting(-1);
    TraceCall* call;
    TraceCallList l = _c->called()->callings();
    for (call=l.first();call;call=l.next()) {

      // do not show calls inside of a cycle
      if (call->inCycle()>0) continue;
      if (call->isRecursion()) continue;

      CallMapCallingItem* i = new CallMapCallingItem(newFactor, call);
      i->init();
      addItem(i);
    }
    setSorting(-2, false);
  }

  return _children;
}


QColor CallMapCallingItem::backColor() const
{
  CallMapView* w = (CallMapView*)widget();
  return w->groupColor(_c->called());
}


// CallMapCallerItem

CallMapCallerItem::CallMapCallerItem(double factor, TraceCall* c)
{
  _factor = factor;
  _c = c;
}

QString CallMapCallerItem::text(int textNo) const
{
  if (textNo == 0) {
    if (!_c)
      return QObject::tr("(no call)");

    return _c->callerName();
  }

  if (textNo == 2) return _c->caller()->prettyLocation();
  if (textNo == 3) return SubCost(_factor * _c->callCount()).pretty();
  if (textNo != 1) return QString();

  TraceEventType* ct;
  ct = ((CallMapView*)widget())->eventType();

  SubCost val = SubCost(_factor * _c->subCost(ct));
  if (GlobalConfig::showPercentage()) {
    TraceCost* t = ((CallMapView*)widget())->totalCost();
    double p  = 100.0 * _factor * _c->subCost(ct) / t->subCost(ct);
    return QString("%1 %")
      .arg(p, 0, 'f', GlobalConfig::percentPrecision());
  }
  return val.pretty();
}


QPixmap CallMapCallerItem::pixmap(int i) const
{
    if (i != 1) return QPixmap();

    // Cost pixmap
    TraceEventType* ct = ((CallMapView*)widget())->eventType();
    TraceCost* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _c, t->subCost(ct) / _factor, true );
}


double CallMapCallerItem::value() const
{
  TraceEventType* ct;
  ct = ((CallMapView*)widget())->eventType();
  return (double) _c->subCost(ct);
}

bool CallMapCallerItem::isMarked(int) const
{
    return ((CallMapView*)widget())->selectedItem() == _c->caller();
}


TreeMapItemList* CallMapCallerItem::children()
{
  if (!initialized()) {
    //qDebug("Create Caller subitems (%s)", name().ascii());

    TraceEventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    SubCost s = _c->caller()->inclusive()->subCost(ct);
    SubCost v = _c->subCost(ct);
    double newFactor = _factor * v / s;


#if 0
    qDebug("CallerItem: Subitems of %s => %s, factor %f * %d/%d => %f",
           _c->caller()->prettyName().ascii(),
           _c->called()->prettyName().ascii(),
           _factor, v, s, newFactor);
#endif
    setSorting(-1);

    TraceCall* call;
    TraceCallList l = _c->caller()->callers();
    for (call=l.first();call;call=l.next()) {

      // do not show calls inside of a cycle
      if (call->inCycle()>0) continue;
      if (call->isRecursion()) continue;

      TreeMapItem* i = new CallMapCallerItem(newFactor, call);
      addItem(i);
    }
    setSorting(-2, false);
  }

  return _children;
}

QColor CallMapCallerItem::backColor() const
{
  CallMapView* w = (CallMapView*)widget();
  return w->groupColor(_c->caller());
}

void CallMapView::restoreOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    setSplitMode(g->value("SplitMode", QString(DEFAULT_SPLITMODE)).toString());

    setFieldVisible(0, g->value("DrawName", DEFAULT_DRAWNAME).toBool());
    setFieldVisible(1, g->value("DrawCost", DEFAULT_DRAWCOST).toBool());
    setFieldVisible(2, g->value("DrawLocation", DEFAULT_DRAWLOCATION).toBool());
    setFieldVisible(3, g->value("DrawCalls", DEFAULT_DRAWCALLS).toBool());

    bool enable = g->value("ForceStrings", DEFAULT_FORCESTRINGS).toBool();
    setFieldForced(0, enable);
    setFieldForced(1, enable);
    setFieldForced(2, enable);
    setFieldForced(3, enable);

    setAllowRotation(g->value("AllowRotation", DEFAULT_ROTATION).toBool());
    setShadingEnabled(g->value("Shading", DEFAULT_SHADING).toBool());
    setFieldStop(0, g->value("StopName", QString(DEFAULT_STOPNAME)).toString());
    setMaxDrawingDepth(g->value("MaxDepth", DEFAULT_MAXDEPTH).toInt());
    setMinimalArea(g->value("MaxArea", DEFAULT_MAXAREA).toInt());

    delete g;
}

void CallMapView::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue("SplitMode", splitModeString(), QString(DEFAULT_SPLITMODE));
    g->setValue("DrawName", fieldVisible(0), DEFAULT_DRAWNAME);
    g->setValue("DrawCost", fieldVisible(1), DEFAULT_DRAWCOST);
    g->setValue("DrawLocation", fieldVisible(2), DEFAULT_DRAWLOCATION);
    g->setValue("DrawCalls", fieldVisible(3), DEFAULT_DRAWCALLS);
    // when option for all text (0-3)
    g->setValue("ForceStrings", fieldForced(0), DEFAULT_FORCESTRINGS);

    g->setValue("AllowRotation", allowRotation(), DEFAULT_ROTATION);
    g->setValue("Shading", isShadingEnabled(), DEFAULT_SHADING);

    g->setValue("StopName", fieldStop(0), QString(DEFAULT_STOPNAME));
    g->setValue("MaxDepth", maxDrawingDepth(), DEFAULT_MAXDEPTH);
    g->setValue("MaxArea", minimalArea(), DEFAULT_MAXAREA);

    delete g;
}

#include "callmapview.moc"
