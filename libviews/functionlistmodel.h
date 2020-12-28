/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2010-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef FUNCTIONLISTMODEL_H
#define FUNCTIONLISTMODEL_H

#include <QAbstractItemModel>
#include <QPixmap>
#include <QRegExp>
#include <QList>

#include "tracedata.h"
#include "subcost.h"


class FunctionListModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    FunctionListModel();
    ~FunctionListModel() override;

    /* Data to show: all functions from <data>
     * - which are part of function group <group>
     * - whose name contains <filter>
     */
    void resetModelData(TraceData* data, TraceCostItem* group, QString filter,
                        EventType* eventType);

    // reimplemented from QAbstractItemModel
    QVariant data(const QModelIndex&, int) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column = 0,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order) override;

    void setFilter(QString filter);
    void setEventType(EventType*);
    void setMaxCount(int);

    TraceFunction* function(const QModelIndex &index);
    // get index of an entry showing a function, optionally adding it if needed
    QModelIndex indexForFunction(TraceFunction *f, bool add = false);

    class FunctionLessThan
    {
    public:
        FunctionLessThan(int column, Qt::SortOrder order, EventType* et)
        { _column = column; _order = order; _eventType = et; }

        bool operator()(TraceFunction *left, TraceFunction *right);

    private:
        int _column;
        Qt::SortOrder _order;
        EventType* _eventType;
    };

private:
    QString getName(TraceFunction *f) const;
    QPixmap getNamePixmap(TraceFunction  *f) const;
    QString getInclCost(TraceFunction *f) const;
    QPixmap getInclPixmap(TraceFunction *f) const;
    QString getSelfCost(TraceFunction *f) const;
    QPixmap getSelfPixmap(TraceFunction *f) const;
    QString getCallCount(TraceFunction *f) const;
    QString getLocation(TraceFunction *f) const;
    QString getSkippedCost(TraceFunction *f, QPixmap *pixmap) const;

    // compute the list of candidates to show, ignoring order
    void computeFilteredList();
    // computes entries to show from candidates using current order
    void computeTopList();

    QList<QVariant> _headerData;
    EventType *_eventType;
    ProfileContext::Type _groupType;
    int _maxCount;

    QList<TraceFunction*> _list;
    QList<TraceFunction*> _filteredList;
    QList<TraceFunction*> _topList;

    // functions with max values at col.0/1/2 from candidate list:
    // these are always shown to have same column widths when resorting
    TraceFunction *_max0, *_max1, *_max2;

    int _sortColumn;
    Qt::SortOrder _sortOrder;
    QRegExp _filter;
    QString _filterString;
};

#endif
