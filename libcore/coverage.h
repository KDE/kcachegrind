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

#ifndef COVERAGE_H
#define COVERAGE_H

#include "tracedata.h"

/**
 * Coverage of a function.
 * When analysis is done, every function involved will have a
 * pointer to an object of this class.
 *
 * This function also holds the main routine for coverage analysis,
 * Coverage::coverage(), as static method.
 */
class Coverage : public TraceAssoziation
{
public:
  /* Direction of coverage analysis */
  enum CoverageMode { Caller, Called };

  // max depth for distance histogram
#define maxHistogramDepthValue 40
  static const int maxHistogramDepth;

  static const int Rtti;

  Coverage();

  virtual int rtti() { return Rtti; }
  void init();

  TraceFunction* function() { return _function; }
  double self() { return _self; }
  double inclusive() { return _incl; }
  double firstPercentage() { return _firstPercentage; }
  double& callCount() { return _callCount; }
  int minDistance() { return _minDistance; }
  int maxDistance() { return _maxDistance; }
  int inclusiveMedian();
  int selfMedian();
  double* selfHistogram() { return _selfHisto; }
  double* inclusiveHistogram() { return _inclHisto; }
  bool isActive() { return _active; }
  bool inRecursion() { return _inRecursion; }

  void setSelf(float p) { _self = p; }
  void setInclusive(float p) { _incl = p; }
  void setCallCount(float cc) { _callCount = cc; }
  void setActive(bool a) { _active = a; }
  void setInRecursion(bool r) { _inRecursion = r; }

  /**
   * Calculate coverage of all functions based on function f.
   * If mode is Called, the coverage of functions called by
   * f is calculated, otherwise that of functions calling f.
   * SubCost type ct is used for the analysis.
   * Self values are undefined for Caller mode.
   *
   * Returns list of functions covered.
   * Coverage degree of returned functions can be get
   * with function->coverage()->percentage()
   */
  static TraceFunctionList coverage(TraceFunction* f, CoverageMode m,
                                    EventType* ct);

private:
  void addCallerCoverage(TraceFunctionList& l, double, int d);
  void addCallingCoverage(TraceFunctionList& l, double, double, int d);

  double _self, _incl, _firstPercentage, _callCount;
  int _minDistance, _maxDistance;
  bool _active, _inRecursion;
  double _selfHisto[maxHistogramDepthValue];
  double _inclHisto[maxHistogramDepthValue];

  // temporary set for one coverage analysis
  static EventType* _costType;
};

#endif

