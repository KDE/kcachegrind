/* This file is part of KCachegrind.
   Copyright (C) 2002, 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * A Widget for visualizing hierarchical metrics as areas.
 * The API is similar to QListView.
 */

#include <math.h>

#include <qpainter.h>
#include <qtooltip.h>
#include <qregexp.h>
#include <qstyle.h>
#include <qpopupmenu.h>

#include <klocale.h>
#include <kconfig.h>
#include <kdebug.h>

#include "treemap.h"


// set this to 1 to enable debug output
#define DEBUG_DRAWING 0
#define MAX_FIELD 12


//
// StoredDrawParams
//
StoredDrawParams::StoredDrawParams()
{
  _selected = false;
  _current = false;
  _shaded = true;
  _rotated = false;

  _backColor = Qt::white;

  // field array has size 0
}

StoredDrawParams::StoredDrawParams(QColor c,
                                   bool selected, bool current)
{
  _backColor = c;

  _selected = selected;
  _current = current;
  _shaded = true;
  _rotated = false;
  _drawFrame = true;

  // field array has size 0
}

QString StoredDrawParams::text(int f) const
{
  if ((f<0) || (f >= (int)_field.size()))
    return QString::null;

  return _field[f].text;
}

QPixmap StoredDrawParams::pixmap(int f) const
{
  if ((f<0) || (f >= (int)_field.size()))
    return QPixmap();

  return _field[f].pix;
}

DrawParams::Position StoredDrawParams::position(int f) const
{
  if ((f<0) || (f >= (int)_field.size()))
    return Default;

  return _field[f].pos;
}

int StoredDrawParams::maxLines(int f) const
{
  if ((f<0) || (f >= (int)_field.size()))
    return 0;

  return _field[f].maxLines;
}

const QFont& StoredDrawParams::font() const
{
  static QFont* f = 0;
  if (!f) f = new QFont(QApplication::font());

  return *f;
}

void StoredDrawParams::ensureField(int f)
{
  static Field* def = 0;
  if (!def) {
    def = new Field();
    def->pos = Default;
    def->maxLines = 0;
  }

  if (f<0 || f>=MAX_FIELD) return;

  if ((int)_field.size() < f+1) _field.resize(f+1, *def);
}


void StoredDrawParams::setField(int f, const QString& t, QPixmap pm,
                           Position p, int maxLines)
{
  if (f<0 || f>=MAX_FIELD) return;
  ensureField(f);

  _field[f].text = t;
  _field[f].pix  = pm;
  _field[f].pos  = p;
  _field[f].maxLines = maxLines;
}

void StoredDrawParams::setText(int f, const QString& t)
{
  if (f<0 || f>=MAX_FIELD) return;
  ensureField(f);

  _field[f].text = t;
}

void StoredDrawParams::setPixmap(int f, const QPixmap& pm)
{
  if (f<0 || f>=MAX_FIELD) return;
  ensureField(f);

  _field[f].pix = pm;
}

void StoredDrawParams::setPosition(int f, Position p)
{
  if (f<0 || f>=MAX_FIELD) return;
  ensureField(f);

  _field[f].pos = p;
}

void StoredDrawParams::setMaxLines(int f, int m)
{
  if (f<0 || f>=MAX_FIELD) return;
  ensureField(f);

  _field[f].maxLines = m;
}



//
// RectDrawing
//

RectDrawing::RectDrawing(QRect r)
{
  _fm = 0;
  _dp = 0;
  setRect(r);
}


RectDrawing::~RectDrawing()
{
  delete _fm;
  delete _dp;
}

DrawParams* RectDrawing::drawParams()
{
  if (!_dp)
    _dp = new StoredDrawParams();

  return _dp;
}


void RectDrawing::setDrawParams(DrawParams* dp)
{
  if (_dp) delete _dp;
  _dp = dp;
}

void RectDrawing::setRect(QRect r)
{
  _rect = r;

  _usedTopLeft = 0;
  _usedTopCenter = 0;
  _usedTopRight = 0;
  _usedBottomLeft = 0;
  _usedBottomCenter = 0;
  _usedBottomRight = 0;

  _fontHeight = 0;
}

QRect RectDrawing::remainingRect(DrawParams* dp)
{
  if (!dp) dp = drawParams();

  if ((_usedTopLeft >0) ||
      (_usedTopCenter >0) ||
      (_usedTopRight >0)) {
    if (dp->rotated())
      _rect.setLeft(_rect.left() + _fontHeight);
    else
      _rect.setTop(_rect.top() + _fontHeight);
  }

  if ((_usedBottomLeft >0) ||
      (_usedBottomCenter >0) ||
      (_usedBottomRight >0)) {
    if (dp->rotated())
      _rect.setRight(_rect.right() - _fontHeight);
    else
      _rect.setBottom(_rect.bottom() - _fontHeight);
  }
  return _rect;
}


void RectDrawing::drawBack(QPainter* p, DrawParams* dp)
{
  if (!dp) dp = drawParams();
  if (_rect.width()<=0 || _rect.height()<=0) return;

  QRect r = _rect;
  QColor normal = dp->backColor();
  if (dp->selected()) normal = normal.light();
  bool isCurrent = dp->current();

  if (dp->drawFrame() || isCurrent) {
    // 3D raised/sunken frame effect...
    QColor high = normal.light();
    QColor low = normal.dark();
    p->setPen( isCurrent ? low:high);
    p->drawLine(r.left(), r.top(), r.right(), r.top());
    p->drawLine(r.left(), r.top(), r.left(), r.bottom());
    p->setPen( isCurrent ? high:low);
    p->drawLine(r.right(), r.top(), r.right(), r.bottom());
    p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
    r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
  }
  if (r.width()<=0 || r.height()<=0) return;

  if (dp->shaded()) {
    // some shading
    bool goDark = qGray(normal.rgb())>128;
    int rBase, gBase, bBase;
    normal.rgb(&rBase, &gBase, &bBase);
    p->setBrush(QBrush::NoBrush);

    // shade parameters:
    int d = 7;
    float factor = 0.1, forth=0.7, back1 =0.9, toBack2 = .7, back2 = 0.97;

    // coefficient corrections because of rectangle size
    int s = r.width();
    if (s > r.height()) s = r.height();
    if (s<100) {
      forth -= .3  * (100-s)/100;
      back1 -= .2  * (100-s)/100;
      back2 -= .02 * (100-s)/100;
    }


    // maximal color difference
    int rDiff = goDark ? -rBase/d : (255-rBase)/d;
    int gDiff = goDark ? -gBase/d : (255-gBase)/d;
    int bDiff = goDark ? -bBase/d : (255-bBase)/d;

    QColor shadeColor;
    while (factor<.95) {
      shadeColor.setRgb((int)(rBase+factor*rDiff+.5),
                        (int)(gBase+factor*gDiff+.5),
                        (int)(bBase+factor*bDiff+.5));
      p->setPen(shadeColor);
      p->drawRect(r);
      r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
      if (r.width()<=0 || r.height()<=0) return;
      factor = 1.0 - ((1.0 - factor) * forth);
    }

    // and back (1st half)
    while (factor>toBack2) {
      shadeColor.setRgb((int)(rBase+factor*rDiff+.5),
                        (int)(gBase+factor*gDiff+.5),
                        (int)(bBase+factor*bDiff+.5));
      p->setPen(shadeColor);
      p->drawRect(r);
      r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
      if (r.width()<=0 || r.height()<=0) return;
      factor = 1.0 - ((1.0 - factor) / back1);
    }

    // and back (2nd half)
    while ( factor>.01) {
      shadeColor.setRgb((int)(rBase+factor*rDiff+.5),
                        (int)(gBase+factor*gDiff+.5),
                        (int)(bBase+factor*bDiff+.5));
      p->setPen(shadeColor);
      p->drawRect(r);
      r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
      if (r.width()<=0 || r.height()<=0) return;

      factor = factor * back2;
    }
  }

  // fill inside
  p->setPen(QPen::NoPen);
  p->setBrush(normal);
  p->drawRect(r);
}


bool RectDrawing::drawField(QPainter* p, int f, DrawParams* dp)
{
  if (!dp) dp = drawParams();

  if (!_fm) {
    _fm = new QFontMetrics(dp->font());
    _fontHeight = _fm->height();
  }

  QRect r = _rect;

  if (0) kdDebug(90100) << "DrawField: Rect " << r.x() << "/" << r.y()
		   << " - " << r.width() << "x" << r.height() << endl;

  int h = _fontHeight;
  bool rotate = dp->rotated();
  int width   = (rotate ? r.height() : r.width()) -4;
  int height  = (rotate ? r.width() : r.height());
  int lines   = height / h;

  // stop if we have no space available
  if (lines<1) return false;

  // calculate free space in first line (<unused>)
  int pos = dp->position(f);
  if (pos == DrawParams::Default) {
    switch(f%4) {
    case 0: pos = DrawParams::TopLeft; break;
    case 1: pos = DrawParams::TopRight; break;
    case 2: pos = DrawParams::BottomRight; break;
    case 3: pos = DrawParams::BottomLeft; break;
    }
  }

  int unused = 0;
  bool isBottom = false;
  bool isCenter = false;
  bool isRight = false;
  int* used = 0;
  switch(pos) {
  case DrawParams::TopLeft:
    used = &_usedTopLeft;
    if (_usedTopLeft == 0) {
      if (_usedTopCenter)
        unused = (width - _usedTopCenter)/2;
      else
        unused = width - _usedTopRight;
    }
    break;

  case DrawParams::TopCenter:
    isCenter = true;
    used = &_usedTopCenter;
    if (_usedTopCenter == 0) {
      if (_usedTopLeft > _usedTopRight)
        unused = width - 2 * _usedTopLeft;
      else
        unused = width - 2 * _usedTopRight;
    }
    break;

  case DrawParams::TopRight:
    isRight = true;
    used = &_usedTopRight;
    if (_usedTopRight == 0) {
      if (_usedTopCenter)
        unused = (width - _usedTopCenter)/2;
      else
        unused = width - _usedTopLeft;
    }
    break;

  case DrawParams::BottomLeft:
    isBottom = true;
    used = &_usedBottomLeft;
    if (_usedBottomLeft == 0) {
      if (_usedBottomCenter)
        unused = (width - _usedBottomCenter)/2;
      else
        unused = width - _usedBottomRight;
    }
    break;

  case DrawParams::BottomCenter:
    isCenter = true;
    isBottom = true;
    used = &_usedBottomCenter;
    if (_usedBottomCenter == 0) {
      if (_usedBottomLeft > _usedBottomRight)
        unused = width - 2 * _usedBottomLeft;
      else
        unused = width - 2 * _usedBottomRight;
    }
    break;

  case DrawParams::BottomRight:
    isRight = true;
    isBottom = true;
    used = &_usedBottomRight;
    if (_usedBottomRight == 0) {
      if (_usedBottomCenter)
        unused = (width - _usedBottomCenter)/2;
      else
        unused = width - _usedBottomLeft;
    }
    break;
  }

  if (isBottom) {
    if ((_usedTopLeft >0) ||
        (_usedTopCenter >0) ||
        (_usedTopRight >0))
      lines--;
  }
  else if (!isBottom) {
    if ((_usedBottomLeft >0) ||
        (_usedBottomCenter >0) ||
        (_usedBottomRight >0))
      lines--;
  }
  if (lines<1) return false;


  int y = isBottom ? height - h : 0;

  if (unused < 0) unused = 0;
  if (unused == 0) {
    // no space available in last line at this position
    y = isBottom ? (y-h) : (y+h);
    lines--;

    if (lines<1) return false;

    // new line: reset used space
    if (isBottom)
      _usedBottomLeft = _usedBottomCenter = _usedBottomRight = 0;
    else
      _usedTopLeft = _usedTopCenter = _usedTopRight = 0;

    unused = width;
  }

  // stop as soon as possible when there's no space for "..."
  static int dotW = 0;
  if (!dotW) dotW = _fm->width("...");
  if (width < dotW) return false;

  // get text and pixmap now, only if we need to, because it is possible
  // that they are calculated on demand (and this can take some time)
  QString name = dp->text(f);
  if (name.isEmpty()) return 0;
  QPixmap pix = dp->pixmap(f);

  // check if pixmap can be drawn
  int pixW = pix.width();
  int pixH = pix.height();
  int pixY = 0;
  bool pixDrawn = true;
  if (pixW>0) {
    pixW += 2; // X distance from pix
    if ((width < pixW + dotW) || (height < pixH)) {
      // don't draw
      pixW = 0;
    }
    else
      pixDrawn = false;
  }

  // width of text and pixmap to be drawn
  int w = pixW + _fm->width(name);

  if (0) kdDebug(90100) << "  For '" << name << "': Unused " << unused
		   << ", StrW " << w << ", Width " << width << endl;

  // if we have limited space at 1st line:
  // use it only if whole name does fit in last line...
  if ((unused < width) && (w > unused)) {
    y = isBottom ? (y-h) : (y+h);
    lines--;

    if (lines<1) return false;

    // new line: reset used space
    if (isBottom)
      _usedBottomLeft = _usedBottomCenter = _usedBottomRight = 0;
    else
      _usedTopLeft = _usedTopCenter = _usedTopRight = 0;
  }

  p->save();
  p->setPen( (qGray(dp->backColor().rgb())>100) ? Qt::black : Qt::white);
  p->setFont(dp->font());
  if (rotate) {
    //p->translate(r.x()+2, r.y()+r.height());
    p->translate(r.x(), r.y()+r.height()-2);
    p->rotate(270);
  }
  else
    p->translate(r.x()+2, r.y());


  // adjust available lines according to maxLines
  int max = dp->maxLines(f);
  if ((max > 0) && (lines>max)) lines = max;

  /* loop over name parts to break up string depending on available width.
   * every char category change is supposed a possible break,
   * with the exception Uppercase=>Lowercase.
   * It's good enough for numbers, Symbols...
   *
   * If the text is to be written at the bottom, we start with the
   * end of the string (so everything is reverted)
   */
  QString remaining;
  int origLines = lines;
  while (lines>0) {

    if (w>width && lines>1) {
      int lastBreakPos = name.length(), lastWidth = w;
      int len = name.length();
      QChar::Category caOld, ca;

      if (!isBottom) {
        // start with comparing categories of last 2 chars
        caOld = name[len-1].category();
        while (len>2) {
          len--;
          ca = name[len-1].category();
          if (ca != caOld) {
            // "Aa" has no break between...
            if (ca == QChar::Letter_Uppercase &&
                caOld == QChar::Letter_Lowercase) {
              caOld = ca;
              continue;
            }
            caOld = ca;
            lastBreakPos = len;
            w = pixW + _fm->width(name, len);
            lastWidth = w;
            if (w <= width) break;
          }
        }
        w = lastWidth;
        remaining = name.mid(lastBreakPos);
        // remove space on break point
        if (name[lastBreakPos-1].category() == QChar::Separator_Space)
          name = name.left(lastBreakPos-1);
        else
          name = name.left(lastBreakPos);
      }
      else { // bottom
        int l = len;
        caOld = name[l-len].category();
        while (len>2) {
          len--;
          ca = name[l-len].category();

          if (ca != caOld) {
            // "Aa" has no break between...
            if (caOld == QChar::Letter_Uppercase &&
                ca == QChar::Letter_Lowercase) {
              caOld = ca;
              continue;
            }
            caOld = ca;
            lastBreakPos = len;
            w = pixW + _fm->width(name.right(len));
            lastWidth = w;
            if (w <= width) break;
          }
        }
        w = lastWidth;
        remaining = name.left(l-lastBreakPos);
        // remove space on break point
        if (name[l-lastBreakPos].category() == QChar::Separator_Space)
          name = name.right(lastBreakPos-1);
        else
          name = name.right(lastBreakPos);
      }
    }
    else
      remaining = QString::null;

    /* truncate and add ... if needed */
    if (w>width) {
      int len = name.length();
      w += dotW;
      while (len>2 && (w > width)) {
        len--;
        w = pixW + _fm->width(name, len) + dotW;
      }
      // stop drawing: we cannot draw 2 chars + "..."
      if (w>width) break;

      name = name.left(len) + "...";
    }

    int x = 0;
    if (isCenter)
      x = (width - w)/2;
    else if (isRight)
      x = width - w;

    if (!pixDrawn) {
        pixY = y+(h-pixH)/2; // default: center vertically
        if (pixH > h) pixY = isBottom ? y-(pixH-h) : y;

	p->drawPixmap( x, pixY, pix);

        // for distance to next text
	pixY = isBottom ? (pixY - h - 2) : (pixY + pixH + 2);
	pixDrawn = true;
    }


    if (0) kdDebug(90100) << "  Drawing '" << name << "' at " 
		     << x+pixW << "/" << y << endl;

    p->drawText( x+pixW, y,
		 width - pixW, h,
		 Qt::AlignLeft, name);
    y = isBottom ? (y-h) : (y+h);
    lines--;

    if (remaining.isEmpty()) break;
    name = remaining;
    w = pixW + _fm->width(name);
  }

  // make sure the pix stays visible
  if (pixDrawn && (pixY>0)) {
    if (isBottom && (pixY<y)) y = pixY;
    if (!isBottom && (pixY>y)) y = pixY;
  }

  if (origLines > lines) {
    // if only 1 line written, don't reset _used* vars
    if (lines - origLines >1) {
      if (isBottom)
        _usedBottomLeft = _usedBottomCenter = _usedBottomRight = 0;
      else
        _usedTopLeft = _usedTopCenter = _usedTopRight = 0;
    }

    // take back one line
    y = isBottom ? (y+h) : (y-h);
    if (used) *used = w;
  }

  // update free space
  if (!isBottom) {
    if (rotate)
      _rect.setRect(r.x()+y, r.y(), r.width()-y, r.height());
    else
      _rect.setRect(r.x(), r.y()+y, r.width(), r.height()-y);
  }
  else {
    if (rotate)
      _rect.setRect(r.x(), r.y(), y+h, r.height());
    else
      _rect.setRect(r.x(), r.y(), r.width(), y+h);
  }

  p->restore();

  return true;
}






//
// TreeMapItemList
//

int TreeMapItemList::compareItems ( Item item1, Item item2 )
{
  bool ascending;
  int result;

  TreeMapItem* parent = ((TreeMapItem*)item1)->parent();
  // shouldn't happen
  if (!parent) return 0;

  int textNo = parent->sorting(&ascending);

  if (textNo < 0) {
    double diff = ((TreeMapItem*)item1)->value() -
                  ((TreeMapItem*)item2)->value();
    result = (diff < -.9) ? -1 : (diff > .9) ? 1 : 0;
  }
  else
    result = (((TreeMapItem*)item1)->text(textNo) <
	      ((TreeMapItem*)item2)->text(textNo)) ? -1 : 1;

  return ascending ? result : -result;
}


TreeMapItem* TreeMapItemList::commonParent()
{
  TreeMapItem* parent, *item;
  parent = first();
  if (parent)
    while( (item = next()) != 0)
      parent = parent->commonParent(item);

  return parent;
}


// TreeMapItem

TreeMapItem::TreeMapItem(TreeMapItem* parent, double value)
{
  _value = value;
  _parent = parent;

  _sum = 0;
  _children = 0;
  _widget = 0;
  _index = -1;
  _depth = -1; // not set
  _unused_self = 0;
  _freeRects = 0;

  if (_parent) {
    // take sorting from parent
    _sortTextNo = _parent->sorting(&_sortAscending);
    _parent->addItem(this);
  }
  else {
    _sortAscending = false;
    _sortTextNo = -1; // default: no sorting
  }
}


TreeMapItem::TreeMapItem(TreeMapItem* parent, double value,
                         QString text1, QString text2,
                         QString text3, QString text4)
{
  _value = value;
  _parent = parent;

  // this resizes the text vector only if needed
  if (!text4.isEmpty()) setText(3, text4);
  if (!text3.isEmpty()) setText(2, text3);
  if (!text2.isEmpty()) setText(1, text2);
  setText(0, text1);

  _sum = 0;
  _children = 0;
  _widget = 0;
  _index = -1;
  _depth = -1; // not set
  _unused_self = 0;
  _freeRects = 0;

  if (_parent) _parent->addItem(this);
}

TreeMapItem::~TreeMapItem()
{
  if (_children) delete _children;
  if (_freeRects) delete _freeRects;

  // finally, notify widget about deletion
  if (_widget) _widget->deletingItem(this);
}

void TreeMapItem::setParent(TreeMapItem* p)
{
  _parent = p;
  if (p) _widget = p->_widget;
}

bool TreeMapItem::isChildOf(TreeMapItem* item)
{
  if (!item) return false;

  TreeMapItem* i = this;
  while (i) {
    if (item == i) return true;
    i = i->_parent;
  }
  return false;
}

TreeMapItem* TreeMapItem::commonParent(TreeMapItem* item)
{
  while (item && !isChildOf(item)) {
    item = item->parent();
  }
  return item;
}

void TreeMapItem::redraw()
{
  if (_widget)
    _widget->redraw(this);
}

void TreeMapItem::clear()
{
  if (_children) {
    // delete selected items below this item from selection
    if (_widget) _widget->clearSelection(this);

    delete _children;
    _children = 0;
  }
}


// invalidates current children and forces redraw
// this is only usefull when children are created on demand in items()
void TreeMapItem::refresh()
{
  clear();
  redraw();
}


QStringList TreeMapItem::path(int textNo) const
{
  QStringList list(text(textNo));

  TreeMapItem* i = _parent;
  while (i) {
    QString text = i->text(textNo);
    if (!text.isEmpty())
        list.prepend(i->text(textNo));
    i = i->_parent;
  }
  return list;
}

int TreeMapItem::depth() const
{
  if (_depth>0) return _depth;

  if (_parent)
    return _parent->depth() + 1;
  return 1;
}


bool TreeMapItem::initialized()
{
  if (!_children) {
    _children = new TreeMapItemList;
    _children->setAutoDelete(true);
    return false;
  }
  return true;
}

void TreeMapItem::addItem(TreeMapItem* i)
{
  if (!i) return;

  if (!_children) {
    _children = new TreeMapItemList;
    _children->setAutoDelete(true);
  }
  i->setParent(this);

  if (sorting(0) == -1)
    _children->append(i); // preserve insertion order
  else
    _children->inSort(i);
}


// default implementations of virtual functions

double TreeMapItem::value() const
{
  return _value;
}

double TreeMapItem::sum() const
{
  return _sum;
}

DrawParams::Position TreeMapItem::position(int f) const
{
  Position p = StoredDrawParams::position(f);
  if (_widget && (p == Default))
    p = _widget->fieldPosition(f);

  return p;
}

// use widget font
const QFont& TreeMapItem::font() const
{
  return _widget->currentFont();
}


bool TreeMapItem::isMarked(int) const
{
    return false;
}


int TreeMapItem::borderWidth() const
{
  if (_widget)
    return _widget->borderWidth();

  return 2;
}

int TreeMapItem::sorting(bool* ascending) const
{
  if (ascending) *ascending = _sortAscending;
  return _sortTextNo;
}

// do *not* set sorting recursively
void TreeMapItem::setSorting(int textNo, bool ascending)
{
    if (_sortTextNo == textNo) {
	if(_sortAscending == ascending) return;
	if (textNo == -1) {
	    // when no sorting is done, order change doesn't do anything
	    _sortAscending = ascending;
	    return;
	}
    }
    _sortAscending = ascending;
    _sortTextNo = textNo;

    if (_children && _sortTextNo != -1) _children->sort();
}

void TreeMapItem::resort(bool recursive)
{
  if (!_children) return;

  if (_sortTextNo != -1) _children->sort();

  if (recursive)
    for (TreeMapItem* i=_children->first(); i; i=_children->next())
      i->resort(recursive);
}


TreeMapItem::SplitMode TreeMapItem::splitMode() const
{
  if (_widget)
    return _widget->splitMode();

  return Best;
}

int TreeMapItem::rtti() const
{
  return 0;
}

TreeMapItemList* TreeMapItem::children()
{
  if (!_children) {
    _children = new TreeMapItemList;
    _children->setAutoDelete(true);
  }
  return _children;
}

void TreeMapItem::clearItemRect()
{
    _rect = QRect();
    clearFreeRects();
}

void TreeMapItem::clearFreeRects()
{
    if (_freeRects) _freeRects->clear();
}

void TreeMapItem::addFreeRect(const QRect& r)
{
    // don't add invalid rects
    if ((r.width() < 1) || (r.height() < 1)) return;

    if (!_freeRects) {
	_freeRects = new QPtrList<QRect>;
	_freeRects->setAutoDelete(true);
    }

    if (0) kdDebug(90100) << "addFree(" << path(0).join("/") << ", "
		     << r.x() << "/" << r.y() << "-"
		     << r.width() << "x" << r.height() << ")" << endl;

    QRect* last = _freeRects->last();
    if (!last) {
	_freeRects->append(new QRect(r));
	return;
    }

    // join rect with last rect if possible
    // this saves memory and doesn't make the tooltip flicker

    bool replaced = false;
    if ((last->left() == r.left()) && (last->width() == r.width())) {
	if ((last->bottom()+1 == r.top()) || (r.bottom()+1 == last->top())) {
	    *last |= r;
	    replaced = true;
	}
    }
    else if ((last->top() == r.top()) && (last->height() == r.height())) {
	if ((last->right()+1 == r.left()) || (r.right()+1 == last->left())) {
	    *last |= r;
	    replaced = true;
	}
    }

    if (!replaced) {
	_freeRects->append(new QRect(r));
	return;
    }

    if (0) kdDebug(90100) << "  united with last to (" 
		     << last->x() << "/" << last->y() << "-"
		     << last->width() << "x" << last->height() << ")" << endl;
}


// Tooltips for TreeMapWidget

class TreeMapTip: public QToolTip
{
public:
  TreeMapTip( QWidget* p ):QToolTip(p) {}

protected:
    void maybeTip( const QPoint & );
};

void TreeMapTip::maybeTip( const QPoint& pos )
{
  if ( !parentWidget()->inherits( "TreeMapWidget" ) )
        return;

  TreeMapWidget* p = (TreeMapWidget*)parentWidget();
  TreeMapItem* i;
  i = p->item(pos.x(), pos.y());
  QPtrList<QRect>* rList = i ? i->freeRects() : 0;
  if (rList) {
      QRect* r;
      for(r=rList->first();r;r=rList->next())
	  if (r->contains(pos))
	      tip(*r, p->tipString(i));
  }
}



// TreeMapWidget

TreeMapWidget::TreeMapWidget(TreeMapItem* base,
			     QWidget* parent, const char* name)
    : QWidget(parent, name)
{
    _base = base;
    _base->setWidget(this);

    _font = font();
    _fontHeight = fontMetrics().height();


  // default behaviour
  _selectionMode = Single;
  _splitMode = TreeMapItem::AlwaysBest;
  _visibleWidth = 2;
  _reuseSpace = false;
  _skipIncorrectBorder = false;
  _drawSeparators = false;
  _allowRotation = true;
  _borderWidth = 2;
  _shading = true; // beautiful is default!
  _maxSelectDepth = -1; // unlimited
  _maxDrawingDepth = -1; // unlimited
  _minimalArea = -1; // unlimited
  _markNo = 0;

  for(int i=0;i<4;i++) {
    _drawFrame[i] = true;
    _transparent[i] = false;
  }

  // _stopAtText will be unset on resizing (per default)
  // _textVisible will be true on resizing (per default)
  // _forceText will be false on resizing (per default)

  // start state: _selection is an empty list
  _current = 0;
  _oldCurrent = 0;
  _pressed = 0;
  _lastOver = 0;
  _needsRefresh = _base;

  setBackgroundMode(Qt::NoBackground);
  setFocusPolicy(QWidget::StrongFocus);
  _tip = new TreeMapTip(this);
}

TreeMapWidget::~TreeMapWidget()
{
  delete _base;
  delete _tip;
}

const QFont& TreeMapWidget::currentFont() const
{
  return _font;
}

void TreeMapWidget::setSplitMode(TreeMapItem::SplitMode m)
{
  if (_splitMode == m) return;

  _splitMode = m;
  redraw();
}

TreeMapItem::SplitMode TreeMapWidget::splitMode() const
{
  return _splitMode;
}

bool TreeMapWidget::setSplitMode(QString mode)
{
  if (mode == "Bisection") setSplitMode(TreeMapItem::Bisection);
  else if (mode == "Columns") setSplitMode(TreeMapItem::Columns);
  else if (mode == "Rows") setSplitMode(TreeMapItem::Rows);
  else if (mode == "AlwaysBest") setSplitMode(TreeMapItem::AlwaysBest);
  else if (mode == "Best")       setSplitMode(TreeMapItem::Best);
  else if (mode == "HAlternate") setSplitMode(TreeMapItem::HAlternate);
  else if (mode == "VAlternate") setSplitMode(TreeMapItem::VAlternate);
  else if (mode == "Horizontal") setSplitMode(TreeMapItem::Horizontal);
  else if (mode == "Vertical")   setSplitMode(TreeMapItem::Vertical);
  else return false;

  return true;
}

QString TreeMapWidget::splitModeString() const
{
  QString mode;
  switch(splitMode()) {
    case TreeMapItem::Bisection:  mode = "Bisection"; break;
    case TreeMapItem::Columns:    mode = "Columns"; break;
    case TreeMapItem::Rows:       mode = "Rows"; break;
    case TreeMapItem::AlwaysBest: mode = "AlwaysBest"; break;
    case TreeMapItem::Best:       mode = "Best"; break;
    case TreeMapItem::HAlternate: mode = "HAlternate"; break;
    case TreeMapItem::VAlternate: mode = "VAlternate"; break;
    case TreeMapItem::Horizontal: mode = "Horizontal"; break;
    case TreeMapItem::Vertical:   mode = "Vertical"; break;
    default: mode = "Unknown"; break;
  }
  return mode;
}


void TreeMapWidget::setShadingEnabled(bool s)
{
  if (_shading == s) return;

  _shading = s;
  redraw();
}

void TreeMapWidget::drawFrame(int d, bool b)
{
  if ((d<0) || (d>=4) || (_drawFrame[d]==b)) return;

  _drawFrame[d] = b;
  redraw();
}

void TreeMapWidget::setTransparent(int d, bool b)
{
  if ((d<0) || (d>=4) || (_transparent[d]==b)) return;

  _transparent[d] = b;
  redraw();
}

void TreeMapWidget::setAllowRotation(bool enable)
{
  if (_allowRotation == enable) return;

  _allowRotation = enable;
  redraw();
}

void TreeMapWidget::setVisibleWidth(int width, bool reuseSpace)
{
  if (_visibleWidth == width && _reuseSpace == reuseSpace) return;

  _visibleWidth = width;
  _reuseSpace = reuseSpace;
  redraw();
}

void TreeMapWidget::setSkipIncorrectBorder(bool enable)
{
  if (_skipIncorrectBorder == enable) return;

  _skipIncorrectBorder = enable;
  redraw();
}

void TreeMapWidget::setBorderWidth(int w)
{
  if (_borderWidth == w) return;

  _borderWidth = w;
  redraw();
}

void TreeMapWidget::setMaxDrawingDepth(int d)
{
  if (_maxDrawingDepth == d) return;

  _maxDrawingDepth = d;
  redraw();
}

QString TreeMapWidget::defaultFieldType(int f) const
{
  return i18n("Text %1").arg(f+1);
}

QString TreeMapWidget::defaultFieldStop(int) const
{
  return QString();
}

bool TreeMapWidget::defaultFieldVisible(int f) const
{
  return (f<2);
}

bool TreeMapWidget::defaultFieldForced(int) const
{
  return false;
}

DrawParams::Position TreeMapWidget::defaultFieldPosition(int f) const
{
  switch(f%4) {
  case 0: return DrawParams::TopLeft;
  case 1: return DrawParams::TopRight;
  case 2: return DrawParams::BottomRight;
  case 3: return DrawParams::BottomLeft;
  default:break;
  }
  return DrawParams::TopLeft;
}

bool TreeMapWidget::resizeAttr(int size)
{
  if (size<0 || size>=MAX_FIELD) return false;

  if (size>(int)_attr.size()) {
    struct FieldAttr a;
    int oldSize = _attr.size();
    _attr.resize(size, a);
    while (oldSize<size) {
      _attr[oldSize].type    = defaultFieldType(oldSize);
      _attr[oldSize].stop    = defaultFieldStop(oldSize);
      _attr[oldSize].visible = defaultFieldVisible(oldSize);
      _attr[oldSize].forced  = defaultFieldForced(oldSize);
      _attr[oldSize].pos     = defaultFieldPosition(oldSize);
      oldSize++;
    }
  }
  return true;
}

void TreeMapWidget::setFieldType(int f, QString type)
{
  if (((int)_attr.size() < f+1) &&
      (type == defaultFieldType(f))) return;
  if (resizeAttr(f+1)) _attr[f].type = type;

  // no need to redraw: the type string is not visible in the TreeMap
}

QString TreeMapWidget::fieldType(int f) const
{
  if (f<0 || (int)_attr.size()<f+1) return defaultFieldType(f);
  return _attr[f].type;
}

void TreeMapWidget::setFieldStop(int f, QString stop)
{
  if (((int)_attr.size() < f+1) &&
      (stop == defaultFieldStop(f))) return;
  if (resizeAttr(f+1)) {
    _attr[f].stop = stop;
    redraw();
  }
}

QString TreeMapWidget::fieldStop(int f) const
{
  if (f<0 || (int)_attr.size()<f+1) return defaultFieldStop(f);
  return _attr[f].stop;
}

void TreeMapWidget::setFieldVisible(int f, bool enable)
{
  if (((int)_attr.size() < f+1) &&
      (enable == defaultFieldVisible(f))) return;

  if (resizeAttr(f+1)) {
    _attr[f].visible = enable;
    redraw();
  }
}

bool TreeMapWidget::fieldVisible(int f) const
{
  if (f<0 || (int)_attr.size()<f+1)
    return defaultFieldVisible(f);

  return _attr[f].visible;
}

void TreeMapWidget::setFieldForced(int f, bool enable)
{
  if (((int)_attr.size() < f+1) &&
      (enable == defaultFieldForced(f))) return;

  if (resizeAttr(f+1)) {
    _attr[f].forced = enable;
    if (_attr[f].visible) redraw();
  }
}

bool TreeMapWidget::fieldForced(int f) const
{
  if (f<0 || (int)_attr.size()<f+1)
    return defaultFieldForced(f);

  return _attr[f].forced;
}

void TreeMapWidget::setFieldPosition(int f, TreeMapItem::Position pos)
{
  if (((int)_attr.size() < f+1) &&
      (pos == defaultFieldPosition(f))) return;

  if (resizeAttr(f+1)) {
    _attr[f].pos = pos;
    if (_attr[f].visible) redraw();
  }
}

DrawParams::Position TreeMapWidget::fieldPosition(int f) const
{
  if (f<0 || (int)_attr.size()<f+1)
    return defaultFieldPosition(f);

  return _attr[f].pos;
}

void TreeMapWidget::setFieldPosition(int f, QString pos)
{
  if (pos == "TopLeft")
    setFieldPosition(f, DrawParams::TopLeft);
  else if (pos == "TopCenter")
    setFieldPosition(f, DrawParams::TopCenter);
  else if (pos == "TopRight")
    setFieldPosition(f, DrawParams::TopRight);
  else if (pos == "BottomLeft")
    setFieldPosition(f, DrawParams::BottomLeft);
  else if (pos == "BottomCenter")
    setFieldPosition(f, DrawParams::BottomCenter);
  else if (pos == "BottomRight")
    setFieldPosition(f, DrawParams::BottomRight);
  else if (pos == "Default")
    setFieldPosition(f, DrawParams::Default);
}

QString TreeMapWidget::fieldPositionString(int f) const
{
  TreeMapItem::Position pos = fieldPosition(f);
  if (pos == DrawParams::TopLeft) return QString("TopLeft");
  if (pos == DrawParams::TopCenter) return QString("TopCenter");
  if (pos == DrawParams::TopRight) return QString("TopRight");
  if (pos == DrawParams::BottomLeft) return QString("BottomLeft");
  if (pos == DrawParams::BottomCenter) return QString("BottomCenter");
  if (pos == DrawParams::BottomRight) return QString("BottomRight");
  if (pos == DrawParams::Default) return QString("Default");
  return QString("unknown");
}

void TreeMapWidget::setMinimalArea(int area)
{
  if (_minimalArea == area) return;

  _minimalArea = area;
  redraw();
}


void TreeMapWidget::deletingItem(TreeMapItem* i)
{
  // remove any references to the item to be deleted
  while(_selection.findRef(i) > -1)
    _selection.remove();

  while(_tmpSelection.findRef(i) > -1)
    _tmpSelection.remove();

  if (_current == i) _current = 0;
  if (_oldCurrent == i) _oldCurrent = 0;
  if (_pressed == i) _pressed = 0;
  if (_lastOver == i) _lastOver = 0;

  // don't redraw a deleted item
  if (_needsRefresh == i) {
    // we can savely redraw the parent, as deleting order is
    // from child to parent; i.e. i->parent() is existing. 
    _needsRefresh = i->parent();
  }
}


QString TreeMapWidget::tipString(TreeMapItem* i) const
{
  QString tip, itemTip;

  while (i) {
    if (!i->text(0).isEmpty()) {
      itemTip = i->text(0);
      if (!i->text(1).isEmpty())
        itemTip += " (" + i->text(1) + ")";

      if (!tip.isEmpty())
        tip += "\n";

      tip += itemTip;
    }
    i = i->parent();
  }
  return tip;
}

TreeMapItem* TreeMapWidget::item(int x, int y) const
{
  TreeMapItem* p = _base;
  TreeMapItem* i;

  if (!rect().contains(x, y)) return 0;
  if (DEBUG_DRAWING) kdDebug(90100) << "item(" << x << "," << y << "):" << endl;

  while (1) {
    TreeMapItemList* list = p->children();
    if (!list)
      i = 0;
    else {
      int idx=0;
      for (i=list->first();i;i=list->next(),idx++) {

        if (DEBUG_DRAWING)
	    kdDebug(90100) << "  Checking " << i->path(0).join("/") << " ("
		      << i->itemRect().x() << "/" << i->itemRect().y()
		      << "-" << i->itemRect().width() 
		      << "x" << i->itemRect().height() << ")" << endl;

        if (i->itemRect().contains(x, y)) {

	    if (DEBUG_DRAWING) kdDebug(90100) << "  .. Got. Index " << idx << endl;

	    p->setIndex(idx);
	    break;
        }
      }
    }

    if (!i) {
      static TreeMapItem* last = 0;
      if (p != last) {
        last = p;

        if (DEBUG_DRAWING)
	    kdDebug(90100) << "item(" << x << "," << y << "): Got " 
		      << p->path(0).join("/") << " (Size " 
		      << p->itemRect().width() << "x" << p->itemRect().height()
		      << ", Val " << p->value() << ")" << endl;
      }

      return p;
    }
    p = i;
  }
  return 0;
}

TreeMapItem* TreeMapWidget::possibleSelection(TreeMapItem* i) const
{
  if (i) {
    if (_maxSelectDepth>=0) {
      int depth = i->depth();
      while(i && depth > _maxSelectDepth) {
        i = i->parent();
        depth--;
      }
    }
  }
  return i;
}

TreeMapItem* TreeMapWidget::visibleItem(TreeMapItem* i) const
{
  if (i) {
    /* Must have a visible area */
    while(i && ((i->itemRect().width() <1) ||
                (i->itemRect().height() <1))) {
      TreeMapItem* p = i->parent();
      if (!p) break;
      int idx = p->children()->findRef(i);
      idx--;
      if (idx<0)
        i = p;
      else
        i = p->children()->at(idx);
    }
  }
  return i;
}

void TreeMapWidget::setSelected(TreeMapItem* item, bool selected)
{
    item = possibleSelection(item);
    setCurrent(item);

    TreeMapItem* changed = setTmpSelected(item, selected);
    if (!changed) return;

    _selection = _tmpSelection;
    if (_selectionMode == Single)
      emit selectionChanged(item);
    emit selectionChanged();
    redraw(changed);
    
    if (0) kdDebug(90100) << (selected ? "S":"Des") << "elected Item "
		     << (item ? item->path(0).join("") : QString("(null)"))
		     << " (depth " << (item ? item->depth() : -1) 
		     << ")" << endl;
}

void TreeMapWidget::setMarked(int markNo, bool redrawWidget)
{
    // if there's no marking, return
    if ((_markNo == 0) && (markNo == 0)) return;

    _markNo = markNo;
    if (!clearSelection() && redrawWidget) redraw();
}

/* Returns all items which appear only in one of the given lists */
TreeMapItemList TreeMapWidget::diff(TreeMapItemList& l1,
				    TreeMapItemList& l2)
{
  TreeMapItemList l;
  TreeMapItemListIterator it1(l1), it2(l2);

  TreeMapItem* item;
  while ( (item = it1.current()) != 0 ) {
    ++it1;
    if (l2.containsRef(item) > 0) continue;
    l.append(item);
  }
  while ( (item = it2.current()) != 0 ) {
    ++it2;
    if (l1.containsRef(item) > 0) continue;
    l.append(item);
  }

  return l;
}

/* Only modifies _tmpSelection.
 * Returns 0 when no change happened, otherwise the TreeMapItem that has
 * to be redrawn for all changes.
 */
TreeMapItem* TreeMapWidget::setTmpSelected(TreeMapItem* item, bool selected)
{
  if (!item) return 0;
  if (_selectionMode == NoSelection) return 0;

  TreeMapItemList old = _tmpSelection;

  if (_selectionMode == Single) {
    _tmpSelection.clear();
    if (selected) _tmpSelection.append(item);
  }
  else {
    if (selected) {
      TreeMapItem* i=_tmpSelection.first();
      while (i) {
	if (i->isChildOf(item) || item->isChildOf(i)) {
	  _tmpSelection.remove();
	  i = _tmpSelection.current();
	}
	else
	  i = _tmpSelection.next();
      }
      _tmpSelection.append(item);
    }
    else
      _tmpSelection.removeRef(item);
  }

  return diff(old, _tmpSelection).commonParent();
}


bool TreeMapWidget::clearSelection(TreeMapItem* parent)
{
  TreeMapItemList old = _selection;

  TreeMapItem* i=_selection.first();
  while (i) {
    if (i->isChildOf(parent)) {
      _selection.remove();
      i = _selection.current();
    }
    else
      i = _selection.next();
  }

  TreeMapItem* changed = diff(old, _selection).commonParent();
  if (changed) {
    changed->redraw();
    emit selectionChanged();
  }
  return (changed != 0);
}

bool TreeMapWidget::isSelected(TreeMapItem* i) const
{
  return _selection.containsRef(i)>0;
}

bool TreeMapWidget::isTmpSelected(TreeMapItem* i)
{
  return _tmpSelection.containsRef(i)>0;
}


void TreeMapWidget::setCurrent(TreeMapItem* i, bool kbd)
{
    TreeMapItem* old = _current;
    _current = i;

    if (_markNo >0) {
	// remove mark
	_markNo = 0;

	if (1) kdDebug(90100) << "setCurrent(" << i->path(0).join("/")
			 << ") - mark removed" << endl;

	// always complete redraw needed to remove mark
	redraw();

	if (old == _current) return;
    }
    else {
	if (old == _current) return;

	if (old) old->redraw();
	if (i) i->redraw();
    }

    //kdDebug(90100) << "Current Item " << (i ? i->path().ascii() : "(null)") << endl;

    emit currentChanged(i, kbd);
}

void TreeMapWidget::setRangeSelection(TreeMapItem* i1,
				      TreeMapItem* i2, bool selected)
{
  i1 = possibleSelection(i1);
  i2 = possibleSelection(i2);
  setCurrent(i2);

  TreeMapItem* changed = setTmpRangeSelection(i1, i2, selected);
  if (!changed) return;

  _selection = _tmpSelection;
  if (_selectionMode == Single)
    emit selectionChanged(i2);
  emit selectionChanged();
  redraw(changed);
}

TreeMapItem* TreeMapWidget::setTmpRangeSelection(TreeMapItem* i1,
						 TreeMapItem* i2,
						 bool selected)
{
  if ((i1 == 0) && (i2 == 0)) return 0;
  if ((i1 == 0) || i1->isChildOf(i2)) return setTmpSelected(i2, selected);
  if ((i2 == 0) || i2->isChildOf(i1)) return setTmpSelected(i1, selected);

  TreeMapItem* changed = setTmpSelected(i1, selected);
  TreeMapItem* changed2 = setTmpSelected(i2, selected);
  if (changed2) changed = changed2->commonParent(changed);

  TreeMapItem* commonParent = i1;
  while (commonParent && !i2->isChildOf(commonParent)) {
    i1 = commonParent;
    commonParent = commonParent->parent();
  }
  if (!commonParent) return changed;
  while (i2 && i2->parent() != commonParent)
    i2 = i2->parent();
  if (!i2) return changed;

  TreeMapItemList* list = commonParent->children();
  if (!list) return changed;

  TreeMapItem* i = list->first();
  bool between = false;
  while (i) {
    if (between) {
      if (i==i1 || i==i2) break;
      changed2 = setTmpSelected(i, selected);
      if (changed2) changed = changed2->commonParent(changed);
    }
    else if (i==i1 || i==i2) 
      between = true;
    i = list->next();
  }

  return changed;
}

void TreeMapWidget::contextMenuEvent( QContextMenuEvent* e )
{
  //kdDebug(90100) << "TreeMapWidget::contextMenuEvent" << endl;

  if ( receivers( SIGNAL(contextMenuRequested(TreeMapItem*, const QPoint &)) ) )
    e->accept();

  if ( e->reason() == QContextMenuEvent::Keyboard ) {
    QRect r = (_current) ? _current->itemRect() : _base->itemRect();
    QPoint p = QPoint(r.left() + r.width()/2, r.top() + r.height()/2);
    emit contextMenuRequested(_current, p);
  }
  else {
    TreeMapItem* i = item(e->x(), e->y());
    emit contextMenuRequested(i, e->pos());
  }
}


void TreeMapWidget::mousePressEvent( QMouseEvent* e )
{
  //kdDebug(90100) << "TreeMapWidget::mousePressEvent" << endl;

  _oldCurrent = _current;

  TreeMapItem* i = item(e->x(), e->y());

  _pressed = i;

  _inShiftDrag = e->state() & ShiftButton;
  _inControlDrag = e->state() & ControlButton;
  _lastOver = _pressed;

  TreeMapItem* changed = 0;
  TreeMapItem* item = possibleSelection(_pressed);

  switch(_selectionMode) {
  case Single:
    changed = setTmpSelected(item, true);
    break;
  case Multi:
    changed = setTmpSelected(item, !isTmpSelected(item));
    break;
  case Extended:
    if (_inControlDrag)
      changed = setTmpSelected(item, !isTmpSelected(item));
    else if (_inShiftDrag) {
      TreeMapItem* sCurrent = possibleSelection(_current);
      changed = setTmpRangeSelection(sCurrent, item,
                                     !isTmpSelected(item));
    }
    else {
      _selectionMode = Single;
      changed = setTmpSelected(item, true);
      _selectionMode = Extended;
    }
    break;
  default:
    break;
  }

  // item under mouse always selected on right button press
  if (e->button() == RightButton) {
    TreeMapItem* changed2 = setTmpSelected(item, true);
    if (changed2) changed = changed2->commonParent(changed);
  }

  setCurrent(_pressed);

  if (changed)
    redraw(changed);

  if (e->button() == RightButton) {    

    // emit selection change
    if (! (_tmpSelection == _selection)) {
      _selection = _tmpSelection;
      if (_selectionMode == Single)
	emit selectionChanged(_lastOver);
      emit selectionChanged();
    }
    _pressed = 0;
    _lastOver = 0;
    emit rightButtonPressed(i, e->pos());
  }
}

void TreeMapWidget::mouseMoveEvent( QMouseEvent* e )
{
  //kdDebug(90100) << "TreeMapWidget::mouseMoveEvent" << endl;

  if (!_pressed) return;
  TreeMapItem* over = item(e->x(), e->y());
  if (_lastOver == over) return;

  setCurrent(over);
  if (over == 0) {
    _lastOver = 0;
    return;
  }

  TreeMapItem* changed = 0;
  TreeMapItem* item = possibleSelection(over);

  switch(_selectionMode) {
  case Single:
    changed = setTmpSelected(item, true);
    break;
  case Multi:
    changed = setTmpSelected(item, !isTmpSelected(item));
    break;
  case Extended:
    if (_inControlDrag)
      changed = setTmpSelected(item, !isTmpSelected(item));
    else {
      TreeMapItem* sLast = possibleSelection(_lastOver);
      changed = setTmpRangeSelection(sLast, item, true);
    }
    break;

  default:
    break;
  }

  _lastOver = over;

  if (changed)
    redraw(changed);
}

void TreeMapWidget::mouseReleaseEvent( QMouseEvent* )
{
  //kdDebug(90100) << "TreeMapWidget::mouseReleaseEvent" << endl;

  if (!_pressed) return;

  if (!_lastOver) {
    // take back
    setCurrent(_oldCurrent);
    TreeMapItem* changed = diff(_tmpSelection, _selection).commonParent();
    _tmpSelection = _selection;
    if (changed)
      redraw(changed);
  }
  else {
    if (! (_tmpSelection == _selection)) {
      _selection = _tmpSelection;
      if (_selectionMode == Single)
        emit selectionChanged(_lastOver);
      emit selectionChanged();
    }
    if (!_inControlDrag && !_inShiftDrag && (_pressed == _lastOver))
      emit clicked(_lastOver);
  }

  _pressed = 0;
  _lastOver = 0;
}


void TreeMapWidget::mouseDoubleClickEvent( QMouseEvent* e )
{
  TreeMapItem* over = item(e->x(), e->y());

  emit doubleClicked(over);
}


/* returns -1 if nothing visible found */
int nextVisible(TreeMapItem* i)
{
  TreeMapItem* p = i->parent();
  if (!p || p->itemRect().isEmpty()) return -1;

  int idx = p->children()->findRef(i);
  if (idx<0) return -1;

  while (idx < (int)p->children()->count()-1) {
    idx++;
    QRect r = p->children()->at(idx)->itemRect();
    if (r.width()>1 && r.height()>1)
      return idx;
  }
  return -1;
}

/* returns -1 if nothing visible found */
int prevVisible(TreeMapItem* i)
{
  TreeMapItem* p = i->parent();
  if (!p || p->itemRect().isEmpty()) return -1;

  int idx = p->children()->findRef(i);
  if (idx<0) return -1;

  while (idx > 0) {
    idx--;
    QRect r = p->children()->at(idx)->itemRect();
    if (r.width()>1 && r.height()>1)
      return idx;
  }
  return -1;
}




void TreeMapWidget::keyPressEvent( QKeyEvent* e )
{
  if (e->key() == Key_Escape && _pressed) {

    // take back
    if (_oldCurrent != _lastOver)
      setCurrent(_oldCurrent);
    if (! (_tmpSelection == _selection)) {
      TreeMapItem* changed = diff(_tmpSelection, _selection).commonParent();
      _tmpSelection = _selection;
      if (changed)
	redraw(changed);
    }
    _pressed = 0;
    _lastOver = 0;
  }

  if ((e->key() == Key_Space) ||
      (e->key() == Key_Return)) {

    switch(_selectionMode) {
    case NoSelection:
      break;
    case Single:
      setSelected(_current, true);
      break;
    case Multi:
      setSelected(_current, !isSelected(_current));
      break;
    case Extended:
      if ((e->state() & ControlButton) || (e->state() & ShiftButton))
        setSelected(_current, !isSelected(_current));
      else {
        _selectionMode = Single;
        setSelected(_current, true);
        _selectionMode = Extended;
      }
    }

    if (_current && (e->key() == Key_Return))
      emit returnPressed(_current);

    return;
  }

  if (!_current) {
    if (e->key() == Key_Down) {
      setCurrent(_base, true);
    }
    return;
  }

  TreeMapItem* old = _current,  *newItem;
  TreeMapItem* p = _current->parent();

  bool goBack;
  if (_current->sorting(&goBack) == -1) {
    // noSorting
    goBack = false;
  }


  if ((e->key() == Key_Backspace) ||
      (e->key() == Key_Up)) {
    newItem = visibleItem(p);
    setCurrent(newItem, true);
  }
  else if (e->key() == Key_Left) {
    int newIdx = goBack ? nextVisible(_current) : prevVisible(_current);
    if (p && newIdx>=0) {
      p->setIndex(newIdx);
      setCurrent(p->children()->at(newIdx), true);
    }
  }
  else if (e->key() == Key_Right) {
    int newIdx = goBack ? prevVisible(_current) : nextVisible(_current);
    if (p && newIdx>=0) {
      p->setIndex(newIdx);
      setCurrent(p->children()->at(newIdx), true);
    }
  }
  else if (e->key() == Key_Down) {
    if (_current->children() && _current->children()->count()>0) {
      int newIdx = _current->index();
      if (newIdx<0)
        newIdx = goBack ? (_current->children()->count()-1) : 0;
      if (newIdx>=(int)_current->children()->count())
        newIdx = _current->children()->count()-1;
      newItem = visibleItem(_current->children()->at(newIdx));
      setCurrent(newItem, true);
    }
  }

  if (old == _current) return;
  if (! (e->state() & ControlButton)) return;
  if (! (e->state() & ShiftButton)) return;

  switch(_selectionMode) {
  case NoSelection:
    break;
  case Single:
    setSelected(_current, true);
    break;
  case Multi:
    setSelected(_current, !isSelected(_current));
    break;
  case Extended:
    if (e->state() & ControlButton)
      setSelected(_current, !isSelected(_current));
    else
      setSelected(_current, isSelected(old));
  }
}

void TreeMapWidget::fontChange( const QFont& )
{
  redraw();
}


void TreeMapWidget::resizeEvent( QResizeEvent * )
{
  // this automatically redraws (as size is changed)
  drawTreeMap();
}

void TreeMapWidget::paintEvent( QPaintEvent * )
{
  drawTreeMap();
}

void TreeMapWidget::showEvent( QShowEvent * )
{
  // refresh only if needed
  drawTreeMap();
}

// Updates screen from shadow buffer,
// but redraws before if needed
void TreeMapWidget::drawTreeMap()
{
  // no need to draw if hidden
  if (!isVisible()) return;

  if (_pixmap.size() != size())
    _needsRefresh = _base;

  if (_needsRefresh) {

    if (DEBUG_DRAWING)
      kdDebug(90100) << "Redrawing " << _needsRefresh->path(0).join("/") << endl;
    
    if (_needsRefresh == _base) {
      // redraw whole widget
      _pixmap = QPixmap(size());
      _pixmap.fill(backgroundColor());
    }
    QPainter p(&_pixmap);
    if (_needsRefresh == _base) {
      p.setPen(black);
      p.drawRect(QRect(2, 2, QWidget::width()-4, QWidget::height()-4));
      _base->setItemRect(QRect(3, 3, QWidget::width()-6, QWidget::height()-6));
    }
    else {
      // only subitem
      if (!_needsRefresh->itemRect().isValid()) return;
    }

    // reset cached font object; it could have been changed
    _font = font();
    _fontHeight = fontMetrics().height();

    drawItems(&p, _needsRefresh);
    _needsRefresh = 0;
  }

  bitBlt( this, 0, 0, &_pixmap, 0, 0,
          QWidget::width(), QWidget::height(), CopyROP, true);

  if (hasFocus()) {
    QPainter p(this);
    style().drawPrimitive( QStyle::PE_FocusRect, &p,
                           QRect(0, 0, QWidget::width(), QWidget::height()),
                           colorGroup() );
  }
}



void TreeMapWidget::redraw(TreeMapItem* i)
{
  if (!i) return;

  if (!_needsRefresh)
    _needsRefresh = i;
  else {
    if (!i->isChildOf(_needsRefresh))
      _needsRefresh = _needsRefresh->commonParent(i);
  }

  if (isVisible()) {
    // delayed drawing if we have multiple redraw requests
    update();
  }
}

void TreeMapWidget::drawItem(QPainter* p,
                             TreeMapItem* item)
{
  bool isSelected = false;
  TreeMapItem* i;

  if (_markNo>0) {
      for(i = item;i;i=i->parent())
	  if (i->isMarked(_markNo)) break;

      isSelected = (i!=0);
  }
  else {
      for (i=_tmpSelection.first();i;i=_tmpSelection.next())
	  if (item->isChildOf(i)) break;

      isSelected = (i!=0);
  }

  bool isCurrent = _current && item->isChildOf(_current);
  int dd = item->depth();
  if (isTransparent(dd)) return;

  RectDrawing d(item->itemRect());
  item->setSelected(isSelected);
  item->setCurrent(isCurrent);
  item->setShaded(_shading);
  item->drawFrame(drawFrame(dd));
  d.drawBack(p, item);
}


bool TreeMapWidget::horizontal(TreeMapItem* i, const QRect& r)
{
  switch(i->splitMode()) {
  case TreeMapItem::HAlternate:
    return (i->depth()%2)==1;
  case TreeMapItem::VAlternate:
    return (i->depth()%2)==0;
  case TreeMapItem::Horizontal:
    return true;
  case TreeMapItem::Vertical:
    return false;
  default:
    return r.width() > r.height();
  }
  return false;
}


/**
 * Draw TreeMapItems recursive, starting from item
 */
void TreeMapWidget::drawItems(QPainter* p,
                              TreeMapItem* item)
{
    if (DEBUG_DRAWING)
	kdDebug(90100) << "+drawItems(" << item->path(0).join("/") << ", "
		  << item->itemRect().x() << "/" << item->itemRect().y()
		  << "-" << item->itemRect().width() << "x"
		  << item->itemRect().height() << "), Val " << item->value()
		  << ", Sum " << item->sum() << endl;

  drawItem(p, item);
  item->clearFreeRects();

  QRect origRect = item->itemRect();
  int bw = item->borderWidth();
  QRect r = QRect(origRect.x()+bw, origRect.y()+bw,
                  origRect.width()-2*bw, origRect.height()-2*bw);

  TreeMapItemList* list = item->children();
  TreeMapItem* i;

  bool stopDrawing = false;

  // only subdivide if there are children
  if (!list || list->count()==0)
    stopDrawing = true;

  // only subdivide if there is enough space
  if (!stopDrawing && (r.width()<=0 || r.height()<=0))
    stopDrawing = true;

  // stop drawing if maximum depth is reached
  if (!stopDrawing &&
      (_maxDrawingDepth>=0 && item->depth()>=_maxDrawingDepth))
    stopDrawing = true;

  // stop drawing if stopAtText is reached
  if (!stopDrawing)
    for (int no=0;no<(int)_attr.size();no++) {
      QString stopAt = fieldStop(no);
      if (!stopAt.isEmpty() && (item->text(no) == stopAt)) {
        stopDrawing = true;
        break;
      }
    }

  // area size is checked later...
#if 0
  // stop drawing if minimal area size is reached
  if (!stopDrawing &&
      (_minimalArea > 0) &&
      (r.width() * r.height() < _minimalArea)) stopDrawing = true;
#endif

  if (stopDrawing) {
    if (list) {
      // invalidate rects
      for (i=list->first();i;i=list->next())
        i->clearItemRect();
    }
    // tooltip apears on whole item rect
    item->addFreeRect(item->itemRect());

    // if we have space for text...
    if ((r.height() < _fontHeight) || (r.width() < _fontHeight)) return;

    RectDrawing d(r);
    item->setRotated(_allowRotation && (r.height() > r.width()));
    for (int no=0;no<(int)_attr.size();no++) {
      if (!fieldVisible(no)) continue;
      d.drawField(p, no, item);
    }
    r = d.remainingRect(item);

    if (DEBUG_DRAWING)
	kdDebug(90100) << "-drawItems(" << item->path(0).join("/") << ")" << endl;
    return;
  }

  double user_sum, child_sum, self;

  // user supplied sum
  user_sum = item->sum();

  // own sum
  child_sum = 0;
  for (i=list->first();i;i=list->next()) {
    child_sum += i->value();
    if (DEBUG_DRAWING)
      kdDebug(90100) << "  child: " << i->text(0) << ", value " 
		<< i->value() << endl;
  }

  QRect orig = r;

  // if we have space for text...
  if ((r.height() >= _fontHeight) && (r.width() >= _fontHeight)) {

    RectDrawing d(r);
    item->setRotated(_allowRotation && (r.height() > r.width()));
    for (int no=0;no<(int)_attr.size();no++) {
      if (!fieldVisible(no)) continue;
      if (!fieldForced(no)) continue;
      d.drawField(p, no, item);
    }
    r = d.remainingRect(item);
  }

  if (orig.x() == r.x()) {
    // Strings on top
    item->addFreeRect(QRect(orig.x(), orig.y(),
			    orig.width(), orig.height()-r.height()));
  }
  else {
    // Strings on the left
    item->addFreeRect(QRect(orig.x(), orig.y(),
			    orig.width()-r.width(), orig.height()));
  }

  if (user_sum == 0) {
    // user didn't supply any sum
    user_sum = child_sum;
    self = 0;
  }
  else {
    self = user_sum - child_sum;

    if (user_sum < child_sum) {
      //kdDebug(90100) << "TreeMWidget " <<
      //       item->path() << ": User sum " << user_sum << " < Child Items sum " << child_sum << endl;

      // invalid user supplied sum: ignore and use calculate sum
      user_sum = child_sum;
      self = 0.0;
    }
    else {
      // Try to put the border waste in self
      // percent of wasted space on border...
      float borderArea = origRect.width() * origRect.height();
      borderArea = (borderArea - r.width()*r.height())/borderArea;
      unsigned borderValue = (unsigned)(borderArea * user_sum);

      if (borderValue > self) {
        if (_skipIncorrectBorder) {
          r = origRect;
          // should add my self to nested self and set my self =0
        }
        else
          self = 0.0;
      }
      else
        self -= borderValue;

      user_sum = child_sum + self;
    }
  }

  bool rotate = (_allowRotation && (r.height() > r.width()));
  int self_length = (int)( ((rotate) ? r.width() : r.height()) *
			   self / user_sum + .5);
  if (self_length > 0) {
    // take space for self cost
    QRect sr = r;
    if (rotate) {
      sr.setWidth( self_length );
      r.setRect(r.x()+sr.width(), r.y(), r.width()-sr.width(), r.height());
    }
    else {
      sr.setHeight( self_length );
      r.setRect(r.x(), r.y()+sr.height(), r.width(), r.height()-sr.height());
    }

    // set selfRect (not occupied by children) for tooltip
    item->addFreeRect(sr);

    if (0) kdDebug(90100) << "Item " << item->path(0).join("/") << ": SelfR "
		     << sr.x() << "/" << sr.y() << "-" << sr.width()
		     << "/" << sr.height() << ", self " << self << "/"
		     << user_sum << endl;

    if ((sr.height() >= _fontHeight) && (sr.width() >= _fontHeight)) {

      RectDrawing d(sr);
      item->setRotated(_allowRotation && (r.height() > r.width()));
      for (int no=0;no<(int)_attr.size();no++) {
	if (!fieldVisible(no)) continue;
	if (fieldForced(no)) continue;
	d.drawField(p, no, item);
      }
    }

    user_sum -= self;
  }

  bool goBack;
  if (item->sorting(&goBack) == -1) {
    // noSorting
    goBack = false;
  }

  TreeMapItemListIterator it(*list);
  if (goBack) it.toLast();

  if (item->splitMode() == TreeMapItem::Columns) {
    int len = list->count();
    bool drawDetails = true;

    while (len>0 && user_sum>0) {
      TreeMapItemListIterator first = it;
      double valSum = 0;
      int lenLeft = len;
      int columns = (int)(sqrt((double)len * r.width()/r.height())+.5);
      if (columns==0) columns = 1; //should never be needed

      while (lenLeft>0 && ((double)valSum*(len-lenLeft) <
                           (double)len*user_sum/columns/columns)) {
        valSum += it.current()->value();
        if (goBack) --it; else ++it;
        lenLeft--;
      }

      // we always split horizontally
      int nextPos = (int)((double)r.width() * valSum / user_sum);
      QRect firstRect = QRect(r.x(), r.y(), nextPos, r.height());

      if (nextPos < _visibleWidth) {
	  if (item->sorting(0) == -1) {
	      // fill current rect with hash pattern
	      drawFill(item, p, firstRect);
	  }
	  else {
	      // fill rest with hash pattern
	      drawFill(item, p, r, first, len, goBack);
	      break;
	  }
      }
      else {
        drawDetails = drawItemArray(p, item, firstRect,
                                    valSum, first, len-lenLeft, goBack);
      }
      r.setRect(r.x()+nextPos, r.y(), r.width()-nextPos, r.height());
      user_sum -= valSum;
      len = lenLeft;

      if (!drawDetails) {
        if (item->sorting(0) == -1)
          drawDetails = true;
        else {
          drawFill(item, p, r, it, len, goBack);
          break;
        }
      }
    }
  }
  else if (item->splitMode() == TreeMapItem::Rows) {
    int len = list->count();
    bool drawDetails = true;

    while (len>0 && user_sum>0) {
      TreeMapItemListIterator first = it;
      double valSum = 0;
      int lenLeft = len;
      int rows = (int)(sqrt((double)len * r.height()/r.width())+.5);
      if (rows==0) rows = 1; //should never be needed

      while (lenLeft>0 && ((double)valSum*(len-lenLeft) <
                           (double)len*user_sum/rows/rows)) {
        valSum += it.current()->value();
        if (goBack) --it; else ++it;
        lenLeft--;
      }

      // we always split horizontally
      int nextPos = (int)((double)r.height() * valSum / user_sum);
      QRect firstRect = QRect(r.x(), r.y(), r.width(), nextPos);

      if (nextPos < _visibleWidth) {
        if (item->sorting(0) == -1) {
	    drawFill(item, p, firstRect);
        }
        else {
	    drawFill(item, p, r, first, len, goBack);
	    break;
        }
      }
      else {
        drawDetails = drawItemArray(p, item, firstRect,
                                    valSum, first, len-lenLeft, goBack);
      }
      r.setRect(r.x(), r.y()+nextPos, r.width(), r.height()-nextPos);
      user_sum -= valSum;
      len = lenLeft;

      if (!drawDetails) {
        if (item->sorting(0) == -1)
          drawDetails = true;
        else {
          drawFill(item, p, r, it, len, goBack);
          break;
        }
      }
    }
  }
  else
    drawItemArray(p, item, r, user_sum, it, list->count(), goBack);

  if (DEBUG_DRAWING)
      kdDebug(90100) << "-drawItems(" << item->path(0).join("/") << ")" << endl;
}

// fills area with a pattern if to small to draw children
void TreeMapWidget::drawFill(TreeMapItem* i, QPainter* p, QRect& r)
{
  p->setBrush(Qt::Dense4Pattern);
  p->setPen(Qt::NoPen);
  p->drawRect(r);
  i->addFreeRect(r);
}

// fills area with a pattern if to small to draw children
void TreeMapWidget::drawFill(TreeMapItem* i, QPainter* p, QRect& r,
                             TreeMapItemListIterator it, int len, bool goBack)
{
  if (DEBUG_DRAWING)
      kdDebug(90100) << "  +drawFill(" << r.x() << "/" << r.y()
		<< "-" << r.width() << "x" << r.height()
		<< ", len " << len << ")" << endl;

  p->setBrush(Qt::Dense4Pattern);
  p->setPen(Qt::NoPen);
  p->drawRect(r);
  i->addFreeRect(r);

  // reset rects
  while (len>0 && it.current()) {

      if (DEBUG_DRAWING)
	  kdDebug(90100) << "   Reset Rect " << (*it)->path(0).join("/") << endl;

      (*it)->clearItemRect();
      if (goBack) --it; else ++it;
      len--;
  }
  if (DEBUG_DRAWING)
      kdDebug(90100) << "  -drawFill(" << r.x() << "/" << r.y()
		<< "-" << r.width() << "x" << r.height()
		<< ", len " << len << ")" << endl;
}

// returns false if rect gets to small
bool TreeMapWidget::drawItemArray(QPainter* p, TreeMapItem* item,
                                  QRect& r, double user_sum,
                                  TreeMapItemListIterator it, int len,
				  bool goBack)
{
  if (user_sum == 0) return false;

  static bool b2t = true;

  // stop recursive bisection for small rectangles
  if (((r.height() < _visibleWidth) &&
       (r.width() < _visibleWidth)) ||
      ((_minimalArea > 0) &&
       (r.width() * r.height() < _minimalArea))) {

    drawFill(item, p, r, it, len, goBack);
    return false;
  }

  if (DEBUG_DRAWING)
      kdDebug(90100) << " +drawItemArray(" << item->path(0).join("/") 
		<< ", " << r.x() << "/" << r.y() << "-" << r.width()
		<< "x" << r.height() << ")" << endl;

  if (len>2 && (item->splitMode() == TreeMapItem::Bisection)) {

    TreeMapItemListIterator first = it;
    double valSum = 0;
    int lenLeft = len;
    //while (lenLeft>0 && valSum<user_sum/2) {
    while (lenLeft>len/2) {
      valSum += it.current()->value();
      if (goBack) --it; else ++it;
      lenLeft--;
    }

    // draw first half...
    bool drawOn;

    if (r.width() > r.height()) {
      int halfPos = (int)((double)r.width() * valSum / user_sum);
      QRect firstRect = QRect(r.x(), r.y(), halfPos, r.height());
      drawOn = drawItemArray(p, item, firstRect,
                             valSum, first, len-lenLeft, goBack);
      r.setRect(r.x()+halfPos, r.y(), r.width()-halfPos, r.height());
    }
    else {
      int halfPos = (int)((double)r.height() * valSum / user_sum);
      QRect firstRect = QRect(r.x(), r.y(), r.width(), halfPos);
      drawOn = drawItemArray(p, item, firstRect,
                             valSum, first, len-lenLeft, goBack);
      r.setRect(r.x(), r.y()+halfPos, r.width(), r.height()-halfPos);
    }

    // if no sorting, don't stop drawing
    if (item->sorting(0) == -1) drawOn = true;

    // second half
    if (drawOn)
      drawOn = drawItemArray(p, item, r, user_sum - valSum,
                             it, lenLeft, goBack);
    else {
      drawFill(item, p, r, it, len, goBack);
    }

    if (DEBUG_DRAWING)
	kdDebug(90100) << " -drawItemArray(" << item->path(0).join("/")
		  << ")" << endl;

    return drawOn;
  }

  bool hor = horizontal(item,r);

  TreeMapItem* i;
  while (len>0) {
    i = it.current();
    if (user_sum <= 0) {

      if (DEBUG_DRAWING)
	  kdDebug(90100) << "drawItemArray: Reset " << i->path(0).join("/") << endl;

      i->clearItemRect();
      if (goBack) --it; else ++it;
      len--;
      continue;
    }

    // stop drawing for small rectangles
    if (((r.height() < _visibleWidth) &&
         (r.width() < _visibleWidth)) ||
        ((_minimalArea > 0) &&
         (r.width() * r.height() < _minimalArea))) {

      drawFill(item, p, r, it, len, goBack);
      if (DEBUG_DRAWING)
	  kdDebug(90100) << " -drawItemArray(" << item->path(0).join("/")
		    << "): Stop" << endl;
      return false;
    }

    if (i->splitMode() == TreeMapItem::AlwaysBest)
      hor = r.width() > r.height();

    int lastPos = hor ? r.width() : r.height();
    double val = i->value();
    int nextPos = (user_sum <= 0.0) ? 0: (int)(lastPos * val / user_sum +.5);
    if (nextPos>lastPos) nextPos = lastPos;

    if ((item->sorting(0) != -1) && (nextPos < _visibleWidth)) {
      drawFill(item, p, r, it, len, goBack);
      if (DEBUG_DRAWING)
	  kdDebug(90100) << " -drawItemArray(" << item->path(0).join("/")
		    << "): Stop" << endl;
      return false;
    }

    QRect currRect = r;

    if (hor)
      currRect.setWidth(nextPos);
    else {
      if (b2t)
        currRect.setRect(r.x(), r.bottom()-nextPos+1, r.width(), nextPos);
      else
        currRect.setHeight(nextPos);
    }

    // don't draw very small rectangles:
    if (nextPos >= _visibleWidth) {
	i->setItemRect(currRect);
	drawItems(p, i);
    }
    else {
	i->clearItemRect();
	drawFill(item, p, currRect);
    }

    // draw Separator
    if (_drawSeparators && (nextPos<lastPos)) {
      p->setPen(black);
      if (hor) {
        if (r.top()<=r.bottom())
          p->drawLine(r.x() + nextPos, r.top(), r.x() + nextPos, r.bottom());
      }
      else {
        if (r.left()<=r.right())
        p->drawLine(r.left(), r.y() + nextPos, r.right(), r.y() + nextPos);
      }
      nextPos++;
    }

    if (hor)
      r.setRect(r.x() + nextPos, r.y(), lastPos-nextPos, r.height());
    else {
      if (b2t)
        r.setRect(r.x(), r.y(), r.width(), lastPos-nextPos);
      else
        r.setRect(r.x(), r.y() + nextPos, r.width(), lastPos-nextPos);
    }

    user_sum -= val;
    if (goBack) --it; else ++it;
    len--;
  }

  if (DEBUG_DRAWING)
      kdDebug(90100) << " -drawItemArray(" << item->path(0).join("/")
		<< "): Continue" << endl;

  return true;
}


/*----------------------------------------------------------------
 * Popup menus for option setting
 */

void TreeMapWidget::splitActivated(int id)
{
  if      (id == _splitID)   setSplitMode(TreeMapItem::Bisection);
  else if (id == _splitID+1) setSplitMode(TreeMapItem::Columns);
  else if (id == _splitID+2) setSplitMode(TreeMapItem::Rows);
  else if (id == _splitID+3) setSplitMode(TreeMapItem::AlwaysBest);
  else if (id == _splitID+4) setSplitMode(TreeMapItem::Best);
  else if (id == _splitID+5) setSplitMode(TreeMapItem::VAlternate);
  else if (id == _splitID+6) setSplitMode(TreeMapItem::HAlternate);
  else if (id == _splitID+7) setSplitMode(TreeMapItem::Horizontal);
  else if (id == _splitID+8) setSplitMode(TreeMapItem::Vertical);
}


void TreeMapWidget::addSplitDirectionItems(QPopupMenu* popup, int id)
{
  _splitID = id;
  popup->setCheckable(true);

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(splitActivated(int)));

  popup->insertItem(i18n("Recursive Bisection"), id);
  popup->insertItem(i18n("Columns"),         id+1);
  popup->insertItem(i18n("Rows"),            id+2);
  popup->insertItem(i18n("Always Best"),     id+3);
  popup->insertItem(i18n("Best"),            id+4);
  popup->insertItem(i18n("Alternate (V)"),   id+5);
  popup->insertItem(i18n("Alternate (H)"),   id+6);
  popup->insertItem(i18n("Horizontal"),      id+7);
  popup->insertItem(i18n("Vertical"),        id+8);

  switch(splitMode()) {
    case TreeMapItem::Bisection:  popup->setItemChecked(id,true); break;
    case TreeMapItem::Columns:    popup->setItemChecked(id+1,true); break;
    case TreeMapItem::Rows:       popup->setItemChecked(id+2,true); break;
    case TreeMapItem::AlwaysBest: popup->setItemChecked(id+3,true); break;
    case TreeMapItem::Best:       popup->setItemChecked(id+4,true); break;
    case TreeMapItem::VAlternate: popup->setItemChecked(id+5,true); break;
    case TreeMapItem::HAlternate: popup->setItemChecked(id+6,true); break;
    case TreeMapItem::Horizontal: popup->setItemChecked(id+7,true); break;
    case TreeMapItem::Vertical:   popup->setItemChecked(id+8,true); break;
    default: break;
  }
}

void TreeMapWidget::visualizationActivated(int id)
{
  if (id == _visID+2) setSkipIncorrectBorder(!skipIncorrectBorder());
  else if (id == _visID+3) setBorderWidth(0);
  else if (id == _visID+4) setBorderWidth(1);
  else if (id == _visID+5) setBorderWidth(2);
  else if (id == _visID+6) setBorderWidth(3);
  else if (id == _visID+10) setAllowRotation(!allowRotation());
  else if (id == _visID+11) setShadingEnabled(!isShadingEnabled());
  else if (id<_visID+19 || id>_visID+100) return;

  id -= 20+_visID;
  int f = id/10;
  if      ((id%10) == 1) setFieldVisible(f, !fieldVisible(f));
  else if ((id%10) == 2) setFieldForced(f, !fieldForced(f));
  else if ((id%10) == 3) setFieldPosition(f, DrawParams::TopLeft);
  else if ((id%10) == 4) setFieldPosition(f, DrawParams::TopCenter);
  else if ((id%10) == 5) setFieldPosition(f, DrawParams::TopRight);
  else if ((id%10) == 6) setFieldPosition(f, DrawParams::BottomLeft);
  else if ((id%10) == 7) setFieldPosition(f, DrawParams::BottomCenter);
  else if ((id%10) == 8) setFieldPosition(f, DrawParams::BottomRight);
}

void TreeMapWidget::addVisualizationItems(QPopupMenu* popup, int id)
{
  _visID = id;

  popup->setCheckable(true);

  QPopupMenu* bpopup = new QPopupMenu();
  bpopup->setCheckable(true);

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(visualizationActivated(int)));
  connect(bpopup, SIGNAL(activated(int)),
          this, SLOT(visualizationActivated(int)));

  QPopupMenu* spopup = new QPopupMenu();
  addSplitDirectionItems(spopup, id+100);
  popup->insertItem(i18n("Nesting"), spopup, id);

  popup->insertItem(i18n("Border"), bpopup, id+1);
  bpopup->insertItem(i18n("Correct Borders Only"), id+2);
  bpopup->insertSeparator();
  bpopup->insertItem(i18n("Width %1").arg(0), id+3);
  bpopup->insertItem(i18n("Width %1").arg(1), id+4);
  bpopup->insertItem(i18n("Width %1").arg(2), id+5);
  bpopup->insertItem(i18n("Width %1").arg(3), id+6);
  bpopup->setItemChecked(id+2, skipIncorrectBorder());
  bpopup->setItemChecked(id+3, borderWidth()==0);
  bpopup->setItemChecked(id+4, borderWidth()==1);
  bpopup->setItemChecked(id+5, borderWidth()==2);
  bpopup->setItemChecked(id+6, borderWidth()==3);

  popup->insertItem(i18n("Allow Rotation"), id+10);
  popup->setItemChecked(id+10,allowRotation());
  popup->insertItem(i18n("Shading"), id+11);
  popup->setItemChecked(id+11,isShadingEnabled());

  if (_attr.size() ==0) return;

  popup->insertSeparator();
  int f;
  QPopupMenu* tpopup;
  id += 20;
  for (f=0;f<(int)_attr.size();f++, id+=10) {
    tpopup = new QPopupMenu();
    tpopup->setCheckable(true);
    popup->insertItem(_attr[f].type, tpopup, id);
    tpopup->insertItem(i18n("Visible"), id+1);
    tpopup->insertItem(i18n("Take Space From Children"), id+2);
    tpopup->insertSeparator();
    tpopup->insertItem(i18n("Top Left"), id+3);
    tpopup->insertItem(i18n("Top Center"), id+4);
    tpopup->insertItem(i18n("Top Right"), id+5);
    tpopup->insertItem(i18n("Bottom Left"), id+6);
    tpopup->insertItem(i18n("Bottom Center"), id+7);
    tpopup->insertItem(i18n("Bottom Right"), id+8);

    tpopup->setItemChecked(id+1,_attr[f].visible);
    tpopup->setItemEnabled(id+2,_attr[f].visible);
    tpopup->setItemEnabled(id+3,_attr[f].visible);
    tpopup->setItemEnabled(id+4,_attr[f].visible);
    tpopup->setItemEnabled(id+5,_attr[f].visible);
    tpopup->setItemEnabled(id+6,_attr[f].visible);
    tpopup->setItemEnabled(id+7,_attr[f].visible);
    tpopup->setItemEnabled(id+8,_attr[f].visible);
    tpopup->setItemChecked(id+2,_attr[f].forced);
    tpopup->setItemChecked(id+3,_attr[f].pos == DrawParams::TopLeft);
    tpopup->setItemChecked(id+4,_attr[f].pos == DrawParams::TopCenter);
    tpopup->setItemChecked(id+5,_attr[f].pos == DrawParams::TopRight);
    tpopup->setItemChecked(id+6,_attr[f].pos == DrawParams::BottomLeft);
    tpopup->setItemChecked(id+7,_attr[f].pos == DrawParams::BottomCenter);
    tpopup->setItemChecked(id+8,_attr[f].pos == DrawParams::BottomRight);

    connect(tpopup, SIGNAL(activated(int)),
            this, SLOT(visualizationActivated(int)));
  }
}

void TreeMapWidget::selectionActivated(int id)
{
  TreeMapItem* i = _menuItem;
  id -= _selectionID;
  while (id>0 && i) {
    i=i->parent();
    id--;
  }
  if (i)
    setSelected(i, true);
}

void TreeMapWidget::addSelectionItems(QPopupMenu* popup,
				      int id, TreeMapItem* i)
{
  if (!i) return;

  _selectionID = id;
  _menuItem = i;

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(selectionActivated(int)));

  while (i) {
    QString name = i->text(0);
    if (name.isEmpty()) break;
    popup->insertItem(i->text(0), id++);
    i = i->parent();
  }
}

void TreeMapWidget::fieldStopActivated(int id)
{
  if (id == _fieldStopID) setFieldStop(0, QString::null);
  else {
    TreeMapItem* i = _menuItem;
    id -= _fieldStopID+1;
    while (id>0 && i) {
      i=i->parent();
      id--;
    }
    if (i)
      setFieldStop(0, i->text(0));
  }
}

void TreeMapWidget::addFieldStopItems(QPopupMenu* popup,
				      int id, TreeMapItem* i)
{
  _fieldStopID = id;

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(fieldStopActivated(int)));

  popup->insertItem(i18n("No %1 Limit").arg(fieldType(0)), id);
  popup->setItemChecked(id, fieldStop(0).isEmpty());
  _menuItem = i;
  bool foundFieldStop = false;
  if (i) {
    popup->insertSeparator();

    while (i) {
      id++;
      QString name = i->text(0);
      if (name.isEmpty()) break;
      popup->insertItem(i->text(0), id);
      if (fieldStop(0) == i->text(0)) {
        popup->setItemChecked(id, true);
        foundFieldStop = true;
      }
      i = i->parent();
    }
  }

  if (!foundFieldStop && !fieldStop(0).isEmpty()) {
    popup->insertSeparator();
    popup->insertItem(fieldStop(0), id+1);
    popup->setItemChecked(id+1, true);
  }
}

void TreeMapWidget::areaStopActivated(int id)
{
  if (id == _areaStopID) setMinimalArea(-1);
  else if (id == _areaStopID+1) {
    int area = _menuItem ? (_menuItem->width() * _menuItem->height()) : -1;
    setMinimalArea(area);
  }
  else if (id == _areaStopID+2) setMinimalArea(100);
  else if (id == _areaStopID+3) setMinimalArea(400);
  else if (id == _areaStopID+4) setMinimalArea(1000);
  else if (id == _areaStopID+5) setMinimalArea(minimalArea()*2);
  else if (id == _areaStopID+6) setMinimalArea(minimalArea()/2);
}

void TreeMapWidget::addAreaStopItems(QPopupMenu* popup,
				     int id, TreeMapItem* i)
{
  _areaStopID = id;
  _menuItem = i;

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(areaStopActivated(int)));

  bool foundArea = false;

  popup->insertItem(i18n("No Area Limit"), id);
  popup->setItemChecked(id, minimalArea() == -1);

  if (i) {
    int area = i->width() * i->height();
    popup->insertSeparator();
    popup->insertItem(i18n("Area of '%1' (%2)")
                       .arg(i->text(0)).arg(area), id+1);
    if (area == minimalArea()) {
      popup->setItemChecked(id+1, true);
      foundArea = true;
    }
  }

  popup->insertSeparator();
  int area = 100, count;
  for (count=0;count<3;count++) {
    popup->insertItem(i18n("1 Pixel", "%n Pixels", area), id+2+count);
    if (area == minimalArea()) {
      popup->setItemChecked(id+2+count, true);
      foundArea = true;
    }
    area = (area==100) ? 400 : (area==400) ? 1000 : 4000;
  }

  if (minimalArea()>0) {
    popup->insertSeparator();
    if (!foundArea) {
      popup->insertItem(i18n("1 Pixel", "%n Pixels", minimalArea()), id+10);
      popup->setItemChecked(id+10, true);
    }

    popup->insertItem(i18n("Double Area Limit (to %1)")
                       .arg(minimalArea()*2), id+5);
    popup->insertItem(i18n("Halve Area Limit (to %1)")
                       .arg(minimalArea()/2), id+6);
  }
}


void TreeMapWidget::depthStopActivated(int id)
{
  if (id == _depthStopID) setMaxDrawingDepth(-1);
  else if (id == _depthStopID+1) {
    int d = _menuItem ? _menuItem->depth() : -1;
    setMaxDrawingDepth(d);
  }
  else if (id == _depthStopID+2) setMaxDrawingDepth(maxDrawingDepth()-1);
  else if (id == _depthStopID+3) setMaxDrawingDepth(maxDrawingDepth()+1);
}

void TreeMapWidget::addDepthStopItems(QPopupMenu* popup,
				      int id, TreeMapItem* i)
{
  _depthStopID = id;
  _menuItem = i;

  connect(popup, SIGNAL(activated(int)),
          this, SLOT(depthStopActivated(int)));

  bool foundDepth = false;

  popup->insertItem(i18n("No Depth Limit"), id);
  popup->setItemChecked(id, maxDrawingDepth() == -1);

  if (i) {
    int d = i->depth();
    popup->insertSeparator();
    popup->insertItem(i18n("Depth of '%1' (%2)")
                       .arg(i->text(0)).arg(d), id+1);
    if (d == maxDrawingDepth()) {
      popup->setItemChecked(id+1, true);
      foundDepth = true;
    }
  }

  if (maxDrawingDepth()>1) {
    popup->insertSeparator();
    if (!foundDepth) {
      popup->insertItem(i18n("Depth %1").arg(maxDrawingDepth()), id+10);
      popup->setItemChecked(id+10, true);
    }

    popup->insertItem(i18n("Decrement (to %1)")
                       .arg(maxDrawingDepth()-1), id+2);
    popup->insertItem(i18n("Increment (to %1)")
                       .arg(maxDrawingDepth()+1), id+3);
  }
}



/*----------------------------------------------------------------
 * Option saving/restoring
 */

void TreeMapWidget::saveOptions(KConfigGroup* config, QString prefix)
{
  config->writeEntry(prefix+"Nesting", splitModeString());
  config->writeEntry(prefix+"AllowRotation", allowRotation());
  config->writeEntry(prefix+"ShadingEnabled", isShadingEnabled());
  config->writeEntry(prefix+"OnlyCorrectBorder", skipIncorrectBorder());
  config->writeEntry(prefix+"BorderWidth", borderWidth());
  config->writeEntry(prefix+"MaxDepth", maxDrawingDepth());
  config->writeEntry(prefix+"MinimalArea", minimalArea());

  int f, fCount = _attr.size();
  config->writeEntry(prefix+"FieldCount", fCount);
  for (f=0;f<fCount;f++) {
    config->writeEntry(QString(prefix+"FieldVisible%1").arg(f),
                       _attr[f].visible);
    config->writeEntry(QString(prefix+"FieldForced%1").arg(f),
                       _attr[f].forced);
    config->writeEntry(QString(prefix+"FieldStop%1").arg(f),
                       _attr[f].stop);
    config->writeEntry(QString(prefix+"FieldPosition%1").arg(f),
                       fieldPositionString(f));
  }
}


void TreeMapWidget::restoreOptions(KConfigGroup* config, QString prefix)
{
  bool enabled;
  int num;
  QString str;

  str = config->readEntry(prefix+"Nesting");
  if (!str.isEmpty()) setSplitMode(str);

  if (config->hasKey(prefix+"AllowRotation")) {
    enabled = config->readBoolEntry(prefix+"AllowRotation", true);
    setAllowRotation(enabled);
  }

  if (config->hasKey(prefix+"ShadingEnabled")) {
    enabled = config->readBoolEntry(prefix+"ShadingEnabled", true);
    setShadingEnabled(enabled);
  }

  if (config->hasKey(prefix+"OnlyCorrectBorder")) {
    enabled = config->readBoolEntry(prefix+"OnlyCorrectBorder", false);
    setSkipIncorrectBorder(enabled);
  }

  num = config->readNumEntry(prefix+"BorderWidth", -2);
  if (num!=-2) setBorderWidth(num);

  num = config->readNumEntry(prefix+"MaxDepth", -2);
  if (num!=-2) setMaxDrawingDepth(num);

  num = config->readNumEntry(prefix+"MinimalArea", -2);
  if (num!=-2) setMinimalArea(num);

  num = config->readNumEntry(prefix+"FieldCount", -2);
  if (num<=0 || num>MAX_FIELD) return;

  int f;
  for (f=0;f<num;f++) {
    str = QString(prefix+"FieldVisible%1").arg(f);
    if (config->hasKey(str))
      setFieldVisible(f, config->readBoolEntry(str));

    str = QString(prefix+"FieldForced%1").arg(f);
    if (config->hasKey(str))
      setFieldForced(f, config->readBoolEntry(str));

    str = config->readEntry(QString(prefix+"FieldStop%1").arg(f));
    setFieldStop(f, str);

    str = config->readEntry(QString(prefix+"FieldPosition%1").arg(f));
    if (!str.isEmpty()) setFieldPosition(f, str);
  }
}

#include "treemap.moc"
