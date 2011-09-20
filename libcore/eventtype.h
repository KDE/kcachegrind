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

#ifndef EVENTTYPE_H
#define EVENTTYPE_H

#include <QString>

#include "subcost.h"
#include "costitem.h"

class EventTypeSet;

/**
 * A cost type, e.g. "L1 Read Miss", short "l1rm".
 *
 * We distinguish "real" cost types, where the values come
 * from the trace file, and "virtual" cost types, which
 * are calculated from the real ones.
 *
 * For a virtual cost type, set a formula to calculate it:
 * e.g. for "Read Misses" : "l1rm + l2rm".
 * To allow for parsing, you must specify a EventTypeSet
 * with according cost types (e.g. "l1rm" and "l2rm" for above formula).
 *
 * The cost type with empty name is the "const" cost type.
 */
class EventType
{
public:

  /**
   * <name> is a short (non-localized) identifier for the cost type,
   *   e.g. "l1rm".
   * <longName> is a long localized string, e.g. "L1 Read Miss"
   * <formula> uses short names to reference other types
   */
  explicit EventType(const QString& name,
                          const QString& longName = QString(),
                          const QString& formula = QString());

  void setName(const QString& n) { _name = n; }
  void setLongName(const QString& n) { _longName = n; }
  void setEventTypeSet(EventTypeSet* m);
  // enforces event to be derived, even with empty formula
  void setFormula(const QString&);
  // default arg is for specifying a real type, but index unknown
  void setRealIndex(int r = ProfileCostArray::MaxRealIndex);

  const QString& name() { return _name; }
  const QString& longName() { return _longName; }
  const QString& formula() { return _formula; }
  EventTypeSet* set() { return _set; }
  int realIndex() { return _realIndex; }
  bool isReal() { return _isReal; }

  /*
   * returns true if all cost type names can be resolved in formula
   */
  bool parseFormula();
  QString parsedFormula();
  QString parsedRealFormula();

  SubCost subCost(ProfileCostArray*);

  /*
   * For virtual costs, returns a histogram for use with
   * partitionPixmap().
   * Returns maximal real index.
   */
  int histCost(ProfileCostArray* c, double total, double* hist);

  // application wide known types, referenced by short name
  // next 2 functions return a new type object instance
  static EventType* knownRealType(const QString&);
  static EventType* knownDerivedType(const QString&);
  static void add(EventType*, bool overwriteExisting = true);
  static bool remove(const QString&);
  static int knownTypeCount();
  static EventType* knownType(int);

private:

  QString _name, _longName, _formula, _parsedFormula;
  EventTypeSet* _set;
  bool _parsed, _inParsing, _isReal;
  // index MaxRealIndex is for constant addition
  int _coefficient[MaxRealIndexValue];
  int _realIndex;

  static QList<EventType*>* _knownTypes;
};


/**
 * A class for managing a set of event types.
 *
 * Each event type has an index:
 * - Real events are in range [0 .. ProfileCostArray:MaxRealIndex[
 * - Derived events are in range [MaxRealIndex, ...]
 */
class EventTypeSet
{
public:
  EventTypeSet();
  ~EventTypeSet();

  /**
   * Defines a mapping from indexes into a list of costs to real event types
   * If <create> is false, checks if this is a existing sub set.
   */
  EventTypeMapping* createMapping(const QString& types);

  // "knows" about some real types
  int addReal(const QString&);
  int add(EventType*);
  bool remove(EventType*);
  int realCount() { return _realCount; }
  int derivedCount() { return _derivedCount; }
  int minDerivedIndex() { return ProfileCostArray::MaxRealIndex; }
  EventType* type(int);
  EventType* realType(int);
  EventType* derivedType(int);
  EventType* type(const QString&);
  EventType* typeForLong(const QString&);
  int realIndex(const QString&);
  int index(const QString&);

  /**
   * Adds all known derived event types that can be parsed
   */
  int addKnownDerivedTypes();

private:
  // we support only a fixed number of real and derived types
  EventType* _real[MaxRealIndexValue];
  EventType* _derived[MaxRealIndexValue];
  int _realCount, _derivedCount;
};

/**
 * A index list into a EventTypeSet
 *
 * This ordered list maps from indexes into real cost types
 * of a set.
 *
 * You can define a set of event types by requesting submaps by name.
 * Every new event type name will get a new real type index.
 *  EventTypeSet s;
 *  m1 = s.createMapping("Event1 Cost1 Cost2");  // returns mapping [0,1,2]
 *  m2 = s.createMapping("Event2 Cost3 Event1"); // returns mapping [3,4,0]
 * Real types of s will be:
 *  (0:Event1, 1:Cost1, 2:Cost2, 3:Event2, 4:Cost3)
 */
class EventTypeMapping
{
public:
  EventTypeMapping(EventTypeSet*);

  bool append(const QString&, bool create=true);
  bool append(int);
  void clear();

  /**
   * Get number of used indexes
   */
  int count() { return _count; }

  /**
   * Is this mapping the identity( i.e. realIndex(i)=i ) ?
   * This often allows for optimizations.
   */
  bool isIdentity() { return _isIdentity; }
  int realIndex(int i)
    { return (i<0 || i>=_count) ? ProfileCostArray::InvalidIndex : _realIndex[i]; }

  /*
   * Allows an iteration over the sorted list of all real indexes not used in
   * this mapping, up to topIndex (use ProfileCostArray::MaxRealIndex for all).
   * Usage: for(i = firstUnused(); i < topIndex; i = nextUnused(i)) { LOOP }
   */
  int firstUnused() { return _firstUnused; }
  int nextUnused(int i) {
      if (i<0 || i>=ProfileCostArray::MaxRealIndex) return ProfileCostArray::InvalidIndex;
      return _nextUnused[i]; }

private:
  EventTypeSet* _set;
  int _count, _firstUnused;
  bool _isIdentity;
  int _realIndex[MaxRealIndexValue];
  int _nextUnused[MaxRealIndexValue];
};


#endif // EVENTTYPE_H
