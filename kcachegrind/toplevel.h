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
 * KCachegrind top level window
 */

#ifndef TOPLEVEL_H
#define TOPLEVEL_H

#include <qdatetime.h>
#include <QLabel>
#include <QProcess>

#include <kxmlguiwindow.h>

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

class KUrl;
class KSelectAction;
class KToggleAction;
class KToolBarPopupAction;

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
  ~TopLevel();

  TraceData* data() { return _data; }
  void setData(TraceData*);

  virtual void saveProperties(KConfigGroup &);
  virtual void readProperties(const KConfigGroup &);

  void createActions();
  void createDocks();

  ProfileContext::Type groupType() { return _groupType; }
  EventType* eventType() { return _eventType; }
  EventType* eventType2() { return _eventType2; }
  TracePartList activeParts() { return _activeParts; }
  TracePartList hiddenParts() { return _hiddenParts; }

  // current config
  bool showPercentage() const { return _showPercentage; }
  bool showExpanded() const { return _showExpanded; }
  bool showCycles() const { return _showCycles; }

  /* convenience functions for often used context menu items */
  void addEventTypeMenu(QMenu*,bool);
  void addGoMenu(QMenu*);

  // Logger overwrites: notifications for file loading
  virtual void loadStart(const QString& filename);
  virtual void loadProgress(int progress); // 0 - 100
  virtual void loadWarning(int line, const QString& msg);
  virtual void loadError(int line, const QString& msg);
  virtual void loadFinished(const QString& msg); // msg could be error

public slots:
  void newTrace();
  void loadTrace();
  void loadTrace(const KUrl&);
  void loadTrace(QString);
  void addTrace();
  void addTrace(const KUrl&);
  void addTrace(QString);

  // for quick showing the main window...
  void loadDelayed(QString);

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
  void configureToolbars();
  void configureKeys();
  bool queryExit();
  bool queryClose();
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
  void activePartsChangedSlot(const TracePartList& list);
  void partsHideSelectedSlot();
  void partsUnhideAllSlot();

  /* These go back to mainloop first by using a timer.
   * So they can be called from event handlers that
   * are not allowed to delete list entries.
   */
  void setEventTypeDelayed(EventType*);
  void setEventType2Delayed(EventType*);
  void setGroupTypeDelayed(ProfileContext::Type);
  void setGroupDelayed(TraceCostItem*);
  void setTraceItemDelayed(CostItem*);
  void partsHideSelectedSlotDelayed();
  void partsUnhideAllSlotDelayed();
  void goBack();
  void goForward();
  void goUp();
  void setDirectionDelayed(TraceItemView::Direction);

  /* SingleShot Slots (without parameters) for the delayed versions */
  void setEventTypeDelayed();
  void setEventType2Delayed();
  void setGroupTypeDelayed();
  void setGroupDelayed();
  void setTraceItemDelayed();
  void loadTraceDelayed();
  void setDirectionDelayed();

  // configuration has changed
  void configChanged();

  //void refresh();
  void slotShowTipOnStart();
  void slotShowTip();

  // progress in status bar, empty message disables progress display
  void showStatus(const QString& msg, int progress);
  void showMessage(const QString&, int msec);

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

  KStatusBar* _statusbar;
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
  QString _loadTraceDelayed;
  TraceItemView::Direction _directionDelayed;

  // for status progress display
  QString _progressMsg;
  QTime _progressStart;
  QProgressBar* _progressBar;

  // toplevel configuration options
  bool _showPercentage, _showExpanded, _showCycles, _hideTemplates;

  // for running callgrind_control in the background
  QProcess* _ccProcess;
  QString _ccOutput;
};

#endif
