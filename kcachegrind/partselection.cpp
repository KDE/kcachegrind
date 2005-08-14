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
 * For part file selection, to be put into a QDockWindow
 */

#include <qtimer.h>
#include <qlistview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qpopupmenu.h>
#include <qlayout.h>

#include <klocale.h>
#include <kconfig.h>
#include <kdebug.h>

#include "partselection.h"
#include "partgraph.h"

PartSelection::PartSelection( QWidget* parent, const char* name)
  : PartSelectionBase(parent, name)
{
  _data = 0;
  _costType = 0;
  _costType2 = 0;
  _groupType = TraceItem::NoCostType;
  _group = 0;
  _function = 0;
  _inSelectionUpdate = false;

  _diagramMode = false;
  _drawFrames = true;

  partAreaWidget->setAllowRotation(false);
  partAreaWidget->setMaxSelectDepth(2);
  partAreaWidget->setSelectionMode(TreeMapWidget::Extended);
  partAreaWidget->setSplitMode(TreeMapItem::HAlternate);
  partAreaWidget->setVisibleWidth(2, true);
  partAreaWidget->setFieldType(0, i18n( "Name" ));
  partAreaWidget->setFieldType(1, i18n( "Cost" ));

  connect(partAreaWidget, SIGNAL(selectionChanged()),
          this, SLOT(selectionChanged()));
  connect(partAreaWidget, SIGNAL(currentChanged(TreeMapItem*, bool)),
          this, SLOT(currentChangedSlot(TreeMapItem*, bool)));
  connect(partAreaWidget, SIGNAL(doubleClicked(TreeMapItem*)),
          this, SLOT(doubleClicked(TreeMapItem*)));
  connect(partAreaWidget,
          SIGNAL(contextMenuRequested(TreeMapItem*,const QPoint &)),
          this,
          SLOT(contextMenuRequested(TreeMapItem*,const QPoint &)));

  _showInfo = true;
  showInfo(false);
}

PartSelection::~PartSelection()
{
}

void PartSelection::setData(TraceData* data)
{
  if (_data == data) return;

  _data = data;
  partAreaWidget->setData(data);
  fillInfo();
}


void PartSelection::refresh()
{
  partAreaWidget->redraw();
  fillInfo();
}

void PartSelection::setCostType(TraceCostType* ct)
{
  if (ct == _costType) return;
  _costType = ct;

  partAreaWidget->setCostType(ct);
}

void PartSelection::setCostType2(TraceCostType* ct)
{  
  if (ct == _costType2) return;
  _costType2 = ct;
  if (!_diagramMode) return;

  //TODO: get max cost(type1)/cost(type2) of shown parts
  //partAreaWidget->setCostType(ct);
}

void PartSelection::setGroupType(TraceItem::CostType gt)
{
  if (gt == _groupType) return;
  _groupType = gt;

  partAreaWidget->setGroupType(gt);
}

void PartSelection::setGroup(TraceCostItem*)
{
}

void PartSelection::setFunction(TraceFunction* f)
{
  if (_function == f) return;
  _function = f;

  //kdDebug() << "PartSelection::setFunction " << f->name() << endl;

  // FIXME: The TreeMap shouldn't produce spurious selectionChanged events
  _inSelectionUpdate = true;
  partAreaWidget->setFunction(_function);
  _inSelectionUpdate = false;
}

void PartSelection::setPart(TracePart*)
{}

void PartSelection::currentChangedSlot(TreeMapItem* i, bool kbd)
{
  if (!i) return;
  if (!kbd) return;
  if (i->text(0).isEmpty()) return;

  QString str = i->text(0);
  if (!i->text(1).isEmpty())
    str += " (" + i->text(1) + ")";
  QString msg = i18n("Profile Part Overview: Current is '%1'").arg(str);
  emit showMessage(msg, 5000);

  if (_showInfo) fillInfo();
}


void PartSelection::doubleClicked(TreeMapItem* i)
{
  if (!i || i->rtti() != 3) return;

  TraceCost* c = ((SubPartItem*) i)->partCostItem();
  TraceCostItem* ci = 0;

  switch(c->type()) {
  case TraceItem::PartFunction:
  {
    TraceFunction* f = ((TracePartFunction*)c)->function();
    if (f)
      emit functionChanged(f);
  }
  return;

  case TraceItem::PartObject:
    ci = ((TracePartObject*)c)->object();
    break;
  case TraceItem::PartClass:
    ci = ((TracePartClass*)c)->cls();
    break;
  case TraceItem::PartFile:
    ci = ((TracePartFile*)c)->file();
    break;
  default:
    break;
  }

  if (ci)
    emit groupChanged(ci);
}


void PartSelection::selectionChanged()
{
  if (_inSelectionUpdate) return;
  
  kdDebug() << "PartSelection::selectionChanged" << endl;

  bool something_changed = false;
  bool nothingSelected = true;

  TracePartList pList;
  TreeMapItem* i;
  TracePart* part;

  // if nothing is selected, activate all parts
  TreeMapItemList* list = partAreaWidget->base()->children();
  if (!list) return;

  for (i=list->first();i;i=list->next())
    if (partAreaWidget->isSelected(i)) {
      nothingSelected = false;
      break;
    }

  for (i=list->first();i;i=list->next()) {
    part = ((PartItem*)i)->part();
    bool active = nothingSelected || partAreaWidget->isSelected(i);
    if (active) {
      pList.append(part);
      something_changed = true;
    }
  }

  if (something_changed) {
    //qDebug("PartSelection: Something changed.");
    emit activePartsChanged(pList);
  }
}

/* this makes the graph selection the same to the parts in the list */
void PartSelection::activePartsChangedSlot(const TracePartList& list)
{
  _inSelectionUpdate = true;

  kdDebug() << "Entering PartSelection::activePartsChangedSlot" << endl;

  TreeMapItem* i;
  TreeMapItemList l = *partAreaWidget->base()->children();
  // first deselect inactive, then select active (makes current active)
  for (i=l.first();i;i=l.next()) {
    TracePart* part = ((PartItem*)i)->part();
    bool active = (list.containsRef(part)>0);
    if (!active && partAreaWidget->isSelected(i)) {
#if 0
      qDebug("PartSelection::partsChangedSlot: Part %s changed to unselected.",
             ((PartItem*)i)->part()->shortName().ascii());
#endif

      partAreaWidget->setSelected(i, false);
    }
  }
  for (i=l.first();i;i=l.next()) {
    TracePart* part = ((PartItem*)i)->part();
    bool active = (list.containsRef(part)>0);
    if (active && !partAreaWidget->isSelected(i)) {
#if 0
      qDebug("PartSelection::partsChangedSlot: Part %s changed to selected.",
             ((PartItem*)i)->part()->shortName().ascii());
#endif
      partAreaWidget->setSelected(i, true);
    }
  }

  _inSelectionUpdate = false;

  kdDebug() << "Leaving PartSelection::activePartsChangedSlot" << endl;

  fillInfo();
}

void PartSelection::contextMenuRequested(TreeMapItem* i,
                                         const QPoint & p)
{
  if (!i) return;

  QPopupMenu popup;
  QPopupMenu ppopup;
  QPopupMenu vpopup;

  QString str;
  TreeMapItem* s = 0;

  if (_data && (_data->parts().count()>1)) {
    s = partAreaWidget->possibleSelection(i);
    if (!s->text(0).isEmpty()) {
      str = (partAreaWidget->isSelected(s)) ?
            i18n("Deselect") : i18n("Select");
      str += " '" + s->text(0) + "'";
      popup.insertItem(str, 1);
    }

    popup.insertItem(i18n("Select All Parts"), 2);

    popup.insertItem(i18n("Visible Parts"), &ppopup, 10);

    ppopup.insertItem(i18n("Hide Selected Parts"), 3);
    ppopup.insertItem(i18n("Unhide Hidden Parts"), 4);

    popup.insertSeparator();
  }

  popup.insertItem(i18n("Go Back"), 99);
  if (i->rtti() == 3) {
    TreeMapItem* ni = i;
    int id = 100;
    while (ni && ni->rtti() == 3) {
      TraceCost* c = ((SubPartItem*)ni)->partCostItem();
      if (c->type() == TraceItem::PartFunction)
        if ( ((TracePartFunction*)c)->function() == _function) break;

      str = i18n("Select") + " '" + ni->text(0) + "'";
      popup.insertItem(str, id);
      ni = ni->parent();
      id++;
    }
  }
  popup.insertSeparator();

  vpopup.setCheckable(true);
  popup.insertItem(i18n("Visualization"), &vpopup, 10);

  vpopup.insertItem(i18n("Partitioning Mode"), 30);
  vpopup.insertItem(i18n("Diagram Mode"), 34);
  vpopup.insertItem(i18n("Zoom Function"), 31);
  vpopup.insertItem(i18n("Show Direct Calls"), 32);
  vpopup.insertItem(i18n("Increment Shown Call Levels"), 33);
  if (partAreaWidget->visualisation() == PartAreaWidget::Partitioning) {
    vpopup.setItemChecked(30, true);
    vpopup.setItemEnabled(31, false);
    vpopup.setItemEnabled(32, false);
    vpopup.setItemEnabled(33, false);
  }
  else {
    vpopup.setItemChecked(31, partAreaWidget->zoomFunction());
  }
  vpopup.setItemChecked(34, _diagramMode);

  vpopup.insertSeparator();

  vpopup.insertItem(i18n("Draw Names"), 20);
  vpopup.insertItem(i18n("Draw Costs"), 21);
  vpopup.insertItem(i18n("Ignore Proportions"), 22);
  vpopup.insertItem(i18n("Draw Frames"), 24);
  vpopup.insertItem(i18n("Allow Rotation"), 23);
  if (!partAreaWidget->fieldVisible(0) &&
      !partAreaWidget->fieldVisible(1)) {
    vpopup.setItemEnabled(22, false);
    vpopup.setItemEnabled(23, false);
  }
  else {
    vpopup.setItemChecked(20,partAreaWidget->fieldVisible(0));
    vpopup.setItemChecked(21,partAreaWidget->fieldVisible(1));
    vpopup.setItemChecked(22,partAreaWidget->fieldForced(0));
    vpopup.setItemChecked(23,partAreaWidget->allowRotation());
    vpopup.setItemChecked(24,_drawFrames);
  }

  if (_showInfo)
    popup.insertItem(i18n("Hide Info"), 40);
  else
    popup.insertItem(i18n("Show Info"), 41);

  int r = popup.exec(partAreaWidget->mapToGlobal(p));

  if (r>=100) {
    TreeMapItem* ci = i;
    while (ci && r>100) {
      ci = ci->parent();
      r--;
    }
    doubleClicked(ci);
    return;
  }

  switch(r) {
  case 1:
    // select/deselect part under mouse
    partAreaWidget->setSelected(s, !partAreaWidget->isSelected(s));
    break;

  case 2:
    // select all parts
  {
    TreeMapItemList list = *partAreaWidget->base()->children();
    partAreaWidget->setRangeSelection(list.first(), list.last(), true);
  }
  break;

  case 3:
    emit partsHideSelected();
    break;

  case 4:
    emit partsUnhideAll();
    break;

  case 99:
    // last selected function
    emit goBack();
    break;

  case 20:
    partAreaWidget->setFieldVisible(0, !vpopup.isItemChecked(20));
    break;

  case 21:
    partAreaWidget->setFieldVisible(1, !vpopup.isItemChecked(21));
    break;

  case 22:
      partAreaWidget->setFieldForced(0, !vpopup.isItemChecked(22));
      partAreaWidget->setFieldForced(1, !vpopup.isItemChecked(22));
      break;

  case 23: partAreaWidget->setAllowRotation(!vpopup.isItemChecked(23));  break;

  case 24: 
    _drawFrames = !_drawFrames;
    partAreaWidget->drawFrame(2,_drawFrames);
    partAreaWidget->drawFrame(3,_drawFrames);
    break;

  case 30:
    partAreaWidget->setVisualisation(!vpopup.isItemChecked(30) ?
                                     PartAreaWidget::Partitioning :
                                     PartAreaWidget::Inclusive);
    break;

  case 31:
    // zoom/unzoom function
    partAreaWidget->setZoomFunction(!vpopup.isItemChecked(31));
    break;

  case 32:
  case 33:
    // change call Levels
  {
    int l = (r==32) ? 1 : partAreaWidget->callLevels()+1;
    partAreaWidget->setCallLevels(l);
  }
  break;

  case 34: 
    _diagramMode = !_diagramMode;
    partAreaWidget->setTransparent(2,_diagramMode);
    break;


  case 40:
  case 41:
    showInfo(r==41);
    break;

  default:
    break;
  }
}

void PartSelection::hiddenPartsChangedSlot(const TracePartList& list)
{
  partAreaWidget->changeHidden(list);
}

void PartSelection::readVisualisationConfig(KConfigGroup* config)
{
  bool enable;

  QString mode = config->readEntry("PartitionMode", "Inclusive");
  if (mode == "Inclusive")
    partAreaWidget->setVisualisation(PartAreaWidget::Inclusive);
  else
    partAreaWidget->setVisualisation(PartAreaWidget::Partitioning);

  _diagramMode = config->readBoolEntry("DiagramMode", false);
  partAreaWidget->setTransparent(2,_diagramMode);

  _drawFrames = config->readBoolEntry("DrawFrames", true);
  partAreaWidget->drawFrame(2,_drawFrames);
  partAreaWidget->drawFrame(3,_drawFrames);

  enable = config->readBoolEntry("GraphZoom", false);
  partAreaWidget->setZoomFunction(enable);

  int levels = config->readNumEntry("GraphLevels", 1);
  partAreaWidget->setCallLevels(levels);

  enable = config->readBoolEntry("GraphDrawName", true);
  partAreaWidget->setFieldVisible(0, enable);

  enable = config->readBoolEntry("GraphDrawCost", true);
  partAreaWidget->setFieldVisible(1, enable);

  enable = config->readBoolEntry("GraphForceStrings", false);
  partAreaWidget->setFieldForced(0, enable);
  partAreaWidget->setFieldForced(1, enable);

  enable = config->readBoolEntry("GraphAllowRotation", true);
  partAreaWidget->setAllowRotation(enable);

  showInfo(config->readBoolEntry("ShowInfo", false));
}

void PartSelection::saveVisualisationConfig(KConfigGroup* config)
{
  QString mode;
  if (partAreaWidget->visualisation() == PartAreaWidget::Inclusive)
    mode = "Inclusive";
  else
    mode = "Partitioning";
  config->writeEntry("PartitionMode", mode);

  config->writeEntry("DiagramMode", _diagramMode);
  config->writeEntry("DrawFrames", _drawFrames);

  config->writeEntry("GraphZoom", partAreaWidget->zoomFunction());
  config->writeEntry("GraphLevels", partAreaWidget->callLevels());
  config->writeEntry("GraphDrawName", partAreaWidget->fieldVisible(0));
  config->writeEntry("GraphDrawCosts", partAreaWidget->fieldVisible(1));
  config->writeEntry("GraphForceStrings", partAreaWidget->fieldForced(0));
  config->writeEntry("GraphAllowRotation", partAreaWidget->allowRotation());

  config->writeEntry("ShowInfo", _showInfo);
}

void PartSelection::showInfo(bool enable)
{
  if (_showInfo == enable) return;

  _showInfo = enable;
  if (enable) {
    rangeLabel->show();
    fillInfo();
  }
  else
    rangeLabel->hide();
}

void PartSelection::fillInfo()
{
  if (!_data) {
    rangeLabel->setText(i18n("(no trace loaded)"));
    return;
  }

  QString info = _data->activePartRange();

  TreeMapItem* i = partAreaWidget->current();
  while (i && i->rtti()!=2) i = i->parent();
  if (i) {
    TracePart* part = ((PartItem*)i)->part();

    //if (!part->trigger().isEmpty()) info += ", " + part->trigger();
    if (!part->timeframe().isEmpty())
      info += ", Time " + part->timeframe() + " BBs";
  }
  else {
    TracePart* part = _data->parts().first();

    if (part && !part->version().isEmpty())
      info += ", Cachegrind " + part->version();
  }


  rangeLabel->setText(info);
}

#include "partselection.moc"
