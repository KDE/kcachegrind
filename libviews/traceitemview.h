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

#include "tracedata.h"

class QWidget;
class QMenu;

class TopLevelBase;

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

  // change from parent, call updateView() to update lazily (only if visible)
  void setEventType(TraceEventType* t) { _newEventType = t; }
  void setEventType2(TraceEventType* t) { _newEventType2 = t; }
  void set(ProfileContext::Type g) { _newGroupType = g; }
  void set(const TracePartList& l) { _newPartList = l; }
  // returns false if nothing can be shown for this trace item
  bool activate(ProfileCost* i);
  void select(ProfileCost* i);
  void notifyChange(int changeType) { _status |= changeType; }
  // all in one
  bool set(int, TraceData*, TraceEventType*, TraceEventType*,
	   ProfileContext::Type, const TracePartList&,
	   ProfileCost*, ProfileCost*);

  // general update request, call if view is/gets visible
  void updateView(bool force = false);

  /**
   * Notification from child views.
   * Default implementation notifies parent
   */
  virtual void selected(TraceItemView* sender, ProfileCost*);
  virtual void partsSelected(TraceItemView* sender, const TracePartList&);
  virtual void directionActivated(TraceItemView* sender, Direction);
  virtual void selectedEventType(TraceItemView* sender, TraceEventType*);
  virtual void selectedEventType2(TraceItemView* sender, TraceEventType*);
  virtual void activated(TraceItemView* sender, ProfileCost*);
  virtual void selectedGroupType(TraceItemView* sender, ProfileContext::Type);

  // getters...
  // always get the newest values
  TraceData* data() const { return _newData; }
  ProfileCost* activeItem() const { return _newActiveItem; }
  ProfileCost* selectedItem() const { return _newSelectedItem; }
  TraceEventType* eventType() const { return _newEventType; }
  TraceEventType* eventType2() const { return _newEventType2; }
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
  virtual ProfileCost* canShow(ProfileCost* i) { return i; }

  /* convenience functions for often used context menu items */
  void addEventTypeMenu(QMenu*,bool withCost2 = true);
  void addGoMenu(QMenu*);

protected:
  // helpers to call selected()/activated() of parentView
  void selected(ProfileCost*);
  void partsSelected(const TracePartList&);
  void activated(ProfileCost*);
  void selectedEventType(TraceEventType*);
  void selectedEventType2(TraceEventType*);
  void selectedGroupType(ProfileContext::Type);
  void directionActivated(TraceItemView::Direction);

  /* Is this view visible?
   * if not, doUpdate() will not be called by updateView()
   */
  virtual bool isViewVisible();

  // update handler (to be reimplemented)
  virtual void doUpdate(int changeType);

  TraceItemView* _parentView;
  TopLevelBase* _topLevel;

  TraceData* _data;
  TracePartList _partList;
  ProfileCost *_activeItem, *_selectedItem;
  TraceEventType *_eventType, *_eventType2;
  ProfileContext::Type _groupType;

private:
  TraceData* _newData;
  TracePartList _newPartList;
  ProfileCost *_newActiveItem, *_newSelectedItem;
  TraceEventType *_newEventType, *_newEventType2;
  ProfileContext::Type _newGroupType;

  QString _title;
  int _status;
  bool _inUpdate;
  Position _pos;
};

#endif
