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

#include <stdlib.h> // for system()

#include <qvbox.h>
#include <qtimer.h>
#include <qwhatsthis.h>
#include <qlineedit.h>
#include <qtextstream.h>
#include <qsizepolicy.h>
#include <qprogressbar.h>

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

#include "toplevel.h"
#include "partselection.h"
#include "functionselection.h"
#include "dumpselection.h"
#include "stackselection.h"
#include "stackbrowser.h"
#include "tracedata.h"
#include "configuration.h"
#include "configdlg.h"
#include "kwidgetaction.h" // needed for KDE 3.0.x
#include "multiview.h"
#include "callgraphview.h"

#define TRACE_UPDATES 1

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
  _dumpDockShown->setChecked(!_dumpDock->isHidden());
  _functionDockShown->setChecked(!_functionDock->isHidden());

  connect(_partDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(partVisibilityChanged(bool)));
  connect(_stackDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(stackVisibilityChanged(bool)));
  connect(_dumpDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(dumpVisibilityChanged(bool)));
  connect(_functionDock, SIGNAL(visibilityChanged(bool)),
          this, SLOT(functionVisibilityChanged(bool)));

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
  _groupType = TraceCost::NoCostType;
  _group = 0;

  // for delayed slots
  _traceItemDelayed = 0;
  _costTypeDelayed = 0;
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
  stateConfig.writeEntry("GroupType", TraceItem::typeName(_groupType));

  _multiView->saveViewConfig(kconfig, QString("MainView"), postfix);
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
  pConfig.writeEntry(QString("GroupType%1").arg(key),
                     TraceItem::typeName(_groupType));

  if (!_data) return;

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

  _multiView->readViewConfig(kconfig, QString("MainView"), postfix);
  _taSplit->setChecked(_multiView->childCount()>1);
  _taSplitDir->setEnabled(_multiView->childCount()>1);
  _taSplitDir->setChecked(_multiView->orientation() == Qt::Horizontal);
}


void TopLevel::createDocks()
{
  _partDock = new QDockWindow(QDockWindow::InDock, this);
  _partDock->setCaption(i18n("Trace Part Overview"));
  _partDock->setCloseMode( QDockWindow::Always );
  _partSelection = new PartSelection(_partDock, "partSelection");
  _partDock->setWidget(_partSelection);
  _partDock->setResizeEnabled(true);
  _partDock->setFixedExtentWidth(200);
  QWhatsThis::add( _partSelection, i18n(
                   "<b>The Trace Part Overview</b>"
                   "<p>A trace consists of multiple trace parts when "
                   "there are several trace files from one profile run. "
                   "The Trace Part Overview dockable shows these, "
                   "horizontally ordered in execution time; "
                   "the rectangle sizes are proportional to the total "
                   "cost spent in the parts. You can select one or several "
                   "parts to constrain all costs shown to these parts only."
                   "</p>"
                   "<p>The parts are further subdivided: there is a "
                   "partitioning and a cumulative split mode: "
                   "<ul><li>Partitioning: You see the "
                   "partitioning into groups for a trace part, according to "
                   "the group type selected. E.g. if ELF object groups are "
                   "selected, you see colored rectangles for each "
                   "used ELF object (shared library or executable), sized "
                   "according to the cost spent therein.</li>"
                   "<li>Cumulative Split: A rectangle showing the cumulative "
                   "cost of the current selected function in the trace part "
                   "is shown. "
                   "This again is split up to show cumulative costs of its "
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
                   "It is build up by starting with the current selected "
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
                   "are spent in, depending on the choosen group type. "
                   "The group list is hidden when group type 'Function' "
                   "is selected.<p>"
                   "<p>The function list contains the functions of the "
                   "selected group (or all for 'Function' group type), "
                   "ordered by the costs spent therein. Functions with "
                   "costs less than 1% are hidden on default.</p>"));

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
                   "option to let KCachegrind regularily poll these data. "
                   "Check the <b>Sync</b> option to let the dockable activate "
                   "the top function in the current loaded dump.</ul></p>"));


  // Restore QT Dock positions...
  KConfigGroup dockConfig(KGlobal::config(), QCString("Docks"));
  QString str = dockConfig.readEntry("Position", QString::null);
  if (0) qDebug("Docks/Position: '%s'", str.ascii());
  if (str.isEmpty()) {
    // default positions
    addDockWindow(_dumpDock, DockLeft);
    addDockWindow(_partDock, DockLeft);
    addDockWindow(_stackDock, DockLeft);
    addDockWindow(_functionDock, DockLeft);
    _stackDock->hide();
    _dumpDock->hide();
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

void TopLevel::createActions()
{
  QString hint;
  KAction* action;

  action = KStdAction::openNew(this, SLOT(newWindow()), actionCollection());
  hint = i18n("<b>New</b><p>Open new empty KCachegrind window.</p>");
  action->setWhatsThis( hint );

  action = new KAction( i18n( "&Reload" ), "reload",
                        KStdAccel::key(KStdAccel::Reload),
                        this, SLOT( reload() ), actionCollection(), "reload" );
  hint = i18n("<b>Reload trace</b>"
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
                               KStdAccel::key(KStdAccel::Redo),
                               this, SLOT( forceTrace() ),
                               actionCollection(), "dump" );
  hint = i18n("<b>Force Dump</b>"
              "<p>This forces a dump for a Cachegrind profile run "
              "in the current directory. This action is checked while "
              "KCachegrind looks for the dump. If the dump is "
              "finished, it automatically reloads the current trace. "
              "If this is the one from the running Cachegrind, the new "
              "created trace part will be loaded, too.</p>"
              "<p>Force dump creates a file 'cachegrind.cmd', and "
              "checks every second for its existance. A running "
              "Cachegrind will detect this file, dump a trace part, "
              "and delete 'cachegrind.cmd'. "
              "The deletion is detected by KCachegrind, "
              "and it does a Reload. If there's <em>no</em> Cachegrind "
              "running, press 'Force Dump' again to cancel the dump "
              "request. This deletes 'cachegrind.cmd' itself and "
              "stops polling for a new dump.</p>"
              "<p>Note: A Cachegrind run <em>only</em> detects "
              "existance of 'cachegrind.cmd' when actively running "
              "a few milliseconds, i.e. "
              "<em>not</em> sleeping. Tip: For a profiled GUI program, "
              "you can awake Cachegrind e.g. by resizing a window "
              "of the program.</p>"
              "<p>Note: You need the call-tree version for Cachegrind!</p>");
  _taDump->setWhatsThis( hint );

  action = KStdAction::open(this, SLOT(loadTrace()), actionCollection());
  hint = i18n("<b>Open Trace</b>"
              "<p>This opens a trace, with possible multiple parts</p>");
  action->setToolTip( hint );
  action->setWhatsThis( hint );

  _openRecent = KStdAction::openRecent(this, SLOT(loadTrace(const KURL&)),
                                       actionCollection());

  KStdAction::showStatusbar(this,
                            SLOT(toggleStatusBar()), actionCollection());

  _partDockShown = new KToggleAction(i18n("Trace Part Overview"), KShortcut(),
                                     this, SLOT(togglePartDock()),
                                     actionCollection(),
                                     "settings_show_partdock");

  hint = i18n("Show/Hide the Trace Part Overview Dockable");
  _partDockShown->setToolTip( hint );
  _partDockShown->setWhatsThis( hint );

  _stackDockShown = new KToggleAction(i18n("Top Cost Call Stack"), KShortcut(),
                                     this, SLOT(toggleStackDock()),
                                     actionCollection(),
                                     "settings_show_stackdock");

  hint = i18n("Show/Hide the Top Call Stack Dockable");
  _stackDockShown->setToolTip( hint );
  _stackDockShown->setWhatsThis( hint );

  _functionDockShown = new KToggleAction(i18n("Flat Profile"), KShortcut(),
                                         this, SLOT(toggleFunctionDock()),
                                         actionCollection(),
                                         "settings_show_profiledock");

  hint = i18n("Show/Hide the Flat Profile Dockable");
  _functionDockShown->setToolTip( hint );
  _functionDockShown->setWhatsThis( hint );

  _dumpDockShown = new KToggleAction(i18n("Profile Dumps"), KShortcut(),
                                     this, SLOT(toggleDumpDock()),
                                     actionCollection(),
                                     "settings_show_dumpdock");

  hint = i18n("Show/Hide the Profile Dumps Dockable");
  _dumpDockShown->setToolTip( hint );
  _dumpDockShown->setWhatsThis( hint );


  _taPercentage = new KToggleAction(i18n("Show Relative Costs"), "percent",
				    KShortcut(),
				    this, SLOT(togglePercentage()),
				    actionCollection(),
				    "view_percentage");

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

  _taCycles = new KToggleAction( i18n( "Show Cycles" ), "undo",
				 KShortcut(),
				 this, SLOT( toggleCycles() ), actionCollection(),
				 "view_cycles" );

  hint = i18n("<b>Detect recursive cycles</b>"
              "<p>If this is switched off, the treemap drawing will show "
	      "black areas when a recursive call is made instead of drawing the "
	      "recursion ad infinitum. Note that "
	      "the size of black areas often will be wrong, as inside of recursive "
	      "cycles, you can't determine the cost of calls. However, the error is "
	      "small for false cycles (see documentation)."
	      "<p>The correct handling for cycles is to detect them and collapse all "
	      "functions of a cycle into a virtual function. This is done when this "
	      "option is selected. Unfortunately with GUI applications, this often will "
	      "lead to huge false cycles, making the analysis impossible. Therefore the "
	      "possibility to switch this off.");
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

  saCost = new KSelectAction( i18n("Cost Type"), KShortcut(),
                              actionCollection(), "view_cost_type");
  hint = i18n("Select cost type of costs shown allover KCachegrind");
  saCost->setComboWidth(300);
  saCost->setToolTip( hint );
  saCost->setWhatsThis( hint );

  // cost types are dependent on loaded data, thus KSelectAction
  // is filled in setData()
  connect( saCost, SIGNAL(activated(const QString&)),
           this, SLOT(costTypeSelected(const QString&)));

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

  hint = i18n("Change Split Orientation when main window is splitted");
  _taSplitDir->setToolTip( hint );
  _taSplitDir->setWhatsThis( hint );

  queryLineEdit = new QLineEdit(0);
  QLabel* label = new QLabel( i18n("&Search:"), this, "kde toolbar widget" );
  label->setBuddy( queryLineEdit );
  KWidgetAction* wa;
  wa = new KWidgetAction( label, i18n("&Search:"), Key_F6,
                          0, 0,
                          actionCollection(), "query_label" );
  //wa->setAutoSized(true);

  action = new KWidgetAction(queryLineEdit, i18n("Function Query"),
                             KShortcut(),
                             this, SLOT(querySlot()),
                             actionCollection(), "query");
  connect(queryLineEdit, SIGNAL(returnPressed()),
          this, SLOT(querySlot()));

  QWhatsThis::add( queryLineEdit, i18n(
                     "Type part of a function name (insensitive) to get "
                     "a list of matching functions from current "
                     "function group"));

  // copied from KMail...
#if KDE_VERSION >= 308 // KDE 3.1
  KStdAction::tipOfDay( this, SLOT( slotShowTip() ), actionCollection() );
#else
  (void) new KAction( KGuiItem( i18n("Tip of the &Day..."), "idea",
                                i18n("Show \"Tip of the Day\"") ),
                      0, this, SLOT(slotShowTip()),
                      actionCollection(), "help_show_tip" );
#endif
};

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
  if (!_dumpDock->isVisible())
    _dumpDock->show();
  else
    _dumpDock->hide();
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
  bool show = _taPercentage->isChecked();
  if (_showPercentage == show) return;
  _showPercentage = show;

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

void TopLevel::dumpVisibilityChanged(bool v)
{
  _dumpDockShown->setChecked(v);
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
  KKeyDialog::configureKeys(actionCollection(), xmlFile(), true, this);
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
    KURL url = KFileDialog::getOpenURL(QString::null,
                                       i18n("cachegrind.out*|Cachegrind Trace"),
                                       this,
                                       i18n("Select Cachegrind Trace"));
    loadTrace(url);
}

void TopLevel::loadTrace(const KURL& url)
{
  if (url.isEmpty()) return;

  // network transparancy
  QString tmpFile;
  if(KIO::NetAccess::download( url, tmpFile )) {
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
  GraphExporter ge(_data, _function, _costType, n);
  ge.writeDot();

  QString cmd = QString("(dot %1 -Tps > %2.ps; kghostview %3.ps)&")
                .arg(n).arg(n).arg(n);
  system(cmd.ascii());
}


bool TopLevel::setCostType(QString s)
{
  TraceCostType* ct;

  ct = (_data) ? _data->mapping()->type(s) : 0;

  // if costtype with given name not found, use first available
  if (!ct && _data) ct = _data->mapping()->type(0);

  return setCostType(ct);
}

void TopLevel::costTypeSelected(const QString& s)
{
  TraceCostType* ct;

  ct = (_data) ? _data->mapping()->typeForLong(s) : 0;
  setCostType(ct);
}

bool TopLevel::setCostType(TraceCostType* ct)
{
  if (_costType == ct) return false;
  _costType = ct;

  if (ct) {
      int idx=0;
      QStringList l = saCost->items();
      for (QStringList::Iterator it = l.begin(); it != l.end(); ++it, ++idx ) {
	  if (*it == ct->longName())
	      saCost->setCurrentItem(idx);
      }
  }

  _partSelection->setCostType(_costType);
  _stackSelection->setCostType(_costType);

  _functionSelection->set(_costType);
  _functionSelection->updateView();

  _multiView->set(_costType);
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
 * temporary variable. And on parameterless slot for
 * forwarding, using this temporary.
 */
void TopLevel::setCostTypeDelayed(TraceCostType* ct)
{
  _costTypeDelayed = ct;
  QTimer::singleShot (0, this, SLOT(setCostTypeDelayed()));
}

void TopLevel::setCostTypeDelayed()
{
  setCostType(_costTypeDelayed);
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
 * A TraceData object can't be viewed many times in different
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
  saCost->setItems(types);
  saCost->setComboWidth(300);

  /* this is needed to let the other widgets know the types */
  restoreTraceTypes();

  _partSelection->setData(_data);
  _stackSelection->setData(_data);

  _functionSelection->setData(_data);
  _functionSelection->updateView();
  _multiView->setData(_data);
  _multiView->updateView();

  QString caption;
  if (_data) {
    caption = _data->traceName();
    if (!_data->command().isEmpty())
      caption += " [" + _data->command() + "]";
  }
  setCaption(caption);

  restoreTraceSettings();

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

  QString groupType, costType;
  groupType =  pConfig.readEntry(QString("GroupType%1").arg(key));
  costType  =  pConfig.readEntry(QString("CostType%1").arg(key));

  if (groupType.isEmpty()) groupType = cConfig.readEntry("GroupType");
  if (costType.isEmpty()) costType = cConfig.readEntry("CostType");

  setGroupType(groupType);
  setCostType(costType);

  // if still no cost type set, use first available
  if (!_costType && !saCost->items().isEmpty())
      costTypeSelected(saCost->items().first());

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


void TopLevel::updateStatusBar()
{
  if (!_data || _data->parts().count()==0) {
    _statusLabel->setText(i18n("No Trace File loaded."));
    return;
  }

  QString status = QString("%1 [%2] - ")
                   .arg(_data->shortTraceName())
                   .arg(_data->activePartRange());

  if (_costType)
    status += i18n("Total %1 Cost: %2")
      .arg(_costType->longName())
      .arg(_data->prettySubCost(_costType));
  else
    status += i18n("No cost type selected");

  if (_groupType != TraceItem::Function) {
    status += QString(" - %1 '%2'")
              .arg(TraceItem::i18nTypeName(_groupType))
              .arg(_group ? _group->prettyName() : i18n("(None)"));
  }

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

  // if part dock was choosen visible even for only 1 part loaded,
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

  _multiView->notifyChange(TraceItemView::partsChanged);
  _multiView->updateView();

  _functionSelection->notifyChange(TraceItemView::partsChanged);
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

  QFile cmd("cachegrind.cmd");
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

  QFile cmd("cachegrind.cmd");
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
