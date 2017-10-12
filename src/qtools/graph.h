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

#ifndef QTOOLS_GRAPH_H
#define QTOOLS_GRAPH_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsItem>
#include <QList>

class Node;
class Edge;
class Bundler;
class Graph;
class QGraphicsSceneMouseEvent;

//===========
// GraphItem
//===========

class GraphItem : public QGraphicsItemGroup {

public:
    GraphItem(Graph* graph);

    QRectF enclosingRect() const;

    QList<Node*> getNodes() const;
    QList<Edge*> getEdges() const;

    void insertNode(Node* node);
    void deleteNode(Node* node);
    void insertEdge(Edge* edge);
    void deleteEdge(Edge* edge);

    void requestEdgeCreation(Node* tail, Node* head);

    // global ?
    Node* transitive();
    void setTransitive(Node* node);

    enum { Type = UserType + 4 };
    int type() const override { return Type; }

private:
    void deleteChild(QGraphicsItem* item);

    Node* mTransitive;
    Graph* mGraphWidget;

};

//=======
// Graph
//=======

class Graph : public QGraphicsView {

    Q_OBJECT

public:
    explicit Graph(QWidget* parent);

    QSize sizeHint() const override;

public slots:

    QList<Node*> getNodes();
    QList<Edge*> getEdges();

    void insertNode(Node* node);
    void deleteNode(Node* node);
    void insertEdge(Edge* edge);
    void deleteEdge(Edge* edge);
    void doLayout(); /*!< automatically arranges nodes (poor design so far) */
    void centerOnScene();

signals:
    void edgeCreation(Node* tail, Node* head);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    GraphItem* mRoot;

};

//======
// Edge
//======

class Edge : public QGraphicsItem {

public:

    enum ArrowPolicy {
        NO_ARROW,
        ORIENTED_ARROW, /*!< draw from tail to head */
        FULL_ARROW /*!< draw both sides */
    };

    Edge(ArrowPolicy policy = ORIENTED_ARROW);
    Edge(Node* tail, Node* head, ArrowPolicy policy = ORIENTED_ARROW);

    GraphItem* graph();

    ArrowPolicy arrowPolicy() const;
    void setArrowPolicy(ArrowPolicy policy);

    const QColor& arrowColor() const;
    void setArrowColor(const QColor& color);

    const QString& label() const;
    void setLabel(const QString& label);

    const QVector<QPointF>& controlPoints() const;
    void setControlPoints();
    void setControlPoints(const QPointF& p1);
    void setControlPoints(const QPointF& p1, const QPointF& p2);

    Node* tail() const;
    Node* head() const;
    bool isLinked() const;
    void setLink(Node* tail, Node* head);
    void breakLink();

    void updateShape();

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QPainterPath shape() const override;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void rawUpdateShape();
    QPointF nodeCenter(Node* node);
    QPointF computeIntersection(Node* node, const QPointF& anchor);
    QPolygonF computeArrow(const QPointF& pt, const QPointF& origin);

    // settings
    Node* mTail;
    Node* mHead;
    QString mLabel;
    ArrowPolicy mPolicy;
    QColor mColor;
    qreal mArrowSize;
    QVector<QPointF> mControlPoints;
    // internal
    QRectF mLabelRect;
    QPainterPath mPath;
    QPolygonF mTailArrow;
    QPolygonF mHeadArrow;
};

//===========
// BasicNode
//===========

class BasicNode : public QGraphicsItem {

public:
    explicit BasicNode(GraphItem* graph);

    GraphItem* graph();
    const QList<Edge*>& edges() const;
    virtual void setWidth(int width) = 0;

protected:
    friend class Edge;
    void addEdge(Edge* edge);
    void remEdge(Edge* edge);

protected:
    void updateEdges();
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QList<Edge*> mEdges;

};

//======
// Node
//======

class Node : public BasicNode {

public:
    static const QString transitiveMime;

    Node(const QString& label, GraphItem* graph);

    void setWidth(int width) override;

    QSize size() const;
    void setSize(const QSize& size);

    QBrush color() const;
    void setColor(const QBrush& brush);

    QBrush backgroundColor() const;
    void setBackgroundColor(const QBrush& brush);

    QBrush alternateBackgroundColor() const;
    void setAlternateBackgroundColor(const QBrush& brush);

    const QString& label() const;
    void setLabel(const QString& label);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void setConnecting(bool status);
    void catchNode(QGraphicsSceneMouseEvent* event);
    void changeNode(Node* node);

    QString mLabel;
    QRect mRect;
    bool mIsConnecting;
    Node* mLastNode;
    QBrush mColor;
    QBrush mBackgroundColor;
    QBrush mAlternateBackgroundColor;

};

//=========
// Bundler
//=========

class Bundler : public BasicNode {

public:
    explicit Bundler(GraphItem* graph);

    void addNode(BasicNode* node);
    void remNode(BasicNode* node);

    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent*) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent*) override;
    void dropEvent(QGraphicsSceneDragDropEvent*) override;

    void setWidth(int width) override;

private:
    QSizeF enclosingSize() const;

private:
    bool mHighlight;
    QSizeF mMinimumSize;
    QList<BasicNode*> mNodes;

};

#endif // QTOOLS_GRAPH_H
