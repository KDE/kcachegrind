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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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
    explicit PartSelection(TopLevelBase*, QWidget* parent = 0);

    virtual QWidget* widget() { return this; }
    QString whatsThis() const;
    void setData(TraceData*);

    PartAreaWidget* graph() { return _partAreaWidget; }

    void saveOptions(const QString& prefix, const QString& postfix);
    void restoreOptions(const QString& prefix, const QString& postfix);

signals:
    void partsHideSelected();
    void partsUnhideAll();

public slots:
    void selectionChanged();
    void doubleClicked(TreeMapItem*);
    void itemSelected();
    void contextMenuRequested(TreeMapItem*, const QPoint &);
    void currentChangedSlot(TreeMapItem*, bool);

    void hiddenPartsChangedSlot(const TracePartList& list);
    void showInfo(bool);

private:
    // reimplementations of TraceItemView
    CostItem* canShow(CostItem*);
    void doUpdate(int, bool);

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
