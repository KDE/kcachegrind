/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Some helper functions for QListViewItem derivates
 */

#ifndef LISTUTILS_H
#define LISTUTILS_H

#include <QPixmap>
#include <QString>
#include <QColor>

#include "subcost.h"

class EventType;
class EventTypeSet;

QString bigNum(SubCost);
QPixmap colorPixmap(int w, int h, QColor c);
QPixmap percentagePixmap(int w, int h, int percent, QColor c, bool framed);
QPixmap partitionPixmap(int w, int h, double* hist, EventTypeSet*,
                        int maxIndex, bool framed);
QPixmap costPixmap(EventType* ct, ProfileCostArray* cost,
                   double total, bool framed);

#endif
