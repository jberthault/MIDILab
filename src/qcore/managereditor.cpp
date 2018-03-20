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

#include <QGraphicsSceneContextMenuEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include "qcore/managereditor.h"
#include "core/misc.h"

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
    connect(Manager::instance, &Manager::handlerListenersChanged, this, &HandlerGraphEditor::updateListeners);
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
    if (handler->mode().any(Handler::Mode::in()))
        mSelector->insertHandler(handler);
    /// @todo change style of nodes for type
}

void HandlerGraphEditor::removeHandler(Handler* handler) {
    HandlerNode* node = mNodes.take(handler);
    if (node != nullptr)
        mGraph->deleteNode(node);
    mSelector->removeHandler(handler);
}

void HandlerGraphEditor::updateListeners(Handler* handler) {
    auto listeners = handler->listeners();
    HandlerNode* tailNode = getNode(handler);
    // can't update handler if not already registered
    if (tailNode == nullptr)
        return;
    // create or update all edges defined in listeners
    for (auto& listener : listeners) {
        HandlerNode* headNode = getNode(listener.handler);
        Q_ASSERT(headNode != nullptr);
        EdgeWrapper* edge = getEdge(tailNode, headNode);
        if (!edge) {
            // no edge existing, must create it
            edge = new EdgeWrapper(tailNode, headNode);
            mGraph->insertEdge(edge);
        }
        edge->setFilter(std::move(listener.filter));
        updateEdgeVisibility(edge);
    }
    // delete old edges that no longer exist
    auto edges = tailNode->edges(); // makes a copy as the list may change
    for (Edge* edge : edges)
        if (edge->tail() == tailNode && listeners.count(static_cast<EdgeWrapper*>(edge)->receiver()->handler()) == 0)
            mGraph->deleteEdge(edge);
}

void HandlerGraphEditor::forwardEdgeCreation(Node* tail, Node* head) {
    Handler* sender = static_cast<HandlerNode*>(tail)->handler();
    Handler* receiver = static_cast<HandlerNode*>(head)->handler();
    if (!sender || !receiver) { // by construction  sender != receiver
        QMessageBox::critical(this, QString(), "Undefined sender or receiver");
    } else if (receiver->mode().none(Handler::Mode::receive())) {
        QMessageBox::warning(this, QString(), "Receiver can not handle event");
    } else {
        if (mFilter->isChecked()) {
            Handler* source = mSelector->currentHandler();
            if (!source || source->mode().none(Handler::Mode::in())) {
                QMessageBox::critical(this, QString(), "Undefined source");
            } else if (sender != source && sender->mode().none(Handler::Mode::thru())) {
                QMessageBox::warning(this, QString(), "Sender must be THRU or source");
            } else {
                Manager::instance->insertConnection(sender, receiver, Filter::handler(source));
            }
        } else {
            if (sender->mode().none(Handler::Mode::forward())) {
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

void updateIcon(QTreeWidgetItem* item, Handler* handler, int column, Handler::Mode mode, Handler::State state) {
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
    mMenu->addAction(QIcon(":/data/eye.svg"), "Edit", this, SLOT(editSelection()));
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
    QTreeWidgetItem* item = mItems.take(handler);
    invisibleRootItem()->removeChild(item);
    delete item;
}

void HandlerListEditor::updateHandler(Handler* handler) {
    QTreeWidgetItem* item = mItems.value(handler, nullptr);
    if (item != nullptr) {
        updateIcon(item, handler, forwardColumn, Handler::Mode::forward(), Handler::State::forward());
        updateIcon(item, handler, receiveColumn, Handler::Mode::receive(), Handler::State::receive());
    }
}

void HandlerListEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (message.event.family() == family_t::custom && (message.event.has_custom_key("Open") || message.event.has_custom_key("Close")))
        updateHandler(handler);
}

void HandlerListEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    auto proxy = getProxy(Manager::instance->handlerProxies(), handlerForItem(item));
    switch (column) {
    case nameColumn: proxy.toggleState(); break;
    case forwardColumn: proxy.toggleState(Handler::State::forward()); break;
    case receiveColumn: proxy.toggleState(Handler::State::receive()); break;
    }
}

void HandlerListEditor::showMenu(const QPoint& point) {
    auto handlers = selectedHandlers();
    if (handlers.size() == 1) {
        auto proxy = getProxy(Manager::instance->handlerProxies(), *handlers.begin());
        mRenameAction->setEnabled(!dynamic_cast<ClosedMetaHandler*>(proxy.metaHandler()));
    } else {
        mRenameAction->setEnabled(false);
    }
    mMenu->exec(mapToGlobal(point));
}

void HandlerListEditor::destroySelection() {
    if (QMessageBox::question(this, QString(), "Are you sure you want to destroy these handlers ?") == QMessageBox::Yes)
        for (Handler* handler : selectedHandlers())
            Manager::instance->removeHandler(handler);
}

void HandlerListEditor::openSelection() {
    for (Handler* handler : selectedHandlers())
        getProxy(Manager::instance->handlerProxies(), handler).setState(true);
}

void HandlerListEditor::closeSelection() {
    for (Handler* handler : selectedHandlers())
        getProxy(Manager::instance->handlerProxies(), handler).setState(false);
}

void HandlerListEditor::toggleSelection() {
    for (Handler* handler : selectedHandlers())
        getProxy(Manager::instance->handlerProxies(), handler).toggleState();
}

void HandlerListEditor::editSelection() {
    for (Handler* handler : selectedHandlers())
        getProxy(Manager::instance->handlerProxies(), handler).show();
}

void HandlerListEditor::renameSelection() {
    auto handlers = selectedHandlers();
    if (handlers.size() == 1) {
        QString name = QInputDialog::getText(this, "Text Selection", "Please set the handler's name");
        if (!name.isEmpty())
            Manager::instance->renameHandler(*handlers.begin(), name);
    } else {
        QMessageBox::warning(this, QString(), tr("You should select one handler"));
    }
}

std::set<Handler*> HandlerListEditor::selectedHandlers() {
    std::set<Handler*> result;
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
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &HandlerCatalogEditor::customContextMenuRequested, this, &HandlerCatalogEditor::showMenu);
    connect(this, &HandlerCatalogEditor::itemDoubleClicked, this, &HandlerCatalogEditor::onDoubleClick);
    for (auto metaHandler : Manager::instance->metaHandlerPool()->metaHandlers()) {
        auto item = new QTreeWidgetItem;
        item->setText(0, metaHandlerName(metaHandler));
        item->setToolTip(0, metaHandler->description());
        item->setData(0, Qt::UserRole, qVariantFromValue(metaHandler));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        invisibleRootItem()->addChild(item);
        if (auto closedMetaHandler = dynamic_cast<ClosedMetaHandler*>(metaHandler))
            refreshMeta(item, closedMetaHandler->instantiables());
    }
}

void HandlerCatalogEditor::showMenu(const QPoint& point) {
    if (auto item = itemAt(point)) {
        if (auto closedMetaHandler = dynamic_cast<ClosedMetaHandler*>(metaHandlerForItem(item))) {
            QMenu menu;
            auto reloadAction = menu.addAction(QIcon(":/data/reload.svg"), "Reload");
            if (menu.exec(mapToGlobal(point)) == reloadAction)
                refreshMeta(item, closedMetaHandler->instantiables());
        }
    }
}

void HandlerCatalogEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    QString fixedName;
    auto metaHandler = metaHandlerForItem(item);
    if (dynamic_cast<ClosedMetaHandler*>(metaHandler))
        return;
    if (!metaHandler) {
        metaHandler = metaHandlerForItem(item->parent());
        fixedName = item->text(column);
        for (const auto& proxy : Manager::instance->handlerProxies()) {
            if (proxy.metaHandler() == metaHandler && proxy.name() == fixedName) {
                QMessageBox::warning(this, {}, tr("This handler already exists"));
                return;
            }
        }
    }
    createHandler(metaHandler, fixedName);
}

void HandlerCatalogEditor::refreshMeta(QTreeWidgetItem *item, const QStringList& instantiables) {
    // remove previous children
    auto previousChildren = item->takeChildren();
    qDeleteAll(previousChildren);
    // make new children
    for (const auto& name : instantiables) {
        auto child = new QTreeWidgetItem;
        child->setText(0, name);
        child->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->addChild(child);
    }
    item->setExpanded(true);
}

void HandlerCatalogEditor::createHandler(MetaHandler* metaHandler, const QString& fixedName) {
    auto configurator = new HandlerConfigurator(metaHandler, this);
    if (!fixedName.isNull())
        configurator->setFixedName(fixedName);
    DialogContainer ask(configurator, this);
    if (ask.exec() != QDialog::Accepted)
        return;
    auto proxy = Manager::instance->loadHandler(metaHandler, configurator->name(), nullptr);
    proxy.setParameters(configurator->parameters());
    proxy.show();
}

MetaHandler* HandlerCatalogEditor::metaHandlerForItem(QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole).value<MetaHandler*>();
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
