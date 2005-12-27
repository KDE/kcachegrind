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
 * Tab View, enclosing detailed views for one trace item in
 * two tab widgets, separated by a splitter
 */

#include <qobjectlist.h>
#include <qsplitter.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qwhatsthis.h>
#include <qpopupmenu.h>

#include <klocale.h>
#include <kconfig.h>

#include "tabview.h"
#include "costtypeview.h"
#include "partview.h"
#include "callview.h"
#include "coverageview.h"
#include "callmapview.h"
#include "instrview.h"
#include "sourceview.h"
#include "callgraphview.h"

// TabBar

TabBar::TabBar(TabView* v, QTabWidget* parent, const char *name)
    : QTabBar(parent, name)
{
    _tabWidget = parent;
    _tabView = v;
}

void TabBar::mousePressEvent(QMouseEvent *e)
{
  if (e->button() == RightButton) {
      QTab *tab = selectTab( e->pos() );
      QWidget* page;
      page = tab ? _tabWidget->page( indexOf( tab->identifier() ) ) :0;

      QPopupMenu popup, popup1, popup2, popup3;
      if (page) {
        TraceItemView::Position p = _tabView->tabPosition(page);
        if (p != TraceItemView::Top) {
          popup.insertItem(i18n("Move to Top"), 81);
          popup2.insertItem(i18n("Top"), 91);
        }
        if (p != TraceItemView::Right) {
          popup.insertItem(i18n("Move to Right"), 82);
          popup2.insertItem(i18n("Right"), 92);
        }
        if (p != TraceItemView::Bottom) {
          popup.insertItem(i18n("Move to Bottom"), 83);
          popup2.insertItem(i18n("Bottom"), 93);
        }
        if (p != TraceItemView::Left) {
          popup.insertItem(i18n("Move to Bottom Left"), 84);
          popup2.insertItem(i18n("Bottom Left"), 94);
        }
        popup.insertItem(i18n("Move Area To"), &popup2, 2);
        popup.insertSeparator();
        popup.insertItem(i18n("Hide This Tab"), 80);
        popup.insertItem(i18n("Hide Area"), 90);

        if (_tabView->visibleTabs() <2) {
          popup.setItemEnabled(80, false);
          popup.setItemEnabled(90, false);
        }
        else if (_tabView->visibleAreas() <2)
          popup.setItemEnabled(90, false);
      }
      popup3.insertItem(i18n("Top"), 101);
      popup3.insertItem(i18n("Right"), 102);
      popup3.insertItem(i18n("Bottom"), 103);
      popup3.insertItem(i18n("Bottom Left"), 104);
      popup.insertItem(i18n("Show Hidden On"), &popup3, 3);

      int r = popup.exec( mapToGlobal( e->pos() ) );

      TraceItemView::Position p = TraceItemView::Hidden;
      if ((r % 10) == 1) p = TraceItemView::Top;
      if ((r % 10) == 2) p = TraceItemView::Right;
      if ((r % 10) == 3) p = TraceItemView::Bottom;
      if ((r % 10) == 4) p = TraceItemView::Left;

      if (r>=80 && r<100) _tabView->moveTab(page, p, r>=90);
      if (r>=100 && r<110) _tabView->moveTab(0, p, true);
  }

  QTabBar::mousePressEvent( e );
}


//
// Splitter
//

Splitter::Splitter(Orientation o, QWidget* parent, const char* name)
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
  const QObjectList *l = children();
  QObjectListIt it( *l );
  QObject *obj;
  while ( (obj = it.current()) != 0 ) {
    ++it;
    if (obj->isA("Splitter")) ((Splitter*)obj)->checkVisiblity();
    else if (obj->isA("TabWidget")) ((TabWidget*)obj)->checkVisibility();
  }
}




//
// TabWidget
//

TabWidget::TabWidget(TabView* v, QWidget* parent,
                     const char* name, WFlags f)
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
 * |  | Top                 | TopRight |
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
  : QWidget(parent, name), TraceItemView(parentView)
{
    setFocusPolicy(QWidget::StrongFocus);

  _isCollapsed = true;

  QVBoxLayout* vbox = new QVBoxLayout( this, 6, 6);

  _nameLabel = new KSqueezedTextLabel( this, "nameLabel" );
  _nameLabel->setText(i18n("(No profile data file loaded)"));
  vbox->addWidget( _nameLabel );

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

  addTop( addTab( i18n("Types"),
		  new CostTypeView(this, _topTW,
				   "CostTypeView")));
  addTop( addTab( i18n("Callers"),
		  new CallView(true, this, _topTW,
			       "CallerView")));
  addTop( addTab( i18n("All Callers"),
		  new CoverageView(true, this, _topTW,
				   "AllCallerView")));
  addTop( addTab( i18n("Caller Map"),
		  new CallMapView(true, this, _bottomTW,
				  "CallerMapView")));
  addTop( addTab( i18n("Source"),
		  new SourceView(this, _topTW,
				 "SourceView")));

  addBottom( addTab( i18n("Parts"),
		     new PartView(this, _bottomTW,
				  "PartView")));
  addBottom( addTab( i18n("Call Graph"),
		     new CallGraphView(this, _bottomTW,
				       "CallGraphView")));
  addBottom( addTab( i18n("Callees"),
		     new CallView(false, this, _bottomTW,
				  "CalleeView")));
  addBottom( addTab( i18n("All Callees"),
		     new CoverageView(false, this, _bottomTW,
				      "AllCalleeView")));

  addBottom( addTab( i18n("Callee Map"),
		     new CallMapView(false, this, _topTW,
				     "CalleeMapView")));
  addBottom( addTab( i18n("Assembler"),
		     new InstrView(this, _bottomTW,
				   "InstrView")));

  // after all child widgets are created...
  _lastFocus = 0;
  _active = false;
  installFocusFilters();

  updateVisibility();

  QWhatsThis::add( this, whatsThis() );
}

void TabView::setData(TraceData* d)
{
  TraceItemView::setData(d);

  TraceItemView* v;
  for (v=_tabs.first();v;v=_tabs.next())
    v->setData(d);  
}

TraceItemView* TabView::addTab(QString label, TraceItemView* view)
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



// This hides/shows splitters and tabwidgets according to tab childs
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

  QValueList<int> s;
  s.append(100);


  // children of mainSplitter
  if (_rightTW->isHidden() != (r == 0)) {
    if (r == 0) {
      _rightTW->hide();

      if (!_topTW->hasVisibleRect() &&
          !_bottomTW->hasVisibleRect() &&
          !_leftTW->hasVisibleRect())  _mainSplitter->setSizes(s);
    }
    else
      _rightTW->show();
  }
  if (_leftSplitter->isHidden() != (t+b+l == 0)) {
    if (t+b+l == 0) {
      _leftSplitter->hide();

      if (!_rightTW->hasVisibleRect()) _mainSplitter->setSizes(s);
    }
    else
      _leftSplitter->show();
  }

  // children of leftSplitter
  if (_topTW->isHidden() != (t == 0)) {
    if (t == 0) {
      _topTW->hide();

      if (!_bottomTW->hasVisibleRect() &&
          !_leftTW->hasVisibleRect()) _leftSplitter->setSizes(s);
    }
    else
      _topTW->show();
  }

  if (_bottomSplitter->isHidden() != (b+l == 0)) {
    if (b+l == 0) {
      _bottomSplitter->hide();

      if (!_topTW->hasVisibleRect()) _leftSplitter->setSizes(s);
    }
    else
      _bottomSplitter->show();
  }

  // children of bottomSplitter
  if (_bottomTW->isHidden() != (b == 0)) {
    if (b == 0) {
      _bottomTW->hide();

      if (!_leftTW->hasVisibleRect()) _bottomSplitter->setSizes(s);
    }
    else
      _bottomTW->show();
  }
  if (_leftTW->isHidden() != (l == 0)) {
    if (l == 0) {
      _leftTW->hide();

      if (!_bottomTW->hasVisibleRect()) _bottomSplitter->setSizes(s);
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

    QPtrList<TraceItemView> tabs;
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
    return i18n( "<b>Information Tabs</b>"
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
		 "For more information, see the <em>What's This?</em> "
		 "help of the corresponding tab widget</p>");
}

void TabView::installFocusFilters()
{
    QObjectList *l = queryList("QWidget");
    QObjectListIt it( *l );
    QObject *obj;

    while ( (obj = it.current()) != 0 ) {
        ++it;
	if ( ((QWidget*)obj)->isFocusEnabled() )
	    obj->installEventFilter(this);
    }
    delete l;
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

    if (0) qDebug("%s::setActive(%s)", name(), a ? "true":"false");

    if (a) emit activated(this);
}

void TabView::doUpdate(int changeType)
{
    if (changeType & (activeItemChanged | configChanged | dataChanged))

	_nameLabel->setText( !_data ? i18n("(No Data loaded)") :
			     !_activeItem ? i18n("(No function selected)") :
			     _activeItem->prettyName());


    // we use our own list iterators because setTabEnabled can
    // invoke tabChanged, which mangles with the lists, too
    bool canShow;
    TraceItemView *v;
    QPtrListIterator<TraceItemView> it( _tabs );
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
      canShow = v->set(changeType, _data, _costType, _costType2,
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

  bool collapsed = (e->size().width()<=1) || (e->size().height()<=1);
  if (_isCollapsed != collapsed) {
    _isCollapsed = collapsed;
    updateView();
  }

  if (0) qDebug("TabView::Resize from (%d/%d) to (%d/%d)",
         e->oldSize().width(), e->oldSize().height(),
         e->size().width(), e->size().height());
}

void TabView::selected(TraceItemView*, TraceItem* s)
{
    // we set selected item for our own children
    select(s);
    updateView();

    // still forward to parent
    if (_parentView) _parentView->selected(this, s);
}


void TabView::readViewConfig(KConfig* c,
			     QString prefix, QString postfix,
			     bool withOptions)
{
    if (0) qDebug("%s::readConfig(%s%s)", name(),
		  prefix.ascii(), postfix.ascii());

    KConfigGroup* g = configGroup(c, prefix, postfix);

    _mainSplitter->setSizes(g->readIntListEntry("MainSizes"));
    _leftSplitter->setSizes(g->readIntListEntry("LeftSizes"));
    _bottomSplitter->setSizes(g->readIntListEntry("BottomSizes"));

    QString activeT = g->readEntry("ActiveTop", "CallerView");
    QString activeB = g->readEntry("ActiveBottom", "CalleeView");
    QString activeL = g->readEntry("ActiveLeft", "");
    QString activeR = g->readEntry("ActiveRight", "");

    QStringList topTabs    = g->readListEntry("TopTabs");
    QStringList bottomTabs = g->readListEntry("BottomTabs");
    QStringList leftTabs   = g->readListEntry("LeftTabs");
    QStringList rightTabs  = g->readListEntry("RightTabs");

    if (topTabs.isEmpty() && bottomTabs.isEmpty() &&
        rightTabs.isEmpty() && leftTabs.isEmpty()) {
      // no tabs visible ?! Reset to default
      topTabs << "CostTypeView" << "CallerView" << "AllCallerView"
              << "CalleeMapView" << "SourceView";
      bottomTabs << "PartView" << "CalleeView" << "CallGraphView"
                 << "AllCalleeView" << "CallerMapView" << "InstrView";
    }

    TraceItemView *activeTop = 0, *activeBottom = 0;
    TraceItemView *activeLeft = 0, *activeRight = 0;

    moveTab(0, TraceItemView::Top, true);
    TraceItemView *v;
    QPtrListIterator<TraceItemView> it( _tabs );
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

      if (withOptions)
	v->readViewConfig(c, QString("%1-%2")
			  .arg(prefix).arg(v->widget()->name()),
			  postfix, true);
    }
    if (activeTop)   _topTW->showPage(activeTop->widget());
    if (activeBottom)_bottomTW->showPage(activeBottom->widget());
    if (activeLeft)  _leftTW->showPage(activeLeft->widget());
    if (activeRight) _rightTW->showPage(activeRight->widget());

    QString activeType = g->readEntry("ActiveItemType", "");
    QString activeName = g->readEntry("ActiveItemName", "");
    QString selectedType = g->readEntry("SelectedItemType", "");
    QString selectedName = g->readEntry("SelectedItemName", "");
    delete g;
    
    if (!_data) return;
    
    if (withOptions) {  
      // restore active item
      TraceItem::CostType t = TraceItem::costType(activeType);
      if (t==TraceItem::NoCostType) t = TraceItem::Function;
      TraceCost* activeItem = _data->search(t, activeName, _costType);
      if (!activeItem) return;
      activate(activeItem);
      
      // restore selected item
      t = TraceItem::costType(selectedType);
      if (t==TraceItem::NoCostType) t = TraceItem::Function;
      TraceCost* selectedItem = _data->search(t, selectedName,
					      _costType, activeItem);
      if (selectedItem) select(selectedItem);
    }

    updateView();
}

void TabView::saveViewConfig(KConfig* c,
			     QString prefix, QString postfix,
			     bool withOptions)
{
    KConfigGroup g(c, (prefix+postfix).ascii());

    g.writeEntry("MainSizes", _mainSplitter->sizes());
    g.writeEntry("LeftSizes", _leftSplitter->sizes());
    g.writeEntry("BottomSizes", _bottomSplitter->sizes());

    QString a;
    if ((_topTW->count()>0) &&
        (_topTW->isTabEnabled(_topTW->currentPage())))
      a = QString(_topTW->currentPage()->name());
    g.writeEntry("ActiveTop", a);

    a.setLength(0);
    if ((_bottomTW->count()>0) &&
        (_bottomTW->isTabEnabled(_bottomTW->currentPage())))
      a = QString(_bottomTW->currentPage()->name());
    g.writeEntry("ActiveBottom", a);

    a.setLength(0);
    if ((_leftTW->count()>0) &&
        (_leftTW->isTabEnabled(_leftTW->currentPage())))
      a = QString(_leftTW->currentPage()->name());
    g.writeEntry("ActiveLeft", a);

    a.setLength(0);
    if ((_rightTW->count()>0) &&
        (_rightTW->isTabEnabled(_rightTW->currentPage())))
      a = QString(_rightTW->currentPage()->name());
    g.writeEntry("ActiveRight", a);

    if (withOptions)
      if (_activeItem) {
	g.writeEntry("ActiveItemType",
                     TraceItem::typeName(_activeItem->type()));
	g.writeEntry("ActiveItemName", _activeItem->name());
	if (_selectedItem) {
	  g.writeEntry("SelectedItemType",
		       TraceItem::typeName(_selectedItem->type()));
	  g.writeEntry("SelectedItemName", _selectedItem->name());
	}
      }

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

    g.writeEntry("TopTabs", topList);
    g.writeEntry("BottomTabs", bottomList);
    g.writeEntry("LeftTabs", leftList);
    g.writeEntry("RightTabs", rightList);

    if (withOptions)
      for (v=_tabs.first();v;v=_tabs.next())
	v->saveViewConfig(c, QString("%1-%2").arg(prefix)
			  .arg(v->widget()->name()), postfix, true);
}

#include "tabview.moc"
