/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017 Julien Berthault

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include "graph.h"

#include <QKeyEvent>
#include <QtMath>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QFontMetrics>
#include <QDrag>
#include <QMimeData>
#include <QKeyEvent>
#include "tools/trace.h"

//===========
// GraphItem
//===========

GraphItem::GraphItem(Graph* graph) : QGraphicsItemGroup(), mGraphWidget(graph) {
    setHandlesChildEvents(false);
}

QRectF GraphItem::enclosingRect() const {
    QRectF rect;
    for (QGraphicsItem* item : childItems())
        if (item->type() == Node::Type)
            rect |= item->mapRectToParent(item->boundingRect());
    return rect;
}

QList<Node*> GraphItem::getNodes() const {
    QList<Node*> nodes;
    for (QGraphicsItem* item : childItems())
        if (item->type() == Node::Type)
            nodes.append(static_cast<Node*>(item));
    return nodes;
}

QList<Edge*> GraphItem::getEdges() const {
    QList<Edge*> edges;
    for (QGraphicsItem* item : childItems())
        if (item->type() == Edge::Type)
            edges.append(static_cast<Edge*>(item));
    return edges;
}

void GraphItem::insertNode(Node* node) {
    if (node != nullptr)
        addToGroup(node);
}

void GraphItem::deleteNode(Node* node) {
    for (Edge* edge : node->edges())
        deleteEdge(edge);
    deleteChild(node);
}

void GraphItem::insertEdge(Edge* edge) {
    addToGroup(edge);
    edge->updateShape();
}

void GraphItem::deleteEdge(Edge* edge) {
    edge->breakLink();
    deleteChild(edge);
}

void GraphItem::requestEdgeCreation(Node* tail, Node* head) {
    emit mGraphWidget->edgeCreation(tail, head);
}

Node* GraphItem::transitive() {
    return mTransitive;
}

void GraphItem::setTransitive(Node* node) {
    mTransitive = node;
}

void GraphItem::deleteChild(QGraphicsItem* item) {
    removeFromGroup(item);
    scene()->removeItem(item);
    delete item;
}

//=======
// Graph
//=======

Graph::Graph(QWidget* parent) : QGraphicsView(parent) {

    QGraphicsScene* gscene = new QGraphicsScene(this);
    gscene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(gscene);

    mRoot = new GraphItem(this);
    gscene->addItem(mRoot);

    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
}

QSize Graph::sizeHint() const {
    return {400, 400};
}

QList<Node*> Graph::getNodes() {
    return mRoot->getNodes();
}

QList<Edge*> Graph::getEdges() {
    return mRoot->getEdges();
}

void Graph::insertNode(Node* node) {
    mRoot->insertNode(node);
}

void Graph::deleteNode(Node* node) {
    mRoot->deleteNode(node);
}

void Graph::insertEdge(Edge* edge) {
    mRoot->insertEdge(edge);
}

void Graph::deleteEdge(Edge* edge) {
    mRoot->deleteEdge(edge);
}

void Graph::wheelEvent(QWheelEvent* event) {
    qreal raw_factor = qPow(2., event->delta() / 240.0);
    qreal factor = transform().scale(raw_factor, raw_factor).mapRect(QRectF(0, 0, 1, 1)).width();
    if (.07 <= factor && factor <= 100)
        scale(raw_factor, raw_factor);
}

void Graph::centerOnScene() {
    setSceneRect(mRoot->enclosingRect());
}

void Graph::doLayout() {
    // layout parameters
    uint maxStack = 2;
    QPointF delta(10, 20);
    // algorithm
    qreal currentWidth = 0;
    QPointF position;
    uint currentStack = 0;
    for (QGraphicsItem* item : mRoot->childItems()) {
        if (item->type() == Node::Type) {
            QSizeF nodeSize = item->boundingRect().size();
            if (currentStack++ == maxStack) {
                position.rx() += currentWidth + delta.x();
                position.ry() = 0;
                currentStack = 1;
                currentWidth = 0;
            }
            item->setPos(position);
            currentWidth = qMax(currentWidth, nodeSize.width());
            position.ry() += nodeSize.height() + delta.y();
        }
    }
    centerOnScene();
}

//======
// Edge
//======

Edge::Edge(ArrowPolicy policy) :
    mTail(nullptr), mHead(nullptr), mPolicy(policy), mColor(Qt::black), mArrowSize(5) {

    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(-1);
}

Edge::Edge(Node* tail, Node* head, ArrowPolicy policy) : Edge(policy) {
    setLink(tail, head);
}

GraphItem* Edge::graph() {
    return qgraphicsitem_cast<GraphItem*>(parentItem());
}

Edge::ArrowPolicy Edge::arrowPolicy() const {
    return mPolicy;
}

void Edge::setArrowPolicy(ArrowPolicy policy) {
    prepareGeometryChange();
    mPolicy = policy;
    rawUpdateShape();
}

const QColor& Edge::arrowColor() const {
    return mColor;
}

void Edge::setArrowColor(const QColor& color) {
    mColor = color;
    update();
}

const QString& Edge::label() const {
    return mLabel;
}

void Edge::setLabel(const QString& label) {
    static const qreal extra_x = 2;
    prepareGeometryChange();
    mLabel = label;
    QFont font;
    QFontMetrics fm(font);
    QRect labelRect = fm.boundingRect(QRect(), Qt::AlignCenter | Qt::TextWordWrap, mLabel);
    labelRect.adjust(-extra_x, 0, extra_x, 0);
    mLabelRect = labelRect;
    rawUpdateShape();
}

const QVector<QPointF>& Edge::controlPoints() const {
    return mControlPoints;
}

void Edge::setControlPoints() {
    prepareGeometryChange();
    mControlPoints.clear();
    rawUpdateShape();
}

void Edge::setControlPoints(const QPointF& p1) {
    prepareGeometryChange();
    mControlPoints.clear();
    mControlPoints << p1;
    rawUpdateShape();
}

void Edge::setControlPoints(const QPointF& p1, const QPointF& p2) {
    prepareGeometryChange();
    mControlPoints.clear();
    mControlPoints << p1 << p2;
    rawUpdateShape();
}

Node* Edge::tail() const {
    return mTail;
}

Node* Edge::head() const {
    return mHead;
}

bool Edge::isLinked() const {
    return mTail && mHead;
}

void Edge::setLink(Node* tail, Node* head) {
    if (isLinked()) {
        TRACE_WARNING("edge already linked");
        return;
    }
    prepareGeometryChange();
    mTail = tail;
    mTail->addEdge(this);
    mHead = head;
    mHead->addEdge(this);
    rawUpdateShape();
}

void Edge::breakLink() {
    prepareGeometryChange();
    if (mTail != nullptr) {
        mTail->remEdge(this);
        mTail = nullptr;
    }
    if (mHead != nullptr) {
        mHead->remEdge(this);
        mHead = nullptr;
    }
    rawUpdateShape();
}

QRectF Edge::boundingRect() const {
    if (!isLinked())
        return {};
    qreal penWidth = 5;
    qreal margin = (penWidth + mArrowSize) / 2.0;
    QRectF pathRect = mPath.controlPointRect().adjusted(-margin, -margin, margin, margin);
    return pathRect | mLabelRect;
}

QPainterPath Edge::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(5);
    return stroker.createStroke(mPath);
}

void Edge::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    if (isLinked()) {
        QBrush brush(mColor);
        painter->setPen(QPen(brush, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(mPath);
        painter->setBrush(brush);
        // painter->setFont();
        if (mPolicy != NO_ARROW)
            painter->drawPolygon(mHeadArrow);
        if (mPolicy == FULL_ARROW)
            painter->drawPolygon(mTailArrow);
        if (!mLabel.isEmpty()) {
            painter->drawText(mLabelRect, Qt::AlignCenter | Qt::TextWordWrap, mLabel);
        }
    }
}

void Edge::rawUpdateShape() {
    if (!isLinked()) {
        /// clear path
        mPath = QPainterPath();
        mLabelRect = QRectF();
    } else {
        int nExtras = mControlPoints.size();
        /// compute anchor points
        QPointF tailAnchor = nExtras == 0 ? nodeCenter(mTail) : mControlPoints.front();
        QPointF headAnchor = nExtras == 0 ? nodeCenter(mHead) : mControlPoints.back();
        /// update extremity points
        QPointF tailPoint = computeIntersection(mTail, headAnchor);
        QPointF headPoint = computeIntersection(mHead, tailAnchor);
        /// update arrows
        if (mPolicy != NO_ARROW)
            mHeadArrow = computeArrow(headPoint, tailAnchor);
        if (mPolicy == FULL_ARROW)
            mTailArrow = computeArrow(tailPoint, headAnchor);
        /// update path
        mPath = QPainterPath(tailPoint);
        switch (nExtras) {
        case 0: mPath.lineTo(headPoint); break;// straight
        case 1: mPath.quadTo(mControlPoints.front(), headPoint); break; // quad
        case 2: mPath.cubicTo(mControlPoints.front(), mControlPoints.back(), headPoint); break; // cubic
        default: TRACE_WARNING("unable to process more than two control points"); break;
        }
        // center label on the path
        QPointF center = mPath.pointAtPercent(.5);
        mLabelRect.moveCenter(center);
    }
}

void Edge::updateShape() {
    prepareGeometryChange();
    rawUpdateShape();
}

QPointF Edge::nodeCenter(Node* node) {
    return mapFromItem(node, node->boundingRect().center());
}

QPointF Edge::computeIntersection(Node* node, const QPointF& anchor) {
    bool intersected = false;
    QPointF result;
    QRectF rect = node->boundingRect();
    QPolygonF polygon = mapFromItem(node, rect);
    QPointF center = mapFromItem(node, rect.center());
    QLineF line(center, anchor);
    for (int i=0 ; i < polygon.size() && !intersected; ++i) {
        QLineF currentLine(polygon[i], polygon[(i+1) % polygon.size()]);
        intersected = line.intersect(currentLine, &result) == QLineF::BoundedIntersection;
    }
    return intersected ? result : center;
}

QPolygonF Edge::computeArrow(const QPointF& pt, const QPointF& origin) {
    QPolygonF points;
    points << pt;
    QLineF line(pt, origin);
    if (!qFuzzyIsNull(line.length())) {
        static const double pi = 3.14159265358979323846264338327950288419717;
        double angle = ::acos(line.dx() / line.length());
        if (line.dy() >= 0)
            angle = 2*pi - angle;
        for (int i=0 ; i < 2 ; ++i) {
            angle += pi/3;
            points << pt + mArrowSize * QPointF(sin(angle), cos(angle));
        }
    }
    return points;
}

void Edge::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton)
        setControlPoints(event->pos());
    QGraphicsItem::mouseMoveEvent(event);
}

//===========
// BasicNode
//===========

BasicNode::BasicNode(GraphItem* graph) : QGraphicsItem(graph) {
    setFlag(ItemIsMovable);
    setFlag(ItemIsFocusable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
}

GraphItem* BasicNode::graph() {
    return qgraphicsitem_cast<GraphItem*>(parentItem());
}

void BasicNode::addEdge(Edge* edge) {
    mEdges.append(edge);
}

void BasicNode::remEdge(Edge* edge) {
    mEdges.removeAll(edge);
}

const QList<Edge*>& BasicNode::edges() const {
    return mEdges;
}

void BasicNode::updateEdges() {
    for (Edge* edge : mEdges)
        edge->updateShape();
}

QVariant BasicNode::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged)
        updateEdges();
    if (change == ItemParentHasChanged)
        {}
    return QGraphicsItem::itemChange(change, value);
}

//======
// Node
//======

const QString Node::transitiveMime = "bin/transitive_node";

Node::Node(const QString& label, GraphItem* graph) :
    BasicNode(graph), mIsConnecting(false), mLastNode(nullptr), mColor(Qt::black) {

    setLabel(label);
}

const QString& Node::label() const {
    return mLabel;
}

void Node::setLabel(const QString& label) {
    mLabel = label;
    QFont font;
    QFontMetrics fm(font);
    QSize size;
    if (!mLabel.isEmpty()) {
        static const int margin = 2;
        QRect rect = fm.boundingRect(QRect(), Qt::AlignCenter | Qt::TextWordWrap, mLabel);
        rect.adjust(-margin, -margin, margin, margin);
        size = rect.size();
    }
    setSize(size);
    update();
}

void Node::setWidth(int width) {
    setSize(QSize(width, mRect.height()));
}

QSize Node::size() const {
    return mRect.size();
}

void Node::setSize(const QSize& size) {
    static const QSize minimumSize(20, 20);
    prepareGeometryChange();
    mRect.setSize(size.expandedTo(minimumSize));
    updateEdges();
}

QBrush Node::color() const {
    return mColor;
}

void Node::setColor(const QBrush& brush) {
    mColor = brush;
    update();
}

QBrush Node::backgroundColor() const {
    return mBackgroundColor;
}

void Node::setBackgroundColor(const QBrush& brush) {
    mBackgroundColor = brush;
    update();
}

QBrush Node::alternateBackgroundColor() const {
    return mAlternateBackgroundColor;
}

void Node::setAlternateBackgroundColor(const QBrush& brush) {
    mAlternateBackgroundColor = brush;
    update();
}

QRectF Node::boundingRect() const {
    static const int margin = 1;
    return mRect.adjusted(-margin, -margin, margin, margin);
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    // rect with 0 width border and "background color"
    painter->setBrush(mIsConnecting ? mAlternateBackgroundColor : mBackgroundColor);
    painter->setPen(QPen(Qt::black, 0));
    painter->drawRect(mRect);
    // aligned text with "color"
    painter->setPen(QPen(mColor, 0));
    painter->drawText(mRect, Qt::AlignCenter | Qt::TextWordWrap, mLabel);
}

void Node::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (event->buttons() & Qt::RightButton)
        catchNode(event);
    QGraphicsItem::mouseMoveEvent(event);
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::RightButton)
        setConnecting(true);
    QGraphicsItem::mousePressEvent(event);
}

void Node::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    QMimeData* data = new QMimeData;
    data->setText(transitiveMime);
    if (graph())
        graph()->setTransitive(this);
    QDrag* drag = new QDrag(event->widget());
    // drag->setPixmap();
    // drag->setHotSpot(mapFromGlobal(QCursor::pos()));
    drag->setMimeData(data);
    if (drag->exec() == Qt::IgnoreAction) {
        // remove bundle
    }
    if (graph())
        graph()->setTransitive(nullptr);
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        /// catchNode(event); : useless
        if (mLastNode != nullptr && graph())
            graph()->requestEdgeCreation(this, mLastNode);
        changeNode(nullptr);
        setConnecting(false);
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void Node::setConnecting(bool status) {
    mIsConnecting = status;
    update();
}

void Node::catchNode(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem* item = scene()->itemAt(event->scenePos(), sceneTransform());
    changeNode(qgraphicsitem_cast<Node*>(item));
}

void Node::changeNode(Node* node) {
    if (mLastNode != node && node != this) {
        if (mLastNode != nullptr)
            mLastNode->setConnecting(false);
        mLastNode = node;
        if (mLastNode != nullptr)
            mLastNode->setConnecting(true);
    }
}

//=========
// Bundler
//=========

Bundler::Bundler(GraphItem* graph) : BasicNode(graph), mHighlight(false), mMinimumSize(50, 50) {
    setAcceptDrops(true);
}

void Bundler::addNode(BasicNode* node) {
    if (mNodes.contains(node)) {
        TRACE_DEBUG("node is already binded to the bundle");
        return;
    }
    prepareGeometryChange();
    node->setParentItem(this);
    node->setFlag(ItemIsMovable, false);
    mNodes.append(node);
    setWidth((int)enclosingSize().width());
}

void Bundler::remNode(BasicNode* node) {
    mNodes.removeAll(node);
}

void Bundler::dragEnterEvent(QGraphicsSceneDragDropEvent*) {
    mHighlight = true;
    update();
}

void Bundler::dragLeaveEvent(QGraphicsSceneDragDropEvent*) {
    mHighlight = false;
    update();
}

void Bundler::dropEvent(QGraphicsSceneDragDropEvent* dropEvent) {
    mHighlight = false;
    const QMimeData* data = dropEvent->mimeData();
    if (data->text() != Node::transitiveMime) {
        dropEvent->ignore();
        update();
    } else if (graph()) {
        Node* node = graph()->transitive();
        if (node)
            addNode(node);
    }
}

QRectF Bundler::boundingRect() const {
    return {QPointF(), enclosingSize().expandedTo(mMinimumSize)};
}

QSizeF Bundler::enclosingSize() const {
    qreal maxWidth=0, totalHeight=0;
    for (BasicNode* node : mNodes) {
        qreal nodeWidth = node->boundingRect().width();
        qreal nodeHeight = node->boundingRect().height();
        if (nodeWidth > maxWidth)
            maxWidth = nodeWidth;
        totalHeight += nodeHeight + 1;
    }
    return {maxWidth, totalHeight};
}

void Bundler::setWidth(int width) {
    for (BasicNode* node : mNodes)
        node->setWidth(width);
}

void Bundler::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    if (!mHighlight)
        painter->setPen(QPen(Qt::gray, 2, Qt::SolidLine));
    else
        painter->setPen(QPen(Qt::blue, 4, Qt::SolidLine));
    painter->drawRect(boundingRect());
    qreal anchor = 0;
    for (BasicNode* node : mNodes) {
        node->setPos(0, anchor);
        qreal nodeHeight = node->boundingRect().height();
        anchor += nodeHeight + 1;
    }
}
