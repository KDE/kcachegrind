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

#ifndef DUMPSELECTION_H
#define DUMPSELECTION_H

#include "dumpselectionbase.h"
#include "traceitemview.h"

class DumpSelection : public DumpSelectionBase, public TraceItemView
{
  Q_OBJECT

public:
  DumpSelection( TopLevel*, QWidget* parent = 0, const char* name = 0);
  virtual ~DumpSelection();

  QWidget* widget() { return this; }
};

#endif
