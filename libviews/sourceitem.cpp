/* This file is part of KCachegrind.
   Copyright (C) 2011 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Items of source view.
 */

#include "sourceitem.h"

#include <QPixmap>
#include <QRegExp>
#include <QPainter>
#include <QPolygon>
#include <QTextDocument>

#include "globalguiconfig.h"
#include "listutils.h"
#include "sourceview.h"


//
// SourceItem
//

// for source lines
SourceItem::SourceItem(SourceView* sv, QTreeWidget* parent,
		       int fileno, unsigned int lineno,
                       bool inside, const QString& src,
                       TraceLine* line)
    : QTreeWidgetItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = inside;
  _line = line;
  _lineCall = 0;
  _lineJump = 0;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

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
SourceItem::SourceItem(SourceView* sv, QTreeWidgetItem* parent,
		       int fileno, unsigned int lineno,
                       TraceLine* line, TraceLineCall* lineCall)
    : QTreeWidgetItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = true;
  _line = line;
  _lineCall = lineCall;
  _lineJump = 0;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().toAscii());

  SubCost cc = _lineCall->callCount();
  QString callStr = "  ";
  if (cc==0)
      callStr += QObject::tr("Active call to '%1'")
          .arg(_lineCall->call()->calledName());
  else
      callStr += QObject::tr("%n call(s) to '%2'", "", (uint64)cc)
          .arg(_lineCall->call()->calledName());

  TraceFunction* calledF = _lineCall->call()->called();
  calledF->addPrettyLocation(callStr);

  setText(4, callStr);

  updateGroup();
  updateCost();
}

// for jump lines
SourceItem::SourceItem(SourceView* sv, QTreeWidgetItem* parent,
		       int fileno, unsigned int lineno,
                       TraceLine* line, TraceLineJump* lineJump)
    : QTreeWidgetItem(parent)
{
  _view = sv;
  _lineno = lineno;
  _fileno = fileno;
  _inside = true;
  _line = line;
  _lineCall = 0;
  _lineJump = lineJump;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().toAscii());

  QString to;
  if (_lineJump->lineTo()->functionSource() == _line->functionSource())
      to = _lineJump->lineTo()->name();
  else
      to = _lineJump->lineTo()->prettyName();

  QString jStr;
  if (_lineJump->isCondJump())
      jStr = QObject::tr("Jump %1 of %2 times to %3")
	  .arg(_lineJump->followedCount().pretty())
	  .arg(_lineJump->executedCount().pretty())
	  .arg(to);
  else
      jStr = QObject::tr("Jump %1 times to %2")
	  .arg(_lineJump->executedCount().pretty())
	  .arg(to);

  setText(4, jStr);
}


void SourceItem::updateGroup()
{
  if (!_lineCall) return;

  TraceFunction* f = _lineCall->call()->called();
  QColor c = GlobalGUIConfig::functionColor(_view->groupType(), f);
  setIcon(4, colorPixmap(10, 10, c));
}

void SourceItem::updateCost()
{
  _pure = SubCost(0);
  _pure2 = SubCost(0);

  if (!_line) return;
  if (_lineJump) return;

  ProfileCostArray* lineCost = _lineCall ? (ProfileCostArray*)_lineCall : (ProfileCostArray*)_line;

  // do not show any cost inside of cycles
  if (_lineCall &&
      ((_lineCall->call()->inCycle()>0) ||
       (_lineCall->call()->isRecursion()))) {
    QString str;
    QPixmap p;

    QString icon = "edit-undo";
#if 0 // TODO
    KIconLoader* loader = KIconLoader::global();
    p= loader->loadIcon(icon, KIconLoader::Small, 0,
                        KIconLoader::DefaultState, QStringList(), 0, true);
#endif
    if (p.isNull())
      str = QObject::tr("(cycle)");

    setText(1, str);
    setIcon(1, p);
    setText(2, str);
    setIcon(2, p);
    return;
  }

  ProfileCostArray* totalCost;
  if (GlobalConfig::showExpanded())
    totalCost = _line->functionSource()->function()->inclusive();
  else
    totalCost = _line->functionSource()->function()->data();

  EventType* ct = _view->eventType();
  _pure = ct ? lineCost->subCost(ct) : SubCost(0);
  if (_pure == 0) {
    setText(1, QString());
    setIcon(1, QPixmap());
  }
  else {
    double total = totalCost->subCost(ct);
    double pure  = 100.0 * _pure / total;

    if (GlobalConfig::showPercentage())
      setText(1, QString("%1")
	      .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
    else
      setText(1, _pure.pretty());

    setIcon(1, costPixmap(ct, lineCost, total, false));
  }

  EventType* ct2 = _view->eventType2();
  _pure2 = ct2 ? lineCost->subCost(ct2) : SubCost(0);
  if (_pure2 == 0) {
    setText(2, QString());
    setIcon(2, QPixmap());
  }
  else {
    double total = totalCost->subCost(ct2);
    double pure2  = 100.0 * _pure2 / total;

    if (GlobalConfig::showPercentage())
      setText(2, QString("%1")
	      .arg(pure2, 0, 'f', GlobalConfig::percentPrecision()));
    else
      setText(2, _pure2.pretty());

    setIcon(2, costPixmap(ct2, lineCost, total, false));
  }
}


bool SourceItem::operator < ( const QTreeWidgetItem & other ) const
{
  const SourceItem* si1 = this;
  const SourceItem* si2 = (SourceItem*) &other;
  int col = treeWidget()->sortColumn();

  if (col==1) {
    return (si1->_pure < si2->_pure);
  }
  if (col==2) {
    return (si1->_pure2 < si2->_pure2);
  }
  if (col==0) {
    // Sort file numbers
    if (si1->_fileno < si2->_fileno) return true;
    if (si1->_fileno > si2->_fileno) return false;

    // Sort line numbers
    if (si1->_lineno < si2->_lineno) return true;
    if (si1->_lineno > si2->_lineno) return false;

    // Same line: code gets above calls/jumps
    if (!si1->_lineCall && !si1->_lineJump) return true;
    if (!si2->_lineCall && !si2->_lineJump) return false;

    // calls above jumps
    if (si1->_lineCall && !si2->_lineCall) return true;
    if (si2->_lineCall && !si1->_lineCall) return false;

    if (si1->_lineCall && si2->_lineCall) {
	// Two calls: desending sort according costs
        if (si1->_pure < si2->_pure) return true;
        if (si1->_pure > si2->_pure) return false;

	// Two calls: sort according function names
	TraceFunction* f1 = si1->_lineCall->call()->called();
	TraceFunction* f2 = si2->_lineCall->call()->called();
        if (f1->prettyName() > f2->prettyName()) return false;
        return true;
    }

    // Two jumps: descending sort according target line
    return (si1->_lineJump->lineTo()->lineno() <
        si2->_lineJump->lineTo()->lineno());
  }
  return QTreeWidgetItem::operator <(other);
}

void SourceItem::setJumpArray(const QVector<TraceLineJump*>& a)
{
    _jump = a;
}



//
// SourceItemDelegate
//

SourceItemDelegate::SourceItemDelegate(SourceView *parent)
{
    _parent = parent;
}

QSize SourceItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QSize sz = QItemDelegate::sizeHint(option, index);

    int c = index.column();
    if (c != 3) return sz;

    SourceView* sv = (SourceView*) _parent;
    int levels = sv->arrowLevels();

    if (levels == 0)
      return QSize(0, sz.height());

    // 10 pixels for the arrow, 1 pixel margin left and right
    return QSize(10 + 6*levels + 2, sz.height());
}

void SourceItemDelegate::paint(QPainter *painter,
			       const QStyleOptionViewItem &option,
			       const QModelIndex &index) const
{
    int column = index.column();
    SourceItem* item = static_cast<SourceItem*>(index.internalPointer());

    QColor color;
    if ( !item->inside() || ((column==1) || (column==2)))
      color = option.palette.color( QPalette::Button );
    else if ((item->lineCall() || item->lineJump()) && column>2)
      color = option.palette.color( QPalette::Midlight );
    if (color.isValid())
        _parent->model()->setData(index, color, Qt::BackgroundRole);

    if(column==3)
        paintArrows(painter, option, index);
    else
        QItemDelegate::paint(painter, option, index);
}

void SourceItemDelegate::paintArrows(QPainter *p,
				     const QStyleOptionViewItem &option,
				     const QModelIndex &index) const
{
    QTreeWidget *lv = _parent;
    if ( !lv ) return;
    SourceView* sv = (SourceView*) lv;
    SourceItem* item = static_cast<SourceItem*>(index.internalPointer());
    const QRect& rect = option.rect;
    int height = rect.height();

    p->save();
    drawBackground(p, option, index);
    p->translate(rect.topLeft());

    int marg = 1;
    int yy = height/2, y1, y2;
    QColor c;

    int start = -1, end = -1;

    TraceLineJump* lineJump = item->lineJump();
    uint lineno = item->lineno();
    TraceLineCall* lineCall = item->lineCall();

    // draw line borders, detect start/stop of a line
    for(int i=0; i< item->jumpCount(); i++) {
        TraceLineJump* jump = item->jump(i);
        if (jump == 0) continue;

        y1 = 0;
        y2 = height;
        if (lineJump &&
            (lineJump->lineTo() == jump->lineTo()) &&
            (jump->lineFrom()->lineno() == lineno)) {

            if (start<0) start = i;
            if (lineJump == jump) {
                if (jump->lineTo()->lineno() <= lineno)
                    y2 = yy;
                else
                    y1 = yy;
            }
        }
        else if (!lineJump && !lineCall &&
                 (jump->lineTo()->lineno() == lineno)) {
            if (end<0) end = i;
            if (jump->lineFrom()->lineno() < lineno)
                y2 = yy;
            else
                y1 = yy;
        }

        c = jump->isCondJump() ? Qt::red : Qt::blue;
        p->fillRect( marg + 6*i, y1, 4, y2, c);
        p->setPen(c.light());
        p->drawLine( marg + 6*i, y1, marg + 6*i, y2);
        p->setPen(c.dark());
        p->drawLine( marg + 6*i +3, y1, marg + 6*i +3, y2);
    }

    // draw start/stop horizontal line
    int x, y = yy-2, w, h = 4;
    if (start >= 0) {
        c = item->jump(start)->isCondJump() ? Qt::red : Qt::blue;
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
        c = item->jump(end)->isCondJump() ? Qt::red : Qt::blue;
        x = marg + 6*end;
        w = 6*(sv->arrowLevels() - end) + 10;

        QPolygon a;
        a.putPoints(0, 8, x,y+h,
                    x,y, x+w-8,y, x+w-8,y-2,
                    x+w,yy,
                    x+w-8,y+h+2, x+w-8,y+h,
                    x,y+h);
        p->setBrush(c);
        p->drawConvexPolygon(a);

        p->setPen(c.light());
        p->drawPolyline(a.constData(), 5);
        p->setPen(c.dark());
        p->drawPolyline(a.constData() + 4, 2);
        p->setPen(c.light());
        p->drawPolyline(a.constData() + 5, 2);
        p->setPen(c.dark());
        p->drawPolyline(a.constData() + 6, 2);
    }

    // draw inner vertical line for start/stop
    // this overwrites borders of horizontal line
    for(int i=0;i< item->jumpCount();i++) {
        TraceLineJump* jump = item->jump(i);
        if (jump == 0) continue;

        c = jump->isCondJump() ? Qt::red : Qt::blue;

        if (jump->lineFrom()->lineno() == lineno) {
            bool drawUp = true;
            if (jump->lineTo()->lineno() == lineno)
                if (start<0) drawUp = false;
            if (jump->lineTo()->lineno() > lineno) drawUp = false;
            if (drawUp)
                p->fillRect( marg + 6*i +1, 0, 2, yy, c);
            else
                p->fillRect( marg + 6*i +1, yy, 2, height-yy, c);
        }
        else if (jump->lineTo()->lineno() == lineno) {
            if (end<0) end = i;
            if (jump->lineFrom()->lineno() < lineno)
                p->fillRect( marg + 6*i +1, 0, 2, yy, c);
            else
                p->fillRect( marg + 6*i +1, yy, 2, height-yy, c);
        }
    }
    p->restore();
}
