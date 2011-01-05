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

#ifndef STACKBROWSER_H
#define STACKBROWSER_H

#include "tracedata.h"

// A history of selected functions within stacks

class Stack
{
public:
  Stack(TraceFunction*);

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
