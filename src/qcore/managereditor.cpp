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

/// @todo use meta parameters for tooltip, custom editors, visibility (user, advanced, private, ...)

namespace {

constexpr int nameColumn = 0;
constexpr int keyColumn = 1;
constexpr int valueColumn = 2;

auto modeIcon(Handler* handler) {
    const auto mode = handler->mode();
    const auto state = handler->state() & Handler::State::duplex();
    if (mode.any(Handler::Mode::thru())) {
        if (state == Handler::State::duplex())
            return QIcon{":/data/modes/thru_open.png"};
        return QIcon{":/data/modes/thru_closed.png"};
    }
    const bool forward = mode.any(Handler::Mode::forward());
    const bool receive = mode.any(Handler::Mode::receive());
    if (forward && !receive) {
        return state == Handler::State::forward() ? QIcon{":/data/modes/forward_open.png"} : QIcon{":/data/modes/forward_closed.png"};
    } else if (!forward && receive) {
        return state == Handler::State::receive() ? QIcon{":/data/modes/receive_open.png"} : QIcon{":/data/modes/receive_closed.png"};
    } else if (forward && receive) {
        if (state == Handler::State::duplex()) return QIcon{":/data/modes/receive_open_forward_open.png"};
        if (state == Handler::State::forward()) return QIcon{":/data/modes/receive_closed_forward_open.png"};
        if (state == Handler::State::receive()) return QIcon{":/data/modes/receive_open_forward_closed.png"};
        return QIcon{":/data/modes/receive_closed_forward_closed.png"};
    }
    return QIcon{};
}

Handler* handlerForItem(QTreeWidgetItem* item) {
    return item ? item->data(nameColumn, Qt::UserRole).value<Handler*>() : nullptr;
}

template<typename PredicateT>
QTreeWidgetItem* findChildIf(QTreeWidgetItem* root, PredicateT&& pred) {
    for(int row = 0 ; row < root->childCount() ; ++row) {
        auto* item = root->child(row);
        if (pred(item))
            return item;
    }
    return nullptr;
}

QTreeWidgetItem* itemForHandler(QTreeWidgetItem* root, const Handler* handler) {
    return findChildIf(root, [=](auto* item) { return handlerForItem(item) == handler; });
}

}

HandlerListEditor::HandlerListEditor(Manager* manager, QWidget* parent) : QTreeWidget{parent}, mManager{manager} {

    setAlternatingRowColors(true);
    setColumnCount(3);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIconSize({35, 20});
    setHeaderHidden(true);
    setEditTriggers(DoubleClicked | EditKeyPressed | AnyKeyPressed);

    // header
    auto* headerView = header();
    headerView->setDefaultSectionSize(1);
    headerView->setStretchLastSection(true);
    headerView->setSectionResizeMode(nameColumn, QHeaderView::Fixed);
    headerView->setSectionResizeMode(keyColumn, QHeaderView::ResizeToContents);

    auto* noEdit = new NoEditDelegate{this};
    setItemDelegateForColumn(nameColumn, noEdit);
    setItemDelegateForColumn(keyColumn, noEdit);

    // menu
    auto* trigger = new MenuDefaultTrigger{this};
    setContextMenuPolicy(Qt::CustomContextMenu);
    mMenu = new QMenu{this};
    addCommandMenu("Open", HandlerProxy::Command::Open)->installEventFilter(trigger);
    addCommandMenu("Close", HandlerProxy::Command::Close)->installEventFilter(trigger);
    addCommandMenu("Toggle", HandlerProxy::Command::Toggle)->installEventFilter(trigger);
    mMenu->addSeparator();
    mMenu->addAction(QIcon{":/data/delete.svg"}, "Delete", this, SLOT(destroySelection()));
    mRenameAction = mMenu->addAction(QIcon{":/data/text.svg"}, "Rename", this, SLOT(renameSelection()));
    mMenu->addAction(QIcon{":/data/eye.svg"}, "Edit", this, SLOT(editSelection()));

    connect(manager, &Context::handlerInserted, this, &HandlerListEditor::insertHandler);
    connect(manager, &Context::handlerRemoved, this, &HandlerListEditor::removeHandler);
    connect(manager, &Context::handlerRenamed, this, &HandlerListEditor::renameHandler);
    connect(manager, &Context::handlerParametersChanged, this, &HandlerListEditor::onParametersChange);
    connect(manager->observer(), &Observer::messageHandled, this, &HandlerListEditor::onMessageHandled);

    connect(this, &HandlerListEditor::customContextMenuRequested, this, &HandlerListEditor::showMenu);
    connect(this, &HandlerListEditor::itemChanged, this, &HandlerListEditor::onItemChange);
}

void HandlerListEditor::insertHandler(Handler* handler) {
    QSignalBlocker sb{this};
    auto* item = new QTreeWidgetItem{invisibleRootItem()};
    item->setFirstColumnSpanned(true);
    item->setData(nameColumn, Qt::UserRole, qVariantFromValue(handler));
    item->setText(nameColumn, handlerName(handler));
    item->setIcon(nameColumn, modeIcon(handler));
    for (const auto& parameter : getProxy(mManager->handlerProxies(), handler).getParameters())
        addParameter(item, parameter);
}

void HandlerListEditor::renameHandler(Handler* handler) {
    if (auto* item = itemForHandler(invisibleRootItem(), handler))
        item->setText(nameColumn, handlerName(handler));
}

void HandlerListEditor::removeHandler(Handler* handler) {
    if (auto* item = itemForHandler(invisibleRootItem(), handler))
        delete item;
}

void HandlerListEditor::onParametersChange(Handler* handler) {
    if(auto* item = itemForHandler(invisibleRootItem(), handler)) {
        QSignalBlocker sb{this};
        for (const auto& parameter : getProxy(mManager->handlerProxies(), handler).getParameters())
            updateParameter(item, parameter);
    }
}

void HandlerListEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (message.event.is(family_t::extended_system) && (Handler::open_ext.affects(message.event) || Handler::close_ext.affects(message.event)))
        if (auto* item = itemForHandler(invisibleRootItem(), handler))
            item->setIcon(nameColumn, modeIcon(handler));
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

void HandlerListEditor::onItemChange(QTreeWidgetItem* item, int column) {
    if (column == valueColumn) {
        if (auto* handler = handlerForItem(item->parent())) {
            QSignalBlocker sb{this};
            auto proxy = getProxy(mManager->handlerProxies(), handler);
            proxy.setParameter({item->text(keyColumn), item->text(valueColumn)}, false);
            proxy.notifyParameters(); /*!< force update even if it failed */
        }
    }
}

void HandlerListEditor::destroySelection() {
    if (QMessageBox::question(this, {}, "Are you sure you want to destroy these handlers ?") == QMessageBox::Yes)
        for (auto* handler : selectedHandlers())
            mManager->removeHandler(handler);
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

void HandlerListEditor::sendToSelection(HandlerProxy::Command command, Handler::State state) {
    for (auto* handler : selectedHandlers())
        getProxy(mManager->handlerProxies(), handler).sendCommand(command, state);
}

QMenu* HandlerListEditor::addCommandMenu(const QString& title, HandlerProxy::Command command) {
    auto* menu = mMenu->addMenu(title);
    auto* duplex = menu->addAction("All");
    auto* receive = menu->addAction("Receive");
    auto* forward = menu->addAction("Forward");
    menu->setDefaultAction(duplex);
    connect(duplex, &QAction::triggered, this, [this, command] { sendToSelection(command, Handler::State::duplex()); });
    connect(receive, &QAction::triggered, this, [this, command] { sendToSelection(command, Handler::State::receive()); });
    connect(forward, &QAction::triggered, this, [this, command] { sendToSelection(command, Handler::State::forward()); });
    return menu;
}

void HandlerListEditor::updateParameter(QTreeWidgetItem* parent, const HandlerView::Parameter& parameter) {
    if (auto* parameterItem = findChildIf(parent, [&](auto* item) { return item->text(keyColumn) == parameter.name; }))
        parameterItem->setText(valueColumn, parameter.value);
    else
        addParameter(parent, parameter);
}

void HandlerListEditor::addParameter(QTreeWidgetItem* parent, const HandlerView::Parameter& parameter) {
    auto* parameterItem = new QTreeWidgetItem{parent};
    parameterItem->setText(keyColumn, parameter.name);
    parameterItem->setText(valueColumn, parameter.value);
    parameterItem->setFlags(parameterItem->flags() | Qt::ItemIsEditable);
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
