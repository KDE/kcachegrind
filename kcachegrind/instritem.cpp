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

#include <qpixmap.h>
#include <qpainter.h>

#include <klocale.h>
#include <kapplication.h>
#include <kiconloader.h>

#include "configuration.h"
#include "listutils.h"
#include "instritem.h"
#include "instrview.h"


// InstrItem

// for messages
InstrItem::InstrItem(QListView* parent, uint addr, const QString& msg)
    : QListViewItem(parent)
{
    _addr = addr;
    _instr = 0;
    _instrCall = 0;
    _instrJump = 0;
    _inside = false;

    setText(0, QString::number(addr,16));
    setText(5, msg);

    setCostType(0);
    setGroupType(TraceCost::NoCostType);
}

// for code lines
InstrItem::InstrItem(QListView* parent, uint addr, bool inside,
		     const QString& code, const QString& cmd,
		     const QString& args,
		     TraceInstr* instr, TraceCostType* ct)
    : QListViewItem(parent)
{
  _addr = addr;
  _instr = instr;
  _instrCall = 0;
  _instrJump = 0;
  _inside = inside;

  if (args == "...")
      setText(0, args);
  else
      setText(0, QString::number(addr,16));
  setText(3, code);
  setText(4, cmd);
  setText(5, args);

  TraceLine* l;
  if (instr && (l = instr->line()))
      setText(6, l->name());

  setCostType(ct);
  setGroupType(TraceCost::NoCostType);
}

// for call lines
InstrItem::InstrItem(QListViewItem* parent, uint addr,
		     TraceInstr* instr, TraceInstrCall* instrCall,
		     TraceCostType* ct,
		     TraceCost::CostType gt)
    : QListViewItem(parent)
{
  _addr = addr;
  _instr = instr;
  _instrCall = instrCall;
  _instrJump = 0;
  _inside = true;

  //qDebug("InstrItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().ascii());

  SubCost cc = _instrCall->callCount();
  QString templ = "  ";
  if (cc==0)
    templ += i18n("Active call to '%1'");
  else
    templ += i18n("%n call to '%1'", "%n calls to '%1'", cc);

  QString callStr = templ.arg(_instrCall->call()->calledName());
  TraceFunction* calledF = _instrCall->call()->called();
  if (calledF->object() && calledF->object()->name() != QString("???"))
    callStr += QString(" (%1)").arg(calledF->object()->shortName());

  setText(5, callStr);

  setCostType(ct);
  setGroupType(gt);
}

// for jump lines
InstrItem::InstrItem(QListViewItem* parent, uint addr,
		     TraceInstr* instr, TraceInstrJump* instrJump)
    : QListViewItem(parent)
{
  _addr = addr;
  _inside = true;
  _instr = instr;
  _instrCall = 0;
  _instrJump = instrJump;

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().ascii());

  QString jStr;
  if (_instrJump->isCondJump())
      jStr = i18n("Jump %1 of %2 times to 0x%3")
	  .arg(_instrJump->followedCount().pretty())
	  .arg(_instrJump->executedCount().pretty())
	  .arg(_instrJump->instrTo()->addr(), 0, 16);
  else
      jStr = i18n("Jump %1 times to 0x%2")
	  .arg(_instrJump->executedCount().pretty())
	  .arg(_instrJump->instrTo()->addr(), 0, 16);

  setText(5, jStr);

  setCostType(0);
  setGroupType(TraceCost::NoCostType);
}


void InstrItem::setCostType(TraceCostType* ct)
{
  _costType = ct;
  update();
}

void InstrItem::setGroupType(TraceCost::CostType gt)
{
  _groupType = gt;
  if (!_instrCall) return;

  TraceFunction* f = _instrCall->call()->called();
  QColor c = Configuration::functionColor(_groupType, f);
  setPixmap(5, colorPixmap(10, 10, c));
}

void InstrItem::update()
{
  if (!_instr) return;
  if (_instrJump) return;

  TraceCost* instrCost = _instrCall ?
      (TraceCost*)_instrCall : (TraceCost*)_instr;
  _pure = instrCost->subCost(_costType);

  // don't show any cost inside of cycles
  if (_instrCall &&
      ((_instrCall->call()->inCycle()>0) ||
       (_instrCall->call()->isRecursion()>0))) {
    QString str;
    QPixmap p;

    QString icon = "undo";
    KIconLoader* loader = KApplication::kApplication()->iconLoader();
    p= loader->loadIcon(icon, KIcon::Small, 0,
                        KIcon::DefaultState, 0, true);
    if (p.isNull())
      str = i18n("(cycle)");

    setText(1, str);
    setPixmap(1, p);
    return;
  }

  if (_pure == 0) {
    setText(1, QString::null);
    setPixmap(1, QPixmap());
    return;
  }

  TraceCost* totalCost;
  if (Configuration::showExpanded())
      totalCost = _instr->function()->cumulative();
  else
      totalCost = _instr->function()->data();

  double total = totalCost->subCost(_costType);
  double pure  = 100.0 * _pure / total;

  if (Configuration::showPercentage())
    setText(1, QString("%1")
            .arg(pure, 0, 'f', Configuration::percentPrecision()));
  else
    setText(1, _pure.pretty());

  setPixmap(1, costPixmap(_costType, instrCost, total));
}


int InstrItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  InstrItem* ii = (InstrItem*) i;
  if (col==1) {
    if (_pure < ii->_pure) return -1;
    if (_pure > ii->_pure) return 1;
    return 0;
  }
  if (col==0) {
    if (_addr < ii->_addr) return -1;
    if (_addr > ii->_addr) return 1;

    // Same address: code gets above calls/jumps
    if (!_instrCall && !_instrJump) return -1;
    if (!ii->_instrCall && !ii->_instrJump) return 1;

    // calls above jumps
    if (_instrCall && !ii->_instrCall) return -1;
    if (ii->_instrCall && !_instrCall) return 1;

    if (_instrCall && ii->_instrCall) {
	// Two calls: desending sort according costs
	if (_pure < ii->_pure) return 1;
	if (_pure > ii->_pure) return -1;

	// Two calls: sort according function names
	TraceFunction* f1 = _instrCall->call()->called();
	TraceFunction* f2 = ii->_instrCall->call()->called();
	if (f1->prettyName() > f2->prettyName()) return 1;
	return -1;
    }

    // Two jumps: descending sort according target address
    if (_instrJump->instrTo()->addr() < ii->_instrJump->instrTo()->addr())
	return -1;
    if (_instrJump->instrTo()->addr() > ii->_instrJump->instrTo()->addr())
	return 1;
    return 0;

  }
  return QListViewItem::compare(i, col, ascending);
}

void InstrItem::paintCell( QPainter *p, const QColorGroup &cg,
			   int column, int width, int alignment )
{
  QColorGroup _cg( cg );

  if ( !_inside || column==1)
    _cg.setColor( QColorGroup::Base, cg.button() );
  else if ((_instrCall || _instrJump) && column>1)
    _cg.setColor( QColorGroup::Base, cg.midlight() );

  if (column == 2)
    paintArrows(p, _cg, width);
  else
    QListViewItem::paintCell( p, _cg, column, width, alignment );
}

void InstrItem::setJumpArray(const QMemArray<TraceInstrJump*>& a)
{
    _jump.duplicate(a);
}

void InstrItem::paintArrows(QPainter *p, const QColorGroup &cg, int width)
{
  QListView *lv = listView();
  if ( !lv ) return;
  InstrView* iv = (InstrView*) lv;

  const BackgroundMode bgmode = lv->viewport()->backgroundMode();
  const QColorGroup::ColorRole crole
    = QPalette::backgroundRoleFromMode( bgmode );
  if ( cg.brush( crole ) != lv->colorGroup().brush( crole ) )
    p->fillRect( 0, 0, width, height(), cg.brush( crole ) );
  else
    iv->paintEmptyArea( p, QRect( 0, 0, width, height() ) );

  if ( isSelected() && lv->allColumnsShowFocus() )
    p->fillRect( 0, 0, width, height(), cg.brush( QColorGroup::Highlight ) );

  int marg = lv->itemMargin();
  int yy = height()/2, y1, y2;
  QColor c;

  int start = -1, end = -1;

  // draw line borders, detect start/stop of a line
  for(int i=0;i< (int)_jump.size();i++) {
      if (_jump[i] == 0) continue;

      y1 = 0;
      y2 = height();
      if ((_instrJump == _jump[i]) &&
	  (_jump[i]->instrFrom()->addr() == _addr)) {

	  if (start<0) start = i;
	  if (_jump[i]->instrTo()->addr() < _addr)
	      y2 = yy;
	  else
	      y1 = yy;
      }
      else if (!_instrJump && !_instrCall &&
	       (_jump[i]->instrTo()->addr() == _addr)) {
	  if (end<0) end = i;
	  if (_jump[i]->instrFrom()->addr() < _addr)
	      y2 = yy;
	  else
	      y1 = yy;
      }

      c = _jump[i]->isCondJump() ? red : blue;
      p->fillRect( marg + 6*i, y1, 4, y2, c);
      p->setPen(c.light());
      p->drawLine( marg + 6*i, y1, marg + 6*i, y2);
      p->setPen(c.dark());
      p->drawLine( marg + 6*i +3, y1, marg + 6*i +3, y2);
  }

  // draw start/stop horizontal line
  int x, y = yy-2, w, h = 4;
  if (start >= 0) {
      c = _jump[start]->isCondJump() ? red : blue;
      x = marg + 6*start;
      w = 6*(iv->arrowLevels() - start) + 10;
      p->fillRect( x, y, w, h, c);
      p->setPen(c.light());
      p->drawLine(x, y, x+w-1, y);
      p->drawLine(x, y, x, y+h-1);
      p->setPen(c.dark());
      p->drawLine(x+w-1, y, x+w-1, y+h-1);
      p->drawLine(x+1, y+h-1, x+w-1, y+h-1);
  }
  if (end >= 0) {
      c = _jump[end]->isCondJump() ? red : blue;
      x = marg + 6*end;
      w = 6*(iv->arrowLevels() - end) + 10;

      QPointArray a;
      a.putPoints(0, 7, x, y+h,
		  x,y, x+w-8, y, x+w-8, y-2,
		  x+w, yy,
		  x+w-8, y+h+2, x+w-8, y+h);
      p->setBrush(c);
      p->drawConvexPolygon(a);

      p->setPen(c.light());
      p->drawPolyline(a, 0, 5);
      p->setPen(c.dark());
      p->drawPolyline(a, 4, 2);
      p->setPen(c.light());
      p->drawPolyline(a, 5, 2);
      p->setPen(c.dark());
      p->drawPolyline(a, 6, 2);
  }

  // draw inner vertical line for start/stop
  // this overwrites borders of horizontal line
  for(int i=0;i< (int)_jump.size();i++) {
      if (_jump[i] == 0) continue;

      c = _jump[i]->isCondJump() ? red : blue;

      if (_jump[i]->instrFrom()->addr() == _addr) {
	  if (_jump[i]->instrTo()->addr() < _addr)
	      p->fillRect( marg + 6*i +1, 0, 2, yy, c);
	  else
	      p->fillRect( marg + 6*i +1, yy, 2, height()-yy, c);
      }
      else if (_jump[i]->instrTo()->addr() == _addr) {
	  if (end<0) end = i;
	  if (_jump[i]->instrFrom()->addr() < _addr)
	      p->fillRect( marg + 6*i +1, 0, 2, yy, c);
	  else
	      p->fillRect( marg + 6*i +1, yy, 2, height()-yy, c);
      }
  }

}

int InstrItem::width( const QFontMetrics& fm,
                      const QListView* lv, int c ) const
{
  if (c != 2) return QListViewItem::width(fm, lv, c);

  InstrView* iv = (InstrView*) lv;
  int levels = iv->arrowLevels();

  if (levels == 0) return 0;

  // 10 pixels for the arrow
  return 10 + 6*levels + lv->itemMargin() * 2;
}

