/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * DumpSelection Dockable
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

#include "moc_dumpselection.cpp"
