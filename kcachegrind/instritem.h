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
 * Items of instruction view.
 */

#ifndef INSTRITEM_H
#define INSTRITEM_H

#include <q3listview.h>
#include "tracedata.h"

class InstrView;

class InstrItem: public Q3ListViewItem
{

public:
  // for messages
  InstrItem(InstrView* iv, Q3ListView* parent,
	    Addr addr, const QString&);

  // for instruction lines
  InstrItem(InstrView* iv, Q3ListView* parent,
	    Addr addr, bool inside,
	    const QString&, const QString&, const QString&,
	    TraceInstr* instr);

  // for call instr
  InstrItem(InstrView* iv, Q3ListViewItem* parent, Addr addr,
	    TraceInstr* instr, TraceInstrCall* instrCall);

  // for jump lines
  InstrItem(InstrView* iv, Q3ListViewItem* parent, Addr addr,
	    TraceInstr* instr, TraceInstrJump* instrJump);

  Addr addr() const { return _addr; }
  TraceInstr* instr() const { return _instr; }
  TraceInstrCall* instrCall() const { return _instrCall; }
  TraceInstrJump* instrJump() const { return _instrJump; }

  int compare(Q3ListViewItem * i, int col, bool ascending ) const;

  void paintCell(QPainter *p, const QColorGroup &cg,
                 int column, int width, int alignment );
  int width( const QFontMetrics& fm,
             const Q3ListView* lv, int c ) const;

  void updateGroup();
  void updateCost();

  // arrow lines
  void setJumpArray(const QVector<TraceInstrJump*>& a);

protected:
  void paintArrows(QPainter *p, const QColorGroup &cg, int width);
  QVector<TraceInstrJump*> _jump;

private:
  InstrView* _view;
  SubCost _pure, _pure2;
  Addr _addr;
  TraceInstr* _instr;
  TraceInstrJump* _instrJump;
  TraceInstrCall* _instrCall;
  bool _inside;
};


#endif
