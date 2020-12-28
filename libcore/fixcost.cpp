/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include "fixcost.h"
#include "utils.h"
#include "addr.h"

// FixCost

FixCost::FixCost(TracePart* part, FixPool* pool,
                 TraceFunctionSource* functionSource,
                 PositionSpec& pos,
                 TracePartFunction* partFunction,
                 FixString& s)
{
    int maxCount = part->eventTypeMapping()->count();

    _part = part;
    _functionSource = functionSource;
    _pos = pos;

    _cost = (SubCost*) pool->reserve(sizeof(SubCost) * maxCount);
    s.stripSpaces();
    int i = 0;
    while(i<maxCount) {
        if (!s.stripUInt64(_cost[i])) {
            // xdebug used to emit negative costs if it freed memory. It no
            // longer does this. However, old cachegrind files still exist,
            // and they cause KCacheGrind to constantly print messages about
            // garbage (the negative cost it couldn't parse as unsigned) at
            // the end of the line. This checks for a negative signed value if
            // we can't find a positive one and clamps to zero, which is what
            // xdebug does now itself. [1]
            // [1]: xdebug commit 688c552e620dc5be7eea22cb893c6b71f395c6d4
            int64 temp;
            if (s.stripInt64(temp) && temp < 0) {
                _cost[i] = 0;
            } else {
                break;
            }
        }
        i++;
    }
    _count = i;

    if (!pool->allocateReserved(sizeof(SubCost) * _count))
        _count = 0;

    _nextCostOfPartFunction = partFunction ?
                                  partFunction->setFirstFixCost(this) : nullptr;
}

void* FixCost::operator new(size_t size, FixPool* pool)
{
    return pool->allocate(size);
}

void FixCost::addTo(ProfileCostArray* c)
{
    EventTypeMapping* sm = _part->eventTypeMapping();

    int i, realIndex;

    c->reserve(sm->maxRealIndex(_count)+1);
    for(i=0; i<_count; i++) {
        realIndex = sm->realIndex(i);
        c->addCost(realIndex, _cost[i]);
    }
}



// FixCallCost

FixCallCost::FixCallCost(TracePart* part, FixPool* pool,
                         TraceFunctionSource* functionSource,
                         unsigned int line, Addr addr,
                         TracePartCall* partCall,
                         SubCost callCount, FixString& s)
{
    if (0) qDebug("Got FixCallCost (addr 0x%s, line %d): calls %s",
                  qPrintable(addr.toString()), line,
                  qPrintable(callCount.pretty()));

    int maxCount = part->eventTypeMapping()->count();

    _part = part;
    _functionSource = functionSource;
    _line = line;
    _addr = addr;

    _cost = (SubCost*) pool->reserve(sizeof(SubCost) * (maxCount+1));
    s.stripSpaces();
    int i = 0;
    while(i<maxCount) {
        if (!s.stripUInt64(_cost[i])) {
            // Same case as in the FixCost ctor.
            int64 temp;
            if (s.stripInt64(temp) && temp < 0) {
                _cost[i] = 0;
            } else {
                break;
            }
        }
        i++;
    }
    _count = i;

    if (!pool->allocateReserved(sizeof(SubCost) * (_count+1) ))
        _count = 0;
    else
        _cost[_count] = callCount;

    _nextCostOfPartCall = partCall ? partCall->setFirstFixCallCost(this) : nullptr;
}

void* FixCallCost::operator new(size_t size, FixPool* pool)
{
    return pool->allocate(size);
}

void FixCallCost::addTo(TraceCallCost* c)
{
    EventTypeMapping* sm = _part->eventTypeMapping();

    int i, realIndex;

    for(i=0; i<_count; i++) {
        realIndex = sm->realIndex(i);
        c->addCost(realIndex, _cost[i]);
    }
    c->addCallCount(_cost[_count]);

    if (0) qDebug("Adding from (addr 0x%s, ln %d): calls %s",
                  qPrintable(_addr.toString()), _line,
                  qPrintable(_cost[_count].pretty()));
}

void FixCallCost::setMax(ProfileCostArray* c)
{
    EventTypeMapping* sm = _part->eventTypeMapping();

    int i, realIndex;

    for(i=0; i<_count; i++) {
        realIndex = sm->realIndex(i);
        c->maxCost(realIndex, _cost[i]);
    }
}


// FixJump

FixJump::FixJump(TracePart* part, FixPool* pool,
                 unsigned int line, Addr addr,
                 TracePartFunction* partFunction,
                 TraceFunctionSource* source,
                 unsigned int targetLine, Addr targetAddr,
                 TraceFunction* targetFunction,
                 TraceFunctionSource* targetSource,
                 bool isCondJump,
                 SubCost executed, SubCost followed)
{
    _part   = part;
    _source = source;
    _line   = line;
    _addr   = addr;

    _targetFunction = targetFunction;
    _targetSource   = targetSource;
    _targetLine     = targetLine;
    _targetAddr     = targetAddr;
    
    _isCondJump = isCondJump;

    int size = (isCondJump ? 2 : 1) * sizeof(SubCost);
    _cost = (SubCost*) pool->allocate(size);
    _cost[0] = executed;
    if (isCondJump) _cost[1] = followed;

    _nextJumpOfPartFunction = partFunction ?
                                  partFunction->setFirstFixJump(this) : nullptr;
}

void* FixJump::operator new(size_t size, FixPool* pool)
{
    return pool->allocate(size);
}

void FixJump::addTo(TraceJumpCost* jc)
{
    jc->addExecutedCount(_cost[0]);
    if (_isCondJump)
        jc->addFollowedCount(_cost[1]);
}
