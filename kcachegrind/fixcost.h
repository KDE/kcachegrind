/*
 * Part of KCacheGrind
 *
 * 2003, Josef Weidendorfer
 */

#ifndef FIXCOST_H
#define FIXCOST_H

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


/**
 * A class holding an unchangable cost item of an input file.
 *
 * As there can be a lot of such cost items, we use our own
 * allocator FixedCostContainer (see below)
 */
class FixCost {

 public:
    FixCost(TracePart*, FixPool*,
	    TraceFunctionSource*,
	    unsigned int line, unsigned int addr,
            TracePartFunction*,
	    FixString&);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceCost*);

    TracePart* part() const { return _part; }
    unsigned int line() const { return _line; }
    unsigned int addr() const { return _addr; }
    TraceFunctionSource* functionSource() const { return _functionSource; }

    FixCost* nextCostOfPartFunction() const
	{ return _nextCostOfPartFunction; }

 private:
    int _count;
    SubCost*  _cost;
    unsigned int _line, _addr;

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
                unsigned int line, unsigned int addr,
                TracePartCall*,
                SubCost, FixString&);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceCallCost*);

    TracePart* part() const { return _part; }
    unsigned int line() const { return _line; }
    unsigned int addr() const { return _addr; }
    SubCost callCount() const { return _cost[_count]; }
    TraceFunctionSource* functionSource() const	{ return _functionSource; }
    FixCallCost* nextCostOfPartCall() const
	{ return _nextCostOfPartCall; }

 private:
    // we use 1 SubCost more than _count: _cost[_count] is the call count
    int _count;
    SubCost*  _cost;
    unsigned int _line, _addr;

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
	    unsigned int line, unsigned int addr,
	    TracePartFunction*, TraceFunctionSource*,
	    /* target position */
	    unsigned int targetLine, unsigned int targetAddr,
	    TraceFunction*, TraceFunctionSource*,
	    bool isCondJump,
	    SubCost, SubCost);

    void *operator new(size_t size, FixPool*);

    void addTo(TraceJumpCost*);

    TracePart* part() const { return _part; }
    unsigned int line() const { return _line; }
    unsigned int addr() const { return _addr; }
    TraceFunctionSource* source() const { return _source; }
    TraceFunction* targetFunction() const { return _targetFunction; }
    unsigned int targetLine() const { return _targetLine; }
    unsigned int targetAddr() const { return _targetAddr; }
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
    unsigned int _line, _addr, _targetLine, _targetAddr;
    
    TracePart* _part;
    TraceFunctionSource *_source, *_targetSource;
    TraceFunction* _targetFunction;
    FixJump *_nextJumpOfPartFunction;
};

#endif


