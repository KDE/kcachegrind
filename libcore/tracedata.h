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
#include <qmap.h>

#include "costitem.h"
#include "subcost.h"
#include "utils.h"
#include "addr.h"
#include "context.h"
#include "eventtype.h"

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
 * As there exists only one TraceData object for a traced program, it is the
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


class FixCost;
class FixCallCost;
class FixJump;
class FixPool;
class DynPool;
class Logger;

class ProfileCostArray;
class EventType;
class EventTypeSet;
class EventTypeMapping;
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

typedef QList<ProfileCostArray*> TraceCostList;
typedef QList<TraceJumpCost*> TraceJumpCostList;
typedef QList<TraceCallCost*> TraceCallCostList;
typedef QList<TraceInclusiveCost*> TraceInclusiveCostList;

typedef QList<TracePartCall*> TracePartCallList;
typedef QList<TracePartInstr*> TracePartInstrList;
typedef QList<TracePartLine*> TracePartLineList;
typedef QList<TracePartLineRegion*> TracePartLineRegionList;
typedef QList<TracePartFunction*> TracePartFunctionList;
typedef QList<TracePartInstrCall*> TracePartInstrCallList;
typedef QList<TracePartLineCall*> TracePartLineCallList;
typedef QList<TracePart*> TracePartList;

typedef QList<TraceInstr*> TraceInstrList;
typedef QList<TraceLine*> TraceLineList;
typedef QList<TraceInstrJump*> TraceInstrJumpList;
typedef QList<TraceLineJump*> TraceLineJumpList;
typedef QList<TraceInstrCall*> TraceInstrCallList;
typedef QList<TraceLineCall*> TraceLineCallList;
typedef QList<TraceCall*> TraceCallList;
typedef QList<TraceFile*> TraceFileList;
typedef QList<TraceLineRegion*> TraceLineRegionList;
typedef QList<TraceFunctionSource*> TraceFunctionSourceList;
typedef QList<TraceFunction*> TraceFunctionList;
typedef QList<TraceFunctionCycle*> TraceFunctionCycleList;
typedef QMap<QString, TraceObject> TraceObjectMap;
typedef QMap<QString, TraceClass> TraceClassMap;
typedef QMap<QString, TraceFile> TraceFileMap;
typedef QMap<QString, TraceFunction> TraceFunctionMap;
typedef QMap<uint, TraceLine> TraceLineMap;
typedef QMap<Addr, TraceInstr> TraceInstrMap;


/**
 * Cost of a (conditional) jump.
 */
class TraceJumpCost: public CostItem
{
 public:
    TraceJumpCost(ProfileContext*);
    virtual ~TraceJumpCost();

    // reimplementations for cost addition
    virtual QString costString(EventTypeSet* m);
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
  virtual QString costString(EventTypeSet* m);
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
  virtual QString costString(EventTypeSet* m);
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
  virtual QString costString(EventTypeSet* m);

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
  TracePart(TraceData*);
  virtual ~TracePart();

  virtual TracePart* part() { return this; }
  virtual const TracePart* part() const { return this; }

  QString shortName() const;
  QString prettyName() const;
  /// @return Name of the file this part was loaded from
  QString name() const { return _name; }
  QString description() const { return _descr; }
  QString trigger() const { return _trigger; }
  QString timeframe() const { return _timeframe; }
  QString version() const { return _version; }
  int partNumber() const { return _number; }
  int threadID() const { return _tid; }
  int processID() const { return _pid; }
  void setDescription(const QString& d) { _descr = d; }
  void setTrigger(const QString& t) { _trigger = t; }
  void setTimeframe(const QString& t) { _timeframe = t; }
  void setVersion(const QString& v) { _version = v; }
  void setName(const QString& n) { _name = n; }
  void setPartNumber(int n);
  void setThreadID(int t);
  void setProcessID(int p);
  ProfileCostArray* totals() { return &_totals; }
  /* passes ownership of mapping */
  void setEventMapping(EventTypeMapping* sm) { _eventTypeMapping = sm; }
  EventTypeMapping* eventTypeMapping() { return _eventTypeMapping; }

  // returns true if something changed
  bool activate(bool);
  bool isActive() const { return _active; }

  // for sorting
  bool operator<(const TracePart&) const;

private:
  QIODevice* _file;
  QString _name;
  QString _descr;
  QString _trigger;
  QString _timeframe;
  QString _version;

  int _number, _tid, _pid;

  bool _active;

  // the totals line
  ProfileCostArray _totals;

  // event type mapping for all fix costs of this part
  EventTypeMapping* _eventTypeMapping;
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
  bool hasCost(EventType*);

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
  bool hasCost(EventType*);
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

typedef QList<TraceAssoziation*> TraceAssoziationList;

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
  QString formattedName() const;
  static QString prettyEmptyName();
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
  static QString prettyEmptyName();

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
  static QString prettyEmptyName();
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

  void setDirectory(const QString& dir);
  void resetDirectory() { _dir = QString(); }
  QString directory();

  void addFunction(TraceFunction*);

  QString shortName() const;
  QString prettyName() const;
  static QString prettyEmptyName();
  const TraceFunctionList& functions() const { return _functions; }

  // part factory
  TracePartObject* partObject(TracePart*);

 private:
  TraceFunctionList _functions;
  QString _dir;
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
  TraceData(QIODevice* file, const QString& filename);
  virtual ~TraceData();

  virtual TraceData* data() { return this; }
  virtual const TraceData* data() const { return this; }

  /**
   * Loads a trace file.
   *
   * This adjusts the EventTypeSet according to given cost types.
   * Returns the number of parts loaded
   */
  int load(const QString&);
  int load(QIODevice*, const QString&);

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

  // to be used by loader
  void addPart(TracePart*);

  TracePartList parts() const { return _parts; }
  TracePart* partWithName(const QString& name);

  // with path
  QString traceName() const { return _traceName; }

  // without path
  QString shortTraceName() const;
  QString activePartRange();

  EventTypeSet* eventTypes() { return &_eventTypes; }

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
		    EventType* ct = 0, ProfileCostArray* parent = 0);

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
  // add profile parts from one file
  int internalLoad(QIODevice* file, const QString& filename);

  // for notification callbacks
  Logger* _logger;

  TracePartList _parts;

  // The set for all costs
  EventTypeSet _eventTypes;

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
