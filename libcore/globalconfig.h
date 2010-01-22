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
 * Global configuration for KCachegrind
 */

#ifndef GLOBALCONFIG_H
#define GLOBALCONFIG_H

#include <QColor>
#include <QStringList>
#include <QHash>

#include "tracedata.h"

class GlobalConfig;

/**
 * Color setting for a cost item
 * Helper class for color settings in configuration.
 *
 * Objects can only be instantiated by the singleton GlobalConfig
 * and therefore are unique; changes will be saved as configuration.
 */
class ConfigColorSetting
{
    friend class GlobalConfig;
    friend class ConfigDlg;

public:
    QColor color() const { return _color; }
    bool automatic() const { return _automatic; }

    static QColor colorForName(QString);
    // returns automatic color calculated from name, but does not change color
    QColor autoColor() const;

    // explicitly set a color, switches off automatic mode
    void setColor(const QColor&);
    // reset to automatic mode
    void reset();

private:
    ConfigColorSetting(QString n); // color calculated from name
    ConfigColorSetting(QString, QColor); // color set explicitly

    QString _name;
    QColor _color;
    bool _automatic;
};

/**
 * Global configuration.
 * A singleton.
 */
class GlobalConfig
{
    friend class ConfigDlg;

public:
  GlobalConfig();
  ~GlobalConfig();

  // gets the singleton instance
  static GlobalConfig* config();

  void saveOptions();
  void readOptions();

  // color for visualization of an object
  static QColor functionColor(ProfileContext::Type gt, TraceFunction*);
  static QColor groupColor(CostItem*);
  static QColor eventTypeColor(EventType*);
  static ConfigColorSetting* groupColorSetting(CostItem*);
  static ConfigColorSetting* groupColorSetting(ProfileContext::Type, QString);

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

  static ConfigColorSetting* colorSetting(const QString&, bool createNew = true);
  QHash<QString, ConfigColorSetting*> _colors;

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

