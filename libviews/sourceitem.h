/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2011-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
               TraceLine* line = nullptr);

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
    bool operator< ( const QTreeWidgetItem & other ) const override;

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
    Q_OBJECT
public:
    explicit SourceItemDelegate(SourceView *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex & index ) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    QWidget* createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

protected:
    void paintArrows(QPainter *p,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) const;

    SourceView* _parent;
};


#endif // SOURCEITEM_H
