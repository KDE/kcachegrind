/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Source View
 */

#ifndef SOURCEVIEW_H
#define SOURCEVIEW_H

#include <QTreeWidget>
#include "traceitemview.h"

class SourceItem;

class SourceView : public QTreeWidget, public TraceItemView
{
    friend class SourceItem;

    Q_OBJECT

public:
    explicit SourceView(TraceItemView* parentView,
                        QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    int arrowLevels() { return _arrowLevels; }

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
    void updateJumpArray(uint,SourceItem*,bool,bool);
    bool searchFile(QString&, TraceFunctionSource*);
    void fillSourceFile(TraceFunctionSource*, int);

    bool _inSelectionUpdate;

    // arrows
    int _arrowLevels;
    // temporary needed on creation...
    QVector<TraceLineJump*> _jump;
    TraceLineJumpList _lowList, _highList;
    TraceLineJumpList::iterator _lowListIter, _highListIter;
};

#endif
