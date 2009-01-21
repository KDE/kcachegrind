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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * Configuration for KCachegrind
 */

#include "kconfiguration.h"

#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>

#include "tracedata.h"
#include "traceitemview.h"
#include "ui_configdlgbase.h"

//
// Configuration
//

KConfiguration::KConfiguration(KConfig* kc)
{
    _kconfig = kc;
}

void KConfiguration::saveOptions()
{
  KConfiguration* c = (KConfiguration*) config();
  KConfig* kconfig = c->_kconfig;

  // color options
  KConfigGroup colorConfig(kconfig, "CostColors");
  Q3DictIterator<ColorSetting> it( c->_colors );
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
  KConfigGroup sourceConfig(kconfig, "Source");
  // XXX joining the list with a custom separator without escaping is
  // bogus. should use regular list entries.
  sourceConfig.writeEntry("Dirs", c->_generalSourceDirs.join(":"));
  Q3DictIterator<QStringList> it2( c->_objectSourceDirs );
  count = 1;
  for( ; it2.current(); ++it2 ) {
    sourceConfig.writeEntry( QString("Object%1").arg(count),
			     it2.currentKey());
    sourceConfig.writeEntry( QString("Dirs%1").arg(count),
			     (*(*it2)).join(":"));
    count++;
  }
  sourceConfig.writeEntry( "Count", count-1);

  // general options
  KConfigGroup generalConfig(kconfig, "General");
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

  KConfigGroup ctConfig(kconfig, "CostTypes");
  int ctCount = TraceEventType::knownTypeCount();
  ctConfig.writeEntry( "Count", ctCount);
  for (int i=0; i<ctCount; i++) {
    TraceEventType* t = TraceEventType::knownType(i);
    ctConfig.writeEntry( QString("Name%1").arg(i+1), t->name());

    // Use localized key
    TraceItemView::writeConfigEntry(ctConfig,
                                    QString("Longname%1").arg(i+1).ascii(),
                                    t->longName(),
                                    knownLongName(t->name()).utf8().data() /*, true */ );
    TraceItemView::writeConfigEntry(ctConfig,
                                    QString("Formula%1").arg(i+1).ascii(),
                                    t->formula(), knownFormula(t->name()).utf8().data());
  }
}




void KConfiguration::readOptions()
{
  int i, count;

  KConfiguration* c = (KConfiguration*) config();
  KConfig* kconfig = c->_kconfig;

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

  KConfigGroup colorConfig(kconfig, "CostColors");
  count = colorConfig.readEntry("Count", 0);
  for (i=1;i<=count;i++) {
    QString n = colorConfig.readEntry(QString("Name%1").arg(i),QString());
    QColor color = colorConfig.readEntry(QString("Color%1").arg(i), QColor(Qt::black));

    if (n.isEmpty()) continue;

    ColorSetting* cs = new ColorSetting;
    cs->name = n;
    cs->automatic = false;
    cs->color = color;

    c->_colors.insert(n, cs);

    //qDebug("Read Color %s", n.ascii());
  }

  // source options
  KConfigGroup sourceConfig(kconfig, "Source");
  QStringList dirs;
  dirs = sourceConfig.readEntry("Dirs", QString()).split(':');
  if (dirs.count()>0) c->_generalSourceDirs = dirs;
  count = sourceConfig.readEntry("Count", 0);
  c->_objectSourceDirs.clear();
  if (count>17) c->_objectSourceDirs.resize(count);
  for (i=1;i<=count;i++) {
    QString n = sourceConfig.readEntry(QString("Object%1").arg(i),QString());
    dirs = sourceConfig.readEntry(QString("Dirs%1").arg(i), QString()).split(':');

    if (n.isEmpty() || (dirs.count()==0)) continue;

    c->_objectSourceDirs.insert(n, new QStringList(dirs));
  }


  // general options
  KConfigGroup generalConfig(kconfig, "General");
  c->_showPercentage = generalConfig.readEntry("ShowPercentage", true);
  c->_showExpanded = generalConfig.readEntry("ShowExpanded", false);
  c->_showCycles = generalConfig.readEntry("ShowCycles", true);
  c->_cycleCut = generalConfig.readEntry("CycleCut", 0.0);
  c->_maxSymbolCount = generalConfig.readEntry("MaxSymbolCount", 10);
  c->_maxListCount = generalConfig.readEntry("MaxListCount", 100);
  c->_maxSymbolLength = generalConfig.readEntry("MaxSymbolLength", 30);
  c->_percentPrecision = generalConfig.readEntry("PercentPrecision", 2);

  c->_context = generalConfig.readEntry("Context", 3);
  c->_noCostInside   = generalConfig.readEntry("NoCostInside", 20);

  // known cost types
  if (TraceEventType::knownTypeCount() >0) return;

  KConfigGroup ctConfig(kconfig, "CostTypes");
  int ctCount = ctConfig.readEntry("Count", 0);

  if (ctCount == 0) {
      addDefaultTypes();
      return;
  }

  for (int i=1;i<=ctCount;i++) {
      QString n = ctConfig.readEntry(QString("Name%1").arg(i),QString());
      QString l = ctConfig.readEntry(QString("Longname%1").arg(i),QString());
      if (l.isEmpty()) l = knownLongName(n);
      QString f = ctConfig.readEntry(QString("Formula%1").arg(i), QString());
      if (f.isEmpty()) f = knownFormula(n);

      TraceEventType::add(new TraceEventType(n, l, f));
  }
}

