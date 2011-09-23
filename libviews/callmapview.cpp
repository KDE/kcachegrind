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
#include <QAction>
#include <QMenu>

#include "config.h"
#include "globalguiconfig.h"
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
                    "(however, there are drawing constraints).</p>") :
              tr("<b>Call Map</b>"
                   "<p>This graph shows the nested hierarchy of "
                   "all callees of the current activated function. "
                   "Each colored rectangle represents a function; "
                   "its size tries to be proportional to the cost spent "
                   "therein while the active function is running "
                   "(however, there are drawing constraints).</p>");

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

void CallMapView::addItemListMenu(QMenu* menu, TreeMapItem* item)
{
    QAction* a;

    QMenu* m = menu->addMenu(tr("Go To"));
    int count = 0;
    while (count<GlobalConfig::maxSymbolCount() && item) {
	QString name = item->text(0);
	a = m->addAction(GlobalConfig::shortenSymbol(name));
	a->setData(QVariant::fromValue( (void*)item ));
	item = item->parent();
	count++;
    }
    connect(m, SIGNAL(triggered(QAction*)),
	    this, SLOT(mapItemTriggered(QAction*)) );
}

void CallMapView::mapItemTriggered(QAction* a)
{
    activatedSlot( (TreeMapItem*) a->data().value<void*>() );
}

QAction* CallMapView::addDrawingDepthAction(QMenu* m,
					    const QString& s, int d)
{
    QAction* a = m->addAction(s);
    a->setData(d);
    a->setCheckable(true);
    a->setChecked(maxDrawingDepth() == d);
    return a;
}

void CallMapView::addDrawingDepthMenu(QMenu* menu,
				      TreeMapItem* i, const QString& name)
{
    QMenu* m = menu->addMenu(tr("Stop at Depth"));
    addDrawingDepthAction(m, tr("No Depth Limit"), -1);
    m->addSeparator();
    addDrawingDepthAction(m, tr("Depth 10"), 10);
    addDrawingDepthAction(m, tr("Depth 15"), 15);
    addDrawingDepthAction(m, tr("Depth 20"), 20);
    if (i) {
	m->addSeparator();
	addDrawingDepthAction(m, tr("Depth of '%1' (%2)")
			      .arg(name).arg(i->depth()),
			      i->depth());
    }
    int maxDepth = maxDrawingDepth();
    if (maxDepth>0) {
	m->addSeparator();
	addDrawingDepthAction(m, tr("Decrement Depth (to %1)").arg(maxDepth-1),
			      maxDepth-1);
	addDrawingDepthAction(m, tr("Increment Depth (to %1)").arg(maxDepth+1),
			      maxDepth+1);
    }

    connect(m, SIGNAL(triggered(QAction*)),
	    this, SLOT(drawingDepthTriggered(QAction*)) );
}

void CallMapView::drawingDepthTriggered(QAction* a)
{
    setMaxDrawingDepth(a->data().toInt());
}

QAction* CallMapView::addStopFunctionAction(QMenu* m,
					    const QString& s,
					    const QString& v)
{
    QAction* a = m->addAction(s);
    a->setData(v);
    a->setCheckable(true);
    a->setChecked(fieldStop(0) == v);
    return a;
}

void CallMapView::addStopFunctionMenu(QMenu* menu, TreeMapItem* item)
{
    QMenu* m = menu->addMenu(tr("Stop at Function"));
    addStopFunctionAction(m, tr("No Function Limit"), QString());

    bool foundStopName = false;
    QAction* a;
    if (item) {
	m->addSeparator();
	int count = 0;
	while (count<GlobalConfig::maxSymbolCount() && item) {
	    QString name = GlobalConfig::shortenSymbol(item->text(0));
	    a = addStopFunctionAction(m, name, item->text(0));
	    if (a->isChecked()) foundStopName = true;
	    item = item->parent();
	    count++;
	}
    }
    if (!foundStopName && !fieldStop(0).isEmpty()) {
	m->addSeparator();
	QString name = GlobalConfig::shortenSymbol(fieldStop(0));
	addStopFunctionAction(m, name, fieldStop(0));
    }

    connect(m, SIGNAL(triggered(QAction*)),
	    this, SLOT(stopFunctionTriggered(QAction*)) );
}

void CallMapView::stopFunctionTriggered(QAction* a)
{
    setFieldStop(0, a->data().toString());
}

QAction* CallMapView::addAreaLimitAction(QMenu* m,
					 const QString& s, int v)
{
    QAction* a = m->addAction(s);
    a->setData(v);
    a->setCheckable(true);
    a->setChecked(minimalArea() == v);
    return a;
}

void CallMapView::addAreaLimitMenu(QMenu* menu, TreeMapItem* i,
				   const QString& name)
{
    QMenu* m = menu->addMenu(tr("Stop at Area"));
    addAreaLimitAction(m, tr("No Area Limit"), -1);
    m->addSeparator();
    addAreaLimitAction(m, tr("100 Pixels"), 100);
    addAreaLimitAction(m, tr("200 Pixels"), 200);
    addAreaLimitAction(m, tr("500 Pixels"), 500);
    addAreaLimitAction(m, tr("1000 Pixels"), 1000);

    int currentArea = 0;
    if (i) {
	currentArea = i->width() * i->height();
	m->addSeparator();
	addAreaLimitAction(m, tr("Area of '%1' (%2)")
			   .arg(name).arg(currentArea), currentArea);
    }
    int mArea = minimalArea();
    if (mArea>0) {
	m->addSeparator();
	addAreaLimitAction(m, tr("Double Area Limit (to %1)")
			   .arg(mArea*2), mArea*2);
	addAreaLimitAction(m, tr("Half Area Limit (to %1)")
			   .arg(mArea/2), mArea/2);
    }

    connect(m, SIGNAL(triggered(QAction*)),
	    this, SLOT(areaLimitTriggered(QAction*)) );
}

void CallMapView::areaLimitTriggered(QAction* a)
{
    setMinimalArea(a->data().toInt());
}

QAction* CallMapView::addBorderWidthAction(QMenu* m, const QString& s, int v)
{
    QAction* a = m->addAction(s);
    a->setData(v);
    a->setCheckable(true);
    a->setChecked(borderWidth() == v);
    return a;
}

void CallMapView::borderWidthTriggered(QAction* a)
{
    setBorderWidth(a->data().toInt());
}

void CallMapView::context(TreeMapItem* i,const QPoint & p)
{
  if (!i) return;

  QMenu popup;
  QAction* a;

  QString shortCurrentName;
  if (i) {
    shortCurrentName = GlobalConfig::shortenSymbol(i->text(0));
  }

  if (i) {
      addItemListMenu(&popup, i);
      popup.addSeparator();
  }
  addGoMenu(&popup);
  popup.addSeparator();
  addDrawingDepthMenu(&popup, i, shortCurrentName);
  addStopFunctionMenu(&popup, i);
  addAreaLimitMenu(&popup, i, shortCurrentName);
  popup.addSeparator();

  QMenu* vpopup = popup.addMenu(tr("Visualization"));
  QMenu* spopup = vpopup->addMenu(tr("Split Direction"));
  addSplitDirectionItems(spopup);

  QAction* skipBorderAction = vpopup->addAction(tr("Skip Incorrect Borders"));
  skipBorderAction->setEnabled(!_showCallers);
  skipBorderAction->setCheckable(true);
  skipBorderAction->setChecked(skipIncorrectBorder());

  QMenu* bpopup = vpopup->addMenu(tr("Border Width"));
  a = addBorderWidthAction(bpopup, tr("Border 0"), 0);
  a->setEnabled(!_showCallers);
  addBorderWidthAction(bpopup, tr("Border 1"), 1);
  addBorderWidthAction(bpopup, tr("Border 2"), 2);
  addBorderWidthAction(bpopup, tr("Border 3"), 3);
  connect(bpopup, SIGNAL(triggered(QAction*)),
	  this, SLOT(borderWidthTriggered(QAction*)) );
  vpopup->addSeparator();

  QAction* drawNamesAction = vpopup->addAction(tr("Draw Symbol Names"));
  drawNamesAction->setCheckable(true);
  QAction* drawCostAction = vpopup->addAction(tr("Draw Cost"));
  drawCostAction->setCheckable(true);
  QAction* drawLocationAction = vpopup->addAction(tr("Draw Location"));
  drawLocationAction->setCheckable(true);
  QAction* drawCallsAction = vpopup->addAction(tr("Draw Calls"));
  drawCallsAction->setCheckable(true);
  vpopup->addSeparator();

  QAction* ignorePropAction = vpopup->addAction(tr("Ignore Proportions"));
  ignorePropAction->setCheckable(true);
  QAction* allowRotationAction = vpopup->addAction(tr("Allow Rotation"));
  allowRotationAction->setCheckable(true);
  if (!fieldVisible(0) &&
      !fieldVisible(1) &&
      !fieldVisible(2) &&
      !fieldVisible(3)) {
      ignorePropAction->setEnabled(false);
      allowRotationAction->setEnabled(false);
  }
  else {
      drawNamesAction->setChecked(fieldVisible(0));
      drawCostAction->setChecked(fieldVisible(1));
      drawLocationAction->setChecked(fieldVisible(2));
      drawCallsAction->setChecked(fieldVisible(3));
      ignorePropAction->setChecked(fieldForced(0));
      allowRotationAction->setChecked(allowRotation());
  }

  QAction* drawShadingAction = vpopup->addAction(tr("Shading"));
  drawShadingAction->setCheckable(true);
  drawShadingAction->setChecked(isShadingEnabled());

  a = popup.exec(mapToGlobal(p));
  if (a == drawNamesAction)
      setFieldVisible(0, !fieldVisible(0));
  else if (a == drawCostAction)
      setFieldVisible(1, !fieldVisible(1));
  else if (a == drawLocationAction)
      setFieldVisible(2, !fieldVisible(2));
  else if (a == drawCallsAction)
      setFieldVisible(3, !fieldVisible(3));
  else if (a == ignorePropAction) {
      bool newSetting = !fieldForced(0);
      setFieldForced(0, newSetting);
      setFieldForced(1, newSetting);
      setFieldForced(2, newSetting);
      setFieldForced(3, newSetting);
  }
  else if (a == allowRotationAction)
      setAllowRotation(!allowRotation());
  else if (a == drawShadingAction)
      setShadingEnabled(!isShadingEnabled());
  else if (a == skipBorderAction)
      setSkipIncorrectBorder(!skipIncorrectBorder());
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

CostItem* CallMapView::canShow(CostItem* i)
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

void CallMapView::doUpdate(int changeType, bool)
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

  return GlobalGUIConfig::functionColor(_groupType, f);
}


QString CallMapView::tipString(TreeMapItem* i) const
{
  QString tip, itemTip;
  int count = 0;

  //qDebug("CallMapView::tipString for '%s'", i->text(0).toAscii());

  // first, SubPartItem's
  while (i && count<GlobalConfig::maxSymbolCount()) {
    itemTip = GlobalConfig::shortenSymbol(i->text(0));

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


ProfileCostArray* CallMapView::totalCost()
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

  EventType* ct = ((CallMapView*)widget())->eventType();
  ProfileCostArray* t      = ((CallMapView*)widget())->totalCost();

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

    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _f->inclusive(), (double) (t->subCost(ct)), true);
}


double CallMapBaseItem::value() const
{
  if (!_f) return 0.0;

  EventType* ct;
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
		  qPrintable(text(0)));

    setSorting(-1);
    if (w->showCallers()) {
        foreach(TraceCall* call, _f->callers()) {
            // do not show calls inside of a cycle
            if (call->inCycle()>0) continue;
            if (call->isRecursion()) continue;

            addItem(new CallMapCallerItem(1.0, call));
        }

        setSum(0);
    }
    else {
        foreach(TraceCall* call, _f->callings()) {
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

  EventType* ct;
  ct = ((CallMapView*)widget())->eventType();

  SubCost val = SubCost(_factor * _c->subCost(ct));
  if (GlobalConfig::showPercentage()) {
    // percentage relative to function cost
    ProfileCostArray* t = ((CallMapView*)widget())->totalCost();
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
    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _c, t->subCost(ct) / _factor, true);
}


double CallMapCallingItem::value() const
{
  EventType* ct;
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
    if (0) qDebug("Create Calling subitems (%s)",
                  qPrintable(path(0).join("/")));

    EventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    // same as sum()
    SubCost s = _c->called()->inclusive()->subCost(ct);
    SubCost v = _c->subCost(ct);
    if (v>s) {
      qDebug("Warning: CallingItem subVal %u > Sum %u (%s)",
             (unsigned)v, (unsigned)s, qPrintable(_c->called()->prettyName()));
      v = s;
    }
    double newFactor = _factor * v / s;

#if 0
    qDebug("CallingItem: Subitems of %s => %s, factor %f * %d/%d => %f",
               qPrintable(_c->caller()->prettyName()),
               qPrintable(_c->called()->prettyName()),
               _factor, v, s, newFactor);
#endif
    setSorting(-1);
    foreach(TraceCall* call, _c->called()->callings()) {
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

  EventType* ct;
  ct = ((CallMapView*)widget())->eventType();

  SubCost val = SubCost(_factor * _c->subCost(ct));
  if (GlobalConfig::showPercentage()) {
    ProfileCostArray* t = ((CallMapView*)widget())->totalCost();
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
    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _c, t->subCost(ct) / _factor, true );
}


double CallMapCallerItem::value() const
{
  EventType* ct;
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
    //qDebug("Create Caller subitems (%s)", name().toAscii());

    EventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    SubCost s = _c->caller()->inclusive()->subCost(ct);
    SubCost v = _c->subCost(ct);
    double newFactor = _factor * v / s;


#if 0
    qDebug("CallerItem: Subitems of %s => %s, factor %f * %d/%d => %f",
           qPrintable(_c->caller()->prettyName()),
           qPrintable(_c->called()->prettyName()),
           _factor, v, s, newFactor);
#endif
    setSorting(-1);

    foreach(TraceCall* call, _c->caller()->callers()) {
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
