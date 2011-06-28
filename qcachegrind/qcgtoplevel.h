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

#ifndef QCGTOPLEVEL_H
#define QCGTOPLEVEL_H

#include <qdatetime.h>
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
  Q_CLASSINFO("D-Bus Interface", "net.sf.qcachegrind")

public:
  QCGTopLevel();
  ~QCGTopLevel();

  TraceData* data() { return _data; }
  void setData(TraceData*);

  void createActions();
  void createDocks();
  void createMenu();
  void createToolbar();

  void closeEvent(QCloseEvent*);

  ProfileContext::Type groupType() { return _groupType; }
  EventType* eventType() { return _eventType; }
  EventType* eventType2() { return _eventType2; }
  TracePartList activeParts() { return _activeParts; }
  TracePartList hiddenParts() { return _hiddenParts; }

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
  void loadTrace();
  void loadTrace(QString, bool addToRecentFiles = true);
  void addTrace();
  void addTrace(QString);

  // shows the main window before loading to see loading progress
  void loadDelayed(QString, bool addToRecentFiles = true);

  void exportGraph();
  void newWindow();
  void configure(QString page = QString::null);
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

  // progress in status bar, empty message disables progress display
  void showStatus(const QString& msg, int progress);
  void showMessage(const QString&, int msec);

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

  QStatusBar* _statusbar;
  QLabel* _statusLabel;
  QString _progressMsg;
  QTime _progressStart;
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
  QAction *_splittedToggleAction, *_splitDirectionToggleAction;
  QAction *_layoutNext, *_layoutPrev, *_layoutRemove, *_layoutDup;
  QAction *_layoutRestore, *_layoutSave;
  QAction *_upAction, *_forwardAction, *_backAction;
  QAction *_configureAction, *_aboutAction, *_aboutQtAction;
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
  QString _loadTraceDelayed;
  bool _addToRecentFiles;
  TraceItemView::Direction _directionDelayed;
};

#endif // QCGTOPLEVEL_H
