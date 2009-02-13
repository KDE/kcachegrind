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
 * Tab View, enclosing detailed views for one trace item in
 * two tab widgets, separated by a splitter
 */


#include "tabview.h"

#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QLayout>
#include <QHideEvent>
#include <QMoveEvent>
#include <QEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <Qt3Support/Q3PopupMenu>
#include <Qt3Support/Q3PtrList>

#include "config.h"
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

TabBar::TabBar(TabView* v, QTabWidget* parent, const char *name) :
	QTabBar(parent)
{
	setObjectName(name);
	_tabWidget = parent;
	_tabView = v;
}

void TabBar::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::RightButton) {
		int idx = tabAt(e->pos());
		QWidget* page = 0;
		if (idx >=0) {
			setCurrentIndex(idx);
			page = _tabWidget->widget(idx);
		}
		Q3PopupMenu popup, popup1, popup2, popup3;
		if (page) {
			TraceItemView::Position p = _tabView->tabPosition(page);
			if (p != TraceItemView::Top) {
				popup.insertItem(tr("Move to Top"), 81);
				popup2.insertItem(tr("Move to Top", "Top"), 91);
			}
			if (p != TraceItemView::Right) {
				popup.insertItem(tr("Move to Right"), 82);
				popup2.insertItem(tr("Move to Right", "Right"), 92);
			}
			if (p != TraceItemView::Bottom) {
				popup.insertItem(tr("Move to Bottom"), 83);
				popup2.insertItem(tr("Move to Bottom", "Bottom"), 93);
			}
			if (p != TraceItemView::Left) {
				popup.insertItem(tr("Move to Bottom Left"), 84);
				popup2.insertItem(tr("Move to Bottom Left", "Bottom Left"), 94);
			}
			popup.insertItem(tr("Move Area To"), &popup2, 2);
			popup.insertSeparator();
			popup.insertItem(tr("Hide This Tab"), 80);
			popup.insertItem(tr("Hide Area"), 90);

			if (_tabView->visibleTabs() <2) {
				popup.setItemEnabled(80, false);
				popup.setItemEnabled(90, false);
			} else if (_tabView->visibleAreas() <2)
				popup.setItemEnabled(90, false);
		}
		popup3.insertItem(tr("Show on Top", "Top"), 101);
		popup3.insertItem(tr("Show on Right", "Right"), 102);
		popup3.insertItem(tr("Show on Bottom", "Bottom"), 103);
		popup3.insertItem(tr("Show on Bottom Left", "Bottom Left"), 104);
		popup.insertItem(tr("Show Hidden On"), &popup3, 3);

		int r = popup.exec(mapToGlobal(e->pos() ) );

		TraceItemView::Position p = TraceItemView::Hidden;
		if ((r % 10) == 1)
			p = TraceItemView::Top;
		if ((r % 10) == 2)
			p = TraceItemView::Right;
		if ((r % 10) == 3)
			p = TraceItemView::Bottom;
		if ((r % 10) == 4)
			p = TraceItemView::Left;

		if (r>=80&& r<100)
			_tabView->moveTab(page, p, r>=90);
		if (r>=100&& r<110)
			_tabView->moveTab(0, p, true);
	}

	QTabBar::mousePressEvent(e );
}


//
// Splitter
//

Splitter::Splitter(Qt::Orientation o, QWidget* parent, const char* name)
  : QSplitter(o, parent, name)
{}

void Splitter::moveEvent(QMoveEvent* e)
{
  QSplitter::moveEvent(e);

  if (0) qDebug("Splitter %s: Move", name());
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

TabWidget::TabWidget(TabView* v, QWidget* parent,
                     const char* name, Qt::WFlags f)
    : QTabWidget(parent, name, f)
{
    _hasVisibleRect = false;
    setTabBar(new TabBar(v, this));
}

void TabWidget::checkVisibility()
{
  bool hasVisibleRect = (visibleRect().width()>1) &&
                        (visibleRect().height()>1);

  if (0) qDebug("TabWidget %s: VR (%dx%d) HasVisibleRect: %s => %s",
                name(),
                visibleRect().width(), visibleRect().height(),
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
                name(),
                e->oldSize().width(), e->oldSize().height(),
                e->size().width(), e->size().height());
  checkVisibility();
}

void TabWidget::showEvent(QShowEvent* e)
{
  QTabWidget::showEvent(e);

  if (0) qDebug("TabWidget %s: Show", name());
  checkVisibility();
}

void TabWidget::hideEvent(QHideEvent* e)
{
  QTabWidget::hideEvent(e);

  if (0) qDebug("TabWidget %s: Hide", name());
  checkVisibility();
}

void TabWidget::moveEvent(QMoveEvent* e)
{
  QTabWidget::moveEvent(e);

  if (0) qDebug("TabWidget %s: Move", name());
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

TabView::TabView(TraceItemView* parentView,
                 QWidget* parent, const char* name)
  : QWidget(parent), TraceItemView(parentView)
{
  setFocusPolicy(Qt::StrongFocus);
  setObjectName(name);

  _isCollapsed = true;

  QVBoxLayout* vbox = new QVBoxLayout( this );
  vbox->setSpacing( 6 );
  vbox->setMargin( 6 );

  _nameLabel = new QLabel(this); //KSqueezedTextLabel( this);
  _nameLabel->setSizePolicy(QSizePolicy( QSizePolicy::Ignored,
					 QSizePolicy::Fixed ));
  _nameLabel->setObjectName( "nameLabel" );
  _nameLabel->setText(tr("(No profile data file loaded)"));
  vbox->addWidget( _nameLabel );
  updateNameLabel(tr("(No profile data file loaded)"));

  _mainSplitter   = new QSplitter(Qt::Horizontal, this);
  _leftSplitter   = new Splitter(Qt::Vertical, _mainSplitter, "Left");
  vbox->addWidget( _mainSplitter );

  _rightTW = new TabWidget(this, _mainSplitter, "Right");
  connect(_rightTW, SIGNAL(currentChanged(QWidget*)),
          this, SLOT(tabChanged(QWidget*)));
  connect(_rightTW, SIGNAL(visibleRectChanged(TabWidget*)),
          this, SLOT(visibleRectChangedSlot(TabWidget*)));

  _topTW = new TabWidget(this, _leftSplitter, "Top");
  connect(_topTW, SIGNAL(currentChanged(QWidget*)),
          this, SLOT(tabChanged(QWidget*)));
  connect(_topTW, SIGNAL(visibleRectChanged(TabWidget*)),
          this, SLOT(visibleRectChangedSlot(TabWidget*)));

  _bottomSplitter = new Splitter(Qt::Horizontal,
                                  _leftSplitter, "Bottom");

  _leftTW = new TabWidget(this, _bottomSplitter, "Left");
  _leftTW->setTabPosition(QTabWidget::Bottom);
  connect(_leftTW, SIGNAL(currentChanged(QWidget*)),
          this, SLOT(tabChanged(QWidget*)));
  connect(_leftTW, SIGNAL(visibleRectChanged(TabWidget*)),
          this, SLOT(visibleRectChangedSlot(TabWidget*)));

  _bottomTW = new TabWidget(this, _bottomSplitter, "Bottom");
  _bottomTW->setTabPosition(QTabWidget::Bottom);
  connect(_bottomTW, SIGNAL(currentChanged(QWidget*)),
          this, SLOT(tabChanged(QWidget*)));
  connect(_bottomTW, SIGNAL(visibleRectChanged(TabWidget*)),
          this, SLOT(visibleRectChangedSlot(TabWidget*)));


  // default positions...
  // Keep following order in sync with DEFAULT_xxxTABS defines!
  
  addTop( addTab( tr("Types"),
		  new EventTypeView(this, 0,
				   "EventTypeView")));
  addTop( addTab( tr("Callers"),
		  new CallView(true, this, 0,
			       "CallerView")));
  addTop( addTab( tr("All Callers"),
		  new CoverageView(true, this, 0,
				   "AllCallerView")));
  addTop( addTab( tr("Callee Map"),
		  new CallMapView(false, this, 0,
				  "CalleeMapView")));
  addTop( addTab( tr("Source Code"),
		  new SourceView(this, 0,
				 "SourceView")));

  addBottom( addTab( tr("Parts"),
		     new PartView(this, 0,
				  "PartView")));
  addBottom( addTab( tr("Callees"),
		     new CallView(false, this, 0,
				  "CalleeView")));
  addBottom( addTab( tr("Call Graph"),
		     new CallGraphView(this, 0,
				       "CallGraphView")));
  addBottom( addTab( tr("All Callees"),
		     new CoverageView(false, this, 0,
				      "AllCalleeView")));
  addBottom( addTab( tr("Caller Map"),
		     new CallMapView(true, this, 0,
				     "CallerMapView")));
  addBottom( addTab( tr("Assembly Code"),
		     new InstrView(this, 0,
				   "InstrView")));

  // after all child widgets are created...
  _lastFocus = 0;
  _active = false;
  installFocusFilters();

  updateVisibility();

  this->setWhatsThis( whatsThis() );
}

void TabView::updateNameLabel(QString n)
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
}

void TabView::setData(TraceData* d)
{
  TraceItemView::setData(d);

  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next())
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
  _topTW->insertTab(view->widget(), view->title());
}

void TabView::addBottom(TraceItemView* view)
{
  view->setPosition(TraceItemView::Bottom);
  _bottomTW->insertTab(view->widget(), view->title());
}

TraceItemView::Position TabView::tabPosition(QWidget* w)
{
  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next())
    if (v->widget() == w) return v->position();

  return Hidden;
}

int TabView::visibleTabs()
{
  int c = 0;
  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next()) {
    if (v->position() == Hidden) continue;
    c++;
  }
  return c;
}


int TabView::visibleAreas()
{
  int c = 0, t = 0, b = 0, r = 0, l = 0;
  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next()) {
    switch(v->position()) {
    case TraceItemView::Top:    t++; break;
    case TraceItemView::Bottom: b++; break;
    case TraceItemView::Left:   l++; break;
    case TraceItemView::Right:  r++; break;
    default: break;
    }
  }
  if (t>0) c++;
  if (b>0) c++;
  if (l>0) c++;
  if (r>0) c++;

  return c;
}



// This hides/shows splitters and tabwidgets according to tab children
void TabView::updateVisibility()
{
  // calculate count of tabs in areas
  int t = 0, b = 0, r = 0, l = 0;
  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next()) {
    switch(v->position()) {
    case TraceItemView::Top:    t++; break;
    case TraceItemView::Bottom: b++; break;
    case TraceItemView::Left:   l++; break;
    case TraceItemView::Right:  r++; break;
    default: break;
    }
  }

  if (0) qDebug("TabView::updateVisiblity t %d, b %d, l %d, r %d",
		t, b, l, r);

  QList<int> s;
  s.append(100);


  // children of mainSplitter
  if (_rightTW->isHidden() != (r == 0)) {
    if (r == 0) {
      _rightTW->hide();
    }
    else
      _rightTW->show();
  }
  if (_leftSplitter->isHidden() != (t+b+l == 0)) {
    if (t+b+l == 0) {
      _leftSplitter->hide();
    }
    else
      _leftSplitter->show();
  }

  // children of leftSplitter
  if (_topTW->isHidden() != (t == 0)) {
    if (t == 0) {
      _topTW->hide();
    }
    else
      _topTW->show();
  }

  if (_bottomSplitter->isHidden() != (b+l == 0)) {
    if (b+l == 0) {
      _bottomSplitter->hide();
    }
    else
      _bottomSplitter->show();
  }

  // children of bottomSplitter
  if (_bottomTW->isHidden() != (b == 0)) {
    if (b == 0) {
      _bottomTW->hide();
    }
    else
      _bottomTW->show();
  }
  if (_leftTW->isHidden() != (l == 0)) {
    if (l == 0) {
      _leftTW->hide();
    }
    else
      _leftTW->show();
  }
}

TabWidget* TabView::tabWidget(Position p)
{
  switch(p) {
  case TraceItemView::Top:    return _topTW;
  case TraceItemView::Bottom: return _bottomTW;
  case TraceItemView::Left:   return _leftTW;
  case TraceItemView::Right:  return _rightTW;
  default: break;
  }
  return 0;
}

void TabView::moveTab(QWidget* w, Position p, bool wholeArea)
{
    TraceItemView *v;
    Position origPos = Hidden;
    if (w) {
      for (v=_tabs.first();v;v=_tabs.next())
        if (v->widget() == w) break;

      if (!v) return;
      origPos = v->position();
    }
    if (origPos == p) return;

    TabWidget *from, *to;
    from = tabWidget(origPos);
    to = tabWidget(p);

    Q3PtrList<TraceItemView> tabs;
    for (v=_tabs.first();v;v=_tabs.next())
      if ((v->position() == origPos) &&
          (wholeArea || (v->widget() == w))) tabs.append(v);

    bool isEnabled;
    for (v=tabs.first();v;v=tabs.next()) {
      v->setPosition(p);
      w = v->widget();

      if (from) {
        isEnabled = from->isTabEnabled(w);
        from->removePage(w);
      }
      else isEnabled = (v->canShow(_activeItem)!=0);

      if (to) {
        TraceItemView *vv;
        int idx = -1, i;
        for(vv = _tabs.first(); vv && (vv!=v); vv = _tabs.next()) {
          i = to->indexOf(vv->widget());
          if (i>=0) idx = i;
        }
        to->insertTab(w, v->title(), idx+1);
        to->setTabEnabled(w, isEnabled);
        if (isEnabled) {
          to->showPage(w);
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
		 "current selected function in different tabs: "
		 "<ul>"
		 "<li>The Costs tab shows a list of available event types "
		 "and the inclusive and self costs regarding to these types.</li>"
		 "<li>The Parts tab shows a list of trace parts "
		 "if the trace consists of more than one part (otherwise, "
		 "this tab is hided). "
		 "The cost of the selected function spent in the different "
		 "parts together with the calls happening is shown.</li>"
		 "<li>The Call Lists tab shows direct callers and "
		 "callees of the function in more detail.</li>"
		 "<li>The Coverage tab shows the same is the Call "
		 "Lists tab, but not only direct callers and callees "
		 "but also indirect ones.</li>"
		 "<li>The Call Graph tab shows a graphical "
		 "visualization of the calls done by this function.</li>"
		 "<li>The Source tab presents annotated source code "
		 "if debugging information and the source file "
		 "is available.</li>"
		 "<li>The Assembler tab presents annotated assembler code "
		 "if trace information on instruction level "
		 "is available.</li></ul>"
		 "For more information, see the <em>What's this</em> "
		 "help of the corresponding tab widget</p>");
}

void TabView::installFocusFilters()
{
#if 0
    QObjectList *l = queryList("QWidget");
    QObjectListIt it( *l );
    QObject *obj;

    while ( (obj = it.current()) != 0 ) {
        ++it;
	if ( ((QWidget*)obj)->isFocusEnabled() )
	    obj->installEventFilter(this);
    }
    delete l;
#endif
}


bool TabView::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::FocusIn) {
	_lastFocus = o->isWidgetType() ? (QWidget*) o : 0;
	setActive(_lastFocus != 0);
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

    if (0) qDebug("%s::setActive(%s)", name(), a ? "true":"false");

    if (a) emit tabActivated(this);
}

void TabView::doUpdate(int changeType)
{
    if (changeType & (activeItemChanged |
		      configChanged |
		      dataChanged))
    {
	updateNameLabel( !_data ? tr("(No profile data file loaded)") :
			 !_activeItem ? tr("(No function selected)") :
			 _activeItem->prettyName() );
    }

    // we use our own list iterators because setTabEnabled can
    // invoke tabChanged, which mangles with the lists, too
    bool canShow;
    TraceItemView *v;
    Q3PtrListIterator<TraceItemView> it( _tabs );
    while ( (v=it.current()) != 0) {
      ++it;

      TabWidget *tw = 0;
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
      if (tw->isTabEnabled(v->widget()) != canShow)
        tw->setTabEnabled(v->widget(), canShow);

      if (v->widget() == tw->currentPage())
        v->updateView();
    }
}


void TabView::tabChanged(QWidget* w)
{
  TraceItemView *v;
  for (v=_tabs.first();v;v=_tabs.next())
    if (v->widget() == w) v->updateView();
}

void TabView::visibleRectChangedSlot(TabWidget* tw)
{
  if (0) qDebug("%s: %svisible !",
                tw->name(), tw->hasVisibleRect() ? "":"un");

  if (tw->hasVisibleRect()) doUpdate(0);
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

void TabView::selected(TraceItemView*, ProfileCost* s)
{
    // we set selected item for our own children
    select(s);
    updateView();

    // still forward to parent
    if (_parentView) _parentView->selected(this, s);
}

void TabView::restoreLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    int rightSize = g->value("RightSize", DEFAULT_RIGHTSIZE).toInt();
    int topSize = g->value("TopSize", DEFAULT_TOPSIZE).toInt();
    int leftSize = g->value("LeftSize", DEFAULT_LEFTSIZE).toInt();

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

    QString activeT = g->value("ActiveTop", QString(DEFAULT_ACTIVETOP)).toString();
    QString activeB = g->value("ActiveBottom", QString(DEFAULT_ACTIVEBOTTOM)).toString();
    QString activeL = g->value("ActiveLeft", QString()).toString();
    QString activeR = g->value("ActiveRight", QString()).toString();

    QStringList topTabsDefault, bottomTabsDefault;
    topTabsDefault << DEFAULT_TOPTABS;
    bottomTabsDefault << DEFAULT_BOTTOMTABS;

    QStringList topTabs    = g->value("TopTabs",topTabsDefault).toStringList();
    QStringList bottomTabs = g->value("BottomTabs",bottomTabsDefault).toStringList();
    QStringList leftTabs   = g->value("LeftTabs",QStringList()).toStringList();
    QStringList rightTabs  = g->value("RightTabs",QStringList()).toStringList();

    delete g;

    if (topTabs.isEmpty() && bottomTabs.isEmpty() &&
        rightTabs.isEmpty() && leftTabs.isEmpty()) {
      // no tabs visible ?! Reset to default
	topTabs = topTabsDefault;
	bottomTabs = bottomTabsDefault;
    }

    TraceItemView *activeTop = 0, *activeBottom = 0;
    TraceItemView *activeLeft = 0, *activeRight = 0;

    moveTab(0, TraceItemView::Top, true);
    TraceItemView *v;
    Q3PtrListIterator<TraceItemView> it( _tabs );
    while ( (v=it.current()) != 0) {
      ++it;

      QString n = QString(v->widget()->name());
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
    if (activeTop)   _topTW->showPage(activeTop->widget());
    if (activeBottom)_bottomTW->showPage(activeBottom->widget());
    if (activeLeft)  _leftTW->showPage(activeLeft->widget());
    if (activeRight) _rightTW->showPage(activeRight->widget());

    if (!_data) return;

    updateView();
}

void TabView::saveLayout(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    // convert splitter sizes into percentage numbers
    QList<int> s;
    s = _mainSplitter->sizes();
    int rightSize = 100 * s[1]/(s[0]+s[1]);
    s = _leftSplitter->sizes();
    int topSize = 100 * s[0]/(s[0]+s[1]);
    s = _bottomSplitter->sizes();
    int leftSize = 100 * s[0]/(s[0]+s[1]);

    g->setValue("RightSize", rightSize, DEFAULT_RIGHTSIZE);
    g->setValue("TopSize", topSize, DEFAULT_TOPSIZE);
    g->setValue("LeftSize", leftSize, DEFAULT_LEFTSIZE);

    QString a;
    if ((_topTW->count()>0) &&
        (_topTW->isTabEnabled(_topTW->currentPage())))
      a = QString(_topTW->currentPage()->name());
    g->setValue("ActiveTop", a, QString(DEFAULT_ACTIVETOP));

    a = QString();
    if ((_bottomTW->count()>0) &&
        (_bottomTW->isTabEnabled(_bottomTW->currentPage())))
      a = QString(_bottomTW->currentPage()->name());
    g->setValue("ActiveBottom", a, QString(DEFAULT_ACTIVEBOTTOM));

    a = QString();
    if ((_leftTW->count()>0) &&
        (_leftTW->isTabEnabled(_leftTW->currentPage())))
      a = QString(_leftTW->currentPage()->name());
    g->setValue("ActiveLeft", a, QString());

    a= QString();
    if ((_rightTW->count()>0) &&
        (_rightTW->isTabEnabled(_rightTW->currentPage())))
      a = QString(_rightTW->currentPage()->name());
    g->setValue("ActiveRight", a, QString());

    QStringList topList, bottomList, leftList, rightList;
    TraceItemView *v;
    for (v=_tabs.first();v;v=_tabs.next()) {
      switch(v->position()) {
      case TraceItemView::Top:
        topList << QString(v->widget()->name());
        break;

      case TraceItemView::Bottom:
        bottomList << QString(v->widget()->name());
        break;

      case TraceItemView::Left:
        leftList << QString(v->widget()->name());
        break;

      case TraceItemView::Right:
        rightList << QString(v->widget()->name());
        break;

      default: break;
      }
    }

    QStringList topTabsDefault, bottomTabsDefault;
    topTabsDefault << DEFAULT_TOPTABS;
    bottomTabsDefault << DEFAULT_BOTTOMTABS;

    g->setValue("TopTabs", topList, topTabsDefault);
    g->setValue("BottomTabs", bottomList, bottomTabsDefault);
    g->setValue("LeftTabs", leftList, QStringList());
    g->setValue("RightTabs", rightList, QStringList());

    delete g;
}

void TabView::restoreOptions(const QString& prefix, const QString& postfix)
{
    TraceItemView *v;
    Q3PtrListIterator<TraceItemView> it( _tabs );
    while ( (v=it.current()) != 0) {
      ++it;

      v->restoreOptions(QString("%1-%2").arg(prefix).arg(v->widget()->name()),
			postfix);
    }

    if (!_data) return;

    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    QString activeType = g->value("ActiveItemType", QString()).toString();
    QString activeName = g->value("ActiveItemName", QString()).toString();
    QString selectedType = g->value("SelectedItemType", QString()).toString();
    QString selectedName = g->value("SelectedItemName", QString()).toString();

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
    ProfileCost* selectedItem = _data->search(t, selectedName,
					      _eventType, activeItem);
    if (selectedItem)
	selected(this, selectedItem);

    updateView();
}

void TabView::saveOptions(const QString& prefix, const QString& postfix)
{
    if (_activeItem) {
	ConfigGroup* g = ConfigStorage::group(prefix + postfix);

	g->setValue("ActiveItemType",
		    ProfileContext::typeName(_activeItem->type()));
	g->setValue("ActiveItemName", _activeItem->name());

	if (_selectedItem) {
	    g->setValue("SelectedItemType",
			ProfileContext::typeName(_selectedItem->type()));
	    g->setValue("SelectedItemName", _selectedItem->name());
	}
	delete g;
    }

    TraceItemView *v;
    for (v=_tabs.first();v;v=_tabs.next())
	v->saveOptions(QString("%1-%2").arg(prefix)
		       .arg(v->widget()->name()), postfix);
}

#include "tabview.moc"
