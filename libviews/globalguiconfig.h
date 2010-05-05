/* This file is part of KCachegrind.
   Copyright (C) 2010 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Global configuration for GUI components of KCachegrind
 */

#ifndef GLOBALGUICONFIG_H
#define GLOBALGUICONFIG_H

#include <QColor>
#include <QString>
#include <QHash>

#include "globalconfig.h"

/**
 * Color setting for a cost item
 * Helper class for color settings in configuration.
 *
 * Objects can only be instantiated by the singleton GlobalConfig
 * and therefore are unique; changes will be saved as configuration.
 */
class ConfigColorSetting
{
    friend class GlobalGUIConfig;
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
 * Extension of global configuration for GUI options.
 * A singleton.
 */
class GlobalGUIConfig: public GlobalConfig
{
    friend class ConfigDlg;

public:
    GlobalGUIConfig();
    ~GlobalGUIConfig();

  // gets the singleton instance
  static GlobalGUIConfig* config();

  void saveOptions();
  void readOptions();

  // color for visualization of an object
  static QColor functionColor(ProfileContext::Type gt, TraceFunction*);
  static QColor groupColor(CostItem*);
  static QColor eventTypeColor(EventType*);
  static ConfigColorSetting* groupColorSetting(CostItem*);
  static ConfigColorSetting* groupColorSetting(ProfileContext::Type, QString);

protected:
  static ConfigColorSetting* colorSetting(const QString&, bool createNew = true);
  QHash<QString, ConfigColorSetting*> _colors;
};

#endif // GLOBALGUICONFIG_H
