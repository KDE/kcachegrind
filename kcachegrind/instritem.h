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
 * Items of instruction view.
 */

#ifndef INSTRITEM_H
#define INSTRITEM_H

#include <qlistview.h>
#include "tracedata.h"

class InstrItem: public QListViewItem
{

public:
  // for messages
  InstrItem(QListView* parent, Addr addr, const QString&);

  // for instruction lines
  InstrItem(QListView* parent, Addr addr, bool inside,
	    const QString&, const QString&, const QString&,
	    TraceInstr* instr, TraceCostType* ct);

  // for call instr
  InstrItem(QListViewItem* parent, Addr addr,
	    TraceInstr* instr, TraceInstrCall* instrCall,
	    TraceCostType* ct, TraceCost::CostType gt);

  // for jump lines
  InstrItem(QListViewItem* parent, Addr addr,
	    TraceInstr* instr, TraceInstrJump* instrJump);

  Addr addr() const { return _addr; }
  TraceInstr* instr() const { return _instr; }
  TraceInstrCall* instrCall() const { return _instrCall; }
  TraceInstrJump* instrJump() const { return _instrJump; }

  int compare(QListViewItem * i, int col, bool ascending ) const;

  void paintCell(QPainter *p, const QColorGroup &cg,
                 int column, int width, int alignment );
  int width( const QFontMetrics& fm,
             const QListView* lv, int c ) const;

  void setCostType(TraceCostType* ct);
  void setGroupType(TraceCost::CostType);
  void update();

  // arrow lines
  void setJumpArray(const QMemArray<TraceInstrJump*>& a);

protected:
  void paintArrows(QPainter *p, const QColorGroup &cg, int width);
  QMemArray<TraceInstrJump*> _jump;

private:
  SubCost _pure;
  TraceCostType* _costType;
  TraceCost::CostType _groupType;
  Addr _addr;
  TraceInstr* _instr;
  TraceInstrJump* _instrJump;
  TraceInstrCall* _instrCall;
  bool _inside;
};


#endif
