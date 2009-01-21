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
 * For part file selection, to be put into a QDockWindow
 */

#include "partselection.h"

#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLayout>
#include <Qt3Support/Q3ListView>
#include <Qt3Support/Q3PopupMenu>

#include "partgraph.h"
#include "globalconfig.h"
#include "config.h"

//
// PartSelection
//

// defaults
#define DEFAULT_PARTITIONMODE "Inclusive"
#define DEFAULT_DIAGRAMMODE   false
#define DEFAULT_DRAWFRAMES    true
#define DEFAULT_SHOWINFO      false
#define DEFAULT_FUNCTIONZOOM  false
#define DEFAULT_CALLEELEVELS  1
#define DEFAULT_DRAWNAME      true
#define DEFAULT_DRAWCOST      true
#define DEFAULT_FORCESTRINGS  false
#define DEFAULT_ALLOWROTATION true

PartSelection::PartSelection(QWidget* parent)
  : QWidget(parent)
{
  _data = 0;
  _eventType = 0;
  _eventType2 = 0;
  _groupType = TraceItem::NoCostType;
  _group = 0;
  _function = 0;
  _inSelectionUpdate = false;

  setWindowTitle(tr("Parts Overview"));

  QVBoxLayout* vboxLayout = new QVBoxLayout(this);
  vboxLayout->setSpacing(6);
  vboxLayout->setMargin(6);

  _partAreaWidget = new PartAreaWidget(this);
  _partAreaWidget->setMinimumHeight(50);
  _partAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  _partAreaWidget->setMaxSelectDepth(2);
  _partAreaWidget->setSelectionMode(TreeMapWidget::Extended);
  _partAreaWidget->setSplitMode(TreeMapItem::HAlternate);
  _partAreaWidget->setVisibleWidth(2, true);
  _partAreaWidget->setFieldType(0, tr("Name", "A thing's name"));
  _partAreaWidget->setFieldType(1, tr("Cost" ));
  vboxLayout->addWidget(_partAreaWidget);

  _rangeLabel = new QLabel(this);
  _rangeLabel->setWordWrap(false);
  _rangeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  vboxLayout->addWidget(_rangeLabel);
  _rangeLabel->setText(tr("(no trace parts)"));
  _showInfo    = true; // label is currently shown

  _diagramMode = DEFAULT_DIAGRAMMODE;
  _drawFrames  = DEFAULT_DRAWFRAMES;

  // sets _showInfo
  showInfo(DEFAULT_SHOWINFO);

  _partAreaWidget->setAllowRotation(DEFAULT_ALLOWROTATION);

  connect(_partAreaWidget, SIGNAL(selectionChanged()),
          this, SLOT(selectionChanged()));
  connect(_partAreaWidget, SIGNAL(currentChanged(TreeMapItem*, bool)),
          this, SLOT(currentChangedSlot(TreeMapItem*, bool)));
  connect(_partAreaWidget, SIGNAL(doubleClicked(TreeMapItem*)),
          this, SLOT(doubleClicked(TreeMapItem*)));
  connect(_partAreaWidget,
          SIGNAL(contextMenuRequested(TreeMapItem*,const QPoint &)),
          this,
          SLOT(contextMenuRequested(TreeMapItem*,const QPoint &)));
}

PartSelection::~PartSelection()
{
}

void PartSelection::setData(TraceData* data)
{
  if (_data == data) return;

  _data = data;
  _partAreaWidget->setData(data);
  fillInfo();
}


void PartSelection::refresh()
{
  _partAreaWidget->redraw();
  fillInfo();
}

void PartSelection::setEventType(TraceEventType* ct)
{
  if (ct == _eventType) return;
  _eventType = ct;

  _partAreaWidget->setEventType(ct);
}

void PartSelection::setEventType2(TraceEventType* ct)
{
  if (ct == _eventType2) return;
  _eventType2 = ct;
  if (!_diagramMode) return;

  //TODO: get max cost(type1)/cost(type2) of shown parts
  //_partAreaWidget->setEventType(ct);
}

void PartSelection::setGroupType(TraceItem::CostType gt)
{
  if (gt == _groupType) return;
  _groupType = gt;

  _partAreaWidget->setGroupType(gt);
}

void PartSelection::setGroup(TraceCostItem*)
{
}

void PartSelection::setFunction(TraceFunction* f)
{
  if (_function == f) return;
  _function = f;

  //qDebug("PartSelection::setFunction %s", f->name().ascii());

  // FIXME: The TreeMap should not produce spurious selectionChanged events
  _inSelectionUpdate = true;
  _partAreaWidget->setFunction(_function);
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
    str += " (" + i->text(1) + ')';
  QString msg = tr("Profile Part Overview: Current is '%1'").arg(str);
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

  qDebug("PartSelection::selectionChanged");

  bool something_changed = false;
  bool nothingSelected = true;

  TracePartList pList;
  TreeMapItem* i;
  TracePart* part;

  // if nothing is selected, activate all parts
  TreeMapItemList* list = _partAreaWidget->base()->children();
  if (!list) return;

  for (i=list->first();i;i=list->next())
    if (_partAreaWidget->isSelected(i)) {
      nothingSelected = false;
      break;
    }

  for (i=list->first();i;i=list->next()) {
    part = ((PartItem*)i)->part();
    bool active = nothingSelected || _partAreaWidget->isSelected(i);
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

  qDebug("Entering PartSelection::activePartsChangedSlot");

  TreeMapItem* i;
  TreeMapItemList l = *_partAreaWidget->base()->children();
  // first deselect inactive, then select active (makes current active)
  for (i=l.first();i;i=l.next()) {
    TracePart* part = ((PartItem*)i)->part();
    bool active = (list.containsRef(part)>0);
    if (!active && _partAreaWidget->isSelected(i)) {
#if 0
      qDebug("PartSelection::partsChangedSlot: Part %s changed to unselected.",
             ((PartItem*)i)->part()->shortName().ascii());
#endif

      _partAreaWidget->setSelected(i, false);
    }
  }
  for (i=l.first();i;i=l.next()) {
    TracePart* part = ((PartItem*)i)->part();
    bool active = (list.containsRef(part)>0);
    if (active && !_partAreaWidget->isSelected(i)) {
#if 0
      qDebug("PartSelection::partsChangedSlot: Part %s changed to selected.",
             ((PartItem*)i)->part()->shortName().ascii());
#endif
      _partAreaWidget->setSelected(i, true);
    }
  }

  _inSelectionUpdate = false;

  qDebug("Leaving PartSelection::activePartsChangedSlot");

  fillInfo();
}

void PartSelection::contextMenuRequested(TreeMapItem* i,
                                         const QPoint & p)
{
  if (!i) return;

  Q3PopupMenu popup;
  Q3PopupMenu ppopup;
  Q3PopupMenu vpopup;

  QString str;
  TreeMapItem* s = 0;

	if (_data && (_data->parts().count()>1)) {
		s = _partAreaWidget->possibleSelection(i);
		if (!s->text(0).isEmpty()) {
			if (_partAreaWidget->isSelected(s))
			    str = tr("Deselect '%1'").arg(s->text(0));
			else
			    str = tr("Select '%1'").arg(s->text(0));

			popup.insertItem(str, 1);
		}

		popup.insertItem(tr("Select All Parts"), 2);
		popup.insertItem(tr("Visible Parts"), &ppopup, 10);

		ppopup.insertItem(tr("Hide Selected Parts"), 3);
		ppopup.insertItem(tr("Unhide Hidden Parts"), 4);

		popup.insertSeparator();
	}

  popup.insertItem(tr("Go Back"), 99);
  if (i->rtti() == 3) {
    TreeMapItem* ni = i;
    int id = 100;
    while (ni && ni->rtti() == 3) {
      TraceCost* c = ((SubPartItem*)ni)->partCostItem();
      if (c->type() == TraceItem::PartFunction)
        if ( ((TracePartFunction*)c)->function() == _function) break;

      str = tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(ni->text(0)));
      popup.insertItem(str, id);
      ni = ni->parent();
      id++;
    }
  }
  popup.insertSeparator();

  vpopup.setCheckable(true);
  popup.insertItem(tr("Visualization"), &vpopup, 10);

  vpopup.insertItem(tr("Partitioning Mode"), 30);
  vpopup.insertItem(tr("Diagram Mode"), 34);
  vpopup.insertItem(tr("Zoom Function"), 31);
  vpopup.insertItem(tr("Show Direct Calls"), 32);
  vpopup.insertItem(tr("Increment Shown Call Levels"), 33);
  if (_partAreaWidget->visualization() == PartAreaWidget::Partitioning) {
    vpopup.setItemChecked(30, true);
    vpopup.setItemEnabled(31, false);
    vpopup.setItemEnabled(32, false);
    vpopup.setItemEnabled(33, false);
  }
  else {
    vpopup.setItemChecked(31, _partAreaWidget->zoomFunction());
  }
  vpopup.setItemChecked(34, _diagramMode);

  vpopup.insertSeparator();

  vpopup.insertItem(tr("Draw Names"), 20);
  vpopup.insertItem(tr("Draw Costs"), 21);
  vpopup.insertItem(tr("Ignore Proportions"), 22);
  vpopup.insertItem(tr("Draw Frames"), 24);
  vpopup.insertItem(tr("Allow Rotation"), 23);
  if (!_partAreaWidget->fieldVisible(0) &&
      !_partAreaWidget->fieldVisible(1)) {
    vpopup.setItemEnabled(22, false);
    vpopup.setItemEnabled(23, false);
  }
  else {
    vpopup.setItemChecked(20,_partAreaWidget->fieldVisible(0));
    vpopup.setItemChecked(21,_partAreaWidget->fieldVisible(1));
    vpopup.setItemChecked(22,_partAreaWidget->fieldForced(0));
    vpopup.setItemChecked(23,_partAreaWidget->allowRotation());
    vpopup.setItemChecked(24,_drawFrames);
  }

  if (_showInfo)
    popup.insertItem(tr("Hide Info"), 40);
  else
    popup.insertItem(tr("Show Info"), 41);

  int r = popup.exec(_partAreaWidget->mapToGlobal(p));

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
    _partAreaWidget->setSelected(s, !_partAreaWidget->isSelected(s));
    break;

  case 2:
    // select all parts
  {
    TreeMapItemList list = *_partAreaWidget->base()->children();
    _partAreaWidget->setRangeSelection(list.first(), list.last(), true);
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
    _partAreaWidget->setFieldVisible(0, !_partAreaWidget->fieldVisible(0));
    break;

  case 21:
    _partAreaWidget->setFieldVisible(1, !_partAreaWidget->fieldVisible(1));
    break;

  case 22:
    _partAreaWidget->setFieldForced(0, !_partAreaWidget->fieldForced(0));
    _partAreaWidget->setFieldForced(1, !_partAreaWidget->fieldForced(0));
    break;

  case 23:
    _partAreaWidget->setAllowRotation(!_partAreaWidget->allowRotation());
    break;

  case 24:
    _drawFrames = !_drawFrames;
    _partAreaWidget->drawFrame(2,_drawFrames);
    _partAreaWidget->drawFrame(3,_drawFrames);
    break;

  case 30:
    _partAreaWidget->setVisualization( (_partAreaWidget->visualization() != PartAreaWidget::Partitioning) ?
                                     PartAreaWidget::Partitioning :
                                     PartAreaWidget::Inclusive);
    break;

  case 31:
    // zoom/unzoom function
    _partAreaWidget->setZoomFunction(!_partAreaWidget->zoomFunction());
    break;

  case 32:
  case 33:
    // change call Levels
  {
    int l = (r==32) ? 1 : _partAreaWidget->callLevels()+1;
    _partAreaWidget->setCallLevels(l);
  }
  break;

  case 34:
    _diagramMode = !_diagramMode;
    _partAreaWidget->setTransparent(2,_diagramMode);
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
  _partAreaWidget->changeHidden(list);
}

void PartSelection::restoreOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    QString pmode = g->value("PartitionMode",
			     QString(DEFAULT_PARTITIONMODE)).toString();
    if (pmode == "Inclusive")
	_partAreaWidget->setVisualization(PartAreaWidget::Inclusive);
    else
	_partAreaWidget->setVisualization(PartAreaWidget::Partitioning);

    _diagramMode = g->value("DiagramMode", DEFAULT_DIAGRAMMODE).toBool();
    _partAreaWidget->setTransparent(2,_diagramMode);

    _drawFrames = g->value("DrawFrames", DEFAULT_DRAWFRAMES).toBool();
    _partAreaWidget->drawFrame(2,_drawFrames);
    _partAreaWidget->drawFrame(3,_drawFrames);

    showInfo(g->value("ShowInfo", DEFAULT_SHOWINFO).toBool());

    bool enable = g->value("FunctionZoom", DEFAULT_FUNCTIONZOOM).toBool();
    _partAreaWidget->setZoomFunction(enable);

    int levels = g->value("CalleeLevels", DEFAULT_CALLEELEVELS).toInt();
    _partAreaWidget->setCallLevels(levels);

    enable = g->value("DrawName", DEFAULT_DRAWNAME).toBool();
    _partAreaWidget->setFieldVisible(0, enable);

    enable = g->value("DrawCost", DEFAULT_DRAWCOST).toBool();
    _partAreaWidget->setFieldVisible(1, enable);

    enable = g->value("ForceStrings", DEFAULT_FORCESTRINGS).toBool();
    _partAreaWidget->setFieldForced(0, enable);
    _partAreaWidget->setFieldForced(1, enable);

    enable = g->value("AllowRotation", DEFAULT_ALLOWROTATION).toBool();
    _partAreaWidget->setAllowRotation(enable);

    delete g;
}

void PartSelection::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    QString mode;
    if (_partAreaWidget->visualization() == PartAreaWidget::Inclusive)
	mode = "Inclusive";
    else
	mode = "Partitioning";

    g->setValue("PartitionMode", mode, QString(DEFAULT_PARTITIONMODE));
    g->setValue("DiagramMode", _diagramMode, DEFAULT_DIAGRAMMODE);
    g->setValue("DrawFrames", _drawFrames, DEFAULT_DRAWFRAMES);
    g->setValue("ShowInfo", _showInfo, DEFAULT_SHOWINFO);

    g->setValue("FunctionZoom",
		_partAreaWidget->zoomFunction(), DEFAULT_FUNCTIONZOOM);
    g->setValue("CalleeLevels",
		_partAreaWidget->callLevels(), DEFAULT_CALLEELEVELS);
    g->setValue("DrawName",
		_partAreaWidget->fieldVisible(0), DEFAULT_DRAWNAME);
    g->setValue("DrawCost",
		_partAreaWidget->fieldVisible(1), DEFAULT_DRAWCOST);
    g->setValue("ForceStrings",
		_partAreaWidget->fieldForced(0), DEFAULT_FORCESTRINGS);
    g->setValue("AllowRotation",
		_partAreaWidget->allowRotation(), DEFAULT_ALLOWROTATION);

    delete g;
}

void PartSelection::showInfo(bool enable)
{
  if (_showInfo == enable) return;

  _showInfo = enable;
  if (enable) {
    _rangeLabel->show();
    fillInfo();
  }
  else
    _rangeLabel->hide();
}

void PartSelection::fillInfo()
{
  if (!_data) {
    _rangeLabel->setText(tr("(no trace loaded)"));
    return;
  }

  QString info = _data->activePartRange();

  TreeMapItem* i = _partAreaWidget->current();
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


  _rangeLabel->setText(info);
}

#include "partselection.moc"
