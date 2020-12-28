/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * PartSelection for KCachegrind
 * For part file selection, to be put into a QDockWindow
 */

#ifndef PARTSELECTION_H
#define PARTSELECTION_H

#include <QWidget>
#include <QPoint>

#include "tracedata.h"
#include "traceitemview.h"

class QLabel;
class TraceData;
class TreeMapItem;
class PartAreaWidget;

class PartSelection: public QWidget, public TraceItemView
{
    Q_OBJECT

public:
    explicit PartSelection(TopLevelBase*, QWidget* parent = nullptr);

    QWidget* widget() override { return this; }
    QString whatsThis() const override;
    void setData(TraceData*) override;

    PartAreaWidget* graph() { return _partAreaWidget; }

    void saveOptions(const QString& prefix, const QString& postfix) override;
    void restoreOptions(const QString& prefix, const QString& postfix) override;

Q_SIGNALS:
    void partsHideSelected();
    void partsUnhideAll();

public Q_SLOTS:
    void selectionChanged();
    void doubleClicked(TreeMapItem*);
    void itemSelected();
    void contextMenuRequested(TreeMapItem*, const QPoint &);
    void currentChangedSlot(TreeMapItem*, bool);

    void hiddenPartsChangedSlot(const TracePartList& list);
    void showInfo(bool);

private:
    // reimplementations of TraceItemView
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;

    // helper for doUpdate
    void selectParts(const TracePartList& list);
    void fillInfo();

    bool _showInfo;
    bool _diagramMode;
    bool _drawFrames;
    bool _inSelectionUpdate;

    PartAreaWidget* _partAreaWidget;
    QLabel* _rangeLabel;
};

#endif
