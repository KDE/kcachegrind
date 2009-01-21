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
 * Configuration for KCachegrind
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <qcolor.h>
#include <qstringlist.h>
#include <q3dict.h>

#include "tracedata.h"

/**
 * Actual storage of the configuration has to be implemented
 * in a derived class, which needs to overwrite save/readOptions.
 * This uses the singleton pattern, with the need to set
 * the global configuration object with setConfig() before use.
 */
class Configuration
{
public:
  // must be set to an instance of a derived class before use
  static void setConfig(Configuration*);
  static Configuration* config();

  virtual void saveOptions() = 0;
  virtual void readOptions() = 0;

  // color for visualization of an object
  static QColor functionColor(TraceItem::CostType gt, TraceFunction*);
  static QColor groupColor(TraceItem*);
  static QColor eventTypeColor(TraceEventType*);
  static QStringList sourceDirs(TraceData*, TraceObject* o = 0);
  static bool showPercentage();
  static bool showExpanded();
  static bool showCycles();

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

  static void setShowPercentage(bool);
  static void setShowExpanded(bool);

  static void setShowCycles(bool);
  // upper limit for cutting of a call in cycle detection
  static double cycleCut();


protected:
  // do not instantiate this class
  Configuration();
  virtual ~Configuration();

  QStringList knownTypes();
  QString knownFormula(const QString& name);
  QString knownLongName(const QString& name);
  void addDefaultTypes();

  struct ColorSetting {
    QString name;
    QColor color;
    bool automatic;
  };

  static ColorSetting* color(const QString&, bool createNew = true);

  Q3Dict<ColorSetting> _colors;

  QStringList _generalSourceDirs;
  Q3Dict<QStringList> _objectSourceDirs;

  bool _showPercentage, _showExpanded, _showCycles;
  double _cycleCut;
  int _percentPrecision;
  int _maxSymbolLength, _maxSymbolCount, _maxListCount;
  int _context, _noCostInside;

  static Configuration* _config;
};


#endif
