/* This file is part of KCachegrind.
   Copyright (C) 2003-2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Trace Item View
 */

#ifndef TRACEITEMVIEW_H
#define TRACEITEMVIEW_H

#include <QTimer>

#include "tracedata.h"

class QWidget;
class QMenu;

class TopLevelBase;
class TraceItemView;

/* Helper class for TraceItemView for merging update requests.
 *
 * This can not be directly done in TraceItemView which can not have slots,
 * as this would need TraceItemView to be inherited from QObject. However,
 * we want subclasses of TraceItemView to also inherit from QWidget, and
 * multiple inheritance of a QObject is impossible
 */
class TraceItemViewUpdateTimer: public QTimer
{
    Q_OBJECT

public:
    explicit TraceItemViewUpdateTimer(TraceItemView* view);

private slots:
    void timeoutTriggered();

private:
    TraceItemView* _view;
};


/**
 * Abstract Base Class for KCachegrind Views
 *
 * This class delivers the shared functionality of all KCachegrind
 * Views for one ProfileCost (like Function, Object...), the "active"
 * item. Additional view attributes are current primary cost type,
 * an optional secondary cost type, group type,
 * and possibly a selected costitem in this view.
 * Note that there is a difference in changing the selected item of
 * a view (this usually changes selection in other views, too), and
 * activating that item.
 */
class TraceItemView
{
    friend class TraceItemViewUpdateTimer;

public:

  /**
   * Change type for update functions
   * - <dataChanged> is used if e.g. cycles are recalculated
   */
  enum { nothingChanged      = 0,
	 eventTypeChanged     = 1,
	 eventType2Changed    = 2,
	 groupTypeChanged    = 4,
	 partsChanged        = 8,
	 activeItemChanged   = 16,
	 selectedItemChanged = 32,
	 dataChanged         = 64,
	 configChanged       = 128 };

  enum Direction { None, Back, Forward, Up };

  // a TraceItemView can have a position in a parent container
  enum Position { Hidden, Top, Right, Left, Bottom };

  explicit TraceItemView(TraceItemView* parentView, TopLevelBase* top = 0);
  virtual ~TraceItemView();

  virtual QString whatsThis() const;

  // visualization layout and options (uses ConfigStorage)
  virtual void saveLayout(const QString& prefix, const QString& postfix);
  virtual void restoreLayout(const QString& prefix, const QString& postfix);
  virtual void saveOptions(const QString& prefix, const QString& postfix);
  virtual void restoreOptions(const QString& prefix, const QString& postfix);

  // Immediate remove all references to old data, and set the new.
  // This resets the visualization state.
  // A GUI update has to be triggered with updateView().
  // Overwrite in container views to also set new data for all members.
  virtual void setData(TraceData* d);

  // modify visualization state, updates automatically
  void setEventType(EventType* t) { _newEventType = t; updateView(); }
  void setEventType2(EventType* t) { _newEventType2 = t; updateView(); }
  void set(ProfileContext::Type g) { _newGroupType = g; updateView(); }
  void set(const TracePartList& l) { _newPartList = l; updateView(); }
  // returns false if nothing can be shown for this trace item
  bool activate(CostItem* i);
  void select(CostItem* i);
  void notifyChange(int changeType) { _status |= changeType; updateView(); }
  // all in one
  bool set(int, TraceData*, EventType*, EventType*,
	   ProfileContext::Type, const TracePartList&,
	   CostItem*, CostItem*);

  // if mergeUpdates is true (default), calls to updateView do not
  // directly trigger an update of the view
  void setMergeUpdates(bool b) { _mergeUpdates = b; }

  // general update request, call if view is/gets visible
  // force: update immediatly even if invisible and no change was detected
  void updateView(bool force = false);

  /**
   * Notification from child views.
   * Default implementation notifies parent
   */
  virtual void selected(TraceItemView* sender, CostItem*);
  virtual void partsSelected(TraceItemView* sender, const TracePartList&);
  virtual void directionActivated(TraceItemView* sender, Direction);
  virtual void selectedEventType(TraceItemView* sender, EventType*);
  virtual void selectedEventType2(TraceItemView* sender, EventType*);
  virtual void activated(TraceItemView* sender, CostItem*);
  virtual void selectedGroupType(TraceItemView* sender, ProfileContext::Type);

  // getters...
  // always get the newest values
  TraceData* data() const { return _newData; }
  CostItem* activeItem() const { return _newActiveItem; }
  CostItem* selectedItem() const { return _newSelectedItem; }
  EventType* eventType() const { return _newEventType; }
  EventType* eventType2() const { return _newEventType2; }
  ProfileContext::Type groupType() const { return _newGroupType; }
  const TracePartList& partList() const { return _newPartList; }

  TraceFunction* activeFunction();
  int status() const { return _status; }

  // pointer to top level window to e.g. show status messages
  void setTopLevel(TopLevelBase* t) { _topLevel = t; }
  TopLevelBase* topLevel() const { return _topLevel; }

  void setPosition(Position p) { _pos = p; }
  Position position() const { return _pos; }

  void setTitle(QString t) { _title = t; }
  QString title() const { return _title; }

  // We depend on derived class to be a widget.
  // Force overiding by making this abstract.
  virtual QWidget* widget() = 0;

  /**
   * This function is called when a new item should become active.
   * Reimplement this in subclasses.
   *
   * Returns the real item to become active. You can call select() here.
   * Return 0 if nothing can be shown.
   * Use the methods like data() instead of _data here, as
   * _data possibly will give old/wrong information.
   */
  virtual CostItem* canShow(CostItem* i) { return i; }

  /* convenience functions for often used context menu items */
  void addEventTypeMenu(QMenu*,bool withCost2 = true);
  void addGoMenu(QMenu*);

protected:
  // helpers to call selected()/activated() of parentView
  void selected(CostItem*);
  void partsSelected(const TracePartList&);
  void activated(CostItem*);
  void selectedEventType(EventType*);
  void selectedEventType2(EventType*);
  void selectedGroupType(ProfileContext::Type);
  void directionActivated(TraceItemView::Direction);

  /* Is this view visible?
   * if not, doUpdate() will not be called by updateView()
   */
  virtual bool isViewVisible();

  // update handler (to be reimplemented)
  virtual void doUpdate(int changeType, bool force);

  TraceItemView* _parentView;
  TopLevelBase* _topLevel;

  TraceData* _data;
  TracePartList _partList;
  CostItem *_activeItem, *_selectedItem;
  EventType *_eventType, *_eventType2;
  ProfileContext::Type _groupType;

private:
  /* Multiple update requests via updateView() are merged, and result in one
   * call to triggerUpdate() after a timeout (using TraceItemViewUpdateTimer)
   */
  void triggerUpdate(bool force);

  TraceData* _newData;
  TracePartList _newPartList;
  CostItem *_newActiveItem, *_newSelectedItem;
  EventType *_newEventType, *_newEventType2;
  ProfileContext::Type _newGroupType;
  TraceItemViewUpdateTimer* _updateTimer;

  QString _title;
  int _status;
  bool _mergeUpdates, _needsUpdate;
  Position _pos;
};



#endif
