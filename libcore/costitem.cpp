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

#include "costitem.h"

#include "tracedata.h"

#define TRACE_DEBUG      0
#define TRACE_ASSERTIONS 0

//---------------------------------------------------
// ProfileCost

CostItem::CostItem(ProfileContext* c)
{
  _position = 0;
  _dep = 0;
  _dirty = true;

  _context = c;
}

CostItem::~CostItem()
{}


void CostItem::clear()
{
    invalidate();
}


QString CostItem::costString(EventTypeSet*)
{
    return QString("(no cost)");
}

QString CostItem::name() const
{
  if (part()) {
      return QObject::tr("%1 from %2").arg(_dep->name()).arg(part()->name());
  }

  if (_dep)
    return _dep->name();

  return QObject::tr("(unknown)");
}

QString CostItem::prettyName() const
{
    if (name().isEmpty()) return QObject::tr("(unknown)");
    return name();
}

QString CostItem::formattedName() const
{
    return QString();
}

QString CostItem::fullName() const
{
  return QString("%1 %2")
    .arg(ProfileContext::typeName(type())).arg(prettyName());
}

QString CostItem::toString()
{
  return QString("%1\n  [%3]").arg(fullName()).arg(costString(0));
}

void CostItem::invalidate()
{
  if (_dirty) return;
  _dirty = true;

  if (_dep)
    _dep->invalidate();
}

void CostItem::update()
{
  _dirty = false;
}

TracePart* CostItem::part()
{
  return _position ? _position->part() : 0;
}

const TracePart* CostItem::part() const
{
  return _position ? _position->part() : 0;
}

TraceData* CostItem::data()
{
  return _position ? _position->data() : 0;
}

const TraceData* CostItem::data() const
{
  return _position ? _position->data() : 0;
}


//---------------------------------------------------
// ProfileCostArray


const int ProfileCostArray::MaxRealIndex = MaxRealIndexValue;
const int ProfileCostArray::InvalidIndex = -1;


ProfileCostArray::ProfileCostArray(ProfileContext* context)
    : CostItem(context)
{
  _cachedType = 0; // no virtual value cached
  _allocCount = 0;
  _count = 0;
  _cost = 0;
}

ProfileCostArray::ProfileCostArray()
    : CostItem(ProfileContext::context(ProfileContext::UnknownType))
{
  _cachedType = 0; // no virtual value cached
  _allocCount = 0;
  _count = 0;
  _cost = 0;
}

ProfileCostArray::~ProfileCostArray()
{
    if (_cost) delete[] _cost;
}


void ProfileCostArray::clear()
{
    _count = 0;
    invalidate();
}

void ProfileCostArray::reserve(int count)
{
    if (count <= _allocCount) return;

    if (_cost) delete[] _cost;
    _cost = new SubCost[count];
    _allocCount = count;
}

void ProfileCostArray::set(EventTypeMapping* mapping, const char* s)
{
    if (!mapping) return;
    if (!s) {
        clear();
	return;
    }

    reserve(mapping->set()->realCount());

    while(*s == ' ') s++;

    if (mapping->isIdentity()) {
	int i = 0;
	while(i<mapping->count()) {
	    if (!_cost[i].set(&s)) break;
	    i++;
	}
	_count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    index = mapping->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == ProfileCostArray::InvalidIndex) break;
	    if (!_cost[index].set(&s)) break;
	    i++;
	}
	// we have to set all costs of unused indexes till maxIndex to zero
	for(i=mapping->firstUnused(); i<=maxIndex; i=mapping->nextUnused(i))
	    _cost[i] = 0;
	_count = maxIndex;
    }
    Q_ASSERT(_count <= _allocCount);
    // a cost change has to be propagated (esp. in subclasses)
    invalidate();
}

void ProfileCostArray::set(EventTypeMapping* mapping, FixString & s)
{
    if (!mapping) return;
    s.stripSpaces();
    if (s.isEmpty()) {
        clear();
        return;
    }

    reserve(mapping->set()->realCount());

    if (mapping->isIdentity()) {
	int i = 0;
	while(i<mapping->count()) {
	    if (!s.stripUInt64(_cost[i])) break;
	    i++;
	}
	_count = i;
    }
    else {
	int i = 0, maxIndex = 0, index;
	while(1) {
	    index = mapping->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == ProfileCostArray::InvalidIndex) break;
	    if (!s.stripUInt64(_cost[index])) break;
	    i++;
	}
	// we have to set all costs of unused indexes till maxIndex to zero
	for(i=mapping->firstUnused(); i<=maxIndex; i=mapping->nextUnused(i))
	    _cost[i] = 0;
	_count = maxIndex+1;
    }
    Q_ASSERT(_count <= _allocCount);
    invalidate();
}


void ProfileCostArray::addCost(EventTypeMapping* mapping, const char* s)
{
    if (!mapping || !s) return;
    reserve(mapping->set()->realCount());

    SubCost v;
    if (mapping->isIdentity()) {
	int i = 0;
	while(i<mapping->count()) {
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
	    index = mapping->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == ProfileCostArray::InvalidIndex) break;
	    if (index<_count)
		_cost[index] += v;
	    else
		_cost[index] = v;
	    i++;
	}
	if (maxIndex >= _count) {
	    /* we have to set all costs of unused indexes in the interval
	     *  [_count;maxIndex] to zero */
	    for(i=mapping->nextUnused(_count-1); i<=maxIndex; i=mapping->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    Q_ASSERT(_count <= _allocCount);
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // do not recurse !
    qDebug("%s\n now %s", qPrintable( fullName() ),
	   qPrintable( ProfileCostArray::costString(0) ));
    _dirty = true; // because of invalidate()
#endif
}

void ProfileCostArray::addCost(EventTypeMapping* mapping, FixString & s)
{
    if (!mapping) return;
    s.stripSpaces();
    if (s.isEmpty()) return;
    reserve(mapping->set()->realCount());

    SubCost v;
    if (mapping->isIdentity()) {
	int i = 0;
	while(i<mapping->count()) {
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
	    index = mapping->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == ProfileCostArray::InvalidIndex) break;
	    if (index<_count)
		_cost[index] += v;
	    else
		_cost[index] = v;
	    i++;
	}
	if (maxIndex >= _count) {
	    /* we have to set all costs of unused indexes in the interval
	     *  [_count;maxIndex] to zero */
	    for(i=mapping->nextUnused(_count-1); i<=maxIndex; i=mapping->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    Q_ASSERT(_count <= _allocCount);
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // do not recurse !
    qDebug("%s\n now %s", qPrintable( fullName() ),
	   qPrintable( ProfileCostArray::costString(0 ) ) );
    _dirty = true; // because of invalidate()
#endif
}


// update each subcost to be maximum of old and given costs
void ProfileCostArray::maxCost(EventTypeMapping* mapping, FixString & s)
{
    if (!mapping) return;
    s.stripSpaces();
    if (s.isEmpty()) return;
    reserve(mapping->set()->realCount());

    SubCost v;
    if (mapping->isIdentity()) {
	int i = 0;
	while(i<mapping->count()) {
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
	    index = mapping->realIndex(i);
	    if (maxIndex<index) maxIndex=index;
	    if (index == ProfileCostArray::InvalidIndex) break;
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
	    for(i=mapping->nextUnused(_count-1); i<=maxIndex; i=mapping->nextUnused(i))
		_cost[i] = 0;
	    _count = maxIndex+1;
	}
    }

    Q_ASSERT(_count <= _allocCount);
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // do not recurse !
    qDebug("%s\n now %s", qPrintable( fullName() ),
	   qPrintable(ProfileCostArray::costString(0)));
    _dirty = true; // because of invalidate()
#endif
}


void ProfileCostArray::addCost(ProfileCostArray* item)
{
    int i;
    if (!item) return;

    // we have to update the other item if needed
    // because we access the item costs directly
    if (item->_dirty) item->update();

    // make sure we have enough space allocated
    reserve(item->_count);

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

    Q_ASSERT(_count <= _allocCount);
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // do not recurse !
    qDebug("%s added cost item\n %s\n  now %s",
	   qPrintable( fullName() ), qPrintable(item->fullName()),
	   qPrintable(ProfileCostArray::costString(0)));
    _dirty = true; // because of invalidate()
#endif
}

void ProfileCostArray::maxCost(ProfileCostArray* item)
{
    int i;

    if (!item) return;

    // we have to update the other item if needed
    // because we access the item costs directly
    if (item->_dirty) item->update();

    // make sure we have enough space allocated
    reserve(item->_count);

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

    Q_ASSERT(_count <= _allocCount);
    invalidate();

#if TRACE_DEBUG
    _dirty = false; // do not recurse !
    qDebug("%s added cost item\n %s\n  now %s",
	   qPrintable( fullName() ), qPrintable(item->fullName()),
	   qPrintable( ProfileCostArray::costString(0) ));
    _dirty = true; // because of invalidate()
#endif
}

void ProfileCostArray::addCost(int realIndex, SubCost value)
{
    if (realIndex<0 || realIndex>=MaxRealIndex) return;

    reserve(realIndex+1);
    if (realIndex < _count)
        _cost[realIndex] += value;
    else {
        for(int i=_count;i<realIndex;i++)
	    _cost[i] = 0;
	_cost[realIndex] = value;
	_count = realIndex+1;
    }

    Q_ASSERT(_count <= _allocCount);
    invalidate();
}

void ProfileCostArray::maxCost(int realIndex, SubCost value)
{
    if (realIndex<0 || realIndex>=MaxRealIndex) return;

    reserve(realIndex+1);
    if (realIndex<_count) {
      if (value>_cost[realIndex]) _cost[realIndex] = value;
    }
    else {
        for(int i=_count;i<realIndex;i++)
	    _cost[i] = 0;
	_cost[realIndex] = value;
	_count = realIndex+1;
    }

    Q_ASSERT(_count <= _allocCount);
    invalidate();
}


ProfileCostArray ProfileCostArray::diff(ProfileCostArray* item)
{
    ProfileCostArray res(context());

  // we have to update the other item if needed
  // because we access the item costs directly
  if (item->_dirty) item->update();

  int maxCount = (item->_count > _count) ? item->_count : _count;

  res.reserve(maxCount);
  for (int i=0; i<maxCount;i++)
    res._cost[i] = item->subCost(i) - subCost(i);
  res._count = maxCount;

  Q_ASSERT(res._count <= res._allocCount);
  return res;
}

QString ProfileCostArray::costString(EventTypeSet* set)
{
  QString res;

  if (_dirty) update();

  int maxIndex = set ? set->realCount() : ProfileCostArray::MaxRealIndex;
  for (int i = 0; i<maxIndex; i++) {
    if (!res.isEmpty()) res += ", ";
    if (set) res += set->type(i)->name() + ' ';

    res += subCost(i).pretty();
  }
  return res;
}


void ProfileCostArray::invalidate()
{
  if (_dirty) return;
  _dirty = true;
  _cachedType = 0; // cached value is invalid, too

  if (_dep)
    _dep->invalidate();
}

void ProfileCostArray::update()
{
  _dirty = false;
}

// this is only for real types
SubCost ProfileCostArray::subCost(int idx)
{
    if (idx<0) return 0;

    /* update if needed as cost could be calculated dynamically in subclasses
     * this can change _count !! */
    if (_dirty) update();
    if (idx>=_count) return 0;

    return _cost[idx];
}


SubCost ProfileCostArray::subCost(EventType* t)
{
  if (!t) return 0;
  if (_cachedType != t) {
      _cachedType = t;
      _cachedCost = t->subCost(this);
  }
  return _cachedCost;
}

QString ProfileCostArray::prettySubCost(EventType* t)
{
    return subCost(t).pretty();
}

