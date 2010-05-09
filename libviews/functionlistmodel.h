/* This file is part of KCachegrind.
   Copyright (C) 2010 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
    ~FunctionListModel();

    /* Data to show: all functions from <data>
     * - which are part of function group <group>
     * - whose name contains <filter>
     */
    void setData(TraceData* data, TraceCostItem* group, QString filter,
                 EventType* eventType);

    // reimplemented from QAbstractItemModel
    QVariant data(const QModelIndex&, int) const;
    Qt::ItemFlags flags(const QModelIndex&) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column = 0,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    void sort(int column, Qt::SortOrder order);

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

    void computeTop();

    QList<QVariant> _headerData;
    EventType *_eventType;
    ProfileContext::Type _groupType;
    int _maxCount;

    QList<TraceFunction*> _list;
    QList<TraceFunction*> _topList;

    int _sortColumn;
    Qt::SortOrder _sortOrder;
    QRegExp _filter;
    QString _filterString;
};

#endif
