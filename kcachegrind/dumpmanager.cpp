/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

/*
 * DumpManager
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
