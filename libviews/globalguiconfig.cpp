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

#include "globalguiconfig.h"

#include "config.h"


//
// ConfigColorSettings
//

ConfigColorSetting::ConfigColorSetting(QString n)
{
    _name = n;
    reset();
}

ConfigColorSetting::ConfigColorSetting(QString n, QColor c)
{
    _name = n;
    setColor(c);
}

void ConfigColorSetting::setColor(const QColor& c)
{
    _color = c;
    _automatic = (c == colorForName(_name));
}

QColor ConfigColorSetting::colorForName(QString n)
{
    int h = 0, s = 100;
    foreach(const QChar c, n) {
      h = (h * 37 + s* c.unicode()) % 256;
      s = (s * 17 + h* c.unicode()) % 192;
    }

    return QColor::fromHsv(h, 64+s, 192);
}

QColor ConfigColorSetting::autoColor() const
{
    return colorForName(_name);
}

void ConfigColorSetting::reset()
{
    _automatic = true;
    _color = colorForName(_name);
}


//
// GlobalGUIConfig
//

GlobalGUIConfig::GlobalGUIConfig()
    : GlobalConfig()
{
}

GlobalGUIConfig::~GlobalGUIConfig()
{
    qDeleteAll(_colors);
    _colors.clear();
}

GlobalGUIConfig* GlobalGUIConfig::config()
{
    GlobalGUIConfig* gc;

    if (_config == 0) {
        gc = new GlobalGUIConfig();
        _config = gc;
    }
    else {
        gc = dynamic_cast<GlobalGUIConfig*>(_config);
        if (gc == 0)
            qFatal("Internal error: config object is not a GlobalGUIConfig.");
    }

    return gc;
}

void GlobalGUIConfig::saveOptions()
{
    // color options
    ConfigGroup* colorConfig = ConfigStorage::group("CostColors");
    int count = 1;
    foreach(ConfigColorSetting* cs, _colors) {
        if ( !cs->_automatic ) {
            colorConfig->setValue( QString("Name%1").arg(count),
                                   cs->_name);
            colorConfig->setValue( QString("Color%1").arg(count),
                                   cs->_color);
            count++;
        }
    }
    colorConfig->setValue("Count", count-1);
    delete colorConfig;

    GlobalConfig::saveOptions();
}

void GlobalGUIConfig::readOptions()
{
    int i, count;

    // color options
    _colors.clear();
    // colors for default event types:
    //  red for LL/L2 misses
    colorSetting("EventType-I2mr")->_color = QColor(240, 0, 0);
    colorSetting("EventType-D2mr")->_color = QColor(180,40,40);
    colorSetting("EventType-D2mw")->_color = QColor(120,80,80);
    colorSetting("EventType-ILmr")->_color = QColor(240, 0, 0);
    colorSetting("EventType-DLmr")->_color = QColor(180,40,40);
    colorSetting("EventType-DLmw")->_color = QColor(120,80,80);
    //  green for L1 misses
    colorSetting("EventType-I1mr")->_color = QColor(0, 240, 0);
    colorSetting("EventType-D1mr")->_color = QColor(40,180,40);
    colorSetting("EventType-D1mw")->_color = QColor(80,120,80);
    // yellow for branches/mispredictions
    colorSetting("EventType-Bc") ->_color = QColor(240,240, 0);
    colorSetting("EventType-Bcm")->_color = QColor(200,200,30);
    colorSetting("EventType-Bi") ->_color = QColor(160,160,60);
    colorSetting("EventType-Bim")->_color = QColor(120,120,90);
    // blue for normal accesses
    colorSetting("EventType-Ir")->_color = QColor(0, 0, 240);
    colorSetting("EventType-Dr")->_color = QColor(40,40,180);
    colorSetting("EventType-Dw")->_color = QColor(80,80,120);

    ConfigGroup* colorConfig = ConfigStorage::group("CostColors");
    count = colorConfig->value("Count", 0).toInt();
    for (i=1;i<=count;i++) {
        QString n = colorConfig->value(QString("Name%1").arg(i),
                                       QString()).toString();
        QColor color = colorConfig->value<QColor>(QString("Color%1").arg(i),
                                                  QColor(Qt::black));

        if (n.isEmpty()) continue;

        ConfigColorSetting* cs = new ConfigColorSetting(n,color);
        _colors.insert(n, cs);
    }
    delete colorConfig;

    GlobalConfig::readOptions();
}

ConfigColorSetting* GlobalGUIConfig::groupColorSetting(CostItem* cost)
{
    QString n;

    if (!cost)
        return colorSetting(QString("default"));

    return groupColorSetting(cost->type(), cost->name());
}

ConfigColorSetting* GlobalGUIConfig::groupColorSetting(ProfileContext::Type t,
                                                       QString name)
{
    QString n = ProfileContext::typeName(t) + '-' + name;
    return colorSetting(n);
}

QColor GlobalGUIConfig::groupColor(CostItem* cost)
{
    return groupColorSetting(cost)->color();
}

QColor GlobalGUIConfig::eventTypeColor(EventType* t)
{
    QString n;

    if (!t)
        n = QString("EventType-default");
    else
        n = QString("EventType-%1").arg(t->name());

    return colorSetting(n)->color();
}

QColor GlobalGUIConfig::functionColor(ProfileContext::Type gt,
                                      TraceFunction* f)
{
    ProfileCostArray* group = f;
    QString n;

    switch(gt) {
    case ProfileContext::Object: group = f->object(); break;
    case ProfileContext::Class:  group = f->cls(); break;
    case ProfileContext::File:   group = f->file(); break;
    default:
        break;
    }

    if (group != f) {
        // first look for manual color of a function in a group
        n = ProfileContext::typeName(group->type()) +
            '-' + group->name() +
            '-' + f->name();

        ConfigColorSetting* cs = colorSetting(n, false);
        if (cs) return cs->color();
    }
    return groupColor(group);
}

ConfigColorSetting* GlobalGUIConfig::colorSetting(const QString& n,
                                                  bool createNew)
{
    // predefined ?
    GlobalGUIConfig* c = config();
    ConfigColorSetting* cs = c->_colors.value(n, 0);
    if (cs || !createNew) return cs;

    cs = new ConfigColorSetting(n);
    c->_colors.insert(n, cs);

    return cs;
}

