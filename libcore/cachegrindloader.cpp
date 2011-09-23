/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2010 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "loader.h"

#include <QIODevice>
#include <QVector>
#include <QDebug>

#include "addr.h"
#include "tracedata.h"
#include "utils.h"
#include "fixcost.h"


#define TRACE_LOADER 0

/*
 * Loader for Callgrind Profile data (format based on Cachegrind format).
 * See Callgrind documentation for the file format.
 */

class CachegrindLoader: public Loader
{
public:
  CachegrindLoader();

  bool canLoad(QIODevice* file);
  int  load(TraceData*, QIODevice* file, const QString& filename);

private:
  void error(QString);
  void warning(QString);

  int loadInternal(TraceData*, QIODevice* file, const QString& filename);

  enum lineType { SelfCost, CallCost, BoringJump, CondJump };

  bool parsePosition(FixString& s, PositionSpec& newPos);

  // position setters
  void clearPosition();
  void ensureObject();
  void ensureFile();
  void ensureFunction();
  void setObject(const QString&);
  void setCalledObject(const QString&);
  void setFile(const QString&);
  void setCalledFile(const QString&);
  void setFunction(const QString&);
  void setCalledFunction(const QString&);

  void prepareNewPart();

  QString _emptyString;

  // current line in file to read in
  QString _filename;
  int _lineNo;

  EventTypeMapping* mapping;
  TraceData* _data;
  TracePart* _part;
  int partsAdded;

  // current position
  lineType nextLineType;
  bool hasLineInfo, hasAddrInfo;
  PositionSpec currentPos;

    // current function/line
  TraceObject* currentObject;
  TracePartObject* currentPartObject;
  TraceFile* currentFile;
  TracePartFile* currentPartFile;
  TraceFunction* currentFunction;
  TracePartFunction* currentPartFunction;
  TraceFunctionSource* currentFunctionSource;
  TraceInstr* currentInstr;
  TracePartInstr* currentPartInstr;
  TraceLine* currentLine;
  TracePartLine* currentPartLine;

  // current call
  TraceObject* currentCalledObject;
  TracePartObject* currentCalledPartObject;
  TraceFile* currentCalledFile;
  TracePartFile* currentCalledPartFile;
  TraceFunction* currentCalledFunction;
  TracePartFunction* currentCalledPartFunction;
  SubCost currentCallCount;

  // current jump
  TraceFile* currentJumpToFile;
  TraceFunction* currentJumpToFunction;
  PositionSpec targetPos;
  SubCost jumpsFollowed, jumpsExecuted;

  /** Support for compressed string format
   * This uses the following string compression model
   * for objects, files, functions:
   * If the name matches
   *   "(<Integer>) Name": this is a compression specification,
   *                       mapping the integer number to Name and using Name.
   *   "(<Integer>)"     : this is a compression reference.
   *                       Assumes previous compression specification of the
   *                       integer number to a name, uses this name.
   *   "Name"            : Regular name
   */
  void clearCompression();
  const QString& checkUnknown(const QString& n);
  TraceObject* compressedObject(const QString& name);
  TraceFile* compressedFile(const QString& name);
  TraceFunction* compressedFunction(const QString& name,
                                    TraceFile*, TraceObject*);

  QVector<TraceCostItem*> _objectVector, _fileVector, _functionVector;
};



/**********************************************************
 * Loader
 */


CachegrindLoader::CachegrindLoader()
  : Loader("Callgrind",
           QObject::tr( "Import filter for Cachegrind/Callgrind generated profile data files") )
{
    _emptyString = QString("");
}

bool CachegrindLoader::canLoad(QIODevice* file)
{
  if (!file) return false;

  Q_ASSERT(file->isOpen());

  /*
   * We recognize this as cachegrind/callgrind format if in the first
   * 2047 bytes we see the string "\nevents:"
   */
  char buf[2048];
  int read = file->read(buf,2047);
  if (read < 0)
	return false;
  buf[read] = 0;

  QByteArray s = QByteArray::fromRawData(buf, read+1);
  int pos = s.indexOf("events:");
  if (pos>0 && buf[pos-1] != '\n') pos = -1;
  return (pos>=0);
}

int CachegrindLoader::load(TraceData* d,
                           QIODevice* file, const QString& filename)
{
  /* do the loading in a new object so parallel load
   * operations do not interfere each other.
   */
  CachegrindLoader l;

  l.setLogger(_logger);

  return l.loadInternal(d, file, filename);
}

Loader* createCachegrindLoader()
{
  return new CachegrindLoader();
}

void CachegrindLoader::error(QString msg)
{
        loadError(_lineNo, msg);
}

void CachegrindLoader::warning(QString msg)
{
        loadWarning(_lineNo, msg);
}

/**
 * Return false if this is no position specification
 */
bool CachegrindLoader::parsePosition(FixString& line,
				     PositionSpec& newPos)
{
    char c;
    uint diff;

    if (hasAddrInfo) {

      if (!line.first(c)) return false;

      if (c == '*') {
	// nothing changed
	line.stripFirst(c);
	newPos.fromAddr = currentPos.fromAddr;
	newPos.toAddr = currentPos.toAddr;
      }
      else if (c == '+') {
	line.stripFirst(c);
	line.stripUInt(diff, false);
	newPos.fromAddr = currentPos.fromAddr + diff;
	newPos.toAddr = newPos.fromAddr;
      }
      else if (c == '-') {
	line.stripFirst(c);
	line.stripUInt(diff, false);
	newPos.fromAddr = currentPos.fromAddr - diff;
	newPos.toAddr = newPos.fromAddr;
      }
      else if (c >= '0') {
	uint64 v;
	line.stripUInt64(v, false);
	newPos.fromAddr = Addr(v);
	newPos.toAddr = newPos.fromAddr;
      }
      else return false;

      // Range specification
      if (line.first(c)) {
	if (c == '+') {
	  line.stripFirst(c);
	  line.stripUInt(diff);
	  newPos.toAddr = newPos.fromAddr + diff;
	}
	else if ((c == '-') || (c == ':')) {
	  line.stripFirst(c);
	  uint64 v;
	  line.stripUInt64(v);
	  newPos.toAddr = Addr(v);
	}
      }
      line.stripSpaces();

#if TRACE_LOADER
      if (newPos.fromAddr == newPos.toAddr)
	qDebug() << " Got Addr " << newPos.fromAddr.toString();
      else
	qDebug() << " Got AddrRange " << newPos.fromAddr.toString()
		  << ":" << newPos.toAddr.toString();
#endif

    }

    if (hasLineInfo) {

      if (!line.first(c)) return false;

      if (c > '9') return false;
      else if (c == '*') {
	// nothing changed
	line.stripFirst(c);
	newPos.fromLine = currentPos.fromLine;
	newPos.toLine   = currentPos.toLine;
      }
      else if (c == '+') {
	line.stripFirst(c);
	line.stripUInt(diff, false);
	newPos.fromLine = currentPos.fromLine + diff;
	newPos.toLine = newPos.fromLine;
      }
      else if (c == '-') {
	line.stripFirst(c);
	line.stripUInt(diff, false);
	if (currentPos.fromLine < diff) {
		error(QString("Negative line number %1")
		      .arg((int)currentPos.fromLine - (int)diff));
	  diff = currentPos.fromLine;
	}
	newPos.fromLine = currentPos.fromLine - diff;
	newPos.toLine = newPos.fromLine;
      }
      else if (c >= '0') {
	line.stripUInt(newPos.fromLine, false);
	newPos.toLine = newPos.fromLine;
      }
      else return false;

      // Range specification
      if (line.first(c)) {
	if (c == '+') {
	  line.stripFirst(c);
	  line.stripUInt(diff);
	  newPos.toLine = newPos.fromLine + diff;
	}
	else if ((c == '-') || (c == ':')) {
	  line.stripFirst(c);
	  line.stripUInt(newPos.toLine);
	}
      }
      line.stripSpaces();

#if TRACE_LOADER
      if (newPos.fromLine == newPos.toLine)
	qDebug() << " Got Line " << newPos.fromLine;
      else
	qDebug() << " Got LineRange " << newPos.fromLine
		  << ":" << newPos.toLine;
#endif

    }

    return true;
}

// Support for compressed strings
void CachegrindLoader::clearCompression()
{
  // this does not delete previous contained objects
  _objectVector.clear();
  _fileVector.clear();
  _functionVector.clear();

  // reset to reasonable init size. We double lengths if needed.
  _objectVector.resize(100);
  _fileVector.resize(1000);
  _functionVector.resize(10000);
}

const QString& CachegrindLoader::checkUnknown(const QString& n)
{
    if (n == "???") return _emptyString;
    return n;
}

TraceObject* CachegrindLoader::compressedObject(const QString& name)
{
  if ((name[0] != '(') || !name[1].isDigit()) return _data->object(checkUnknown(name));

  // compressed format using _objectVector
  int p = name.indexOf(')');
  if (p<2) {
	  error(QString("Invalid compressed ELF object ('%1')").arg(name));
	  return 0;
  }
  int index = name.mid(1, p-1).toInt();
  TraceObject* o = 0;
  p++;
  while((name.length()>p) && name.at(p).isSpace()) p++;
  if (name.length()>p) {
    if (_objectVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      qDebug() << " CachegrindLoader: objectVector enlarged to "
		<< newSize;
#endif
      _objectVector.resize(newSize);
    }

    QString realName = checkUnknown(name.mid(p));
    o = (TraceObject*) _objectVector.at(index);
    if (o && (o->name() != realName)) {
      error(QString("Redefinition of compressed ELF object index %1 (was '%2') to %3")
            .arg(index).arg(o->name()).arg(realName));
    }

    o = _data->object(realName);
    _objectVector.replace(index, o);
  }
  else {
    if ((_objectVector.size() <= index) ||
	( (o=(TraceObject*)_objectVector.at(index)) == 0)) {
      error(QString("Undefined compressed ELF object index %1").arg(index));
      return 0;
    }
  }

  return o;
}


// Note: Callgrind sometimes gives different IDs for same file
// (when references to same source file come from different ELF objects)
TraceFile* CachegrindLoader::compressedFile(const QString& name)
{
  if ((name[0] != '(') || !name[1].isDigit()) return _data->file(checkUnknown(name));

  // compressed format using _fileVector
  int p = name.indexOf(')');
  if (p<2) {
	  error(QString("Invalid compressed file ('%1')").arg(name));
	  return 0;
  }
  int index = name.mid(1, p-1).toUInt();
  TraceFile* f = 0;
  p++;
  while((name.length()>p) && name.at(p).isSpace()) p++;
  if (name.length()>p) {
    if (_fileVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      qDebug() << " CachegrindLoader::fileVector enlarged to "
		<< newSize;
#endif
      _fileVector.resize(newSize);
    }

    QString realName = checkUnknown(name.mid(p));
    f = (TraceFile*) _fileVector.at(index);
    if (f && (f->name() != realName)) {
      error(QString("Redefinition of compressed file index %1 (was '%2') to %3")
            .arg(index).arg(f->name()).arg(realName));
    }

    f = _data->file(realName);
    _fileVector.replace(index, f);
  }
  else {
    if ((_fileVector.size() <= index) ||
	( (f=(TraceFile*)_fileVector.at(index)) == 0)) {
      error(QString("Undefined compressed file index %1").arg(index));
      return 0;
    }
  }

  return f;
}

// Note: Callgrind gives different IDs even for same function
// when parts of the function are from different source files.
// Thus, it is no error when multiple indexes map to same function.
TraceFunction* CachegrindLoader::compressedFunction(const QString& name,
						    TraceFile* file,
						    TraceObject* object)
{
  if ((name[0] != '(') || !name[1].isDigit())
    return _data->function(checkUnknown(name), file, object);

  // compressed format using _functionVector
  int p = name.indexOf(')');
  if (p<2) {
	  error(QString("Invalid compressed function ('%1')").arg(name));
	  return 0;
  }


  int index = name.mid(1, p-1).toUInt();
  TraceFunction* f = 0;
  p++;
  while((name.length()>p) && name.at(p).isSpace()) p++;
  if (name.length()>p) {
    if (_functionVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      qDebug() << " CachegrindLoader::functionVector enlarged to "
		<< newSize;
#endif
      _functionVector.resize(newSize);
    }

    QString realName = checkUnknown(name.mid(p));
    f = (TraceFunction*) _functionVector.at(index);
    if (f && (f->name() != realName)) {
      error(QString("Redefinition of compressed function index %1 (was '%2') to %3")
            .arg(index).arg(f->name()).arg(realName));
    }

    f = _data->function(realName, file, object);
    _functionVector.replace(index, f);

#if TRACE_LOADER
    qDebug() << "compressedFunction: Inserted at Index " << index
	      << "\n  " << f->fullName()
	      << "\n  in " << f->cls()->fullName()
	      << "\n  in " << f->file()->fullName()
	      << "\n  in " << f->object()->fullName();
#endif
  }
  else {
    if ((_functionVector.size() <= index) ||
	( (f=(TraceFunction*)_functionVector.at(index)) == 0)) {
      error(QString("Undefined compressed function index %1").arg(index));
      return 0;
    }

    // there was a check if the used function (returned from KCachegrinds
    // model) has the same object and file as here given to us, but that was wrong:
    // that holds only if we make this assumption on the model...
  }


  return f;
}


// make sure that a valid object is set, at least dummy with empty name
void CachegrindLoader::ensureObject()
{
  if (currentObject) return;

  currentObject = _data->object(_emptyString);
  currentPartObject = currentObject->partObject(_part);
}

void CachegrindLoader::setObject(const QString& name)
{
  currentObject = compressedObject(name);
  if (!currentObject) {
	  error(QString("Invalid ELF object specification, setting to unknown"));

    currentObject = _data->object(_emptyString);
  }

  currentPartObject = currentObject->partObject(_part);
  currentFunction = 0;
  currentPartFunction = 0;
}

void CachegrindLoader::setCalledObject(const QString& name)
{
  currentCalledObject = compressedObject(name);

  if (!currentCalledObject) {
	  error(QString("Invalid specification of called ELF object, setting to unknown"));

    currentCalledObject = _data->object(_emptyString);
  }

  currentCalledPartObject = currentCalledObject->partObject(_part);
}


// make sure that a valid file is set, at least dummy with empty name
void CachegrindLoader::ensureFile()
{
  if (currentFile) return;

  currentFile = _data->file(_emptyString);
  currentPartFile = currentFile->partFile(_part);
}

void CachegrindLoader::setFile(const QString& name)
{
  currentFile = compressedFile(name);

  if (!currentFile) {
	  error(QString("Invalid file specification, setting to unknown"));

    currentFile = _data->file(_emptyString);
  }

  currentPartFile = currentFile->partFile(_part);
  currentLine = 0;
  currentPartLine = 0;
}

void CachegrindLoader::setCalledFile(const QString& name)
{
  currentCalledFile = compressedFile(name);

  if (!currentCalledFile) {
	  error(QString("Invalid specification of called file, setting to unknown"));

    currentCalledFile = _data->file(_emptyString);
  }

  currentCalledPartFile = currentCalledFile->partFile(_part);
}

// make sure that a valid function is set, at least dummy with empty name
void CachegrindLoader::ensureFunction()
{
  if (currentFunction) return;

  error(QString("Function not specified, setting to unknown"));

  ensureFile();
  ensureObject();

  currentFunction = _data->function(_emptyString,
				    currentFile,
				    currentObject);
  currentPartFunction = currentFunction->partFunction(_part,
						      currentPartFile,
						      currentPartObject);
}

void CachegrindLoader::setFunction(const QString& name)
{
  ensureFile();
  ensureObject();

  currentFunction = compressedFunction( name,
					currentFile,
					currentObject);

  if (!currentFunction) {
	  error(QString("Invalid function specification, setting to unknown"));

    currentFunction = _data->function(_emptyString,
				      currentFile,
				      currentObject);
  }

  currentPartFunction = currentFunction->partFunction(_part,
						      currentPartFile,
						      currentPartObject);

  currentFunctionSource = 0;
  currentLine = 0;
  currentPartLine = 0;
}

void CachegrindLoader::setCalledFunction(const QString& name)
{
  // if called object/file not set, use current object/file
  if (!currentCalledObject) {
    currentCalledObject = currentObject;
    currentCalledPartObject = currentPartObject;
  }

  if (!currentCalledFile) {
    // !=0 as functions needs file
    currentCalledFile = currentFile;
    currentCalledPartFile = currentPartFile;
  }

  currentCalledFunction = compressedFunction(name,
					     currentCalledFile,
					     currentCalledObject);
  if (!currentCalledFunction) {
    error("Invalid called function, setting to unknown");

    currentCalledFunction = _data->function(_emptyString,
					    currentCalledFile,
					    currentCalledObject);
  }

  currentCalledPartFunction =
    currentCalledFunction->partFunction(_part,
					currentCalledPartFile,
					currentCalledPartObject);
}


void CachegrindLoader::clearPosition()
{
  currentPos = PositionSpec();

  // current function/line
  currentFunction = 0;
  currentPartFunction = 0;
  currentFunctionSource = 0;
  currentFile = 0;
  currentPartFile = 0;
  currentObject = 0;
  currentPartObject = 0;
  currentLine = 0;
  currentPartLine = 0;
  currentInstr = 0;
  currentPartInstr = 0;

  // current call
  currentCalledObject = 0;
  currentCalledPartObject = 0;
  currentCalledFile = 0;
  currentCalledPartFile = 0;
  currentCalledFunction = 0;
  currentCalledPartFunction = 0;
  currentCallCount = 0;

  // current jump
  currentJumpToFile = 0;
  currentJumpToFunction = 0;
  targetPos = PositionSpec();
  jumpsFollowed = 0;
  jumpsExecuted = 0;

  mapping = 0;
}

void CachegrindLoader::prepareNewPart()
{
    if (_part) {
        // really new part needed?
        if (mapping == 0) return;

        // yes
        _part->invalidate();
        _part->totals()->clear();
        _part->totals()->addCost(_part);
        _data->addPart(_part);
        partsAdded++;
    }

    clearCompression();
    clearPosition();

    _part = new TracePart(_data);
    _part->setName(_filename);
}

/**
 * The main import function...
 */
int CachegrindLoader::loadInternal(TraceData* data,
                                   QIODevice* device, const QString& filename)
{
  if (!data || !device) return 0;

  _data = data;
  _filename = filename;
  _lineNo = 0;

  loadStart(_filename);

  FixFile file(device, _filename);
  if (!file.exists()) {
    loadFinished("File does not exist");
    return 0;
  }

  int statusProgress = 0;

#if USE_FIXCOST
  // FixCost Memory Pool
  FixPool* pool = _data->fixPool();
#endif

  _part = 0;
  partsAdded = 0;
  prepareNewPart();

  FixString line;
  char c;

  // current position
  nextLineType  = SelfCost;
  // default if there is no "positions:" line
  hasLineInfo = true;
  hasAddrInfo = false;

  while (file.nextLine(line)) {

      _lineNo++;

#if TRACE_LOADER
      qDebug() << "[CachegrindLoader] " << _filename << ":" << _lineNo
		<< " - '" << QString(line) << "'";
#endif

      // if we cannot strip a character, this was an empty line
      if (!line.first(c)) continue;

      if (c <= '9') {

	  if (c == '#') continue;

	  // parse position(s)
	  if (!parsePosition(line, currentPos)) {
		  error(QString("Invalid position specification '%1'").arg(line));
	      continue;
	  }

	  // go through after big switch
      }
      else { // if (c > '9')

	line.stripFirst(c);

	/* in order of probability */
	switch(c) {

	case 'f':

	    // fl=, fi=, fe=
	    if (line.stripPrefix("l=") ||
		line.stripPrefix("i=") ||
		line.stripPrefix("e=")) {

	      setFile(line);
	      continue;
	    }

	    // fn=
	    if (line.stripPrefix("n=")) {

	      setFunction(line);

	      // on a new function, update status
	      int progress = (int)(100.0 * file.current() / file.len() +.5);
	      if (progress != statusProgress) {
		statusProgress = progress;

		/* When this signal is connected, it most probably
		 * should lead to GUI update. Thus, when multiple
		 * "long operations" (like file loading) are in progress,
		 * this can temporarly switch to another operation.
		 */
		loadProgress(statusProgress);
	      }

	      continue;
	    }

	    break;

	case 'c':
	    // cob=
	    if (line.stripPrefix("ob=")) {
	      setCalledObject(line);
	      continue;
	    }

	    // cfi= / cfl=
	    if (line.stripPrefix("fl=") ||
		line.stripPrefix("fi=")) {
	      setCalledFile(line);
	      continue;
	    }

	    // cfn=
	    if (line.stripPrefix("fn=")) {

	      setCalledFunction(line);
	      continue;
	    }

	    // calls=
	    if (line.stripPrefix("alls=")) {
		// ignore long lines...
		line.stripUInt64(currentCallCount);
		nextLineType = CallCost;
		continue;
	    }

	    // cmd:
	    if (line.stripPrefix("md:")) {
		QString command = QString(line).trimmed();
		if (!_data->command().isEmpty() &&
		    _data->command() != command) {

		  error(QString("Redefined command, was '%1'").arg(_data->command()));
		}
		_data->setCommand(command);
		continue;
	    }

	    // creator:
	    if (line.stripPrefix("reator:")) {
		// ignore ...
	        continue;
	    }

	    break;

	case 'j':

	    // jcnd=
	    if (line.stripPrefix("cnd=")) {
		bool valid;

		valid = line.stripUInt64(jumpsFollowed) &&
		    line.stripPrefix("/") &&
		    line.stripUInt64(jumpsExecuted) &&
		    parsePosition(line, targetPos);

		if (!valid) {
			error(QString("Invalid line after 'jcnd'"));
		}
		else
		    nextLineType = CondJump;
		continue;
	    }

	    if (line.stripPrefix("ump=")) {
		bool valid;

		valid = line.stripUInt64(jumpsExecuted) &&
		    parsePosition(line, targetPos);

		if (!valid) {
			error(QString("Invalid line after 'jump'"));
		}
		else
		    nextLineType = BoringJump;
		continue;
	    }

	    // jfi=
	    if (line.stripPrefix("fi=")) {
		currentJumpToFile = compressedFile(line);
		continue;
	    }

	    // jfn=
	    if (line.stripPrefix("fn=")) {

		if (!currentJumpToFile) {
		    // !=0 as functions needs file
		    currentJumpToFile = currentFile;
		}

		currentJumpToFunction =
		    compressedFunction(line,
				       currentJumpToFile,
				       currentObject);
		continue;
	    }

	    break;

	case 'o':

	    // ob=
	    if (line.stripPrefix("b=")) {
	      setObject(line);
	      continue;
	    }

	    break;

	case '#':
	    continue;

	case 't':

	    // totals:
	    if (line.stripPrefix("otals:")) continue;

	    // thread:
	    if (line.stripPrefix("hread:")) {
                prepareNewPart();
                _part->setThreadID(QString(line).toInt());
		continue;
	    }

	    // timeframe (BB):
	    if (line.stripPrefix("imeframe (BB):")) {
                _part->setTimeframe(line);
		continue;
	    }

	    break;

	case 'd':

	    // desc:
	    if (line.stripPrefix("esc:")) {

	      line.stripSurroundingSpaces();

	      // desc: Trigger:
	      if (line.stripPrefix("Trigger:")) {
                _part->setTrigger(line);
	      }

	      continue;
	    }
	    break;

	case 'e':

		// events:
		if (line.stripPrefix("vents:")) {
                    prepareNewPart();
                    mapping = _data->eventTypes()->createMapping(line);
                    _part->setEventMapping(mapping);
                    continue;
		}

	    // event:<name>[=<formula>][:<long name>]
	    if (line.stripPrefix("vent:")) {
	      line.stripSurroundingSpaces();

	      FixString e, f, l;
	      if (!line.stripName(e)) {
				error(QString("Invalid event"));
		continue;
	      }
	      line.stripSpaces();
	      if (!line.stripFirst(c)) continue;

	      if (c=='=') f = line.stripUntil(':');
	      line.stripSpaces();

	      // add to known cost types
	      if (line.isEmpty()) line = e;
	      EventType::add(new EventType(e,line,f));
	      continue;
	    }
	    break;

	case 'p':

	    // part:
	    if (line.stripPrefix("art:")) {
                prepareNewPart();
                _part->setPartNumber(QString(line).toInt());
		continue;
	    }

	    // pid:
	    if (line.stripPrefix("id:")) {
                prepareNewPart();
                _part->setProcessID(QString(line).toInt());
		continue;
	    }

	    // positions:
	    if (line.stripPrefix("ositions:")) {
                prepareNewPart();
		QString positions(line);
		hasLineInfo = positions.contains("line");
		hasAddrInfo = positions.contains("instr");
		continue;
	    }
	    break;

	case 'v':

	    // version:
	    if (line.stripPrefix("ersion:")) {
                // ignore for now
		continue;
	    }
	    break;

	case 's':

	    // summary:
	    if (line.stripPrefix("ummary:")) {
		if (!mapping) {
			error(QString("No event line found. Skipping file"));
                        delete _part;
			return false;
		}

                _part->totals()->set(mapping, line);
		continue;
	    }

	case 'r':

	    // rcalls= (deprecated)
	    if (line.stripPrefix("calls=")) {
		// handle like normal calls: we need the sum of call count
		// recursive cost is discarded in cycle detection
		line.stripUInt64(currentCallCount);
		nextLineType = CallCost;

		warning(QString("Old file format using deprecated 'rcalls'"));
		continue;
	    }
	    break;

	default:
	    break;
	}

	error(QString("Invalid line '%1%2'").arg(c).arg(line));
	continue;
      }

      if (!mapping) {
        error(QString("No event line found. Skipping file"));
        delete _part;
        return false;
      }

      // for a cost line, we always need a current function
      ensureFunction();


#if USE_FIXCOST
      if (!currentFunctionSource ||
	  (currentFunctionSource->file() != currentFile))
	  currentFunctionSource = currentFunction->sourceFile(currentFile,
							      true);
#else
      if (hasAddrInfo) {
	  if (!currentInstr ||
	      (currentInstr->addr() != currentPos.fromAddr)) {
	      currentInstr = currentFunction->instr(currentPos.fromAddr,
						    true);

	      if (!currentInstr) {
	        error(QString("Invalid address '%1'").arg(currentPos.fromAddr.toString()));

	        continue;
	      }

	      currentPartInstr = currentInstr->partInstr(part,
							 currentPartFunction);
	  }
      }

      if (hasLineInfo) {
	  if (!currentLine ||
	      (currentLine->lineno() != currentPos.fromLine)) {

	    currentLine = currentFunction->line(currentFile,
						currentPos.fromLine,
						true);
	    currentPartLine = currentLine->partLine(part,
						    currentPartFunction);
	  }
	  if (hasAddrInfo && currentInstr)
	      currentInstr->setLine(currentLine);
      }
#endif

#if TRACE_LOADER
      qDebug() << _filename << ":" << _lineNo;
      qDebug() << "  currentInstr "
               << (currentInstr ? qPrintable(currentInstr->toString()) : ".");
      qDebug() << "  currentLine "
               << (currentLine ? qPrintable(currentLine->toString()) : ".")
	       << "( file " << currentFile->name() << ")";
      qDebug() << "  currentFunction "
               << qPrintable(currentFunction->prettyName());
      qDebug() << "  currentCalled "
               << (currentCalledFunction ? qPrintable(currentCalledFunction->prettyName()) : ".");
#endif

    // create cost item

    if (nextLineType == SelfCost) {

#if USE_FIXCOST
      new (pool) FixCost(_part, pool,
			 currentFunctionSource,
			 currentPos,
                         currentPartFunction,
                         line);
#else
      if (hasAddrInfo) {
	  TracePartInstr* partInstr;
	  partInstr = currentInstr->partInstr(part, currentPartFunction);

	  if (hasLineInfo) {
	      // we need to set <line> back after reading for the line
	      int l = line.len();
	      const char* s = line.ascii();

	      partInstr->addCost(mapping, line);
	      line.set(s,l);
	  }
	  else
	      partInstr->addCost(mapping, line);
      }

      if (hasLineInfo) {
	  TracePartLine* partLine;
	  partLine = currentLine->partLine(part, currentPartFunction);
	  partLine->addCost(mapping, line);
      }
#endif

      if (!line.isEmpty()) {
        error(QString("Garbage at end of cost line ('%1')").arg(line));
      }
    }
    else if (nextLineType == CallCost) {
      nextLineType = SelfCost;

      TraceCall* calling = currentFunction->calling(currentCalledFunction);
      TracePartCall* partCalling =
        calling->partCall(_part, currentPartFunction,
                          currentCalledPartFunction);

#if USE_FIXCOST
      FixCallCost* fcc;
      fcc = new (pool) FixCallCost(_part, pool,
				   currentFunctionSource,
				   hasLineInfo ? currentPos.fromLine : 0,
				   hasAddrInfo ? currentPos.fromAddr : Addr(0),
				   partCalling,
				   currentCallCount, line);
      fcc->setMax(_data->callMax());
#else
      if (hasAddrInfo) {
	  TraceInstrCall* instrCall;
	  TracePartInstrCall* partInstrCall;

	  instrCall = calling->instrCall(currentInstr);
	  partInstrCall = instrCall->partInstrCall(part, partCalling);
	  partInstrCall->addCallCount(currentCallCount);

	  if (hasLineInfo) {
	      // we need to set <line> back after reading for the line
	      int l = line.len();
	      const char* s = line.ascii();

	      partInstrCall->addCost(mapping, line);
	      line.set(s,l);
	  }
	  else
	      partInstrCall->addCost(mapping, line);

	  // update maximum of call cost
	  _data->callMax()->maxCost(partInstrCall);
      }

      if (hasLineInfo) {
	  TraceLineCall* lineCall;
	  TracePartLineCall* partLineCall;

	  lineCall = calling->lineCall(currentLine);
	  partLineCall = lineCall->partLineCall(part, partCalling);

	  partLineCall->addCallCount(currentCallCount);
	  partLineCall->addCost(mapping, line);

	  // update maximum of call cost
	  _data->callMax()->maxCost(partLineCall);
      }
#endif
      currentCalledFile = 0;
      currentCalledPartFile = 0;
      currentCalledObject = 0;
      currentCalledPartObject = 0;
      currentCallCount = 0;

      if (!line.isEmpty()) {
        error(QString("Garbage at end of call cost line ('%1')").arg(line));
      }
    }
    else { // (nextLineType == BoringJump || nextLineType == CondJump)

	TraceFunctionSource* targetSource;

	if (!currentJumpToFunction)
	    currentJumpToFunction = currentFunction;

	targetSource = (currentJumpToFile) ?
	    currentJumpToFunction->sourceFile(currentJumpToFile, true) :
	    currentFunctionSource;

#if USE_FIXCOST
      new (pool) FixJump(_part, pool,
			 /* source */
			 hasLineInfo ? currentPos.fromLine : 0,
			 hasAddrInfo ? currentPos.fromAddr : 0,
			 currentPartFunction,
			 currentFunctionSource,
			 /* target */
			 hasLineInfo ? targetPos.fromLine : 0,
			 hasAddrInfo ? targetPos.fromAddr : Addr(0),
			 currentJumpToFunction,
			 targetSource,
			 (nextLineType == CondJump),
			 jumpsExecuted, jumpsFollowed);
#endif

      if (0) {
	qDebug() << _filename << ":" << _lineNo
		  << " - jump from 0x" << currentPos.fromAddr.toString()
		  << " (line " << currentPos.fromLine
		  << ") to 0x" << targetPos.fromAddr.toString()
		  << " (line " << targetPos.fromLine << ")";

	if (nextLineType == BoringJump)
	  qDebug() << " Boring Jump, count " << jumpsExecuted.pretty();
	else
	  qDebug() << " Cond. Jump, followed " << jumpsFollowed.pretty()
		    << ", executed " << jumpsExecuted.pretty();
      }

      nextLineType = SelfCost;
      currentJumpToFunction = 0;
      currentJumpToFile = 0;

      if (!line.isEmpty()) {
        error(QString("Garbage at end of jump cost line ('%1')").arg(line));
      }

    }
  }

  loadFinished();

  if (mapping) {
      _part->invalidate();
      _part->totals()->clear();
      _part->totals()->addCost(_part);
      data->addPart(_part);
      partsAdded++;
  }
  else {
      delete _part;
  }

  device->close();

  return partsAdded;
}

