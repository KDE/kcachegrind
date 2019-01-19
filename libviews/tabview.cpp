/* This file is part of KCachegrind.
   Copyright (c) 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Tab View, enclosing detailed views for one trace item in
 * two tab widgets, separated by a splitter
 */


#include "tabview.h"

#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QHideEvent>
#include <QMoveEvent>
#include <QEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QAction>
#include <QMenu>

#include "config.h"
#include "globalconfig.h"
#include "eventtypeview.h"
#include "partview.h"
#include "callview.h"
#include "coverageview.h"
#include "callmapview.h"
#include "instrview.h"
#include "sourceview.h"
#include "callgraphview.h"


// defaults for subviews in TabView

#define DEFAULT_TOPTABS \
    "EventTypeView" << "CallerView" << "AllCallerView" \
    << "CalleeMapView" << "SourceView"
#define DEFAULT_BOTTOMTABS \
    "PartView" << "CalleeView" << "CallGraphView" \
    << "AllCalleeView" << "CallerMapView" << "InstrView"

#define DEFAULT_ACTIVETOP "CallerView"
#define DEFAULT_ACTIVEBOTTOM "CalleeView"

#define DEFAULT_RIGHTSIZE 0
#define DEFAULT_TOPSIZE   50
#define DEFAULT_LEFTSIZE  0

// TabBar

TabBar::TabBar(TabView* v, QTabWidget* parent) :
    QTabBar(parent)
{
    _tabWidget = parent;
    _tabView = v;
}

void TabBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {
        int idx = tabAt(e->pos());
        QWidget* page = nullptr;
        if (idx >=0) {
            setCurrentIndex(idx);
            page = _tabWidget->widget(idx);
        }
        context(page, e->globalPos());
    }
    QTabBar::mousePressEvent(e );
}

void TabBar::context(QWidget* page, const QPoint & pos)
{
    QMenu popup, popup2, popup3;

    QAction* pageToTopAction = nullptr;
    QAction* areaToTopAction = nullptr;
    QAction* showOnTopAction = nullptr;
    QAction* pageToRightAction = nullptr;
    QAction* areaToRightAction = nullptr;
    QAction* showOnRightAction = nullptr;
    QAction* pageToBottomAction = nullptr;
    QAction* areaToBottomAction = nullptr;
    QAction* showOnBottomAction = nullptr;
    QAction* pageToLeftAction = nullptr;
    QAction* areaToLeftAction = nullptr;
    QAction* showOnLeftAction = nullptr;
    QAction* hidePageAction = nullptr;
    QAction* hideAreaAction = nullptr;

    if (page) {
        TraceItemView::Position p = _tabView->tabPosition(page);
        if (p != TraceItemView::Top) {
            pageToTopAction = popup.addAction(tr("Move to Top"));
            areaToTopAction = popup2.addAction(tr("Top", "Move to Top"));
        }
        if (p != TraceItemView::Right) {
            pageToRightAction = popup.addAction(tr("Move to Right"));
            areaToRightAction = popup2.addAction(tr("Right", "Move to Right"));
        }
        if (p != TraceItemView::Bottom) {
            pageToBottomAction = popup.addAction(tr("Move to Bottom"));
            areaToBottomAction = popup2.addAction(tr("Bottom", "Move to Bottom"));
        }
        if (p != TraceItemView::Left) {
            pageToLeftAction = popup.addAction(tr("Move to Bottom Left"));
            areaToLeftAction = popup2.addAction(tr("Bottom Left", "Move to Bottom Left"));
        }
        popup2.setTitle(tr("Move Area To"));
        popup.addMenu(&popup2);
        popup.addSeparator();
        hidePageAction = popup.addAction(tr("Hide This Tab"));
        hideAreaAction = popup.addAction(tr("Hide Area"));

        if (_tabView->visibleTabs() <2) {
            hidePageAction->setEnabled(false);
            hideAreaAction->setEnabled(false);
        } else if (_tabView->visibleAreas() <2)
            hideAreaAction->setEnabled(false);
    }

    showOnTopAction = popup3.addAction(tr("Top", "Show on Top"));
    showOnRightAction = popup3.addAction(tr("Right", "Show on Right"));
    showOnBottomAction = popup3.addAction(tr("Bottom", "Show on Bottom"));
    showOnLeftAction = popup3.addAction(tr("Bottom Left", "Show on Bottom Left"));
    popup3.setTitle(tr("Show Hidden On"));
    popup.addMenu(&popup3);

    QAction* a = popup.exec(pos);
    if (a == hidePageAction)
        _tabView->moveTab(page, TraceItemView::Hidden, false);
    else if (a == hideAreaAction)
        _tabView->moveTab(page, TraceItemView::Hidden, true);

    else if (a == pageToTopAction)
        _tabView->moveTab(page, TraceItemView::Top, false);
    else if (a == pageToRightAction)
        _tabView->moveTab(page, TraceItemView::Right, false);
    else if (a == pageToBottomAction)
        _tabView->moveTab(page, TraceItemView::Bottom, false);
    else if (a == pageToLeftAction)
        _tabView->moveTab(page, TraceItemView::Left, false);

    else if (a == areaToTopAction)
        _tabView->moveTab(page, TraceItemView::Top, true);
    else if (a == areaToRightAction)
        _tabView->moveTab(page, TraceItemView::Right, true);
    else if (a == areaToBottomAction)
        _tabView->moveTab(page, TraceItemView::Bottom, true);
    else if (a == areaToLeftAction)
        _tabView->moveTab(page, TraceItemView::Left, true);

    else if (a == showOnTopAction)
        _tabView->moveTab(nullptr, TraceItemView::Top, true);
    else if (a == showOnRightAction)
        _tabView->moveTab(nullptr, TraceItemView::Right, true);
    else if (a == showOnBottomAction)
        _tabView->moveTab(nullptr, TraceItemView::Bottom, true);
    else if (a == showOnLeftAction)
        _tabView->moveTab(nullptr, TraceItemView::Left, true);
}


//
// Splitter
//

Splitter::Splitter(Qt::Orientation o, QWidget* parent)
    : QSplitter(o, parent)
{}

void Splitter::moveEvent(QMoveEvent* e)
{
    QSplitter::moveEvent(e);

    if (0) qDebug("Splitter %s: Move", qPrintable(objectName()));
    checkVisiblity();
}

void Splitter::checkVisiblity()
{
#if 0
    const QObjectList l = children();
    QObjectList::Iterator it( l );
    QObject *obj;
    while ( (obj = it.current()) != 0 ) {
        ++it;
        if (obj->isA("Splitter")) ((Splitter*)obj)->checkVisiblity();
        else if (obj->isA("TabWidget")) ((TabWidget*)obj)->checkVisibility();
    }
#endif
}




//
// TabWidget
//

TabWidget::TabWidget(TabView* v, QWidget* parent)
    : QTabWidget(parent)
{
    _hasVisibleRect = false;
    setTabBar(new TabBar(v, this));
}

void TabWidget::checkVisibility()
{
    bool hasVisibleRect = (visibleRegion().boundingRect().width()>1) &&
                          (visibleRegion().boundingRect().height()>1);

    if (0) qDebug("TabWidget %s: VR (%dx%d) HasVisibleRect: %s => %s",
                  qPrintable(objectName()),
                  visibleRegion().boundingRect().width(),
                  visibleRegion().boundingRect().height(),
                  _hasVisibleRect ? "Yes":"No",
                  hasVisibleRect ? "Yes":"No");

    if (hasVisibleRect != _hasVisibleRect) {
        _hasVisibleRect = hasVisibleRect;
        emit visibleRectChanged(this);
    }
}

void TabWidget::resizeEvent(QResizeEvent *e)
{
    QTabWidget::resizeEvent(e);
    if (0) qDebug("TabWidget %s:\n  Resize from (%d/%d) to (%d/%d)",
                  objectName().toLatin1().constData(),
                  e->oldSize().width(), e->oldSize().height(),
                  e->size().width(), e->size().height());
    checkVisibility();
}

void TabWidget::showEvent(QShowEvent* e)
{
    QTabWidget::showEvent(e);

    if (0) qDebug("TabWidget %s: Show", objectName().toLatin1().constData());
    checkVisibility();
}

void TabWidget::hideEvent(QHideEvent* e)
{
    QTabWidget::hideEvent(e);

    if (0) qDebug("TabWidget %s: Hide", objectName().toLatin1().constData());
    checkVisibility();
}

void TabWidget::moveEvent(QMoveEvent* e)
{
    QTabWidget::moveEvent(e);

    if (0) qDebug("TabWidget %s: Move", objectName().toLatin1().constData());
    checkVisibility();
}



//
// TabView
//

/*
 * Areas for child views
 *
 * leftSplitter
 * |
 * |    -----                 -----
 * |  _/     \_______________/     \____
 * |  | Top                 | Right    |
 * |  |                     |          |
 * -> |---------------------|          |
 *    | BottomLeft | Bottom |          |
 *    |            |        |          |
 *    -\_____/------\____/--------------
 *
 *                 ^        ^
 *      bottomSplitter    mainSplitter
 */

TabView::TabView(TraceItemView* parentView, QWidget* parent)
    : QWidget(parent), TraceItemView(parentView)
{
    setFocusPolicy(Qt::StrongFocus);

    _isCollapsed = true;

    QVBoxLayout* vbox = new QVBoxLayout( this );
    vbox->setSpacing( 6 );
    vbox->setMargin( 6 );

    _nameLabel = new QLabel(this); //KSqueezedTextLabel( this);
    _nameLabel->setSizePolicy(QSizePolicy( QSizePolicy::Ignored,
                                           QSizePolicy::Fixed ));
    _nameLabel->setObjectName( QStringLiteral("nameLabel") );
    _nameLabel->setText(tr("(No profile data file loaded)"));
    vbox->addWidget( _nameLabel );
    updateNameLabel(tr("(No profile data file loaded)"));

    _mainSplitter   = new QSplitter(Qt::Horizontal, this);
    _leftSplitter   = new Splitter(Qt::Vertical, _mainSplitter);
    _leftSplitter->setObjectName(QStringLiteral("Left"));
    vbox->addWidget( _mainSplitter );

    _rightTW = new TabWidget(this, _mainSplitter);
    _rightTW->setObjectName(QStringLiteral("Right"));
    connect(_rightTW, &QTabWidget::currentChanged,
            this, &TabView::tabChanged);
    connect(_rightTW, &TabWidget::visibleRectChanged,
            this, &TabView::visibleRectChangedSlot);

    _topTW = new TabWidget(this, _leftSplitter);
    _topTW->setObjectName(QStringLiteral("Top"));
    connect(_topTW, &QTabWidget::currentChanged,
            this, &TabView::tabChanged);
    connect(_topTW, &TabWidget::visibleRectChanged,
            this, &TabView::visibleRectChangedSlot);

    _bottomSplitter = new Splitter(Qt::Horizontal, _leftSplitter);
    _bottomSplitter->setObjectName(QStringLiteral("Bottom"));

    _leftTW = new TabWidget(this, _bottomSplitter);
    _leftTW->setObjectName(QStringLiteral("Left"));
    _leftTW->setTabPosition(QTabWidget::South);
    connect(_leftTW, &QTabWidget::currentChanged,
            this, &TabView::tabChanged);
    connect(_leftTW, &TabWidget::visibleRectChanged,
            this, &TabView::visibleRectChangedSlot);

    _bottomTW = new TabWidget(this, _bottomSplitter);
    _bottomTW->setObjectName(QStringLiteral("Bottom"));
    _bottomTW->setTabPosition(QTabWidget::South);
    connect(_bottomTW, &QTabWidget::currentChanged,
            this, &TabView::tabChanged);
    connect(_bottomTW, &TabWidget::visibleRectChanged,
            this, &TabView::visibleRectChangedSlot);

    CallView* callerView = new CallView(true, this);
    CallView* calleeView = new CallView(false, this);
    CoverageView * allCallerView = new CoverageView(true, this);
    CoverageView * allCalleeView = new CoverageView(false, this);
    SourceView* sourceView = new SourceView(this);
    InstrView* instrView = new InstrView(this);
    PartView* partView = new PartView(this);

    // Options of visualization views are stored by their view name
    callerView->setObjectName(QStringLiteral("CallerView"));
    calleeView->setObjectName(QStringLiteral("CalleeView"));
    allCallerView->setObjectName(QStringLiteral("AllCallerView"));
    allCalleeView->setObjectName(QStringLiteral("AllCalleeView"));
    sourceView->setObjectName(QStringLiteral("SourceView"));
    instrView->setObjectName(QStringLiteral("InstrView"));
    partView->setObjectName(QStringLiteral("PartView"));

    // default positions...
    // Keep following order in sync with DEFAULT_xxxTABS defines!

    addTop( addTab( tr("Types"),
                    new EventTypeView(this, nullptr,
                                      "EventTypeView")));
    addTop( addTab( tr("Callers"), callerView) );
    addTop( addTab( tr("All Callers"), allCallerView) );
    addTop( addTab( tr("Callee Map"),
                    new CallMapView(false, this, nullptr,
                                    "CalleeMapView")));
    addTop( addTab( tr("Source Code"), sourceView) );

    addBottom( addTab( tr("Parts"), partView ) );
    addBottom( addTab( tr("Callees"), calleeView) );
    addBottom( addTab( tr("Call Graph"),
                       new CallGraphView(this, nullptr,
                                         "CallGraphView")));
    addBottom( addTab( tr("All Callees"), allCalleeView) );
    addBottom( addTab( tr("Caller Map"),
                       new CallMapView(true, this, nullptr,
                                       "CallerMapView")));
    addBottom( addTab( tr("Machine Code"), instrView) );

    // after all child widgets are created...
    _lastFocus = nullptr;
    _active = false;
    installFocusFilters();

    updateVisibility();

    this->setWhatsThis( whatsThis() );
}

void TabView::updateNameLabel(const QString& n)
{
    QFontMetrics fm(_nameLabel->fontMetrics());

    if (!n.isNull()) {
        _nameLabelText = n;
        _textWidth = fm.width(_nameLabelText);
    }

    int labelWidth = _nameLabel->size().width();
    if (_textWidth > labelWidth) {
        _nameLabel->setText(fm.elidedText(_nameLabelText,
                                          Qt::ElideMiddle, labelWidth));
        _nameLabel->setToolTip(_nameLabelText);
    }
    else {
        _nameLabel->setText(_nameLabelText);
        _nameLabel->setToolTip(QString());
    }
    if (!_nameLabelTooltip.isEmpty())
        _nameLabel->setToolTip(_nameLabelTooltip);
}

void TabView::setData(TraceData* d)
{
    TraceItemView::setData(d);

    foreach(TraceItemView* v, _tabs)
        v->setData(d);
}

TraceItemView* TabView::addTab(const QString& label, TraceItemView* view)
{
    view->setTitle(label);
    _tabs.append(view);
    return view;
}

void TabView::addTop(TraceItemView* view)
{
    view->setPosition(TraceItemView::Top);
    _topTW->addTab(view->widget(), view->title());
}

void TabView::addBottom(TraceItemView* view)
{
    view->setPosition(TraceItemView::Bottom);
    _bottomTW->addTab(view->widget(), view->title());
}

TraceItemView::Position TabView::tabPosition(QWidget* w)
{
    foreach(TraceItemView* v, _tabs)
        if (v->widget() == w) return v->position();

    return Hidden;
}

int TabView::visibleTabs()
{
    int c = 0;
    foreach(TraceItemView* v, _tabs) {
        if (v->position() == Hidden) continue;
        c++;
    }
    return c;
}

// calculate count of tabs in areas
void TabView::tabCounts(int& top, int& bottom,
                        int& left, int& right)
{
    top = bottom = left = right = 0;

    foreach(TraceItemView* v, _tabs) {
        switch(v->position()) {
        case TraceItemView::Top:
            top++;
            break;
        case TraceItemView::Bottom:
            bottom++;
            break;
        case TraceItemView::Left:
            left++;
            break;
        case TraceItemView::Right:
            right++;
            break;
        default:
            break;
        }
    }

    if (0) qDebug("TabView::tabCounts top %d, bottom %d, left %d, right %d",
                  top, bottom, left, right);
}

int TabView::visibleAreas()
{
    int count, top, bottom, left, right;

    tabCounts(top, bottom, left, right);
    count = 0;
    if (top>0) count++;
    if (bottom>0) count++;
    if (left>0) count++;
    if (right>0) count++;

    return count;
}

// This hides/shows splitters and tabwidgets according to tab children
void TabView::updateVisibility()
{
    int top, bottom, left, right;

    tabCounts(top, bottom, left, right);

    QList<int> s;
    s.append(100);

    // children of mainSplitter
    if (_rightTW->isHidden() != (right == 0)) {
        if (right == 0) {
            _rightTW->hide();
        }
        else
            _rightTW->show();
    }
    if (_leftSplitter->isHidden() != (top+bottom+left == 0)) {
        if (top+bottom+left == 0) {
            _leftSplitter->hide();
        }
        else
            _leftSplitter->show();
    }

    // children of leftSplitter
    if (_topTW->isHidden() != (top == 0)) {
        if (top == 0) {
            _topTW->hide();
        }
        else
            _topTW->show();
    }

    if (_bottomSplitter->isHidden() != (bottom+left == 0)) {
        if (bottom+left == 0) {
            _bottomSplitter->hide();
        }
        else
            _bottomSplitter->show();
    }

    // children of bottomSplitter
    if (_bottomTW->isHidden() != (bottom == 0)) {
        if (bottom == 0) {
            _bottomTW->hide();
        }
        else
            _bottomTW->show();
    }
    if (_leftTW->isHidden() != (left == 0)) {
        if (left == 0) {
            _leftTW->hide();
        }
        else
            _leftTW->show();
    }
}

TabWidget* TabView::tabWidget(Position p)
{
    switch(p) {
    case TraceItemView::Top:
        return _topTW;
    case TraceItemView::Bottom:
        return _bottomTW;
    case TraceItemView::Left:
        return _leftTW;
    case TraceItemView::Right:
        return _rightTW;
    default:
        break;
    }
    return nullptr;
}

void TabView::moveTab(QWidget* w, Position p, bool wholeArea)
{
    Position origPos = Hidden;
    if (w) {
        TraceItemView* found = nullptr;
        foreach(TraceItemView* v, _tabs)
            if (v->widget() == w) { found = v; break; }

        if (!found) return;
        origPos = found->position();
    }
    if (origPos == p) return;

    TabWidget *from, *to;
    from = tabWidget(origPos);
    to = tabWidget(p);

    QList<TraceItemView*> tabs;
    foreach(TraceItemView* v, _tabs)
        if ((v->position() == origPos) &&
            (wholeArea || (v->widget() == w))) tabs.append(v);

    bool isEnabled;
    foreach(TraceItemView* v, tabs) {
        v->setPosition(p);
        w = v->widget();

        if (from) {
            isEnabled = from->isTabEnabled(from->indexOf(w));
            from->removeTab(from->indexOf(w));
        }
        else isEnabled = (v->canShow(_activeItem)!=nullptr);

        if (to) {
            int idx = -1, i;
            foreach(TraceItemView* vv, _tabs) {
                if (v == vv) continue;
                i = to->indexOf(vv->widget());
                if (i>=0) idx = i;
            }
            to->insertTab(idx+1, w, v->title());
            to->setTabEnabled(to->indexOf(w), isEnabled);
            if (isEnabled) {
                to->setCurrentIndex(to->indexOf(w));
                v->updateView();
            }
        }
    }
    updateVisibility();
}


QString TabView::whatsThis() const
{
    return tr( "<b>Information Tabs</b>"
               "<p>This widget shows information for the "
               "currently selected function in different tabs: "
               "<ul>"
               "<li>The Costs tab shows a list of available event types "
               "and the inclusive and self-costs related to these types.</li>"
               "<li>The Parts tab shows a list of trace parts "
               "if the trace consists of more than one part (otherwise, "
               "this tab is hidden). "
               "The cost of the selected function spent in the different "
               "parts together with the calls happening is shown.</li>"
               "<li>The Call Lists tab shows direct callers and "
               "callees of the function in more detail.</li>"
               "<li>The Coverage tab shows the same as the Call "
               "Lists tab, but also shows indirect callers and callees, "
               "not just direct ones.</li>"
               "<li>The Call Graph tab shows a graphical "
               "visualization of the calls made by this function.</li>"
               "<li>The Source Code tab presents annotated source code "
               "if debugging information and the source file "
               "are available.</li>"
               "<li>The Machine Code tab presents annotated assembly "
               "instructions if profile information at instruction level "
               "is available.</li></ul>"
               "For more information, see the <em>What's This?</em> "
               "help of the corresponding tab widget.</p>");
}

void TabView::installFocusFilters()
{
    QList<QWidget*> wList = findChildren<QWidget*>();
    foreach(QWidget* w, wList) {
        if (w->focusPolicy() != Qt::NoFocus)
            w->installEventFilter(this);
    }
}


bool TabView::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::FocusIn) {
        _lastFocus = o->isWidgetType() ? (QWidget*) o : nullptr;
        setActive(_lastFocus != nullptr);
    }
    return QWidget::eventFilter(o,e);
}

void TabView::mousePressEvent(QMouseEvent*)
{
    if (_lastFocus)
        _lastFocus->setFocus();
    setActive(true);
}

void TabView::setActive(bool a)
{
    if (a == _active) return;
    _active = a;

    QFont nameLabel_font(  _nameLabel->font() );
    nameLabel_font.setBold(a);
    _nameLabel->setFont( nameLabel_font );
    // force recalculation of label width by passing current label text
    updateNameLabel(_nameLabelText);

    if (0) qDebug("%s::setActive(%s)", objectName().toLatin1().constData(),
                  a ? "true":"false");

    if (a) emit tabActivated(this);
}

void TabView::doUpdate(int changeType, bool force)
{
    if (changeType & (activeItemChanged |
                      configChanged |
                      dataChanged))
    {
        if (_data && _activeItem) {
            _nameLabelTooltip = _activeItem->formattedName();
            updateNameLabel(_activeItem->prettyName());
        }
        else {
            _nameLabelTooltip = QString();
            updateNameLabel( !_data ?
                                 tr("(No profile data file loaded)") :
                                 tr("(No function selected)"));
        }
    }

    bool canShow;
    foreach(TraceItemView *v, _tabs) {

        TabWidget *tw = nullptr;
        switch(v->position()) {
        case TraceItemView::Top:    tw = _topTW; break;
        case TraceItemView::Bottom: tw = _bottomTW; break;
        case TraceItemView::Left:   tw = _leftTW; break;
        case TraceItemView::Right:  tw = _rightTW; break;
        default: break;
        }

        // update even if hidden
        if (tw) {
            if (!tw->hasVisibleRect()) continue;
        }
        canShow = v->set(changeType, _data, _eventType, _eventType2,
                         _groupType, _partList,
                         _activeItem, _selectedItem);
        v->notifyChange(changeType);

        if (!tw) continue;
        int idx = tw->indexOf(v->widget());
        if (tw->isTabEnabled(idx) != canShow)
            tw->setTabEnabled(idx, canShow);

        if (v->widget() == tw->currentWidget())
            v->updateView(force);
    }
}


void TabView::tabChanged(int i)
{
    TabWidget* tw = qobject_cast<TabWidget*>(sender());
    if (!tw) return;
    QWidget* w = tw->widget(i);

    foreach(TraceItemView *v, _tabs)
        if (v->widget() == w) v->updateView();
}

void TabView::visibleRectChangedSlot(TabWidget* tw)
{
    if (0) qDebug("%s: %svisible !",
                  tw->objectName().toLatin1().constData(),
                  tw->hasVisibleRect() ? "":"un");

    if (tw->hasVisibleRect()) doUpdate(0, false);
}

void TabView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    updateNameLabel();

    bool collapsed = (e->size().width()<=1) || (e->size().height()<=1);
    if (_isCollapsed != collapsed) {
        _isCollapsed = collapsed;
        updateView();
    }

    if (0) qDebug("TabView::Resize from (%d/%d) to (%d/%d)",
                  e->oldSize().width(), e->oldSize().height(),
                  e->size().width(), e->size().height());
}

void TabView::selected(TraceItemView*, CostItem* s)
{
    // we set selected item for our own children
    select(s);

    // still forward to parent
    if (_parentView) _parentView->selected(this, s);
}

void TabView::restoreLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    int rightSize = g->value(QStringLiteral("RightSize"), DEFAULT_RIGHTSIZE).toInt();
    int topSize = g->value(QStringLiteral("TopSize"), DEFAULT_TOPSIZE).toInt();
    int leftSize = g->value(QStringLiteral("LeftSize"), DEFAULT_LEFTSIZE).toInt();

    QList<int> mainSizes, leftSizes, bottomSizes;

    int mainWidth = _mainSplitter->width();
    mainSizes << (100 - rightSize)*mainWidth/100 << rightSize*mainWidth/100;
    _mainSplitter->setSizes(mainSizes);

    int leftHeight = _leftSplitter->height();
    leftSizes << topSize*leftHeight/100 << (100 - topSize)*leftHeight/100;
    _leftSplitter->setSizes(leftSizes);

    int bottomWidth = _bottomSplitter->width();
    bottomSizes << leftSize*bottomWidth/100 << (100 - leftSize)*bottomWidth/100;
    _bottomSplitter->setSizes(bottomSizes);

    QString activeT = g->value(QStringLiteral("ActiveTop"), QStringLiteral(DEFAULT_ACTIVETOP)).toString();
    QString activeB = g->value(QStringLiteral("ActiveBottom"), QStringLiteral(DEFAULT_ACTIVEBOTTOM)).toString();
    QString activeL = g->value(QStringLiteral("ActiveLeft"), QString()).toString();
    QString activeR = g->value(QStringLiteral("ActiveRight"), QString()).toString();

    QStringList topTabsDefault, bottomTabsDefault;
    topTabsDefault << DEFAULT_TOPTABS;
    bottomTabsDefault << DEFAULT_BOTTOMTABS;

    QStringList topTabs    = g->value(QStringLiteral("TopTabs"),topTabsDefault).toStringList();
    QStringList bottomTabs = g->value(QStringLiteral("BottomTabs"),bottomTabsDefault).toStringList();
    QStringList leftTabs   = g->value(QStringLiteral("LeftTabs"),QStringList()).toStringList();
    QStringList rightTabs  = g->value(QStringLiteral("RightTabs"),QStringList()).toStringList();

    delete g;

    if (topTabs.isEmpty() && bottomTabs.isEmpty() &&
        rightTabs.isEmpty() && leftTabs.isEmpty()) {
        // no tabs visible ?! Reset to default
        topTabs = topTabsDefault;
        bottomTabs = bottomTabsDefault;
    }

    TraceItemView *activeTop = nullptr, *activeBottom = nullptr;
    TraceItemView *activeLeft = nullptr, *activeRight = nullptr;

    moveTab(nullptr, TraceItemView::Top, true);
    foreach(TraceItemView *v, _tabs) {
        QString n = v->widget()->objectName();
        if (topTabs.contains(n)) {
            moveTab(v->widget(), TraceItemView::Top);
            if (n == activeT) activeTop = v;
        }
        else if (bottomTabs.contains(n)) {
            moveTab(v->widget(), TraceItemView::Bottom);
            if (n == activeB) activeBottom = v;
        }
        else if (leftTabs.contains(n)) {
            moveTab(v->widget(), TraceItemView::Left);
            if (n == activeL) activeLeft = v;
        }
        else if (rightTabs.contains(n)) {
            moveTab(v->widget(), TraceItemView::Right);
            if (n == activeR) activeRight = v;
        }
        else moveTab(v->widget(), Hidden);
    }
    if (activeTop)
        _topTW->setCurrentIndex(_topTW->indexOf(activeTop->widget()));
    if (activeBottom)
        _bottomTW->setCurrentIndex(_bottomTW->indexOf(activeBottom->widget()));
    if (activeLeft)
        _leftTW->setCurrentIndex(_leftTW->indexOf(activeLeft->widget()));
    if (activeRight)
        _rightTW->setCurrentIndex(_rightTW->indexOf(activeRight->widget()));

    if (!_data) return;

    updateView();
}

void TabView::saveLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    // convert splitter sizes into percentage numbers
    QList<int> s;
    s = _mainSplitter->sizes();
    int rightSize = (s[0]+s[1]==0) ? 0 : (100 * s[1]/(s[0]+s[1]));
    s = _leftSplitter->sizes();
    int topSize = (s[0]+s[1]==0) ? 0 : (100 * s[0]/(s[0]+s[1]));
    s = _bottomSplitter->sizes();
    int leftSize = (s[0]+s[1]==0) ? 0 : (100 * s[0]/(s[0]+s[1]));

    g->setValue(QStringLiteral("RightSize"), rightSize, DEFAULT_RIGHTSIZE);
    g->setValue(QStringLiteral("TopSize"), topSize, DEFAULT_TOPSIZE);
    g->setValue(QStringLiteral("LeftSize"), leftSize, DEFAULT_LEFTSIZE);

    QString a;
    QWidget* w;
    w = _topTW->currentWidget();
    if ((_topTW->count()>0) &&
        (_topTW->isTabEnabled(_topTW->indexOf(w))))
        a = w->objectName();
    g->setValue(QStringLiteral("ActiveTop"), a, QStringLiteral(DEFAULT_ACTIVETOP));

    a = QString();
    w = _bottomTW->currentWidget();
    if ((_bottomTW->count()>0) &&
        (_bottomTW->isTabEnabled(_bottomTW->indexOf(w))))
        a = w->objectName();
    g->setValue(QStringLiteral("ActiveBottom"), a, QStringLiteral(DEFAULT_ACTIVEBOTTOM));

    a = QString();
    w = _leftTW->currentWidget();
    if ((_leftTW->count()>0) &&
        (_leftTW->isTabEnabled(_leftTW->indexOf(w))))
        a = w->objectName();
    g->setValue(QStringLiteral("ActiveLeft"), a, QString());

    a= QString();
    w = _rightTW->currentWidget();
    if ((_rightTW->count()>0) &&
        (_rightTW->isTabEnabled(_rightTW->indexOf(w))))
        a = w->objectName();
    g->setValue(QStringLiteral("ActiveRight"), a, QString());

    QStringList topList, bottomList, leftList, rightList;
    foreach(TraceItemView *v, _tabs) {
        switch(v->position()) {
        case TraceItemView::Top:
            topList << v->widget()->objectName();
            break;

        case TraceItemView::Bottom:
            bottomList << v->widget()->objectName();
            break;

        case TraceItemView::Left:
            leftList << v->widget()->objectName();
            break;

        case TraceItemView::Right:
            rightList << v->widget()->objectName();
            break;

        default: break;
        }
    }

    QStringList topTabsDefault, bottomTabsDefault;
    topTabsDefault << DEFAULT_TOPTABS;
    bottomTabsDefault << DEFAULT_BOTTOMTABS;

    g->setValue(QStringLiteral("TopTabs"), topList, topTabsDefault);
    g->setValue(QStringLiteral("BottomTabs"), bottomList, bottomTabsDefault);
    g->setValue(QStringLiteral("LeftTabs"), leftList, QStringList());
    g->setValue(QStringLiteral("RightTabs"), rightList, QStringList());

    delete g;
}

void TabView::restoreOptions(const QString& prefix, const QString& postfix)
{
    foreach(TraceItemView *v, _tabs)
        v->restoreOptions(QStringLiteral("%1-%2").arg(prefix).arg(v->widget()->objectName()),
                          postfix);

    if (!_data) return;

    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    QString activeType = g->value(QStringLiteral("ActiveItemType"), QString()).toString();
    QString activeName = g->value(QStringLiteral("ActiveItemName"), QString()).toString();
    QString selectedType = g->value(QStringLiteral("SelectedItemType"), QString()).toString();
    QString selectedName = g->value(QStringLiteral("SelectedItemName"), QString()).toString();

    delete g;

    // restore active item
    ProfileContext::Type t = ProfileContext::type(activeType);
    if (t==ProfileContext::InvalidType) t = ProfileContext::Function;
    ProfileCostArray* activeItem = _data->search(t, activeName, _eventType);
    if (!activeItem) return;
    activated(activeItem);

    // restore selected item
    t = ProfileContext::type(selectedType);
    if (t==ProfileContext::InvalidType) t = ProfileContext::Function;
    ProfileCostArray* selectedItem;
    selectedItem = _data->search(t, selectedName, _eventType, activeItem);
    if (selectedItem)
        selected(this, selectedItem);
}

void TabView::saveOptions(const QString& prefix, const QString& postfix)
{
    if (_activeItem) {
        ConfigGroup* g = ConfigStorage::group(prefix + postfix);

        g->setValue(QStringLiteral("ActiveItemType"),
                    ProfileContext::typeName(_activeItem->type()));
        g->setValue(QStringLiteral("ActiveItemName"), _activeItem->name());

        if (_selectedItem) {
            g->setValue(QStringLiteral("SelectedItemType"),
                        ProfileContext::typeName(_selectedItem->type()));
            g->setValue(QStringLiteral("SelectedItemName"), _selectedItem->name());
        }
        delete g;
    }

    foreach(TraceItemView *v, _tabs)
        v->saveOptions(QStringLiteral("%1-%2").arg(prefix)
                       .arg(v->widget()->objectName()), postfix);
}

