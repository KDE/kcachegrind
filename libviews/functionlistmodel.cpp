/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2010-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include "functionlistmodel.h"

#include "globalguiconfig.h"
#include "listutils.h"

/* helper for setting function filter: we want it to work similar to globbing:
 * - escape most special characters in regexps: ( ) [ ] | . \
 * - change * to .*
 */
QString glob2Regex(QString pattern)
{
    pattern.replace(QChar('\\'),QLatin1String("\\\\"));
    pattern.replace(QChar('('),QLatin1String("\\("));
    pattern.replace(QChar(')'),QLatin1String("\\)"));
    pattern.replace(QChar('['),QLatin1String("\\["));
    pattern.replace(QChar(']'),QLatin1String("\\]"));
    pattern.replace(QChar('|'),QLatin1String("\\|"));
    pattern.replace(QChar('.'),QLatin1String("\\."));
    pattern.replace(QChar('*'),QLatin1String(".*"));

    return pattern;
}


//
// FunctionListModel
//

FunctionListModel::FunctionListModel()
    : QAbstractItemModel(nullptr)
{
    _maxCount = 300;
    _sortColumn = 0;
    _sortOrder = Qt::DescendingOrder;

    _headerData
            << tr("Incl.")
            << tr("Self")
            << tr("Called")
            << tr("Function")
            << tr("Location");

    _max0 = _max1 = _max2 = nullptr;
}

FunctionListModel::~FunctionListModel()
{}

int FunctionListModel::columnCount(const QModelIndex& parent) const
{
    return (parent.isValid()) ? 0 : 5;
}

int FunctionListModel::rowCount(const QModelIndex& parent ) const
{
    if (parent.isValid()) return 0;

    int rowCount = _topList.count();
    // add one more row if functions are skipped
    if (_topList.count() < _filteredList.count()) rowCount++;
    return rowCount;
}

TraceFunction* FunctionListModel::function(const QModelIndex& index)
{
    if (!index.isValid()) return nullptr;

    return (TraceFunction*) index.internalPointer();
}

QVariant FunctionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    // the skipped items entry
    if ( (_topList.count() < _filteredList.count()) &&
         (index.row() == _topList.count()) ) {
        if( (role != Qt::DisplayRole) || (index.column() != 3))
            return QVariant();

        return tr("(%1 function(s) skipped)").arg(_filteredList.count() - _topList.count());
    }

    TraceFunction *f = (TraceFunction*) index.internalPointer();
    Q_ASSERT(f != nullptr);
    switch(role) {
    case Qt::TextAlignmentRole:
        return (index.column()<3) ? Qt::AlignRight : Qt::AlignLeft;

    case Qt::DecorationRole:
        switch (index.column()) {
        case 0:
            return getInclPixmap(f);
        case 1:
            return getSelfPixmap(f);
        case 3:
            return getNamePixmap(f);
        default:
            break;
        }
        break;

    case Qt::DisplayRole:
        switch (index.column()) {
        case 0:
            return getInclCost(f);
        case 1:
            return getSelfCost(f);
        case 2:
            return getCallCount(f);
        case 3:
            return getName(f);
        case 4:
            return getLocation(f);
        default:
            break;
        }

    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags FunctionListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant FunctionListModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole))
        return QVariant();

    return _headerData.value(section);
}

QModelIndex FunctionListModel::index(int row, int column,
                                     const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();

    //the skipped items entry
    if ( (_topList.count() < _list.count()) && (row == _topList.count()) )
        return createIndex(row, column);

    return createIndex(row, column, (void*)_topList[row]);
}

QModelIndex FunctionListModel::indexForFunction(TraceFunction *f, bool add)
{
    if (!f) return QModelIndex();

    int row = _topList.indexOf(f);
    if (row<0) {
        // we only add a function from _list matching the filter
        if ( !add ||
             !_filteredList.contains(f) ) return QModelIndex();

        // find insertion point with current list order
        FunctionLessThan lessThan(_sortColumn, _sortOrder, _eventType);
        QList<TraceFunction*>::iterator insertPos;
        insertPos = std::lower_bound(_topList.begin(), _topList.end(),
                                     f, lessThan);
        row = insertPos - _topList.begin();
        beginInsertRows(QModelIndex(), row, row);
        _topList.insert(row, f);
        endInsertRows();
    }

    return createIndex(row, 0, (void*)f);
}

QModelIndex FunctionListModel::parent(const QModelIndex& /*index*/ ) const
{
    /* only toplevel items */
    return QModelIndex();
}

void FunctionListModel::sort(int column, Qt::SortOrder order)
{
    _sortColumn = column;
    _sortOrder = order;
    computeTopList();
}

void FunctionListModel::setFilter(QString filterString)
{
    if (_filterString == filterString) return;
    _filterString = filterString;

    _filter = QRegularExpression(glob2Regex(_filterString),
                                 QRegularExpression::CaseInsensitiveOption);
    computeFilteredList();
    computeTopList();
}

void FunctionListModel::setEventType(EventType* et)
{
    _eventType = et;
    // needed to recalculate max value entries
    computeFilteredList();
    computeTopList();
}

void FunctionListModel::setMaxCount(int c)
{
    if (_maxCount == c) return;
    _maxCount = c;
    computeTopList();
}

void FunctionListModel::resetModelData(TraceData *data,
                                       TraceCostItem *group, QString filterString,
                                       EventType * eventType)
{
    _eventType = eventType;

    if (!group) {
        _list.clear();
        _groupType = ProfileContext::Function;
        if (data) {
            TraceFunctionMap::iterator i = data->functionMap().begin();
            while (i != data->functionMap().end()) {
                _list.append(&(i.value()));
                ++i;
            }
            foreach(TraceFunction* f, data->functionCycles())
                _list.append(f);
        }
    }
    else {
        _groupType = group->type();
        switch(_groupType) {
        case ProfileContext::Object:
        {
            TraceObject* o = dynamic_cast<TraceObject*>(group);
            Q_ASSERT(o != nullptr);
            _list = o->functions();
        }
            break;

        case ProfileContext::Class:
        {
            TraceClass* c = dynamic_cast<TraceClass*>(group);
            Q_ASSERT(c != nullptr);
            _list = c->functions();
        }
            break;

        case ProfileContext::File:
        {
            TraceFile* f = dynamic_cast<TraceFile*>(group);
            Q_ASSERT(f != nullptr);
            _list = f->functions();
        }
            break;

        case ProfileContext::FunctionCycle:
        {
            TraceFunctionCycle* c = dynamic_cast<TraceFunctionCycle*>(group);
            Q_ASSERT(c != nullptr);
            _list = c->members();
        }
            break;

        default:
            _list.clear();
            break;
        }
    }

    _filterString = filterString;
    _filter = QRegularExpression(glob2Regex(_filterString),
                                 QRegularExpression::CaseInsensitiveOption);

    computeFilteredList();
    computeTopList();
}

void FunctionListModel::computeFilteredList()
{
    FunctionLessThan lessThan0(0, Qt::AscendingOrder, _eventType);
    FunctionLessThan lessThan1(1, Qt::AscendingOrder, _eventType);
    FunctionLessThan lessThan2(2, Qt::AscendingOrder, _eventType);

    // reset max functions
    _max0 = nullptr;
    _max1 = nullptr;
    _max2 = nullptr;

    _filteredList.clear();
    int index = 0;
    foreach(TraceFunction* f, _list) {
        if (!_filterString.isEmpty() && _filter.isValid())
            if (!f->name().contains(_filter)) continue;

        _filteredList.append(f);
        if (!_max0 || lessThan0(_max0, f)) { _max0 = f; }
        if (!_max1 || lessThan1(_max1, f)) { _max1 = f; }
        if (!_max2 || lessThan2(_max2, f)) { _max2 = f; }
        index++;
    }
}

void FunctionListModel::computeTopList()
{
    beginResetModel();
    _topList.clear();
    if (_filteredList.isEmpty()) {
        endResetModel();
        return;
    }

    FunctionLessThan lessThan(_sortColumn, _sortOrder, _eventType);
    std::stable_sort(_filteredList.begin(), _filteredList.end(), lessThan);

    foreach(TraceFunction* f, _filteredList) {
        _topList.append(f);
        if (_topList.count() >= _maxCount) break;
    }

    // append max entries
    QList<TraceFunction*> maxList;
    if (_max0 && !_topList.contains(_max0)) maxList.append(_max0);
    if (_max1 && !_topList.contains(_max1)) maxList.append(_max1);
    if (_max2 && !_topList.contains(_max2)) maxList.append(_max2);
    std::stable_sort(maxList.begin(), maxList.end(), lessThan);
    _topList.append(maxList);

    endResetModel();
}

QString FunctionListModel::getName(TraceFunction *f) const
{
    return f->prettyName();
}

QPixmap FunctionListModel::getNamePixmap(TraceFunction *f) const
{
    QColor c = GlobalGUIConfig::functionColor(_groupType, f);
    return colorPixmap(10, 10, c);
}

QString FunctionListModel::getLocation(TraceFunction *f) const
{
    return f->prettyLocation();
}

QString FunctionListModel::getSelfCost(TraceFunction *f) const
{
    ProfileCostArray* selfCost = f->data();
    if (GlobalConfig::showExpanded()) {
        switch(_groupType) {
        case ProfileContext::Object: selfCost = f->object(); break;
        case ProfileContext::Class:  selfCost = f->cls(); break;
        case ProfileContext::File:   selfCost = f->file(); break;
        default: break;
        }
    }
    double selfTotal = selfCost->subCost(_eventType);
    if (selfTotal == 0.0)
        return QStringLiteral("-");

    // self
    SubCost pure = f->subCost(_eventType);
    double self  = 100.0 * pure / selfTotal;
    if (GlobalConfig::showPercentage())
        return QStringLiteral("%1")
                .arg(self, 0, 'f', GlobalConfig::percentPrecision());
    else
        return f->prettySubCost(_eventType);
}

QPixmap FunctionListModel::getSelfPixmap(TraceFunction *f) const
{
    ProfileCostArray* selfCost = f->data();
    if (GlobalConfig::showExpanded()) {
        switch(_groupType) {
        case ProfileContext::Object: selfCost = f->object(); break;
        case ProfileContext::Class:  selfCost = f->cls(); break;
        case ProfileContext::File:   selfCost = f->file(); break;
        default: break;
        }
    }
    double selfTotal = selfCost->subCost(_eventType);
    if (selfTotal == 0.0)
        return QPixmap();

    return costPixmap(_eventType, f, selfTotal, false);
}

QString FunctionListModel::getInclCost(TraceFunction *f) const
{
    double inclTotal = f->data()->subCost(_eventType);
    if (inclTotal == 0.0)
        return QStringLiteral("-");

    SubCost sum  = f->inclusive()->subCost(_eventType);
    double incl  = 100.0 * sum / inclTotal;
    if (GlobalConfig::showPercentage())
        return QStringLiteral("%1")
                .arg(incl, 0, 'f', GlobalConfig::percentPrecision());
    else
        return f->inclusive()->prettySubCost(_eventType);
}

QPixmap FunctionListModel::getInclPixmap(TraceFunction *f) const
{
    double inclTotal = f->data()->subCost(_eventType);
    if (inclTotal == 0.0)
        return QPixmap();

    return costPixmap(_eventType, f->inclusive(), inclTotal, false);
}


QString FunctionListModel::getCallCount(TraceFunction *f) const
{
    QString str;
    if (f->calledCount() > 0)
        str = f->prettyCalledCount();
    else {
        if (f == f->cycle())
            str = QStringLiteral("-");
        else
            str = QStringLiteral("(0)");
    }
    return str;
}

//
// FunctionListModel::FunctionLessThan
//
bool FunctionListModel::FunctionLessThan::operator()(TraceFunction *left,
                                                     TraceFunction *right)
{
    TraceFunction* f1 = left;
    TraceFunction* f2 = right;

    // descending: swap arguments
    if (_order == Qt::DescendingOrder) {
        TraceFunction* temp = f1;
        f1 = f2;
        f2 = temp;
    }

    switch(_column) {
    case 0:
    {
        SubCost sum1 = f1->inclusive()->subCost(_eventType);
        SubCost sum2 = f2->inclusive()->subCost(_eventType);
        return sum1 < sum2;
    }

    case 1:
    {
        SubCost pure1 = f1->subCost(_eventType);
        SubCost pure2 = f2->subCost(_eventType);
        return pure1 < pure2;
    }

    case 2:
        return f1->calledCount() < f2->calledCount();

    case 3:
        return f1->name() < f2->name();

    case 4:
        return f1->object()->name() < f2->object()->name();
    }

    return false;
}


