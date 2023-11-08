/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2007-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Callgraph View
 */

#include "callgraphview.h"

#include <stdlib.h>
#include <math.h>

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTransform>
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
#include <QScreen>
#include <QProcess>
#include <QMenu>


#include "config.h"
#include "globalguiconfig.h"
#include "listutils.h"


#define DEBUG_GRAPH 0

// CallGraphView defaults

#define DEFAULT_FUNCLIMIT     .05
#define DEFAULT_CALLLIMIT     1.
#define DEFAULT_MAXCALLER     2
#define DEFAULT_MAXCALLEE     -1
#define DEFAULT_SHOWSKIPPED   false
#define DEFAULT_EXPANDCYCLES  false
#define DEFAULT_CLUSTERGROUPS false
#define DEFAULT_DETAILLEVEL   1
#define DEFAULT_LAYOUT        GraphOptions::TopDown
#define DEFAULT_ZOOMPOS       Auto


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
        if (!ce1 && !ce2) {
            // strict ordering required for std::sort
            return (ge1 < ge2);
        }
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
    _f=nullptr;
    self = incl = 0;
    _cn = nullptr;

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
    std::sort(callers.begin(), callers.end(), callerGraphEdgeLessThan);
    std::sort(callees.begin(), callees.end(), calleeGraphEdgeLessThan);
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
               qPrintable(_f->prettyName()), _lastCallerIndex, (int) callers.count());

    // can not use at(): index can be -1 (out of bounds), result is 0 then
    GraphEdge* e = callers.value(_lastCallerIndex);
    if (e && !e->isVisible())
        e = nullptr;
    if (!e) {
        double maxCost = 0.0;
        GraphEdge* maxEdge = nullptr;
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
    return e ? e->call() : nullptr;
}

TraceCall* GraphNode::visibleCallee()
{
    if (0)
        qDebug("GraphNode::visibleCallee %s: last %d, count %d",
               qPrintable(_f->prettyName()), _lastCalleeIndex, (int) callees.count());

    GraphEdge* e = callees.value(_lastCalleeIndex);
    if (e && !e->isVisible())
        e = nullptr;

    if (!e) {
        double maxCost = 0.0;
        GraphEdge* maxEdge = nullptr;
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
    return e ? e->call() : nullptr;
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
    return nullptr;
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
    return nullptr;
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
    return nullptr;
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
    return nullptr;
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
    return nullptr;
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
    return nullptr;
}


//
// GraphEdge
//

GraphEdge::GraphEdge()
{
    _c=nullptr;
    _from = _to = nullptr;
    _fromNode = _toNode = nullptr;
    cost = count = 0;
    _ce = nullptr;

    _visible = false;
    _lastFromCaller = true;
}

QString GraphEdge::prettyName()
{
    if (_c)
        return _c->prettyName();

    if (_from)
        return QObject::tr("Call(s) from %1").arg(_from->prettyName());

    if (_to)
        return QObject::tr("Call(s) to %1").arg(_to->prettyName());

    return QObject::tr("(unknown call)");
}

TraceFunction* GraphEdge::visibleCaller()
{
    if (_from) {
        _lastFromCaller = true;
        if (_fromNode)
            _fromNode->setCallee(this);
        return _from;
    }
    return nullptr;
}

TraceFunction* GraphEdge::visibleCallee()
{
    if (_to) {
        _lastFromCaller = false;
        if (_toNode)
            _toNode->setCaller(this);
        return _to;
    }
    return nullptr;
}

TraceCall* GraphEdge::nextVisible()
{
    TraceCall* res = nullptr;

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
    TraceCall* res = nullptr;

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
        return QStringLiteral("Circular");
    if (l == LeftRight)
        return QStringLiteral("LeftRight");
    return QStringLiteral("TopDown");
}

GraphOptions::Layout GraphOptions::layout(QString s)
{
    if (s == QLatin1String("Circular"))
        return Circular;
    if (s == QLatin1String("LeftRight"))
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
    _tmpFile = nullptr;
    _item = nullptr;
    reset(nullptr, nullptr, nullptr, ProfileContext::InvalidType, QString());
}

GraphExporter::GraphExporter(TraceData* d, TraceFunction* f,
                             EventType* ct, ProfileContext::Type gt,
                             QString filename)
{
    _go = this;
    _tmpFile = nullptr;
    _item = nullptr;
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


void GraphExporter::reset(TraceData*, CostItem* i, EventType* ct,
                          ProfileContext::Type gt, QString filename)
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
        case ProfileContext::Function:
        case ProfileContext::FunctionCycle:
        case ProfileContext::Call:
            break;
        default:
            i = nullptr;
        }
    }

    _item = i;
    _eventType = ct;
    _groupType = gt;
    if (!i)
        return;

    if (filename.isEmpty()) {
        _tmpFile = new QTemporaryFile();
        //_tmpFile->setSuffix(".dot");
        _tmpFile->setAutoRemove(false);
        _tmpFile->open();
        _dotName = _tmpFile->fileName();
        _useBox = true;
    } else {
        _tmpFile = nullptr;
        _dotName = filename;
        _useBox = false;
    }
}



void GraphExporter::setGraphOptions(GraphOptions* go)
{
    if (go == nullptr)
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

    if ((_item->type() == ProfileContext::Function) ||(_item->type()
                                                       == ProfileContext::FunctionCycle)) {
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


bool GraphExporter::writeDot(QIODevice* device)
{
    if (!_item)
        return false;

    QFile* file = nullptr;
    QTextStream* stream = nullptr;

    if (device)
        stream = new QTextStream(device);
    else {
        if (_tmpFile)
            stream = new QTextStream(_tmpFile);
        else {
            file = new QFile(_dotName);
            if ( !file->open(QIODevice::WriteOnly ) ) {
                qDebug() << "Can not write dot file '"<< _dotName << "'";
                delete file;
                return false;
            }
            stream = new QTextStream(file);
        }
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
        *stream << QStringLiteral("  rankdir=LR;\n");
    } else if (_go->layout() == Circular) {
        TraceFunction *f = nullptr;
        switch (_item->type()) {
        case ProfileContext::Function:
        case ProfileContext::FunctionCycle:
            f = (TraceFunction*) _item;
            break;
        case ProfileContext::Call:
            f = ((TraceCall*)_item)->caller(true);
            break;
        default:
            break;
        }
        if (f)
            *stream << QStringLiteral("  center=F%1;\n").arg((qptrdiff)f, 0, 16);
        *stream << QStringLiteral("  overlap=false;\n  splines=true;\n");
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
        case ProfileContext::Object:
            g = f->object();
            break;
        case ProfileContext::Class:
            g = f->cls();
            break;
        case ProfileContext::File:
            g = f->file();
            break;
        case ProfileContext::FunctionCycle:
            g = f->cycle();
            break;
        default:
            g = nullptr;
            break;
        }
        nLists[g].append(&n);
    }

    QMap<TraceCostItem*,QList<GraphNode*> >::Iterator lit;
    int cluster = 0;
    for (lit = nLists.begin(); lit != nLists.end(); ++lit, cluster++) {
        QList<GraphNode*>& l = lit.value();
        TraceCostItem* i = lit.key();

        if (_go->clusterGroups() && i) {
            QString iabr = GlobalConfig::shortenSymbol(i->prettyName());
            // escape quotation marks in symbols to avoid invalid dot syntax
            iabr.replace("\"", "\\\"");
            *stream << QStringLiteral("subgraph \"cluster%1\" { label=\"%2\";\n")
                       .arg(cluster).arg(iabr);
        }

        foreach(GraphNode* np, l) {
            TraceFunction* f = np->function();

            QString abr = GlobalConfig::shortenSymbol(f->prettyName());
            // escape quotation marks to avoid invalid dot syntax
            abr.replace("\"", "\\\"");
            *stream << QStringLiteral("  F%1 [").arg((qptrdiff)f, 0, 16);
            if (_useBox) {
                // we want a minimal size for cost display
                if ((int)abr.length() < 8) abr = abr + QString(8 - abr.length(),'_');

                // make label 3 lines for CallGraphView
                *stream << QStringLiteral("shape=box,label=\"** %1 **\\n**\\n%2\"];\n")
                           .arg(abr)
                           .arg(SubCost(np->incl).pretty());
            } else
                *stream << QStringLiteral("label=\"%1\\n%2\"];\n")
                           .arg(abr)
                           .arg(SubCost(np->incl).pretty());
        }

        if (_go->clusterGroups() && i)
            *stream << QStringLiteral("}\n");
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
        to.removeEdge(&e);

        *stream << QStringLiteral("  F%1 -> F%2 [weight=%3")
                   .arg((qptrdiff)e.from(), 0, 16)
                   .arg((qptrdiff)e.to(), 0, 16)
                   .arg((long)log(log(e.cost)));

        if (_go->detailLevel() ==1) {
            *stream << QStringLiteral(",label=\"%1 (%2x)\"")
                       .arg(SubCost(e.cost).pretty())
                       .arg(SubCost(e.count).pretty());
        }
        else if (_go->detailLevel() ==2)
            *stream << QStringLiteral(",label=\"%3\\n%4 x\"")
                       .arg(SubCost(e.cost).pretty())
                       .arg(SubCost(e.count).pretty());

        *stream << QStringLiteral("];\n");
    }

    if (_go->showSkipped()) {

        // Create sum-edges for skipped edges
        GraphEdge* e;
        double costSum, countSum;
        for (nit = _nodeMap.begin(); nit != _nodeMap.end(); ++nit ) {
            GraphNode& n = *nit;
            if (n.incl <= _realFuncLimit)
                continue;

            // add edge for all skipped callers if cost sum is high enough
            costSum = n.callerCostSum();
            countSum = n.callerCountSum();
            if (costSum > _realCallLimit) {

                QPair<TraceFunction*,TraceFunction*> p(nullptr, n.function());
                e = &(_edgeMap[p]);
                e->setCallee(p.second);
                e->cost = costSum;
                e->count = countSum;

                *stream << QStringLiteral("  R%1 [shape=point,label=\"\"];\n")
                           .arg((qptrdiff)n.function(), 0, 16);
                *stream << QStringLiteral("  R%1 -> F%2 [label=\"%3\\n%4 x\",weight=%5];\n")
                           .arg((qptrdiff)n.function(), 0, 16)
                           .arg((qptrdiff)n.function(), 0, 16)
                           .arg(SubCost(costSum).pretty())
                           .arg(SubCost(countSum).pretty())
                           .arg((int)log(costSum));
            }

            // add edge for all skipped callees if cost sum is high enough
            costSum = n.calleeCostSum();
            countSum = n.calleeCountSum();
            if (costSum > _realCallLimit) {

                QPair<TraceFunction*,TraceFunction*> p(n.function(), nullptr);
                e = &(_edgeMap[p]);
                e->setCaller(p.first);
                e->cost = costSum;
                e->count = countSum;

                *stream << QStringLiteral("  S%1 [shape=point,label=\"\"];\n")
                           .arg((qptrdiff)n.function(), 0, 16);
                *stream << QStringLiteral("  F%1 -> S%2 [label=\"%3\\n%4 x\",weight=%5];\n")
                           .arg((qptrdiff)n.function(), 0, 16)
                           .arg((qptrdiff)n.function(), 0, 16)
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

    if (!device) {
        if (_tmpFile) {
            stream->flush();
            _tmpFile->seek(0);
        } else {
            file->close();
            delete file;
        }
    }
    delete stream;
    return true;
}

bool GraphExporter::savePrompt(QWidget *parent, TraceData *data,
                               TraceFunction *function, EventType *eventType,
                               ProfileContext::Type groupType,
                               CallGraphView* cgv)
{
    // More formats can be added; it's a matter of adding the filter and if
    QFileDialog saveDialog(parent, QObject::tr("Export Graph"));
    saveDialog.setMimeTypeFilters( {"text/vnd.graphviz", "application/pdf", "application/postscript"} );
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (saveDialog.exec()) {
        QString intendedName = saveDialog.selectedFiles().first();
        if (intendedName.isNull() || intendedName.isEmpty())
            return false;
        bool wrote = false;
        QString dotName, dotRenderType;
        QTemporaryFile maybeTemp;
        maybeTemp.open();
        const QString mime = saveDialog.selectedMimeTypeFilter();
        if (mime == "text/vnd.graphviz") {
            dotName = intendedName;
            dotRenderType = "";
        }
        else if (mime == "application/pdf") {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tpdf";
        }
        else if (mime == "application/postscript") {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tps";
        }
        GraphExporter ge(data, function, eventType, groupType, dotName);
        if (cgv != nullptr)
            ge.setGraphOptions(cgv);
        wrote = ge.writeDot();
        if (wrote && mime != "text/vnd.graphviz") {
            QProcess proc;
            proc.setStandardOutputFile(intendedName, QFile::Truncate);
            proc.start("dot", { dotRenderType, dotName }, QProcess::ReadWrite);
            proc.waitForFinished();
            wrote = proc.exitStatus() == QProcess::NormalExit;

            // Open in the default app, except for GraphViz files. The .dot
            // association is usually for Microsoft Word templates and will
            // open a word processor instead, which isn't what we want.
            if (wrote) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(intendedName));
            }
        }

        return wrote;
    }

    return false;
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
        return nullptr;
    bool ok;
    TraceFunction* f = (TraceFunction*) s.mid(1).toULongLong(&ok, 16);
    if (!ok)
        return nullptr;

    return f;
}

GraphNode* GraphExporter::node(TraceFunction* f)
{
    if (!f)
        return nullptr;

    GraphNodeMap::Iterator it = _nodeMap.find(f);
    if (it == _nodeMap.end())
        return nullptr;

    return &(*it);
}

GraphEdge* GraphExporter::edge(TraceFunction* f1, TraceFunction* f2)
{
    GraphEdgeMap::Iterator it = _edgeMap.find(qMakePair(f1, f2));
    if (it == _edgeMap.end())
        return nullptr;

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
void GraphExporter::buildGraph(TraceFunction* f, int depth, bool toCallees,
                               double factor)
{
#if DEBUG_GRAPH
    qDebug() << "buildGraph(" << f->prettyName() << "," << d << "," << factor
             << ") [to " << (toCallees ? "Callees":"Callers") << "]";
#endif

    double oldIncl = 0.0;
    GraphNode& n = _nodeMap[f];
    if (n.function() == nullptr) {
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
    // Never go beyond a depth of 100
    if ((maxDepth < 0) || (maxDepth>100)) maxDepth = 100;
    if (depth >= maxDepth) {
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

    TraceFunction* f2;

    // on entering a cycle, only go the FunctionCycle
    TraceCallList l = toCallees ? f->callings(false) : f->callers(false);

    foreach(TraceCall* call, l) {

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
        if (e.call() == nullptr) {
            e.setCall(call);
            e.setCaller(p.first);
            e.setCallee(p.second);
        } else
            oldCost = e.cost;

        e.cost += cost;
        e.count += count;
        if (0)
            qDebug("  Edge to %s, added cost %f, now %f",
                   qPrintable(f2->prettyName()), cost, e.cost);

        // if this call goes into a FunctionCycle, we also show the real call
        if (f2->cycle() == f2) {
            TraceFunction* realF;
            realF = toCallees ? call->called(true) : call->caller(true);
            QPair<TraceFunction*,TraceFunction*>
                    realP(toCallees ? f : realF, toCallees ? realF : f);
            GraphEdge& e = _edgeMap[realP];
            if (e.call() == nullptr) {
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
        if ((cost <= 0) || (cost <= _realCallLimit)) {
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

        // Never recurse if s or v is 0 (can happen with bogus input)
        if ((v == 0) || (s== 0)) continue;

        buildGraph(f2, depth+1, toCallees, factor * v / s);
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
    QPen pen(red.darker());
    pen.setWidthF(2.0 / transform().m11());
    p->setPen(pen);

    QColor c(red.darker());
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

    ProfileCostArray* totalCost;
    if (GlobalConfig::showExpanded()) {
        if (_view->activeFunction()) {
            if (_view->activeFunction()->cycle())
                totalCost = _view->activeFunction()->cycle()->inclusive();
            else
                totalCost = _view->activeFunction()->inclusive();
        } else
            totalCost = (ProfileCostArray*) _view->activeItem();
    } else
        totalCost = ((TraceItemView*)_view)->data();
    double total = totalCost->subCost(_view->eventType());
    double inclP = 100.0 * n->incl/ total;
    if (GlobalConfig::showPercentage())
        setText(1, QStringLiteral("%1 %")
                .arg(inclP, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(1, SubCost(n->incl).pretty());
    setPixmap(1, percentagePixmap(25, 10, (int)(inclP+.5), Qt::blue, true));

    setToolTip(QStringLiteral("%1 (%2)").arg(text(0)).arg(text(1)));
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

    QColor c = GlobalGUIConfig::functionColor(_view->groupType(),
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

#if QT_VERSION >= 0x040600
    if (option->levelOfDetailFromTransform(p->transform()) < .5)
        return;
#else
    if (option->levelOfDetail < .5)
        return;
#endif

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
    ProfileCostArray* totalCost;
    if (GlobalConfig::showExpanded()) {
        if (_view->activeFunction()) {
            if (_view->activeFunction()->cycle())
                totalCost = _view->activeFunction()->cycle()->inclusive();
            else
                totalCost = _view->activeFunction()->inclusive();
        } else
            totalCost = (ProfileCostArray*) _view->activeItem();
    } else
        totalCost = ((TraceItemView*)_view)->data();
    double total = totalCost->subCost(_view->eventType());
    double inclP = 100.0 * e->cost/ total;
    if (GlobalConfig::showPercentage())
        setText(1, QStringLiteral("%1 %")
                .arg(inclP, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(1, SubCost(e->cost).pretty());

    int pixPos = 1;
    if (((TraceItemView*)_view)->data()->maxCallCount() > 0) {
        setPosition(0, DrawParams::TopCenter);
        SubCost count((e->count < 1.0) ? 1.0 : e->count);
        setText(0, QStringLiteral("%1 x").arg(count.pretty()));
        pixPos = 0;
        setToolTip(QStringLiteral("%1 (%2)").arg(text(0)).arg(text(1)));
    }
    setPixmap(pixPos, percentagePixmap(25, 10, (int)(inclP+.5), Qt::blue, true));

    _percentage = inclP;
    if (_percentage > 100.0) _percentage = 100.0;

    if (e->call() && (e->call()->isRecursion() || e->call()->inCycle())) {
        QFontMetrics fm(font());
        QPixmap p = QIcon::fromTheme(QStringLiteral("edit-undo")).pixmap(fm.height());
        setPixmap(pixPos, p); // replace percentage pixmap
    }
}

void CanvasEdgeLabel::paint(QPainter* p,
                            const QStyleOptionGraphicsItem* option, QWidget*)
{
    // draw nothing in PanningView
#if QT_VERSION >= 0x040600
    if (option->levelOfDetailFromTransform(p->transform()) < .5)
        return;
#else
    if (option->levelOfDetail < .5)
        return;
#endif

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
    _label = nullptr;
    _arrow = nullptr;
    _thickness = 0;

    setFlag(QGraphicsItem::ItemIsSelectable);
}

void CanvasEdge::setLabel(CanvasEdgeLabel* l)
{
    _label = l;

    if (l) {
        QString tip = QStringLiteral("%1 (%2)").arg(l->text(0)).arg(l->text(1));

        setToolTip(tip);
        if (_arrow) _arrow->setToolTip(tip);

        _thickness = log(l->percentage());
        if (_thickness < .9) _thickness = .9;
    }
}

void CanvasEdge::setArrow(CanvasEdgeArrow* a)
{
    _arrow = a;

    if (a && _label) a->setToolTip(QStringLiteral("%1 (%2)")
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
                       const QStyleOptionGraphicsItem* option, QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);

    qreal levelOfDetail;
#if QT_VERSION >= 0x040600
    levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    levelOfDetail = option->levelOfDetail;
#endif

    QPen mypen = pen();
    mypen.setWidthF(1.0/levelOfDetail * _thickness);
    p->setPen(mypen);
    p->drawPath(path());

    if (isSelected()) {
        mypen.setColor(Qt::red);
        mypen.setWidthF(1.0/levelOfDetail * _thickness/2.0);
        p->setPen(mypen);
        p->drawPath(path());
    }
}


//
// CanvasFrame
//

QPixmap* CanvasFrame::_p = nullptr;

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
    qreal levelOfDetail;
#if QT_VERSION >= 0x040600
    levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    levelOfDetail = option->levelOfDetail;
#endif
    if (levelOfDetail < .5) {
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
                             const QString& name) :
    QGraphicsView(parent), TraceItemView(parentView)
{
    setObjectName(name);
    _zoomPosition = DEFAULT_ZOOMPOS;
    _lastAutoPosition = TopLeft;

    _scene = nullptr;
    _xMargin = _yMargin = 0;
    _panningView = new PanningView(this);
    _panningZoom = 1;
    _selectedNode = nullptr;
    _selectedEdge = nullptr;
    _isMoving = false;

    _exporter.setGraphOptions(this);

    _panningView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->raise();
    _panningView->hide();

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);

    connect(_panningView, &PanningView::zoomRectMoved, this, &CallGraphView::zoomRectMoved);
    connect(_panningView, &PanningView::zoomRectMoveFinished, this, &CallGraphView::zoomRectMoveFinished);

    this->setWhatsThis(whatsThis() );

    // tooltips...
    //_tip = new CallGraphTip(this);

    _renderProcess = nullptr;
    _prevSelectedNode = nullptr;
    connect(&_renderTimer, &QTimer::timeout,
            this, &CallGraphView::showRenderWarning);
}

CallGraphView::~CallGraphView()
{
    clear();
    delete _panningView;
}

QString CallGraphView::whatsThis() const
{
    return tr("<b>Call Graph around active Function</b>"
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

        QTransform m;
        _panningView->setTransform(m.scale(zoom, zoom));

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
    if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))
        &&(_selectedNode || _selectedEdge)&&((e->key() == Qt::Key_Up)
                                             ||(e->key() == Qt::Key_Down)||(e->key() == Qt::Key_Left)||(e->key()
                                                                                                        == Qt::Key_Right))) {

        TraceFunction* f = nullptr;
        TraceCall* c = nullptr;

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

CostItem* CallGraphView::canShow(CostItem* i)
{
    if (i) {
        switch (i->type()) {
        case ProfileContext::Function:
        case ProfileContext::FunctionCycle:
        case ProfileContext::Call:
            return i;
        default:
            break;
        }
    }
    return nullptr;
}

void CallGraphView::doUpdate(int changeType, bool)
{
    // Special case ?
    if (changeType == eventType2Changed)
        return;

    if (changeType == selectedItemChanged) {
        if (!_scene)
            return;

        if (!_selectedItem)
            return;

        GraphNode* n = nullptr;
        GraphEdge* e = nullptr;
        if ((_selectedItem->type() == ProfileContext::Function)
            ||(_selectedItem->type() == ProfileContext::FunctionCycle)) {
            n = _exporter.node((TraceFunction*)_selectedItem);
            if (n == _selectedNode)
                return;
        } else if (_selectedItem->type() == ProfileContext::Call) {
            TraceCall* c = (TraceCall*)_selectedItem;
            e = _exporter.edge(c->caller(false), c->called(false));
            if (e == _selectedEdge)
                return;
        }

        // unselect any selected item
        if (_selectedNode && _selectedNode->canvasNode()) {
            _selectedNode->canvasNode()->setSelected(false);
        }
        _selectedNode = nullptr;
        if (_selectedEdge && _selectedEdge->canvasEdge()) {
            _selectedEdge->canvasEdge()->setSelected(false);
        }
        _selectedEdge = nullptr;

        // select
        CanvasNode* sNode = nullptr;
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
        _selectedNode = nullptr;
        _selectedEdge = nullptr;
    }

    refresh();
}

void CallGraphView::clear()
{
    if (!_scene)
        return;

    _panningView->setScene(nullptr);
    setScene(nullptr);
    delete _scene;
    _scene = nullptr;
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
        s = tr("Warning: a long lasting graph layouting is in progress.\n"
               "Reduce node/edge limits for speedup.\n");
    else
        s = tr("Layouting stopped.\n");

    s.append(tr("The call graph has %1 nodes and %2 edges.\n")
             .arg(_exporter.nodeCount()).arg(_exporter.edgeCount()));

    showText(s);
}

void CallGraphView::showRenderError(QString s)
{
    QString err;
    err = tr("No graph available because the layouting process failed.\n");
    if (_renderProcess)
        err += tr("Trying to run the following command did not work:\n"
                  "'%1'\n").arg(_renderProcessCmdLine);
    err += tr("Please check that 'dot' is installed (package GraphViz).");

    if (!s.isEmpty())
        err += QStringLiteral("\n\n%1").arg(s);

    showText(err);
}

void CallGraphView::stopRendering()
{
    if (!_renderProcess)
        return;

    qDebug("CallGraphView::stopRendering: Killing QProcess %p",
           _renderProcess);

    _renderProcess->kill();

    // forget about this process, not interesting any longer
    _renderProcess->deleteLater();
    _renderProcess = nullptr;
    _unparsedOutput = QString();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(200);
}

void CallGraphView::refresh()
{
    // trigger start of new layouting via 'dot'
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
        showText(tr("No item activated for which to "
                    "draw the call graph."));
        return;
    }

    ProfileContext::Type t = _activeItem->type();
    switch (t) {
    case ProfileContext::Function:
    case ProfileContext::FunctionCycle:
    case ProfileContext::Call:
        break;
    default:
        showText(tr("No call graph can be drawn for "
                    "the active item."));
        return;
    }

    if (1)
        qDebug() << "CallGraphView::refresh";

    _selectedNode = nullptr;
    _selectedEdge = nullptr;

    /*
     * Call 'dot' asynchronously in the background with the aim to
     * - have responsive GUI while layout task runs (potentially long!)
     * - notify user about a long run, using a timer
     * - kill long running 'dot' processes when another layout is
     *   requested, as old data is not needed any more
     *
     * Even after killing a process, the QProcess needs some time
     * to make sure the process is destroyed; also, stdout data
     * still can be delivered after killing. Thus, there can/should be
     * multiple QProcess's at one time.
     * The QProcess we currently wait for data from is <_renderProcess>
     * Signals from other QProcesses are ignored with the exception of
     * the finished() signal, which triggers QProcess destruction.
     */
    QString renderProgram;
    QStringList renderArgs;
    if (_layout == GraphOptions::Circular)
        renderProgram = QStringLiteral("twopi");
    else
        renderProgram = QStringLiteral("dot");
    renderArgs << QStringLiteral("-Tplain");

    _unparsedOutput = QString();

    // display warning if layouting takes > 1s
    _renderTimer.setSingleShot(true);
    _renderTimer.start(1000);

    _renderProcess = new QProcess(this);
    connect(_renderProcess, &QProcess::readyReadStandardOutput,
            this, &CallGraphView::readDotOutput);
    connect(_renderProcess, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(dotError()));
    connect(_renderProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(dotExited()));

    _renderProcessCmdLine =  renderProgram + QLatin1Char(' ') + renderArgs.join(QLatin1Char(' '));
    qDebug("CallGraphView::refresh: Starting process %p, '%s'",
           _renderProcess, qPrintable(_renderProcessCmdLine));

    // _renderProcess can be set to 0 on error after start().
    // thus, we use a local copy afterwards
    QProcess* p = _renderProcess;
    p->start(renderProgram, renderArgs);
    _exporter.reset(_data, _activeItem, _eventType, _groupType);
    _exporter.writeDot(p);
    p->closeWriteChannel();
}

void CallGraphView::readDotOutput()
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("CallGraphView::readDotOutput: QProcess %p", p);

    // signal from old/uninteresting process?
    if ((_renderProcess == nullptr) || (p != _renderProcess)) {
        p->deleteLater();
        return;
    }

    _unparsedOutput.append(QString::fromLocal8Bit(_renderProcess->readAllStandardOutput()));
}

void CallGraphView::dotError()
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("CallGraphView::dotError: Got %d from QProcess %p",
           p->error(), p);

    // signal from old/uninteresting process?
    if ((_renderProcess == nullptr) || (p != _renderProcess)) {
        p->deleteLater();
        return;
    }

    showRenderError(QString::fromLocal8Bit(_renderProcess->readAllStandardError()));

    // not interesting any longer
    _renderProcess->deleteLater();
    _renderProcess = nullptr;
}


void CallGraphView::dotExited()
{
    QProcess* p = qobject_cast<QProcess*>(sender());
    qDebug("CallGraphView::dotExited: QProcess %p", p);

    // signal from old/uninteresting process?
    if ((_renderProcess == nullptr) || (p != _renderProcess)) {
        p->deleteLater();
        return;
    }

    _unparsedOutput.append(QString::fromLocal8Bit(_renderProcess->readAllStandardOutput()));
    _renderProcess->deleteLater();
    _renderProcess = nullptr;

    QString line, cmd;
    CanvasNode *rItem;
    QGraphicsEllipseItem* eItem;
    CanvasEdge* sItem;
    CanvasEdgeLabel* lItem;
    QTextStream* dotStream;
    double scale = 1.0, scaleX = 1.0, scaleY = 1.0;
    double dotWidth = 0, dotHeight = 0;
    GraphNode* activeNode = nullptr;
    GraphEdge* activeEdge = nullptr;

    _renderTimer.stop();
    viewport()->setUpdatesEnabled(false);
    clear();
    dotStream = new QTextStream(&_unparsedOutput, QIODevice::ReadOnly);

    // First pass to adjust coordinate scaling by node height given from dot
    // Normal detail level (=1) should be 3 lines using general KDE font
    double nodeHeight = 0.0;
    while(1) {
        line = dotStream->readLine();
        if (line.isNull()) break;
        if (line.isEmpty()) continue;
        QTextStream lineStream(&line, QIODevice::ReadOnly);
        lineStream >> cmd;
        if (cmd != QLatin1String("node")) continue;
        QString s, h;
        lineStream >> s /*name*/ >> s /*x*/ >> s /*y*/ >> s /*width*/ >> h /*height*/;
        nodeHeight = h.toDouble();
        break;
    }
    if (nodeHeight > 0.0) {
        scaleY = (8 + (1 + 2 * _detailLevel) * fontMetrics().height()) / nodeHeight;
        scaleX = 80;
    }
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
                   qPrintable(_exporter.filename()),
                   lineno, qPrintable(line), qPrintable(cmd));

        if (cmd == QLatin1String("stop"))
            break;

        if (cmd == QLatin1String("graph")) {
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
                if (w < QApplication::primaryScreen()->size().width())
                    _xMargin += (QApplication::primaryScreen()->size().width()-w)/2;

                _yMargin = 50;
                if (h < QApplication::primaryScreen()->size().height())
                    _yMargin += (QApplication::primaryScreen()->size().height()-h)/2;

                _scene = new QGraphicsScene( 0.0, 0.0,
                                             qreal(w+2*_xMargin), qreal(h+2*_yMargin));
                // Change background color for call graph from default system color to
                // white. It has to blend into the gradient for the selected function.
                _scene->setBackgroundBrush(Qt::white);

#if DEBUG_GRAPH
                qDebug() << qPrintable(_exporter.filename()) << ":" << lineno
                         << " - graph (" << dotWidth << " x " << dotHeight
                         << ") => (" << w << " x " << h << ")";
#endif
            } else
                qDebug() << "Ignoring 2nd 'graph' from dot ("
                         << _exporter.filename() << ":"<< lineno << ")";
            continue;
        }

        if ((cmd != QLatin1String("node")) && (cmd != QLatin1String("edge"))) {
            qDebug() << "Ignoring unknown command '"<< cmd
                     << "' from dot ("<< _exporter.filename() << ":"<< lineno
                     << ")";
            continue;
        }

        if (_scene == nullptr) {
            qDebug() << "Ignoring '"<< cmd
                     << "' without 'graph' from dot ("<< _exporter.filename()
                     << ":"<< lineno << ")";
            continue;
        }

        if (cmd == QLatin1String("node")) {
            // x, y are centered in node
            QString nodeName, nodeX, nodeY, nodeWidth, nodeHeight;
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
            qDebug() << _exporter.filename() << ":" << lineno
                     << " - node '" << nodeName << "' ( "
                     << x << "/" << y << " - "
                     << width << "x" << height << " ) => ("
                     << xx-w/2 << "/" << yy-h/2 << " - "
                     << w << "x" << h << ")" << endl;
#endif

            // Unnamed nodes with collapsed edges (with 'R' and 'S')
            if (nodeName[0] == 'R'|| nodeName[0] == 'S') {
                w = 10, h = 10;
                eItem = new QGraphicsEllipseItem( QRectF(xx-w/2, yy-h/2, w, h) );
                _scene->addItem(eItem);
                eItem->setBrush(Qt::gray);
                eItem->setZValue(1.0);
                eItem->show();
                continue;
            }

            if (!n) {
                qDebug("Warning: Unknown function '%s' ?!",
                       qPrintable(nodeName));
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
        QPolygon poly;
        int points, i;
        lineStream >> node1Name >> node2Name >> points;

        GraphEdge* e = _exporter.edge(_exporter.toFunc(node1Name),
                                      _exporter.toFunc(node2Name));
        if (!e) {
            qDebug() << "Unknown edge '"<< node1Name << "'-'"<< node2Name
                     << "' from dot ("<< _exporter.filename() << ":"<< lineno
                     << ")";
            continue;
        }
        e->setVisible(true);
        if (e->fromNode())
            e->fromNode()->addCallee(e);
        if (e->toNode())
            e->toNode()->addCaller(e);

        if (0)
            qDebug("  Edge with %d points:", points);

        poly.resize(points);
        for (i=0; i<points; ++i) {
            if (lineStream.atEnd())
                break;
            lineStream >> edgeX >> edgeY;
            x = edgeX.toDouble();
            y = edgeY.toDouble();

            int xx = (int)(scaleX * x + _xMargin);
            int yy = (int)(scaleY * (dotHeight - y)+ _yMargin);

            if (0)
                qDebug("   P %d: ( %f / %f ) => ( %d / %d)", i, x, y, xx, yy);

            poly.setPoint(i, xx, yy);
        }
        if (i < points) {
            qDebug("CallGraphView: Can not read %d spline points (%s:%d)",
                   points, qPrintable(_exporter.filename()), lineno);
            continue;
        }

        // calls into/out of cycles are special: make them blue
        QColor arrowColor = Qt::black;
        TraceFunction* caller = e->fromNode() ? e->fromNode()->function() : nullptr;
        TraceFunction* called = e->toNode() ? e->toNode()->function() : nullptr;
        if ( (caller && (caller->cycle() == caller)) ||
             (called && (called->cycle() == called)) ) arrowColor = Qt::blue;

        sItem = new CanvasEdge(e);
        _scene->addItem(sItem);
        e->setCanvasEdge(sItem);
        sItem->setControlPoints(poly);
        // width of pen will be adjusted in CanvasEdge::paint()
        sItem->setPen(QPen(arrowColor));
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
        CanvasNode* fromNode = e->fromNode() ? e->fromNode()->canvasNode() : nullptr;
        if (fromNode) {
            QPointF toCenter = fromNode->rect().center();
            qreal dx0 = poly.point(0).x() - toCenter.x();
            qreal dy0 = poly.point(0).y() - toCenter.y();
            qreal dx1 = poly.point(points-1).x() - toCenter.x();
            qreal dy1 = poly.point(points-1).y() - toCenter.y();
            if (dx0*dx0+dy0*dy0 > dx1*dx1+dy1*dy1) {
                // start of spline is nearer to call target node
                indexHead=-1;
                while (arrowDir.isNull() && (indexHead<points-2)) {
                    indexHead++;
                    arrowDir = poly.point(indexHead) - poly.point(indexHead+1);
                }
            }
        }

        if (arrowDir.isNull()) {
            indexHead = points;
            // sometimes the last spline points from dot are the same...
            while (arrowDir.isNull() && (indexHead>1)) {
                indexHead--;
                arrowDir = poly.point(indexHead) - poly.point(indexHead-1);
            }
        }

        if (!arrowDir.isNull()) {
            // arrow around pa.point(indexHead) with direction arrowDir
            arrowDir *= 10.0/sqrt(double(arrowDir.x()*arrowDir.x() +
                                         arrowDir.y()*arrowDir.y()));

            QPolygonF a;
            a << QPointF(poly.point(indexHead) + arrowDir);
            a << QPointF(poly.point(indexHead) + QPoint(arrowDir.y()/2,
                                                        -arrowDir.x()/2));
            a << QPointF(poly.point(indexHead) + QPoint(-arrowDir.y()/2,
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
            qDebug("   Label '%s': ( %f / %f ) => ( %d / %d)",
                   qPrintable(label), x, y, xx, yy);

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

        QString s = tr("Error running the graph layouting tool.\n");
        s += tr("Please check that 'dot' is installed (package GraphViz).");
        _scene->addSimpleText(s);
        centerOn(0, 0);
    } else if (!activeNode && !activeEdge) {
        QString s = tr("There is no call graph available for function\n"
                       "\t'%1'\n"
                       "because it has no cost of the selected event type.")
                    .arg(_activeItem->name());
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

    CanvasNode* sNode = nullptr;
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
    _renderProcess = nullptr;
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

// Handles zooming in and out with 'ctrl + mouse-wheel-up/down'
void CallGraphView::wheelEvent(QWheelEvent* e) 
{
    if (e->modifiers() & Qt::ControlModifier)
    {
        int angle = e->angleDelta().y();
        if ((_zoomLevel <= 0.5 && angle < 0) || (_zoomLevel >= 1.3 && angle > 0))
            return;

        const ViewportAnchor anchor = transformationAnchor();
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        qreal factor;
        if (angle > 0)
            factor = 1.1;
        else
            factor = 0.9;

        scale(factor, factor);
        _zoomLevel = transform().m11();
        setTransformationAnchor(anchor);
        return;
    }
    // Don't eat the event if we didn't hold 'ctrl'
    QGraphicsView::wheelEvent(e);
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
                qDebug("CallGraphView: Got Node '%s'",
                       qPrintable(n->function()->prettyName()));

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
                qDebug("CallGraphView: Got Edge '%s'",
                       qPrintable(e->prettyName()));

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
    if (i == nullptr)
        return;

    if (i->type() == CANVAS_NODE) {
        GraphNode* n = ((CanvasNode*)i)->node();
        if (0)
            qDebug("CallGraphView: Double Clicked on Node '%s'",
                   qPrintable(n->function()->prettyName()));

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
                qDebug("CallGraphView: Double Clicked On Edge '%s'",
                       qPrintable(e->call()->prettyName()));

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

QMenu* CallGraphView::addCallerDepthMenu(QMenu* menu)
{
    QAction* a;
    QMenu* m;

    m = menu->addMenu(tr("Caller Depth"));
    a = addCallerDepthAction(m, tr("Unlimited"), -1);
    a->setEnabled(_funcLimit>0.005);
    m->addSeparator();
    addCallerDepthAction(m, tr("Depth 0", "None"), 0);
    addCallerDepthAction(m, tr("max. 1"), 1);
    addCallerDepthAction(m, tr("max. 2"), 2);
    addCallerDepthAction(m, tr("max. 5"), 5);
    addCallerDepthAction(m, tr("max. 10"), 10);
    addCallerDepthAction(m, tr("max. 15"), 15);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::callerDepthTriggered );

    return m;
}

void CallGraphView::callerDepthTriggered(QAction* a)
{
    _maxCallerDepth = a->data().toInt(nullptr);
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

QMenu* CallGraphView::addCalleeDepthMenu(QMenu* menu)
{
    QAction* a;
    QMenu* m;

    m = menu->addMenu(tr("Callee Depth"));
    a = addCalleeDepthAction(m, tr("Unlimited"), -1);
    a->setEnabled(_funcLimit>0.005);
    m->addSeparator();
    addCalleeDepthAction(m, tr("Depth 0", "None"), 0);
    addCalleeDepthAction(m, tr("max. 1"), 1);
    addCalleeDepthAction(m, tr("max. 2"), 2);
    addCalleeDepthAction(m, tr("max. 5"), 5);
    addCalleeDepthAction(m, tr("max. 10"), 10);
    addCalleeDepthAction(m, tr("max. 15"), 15);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::calleeDepthTriggered );

    return m;
}

void CallGraphView::calleeDepthTriggered(QAction* a)
{
    _maxCalleeDepth = a->data().toInt(nullptr);
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

QMenu* CallGraphView::addNodeLimitMenu(QMenu* menu)
{
    QAction* a;
    QMenu* m;

    m = menu->addMenu(tr("Min. Node Cost"));
    a = addNodeLimitAction(m, tr("No Minimum"), 0.0);
    // Unlimited node cost easily produces huge graphs such that 'dot'
    // would need a long time to layout. For responsiveness, we only allow
    // for unlimited node cost if a caller and callee depth limit is set.
    a->setEnabled((_maxCallerDepth>=0) && (_maxCalleeDepth>=0));
    m->addSeparator();
    addNodeLimitAction(m, tr("50 %"), .5);
    addNodeLimitAction(m, tr("20 %"), .2);
    addNodeLimitAction(m, tr("10 %"), .1);
    addNodeLimitAction(m, tr("5 %"), .05);
    addNodeLimitAction(m, tr("2 %"), .02);
    addNodeLimitAction(m, tr("1 %"), .01);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::nodeLimitTriggered );

    return m;
}

void CallGraphView::nodeLimitTriggered(QAction* a)
{
    _funcLimit = a->data().toDouble(nullptr);
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

QMenu* CallGraphView::addCallLimitMenu(QMenu* menu)
{
    QMenu* m;

    m = menu->addMenu(tr("Min. Call Cost"));
    addCallLimitAction(m, tr("Same as Node"), 1.0);
    // xgettext: no-c-format
    addCallLimitAction(m, tr("50 % of Node"), .5);
    // xgettext: no-c-format
    addCallLimitAction(m, tr("20 % of Node"), .2);
    // xgettext: no-c-format
    addCallLimitAction(m, tr("10 % of Node"), .1);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::callLimitTriggered );

    return m;
}

void CallGraphView::callLimitTriggered(QAction* a)
{
    _callLimit = a->data().toDouble(nullptr);
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

QMenu* CallGraphView::addZoomPosMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(tr("Birds-eye View"));
    addZoomPosAction(m, tr("Top Left"), TopLeft);
    addZoomPosAction(m, tr("Top Right"), TopRight);
    addZoomPosAction(m, tr("Bottom Left"), BottomLeft);
    addZoomPosAction(m, tr("Bottom Right"), BottomRight);
    addZoomPosAction(m, tr("Automatic"), Auto);
    addZoomPosAction(m, tr("Hide"), Hide);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::zoomPosTriggered );

    return m;
}


void CallGraphView::zoomPosTriggered(QAction* a)
{
    _zoomPosition = (ZoomPosition) a->data().toInt(nullptr);
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

QMenu* CallGraphView::addLayoutMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(tr("Layout"));
    addLayoutAction(m, tr("Top to Down"), TopDown);
    addLayoutAction(m, tr("Left to Right"), LeftRight);
    addLayoutAction(m, tr("Circular"), Circular);

    connect(m, &QMenu::triggered,
            this, &CallGraphView::layoutTriggered );

    return m;
}


void CallGraphView::layoutTriggered(QAction* a)
{
    _layout = (Layout) a->data().toInt(nullptr);
    refresh();
}


void CallGraphView::contextMenuEvent(QContextMenuEvent* e)
{
    _isMoving = false;

    QGraphicsItem* i = itemAt(e->pos());

    QMenu popup;
    TraceFunction *f = nullptr, *cycle = nullptr;
    TraceCall* c = nullptr;

    QAction* activateFunction = nullptr;
    QAction* activateCycle = nullptr;
    QAction* activateCall = nullptr;
    if (i) {
        if (i->type() == CANVAS_NODE) {
            GraphNode* n = ((CanvasNode*)i)->node();
            if (0)
                qDebug("CallGraphView: Menu on Node '%s'",
                       qPrintable(n->function()->prettyName()));

            f = n->function();
            cycle = f->cycle();

            QString name = f->prettyName();
            QString menuStr = tr("Go to '%1'")
                              .arg(GlobalConfig::shortenSymbol(name));
            activateFunction = popup.addAction(menuStr);
            if (cycle && (cycle != f)) {
                name = GlobalConfig::shortenSymbol(cycle->prettyName());
                activateCycle = popup.addAction(tr("Go to '%1'").arg(name));
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
                qDebug("CallGraphView: Menu on Edge '%s'",
                       qPrintable(e->prettyName()));

            c = e->call();
            if (c) {
                QString name = c->prettyName();
                QString menuStr = tr("Go to '%1'")
                                  .arg(GlobalConfig::shortenSymbol(name));
                activateCall = popup.addAction(menuStr);

                popup.addSeparator();
            }
        }
    }

    QAction* stopLayout = nullptr;
    if (_renderProcess) {
        stopLayout = popup.addAction(tr("Stop Layouting"));
        popup.addSeparator();
    }

    addGoMenu(&popup);
    popup.addSeparator();

    QMenu* epopup = popup.addMenu(tr("Export Graph"));
    QAction* exportAsDot = epopup->addAction(tr("As DOT file..."));
    QAction* exportAsImage = epopup->addAction(tr("As Image..."));
    popup.addSeparator();

    QMenu* gpopup = popup.addMenu(tr("Graph"));
    addCallerDepthMenu(gpopup);
    addCalleeDepthMenu(gpopup);
    addNodeLimitMenu(gpopup);
    addCallLimitMenu(gpopup);
    gpopup->addSeparator();

    QAction* toggleSkipped;
    toggleSkipped = gpopup->addAction(tr("Arrows for Skipped Calls"));
    toggleSkipped->setCheckable(true);
    toggleSkipped->setChecked(_showSkipped);

    QAction* toggleExpand;
    toggleExpand = gpopup->addAction(tr("Inner-cycle Calls"));
    toggleExpand->setCheckable(true);
    toggleExpand->setChecked(_expandCycles);

    QAction* toggleCluster;
    toggleCluster = gpopup->addAction(tr("Cluster Groups"));
    toggleCluster->setCheckable(true);
    toggleCluster->setChecked(_clusterGroups);

    QMenu* vpopup = popup.addMenu(tr("Visualization"));
    QAction* layoutCompact = vpopup->addAction(tr("Compact"));
    layoutCompact->setCheckable(true);
    layoutCompact->setChecked(_detailLevel == 0);
    QAction* layoutNormal = vpopup->addAction(tr("Normal"));
    layoutNormal->setCheckable(true);
    layoutNormal->setChecked(_detailLevel == 1);
    QAction* layoutTall = vpopup->addAction(tr("Tall"));
    layoutTall->setCheckable(true);
    layoutTall->setChecked(_detailLevel == 2);

    addLayoutMenu(&popup);
    addZoomPosMenu(&popup);

    QAction* a = popup.exec(e->globalPos());

    if (a == activateFunction)
        activated(f);
    else if (a == activateCycle)
        activated(cycle);
    else if (a == activateCall)
        activated(c);

    else if (a == stopLayout)
        stopRendering();

    else if (a == exportAsDot) {
        TraceFunction* f = activeFunction();
        if (!f) return;

        GraphExporter::savePrompt(this, TraceItemView::data(), f, eventType(),
                                  groupType(), this);
    }
    else if (a == exportAsImage) {
        // write current content of canvas as image to file
        if (!_scene) return;

        QString n;
        n = QFileDialog::getSaveFileName(this,
                                         tr("Export Graph As Image"),
                                         QString(),
                                         tr("Images (*.png *.jpg)"));

        if (!n.isEmpty()) {
            QRect r = _scene->sceneRect().toRect();
            QPixmap pix(r.width(), r.height());
            QPainter p(&pix);
            _scene->render( &p );
            pix.save(n);
        }
    }

    else if (a == toggleSkipped) {
        _showSkipped = !_showSkipped;
        refresh();
    }
    else if (a == toggleExpand) {
        _expandCycles = !_expandCycles;
        refresh();
    }
    else if (a == toggleCluster) {
        _clusterGroups = !_clusterGroups;
        refresh();
    }

    else if (a == layoutCompact) {
        _detailLevel = 0;
        refresh();
    }
    else if (a == layoutNormal) {
        _detailLevel = 1;
        refresh();
    }
    else if (a == layoutTall) {
        _detailLevel = 2;
        refresh();
    }
}


CallGraphView::ZoomPosition CallGraphView::zoomPos(QString s)
{
    if (s == QLatin1String("TopLeft"))
        return TopLeft;
    if (s == QLatin1String("TopRight"))
        return TopRight;
    if (s == QLatin1String("BottomLeft"))
        return BottomLeft;
    if (s == QLatin1String("BottomRight"))
        return BottomRight;
    if (s == QLatin1String("Automatic"))
        return Auto;
    if (s == QLatin1String("Hide"))
        return Hide;

    return DEFAULT_ZOOMPOS;
}

QString CallGraphView::zoomPosString(ZoomPosition p)
{
    if (p == TopLeft)
        return QStringLiteral("TopLeft");
    if (p == TopRight)
        return QStringLiteral("TopRight");
    if (p == BottomLeft)
        return QStringLiteral("BottomLeft");
    if (p == BottomRight)
        return QStringLiteral("BottomRight");
    if (p == Auto)
        return QStringLiteral("Automatic");
    if (p == Hide)
        return QStringLiteral("Hide");

    return QString();
}

void CallGraphView::restoreOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    _maxCallerDepth = g->value(QStringLiteral("MaxCaller"), DEFAULT_MAXCALLER).toInt();
    _maxCalleeDepth = g->value(QStringLiteral("MaxCallee"), DEFAULT_MAXCALLEE).toInt();
    _funcLimit = g->value(QStringLiteral("FuncLimit"), DEFAULT_FUNCLIMIT).toDouble();
    _callLimit = g->value(QStringLiteral("CallLimit"), DEFAULT_CALLLIMIT).toDouble();
    _showSkipped = g->value(QStringLiteral("ShowSkipped"), DEFAULT_SHOWSKIPPED).toBool();
    _expandCycles = g->value(QStringLiteral("ExpandCycles"), DEFAULT_EXPANDCYCLES).toBool();
    _clusterGroups = g->value(QStringLiteral("ClusterGroups"), DEFAULT_CLUSTERGROUPS).toBool();
    _detailLevel = g->value(QStringLiteral("DetailLevel"), DEFAULT_DETAILLEVEL).toInt();
    _layout = GraphOptions::layout(g->value(QStringLiteral("Layout"),
                                            layoutString(DEFAULT_LAYOUT)).toString());
    _zoomPosition = zoomPos(g->value(QStringLiteral("ZoomPosition"),
                                     zoomPosString(DEFAULT_ZOOMPOS)).toString());

    delete g;
}

void CallGraphView::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue(QStringLiteral("MaxCaller"), _maxCallerDepth, DEFAULT_MAXCALLER);
    g->setValue(QStringLiteral("MaxCallee"), _maxCalleeDepth, DEFAULT_MAXCALLEE);
    g->setValue(QStringLiteral("FuncLimit"), _funcLimit, DEFAULT_FUNCLIMIT);
    g->setValue(QStringLiteral("CallLimit"), _callLimit, DEFAULT_CALLLIMIT);
    g->setValue(QStringLiteral("ShowSkipped"), _showSkipped, DEFAULT_SHOWSKIPPED);
    g->setValue(QStringLiteral("ExpandCycles"), _expandCycles, DEFAULT_EXPANDCYCLES);
    g->setValue(QStringLiteral("ClusterGroups"), _clusterGroups, DEFAULT_CLUSTERGROUPS);
    g->setValue(QStringLiteral("DetailLevel"), _detailLevel, DEFAULT_DETAILLEVEL);
    g->setValue(QStringLiteral("Layout"), layoutString(_layout), layoutString(DEFAULT_LAYOUT));
    g->setValue(QStringLiteral("ZoomPosition"), zoomPosString(_zoomPosition),
                zoomPosString(DEFAULT_ZOOMPOS));

    delete g;
}

#include "moc_callgraphview.cpp"
