/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of instruction view.
 */

#ifndef INSTRITEM_H
#define INSTRITEM_H


#include <QTreeWidget>
#include <QItemDelegate>

#include "tracedata.h"

class InstrView;

class InstrItem: public QTreeWidgetItem
{

public:
    // for messages
    InstrItem(InstrView* iv, QTreeWidget* parent,
              Addr addr, const QString&);

    // for instruction lines
    InstrItem(InstrView* iv, QTreeWidget* parent,
              Addr addr, bool inside,
              const QString&, const QString&, const QString&,
              TraceInstr* instr);

    // for call instr
    InstrItem(InstrView* iv, QTreeWidgetItem* parent, Addr addr,
              TraceInstr* instr, TraceInstrCall* instrCall);

    // for jump lines
    InstrItem(InstrView* iv, QTreeWidgetItem* parent, Addr addr,
              TraceInstr* instr, TraceInstrJump* instrJump);

    Addr addr() const { return _addr; }
    bool inside() const { return _inside; }
    TraceInstr* instr() const { return _instr; }
    TraceInstrCall* instrCall() const { return _instrCall; }
    TraceInstrJump* instrJump() const { return _instrJump; }
    TraceInstrJump* jump(int i) const { return _jump[i]; }
    int jumpCount() const { return _jump.size(); }
    bool operator< ( const QTreeWidgetItem & other ) const override;

    void updateGroup();
    void updateCost();

    // arrow lines
    void setJumpArray(const QVector<TraceInstrJump*>& a);

private:
    InstrView* _view;
    SubCost _pure, _pure2;
    Addr _addr;
    TraceInstr* _instr;
    TraceInstrJump* _instrJump;
    TraceInstrCall* _instrCall;
    bool _inside;

    QVector<TraceInstrJump*> _jump;
};

// Delegate for drawing the arrows column

class InstrItemDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit InstrItemDelegate(InstrView *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex & index ) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

protected:
    void paintArrows(QPainter *p,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) const;

    InstrView* _parent;
};


#endif // INSTRITEM_H
