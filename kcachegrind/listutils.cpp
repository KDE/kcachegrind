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
 * Some helper functions for QListViewItem derivates
 */

#include <qpainter.h>
#include "listutils.h"

QPixmap colorPixmap(int w, int h, QColor c)
{
  static QPixmap* pixs[37];
  static QColor cols[37];
  static bool inited = false;

  if (!inited) {
    for (int i=0;i<37;i++) pixs[i]=0;
    inited = true;
  }
  int hash = (w+h+c.red()+c.green()+c.blue()) % 37;
  if (pixs[hash]) {
    if ((pixs[hash]->width() == w) &&
        (pixs[hash]->height() == h) &&
        (cols[hash] == c))
      return *pixs[hash];

    delete pixs[hash];
  }


  QPixmap* pix = new QPixmap(w, h);
  pix->fill(c);
  QPainter p(pix);
  p.setPen(c.light());
  p.drawLine(0, 0, w-1, 0);
  p.drawLine(0, 0, 0, h-1);
  p.setPen(c.dark());
  p.drawLine(w-1, 0, w-1, h-1);
  p.drawLine(0, h-1, w-1, h-1);

  pixs[hash] = pix;
  cols[hash] = c;
  return *pix;
}

// create a percentage pixmap
// with a filling rate of p percent (0-100)
QPixmap percentagePixmap(int w, int h, int percent, QColor c)
{
#if 0
  static QPixmap* pixs[101];
  static bool inited = false;

  if (!inited) {
    for (int i=0;i<101;i++) pixs[i]=0;
    inited = true;
  }
  if (percent<0 || percent>100) return QPixmap();

  if (pixs[percent]) {
    if ((pixs[percent]->width() == w) &&
        (pixs[percent]->height() == h))
      return *pixs[percent];

    delete pixs[percent];
  }

  QPixmap* pix = new QPixmap(w, h);
  int x1 = (2*(w-2) * percent + 50) / 100;
  int x2 = 0;
  if (x1>(w-2)) { x2 = x1-(w-2); x1 = w-2; }
  int y = h/2;

  pix->fill(Qt::white);
  QPainter p(pix);
  p.setPen(Qt::black);
  p.drawRect(0, 0, w, h);
  if (x1>0) {

    // frame
    p.setPen(c.light());
    p.drawLine(1, 1, x1, 1);
    p.drawLine(1, 1, 1, (x2>0) ? h-2 : y);
    p.setPen(c.dark());
    p.drawLine(x1, 1, x1, y);
    if (x2<x1)
      p.drawLine(x2+1, y, x1, y);
    if (x2>0) {
      p.drawLine(x2, y, x2, h-2);
      p.drawLine(1, h-2, x2, h-2);
    }

    // inside
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    if (x1>2) p.drawRect(2, 2, x1-2, y-2);
    if (x2>2) p.drawRect(2, 2, x2-2, h-4);
  }
  pixs[percent] = pix;
  return *pix;
#else
  QPixmap pix(w, h);
  int x1 = (2*(w-2) * percent + 50) / 100;
  int x2 = 0;
  if (x1>(w-2)) { x2 = x1-(w-2); x1 = w-2; }
  int y = h/2;

  pix.fill(Qt::white);
  QPainter p(&pix);
  p.setPen(Qt::black);
  p.drawRect(0, 0, w, h);
  if (x1>0) {

    // frame
    p.setPen(c.light());
    p.drawLine(1, 1, x1, 1);
    p.drawLine(1, 1, 1, (x2>0) ? h-2 : y);
    p.setPen(c.dark());
    p.drawLine(x1, 1, x1, y);
    if (x2<x1)
      p.drawLine(x2+1, y, x1, y);
    if (x2>0) {
      p.drawLine(x2, y, x2, h-2);
      p.drawLine(1, h-2, x2, h-2);
    }

    // inside
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    if (x1>2) p.drawRect(2, 2, x1-2, y-2);
    if (x2>2) p.drawRect(2, 2, x2-2, h-4);
  }
  return pix;
#endif
}

inline QColor partitionColor(int d, int max)
{
  return QColor( (720*d/max) % 360,
                 255-(128*d/max), 192, QColor::Hsv);
}


QPixmap partitionPixmap(int w, int h,
                        double* hist, QColor* cArray, int maxIndex)
{
  QPixmap pix(w, h);

  pix.fill(Qt::white);
  QPainter p(&pix);
  p.setPen(Qt::black);
  p.drawRect(0, 0, w, h);

  QColor c, cLast;
  int lastPos = 0, nextPos;
  double val=0.0, sum=0.0;
  int d, dmin=maxIndex, dmax=0;
  for (d = 0;d<maxIndex;d++)
    if (hist[d]>0.0) {
      sum += hist[d];
      if (dmin>d) dmin = d;
      if (dmax<d) dmax = d;
    }

  int dw; // double width
  dw = (int)(2*(w-2)*sum+.5); // now recalculate width

  //qDebug("Sum %f, dw %d", sum,dw);

  bool leftDrawn = false;
  bool upperDrawn = false;
  int x1, x2=0, y1=0, y2=0;
  int lastDiff=0, diff;
  d=dmin;
  while (d<dmax+1) {
    val += hist[d];
    nextPos = (int)(dw * val/sum);

    //qDebug(" hist[%d] %f, val %f, nextPos %d", d, hist[d], val, nextPos);

    diff = nextPos-lastPos;
    // skip drawing of a distance rect if it's small and not the last
    if (diff <2 && d<dmax+1) {
      d++;
      continue;
    }

    c = cArray ? cArray[d] : partitionColor(d,maxIndex);

    if (upperDrawn) {
      y1 = h/2;
      y2 = h - 2;
      x1 = lastPos-(w-2)+1;
      x2 = nextPos-(w-2)+1;
      if (x1<1) x1 = 1;
    }
    else {
      y1 = 1;
      y2 = h/2-1;
      x1 = lastPos+1;
      x2 = nextPos+1;
    }
    if (x2>w-2) x2=w-2;

    // inside
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRect(x1, y1, x2-x1+1, y2-y1+1);

    // lighter top border
    p.setPen(c.light());
    p.drawLine(x1, y1, x2-1, y1);

    // when width for last and current distance >2, draw full 3D effect...
    if (!leftDrawn || (diff>2 && lastDiff >2)) {
      p.drawLine(x1, y1+1, x1, y2);
      leftDrawn = true;

      if (x1>1) {
        // right border of last rect (in last color)
        p.setPen(cLast.dark());
        p.drawLine(x1-1, y1+1, x1-1, y2);
      }
    }

    // darker bottom border
    p.setPen(c.dark());
    p.drawLine(x1, y2, x2-1, y2);

    if ((x2==w-2) && !upperDrawn) {
      // right dark border
      p.drawLine(x2, y1, x2, y2);

      upperDrawn = true;
      leftDrawn = false;
      // we are wrapping from upper to lower part...
      // draw same distance again; and don't add hist[d] twice
      val -= hist[d];
      continue;
    }


    lastPos = nextPos;
    lastDiff = diff;
    cLast = c;
    d++;
  }

  // right border (in last color)
  if (x2>0)
    p.drawLine(x2, y1, x2, y2);

  return pix;
}


QPixmap costPixmap(TraceCostType* ct, TraceCost* cost, double total)
{
    if (ct->isReal()) {
	QColor color = ct->color();
	double p = 100.0 * cost->subCost(ct) / total;
	return percentagePixmap(25, 10, (int)(p+.5), color);
    }

    int maxIndex;
    double h[TraceCost::MaxRealIndex];
    QColor* cs = ct->mapping()->realColors();
    maxIndex = ct->histCost(cost, total, h);

    if (maxIndex ==0) return QPixmap();    
    return partitionPixmap(25, 10, h, cs, maxIndex);
}



// HighestCostList

HighestCostList::HighestCostList()
{
    _maxSize = 0;
    _count = 0;
    _costType = 0;
}

void HighestCostList::clear(int maxSize)
{
    _maxSize = maxSize;
    _count = 0;
    _item.resize(maxSize);
    _cost.resize(maxSize);
}

void HighestCostList::addCost(TraceCost* c, SubCost cost)
{
    int i;

    _count++;
    if (_count > _maxSize) {
	if (_cost[_maxSize-1] >= cost) return;
	i = _maxSize-1;
    }
    else i = _count-1;

    for(; i>0; i--) {
	if (_cost[i-1] >= cost) break;
	else {
	    _cost[i] = _cost[i-1];
	    _item[i] = _item[i-1];
	}
    }
    _cost[i] = cost;
    _item[i] = c;
}

	    
