/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
                         QWidget* parent, const QString& name)
    : TreeMapWidget(new CallMapRootItem(), parent),  TraceItemView(parentView)
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
            &TreeMapWidget::doubleClicked,
            this, &CallMapView::activatedSlot);
    connect(this,
            &TreeMapWidget::returnPressed,
            this, &CallMapView::activatedSlot);
    connect(this,
            &TreeMapWidget::currentChanged,
            this, &CallMapView::selectedSlot);
    connect(this,
            &TreeMapWidget::contextMenuRequested,
            this, &CallMapView::context);

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
             "context menu. To get exact size proportions, "
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

    ((CallMapRootItem*)base())->setFunction(nullptr);
}

void CallMapView::addItemListMenu(QMenu* menu, TreeMapItem* item)
{
    QAction* a;

    QMenu* m = menu->addMenu(tr("Go To"));
    int count = 0;
    while (count<GlobalConfig::maxSymbolCount() && item) {
        QString name = item->text(IDX_FUNCNAME);
        a = m->addAction(GlobalConfig::shortenSymbol(name));
        a->setData(QVariant::fromValue( (void*)item ));
        item = item->parent();
        count++;
    }
    connect(m, &QMenu::triggered,
            this, &CallMapView::mapItemTriggered );
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

    connect(m, &QMenu::triggered,
            this, &CallMapView::drawingDepthTriggered );
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
    a->setChecked(fieldStop(IDX_FUNCNAME) == v);
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
            QString name = GlobalConfig::shortenSymbol(item->text(IDX_FUNCNAME));
            a = addStopFunctionAction(m, name, item->text(IDX_FUNCNAME));
            if (a->isChecked()) foundStopName = true;
            item = item->parent();
            count++;
        }
    }
    if (!foundStopName && !fieldStop(IDX_FUNCNAME).isEmpty()) {
        m->addSeparator();
        QString name = GlobalConfig::shortenSymbol(fieldStop(IDX_FUNCNAME));
        addStopFunctionAction(m, name, fieldStop(IDX_FUNCNAME));
    }

    connect(m, &QMenu::triggered,
            this, &CallMapView::stopFunctionTriggered );
}

void CallMapView::stopFunctionTriggered(QAction* a)
{
    setFieldStop(IDX_FUNCNAME, a->data().toString());
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

    connect(m, &QMenu::triggered,
            this, &CallMapView::areaLimitTriggered );
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
        shortCurrentName = GlobalConfig::shortenSymbol(i->text(IDX_FUNCNAME));
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
    connect(bpopup, &QMenu::triggered,
            this, &CallMapView::borderWidthTriggered );
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
        drawNamesAction->setChecked(fieldVisible(IDX_FUNCNAME));
        drawCostAction->setChecked(fieldVisible(IDX_COST));
        drawLocationAction->setChecked(fieldVisible(IDX_LOCATION));
        drawCallsAction->setChecked(fieldVisible(IDX_CALLCOUNT));
        ignorePropAction->setChecked(fieldForced(0));
        allowRotationAction->setChecked(allowRotation());
    }

    QAction* drawShadingAction = vpopup->addAction(tr("Shading"));
    drawShadingAction->setCheckable(true);
    drawShadingAction->setChecked(isShadingEnabled());

    a = popup.exec(mapToGlobal(p));
    if (a == drawNamesAction)
        setFieldVisible(IDX_FUNCNAME, !fieldVisible(IDX_FUNCNAME));
    else if (a == drawCostAction)
        setFieldVisible(IDX_COST, !fieldVisible(IDX_COST));
    else if (a == drawLocationAction)
        setFieldVisible(IDX_LOCATION, !fieldVisible(IDX_LOCATION));
    else if (a == drawCallsAction)
        setFieldVisible(IDX_CALLCOUNT, !fieldVisible(IDX_CALLCOUNT));
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
        CallMapRootItem* bi = (CallMapRootItem*)item;
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
    if (item->text(IDX_FUNCNAME).isEmpty()) return;

    if (kbd) {
        QString msg = tr("Call Map: Current is '%1'").arg(item->text(IDX_FUNCNAME));
        if (_topLevel)
            _topLevel->showMessage(msg, 5000);
    }

    TraceFunction* f = nullptr;

    if (item->rtti() == 1) {
        CallMapRootItem* bi = (CallMapRootItem*)item;
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
    return nullptr;
}

void CallMapView::doUpdate(int changeType, bool)
{
    if (changeType == eventType2Changed) return;

    // if there is a selected item, always draw marking...
    if (changeType & selectedItemChanged) {
        TraceFunction* f = nullptr;

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
        TraceFunction* f = nullptr;

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
        ((CallMapRootItem*)base())->setFunction(f);
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
        itemTip = GlobalConfig::shortenSymbol(i->text(IDX_FUNCNAME));

        if (!i->text(IDX_COST).isEmpty())
            itemTip += " (" + i->text(IDX_COST) + ')';

        if (!tip.isEmpty()) tip += '\n';

        tip += itemTip;
        i = i->parent();
        count++;
    }
    if (count == GlobalConfig::maxSymbolCount()) tip += QLatin1String("\n...");

    return tip;
}


ProfileCostArray* CallMapView::totalCost()
{
    TraceFunction* f = ((CallMapRootItem*)base())->function();
    if (!f) return nullptr;

    return GlobalConfig::showExpanded() ? f->inclusive() : f->data();
}



// CallMapItemBase

int CallMapItemBase::maxLines(int i) const
{
    if ((i == IDX_FUNCNAME) || (i == IDX_LOCATION)) return 1;
    return 0;
}

bool CallMapItemBase::allowBreak(int i) const
{
    if ((i == IDX_COST) || (i == IDX_CALLCOUNT)) return false;
    return true;
}

bool CallMapItemBase::allowTruncation(int i) const
{
    if ((i == IDX_COST) || (i == IDX_CALLCOUNT)) return false;
    return true;
}

DrawParams::Position CallMapItemBase::position(int i) const
{
    if ((i == IDX_FUNCNAME) || (i == IDX_LOCATION)) return TopLeft;
    return TopRight;
}



// CallMapRootItem

CallMapRootItem::CallMapRootItem()
{
    _f = nullptr;
}

void CallMapRootItem::setFunction(TraceFunction* f)
{
    if (f == _f) return;

    _f = f;
    refresh();
}


QString CallMapRootItem::text(int i) const
{
    if (i == IDX_FUNCNAME) {
        if (!_f)
            return QObject::tr("(no function)");

        return _f->prettyName();
    }

    if (!_f) return QString();

    if (i == IDX_LOCATION)
        return _f->prettyLocation();
    if (i == IDX_CALLCOUNT)
        return QString("%1 x").arg(_f->calledCount().pretty());
    if (i != IDX_COST)
        return QString();

    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t = ((CallMapView*)widget())->totalCost();

    if (GlobalConfig::showPercentage()) {
        double sum, total = t->subCost(ct);
        if (total == 0.0)
            sum = 100.0;
        else
            sum = 100.0 * _f->inclusive()->subCost(ct) / total;

        return QStringLiteral("%1 %")
                .arg(sum, 0, 'f', GlobalConfig::percentPrecision());
    }
    return _f->inclusive()->prettySubCost(ct);
}


QPixmap CallMapRootItem::pixmap(int i) const
{
    if ((i != IDX_COST) || !_f) return QPixmap();

    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t      = ((CallMapView*)widget())->totalCost();

    // colored level meter with frame
    return costPixmap( ct, _f->inclusive(), (double) (t->subCost(ct)), true);
}


double CallMapRootItem::value() const
{
    if (!_f) return 0.0;

    EventType* ct;
    ct = ((CallMapView*)widget())->eventType();
    return (double) _f->inclusive()->subCost(ct);
}


double CallMapRootItem::sum() const
{
    if (!_f) return 0.0;

    CallMapView* w = (CallMapView*)widget();

    if (w->showCallers())
        return 0.0;
    else
        return (double) _f->inclusive()->subCost(w->eventType());
}


bool CallMapRootItem::isMarked(int) const
{
    return ((CallMapView*)widget())->selectedItem() == _f;
}

TreeMapItemList* CallMapRootItem::children()
{
    if (_f && !initialized()) {
        CallMapView* w = (CallMapView*)widget();

        if (0) qDebug("Create Function %s (%s)",
                      w->showCallers() ? "Callers":"Callees",
                      qPrintable(text(IDX_FUNCNAME)));

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

QColor CallMapRootItem::backColor() const
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
    // create association: if not possible, i.e. an ass. already exists
    // for the function, we need to draw the recursive version
    _recursive = !setFunction(_c->called());
    _valid = true;
#endif
}

QString CallMapCallingItem::text(int textNo) const
{
    if (textNo == IDX_FUNCNAME) {
        if (!_c)
            return QObject::tr("(no call)");

        return _c->calledName();
    }

    if (textNo == IDX_LOCATION)
        return _c->called()->prettyLocation();
    if (textNo == IDX_CALLCOUNT)
        return QString("%1 x").arg(SubCost(_factor * _c->callCount()).pretty());
    if (textNo != IDX_COST)
        return QString();

    EventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    SubCost val = SubCost(_factor * _c->subCost(ct));
    if (GlobalConfig::showPercentage()) {
        // percentage relative to function cost
        ProfileCostArray* t = ((CallMapView*)widget())->totalCost();
        double p  = 100.0 * _factor * _c->subCost(ct) / t->subCost(ct);
        return QStringLiteral("%1 %")
                .arg(p, 0, 'f', GlobalConfig::percentPrecision());
    }
    return val.pretty();
}


QPixmap CallMapCallingItem::pixmap(int i) const
{
    if (i != IDX_COST) return QPixmap();

    // Cost pixmap
    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t = ((CallMapView*)widget())->totalCost();

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

QString CallMapCallerItem::text(int i) const
{
    if (i == IDX_FUNCNAME) {
        if (!_c)
            return QObject::tr("(no call)");

        return _c->callerName();
    }

    if (i == IDX_LOCATION)
        return _c->caller()->prettyLocation();
    if (i == IDX_CALLCOUNT)
        return QString("%1 x").arg(SubCost(_factor * _c->callCount()).pretty());
    if (i != IDX_COST)
        return QString();

    EventType* ct;
    ct = ((CallMapView*)widget())->eventType();

    SubCost val = SubCost(_factor * _c->subCost(ct));
    if (GlobalConfig::showPercentage()) {
        ProfileCostArray* t = ((CallMapView*)widget())->totalCost();
        double p  = 100.0 * _factor * _c->subCost(ct) / t->subCost(ct);
        return QStringLiteral("%1 %")
                .arg(p, 0, 'f', GlobalConfig::percentPrecision());
    }
    return val.pretty();
}

QPixmap CallMapCallerItem::pixmap(int i) const
{
    if (i != IDX_COST) return QPixmap();

    // Cost pixmap
    EventType* ct = ((CallMapView*)widget())->eventType();
    ProfileCostArray* t = ((CallMapView*)widget())->totalCost();

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

    setSplitMode(g->value(QStringLiteral("SplitMode"), QStringLiteral(DEFAULT_SPLITMODE)).toString());

    setFieldVisible(IDX_FUNCNAME,
                    g->value(QStringLiteral("DrawName"), DEFAULT_DRAWNAME).toBool());
    setFieldVisible(IDX_COST,
                    g->value(QStringLiteral("DrawCost"), DEFAULT_DRAWCOST).toBool());
    setFieldVisible(IDX_LOCATION,
                    g->value(QStringLiteral("DrawLocation"), DEFAULT_DRAWLOCATION).toBool());
    setFieldVisible(IDX_CALLCOUNT,
                    g->value(QStringLiteral("DrawCalls"), DEFAULT_DRAWCALLS).toBool());

    bool enable = g->value(QStringLiteral("ForceStrings"), DEFAULT_FORCESTRINGS).toBool();
    setFieldForced(0, enable);
    setFieldForced(1, enable);
    setFieldForced(2, enable);
    setFieldForced(3, enable);

    setAllowRotation(g->value(QStringLiteral("AllowRotation"), DEFAULT_ROTATION).toBool());
    setShadingEnabled(g->value(QStringLiteral("Shading"), DEFAULT_SHADING).toBool());
    setFieldStop(IDX_FUNCNAME,
                 g->value(QStringLiteral("StopName"), QLatin1String(DEFAULT_STOPNAME)).toString());
    setMaxDrawingDepth(g->value(QStringLiteral("MaxDepth"), DEFAULT_MAXDEPTH).toInt());
    setMinimalArea(g->value(QStringLiteral("MaxArea"), DEFAULT_MAXAREA).toInt());

    delete g;
}

void CallMapView::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue(QStringLiteral("SplitMode"), splitModeString(), QStringLiteral(DEFAULT_SPLITMODE));
    g->setValue(QStringLiteral("DrawName"),
                fieldVisible(IDX_FUNCNAME), DEFAULT_DRAWNAME);
    g->setValue(QStringLiteral("DrawCost"),
                fieldVisible(IDX_COST), DEFAULT_DRAWCOST);
    g->setValue(QStringLiteral("DrawLocation"),
                fieldVisible(IDX_LOCATION), DEFAULT_DRAWLOCATION);
    g->setValue(QStringLiteral("DrawCalls"),
                fieldVisible(IDX_CALLCOUNT), DEFAULT_DRAWCALLS);
    // when option for all text (0-3)
    g->setValue(QStringLiteral("ForceStrings"), fieldForced(IDX_FUNCNAME), DEFAULT_FORCESTRINGS);

    g->setValue(QStringLiteral("AllowRotation"), allowRotation(), DEFAULT_ROTATION);
    g->setValue(QStringLiteral("Shading"), isShadingEnabled(), DEFAULT_SHADING);

    g->setValue(QStringLiteral("StopName"), fieldStop(IDX_FUNCNAME), QLatin1String(DEFAULT_STOPNAME));
    g->setValue(QStringLiteral("MaxDepth"), maxDrawingDepth(), DEFAULT_MAXDEPTH);
    g->setValue(QStringLiteral("MaxArea"), minimalArea(), DEFAULT_MAXAREA);

    delete g;
}

#include "moc_callmapview.cpp"
