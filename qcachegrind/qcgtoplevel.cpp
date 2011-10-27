/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * QCachegrind top level window
 */

#define TRACE_UPDATES 0

#include "qcgtoplevel.h"

#include <stdlib.h> // for system()

#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QTimer>
#include <QByteArray>
#include <QLabel>
#include <QMenuBar>
#include <QProgressBar>
#include <QFile>
#include <QFileDialog>
#include <QEventLoop>
#include <QToolBar>
#include <QComboBox>
#include <QMessageBox>
#include <QStatusBar>
#include <QWhatsThis>

#ifdef QT_DBUS_SUPPORT
#include <QtDBus/QDBusConnection>
#endif

#include "partselection.h"
#include "functionselection.h"
#include "stackselection.h"
#include "stackbrowser.h"
#include "tracedata.h"
#include "config.h"
#include "globalguiconfig.h"
#include "multiview.h"
#include "callgraphview.h"
#include "configdialog.h"

QCGTopLevel::QCGTopLevel()
{
#ifdef QT_DBUS_SUPPORT
    QDBusConnection con = QDBusConnection::sessionBus();
    con.registerObject("/QCachegrind", this,
		       QDBusConnection::ExportScriptableSlots);
#endif

    _progressBar = 0;
    _statusbar = statusBar();
    _statusLabel = new QLabel(_statusbar);
    _statusbar->addWidget(_statusLabel, 1);

    _layoutCount = 1;
    _layoutCurrent = 0;

    resetState();

    GlobalGUIConfig::config()->readOptions();

    createActions();
    createDocks();
    createMenu();
    createToolbar();

    _multiView = new MultiView(this, this);
    _multiView->setObjectName("MultiView");
    setCentralWidget(_multiView);

    // restore current state settings (not configuration options)
    restoreCurrentState(QString::null);

    // restore docks & toolbars from config
    QByteArray state, geometry;
    ConfigGroup* topConfig = ConfigStorage::group("TopWindow");
    _forcePartDock = topConfig->value("ForcePartDockVisible", false).toBool();
    state = topConfig->value("State", QByteArray()).toByteArray();
    geometry = topConfig->value("Geometry", QByteArray()).toByteArray();
    delete topConfig;

    if (!geometry.isEmpty())
	restoreGeometry(geometry);
    if (!state.isEmpty())
	restoreState(state);

    setWindowIcon(QIcon(":/app.png"));
    setAttribute(Qt::WA_DeleteOnClose);
}

QCGTopLevel::~QCGTopLevel()
{
    delete _data;
}

// reset the visualization state, e.g. before loading new data
void QCGTopLevel::resetState()
{
    _activeParts.clear();
    _hiddenParts.clear();

    _data = 0;
    _function = 0;
    _eventType = 0;
    _eventType2 = 0;
    _groupType = ProfileContext::InvalidType;
    _group = 0;

    // for delayed slots
    _traceItemDelayed = 0;
    _eventTypeDelayed = 0;
    _eventType2Delayed = 0;
    _groupTypeDelayed = ProfileContext::InvalidType;
    _groupDelayed = 0;
    _directionDelayed = TraceItemView::None;
    _lastSender = 0;
}


/**
 * This saves the current state of the main window and
 * sub widgets.
 *
 * No positions are saved. These is done automatically for
 * KToolbar, and manually in queryExit() for QT docks.
 */
void QCGTopLevel::saveCurrentState(const QString& postfix)
{
    QString eventType, eventType2;
    if (_eventType) eventType = _eventType->name();
    if (_eventType2) eventType2 = _eventType2->name();

    ConfigGroup* stateConfig = ConfigStorage::group(QString("CurrentState") + postfix);
    stateConfig->setValue("EventType", eventType);
    stateConfig->setValue("EventType2", eventType2);
    stateConfig->setValue("GroupType", ProfileContext::typeName(_groupType));
    delete stateConfig;

    _partSelection->saveOptions(QString("PartOverview"), postfix);
    _multiView->saveLayout(QString("MainView"), postfix);
    _multiView->saveOptions(QString("MainView"), postfix);
}

/**
 * This function is called when a trace is closed.
 * Save browsing position for later restoring
 */
void QCGTopLevel::saveTraceSettings()
{
    QString key = traceKey();

    ConfigGroup* lConfig = ConfigStorage::group("Layouts");
    lConfig->setValue(QString("Count%1").arg(key), _layoutCount);
    lConfig->setValue(QString("Current%1").arg(key), _layoutCurrent);
    delete lConfig;

    ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
    if (_eventType)
	pConfig->setValue(QString("EventType%1").arg(key), _eventType->name());
    if (_eventType2)
	pConfig->setValue(QString("EventType2%1").arg(key), _eventType2->name());
    if (_groupType != ProfileContext::InvalidType)
	pConfig->setValue(QString("GroupType%1").arg(key),
			  ProfileContext::typeName(_groupType));

    if (_data) {
	if (_group)
	    pConfig->setValue(QString("Group%1").arg(key), _group->name());
	saveCurrentState(key);
    }
    delete pConfig;
}

/**
 * This restores the current visualization state of the main window and
 * of the profile views.
 */
void QCGTopLevel::restoreCurrentState(const QString& postfix)
{
    _partSelection->restoreOptions(QString("PartOverview"), postfix);
    _multiView->restoreLayout(QString("MainView"), postfix);
    _multiView->restoreOptions(QString("MainView"), postfix);

    _splittedToggleAction->setChecked(_multiView->childCount()>1);
    _splitDirectionToggleAction->setEnabled(_multiView->childCount()>1);
    _splitDirectionToggleAction->setChecked(_multiView->orientation() ==
					    Qt::Horizontal);
}

void QCGTopLevel::sidebarMenuAboutToShow()
{
    QAction* action;
    QMenu *popup = _sidebarMenuAction->menu();

    popup->clear();

    action = popup->addAction(tr("Parts Overview"));
    action->setCheckable(true);
    action->setChecked(_partDock->isVisible());
    connect(action, SIGNAL(triggered(bool)), this, SLOT(togglePartDock()));

    action = popup->addAction(tr("Top Cost Call Stack"));
    action->setCheckable(true);
    action->setChecked(_stackDock->isVisible());
    connect(action, SIGNAL(triggered(bool)), this, SLOT(toggleStackDock()));

    action = popup->addAction(tr("Flat Profile"));
    action->setCheckable(true);
    action->setChecked(_functionDock->isVisible());
    connect(action, SIGNAL(triggered(bool)), this, SLOT(toggleFunctionDock()));
}

void QCGTopLevel::recentFilesMenuAboutToShow()
{
    QStringList recentFiles;
    QMenu *popup = _recentFilesMenuAction->menu();

    popup->clear();

    ConfigGroup* generalConfig = ConfigStorage::group("GeneralSettings");
    recentFiles = generalConfig->value("RecentFiles",
				       QStringList()).toStringList();
    delete generalConfig;

    if (recentFiles.count() == 0)
	popup->addAction(tr("(No recent files)"));
    else {
	foreach(const QString& file, recentFiles) {
	    // paths shown to user should use OS-native separators
	    popup->addAction(QDir::toNativeSeparators(file));
	}
    }
}

void QCGTopLevel::recentFilesTriggered(QAction* action)
{
    if (action)
	loadTrace(QDir::fromNativeSeparators(action->text()));
}

void QCGTopLevel::createDocks()
{
    // part visualization/selection side bar
    _partDock = new QDockWidget(this);
    _partDock->setObjectName("part-dock");
    _partDock->setWindowTitle(tr("Parts Overview"));
    _partSelection = new PartSelection(this, _partDock);
    _partDock->setWidget(_partSelection);

    connect(_partSelection, SIGNAL(partsHideSelected()),
	    this, SLOT(partsHideSelectedSlotDelayed()));
    connect(_partSelection, SIGNAL(partsUnhideAll()),
	    this, SLOT(partsUnhideAllSlotDelayed()));

    // stack selection side bar
    _stackDock = new QDockWidget(this);
    _stackDock->setObjectName("stack-dock");
    _stackSelection = new StackSelection(_stackDock);
    _stackDock->setWidget(_stackSelection);
    _stackDock->setWindowTitle(tr("Top Cost Call Stack"));
    _stackSelection->setWhatsThis( tr(
                   "<b>The Top Cost Call Stack</b>"
                   "<p>This is a purely fictional 'most probable' call stack. "
                   "It is built up by starting with the current selected "
                   "function and adds the callers/callees with highest cost "
                   "at the top and to bottom.</p>"
                   "<p>The <b>Cost</b> and <b>Calls</b> columns show the "
                   "cost used for all calls from the function in the line "
                   "above.</p>"));
    connect(_stackSelection, SIGNAL(functionSelected(CostItem*)),
	    this, SLOT(setTraceItemDelayed(CostItem*)));
    // actions are already created
    connect(_upAction, SIGNAL(triggered(bool)),
	    _stackSelection, SLOT(browserUp()) );
    connect(_backAction, SIGNAL(triggered(bool)),
	    _stackSelection, SLOT(browserBack()) );
    connect(_forwardAction, SIGNAL(triggered(bool)),
	    _stackSelection, SLOT(browserForward()));

    // flat function profile side bar
    _functionDock = new QDockWidget(this);
    _functionDock->setObjectName("function-dock");
    _functionDock->setWindowTitle(tr("Flat Profile"));
    _functionSelection = new FunctionSelection(this, _functionDock);
    _functionDock->setWidget(_functionSelection);
    // functionDock needs call to updateView() when getting visible
    connect(_functionDock, SIGNAL(visibilityChanged(bool)),
	    this, SLOT(functionVisibilityChanged(bool)));

    // defaults (later to be adjusted from stored state in config)
    addDockWidget(Qt::LeftDockWidgetArea, _partDock );
    addDockWidget(Qt::LeftDockWidgetArea, _stackDock );
    addDockWidget(Qt::LeftDockWidgetArea, _functionDock );
    _stackDock->hide();
    _partDock->hide();
}




void QCGTopLevel::createActions()
{
    QString hint;
    QIcon icon;

    // file menu actions
    _newAction = new QAction(tr("&New"), this);
    _newAction->setShortcuts(QKeySequence::New);
    _newAction->setStatusTip(tr("Open new empty window"));
    connect(_newAction, SIGNAL(triggered()), this, SLOT(newWindow()));

    icon = QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton);
    _openAction = new QAction(icon, tr("&Open..."), this);
    _openAction->setShortcuts(QKeySequence::Open);
    _openAction->setStatusTip(tr("Open profile data file"));
    connect(_openAction, SIGNAL(triggered()), this, SLOT(loadTrace()));

    _addAction = new QAction(tr( "&Add..." ), this);
    _addAction->setStatusTip(tr("Add profile data to current window"));
    connect(_addAction, SIGNAL(triggered(bool)), SLOT(addTrace()));

    _exportAction = new QAction(tr("Export Graph"), this);
    _exportAction->setStatusTip(tr("Generate GraphViz file 'callgraph.dot'"));
    connect(_exportAction, SIGNAL(triggered(bool)), SLOT(exportGraph()));

    _recentFilesMenuAction = new QAction(tr("Open &Recent"), this);
    _recentFilesMenuAction->setMenu(new QMenu(this));
    connect(_recentFilesMenuAction->menu(), SIGNAL(aboutToShow()),
	     this, SLOT(recentFilesMenuAboutToShow()));
    connect(_recentFilesMenuAction->menu(), SIGNAL(triggered(QAction*)),
	    this, SLOT(recentFilesTriggered(QAction*)));

    _exitAction = new QAction(tr("E&xit"), this);
    _exitAction->setShortcut(tr("Ctrl+Q"));
    _exitAction->setStatusTip(tr("Exit the application"));
    connect(_exitAction, SIGNAL(triggered()), this, SLOT(close()));

    // view menu actions
    icon = QApplication::style()->standardIcon(QStyle::SP_BrowserReload);
    _cyclesToggleAction = new QAction(icon, tr("Detect Cycles"), this);
    _cyclesToggleAction->setCheckable(true);
    _cyclesToggleAction->setStatusTip(tr("Do Cycle Detection"));
    hint = tr("<b>Detect recursive cycles</b>"
              "<p>If this is switched off, the treemap drawing will show "
              "black areas when a recursive call is made instead of drawing "
	      "the recursion ad infinitum. Note that "
              "the size of black areas often will be wrong, as inside "
	      "recursive cycles the cost of calls cannot be determined; "
	      "the error is small, "
              "however, for false cycles (see documentation).</p>"
              "<p>The correct handling for cycles is to detect them and "
	      "collapse all functions of a cycle into an artificial "
	      "function, which is done when this option is selected. "
	      "Unfortunately, with GUI applications, this often will "
              "lead to huge false cycles, making the analysis impossible; "
	      "therefore, there is the option to switch this off.</p>");
    _cyclesToggleAction->setWhatsThis(hint);
    connect(_cyclesToggleAction, SIGNAL(triggered(bool)),
	    this, SLOT(toggleCycles()));
    _cyclesToggleAction->setChecked(GlobalConfig::showCycles());

    _percentageToggleAction = new QAction(QIcon(":/percent.png"),
					  tr("Relative Cost"), this);
    _percentageToggleAction->setCheckable(true);
    _percentageToggleAction->setStatusTip(tr("Show Relative Costs"));
    connect(_percentageToggleAction, SIGNAL(triggered(bool)),
	    this, SLOT(togglePercentage()));
    _percentageToggleAction->setChecked(GlobalConfig::showPercentage());

    _hideTemplatesToggleAction = new QAction(QIcon(":/hidetemplates.png"),
                                             tr("Shorten Templates"), this);
    _hideTemplatesToggleAction->setCheckable(true);
    _hideTemplatesToggleAction->setStatusTip(tr("Hide Template Parameters "
                                                "in C++ Symbols"));
    connect(_hideTemplatesToggleAction, SIGNAL(triggered(bool)),
            this, SLOT(toggleHideTemplates()));
    _hideTemplatesToggleAction->setChecked(GlobalConfig::hideTemplates());
    hint = tr("<b>Hide Template Parameters in C++ Symbols</b>"
              "<p>If this is switched on, every symbol displayed will have "
              "any C++ template parameters hidden, just showing &lt;&gt; "
              "instead of a potentially nested template parameter.</p>"
              "<p>In this mode, you can hover the mouse pointer over the "
              "activated symbol label to show a tooltip with the "
              "unabbreviated symbol.</p>");
    _hideTemplatesToggleAction->setWhatsThis(hint);

    _expandedToggleAction = new QAction(QIcon(":/move.png"),
					tr("Relative to Parent"), this);
    _expandedToggleAction->setCheckable(true);
    _expandedToggleAction->setStatusTip(
	tr("Show Percentage relative to Parent"));
    hint = tr("<b>Show percentage costs relative to parent</b>"
              "<p>If this is switched off, percentage costs are always "
	      "shown relative to the total cost of the profile part(s) "
	      "that are currently browsed. By turning on this option, "
	      "percentage cost of shown cost items will be relative "
	      "to the parent cost item.</p>"
              "<ul><table>"
              "<tr><td><b>Cost Type</b></td><td><b>Parent Cost</b></td></tr>"
              "<tr><td>Function Inclusive</td><td>Total</td></tr>"
              "<tr><td>Function Self</td><td>Function Group (*)/Total</td></tr>"
              "<tr><td>Call</td><td>Function Inclusive</td></tr>"
              "<tr><td>Source Line</td><td>Function Inclusive</td></tr>"
              "</table></ul>"
              "<p>(*) Only if function grouping is switched on "
	      "(e.g. ELF object grouping).</p>");
    _expandedToggleAction->setWhatsThis( hint );
    connect(_expandedToggleAction, SIGNAL(triggered(bool)),
	    this, SLOT(toggleExpanded()));
    _expandedToggleAction->setChecked(GlobalConfig::showExpanded());

    _splittedToggleAction = new QAction(tr("Splitted Visualization"), this);
    _splittedToggleAction->setCheckable(true);
    _splittedToggleAction->setStatusTip(
	tr("Show visualization of two cost items"));
    connect(_splittedToggleAction, SIGNAL(triggered(bool)),
	    this, SLOT(toggleSplitted()));

    _splitDirectionToggleAction = new QAction(tr("Split Horizontal"), this);
    _splitDirectionToggleAction->setCheckable(true);
    _splitDirectionToggleAction->setStatusTip(
	tr("Split visualization area horizontally"));
    connect(_splitDirectionToggleAction, SIGNAL(triggered(bool)),
	    this, SLOT(toggleSplitDirection()));

    _sidebarMenuAction = new QAction(tr("Sidebars"), this);
    _sidebarMenuAction->setMenu(new QMenu(this));
    connect( _sidebarMenuAction->menu(), SIGNAL( aboutToShow() ),
	     this, SLOT( sidebarMenuAboutToShow() ));

    _layoutDup = new QAction(tr("&Duplicate"), this);
    connect(_layoutDup, SIGNAL(triggered()), SLOT(layoutDuplicate()));
    _layoutDup->setShortcut(Qt::CTRL + Qt::Key_Plus);
    _layoutDup->setStatusTip(tr("Duplicate current layout"));

    _layoutRemove = new QAction(tr("&Remove"), this);
    connect(_layoutRemove, SIGNAL(triggered()), SLOT(layoutRemove()));
    _layoutRemove->setStatusTip(tr("Remove current layout"));

    _layoutNext = new QAction(tr("Go to &Next"), this);
    connect(_layoutNext, SIGNAL(triggered()), SLOT(layoutNext()));
    _layoutNext->setShortcut(Qt::CTRL + Qt::Key_Right);
    _layoutNext->setStatusTip(tr("Switch to next layout"));

    _layoutPrev = new QAction(tr("Go to &Previous"), this);
    connect(_layoutPrev, SIGNAL(triggered()), SLOT(layoutPrevious()));
    _layoutPrev->setShortcut(Qt::CTRL + Qt::Key_Left);
    _layoutPrev->setStatusTip(tr("Switch to previous layout"));

    _layoutRestore = new QAction(tr("&Restore to Default"), this);
    connect(_layoutRestore, SIGNAL(triggered()), SLOT(layoutRestore()));
    _layoutRestore->setStatusTip(tr("Restore layouts to default"));

    _layoutSave = new QAction(tr("&Save as Default"), this);
    connect(_layoutSave, SIGNAL(triggered()), SLOT(layoutSave()));
    _layoutSave->setStatusTip(tr("Save layouts as default"));

    // go menu actions
    icon = QApplication::style()->standardIcon(QStyle::SP_ArrowUp);
    _upAction = new QAction(icon, tr( "Up" ), this );
    _upAction->setShortcut( QKeySequence(Qt::ALT+Qt::Key_Up) );
    _upAction->setStatusTip(tr("Go Up in Call Stack"));
    _upAction->setMenu(new QMenu(this));
    connect(_upAction->menu(), SIGNAL(aboutToShow()),
	    this, SLOT(upAboutToShow()) );
    connect(_upAction->menu(), SIGNAL(triggered(QAction*)),
	    this, SLOT(upTriggered(QAction*)) );
    hint = tr("Go to last selected caller of current function");
    _upAction->setToolTip(hint);

    icon = QApplication::style()->standardIcon(QStyle::SP_ArrowBack);
    _backAction = new QAction(icon, tr("Back"), this);
    _backAction->setShortcut( QKeySequence(Qt::ALT+Qt::Key_Left) );
    _backAction->setStatusTip(tr("Go Back"));
    _backAction->setMenu(new QMenu(this));
    connect(_backAction->menu(), SIGNAL(aboutToShow()),
           this, SLOT(backAboutToShow()) );
    connect(_backAction->menu(), SIGNAL(triggered(QAction*)),
	    this, SLOT(backTriggered(QAction*)) );
    hint = tr("Go back in function selection history");
    _backAction->setToolTip(hint);

    icon = QApplication::style()->standardIcon(QStyle::SP_ArrowForward);
    _forwardAction = new QAction(icon, tr("Forward"), this);
    _forwardAction->setShortcut( QKeySequence(Qt::ALT+Qt::Key_Right) );
    _forwardAction->setStatusTip(tr("Go Forward"));
    _forwardAction->setMenu(new QMenu(this));
    connect(_forwardAction->menu(), SIGNAL(aboutToShow()),
	    this, SLOT(forwardAboutToShow()) );
    connect(_forwardAction->menu(), SIGNAL(triggered(QAction*)),
	    this, SLOT(forwardTriggered(QAction*)) );
    hint = tr("Go forward in function selection history");
    _forwardAction->setToolTip( hint );

    // settings menu actions
    _configureAction = new QAction(tr("&Configure..."), this);
    _configureAction->setStatusTip(tr("Configure QCachegrind"));
    connect(_configureAction, SIGNAL(triggered()), this, SLOT(configure()));

    // help menu actions
    _aboutAction = new QAction(tr("&About QCachegrind..."), this);
    _aboutAction->setStatusTip(tr("Show the application's About box"));
    connect(_aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    _aboutQtAction = new QAction(tr("About Qt..."), this);
    connect(_aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    // toolbar actions
    _eventTypeBox = new QComboBox(this);
    _eventTypeBox->setMinimumContentsLength(25);
    hint = tr("Select primary event type of costs");
    _eventTypeBox->setToolTip( hint );
    connect( _eventTypeBox, SIGNAL(activated(const QString&)),
	     this, SLOT(eventTypeSelected(const QString&)));
}

void QCGTopLevel::createMenu()
{
    QMenuBar* mBar = menuBar();

    QMenu* fileMenu = mBar->addMenu(tr("&File"));
    fileMenu->addAction(_newAction);
    fileMenu->addAction(_openAction);
    fileMenu->addAction(_recentFilesMenuAction);
    fileMenu->addAction(_addAction);
    fileMenu->addSeparator();
    fileMenu->addAction(_exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(_exitAction);

    QMenu* layoutMenu = new QMenu(tr("&Layout"), this);
    layoutMenu->addAction(_layoutDup);
    layoutMenu->addAction(_layoutRemove);
    layoutMenu->addSeparator();
    layoutMenu->addAction(_layoutPrev);
    layoutMenu->addAction(_layoutNext);
    layoutMenu->addSeparator();
    layoutMenu->addAction(_layoutSave);
    layoutMenu->addAction(_layoutRestore);

    QMenu* viewMenu = mBar->addMenu(tr("&View"));
    viewMenu->addAction(_cyclesToggleAction);
    viewMenu->addAction(_percentageToggleAction);
    viewMenu->addAction(_expandedToggleAction);
    viewMenu->addAction(_hideTemplatesToggleAction);
    viewMenu->addSeparator();
    viewMenu->addAction(_splittedToggleAction);
    viewMenu->addAction(_splitDirectionToggleAction);
    viewMenu->addMenu(layoutMenu);

    QMenu* goMenu = mBar->addMenu(tr("&Go"));
    goMenu->addAction(_backAction);
    goMenu->addAction(_forwardAction);
    goMenu->addAction(_upAction);

    QMenu* settingsMenu = mBar->addMenu(tr("&Settings"));
    settingsMenu->addAction(_sidebarMenuAction);
    settingsMenu->addSeparator();
    settingsMenu->addAction(_configureAction);

    QMenu* helpMenu = mBar->addMenu(tr("&Help"));
    helpMenu->addAction(QWhatsThis::createAction(this));
    helpMenu->addSeparator();
    helpMenu->addAction(_aboutAction);
    helpMenu->addAction(_aboutQtAction);
}

void QCGTopLevel::createToolbar()
{
    QToolBar* tb = new QToolBar(tr("Main Toolbar"), this);
    tb->setObjectName("main-toolbar");
    addToolBar(Qt::TopToolBarArea, tb);

    tb->addAction(_openAction);
    tb->addSeparator();

    tb->addAction(_cyclesToggleAction);
    tb->addAction(_percentageToggleAction);
    tb->addAction(_expandedToggleAction);
    tb->addAction(_hideTemplatesToggleAction);
    tb->addSeparator();

    tb->addAction(_backAction);
    tb->addAction(_forwardAction);
    tb->addAction(_upAction);
    tb->addSeparator();

    tb->addWidget(_eventTypeBox);
}


void QCGTopLevel::about()
{
    QString text, version;
    version = QLatin1String("0.6");
    text = QString("<h3>QCachegrind %1</h3>").arg(version);
    text += tr("<p>QCachegrind is a graphical user interface for analysing "
	      "profiling data, which helps in the performance optimization "
	      "phase of developing a computer program. "
	      "QCachegrind is open-source, and it is distributed under the "
	      "terms of the GPL v2. For details and source code, see the "
	      "<a href=\"http://kcachegrind.sf.net\">homepage</a> of the "
	      "KCachegrind project.</p>"
              "Main author and maintainer: "
	      "<a href=\"mailto:Josef.Weidendorfer@gmx.de\">"
              "Josef Weidendorfer</a><br>"
              "(with lots of bug fixes and porting to Qt4 by the KDE team)");
    QMessageBox::about(this, tr("About QCachegrind"), text);
}

void QCGTopLevel::configure(QString s)
{
    static QString lastPage = QString::null;

    // if no specific config item should be focussed, use last page
    if (s.isEmpty()) s = lastPage;
    ConfigDialog d(_data, this, s);

    if (d.exec() == QDialog::Accepted) {
	GlobalConfig::config()->saveOptions();
	configChanged();
    }
    lastPage = d.currentPage();
}

void QCGTopLevel::togglePartDock()
{
  if (!_partDock->isVisible())
    _partDock->show();
  else
    _partDock->hide();
}

void QCGTopLevel::toggleStackDock()
{
  if (!_stackDock->isVisible())
    _stackDock->show();
  else
    _stackDock->hide();
}

void QCGTopLevel::toggleFunctionDock()
{
  if (!_functionDock->isVisible())
    _functionDock->show();
  else
    _functionDock->hide();
}

void QCGTopLevel::togglePercentage()
{
  setPercentage(_percentageToggleAction->isChecked());
}


void QCGTopLevel::setAbsoluteCost()
{
  setPercentage(false);
}

void QCGTopLevel::setRelativeCost()
{
  setPercentage(true);
}

void QCGTopLevel::setPercentage(bool show)
{
    if (GlobalConfig::showPercentage() == show) return;
    if (_percentageToggleAction->isChecked() != show)
	_percentageToggleAction->setChecked(show);
    _expandedToggleAction->setEnabled(show);
    GlobalConfig::setShowPercentage(show);

    _partSelection->notifyChange(TraceItemView::configChanged);
    _stackSelection->refresh();
    _functionSelection->notifyChange(TraceItemView::configChanged);
    _multiView->notifyChange(TraceItemView::configChanged);
}

void QCGTopLevel::toggleHideTemplates()
{
    bool show = _hideTemplatesToggleAction->isChecked();
    if (GlobalConfig::hideTemplates() == show) return;
    GlobalConfig::setHideTemplates(show);

    _partSelection->notifyChange(TraceItemView::configChanged);
    _stackSelection->refresh();
    _functionSelection->notifyChange(TraceItemView::configChanged);
    _multiView->notifyChange(TraceItemView::configChanged);
}

void QCGTopLevel::toggleExpanded()
{
    bool show = _expandedToggleAction->isChecked();
    if (GlobalConfig::showExpanded() == show) return;
    GlobalConfig::setShowExpanded(show);

    _partSelection->notifyChange(TraceItemView::configChanged);
    _stackSelection->refresh();
    _functionSelection->notifyChange(TraceItemView::configChanged);
    _multiView->notifyChange(TraceItemView::configChanged);
}

void QCGTopLevel::toggleCycles()
{
    bool show = _cyclesToggleAction->isChecked();
    if (GlobalConfig::showCycles() == show) return;
    GlobalConfig::setShowCycles(show);

    if (!_data) return;

    _data->invalidateDynamicCost();
    _data->updateFunctionCycles();

    _partSelection->notifyChange(TraceItemView::configChanged);
    _stackSelection->rebuildStackList();
    _functionSelection->notifyChange(TraceItemView::configChanged);
    _multiView->notifyChange(TraceItemView::configChanged);
}


void QCGTopLevel::functionVisibilityChanged(bool v)
{
    if (v)
	_functionSelection->updateView();
}


void QCGTopLevel::newWindow()
{
    QCGTopLevel* t = new QCGTopLevel();
    t->show();
}


void QCGTopLevel::loadTrace()
{
    QString file;
    file = QFileDialog::getOpenFileName(this,
					tr("Open Callgrind Data"),
					_lastFile,
					tr("Callgrind Files (callgrind.*);;All Files (*)"));
    loadTrace(file);
}

void QCGTopLevel::loadTrace(QString file, bool addToRecentFiles)
{
    if (file.isEmpty()) return;
    _lastFile = file;

    if (_data && _data->parts().count()>0) {

	// In new window
	QCGTopLevel* t = new QCGTopLevel();
	t->show();
	t->loadDelayed(file, addToRecentFiles);
	return;
    }

    // this constructor enables progress bar callbacks
    TraceData* d = new TraceData(this);
    int filesLoaded = d->load(file);
    if (filesLoaded >0)
	setData(d);

    if (!addToRecentFiles) return;

    // add to recent file list in config
    QStringList recentFiles;
    ConfigGroup* generalConfig = ConfigStorage::group("GeneralSettings");
    recentFiles = generalConfig->value("RecentFiles",
				       QStringList()).toStringList();
    recentFiles.removeAll(file);
    if (filesLoaded >0)
	recentFiles.prepend(file);
    if (recentFiles.count() >5)
	recentFiles.removeLast();
    generalConfig->setValue("RecentFiles", recentFiles);
    delete generalConfig;
}


void QCGTopLevel::addTrace()
{
    QString file;
    file = QFileDialog::getOpenFileName(this,
					tr("Add Callgrind Data"),
					_lastFile,
					tr("Callgrind Files (callgrind.*);;All Files (*)"));
    addTrace(file);
}


void QCGTopLevel::addTrace(QString file)
{
    if (file.isEmpty()) return;
    _lastFile = file;

    if (_data) {
	_data->load(file);

	// GUI update for added data
	configChanged();
	return;
    }

    // this constructor enables progress bar callbacks
    TraceData* d = new TraceData(this);
    int filesLoaded = d->load(file);
    if (filesLoaded >0)
	setData(d);
}



void QCGTopLevel::loadDelayed(QString file, bool addToRecentFiles)
{
    _loadTraceDelayed = file;
    _addToRecentFiles = addToRecentFiles;
    QTimer::singleShot(0, this, SLOT(loadTraceDelayed()));
}

void QCGTopLevel::loadTraceDelayed()
{
    if (_loadTraceDelayed.isEmpty()) return;

    loadTrace(_loadTraceDelayed, _addToRecentFiles);
    _loadTraceDelayed = QString();
}


void QCGTopLevel::exportGraph()
{
    if (!_data || !_function) return;

    QString n = QString("callgraph.dot");
    GraphExporter ge(_data, _function, _eventType, _groupType, n);
    ge.writeDot();

#ifdef Q_OS_UNIX
    // shell commands only work in UNIX
    QString cmd = QString("(dot %1 -Tps > %2.ps; kghostview %3.ps)&")
	.arg(n).arg(n).arg(n);
    if (::system(QFile::encodeName( cmd ))<0)
	qDebug() << "QCGTopLevel::exportGraph: can not run " << cmd;
#endif
}


bool QCGTopLevel::setEventType(QString s)
{
  EventType* ct;

  ct = (_data) ? _data->eventTypes()->type(s) : 0;

  // if costtype with given name not found, use first available
  if (!ct && _data) ct = _data->eventTypes()->type(0);

  return setEventType(ct);
}

bool QCGTopLevel::setEventType2(QString s)
{
  EventType* ct;

  // Special type tr("(Hidden)") gives 0
  ct = (_data) ? _data->eventTypes()->type(s) : 0;

  return setEventType2(ct);
}

void QCGTopLevel::eventTypeSelected(const QString& s)
{
  EventType* ct;

  ct = (_data) ? _data->eventTypes()->typeForLong(s) : 0;
  setEventType(ct);
}

void QCGTopLevel::eventType2Selected(const QString& s)
{
  EventType* ct;

  ct = (_data) ? _data->eventTypes()->typeForLong(s) : 0;
  setEventType2(ct);
}

bool QCGTopLevel::setEventType(EventType* ct)
{
  if (_eventType == ct) return false;
  _eventType = ct;

  if (ct) {
      int idx = _eventTypeBox->findText(ct->longName());
      if (idx >=0) _eventTypeBox->setCurrentIndex(idx);
  }

  _partSelection->setEventType(_eventType);
  _stackSelection->setEventType(_eventType);
  _functionSelection->setEventType(_eventType);
  _multiView->setEventType(_eventType);

  updateStatusBar();

  return true;
}

bool QCGTopLevel::setEventType2(EventType* ct)
{
  if (_eventType2 == ct) return false;
  _eventType2 = ct;

  QString longName = ct ? ct->longName() : tr("(Hidden)");

  _partSelection->setEventType2(_eventType2);
  _stackSelection->setEventType2(_eventType2);
  _functionSelection->setEventType2(_eventType2);
  _multiView->setEventType2(_eventType2);

  updateStatusBar();

  return true;
}


void QCGTopLevel::groupTypeSelected(int cg)
{
  switch(cg) {
  case 0: setGroupType( ProfileContext::Function ); break;
  case 1: setGroupType( ProfileContext::Object ); break;
  case 2: setGroupType( ProfileContext::File ); break;
  case 3: setGroupType( ProfileContext::Class ); break;
  case 4: setGroupType( ProfileContext::FunctionCycle ); break;
  default: break;
  }
}

bool QCGTopLevel::setGroupType(QString s)
{
  ProfileContext::Type gt;

  gt = ProfileContext::type(s);
  // only allow Function/Object/File/Class as grouptype
  switch(gt) {
  case ProfileContext::Object:
  case ProfileContext::File:
  case ProfileContext::Class:
  case ProfileContext::FunctionCycle:
    break;
  default:
    gt = ProfileContext::Function;
  }

  return setGroupType(gt);
}

bool QCGTopLevel::setGroupType(ProfileContext::Type gt)
{
  if (_groupType == gt) return false;
  _groupType = gt;

  int idx = -1;
  switch(gt) {
  case ProfileContext::Function: idx = 0; break;
  case ProfileContext::Object: idx = 1; break;
  case ProfileContext::File: idx = 2; break;
  case ProfileContext::Class: idx = 3; break;
  case ProfileContext::FunctionCycle: idx = 4; break;
  default:
    break;
  }

  if (idx==-1) return false;

#if 0
  if (saGroup->currentItem() != idx)
    saGroup->setCurrentItem(idx);
#endif

  _stackSelection->setGroupType(_groupType);

  _partSelection->set(_groupType);
  _functionSelection->set(_groupType);
  _multiView->set(_groupType);

  updateStatusBar();

  return true;
}

bool QCGTopLevel::setGroup(QString s)
{
    TraceCostItem* ci = _functionSelection->group(s);
    if (!ci)
	return false;

    return setGroup(ci);
}


bool QCGTopLevel::setGroup(TraceCostItem* g)
{
    if (_group == g) return false;
    _group = g;

    _functionSelection->setGroup(g);
    updateStatusBar();

    return true;
}

bool QCGTopLevel::setFunction(QString s)
{
  if (!_data) return false;

  ProfileCostArray* f = _data->search(ProfileContext::Function, s, _eventType);
  if (!f) return false;

  return setFunction((TraceFunction*)f);
}

bool QCGTopLevel::setFunction(TraceFunction* f)
{
  if (_function == f) return false;
  _function = f;

  _multiView->activate(f);
  _functionSelection->activate(f);
  _partSelection->activate(f);
  _stackSelection->setFunction(_function);

  StackBrowser* b = _stackSelection->browser();
  if (b) {
    // do not disable up: a press forces stack-up extending...
    _forwardAction->setEnabled(b->canGoForward());
    _backAction->setEnabled(b->canGoBack());
  }

#if TRACE_UPDATES
  qDebug("QCGTopLevel::setFunction(%s), lastSender %s",
         f ? f->prettyName().toAscii() : "0",
         _lastSender ? _lastSender->name() :"0" );
#endif

  return true;
}


/**
 * Delayed versions.
 * We always have a pair of slots: One receiver to start the
 * delay with a singleShot Timer. It stores the parameter into a
 * temporary variable. And one parameterless slot for
 * forwarding, using this temporary.
 */
void QCGTopLevel::setEventTypeDelayed(EventType* ct)
{
  _eventTypeDelayed = ct;
  QTimer::singleShot (0, this, SLOT(setEventTypeDelayed()));
}

void QCGTopLevel::setEventType2Delayed(EventType* ct)
{
  _eventType2Delayed = ct;
  QTimer::singleShot (0, this, SLOT(setEventType2Delayed()));
}

void QCGTopLevel::setEventTypeDelayed()
{
  setEventType(_eventTypeDelayed);
}

void QCGTopLevel::setEventType2Delayed()
{
  setEventType2(_eventType2Delayed);
}

void QCGTopLevel::setGroupTypeDelayed(ProfileContext::Type gt)
{
  _groupTypeDelayed = gt;
  QTimer::singleShot (0, this, SLOT(setGroupTypeDelayed()));
}

void QCGTopLevel::setGroupTypeDelayed()
{
  setGroupType(_groupTypeDelayed);
}

void QCGTopLevel::setGroupDelayed(TraceCostItem* g)
{
#if TRACE_UPDATES
  qDebug("QCGTopLevel::setGroupDelayed(%s), sender %s",
         g ? g->prettyName().toAscii() : "0",
         _lastSender ? _lastSender->name() :"0" );
#endif

  _groupDelayed = g;
  QTimer::singleShot (0, this, SLOT(setGroupDelayed()));
}

void QCGTopLevel::setGroupDelayed()
{
  setGroup(_groupDelayed);
}

void QCGTopLevel::setDirectionDelayed(TraceItemView::Direction d)
{
  _directionDelayed = d;
  QTimer::singleShot (0, this, SLOT(setDirectionDelayed()));
}

void QCGTopLevel::setDirectionDelayed()
{
    switch(_directionDelayed) {
    case TraceItemView::Back:
	_stackSelection->browserBack();
	break;

    case TraceItemView::Forward:
        _stackSelection->browserForward();
	break;

    case TraceItemView::Up:
	{
	    StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
	    HistoryItem* hi = b ? b->current() : 0;
	    TraceFunction* f = hi ? hi->function() : 0;

	    if (!f) break;
	    f = hi->stack()->caller(f, false);
	    if (f) setFunction(f);
	}
    break;

    default: break;
    }

    _directionDelayed = TraceItemView::None;
}


void QCGTopLevel::setTraceItemDelayed(CostItem* i)
{
  // no need to select same item a 2nd time...
  if (_traceItemDelayed == i) return;
  _traceItemDelayed = i;
  _lastSender = sender();

  qDebug() << "Selected " << (i ? i->prettyName() : "(none)");

#if TRACE_UPDATES
  qDebug("QCGTopLevel::setTraceItemDelayed(%s), sender %s",
         i ? i->prettyName().toAscii() : "0",
         _lastSender ? _lastSender->name() :"0" );
#endif

  QTimer::singleShot (0, this, SLOT(setTraceItemDelayed()));
}

void QCGTopLevel::setTraceItemDelayed()
{
    if (!_traceItemDelayed) return;

    switch(_traceItemDelayed->type()) {
    case ProfileContext::Function:
    case ProfileContext::FunctionCycle:
	setFunction((TraceFunction*)_traceItemDelayed);
	break;

    case ProfileContext::Object:
    case ProfileContext::File:
    case ProfileContext::Class:
	_multiView->activate(_traceItemDelayed);
	break;

#if 0
	// this conflicts with the selection policy of InstrView ?!?
    case ProfileContext::Instr:
    case ProfileContext::Line:
	// only for multiview
	_multiView->activate(_traceItemDelayed);
	break;
#endif

    default: break;
    }

    _traceItemDelayed = 0;
    _lastSender = 0;
}

/**
 * A TraceData object cannot be viewed many times in different
 * toplevel windows. Thus, this toplevel window takes ownership
 * of the TraceData object: on closing the window or opening
 * another trace, the object is destroyed.
 */
void QCGTopLevel::setData(TraceData* data)
{
  if (data == _data) return;

  _lastSender = 0;

  saveTraceSettings();

  if (_data) {
    _partSelection->setData(0);
    _stackSelection->setData(0);
    _functionSelection->setData(0);
    _multiView->setData(0);
    _multiView->updateView(true);

    // we are the owner...
    delete _data;
  }

  // reset members
  resetState();

  _data = data;

  // fill cost type list
  QStringList types;

  if (_data) {
      /* add all supported virtual types */
      EventTypeSet* m = _data->eventTypes();
      m->addKnownDerivedTypes();

      /* first, fill selection list with available cost types */
      for (int i=0;i<m->realCount();i++)
	  types << m->realType(i)->longName();
      for (int i=0;i<m->derivedCount();i++)
	  types << m->derivedType(i)->longName();
  }
  _eventTypes = types;
  _eventTypeBox->addItems(types);

  _stackSelection->setData(_data);
  _partSelection->setData(_data);
  _functionSelection->setData(_data);
  _multiView->setData(_data);
  // Force update of _data in all children of _multiView
  // This is needed to make restoring of activeItem work!
  _multiView->updateView(true);

  /* this is needed to let the other widgets know the types */
  restoreTraceTypes();

  restoreTraceSettings();

  QString caption;
  if (_data) {
    caption = QDir::toNativeSeparators(_data->traceName());
    if (!_data->command().isEmpty())
      caption += " [" + _data->command() + ']';
  }
  setWindowTitle(caption);

  if (!_data || (!_forcePartDock && _data->parts().count()<2))
    _partDock->hide();
  else
    _partDock->show();

  updateStatusBar();
}

void QCGTopLevel::addEventTypeMenu(QMenu* popup, bool withCost2)
{
  if (_data) {
    QMenu *popup1, *popup2 = 0;
    QAction* action;

    popup1 = popup->addMenu(tr("Primary Event Type"));
    connect(popup1, SIGNAL(triggered(QAction*)),
	     this, SLOT(setEventType(QAction*)));

    if (withCost2) {
      popup2 = popup->addMenu(tr("Secondary Event Type"));
      connect(popup2, SIGNAL(triggered(QAction*)),
	       this, SLOT(setEventType2(QAction*)));

      if (_eventType2) {
	action = popup2->addAction(tr("Hide"));
	action->setData(199);
	popup2->addSeparator();
      }
    }

    EventTypeSet* m = _data->eventTypes();
    EventType* ct;
    for (int i=0;i<m->realCount();i++) {
      ct = m->realType(i);

      action = popup1->addAction(ct->longName());
      action->setCheckable(true);
      action->setData(100+i);
      if (_eventType == ct) action->setChecked(true);

      if (popup2) {
	action = popup2->addAction(ct->longName());
	action->setCheckable(true);
	action->setData(100+i);
	if (_eventType2 == ct) action->setChecked(true);
      }
    }

    for (int i=0;i<m->derivedCount();i++) {
      ct = m->derivedType(i);

      action = popup1->addAction(ct->longName());
      action->setCheckable(true);
      action->setData(200+i);
      if (_eventType == ct) action->setChecked(true);

      if (popup2) {
	action = popup2->addAction(ct->longName());
	action->setCheckable(true);
	action->setData(200+i);
	if (_eventType2 == ct) action->setChecked(true);
      }
    }
  }

  if (GlobalConfig::showPercentage())
    popup->addAction(tr("Show Absolute Cost"),
		      this, SLOT(setAbsoluteCost()));
  else
    popup->addAction(tr("Show Relative Cost"),
		      this, SLOT(setRelativeCost()));
}

bool QCGTopLevel::setEventType(QAction* action)
{
  if (!_data) return false;
  int id = action->data().toInt(0);

  EventTypeSet* m = _data->eventTypes();
  EventType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->derivedType(id-200);

  return ct ? setEventType(ct) : false;
}

bool QCGTopLevel::setEventType2(QAction* action)
{
  if (!_data) return false;
  int id = action->data().toInt(0);

  EventTypeSet* m = _data->eventTypes();
  EventType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->derivedType(id-200);

  return setEventType2(ct);
}

void QCGTopLevel::addGoMenu(QMenu* popup)
{
  StackBrowser* b = _stackSelection->browser();
  if (b) {
      if (b->canGoBack())
	  popup->addAction(tr("Go Back"), this, SLOT(goBack()));
      if (b->canGoForward())
	  popup->addAction(tr("Go Forward"), this, SLOT(goForward()));
  }
  // do not disable up: a press forces stack-up extending...
  popup->addAction(tr("Go Up"), this, SLOT(goUp()));
}

void QCGTopLevel::goBack()
{
  setDirectionDelayed(TraceItemView::Back);
}

void QCGTopLevel::goForward()
{
  setDirectionDelayed(TraceItemView::Forward);
}

void QCGTopLevel::goUp()
{
  setDirectionDelayed(TraceItemView::Up);
}

QString QCGTopLevel::traceKey()
{
  if (!_data || _data->command().isEmpty()) return QString();

  QString name = _data->command();
  QString key;
  for (int l=0;l<name.length();l++)
    if (name[l].isLetterOrNumber()) key += name[l];

  return QString("-") + key;
}


void QCGTopLevel::restoreTraceTypes()
{
    QString key = traceKey();
    QString groupType, eventType, eventType2;

    ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
    groupType = pConfig->value(QString("GroupType%1").arg(key),QString()).toString();
    eventType = pConfig->value(QString("EventType%1").arg(key),QString()).toString();
    eventType2 = pConfig->value(QString("EventType2%1").arg(key),QString()).toString();
    delete pConfig;

    ConfigGroup* cConfig = ConfigStorage::group("CurrentState");
    if (groupType.isEmpty())
	groupType = cConfig->value("GroupType",QString()).toString();
    if (eventType.isEmpty())
	eventType = cConfig->value("EventType",QString()).toString();
    if (eventType2.isEmpty())
	eventType2 = cConfig->value("EventType2",QString()).toString();
    delete cConfig;

    setGroupType(groupType);
    setEventType(eventType);
    setEventType2(eventType2);

    // if still no event type set, use first available
    if (!_eventType && !_eventTypes.isEmpty())
	eventTypeSelected(_eventTypes.first());

    ConfigGroup* aConfig = ConfigStorage::group("Layouts");
    _layoutCount = aConfig->value(QString("Count%1").arg(key), 0).toInt();
    _layoutCurrent = aConfig->value(QString("Current%1").arg(key), 0).toInt();
    delete aConfig;

    if (_layoutCount == 0) layoutRestore();
    updateLayoutActions();
}


/**
 * This must be called after setting group/cost types in the function
 * selection widget, because the group/function choosing depends on
 * filled lists in the function selection widget
 */
void QCGTopLevel::restoreTraceSettings()
{
  if (!_data) return;

  QString key = traceKey();

  restoreCurrentState(key);

  ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
  QString group = pConfig->value(QString("Group%1").arg(key),QString()).toString();
  delete pConfig;
  if (!group.isEmpty()) setGroup(group);

  // restoreCurrentState() usually leads to a call to setTraceItemDelayed()
  // to restore last active item...
  if (!_traceItemDelayed) {
    // function not available any more.. try with "main"
      if (!setFunction("main")) {
#if 1
	_functionSelection->selectTopFunction();
#else
	HighestCostList hc;
	hc.clear(50);
	TraceFunctionMap::Iterator it;
	for ( it = _data->functionMap().begin();
	      it != _data->functionMap().end(); ++it )
	    hc.addCost(&(*it), (*it).inclusive()->subCost(_eventType));

	setFunction( (TraceFunction*) hc[0]);
#endif
      }
  }
}


/* Layout */

void QCGTopLevel::layoutDuplicate()
{
    // save current and allocate a new slot
    _multiView->saveLayout(QString("Layout%1-MainView").arg(_layoutCurrent),
			   traceKey());
    _layoutCurrent = _layoutCount;
    _layoutCount++;

    updateLayoutActions();

    qDebug() << "QCGTopLevel::layoutDuplicate: count " << _layoutCount;
}

void QCGTopLevel::layoutRemove()
{
    if (_layoutCount <2) return;

    int from = _layoutCount-1;
    if (_layoutCurrent == from) { _layoutCurrent--; from--; }

    // restore from last and decrement count
    _multiView->restoreLayout(QString("Layout%1-MainView").arg(from),
			      traceKey());
    _layoutCount--;

    updateLayoutActions();

    qDebug() << "QCGTopLevel::layoutRemove: count " << _layoutCount;
}

void QCGTopLevel::layoutNext()
{
    if (_layoutCount <2) return;

    QString key = traceKey();
    QString layoutPrefix = QString("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);
    _layoutCurrent++;
    if (_layoutCurrent == _layoutCount) _layoutCurrent = 0;
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    qDebug() << "QCGTopLevel::layoutNext: current " << _layoutCurrent;
}

void QCGTopLevel::layoutPrevious()
{
    if (_layoutCount <2) return;

    QString key = traceKey();
    QString layoutPrefix = QString("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);
    _layoutCurrent--;
    if (_layoutCurrent <0) _layoutCurrent = _layoutCount-1;
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    qDebug() << "QCGTopLevel::layoutPrevious: current " << _layoutCurrent;
}

void QCGTopLevel::layoutSave()
{
    QString key = traceKey();
    QString layoutPrefix = QString("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);

    // save all layouts as defaults (ie. without any group name postfix)
    for(int i=0;i<_layoutCount;i++) {
	_multiView->restoreLayout(layoutPrefix.arg(i), key);
	_multiView->saveLayout(layoutPrefix.arg(i), QString());
    }
    // restore the previously saved current layout
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    ConfigGroup* layoutConfig = ConfigStorage::group("Layouts");
    layoutConfig->setValue("DefaultCount", _layoutCount);
    layoutConfig->setValue("DefaultCurrent", _layoutCurrent);
    delete layoutConfig;
}

void QCGTopLevel::layoutRestore()
{
    ConfigGroup* layoutConfig = ConfigStorage::group("Layouts");
    _layoutCount = layoutConfig->value("DefaultCount", 0).toInt();
    _layoutCurrent = layoutConfig->value("DefaultCurrent", 0).toInt();
    delete layoutConfig;

    if (_layoutCount == 0) {
	_layoutCount++;
	return;
    }

    QString layoutPrefix = QString("Layout%1-MainView");
    _multiView->restoreLayout( layoutPrefix.arg(_layoutCurrent), traceKey());

    updateLayoutActions();
}


void QCGTopLevel::updateLayoutActions()
{
    if (_layoutNext)
	_layoutNext->setEnabled(_layoutCount>1);

    if (_layoutPrev)
	_layoutPrev->setEnabled(_layoutCount>1);

    if (_layoutRemove)
	_layoutRemove->setEnabled(_layoutCount>1);

    if (_statusbar)
        _statusbar->showMessage(tr("Layout Count: %1").arg(_layoutCount),
                                1000);
}


void QCGTopLevel::updateStatusBar()
{
  if (!_data || _data->parts().count()==0) {
    _statusLabel->setText(tr("No profile data file loaded."));
    return;
  }

  QString status = QString("%1 [%2] - ")
                   .arg(_data->shortTraceName())
                   .arg(_data->activePartRange());

  if (_eventType) {
      status += tr("Total %1 Cost: %2")
	  .arg(_eventType->longName())
	  .arg(_data->prettySubCost(_eventType));

    /* this gets too long...
    if (_eventType2 && (_eventType2 != _eventType))
      status += tr(", %1 Cost: %2")
	.arg(_eventType2->longName())
	.arg(_data->prettySubCost(_eventType2));
    */
  }
  else
    status += tr("No event type selected");

  /* Not working... should give group of selected function

  if (_groupType != ProfileContext::Function) {
    status += QString(" - %1 '%2'")
              .arg(ProfileContext::trTypeName(_groupType))
              .arg(_group ? _group->prettyName() : tr("(None)"));
  }
  */

  _statusLabel->setText(status);
}


void QCGTopLevel::closeEvent(QCloseEvent* event)
{
    GlobalConfig::config()->saveOptions();

    saveTraceSettings();
    saveCurrentState(QString::null);

    // if part dock was chosen visible even for only 1 part loaded,
    // keep this choice...
    _forcePartDock = false;
    if (_data && (_data->parts().count()<2) && _partDock->isVisible())
	_forcePartDock=true;

    ConfigGroup* topConfig = ConfigStorage::group("TopWindow");
    topConfig->setValue("ForcePartDockVisible", _forcePartDock, false);
    topConfig->setValue("State", saveState());
    topConfig->setValue("Geometry", saveGeometry());
    delete topConfig;

    event->accept();
}


void QCGTopLevel::toggleSplitted()
{
    int count = _multiView->childCount();
    if (count<1) count = 1;
    if (count>2) count = 2;
    count = 3-count;
    _multiView->setChildCount(count);

    _splittedToggleAction->setChecked(count>1);
    _splitDirectionToggleAction->setEnabled(count>1);
    _splitDirectionToggleAction->setChecked(_multiView->orientation() ==
					    Qt::Horizontal);
}

void QCGTopLevel::toggleSplitDirection()
{
  _multiView->setOrientation( _splitDirectionToggleAction->isChecked() ?
                              Qt::Horizontal : Qt::Vertical );
}



// this is called after a config change in the dialog
void QCGTopLevel::configChanged()
{
  // invalidate found/cached dirs of source files
    if (_data)
	_data->resetSourceDirs();

    _partSelection->notifyChange(TraceItemView::configChanged);
    _stackSelection->refresh();
    _functionSelection->notifyChange(TraceItemView::configChanged);
    _multiView->notifyChange(TraceItemView::configChanged);
}



void QCGTopLevel::activePartsChangedSlot(const TracePartList& list)
{
  if (!_data) return;

  if (!_data->activateParts(list)) {
//    qDebug("QCGTopLevel::activePartsChangedSlot: No Change!");
    return;
  }
  _activeParts = list;

  _partSelection->set(list);
  _stackSelection->refresh();
  _functionSelection->set(list);
  _multiView->set(list);

  updateStatusBar();
}

void QCGTopLevel::partsHideSelectedSlotDelayed()
{
  QTimer::singleShot( 0, this, SLOT(partsHideSelectedSlot()) );
}

// this puts selected parts into hidden list,
// deselects them and makes the remaining parts selected
void QCGTopLevel::partsHideSelectedSlot()
{
  if (!_data) return;

  TracePartList newHidden, newActive;
  foreach(TracePart* part, _data->parts()) {
      if (_activeParts.contains(part) ||
          _hiddenParts.contains(part))
          newHidden.append(part);
      else
          newActive.append(part);
  }

  _hiddenParts = newHidden;
  _partSelection->hiddenPartsChangedSlot(_hiddenParts);

#if 0
  _mainWidget1->hiddenPartsChangedSlot(_hiddenParts);
  _mainWidget2->hiddenPartsChangedSlot(_hiddenParts);
#endif

  activePartsChangedSlot(newActive);
}

void QCGTopLevel::partsUnhideAllSlotDelayed()
{
  QTimer::singleShot( 0, this, SLOT(partsUnhideAllSlot()) );
}

// this unhides all hidden parts. Does NOT change selection
void QCGTopLevel::partsUnhideAllSlot()
{
  if (!_data) return;

  _hiddenParts.clear();
  _partSelection->hiddenPartsChangedSlot(_hiddenParts);

#if 0
  _mainWidget1->hiddenPartsChangedSlot(_hiddenParts);
  _mainWidget2->hiddenPartsChangedSlot(_hiddenParts);
#endif
}

void QCGTopLevel::forwardAboutToShow()
{
  QMenu *popup = _forwardAction->menu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f;
  QAction* action;

  if (!hi) {
    popup->addAction(tr("(No Stack)"));
    return;
  }

  hi = hi->next();
  if (!hi) {
    popup->addAction(tr("(No next function)"));
    return;
  }

  int count = 1;
  while (count<GlobalConfig::maxSymbolCount() && hi) {
    f = hi->function();
    if (!f) break;

    QString name = GlobalConfig::shortenSymbol(f->prettyName());

    //qDebug("forward: Adding %s", name.toAscii());
    action = popup->addAction(name);
    action->setData(count);

    hi = hi->next();
    count++;
  }
}

void QCGTopLevel::backAboutToShow()
{
  QMenu *popup = _backAction->menu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f;
  QAction* action;

  if (!hi) {
    popup->addAction(tr("(No Stack)"));
    return;
  }

  hi = hi->last();
  if (!hi) {
    popup->addAction(tr("(No previous function)"));
    return;
  }

  int count = 1;
  while (count<GlobalConfig::maxSymbolCount() && hi) {
    f = hi->function();
    if (!f) break;

    QString name = GlobalConfig::shortenSymbol(f->prettyName());

    //qDebug("back: Adding %s", name.toAscii());
    action = popup->addAction(name);
    action->setData(count);

    hi = hi->last();
    count++;
  }
}

void QCGTopLevel::upAboutToShow()
{
  QMenu *popup = _upAction->menu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f = hi ? hi->function() : 0;
  QAction* action;

  if (!f) {
    popup->addAction(tr("(No Stack)"));
    return;
  }
  f = hi->stack()->caller(f, false);
  if (!f) {
    popup->addAction(tr("(No Function Up)"));
    return;
  }

  int count = 1;
  while (count<GlobalConfig::maxSymbolCount() && f) {
    QString name = GlobalConfig::shortenSymbol(f->prettyName());

    action = popup->addAction(name);
    action->setData(count);

    f = hi->stack()->caller(f, false);
    count++;
  }
}

void QCGTopLevel::forwardTriggered(QAction* action)
{
  int count = action->data().toInt(0);
  //qDebug("forwardTriggered: %d", count);
  if( count <= 0)
      return;

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  if (!b) return;

  while (count>1) {
    b->goForward();
    count--;
  }
  _stackSelection->browserForward();
}

void QCGTopLevel::backTriggered(QAction* action)
{
  int count = action->data().toInt(0);
  //qDebug("backTriggered: %d", count);
  if( count <= 0)
     return;

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  if (!b) return;

  while (count>1) {
    b->goBack();
    count--;
  }
  _stackSelection->browserBack();
}

void QCGTopLevel::upTriggered(QAction* action)
{
  int count = action->data().toInt(0);
  //qDebug("upTriggered: %d", count);
  if( count <= 0)
     return;

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  if (!hi) return;

  TraceFunction* f = hi->function();

  while (count>0 && f) {
    f = hi->stack()->caller(f, false);
    count--;
  }

  //qDebug("upActivated: %s", f ? f->prettyName().toAscii() : "??" );
  if (f)
    setFunction(f);
}

void QCGTopLevel::showMessage(const QString& msg, int ms)
{
	if (_statusbar)
		_statusbar->showMessage(msg, ms);
}

void QCGTopLevel::showStatus(const QString& msg, int progress)
{
	static bool msgUpdateNeeded = true;

	if (!_statusbar) return;

	if (msg.isEmpty()) {
	        //reset status
		if (_progressBar) {
		    _statusbar->removeWidget(_progressBar);
		    delete _progressBar;
		    _progressBar = 0;
		}
		_statusbar->clearMessage();
		_progressMsg = msg;
		return;
	}

	if (_progressMsg.isEmpty())
		_progressStart.start();

	if (msg != _progressMsg) {
		_progressMsg = msg;
		msgUpdateNeeded = true;
	}

	// do nothing if last change was less than 0.5 seconds ago
	if (_progressStart.elapsed() < 500)
		return;

	if (!_progressBar) {
		_progressBar = new QProgressBar(_statusbar);
		_progressBar->setMaximumSize(200, _statusbar->height()-4);
		_statusbar->addPermanentWidget(_progressBar, 1);
		_progressBar->show();
		msgUpdateNeeded = true;
	}

	_progressStart.restart();

	if (msgUpdateNeeded) {
		_statusbar->showMessage(msg);
		msgUpdateNeeded = false;
	}
	_progressBar->setValue(progress);

	// let the progress bar update itself
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void QCGTopLevel::loadStart(const QString& filename)
{
    showStatus(QString("Loading %1").arg(filename), 0);
    Logger::_filename = filename;
}

void QCGTopLevel::loadFinished(const QString& msg)
{
    showStatus(QString::null, 0);
    if (!msg.isEmpty())
	showMessage(QString("Error loading %1: %2").arg(_filename).arg(msg),
		    2000);
}

void QCGTopLevel::loadProgress(int progress)
{
    showStatus(QString("Loading %1").arg(_filename), progress);
}

void QCGTopLevel::loadError(int line, const QString& msg)
{
    qCritical() << "Loading" << _filename << ":" << line << ": " << msg.toAscii();
}

void QCGTopLevel::loadWarning(int line, const QString& msg)
{
    qWarning() << "Loading" << _filename.toAscii() << ":" << line << ": " << msg.toAscii();
}

#include "qcgtoplevel.moc"
