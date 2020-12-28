/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef STACKBROWSER_H
#define STACKBROWSER_H

#include "tracedata.h"

// A history of selected functions within stacks

class Stack
{
public:
    explicit Stack(TraceFunction*);

    // extend the stack at top/bottom if possible
    bool contains(TraceFunction*);

    void extendBottom();
    void extendTop();

    // search for a function on stack calling specified function.
    // if found, return upper part with new function call
    Stack* split(TraceFunction*);

    // increment reference count
    void ref() { _refCount++; }
    // decrement reference count
    bool deref() { return --_refCount; }
    int refCount() const { return _refCount; }

    TraceFunction* top() { return _top; }
    TraceCallList calls() { return _calls; }
    TraceFunction* caller(TraceFunction*, bool extend);
    TraceFunction* called(TraceFunction*, bool extend);

    QString toString();

private:
    Stack(TraceFunction* top, TraceCallList list);

    // at the top of the stack we have a function...
    TraceFunction* _top;
    // list ordered from top to bottom
    TraceCallList _calls;
    int _refCount;
};

class HistoryItem
{
public:
    HistoryItem(Stack*, TraceFunction*);
    ~HistoryItem();

    Stack* stack() { return _stack; }
    TraceFunction* function() { return _function; }
    HistoryItem* last() { return _last; }
    HistoryItem* next() { return _next; }
    void setLast(HistoryItem* h) { _last = h; }
    void setNext(HistoryItem* h) { _next = h; }

private:

    HistoryItem *_last, *_next;
    Stack* _stack;
    TraceFunction* _function;
};


class StackBrowser
{
public:
    StackBrowser();
    ~StackBrowser();

    // A function was selected. This creates a new history entry
    HistoryItem* select(TraceFunction*);

    HistoryItem* current() { return _current; }
    bool canGoBack();
    bool canGoForward();
    bool canGoUp();
    bool canGoDown();
    HistoryItem* goBack();
    HistoryItem* goForward();
    HistoryItem* goUp();
    HistoryItem* goDown();

private:
    HistoryItem* _current;
};


#endif
