/**
 * DumpManager
 * Part of KCachegrind
 * 2003, Josef Weidendorfer (GPL V2)
 */

#include "dumpmanager.h"


//
// Dump
//

Dump::Dump(QString file)
{
  _filename = file;
}


//
// DumpManager
//

DumpManager* DumpManager::_self = 0;


DumpManager::DumpManager()
{
}

DumpManager* DumpManager::self()
{
  if (!_self)
    _self = new DumpManager();

  return _self;
}


DumpList DumpManager::loadableDumps()
{
  DumpList res;

  return res;
}

TraceData* DumpManager::load(Dump*)
{
  return 0;
}
