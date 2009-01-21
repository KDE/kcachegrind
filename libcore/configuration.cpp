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

#include <QtDebug>

// FIXME: Need to get rid of KDE dependencies in libcore
//#include <klocale.h>

#include "configuration.h"
#include "tracedata.h"

//
// Some predefined event types...
//

QStringList Configuration::knownTypes()
{
  QStringList l;

  l << "Ir"   << "Dr"   << "Dw"
    << "I1mr" << "D1mr" << "D1mw"
    << "I2mr" << "D2mr" << "D2mw"

    << "Smp"  << "Sys"  << "User"
    << "L1m"  << "L2m"  << "CEst";

  return l;
}


QString Configuration::knownFormula(const QString& name)
{
  if (name =="L1m") return QString("I1mr + D1mr + D1mw");
  if (name =="L2m") return QString("I2mr + D2mr + D2mw");
  if (name =="CEst") return QString("Ir + 10 L1m + 100 L2m");

  return QString();
}

QString Configuration::knownLongName(const QString& name)
{
    if (name == "Ir") return QObject::tr("Instruction Fetch");
    if (name =="Dr") return QObject::tr("Data Read Access");
    if (name =="Dw") return QObject::tr("Data Write Access");
    if (name =="I1mr") return QObject::tr("L1 Instr. Fetch Miss");
    if (name =="D1mr") return QObject::tr("L1 Data Read Miss");
    if (name =="D1mw") return QObject::tr("L1 Data Write Miss");
    if (name =="I2mr") return QObject::tr("L2 Instr. Fetch Miss");
    if (name =="D2mr") return QObject::tr("L2 Data Read Miss");
    if (name =="D2mw") return QObject::tr("L2 Data Write Miss");
    if (name =="Smp") return QObject::tr("Samples");
    if (name =="Sys") return QObject::tr("System Time");
    if (name =="User") return QObject::tr("User Time");
    if (name =="L1m") return QObject::tr("L1 Miss Sum");
    if (name =="L2m") return QObject::tr("L2 Miss Sum");
    if (name =="CEst") return QObject::tr("Cycle Estimation");

    return QString();
}


//
// Configuration
//

Configuration* Configuration::_config = 0;

Configuration::Configuration()
  :_colors(517)
{
  _config = 0;

  _colors.setAutoDelete(true);
  _objectSourceDirs.setAutoDelete(true);

  // defaults
  _showPercentage = true;
  _showExpanded = false;
  _showCycles = true;
  _cycleCut = 0.0;
  _percentPrecision = 2;

  // max symbol count/length in tooltip/popup
  _maxSymbolLength = 30;
  _maxSymbolCount = 10;
  _maxListCount = 100;

  // annotation behaviour
  _context = 3;
  _noCostInside   = 20;
}

Configuration::~Configuration()
{}

Configuration* Configuration::config()
{
  Q_ASSERT(_config != 0);

  return _config;
}

void Configuration::setConfig(Configuration* c)
{
    _config = c;
}

void Configuration::saveOptions()
{}

void Configuration::readOptions()
{}

void Configuration::addDefaultTypes()
{
    QString longName, formula;
    TraceEventType* ct;
    QStringList l = knownTypes();
    for ( QStringList::const_iterator it = l.begin();
	  it != l.end(); ++it ) {
        longName = knownLongName(*it);
        formula  = knownFormula(*it);
        ct = new TraceEventType(*it, longName, formula);
        TraceEventType::add(ct);
    }
}

QColor Configuration::groupColor(TraceItem* cost)
{
   QString n;

  if (!cost)
    n = QString("default");
  else
    n = TraceCost::typeName(cost->type()) + '-' + cost->prettyName();

  return color(n)->color;
}

QColor Configuration::eventTypeColor(TraceEventType* t)
{
   QString n;

   if (!t)
     n = QString("CostType-default");
   else
     n = QString("CostType-%1").arg(t->name());

   return color(n)->color;
}

QColor Configuration::functionColor(TraceCost::CostType gt,
                                    TraceFunction* f)
{
  TraceCost* group = f;
  QString n;

  switch(gt) {
  case TraceCost::Object: group = f->object(); break;
  case TraceCost::Class:  group = f->cls(); break;
  case TraceCost::File:   group = f->file(); break;
  default:
    break;
  }

  if (group != f) {
    // first look for manual color of a function in a group
    n = TraceCost::typeName(group->type()) +
        '-' + group->prettyName() +
        '-' + f->prettyName();

    ColorSetting* cs = color(n, false);
    if (cs) return cs->color;
  }
  return groupColor(group);
}

Configuration::ColorSetting* Configuration::color(const QString& n, bool createNew)
{
//  qDebug("Color for %s", n.latin1());

  // predefined ?
  Configuration* c = config();
  ColorSetting* cs = c->_colors[n];
  if (cs || !createNew) return cs;

  // automatic colors...
  int h = 0, s = 100;
  const char* str = n.ascii();
  while (*str) {
    h = (h * 37 + s* (unsigned)*str) % 256;
    s = (s * 17 + h* (unsigned)*str) % 192;
    str++;
  }

  //qDebug("New color for %s: H %d, S %d", n.ascii(), h, 64+s);
  QColor color = QColor(h, 64+s, 192, QColor::Hsv);

  cs = new ColorSetting;
  cs->name = n;
  cs->automatic = true;
  cs->color = color;
  c->_colors.insert(n, cs);

  //qDebug("new Color %s", n.ascii());

  return cs;
}

/* Gives back a list of all Source Base Directories of Objects in
 * current trace. If a special object is given in 2nd argument,
 * put its Source Base in front.
 */
QStringList Configuration::sourceDirs(TraceData* data, TraceObject* o)
{
  QStringList l = config()->_generalSourceDirs, *ol, *ol2 = 0;
  TraceObjectMap::Iterator oit;
  for ( oit = data->objectMap().begin();
        oit != data->objectMap().end(); ++oit ) {
    ol = config()->_objectSourceDirs[(*oit).name()];
    if (&(*oit) == o) {
      ol2 = ol;
      continue;
    }
    if (!ol) continue;

    for(int i=0;i<ol->count();i++)
      l.prepend( (*ol)[i] );
  }
  if (ol2) {
    for(int i=0;i<ol2->count();i++)
      l.prepend( (*ol2)[i] );
  }
  if (0) qDebug() << "Configuration::sourceDirs: " << l.join(":");

  return l;
}

bool Configuration::showPercentage()
{
  return config()->_showPercentage;
}

bool Configuration::showExpanded()
{
  return config()->_showExpanded;
}

bool Configuration::showCycles()
{
  return config()->_showCycles;
}

void Configuration::setShowPercentage(bool s)
{
  Configuration* c = config();
  if (c->_showPercentage == s) return;

  c->_showPercentage = s;
}

void Configuration::setShowExpanded(bool s)
{
  Configuration* c = config();
  if (c->_showExpanded == s) return;

  c->_showExpanded = s;
}

void Configuration::setShowCycles(bool s)
{
  Configuration* c = config();
  if (c->_showCycles == s) return;

  c->_showCycles = s;
}

double Configuration::cycleCut()
{
  return config()->_cycleCut;
}

int Configuration::percentPrecision()
{
  return config()->_percentPrecision;
}

int Configuration::maxSymbolLength()
{
  return config()->_maxSymbolLength;
}

QString Configuration::shortenSymbol(const QString& s)
{
	if ((int)s.length() > maxSymbolLength())
		return s.left(maxSymbolLength()) + "...";
	return s;
}

int Configuration::maxListCount()
{
  return config()->_maxListCount;
}

int Configuration::maxSymbolCount()
{
  return config()->_maxSymbolCount;
}

int Configuration::context()
{
  return config()->_context;
}

int Configuration::noCostInside()
{
  return config()->_noCostInside;
}
