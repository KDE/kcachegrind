/* This file is part of KCachegrind.
   Copyright (C) 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * KCachegrind top level window
 */

#define TRACE_UPDATES 0
#define ENABLE_DUMPDOCK 0

#include "toplevel.h"

#include <stdlib.h> // for system()

#include <QDebug>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeDatabase>
#include <QProcess>
#include <QProgressBar>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QDBusConnection>

#include <KToggleAction>
#include <KToolBarPopupAction>
#include <KActionCollection>
#include <KSelectAction>
#include <KRecentFilesAction>
#include <KToolBar>
#include <KStandardGuiItem>
#include <KStandardShortcut>
#include <KStandardAction>
#include <KIO/Job>
#include <KJobWidgets>
#include <KEditToolBar>
#include <KShortcutsDialog>
#include <KTipDialog>
#include <KMessageBox>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KFilterDev>

#if ENABLE_DUMPDOCK
#include "dumpselection.h"
#endif

#include "partselection.h"
#include "functionselection.h"
#include "stackselection.h"
#include "stackbrowser.h"
#include "tracedata.h"
#include "globalguiconfig.h"
#include "config.h"
#include "configdlg.h"
#include "multiview.h"
#include "callgraphview.h"

TopLevel::TopLevel()
    : KXmlGuiWindow(nullptr)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/KCachegrind"), this, QDBusConnection::ExportScriptableSlots);

    _progressBar = nullptr;
    _statusbar = statusBar();
    _statusLabel = new QLabel(_statusbar);
    _statusbar->addWidget(_statusLabel, 1);
    _ccProcess = nullptr;

    _layoutCount = 1;
    _layoutCurrent = 0;

    resetState();

    KConfig *kconfig = KSharedConfig::openConfig().data();
    GlobalGUIConfig::config()->readOptions();

    createDocks();

    _multiView = new MultiView(this, this );
    _multiView->setObjectName(QStringLiteral("MultiView"));
    setCentralWidget(_multiView);

    createActions();

    _partDockShown->setChecked(!_partDock->isHidden());
    _stackDockShown->setChecked(!_stackDock->isHidden());
    _functionDockShown->setChecked(!_functionDock->isHidden());

    connect(_partDock, &QDockWidget::visibilityChanged,
            this, &TopLevel::partVisibilityChanged);
    connect(_stackDock, &QDockWidget::visibilityChanged,
            this, &TopLevel::stackVisibilityChanged);
    connect(_functionDock, &QDockWidget::visibilityChanged,
            this, &TopLevel::functionVisibilityChanged);

#if ENABLE_DUMPDOCK
    _dumpDockShown->setChecked(!_dumpDock->isHidden());
    connect(_dumpDock, SIGNAL(visibilityChanged(bool)),
            this, SLOT(dumpVisibilityChanged(bool)));
#endif


    // set toggle after reading configuration
    _showPercentage = GlobalConfig::showPercentage();
    _showExpanded   = GlobalConfig::showExpanded();
    _showCycles     = GlobalConfig::showCycles();
    _hideTemplates  = GlobalConfig::hideTemplates();
    _taPercentage->setChecked(_showPercentage);
    _taExpanded->setChecked(_showExpanded);
    _taCycles->setChecked(_showCycles);
    _taHideTemplates->setChecked(_hideTemplates);

    setupPartSelection(_partSelection);

    // KCachegrind for KDE 3.0.x does not allow to hide toolbars...
    setStandardToolBarMenuEnabled(true);
    _openRecent->loadEntries( KConfigGroup( kconfig, "" ) );

    // QT dock windows are created before (using QT position restoring)
    createGUI();

    setAutoSaveSettings();

    // restore current state settings (not configuration options)
    restoreCurrentState(QString());

    // if this is the first toplevel, show tip of day
    if (memberList().count() == 1)
        QTimer::singleShot( 200, this, &TopLevel::slotShowTipOnStart );
}

void TopLevel::resetState()
{
    _activeParts.clear();
    _hiddenParts.clear();

    _data = nullptr;
    _function = nullptr;
    _eventType = nullptr;
    _eventType2 = nullptr;
    _groupType = ProfileContext::InvalidType;
    _group = nullptr;

    // for delayed slots
    _traceItemDelayed = nullptr;
    _eventTypeDelayed = nullptr;
    _eventType2Delayed = nullptr;
    _groupTypeDelayed = ProfileContext::InvalidType;
    _groupDelayed = nullptr;
    _directionDelayed = TraceItemView::None;
    _lastSender = nullptr;
}


/**
 * Setup the part selection widget.
 * Statusbar has to be created before.
 */
void TopLevel::setupPartSelection(PartSelection* ps)
{
    // setup connections from the part selection widget

    connect(ps, &PartSelection::partsHideSelected,
            this, &TopLevel::partsHideSelectedSlotDelayed);
    connect(ps, &PartSelection::partsUnhideAll,
            this, &TopLevel::partsUnhideAllSlotDelayed);
}

/**
 * This saves the current state of the main window and
 * sub widgets.
 *
 * No positions are saved. These is done automatically for
 * KToolbar, and manually in queryClose() for Qt docks.
 */
void TopLevel::saveCurrentState(const QString& postfix)
{
    QString eventType = _eventType ? _eventType->name() : QStringLiteral("?");
    QString eventType2 = _eventType2 ? _eventType2->name() : QStringLiteral("?");

    ConfigGroup* stateConfig = ConfigStorage::group(QStringLiteral("CurrentState") + postfix);
    stateConfig->setValue(QStringLiteral("EventType"), eventType);
    stateConfig->setValue(QStringLiteral("EventType2"), eventType2);
    stateConfig->setValue(QStringLiteral("GroupType"), ProfileContext::typeName(_groupType));
    delete stateConfig;

    _partSelection->saveOptions(QStringLiteral("PartOverview"), postfix);
    _multiView->saveLayout(QStringLiteral("MainView"), postfix);
    _multiView->saveOptions(QStringLiteral("MainView"), postfix);
}

/**
 * This function is called when a trace is closed.
 * Save browsing position for later restoring
 */
void TopLevel::saveTraceSettings()
{
    QString key = traceKey();

    ConfigGroup* lConfig = ConfigStorage::group(QStringLiteral("Layouts"));
    lConfig->setValue(QStringLiteral("Count%1").arg(key), _layoutCount);
    lConfig->setValue(QStringLiteral("Current%1").arg(key), _layoutCurrent);
    delete lConfig;

    ConfigGroup* pConfig = ConfigStorage::group(QStringLiteral("TracePositions"));
    if (_eventType)
        pConfig->setValue(QStringLiteral("EventType%1").arg(key), _eventType->name());
    if (_eventType2)
        pConfig->setValue(QStringLiteral("EventType2%1").arg(key), _eventType2->name());
    if (_groupType != ProfileContext::InvalidType)
        pConfig->setValue(QStringLiteral("GroupType%1").arg(key),
                          ProfileContext::typeName(_groupType));

    if (_data) {
        if (_group)
            pConfig->setValue(QStringLiteral("Group%1").arg(key), _group->name());
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
    _partSelection->restoreOptions(QStringLiteral("PartOverview"), postfix);
    _multiView->restoreLayout(QStringLiteral("MainView"), postfix);
    _multiView->restoreOptions(QStringLiteral("MainView"), postfix);

    _taSplit->setChecked(_multiView->childCount()>1);
    _taSplitDir->setEnabled(_multiView->childCount()>1);
    _taSplitDir->setChecked(_multiView->orientation() == Qt::Horizontal);
}


void TopLevel::createDocks()
{
    _partDock = new QDockWidget(this);
    _partDock->setObjectName(QStringLiteral("part dock"));
    _partDock->setWindowTitle(i18n("Parts Overview"));
    _partSelection = new PartSelection(this, _partDock);
    _partDock->setWidget(_partSelection);

    _stackDock = new QDockWidget(this);
    _stackDock->setObjectName(QStringLiteral("stack dock"));
    // Why is the caption only correct with a close button?
    _stackSelection = new StackSelection(_stackDock);
    _stackDock->setWidget(_stackSelection);
    _stackDock->setWindowTitle(i18n("Top Cost Call Stack"));
    _stackSelection->setWhatsThis( i18n(
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

    _functionDock = new QDockWidget(this);
    _functionDock->setObjectName(QStringLiteral("function dock"));
    _functionDock->setWindowTitle(i18n("Flat Profile"));
    _functionSelection = new FunctionSelection(this, _functionDock);
    _functionDock->setWidget(_functionSelection);

#if ENABLE_DUMPDOCK
    _dumpDock = new QDockWidget(this);
    _dumpDock->setWindowTitle(i18n("Profile Dumps"));
    _dumpSelection = new DumpSelection(this, _dumpDock,
                                       "dumpSelection");
    _dumpSelection->setTopLevel(this);

    _dumpDock->setWidget(_dumpSelection);
    _dumpSelection->setWhatsThis( i18n(
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
    _partDock->hide();

#if ENABLE_DUMPDOCK
    addDockWidget( Qt::LeftDockWidgetArea, _dumpDock );
    _dumpDock->hide();
#endif

    KConfigGroup dockConfig(KSharedConfig::openConfig(), "Docks");
    _forcePartDock = dockConfig.readEntry("ForcePartDockVisible", false);
}


TopLevel::~TopLevel()
{
    delete _data;
}


void TopLevel::saveProperties(KConfigGroup & c)
{
    if ( _data )
        c.writeEntry("TraceName", _data->traceName());
}

void TopLevel::readProperties(const KConfigGroup &c)
{
    QString traceName = c.readEntry("TraceName");
    if (!traceName.isEmpty()) {
        openDataFile(traceName);
    }
}

void TopLevel::createLayoutActions()
{
    QString hint;
    QAction* action;

    action = actionCollection()->addAction( QStringLiteral("layout_duplicate") );
    action->setText( i18n( "&Duplicate" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutDuplicate);
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_Plus));
    hint = i18n("<b>Duplicate Current Layout</b>"
                "<p>Make a copy of the current layout.</p>");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("layout_remove") );
    action->setText( i18n( "&Remove" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutRemove);
    hint = i18n("<b>Remove Current Layout</b>"
                "<p>Delete current layout and make the previous active.</p>");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("layout_next") );
    action->setText( i18n( "&Go to Next" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutNext);
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_Right));
    hint = i18n("Go to Next Layout");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("layout_previous") );
    action->setText( i18n( "&Go to Previous" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutPrevious);
    actionCollection()->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_Left));
    hint = i18n("Go to Previous Layout");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("layout_restore") );
    action->setText( i18n( "&Restore to Default" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutRestore);
    hint = i18n("Restore Layouts to Default");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("layout_save") );
    action->setText( i18n( "&Save as Default" ) );
    connect(action, &QAction::triggered, this, &TopLevel::layoutSave);
    hint = i18n("Save Layouts as Default");
    action->setWhatsThis( hint );
}

// TODO: split this up...
void TopLevel::createMiscActions()
{
    QString hint;
    QAction* action;

    action = KStandardAction::openNew(this, SLOT(newWindow()), actionCollection());
    hint = i18n("<b>New</b><p>Open new empty KCachegrind window.</p>");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("file_add") );
    action->setText( i18n( "&Add..." ) );
    connect(action, SIGNAL(triggered(bool)), SLOT(add()));
    hint = i18n("<b>Add Profile Data</b>"
                "<p>This opens an additional profile data file in the current window.</p>");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("reload") );
    action->setIcon( QIcon::fromTheme(QStringLiteral("view-refresh")) );
    action->setText( i18nc("Reload a document", "&Reload" ) );
    connect(action, &QAction::triggered, this, &TopLevel::reload);
    actionCollection()->setDefaultShortcut(action, KStandardShortcut::Reload);
    hint = i18n("<b>Reload Profile Data</b>"
                "<p>This loads any new created parts, too.</p>");
    action->setWhatsThis( hint );

    action = actionCollection()->addAction( QStringLiteral("export") );
    action->setText( i18n( "&Export Graph" ) );
    connect(action, &QAction::triggered, this, &TopLevel::exportGraph);

    hint = i18n("<b>Export Call Graph</b>"
                "<p>Generates a file with extension .dot for the tools "
                "of the GraphViz package.</p>");
    action->setWhatsThis( hint );


    _taDump = actionCollection()->add<KToggleAction>( QStringLiteral("dump") );
    _taDump->setIcon( QIcon::fromTheme(QStringLiteral("edit-redo")) );
    _taDump->setText( i18n( "&Force Dump" ) );
    connect(_taDump, &QAction::triggered, this, &TopLevel::forceTrace);
    actionCollection()->setDefaultShortcut(_taDump, QKeySequence::Undo);
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

    action = KStandardAction::open(this, SLOT(load()), actionCollection());
    hint = i18n("<b>Open Profile Data</b>"
                "<p>This opens a profile data file, with possible multiple parts</p>");
    action->setToolTip( hint );
    action->setWhatsThis( hint );

    _openRecent = KStandardAction::openRecent(this, SLOT(load(QUrl)),
                                              actionCollection());

    KStandardAction::showStatusbar(this,
                                   SLOT(toggleStatusBar()), actionCollection());

    _partDockShown = actionCollection()->add<KToggleAction>(QStringLiteral("settings_show_partdock"));
    _partDockShown->setText(i18n("Parts Overview"));
    connect(_partDockShown, &QAction::triggered, this, &TopLevel::togglePartDock);

    hint = i18n("Show/Hide the Parts Overview Dockable");
    _partDockShown->setToolTip( hint );
    _partDockShown->setWhatsThis( hint );

    _stackDockShown = actionCollection()->add<KToggleAction>(QStringLiteral("settings_show_stackdock"));
    _stackDockShown->setText(i18n("Call Stack"));
    connect(_stackDockShown, &QAction::triggered, this, &TopLevel::toggleStackDock);

    hint = i18n("Show/Hide the Call Stack Dockable");
    _stackDockShown->setToolTip( hint );
    _stackDockShown->setWhatsThis( hint );

    _functionDockShown = actionCollection()->add<KToggleAction>(QStringLiteral("settings_show_profiledock"));
    _functionDockShown->setText(i18n("Function Profile"));
    connect(_functionDockShown, &QAction::triggered, this, &TopLevel::toggleFunctionDock);

    hint = i18n("Show/Hide the Function Profile Dockable");
    _functionDockShown->setToolTip( hint );
    _functionDockShown->setWhatsThis( hint );

#if ENABLE_DUMPDOCK
    _dumpDockShown = actionCollection()->add<KToggleAction>("settings_show_dumpdock",
                                                            this, SLOT(toggleDumpDock()));
    _dumpDockShown->setText(i18n("Profile Dumps"));
    hint = i18n("Show/Hide the Profile Dumps Dockable");
    _dumpDockShown->setToolTip( hint );
    _dumpDockShown->setWhatsThis( hint );
#endif

    _taPercentage = actionCollection()->add<KToggleAction>(QStringLiteral("view_percentage"));
    _taPercentage->setIcon(QIcon::fromTheme(QStringLiteral("percent")));
    _taPercentage->setText(i18n("Relative"));
    connect(_taPercentage, &QAction::triggered, this, &TopLevel::togglePercentage);
    hint = i18n("Show relative instead of absolute costs");
    _taPercentage->setToolTip( hint );
    _taPercentage->setWhatsThis( hint );

    _taExpanded = actionCollection()->add<KToggleAction>(QStringLiteral("view_expanded"));
    _taExpanded->setIcon(QIcon::fromTheme(QStringLiteral("move")));
    _taExpanded->setText(i18n("Relative to Parent"));
    connect(_taExpanded, &QAction::triggered, this, &TopLevel::toggleExpanded);

    hint = i18n("Show percentage costs relative to parent");
    _taExpanded->setToolTip( hint );
    _taExpanded->setWhatsThis( hint );

    hint = i18n("<b>Show percentage costs relative to parent</b>"
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

    _taCycles = actionCollection()->add<KToggleAction>(QStringLiteral("view_cycles"));
    _taCycles->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    _taCycles->setText(i18n( "Cycle Detection" ));
    connect(_taCycles, &QAction::triggered, this, &TopLevel::toggleCycles);

    hint = i18n("<b>Detect recursive cycles</b>"
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

    _taHideTemplates = actionCollection()->add<KToggleAction>(QStringLiteral("hide_templates"));
    _taHideTemplates->setIcon(QIcon::fromTheme(QStringLiteral("hidetemplates")));
    _taHideTemplates->setText(i18n( "Shorten Templates" ));
    connect(_taHideTemplates, &QAction::triggered, this, &TopLevel::toggleHideTemplates);
    _taHideTemplates->setToolTip(i18n( "Hide Template Parameters in C++ Symbols" ));
    hint = i18n("<b>Hide Template Parameters in C++ Symbols</b>"
                "<p>If this is switched on, every symbol displayed will have "
                "any C++ template parameters hidden, just showing &lt;&gt; "
                "instead of a potentially nested template parameter.</p>"
                "<p>In this mode, you can hover the mouse pointer over the "
                "activated symbol label to show a tooltip with the "
                "unabbreviated symbol.</p>");
    _taHideTemplates->setWhatsThis(hint);

    KStandardAction::quit(this, SLOT(close()), actionCollection());
    KStandardAction::preferences(this, SLOT(configure()), actionCollection());
    KStandardAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
    KStandardAction::configureToolbars(this,SLOT(configureToolbars()),
                                       actionCollection());

    _paUp = new KToolBarPopupAction( QIcon::fromTheme( QStringLiteral("go-up") ),
                                     i18n( "&Up" ), this );
    connect( _paUp, &QAction::triggered,
             _stackSelection, &StackSelection::browserUp );
    actionCollection()->addAction( QStringLiteral("go_up"), _paUp );
    actionCollection()->setDefaultShortcut(_paUp, QKeySequence(Qt::ALT + Qt::Key_Up));
    connect( _paUp->menu(), &QMenu::aboutToShow,
             this, &TopLevel::upAboutToShow );
    connect( _paUp->menu(), &QMenu::triggered,
             this, &TopLevel::upTriggered );
    hint = i18n("<b>Go Up</b>"
                "<p>Go to last selected caller of current function. "
                "If no caller was visited, use that with highest cost.</p>");
    _paUp->setToolTip( hint );
    _paUp->setWhatsThis( hint );

    QPair< KGuiItem, KGuiItem > backForward = KStandardGuiItem::backAndForward();
    _paBack = new KToolBarPopupAction( backForward.first.icon(),
                                       backForward.first.text(), this );
    connect( _paBack, &QAction::triggered,
             _stackSelection, &StackSelection::browserBack );
    actionCollection()->addAction( QStringLiteral("go_back"), _paBack );
    actionCollection()->setDefaultShortcut(_paBack, QKeySequence(Qt::ALT + Qt::Key_Left));
    connect( _paBack->menu(), &QMenu::aboutToShow,
             this, &TopLevel::backAboutToShow );
    connect( _paBack->menu(), &QMenu::triggered,
             this, &TopLevel::backTriggered );
    hint = i18n("Go back in function selection history");
    _paBack->setToolTip( hint );
    _paBack->setWhatsThis( hint );

    _paForward = new KToolBarPopupAction( backForward.second.icon(),
                                          backForward.second.text(), this );
    connect( _paForward, &QAction::triggered,
             _stackSelection, &StackSelection::browserForward );
    actionCollection()->addAction( QStringLiteral("go_forward"), _paForward );
    actionCollection()->setDefaultShortcut(_paForward, QKeySequence(Qt::ALT + Qt::Key_Right));
    connect( _paForward->menu(), &QMenu::aboutToShow,
             this, &TopLevel::forwardAboutToShow );
    connect( _paForward->menu(), &QMenu::triggered,
             this, &TopLevel::forwardTriggered );
    hint = i18n("Go forward in function selection history");
    _paForward->setToolTip( hint );
    _paForward->setWhatsThis( hint );

    _saCost = actionCollection()->add<KSelectAction>(QStringLiteral("view_cost_type"));
    _saCost->setText(i18n("Primary Event Type"));
    hint = i18n("Select primary event type of costs");
    _saCost->setComboWidth(300);
    _saCost->setToolTip( hint );
    _saCost->setWhatsThis( hint );

    // This is needed because for KDE4, "_saCost->setComboWidth(300);" seems to
    // have no effect. Instead, list box entry widths are used to determine the
    // combobox width. However, at KCachegrind startup, we do not have yet
    // a list of event types, as this depends on the profile data.
    // In KDE 4.2, we used a translatable string, which did not really work as
    // the semantic is not known to translators. Instead, we use a
    // nontranslatable string now...
    QStringList dummyItems;
    dummyItems << QStringLiteral("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    _saCost->setItems(dummyItems);

    // cost types are dependent on loaded data, thus KSelectAction
    // is filled in setData()
    connect( _saCost, SIGNAL(triggered(QString)),
             this, SLOT(eventTypeSelected(QString)));

    _saCost2 = actionCollection()->add<KSelectAction>(QStringLiteral("view_cost_type2"));
    _saCost2->setText(i18n("Secondary Event Type"));
    hint = i18n("Select secondary event type for cost e.g. shown in annotations");
    _saCost2->setComboWidth(300);
    _saCost2->setToolTip( hint );
    _saCost2->setWhatsThis( hint );
    _saCost2->setItems(dummyItems);

    connect( _saCost2, SIGNAL(triggered(QString)),
             this, SLOT(eventType2Selected(QString)));

    saGroup = actionCollection()->add<KSelectAction>(QStringLiteral("view_group_type"));
    saGroup->setText(i18n("Grouping"));

    hint = i18n("Select how functions are grouped into higher level cost items");
    saGroup->setToolTip( hint );
    saGroup->setWhatsThis( hint );

    QStringList args;

    args << i18n("(No Grouping)")
         << ProfileContext::i18nTypeName(ProfileContext::Object)
         << ProfileContext::i18nTypeName(ProfileContext::File)
         << ProfileContext::i18nTypeName(ProfileContext::Class)
         << ProfileContext::i18nTypeName(ProfileContext::FunctionCycle);

    saGroup->setItems(args);
    connect( saGroup, SIGNAL(triggered(int)),
             this, SLOT(groupTypeSelected(int)));

    _taSplit = actionCollection()->add<KToggleAction>(QStringLiteral("view_split"));
    _taSplit->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    _taSplit->setText(i18n("Split"));
    connect(_taSplit, &QAction::triggered, this, &TopLevel::splitSlot);

    hint = i18n("Show two information panels");
    _taSplit->setToolTip( hint );
    _taSplit->setWhatsThis( hint );

    _taSplitDir = actionCollection()->add<KToggleAction>(QStringLiteral("view_split_dir"));
    _taSplitDir->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    _taSplitDir->setText(i18n("Split Horizontal"));
    connect(_taSplitDir, &QAction::triggered, this, &TopLevel::splitDirSlot);

    hint = i18n("Change Split Orientation when main window is split.");
    _taSplitDir->setToolTip( hint );
    _taSplitDir->setWhatsThis( hint );

    // copied from KMail...
    KStandardAction::tipOfDay( this, SLOT(slotShowTip()), actionCollection() );
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

void TopLevel::updateViewsOnChange(int change)
{
    _partSelection->notifyChange(change);
    _functionSelection->notifyChange(change);
    _multiView->notifyChange(change);
}

void TopLevel::setPercentage(bool show)
{
    if (_showPercentage == show) return;
    _showPercentage = show;
    if (_taPercentage->isChecked() != show)
        _taPercentage->setChecked(show);

    GlobalConfig::setShowPercentage(_showPercentage);

    _stackSelection->refresh();

    updateViewsOnChange(TraceItemView::configChanged);
}

void TopLevel::toggleExpanded()
{
    bool show = _taExpanded->isChecked();
    if (_showExpanded == show) return;
    _showExpanded = show;

    GlobalConfig::setShowExpanded(_showExpanded);

    _stackSelection->refresh();

    updateViewsOnChange(TraceItemView::configChanged);
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

    _stackSelection->rebuildStackList();

    updateViewsOnChange(TraceItemView::configChanged);
}

void TopLevel::toggleHideTemplates()
{
    bool b = _taHideTemplates->isChecked();
    if (_hideTemplates == b) return;
    _hideTemplates = b;

    GlobalConfig::setHideTemplates(_hideTemplates);

    _stackSelection->refresh();

    updateViewsOnChange(TraceItemView::configChanged);
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
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}


void TopLevel::configureToolbars()
{
    KEditToolBar *dlg = new KEditToolBar(guiFactory(), this);

    if (dlg->exec())
        createGUI();

    delete dlg;
}


void TopLevel::newWindow()
{
    TopLevel* t = new TopLevel();
    t->show();
}


void TopLevel::load()
{
    QUrl url = QFileDialog::getOpenFileUrl(this,
                                           i18n("Select Callgrind Profile Data"),
                                           QUrl(),
                                           i18n("Callgrind Profile Data (cachegrind.out* callgrind.out*);;All Files (*)"));

    load(url);
}

void TopLevel::load(const QUrl& url)
{
    if (url.isEmpty()) return;

    QString tmpFileName;
    QTemporaryFile tmpFile;
    if (url.isLocalFile()) {
        tmpFileName = url.toLocalFile();
    }
    else if (tmpFile.open()){
        // network transparency
        tmpFileName = tmpFile.fileName();
        KIO::FileCopyJob *job = KIO::file_copy(url,
                                               QUrl::fromLocalFile(tmpFileName));
        KJobWidgets::setWindow(job, this);
        job->exec();
    }
    if (!tmpFileName.isEmpty()) {
        _openRecent->addUrl(url);
        _openRecent->saveEntries( KConfigGroup( KSharedConfig::openConfig(), QString() ) );

        load(tmpFileName);
    } else {
        KMessageBox::error(this, i18n("Could not open the file \"%1\". "
                                      "Check it exists and you have enough "
                                      "permissions to read it.",
                                      url.toDisplayString()));
    }
}

/* if file name is ".": load first file found in current directory, but do
 * not show an error message if nothing could be loaded
 */
void TopLevel::load(QString file)
{
    if (file.isEmpty()) return;

    bool showError = true;
    if (file == QStringLiteral("."))
        showError = false;

    if (_data && _data->parts().count()>0) {

        // In new window
        TopLevel* t = new TopLevel();
        t->show();
        t->loadDelayed(file);
        return;
    }

    bool loaded = openDataFile(file);
    if (!loaded && showError)
        KMessageBox::error(this, i18n("Could not open the file \"%1\". "
                                      "Check it exists and you have enough "
                                      "permissions to read it.", file));
}


void TopLevel::add()
{
    QUrl url = QFileDialog::getOpenFileUrl(this,
                                           i18n("Add Callgrind Profile Data"),
                                           QUrl(),
                                           i18n("Callgrind Profile Data (cachegrind.out* callgrind.out*);;All Files (*)"));

    add(url);
}

void TopLevel::add(const QUrl &url)
{
    if (url.isEmpty()) return;

    QString tmpFileName;
    QTemporaryFile tmpFile;
    if (url.isLocalFile()) {
        tmpFileName = url.toLocalFile();
    }
    else if (tmpFile.open()){
        // network transparency
        tmpFileName = tmpFile.fileName();
        KIO::FileCopyJob *job = KIO::file_copy(url,
                                               QUrl::fromLocalFile(tmpFileName));
        KJobWidgets::setWindow(job, this);
        job->exec();
    }
    if (!tmpFileName.isEmpty()) {
        _openRecent->addUrl(url);
        _openRecent->saveEntries( KSharedConfig::openConfig()->group( QString() ) );

        add(tmpFileName);
    }
}

void TopLevel::add(QString file)
{
    if (file.isEmpty()) return;

    if (_data) {
        _data->load(file);

        // GUI update for added data
        configChanged();
        return;
    }

    openDataFile(file);
}



void TopLevel::loadDelayed(QString file)
{
    _loadFilesDelayed << file;
    QTimer::singleShot(0, this, &TopLevel::loadTraceDelayed);
}

void TopLevel::loadDelayed(QStringList files)
{
    _loadFilesDelayed << files;
    QTimer::singleShot(0, this, &TopLevel::loadTraceDelayed);
}

void TopLevel::loadTraceDelayed()
{
    if (_loadFilesDelayed.isEmpty()) return;

    if (_loadFilesDelayed.count()>1) {
        // FIXME: we expect all files to be local and existing
        TraceData* d = new TraceData(this);
        d->load(_loadFilesDelayed);
        setData(d);
    }
    else {
        QString file = _loadFilesDelayed[0];

        // if URL scheme is missing (URL is relative), this is a local file
        QUrl u = QUrl::fromUserInput(file, QString(), QUrl::AssumeLocalFile);
        if (u.isLocalFile())
            load(file); // special case for local file: maybe just prefix
        else
            load(u);
    }
    _loadFilesDelayed.clear();
}


void TopLevel::reload()
{
    QString trace;
    if (!_data || _data->parts().count()==0)
        trace = QStringLiteral("."); // open first trace found in dir
    else
        trace = _data->traceName();

    // this also keeps sure we have the same browsing position...
    openDataFile(trace);
}

void TopLevel::exportGraph()
{
    if (!_data || !_function) return;

    QString n = QStringLiteral("callgraph.dot");
    GraphExporter ge(_data, _function, _eventType, _groupType, n);
    ge.writeDot();

#ifdef Q_OS_UNIX
    // shell commands only work in UNIX
    QString cmd = QStringLiteral("(dot %1 -Tps > %2.ps; xdg-open %3.ps)&")
                  .arg(n).arg(n).arg(n);
    if (::system(QFile::encodeName( cmd ))<0)
        qDebug() << "TopLevel::exportGraph: can not run " << cmd;
#endif
}


bool TopLevel::setEventType(QString s)
{
    EventType* ct;

    ct = (_data) ? _data->eventTypes()->type(s) : nullptr;

    // if costtype with given name not found, use first available
    if (!ct && _data) ct = _data->eventTypes()->type(0);

    return setEventType(ct);
}

bool TopLevel::setEventType2(QString s)
{
    EventType* ct;

    // Special type i18n("(Hidden)") gives 0
    ct = (_data) ? _data->eventTypes()->type(s) : nullptr;

    return setEventType2(ct);
}

void TopLevel::eventTypeSelected(const QString& s)
{
    EventType* ct;

    ct = (_data) ? _data->eventTypes()->typeForLong(s) : nullptr;
    setEventType(ct);
}

void TopLevel::eventType2Selected(const QString& s)
{
    EventType* ct;

    ct = (_data) ? _data->eventTypes()->typeForLong(s) : nullptr;
    setEventType2(ct);
}

bool TopLevel::setEventType(EventType* ct)
{
    if (_eventType == ct) return false;
    _eventType = ct;

    if (ct) {
        QStringList l = _saCost->items();
        int idx = l.indexOf(ct->longName());
        if (idx >= 0)
            _saCost->setCurrentItem(idx);
    }

    _partSelection->setEventType(_eventType);
    _stackSelection->setEventType(_eventType);
    _functionSelection->setEventType(_eventType);
    _multiView->setEventType(_eventType);

    updateStatusBar();

    return true;
}

bool TopLevel::setEventType2(EventType* ct)
{
    if (_eventType2 == ct) return false;
    _eventType2 = ct;

    QString longName = ct ? ct->longName() : i18n("(Hidden)");
    QStringList l = _saCost2->items();
    int idx = l.indexOf(longName);
    if (idx >= 0)
        _saCost2->setCurrentItem(idx);

    _partSelection->setEventType2(_eventType2);
    _stackSelection->setEventType2(_eventType2);
    _functionSelection->setEventType2(_eventType2);
    _multiView->setEventType2(_eventType2);

    updateStatusBar();

    return true;
}


void TopLevel::groupTypeSelected(int cg)
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

bool TopLevel::setGroupType(QString s)
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

bool TopLevel::setGroupType(ProfileContext::Type gt)
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

    if (saGroup->currentItem() != idx)
        saGroup->setCurrentItem(idx);

    _stackSelection->setGroupType(_groupType);
    _partSelection->set(_groupType);
    _functionSelection->set(_groupType);
    _multiView->set(_groupType);

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
    if (_group == g) return false;
    _group = g;

    _functionSelection->setGroup(g);
    updateStatusBar();

    return true;
}

bool TopLevel::setFunction(QString s)
{
    if (!_data) return false;

    ProfileCostArray* f = _data->search(ProfileContext::Function, s, _eventType);
    if (!f) return false;

    return setFunction((TraceFunction*)f);
}

bool TopLevel::setFunction(TraceFunction* f)
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
void TopLevel::setEventTypeDelayed(EventType* ct)
{
    _eventTypeDelayed = ct;
    QTimer::singleShot (0, this, SLOT(setEventTypeDelayed()));
}

void TopLevel::setEventType2Delayed(EventType* ct)
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

void TopLevel::setGroupTypeDelayed(ProfileContext::Type gt)
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
        StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
        HistoryItem* hi = b ? b->current() : nullptr;
        TraceFunction* f = hi ? hi->function() : nullptr;

        if (!f) break;
        f = hi->stack()->caller(f, false);
        if (f) setFunction(f);
    }
        break;

    default: break;
    }

    _directionDelayed = TraceItemView::None;
}


void TopLevel::setTraceItemDelayed(CostItem* i)
{
    // no need to select same item a 2nd time...
    if (_traceItemDelayed == i) return;
    _traceItemDelayed = i;
    _lastSender = sender();

    qDebug() << "Selected " << (i ? i->prettyName() : QStringLiteral("(none)"));

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

    _traceItemDelayed = nullptr;
    _lastSender = nullptr;
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

    _lastSender = nullptr;

    saveTraceSettings();

    if (_data) {
        _partSelection->setData(nullptr);
        _stackSelection->setData(nullptr);
        _functionSelection->setData(nullptr);
        _multiView->setData(nullptr);
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
    _multiView->setData(_data);
    // Force update of _data in all children of _multiView
    // This is needed to make restoring of activeItem work!
    _multiView->updateView(true);

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
}

void TopLevel::addEventTypeMenu(QMenu* popup, bool withCost2)
{
    if (_data) {
        QMenu *popup1, *popup2 = nullptr;
        QAction* action;

        popup1 = popup->addMenu(i18n("Primary Event Type"));
        connect(popup1, SIGNAL(triggered(QAction*)),
                this, SLOT(setEventType(QAction*)));

        if (withCost2) {
            popup2 = popup->addMenu(i18n("Secondary Event Type"));
            connect(popup2, SIGNAL(triggered(QAction*)),
                    this, SLOT(setEventType2(QAction*)));

            if (_eventType2) {
                action = popup2->addAction(i18n("Hide"));
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

    if (_showPercentage)
        popup->addAction(i18n("Show Absolute Cost"),
                         this, SLOT(setAbsoluteCost()));
    else
        popup->addAction(i18n("Show Relative Cost"),
                         this, SLOT(setRelativeCost()));
}

bool TopLevel::setEventType(QAction* action)
{
    if (!_data) return false;
    int id = action->data().toInt(nullptr);

    EventTypeSet* m = _data->eventTypes();
    EventType* ct=nullptr;
    if (id >=100 && id<199) ct = m->realType(id-100);
    if (id >=200 && id<299) ct = m->derivedType(id-200);

    return ct ? setEventType(ct) : false;
}

bool TopLevel::setEventType2(QAction* action)
{
    if (!_data) return false;
    int id = action->data().toInt(nullptr);

    EventTypeSet* m = _data->eventTypes();
    EventType* ct=nullptr;
    if (id >=100 && id<199) ct = m->realType(id-100);
    if (id >=200 && id<299) ct = m->derivedType(id-200);

    return setEventType2(ct);
}

void TopLevel::addGoMenu(QMenu* popup)
{
    popup->addAction(i18n("Go Back"), this, SLOT(goBack()));
    popup->addAction(i18n("Go Forward"), this, SLOT(goForward()));
    popup->addAction(i18n("Go Up"), this, SLOT(goUp()));
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

    return QStringLiteral("-") + key;
}


void TopLevel::restoreTraceTypes()
{
    QString key = traceKey();
    QString groupType, eventType, eventType2;

    ConfigGroup* pConfig = ConfigStorage::group(QStringLiteral("TracePositions"));
    groupType = pConfig->value(QStringLiteral("GroupType%1").arg(key),QString()).toString();
    eventType = pConfig->value(QStringLiteral("EventType%1").arg(key),QString()).toString();
    eventType2 = pConfig->value(QStringLiteral("EventType2%1").arg(key),QString()).toString();
    delete pConfig;

    ConfigGroup* cConfig = ConfigStorage::group(QStringLiteral("CurrentState"));
    if (groupType.isEmpty())
        groupType = cConfig->value(QStringLiteral("GroupType"),QString()).toString();
    if (eventType.isEmpty())
        eventType = cConfig->value(QStringLiteral("EventType"),QString()).toString();
    if (eventType2.isEmpty())
        eventType2 = cConfig->value(QStringLiteral("EventType2"),QString()).toString();
    delete cConfig;

    setGroupType(groupType);
    setEventType(eventType);
    setEventType2(eventType2);

    // if still no cost type set, use first available
    if (!_eventType && !_saCost->items().isEmpty())
        eventTypeSelected(_saCost->items().first());

    ConfigGroup* aConfig = ConfigStorage::group(QStringLiteral("Layouts"));
    _layoutCount = aConfig->value(QStringLiteral("Count%1").arg(key), 0).toInt();
    _layoutCurrent = aConfig->value(QStringLiteral("Current%1").arg(key), 0).toInt();
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

    restoreCurrentState(key);

    ConfigGroup* pConfig = ConfigStorage::group(QStringLiteral("TracePositions"));
    QString group = pConfig->value(QStringLiteral("Group%1").arg(key),QString()).toString();
    delete pConfig;
    if (!group.isEmpty()) setGroup(group);


    // restoreCurrentState() usually leads to a call to setTraceItemDelayed()
    // to restore last active item...
    if (!_traceItemDelayed) {
        // function not available any more.. try with "main"
        if (!setFunction(QStringLiteral("main")))
            _functionSelection->selectTopFunction();
    }
}


/* Layout */

void TopLevel::layoutDuplicate()
{
    // save current and allocate a new slot
    _multiView->saveLayout(QStringLiteral("Layout%1-MainView").arg(_layoutCurrent),
                           traceKey());
    _layoutCurrent = _layoutCount;
    _layoutCount++;

    updateLayoutActions();

    if (0) qDebug() << "TopLevel::layoutDuplicate: count " << _layoutCount;
}

void TopLevel::layoutRemove()
{
    if (_layoutCount <2) return;

    int from = _layoutCount-1;
    if (_layoutCurrent == from) { _layoutCurrent--; from--; }

    // restore from last and decrement count
    _multiView->restoreLayout(QStringLiteral("Layout%1-MainView").arg(from),
                              traceKey());
    _layoutCount--;

    updateLayoutActions();
}

void TopLevel::layoutNext()
{
    if (_layoutCount <2) return;

    QString key = traceKey();
    QString layoutPrefix = QStringLiteral("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);
    _layoutCurrent++;
    if (_layoutCurrent == _layoutCount) _layoutCurrent = 0;
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    if (0) qDebug() << "TopLevel::layoutNext: current "
                    << _layoutCurrent << endl;
}

void TopLevel::layoutPrevious()
{
    if (_layoutCount <2) return;

    QString key = traceKey();
    QString layoutPrefix = QStringLiteral("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);
    _layoutCurrent--;
    if (_layoutCurrent <0) _layoutCurrent = _layoutCount-1;
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    if (0) qDebug() << "TopLevel::layoutPrevious: current "
                    << _layoutCurrent << endl;
}

void TopLevel::layoutSave()
{
    QString key = traceKey();
    QString layoutPrefix = QStringLiteral("Layout%1-MainView");

    _multiView->saveLayout(layoutPrefix.arg(_layoutCurrent), key);

    // save all layouts as defaults (ie. without any group name postfix)
    for(int i=0;i<_layoutCount;i++) {
        _multiView->restoreLayout(layoutPrefix.arg(i), key);
        _multiView->saveLayout(layoutPrefix.arg(i), QString());
    }
    // restore the previously saved current layout
    _multiView->restoreLayout(layoutPrefix.arg(_layoutCurrent), key);

    ConfigGroup* layoutConfig = ConfigStorage::group(QStringLiteral("Layouts"));
    layoutConfig->setValue(QStringLiteral("DefaultCount"), _layoutCount);
    layoutConfig->setValue(QStringLiteral("DefaultCurrent"), _layoutCurrent);
    delete layoutConfig;
}

void TopLevel::layoutRestore()
{
    KConfig *config = KSharedConfig::openConfig().data();
    KConfigGroup aConfig(config, "Layouts");
    _layoutCount = aConfig.readEntry("DefaultCount", 0);
    _layoutCurrent = aConfig.readEntry("DefaultCurrent", 0);
    if (_layoutCount == 0) {
        _layoutCount++;
        return;
    }

    QString layoutPrefix = QStringLiteral("Layout%1-MainView");
    _multiView->restoreLayout( layoutPrefix.arg(_layoutCurrent), traceKey());

    updateLayoutActions();
}


void TopLevel::updateLayoutActions()
{
    QAction* ka;

    ka = actionCollection()->action(QStringLiteral("layout_next"));
    if (ka) ka->setEnabled(_layoutCount>1);

    ka = actionCollection()->action(QStringLiteral("layout_previous"));
    if (ka) ka->setEnabled(_layoutCount>1);

    ka = actionCollection()->action(QStringLiteral("layout_remove"));
    if (ka) ka->setEnabled(_layoutCount>1);

    _statusbar->showMessage(i18n("Layout Count: %1", _layoutCount), 1000);
}


void TopLevel::updateStatusBar()
{
    if (!_data || _data->parts().count()==0) {
        _statusLabel->setText(i18n("No profile data file loaded."));
        return;
    }

    QString status = QStringLiteral("%1 [%2] - ")
                     .arg(_data->shortTraceName())
                     .arg(_data->activePartRange());

    if (_eventType) {
        status += i18n("Total %1 Cost: %2",
                       _eventType->longName(),
                       _data->prettySubCost(_eventType));

        /* this gets too long...
        if (_eventType2 && (_eventType2 != _eventType))
          status += i18n(", %1 Cost: %2")
        .arg(_eventType2->longName())
        .arg(_data->prettySubCost(_eventType2));
        */
    }
    else
        status += i18n("No event type selected");

    /* Not working... should give group of selected function

    if (_groupType != ProfileContext::Function) {
      status += QString(" - %1 '%2'")
                .arg(ProfileContext::i18nTypeName(_groupType))
                .arg(_group ? _group->prettyName() : i18n("(None)"));
    }
    */

    _statusLabel->setText(status);
}

void TopLevel::configure()
{
    if (ConfigDlg::configure( GlobalGUIConfig::config(),
                              _data, this)) {
        GlobalGUIConfig::config()->saveOptions();

        configChanged();
    }
    else
        GlobalGUIConfig::config()->readOptions();
}

bool TopLevel::queryClose()
{
    saveTraceSettings();

    // save current toplevel options as defaults...
    GlobalConfig::setShowPercentage(_showPercentage);
    GlobalConfig::setShowExpanded(_showExpanded);
    GlobalConfig::setShowCycles(_showCycles);
    GlobalConfig::setHideTemplates(_hideTemplates);
    GlobalGUIConfig::config()->saveOptions();

    saveCurrentState(QString());

    // toolbar and dock positions are automatically stored

    // if part dock was chosen visible even for only 1 part loaded,
    // keep this choice...
    _forcePartDock = false;
    if (_data && (_data->parts().count()<2) && _partDock->isVisible())
        _forcePartDock=true;
    KConfigGroup dockConfig(KSharedConfig::openConfig(), "Docks");
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
    // Invalidate found/cached dirs of source files if we have TraceData loaded.
    if (_data) {
        _data->resetSourceDirs();
    }

    _stackSelection->refresh();

    updateViewsOnChange(TraceItemView::configChanged);
}

void TopLevel::slotShowTipOnStart() {
    KTipDialog::showTip(this);
}

void TopLevel::slotShowTip() {
    KTipDialog::showTip( this, QString(), true );
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
    _multiView->set(list);
    _functionSelection->set(list);
    _stackSelection->refresh();

    updateStatusBar();
}

void TopLevel::partsHideSelectedSlotDelayed()
{
    QTimer::singleShot( 0, this, &TopLevel::partsHideSelectedSlot );
}

// this puts selected parts into hidden list,
// deselects them and makes the remaining parts selected
void TopLevel::partsHideSelectedSlot()
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

void TopLevel::partsUnhideAllSlotDelayed()
{
    QTimer::singleShot( 0, this, &TopLevel::partsUnhideAllSlot );
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
    if (_ccProcess) {
        // callgrind_control still running, cancel request
        qDebug("TopLevel::forceTrace: killing old callgrind_control");
        _ccProcess->kill();
        delete _ccProcess;
        _ccProcess = nullptr;
        _ccOutput = QString();
    }
    if (!_taDump->isChecked()) return;

    // get PID of first loaded part
    int pid = 0;
    TracePart* p = nullptr;
    TracePartList pl;
    if (_data) pl = _data->parts();
    if (!pl.isEmpty()) p = pl.first();
    if (p) pid = p->processID();
    if (pid == 0) {
        showMessage(i18n("Cannot determine receiver PID for dump request"),
                    5000);
        _taDump->setChecked(false);
        return;
    }

    qDebug("TopLevel::forceTrace: run 'callgrind_control -d %d'", pid);

    _ccProcess = new QProcess(this);
    connect(_ccProcess, &QProcess::readyReadStandardOutput,
            this, &TopLevel::ccReadOutput);
    connect(_ccProcess, SIGNAL(error(QProcess::ProcessError)),
            SLOT(ccError(QProcess::ProcessError)));
    connect(_ccProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            SLOT(ccExit(int,QProcess::ExitStatus)));
    _ccProcess->start(QStringLiteral("callgrind_control -d %1").arg(pid),
                      QIODevice::ReadOnly);
}

void TopLevel::ccReadOutput()
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("TopLevel::ccReadOutput: QProcess %p", p);

    // signal from old/uninteresting process?
    if (!_ccProcess) return;
    if (p != _ccProcess) return;

    _ccOutput.append(QString::fromLocal8Bit(_ccProcess->readAllStandardOutput()));
}

void TopLevel::ccError(QProcess::ProcessError e)
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("TopLevel::ccError: Got %d from QProcess %p",
           e, p);

    // signal from old/uninteresting process?
    if (!_ccProcess) return;
    if (p != _ccProcess) return;

    showMessage(i18n("Error running callgrind_control"), 5000);

    _ccProcess->deleteLater();
    _ccProcess = nullptr;
}

void TopLevel::ccExit(int exitCode, QProcess::ExitStatus s)
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("TopLevel::ccExit: QProcess %p, exitCode %d",
           p, exitCode);

    // signal from old/uninteresting process?
    if (!_ccProcess) return;
    if (p != _ccProcess) return;
    _ccProcess->deleteLater();
    _ccProcess = nullptr;
    _taDump->setChecked(false);

    // if not successful no need to reload
    if ((s == QProcess::CrashExit) || (exitCode != 0))
        return;

    // FIXME: Are we sure that file is completely
    //        dumped after waiting one second?
    QTimer::singleShot( 1000, this, &TopLevel::reload );
}

void TopLevel::forwardAboutToShow()
{
    QMenu *popup = _paForward->menu();

    popup->clear();
    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    HistoryItem* hi = b ? b->current() : nullptr;
    TraceFunction* f;
    QAction* action;

    if (!hi) {
        popup->addAction(i18n("(No Stack)"));
        return;
    }

    hi = hi->next();
    if (!hi) {
        popup->addAction(i18n("(No next function)"));
        return;
    }

    int count = 1;
    while (count<GlobalConfig::maxSymbolCount() && hi) {
        f = hi->function();
        if (!f) break;

        QString name = GlobalConfig::shortenSymbol(f->prettyName());

        //qDebug("forward: Adding %s", name.ascii());
        action = popup->addAction(name);
        action->setData(count);

        hi = hi->next();
        count++;
    }
}

void TopLevel::backAboutToShow()
{
    QMenu *popup = _paBack->menu();

    popup->clear();
    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    HistoryItem* hi = b ? b->current() : nullptr;
    TraceFunction* f;
    QAction* action;

    if (!hi) {
        popup->addAction(i18n("(No Stack)"));
        return;
    }

    hi = hi->last();
    if (!hi) {
        popup->addAction(i18n("(No previous function)"));
        return;
    }

    int count = 1;
    while (count<GlobalConfig::maxSymbolCount() && hi) {
        f = hi->function();
        if (!f) break;

        QString name = GlobalConfig::shortenSymbol(f->prettyName());

        //qDebug("back: Adding %s", name.ascii());
        action = popup->addAction(name);
        action->setData(count);

        hi = hi->last();
        count++;
    }
}

void TopLevel::upAboutToShow()
{
    QMenu *popup = _paUp->menu();

    popup->clear();
    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    HistoryItem* hi = b ? b->current() : nullptr;
    TraceFunction* f = hi ? hi->function() : nullptr;
    QAction* action;

    if (!f) {
        popup->addAction(i18n("(No Stack)"));
        return;
    }
    f = hi->stack()->caller(f, false);
    if (!f) {
        popup->addAction(i18n("(No Function Up)"));
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

void TopLevel::forwardTriggered(QAction* action)
{
    int count = action->data().toInt(nullptr);
    //qDebug("forwardTriggered: %d", count);
    if( count <= 0)
        return;

    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    if (!b) return;

    while (count>1) {
        b->goForward();
        count--;
    }
    _stackSelection->browserForward();
}

void TopLevel::backTriggered(QAction* action)
{
    int count = action->data().toInt(nullptr);
    //qDebug("backTriggered: %d", count);
    if( count <= 0)
        return;

    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    if (!b) return;

    while (count>1) {
        b->goBack();
        count--;
    }
    _stackSelection->browserBack();
}

void TopLevel::upTriggered(QAction* action)
{
    int count = action->data().toInt(nullptr);
    //qDebug("upTriggered: %d", count);
    if( count <= 0)
        return;

    StackBrowser* b = _stackSelection ? _stackSelection->browser() : nullptr;
    HistoryItem* hi = b ? b->current() : nullptr;
    if (!hi) return;

    TraceFunction* f = hi->function();

    while (count>0 && f) {
        f = hi->stack()->caller(f, false);
        count--;
    }

    //qDebug("upActivated: %s", f ? f->prettyName().ascii() : "??" );
    if (f)
        setFunction(f);
}

void TopLevel::showMessage(const QString& msg, int ms)
{
    if (_statusbar)
        _statusbar->showMessage(msg, ms);
}

void TopLevel::showStatus(const QString& msg, int progress)
{
    static bool msgUpdateNeeded = true;

    if (!_statusbar) return;

    if (msg.isEmpty()) {
        //reset status
        if (_progressBar) {
            _statusbar->removeWidget(_progressBar);
            delete _progressBar;
            _progressBar = nullptr;
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

void TopLevel::loadStart(const QString& filename)
{
    showStatus(i18n("Loading %1", filename), 0);
    Logger::_filename = filename;
}

void TopLevel::loadFinished(const QString& msg)
{
    showStatus(QString(), 0);
    if (!msg.isEmpty())
        showMessage(i18n("Error loading %1: %2", _filename, msg),
                    2000);
}

void TopLevel::loadProgress(int progress)
{
    showStatus(i18n("Loading %1", _filename), progress);
}

void TopLevel::loadError(int line, const QString& msg)
{
    qCritical() << "Loading" << _filename << ":" << line << ": " << msg;
}

void TopLevel::loadWarning(int line, const QString& msg)
{
    qWarning() << "Loading" << _filename << ":" << line << ": " << msg;
}

bool TopLevel::openDataFile(const QString& file)
{
    TraceData* d = new TraceData(this);
    int filesLoaded;

    // see whether this file is compressed, than take the direct route
    QMimeDatabase dataBase;
    QString mimeType = dataBase.mimeTypeForFile(file, QMimeDatabase::MatchContent).name();

    KCompressionDevice* compressed;
    compressed = new KCompressionDevice(file,
                                        KFilterDev::compressionTypeForMimeType(mimeType));
    if (compressed &&
        (compressed->compressionType() != KCompressionDevice::None)) {
        filesLoaded = d->load(compressed, file);
    } else {
        // else fallback to string based method that can also find multi-part callgrind data.
        filesLoaded = d->load(file);
    }
    if (filesLoaded > 0) {
        setData(d);
        return true;
    } else {
        return false;
    }
}


