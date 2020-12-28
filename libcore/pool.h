/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef POOL_H
#define POOL_H

/**
 * Pool objects: containers for many small objects.
 */

struct SpaceChunk;

/**
 * FixPool
 *
 * For objects with fixed size and life time
 * ending with that of the pool.
 */
class FixPool
{
public:
    FixPool();
    ~FixPool();

    /**
     * Take @p size bytes from the pool
     * @param size is the number of bytes
     */
    void* allocate(unsigned int size);

    /**
     * Reserve space. If you call allocateReservedSpace(realsize)
     * with realSize < reserved size directly after, you
     * will get the same memory area.
     */
    void* reserve(unsigned int size);

    /**
     * Before calling this, you have to reserve at least @p size bytes
     * with reserveSpace().
     * @param size is the number of bytes
     */
    bool allocateReserved(unsigned int size);

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
 * DynPool
 *
 * For objects which probably need to be resized
 * in the future. Objects also can be deleted to free up space.
 * As objects can also be moved in a defragmentation step,
 * access has to be done via the given pointer object.
 */
class DynPool
{
public:
    DynPool();
    ~DynPool();

    /**
     * Take @p size bytes from the pool, changing @p *ptr
     * to point to this allocated space.
     * @param size is the number of bytes
     * @param *ptr will be changed if the object is moved.
     * Returns false if no space available.
     */
    bool allocate(char** ptr, unsigned int size);

    /**
     * To resize, first allocate new space, and free old
     * afterwards.
     */
    void free(char** ptr);

private:
    /* Checks that there is enough space. If not,
     * it compactifies, possibly moving objects.
     */
    bool ensureSpace(unsigned int);

    char* _data;
    unsigned int _used, _size;
};

#endif // POOL_H
