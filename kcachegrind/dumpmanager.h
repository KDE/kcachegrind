/**
 * DumpManager
 * Part of KCachegrind
 * 2003, Josef Weidendorfer (GPL V2)
 *
 * DumpManager is a Singleton.
 * - Has List of current loaded dumps / loadable dumps
 * - Does "communication" with current running profiles
 *   for dump selection dockable
 */

#ifndef DUMPMANAGER_H
#define DUMPMANAGER_H

#include <qstring.h>
#include <qptrlist.h>

class Dump;
class TraceData;

typedef QPtrList<Dump> DumpList;


/**
 * A loadable profile Dump
 */
class Dump
{
public:
  Dump(QString);

  QString filename() { return _filename; }

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
