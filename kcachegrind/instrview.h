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

#ifndef INSTRVIEW_H
#define INSTRVIEW_H

#include <qlistview.h>
#include "traceitemview.h"

class InstrItem;

class InstrView : public QListView, public TraceItemView
{
  friend class InstrItem;

  Q_OBJECT

public:
  InstrView(TraceItemView* parentView,
            QWidget* parent = 0, const char* name = 0);

  virtual QWidget* widget() { return this; }
  QString whatsThis() const;

  void readViewConfig(KConfig*, QString prefix, QString postfix, bool);
  void saveViewConfig(KConfig*, QString prefix, QString postfix, bool);

protected:
  int arrowLevels() { return _arrowLevels; }
  void paintEmptyArea( QPainter *, const QRect & );

private slots:
  void context(QListViewItem*, const QPoint &, int);
  void selectedSlot(QListViewItem *);
  void activatedSlot(QListViewItem *);

private:
  TraceItem* canShow(TraceItem*);
  void doUpdate(int);
  void refresh();
  void setColumnWidths();
  void fillInstr();
  void updateJumpArray(Addr,InstrItem*,bool,bool);
  bool fillInstrRange(TraceFunction*,
                      TraceInstrMap::Iterator,TraceInstrMap::Iterator);
  void updateInstrItems();

  bool _inSelectionUpdate;

  // arrows
  int _arrowLevels;
  // temporary needed on creation...
  QMemArray<TraceInstrJump*> _jump;
  TraceInstrJumpList _lowList, _highList;

  // remember width of hex code column if hidden
  int _lastHexCodeWidth;

  // widget options
  bool _showHexCode;
};

#endif
