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
 * Items of source view.
 */

#ifndef SOURCEITEM_H
#define SOURCEITEM_H

#include <QTreeWidget>
#include <QItemDelegate>

#include "tracedata.h"

class SourceView;

class SourceItem: public QTreeWidgetItem
{
public:
  // for source lines
  SourceItem(SourceView* sv, QTreeWidget* parent,
	     int fileno, unsigned int lineno,
	     bool inside, const QString& src,
             TraceLine* line = 0);

  // for call lines
  SourceItem(SourceView* sv, QTreeWidgetItem* parent,
	     int fileno, unsigned int lineno,
             TraceLine* line, TraceLineCall* lineCall);

  // for jump lines
  SourceItem(SourceView* sv, QTreeWidgetItem* parent,
	     int fileno, unsigned int lineno,
             TraceLine* line, TraceLineJump* lineJump);

  uint lineno() const { return _lineno; }
  int fileNumber() const { return _fileno; }
  bool inside() const { return _inside; }
  TraceLine* line() const { return _line; }
  TraceLineCall* lineCall() const { return _lineCall; }
  TraceLineJump* lineJump() const { return _lineJump; }
  TraceLineJump* jump(int i) const { return _jump[i]; }
  int jumpCount() const { return _jump.size(); }
  bool operator< ( const QTreeWidgetItem & other ) const;

  void updateGroup();
  void updateCost();

  // arrow lines
  void setJumpArray(const QVector<TraceLineJump*>& a);

  QVector<TraceLineJump*> _jump;

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


// Delegate for drawing the arrows column

class SourceItemDelegate : public QItemDelegate
{
public:
    SourceItemDelegate(SourceView *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex & index ) const;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

protected:
    void paintArrows(QPainter *p,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) const;

    SourceView* _parent;
};


#endif // SOURCEITEM_H
