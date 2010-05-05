/* This file is part of KCachegrind.
   Copyright (C) 2002-2004 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <string.h>
#include <stdlib.h>
#include <qglobal.h>
#include "pool.h"

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

    if (0) qDebug("~FixPool: Had %d objects with total size %d\n",
		  _count, _size);
}

void* FixPool::allocate(unsigned int size)
{
    if (!ensureSpace(size)) return 0;

    _reservation = 0;
    void* result = _last->space + _last->used;
    _last->used += size;

    _count++;
    _size += size;

    return result;
}

void* FixPool::reserve(unsigned int size)
{
    if (!ensureSpace(size)) return 0;
    _reservation = size;

    return _last->space + _last->used;
}


bool FixPool::allocateReserved(unsigned int size)
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

    // we do not allow allocation sizes > CHUNK_SIZE
    if (size > CHUNK_SIZE) return false;

    newChunk = (struct SpaceChunk*) malloc(sizeof(struct SpaceChunk) +
					   CHUNK_SIZE);
    if (!newChunk) {
        qFatal("ERROR: Out of memory. Sorry. KCachegrind has to terminate.\n\n"
               "You probably tried to load a profile data file too huge for"
               "this system. You could try loading this file on a 64-bit OS.");
        exit(1);
    }
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


// DynPool

DynPool::DynPool()
{
  _data = (char*) malloc(CHUNK_SIZE);
  _used = 0;
  _size = CHUNK_SIZE;

  // end marker
  *(int*)_data = 0;
}

DynPool::~DynPool()
{
  // we could check for correctness by iteration over all objects

  ::free(_data);
}

bool DynPool::allocate(char** ptr, unsigned int size)
{
  // round up to multiple of 4
  size = (size+3) & ~3;

  /* need 12 bytes more:
   * - 4 bytes for forward chain
   * - 4 bytes for pointer to ptr
   * - 4 bytes as end marker (not used for new object)
   */
  if (!ensureSpace(size + 12)) return false;

  char** obj = (char**) (_data+_used);
  obj[0] = (char*)(_data + _used + size + 8);
  obj[1] = (char*)ptr;
  *(int*)(_data+_used+size+8) = 0;
  *ptr = _data+_used+8;

  _used += size + 8;

  return true;
}

void DynPool::free(char** ptr)
{
  if (!ptr ||
      !*ptr ||
      (*(char**)(*ptr - 4)) != (char*)ptr )
    qFatal("Chaining error in DynPool::free");

  (*(char**)(*ptr - 4)) = 0;
  *ptr = 0;
}

bool DynPool::ensureSpace(unsigned int size)
{
  if (_used + size <= _size) return true;

  unsigned int newsize = _size *3/2 + CHUNK_SIZE;
  char* newdata = (char*) malloc(newsize);

  unsigned int freed = 0, len;
  char **p, **pnext, **pnew;

  qDebug("DynPool::ensureSpace size: %d => %d, used %d. %p => %p",
	 _size, newsize, _used, _data, newdata);

  pnew = (char**) newdata;
  p = (char**) _data;
  while(*p) {
    pnext = (char**) *p;
    len = (char*)pnext - (char*)p;

    if (0) qDebug(" [%8p] Len %d (ptr %p), freed %d (=> %p)",
		  p, len, p[1], freed, pnew);

    /* skip freed space ? */
    if (p[1] == 0) {
      freed += len;
      p = pnext;
      continue;
    }
    
    // new and old still at same address ?
    if (pnew == p) {
      pnew = p = pnext;
      continue;
    }

    // copy object
    pnew[0] = (char*)pnew + len;
    pnew[1] = p[1];
    memcpy((char*)pnew + 8, (char*)p + 8, len-8);

    // update pointer to object
    char** ptr = (char**) p[1];
    if (*ptr != ((char*)p)+8)
      qFatal("Chaining error in DynPool::ensureSpace");
    *ptr = ((char*)pnew)+8;

    pnew = (char**) pnew[0];
    p = pnext;
  }
  pnew[0] = 0;

  unsigned int newused = (char*)pnew - (char*)newdata;
  qDebug("DynPool::ensureSpace size: %d => %d, used %d => %d (%d freed)",
	 _size, newsize, _used, newused, freed);
  
  ::free(_data);
  _data = newdata;
  _size = newsize;
  _used = newused;

  return true;
}

/* Testing the DynPool
int main()
{
  char* bufs[CHUNK_SIZE];
  int i;

  DynPool p;

  for(i=0;i<CHUNK_SIZE;i++) {
    p.allocate(bufs+i, 10+i%10);
    if (((i%3)==0) && (i>20))
      p.free(bufs+i-20);
  }

  for(i=0;i<CHUNK_SIZE;i++) {
    if ((bufs[i]==0) || ((i%7)==0)) continue;
    p.free(bufs+i);
  }

  for(i=0;i<CHUNK_SIZE;i++) {
    if (bufs[i]) continue;
    p.allocate(bufs+i, 10+i%10);
  }
}
*/
