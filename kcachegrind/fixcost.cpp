/*
 * Part of KCacheGrind
 *
 * 2003, Josef Weidendorfer
 */

#include <stdlib.h>
#include "fixcost.h"
#include "utils.h"


// FixPool

#define CHUNK_SIZE 100000

struct SpaceChunk
{
    struct SpaceChunk* next;
    unsigned int used;
    char space[1];
};

FixPool::FixPool()
{
    _first = _last = 0;
    _reservation = 0;
    _count = 0;
    _size = 0;
}

FixPool::~FixPool()
{
    struct SpaceChunk* chunk = _first, *next;

    while(chunk) {
	next = chunk->next;
	free(chunk);
	chunk = next;
    }

    if (0) printf("~FixPool: Had %d objects with total size %d\n",
		  _count, _size);
}

void* FixPool::allocateSpace(unsigned int size)
{
    if (!ensureSpace(size)) return 0;

    _reservation = 0;
    void* result = _last->space + _last->used;
    _last->used += size;

    _count++;
    _size += size;

    return result;
}

void* FixPool::reserveSpace(unsigned int size)
{
    if (!ensureSpace(size)) return 0;
    _reservation = size;

    return _last->space + _last->used;
}


bool FixPool::allocateReservedSpace(unsigned int size)
{
    if (_reservation < size) return false;

    _reservation = 0;
    _last->used += size;

    _count++;
    _size += size;

    return true;
}

bool FixPool::ensureSpace(unsigned int size)
{
    if (_last && _last->used + size <= CHUNK_SIZE) return true;

    struct SpaceChunk* newChunk;

    // we don't allow allocation sizes > CHUNK_SIZE
    if (size > CHUNK_SIZE) return false;

    newChunk = (struct SpaceChunk*) malloc(sizeof(struct SpaceChunk) +
					   CHUNK_SIZE);
    newChunk->next = 0;
    newChunk->used = 0;

    if (!_last) {
	_last = _first = newChunk;
    }
    else {
	_last->next = newChunk;
	_last = newChunk;
    }
    return true;
}



// FixCost

FixCost::FixCost(TracePart* part, FixPool* pool,
		 TraceFunctionSource* functionSource,
		 unsigned int line, unsigned int addr,
                 TracePartFunction* partFunction,
		 FixString& s)
{
    int maxCount = part->fixSubMapping()->count();

    _part = part;
    _functionSource = functionSource;
    _line = line;
    _addr = addr;

    _cost = (SubCost*) pool->reserveSpace(sizeof(SubCost) *
					  maxCount);
    s.stripSpaces();
    int i = 0;
    while(i<maxCount) {
	if (!s.stripUInt64(_cost[i])) break;
	i++;
    }
    _count = i;

    if (!pool->allocateReservedSpace(sizeof(SubCost) * _count))
	_count = 0;

    _nextCostOfPartFunction = partFunction ?
	partFunction->setFirstFixCost(this) : 0;
}

void* FixCost::operator new(size_t size, FixPool* pool)
{
    return pool->allocateSpace(size);
}

void FixCost::addTo(TraceCost* c)
{
    TraceSubMapping* sm = _part->fixSubMapping();

    int i, realIndex;

    for(i=0; i<_count; i++) {
	realIndex = sm->realIndex(i);
	c->addCost(realIndex, _cost[i]);
    }
}



// FixCallCost

FixCallCost::FixCallCost(TracePart* part, FixPool* pool,
			 TraceFunctionSource* functionSource,
                         unsigned int line, unsigned int addr,
                         TracePartCall* partCall,
                         SubCost callCount, FixString& s)
{
  if (0) qDebug("Got FixCallCost (addr 0x%x, ln %d): calls %s",
                addr, line,
		callCount.pretty().ascii());

  int maxCount = part->fixSubMapping()->count();

    _part = part;
    _functionSource = functionSource;
    _line = line;
    _addr = addr;

    _cost = (SubCost*) pool->reserveSpace(sizeof(SubCost) * (maxCount+1));
    s.stripSpaces();
    int i = 0;
    while(i<maxCount) {
	if (!s.stripUInt64(_cost[i])) break;
	i++;
    }
    _count = i;

    if (!pool->allocateReservedSpace(sizeof(SubCost) * (_count+1) ))
      _count = 0;
    else
      _cost[_count] = callCount;

    _nextCostOfPartCall = partCall ? partCall->setFirstFixCallCost(this) : 0;
}

void* FixCallCost::operator new(size_t size, FixPool* pool)
{
    return pool->allocateSpace(size);
}

void FixCallCost::addTo(TraceCallCost* c)
{
    TraceSubMapping* sm = _part->fixSubMapping();

    int i, realIndex;

    for(i=0; i<_count; i++) {
	realIndex = sm->realIndex(i);
	c->addCost(realIndex, _cost[i]);
    }
    c->addCallCount(_cost[_count]);

    if (0) qDebug("Adding from (addr 0x%x, ln %d): calls %s",
                  _addr, _line,
		  _cost[_count].pretty().ascii());
}


// FixJump

FixJump::FixJump(TracePart* part, FixPool* pool,
		 unsigned int line, unsigned int addr,
		 TracePartFunction* partFunction,
		 TraceFunctionSource* source,
		 unsigned int targetLine, unsigned int targetAddr,
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
    _cost = (SubCost*) pool->allocateSpace(size);
    _cost[0] = executed;
    if (isCondJump) _cost[1] = followed;

    _nextJumpOfPartFunction = partFunction ?
	partFunction->setFirstFixJump(this) : 0;
}

void* FixJump::operator new(size_t size, FixPool* pool)
{
    return pool->allocateSpace(size);
}

void FixJump::addTo(TraceJumpCost* jc)
{
    jc->addExecutedCount(_cost[0]);
    if (_isCondJump) 
	jc->addFollowedCount(_cost[1]);
}
