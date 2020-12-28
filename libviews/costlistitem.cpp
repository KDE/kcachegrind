/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include "costlistitem.h"

#include <math.h>

#include <QPixmap>

#include "listutils.h"
#include "coverage.h"
#include "globalguiconfig.h"


// CostListItem


CostListItem::CostListItem(QTreeWidget* parent, TraceCostItem* costItem,
                           EventType* et, int size)
    :QTreeWidgetItem(parent)
{
    _groupSize = size;
    _skipped = 0;
    _costItem = costItem;
    setEventType(et);

    setTextAlignment(0, Qt::AlignRight);

    if (costItem) {
        updateName();
        setIcon(1, colorPixmap(10, 10,
                               GlobalGUIConfig::groupColor(_costItem)));
    }
}

CostListItem::CostListItem(QTreeWidget* parent, int skipped,
                           TraceCostItem* costItem, EventType* et)
    :QTreeWidgetItem(parent)
{
    _skipped = skipped;
    _costItem = costItem;
    setEventType(et);

    setTextAlignment(0, Qt::AlignRight);

    //~ singular (%n item skipped)
    //~ plural (%n items skipped)
    setText(1, QObject::tr("(%n item(s) skipped)", "", _skipped));
}

void CostListItem::setEventType(EventType* et)
{
    _eventType = et;
    update();
}

void CostListItem::updateName()
{
    if (!_costItem) return;

    QString n = _costItem->prettyName();
    if (_groupSize>=0) n += QStringLiteral(" (%1)").arg(_groupSize);

    setText(1, n);
}

void CostListItem::setSize(int s)
{
    _groupSize = s;
    updateName();
}

void CostListItem::update()
{
    if (!_costItem) return;
    TraceData* d = _costItem->data();

    double total = d->subCost(_eventType);
    if (total == 0.0) {
        setText(0, QStringLiteral("---"));
        setIcon(0, QPixmap());
        return;
    }

    _pure = _costItem->subCost(_eventType);
    double pure  = 100.0 * _pure / total;
    QString str;
    if (GlobalConfig::showPercentage())
        str = QStringLiteral("%1").arg(pure, 0, 'f', GlobalConfig::percentPrecision());
    else
        str = _costItem->prettySubCost(_eventType);

    if (_skipped) {
        // special handling for skip entries...
        setText(0, QStringLiteral("< %1").arg(str));
        return;
    }

    setText(0, str);
    setIcon(0, costPixmap(_eventType, _costItem, total, false));
}

bool CostListItem::operator< ( const QTreeWidgetItem & other ) const
{
    const CostListItem* fi1 = this;
    const CostListItem* fi2 = (CostListItem*) &other;
    int col = treeWidget()->sortColumn();

    // a skip entry is always sorted last
    if (fi1->_skipped) return true;
    if (fi2->_skipped) return false;

    if (col==0)
        return (fi1->_pure < fi2->_pure);

    return QTreeWidgetItem::operator <(other);
}
