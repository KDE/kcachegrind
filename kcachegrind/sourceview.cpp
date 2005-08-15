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
 * Source View
 */

#include <qdir.h>
#include <qfile.h>
#include <qwhatsthis.h>
#include <qpopupmenu.h>
#include <klocale.h>
#include <kdebug.h>

#include "configuration.h"
#include "sourceitem.h"
#include "sourceview.h"


//
// SourceView
//


SourceView::SourceView(TraceItemView* parentView,
                       QWidget* parent, const char* name)
  : QListView(parent, name), TraceItemView(parentView)
{
  _inSelectionUpdate = false;

  _arrowLevels = 0;
  _lowList.setSortLow(true);
  _highList.setSortLow(false);

  addColumn( i18n( "#" ) );
  addColumn( i18n( "Cost" ) );
  addColumn( i18n( "Cost 2" ) );
  addColumn( "" );
  addColumn( i18n( "Source (unknown)" ) );

  setAllColumnsShowFocus(true);
  setColumnAlignment(0, Qt::AlignRight);
  setColumnAlignment(1, Qt::AlignRight);
  setColumnAlignment(2, Qt::AlignRight);
  setResizeMode(QListView::LastColumn);

  connect(this,
          SIGNAL(contextMenuRequested(QListViewItem*, const QPoint &, int)),
          SLOT(context(QListViewItem*, const QPoint &, int)));

  connect(this,
          SIGNAL(selectionChanged(QListViewItem*)),
          SLOT(selectedSlot(QListViewItem*)));

  connect(this,
          SIGNAL(doubleClicked(QListViewItem*)),
          SLOT(activatedSlot(QListViewItem*)));

  connect(this,
          SIGNAL(returnPressed(QListViewItem*)),
          SLOT(activatedSlot(QListViewItem*)));

  QWhatsThis::add( this, whatsThis());
}

void SourceView::paintEmptyArea( QPainter * p, const QRect & r)
{
  QListView::paintEmptyArea(p, r);
}


QString SourceView::whatsThis() const
{
    return i18n( "<b>Annotated Source</b>"
		 "<p>The annotated source list shows the "
		 "source lines of the current selected function "
		 "together with (self) cost spent while executing the "
		 "code of this source line. If there was a call "
		 "in a source line, lines with details on the "
		 "call happening are inserted into the source: "
		 "the cost spent inside of the call, the "
		 "number of calls happening, and the call destination.</p>"
		 "<p>Select a inserted call information line to "
		 "make the destination function current.</p>");
}

void SourceView::context(QListViewItem* i, const QPoint & p, int c)
{
  QPopupMenu popup;

  // Menu entry:
  TraceLineCall* lc = i ? ((SourceItem*) i)->lineCall() : 0;
  TraceLineJump* lj = i ? ((SourceItem*) i)->lineJump() : 0;
  TraceFunction* f = lc ? lc->call()->called() : 0;
  TraceLine* line = lj ? lj->lineTo() : 0;

  if (f) {
    QString name = f->name();
    if ((int)name.length()>Configuration::maxSymbolLength())
      name = name.left(Configuration::maxSymbolLength()) + "...";
    popup.insertItem(i18n("Go to '%1'").arg(name), 93);
    popup.insertSeparator();
  }
  else if (line) {
    popup.insertItem(i18n("Go to Line %1").arg(line->name()), 93);
    popup.insertSeparator();
  }

  if ((c == 1) || (c == 2)) {
    addCostMenu(&popup);
    popup.insertSeparator();
  }
  addGoMenu(&popup);

  int r = popup.exec(p);
  if (r == 93) {
    if (f) activated(f);
    if (line) activated(line);
  }
}


void SourceView::selectedSlot(QListViewItem * i)
{
  if (!i) return;
  // programatically selected items are not signalled
  if (_inSelectionUpdate) return;

  TraceLineCall* lc = ((SourceItem*) i)->lineCall();
  TraceLineJump* lj = ((SourceItem*) i)->lineJump();

  if (!lc && !lj) {
      TraceLine* l = ((SourceItem*) i)->line();
      if (l) {
	  _selectedItem = l;
	  selected(l);
      }
      return;
  }

  TraceFunction* f = lc ? lc->call()->called() : 0;
  if (f) {
      _selectedItem = f;
      selected(f);
  }
  else {
    TraceLine* line = lj ? lj->lineTo() : 0;
    if (line) {
	_selectedItem = line;
	selected(line);
    }
  }
}

void SourceView::activatedSlot(QListViewItem * i)
{
  if (!i) return;
  TraceLineCall* lc = ((SourceItem*) i)->lineCall();
  TraceLineJump* lj = ((SourceItem*) i)->lineJump();

  if (!lc && !lj) {
      TraceLine* l = ((SourceItem*) i)->line();
      if (l) activated(l);
      return;
  }

  TraceFunction* f = lc ? lc->call()->called() : 0;
  if (f) activated(f);
  else {
    TraceLine* line = lj ? lj->lineTo() : 0;
    if (line) activated(line);
  }
}

TraceItem* SourceView::canShow(TraceItem* i)
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

void SourceView::doUpdate(int changeType)
{
  // Special case ?
  if (changeType == selectedItemChanged) {

      if (!_selectedItem) {
	  clearSelection();
	  return;
      }

      TraceLine* sLine = 0;
      if (_selectedItem->type() == TraceItem::Line)
	  sLine = (TraceLine*) _selectedItem;
      if (_selectedItem->type() == TraceItem::Instr)
	  sLine = ((TraceInstr*)_selectedItem)->line();

      SourceItem* si = (SourceItem*)QListView::selectedItem();
      if (si) {
	  if (si->line() == sLine) return;
	  if (si->lineCall() &&
	      (si->lineCall()->call()->called() == _selectedItem)) return;
      }

      QListViewItem *item, *item2;
      for (item = firstChild();item;item = item->nextSibling()) {
	  si = (SourceItem*)item;
	  if (si->line() == sLine) {
	      ensureItemVisible(item);
              _inSelectionUpdate = true;
	      setCurrentItem(item);
              _inSelectionUpdate = false;
	      break;
	  }
	  item2 = item->firstChild();
	  for (;item2;item2 = item2->nextSibling()) {
	      si = (SourceItem*)item2;
	      if (!si->lineCall()) continue;
	      if (si->lineCall()->call()->called() == _selectedItem) {
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
        ((SourceItem*)item2)->updateGroup();
  }

  refresh();
}

void SourceView::refresh()
{
  clear();
  setColumnWidth(0, 20);
  setColumnWidth(1, 50);
  setColumnWidth(2, _costType2 ? 50:0);
  setColumnWidth(3, 0); // arrows, defaults to invisible
  setSorting(0); // always reset to line number sort
  if (_costType)
    setColumnText(1, _costType->name());
  if (_costType2)
    setColumnText(2, _costType2->name());

  _arrowLevels = 0;

  if (!_data || !_activeItem) {
    setColumnText(4, i18n("(No Source)"));
    return;
  }

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

  // Allow resizing of column 2
  setColumnWidthMode(2, QListView::Maximum);

  TraceFunctionSource* mainSF = f->sourceFile();

  // skip first source if there's no debug info and there are more sources
  // (this is for a bug in GCC 2.95.x giving unknown source for prologs)
  if (mainSF &&
      (mainSF->firstLineno() == 0) &&
      (mainSF->lastLineno() == 0) &&
      (f->sourceFiles().count()>1) ) {
	  // skip
  }
  else
      fillSourceFile(mainSF, 0);

  TraceFunctionSource* sf;
  int fileno = 1;
  TraceFunctionSourceList l = f->sourceFiles();
  for (sf=l.first();sf;sf=l.next(), fileno++)
    if (sf != mainSF)
      fillSourceFile(sf, fileno);

  if (!_costType2) {
    setColumnWidthMode(2, QListView::Manual);
    setColumnWidth(2, 0);
  }
}


// helper for fillSourceList:
// search recursive for a file, starting from a base dir
static bool checkFileExistance(QString& dir, const QString& name)
{
  // we leave this in...
  qDebug("Checking %s/%s", dir.ascii(), name.ascii());

  if (QFile::exists(dir + "/" + name)) return true;

  // check in subdirectories
  QDir d(dir);
  d.setFilter( QDir::Dirs | QDir::NoSymLinks );
  d.setSorting( QDir::Unsorted );
  QStringList subdirs = d.entryList();
  QStringList::Iterator it =subdirs.begin();
  for(; it != subdirs.end(); ++it ) {
    if (*it == "." || *it == ".." || *it == "CVS") continue;

    dir = d.filePath(*it);
    if (checkFileExistance(dir, name)) return true;
  }
  return false;
}


void SourceView::updateJumpArray(uint lineno, SourceItem* si,
				 bool ignoreFrom, bool ignoreTo)
{
    TraceLineJump* lj;
    uint lowLineno, highLineno;
    int iEnd = -1, iStart = -1;

    if (0) qDebug("updateJumpArray(line %d, jump to %s)",
		  lineno,
		  si->lineJump()
		  ? si->lineJump()->lineTo()->name().ascii() : "?" );


    lj=_lowList.current();
    while(lj) {
	lowLineno = lj->lineFrom()->lineno();
	if (lj->lineTo()->lineno() < lowLineno)
	    lowLineno = lj->lineTo()->lineno();

	if (lowLineno > lineno) break;

	if (ignoreFrom && (lowLineno < lj->lineTo()->lineno())) break;
	if (ignoreTo && (lowLineno < lj->lineFrom()->lineno())) break;

	if (si->lineJump() && (lj != si->lineJump())) break;

	int asize = (int)_jump.size();
#if 0
	for(iStart=0;iStart<asize;iStart++)
	    if (_jump[iStart] &&
		(_jump[iStart]->lineTo() == lj->lineTo())) break;
#else
	iStart = asize;
#endif

	if (iStart == asize) {
	    for(iStart=0;iStart<asize;iStart++)
		if (_jump[iStart] == 0) break;

	    if (iStart== asize) {
		asize++;
		_jump.resize(asize);
		if (asize > _arrowLevels) _arrowLevels = asize;
	    }

	    if (0) qDebug(" start %d (%s to %s)",
			  iStart,
			  lj->lineFrom()->name().ascii(),
			  lj->lineTo()->name().ascii());

	    _jump[iStart] = lj;
	}
	lj=_lowList.next();
    }

    si->setJumpArray(_jump);

    lj=_highList.current();
    while(lj) {
	highLineno = lj->lineFrom()->lineno();
	if (lj->lineTo()->lineno() > highLineno) {
	    highLineno = lj->lineTo()->lineno();
	    if (ignoreTo) break;
	}
	else if (ignoreFrom) break;

	if (highLineno > lineno) break;

	for(iEnd=0;iEnd< (int)_jump.size();iEnd++)
	    if (_jump[iEnd] == lj) break;
	if (iEnd == (int)_jump.size()) {
	    qDebug("LineView: no jump start for end at %x ?", highLineno);
	    iEnd = -1;
	}
	lj=_highList.next();

	if (0 && (iEnd>=0))
	    qDebug(" end %d (%s to %s)",
		   iEnd,
		   _jump[iEnd]->lineFrom()->name().ascii(),
		   _jump[iEnd]->lineTo()->name().ascii());

	if (0 && lj) qDebug("next end: %s to %s",
			    lj->lineFrom()->name().ascii(),
			    lj->lineTo()->name().ascii());

	if (highLineno > lineno)
	    break;
	else {
	    if (iEnd>=0) _jump[iEnd] = 0;
	    iEnd = -1;
	}
    }
    if (iEnd>=0) _jump[iEnd] = 0;
}


/* If sourceList is empty we set the source file name into the header,
 * else this code is of a inlined function, and we add "inlined from..."
 */
void SourceView::fillSourceFile(TraceFunctionSource* sf, int fileno)
{
  if (!sf) return;

  if (0) qDebug("Selected Item %s", 
		_selectedItem ? _selectedItem->name().ascii() : "(none)");

  TraceLineMap::Iterator lineIt, lineItEnd;
  int nextCostLineno = 0, lastCostLineno = 0;

  bool validSourceFile = (!sf->file()->name().isEmpty());

  TraceLine* sLine = 0;
  if (_selectedItem) {
    if (_selectedItem->type() == TraceItem::Line)
      sLine = (TraceLine*) _selectedItem;
    if (_selectedItem->type() == TraceItem::Instr)
      sLine = ((TraceInstr*)_selectedItem)->line();
  }

  if (validSourceFile) {
      TraceLineMap* lineMap = sf->lineMap();
      if (lineMap) {
	  lineIt    = lineMap->begin();
	  lineItEnd = lineMap->end();
	  // get first line with cost of selected type
	  while(lineIt != lineItEnd) {
	    if (&(*lineIt) == sLine) break;
	    if ((*lineIt).hasCost(_costType)) break;
	    if (_costType2 && (*lineIt).hasCost(_costType2)) break;
	    ++lineIt;
	  }

	  nextCostLineno     = (lineIt == lineItEnd) ? 0 : (*lineIt).lineno();
	  if (nextCostLineno<0) {
	    kdError() << "SourceView::fillSourceFile: Negative line number "
		      << nextCostLineno << endl
		      << "  Function '" << sf->function()->name() << "'" << endl
		      << "  File '" << sf->file()->name() << "'" << endl;
	    nextCostLineno = 0;
	  }
	    
      }

      if (nextCostLineno == 0) {
	  new SourceItem(this, this, fileno, 0, false,
			 i18n("There is no cost of current selected type associated"));
	  new SourceItem(this, this, fileno, 1, false,
			 i18n("with any source line of this function in file"));
	  new SourceItem(this, this, fileno, 2, false,
			 QString("    '%1'").arg(sf->function()->prettyName()));
	  new SourceItem(this, this, fileno, 3, false,
			 i18n("Thus, no annotated source can be shown."));
	  return;
      }
  }

  QString filename = sf->file()->shortName();
  QString dir = sf->file()->directory();
  if (!dir.isEmpty())
    filename = dir + "/" + filename;

  if (nextCostLineno>0) {
    // we have debug info... search for source file
    if (!QFile::exists(filename)) {
      QStringList list = Configuration::sourceDirs(_data,
						   sf->function()->object());
      QStringList::Iterator it;

      for ( it = list.begin(); it != list.end(); ++it ) {
        dir = *it;
        if (checkFileExistance(dir, sf->file()->shortName())) break;
      }

      if (it == list.end())
	  nextCostLineno = 0;
      else {
        filename = dir + "/" + sf->file()->shortName();
        // no need to search again
        sf->file()->setDirectory(dir);
      }
    }
  }

  // do it here, because the source directory could have been set before
  if (childCount()==0) {
    setColumnText(4, validSourceFile ?
                  i18n("Source ('%1')").arg(filename) :
                  i18n("Source (unknown)"));
  }
  else {
    new SourceItem(this, this, fileno, 0, true,
                   validSourceFile ?
                   i18n("--- Inlined from '%1' ---").arg(filename) :
                   i18n("--- Inlined from unknown source ---"));
  }

  if (nextCostLineno == 0) {
    new SourceItem(this, this, fileno, 0, false,
                   i18n("There is no source available for the following function:"));
    new SourceItem(this, this, fileno, 1, false,
                   QString("    '%1'").arg(sf->function()->prettyName()));
    if (sf->file()->name().isEmpty()) {
      new SourceItem(this, this, fileno, 2, false,
                     i18n("This is because no debug information is present."));
      new SourceItem(this, this, fileno, 3, false,
                     i18n("Recompile source and redo the profile run."));
      if (sf->function()->object()) {
        new SourceItem(this, this, fileno, 4, false,
                       i18n("The function is located in this ELF object:"));
        new SourceItem(this, this, fileno, 5, false,
                       QString("    '%1'")
                       .arg(sf->function()->object()->prettyName()));
      }
    }
    else {
      new SourceItem(this, this, fileno, 2, false,
                     i18n("This is because its source file cannot be found:"));
      new SourceItem(this, this, fileno, 3, false,
                     QString("    '%1'").arg(sf->file()->name()));
      new SourceItem(this, this, fileno, 4, false,
                     i18n("Add the folder of this file to the source folder list."));
      new SourceItem(this, this, fileno, 5, false,
                     i18n("The list can be found in the configuration dialog."));
    }
    return;
  }


  // initialisation for arrow drawing
  // create sorted list of jumps (for jump arrows)
  TraceLineMap::Iterator it = lineIt, nextIt;
  _lowList.clear();
  _highList.clear();
  while(1) {

      nextIt = it;
      ++nextIt;
      while(nextIt != lineItEnd) {
	if (&(*nextIt) == sLine) break;
	if ((*nextIt).hasCost(_costType)) break;
	if (_costType2 && (*nextIt).hasCost(_costType2)) break;
	++nextIt;
      }

      TraceLineJumpList jlist = (*it).lineJumps();
      TraceLineJump* lj;
      for (lj=jlist.first();lj;lj=jlist.next()) {
	  if (lj->executedCount()==0) continue;
	  // skip jumps to next source line with cost
	  //if (lj->lineTo() == &(*nextIt)) continue;

	  _lowList.append(lj);
	  _highList.append(lj);
      }
      it = nextIt;
      if (it == lineItEnd) break;
  }
  _lowList.sort();
  _highList.sort();
  _lowList.first(); // iterators to list start
  _highList.first();
  _jump.resize(0);


  char buf[256];
  bool inside = false, skipLineWritten = true;
  int readBytes;
  int fileLineno = 0;
  SubCost most = 0;

  TraceLine* currLine;
  SourceItem *si, *si2, *item = 0, *first = 0, *selected = 0;
  QFile file(filename);
  if (!file.open(IO_ReadOnly)) return;
  while (1) {
    readBytes=file.readLine(buf, sizeof( buf ));
    if (readBytes<=0) {
      // for nice empty 4 lines after function with EOF
      buf[0] = 0;
    }

    if (readBytes >= (int) sizeof( buf )) {
      qDebug("%s:%d  Line too long\n",
	     sf->file()->name().ascii(), fileLineno);
    }
    else if ((readBytes>0) && (buf[readBytes-1] == '\n'))
      buf[readBytes-1] = 0;


    // keep fileLineno inside [lastCostLineno;nextCostLineno]
    fileLineno++;
    if (fileLineno == nextCostLineno) {
	currLine = &(*lineIt);

	// get next line with cost of selected type
	++lineIt;
	while(lineIt != lineItEnd) {
	  if (&(*lineIt) == sLine) break;
	  if ((*lineIt).hasCost(_costType)) break;
	  if (_costType2 && (*lineIt).hasCost(_costType2)) break;
	  ++lineIt;
	}

	lastCostLineno = nextCostLineno;
	nextCostLineno = (lineIt == lineItEnd) ? 0 : (*lineIt).lineno();
    }
    else
	currLine = 0;

    // update inside
    if (!inside) {
	if (currLine) inside = true;
    }
    else {
	if ( (fileLineno > lastCostLineno) &&
	     ((nextCostLineno == 0) ||
	      (fileLineno < nextCostLineno - Configuration::noCostInside()) ))
	    inside = false;
    }

    int context = Configuration::context();

    if ( ((lastCostLineno==0) || (fileLineno > lastCostLineno + context)) &&
	 ((nextCostLineno==0) || (fileLineno < nextCostLineno - context))) {
	if (lineIt == lineItEnd) break;

	if (!skipLineWritten) {
	    skipLineWritten = true;
	    // a "skipping" line: print "..." instead of a line number
	    strcpy(buf,"...");
	}
	else
	    continue;
    }
    else
	skipLineWritten = false;

    si = new SourceItem(this, this, 
			fileno, fileLineno, inside, QString(buf),
                        currLine);

    if (!currLine) continue;

    if (!selected && (currLine == sLine)) selected = si;
    if (!first) first = si;

    if (currLine->subCost(_costType) > most) {
      item = si;
      most = currLine->subCost(_costType);
    }

    si->setOpen(true);
    TraceLineCallList list = currLine->lineCalls();
    TraceLineCall* lc;
    for (lc=list.first();lc;lc=list.next()) {
	if ((lc->subCost(_costType)==0) &&
	    (lc->subCost(_costType2)==0)) continue;

      if (lc->subCost(_costType) > most) {
        item = si;
        most = lc->subCost(_costType);
      }

      si2 = new SourceItem(this, si, fileno, fileLineno, currLine, lc);

      if (!selected && (lc->call()->called() == _selectedItem))
	  selected = si2;
    }

    TraceLineJumpList jlist = currLine->lineJumps();
    TraceLineJump* lj;
    for (lj=jlist.first();lj;lj=jlist.next()) {
	if (lj->executedCount()==0) continue;

	new SourceItem(this, si, fileno, fileLineno, currLine, lj);
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

  // for arrows: go down the list according to list sorting
  sort();
  QListViewItem *item1, *item2;
  for (item1=firstChild();item1;item1 = item1->nextSibling()) {
      si = (SourceItem*)item1;
      updateJumpArray(si->lineno(), si, true, false);

      for (item2=item1->firstChild();item2;item2 = item2->nextSibling()) {
	  si2 = (SourceItem*)item2;
	  if (si2->lineJump())
	      updateJumpArray(si->lineno(), si2, false, true);
	  else
	      si2->setJumpArray(_jump);
      }
  }

  if (arrowLevels())
      setColumnWidth(3, 10 + 6*arrowLevels() + itemMargin() * 2);
  else
      setColumnWidth(3, 0);
}


void SourceView::updateSourceItems()
{
    setColumnWidth(1, 50);
    setColumnWidth(2, _costType2 ? 50:0);
    // Allow resizing of column 2
    setColumnWidthMode(2, QListView::Maximum);
    
    if (_costType)
      setColumnText(1, _costType->name());
    if (_costType2)
      setColumnText(2, _costType2->name());

    SourceItem* si;
    QListViewItem* item  = firstChild();
    for (;item;item = item->nextSibling()) {
	si = (SourceItem*)item;
	TraceLine* l = si->line();
	if (!l) continue;

	si->updateCost();

	QListViewItem *next, *i  = si->firstChild();
	for (;i;i = next) {
	    next = i->nextSibling();
	    ((SourceItem*)i)->updateCost();
	}
    }

    if (!_costType2) {
      setColumnWidthMode(2, QListView::Manual);
      setColumnWidth(2, 0);
    }
}

#include "sourceview.moc"
