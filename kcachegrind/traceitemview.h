/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Trace Item View
 */

#ifndef TRACEITEMVIEW_H
#define TRACEITEMVIEW_H

#include "tracedata.h"

class KConfig;
class KConfigGroup;
class KConfigBase;

class TopLevel;
class QWidget;

/**
 * Base Class for KCachegrind Views
 *
 * This class delivers the shared functionality of all KCachegrind
 * Views for one TraceItem (like Function, Object...), the "active"
 * item. Additional view attributes are current cost type, group type,
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
	 costTypeChanged     = 1,
	 groupTypeChanged    = 2,
	 partsChanged        = 4,
	 activeItemChanged   = 8,
	 selectedItemChanged = 16,
	 dataChanged         = 32,
	 configChanged       = 64 };

  enum Direction { None, Back, Forward, Up };

  // a TraceItemView can have a position in a parent container
  enum Position { Hidden, Top, Right, Left, Bottom };

  TraceItemView(TraceItemView* parentView, TopLevel* top = 0);
  virtual ~TraceItemView() {}

  virtual QString whatsThis();

  static KConfigGroup* configGroup(KConfig*, QString prefix, QString postfix);
  static void writeConfigEntry(KConfigBase*, const char* pKey, QString value,
                               const char* def, bool bNLS = false);
  static void writeConfigEntry(KConfigBase*, const char* pKey,
                               int value, int def);
  static void writeConfigEntry(KConfigBase*, const char* pKey,
                               bool value, bool def);
  static void writeConfigEntry(KConfigBase*, const char* pKey,
                               double value, double def);
  virtual void readViewConfig(KConfig*, QString prefix, QString postfix);
  virtual void saveViewConfig(KConfig*, QString prefix, QString postfix);

  // change from parent, call update() afterwards if view is visible
  void setData(TraceData* d) { _newData = d; }
  void set(TraceCostType* t) { _newCostType = t; }
  void set(TraceItem::CostType g) { _newGroupType = g; }
  // returns false if nothing can be shown for this trace item
  bool activate(TraceItem* i);
  void select(TraceItem* i);
  void notifyChange(int changeType) { _status |= changeType; }
  // all in one
  bool set(int, TraceData*, TraceCostType*, TraceItem::CostType,
	   TraceItem*, TraceItem*);

  // general update request, call if view is/gets visible
  void updateView(bool force = false);

  /**
   * Notification from child views.
   * Default implementation notifies parent
   */
  virtual void selected(TraceItemView* sender, TraceItem*);
  virtual void activated(TraceItemView* sender, Direction);
  virtual void selected(TraceItemView* sender, TraceCostType*);
  virtual void activated(TraceItemView* sender, TraceItem*);

  // getters...
  // always get the newest values
  TraceData* data() { return _newData; }
  TraceItem* activeItem() { return _newActiveItem; }
  TraceItem* selectedItem() { return _newSelectedItem; }
  TraceCostType* costType() { return _newCostType; }
  TraceItem::CostType groupType() { return _newGroupType; }

  TraceFunction* activeFunction();
  int status() { return _status; }

  // pointer to top level window to e.g. show status messages
  void setTopLevel(TopLevel* t) { _topLevel = t; }
  TopLevel* topLevel() { return _topLevel; }

  void setPosition(Position p) { _pos = p; }
  Position position() { return _pos; }

  void setTitle(QString t) { _title = t; }
  QString title() { return _title; }

  virtual QWidget* widget() const;

  /**
   * This function is called when a new item should become active.
   * Reimplement this in subclasses.
   *
   * Returns the real item to become active. You can call select() here.
   * Return 0 if nothing can be shown.
   * Use the methods like data() instead of _data here, as
   * _data possibly will give old/wrong information.
   */
  virtual TraceItem* canShow(TraceItem* i) { return i; }

protected:
  // helpers to call selected()/activated() of parentView
  void selected(TraceItem*);
  void activated(TraceItem*);
  void selected(TraceCostType*);
  void activated(Direction);

  // Is this view visible? If not, doUpdate() won't be called by updateView()
  virtual bool isViewVisible();

  // update handler (to be reimplemented)
  virtual void doUpdate(int changeType);

  TraceItemView* _parentView;
  TopLevel* _topLevel;

  TraceData* _data;
  TraceItem *_activeItem, *_selectedItem;
  TraceCostType* _costType;
  TraceItem::CostType _groupType;

private:
  TraceData* _newData;
  TraceItem *_newActiveItem, *_newSelectedItem;
  TraceCostType* _newCostType;
  TraceItem::CostType _newGroupType;

  QString _title;
  int _status;
  bool _inUpdate;
  Position _pos;
};

#endif
