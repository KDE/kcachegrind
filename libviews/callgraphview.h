/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Callgraph View
 */

#ifndef CALLGRAPHVIEW_H
#define CALLGRAPHVIEW_H

#include <qwidget.h>
#include <qmap.h>
#include <qtimer.h>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsPathItem>
#include <QPixmap>
#include <QFocusEvent>
#include <QPolygon>
#include <QList>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>

#include "treemap.h" // for DrawParams
#include "tracedata.h"
#include "traceitemview.h"

class QProcess;
class QTemporaryFile;
class QIODevice;

class CanvasNode;
class CanvasEdge;
class GraphEdge;
class CallGraphView;


// temporary parts of call graph to be shown
class GraphNode
{
public:
    GraphNode();

    TraceFunction* function() const
    {
        return _f;
    }

    void setFunction(TraceFunction* f)
    {
        _f = f;
    }

    CanvasNode* canvasNode() const
    {
        return _cn;
    }

    void setCanvasNode(CanvasNode* cn)
    {
        _cn = cn;
    }

    bool isVisible() const
    {
        return _visible;
    }

    void setVisible(bool v)
    {
        _visible = v;
    }

    void clearEdges();
    void sortEdges();
    void addCallee(GraphEdge*);
    void addCaller(GraphEdge*);
    void addUniqueCallee(GraphEdge*);
    void addUniqueCaller(GraphEdge*);
    void removeEdge(GraphEdge*);
    double calleeCostSum();
    double calleeCountSum();
    double callerCostSum();
    double callerCountSum();

    // keyboard navigation
    TraceCall* visibleCaller();
    TraceCall* visibleCallee();
    void setCallee(GraphEdge*);
    void setCaller(GraphEdge*);
    TraceFunction* nextVisible();
    TraceFunction* priorVisible();
    TraceCall* nextVisibleCaller(GraphEdge* = nullptr);
    TraceCall* nextVisibleCallee(GraphEdge* = nullptr);
    TraceCall* priorVisibleCaller(GraphEdge* = nullptr);
    TraceCall* priorVisibleCallee(GraphEdge* = nullptr);

    double self, incl;

private:
    TraceFunction* _f;
    CanvasNode* _cn;
    bool _visible;

    QList<GraphEdge*> callers, callees;

    // for keyboard navigation
    int _lastCallerIndex, _lastCalleeIndex;
    bool _lastFromCaller;
};


class GraphEdge
{
public:
    GraphEdge();

    CanvasEdge* canvasEdge() const
    {
        return _ce;
    }

    void setCanvasEdge(CanvasEdge* ce)
    {
        _ce = ce;
    }

    TraceCall* call() const
    {
        return _c;
    }

    void setCall(TraceCall* c)
    {
        _c = c;
    }

    bool isVisible() const
    {
        return _visible;
    }

    void setVisible(bool v)
    {
        _visible = v;
    }

    GraphNode* fromNode() const
    {
        return _fromNode;
    }

    GraphNode* toNode() const
    {
        return _toNode;
    }

    TraceFunction* from() const
    {
        return _from;
    }

    TraceFunction* to() const
    {
        return _to;
    }

    // has special cases for collapsed edges
    QString prettyName();

    void setCaller(TraceFunction* f)
    {
        _from = f;
    }

    void setCallee(TraceFunction* f)
    {
        _to = f;
    }

    void setCallerNode(GraphNode* n)
    {
        _fromNode = n;
    }

    void setCalleeNode(GraphNode* n)
    {
        _toNode = n;
    }

    // keyboard navigation
    TraceFunction* visibleCaller();
    TraceFunction* visibleCallee();
    TraceCall* nextVisible();
    TraceCall* priorVisible();

    double cost, count;

private:
    // we have a _c *and* _from/_to because for collapsed edges,
    // only _to or _from will be unequal nullptr
    TraceCall* _c;
    TraceFunction * _from, * _to;
    GraphNode *_fromNode, *_toNode;
    CanvasEdge* _ce;
    bool _visible;
    // for keyboard navigation: have we last reached this edge via a caller?
    bool _lastFromCaller;
};


typedef QMap<TraceFunction*, GraphNode> GraphNodeMap;
typedef QMap<QPair<TraceFunction*, TraceFunction*>, GraphEdge> GraphEdgeMap;


/* Abstract Interface for graph options */
class GraphOptions
{
public:
    enum Layout {TopDown, LeftRight, Circular};
    virtual ~GraphOptions() {}
    virtual double funcLimit() = 0;
    virtual double callLimit() = 0;
    virtual int maxCallerDepth() = 0;
    virtual int maxCalleeDepth() = 0;
    virtual bool showSkipped() = 0;
    virtual bool expandCycles() = 0;
    virtual bool clusterGroups() = 0;
    virtual int detailLevel() = 0;
    virtual Layout layout() = 0;

    static QString layoutString(Layout);
    static Layout layout(QString);
};

/* Graph Options Storage */
class StorableGraphOptions: public GraphOptions
{
public:
    StorableGraphOptions();
    ~StorableGraphOptions() override{}
    // implementation of getters
    double funcLimit() override { return _funcLimit; }
    double callLimit() override { return _callLimit; }
    int maxCallerDepth() override { return _maxCallerDepth; }
    int maxCalleeDepth() override { return _maxCalleeDepth; }
    bool showSkipped() override { return _showSkipped; }
    bool expandCycles() override { return _expandCycles; }
    bool clusterGroups() override { return _clusterGroups; }
    int detailLevel() override { return _detailLevel; }
    Layout layout() override { return _layout; }

    // setters
    void setMaxCallerDepth(int d) { _maxCallerDepth = d; }
    void setMaxCalleeDepth(int d) { _maxCalleeDepth = d; }
    void setFuncLimit(double l) { _funcLimit = l; }
    void setCallLimit(double l) { _callLimit = l; }
    void setShowSkipped(bool b) { _showSkipped = b; }
    void setExpandCycles(bool b) { _expandCycles = b; }
    void setClusterGroups(bool b) { _clusterGroups = b; }
    void setDetailLevel(int l) { _detailLevel = l; }
    void setLayout(Layout l) { _layout = l; }

protected:
    double _funcLimit, _callLimit;
    int _maxCallerDepth, _maxCalleeDepth;
    bool _showSkipped, _expandCycles, _clusterGroups;
    int _detailLevel;
    Layout _layout;
};

/**
 * GraphExporter
 *
 * Generates a graph file for "dot"
 * Create an instance and
 */
class GraphExporter : public StorableGraphOptions
{
public:
    GraphExporter();
    GraphExporter(TraceData*, TraceFunction*, EventType*,
                  ProfileContext::Type,
                  QString filename = QString());
    ~GraphExporter() override;

    void reset(TraceData*, CostItem*, EventType*,
               ProfileContext::Type,
               QString filename = QString());

    QString filename()
    {
        return _dotName;
    }

    int edgeCount()
    {
        return _edgeMap.count();
    }

    int nodeCount()
    {
        return _nodeMap.count();
    }

    // Set the object from which to get graph options for creation.
    // Default is this object itself (supply 0 for default)
    void setGraphOptions(GraphOptions* go = nullptr);

    // Create a subgraph with given limits/maxDepths
    void createGraph();

    // calls createGraph before dumping of not already created
    bool writeDot(QIODevice* = nullptr);

    // ephemereal save dialog and exporter
    static bool savePrompt(QWidget *, TraceData*, TraceFunction*,
                           EventType*, ProfileContext::Type,
                           CallGraphView*);

    // to map back to structures when parsing a layouted graph

    /* <toFunc> is a helper for node() and edge().
     * Do not use the returned pointer directly, but only with
     * node() or edge(), because it could be a dangling pointer.
     */
    TraceFunction* toFunc(QString);
    GraphNode* node(TraceFunction*);
    GraphEdge* edge(TraceFunction*, TraceFunction*);

    /* After CanvasEdges are attached to GraphEdges, we can
     * sort the incoming and outgoing edges of all nodes
     * regarding start/end points for keyboard navigation
     */
    void sortEdges();

private:
    void buildGraph(TraceFunction*, int, bool, double);

    QString _dotName;
    CostItem* _item;
    EventType* _eventType;
    ProfileContext::Type _groupType;
    QTemporaryFile* _tmpFile;
    double _realFuncLimit, _realCallLimit;
    bool _graphCreated;

    GraphOptions* _go;

    // optional graph attributes
    bool _useBox;

    // graph parts written to file
    GraphNodeMap _nodeMap;
    GraphEdgeMap _edgeMap;
};


/**
 * A panner laid over a QGraphicsScene
 */
class PanningView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit PanningView(QWidget * parent = nullptr);

    void setZoomRect(const QRectF& r);

Q_SIGNALS:
    void zoomRectMoved(qreal dx, qreal dy);
    void zoomRectMoveFinished();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void drawForeground(QPainter * p, const QRectF&) override;

    QRectF _zoomRect;
    bool _movingZoomRect;
    QPointF _lastPos;
};


/*
 * Canvas Items:
 * - CanvasNode       (Rectangular Area)
 * - CanvasEdge       (Spline curve)
 * - CanvasEdgeLabel  (Label for edges)
 * - CanvasEdgeArrow  (Arrows at the end of the edge spline)
 * - CanvasFrame      (Grey background blending to show active node)
 */

enum {
    CANVAS_NODE = 1122,
    CANVAS_EDGE, CANVAS_EDGELABEL, CANVAS_EDGEARROW,
    CANVAS_FRAME
};

class CanvasNode : public QGraphicsRectItem, public StoredDrawParams
{
public:
    CanvasNode(CallGraphView*, GraphNode*, int, int, int, int);

    void updateGroup();
    void setSelected(bool);
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    GraphNode* node()
    {
        return _node;
    }

    int type() const override
    {
        return CANVAS_NODE;
    }

private:
    GraphNode* _node;
    CallGraphView* _view;
};


class CanvasEdgeLabel : public QGraphicsRectItem, public StoredDrawParams
{
public:
    CanvasEdgeLabel(CallGraphView*, CanvasEdge*, int, int, int, int);

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    CanvasEdge* canvasEdge()
    {
        return _ce;
    }

    int type() const override
    {
        return CANVAS_EDGELABEL;
    }

    double percentage() const
    {
        return _percentage;
    }

private:
    CanvasEdge* _ce;
    CallGraphView* _view;

    double _percentage;
};


class CanvasEdgeArrow : public QGraphicsPolygonItem
{
public:
    explicit CanvasEdgeArrow(CanvasEdge*);

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    CanvasEdge* canvasEdge()
    {
        return _ce;
    }

    int type() const override
    {
        return CANVAS_EDGEARROW;
    }

private:
    CanvasEdge* _ce;
};


class CanvasEdge : public QGraphicsPathItem
{
public:
    explicit CanvasEdge(GraphEdge*);

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

    void setSelected(bool);

    CanvasEdgeLabel* label()
    {
        return _label;
    }

    void setLabel(CanvasEdgeLabel* l);

    CanvasEdgeArrow* arrow()
    {
        return _arrow;
    }

    void setArrow(CanvasEdgeArrow* a);

    const QPolygon& controlPoints() const
    {
        return _points;
    }

    void setControlPoints(const QPolygon& a);

    GraphEdge* edge()
    {
        return _edge;
    }

    int type() const override
    {
        return CANVAS_EDGE;
    }

private:
    GraphEdge* _edge;
    CanvasEdgeLabel* _label;
    CanvasEdgeArrow* _arrow;
    QPolygon _points;

    double _thickness;
};


class CanvasFrame : public QGraphicsRectItem
{
public:
    explicit CanvasFrame(CanvasNode*);

    int type() const override
    {
        return CANVAS_FRAME;
    }

    bool hit(const QPoint&) const
    {
        return false;
    }

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
    static QPixmap* _p;
};


class CallGraphTip;

/**
 * A QGraphicsView showing a part of the call graph
 * and another zoomed out CanvasView in a border acting as
 * a panner to select to visible part (only if needed)
 */
class CallGraphView : public QGraphicsView, public TraceItemView,
        public StorableGraphOptions
{
    Q_OBJECT

public:
    enum ZoomPosition {TopLeft, TopRight, BottomLeft, BottomRight, Auto, Hide};

    explicit CallGraphView(TraceItemView* parentView, QWidget* parent,
                           const QString& name);
    ~CallGraphView() override;

    void restoreOptions(const QString& prefix, const QString& postfix) override;
    void saveOptions(const QString& prefix, const QString& postfix) override;

    QWidget* widget() override
    {
        return this;
    }

    QString whatsThis() const override;

    ZoomPosition zoomPos() const
    {
        return _zoomPosition;
    }

    static ZoomPosition zoomPos(QString);
    static QString zoomPosString(ZoomPosition);

public Q_SLOTS:
    void zoomRectMoved(qreal, qreal);
    void zoomRectMoveFinished();

    void showRenderWarning();
    void showRenderError(QString);
    void stopRendering();
    void readDotOutput();
    void dotError();
    void dotExited();

    // context menu trigger handlers
    void callerDepthTriggered(QAction*);
    void calleeDepthTriggered(QAction*);
    void nodeLimitTriggered(QAction*);
    void callLimitTriggered(QAction*);
    void zoomPosTriggered(QAction*);
    void layoutTriggered(QAction*);

protected:
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void scrollContentsBy(int dx, int dy) override;
	void wheelEvent(QWheelEvent*) override;

private:
    void updateSizes(QSize s = QSize(0,0));
    CostItem* canShow(CostItem*) override;
    void doUpdate(int, bool) override;
    void refresh();
    void makeFrame(CanvasNode*, bool active);
    void clear();
    void showText(QString);

    // context menu builders
    QAction* addCallerDepthAction(QMenu*,QString,int);
    QMenu* addCallerDepthMenu(QMenu*);
    QAction* addCalleeDepthAction(QMenu*,QString,int);
    QMenu* addCalleeDepthMenu(QMenu*);
    QAction* addNodeLimitAction(QMenu*,QString,double);
    QMenu* addNodeLimitMenu(QMenu*);
    QAction* addCallLimitAction(QMenu*,QString,double);
    QMenu* addCallLimitMenu(QMenu*);
    QAction* addZoomPosAction(QMenu*,QString,ZoomPosition);
    QMenu* addZoomPosMenu(QMenu*);
    QAction* addLayoutAction(QMenu*,QString,Layout);
    QMenu* addLayoutMenu(QMenu*);

    QGraphicsScene *_scene;
    int _xMargin, _yMargin;
    PanningView *_panningView;
    double _panningZoom;

    CallGraphTip* _tip;

    bool _isMoving;
    QPoint _lastPos;

    GraphExporter _exporter;

    GraphNode* _selectedNode;
    GraphEdge* _selectedEdge;

    qreal _zoomLevel = 1;

    // widget options
    ZoomPosition _zoomPosition, _lastAutoPosition;

    // background rendering
    QProcess* _renderProcess;
    QString _renderProcessCmdLine;
    QTimer _renderTimer;
    GraphNode* _prevSelectedNode;
    QPoint _prevSelectedPos;
    QString _unparsedOutput;
};


#endif
