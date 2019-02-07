/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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

#ifndef QCORE_MANAGER_EDITOR_H
#define QCORE_MANAGER_EDITOR_H

#include "qcore/manager.h"
#include "qtools/graph.h"

//=============
// HandlerNode
//=============

class HandlerGraphEditor;

class HandlerNode : public Node {

public:
    HandlerNode(Handler* handler, HandlerGraphEditor* parent);

    Handler* handler() const;
    HandlerGraphEditor* parent() const;

    void updateLabel();

private:
    Handler* mHandler;
    HandlerGraphEditor* mParent;

};

//=============
// EdgeWrapper
//=============

class EdgeWrapper : public Edge {

public:
    EdgeWrapper(HandlerNode* sender, HandlerNode* receiver);

    HandlerNode* sender() const;
    HandlerNode* receiver() const;
    HandlerGraphEditor* parent() const;

    void setFilter(Filter filter);
    void setVisibility(Filter::match_type match);

    void updateVisibility(Handler* source);
    void updateVisibility();

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    Filter mFilter;

};

//====================
// HandlerGraphEditor
//====================

class HandlerGraphEditor : public QWidget {

    Q_OBJECT

    Q_PROPERTY(QBrush nodeColor READ nodeColor WRITE setNodeColor)
    Q_PROPERTY(QBrush nodeBackgroundColor READ nodeBackgroundColor WRITE setNodeBackgroundColor)
    Q_PROPERTY(QBrush nodeAlternateBackgroundColor READ nodeAlternateBackgroundColor WRITE setNodeAlternateBackgroundColor)

public:
    explicit HandlerGraphEditor(Manager* manager, QWidget* parent);

    Graph* graph();

    QBrush nodeColor() const;
    void setNodeColor(const QBrush& brush);

    QBrush nodeBackgroundColor() const;
    void setNodeBackgroundColor(const QBrush& brush);

    QBrush nodeAlternateBackgroundColor() const;
    void setNodeAlternateBackgroundColor(const QBrush& brush);

    HandlerNode* getNode(Handler* handler);
    EdgeWrapper* getEdge(Handler* tail, Handler* head);
    EdgeWrapper* getEdge(HandlerNode* tail, HandlerNode* head);

public slots:
    void forwardEdgeRemoval(EdgeWrapper* edge);

protected slots:
    // mapper changes
    void renameHandler(Handler* handler);
    void insertHandler(Handler* handler);
    void removeHandler(Handler* handler);
    void updateListeners(Handler* handler);
    // callbacks
    void forwardEdgeCreation(Node* tail, Node* head);
    // others
    void updateEdgeVisibility(EdgeWrapper* edge);
    void updateEdgesVisibility();

private:
    Graph* mGraph;
    QCheckBox* mFilter;
    HandlerSelector* mSelector;
    QMap<Handler*, HandlerNode*> mNodes;
    QBrush mNodeColor {Qt::black};
    QBrush mNodeBackgroundColor;
    QBrush mNodeAlternateBackgroundColor;
    Manager* mManager;

};

//===================
// HandlerListEditor
//===================

class HandlerListEditor : public QWidget {

    Q_OBJECT

public:
    explicit HandlerListEditor(Manager* manager, QWidget* parent);

    QSize sizeHint() const override;

private slots:
    void insertHandler(Handler* handler);
    void renameHandler(Handler* handler);
    void removeHandler(Handler* handler);
    void refreshHandler(Handler* handler);
    void onMessageHandled(Handler* handler, const Message& message);

    void showMenu(const QPoint& point);
    void onItemChange(QTreeWidgetItem* item, int column);
    void onVisibilityChange(int index);

    void destroySelection();
    void editSelection();
    void renameSelection();
    void resetSelectionParameters();
    void refreshSelectionParameters();
    void sendToSelection(HandlerProxy::Command command, Handler::State state);

private:
    QMenu* addCommandMenu(const QIcon &icon, const QString& title, HandlerProxy::Command command);
    void updateParameter(QTreeWidgetItem* parent, const HandlerView::Parameter& parameter, MetaHandler* metaHandler);
    void addParameter(QTreeWidgetItem* parent, const HandlerView::Parameter& parameter, MetaHandler* metaHandler);
    std::set<Handler*> selectedHandlers();

private:
    QTreeWidget* mTree;
    QComboBox* mVisibilityBox;
    QMenu* mMenu;
    QAction* mRenameAction;
    Manager* mManager;

};

//======================
// HandlerCatalogEditor
//======================

class HandlerCatalogEditor : public QWidget {

    Q_OBJECT

public:
    explicit HandlerCatalogEditor(Manager* manager, QWidget* parent);

    QSize sizeHint() const override;

private slots:
    void showMenu(const QPoint& point);
    void onLineReturn();
    void onDoubleClick(QTreeWidgetItem* item, int column);

    void refreshMeta(QTreeWidgetItem* item, const QStringList& instantiables);
    void createHandler(MetaHandler* metaHandler, const QString& fixedName);

private:
    MetaHandler* metaHandlerForItem(QTreeWidgetItem* item);

private:
    QTreeWidget* mTreeWidget;
    QLineEdit* mLineEdit;
    Manager* mManager;

};

#endif // QCORE_MANAGER_EDITOR_H
