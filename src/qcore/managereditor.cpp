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
    Node{handlerName(handler), nullptr}, mHandler{handler}, mParent{parent} {

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

//=============
// EdgeWrapper
//=============

EdgeWrapper::EdgeWrapper(HandlerNode* sender, HandlerNode* receiver) : Edge{sender, receiver} {

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
    auto* straightenAction = menu.addAction("Straighten");
    straightenAction->setEnabled(!controlPoints().empty());
    auto* deleteAction = menu.addAction(QIcon{":/data/delete.svg"}, "Delete");
    auto* infoAction = menu.addAction("Info");
    auto* selectedAction = menu.exec(event->screenPos());
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

HandlerGraphEditor::HandlerGraphEditor(Manager* manager, QWidget* parent) : QWidget{parent}, mManager{manager} {
    // graph ! @warning graph removal must be called before selector removal
    mGraph = new Graph{this};
    connect(mGraph, &Graph::edgeCreation, this, &HandlerGraphEditor::forwardEdgeCreation);
    /// @todo fill already created children
    connect(manager, &Context::handlerInserted, this, &HandlerGraphEditor::insertHandler);
    connect(manager, &Context::handlerRemoved, this, &HandlerGraphEditor::removeHandler);
    connect(manager, &Context::handlerListenersChanged, this, &HandlerGraphEditor::updateListeners);
    connect(manager, &Context::handlerRenamed, this, &HandlerGraphEditor::renameHandler);
    // filter
    mFilter = new QCheckBox{this};
    mFilter->setToolTip("Filter by source");
    // selector
    mSelector = new HandlerSelector{this};
    mSelector->setEnabled(mFilter->isChecked());
    // center
    auto* centerButton = new QPushButton{"Center", this};
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
    QMapIterator<Handler*, HandlerNode*> it{mNodes};
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
    QMapIterator<Handler*, HandlerNode*> it{mNodes};
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
    QMapIterator<Handler*, HandlerNode*> it{mNodes};
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
        for (auto* edge : tail->edges())
            if (edge->tail() == tail && edge->head() == head)
                return static_cast<EdgeWrapper*>(edge);
    return nullptr;
}

void HandlerGraphEditor::renameHandler(Handler* handler) {
    if (auto* node = getNode(handler))
        node->updateLabel();
    mSelector->renameHandler(handler);
}

void HandlerGraphEditor::insertHandler(Handler* handler) {
    if (!mNodes.contains(handler)) {
        auto* node = new HandlerNode{handler, this};
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
    if (auto* node = mNodes.take(handler))
        mGraph->deleteNode(node);
    mSelector->removeHandler(handler);
}

void HandlerGraphEditor::updateListeners(Handler* handler) {
    const auto listeners = handler->listeners();
    auto* tailNode = getNode(handler);
    // can't update handler if not already registered
    if (tailNode == nullptr)
        return;
    // create or update all edges defined in listeners
    for (auto& listener : listeners) {
        auto* headNode = getNode(listener.handler);
        Q_ASSERT(headNode != nullptr);
        auto* edge = getEdge(tailNode, headNode);
        if (!edge) {
            // no edge existing, must create it
            edge = new EdgeWrapper{tailNode, headNode};
            mGraph->insertEdge(edge);
        }
        edge->setFilter(std::move(listener.filter));
        updateEdgeVisibility(edge);
    }
    // delete old edges that no longer exist
    auto edges = tailNode->edges(); // makes a copy as the list may change
    for (auto* edge : edges)
        if (edge->tail() == tailNode && listeners.count(static_cast<EdgeWrapper*>(edge)->receiver()->handler()) == 0)
            mGraph->deleteEdge(edge);
}

void HandlerGraphEditor::forwardEdgeCreation(Node* tail, Node* head) {
    auto* sender = static_cast<HandlerNode*>(tail)->handler();
    auto* receiver = static_cast<HandlerNode*>(head)->handler();
    if (!sender || !receiver) { // by construction  sender != receiver
        QMessageBox::critical(this, {}, "Undefined sender or receiver");
    } else if (receiver->mode().none(Handler::Mode::receive())) {
        QMessageBox::warning(this, {}, "Receiver can not handle event");
    } else {
        if (mFilter->isChecked()) {
            auto* source = mSelector->currentHandler();
            if (!source || source->mode().none(Handler::Mode::in())) {
                QMessageBox::critical(this, {}, "Undefined source");
            } else if (sender != source && sender->mode().none(Handler::Mode::thru())) {
                QMessageBox::warning(this, {}, "Sender must be THRU or source");
            } else {
                mManager->insertConnection(sender, receiver, Filter::handler(source));
            }
        } else {
            if (sender->mode().none(Handler::Mode::forward())) {
                QMessageBox::information(this, {}, "Sender can not forward event");
            } else {
                mManager->insertConnection(sender, receiver);
            }
        }
    }
}

void HandlerGraphEditor::forwardEdgeRemoval(EdgeWrapper* edge) {
    if (mFilter->isChecked())
        mManager->removeConnection(edge->sender()->handler(), edge->receiver()->handler(), mSelector->currentHandler());
    else
        mManager->removeConnection(edge->sender()->handler(), edge->receiver()->handler());
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
        auto* source = mSelector->currentHandler();
        for (auto* edge : mGraph->getEdges())
            static_cast<EdgeWrapper*>(edge)->updateVisibility(source);
    } else {
        for (auto* edge : mGraph->getEdges())
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
    item->setData(column, Qt::DecorationRole, QIcon{filename});
}

}

HandlerListEditor::HandlerListEditor(Manager* manager, QWidget* parent) : QTreeWidget{parent}, mManager{manager} {

    setAlternatingRowColors(true);

    QStringList labels;
    labels << "Name" << "In" << "Out";
    setHeaderLabels(labels);
    setColumnCount(labels.size());
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(this, &HandlerListEditor::itemDoubleClicked, this, &HandlerListEditor::onDoubleClick);

    // header
    auto* headerView = header();
    headerView->setStretchLastSection(false);
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(nameColumn, QHeaderView::Stretch);

    // menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    mMenu = new QMenu{this};
    mMenu->addAction("Open", this, SLOT(openSelection()));
    mMenu->addAction("Close", this, SLOT(closeSelection()));
    mMenu->addAction("Toggle", this, SLOT(toggleSelection()));
    mMenu->addSeparator();
    mMenu->addAction(QIcon{":/data/delete.svg"}, "Delete", this, SLOT(destroySelection()));
    mRenameAction = mMenu->addAction(QIcon{":/data/text.svg"}, "Rename", this, SLOT(renameSelection()));
    mMenu->addAction(QIcon{":/data/eye.svg"}, "Edit", this, SLOT(editSelection()));
    connect(this, &HandlerListEditor::customContextMenuRequested, this, &HandlerListEditor::showMenu);

    connect(manager, &Context::handlerInserted, this, &HandlerListEditor::insertHandler);
    connect(manager, &Context::handlerRemoved, this, &HandlerListEditor::removeHandler);
    connect(manager, &Context::handlerRenamed, this, &HandlerListEditor::renameHandler);
    connect(manager->observer(), &Observer::messageHandled, this, &HandlerListEditor::onMessageHandled);
}

Handler* HandlerListEditor::handlerForItem(QTreeWidgetItem* item) {
    return item ? item->data(nameColumn, Qt::UserRole).value<Handler*>() : nullptr;
}

QTreeWidgetItem* HandlerListEditor::insertHandler(Handler* handler) {
    if (mItems.contains(handler))
        return mItems[handler];
    auto* item = new QTreeWidgetItem;
    item->setData(nameColumn, Qt::UserRole, qVariantFromValue(handler));
    mItems[handler] = item;
    invisibleRootItem()->addChild(item);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setText(nameColumn, handlerName(handler));
    updateHandler(handler);
    return item;
}

void HandlerListEditor::renameHandler(Handler* handler) {
    if (auto* item = mItems.value(handler, nullptr))
        item->setData(nameColumn, Qt::DisplayRole, handlerName(handler));
}

void HandlerListEditor::removeHandler(Handler* handler) {
    auto* item = mItems.take(handler);
    invisibleRootItem()->removeChild(item);
    delete item;
}

void HandlerListEditor::updateHandler(Handler* handler) {
    if (auto* item = mItems.value(handler, nullptr)) {
        updateIcon(item, handler, forwardColumn, Handler::Mode::forward(), Handler::State::forward());
        updateIcon(item, handler, receiveColumn, Handler::Mode::receive(), Handler::State::receive());
    }
}

void HandlerListEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (message.event.is(family_t::extended_system) && (Handler::open_ext.affects(message.event) || Handler::close_ext.affects(message.event)))
        updateHandler(handler);
}

void HandlerListEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    auto proxy = getProxy(mManager->handlerProxies(), handlerForItem(item));
    switch (column) {
    case nameColumn: proxy.toggleState(); break;
    case forwardColumn: proxy.toggleState(Handler::State::forward()); break;
    case receiveColumn: proxy.toggleState(Handler::State::receive()); break;
    }
}

void HandlerListEditor::showMenu(const QPoint& point) {
    const auto handlers = selectedHandlers();
    if (!handlers.empty()) {
        bool renameEnabled = false;
        if (handlers.size() == 1) {
            auto proxy = getProxy(mManager->handlerProxies(), *handlers.begin());
            if (proxy.metaHandler() && !dynamic_cast<ClosedProxyFactory*>(proxy.metaHandler()->factory()))
                renameEnabled = true;
        }
        mRenameAction->setEnabled(renameEnabled);
        mMenu->exec(mapToGlobal(point));
    }
}

void HandlerListEditor::destroySelection() {
    if (QMessageBox::question(this, {}, "Are you sure you want to destroy these handlers ?") == QMessageBox::Yes)
        for (auto* handler : selectedHandlers())
            mManager->removeHandler(handler);
}

void HandlerListEditor::openSelection() {
    for (auto* handler : selectedHandlers())
        getProxy(mManager->handlerProxies(), handler).setState(true);
}

void HandlerListEditor::closeSelection() {
    for (auto* handler : selectedHandlers())
        getProxy(mManager->handlerProxies(), handler).setState(false);
}

void HandlerListEditor::toggleSelection() {
    for (auto* handler : selectedHandlers())
        getProxy(mManager->handlerProxies(), handler).toggleState();
}

void HandlerListEditor::editSelection() {
    for (auto* handler : selectedHandlers())
        getProxy(mManager->handlerProxies(), handler).show();
}

void HandlerListEditor::renameSelection() {
    const auto handlers = selectedHandlers();
    if (handlers.size() == 1) {
        const auto name = QInputDialog::getText(this, "Text Selection", "Please set the handler's name");
        if (!name.isEmpty())
            mManager->renameHandler(*handlers.begin(), name);
    } else {
        QMessageBox::warning(this, {}, tr("You should select one handler"));
    }
}

std::set<Handler*> HandlerListEditor::selectedHandlers() {
    std::set<Handler*> result;
    for (auto* item : selectedItems())
        if (auto* handler = handlerForItem(item))
            result.insert(handler);
    return result;
}

//======================
// HandlerCatalogEditor
//======================

HandlerCatalogEditor::HandlerCatalogEditor(Manager* manager, QWidget* parent) : QTreeWidget{parent}, mManager{manager} {
    setHeaderHidden(true);
    setColumnCount(1);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &HandlerCatalogEditor::customContextMenuRequested, this, &HandlerCatalogEditor::showMenu);
    connect(this, &HandlerCatalogEditor::itemDoubleClicked, this, &HandlerCatalogEditor::onDoubleClick);
    for (auto* metaHandler : manager->metaHandlerPool()->metaHandlers()) {
        auto* item = new QTreeWidgetItem;
        item->setText(0, metaHandlerName(metaHandler));
        item->setToolTip(0, metaHandler->description());
        item->setData(0, Qt::UserRole, qVariantFromValue(metaHandler));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        invisibleRootItem()->addChild(item);
        if (auto* factory = dynamic_cast<ClosedProxyFactory*>(metaHandler->factory()))
            refreshMeta(item, factory->instantiables());
    }
}

void HandlerCatalogEditor::showMenu(const QPoint& point) {
    if (auto* item = itemAt(point)) {
        if (auto* metaHandler = metaHandlerForItem(item)) {
            if (auto* factory = dynamic_cast<ClosedProxyFactory*>(metaHandler->factory())) {
                QMenu menu;
                auto* reloadAction = menu.addAction(QIcon{":/data/reload.svg"}, "Reload");
                if (menu.exec(mapToGlobal(point)) == reloadAction)
                    refreshMeta(item, factory->instantiables());
            }
        }
    }
}

void HandlerCatalogEditor::onDoubleClick(QTreeWidgetItem* item, int column) {
    QString fixedName;
    auto* metaHandler = metaHandlerForItem(item);
    if (metaHandler && dynamic_cast<ClosedProxyFactory*>(metaHandler->factory()))
        return;
    if (!metaHandler) { // fixed name clicked
        metaHandler = metaHandlerForItem(item->parent());
        fixedName = item->text(column);
        for (const auto& proxy : mManager->handlerProxies()) {
            if (proxy.metaHandler() == metaHandler && proxy.name() == fixedName) {
                QMessageBox::warning(this, {}, tr("This handler already exists"));
                return;
            }
        }
    }
    createHandler(metaHandler, fixedName);
}

void HandlerCatalogEditor::refreshMeta(QTreeWidgetItem* item, const QStringList& instantiables) {
    // remove previous children
    auto previousChildren = item->takeChildren();
    qDeleteAll(previousChildren);
    // make new children
    for (const auto& name : instantiables) {
        auto* child = new QTreeWidgetItem;
        child->setText(0, name);
        child->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->addChild(child);
    }
    item->setExpanded(true);
}

void HandlerCatalogEditor::createHandler(MetaHandler* metaHandler, const QString& fixedName) {
    auto* configurator = new HandlerConfigurator{metaHandler, this};
    if (!fixedName.isNull())
        configurator->setFixedName(fixedName);
    DialogContainer ask{configurator, this};
    if (ask.exec() != QDialog::Accepted)
        return;
    auto proxy = mManager->loadHandler(metaHandler, configurator->name(), nullptr);
    proxy.setParameters(configurator->parameters());
    proxy.show();
}

MetaHandler* HandlerCatalogEditor::metaHandlerForItem(QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole).value<MetaHandler*>();
}

//===============
// ManagerEditor
//===============

ManagerEditor::ManagerEditor(Manager* manager, QWidget* parent) : QTabWidget{parent} {
    setWindowFlags(Qt::Dialog);
    setWindowTitle("Handlers");
    setWindowIcon(QIcon{":/data/wrench.svg"});
    mListEditor = new HandlerListEditor{manager, this};
    mGraphEditor = new HandlerGraphEditor{manager, this};
    mCatalogEditor = new HandlerCatalogEditor{manager, this};
    addTab(mListEditor, QIcon{":/data/list.svg"}, "List");
    addTab(mGraphEditor, QIcon{":/data/fork.svg"}, "Graph");
    addTab(mCatalogEditor, QIcon{":/data/book.svg"}, "Catalog");
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
