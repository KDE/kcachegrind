/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "eventtype.h"

#include <QRegExp>
#include <QDebug>

#include "globalconfig.h"

//---------------------------------------------------
// TraceEventType

QList<TraceEventType*>* TraceEventType::_knownTypes = 0;

TraceEventType::TraceEventType(const QString& name, const QString& longName,
                               const QString& formula)
{
  _name = name;
  _longName = longName;
  _formula = formula;
  _mapping = 0;
  _realIndex = ProfileCostArray::InvalidIndex;
  _parsed = false;
  _inParsing = false;

  for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
    _coefficient[i] = 0;
}

void TraceEventType::setFormula(const QString& formula)
{
  _formula = formula;
  _realIndex = ProfileCostArray::InvalidIndex;
  _parsed = false;
}

void TraceEventType::setMapping(TraceEventTypeMapping* m)
{
  _parsed = false;
  _mapping = m;
}

// setting the index to ProfileCostArray::MaxRealIndex makes it a
// real type with unspecified index
void TraceEventType::setRealIndex(int i)
{
  if (i<0 || i>ProfileCostArray::MaxRealIndex)
    i=ProfileCostArray::InvalidIndex;

  _realIndex = i;
  _formula = QString();
}

// checks for existing types and sets coefficients
bool TraceEventType::parseFormula()
{
  if (_parsed) return true;
  if (_inParsing) {
    qDebug("TraceEventType::parseFormula: Recursion detected.");
    return false;
  }

  if (!_mapping) {
    qDebug("TraceEventType::parseFormula: No mapping set!");
    return false;
  }

  _inParsing = true;

  for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
    _coefficient[i] = 0;

  QRegExp rx( "((?:\\+|\\-)?)\\s*(\\d*)\\s*\\*?\\s*(\\w+)" );

  int factor, pos;
  QString costName;
  TraceEventType* eventType;

  pos = 0;
  while (1) {
    pos = rx.search(_formula, pos);
    if (pos<0) break;
    pos += rx.matchedLength();
    if (rx.cap(0).isEmpty()) break;

    //qDebug("parseFormula: matched '%s','%s','%s'",
    //       qPrintable(rx.cap(1)), qPrintable(rx.cap(2)), qPrintable(rx.cap(3)));

    costName = rx.cap(3);
    eventType = _mapping->type(costName);
    if (!eventType) {
	// qDebug("Cost type '%s': In formula cost '%s' unknown.",
        //     qPrintable(_name), qPrintable(costName));

	_inParsing = false;
	return false;
    }

    factor = (rx.cap(2).isEmpty()) ? 1 : rx.cap(2).toInt();
    if (rx.cap(1) == "-") factor = -factor;

    if (eventType->isReal())
      _coefficient[eventType->realIndex()] += factor;
    else {
      eventType->parseFormula();
      for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
        _coefficient[i] += factor * eventType->_coefficient[i];
    }
  }

  _inParsing = false;
  _parsed = true;

  return true;
}

QString TraceEventType::parsedFormula()
{
  QString res;

  if (!parseFormula()) return res;

  for (int i=0; i<ProfileCostArray::MaxRealIndex;i++) {
    int c = _coefficient[i];
    if (c == 0) continue;

    if (!res.isEmpty()) {
      res += ' ';
      if (c>0) res += "+ ";
    }
    if (c<0) { res += "- "; c = -c; }
    res += QString::number(c);

    TraceEventType* t = _mapping->type(i);
    if (!t) continue;

    if (!t->name().isEmpty())
      res += QString(" * %1").arg(t->name());
  }

  return res;
}

SubCost TraceEventType::subCost(ProfileCostArray* c)
{
  if (_realIndex != ProfileCostArray::InvalidIndex)
    return c->subCost(_realIndex);

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }
  SubCost res = 0;

  int rc = _mapping->realCount();
  for (int i = 0;i<rc;i++)
    if (_coefficient[i] != 0)
      res += _coefficient[i] * c->subCost(i);

  return res;
}

int TraceEventType::histCost(ProfileCostArray* c, double total, double* hist)
{
  if (total == 0.0) return 0;

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }

  int rc = _mapping->realCount();
  for (int i = 0;i<rc;i++) {
    if (_coefficient[i] != 0)
      hist[i] = _coefficient[i] * c->subCost(i) / total;
    else
      hist[i] = 0.0;
  }

  return rc;
}




TraceEventType* TraceEventType::knownRealType(const QString& n)
{
  if (!_knownTypes) return 0;

  foreach (TraceEventType* t, *_knownTypes)
    if (t->isReal() && (t->name() == n)) {
      TraceEventType* type = new TraceEventType(*t);
      return type;
    }

  return 0;
}

TraceEventType* TraceEventType::knownDerivedType(const QString& n)
{
  if (!_knownTypes) return 0;

  foreach (TraceEventType* t, *_knownTypes)
    if (!t->isReal() && (t->name() == n)) {
      TraceEventType* type = new TraceEventType(*t);
      return type;
    }

  return 0;
}

// we take ownership
void TraceEventType::add(TraceEventType* t)
{
  if (!t) return;

  t->setMapping(0);

  if (!_knownTypes)
    _knownTypes = new QList<TraceEventType*>;

  /* Already known? */
  TraceEventType* kt = 0;
  foreach (kt, *_knownTypes)
      if (kt->name() == t->name()) {
	  // Overwrite old type
	  if (!t->longName().isEmpty() &&
	      (t->longName() != t->name())) kt->setLongName(t->longName());
	  if (!t->formula().isEmpty()) kt->setFormula(t->formula());

	  delete t;
	  return;
      }

  if (t->longName().isEmpty()) t->setLongName(t->name());
  _knownTypes->append(t);
}


int TraceEventType::knownTypeCount()
{
  if (!_knownTypes) return 0;

  return _knownTypes->count();
}

bool TraceEventType::remove(const QString& n)
{
  if (!_knownTypes) return false;

  foreach (TraceEventType* t, *_knownTypes)
    if (!t->isReal() && (t->name() == n)) {
      _knownTypes->removeAll(t);
      delete t;
      return true;
    }

  return false;
}

TraceEventType* TraceEventType::knownType(int i)
{
  if (!_knownTypes) return 0;
  if (i<0 || i>=(int)_knownTypes->count()) return 0;

  return _knownTypes->at(i);
}

QColor TraceEventType::color()
{
  if (!_mapping) return QColor();
  return _mapping->realColors()[_realIndex];
}


//---------------------------------------------------
// TraceEventTypeMapping

TraceEventTypeMapping::TraceEventTypeMapping()
{
  _realCount = 0;
  _derivedCount = 0;
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++) _real[i] = 0;
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++) _derived[i] = 0;
}

TraceEventTypeMapping::~TraceEventTypeMapping()
{
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++)
    if (_real[i]) delete _real[i];

  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++)
    if (_derived[i]) delete _derived[i];
}

TraceSubMapping* TraceEventTypeMapping::subMapping(const QString& types, bool create)
{
  // first check if there is enough space in the mapping
  int newCount = 0;
  int pos = 0, pos2, len = types.length();

  while (1) {
    // skip space
    while((pos<len) && types[pos].isSpace()) pos++;

    pos2 = pos;
    while((pos2<len) && !types[pos2].isSpace()) pos2++;
    if (pos2 == pos) break;

    if (realIndex(types.mid(pos,pos2-pos)) == ProfileCostArray::InvalidIndex)
      newCount++;

    pos = pos2;
  }

  if (!create && (newCount>0)) return 0;

  if (newCount+_realCount > ProfileCostArray::MaxRealIndex) {
    qDebug() << "TraceCostMapping::subMapping: No space for "
	      << newCount << " sub costs.";
    return 0;
  }

  TraceSubMapping* sm = new TraceSubMapping(this);

  pos = 0;
  while (1) {
    // skip space
    while((pos<len) && types[pos].isSpace()) pos++;

    pos2 = pos;
    while((pos2<len) && !types[pos2].isSpace()) pos2++;
    if (pos2 == pos) break;

    sm->append(addReal(types.mid(pos,pos2-pos)));

    pos = pos2;
  }

  return sm;
}

int TraceEventTypeMapping::addReal(const QString& t)
{
  int index = realIndex(t);
  if (index>=0) return index;

  TraceEventType* ct = TraceEventType::knownRealType(t);
  if (!ct) ct = new TraceEventType(t, t);

  // make it real
  ct->setRealIndex();

  return add(ct);
}

// add an event type to a mapping
// this transfers ownership of the type!
int TraceEventTypeMapping::add(TraceEventType* et)
{
  if (!et) return ProfileCostArray::InvalidIndex;

  et->setMapping(this);

  if (et->isReal()) {
    if (_realCount >= ProfileCostArray::MaxRealIndex) {
      qDebug("WARNING: Maximum for real event types reached (on adding '%s')",
             qPrintable(et->name()));
      return ProfileCostArray::InvalidIndex;
    }
    _real[_realCount] = et;
    et->setRealIndex(_realCount);
    _realColor[_realCount] = GlobalConfig::eventTypeColor(et);

    _realCount++;
    return _realCount-1;
  }

  if (_derivedCount >= ProfileCostArray::MaxRealIndex) {
    qDebug("WARNING: Maximum for virtual event types reached (on adding '%s')",
           qPrintable(et->name()));
    return ProfileCostArray::InvalidIndex;
  }
  _derived[_derivedCount] = et;
  _derivedCount++;
  return _derivedCount-1;
}

// we delete the type: t is invalid when returning true!
bool TraceEventTypeMapping::remove(TraceEventType* t)
{
  if (!t) return false;
  if (t->mapping() != this) return false;

  // do not delete real types
  if (t->isReal()) return false;

  int i;
  for(i=0;i<_derivedCount;i++)
    if (_derived[i] == t) break;

  // not found?
  if (i == _derivedCount) return false;

  // delete known type with same name
  TraceEventType::remove(t->name());

  // delete this type
  _derived[i] = 0;
  delete t;
  if (i+1 == _derivedCount) {
    // we can reuse the last index
    _derivedCount--;
  }
  return true;
}


TraceEventType* TraceEventTypeMapping::realType(int t)
{
  if (t<0 || t>=_realCount) return 0;
  return _real[t];
}

TraceEventType* TraceEventTypeMapping::derivedType(int t)
{
  if (t<0 || t>=_derivedCount) return 0;
  return _derived[t];
}


TraceEventType* TraceEventTypeMapping::type(int t)
{
  if (t<0) return 0;
  if (t<_realCount) return _real[t];

  t -= ProfileCostArray::MaxRealIndex;
  if (t<0) return 0;
  if (t<_derivedCount) return _derived[t];

  return 0;
}

TraceEventType* TraceEventTypeMapping::type(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return _real[i];

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->name() == name))
      return _derived[i];

  return 0;
}

TraceEventType* TraceEventTypeMapping::typeForLong(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->longName() == name))
      return _real[i];

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->longName() == name))
      return _derived[i];

  return 0;
}


int TraceEventTypeMapping::realIndex(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  return ProfileCostArray::InvalidIndex;
}

int TraceEventTypeMapping::index(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->name() == name))
      return ProfileCostArray::MaxRealIndex + 1 + i;

  return ProfileCostArray::InvalidIndex;
}

int TraceEventTypeMapping::addKnownDerivedTypes()
{
  int addCount = 0;
  int addDiff, i;
  int knownCount = TraceEventType::knownTypeCount();

  while (1) {
    addDiff = 0;
    for (i=0; i<knownCount; i++) {
      TraceEventType* t = TraceEventType::knownType(i);
      if (t->isReal()) continue;
      if (index(t->name()) != ProfileCostArray::InvalidIndex) continue;
      t->setMapping(this);
      if (t->parseFormula()) {
        addDiff++;
        add(new TraceEventType(t->name(), t->longName(), t->formula()));
      }
      t->setMapping(0);
    }
    if (addDiff == 0) break;
    addCount += addDiff;
  }
  return addCount;
}


//---------------------------------------------------
// TraceSubMapping

TraceSubMapping::TraceSubMapping(TraceEventTypeMapping* mapping)
{
  _mapping = mapping;
  clear();
}

void TraceSubMapping::clear()
{
  _count = 0;
  _isIdentity = true;
  _firstUnused = 0;
  for(int i=0;i<ProfileCostArray::MaxRealIndex;i++) {
    _realIndex[i] = ProfileCostArray::InvalidIndex;
    _nextUnused[i] = i+1;
  }
}

bool TraceSubMapping::append(const QString& type, bool create)
{
  if (!_mapping) return false;
  int index = create ? _mapping->addReal(type) : _mapping->realIndex(type);

  return append(index);
}

bool TraceSubMapping::append(int type)
{
  if (!_mapping) return false;
  if ((type<0) || (type >= _mapping->realCount())) return false;

  if ( _count >=  ProfileCostArray::MaxRealIndex) return false;

  _realIndex[_count] = type;

  if (_isIdentity && (_count != type)) _isIdentity = false;
  if (type == _firstUnused)
      _firstUnused = _nextUnused[type];
  for(int i=0;i<type;i++)
      if (_nextUnused[i] == type)
	  _nextUnused[i]=_nextUnused[type];

  _count++;
  return true;
}
