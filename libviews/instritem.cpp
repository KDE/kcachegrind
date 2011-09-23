/* This file is part of KCachegrind.
   Copyright (C) 2003-2011 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "instritem.h"

#include <QPixmap>
#include <QPainter>
#include <QPolygon>

#include "globalguiconfig.h"
#include "listutils.h"
#include "instrview.h"

//
// InstrItem
//

// for messages
InstrItem::InstrItem(InstrView* iv, QTreeWidget* parent,
		     Addr addr, const QString& msg)
    : QTreeWidgetItem(parent)
{
  _view = iv;
  _addr = addr;
  _instr = 0;
  _instrCall = 0;
  _instrJump = 0;
  _inside = false;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  setText(0, addr.pretty());
  setText(6, msg);

  updateGroup();
  updateCost();
}

// for code lines
InstrItem::InstrItem(InstrView* iv, QTreeWidget* parent,
		     Addr addr, bool inside,
		     const QString& code, const QString& cmd,
		     const QString& args, TraceInstr* instr)
    : QTreeWidgetItem(parent)
{
  _view = iv;
  _addr = addr;
  _instr = instr;
  _instrCall = 0;
  _instrJump = 0;
  _inside = inside;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  if (args == "...")
      setText(0, args);
  else
      setText(0, addr.pretty());
  setText(4, code);
  setText(5, cmd);
  setText(6, args);

  TraceLine* l;
  if (instr && (l = instr->line()))
      setText(7, l->name());

  updateGroup();
  updateCost();
}

// for call lines
InstrItem::InstrItem(InstrView* iv, QTreeWidgetItem* parent, Addr addr,
		     TraceInstr* instr, TraceInstrCall* instrCall)
    : QTreeWidgetItem(parent)
{
  _view = iv;
  _addr = addr;
  _instr = instr;
  _instrCall = instrCall;
  _instrJump = 0;
  _inside = true;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  //qDebug("InstrItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().toAscii());

  SubCost cc = _instrCall->callCount();
  QString callStr = "  ";
  if (cc==0)
    callStr += QObject::tr("Active call to '%1'")
               .arg(_instrCall->call()->calledName());
  else
    callStr += QObject::tr("%n call(s) to '%2'", "", (uint64)cc)
               .arg(_instrCall->call()->calledName());

  TraceFunction* calledF = _instrCall->call()->called();
  calledF->addPrettyLocation(callStr);

  setText(6, callStr);

  updateGroup();
  updateCost();
}

// for jump lines
InstrItem::InstrItem(InstrView* iv, QTreeWidgetItem* parent, Addr addr,
		     TraceInstr* instr, TraceInstrJump* instrJump)
    : QTreeWidgetItem(parent)
{
  _view = iv;
  _addr = addr;
  _inside = true;
  _instr = instr;
  _instrCall = 0;
  _instrJump = instrJump;

  setTextAlignment(0, Qt::AlignRight);
  setTextAlignment(1, Qt::AlignRight);
  setTextAlignment(2, Qt::AlignRight);

  //qDebug("SourceItem: (file %d, line %d) Linecall to %s",
  //       fileno, lineno, _lineCall->call()->called()->prettyName().toAscii());

  QString jStr;
  if (_instrJump->isCondJump())
      jStr = QObject::tr("Jump %1 of %2 times to 0x%3")
            .arg(_instrJump->followedCount().pretty())
            .arg(_instrJump->executedCount().pretty())
            .arg(_instrJump->instrTo()->addr().toString());
  else
      jStr = QObject::tr("Jump %1 times to 0x%2")
            .arg(_instrJump->executedCount().pretty())
            .arg(_instrJump->instrTo()->addr().toString());

  setText(6, jStr);

  updateGroup();
  updateCost();
}


void InstrItem::updateGroup()
{
  if (!_instrCall) return;

  TraceFunction* f = _instrCall->call()->called();
  QColor c = GlobalGUIConfig::functionColor(_view->groupType(), f);
  setIcon(6, colorPixmap(10, 10, c));
}

void InstrItem::updateCost()
{
  _pure = SubCost(0);
  _pure2 = SubCost(0);

  if (!_instr) return;
  if (_instrJump) return;

  ProfileCostArray* instrCost = _instrCall ?
      (ProfileCostArray*)_instrCall : (ProfileCostArray*)_instr;

  // do not show any cost inside of cycles
  if (_instrCall &&
      ((_instrCall->call()->inCycle()>0) ||
       (_instrCall->call()->isRecursion()))) {
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
      totalCost = _instr->function()->inclusive();
  else
      totalCost = _instr->function()->data();

  EventType *et = _view->eventType();
  _pure = et ? instrCost->subCost(et) : SubCost(0);
  if (_pure == 0) {
    setText(1, QString());
    setIcon(1, QPixmap());
  }
  else {
    double total = totalCost->subCost(et);
    double pure  = 100.0 * _pure / total;

    if (GlobalConfig::showPercentage())
      setText(1, QString("%1")
	      .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
    else
      setText(1, _pure.pretty());

    setIcon(1, costPixmap(et, instrCost, total, false));
  }

  EventType *ct2 = _view->eventType2();
  _pure2 = ct2 ? instrCost->subCost(ct2) : SubCost(0);
  if (_pure2 == 0) {
    setText(2, QString());
    setIcon(2, QPixmap());
  }
  else {
    double total = totalCost->subCost(ct2);
    double pure  = 100.0 * _pure2 / total;

    if (GlobalConfig::showPercentage())
      setText(2, QString("%1")
	      .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
    else
      setText(2, _pure2.pretty());

    setIcon(2, costPixmap(ct2, instrCost, total, false));
  }
}

bool InstrItem::operator<( const QTreeWidgetItem & other ) const
{
    const InstrItem* ii1 = this;
    const InstrItem* ii2 = (InstrItem*) &other;
    int col = treeWidget()->sortColumn();

    if (col==1)
        return (ii1->_pure < ii2->_pure);

    if (col==2)
        return (ii1->_pure2 < ii2->_pure2);

    if (col==0) {
        if (ii1->_addr < ii2->_addr) return true;
        if (ii1->_addr > ii2->_addr) return false;

        // Same address: code gets above calls/jumps
        if (!ii1->_instrCall && !ii1->_instrJump) return true;
        if (!ii2->_instrCall && !ii2->_instrJump) return false;

        // calls above jumps
        if (ii1->_instrCall && !ii2->_instrCall) return true;
        if (ii2->_instrCall && !ii1->_instrCall) return false;

        if (ii1->_instrCall && ii2->_instrCall) {
            // Two calls: desending sort according costs
            if (ii1->_pure < ii2->_pure) return true;
            if (ii1->_pure > ii2->_pure) return false;

            // Two calls: sort according function names
            TraceFunction* f1 = ii1->_instrCall->call()->called();
            TraceFunction* f2 = ii2->_instrCall->call()->called();
            return (f1->prettyName() < f2->prettyName());
        }

        // Two jumps: descending sort according target address
        return (ii1->_instrJump->instrTo()->addr() <
                ii2->_instrJump->instrTo()->addr());
    }
    return QTreeWidgetItem::operator<(other);
}

void InstrItem::setJumpArray(const QVector<TraceInstrJump*>& a)
{
    _jump = a;
}


//
// InstrItemDelegate
//

InstrItemDelegate::InstrItemDelegate(InstrView *parent)
{
    _parent = parent;
}

QSize InstrItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize sz = QItemDelegate::sizeHint(option, index);

    int c = index.column();
    if (c != 3) return sz;

    InstrView* iv = (InstrView*) _parent;
    int levels = iv->arrowLevels();

    if (levels == 0)
        return QSize(0, sz.height());

    // 10 pixels for the arrow, 1 pixel margin left and right
    return QSize(10 + 6*levels + 2, sz.height());
}

void InstrItemDelegate::paint(QPainter *painter,
			      const QStyleOptionViewItem &option,
			      const QModelIndex &index) const
{
    int column = index.column();
    InstrItem* item = static_cast<InstrItem*>(index.internalPointer());

    QColor color;
    if ( !item->inside() || ((column==1) || (column==2)))
        color = option.palette.color( QPalette::Button );
    else if ((item->instrCall() || item->instrJump()) && column>2)
        color = option.palette.color( QPalette::Midlight );
    if (color.isValid())
        _parent->model()->setData(index, color, Qt::BackgroundRole);

    if(column==3)
        paintArrows(painter, option, index);
    else
        QItemDelegate::paint(painter, option, index);
}

void InstrItemDelegate::paintArrows(QPainter *p,
				    const QStyleOptionViewItem &option,
				    const QModelIndex &index) const
{
    QTreeWidget* tw = _parent;
    if ( !tw ) return;
    InstrView* iv = (InstrView*) tw;
    InstrItem* item = static_cast<InstrItem*>(index.internalPointer());
    const QRect& rect = option.rect;
    int height = rect.height();

    p->save();
    drawBackground(p, option, index);
    p->translate(rect.topLeft());

    int marg = 1;
    int yy = height/2, y1, y2;
    QColor c;

    int start = -1, end = -1;

    TraceInstrJump* instrJump = item->instrJump();
    Addr addr = item->addr();
    TraceInstrCall* instrCall = item->instrCall();

    // draw line borders, detect start/stop of a line
    for(int i=0; i< item->jumpCount(); i++) {
        TraceInstrJump* jump = item->jump(i);
        if (jump == 0) continue;

        y1 = 0;
        y2 = height;
        if ((instrJump == jump) &&
            (jump->instrFrom()->addr() == addr)) {

            //qDebug() << "InstrItem " << addr.toString() << ": start " << i;
            if (start<0) start = i;
            if (jump->instrTo()->addr() <= addr)
                y2 = yy;
            else
                y1 = yy;
        }
        else if (!instrJump && !instrCall &&
                 (jump->instrTo()->addr() == addr)) {

            //qDebug() << "InstrItem " << addr.toString() << ": end " << i;
            if (end<0) end = i;
            if (jump->instrFrom()->addr() < addr)
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
        c = item->jump(end)->isCondJump() ? Qt::red : Qt::blue;
        x = marg + 6*end;
        w = 6*(iv->arrowLevels() - end) + 10;

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
    for(int i=0; i< item->jumpCount(); i++) {
        TraceInstrJump* jump = item->jump(i);
        if (jump == 0) continue;

        c = jump->isCondJump() ? Qt::red : Qt::blue;

        if (jump->instrFrom()->addr() == addr) {
            bool drawUp = true;
            if (jump->instrTo()->addr() == addr)
                if (start<0) drawUp=false;
            if (jump->instrTo()->addr() > addr) drawUp=false;
            if (drawUp)
                p->fillRect( marg + 6*i +1, 0, 2, yy, c);
            else
                p->fillRect( marg + 6*i +1, yy, 2, height-yy, c);
        }
        else if (jump->instrTo()->addr() == addr) {
            if (end<0) end = i;
            if (jump->instrFrom()->addr() < addr)
                p->fillRect( marg + 6*i +1, 0, 2, yy, c);
            else
                p->fillRect( marg + 6*i +1, yy, 2, height-yy, c);
        }
    }
    p->restore();
}

