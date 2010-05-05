/* This file is part of KCachegrind.
   Copyright (C) 2002-2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Global configuration for KCachegrind (only non-GUI options)
 */

#ifndef GLOBALCONFIG_H
#define GLOBALCONFIG_H

#include <QStringList>
#include <QHash>

#include "tracedata.h"

class GlobalConfig;

/**
 * Global configuration (only non-GUI options).
 * A singleton.
 */
class GlobalConfig
{
    friend class ConfigDlg;

public:
    GlobalConfig();
    virtual ~GlobalConfig();

    // gets the singleton instance
    static GlobalConfig* config();

    virtual void saveOptions();
    virtual void readOptions();

    static QStringList sourceDirs(TraceData*, TraceObject* o = 0);
    static bool showPercentage();
    static bool showExpanded();
    static bool showCycles();
    static bool hideTemplates();

    // lower percentage limit of cost items filled into lists
    static int percentPrecision();
    // max symbol lengths/count in tooltip/popup
    static int maxSymbolLength();
    // strip a symbol name according to <maxSymbolLength>
    static QString shortenSymbol(const QString&);
    static int maxSymbolCount();
    // max. number of items in lists
    static int maxListCount();

    // how many lines of context to show before/after annotated source/assembler
    static int context();
    // how many lines without cost are still regarded as inside a function
    static int noCostInside();

    const QStringList& generalSourceDirs();
    QStringList objectSourceDirs(QString);
    void setGeneralSourceDirs(QStringList);
    void setObjectSourceDirs(QString, QStringList);

    void setPercentPrecision(int);
    void setMaxSymbolLength(int);
    void setMaxSymbolCount(int);
    void setMaxListCount(int);
    void setContext(int);

    static void setShowPercentage(bool);
    static void setShowExpanded(bool);

    static void setShowCycles(bool);

    static void setHideTemplates(bool);
    // upper limit for cutting of a call in cycle detection
    static double cycleCut();

    void addDefaultTypes();

protected:
    QStringList knownTypes();
    QString knownFormula(const QString& name);
    QString knownLongName(const QString& name);

    QStringList _generalSourceDirs;
    QHash<QString, QStringList> _objectSourceDirs;

    bool _showPercentage, _showExpanded, _showCycles, _hideTemplates;
    double _cycleCut;
    int _percentPrecision;
    int _maxSymbolLength, _maxSymbolCount, _maxListCount;
    int _context, _noCostInside;

    static GlobalConfig* _config;
};

#endif // GLOBALCONFIG_H
