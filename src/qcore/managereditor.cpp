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

#include <QInputDialog>
#include <QGraphicsSceneContextMenuEvent>
#include "managereditor.h"
#include "manager.h"
#include "qtools/displayer.h"
#include "qtools/misc.h"

using namespace handler_ns;

//=============
// HandlerNode
//=============

HandlerNode::HandlerNode(Handler* handler, HandlerGraphEditor* parent) :
    Node(handlerName(handler), nullptr), mHandler(handler), mParent(parent) {

}

Handler* HandlerNode::handler() const {
    return mHandler;
}

HandlerGraphEditor* HandlerNode::parent() const {
    return mParent;
}

void HandlerNode::updateLabel() {
    setLabel(handlerName(mHandler));
}

//void HandlerNode::keyPressEvent(QKeyEvent* event) {
//    if (event->key() == Qt::Key_Delete) {
//        Manager::instance->killHandler(mHandler);
//    } else {
//        QGraphicsItem::keyPressEvent(event);
//    }
//}

//=============
// EdgeWrapper
//=============

EdgeWrapper::EdgeWrapper(HandlerNode* sender, HandlerNode* receiver) :
    Edge(sender, receiver), mFilter() {

}

HandlerNode* EdgeWrapper::sender() const {
    return static_cast<HandlerNode*>(tail());
}

HandlerNode* EdgeWrapper::receiver() const {
    return static_cast<HandlerNode*>(head());
}

HandlerGraphEditor* EdgeWrapper::parent() const {
   return sender()->parent();
}

void EdgeWrapper::setFilter(Filter filter) {
    mFilter = std::move(filter);
}

void EdgeWrapper::setVisibility(Filter::match_type match) {
    switch (match.value) {
        case Filter::match_type::true_value:
            setVisible(true);
            setArrowColor(Qt::black);
        break;
        case Filter::match_type::false_value:
            setVisible(false);
        break;
        case Filter::match_type::indeterminate_value:
            setVisible(true);
            setArrowColor(Qt::darkGray);
        break;
    }
}

void EdgeWrapper::updateVisibility(Handler* source) {
    setVisibility(mFilter.match_handler(source));
}

void EdgeWrapper::updateVisibility() {
    setVisibility(mFilter.match_nothing());
}

void EdgeWrapper::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    QAction* straightenAction = menu.addAction("Straighten");
    straightenAction->setEnabled(!controlPoints().empty());
    QAction* deleteAction = menu.addAction(QIcon(":/data/delete.svg"), "Delete");
    QAction* infoAction = menu.addAction("Info");
    QAction* selectedAction = menu.exec(event->screenPos());
    if (selectedAction == straightenAction) {
        setControlPoints();
    } else if (selectedAction == deleteAction) {
        parent()->forwardEdgeRemoval(this);
    } else if (selectedAction == infoAction) {
        QMessageBox::information(nullptr, "Filter", QString::fromStdString(mFilter.string()));
    }
}

//====================
// HandlerGraphEditor
//====================

HandlerGraphEditor::HandlerGraphEditor(QWidget* parent) :
    QWidget(parent), mNodeColor(Qt::black) {

    // graph ! @warning graph removal must be called before selector removal
    mGraph = new Graph(this);
    connect(mGraph, &Graph::edgeCreation, this, &HandlerGraphEditor::forwardEdgeCreation);
    // connect changes from mapper @todo fill already created children
    connect(Manager::instance, &Manager::handlerInserted, this, &HandlerGraphEditor::insertHandler);
    connect(Manager::instance, &Manager::handlerRemoved, this, &HandlerGraphEditor::removeHandler);
    connect(Manager::instance, &Manager::sinksChanged, this, &HandlerGraphEditor::updateSinks);
    connect(Manager::instance, &Manager::handlerRenamed, this, &HandlerGraphEditor::renameHandler);
    // filter
    mFilter = new QCheckBox(this);
    mFilter->setToolTip("Filter by source");
    // selector
    mSelector = new HandlerSelector(this);
    mSelector->setEnabled(mFilter->isChecked());
    // center
    QPushButton* centerButton = new QPushButton("Center", this);
    // connections
    connect(mFilter, &QCheckBox::clicked, mSelector, &HandlerSelector::setEnabled);
    connect(mFilter, &QCheckBox::clicked, this, &HandlerGraphEditor::updateEdgesVisibility);
    connect(mSelector, &HandlerSelector::handlerChanged, this, &HandlerGraphEditor::updateEdgesVisibility);
    connect(centerButton, &QPushButton::clicked, mGraph, &Graph::centerOnScene);
    // layout
    setLayout(make_vbox(margin_tag{0}, mGraph, make_hbox(stretch_tag{}, centerButton, mFilter, mSelector)));
}

Graph* HandlerGraphEditor::graph() {
    return mGraph;
}

QBrush HandlerGraphEditor::nodeColor() const {
    return mNodeColor;
}

void HandlerGraphEditor::setNodeColor(const QBrush& brush) {
    mNodeColor = brush;
    QMapIterator<Handler*, HandlerNode*> it(mNodes);
    while (it.hasNext()) {
        it.next();
        it.value()->setColor(mNodeColor);
    }
}

QBrush HandlerGraphEditor::nodeBackgroundColor() const {
    return mNodeBackgroundColor;
}

void HandlerGraphEditor::setNodeBackgroundColor(const QBrush& brush) {
    mNodeBackgroundColor = brush;
    QMapIterator<Handler*, HandlerNode*> it(mNodes);
    while (it.hasNext()) {
        it.next();
        it.value()->setBackgroundColor(mNodeBackgroundColor);
    }
}

QBrush HandlerGraphEditor::nodeAlternateBackgroundColor() const {
    return mNodeAlternateBackgroundColor;
}

void HandlerGraphEditor::setNodeAlternateBackgroundColor(const QBrush& brush) {
    mNodeAlternateBackgroundColor = brush;
    QMapIterator<Handler*, HandlerNode*> it(mNodes);
    while (it.hasNext()) {
        it.next();
        it.value()->setAlternateBackgroundColor(mNodeAlternateBackgroundColor);
    }
}

HandlerNode* HandlerGraphEditor::getNode(Handler* handler) {
    return mNodes.value(handler, nullptr);
}

EdgeWrapper* HandlerGraphEditor::getEdge(Handler* tail, Handler* head) {
    return getEdge(getNode(tail), getNode(head));
}

EdgeWrapper* HandlerGraphEditor::getEdge(HandlerNode* tail, HandlerNode* head) {
    if (tail && head)
        for (Edge* edge : tail->edges())
            if (edge->tail() == tail && edge->head() == head)
                return static_cast<EdgeWrapper*>(edge);
    return nullptr;
}

void HandlerGraphEditor::renameHandler(Handler* handler) {
    HandlerNode* node = getNode(handler);
    if (node)
        node->updateLabel();
    mSelector->renameHandler(handler);
}

void HandlerGraphEditor::insertHandler(Handler* handler) {
    if (!mNodes.contains(handler)) {
        HandlerNode* node = new HandlerNode(handler, this);
        node->setColor(mNodeColor);
        node->setBackgroundColor(mNodeBackgroundColor);
        node->setAlternateBackgroundColor(mNodeAlternateBackgroundColor);
        mNodes[handler] = node;
        mGraph->insertNode(node);
    }
    if (asMode(handler, in_mode))
        mSelector->insertHandler(handler);
    /// @todo change style of nodes for type
}

void HandlerGraphEditor::removeHandler(Handler* handler) {
    HandlerNode* node = getNode(handler);
    if (node != nullptr) {
        mNodes.remove(handler);
        mGraph->deleteNode(node);
    }
    mSelector->removeHandler(handler);
}

void HandlerGraphEditor::updateSinks(Handler* handler) {
    Handler::sinks_type sinks = handler->sinks();
    HandlerNode* tailNode = getNode(handler);
    // can't update handler if not already registered
    if (tailNode == nullptr)
        return;
    // create or update all edges defined in sinks
    for (auto& pair : sinks) {
        Handler* head = pair.first;
        HandlerNode* headNode = getNode(head);
        Q_ASSERT(headNode != nullptr);
        EdgeWrapper* edge = getEdge(tailNode, headNode);
        if (!edge) {
            // no edge existing, must create it
            edge = new EdgeWrapper(tailNode, headNode);
            mGraph->insertEdge(edge);
        }
        edge->setFilter(std::move(pair.second));
        updateEdgeVisibility(edge);
    }
    // delete old edges that no longer exist
    for (Edge* edge : tailNode->edges())
        if (edge->tail() == tailNode && sinks.count(static_cast<EdgeWrapper*>(edge)->receiver()->handler()) == 0)
            mGraph->deleteEdge(edge);
}

void HandlerGraphEditor::forwardEdgeCreation(Node* tail, Node* head) {
    Handler* sender = static_cast<HandlerNode*>(tail)->handler();
    Handler* receiver = static_cast<HandlerNode*>(head)->handler();
    if (!sender || !receiver) { // by construction  sender != receiver
        QMessageBox::critical(this, QString(), "Undefined sender or receiver");
    } else if (!asMode(receiver, receive_mode)) {
        QMessageBox::warning(this, QString(), "Receiver can not handle event");
    } else {
        if (mFilter->isChecked()) {
            Handler* source = mSelector->currentHandler();
            if (!source || !asMode(source, in_mode)) {
                QMessageBox::critical(this, QString(), "Undefined source");
            } else if (sender != source && !asMode(sender, thru_mode)) {
                QMessageBox::warning(this, QString(), "Sender must be THRU or source");
            } else {
                Manager::instance->insertConnection(sender, receiver, source);
            }
        } else {
            if (!asMode(sender, forward_mode)) {
                QMessageBox::information(this, QString(), "Sender can not forward event");
            } else {
                Manager::instance->insertConnection(sender, receiver);
            }
        }
    }
}

void HandlerGraphEditor::forwardEdgeRemoval(EdgeWrapper* edge) {
    if (mFilter->isChecked())
        Manager::instance->removeConnection(edge->sender()->handler(), edge->receiver()->handler(), mSelector->currentHandler());
    else
        Manager::instance->removeConnection(edge->sender()->handler(), edge->receiver()->handler());
}

void HandlerGraphEditor::updateEdgeVisibility(EdgeWrapper* edge) {
    if (mFilter->isChecked()) {
        edge->updateVisibility(mSelector->currentHandler());
    } else {
        edge->updateVisibility();
    }
}

void HandlerGraphEditor::updateEdgesVisibility() {
    if (mFilter->isChecked()) {
        Handler* source = mSelector->currentHandler();
        for (Edge* edge : mGraph->getEdges())
            static_cast<EdgeWrapper*>(edge)->updateVisibility(source);
    } else {
        for (Edge* edge : mGraph->getEdges())
            static_cast<EdgeWrapper*>(edge)->updateVisibility();
    }
}

//===================
// HandlerListEditor
//===================

namespace {

void updateIcon(QTreeWidgetItem* item, Handler* handler, int column, Handler::mode_type mode, Handler::state_type state) {
    QString filename;
    if (handler->mode().none(mode))
        filename = ":/data/light-gray.svg";
    else if (handler->state().any(state))
        filename = ":/data/light-green.svg";
    else
        filename = ":/data/light-red.svg";
    item->setData(column, Qt::DecorationRole, QIcon(filename));
}

}

HandlerListEditor::HandlerListEditor(QWidget* parent) : QTreeWidget(parent) {

    setAlternatingRowColors(true);

    QStringList labels;
    labels << "Name" << "In" << "Out";
    setHeaderLabels(labels);
    setColumnCount(labels.size());
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(this, &HandlerListEditor::itemDoubleClicked, this, &HandlerListEditor::onDoubleClick);

    // header
    QHeaderView* headerView = header();
    headerView->setStretchLastSection(false);
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(nameColumn, QHeaderView::Stretch);

    // menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    mMenu = new QMenu(this);
    mMenu->addAction("Open", this, SLOT(openSelection()));
    mMenu->addAction("Close", this, SLOT(closeSelection()));
    mMenu->addAction("Toggle", this, SLOT(toggleSelection()));
    mMenu->addSeparator();
    mMenu->addAction(QIcon(":/data/delete.svg"), "Delete", this, SLOT(destroySelection()));
    mRenameAction = mMenu->addAction(QIcon(":/data/text.svg"), "Rename", this, SLOT(renameSelection()));
    mEditAction = mMenu->addAction(QIcon(":/data/eye.svg"), "Edit", this, SLOT(editSelection()));
    connect(this, &HandlerListEditor::customContextMenuRequested, this, &HandlerListEditor::showMenu);

    connect(Manager::instance, &Manager::handlerInserted, this, &HandlerListEditor::insertHandler);
    connect(Manager::instance, &Manager::handlerRemoved, this, &HandlerListEditor::removeHandler);
    connect(Manager::instance, &Manager::handlerRenamed, this, &HandlerListEditor::renameHandler);
    connect(Manager::instance->observer(), &Observer::messageHandled, this, &HandlerListEditor::onMessageHandled);
}

Handler* HandlerListEditor::handlerForItem(QTreeWidgetItem* item) {
    return item ? item->data(nameColumn, Qt::UserRole).value<Handler*>() : nullptr;
}

QTreeWidgetItem* HandlerListEditor::insertHandler(Handler* handler) {
    Q_ASSERT(handler);
    if (mItems.contains(handler))
        return mItems[handler];
    QTreeWidgetItem* item = new QTreeWidgetItem;
    item->setData(nameColumn, Qt::UserRole, qVariantFromValue(handler));
    mItems[handler] = item;
    invisibleRootItem()->addChild(item);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setText(nameColumn, handlerName(handler));
    updateHandler(handler);
    return item;
}

void HandlerListEditor::renameHandler(Handler* handler) {
    QTreeWidgetItem* item = mItems.value(handler, nullptr);
    if (item)
        item->setData(nameColumn, Qt::DisplayRole, handlerName(handler));
}

void HandlerListEditor::removeHandler(Handler* handler) {
    QTreeWidgetItem* item = mItems.value(handler, nullptr);
    invisibleRootItem()->removeChild(item);
    delete item;
}

void HandlerListEditor::updateHandler(Handler* handler) {
    QTreeWidgetItem* item = mItems.value(handler, nullptr);
    if (item != nullptr) {
        updateIcon(item, handler, forwardColumn, forward_mode, forward_state);
        updateIcon(item, handler, receiveColumn, receive_mode, receive_state);
    }
}

void HandlerListEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (message.event.family() == family_t::custom) {
        auto key = message.event.get_custom_key();
        if (key == "Open" || key == "Close")
            updateHandler(handler);
    }
}

void HandlerListEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    Handler* handler = handlerForItem(item);
    if (!handler)
        return;
    switch (column) {
    case nameColumn:
        Manager::instance->toggleHandler(handler);
        break;
    case forwardColumn:
        if (handler->mode().any(thru_mode))
            Manager::instance->toggleHandler(handler);
        else
            Manager::instance->toggleHandler(handler, forward_state);
        break;
    case receiveColumn:
        Manager::instance->toggleHandler(handler, receive_state);
        break;
    default: break;
    }
}

void HandlerListEditor::showMenu(const QPoint& point) {
    QSet<Handler*> handlers = selectedHandlers();
    bool singleton = handlers.size() == 1;
    mEditAction->setEnabled(singleton);
    mRenameAction->setEnabled(singleton);
    mMenu->exec(mapToGlobal(point));
}

void HandlerListEditor::destroySelection() {
    if (QMessageBox::question(this, QString(), "Are you sure you want to destroy these handlers ?") == QMessageBox::Yes)
        for (Handler* handler : selectedHandlers())
            Manager::instance->removeHandler(handler);
}

void HandlerListEditor::openSelection() {
    for (Handler* handler : selectedHandlers())
        Manager::instance->setHandlerOpen(handler, true);
}

void HandlerListEditor::closeSelection() {
    for (Handler* handler : selectedHandlers())
        Manager::instance->setHandlerOpen(handler, false);
}

void HandlerListEditor::toggleSelection() {
    for (Handler* handler : selectedHandlers())
        Manager::instance->toggleHandler(handler);
}

void HandlerListEditor::editSelection() {
    for (Handler* handler : selectedHandlers())
        Manager::instance->editHandler(handler);
}

void HandlerListEditor::renameSelection() {
    QSet<Handler*> handlers = selectedHandlers();
    if (handlers.size() == 1) {
        QString name = QInputDialog::getText(this, "Text Selection", "Please set the handler's name");
        if (!name.isEmpty())
            Manager::instance->renameHandler(*handlers.begin(), name);
    } else {
        QMessageBox::warning(this, QString(), tr("You should select one handler"));
    }
}

QSet<Handler*> HandlerListEditor::selectedHandlers() {
    QSet<Handler*> result;
    for (QTreeWidgetItem* item : selectedItems()) {
        Handler* handler = handlerForItem(item);
        if (handler)
            result.insert(handler);
    }
    return result;
}

//======================
// HandlerCatalogEditor
//======================

HandlerCatalogEditor::HandlerCatalogEditor(QWidget* parent) : QTreeWidget(parent) {
    setHeaderHidden(true);
    setColumnCount(1);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);
    connect(this, &HandlerCatalogEditor::itemDoubleClicked, this, &HandlerCatalogEditor::onDoubleClick);
    for (auto meta : Manager::instance->collector()->metaHandlers()) {
        auto item = new QTreeWidgetItem;
        item->setText(0, metaHandlerName(meta));
        item->setToolTip(0, meta->description());
        item->setData(0, Qt::UserRole, qVariantFromValue(meta));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        invisibleRootItem()->addChild(item);
    }
}

void HandlerCatalogEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    createHandler(item->data(column, Qt::UserRole).value<MetaHandler*>());
}

void HandlerCatalogEditor::createHandler(MetaHandler* meta) {
    auto configurator = new HandlerConfigurator(meta, this);
    DialogContainer ask(configurator, this);
    if (ask.exec() != QDialog::Accepted)
        return;
    HandlerConfiguration hc(configurator->name());
    hc.group = configurator->group();
    hc.parameters = configurator->parameters();
    Handler* handler = Manager::instance->loadHandler(meta, hc);
    if (handler)
        Manager::instance->editHandler(handler);
}

//===============
// ManagerEditor
//===============

ManagerEditor::ManagerEditor(QWidget* parent) : QTabWidget(parent) {
    setWindowFlags(Qt::Dialog);
    setWindowTitle("Handlers");
    setWindowIcon(QIcon(":/data/wrench.svg"));
    mListEditor = new HandlerListEditor(this);
    mGraphEditor = new HandlerGraphEditor(this);
    mCatalogEditor = new HandlerCatalogEditor(this);
    addTab(mListEditor, QIcon(":/data/list.svg"), "List");
    addTab(mGraphEditor, QIcon(":/data/fork.svg"), "Graph");
    addTab(mCatalogEditor, QIcon(":/data/book.svg"), "Catalog");
    setTabToolTip(0, "Edit handlers : rename, delete, mute, ...");
    setTabToolTip(1, "Edit connections");
    setTabToolTip(2, "See & create new handlers");
}

HandlerListEditor* ManagerEditor::listEditor() {
    return mListEditor;
}

HandlerGraphEditor* ManagerEditor::graphEditor() {
    return mGraphEditor;
}

HandlerCatalogEditor* ManagerEditor::catalogEditor() {
    return mCatalogEditor;
}
