/* This file is part of KCachegrind.
   Copyright (C) 2007 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Callgraph View
 */

#include "callgraphview.h"

#include <stdlib.h>
#include <math.h>

#include <QFile>
#include <QTextStream>
#include <QMatrix>
#include <QPair>
#include <QPainter>
#include <QStyle>
#include <QScrollBar>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QStyleOptionGraphicsItem>
#include <QContextMenuEvent>
#include <QList>
#include <QPixmap>
#include <QDesktopWidget>
#include <Qt3Support/Q3PointArray>
#include <Qt3Support/Q3PopupMenu>
#include <Qt3Support/Q3Process>

#include <kdebug.h>
#include <klocale.h>
#include <kconfig.h>
#include <ktemporaryfile.h>
#include <kiconloader.h>
#include <kfiledialog.h>
#include <kconfiggroup.h>

#include "configuration.h"
#include "listutils.h"


#define DEBUG_GRAPH 0

// CallGraphView defaults

#define DEFAULT_FUNCLIMIT   .05
#define DEFAULT_CALLLIMIT    1.
#define DEFAULT_MAXCALLER    2
#define DEFAULT_MAXCALLEE   -1
#define DEFAULT_SHOWSKIPPED  false
#define DEFAULT_EXPANDCYCLES false
#define DEFAULT_CLUSTERGROUPS false
#define DEFAULT_DETAILLEVEL  1
#define DEFAULT_LAYOUT       GraphOptions::TopDown
#define DEFAULT_ZOOMPOS      Auto


// LessThen functors as helpers for sorting of graph edges
// for keyboard navigation. Sorting is done according to
// the angle at which a edge spline goes out or in of a function.

// Sort angles of outgoing edges (edge seen as attached to the caller)
class CallerGraphEdgeLessThan
{
public:
	bool operator()(const GraphEdge* ge1, const GraphEdge* ge2) const
	{
		const CanvasEdge* ce1 = ge1->canvasEdge();
		const CanvasEdge* ce2 = ge2->canvasEdge();

		// sort invisible edges (ie. without matching CanvasEdge) in front
		if (!ce1) return true;
		if (!ce2) return false;

		QPolygon p1 = ce1->controlPoints();
		QPolygon p2 = ce2->controlPoints();
		QPoint d1 = p1.point(1) - p1.point(0);
		QPoint d2 = p2.point(1) - p2.point(0);
		double angle1 = atan2(double(d1.y()), double(d1.x()));
		double angle2 = atan2(double(d2.y()), double(d2.x()));

		return (angle1 < angle2);
	}
};

// Sort angles of ingoing edges (edge seen as attached to the callee)
class CalleeGraphEdgeLessThan
{
public:
	bool operator()(const GraphEdge* ge1, const GraphEdge* ge2) const
	{
		const CanvasEdge* ce1 = ge1->canvasEdge();
		const CanvasEdge* ce2 = ge2->canvasEdge();

		// sort invisible edges (ie. without matching CanvasEdge) in front
		if (!ce1) return true;
		if (!ce2) return false;

		QPolygon p1 = ce1->controlPoints();
		QPolygon p2 = ce2->controlPoints();
		QPoint d1 = p1.point(p1.count()-2) - p1.point(p1.count()-1);
		QPoint d2 = p2.point(p2.count()-2) - p2.point(p2.count()-1);
		double angle1 = atan2(double(d1.y()), double(d1.x()));
		double angle2 = atan2(double(d2.y()), double(d2.x()));

		// for ingoing edges sort according to descending angles
		return (angle2 < angle1);
	}
};



//
// GraphNode
//

GraphNode::GraphNode()
{
	_f=0;
	self = incl = 0;
	_cn = 0;

	_visible = false;
	_lastCallerIndex = _lastCalleeIndex = -1;

	_lastFromCaller = true;
}

void GraphNode::clearEdges()
{
	callees.clear();
	callers.clear();
}

CallerGraphEdgeLessThan callerGraphEdgeLessThan;
CalleeGraphEdgeLessThan calleeGraphEdgeLessThan;

void GraphNode::sortEdges()
{
	qSort(callers.begin(), callers.end(), callerGraphEdgeLessThan);
	qSort(callees.begin(), callees.end(), calleeGraphEdgeLessThan);
}

void GraphNode::addCallee(GraphEdge* e)
{
	if (e)
		callees.append(e);
}

void GraphNode::addCaller(GraphEdge* e)
{
	if (e)
		callers.append(e);
}

void GraphNode::addUniqueCallee(GraphEdge* e)
{
	if (e && (callees.count(e) == 0))
		callees.append(e);
}

void GraphNode::addUniqueCaller(GraphEdge* e)
{
	if (e && (callers.count(e) == 0))
		callers.append(e);
}

void GraphNode::removeEdge(GraphEdge* e)
{
	callers.removeAll(e);
	callees.removeAll(e);
}

double GraphNode::calleeCostSum()
{
	double sum = 0.0;

	foreach(GraphEdge* e, callees)
		sum += e->cost;

	return sum;
}

double GraphNode::calleeCountSum()
{
	double sum = 0.0;

	foreach(GraphEdge* e, callees)
		sum += e->count;

	return sum;
}

double GraphNode::callerCostSum()
{
	double sum = 0.0;

	foreach(GraphEdge* e, callers)
		sum += e->cost;

	return sum;
}

double GraphNode::callerCountSum()
{
	double sum = 0.0;

	foreach(GraphEdge* e, callers)
		sum += e->count;

	return sum;
}


TraceCall* GraphNode::visibleCaller()
{
	if (0)
		qDebug("GraphNode::visibleCaller %s: last %d, count %d",
		       _f->prettyName().ascii(), _lastCallerIndex, callers.count());

	// can not use at(): index can be -1 (out of bounds), result is 0 then
	GraphEdge* e = callers.value(_lastCallerIndex);
	if (e && !e->isVisible())
		e = 0;
	if (!e) {
		double maxCost = 0.0;
		GraphEdge* maxEdge = 0;
		for(int i = 0; i<callers.size(); i++) {
			e = callers[i];
			if (e->isVisible() && (e->cost > maxCost)) {
				maxCost = e->cost;
				maxEdge = e;
				_lastCallerIndex = i;
			}
		}
		e = maxEdge;
	}
	return e ? e->call() : 0;
}

TraceCall* GraphNode::visibleCallee()
{
	if (0)
		qDebug("GraphNode::visibleCallee %s: last %d, count %d",
		       _f->prettyName().ascii(), _lastCalleeIndex, callees.count());

	GraphEdge* e = callees.value(_lastCalleeIndex);
	if (e && !e->isVisible())
		e = 0;

	if (!e) {
		double maxCost = 0.0;
		GraphEdge* maxEdge = 0;
		for(int i = 0; i<callees.size(); i++) {
			e = callees[i];
			if (e->isVisible() && (e->cost > maxCost)) {
				maxCost = e->cost;
				maxEdge = e;
				_lastCalleeIndex = i;
			}
		}
		e = maxEdge;
	}
	return e ? e->call() : 0;
}

void GraphNode::setCallee(GraphEdge* e)
{
	_lastCalleeIndex = callees.indexOf(e);
	_lastFromCaller = false;
}

void GraphNode::setCaller(GraphEdge* e)
{
	_lastCallerIndex = callers.indexOf(e);
	_lastFromCaller = true;
}

TraceFunction* GraphNode::nextVisible()
{
	TraceCall* c;

	if (_lastFromCaller) {
		c = nextVisibleCaller();
		if (c)
			return c->called(true);
		c = nextVisibleCallee();
		if (c)
			return c->caller(true);
	} else {
		c = nextVisibleCallee();
		if (c)
			return c->caller(true);
		c = nextVisibleCaller();
		if (c)
			return c->called(true);
	}
	return 0;
}

TraceFunction* GraphNode::priorVisible()
{
	TraceCall* c;

	if (_lastFromCaller) {
		c = priorVisibleCaller();
		if (c)
			return c->called(true);
		c = priorVisibleCallee();
		if (c)
			return c->caller(true);
	} else {
		c = priorVisibleCallee();
		if (c)
			return c->caller(true);
		c = priorVisibleCaller();
		if (c)
			return c->called(true);
	}
	return 0;
}

TraceCall* GraphNode::nextVisibleCaller(GraphEdge* e)
{
	int idx = e ? callers.indexOf(e) : _lastCallerIndex;
	idx++;
	while(idx < callers.size()) {
		if (callers[idx]->isVisible()) {
			_lastCallerIndex = idx;
			return callers[idx]->call();
		}
		idx++;
	}
	return 0;
}

TraceCall* GraphNode::nextVisibleCallee(GraphEdge* e)
{
	int idx = e ? callees.indexOf(e) : _lastCalleeIndex;
	idx++;
	while(idx < callees.size()) {
		if (callees[idx]->isVisible()) {
			_lastCalleeIndex = idx;
			return callees[idx]->call();
		}
		idx++;
	}
	return 0;
}

TraceCall* GraphNode::priorVisibleCaller(GraphEdge* e)
{
	int idx = e ? callers.indexOf(e) : _lastCallerIndex;

	idx = (idx<0) ? callers.size()-1 : idx-1;
	while(idx >= 0) {
		if (callers[idx]->isVisible()) {
			_lastCallerIndex = idx;
			return callers[idx]->call();
		}
		idx--;
	}
	return 0;
}

TraceCall* GraphNode::priorVisibleCallee(GraphEdge* e)
{
	int idx = e ? callees.indexOf(e) : _lastCalleeIndex;

	idx = (idx<0) ? callees.size()-1 : idx-1;
	while(idx >= 0) {
		if (callees[idx]->isVisible()) {
			_lastCalleeIndex = idx;
			return callees[idx]->call();
		}
		idx--;
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
	if (_c)
		return _c->prettyName();

	if (_from)
		return i18n("Call(s) from %1", _from->prettyName());

	if (_to)
		return i18n("Call(s) to %1", _to->prettyName());

	return i18n("(unknown call)");
}

TraceFunction* GraphEdge::visibleCaller()
{
	if (_from) {
		_lastFromCaller = true;
		if (_fromNode)
			_fromNode->setCallee(this);
		return _from;
	}
	return 0;
}

TraceFunction* GraphEdge::visibleCallee()
{
	if (_to) {
		_lastFromCaller = false;
		if (_toNode)
			_toNode->setCaller(this);
		return _to;
	}
	return 0;
}

TraceCall* GraphEdge::nextVisible()
{
	TraceCall* res = 0;

	if (_lastFromCaller && _fromNode) {
		res = _fromNode->nextVisibleCallee(this);
		if (!res && _toNode)
			res = _toNode->nextVisibleCaller(this);
	} else if (_toNode) {
		res = _toNode->nextVisibleCaller(this);
		if (!res && _fromNode)
			res = _fromNode->nextVisibleCallee(this);
	}
	return res;
}

TraceCall* GraphEdge::priorVisible()
{
	TraceCall* res = 0;

	if (_lastFromCaller && _fromNode) {
		res = _fromNode->priorVisibleCallee(this);
		if (!res && _toNode)
			res = _toNode->priorVisibleCaller(this);
	} else if (_toNode) {
		res = _toNode->priorVisibleCaller(this);
		if (!res && _fromNode)
			res = _fromNode->priorVisibleCallee(this);
	}
	return res;
}



//
// GraphOptions
//

QString GraphOptions::layoutString(Layout l)
{
	if (l == Circular)
		return QString("Circular");
	if (l == LeftRight)
		return QString("LeftRight");
	return QString("TopDown");
}

GraphOptions::Layout GraphOptions::layout(QString s)
{
	if (s == QString("Circular"))
		return Circular;
	if (s == QString("LeftRight"))
		return LeftRight;
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
  _maxCalleeDepth    = DEFAULT_MAXCALLEE;
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
	reset(0, 0, 0, TraceItem::NoCostType, QString());
}

GraphExporter::GraphExporter(TraceData* d, TraceFunction* f,
                             TraceEventType* ct, TraceItem::CostType gt,
                             QString filename)
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
		_tmpFile->setAutoRemove(true);
#endif
		delete _tmpFile;
	}
}


void GraphExporter::reset(TraceData*, TraceItem* i, TraceEventType* ct,
                          TraceItem::CostType gt, QString filename)
{
	_graphCreated = false;
	_nodeMap.clear();
	_edgeMap.clear();

	if (_item && _tmpFile) {
		_tmpFile->setAutoRemove(true);
		delete _tmpFile;
	}

	if (i) {
		switch (i->type()) {
		case TraceItem::Function:
		case TraceItem::FunctionCycle:
		case TraceItem::Call:
			break;
		default:
			i = 0;
		}
	}

	_item = i;
	_eventType = ct;
	_groupType = gt;
	if (!i)
		return;

	if (filename.isEmpty()) {
		_tmpFile = new KTemporaryFile();
		_tmpFile->setSuffix(".dot");
		_tmpFile->setAutoRemove(false);
		_tmpFile->open();
		_dotName = _tmpFile->fileName();
		_useBox = true;
	} else {
		_tmpFile = 0;
		_dotName = filename;
		_useBox = false;
	}
}



void GraphExporter::setGraphOptions(GraphOptions* go)
{
	if (go == 0)
		go = this;
	_go = go;
}

void GraphExporter::createGraph()
{
	if (!_item)
		return;
	if (_graphCreated)
		return;
	_graphCreated = true;

	if ((_item->type() == TraceItem::Function) ||(_item->type()
	        == TraceItem::FunctionCycle)) {
		TraceFunction* f = (TraceFunction*) _item;

		double incl = f->inclusive()->subCost(_eventType);
		_realFuncLimit = incl * _go->funcLimit();
		_realCallLimit = _realFuncLimit * _go->callLimit();

		buildGraph(f, 0, true, 1.0); // down to callees

		// set costs of function back to 0, as it will be added again
		GraphNode& n = _nodeMap[f];
		n.self = n.incl = 0.0;

		buildGraph(f, 0, false, 1.0); // up to callers
	} else {
		TraceCall* c = (TraceCall*) _item;

		double incl = c->subCost(_eventType);
		_realFuncLimit = incl * _go->funcLimit();
		_realCallLimit = _realFuncLimit * _go->callLimit();

		// create edge
		TraceFunction *caller, *called;
		caller = c->caller(false);
		called = c->called(false);
		QPair<TraceFunction*,TraceFunction*> p(caller, called);
		GraphEdge& e = _edgeMap[p];
		e.setCall(c);
		e.setCaller(p.first);
		e.setCallee(p.second);
		e.cost = c->subCost(_eventType);
		e.count = c->callCount();

		SubCost s = called->inclusive()->subCost(_eventType);
		buildGraph(called, 0, true, e.cost / s); // down to callees
		s = caller->inclusive()->subCost(_eventType);
		buildGraph(caller, 0, false, e.cost / s); // up to callers
	}
}


void GraphExporter::writeDot()
{
	if (!_item)
		return;

	QFile* file = 0;
	QTextStream* stream = 0;

	if (_tmpFile)
		stream = new QTextStream(_tmpFile);
	else {
		file = new QFile(_dotName);
		if ( !file->open(QIODevice::WriteOnly ) ) {
			kError() << "Can not write dot file '"<< _dotName << "'"<< endl;
                        delete file;
			return;
		}
		stream = new QTextStream(file);
	}

	if (!_graphCreated)
		createGraph();

	/* Generate dot format...
	 * When used for the CallGraphView (in contrast to "Export Callgraph..."),
	 * the labels are only dummy placeholders to reserve space for our own
	 * drawings.
	 */

	*stream << "digraph \"callgraph\" {\n";

	if (_go->layout() == LeftRight) {
		*stream << QString("  rankdir=LR;\n");
	} else if (_go->layout() == Circular) {
		TraceFunction *f = 0;
		switch (_item->type()) {
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
	QMap<TraceCostItem*,QList<GraphNode*> > nLists;

	GraphNodeMap::Iterator nit;
	for (nit = _nodeMap.begin(); nit != _nodeMap.end(); ++nit ) {
		GraphNode& n = *nit;

		if (n.incl <= _realFuncLimit)
			continue;

		// for clustering: get cost item group of function
		TraceCostItem* g;
		TraceFunction* f = n.function();
		switch (_groupType) {
		case TraceItem::Object:
			g = f->object();
			break;
		case TraceItem::Class:
			g = f->cls();
			break;
		case TraceItem::File:
			g = f->file();
			break;
		case TraceItem::FunctionCycle:
			g = f->cycle();
			break;
		default:
			g = 0;
			break;
		}
		nLists[g].append(&n);
	}

	QMap<TraceCostItem*,QList<GraphNode*> >::Iterator lit;
	int cluster = 0;
	for (lit = nLists.begin(); lit != nLists.end(); ++lit, cluster++) {
		QList<GraphNode*>& l = lit.data();
		TraceCostItem* i = lit.key();

		if (_go->clusterGroups() && i) {
			QString iabr = i->prettyName();
			if ((int)iabr.length() > Configuration::maxSymbolLength())
				iabr = iabr.left(Configuration::maxSymbolLength()) + "...";

			*stream << QString("subgraph \"cluster%1\" { label=\"%2\";\n")
			.arg(cluster).arg(iabr);
		}

		foreach(GraphNode* np, l) {
			TraceFunction* f = np->function();

			QString abr = f->prettyName();
			if ((int)abr.length() > Configuration::maxSymbolLength())
				abr = abr.left(Configuration::maxSymbolLength()) + "...";

			*stream << QString("  F%1 [").arg((long)f, 0, 16);
			if (_useBox) {
				// we want a minimal size for cost display
				if ((int)abr.length() < 8) abr = abr + QString(8 - abr.length(),'_');

				// make label 3 lines for CallGraphView
				*stream << QString("shape=box,label=\"** %1 **\\n**\\n%2\"];\n")
				.arg(abr)
				.arg(SubCost(np->incl).pretty());
			} else
				*stream << QString("label=\"%1\\n%2\"];\n")
				.arg(abr)
				.arg(SubCost(np->incl).pretty());
		}

		if (_go->clusterGroups() && i)
			*stream << QString("}\n");
	}

	GraphEdgeMap::Iterator eit;
	for (eit = _edgeMap.begin(); eit != _edgeMap.end(); ++eit ) {
		GraphEdge& e = *eit;

		if (e.cost < _realCallLimit)
			continue;
		if (!_go->expandCycles()) {
			// do not show inner cycle calls
			if (e.call()->inCycle()>0)
				continue;
		}

		GraphNode& from = _nodeMap[e.from()];
		GraphNode& to = _nodeMap[e.to()];

		e.setCallerNode(&from);
		e.setCalleeNode(&to);

		if ((from.incl <= _realFuncLimit) ||(to.incl <= _realFuncLimit))
			continue;

		// remove dumped edges from n.callers/n.callees
		from.removeEdge(&e);

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
		for (nit = _nodeMap.begin(); nit != _nodeMap.end(); ++nit ) {
			GraphNode& n = *nit;
			if (n.incl <= _realFuncLimit)
				continue;

			costSum = n.calleeCostSum();
			countSum = n.calleeCountSum();

			if (costSum > _realCallLimit) {

				QPair<TraceFunction*,TraceFunction*> p(0, n.function());
				e = &(_edgeMap[p]);
				e->setCallee(p.second);
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

			costSum = n.callerCostSum();
			countSum = n.callerCountSum();

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
	for (nit = _nodeMap.begin(); nit != _nodeMap.end(); ++nit ) {
		GraphNode& n = *nit;
		n.clearEdges();
	}

	*stream << "}\n";

	if (_tmpFile) {
		stream->flush();
		_tmpFile->seek(0);
		delete stream;
	} else {
		file->close();
		delete file;
		delete stream;
	}
}

void GraphExporter::sortEdges()
{
	GraphNodeMap::Iterator nit;
	for (nit = _nodeMap.begin(); nit != _nodeMap.end(); ++nit ) {
		GraphNode& n = *nit;
		n.sortEdges();
	}
}

TraceFunction* GraphExporter::toFunc(QString s)
{
	if (s[0] != 'F')
		return 0;
	bool ok;
	TraceFunction* f = (TraceFunction*) s.mid(1).toUInt(&ok, 16);
	if (!ok)
		return 0;

	return f;
}

GraphNode* GraphExporter::node(TraceFunction* f)
{
	if (!f)
		return 0;

	GraphNodeMap::Iterator it = _nodeMap.find(f);
	if (it == _nodeMap.end())
		return 0;

	return &(*it);
}

GraphEdge* GraphExporter::edge(TraceFunction* f1, TraceFunction* f2)
{
	GraphEdgeMap::Iterator it = _edgeMap.find(qMakePair(f1, f2));
	if (it == _edgeMap.end())
		return 0;

	return &(*it);
}

/**
 * We do a DFS and do not stop on already visited nodes/edges,
 * but add up costs. We only stop if limits/max depth is reached.
 *
 * For a node/edge, it can happen that the first time visited the
 * cost will below the limit, so the search is stopped.
 * If on a further visit of the node/edge the limit is reached,
 * we use the whole node/edge cost and continue search.
 */
void GraphExporter::buildGraph(TraceFunction* f, int d, bool toCallees,
                               double factor)
{
#if DEBUG_GRAPH
	kDebug() << "buildGraph(" << f->prettyName() << "," << d << "," << factor
	<< ") [to " << (toCallees ? "Callees":"Callers") << "]" << endl;
#endif

	double oldIncl = 0.0;
	GraphNode& n = _nodeMap[f];
	if (n.function() == 0) {
		n.setFunction(f);
	} else
		oldIncl = n.incl;

	double incl = f->inclusive()->subCost(_eventType) * factor;
	n.incl += incl;
	n.self += f->subCost(_eventType) * factor;
	if (0)
		qDebug("  Added Incl. %f, now %f", incl, n.incl);

	// A negative depth limit means "unlimited"
	int maxDepth = toCallees ? _go->maxCalleeDepth()
	        : _go->maxCallerDepth();
	if ((maxDepth>=0) && (d >= maxDepth)) {
		if (0)
			qDebug("  Cutoff, max depth reached");
		return;
	}

	// if we just reached the limit by summing, do a DFS
	// from here with full incl. cost because of previous cutoffs
	if ((n.incl >= _realFuncLimit) && (oldIncl < _realFuncLimit))
		incl = n.incl;

	if (f->cycle()) {
		// for cycles members, we never stop on first visit, but always on 2nd
		// note: a 2nd visit never should happen, as we do not follow inner-cycle
		//       calls
		if (oldIncl > 0.0) {
			if (0)
				qDebug("  Cutoff, 2nd visit to Cycle Member");
			// and takeback cost addition, as it is added twice
			n.incl = oldIncl;
			n.self -= f->subCost(_eventType) * factor;
			return;
		}
	} else if (incl <= _realFuncLimit) {
		if (0)
			qDebug("  Cutoff, below limit");
		return;
	}

	TraceCall* call;
	TraceFunction* f2;

	// on entering a cycle, only go the FunctionCycle
	TraceCallList l = toCallees ? f->callings(false) : f->callers(false);

	for (call=l.first(); call; call=l.next()) {

		f2 = toCallees ? call->called(false) : call->caller(false);

		double count = call->callCount() * factor;
		double cost = call->subCost(_eventType) * factor;

		// ignore function calls with absolute cost < 3 per call
		// No: This would skip a lot of functions e.g. with L2 cache misses
		// if (count>0.0 && (cost/count < 3)) continue;

		double oldCost = 0.0;
		QPair<TraceFunction*,TraceFunction*> p(toCallees ? f : f2,
		                                       toCallees ? f2 : f);
		GraphEdge& e = _edgeMap[p];
		if (e.call() == 0) {
			e.setCall(call);
			e.setCaller(p.first);
			e.setCallee(p.second);
		} else
			oldCost = e.cost;

		e.cost += cost;
		e.count += count;
		if (0)
			qDebug("  Edge to %s, added cost %f, now %f", f2->prettyName().ascii(), cost, e.cost);

		// if this call goes into a FunctionCycle, we also show the real call
		if (f2->cycle() == f2) {
			TraceFunction* realF;
			realF = toCallees ? call->called(true) : call->caller(true);
			QPair<TraceFunction*,TraceFunction*>
			        realP(toCallees ? f : realF, toCallees ? realF : f);
			GraphEdge& e = _edgeMap[realP];
			if (e.call() == 0) {
				e.setCall(call);
				e.setCaller(realP.first);
				e.setCallee(realP.second);
			}
			e.cost += cost;
			e.count += count;
		}

		// - do not do a DFS on calls in recursion/cycle
		if (call->inCycle()>0)
			continue;
		if (call->isRecursion())
			continue;

		if (toCallees)
			n.addUniqueCallee(&e);
		else
			n.addUniqueCaller(&e);

		// if we just reached the call limit (=func limit by summing, do a DFS
		// from here with full incl. cost because of previous cutoffs
		if ((e.cost >= _realCallLimit) && (oldCost < _realCallLimit))
			cost = e.cost;
		if (cost < _realCallLimit) {
			if (0)
				qDebug("  Edge Cutoff, limit not reached");
			continue;
		}

		SubCost s;
		if (call->inCycle())
			s = f2->cycle()->inclusive()->subCost(_eventType);
		else
			s = f2->inclusive()->subCost(_eventType);
		SubCost v = call->subCost(_eventType);
		buildGraph(f2, d+1, toCallees, factor * v / s);
	}
}

//
// PannerView
//
PanningView::PanningView(QWidget * parent)
	: QGraphicsView(parent)
{
	_movingZoomRect = false;

	// FIXME: Why does this not work?
	viewport()->setFocusPolicy(Qt::NoFocus);
}

void PanningView::setZoomRect(const QRectF& r)
{
	_zoomRect = r;
	viewport()->update();
}

void PanningView::drawForeground(QPainter * p, const QRectF&)
{
	if (!_zoomRect.isValid())
		return;

	QColor red(Qt::red);
	QPen pen(red.dark());
	pen.setWidthF(2.0 / matrix().m11());
	p->setPen(pen);

	QColor c(red.dark());
	c.setAlphaF(0.05);
	p->setBrush(QBrush(c));

	p->drawRect(QRectF(_zoomRect.x(), _zoomRect.y(),
	                   _zoomRect.width()-1, _zoomRect.height()-1));
}

void PanningView::mousePressEvent(QMouseEvent* e)
{
	QPointF sPos = mapToScene(e->pos());

	if (_zoomRect.isValid()) {
		if (!_zoomRect.contains(sPos))
			emit zoomRectMoved(sPos.x() - _zoomRect.center().x(),
			                   sPos.y() - _zoomRect.center().y());

		_movingZoomRect = true;
		_lastPos = sPos;
	}
}

void PanningView::mouseMoveEvent(QMouseEvent* e)
{
	QPointF sPos = mapToScene(e->pos());
	if (_movingZoomRect) {
		emit zoomRectMoved(sPos.x() - _lastPos.x(),
		                   sPos.y() - _lastPos.y());
		_lastPos = sPos;
	}
}

void PanningView::mouseReleaseEvent(QMouseEvent*)
{
    _movingZoomRect = false;
    emit zoomRectMoveFinished();
}





//
// CanvasNode
//

CanvasNode::CanvasNode(CallGraphView* v, GraphNode* n, int x, int y, int w,
                       int h) :
	QGraphicsRectItem(QRect(x, y, w, h)), _node(n), _view(v)
{
	setPosition(0, DrawParams::TopCenter);
	setPosition(1, DrawParams::BottomCenter);

	updateGroup();

	if (!_node || !_view)
		return;

	if (_node->function())
		setText(0, _node->function()->prettyName());

	TraceCost* totalCost;
	if (Configuration::showExpanded()) {
		if (_view->activeFunction()) {
			if (_view->activeFunction()->cycle())
				totalCost = _view->activeFunction()->cycle()->inclusive();
			else
				totalCost = _view->activeFunction()->inclusive();
		} else
			totalCost = (TraceCost*) _view->activeItem();
	} else
		totalCost = ((TraceItemView*)_view)->data();
	double total = totalCost->subCost(_view->eventType());
	double inclP = 100.0 * n->incl/ total;
	if (Configuration::showPercentage())
		setText(1, QString("%1 %")
		.arg(inclP, 0, 'f', Configuration::percentPrecision()));
	else
		setText(1, SubCost(n->incl).pretty());
	setPixmap(1, percentagePixmap(25, 10, (int)(inclP+.5), Qt::blue, true));

	setToolTip(QString("%1 (%2)").arg(text(0)).arg(text(1)));
}

void CanvasNode::setSelected(bool s)
{
	StoredDrawParams::setSelected(s);
	update();
}

void CanvasNode::updateGroup()
{
	if (!_view || !_node)
		return;

	QColor c = Configuration::functionColor(_view->groupType(),
	                                        _node->function());
	setBackColor(c);
	update();
}

void CanvasNode::paint(QPainter* p,
		const QStyleOptionGraphicsItem* option,
		QWidget*)
{
	QRect r = rect().toRect(), origRect = r;

	r.setRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);

	RectDrawing d(r);
	d.drawBack(p, this);
	r.setRect(r.x()+2, r.y()+2, r.width()-4, r.height()-4);

#if 0
	if (StoredDrawParams::selected() && _view->hasFocus()) {
		_view->style().drawPrimitive( QStyle::PE_FocusRect, &p, r,
				_view->colorGroup());
	}
#endif

	// draw afterwards to always get a frame even when zoomed
	p->setPen(StoredDrawParams::selected() ? Qt::red : Qt::black);
	p->drawRect(QRect(origRect.x(), origRect.y(), origRect.width()-1,
	                  origRect.height()-1));

	if (option->levelOfDetail < .5)
		return;

	d.setRect(r);
	d.drawField(p, 0, this);
	d.drawField(p, 1, this);
}


//
// CanvasEdgeLabel
//

CanvasEdgeLabel::CanvasEdgeLabel(CallGraphView* v, CanvasEdge* ce, int x,
                                 int y, int w, int h) :
	QGraphicsRectItem(QRect(x, y, w, h)), _ce(ce), _view(v), _percentage(0.0)
{
	GraphEdge* e = ce->edge();
	if (!e)
		return;

	setPosition(1, DrawParams::BottomCenter);
	TraceCost* totalCost;
	if (Configuration::showExpanded()) {
		if (_view->activeFunction()) {
			if (_view->activeFunction()->cycle())
				totalCost = _view->activeFunction()->cycle()->inclusive();
			else
				totalCost = _view->activeFunction()->inclusive();
		} else
			totalCost = (TraceCost*) _view->activeItem();
	} else
		totalCost = ((TraceItemView*)_view)->data();
	double total = totalCost->subCost(_view->eventType());
	double inclP = 100.0 * e->cost/ total;
	if (Configuration::showPercentage())
		setText(1, QString("%1 %")
		.arg(inclP, 0, 'f', Configuration::percentPrecision()));
	else
		setText(1, SubCost(e->cost).pretty());

	setPosition(0, DrawParams::TopCenter);
	SubCost count((e->count < 1.0) ? 1.0 : e->count);
	setText(0, QString("%1 x").arg(count.pretty()));
	setPixmap(0, percentagePixmap(25, 10, (int)(inclP+.5), Qt::blue, true));

	_percentage = inclP;
	if (_percentage > 100.0) _percentage = 100.0;

	if (e->call() && (e->call()->isRecursion() || e->call()->inCycle())) {
		QString icon = "edit-undo";
		KIconLoader* loader = KIconLoader::global();
		QPixmap p= loader->loadIcon(icon, KIconLoader::Small, 0,
		                            KIconLoader::DefaultState, QStringList(), 0,
		                            true);
		setPixmap(0, p);
	}

	setToolTip(QString("%1 (%2)").arg(text(0)).arg(text(1)));
}

void CanvasEdgeLabel::paint(QPainter* p,
                            const QStyleOptionGraphicsItem* option, QWidget*)
{
	// draw nothing in PanningView
	if (option->levelOfDetail < .5)
		return;

	QRect r = rect().toRect();

	RectDrawing d(r);
	d.drawField(p, 0, this);
	d.drawField(p, 1, this);
}



//
// CanvasEdgeArrow

CanvasEdgeArrow::CanvasEdgeArrow(CanvasEdge* ce)
    : _ce(ce)
{}

void CanvasEdgeArrow::paint(QPainter* p,
		const QStyleOptionGraphicsItem *, QWidget *)
{
	p->setRenderHint(QPainter::Antialiasing);
	p->setBrush(_ce->isSelected() ? Qt::red : Qt::black);
	p->drawPolygon(polygon(), Qt::OddEvenFill);
}


//
// CanvasEdge
//

CanvasEdge::CanvasEdge(GraphEdge* e) :
	_edge(e)
{
	_label = 0;
	_arrow = 0;
	_thickness = 0;

	setFlag(QGraphicsItem::ItemIsSelectable);
}

void CanvasEdge::setLabel(CanvasEdgeLabel* l)
{
	_label = l;

	if (l) {
		QString tip = QString("%1 (%2)").arg(l->text(0)).arg(l->text(1));

		setToolTip(tip);
		if (_arrow) _arrow->setToolTip(tip);

		_thickness = log(l->percentage());
		if (_thickness < .9) _thickness = .9;
	}
}

void CanvasEdge::setArrow(CanvasEdgeArrow* a)
{
	_arrow = a;

	if (a && _label) a->setToolTip(QString("%1 (%2)")
	                               .arg(_label->text(0)).arg(_label->text(1)));
}

void CanvasEdge::setSelected(bool s)
{
	QGraphicsItem::setSelected(s);
	update();
}

void CanvasEdge::setControlPoints(const QPolygon& pa)
{
	_points = pa;

	QPainterPath path;
	path.moveTo(pa[0]);
	for (int i = 1; i < pa.size(); i += 3)
		path.cubicTo(pa[i], pa[(i + 1) % pa.size()], pa[(i + 2) % pa.size()]);

	setPath(path);
}


void CanvasEdge::paint(QPainter* p,
		const QStyleOptionGraphicsItem* options, QWidget*)
{
	p->setRenderHint(QPainter::Antialiasing);

	QPen pen = QPen(Qt::black);
	pen.setWidthF(1.0/options->levelOfDetail * _thickness);
	p->setPen(pen);
	p->drawPath(path());

	if (isSelected()) {
		pen.setColor(Qt::red);
		pen.setWidthF(1.0/options->levelOfDetail * _thickness/2.0);
		p->setPen(pen);
		p->drawPath(path());
	}
}


//
// CanvasFrame
//

QPixmap* CanvasFrame::_p = 0;

CanvasFrame::CanvasFrame(CanvasNode* n)
{
	if (!_p) {

		int d = 5;
		float v1 = 130.0f, v2 = 10.0f, v = v1, f = 1.03f;

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

		r.translate(-r.x(), -r.y());

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

	setRect(QRectF(n->rect().center().x() - _p->width()/2,
	               n->rect().center().y() - _p->height()/2, _p->width(), _p->height()) );
}


void CanvasFrame::paint(QPainter* p,
		const QStyleOptionGraphicsItem* option, QWidget*)
{
	if (option->levelOfDetail < .5) {
		QRadialGradient g(rect().center(), rect().width()/3);
		g.setColorAt(0.0, Qt::gray);
		g.setColorAt(1.0, Qt::white);

		p->setBrush(QBrush(g));
		p->setPen(Qt::NoPen);
		p->drawRect(rect());
		return;
	}

	p->drawPixmap(int( rect().x()),int( rect().y()), *_p );
}



//
// CallGraphView
//
CallGraphView::CallGraphView(TraceItemView* parentView, QWidget* parent,
                             const char* name) :
	QGraphicsView(parent), TraceItemView(parentView)
{
	setObjectName(name);
	_zoomPosition = DEFAULT_ZOOMPOS;
	_lastAutoPosition = TopLeft;

	_scene = 0;
	_xMargin = _yMargin = 0;
	_panningView = new PanningView(this);
	_panningZoom = 1;
	_selectedNode = 0;
	_selectedEdge = 0;
	_isMoving = false;

	_exporter.setGraphOptions(this);

	_panningView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_panningView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_panningView->raise();
	_panningView->hide();

	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_NoSystemBackground, true);

	connect(_panningView, SIGNAL(zoomRectMoved(qreal,qreal)), this, SLOT(zoomRectMoved(qreal,qreal)));
	connect(_panningView, SIGNAL(zoomRectMoveFinished()), this, SLOT(zoomRectMoveFinished()));

	this->setWhatsThis(whatsThis() );

	// tooltips...
	//_tip = new CallGraphTip(this);

	_renderProcess = 0;
	_prevSelectedNode = 0;
	connect(&_renderTimer, SIGNAL(timeout()), this, SLOT(showRenderWarning()));
}

CallGraphView::~CallGraphView()
{
	clear();
	delete _panningView;
}

QString CallGraphView::whatsThis() const
{
	return i18n("<b>Call Graph around active Function</b>"
		"<p>Depending on configuration, this view shows "
		"the call graph environment of the active function. "
		"Note: the shown cost is <b>only</b> the cost which is "
		"spent while the active function was actually running; "
		"i.e. the cost shown for main() - if it is visible - should "
		"be the same as the cost of the active function, as that is "
		"the part of inclusive cost of main() spent while the active "
		"function was running.</p>"
		"<p>For cycles, blue call arrows indicate that this is an "
		"artificial call added for correct drawing which "
		"actually never happened.</p>"
		"<p>If the graph is larger than the widget area, an overview "
		"panner is shown in one edge. "
		"There are similar visualization options to the "
		"Call Treemap; the selected function is highlighted.</p>");
}

void CallGraphView::updateSizes(QSize s)
{
	if (!_scene)
		return;

	if (s == QSize(0, 0))
		s = size();

	// the part of the scene that should be visible
	int cWidth = (int)_scene->width() - 2*_xMargin + 100;
	int cHeight = (int)_scene->height() - 2*_yMargin + 100;

	// hide birds eye view if no overview needed
	if (!_data || !_activeItem ||
			((cWidth < s.width()) && (cHeight < s.height())) ) {
		_panningView->hide();
		return;
	}

	// first, assume use of 1/3 of width/height (possible larger)
	double zoom = .33 * s.width() / cWidth;
	if (zoom * cHeight < .33 * s.height())
		zoom = .33 * s.height() / cHeight;

	// fit to widget size
	if (cWidth * zoom > s.width())
		zoom = s.width() / (double)cWidth;
	if (cHeight * zoom > s.height())
		zoom = s.height() / (double)cHeight;

	// scale to never use full height/width
	zoom = zoom * 3/4;

	// at most a zoom of 1/3
	if (zoom > .33)
		zoom = .33;

	if (zoom != _panningZoom) {
		_panningZoom = zoom;
		if (0)
			qDebug("Canvas Size: %fx%f, Content: %dx%d, Zoom: %f",
			       _scene->width(), _scene->height(), cWidth, cHeight, zoom);

		QMatrix m;
		_panningView->setMatrix(m.scale(zoom, zoom));

		// make it a little bigger to compensate for widget frame
		_panningView->resize(int(cWidth * zoom) + 4, int(cHeight * zoom) + 4);

		// update ZoomRect in panningView
		scrollContentsBy(0, 0);
	}

    _panningView->centerOn(_scene->width()/2, _scene->height()/2);

	int cvW = _panningView->width();
	int cvH = _panningView->height();
	int x = width()- cvW - verticalScrollBar()->width() -2;
	int y = height()-cvH - horizontalScrollBar()->height() -2;
	QPoint oldZoomPos = _panningView->pos();
	QPoint newZoomPos = QPoint(0, 0);
	ZoomPosition zp = _zoomPosition;
	if (zp == Auto) {
		int tlCols = items(QRect(0,0, cvW,cvH)).count();
		int trCols = items(QRect(x,0, cvW,cvH)).count();
		int blCols = items(QRect(0,y, cvW,cvH)).count();
		int brCols = items(QRect(x,y, cvW,cvH)).count();
		int minCols = tlCols;

		zp = _lastAutoPosition;
		switch (zp) {
		case TopRight:
			minCols = trCols;
			break;
		case BottomLeft:
			minCols = blCols;
			break;
		case BottomRight:
			minCols = brCols;
			break;
		default:
		case TopLeft:
			minCols = tlCols;
			break;
		}

		if (minCols > tlCols) {
			minCols = tlCols;
			zp = TopLeft;
		}
		if (minCols > trCols) {
			minCols = trCols;
			zp = TopRight;
		}
		if (minCols > blCols) {
			minCols = blCols;
			zp = BottomLeft;
		}
		if (minCols > brCols) {
			minCols = brCols;
			zp = BottomRight;
		}

		_lastAutoPosition = zp;
	}

	switch (zp) {
	case TopLeft:
		newZoomPos = QPoint(0, 0);
		break;
	case TopRight:
		newZoomPos = QPoint(x, 0);
		break;
	case BottomLeft:
		newZoomPos = QPoint(0, y);
		break;
	case BottomRight:
		newZoomPos = QPoint(x, y);
		break;
	default:
		break;
	}

	if (newZoomPos != oldZoomPos)
		_panningView->move(newZoomPos);

	if (zp == Hide)
		_panningView->hide();
	else
		_panningView->show();
}

void CallGraphView::focusInEvent(QFocusEvent*)
{
    if (!_scene) return;

    if (_selectedNode && _selectedNode->canvasNode()) {
	_selectedNode->canvasNode()->setSelected(true); // requests item update
	_scene->update();
    }
}

void CallGraphView::focusOutEvent(QFocusEvent* e)
{
    // trigger updates as in focusInEvent
    focusInEvent(e);
}

void CallGraphView::keyPressEvent(QKeyEvent* e)
{
	if (!_scene) {
		e->ignore();
		return;
	}

	if ((e->key() == Qt::Key_Return) ||(e->key() == Qt::Key_Space)) {
		if (_selectedNode)
			activated(_selectedNode->function());
		else if (_selectedEdge && _selectedEdge->call())
			activated(_selectedEdge->call());
		return;
	}

	// move selected node/edge
	if (!(e->state() & (Qt::ShiftModifier | Qt::ControlModifier))
	        &&(_selectedNode || _selectedEdge)&&((e->key() == Qt::Key_Up)
	        ||(e->key() == Qt::Key_Down)||(e->key() == Qt::Key_Left)||(e->key()
	        == Qt::Key_Right))) {

		TraceFunction* f = 0;
		TraceCall* c = 0;

		// rotate arrow key meaning for LeftRight layout
		int key = e->key();
		if (_layout == LeftRight) {
			switch (key) {
			case Qt::Key_Up:
				key = Qt::Key_Left;
				break;
			case Qt::Key_Down:
				key = Qt::Key_Right;
				break;
			case Qt::Key_Left:
				key = Qt::Key_Up;
				break;
			case Qt::Key_Right:
				key = Qt::Key_Down;
				break;
			default:
				break;
			}
		}

		if (_selectedNode) {
			if (key == Qt::Key_Up)
				c = _selectedNode->visibleCaller();
			if (key == Qt::Key_Down)
				c = _selectedNode->visibleCallee();
			if (key == Qt::Key_Right)
				f = _selectedNode->nextVisible();
			if (key == Qt::Key_Left)
				f = _selectedNode->priorVisible();
		} else if (_selectedEdge) {
			if (key == Qt::Key_Up)
				f = _selectedEdge->visibleCaller();
			if (key == Qt::Key_Down)
				f = _selectedEdge->visibleCallee();
			if (key == Qt::Key_Right)
				c = _selectedEdge->nextVisible();
			if (key == Qt::Key_Left)
				c = _selectedEdge->priorVisible();
		}

		if (c)
			selected(c);
		if (f)
			selected(f);
		return;
	}

	// move canvas...
	QPointF center = mapToScene(viewport()->rect().center());
	if (e->key() == Qt::Key_Home)
		centerOn(center + QPointF(-_scene->width(), 0));
	else if (e->key() == Qt::Key_End)
		centerOn(center + QPointF(_scene->width(), 0));
	else if (e->key() == Qt::Key_PageUp) {
		QPointF dy = mapToScene(0, height()) - mapToScene(0, 0);
		centerOn(center + QPointF(-dy.x()/2, -dy.y()/2));
	} else if (e->key() == Qt::Key_PageDown) {
		QPointF dy = mapToScene(0, height()) - mapToScene(0, 0);
		centerOn(center + QPointF(dy.x()/2, dy.y()/2));
	} else if (e->key() == Qt::Key_Left) {
		QPointF dx = mapToScene(width(), 0) - mapToScene(0, 0);
		centerOn(center + QPointF(-dx.x()/10, -dx.y()/10));
	} else if (e->key() == Qt::Key_Right) {
		QPointF dx = mapToScene(width(), 0) - mapToScene(0, 0);
		centerOn(center + QPointF(dx.x()/10, dx.y()/10));
	} else if (e->key() == Qt::Key_Down) {
		QPointF dy = mapToScene(0, height()) - mapToScene(0, 0);
		centerOn(center + QPointF(dy.x()/10, dy.y()/10));
	} else if (e->key() == Qt::Key_Up) {
		QPointF dy = mapToScene(0, height()) - mapToScene(0, 0);
		centerOn(center + QPointF(-dy.x()/10, -dy.y()/10));
	} else
		e->ignore();
}

void CallGraphView::resizeEvent(QResizeEvent* e)
{
	QGraphicsView::resizeEvent(e);
	if (_scene)
		updateSizes(e->size());
}

TraceItem* CallGraphView::canShow(TraceItem* i)
{
	if (i) {
		switch (i->type()) {
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
	if (changeType == eventType2Changed)
		return;

	if (changeType == selectedItemChanged) {
		if (!_scene)
			return;

		if (!_selectedItem)
			return;

		GraphNode* n = 0;
		GraphEdge* e = 0;
		if ((_selectedItem->type() == TraceItem::Function)
		        ||(_selectedItem->type() == TraceItem::FunctionCycle)) {
			n = _exporter.node((TraceFunction*)_selectedItem);
			if (n == _selectedNode)
				return;
		} else if (_selectedItem->type() == TraceItem::Call) {
			TraceCall* c = (TraceCall*)_selectedItem;
			e = _exporter.edge(c->caller(false), c->called(false));
			if (e == _selectedEdge)
				return;
		}

		// unselect any selected item
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

			if (!_isMoving)
				sNode = _selectedNode->canvasNode();
		}
		if (e && e->canvasEdge()) {
			_selectedEdge = e;
			_selectedEdge->canvasEdge()->setSelected(true);

#if 0 // do not change position when selecting edge
			if (!_isMoving) {
				if (_selectedEdge->fromNode())
				sNode = _selectedEdge->fromNode()->canvasNode();
				if (!sNode && _selectedEdge->toNode())
				sNode = _selectedEdge->toNode()->canvasNode();
			}
#endif
		}
		if (sNode)
			ensureVisible(sNode);

		_scene->update();
		return;
	}

	if (changeType == groupTypeChanged) {
		if (!_scene)
			return;

		if (_clusterGroups) {
			refresh();
			return;
		}

		QList<QGraphicsItem *> l = _scene->items();
		for (int i = 0; i < l.size(); ++i)
			if (l[i]->type() == CANVAS_NODE)
				((CanvasNode*)l[i])->updateGroup();

		_scene->update();
		return;
	}

	if (changeType & dataChanged) {
		// invalidate old selection and graph part
		_exporter.reset(_data, _activeItem, _eventType, _groupType);
		_selectedNode = 0;
		_selectedEdge = 0;
	}

	refresh();
}

void CallGraphView::clear()
{
	if (!_scene)
		return;

	_panningView->setScene(0);
	setScene(0);
	delete _scene;
	_scene = 0;
}

void CallGraphView::showText(QString s)
{
	clear();
	_renderTimer.stop();

	_scene = new QGraphicsScene;

	_scene->addSimpleText(s);
	centerOn(0, 0);
	setScene(_scene);
	_scene->update();
	_panningView->hide();
}

void CallGraphView::showRenderWarning()
{
	QString s;

	if (_renderProcess)
		s = i18n("Warning: a long lasting graph layouting is in progress.\n"
			"Reduce node/edge limits for speedup.\n");
	else
		s = i18n("Layouting stopped.\n");

	s.append(i18n("The call graph has %1 nodes and %2 edges.\n",
	        _exporter.nodeCount(), _exporter.edgeCount()));

	showText(s);
}

void CallGraphView::stopRendering()
{
	if (!_renderProcess)
		return;

	_renderProcess->kill();
	delete _renderProcess;
	_renderProcess = 0;
	_unparsedOutput = QString();

	_renderTimer.start(200, true);
}

void CallGraphView::refresh()
{
	// trigger start of background rendering
	if (_renderProcess)
		stopRendering();

	// we want to keep a selected node item at the same global position
	_prevSelectedNode = _selectedNode;
	_prevSelectedPos = QPoint(-1, -1);
	if (_selectedNode) {
		QPointF center = _selectedNode->canvasNode()->rect().center();
		_prevSelectedPos = mapFromScene(center);
	}

	if (!_data || !_activeItem) {
		showText(i18n("No item activated for which to draw the call graph."));
		return;
	}

	TraceItem::CostType t = _activeItem->type();
	switch (t) {
	case TraceItem::Function:
	case TraceItem::FunctionCycle:
	case TraceItem::Call:
		break;
	default:
		showText(i18n("No call graph can be drawn for the active item."));
		return;
	}

	if (1)
		kDebug() << "CallGraphView::refresh";

	_selectedNode = 0;
	_selectedEdge = 0;
	_exporter.reset(_data, _activeItem, _eventType, _groupType);
	_exporter.writeDot();

	_renderProcess = new Q3Process(this);
	if (_layout == GraphOptions::Circular)
		_renderProcess->addArgument("twopi");
	else
		_renderProcess->addArgument("dot");
	_renderProcess->addArgument(_exporter.filename());
	_renderProcess->addArgument("-Tplain");

	connect(_renderProcess, SIGNAL(readyReadStdout()), this, SLOT(readDotOutput()));
	connect(_renderProcess, SIGNAL(processExited()), this, SLOT(dotExited()));

	if (1)
		kDebug() << "Running '" << _renderProcess->arguments().join(" ") << "'...";

	if ( !_renderProcess->start() ) {
		QString e = i18n("No call graph is available because the following\n"
			"command cannot be run:\n'%1'\n", _renderProcess->arguments().join(" "));
		e += i18n("Please check that 'dot' is installed (package GraphViz).");
		showText(e);

		delete _renderProcess;
		_renderProcess = 0;

		return;
	}

	_unparsedOutput = QString();

	// layouting of more than seconds is dubious
	_renderTimer.setSingleShot(true);
	_renderTimer.start(1000);
}

void CallGraphView::readDotOutput()
{
	_unparsedOutput.append(_renderProcess->readStdout() );
}

void CallGraphView::dotExited()
{
	QString line, cmd;
	CanvasNode *rItem;
	QGraphicsEllipseItem* eItem;
	CanvasEdge* sItem;
	CanvasEdgeLabel* lItem;
	QTextStream* dotStream;
	double scale = 1.0, scaleX = 1.0, scaleY = 1.0;
	double dotWidth = 0, dotHeight = 0;
	GraphNode* activeNode = 0;
	GraphEdge* activeEdge = 0;

	_renderTimer.stop();
	viewport()->setUpdatesEnabled(false);
	clear();
	dotStream = new QTextStream(&_unparsedOutput, QIODevice::ReadOnly);

	// First pass to adjust coordinate scaling by node height given from dot
	// Normal detail level (=1) should be 3 lines using general KDE font
	double nodeHeight = 0.0;
	while(1) {
		line = dotStream->readLine();
		if (line.isNull() || line.isEmpty()) continue;
		QTextStream lineStream(&line, QIODevice::ReadOnly);
		lineStream >> cmd;
		if (cmd != "node") continue;
		QString s, h;
		lineStream >> s /*name*/ >> s /*x*/ >> s /*y*/ >> s /*width*/ >> h /*height*/;
		nodeHeight = h.toDouble();
		break;
	}
	Q_ASSERT(nodeHeight > 0.0);
	scaleY = (8 + (1 + 2 * _detailLevel) * fontMetrics().height()) / nodeHeight;
	scaleX = 80;

	dotStream->seek(0);
	int lineno = 0;
	while (1) {
		line = dotStream->readLine();
		if (line.isNull())
			break;

		lineno++;
		if (line.isEmpty())
			continue;

		QTextStream lineStream(&line, QIODevice::ReadOnly);
		lineStream >> cmd;

		if (0)
			qDebug("%s:%d - line '%s', cmd '%s'",
			       _exporter.filename().ascii(),
			       lineno, line.ascii(), cmd.ascii());

		if (cmd == "stop")
			break;

		if (cmd == "graph") {
			QString dotWidthString, dotHeightString;
			// scale will not be used
			lineStream >> scale >> dotWidthString >> dotHeightString;
			dotWidth = dotWidthString.toDouble();
			dotHeight = dotHeightString.toDouble();

			if (!_scene) {
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

				_scene = new QGraphicsScene( 0.0, 0.0,
						qreal(w+2*_xMargin), qreal(h+2*_yMargin));
				// Change background color for call graph from default system color to
				// white. It has to blend into the gradient for the selected function.
				_scene->setBackgroundBrush(Qt::white);

#if DEBUG_GRAPH
				kDebug() << _exporter.filename().ascii() << ":" << lineno
					<< " - graph (" << dotWidth << " x " << dotHeight
					<< ") => (" << w << " x " << h << ")" << endl;
#endif
			} else
				kWarning() << "Ignoring 2nd 'graph' from dot ("
				        << _exporter.filename() << ":"<< lineno << ")"<< endl;
			continue;
		}

    if ((cmd != "node") && (cmd != "edge")) {
			kWarning() << "Ignoring unknown command '"<< cmd
			        << "' from dot ("<< _exporter.filename() << ":"<< lineno
			        << ")"<< endl;
			continue;
		}

		if (_scene == 0) {
			kWarning() << "Ignoring '"<< cmd
			        << "' without 'graph' from dot ("<< _exporter.filename()
			        << ":"<< lineno << ")"<< endl;
			continue;
		}

		if (cmd == "node") {
			// x, y are centered in node
			QString nodeName, label, nodeX, nodeY, nodeWidth, nodeHeight;
			double x, y, width, height;
			lineStream >> nodeName >> nodeX >> nodeY >> nodeWidth
			        >> nodeHeight;
			x = nodeX.toDouble();
			y = nodeY.toDouble();
			width = nodeWidth.toDouble();
			height = nodeHeight.toDouble();

			GraphNode* n = _exporter.node(_exporter.toFunc(nodeName));

			int xx = (int)(scaleX * x + _xMargin);
			int yy = (int)(scaleY * (dotHeight - y)+ _yMargin);
			int w = (int)(scaleX * width);
			int h = (int)(scaleY * height);

#if DEBUG_GRAPH
			kDebug() << _exporter.filename() << ":" << lineno
			<< " - node '" << nodeName << "' ( "
			<< x << "/" << y << " - "
			<< width << "x" << height << " ) => ("
			<< xx-w/2 << "/" << yy-h/2 << " - "
			<< w << "x" << h << ")" << endl;
#endif

			// Unnamed nodes with collapsed edges (with 'R' and 'S')
			if (nodeName[0] == 'R'|| nodeName[0] == 'S') {
				w = 10, h = 10;
				eItem = new QGraphicsEllipseItem( QRectF(xx, yy, w, h) );
				_scene->addItem(eItem);
				eItem->setBrush(Qt::gray);
				eItem->setZValue(1.0);
				eItem->show();
				continue;
			}

			if (!n) {
				qDebug("Warning: Unknown function '%s' ?!", nodeName.ascii());
				continue;
			}
			n->setVisible(true);

			rItem = new CanvasNode(this, n, xx-w/2, yy-h/2, w, h);
			// limit symbol space to a maximal number of lines depending on detail level
			if (_detailLevel>0) rItem->setMaxLines(0, 2*_detailLevel);
			_scene->addItem(rItem);
			n->setCanvasNode(rItem);

			if (n) {
				if (n->function() == activeItem())
					activeNode = n;
				if (n->function() == selectedItem())
					_selectedNode = n;
				rItem->setSelected(n == _selectedNode);
			}

			rItem->setZValue(1.0);
			rItem->show();

			continue;
		}

		// edge

		QString node1Name, node2Name, label, edgeX, edgeY;
		double x, y;
		Q3PointArray pa;
		int points, i;
		lineStream >> node1Name >> node2Name >> points;

		GraphEdge* e = _exporter.edge(_exporter.toFunc(node1Name),
		                              _exporter.toFunc(node2Name));
		if (!e) {
			kWarning() << "Unknown edge '"<< node1Name << "'-'"<< node2Name
			        << "' from dot ("<< _exporter.filename() << ":"<< lineno
			        << ")"<< endl;
			continue;
		}
		e->setVisible(true);
		if (e->fromNode())
			e->fromNode()->addCallee(e);
		if (e->toNode())
			e->toNode()->addCaller(e);

		if (0)
			qDebug("  Edge with %d points:", points);

		pa.resize(points);
		for (i=0; i<points; i++) {
			if (lineStream.atEnd())
				break;
			lineStream >> edgeX >> edgeY;
			x = edgeX.toDouble();
			y = edgeY.toDouble();

			int xx = (int)(scaleX * x + _xMargin);
			int yy = (int)(scaleY * (dotHeight - y)+ _yMargin);

			if (0)
				qDebug("   P %d: ( %f / %f ) => ( %d / %d)", i, x, y, xx, yy);

			pa.setPoint(i, xx, yy);
		}
		if (i < points) {
			qDebug("CallGraphView: Can not read %d spline points (%s:%d)",
			       points, _exporter.filename().ascii(), lineno);
			continue;
		}

		// artifical calls should be blue
		bool isArtifical = false;
		if(e->fromNode()) {
			TraceFunction* caller = e->fromNode()->function();
			if (caller->cycle() == caller)
				isArtifical = true;
		}
		if(e->toNode()) {
			TraceFunction* called = e->toNode()->function();
			if (called->cycle() == called)
				isArtifical = true;
		}
		QColor arrowColor = isArtifical ? Qt::blue : Qt::black;

		sItem = new CanvasEdge(e);
		_scene->addItem(sItem);
		e->setCanvasEdge(sItem);
		sItem->setControlPoints(pa);
		sItem->setPen(QPen(arrowColor, 1 /*(int)log(log(e->cost))*/));
		sItem->setZValue(0.5);
		sItem->show();

		if (e->call() == selectedItem())
			_selectedEdge = e;
		if (e->call() == activeItem())
			activeEdge = e;
		sItem->setSelected(e == _selectedEdge);

		// Arrow head
		QPoint arrowDir;
		int indexHead = -1;

		// check if head is at start of spline...
		// this is needed because dot always gives points from top to bottom
		CanvasNode* fromNode = e->fromNode() ? e->fromNode()->canvasNode() : 0;
		if (fromNode) {
			QPointF toCenter = fromNode->rect().center();
			qreal dx0 = pa.point(0).x() - toCenter.x();
			qreal dy0 = pa.point(0).y() - toCenter.y();
			qreal dx1 = pa.point(points-1).x() - toCenter.x();
			qreal dy1 = pa.point(points-1).y() - toCenter.y();
			if (dx0*dx0+dy0*dy0 > dx1*dx1+dy1*dy1) {
				// start of spline is nearer to call target node
				indexHead=-1;
				while (arrowDir.isNull() && (indexHead<points-2)) {
					indexHead++;
					arrowDir = pa.point(indexHead) - pa.point(indexHead+1);
				}
			}
		}

		if (arrowDir.isNull()) {
			indexHead = points;
			// sometimes the last spline points from dot are the same...
			while (arrowDir.isNull() && (indexHead>1)) {
				indexHead--;
				arrowDir = pa.point(indexHead) - pa.point(indexHead-1);
			}
		}

		if (!arrowDir.isNull()) {
			// arrow around pa.point(indexHead) with direction arrowDir
			arrowDir *= 10.0/sqrt(double(arrowDir.x()*arrowDir.x() +
					arrowDir.y()*arrowDir.y()));

			QPolygonF a;
			a << QPointF(pa.point(indexHead) + arrowDir);
			a << QPointF(pa.point(indexHead) + QPoint(arrowDir.y()/2,
			                                          -arrowDir.x()/2));
			a << QPointF(pa.point(indexHead) + QPoint(-arrowDir.y()/2,
			                                          arrowDir.x()/2));

			if (0)
				qDebug("  Arrow: ( %f/%f, %f/%f, %f/%f)", a[0].x(), a[0].y(),
				       a[1].x(), a[1].y(), a[2].x(), a[1].y());

			CanvasEdgeArrow* aItem = new CanvasEdgeArrow(sItem);
			_scene->addItem(aItem);
			aItem->setPolygon(a);
			aItem->setBrush(arrowColor);
			aItem->setZValue(1.5);
			aItem->show();

			sItem->setArrow(aItem);
		}

		if (lineStream.atEnd())
			continue;

		// parse quoted label
		QChar c;
		lineStream >> c;
		while (c.isSpace())
			lineStream >> c;
		if (c != '\"') {
			lineStream >> label;
			label = c + label;
		} else {
			lineStream >> c;
			while (!c.isNull() && (c != '\"')) {
				//if (c == '\\') lineStream >> c;

				label += c;
				lineStream >> c;
			}
		}
		lineStream >> edgeX >> edgeY;
		x = edgeX.toDouble();
		y = edgeY.toDouble();

		int xx = (int)(scaleX * x + _xMargin);
		int yy = (int)(scaleY * (dotHeight - y)+ _yMargin);

		if (0)
			qDebug("   Label '%s': ( %f / %f ) => ( %d / %d)", label.ascii(),
			       x, y, xx, yy);

		// Fixed Dimensions for Label: 100 x 40
		int w = 100;
		int h = _detailLevel * 20;
		lItem = new CanvasEdgeLabel(this, sItem, xx-w/2, yy-h/2, w, h);
		_scene->addItem(lItem);
		// edge labels above nodes
		lItem->setZValue(1.5);
		sItem->setLabel(lItem);
		if (h>0)
			lItem->show();

	}
	delete dotStream;

	// for keyboard navigation
	_exporter.sortEdges();

	if (!_scene) {
		_scene = new QGraphicsScene;

		QString s = i18n("Error running the graph layouting tool.\n");
		s += i18n("Please check that 'dot' is installed (package GraphViz).");
		_scene->addSimpleText(s);
		centerOn(0, 0);
	} else if (!activeNode && !activeEdge) {
		QString s = i18n("There is no call graph available for function\n"
			"\t'%1'\n"
			"because it has no cost of the selected event type.",
		                 _activeItem->name());
		_scene->addSimpleText(s);
		centerOn(0, 0);
	}

	_panningView->setScene(_scene);
	setScene(_scene);

	// if we do not have a selection, or the old selection is not
	// in visible graph, make active function selected for this view
	if ((!_selectedNode || !_selectedNode->canvasNode()) &&
		(!_selectedEdge || !_selectedEdge->canvasEdge())) {
		if (activeNode) {
			_selectedNode = activeNode;
			_selectedNode->canvasNode()->setSelected(true);
		} else if (activeEdge) {
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
		if (_prevSelectedNode) {
			QPointF prevPos = mapToScene(_prevSelectedPos);
			if (rect().contains(_prevSelectedPos)) {
				QPointF wCenter = mapToScene(viewport()->rect().center());
				centerOn(sNode->rect().center() +
				         wCenter - prevPos);
			}
			else
				ensureVisible(sNode);
		} else
			centerOn(sNode);
	}

	if (activeNode) {
		CanvasNode* cn = activeNode->canvasNode();
		CanvasFrame* f = new CanvasFrame(cn);
		_scene->addItem(f);
		f->setPos(cn->pos());
		f->setZValue(-1);
	}

	_panningZoom = 0;
	updateSizes();

	_scene->update();
	viewport()->setUpdatesEnabled(true);

	delete _renderProcess;
	_renderProcess = 0;
}


// Called by QAbstractScrollArea to notify about scrollbar changes
void CallGraphView::scrollContentsBy(int dx, int dy)
{
	// call QGraphicsView implementation
	QGraphicsView::scrollContentsBy(dx, dy);

	QPointF topLeft = mapToScene(QPoint(0, 0));
	QPointF bottomRight = mapToScene(QPoint(width(), height()));

	QRectF z(topLeft, bottomRight);
	if (0)
		qDebug("CallGraphView::scrollContentsBy(dx %d, dy %d) - to (%f,%f - %f,%f)",
		       dx, dy, topLeft.x(), topLeft.y(),
		       bottomRight.x(), bottomRight.y());
	_panningView->setZoomRect(z);
}

void CallGraphView::zoomRectMoved(qreal dx, qreal dy)
{
	//FIXME if (leftMargin()>0) dx = 0;
	//FIXME if (topMargin()>0) dy = 0;

	QScrollBar *hBar = horizontalScrollBar();
	QScrollBar *vBar = verticalScrollBar();
	hBar->setValue(hBar->value() + int(dx));
	vBar->setValue(vBar->value() + int(dy));
}

void CallGraphView::zoomRectMoveFinished()
{
	if (_zoomPosition == Auto)
		updateSizes();
}

void CallGraphView::mousePressEvent(QMouseEvent* e)
{
	// clicking on the viewport sets focus
	setFocus();

	// activate scroll mode on left click
	if (e->button() == Qt::LeftButton)	_isMoving = true;

	QGraphicsItem* i = itemAt(e->pos());
	if (i) {

		if (i->type() == CANVAS_NODE) {
			GraphNode* n = ((CanvasNode*)i)->node();
			if (0)
				qDebug("CallGraphView: Got Node '%s'", n->function()->prettyName().ascii());

			selected(n->function());
		}

		// redirect from label / arrow to edge
		if (i->type() == CANVAS_EDGELABEL)
			i = ((CanvasEdgeLabel*)i)->canvasEdge();
		if (i->type() == CANVAS_EDGEARROW)
			i = ((CanvasEdgeArrow*)i)->canvasEdge();

		if (i->type() == CANVAS_EDGE) {
			GraphEdge* e = ((CanvasEdge*)i)->edge();
			if (0)
				qDebug("CallGraphView: Got Edge '%s'", e->prettyName().ascii());

			if (e->call())
				selected(e->call());
		}
	}
	_lastPos = e->pos();
}

void CallGraphView::mouseMoveEvent(QMouseEvent* e)
{
	if (_isMoving) {
		QPoint delta = e->pos() - _lastPos;

		QScrollBar *hBar = horizontalScrollBar();
		QScrollBar *vBar = verticalScrollBar();
		hBar->setValue(hBar->value() - delta.x());
		vBar->setValue(vBar->value() - delta.y());

		_lastPos = e->pos();
  }
}

void CallGraphView::mouseReleaseEvent(QMouseEvent*)
{
	_isMoving = false;
	if (_zoomPosition == Auto)
		updateSizes();
}

void CallGraphView::mouseDoubleClickEvent(QMouseEvent* e)
{
	QGraphicsItem* i = itemAt(e->pos());
	if (i == 0)
		return;

	if (i->type() == CANVAS_NODE) {
		GraphNode* n = ((CanvasNode*)i)->node();
		if (0)
			qDebug("CallGraphView: Double Clicked on Node '%s'", n->function()->prettyName().ascii());

		activated(n->function());
	}

	// redirect from label / arrow to edge
	if (i->type() == CANVAS_EDGELABEL)
		i = ((CanvasEdgeLabel*)i)->canvasEdge();
	if (i->type() == CANVAS_EDGEARROW)
		i = ((CanvasEdgeArrow*)i)->canvasEdge();

	if (i->type() == CANVAS_EDGE) {
		GraphEdge* e = ((CanvasEdge*)i)->edge();
		if (e->call()) {
			if (0)
				qDebug("CallGraphView: Double Clicked On Edge '%s'", e->call()->prettyName().ascii());

			activated(e->call());
		}
	}
}

// helper functions for context menu:
// submenu builders and trigger handlers

QAction* CallGraphView::addCallerDepthAction(QMenu* m, QString s, int d)
{
	QAction* a;

	a = m->addAction(s);
	a->setData(d);
	a->setCheckable(true);
	a->setChecked(_maxCallerDepth == d);

	return a;
}

QMenu* CallGraphView::callerDepthMenu(QWidget* parent)
{
	QAction* a;
	QMenu* m;

	m = new QMenu(parent);
	a = addCallerDepthAction(m, i18n("Unlimited"), -1);
	a->setEnabled(_funcLimit>0.005);
	m->addSeparator();
	addCallerDepthAction(m, i18nc("Depth 0", "None"), 0);
	addCallerDepthAction(m, i18n("max. 2"), 2);
	addCallerDepthAction(m, i18n("max. 5"), 5);
	addCallerDepthAction(m, i18n("max. 10"), 10);
	addCallerDepthAction(m, i18n("max. 15"), 15);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(callerDepthTriggered(QAction*)) );

	return m;
}

void CallGraphView::callerDepthTriggered(QAction* a)
{
	_maxCallerDepth = a->data().toInt(0);
	refresh();
}

QAction* CallGraphView::addCalleeDepthAction(QMenu* m, QString s, int d)
{
	QAction* a;

	a = m->addAction(s);
	a->setData(d);
	a->setCheckable(true);
	a->setChecked(_maxCalleeDepth == d);

	return a;
}

QMenu* CallGraphView::calleeDepthMenu(QWidget* parent)
{
	QAction* a;
	QMenu* m;

	m = new QMenu(parent);
	a = addCalleeDepthAction(m, i18n("Unlimited"), -1);
	a->setEnabled(_funcLimit>0.005);
	m->addSeparator();
	addCalleeDepthAction(m, i18nc("Depth 0", "None"), 0);
	addCalleeDepthAction(m, i18n("max. 2"), 2);
	addCalleeDepthAction(m, i18n("max. 5"), 5);
	addCalleeDepthAction(m, i18n("max. 10"), 10);
	addCalleeDepthAction(m, i18n("max. 15"), 15);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(calleeDepthTriggered(QAction*)) );

	return m;
}

void CallGraphView::calleeDepthTriggered(QAction* a)
{
	_maxCalleeDepth = a->data().toInt(0);
	refresh();
}

QAction* CallGraphView::addNodeLimitAction(QMenu* m, QString s, double l)
{
	QAction* a;

	a = m->addAction(s);
	a->setData(l);
	a->setCheckable(true);
	a->setChecked(_funcLimit == l);

	return a;
}

QMenu* CallGraphView::nodeLimitMenu(QWidget* parent)
{
	QAction* a;
	QMenu* m;

	m = new QMenu(parent);
	a = addNodeLimitAction(m, i18n("No Minimum"), 0.0);
	// Unlimited node cost easily produces huge graphs such that 'dot'
	// would need a long time to layout. For responsiveness, we only allow
	// for unlimited node cost if a caller and callee depth limit is set.
	a->setEnabled((_maxCallerDepth>=0) && (_maxCalleeDepth>=0));
	m->addSeparator();
	addNodeLimitAction(m, i18n("50 %"), .5);
	addNodeLimitAction(m, i18n("20 %"), .2);
	addNodeLimitAction(m, i18n("10 %"), .1);
	addNodeLimitAction(m, i18n("5 %"), .05);
	addNodeLimitAction(m, i18n("2 %"), .02);
	addNodeLimitAction(m, i18n("1 %"), .01);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(nodeLimitTriggered(QAction*)) );

	return m;
}

void CallGraphView::nodeLimitTriggered(QAction* a)
{
	_funcLimit = a->data().toDouble(0);
	refresh();
}

QAction* CallGraphView::addCallLimitAction(QMenu* m, QString s, double l)
{
	QAction* a;

	a = m->addAction(s);
	a->setData(l);
	a->setCheckable(true);
	a->setChecked(_callLimit == l);

	return a;
}

QMenu* CallGraphView::callLimitMenu(QWidget* parent)
{
	QMenu* m;

	m = new QMenu(parent);
	addCallLimitAction(m, i18n("Same as Node"), 1.0);
	// xgettext: no-c-format
	addCallLimitAction(m, i18n("50 % of Node"), .5);
	// xgettext: no-c-format
	addCallLimitAction(m, i18n("20 % of Node"), .2);
	// xgettext: no-c-format
	addCallLimitAction(m, i18n("10 % of Node"), .1);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(callLimitTriggered(QAction*)) );

	return m;
}

void CallGraphView::callLimitTriggered(QAction* a)
{
	_callLimit = a->data().toDouble(0);
	refresh();
}

QAction* CallGraphView::addZoomPosAction(QMenu* m, QString s,
                                         CallGraphView::ZoomPosition p)
{
	QAction* a;

	a = m->addAction(s);
	a->setData((int)p);
	a->setCheckable(true);
	a->setChecked(_zoomPosition == p);

	return a;
}

QMenu* CallGraphView::zoomPosMenu(QWidget* parent)
{
	QMenu* m;

	m = new QMenu(parent);
	addZoomPosAction(m, i18n("Top Left"), TopLeft);
	addZoomPosAction(m, i18n("Top Right"), TopRight);
	addZoomPosAction(m, i18n("Bottom Left"), BottomLeft);
	addZoomPosAction(m, i18n("Bottom Right"), BottomRight);
	addZoomPosAction(m, i18n("Automatic"), Auto);
	addZoomPosAction(m, i18n("Hide"), Hide);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(zoomPosTriggered(QAction*)) );

	return m;
}


void CallGraphView::zoomPosTriggered(QAction* a)
{
	_zoomPosition = (ZoomPosition) a->data().toInt(0);
	updateSizes();
}

QAction* CallGraphView::addLayoutAction(QMenu* m, QString s,
                                         GraphOptions::Layout l)
{
	QAction* a;

	a = m->addAction(s);
	a->setData((int)l);
	a->setCheckable(true);
	a->setChecked(_layout == l);

	return a;
}

QMenu* CallGraphView::layoutMenu(QWidget* parent)
{
	QMenu* m;

	m = new QMenu(parent);
	addLayoutAction(m, i18n("Top to Down"), TopDown);
	addLayoutAction(m, i18n("Left to Right"), LeftRight);
	addLayoutAction(m, i18n("Circular"), Circular);

	connect(m, SIGNAL(triggered(QAction*)),
			this, SLOT(layoutTriggered(QAction*)) );

	return m;
}


void CallGraphView::layoutTriggered(QAction* a)
{
	_layout = (Layout) a->data().toInt(0);
	refresh();
}


void CallGraphView::contextMenuEvent(QContextMenuEvent* e)
{
	_isMoving = false;

	QGraphicsItem* i = itemAt(e->pos());

	Q3PopupMenu popup;
	TraceFunction *f = 0, *cycle = 0;
	TraceCall* c = 0;

	if (i) {
		if (i->type() == CANVAS_NODE) {
			GraphNode* n = ((CanvasNode*)i)->node();
			if (0)
				qDebug("CallGraphView: Menu on Node '%s'", n->function()->prettyName().ascii());
			f = n->function();
			cycle = f->cycle();

			QString name = f->prettyName();
			popup.insertItem(i18n("Go to '%1'",
			                      Configuration::shortenSymbol(name)), 93);
			if (cycle && (cycle != f)) {
				name = Configuration::shortenSymbol(cycle->prettyName());
				popup.insertItem(i18n("Go to '%1'", name), 94);
			}
			popup.addSeparator();
		}

		// redirect from label / arrow to edge
		if (i->type() == CANVAS_EDGELABEL)
			i = ((CanvasEdgeLabel*)i)->canvasEdge();
		if (i->type() == CANVAS_EDGEARROW)
			i = ((CanvasEdgeArrow*)i)->canvasEdge();

		if (i->type() == CANVAS_EDGE) {
			GraphEdge* e = ((CanvasEdge*)i)->edge();
			if (0)
				qDebug("CallGraphView: Menu on Edge '%s'", e->prettyName().ascii());
			c = e->call();
			if (c) {
				QString name = c->prettyName();
				popup.insertItem(i18n("Go to '%1'",
				                      Configuration::shortenSymbol(name)), 95);

				popup.addSeparator();
			}
		}
	}

	if (_renderProcess) {
		popup.insertItem(i18n("Stop Layouting"), 999);
		popup.addSeparator();
	}

	addGoMenu(&popup);
	popup.addSeparator();

	Q3PopupMenu epopup;
	epopup.insertItem(i18n("As PostScript"), 201);
	epopup.insertItem(i18n("As Image ..."), 202);

	popup.insertItem(i18n("Export Graph"), &epopup, 200);
	popup.addSeparator();

	Q3PopupMenu gpopup;
	gpopup.setCheckable(true);
	gpopup.insertItem(i18n("Caller Depth"), callerDepthMenu(&gpopup));
	gpopup.insertItem(i18n("Callee Depth"), calleeDepthMenu(&gpopup));
	gpopup.insertItem(i18n("Min. Node Cost"), nodeLimitMenu(&gpopup));
	gpopup.insertItem(i18n("Min. Call Cost"), callLimitMenu(&gpopup));
	gpopup.addSeparator();
	gpopup.insertItem(i18n("Arrows for Skipped Calls"), 130);
	gpopup.setItemChecked(130, _showSkipped);
	gpopup.insertItem(i18n("Inner-cycle Calls"), 131);
	gpopup.setItemChecked(131, _expandCycles);
	gpopup.insertItem(i18n("Cluster Groups"), 132);
	gpopup.setItemChecked(132, _clusterGroups);

	Q3PopupMenu vpopup;
	vpopup.setCheckable(true);
	vpopup.insertItem(i18n("Compact"), 140);
	vpopup.insertItem(i18n("Normal"), 141);
	vpopup.insertItem(i18n("Tall"), 142);
	vpopup.setItemChecked(140, _detailLevel == 0);
	vpopup.setItemChecked(141, _detailLevel == 1);
	vpopup.setItemChecked(142, _detailLevel == 2);

	popup.insertItem(i18n("Graph"), &gpopup, 70);
	popup.insertItem(i18n("Visualization"), &vpopup, 71);
	popup.insertItem(i18n("Layout"), layoutMenu(&popup));
	popup.insertItem(i18n("Birds-eye View"), zoomPosMenu(&popup));

	int r = popup.exec(e->globalPos());

	switch (r) {
	case 93:
		activated(f);
		break;
	case 94:
		activated(cycle);
		break;
	case 95:
		activated(c);
		break;

	case 999:
		stopRendering();
		break;

	case 201: {
		TraceFunction* f = activeFunction();
		if (!f)
			break;

		QString n = QString("callgraph");
		GraphExporter ge(TraceItemView::data(), f, eventType(), groupType(),
		                 QString("%1.dot").arg(n));
		ge.setGraphOptions(this);
		ge.writeDot();

		QString cmd = QString("(dot %1.dot -Tps > %2.ps; kghostview %3.ps)&")
		.arg(n).arg(n).arg(n);
		system(cmd.toAscii());
	}
		break;

	case 202:
		// write current content of canvas as image to file
	{
		if (!_scene)
			return;

		QString fn = KFileDialog::getSaveFileName(KUrl("kfiledialog:///"),
		                                          "*.png");

		if (!fn.isEmpty()) {
			QRect r = _scene->sceneRect().toRect();
			QPixmap pix(r.width(), r.height());
			QPainter p(&pix);
			_scene->render( &p );
			pix.save(fn, "PNG");
		}
	}
		break;

	case 130:
		_showSkipped = !_showSkipped;
		break;
	case 131:
		_expandCycles = !_expandCycles;
		break;
	case 132:
		_clusterGroups = !_clusterGroups;
		break;

	case 140:
		_detailLevel = 0;
		break;
	case 141:
		_detailLevel = 1;
		break;
	case 142:
		_detailLevel = 2;
		break;

	default:
		break;
	}

	if (r>99&& r<170)
		refresh();
}


CallGraphView::ZoomPosition CallGraphView::zoomPos(QString s)
{
	if (s == QString("TopLeft"))
		return TopLeft;
	if (s == QString("TopRight"))
		return TopRight;
	if (s == QString("BottomLeft"))
		return BottomLeft;
	if (s == QString("BottomRight"))
		return BottomRight;
	if (s == QString("Automatic"))
		return Auto;
	if (s == QString("Hide"))
		return Hide;

	return DEFAULT_ZOOMPOS;
}

QString CallGraphView::zoomPosString(ZoomPosition p)
{
	if (p == TopLeft)
		return QString("TopLeft");
	if (p == TopRight)
		return QString("TopRight");
	if (p == BottomLeft)
		return QString("BottomLeft");
	if (p == BottomRight)
		return QString("BottomRight");
	if (p == Auto)
		return QString("Automatic");
	if (p == Hide)
		return QString("Hide");

	return QString();
}

void CallGraphView::readViewConfig(KConfig* c, const QString& prefix,
                                   const QString& postfix, bool)
{
	KConfigGroup g = configGroup(c, prefix, postfix);

	if (0)
		qDebug("CallGraphView::readViewConfig");

	_maxCallerDepth = g.readEntry("MaxCaller", DEFAULT_MAXCALLER);
	_maxCalleeDepth = g.readEntry("MaxCallee", DEFAULT_MAXCALLEE);
	_funcLimit = g.readEntry("FuncLimit", DEFAULT_FUNCLIMIT);
	_callLimit = g.readEntry("CallLimit", DEFAULT_CALLLIMIT);
	_showSkipped = g.readEntry("ShowSkipped", DEFAULT_SHOWSKIPPED);
	_expandCycles = g.readEntry("ExpandCycles", DEFAULT_EXPANDCYCLES);
	_clusterGroups = g.readEntry("ClusterGroups", DEFAULT_CLUSTERGROUPS);
	_detailLevel = g.readEntry("DetailLevel", DEFAULT_DETAILLEVEL);
	_layout = GraphOptions::layout(g.readEntry("Layout",
	                                           layoutString(DEFAULT_LAYOUT)));
	_zoomPosition = zoomPos(g.readEntry("ZoomPosition",
	                                    zoomPosString(DEFAULT_ZOOMPOS)));

}

void CallGraphView::saveViewConfig(KConfig* c, const QString& prefix,
                                   const QString& postfix, bool)
{
	KConfigGroup g(c, prefix+postfix);

	writeConfigEntry(g, "MaxCaller", _maxCallerDepth, DEFAULT_MAXCALLER);
	writeConfigEntry(g, "MaxCallee", _maxCalleeDepth, DEFAULT_MAXCALLEE);
	writeConfigEntry(g, "FuncLimit", _funcLimit, DEFAULT_FUNCLIMIT);
	writeConfigEntry(g, "CallLimit", _callLimit, DEFAULT_CALLLIMIT);
	writeConfigEntry(g, "ShowSkipped", _showSkipped, DEFAULT_SHOWSKIPPED);
	writeConfigEntry(g, "ExpandCycles", _expandCycles, DEFAULT_EXPANDCYCLES);
	writeConfigEntry(g, "ClusterGroups", _clusterGroups, DEFAULT_CLUSTERGROUPS);
	writeConfigEntry(g, "DetailLevel", _detailLevel, DEFAULT_DETAILLEVEL);
	writeConfigEntry(g, "Layout", layoutString(_layout),
	                 layoutString(DEFAULT_LAYOUT).toUtf8().data());
	writeConfigEntry(g, "ZoomPosition", zoomPosString(_zoomPosition),
	                 zoomPosString(DEFAULT_ZOOMPOS).toUtf8().data());
}

#include "callgraphview.moc"

