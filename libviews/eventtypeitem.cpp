/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Items of event type view.
 */

#include "eventtypeitem.h"

#include <QPixmap>

#include "globalconfig.h"
#include "listutils.h"


// EventTypeItem


EventTypeItem::EventTypeItem(TraceCostItem* costItem,
                             EventType* ct, ProfileContext::Type gt)
{
    _costItem = costItem;
    _eventType = ct;
    _groupType = gt;

    setTextAlignment(1, Qt::AlignRight);
    setTextAlignment(2, Qt::AlignRight);
    setTextAlignment(3, Qt::AlignRight);
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (ct) {
        setText(0, ct->longName());
        setText(3, ct->name());
        setText(5, ct->parsedFormula());
        if (!ct->isReal()) {
            setText(4, QStringLiteral("="));
            // we have a virtual type: allow editing
            // FIXME: How to enable this only for columns 0,3,5 ?!
            setFlags(flags() | Qt::ItemIsEditable);
        }
    }
    else {
        setText(0, QObject::tr("Unknown Type"));
    }
    update();
}

void EventTypeItem::setGroupType(ProfileContext::Type gt)
{
    if (_groupType == gt) return;

    _groupType = gt;
    update();
}

void EventTypeItem::update()
{
    TraceData* d = _costItem ? _costItem->data() : nullptr;
    double total = d ? ((double)d->subCost(_eventType)) : 0.0;

    if (total == 0.0) {
        setText(1, QStringLiteral("-"));
        setIcon(1, QIcon());
        setText(2, QStringLiteral("-"));
        setIcon(2, QIcon());
        return;
    }

    TraceFunction* f = (_costItem && _costItem->type()==ProfileContext::Function) ?
                           (TraceFunction*)_costItem : nullptr;

    ProfileCostArray* selfTotalCost = f ? f->data() : d;
    if (f && GlobalConfig::showExpanded()) {
        ProfileCostArray* parent = nullptr;
        switch(_groupType) {
        case ProfileContext::Object:        parent = f->object(); break;
        case ProfileContext::Class:         parent = f->cls(); break;
        case ProfileContext::File:          parent = f->file(); break;
        case ProfileContext::FunctionCycle: parent = f->cycle(); break;
        default: break;
        }
        if (parent) selfTotalCost = parent;
    }
    if (_costItem && _costItem->type()==ProfileContext::FunctionCycle) {
        f = (TraceFunction*)_costItem;
        selfTotalCost = f->data();
    }

    double selfTotal = selfTotalCost->subCost(_eventType);

    // for all cost items there is a self cost
    _pure = _costItem ? _costItem->subCost(_eventType) : SubCost(0);
    double pure  = 100.0 * _pure / selfTotal;
    if (GlobalConfig::showPercentage()) {
        setText(2, QStringLiteral("%1")
                .arg(pure, 0, 'f', GlobalConfig::percentPrecision()));
    }
    else if (_costItem)
        setText(2, _costItem->prettySubCost(_eventType));

    setIcon(2, QIcon(costPixmap(_eventType, _costItem, selfTotal, false)));

    if (!f) {
        setText(1, QStringLiteral("-"));
        setIcon(1, QIcon());
        return;
    }

    _sum = f->inclusive()->subCost(_eventType);
    double sum  = 100.0 * _sum / total;
    if (GlobalConfig::showPercentage()) {
        setText(1, QStringLiteral("%1")
                .arg(sum, 0, 'f', GlobalConfig::percentPrecision()));
    }
    else
        setText(1, _sum.pretty());

    setIcon(1, QIcon(costPixmap(_eventType, f->inclusive(), total, false)));
}

bool EventTypeItem::operator<(const QTreeWidgetItem &other) const
{
    int col = treeWidget()->sortColumn();
    EventTypeItem* o = (EventTypeItem*) &other;
    if (col==0)
        return _sum < o->_sum;
    if (col==1)
        return _pure < o->_pure;

    return QTreeWidgetItem::operator<(other);
}

QVariant EventTypeItem::data(int column, int role) const
{
    if ((column == 5) && (role == Qt::EditRole))
        return QVariant(_eventType->formula());
    return QTreeWidgetItem::data(column, role);
}


