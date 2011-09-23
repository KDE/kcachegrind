/* This file is part of KCachegrind.
   Copyright (C) 2011 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Source View
 */

#include "sourceview.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QAction>
#include <QMenu>
#include <QScrollBar>
#include <QHeaderView>
#include <QKeyEvent>

#include "globalconfig.h"
#include "sourceitem.h"



//
// SourceView
//


SourceView::SourceView(TraceItemView* parentView,
                       QWidget* parent)
  : QTreeWidget(parent), TraceItemView(parentView)
{
  _inSelectionUpdate = false;

  _arrowLevels = 0;


  setColumnCount(5);
  setRootIsDecorated(false);
  setAllColumnsShowFocus(true);
  setUniformRowHeights(true);
  // collapsing call/jump lines by double-click is confusing
  setExpandsOnDoubleClick(false);

  QStringList headerLabels;
  headerLabels << tr( "#" )
               << tr( "Cost" )
               << tr( "Cost 2" )
               << ""
               <<  tr( "Source (unknown)");
  setHeaderLabels(headerLabels);

  // sorting will be enabled after refresh()
  sortByColumn(0, Qt::AscendingOrder);
  header()->setSortIndicatorShown(false);
  this->setItemDelegate(new SourceItemDelegate(this));
  this->setWhatsThis( whatsThis());

  connect( this,
           SIGNAL( currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
           SLOT( selectedSlot(QTreeWidgetItem*,QTreeWidgetItem*) ) );

  setContextMenuPolicy(Qt::CustomContextMenu);
  connect( this,
           SIGNAL(customContextMenuRequested(const QPoint &) ),
           SLOT(context(const QPoint &)));

  connect(this,
          SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
          SLOT(activatedSlot(QTreeWidgetItem*,int)));

  connect(header(), SIGNAL(sectionClicked(int)),
          this, SLOT(headerClicked(int)));
}

QString SourceView::whatsThis() const
{
    return tr( "<b>Annotated Source</b>"
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

void SourceView::context(const QPoint & p)
{
  int c = columnAt(p.x());
  QTreeWidgetItem* i = itemAt(p);
  QMenu popup;

  TraceLineCall* lc = i ? ((SourceItem*) i)->lineCall() : 0;
  TraceLineJump* lj = i ? ((SourceItem*) i)->lineJump() : 0;
  TraceFunction* f = lc ? lc->call()->called() : 0;
  TraceLine* line = lj ? lj->lineTo() : 0;

  QAction* activateFunctionAction = 0;
  QAction* activateLineAction = 0;
  if (f) {
      QString menuText = tr("Go to '%1'").arg(GlobalConfig::shortenSymbol(f->prettyName()));
      activateFunctionAction = popup.addAction(menuText);
      popup.addSeparator();
  }
  else if (line) {
      QString menuText = tr("Go to Line %1").arg(line->name());
      activateLineAction = popup.addAction(menuText);
      popup.addSeparator();
  }

  if ((c == 1) || (c == 2)) {
    addEventTypeMenu(&popup);
    popup.addSeparator();
  }
  addGoMenu(&popup);

  QAction* a = popup.exec(mapToGlobal(p + QPoint(0,header()->height())));
  if (a == activateFunctionAction)
      TraceItemView::activated(f);
  else if (a == activateLineAction)
      TraceItemView::activated(line);
}


void SourceView::selectedSlot(QTreeWidgetItem *i, QTreeWidgetItem *)
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

void SourceView::activatedSlot(QTreeWidgetItem* i, int)
{
  if (!i) return;

  TraceLineCall* lc = ((SourceItem*) i)->lineCall();
  TraceLineJump* lj = ((SourceItem*) i)->lineJump();

  if (!lc && !lj) {
      TraceLine* l = ((SourceItem*) i)->line();
      if (l) TraceItemView::activated(l);
      return;
  }

  TraceFunction* f = lc ? lc->call()->called() : 0;
  if (f) TraceItemView::activated(f);
  else {
    TraceLine* line = lj ? lj->lineTo() : 0;
    if (line) TraceItemView::activated(line);
  }
}

void SourceView::keyPressEvent(QKeyEvent* event)
{
    QTreeWidgetItem *item = currentItem();
    if (item && ((event->key() == Qt::Key_Return) ||
                 (event->key() == Qt::Key_Space)))
    {
        activatedSlot(item, 0);
    }
    QTreeView::keyPressEvent(event);
}

CostItem* SourceView::canShow(CostItem* i)
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

void SourceView::doUpdate(int changeType, bool)
{
  // Special case ?
  if (changeType == selectedItemChanged) {

      if (!_selectedItem) {
	  clearSelection();
	  return;
      }

      TraceLine* sLine = 0;
      if (_selectedItem->type() == ProfileContext::Line)
          sLine = (TraceLine*) _selectedItem;
      if (_selectedItem->type() == ProfileContext::Instr)
	  sLine = ((TraceInstr*)_selectedItem)->line();
      if (!sLine)
          return;

      QList<QTreeWidgetItem*> items = selectedItems();
      SourceItem* si = (items.count() > 0) ? (SourceItem*)items[0] : 0;
      if (si) {
	  if (si->line() == sLine) return;
	  if (si->lineCall() &&
	      (si->lineCall()->call()->called() == _selectedItem)) return;
      }

      QTreeWidgetItem *item, *item2;
      for (int i=0; i<topLevelItemCount(); i++) {
          item = topLevelItem(i);
	  si = (SourceItem*)item;
	  if (si->line() == sLine) {
              scrollToItem(item);
              _inSelectionUpdate = true;
	      setCurrentItem(item);
              _inSelectionUpdate = false;
	      break;
	  }
	  item2 = 0;
          for (int j=0; i<item->childCount(); j++) {
              item2 = item->child(j);
	      si = (SourceItem*)item2;
	      if (!si->lineCall()) continue;
	      if (si->lineCall()->call()->called() == _selectedItem) {
                  scrollToItem(item2);
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
      // update group colors for call lines
      QTreeWidgetItem *item, *item2;
      for (int i=0; i<topLevelItemCount(); i++) {
          item = topLevelItem(i);
          for (int j=0; i<item->childCount(); i++) {
              item2 = item->child(j);
              ((SourceItem*)item2)->updateGroup();
          }
      }
      return;
  }

  // On eventTypeChanged, we can not just change the costs shown in
  // already existing items, as costs of 0 should make the line to not
  // be shown at all. So we do a full refresh.

  refresh();
}

void SourceView::refresh()
{
  int originalPosition = verticalScrollBar()->value();
  clear();
  setColumnWidth(0, 20);
  setColumnWidth(1, 50);
  setColumnWidth(2, _eventType2 ? 50:0);
  setColumnWidth(3, 0); // arrows, defaults to invisible
  if (_eventType)
      headerItem()->setText(1, _eventType->name());
  if (_eventType2)
      headerItem()->setText(2, _eventType2->name());

  _arrowLevels = 0;
  if (!_data || !_activeItem) {
      headerItem()->setText(4, tr("(No Source)"));
      return;
  }

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

  TraceFunctionSource* mainSF = f->sourceFile();

  // skip first source if there is no debug info and there are more sources
  // (this is for a bug in GCC 2.95.x giving unknown source for prologs)
  if (mainSF &&
      (mainSF->firstLineno() == 0) &&
      (mainSF->lastLineno() == 0) &&
      (f->sourceFiles().count()>1) ) {
	  // skip
  }
  else
      fillSourceFile(mainSF, 0);

  int fileno = 0;
  foreach(TraceFunctionSource* sf, f->sourceFiles()) {
      fileno++;
      if (sf != mainSF)
          fillSourceFile(sf, fileno);
  }

  if (!_eventType2) {
      header()->setResizeMode(2, QHeaderView::Interactive);
      setColumnWidth(2, 0);
  }
  // reset to the original position - this is useful when the view
  // is refreshed just because we change between relative/absolute
  verticalScrollBar()->setValue(originalPosition);
}


/* Helper for fillSourceList:
 * search recursive for a file, starting from a base dir
 * If found, returns true and <dir> is set to the file path.
 */
static bool searchFileRecursive(QString& dir, const QString& name)
{
  // we leave this in...
  qDebug("Checking %s/%s", qPrintable(dir), qPrintable(name));

  if (QFile::exists(dir + '/' + name)) return true;

  // check in subdirectories
  QDir d(dir);
  d.setFilter( QDir::Dirs | QDir::NoSymLinks );
  d.setSorting( QDir::Unsorted );
  QStringList subdirs = d.entryList();
  QStringList::const_iterator it =subdirs.constBegin();
  for(; it != subdirs.constEnd(); ++it ) {
    if (*it == "." || *it == ".." || *it == "CVS") continue;

    dir = d.filePath(*it);
    if (searchFileRecursive(dir, name)) return true;
  }
  return false;
}

/* Search for a source file in different places.
 * If found, returns true and <dir> is set to the file path.
 */
bool SourceView::searchFile(QString& dir,
			    TraceFunctionSource* sf)
{
    QString name = sf->file()->shortName();

    if (QDir::isAbsolutePath(dir)) {
	if (QFile::exists(dir + '/' + name)) return true;
    }
    else {
	/* Directory is relative. Check
	 * - relative to cwd
	 * - relative to path of data file
	 */
	QString base = QDir::currentPath() + '/' + dir;
	if (QFile::exists(base + '/' + name)) {
	    dir = base;
	    return true;
	}

	TracePart* firstPart = _data->parts().first();
	if (firstPart) {
	    QFileInfo partFile(firstPart->name());
	    if (QFileInfo(partFile.absolutePath(), name).exists()) {
		dir = partFile.absolutePath();
		return true;
	    }
	}
    }

    QStringList list = GlobalConfig::sourceDirs(_data,
						sf->function()->object());
    QStringList::const_iterator it;
    for ( it = list.constBegin(); it != list.constEnd(); ++it ) {
        dir = *it;
        if (searchFileRecursive(dir, name)) return true;
    }

    return false;
}


void SourceView::updateJumpArray(uint lineno, SourceItem* si,
				 bool ignoreFrom, bool ignoreTo)
{
    uint lowLineno, highLineno;
    int iEnd = -1, iStart = -1;

    if (0) qDebug("updateJumpArray(line %d, jump to %s)",
		  lineno,
		  si->lineJump()
		  ? qPrintable(si->lineJump()->lineTo()->name()) : "?" );

    while(_lowListIter != _lowList.end()) {
        TraceLineJump* lj= *_lowListIter;
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
			  qPrintable(lj->lineFrom()->name()),
			  qPrintable(lj->lineTo()->name()));

	    _jump[iStart] = lj;
	}
        _lowListIter++;
    }

    si->setJumpArray(_jump);

    while(_highListIter != _highList.end()) {
        TraceLineJump* lj= *_highListIter;
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

	if (0 && (iEnd>=0))
	    qDebug(" end %d (%s to %s)",
		   iEnd,
		   qPrintable(_jump[iEnd]->lineFrom()->name()),
		   qPrintable(_jump[iEnd]->lineTo()->name()));

	if (0 && lj) qDebug("next end: %s to %s",
			    qPrintable(lj->lineFrom()->name()),
			    qPrintable(lj->lineTo()->name()));

        _highListIter++;

	if (highLineno > lineno)
	    break;
	else {
	    if (iEnd>=0) _jump[iEnd] = 0;
	    iEnd = -1;
	}
    }
    if (iEnd>=0) _jump[iEnd] = 0;
}


// compare functions for jump arrow drawing

void getJumpLines(const TraceLineJump* jump, uint& low, uint& high)
{
    low  = jump->lineFrom()->lineno();
    high = jump->lineTo()->lineno();

    if (low > high) {
        uint t = low;
        low = high;
        high = t;
    }
}

// sort jumps according to lower line number
bool lineJumpLowLessThan(const TraceLineJump* jump1,
                          const TraceLineJump* jump2)
{
    uint line1Low, line1High, line2Low, line2High;

    getJumpLines(jump1, line1Low, line1High);
    getJumpLines(jump2, line2Low, line2High);

    if (line1Low != line2Low) return (line1Low < line2Low);
    // jump ends come before jump starts
    if (line1Low == jump1->lineTo()->lineno()) return true;
    if (line2Low == jump2->lineTo()->lineno()) return false;
    return (line1High < line2High);
}

// sort jumps according to higher line number
bool lineJumpHighLessThan(const TraceLineJump* jump1,
                           const TraceLineJump* jump2)
{
    uint line1Low, line1High, line2Low, line2High;

    getJumpLines(jump1, line1Low, line1High);
    getJumpLines(jump2, line2Low, line2High);

    if (line1High != line2High) return (line1High < line2High);
    // jump ends come before jump starts
    if (line1High == jump1->lineTo()->lineno()) return true;
    if (line2High == jump2->lineTo()->lineno()) return false;
    return (line1Low < line2Low);
}

/* If sourceList is empty we set the source file name into the header,
 * else this code is of a inlined function, and we add "inlined from..."
 */
void SourceView::fillSourceFile(TraceFunctionSource* sf, int fileno)
{
  if (!sf) return;

  if (0) qDebug("Selected Item %s",
                _selectedItem ? qPrintable(_selectedItem->name()) : "(none)");

  TraceLineMap::Iterator lineIt, lineItEnd;
  int nextCostLineno = 0, lastCostLineno = 0;

  bool validSourceFile = (!sf->file()->name().isEmpty());

  TraceLine* sLine = 0;
  if (_selectedItem) {
    if (_selectedItem->type() == ProfileContext::Line)
      sLine = (TraceLine*) _selectedItem;
    if (_selectedItem->type() == ProfileContext::Instr)
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
	    if ((*lineIt).hasCost(_eventType)) break;
	    if (_eventType2 && (*lineIt).hasCost(_eventType2)) break;
	    ++lineIt;
	  }

	  nextCostLineno     = (lineIt == lineItEnd) ? 0 : (*lineIt).lineno();
	  if (nextCostLineno<0) {
	    qDebug() << "SourceView::fillSourceFile: Negative line number "
			<< nextCostLineno;
	    qDebug() << "  Function '" << sf->function()->name() << "'";
	    qDebug() << "  File '" << sf->file()->name() << "'";
	    nextCostLineno = 0;
	  }

      }

      if (nextCostLineno == 0) {
	  new SourceItem(this, this, fileno, 1, false,
			 tr("There is no cost of current selected type associated"));
	  new SourceItem(this, this, fileno, 2, false,
			 tr("with any source line of this function in file"));
	  new SourceItem(this, this, fileno, 3, false,
			 QString("    '%1'").arg(sf->function()->prettyName()));
	  new SourceItem(this, this, fileno, 4, false,
			 tr("Thus, no annotated source can be shown."));
	  return;
      }
  }

  QString filename = sf->file()->shortName();
  QString dir = sf->file()->directory();
  if (!dir.isEmpty())
    filename = dir + '/' + filename;

  if (nextCostLineno>0) {
      // we have debug info... search for source file
      if (searchFile(dir, sf)) {
	  filename = dir + '/' + sf->file()->shortName();
	  // no need to search again
	  sf->file()->setDirectory(dir);
      }
      else
	  nextCostLineno = 0;
  }

  // do it here, because the source directory could have been set before
  if (topLevelItemCount()==0) {
      headerItem()->setText(4, validSourceFile ?
                                tr("Source ('%1')").arg(filename) :
                                tr("Source (unknown)"));
  }
  else {
    new SourceItem(this, this, fileno, 0, true,
                   validSourceFile ?
                   tr("--- Inlined from '%1' ---").arg(filename) :
                   tr("--- Inlined from unknown source ---"));
  }

  if (nextCostLineno == 0) {
    new SourceItem(this, this, fileno, 1, false,
                   tr("There is no source available for the following function:"));
    new SourceItem(this, this, fileno, 2, false,
                   QString("    '%1'").arg(sf->function()->prettyName()));
    if (sf->file()->name().isEmpty()) {
      new SourceItem(this, this, fileno, 3, false,
                     tr("This is because no debug information is present."));
      new SourceItem(this, this, fileno, 4, false,
                     tr("Recompile source and redo the profile run."));
      if (sf->function()->object()) {
	new SourceItem(this, this, fileno, 5, false,
                       tr("The function is located in this ELF object:"));
	new SourceItem(this, this, fileno, 6, false,
                       QString("    '%1'")
                       .arg(sf->function()->object()->prettyName()));
      }
    }
    else {
      new SourceItem(this, this, fileno, 3, false,
                     tr("This is because its source file cannot be found:"));
      new SourceItem(this, this, fileno, 4, false,
                     QString("    '%1'").arg(sf->file()->name()));
      new SourceItem(this, this, fileno, 5, false,
                     tr("Add the folder of this file to the source folder list."));
      new SourceItem(this, this, fileno, 6, false,
                     tr("The list can be found in the configuration dialog."));
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
	if ((*nextIt).hasCost(_eventType)) break;
	if (_eventType2 && (*nextIt).hasCost(_eventType2)) break;
	++nextIt;
      }

      TraceLineJumpList jlist = (*it).lineJumps();
      foreach(TraceLineJump* lj, jlist) {
	  if (lj->executedCount()==0) continue;
	  // skip jumps to next source line with cost
	  //if (lj->lineTo() == &(*nextIt)) continue;

	  _lowList.append(lj);
	  _highList.append(lj);
      }
      it = nextIt;
      if (it == lineItEnd) break;
  }
  qSort(_lowList.begin(), _lowList.end(), lineJumpLowLessThan);
  qSort(_highList.begin(), _highList.end(), lineJumpHighLessThan);
  _lowListIter = _lowList.begin(); // iterators to list start
  _highListIter = _highList.begin();
  _jump.resize(0);

  char buf[160];
  bool inside = false, skipLineWritten = true;
  int readBytes;
  int fileLineno = 0;
  SubCost most = 0;

  QList<QTreeWidgetItem*> items;
  TraceLine* currLine;
  SourceItem *si, *si2, *item = 0, *first = 0, *selected = 0;
  QFile file(filename);
  bool fileEndReached = false;
  if (!file.open(QIODevice::ReadOnly)) return;
  while (1) {
    readBytes=file.readLine(buf, sizeof( buf ));
    if (readBytes<=0) {
      // for nice empty 4 lines after function with EOF
      buf[0] = 0;
      if (readBytes<0) fileEndReached = true;
    }

    if ((readBytes >0) && (buf[readBytes-1] != '\n')) {
        /* Something was read but not ending in newline. I.e.
         * - buffer was not big enough: discard rest of line, add "..."
         * - this is last line of file, not ending in newline
         * NB: checking for '\n' is enough for all systems.
         */
        int r;
        char buf2[32];
        bool somethingRead = false;
        while(1) {
            r = file.readLine(buf2, sizeof(buf2));
            if ((r<=0) || (buf2[r-1] == '\n')) break;
            somethingRead = true;
        }
        if (somethingRead) {
            // add dots as sign that we truncated the line
            Q_ASSERT(readBytes>3);
            buf[readBytes-1] = buf[readBytes-2] = buf[readBytes-3] = '.';
        }
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
	  if ((*lineIt).hasCost(_eventType)) break;
	  if (_eventType2 && (*lineIt).hasCost(_eventType2)) break;
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
	      (fileLineno < nextCostLineno - GlobalConfig::noCostInside()) ))
	    inside = false;
    }

    int context = GlobalConfig::context();

    if ( ((lastCostLineno==0) || (fileLineno > lastCostLineno + context)) &&
	 ((nextCostLineno==0) || (fileLineno < nextCostLineno - context))) {
	if ((lineIt == lineItEnd) || fileEndReached) break;

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

    QString s = QString(buf);
    if(s.size() > 0 && s.at(s.length()-1) == '\r')
        s = s.left(s.length()-1);
    si = new SourceItem(this, 0,
                        fileno, fileLineno, inside, s,
                        currLine);
    items.append(si);

    if (!currLine) continue;

    if (!selected && (currLine == sLine)) selected = si;
    if (!first) first = si;

    if (currLine->subCost(_eventType) > most) {
      item = si;
      most = currLine->subCost(_eventType);
    }

    si->setExpanded(true);
    foreach(TraceLineCall* lc,  currLine->lineCalls()) {
	if ((lc->subCost(_eventType)==0) &&
	    (lc->subCost(_eventType2)==0)) continue;

      if (lc->subCost(_eventType) > most) {
        item = si;
        most = lc->subCost(_eventType);
      }

      si2 = new SourceItem(this, si, fileno, fileLineno, currLine, lc);

      if (!selected && (lc->call()->called() == _selectedItem))
	  selected = si2;
    }

    foreach(TraceLineJump* lj, currLine->lineJumps()) {
	if (lj->executedCount()==0) continue;

	new SourceItem(this, si, fileno, fileLineno, currLine, lj);
    }
  }

  file.close();

  // Resize column 1/2 to contents
  header()->setResizeMode(1, QHeaderView::ResizeToContents);
  header()->setResizeMode(2, QHeaderView::ResizeToContents);

  setSortingEnabled(false);
  addTopLevelItems(items);
  this->expandAll();
  setSortingEnabled(true);
  // always reset to line number sort
  sortByColumn(0, Qt::AscendingOrder);
  header()->setSortIndicatorShown(false);

  // Reallow interactive column size change after resizing to content
  header()->setResizeMode(1, QHeaderView::Interactive);
  header()->setResizeMode(2, QHeaderView::Interactive);

  if (selected) item = selected;
  if (item) first = item;
  if (first) {
      scrollToItem(first);
      _inSelectionUpdate = true;
      setCurrentItem(first);
      _inSelectionUpdate = false;
  }

  // for arrows: go down the list according to list sorting
  QTreeWidgetItem *item1, *item2;
  for (int i=0; i<topLevelItemCount(); i++) {
      item1 = topLevelItem(i);
      si = (SourceItem*)item1;
      updateJumpArray(si->lineno(), si, true, false);

      for (int j=0; j<item1->childCount(); j++) {
          item2 = item1->child(j);
          si2 = (SourceItem*)item2;
          if (si2->lineJump())
              updateJumpArray(si->lineno(), si2, false, true);
          else
              si2->setJumpArray(_jump);
      }
  }

  if (arrowLevels())
      //fix this: setColumnWidth(3, 10 + 6*arrowLevels() + itemMargin() * 2);
      setColumnWidth(3, 10 + 6*arrowLevels() + 2);
  else
      setColumnWidth(3, 0);
}


void SourceView::headerClicked(int col)
{
    if (col == 0) {
        sortByColumn(col, Qt::AscendingOrder);
    }
    //All others but Source Text column Descending
    else if (col !=4) {
        sortByColumn(col, Qt::DescendingOrder);
    }
}

#include "sourceview.moc"
