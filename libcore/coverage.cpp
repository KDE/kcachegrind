/* This file is part of KCachegrind.
   Copyright (C) 2002 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Function Coverage Analysis
 */

#include "coverage.h"

//#define DEBUG_COVERAGE 1

EventType* Coverage::_costType;

const int Coverage::maxHistogramDepth = maxHistogramDepthValue;
const int Coverage::Rtti = 1;

Coverage::Coverage()
{
}

void Coverage::init()
{
  _self = 0.0;
  _incl  = 0.0;
  _callCount = 0.0;
  // should always be overwritten before usage
  _firstPercentage = 1.0;
  _minDistance = 9999;
  _maxDistance = 0;
  _active = false;
  _inRecursion = false;
  for (int i = 0;i<maxHistogramDepth;i++) {
    _selfHisto[i] = 0.0;
    _inclHisto[i] = 0.0;
  }

  _valid = true;
}

int Coverage::inclusiveMedian()
{
  double maxP = _inclHisto[0];
  int medD = 0;
  for (int i = 1;i<maxHistogramDepth;i++)
    if (_inclHisto[i]>maxP) {
      maxP = _inclHisto[i];
      medD = i;
    }

  return medD;
}

int Coverage::selfMedian()
{
  double maxP = _selfHisto[0];
  int medD = 0;
  for (int i = 1;i<maxHistogramDepth;i++)
    if (_selfHisto[i]>maxP) {
      maxP = _selfHisto[i];
      medD = i;
    }

  return medD;
}

TraceFunctionList Coverage::coverage(TraceFunction* f, CoverageMode m,
                                      EventType* ct)
{
  invalidate(f->data(), Coverage::Rtti);

  _costType = ct;

  // function f takes ownership over c!
  Coverage* c = new Coverage();
  c->setFunction(f);
  c->init();

  TraceFunctionList l;

  if (m == Caller)
    c->addCallerCoverage(l, 1.0, 0);
  else
    c->addCallingCoverage(l, 1.0, 1.0, 0);

  return l;
}

void Coverage::addCallerCoverage(TraceFunctionList& fList,
                                 double pBack, int d)
{
  if (_inRecursion) return;

  double incl;
  incl  = (double) (_function->inclusive()->subCost(_costType));

  if (_active) {
#ifdef DEBUG_COVERAGE
    qDebug("CallerCov: D %d, %s (was active, incl %f, self %f): newP %f", d,
           qPrintable(_function->prettyName()), _incl, _self, pBack);
#endif
    _inRecursion = true;
  }
  else {
    _active = true;

    // only add cost if this is no recursion

    _incl += pBack;
    _firstPercentage = pBack;

    if (_minDistance > d) _minDistance = d;
    if (_maxDistance < d) _maxDistance = d;
    if (d<maxHistogramDepth) {
      _inclHisto[d] += pBack;
    }
    else {
      _inclHisto[maxHistogramDepth-1] += pBack;
    }

#ifdef DEBUG_COVERAGE
    qDebug("CallerCov: D %d, %s (now active, new incl %f): newP %f",
           d, qPrintable(_function->prettyName()), _incl, pBack);
#endif
  }

  double callVal, pBackNew;

  foreach(TraceCall* call, _function->callers()) {
    if (call->inCycle()>0) continue;
    if (call->isRecursion()) continue;

    if (call->subCost(_costType)>0) {
      TraceFunction* caller = call->caller();

      Coverage* c = (Coverage*) caller->assoziation(rtti());
      if (!c) {
        c = new Coverage();
        c->setFunction(caller);
      }
      if (!c->isValid()) {
        c->init();
        fList.append(caller);
      }

      if (c->isActive()) continue;
      if (c->inRecursion()) continue;

      callVal = (double) call->subCost(_costType);
      pBackNew = pBack * (callVal / incl);

      // FIXME ?!?

      if (!c->isActive()) {
	  if (d>=0)
	      c->callCount() += (double)call->callCount();
	  else
	      c->callCount() += _callCount;
      }
      else {
        // adjust pNew by sum of geometric series of recursion factor.
        // Thus we can avoid endless recursion here
        pBackNew *= 1.0 / (1.0 - pBackNew / c->firstPercentage());
      }

      // Limit depth
      if (pBackNew > 0.0001)
        c->addCallerCoverage(fList, pBackNew, d+1);
    }
  }

  if (_inRecursion)
    _inRecursion = false;
  else if (_active)
    _active = false;
}

/**
 * pForward is time on percent used,
 * pBack is given to allow for calculation of call counts
 */
void Coverage::addCallingCoverage(TraceFunctionList& fList,
                                  double pForward, double pBack, int d)
{
  if (_inRecursion) return;

#ifdef DEBUG_COVERAGE
  static const char* spaces = "                                            ";
#endif

  double self, incl;
  incl  = (double) (_function->inclusive()->subCost(_costType));

#ifdef DEBUG_COVERAGE
    qDebug("CngCov:%s - %s (incl %f, self %f): forward %f, back %f",
	   spaces+strlen(spaces)-d,
	   qPrintable(_function->prettyName()), _incl, _self, pForward, pBack);
#endif


  if (_active) {
    _inRecursion = true;

#ifdef DEBUG_COVERAGE
    qDebug("CngCov:%s < %s: STOP (is active)",
	   spaces+strlen(spaces)-d,
	   qPrintable(_function->prettyName()));
#endif

  }
  else {
    _active = true;

    // only add cost if this is no recursion
    self = pForward * (_function->subCost(_costType)) / incl;
    _incl += pForward;
    _self += self;
    _firstPercentage = pForward;

    if (_minDistance > d) _minDistance = d;
    if (_maxDistance < d) _maxDistance = d;
     if (d<maxHistogramDepth) {
      _inclHisto[d] += pForward;
      _selfHisto[d] += self;
    }
    else {
      _inclHisto[maxHistogramDepth-1] += pForward;
      _selfHisto[maxHistogramDepth-1] += self;
    }

#ifdef DEBUG_COVERAGE
    qDebug("CngCov:%s < %s (incl %f, self %f)",
	   spaces+strlen(spaces)-d,
	   qPrintable(_function->prettyName()), _incl, _self);
#endif
  }

  double callVal, pForwardNew, pBackNew;

  foreach(TraceCall* call, _function->callings()) {
    if (call->inCycle()>0) continue;
    if (call->isRecursion()) continue;

    if (call->subCost(_costType)>0) {
      TraceFunction* calling = call->called();

      Coverage* c = (Coverage*) calling->assoziation(rtti());
      if (!c) {
        c = new Coverage();
        c->setFunction(calling);
      }
      if (!c->isValid()) {
        c->init();
        fList.append(calling);
      }

      if (c->isActive()) continue;
      if (c->inRecursion()) continue;

      callVal = (double) call->subCost(_costType);
      pForwardNew = pForward * (callVal / incl);
      pBackNew    = pBack * (callVal / 
			     calling->inclusive()->subCost(_costType));

      if (!c->isActive()) {
	  c->callCount() += pBack * call->callCount();

#ifdef DEBUG_COVERAGE
        qDebug("CngCov:%s  > %s: forward %f, back %f, calls %f -> %f, now %f",
	       spaces+strlen(spaces)-d,
	       qPrintable(calling->prettyName()),
	       pForwardNew, pBackNew,
	       (double)call->callCount(),
	       pBack * call->callCount(),
	       c->callCount());
#endif
      }
      else {
        // adjust pNew by sum of geometric series of recursion factor.
        // Thus we can avoid endless recursion here
        double fFactor = 1.0 / (1.0 - pForwardNew / c->firstPercentage());
        double bFactor = 1.0 / (1.0 - pBackNew);
#ifdef DEBUG_COVERAGE
        qDebug("CngCov:%s Recursion - origP %f, actP %f => factor %f, newP %f",
	       spaces+strlen(spaces)-d,
               c->firstPercentage(), pForwardNew,
	       fFactor, pForwardNew * fFactor);
#endif
        pForwardNew *= fFactor;
        pBackNew *= bFactor;

      }

      // Limit depth
      if (pForwardNew > 0.0001)
        c->addCallingCoverage(fList, pForwardNew, pBackNew, d+1);
    }
  }

  if (_inRecursion)
    _inRecursion = false;
  else if (_active)
    _active = false;
}

