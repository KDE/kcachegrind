/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * QCachegrind top level window
 */

#ifndef QCGTOPLEVEL_H
#define QCGTOPLEVEL_H

#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QStringList>
#include <QString>

#include "logger.h"
#include "traceitemview.h"
#include "tracedata.h"
#include "toplevelbase.h"

class MultiView;
class QDockWidget;
class QLabel;
class QComboBox;
class QProgressBar;
class QMenu;

class TraceData;
class MainWidget;
class PartSelection;
class FunctionSelection;
class StackSelection;
class TraceFunction;

class QCGTopLevel : public QMainWindow, public Logger, public TopLevelBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.qcachegrind")

public:
    QCGTopLevel();
    ~QCGTopLevel() override;

    TraceData* data() { return _data; }
    void setData(TraceData*);

    void createActions();
    void createDocks();
    void createMenu();
    void createToolbar();

    void closeEvent(QCloseEvent*) override;

    ProfileContext::Type groupType() { return _groupType; }
    EventType* eventType() { return _eventType; }
    EventType* eventType2() { return _eventType2; }
    TracePartList activeParts() { return _activeParts; }
    TracePartList hiddenParts() override { return _hiddenParts; }

    /* convenience functions for often used context menu items */
    void addEventTypeMenu(QMenu*,bool) override;
    void addGoMenu(QMenu*) override;

    // Logger overwrites: notifications for file loading
    void loadStart(const QString& filename) override;
    void loadProgress(int progress) override; // 0 - 100
    void loadWarning(int line, const QString& msg) override;
    void loadError(int line, const QString& msg) override;
    void loadFinished(const QString& msg) override; // msg could be error

public Q_SLOTS:
    void load();
    void load(QStringList files, bool addToRecentFiles = true);
    void add();
    void add(QStringList files);

    // shows the main window before loading to see loading progress
    void loadDelayed(QString file, bool addToRecentFiles = true);
    void loadDelayed(QStringList files, bool addToRecentFiles = true);

    void exportGraph();
    void newWindow();
    void configure(QString page = QString());
    void about();

    // layouts
    void layoutDuplicate();
    void layoutRemove();
    void layoutNext();
    void layoutPrevious();
    void layoutSave();
    void layoutRestore();
    void updateLayoutActions();

    void updateStatusBar();
    void eventTypeSelected(const QString&);
    void eventType2Selected(const QString&);
    void groupTypeSelected(int);
    void toggleSplitted();
    void toggleSplitDirection();
    void togglePartDock();
    void toggleStackDock();
    void toggleFunctionDock();
    void functionVisibilityChanged(bool);
    void togglePercentage();
    void setPercentage(bool);
    void setAbsoluteCost();
    void setRelativeCost();
    void toggleHideTemplates();
    void toggleExpanded();
    void toggleCycles();
    void recentFilesMenuAboutToShow();
    void recentFilesTriggered(QAction*);
    void sidebarMenuAboutToShow();
    void forwardAboutToShow();
    void forwardTriggered(QAction*);
    void backAboutToShow();
    void backTriggered(QAction*);
    void upAboutToShow();
    void upTriggered(QAction*);
    void primaryAboutToShow();
    void secondaryAboutToShow();
    void groupingAboutToShow();
    void windowListTriggered(QAction*);
    void windowListAboutToShow();
    void macDockMenuAboutToShow();
    void insertWindowList(QMenu*);
    void reinstallMacDock();

    void setEventType(EventType*);
    void setEventType2(EventType*);
    void setEventType(QString);
    void setEventType2(QString);
    void setEventType(QAction*);
    void setEventType2(QAction*);
    bool setGroupType(ProfileContext::Type);
    bool setGroupType(QString);
    bool setGroup(TraceCostItem*);
    bool setGroup(QString);
    bool setFunction(TraceFunction*);
    bool setFunction(QString);
    void activePartsChangedSlot(const TracePartList& list) override;
    void partsHideSelectedSlot();
    void partsUnhideAllSlot();

    /* These go back to mainloop first by using a timer.
   * So they can be called from event handlers that
   * are not allowed to delete list entries.
   */
    void setEventTypeDelayed(EventType*) override;
    void setEventType2Delayed(EventType*) override;
    void setGroupTypeDelayed(ProfileContext::Type) override;
    void setGroupDelayed(TraceCostItem*) override;
    void setTraceItemDelayed(CostItem*) override;
    void partsHideSelectedSlotDelayed();
    void partsUnhideAllSlotDelayed();
    void goBack();
    void goForward();
    void goUp();
    void setDirectionDelayed(TraceItemView::Direction) override;

    /* SingleShot Slots (without parameters) for the delayed versions */
    void setEventTypeDelayed();
    void setEventType2Delayed();
    void setGroupTypeDelayed();
    void setGroupDelayed();
    void setTraceItemDelayed();
    void loadFilesDelayed();
    void setDirectionDelayed();

    // configuration has changed
    void configChanged() override;

    //void refresh();

    // progress in status bar, empty message disables progress display
    void showStatus(const QString& msg, int progress);
    void showMessage(const QString&, int msec) override;

private:
    void resetState();
    void createLayoutActions();
    void createMiscActions();
    void setupMainWidget(MainWidget*);
    void setupPartSelection(PartSelection*);
    void updateEventTypeMenu(QMenu* m, bool secondary);
    void restoreCurrentState(const QString& postfix);
    void saveCurrentState(const QString& postfix);
    void saveTraceSettings();
    QString traceKey();
    void restoreTraceTypes();
    void restoreTraceSettings();

    QStatusBar* _statusbar;
    QLabel* _statusLabel;
    QString _progressMsg;
    QElapsedTimer _progressStart;
    QProgressBar* _progressBar;

    MultiView* _multiView;
    Qt::Orientation _spOrientation;
    bool _twoMainWidgets;
    FunctionSelection* _functionSelection;
    PartSelection* _partSelection;
    StackSelection* _stackSelection;
    QDockWidget *_partDock, *_stackDock, *_functionDock;
    bool _forcePartDock;

    // menu/toolbar actions
    QAction *_newAction, *_openAction, *_addAction, *_reloadAction;
    QAction *_exportAction, *_dumpToggleAction, *_exitAction;
    QAction *_sidebarMenuAction, *_recentFilesMenuAction;
    QAction *_cyclesToggleAction, *_percentageToggleAction;
    QAction *_expandedToggleAction, *_hideTemplatesToggleAction;
    QAction *_primaryMenuAction, *_secondaryMenuAction;
    QAction *_groupingMenuAction, *_closeAction;
    QAction *_splittedToggleAction, *_splitDirectionToggleAction;
    QAction *_layoutNext, *_layoutPrev, *_layoutRemove, *_layoutDup;
    QAction *_layoutRestore, *_layoutSave, *_toolbarToggleAction;
    QAction *_upAction, *_forwardAction, *_backAction;
    QAction *_configureAction, *_aboutAction, *_aboutQtAction;
    QAction *_minimizeAction, *_zoomAction;
    QMenu *windowMenu, *macDockMenu;
    QToolBar *tb;
    QComboBox* _eventTypeBox;

    TraceFunction* _function;
    const QObject* _lastSender;

    // trace data shown in this window
    TraceData* _data;
    // subcost types used for visualization
    EventType* _eventType;
    EventType* _eventType2;
    QStringList _eventTypes;
    // grouping of function list
    ProfileContext::Type _groupType;
    // selected group
    TraceCostItem* _group;
    // selected parts
    TracePartList _activeParts;
    // hidden parts
    TracePartList _hiddenParts;
    // layouts
    int _layoutCurrent, _layoutCount;
    // remember last file directory for new QFileDialogs
    QString _lastFile;

    // for delayed slots
    EventType* _eventTypeDelayed;
    EventType* _eventType2Delayed;
    ProfileContext::Type _groupTypeDelayed;
    TraceCostItem* _groupDelayed;
    CostItem* _traceItemDelayed;
    QStringList _loadFilesDelayed;
    bool _addToRecentFiles;
    TraceItemView::Direction _directionDelayed;
};

#endif // QCGTOPLEVEL_H
