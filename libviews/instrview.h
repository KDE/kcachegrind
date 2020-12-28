/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-201 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
                       QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    int arrowLevels() { return _arrowLevels; }

    void restoreOptions(const QString& prefix, const QString& postfix) override;
    void saveOptions(const QString& prefix, const QString& postfix) override;

protected Q_SLOTS:
    void context(const QPoint &);
    void selectedSlot(QTreeWidgetItem*, QTreeWidgetItem*);
    void activatedSlot(QTreeWidgetItem*,int);
    void headerClicked(int);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;
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
