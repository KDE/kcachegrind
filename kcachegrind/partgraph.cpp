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
 * TracePart as Nested Area
 */

#include <klocale.h>

#include "partgraph.h"
#include "configuration.h"
#include "listutils.h"


// PartAreaWidget

PartAreaWidget::PartAreaWidget(QWidget* parent, const char* name)
    : TreeMapWidget(new BasePartItem(), parent, name)
{
  _data = 0;
  _function = 0;

  _costType = 0;
  _groupType = TraceCost::NoCostType;
  _visualisation = NoVisualisation;
  _zoomFunction = false;
  _callLevels = 1;
}

void PartAreaWidget::setData(TraceData* data)
{
  if (data == _data) return;

  _data = data;
  _function = 0;
  _hiddenParts.clear();

  ((BasePartItem*)base())->setData(data);
}

void PartAreaWidget::changeHidden(const TracePartList& list)
{
  _hiddenParts = list;
  base()->refresh();
}


void PartAreaWidget::setCostType(TraceCostType* ct)
{
  _costType = ct;

  // this resizes items
  base()->redraw();
}

void PartAreaWidget::setVisualisation(VisualisationMode m)
{
  _visualisation = m;
  refreshParts();
}

void PartAreaWidget::setZoomFunction(bool zoomFunction)
{
  _zoomFunction = zoomFunction;
  refreshParts();
}

void PartAreaWidget::setCallLevels(int callLevels)
{
  _callLevels = callLevels;
  refreshParts();
}

void PartAreaWidget::refreshParts()
{
  // rebuild only subparts to keep part selection state
  TreeMapItem* i;
  TreeMapItemList* l = base()->children();
  if (l)
    for (i=l->first();i;i=l->next())
      i->refresh();

  // but resize part areas
  base()->redraw();
}


void PartAreaWidget::setFunction(TraceFunction* f)
{
  _function = f;

  if (_visualisation == PartAreaWidget::Inclusive)
    refreshParts();
}

void PartAreaWidget::setGroupType(TraceCost::CostType gt)
{
  _groupType = gt;

  // rebuild hierarchy below parts.
  // thus, selected parts stay selected
  TreeMapItem* i;
  TreeMapItemList* l = base()->children();
  if (l)
    for (i=l->first();i;i=l->next())
      i->refresh();

  base()->redraw();
}

bool PartAreaWidget::isHidden(TracePart* part) const
{
  return (_hiddenParts.containsRef(part)>0);
}

QColor PartAreaWidget::groupColor(TraceFunction* f) const
{
  if (!f)
    return colorGroup().button();

  return Configuration::functionColor(_groupType, f);
}

QString PartAreaWidget::tipString(TreeMapItem* i) const
{
  QString tip, itemTip;
  int count = 0;

  //qDebug("PartAreaWidget::tipString for '%s'", i->name().ascii());

  // first, SubPartItem's
  while (i && count<Configuration::maxSymbolCount() && i->rtti() == 3) {
    itemTip = i->text(0);
    if ((int)itemTip.length()>Configuration::maxSymbolLength())
      itemTip = itemTip.left(Configuration::maxSymbolLength()) + "...";

    if (!i->text(1).isEmpty())
      itemTip += " (" + i->text(1) + ")";

    if (!tip.isEmpty())
      itemTip += "\n";

    tip = itemTip + tip;
    i = i->parent();
    count++;
  }

  // skip to part
  while (i && i->rtti()==3) i = i->parent();

  if (i && i->rtti()==2) {
    itemTip = i18n("Profile Part %1").arg(i->text(0));
    if (!i->text(1).isEmpty())
        itemTip += " (" + i->text(1) + ")";

    if (!tip.isEmpty())
      itemTip += "\n";

    tip = itemTip + tip;
  }

//  qDebug("PartAreaWidget:: tip %s, itemTip %s",
//         tip.ascii(), itemTip.ascii());

  return tip;
}





// BasePartItem

BasePartItem::BasePartItem()
  : TreeMapItem()
{
  _data = 0;
  setSorting(-1);
}

void BasePartItem::setData(TraceData* data)
{
  if (data == _data) return;

  _data = data;
  refresh();
}

TreeMapItemList* BasePartItem::children()
{
  if (!_data) return _children;

  if (!initialized()) {
//    qDebug("Create Parts (%s)", name().ascii());

    PartAreaWidget* w = (PartAreaWidget*) widget();
    TracePart* part;
    TracePartList l = _data->parts();
    for (part=l.first();part;part=l.next())
      if (!w->isHidden(part))
        addItem(new PartItem(part));
  }

  return _children;
}

QString BasePartItem::text(int textNo) const
{
  if (textNo == 0) {
    if (!_data)
      return i18n("(no trace)");

    if (_data->parts().count() == 0)
      return i18n("(no part)");
  }
  return QString::null;
}


QColor BasePartItem::backColor() const
{
  return widget()->colorGroup().base();
}

double BasePartItem::value() const
{
  if (!_data) return 0;

  PartAreaWidget* w = (PartAreaWidget*) widget();
  return (double)_data->subCost(w->costType());
}





// PartItem

PartItem::PartItem(TracePart* p)
{
  _p = p;
  _factor=1;
}

QString PartItem::text(int textNo) const
{
  if (textNo == 0)
    return _p->prettyName();

  if (textNo != 1)
    return QString::null;

  TraceCostType* ct;
  PartAreaWidget* w = (PartAreaWidget*)widget();
  SubCost v;

  ct = w->costType();
  v = _p->subCost(ct);

  if (Configuration::showPercentage()) {
    TraceCost* t = _p->data()->totals();
    double p  = 100.0 * v / t->subCost(ct);
    return QString("%1 %")
      .arg(p, 0, 'f', Configuration::percentPrecision());
  }
  return v.pretty();
}


QPixmap PartItem::pixmap(int i) const
{
    if (i != 1) return QPixmap();

    // Cost pixmap

    TraceCostType* ct = ((PartAreaWidget*)widget())->costType();
    return costPixmap( ct, _p, (double) (_p->data()->totals()->subCost(ct)), false );
}


double PartItem::value() const
{
  PartAreaWidget* w = (PartAreaWidget*)widget();
  TraceCostType* ct = w->costType();
  if ((w->visualisation() == PartAreaWidget::Inclusive) &&
      w->zoomFunction()) {

    // use value of zoomed function
    TraceFunction* f = w->function();
    if (f) {
      TracePartFunction* pf = (TracePartFunction*) f->findDepFromPart(_p);
      if (pf)
        return (double) pf->inclusive()->subCost(ct);
      // when function is not available in part, hide part
      return 0.0;
    }
  }
  return (double) _p->subCost(ct);
}

double PartItem::sum() const
{
  PartAreaWidget* w = (PartAreaWidget*)widget();
  if (w->visualisation() == PartAreaWidget::Inclusive) {
    double s = value();
    //qDebug("PartItem::sum [part %s]: %d", _p->name().ascii(), s);
    return s;
  }
  return 0.0;
}

TreeMapItemList* PartItem::children()
{
  if (initialized()) return _children;

  TraceCost* c;
//    qDebug("Create Part subitems (%s)", name().ascii());

  PartAreaWidget* w = (PartAreaWidget*)widget();
  if (w->visualisation() == PartAreaWidget::Inclusive) {
    TraceFunction* f = w->function();
    if (f) {
      c = f->findDepFromPart(_p);
      if (c) addItem(new SubPartItem(c));
    }

    return _children;
  }


  switch( ((PartAreaWidget*)widget())->groupType() ) {

  case TraceCost::Object:
  {
    TraceObjectMap::Iterator it;
    for ( it = _p->data()->objectMap().begin();
          it != _p->data()->objectMap().end(); ++it ) {
      c = (*it).findDepFromPart(_p);
      if (c)
        addItem(new SubPartItem(c));
    }
  }
  break;

  case TraceCost::Class:
  {
    TraceClassMap::Iterator it;
    for ( it = _p->data()->classMap().begin();
          it != _p->data()->classMap().end(); ++it ) {
      c = (*it).findDepFromPart(_p);
      if (c)
        addItem(new SubPartItem(c));
    }
  }
  break;

  case TraceCost::File:
  {
    TraceFileMap::Iterator it;
    for ( it = _p->data()->fileMap().begin();
          it != _p->data()->fileMap().end(); ++it ) {
      c = (*it).findDepFromPart(_p);
      if (c)
        addItem(new SubPartItem(c));
    }
  }
  break;

  case TraceCost::Function:
  {
    TraceFunctionMap::Iterator it;
    for ( it = _p->data()->functionMap().begin();
          it != _p->data()->functionMap().end(); ++it ) {
      c = (*it).findDepFromPart(_p);
      if (c)
        addItem(new SubPartItem(c));
    }
  }
  break;

  default:
    break;
  }

  return _children;
}


QColor PartItem::backColor() const
{
  PartAreaWidget* w = (PartAreaWidget*)widget();
  return w->groupColor(0);
}


// SubPartItem

SubPartItem::SubPartItem(TraceCost* c)
{
  _partCostItem = c;
  _factor=1;
}

QString SubPartItem::text(int textNo) const
{
  if (textNo == 0) {
    if (!_partCostItem)
      return i18n("(unknown)");

    return _partCostItem->dependant()->prettyName();
  }

  if (textNo != 1)
    return QString::null;

  TraceCostType* ct;
  PartAreaWidget* w = (PartAreaWidget*)widget();
  SubCost v;

  ct = w->costType();
  if (w->visualisation() == PartAreaWidget::Inclusive)
    v = ((TracePartFunction*)_partCostItem)->inclusive()->subCost(ct);
  else
    v = _partCostItem->subCost(ct);

  if (Configuration::showPercentage()) {
    TraceCost* t = Configuration::showExpanded() ?
	_partCostItem->part() : _partCostItem->part()->data()->totals();
    double p  = 100.0 * v / t->subCost(ct);
    return QString("%1 %")
      .arg(p, 0, 'f', Configuration::percentPrecision());
  }
  return v.pretty();
}

QPixmap SubPartItem::pixmap(int i) const
{
    if (i != 1) return QPixmap();

    // Cost pixmap

    PartAreaWidget* w = (PartAreaWidget*)widget();
    TraceCostType* ct = w->costType();
    TraceCost* t = Configuration::showExpanded() ?
	_partCostItem->part() : _partCostItem->part()->data()->totals();
    TraceCost* c;
    if (w->visualisation() == PartAreaWidget::Inclusive)
	c = ((TracePartFunction*)_partCostItem)->inclusive();
    else
	c = _partCostItem;

    return costPixmap( ct, c, (double) (t->subCost(ct)), false );
}

double SubPartItem::value() const
{
  TraceCostType* ct;
  PartAreaWidget* w = (PartAreaWidget*)widget();

  ct = w->costType();
  if (w->visualisation() == PartAreaWidget::Inclusive)
    return (double)
	((TracePartFunction*)_partCostItem)->inclusive()->subCost(ct);

  return (double) _partCostItem->subCost(ct);
}

double SubPartItem::sum() const
{
  PartAreaWidget* w = (PartAreaWidget*)widget();
  if (w->visualisation() == PartAreaWidget::Inclusive) {
    double s = value();
    //qDebug("SubPartItem::sum [Cost %s]: %d", _cost->name().ascii(), s);
    return s;
  }
  return 0.0;
}

TreeMapItemList* SubPartItem::children()
{
  if (!initialized()) {
//    qDebug("Create Part sub-subitems (%s)", name().ascii());

    PartAreaWidget* w = (PartAreaWidget*)widget();

    if (depth()-2 > w->callLevels())
      return _children;

    if (w->visualisation() == PartAreaWidget::Inclusive) {
      TracePartCall* call;
      TracePartCallList l;

      setSum(value());

      l = ((TracePartFunction*)_partCostItem)->partCallings();
      for (call=l.first();call;call=l.next()) {
        TraceFunction* called = call->call()->called();
        TraceCost* partCalled = called->findDepFromPart(call->part());
        if (partCalled)
          addItem(new SubPartItem(partCalled));
      }
    }
  }

  return _children;
}


QColor SubPartItem::backColor() const
{
  PartAreaWidget* w = (PartAreaWidget*)widget();
  if (w->visualisation() == PartAreaWidget::Inclusive)
    return w->groupColor((TraceFunction*)(_partCostItem->dependant()));

  return Configuration::groupColor(_partCostItem->dependant());
}


#include "partgraph.moc"
