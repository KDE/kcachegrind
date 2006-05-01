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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/


#include <stdlib.h>

#include <qfile.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include <klocale.h>
#include <kdebug.h>

#include "tracedata.h"
#include "toplevel.h"
#include "loader.h"
#include "configuration.h"
#include "utils.h"
#include "fixcost.h"


#define TRACE_DEBUG      0
#define TRACE_ASSERTIONS 0

const int TraceCost::MaxRealIndex = MaxRealIndexValue;
const int TraceCost::InvalidIndex = -1;

//---------------------------------------------------
// Addr

bool Addr::set(FixString& s)
{
  return s.stripUInt64(_v); 
}

int Addr::set(const char *s)
{
  int n = 0;
  _v = 0;

  while((n<16) && *s) {
    if ((*s>='0') && (*s<='9'))
      _v = 16*_v + (*s-'0');
    else if ((*s>='a') && (*s<='f'))
      _v = 16*_v + 10 + (*s-'a');
    else if ((*s>='A') && (*s<='F'))
      _v = 16*_v + 10 + (*s-'A');    
    else break;
    s++;
    n++;
  }
  
  return n;
}


QString Addr::toString() const
{
  if (_v == 0) return QString("0");

  uint64 n = _v;
  QString hex;
#if (QT_VERSION-0 >= 0x030200)
  hex.reserve(16);
#endif

  while(n>0) {
    int d = (n & 15);
    hex = QChar((d<10) ? ('0'+d) : ('A'-10+d)) + hex;
    n /= 16;
  }

  return hex;
}

QString Addr::pretty() const
{
  if (_v == 0) return QString("0");

  uint64 n = _v;
  int p = 0;
  QString hex;
#if (QT_VERSION-0 >= 0x030200)
  hex.reserve(20);
#endif

  while(n>0) {
    int d = (n & 15);
    if ((p>0) && ((p%4)==0)) hex = " " + hex;
    hex = QChar((d<10) ? ('0'+d) : ('A'-10+d)) + hex;
    n /= 16;
    p++;
  }

  return hex;
}

bool Addr::isInRange(Addr a, int distance)
{
  uint64 diff = (a._v > _v) ? (a._v - _v) : (_v - a._v);
  uint64 dist = (distance<0) ? distance : -distance;
  return (diff < dist);
}

//---------------------------------------------------
// TraceItem

QString* TraceItem::_typeName = 0;
QString* TraceItem::_i18nTypeName = 0;

TraceItem::TraceItem()
{
  _position = 0;
  _dep = 0;
  _dirty = true;
}

TraceItem::~TraceItem()
{}

void TraceItem::cleanup()
{
  if (_typeName) {
    delete [] _typeName;
    _typeName = 0;
  }
  if (_i18nTypeName) {
    delete [] _i18nTypeName;
    _i18nTypeName = 0;
  }
}

QString TraceItem::typeName(CostType t)
{
    if (!_typeName) {
      _typeName = new QString [MaxCostType+1];
      QString* strs = _typeName;
      for(int i=0;i<=MaxCostType;i++)
	strs[i] = QString("?");

       strs[Item] = I18N_NOOP("Abstract Item");
       strs[Cost] = I18N_NOOP("Cost Item");
       strs[PartLine] = I18N_NOOP("Part Source Line");
       strs[Line] = I18N_NOOP("Source Line");
       strs[PartLineCall] = I18N_NOOP("Part Line Call");
       strs[LineCall] = I18N_NOOP("Line Call");
       strs[PartLineJump] = I18N_NOOP("Part Jump");
       strs[LineJump] = I18N_NOOP("Jump");
       strs[PartInstr] = I18N_NOOP("Part Instruction");
       strs[Instr] = I18N_NOOP("Instruction");
       strs[PartInstrJump] = I18N_NOOP("Part Instruction Jump");
       strs[InstrJump] = I18N_NOOP("Instruction Jump");
       strs[PartInstrCall] = I18N_NOOP("Part Instruction Call");
       strs[InstrCall] = I18N_NOOP("Instruction Call");
       strs[PartCall] = I18N_NOOP("Part Call");
       strs[Call] = I18N_NOOP("Call");
       strs[PartFunction] = I18N_NOOP("Part Function");
       strs[FunctionSource] = I18N_NOOP("Function Source File");
       strs[Function] = I18N_NOOP("Function");
       strs[FunctionCycle] = I18N_NOOP("Function Cycle");
       strs[PartClass] = I18N_NOOP("Part Class");
       strs[Class] = I18N_NOOP("Class");
       strs[PartFile] = I18N_NOOP("Part Source File");
       strs[File] = I18N_NOOP("Source File");
       strs[PartObject] = I18N_NOOP("Part ELF Object");
       strs[Object] = I18N_NOOP("ELF Object");
       strs[Part] = I18N_NOOP("Profile Part");
       strs[Data] = I18N_NOOP("Program Trace");
    }
    if (t<0 || t> MaxCostType) t = MaxCostType;
    return _typeName[t];
}

TraceItem::CostType TraceItem::costType(QString s)
{
  // This is the default cost Type
  if (s.isEmpty()) return Function;

  CostType type;
  for (int i=0; i<MaxCostType;i++) {
    type = (CostType) i;
    if (typeName(type) == s)
      return type;
  }
  return NoCostType;
}

// all strings of typeName() are translatable because of I18N_NOOP there
QString TraceItem::i18nTypeName(CostType t)
{
    if (!_i18nTypeName) {
	_i18nTypeName = new QString [MaxCostType+1];
	for(int i=0;i<=MaxCostType;i++)
	    _i18nTypeName[i] = i18n(typeName((CostType)i).utf8().data());
    }
    if (t<0 || t> MaxCostType) t = MaxCostType;
    return _i18nTypeName[t];
}

TraceItem::CostType TraceItem::i18nCostType(QString s)
{
  // This is the default cost Type
  if (s.isEmpty()) return Function;

  CostType type;
  for (int i=0; i<MaxCostType;i++) {
    type = (CostType) i;
    if (i18nTypeName(type) == s)
      return type;
  }
  return NoCostType;
}


void TraceItem::clear()
{
    invalidate();
}


QString TraceItem::costString(TraceCostMapping*)
{
    return QString("(no cost)");
}

QString TraceItem::name() const
{
  if (part()) {
    return i18n("%1 from %2")
      .arg(_dep->name())
      .arg(part()->name());
  }

  if (_dep)
    return _dep->name();

  return i18n("(unknown)");
}

QString TraceItem::prettyName() const
{
    if (name().isEmpty()) return i18n("(unknown)");
    return name();
}


QString TraceItem::fullName() const
{
  return QString("%1 %2")
    .arg(typeName(type())).arg(prettyName());
}

QString TraceItem::toString()
{
  return QString("%1\n  [%3]").arg(fullName()).arg(costString(0));
}

void TraceItem::invalidate()
{
  if (_dirty) return;
  _dirty = true;

  if (_dep)
    _dep->invalidate();
}

void TraceItem::update()
{
  _dirty = false;
}

TracePart* TraceItem::part()
{
  return _position ? _position->part() : 0; 
}

const TracePart* TraceItem::part() const
{
  return _position ? _position->part() : 0; 
}

TraceData* TraceItem::data()
{
  return _position ? _position->data() : 0;
}

const TraceData* TraceItem::data() const
{
  return _position ? _position->data() : 0; 
}


//---------------------------------------------------
// TraceCost

TraceCost::TraceCost()
    : TraceItem()
{
  _cachedType = 0; // no virtual value cached

  TraceCost::clear();
}

TraceCost::~TraceCost()
{}


void TraceCost::clear()
{
    // simple set usage count to 0
    _count = 0;

    TraceItem::clear();
}



void TraceCost::set(TraceSubMapping* sm, const char* s)
{
    if (!sm) return;
    if (!s) {
	if (_count>0) clear();
	return;
    }

    while(*s == ' ') s++;

    if (sm->isIdentity()) {
	int i = 0;
	while(i<sm->count()) {
	    if (!_cost[i].set(&s)) break;
	    i++;
	}
	_count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    index = sm->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == TraceCost::InvalidIndex) break;
	    if (!_cost[index].set(&s)) break;
	    i++;
	}
	// we have to set all costs of unused indexes till maxIndex to zero
	for(i=sm->firstUnused(); i<=maxIndex; i=sm->nextUnused(i))
	    _cost[i] = 0;
	_count = maxIndex;
    }
    // a cost change has to be propagated (esp. in subclasses)
    invalidate();
}

void TraceCost::set(TraceSubMapping* sm, FixString & s)
{
    if (!sm) return;

    s.stripSpaces();

    if (sm->isIdentity()) {
	int i = 0;
	while(i<sm->count()) {
	    if (!s.stripUInt64(_cost[i])) break;
	    i++;
	}
	_count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    index = sm->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == TraceCost::InvalidIndex) break;
	    if (!s.stripUInt64(_cost[index])) break;
	    i++;
	}
	// we have to set all costs of unused indexes till maxIndex to zero
	for(i=sm->firstUnused(); i<=maxIndex; i=sm->nextUnused(i))
	    _cost[i] = 0;
	_count = maxIndex+1;
    }
    invalidate();
}


void TraceCost::addCost(TraceSubMapping* sm, const char* s)
{
    if (!sm || !s) return;

    SubCost v;

    if (sm->isIdentity()) {
	int i = 0;
	while(i<sm->count()) {
	    if (!v.set(&s)) break;
	    if (i<_count)
		_cost[i] += v;
	    else
		_cost[i] = v;
	    i++;
	}
	if (i > _count) _count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    if (!v.set(&s)) break;
	    index = sm->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == TraceCost::InvalidIndex) break;
	    if (index<_count)
		_cost[index] += v;
	    else
		_cost[index] = v;
	    i++;
	}
	if (maxIndex >= _count) {
	    /* we have to set all costs of unused indexes in the interval
	     *  [_count;maxIndex] to zero */
	    for(i=sm->nextUnused(_count-1); i<=maxIndex; i=sm->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    // a cost change has to be propagated (esp. in subclasses)
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // don't recurse !
    qDebug("%s\n now %s", fullName().ascii(),
	   TraceCost::costString(0).ascii());
    _dirty = true; // because of invalidate()
#endif
}

void TraceCost::addCost(TraceSubMapping* sm, FixString & s)
{
    if (!sm) return;

    s.stripSpaces();

    SubCost v;

    if (sm->isIdentity()) {
	int i = 0;
	while(i<sm->count()) {
	    if (!s.stripUInt64(v)) break;
	    if (i<_count)
		_cost[i] += v;
	    else
		_cost[i] = v;
	    i++;
	}
	if (i > _count) _count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    if (!s.stripUInt64(v)) break;
	    index = sm->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == TraceCost::InvalidIndex) break;
	    if (index<_count)
		_cost[index] += v;
	    else
		_cost[index] = v;
	    i++;
	}
	if (maxIndex >= _count) {
	    /* we have to set all costs of unused indexes in the interval
	     *  [_count;maxIndex] to zero */
	    for(i=sm->nextUnused(_count-1); i<=maxIndex; i=sm->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    invalidate();

#if TRACE_DEBUG
    _dirty = false; // don't recurse !
    qDebug("%s\n now %s", fullName().ascii(),
	   TraceCost::costString(0).ascii());
    _dirty = true; // because of invalidate()
#endif
}


// update each subcost to be maximum of old and given costs
void TraceCost::maxCost(TraceSubMapping* sm, FixString & s)
{
    if (!sm) return;

    s.stripSpaces();

    SubCost v;

    if (sm->isIdentity()) {
	int i = 0;
	while(i<sm->count()) {
	    if (!s.stripUInt64(v)) break;
	    if (i<_count) {
	      if (v>_cost[i]) _cost[i] = v;
	    }
	    else
		_cost[i] = v;
	    i++;
	}
	if (i > _count) _count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    if (!s.stripUInt64(v)) break;
	    index = sm->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == TraceCost::InvalidIndex) break;
	    if (index<_count) {
	      if (v>_cost[index]) _cost[index] = v;
	    }
	    else
		_cost[index] = v;
	    i++;
	}
	if (maxIndex >= _count) {
	    /* we have to set all costs of unused indexes in the interval
	     *  [_count;maxIndex] to zero */
	    for(i=sm->nextUnused(_count-1); i<=maxIndex; i=sm->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    invalidate();

#if TRACE_DEBUG
    _dirty = false; // don't recurse !
    qDebug("%s\n now %s", fullName().ascii(),
	   TraceCost::costString(0).ascii());
    _dirty = true; // because of invalidate()
#endif
}


void TraceCost::addCost(TraceCost* item)
{
    int i;

    if (!item) return;

    // we have to update the other item if needed
    // because we access the item costs directly
    if (item->_dirty) item->update();

    if (item->_count < _count) {
	for (i = 0; i<item->_count; i++)
	    _cost[i] += item->_cost[i];
    }
    else {
	for (i = 0; i<_count; i++)
	    _cost[i] += item->_cost[i];
	for (; i<item->_count; i++)
	    _cost[i] = item->_cost[i];
	_count = item->_count;
    }

    // a cost change has to be propagated (esp. in subclasses)
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // don't recurse !
    qDebug("%s added cost item\n %s\n  now %s",
	   fullName().ascii(), item->fullName().ascii(),
	   TraceCost::costString(0).ascii());
    _dirty = true; // because of invalidate()
#endif
}

void TraceCost::maxCost(TraceCost* item)
{
    int i;

    if (!item) return;

    // we have to update the other item if needed
    // because we access the item costs directly
    if (item->_dirty) item->update();

    if (item->_count < _count) {
	for (i = 0; i<item->_count; i++)
	  if (_cost[i] < item->_cost[i]) _cost[i] = item->_cost[i];
    }
    else {
	for (i = 0; i<_count; i++)
	  if (_cost[i] < item->_cost[i]) _cost[i] = item->_cost[i];
	for (; i<item->_count; i++)
	    _cost[i] = item->_cost[i];
	_count = item->_count;
    }

    // a cost change has to be propagated (esp. in subclasses)
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // don't recurse !
    qDebug("%s added cost item\n %s\n  now %s",
	   fullName().ascii(), item->fullName().ascii(),
	   TraceCost::costString(0).ascii());
    _dirty = true; // because of invalidate()
#endif
}

void TraceCost::addCost(int type, SubCost value)
{
    if (type<0 || type>=MaxRealIndex) return;
    if (type<_count)
	_cost[type] += value;
    else {
	for(int i=_count;i<type;i++)
	    _cost[i] = 0;
	_cost[type] = value;
	_count = type+1;
    }

    // a cost change has to be propagated (esp. in subclasses)
    invalidate();
}

void TraceCost::maxCost(int type, SubCost value)
{
    if (type<0 || type>=MaxRealIndex) return;
    if (type<_count) {
      if (value>_cost[type]) _cost[type] = value;
    }
    else {
	for(int i=_count;i<type;i++)
	    _cost[i] = 0;
	_cost[type] = value;
	_count = type+1;
    }

    // a cost change has to be propagated (esp. in subclasses)
    invalidate();
}


TraceCost TraceCost::diff(TraceCost* item)
{
  TraceCost res;

  // we have to update the other item if needed
  // because we access the item costs directly
  if (item->_dirty) item->update();

  int maxCount = (item->_count > _count) ? item->_count : _count;

  res._count = maxCount;
  for (int i=0; i<maxCount;i++)
    res._cost[i] = item->subCost(i) - subCost(i);

  return res;
}

QString TraceCost::costString(TraceCostMapping* m)
{
  QString res;

  if (_dirty) update();

  int maxIndex = m ? m->realCount() : TraceCost::MaxRealIndex;
  for (int i = 0; i<maxIndex; i++) {
    if (!res.isEmpty()) res += ", ";
    if (m) res += m->type(i)->name() + " ";

    res += subCost(i).pretty();
  }
  return res;
}


void TraceCost::invalidate()
{
  if (_dirty) return;
  _dirty = true;
  _cachedType = 0; // cached value is invalid, too

  if (_dep)
    _dep->invalidate();
}

void TraceCost::update()
{
  _dirty = false;
}

// this is only for real types
SubCost TraceCost::subCost(int idx)
{
    if (idx<0) return 0;

    /* update if needed as cost could be calculated dynamically in subclasses
     * this can change _count !! */
    if (_dirty) update();
    if (idx>=_count) return 0;

    return _cost[idx];
}

SubCost TraceCost::subCost(TraceCostType* t)
{
  if (!t) return 0;
  if (_cachedType != t) {
      _cachedType = t;
      _cachedCost = t->subCost(this);
  }
  return _cachedCost;
}

QString TraceCost::prettySubCost(TraceCostType* t)
{
    return subCost(t).pretty();
}



//---------------------------------------------------
// TraceJumpCost

TraceJumpCost::TraceJumpCost()
    :TraceItem()
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

QString TraceJumpCost::costString(TraceCostMapping*)
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
// TraceCostType

QPtrList<TraceCostType>* TraceCostType::_knownTypes = 0;

TraceCostType::TraceCostType(QString name,  QString longName, QString formula)
{
  _name = name;
  _longName = longName;
  _formula = formula;
  _mapping = 0;
  _realIndex = TraceCost::InvalidIndex;
  _parsed = false;
  _inParsing = false;

  for (int i=0; i<TraceCost::MaxRealIndex;i++)
    _coefficient[i] = 0;
}

void TraceCostType::setFormula(QString formula)
{
  _formula = formula;
  _realIndex = TraceCost::InvalidIndex;
  _parsed = false;
}

void TraceCostType::setMapping(TraceCostMapping* m)
{
  _parsed = false;
  _mapping = m;
}

// setting the index to TraceCost::MaxRealIndex makes it a
// real type with unspecified index
void TraceCostType::setRealIndex(int i)
{
  if (i<0 || i>TraceCost::MaxRealIndex)
    i=TraceCost::InvalidIndex;

  _realIndex = i;
  _formula = QString::null;
}

// checks for existing types and sets coefficients
bool TraceCostType::parseFormula()
{
  if (_parsed) return true;
  if (_inParsing) {
    qDebug("TraceCostType::parseFormula: Recursion detected.");
    return false;
  }

  if (!_mapping) {
    qDebug("TraceCostType::parseFormula: No mapping set!");
    return false;
  }

  _inParsing = true;

  for (int i=0; i<TraceCost::MaxRealIndex;i++)
    _coefficient[i] = 0;

  QRegExp rx( "((?:\\+|\\-)?)\\s*(\\d*)\\s*\\*?\\s*(\\w+)" );

  int factor, pos;
  QString costName;
  TraceCostType* costType;

  pos = 0;
  while (1) {
    pos = rx.search(_formula, pos);
    if (pos<0) break;
    pos += rx.matchedLength();
    if (rx.cap(0).isEmpty()) break;

    //qDebug("parseFormula: matched '%s','%s','%s'",
    //       rx.cap(1).ascii(), rx.cap(2).ascii(), rx.cap(3).ascii());

    costName = rx.cap(3);
    costType = _mapping->type(costName);
    if (!costType) {
	// qDebug("Cost type '%s': In formula cost '%s' unknown.",
        //     _name.ascii(), costName.ascii());

	_inParsing = false;
	return false;
    }

    factor = (rx.cap(2).isEmpty()) ? 1 : rx.cap(2).toInt();
    if (rx.cap(1) == "-") factor = -factor;

    if (costType->isReal())
      _coefficient[costType->realIndex()] += factor;
    else {
      costType->parseFormula();
      for (int i=0; i<TraceCost::MaxRealIndex;i++)
        _coefficient[i] += factor * costType->_coefficient[i];
    }
  }

  _inParsing = false;
  _parsed = true;

  return true;
}

QString TraceCostType::parsedFormula()
{
  QString res;

  if (!parseFormula()) return res;

  for (int i=0; i<TraceCost::MaxRealIndex;i++) {
    int c = _coefficient[i];
    if (c == 0) continue;

    if (!res.isEmpty()) {
      res += " ";
      if (c>0) res += "+ ";
    }
    if (c<0) { res += "- "; c = -c; }
    res += QString::number(c);

    TraceCostType* t = _mapping->type(i);
    if (!t) continue;

    if (!t->name().isEmpty())
      res += QString(" * %1").arg(t->name());
  }

  return res;
}

SubCost TraceCostType::subCost(TraceCost* c)
{
  if (_realIndex != TraceCost::InvalidIndex)
    return c->subCost(_realIndex);

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }
  SubCost res = 0;

  int rc = _mapping->realCount();
  for (int i = 0;i<rc;i++)
    if (_coefficient[i] != 0)
      res += _coefficient[i] * c->subCost(i);

  return res;
}

int TraceCostType::histCost(TraceCost* c, double total, double* hist)
{
  if (total == 0.0) return 0;

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }

  int rc = _mapping->realCount();
  for (int i = 0;i<rc;i++) {
    if (_coefficient[i] != 0)
      hist[i] = _coefficient[i] * c->subCost(i) / total;
    else
      hist[i] = 0.0;
  }

  return rc;
}




TraceCostType* TraceCostType::knownRealType(QString n)
{
  if (!_knownTypes) return 0;

  TraceCostType* t;
  for (t=_knownTypes->first();t;t=_knownTypes->next())
    if (t->isReal() && (t->name() == n)) {
      TraceCostType* type = new TraceCostType(*t);
      return type;
    }

  return 0;
}

TraceCostType* TraceCostType::knownVirtualType(QString n)
{
  if (!_knownTypes) return 0;

  TraceCostType* t;
  for (t=_knownTypes->first();t;t=_knownTypes->next())
    if (!t->isReal() && (t->name() == n)) {
      TraceCostType* type = new TraceCostType(*t);
      return type;
    }

  return 0;
}

// we take ownership
void TraceCostType::add(TraceCostType* t)
{
  if (!t) return;

  t->setMapping(0);

  if (!_knownTypes)
    _knownTypes = new QPtrList<TraceCostType>;

  /* Already known? */
  TraceCostType* kt;
  for (kt=_knownTypes->first();kt;kt=_knownTypes->next())
    if (kt->name() == t->name()) break;

  if (kt) {
    // Overwrite old type
    if (!t->longName().isEmpty() &&
	(t->longName() != t->name())) kt->setLongName(t->longName());
    if (!t->formula().isEmpty()) kt->setFormula(t->formula());

    delete t;
  }
  else {
    if (t->longName().isEmpty()) t->setLongName(t->name());
    _knownTypes->append(t);
  }
}


int TraceCostType::knownTypeCount()
{
  if (!_knownTypes) return 0;

  return _knownTypes->count();
}

bool TraceCostType::remove(QString n)
{
  if (!_knownTypes) return false;

  TraceCostType* t;
  for (t=_knownTypes->first();t;t=_knownTypes->next())
    if (!t->isReal() && (t->name() == n)) {
      _knownTypes->removeRef(t);
      delete t;
      return true;
    }

  return false;
}

TraceCostType* TraceCostType::knownType(int i)
{
  if (!_knownTypes) return 0;
  if (i<0 || i>=(int)_knownTypes->count()) return 0;

  return _knownTypes->at(i);
}

QColor TraceCostType::color()
{
  if (!_mapping) return QColor();
  return _mapping->realColors()[_realIndex];
}


//---------------------------------------------------
// TraceCostMapping

TraceCostMapping::TraceCostMapping()
{
  _realCount = 0;
  _virtualCount = 0;
  for (int i=0;i<TraceCost::MaxRealIndex;i++) _real[i] = 0;
  for (int i=0;i<TraceCost::MaxRealIndex;i++) _virtual[i] = 0;
}

TraceCostMapping::~TraceCostMapping()
{
  for (int i=0;i<TraceCost::MaxRealIndex;i++)
    if (_real[i]) delete _real[i];

  for (int i=0;i<TraceCost::MaxRealIndex;i++)
    if (_virtual[i]) delete _virtual[i];
}

TraceSubMapping* TraceCostMapping::subMapping(QString types, bool create)
{
  // first check if there's enough space in the mapping
  int newCount = 0;
  int pos = 0, pos2, len = types.length();

  while (1) {
    // skip space
    while((pos<len) && types[pos].isSpace()) pos++;

    pos2 = pos;
    while((pos2<len) && !types[pos2].isSpace()) pos2++;
    if (pos2 == pos) break;

    if (realIndex(types.mid(pos,pos2-pos)) == TraceCost::InvalidIndex)
      newCount++;

    pos = pos2;
  }

  if (!create && (newCount>0)) return 0;

  if (newCount+_realCount > TraceCost::MaxRealIndex) {
    kdDebug() << "TraceCostMapping::subMapping: No space for " 
	      << newCount << " sub costs." << endl;
    return 0;
  }

  TraceSubMapping* sm = new TraceSubMapping(this);

  pos = 0;
  while (1) {
    // skip space
    while((pos<len) && types[pos].isSpace()) pos++;

    pos2 = pos;
    while((pos2<len) && !types[pos2].isSpace()) pos2++;
    if (pos2 == pos) break;

    sm->append(addReal(types.mid(pos,pos2-pos)));

    pos = pos2;
  }

  return sm;
}


int TraceCostMapping::addReal(QString t)
{
  int index = realIndex(t);
  if (index>=0) return index;

  TraceCostType* ct = TraceCostType::knownRealType(t);
  if (!ct) ct = new TraceCostType(t, t);

  // make it real
  ct->setRealIndex();

  return add(ct);
}

// add a cost type to a mapping
// this transfers ownership of the type!
int TraceCostMapping::add(TraceCostType* ct)
{
  if (!ct) return TraceCost::InvalidIndex;

  ct->setMapping(this);

  if (ct->isReal()) {
    if (_realCount >= TraceCost::MaxRealIndex) {
      qDebug("WARNING: Maximum for real cost types reached (on adding '%s')",
             ct->name().ascii());
      return TraceCost::InvalidIndex;
    }
    _real[_realCount] = ct;
    ct->setRealIndex(_realCount);
    _realColor[_realCount] = Configuration::costTypeColor(ct);

    _realCount++;
    return _realCount-1;
  }

  if (_virtualCount >= TraceCost::MaxRealIndex) {
    qDebug("WARNING: Maximum for virtual cost types reached (on adding '%s')",
           ct->name().ascii());
    return TraceCost::InvalidIndex;
  }
  _virtual[_virtualCount] = ct;
  _virtualCount++;
  return _virtualCount-1;
}

// we delete the type: t is invalid when returning true!
bool TraceCostMapping::remove(TraceCostType* t)
{
  if (!t) return false;
  if (t->mapping() != this) return false;

  // don't delete real types
  if (t->isReal()) return false;

  int i;
  for(i=0;i<_virtualCount;i++)
    if (_virtual[i] == t) break;
  
  // not found?
  if (i == _virtualCount) return false;

  // delete known type with same name
  TraceCostType::remove(t->name());

  // delete this type
  _virtual[i] = 0;
  delete t;
  if (i+1 == _virtualCount) {
    // we can reuse the last index
    _virtualCount--;
  }
  return true;
}


TraceCostType* TraceCostMapping::realType(int t)
{
  if (t<0 || t>=_realCount) return 0;
  return _real[t];
}

TraceCostType* TraceCostMapping::virtualType(int t)
{
  if (t<0 || t>=_virtualCount) return 0;
  return _virtual[t];
}


TraceCostType* TraceCostMapping::type(int t)
{
  if (t<0) return 0;
  if (t<_realCount) return _real[t];

  t -= TraceCost::MaxRealIndex;
  if (t<0) return 0;
  if (t<_virtualCount) return _virtual[t];

  return 0;
}

TraceCostType* TraceCostMapping::type(QString name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return _real[i];

  for (int i=0;i<_virtualCount;i++)
    if (_virtual[i] && (_virtual[i]->name() == name))
      return _virtual[i];

  return 0;
}

TraceCostType* TraceCostMapping::typeForLong(QString name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->longName() == name))
      return _real[i];

  for (int i=0;i<_virtualCount;i++)
    if (_virtual[i] && (_virtual[i]->longName() == name))
      return _virtual[i];

  return 0;
}


int TraceCostMapping::realIndex(QString name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  return TraceCost::InvalidIndex;
}

int TraceCostMapping::index(QString name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  for (int i=0;i<_virtualCount;i++)
    if (_virtual[i] && (_virtual[i]->name() == name))
      return TraceCost::MaxRealIndex + 1 + i;

  return TraceCost::InvalidIndex;
}

int TraceCostMapping::addKnownVirtualTypes()
{
  int addCount = 0;
  int addDiff, i;
  int knownCount = TraceCostType::knownTypeCount();

  while (1) {
    addDiff = 0;
    for (i=0; i<knownCount; i++) {
      TraceCostType* t = TraceCostType::knownType(i);
      if (t->isReal()) continue;
      if (index(t->name()) != TraceCost::InvalidIndex) continue;
      t->setMapping(this);
      if (t->parseFormula()) {
        addDiff++;
        add(new TraceCostType(t->name(), t->longName(), t->formula()));
      }
      t->setMapping(0);
    }
    if (addDiff == 0) break;
    addCount += addDiff;
  }
  return addCount;
}


//---------------------------------------------------
// TraceSubMapping

TraceSubMapping::TraceSubMapping(TraceCostMapping* mapping)
{
  _mapping = mapping;
  clear();
}

void TraceSubMapping::clear()
{
  _count = 0;
  _isIdentity = true;
  _firstUnused = 0;
  for(int i=0;i<TraceCost::MaxRealIndex;i++) {
    _realIndex[i] = TraceCost::InvalidIndex;
    _nextUnused[i] = i+1;
  }
}

bool TraceSubMapping::append(QString type, bool create)
{
  if (!_mapping) return false;
  int index = create ? _mapping->addReal(type) : _mapping->realIndex(type);

  return append(index);
}

bool TraceSubMapping::append(int type)
{
  if (!_mapping) return false;
  if ((type<0) || (type >= _mapping->realCount())) return false;

  if ( _count >=  TraceCost::MaxRealIndex) return false;

  _realIndex[_count] = type;

  if (_isIdentity && (_count != type)) _isIdentity = false;
  if (type == _firstUnused)
      _firstUnused = _nextUnused[type];
  for(int i=0;i<type;i++)
      if (_nextUnused[i] == type)
	  _nextUnused[i]=_nextUnused[type];

  _count++;
  return true;
}


//---------------------------------------------------
// TraceCallCost

TraceCallCost::TraceCallCost()
{
  _callCount = 0;
}

TraceCallCost::~TraceCallCost()
{}


QString TraceCallCost::costString(TraceCostMapping* m)
{
  return QString("%1, Calls %2")
      .arg(TraceCost::costString(m))
      .arg(_callCount.pretty());
}

QString TraceCallCost::prettyCallCount()
{
  return _callCount.pretty();
}

void TraceCallCost::clear()
{
  _callCount = 0;
  TraceCost::clear();
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

TraceInclusiveCost::TraceInclusiveCost()
{}

TraceInclusiveCost::~TraceInclusiveCost()
{}

QString TraceInclusiveCost::costString(TraceCostMapping* m)
{
  return QString("%1, Inclusive %2")
    .arg(TraceCost::costString(m))
    .arg(_inclusive.costString(m));
}

void TraceInclusiveCost::clear()
{
  _inclusive.clear();
  TraceCost::clear();
}

TraceCost* TraceInclusiveCost::inclusive()
{
  if (_dirty) update();

  return &_inclusive;
}

void TraceInclusiveCost::addInclusive(TraceCost* c)
{
  _inclusive.addCost(c);

  invalidate();
}


//---------------------------------------------------
// TraceListCost

TraceListCost::TraceListCost()
{
  _lastDep = 0;
}

TraceListCost::~TraceListCost()
{}

void TraceListCost::addDep(TraceCost* dep)
{
#if TRACE_ASSERTIONS
  if (_deps.findRef(dep)>=0) {
    qDebug("addDep: %s already in list!",
           dep->fullName().ascii());
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), dep->fullName().ascii(),
         _deps.count());
#endif
}

TraceCost* TraceListCost::findDepFromPart(TracePart* part)
{
  if (_lastDep && _lastDep->part() == part)
    return _lastDep;

  TraceCost* dep;
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
         fullName().ascii(), _deps.count());
#endif

  clear();
  TraceCost* item;
  for (item = _deps.first(); item; item = _deps.next()) {
    if (onlyActiveParts())
      if (!item->part() || !item->part()->isActive()) continue;

    addCost(item);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", costString(0).ascii());
#endif
}



//---------------------------------------------------
// TraceJumpListCost

TraceJumpListCost::TraceJumpListCost()
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
           dep->fullName().ascii());
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), dep->fullName().ascii(),
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
         fullName().ascii(), _deps.count());
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
  qDebug("   > %s", costString(0).ascii());
#endif
}



//---------------------------------------------------
// TraceCallListCost

TraceCallListCost::TraceCallListCost()
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
           dep->fullName().ascii());
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), dep->fullName().ascii(),
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
         fullName().ascii(), _deps.count());
#endif

  /* Without dependent cost items, assume fixed costs,
   * i.e. don't change cost */
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
  qDebug("   > %s", costString(0).ascii());
#endif
}


//---------------------------------------------------
// TraceInclusiveListCost

TraceInclusiveListCost::TraceInclusiveListCost()
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
           dep->fullName().ascii());
    return;
  }
#endif

  _deps.append(dep);
  _lastDep = dep;
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), dep->fullName().ascii(),
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
         fullName().ascii(), _deps.count());
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
  qDebug("   > %s", costString(0).ascii());
#endif
}



//---------------------------------------------------
// TracePartInstrJump

TracePartInstrJump::TracePartInstrJump(TraceInstrJump* instrJump,
				       TracePartInstrJump* next)
{
  _dep = instrJump;
  _next = next;
}

TracePartInstrJump::~TracePartInstrJump()
{}


//---------------------------------------------------
// TracePartInstrCall

TracePartInstrCall::TracePartInstrCall(TraceInstrCall* instrCall)
{
  _dep = instrCall;
}

TracePartInstrCall::~TracePartInstrCall()
{}



//---------------------------------------------------
// TracePartInstr

TracePartInstr::TracePartInstr(TraceInstr* instr)
{
  _dep = instr;
}

TracePartInstr::~TracePartInstr()
{}



//---------------------------------------------------
// TracePartLineJump

TracePartLineJump::TracePartLineJump(TraceLineJump* lineJump)
{
  _dep = lineJump;
}

TracePartLineJump::~TracePartLineJump()
{}


//---------------------------------------------------
// TracePartLineCall

TracePartLineCall::TracePartLineCall(TraceLineCall* lineCall)
{
  _dep = lineCall;
}

TracePartLineCall::~TracePartLineCall()
{}


//---------------------------------------------------
// TracePartLine

TracePartLine::TracePartLine(TraceLine* line)
{
  _dep = line;
}

TracePartLine::~TracePartLine()
{}




//---------------------------------------------------
// TracePartCall

TracePartCall::TracePartCall(TraceCall* call)
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
  qDebug("update %s", fullName().ascii());
#endif

  /* Without dependent cost items, assume fixed costs,
   * i.e. don't change cost */
  if (_firstFixCallCost) {
    clear();
    FixCallCost* item;
    for (item = _firstFixCallCost; item; item = item->nextCostOfPartCall())
      item->addTo(this);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", costString(0).ascii());
#endif

#endif // USE_FIXCOST
}


//---------------------------------------------------
// TracePartFunction

TracePartFunction::TracePartFunction(TraceFunction* function,
                                     TracePartObject* partObject,
				     TracePartFile *partFile)
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

QString TracePartFunction::costString(TraceCostMapping* m)
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
           ref->name().ascii());
    return;
  }
#endif

  _partInstr.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), ref->fullName().ascii(),
         _partInstr.count());
#endif
}


void TracePartFunction::addPartLine(TracePartLine* ref)
{
#if TRACE_ASSERTIONS
  if (_partLines.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartLine: %s already in list!",
           ref->name().ascii());
    return;
  }
#endif

  _partLines.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(), ref->fullName().ascii(),
         _partLines.count());
#endif
}


void TracePartFunction::addPartCaller(TracePartCall* ref)
{
#if TRACE_ASSERTIONS
  if (_partCallers.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartCaller: %s already in list!",
           ref->name().ascii());
    return;
  }
#endif

  _partCallers.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Caller\n %s (now %d)",
         fullName().ascii(), ref->fullName().ascii(),
         _partCallers.count());
#endif
}


void TracePartFunction::addPartCalling(TracePartCall* ref)
{
#if TRACE_ASSERTIONS
  if (_partCallings.findRef(ref)>=0) {
    qDebug("TracePartFunction::addPartCalling: %s already in list!",
           ref->name().ascii());
    return;
  }
#endif

  _partCallings.append(ref);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Calling\n %s (now %d)",
         fullName().ascii(), ref->fullName().ascii(),
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
    TraceCost::clear();

    TracePartLine* line;
    for (line = _partLines.first(); line; line = _partLines.next())
      addCost(line);
  }
#else
  if (_firstFixCost) {
    TraceCost::clear();

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
    _dirty = false; // don't recurse!
    addInclusive(this);
  }

  _dirty = false;

#if TRACE_DEBUG
  qDebug("   > %s", costString(0).ascii());
#endif
}



//---------------------------------------------------
// TracePartClass

TracePartClass::TracePartClass(TraceClass* cls)
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
{
  _dep = file;
}

TracePartFile::~TracePartFile()
{}


//---------------------------------------------------
// TracePartObject

TracePartObject::TracePartObject(TraceObject* object)
{
  _dep = object;
}

TracePartObject::~TracePartObject()
{}




//---------------------------------------------------
// TraceInstrJump

TraceInstrJump::TraceInstrJump(TraceInstr* instrFrom, TraceInstr* instrTo,
			       bool isCondJump)
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
  qDebug("updated %s", fullName().ascii());
#endif

#if TRACE_DEBUG
  qDebug("   > %s", costString(0).ascii());
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
    qDebug("Created %s [TraceCall::instrCall]", icall->fullName().ascii());
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
    qDebug("Created %s [TraceCall::lineCall]", lcall->fullName().ascii());
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
  if (!_caller) return i18n("(no caller)");

  if (!skipCycle) {
      // if this call goes into a cycle, add the entry function
      TraceFunctionCycle* c = _called->cycle();
      if (c && _caller && (_caller->cycle() != c)) {
	  QString via = _called->prettyName();
	  return i18n("%1 via %2").arg(_caller->prettyName()).arg(via);
      }
  }

  return _caller->prettyName();
}

QString TraceCall::calledName(bool skipCycle) const
{
  if (!_called) return i18n("(no callee)");

  if (!skipCycle) {
      // if this call goes into a cycle, add the entry function
      TraceFunctionCycle* c = _called->cycle();
      if (c && _caller && (_caller->cycle() != c)) {
	  // HACK to get rid of cycle postfix...
	  _called->setCycle(0);
	  QString via = _called->prettyName();
	  _called->setCycle(c);
	  return i18n("%1 via %2").arg(c->name()).arg(via);
      }
  }
  return _called->prettyName();
}


//---------------------------------------------------
// TraceInstr

TraceInstr::TraceInstr()
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

bool TraceInstr::hasCost(TraceCostType* ct)
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
    qDebug("Can't add instruction call to another instruction!");
    return;
  }
#endif

  _instrCalls.append(instrCall);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(),
         instrCall->fullName().ascii(), _instrCalls.count());
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
{
  // we are the owner of TracePartLine's generated in our factory
  _deps.setAutoDelete(true);
  _lineJumps.setAutoDelete(true);

  _lineno = 0;
  _sourceFile = 0;
}

TraceLine::~TraceLine()
{}

bool TraceLine::hasCost(TraceCostType* ct)
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
    qDebug("Can't add line call to another line!");
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
         fullName().ascii(),
         lineCall->fullName().ascii(), _lineCalls.count());
#endif
}


QString TraceLine::name() const
{
    QString fileShortName = _sourceFile->file()->shortName();
    if (fileShortName.isEmpty())
	return i18n("(unknown)");

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

TraceCostItem::TraceCostItem()
{
}

TraceCostItem::~TraceCostItem()
{}


//---------------------------------------------------
// TraceFunctionSource

TraceFunctionSource::TraceFunctionSource(TraceFunction* function,
                                                 TraceFile* file)
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
           l.fullName().ascii());
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

	  // don't display jumps to same or following line
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
  // don't delete from TraceFunction
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
    // don't delete ourself
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


// helper for prettyName
bool TraceFunction::isUniquePrefix(QString prefix) const
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


QString TraceFunction::prettyName() const
{
  QString res = _name;

  if (_name.isEmpty())
      return i18n("(unknown)");

  int p = _name.find('(');
  if (p>0) {
    // handle C++ "operator()" correct
    if ((_name[p+1] == ')') && (_name[p+2] == '(')) p+=2;

    // we have a C++ symbol with argument types:
    // check for unique function name (inclusive '(' !)    
    if (isUniquePrefix(_name.left(p+1)))
      res = _name.left(p);
  }

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
    if (l.isEmpty()) return i18n("(unknown)");

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
  TraceInstrMap::ConstIterator it = _instrMap->begin();
  return (*it).addr();
}

Addr TraceFunction::lastAddress() const
{
    // ignore address 0 here
  if (!_instrMap || _instrMap->count() == 0) return 0;
  TraceInstrMap::ConstIterator it = _instrMap->end();
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
           i.fullName().ascii());
#endif
  }
  return &i;
}

void TraceFunction::addCaller(TraceCall* caller)
{
#if TRACE_ASSERTIONS
  if (caller->called() != this) {
    qDebug("Can't add call to another line!\n");
    return;
  }

  if (_callers.findRef(caller)>=0) return;
#endif

  _callers.append(caller);
  invalidate();

#if TRACE_DEBUG
  qDebug("%s added Caller\n %s (now %d)",
         fullName().ascii(), caller->fullName().ascii(), _callers.count());
#endif
}



TraceCall* TraceFunction::calling(TraceFunction* called)
{
  TraceCallMap::Iterator it = _callingMap.find(called);
  TraceCall* calling = (it == _callingMap.end()) ? 0 : it.data();

  if (!calling) {
    calling = new TraceCall(this, called);

    _callingMap.insert(called, calling);
    _callings.append(calling);

    // we have to invalidate ourself so invalidations from item propagate up
    invalidate();

#if TRACE_DEBUG
    qDebug("Created %s [TraceFunction::calling]", calling->fullName().ascii());
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
    if (type() == FunctionCycle) {
      // cycle: self cost is sum of cycle member self costs, but
      //        doesn't add to inclusive cost
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
      _dirty = false; // don't recurse
      addInclusive(this);
    }
  }
  _dirty = false;

#if TRACE_DEBUG
  qDebug("> %s", costString(0).ascii());
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

// this doesn't mark functions calling themself !
void TraceFunction::cycleDFS(int d, int& pNo, TraceFunction** pTop)
{
  if (_cycleLow != 0) return;

  if (0)
      qDebug("%s D%02d > %s (%d)",
	     QString().fill(' ', d).ascii(), d, prettyName().ascii(), pNo+1);



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

  SubCost cutLimit = SubCost(base * Configuration::cycleCut());

  if (0)
      qDebug("%s       Cum. %s, Max Caller %s, cut limit %s",
	     QString().fill(' ', d).ascii(),
	     inclusive()->subCost(0).pretty().ascii(),
	     base.pretty().ascii(),
	     cutLimit.pretty().ascii());

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

	if (0)
	    qDebug("%s D%02d - %s (%d)",
		   QString().fill(' ', d+1).ascii(), d+1, 
		   called->prettyName().ascii(), called->_cycleLow);
    }
    else {
	if (0)
	    qDebug("%s D%02d - %s (%d) [Not on stack]",
		   QString().fill(' ', d+1).ascii(), d+1, 
		   called->prettyName().ascii(), called->_cycleLow);
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
      if (0) qDebug("BASE CYC %d %s",
             cycle->cycleNo(), prettyName().ascii());
      while(*pTop) {
        TraceFunction* top = *pTop;
        cycle->add(top);

        // remove from stack
        *pTop = top->_cycleStackDown;
        top->_cycleStackDown = 0;

        if (0) qDebug("CYC %s", top->prettyName().ascii());
        if (top == this) break;
      }
    }
  }
  if (0)
      qDebug("%s D%02d < %s (%d)",
	     QString().fill(' ', d).ascii(), d, 
	     prettyName().ascii(), _cycleLow);
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
  _memberSet.clear();
  _callers.clear();
  // this deletes all TraceCall's to members
  _callings.clear();

  invalidate();
}

void TraceFunctionCycle::add(TraceFunction* f)
{
  _members.append(f);
  _memberSet.insert(f,1);
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
      if ( _memberSet.contains(call->caller()) ) continue;
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
    qDebug("Can't add function to a class not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(),
         function->fullName().ascii(), _functions.count());
#endif
}



//---------------------------------------------------
// TraceFile

TraceFile::TraceFile()
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
    qDebug("Can't add function to a file not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(),
         function->fullName().ascii(), _functions.count());
#endif
}


void TraceFile::addSourceFile(TraceFunctionSource* sourceFile)
{
#if TRACE_ASSERTIONS
  if (sourceFile->file() != this) {
    qDebug("Can't add sourceFile to a file not having lines for it\n");
    return;
  }
#endif

  _sourceFiles.append(sourceFile);
  // not truely needed, as we don't use the sourceFiles for cost update
  invalidate();

#if TRACE_DEBUG
  qDebug("%s \n added SourceFile %s (now %d)",
         fullName().ascii(), sourceFile->fullName().ascii(),
         _sourceFiles.count());
#endif
}



void TraceFile::setDirectory(const QString& dir)
{
  if (dir.endsWith("/"))
    _dir = dir.left(dir.length()-1);
  else
    _dir = dir;
}

QString TraceFile::directory()
{
  if (!_dir.isEmpty()) return _dir;

  int lastIndex = 0, index;
  while ( (index=_name.find("/", lastIndex)) >=0)
    lastIndex = index+1;

  if (lastIndex==0) return QString::null;

  // without ending "/"
  return _name.left(lastIndex-1);
}


QString TraceFile::shortName() const
{
  int lastIndex = 0, index;
  while ( (index=_name.find("/", lastIndex)) >=0)
    lastIndex = index+1;

  return _name.mid(lastIndex);
}

QString TraceFile::prettyName() const
{
    QString sn = shortName();
    
    if (sn.isEmpty())
	return i18n("(unknown)");

    return sn;
}

QString TraceFile::prettyLongName() const
{
  if (_name.isEmpty())
    return i18n("(unknown)");
  return _name;
}


//---------------------------------------------------
// TraceObject

TraceObject::TraceObject()
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
    qDebug("Can't add function to an object not enclosing this function\n");
    return;
  }

  if (_functions.findRef(function)>=0) return;
#endif

  _functions.append(function);

  invalidate();

#if TRACE_DEBUG
  qDebug("%s added\n %s (now %d)",
         fullName().ascii(),
         function->fullName().ascii(), _functions.count());
#endif
}

// strip path
void TraceObject::setName(const QString& name)
{
  _name = name;

  int lastIndex = 0, index;
  while ( (index=_name.find("/", lastIndex)) >=0)
    lastIndex = index+1;

  _shortName = _name.mid(lastIndex);
}

QString TraceObject::prettyName() const
{
  if (_shortName.isEmpty())
    return i18n("(unknown)");

  return _shortName;
}

//---------------------------------------------------
// TracePart

TracePart::TracePart(TraceData* data, QFile* file)
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

  _fixSubMapping = 0;
}

TracePart::~TracePart()
{
  delete _file;

  delete _fixSubMapping;
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
  while ( (index=_name.find("/", lastIndex)) >=0)
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
TraceData::TraceData(TopLevel* top)
{
    _topLevel = top;
    init();
}

TraceData::TraceData(const QString& base)
{
    _topLevel = 0;
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
  while ( (index=_traceName.find("/", lastIndex)) >=0)
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
 *
 * - <base> is a directory: Load first profile data file available
 * - <base> is a file name without part/thread suffixes
 */
void TraceData::load(const QString& base)
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
      for ( QStringList::Iterator it = prefixList.begin();
	    it != prefixList.end(); ++it ) {
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

      _traceName = dir.path() + "/" + file;
  }

  QStringList strList;
  strList += dir.entryList(file+".*", QDir::Files);
  strList += dir.entryList(file+"-*", QDir::Files);

  baseExisting = QFile::exists(_traceName);
  if (baseExisting)
      strList << file;

  if (strList.count() == 0) {
      _traceName = base + "/" + file + " " + i18n("(not found)");
      return;
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

  QStringList::Iterator it;
  unsigned int maxNumber = 0;
  for (it = strList.begin(); it != strList.end(); ++it ) {
    TracePart* p = addPart( dir.path(), *it );

    if (!p) {
      kdDebug() << "Error loading " << *it << endl;
      continue;
    }

    const QString& str = *it;
    unsigned int pos = file.length();

    // try to guess part number from file name
    unsigned int n = 0;
    if ((str.length() > pos) && (str[pos] == '.')) {
	pos++;
	while(str.length()>pos) {
	    if (str[pos] < '0' || str[pos] > '9') break;
	    n = 10*n + (str[pos++] - '0');
	}
    }

    // try to guess thread number from file name
    unsigned int t = 0;
    if ((str.length() > pos) && (str[pos] == '-')) {
	pos++;
	while(str.length()>pos) {
	    if (str[pos] < '0' || str[pos] > '9') break;
	    t = 10*t + (str[pos++] - '0');
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
  }
  _parts.sort();

  invalidateDynamicCost();
  updateFunctionCycles();

  // clear loading messages from status bar
  if (_topLevel) _topLevel->showStatus(QString::null, 0);
}

TracePart* TraceData::addPart(const QString& dir, const QString& name)
{
    QString filename = QString("%1/%2").arg(dir).arg(name);
#if TRACE_DEBUG
  qDebug("TraceData::addPart('%s')", filename.ascii());
#endif

  QFile* file = new QFile(filename);

  Loader* l = Loader::matchingLoader(file);
  if (!l) return 0;

  if (_topLevel)
      _topLevel->connect(l, SIGNAL(updateStatus(QString, int)),
			 SLOT(showStatus(QString, int)));

  TracePart* part = new TracePart(this, file);

  if (! l->loadTrace(part)) {
      delete part;
      part = 0;
  }

  if (_topLevel) l->disconnect(_topLevel);

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


TracePart* TraceData::part(QString& name)
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
        if (!res.isEmpty()) res += ";";
        if (r1==r2) res += QString::number(r1);
        else res += QString("%1-%2").arg(r1).arg(r2);
        r1 = r2 = count;
      }
    }
  if (r1>=0) {
    if (!res.isEmpty()) res += ";";
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
           o.fullName().ascii());
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
           f.fullName().ascii());
#endif
  }
  return &f;
}


// usually only called by function()
TraceClass* TraceData::cls(const QString& fnName, QString& shortName)
{
  int lastIndex = 0, index, pIndex;

  // we ignore any "::" after a '(' or a space
  pIndex=fnName.find("(", 0);

#if 0
  int sIndex=fnName.find(" ", 0);
  if (sIndex>=0)
    if ((pIndex == -1) || (sIndex < pIndex))
      pIndex = sIndex;
#endif

  while ((index=fnName.find("::", lastIndex)) >=0) {
    if (pIndex>=0 && pIndex<index) break;
    lastIndex = index+2;
  }

  QString clsName = (lastIndex < 3) ? QString::null :
                    fnName.left(lastIndex-2);
  shortName = fnName.mid(lastIndex);

  TraceClass& c = _classMap[clsName];
  if (!c.data()) {
    // was created
    c.setPosition(this);
    c.setName(clsName);

#if TRACE_DEBUG
    qDebug("Created %s [TraceData::cls]",
           c.fullName().ascii());
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
    qDebug("ERROR - no file/object/class for %s ?!", name.ascii());
    return 0;
  }

  // Don't use file in key: A function can go over many files
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
      f.setMapIterator(it);

#if TRACE_DEBUG
      qDebug("Created %s [TraceData::function]\n  for %s, %s, %s",
	     f.fullName().ascii(),
	     c->fullName().ascii(), file->fullName().ascii(),
	     object ? object->fullName().ascii() : "(unknown object)");
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

TraceCost* TraceData::search(TraceItem::CostType t, QString name,
			     TraceCostType* ct, TraceCost* parent)
{
    TraceCost* result = 0;
    TraceItem::CostType pt = parent ? parent->type() : NoCostType;
    SubCost sc, scTop = 0;

    switch(t) {
    case Function:
	{
	    TraceFunction *f;
	    TraceFunctionMap::Iterator it;
	    for ( it = _functionMap.begin();
		  it != _functionMap.end(); ++it ) {
		f = &(*it);

		if (f->name() != name) continue;

		if ((pt == Class) && (parent != f->cls())) continue;
		if ((pt == File) && (parent != f->file())) continue;
		if ((pt == Object) && (parent != f->object())) continue;

		if (ct) {
		    sc = f->inclusive()->subCost(ct);
		    if (sc <= scTop) continue;
		    scTop = sc;
		}

		result = f;
	    }
	}
	break;

    case File:
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

    case Class:
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

    case Object:
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

    case Instr:
	if (pt == Function) {
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

    case Line:
	{
	    TraceFunctionSourceList sList;
	    if (pt == Function)
		sList = ((TraceFunction*)parent)->sourceFiles();
	    else if (pt == FunctionSource)
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

  if (!Configuration::showCycles()) return;

  _inFunctionCycleUpdate = true;


#if 0
  int fCount = _functionMap.size(), fNo = 0, progress=0, p;
  QString msg = i18n("Recalculating Function Cycles...");
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
  if (0) if (_topLevel) _topLevel->showStatus(QString::null,0);
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


