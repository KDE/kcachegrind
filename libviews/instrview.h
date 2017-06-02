/* This file is part of KCachegrind.
   Copyright (c) 2003-201 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#ifndef INSTRVIEW_H
#define INSTRVIEW_H

#include <QTreeWidget>

#include "traceitemview.h"

class InstrItem;

class InstrView : public QTreeWidget, public TraceItemView
{
    friend class InstrItem;

    Q_OBJECT

public:
    explicit InstrView(TraceItemView* parentView,
                       QWidget* parent = 0);

    QWidget* widget() Q_DECL_OVERRIDE { return this; }
    QString whatsThis() const Q_DECL_OVERRIDE;
    int arrowLevels() { return _arrowLevels; }

    void restoreOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;
    void saveOptions(const QString& prefix, const QString& postfix) Q_DECL_OVERRIDE;

protected slots:
    void context(const QPoint &);
    void selectedSlot(QTreeWidgetItem*, QTreeWidgetItem*);
    void activatedSlot(QTreeWidgetItem*,int);
    void headerClicked(int);

protected:
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

private:
    CostItem* canShow(CostItem*) Q_DECL_OVERRIDE;
    void doUpdate(int, bool) Q_DECL_OVERRIDE;
    void refresh();
    void setColumnWidths();
    bool searchFile(QString&, TraceObject*);
    void fillInstr();
    void updateJumpArray(Addr,InstrItem*,bool,bool);
    bool fillInstrRange(TraceFunction*,
                        TraceInstrMap::Iterator,TraceInstrMap::Iterator);

    bool _inSelectionUpdate;

    // arrows
    int _arrowLevels;
    // temporary needed on creation...
    QVector<TraceInstrJump*> _jump;
    TraceInstrJumpList _lowList, _highList;
    TraceInstrJumpList::iterator _lowListIter, _highListIter;

    // remember width of hex code column if hidden
    int _lastHexCodeWidth;

    // widget options
    bool _showHexCode;
};

#endif
