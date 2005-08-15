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
 * Items of source view.
 */

#include <qpixmap.h>
#include <qregexp.h>
#include <qpainter.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kapplication.h>

#include "configuration.h"
#include "listutils.h"
#include "sourceview.h"
#include "sourceitem.h"


// SourceItem

// for source lines
SourceItem::SourceItem(SourceView* sv, QListView* parent,
		       int fileno, unsigned int lineno,
                       bool inside, const QString& src,
                       TraceLine* line)
    : QListViewItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = inside;
  _line = line;
  _lineCall = 0;
  _lineJump = 0;

  if (src == "...")
      setText(0, src);
  else
      setText(0, QString::number(lineno));

  QString s = src;
  setText(4, s.replace( QRegExp("\t"), "        " ));

  updateGroup();
  updateCost();
}

// for call lines
SourceItem::SourceItem(SourceView* sv, QListViewItem* parent,
		       int fileno, unsigned int lineno,
                       TraceLine* line, TraceLineCall* lineCall)
    : QListViewItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = true;
  _line = line;
  _lineCall = lineCall;
  _lineJump = 0;

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().ascii());

  SubCost cc = _lineCall->callCount();
  QString templ = "  ";
  if (cc==0)
    templ += i18n("Active call to '%1'");
  else
    templ += i18n("%n call to '%1'", "%n calls to '%1'", cc);

  QString callStr = templ.arg(_lineCall->call()->calledName());
  TraceFunction* calledF = _lineCall->call()->called();
  calledF->addPrettyLocation(callStr);

  setText(4, callStr);

  updateGroup();
  updateCost();
}

// for jump lines
SourceItem::SourceItem(SourceView* sv, QListViewItem* parent,
		       int fileno, unsigned int lineno,
                       TraceLine* line, TraceLineJump* lineJump)
    : QListViewItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = true;
  _line = line;
  _lineCall = 0;
  _lineJump = lineJump;

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().ascii());

  QString to;
  if (_lineJump->lineTo()->functionSource() == _line->functionSource())
      to = _lineJump->lineTo()->name();
  else
      to = _lineJump->lineTo()->prettyName();

  QString jStr;
  if (_lineJump->isCondJump())
      jStr = i18n("Jump %1 of %2 times to %3")
	  .arg(_lineJump->followedCount().pretty())
	  .arg(_lineJump->executedCount().pretty())
	  .arg(to);
  else
      jStr = i18n("Jump %1 times to %2")
	  .arg(_lineJump->executedCount().pretty())
	  .arg(to);

  setText(4, jStr);
}


void SourceItem::updateGroup()
{
  if (!_lineCall) return;

  TraceFunction* f = _lineCall->call()->called();
  QColor c = Configuration::functionColor(_view->groupType(), f);
  setPixmap(4, colorPixmap(10, 10, c));
}

void SourceItem::updateCost()
{
  _pure = SubCost(0);
  _pure2 = SubCost(0);

  if (!_line) return;
  if (_lineJump) return;

  TraceCost* lineCost = _lineCall ? (TraceCost*)_lineCall : (TraceCost*)_line;

  // don't show any cost inside of cycles
  if (_lineCall &&
      ((_lineCall->call()->inCycle()>0) ||
       (_lineCall->call()->isRecursion()>0))) {
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
    setText(2, str);
    setPixmap(2, p);
    return;
  }

  TraceCost* totalCost;
  if (Configuration::showExpanded())
    totalCost = _line->functionSource()->function()->inclusive();
  else
    totalCost = _line->functionSource()->function()->data();

  TraceCostType* ct = _view->costType();
  _pure = ct ? lineCost->subCost(ct) : SubCost(0);
  if (_pure == 0) {
    setText(1, QString::null);
    setPixmap(1, QPixmap());
  }
  else {
    double total = totalCost->subCost(ct);
    double pure  = 100.0 * _pure / total;

    if (Configuration::showPercentage())
      setText(1, QString("%1")
	      .arg(pure, 0, 'f', Configuration::percentPrecision()));
    else
      setText(1, _pure.pretty());

    setPixmap(1, costPixmap(ct, lineCost, total, false));
  }

  TraceCostType* ct2 = _view->costType2();
  _pure2 = ct2 ? lineCost->subCost(ct2) : SubCost(0);
  if (_pure2 == 0) {
    setText(2, QString::null);
    setPixmap(2, QPixmap());
  }
  else {
    double total = totalCost->subCost(ct2);
    double pure2  = 100.0 * _pure2 / total;

    if (Configuration::showPercentage())
      setText(2, QString("%1")
	      .arg(pure2, 0, 'f', Configuration::percentPrecision()));
    else
      setText(2, _pure2.pretty());

    setPixmap(2, costPixmap(ct2, lineCost, total, false));
  }
}


int SourceItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  const SourceItem* si1 = this;
  const SourceItem* si2 = (SourceItem*) i;

  // we always want descending order
  if (((col>0) && ascending) ||
      ((col==0) && !ascending) ) {
    si1 = si2;
    si2 = this;
  }

  if (col==1) {
    if (si1->_pure < si2->_pure) return -1;
    if (si1->_pure > si2->_pure) return 1;
    return 0;
  }
  if (col==2) {
    if (si1->_pure2 < si2->_pure2) return -1;
    if (si1->_pure2 > si2->_pure2) return 1;
    return 0;
  }
  if (col==0) {
    // Sort file numbers
    if (si1->_fileno < si2->_fileno) return -1;
    if (si1->_fileno > si2->_fileno) return 1;

    // Sort line numbers
    if (si1->_lineno < si2->_lineno) return -1;
    if (si1->_lineno > si2->_lineno) return 1;

    // Same line: code gets above calls/jumps
    if (!si1->_lineCall && !si1->_lineJump) return -1;
    if (!si2->_lineCall && !si2->_lineJump) return 1;

    // calls above jumps
    if (si1->_lineCall && !si2->_lineCall) return -1;
    if (si2->_lineCall && !si1->_lineCall) return 1;

    if (si1->_lineCall && si2->_lineCall) {
	// Two calls: desending sort according costs
	if (si1->_pure < si2->_pure) return 1;
	if (si1->_pure > si2->_pure) return -1;

	// Two calls: sort according function names
	TraceFunction* f1 = si1->_lineCall->call()->called();
	TraceFunction* f2 = si2->_lineCall->call()->called();
	if (f1->prettyName() > f2->prettyName()) return 1;
	return -1;
    }

    // Two jumps: descending sort according target line
    if (si1->_lineJump->lineTo()->lineno() <
	si2->_lineJump->lineTo()->lineno())
	return -1;
    if (si1->_lineJump->lineTo()->lineno() >
	si2->_lineJump->lineTo()->lineno())
	return 1;
    return 0;
  }
  return QListViewItem::compare(i, col, ascending);
}

void SourceItem::paintCell( QPainter *p, const QColorGroup &cg,
                            int column, int width, int alignment )
{
  QColorGroup _cg( cg );

  if ( !_inside || ((column==1) || (column==2)))
    _cg.setColor( QColorGroup::Base, cg.button() );
  else if ((_lineCall || _lineJump) && column>2)
   _cg.setColor( QColorGroup::Base, cg.midlight() );

  if (column == 3)
      paintArrows(p, _cg, width);
  else
      QListViewItem::paintCell( p, _cg, column, width, alignment );
}

void SourceItem::setJumpArray(const QMemArray<TraceLineJump*>& a)
{
    _jump.duplicate(a);
}

void SourceItem::paintArrows(QPainter *p, const QColorGroup &cg, int width)
{
  QListView *lv = listView();
  if ( !lv ) return;
  SourceView* sv = (SourceView*) lv;

  const BackgroundMode bgmode = lv->viewport()->backgroundMode();
  const QColorGroup::ColorRole crole
    = QPalette::backgroundRoleFromMode( bgmode );
  if ( cg.brush( crole ) != lv->colorGroup().brush( crole ) )
    p->fillRect( 0, 0, width, height(), cg.brush( crole ) );
  else
    sv->paintEmptyArea( p, QRect( 0, 0, width, height() ) );

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
      if (_lineJump &&
	  (_lineJump->lineTo() == _jump[i]->lineTo()) &&
	  (_jump[i]->lineFrom()->lineno() == _lineno)) {

	  if (start<0) start = i;
	  if (_lineJump == _jump[i]) {
	      if (_jump[i]->lineTo()->lineno() <= _lineno)
		  y2 = yy;
	      else
		  y1 = yy;
	  }
      }
      else if (!_lineJump && !_lineCall &&
	       (_jump[i]->lineTo()->lineno() == _lineno)) {
	  if (end<0) end = i;
	  if (_jump[i]->lineFrom()->lineno() < _lineno)
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
      w = 6*(sv->arrowLevels() - start) + 10;
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
      w = 6*(sv->arrowLevels() - end) + 10;

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

      if (_jump[i]->lineFrom()->lineno() == _lineno) {
	  bool drawUp = true;
	  if (_jump[i]->lineTo()->lineno() == _lineno)
	      if (start<0) drawUp = false;
	  if (_jump[i]->lineTo()->lineno() > _lineno) drawUp = false;
	  if (drawUp)
	      p->fillRect( marg + 6*i +1, 0, 2, yy, c);
	  else
	      p->fillRect( marg + 6*i +1, yy, 2, height()-yy, c);
      }
      else if (_jump[i]->lineTo()->lineno() == _lineno) {
	  if (end<0) end = i;
	  if (_jump[i]->lineFrom()->lineno() < _lineno)
	      p->fillRect( marg + 6*i +1, 0, 2, yy, c);
	  else
	      p->fillRect( marg + 6*i +1, yy, 2, height()-yy, c);
      }
  }

}

int SourceItem::width( const QFontMetrics& fm,
		       const QListView* lv, int c ) const
{
  if (c != 3) return QListViewItem::width(fm, lv, c);

  SourceView* sv = (SourceView*) lv;
  int levels = sv->arrowLevels();

  if (levels == 0) return 0;

  // 10 pixels for the arrow
  return 10 + 6*levels + lv->itemMargin() * 2;
}

