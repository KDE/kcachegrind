/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * KCachegrind top level window
 */

#ifndef TOPLEVEL_H
#define TOPLEVEL_H

#include <QElapsedTimer>
#include <QLabel>
#include <QProcess>
#include <QUrl>

#include <KXmlGuiWindow>

#include "logger.h"
#include "traceitemview.h"
#include "tracedata.h"
#include "toplevelbase.h"

class MultiView;
class QLineEdit;
class QDockWidget;
class QLabel;
class QProgressBar;
class QMenu;

class QUrl;
class KSelectAction;
class KToggleAction;
class KToolBarPopupAction;
class KStatusBar;

class TraceData;
class KRecentFilesAction;
class MainWidget;
class PartSelection;
class FunctionSelection;
class DumpSelection;
class StackSelection;
class TraceFunction;

class TopLevel : public KXmlGuiWindow, public Logger, public TopLevelBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kcachegrind")
public:
    TopLevel();
    ~TopLevel() override;

    TraceData* data() { return _data; }
    void setData(TraceData*);

    void saveProperties(KConfigGroup &) override;
    void readProperties(const KConfigGroup &) override;

    void createActions();
    void createDocks();

    ProfileContext::Type groupType() { return _groupType; }
    EventType* eventType() { return _eventType; }
    EventType* eventType2() { return _eventType2; }
    TracePartList activeParts() { return _activeParts; }
    TracePartList hiddenParts() override { return _hiddenParts; }

    // current config
    bool showPercentage() const { return _showPercentage; }
    bool showExpanded() const { return _showExpanded; }
    bool showCycles() const { return _showCycles; }

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
    void load(const QUrl&);
    void load(QString);
    void add();
    void add(const QUrl&);
    void add(QString);

    // for quickly showing the main window...
    void loadDelayed(QString);
    void loadDelayed(QStringList);

    void reload();
    void exportGraph();
    void newWindow();
    void configure();
    void querySlot();
    void dummySlot();

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
    void splitSlot();
    void splitDirSlot();
    void configureToolbars() override;
    void configureKeys();
    bool queryClose() override;
    void togglePartDock();
    void toggleStackDock();
    void toggleFunctionDock();
    void toggleDumpDock();
    void toggleStatusBar();
    void partVisibilityChanged(bool);
    void dumpVisibilityChanged(bool);
    void stackVisibilityChanged(bool);
    void functionVisibilityChanged(bool);
    void togglePercentage();
    void setPercentage(bool);
    void setAbsoluteCost();
    void setRelativeCost();
    void toggleExpanded();
    void toggleCycles();
    void toggleHideTemplates();
    void forceTrace();
    void forwardAboutToShow();
    void forwardTriggered(QAction*);
    void backAboutToShow();
    void backTriggered(QAction*);
    void upAboutToShow();
    void upTriggered(QAction*);

    bool setEventType(EventType*);
    bool setEventType2(EventType*);
    bool setEventType(QString);
    bool setEventType2(QString);
    bool setEventType(QAction*);
    bool setEventType2(QAction*);
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
    void loadTraceDelayed();
    void setDirectionDelayed();

    // configuration has changed
    void configChanged() override;

    //void refresh();

    // progress in status bar, empty message disables progress display
    void showStatus(const QString& msg, int progress);
    void showMessage(const QString&, int msec) override;

    // for running callgrind_control in the background
    void ccReadOutput();
    void ccError(QProcess::ProcessError);
    void ccExit(int,QProcess::ExitStatus);

private:
    void resetState();
    void createLayoutActions();
    void createMiscActions();
    void setupMainWidget(MainWidget*);
    void setupPartSelection(PartSelection*);
    void restoreCurrentState(const QString& postfix);
    void saveCurrentState(const QString& postfix);
    void saveTraceSettings();
    QString traceKey();
    void restoreTraceTypes();
    void restoreTraceSettings();
    void updateViewsOnChange(int);
    /// open @p file, might be compressed
    /// @return true when the file could be opened, false otherwise.
    bool openDataFile(const QString& file);

    QStatusBar* _statusbar;
    QLabel* _statusLabel;
    KRecentFilesAction* _openRecent;
    bool _twoMainWidgets;
    Qt::Orientation _spOrientation;

    MultiView* _multiView;
    FunctionSelection* _functionSelection;
    DumpSelection* _dumpSelection;
    PartSelection* _partSelection;
    StackSelection* _stackSelection;
    QLineEdit* queryLineEdit;

    QDockWidget *_partDock, *_stackDock, *_functionDock, *_dumpDock;
    bool _forcePartDock;

    KSelectAction *_saCost, *_saCost2, *saGroup;
    KToggleAction *_partDockShown, *_stackDockShown;
    KToggleAction *_functionDockShown, *_dumpDockShown;
    KToggleAction *_taPercentage, *_taExpanded, *_taCycles, *_taHideTemplates;
    KToggleAction *_taDump, *_taSplit, *_taSplitDir;
    KToolBarPopupAction *_paForward, *_paBack, *_paUp;

    TraceFunction* _function;
    const QObject* _lastSender;

    // trace data shown in this window
    TraceData* _data;
    // subcost types used for visualization
    EventType* _eventType;
    EventType* _eventType2;
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

    // for delayed slots
    EventType* _eventTypeDelayed;
    EventType* _eventType2Delayed;
    ProfileContext::Type _groupTypeDelayed;
    TraceCostItem* _groupDelayed;
    CostItem* _traceItemDelayed;
    QStringList _loadFilesDelayed;
    TraceItemView::Direction _directionDelayed;

    // for status progress display
    QString _progressMsg;
    QElapsedTimer _progressStart;
    QProgressBar* _progressBar;

    // toplevel configuration options
    bool _showPercentage, _showExpanded, _showCycles, _hideTemplates;

    // for running callgrind_control in the background
    QProcess* _ccProcess;
    QString _ccOutput;
};

#endif
