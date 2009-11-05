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


#include "tracedata.h"

#include <errno.h>
#include <stdlib.h>

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include "logger.h"
#include "loader.h"
#include "globalconfig.h"
#include "utils.h"
#include "fixcost.h"


#define TRACE_DEBUG      0
#define TRACE_ASSERTIONS 0




//---------------------------------------------------
// TraceJumpCost

TraceJumpCost::TraceJumpCost(ProfileContext* c)
    :CostItem(c)
{
    TraceJumpCost::clear();
}

TraceJumpCost::~TraceJumpCost()
{}

SubCost TraceJumpCost::executedCount()
{
    if (_dirty) update();

    return _executedCount;
}

SubCost TraceJumpCost::followedCount()
{
    if (_dirty) update();

    return _followedCount;
}

QString TraceJumpCost::costString(EventTypeSet*)
{
  if (_dirty) update();

  return QString("%1/%2")
      .arg(_followedCount.pretty())
      .arg(_executedCount.pretty());
}

void TraceJumpCost::clear()
{
    _followedCount = 0;
    _executedCount = 0;
}

void TraceJumpCost::addCost(TraceJumpCost* item)
{
    if (item->_dirty) item->update();

    _followedCount += item->followedCount();
    _executedCount += item->executedCount();
}



//---------------------------------------------------
// TraceCallCost

TraceCallCost::TraceCallCost(ProfileContext* context)
    : ProfileCostArray(context)
{
  _callCount = 0;
}

TraceCallCost::~TraceCallCost()
{}


QString TraceCallCost::costString(EventTypeSet* m)
{
  return QString("%1, Calls %2")
      .arg(ProfileCostArray::costString(m))
      .arg(_callCount.pretty());
}

QString TraceCallCost::prettyCallCount()
{
  return _callCount.pretty();
}

void TraceCallCost::clear()
{
  _callCount = 0;
  ProfileCostArray::clear();
}

SubCost TraceCallCost::callCount()
{
  if (_dirty) update();

  return _callCount;
}

void TraceCallCost::addCallCount(SubCost c)
{
  _callCount += c;

  invalidate();
}


//---------------------------------------------------
// TraceInclusiveCost

TraceInclusiveCost::TraceInclusiveCost(ProfileContext* context)
    : ProfileCostArray(context), _inclusive(context)
{}

TraceInclusiveCost::~TraceInclusiveCost()
{}

QString TraceInclusiveCost::costString(EventTypeSet* m)
{
  return QString("%1, Inclusive %2")
    .arg(ProfileCostArray::costString(m))
    .arg(_inclusive.costString(m));
}

void TraceInclusiveCost::clear()
{
  _inclusive.clear();
  ProfileCostArray::clear();
}

ProfileCostArray* TraceInclusiveCost::inclusive()
{
  if (_dirty) update();

  return &_inclusive;
}

void TraceInclusiveCost::addInclusive(ProfileCostArray* c)
{
  _inclusive.addCost(c);

  invalidate();
}


//---------------------------------------------------
// TraceListCost

TraceListCost::TraceListCost(ProfileContext* context)
    : ProfileCostArray(context)
{
  _lastDep = 0;
}

TraceListCost::~TraceListCost()
{}

void TraceListCost::addDep(ProfileCostArray* dep)
{
#if TRACE_ASSERTIONS
  if (_deps.findRef(dep)>=0) {
    qDebug("addDep: %s already in list!",
           qPrintable(dep->fullName()));
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(dep->fullName()),
         _deps.count());
#endif
}

ProfileCostArray* TraceListCost::findDepFromPart(TracePart* part)
{
  if (_lastDep && _lastDep->part() == part)
    return _lastDep;

  ProfileCostArray* dep;
  for (dep = _deps.first(); dep; dep = _deps.next())
    if (dep->part() == part) {
      _lastDep = dep;
      return dep;
    }
  return 0;
}


void TraceListCost::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("update %s (count %d)",
         qPrintable( fullName() ), _deps.count());
#endif

  clear();
  ProfileCostArray* item;
  for (item = _deps.first(); item; item = _deps.next()) {
    if (onlyActiveParts())
      if (!item->part() || !item->part()->isActive()) continue;

    addCost(item);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}



//---------------------------------------------------
// TraceJumpListCost

TraceJumpListCost::TraceJumpListCost(ProfileContext* context)
    : TraceJumpCost(context)
{
  _lastDep = 0;
}

TraceJumpListCost::~TraceJumpListCost()
{}

void TraceJumpListCost::addDep(TraceJumpCost* dep)
{
#if TRACE_ASSERTIONS
  if (_deps.findRef(dep)>=0) {
    qDebug("addDep: %s already in list!",
           qPrintable(dep->fullName()));
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(dep->fullName()),
         _deps.count());
#endif
}

TraceJumpCost* TraceJumpListCost::findDepFromPart(TracePart* part)
{
  if (_lastDep && _lastDep->part() == part)
    return _lastDep;

  TraceJumpCost* dep;
  for (dep = _deps.first(); dep; dep = _deps.next())
    if (dep->part() == part) {
      _lastDep = dep;
      return dep;
    }
  return 0;
}


void TraceJumpListCost::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("update %s (count %d)",
         qPrintable( fullName() ), _deps.count());
#endif

  clear();
  TraceJumpCost* item;
  for (item = _deps.first(); item; item = _deps.next()) {
    if (onlyActiveParts())
      if (!item->part() || !item->part()->isActive()) continue;

    addCost(item);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}



//---------------------------------------------------
// TraceCallListCost

TraceCallListCost::TraceCallListCost(ProfileContext* context)
    : TraceCallCost(context)
{
  _lastDep = 0;
}

TraceCallListCost::~TraceCallListCost()
{}

void TraceCallListCost::addDep(TraceCallCost* dep)
{
#if TRACE_ASSERTIONS
  if (_deps.findRef(dep)>=0) {
    qDebug("addDep: %s already in list!",
           qPrintable(dep->fullName()));
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(dep->fullName()),
         _deps.count());
#endif
}

TraceCallCost* TraceCallListCost::findDepFromPart(TracePart* part)
{
  if (_lastDep && _lastDep->part() == part)
    return _lastDep;

  TraceCallCost* dep;
  for (dep = _deps.first(); dep; dep = _deps.next())
    if (dep->part() == part) {
      _lastDep = dep;
      return dep;
    }
  return 0;
}


void TraceCallListCost::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("update %s (count %d)",
         qPrintable( fullName() ), _deps.count());
#endif

  /* Without dependent cost items, assume fixed costs,
   * i.e. do not change cost */
  if (_deps.count()>0) {
    clear();
    TraceCallCost* item;
    for (item = _deps.first(); item; item = _deps.next()) {
      if (onlyActiveParts())
	if (!item->part() || !item->part()->isActive()) continue;

      addCost(item);
      addCallCount(item->callCount());
    }
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}


//---------------------------------------------------
// TraceInclusiveListCost

TraceInclusiveListCost::TraceInclusiveListCost(ProfileContext* context)
    : TraceInclusiveCost(context)
{
  _lastDep = 0;
}

TraceInclusiveListCost::~TraceInclusiveListCost()
{}


void TraceInclusiveListCost::addDep(TraceInclusiveCost* dep)
{
#if TRACE_ASSERTIONS
  if (_deps.findRef(dep)>=0) {
    qDebug("addDep: %s already in list!",
           qPrintable(dep->fullName()));
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(dep->fullName()),
         _deps.count());
#endif
}

TraceInclusiveCost* TraceInclusiveListCost::findDepFromPart(TracePart* part)
{
  if (_lastDep && _lastDep->part() == part)
    return _lastDep;

  TraceInclusiveCost* dep;
  for (dep = _deps.first(); dep; dep = _deps.next())
    if (dep->part() == part) {
      _lastDep = dep;
      return dep;
    }
  return 0;
}

void TraceInclusiveListCost::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("update %s (count %d)",
         qPrintable( fullName() ), _deps.count());
#endif

  clear();
  TraceInclusiveCost* item;
  for (item = _deps.first(); item; item = _deps.next()) {
    if (onlyActiveParts())
      if (!item->part() || !item->part()->isActive()) continue;

    addCost(item);
    addInclusive(item->inclusive());
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}



//---------------------------------------------------
// TracePartInstrJump

TracePartInstrJump::TracePartInstrJump(TraceInstrJump* instrJump,
				       TracePartInstrJump* next)
    : TraceJumpCost(ProfileContext::context(ProfileContext::PartInstrJump))
{
  _dep = instrJump;
  _next = next;
}

TracePartInstrJump::~TracePartInstrJump()
{}


//---------------------------------------------------
// TracePartInstrCall

TracePartInstrCall::TracePartInstrCall(TraceInstrCall* instrCall)
  : TraceCallCost(ProfileContext::context(ProfileContext::PartInstrCall))
{
  _dep = instrCall;
}

TracePartInstrCall::~TracePartInstrCall()
{}



//---------------------------------------------------
// TracePartInstr

TracePartInstr::TracePartInstr(TraceInstr* instr)
  : ProfileCostArray(ProfileContext::context(ProfileContext::PartInstr))
{
  _dep = instr;
}

TracePartInstr::~TracePartInstr()
{}



//---------------------------------------------------
// TracePartLineJump

TracePartLineJump::TracePartLineJump(TraceLineJump* lineJump)
  : TraceJumpCost(ProfileContext::context(ProfileContext::PartLineJump))
{
  _dep = lineJump;
}

TracePartLineJump::~TracePartLineJump()
{}


//---------------------------------------------------
// TracePartLineCall

TracePartLineCall::TracePartLineCall(TraceLineCall* lineCall)
  : TraceCallCost(ProfileContext::context(ProfileContext::PartLineCall))
{
  _dep = lineCall;
}

TracePartLineCall::~TracePartLineCall()
{}


//---------------------------------------------------
// TracePartLine

TracePartLine::TracePartLine(TraceLine* line)
  : ProfileCostArray(ProfileContext::context(ProfileContext::PartLine))
{
  _dep = line;
}

TracePartLine::~TracePartLine()
{}




//---------------------------------------------------
// TracePartCall

TracePartCall::TracePartCall(TraceCall* call)
  : TraceCallListCost(ProfileContext::context(ProfileContext::PartCall))
{
  _dep = call;

  _firstFixCallCost = 0;
}

TracePartCall::~TracePartCall()
{}

bool TracePartCall::isRecursion()
{
  return call()->isRecursion();
}

void TracePartCall::update()
{
#if !USE_FIXCOST
  TraceCallListCost::update();
#else

  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("update %s", qPrintable( fullName() ));
#endif

  /* Without dependent cost items, assume fixed costs,
   * i.e. do not change cost */
  if (_firstFixCallCost) {
    clear();
    FixCallCost* item;
    for (item = _firstFixCallCost; item; item = item->nextCostOfPartCall())
      item->addTo(this);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif

#endif // USE_FIXCOST
}


//---------------------------------------------------
// TracePartFunction

TracePartFunction::TracePartFunction(TraceFunction* function,
                                     TracePartObject* partObject,
				     TracePartFile *partFile)
  : TraceInclusiveCost(ProfileContext::context(ProfileContext::PartFunction))
{
  _dep = function;
  _partObject = partObject;
  _partFile = partFile;
  _partClass = 0;

  _calledCount     = 0;
  _callingCount    = 0;
  _calledContexts  = 0;
  _callingContexts = 0;

  _firstFixCost = 0;
  _firstFixJump = 0;
}

TracePartFunction::~TracePartFunction()
{}

QString TracePartFunction::prettyCalledCount()
{
  return _calledCount.pretty();
}

QString TracePartFunction::prettyCallingCount()
{
  return _callingCount.pretty();
}

QString TracePartFunction::costString(EventTypeSet* m)
{
  update();

  QString res = TraceInclusiveCost::costString(m);
  res += QString(", called from %1: %2")
         .arg(_calledContexts).arg(prettyCalledCount());
  res += QString(", calling from %1: %2")
         .arg(_callingContexts).arg(prettyCallingCount());

  return res;
}


void TracePartFunction::addPartInstr(TracePartInstr* ref)
{
#if TRACE_ASSERTIONS
  if (_partInstr.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartInstr: %s already in list!",
           qPrintable(ref->name()));
    return;
  }
#endif

  _partInstr.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(ref->fullName()),
         _partInstr.count());
#endif
}


void TracePartFunction::addPartLine(TracePartLine* ref)
{
#if TRACE_ASSERTIONS
  if (_partLines.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartLine: %s already in list!",
           qPrintable(ref->name()));
    return;
  }
#endif

  _partLines.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(ref->fullName()),
         _partLines.count());
#endif
}


void TracePartFunction::addPartCaller(TracePartCall* ref)
{
#if TRACE_ASSERTIONS
  if (_partCallers.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartCaller: %s already in list!",
           qPrintable(ref->name()));
    return;
  }
#endif

  _partCallers.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Caller\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(ref->fullName()),
         _partCallers.count());
#endif
}


void TracePartFunction::addPartCalling(TracePartCall* ref)
{
#if TRACE_ASSERTIONS
  if (_partCallings.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartCalling: %s already in list!",
           qPrintable(ref->name()));
    return;
  }
#endif

  _partCallings.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Calling\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(ref->fullName()),
         _partCallings.count());
#endif
}

SubCost TracePartFunction::calledCount()
{
  if (_dirty) update();

  return _calledCount;
}

int TracePartFunction::calledContexts()
{
  if (_dirty) update();

  return _calledContexts;
}

SubCost TracePartFunction::callingCount()
{
  if (_dirty) update();

  return _callingCount;
}


int TracePartFunction::callingContexts()
{
  if (_dirty) update();

  return _callingContexts;
}


void TracePartFunction::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("TracePartFunction::update %s (Callers %d, Callings %d, lines %d)",
         name().ascii(), _partCallers.count(), _partCallings.count(),
         _partLines.count());
#endif

  _calledCount     = 0;
  _callingCount    = 0;
  _calledContexts  = 0;
  _callingContexts = 0;

  // calculate additional cost metrics
  TracePartCall *caller, *calling;
  for (caller=_partCallers.first();caller;caller=_partCallers.next()) {

    // FIXME
    if (caller->subCost(0)>0)
      _calledContexts++;

    SubCost c = caller->callCount();
    if (c>0) {
      _calledCount += c;
    }
  }
  for (calling=_partCallings.first();calling;calling=_partCallings.next()) {
    // FIXME
    if (calling->subCost(0)>0)
      _callingContexts++;

    SubCost c = calling->callCount();
    if (c>0) {
      _callingCount += c;
    }
  }

  // self cost
#if !USE_FIXCOST
  if (_partLines.count()>0) {
    ProfileCostArray::clear();

    TracePartLine* line;
    for (line = _partLines.first(); line; line = _partLines.next())
      addCost(line);
  }
#else
  if (_firstFixCost) {
    ProfileCostArray::clear();

    FixCost* item;
    for (item = _firstFixCost; item; item = item->nextCostOfPartFunction())
      item->addTo(this);
  }
#endif


  /* There are two possibilities to calculate inclusive cost:
   * 1) sum of call costs to this function
   * 2) sum of call costs from this function + self cost
   *
   * 1) is wrong if a function was called spontaneous, but also by a call.
   * This eventually can happen with thread/process startup functions,
   * and signal handlers.
   *
   * 2) is wrong with "skipped PLT" and the calltree skin, because
   *    cost of PLT is attributed to called function (?)
   *
   * For now, do 1) if there are callers, otherwise 2).
   * Should this be fixed to take the maximum of 1) and 2) ?
   */
  _inclusive.clear();
  if (_calledCount>0) {
      // inclusive cost: if possible, use caller sums
    for (caller=_partCallers.first();caller;caller=_partCallers.next()) {
      // detect simple recursion (no cycle)
      if (caller->isRecursion()) continue;

      addInclusive(caller);
    }
  }
  else {
    // without caller info, use calling sum + line costs
    for (calling=_partCallings.first();calling;calling=_partCallings.next()) {
      // detect simple recursion (no cycle)
      if (calling->isRecursion()) continue;

      addInclusive(calling);
    }
    _dirty = false; // do not recurse!
    addInclusive(this);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}



//---------------------------------------------------
// TracePartClass

TracePartClass::TracePartClass(TraceClass* cls)
    : TraceInclusiveListCost(ProfileContext::context(ProfileContext::PartClass))
{
  _dep = cls;
}

TracePartClass::~TracePartClass()
{}

QString TracePartClass::prettyName() const
{
  return QString("%1 from %2")
    .arg( _dep->name().isEmpty() ? QString("(global)") : _dep->name())
    .arg(part()->name());
}

//---------------------------------------------------
// TracePartFile

TracePartFile::TracePartFile(TraceFile* file)
    : TraceInclusiveListCost(ProfileContext::context(ProfileContext::PartFile))
{
  _dep = file;
}

TracePartFile::~TracePartFile()
{}


//---------------------------------------------------
// TracePartObject

TracePartObject::TracePartObject(TraceObject* object)
  : TraceInclusiveListCost(ProfileContext::context(ProfileContext::PartObject))
{
  _dep = object;
}

TracePartObject::~TracePartObject()
{}




//---------------------------------------------------
// TraceInstrJump

TraceInstrJump::TraceInstrJump(TraceInstr* instrFrom, TraceInstr* instrTo,
			       bool isCondJump)
    : TraceJumpCost(ProfileContext::context(ProfileContext::InstrJump))
{
  _first = 0;

  _instrFrom = instrFrom;
  _instrTo = instrTo;
  _isCondJump = isCondJump;
}

TraceInstrJump::~TraceInstrJump()
{
  // we are the owner of the TracePartInstrJump's generated in our factory
  TracePartInstrJump* item = _first, *next;
  while(item) {
    next = item->next();
    delete item;
    item = next;
  }
}

TracePartInstrJump* TraceInstrJump::partInstrJump(TracePart* part)
{
  static TracePartInstrJump* item = 0;

  // shortcut
  if (item && (item->instrJump()==this) && (item->part() == part)) return item;

  for(item = _first; item; item = item->next())
    if (item->part() == part) break;

  if (!item) {
    item = new TracePartInstrJump(this, _first);
    item->setPosition(part);
    _first = item;
  }
  return item;
}

void TraceInstrJump::update()
{
  if (!_dirty) return;

  clear();
  TracePartInstrJump* item;
  for (item = _first; item; item = item->next()) {
    if (!item->part() || !item->part()->isActive()) continue;

    addCost(item);
  }
  _dirty = false;

#if TRACE_DEBUG
  qDebug("updated %s", qPrintable( fullName() ));
#endif

#if TRACE_DEBUG
  qDebug("   > %s", qPrintable(costString(0)));
#endif
}

QString TraceInstrJump::name() const
{
  return QString("jump at 0x%1 to 0x%2")
    .arg(_instrFrom->addr().toString())
    .arg(_instrTo->addr().toString());
}


//---------------------------------------------------
// TraceInstrJumpList


int TraceInstrJumpList::compareItems ( Item item1, Item item2 )
{
    TraceInstrJump* ij1 = (TraceInstrJump*) item1;
    TraceInstrJump* ij2 = (TraceInstrJump*) item2;

    Addr addr1Low  = ij1->instrFrom()->addr();
    Addr addr2Low  = ij2->instrFrom()->addr();
    Addr addr1High = ij1->instrTo()->addr();
    Addr addr2High = ij2->instrTo()->addr();
    Addr t;

    if (addr1Low > addr1High) {
	t = addr1Low;
	addr1Low = addr1High;
	addr1High = t;
    }

    if (addr2Low > addr2High) {
	t = addr2Low;
	addr2Low = addr2High;
	addr2High = t;
    }

    if (_sortLow) {
	// we sort according to smallest instruction address
	if (addr1Low != addr2Low) return (addr1Low > addr2Low) ? 1:-1;
	// jump ends come before jump starts
	if (addr1Low == ij1->instrTo()->addr()) return -1;
	if (addr2Low == ij2->instrTo()->addr()) return 1;
	return (addr1High > addr2High) ? 1:-1;
    }

    // we sort according to highest instruction address
    if (addr1High != addr2High) return (addr1High > addr2High) ? 1:-1;
    // jump ends come before jump starts
    if (addr1High == ij1->instrTo()->addr()) return -1;
    if (addr2High == ij2->instrTo()->addr()) return 1;
    return (addr1Low > addr2Low) ? 1:-1;
}


//---------------------------------------------------
// TraceLineJump

TraceLineJump::TraceLineJump(TraceLine* lineFrom, TraceLine* lineTo,
			     bool isCondJump)
    : TraceJumpListCost(ProfileContext::context(ProfileContext::LineJump))
{
  // we are the owner of TracePartLineJump's generated in our factory
  _deps.setAutoDelete(true);

  _lineFrom = lineFrom;
  _lineTo   = lineTo;
  _isCondJump = isCondJump;
}

TraceLineJump::~TraceLineJump()
{}


TracePartLineJump* TraceLineJump::partLineJump(TracePart* part)
{
  TracePartLineJump* item = (TracePartLineJump*) findDepFromPart(part);
  if (!item) {
    item = new TracePartLineJump(this);
    item->setPosition(part);
    addDep(item);
  }
  return item;
}


QString TraceLineJump::name() const
{
  return QString("jump at %1 to %2")
      .arg(_lineFrom->prettyName())
      .arg(_lineTo->prettyName());
}


//---------------------------------------------------
// TraceLineJumpList


int TraceLineJumpList::compareItems ( Item item1, Item item2 )
{
    TraceLineJump* lj1 = (TraceLineJump*) item1;
    TraceLineJump* lj2 = (TraceLineJump*) item2;

    uint line1Low  = lj1->lineFrom()->lineno();
    uint line2Low  = lj2->lineFrom()->lineno();
    uint line1High = lj1->lineTo()->lineno();
    uint line2High = lj2->lineTo()->lineno();
    uint t;

    if (line1Low > line1High) {
	t = line1Low; line1Low = line1High; line1High = t;
    }
    if (line2Low > line2High) {
	t = line2Low; line2Low = line2High; line2High = t;
    }

    if (_sortLow) {
	// we sort according to smallest line number
	if (line1Low != line2Low) return line1Low - line2Low;
	// jump ends come before jump starts
	if (line1Low == lj1->lineTo()->lineno()) return -1;
	if (line2Low == lj2->lineTo()->lineno()) return 1;
	return line1High - line2High;
    }

    // we sort according to highest line number
    if (line1High != line2High) return line1High - line2High;
    // jump ends come before jump starts
    if (line1High == lj1->lineTo()->lineno()) return -1;
    if (line2High == lj2->lineTo()->lineno()) return 1;
    return line1Low - line2Low;
}


//---------------------------------------------------
// TraceInstrCall

TraceInstrCall::TraceInstrCall(TraceCall* call, TraceInstr* instr)
    : TraceCallListCost(ProfileContext::context(ProfileContext::InstrCall))
{
  // we are the owner of TracePartInstrCall's generated in our factory
  _deps.setAutoDelete(true);

  _call  = call;
  _instr = instr;
}

TraceInstrCall::~TraceInstrCall()
{}


TracePartInstrCall* TraceInstrCall::partInstrCall(TracePart* part,
						  TracePartCall*)
{
  TracePartInstrCall* item = (TracePartInstrCall*) findDepFromPart(part);
  if (!item) {
    item = new TracePartInstrCall(this);
    item->setPosition(part);
    addDep(item);
    // instruction calls are not registered in function calls
    // as together with line calls calls are duplicated
    //partCall->addDep(item);
  }
  return item;
}


QString TraceInstrCall::name() const
{
  return QString("%1 at %2").arg(_call->name()).arg(_instr->name());
}


//---------------------------------------------------
// TraceLineCall

TraceLineCall::TraceLineCall(TraceCall* call, TraceLine* line)
    : TraceCallListCost(ProfileContext::context(ProfileContext::LineCall))
{
  // we are the owner of TracePartLineCall's generated in our factory
  _deps.setAutoDelete(true);

  _call = call;
  _line = line;
}

TraceLineCall::~TraceLineCall()
{}


TracePartLineCall* TraceLineCall::partLineCall(TracePart* part,
                                               TracePartCall* partCall)
{
  TracePartLineCall* item = (TracePartLineCall*) findDepFromPart(part);
  if (!item) {
    item = new TracePartLineCall(this);
    item->setPosition(part);
    addDep(item);
    partCall->addDep(item);
  }
  return item;
}


QString TraceLineCall::name() const
{
  return QString("%1 at %2").arg(_call->name()).arg(_line->name());
}


//---------------------------------------------------
// TraceCall

TraceCall::TraceCall(TraceFunction* caller, TraceFunction* called)
    : TraceCallListCost(ProfileContext::context(ProfileContext::Call))
{
  // we are the owner of all items generated in our factory
  _deps.setAutoDelete(true);
  _lineCalls.setAutoDelete(true);

  _caller = caller;
  _called = called;
}


TraceCall::~TraceCall()
{}

TracePartCall* TraceCall::partCall(TracePart* part,
                                   TracePartFunction* partCaller,
                                   TracePartFunction* partCalling)
{
  TracePartCall* item = (TracePartCall*) findDepFromPart(part);
  if (!item) {
    item = new TracePartCall(this);
    item->setPosition(part);
    addDep(item);
    partCaller->addPartCalling(item);
    partCalling->addPartCaller(item);
  }
  return item;
}

TraceInstrCall* TraceCall::instrCall(TraceInstr* i)
{
  TraceInstrCall* icall;
  for (icall=_instrCalls.first();icall;icall=_instrCalls.next())
    if (icall->instr() == i)
      break;

  if (!icall) {
    icall = new TraceInstrCall(this, i);

    _instrCalls.append(icall);
    invalidate();

#if TRACE_DEBUG
    qDebug("Created %s [TraceCall::instrCall]", qPrintable(icall->fullName()));
#endif
    i->addInstrCall(icall);
  }
  return icall;
}


TraceLineCall* TraceCall::lineCall(TraceLine* l)
{
  TraceLineCall* lcall;
  for (lcall=_lineCalls.first();lcall;lcall=_lineCalls.next())
    if (lcall->line() == l)
      break;

  if (!lcall) {
    lcall = new TraceLineCall(this, l);

    _lineCalls.append(lcall);
    invalidate();

#if TRACE_DEBUG
    qDebug("Created %s [TraceCall::lineCall]", qPrintable(lcall->fullName()));
#endif
    l->addLineCall(lcall);
  }
  return lcall;
}


void TraceCall::invalidateDynamicCost()
{
  TraceLineCall* lc;
  for (lc=_lineCalls.first();lc;lc=_lineCalls.next())
    lc->invalidate();

  TraceInstrCall* ic;
  for (ic=_instrCalls.first();ic;ic=_instrCalls.next())
    ic->invalidate();

  invalidate();
}


QString TraceCall::name() const
{
  return QString("%1 => %2")
    .arg(_caller->name())
    .arg(_called->name());
}

int TraceCall::inCycle()
{
  if (!_caller || !_called) return 0;
  if (!_caller->cycle()) return 0;
  if (_caller == _caller->cycle()) return 0;
  if (_caller->cycle() != _called->cycle()) return 0;

  return _caller->cycle()->cycleNo();
}

void TraceCall::update()
{
  if (!_dirty) return;

  // special handling for cycles
  if (_caller && _caller->cycle() && _caller==_caller->cycle()) {

    // we have no part calls: use inclusive cost of called function
    clear();
    if (_called)
      addCost(_called->inclusive());
    _dirty = false;
    return;
  }

  TraceCallListCost::update();
}

TraceFunction* TraceCall::caller(bool /*skipCycle*/) const
{
  return _caller;
}

TraceFunction* TraceCall::called(bool skipCycle) const
{
  if (!skipCycle && _called) {
    // if this is a call to a cycle member from outside of the cycle,
    // fake it to be a call to the whole cycle
    if (_called->cycle() && _caller &&
        (_caller->cycle() != _called->cycle()))
      return _called->cycle();
  }

  return _called;
}

QString TraceCall::callerName(bool skipCycle) const
{
  if (!_caller) return QObject::tr("(no caller)");

  if (!skipCycle) {
      // if this call goes into a cycle, add the entry function
      TraceFunctionCycle* c = _called->cycle();
      if (c && _caller && (_caller->cycle() != c)) {
	  QString via = _called->prettyName();
	  return QObject::tr("%1 via %2").arg(_caller->prettyName()).arg(via);
      }
  }

  return _caller->prettyName();
}

QString TraceCall::calledName(bool skipCycle) const
{
  if (!_called) return QObject::tr("(no callee)");

  if (!skipCycle) {
      // if this call goes into a cycle, add the entry function
      TraceFunctionCycle* c = _called->cycle();
      if (c && _caller && (_caller->cycle() != c)) {
	  // HACK to get rid of cycle postfix...
	  _called->setCycle(0);
	  QString via = _called->prettyName();
	  _called->setCycle(c);
	  return QObject::tr("%1 via %2").arg(c->name()).arg(via);
      }
  }
  return _called->prettyName();
}


//---------------------------------------------------
// TraceInstr

TraceInstr::TraceInstr()
    : TraceListCost(ProfileContext::context(ProfileContext::Instr))
{
  // we are the owner of TracePartInstr's generated in our factory
  _deps.setAutoDelete(true);
  _instrJumps.setAutoDelete(true);

  _addr = 0;
  _line = 0;
  _function = 0;
}

TraceInstr::~TraceInstr()
{}

bool TraceInstr::hasCost(EventType* ct)
{
    bool res = subCost(ct) > 0;
    if (!res) {
	TraceInstrCall* ic;
	for(ic=_instrCalls.first();ic;ic=_instrCalls.next())
	    if (ic->subCost(ct) > 0) break;
	res = (ic != 0);
	if (!res) {
	    TraceInstrJump* ij;
	    for(ij=_instrJumps.first();ij;ij=_instrJumps.next())
		if (ij->executedCount() > 0) break;
	    res = (ij != 0);
	}
    }

    return res;
}

TracePartInstr* TraceInstr::partInstr(TracePart* part,
				      TracePartFunction* partFunction)
{
  TracePartInstr* item = (TracePartInstr*) findDepFromPart(part);
  if (!item) {
    item = new TracePartInstr(this);
    item->setPosition(part);
    addDep(item);
    //part->addDep(item);
    partFunction->addPartInstr(item);
  }
  return item;
}

TraceInstrJump* TraceInstr::instrJump(TraceInstr* to, bool isJmpCond)
{
  TraceInstrJump* jump;
  for (jump=_instrJumps.first();jump;jump=_instrJumps.next())
    if (jump->instrTo() == to)
      break;

  if (!jump) {
    jump = new TraceInstrJump(this, to, isJmpCond);

    _instrJumps.append(jump);
  }
  return jump;
}



void TraceInstr::addInstrCall(TraceInstrCall* instrCall)
{
#if TRACE_ASSERTIONS
  if (_instrCalls.findRef(instrCall)>=0) return;

  if (instrCall->instr() != this) {
    qDebug("Can not add instruction call to another instruction!");
    return;
  }
#endif

  _instrCalls.append(instrCall);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ),
         qPrintable(instrCall->fullName()), _instrCalls.count());
#endif
}


QString TraceInstr::name() const
{
    return QString("0x%1").arg(_addr.toString());
}

QString TraceInstr::prettyName() const
{
    return QString("0x%1").arg(_addr.toString());
}


//---------------------------------------------------
// TraceLine

TraceLine::TraceLine()
    : TraceListCost(ProfileContext::context(ProfileContext::Line))
{
  // we are the owner of TracePartLine's generated in our factory
  _deps.setAutoDelete(true);
  _lineJumps.setAutoDelete(true);

  _lineno = 0;
  _sourceFile = 0;
}

TraceLine::~TraceLine()
{}

bool TraceLine::hasCost(EventType* ct)
{
    bool res = subCost(ct) > 0;
    if (!res) {
	TraceLineCall* lc;
	for(lc=_lineCalls.first();lc;lc=_lineCalls.next())
	    if (lc->subCost(ct) > 0) break;
	res = (lc != 0);
	if (!res) {
	    TraceLineJump* lj;
	    for(lj=_lineJumps.first();lj;lj=_lineJumps.next())
		if (lj->executedCount() > 0) break;
	    res = (lj != 0);
	}
    }

    return res;
}

TracePartLine* TraceLine::partLine(TracePart* part,
                                   TracePartFunction* partFunction)
{
  TracePartLine* item = (TracePartLine*) findDepFromPart(part);
  if (!item) {
    item = new TracePartLine(this);
    item->setPosition(part);
    addDep(item);
#if !USE_FIXCOST
    part->addDep(item);
#endif
    partFunction->addPartLine(item);
  }
  return item;
}

TraceLineJump* TraceLine::lineJump(TraceLine* to, bool isJmpCond)
{
  TraceLineJump* jump;
  for (jump=_lineJumps.first();jump;jump=_lineJumps.next())
    if (jump->lineTo() == to)
      break;

  if (!jump) {
    jump = new TraceLineJump(this, to, isJmpCond);

    _lineJumps.append(jump);
  }
  return jump;
}


void TraceLine::addLineCall(TraceLineCall* lineCall)
{
#if TRACE_ASSERTIONS
  if (_lineCalls.findRef(lineCall)>=0) return;

  if (lineCall->line() != this) {
    qDebug("Can not add line call to another line!");
    return;
  }
#endif

  TraceFunction* caller = lineCall->call()->caller();
  TraceFunction* function = _sourceFile->function();
  if (caller != function) {
    // We regard 2 functions as the same if they have
    // same class, name, object
    if ((caller->cls() != function->cls()) ||
        (caller->name() != function->name()) ||
        (caller->object() != function->object())) {

    qDebug("ERROR: Adding line call, line %d\n  of %s to\n  %s ?!",
           lineCall->line()->lineno(),
           caller->info().ascii(), function->info().ascii());
    }
  }

  _lineCalls.append(lineCall);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ),
         qPrintable(lineCall->fullName()), _lineCalls.count());
#endif
}


QString TraceLine::name() const
{
    QString fileShortName = _sourceFile->file()->shortName();
    if (fileShortName.isEmpty())
	return QObject::tr("(unknown)");

    return QString("%1:%2")
	.arg(fileShortName).arg(_lineno);
}

QString TraceLine::prettyName() const
{
  return QString("%1 [%2]")
    .arg(name()).arg(_sourceFile->function()->prettyName());
}

//---------------------------------------------------
// TraceCostItem

TraceCostItem::TraceCostItem(ProfileContext* context)
    : TraceInclusiveListCost(context)
{
}

TraceCostItem::~TraceCostItem()
{}


//---------------------------------------------------
// TraceFunctionSource

TraceFunctionSource::TraceFunctionSource(TraceFunction* function,
					 TraceFile* file)
    : ProfileCostArray(ProfileContext::context(ProfileContext::FunctionSource))
{
  _file = file;
  _function = function;

  // the function is dependent from our cost sum
  _dep = _function;

  _lineMap = 0;
  _lineMapFilled = false;
  _line0 = 0;
}

TraceFunctionSource::~TraceFunctionSource()
{
    if (_lineMap) delete _lineMap;
    if (_line0) delete _line0;
}

QString TraceFunctionSource::name() const
{
  return QString("%1 for %2").arg(_file->name()).arg(_function->name());
}

uint TraceFunctionSource::firstLineno()
{
  // lazy generate the map if not done up to now
  TraceLineMap* map = lineMap();
  // ignore line 0 here
  if (!map || map->count() == 0) return 0;
  TraceLineMap::Iterator it = map->begin();
  return (*it).lineno();
}

uint TraceFunctionSource::lastLineno()
{
  // lazy generate the map if not done up to now
  TraceLineMap* map = lineMap();
  // ignore line 0 here
  if (!map || map->count() == 0) return 0;
  TraceLineMap::Iterator it = map->end();
  --it;
  return (*it).lineno();
}

/* factory */
TraceLine* TraceFunctionSource::line(uint lineno, bool createNew)
{
    if (lineno == 0) {
	if (!_line0) {
	    if (!createNew) return 0;
	    _line0 = new TraceLine;
	    _line0->setSourceFile(this);
	    _line0->setLineno(0);
	}
	return _line0;
    }

  if (!createNew) {
    if (!_lineMap) return 0;
    TraceLineMap::Iterator it = _lineMap->find(lineno);
    if (it == _lineMap->end()) return 0;
    return &(it.data());
  }

  if (!_lineMap) _lineMap = new TraceLineMap;

  TraceLine& l = (*_lineMap)[lineno];
  if (!l.isValid()) {
    l.setSourceFile(this);
    l.setLineno(lineno);

#if TRACE_DEBUG
    qDebug("Created %s [TraceFunctionSource::line]",
           qPrintable(l.fullName()));
#endif
  }
  return &l;
}

void TraceFunctionSource::update()
{
  if (!_dirty) return;

  clear();

  // no need to create lineMap if not already created
  if (_lineMap) {
    TraceLineMap::Iterator lit;
    for ( lit = _lineMap->begin();
          lit != _lineMap->end(); ++lit )
      addCost( &(*lit) );
  }

  _dirty = false;
}

void TraceFunctionSource::invalidateDynamicCost()
{
  // no need to create lineMap if not already created
  if (_lineMap) {
    TraceLineMap::Iterator lit;
    for ( lit = _lineMap->begin();
          lit != _lineMap->end(); ++lit )
      (*lit).invalidate();
  }

  invalidate();
}

TraceLineMap* TraceFunctionSource::lineMap()
{
#if USE_FIXCOST

    if (_lineMapFilled) return _lineMap;
    _lineMapFilled = true;
    if (!_lineMap)
	_lineMap = new TraceLineMap;

  TraceLine* l = 0;
  TracePartLine* pl = 0;
  TraceLineCall* lc = 0;
  TracePartLineCall* plc = 0;

  /* go over all part objects for this function, and
   * - build TraceLines (the line map) using FixCost objects
   * - build TraceJumpLines using FixJump objects
   */
  TraceInclusiveCostList pfList = _function->deps();
  TracePartFunction* pf = (TracePartFunction*) pfList.first();
  for(; pf; pf = (TracePartFunction*) pfList.next()) {

      if (0) qDebug("PartFunction %s:%d",
		    pf->function()->name().ascii(), pf->part()->partNumber());

      FixCost* fc = pf->firstFixCost();
      for(; fc; fc = fc->nextCostOfPartFunction()) {
	  if (fc->line() == 0) continue;
	  if (fc->functionSource() != this) continue;

	  if (!l || l->lineno() != fc->line()) {
	      l = &(*_lineMap)[fc->line()];
	      if (!l->isValid()) {
		  l->setSourceFile(this);
		  l->setLineno(fc->line());
	      }
	      pl = 0;
	  }
	  if (!pl || pl->part() != fc->part())
	      pl = l->partLine(fc->part(), pf);
	  fc->addTo(pl);
      }

      TraceLine* to = 0;
      TraceLineJump* lj;
      TracePartLineJump* plj;
      FixJump* fj = pf->firstFixJump();
      for(; fj; fj = fj->nextJumpOfPartFunction()) {
	  if (fj->line() == 0) continue;
	  if (fj->source() != this) continue;
	  if (!fj->targetSource()) {
	    // be robust against buggy loaders
	    continue;
	  }

	  // do not display jumps to same or following line
	  if ((fj->line() == fj->targetLine()) ||
	      (fj->line()+1 == fj->targetLine())) continue;

	  if (!l || l->lineno() != fj->line()) {
	      l = &(*_lineMap)[fj->line()];
	      if (!l->isValid()) {
		  l->setSourceFile(this);
		  l->setLineno(fj->line());
	      }
	  }

	  to = fj->targetSource()->line(fj->targetLine(), true);

	  lj = l->lineJump(to, fj->isCondJump());
	  plj = lj->partLineJump(fj->part());

	  fj->addTo(plj);
      }


      TracePartCallList pcList = pf->partCallings();
      TracePartCall* pc = pcList.first();
      for(; pc; pc = pcList.next()) {

	  if (0) qDebug("PartCall %s:%d",
			pc->call()->name().ascii(),
			pf->part()->partNumber());

	  FixCallCost* fcc = pc->firstFixCallCost();
	  for(; fcc; fcc = fcc->nextCostOfPartCall()) {
	      if (fcc->line() == 0) continue;
	      if (fcc->functionSource() != this) continue;

	      if (!l || l->lineno() != fcc->line()) {
		  l = &(*_lineMap)[fcc->line()];
		  if (!l->isValid()) {
		      l->setSourceFile(this);
		      l->setLineno(fcc->line());
		  }
	      }
	      if (!lc || lc->call() != pc->call() || lc->line() != l) {
		  lc = pc->call()->lineCall(l);
		  plc = 0;
	      }
	      if (!plc || plc->part() != fcc->part())
		  plc = lc->partLineCall(fcc->part(), pc);

	      fcc->addTo(plc);
	      if (0) qDebug("Add FixCallCost %s:%d/0x%s, CallCount %s",
			    fcc->functionSource()->file()->shortName().ascii(),
			    fcc->line(), fcc->addr().toString().ascii(),
			    fcc->callCount().pretty().ascii());
	  }
      }
  }

#endif

  return _lineMap;
}



//---------------------------------------------------
// TraceAssoziation

TraceAssoziation::TraceAssoziation()
{
  _function = 0;
  _valid = false;
}

TraceAssoziation::~TraceAssoziation()
{
  // do not delete from TraceFunction
  if (_function) _function->removeAssoziation(this);
}

bool TraceAssoziation::isAssoziated()
{
  if (!_function) return false;

  return _function->assoziation(rtti())==this;
}

bool TraceAssoziation::setFunction(TraceFunction* f)
{
  if (_function == f)
    return isAssoziated();

  if (_function) {
    // do not delete ourself
    _function->removeAssoziation(this);
  }

  _function = f;
  if (f && f->assoziation(rtti()) == 0) {
    f->addAssoziation(this);
    return true;
  }
  return false;
}

void TraceAssoziation::clear(TraceData* d, int rtti)
{
  TraceFunctionMap::Iterator it;
  for ( it = d->functionMap().begin();
        it != d->functionMap().end(); ++it )
    (*it).removeAssoziation(rtti);
}

void TraceAssoziation::invalidate(TraceData* d, int rtti)
{
  TraceFunctionMap::Iterator it;
  for ( it = d->functionMap().begin();
        it != d->functionMap().end(); ++it )
    (*it).invalidateAssoziation(rtti);
}


//---------------------------------------------------
// TraceFunction

TraceFunction::TraceFunction()
    : TraceCostItem(ProfileContext::context(ProfileContext::Function))
{
  _object = 0;
  _file = 0;
  _cls = 0;
  _cycle = 0;

    // we are the owner of items generated in our factory
  _deps.setAutoDelete(true);
  _callings.setAutoDelete(true);
  _sourceFiles.setAutoDelete(true);

  _calledCount     = 0;
  _callingCount    = 0;
  _calledContexts  = 0;
  _callingContexts = 0;

  _instrMap = 0;
  _instrMapFilled = false;
}


TraceFunction::~TraceFunction()
{
  _assoziations.setAutoDelete(true);
  _assoziations.clear();

  if (_instrMap) delete _instrMap;
}

// no unique check is done!
void TraceFunction::addAssoziation(TraceAssoziation* a)
{
  if (!a) return;
  _assoziations.append(a);
}

void TraceFunction::removeAssoziation(TraceAssoziation* a)
{
  _assoziations.removeRef(a);
}

void TraceFunction::removeAssoziation(int rtti, bool reallyDelete)
{
  if (rtti==0) {
    if (reallyDelete)
      _assoziations.setAutoDelete(true);
    _assoziations.clear();
    _assoziations.setAutoDelete(false);
    return;
  }

  TraceAssoziation* a;
  for (a=_assoziations.first();a;a=_assoziations.next())
    if (a->rtti() == rtti) {
      if (reallyDelete) delete a;
      _assoziations.remove();
      return;
    }
}

void TraceFunction::invalidateAssoziation(int rtti)
{
  TraceAssoziation* a;
  for (a=_assoziations.first();a;a=_assoziations.next())
    if ((rtti==0) || (a->rtti() == rtti))
      a->invalidate();
}

TraceAssoziation* TraceFunction::assoziation(int rtti)
{
  TraceAssoziation* a;
  for (a=_assoziations.first();a;a=_assoziations.next())
    if (a->rtti() == rtti)
      return a;
  return 0;
}

#if 0
// helper for prettyName
bool TraceFunction::isUniquePrefix(const QString& prefix) const
{
  TraceFunctionMap::ConstIterator it, it2;
  it = it2 = _myMapIterator;
  if (it != data()->functionBeginIterator()) {
    it2--;
    if ((*it2).name().startsWith(prefix)) return false;
  }
  if (it != data()->functionEndIterator()) {
    it++;
    if ((*it).name().startsWith(prefix)) return false;
  }
  return true;
}
#endif

QString TraceFunction::prettyName() const
{
  QString res = _name;

  if (_name.isEmpty())
      return QObject::tr("(unknown)");

#if 0
  // TODO: make it a configuration, but disabled by default.
  //
  // Stripping parameter signature of C++ symbols is fine
  // if the function name is unique in the whole program.
  // However, we only can detect if it is unique in the profile,
  // which makes this "beautification" potentially confusing
  int p = _name.indexOf('(');
  if (p>0) {
    // handle C++ "operator()" correct
    if ( (p+2 < _name.size()) && (_name[p+1] == ')') && (_name[p+2] == '(')) p+=2;

    // we have a C++ symbol with argument types:
    // check for unique function name (inclusive '(' !)
    if (isUniquePrefix(_name.left(p+1)))
      res = _name.left(p);
  }
#endif

  // cycle members
  if (_cycle) {
    if (_cycle != this)
      res = QString("%1 <cycle %2>").arg(res).arg(_cycle->cycleNo());
    else
      res = QString("<cycle %2>").arg(_cycle->cycleNo());
  }


  return res;
}

/*
 * Returns location string: ELF object and source file(s).
 */
QString TraceFunction::location(int maxFiles) const
{
  QString loc;

  // add object file with address range
  if (_object) {
    loc = _object->shortName();

#if 0
    uint from = firstAddress();
    uint to = lastAddress();
    if (from != 0 && to != 0) {
      if (from == to)
        loc += QString(" (0x%1)").arg(to, 0, 16);
      else
        loc += QString(" (0x%1-0x%2)").arg(from, 0, 16).arg(to, 0, 16);
    }
#endif
  }

  // add all source files
  int filesAdded = 0;
  TraceFunctionSourceList list = _sourceFiles;
  TraceFunctionSource* sourceFile = list.first();
  for (;sourceFile;sourceFile=list.next()) {
    if (!sourceFile->file() ||
	(sourceFile->file()->name().isEmpty()) )
      continue;

    if (!loc.isEmpty())
      loc += (filesAdded>0) ? ", " : ": ";
    filesAdded++;

    if ((maxFiles>0) && (filesAdded>maxFiles)) {
	loc += "...";
	break;
    }
    loc += sourceFile->file()->shortName();

#if 0
    from = sourceFile->firstLineno();
    to = sourceFile->lastLineno();
    if (from != 0 && to != 0) {
      if (from == to)
        loc += QString(" (%1)").arg(to);
      else
        loc += QString(" (%1-%2)").arg(from).arg(to);
    }
#endif
  }

  return loc;
}

// pretty version is allowed to mangle the string...
QString TraceFunction::prettyLocation(int maxFiles) const
{
    QString l = location(maxFiles);
    if (l.isEmpty()) return QObject::tr("(unknown)");

    return l;
}

void TraceFunction::addPrettyLocation(QString& s, int maxFiles) const
{
    QString l = location(maxFiles);
    if (l.isEmpty()) return;

    s += QString(" (%1)").arg(l);
}

QString TraceFunction::prettyNameWithLocation(int maxFiles) const
{
    QString l = location(maxFiles);
    if (l.isEmpty()) return prettyName();

    return QString("%1 (%2)").arg(prettyName()).arg(l);
}

QString TraceFunction::info() const
{
    QString l = location();
    if (l.isEmpty())
	return QString("Function %1").arg(name());

    return QString("Function %1 (location %2)")
	.arg(name()).arg(l);
}


Addr TraceFunction::firstAddress() const
{
    // ignore address 0 here
  if (!_instrMap || _instrMap->count() == 0) return 0;
  TraceInstrMap::ConstIterator it = _instrMap->constBegin();
  return (*it).addr();
}

Addr TraceFunction::lastAddress() const
{
    // ignore address 0 here
  if (!_instrMap || _instrMap->count() == 0) return 0;
  TraceInstrMap::ConstIterator it = _instrMap->constEnd();
  --it;
  return (*it).addr();
}

/* factory */
TraceInstr* TraceFunction::instr(Addr addr, bool createNew)
{
    // address 0 not allowed
    if (addr == Addr(0)) return 0;

  if (!createNew) {
      if (!_instrMap) return 0;
      TraceInstrMap::Iterator it = _instrMap->find(addr);
      if (it == _instrMap->end())
	  return 0;
      return &(it.data());
  }

  if (!_instrMap) _instrMap = new TraceInstrMap;

  TraceInstr& i = (*_instrMap)[addr];
  if (!i.isValid()) {
    i.setAddr(addr);
    i.setFunction(this);

#if TRACE_DEBUG
    qDebug("Created %s [TraceFunction::instr]",
           qPrintable(i.fullName()));
#endif
  }
  return &i;
}

void TraceFunction::addCaller(TraceCall* caller)
{
#if TRACE_ASSERTIONS
  if (caller->called() != this) {
    qDebug("Can not add call to another line!\n");
    return;
  }

  if (_callers.findRef(caller)>=0) return;
#endif

  _callers.append(caller);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Caller\n %s (now %d)",
         qPrintable( fullName() ), qPrintable(caller->fullName()), _callers.count());
#endif
}



TraceCall* TraceFunction::calling(TraceFunction* called)
{
  TraceCall* calling;
  for (calling=_callings.first();calling;calling=_callings.next())
    if (calling->called() == called)
      break;

  if (!calling) {
    calling = new TraceCall(this, called);

    _callings.append(calling);

    // we have to invalidate ourself so invalidations from item propagate up
    invalidate();

#if TRACE_DEBUG
    qDebug("Created %s [TraceFunction::calling]", qPrintable(calling->fullName()));
#endif
    called->addCaller(calling);
  }
  return calling;
}

TraceFunctionSource* TraceFunction::sourceFile(TraceFile* file,
                                                   bool createNew)
{
  if (!file) file = _file;

  TraceFunctionSource* sourceFile = _sourceFiles.first();
  for (;sourceFile;sourceFile=_sourceFiles.next())
    if (sourceFile->file() == file) break;

  if (!sourceFile && createNew) {
    sourceFile = new TraceFunctionSource(this, file);

    _sourceFiles.append(sourceFile);

    // we have to invalidate ourself so invalidations from item propagate up
    invalidate();

#if TRACE_DEBUG
    qDebug("Created SourceFile %s [TraceFunction::line]",
           file->name().ascii());
#endif
    file->addSourceFile(sourceFile);
  }
  return sourceFile;
}

TraceLine* TraceFunction::line(TraceFile* file, uint lineno,
                               bool createNew)
{
  Q_ASSERT(file!=0);

  TraceFunctionSource* sf = sourceFile(file, createNew);
  if (!sf)
    return 0;
  else
    return sf->line(lineno, createNew);
}


TracePartFunction* TraceFunction::partFunction(TracePart* part,
                                               TracePartFile* partFile,
                                               TracePartObject* partObject)
{
  TracePartFunction* item = (TracePartFunction*) findDepFromPart(part);
  if (!item) {
    item = new TracePartFunction(this, partObject, partFile);
    item->setPosition(part);
    addDep(item);
#if USE_FIXCOST
    part->addDep(item);
#endif

    if (_cls) {
      TracePartClass* partClass = _cls->partClass(part);
      partClass->addPartFunction(item);
      item->setPartClass(partClass);
    }

    partFile->addPartFunction(item);
    if (partObject)
      partObject->addPartFunction(item);
  }
  else if (item->partObject()==0 && partObject) {
    item->setPartObject(partObject);
    partObject->addPartFunction(item);
  }

  return item;
}


SubCost TraceFunction::calledCount()
{
  if (_dirty) update();

  return _calledCount;
}

int TraceFunction::calledContexts()
{
  if (_dirty) update();

  return _calledContexts;
}

SubCost TraceFunction::callingCount()
{
  if (_dirty) update();

  return _callingCount;
}

int TraceFunction::callingContexts()
{
  if (_dirty) update();

  return _callingContexts;
}

QString TraceFunction::prettyCalledCount()
{
  return _calledCount.pretty();
}

QString TraceFunction::prettyCallingCount()
{
  return _callingCount.pretty();
}


TraceCallList TraceFunction::callers(bool skipCycle) const
{
    if (skipCycle) return _callers;

  // fake the callers for cycle members
  if (_cycle && (_cycle != this)) {
    TraceCallList l;
    TraceCall* c;

    // inner-cycle-callers
    TraceCallList list=_callers;
    for (c=list.first();c;c=list.next())
      if (c->caller()->cycle() == _cycle)
        l.append(c);

    // call from cycle itself
    for (c=_cycle->_callings.first();c;c=_cycle->_callings.next())
      if (c->called() == this) {
        l.append(c);
        return l;
      }
  }

  return _callers;
}

const TraceCallList& TraceFunction::callings(bool /* skipCycle */) const
{
  return _callings;
}

void TraceFunction::invalidateDynamicCost()
{
  TraceCall* c;
  for (c=_callings.first();c;c=_callings.next())
    c->invalidateDynamicCost();

  TraceFunctionSource* sf;
  for (sf=_sourceFiles.first();sf;sf=_sourceFiles.next())
    sf->invalidateDynamicCost();

  if (_instrMap) {
    TraceInstrMap::Iterator iit;
    for ( iit = _instrMap->begin();
          iit != _instrMap->end(); ++iit )
      (*iit).invalidate();
  }

  invalidate();
}

void TraceFunction::update()
{
  if (!_dirty) return;

#if TRACE_DEBUG
  qDebug("Update %s (Callers %d, sourceFiles %d, instrs %d)",
         _name.ascii(), _callers.count(),
	 _sourceFiles.count(), _instrMap ? _instrMap->count():0);
#endif

  _calledCount    = 0;
  _callingCount    = 0;
  _calledContexts  = 0;
  _callingContexts = 0;
  clear();

  // context count is NOT the sum of part contexts
  TraceCall *caller, *calling;
  for (caller=_callers.first();caller;caller=_callers.next()) {
    // FIXME
    if (caller->subCost(0)>0)
      _calledContexts++;
    _calledCount += caller->callCount();
  }

  for (calling=_callings.first();calling;calling=_callings.next()) {
    // FIXME
    if (calling->subCost(0)>0)  _callingContexts++;
    _callingCount += calling->callCount();
  }

  if (data()->inFunctionCycleUpdate() || !_cycle) {
      // usual case (no cycle member)
      TraceInclusiveCost* item;
      for (item=_deps.first();item;item=_deps.next()) {
	  if (!item->part() || !item->part()->isActive()) continue;

	  addCost(item);
	  addInclusive(item->inclusive());
      }
  }
  else {
    // this is a cycle or cycle member
    for (calling=_callings.first();calling;calling=_callings.next()) {

	// ignore inner-cycle member calls for inclusive cost
	if ((_cycle != this) &&
	    (calling->inCycle()>0))  continue;

	addInclusive(calling);
    }

    // self cost
    if (type() == ProfileContext::FunctionCycle) {
      // cycle: self cost is sum of cycle member self costs, but
      //        does not add to inclusive cost
      TraceFunctionList mList = ((TraceFunctionCycle*)this)->members();
      TraceFunction* m;
      for (m=mList.first();m;m=mList.next())
	  addCost(m);
    }
    else {
      // cycle member
      TraceInclusiveCost* item;
      for (item=_deps.first();item;item=_deps.next()) {
	if (!item->part() || !item->part()->isActive()) continue;

	addCost(item);
      }
      _dirty = false; // do not recurse
      addInclusive(this);
    }
  }
  _dirty = false;

#if TRACE_DEBUG
  qDebug("> %s", qPrintable(costString(0)));
#endif
}

bool TraceFunction::isCycle()
{
    return _cycle == this;
}

bool TraceFunction::isCycleMember()
{
    return _cycle && (_cycle != this);
}

void TraceFunction::cycleReset()
{
  _cycle = 0;
  _cycleStackDown = 0;
  _cycleLow = 0;
}

// this does not mark functions calling themself !
void TraceFunction::cycleDFS(int d, int& pNo, TraceFunction** pTop)
{
  if (_cycleLow != 0) return;

  // initialize with prefix order
  pNo++;
  int prefixNo = pNo;
  _cycleLow = prefixNo;

  // put myself on stack
  _cycleStackDown = *pTop;
  *pTop = this;

  /* cycle cut heuristic:
   * skip calls for cycle detection if they make less than _cycleCut
   * percent of the cost of the function.
   * FIXME: Which cost type to use for this heuristic ?!
   */

  SubCost base = 0;
  if (_callers.count()>0) {
      TraceCallList l = _callers;
      TraceCall *caller;

      for (caller=l.first();caller;caller=l.next())
	  if (caller->subCost(0) > base)
	      base = caller->subCost(0);
  }
  else base = inclusive()->subCost(0);

  SubCost cutLimit = SubCost(base * GlobalConfig::cycleCut());

  if (0) {
      qDebug("%s (%d) Visiting %s",
	     QString().fill(' ', d).ascii(), pNo, prettyName().ascii());
      qDebug("%s       Cum. %s, Max Caller %s, cut limit %s",
	     QString().fill(' ', d).ascii(),
	     inclusive()->subCost(0).pretty().ascii(),
	     base.pretty().ascii(),
	     cutLimit.pretty().ascii());
  }

  TraceCall *calling;
  TraceCallList l = _callings;
  for (calling=l.first();calling;calling=l.next()) {
    TraceFunction* called = calling->called();

      // cycle cut heuristic
      if (calling->subCost(0) < cutLimit) {
	  if (0) qDebug("%s       Cut call to %s (cum. %s)",
			QString().fill(' ', d).ascii(),
			called->prettyName().ascii(),
			calling->subCost(0).pretty().ascii());

	  continue;
      }

    if (called->_cycleLow==0) {
      // not visited yet
      called->cycleDFS(d+1, pNo, pTop);
      if (called->_cycleLow < _cycleLow)
        _cycleLow = called->_cycleLow;
    }
    else if (called->_cycleStackDown) {
      // backlink to same SCC (still in stack)
      if (called->_cycleLow < _cycleLow)
        _cycleLow = called->_cycleLow;

      if (0) qDebug("%s (low %d) Back to %s",
                    QString().fill(' ', d).ascii(),
                    _cycleLow, called->prettyName().ascii());
    }
  }

  if (prefixNo == _cycleLow) {
    // this is the base of a SCC.

    if (*pTop == this) {
      *pTop = _cycleStackDown;
      _cycleStackDown = 0;
    }
    else {
      // a SCC with >1 members

      TraceFunctionCycle* cycle = data()->functionCycle(this);
      if (0) qDebug("Found Cycle %d with base %s:",
             cycle->cycleNo(), prettyName().ascii());
      while(*pTop) {
        TraceFunction* top = *pTop;
        cycle->add(top);

        // remove from stack
        *pTop = top->_cycleStackDown;
        top->_cycleStackDown = 0;

        if (0) qDebug("  %s", top->prettyName().ascii());
        if (top == this) break;
      }
    }
  }
}


TraceInstrMap* TraceFunction::instrMap()
{
#if USE_FIXCOST

    if (_instrMapFilled) return _instrMap;
    _instrMapFilled = true;
    if (!_instrMap)
	_instrMap = new TraceInstrMap;

  TraceLine* l = 0;
  TraceInstr* i = 0;
  TracePartInstr* pi = 0;
  TraceInstrCall* ic = 0;
  TracePartInstrCall* pic = 0;

  TraceInclusiveCostList pfList = deps();
  TracePartFunction* pf = (TracePartFunction*) pfList.first();
  for(; pf; pf = (TracePartFunction*) pfList.next()) {

      if (0) qDebug("PartFunction %s:%d",
		    pf->function()->name().ascii(), pf->part()->partNumber());

      FixCost* fc = pf->firstFixCost();
      for(; fc; fc = fc->nextCostOfPartFunction()) {
	  if (fc->addr() == 0) continue;

	  if (!l || (l->lineno() != fc->line()) ||
	      (l->functionSource() != fc->functionSource()))
	      l = fc->functionSource()->line(fc->line(),true);

	  if (!i || i->addr() != fc->addr()) {
	      i = &(*_instrMap)[fc->addr()];
	      if (!i->isValid()) {
		  i->setFunction(this);
		  i->setAddr(fc->addr());
		  i->setLine(l);
	      }
	      pi = 0;
	  }
	  if (!pi || pi->part() != fc->part())
	      pi = i->partInstr(fc->part(), pf);
	  fc->addTo(pi);
      }

      TraceInstr* to = 0;
      TraceInstrJump* ij;
      TracePartInstrJump* pij;
      FixJump* fj = pf->firstFixJump();
      for(; fj; fj = fj->nextJumpOfPartFunction()) {
	  if (fj->addr() == 0) continue;

	  if (!l || (l->lineno() != fj->line()) ||
	      (l->functionSource() != fj->source()))
	      l = fj->source()->line(fj->line(),true);

	  if (!i || i->addr() != fj->addr()) {
	      i = &(*_instrMap)[fj->addr()];
	      if (!i->isValid()) {
		  i->setFunction(this);
		  i->setAddr(fj->addr());
		  i->setLine(l);
	      }
	  }

	  to = fj->targetFunction()->instr(fj->targetAddr(), true);

	  ij = i->instrJump(to, fj->isCondJump());
	  pij = ij->partInstrJump(fj->part());

	  fj->addTo(pij);
      }

      TracePartCallList pcList = pf->partCallings();
      TracePartCall* pc = pcList.first();
      for(; pc; pc = pcList.next()) {

	  if (0) qDebug("PartCall %s:%d",
			pc->call()->name().ascii(),
			pf->part()->partNumber());

	  FixCallCost* fcc = pc->firstFixCallCost();
	  for(; fcc; fcc = fcc->nextCostOfPartCall()) {
	      if (fcc->addr() == 0) continue;

	      if (!l || (l->lineno() != fcc->line()) ||
		  (l->functionSource() != fcc->functionSource()))
		  l = fcc->functionSource()->line(fcc->line(),true);

	      if (!i || i->addr() != fcc->addr()) {
		  i = &(*_instrMap)[fcc->addr()];
		  if (!i->isValid()) {
		      i->setFunction(this);
		      i->setAddr(fcc->addr());
		      i->setLine(l);
		  }
	      }
	      if (!ic || ic->call() != pc->call() || ic->instr() != i) {
		  ic = pc->call()->instrCall(i);
		  pic = 0;
	      }
	      if (!pic || pic->part() != fcc->part())
		  pic = ic->partInstrCall(fcc->part(), pc);

	      fcc->addTo(pic);
	      if (0) qDebug("Add FixCallCost %s:%d/0x%s, CallCount %s",
			    fcc->functionSource()->file()->shortName().ascii(),
			    fcc->line(), fcc->addr().toString().ascii(),
			    fcc->callCount().pretty().ascii());
	  }
      }
  }

#endif

  return _instrMap;
}



//---------------------------------------------------
// TraceFunctionCycle

TraceFunctionCycle::TraceFunctionCycle(TraceFunction* f, int n)
{
  _base = f;
  _cycleNo = n;
  _cycle = this;

  setContext(ProfileContext::context(ProfileContext::FunctionCycle));

  setPosition(f->data());
  setName(QString("<cycle %1>").arg(n));

  // reset to attributes of base function
  setFile(_base->file());
  setClass(_base->cls());
  setObject(_base->object());
}

void TraceFunctionCycle::init()
{
  _members.clear();
  _callers.clear();
  // this deletes all TraceCall's to members
  _callings.clear();

  invalidate();
}

void TraceFunctionCycle::add(TraceFunction* f)
{
  _members.append(f);
}

void TraceFunctionCycle::setup()
{
  if (_members.count()==0) return;

  TraceFunction* f;
  for (f=_members.first();f;f=_members.next()) {

    // the cycle takes all outside callers from its members
    TraceCall *call;
    TraceCallList l = f->callers();
    for (call=l.first();call;call=l.next()) {
      if (_members.containsRef(call->caller())>0) continue;
      _callers.append(call);
    }

    // the cycle has a call to each member
    call = new TraceCall(this, f);
    call->invalidate();
    _callings.append(call);

    // now do some faking...
    f->setCycle(this);
  }
  invalidate();
}


//---------------------------------------------------
// TraceClass

TraceClass::TraceClass()
    : TraceCostItem(ProfileContext::context(ProfileContext::Class))
{
  // we are the owner of items generated in our factory
  _deps.setAutoDelete(true);
}

TraceClass::~TraceClass()
{}

QString TraceClass::prettyName() const
{
  if (_name.isEmpty())
    return QString("(global)");
  return _name;
}

TracePartClass* TraceClass::partClass(TracePart* part)
{
  TracePartClass* item = (TracePartClass*) findDepFromPart(part);
  if (!item) {
    item = new TracePartClass(this);
    item->setPosition(part);
    addDep(item);
  }
  return item;
}

void TraceClass::addFunction(TraceFunction* function)
{
#if TRACE_ASSERTIONS
  if (function->cls() != this) {
    qDebug("Can not add function to a class not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ),
         qPrintable(function->fullName()), _functions.count());
#endif
}



//---------------------------------------------------
// TraceFile

TraceFile::TraceFile()
    : TraceCostItem(ProfileContext::context(ProfileContext::File))
{
  // we are the owner of items generated in our factory
  _deps.setAutoDelete(true);
}

TraceFile::~TraceFile()
{}

TracePartFile* TraceFile::partFile(TracePart* part)
{
  TracePartFile* item = (TracePartFile*) findDepFromPart(part);
  if (!item) {
    item = new TracePartFile(this);
    item->setPosition(part);
    addDep(item);
  }
  return item;
}

void TraceFile::addFunction(TraceFunction* function)
{
#if TRACE_ASSERTIONS
  if (function->file() != this) {
    qDebug("Can not add function to a file not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ),
         qPrintable(function->fullName()), _functions.count());
#endif
}


void TraceFile::addSourceFile(TraceFunctionSource* sourceFile)
{
#if TRACE_ASSERTIONS
  if (sourceFile->file() != this) {
    qDebug("Can not add sourceFile to a file not having lines for it\n");
    return;
  }
#endif

  _sourceFiles.append(sourceFile);
  // not truly needed, as we do not use the sourceFiles for cost update
  invalidate();

#if TRACE_DEBUG
  qDebug("%s \n added SourceFile %s (now %d)",
         qPrintable( fullName() ), qPrintable(sourceFile->fullName()),
         _sourceFiles.count());
#endif
}



void TraceFile::setDirectory(const QString& dir)
{
  if (dir.endsWith('/'))
    _dir = dir.left(dir.length()-1);
  else
    _dir = dir;
}

QString TraceFile::directory()
{
  if (!_dir.isEmpty()) return _dir;

  int lastIndex = 0, index;
  while ( (index=_name.indexOf("/", lastIndex)) >=0)
    lastIndex = index+1;

  if (lastIndex==0) return QString();

  // without ending "/"
  return _name.left(lastIndex-1);
}


QString TraceFile::shortName() const
{
  int lastIndex = 0, index;
  while ( (index=_name.indexOf("/", lastIndex)) >=0)
    lastIndex = index+1;

  return _name.mid(lastIndex);
}

QString TraceFile::prettyName() const
{
    QString sn = shortName();

    if (sn.isEmpty())
	return QObject::tr("(unknown)");

    return sn;
}

QString TraceFile::prettyLongName() const
{
  if (_name.isEmpty())
    return QObject::tr("(unknown)");
  return _name;
}


//---------------------------------------------------
// TraceObject

TraceObject::TraceObject()
    : TraceCostItem(ProfileContext::context(ProfileContext::Object))
{
  // we are the owner of items generated in our factory
  _deps.setAutoDelete(true);
}

TraceObject::~TraceObject()
{}

TracePartObject* TraceObject::partObject(TracePart* part)
{
  TracePartObject* item = (TracePartObject*) findDepFromPart(part);
  if (!item) {
    item = new TracePartObject(this);
    item->setPosition(part);
    addDep(item);
  }
  return item;
}

void TraceObject::addFunction(TraceFunction* function)
{
#if TRACE_ASSERTIONS
  if (function->object() != this) {
    qDebug("Can not add function to an object not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         qPrintable( fullName() ),
         qPrintable(function->fullName()), _functions.count());
#endif
}

void TraceObject::setDirectory(const QString& dir)
{
    if (dir.endsWith('/'))
	_dir = dir.left(dir.length()-1);
    else
	_dir = dir;
}

QString TraceObject::directory()
{
    if (!_dir.isEmpty()) return _dir;

    int lastIndex = 0, index;
    while ( (index=_name.indexOf("/", lastIndex)) >=0)
	lastIndex = index+1;

    if (lastIndex==0) return QString();

    // without ending "/"
    return _name.left(lastIndex-1);
}


QString TraceObject::shortName() const
{
    int lastIndex = 0, index;
    while ( (index=_name.indexOf("/", lastIndex)) >=0)
	lastIndex = index+1;

    return _name.mid(lastIndex);
}

QString TraceObject::prettyName() const
{
    QString sn = shortName();

    if (sn.isEmpty())
	return QObject::tr("(unknown)");

    return sn;
}

//---------------------------------------------------
// TracePart

TracePart::TracePart(TraceData* data, QFile* file)
    : TraceListCost(ProfileContext::context(ProfileContext::Part))
{
  setPosition(data);

  _dep = data;
  _file = file;
  if (_file)
    _name = _file->name();
  _active = true;

  _number = 0;
  _tid = 0;
  _pid = 0;

  _eventTypeMapping = 0;
}

TracePart::~TracePart()
{
  delete _file;
  delete _eventTypeMapping;
}

void TracePart::setPartNumber(int n)
{
    if (data()->maxPartNumber() <n) data()->setMaxPartNumber(n);
    _number = n;
}

void TracePart::setThreadID(int tid)
{
    if (data()->maxThreadID() <tid) data()->setMaxThreadID(tid);
    _tid = tid;
}

void TracePart::setProcessID(int pid)
{
    _pid = pid;
}



// strip path
QString TracePart::shortName() const
{
  int lastIndex = 0, index;
  while ( (index=_name.indexOf("/", lastIndex)) >=0)
    lastIndex = index+1;

  return _name.mid(lastIndex);
}

QString TracePart::prettyName() const
{
    QString name = QString("%1.%2").arg(_pid).arg(_number);
    if (data()->maxThreadID()>1)
	name += QString("-%3").arg(_tid);
    return name;
}

bool TracePart::activate(bool active)
{
  if (_active == active) return false;
  _active = active;

  // to be done by the client of this function
  //  data()->invalidateDynamicCost();
  // So better use the TraceData functions...

  return true;
}



//---------------------------------------------------
// TracePartList

int TracePartList::compareItems ( Item item1, Item item2 )
{
    TracePart* p1 = (TracePart*) item1;
    TracePart* p2 = (TracePart*) item2;
    int mTID = p1->data()->maxThreadID()+1;
    int mNum = p1->data()->maxPartNumber()+1;

    return
	(p1->processID()  - p2->processID()) * mTID * mNum +
	(p1->partNumber() - p2->partNumber()) * mTID +
	(p1->threadID()   - p2->threadID());
}

QString TracePartList::names() const
{
  QString res;
  TracePart* p;
  TracePartList l = *this;
  for (p=l.first();p;p=l.next()) {
    if (!res.isEmpty()) res += ", ";
    res += p->shortName();
  }

  return res;
}



//---------------------------------------------------
// TraceData


// create vectors with reasonable default sizes, but not wasting memory
TraceData::TraceData(Logger* l)
    : ProfileCostArray(ProfileContext::context(ProfileContext::Data))
{
    _logger = l;
    init();
}

TraceData::TraceData(const QString& base)
    : ProfileCostArray(ProfileContext::context(ProfileContext::Data))
{
    _logger = 0;

    init();
    load(base);
}

void TraceData::init()
{
  _parts.setAutoDelete(true);

  _functionCycleCount = 0;
  _inFunctionCycleUpdate = false;

  _maxThreadID = 0;
  _maxPartNumber = 0;
  _fixPool = 0;
  _dynPool = 0;
}

TraceData::~TraceData()
{
  if (_fixPool) delete _fixPool;
  if (_dynPool) delete _dynPool;
}

QString TraceData::shortTraceName() const
{
  int lastIndex = 0, index;
  while ( (index=_traceName.indexOf("/", lastIndex)) >=0)
    lastIndex = index+1;

  return _traceName.mid(lastIndex);
}

FixPool* TraceData::fixPool()
{
  if (!_fixPool)
    _fixPool = new FixPool();

  return _fixPool;
}

DynPool* TraceData::dynPool()
{
  if (!_dynPool)
    _dynPool = new DynPool();

  return _dynPool;
}


/**
 * Two cases:
 * - <base> is a directory: Load first profile data file available
 * - <base> is a file name without part/thread suffixes
 *
 * Returns 0 if nothing found to load
 */
int TraceData::load(const QString& base)
{
  bool baseExisting = true;

  _traceName = base;
  QFileInfo finfo(base);
  QString file = finfo.fileName();
  QDir dir = finfo.dir();

  if (!finfo.exists()) {
    baseExisting = false;
  }
  else if (finfo.isDir()) {
      // search for first profile data file in directory
      dir = QDir(base);

      QStringList prefixList;
      prefixList << "callgrind.out" << "cachegrind.out";
      for ( QStringList::const_iterator it = prefixList.constBegin();
	    it != prefixList.constEnd(); ++it ) {
        file = *it;

	// search for ".pid"
	QStringList strList = dir.entryList(file+".*", QDir::Files);
	if (strList.count()>0) {
	  int l = file.length();
	  file = strList.first();
	  l++;
	  while(file[l] >= '0' && file[l] <= '9') l++;
	  file = file.left(l);
	  break;
	}
      }

      _traceName = dir.path() + '/' + file;
  }

  QStringList strList;
  strList += dir.entryList(file+".*", QDir::Files);
  strList += dir.entryList(file+"-*", QDir::Files);

  baseExisting = QFile::exists(_traceName);
  if (baseExisting)
      strList << file;

  if (strList.count() == 0) {
      _traceName = base + '/' + file + ' ' + QObject::tr("(not found)");
      return 0;
  }


  // try to guess pid from file name
  unsigned int pos = file.length();
  unsigned int pid = 0, f=1;
  pos--;
  while(pos>0) {
      if (file[pos] < '0' || file[pos] > '9') break;
      pid += f * (file[pos].latin1() - '0');
      pos--;
      f *= 10;
  }

  QStringList::const_iterator it;
  unsigned int maxNumber = 0;
  int partsLoaded = 0;
  for (it = strList.constBegin(); it != strList.constEnd(); ++it ) {
    TracePart* p = addPart(dir.path(), *it );

    if (!p) {
	//kDebug() << "Error loading " << *it;
	continue;
    }

    const QString& str = *it;
    int pos = file.length();

    // try to guess part number from file name
    unsigned int n = 0;
    if ((str.length() > pos) && (str[pos] == '.')) {
	pos++;
	while(str.length()>pos) {
	    if (str[pos] < '0' || str[pos] > '9') break;
	    n = 10*n + (str[pos++].latin1() - '0');
	}
    }

    // try to guess thread number from file name
    unsigned int t = 0;
    if ((str.length() > pos) && (str[pos] == '-')) {
	pos++;
	while(str.length()>pos) {
	    if (str[pos] < '0' || str[pos] > '9') break;
	    t = 10*t + (str[pos++].latin1() - '0');
	}
    }

    //qDebug("File %s: Part %d, Thread %d", (*it).ascii(), n, t);

    if (p->partNumber()>0) n = p->partNumber();
    if (n>maxNumber) maxNumber = n;
    if (n==0) n = maxNumber+1;
    p->setPartNumber(n);

    if (p->threadID()==0) p->setThreadID(t);
    if (p->processID()==0) p->setProcessID(pid);

    _parts.append(p);
    partsLoaded++;
  }
  if (partsLoaded == 0) return 0;

  _parts.sort();
  invalidateDynamicCost();
  updateFunctionCycles();

  return partsLoaded;
}

TracePart* TraceData::addPart(const QString& dir, const QString& name)
{
    QString filename = QString("%1/%2").arg(dir).arg(name);
#if TRACE_DEBUG
  qDebug("TraceData::addPart('%s')", filename.ascii());
#endif

  QFile* file = new QFile(filename);
  if (!file->open( QIODevice::ReadOnly ) ) {
      _logger->loadStart(filename);
      _logger->loadFinished(QString(strerror( errno )));
      return 0;
  }
  Loader* l = Loader::matchingLoader(file);
  if (!l) {
      // special case emtpy file: ignore...
      if (file->size() == 0) return 0;

      _logger->loadStart(filename);
      _logger->loadFinished(QString("Unknown file format"));
      return 0;
  }
  l->setLogger(_logger);

  TracePart* part = new TracePart(this, file);

  if (! l->loadTrace(part)) {
      delete part;
      part = 0;
  }

  l->setLogger(0);

  return part;
}

bool TraceData::activateParts(const TracePartList& l)
{
  bool changed = false;

  TracePart* part;
  for (part=_parts.first();part;part=_parts.next())
    if (part->activate(l.containsRef(part)>0))
        changed = true;

  if (changed) {
      // because active parts have changed, throw away calculated
      // costs...
      invalidateDynamicCost();
      updateFunctionCycles();
  }

  return changed;
}


bool TraceData::activateParts(TracePartList l, bool active)
{
  bool changed = false;

  TracePart* part;
  for (part=l.first();part;part=l.next())
    if (_parts.findRef(part)>=0)
      if (part->activate(active))
        changed = true;

  if (changed) {
      invalidateDynamicCost();
      updateFunctionCycles();
  }

  return changed;
}

bool TraceData::activatePart(TracePart* p, bool active)
{
  return p->activate(active);
}

bool TraceData::activateAll(bool active)
{
  return activateParts(_parts, active);
}


TracePart* TraceData::partWithName(const QString& name)
{
  TracePart* part;
  for (part=_parts.first();part;part=_parts.next())
    if (part->name() == name)
      return part;
  return 0;
}

QString TraceData::activePartRange()
{
  QString res;
  int r1=-1, r2=-1, count=1;
  TracePart* part;
  for (part=_parts.first();part;part=_parts.next(), count++)
    if (part->isActive()) {
      if (r1<0) { r1 = r2 = count; }
      else if (r2 == count-1) { r2 = count; }
      else {
        if (!res.isEmpty()) res += ';';
        if (r1==r2) res += QString::number(r1);
        else res += QString("%1-%2").arg(r1).arg(r2);
        r1 = r2 = count;
      }
    }
  if (r1>=0) {
    if (!res.isEmpty()) res += ';';
    if (r1==r2) res += QString::number(r1);
    else res += QString("%1-%2").arg(r1).arg(r2);
  }

  return res;
}

void TraceData::invalidateDynamicCost()
{
  // invalidate all dynamic costs

  TraceObjectMap::Iterator oit;
  for ( oit = _objectMap.begin();
        oit != _objectMap.end(); ++oit )
    (*oit).invalidate();

  TraceClassMap::Iterator cit;
  for ( cit = _classMap.begin();
        cit != _classMap.end(); ++cit )
    (*cit).invalidate();

  TraceFileMap::Iterator fit;
  for ( fit = _fileMap.begin();
        fit != _fileMap.end(); ++fit )
    (*fit).invalidate();

  TraceFunctionMap::Iterator it;
  for ( it = _functionMap.begin();
        it != _functionMap.end(); ++it ) {
    (*it).invalidateDynamicCost();
  }

  invalidate();

}


TraceObject* TraceData::object(const QString& name)
{
  TraceObject& o = _objectMap[name];
  if (!o.data()) {
    // was created
    o.setPosition(this);
    o.setName(name);

#if TRACE_DEBUG
    qDebug("Created %s [TraceData::object]",
           qPrintable(o.fullName()));
#endif
  }
  return &o;
}


TraceFile* TraceData::file(const QString& name)
{
  TraceFile& f = _fileMap[name];
  if (!f.data()) {
    // was created
    f.setPosition(this);
    f.setName(name);

#if TRACE_DEBUG
    qDebug("Created %s [TraceData::file]",
           qPrintable(f.fullName()));
#endif
  }
  return &f;
}


// usually only called by function()
TraceClass* TraceData::cls(const QString& fnName, QString& shortName)
{
  int lastIndex = 0, index, pIndex;

  // we ignore any "::" after a '(' or a space
  pIndex=fnName.indexOf('(', 0);

#if 0
  int sIndex=fnName.find(" ", 0);
  if (sIndex>=0)
    if ((pIndex == -1) || (sIndex < pIndex))
      pIndex = sIndex;
#endif

  while ((index=fnName.indexOf("::", lastIndex)) >=0) {
    if (pIndex>=0 && pIndex<index) break;
    lastIndex = index+2;
  }

  QString clsName = (lastIndex < 3) ? QString::null :	//krazy:exclude=nullstrassign for old broken gcc
                    fnName.left(lastIndex-2);
  shortName = fnName.mid(lastIndex);

  TraceClass& c = _classMap[clsName];
  if (!c.data()) {
    // was created
    c.setPosition(this);
    c.setName(clsName);

#if TRACE_DEBUG
    qDebug("Created %s [TraceData::cls]",
           qPrintable(c.fullName()));
#endif
  }
  return &c;
}


// name is inclusive class/namespace prefix
TraceFunction* TraceData::function(const QString& name,
                                   TraceFile* file, TraceObject* object)
{
  // strip class name
  QString shortName;
  TraceClass* c = cls(name, shortName);

  if (!file || !object || !c) {
    qDebug("ERROR - no file/object/class for %s ?!", qPrintable(name));
    return 0;
  }

  // Do not use file in key: A function can go over many files
  // (inlined parts), but still is ONE function.
  QString key = name + object->shortName();

  TraceFunctionMap::Iterator it;
  it = _functionMap.find(key);
  if (it == _functionMap.end()) {
      it = _functionMap.insert(key, TraceFunction());
      TraceFunction& f = it.data();

      f.setPosition(this);
      f.setName(name);
      f.setClass(c);
      f.setObject(object);
      f.setFile(file);
      //f.setMapIterator(it);

#if TRACE_DEBUG
      qDebug("Created %s [TraceData::function]\n  for %s, %s, %s",
	     qPrintable(f.fullName()),
	     qPrintable(c->fullName()), qPrintable(file->fullName()),
	     object ? qPrintable(object->fullName()) : "(unknown object)");
#endif

      c->addFunction(&f);
      object->addFunction(&f);
      file->addFunction(&f);
  }

  return &(it.data());
}

TraceFunctionMap::Iterator TraceData::functionIterator(TraceFunction* f)
{

  // IMPORTANT: build as SAME key as used in function() above !!
  QString key;

  if (f->cls()) key = f->cls()->name() + "::";
  key += f->name();
  key += f->object()->shortName();

  return _functionMap.find(key);
}

TraceFunctionMap::ConstIterator TraceData::functionBeginIterator() const
{
  return _functionMap.begin();
}

TraceFunctionMap::ConstIterator TraceData::functionEndIterator() const
{
  return _functionMap.end();
}


void TraceData::resetSourceDirs()
{
  TraceFileMap::Iterator fit;
  for ( fit = _fileMap.begin();
        fit != _fileMap.end(); ++fit )
    (*fit).resetDirectory();
}

void TraceData::update()
{
  if (!_dirty) return;

  clear();
  _totals.clear();

  TracePart* part;
  for (part=_parts.first();part;part=_parts.next()) {
    _totals.addCost(part->totals());
    if (part->isActive())
	addCost(part->totals());
  }

  _dirty = false;
}

ProfileCostArray* TraceData::search(ProfileContext::Type t, QString name,
			     EventType* ct, ProfileCostArray* parent)
{
    ProfileCostArray* result = 0;
    ProfileContext::Type pt;
    SubCost sc, scTop = 0;

    pt = parent ? parent->type() : ProfileContext::InvalidType;
    switch(t) {
    case ProfileContext::Function:
	{
	    TraceFunction *f;
	    TraceFunctionMap::Iterator it;
	    for ( it = _functionMap.begin();
		  it != _functionMap.end(); ++it ) {
		f = &(*it);

		if (f->name() != name) continue;

		if ((pt == ProfileContext::Class) && (parent != f->cls())) continue;
		if ((pt == ProfileContext::File) && (parent != f->file())) continue;
		if ((pt == ProfileContext::Object) && (parent != f->object())) continue;

		if (ct) {
		    sc = f->inclusive()->subCost(ct);
		    if (sc <= scTop) continue;
		    scTop = sc;
		}

		result = f;
	    }
	}
	break;

    case ProfileContext::File:
	{
	    TraceFile *f;
	    TraceFileMap::Iterator it;
	    for ( it = _fileMap.begin();
		  it != _fileMap.end(); ++it ) {
		f = &(*it);
		if (f->name() != name) continue;
		if (ct) {
		    sc = f->subCost(ct);
		    if (sc <= scTop) continue;
		    scTop = sc;
		}
		result = f;
	    }
	}
	break;

    case ProfileContext::Class:
	{
	    TraceClass *c;
	    TraceClassMap::Iterator it;
	    for ( it = _classMap.begin();
		  it != _classMap.end(); ++it ) {
		c = &(*it);
		if (c->name() != name) continue;
		if (ct) {
		    sc = c->subCost(ct);
		    if (sc <= scTop) continue;
		    scTop = sc;
		}
		result = c;
	    }
	}
	break;

    case ProfileContext::Object:
	{
	    TraceObject *o;
	    TraceObjectMap::Iterator it;
	    for ( it = _objectMap.begin();
		  it != _objectMap.end(); ++it ) {
		o = &(*it);
		if (o->name() != name) continue;
		if (ct) {
		    sc = o->subCost(ct);
		    if (sc <= scTop) continue;
		    scTop = sc;
		}
		result = o;
	    }
	}
	break;

    case ProfileContext::Instr:
	if (pt == ProfileContext::Function) {
	    TraceInstrMap* instrMap = ((TraceFunction*)parent)->instrMap();
	    if (!instrMap) break;

	    TraceInstr *instr;
	    TraceInstrMap::Iterator it;
	    for ( it = instrMap->begin();
		  it != instrMap->end(); ++it ) {
		instr = &(*it);
		if (instr->name() != name) continue;
		result = instr;
	    }
	}
	break;

    case ProfileContext::Line:
	{
	    TraceFunctionSourceList sList;
	    if (pt == ProfileContext::Function)
		sList = ((TraceFunction*)parent)->sourceFiles();
	    else if (pt == ProfileContext::FunctionSource)
		sList.append((TraceFunctionSource*) parent);
	    else break;

	    TraceLineMap* lineMap;
	    TraceLine* line;
	    TraceLineMap::Iterator it;
	    TraceFunctionSource* fs;
	    for(fs = sList.first(); fs; fs = sList.next()) {
		lineMap = fs->lineMap();
		if (!lineMap) continue;

		for ( it = lineMap->begin();
		      it != lineMap->end(); ++it ) {
		    line = &(*it);
		    if (line->name() != name) continue;
		    result = line;
		}
	    }
	}
	break;

    default:
	break;
    }

    return result;
}


TraceFunctionCycle* TraceData::functionCycle(TraceFunction* f)
{
  TraceFunctionCycle* cycle;
  for (cycle=_functionCycles.first();cycle;cycle=_functionCycles.next())
    if (cycle->base() == f) return cycle;

  _functionCycleCount++;
  cycle = new TraceFunctionCycle(f, _functionCycleCount);

  _functionCycles.append(cycle);
  return cycle;
}


void TraceData::updateFunctionCycles()
{
    //qDebug("Updating cycles...");

  // init cycle info
  TraceFunctionCycle* cycle;
  for (cycle=_functionCycles.first();cycle;cycle=_functionCycles.next())
    cycle->init();

  TraceFunctionMap::Iterator it;
  for ( it = _functionMap.begin(); it != _functionMap.end(); ++it )
    (*it).cycleReset();

  if (!GlobalConfig::showCycles()) return;

  _inFunctionCycleUpdate = true;


#if 0
  int fCount = _functionMap.size(), fNo = 0, progress=0, p;
  QString msg = tr("Recalculating Function Cycles...");
  if (_topLevel) _topLevel->showStatus(msg,0);
#endif

  // DFS and collapse strong connected components (Tarjan)
  int pNo = 0;
  TraceFunction* stackTop;
  for ( it = _functionMap.begin(); it != _functionMap.end(); ++it ) {

#if 0
      if (_topLevel) {
	  fNo++;
	  p = 100*fNo/fCount;
	  if (p> progress) {
	      progress = p;
	      _topLevel->showStatus(msg, p);
	  }
      }
#endif

    stackTop = 0;
    (*it).cycleDFS(1, pNo, &stackTop);
  }

  // postprocess cycles
  for (cycle=_functionCycles.first();cycle;cycle=_functionCycles.next())
    cycle->setup();

  _inFunctionCycleUpdate = false;
  // we have to invalidate costs because cycles are now taken into account
  invalidateDynamicCost();

#if 0
  if (0) if (_topLevel) _topLevel->showStatus(QString(), 0);
#endif
}

void TraceData::updateObjectCycles()
{
}


void TraceData::updateClassCycles()
{
}


void TraceData::updateFileCycles()
{
}


