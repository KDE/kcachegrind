/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * StackSelection for KCachegrind
 * For function selection of a most expected stack,
 *  to be put into a QDockWindow
 */

#include "stackselection.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>

#include "stackbrowser.h"
#include "stackitem.h"


StackSelection::StackSelection(QWidget* parent)
    : QWidget(parent)
{
    _data = nullptr;
    _browser = new StackBrowser();
    _function = nullptr;
    _eventType = nullptr;
    _eventType2 = nullptr;
    _groupType = ProfileContext::Function;

    setWindowTitle(tr("Stack Selection"));

    QVBoxLayout* vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(6);
    vboxLayout->setContentsMargins(3, 3, 3, 3);

    _stackList = new QTreeWidget(this);
    QStringList headerLabels;
    headerLabels << tr("Cost")
                 << tr("Cost2")
                 << tr("Calls")
                 << tr("Function");
    _stackList->setHeaderLabels(headerLabels);
    _stackList->setRootIsDecorated(false);
    _stackList->setAllColumnsShowFocus(true);
    _stackList->setUniformRowHeights(true);
    _stackList->setSortingEnabled(false);
    _stackList->setColumnWidth(0, 50);
    // 2nd cost column hidden at first (_eventType2 == 0)
    _stackList->setColumnWidth(1, 0);
    _stackList->setColumnWidth(2, 50);
    vboxLayout->addWidget(_stackList);

    connect(_stackList,
            &QTreeWidget::currentItemChanged,
            this, &StackSelection::stackSelected );
}

StackSelection::~StackSelection()
{
    delete _browser;
}

void StackSelection::setData(TraceData* data)
{
    if (_data == data) return;

    _data = data;

    _stackList->clear();
    delete _browser;
    _browser = new StackBrowser();
    _function = nullptr;
}


void StackSelection::setFunction(TraceFunction* f)
{
    if (_function == f) return;
    _function = f;

    if (!_data || !_function) return;

    //qDebug() << "StackSelection::setFunction " << f->name();

    HistoryItem* item = _browser->current();
    if (!item || item->function() != f) {
        _browser->select(f);
        rebuildStackList();
    }
}


void StackSelection::rebuildStackList()
{
    HistoryItem* item = _browser->current();
    _stackList->clear();
    _stackList->setColumnWidth(0, 50);
    _stackList->setColumnWidth(1, _eventType2 ? 50:0);
    _stackList->setColumnWidth(2, 50);
    if (!item || !item->stack()) return;

    TraceFunction* top = item->stack()->top();
    if (!top) return;


    QList<QTreeWidgetItem*> items;
    QTreeWidgetItem* activeItem = nullptr;
    TraceCallList l = item->stack()->calls();
    for(int i=l.count()-1; i>=0; i--) {
        StackItem* si = new StackItem(this, nullptr, l.at(i));
        if (si->function() == item->function())
            activeItem = si;
        items.prepend(si);
    }
    StackItem* si = new StackItem(this, nullptr, top);
    if (si->function() == item->function())
        activeItem = si;
    items.prepend(si);

#if QT_VERSION >= 0x050000
    _stackList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _stackList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _stackList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
#else
    _stackList->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    _stackList->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    _stackList->header()->setResizeMode(2, QHeaderView::ResizeToContents);
#endif

    _stackList->addTopLevelItems(items);
    if (activeItem) {
        // this calls stackFunctionSelected()
        _stackList->setCurrentItem(activeItem);
        _stackList->scrollToItem(activeItem);
    }

#if QT_VERSION >= 0x050000
    _stackList->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    _stackList->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    _stackList->header()->setSectionResizeMode(2, QHeaderView::Interactive);
#else
    _stackList->header()->setResizeMode(0, QHeaderView::Interactive);
    _stackList->header()->setResizeMode(1, QHeaderView::Interactive);
    _stackList->header()->setResizeMode(2, QHeaderView::Interactive);
#endif

    if (!_eventType2) {
        _stackList->setColumnWidth(1, 0);
    }
}

void StackSelection::stackSelected(QTreeWidgetItem* i, QTreeWidgetItem*)
{
    if (!i) return;

    TraceFunction* f = ((StackItem*)i)->function();
    emit functionSelected(f);
}


void StackSelection::browserBack()
{
    if (_browser && _browser->canGoBack()) {
        _browser->goBack();
        rebuildStackList();
    }
}

void StackSelection::browserForward()
{
    if (_browser && _browser->canGoForward()) {
        _browser->goForward();
        rebuildStackList();
    }
}

void StackSelection::browserUp()
{
    if (_browser) {
        _browser->goUp();
        rebuildStackList();
    }
}

void StackSelection::browserDown()
{
    if (_browser) {
        _browser->goDown();
        rebuildStackList();
    }
}

void StackSelection::refresh()
{
#if QT_VERSION >= 0x050000
    _stackList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _stackList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
#else
    _stackList->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    _stackList->header()->setResizeMode(1, QHeaderView::ResizeToContents);
#endif

    // there is no resorting allowed, so this is save
    for(int i = 0; i < _stackList->topLevelItemCount(); i++) {
        QTreeWidgetItem* item = _stackList->topLevelItem(i);
        ((StackItem*)item)->updateCost();
    }

    if (!_eventType2) {
#if QT_VERSION >= 0x050000
        _stackList->header()->setSectionResizeMode(1, QHeaderView::Interactive);
#else
        _stackList->header()->setResizeMode(1, QHeaderView::Interactive);
#endif
        _stackList->setColumnWidth(1, 0);
    }
}

void StackSelection::setEventType(EventType* ct)
{
    if (ct == _eventType) return;
    _eventType = ct;

    if (_eventType)
        _stackList->headerItem()->setText(0, _eventType->name());

    refresh();
}

void StackSelection::setEventType2(EventType* ct)
{
    if (ct == _eventType2) return;
    _eventType2 = ct;

    if (_eventType2)
        _stackList->headerItem()->setText(1, _eventType2->name());

    refresh();
}

void StackSelection::setGroupType(ProfileContext::Type gt)
{
    if (_groupType == gt) return;
    _groupType = gt;

    for(int i = 0; i < _stackList->topLevelItemCount(); i++) {
        QTreeWidgetItem* item = _stackList->topLevelItem(i);
        ((StackItem*)item)->updateGroup();
    }
}

#include "moc_stackselection.cpp"
