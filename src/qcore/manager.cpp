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

#include <QAbstractEventDispatcher>
#include <QPluginLoader>
#include <QMainWindow>
#include <QDockWidget>
#include <QSettings>
#include "manager.h"
#include "qtools/displayer.h"
#include "handlers/handlers.h"

using namespace handler_ns;

//======================
// HandlerConfiguration
//======================

HandlerConfiguration::HandlerConfiguration(QString name) :
    name(std::move(name)), host(nullptr), group("default"), parameters() {

}

//=========
// Manager
//=========

Manager* Manager::instance = nullptr;

Manager::Manager(QObject* parent) : Context(parent) {

    Q_ASSERT(instance == nullptr);
    instance = this;

    mChannelEditor = new ChannelEditor(static_cast<QWidget*>(parent));
    mChannelEditor->setWindowFlags(Qt::Dialog);

    mCollector = new MetaHandlerCollector(this);
    mCollector->addFactory(new StandardFactory(this));

    mGUIHolder = new GraphicalHolder(this);

    mReceiver = new DefaultReceiver(this);

    QSettings settings;

    mPathRetrievers["midi"] = new PathRetriever(this);
    mPathRetrievers["midi"]->setCaption("MIDI files");
    mPathRetrievers["midi"]->setFilter("MIDI Files (*.mid *.midi *.kar);;All Files (*)");
    mPathRetrievers["midi"]->setDir(settings.value("paths/midi").toString());

    mPathRetrievers["soundfont"] = new PathRetriever(this);
    mPathRetrievers["soundfont"]->setCaption("SoundFont Files");
    mPathRetrievers["soundfont"]->setFilter("SoundFont Files (*.sf2);;All Files (*)");
    mPathRetrievers["soundfont"]->setDir(settings.value("paths/soundfont").toString());

    mPathRetrievers["configuration"] = new PathRetriever(this);
    mPathRetrievers["configuration"]->setCaption("Configuration Files");
    mPathRetrievers["configuration"]->setFilter("Configuration Files (*.xml);;All Files (*)");
    mPathRetrievers["configuration"]->setDir(settings.value("paths/configuration").toString());

    qApp->setQuitOnLastWindowClosed(false);
    connect(qApp, &QApplication::lastWindowClosed, this, &Manager::quit);

}

Manager::~Manager() {
    QMapIterator<Handler*, Data> it(mStorage);
    while (it.hasNext()) {
        it.next();
        if (it.value().owns)
            delete it.key();
    }
    instance = nullptr;
}

Observer* Manager::observer() const {
    return mReceiver;
}

MetaHandlerCollector* Manager::collector() const {
    return mCollector;
}

const QMap<Handler*, Manager::Data>& Manager::storage() const {
    return mStorage;
}

QWidget* Manager::editor(Handler* handler) {
    auto it = mStorage.find(handler);
    return it != mStorage.end() ? it.value().editor : nullptr;
}

ChannelEditor* Manager::channelEditor() {
    return mChannelEditor;
}

QList<Handler*> Manager::getHandlers() {
    QList<Handler*> result;
    QMapIterator<Handler*, Data> it(mStorage);
    while (it.hasNext())
        result.push_back(it.next().key());
    return result;
}

PathRetriever* Manager::pathRetriever(const QString& type) {
    auto it = mPathRetrievers.find(type);
    return it == mPathRetrievers.end() ? nullptr : it.value();
}

Handler* Manager::loadHandler(MetaHandler* meta, const HandlerConfiguration& config) {
    MetaHandler::Instance instance(nullptr, nullptr);
    if (meta) {
        instance = meta->instantiate();
        // set handler's name
        instance.first->set_name(qstring2name(config.name));
        // get instance view
        HandlerView* view = dynamic_cast<GraphicalHandler*>(instance.first);
        if (!view)
            view = instance.second;
        // set view's parent
        if (view) {
            auto displayer = dynamic_cast<SingleDisplayer*>(config.host);
            if (displayer) {
                displayer->setWidget(view);
            } else {
                auto mainWindow = static_cast<QMainWindow*>(parent());
                auto container = static_cast<MultiDisplayer*>(mainWindow->centralWidget())->insertDetached();
                displayer = container->insertSingle();
                displayer->setWidget(view);
                container->show();
            }
        }
        // insert the handler
        insertHandler(instance.first, instance.second, metaHandlerName(meta), config.group);
        // set handler parameters
        setParameters(instance.first, config.parameters);
    }
    return instance.first;
}

Handler* Manager::loadHandler(const QString& type, const HandlerConfiguration& config) {
    return loadHandler(mCollector->metaHandler(type), config);
}

void Manager::setHandlerOpen(Handler* handler, bool open) {
    setHandlerState(handler, endpoints_state, open);
}

void Manager::setHandlerState(Handler* handler, Handler::state_type state, bool open) {
    Q_ASSERT(handler);
    if (handler->mode().none(forward_mode))
        state &= ~forward_state;
    if (handler->mode().none(receive_mode))
        state &= ~receive_state;
    if (handler->state().all(state) == open) {
        qWarning() << handlerName(handler) << "handler already" << (open ? "opened" : "closed");
        return;
    }
    handler->send_message(open ? Handler::open_event(state) : Handler::close_event(state));
}

void Manager::toggleHandler(Handler* handler) {
    Q_ASSERT(handler);
    bool forward_ok = handler->mode().none(forward_mode) || handler->state().any(forward_state);
    bool receive_ok = handler->mode().none(receive_mode) || handler->state().any(receive_state);
    bool is_open = forward_ok && receive_ok;
    setHandlerOpen(handler, !is_open);
}

void Manager::toggleHandler(Handler* handler, Handler::state_type state) {
    Q_ASSERT(handler);
    setHandlerState(handler, state, !handler->state().all(state));
}

void Manager::renameHandler(Handler* handler, const QString& name) {
    Q_ASSERT(handler);
    handler->set_name(qstring2name(name));
    emit handlerRenamed(handler);
}

bool Manager::editHandler(Handler* handler) {
    QWidget* widget = editor(handler);
    if (!widget)
        widget = dynamic_cast<QWidget*>(handler); // handler is its own editor
    if (!widget)
        return false;
    QWidget* window = widget->window();
    if (window)
        window->show();
    widget->activateWindow();
    widget->raise();
    return true;
}

void Manager::setParameters(Handler* handler, const HandlerView::Parameters& parameters) {
    Q_ASSERT(handler);
    HandlerView* view = dynamic_cast<GraphicalHandler*>(handler);
    if (!view)
        view = dynamic_cast<HandlerView*>(editor(handler));
    if (view)
        view->setParameters(parameters);
    else
        qWarning() << "no suitable class for setting" << handlerName(handler) << "parameters";
}

void Manager::insertHandler(Handler* handler, HandlerEditor* editor, const QString& type, const QString& group) {
    Q_ASSERT(handler && !handler->holder());
    auto graphicalHandler = dynamic_cast<GraphicalHandler*>(handler);
    // set context
    if (graphicalHandler)
        graphicalHandler->setContext(this);
    if (editor)
        editor->setContext(this);
    // set holder
    handler->set_holder(graphicalHandler ? mGUIHolder : holderAt(group));
    // set receiver
    auto customReceiver = dynamic_cast<CustomReceiver*>(handler->receiver());
    if (customReceiver)
        customReceiver->setObserver(mReceiver);
    else
        handler->set_receiver(mReceiver);
    // register instance
    Data& data = mStorage[handler];
    data.editor = editor;
    data.type = type;
    data.owns = graphicalHandler == nullptr;
    // opens it
    setHandlerOpen(handler, true);
    // notify listeners
    emit handlerInserted(handler);
}

void Manager::removeHandler(Handler* handler) {
    Q_ASSERT(handler);
    // remove from known handlers
    Data data = mStorage.take(handler);
    // clear sinks
    setSinks(handler, Handler::sinks_type());
    // clear usages
    for (Handler* h : getHandlers()) {
        Handler::sinks_type sinks = h->sinks();
        sinks.erase(handler);
        for (auto& pair : sinks)
            pair.second.remove_usage(handler);
        setSinks(h, sinks);
    }
    // notify listeners
    emit handlerRemoved(handler);
    // delete editor
    if (data.editor)
        data.editor->deleteLater();
    // delete object
    QObject* qobject = dynamic_cast<QObject*>(handler);
    if (qobject) {
        qobject->deleteLater();
    } else {
        delete handler;
    }
}

void Manager::setSinks(Handler* handler, Handler::sinks_type sinks) {
    handler->set_sinks(std::move(sinks));
    emit sinksChanged(handler);
}

void Manager::insertConnection(Handler* tail, Handler* head) {
    Q_ASSERT(tail && head);
    if (tail == head) {
        TRACE_ERROR("insertConnection fails: the tail can't be the head");
        return;
    }
    Handler::sinks_type sinks = tail->sinks();
    sinks[head] = Filter(); // always match
    setSinks(tail, sinks);
}

void Manager::insertConnection(Handler* tail, Handler* head, Handler* source) {
    Q_ASSERT(tail && head && source);
    if (tail == head) {
        TRACE_ERROR("insertConnection fails: the tail can't be the head");
        return;
    }
    Handler::sinks_type sinks = tail->sinks();
    auto it = sinks.find(head);
    if (it == sinks.end()) // new filter
        sinks.emplace(head, Filter::handler(source));
    else if (it->second.match_handler(source)) // source already always match
        return;
    else // add filter to the existing one
        it->second = it->second | Filter::handler(source);
    setSinks(tail, sinks);
}

void Manager::removeConnection(Handler* tail, Handler* head) {
    Q_ASSERT(tail && head);
    auto sinks = tail->sinks();
    if (sinks.erase(head))
        setSinks(tail, sinks);
}

void Manager::removeConnection(Handler* tail, Handler* head, Handler* source) {
    Q_ASSERT(tail && head && source);
    Handler::sinks_type sinks = tail->sinks();
    auto it = sinks.find(head);
    if (it != sinks.end()) {
        if (it->second.remove_usage(source)) {
            if (!it->second.match_nothing())
                sinks.erase(it);
            setSinks(tail, sinks);
        }
    }
}

Holder* Manager::holderAt(const QString& group) {
    std::string name = group.toStdString();
    for (std::unique_ptr<StandardHolder>& holder : mHolders)
        if (holder->name() == name)
            return holder.get();
    StandardHolder* holder = new StandardHolder(priority_t::highest, name); // let SF threads the opportunity to be realtime
    TRACE_DEBUG("creating holder " << name << " (" << holder->get_id() << ") ...");
    mHolders.emplace_back(holder);
    return holder;
}

void Manager::quit() {
    // ensure being called only once
    // it seems lastWindowClosed can be emitted multiple times
    static bool visited = false;
    if (visited)
        return;
    visited = true;
    // auto disconnect
    TRACE_DEBUG("disconnecting handlers ...");
    for (Handler* handler : getHandlers())
        handler->set_sinks(Handler::sinks_type());
    // delete orphan displayers
    TRACE_DEBUG("deleting displayers ...");
    for (MultiDisplayer* multiDisplayer : MultiDisplayer::topLevelDisplayers())
        multiDisplayer->deleteLater();
    // save path settings
    QSettings settings;
    settings.setValue("paths/midi", mPathRetrievers["midi"]->dir());
    settings.setValue("paths/soundfont", mPathRetrievers["soundfont"]->dir());
    settings.setValue("paths/configuration", mPathRetrievers["configuration"]->dir());
    // now we can quit
    qApp->quit();
}
