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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Configuration for KCachegrind
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <qcolor.h>
#include <qstringlist.h>
#include <qdict.h>

#include "tracedata.h"

class KConfig;

class Configuration
{
  friend class ConfigDlg;

public:
  Configuration();

  static Configuration* config();

  static void saveOptions(KConfig*);
  static void readOptions(KConfig*);

  // color for visualisation of an object
  static QColor functionColor(TraceItem::CostType gt, TraceFunction*);
  static QColor groupColor(TraceItem*);
  static QColor costTypeColor(TraceCostType*);
  static QStringList sourceDirs(TraceData*, TraceObject* o = 0);
  static bool showPercentage();
  static bool showExpanded();
  static bool showCycles();

  // lower percentage limit of cost items filled into lists
  static int percentPrecision();
  // max symbol lengths/count in tooltip/popup
  static int maxSymbolLength();
  // strip a symbol name according to <maxSymbolLength>
  static QString shortenSymbol(QString);
  static int maxSymbolCount();
  // max. number of items in lists
  static int maxListCount();
  
  // how many lines of context to show before/after annotated source/assembler
  static int context();
  // how many lines without cost are still regarded as inside a function
  static int noCostInside();

  static void setShowPercentage(bool);
  static void setShowExpanded(bool);

  static void setShowCycles(bool);
  // upper limit for cutting of a call in cycle detection
  static double cycleCut();

private:
  struct ColorSetting {
    QString name;
    QColor color;
    bool automatic;
  };

  static ColorSetting* color(QString, bool createNew = true);

  QDict<ColorSetting> _colors;

  QStringList _generalSourceDirs;
  QDict<QStringList> _objectSourceDirs;

  bool _showPercentage, _showExpanded, _showCycles;
  double _cycleCut;
  int _percentPrecision;
  int _maxSymbolLength, _maxSymbolCount, _maxListCount;
  int _context, _noCostInside;

  static Configuration* _config;
};


#endif
