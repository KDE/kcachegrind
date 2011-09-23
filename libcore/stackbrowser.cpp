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

#include "stackbrowser.h"


// Stack

Stack::Stack(TraceFunction* top, TraceCallList calls)
{
  _refCount = 0;
  _top = top;
  _calls = calls;

  extendBottom();
}

Stack::Stack(TraceFunction* f)
{
  _refCount = 0;
  _top = f;

  extendBottom();
  extendTop();
}

void Stack::extendBottom()
{
  SubCost most;
  TraceFunction* f;

  if (!_calls.isEmpty())
    f = _calls.last()->called();
  else
    f = _top;

  if (!f) return;
  // do not follow calls from cycles
  if (f->cycle() == f) return;


  int max = 30;

  // try to extend to lower stack frames
  while (f && (max-- >0)) {
    TraceCall* call = 0;
    most = 0;
    foreach(TraceCall* c, f->callings()) {
	// no cycle calls in stack: could be deleted without notice
	if (c->called()->cycle() == c->called()) continue;
	// no simple recursions
	if (c->called() == _top) continue;

      if (c->called()->name().isEmpty()) continue;
      SubCost sc = c->subCost(0); // FIXME
      if (sc == 0) continue;

      if (sc > most) {
        most = sc;
        call = c;
      }
    }
    if (!call)
      break;

    _calls.append(call);
    f = call->called();
  }
}


void Stack::extendTop()
{
  SubCost most;

  int max = 10;

  // do not follow calls from cycles
  if (_top->cycle() == _top) return;

  // try to extend to upper stack frames
  while (_top && (max-- >0)) {
    TraceCall* call = 0;
    most = 0;
    foreach(TraceCall* c, _top->callers()) {
	// no cycle calls in stack: could be deleted without notice
	if (c->caller()->cycle() == c->caller()) continue;
	// no simple recursions
	if (c->caller() == _top) continue;

      if (c->caller()->name().isEmpty()) continue;
      SubCost sc = c->subCost(0); // FIXME
      if (sc == 0) continue;

      if (sc > most) {
        most = sc;
        call = c;
      }
    }
    if (!call)
      break;

    _calls.prepend(call);
    _top = call->caller();
  }
}

TraceFunction* Stack::caller(TraceFunction* fn, bool extend)
{
  TraceFunction* f;

  if (extend && (_top == fn)) {
    // extend at top
    extendTop();
    f = _top;
  }

  foreach(TraceCall* c, _calls) {
    f = c->called();
    if (f == fn)
      return c->caller();
  }
  return 0;
}

TraceFunction* Stack::called(TraceFunction* fn, bool extend)
{
  TraceFunction* f;

  foreach(TraceCall* c, _calls) {
    f = c->caller();
    if (f == fn)
      return c->called();
  }

  if (extend) {
    // extend at bottom
    extendBottom();

    // and search again
    foreach(TraceCall* c, _calls) {
      f = c->caller();
      if (f == fn)
        return c->called();
    }
  }

  return 0;
}

bool Stack::contains(TraceFunction* fn)
{
    // cycles are listed on there own
    if (fn->cycle() == fn) return false;
    if (_top->cycle() == _top) return false;

  if (fn == _top)
    return true;

  TraceFunction* f = _top;

  foreach(TraceCall* c, _calls) {
    f = c->called();
    if (f == fn)
      return true;
  }

  // try to extend at bottom (even if callCount 0)
  foreach(TraceCall* c, f->callings()) {
    f = c->called();
    if (f == fn) {
        _calls.append(c);

        // extend at bottom after found one
        extendBottom();
        return true;
    }
  }

  // try to extend at top (even if callCount 0)
  foreach(TraceCall* c, _top->callers()) {
    f = c->caller();
    if (f == fn) {
        _calls.prepend(c);

        // extend at top after found one
        extendTop();
        return true;
    }
  }

  return false;
}

Stack* Stack::split(TraceFunction* f)
{
  TraceCallList calls = _calls;

  // cycles are listed on there own
  if (f->cycle() == f) return 0;
  if (_top->cycle() == _top) return false;

  foreach(TraceCall* c, calls) {
    foreach(TraceCall* c2, c->called()->callings()) {
      if (c2 == c) continue;
      if (c2->called() != f) continue;

      // remove bottom part
      while (!calls.isEmpty() && (calls.last()!=c))
        calls.removeLast();

      calls.append(c2);
      return new Stack(_top, calls);
    }
  }
  return 0;
}

QString Stack::toString()
{
  QString res = _top->name();
  foreach(TraceCall *c, _calls)
    res += "\n > " + c->called()->name();

  return res;
}


// HistoryItem

HistoryItem::HistoryItem(Stack* stack, TraceFunction* function)
{
  _stack = stack;
  _function = function;
  if (_stack)
    _stack->ref();

  _last = 0;
  _next = 0;

/*
  qDebug("New Stack History Item (sRef %d): %s\n  %s",
         _stack->refCount(), qPrintable(_function->name()),
         qPrintable(_stack->toString()));
*/
}

HistoryItem::~HistoryItem()
{
  if (0) qDebug("Deleting Stack History Item (sRef %d): %s",
                _stack->refCount(),
                qPrintable(_function->name()));

  if (_last)
    _last->_next = _next;
  if (_stack) {
    if (_stack->deref() == 0)
      delete _stack;
  }
}


// StackBrowser

StackBrowser::StackBrowser()
{
  _current = 0;
}

StackBrowser::~StackBrowser()
{
  delete _current;
}

HistoryItem* StackBrowser::select(TraceFunction* f)
{
  if (!_current) {
    Stack* s = new Stack(f);
    _current = new HistoryItem(s, f);
  }
  else if (_current->function() != f) {
    // make current item the last one
    HistoryItem* item = _current;
    if (item->next()) {
      item = item->next();
      item->last()->setNext(0);

      while (item->next()) {
        item = item->next();
        delete item->last();
      }
      delete item;
    }

    Stack* s = _current->stack();
    if (!s->contains(f)) {
      s = s->split(f);
      if (!s)
        s = new Stack(f);
    }

    item = _current;
    _current = new HistoryItem(s, f);
    item->setNext(_current);
    _current->setLast(item);
  }

  // qDebug("Selected %s in StackBrowser", qPrintable(f->name()));

  return _current;
}

HistoryItem* StackBrowser::goBack()
{
  if (_current && _current->last())
    _current = _current->last();

  return _current;
}

HistoryItem* StackBrowser::goForward()
{
  if (_current && _current->next())
    _current = _current->next();

  return _current;
}

HistoryItem* StackBrowser::goUp()
{
  if (_current) {
    TraceFunction* f = _current->stack()->caller(_current->function(), true);
    if (f)
      _current = select(f);
  }

  return _current;
}

HistoryItem* StackBrowser::goDown()
{
  if (_current) {
    TraceFunction* f = _current->stack()->called(_current->function(), true);
    if (f)
      _current = select(f);
  }

  return _current;
}

bool StackBrowser::canGoBack()
{
  return _current && _current->last();
}

bool StackBrowser::canGoForward()
{
  return _current && _current->next();
}

bool StackBrowser::canGoUp()
{
  if (!_current) return false;

  return _current->stack()->caller(_current->function(), false);
}

bool StackBrowser::canGoDown()
  {
  if (!_current) return false;

  return _current->stack()->called(_current->function(), false);
}
