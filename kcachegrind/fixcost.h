/*
 * Part of KCacheGrind
 *
 * 2003, Josef Weidendorfer
 */

#ifndef FIXCOST_H
#define FIXCOST_H

#define USE_FIXCOST 1

#include "tracedata.h"

struct SpaceChunk;

/**
 * A Pool for fixed cost events (FixCost).
 *
 * The primitive allocation algorithm is only suited for small items.
 */
class FixPool {
 public:
    FixPool();
    ~FixPool();

    void* allocateSpace(unsigned int size);

    /**
     * Reserve space. If you call reservedSpace() directly after, you
     * will get the same memory area.
     */
    void* reserveSpace(unsigned int size);

    /**
     * Before calling this, you have to reserve at least <size> bytes
     * with reserveSpace().
     */
    bool allocateReservedSpace(unsigned int size);

 private:
    /* Checks that there is enough space in the last chunk.
     * Returns false if this is not possible.
     */
    bool ensureSpace(unsigned int);

    struct SpaceChunk *_first, *_last;
    unsigned int _reservation;
    int _count, _size;
};


class PositionSpec
{
 public:
  PositionSpec()
    { fromLine = 0, toLine = 0, fromAddr = 0, toAddr = 0; }
  PositionSpec(uint l1, uint l2, Addr a1, Addr a2)
    { fromLine = l1, toLine = l2, fromAddr = a1, toAddr = a2; }

  bool isLineRegion() const { return (fromLine != toLine); }
  bool isAddrRegion() const { return (fromAddr != toAddr); }

  uint fromLine, toLine;
  Addr fromAddr, toAddr;
};

/**
 * A class holding an unchangable cost item of an input file.
 *
 * As there can be a lot of such cost items, we use our own
 * allocator which uses FixPool
 */
class FixCost {

 public:
    FixCost(TracePart*, FixPool*,
	    TraceFunctionSource*,
	    PositionSpec&,
            TracePartFunction*,
	    FixString&);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceCost*);

    TracePart* part() const { return _part; }
    bool isLineRegion() const { return _pos.isLineRegion(); }
    bool isAddrRegion() const { return _pos.isAddrRegion(); }
    uint fromLine() const { return _pos.fromLine; }
    uint line() const { return _pos.fromLine; }
    uint toLine() const { return _pos.toLine; }
    Addr fromAddr() const { return _pos.fromAddr; }
    Addr addr() const { return _pos.fromAddr; }
    Addr toAddr() const { return _pos.toAddr; }
    TraceFunctionSource* functionSource() const { return _functionSource; }

    FixCost* nextCostOfPartFunction() const
	{ return _nextCostOfPartFunction; }

 private:
    int _count;
    SubCost*  _cost;
    PositionSpec _pos;

    TracePart* _part;
    TraceFunctionSource* _functionSource;
    FixCost *_nextCostOfPartFunction;
};

/**
 * A FixCallCost will be inserted into a
 * - TracePartCall to keep source/target function info
 * - TraceFunctionSourceFile to keep file info of call source
 */
class FixCallCost {

 public:
    FixCallCost(TracePart*, FixPool*,
                TraceFunctionSource*,
                unsigned int line,
		Addr addr,
                TracePartCall*,
                SubCost, FixString&);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceCallCost*);
    void setMax(TraceCost*);

    TracePart* part() const { return _part; }
    unsigned int line() const { return _line; }
    Addr addr() const { return _addr; }
    SubCost callCount() const { return _cost[_count]; }
    TraceFunctionSource* functionSource() const	{ return _functionSource; }
    FixCallCost* nextCostOfPartCall() const
	{ return _nextCostOfPartCall; }

 private:
    // we use 1 SubCost more than _count: _cost[_count] is the call count
    int _count;
    SubCost*  _cost;
    unsigned int _line;
    Addr _addr;

    TracePart* _part;
    TraceFunctionSource* _functionSource;
    FixCallCost* _nextCostOfPartCall;
};

/**
 * A class holding a jump (mostly) inside of a function
 */
class FixJump {

 public:
    FixJump(TracePart*, FixPool*,
	    /* source position */
	    unsigned int line, Addr addr,
	    TracePartFunction*, TraceFunctionSource*,
	    /* target position */
	    unsigned int targetLine, Addr targetAddr,
	    TraceFunction*, TraceFunctionSource*,
	    bool isCondJump,
	    SubCost, SubCost);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceJumpCost*);

    TracePart* part() const { return _part; }
    unsigned int line() const { return _line; }
    Addr addr() const { return _addr; }
    TraceFunctionSource* source() const { return _source; }
    TraceFunction* targetFunction() const { return _targetFunction; }
    unsigned int targetLine() const { return _targetLine; }
    Addr targetAddr() const { return _targetAddr; }
    TraceFunctionSource* targetSource() const { return _targetSource; }
    bool isCondJump() { return _isCondJump; }
    SubCost executedCount() const { return _cost[0]; }
    SubCost followedCount() const
	{ return _isCondJump ? _cost[1] : SubCost(0); }

    FixJump* nextJumpOfPartFunction() const
	{ return _nextJumpOfPartFunction; }

 private:
    bool _isCondJump;
    SubCost*  _cost;
    unsigned int _line, _targetLine;
    Addr _addr, _targetAddr;
    
    TracePart* _part;
    TraceFunctionSource *_source, *_targetSource;
    TraceFunction* _targetFunction;
    FixJump *_nextJumpOfPartFunction;
};

#endif


