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

#ifndef LISTUTILS_H
#define LISTUTILS_H

#include <qpixmap.h>
#include <qstring.h>
#include <qcolor.h>
#include "tracedata.h"

QString bigNum(SubCost);
QPixmap colorPixmap(int w, int h, QColor c);
QPixmap percentagePixmap(int w, int h, int percent, QColor c, bool framed);
QPixmap partitionPixmap(int w, int h, double* hist, QColor*,
			int maxIndex, bool framed);
QPixmap costPixmap(TraceCostType* ct, TraceCost* cost, double total, bool framed);

/**
 * A class to calculate the <maxSize> TraceCost items
 * with highest cost.
 */

class HighestCostList
{
 public:
    HighestCostList();
    
    void clear(int maxSize);
    void addCost(TraceCost*, SubCost);
    int count() { return _count; }
    int realCount() { return (_count > _maxSize) ? _maxSize:_count; }
    int maxSize() { return _maxSize; }
    bool hasMore() { return _count > _maxSize; }
    TraceCost* operator[] (int i)
	{ return (i>=0 && i<_count && i<_maxSize) ? _item[i] : 0; }
    
 private:
    TraceCostList _list;
    int _maxSize, _count;
    TraceCostType* _costType;
    QMemArray<TraceCost*> _item;
    QMemArray<SubCost> _cost;
};

#endif
