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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * List Item for the FunctionSelection list
 */

#ifndef FUNCTIONITEM_H
#define FUNCTIONITEM_H

#include <qlistview.h>
#include "tracedata.h"

class FunctionItem: public QListViewItem
{
public:    
  FunctionItem(QListView* parent, TraceFunction* function,
	       TraceCostType* ct, TraceCost::CostType gt);
  // constructor for a "Skipped ... " entry
  FunctionItem(QListView* parent, int skipped,
	       TraceFunction* function, TraceCostType* ct);

  int compare(QListViewItem * i, int col, bool ascending ) const;
  TraceFunction* function() { return (_skipped) ? 0 : _function; }
  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

#if 0
  const QPixmap* pixmap (int column) const;
  bool _costPixValid, _groupPixValid;
  QPixMap _costPix, _groupPix;
#endif

private:
  SubCost _sum, _pure;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  TraceFunction* _function;
  int _skipped;
};

#endif
