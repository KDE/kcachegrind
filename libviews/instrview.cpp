/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Instruction View
 */

#include "instrview.h"

#include <assert.h>

#include <QDebug>
#include <QFile>
#include <QRegExp>
#include <QProcess>
#include <Qt3Support/Q3PopupMenu>

#include "config.h"
#include "globalconfig.h"
#include "instritem.h"


// InstrView defaults

#define DEFAULT_SHOWHEXCODE true


// Helpers for parsing output of 'objdump'

static Addr parseAddr(char* buf)
{
    Addr addr;
    uint pos = 0;

    // check for instruction line: <space>* <hex address> ":" <space>*
    while(buf[pos]==' ' || buf[pos]=='\t') pos++;

    int digits = addr.set(buf + pos);
    if ((digits==0) || (buf[pos+digits] != ':')) return Addr(0);

    return addr;
}

/**
 * Parses a line from objdump assembly output, returning false for
 * a line without an assembly instruction. Otherwise, it sets the
 * output parameters addr, code, mnemonic, operands.
 */
static bool parseLine(const char* buf, Addr& addr,
		      QString& code, QString& mnemonic, QString& operands)
{
    uint pos, start;

    // check for instruction line: <space>* <hex address> ":" <space>*

    pos = 0;
    while(buf[pos]==' ' || buf[pos]=='\t') pos++;

    int digits = addr.set(buf + pos);
    pos += digits;
    if ((digits==0) || (buf[pos] != ':')) return false;

    // further parsing of objdump output...
    pos++;
    while(buf[pos]==' ' || buf[pos]=='\t') pos++;

    // check for hex code, pattern "xx "* (with "x" being a lower case hex digit)
    start = pos;
    while(1) {
	if (! ((buf[pos]>='0' && buf[pos]<='9') ||
	       (buf[pos]>='a' && buf[pos]<='f')) ) break;
	if (! ((buf[pos+1]>='0' && buf[pos+1]<='9') ||
	       (buf[pos+1]>='a' && buf[pos+1]<='f')) ) break;
	if (buf[pos+2] != ' ') break;
	pos += 3;
    }
    if (pos <= start) return false;
    code = QString::fromAscii(buf + start, pos - start - 1);

    // skip whitespace
    while(buf[pos]==' ' || buf[pos]=='\t') pos++;

    // check for mnemonic
    start = pos;
    while(buf[pos] && buf[pos]!=' ' && buf[pos]!='\t') pos++;
    mnemonic = QString::fromAscii(buf + start, pos - start);

    // skip whitespace
    while(buf[pos]==' '|| buf[pos]=='\t') pos++;

    // last part are the operands
    int operandsLen = strlen(buf + pos);

    // ignore a newline at end
    if ((operandsLen>0) && (buf[pos + operandsLen - 1] == '\n'))
	operandsLen--;

    // maximal 50 chars
    if (operandsLen > 50)
	operands = QString::fromAscii(buf + pos, 47) + QString("...");
    else
	operands = QString::fromAscii(buf+pos, operandsLen);

    if (0) qDebug("For 0x%s: Code '%s', Mnemonic '%s', Operands '%s'",
		  addr.toString().ascii(), code.ascii(),
		  mnemonic.ascii(), operands.ascii());

    return true;
}




//
// InstrView
//


InstrView::InstrView(TraceItemView* parentView,
                     QWidget* parent, const char* name)
  : Q3ListView(parent, name), TraceItemView(parentView)
{
  _showHexCode = DEFAULT_SHOWHEXCODE;
  _lastHexCodeWidth = 50;

  _inSelectionUpdate = false;
  _arrowLevels = 0;
  _lowList.setSortLow(true);
  _highList.setSortLow(false);

  addColumn( tr( "#" ) );
  addColumn( tr( "Cost" ) );
  addColumn( tr( "Cost 2" ) );
  addColumn( "" );
  addColumn( tr( "Hex" ) );
  addColumn( "" ); // Instruction
  addColumn( tr( "Assembler" ) );
  addColumn( tr( "Source Position" ) );

  setAllColumnsShowFocus(true);
  setColumnAlignment(1, Qt::AlignRight);
  setColumnAlignment(2, Qt::AlignRight);

  connect(this,
          SIGNAL(contextMenuRequested(Q3ListViewItem*, const QPoint &, int)),
          SLOT(context(Q3ListViewItem*, const QPoint &, int)));

  connect(this, SIGNAL(selectionChanged(Q3ListViewItem*)),
          SLOT(selectedSlot(Q3ListViewItem*)));

  connect(this,
          SIGNAL(doubleClicked(Q3ListViewItem*)),
          SLOT(activatedSlot(Q3ListViewItem*)));

  connect(this,
          SIGNAL(returnPressed(Q3ListViewItem*)),
          SLOT(activatedSlot(Q3ListViewItem*)));

  this->setWhatsThis( whatsThis());
}

void InstrView::paintEmptyArea( QPainter * p, const QRect & r)
{
  Q3ListView::paintEmptyArea(p, r);
}

QString InstrView::whatsThis() const
{
    return tr( "<b>Annotated Assembler</b>"
		 "<p>The annotated assembler list shows the "
		 "machine code instructions of the current selected "
		 "function together with (self) cost spent while "
		 "executing an instruction. If this is a call "
		 "instruction, lines with details on the "
		 "call happening are inserted into the source: "
		 "the cost spent inside of the call, the "
		 "number of calls happening, and the call destination.</p>"
		 "<p>The disassembler output shown is generated with "
		 "the 'objdump' utility from the 'binutils' package.</p>"
		 "<p>Select a line with call information to "
		 "make the destination function of this call current.</p>");
}

void InstrView::context(Q3ListViewItem* i, const QPoint & p, int c)
{
  Q3PopupMenu popup;

  TraceInstrCall* ic = i ? ((InstrItem*) i)->instrCall() : 0;
  TraceInstrJump* ij = i ? ((InstrItem*) i)->instrJump() : 0;
  TraceFunction* f = ic ? ic->call()->called() : 0;
  TraceInstr* instr = ij ? ij->instrTo() : 0;

  if (f) {
      popup.insertItem(tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(f->prettyName())), 93);
    popup.insertSeparator();
  }
  else if (instr) {
      popup.insertItem(tr("Go to Address %1").arg(instr->name()), 93);
    popup.insertSeparator();
  }

  if ((c == 1) || (c == 2)) {
    addEventTypeMenu(&popup);
    popup.insertSeparator();
  }
  addGoMenu(&popup);

  popup.insertSeparator();
  popup.setCheckable(true);
  popup.insertItem(tr("Hex Code"), 94);
  if (_showHexCode) popup.setItemChecked(94,true);

  int r = popup.exec(p);
  if (r == 93) {
    if (f) activated(f);
    if (instr) activated(instr);
  }
  else if (r == 94) {
    _showHexCode = !_showHexCode;
    // remember width when hiding
    if (!_showHexCode)
      _lastHexCodeWidth = columnWidth(4);
    setColumnWidths();
  }
}


void InstrView::selectedSlot(Q3ListViewItem * i)
{
  if (!i) return;
  // programatically selected items are not signalled
  if (_inSelectionUpdate) return;

  TraceInstrCall* ic = ((InstrItem*) i)->instrCall();
  TraceInstrJump* ij = ((InstrItem*) i)->instrJump();

  if (!ic && !ij) {
      TraceInstr* instr = ((InstrItem*) i)->instr();
      if (instr) {
	  _selectedItem = instr;
	  selected(instr);
      }
      return;
  }

  if (ic) {
      _selectedItem = ic;
      selected(ic);
  }
  else if (ij) {
      _selectedItem = ij;
      selected(ij);
  }
}

void InstrView::activatedSlot(Q3ListViewItem * i)
{
  if (!i) return;
  TraceInstrCall* ic = ((InstrItem*) i)->instrCall();
  TraceInstrJump* ij = ((InstrItem*) i)->instrJump();

  if (!ic && !ij) {
      TraceInstr* instr = ((InstrItem*) i)->instr();
      if (instr) activated(instr);
      return;
  }

  if (ic) {
    TraceFunction* f = ic->call()->called();
    if (f) activated(f);
  }
  else if (ij) {
    TraceInstr* instr = ij->instrTo();
    if (instr) activated(instr);
  }
}


CostItem* InstrView::canShow(CostItem* i)
{
    ProfileContext::Type t = i ? i->type() : ProfileContext::InvalidType;
  TraceFunction* f = 0;

  switch(t) {
  case ProfileContext::Function:
      f = (TraceFunction*) i;
      break;

  case ProfileContext::Instr:
      f = ((TraceInstr*)i)->function();
      select(i);
      break;

  case ProfileContext::Line:
      f = ((TraceLine*)i)->functionSource()->function();
      select(i);
      break;

  default:
      break;
  }

  return f;
}


void InstrView::doUpdate(int changeType)
{
  // Special case ?
  if (changeType == selectedItemChanged) {

      if (!_selectedItem) {
	  clearSelection();
	  return;
      }

      InstrItem *ii = (InstrItem*)Q3ListView::selectedItem();
      if (ii) {
	  if ((ii->instr() == _selectedItem) ||
	      (ii->instr() && (ii->instr()->line() == _selectedItem))) return;
      }

      Q3ListViewItem *item, *item2;
      for (item = firstChild();item;item = item->nextSibling()) {
	  ii = (InstrItem*)item;
	  if ((ii->instr() == _selectedItem) ||
	      (ii->instr() && (ii->instr()->line() == _selectedItem))) {
	      ensureItemVisible(item);
              _inSelectionUpdate = true;
	      setCurrentItem(item);
              _inSelectionUpdate = false;
	      break;
	  }
	  item2 = item->firstChild();
	  for (;item2;item2 = item2->nextSibling()) {
	      ii = (InstrItem*)item2;
	      if (!ii->instrCall()) continue;
	      if (ii->instrCall()->call()->called() == _selectedItem) {
		  ensureItemVisible(item2);
                  _inSelectionUpdate = true;
		  setCurrentItem(item2);
                  _inSelectionUpdate = false;
		  break;
	      }
	  }
	  if (item2) break;
      }
      return;
  }

  if (changeType == groupTypeChanged) {
    Q3ListViewItem *item, *item2;
    for (item = firstChild();item;item = item->nextSibling())
      for (item2 = item->firstChild();item2;item2 = item2->nextSibling())
        ((InstrItem*)item2)->updateGroup();
    return;
  }

  refresh();
}

void InstrView::setColumnWidths()
{
  if (_showHexCode) {
    setColumnWidthMode(4, Q3ListView::Maximum);
    setColumnWidth(4, _lastHexCodeWidth);
  }
  else {
    setColumnWidthMode(4, Q3ListView::Manual);
    setColumnWidth(4, 0);
  }
}

void InstrView::refresh()
{
    _arrowLevels = 0;

    // reset to automatic sizing to get column width
    setColumnWidthMode(4, Q3ListView::Maximum);

    clear();
    setColumnWidth(0, 20);
    setColumnWidth(1, 50);
    setColumnWidth(2, _eventType2 ? 50:0);
    setColumnWidth(3, 0);   // arrows, defaults to invisible
    setColumnWidth(4, 0);   // hex code column
    setColumnWidth(5, 20);  // command column
    setColumnWidth(6, 200); // arg column
    setSorting(0); // always reset to address number sort
    if (_eventType)
      setColumnText(1, _eventType->name());
    if (_eventType2)
      setColumnText(2, _eventType2->name());

    if (!_data || !_activeItem) return;

    ProfileContext::Type t = _activeItem->type();
    TraceFunction* f = 0;
    if (t == ProfileContext::Function) f = (TraceFunction*) _activeItem;
    if (t == ProfileContext::Instr) {
	f = ((TraceInstr*)_activeItem)->function();
	if (!_selectedItem) _selectedItem = _activeItem;
    }
    if (t == ProfileContext::Line) {
	f = ((TraceLine*)_activeItem)->functionSource()->function();
	if (!_selectedItem) _selectedItem = _activeItem;
    }

    if (!f) return;

    // Allow resizing of column 2
    setColumnWidthMode(2, Q3ListView::Maximum);

    // check for instruction map
    TraceInstrMap::Iterator itStart, it, tmpIt, itEnd;
    TraceInstrMap* instrMap = f->instrMap();
    if (instrMap) {
	it    = instrMap->begin();
	itEnd = instrMap->end();
	// get first instruction with cost of selected type
	while(it != itEnd) {
	  if ((*it).hasCost(_eventType)) break;
	  if (_eventType2 && (*it).hasCost(_eventType2)) break;
	  ++it;
	}
    }
    if (!instrMap || (it == itEnd)) {
	new InstrItem(this, this, 1,
		      tr("There is no instruction info in the profile data file."));
	new InstrItem(this, this, 2,
		      tr("Tip: For Callgrind, rerun with option"));
	new InstrItem(this, this, 3, tr("      --dump-instr=yes"));
	new InstrItem(this, this, 4, tr("To see (conditional) jumps, additionally specify"));
	new InstrItem(this, this, 5, tr("      --trace-jump=yes"));
	return;
    }

    // initialisation for arrow drawing
    // create sorted list of jumps (for jump arrows)
    _lowList.clear();
    _highList.clear();
    itStart = it;
    while(1) {
	TraceInstrJumpList jlist = (*it).instrJumps();
	TraceInstrJump* ij;
	for (ij=jlist.first();ij;ij=jlist.next()) {
	    if (ij->executedCount()==0) continue;
	    _lowList.append(ij);
	    _highList.append(ij);
	}
	++it;
	while(it != itEnd) {
	  if ((*it).hasCost(_eventType)) break;
	  if (_eventType2 && (*it).hasCost(_eventType2)) break;
	  ++it;
	}
	if (it == itEnd) break;
    }
    _lowList.sort();
    _highList.sort();
    _lowList.first(); // iterators to list start
    _highList.first();
    _arrowLevels = 0;
    _jump.resize(0);


    // do multiple calls to 'objdump' if there are large gaps in addresses
    it = itStart;
    while(1) {
	itStart = it;
	while(1) {
	    tmpIt = it;
	    ++it;
	    while(it != itEnd) {
	      if ((*it).hasCost(_eventType)) break;
	      if (_eventType2 && (*it).hasCost(_eventType2)) break;
	      ++it;
	    }
	    if (it == itEnd) break;
	    if (!(*it).addr().isInRange( (*tmpIt).addr(),10000) ) break;
	}

	// tmpIt is always last instruction with cost
	if (!fillInstrRange(f, itStart, ++tmpIt)) break;
	if (it == itEnd) break;
    }

    _lastHexCodeWidth = columnWidth(4);
    setColumnWidths();

    if (!_eventType2) {
      setColumnWidthMode(2, Q3ListView::Manual);
      setColumnWidth(2, 0);
    }
}

/* This is called after adding instrItems, for each of them in
 * address order. _jump is the global array of valid jumps
 * for a line while we iterate downwards.
 * The existing jumps, sorted in lowList according lower address,
 * is iterated in the same way.
 */
void InstrView::updateJumpArray(Addr addr, InstrItem* ii,
				bool ignoreFrom, bool ignoreTo)
{
    TraceInstrJump* ij;
    Addr lowAddr, highAddr;
    int iEnd = -1, iStart = -1;

    if (0) qDebug("updateJumpArray(addr 0x%s, jump to %s)",
		  addr.toString().ascii(),
		  ii->instrJump()
		  ? ii->instrJump()->instrTo()->name().ascii() : "?" );

    // check for new arrows starting from here downwards
    ij=_lowList.current();
    while(ij) {
	lowAddr = ij->instrFrom()->addr();
	if (ij->instrTo()->addr() < lowAddr)
	    lowAddr = ij->instrTo()->addr();

	if (lowAddr > addr) break;

	// if target is downwards but we draw no source, break
	if (ignoreFrom && (lowAddr < ij->instrTo()->addr())) break;
	// if source is downward but we draw no target, break
	if (ignoreTo && (lowAddr < ij->instrFrom()->addr())) break;
	// if this is another jump start, break
	if (ii->instrJump() && (ij != ii->instrJump())) break;

#if 0
	for(iStart=0;iStart<_arrowLevels;iStart++)
	    if (_jump[iStart] &&
		(_jump[iStart]->instrTo() == ij->instrTo())) break;
#else
	iStart = _arrowLevels;
#endif

	if (iStart==_arrowLevels) {
	    for(iStart=0;iStart<_arrowLevels;iStart++)
		if (_jump[iStart] == 0) break;
	    if (iStart==_arrowLevels) {
		_arrowLevels++;
		_jump.resize(_arrowLevels);
	    }
	    if (0) qDebug("  new start at %d for %s", iStart, ij->name().ascii());
	    _jump[iStart] = ij;
	}
	ij=_lowList.next();
    }

    ii->setJumpArray(_jump);

    // check for active arrows ending here
    ij=_highList.current();
    while(ij) {
	highAddr = ij->instrFrom()->addr();
	if (ij->instrTo()->addr() > highAddr) {
	    highAddr = ij->instrTo()->addr();
	    if (ignoreTo) break;
	}
	else if (ignoreFrom) break;

	if (highAddr > addr) break;

	for(iEnd=0;iEnd<_arrowLevels;iEnd++)
	    if (_jump[iEnd] == ij) break;
	if (iEnd==_arrowLevels) {
	  qDebug() << "InstrView: no jump start for end at 0x"
		    << highAddr.toString() << " ?";
	  iEnd = -1;
	}

	if (0 && (iEnd>=0))
	    qDebug(" end %d (%s to %s)",
		   iEnd,
		   _jump[iEnd]->instrFrom()->name().ascii(),
		   _jump[iEnd]->instrTo()->name().ascii());

	if (0 && ij) qDebug("next end: %s to %s",
			    ij->instrFrom()->name().ascii(),
			    ij->instrTo()->name().ascii());

	ij=_highList.next();
	if (highAddr > addr)
	    break;
	else {
	    if (iEnd>=0) _jump[iEnd] = 0;
	    iEnd = -1;
	}
    }
    if (iEnd>=0) _jump[iEnd] = 0;
}



/**
 * Fill up with instructions from cost range [it;itEnd[
 */
bool InstrView::fillInstrRange(TraceFunction* function,
                               TraceInstrMap::Iterator it,
                               TraceInstrMap::Iterator itEnd)
{
    Addr costAddr, nextCostAddr, objAddr, addr;
    Addr dumpStartAddr, dumpEndAddr;
    TraceInstrMap::Iterator costIt;

    // should not happen
    if (it == itEnd) return false;

    // calculate address range for call to objdump
    TraceInstrMap::Iterator tmpIt = itEnd;
    --tmpIt;
    nextCostAddr = (*it).addr();
    dumpStartAddr = (nextCostAddr<20) ? Addr(0) : nextCostAddr -20;
    dumpEndAddr   = (*tmpIt).addr() +20;

    // call objdump synchroneously
    QString objfile;
    objfile = function->object()->name();
    objfile = objfile.replace(QRegExp("[\"']"), ""); // security...
    QStringList objdumpArgs = QStringList()
	<< "-C" << "-d"
	<< QString("--start-address=0x%1").arg(dumpStartAddr.toString())
	<< QString("--stop-address=0x%1").arg(dumpEndAddr.toString())
	<< objfile;
    QString objdumpCmd = "objdump " + objdumpArgs.join(" ");

    if (1) qDebug("Running '%s'...", objdumpCmd.ascii());

    // and run...
    QProcess objdump;
    objdump.start("objdump", objdumpArgs);
    if (!objdump.waitForStarted() ||
	!objdump.waitForFinished()) {

	new InstrItem(this, this, 1,
		      tr("There is an error trying to execute the command"));
	new InstrItem(this, this, 2, "");
	new InstrItem(this, this, 3, objdumpCmd);
	new InstrItem(this, this, 4, "");
	new InstrItem(this, this, 5,
		      tr("Check that you have installed 'objdump'."));
	new InstrItem(this, this, 6,
		      tr("This utility can be found in the 'binutils' package."));
	return false;
    }


#define BUF_SIZE  256

    char buf[BUF_SIZE];
    bool inside = false, skipLineWritten = true;
    int readBytes = -1;
    int objdumpLineno = 0, dumpedLines = 0, noAssLines = 0;
    SubCost most = 0;
    TraceInstr* currInstr;
    InstrItem *ii, *ii2, *item = 0, *first = 0, *selected = 0;
    QString code, cmd, args;
    bool needObjAddr = true, needCostAddr = true;

    costAddr = 0;
    objAddr  = 0;

    while (1) {

      if (needObjAddr) {
	  needObjAddr = false;

        // read next objdump line
        while (1) {
	  readBytes=objdump.readLine(buf, BUF_SIZE);
          if (readBytes<=0) {
	    objAddr = 0;
	    break;
	  }

          objdumpLineno++;
          if (readBytes == BUF_SIZE) {
	    qDebug("ERROR: Line %d of '%s' too long\n",
                   objdumpLineno, objdumpCmd.ascii());
          }
          else if ((readBytes>0) && (buf[readBytes-1] == '\n'))
	    buf[readBytes-1] = 0;

          objAddr = parseAddr(buf);
          if ((objAddr<dumpStartAddr) || (objAddr>dumpEndAddr))
            objAddr = 0;
          if (objAddr != 0) break;
        }

        if (0) qDebug() << "Got ObjAddr: 0x" << objAddr.toString();
      }

      // try to keep objAddr in [costAddr;nextCostAddr]
      if (needCostAddr &&
	  (nextCostAddr > 0) &&
	  ((objAddr == Addr(0)) || (objAddr >= nextCostAddr)) ) {
	  needCostAddr = false;

	  costIt = it;
	  ++it;
	  while(it != itEnd) {
	    if ((*it).hasCost(_eventType)) break;
	    if (_eventType2 && (*it).hasCost(_eventType2)) break;
	    ++it;
	  }
	  costAddr = nextCostAddr;
	  nextCostAddr = (it == itEnd) ? Addr(0) : (*it).addr();

	  if (0) qDebug() << "Got nextCostAddr: 0x" << nextCostAddr.toString()
			   << ", costAddr 0x" << costAddr.toString();
      }

      // if we have no more address from objdump, stop
      if (objAddr == 0) break;

      if ((nextCostAddr==0) || (costAddr == 0) ||
	  (objAddr < nextCostAddr)) {
	  // next line is objAddr

	  // this sets addr, code, cmd, args
	  bool isAssemblyInstr = parseLine(buf, addr, code, cmd, args);
	  assert(isAssemblyInstr && (objAddr == addr));

	  if (costAddr == objAddr) {
	      currInstr = &(*costIt);
	      needCostAddr = true;
	  }
	  else
	      currInstr = 0;

	  needObjAddr = true;

	  if (0) qDebug() << "Dump Obj Addr: 0x" << addr.toString()
			   << " [" << cmd << " " << args << "], cost (0x"
			   << costAddr.toString() << ", next 0x"
			   << nextCostAddr.toString() << ")";
      }
      else {
	  addr = costAddr;
	  code = cmd = QString();
	  args = tr("(No Assembler)");

	  currInstr = &(*costIt);
	  needCostAddr = true;

	  noAssLines++;
	  if (0) qDebug() << "Dump Cost Addr: 0x" << addr.toString()
			   << " (no ass), objAddr 0x" << objAddr.toString();
      }

      // update inside
      if (!inside) {
	  if (currInstr) inside = true;
      }
      else {
	if (0) qDebug() << "Check if 0x" << addr.toString() << " is in ]0x"
			 << costAddr.toString() << ",0x"
			 << (nextCostAddr - 3*GlobalConfig::noCostInside()).toString()
			 << "[" << endl;

	  // Suppose a average instruction len of 3 bytes
	  if ( (addr > costAddr) &&
	       ((nextCostAddr==0) ||
		(addr < nextCostAddr - 3*GlobalConfig::noCostInside()) ))
	      inside = false;
      }

      int context = GlobalConfig::context();

      if ( ((costAddr==0)     || (addr > costAddr + 3*context)) &&
           ((nextCostAddr==0) || (addr < nextCostAddr - 3*context)) ) {

	  // the very last skipLine can be ommitted
	  if ((it == itEnd) &&
	      (itEnd == function->instrMap()->end())) skipLineWritten=true;

	  if (!skipLineWritten) {
	      skipLineWritten = true;
	      // a "skipping" line: print "..." instead of a line number
	      code = cmd = QString();
	      args = QString("...");
	  }
	  else
	      continue;
      }
      else
	  skipLineWritten = false;


      ii = new InstrItem(this, this, addr, inside,
                         code, cmd, args, currInstr);
      dumpedLines++;
      if (0) qDebug() << "Dumped 0x" << addr.toString() << " "
		       << (inside ? "Inside " : "Outside")
		      << (currInstr ? "Cost" : "");

      // no calls/jumps if we have no cost for this line
      if (!currInstr) continue;

      if (!selected &&
          ((currInstr == _selectedItem) ||
           (currInstr->line() == _selectedItem))) selected = ii;

      if (!first) first = ii;

      if (currInstr->subCost(_eventType) > most) {
        item = ii;
        most = currInstr->subCost(_eventType);
      }

      ii->setOpen(true);
      TraceInstrCallList list = currInstr->instrCalls();
      TraceInstrCall* ic;
      for (ic=list.first();ic;ic=list.next()) {
	  if ((ic->subCost(_eventType)==0) &&
	      (ic->subCost(_eventType2)==0)) continue;

	  if (ic->subCost(_eventType) > most) {
	      item = ii;
	      most = ic->subCost(_eventType);
	  }

	  ii2 = new InstrItem(this, ii, addr, currInstr, ic);

	  if (!selected && (ic->call()->called() == _selectedItem))
	      selected = ii2;
      }

      TraceInstrJumpList jlist = currInstr->instrJumps();
      TraceInstrJump* ij;
      for (ij=jlist.first();ij;ij=jlist.next()) {
	  if (ij->executedCount()==0) continue;

	  new InstrItem(this, ii, addr, currInstr, ij);
      }
    }

    if (selected) item = selected;
    if (item) first = item;
    if (first) {
	ensureItemVisible(first);
        _inSelectionUpdate = true;
        setCurrentItem(first);
        _inSelectionUpdate = false;
    }

    // for arrows: go down the list according to list sorting
    sort();
    Q3ListViewItem *item1, *item2;
    for (item1=firstChild();item1;item1 = item1->nextSibling()) {
	ii = (InstrItem*)item1;
	updateJumpArray(ii->addr(), ii, true, false);

	for (item2=item1->firstChild();item2;item2 = item2->nextSibling()) {
	    ii2 = (InstrItem*)item2;
	    if (ii2->instrJump())
		updateJumpArray(ii->addr(), ii2, false, true);
	    else
		ii2->setJumpArray(_jump);
	}
    }

    if (arrowLevels())
	setColumnWidth(3, 10 + 6*arrowLevels() + itemMargin() * 2);
    else
	setColumnWidth(3, 0);


    if (noAssLines > 1) {
	// trace cost not machting code

	new InstrItem(this, this, 1,
		      tr("There are %1 cost line(s) without assembler code.",
			 "", noAssLines));
	new InstrItem(this, this, 2,
		      tr("This happens because the code of"));
	new InstrItem(this, this, 3, QString("        %1").arg(objfile));
	new InstrItem(this, this, 4,
		      tr("does not seem to match the profile data file."));
	new InstrItem(this, this, 5, "");
	new InstrItem(this, this, 6,
		      tr("Are you using an old profile data file or is the above mentioned"));
	new InstrItem(this, this, 7,
		      tr("ELF object from an updated installation/another machine?"));
	new InstrItem(this, this, 8, "");
	return false;
    }

    if (dumpedLines == 0) {
	// no matching line read from popen
	new InstrItem(this, this, 1,
		      tr("There seems to be an error trying to execute the command"));
	new InstrItem(this, this, 2, "");
	new InstrItem(this, this, 3, objdumpCmd);
	new InstrItem(this, this, 4, "");
	new InstrItem(this, this, 5,
		      tr("Check that the ELF object used in the command exists."));
	new InstrItem(this, this, 6,
		      tr("Check that you have installed 'objdump'."));
	new InstrItem(this, this, 7,
		      tr("This utility can be found in the 'binutils' package."));
	return false;
    }

    return true;
}


void InstrView::updateInstrItems()
{
    InstrItem* ii;
    Q3ListViewItem* item  = firstChild();
    for (;item;item = item->nextSibling()) {
	ii = (InstrItem*)item;
	TraceInstr* instr = ii->instr();
	if (!instr) continue;

	ii->updateCost();

	Q3ListViewItem *next, *i  = ii->firstChild();
	for (;i;i = next) {
	    next = i->nextSibling();
	    ((InstrItem*)i)->updateCost();
	}
    }
}

void InstrView::restoreOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    _showHexCode  = g->value("ShowHexCode", DEFAULT_SHOWHEXCODE).toBool();
    delete g;
}

void InstrView::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue("ShowHexCode", _showHexCode, DEFAULT_SHOWHEXCODE);
    delete g;
}

#include "instrview.moc"
