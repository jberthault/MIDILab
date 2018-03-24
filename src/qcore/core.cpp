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

#include <QPluginLoader>
#include <QApplication>
#include <QSettings>
#include "qcore/core.h"

//=================
// Name Conversion
//=================

std::string qstring2name(const QString& string) {
    return {string.toUtf8().constData()};
}

QString name2qstring(const std::string& name) {
    return QString::fromUtf8(name.data());
}

QString handlerName(const Handler* handler) {
    return handler ? name2qstring(handler->name()) : "null";
}

QString eventName(const Event& event) {
    return QString::fromLocal8Bit(event.name());
}

QString metaHandlerName(const MetaHandler* meta) {
    return meta ? meta->identifier() : "Unknown MetaHandler";
}

//=======================
// GraphicalSynchronizer
//=======================

GraphicalSynchronizer::GraphicalSynchronizer(QObject* parent) : QObject{parent}, Synchronizer{}, mQueue{128} {
    startTimer(1);
}

void GraphicalSynchronizer::sync_handler(Handler* target) {
    mQueue.push(target);
}

void GraphicalSynchronizer::timerEvent(QTimerEvent*) {
    mQueue.consume_all([](auto* handler) {
        handler->flush_messages();
    });
}

//==========
// Observer
//==========

Observer::Observer(QObject* parent) : QObject{parent}, Interceptor{} {
    startTimer(5);
}

Handler::Result Observer::seizeOne(Handler* target, const Message& message) {
    const auto result = target->handle_message(message);
    if (result == Handler::Result::success && message.event.is(~families_t::note()))
        mQueue.produce(Item{target, message});
    return result;
}

void Observer::seizeAll(Handler* target, const Messages& messages) {
    for (const auto& message : messages)
        seizeOne(target, message);
}

void Observer::seize_messages(Handler* target, const Messages& messages) {
    seizeAll(target, messages);
}

void Observer::timerEvent(QTimerEvent*) {
    mQueue.unsynchronized_consume([this](const auto& items) {
        for (const auto& item : items)
            emit messageHandled(item.first, item.second);
    });
}

//=======================
// ObservableInterceptor
//=======================

ObservableInterceptor::ObservableInterceptor(QObject* parent) : QObject(parent), Interceptor(), mObserver(nullptr) {

}

Handler::Result ObservableInterceptor::seizeOne(Handler* target, const Message& message) {
    return mObserver->seizeOne(target, message);
}

void ObservableInterceptor::seizeAll(Handler* target, const Messages& messages) {
    mObserver->seizeAll(target, messages);
}

Observer* ObservableInterceptor::observer() {
    return mObserver;
}

void ObservableInterceptor::setObserver(Observer* observer) {
    mObserver = observer;
}

//=============
// HandlerView
//=============

HandlerView::HandlerView() : QWidget(nullptr), mContext(nullptr) {

}

ChannelEditor* HandlerView::channelEditor() {
    return context() ? context()->channelEditor() : nullptr;
}

Context* HandlerView::context() {
    return mContext;
}

void HandlerView::setContext(Context* context) {
    updateContext(context);
    mContext = context;
}

HandlerView::Parameters HandlerView::getParameters() const {
    return {};
}

size_t HandlerView::setParameter(const Parameter& /*parameter*/) {
    return 0;
}

void HandlerView::updateContext(Context* /*context*/) {

}

//=================
// EditableHandler
//=================

EditableHandler::EditableHandler(Mode mode) : HandlerView(), Handler(mode) {

}

//==============
// HandlerProxy
//==============

HandlerProxy::HandlerProxy(MetaHandler* metaHandler) : mHandler(nullptr), mView(nullptr), mMetaHandler(metaHandler) {

}

Handler* HandlerProxy::handler() const {
    return mHandler;
}

HandlerView* HandlerProxy::view() const {
    return mView;
}

EditableHandler* HandlerProxy::editable() const {
    return dynamic_cast<EditableHandler*>(mHandler);
}

HandlerEditor* HandlerProxy::editor() const {
    return dynamic_cast<HandlerEditor*>(mView);
}

MetaHandler* HandlerProxy::metaHandler() const {
    return mMetaHandler;
}

void HandlerProxy::setContent(Handler* handler) {
    mHandler = handler;
    mView = dynamic_cast<EditableHandler*>(handler);
}

void HandlerProxy::setContent(HandlerEditor* editor) {
    mHandler = editor->getHandler();
    mView = editor;
    Q_ASSERT(editable() == nullptr);
}

void HandlerProxy::setObserver(Observer* observer) const {
    if (mHandler) {
        if (auto* interceptor = dynamic_cast<ObservableInterceptor*>(mHandler->interceptor()))
            interceptor->setObserver(observer);
        else
            mHandler->set_interceptor(observer);
    }
}

QString HandlerProxy::name() const {
    return handlerName(mHandler);
}

void HandlerProxy::setName(const QString& name) const {
    if (mHandler)
        mHandler->set_name(qstring2name(name));
}

Handler::State HandlerProxy::currentState() const {
    return mHandler ? mHandler->state() : Handler::State{};
}

Handler::State HandlerProxy::supportedState() const {
    Handler::State state;
    if (mHandler) {
        if (mHandler->mode().any(Mode::forward()))
            state |= State::forward();
        if (mHandler->mode().any(Mode::receive()))
            state |= State::receive();
    }
    return state;
}

void HandlerProxy::setState(bool open, State state) const {
    if (mHandler) {
        if (mHandler->mode().any(Mode::thru())) {
            // thru mode should process all states at once
            state = State::endpoints();
        } else {
            // do not process states that are not supported
            state &= supportedState();
        }
        if (mHandler->state().all(state) == open) {
            TRACE_WARNING(name() << " handler already " << (open ? "open" : "closed"));
            return;
        }
        mHandler->send_message(open ? Handler::open_event(state) : Handler::close_event(state));
    }
}

void HandlerProxy::toggleState(State state) const {
    bool isOpen = currentState().all(state & supportedState());
    setState(!isOpen, state);
}

HandlerProxy::Parameters HandlerProxy::getParameters() const {
    return mView ? mView->getParameters() : Parameters{};
}

void HandlerProxy::setParameter(const Parameter& parameter) const {
    if (mView && !mView->setParameter(parameter))
        TRACE_WARNING(name() << ": unable to set parameter " << parameter.name);
}

void HandlerProxy::setParameters(const Parameters& parameters) const {
    for (const auto& parameter : parameters)
        setParameter(parameter);
}

Context* HandlerProxy::context() const {
    return mView ? mView->context() : nullptr;
}

void HandlerProxy::setContext(Context* context) const {
    if (mView)
        mView->setContext(context);
}

void HandlerProxy::show() const {
    if (mView) {
        mView->window()->show();
        mView->activateWindow();
        mView->raise();
    }
}

void HandlerProxy::destroy() {
    if (mView)
        mView->deleteLater();
    else if (mHandler)
        delete mHandler;
    mHandler = nullptr;
    mView = nullptr;
}

HandlerProxy getProxy(const HandlerProxies& proxies, const Handler* handler) {
    auto it = std::find_if(proxies.begin(), proxies.end(), [=](const auto& proxy) { return proxy.handler() == handler; });
    return it != proxies.end() ? *it : HandlerProxy{};
}

HandlerProxy takeProxy(HandlerProxies& proxies, const Handler* handler) {
    HandlerProxy proxy;
    auto it = std::find_if(proxies.begin(), proxies.end(), [=](const auto& proxy) { return proxy.handler() == handler; });
    if (it != proxies.end()) {
        proxy = *it;
        proxies.erase(it);
    }
    return proxy;
}

//=============
// MetaHandler
//=============

const QString& MetaHandler::identifier() const {
    return mIdentifier;
}

const QString& MetaHandler::description() const {
    return mDescription;
}

void MetaHandler::setIdentifier(const QString& identifier) {
    mIdentifier = identifier;
}

void MetaHandler::setDescription(const QString& description) {
    mDescription = description;
}

const MetaHandler::MetaParameters& MetaHandler::parameters() const {
    return mParameters;
}

void MetaHandler::addParameters(const MetaParameters& parameters) {
    mParameters.insert(mParameters.end(), parameters.begin(), parameters.end());
}

void MetaHandler::addParameter(const MetaParameter& parameter) {
    mParameters.push_back(parameter);
}

void MetaHandler::addParameter(const QString& name, const QString& type, const QString& description, const QString& defaultValue) {
    addParameter({name, type, description, defaultValue});
}

//=================
// OpenMetaHandler
//=================

HandlerProxy OpenMetaHandler::instantiate(const QString& name) {
    HandlerProxy proxy(this);
    setContent(proxy);
    proxy.setName(name);
    return proxy;
}

//=================
// MetaHandlerPool
//=================

const QList<MetaHandler*>& MetaHandlerPool::metaHandlers() const {
    return mMetaHandlers;
}

MetaHandler* MetaHandlerPool::get(const QString& identifier) {
    for (auto* metaHandler : mMetaHandlers)
        if (metaHandler->identifier() == identifier)
            return metaHandler;
    return nullptr;
}

size_t MetaHandlerPool::addMetaHandler(MetaHandler* meta) {
    if (meta) {
        meta->setParent(this);
        mMetaHandlers << meta;
        return 1;
    }
    return 0;
}

size_t MetaHandlerPool::addFactory(MetaHandlerFactory* factory) {
    QList<MetaHandler*> newMetaHandlers;
    if (factory)
        newMetaHandlers = factory->spawn();
    mMetaHandlers << newMetaHandlers;
    return newMetaHandlers.size();
}

size_t MetaHandlerPool::addPlugin(const QString& filename) {
    size_t count = 0;
    QPluginLoader loader(filename, this);
    QObject* plugin = loader.instance();
    if (plugin) { /// @note lifetime is automatically managed
        count += addFactory(dynamic_cast<MetaHandlerFactory*>(plugin));
    } else {
        TRACE_WARNING("file " << filename << "is not a plugin: " << loader.errorString());
    }
    return count;
}

size_t MetaHandlerPool::addPlugins(const QDir& dir) {
    size_t count = 0;
    for (const QString& filename : dir.entryList(QDir::Files))
        if (QLibrary::isLibrary(filename))
            count += addPlugin(dir.absoluteFilePath(filename));
    if (count == 0)
        TRACE_WARNING("can't find any plugin in " << dir.absolutePath());
    return count;
}

//==================
// SynchronizerPool
//==================

SynchronizerPool::SynchronizerPool(QObject *parent) : QObject{parent} {
    mGUISynchronizer = new GraphicalSynchronizer{this};
}

SynchronizerPool::~SynchronizerPool() {
    stop();
}

void SynchronizerPool::configure(const HandlerProxy& proxy) {
    Q_ASSERT(!proxy.handler()->synchronizer());
    if (proxy.editable()) {
        proxy.handler()->set_synchronizer(mGUISynchronizer);
    } else {
        mDefaultSynchronizer.start();
        proxy.handler()->set_synchronizer(&mDefaultSynchronizer);
    }
}

void SynchronizerPool::stop() {
    mDefaultSynchronizer.stop();
}

//===================
// PathRetrieverPool
//===================

namespace {

void initializePathRetriever(PathRetriever* pathRetriever, const QString& caption, const QString& filters) {
    pathRetriever->setCaption(caption);
    pathRetriever->setFilter(QString("%1 (%2);;All Files (*)").arg(caption, filters));
}

}

PathRetrieverPool::PathRetrieverPool(QObject* parent) : QObject{parent} {
    initializePathRetriever(get("midi"), "MIDI Files", "*.mid *.midi *.kar");
    initializePathRetriever(get("soundfont"), "SoundFont Files", "*.sf2");
    initializePathRetriever(get("configuration"), "Configuration Files", "*.xml");
}

void PathRetrieverPool::load() {
    QSettings settings;
    settings.beginGroup("paths");
    for (const auto& key : settings.childKeys())
        get(key)->setDir(settings.value(key).toString());
    settings.endGroup();
}

void PathRetrieverPool::save() {
    QSettings settings;
    settings.beginGroup("paths");
    for (const auto& pair : mPathRetrievers)
        settings.setValue(pair.first, pair.second->dir());
    settings.endGroup();
}

PathRetriever* PathRetrieverPool::get(const QString& type) {
    auto& retriever = mPathRetrievers[type];
    if (!retriever)
        retriever = new PathRetriever{this};
    return retriever;
}
