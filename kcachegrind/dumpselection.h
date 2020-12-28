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

#ifndef DUMPSELECTION_H
#define DUMPSELECTION_H

#include "dumpselectionbase.h"
#include "traceitemview.h"

class DumpSelection : public DumpSelectionBase, public TraceItemView
{
  Q_OBJECT

public:
  explicit DumpSelection( TopLevel*, QWidget* parent = 0, const char* name = 0);
  virtual ~DumpSelection();

  QWidget* widget() { return this; }
};

#endif
