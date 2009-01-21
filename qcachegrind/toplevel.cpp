/* This file is part of KCachegrind.
   Copyright (C) 2002, 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
#define ENABLE_DUMPDOCK 0

#include "toplevel.h"

#include <stdlib.h> // for system()

#include <QDebug>
#include <QDockWidget>
#include <QTimer>
#include <QByteArray>
#include <QLabel>
#include <QStatusBar>
#include <QMenuBar>
#include <QLineEdit>
#include <QTextStream>
#include <QSizePolicy>
#include <QProgressBar>
#include <QFile>
#include <QFileDialog>
#include <QEventLoop>
#include <QtDBus/QDBusConnection>

#if ENABLE_DUMPDOCK
#include "dumpselection.h"
#endif

#include "partselection.h"
#include "functionselection.h"
//#include "stackselection.h"
//#include "stackbrowser.h"
#include "tracedata.h"
#include "config.h"
#include "globalconfig.h"
//#include "configdlg.h"
#include "multiview.h"
#include "callgraphview.h"

TopLevel::TopLevel()
{
    QDBusConnection::sessionBus().registerObject("/QCachegrind", this,
						 QDBusConnection::ExportScriptableSlots);
  init();

  createDocks();

  _multiView = new MultiView(this, this);
  _multiView->setObjectName("MultiView");
  setCentralWidget(_multiView);

  createActions();

  _partDockShown->setChecked(!_partDock->isHidden());
  _stackDockShown->setChecked(!_stackDock->isHidden());
  _functionDockShown->setChecked(!_functionDock->isHidden());

  connect(_partDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(partVisibilityChanged(bool)));
  connect(_stackDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(stackVisibilityChanged(bool)));
  connect(_functionDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(functionVisibilityChanged(bool)));

#if ENABLE_DUMPDOCK
  _dumpDockShown->setChecked(!_dumpDock->isHidden());
  connect(_dumpDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(dumpVisibilityChanged(bool)));
#endif

  _statusbar = statusBar();
  _statusLabel = new QLabel(_statusbar);
#if 0
  // how to do avoid main window resizing on large statusbar label?
  QSizePolicy p(QSizePolicy::Fixed, QSizePolicy::Expanding);
  _statusLabel->setSizePolicy(p);
  _statusbar->setSizePolicy(p);
#endif
  _statusbar->addWidget(_statusLabel, 1);

  GlobalConfig::config()->readOptions();
  //_openRecent->loadEntries( KConfigGroup( kconfig, "" ) );

  // set toggle after reading configuration
  _showPercentage = GlobalConfig::showPercentage();
  _showExpanded   = GlobalConfig::showExpanded();
  _showCycles     = GlobalConfig::showCycles();
  _taPercentage->setChecked(_showPercentage);
  _taExpanded->setChecked(_showExpanded);
  _taCycles->setChecked(_showCycles);

  setupPartSelection(_partSelection);

  // KCachegrind for KDE 3.0.x does not allow to hide toolbars...
  //setStandardToolBarMenuEnabled(true);

  // QT dock windows are created before (using QT position restoring)
  //createGUI();

  //setAutoSaveSettings();

  // restore current state settings (not configuration options)
  restoreCurrentState(QString::null);	//krazy:exclude=nullstrassign for old broken gcc

  // if this is the first toplevel, show tip of day
  //if (memberList().count() == 1)
  //  QTimer::singleShot( 200, this, SLOT(slotShowTipOnStart()) );
}

void TopLevel::init()
{
  _activeParts.clear();
  _hiddenParts.clear();

  _progressBar = 0;

  _data = 0;
  _function = 0;
  _eventType = 0;
  _eventType2 = 0;
  _groupType = TraceCost::NoCostType;
  _group = 0;

  _layoutCurrent = 0;
  _layoutCount = 1;

  // for delayed slots
  _traceItemDelayed = 0;
  _eventTypeDelayed = 0;
  _eventType2Delayed = 0;
  _groupTypeDelayed = TraceCost::NoCostType;
  _groupDelayed = 0;
  _directionDelayed = TraceItemView::None;
  _lastSender = 0;
}


/**
 * Setup the part selection widget.
 * Statusbar has to be created before.
 */
void TopLevel::setupPartSelection(PartSelection* ps)
{
  // setup connections from the part selection widget

  connect(ps, SIGNAL(partsHideSelected()),
          this, SLOT(partsHideSelectedSlotDelayed()));
  connect(ps, SIGNAL(partsUnhideAll()),
          this, SLOT(partsUnhideAllSlotDelayed()));
}

/**
 * This saves the current state of the main window and
 * sub widgets.
 *
 * No positions are saved. These is done automatically for
 * KToolbar, and manually in queryExit() for QT docks.
 */
void TopLevel::saveCurrentState(const QString& postfix)
{
  QString eventType = _eventType ? _eventType->name() : QString("?");
  QString eventType2 = _eventType2 ? _eventType2->name() : QString("?");

  ConfigGroup* stateConfig = ConfigStorage::group(QString("CurrentState") + postfix);
  stateConfig->setValue("EventType", eventType);
  stateConfig->setValue("EventType2", eventType2);
  stateConfig->setValue("GroupType", TraceItem::typeName(_groupType));
  delete stateConfig;

  _partSelection->saveOptions(QString("PartOverview"), postfix);
  _multiView->saveLayout(QString("MainView"), postfix);
  _multiView->saveOptions(QString("MainView"), postfix);
}

/**
 * This function is called when a trace is closed.
 * Save browsing position for later restoring
 */
void TopLevel::saveTraceSettings()
{
  QString key = traceKey();
  QString eventType = _eventType ? _eventType->name() : QString("?");
  QString eventType2 = _eventType2 ? _eventType2->name() : QString("?");

  ConfigGroup* lConfig = ConfigStorage::group("Layouts");
  lConfig->setValue(QString("Count%1").arg(key), _layoutCount);
  lConfig->setValue(QString("Current%1").arg(key), _layoutCurrent);
  delete lConfig;

  ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
  pConfig->setValue(QString("EventType%1").arg(key), eventType);
  pConfig->setValue(QString("EventType2%1").arg(key), eventType2);
  pConfig->setValue(QString("GroupType%1").arg(key),
		    TraceItem::typeName(_groupType));

  if (_data) {
      pConfig->setValue(QString("Group%1").arg(key),
			_group ? _group->name() : QString::null); //krazy:exclude=nullstrassign for old broken gcc
      saveCurrentState(key);
  }
  delete pConfig;
}

/**
 * This restores the current state of the main window and
 * sub widgets.
 *
 * This does NOT restore any positions. This is done automatically for
 * KToolbar, and manually in the createDocks() for QT docks..
 */
void TopLevel::restoreCurrentState(const QString& postfix)
{
  _partSelection->restoreOptions(QString("PartOverview"), postfix);
  _multiView->restoreLayout(QString("MainView"), postfix);
  _multiView->restoreOptions(QString("MainView"), postfix);

  _taSplit->setChecked(_multiView->childCount()>1);
  _taSplitDir->setEnabled(_multiView->childCount()>1);
  _taSplitDir->setChecked(_multiView->orientation() == Qt::Horizontal);
}


void TopLevel::createDocks()
{
  _partDock = new QDockWidget(this);
  _partDock->setObjectName("part dock");
  _partDock->setWindowTitle(tr("Parts Overview"));
  _partSelection = new PartSelection(this, _partDock);
  _partDock->setWidget(_partSelection);

  _stackDock = new QDockWidget(this);
  _stackDock->setObjectName("stack dock");
  // Why is the caption only correct with a close button?
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

  //connect(_stackSelection, SIGNAL(functionSelected(TraceItem*)),
  //        this, SLOT(setTraceItemDelayed(TraceItem*)));

  _functionDock = new QDockWidget(this);
  _functionDock->setObjectName("function dock");
  _functionDock->setWindowTitle(tr("Flat Profile"));
  _functionSelection = new FunctionSelection(this, _functionDock);
  _functionDock->setWidget(_functionSelection);

#if ENABLE_DUMPDOCK
  _dumpDock = new QDockWidget(this);
  _dumpDock->setWindowTitle(tr("Profile Dumps"));
  _dumpSelection = new DumpSelection(this, _dumpDock,
                                     "dumpSelection");
  _dumpSelection->setTopLevel(this);

  _dumpDock->setWidget(_dumpSelection);
  _dumpSelection->setWhatsThis( tr(
                   "<b>Profile Dumps</b>"
                   "<p>This dockable shows in the top part the list of "
                   "loadable profile dumps in all subdirectories of: "
                   "<ul><li>current working directory of KCachegrind, "
                   "i.e. where it was started from, and </li>"
                   "<li>the default profile dump directory given in the "
                   "configuration.</li></ul> "
                   "The list is sorted according to the target command "
                   "profiled in the corresponding dump.</p>"
                   "<p>On selecting a profile dump, information for it "
                   "is shown in the bottom area of the dockable: "
                   "<ul><li><b>Options</b> allows you to view the profiled "
                   "command and profile options of this dump. By changing "
                   "any item, a new (yet unexisting) profile template "
                   "is created. Press <b>Run Profile</b> to start a "
                   "profile run with these options in the background. </li>"
                   "<li><b>Info</b> gives detailed info on the selected "
                   "dump like event cost summary and properties of the "
                   "simulated cache. </li>"
                   "<li><b>State</b> is only available for current happening "
                   "profiles runs. Press <b>Update</b> to see different "
                   "counters of the run, and a stack trace of the current "
                   "position in the program profiled. Check the <b>Every</b> "
                   "option to let KCachegrind regularly poll these data. "
                   "Check the <b>Sync</b> option to let the dockable activate "
                   "the top function in the current loaded dump.</li></ul></p>"));
#endif

	// default positions, will be adjusted automatically by stored state in config
	addDockWidget(Qt::LeftDockWidgetArea, _partDock );
	addDockWidget(Qt::LeftDockWidgetArea, _stackDock );
	addDockWidget(Qt::LeftDockWidgetArea, _functionDock );
	_stackDock->hide();

#if ENABLE_DUMPDOCK
	addDockWidget( Qt::LeftDockWidgetArea, _dumpDock );
	_dumpDock->hide();
#endif

	ConfigGroup* dockConfig = ConfigStorage::group("Docks");
	_forcePartDock = dockConfig->value("ForcePartDockVisible", false).toBool();
	delete dockConfig;
}


TopLevel::~TopLevel()
{
  delete _data;
}

#if 0
void TopLevel::saveProperties(KConfigGroup & c)
{
  c.writeEntry("TraceName", _data->traceName());
}

void TopLevel::readProperties(const KConfigGroup &c)
{
  QString traceName = c.readEntry("TraceName");
  if (!traceName.isEmpty()) {
      TraceData* d = new TraceData(this);
      d->load(traceName);
      setData(d);
  }
}
#endif

void TopLevel::createLayoutActions()
{
#if 0
  QString hint;
  QAction* action;

  action = addAction( "layout_duplicate" );
  action->setText( tr( "&Duplicate" ) );
  connect(action, SIGNAL(triggered(bool)), SLOT(layoutDuplicate()));
  action->setShortcuts(KShortcut(Qt::CTRL+Qt::Key_Plus));
  hint = tr("<b>Duplicate Current Layout</b>"
              "<p>Make a copy of the current layout.</p>");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "layout_remove" );
  action->setText( tr( "&Remove" ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT(layoutRemove()));
  hint = tr("<b>Remove Current Layout</b>"
              "<p>Delete current layout and make the previous active.</p>");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "layout_next" );
  action->setText( tr( "&Go to Next" ) );
  connect(action, SIGNAL(triggered(bool)), SLOT(layoutNext()));
  action->setShortcuts(KShortcut(Qt::CTRL+Qt::Key_Right));
  hint = tr("Go to Next Layout");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "layout_previous" );
  action->setText( tr( "&Go to Previous" ) );
  connect(action, SIGNAL(triggered(bool)), SLOT(layoutPrevious()));
  action->setShortcuts(KShortcut(Qt::CTRL+Qt::Key_Left));
  hint = tr("Go to Previous Layout");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "layout_restore" );
  action->setText( tr( "&Restore to Default" ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT(layoutRestore()));
  hint = tr("Restore Layouts to Default");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "layout_save" );
  action->setText( tr( "&Save as Default" ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT(layoutSave()));
  hint = tr("Save Layouts as Default");
  action->setWhatsThis( hint );
#endif
}

// TODO: split this up...
void TopLevel::createMiscActions()
{
  QString hint;
  QAction* action;
  QMenuBar* mBar = menuBar();
  QMenu* fileMenu = mBar->addMenu("&File");

  action = new QAction(tr("&New"), this);
  action->setShortcut(tr("Ctrl+N"));
  action->setStatusTip(tr("Open new empty window"));
  connect(action, SIGNAL(triggered()), this, SLOT(newWindow()));
  fileMenu->addAction(action);

  _partDockShown = new QAction(tr("Show Parts"), this);
  _stackDockShown = new QAction(tr("Show Stack"), this);
  _functionDockShown = new QAction(tr("Show Profile"), this);

  _taPercentage = new QAction(this);
  _taExpanded = new QAction(this);
  _taCycles = new QAction(this);
  _taSplit = new QAction(this);
  _taSplitDir = new QAction(this);

#if 0

  action = actionCollection()->addAction( "file_add" );
  action->setText( tr( "&Add..." ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT(addTrace()));
  hint = tr("<b>Add Profile Data</b>"
              "<p>This opens an additional profile data file in the current window.</p>");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "reload" );
  action->setIcon( KIcon("view-refresh") );
  action->setText( trc("Reload a document", "&Reload" ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT( reload() ));
  action->setShortcuts(KStandardShortcut::shortcut(KStandardShortcut::Reload));
  hint = tr("<b>Reload Profile Data</b>"
              "<p>This loads any new created parts, too.</p>");
  action->setWhatsThis( hint );

  action = actionCollection()->addAction( "export" );
  action->setText( tr( "&Export Graph" ) );
  connect(action, SIGNAL(triggered(bool) ), SLOT(exportGraph()));

  hint = tr("<b>Export Call Graph</b>"
              "<p>Generates a file with extension .dot for the tools "
              "of the GraphViz package.</p>");
  action->setWhatsThis( hint );


  _taDump = actionCollection()->add<KToggleAction>( "dump" );
  _taDump->setIcon( KIcon("edit-redo") );
  _taDump->setText( tr( "&Force Dump" ) );
  connect(_taDump, SIGNAL(triggered(bool) ), SLOT( forceTrace() ));
  _taDump->setShortcut(KStandardShortcut::shortcut(KStandardShortcut::Redo));
  hint = tr("<b>Force Dump</b>"
              "<p>This forces a dump for a Callgrind profile run "
              "in the current directory. This action is checked while "
              "KCachegrind looks for the dump. If the dump is "
              "finished, it automatically reloads the current trace. "
              "If this is the one from the running Callgrind, the new "
              "created trace part will be loaded, too.</p>"
              "<p>Force dump creates a file 'callgrind.cmd', and "
              "checks every second for its existence. A running "
              "Callgrind will detect this file, dump a trace part, "
              "and delete 'callgrind.cmd'. "
              "The deletion is detected by KCachegrind, "
              "and it does a Reload. If there is <em>no</em> Callgrind "
              "running, press 'Force Dump' again to cancel the dump "
              "request. This deletes 'callgrind.cmd' itself and "
              "stops polling for a new dump.</p>"
              "<p>Note: A Callgrind run <em>only</em> detects "
              "existence of 'callgrind.cmd' when actively running "
              "a few milliseconds, i.e. "
              "<em>not</em> sleeping. Tip: For a profiled GUI program, "
              "you can awake Callgrind e.g. by resizing a window "
              "of the program.</p>");
  _taDump->setWhatsThis( hint );

  action = KStandardAction::open(this, SLOT(loadTrace()), actionCollection());
  hint = tr("<b>Open Profile Data</b>"
              "<p>This opens a profile data file, with possible multiple parts</p>");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  _openRecent = KStandardAction::openRecent(this, SLOT(loadTrace(const KUrl&)),
                                       actionCollection());

  KStandardAction::showStatusbar(this,
                            SLOT(toggleStatusBar()), actionCollection());

  _partDockShown = actionCollection()->add<KToggleAction>("settings_show_partdock");
  _partDockShown->setText(tr("Parts Overview"));
  connect(_partDockShown, SIGNAL(triggered(bool) ), SLOT(togglePartDock()));

  hint = tr("Show/Hide the Parts Overview Dockable");
  _partDockShown->setToolTip( hint );
  _partDockShown->setWhatsThis( hint );

  _stackDockShown = actionCollection()->add<KToggleAction>("settings_show_stackdock");
  _stackDockShown->setText(tr("Call Stack"));
  connect(_stackDockShown, SIGNAL(triggered(bool) ), SLOT(toggleStackDock()));

  hint = tr("Show/Hide the Call Stack Dockable");
  _stackDockShown->setToolTip( hint );
  _stackDockShown->setWhatsThis( hint );

  _functionDockShown = actionCollection()->add<KToggleAction>("settings_show_profiledock");
  _functionDockShown->setText(tr("Function Profile"));
  connect(_functionDockShown, SIGNAL(triggered(bool) ), SLOT(toggleFunctionDock()));

  hint = tr("Show/Hide the Function Profile Dockable");
  _functionDockShown->setToolTip( hint );
  _functionDockShown->setWhatsThis( hint );

#if ENABLE_DUMPDOCK
  _dumpDockShown = actionCollection()->add<KToggleAction>("settings_show_dumpdock",
                                                          this, SLOT(toggleDumpDock()));
  _dumpDockShown->setText(tr("Profile Dumps"));
  hint = tr("Show/Hide the Profile Dumps Dockable");
  _dumpDockShown->setToolTip( hint );
  _dumpDockShown->setWhatsThis( hint );
#endif

  _taPercentage = actionCollection()->add<KToggleAction>("view_percentage");
  _taPercentage->setIcon(KIcon("percent"));
  _taPercentage->setText(tr("Show Relative Costs"));
  connect(_taPercentage, SIGNAL(triggered(bool) ), SLOT(togglePercentage()));
  _taPercentage->setCheckedState(KGuiItem(tr("Show Absolute Costs")));

  hint = tr("Show relative instead of absolute costs");
  _taPercentage->setToolTip( hint );
  _taPercentage->setWhatsThis( hint );

  _taExpanded = actionCollection()->add<KToggleAction>("view_expanded");
  _taExpanded->setIcon(KIcon("move"));
  _taExpanded->setText(tr("Percentage Relative to Parent"));
  connect(_taExpanded, SIGNAL(triggered(bool) ), SLOT(toggleExpanded()));

  hint = tr("Show percentage costs relative to parent");
  _taExpanded->setToolTip( hint );
  _taExpanded->setWhatsThis( hint );

  hint = tr("<b>Show percentage costs relative to parent</b>"
              "<p>If this is switched off, percentage costs are always shown "
              "relative to the total cost of the profile part(s) that are "
              "currently browsed. By turning on this option, percentage cost "
              "of shown cost items will be relative to the parent cost item.</p>"
              "<ul><table>"
              "<tr><td><b>Cost Type</b></td><td><b>Parent Cost</b></td></tr>"
              "<tr><td>Function Cumulative</td><td>Total</td></tr>"
              "<tr><td>Function Self</td><td>Function Group (*) / Total</td></tr>"
              "<tr><td>Call</td><td>Function Inclusive</td></tr>"
              "<tr><td>Source Line</td><td>Function Inclusive</td></tr>"
              "</table></ul>"
              "<p>(*) Only if function grouping is switched on (e.g. ELF object grouping).</p>");
  _taExpanded->setWhatsThis( hint );

  _taCycles = actionCollection()->add<KToggleAction>("view_cycles");
  _taCycles->setIcon(KIcon("edit-undo"));
  _taCycles->setText(tr( "Do Cycle Detection" ));
  connect(_taCycles, SIGNAL(triggered(bool) ), SLOT( toggleCycles() ));
  _taCycles->setCheckedState(KGuiItem(tr("Skip Cycle Detection")));

  hint = tr("<b>Detect recursive cycles</b>"
              "<p>If this is switched off, the treemap drawing will show "
              "black areas when a recursive call is made instead of drawing the "
              "recursion ad infinitum. Note that "
              "the size of black areas often will be wrong, as inside recursive "
              "cycles the cost of calls cannot be determined; the error is small, "
              "however, for false cycles (see documentation).</p>"
              "<p>The correct handling for cycles is to detect them and collapse all "
              "functions of a cycle into an artificial function, which is done when this "
              "option is selected. Unfortunately, with GUI applications, this often will "
              "lead to huge false cycles, making the analysis impossible; therefore, there "
              "is the option to switch this off.</p>");
  _taCycles->setWhatsThis( hint );

  KStandardAction::quit(this, SLOT(close()), actionCollection());
  KStandardAction::preferences(this, SLOT(configure()), actionCollection());
  KStandardAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
  KStandardAction::configureToolbars(this,SLOT(configureToolbars()),
                                actionCollection());
#if 0
  action = KStandardAction::back(_stackSelection, SLOT(browserBack()),
                            actionCollection());
  hint = tr("Go back in function selection history");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  action = KStandardAction::forward(_stackSelection, SLOT(browserForward()),
                      actionCollection());
  hint = tr("Go forward in function selection history");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  action = KStandardAction::up(_stackSelection, SLOT(browserUp()),
                      actionCollection());
  hint = tr("<b>Go Up</b>"
              "<p>Go to last selected caller of current function. "
              "If no caller was visited, use that with highest cost.</p>");
  action->setToolTip( hint );
  action->setWhatsThis( hint );
#else
  _paUp = new KToolBarPopupAction( KIcon( "go-up" ), tr( "&Up" ), this );
  _paUp->setShortcuts( KShortcut(Qt::ALT+Qt::Key_Up) );
  connect( _paUp, SIGNAL( triggered( bool ) ), _stackSelection, SLOT( browserUp() ) );
  actionCollection()->addAction( "go_up", _paUp );
  connect( _paUp->menu(), SIGNAL( aboutToShow() ),
	    this, SLOT( upAboutToShow() ) );
  connect( _paUp->menu(), SIGNAL( triggered( QAction* ) ),
	    this, SLOT( upTriggered( QAction* ) ) );
  hint = tr("<b>Go Up</b>"
              "<p>Go to last selected caller of current function. "
              "If no caller was visited, use that with highest cost.</p>");
  _paUp->setToolTip( hint );
  _paUp->setWhatsThis( hint );

  QPair< KGuiItem, KGuiItem > backForward = KStandardGuiItem::backAndForward();
  _paBack = new KToolBarPopupAction( backForward.first.icon(), backForward.first.text(), this );
  _paBack->setShortcuts( KShortcut(Qt::ALT+Qt::Key_Left) );
  connect( _paBack, SIGNAL( triggered( bool ) ), _stackSelection, SLOT( browserBack() ) );
  actionCollection()->addAction( "go_back", _paBack );
  connect( _paBack->menu(), SIGNAL( aboutToShow() ),
           this, SLOT( backAboutToShow() ) );
  connect( _paBack->menu(), SIGNAL( triggered( QAction* ) ),
           this, SLOT( backTriggered( QAction* ) ) );
  hint = tr("Go back in function selection history");
  _paBack->setToolTip( hint );
  _paBack->setWhatsThis( hint );

  _paForward = new KToolBarPopupAction( backForward.second.icon(), backForward.second.text(), this );
  _paForward->setShortcuts( KShortcut(Qt::ALT+Qt::Key_Right) );
  connect( _paForward, SIGNAL( triggered( bool ) ), _stackSelection, SLOT( browserForward() ) );
  actionCollection()->addAction( "go_forward", _paForward );
  connect( _paForward->menu(), SIGNAL( aboutToShow() ),
           this, SLOT( forwardAboutToShow() ) );
  connect( _paForward->menu(), SIGNAL( triggered( QAction* ) ),
           this, SLOT( forwardTriggered( QAction* ) ) );
  hint = tr("Go forward in function selection history");
  _paForward->setToolTip( hint );
  _paForward->setWhatsThis( hint );
#endif

  _saCost = actionCollection()->add<KSelectAction>("view_cost_type");
  _saCost->setText(tr("Primary Event Type"));
  hint = tr("Select primary event type of costs");
  _saCost->setComboWidth(300);
  _saCost->setToolTip( hint );
  _saCost->setWhatsThis( hint );

  // A dummy entry forces a minimum size of combobox in toolbar
  QStringList dummyItems;
  dummyItems << tr("(Placeholder for list of event types)");
  _saCost->setItems(dummyItems);

  // cost types are dependent on loaded data, thus KSelectAction
  // is filled in setData()
  connect( _saCost, SIGNAL(triggered(const QString&)),
           this, SLOT(eventTypeSelected(const QString&)));

  _saCost2 = actionCollection()->add<KSelectAction>("view_cost_type2");
  _saCost2->setText(tr("Secondary Event Type"));
  hint = tr("Select secondary event type for cost e.g. shown in annotations");
  _saCost2->setComboWidth(300);
  _saCost2->setToolTip( hint );
  _saCost2->setWhatsThis( hint );
  _saCost2->setItems(dummyItems);

  connect( _saCost2, SIGNAL(triggered(const QString&)),
           this, SLOT(eventType2Selected(const QString&)));

  saGroup = actionCollection()->add<KSelectAction>("view_group_type");
  saGroup->setText(tr("Grouping"));

  hint = tr("Select how functions are grouped into higher level cost items");
  saGroup->setToolTip( hint );
  saGroup->setWhatsThis( hint );

  QStringList args;

  args << tr("(No Grouping)")
       << TraceCost::trTypeName(TraceItem::Object)
       << TraceCost::trTypeName(TraceItem::File)
       << TraceCost::trTypeName(TraceItem::Class)
       << TraceCost::trTypeName(TraceItem::FunctionCycle);

  saGroup->setItems(args);
  connect( saGroup, SIGNAL(triggered(int)),
           this, SLOT(groupTypeSelected(int)));

  _taSplit = actionCollection()->add<KToggleAction>("view_split");
  _taSplit->setIcon(KIcon("view-split-left-right"));
  _taSplit->setText(tr("Split"));
  connect(_taSplit, SIGNAL(triggered(bool) ), SLOT(splitSlot()));

  hint = tr("Show two information panels");
  _taSplit->setToolTip( hint );
  _taSplit->setWhatsThis( hint );

 _taSplitDir = actionCollection()->add<KToggleAction>("view_split_dir");
 _taSplitDir->setIcon(KIcon("view-split-left-right"));
 _taSplitDir->setText(tr("Split Horizontal"));
 connect(_taSplitDir, SIGNAL(triggered(bool) ), SLOT(splitDirSlot()));

  hint = tr("Change Split Orientation when main window is split.");
  _taSplitDir->setToolTip( hint );
  _taSplitDir->setWhatsThis( hint );

  // copied from KMail...
  KStandardAction::tipOfDay( this, SLOT( slotShowTip() ), actionCollection() );
#endif
}

void TopLevel::createActions()
{
  createMiscActions();
  createLayoutActions();
}

void TopLevel::toggleStatusBar()
{
  if (statusBar()->isVisible())
    statusBar()->hide();
  else
    statusBar()->show();
}

void TopLevel::togglePartDock()
{
  if (!_partDock->isVisible())
    _partDock->show();
  else
    _partDock->hide();
}

void TopLevel::toggleStackDock()
{
  if (!_stackDock->isVisible())
    _stackDock->show();
  else
    _stackDock->hide();
}

void TopLevel::toggleDumpDock()
{
#if ENABLE_DUMPDOCK
  if (!_dumpDock->isVisible())
    _dumpDock->show();
  else
    _dumpDock->hide();
#endif
}

void TopLevel::toggleFunctionDock()
{
  if (!_functionDock->isVisible())
    _functionDock->show();
  else
    _functionDock->hide();
}

void TopLevel::togglePercentage()
{
  setPercentage(_taPercentage->isChecked());
}

void TopLevel::setAbsoluteCost()
{
  setPercentage(false);
}

void TopLevel::setRelativeCost()
{
  setPercentage(true);
}

void TopLevel::setPercentage(bool show)
{
  if (_showPercentage == show) return;
  _showPercentage = show;
  if (_taPercentage->isChecked() != show)
    _taPercentage->setChecked(show);

  GlobalConfig::setShowPercentage(_showPercentage);

  _partSelection->notifyChange(TraceItemView::configChanged);
  _partSelection->updateView();

  //_stackSelection->refresh();

  _functionSelection->notifyChange(TraceItemView::configChanged);
  _functionSelection->updateView();

  _multiView->notifyChange(TraceItemView::configChanged);
  _multiView->updateView();
}

void TopLevel::toggleExpanded()
{
  bool show = _taExpanded->isChecked();
  if (_showExpanded == show) return;
  _showExpanded = show;

  GlobalConfig::setShowExpanded(_showExpanded);

  _partSelection->notifyChange(TraceItemView::configChanged);
  _partSelection->updateView();

  //_stackSelection->refresh();

  _functionSelection->notifyChange(TraceItemView::configChanged);
  _functionSelection->updateView();

  _multiView->notifyChange(TraceItemView::configChanged);
  _multiView->updateView();
}

void TopLevel::toggleCycles()
{
  bool show = _taCycles->isChecked();
  if (_showCycles == show) return;
  _showCycles = show;

  GlobalConfig::setShowCycles(_showCycles);

  if (!_data) return;

  _data->invalidateDynamicCost();
  _data->updateFunctionCycles();

  _partSelection->notifyChange(TraceItemView::configChanged);
  _partSelection->updateView();

  //_stackSelection->rebuildStackList();

  _functionSelection->notifyChange(TraceItemView::configChanged);
  _functionSelection->updateView();

  _multiView->notifyChange(TraceItemView::configChanged);
  _multiView->updateView();
}

void TopLevel::partVisibilityChanged(bool v)
{
  _partDockShown->setChecked(v);
}

void TopLevel::stackVisibilityChanged(bool v)
{
  _stackDockShown->setChecked(v);
}

#if ENABLE_DUMPDOCK
void TopLevel::dumpVisibilityChanged(bool v)
#else
void TopLevel::dumpVisibilityChanged(bool)
#endif
{
#if ENABLE_DUMPDOCK
  _dumpDockShown->setChecked(v);
#endif
}

void TopLevel::functionVisibilityChanged(bool v)
{
  _functionDockShown->setChecked(v);
  if (v)
    _functionSelection->updateView();
}


void TopLevel::querySlot()
{
//  _functionSelection->query(queryLineEdit->text());
}

void TopLevel::configureKeys()
{
    //KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}


void TopLevel::configureToolbars()
{
#if 0
  KEditToolBar *dlg = new KEditToolBar(guiFactory(), this);

  if (dlg->exec())
    createGUI();

  delete dlg;
#endif
}


void TopLevel::newTrace()
{
  // start cachegrind on command...
}

void TopLevel::newWindow()
{
  TopLevel* t = new TopLevel();
  t->show();
}


void TopLevel::loadTrace()
{
    QString fileName = QFileDialog::getOpenFileName(this,
						    tr("Open Callgrind Data"),
						    QString::null,
						    tr("Callgrind Files (callgrind.*)"));
    loadTrace(fileName);
}

void TopLevel::loadTrace(QString file)
{
  if (file.isEmpty()) return;

  if (_data && _data->parts().count()>0) {

    // In new window
    TopLevel* t = new TopLevel();
    t->show();
    t->loadDelayed(file);
    return;
  }

  // this constructor enables progress bar callbacks
  TraceData* d = new TraceData(this);
  d->load(file);
  setData(d);
}


void TopLevel::addTrace()
{
    QString fileName = QFileDialog::getOpenFileName(this,
						    tr("Add Callgrind Data"),
						    QString::null,
						    tr("Callgrind Files (callgrind.*)"));
    addTrace(fileName);
}


void TopLevel::addTrace(QString file)
{
  if (file.isEmpty()) return;

  if (_data) {
    _data->load(file);

    // GUI update for added data
    configChanged();
    return;
  }

  // this constructor enables progress bar callbacks
  TraceData* d = new TraceData(this);
  d->load(file);
  setData(d);
}



void TopLevel::loadDelayed(QString file)
{
  _loadTraceDelayed = file;
  QTimer::singleShot(0, this, SLOT(loadTraceDelayed()));
}

void TopLevel::loadTraceDelayed()
{
  if (_loadTraceDelayed.isEmpty()) return;

  loadTrace(_loadTraceDelayed);
  _loadTraceDelayed = QString();
}


void TopLevel::reload()
{
  QString trace;
  if (!_data || _data->parts().count()==0)
    trace = "."; // open first trace found in dir
  else
    trace = _data->traceName();

  // this also keeps sure we have the same browsing position...
  TraceData* d = new TraceData(this);
  d->load(trace);
  setData(d);
}

void TopLevel::exportGraph()
{
  if (!_data || !_function) return;

  QString n = QString("callgraph.dot");
  GraphExporter ge(_data, _function, _eventType, _groupType, n);
  ge.writeDot();

  QString cmd = QString("(dot %1 -Tps > %2.ps; kghostview %3.ps)&")
                .arg(n).arg(n).arg(n);
  system(QFile::encodeName( cmd ));
}


bool TopLevel::setEventType(QString s)
{
  TraceEventType* ct;

  ct = (_data) ? _data->mapping()->type(s) : 0;

  // if costtype with given name not found, use first available
  if (!ct && _data) ct = _data->mapping()->type(0);

  return setEventType(ct);
}

bool TopLevel::setEventType2(QString s)
{
  TraceEventType* ct;

  // Special type tr("(Hidden)") gives 0
  ct = (_data) ? _data->mapping()->type(s) : 0;

  return setEventType2(ct);
}

void TopLevel::eventTypeSelected(const QString& s)
{
  TraceEventType* ct;

  ct = (_data) ? _data->mapping()->typeForLong(s) : 0;
  setEventType(ct);
}

void TopLevel::eventType2Selected(const QString& s)
{
  TraceEventType* ct;

  ct = (_data) ? _data->mapping()->typeForLong(s) : 0;
  setEventType2(ct);
}

bool TopLevel::setEventType(TraceEventType* ct)
{
  if (_eventType == ct) return false;
  _eventType = ct;

  if (ct) {
      int idx=0;
      QStringList l;// = _saCost->items();
      for (QStringList::const_iterator it = l.begin(); it != l.end(); ++it, ++idx ) {
	  if (*it == ct->longName())
	  {}//_saCost->setCurrentItem(idx);
      }
  }

  _partSelection->setEventType(_eventType);
  //_stackSelection->setEventType(_eventType);

  _functionSelection->setEventType(_eventType);
  _functionSelection->updateView();

  _multiView->setEventType(_eventType);
  _multiView->updateView();

  updateStatusBar();

  return true;
}

bool TopLevel::setEventType2(TraceEventType* ct)
{
  if (_eventType2 == ct) return false;
  _eventType2 = ct;

  QString longName = ct ? ct->longName() : tr("(Hidden)");

  int idx=0;
  QStringList l;// = _saCost2->items();
  for (QStringList::const_iterator it = l.begin(); it != l.end(); ++it, ++idx ) {
    if (*it == longName)
    {}//_saCost2->setCurrentItem(idx);
  }

  _partSelection->setEventType2(_eventType2);
  //_stackSelection->setEventType2(_eventType2);

  _functionSelection->setEventType2(_eventType2);
  _functionSelection->updateView();

  _multiView->setEventType2(_eventType2);
  _multiView->updateView();

  updateStatusBar();

  return true;
}


void TopLevel::groupTypeSelected(int cg)
{
  switch(cg) {
  case 0: setGroupType( TraceItem::Function ); break;
  case 1: setGroupType( TraceItem::Object ); break;
  case 2: setGroupType( TraceItem::File ); break;
  case 3: setGroupType( TraceItem::Class ); break;
  case 4: setGroupType( TraceItem::FunctionCycle ); break;
  default: break;
  }
}

bool TopLevel::setGroupType(QString s)
{
  TraceItem::CostType gt;

  gt = (_data) ? _data->costType(s) : TraceData::costType(s);
  // only allow Function/Object/File/Class as grouptype
  switch(gt) {
  case TraceItem::Object:
  case TraceItem::File:
  case TraceItem::Class:
  case TraceItem::FunctionCycle:
    break;
  default:
    gt = TraceItem::Function;
  }

  return setGroupType(gt);
}

bool TopLevel::setGroupType(TraceItem::CostType gt)
{
  if (_groupType == gt) return false;
  _groupType = gt;

  int idx = -1;
  switch(gt) {
  case TraceItem::Function: idx = 0; break;
  case TraceItem::Object: idx = 1; break;
  case TraceItem::File: idx = 2; break;
  case TraceItem::Class: idx = 3; break;
  case TraceItem::FunctionCycle: idx = 4; break;
  default:
    break;
  }

  if (idx==-1) return false;

#if 0
  if (saGroup->currentItem() != idx)
    saGroup->setCurrentItem(idx);

  _stackSelection->setGroupType(_groupType);
#endif

  _partSelection->set(_groupType);
  _partSelection->updateView();

  _functionSelection->set(_groupType);
  _functionSelection->updateView();

  _multiView->set(_groupType);
  _multiView->updateView();

  updateStatusBar();

  return true;
}

bool TopLevel::setGroup(QString s)
{
    TraceCostItem* ci = _functionSelection->group(s);
    if (!ci)
	return false;

    return setGroup(ci);
}


bool TopLevel::setGroup(TraceCostItem* g)
{
  _multiView->activate(g);
  _multiView->updateView();
  _functionSelection->activate(g);
  _functionSelection->updateView();

  if (_group == g) return false;
  _group = g;


  updateStatusBar();

  return true;
}

bool TopLevel::setFunction(QString s)
{
  if (!_data) return false;

  TraceCost* f = _data->search(TraceItem::Function, s, _eventType);
  if (!f) return false;

  return setFunction((TraceFunction*)f);
}

bool TopLevel::setFunction(TraceFunction* f)
{
  if (_function == f) return false;
  _function = f;

  _multiView->activate(f);
  _multiView->updateView();

  _functionSelection->activate(f);
  _functionSelection->updateView();

  _partSelection->activate(f);
  _partSelection->updateView();

#if 0
  _stackSelection->setFunction(_function);

  StackBrowser* b = _stackSelection->browser();
  if (b) {
    // do not disable up: a press forces stack-up extending...
    _paForward->setEnabled(b->canGoForward());
    _paBack->setEnabled(b->canGoBack());
  }
#endif

#if TRACE_UPDATES
  qDebug("TopLevel::setFunction(%s), lastSender %s",
         f ? f->prettyName().ascii() : "0",
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
void TopLevel::setEventTypeDelayed(TraceEventType* ct)
{
  _eventTypeDelayed = ct;
  QTimer::singleShot (0, this, SLOT(setEventTypeDelayed()));
}

void TopLevel::setEventType2Delayed(TraceEventType* ct)
{
  _eventType2Delayed = ct;
  QTimer::singleShot (0, this, SLOT(setEventType2Delayed()));
}

void TopLevel::setEventTypeDelayed()
{
  setEventType(_eventTypeDelayed);
}

void TopLevel::setEventType2Delayed()
{
  setEventType2(_eventType2Delayed);
}

void TopLevel::setGroupTypeDelayed(TraceItem::CostType gt)
{
  _groupTypeDelayed = gt;
  QTimer::singleShot (0, this, SLOT(setGroupTypeDelayed()));
}

void TopLevel::setGroupTypeDelayed()
{
  setGroupType(_groupTypeDelayed);
}

void TopLevel::setGroupDelayed(TraceCostItem* g)
{
#if TRACE_UPDATES
  qDebug("TopLevel::setGroupDelayed(%s), sender %s",
         g ? g->prettyName().ascii() : "0",
         _lastSender ? _lastSender->name() :"0" );
#endif

  _groupDelayed = g;
  QTimer::singleShot (0, this, SLOT(setGroupDelayed()));
}

void TopLevel::setGroupDelayed()
{
  setGroup(_groupDelayed);
}

void TopLevel::setDirectionDelayed(TraceItemView::Direction d)
{
  _directionDelayed = d;
  QTimer::singleShot (0, this, SLOT(setDirectionDelayed()));
}

void TopLevel::setDirectionDelayed()
{
    switch(_directionDelayed) {
    case TraceItemView::Back:
	//_stackSelection->browserBack();
	break;

    case TraceItemView::Forward:
        //_stackSelection->browserForward();
	break;

    case TraceItemView::Up:
	{
#if 0
	    StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
	    HistoryItem* hi = b ? b->current() : 0;
	    TraceFunction* f = hi ? hi->function() : 0;

	    if (!f) break;
	    f = hi->stack()->caller(f, false);
	    if (f) setFunction(f);
#endif
	}
    break;

    default: break;
    }

    _directionDelayed = TraceItemView::None;
}


void TopLevel::setTraceItemDelayed(TraceItem* i)
{
  // no need to select same item a 2nd time...
  if (_traceItemDelayed == i) return;
  _traceItemDelayed = i;
  _lastSender = sender();

  qDebug() << "Selected " << (i ? i->prettyName() : "(none)");

#if TRACE_UPDATES
  qDebug("TopLevel::setTraceItemDelayed(%s), sender %s",
         i ? i->prettyName().ascii() : "0",
         _lastSender ? _lastSender->name() :"0" );
#endif

  QTimer::singleShot (0, this, SLOT(setTraceItemDelayed()));
}

void TopLevel::setTraceItemDelayed()
{
    if (!_traceItemDelayed) return;

    switch(_traceItemDelayed->type()) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
      setFunction((TraceFunction*)_traceItemDelayed);
      break;

    case TraceItem::Object:
    case TraceItem::File:
    case TraceItem::Class:
	setGroup((TraceCostItem*)_traceItemDelayed);
	break;

#if 0
	// this conflicts with the selection policy of InstrView ?!?
    case TraceItem::Instr:
    case TraceItem::Line:
	// only for multiview
	_multiView->activate(_traceItemDelayed);
	_multiView->updateView();
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
void TopLevel::setData(TraceData* data)
{
  if (data == _data) return;

  _lastSender = 0;

  saveTraceSettings();

  if (_data) {
    _partSelection->setData(0);
    //_stackSelection->setData(0);

    _functionSelection->setData(0);
    _functionSelection->updateView();

    _multiView->setData(0);
    _multiView->updateView();

    // we are the owner...
    delete _data;
  }

  // reset members
  init();

  _data = data;

  // fill cost type list
  QStringList types;

  if (_data) {
      /* add all supported virtual types */
      TraceEventTypeMapping* m = _data->mapping();
      m->addKnownDerivedTypes();

      /* first, fill selection list with available cost types */
      for (int i=0;i<m->realCount();i++)
	  types << m->realType(i)->longName();
      for (int i=0;i<m->derivedCount();i++)
	  types << m->derivedType(i)->longName();
  }
  _eventTypes = types;

#if 0
  _saCost->setItems(types);
  _saCost->setComboWidth(300);

  if (types.count()>0) {
    // second type list gets an additional "(Hidden)"
    types.prepend(tr("(Hidden)"));
  }
  _saCost2->setItems(types);
  _saCost2->setComboWidth(300);
  // default is hidden
  if (types.count()>0)
      _saCost2->setCurrentItem(0);

  _stackSelection->setData(_data);
#endif

  _partSelection->setData(_data);
  _partSelection->updateView();

  _functionSelection->setData(_data);
  _functionSelection->updateView();

  _multiView->setData(_data);
  _multiView->updateView();

  /* this is needed to let the other widgets know the types */
  restoreTraceTypes();

  restoreTraceSettings();

  QString caption;
  if (_data) {
    caption = _data->traceName();
    if (!_data->command().isEmpty())
      caption += " [" + _data->command() + ']';
  }
  setWindowTitle(caption);

  if (!_data || (!_forcePartDock && _data->parts().count()<2)) {
    _partDock->hide();
    _partDockShown->setChecked(false);
  }
  else {
    _partDock->show();
    _partDockShown->setChecked(true);
  }

  updateStatusBar();

  if (_data)
      setEventType(_data->mapping()->realType(0));
}

void TopLevel::addEventTypeMenu(QMenu* popup, bool withCost2)
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

    TraceEventTypeMapping* m = _data->mapping();
    TraceEventType* ct;
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

  if (_showPercentage)
    popup->addAction(tr("Show Absolute Cost"),
		      this, SLOT(setAbsoluteCost()));
  else
    popup->addAction(tr("Show Relative Cost"),
		      this, SLOT(setRelativeCost()));
}

bool TopLevel::setEventType(QAction* action)
{
  if (!_data) return false;
  int id = action->data().toInt(0);

  TraceEventTypeMapping* m = _data->mapping();
  TraceEventType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->derivedType(id-200);

  return ct ? setEventType(ct) : false;
}

bool TopLevel::setEventType2(QAction* action)
{
  if (!_data) return false;
  int id = action->data().toInt(0);

  TraceEventTypeMapping* m = _data->mapping();
  TraceEventType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->derivedType(id-200);

  return setEventType2(ct);
}

void TopLevel::addGoMenu(QMenu* popup)
{
#if 0
  StackBrowser* b = _stackSelection->browser();
  if (b) {
      if (b->canGoBack())
	  popup->addAction(tr("Go Back"), this, SLOT(goBack()));
      if (b->canGoForward())
	  popup->addAction(tr("Go Forward"), this, SLOT(goForward()));
  }
#else
  popup->addAction(tr("Go Back"), this, SLOT(goBack()));
  popup->addAction(tr("Go Forward"), this, SLOT(goForward()));
#endif
  // do not disable up: a press forces stack-up extending...
  popup->addAction(tr("Go Up"), this, SLOT(goUp()));
}

void TopLevel::goBack()
{
  setDirectionDelayed(TraceItemView::Back);
}

void TopLevel::goForward()
{
  setDirectionDelayed(TraceItemView::Forward);
}

void TopLevel::goUp()
{
  setDirectionDelayed(TraceItemView::Up);
}

QString TopLevel::traceKey()
{
  if (!_data || _data->command().isEmpty()) return QString();

  QString name = _data->command();
  QString key;
  for (int l=0;l<name.length();l++)
    if (name[l].isLetterOrNumber()) key += name[l];

  return QString("-") + key;
}


void TopLevel::restoreTraceTypes()
{
  QString key = traceKey();
  QString groupType, costType, costType2;

  ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
  groupType =  pConfig->value(QString("GroupType%1").arg(key),QString()).toString();
  costType  =  pConfig->value(QString("CostType%1").arg(key),QString()).toString();
  costType2 =  pConfig->value(QString("CostType2%1").arg(key),QString()).toString();
  delete pConfig;

  ConfigGroup* cConfig = ConfigStorage::group("CurrentState");
  if (groupType.isEmpty()) groupType = cConfig->value("GroupType",QString()).toString();
  if (costType.isEmpty()) costType = cConfig->value("CostType",QString()).toString();
  if (costType2.isEmpty()) costType2 = cConfig->value("CostType2",QString()).toString();
  delete cConfig;

  setGroupType(groupType);
  setEventType(costType);
  setEventType2(costType2);

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
void TopLevel::restoreTraceSettings()
{
  if (!_data) return;

  QString key = traceKey();

  ConfigGroup* pConfig = ConfigStorage::group("TracePositions");
  QString group = pConfig->value(QString("Group%1").arg(key),QString()).toString();
  if (!group.isEmpty()) setGroup(group);

  restoreCurrentState(key);

  // restoreCurrentState() usually leads to a call to setTraceItemDelayed()
  // to restore last active item...
  if (!_traceItemDelayed) {
    // function not available any more.. try with "main"
      if (!setFunction("main")) {
#if 1
	_functionSelection->setTopFunction();
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

void TopLevel::layoutDuplicate()
{
  // save current and allocate a new slot
//   _multiView->saveViewConfig(KGlobal::config().data(),
//			     QString("Layout%1-MainView").arg(_layoutCurrent),
//			     traceKey(), false);
  _layoutCurrent = _layoutCount;
  _layoutCount++;

  updateLayoutActions();

//  kDebug() << "TopLevel::layoutDuplicate: count " << _layoutCount;
}

void TopLevel::layoutRemove()
{
  if (_layoutCount <2) return;

  int from = _layoutCount-1;
  if (_layoutCurrent == from) { _layoutCurrent--; from--; }
  // restore from last and decrement count
//  _multiView->readViewConfig(KGlobal::config().data(),
//			     QString("Layout%1-MainView").arg(from),
//			     traceKey(), false);
  _layoutCount--;

  updateLayoutActions();
}

void TopLevel::layoutNext()
{
  if (_layoutCount <2) return;

  //KConfig *config = KGlobal::config().data();
  QString key = traceKey();

//  _multiView->saveViewConfig(config,
//			     QString("Layout%1-MainView").arg(_layoutCurrent),
//			     key, false);
  _layoutCurrent++;
  if (_layoutCurrent == _layoutCount) _layoutCurrent = 0;

//  _multiView->readViewConfig(config,
//			     QString("Layout%1-MainView").arg(_layoutCurrent),
//			     key, false);

  if (0) qDebug() << "TopLevel::layoutNext: current "
		   << _layoutCurrent << endl;
}

void TopLevel::layoutPrevious()
{
  if (_layoutCount <2) return;

//  KConfig *config = KGlobal::config().data();
  QString key = traceKey();

//  _multiView->saveViewConfig(config,
//			     QString("Layout%1-MainView").arg(_layoutCurrent),
//			     key, false);
  _layoutCurrent--;
  if (_layoutCurrent <0) _layoutCurrent = _layoutCount-1;

//  _multiView->readViewConfig(config,
//			     QString("Layout%1-MainView").arg(_layoutCurrent),
//			     key, false);

  if (0) qDebug() << "TopLevel::layoutPrevious: current "
		  << _layoutCurrent;
}

void TopLevel::layoutSave()
{
#if 0
  KConfig *config = KGlobal::config().data();
  QString key = traceKey();

  _multiView->saveViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);

  for(int i=0;i<_layoutCount;i++) {
    _multiView->readViewConfig(config,
			       QString("Layout%1-MainView").arg(i),
			       key, false);
    _multiView->saveViewConfig(config,
			       QString("Layout%1-MainView").arg(i),
			       QString(), false);
  }

  _multiView->readViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);

  KConfigGroup aConfig(config, "Layouts");
  aConfig.writeEntry("DefaultCount", _layoutCount);
  aConfig.writeEntry("DefaultCurrent", _layoutCurrent);
#endif
}

void TopLevel::layoutRestore()
{
#if 0
  KConfig *config = KGlobal::config().data();
  KConfigGroup aConfig(config, "Layouts");
  _layoutCount = aConfig.readEntry("DefaultCount", 0);
  _layoutCurrent = aConfig.readEntry("DefaultCurrent", 0);
  if (_layoutCount == 0) {
    _layoutCount++;
    return;
  }

  QString key = traceKey();
  for(int i=0;i<_layoutCount;i++) {
    _multiView->readViewConfig(config,
			       QString("Layout%1-MainView").arg(i),
			       QString(), false);
    _multiView->saveViewConfig(config,
			       QString("Layout%1-MainView").arg(i),
			       key, false);
  }

  _multiView->readViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);

  updateLayoutActions();
#endif
}


void TopLevel::updateLayoutActions()
{
#if 0
  QAction* ka;

  ka = actionCollection()->action("layout_next");
  if (ka) ka->setEnabled(_layoutCount>1);

  ka = actionCollection()->action("layout_previous");
  if (ka) ka->setEnabled(_layoutCount>1);

  ka = actionCollection()->action("layout_remove");
  if (ka) ka->setEnabled(_layoutCount>1);

  _statusbar->message(tr("Layout Count: %1", _layoutCount), 1000);
#endif
}


void TopLevel::updateStatusBar()
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

  if (_groupType != TraceItem::Function) {
    status += QString(" - %1 '%2'")
              .arg(TraceItem::trTypeName(_groupType))
              .arg(_group ? _group->prettyName() : tr("(None)"));
  }
  */

  _statusLabel->setText(status);
}

void TopLevel::configure()
{
#if 0
    if (ConfigDlg::configure( (KConfiguration*) GlobalConfig::config(),
			      _data, this)) {
      GlobalConfig::config()->saveOptions();

    configChanged();
  }
  else
      GlobalConfig::config()->readOptions();
#endif
}

bool TopLevel::queryClose()
{
    //saveTraceSettings();

  return true;
}

bool TopLevel::queryExit()
{
    // save current toplevel options as defaults...
    GlobalConfig::setShowPercentage(_showPercentage);
    GlobalConfig::setShowExpanded(_showExpanded);
    GlobalConfig::setShowCycles(_showCycles);
    GlobalConfig::config()->saveOptions();

    saveCurrentState(QString::null);	//krazy:exclude=nullstrassign for old broken gcc

  // toolbar and dock positions are automatically stored

  // if part dock was chosen visible even for only 1 part loaded,
  // keep this choice...
  _forcePartDock = false;
  if (_data && (_data->parts().count()<2) && _partDock->isVisible())
    _forcePartDock=true;

  ConfigGroup* dockConfig = ConfigStorage::group("Docks");
  dockConfig->setValue("ForcePartDockVisible", _forcePartDock, false);
  delete dockConfig;

  return true;
}


void TopLevel::splitSlot()
{
    int count = _multiView->childCount();
    if (count<1) count = 1;
    if (count>2) count = 2;
    count = 3-count;
    _multiView->setChildCount(count);

    _taSplit->setChecked(count>1);
    _taSplitDir->setEnabled(count>1);
    _taSplitDir->setChecked(_multiView->orientation() == Qt::Horizontal);
}

void TopLevel::splitDirSlot()
{
  _multiView->setOrientation( _taSplitDir->isChecked() ?
                              Qt::Horizontal : Qt::Vertical );
}



// this is called after a config change in the dialog
void TopLevel::configChanged()
{
  //qDebug("TopLevel::configChanged");
  //_showPercentage->setChecked(GlobalConfig::showPercentage());

  // invalidate found/cached dirs of source files
  _data->resetSourceDirs();

  _partSelection->notifyChange(TraceItemView::configChanged);
  _partSelection->updateView();

  //_stackSelection->refresh();

  _functionSelection->notifyChange(TraceItemView::configChanged);
  _functionSelection->updateView();

  _multiView->notifyChange(TraceItemView::configChanged);
  _multiView->updateView();
}

void TopLevel::slotShowTipOnStart() {
    //KTipDialog::showTip(this);
}

void TopLevel::slotShowTip() {
    //KTipDialog::showTip( this, QString(), true );
}

void TopLevel::dummySlot()
{
}

void TopLevel::activePartsChangedSlot(const TracePartList& list)
{
  if (!_data) return;

  if (!_data->activateParts(list)) {
//    qDebug("TopLevel::activePartsChangedSlot: No Change!");
    return;
  }
  _activeParts = list;

  _partSelection->set(list);
  _partSelection->updateView();

  //_stackSelection->refresh();

  _functionSelection->set(list);
  _functionSelection->updateView();

  _multiView->set(list);
  _multiView->updateView();

  updateStatusBar();
}

void TopLevel::partsHideSelectedSlotDelayed()
{
  QTimer::singleShot( 0, this, SLOT(partsHideSelectedSlot()) );
}

// this puts selected parts into hidden list,
// deselects them and makes the remaining parts selected
void TopLevel::partsHideSelectedSlot()
{
  if (!_data) return;

  TracePart* part;
  TracePartList newHidden, newActive;
  TracePartList l = _data->parts();
  for (part=l.first();part;part=l.next()) {
    if ((_activeParts.findRef(part)>=0) ||
        (_hiddenParts.findRef(part)>=0))
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

void TopLevel::partsUnhideAllSlotDelayed()
{
  QTimer::singleShot( 0, this, SLOT(partsUnhideAllSlot()) );
}

// this unhides all hidden parts. Does NOT change selection
void TopLevel::partsUnhideAllSlot()
{
  if (!_data) return;

  _hiddenParts.clear();
  _partSelection->hiddenPartsChangedSlot(_hiddenParts);

#if 0
  _mainWidget1->hiddenPartsChangedSlot(_hiddenParts);
  _mainWidget2->hiddenPartsChangedSlot(_hiddenParts);
#endif
}

void TopLevel::forceTrace()
{
//  qDebug("forceTrace");

  // Needs Callgrind now...
  QFile cmd("callgrind.cmd");
  if (!cmd.exists()) {
    cmd.open(QIODevice::WriteOnly);
    cmd.write("DUMP\n", 5);
    cmd.close();
  }
  if (_taDump->isChecked())
    QTimer::singleShot( 1000, this, SLOT(forceTraceReload()) );
  else {
    // cancel request
    cmd.remove();
  }

}

void TopLevel::forceTraceReload()
{
//  qDebug("forceTraceReload");

  QFile cmd("callgrind.cmd");
  if (cmd.exists()) {
    if (_taDump->isChecked())
      QTimer::singleShot( 1000, this, SLOT(forceTraceReload()) );
    return;
  }
  _taDump->setChecked(false);
  reload();
}

void TopLevel::forwardAboutToShow()
{
#if 0
  QMenu *popup = _paForward->menu();

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

    QString name = f->prettyName();
    if ((int)name.length()>GlobalConfig::maxSymbolLength())
      name = name.left(GlobalConfig::maxSymbolLength()) + "...";

    //qDebug("forward: Adding %s", name.ascii());
    action = popup->addAction(name);
    action->setData(count);

    hi = hi->next();
    count++;
  }
#endif
}

void TopLevel::backAboutToShow()
{
#if 0
  QMenu *popup = _paBack->menu();

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

    QString name = f->prettyName();
    if ((int)name.length()>GlobalConfig::maxSymbolLength())
      name = name.left(GlobalConfig::maxSymbolLength()) + "...";

    //qDebug("back: Adding %s", name.ascii());
    action = popup->addAction(name);
    action->setData(count);

    hi = hi->last();
    count++;
  }
#endif
}

void TopLevel::upAboutToShow()
{
#if 0
  QMenu *popup = _paUp->menu();

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
    QString name = f->prettyName();
    if ((int)name.length()>GlobalConfig::maxSymbolLength())
      name = name.left(GlobalConfig::maxSymbolLength()) + "...";

    action = popup->addAction(name);
    action->setData(count);

    f = hi->stack()->caller(f, false);
    count++;
  }
#endif
}

void TopLevel::forwardTriggered(QAction* action)
{
#if 0
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
#endif
}

void TopLevel::backTriggered(QAction* action)
{
#if 0
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
#endif
}

void TopLevel::upTriggered(QAction* action)
{
#if 0
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

  //qDebug("upActivated: %s", f ? f->prettyName().ascii() : "??" );
  if (f)
    setFunction(f);
#endif
}

void TopLevel::showMessage(const QString& msg, int ms)
{
	if (_statusbar)
		_statusbar->message(msg, ms);
}

void TopLevel::showStatus(const QString& msg, int progress)
{
	static bool msgUpdateNeeded = true;

	if (msg.isEmpty()) {
	        //reset status
		if (_progressBar) {
			_statusbar->removeWidget(_progressBar);
			delete _progressBar;
			_progressBar = 0;
		}
		_statusbar->clear();
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
		_statusbar->addWidget(_progressBar, 1, true);
		_progressBar->show();
		msgUpdateNeeded = true;
	}

	_progressStart.restart();

	if (msgUpdateNeeded) {
		_statusbar->message(msg);
		msgUpdateNeeded = false;
	}
	_progressBar->setValue(progress);

	// let the progress bar update itself
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void TopLevel::loadStart(const QString& filename)
{
    showStatus(QString("Loading %1").arg(filename), 0);
    Logger::_filename = filename;
}

void TopLevel::loadFinished(const QString& msg)
{
    showStatus(QString::null, 0);
    if (!msg.isEmpty())
	showMessage(QString("Error loading %1: %2").arg(_filename).arg(msg),
		    2000);
}

void TopLevel::loadProgress(int progress)
{
    showStatus(QString("Loading %1").arg(_filename), progress);
}

void TopLevel::loadError(int line, const QString& msg)
{
    qCritical() << "Loading" << _filename.ascii() << ":" << line << ": " << msg.ascii();
}

void TopLevel::loadWarning(int line, const QString& msg)
{
    qWarning() << "Loading" << _filename.ascii() << ":" << line << ": " << msg.ascii();
}

#include "toplevel.moc"
