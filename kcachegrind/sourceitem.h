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
 * Items of source view.
 */

#ifndef SOURCEITEM_H
#define SOURCEITEM_H

#include <qlistview.h>
#include "tracedata.h"

class SourceView;

class SourceItem: public QListViewItem
{
public:
  // for source lines
  SourceItem(SourceView* sv, QListView* parent, 
	     int fileno, unsigned int lineno,
	     bool inside, const QString& src,
             TraceLine* line = 0);

  // for call lines
  SourceItem(SourceView* sv, QListViewItem* parent, 
	     int fileno, unsigned int lineno,
             TraceLine* line, TraceLineCall* lineCall);

  // for jump lines
  SourceItem(SourceView* sv, QListViewItem* parent, 
	     int fileno, unsigned int lineno,
             TraceLine* line, TraceLineJump* lineJump);

  uint lineno() const { return _lineno; }
  int fileNumber() const { return _fileno; }
  bool inside() const { return _inside; }
  TraceLine* line() const { return _line; }
  TraceLineCall* lineCall() const { return _lineCall; }
  TraceLineJump* lineJump() const { return _lineJump; }

  int compare(QListViewItem * i, int col, bool ascending ) const;

  void paintCell( QPainter *p, const QColorGroup &cg,
                  int column, int width, int alignment );
  int width( const QFontMetrics& fm,
             const QListView* lv, int c ) const;
  void updateGroup();
  void updateCost();

  // arrow lines
  void setJumpArray(const QMemArray<TraceLineJump*>& a);

protected:
  void paintArrows(QPainter *p, const QColorGroup &cg, int width);
  QMemArray<TraceLineJump*> _jump;

private:
  SourceView* _view;
  SubCost _pure, _pure2;
  uint _lineno;
  int _fileno; // for line sorting (even with multiple files)
  bool _inside;
  TraceLine* _line;
  TraceLineJump* _lineJump;
  TraceLineCall* _lineCall;
};

#endif
