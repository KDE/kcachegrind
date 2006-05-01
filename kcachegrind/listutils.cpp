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

#define COSTPIX_WIDTH 25

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

/**
 * Create a percentage pixmap with a filling rate of p percent (0-100).
 * When withFrame==false, the pixmap is truncated to only the filled portion.
 */
QPixmap percentagePixmap(int w, int h, int percent, QColor c, bool framed)
{
  int iw, ix1, ix2, ih, iy1, iy2;

  // inner rectangle to fill with bar
  if (framed) {
    iw = w-2, ix1 = 1, ix2 = w-2;
    ih = h-2, iy1 = 1, iy2 = h-2;
  }
  else {
    iw = w; ix1 = 0; ix2 = w-1;
    ih = h; iy1 = 0; iy2 = h-1;
  }

  /* Limit bar to 100%  */
  int filled = (percent>100) ? iw+1 : iw*percent/100+1;
  if (!framed) w=filled-1;
  if (w<3) return QPixmap();

  QPixmap pix(w, h);
  pix.fill(Qt::white);
  QPainter p(&pix);
  p.setPen(Qt::black);
  if (framed)
    p.drawRect(0, 0, w, h);

  // inside
  p.setPen(Qt::NoPen);
  p.setBrush(c);
  p.drawRect(ix1, iy1, filled-1,ih);

  // frame
  ix2 = ix1+filled-2;
  p.setPen(c.light());
  p.drawLine(ix1, iy1, ix2, iy1);
  p.drawLine(ix1, iy1, ix1, iy2);
  p.setPen(c.dark());
  p.drawLine(ix1+1, iy2, ix2, iy2);
  p.drawLine(ix2, iy1, ix2, iy2);

  return pix;
}

inline QColor partitionColor(int d, int max)
{
  return QColor( (720*d/max) % 360,
                 255-(128*d/max), 192, QColor::Hsv);
}


QPixmap partitionPixmap(int w, int h,
                        double* hist, QColor* cArray, int maxIndex, bool framed)
{
  int lastPos = 0, nextPos;
  double val=0.0, sum=0.0;
  int d, dmin=maxIndex, dmax=0;
  for (d = 0;d<maxIndex;d++)
    if (hist[d]>0.0) {
      sum += hist[d];
      if (dmin>d) dmin = d;
      if (dmax<d) dmax = d;
    }

  // inner rectangle to fill with bar
  int iw, ix1, ix2, ih, iy1, iy2;
  if (framed) {
    iw = w-2, ix1 = 1, ix2 = w-2;
    ih = h-2, iy1 = 1, iy2 = h-2;
  }
  else {
    iw = w; ix1 = 0; ix2 = w-1;
    ih = h; iy1 = 0; iy2 = h-1;
  }

  int filled = (int)(iw*sum+1);
  if (!framed && (filled < w)) w=filled;
  if (w<3) return QPixmap();

  QPixmap pix(w, h);
  pix.fill(Qt::white);
  QPainter p(&pix);
  p.setPen(Qt::black);
  if (framed)
    p.drawRect(0, 0, w, h);

  //qDebug("Sum %f, dw %d", sum,dw);

  QColor c, cLast;
  bool leftDrawn = false;
  int x1, x2=0;
  int lastDiff=0, diff;
  d=dmin;
  while (d<dmax+1) {
    val += hist[d];
    nextPos = (int)(filled * val/sum);

    //qDebug(" hist[%d] %f, val %f, nextPos %d", d, hist[d], val, nextPos);

    diff = nextPos-lastPos;
    if (diff==0) { d++; continue; }

    c = cArray ? cArray[d] : partitionColor(d,maxIndex);

    x1 = ix1+lastPos;
    x2 = ix1+nextPos;
    if (x2>=iw) x2=iw-1;

    // inside
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRect(x1, iy1, x2-x1+1, ih);

    // lighter top border
    p.setPen(c.light());
    p.drawLine(x1, iy1, x2-1, iy1);

    // when width for last and current distance >2, draw full 3D effect...
    if (!leftDrawn) {
      p.drawLine(x1, iy1+1, x1, iy2);
      leftDrawn = true;
    }

    // darker bottom border
    p.setPen(c.dark());
    p.drawLine(x1, iy2, x2-1, iy2);

    lastPos = nextPos;
    lastDiff = diff;
    cLast = c;
    d++;
  }

  // right border (in last color)
  if (x2>0)
    p.drawLine(x2, iy1, x2, iy2);

  return pix;
}


QPixmap costPixmap(TraceCostType* ct, TraceCost* cost, double total, bool framed)
{
    if (ct->isReal()) {
	QColor color = ct->color();
	double p = 100.0 * cost->subCost(ct) / total;
	return percentagePixmap(COSTPIX_WIDTH, 10, (int)(p+.5), color, framed);
    }

    int maxIndex;
    double h[MaxRealIndexValue];
    QColor* cs = ct->mapping()->realColors();
    maxIndex = ct->histCost(cost, total, h);

    if (maxIndex ==0) return QPixmap();    
    return partitionPixmap(COSTPIX_WIDTH, 10, h, cs, maxIndex, framed);
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

	    
