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
QPixmap costPixmap(TraceEventType* ct, ProfileCostArray* cost, double total, bool framed);

#endif
