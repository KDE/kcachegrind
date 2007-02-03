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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Callgraph View
 */

#include <stdlib.h>
#include <math.h>

#include <qtooltip.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qwhatsthis.h>
#include <qcanvas.h>
#include <qwmatrix.h>
#include <qpair.h>
#include <qpainter.h>
#include <qpopupmenu.h>
#include <qstyle.h>
#include <qprocess.h>

#include <kdebug.h>
#include <klocale.h>
#include <kconfig.h>
#include <ktempfile.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kfiledialog.h>

#include "configuration.h"
#include "callgraphview.h"
#include "toplevel.h"
#include "listutils.h"


/*
 * TODO:
 * - Zooming option for work canvas? (e.g. 1:1 - 1:3)
 */

#define DEBUG_GRAPH 0

// CallGraphView defaults

#define DEFAULT_FUNCLIMIT   .05
#define DEFAULT_CALLLIMIT   .05
#define DEFAULT_MAXCALLER    2
#define DEFAULT_MAXCALLING   -1
#define DEFAULT_SHOWSKIPPED  false
#define DEFAULT_EXPANDCYCLES false
#define DEFAULT_CLUSTERGROUPS false
#define DEFAULT_DETAILLEVEL  1
#define DEFAULT_LAYOUT       GraphOptions::TopDown
#define DEFAULT_ZOOMPOS      Auto


//
// GraphEdgeList
//

GraphEdgeList::GraphEdgeList()
    : _sortCallerPos(true)
{}

int GraphEdgeList::compareItems(Item item1, Item item2)
{
    CanvasEdge* e1 = ((GraphEdge*)item1)->canvasEdge();
    CanvasEdge* e2 = ((GraphEdge*)item2)->canvasEdge();

    // edges without arrow visualisations are sorted as low
    if (!e1) return -1;
    if (!e2) return 1;

    int dx1, dy1, dx2, dy2;
    int x, y;
    if (_sortCallerPos) {
	e1->controlPoints().point(0,&x,&y);
	e2->controlPoints().point(0,&dx1,&dy1);
	dx1 -= x; dy1 -= y;
    }
    else {
	QPointArray a1 = e1->controlPoints();
	QPointArray a2 = e2->controlPoints();
	a1.point(a1.count()-2,&x,&y);
	a2.point(a2.count()-1,&dx2,&dy2);
	dx2 -= x; dy2 -= y;
    }
    double at1 = atan2(double(dx1), double(dy1));
    double at2 = atan2(double(dx2), double(dy2));

    return (at1 < at2) ? 1:-1;
}




//
// GraphNode
//

GraphNode::GraphNode()
{
    _f=0;
    self = incl = 0;
    _cn = 0;

    _visible = false;
    _lastCallerIndex = _lastCallingIndex = -1;

    callers.setSortCallerPos(false);
    callings.setSortCallerPos(true);
    _lastFromCaller = true;
}

TraceCall* GraphNode::visibleCaller()
{
    if (0) qDebug("GraphNode::visibleCaller %s: last %d, count %d",
		  _f->prettyName().ascii(), _lastCallerIndex, callers.count());

    GraphEdge* e = callers.at(_lastCallerIndex);
    if (e && !e->isVisible()) e = 0;
    if (!e) {
	double maxCost = 0.0;
	GraphEdge* maxEdge = 0;
	int idx = 0;
	for(e = callers.first();e; e=callers.next(),idx++)
	    if (e->isVisible() && (e->cost > maxCost)) {
		maxCost = e->cost;
		maxEdge = e;
		_lastCallerIndex = idx;
	    }
	e = maxEdge;
    }
    return e ? e->call() : 0;
}

TraceCall* GraphNode::visibleCalling()
{
    if (0) qDebug("GraphNode::visibleCalling %s: last %d, count %d",
		  _f->prettyName().ascii(), _lastCallingIndex, callings.count());

    GraphEdge* e = callings.at(_lastCallingIndex);
    if (e && !e->isVisible()) e = 0;
    if (!e) {
	double maxCost = 0.0;
	GraphEdge* maxEdge = 0;
	int idx = 0;
	for(e = callings.first();e; e=callings.next(),idx++)
	    if (e->isVisible() && (e->cost > maxCost)) {
		maxCost = e->cost;
		maxEdge = e;
		_lastCallingIndex = idx;
	    }
	e = maxEdge;
    }
    return e ? e->call() : 0;
}

void GraphNode::setCalling(GraphEdge* e)
{
    _lastCallingIndex = callings.findRef(e);
    _lastFromCaller = false;
}

void GraphNode::setCaller(GraphEdge* e)
{
    _lastCallerIndex = callers.findRef(e);
    _lastFromCaller = true;
}

TraceFunction* GraphNode::nextVisible()
{
  TraceCall* c;
  if (_lastFromCaller) {
    c = nextVisibleCaller(callers.at(_lastCallerIndex));
    if (c) return c->called(true);
    c = nextVisibleCalling(callings.at(_lastCallingIndex));
    if (c) return c->caller(true);
  }
  else {
    c = nextVisibleCalling(callings.at(_lastCallingIndex));
    if (c) return c->caller(true);
    c = nextVisibleCaller(callers.at(_lastCallerIndex));
    if (c) return c->called(true);
  }
  return 0;
}

TraceFunction* GraphNode::priorVisible()
{
  TraceCall* c;
  if (_lastFromCaller) {
    c = priorVisibleCaller(callers.at(_lastCallerIndex));
    if (c) return c->called(true);
    c = priorVisibleCalling(callings.at(_lastCallingIndex));
    if (c) return c->caller(true);
  }
  else {
    c = priorVisibleCalling(callings.at(_lastCallingIndex));
    if (c) return c->caller(true);
    c = priorVisibleCaller(callers.at(_lastCallerIndex));
    if (c) return c->called(true);
  }
  return 0;
}

TraceCall* GraphNode::nextVisibleCaller(GraphEdge* last)
{
    GraphEdge* e;
    bool found = false;
    int idx = 0;
    for(e = callers.first();e; e=callers.next(),idx++) {
	if (found && e->isVisible()) {
	    _lastCallerIndex = idx;
	    return e->call();
	}
	if (e == last) found = true;
    }
    return 0;
}

TraceCall* GraphNode::nextVisibleCalling(GraphEdge* last)
{
    GraphEdge* e;
    bool found = false;
    int idx = 0;
    for(e = callings.first();e; e=callings.next(),idx++) {
	if (found && e->isVisible()) {
	    _lastCallingIndex = idx;
	    return e->call();
	}
	if (e == last) found = true;
    }
    return 0;
}

TraceCall* GraphNode::priorVisibleCaller(GraphEdge* last)
{
    GraphEdge *e, *prev = 0;
    int prevIdx = -1, idx = 0;
    for(e = callers.first(); e; e=callers.next(),idx++) {
	if (e == last) {
	    _lastCallerIndex = prevIdx;
	    return prev ? prev->call() : 0;
	}
	if (e->isVisible()) {
	    prev = e;
	    prevIdx = idx;
	}
    }
    return 0;
}

TraceCall* GraphNode::priorVisibleCalling(GraphEdge* last)
{
    GraphEdge *e, *prev = 0;
    int prevIdx = -1, idx = 0;
    for(e = callings.first(); e; e=callings.next(),idx++) {
	if (e == last) {
	    _lastCallingIndex = prevIdx;
	    return prev ? prev->call() : 0;
	}
	if (e->isVisible()) {
	    prev = e;
	    prevIdx = idx;
	}
    }
    return 0;
}

//
// GraphEdge
//

GraphEdge::GraphEdge()
{
    _c=0;
    _from = _to = 0;
    _fromNode = _toNode = 0;
    cost = count = 0;
    _ce = 0;

    _visible = false;
    _lastFromCaller = true;
}

QString GraphEdge::prettyName()
{
    if (_c) return _c->prettyName();
    if (_from) return i18n("Call(s) from %1").arg(_from->prettyName());
    if (_to) return i18n("Call(s) to %1").arg(_to->prettyName());
    return i18n("(unknown call)");
}


TraceFunction* GraphEdge::visibleCaller()
{
    if (_from) {
	_lastFromCaller = true;
	if (_fromNode) _fromNode->setCalling(this);
	return _from;
    }
    return 0;
}

TraceFunction* GraphEdge::visibleCalling()
{
    if (_to) {
	_lastFromCaller = false;
	if (_toNode) _toNode->setCaller(this);
	return _to;
    }
    return 0;
}

TraceCall* GraphEdge::nextVisible()
{
    TraceCall* res = 0;

    if (_lastFromCaller && _fromNode) {
	res = _fromNode->nextVisibleCalling(this);
	if (!res && _toNode)
	    res = _toNode->nextVisibleCaller(this);
    }
    else if (_toNode) {
	res = _toNode->nextVisibleCaller(this);
	if (!res && _fromNode)
	    res = _fromNode->nextVisibleCalling(this);
    }
    return res;
}

TraceCall* GraphEdge::priorVisible()
{
    TraceCall* res = 0;

    if (_lastFromCaller && _fromNode) {
	res = _fromNode->priorVisibleCalling(this);
	if (!res && _toNode)
	    res = _toNode->priorVisibleCaller(this);
    }
    else if (_toNode) {
	res = _toNode->priorVisibleCaller(this);
	if (!res && _fromNode)
	    res = _fromNode->priorVisibleCalling(this);
    }
    return res;
}



//
// GraphOptions
//

QString GraphOptions::layoutString(Layout l)
{
    if (l == Circular) return QString("Circular");
    if (l == LeftRight) return QString("LeftRight");
    return QString("TopDown");
}

GraphOptions::Layout GraphOptions::layout(QString s)
{
    if (s == QString("Circular")) return Circular;
    if (s == QString("LeftRight")) return LeftRight;
    return TopDown;
}


//
// StorableGraphOptions
//

StorableGraphOptions::StorableGraphOptions()
{
  // default options
  _funcLimit         = DEFAULT_FUNCLIMIT;
  _callLimit         = DEFAULT_CALLLIMIT;
  _maxCallerDepth    = DEFAULT_MAXCALLER;
  _maxCallingDepth   = DEFAULT_MAXCALLING;
  _showSkipped       = DEFAULT_SHOWSKIPPED;
  _expandCycles      = DEFAULT_EXPANDCYCLES;
  _detailLevel       = DEFAULT_DETAILLEVEL;
  _layout            = DEFAULT_LAYOUT;
}




//
// GraphExporter
//

GraphExporter::GraphExporter()
{
    _go = this;
    _tmpFile = 0;
    _item = 0;
    reset(0, 0, 0, TraceItem::NoCostType, QString::null);
}


GraphExporter::GraphExporter(TraceData* d, TraceFunction* f, TraceCostType* ct,
                             TraceItem::CostType gt, QString filename)
{
    _go = this;
    _tmpFile = 0;
    _item = 0;
    reset(d, f, ct, gt, filename);
}


GraphExporter::~GraphExporter()
{
  if (_item && _tmpFile) {
#if DEBUG_GRAPH
    _tmpFile->unlink();
#endif
    delete _tmpFile;
  }
}


void GraphExporter::reset(TraceData*, TraceItem* i, TraceCostType* ct,
                          TraceItem::CostType gt, QString filename)
{
  _graphCreated = false;
  _nodeMap.clear();
  _edgeMap.clear();

  if (_item && _tmpFile) {
    _tmpFile->unlink();
    delete _tmpFile;
  }

  if (i) {
    switch(i->type()) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
    case TraceItem::Call:
      break;
    default:
      i = 0;
    }
  }

  _item = i;
  _costType = ct;
  _groupType = gt;
  if (!i) return;

  if (filename.isEmpty()) {
    _tmpFile = new KTempFile(QString::null, ".dot");
    _dotName = _tmpFile->name();
    _useBox = true;
  }
  else {
    _tmpFile = 0;
    _dotName = filename;
    _useBox = false;
  }
}



void GraphExporter::setGraphOptions(GraphOptions* go)
{
    if (go == 0) go = this;
    _go = go;
}

void GraphExporter::createGraph()
{
  if (!_item) return;
  if (_graphCreated) return;
  _graphCreated = true;

  if ((_item->type() == TraceItem::Function) ||
      (_item->type() == TraceItem::FunctionCycle)) {
    TraceFunction* f = (TraceFunction*) _item;

    double incl = f->inclusive()->subCost(_costType);
    _realFuncLimit = incl * _go->funcLimit();
    _realCallLimit = incl * _go->callLimit();

    buildGraph(f, 0, true, 1.0); // down to callings

    // set costs of function back to 0, as it will be added again
    GraphNode& n = _nodeMap[f];
    n.self = n.incl = 0.0;

    buildGraph(f, 0, false, 1.0); // up to callers
  }
  else {
    TraceCall* c = (TraceCall*) _item;

    double incl = c->subCost(_costType);
    _realFuncLimit = incl * _go->funcLimit();
    _realCallLimit = incl * _go->callLimit();

    // create edge
    TraceFunction *caller, *called;
    caller = c->caller(false);
    called = c->called(false);
    QPair<TraceFunction*,TraceFunction*> p(caller, called);
    GraphEdge& e = _edgeMap[p];
    e.setCall(c);
    e.setCaller(p.first);
    e.setCalling(p.second);
    e.cost  = c->subCost(_costType);
    e.count = c->callCount();

    SubCost s = called->inclusive()->subCost(_costType);
    buildGraph(called, 0, true,  e.cost / s); // down to callings
    s = caller->inclusive()->subCost(_costType);
    buildGraph(caller, 0, false, e.cost / s); // up to callers
  }
}

void GraphExporter::writeDot()
{
  if (!_item) return;

  QFile* file = 0;
  QTextStream* stream = 0;

  if (_tmpFile)
    stream = _tmpFile->textStream();
  else {
    file = new QFile(_dotName);
    if ( !file->open( IO_WriteOnly ) ) {
      kdError() << "Can't write dot file '" << _dotName << "'" << endl;
      return;
    }
    stream = new QTextStream(file);
  }

  if (!_graphCreated) createGraph();

  /* Generate dot format...
   * When used for the CallGraphView (in contrast to "Export Callgraph..."),
   * the labels are only dummy placeholders to reserve space for our own
   * drawings.
   */

  *stream << "digraph \"callgraph\" {\n";

  if (_go->layout() == LeftRight) {
      *stream << QString("  rankdir=LR;\n");
  }
  else if (_go->layout() == Circular) {
    TraceFunction *f = 0;
    switch(_item->type()) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
      f = (TraceFunction*) _item;
      break;
    case TraceItem::Call:
      f = ((TraceCall*)_item)->caller(true);
      break;
    default:
      break;
    }
    if (f)
      *stream << QString("  center=F%1;\n").arg((long)f, 0, 16);
    *stream << QString("  overlap=false;\n  splines=true;\n");
  }

  // for clustering
  QMap<TraceCostItem*,QPtrList<GraphNode> > nLists;

  GraphNodeMap::Iterator nit;
  for ( nit = _nodeMap.begin();
        nit != _nodeMap.end(); ++nit ) {
    GraphNode& n = *nit;

    if (n.incl <= _realFuncLimit) continue;

    // for clustering: get cost item group of function
    TraceCostItem* g;
    TraceFunction* f = n.function();
    switch(_groupType) {
    case TraceItem::Object: g = f->object(); break;
    case TraceItem::Class:  g = f->cls(); break;
    case TraceItem::File:   g = f->file(); break;
    case TraceItem::FunctionCycle: g = f->cycle();  break;
    default: g = 0; break;
    }
    nLists[g].append(&n);
  }

  QMap<TraceCostItem*,QPtrList<GraphNode> >::Iterator lit;
  int cluster = 0;
  for ( lit = nLists.begin();
        lit != nLists.end(); ++lit, cluster++ ) {
    QPtrList<GraphNode>& l = lit.data();
    TraceCostItem* i = lit.key();

    if (_go->clusterGroups() && i) {
      QString iabr = i->prettyName();
      if ((int)iabr.length() > Configuration::maxSymbolLength())
	iabr = iabr.left(Configuration::maxSymbolLength()) + "...";

      *stream << QString("subgraph \"cluster%1\" { label=\"%2\";\n")
	.arg(cluster).arg(iabr);
    }

    GraphNode* np;
    for(np = l.first(); np; np = l.next() ) {
      TraceFunction* f = np->function();

      QString abr = f->prettyName();
      if ((int)abr.length() > Configuration::maxSymbolLength())
	abr = abr.left(Configuration::maxSymbolLength()) + "...";
      
      *stream << QString("  F%1 [").arg((long)f, 0, 16);
      if (_useBox) {
	// make label 3 lines for CallGraphView
	*stream << QString("shape=box,label=\"** %1 **\\n**\\n%2\"];\n")
	  .arg(abr)
	  .arg(SubCost(np->incl).pretty());
      }
      else
	*stream << QString("label=\"%1\\n%2\"];\n")
	  .arg(abr)
	  .arg(SubCost(np->incl).pretty());
    }

    if (_go->clusterGroups() && i)
      *stream << QString("}\n");
  }

  GraphEdgeMap::Iterator eit;
  for ( eit = _edgeMap.begin();
        eit != _edgeMap.end(); ++eit ) {
    GraphEdge& e = *eit;

    if (e.cost < _realCallLimit) continue;
    if (!_go->expandCycles()) {
      // don't show inner cycle calls
      if (e.call()->inCycle()>0) continue;
    }


    GraphNode& from = _nodeMap[e.from()];
    GraphNode& to   = _nodeMap[e.to()];

    e.setCallerNode(&from);
    e.setCallingNode(&to);

    if ((from.incl <= _realFuncLimit) ||
        (to.incl <= _realFuncLimit)) continue;

    // remove dumped edges from n.callers/n.callings
    from.callings.removeRef(&e);
    to.callers.removeRef(&e);
    from.callingSet.remove(&e);
    to.callerSet.remove(&e);

    *stream << QString("  F%1 -> F%2 [weight=%3")
      .arg((long)e.from(), 0, 16)
      .arg((long)e.to(), 0, 16)
      .arg((long)log(log(e.cost)));

    if (_go->detailLevel() ==1)
	*stream << QString(",label=\"%1\"")
	    .arg(SubCost(e.cost).pretty());
    else if (_go->detailLevel() ==2)
	*stream << QString(",label=\"%3\\n%4 x\"")
	    .arg(SubCost(e.cost).pretty())
	    .arg(SubCost(e.count).pretty());

    *stream << QString("];\n");
  }

  if (_go->showSkipped()) {

      // Create sum-edges for skipped edges
      GraphEdge* e;
      double costSum, countSum;
      for ( nit = _nodeMap.begin();
	    nit != _nodeMap.end(); ++nit ) {
	  GraphNode& n = *nit;
	  if (n.incl <= _realFuncLimit) continue;

	  costSum = countSum = 0.0;
	  for (e=n.callers.first();e;e=n.callers.next()) {
	      costSum += e->cost;
	      countSum += e->count;
	  }
	  if (costSum > _realCallLimit) {

	      QPair<TraceFunction*,TraceFunction*> p(0, n.function());
	      e = &(_edgeMap[p]);
	      e->setCalling(p.second);
	      e->cost = costSum;
	      e->count = countSum;

	      *stream << QString("  R%1 [shape=point,label=\"\"];\n")
		  .arg((long)n.function(), 0, 16);
	      *stream << QString("  R%1 -> F%2 [label=\"%3\\n%4 x\",weight=%5];\n")
		  .arg((long)n.function(), 0, 16)
		  .arg((long)n.function(), 0, 16)
		  .arg(SubCost(costSum).pretty())
		  .arg(SubCost(countSum).pretty())
		  .arg((int)log(costSum));
	  }

	  costSum = countSum = 0.0;
	  for (e=n.callings.first();e;e=n.callings.next()) {
	      costSum += e->cost;
	      countSum += e->count;
	  }
	  if (costSum > _realCallLimit) {

	      QPair<TraceFunction*,TraceFunction*> p(n.function(), 0);
	      e = &(_edgeMap[p]);
	      e->setCaller(p.first);
	      e->cost = costSum;
	      e->count = countSum;

	      *stream << QString("  S%1 [shape=point,label=\"\"];\n")
		  .arg((long)n.function(), 0, 16);
	      *stream << QString("  F%1 -> S%2 [label=\"%3\\n%4 x\",weight=%5];\n")
		  .arg((long)n.function(), 0, 16)
		  .arg((long)n.function(), 0, 16)
		  .arg(SubCost(costSum).pretty())
		  .arg(SubCost(countSum).pretty())
		  .arg((int)log(costSum));
	  }
      }
  }

  // clear edges here completely.
  // Visible edges are inserted again on parsing in CallGraphView::refresh
  for ( nit = _nodeMap.begin();
	nit != _nodeMap.end(); ++nit ) {
      GraphNode& n = *nit;
      n.callers.clear();
      n.callings.clear();
      n.callerSet.clear();
      n.callingSet.clear();
  }

  *stream << "}\n";

  if (_tmpFile) {
    _tmpFile->close();
  }
  else {
    file->close();
    delete file;
    delete stream;
  }
}

void GraphExporter::sortEdges()
{
    GraphNodeMap::Iterator nit;
    for ( nit = _nodeMap.begin();
	  nit != _nodeMap.end(); ++nit ) {
	GraphNode& n = *nit;

	n.callers.sort();
	n.callings.sort();
  }
}

TraceFunction* GraphExporter::toFunc(QString s)
{
  if (s[0] != 'F') return 0;
  bool ok;
  TraceFunction* f = (TraceFunction*) s.mid(1).toULong(&ok, 16);
  if (!ok) return 0;

  return f;
}

GraphNode* GraphExporter::node(TraceFunction* f)
{
  if (!f) return 0;

  GraphNodeMap::Iterator it = _nodeMap.find(f);
  if (it == _nodeMap.end()) return 0;

  return &(*it);
}

GraphEdge* GraphExporter::edge(TraceFunction* f1, TraceFunction* f2)
{
  GraphEdgeMap::Iterator it = _edgeMap.find(qMakePair(f1, f2));
  if (it == _edgeMap.end()) return 0;

  return &(*it);
}


/**
 * We do a DFS and don't stop on already visited nodes/edges,
 * but add up costs. We only stop if limits/max depth is reached.
 *
 * For a node/edge, it can happen that the first time visited the
 * cost will below the limit, so the search is stopped.
 * If on a further visit of the node/edge the limit is reached,
 * we use the whole node/edge cost and continue search.
 */
void GraphExporter::buildGraph(TraceFunction* f, int d,
                               bool toCallings, double factor)
{
#if DEBUG_GRAPH
  kdDebug() << "buildGraph(" << f->prettyName() << "," << d << "," << factor
	    << ") [to " << (toCallings ? "Callings":"Callers") << "]" << endl;
#endif

  double oldIncl = 0.0;
  GraphNode& n = _nodeMap[f];
  if (n.function() == 0) {
    n.setFunction(f);
  }
  else
    oldIncl = n.incl;

  double incl = f->inclusive()->subCost(_costType) * factor;
  n.incl  += incl;
  n.self += f->subCost(_costType) * factor;
  if (0) qDebug("  Added Incl. %f, now %f", incl, n.incl);

  // A negative depth limit means "unlimited"
  int maxDepth = toCallings ? _go->maxCallingDepth() : _go->maxCallerDepth();
  if ((maxDepth>=0) && (d >= maxDepth)) {
    if (0) qDebug("  Cutoff, max depth reached");
    return;
  }

  // if we just reached the limit by summing, do a DFS
  // from here with full incl. cost because of previous cutoffs
  if ((n.incl >= _realFuncLimit) && (oldIncl < _realFuncLimit)) incl = n.incl;

  if (f->cycle()) {
    // for cycles members, we never stop on first visit, but always on 2nd
    // note: a 2nd visit never should happen, as we don't follow inner-cycle
    //       calls
    if (oldIncl > 0.0) {
      if (0) qDebug("  Cutoff, 2nd visit to Cycle Member");
      // and takeback cost addition, as it's added twice
      n.incl  = oldIncl;
      n.self -= f->subCost(_costType) * factor;
      return;
    }
  }
  else if (incl <= _realFuncLimit) {
    if (0) qDebug("  Cutoff, below limit");
    return;
  }

  TraceCall* call;
  TraceFunction* f2;


  // on entering a cycle, only go the FunctionCycle
  TraceCallList l = toCallings ?
    f->callings(false) : f->callers(false);

  for (call=l.first();call;call=l.next()) {

    f2 = toCallings ? call->called(false) : call->caller(false);

    double count = call->callCount() * factor;
    double cost = call->subCost(_costType) * factor;

    // ignore function calls with absolute cost < 3 per call
    // No: This would skip a lot of functions e.g. with L2 cache misses
    // if (count>0.0 && (cost/count < 3)) continue;

    double oldCost = 0.0;
    QPair<TraceFunction*,TraceFunction*> p(toCallings ? f:f2,
					   toCallings ? f2:f);
    GraphEdge& e = _edgeMap[p];
    if (e.call() == 0) {
      e.setCall(call);
      e.setCaller(p.first);
      e.setCalling(p.second);
    }
    else
      oldCost = e.cost;

    e.cost  += cost;
    e.count += count;
    if (0) qDebug("  Edge to %s, added cost %f, now %f",
                  f2->prettyName().ascii(), cost, e.cost);

    // if this call goes into a FunctionCycle, we also show the real call
    if (f2->cycle() == f2) {
      TraceFunction* realF;
      realF = toCallings ? call->called(true) : call->caller(true);
      QPair<TraceFunction*,TraceFunction*> realP(toCallings ? f:realF,
						 toCallings ? realF:f);
      GraphEdge& e = _edgeMap[realP];
      if (e.call() == 0) {
	e.setCall(call);
	e.setCaller(realP.first);
	e.setCalling(realP.second);
      }
      e.cost  += cost;
      e.count += count;
    }

    // - don't do a DFS on calls in recursion/cycle
    if (call->inCycle()>0) continue;
    if (call->isRecursion()) continue;

    if (toCallings) {
	GraphEdgeSet::Iterator it = n.callingSet.find(&e);
	if (it == n.callingSet.end()) {
	    n.callings.append(&e);
	    n.callingSet.insert(&e, 1 );
	}
    }
    else {
	GraphEdgeSet::Iterator it = n.callerSet.find(&e);
	if (it == n.callerSet.end()) {
	    n.callers.append(&e);
	    n.callerSet.insert(&e, 1 );
	}
    }

    // if we just reached the call limit (=func limit by summing, do a DFS
    // from here with full incl. cost because of previous cutoffs
    if ((e.cost >= _realCallLimit) && (oldCost < _realCallLimit)) cost = e.cost;
    if (cost < _realCallLimit) {
      if (0) qDebug("  Edge Cutoff, limit not reached");
      continue;
    }

    SubCost s;
    if (call->inCycle())
      s = f2->cycle()->inclusive()->subCost(_costType);
    else
      s = f2->inclusive()->subCost(_costType);
    SubCost v = call->subCost(_costType);
    buildGraph(f2, d+1, toCallings, factor * v / s);
  }
}


//
// PannerView
//
PannerView::PannerView(QWidget * parent, const char * name)
  : QCanvasView(parent, name, WNoAutoErase | WStaticContents)
{
  _movingZoomRect = false;

  // why doesn't this avoid flicker ?
  viewport()->setBackgroundMode(Qt::NoBackground);
  setBackgroundMode(Qt::NoBackground);
}

void PannerView::setZoomRect(QRect r)
{
  QRect oldRect = _zoomRect;
  _zoomRect = r;
  updateContents(oldRect);
  updateContents(_zoomRect);
}

void PannerView::drawContents(QPainter * p, int clipx, int clipy, int clipw, int cliph)
{
  // save/restore around QCanvasView::drawContents seems to be needed
  // for QT 3.0 to get the red rectangle drawn correct
  p->save();
  QCanvasView::drawContents(p,clipx,clipy,clipw,cliph);
  p->restore();
  if (_zoomRect.isValid()) {
    p->setPen(red.dark());
    p->drawRect(_zoomRect);
    p->setPen(red);
    p->drawRect(QRect(_zoomRect.x()+1, _zoomRect.y()+1,
		      _zoomRect.width()-2, _zoomRect.height()-2));
  }
}

void PannerView::contentsMousePressEvent(QMouseEvent* e)
{
  if (_zoomRect.isValid()) {
    if (!_zoomRect.contains(e->pos()))
      emit zoomRectMoved(e->pos().x() - _zoomRect.center().x(),
                         e->pos().y() - _zoomRect.center().y());

    _movingZoomRect = true;
    _lastPos = e->pos();
  }
}

void PannerView::contentsMouseMoveEvent(QMouseEvent* e)
{
  if (_movingZoomRect) {
    emit zoomRectMoved(e->pos().x() - _lastPos.x(), e->pos().y() - _lastPos.y());
    _lastPos = e->pos();
  }
}

void PannerView::contentsMouseReleaseEvent(QMouseEvent*)
{
    _movingZoomRect = false;
    emit zoomRectMoveFinished();
}





//
// CanvasNode
//

CanvasNode::CanvasNode(CallGraphView* v, GraphNode* n,
		       int x, int y, int w, int h, QCanvas* c)
    : QCanvasRectangle(x, y, w, h, c), _node(n), _view(v)
{
    setPosition(0, DrawParams::TopCenter);
    setPosition(1, DrawParams::BottomCenter);

    updateGroup();

    if (!_node || !_view) return;

    if (_node->function())
	setText(0, _node->function()->prettyName());

    TraceCost* totalCost;
    if (_view->topLevel()->showExpanded()) {
      if (_view->activeFunction()) {
        if (_view->activeFunction()->cycle())
          totalCost = _view->activeFunction()->cycle()->inclusive();
        else
          totalCost = _view->activeFunction()->inclusive();
      }
      else
        totalCost = (TraceCost*) _view->activeItem();
    }
    else
	totalCost = _view->data();
    double total = totalCost->subCost(_view->costType());
    double inclP  = 100.0 * n->incl / total;
    if (_view->topLevel()->showPercentage())
	setText(1, QString("%1 %")
		.arg(inclP, 0, 'f', Configuration::percentPrecision()));
    else
	setText(1, SubCost(n->incl).pretty());
    setPixmap(1, percentagePixmap(25,10,(int)(inclP+.5), Qt::blue, true));
}

void CanvasNode::setSelected(bool s)
{
  StoredDrawParams::setSelected(s);
  update();
}

void CanvasNode::updateGroup()
{
    if (!_view || !_node) return;

    QColor c = Configuration::functionColor(_view->groupType(),
					    _node->function());
    setBackColor(c);
    update();
}

void CanvasNode::drawShape(QPainter& p)
{
    QRect r = rect(), origRect = r;

  r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);

  RectDrawing d(r);
  d.drawBack(&p, this);
  r.setRect(r.x()+2, r.y()+2, r.width()-4, r.height()-4);

  if (StoredDrawParams::selected() && _view->hasFocus()) {
    _view->style().drawPrimitive( QStyle::PE_FocusRect, &p, r,
				  _view->colorGroup());
  }

  // draw afterwards to always get a frame even when zoomed
  p.setPen(StoredDrawParams::selected() ? red : black);
  p.drawRect(origRect);

  d.setRect(r);
  d.drawField(&p, 0, this);
  d.drawField(&p, 1, this);
}


//
// CanvasEdgeLabel
//

CanvasEdgeLabel::CanvasEdgeLabel(CallGraphView* v, CanvasEdge* ce,
                                 int x, int y, int w, int h, QCanvas* c)
    : QCanvasRectangle(x, y, w, h, c), _ce(ce), _view(v)
{
    GraphEdge* e = ce->edge();
    if (!e) return;

    setPosition(1, DrawParams::TopCenter);
    setText(1, QString("%1 x").arg(SubCost(e->count).pretty()));

    setPosition(0, DrawParams::BottomCenter);

    TraceCost* totalCost;
    if (_view->topLevel()->showExpanded()) {
      if (_view->activeFunction()) {
        if (_view->activeFunction()->cycle())
          totalCost = _view->activeFunction()->cycle()->inclusive();
        else
          totalCost = _view->activeFunction()->inclusive();
      }
      else
        totalCost = (TraceCost*) _view->activeItem();
    }
    else
        totalCost = _view->data();
    double total = totalCost->subCost(_view->costType());
    double inclP  = 100.0 * e->cost / total;
    if (_view->topLevel()->showPercentage())
        setText(0, QString("%1 %")
		.arg(inclP, 0, 'f', Configuration::percentPrecision()));
    else
        setText(0, SubCost(e->cost).pretty());
    setPixmap(0, percentagePixmap(25,10,(int)(inclP+.5), Qt::blue, true));

    if (e->call() && (e->call()->isRecursion() || e->call()->inCycle())) {
	QString icon = "undo";
	KIconLoader* loader = KApplication::kApplication()->iconLoader();
	QPixmap p= loader->loadIcon(icon, KIcon::Small, 0,
				      KIcon::DefaultState, 0, true);
	setPixmap(0, p);
    }
}

void CanvasEdgeLabel::drawShape(QPainter& p)
{
  QRect r = rect();
  //p.setPen(blue);
  //p.drawRect(r);
  RectDrawing d(r);
  d.drawField(&p, 0, this);
  d.drawField(&p, 1, this);
}

//
// CanvasEdgeArrow

CanvasEdgeArrow::CanvasEdgeArrow(CanvasEdge* ce, QCanvas* c)
    : QCanvasPolygon(c), _ce(ce)
{}

void CanvasEdgeArrow::drawShape(QPainter& p)
{
    if (_ce->isSelected()) p.setBrush(Qt::red);

    QCanvasPolygon::drawShape(p);
}

//
// CanvasEdge
//

CanvasEdge::CanvasEdge(GraphEdge* e, QCanvas* c)
  : QCanvasSpline(c), _edge(e)
{
    _label = 0;
    _arrow = 0;
}

void CanvasEdge::setSelected(bool s)
{
    QCanvasItem::setSelected(s);
    update();
    if (_arrow) _arrow->setSelected(s);
}

QPointArray CanvasEdge::areaPoints() const
{
  int minX = poly[0].x(), minY = poly[0].y();
  int maxX = minX, maxY = minY;
  int i;

  if (0) qDebug("CanvasEdge::areaPoints\n  P 0: %d/%d", minX, minY);
  int len = poly.count();
  for (i=1;i<len;i++) {
    if (poly[i].x() < minX) minX = poly[i].x();
    if (poly[i].y() < minY) minY = poly[i].y();
    if (poly[i].x() > maxX) maxX = poly[i].x();
    if (poly[i].y() > maxY) maxY = poly[i].y();
    if (0) qDebug("  P %d: %d/%d", i, poly[i].x(), poly[i].y());
  }
  QPointArray a = poly.copy(),  b = poly.copy();
  if (minX == maxX) {
    a.translate(-2, 0);
    b.translate(2, 0);
  }
  else {
    a.translate(0, -2);
    b.translate(0, 2);
  }
  a.resize(2*len);
  for (i=0;i<len;i++)
    a[2 * len - 1 -i] = b[i];

  if (0) {
      qDebug(" Result:");
      for (i=0;i<2*len;i++)
	  qDebug("  P %d: %d/%d", i, a[i].x(), a[i].y());
  }

  return a;
}

void CanvasEdge::drawShape(QPainter& p)
{
    if (isSelected()) p.setPen(Qt::red);

    p.drawPolyline(poly);
}


//
// CanvasFrame
//

QPixmap* CanvasFrame::_p = 0;

CanvasFrame::CanvasFrame(CanvasNode* n, QCanvas* c)
 : QCanvasRectangle(c)
{
  if (!_p) {

    int d = 5;
    float v1 = 130.0, v2 = 10.0, v = v1, f = 1.03;

    // calculate pix size
    QRect r(0, 0, 30, 30);
    while (v>v2) {
      r.setRect(r.x()-d, r.y()-d, r.width()+2*d, r.height()+2*d);
      v /= f;
    }

    _p = new QPixmap(r.size());
    _p->fill(Qt::white);
    QPainter p(_p);
    p.setPen(Qt::NoPen);

    r.moveBy(-r.x(), -r.y());

    while (v<v1) {
      v *= f;
      p.setBrush(QColor(265-(int)v, 265-(int)v, 265-(int)v));

      p.drawRect(QRect(r.x(), r.y(), r.width(), d));
      p.drawRect(QRect(r.x(), r.bottom()-d, r.width(), d));
      p.drawRect(QRect(r.x(), r.y()+d, d, r.height()-2*d));
      p.drawRect(QRect(r.right()-d, r.y()+d, d, r.height()-2*d));

      r.setRect(r.x()+d, r.y()+d, r.width()-2*d, r.height()-2*d);
    }
  }

  setSize(_p->width(), _p->height());
  move(n->rect().center().x()-_p->width()/2,
       n->rect().center().y()-_p->height()/2);
}


void CanvasFrame::drawShape(QPainter& p)
{
  p.drawPixmap( int(x()), int(y()), *_p );
}




//
// Tooltips for CallGraphView
//

class CallGraphTip: public QToolTip
{
public:
  CallGraphTip( QWidget* p ):QToolTip(p) {}

protected:
    void maybeTip( const QPoint & );
};

void CallGraphTip::maybeTip( const QPoint& pos )
{
  if (!parentWidget()->inherits( "CallGraphView" )) return;
  CallGraphView* cgv = (CallGraphView*)parentWidget();

  QPoint cPos = cgv->viewportToContents(pos);

  if (0) qDebug("CallGraphTip for (%d/%d) -> (%d/%d) ?",
		pos.x(), pos.y(), cPos.x(), cPos.y());

  QCanvasItemList l = cgv->canvas()->collisions(cPos);
  if (l.count() == 0) return;
  QCanvasItem* i = l.first();

  if (i->rtti() == CANVAS_NODE) {
      CanvasNode* cn = (CanvasNode*)i;
      GraphNode* n = cn->node();
      if (0) qDebug("CallGraphTip: Mouse on Node '%s'",
		    n->function()->prettyName().ascii());

      QString tipStr = QString("%1 (%2)").arg(cn->text(0)).arg(cn->text(1));
      QPoint vPosTL = cgv->contentsToViewport(i->boundingRect().topLeft());
      QPoint vPosBR = cgv->contentsToViewport(i->boundingRect().bottomRight());
      tip(QRect(vPosTL, vPosBR), tipStr);

      return;
  }

  // redirect from label / arrow to edge
  if (i->rtti() == CANVAS_EDGELABEL)
      i = ((CanvasEdgeLabel*)i)->canvasEdge();
  if (i->rtti() == CANVAS_EDGEARROW)
      i = ((CanvasEdgeArrow*)i)->canvasEdge();

  if (i->rtti() == CANVAS_EDGE) {
      CanvasEdge* ce = (CanvasEdge*)i;
      GraphEdge* e = ce->edge();
      if (0) qDebug("CallGraphTip: Mouse on Edge '%s'",
		    e->prettyName().ascii());

      QString tipStr;
      if (!ce->label())
	  tipStr = e->prettyName();
      else
	  tipStr = QString("%1 (%2)")
	      .arg(ce->label()->text(0)).arg(ce->label()->text(1));
      tip(QRect(pos.x()-5,pos.y()-5,pos.x()+5,pos.y()+5), tipStr);
  }
}




//
// CallGraphView
//
CallGraphView::CallGraphView(TraceItemView* parentView,
                             QWidget* parent, const char* name)
  : QCanvasView(parent, name), TraceItemView(parentView)
{
    _zoomPosition = DEFAULT_ZOOMPOS;
    _lastAutoPosition = TopLeft;

  _canvas = 0;
  _xMargin = _yMargin = 0;
  _completeView = new PannerView(this);
  _cvZoom = 1;
  _selectedNode = 0;
  _selectedEdge = 0;

  _exporter.setGraphOptions(this);

  _completeView->setVScrollBarMode(QScrollView::AlwaysOff);
  _completeView->setHScrollBarMode(QScrollView::AlwaysOff);
  _completeView->raise();
  _completeView->hide();

  setFocusPolicy(QWidget::StrongFocus);
  setBackgroundMode(Qt::NoBackground);

  connect(this, SIGNAL(contentsMoving(int,int)),
          this, SLOT(contentsMovingSlot(int,int)));
  connect(_completeView, SIGNAL(zoomRectMoved(int,int)),
          this, SLOT(zoomRectMoved(int,int)));
  connect(_completeView, SIGNAL(zoomRectMoveFinished()),
          this, SLOT(zoomRectMoveFinished()));

  QWhatsThis::add( this, whatsThis() );

  // tooltips...
  _tip = new CallGraphTip(this);

  _renderProcess = 0;
  _prevSelectedNode = 0;
  connect(&_renderTimer, SIGNAL(timeout()),
          this, SLOT(showRenderWarning()));
}

CallGraphView::~CallGraphView()
{
  delete _completeView;
  delete _tip;

  if (_canvas) {
    setCanvas(0);
    delete _canvas;
  }
}

QString CallGraphView::whatsThis() const
{
    return i18n( "<b>Call Graph around active Function</b>"
                 "<p>Depending on configuration, this view shows "
                 "the call graph environment of the active function. "
		 "Note: the shown cost is <b>only</b> the cost which is "
		 "spent while the active function was actually running; "
		 "i.e. the cost shown for main() - if it's visible - should "
		 "be the same as the cost of the active function, as that's "
		 "the part of inclusive cost of main() spent while the active "
		 "function was running.</p>"
		 "<p>For cycles, blue call arrows indicate that this is an "
		 "artificial call added for correct drawing which "
		 "actually never happened.</p>"
                 "<p>If the graph is larger than the widget area, an overview "
                 "panner is shown in one edge. "
                 "There are similar visualization options to the "
                 "Call Treemap; the selected function is highlighted.<p>");
}

void CallGraphView::updateSizes(QSize s)
{
  if (!_canvas) return;

    if (s == QSize(0,0)) s = size();

    // the part of the canvas that should be visible
    int cWidth  = _canvas->width()  - 2*_xMargin + 100;
    int cHeight = _canvas->height() - 2*_yMargin + 100;

    // hide birds eye view if no overview needed
    if (!_data || !_activeItem ||
	((cWidth < s.width()) && cHeight < s.height())) {
      _completeView->hide();
      return;
    }
    _completeView->show();

    // first, assume use of 1/3 of width/height (possible larger)
    double zoom = .33 * s.width() / cWidth;
    if (zoom * cHeight < .33 * s.height()) zoom = .33 * s.height() / cHeight;

    // fit to widget size
    if (cWidth  * zoom  > s.width())   zoom = s.width() / (double)cWidth;
    if (cHeight * zoom  > s.height())  zoom = s.height() / (double)cHeight;

    // scale to never use full height/width
    zoom = zoom * 3/4;

    // at most a zoom of 1/3
    if (zoom > .33) zoom = .33;

    if (zoom != _cvZoom) {
      _cvZoom = zoom;
      if (0) qDebug("Canvas Size: %dx%d, Visible: %dx%d, Zoom: %f",
		    _canvas->width(), _canvas->height(),
		    cWidth, cHeight, zoom);

      QWMatrix wm;
      wm.scale( zoom, zoom );
      _completeView->setWorldMatrix(wm);

      // make it a little bigger to compensate for widget frame
      _completeView->resize(int(cWidth * zoom) + 4,
                            int(cHeight * zoom) + 4);

      // update ZoomRect in completeView
      contentsMovingSlot(contentsX(), contentsY());
    }

    _completeView->setContentsPos(int(zoom*(_xMargin-50)),
				  int(zoom*(_yMargin-50)));

    int cvW = _completeView->width();
    int cvH = _completeView->height();
    int x = width()- cvW - verticalScrollBar()->width()    -2;
    int y = height()-cvH - horizontalScrollBar()->height() -2;
    QPoint oldZoomPos = _completeView->pos();
    QPoint newZoomPos = QPoint(0,0);
    ZoomPosition zp = _zoomPosition;
    if (zp == Auto) {
	QPoint tl1Pos = viewportToContents(QPoint(0,0));
	QPoint tl2Pos = viewportToContents(QPoint(cvW,cvH));
	QPoint tr1Pos = viewportToContents(QPoint(x,0));
	QPoint tr2Pos = viewportToContents(QPoint(x+cvW,cvH));
	QPoint bl1Pos = viewportToContents(QPoint(0,y));
	QPoint bl2Pos = viewportToContents(QPoint(cvW,y+cvH));
	QPoint br1Pos = viewportToContents(QPoint(x,y));
	QPoint br2Pos = viewportToContents(QPoint(x+cvW,y+cvH));
	int tlCols = _canvas->collisions(QRect(tl1Pos,tl2Pos)).count();
	int trCols = _canvas->collisions(QRect(tr1Pos,tr2Pos)).count();
	int blCols = _canvas->collisions(QRect(bl1Pos,bl2Pos)).count();
	int brCols = _canvas->collisions(QRect(br1Pos,br2Pos)).count();
	int minCols = tlCols;
	zp = _lastAutoPosition;
	switch(zp) {
	case TopRight:    minCols = trCols; break;
	case BottomLeft:  minCols = blCols; break;
	case BottomRight: minCols = brCols; break;
	default:
	case TopLeft:     minCols = tlCols; break;
	}
	if (minCols > tlCols) { minCols = tlCols; zp = TopLeft; }
	if (minCols > trCols) { minCols = trCols; zp = TopRight; }
	if (minCols > blCols) { minCols = blCols; zp = BottomLeft; }
	if (minCols > brCols) { minCols = brCols; zp = BottomRight; }

	_lastAutoPosition = zp;
    }

    switch(zp) {
    case TopRight:
	newZoomPos = QPoint(x,0);
	break;
    case BottomLeft:
	newZoomPos = QPoint(0,y);
	break;
    case BottomRight:
	newZoomPos = QPoint(x,y);
	break;
    default:
	break;
    }
    if (newZoomPos != oldZoomPos) _completeView->move(newZoomPos);
}

void CallGraphView::focusInEvent(QFocusEvent*)
{
    if (!_canvas) return;

    if (_selectedNode && _selectedNode->canvasNode()) {
	_selectedNode->canvasNode()->setSelected(true); // requests item update
	_canvas->update();
    }
}

void CallGraphView::focusOutEvent(QFocusEvent* e)
{
    // trigger updates as in focusInEvent
    focusInEvent(e);
}

void CallGraphView::keyPressEvent(QKeyEvent* e)
{
    if (!_canvas) {
	e->ignore();
	return;
    }

    if ((e->key() == Key_Return) ||
	(e->key() == Key_Space)) {
	if (_selectedNode)
	    activated(_selectedNode->function());
	else if (_selectedEdge && _selectedEdge->call())
	    activated(_selectedEdge->call());
	return;
    }

    // move selected node/edge
    if (!(e->state() & (ShiftButton | ControlButton)) &&
	(_selectedNode || _selectedEdge) &&
	((e->key() == Key_Up) ||
	 (e->key() == Key_Down) ||
	 (e->key() == Key_Left) ||
	 (e->key() == Key_Right))) {

	TraceFunction* f = 0;
	TraceCall* c = 0;

	// rotate arrow key meaning for LeftRight layout
	int key = e->key();
	if (_layout == LeftRight) {
	    switch(key) {
	    case Key_Up:    key = Key_Left; break;
	    case Key_Down:  key = Key_Right; break;
	    case Key_Left:  key = Key_Up; break;
	    case Key_Right: key = Key_Down; break;
	    default: break;
	    }
	}

	if (_selectedNode) {
	    if (key == Key_Up)    c = _selectedNode->visibleCaller();
	    if (key == Key_Down)  c = _selectedNode->visibleCalling();
	    if (key == Key_Right) f = _selectedNode->nextVisible();
	    if (key == Key_Left)  f = _selectedNode->priorVisible();
	}
	else if (_selectedEdge) {
	    if (key == Key_Up)    f = _selectedEdge->visibleCaller();
	    if (key == Key_Down)  f = _selectedEdge->visibleCalling();
	    if (key == Key_Right) c = _selectedEdge->nextVisible();
	    if (key == Key_Left)  c = _selectedEdge->priorVisible();
	}

	if (c) selected(c);
	if (f) selected(f);
	return;
    }

    // move canvas...
    if (e->key() == Key_Home)
	scrollBy(-_canvas->width(),0);
    else if (e->key() == Key_End)
	scrollBy(_canvas->width(),0);
    else if (e->key() == Key_Prior)
	scrollBy(0,-visibleHeight()/2);
    else if (e->key() == Key_Next)
	scrollBy(0,visibleHeight()/2);
    else if (e->key() == Key_Left)
	scrollBy(-visibleWidth()/10,0);
    else if (e->key() == Key_Right)
	scrollBy(visibleWidth()/10,0);
    else if (e->key() == Key_Down)
	scrollBy(0,visibleHeight()/10);
    else if (e->key() == Key_Up)
	scrollBy(0,-visibleHeight()/10);
    else e->ignore();
}

void CallGraphView::resizeEvent(QResizeEvent* e)
{
  QCanvasView::resizeEvent(e);
  if (_canvas) updateSizes(e->size());
}

TraceItem* CallGraphView::canShow(TraceItem* i)
{
  if (i) {
    switch(i->type()) {
    case TraceItem::Function:
    case TraceItem::FunctionCycle:
    case TraceItem::Call:
      return i;
    default:
      break;
    }
  }
  return 0;
}

void CallGraphView::doUpdate(int changeType)
{
  // Special case ?
  if (changeType == costType2Changed) return;

  if (changeType == selectedItemChanged) {
    if (!_canvas) return;

    if (!_selectedItem) return;

    GraphNode* n = 0;
    GraphEdge* e = 0;
    if ((_selectedItem->type() == TraceItem::Function) ||
        (_selectedItem->type() == TraceItem::FunctionCycle)) {
	n = _exporter.node((TraceFunction*)_selectedItem);
	if (n == _selectedNode) return;
    }
    else if (_selectedItem->type() == TraceItem::Call) {
	TraceCall* c = (TraceCall*)_selectedItem;
	e = _exporter.edge(c->caller(false), c->called(false));
	if (e == _selectedEdge) return;
    }

    // unselected any selected item
    if (_selectedNode && _selectedNode->canvasNode()) {
	_selectedNode->canvasNode()->setSelected(false);
    }
    _selectedNode = 0;
    if (_selectedEdge && _selectedEdge->canvasEdge()) {
	_selectedEdge->canvasEdge()->setSelected(false);
    }
    _selectedEdge = 0;

    // select
    CanvasNode* sNode = 0;
    if (n && n->canvasNode()) {
	_selectedNode = n;
	_selectedNode->canvasNode()->setSelected(true);

	if (!_isMoving) sNode = _selectedNode->canvasNode();
    }
    if (e && e->canvasEdge()) {
	_selectedEdge = e;
	_selectedEdge->canvasEdge()->setSelected(true);

#if 0 // don't change position when selecting edge
	if (!_isMoving) {
	    if (_selectedEdge->fromNode())
		sNode = _selectedEdge->fromNode()->canvasNode();
	    if (!sNode && _selectedEdge->toNode())
		sNode = _selectedEdge->toNode()->canvasNode();
	}
#endif
    }
    if (sNode) {
	double x = sNode->x() + sNode->width()/2;
	double y = sNode->y() + sNode->height()/2;

	ensureVisible(int(x),int(y),
                      sNode->width()/2+50, sNode->height()/2+50);
    }

    _canvas->update();
    return;
  }

  if (changeType == groupTypeChanged) {
    if (!_canvas) return;

    if (_clusterGroups) { 
      refresh(); 
      return;
    }

    QCanvasItemList l = _canvas->allItems();
    QCanvasItemList::iterator it;
    for (it = l.begin();it != l.end(); ++it)
	if ((*it)->rtti() == CANVAS_NODE)
	    ((CanvasNode*) (*it))->updateGroup();

    _canvas->update();
    return;
  }

  if (changeType & dataChanged) {
      // invalidate old selection and graph part
    _exporter.reset(_data, _activeItem, _costType, _groupType);
      _selectedNode = 0;
      _selectedEdge = 0;
  }

  refresh();
}

void CallGraphView::clear()
{
  if (!_canvas) return;

  delete _canvas;
  _canvas = 0;
  _completeView->setCanvas(0);
  setCanvas(0);
}

void CallGraphView::showText(QString s)
{
  clear();
  _renderTimer.stop();

  _canvas = new QCanvas(QApplication::desktop()->width(),
			QApplication::desktop()->height());

  QCanvasText* t = new QCanvasText(s, _canvas);
  t->move(5, 5);
  t->show();
  center(0,0);
  setCanvas(_canvas);
  _canvas->update();
  _completeView->hide();
}

void CallGraphView::showRenderWarning()
{
  QString s;

  if (_renderProcess)
    s =i18n("Warning: a long lasting graph layouting is in progress.\n"
	    "Reduce node/edge limits for speedup.\n");
  else
    s = i18n("Layouting stopped.\n");
  
  s.append(i18n("The call graph has %1 nodes and %2 edges.\n")
	   .arg(_exporter.nodeCount())
	   .arg(_exporter.edgeCount()));

  showText(s);
}

void CallGraphView::stopRendering()
{
  if (!_renderProcess) return;

  _renderProcess->kill();
  delete _renderProcess;
  _renderProcess = 0;
  _unparsedOutput = QString::null;

  _renderTimer.start(200, true);
}

void CallGraphView::refresh()
{
  // trigger start of background rendering
  if (_renderProcess) stopRendering();

  // we want to keep a selected node item at the same global position
  _prevSelectedNode = _selectedNode;
  _prevSelectedPos = QPoint(-1,-1);
  if (_selectedNode) {
    QPoint center = _selectedNode->canvasNode()->boundingRect().center();
    _prevSelectedPos  = contentsToViewport(center);
  }

  if (!_data || !_activeItem) {
    showText(i18n("No item activated for which to draw the call graph."));
    return;
  }

  TraceItem::CostType t = _activeItem->type();
  switch(t) {
  case TraceItem::Function:
  case TraceItem::FunctionCycle:
  case TraceItem::Call:
    break;
  default:
    showText(i18n("No call graph can be drawn for the active item."));
    return;
  }

  if (1) kdDebug() << "CallGraphView::refresh" << endl;

  _selectedNode = 0;
  _selectedEdge = 0;
  _exporter.reset(_data, _activeItem, _costType, _groupType);
  _exporter.writeDot();

  _renderProcess = new QProcess(this);
  if (_layout == GraphOptions::Circular)
    _renderProcess->addArgument( "twopi" );
  else
    _renderProcess->addArgument( "dot" );
  _renderProcess->addArgument(_exporter.filename());
  _renderProcess->addArgument( "-Tplain" );

  connect( _renderProcess, SIGNAL(readyReadStdout()),
	   this, SLOT(readDotOutput()) );
  connect( _renderProcess, SIGNAL(processExited()),
	   this, SLOT(dotExited()) );

  if (1) kdDebug() << "Running '" 
		   << _renderProcess->arguments().join(" ")
		   << "'..." << endl;

  if ( !_renderProcess->start() ) {
    QString e = i18n("No call graph is available because the following\n"
		     "command cannot be run:\n'%1'\n")
      .arg(_renderProcess->arguments().join(" "));
    e += i18n("Please check that 'dot' is installed (package GraphViz).");
    showText(e);

    delete _renderProcess;
    _renderProcess = 0;

    return;
  }

  _unparsedOutput = QString::null;

  // layouting of more than seconds is dubious
  _renderTimer.start(1000, true);  
}

void CallGraphView::readDotOutput()
{
  _unparsedOutput.append( _renderProcess->readStdout() );
}

void CallGraphView::dotExited()
{
  QString line, cmd;
  CanvasNode *rItem;
  QCanvasEllipse* eItem;
  CanvasEdge* sItem;
  CanvasEdgeLabel* lItem;
  QTextStream* dotStream;
  double scale = 1.0, scaleX = 1.0, scaleY = 1.0;
  double dotWidth, dotHeight;
  GraphNode* activeNode = 0;
  GraphEdge* activeEdge = 0;

  _renderTimer.stop();
  viewport()->setUpdatesEnabled(false);
  clear();
  dotStream = new QTextStream(_unparsedOutput, IO_ReadOnly);

  int lineno = 0;
  while (1) {
    line = dotStream->readLine();
    if (line.isNull()) break;
    lineno++;
    if (line.isEmpty()) continue;

    QTextStream lineStream(line, IO_ReadOnly);
    lineStream >> cmd;

    if (0) qDebug("%s:%d - line '%s', cmd '%s'",
                  _exporter.filename().ascii(), lineno,
                  line.ascii(), cmd.ascii());

    if (cmd == "stop") break;

    if (cmd == "graph") {
      QString dotWidthString, dotHeightString;
      lineStream >> scale >> dotWidthString >> dotHeightString;
      dotWidth = dotWidthString.toDouble();
      dotHeight = dotHeightString.toDouble();

      if (_detailLevel == 0)      { scaleX = scale * 70; scaleY = scale * 40; }
      else if (_detailLevel == 1) { scaleX = scale * 80; scaleY = scale * 70; }
      else                        { scaleX = scale * 60; scaleY = scale * 100; }

      if (!_canvas) {
        int w = (int)(scaleX * dotWidth);
        int h = (int)(scaleY * dotHeight);

	// We use as minimum canvas size the desktop size.
	// Otherwise, the canvas would have to be resized on widget resize.
	_xMargin = 50;
	if (w < QApplication::desktop()->width())
	    _xMargin += (QApplication::desktop()->width()-w)/2;

	_yMargin = 50;
	if (h < QApplication::desktop()->height())
	    _yMargin += (QApplication::desktop()->height()-h)/2;

        _canvas = new QCanvas(int(w+2*_xMargin), int(h+2*_yMargin));

#if DEBUG_GRAPH
        kdDebug() << _exporter.filename().ascii() << ":" << lineno
		  << " - graph (" << dotWidth << " x " << dotHeight
		  << ") => (" << w << " x " << h << ")" << endl;
#endif
      }
      else
        kdWarning() << "Ignoring 2nd 'graph' from dot ("
		    << _exporter.filename() << ":" << lineno << ")" << endl;
      continue;
    }

    if ((cmd != "node") && (cmd != "edge")) {
      kdWarning() << "Ignoring unknown command '" << cmd << "' from dot ("
		    << _exporter.filename() << ":" << lineno << ")" << endl;
      continue;
    }

    if (_canvas == 0) {
      kdWarning() << "Ignoring '" << cmd << "' without 'graph' from dot ("
		  << _exporter.filename() << ":" << lineno << ")" << endl;
      continue;
    }

    if (cmd == "node") {
      // x, y are centered in node
      QString nodeName, label, nodeX, nodeY, nodeWidth, nodeHeight;
      double x, y, width, height;
      lineStream >> nodeName >> nodeX >> nodeY >> nodeWidth >> nodeHeight;
      x = nodeX.toDouble();
      y = nodeY.toDouble();
      width = nodeWidth.toDouble();
      height = nodeHeight.toDouble();

      GraphNode* n = _exporter.node(_exporter.toFunc(nodeName));

      int xx = (int)(scaleX * x + _xMargin);
      int yy = (int)(scaleY * (dotHeight - y) + _yMargin);
      int w = (int)(scaleX * width);
      int h = (int)(scaleY * height);

#if DEBUG_GRAPH
      kdDebug() << _exporter.filename() << ":" << lineno
		<< " - node '" << nodeName << "' ( "
		<< x << "/" << y << " - "
		<< width << "x" << height << " ) => ("
		<< xx-w/2 << "/" << yy-h/2 << " - "
		<< w << "x" << h << ")" << endl;
#endif


      // Unnamed nodes with collapsed edges (with 'R' and 'S')
      if (nodeName[0] == 'R' || nodeName[0] == 'S') {
	  w = 10, h = 10;
        eItem = new QCanvasEllipse(w, h, _canvas);
        eItem->move(xx, yy);
        eItem->setBrush(Qt::gray);
        eItem->setZ(1.0);
        eItem->show();
        continue;
      }

      if (!n) {
	  qDebug("Warning: Unknown function '%s' ?!", nodeName.ascii());
	  continue;
      }
      n->setVisible(true);

      rItem = new CanvasNode(this, n, xx-w/2, yy-h/2, w, h, _canvas);
      n->setCanvasNode(rItem);

      if (n) {
        if (n->function() == activeItem()) activeNode = n;
        if (n->function() == selectedItem()) _selectedNode = n;
        rItem->setSelected(n == _selectedNode);
      }

      rItem->setZ(1.0);
      rItem->show();

      continue;
    }

    // edge

    QString node1Name, node2Name, label, edgeX, edgeY;
    double x, y;
    QPointArray pa;
    int points, i;
    lineStream >> node1Name >> node2Name >> points;

    GraphEdge* e = _exporter.edge(_exporter.toFunc(node1Name),
                                  _exporter.toFunc(node2Name));
    if (!e) {
      kdWarning() << "Unknown edge '" << node1Name << "'-'"
		  << node2Name << "' from dot ("
		  << _exporter.filename() << ":" << lineno << ")" << endl;
      continue;
    }
    e->setVisible(true);
    if (e->fromNode()) e->fromNode()->callings.append(e);
    if (e->toNode()) e->toNode()->callers.append(e);

    if (0) qDebug("  Edge with %d points:", points);

    pa.resize(points);
    for (i=0;i<points;i++) {
      if (lineStream.atEnd()) break;
      lineStream >> edgeX >> edgeY;
      x = edgeX.toDouble();
      y = edgeY.toDouble();

      int xx = (int)(scaleX * x + _xMargin);
      int yy = (int)(scaleY * (dotHeight - y) + _yMargin);

      if (0) qDebug("   P %d: ( %f / %f ) => ( %d / %d)",
                    i, x, y, xx, yy);

      pa.setPoint(i, xx, yy);
    }
    if (i < points) {
      qDebug("CallGraphView: Can't read %d spline points (%s:%d)",
             points, _exporter.filename().ascii(), lineno);
      continue;
    }

    // artifical calls should be blue
    bool isArtifical = false;
    TraceFunction* caller = e->fromNode()->function();
    TraceFunction* called = e->toNode()->function();
    if (caller->cycle() == caller) isArtifical = true;
    if (called->cycle() == called) isArtifical = true;
    QColor arrowColor = isArtifical ? Qt::blue : Qt::black;

    sItem = new CanvasEdge(e, _canvas);
    e->setCanvasEdge(sItem);
    sItem->setControlPoints(pa, false);
    sItem->setPen(QPen(arrowColor, 1 /*(int)log(log(e->cost))*/ ));
    sItem->setZ(0.5);
    sItem->show();

    if (e->call() == selectedItem()) _selectedEdge = e;
    if (e->call() == activeItem()) activeEdge = e;
    sItem->setSelected(e == _selectedEdge);

    // Arrow head
    QPoint arrowDir;
    int indexHead = -1;

    // check if head is at start of spline...
    // this is needed because dot always gives points from top to bottom
    CanvasNode* fromNode = e->fromNode() ? e->fromNode()->canvasNode() : 0;
    if (fromNode) {
      QPoint toCenter = fromNode->rect().center();
      int dx0 = pa.point(0).x() - toCenter.x();
      int dy0 = pa.point(0).y() - toCenter.y();
      int dx1 = pa.point(points-1).x() - toCenter.x();
      int dy1 = pa.point(points-1).y() - toCenter.y();
      if (dx0*dx0+dy0*dy0 > dx1*dx1+dy1*dy1) {
	// start of spline is nearer to call target node
	indexHead=-1;
	while(arrowDir.isNull() && (indexHead<points-2)) {
	  indexHead++;
	  arrowDir = pa.point(indexHead) - pa.point(indexHead+1);
	}
      }
    }

    if (arrowDir.isNull()) {
      indexHead = points;
      // sometimes the last spline points from dot are the same...
      while(arrowDir.isNull() && (indexHead>1)) {
	indexHead--;
	arrowDir = pa.point(indexHead) - pa.point(indexHead-1);
      }
    }

    if (!arrowDir.isNull()) {
	// arrow around pa.point(indexHead) with direction arrowDir
	arrowDir *= 10.0/sqrt(double(arrowDir.x()*arrowDir.x() +
			             arrowDir.y()*arrowDir.y()));
	QPointArray a(3);
	a.setPoint(0, pa.point(indexHead) + arrowDir);
	a.setPoint(1, pa.point(indexHead) + QPoint(arrowDir.y()/2,
						   -arrowDir.x()/2));
	a.setPoint(2, pa.point(indexHead) + QPoint(-arrowDir.y()/2,
						   arrowDir.x()/2));

	if (0) qDebug("  Arrow: ( %d/%d, %d/%d, %d/%d)",
		      a.point(0).x(), a.point(0).y(),
		      a.point(1).x(), a.point(1).y(),
		      a.point(2).x(), a.point(2).y());

	CanvasEdgeArrow* aItem = new CanvasEdgeArrow(sItem,_canvas);
	aItem->setPoints(a);
	aItem->setBrush(arrowColor);
	aItem->setZ(1.5);
	aItem->show();

	sItem->setArrow(aItem);
    }

    if (lineStream.atEnd()) continue;

    // parse quoted label
    QChar c;
    lineStream >> c;
    while (c.isSpace()) lineStream >> c;
    if (c != '\"') {
      lineStream >> label;
      label = c + label;
    }
    else {
      lineStream >> c;
      while(!c.isNull() && (c != '\"')) {
        //if (c == '\\') lineStream >> c;

        label += c;
        lineStream >> c;
      }
    }
    lineStream >> edgeX >> edgeY;
    x = edgeX.toDouble();
    y = edgeY.toDouble();

    int xx = (int)(scaleX * x + _xMargin);
    int yy = (int)(scaleY * (dotHeight - y) + _yMargin);

    if (0) qDebug("   Label '%s': ( %f / %f ) => ( %d / %d)",
                  label.ascii(), x, y, xx, yy);

    // Fixed Dimensions for Label: 100 x 40
    int w = 100;
    int h = _detailLevel * 20;
    lItem = new CanvasEdgeLabel(this, sItem, xx-w/2, yy-h/2, w, h, _canvas);
    // edge labels above nodes
    lItem->setZ(1.5);
    sItem->setLabel(lItem);
    if (h>0) lItem->show();

  }
  delete dotStream;

  // for keyboard navigation
  // TODO: Edge sorting. Better keep left-to-right edge order from dot now
  // _exporter.sortEdges();

  if (!_canvas) {
    _canvas = new QCanvas(size().width(),size().height());
    QString s = i18n("Error running the graph layouting tool.\n");
    s += i18n("Please check that 'dot' is installed (package GraphViz).");
    QCanvasText* t = new QCanvasText(s, _canvas);
    t->move(5, 5);
    t->show();
    center(0,0);
  }
  else if (!activeNode && !activeEdge) {
    QString s = i18n("There is no call graph available for function\n"
		     "\t'%1'\n"
		     "because it has no cost of the selected event type.");
    QCanvasText* t = new QCanvasText(s.arg(_activeItem->name()), _canvas);
    //    t->setTextFlags(Qt::AlignHCenter | Qt::AlignVCenter);
    t->move(5,5);
    t->show();
    center(0,0);
  }

  _completeView->setCanvas(_canvas);
  setCanvas(_canvas);

  // if we don't have a selection, or the old selection is not
  // in visible graph, make active function selected for this view
  if ((!_selectedNode || !_selectedNode->canvasNode()) &&
      (!_selectedEdge || !_selectedEdge->canvasEdge())) {
    if (activeNode) {
      _selectedNode = activeNode;
      _selectedNode->canvasNode()->setSelected(true);
    }
    else if (activeEdge) {
      _selectedEdge = activeEdge;
      _selectedEdge->canvasEdge()->setSelected(true);
    }
  }

  CanvasNode* sNode = 0;
  if (_selectedNode)
      sNode = _selectedNode->canvasNode();
  else if (_selectedEdge) {
      if (_selectedEdge->fromNode())
	  sNode = _selectedEdge->fromNode()->canvasNode();
      if (!sNode && _selectedEdge->toNode())
	  sNode = _selectedEdge->toNode()->canvasNode();
  }
  if (sNode) {
      int x = int(sNode->x() + sNode->width()/2);
      int y = int(sNode->y() + sNode->height()/2);

      if (_prevSelectedNode) {
	  if (rect().contains(_prevSelectedPos))
	      setContentsPos(x-_prevSelectedPos.x(),
                             y-_prevSelectedPos.y());
	  else
	      ensureVisible(x,y,
                            sNode->width()/2+50, sNode->height()/2+50);
      }
      else center(x,y);
  }

  if (activeNode) {
    CanvasNode* cn = activeNode->canvasNode();
    CanvasFrame* f = new CanvasFrame(cn, _canvas);
    f->setZ(-1);
    f->show();
  }

  _cvZoom = 0;
  updateSizes();

  _canvas->update();
  viewport()->setUpdatesEnabled(true);

  delete _renderProcess;
  _renderProcess = 0;
}

void CallGraphView::contentsMovingSlot(int x, int y)
{
  QRect z(int(x * _cvZoom), int(y * _cvZoom),
          int(visibleWidth() * _cvZoom)-1, int(visibleHeight() * _cvZoom)-1);
  if (0) qDebug("moving: (%d,%d) => (%d/%d - %dx%d)",
                x, y, z.x(), z.y(), z.width(), z.height());
  _completeView->setZoomRect(z);
}

void CallGraphView::zoomRectMoved(int dx, int dy)
{
  if (leftMargin()>0) dx = 0;
  if (topMargin()>0) dy = 0;
  scrollBy(int(dx/_cvZoom),int(dy/_cvZoom));
}

void CallGraphView::zoomRectMoveFinished()
{
    if (_zoomPosition == Auto) updateSizes();
}

void CallGraphView::contentsMousePressEvent(QMouseEvent* e)
{
    // clicking on the viewport sets focus
    setFocus();

  _isMoving = true;

  QCanvasItemList l = canvas()->collisions(e->pos());
  if (l.count()>0) {
    QCanvasItem* i = l.first();

    if (i->rtti() == CANVAS_NODE) {
      GraphNode* n = ((CanvasNode*)i)->node();
      if (0) qDebug("CallGraphView: Got Node '%s'",
                    n->function()->prettyName().ascii());

      selected(n->function());
    }

    // redirect from label / arrow to edge
    if (i->rtti() == CANVAS_EDGELABEL)
	i = ((CanvasEdgeLabel*)i)->canvasEdge();
    if (i->rtti() == CANVAS_EDGEARROW)
	i = ((CanvasEdgeArrow*)i)->canvasEdge();

    if (i->rtti() == CANVAS_EDGE) {
      GraphEdge* e = ((CanvasEdge*)i)->edge();
      if (0) qDebug("CallGraphView: Got Edge '%s'",
                    e->prettyName().ascii());

      if (e->call()) selected(e->call());
    }
  }
  _lastPos = e->globalPos();
}

void CallGraphView::contentsMouseMoveEvent(QMouseEvent* e)
{
  if (_isMoving) {
    int dx = e->globalPos().x() - _lastPos.x();
    int dy = e->globalPos().y() - _lastPos.y();
    scrollBy(-dx, -dy);
    _lastPos = e->globalPos();
  }
}

void CallGraphView::contentsMouseReleaseEvent(QMouseEvent*)
{
  _isMoving = false;
  if (_zoomPosition == Auto) updateSizes();
}

void CallGraphView::contentsMouseDoubleClickEvent(QMouseEvent* e)
{
  QCanvasItemList l = canvas()->collisions(e->pos());
  if (l.count() == 0) return;
  QCanvasItem* i = l.first();

  if (i->rtti() == CANVAS_NODE) {
    GraphNode* n = ((CanvasNode*)i)->node();
    if (0) qDebug("CallGraphView: Double Clicked on Node '%s'",
		  n->function()->prettyName().ascii());

    activated(n->function());
  }

  // redirect from label / arrow to edge
  if (i->rtti() == CANVAS_EDGELABEL)
      i = ((CanvasEdgeLabel*)i)->canvasEdge();
  if (i->rtti() == CANVAS_EDGEARROW)
      i = ((CanvasEdgeArrow*)i)->canvasEdge();

  if (i->rtti() == CANVAS_EDGE) {
    GraphEdge* e = ((CanvasEdge*)i)->edge();
    if (e->call()) {
      if (0) qDebug("CallGraphView: Double Clicked On Edge '%s'",
		    e->call()->prettyName().ascii());

      activated(e->call());
    }
  }
}

void CallGraphView::contentsContextMenuEvent(QContextMenuEvent* e)
{
  QCanvasItemList l = canvas()->collisions(e->pos());
  QCanvasItem* i = (l.count() == 0) ? 0 : l.first();

  QPopupMenu popup;
  TraceFunction *f = 0, *cycle = 0;
  TraceCall* c = 0;

  if (i) {
    if (i->rtti() == CANVAS_NODE) {
      GraphNode* n = ((CanvasNode*)i)->node();
      if (0) qDebug("CallGraphView: Menu on Node '%s'",
		    n->function()->prettyName().ascii());
      f = n->function();
      cycle = f->cycle();

      QString name = f->prettyName();
      popup.insertItem(i18n("Go to '%1'")
		     .arg(Configuration::shortenSymbol(name)), 93);
      if (cycle && (cycle != f)) {
	name = Configuration::shortenSymbol(cycle->prettyName());
	popup.insertItem(i18n("Go to '%1'").arg(name), 94);
      }
      popup.insertSeparator();
    }

    // redirect from label / arrow to edge
    if (i->rtti() == CANVAS_EDGELABEL)
	i = ((CanvasEdgeLabel*)i)->canvasEdge();
    if (i->rtti() == CANVAS_EDGEARROW)
	i = ((CanvasEdgeArrow*)i)->canvasEdge();

    if (i->rtti() == CANVAS_EDGE) {
      GraphEdge* e = ((CanvasEdge*)i)->edge();
      if (0) qDebug("CallGraphView: Menu on Edge '%s'",
		    e->prettyName().ascii());
      c = e->call();
      if (c) {
	  QString name = c->prettyName();
	  popup.insertItem(i18n("Go to '%1'")
			   .arg(Configuration::shortenSymbol(name)), 95);

	  popup.insertSeparator();
      }
    }
  }

  if (_renderProcess) {
    popup.insertItem(i18n("Stop Layouting"), 999);
    popup.insertSeparator();
  }

  addGoMenu(&popup);
  popup.insertSeparator();

  QPopupMenu epopup;
  epopup.insertItem(i18n("As PostScript"), 201);
  epopup.insertItem(i18n("As Image ..."), 202);

  popup.insertItem(i18n("Export Graph"), &epopup, 200);
  popup.insertSeparator();

  QPopupMenu gpopup1;
  gpopup1.setCheckable(true);
  gpopup1.insertItem(i18n("Unlimited"), 100);
  gpopup1.setItemEnabled(100, (_funcLimit>0.005));
  gpopup1.insertSeparator();
  gpopup1.insertItem(i18n("None"), 101);
  gpopup1.insertItem(i18n("max. 2"), 102);
  gpopup1.insertItem(i18n("max. 5"), 103);
  gpopup1.insertItem(i18n("max. 10"), 104);
  gpopup1.insertItem(i18n("max. 15"), 105);
  if (_maxCallerDepth<-1) _maxCallerDepth=-1;
  switch(_maxCallerDepth) {
  case -1: gpopup1.setItemChecked(100,true); break;
  case  0: gpopup1.setItemChecked(101,true); break;
  case  2: gpopup1.setItemChecked(102,true); break;
  case  5: gpopup1.setItemChecked(103,true); break;
  case 10: gpopup1.setItemChecked(104,true); break;
  case 15: gpopup1.setItemChecked(105,true); break;
  default:
    gpopup1.insertItem(i18n("< %1").arg(_maxCallerDepth), 106);
    gpopup1.setItemChecked(106,true); break;
  }

  QPopupMenu gpopup2;
  gpopup2.setCheckable(true);
  gpopup2.insertItem(i18n("Unlimited"), 110);
  gpopup2.setItemEnabled(110, (_funcLimit>0.005));
  gpopup2.insertSeparator();
  gpopup2.insertItem(i18n("None"), 111);
  gpopup2.insertItem(i18n("max. 2"), 112);
  gpopup2.insertItem(i18n("max. 5"), 113);
  gpopup2.insertItem(i18n("max. 10"), 114);
  gpopup2.insertItem(i18n("max. 15"), 115);
  if (_maxCallingDepth<-1) _maxCallingDepth=-1;
  switch(_maxCallingDepth) {
  case -1: gpopup2.setItemChecked(110,true); break;
  case  0: gpopup2.setItemChecked(111,true); break;
  case  2: gpopup2.setItemChecked(112,true); break;
  case  5: gpopup2.setItemChecked(113,true); break;
  case 10: gpopup2.setItemChecked(114,true); break;
  case 15: gpopup2.setItemChecked(115,true); break;
  default:
    gpopup2.insertItem(i18n("< %1").arg(_maxCallingDepth), 116);
    gpopup2.setItemChecked(116,true); break;
  }

  QPopupMenu gpopup3;
  gpopup3.setCheckable(true);
  gpopup3.insertItem(i18n("No Minimum"), 120);
  gpopup3.setItemEnabled(120,
                         (_maxCallerDepth>=0) && (_maxCallingDepth>=0));
  gpopup3.insertSeparator();
  gpopup3.insertItem(i18n("50 %"), 121);
  gpopup3.insertItem(i18n("20 %"), 122);
  gpopup3.insertItem(i18n("10 %"), 123);
  gpopup3.insertItem(i18n("5 %"), 124);
  gpopup3.insertItem(i18n("3 %"), 125);
  gpopup3.insertItem(i18n("2 %"), 126);
  gpopup3.insertItem(i18n("1.5 %"), 127);
  gpopup3.insertItem(i18n("1 %"), 128);
  if (_funcLimit<0) _funcLimit = DEFAULT_FUNCLIMIT;
  if (_funcLimit>.5) _funcLimit = .5;
  if (_funcLimit == 0.0) gpopup3.setItemChecked(120,true);
  else if (_funcLimit >= 0.5) gpopup3.setItemChecked(121,true);
  else if (_funcLimit >= 0.2) gpopup3.setItemChecked(122,true);
  else if (_funcLimit >= 0.1) gpopup3.setItemChecked(123,true);
  else if (_funcLimit >= 0.05) gpopup3.setItemChecked(124,true);
  else if (_funcLimit >= 0.03) gpopup3.setItemChecked(125,true);
  else if (_funcLimit >= 0.02) gpopup3.setItemChecked(126,true);
  else if (_funcLimit >= 0.015) gpopup3.setItemChecked(127,true);
  else gpopup3.setItemChecked(128,true);
  double oldFuncLimit = _funcLimit;

  QPopupMenu gpopup4;
  gpopup4.setCheckable(true);
  gpopup4.insertItem(i18n("Same as Node"), 160);
  gpopup4.insertItem(i18n("50 % of Node"), 161);
  gpopup4.insertItem(i18n("20 % of Node"), 162);
  gpopup4.insertItem(i18n("10 % of Node"), 163);
  if (_callLimit<0) _callLimit = DEFAULT_CALLLIMIT;
  if (_callLimit >= _funcLimit) _callLimit = _funcLimit;
  if (_callLimit == _funcLimit) gpopup4.setItemChecked(160,true);
  else if (_callLimit >= 0.5 * _funcLimit) gpopup4.setItemChecked(161,true);
  else if (_callLimit >= 0.2 * _funcLimit) gpopup4.setItemChecked(162,true);
  else gpopup4.setItemChecked(163,true);

  QPopupMenu gpopup;
  gpopup.setCheckable(true);
  gpopup.insertItem(i18n("Caller Depth"), &gpopup1, 80);
  gpopup.insertItem(i18n("Callee Depth"), &gpopup2, 81);
  gpopup.insertItem(i18n("Min. Node Cost"), &gpopup3, 82);
  gpopup.insertItem(i18n("Min. Call Cost"), &gpopup4, 83);
  gpopup.insertSeparator();
  gpopup.insertItem(i18n("Arrows for Skipped Calls"), 130);
  gpopup.setItemChecked(130,_showSkipped);
  gpopup.insertItem(i18n("Inner-cycle Calls"), 131);
  gpopup.setItemChecked(131,_expandCycles);
  gpopup.insertItem(i18n("Cluster Groups"), 132);
  gpopup.setItemChecked(132,_clusterGroups);

  QPopupMenu vpopup;
  vpopup.setCheckable(true);
  vpopup.insertItem(i18n("Compact"), 140);
  vpopup.insertItem(i18n("Normal"), 141);
  vpopup.insertItem(i18n("Tall"), 142);
  vpopup.setItemChecked(140,_detailLevel == 0);
  vpopup.setItemChecked(141,_detailLevel == 1);
  vpopup.setItemChecked(142,_detailLevel == 2);
  vpopup.insertSeparator();
  vpopup.insertItem(i18n("Top to Down"), 150);
  vpopup.insertItem(i18n("Left to Right"), 151);
  vpopup.insertItem(i18n("Circular"), 152);
  vpopup.setItemChecked(150,_layout == TopDown);
  vpopup.setItemChecked(151,_layout == LeftRight);
  vpopup.setItemChecked(152,_layout == Circular);

  QPopupMenu opopup;
  opopup.insertItem(i18n("TopLeft"), 170);
  opopup.insertItem(i18n("TopRight"), 171);
  opopup.insertItem(i18n("BottomLeft"), 172);
  opopup.insertItem(i18n("BottomRight"), 173);
  opopup.insertItem(i18n("Automatic"), 174);
  opopup.setItemChecked(170,_zoomPosition == TopLeft);
  opopup.setItemChecked(171,_zoomPosition == TopRight);
  opopup.setItemChecked(172,_zoomPosition == BottomLeft);
  opopup.setItemChecked(173,_zoomPosition == BottomRight);
  opopup.setItemChecked(174,_zoomPosition == Auto);

  popup.insertItem(i18n("Graph"), &gpopup, 70);
  popup.insertItem(i18n("Visualization"), &vpopup, 71);
  popup.insertItem(i18n("Birds-eye View"), &opopup, 72);

  int r = popup.exec(e->globalPos());

  switch(r) {
  case 93: activated(f); break;
  case 94: activated(cycle); break;
  case 95: activated(c); break;

  case 999: stopRendering(); break;

  case 201:
      {
	  TraceFunction* f = activeFunction();
	  if (!f) break;

	  GraphExporter ge(data(), f, costType(), groupType(),
			   QString("callgraph.dot"));
	  ge.setGraphOptions(this);
	  ge.writeDot();

	  system("(dot callgraph.dot -Tps > callgraph.ps; kghostview callgraph.ps)&");
      }
      break;

  case 202:    
      // write current content of canvas as image to file
      {
	if (!_canvas) return;

	QString fn = KFileDialog::getSaveFileName(":","*.png");

	if (!fn.isEmpty()) {
	  QPixmap pix(_canvas->size());
	  QPainter p(&pix);
	  _canvas->drawArea( _canvas->rect(), &p );
	  pix.save(fn,"PNG");
	}
      }
      break;

  case 100: _maxCallerDepth = -1; break;
  case 101: _maxCallerDepth = 0; break;
  case 102: _maxCallerDepth = 2; break;
  case 103: _maxCallerDepth = 5; break;
  case 104: _maxCallerDepth = 10; break;
  case 105: _maxCallerDepth = 15; break;

  case 110: _maxCallingDepth = -1; break;
  case 111: _maxCallingDepth = 0; break;
  case 112: _maxCallingDepth = 2; break;
  case 113: _maxCallingDepth = 5; break;
  case 114: _maxCallingDepth = 10; break;
  case 115: _maxCallingDepth = 15; break;

  case 120: _funcLimit = 0; break;
  case 121: _funcLimit = 0.5; break;
  case 122: _funcLimit = 0.2; break;
  case 123: _funcLimit = 0.1; break;
  case 124: _funcLimit = 0.05; break;
  case 125: _funcLimit = 0.03; break;
  case 126: _funcLimit = 0.02; break;
  case 127: _funcLimit = 0.015; break;
  case 128: _funcLimit = 0.01; break;

  case 130: _showSkipped = !_showSkipped; break;
  case 131: _expandCycles = !_expandCycles; break;
  case 132: _clusterGroups = !_clusterGroups; break;

  case 140: _detailLevel = 0; break;
  case 141: _detailLevel = 1; break;
  case 142: _detailLevel = 2; break;

  case 150: _layout = TopDown; break;
  case 151: _layout = LeftRight; break;
  case 152: _layout = Circular; break;

  case 160: _callLimit = _funcLimit; break;
  case 161: _callLimit = .5 * _funcLimit; break;
  case 162: _callLimit = .2 * _funcLimit; break;
  case 163: _callLimit = .1 * _funcLimit; break;

  case 170: _zoomPosition = TopLeft; break;
  case 171: _zoomPosition = TopRight; break;
  case 172: _zoomPosition = BottomLeft; break;
  case 173: _zoomPosition = BottomRight; break;
  case 174: _zoomPosition = Auto; break;

  default: break;
  }
  if (r>=120 && r<130) _callLimit *= _funcLimit / oldFuncLimit;

  if (r>99 && r<170) refresh();
  if (r>169 && r<180) updateSizes();
}

CallGraphView::ZoomPosition CallGraphView::zoomPos(QString s)
{
    if (s == QString("TopLeft")) return TopLeft;
    if (s == QString("TopRight")) return TopRight;
    if (s == QString("BottomLeft")) return BottomLeft;
    if (s == QString("BottomRight")) return BottomRight;
    if (s == QString("Automatic")) return Auto;

    return DEFAULT_ZOOMPOS;
}

QString CallGraphView::zoomPosString(ZoomPosition p)
{
    if (p == TopRight) return QString("TopRight");
    if (p == BottomLeft) return QString("BottomLeft");
    if (p == BottomRight) return QString("BottomRight");
    if (p == Auto) return QString("Automatic");

    return QString("TopLeft");
}

void CallGraphView::readViewConfig(KConfig* c, 
				   QString prefix, QString postfix, bool)
{
    KConfigGroup* g = configGroup(c, prefix, postfix);

    if (0) qDebug("CallGraphView::readViewConfig");

    _maxCallerDepth  = g->readNumEntry("MaxCaller", DEFAULT_MAXCALLER);
    _maxCallingDepth = g->readNumEntry("MaxCalling", DEFAULT_MAXCALLING);
    _funcLimit       = g->readDoubleNumEntry("FuncLimit", DEFAULT_FUNCLIMIT);
    _callLimit       = g->readDoubleNumEntry("CallLimit", DEFAULT_CALLLIMIT);
    _showSkipped     = g->readBoolEntry("ShowSkipped", DEFAULT_SHOWSKIPPED);
    _expandCycles    = g->readBoolEntry("ExpandCycles", DEFAULT_EXPANDCYCLES);
    _clusterGroups   = g->readBoolEntry("ClusterGroups",
					DEFAULT_CLUSTERGROUPS);
    _detailLevel     = g->readNumEntry("DetailLevel", DEFAULT_DETAILLEVEL);
    _layout          = GraphOptions::layout(g->readEntry("Layout",
					   layoutString(DEFAULT_LAYOUT)));
    _zoomPosition    = zoomPos(g->readEntry("ZoomPosition",
					    zoomPosString(DEFAULT_ZOOMPOS)));

    delete g;
}

void CallGraphView::saveViewConfig(KConfig* c,
				   QString prefix, QString postfix, bool)
{
    KConfigGroup g(c, (prefix+postfix).ascii());

    writeConfigEntry(&g, "MaxCaller", _maxCallerDepth, DEFAULT_MAXCALLER);
    writeConfigEntry(&g, "MaxCalling", _maxCallingDepth, DEFAULT_MAXCALLING);
    writeConfigEntry(&g, "FuncLimit", _funcLimit, DEFAULT_FUNCLIMIT);
    writeConfigEntry(&g, "CallLimit", _callLimit, DEFAULT_CALLLIMIT);
    writeConfigEntry(&g, "ShowSkipped", _showSkipped, DEFAULT_SHOWSKIPPED);
    writeConfigEntry(&g, "ExpandCycles", _expandCycles, DEFAULT_EXPANDCYCLES);
    writeConfigEntry(&g, "ClusterGroups", _clusterGroups,
		     DEFAULT_CLUSTERGROUPS);
    writeConfigEntry(&g, "DetailLevel", _detailLevel, DEFAULT_DETAILLEVEL);
    writeConfigEntry(&g, "Layout",
		     layoutString(_layout), layoutString(DEFAULT_LAYOUT).utf8().data());
    writeConfigEntry(&g, "ZoomPosition",
		     zoomPosString(_zoomPosition),
		     zoomPosString(DEFAULT_ZOOMPOS).utf8().data());
}

#include "callgraphview.moc"

