/* This file is part of KCachegrind.
   Copyright (C) 2004 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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


#include "subcost.h"
#include "tracedata.h"

#include <QString>


//---------------------------------------------------
// SubCost

bool SubCost::set(const char** ps)
{
    const char* s = *ps;
    if (!s || (*s < '0') || (*s > '9')) return false;

    v = *s - '0';
    s++;
    while(*s >= '0' && *s <= '9') {
	v = 10* v + (*s-'0');
	s++;
    }
    while(*s == ' ') s++;
    *ps = s;

    return true;
}

QString SubCost::pretty(char sep) const
{
    unsigned long long n = v;

    if (n==0) return QString("0");

    int i = 0;
    QString res = "";

    while (n) {
	if ((i>0) && !(i%3)) res = sep + res;
	i++;
	res = QChar('0'+int(n%10)) + res;
	n /= 10;
    }
    return res;
}




// HighestCostList

HighestCostList::HighestCostList()
{
    _costType = 0;
    clear(1);
}

void HighestCostList::clear(int maxSize)
{
    _maxSize = maxSize;
    _count = 0;
    _item.resize(maxSize);
    _cost.resize(maxSize);
}

void HighestCostList::addCost(ProfileCostArray* c, SubCost cost)
{
    int i;

    if (_maxSize == 0) return;

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


