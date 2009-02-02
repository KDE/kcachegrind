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
#include <QColor>

#include "subcost.h"
#include "cost.h"

class TraceEventTypeMapping;

/**
 * A cost type, e.g. "L1 Read Miss", short "l1rm".
 *
 * We distinguish "real" cost types, where the values come
 * from the trace file, and "virtual" cost types, which
 * are calculated from the real ones.
 *
 * For a virtual cost type, set a formula to calculate it:
 * e.g. for "Read Misses" : "l1rm + l2rm".
 * To allow for parsing, you must specify a TraceEventTypeMapping
 * with according cost types (e.g. "l1rm" and "l2rm" for above formula).
 *
 * The cost type with empty name is the "const" cost type.
 */
class TraceEventType
{
public:

  /**
   * <name> is a short (non-localized) identifier for the cost type,
   *   e.g. "l1rm".
   * <longName> is a long localized string, e.g. "L1 Read Miss"
   * <formula> uses short names to reference other types
   */
  explicit TraceEventType(const QString& name,
                          const QString& longName = QString(),
                          const QString& formula = QString());

  void setName(const QString& n) { _name = n; }
  void setLongName(const QString& n) { _longName = n; }
  void setMapping(TraceEventTypeMapping* m);
  void setFormula(const QString&);
  // default arg is for specifying a real type, but index unknown
  void setRealIndex(int r = ProfileCostArray::MaxRealIndex);

  const QString& name() { return _name; }
  const QString& longName() { return _longName; }
  const QString& formula() { return _formula; }
  TraceEventTypeMapping* mapping() { return _mapping; }
  int realIndex() { return _realIndex; }
  bool isReal() { return _formula.isEmpty(); }
  QColor color();

  /*
   * returns true if all cost type names can be resolved in formula
   */
  bool parseFormula();
  QString parsedFormula();

  SubCost subCost(ProfileCostArray*);

  /*
   * For virtual costs, returns a histogram for use with
   * partitionPixmap().
   * Returns maximal real index.
   */
  int histCost(ProfileCostArray* c, double total, double* hist);

  // application wide known types, referenced by short name
  // next 2 functions return a new type object instance
  static TraceEventType* knownRealType(const QString&);
  static TraceEventType* knownDerivedType(const QString&);
  static void add(TraceEventType*);
  static bool remove(const QString&);
  static int knownTypeCount();
  static TraceEventType* knownType(int);

private:

  QString _name, _longName, _formula;
  TraceEventTypeMapping* _mapping;
  bool _parsed, _inParsing;
  // index MaxRealIndex is for constant addition
  int _coefficient[MaxRealIndexValue];
  int _realIndex;

  static QList<TraceEventType*>* _knownTypes;
};


/**
 * A class for managing a set of event types.
 *
 * Each event type has an index:
 * - Real events are in range [0 .. ProfileCostArray:MaxRealIndex[
 * - Derived events are in range [MaxRealIndex, ...]
 */
class TraceEventTypeMapping
{
public:
  TraceEventTypeMapping();
  ~TraceEventTypeMapping();

  /**
   * Defines a sub mapping with a list of real event types
   * If <create> is false, checks if this is a existing sub mapping.
   */
  TraceSubMapping* subMapping(const QString& types, bool create = true);

  // "knows" about some real types
  int addReal(const QString&);
  int add(TraceEventType*);
  bool remove(TraceEventType*);
  int realCount() { return _realCount; }
  int derivedCount() { return _derivedCount; }
  int minDerivedIndex() { return ProfileCostArray::MaxRealIndex; }
  TraceEventType* type(int);
  TraceEventType* realType(int);
  TraceEventType* derivedType(int);
  TraceEventType* type(const QString&);
  TraceEventType* typeForLong(const QString&);
  int realIndex(const QString&);
  int index(const QString&);
  QColor* realColors() { return _realColor; }

  /**
   * Adds all known derived event types that can be parsed
   */
  int addKnownDerivedTypes();

private:
  // we support only a fixed number of real and derived types
  TraceEventType* _real[MaxRealIndexValue];
  QColor _realColor[MaxRealIndexValue];
  TraceEventType* _derived[MaxRealIndexValue];
  int _realCount, _derivedCount;
};

/**
 * A submapping of a TraceEventTypeMapping
 *
 * This is a fixed ordered list of indexes for real cost types
 * in a mapping.
 *
 * You can define a mapping by requesting submappings. Undefined cost
 * types will get a new real type index.
 *  TraceEventTypeMapping m;
 *  sm1 = m.subMapping("Event1 Cost1 Cost2");  // returns submap [0,1,2]
 *  sm2 = m.subMapping("Event2 Cost3 Event1"); // returns submap [3,4,0]
 * Real types of m will be:
 *  (0:Event1, 1:Cost1, 2:Cost2, 3:Event2, 4:Cost3)
 */
class TraceSubMapping
{
public:
  TraceSubMapping(TraceEventTypeMapping*);

  bool append(const QString&, bool create=true);
  bool append(int);
  void clear();

  /**
   * Get number of used indexes
   */
  int count() { return _count; }

  /**
   * Is this submapping the identity( i.e. realIndex(i)=i ) ?
   * This often allows for optimizations.
   */
  bool isIdentity() { return _isIdentity; }
  int realIndex(int i)
    { return (i<0 || i>=_count) ? ProfileCostArray::InvalidIndex : _realIndex[i]; }

  /*
   * Allows an iteration over the sorted list of all real indexes not used in
   * this submapping, up to topIndex (use ProfileCostArray::MaxRealIndex for all).
   * Usage: for(i = firstUnused(); i < topIndex; i = nextUnused(i)) { LOOP }
   */
  int firstUnused() { return _firstUnused; }
  int nextUnused(int i) {
      if (i<0 || i>=ProfileCostArray::MaxRealIndex) return ProfileCostArray::InvalidIndex;
      return _nextUnused[i]; }

private:
  TraceEventTypeMapping* _mapping;
  int _count, _firstUnused;
  bool _isIdentity;
  int _realIndex[MaxRealIndexValue];
  int _nextUnused[MaxRealIndexValue];
};


#endif // EVENTTYPE_H
