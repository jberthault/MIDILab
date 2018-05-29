/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2018 Julien Berthault

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
    explicit HandlerGraphEditor(QWidget* parent);

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
    QBrush mNodeColor;
    QBrush mNodeBackgroundColor;
    QBrush mNodeAlternateBackgroundColor;

};

//===================
// HandlerListEditor
//===================

class HandlerListEditor : public QTreeWidget {

    Q_OBJECT

public:
    static const int nameColumn = 0;
    static const int forwardColumn = 1;
    static const int receiveColumn = 2;

    explicit HandlerListEditor(QWidget* parent);

public slots:
    Handler* handlerForItem(QTreeWidgetItem* item);

    QTreeWidgetItem* insertHandler(Handler* handler);
    void renameHandler(Handler* handler);
    void removeHandler(Handler* handler);
    void updateHandler(Handler* handler);
    void onMessageHandled(Handler* handler, const Message& message);

protected slots:
    void onDoubleClick(QTreeWidgetItem* item, int column);
    void showMenu(const QPoint& point);

    void destroySelection();
    void openSelection();
    void closeSelection();
    void toggleSelection();
    void editSelection();
    void renameSelection();

    std::set<Handler*> selectedHandlers();

private:
    QMap<Handler*, QTreeWidgetItem*> mItems;
    QMenu* mMenu;
    QAction* mRenameAction;

};

//======================
// HandlerCatalogEditor
//======================

class HandlerCatalogEditor : public QTreeWidget {

    Q_OBJECT

public:
    explicit HandlerCatalogEditor(QWidget* parent);

protected slots:
    void showMenu(const QPoint& point);
    void onDoubleClick(QTreeWidgetItem* item, int column);

    void refreshMeta(QTreeWidgetItem* item, const QStringList& instantiables);
    void createHandler(MetaHandler* metaHandler, const QString& fixedName);

    MetaHandler* metaHandlerForItem(QTreeWidgetItem* item);

};

//===============
// ManagerEditor
//===============

class ManagerEditor : public QTabWidget {

    Q_OBJECT

public:
    explicit ManagerEditor(QWidget* parent);

    HandlerListEditor* listEditor();
    HandlerGraphEditor* graphEditor();
    HandlerCatalogEditor* catalogEditor();

private:
    HandlerGraphEditor* mGraphEditor;
    HandlerListEditor* mListEditor;
    HandlerCatalogEditor* mCatalogEditor;

};

#endif // QCORE_MANAGER_EDITOR_H
