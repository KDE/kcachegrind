/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2004-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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

    if (n==0) return QStringLiteral("0");

    int i = 0;
    QString res;

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
    _costType = nullptr;
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


