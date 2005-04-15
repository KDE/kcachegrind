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

#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>

#include "configuration.h"
#include "tracedata.h"
#include "configdlgbase.h"

#include "traceitemview.h"

//
// Some predefined cost types...
//

static QStringList knownTypes()
{
  QStringList l;

  l << "Ir"   << "Dr"   << "Dw"
    << "I1mr" << "D1mr" << "D1mw"
    << "I2mr" << "D2mr" << "D2mw"

    << "Smp"  << "Sys"  << "User"
    << "L1m"  << "L2m"  << "CEst";

  return l;
}


static QString knownFormula(QString name)
{
  if (name =="L1m") return QString("I1mr + D1mr + D1mw");
  if (name =="L2m") return QString("I2mr + D2mr + D2mw");
  if (name =="CEst") return QString("Ir + 10 L1m + 100 L2m");

  return QString::null;
}

static QString knownLongName(QString name)
{
    if (name == "Ir") return i18n("Instruction Fetch");
    if (name =="Dr") return i18n("Data Read Access");
    if (name =="Dw") return i18n("Data Write Access");
    if (name =="I1mr") return i18n("L1 Instr. Fetch Miss");
    if (name =="D1mr") return i18n("L1 Data Read Miss");
    if (name =="D1mw") return i18n("L1 Data Write Miss");
    if (name =="I2mr") return i18n("L2 Instr. Fetch Miss");
    if (name =="D2mr") return i18n("L2 Data Read Miss");
    if (name =="D2mw") return i18n("L2 Data Write Miss");
    if (name =="Smp") return i18n("Samples");
    if (name =="Sys") return i18n("System Time");
    if (name =="User") return i18n("User Time");
    if (name =="L1m") return i18n("L1 Miss Sum");
    if (name =="L2m") return i18n("L2 Miss Sum");
    if (name =="CEst") return i18n("Cycle Estimation");

    return QString::null;
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

Configuration* Configuration::config()
{
  if (!_config)
    _config = new Configuration();

  return _config;
}


void Configuration::saveOptions(KConfig* kconfig)
{
  Configuration* c = config();

  // color options
  KConfigGroup colorConfig(kconfig, QCString("CostColors"));
  QDictIterator<ColorSetting> it( c->_colors );
  int count = 1;
  for( ; it.current(); ++it ) {
    if ( !(*it)->automatic ) {
      colorConfig.writeEntry( QString("Name%1").arg(count),
                              it.currentKey());
      colorConfig.writeEntry( QString("Color%1").arg(count),
                              (*it)->color);
      //qDebug("Written Color %s (%d)", it.currentKey().ascii(), count);

      count++;
    }
  }
  colorConfig.writeEntry( "Count", count-1);

  // source options
  KConfigGroup sourceConfig(kconfig, QCString("Source"));
  sourceConfig.writeEntry("Dirs", c->_generalSourceDirs, ':');
  QDictIterator<QStringList> it2( c->_objectSourceDirs );
  count = 1;
  for( ; it2.current(); ++it2 ) {
    sourceConfig.writeEntry( QString("Object%1").arg(count),
			     it2.currentKey());
    sourceConfig.writeEntry( QString("Dirs%1").arg(count),
			     *(*it2), ':');
    count++;
  }
  sourceConfig.writeEntry( "Count", count-1);

  // general options
  KConfigGroup generalConfig(kconfig, QCString("General"));
  generalConfig.writeEntry("ShowPercentage", c->_showPercentage);
  generalConfig.writeEntry("ShowExpanded", c->_showExpanded);
  generalConfig.writeEntry("ShowCycles", c->_showCycles);
  generalConfig.writeEntry("CycleCut", c->_cycleCut);
  generalConfig.writeEntry("MaxSymbolCount", c->_maxSymbolCount);
  generalConfig.writeEntry("MaxListCount", c->_maxListCount);
  generalConfig.writeEntry("MaxSymbolLength", c->_maxSymbolLength);
  generalConfig.writeEntry("PercentPrecision", c->_percentPrecision);

  generalConfig.writeEntry("Context", c->_context);
  generalConfig.writeEntry("NoCostInside", c->_noCostInside);

  KConfigGroup ctConfig(kconfig, QCString("CostTypes"));
  int ctCount = TraceCostType::knownTypeCount();
  ctConfig.writeEntry( "Count", ctCount);
  for (int i=0; i<ctCount; i++) {
    TraceCostType* t = TraceCostType::knownType(i);
    ctConfig.writeEntry( QString("Name%1").arg(i+1), t->name());

    // Use localized key
    TraceItemView::writeConfigEntry(&ctConfig,
                                    QString("Longname%1").arg(i+1).ascii(),
                                    t->longName(),
                                    knownLongName(t->name()).utf8().data() /*, true */ );
    TraceItemView::writeConfigEntry(&ctConfig,
                                    QString("Formula%1").arg(i+1).ascii(),
                                    t->formula(), knownFormula(t->name()).utf8().data());
  }
}




void Configuration::readOptions(KConfig* kconfig)
{
  int i, count;
  Configuration* c = config();

  // color options
  c->_colors.clear();

  // colors for default cost types:
  //  red for L2 misses, green for L1 misses, blue for normal accesses
  c->color("CostType-I2mr")->color = QColor(240, 0, 0);
  c->color("CostType-D2mr")->color = QColor(180,40,40);
  c->color("CostType-D2mw")->color = QColor(120,80,80);

  c->color("CostType-I1mr")->color = QColor(0, 240, 0);
  c->color("CostType-D1mr")->color = QColor(40,180,40);
  c->color("CostType-D1mw")->color = QColor(80,120,80);

  c->color("CostType-Ir")->color = QColor(0, 0, 240);
  c->color("CostType-Dr")->color = QColor(40,40,180);
  c->color("CostType-Dw")->color = QColor(80,80,120);

  KConfigGroup colorConfig(kconfig, QCString("CostColors"));
  count = colorConfig.readNumEntry("Count", 0);
  for (i=1;i<=count;i++) {
    QString n = colorConfig.readEntry(QString("Name%1").arg(i));
    QColor color = colorConfig.readColorEntry(QString("Color%1").arg(i));

    if (n.isEmpty()) continue;

    ColorSetting* cs = new ColorSetting;
    cs->name = n;
    cs->automatic = false;
    cs->color = color;

    c->_colors.insert(n, cs);

    //qDebug("Read Color %s", n.ascii());
  }

  // source options
  KConfigGroup sourceConfig(kconfig, QCString("Source"));
  QStringList dirs;
  dirs = sourceConfig.readListEntry("Dirs", ':');
  if (dirs.count()>0) c->_generalSourceDirs = dirs;
  count = sourceConfig.readNumEntry("Count", 0);
  c->_objectSourceDirs.clear();
  if (count>17) c->_objectSourceDirs.resize(count);
  for (i=1;i<=count;i++) {
    QString n = sourceConfig.readEntry(QString("Object%1").arg(i));
    dirs = sourceConfig.readListEntry(QString("Dirs%1").arg(i), ':');

    if (n.isEmpty() || (dirs.count()==0)) continue;

    c->_objectSourceDirs.insert(n, new QStringList(dirs));
  }


  // general options
  KConfigGroup generalConfig(kconfig, QCString("General"));
  c->_showPercentage = generalConfig.readBoolEntry("ShowPercentage", true);
  c->_showExpanded = generalConfig.readBoolEntry("ShowExpanded", false);
  c->_showCycles = generalConfig.readBoolEntry("ShowCycles", true);
  c->_cycleCut = generalConfig.readDoubleNumEntry("CycleCut", 0.0);
  c->_maxSymbolCount = generalConfig.readNumEntry("MaxSymbolCount", 10);
  c->_maxListCount = generalConfig.readNumEntry("MaxListCount", 100);
  c->_maxSymbolLength = generalConfig.readNumEntry("MaxSymbolLength", 30);
  c->_percentPrecision = generalConfig.readNumEntry("PercentPrecision", 2);

  c->_context = generalConfig.readNumEntry("Context", 3);
  c->_noCostInside   = generalConfig.readNumEntry("NoCostInside", 20);

  // known cost types
  if (TraceCostType::knownTypeCount()==0) {

    KConfigGroup ctConfig(kconfig, QCString("CostTypes"));
    int ctCount = ctConfig.readNumEntry("Count", 0);
    if (ctCount>0) {
      for (int i=1;i<=ctCount;i++) {
        QString n = ctConfig.readEntry(QString("Name%1").arg(i));
        QString l = ctConfig.readEntry(QString("Longname%1").arg(i));
	if (l.isEmpty()) l = knownLongName(n);
        QString f = ctConfig.readEntry(QString("Formula%1").arg(i));
	if (f.isEmpty()) f = knownFormula(n);

        TraceCostType::add(new TraceCostType(n, l, f));
      }
    }
    else {
      // add default types

      QString longName, formula;
      TraceCostType* ct;
      QStringList l = knownTypes();
      for ( QStringList::Iterator it = l.begin();
            it != l.end(); ++it ) {
        longName = knownLongName(*it);
        formula  = knownFormula(*it);
        ct = new TraceCostType(*it, longName, formula);
        TraceCostType::add(ct);
      }
    }
  }
}

QColor Configuration::groupColor(TraceItem* cost)
{
   QString n;

  if (!cost)
    n = QString("default");
  else
    n = TraceCost::typeName(cost->type()) + "-" + cost->prettyName();

  return color(n)->color;
}

QColor Configuration::costTypeColor(TraceCostType* t)
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
        "-" + group->prettyName() +
        "-" + f->prettyName();

    ColorSetting* cs = color(n, false);
    if (cs) return cs->color;
  }
  return groupColor(group);
}

Configuration::ColorSetting* Configuration::color(QString n, bool createNew)
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

    for(unsigned int i=0;i<ol->count();i++)
      l.prepend( (*ol)[i] );
  }
  if (ol2) {
    for(unsigned int i=0;i<ol2->count();i++)
      l.prepend( (*ol2)[i] );
  }
  if (0) kdDebug() << "Configuration::sourceDirs: " << l.join(":") << endl;

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

QString Configuration::shortenSymbol(QString s)
{
    if ((int)s.length() > maxSymbolLength())
	s = s.left(maxSymbolLength()) + "...";
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
