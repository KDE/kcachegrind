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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * KCachegrind top level window
 */

#define TRACE_UPDATES 0
#define ENABLE_DUMPDOCK 0

#include <stdlib.h> // for system()

#include <qvbox.h>
#include <qtimer.h>
#include <qwhatsthis.h>
#include <qlineedit.h>
#include <qtextstream.h>
#include <qsizepolicy.h>
#include <qprogressbar.h>
#include <qfile.h>

// With Qt 3.1, we can disallow user interaction with long tasks.
// This needs QEventLoop. Otherwise, QApplication::processEvents is used.
#if (QT_VERSION-0 >= 0x030100)
#include <qeventloop.h>
#endif

#include <kapplication.h>
#include <klocale.h>
#include <kstatusbar.h>
#include <kstdaccel.h>
#include <kstdaction.h>
#include <kaction.h>
#include <kurl.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>
#include <kedittoolbar.h>
#include <kkeydialog.h>
#include <ktip.h>
#include <kpopupmenu.h>
#include <kdebug.h>

#if ENABLE_DUMPDOCK
#include "dumpselection.h"
#endif

#include "toplevel.h"
#include "partselection.h"
#include "functionselection.h"
#include "stackselection.h"
#include "stackbrowser.h"
#include "tracedata.h"
#include "configuration.h"
#include "configdlg.h"
#include "multiview.h"
#include "callgraphview.h"


TopLevel::TopLevel(const char *name)
  : KMainWindow(0, name), DCOPObject("KCachegrindIface")
{
  init();

  createDocks();

  _multiView = new MultiView(this, this, "MultiView");
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

  KConfig* kconfig = KGlobal::config();
  Configuration::readOptions( kconfig );
  _openRecent->loadEntries( kconfig );

  // set toggle after reading configuration
  _showPercentage = Configuration::showPercentage();
  _showExpanded   = Configuration::showExpanded();
  _showCycles     = Configuration::showCycles();
  _taPercentage->setChecked(_showPercentage);
  _taExpanded->setChecked(_showExpanded);
  _taCycles->setChecked(_showCycles);

  setupPartSelection(_partSelection);

  // KCachegrind for KDE 3.0.x does not allow to hide toolbars...
#if KDE_VERSION >= 308 // KDE 3.1
  setStandardToolBarMenuEnabled(true);
#endif

  // QT dock windows are created before (using QT position restoring)
  createGUI();

  setAutoSaveSettings();

  // restore current state settings (not configuration options)
  restoreCurrentState(QString::null);

  // if this is the first toplevel, show tip of day
  if (memberList->count() == 1)
    QTimer::singleShot( 200, this, SLOT(slotShowTipOnStart()) );
}

void TopLevel::init()
{
  _activeParts.clear();
  _hiddenParts.clear();

  _progressBar = 0;

  _data = 0;
  _function = 0;
  _costType = 0;
  _costType2 = 0;
  _groupType = TraceCost::NoCostType;
  _group = 0;

  _layoutCurrent = 0;
  _layoutCount = 1;

  // for delayed slots
  _traceItemDelayed = 0;
  _costTypeDelayed = 0;
  _costType2Delayed = 0;
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

  connect(ps, SIGNAL(activePartsChanged(const TracePartList&)),
          this, SLOT(activePartsChangedSlot(const TracePartList&)));
  connect(ps, SIGNAL(groupChanged(TraceCostItem*)),
          this, SLOT(setGroupDelayed(TraceCostItem*)));
  connect(ps, SIGNAL(functionChanged(TraceItem*)),
          this, SLOT(setTraceItemDelayed(TraceItem*)));

  connect(ps, SIGNAL(goBack()),
          _stackSelection, SLOT(browserBack()));

  connect(ps, SIGNAL(partsHideSelected()),
          this, SLOT(partsHideSelectedSlotDelayed()));
  connect(ps, SIGNAL(partsUnhideAll()),
          this, SLOT(partsUnhideAllSlotDelayed()));

  connect(ps, SIGNAL(showMessage(const QString&, int)),
          _statusbar, SLOT(message(const QString&, int)));
}

/**
 * This saves the current state of the main window and
 * sub widgets.
 *
 * No positions are saved. These is done automatically for
 * KToolbar, and manually in queryExit() for QT docks.
 */
void TopLevel::saveCurrentState(QString postfix)
{
  KConfig* kconfig = KGlobal::config();
  QCString pf = postfix.ascii();

  KConfigGroup psConfig(kconfig, QCString("PartOverview")+pf);
  _partSelection->saveVisualisationConfig(&psConfig);

  KConfigGroup stateConfig(kconfig, QCString("CurrentState")+pf);
  stateConfig.writeEntry("CostType",
			 _costType ? _costType->name() : QString("?"));
  stateConfig.writeEntry("CostType2",
			 _costType2 ? _costType2->name() : QString("?"));
  stateConfig.writeEntry("GroupType", TraceItem::typeName(_groupType));

  _multiView->saveViewConfig(kconfig, QString("MainView"), postfix, true);
}

/**
 * This function is called when a trace is closed.
 * Save browsing position for later restoring
 */
void TopLevel::saveTraceSettings()
{
  QString key = traceKey();

  KConfigGroup pConfig(KGlobal::config(), QCString("TracePositions"));
  pConfig.writeEntry(QString("CostType%1").arg(key),
                     _costType ? _costType->name() : QString("?"));
  pConfig.writeEntry(QString("CostType2%1").arg(key),
                     _costType2 ? _costType2->name() : QString("?"));
  pConfig.writeEntry(QString("GroupType%1").arg(key),
                     TraceItem::typeName(_groupType));

  if (!_data) return;

  KConfigGroup aConfig(KGlobal::config(), QCString("Layouts"));
  aConfig.writeEntry(QString("Count%1").arg(key), _layoutCount);
  aConfig.writeEntry(QString("Current%1").arg(key), _layoutCurrent);

  saveCurrentState(key);
  pConfig.writeEntry(QString("Group%1").arg(key),
                     _group ? _group->name() : QString::null);
}

/**
 * This restores the current state of the main window and
 * sub widgets.
 *
 * This does NOT restore any positions. This is done automatically for
 * KToolbar, and manually in the createDocks() for QT docks..
 */
void TopLevel::restoreCurrentState(QString postfix)
{
  KConfig* kconfig = KGlobal::config();
  QStringList gList = kconfig->groupList();
  QCString pf = postfix.ascii();

  // dock properties (not position, this should be have done before)
  QCString group = QCString("PartOverview");
  if (gList.contains(group+pf)) group += pf;
  KConfigGroup psConfig(kconfig, group);
  _partSelection->readVisualisationConfig(&psConfig);

  _multiView->readViewConfig(kconfig, QString("MainView"), postfix, true);
  _taSplit->setChecked(_multiView->childCount()>1);
  _taSplitDir->setEnabled(_multiView->childCount()>1);
  _taSplitDir->setChecked(_multiView->orientation() == Qt::Horizontal);
}


void TopLevel::createDocks()
{
  _partDock = new QDockWindow(QDockWindow::InDock, this);
  _partDock->setCaption(i18n("Parts Overview"));
  _partDock->setCloseMode( QDockWindow::Always );
  _partSelection = new PartSelection(_partDock, "partSelection");
  _partDock->setWidget(_partSelection);
  _partDock->setResizeEnabled(true);
  _partDock->setFixedExtentWidth(200);
  QWhatsThis::add( _partSelection, i18n(
                   "<b>The Parts Overview</b>"
                   "<p>A trace consists of multiple trace parts when "
                   "there are several profile data files from one profile run. "
                   "The Trace Part Overview dockable shows these, "
                   "horizontally ordered in execution time; "
                   "the rectangle sizes are proportional to the total "
                   "cost spent in the parts. You can select one or several "
                   "parts to constrain all costs shown to these parts only."
                   "</p>"
                   "<p>The parts are further subdivided: there is a "
                   "partitioning and an callee split mode: "
                   "<ul><li>Partitioning: You see the "
                   "partitioning into groups for a trace part, according to "
                   "the group type selected. E.g. if ELF object groups are "
                   "selected, you see colored rectangles for each "
                   "used ELF object (shared library or executable), sized "
                   "according to the cost spent therein.</li>"
                   "<li>Callee: A rectangle showing the inclusive "
                   "cost of the current selected function in the trace part "
                   "is shown. "
                   "This is split up into smaller rectangles to show the costs of its "
                   "callees.</li></ul></p>"));

  _stackDock = new QDockWindow(QDockWindow::InDock, this);
  _stackDock->setResizeEnabled(true);
  // Why is the caption only correct with a close button?
  _stackDock->setCloseMode( QDockWindow::Always );
  _stackSelection = new StackSelection(_stackDock, "stackSelection");
  _stackDock->setWidget(_stackSelection);
  _stackDock->setFixedExtentWidth(200);
  _stackDock->setCaption(i18n("Top Cost Call Stack"));
  QWhatsThis::add( _stackSelection, i18n(
                   "<b>The Top Cost Call Stack</b>"
                   "<p>This is a purely fictional 'most probable' call stack. "
                   "It is built up by starting with the current selected "
                   "function and adds the callers/callees with highest cost "
                   "at the top and to bottom.</p>"
                   "<p>The <b>Cost</b> and <b>Calls</b> columns show the "
                   "cost used for all calls from the function in the line "
                   "above.</p>"));

  connect(_stackSelection, SIGNAL(functionSelected(TraceItem*)),
          this, SLOT(setTraceItemDelayed(TraceItem*)));

  _functionDock = new QDockWindow(QDockWindow::InDock, this);
  _functionDock->setCaption(i18n("Flat Profile"));
  _functionDock->setCloseMode( QDockWindow::Always );
  _functionSelection = new FunctionSelection(this, _functionDock,
                                             "functionSelection");
  _functionSelection->setTopLevel(this);

  _functionDock->setWidget(_functionSelection);
  _functionDock->setResizeEnabled(true);
  _functionDock->setFixedExtentWidth(200);
  QWhatsThis::add( _functionSelection, i18n(
                   "<b>The Flat Profile</b>"
                   "<p>The flat profile contains a group and a function "
                   "selection list. The group list contains all groups "
                   "where costs "
                   "are spent in, depending on the chosen group type. "
                   "The group list is hidden when group type 'Function' "
                   "is selected.<p>"
                   "<p>The function list contains the functions of the "
                   "selected group (or all for 'Function' group type), "
                   "ordered by the costs spent therein. Functions with "
                   "costs less than 1% are hidden on default.</p>"));

#if ENABLE_DUMPDOCK
  _dumpDock = new QDockWindow(QDockWindow::InDock, this);
  _dumpDock->setCaption(i18n("Profile Dumps"));
  _dumpDock->setCloseMode( QDockWindow::Always );
  _dumpSelection = new DumpSelection(this, _dumpDock,
                                     "dumpSelection");
  _dumpSelection->setTopLevel(this);

  _dumpDock->setWidget(_dumpSelection);
  _dumpDock->setResizeEnabled(true);
  _dumpDock->setFixedExtentWidth(200);
  QWhatsThis::add( _dumpSelection, i18n(
                   "<b>Profile Dumps</b>"
                   "<p>This dockable shows in the top part the list of "
                   "loadable profile dumps in all subdirectories of: "
                   "<ul><li>current working directory of KCachegrind, "
                   "i.e. where it was started from, and "
                   "<li>the default profile dump directory given in the "
                   "configuration.</ul> "
                   "The list is sorted according the the target command "
                   "profiled in the corresponding dump.</p>"
                   "<p>On selecting a profile dump, information for it "
                   "is shown in the bottom area of the dockable: "
                   "<ul><li><b>Options</b> allows you to view the profiled "
                   "command and profile options of this dump. By changing "
                   "any item, a new (yet unexisting) profile template "
                   "is created. Press <b>Run Profile</b> to start a"
                   "profile run with these options in the background. "
                   "<li><b>Info</b> gives detailed info on the selected "
                   "dump like event cost summary and properties of the "
                   "simulated cache. "
                   "<li><b>State</b> is only available for current happening "
                   "profiles runs. Press <b>Update</b> to see different "
                   "counters of the run, and a stack trace of the current "
                   "position in the program profiled. Check the <b>Every</b> "
                   "option to let KCachegrind regularly poll these data. "
                   "Check the <b>Sync</b> option to let the dockable activate "
                   "the top function in the current loaded dump.</ul></p>"));
#endif

  // Restore QT Dock positions...
  KConfigGroup dockConfig(KGlobal::config(), QCString("Docks"));
  QString str = dockConfig.readEntry("Position", QString::null);
  if (0) qDebug("Docks/Position: '%s'", str.ascii());
  if (str.isEmpty()) {
    // default positions
    addDockWindow(_partDock, DockLeft);
    addDockWindow(_stackDock, DockLeft);
    addDockWindow(_functionDock, DockLeft);
    _stackDock->hide();
#if ENABLE_DUMPDOCK
    addDockWindow(_dumpDock, DockLeft);
    _dumpDock->hide();
#endif
  }
  else {
    QTextStream ts( &str, IO_ReadOnly );
    ts >> *this;
  }
  _forcePartDock = dockConfig.readBoolEntry("ForcePartDockVisible", false);

#if 0
  // dock context menu
  setAppropriate(_partDock, true);
  setAppropriate(_stackDock, true);
  setAppropriate(_dumpDock, true);
  setAppropriate(_functionDock, true);

  connect( _partDock, SIGNAL(contextMenuRequested(const QPoint &)),
           this, SLOT(showDockMenu(const QPoint &)));
#endif
}


TopLevel::~TopLevel()
{
  delete _data;
}


void TopLevel::saveProperties(KConfig* c)
{
  c->writeEntry("TraceName", _data->traceName());
}

void TopLevel::readProperties(KConfig* c)
{
  QString traceName = c->readEntry("TraceName");
  if (!traceName.isEmpty()) {
      TraceData* d = new TraceData(this);
      d->load(traceName);
      setData(d);
  }
}

void TopLevel::createLayoutActions()
{
  QString hint;
  KAction* action;

  action = new KAction( i18n( "&Duplicate" ),
			KShortcut(KKey("Ctrl+Plus")),
                        this, SLOT(layoutDuplicate()),
                        actionCollection(), "layout_duplicate" );
  hint = i18n("<b>Duplicate Current Layout</b>"
              "<p>Make a copy of the current layout.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Remove" ), KShortcut(),
                        this, SLOT(layoutRemove()),
                        actionCollection(), "layout_remove" );
  hint = i18n("<b>Remove Current Layout</b>"
              "<p>Delete current layout and make the previous active.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Go to Next" ),
			KShortcut(KKey("Ctrl+Right")),
                        this, SLOT(layoutNext()),
                        actionCollection(), "layout_next" );
  hint = i18n("Go to Next Layout");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Go to Previous" ),
			KShortcut(KKey("Ctrl+Left")),
                        this, SLOT(layoutPrevious()),
                        actionCollection(), "layout_previous" );
  hint = i18n("Go to Previous Layout");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Restore to Default" ), KShortcut(),
                        this, SLOT(layoutRestore()),
                        actionCollection(), "layout_restore" );
  hint = i18n("Restore Layouts to Default");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Save as Default" ), KShortcut(),
                        this, SLOT(layoutSave()),
                        actionCollection(), "layout_save" );
  hint = i18n("Save Layouts as Default");
  action->setWhatsThis( hint );
}

// TODO: split this up...
void TopLevel::createMiscActions()
{
  QString hint;
  KAction* action;

  action = KStdAction::openNew(this, SLOT(newWindow()), actionCollection());
  hint = i18n("<b>New</b><p>Open new empty KCachegrind window.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Add..." ), KShortcut(),
                        this, SLOT(addTrace()),
                        actionCollection(), "file_add" );
  hint = i18n("<b>Add Profile Data</b>"
              "<p>This opens an additional profile data file in the current window.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Reload" ), "reload",
#if KDE_VERSION > 0x030190
  // for KDE 3.2: KStdAccel::key is deprecated
			KStdAccel::shortcut(KStdAccel::Reload),
#else
                        KStdAccel::key(KStdAccel::Reload),
#endif
                        this, SLOT( reload() ), actionCollection(), "reload" );
  hint = i18n("<b>Reload Profile Data</b>"
              "<p>This loads any new created parts, too.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Export Graph" ), KShortcut(),
                        this, SLOT(exportGraph()),
                        actionCollection(), "export" );

  hint = i18n("<b>Export Call Graph</b>"
              "<p>Generates a file with extension .dot for the tools "
              "of the GraphViz package.</p>");
  action->setWhatsThis( hint );


  _taDump = new KToggleAction( i18n( "&Force Dump" ), "redo",
#if KDE_VERSION > 0x030190
  // for KDE 3.2: KStdAccel::key is deprecated
			       KStdAccel::shortcut(KStdAccel::Redo),
#else
                               KStdAccel::key(KStdAccel::Redo),
#endif
                               this, SLOT( forceTrace() ),
                               actionCollection(), "dump" );
  hint = i18n("<b>Force Dump</b>"
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
              "and it does a Reload. If there's <em>no</em> Callgrind "
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

  action = KStdAction::open(this, SLOT(loadTrace()), actionCollection());
  hint = i18n("<b>Open Profile Data</b>"
              "<p>This opens a profile data file, with possible multiple parts</p>");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  _openRecent = KStdAction::openRecent(this, SLOT(loadTrace(const KURL&)),
                                       actionCollection());

  KStdAction::showStatusbar(this,
                            SLOT(toggleStatusBar()), actionCollection());

  _partDockShown = new KToggleAction(i18n("Parts Overview"), KShortcut(),
                                     this, SLOT(togglePartDock()),
                                     actionCollection(),
                                     "settings_show_partdock");

  hint = i18n("Show/Hide the Parts Overview Dockable");
  _partDockShown->setToolTip( hint );
  _partDockShown->setWhatsThis( hint );

  _stackDockShown = new KToggleAction(i18n("Call Stack"), KShortcut(),
                                     this, SLOT(toggleStackDock()),
                                     actionCollection(),
                                     "settings_show_stackdock");

  hint = i18n("Show/Hide the Call Stack Dockable");
  _stackDockShown->setToolTip( hint );
  _stackDockShown->setWhatsThis( hint );

  _functionDockShown = new KToggleAction(i18n("Function Profile"), KShortcut(),
                                         this, SLOT(toggleFunctionDock()),
                                         actionCollection(),
                                         "settings_show_profiledock");

  hint = i18n("Show/Hide the Function Profile Dockable");
  _functionDockShown->setToolTip( hint );
  _functionDockShown->setWhatsThis( hint );

#if ENABLE_DUMPDOCK
  _dumpDockShown = new KToggleAction(i18n("Profile Dumps"), KShortcut(),
                                     this, SLOT(toggleDumpDock()),
                                     actionCollection(),
                                     "settings_show_dumpdock");

  hint = i18n("Show/Hide the Profile Dumps Dockable");
  _dumpDockShown->setToolTip( hint );
  _dumpDockShown->setWhatsThis( hint );
#endif

  _taPercentage = new KToggleAction(i18n("Show Relative Costs"), "percent",
				    KShortcut(),
				    this, SLOT(togglePercentage()),
				    actionCollection(),
				    "view_percentage");
#if KDE_VERSION >= 0x030290
  // for KDE 3.3: show another text instead of a checkmark
  _taPercentage->setCheckedState(i18n("Show Absolute Costs"));
#endif

  hint = i18n("Show relative instead of absolute costs");
  _taPercentage->setToolTip( hint );
  _taPercentage->setWhatsThis( hint );

  _taExpanded = new KToggleAction(i18n("Percentage Relative to Parent"), "move",
				  KShortcut(),
				  this, SLOT(toggleExpanded()),
				  actionCollection(),
				  "view_expanded");

  hint = i18n("Show percentage costs relative to parent");
  _taExpanded->setToolTip( hint );
  _taExpanded->setWhatsThis( hint );

  hint = i18n("<b>Show percentage costs relative to parent</b>"
              "<p>If this is switched off, percentage costs are always shown "
	      "relative to the total cost of the profile part(s) that are "
	      "currently browsed. By turning on this option, percentage cost "
	      "of shown cost items will be relative to the parent cost item."
	      "<ul><table>"
	      "<tr><td><b>Cost Type</td><td><b>Parent Cost</td></tr>"
	      "<tr><td>Function Cumulative</td><td>Total</td></tr>"
	      "<tr><td>Function Self</td><td>Function Group (*) / Total</td></tr>"
	      "<tr><td>Call</td><td>Function Cumulative</td></tr>"
	      "<tr><td>Source Line</td><td>Function Cumulative</td></tr>"
	      "</table>"
	      "<p>(*) Only if function grouping is switched on (e.g. ELF object grouping).");
  _taExpanded->setWhatsThis( hint );

  _taCycles = new KToggleAction( i18n( "Do Cycle Detection" ), "undo",
				 KShortcut(),
				 this, SLOT( toggleCycles() ), actionCollection(),
				 "view_cycles" );
#if KDE_VERSION >= 0x030290
  // for KDE 3.3: show another text instead of a checkmark
  _taCycles->setCheckedState(i18n("Skip Cycle Detection"));
#endif

  hint = i18n("<b>Detect recursive cycles</b>"
              "<p>If this is switched off, the treemap drawing will show "
	      "black areas when a recursive call is made instead of drawing the "
	      "recursion ad infinitum. Note that "
	      "the size of black areas often will be wrong, as inside recursive "
              "cycles the cost of calls cannot be determined; the error is small, "
              "however, for false cycles (see documentation)."
	      "<p>The correct handling for cycles is to detect them and collapse all "
              "functions of a cycle into a virtual function, which is done when this "
              "option is selected. Unfortunately, with GUI applications, this often will "
              "lead to huge false cycles, making the analysis impossible; therefore, there "
              "is the option to switch this off.");
  _taCycles->setWhatsThis( hint );

  KStdAction::quit(this, SLOT(close()), actionCollection());
  KStdAction::preferences(this, SLOT(configure()), actionCollection());
  KStdAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
  KStdAction::configureToolbars(this,SLOT(configureToolbars()),
                                actionCollection());
#if 0
  action = KStdAction::back(_stackSelection, SLOT(browserBack()),
                            actionCollection());
  hint = i18n("Go back in function selection history");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  action = KStdAction::forward(_stackSelection, SLOT(browserForward()),
                      actionCollection());
  hint = i18n("Go forward in function selection history");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  action = KStdAction::up(_stackSelection, SLOT(browserUp()),
                      actionCollection());
  hint = i18n("<b>Go Up</b>"
              "<p>Go to last selected caller of current function. "
              "If no caller was visited, use that with highest cost.</p>");
  action->setToolTip( hint );
  action->setWhatsThis( hint );
#else
  _paUp = new KToolBarPopupAction( i18n( "&Up" ), "up",
                                   ALT+Key_Up,
                                   _stackSelection, SLOT( browserUp() ),
                                   actionCollection(), "go_up" );
  connect( _paUp->popupMenu(), SIGNAL( aboutToShow() ),
           this, SLOT( upAboutToShow() ) );
  connect( _paUp->popupMenu(), SIGNAL( activated( int ) ),
           this, SLOT( upActivated( int ) ) );
  hint = i18n("<b>Go Up</b>"
              "<p>Go to last selected caller of current function. "
              "If no caller was visited, use that with highest cost.</p>");
  _paUp->setToolTip( hint );
  _paUp->setWhatsThis( hint );

  QPair< KGuiItem, KGuiItem > backForward = KStdGuiItem::backAndForward();
  _paBack = new KToolBarPopupAction( backForward.first, ALT+Key_Left,
                                     _stackSelection, SLOT(browserBack()),
                                     actionCollection(), "go_back" );
  connect( _paBack->popupMenu(), SIGNAL( aboutToShow() ),
           this, SLOT( backAboutToShow() ) );
  connect( _paBack->popupMenu(), SIGNAL( activated( int ) ),
           this, SLOT( backActivated( int ) ) );
  hint = i18n("Go back in function selection history");
  _paBack->setToolTip( hint );
  _paBack->setWhatsThis( hint );

  _paForward = new KToolBarPopupAction( backForward.second, ALT+Key_Right,
                                        _stackSelection,
                                        SLOT(browserForward()),
                                        actionCollection(), "go_forward" );
  connect( _paForward->popupMenu(), SIGNAL( aboutToShow() ),
           this, SLOT( forwardAboutToShow() ) );
  connect( _paForward->popupMenu(), SIGNAL( activated( int ) ),
           this, SLOT( forwardActivated( int ) ) );
  hint = i18n("Go forward in function selection history");
  _paForward->setToolTip( hint );
  _paForward->setWhatsThis( hint );
#endif

  _saCost = new KSelectAction( i18n("Primary Event Type"), KShortcut(),
			       actionCollection(), "view_cost_type");
  hint = i18n("Select primary event type of costs");
  _saCost->setComboWidth(300);
  _saCost->setToolTip( hint );
  _saCost->setWhatsThis( hint );

  // cost types are dependent on loaded data, thus KSelectAction
  // is filled in setData()
  connect( _saCost, SIGNAL(activated(const QString&)),
           this, SLOT(costTypeSelected(const QString&)));

  _saCost2 = new KSelectAction( i18n("Secondary Event Type"), KShortcut(),
				actionCollection(), "view_cost_type2");
  hint = i18n("Select secondary event type for cost e.g. shown in annotations");
  _saCost2->setComboWidth(300);
  _saCost2->setToolTip( hint );
  _saCost2->setWhatsThis( hint );

  connect( _saCost2, SIGNAL(activated(const QString&)),
           this, SLOT(costType2Selected(const QString&)));

  saGroup = new KSelectAction( i18n("Grouping"), KShortcut(),
                               actionCollection(), "view_group_type");

  hint = i18n("Select how functions are grouped into higher level cost items");
  saGroup->setToolTip( hint );
  saGroup->setWhatsThis( hint );

  QStringList args;

  args << i18n("(No Grouping)")
       << TraceCost::i18nTypeName(TraceItem::Object)
       << TraceCost::i18nTypeName(TraceItem::File)
       << TraceCost::i18nTypeName(TraceItem::Class)
       << TraceCost::i18nTypeName(TraceItem::FunctionCycle);

  saGroup->setItems(args);
  connect( saGroup, SIGNAL(activated(int)),
           this, SLOT(groupTypeSelected(int)));

  _taSplit = new KToggleAction(i18n("Split"), "view_left_right", KShortcut(),
                               this, SLOT(splitSlot()),
                               actionCollection(), "view_split");

  hint = i18n("Show two information panels");
  _taSplit->setToolTip( hint );
  _taSplit->setWhatsThis( hint );

 _taSplitDir = new KToggleAction(i18n("Split Horizontal"),
                                  "view_left_right", KShortcut(),
                                  this, SLOT(splitDirSlot()),
                                  actionCollection(), "view_split_dir");

  hint = i18n("Change Split Orientation when main window is split.");
  _taSplitDir->setToolTip( hint );
  _taSplitDir->setWhatsThis( hint );

  // copied from KMail...
#if KDE_VERSION >= 308 // KDE 3.1
  KStdAction::tipOfDay( this, SLOT( slotShowTip() ), actionCollection() );
#else
  (void) new KAction( KGuiItem( i18n("Tip of the &Day..."), "idea",
                                i18n("Show \"Tip of the Day\"") ),
                      0, this, SLOT(slotShowTip()),
                      actionCollection(), "help_show_tip" );
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

  // FIXME: Delete when no view gets this config from Configuration
  Configuration::setShowPercentage(_showPercentage);

  _partSelection->refresh();
  _stackSelection->refresh();

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

  // FIXME: Delete when no view gets this config from Configuration
  Configuration::setShowExpanded(_showExpanded);

  _partSelection->refresh();
  _stackSelection->refresh();

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

  // FIXME: Delete when no view gets this config from Configuration
  Configuration::setShowCycles(_showCycles);

  if (!_data) return;

  _data->invalidateDynamicCost();
  _data->updateFunctionCycles();

  _partSelection->refresh();
  _stackSelection->rebuildStackList();

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
  _functionSelection->query(queryLineEdit->text());
}

void TopLevel::configureKeys()
{
#if KDE_VERSION > 0x030190
  // for KDE 3.2: KKeyDialog::configureKeys is deprecated
  KKeyDialog::configure(actionCollection(), this, true);
#else
  KKeyDialog::configureKeys(actionCollection(), xmlFile(), true, this);
#endif
}


void TopLevel::configureToolbars()
{
  KEditToolbar *dlg = new KEditToolbar(guiFactory(),this);

  if (dlg->exec())
    createGUI();

  delete dlg;
}


void TopLevel::newTrace()
{
  // start cachegrind on command...
}

void TopLevel::newWindow()
{
  TopLevel* t = new TopLevel(0);
  t->show();
}


void TopLevel::loadTrace()
{
    KURL url = KFileDialog::getOpenURL(":",
                                       i18n("cachegrind.out* callgrind.out*|Callgrind Profile Data\n*|All Files"),
                                       this,
                                       i18n("Select Callgrind Profile Data"));
    loadTrace(url);
}

void TopLevel::loadTrace(const KURL& url)
{
  if (url.isEmpty()) return;

  // network transparancy
  QString tmpFile;
#if KDE_VERSION > 0x030190
  // for KDE 3.2: KIO::NetAccess::download with 2 args is deprecated
  if(KIO::NetAccess::download( url, tmpFile, this )) {
#else
  if(KIO::NetAccess::download( url, tmpFile )) {
#endif
    _openRecent->addURL(url);
    _openRecent->saveEntries( KGlobal::config() );

    loadTrace(tmpFile);
    KIO::NetAccess::removeTempFile( tmpFile );
  }
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
    KURL url = KFileDialog::getOpenURL(QString::null,
                                       i18n("cachegrind.out* callgrind.out*|Callgrind Profile Data\n*|All Files"),
                                       this,
                                       i18n("Add Callgrind Profile Data"));
    addTrace(url);
}

void TopLevel::addTrace(const KURL& url)
{
  if (url.isEmpty()) return;

  // network transparancy
  QString tmpFile;
#if KDE_VERSION > 0x030190
  // for KDE 3.2: KIO::NetAccess::download with 2 args is deprecated
  if(KIO::NetAccess::download( url, tmpFile, this )) {
#else
  if(KIO::NetAccess::download( url, tmpFile )) {
#endif
    _openRecent->addURL(url);
    _openRecent->saveEntries( KGlobal::config() );

    addTrace(tmpFile);
    KIO::NetAccess::removeTempFile( tmpFile );
  }
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
  _loadTraceDelayed = QString::null;
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
  GraphExporter ge(_data, _function, _costType, _groupType, n);
  ge.writeDot();

  QString cmd = QString("(dot %1 -Tps > %2.ps; kghostview %3.ps)&")
                .arg(n).arg(n).arg(n);
  system(QFile::encodeName( cmd ));
}


bool TopLevel::setCostType(QString s)
{
  TraceCostType* ct;

  ct = (_data) ? _data->mapping()->type(s) : 0;

  // if costtype with given name not found, use first available
  if (!ct && _data) ct = _data->mapping()->type(0);

  return setCostType(ct);
}

bool TopLevel::setCostType2(QString s)
{
  TraceCostType* ct;

  // Special type i18n("(Hidden)") gives 0
  ct = (_data) ? _data->mapping()->type(s) : 0;

  return setCostType2(ct);
}

void TopLevel::costTypeSelected(const QString& s)
{
  TraceCostType* ct;

  ct = (_data) ? _data->mapping()->typeForLong(s) : 0;
  setCostType(ct);
}

void TopLevel::costType2Selected(const QString& s)
{
  TraceCostType* ct;

  ct = (_data) ? _data->mapping()->typeForLong(s) : 0;
  setCostType2(ct);
}

bool TopLevel::setCostType(TraceCostType* ct)
{
  if (_costType == ct) return false;
  _costType = ct;

  if (ct) {
      int idx=0;
      QStringList l = _saCost->items();
      for (QStringList::Iterator it = l.begin(); it != l.end(); ++it, ++idx ) {
	  if (*it == ct->longName())
	      _saCost->setCurrentItem(idx);
      }
  }

  _partSelection->setCostType(_costType);
  _stackSelection->setCostType(_costType);

  _functionSelection->setCostType(_costType);
  _functionSelection->updateView();

  _multiView->setCostType(_costType);
  _multiView->updateView();

  updateStatusBar();

  return true;
}

bool TopLevel::setCostType2(TraceCostType* ct)
{
  if (_costType2 == ct) return false;
  _costType2 = ct;

  QString longName = ct ? ct->longName() : i18n("(Hidden)");

  int idx=0;
  QStringList l = _saCost2->items();
  for (QStringList::Iterator it = l.begin(); it != l.end(); ++it, ++idx ) {
    if (*it == longName)
      _saCost2->setCurrentItem(idx);
  }

  _partSelection->setCostType2(_costType2);
  _stackSelection->setCostType2(_costType2);

  _functionSelection->setCostType2(_costType2);
  _functionSelection->updateView();

  _multiView->setCostType2(_costType2);
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

  if (saGroup->currentItem() != idx)
    saGroup->setCurrentItem(idx);

  _stackSelection->setGroupType(_groupType);
  _partSelection->setGroupType(_groupType);

  _functionSelection->set(_groupType);
  _functionSelection->updateView();

  _multiView->set(_groupType);
  _multiView->updateView();

  updateStatusBar();

  return true;
}

bool TopLevel::setGroup(QString s)
{
    return true;
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

  TraceCost* f = _data->search(TraceItem::Function, s, _costType);
  if (!f) return false;

  return setFunction((TraceFunction*)f);
}

bool TopLevel::setFunction(TraceFunction* f)
{
  _multiView->activate(f);
  _multiView->updateView();

  _functionSelection->activate(f);
  _functionSelection->updateView();

  if (_function == f) return false;
  _function = f;

  _partSelection->setFunction(_function);
  _stackSelection->setFunction(_function);

  StackBrowser* b = _stackSelection->browser();
  if (b) {
    // don't disable up: a press forces stack-up extending...
    _paForward->setEnabled(b->canGoForward());
    _paBack->setEnabled(b->canGoBack());
  }

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
void TopLevel::setCostTypeDelayed(TraceCostType* ct)
{
  _costTypeDelayed = ct;
  QTimer::singleShot (0, this, SLOT(setCostTypeDelayed()));
}

void TopLevel::setCostType2Delayed(TraceCostType* ct)
{
  _costType2Delayed = ct;
  QTimer::singleShot (0, this, SLOT(setCostType2Delayed()));
}

void TopLevel::setCostTypeDelayed()
{
  setCostType(_costTypeDelayed);
}

void TopLevel::setCostType2Delayed()
{
  setCostType2(_costType2Delayed);
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


void TopLevel::setTraceItemDelayed(TraceItem* i)
{
  // no need to select same item a 2nd time...
  if (_traceItemDelayed == i) return;
  _traceItemDelayed = i;
  _lastSender = sender();

  kdDebug() << "Selected " << (i ? i->prettyName() : "(none)") << endl;

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
    _stackSelection->setData(0);

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
      TraceCostMapping* m = _data->mapping();
      m->addKnownVirtualTypes();

      /* first, fill selection list with available cost types */
      for (int i=0;i<m->realCount();i++)
	  types << m->realType(i)->longName();
      for (int i=0;i<m->virtualCount();i++)
	  types << m->virtualType(i)->longName();
  }
  _saCost->setItems(types);
  _saCost->setComboWidth(300);

  if (types.count()>0) {
    // second type list gets an additional "(Hidden)"
    types.prepend(i18n("(Hidden)"));
  }
  _saCost2->setItems(types);
  _saCost2->setComboWidth(300);
  // default is hidden
  if (types.count()>0)
    _saCost2->setCurrentItem(0);

  _partSelection->setData(_data);
  _stackSelection->setData(_data);
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
      caption += " [" + _data->command() + "]";
  }
  setCaption(caption);

  if (!_data || (!_forcePartDock && _data->parts().count()<2)) {
    _partDock->hide();
    _partDockShown->setChecked(false);
  }
  else {
    _partDock->show();
    _partDockShown->setChecked(true);
  }

  updateStatusBar();
}

void TopLevel::addCostMenu(QPopupMenu* popup, bool withCost2)
{
  if (_data) {
    QPopupMenu *popup1 = new QPopupMenu(popup);
    QPopupMenu *popup2 = 0;
    popup1->setCheckable(true);

    if (withCost2) {
      popup2 = new QPopupMenu(popup);
      popup2->setCheckable(true);

      if (_costType2) {
	popup2->insertItem(i18n("Hide"),199);
	popup2->insertSeparator();
      }
    }

    TraceCostMapping* m = _data->mapping();
    TraceCostType* ct;
    for (int i=0;i<m->realCount();i++) {
      ct = m->realType(i);
      popup1->insertItem(ct->longName(), 100+i);
      if (_costType == ct) popup1->setItemChecked(100+i,true);
      if (popup2) {
	popup2->insertItem(ct->longName(), 100+i);
	if (_costType2 == ct) popup2->setItemChecked(100+i,true);
      }
    }
    for (int i=0;i<m->virtualCount();i++) {
      ct = m->virtualType(i);
      popup1->insertItem(ct->longName(), 200+i);
      if (_costType == ct) popup1->setItemChecked(200+i,true);
      if (popup2) {
	popup2->insertItem(ct->longName(), 200+i);
	if (_costType2 == ct) popup2->setItemChecked(200+i,true);
      }
    }
    popup->insertItem(i18n("Primary Event Type"), popup1);
    connect(popup1,SIGNAL(activated(int)),this,SLOT(setCostType(int)));
    if (popup2) {
      popup->insertItem(i18n("Secondary Event Type"), popup2);
      connect(popup2,SIGNAL(activated(int)),this,SLOT(setCostType2(int)));
    }
  }
  if (_showPercentage)
    popup->insertItem(i18n("Show Absolute Cost"),
		      this, SLOT(setAbsoluteCost()));
  else
    popup->insertItem(i18n("Show Relative Cost"),
		      this, SLOT(setRelativeCost()));
}

bool TopLevel::setCostType(int id)
{
  if (!_data) return false;

  TraceCostMapping* m = _data->mapping();
  TraceCostType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->virtualType(id-200);

  return ct ? setCostType(ct) : false;
}

bool TopLevel::setCostType2(int id)
{
  if (!_data) return false;

  TraceCostMapping* m = _data->mapping();
  TraceCostType* ct=0;
  if (id >=100 && id<199) ct = m->realType(id-100);
  if (id >=200 && id<299) ct = m->virtualType(id-200);

  return setCostType2(ct);
}

void TopLevel::addGoMenu(QPopupMenu* popup)
{
  popup->insertItem(i18n("Go Back"), this, SLOT(goBack()));
  popup->insertItem(i18n("Go Forward"), this, SLOT(goForward()));
  popup->insertItem(i18n("Go Up"), this, SLOT(goUp()));
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
  if (!_data || _data->command().isEmpty()) return QString::null;

  QString name = _data->command();
  QString key;
  for (unsigned int l=0;l<name.length();l++)
    if (name[l].isLetterOrNumber()) key += name[l];

  return QString("-") + key;
}


void TopLevel::restoreTraceTypes()
{
  QString key = traceKey();

  KConfigGroup cConfig(KGlobal::config(), QCString("CurrentState"));
  KConfigGroup pConfig(KGlobal::config(), QCString("TracePositions"));

  QString groupType, costType, costType2;
  groupType =  pConfig.readEntry(QString("GroupType%1").arg(key));
  costType  =  pConfig.readEntry(QString("CostType%1").arg(key));
  costType2 =  pConfig.readEntry(QString("CostType2%1").arg(key));

  if (groupType.isEmpty()) groupType = cConfig.readEntry("GroupType");
  if (costType.isEmpty()) costType = cConfig.readEntry("CostType");
  if (costType2.isEmpty()) costType2 = cConfig.readEntry("CostType2");

  setGroupType(groupType);
  setCostType(costType);
  setCostType2(costType2);

  // if still no cost type set, use first available
  if (!_costType && !_saCost->items().isEmpty())
      costTypeSelected(_saCost->items().first());

  KConfigGroup aConfig(KGlobal::config(), QCString("Layouts"));
  _layoutCount = aConfig.readNumEntry(QString("Count%1").arg(key), 0);
  _layoutCurrent = aConfig.readNumEntry(QString("Current%1").arg(key), 0);
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

  KConfigGroup pConfig(KGlobal::config(), QCString("TracePositions"));
  QString group = pConfig.readEntry(QString("Group%1").arg(key));
  if (!group.isEmpty()) setGroup(group);

  restoreCurrentState(key);

  // restoreCurrentState() usually leads to a call to setTraceItemDelayed()
  // to restore last active item...
  if (!_traceItemDelayed) {
    // function not available any more.. try with "main"
    if (!setFunction("main"))
      _functionSelection->setTopFunction();
  }
}


/* Layout */

void TopLevel::layoutDuplicate()
{
  // save current and allocate a new slot
  _multiView->saveViewConfig(KGlobal::config(),
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     traceKey(), false);
  _layoutCurrent = _layoutCount;
  _layoutCount++;

  updateLayoutActions();

  kdDebug() << "TopLevel::layoutDuplicate: count " << _layoutCount << endl;
}

void TopLevel::layoutRemove()
{
  if (_layoutCount <2) return;

  int from = _layoutCount-1;
  if (_layoutCurrent == from) { _layoutCurrent--; from--; }
  // restore from last and decrement count
  _multiView->readViewConfig(KGlobal::config(),
			     QString("Layout%1-MainView").arg(from),
			     traceKey(), false);
  _layoutCount--;

  updateLayoutActions();
}

void TopLevel::layoutNext()
{
  if (_layoutCount <2) return;

  KConfig* config = KGlobal::config();
  QString key = traceKey();

  _multiView->saveViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);
  _layoutCurrent++;
  if (_layoutCurrent == _layoutCount) _layoutCurrent = 0;

  _multiView->readViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);

  if (0) kdDebug() << "TopLevel::layoutNext: current "
		   << _layoutCurrent << endl;
}

void TopLevel::layoutPrevious()
{
  if (_layoutCount <2) return;

  KConfig* config = KGlobal::config();
  QString key = traceKey();

  _multiView->saveViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);
  _layoutCurrent--;
  if (_layoutCurrent <0) _layoutCurrent = _layoutCount-1;

  _multiView->readViewConfig(config,
			     QString("Layout%1-MainView").arg(_layoutCurrent),
			     key, false);

  if (0) kdDebug() << "TopLevel::layoutPrevious: current "
		   << _layoutCurrent << endl;
}

void TopLevel::layoutSave()
{
  KConfig* config = KGlobal::config();
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

  KConfigGroup aConfig(config, QCString("Layouts"));
  aConfig.writeEntry("DefaultCount", _layoutCount);
  aConfig.writeEntry("DefaultCurrent", _layoutCurrent);
}

void TopLevel::layoutRestore()
{
  KConfig* config = KGlobal::config();
  KConfigGroup aConfig(config, QCString("Layouts"));
  _layoutCount = aConfig.readNumEntry("DefaultCount", 0);
  _layoutCurrent = aConfig.readNumEntry("DefaultCurrent", 0);
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
}


void TopLevel::updateLayoutActions()
{
  KAction* ka;

  ka = actionCollection()->action("layout_next");
  if (ka) ka->setEnabled(_layoutCount>1);

  ka = actionCollection()->action("layout_previous");
  if (ka) ka->setEnabled(_layoutCount>1);

  ka = actionCollection()->action("layout_remove");
  if (ka) ka->setEnabled(_layoutCount>1);

  _statusbar->message(i18n("Layout Count: %1").arg(_layoutCount), 1000);
}


void TopLevel::updateStatusBar()
{
  if (!_data || _data->parts().count()==0) {
    _statusLabel->setText(i18n("No profile data file loaded."));
    return;
  }

  QString status = QString("%1 [%2] - ")
                   .arg(_data->shortTraceName())
                   .arg(_data->activePartRange());

  if (_costType) {
    status += i18n("Total %1 Cost: %2")
      .arg(_costType->longName())
      .arg(_data->prettySubCost(_costType));

    /* this gets too long...
    if (_costType2 && (_costType2 != _costType))
      status += i18n(", %1 Cost: %2")
	.arg(_costType2->longName())
	.arg(_data->prettySubCost(_costType2));
    */
  }
  else
    status += i18n("No event type selected");

  /* Not working... should give group of selected function
  
  if (_groupType != TraceItem::Function) {
    status += QString(" - %1 '%2'")
              .arg(TraceItem::i18nTypeName(_groupType))
              .arg(_group ? _group->prettyName() : i18n("(None)"));
  }
  */

  _statusLabel->setText(status);
}

void TopLevel::configure()
{
  if (ConfigDlg::configure(Configuration::config(), _data, this)) {
    Configuration::saveOptions(KGlobal::config());

    configChanged();
  }
  else
    Configuration::readOptions(KGlobal::config());
}

bool TopLevel::queryClose()
{
  saveTraceSettings();

  return true;
}

bool TopLevel::queryExit()
{
    // save current toplevel options as defaults...
    Configuration::setShowPercentage(_showPercentage);
    Configuration::setShowExpanded(_showExpanded);
    Configuration::setShowCycles(_showCycles);
    Configuration::saveOptions(KGlobal::config());

    saveCurrentState(QString::null);

  // save QT dock positions...

  // We don't want to save the KToolbar position here.
  // Its already stored.
  delete toolBar();

  KConfigGroup dockConfig(KGlobal::config(), QCString("Docks"));
  QString str;
  QTextStream ts( &str, IO_WriteOnly );
  ts << *this;
#if 1
  dockConfig.writeEntry("Position", str);
#else
  /* We store this with a localized key because for dock positions,
   * QT uses the localized captions of docks.
   * This way, when changing languages, you don't loose dock position
   * settings.
   * For the retrieval to work, we need to store a non-localized.
   */
  dockConfig.writeEntry("Position", str, true, false, true);
#endif

  // if part dock was chosen visible even for only 1 part loaded,
  // keep this choice...
  _forcePartDock = false;
  if (_data && (_data->parts().count()<2) && _partDock->isVisible())
    _forcePartDock=true;
  dockConfig.writeEntry("ForcePartDockVisible", _forcePartDock);

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
  //_showPercentage->setChecked(Configuration::showPercentage());

  // invalidate found/cached dirs of source files
  _data->resetSourceDirs();

  _partSelection->refresh();
  _stackSelection->refresh();

  _functionSelection->notifyChange(TraceItemView::configChanged);
  _functionSelection->updateView();

  _multiView->notifyChange(TraceItemView::configChanged);
  _multiView->updateView();
}

void TopLevel::slotShowTipOnStart() {
  KTipDialog::showTip(this);
}

void TopLevel::slotShowTip() {
  KTipDialog::showTip( this, QString::null, true );
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

  _partSelection->activePartsChangedSlot(list);

  _multiView->set(list);
  _multiView->updateView();

  _functionSelection->set(list);
  _functionSelection->updateView();

  _stackSelection->refresh();

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
    cmd.open(IO_WriteOnly);
    cmd.writeBlock("DUMP\n", 5);
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
  QPopupMenu *popup = _paForward->popupMenu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f;

  if (!hi) {
    popup->insertItem(i18n("(No Stack)"));
    return;
  }

  hi = hi->next();
  if (!hi) {
    popup->insertItem(i18n("(No next function)"));
    return;
  }

  int count = 1;
  while (count<Configuration::maxSymbolCount() && hi) {
    f = hi->function();
    if (!f) break;

    QString name = f->prettyName();
    if ((int)name.length()>Configuration::maxSymbolLength())
      name = name.left(Configuration::maxSymbolLength()) + "...";

    //qDebug("forward: Adding %s", name.ascii());
    popup->insertItem(name, count);
    hi = hi->next();
    count++;
  }
}

void TopLevel::backAboutToShow()
{
  QPopupMenu *popup = _paBack->popupMenu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f;

  if (!hi) {
    popup->insertItem(i18n("(No Stack)"));
    return;
  }

  hi = hi->last();
  if (!hi) {
    popup->insertItem(i18n("(No previous function)"));
    return;
  }

  int count = 1;
  while (count<Configuration::maxSymbolCount() && hi) {
    f = hi->function();
    if (!f) break;

    QString name = f->prettyName();
    if ((int)name.length()>Configuration::maxSymbolLength())
      name = name.left(Configuration::maxSymbolLength()) + "...";

    //qDebug("back: Adding %s", name.ascii());
    popup->insertItem(name, count);
    hi = hi->last();
    count++;
  }
}

void TopLevel::upAboutToShow()
{
  QPopupMenu *popup = _paUp->popupMenu();

  popup->clear();
  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  TraceFunction* f = hi ? hi->function() : 0;

  if (!f) {
    popup->insertItem(i18n("(No Stack)"));
    return;
  }
  f = hi->stack()->caller(f, false);
  if (!f) {
    popup->insertItem(i18n("(No Function Up)"));
    return;
  }

  int count = 1;
  while (count<Configuration::maxSymbolCount() && f) {
    QString name = f->prettyName();
    if ((int)name.length()>Configuration::maxSymbolLength())
      name = name.left(Configuration::maxSymbolLength()) + "...";

    popup->insertItem(name, count);
    f = hi->stack()->caller(f, false);
    count++;
  }

}

void TopLevel::forwardActivated(int id)
{
  //qDebug("forwardActivated: %d", id);

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  if (!b) return;

  while (id>1) {
    b->goForward();
    id--;
  }
  _stackSelection->browserForward();
}

void TopLevel::backActivated(int id)
{
  //qDebug("backActivated: %d", id);

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  if (!b) return;

  while (id>1) {
    b->goBack();
    id--;
  }
  _stackSelection->browserBack();
}

void TopLevel::upActivated(int id)
{
  //qDebug("upActivated: %d", id);

  StackBrowser* b = _stackSelection ? _stackSelection->browser() : 0;
  HistoryItem* hi = b ? b->current() : 0;
  if (!hi) return;

  TraceFunction* f = hi->function();

  while (id>0 && f) {
    f = hi->stack()->caller(f, false);
    id--;
  }

  //qDebug("upActivated: %s", f ? f->prettyName().ascii() : "??" );
  if (f)
    setFunction(f);

}

void TopLevel::showMessage(const QString& msg, int ms)
{
  if (_statusbar)
    _statusbar->message(msg, ms);
}

void TopLevel::showStatus(QString msg, int progress)
{
    static bool msgUpdateNeeded = true;

    if (msg.isEmpty()) {
	if (_progressBar) {
	    _statusbar->removeWidget(_progressBar);
	    delete _progressBar;
	    _progressBar = 0;
	}
	_statusbar->clear();
	_progressMsg = msg;
	return;
    }

    if (_progressMsg.isEmpty())	_progressStart.start();

    if (msg != _progressMsg) {
	_progressMsg = msg;
	msgUpdateNeeded = true;
    }

    // do nothing if last change was less than 0.5 seconds ago
    if (_progressStart.elapsed() < 500) return;

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
    _progressBar->setProgress(progress);

    // let the progress bar update itself
#if (QT_VERSION-0 >= 0x030100)
    QEventLoop* l = qApp->eventLoop();
    if (l) l->processEvents(QEventLoop::ExcludeUserInput);
#else
    // for Qt 3.0.x. This allows user input and thus potentially races
    qApp->processEvents();
#endif
}

#include "toplevel.moc"
