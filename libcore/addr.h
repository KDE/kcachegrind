/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef ADDR_H
#define ADDR_H

#include <QString>

#include "utils.h"

/**
 * Addresses are 64bit values like costs to be able
 * to always load profile data produced on 64bit
 * architectures.
 */
class Addr
{
public:
    Addr() { _v=0; }
    // no "explicit": we want implicit conversion
    Addr(uint64 v) { _v = v; }

    // Interprets char data at s as hex (without "0x" prefix)
    // and return number of interpreted chars.
    int set(const char *s);
    bool set(FixString& s);
    QString toString() const;
    // similar to toString(), but adds a space every 4 digits
    QString pretty() const;

    // returns true if this address is in [a-distance;a+distance]
    bool isInRange(Addr a, int distance);

    bool operator==(const Addr& a) const { return (_v == a._v); }
    bool operator!=(const Addr& a) const { return (_v != a._v); }
    bool operator>(const Addr& a) const { return _v > a._v; }
    bool operator>=(const Addr& a) const { return _v >= a._v; }
    bool operator<(const Addr& a) const { return _v < a._v; }
    bool operator<=(const Addr& a) const { return _v <= a._v; }

    Addr operator+(int d) const { return Addr(_v + d); }
    Addr operator-(int d) const { return Addr(_v - d); }

    // return decremented address until it is a multiple of <a>, a power of 2
    Addr alignedDown(int a) { return Addr(_v & ~( ((uint64)a) -1)); }

private:
    uint64 _v;
};

#endif // ADDR_H
