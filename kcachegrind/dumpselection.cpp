/**
 * DumpSelection Dockable
 * Part of KCachegrind
 * 2003, Josef Weidendorfer (GPL V2)
 *
 * - Fast Selection of dumps to load/activate/use for comparing
 * - Start a profile run from GUI (current supported: Callgrind)
 * - View state of running profile runs.
 *
 */

#include "dumpselection.h"

/*
 * TODO:
 *  - Everything !!
 *  - Request State info on current function..
 *
 */


DumpSelection::DumpSelection( TopLevel* top,
                              QWidget* parent, const char* name)
    : DumpSelectionBase(parent, name), TraceItemView(0, top)
{
}

DumpSelection::~DumpSelection()
{}


#include "dumpselection.moc"

