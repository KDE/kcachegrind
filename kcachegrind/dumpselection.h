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
