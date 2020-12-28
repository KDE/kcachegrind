/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
