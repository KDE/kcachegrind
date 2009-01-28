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

/*
 * Classes holding profiling data for
 * multiple tracefiles for one command.
 * See class TraceData first.
 */

#ifndef TRACEDATA_H
#define TRACEDATA_H

#include <qstring.h>
#include <qstringlist.h>
#include <q3ptrlist.h>
#include <qmap.h>
#include <q3ptrvector.h>
#include <qcolor.h>

#include <QList>

#include "subcost.h"
#include "utils.h"

class QFile;

/**
 * All cost items are classes prefixed with "Trace".
 * "ProfileCostArray" holds basic cost metrics for the simplest, smallest
 * trace entity: These are events counted for an instruction at
 * a specific memory address of the traced program.
 * All other cost items are derived from ProfileCostArray, and add needed
 * cost metrics, e.g. for a call the number of calls that happened.
 *
 * Abstract, i.e. never instantiated cost items are
 * - ProfileCostArray: Basic cost metrics (instr/read/write access + cache events)
 * - TraceCallCost: Additional call count cost metric.
 * - TraceInclusiveCost: Additional ProfileCostArray aggregated.
 * - TraceListCost: Adds dependency to a list of ProfileCostArray's
 * - TraceCallListCost: same for list of TraceCallCost's
 * - TraceInclusiveListCost: same for list of TraceInclusiveCost's
 * - TraceCostItem: Base for cost items for "interesting" costs:
 *              TraceFunction, TraceClass, TraceFile, TraceObject
 *
 * The smallest Cachegrind output is trace data indexed by a source
 * line number, a TracePartLine. Another one is a call from one
 * source line of a function to another function, a TracePartLineCall.
 * All other cost items derive the value by summation of cost metrics
 * from TraceLineItem and TracePartLineCall costs; their cost is
 * calculated lazy on demand and cached afterwards.
 *
 * For cost items, which are sums over all trace files read in, the
 * summed cost metrics change when e.g. a new trace file is read.
 * Thus, their cached costs are invalidated, and again recalculated
 * only on demand. In the following list, theses cost items are called
 * "dynamic", the other "fixed" (but neverless calculated lazy).
 *
 *  Cost Item          Type      Summation of ...
 *
 *  TracePartLineCall  fixed     Read from trace file
 *  TracePartLine      fixed     Read from trace file
 *  TracePartCall      fixed     TracePartLineCall's
 *  TraceLineCall      dynamic   TracePartLineCall's
 *  TraceCall          dynamic   TraceLineCall's
 *  TraceLine          dynamic   TracePartLine's and TraceLineCall's
 *  TracePartFunction  fixed     TracePartLine's / TracePartCall's
 *  TraceFunction      dynamic   TraceLine's / TraceCall's (called from)
 *  TracePartClass     fixed     TracePartFunction's
 *  TraceClass         dynamic   TraceFunction's
 *  TracePartFile      fixed     TracePartFunction's
 *  TraceFile          dynamic   TraceFunction's
 *  TracePartObject    fixed     TracePartFunction's
 *  TraceObject        dynamic   TraceFunction's
 *  TracePart          fixed     TracePartLine's
 *  TraceData          dynamic   TracePart's
 *
 * As there exists only one TraceData object for a traced program, its the
 * owner of some "high level" cost items. The following shows the owner
 * relationship of the cost item classes, together with references.
 *
 *  Cost Item          Owner (& back ref)   Other References to
 *
 *  TracePartLineCall  TraceLineCall
 *  TracePartCall      TraceCall            TracePartLineCall's
 *  TracePartLine      TraceLine            TracePartLineCall's
 *  TracePartFunction  TraceFunction
 *  TracePartClass     TraceClass           TracePart
 *  TracePartFile      TraceFile            TracePart
 *  TracePartObject    TraceObject          TracePart
 *  TraceLineCall      TraceCall            TracePartLineCall's
 *  TraceCall          TraceFunction        TracePartCall's
 *  TraceLine          TraceData            TraceLineCall's
 *  TraceFunction      TraceData            TraceCall's (calling)
 *  TraceClass         TraceData
 *  TraceFile          TraceData
 *  TraceObject        TraceData
 *  TracePart          TraceData
 *  TraceData          Main Application
 *
 * Convention:
 * - The owner has a factory method for owned objects,
 *   and calls addXXX() to install references in other objects
 * - The owner is first arg in a constructor.
 */


class FixString;
class FixCost;
class FixCallCost;
class FixJump;
class FixPool;
class DynPool;
class Logger;

class ProfileCostArray;
class TraceEventType;
class TraceEventTypeMapping;
class TraceSubMapping;
class TraceJumpCost;
class TraceCallCost;
class TraceInclusiveCost;

class TracePartInstr;
class TracePartInstrCall;
class TracePartLine;
class TracePartLineCall;
class TracePartCall;
class TracePartLineRegion;
class TracePartFunction;
class TracePartClass;
class TracePartObject;
class TracePartFile;

class TraceInstr;
class TraceInstrJump;
class TraceInstrCall;
class TraceLine;
class TraceLineJump;
class TraceLineCall;
class TraceCall;
class TraceLineRegion;
class TraceFunctionSource;
class TraceFunction;
class TraceFunctionCycle;
class TraceClass;
class TraceObject;
class TraceFile;
class TracePart;
class TraceData;

typedef Q3PtrList<ProfileCostArray> TraceCostList;
typedef Q3PtrList<TraceJumpCost> TraceJumpCostList;
typedef Q3PtrList<TraceCallCost> TraceCallCostList;
typedef Q3PtrList<TraceInclusiveCost> TraceInclusiveCostList;

typedef Q3PtrList<TracePartCall>  TracePartCallList;
typedef Q3PtrList<TracePartInstr> TracePartInstrList;
typedef Q3PtrList<TracePartLine>  TracePartLineList;
typedef Q3PtrList<TracePartLineRegion>  TracePartLineRegionList;
typedef Q3PtrList<TracePartFunction>  TracePartFunctionList;
typedef Q3PtrList<TracePartInstrCall> TracePartInstrCallList;
typedef Q3PtrList<TracePartLineCall>  TracePartLineCallList;


typedef Q3PtrList<TraceInstr> TraceInstrList;
typedef Q3PtrList<TraceLine> TraceLineList;
typedef Q3PtrList<TraceInstrCall> TraceInstrCallList;
typedef Q3PtrList<TraceLineCall> TraceLineCallList;
typedef Q3PtrList<TraceCall> TraceCallList;
typedef Q3PtrList<TraceFile> TraceFileList;
typedef Q3PtrList<TraceLineRegion> TraceLineRegionList;
typedef Q3PtrList<TraceFunctionSource> TraceFunctionSourceList;
typedef Q3PtrList<TraceFunction> TraceFunctionList;
typedef Q3PtrList<TraceFunctionCycle> TraceFunctionCycleList;
typedef QMap<QString, TraceObject> TraceObjectMap;
typedef QMap<QString, TraceClass> TraceClassMap;
typedef QMap<QString, TraceFile> TraceFileMap;
typedef QMap<QString, TraceFunction> TraceFunctionMap;
typedef QMap<uint, TraceLine> TraceLineMap;


/**
 * Addresses are 64bit values like costs to be able
 * to always load profile data produced on 64bit
 * architectures.
 */
class Addr
{
 public:
  Addr() { _v=0; }
  Addr(uint64 v) { _v = v; }

  // Interpretes char data at s as hex (without "0x" prefix)
  // and return number of interpreted chars.
  int set(const char *s);
  bool set(FixString& s);
  QString toString() const;
  // similar to toString(), but adds a space every 4 digits
  QString pretty() const;

  // returns true if this address is in [a-distance;a+distance]
  bool isInRange(Addr a, int distance);

  bool operator==(const Addr& a) const { return (_v == a._v); }
  bool operator!=(const Addr& a) const { return (_v != a._v); }
  bool operator>(const Addr& a) const { return _v > a._v; }
  bool operator>=(const Addr& a) const { return _v >= a._v; }
  bool operator<(const Addr& a) const { return _v < a._v; }
  bool operator<=(const Addr& a) const { return _v <= a._v; }

  Addr operator+(int d) const { return Addr(_v + d); }
  Addr operator-(int d) const { return Addr(_v - d); }

 private:
  uint64 _v;
};

typedef QMap<Addr, TraceInstr> TraceInstrMap;

/**
 * Base class for cost contexts.
 */
class ProfileContext
{
public:
    // RTTI for trace item classes, using type() method
    enum Type {
	InvalidType, UnknownType,
	PartInstr, Instr,
	PartLine, Line,
	PartInstrJump, InstrJump,
	PartLineJump, LineJump,
	PartInstrCall, InstrCall,
	PartLineCall, LineCall,
	PartCall, Call,
	PartLineRegion, LineRegion,
	PartFunction, FunctionSource, Function, FunctionCycle,
	PartClass, Class, ClassCycle,
	PartFile, File, FileCycle,
	PartObject, Object, ObjectCycle,
	Part, Data,
	MaxType };

    ProfileContext(ProfileContext::Type);

    ProfileContext::Type type() { return _type; }

    static ProfileContext* context(ProfileContext::Type);

    // conversion of context type to locale independent string (e.g. for config)
    static QString typeName(Type);
    static Type type(const QString&);
    // the versions below should be used for user visible strings, as
    // these use localization settings
    static QString i18nTypeName(Type);
    static Type i18nType(const QString&);

    // clean up some static data
    static void cleanup();

private:
    Type _type;

    static QHash<QString, ProfileContext*> _contexts;
    static QString* _typeName;
    static QString* _i18nTypeName;
};

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


/**
 * Cost of a (conditional) jump.
 */
class TraceJumpCost: public ProfileCost
{
 public:
    TraceJumpCost(ProfileContext*);
    virtual ~TraceJumpCost();

    // reimplementations for cost addition
    virtual QString costString(TraceEventTypeMapping* m);
    virtual void clear();

    void addCost(TraceJumpCost*);

    // additional cost metrics
    SubCost followedCount();
    SubCost executedCount();
    void addFollowedCount(SubCost c) { _followedCount += c; }
    void addExecutedCount(SubCost c) { _executedCount += c; }

 protected:
    SubCost _executedCount, _followedCount;
};



/**
 * Cost item with additional call count metric.
 */
class TraceCallCost: public ProfileCostArray
{
 public:
  TraceCallCost(ProfileContext*);
  virtual ~TraceCallCost();

  // reimplementations for cost addition
  virtual QString costString(TraceEventTypeMapping* m);
  virtual void clear();

  // additional cost metric
  SubCost callCount();
  QString prettyCallCount();
  void addCallCount(SubCost c);

 protected:
  SubCost _callCount;
};


/**
 * Cost item with additional inclusive metric
 */
class TraceInclusiveCost: public ProfileCostArray
{
 public:
  TraceInclusiveCost(ProfileContext*);
  virtual ~TraceInclusiveCost();

  // reimplementations for cost addition
  virtual QString costString(TraceEventTypeMapping* m);
  virtual void clear();

  // additional cost metric
  ProfileCostArray* inclusive();
  void addInclusive(ProfileCostArray*);

 protected:
  ProfileCostArray _inclusive;
};


/**
 * Cost Item
 * depends on a list of cost items.
 */
class TraceListCost: public ProfileCostArray
{
 public:
  TraceListCost(ProfileContext*);
  virtual ~TraceListCost();

  // reimplementation for dependency list
  virtual void update();

  TraceCostList& deps() { return _deps; }
  void addDep(ProfileCostArray*);
  ProfileCostArray* findDepFromPart(TracePart*);

 protected:
  // overwrite in subclass to change update behaviour
  virtual bool onlyActiveParts() { return false; }

  TraceCostList _deps;

 private:
  // very temporary: cached
  ProfileCostArray* _lastDep;
};


/**
 * Jump Cost Item
 * depends on a list of Jump cost items.
 */
class TraceJumpListCost: public TraceJumpCost
{
 public:
  TraceJumpListCost(ProfileContext*);
  virtual ~TraceJumpListCost();

  // reimplementation for dependency list
  virtual void update();

  TraceJumpCostList deps() { return _deps; }
  void addDep(TraceJumpCost*);
  TraceJumpCost* findDepFromPart(TracePart*);

 protected:
  // overwrite in subclass to change update behaviour
  virtual bool onlyActiveParts() { return false; }

  TraceJumpCostList _deps;

 private:
  // very temporary: cached
  TraceJumpCost* _lastDep;
};




/**
 * Call Cost Item
 * depends on a list of Call cost items.
 */
class TraceCallListCost: public TraceCallCost
{
 public:
  TraceCallListCost(ProfileContext*);
  virtual ~TraceCallListCost();

  // reimplementation for dependency list
  virtual void update();

  TraceCallCostList deps() { return _deps; }
  void addDep(TraceCallCost*);
  TraceCallCost* findDepFromPart(TracePart*);

 protected:
  // overwrite in subclass to change update behaviour
  virtual bool onlyActiveParts() { return false; }

  TraceCallCostList _deps;

 private:
  // very temporary: cached
  TraceCallCost* _lastDep;
};


/**
 * Inclusive Cost Item depends on a list of inclusive cost items.
 */
class TraceInclusiveListCost: public TraceInclusiveCost
{
 public:
  TraceInclusiveListCost(ProfileContext*);
  virtual ~TraceInclusiveListCost();

  // reimplementation for dependency
  virtual void update();

  TraceInclusiveCostList deps() { return _deps; }
  void addDep(TraceInclusiveCost*);
  TraceInclusiveCost* findDepFromPart(TracePart*);

 protected:
  // overwrite in subclass to change update behaviour
  virtual bool onlyActiveParts() { return false; }

  TraceInclusiveCostList _deps;

 private:
  // very temporary: cached
  TraceInclusiveCost* _lastDep;
};





/*-----------------------------------------------------------------
 * Classes for cost items of one trace file, i.e. a "trace part"
 *-----------------------------------------------------------------
 */

/**
 * Cost of jump at a instruction code address from a trace file.
 */
class TracePartInstrJump: public TraceJumpCost
{
 public:
  TracePartInstrJump(TraceInstrJump*, TracePartInstrJump*);
  virtual ~TracePartInstrJump();

    // fix cost item
    virtual void update() {}
    TraceInstrJump* instrJump() const { return (TraceInstrJump*) _dep; }
    TracePartInstrJump* next() const { return _next; }

 private:
    // chaining all parts for InstrJump
    TracePartInstrJump* _next;
};


/**
 * Cost of a call at a instruction code address from a trace file.
 * Cost is always up to date, no lazy update needed.
 */
class TracePartInstrCall: public TraceCallCost
{
public:
  TracePartInstrCall(TraceInstrCall*);
  virtual ~TracePartInstrCall();

  // fix cost item
  virtual void update() {}
  TraceInstrCall* instrCall() const { return (TraceInstrCall*) _dep; }
};


/**
 * Cost of a code instruction address from a trace file.
 * Cost is always up to date, no lazy update needed.
 */
class TracePartInstr: public ProfileCostArray
{
public:
  TracePartInstr(TraceInstr*);
  virtual ~TracePartInstr();

  // fix cost item
  virtual void update() {}

  TraceInstr* instr() const { return (TraceInstr*)_dep; }
};


/**
 * Cost of jump at a source line from a trace file.
 */
class TracePartLineJump: public TraceJumpCost
{
 public:
    TracePartLineJump(TraceLineJump*);
    virtual ~TracePartLineJump();

    // fix cost item
    virtual void update() {}
    TraceLineJump* lineJump() const { return (TraceLineJump*) _dep; }
};


/**
 * Cost of a call at a line from a trace file.
 * Cost is always up to date, no lazy update needed.
 */
class TracePartLineCall: public TraceCallCost
{
public:
  TracePartLineCall(TraceLineCall*);
  virtual ~TracePartLineCall();

  // fix cost item
  virtual void update() {}
  TraceLineCall* lineCall() const { return (TraceLineCall*) _dep; }
};



/**
 * Cost of a line from a trace file.
 * Cost is always up to date, no lazy update needed.
 */
class TracePartLine: public ProfileCostArray
{
public:
  TracePartLine(TraceLine*);
  virtual ~TracePartLine();

  // fix cost item
  virtual void update() {}

  TraceLine* line() const { return (TraceLine*)_dep; }
};


/**
 * Cost of a source region.
 */
class TracePartLineRegion: public TraceInclusiveCost
{
public:
  TracePartLineRegion(TraceLineRegion*);
  virtual ~TracePartLineRegion();

  virtual void update();

  TraceLineRegion* region() const { return (TraceLineRegion*)_dep; }
};


/**
 * Cost of a call at a function to another function,
 * from a single trace file.
 */
class TracePartCall: public TraceCallListCost
{
public:
  TracePartCall(TraceCall* call);
  virtual ~TracePartCall();

  // calls a function itself?
  bool isRecursion();

  // reimplementation for dependency list
  virtual void update();

  TraceCall* call() const { return (TraceCall*)_dep; }

  FixCallCost* setFirstFixCallCost(FixCallCost* fc)
    { FixCallCost* t = _firstFixCallCost; _firstFixCallCost = fc; return t; }
  FixCallCost* firstFixCallCost() const { return _firstFixCallCost; }

private:
  FixCallCost* _firstFixCallCost;
};


/**
 * Cost of a function,
 * from a single trace file.
 */
class TracePartFunction: public TraceInclusiveCost
{
public:
  TracePartFunction(TraceFunction*,
		    TracePartObject*, TracePartFile*);
  virtual ~TracePartFunction();

  virtual void update();
  virtual QString costString(TraceEventTypeMapping* m);

  void addPartInstr(TracePartInstr*);
  void addPartLine(TracePartLine*);
  void addPartCaller(TracePartCall*);
  void addPartCalling(TracePartCall*);

  TraceFunction* function() { return (TraceFunction*) _dep; }
  TracePartObject* partObject() { return _partObject; }
  TracePartClass* partClass() { return _partClass; }
  TracePartFile* partFile() { return _partFile; }
  const TracePartCallList& partCallers() { return _partCallers; }
  const TracePartCallList& partCallings() { return _partCallings; }
  void setPartObject(TracePartObject* o) { _partObject = o; }
  void setPartClass(TracePartClass* c) { _partClass = c; }
  void setPartFile(TracePartFile* f) { _partFile = f; }

  /* for linked list of FixXXX objects */
  FixCost* setFirstFixCost(FixCost* fc)
      { FixCost* t = _firstFixCost; _firstFixCost = fc; return t; }
  FixCost* firstFixCost() const { return _firstFixCost; }
  FixJump* setFirstFixJump(FixJump* fj)
      { FixJump* t = _firstFixJump; _firstFixJump = fj; return t; }
  FixJump* firstFixJump() const { return _firstFixJump; }

  // additional cost metrics
  SubCost calledCount();
  SubCost callingCount();
  QString prettyCalledCount();
  QString prettyCallingCount();
  int calledContexts();
  int callingContexts();

private:
  TracePartObject* _partObject;
  TracePartClass* _partClass;
  TracePartFile* _partFile;

  TracePartCallList _partCallings;
  TracePartCallList _partCallers;
  TracePartInstrList _partInstr;
  TracePartLineList _partLines;

  // cached
  SubCost _calledCount, _callingCount;
  int _calledContexts, _callingContexts;

  FixCost* _firstFixCost;
  FixJump* _firstFixJump;
};


/**
 * Cost of a class,
 * from a single trace file.
 */
class TracePartClass: public TraceInclusiveListCost
{
public:
  TracePartClass(TraceClass*);
  virtual ~TracePartClass();

  QString prettyName() const;

  TraceClass* cls() { return (TraceClass*)_dep; }
  void addPartFunction(TracePartFunction* f) { addDep(f); }
};


/**
 * Cost of a source file,
 * from a single trace file.
 */
class TracePartFile: public TraceInclusiveListCost
{
public:
  TracePartFile(TraceFile*);
  virtual ~TracePartFile();

  TraceFile* file() { return (TraceFile*)_dep; }
  void addPartFunction(TracePartFunction* f) { addDep(f); }
};


/**
 * Cost of a object,
 * from a single trace file.
 */
class TracePartObject: public TraceInclusiveListCost
{
public:
  TracePartObject(TraceObject*);
  virtual ~TracePartObject();

  TraceObject* object() const { return (TraceObject*)_dep; }
  void addPartFunction(TracePartFunction* f) { addDep(f); }
};



/**
 * A Trace Part: All data read from a trace file, containing all costs
 * that happened in a specified time interval of the executed command.
 */
class TracePart: public TraceListCost
{
public:
  TracePart(TraceData*, QFile* file);
  virtual ~TracePart();

  virtual TracePart* part() { return this; }
  virtual const TracePart* part() const { return this; }

  QString shortName() const;
  QString prettyName() const;
  QFile* file() const { return _file; }
  QString name() const { return _name; }
  QString description() const { return _descr; }
  QString trigger() const { return _trigger; }
  QString timeframe() const { return _timeframe; }
  QString version() const { return _version; }
  int partNumber() { return _number; }
  int threadID() { return _tid; }
  int processID() { return _pid; }
  void setDescription(const QString& d) { _descr = d; }
  void setTrigger(const QString& t) { _trigger = t; }
  void setTimeframe(const QString& t) { _timeframe = t; }
  void setVersion(const QString& v) { _version = v; }
  void setPartNumber(int n);
  void setThreadID(int t);
  void setProcessID(int p);
  ProfileCostArray* totals() { return &_totals; }
  /* we get owner of the submapping */
  void setFixSubMapping(TraceSubMapping* sm) { _fixSubMapping = sm; }
  TraceSubMapping* fixSubMapping() { return _fixSubMapping; }

  // returns true if something changed
  bool activate(bool);
  bool isActive() { return _active; }

private:
  QFile* _file;
  QString _name;
  QString _descr;
  QString _trigger;
  QString _timeframe;
  QString _version;

  int _number, _tid, _pid;

  bool _active;

  // the totals line
  ProfileCostArray _totals;

  // submapping for all fix costs of this part
  TraceSubMapping* _fixSubMapping;
};


class TracePartList: public Q3PtrList<TracePart>
{
 public:
  QString names() const;
 protected:
  int compareItems ( Item item1, Item item2 );
};


/*-----------------------------------------------------------------
 * Classes for cost items summed up from multiple trace parts
 *-----------------------------------------------------------------
 */


/**
 * A jump from an instruction to another inside of a function
 */
class TraceInstrJump: public TraceJumpCost
{
public:
    TraceInstrJump(TraceInstr* instrFrom, TraceInstr* instrTo,
		   bool isCondJump);
    virtual ~TraceInstrJump();

    virtual QString name() const;

    virtual void update();

    TraceInstr* instrFrom() const { return _instrFrom; }
    TraceInstr* instrTo() const { return _instrTo; }
    bool isCondJump() const { return _isCondJump; }

    // part factory
    TracePartInstrJump* partInstrJump(TracePart*);

 private:
    TraceInstr *_instrFrom, *_instrTo;
    bool _isCondJump;
    // list of parts for this InstrJump
    TracePartInstrJump* _first;
};

class TraceInstrJumpList: public Q3PtrList<TraceInstrJump>
{
 public:
    TraceInstrJumpList() { _sortLow = true; }
    void setSortLow(bool s) { _sortLow = s; }

 protected:
  int compareItems ( Item item1, Item item2 );

 private:
  bool _sortLow;
};


/**
 * A jump from one line to another inside of a function.
 */
class TraceLineJump: public TraceJumpListCost
{
 public:
  TraceLineJump(TraceLine* lineFrom, TraceLine* lineTo,
		bool isCondJump);
  virtual ~TraceLineJump();

  virtual QString name() const;

  TraceLine* lineFrom() const { return _lineFrom; }
  TraceLine* lineTo() const { return _lineTo; }
  bool isCondJump() { return _isCondJump; }

  // part factory
  TracePartLineJump* partLineJump(TracePart*);

 protected:
  bool onlyActiveParts() { return true; }

 private:
  TraceLine *_lineFrom, *_lineTo;
  bool _isCondJump;
};


class TraceLineJumpList: public Q3PtrList<TraceLineJump>
{
 public:
    TraceLineJumpList() { _sortLow = true; }
    void setSortLow(bool s) { _sortLow = s; }

 protected:
  int compareItems ( Item item1, Item item2 );

 private:
  bool _sortLow;
};


/**
 * A call from an instruction of one function to another function
 */
class TraceInstrCall: public TraceCallListCost
{
 public:
  TraceInstrCall(TraceCall* call, TraceInstr* instr);
  virtual ~TraceInstrCall();

  virtual QString name() const;

  TraceInstr* instr() const { return _instr; }
  TraceCall* call() const { return _call; }

  // part factory
  TracePartInstrCall* partInstrCall(TracePart*, TracePartCall*);

 protected:
  bool onlyActiveParts() { return true; }

 private:
  TraceInstr* _instr;
  TraceCall* _call;
};


/**
 * A call from a line of one function to another function.
 */
class TraceLineCall: public TraceCallListCost
{
 public:
  TraceLineCall(TraceCall* call, TraceLine* line);
  virtual ~TraceLineCall();

  virtual QString name() const;

  TraceLine* line() const { return _line; }
  TraceCall* call() const { return _call; }

  // part factory
  TracePartLineCall* partLineCall(TracePart*, TracePartCall*);

 protected:
  bool onlyActiveParts() { return true; }

 private:
  TraceLine* _line;
  TraceCall* _call;
};


/**
 * A call from one to another function.
 * Consists of a list a TraceLineCalls
 */
class TraceCall: public TraceCallListCost
{
 public:
  TraceCall(TraceFunction* caller, TraceFunction* called);
  virtual ~TraceCall();

  virtual QString name() const;

  // calls a function itself?
  bool isRecursion() { return _caller == _called; }

  // return cycle number >0 if call is inside of a cycle
  int inCycle();
  // we need some special handling for cycle calls
  void update();

  void invalidateDynamicCost();

  // factories
  TracePartCall* partCall(TracePart*,
                          TracePartFunction*, TracePartFunction*);
  TraceLineCall* lineCall(TraceLine*);
  TraceInstrCall* instrCall(TraceInstr*);

  TraceFunction* caller(bool skipCycle=false) const;
  TraceFunction* called(bool skipCycle=false) const;
  QString callerName(bool skipCycle=false) const;
  QString calledName(bool skipCycle=false) const;
  const TraceLineCallList& lineCalls() const { return _lineCalls; }
  const TraceInstrCallList& instrCalls() const { return _instrCalls; }

  FixCallCost* setFirstFixCost(FixCallCost* fc)
    { FixCallCost* t = _firstFixCost; _firstFixCost = fc; return t; }

 protected:
  bool onlyActiveParts() { return true; }

 private:
  TraceInstrCallList _instrCalls;
  TraceLineCallList _lineCalls;
  TraceFunction* _caller;
  TraceFunction* _called;

  FixCallCost* _firstFixCost;
};


/**
 * A code instruction address of the program.
 * Consists of a list a TracePartInstr from different trace files
 * and a list of TraceInstrCalls if there are calls from this address.
 */
class TraceInstr: public TraceListCost
{
 public:
  TraceInstr();
  virtual ~TraceInstr();

  virtual QString name() const;
  QString prettyName() const;

  bool isValid() { return _addr != Addr(0); }

  // factories
  TracePartInstr* partInstr(TracePart* part,
			    TracePartFunction* partFunction);
  TraceInstrJump* instrJump(TraceInstr* to, bool isCondJump);

  void addInstrCall(TraceInstrCall*);

  Addr addr() const { return _addr; }
  TraceFunction* function() const { return _function; }
  TraceLine* line() const { return _line; }
  const TraceInstrJumpList& instrJumps() const { return _instrJumps; }
  const TraceInstrCallList& instrCalls() const { return _instrCalls; }
  bool hasCost(TraceEventType*);

  // only to be called after default constructor
  void setAddr(const Addr addr) { _addr = addr; }
  void setFunction(TraceFunction* f) { _function = f; }
  void setLine(TraceLine* l) { _line = l; }

 protected:
  bool onlyActiveParts() { return true; }

 private:
  Addr _addr;
  TraceFunction* _function;
  TraceLine* _line;

  TraceInstrJumpList _instrJumps;
  TraceInstrCallList _instrCalls;
};


/**
 * A source line of the program.
 * Consists of a list a TracePartLines from different trace files
 * and a list of TraceLineCalls if there are calls from this line.
 */
class TraceLine: public TraceListCost
{
public:
  TraceLine();
  virtual ~TraceLine();

  virtual QString name() const;
  QString prettyName() const;

  // factories
  TracePartLine* partLine(TracePart* part,
                          TracePartFunction* partFunction);
  TraceLineJump* lineJump(TraceLine* to, bool isCondJump);

  void addLineCall(TraceLineCall*);


  bool isValid() { return _sourceFile != 0; }
  bool hasCost(TraceEventType*);
  TraceFunctionSource* functionSource() const { return _sourceFile; }
  uint lineno() const { return _lineno; }
  const TraceLineCallList& lineCalls() const { return _lineCalls; }
  const TraceLineJumpList& lineJumps() const { return _lineJumps; }

  // only to be called after default constructor
  void setSourceFile(TraceFunctionSource* sf) { _sourceFile = sf; }
  void setLineno(uint lineno) { _lineno = lineno; }

 protected:
  bool onlyActiveParts() { return true; }

 private:
  TraceFunctionSource* _sourceFile;
  uint _lineno;

  TraceLineJumpList _lineJumps;
  TraceLineCallList _lineCalls;
};


/*
 * Base class for all costs which
 * represent "interesting" items or group of items
 * with settable name and inclusive cost
 */
class TraceCostItem: public TraceInclusiveListCost
{
 public:
  TraceCostItem(ProfileContext*);
  virtual ~TraceCostItem();

  virtual QString name() const { return _name; }
  virtual void setName(const QString& name) { _name = name; }

 protected:
  bool onlyActiveParts() { return true; }

 protected:
  QString _name;
};


/**
 * Cost of a source region.
 */
class TraceLineRegion: public TraceInclusiveListCost
{
public:
  TraceLineRegion(uint from, uint to, QString name);
  virtual ~TraceLineRegion();

  virtual void update();

  uint from() const { return _from; }
  uint to() const { return _to; }
  QString name() const { return _name; }

  // factories
  TracePartLine* partLineRegion(TracePart* part,
			    TracePartFunction* partFunction);
 private:
  uint _from, _to;
  QString _name;
};


/**
 * A container helper class for TraceFunction for source lines
 * where a function is implemented in.
 * With inlining, lines of the same function can come from
 * different source files.
 * An instance of this class holds all lines of one source file
 * for a function in a map
 */
class TraceFunctionSource: public ProfileCostArray
{
public:
  TraceFunctionSource(TraceFunction*, TraceFile*);
  virtual ~TraceFunctionSource();

  virtual QString name() const;

  // reimplementation for dependency map
  virtual void update();

  TraceFile* file() const { return _file; }
  TraceFunction* function() const { return _function; }
  uint firstLineno();
  uint lastLineno();
  TraceLineMap* lineMap();

  void invalidateDynamicCost();

  /* factories */
  TraceLine* line(uint lineno, bool createNew = true);
  TraceLineRegion* region(uint from, uint to, QString name,
		      bool createNew = true);

 private:
  TraceFile* _file;
  TraceFunction* _function;
  TraceLineMap* _lineMap;
  TraceLine* _line0;
  TraceLineRegionList* _regions;

  bool _lineMapFilled;
};


/**
 * For temporary assoziation of objects with TraceFunctions.
 * Used in coverage analysis and TreeMap drawing.
 */
class TraceAssoziation
{
 public:
  /**
   * Creates an invalid assoziation.
   */
  TraceAssoziation();
  virtual ~TraceAssoziation();

  // for runtime detection
  virtual int rtti() { return 0; }

  /**
   * Could we set the function assoziation to ourself?
   * This only can return false if this is a unique assoziation.
   */
  bool isAssoziated();

  /**
   * reset function to assoziate this object to.
   * returns true if assoziation could be established
   */
  bool setFunction(TraceFunction*);
  TraceFunction* function() { return _function; }

  void invalidate() { _valid = false; }
  bool isValid() { return _valid; }

  /**
   * Delete all assoziations in TraceFunctions of data with
   * rtti runtime info. rtti = 0: delete ALL assoziations.
   */
  static void clear(TraceData* data, int rtti);

  /**
   * Invalidate all assoziations in TraceFunctions of data with
   * rtti runtime info. rtti = 0: Invalidate ALL assoziations.
   */
  static void invalidate(TraceData* data, int rtti);

 protected:
  TraceFunction* _function;
  bool _valid;
};

typedef Q3PtrList<TraceAssoziation> TraceAssoziationList;

/**
 * A traced function
 *
 * References to functions are stored in
 *  (1) a function map in TraceData (by value)
 *  (2) a TraceClass
 */
class TraceFunction: public TraceCostItem
{
 public:
  TraceFunction();
  TraceFunction(TraceData* data, const QString& name,
                TraceClass* cls, TraceFile* file, TraceObject* object);
  virtual ~TraceFunction();

  virtual void update();

  // this invalidate all subcosts of function depending on
  // active status of parts
  void invalidateDynamicCost();

  void addCaller(TraceCall*);

  // factories
  TraceCall* calling(TraceFunction* called);
  TraceLine* line(TraceFile*, uint lineno, bool createNew = true);
  TraceInstr* instr(Addr addr, bool createNew = true);
  TracePartFunction* partFunction(TracePart*,
                                  TracePartFile*, TracePartObject*);

  /**
   * Returns empty string if location is fully unknown.
   * Use prettyLocation for single user-visible string.
   * A function can have a lot of code from different sources (inlined);
   * maxItems limits this list. Default is full list
   */
  QString location(int maxFiles = 0) const;

  QString prettyName() const;
  QString prettyLocation(int maxFiles = 0) const;
  QString prettyNameWithLocation(int maxFiles = 1) const;
  void addPrettyLocation(QString&, int maxFiles = 1) const;
  // type + name + location
  QString info() const;

  TraceClass* cls() const { return _cls; }
  TraceFile* file() const { return _file; }
  TraceObject* object() const { return _object; }
  // get the source file with lines from function declaration (not inlined)
  TraceFunctionSource* sourceFile(TraceFile* file = 0,
                                      bool createNew = false);
  const TraceFunctionSourceList& sourceFiles() const
    { return _sourceFiles; }
  TraceCallList callers(bool skipCycle=false) const;
  const TraceCallList& callings(bool skipCycle=false) const;

  Addr firstAddress() const;
  Addr lastAddress() const;
  TraceInstrMap* instrMap();

  // cost metrics
  SubCost calledCount();
  SubCost callingCount();
  QString prettyCalledCount();
  QString prettyCallingCount();
  int calledContexts();
  int callingContexts();

  // only to be called after default constructor
  void setFile(TraceFile* file) { _file = file; }
  void setObject(TraceObject* object) { _object = object; }
  void setClass(TraceClass* cls) { _cls = cls; }
  //void setMapIterator(TraceFunctionMap::Iterator it) { _myMapIterator = it; }

  // see TraceFunctionAssoziation
  void addAssoziation(TraceAssoziation* a);
  void removeAssoziation(TraceAssoziation* a);
  void removeAssoziation(int rtti, bool reallyDelete = true);
  void invalidateAssoziation(int rtti);
  TraceAssoziation* assoziation(int rtti);

  // cycles
  void setCycle(TraceFunctionCycle* c) { _cycle = c; }
  TraceFunctionCycle* cycle() { return _cycle; }
  bool isCycle();
  bool isCycleMember();
  void cycleReset();
  void cycleDFS(int d, int& pNo, TraceFunction** pTop);

 protected:
  TraceCallList _callers; // list of calls we are called from
  TraceCallList _callings; // list of calls we are calling (we are owner)
  TraceFunctionCycle* _cycle;

 private:
  bool isUniquePrefix(const QString&) const;
  //TraceFunctionMap::Iterator _myMapIterator;

  TraceClass* _cls;
  TraceObject* _object;
  TraceFile* _file;

  TraceFunctionSourceList _sourceFiles; // we are owner
  TraceInstrMap* _instrMap; // we are owner
  bool _instrMapFilled;

  // see TraceAssoziation
  TraceAssoziationList _assoziations;

  // for cycle detection
  int _cycleLow;
  TraceFunction* _cycleStackDown;

  // cached
  SubCost _calledCount, _callingCount;
  int _calledContexts, _callingContexts;
};


/**
 * A cycle of recursive calling functions.
 *
 * This is itself shown as a function
 */
class TraceFunctionCycle: public TraceFunction
{
 public:
  TraceFunctionCycle(TraceFunction*, int n);

  // this removes all members from this cycle
  void init();
  void add(TraceFunction*);
  // this sets up the cycle once members are added
  void setup();

  TraceFunction* base() const { return _base; }
  int cycleNo() const { return _cycleNo; }
  const TraceFunctionList& members() const { return _members; }

 private:
  TraceFunction* _base;
  int _cycleNo;

  TraceFunctionList _members;
};


/**
 * A C++ Class / Namespace
 *
 * If a function symbol has a prefix ending in "::",
 * the prefix is supposed to be a class/namespace specifier.
 * Without such a prefix, we put a symbol in the "(global)" namespace.
 */
class TraceClass: public TraceCostItem
{
 public:
  TraceClass();
  virtual ~TraceClass();

  virtual QString prettyName() const;

  void addFunction(TraceFunction*);
  const TraceFunctionList& functions() const { return _functions; }

  // part factory
  TracePartClass* partClass(TracePart*);

 private:
  TraceFunctionList _functions;
};



/**
 * A source file containing function definitions
 */
class TraceFile: public TraceCostItem
{
 public:
  TraceFile();
  virtual ~TraceFile();

  void setDirectory(const QString& dir);
  void resetDirectory() { _dir = QString(); }
  QString directory();

  void addFunction(TraceFunction*);
  void addSourceFile(TraceFunctionSource*);

  // without path
  QString shortName() const;
  QString prettyName() const;
  QString prettyLongName() const;
  const TraceFunctionList& functions() const { return _functions; }
  const TraceFunctionSourceList& sourceFiles() const
    { return _sourceFiles; }

  // part factory
  TracePartFile* partFile(TracePart*);

 private:
  TraceFunctionList _functions;
  TraceFunctionSourceList _sourceFiles;
  QString _dir;
};


/**
 * A object containing a text segment (shared lib/executable)
 * with defined functions
 */
class TraceObject: public TraceCostItem
{
 public:
  TraceObject();
  virtual ~TraceObject();

  void addFunction(TraceFunction*);

  virtual void setName(const QString& name);
  QString shortName() const { return _shortName; }
  QString prettyName() const;
  const TraceFunctionList& functions() const { return _functions; }

  // part factory
  TracePartObject* partObject(TracePart*);

 private:
  TraceFunctionList _functions;
  QString _shortName;
};



/**
 * This class holds profiling data of multiple tracefiles
 * generated with cachegrind on one command.
 *
 */
class TraceData: public ProfileCostArray
{
 public:
  TraceData(Logger* l = 0);
  TraceData(const QString& base);
  virtual ~TraceData();

  virtual TraceData* data() { return this; }
  virtual const TraceData* data() const { return this; }

  /**
   * Loads a trace file.
   *
   * This adjusts the TraceEventTypeMapping according to given cost types
   */
  void load(const QString&);

  /** returns true if something changed. These do NOT
   * invalidate the dynamic costs on a activation change,
   * i.e. all cost items depends on active parts.
   * This has to be done by the caller when true is returned by
   * calling invalidateDynamicCost().
   */
  bool activateParts(const TracePartList&);
  bool activateParts(TracePartList, bool active);
  bool activatePart(TracePart*, bool active);
  bool activateAll(bool active=true);

  TracePartList parts() const { return _parts; }
  TracePart* partWithName(const QString& name);

  // with path
  QString traceName() const { return _traceName; }

  // without path
  QString shortTraceName() const;
  QString activePartRange();

  TraceEventTypeMapping* mapping() { return &_mapping; }

  // memory pools
  FixPool* fixPool();
  DynPool* dynPool();

  // factories for object/file/class/function/line instances
  TraceObject* object(const QString& name);
  TraceFile* file(const QString& name);
  TraceClass* cls(const QString& fnName, QString& shortName);
  // function creation involves class creation if needed
  TraceFunction* function(const QString& name, TraceFile*, TraceObject*);
  // factory for function cycles
  TraceFunctionCycle* functionCycle(TraceFunction*);

  /**
   * Search for item with given name and highest subcost of given cost type.
   *
   * For some items, they will only be found if the parent cost is given:
   *  Instr, Line, Call  => need parent of type Function
   * For Function, a parent of type Obj/File/Class can be given, but
   * is not needed.
   */
  ProfileCostArray* search(ProfileContext::Type, QString,
		    TraceEventType* ct = 0, ProfileCostArray* parent = 0);

  // for pretty function names without signature if unique...
  TraceFunctionMap::Iterator functionIterator(TraceFunction*);
  TraceFunctionMap::ConstIterator functionBeginIterator() const;
  TraceFunctionMap::ConstIterator functionEndIterator() const;

  TraceObjectMap& objectMap() { return _objectMap; }
  TraceFileMap& fileMap() { return _fileMap; }
  TraceClassMap& classMap() { return _classMap; }
  TraceFunctionMap& functionMap() { return _functionMap; }

  const TraceFunctionCycleList& functionCycles() { return _functionCycles; }

  ProfileCostArray* callMax() { return &_callMax; }

  void setCommand(const QString& command) { _command = command; }
  QString command() const { return _command; }
  ProfileCostArray* totals() { return &_totals; }
  void setMaxThreadID(int tid) { _maxThreadID = tid; }
  int maxThreadID() const { return _maxThreadID; }
  void setMaxPartNumber(int n) { _maxPartNumber = n; }
  int maxPartNumber() const { return _maxPartNumber; }

  // reset all manually set directories for source files
  void resetSourceDirs();

  virtual void update();

  // invalidates all cost items dependant on active state of parts
  void invalidateDynamicCost();

  // cycle detection
  void updateFunctionCycles();
  void updateObjectCycles();
  void updateClassCycles();
  void updateFileCycles();
  bool inFunctionCycleUpdate() { return _inFunctionCycleUpdate; }

 private:
  void init();
  // add trace part: events from one trace file
  TracePart* addPart(const QString& dir, const QString& file);

  // for notification callbacks
  Logger* _logger;

  TracePartList _parts;

  // The mapping for all costs
  TraceEventTypeMapping _mapping;

  FixPool* _fixPool;
  DynPool* _dynPool;

  // always the trace totals (not dependent on active parts)
  ProfileCostArray _totals;
  int _maxThreadID;
  int _maxPartNumber;

  TraceObjectMap _objectMap;
  TraceClassMap _classMap;
  TraceFileMap _fileMap;
  TraceFunctionMap _functionMap;
  QString _command;
  QString _traceName;

  // Max of all costs of calls: This allows to see if the incl. cost can
  // be hidden for a cost type, as it is always the same as self cost
  ProfileCostArray _callMax;

  // cycles
  TraceFunctionCycleList _functionCycles;
  int _functionCycleCount;
  bool _inFunctionCycleUpdate;
};



#endif
