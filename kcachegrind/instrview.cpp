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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Instruction View
 */

#include <qfile.h>
#include <qregexp.h>
#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <klocale.h>

#include "configuration.h"
#include "instritem.h"
#include "instrview.h"


// Helpers for parsing output of 'objdump'

static uint parseAddr(char* buf)
{
    uint addr = 0;
    uint pos = 0;

    // check for instruction line: <space>* <hex address> ":" <space>*
    while(buf[pos]==' ' || buf[pos]=='\t') pos++;

    while(1) {
	if (buf[pos]>='0' && buf[pos]<='9')
	    addr = 16*addr + (buf[pos]-'0');
	else if (buf[pos]>='a' && buf[pos]<='f')
	    addr = 16*addr + 10 + (buf[pos]-'a');
	else break;
	pos++;
    }
    if (buf[pos] != ':') addr = 0;

    return addr;
}


static bool parseLine(char* buf, uint& addr,
                      uint& pos1, uint& pos2, uint& pos3)
{
    // check for instruction line: <space>* <hex address> ":" <space>*
    pos1 = 0;
    while(buf[pos1]==' ' || buf[pos1]=='\t') pos1++;

    addr = 0;
    while(1) {
	if (buf[pos1]>='0' && buf[pos1]<='9')
	    addr = 16*addr + (buf[pos1]-'0');
	else if (buf[pos1]>='a' && buf[pos1]<='f')
	    addr = 16*addr + 10 + (buf[pos1]-'a');
	else break;
	pos1++;
    }
    if ((addr == 0) || (buf[pos1] != ':')) return false;

    // further parsing of objdump output...
    pos1++;
    while(buf[pos1]==' ' || buf[pos1]=='\t') pos1++;

    // skip code, pattern "xx "*
    pos2 = pos1;
    while(1) {
	if (! ((buf[pos2]>='0' && buf[pos2]<='9') ||
	       (buf[pos2]>='a' && buf[pos2]<='f')) ) break;
	if (! ((buf[pos2+1]>='0' && buf[pos2+1]<='9') ||
	       (buf[pos2+1]>='a' && buf[pos2+1]<='f')) ) break;
	if (buf[pos2+2] != ' ') break;
	pos2 += 3;
    }
    buf[pos2-1]=0;
    while(buf[pos2]==' '|| buf[pos2]=='\t') pos2++;

    // skip mnemonic
    pos3 = pos2;
    while(buf[pos3] && buf[pos3]!=' ' && buf[pos3]!='\t') pos3++;
    if (buf[pos3] != 0) {
	buf[pos3] = 0;
	pos3++;
	while(buf[pos3]==' '|| buf[pos3]=='\t') pos3++;
    }

    // maximal 50 chars
    if (strlen(buf+pos2) > 50)
	strcpy(buf+pos2+47, "...");

    if (0) qDebug("For 0x%x: Code '%s', Mnc '%s', Args '%s'",
		  addr, buf+pos1, buf+pos2, buf+pos3);

    return true;
}




//
// InstrView
//


InstrView::InstrView(TraceItemView* parentView,
                     QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
  _inSelectionUpdate = false;
  _arrowLevels = 0;
  _lowList.setSortLow(true);
  _highList.setSortLow(false);

  addColumn( i18n( "#" ) );
  addColumn( i18n( "Cost" ) );
  addColumn( "" );
  addColumn( i18n( "Machine Code" ) );
  addColumn( i18n( "Cmd." ) );
  addColumn( i18n( "Assembler" ) );
  addColumn( i18n( "Source Position" ) );

  setAllColumnsShowFocus(true);
  setColumnAlignment(1, Qt::AlignRight);

  connect(this,
          SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
          SLOT(context(QListViewItem*, const QPoint &, int)));

  connect(this, SIGNAL(selectionChanged(QListViewItem*)),
          SLOT(selectedSlot(QListViewItem*)));

  connect(this,
          SIGNAL(doubleClicked(QListViewItem*)),
          SLOT(activatedSlot(QListViewItem*)));

  connect(this,
          SIGNAL(returnPressed(QListViewItem*)),
          SLOT(activatedSlot(QListViewItem*)));

  QWhatsThis::add( this, whatsThis());
}

void InstrView::paintEmptyArea( QPainter * p, const QRect & r)
{
  QListView::paintEmptyArea(p, r);
}

QString InstrView::whatsThis() const
{
    return i18n( "<b>Annotated Assembler</b>"
		 "<p>The annotated assembler list shows the "
		 "machine code instructions of the current selected "
		 "function together with (self) cost spent while "
		 "executing an instruction. If this is a call "
		 "instruction, lines with details on the "
		 "call happening are inserted into the source: "
		 "These are (cumulative) cost spent inside of the call, "
		 "number of calls happening, and call destination.</p>"
		 "<p>The disassembler output shown is generated with "
		 "the 'objdump' utility from the 'binutils' package.</p>"
		 "<p>Select a line with call information to "
		 "make the destination function of this call current.</p>");
}

void InstrView::context(QListViewItem* i, const QPoint & p, int)
{
  QPopupMenu popup;

  TraceInstrCall* ic = i ? ((InstrItem*) i)->instrCall() : 0;
  TraceInstrJump* ij = i ? ((InstrItem*) i)->instrJump() : 0;
  TraceFunction* f = ic ? ic->call()->called() : 0;
  TraceInstr* instr = ij ? ij->instrTo() : 0;

  if (f) {
    QString name = f->name();
    if ((int)name.length()>Configuration::maxSymbolLength())
      name = name.left(Configuration::maxSymbolLength()) + "...";
    popup.insertItem(i18n("Go to '%1'").arg(name), 93);
    popup.insertSeparator();
  }
  else if (instr) {
    popup.insertItem(i18n("Go to Address %1").arg(instr->name()), 93);
    popup.insertSeparator();
  }

  popup.insertItem(i18n("Go Back"), 90);
  popup.insertItem(i18n("Go Forward"), 91);
  popup.insertItem(i18n("Go Up"), 92);

  int r = popup.exec(p);
  if      (r == 90) activated(Back);
  else if (r == 91) activated(Forward);
  else if (r == 92) activated(Up);
  else if (r == 93) {
    if (f) activated(f);
    if (instr) activated(instr);
  }
}


void InstrView::selectedSlot(QListViewItem * i)
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
    TraceFunction* f = ic->call()->called();
    if (f) {
	_selectedItem = f;
	selected(f);
    }
  }
  else if (ij) {
    TraceInstr* instr = ij->instrTo();
    if (instr) {
	_selectedItem = instr;
	selected(instr);
    }
  }
}

void InstrView::activatedSlot(QListViewItem * i)
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


TraceItem* InstrView::canShow(TraceItem* i)
{
  TraceItem::CostType t = i ? i->type() : TraceItem::NoCostType;
  TraceFunction* f = 0;

  switch(t) {
  case TraceItem::Function:
      f = (TraceFunction*) i;
      break;

  case TraceItem::Instr:
      f = ((TraceInstr*)i)->function();
      select(i);
      break;

  case TraceItem::Line:
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

      InstrItem *ii = (InstrItem*)QListView::selectedItem();
      if (ii) {
	  if ((ii->instr() == _selectedItem) ||
	      (ii->instr() && (ii->instr()->line() == _selectedItem))) return;
      }

      QListViewItem *item, *item2;
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
    QListViewItem *item, *item2;
    for (item = firstChild();item;item = item->nextSibling())
      for (item2 = item->firstChild();item2;item2 = item2->nextSibling())
        ((InstrItem*)item2)->setGroupType(_groupType);
    return;
  }

  refresh();
}

void InstrView::refresh()
{
    _arrowLevels = 0;

    clear();
    setColumnWidth(0, 20);
    setColumnWidth(1, 50);
    setColumnWidth(2, 0);  // arrows, defaults to invisible
    setColumnWidth(3, 50); // machine code column
    setColumnWidth(4, 20); // command column
    setColumnWidth(5, 200); // arg column
    setSorting(0); // always reset to address number sort

    if (!_data || !_activeItem) return;

    TraceItem::CostType t = _activeItem->type();
    TraceFunction* f = 0;
    if (t == TraceItem::Function) f = (TraceFunction*) _activeItem;
    if (t == TraceItem::Instr) {
	f = ((TraceInstr*)_activeItem)->function();
	if (!_selectedItem) _selectedItem = _activeItem;
    }
    if (t == TraceItem::Line) {
	f = ((TraceLine*)_activeItem)->functionSource()->function();
	if (!_selectedItem) _selectedItem = _activeItem;
    }

    if (!f) return;

    // check for instruction map
    TraceInstrMap::Iterator itStart, it, tmpIt, itEnd;
    TraceInstrMap* instrMap = f->instrMap();
    if (instrMap) {
	it    = instrMap->begin();
	itEnd = instrMap->end();
	// get first instruction with cost of selected type
	while((it != itEnd) && !(*it).hasCost(_costType)) ++it;
    }
    if (!instrMap || (it == itEnd)) {
	new InstrItem(this, 1,
		      i18n("There is no instruction info in the trace file."));
	new InstrItem(this, 2,
		      i18n("For the Valgrind Calltree Skin, rerun with option"));
	new InstrItem(this, 3, i18n("      --dump-instr=yes"));
	new InstrItem(this, 4, i18n("To see (conditional) jumps, additionally specify"));
	new InstrItem(this, 5, i18n("      --trace-jump=yes"));
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
	while((it != itEnd) && !(*it).hasCost(_costType)) ++it;
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
	    while((it != itEnd) && !(*it).hasCost(_costType)) ++it;
	    if (it == itEnd) break;
	    if ((*it).addr() - (*tmpIt).addr() > 10000) break;
	}

	// tmpIt is always last instruction with cost
	if (!fillInstrRange(f, itStart, ++tmpIt)) break;
	if (it == itEnd) break;
    }
}


void InstrView::updateJumpArray(uint addr, InstrItem* ii,
				bool ignoreFrom, bool ignoreTo)
{
    TraceInstrJump* ij;
    uint lowAddr, highAddr;
    int iEnd = -1, iStart = -1;

    if (0) qDebug("updateJumpArray(addr 0x%x, jump to %s)",
		  addr,
		  ii->instrJump()
		  ? ii->instrJump()->instrTo()->name().ascii() : "?" );

    ij=_lowList.current();
    while(ij) {
	lowAddr = ij->instrFrom()->addr();
	if (ij->instrTo()->addr() < lowAddr) {
	    lowAddr = ij->instrTo()->addr();
	    if (ignoreTo) break;
	}
	else if (ignoreFrom) break;
	if (lowAddr > addr) break;

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
	    _jump[iStart] = ij;
	}
	ij=_lowList.next();
	if (lowAddr == addr) break;
    }

    ii->setJumpArray(_jump);

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
	    qDebug("InstrView: no jump start for end at %x ?", highAddr);
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
    uint costAddr, nextCostAddr, objAddr, addr;
    uint dumpStartAddr, dumpEndAddr;
    TraceInstrMap::Iterator costIt;

    // shouldn't happen
    if (it == itEnd) return false;

    // calculate address range for call to objdump
    TraceInstrMap::Iterator tmpIt = itEnd;
    --tmpIt;
    nextCostAddr = (*it).addr();
    dumpStartAddr = (nextCostAddr<20) ? 0 : nextCostAddr -20;
    dumpEndAddr   = (*tmpIt).addr() +20;

    // generate command
    QString popencmd, objfile;
    objfile = function->object()->name();
    objfile = objfile.replace(QRegExp("[\"']"), ""); // security...
    popencmd = QString("objdump -C -D "
                       "--start-address=0x%1 --stop-address=0x%2 \"%3\"")
	.arg(dumpStartAddr,0,16).arg(dumpEndAddr,0,16)
	.arg(objfile);
    if (0) qDebug("Running '%s'...", popencmd.ascii());

    // and run...
    FILE* iFILE = popen(QFile::encodeName( popencmd ), "r");
    if (iFILE == 0) {
	new InstrItem(this, 1,
		      i18n("There is an error trying to execute the command"));
	new InstrItem(this, 2, "");
	new InstrItem(this, 3, popencmd);
	new InstrItem(this, 4, "");
	new InstrItem(this, 5,
		      i18n("Check that you have installed the 'objdump'\n"
                   "utility from the 'binutils' package and try again."));
	return false;
    }
    QFile file;
    file.open(IO_ReadOnly, iFILE);

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
          readBytes=file.readLine(buf, BUF_SIZE);
          if (readBytes<=0) break;

          objdumpLineno++;
          if (readBytes == BUF_SIZE) {
	    qDebug("ERROR: Line %d of '%s' too long\n",
                   objdumpLineno, popencmd.ascii());
          }
          if (buf[readBytes-1] == '\n')
	    buf[readBytes-1] = 0;

          objAddr = parseAddr(buf);
          if ((objAddr<dumpStartAddr) || (objAddr>dumpEndAddr))
            objAddr = 0;
          if (objAddr != 0) break;
        }

        if (0) qDebug("Got ObjAddr: 0x%x", objAddr);
      }

      // try to keep objAddr in [costAddr;nextCostAddr]
      if (needCostAddr &&
	  (nextCostAddr > 0) &&
	  ((objAddr ==0) || (objAddr >= nextCostAddr)) ) {
	  needCostAddr = false;

	  costIt = it;
	  ++it;
	  while((it != itEnd) && !(*it).hasCost(_costType)) ++it;
	  costAddr = nextCostAddr;
	  nextCostAddr = (it == itEnd) ? 0 : (*it).addr();

	  if (0) qDebug("Got nextCostAddr: 0x%x, costAddr 0x%x",
			nextCostAddr, costAddr);
      }

      // if we have no more address from objdump, stop
      if (objAddr == 0) break;

      if ((nextCostAddr==0) || (costAddr == 0) ||
	  (objAddr < nextCostAddr)) {
	  // next line is objAddr

	  uint pos1, pos2, pos3;

	  // this sets addr
	  parseLine(buf, addr, pos1, pos2, pos3);
	  code = QString(buf + pos1);
	  cmd  = QString(buf + pos2);
	  args = QString(buf + pos3);

	  if (costAddr == objAddr) {
	      currInstr = &(*costIt);
	      needCostAddr = true;
	  }
	  else
	      currInstr = 0;

	  needObjAddr = true;

	  if (0) qDebug("Dump Obj Addr: 0x%x [%s %s], cost (0x%x, next 0x%x)",
			addr, cmd.ascii(), args.ascii(),
			costAddr, nextCostAddr);
      }
      else {
	  addr = costAddr;
	  code = cmd = QString();
	  args = i18n("(No Assembler)");

	  currInstr = &(*costIt);
	  needCostAddr = true;

	  noAssLines++;
	  if (0) qDebug("Dump Cost Addr: 0x%x (no ass), objAddr 0x%x",
			addr, objAddr);
      }

      // update inside
      if (!inside) {
	  if (currInstr) inside = true;
      }
      else {
	  if (0) qDebug("Check if 0x%x is in ]0x%x,0x%x[",
			addr, costAddr,
			nextCostAddr - 3*Configuration::noCostInside() );

	  // Suppose a average instruction len of 3 bytes
	  if ( (addr > costAddr) &&
	       ((nextCostAddr==0) ||
		(addr < nextCostAddr - 3*Configuration::noCostInside()) ))
	      inside = false;
      }

      int context = inside ? Configuration::contextInside() :
                    Configuration::contextOutside();

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


      ii = new InstrItem(this, addr, inside,
                         code, cmd, args, currInstr, _costType);
      dumpedLines++;
      if (0) qDebug("Dumped 0x%x %s%s", addr,
		    inside ? "Inside " : "Outside",
		    currInstr ? "Cost" : "");

      // no calls/jumps if we have no cost for this line
      if (!currInstr) continue;


      if (!selected &&
	  (currInstr == _selectedItem) ||
	  (currInstr->line() == _selectedItem)) selected = ii;

      if (!first) first = ii;

      if (currInstr->subCost(_costType) > most) {
        item = ii;
        most = currInstr->subCost(_costType);
      }

      ii->setOpen(true);
      TraceInstrCallList list = currInstr->instrCalls();
      TraceInstrCall* ic;
      for (ic=list.first();ic;ic=list.next()) {
	  if (ic->subCost(_costType)==0) continue;

	  if (ic->subCost(_costType) > most) {
	      item = ii;
	      most = ic->subCost(_costType);
	  }

	  ii2 = new InstrItem(ii, addr, currInstr, ic, _costType, _groupType);

	  if (!selected && (ic->call()->called() == _selectedItem))
	      selected = ii2;
      }

      TraceInstrJumpList jlist = currInstr->instrJumps();
      TraceInstrJump* ij;
      for (ij=jlist.first();ij;ij=jlist.next()) {
	  if (ij->executedCount()==0) continue;

	  new InstrItem(ii, addr, currInstr, ij);
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

    file.close();
    pclose(iFILE);


    // for arrows: go down the list according to list sorting
    sort();
    QListViewItem *item1, *item2;
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
	setColumnWidth(2, 10 + 6*arrowLevels() + itemMargin() * 2);
    else
	setColumnWidth(2, 0);


    if (noAssLines > 1) {
	// trace cost not machting code
	new InstrItem(this, 1,
		      i18n("There is %n cost line without assembler code.",
                   "There are %n cost lines without assembler code.",
                   noAssLines));
	new InstrItem(this, 2,
		      i18n("This happens because the code of"));
	new InstrItem(this, 3, QString("        %1").arg(objfile));
	new InstrItem(this, 4,
		      i18n("doesn't seem to match the trace file."));
	new InstrItem(this, 5, "");
	new InstrItem(this, 6,
		      i18n("Are you using an old trace file or is the above mentioned"));
	new InstrItem(this, 7,
		      i18n("ELF object from an updated installation/another machine?"));
	new InstrItem(this, 8, "");
	return false;
    }

    if (dumpedLines == 0) {
	// no matching line read from popen
	new InstrItem(this, 1,
		      i18n("There seems to be an error trying to execute the command"));
	new InstrItem(this, 2, "");
	new InstrItem(this, 3, popencmd);
	new InstrItem(this, 4, "");
	new InstrItem(this, 5,
		      i18n("Check that the ELF object used in the command is existing"));
	new InstrItem(this, 5,
		      i18n("on this machine and that you have installed the 'objdump'"));
	new InstrItem(this, 6,
		      i18n("utility from the 'binutils' package and try again."));
	return false;
    }

    return true;
}


void InstrView::updateInstrItems()
{
    InstrItem* ii;
    QListViewItem* item  = firstChild();
    for (;item;item = item->nextSibling()) {
	ii = (InstrItem*)item;
	TraceInstr* instr = ii->instr();
	if (!instr) continue;

	ii->update();

	QListViewItem *next, *i  = ii->firstChild();
	for (;i;i = next) {
	    next = i->nextSibling();
	    ((InstrItem*)i)->update();
	}
    }
}

#include "instrview.moc"
