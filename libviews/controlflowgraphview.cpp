/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2024 Alexander Dolgov <dolgov04@list.ru>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <cmath>
#include <array>
#include <cstring>
#include <map>

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QRectF>
#include <QRect>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QSize>
#include <QTransform>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QApplication>
#include <QScreen>

#include "controlflowgraphview.h"
#include "globalconfig.h"
#include "globalguiconfig.h"

// ======================================================================================

//
// CFGNode
//

CFGNode::CFGNode(TraceBasicBlock* bb, uint64 cost) : _bb{bb}, _cost{cost} {}

void CFGNode::addOutgoingEdge(CFGEdge* edge)
{
    if (edge)
        _outgoingEdges.push_back(edge);
}

void CFGNode::addIncomingEdge(CFGEdge* edge)
{
    if (edge)
        _incomingEdges.push_back(edge);
}

void CFGNode::clearEdges()
{
    _incomingEdges.clear();
    _outgoingEdges.clear();
}

namespace
{

qreal calcAngle(qreal x, qreal y)
{
    qreal angle = std::atan2(y, x);
    if (angle < 0)
        angle += 2 * M_PI;

    return angle;
}

} // unnamed namespace

class OutgoingEdgesComparator final
{
public:
    OutgoingEdgesComparator(CanvasCFGNode* cn)
    {
        assert(cn);
        const QRectF nodeRect = cn->rect();
        _center.setX(nodeRect.center().x());
        _center.setY(nodeRect.bottom());
    }

    bool operator()(const CFGEdge* ge1, const CFGEdge* ge2)
    {
        const CanvasCFGEdge* ce1 = ge1->canvasEdge();
        const CanvasCFGEdge* ce2 = ge2->canvasEdge();

        if (!ce1 && !ce2)
            return ce1 > ce2;
        else if (!ce1)
            return false;
        else if (!ce2)
            return true;
        else
        {
            const QPointF d1 = ce1->controlPoints().back() - _center;
            const QPointF d2 = ce2->controlPoints().back() - _center;

            return calcAngle(d1.x(), d1.y()) > calcAngle(d2.x(), d2.y());
        }
    }

private:
    QPointF _center;
};

class IncomingEdgesComparator final
{
public:
    IncomingEdgesComparator(CanvasCFGNode* cn)
    {
        assert(cn);
        const QRectF nodeRect = cn->rect();
        _center.setX(nodeRect.center().x());
        _center.setY(nodeRect.top());
    }

    bool operator()(const CFGEdge* ge1, const CFGEdge* ge2)
    {
        const CanvasCFGEdge* ce1 = ge1->canvasEdge();
        const CanvasCFGEdge* ce2 = ge2->canvasEdge();

        if (!ce1 && !ce2)
            return ce1 > ce2;
        else if (!ce1)
            return false;
        else if (!ce2)
            return true;
        else
        {
            const QPointF d1 = ce1->controlPoints().front() - _center;
            const QPointF d2 = ce2->controlPoints().front() - _center;

            return calcAngle(d1.x(), d1.y()) < calcAngle(d2.x(), d2.y());
        }
    };

private:
    QPointF _center;
};

void CFGNode::sortEdges()
{
    if (_outgoingEdges.size() > 1)
        std::sort(_outgoingEdges.begin(), _outgoingEdges.end(), OutgoingEdgesComparator{_cn});

    if (_incomingEdges.size() > 1)
        std::sort(_incomingEdges.begin(), _incomingEdges.end(), IncomingEdgesComparator{_cn});
}

void CFGNode::selectOutgoingEdge(CFGEdge* edge)
{
    _lastOutgoingEdgeIndex = _outgoingEdges.indexOf(edge);
}

void CFGNode::selectIncomingEdge(CFGEdge* edge)
{
    _lastIncomingEdgeIndex = _incomingEdges.indexOf(edge);
}

CFGEdge* CFGNode::keyboardNextEdge()
{
    CFGEdge* edge = _outgoingEdges.value(_lastOutgoingEdgeIndex);

    if (edge && !edge->isVisible())
        edge = nullptr;

    if (edge)
        edge->setVisitedFrom(CFGEdge::NodeType::nodeFrom);
    else if (!_outgoingEdges.isEmpty())
    {
        CFGEdge* maxEdge = _outgoingEdges[0];
        uint64 maxCount = maxEdge->count();

        for (decltype(_outgoingEdges.size()) i = 1; i < _outgoingEdges.size(); ++i)
        {
            edge = _outgoingEdges[i];

            if (edge->isVisible() && edge->count() > maxCount)
            {
                maxEdge = edge;
                maxCount = maxEdge->count();
                _lastOutgoingEdgeIndex = i;
            }
        }

        edge = maxEdge;
        edge->setVisitedFrom(CFGEdge::NodeType::nodeFrom);
    }

    return edge;
}

CFGEdge* CFGNode::keyboardPrevEdge()
{
    CFGEdge* edge = _incomingEdges.value(_lastIncomingEdgeIndex);

    if (edge && !edge->isVisible())
        edge = nullptr;

    if (edge)
        edge->setVisitedFrom(CFGEdge::NodeType::nodeTo);
    else if (!_incomingEdges.isEmpty())
    {
        CFGEdge* maxEdge = _incomingEdges[0];
        uint64 maxCount = maxEdge->count();

        for (decltype(_incomingEdges.size()) i = 1; i < _incomingEdges.size(); ++i)
        {
            edge = _incomingEdges[i];

            if (edge->isVisible() && edge->count() > maxCount)
            {
                maxEdge = edge;
                maxCount = maxEdge->count();
                _lastIncomingEdgeIndex = i;
            }
        }

        edge = maxEdge;
        edge->setVisitedFrom(CFGEdge::NodeType::nodeTo);
    }

    return edge;
}

CFGEdge* CFGNode::nextVisibleOutgoingEdge(CFGEdge* edge)
{
    assert(edge);

    auto begin = std::next(_outgoingEdges.begin(), _outgoingEdges.indexOf(edge) + 1);
    auto end = _outgoingEdges.end();

    auto it = std::find_if(begin, end, [](const CFGEdge* e){ return e->isVisible(); });

    if (it == end)
        return nullptr;
    else
    {
        _lastOutgoingEdgeIndex = std::distance(_outgoingEdges.begin(), it);
        return *it;
    }
}

CFGEdge* CFGNode::nextVisibleIncomingEdge(CFGEdge* edge)
{
    assert(edge);

    auto begin = std::next(_incomingEdges.begin(), _incomingEdges.indexOf(edge) + 1);
    auto end = _incomingEdges.end();

    auto it = std::find_if(begin, end, [](const CFGEdge* e){ return e->isVisible(); });

    if (it == end)
        return nullptr;
    else
    {
        _lastIncomingEdgeIndex = std::distance(_incomingEdges.begin(), it);
        return *it;
    }
}

CFGEdge* CFGNode::priorVisibleOutgoingEdge(CFGEdge* edge)
{
    assert(edge);

    int idx = _outgoingEdges.indexOf(edge);

    idx = (idx < 0) ? _outgoingEdges.size() - 1 : idx - 1;
    for (; idx >= 0; --idx)
    {
        edge = _outgoingEdges[idx];
        if (edge->isVisible())
        {
            _lastOutgoingEdgeIndex = idx;
            return edge;
        }
    }

    return nullptr;
}

CFGEdge* CFGNode::priorVisibleIncomingEdge(CFGEdge* edge)
{
    assert(edge);

    int idx = _incomingEdges.indexOf(edge);

    idx = (idx < 0) ? _incomingEdges.size() - 1 : idx - 1;
    for (; idx >= 0; --idx)
    {
        edge = _incomingEdges[idx];
        if (edge->isVisible())
        {
            _lastIncomingEdgeIndex = idx;
            return edge;
        }
    }

    return nullptr;
}

// ======================================================================================

//
// CFGEdge
//

CFGEdge::CFGEdge(TraceBranch* branch, CFGNode* nodeFrom, CFGNode* nodeTo)
    : _branch{branch}, _count{_branch->executedCount()}, _nodeFrom{nodeFrom}, _nodeTo{nodeTo} {}

CFGNode* CFGEdge::keyboardNextNode()
{
    if (_nodeTo)
        _nodeTo->selectIncomingEdge(this);

    return _nodeTo;
}

CFGNode* CFGEdge::keyboardPrevNode()
{
    if (_nodeFrom)
        _nodeFrom->selectOutgoingEdge(this);

    return _nodeFrom;
}

CFGEdge* CFGEdge::keyboardNextEdge()
{
    switch (_visitedFrom)
    {
        case NodeType::nodeTo:
        {
            assert(_nodeTo);

            CFGEdge* edge = _nodeTo->nextVisibleIncomingEdge(this);
            if (edge)
                edge->setVisitedFrom(NodeType::nodeTo);

            return edge;
        }

        case NodeType::nodeFrom:
        {
            assert(_nodeFrom);

            CFGEdge* edge = _nodeFrom->nextVisibleOutgoingEdge(this);
            if (edge)
                edge->setVisitedFrom(NodeType::nodeFrom);

            return edge;
        }

        default:
            return nullptr;
    }
}

CFGEdge* CFGEdge::keyboardPrevEdge()
{
    switch (_visitedFrom)
    {
        case NodeType::nodeTo:
        {
            assert(_nodeTo);

            CFGEdge* edge = _nodeTo->priorVisibleIncomingEdge(this);
            if (edge)
                edge->setVisitedFrom(NodeType::nodeTo);

            return edge;
        }

        case NodeType::nodeFrom:
        {
            assert(_nodeFrom);

            CFGEdge* edge = _nodeFrom->priorVisibleOutgoingEdge(this);
            if (edge)
                edge->setVisitedFrom(NodeType::nodeFrom);

            return edge;
        }

        default:
            return nullptr;
    }
}

// ======================================================================================

//
// CFGExporter
//

CFGExporter::CFGExporter(const CFGExporter& otherExporter, TraceFunction* func, EventType* et,
                         const QString& filename)
    : _func{func}, _eventType{et}, _layout{otherExporter._layout},
      _graphOptions{otherExporter._graphOptions},
      _globalGraphOptions{otherExporter._globalGraphOptions}
{
    if (!_func)
        return;

    setDumpFile(filename);
}

CFGExporter::~CFGExporter()
{
    delete _tmpFile;
}

CFGExporter::Layout CFGExporter::strToLayout(const QString& s)
{
    if (s == QLatin1String("LeftRight"))
        return Layout::LeftRight;
    return Layout::TopDown;
}

QString CFGExporter::layoutToStr(Layout l)
{
    if (l == Layout::LeftRight)
        return QStringLiteral("LeftRight");
    return QStringLiteral("TopDown");
}

void CFGExporter::setDumpFile(const QString& filename)
{
    if (filename.isEmpty())
    {
        _tmpFile = new QTemporaryFile{};
        _tmpFile->setAutoRemove(false);
        _tmpFile->open();
        _dotName = _tmpFile->fileName();
    }
    else
    {
        _tmpFile = nullptr;
        _dotName = filename;
    }
}

CFGExporter::Options CFGExporter::getNodeOptions(const TraceBasicBlock* bb) const
{
    auto it = _graphOptions.find(bb);
    if (it == _graphOptions.end())
        return Options::invalid;
    else
        return static_cast<Options>(*it);
}

void CFGExporter::setNodeOption(const TraceBasicBlock* bb, Options option)
{
    auto it = _graphOptions.find(bb);
    if (it != _graphOptions.end())
        *it |= option;
}

void CFGExporter::resetNodeOption(const TraceBasicBlock* bb, Options option)
{
    auto it = _graphOptions.find(bb);
    if (it != _graphOptions.end())
        *it &= ~option;
}

void CFGExporter::switchNodeOption(const TraceBasicBlock* bb, Options option)
{
    auto it = _graphOptions.find(bb);
    if (it != _graphOptions.end())
        *it ^= option;
}

CFGExporter::Options CFGExporter::getGraphOptions(TraceFunction* func) const
{
    auto it = _globalGraphOptions.find(func);
    if (it == _globalGraphOptions.end())
        return Options::invalid;
    else
        return static_cast<Options>(it->first);
}

void CFGExporter::setGraphOption(TraceFunction* func, Options option)
{
    assert(func);

    auto it = _globalGraphOptions.find(func);
    if (it != _globalGraphOptions.end())
    {
        it->first |= option;
        for (auto bb : func->basicBlocks())
            setNodeOption(bb, option);
    }
}

void CFGExporter::resetGraphOption(TraceFunction* func, Options option)
{
    assert(func);

    auto it = _globalGraphOptions.find(func);
    if (it != _globalGraphOptions.end())
    {
        it->first &= ~option;
        for (auto bb : func->basicBlocks())
            resetNodeOption(bb, option);
    }
}

void CFGExporter::minimizeBBsWithCostLessThan(uint64 minimalCost)
{
    for (const auto& node : _nodeMap)
    {
        if (node.cost() <= minimalCost)
            setNodeOption(node.basicBlock(), Options::reduced);
        else
            resetNodeOption(node.basicBlock(), Options::reduced);
    }
}

double CFGExporter::minimalCostPercentage(TraceFunction* func) const
{
    auto it = _globalGraphOptions.find(func);
    if (it == _globalGraphOptions.end())
        return NAN;
    else
        return it->second;
}

void CFGExporter::setMinimalCostPercentage(TraceFunction* func, double percentage)
{
    auto it = _globalGraphOptions.find(func);
    if (it != _globalGraphOptions.end())
        it->second = percentage;
}

const CFGNode* CFGExporter::findNode(const TraceBasicBlock* bb) const
{
    if (!bb)
        return nullptr;

    auto it = _nodeMap.find(bb);
    return (it == _nodeMap.end()) ? nullptr : std::addressof(*it);
}

CFGNode* CFGExporter::findNode(const TraceBasicBlock* bb)
{
    return const_cast<CFGNode*>(static_cast<const CFGExporter*>(this)->findNode(bb));
}

const CFGEdge* CFGExporter::findEdge(const TraceBasicBlock* bbFrom, const TraceBasicBlock* bbTo) const
{
    auto it = _edgeMap.find(std::make_pair(bbFrom, bbTo));
    return (it == _edgeMap.end()) ? nullptr : std::addressof(*it);
}

CFGEdge* CFGExporter::findEdge(const TraceBasicBlock* bbFrom, const TraceBasicBlock* bbTo)
{
    return const_cast<CFGEdge*>(static_cast<const CFGExporter*>(this)->findEdge(bbFrom, bbTo));
}

void CFGExporter::reset(CostItem* i, EventType* et, QString filename)
{
    _graphCreated = false;

    _eventType = et;

    _nodeMap.clear();
    _edgeMap.clear();

    if (_func && _tmpFile)
    {
        _tmpFile->setAutoRemove(true);
        delete _tmpFile;
    }

    if (i)
    {
        switch (i->type())
        {
            case ProfileContext::Function:
                _func = static_cast<TraceFunction*>(i);
                break;
            case ProfileContext::Call:
                _func = static_cast<TraceCall*>(i)->caller(true);
                break;
            case ProfileContext::BasicBlock:
                _func = static_cast<TraceBasicBlock*>(i)->function();
                break;
            default:
                _func = nullptr;
                return;
        }

        const auto& BBs = _func->basicBlocks();
        if (BBs.empty())
        {
            _errorMessage = QObject::tr("Control-flow graph requires running "
                                        "callgrind with option --dump-instr=yes");
            return;
        }

        if (!_globalGraphOptions.contains(_func))
        {
            _globalGraphOptions.insert(_func, std::make_pair(Options::default_, -1.0));
            for (auto bb : BBs)
                _graphOptions.insert(bb, Options::default_);
        }

        setDumpFile(filename);
    }
    else
    {
        _func = nullptr;
        _dotName.clear();
    }
}

void CFGExporter::sortEdges()
{
    for (auto& node : _nodeMap)
        node.sortEdges();
}

bool CFGExporter::writeDot(QIODevice* device)
{
    if (!_func)
        return false;

    // copy-constructors of QFile and QTextStream are deleted
    // so we have to allocate them on heap
    QFile* file = nullptr;
    QTextStream* stream = nullptr;

    if (device)
        stream = new QTextStream{device};
    else if (_tmpFile)
        stream = new QTextStream{_tmpFile};
    else
    {
        file = new QFile{_dotName};
        if (!file->open(QIODevice::WriteOnly))
        {
            qDebug() << "Cannot write dot file \'" << _dotName << "\'";
            delete file;
            return false;
        }
        stream = new QTextStream{file};
    }

    if (_graphCreated || createGraph())
    {
        *stream << "digraph \"control-flow graph\" {\n";

        if (_layout == Layout::LeftRight)
            *stream << "  rankdir=LR;\n";

        dumpNodes(*stream);
        dumpEdges(*stream);

        *stream << "}\n";
    }

    if (!device)
    {
        if (_tmpFile)
        {
            stream->flush();
            _tmpFile->seek(0);
        }
        else
            delete file;
    }

    delete stream;

    return true;
}

bool CFGExporter::createGraph()
{
    if (!_func || _graphCreated)
        return false;

    _graphCreated = true;

    for (auto bb : _func->basicBlocks())
        _nodeMap.insert(bb, CFGNode{bb, bb->subCost(_eventType)});

    for (auto& node : _nodeMap)
    {
        TraceBasicBlock* bbFrom = node.basicBlock();

        for (auto& br : bbFrom->outgoingBranches())
        {
            TraceBasicBlock* bbTo = br.bbTo();
            CFGNode* nodeTo = findNode(bbTo);

            auto edgeIt = _edgeMap.insert(std::make_pair(bbFrom, bbTo),
                                          CFGEdge{std::addressof(br), std::addressof(node), nodeTo});
            CFGEdge* edgePtr = std::addressof(*edgeIt);

            node.addOutgoingEdge(edgePtr);
            nodeTo->addIncomingEdge(edgePtr);
        }
    }

    return fillInstrStrings(_func);
}

class LineBuffer final
{
public:
    using pos_type = std::size_t;

    LineBuffer() = default;

    pos_type capacity() const { return _bufSize; }

    pos_type getPos() const { return _pos; }
    void setPos(pos_type pos) { _pos = pos; }

    char elem(pos_type offset = 0) const { return _buf[_pos + offset]; }
    void setElem(pos_type pos, char c) { _buf[pos] = c; }

    // relatively to the beginning of _buf
    char* data(pos_type offset = 0) { return _buf + offset; }
    const char* data(pos_type offset = 0) const { return _buf + offset; }

    // relatively to _pos
    char* relData() { return data(_pos); }
    const char* relData() const { return data(_pos); }

    void advance(pos_type offset) { _pos += offset; }

    void skipWhitespaces()
    {
        while (_buf[_pos] == ' ' || _buf[_pos] == '\t')
            _pos++;
    }

private:
    static constexpr pos_type _bufSize = 256;
    char _buf[_bufSize];
    pos_type _pos = 0;
};

class FileSearcher final
{
public:
    FileSearcher(const TraceData* data, const QString& filename);

    bool searchFile(QString& dir) const;
    QString getObjDump() const;

private:
    QProcessEnvironment _env;
    const TraceData* _data;
    QString _filename;
};

FileSearcher::FileSearcher(const TraceData* data, const QString& filename)
    : _env{QProcessEnvironment::systemEnvironment()}, _data{data}, _filename{filename} {}

bool FileSearcher::searchFile(QString& dir) const
{
    if (QDir::isAbsolutePath(dir))
    {
        if (QFile::exists(dir + '/' + _filename))
            return true;
        else
        {
            QString sysRoot = _env.value(QStringLiteral("SYSROOT"));
            if (!sysRoot.isEmpty())
            {
                if (!dir.startsWith('/') && !sysRoot.endsWith('/'))
                    sysRoot.append('/');

                dir.prepend(sysRoot);

                return QFile::exists(dir + '/' + _filename);
            }
        }
    }
    else
    {
        const QFileInfo fi(dir, _filename);
        if (fi.exists()) {
            dir = fi.absolutePath();
            return true;
        }
        else
        {
            TracePart* firstPart = _data->parts().first();
            if (firstPart)
            {
                QFileInfo partFile{firstPart->name()};
                QString absolutePath = partFile.absolutePath();
                if (QFileInfo{absolutePath, _filename}.exists())
                {
                    dir = std::move(absolutePath);
                    return true;
                }
            }
        }
    }

    return false;
}

QString FileSearcher::getObjDump() const
{
    QString objdump = _env.value(QStringLiteral("OBJDUMP_FORMAT"));
    if (objdump.isEmpty())
        return _env.value(QStringLiteral("OBJDUMP"), QStringLiteral("objdump"));
    return objdump;
}

class ObjdumpParser final
{
public:
    using instrStringsMap = std::map<Addr, std::pair<QString, QString>>;

    ObjdumpParser(TraceFunction* func, EventType* et);

    const instrStringsMap& getInstrStrings();
    const QString& errorMessage() const { return _errorMessage; }

private:
    using instr_iterator = typename TraceInstrMap::iterator;

    void runObjdump();

    static bool isHexDigit(char c);

    Addr parseAddress();
    QString parseEncoding();
    QString parseMnemonic();
    QString parseOperands();

    void getObjAddr();
    void getCostAddr();

    TraceFunction* _func;
    EventType* _eventType;

    QProcess _objdump;

    QString _objFile;
    QString _objdumpCmd;
    QString _errorMessage;

    LineBuffer _line;

    instr_iterator _it;
    instr_iterator _ite;

    Addr _objAddr;
    Addr _costAddr;
    Addr _nextCostAddr;
    Addr _dumpStartAddr;
    Addr _dumpEndAddr;

    bool _needObjAddr = true;
    bool _needCostAddr = true;
    bool _isArm;

    int _objdumpLineno = 0;

    instrStringsMap _instrStrings;
};

ObjdumpParser::ObjdumpParser(TraceFunction* func, EventType* et)
    : _func{func}, _eventType{et}
{
    assert(_func);
    assert(_func->data());

    _isArm = (_func->data()->architecture() == TraceData::ArchARM);

    auto instrMap = _func->instrMap();
    assert(!instrMap->empty());

    _it = instrMap->begin();
    _ite = instrMap->end();

    _nextCostAddr = _it->addr();
    if (_isArm)
        _nextCostAddr = _nextCostAddr.alignedDown(2);

    _dumpStartAddr = _nextCostAddr;
    _dumpEndAddr = _func->lastAddress() + 2;
}

void ObjdumpParser::runObjdump()
{
    TraceObject* objectFile = _func->object();
    QString dir = objectFile->directory();

    FileSearcher searcher{_func->data(), objectFile->shortName()};
    if (!searcher.searchFile(dir))
    {
        _errorMessage =
            QObject::tr("For annotated machine code, the following object file is needed\n"
                        "    \'%1\'\n"
                        "This file cannot be found.\n").arg(objectFile->name());
        if (_isArm)
            _errorMessage += QObject::tr("If cross-compiled, set SYSROOT variable.");
    }
    else
    {
        objectFile->setDirectory(dir);
        _objFile = dir + '/' + objectFile->shortName();

        const int margin = _isArm ? 4 : 20;

        QStringList args{"-C", "-d"};
        args << QStringLiteral("--start-address=0x%1").arg(_dumpStartAddr.toString()),
        args << QStringLiteral("--stop-address=0x%1").arg((_dumpEndAddr + margin).toString()),
        args << _objFile;

        QString objdump = searcher.getObjDump();
        _objdumpCmd = objdump + ' ' + args.join(' ');

        qDebug("Running \'%s\'...", qPrintable(_objdumpCmd));
        _objdump.start(objdump, args);
        if (!_objdump.waitForStarted() || !_objdump.waitForFinished())
        {
            _errorMessage = QObject::tr("There is an error trying to execute the command\n"
                                        "    \'%1\'\n"
                                        "Check that you have installed \'objdump\'.\n"
                                        "This utility can be found in the \'binutils\' package")
                                       .arg(_objdumpCmd);
        }
    }
}

const ObjdumpParser::instrStringsMap& ObjdumpParser::getInstrStrings()
{
    if (!_instrStrings.empty())
        return _instrStrings;

    runObjdump();
    if (!_errorMessage.isEmpty())
        return _instrStrings;

    int noAssLines = 0;

    for (bool skipLineWritten = true; ; _line.setPos(0))
    {
        if (_needObjAddr)
            getObjAddr();

        if (_objAddr == 0 || _objAddr > _dumpEndAddr)
            break;

        if (_needCostAddr && Addr{0} < _nextCostAddr && _nextCostAddr <= _objAddr)
            getCostAddr();

        Addr addr;
        QString mnemonic, operands;
        if (_objAddr < _nextCostAddr || _nextCostAddr == 0 || _costAddr == 0)
        {
            addr = parseAddress();
            assert(addr == _objAddr);

            QString encoding = parseEncoding();
            Q_UNUSED(encoding);
            assert(!encoding.isNull());

            mnemonic = parseMnemonic();
            operands = parseOperands();

            if (_costAddr == _objAddr)
                _needCostAddr = true;

            _needObjAddr = true;
        }
        else
        {
            addr = _costAddr;
            operands = QStringLiteral("(No Instruction)");
            noAssLines++;

            _needCostAddr = true;
        }

        if ((_costAddr == 0     || addr > _costAddr + 3 * GlobalConfig::context()) &&
            (_nextCostAddr == 0 || addr < _nextCostAddr - 3 * GlobalConfig::context()))
        {
            if (skipLineWritten || _it == _ite)
                continue;
            else
            {
                skipLineWritten = true;

                mnemonic.clear();
                operands = QStringLiteral("...");
            }
        }
        else
            skipLineWritten = false;

        if (!mnemonic.isEmpty())
            _instrStrings.emplace(addr, std::make_pair(mnemonic, operands));
    }

    if (noAssLines > 1)
    {
        _errorMessage = QObject::tr("There are %1 cost line(s) without machine code.\n"
                                    "This happens because the code of %2 does not seem "
                                    "to match the profile data file.\n"
                                    "Are you using an old profile data file or is the above"
                                    "mentioned\n"
                                    "ELF object from an updated installation/another machine?")
                                   .arg(noAssLines).arg(_objFile);
    }
    else if (_instrStrings.empty())
    {
        _errorMessage = QObject::tr("There seems to be an error trying to execute the command "
                                    "\'%1\'.\n"
                                    "Check that the ELF object used in the command exists.\n"
                                    "Check that you have installed \'objdump\'.\n"
                                    "This utility can be found in the \'binutils\' package.")
                                   .arg(_objdumpCmd);
    }

    return _instrStrings;
}

void ObjdumpParser::getObjAddr()
{
    _needObjAddr = false;
    for (; ; _line.setPos(0))
    {
        const qint64 readBytes = _objdump.readLine(_line.data(), _line.capacity());
        if (readBytes <= 0)
        {
            _objAddr = Addr{0};
            break;
        }
        else
        {
            _objdumpLineno++;
            if (readBytes == static_cast<qint64>(_line.capacity()))
                qDebug("ERROR: Line %d is too long\n", _objdumpLineno);
            else if (_line.data()[readBytes - 1] == '\n')
                _line.setElem(readBytes - 1, '\0');

            _objAddr = parseAddress();
            if (_dumpStartAddr <= _objAddr && _objAddr <= _dumpEndAddr)
                break;
        }
    }

    _line.setPos(0);
}

void ObjdumpParser::getCostAddr()
{
    _needCostAddr = false;

    _it = std::find_if(++_it, _ite, [this](TraceInstr& i){ return i.hasCost(_eventType); });

    _costAddr = _nextCostAddr;
    _nextCostAddr = (_it == _ite) ? Addr{0} : _it->addr();
    if (_isArm)
        _nextCostAddr = _nextCostAddr.alignedDown(2);
}

bool ObjdumpParser::isHexDigit(char c)
{
    return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f');
}

Addr ObjdumpParser::parseAddress()
{
    _line.skipWhitespaces();

    Addr addr;
    int digits = addr.set(_line.relData());

    if (digits == 0 || _line.elem(digits) != ':')
        return Addr{0};
    else
    {
        _line.advance(digits + 1);
        return addr;
    }
}

QString ObjdumpParser::parseEncoding()
{
    _line.skipWhitespaces();
    const LineBuffer::pos_type start = _line.getPos();

    while (true)
    {
        if (!isHexDigit(_line.elem()) || !isHexDigit(_line.elem(1)))
            break;
        else if (_line.elem(2) == ' ')
            _line.advance(3);
        else if (!isHexDigit(_line.elem(2)) || !isHexDigit(_line.elem(3)))
            break;
        else if (_line.elem(4) == ' ')
            _line.advance(5);
        else if (!isHexDigit(_line.elem(4)) ||
                 !isHexDigit(_line.elem(5)) ||
                 !isHexDigit(_line.elem(6)) ||
                 !isHexDigit(_line.elem(7)) ||
                _line.elem(8) != ' ')
            break;
        else
            _line.advance(9);
    }

    if (_line.getPos() > start)
        return QString::fromLatin1(_line.data(start), _line.getPos() - start - 1);
    else
        return QString{};
}

QString ObjdumpParser::parseMnemonic()
{
    _line.skipWhitespaces();
    const LineBuffer::pos_type start = _line.getPos();

    while (_line.elem() && _line.elem() != ' ' && _line.elem() != '\t')
        _line.advance(1);

    return QString::fromLatin1(_line.data(start), _line.getPos() - start);
}

QString ObjdumpParser::parseOperands()
{
    _line.skipWhitespaces();

    char* operandsPos = _line.relData();
    const char commentDelimiter = _isArm ? ';' : '#';
    char* commentPos = std::strchr(operandsPos, commentDelimiter);

    std::size_t operandsLen = std::strlen(operandsPos);
    if (commentPos)
        operandsLen = std::min<std::size_t>(operandsLen, commentPos - operandsPos);

    if (operandsLen > 0 && _line.elem(operandsLen - 1) == '\n')
        operandsLen--;

    if (operandsLen > 50)
        return QString::fromLatin1(operandsPos, 47) + QStringLiteral("...");
    else
        return QString::fromLatin1(operandsPos, operandsLen);
}

bool CFGExporter::fillInstrStrings(TraceFunction* func)
{
    assert(func);

    if (_nodeMap.empty())
        return false;

    ObjdumpParser parser{func, _eventType};
    const ObjdumpParser::instrStringsMap& instrStrings = parser.getInstrStrings();
    if (!parser.errorMessage().isEmpty())
    {
        _errorMessage = parser.errorMessage();
        return false;
    }

    for (auto& node : _nodeMap)
    {
        const TraceBasicBlock* bb = node.basicBlock();

        auto firstIt = instrStrings.find(bb->firstAddr());
        auto lastIt = instrStrings.find(bb->lastAddr());
        assert(lastIt != instrStrings.end());

        node.insertInstructions(firstIt, std::next(lastIt));
    }

    return true;
}

void CFGExporter::dumpNodes(QTextStream& ts)
{
    for (auto& node : _nodeMap)
    {
        if (getNodeOptions(node.basicBlock()) & Options::reduced)
            dumpNodeReduced(ts, node);
        else
            dumpNodeExtended(ts, node);
    }
}

void CFGExporter::dumpNodeReduced(QTextStream& ts, const CFGNode& node)
{
    const TraceBasicBlock* bb = node.basicBlock();

    ts << QStringLiteral("  bb%1 [shape=record, label=\"")
                        .arg(reinterpret_cast<qulonglong>(bb), 0, 16);

    if (_layout == Layout::TopDown)
        ts << '{';

    ts << QStringLiteral(" cost: %1 | 0x%2 ").arg(node.cost()).arg(bb->firstAddr().toString());

    if (_layout == Layout::TopDown)
        ts << '}';

    ts << "\"]\n";
}

enum DumpMode
{
    none = 0,
    pc   = 1 << 0,
    cost = 1 << 1
};

void CFGExporter::dumpNodeExtended(QTextStream& ts, const CFGNode& node)
{
    const TraceBasicBlock* bb = node.basicBlock();
    assert(bb);

    Options options = getNodeOptions(bb);

    int mode = DumpMode::none;
    int span = 2;
    if (options & Options::showInstrPC)
    {
        mode |= DumpMode::pc;
        span++;
    }
    if (options & Options::showInstrCost)
    {
        mode |= DumpMode::cost;
        span++;
    }

    ts << QStringLiteral("  bb%1 [shape=plaintext, label=<\n"
                         "  <table border=\"0\" cellborder=\"1\" cellspacing=\"0\">\n"
                         "  <tr>\n"
                         "    <td colspan=\"%2\">cost: %3</td>\n"
                         "  </tr>\n"
                         "  <tr>\n"
                         "    <td colspan=\"%4\">0x%5</td>\n"
                         "  </tr>\n").arg(reinterpret_cast<qulonglong>(bb), 0, 16)
                                     .arg(span).arg(node.cost()).arg(span)
                                     .arg(bb->firstAddr().toString());

    auto strIt = node.begin();
    auto instrIt = bb->begin();
    auto lastInstrIt = std::prev(bb->end());

    if (instrIt != lastInstrIt)
    {
        TraceInstr* instr = *instrIt;
        ts << QStringLiteral("  <tr>\n"
                             "    <td port=\"IL%1\" align=\"left\">").arg(instr->addr().toString());

        dumpAlmostEntireInstr(ts, strIt, instr, mode);

        for (++instrIt, ++strIt; instrIt != lastInstrIt; ++instrIt, ++strIt)
        {
            ts << "  <tr>\n"
                  "    <td align=\"left\">";

            dumpAlmostEntireInstr(ts, strIt, *instrIt, mode);
        }
    }

    ts << QStringLiteral("  <tr>\n"
                         "    <td port=\"IL%1\" align=\"left\">").arg(bb->lastAddr().toString());

    dumpAlmostEntireInstr(ts, strIt, *lastInstrIt, mode);

    ts << "  </table>>]\n";
}

void CFGExporter::dumpAlmostEntireInstr(QTextStream& ts, CFGNode::const_iterator strIt,
                                        TraceInstr* instr, int mode)
{
    if (mode & DumpMode::pc)
        ts << QStringLiteral("0x%1</td>\n"
                             "    <td align=\"left\">").arg(instr->addr().toString());

    if (mode & DumpMode::cost)
        ts << QStringLiteral("%1</td>\n"
                             "    <td align=\"left\">").arg(instr->subCost(_eventType).pretty());

    ts << QStringLiteral("%1</td>\n"
                         "    <td align=\"left\">%2</td>\n"
                         "  </tr>\n").arg(strIt->_mnemonic).arg(strIt->_operandsHTML);
}

namespace
{

void dumpNonFalseBranchColor(QTextStream& ts, const TraceBranch* br)
{
    const char* color;
    switch (br->brType())
    {
        case TraceBranch::Type::true_:
            color = "blue";
            break;
        case TraceBranch::Type::unconditional:
            color = "black";
            break;
        case TraceBranch::Type::indirect:
            color = "green";
            break;
        case TraceBranch::Type::fallThrough:
            color = "purple";
            break;
        default:
            assert(false);
            return;
    }

    ts << QStringLiteral("color=%1, ").arg(color);
}

void dumpRegularBranch(QTextStream& ts, const TraceBranch* br)
{
    ts << QStringLiteral("  bb%1:s -> bb%2:n [")
                        .arg(reinterpret_cast<qulonglong>(br->bbFrom()), 0, 16)
                        .arg(reinterpret_cast<qulonglong>(br->bbTo()), 0, 16);

    if (br->brType() == TraceBranch::Type::false_)
        ts << "color=red, ";
    else
        dumpNonFalseBranchColor(ts, br);
}

} // unnamed namespace

void CFGExporter::dumpEdges(QTextStream& ts)
{
    for (auto& edge : _edgeMap)
    {
        TraceBranch* br = edge.branch();
        assert(br);

        if (br->isCycle())
            dumpCyclicEdge(ts, br);
        else
            dumpRegularBranch(ts, br);

        ts << QStringLiteral("label=\"%1\"]\n").arg(br->executedCount().v);
    }
}

void CFGExporter::dumpCyclicEdge(QTextStream& ts, const TraceBranch* br)
{
    assert(br->bbFrom() == br->bbTo());

    const TraceBasicBlock* bb = br->bbFrom();
    const auto bbI = reinterpret_cast<qulonglong>(bb);

    if (getNodeOptions(bb) & Options::reduced)
    {
        ts << QStringLiteral("  bb%1:w -> bb%2:w [constraint=false, ")
                            .arg(bbI, 0, 16).arg(bbI, 0, 16);
    }
    else
    {
        ts << QStringLiteral("  bb%1:IL%2:w -> bb%3:IL%4:w [constraint=false, ")
                            .arg(bbI, 0, 16).arg(bb->lastAddr().toString())
                            .arg(bbI, 0, 16).arg(bb->firstAddr().toString());
    }

    dumpNonFalseBranchColor(ts, br);
}

bool CFGExporter::savePrompt(QWidget* parent, TraceFunction* func, EventType* eventType,
                             const CFGExporter& origExporter)
{
    const QString filter1 = QStringLiteral("text/vnd.graphviz");
    const QString filter2 = QStringLiteral("application/pdf");
    const QString filter3 = QStringLiteral("application/postscript");

    QFileDialog saveDialog{parent, QObject::tr("Export Graph")};
    saveDialog.setMimeTypeFilters(QStringList{filter1, filter2, filter3});
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (saveDialog.exec())
    {
        const QString intendedName = saveDialog.selectedFiles().first();
        if (intendedName.isNull() || intendedName.isEmpty())
            return false;

        QTemporaryFile maybeTemp;
        QString dotName;

        const QString mime = saveDialog.selectedMimeTypeFilter();
        if (mime == filter1)
            dotName = intendedName;
        else
        {
            maybeTemp.open();
            dotName = maybeTemp.fileName();
        }

        CFGExporter ge{origExporter, func, eventType, dotName};

        if (ge.writeDot())
        {
            if (mime == filter1)
                return true;
            else
            {
                QProcess proc;
                proc.setStandardOutputFile(intendedName, QFile::Truncate);
                proc.start("dot", QStringList{(mime == filter2) ? "-Tpdf" : "-Tps", dotName},
                           QProcess::ReadWrite);
                proc.waitForFinished();

                if (proc.exitStatus() == QProcess::NormalExit)
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(intendedName));
                    return true;
                }
            }
        }
    }

    return false;
}

// ======================================================================================

//
// CanvasCFGNode
//

CanvasCFGNode::CanvasCFGNode(ControlFlowGraphView* view, CFGNode* node,
                             qreal x, qreal y, qreal w, qreal h) :
    QGraphicsRectItem{x, y, w, h}, _node{node}, _view{view}
{
    if (!_node || !_view)
        return;

    const QFontMetrics fm = _view->fontMetrics();

    TraceBasicBlock* bb = _node->basicBlock();
    if (_view->showInstrPC(_node))
        _pcLen = fm.size(Qt::TextSingleLine, "0x" + bb->firstAddr().toString()).width() + _margin;
    else
        _pcLen = 0;

    if (_view->showInstrCost(_node))
    {
        QString costStr = bb->firstInstr()->subCost(_view->eventType()).pretty();
        _costLen = fm.size(Qt::TextSingleLine, costStr).width() + _margin;
    }
    else
        _costLen = 0;

    std::vector<int> instrWidths(_node->instrNumber());
    std::transform(_node->begin(), _node->end(),
                   instrWidths.begin(), [&fm](const CFGNode::instrString& instr)
    {
        return fm.size(Qt::TextSingleLine, instr._mnemonic).width();
    });

    auto maxLenIt = std::max_element(instrWidths.begin(), instrWidths.end());

    _mnemonicLen = *maxLenIt + _margin;
    _argsLen = w - mnemonicRightBorder();

    setFlag(QGraphicsItem::ItemIsSelectable);
    setZValue(1.0);
}

void CanvasCFGNode::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const QRectF r = rect();
    const bool reduced = _view->isReduced(_node);
    const qreal step = r.height() / (reduced ? 2 : _node->instrNumber() + 2);

    TraceBasicBlock* bb = _node->basicBlock();

    qreal topLineY = r.y();
    p->fillRect(r.x() + 1, topLineY + 1, r.width(), step * 2, Qt::gray);

    QString cost;
    if (GlobalConfig::showPercentage())
    {
        const SubCost total = bb->function()->subCost(_view->eventType());
        const double selfPercentage = 100.0 * _node->cost() / total;
        cost = QStringLiteral("%1 %").arg(selfPercentage, 0, 'f',
                                          GlobalConfig::percentPrecision());
    }
    else
        cost = SubCost{_node->cost()}.pretty();

    p->drawText(r.x(), topLineY, r.width(), step,
                Qt::AlignCenter, cost);
    topLineY += step;

    p->drawText(r.x(), topLineY, r.width(), step,
                Qt::AlignCenter, "0x" + bb->firstAddr().toString());
    p->drawLine(r.x(),             topLineY,
                r.x() + r.width(), topLineY);

    if (!reduced)
    {
        topLineY += step;

        auto instrIt = bb->begin();
        for (auto& str : *_node)
        {
            TraceInstr* instr = *instrIt;

            if (_pcLen != 0)
                p->drawText(r.x() + 2, topLineY, _pcLen, step,
                            Qt::AlignLeft, "0x" + instr->addr().toString());

            if (_costLen != 0)
                p->drawText(r.x() + _pcLen + 2, topLineY, _costLen, step,
                            Qt::AlignLeft, instr->subCost(_view->eventType()).pretty());

            p->drawText(r.x() + costRightBorder() + 2, topLineY, _mnemonicLen, step,
                        Qt::AlignLeft, str._mnemonic);
            p->drawText(r.x() + mnemonicRightBorder() + 2, topLineY, _argsLen, step,
                        Qt::AlignLeft, str._operands);
            p->drawLine(r.x(),             topLineY,
                        r.x() + r.width(), topLineY);

            topLineY += step;
            ++instrIt;
        }

        const qreal bottomLineY = r.y() + r.height();
        const qreal firstInstrTopLineY = r.y() + step * 2;

        if (_pcLen != 0)
            p->drawLine(r.x() + _pcLen, firstInstrTopLineY,
                        r.x() + _pcLen, bottomLineY);

        if (_costLen != 0)
            p->drawLine(r.x() + costRightBorder(), firstInstrTopLineY,
                        r.x() + costRightBorder(), bottomLineY);

        p->drawLine(r.x() + mnemonicRightBorder(), firstInstrTopLineY,
                    r.x() + mnemonicRightBorder(), bottomLineY);
    }

    if (isSelected())
        p->setPen(QPen{Qt::darkGreen, 2});

    p->drawRect(r);
}

// ======================================================================================

//
// CanvasCFGEdgeLabel
//

CanvasCFGEdgeLabel::CanvasCFGEdgeLabel(CanvasCFGEdge* ce, qreal x, qreal y, qreal w, qreal h)
    : QGraphicsRectItem{x, y, w, h}, _ce{ce}
{
    CFGEdge* e = _ce->edge();
    if (!e)
        return;

    _label = QStringLiteral("%1 x").arg(e->count());

    setZValue(1.5);
}

void CanvasCFGEdgeLabel::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    p->drawText(rect(), Qt::AlignCenter, _label);
}

// ======================================================================================

//
// CanvasCFGEdgeArrow
//

CanvasCFGEdgeArrow::CanvasCFGEdgeArrow(CanvasCFGEdge* ce, const QPolygon& arrow)
    : QGraphicsPolygonItem{arrow}, _ce{ce}
{
    setZValue(1.5);
}

void CanvasCFGEdgeArrow::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::black);
    p->drawPolygon(polygon(), Qt::OddEvenFill);
}

// ======================================================================================

//
// CanvasCFGEdge
//

CanvasCFGEdge::CanvasCFGEdge(CFGEdge* e, const QPolygon& poly, const QColor& edgeColor)
    : _edge{e}, _points{poly}
{
    const int nPoints = _points.size();

    QPainterPath path;
    path.moveTo(_points[0]);
    for (int i = 1; i < nPoints; i += 3)
        path.cubicTo(_points[i], _points[(i + 1) % nPoints], _points[(i + 2) % nPoints]);

    setPath(path);
    setPen(QPen{edgeColor});
    setFlag(QGraphicsItem::ItemIsSelectable);
    setZValue(0.5);
}

void CanvasCFGEdge::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*)
{
#if QT_VERSION >= 0x040600
    qreal levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    qreal levelOfDetail = option->levelOfDetail;
#endif

    static constexpr double thickness = 0.9;

    QPen mypen = pen();
    mypen.setWidthF(isSelected() ? 2.0 : thickness / levelOfDetail);
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(mypen);
    p->drawPath(path());
}

// ======================================================================================

//
// ControlFlowGraphView
//

ControlFlowGraphView::ControlFlowGraphView(TraceItemView* parentView, QWidget* parent,
                                           const QString& name)
    : QGraphicsView{parent}, TraceItemView{parentView}
{
    setObjectName(name);
    setWhatsThis(whatsThis());
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);

    _panningView = new PanningView(this);

    _panningView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->raise();
    _panningView->hide();

    connect(_panningView, &PanningView::zoomRectMoved,
            this, &ControlFlowGraphView::zoomRectMoved);
    connect(_panningView, &PanningView::zoomRectMoveFinished,
            this, &ControlFlowGraphView::zoomRectMoveFinished);
    connect(std::addressof(_renderTimer), &QTimer::timeout,
            this, &ControlFlowGraphView::showRenderWarning);
}

ControlFlowGraphView::~ControlFlowGraphView()
{
    if (_scene)
    {
        _panningView->setScene(nullptr);
        setScene(nullptr);
        delete _scene;
    }

    delete _panningView;
}

ControlFlowGraphView::ZoomPosition ControlFlowGraphView::strToZoomPos(const QString& s)
{
    if (s == QLatin1String("TopLeft"))
        return ZoomPosition::TopLeft;
    if (s == QLatin1String("TopRight"))
        return ZoomPosition::TopRight;
    if (s == QLatin1String("BottomLeft"))
        return ZoomPosition::BottomLeft;
    if (s == QLatin1String("BottomRight"))
        return ZoomPosition::BottomRight;
    if (s == QLatin1String("Auto"))
        return ZoomPosition::Auto;
    if (s == QLatin1String("Hide"))
        return ZoomPosition::Hide;
    return ZoomPosition::Auto;
}

QString ControlFlowGraphView::zoomPosToStr(ZoomPosition p)
{
    switch (p)
    {
        case ZoomPosition::TopLeft:
            return QStringLiteral("TopLeft");
        case ZoomPosition::TopRight:
            return QStringLiteral("TopRight");
        case ZoomPosition::BottomLeft:
            return QStringLiteral("BottomLeft");
        case ZoomPosition::BottomRight:
            return QStringLiteral("BottomRight");
        case ZoomPosition::Auto:
            return QStringLiteral("Auto");
        case ZoomPosition::Hide:
            return QStringLiteral("Hide");
        default:
            return QString{};
    }
}

void ControlFlowGraphView::restoreOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix, postfix);

    _exporter.setLayout(
        CFGExporter::strToLayout(g->value(QStringLiteral("Layout"),
                                 CFGExporter::layoutToStr(CFGExporter::Layout::TopDown)).toString()));
    _zoomPosition = strToZoomPos(g->value(QStringLiteral("ZoomPosition"),
                                          zoomPosToStr(ZoomPosition::Auto)).toString());

    delete g;
}

void ControlFlowGraphView::saveOptions(const QString& prefix, const QString& postfix)
{
    ConfigGroup* g = ConfigStorage::group(prefix + postfix);

    g->setValue(QStringLiteral("Layout"), CFGExporter::layoutToStr(_exporter.layout()),
                                          CFGExporter::layoutToStr(CFGExporter::Layout::TopDown));
    g->setValue(QStringLiteral("ZoomPosition"), zoomPosToStr(_zoomPosition),
                                                zoomPosToStr(ZoomPosition::Auto));

    delete g;
}

QString ControlFlowGraphView::whatsThis() const
{
    return QObject::tr("This is Control Flow Graph by KetchuppOfficial");
}

bool ControlFlowGraphView::isReduced(const CFGNode* node) const
{
    return _exporter.getNodeOptions(node->basicBlock()) & CFGExporter::Options::reduced;
}

bool ControlFlowGraphView::showInstrPC(const CFGNode* node) const
{
    return _exporter.getNodeOptions(node->basicBlock()) & CFGExporter::Options::showInstrPC;
}

bool ControlFlowGraphView::showInstrCost(const CFGNode* node) const
{
    return _exporter.getNodeOptions(node->basicBlock()) & CFGExporter::Options::showInstrCost;
}

TraceFunction* ControlFlowGraphView::getFunction()
{
    assert(_activeItem);

    switch (_activeItem->type())
    {
        case ProfileContext::BasicBlock:
            return static_cast<TraceBasicBlock*>(_activeItem)->function();
        case ProfileContext::Branch:
            return static_cast<TraceBranch*>(_activeItem)->bbFrom()->function();
        case ProfileContext::Call:
            return static_cast<TraceCall*>(_activeItem)->caller();
        case ProfileContext::Function:
            return static_cast<TraceFunction*>(_activeItem);
        default:
            assert(false);
            return nullptr;
    }
}

void ControlFlowGraphView::zoomRectMoved(qreal dx, qreal dy)
{
    QScrollBar* hBar = horizontalScrollBar();
    QScrollBar* vBar = verticalScrollBar();
    hBar->setValue(hBar->value() + static_cast<int>(dx));
    vBar->setValue(vBar->value() + static_cast<int>(dy));
}

void ControlFlowGraphView::zoomRectMoveFinished()
{
    if (_zoomPosition == ZoomPosition::Auto)
        updateSizes();
}

void ControlFlowGraphView::showRenderWarning()
{
    QString s;
    if (_renderProcess)
        s = QObject::tr("Warning: a long lasting graph layouting is in progress.\n"
                        "Reduce node/edge limits for speedup.\n");
    else
        s = QObject::tr("Layouting stopped.\n");

    s += QObject::tr("The call graph has %1 nodes and %2 edges.\n")
                    .arg(_exporter.nodeCount()).arg(_exporter.edgeCount());

    showText(s);
}

void ControlFlowGraphView::showRenderError(const QString& text)
{
    QString err = QObject::tr("No graph available because the layouting process failed.\n");
    if (_renderProcess)
        err += QObject::tr("Trying to run the following command did not work:\n"
                           "\'%1\'\n").arg(_renderProcessCmdLine);
    err += QObject::tr("Please check that \'dot\' is installed (package GraphViz).");

    if (!text.isEmpty())
        err += QStringLiteral("\n\n%1").arg(text);

    showText(err);
}

void ControlFlowGraphView::stopRendering()
{
    if (!_renderProcess)
        return;

    _renderProcess->kill();
    _renderProcess->deleteLater();
    _renderProcess = nullptr;

    _unparsedOutput.clear();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(200);
}

void ControlFlowGraphView::readDotOutput()
{
    auto process = qobject_cast<QProcess*>(sender());
    qDebug() << "ControlFlowGraphView::readDotOutput: QProcess " << process;

    if (_renderProcess && process == _renderProcess)
        _unparsedOutput += QString::fromLocal8Bit(_renderProcess->readAllStandardOutput());
    else
        process->deleteLater();
}

void ControlFlowGraphView::dotError()
{
    auto process = qobject_cast<QProcess*>(sender());
    qDebug() << "ControlFlowGraphView::dotError: Got " << process->error() << " from QProcess "
             << process;

    if (_renderProcess && process == _renderProcess)
    {
        showRenderError(QString::fromLocal8Bit(_renderProcess->readAllStandardError()));

        _renderProcess->deleteLater();
        _renderProcess = nullptr;
    }
    else
        process->deleteLater();
}

void ControlFlowGraphView::dotExited()
{
    auto process = qobject_cast<QProcess*>(sender());
    qDebug() << "ControlFlowGraphView::dotExited: QProcess " << process;

    if (!_renderProcess || process != _renderProcess)
    {
        process->deleteLater();
        return;
    }

    _unparsedOutput += QString::fromLocal8Bit(_renderProcess->readAllStandardOutput());
    _renderProcess->deleteLater();
    _renderProcess = nullptr;

    _renderTimer.stop();
    viewport()->setUpdatesEnabled(false);
    clear();

    parseDot();
    checkScene();

    _exporter.sortEdges();

    _panningZoom = 0.0;
    _panningView->setScene(_scene);
    setScene(_scene);

    centerOnSelectedNodeOrEdge();

    updateSizes();

    _scene->update();
    viewport()->setUpdatesEnabled(true);
}

void ControlFlowGraphView::parseDot()
{
    QTextStream dotStream{std::addressof(_unparsedOutput), QIODevice::ReadOnly};
    _scaleY = 8 + 3 * fontMetrics().height();

    QString cmd;
    for (auto lineno = 1; ; lineno++)
    {
        QString line = dotStream.readLine();

        if (line.isNull())
            break;
        else if (!line.isEmpty())
        {
            QTextStream lineStream{std::addressof(line), QIODevice::ReadOnly};
            lineStream >> cmd;

            if (cmd == QLatin1String("stop"))
                break;
            else if (cmd == QLatin1String("graph"))
                setupScreen(lineStream, lineno);
            else if (!_scene)
                qDebug() << "Ignoring \'" << cmd << "\' without \'graph\' form dot ("
                            << _exporter.filename() << ":" << lineno << ")";
            else if (cmd == QLatin1String("node"))
                parseNode(lineStream);
            else if (cmd == QLatin1String("edge"))
                parseEdge(lineStream, lineno);
        }
    }
}

void ControlFlowGraphView::setupScreen(QTextStream& lineStream, int lineno)
{
    double dotWidth, dotHeight;
    lineStream >> dotWidth >> dotWidth >> dotHeight;

    _dotHeight = dotHeight;

    if (_scene)
        qDebug() << "Ignoring 2nd \'graph\' from dot ("
                 << _exporter.filename() << ":" << lineno << ")";
    else
    {
        const QSize pScreenSize = QApplication::primaryScreen()->size();

        _xMargin = 50;
        const auto w = static_cast<int>(_scaleX * dotWidth);
        if (w < pScreenSize.width())
            _xMargin += (pScreenSize.width() - w) / 2;

        _yMargin = 50;
        const auto h = static_cast<int>(_scaleY * _dotHeight);
        if (h < pScreenSize.height())
            _yMargin += (pScreenSize.height() - h) / 2;

        _scene = new QGraphicsScene{0.0, 0.0, static_cast<qreal>(w + 2 * _xMargin),
                                              static_cast<qreal>(h + 2 * _yMargin)};
        _scene->setBackgroundBrush(Qt::white);
    }
}

std::pair<int, int> ControlFlowGraphView::calculateCoords(QTextStream& lineStream)
{
    double xStr, yStr;
    lineStream >> xStr >> yStr;

    auto xx = static_cast<int>(_scaleX * xStr + _xMargin);
    auto yy = static_cast<int>(_scaleY * (_dotHeight - yStr) + _yMargin);

    return std::make_pair(xx, yy);
}

void ControlFlowGraphView::parseNode(QTextStream& lineStream)
{
    CFGNode* node = getNodeFromDot(lineStream);
    assert(node);
    assert(node->instrNumber() > 0);

    const std::pair<int, int> coords = calculateCoords(lineStream);

    double nodeWidth, nodeHeight;
    lineStream >> nodeWidth >> nodeHeight;

    node->setVisible(true);
    const qreal w = (_scaleX - 4.5) * nodeWidth;
    const qreal h = _scaleY * nodeHeight;

    auto rItem = new CanvasCFGNode{this, node, coords.first - w / 2, coords.second - h / 2, w, h};

    node->setCanvasNode(rItem);

    _scene->addItem(rItem);

    if (node->basicBlock() == selectedItem())
    {
        _selectedNode = node;
        rItem->setSelected(true);
    }
    else
        rItem->setSelected(node == _selectedNode);
}

CFGNode* ControlFlowGraphView::getNodeFromDot(QTextStream& lineStream)
{
    QString s;
    lineStream >> s;

    assert(s.length() >= 3);
    assert(s[0] == 'b' && s[1] == 'b');

    bool ok;
    qulonglong ibb = s.mid(2).toULongLong(&ok, 16);
    assert(ok);

    return _exporter.findNode(reinterpret_cast<TraceBasicBlock*>(ibb));
}

namespace
{

QColor getEdgeColor(CFGEdge* edge)
{
    assert(edge);
    assert(edge->branch());

    switch (edge->branch()->brType())
    {
        case TraceBranch::Type::unconditional:
            return Qt::black;
        case TraceBranch::Type::true_:
            return Qt::blue;
        case TraceBranch::Type::false_:
            return Qt::red;
        case TraceBranch::Type::indirect:
            return Qt::darkGreen;
        case TraceBranch::Type::fallThrough:
            return Qt::magenta;
        default:
            assert(false);
            return QColor{};
    }
}

CanvasCFGEdge* createEdge(CFGEdge* edge, const QPolygon& poly, QColor arrowColor)
{
    auto cEdge = new CanvasCFGEdge{edge, poly, arrowColor};
    edge->setCanvasEdge(cEdge);
    return cEdge;
}

CanvasCFGEdgeArrow* createArrow(CanvasCFGEdge* cEdge, const QPolygon& poly)
{
    const QPoint headPoint{poly.point(poly.size() - 1)};
    QPoint arrowDir{headPoint - poly.point(poly.size() - 2)};
    assert(!arrowDir.isNull());

    const auto length = static_cast<qreal>(arrowDir.x() * arrowDir.x() +
                                           arrowDir.y() * arrowDir.y());
    arrowDir *= 10.0 / std::sqrt(length);

    QPolygon arrow;
    arrow << QPoint{headPoint + arrowDir};
    arrow << QPoint{headPoint + QPoint{arrowDir.y() / 2, -arrowDir.x() / 2}};
    arrow << QPoint{headPoint + QPoint{-arrowDir.y() / 2, arrowDir.x() / 2}};

    return new CanvasCFGEdgeArrow{cEdge, arrow};
}

} // unnamed namespace

void ControlFlowGraphView::parseEdge(QTextStream& lineStream, int lineno)
{
    CFGEdge* edge = getEdgeFromDot(lineStream, lineno);
    if (!edge)
        return;

    QPolygon poly = getEdgePolygon(lineStream, lineno);
    if (poly.empty())
        return;

    edge->setVisible(true);

    CanvasCFGEdge* cEdge = createEdge(edge, poly, getEdgeColor(edge));
    if (edge->branch() == selectedItem())
    {
        _selectedEdge = edge;
        cEdge->setSelected(true);
    }
    else
        cEdge->setSelected(edge == _selectedEdge);

    _scene->addItem(cEdge);
    _scene->addItem(createArrow(cEdge, poly));

    QString label;
    Q_UNUSED(label);
    lineStream >> label;

    const std::pair<int, int> coords = calculateCoords(lineStream);
    auto cLabel = new CanvasCFGEdgeLabel{cEdge,
                                         static_cast<qreal>(coords.first - 60),
                                         static_cast<qreal>(coords.second - 10),
                                         100.0, 20.0};

    _scene->addItem(cLabel);
}

namespace
{

TraceBasicBlock* getEdgeEndpoint(QTextStream& lineStream)
{
    QString bbStr;
    lineStream >> bbStr;

    const qsizetype colonI = bbStr.indexOf(':');
    assert(colonI != -1);
    bbStr = bbStr.mid(2, colonI - 2);

    bool ok;
    qulonglong from = bbStr.toULongLong(&ok, 16);
    assert(ok);

    return reinterpret_cast<TraceBasicBlock*>(from);
}

} // unnamed namespace

CFGEdge* ControlFlowGraphView::getEdgeFromDot(QTextStream& lineStream, int lineno)
{
    TraceBasicBlock* bbFrom = getEdgeEndpoint(lineStream);
    assert(bbFrom);
    TraceBasicBlock* bbTo = getEdgeEndpoint(lineStream);
    assert(bbTo);

    CFGEdge* edge = _exporter.findEdge(bbFrom, bbTo);
    if (!edge)
    {
        qDebug() << "Unknown edge \'" << bbFrom << "\'-\'" << bbTo << "\' from dot ("
                << _exporter.filename() << ":" << lineno << ")";
    }

    return edge;
}

QPolygon ControlFlowGraphView::getEdgePolygon(QTextStream& lineStream, int lineno)
{
    int nPoints;
    lineStream >> nPoints;
    assert(nPoints > 1);

    QPolygon poly{nPoints};

    for (auto i = 0; i != nPoints; ++i)
    {
        if (lineStream.atEnd())
        {
            qDebug("ControlFlowGraphView: Can not read %d spline nPoints (%s:%d)",
                    nPoints, qPrintable(_exporter.filename()), lineno);
            return QPolygon{};
        }

        const std::pair<int, int> coords = calculateCoords(lineStream);
        poly.setPoint(i, coords.first, coords.second);
    }

    return poly;
}

void ControlFlowGraphView::checkScene()
{
    if (!_scene)
    {
        _scene = new QGraphicsScene;

        if (_exporter.CFGAvailable())
            _scene->addSimpleText(QObject::tr("Error running the graph layouting tool.\n"
                                              "Please check that \'dot\' is installed "
                                              "(package Graphviz)."));
        else
            _scene->addSimpleText(_exporter.errorMessage());

        centerOn(0, 0);
    }
}

void ControlFlowGraphView::centerOnSelectedNodeOrEdge()
{
    CanvasCFGNode* cNode = nullptr;
    if (_selectedNode)
        cNode = _selectedNode->canvasNode();
    else if (_selectedEdge)
    {
        if (_selectedEdge->nodeFrom())
            cNode = _selectedEdge->nodeFrom()->canvasNode();

        if (!cNode && _selectedEdge->nodeTo())
            cNode = _selectedEdge->nodeTo()->canvasNode();
    }

    if (cNode)
    {
        if (_prevSelectedNode)
        {
            if (rect().contains(_prevSelectedPos))
            {
                QPointF wCenter = mapToScene(viewport()->rect().center());
                QPointF prevPos = mapToScene(_prevSelectedPos);
                centerOn(cNode->rect().center() + wCenter - prevPos);
            }
            else
                ensureVisible(cNode);
        }
        else
            centerOn(cNode);
    }
}

void ControlFlowGraphView::zoomPosTriggered(QAction* a)
{
    _zoomPosition = static_cast<ZoomPosition>(a->data().toInt());
    updateSizes();
}

void ControlFlowGraphView::layoutTriggered(QAction* a)
{
    _exporter.setLayout(static_cast<CFGExporter::Layout>(a->data().toInt()));
    refresh(false);
}

void ControlFlowGraphView::minimizationTriggered(QAction* a)
{
    TraceFunction* func = getFunction();
    const uint64 totalCost = func->subCost(_eventType).v;
    const double percentage = a->data().toDouble();

    _exporter.setMinimalCostPercentage(func, percentage);
    _exporter.minimizeBBsWithCostLessThan(totalCost * percentage / 100);
    refresh(false);
}

void ControlFlowGraphView::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    if (_scene)
        updateSizes(e->size());
}

void ControlFlowGraphView::mouseEvent(void (TraceItemView::* func)(CostItem*), QGraphicsItem* item)
{
    assert(item);

    if (item->type() == CanvasParts::Node)
    {
        CFGNode* node = static_cast<CanvasCFGNode*>(item)->node();
        (this->*func)(node->basicBlock());
    }
    else
    {
        if (item->type() == CanvasParts::EdgeLabel)
            item = static_cast<CanvasCFGEdgeLabel*>(item)->canvasEdge();
        else if (item->type() == CanvasParts::EdgeArrow)
            item = static_cast<CanvasCFGEdgeArrow*>(item)->canvasEdge();

        if (item->type() == CanvasParts::Edge)
        {
            CFGEdge* edge = static_cast<CanvasCFGEdge*>(item)->edge();

            if (edge->branch())
                (this->*func)(edge->branch());
        }
    }
}

void ControlFlowGraphView::mousePressEvent(QMouseEvent* event)
{
    setFocus();

    if (event->button() == Qt::LeftButton)
        _isMoving = true;

    QGraphicsItem* item = itemAt(event->pos());
    if (item)
        mouseEvent(&ControlFlowGraphView::selected, item);

    _lastPos = event->pos();
}

void ControlFlowGraphView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QGraphicsItem* item = itemAt(event->pos());
    if (!item)
        return;

    mouseEvent(&ControlFlowGraphView::activated, item);

    if (_selectedNode)
    {
        TraceBasicBlock* bb = _selectedNode->basicBlock();
        _exporter.switchNodeOption(bb, CFGExporter::Options::reduced);
        _exporter.setMinimalCostPercentage(bb->function(), -1);
        refresh(false);
    }

    centerOnSelectedNodeOrEdge();
}

void ControlFlowGraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (_isMoving)
    {
        const QPoint delta = event->pos() - _lastPos;
        zoomRectMoved(-delta.x(), -delta.y());
        _lastPos = event->pos();
    }
}

void ControlFlowGraphView::mouseReleaseEvent(QMouseEvent*)
{
    _isMoving = false;
    if (_zoomPosition == ZoomPosition::Auto)
        updateSizes();
}

namespace
{

enum MenuActions
{
    stopLayout,
    exportAsDot,
    exportAsImage,

    pcOnly,
    showInstrCost,
    showInstrPC,

    pcOnlyGlobal,
    showInstrPCGlobal,
    showInstrCostGlobal,

    // special value
    nActions
};

} // unnamed namespace

void ControlFlowGraphView::contextMenuEvent(QContextMenuEvent* event)
{
    _isMoving = false;

    std::array<QAction*, MenuActions::nActions> actions;

    QMenu popup;
    QGraphicsItem* item = itemAt(event->pos());

    TraceFunction* func = getFunction();

    TraceBasicBlock* bb;
    if (item && item->type() == CanvasParts::Node)
    {
        CFGNode* node = static_cast<CanvasCFGNode*>(item)->node();
        bb = node->basicBlock();

        QMenu* detailsMenu = popup.addMenu(QObject::tr("This basic block"));
        actions[MenuActions::pcOnly] =
                addOptionsAction(detailsMenu, QObject::tr("PC only"), node,
                                 CFGExporter::Options::reduced);

        actions[MenuActions::showInstrPC] =
                addOptionsAction(detailsMenu, QObject::tr("Show instructions\' PC"), node,
                                 CFGExporter::Options::showInstrPC);

        actions[MenuActions::showInstrCost] =
                addOptionsAction(detailsMenu, QObject::tr("Show instructions\' cost"), node,
                                 CFGExporter::Options::showInstrCost);

        popup.addSeparator();
    }
    else
        bb = nullptr;

    actions[MenuActions::stopLayout] = addStopLayoutAction(popup);

    popup.addSeparator();

    actions[MenuActions::pcOnlyGlobal] =
            addOptionsAction(std::addressof(popup), QObject::tr("PC only"), func,
                             CFGExporter::Options::reduced);

    actions[MenuActions::showInstrPCGlobal] =
            addOptionsAction(std::addressof(popup), QObject::tr("Show instructions\' PC"), func,
                             CFGExporter::Options::showInstrPC);

    actions[MenuActions::showInstrCostGlobal] =
            addOptionsAction(std::addressof(popup), QObject::tr("Show instructions\' cost"), func,
                             CFGExporter::Options::showInstrCost);

    popup.addSeparator();

    addMinimizationMenu(popup, func);

    popup.addSeparator();

    QMenu* exportMenu = popup.addMenu(QObject::tr("Export Graph"));
    actions[MenuActions::exportAsDot] = exportMenu->addAction(QObject::tr("As DOT file..."));
    actions[MenuActions::exportAsImage] = exportMenu->addAction(QObject::tr("As Image..."));

    popup.addSeparator();

    addLayoutMenu(popup);
    addZoomPosMenu(popup);

    QAction* action = popup.exec(event->globalPos());
    const auto index = std::distance(actions.begin(),
                                     std::find(actions.begin(), actions.end(), action));

    switch (index)
    {
        case MenuActions::stopLayout:
            stopRendering();
            break;
        case MenuActions::exportAsDot:
        {
            TraceFunction* func = activeFunction();
            if (func)
                CFGExporter::savePrompt(this, func, eventType(), _exporter);
            break;
        }
        case MenuActions::exportAsImage:
            if (_scene)
                exportGraphAsImage();
            break;

        case MenuActions::pcOnly:
            _exporter.switchNodeOption(bb, CFGExporter::Options::reduced);
            _exporter.setMinimalCostPercentage(func, -1);
            refresh(false);
            break;

        case MenuActions::showInstrPC:
            _exporter.switchNodeOption(bb, CFGExporter::Options::showInstrPC);
            refresh(false);
            break;

        case MenuActions::showInstrCost:
            _exporter.switchNodeOption(bb, CFGExporter::Options::showInstrCost);
            refresh(false);
            break;

        case MenuActions::pcOnlyGlobal:

            if (action->isChecked())
            {
                _exporter.setMinimalCostPercentage(func, 10);
                _exporter.setGraphOption(func, CFGExporter::Options::reduced);
            }
            else
            {
                _exporter.setMinimalCostPercentage(func, 0);
                _exporter.resetGraphOption(func, CFGExporter::Options::reduced);
            }

            refresh(false);
            break;

        case MenuActions::showInstrPCGlobal:

            if (action->isChecked())
                _exporter.setGraphOption(func, CFGExporter::Options::showInstrPC);
            else
                _exporter.resetGraphOption(func, CFGExporter::Options::showInstrPC);

            refresh(false);
            break;

        case MenuActions::showInstrCostGlobal:

            if (action->isChecked())
                _exporter.setGraphOption(func, CFGExporter::Options::showInstrCost);
            else
                _exporter.resetGraphOption(func, CFGExporter::Options::showInstrCost);

            refresh(false);
            break;

        default: // practically nActions
            break;
    }
}

void ControlFlowGraphView::exportGraphAsImage()
{
    assert(_scene);

    const QString fileName = QFileDialog::getSaveFileName(this,
                                                          QObject::tr("Export Graph as Image"),
                                                          QString{},
                                                          QObject::tr("Images (*.png *.jpg)"));
    if (!fileName.isEmpty())
    {
        const QRect rect = _scene->sceneRect().toRect();
        QPixmap pix{rect.width(), rect.height()};
        QPainter painter{std::addressof(pix)};
        _scene->render(std::addressof(painter));
        pix.save(fileName);
    }
}

namespace
{

CFGEdge* getEdgeToSelect(CFGNode* node, int key)
{
    assert(node);

    switch (key)
    {
        case Qt::Key_Up:
            return node->keyboardPrevEdge();
        case Qt::Key_Down:
            return node->keyboardNextEdge();
        default:
            return nullptr;
    }
}

std::pair<CFGNode*, CFGEdge*> getNodeOrEdgeToSelect(CFGEdge* edge, int key)
{
    assert(edge);

    switch (key)
    {
        case Qt::Key_Up:
            return std::make_pair(edge->keyboardPrevNode(), nullptr);
        case Qt::Key_Down:
            return std::make_pair(edge->keyboardNextNode(), nullptr);
        case Qt::Key_Left:
            return std::make_pair(nullptr, edge->keyboardPrevEdge());
        case Qt::Key_Right:
            return std::make_pair(nullptr, edge->keyboardNextEdge());
        default:
            return std::make_pair(nullptr, nullptr);
    }
}

} // namespace

void ControlFlowGraphView::keyPressEvent(QKeyEvent* e)
{
    if (!_scene)
        e->ignore();
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Space)
    {
        if (_selectedNode)
            activated(_selectedNode->basicBlock());
        else if (_selectedEdge && _selectedEdge->branch())
            activated(_selectedEdge->branch());
    }
    else if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
    {
        if (_selectedNode)
        {
            CFGEdge* edge = getEdgeToSelect(_selectedNode, e->key());

            if (edge && edge->branch())
                selected(edge->branch());
        }
        else if (_selectedEdge)
        {
            CFGNode* node;
            CFGEdge* edge;
            std::tie(node, edge) = getNodeOrEdgeToSelect(_selectedEdge, e->key());

            if (node && node->basicBlock())
                selected(node->basicBlock());
            else if (edge && edge->branch())
                selected(edge->branch());
        }
    }
    else
        movePointOfView(e);
}

void ControlFlowGraphView::movePointOfView(QKeyEvent* e)
{
    auto dx = [this]{ return mapToScene(width(), 0) - mapToScene(0, 0); };
    auto dy = [this]{ return mapToScene(0, height()) - mapToScene(0, 0); };

    const QPointF center = mapToScene(viewport()->rect().center());
    switch (e->key())
    {
        case Qt::Key_Home:
            centerOn(center + QPointF{-_scene->width(), 0});
            break;
        case Qt::Key_End:
            centerOn(center + QPointF{_scene->width(), 0});
            break;
        case Qt::Key_PageUp:
        {
            const QPointF delta = dy();
            centerOn(center + QPointF(-delta.x() / 2.0, -delta.y() / 2.0));
            break;
        }
        case Qt::Key_PageDown:
        {
            const QPointF delta = dy();
            centerOn(center + QPointF(delta.x() / 2.0, delta.y() / 2.0));
            break;
        }
        case Qt::Key_Left:
        {
            const QPointF delta = dx();
            centerOn(center + QPointF(-delta.x() / 10.0, -delta.y() / 10.0));
            break;
        }
        case Qt::Key_Right:
        {
            const QPointF delta = dx();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Down:
        {
            const QPointF delta = dy();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Up:
        {
            const QPointF delta = dy();
            centerOn(center + QPointF(-delta.x() / 10, -delta.y() / 10));
            break;
        }
        default:
            e->ignore();
            break;
    }
}

// Called by QAbstractScrollArea to notify about scrollbar changes
void ControlFlowGraphView::scrollContentsBy(int dx, int dy)
{
    // call QGraphicsView implementation
    QGraphicsView::scrollContentsBy(dx, dy);

    const QPointF topLeft = mapToScene(QPoint(0, 0));
    const QPointF bottomRight = mapToScene(QPoint(width(), height()));

    _panningView->setZoomRect(QRectF{topLeft, bottomRight});
}

namespace
{

double calculate_zoom(QSize s, int cWidth, int cHeight)
{
    // first, assume use of 1/3 of width/height (possible larger)
    qreal zoom = (s.width() * cHeight < s.height() * cWidth) ? .33 * s.height() / cHeight
                                                             : .33 * s.width()  / cWidth;

    // fit to widget size
    if (cWidth * zoom > s.width())
        zoom = s.width() / static_cast<double>(cWidth);
    if (cHeight * zoom > s.height())
        zoom = s.height() / static_cast<double>(cHeight);

    // scale to never use full height/width
    zoom *= 0.75;

    // at most a zoom of 1/3
    if (zoom > .33)
        zoom = .33;

    return zoom;
}

} // unnamed namespace

void ControlFlowGraphView::updateSizes(QSize s)
{
    if (!_scene)
        return;

    if (s.isNull())
        s = size();

    // the part of the scene that should be visible
    const auto cWidth = static_cast<int>(_scene->width()) - 2*_xMargin + 100;
    const auto cHeight = static_cast<int>(_scene->height()) - 2*_yMargin + 100;

    // hide birds eye view if no overview needed
    if (!_data || !_activeItem ||
        ((cWidth < s.width()) && (cHeight < s.height())) )
    {
        _panningView->hide();
        return;
    }

    const double zoom = calculate_zoom(s, cWidth, cHeight);

    if (zoom != _panningZoom)
    {
        _panningZoom = zoom;

        QTransform m;
        _panningView->setTransform(m.scale(zoom, zoom));

        // make it a little bigger to compensate for widget frame
        _panningView->resize(static_cast<int>(cWidth * zoom) + 4,
                             static_cast<int>(cHeight * zoom) + 4);

        // update ZoomRect in panningView
        scrollContentsBy(0, 0);
    }

    _panningView->centerOn(_scene->width() / 2, _scene->height() / 2);

    const int cvW = _panningView->width();
    const int cvH = _panningView->height();
    const int x = width() - cvW - verticalScrollBar()->width() - 2;
    const int y = height() - cvH - horizontalScrollBar()->height() - 2;

    ZoomPosition zp = _zoomPosition;
    if (zp == ZoomPosition::Auto)
    {
        const auto tlCols = items(QRect(0, 0, cvW, cvH)).count();
        const auto trCols = items(QRect(x, 0, cvW, cvH)).count();
        const auto blCols = items(QRect(0, y, cvW, cvH)).count();
        const auto brCols = items(QRect(x, y, cvW, cvH)).count();
        qsizetype minCols;

        switch (_lastAutoPosition)
        {
            case ZoomPosition::TopRight:
                minCols = trCols;
                break;
            case ZoomPosition::BottomLeft:
                minCols = blCols;
                break;
            case ZoomPosition::BottomRight:
                minCols = brCols;
                break;
            default:
            case ZoomPosition::TopLeft:
                minCols = tlCols;
                break;
        }

        if (minCols > tlCols)
        {
            minCols = tlCols;
            _lastAutoPosition = ZoomPosition::TopLeft;
        }
        if (minCols > trCols)
        {
            minCols = trCols;
            _lastAutoPosition = ZoomPosition::TopRight;
        }
        if (minCols > blCols)
        {
            minCols = blCols;
            _lastAutoPosition = ZoomPosition::BottomLeft;
        }
        if (minCols > brCols)
        {
            minCols = brCols;
            _lastAutoPosition = ZoomPosition::BottomRight;
        }

        zp = _lastAutoPosition;
    }

    QPoint newZoomPos{0, 0};
    switch (zp)
    {
        case ZoomPosition::TopRight:
            newZoomPos = QPoint(x, 0);
            break;
        case ZoomPosition::BottomLeft:
            newZoomPos = QPoint(0, y);
            break;
        case ZoomPosition::BottomRight:
            newZoomPos = QPoint(x, y);
            break;
        case ZoomPosition::TopLeft:
        default:
            break;
    }

    if (newZoomPos != _panningView->pos())
        _panningView->move(newZoomPos);

    if (zp == ZoomPosition::Hide)
        _panningView->hide();
    else
        _panningView->show();
}

CostItem* ControlFlowGraphView::canShow(CostItem* i)
{
    if (i)
    {
        switch (i->type())
        {
            case ProfileContext::Function:
            case ProfileContext::FunctionCycle:
            case ProfileContext::Call:
            case ProfileContext::BasicBlock:
                return i;
            default:
                break;
        }
    }

    return nullptr;
}

void ControlFlowGraphView::doUpdate(int changeType, bool)
{
    if (changeType == TraceItemView::selectedItemChanged)
    {
        if (!_scene || !_selectedItem)
            return;

        switch (_selectedItem->type())
        {
            case ProfileContext::BasicBlock:
            {
                CFGNode* node = _exporter.findNode(static_cast<TraceBasicBlock*>(_selectedItem));
                if (node == _selectedNode)
                    return;

                unselectNode();
                unselectEdge();
                selectNode(node);
                break;
            }

            case ProfileContext::Branch:
            {
                auto branch = static_cast<TraceBranch*>(_selectedItem);
                CFGEdge* edge = _exporter.findEdge(branch->bbFrom(), branch->bbTo());
                if (edge == _selectedEdge)
                    return;

                unselectNode();
                unselectEdge();
                selectEdge(edge);
                break;
            }

            default:
                unselectNode();
                unselectEdge();
                break;
        }

        _scene->update();
    }
    else if (changeType & TraceItemView::dataChanged)
    {
        _exporter.reset(_activeItem, _eventType);
        _selectedNode = nullptr;
        _selectedEdge = nullptr;
        refresh();
    }
    else if (changeType != TraceItemView::eventType2Changed)
        refresh();
}

void ControlFlowGraphView::unselectNode()
{
    if (_selectedNode)
    {
        CanvasCFGNode* cn = _selectedNode->canvasNode();
        if (cn)
            cn->setSelected(false);

        _selectedNode = nullptr;
    }
}

void ControlFlowGraphView::unselectEdge()
{
    if (_selectedEdge)
    {
        CanvasCFGEdge* ce = _selectedEdge->canvasEdge();
        if (ce)
            ce->setSelected(false);

        _selectedEdge = nullptr;
    }
}

void ControlFlowGraphView::selectNode(CFGNode* node)
{
    if (node && node->canvasNode())
    {
        _selectedNode = node;

        CanvasCFGNode* cNode = _selectedNode->canvasNode();
        cNode->setSelected(true);

        if (!_isMoving)
            ensureVisible(cNode);
    }
}

void ControlFlowGraphView::selectEdge(CFGEdge* edge)
{
    if (edge && edge->canvasEdge())
    {
        _selectedEdge = edge;
        _selectedEdge->canvasEdge()->setSelected(true);
    }
}

void ControlFlowGraphView::refresh(bool reset)
{
    if (_renderProcess)
        stopRendering();

    _prevSelectedNode = _selectedNode;
    if (_selectedNode)
        _prevSelectedPos = mapFromScene(_selectedNode->canvasNode()->rect().center());
    else
        _prevSelectedPos = QPoint{-1, -1};

    if (!_data)
    {
        showText(QObject::tr("No trace data"));
        return;
    }
    else if (!_activeItem)
    {
        showText(QObject::tr("No item activated for which to draw the control-flow graph."));
        return;
    }

    switch (_activeItem->type())
    {
        case ProfileContext::BasicBlock:
        case ProfileContext::Branch:
        case ProfileContext::Function:
        case ProfileContext::Call:
            break;
        default:
            showText(QObject::tr("No control-flow graph can be drawn for the active item."));
            return;
    }

    _selectedNode = nullptr;
    _selectedEdge = nullptr;

    if (_scene)
        _scene->clear();
    _unparsedOutput.clear();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(1000);

    _renderProcess = new QProcess{this};

    connect(_renderProcess, &QProcess::readyReadStandardOutput,
            this, &ControlFlowGraphView::readDotOutput);

    connect(_renderProcess, &QProcess::errorOccurred,
            this, &ControlFlowGraphView::dotError);

    static constexpr auto finishedPtr =
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    connect(_renderProcess, finishedPtr,
            this, &ControlFlowGraphView::dotExited);

    const QString renderProgram = QStringLiteral("dot");
    QStringList renderArgs{QStringLiteral("-Tplain-ext")};

    _renderProcessCmdLine = renderProgram + QLatin1Char(' ') + renderArgs.join(QLatin1Char(' '));

    qDebug() << "ControlFlowGraphView::refresh: Starting process "
             << _renderProcess << ", \'" << _renderProcess << "\'";

    QProcess* process = _renderProcess;
    process->start(renderProgram, renderArgs);
    if (reset)
        _exporter.reset(_selectedItem ? _selectedItem : _activeItem, _eventType);
    _exporter.writeDot(process);
    process->closeWriteChannel();
}

void ControlFlowGraphView::clear()
{
    if (!_scene)
        return;

    _panningView->setScene(nullptr);
    setScene(nullptr);
    delete _scene;
    _scene = nullptr;
}

void ControlFlowGraphView::showText(const QString& text)
{
    clear();
    _renderTimer.stop();

    _scene = new QGraphicsScene;
    _scene->addSimpleText(text);

    centerOn(0, 0);
    setScene(_scene);
    _scene->update();
    _panningView->hide();
}

QAction* ControlFlowGraphView::addZoomPosAction(QMenu* menu, const QString& descr, ZoomPosition pos)
{
    QAction* a = menu->addAction(descr);

    a->setData(static_cast<int>(pos));
    a->setCheckable(true);
    a->setChecked(_zoomPosition == pos);

    return a;
}

QAction* ControlFlowGraphView::addLayoutAction(QMenu* menu, const QString& descr,
                                               CFGExporter::Layout layout)
{
    QAction* a = menu->addAction(descr);

    a->setData(static_cast<int>(layout));
    a->setCheckable(true);
    a->setChecked(_exporter.layout() == layout);

    return a;
}

QAction* ControlFlowGraphView::addStopLayoutAction(QMenu& menu)
{
    if (_renderProcess)
    {
        QAction* stopLayout_ = menu.addAction(QObject::tr("Stop Layouting"));
        menu.addSeparator();

        return stopLayout_;
    }
    else
        return nullptr;
}

QAction* ControlFlowGraphView::addOptionsAction(QMenu* menu, const QString& descr, CFGNode* node,
                                                CFGExporter::Options option)
{
    CFGExporter::Options graphOptions = _exporter.getNodeOptions(node->basicBlock());

    QAction* a = menu->addAction(descr);

    a->setData(option);
    a->setCheckable(true);

    if ((graphOptions & CFGExporter::Options::reduced) &&
        (option & (CFGExporter::Options::showInstrPC | CFGExporter::Options::showInstrCost)))
        a->setEnabled(false);

    a->setChecked(graphOptions & option);

    return a;
}

QAction* ControlFlowGraphView::addOptionsAction(QMenu* menu, const QString& descr, TraceFunction* func,
                                                CFGExporter::Options option)
{
    CFGExporter::Options graphOptions = _exporter.getGraphOptions(func);

    QAction* a = menu->addAction(descr);

    a->setData(option);
    a->setCheckable(true);

    if ((graphOptions & CFGExporter::Options::reduced) &&
        (option & (CFGExporter::Options::showInstrPC | CFGExporter::Options::showInstrCost)))
        a->setEnabled(false);

    a->setChecked(graphOptions & option);

    return a;
}

QAction* ControlFlowGraphView::addMinimizationAction(QMenu* menu, const QString& descr,
                                                     TraceFunction* func, double percentage)
{
    QAction* a = menu->addAction(descr);

    a->setData(percentage);
    a->setCheckable(true);
    a->setChecked(percentage == _exporter.minimalCostPercentage(func));
    if (percentage == -1)
        a->setEnabled(false);

    return a;
}

QMenu* ControlFlowGraphView::addZoomPosMenu(QMenu& menu)
{
    QMenu* submenu = menu.addMenu(QObject::tr("Birds-eye View"));

    addZoomPosAction(submenu, QObject::tr("Top Left"), ZoomPosition::TopLeft);
    addZoomPosAction(submenu, QObject::tr("Top Right"), ZoomPosition::TopRight);
    addZoomPosAction(submenu, QObject::tr("Bottom Left"), ZoomPosition::BottomLeft);
    addZoomPosAction(submenu, QObject::tr("Bottom Right"), ZoomPosition::BottomRight);
    addZoomPosAction(submenu, QObject::tr("Automatic"), ZoomPosition::Auto);
    addZoomPosAction(submenu, QObject::tr("Hide"), ZoomPosition::Hide);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::zoomPosTriggered);

    return submenu;
}

QMenu* ControlFlowGraphView::addLayoutMenu(QMenu& menu)
{
    QMenu* submenu = menu.addMenu(QObject::tr("Layout"));

    addLayoutAction(submenu, QObject::tr("Top to Down"), CFGExporter::Layout::TopDown);
    addLayoutAction(submenu, QObject::tr("Left to Right"), CFGExporter::Layout::LeftRight);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::layoutTriggered);

    return submenu;
}

QMenu* ControlFlowGraphView::addMinimizationMenu(QMenu& menu, TraceFunction* func)
{
    QMenu* submenu = menu.addMenu(QObject::tr("Min. basic block cost"));

    addMinimizationAction(submenu, QObject::tr("Undefined"), func, -1);
    submenu->addSeparator();
    for (auto percentage : {0.0, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0})
        addMinimizationAction(submenu, QObject::tr("%1%").arg(percentage), func, percentage);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::minimizationTriggered);

    return submenu;
}
