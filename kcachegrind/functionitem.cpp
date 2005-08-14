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


//#include <math.h>

//#include <qpainter.h>
//#include <qregexp.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kapplication.h>

#include "listutils.h"
#include "functionitem.h"
#include "configuration.h"


// FunctionItem

FunctionItem::FunctionItem(QListView* parent, TraceFunction* f,
                           TraceCostType* ct, TraceCost::CostType gt)
  :QListViewItem(parent)
{
#if 0
    _costPixValid = false;
    _groupPixValid = false;
#endif

    _function = f;
    _skipped = 0;
    _groupType = TraceCost::NoCostType;
    setGroupType(gt);
    setCostType(ct);

    setText(3, f->prettyName());
    setText(4, f->prettyLocation());
}

FunctionItem::FunctionItem(QListView* parent, int skipped,
			   TraceFunction* f, TraceCostType* ct)
  :QListViewItem(parent)
{
#if 0
    _costPixValid = false;
    _groupPixValid = false;
#endif
    _skipped = skipped;
    _function = f;
    _groupType = TraceCost::NoCostType;
    setCostType(ct);

    setText(3, i18n("(%n function skipped)", "(%n functions skipped)", skipped));
}

#if 0
const QPixmap* FunctionItem::pixmap(int column) const
{
    if (column == 3) {
	if (!_groupPixValid) {
	    QColor c = Configuration::functionColor(_groupType, _function);
	    _groupPix = colorPixmap(10, 10, c);
	    _groupPixValid = true;
	}
	return &_groupPix;
    }
    if (column == 1) {
	if (!_costPixValid) {
	    _costPix = colorPixmap(10, 10, c);
	    _costPixValid = true;
	}
	return &_costPix;
    }
    return 0;
}
#endif

void  FunctionItem::setGroupType(TraceCost::CostType gt)
{
  if (_skipped) return;
  if (_groupType == gt) return;
  _groupType = gt;


#if 0
  _groupPixValid = false;
  viewList()->repaint();
#else
  QColor c = Configuration::functionColor(_groupType, _function);
  setPixmap(3, colorPixmap(10, 10, c));
#endif
}

void FunctionItem::setCostType(TraceCostType* c)
{
  _costType = c;
  update();
}

void FunctionItem::update()
{
  double inclTotal = _function->data()->subCost(_costType);
  QString str;

  TraceCost* selfCost = _function->data();
  if (Configuration::showExpanded()) {
      switch(_groupType) {
      case TraceCost::Object: selfCost = _function->object(); break;
      case TraceCost::Class:  selfCost = _function->cls(); break;
      case TraceCost::File:   selfCost = _function->file(); break;
      default: break;
      }
  }
  double selfTotal = selfCost->subCost(_costType);

  if (_skipped) {
    // special handling for skip entries...

    // only text updates of incl./self

    // for all skipped functions, cost is below the given function
    _sum  = _function->inclusive()->subCost(_costType);
    double incl  = 100.0 * _sum / inclTotal;
    if (Configuration::showPercentage())
      str = QString("%1").arg(incl, 0, 'f', Configuration::percentPrecision());
    else
      str = _function->inclusive()->prettySubCost(_costType);
    str = "< " + str;
    setText(0, str);
    setText(1, str);
    return;
  }

  // Call count...
  if (_function->calledCount() >0)
    str = _function->prettyCalledCount();
  else {
    if (_function == _function->cycle())
      str = QString("-");
    else
      str = QString("(0)");
  }
  setText(2, str);

  // Incl. cost
  _sum  = _function->inclusive()->subCost(_costType);
  if (inclTotal == 0.0) {
    setPixmap(0, QPixmap());
    setText(0, "-");
  }
  else {
      double incl  = 100.0 * _sum / inclTotal;
      if (Configuration::showPercentage())
	  setText(0, QString("%1")
		  .arg(incl, 0, 'f', Configuration::percentPrecision()));
      else
	  setText(0, _function->inclusive()->prettySubCost(_costType));

      setPixmap(0, costPixmap(_costType, _function->inclusive(), inclTotal, false));
  }

  // self
  _pure = _function->subCost(_costType);
  if (selfTotal == 0.0) {
    setPixmap(1, QPixmap());
    setText(1, "-");
  }
  else {
      double self  = 100.0 * _pure / selfTotal;

      if (Configuration::showPercentage())
	  setText(1, QString("%1")
		  .arg(self, 0, 'f', Configuration::percentPrecision()));
      else
	  setText(1, _function->prettySubCost(_costType));

      setPixmap(1, costPixmap(_costType, _function, selfTotal, false));
  }
}


int FunctionItem::compare(QListViewItem * i, int col, bool ascending ) const
{
  const FunctionItem* fi1 = this;
  const FunctionItem* fi2 = (FunctionItem*) i;

  // we always want descending order
  if (ascending) {
    fi1 = fi2;
    fi2 = this;
  }

  // a skip entry is always sorted last
  if (fi1->_skipped) return -1;
  if (fi2->_skipped) return 1;

  if (col==0) {
    if (fi1->_sum < fi2->_sum) return -1;
    if (fi1->_sum > fi2->_sum) return 1;
    return 0;
  }
  if (col==1) {
    if (fi1->_pure < fi2->_pure) return -1;
    if (fi1->_pure > fi2->_pure) return 1;
    return 0;
  }
  if (col==2) {
    if (fi1->_function->calledCount() <
	fi2->_function->calledCount()) return -1;
    if (fi1->_function->calledCount() >
	fi2->_function->calledCount()) return 1;
    return 0;
  }

  return QListViewItem::compare(i, col, ascending);
}

