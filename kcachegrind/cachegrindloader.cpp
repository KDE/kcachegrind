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

#include <qfile.h>

#include <klocale.h>
#include <kdebug.h>

#include "loader.h"
#include "tracedata.h"
#include "utils.h"
#include "fixcost.h"


#define TRACE_LOADER 0
#define USE_FIXCOST 1


/*
 * Loader for Calltree Profile data (format based on Cachegrind format).
 * See Calltree documentation for the file format.
 */

class CachegrindLoader: public Loader
{
public:
    CachegrindLoader();

    bool canLoadTrace(QString file);
    bool loadTrace(TracePart*);
    bool isPartOfTrace(QString file, TraceData*);

private:

    enum lineType { SelfCost, CallCost, BoringJump, CondJump };

    bool parsePosition(FixString& s,
		       uint& newLineno, uint& newAddr);

    int lineNo;
    TraceSubMapping* subMapping;
    TraceData* _data;

    // current position
    lineType nextLineType;
    bool hasLineInfo, hasAddrInfo;
    uint currentLineno, currentAddr;

    // current function/line
    TraceFunction* currentFunction;
    TracePartFunction* currentPartFunction;
    TraceFunctionSource* currentFunctionSource;
    TraceFile* currentFile;
    TracePartFile* currentPartFile;
    TraceObject* currentObject;
    TracePartObject* currentPartObject;
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
    uint targetLineno, targetAddr;
    SubCost jumpsFollowed, jumpsExecuted;

  // support for compressed string format
  void initStringCompression();
  TraceObject* compressedObject(const QString& name);
  TraceFile* compressedFile(const QString& name);
  TraceFunction* compressedFunction(const QString& name,
                                    TraceFile*, TraceObject*);

  QPtrVector<TraceObject> _objectVector;
  QPtrVector<TraceFile> _fileVector;
  QPtrVector<TraceFunction> _functionVector;
};



CachegrindLoader::CachegrindLoader()
  : Loader("Callgrind",
           i18n( "Import filter for Cachegrind/Callgrind generated trace files") )
{}

bool CachegrindLoader::canLoadTrace(QString name)
{
  // strip path
  int lastIndex = 0, index;
  while ( (index=name.find("/", lastIndex)) >=0)
    lastIndex = index+1;

  if (name.mid(lastIndex).startsWith("cachegrind.out")) return true;
  if (name.mid(lastIndex).startsWith("callgrind.out")) return true;

  return false;
}

Loader* createCachegrindLoader()
{
  return new CachegrindLoader();
}


/****************************************
 * Support for compressed strings
 */

void CachegrindLoader::initStringCompression()
{
  // this doesn't delete previous contained objects
  _objectVector.clear();
  _fileVector.clear();
  _functionVector.clear();

  // reset to reasonable init size. We double lengths if needed.
  _objectVector.resize(100);
  _fileVector.resize(1000);
  _functionVector.resize(10000);
}
  

TraceObject* CachegrindLoader::compressedObject(const QString& name)
{
  if ((name[0] != '(') || !name[1].isDigit()) return _data->object(name);

  // compressed format using _objectVector
  int p = name.find(')');
  if (p<2) {
    kdError() << "Loader: Invalid compressed format for ELF object:\n '" 
	      << name << "'" << endl;
    return 0;
  }
  unsigned index = name.mid(1, p-1).toInt();
  TraceObject* o = 0;
  if ((int)name.length()>p+2) {
    o = _data->object(name.mid(p+2));

    if (_objectVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      kdDebug() << " CachegrindLoader::objectVector enlarged to "
		<< newSize << endl;
#endif
      _objectVector.resize(newSize);
    }

    _objectVector.insert(index, o);
  }
  else {
    if (_objectVector.size() <= index) {
      kdError() << "Loader: Invalid compressed object index " << index
		<< ", size " << _objectVector.size() << endl;
      return 0;
    }
    o = _objectVector.at(index);
  }

  return o;
}


// Note: Cachegrind sometimes gives different IDs for same file
// (when references to same source file come from differen ELF objects)
TraceFile* CachegrindLoader::compressedFile(const QString& name)
{
  if ((name[0] != '(') || !name[1].isDigit()) return _data->file(name);

  // compressed format using _objectVector
  int p = name.find(')');
  if (p<2) {
    kdError() << "Loader: Invalid compressed format for file:\n '" 
	      << name << "'" << endl;
    return 0;
  }
  unsigned int index = name.mid(1, p-1).toUInt();
  TraceFile* f = 0;
  if ((int)name.length()>p+2) {
    f = _data->file(name.mid(p+2));

    if (_fileVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      kdDebug() << " CachegrindLoader::fileVector enlarged to "
		<< newSize << endl;
#endif
      _fileVector.resize(newSize);
    }

    _fileVector.insert(index, f);
  }
  else {
    if (_fileVector.size() <= index) {
      kdError() << "Loader: Invalid compressed file index " << index
		<< ", size " << _fileVector.size() << endl;
      return 0;
    }
    f = _fileVector.at(index);
  }

  return f;
}

TraceFunction* CachegrindLoader::compressedFunction(const QString& name,
						    TraceFile* file,
						    TraceObject* object)
{
  if ((name[0] != '(') || !name[1].isDigit())
    return _data->function(name, file, object);

  // compressed format using _functionVector
  int p = name.find(')');
  if (p<2) {
    kdError() << "Loader: Invalid compressed format for function:\n '" 
	      << name << "'" << endl;
    return 0;
  }

  // Note: Cachegrind gives different IDs even for same function
  // when parts of the function are from different source files.
  // Thus, many indexes can map to same function!
  unsigned int index = name.mid(1, p-1).toUInt();
  TraceFunction* f = 0;
  if ((int)name.length()>p+2) {
    f = _data->function(name.mid(p+2), file, object);

    if (_functionVector.size() <= index) {
      int newSize = index * 2;
#if TRACE_LOADER
      kdDebug() << " CachegrindLoader::functionVector enlarged to "
		<< newSize << endl;
#endif
      _functionVector.resize(newSize);
    }

    _functionVector.insert(index, f);

#if TRACE_LOADER
    kdDebug() << "compressedFunction: Inserted at Index " << index
	      << "\n  " << f->fullName()
	      << "\n  in " << f->cls()->fullName()
	      << "\n  in " << f->file()->fullName()
	      << "\n  in " << f->object()->fullName() << endl;
#endif
  }
  else {
    if (_functionVector.size() <= index) {
      kdError() << "Loader: Invalid compressed function index " << index
		<< ", size " << _functionVector.size() << endl;
      return 0;
    }
    f = _functionVector.at(index);

    if (!f->object() && object) {
      f->setObject(object);
      object->addFunction(f);
    }
    else if (f->object() != object) {
      kdError() << "TraceData::compressedFunction: Object mismatch\n  "
		<< f->info() 
		<< "\n  Found: " << f->object()->name()
		<< "\n  Given: " << object->name() << endl;
    }
  }

  return f;
}




/**********************************************************
 * Loader
 */

/**
 * Return false if this is no position specification
 */
bool CachegrindLoader::parsePosition(FixString& line,
				     uint& newLineno, uint& newAddr)
{
    uint *newPos, *lastPos, diff;
    char c;

    if (hasAddrInfo) {
	newPos  = &newAddr;
	lastPos = &currentAddr;
    }
    else {
	newPos  = &newLineno;
	lastPos = &currentLineno;
    }

    if (!line.first(c)) return false;

    if (c == '*') {
	// nothing changed
	line.stripFirst(c);
	line.stripSpaces();
    }
    else if (c == '+') {
	line.stripFirst(c);
	line.stripUInt(diff);
	*newPos = *lastPos + diff;
    }
    else if (c == '-') {
	line.stripFirst(c);
	line.stripUInt(diff);
	*newPos = *lastPos - diff;
    }
    else if (c >= '0') {
	line.stripUInt(*newPos);
    }
    else return false;

    // if there's only one position spec, we are finished
    if (! (hasAddrInfo && hasLineInfo)) return true;

    // the second position must be a line
    newPos  = &newLineno;
    lastPos = &currentLineno;

    if (!line.first(c)) return false;

    if (c > '9') return false;
    else if (c == '*') {
	// nothing changed
	line.stripFirst(c);
	line.stripSpaces();
    }
    else if (c == '+') {
	line.stripFirst(c);
	line.stripUInt(diff);
	*newPos = *lastPos + diff;
    }
    else if (c == '-') {
	line.stripFirst(c);
	line.stripUInt(diff);
	*newPos = *lastPos - diff;
    }
    else if (c >= '0') {
	line.stripUInt(*newPos);
    }
    else return false;

    return true;
}





/**
 * The main import function...
 */
bool CachegrindLoader::loadTrace(TracePart* part)
{
  initStringCompression();

  QString name = part->name();
  _data = part->data();

#ifdef USE_FIXCOST
  // FixCost Memory Pool
  FixPool* pool = _data->fixPool();
#endif

  FixFile file(name);
  if (!file.exists()) {
    kdError() << "File doesn't exist\n" << endl;
    return false;
  }

  kdDebug() << "Loading " << name << " ..." << endl;

  QString statusMsg = i18n("Loading %1").arg(name);
  int statusProgress = 0;
  emit updateStatus(statusMsg,statusProgress);

  lineNo = 0;
  FixString line;
  char c;

  // current position
  nextLineType  = SelfCost;
  currentLineno =0;
  currentAddr   =0;
  // default if there's no "positions:" line
  hasLineInfo = true;
  hasAddrInfo = false;

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
  targetLineno = 0;
  targetAddr   = 0;
  jumpsFollowed = 0;
  jumpsExecuted = 0;


  subMapping = 0;

  while (file.nextLine(line)) {

      lineNo++;

#if TRACE_LOADER
      kdDebug() << "[CachegrindLoader] " << name << ":" << lineNo
		<< " - '" << QString(line) << "'" << endl;
#endif

      // if we cannot strip a character, this was an empty line
      if (!line.first(c)) continue;

      if (c <= '9') {

	  // parse position(s)
	  if (!parsePosition(line, currentLineno, currentAddr)) continue;

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

		currentFile = compressedFile(line);
		currentPartFile = currentFile->partFile(part);
		currentLine = 0;
		currentPartLine = 0;
		continue;
	    }

	    // fn=
	    if (line.stripPrefix("n=")) {

		if (!currentFile) {
		  kdError() << name << ":" << lineNo
			    << " - function '" << QString(line)
			    << "' without source" << endl;

		  currentFile = _data->file(QString("???"));
		  currentPartFile = currentFile->partFile(part);
		}
		if (!currentObject) {
		  kdWarning() << name << ":" << lineNo
			      << " - function '" << QString(line)
			      << "' without ELF object" << endl;

		  currentObject = _data->object(QString("???"));
		  currentPartObject = currentObject->partObject(part);
		}

		// creates TraceFunction/TraceClass if not already existing
		currentFunction = compressedFunction( line,
						      currentFile,
						      currentObject);
		currentPartFunction =
		    currentFunction->partFunction(part,
						  currentPartFile,
						  currentPartObject);
                currentFunctionSource = 0;
		currentLine = 0;
		currentPartLine = 0;

		// on a new function, update status
		int progress = (int)(100.0 * file.current() / file.len() +.5);
		if (progress != statusProgress) {
		    statusProgress = progress;
		    emit updateStatus(statusMsg,statusProgress);
		}

		continue;
	    }

	    break;

	case 'c':
	    // cob=
	    if (line.stripPrefix("ob=")) {
		currentCalledObject = compressedObject(line);
		currentCalledPartObject = currentCalledObject->partObject(part);
		continue;
	    }

	    // cfi=
	    if (line.stripPrefix("fi=")) {
		currentCalledFile = compressedFile(line);
		currentCalledPartFile = currentCalledFile->partFile(part);
		continue;
	    }

	    // cfn=
	    if (line.stripPrefix("fn=")) {

		if (!currentCalledObject) {
		    currentCalledObject = currentObject;
		    currentCalledPartObject = currentPartObject;
		}

		if (!currentCalledFile) {
		    // !=0 as functions needs file
		    currentCalledFile = currentFile;
		    currentCalledPartFile = currentPartFile;
		}

		currentCalledFunction = compressedFunction(line,
							   currentCalledFile,
							   currentCalledObject);
		currentCalledPartFunction =
		    currentCalledFunction->partFunction(part,
							currentCalledPartFile,
							currentCalledPartObject);
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
		QString command = QString(line).stripWhiteSpace();
		if (!_data->command().isEmpty() &&
		    _data->command() != command) {

		  kdWarning() << name << ":" << lineNo
			      << " - redefined command, was '"
			      << _data->command()
			      << "'" << endl;
		}
		_data->setCommand(command);
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
		    parsePosition(line, targetLineno, targetAddr);

		if (!valid) {
		  kdError() << name << ":" << lineNo
			    << " - invalid jcnd line" << endl;
		}
		else
		    nextLineType = CondJump;
		continue;
	    }

	    if (line.stripPrefix("ump=")) {
		bool valid;

		valid = line.stripUInt64(jumpsExecuted) &&
		    parsePosition(line, targetLineno, targetAddr);

		if (!valid) {
		  kdError() << name << ":" << lineNo
			    << " - invalid jump line" << endl;
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
		currentObject = compressedObject(line);
		currentPartObject = currentObject->partObject(part);
		currentFunction = 0;
		currentPartFunction = 0;
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
		part->setThreadID(QString(line).toInt());
		continue;
	    }

	    // trigger:
	    if (line.stripPrefix("rigger:")) {
		part->setTrigger(line);
		continue;
	    }

	    // timeframe (BB):
	    if (line.stripPrefix("imeframe (BB):")) {
		part->setTimeframe(line);
		continue;
	    }

	    break;

	case 'd':

	    // desc:
	    if (line.stripPrefix("esc:")) continue;
	    break;

	case 'e':

	    // events:
	    if (line.stripPrefix("vents:")) {
		subMapping = _data->mapping()->subMapping(line);
                part->setFixSubMapping(subMapping);
		continue;
	    }
	    break;

	case 'p':

	    // part:
	    if (line.stripPrefix("art:")) {
		part->setPartNumber(QString(line).toInt());
		continue;
	    }

	    // pid:
	    if (line.stripPrefix("id:")) {
		part->setProcessID(QString(line).toInt());
		continue;
	    }

	    // positions:
	    if (line.stripPrefix("ositions:")) {
		QString positions(line);
		hasLineInfo = (positions.find("line")>=0);
		hasAddrInfo = (positions.find("instr")>=0);
		continue;
	    }
	    break;

	case 'v':

	    // version:
	    if (line.stripPrefix("ersion:")) {
		part->setVersion(line);
		continue;
	    }
	    break;

	case 's':

	    // summary:
	    if (line.stripPrefix("ummary:")) {
		if (!subMapping) {
		  kdError() << "No event line found. Skipping '" << name << endl;
		  return false;
		}

		part->totals()->set(subMapping, line);
		continue;
	    }

	case 'r':

	    // rcalls= (deprecated)
	    if (line.stripPrefix("calls=")) {
		// handle like normal calls: we need the sum of call count
		// recursive cost is discarded in cycle detection
		line.stripUInt64(currentCallCount);
		nextLineType = CallCost;

		kdDebug() << "WARNING: This trace dump was generated by an old "
		  "version\n of the call-tree skin. Use a new one!" << endl;

		continue;
	    }
	    break;

	default:
	    break;
	}

	kdError() << name << ":" << lineNo
		  << "- invalid line '" << c << QString(line) << "'" << endl;
	continue;
      }

      if (!subMapping) {
	kdError() << "No event line found. Skipping '" << name << "'" << endl;
	return false;
      }

      // for a cost line, we always need a current function
      if (!currentFunction) {
	kdError() << name << ":" << lineNo
		  << "- no currentFunction, using ???:???" << endl;

	currentFunction = _data->function("???", 0, 0);
	currentPartFunction = currentFunction->partFunction(part, 0, 0);
      }

#ifdef USE_FIXCOST
      if (!currentFunctionSource ||
	  (currentFunctionSource->file() != currentFile))
	  currentFunctionSource = currentFunction->sourceFile(currentFile,
							      true);
#else
      if (hasAddrInfo) {
	  if (!currentInstr || (currentInstr->address() != currentAddr)) {
	      currentInstr = currentFunction->instr(currentAddr, true);

	      if (!currentInstr) {
		kdError() << name << ":" << lineNo
			  << "- invalid address " << currentAddr << endl;

		continue;
	      }

	      currentPartInstr = currentInstr->partInstr(part,
							 currentPartFunction);
	  }
      }

      if (hasLineInfo) {
	  if (!currentLine || (currentLine->lineno() != currentLineno)) {
	      currentLine = currentFunction->line(currentFile,
						  currentLineno, true);
	      currentPartLine = currentLine->partLine(part,
						      currentPartFunction);
	  }
	  if (hasAddrInfo && currentInstr)
	      currentInstr->setLine(currentLine);
      }
#endif

#if TRACE_LOADER
      kdDebug() << name << ":" << lineNo
		<< endl << "  currentInstr "
		<< (currentInstr ? currentInstr->toString().ascii() : ".")
		<< endl << "  currentLine "
		<< (currentLine ? currentLine->toString().ascii() : ".")
		<< "( file " << currentFile->name() << ")"
		<< endl << "  currentFunction "
		<< currentFunction->prettyName().ascii()
		<< endl << "  currentCalled "
		<< (currentCalledFunction ? currentCalledFunction->prettyName().ascii() : ".")
		<< endl;
#endif

    // create cost item

    if (nextLineType == SelfCost) {

#ifdef USE_FIXCOST
      new (pool) FixCost(part, pool,
			 currentFunctionSource,
                         hasLineInfo ? currentLineno : 0,
                         hasAddrInfo ? currentAddr : 0,
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

	      partInstr->addCost(subMapping, line);
	      line.set(s,l);
	  }
	  else
	      partInstr->addCost(subMapping, line);
      }

      if (hasLineInfo) {
	  TracePartLine* partLine;
	  partLine = currentLine->partLine(part, currentPartFunction);
	  partLine->addCost(subMapping, line);
      }
#endif
    }
    else if (nextLineType == CallCost) {
      nextLineType = SelfCost;

      TraceCall* calling = currentFunction->calling(currentCalledFunction);
      TracePartCall* partCalling =
        calling->partCall(part, currentPartFunction,
                          currentCalledPartFunction);

#ifdef USE_FIXCOST
      FixCallCost* fcc;
      fcc = new (pool) FixCallCost(part, pool,
				   currentFunctionSource,
				   hasLineInfo ? currentLineno : 0,
				   hasAddrInfo ? currentAddr : 0,
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

	      partInstrCall->addCost(subMapping, line);
	      line.set(s,l);
	  }
	  else
	      partInstrCall->addCost(subMapping, line);
      }

      if (hasLineInfo) {
	  TraceLineCall* lineCall;
	  TracePartLineCall* partLineCall;

	  lineCall = calling->lineCall(currentLine);
	  partLineCall = lineCall->partLineCall(part, partCalling);

	  partLineCall->addCallCount(currentCallCount);
	  partLineCall->addCost(subMapping, line);
      }
#endif
      currentCalledFile = 0;
      currentCalledPartFile = 0;
      currentCalledObject = 0;
      currentCalledPartObject = 0;
      currentCallCount = 0;
    }
    else { // (nextLineType == BoringJump || nextLineType == CondJump)

	TraceFunctionSource* targetSource;

	if (!currentJumpToFunction)
	    currentJumpToFunction = currentFunction;

	targetSource = (currentJumpToFile) ?
	    currentJumpToFunction->sourceFile(currentJumpToFile) :
	    currentFunctionSource;

#ifdef USE_FIXCOST
      new (pool) FixJump(part, pool,
			 /* source */
			 hasLineInfo ? currentLineno : 0,
			 hasAddrInfo ? currentAddr : 0,
			 currentPartFunction,
			 currentFunctionSource,
			 /* target */
			 hasLineInfo ? targetLineno : 0,
			 hasAddrInfo ? targetAddr : 0,
			 currentJumpToFunction,
			 targetSource,
			 (nextLineType == CondJump),
			 jumpsExecuted, jumpsFollowed);
#endif

      if (0) {
	kdDebug() << name << ":" << lineNo
		  << " - jump from 0x" << QString::number(currentAddr, 16)
		  << " (line " << currentLineno
		  << ") to 0x" << QString::number(targetAddr, 16)
		  << " (line " << targetLineno << ")" << endl;

	if (nextLineType == BoringJump)
	  kdDebug() << " Boring Jump, count " << jumpsExecuted.pretty() << endl;
	else
	  kdDebug() << " Cond. Jump, followed " << jumpsFollowed.pretty()
		    << ", executed " << jumpsExecuted.pretty() << endl;
      }

      nextLineType = SelfCost;
      currentJumpToFunction = 0;
      currentJumpToFile = 0;
    }
  }


  emit updateStatus(statusMsg,100);

  part->invalidate();

  return true;
}

