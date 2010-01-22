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

#include "globalconfig.h"

#include <QtDebug>

#include "config.h"
#include "tracedata.h"


// GlobalConfig defaults
#define DEFAULT_SHOWPERCENTAGE   true
#define DEFAULT_SHOWEXPANDED     false
#define DEFAULT_SHOWCYCLES       true
#define DEFAULT_HIDETEMPLATES    false
#define DEFAULT_CYCLECUT         0.0
#define DEFAULT_PERCENTPRECISION 2
#define DEFAULT_MAXSYMBOLLENGTH  30
#define DEFAULT_MAXSYMBOLCOUNT   10
#define DEFAULT_MAXLISTCOUNT     100
#define DEFAULT_CONTEXT          3
#define DEFAULT_NOCOSTINSIDE     20



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
    const char* str = n.ascii();
    while (*str) {
      h = (h * 37 + s* (unsigned)*str) % 256;
      s = (s * 17 + h* (unsigned)*str) % 192;
      str++;
    }

    return QColor(h, 64+s, 192, QColor::Hsv);
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
// GlobalConfig
//

// Some predefined event types...
QStringList GlobalConfig::knownTypes()
{
  QStringList l;

  l << "Ir"   << "Dr"   << "Dw"
    << "I1mr" << "D1mr" << "D1mw"
    << "I2mr" << "D2mr" << "D2mw"

    << "Smp"  << "Sys"  << "User"
    << "L1m"  << "L2m"  << "CEst";

  return l;
}


QString GlobalConfig::knownFormula(const QString& name)
{
  if (name == "L1m") return QString("I1mr + D1mr + D1mw");
  if (name == "L2m") return QString("I2mr + D2mr + D2mw");
  if (name == "CEst") return QString("Ir + 10 L1m + 100 L2m");

  return QString();
}

QString GlobalConfig::knownLongName(const QString& name)
{
    if (name == "Ir") return QObject::tr("Instruction Fetch");
    if (name == "Dr") return QObject::tr("Data Read Access");
    if (name == "Dw") return QObject::tr("Data Write Access");
    if (name == "I1mr") return QObject::tr("L1 Instr. Fetch Miss");
    if (name == "D1mr") return QObject::tr("L1 Data Read Miss");
    if (name == "D1mw") return QObject::tr("L1 Data Write Miss");
    if (name == "I2mr") return QObject::tr("L2 Instr. Fetch Miss");
    if (name == "D2mr") return QObject::tr("L2 Data Read Miss");
    if (name == "D2mw") return QObject::tr("L2 Data Write Miss");
    if (name == "Smp") return QObject::tr("Samples");
    if (name == "Sys") return QObject::tr("System Time");
    if (name == "User") return QObject::tr("User Time");
    if (name == "L1m") return QObject::tr("L1 Miss Sum");
    if (name == "L2m") return QObject::tr("L2 Miss Sum");
    if (name == "CEst") return QObject::tr("Cycle Estimation");

    return QString();
}


//
// GlobalConfig
//

GlobalConfig* GlobalConfig::_config = 0;

GlobalConfig::GlobalConfig()
{
  _config = 0;

  // general presentation options
  _showPercentage   = DEFAULT_SHOWPERCENTAGE;
  _showExpanded     = DEFAULT_SHOWEXPANDED;
  _showCycles       = DEFAULT_SHOWCYCLES;
  _cycleCut         = DEFAULT_CYCLECUT;
  _percentPrecision = DEFAULT_PERCENTPRECISION;
  _hideTemplates    = DEFAULT_HIDETEMPLATES;

  // max symbol count/length in tooltip/popup
  _maxSymbolLength  = DEFAULT_MAXSYMBOLLENGTH;
  _maxSymbolCount   = DEFAULT_MAXSYMBOLCOUNT;
  _maxListCount     = DEFAULT_MAXLISTCOUNT;

  // annotation behaviour
  _context          = DEFAULT_CONTEXT;
  _noCostInside     = DEFAULT_NOCOSTINSIDE;
}

GlobalConfig::~GlobalConfig()
{
    qDeleteAll(_colors);
    _colors.clear();
}

GlobalConfig* GlobalConfig::config()
{
    if (_config == 0)
	_config = new GlobalConfig();

    return _config;
}


void GlobalConfig::saveOptions()
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

    // source options
    ConfigGroup* sourceConfig = ConfigStorage::group("Source");
    sourceConfig->setValue("Dirs", _generalSourceDirs);
    QHashIterator<QString,QStringList> it( _objectSourceDirs );
    count = 1;
    while( it.hasNext() ) {
	it.next();
	sourceConfig->setValue( QString("Object%1").arg(count),
				it.key() );
	sourceConfig->setValue( QString("Dirs%1").arg(count),
				it.value() );
	count++;
    }
    sourceConfig->setValue("Count", count-1);
    delete sourceConfig;

    // general options
    ConfigGroup* generalConfig = ConfigStorage::group("GeneralSettings");
    generalConfig->setValue("ShowPercentage", _showPercentage,
			    DEFAULT_SHOWPERCENTAGE);
    generalConfig->setValue("ShowExpanded", _showExpanded,
			    DEFAULT_SHOWEXPANDED);
    generalConfig->setValue("ShowCycles", _showCycles,
			    DEFAULT_SHOWCYCLES);
    generalConfig->setValue("CycleCut", _cycleCut,
			    DEFAULT_CYCLECUT);
    generalConfig->setValue("PercentPrecision", _percentPrecision,
			    DEFAULT_PERCENTPRECISION);
    generalConfig->setValue("MaxSymbolLength", _maxSymbolLength,
			    DEFAULT_MAXSYMBOLLENGTH);
    generalConfig->setValue("MaxSymbolCount", _maxSymbolCount,
			    DEFAULT_MAXSYMBOLCOUNT);
    generalConfig->setValue("MaxListCount", _maxListCount,
			    DEFAULT_MAXLISTCOUNT);
    generalConfig->setValue("Context", _context,
			    DEFAULT_CONTEXT);
    generalConfig->setValue("NoCostInside", _noCostInside,
			    DEFAULT_NOCOSTINSIDE);
    generalConfig->setValue("HideTemplates", _hideTemplates,
                            DEFAULT_HIDETEMPLATES);
    delete generalConfig;

    // event types
    ConfigGroup* etConfig = ConfigStorage::group("EventTypes");
    int etCount = EventType::knownTypeCount();
    etConfig->setValue( "Count", etCount);
    for (int i=0; i<etCount; i++) {
	EventType* t = EventType::knownType(i);
	etConfig->setValue( QString("Name%1").arg(i+1), t->name());
	etConfig->setValue( QString("Longname%1").arg(i+1),
			    t->longName(),
			    knownLongName(t->name()) );
	etConfig->setValue( QString("Formula%1").arg(i+1),
			    t->formula(), knownFormula(t->name()) );
    }
    delete etConfig;
}

void GlobalConfig::readOptions()
{
    int i, count;

    // color options
    _colors.clear();
    // colors for default cost types:
    //  red for L2 misses, green for L1 misses, blue for normal accesses
    colorSetting("CostType-I2mr")->_color = QColor(240, 0, 0);
    colorSetting("CostType-D2mr")->_color = QColor(180,40,40);
    colorSetting("CostType-D2mw")->_color = QColor(120,80,80);

    colorSetting("CostType-I1mr")->_color = QColor(0, 240, 0);
    colorSetting("CostType-D1mr")->_color = QColor(40,180,40);
    colorSetting("CostType-D1mw")->_color = QColor(80,120,80);

    colorSetting("CostType-Ir")->_color = QColor(0, 0, 240);
    colorSetting("CostType-Dr")->_color = QColor(40,40,180);
    colorSetting("CostType-Dw")->_color = QColor(80,80,120);

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

    // source options
    ConfigGroup* sourceConfig = ConfigStorage::group("Source");
    QStringList dirs;
    dirs = sourceConfig->value("Dirs", QStringList()).toStringList();
    if (dirs.count()>0) _generalSourceDirs = dirs;
    count = sourceConfig->value("Count", 0).toInt();
    _objectSourceDirs.clear();
    for (i=1;i<=count;i++) {
	QString n = sourceConfig->value(QString("Object%1").arg(i),
					QString()).toString();
	dirs = sourceConfig->value(QString("Dirs%1").arg(i),
				   QStringList()).toStringList();

	if (n.isEmpty() || (dirs.count()==0)) continue;

	_objectSourceDirs.insert(n, dirs);
    }
    delete sourceConfig;

    // general options
    ConfigGroup* generalConfig = ConfigStorage::group("GeneralSettings");
    _showPercentage   = generalConfig->value("ShowPercentage",
					     DEFAULT_SHOWPERCENTAGE).toBool();
    _showExpanded     = generalConfig->value("ShowExpanded",
					     DEFAULT_SHOWEXPANDED).toBool();
    _showCycles       = generalConfig->value("ShowCycles",
					     DEFAULT_SHOWCYCLES).toBool();
    _cycleCut         = generalConfig->value("CycleCut",
					     DEFAULT_CYCLECUT).toDouble();
    _percentPrecision = generalConfig->value("PercentPrecision",
					     DEFAULT_PERCENTPRECISION).toInt();
    _maxSymbolLength  = generalConfig->value("MaxSymbolLength",
					     DEFAULT_MAXSYMBOLLENGTH).toInt();
    _maxSymbolCount   = generalConfig->value("MaxSymbolCount",
					     DEFAULT_MAXSYMBOLCOUNT).toInt();
    _maxListCount     = generalConfig->value("MaxListCount",
					     DEFAULT_MAXLISTCOUNT).toInt();
    _context          = generalConfig->value("Context",
					     DEFAULT_CONTEXT).toInt();
    _noCostInside     = generalConfig->value("NoCostInside",
					     DEFAULT_NOCOSTINSIDE).toInt();
    _hideTemplates    = generalConfig->value("HideTemplates",
		                             DEFAULT_HIDETEMPLATES).toBool();
    delete generalConfig;

    // event types
    if (EventType::knownTypeCount() >0) return; // already read
    ConfigGroup* etConfig = ConfigStorage::group("EventTypes");
    int etCount = etConfig->value("Count", 0).toInt();

    if (etCount == 0) {
	addDefaultTypes();
	return;
    }

    for (int i=1;i<=etCount;i++) {
	QString n = etConfig->value(QString("Name%1").arg(i),
				    QString()).toString();
	QString l = etConfig->value(QString("Longname%1").arg(i),
				    QString()).toString();
	if (l.isEmpty()) l = knownLongName(n);
	QString f = etConfig->value(QString("Formula%1").arg(i),
				    QString()).toString();
	if (f.isEmpty()) f = knownFormula(n);

	EventType::add(new EventType(n, l, f));
    }
    delete etConfig;
}

void GlobalConfig::addDefaultTypes()
{
    QString longName, formula;
    EventType* ct;
    QStringList l = knownTypes();
    for ( QStringList::const_iterator it = l.constBegin();
	  it != l.constEnd(); ++it ) {
        longName = knownLongName(*it);
        formula  = knownFormula(*it);
        ct = new EventType(*it, longName, formula);
        EventType::add(ct);
    }
}

ConfigColorSetting* GlobalConfig::groupColorSetting(CostItem* cost)
{
    QString n;

   if (!cost)
     return colorSetting(QString("default"));

   return groupColorSetting(cost->type(), cost->prettyName());
}

ConfigColorSetting* GlobalConfig::groupColorSetting(ProfileContext::Type t,
						    QString name)
{
    QString n = ProfileContext::typeName(t) + '-' + name;
    return colorSetting(n);
}

QColor GlobalConfig::groupColor(CostItem* cost)
{
   return groupColorSetting(cost)->color();
}

QColor GlobalConfig::eventTypeColor(EventType* t)
{
   QString n;

   if (!t)
     n = QString("CostType-default");
   else
     n = QString("CostType-%1").arg(t->name());

   return colorSetting(n)->color();
}

QColor GlobalConfig::functionColor(ProfileContext::Type gt,
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
        '-' + group->prettyName() +
        '-' + f->prettyName();

    ConfigColorSetting* cs = colorSetting(n, false);
    if (cs) return cs->color();
  }
  return groupColor(group);
}

ConfigColorSetting* GlobalConfig::colorSetting(const QString& n,
					       bool createNew)
{
  // predefined ?
  GlobalConfig* c = config();
  ConfigColorSetting* cs = c->_colors.value(n, 0);
  if (cs || !createNew) return cs;

  cs = new ConfigColorSetting(n);
  c->_colors.insert(n, cs);

  return cs;
}

/* Gives back a list of all Source Base Directories of Objects in
 * current trace. If a special object is given in 2nd argument,
 * put its Source Base in front.
 */
QStringList GlobalConfig::sourceDirs(TraceData* data, TraceObject* o)
{
  QStringList l = config()->_generalSourceDirs, ol, ol2;
  TraceObjectMap::Iterator oit;
  for ( oit = data->objectMap().begin();
        oit != data->objectMap().end(); ++oit ) {
    ol = config()->_objectSourceDirs[(*oit).name()];
    if (&(*oit) == o) {
      ol2 = ol;
      continue;
    }

    for(int i=0;i<ol.count();i++)
      l.prepend( ol[i] );
  }
  for(int i=0;i<ol2.count();i++)
    l.prepend( ol2[i] );

  if (0) qDebug() << "GlobalConfig::sourceDirs: " << l.join(":");

  return l;
}

bool GlobalConfig::showPercentage()
{
  return config()->_showPercentage;
}

bool GlobalConfig::showExpanded()
{
  return config()->_showExpanded;
}

bool GlobalConfig::showCycles()
{
  return config()->_showCycles;
}

bool GlobalConfig::hideTemplates()
{
  return config()->_hideTemplates;
}

void GlobalConfig::setShowPercentage(bool s)
{
  GlobalConfig* c = config();
  if (c->_showPercentage == s) return;

  c->_showPercentage = s;
}

void GlobalConfig::setShowExpanded(bool s)
{
  GlobalConfig* c = config();
  if (c->_showExpanded == s) return;

  c->_showExpanded = s;
}

void GlobalConfig::setShowCycles(bool s)
{
  GlobalConfig* c = config();
  if (c->_showCycles == s) return;

  c->_showCycles = s;
}

void GlobalConfig::setHideTemplates(bool s)
{
  GlobalConfig* c = config();
  if (c->_hideTemplates == s) return;

  c->_hideTemplates = s;
}

double GlobalConfig::cycleCut()
{
  return config()->_cycleCut;
}

int GlobalConfig::percentPrecision()
{
  return config()->_percentPrecision;
}

int GlobalConfig::maxSymbolLength()
{
  return config()->_maxSymbolLength;
}

QString GlobalConfig::shortenSymbol(const QString& s)
{
	if ((int)s.length() > maxSymbolLength())
		return s.left(maxSymbolLength()) + "...";
	return s;
}

int GlobalConfig::maxListCount()
{
  return config()->_maxListCount;
}

int GlobalConfig::maxSymbolCount()
{
  return config()->_maxSymbolCount;
}

int GlobalConfig::context()
{
  return config()->_context;
}

int GlobalConfig::noCostInside()
{
  return config()->_noCostInside;
}

void GlobalConfig::setPercentPrecision(int v)
{
    if ((v<1) || (v >5)) return;
    _percentPrecision = v;
}

void GlobalConfig::setMaxSymbolLength(int v)
{
    if ((v<1) || (v >1000)) return;
    _maxSymbolLength = v;
}

void GlobalConfig::setMaxSymbolCount(int v)
{
    if ((v<1) || (v >50)) return;
    _maxSymbolCount = v;
}

void GlobalConfig::setMaxListCount(int v)
{
    if ((v<1) || (v >500)) return;
    _maxListCount = v;
}

void GlobalConfig::setContext(int v)
{
    if ((v<1) || (v >500)) return;
    _context = v;
}

const QStringList& GlobalConfig::generalSourceDirs()
{
    return _generalSourceDirs;
}

QStringList GlobalConfig::objectSourceDirs(QString obj)
{
    if (_objectSourceDirs.contains(obj))
	return _objectSourceDirs[obj];
    else
	return QStringList();
}

void GlobalConfig::setGeneralSourceDirs(QStringList dirs)
{
    _generalSourceDirs = dirs;
}

void GlobalConfig::setObjectSourceDirs(QString obj, QStringList dirs)
{
    if (dirs.count() == 0)
	_objectSourceDirs.remove(obj);
    else
	_objectSourceDirs.insert(obj, dirs);
}
