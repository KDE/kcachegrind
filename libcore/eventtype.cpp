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
// EventType

QList<EventType*>* EventType::_knownTypes = 0;

EventType::EventType(const QString& name, const QString& longName,
                               const QString& formula)
{
  _name = name;
  _longName = longName;
  _formula = formula;
  _isReal = formula.isEmpty();
  _set = 0;
  _realIndex = ProfileCostArray::InvalidIndex;
  _parsed = false;
  _inParsing = false;

  for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
    _coefficient[i] = 0;
}

void EventType::setFormula(const QString& formula)
{
  _formula = formula;
  _realIndex = ProfileCostArray::InvalidIndex;
  _parsed = false;
  _isReal = false;
}

void EventType::setEventTypeSet(EventTypeSet* m)
{
  _parsed = false;
  _set = m;
}

// setting the index to ProfileCostArray::MaxRealIndex makes it a
// real type with unspecified index
void EventType::setRealIndex(int i)
{
  if (i<0 || i>ProfileCostArray::MaxRealIndex)
    i=ProfileCostArray::InvalidIndex;

  _realIndex = i;
  _formula = QString();
  _isReal = true;
}

// checks for existing types and sets coefficients
bool EventType::parseFormula()
{
  if (isReal()) return true;
  if (_parsed) return true;

  if (_inParsing) {
    qDebug("TraceEventType::parseFormula: Recursion detected.");
    return false;
  }

  if (!_set) {
    qDebug("TraceEventType::parseFormula: Container of this event type unknown!");
    return false;
  }

  _inParsing = true;

  for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
    _coefficient[i] = 0;
  _parsedFormula = QString();

  QRegExp rx( "((?:\\+|\\-)?)\\s*(\\d*)\\s*\\*?\\s*(\\w+)" );

  int factor, pos, found, matching;
  QString costName;
  EventType* eventType;

  found = 0;    // how many types are referenced in formula
  matching = 0; // how many types actually are defined in profile data
  pos = 0;
  while (1) {
    pos = rx.indexIn(_formula, pos);
    if (pos<0) break;
    pos += rx.matchedLength();
    if (rx.cap(0).isEmpty()) break;
    found++;

    //qDebug("parseFormula: matched '%s','%s','%s'",
    //       qPrintable(rx.cap(1)), qPrintable(rx.cap(2)), qPrintable(rx.cap(3)));

    costName = rx.cap(3);
    eventType = _set->type(costName);
    if (!eventType) {
        //qDebug("Cost type '%s': In formula cost '%s' unknown.",
        //     qPrintable(_name), qPrintable(costName));
        continue;
    }

    factor = (rx.cap(2).isEmpty()) ? 1 : rx.cap(2).toInt();
    if (rx.cap(1) == "-") factor = -factor;
    if (factor == 0) continue;

    matching++;

    if (!_parsedFormula.isEmpty()) {
        _parsedFormula += QString(" %1 ").arg((factor>0) ? '+':'-');
    }
    else if (factor<0)
        _parsedFormula += "- ";
    if ((factor!=-1) && (factor!=1))
        _parsedFormula += QString::number( (factor>0)?factor:-factor ) + ' ';
    _parsedFormula += costName;

    if (eventType->isReal())
      _coefficient[eventType->realIndex()] += factor;
    else {
      eventType->parseFormula();
      for (int i=0; i<ProfileCostArray::MaxRealIndex;i++)
        _coefficient[i] += factor * eventType->_coefficient[i];
    }
  }

  _inParsing = false;
  if (found == 0) {
      // empty formula
      _parsedFormula = QString("0");
      _parsed = true;
      return true;
  }
  if (matching>0) {
      _parsed = true;
      return true;
  }
  return false;
}


QString EventType::parsedFormula()
{
    if (isReal()) return QString();

    parseFormula();
    return _parsedFormula;
}

QString EventType::parsedRealFormula()
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

    EventType* t = _set->type(i);
    if (!t) continue;

    if (!t->name().isEmpty())
      res += QString(" * %1").arg(t->name());
  }

  return res;
}

SubCost EventType::subCost(ProfileCostArray* c)
{
  if (_realIndex != ProfileCostArray::InvalidIndex)
    return c->subCost(_realIndex);

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }
  SubCost res = 0;

  int rc = _set->realCount();
  for (int i = 0;i<rc;i++)
    if (_coefficient[i] != 0)
      res += _coefficient[i] * c->subCost(i);

  return res;
}

int EventType::histCost(ProfileCostArray* c, double total, double* hist)
{
  if (total == 0.0) return 0;

  if (!_parsed) {
    if (!parseFormula()) return 0;
  }

  int rc = _set->realCount();
  for (int i = 0;i<rc;i++) {
    if (_coefficient[i] != 0)
      hist[i] = _coefficient[i] * c->subCost(i) / total;
    else
      hist[i] = 0.0;
  }

  return rc;
}




EventType* EventType::knownRealType(const QString& n)
{
  if (!_knownTypes) return 0;

  foreach (EventType* t, *_knownTypes)
    if (t->isReal() && (t->name() == n)) {
      EventType* type = new EventType(*t);
      return type;
    }

  return 0;
}

EventType* EventType::knownDerivedType(const QString& n)
{
  if (!_knownTypes) return 0;

  foreach (EventType* t, *_knownTypes)
    if (!t->isReal() && (t->name() == n)) {
      EventType* type = new EventType(*t);
      return type;
    }

  return 0;
}

// we take ownership
void EventType::add(EventType* t, bool overwriteExisting)
{
  if (!t) return;

  t->setEventTypeSet(0);

  if (!_knownTypes)
    _knownTypes = new QList<EventType*>;

  /* Already known? */
  foreach (EventType* kt, *_knownTypes)
      if (kt->name() == t->name()) {
          if (overwriteExisting) {
              // Overwrite old type
              if (!t->longName().isEmpty() && (t->longName() != t->name()))
                  kt->setLongName(t->longName());
              if (!t->formula().isEmpty())
                  kt->setFormula(t->formula());
          }
	  delete t;
	  return;
      }

  if (t->longName().isEmpty()) t->setLongName(t->name());
  _knownTypes->append(t);
}


int EventType::knownTypeCount()
{
  if (!_knownTypes) return 0;

  return _knownTypes->count();
}

bool EventType::remove(const QString& n)
{
  if (!_knownTypes) return false;

  foreach (EventType* t, *_knownTypes)
    if (!t->isReal() && (t->name() == n)) {
      _knownTypes->removeAll(t);
      delete t;
      return true;
    }

  return false;
}

EventType* EventType::knownType(int i)
{
  if (!_knownTypes) return 0;
  if (i<0 || i>=(int)_knownTypes->count()) return 0;

  return _knownTypes->at(i);
}


//---------------------------------------------------
// EventTypeSet

EventTypeSet::EventTypeSet()
{
  _realCount = 0;
  _derivedCount = 0;
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++) _real[i] = 0;
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++) _derived[i] = 0;
}

EventTypeSet::~EventTypeSet()
{
  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++)
    if (_real[i]) delete _real[i];

  for (int i=0;i<ProfileCostArray::MaxRealIndex;i++)
    if (_derived[i]) delete _derived[i];
}

EventTypeMapping* EventTypeSet::createMapping(const QString& types)
{
  // first check if there is enough space in the set
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

  if (newCount+_realCount > ProfileCostArray::MaxRealIndex) {
    qDebug() << "EventTypeSet::createMapping: No space for "
             << newCount << " cost entries.";
    qDebug() << "Increase MaxRealIndexValue in libcore/costitem.h and recompile.";
    return 0;
  }

  EventTypeMapping* mapping = new EventTypeMapping(this);

  pos = 0;
  while (1) {
    // skip space
    while((pos<len) && types[pos].isSpace()) pos++;

    pos2 = pos;
    while((pos2<len) && !types[pos2].isSpace()) pos2++;
    if (pos2 == pos) break;

    mapping->append(addReal(types.mid(pos,pos2-pos)));

    pos = pos2;
  }

  return mapping;
}

int EventTypeSet::addReal(const QString& t)
{
  int index = realIndex(t);
  if (index>=0) return index;

  EventType* ct = EventType::knownRealType(t);
  if (!ct) ct = new EventType(t, t);

  // make it real
  ct->setRealIndex();

  return add(ct);
}

// add an event type to a set
// this transfers ownership of the type!
int EventTypeSet::add(EventType* et)
{
  if (!et) return ProfileCostArray::InvalidIndex;

  et->setEventTypeSet(this);

  if (et->isReal()) {
    if (_realCount >= ProfileCostArray::MaxRealIndex) {
      qDebug("WARNING: Maximum for real event types reached (on adding '%s')",
             qPrintable(et->name()));
      return ProfileCostArray::InvalidIndex;
    }
    _real[_realCount] = et;
    et->setRealIndex(_realCount);

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
bool EventTypeSet::remove(EventType* t)
{
  if (!t) return false;
  if (t->set() != this) return false;

  // do not delete real types
  if (t->isReal()) return false;

  int i;
  for(i=0;i<_derivedCount;i++)
    if (_derived[i] == t) break;

  // not found?
  if (i == _derivedCount) return false;

  // delete known type with same name
  EventType::remove(t->name());

  // delete this type
  _derived[i] = 0;
  delete t;
  if (i+1 == _derivedCount) {
    // we can reuse the last index
    _derivedCount--;
  }
  return true;
}


EventType* EventTypeSet::realType(int t)
{
  if (t<0 || t>=_realCount) return 0;
  return _real[t];
}

EventType* EventTypeSet::derivedType(int t)
{
  if (t<0 || t>=_derivedCount) return 0;
  return _derived[t];
}


EventType* EventTypeSet::type(int t)
{
  if (t<0) return 0;
  if (t<_realCount) return _real[t];

  t -= ProfileCostArray::MaxRealIndex;
  if (t<0) return 0;
  if (t<_derivedCount) return _derived[t];

  return 0;
}

EventType* EventTypeSet::type(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return _real[i];

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->name() == name))
      return _derived[i];

  return 0;
}

EventType* EventTypeSet::typeForLong(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->longName() == name))
      return _real[i];

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->longName() == name))
      return _derived[i];

  return 0;
}


int EventTypeSet::realIndex(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  return ProfileCostArray::InvalidIndex;
}

int EventTypeSet::index(const QString& name)
{
  for (int i=0;i<_realCount;i++)
    if (_real[i] && (_real[i]->name() == name))
      return i;

  for (int i=0;i<_derivedCount;i++)
    if (_derived[i] && (_derived[i]->name() == name))
      return ProfileCostArray::MaxRealIndex + 1 + i;

  return ProfileCostArray::InvalidIndex;
}

int EventTypeSet::addKnownDerivedTypes()
{
  int addCount = 0;
  int addDiff, i;
  int knownCount = EventType::knownTypeCount();

  while (1) {
    addDiff = 0;
    for (i=0; i<knownCount; i++) {
      EventType* t = EventType::knownType(i);
      if (t->isReal()) continue;
      if (index(t->name()) != ProfileCostArray::InvalidIndex) continue;
      t->setEventTypeSet(this);
      if (t->parseFormula()) {
        addDiff++;
        add(new EventType(t->name(), t->longName(), t->formula()));
      }
      t->setEventTypeSet(0);
    }
    if (addDiff == 0) break;
    addCount += addDiff;
  }
  return addCount;
}


//---------------------------------------------------
// EventTypeMapping

EventTypeMapping::EventTypeMapping(EventTypeSet* set)
{
  _set = set;
  clear();
}

void EventTypeMapping::clear()
{
  _count = 0;
  _isIdentity = true;
  _firstUnused = 0;
  for(int i=0;i<ProfileCostArray::MaxRealIndex;i++) {
    _realIndex[i] = ProfileCostArray::InvalidIndex;
    _nextUnused[i] = i+1;
  }
}

bool EventTypeMapping::append(const QString& type, bool create)
{
  if (!_set) return false;
  int index = create ? _set->addReal(type) : _set->realIndex(type);

  return append(index);
}

bool EventTypeMapping::append(int type)
{
  if (!_set) return false;
  if ((type<0) || (type >= _set->realCount())) return false;

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
