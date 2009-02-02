/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#ifndef COST_H
#define COST_H

#include <QString>

#include "subcost.h"
#include "context.h"
#include "utils.h"

class TraceEventType;
class TraceEventTypeMapping;
class TraceSubMapping;
class TracePart;
class TraceData;

/**
 * Base class for cost items.
 */
class ProfileCost
{
public:

    ProfileCost(ProfileContext*);
    virtual ~ProfileCost();

    ProfileContext* context() const { return _context; }
    ProfileContext::Type type() const { return context()->type(); }

  /**
   * Returns dynamic name info (without type)
   */
  virtual QString name() const;

  /**
   * Same as name, but sometimes nicer for humans :-)
   */
  virtual QString prettyName() const;

  /**
   * Returns text of all cost metrics
   */
  virtual QString costString(TraceEventTypeMapping*);

  /**
   * Returns type name + dynamic name
   */
  QString fullName() const;

  /**
   * Returns full name + cost text
   */
  QString toString();

  /**
   * Set all cost counters to zero
   */
  virtual void clear();

  /** Invalidate the cost attributes.
   * An invalidated object needs to be recalculated when a cost
   * attribute is requested (e.g. by subCost()).
   * Has to be overwritten by subclasses when the cost influences costs of
   * other cost items. If only one item depends on the cost of this item,
   * it can by set with setDependant() without a need for overwriting.
   */
  virtual void invalidate();

  /**
   * Sets a dependant to be invalidated when this cost is invalidated.
   * Call this function directly after the constructor.
   */
  void setDependant(ProfileCost* d) { _dep = d; }

  ProfileCost* dependant() { return _dep; }

  /**
   * If this item is from a single profile data file, position
   * points to a TracePart, otherwise to a TraceData object.
   */
  void setPosition(ProfileCost* p) { _position = p; }

  // getters for specific positions, to be overwritten
  virtual TracePart* part();
  virtual const TracePart* part() const;
  virtual TraceData* data();
  virtual const TraceData* data() const;

 protected:
  /** Updates cost attributes.
   * This has to be called by subclasses that access cost attributes
   * directly
   */
  virtual void update();

  ProfileContext* _context;
  bool _dirty;

  ProfileCost* _position;
  ProfileCost* _dep;
};



/**
 * An array of basic cost metrics for a trace item.
 *
 * The semantic of specific indexes is stored in the
 * TraceEventTypeMapping of the TraceData object holding this ProfileCostArray.
 */
class ProfileCostArray: public ProfileCost
{
public:
  /**
   * The maximal number of subcosts a ProfileCostArray can have.
   */
  static const int MaxRealIndex;
#define MaxRealIndexValue 13
  static const int InvalidIndex;


  ProfileCostArray(ProfileContext*);
  ProfileCostArray();
  virtual ~ProfileCostArray();

  virtual QString costString(TraceEventTypeMapping*);

  virtual void clear();

  // set the cost according to a submapping and a list of ASCII numbers
  void set(TraceSubMapping*, const char*);
  void set(TraceSubMapping*, FixString&);
  // add a cost according to a submapping and a list of ASCII numbers
  void addCost(TraceSubMapping*, const char*);
  void addCost(TraceSubMapping*, FixString&);
  // add the cost of another item
  void addCost(ProfileCostArray* item);
  void addCost(int index, SubCost value);

  // maximal cost
  void maxCost(TraceSubMapping*, FixString&);
  void maxCost(ProfileCostArray* item);
  void maxCost(int index, SubCost value);
  ProfileCostArray diff(ProfileCostArray* item);

  virtual void invalidate();

  /** Returns a sub cost. This automatically triggers
   * a call to update() if needed.
   */
  SubCost subCost(TraceEventType*);

  /**
   * Same as above, but only for real types
   */
  SubCost subCost(int);

  /** Returns a cost attribute converted to a string
   * (with space after every 3 digits)
   */
  QString prettySubCost(TraceEventType*);

 protected:
  virtual void update();

  SubCost _cost[MaxRealIndexValue];
  int _count; // only _count first indexes of _cost are used

  // cache last virtual subcost for faster access
  SubCost _cachedCost;
  TraceEventType* _cachedType;
};


#endif // COST_H
