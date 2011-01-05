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
 *
 * DumpManager is a Singleton.
 * - Has List of current loaded dumps / loadable dumps
 * - Does "communication" with current running profiles
 *   for dump selection dockable
 */

#ifndef DUMPMANAGER_H
#define DUMPMANAGER_H

#include <qstring.h>
#include <qlist.h>

class Dump;
class TraceData;

typedef QList<Dump*> DumpList;


/**
 * A loadable profile Dump
 */
class Dump
{
public:
  Dump(QString);

  QString filename() const { return _filename; }

private:
  QString _filename;
};


/*
 * TODO:
 * - Everything
 *
 */

class DumpManager
{
public:
  DumpManager();

  DumpManager* self();

  DumpList loadableDumps();
  TraceData* load(Dump*);

private:
  static DumpManager* _self;
};

#endif
