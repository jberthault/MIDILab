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

#include <QPluginLoader>
#include <QApplication>
#include <QSettings>
#include "qcore/core.h"

range_t<ChildItemIterator> makeChildRange(QTreeWidgetItem* root) {
    return {{root, 0}, {root, root->childCount()}};
}

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
    return QString::fromLocal8Bit(family_name(event.family()));
}

QString metaHandlerName(const MetaHandler* meta) {
    return meta ? meta->identifier() : "Unknown MetaHandler";
}

//=======================
// GraphicalSynchronizer
//=======================

GraphicalSynchronizer::GraphicalSynchronizer(QObject* parent) : QObject{parent}, Synchronizer{} {
    startTimer(8); // 125 Hz
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
    startTimer(50); // 20 Hz
}

Handler::Result Observer::seizeOne(Handler* target, const Message& message) {
    const auto result = target->receive_message(message);
    if (result == Handler::Result::success && message.event.is(~families_t::standard_note()))
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
    mQueue.consume([this](const auto& items) {
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

void HandlerProxy::setMetaHandler(MetaHandler* metaHandler) {
    mMetaHandler = metaHandler;
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

void HandlerProxy::sendCommand(Command command, State state) const {
    if (mHandler) {
        // compute supported states
        Handler::State supportedState;
        if (mHandler->mode().any(Mode::forward()))
            supportedState |= State::forward();
        if (mHandler->mode().any(Mode::receive()))
            supportedState |= State::receive();
        // compute state considered active
        Handler::State activatedState;
        switch(command) {
        case Command::Open: activatedState = {}; break;
        case Command::Close: activatedState = State::duplex(); break;
        case Command::Toggle: activatedState = mHandler->state(); break;
        }
        // send messages
        if (const auto openingState = state & supportedState & ~activatedState)
            mHandler->send_message(Handler::open_ext(openingState));
        if (const auto closingState = state & supportedState & activatedState)
            mHandler->send_message(Handler::close_ext(closingState));
    }
}

HandlerProxy::Parameters HandlerProxy::getParameters() const {
    return mView ? mView->getParameters() : Parameters{};
}

size_t HandlerProxy::setParameter(const Parameter& parameter, bool notify) const {
    const size_t count = mView ? mView->setParameter(parameter) : 0;
    if (count == 0)
        TRACE_ERROR(name() << ": unable to set parameter " << parameter.name);
    else if (notify)
        notifyParameters();
    return count;
}

size_t HandlerProxy::setParameters(const Parameters& parameters, bool notify) const {
    size_t count = 0;
    for (const auto& parameter : parameters)
        count += setParameter(parameter, false);
    if (count != 0 && notify)
        notifyParameters();
    return count;
}

size_t HandlerProxy::resetParameter(const QString& name, bool notify) const {
    const auto& parameters = mMetaHandler->parameters();
    auto metaIt = std::find_if(parameters.begin(), parameters.end(), [&](const auto& metaParameter) { return metaParameter.name == name; });
    return metaIt != parameters.end() && !metaIt->defaultValue.isNull() ? setParameter({name, metaIt->defaultValue}, notify) : 0;
}

size_t HandlerProxy::resetParameters(bool notify) const {
    size_t count = 0;
    for (const auto& parameter : mMetaHandler->parameters())
        if (!parameter.defaultValue.isNull())
            count += setParameter({parameter.name, parameter.defaultValue}, false);
    if (count != 0 && notify)
        notifyParameters();
    return count;
}

void HandlerProxy::notifyParameters() const {
    if (auto* context_ = context())
        emit context_->handlerParametersChanged(mHandler);
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

HandlerProxy getProxy(const HandlerProxies& proxies, const Handler* handler) {
    return getProxyIf(proxies, [=](const auto& proxy) { return proxy.handler() == handler; });
}

HandlerProxy takeProxy(HandlerProxies& proxies, const Handler* handler) {
    return takeProxyIf(proxies, [=](const auto& proxy) { return proxy.handler() == handler; });
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

HandlerProxyFactory* MetaHandler::factory() {
    return mFactory;
}

void MetaHandler::addParameters(const MetaParameters& parameters) {
    mParameters.insert(mParameters.end(), parameters.begin(), parameters.end());
}

void MetaHandler::addParameter(MetaParameter parameter) {
    mParameters.push_back(std::move(parameter));
}

void MetaHandler::setFactory(HandlerProxyFactory* factory) {
    Q_ASSERT(!mFactory);
    mFactory = factory;
    mFactory->setParent(this);
}

HandlerProxy MetaHandler::instantiate(const QString& name) {
    auto proxy = mFactory->instantiate(name);
    proxy.setMetaHandler(this);
    return proxy;
}

//=================
// MetaHandlerPool
//=================

const MetaHandlers& MetaHandlerPool::metaHandlers() const {
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
        mMetaHandlers.push_back(meta);
        return 1;
    }
    return 0;
}

size_t MetaHandlerPool::addFactory(MetaHandlerFactory* factory) {
    if (!factory)
        return 0;
    const auto& newMetaHandlers = factory->spawn();
    mMetaHandlers.insert(mMetaHandlers.end(), newMetaHandlers.begin(), newMetaHandlers.end());
    return newMetaHandlers.size();
}

size_t MetaHandlerPool::addPlugin(const QString& filename) {
    size_t count = 0;
    QPluginLoader loader(filename, this);
    if (auto* plugin = loader.instance()) { /// @note lifetime is automatically managed
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

//===================
// PathRetrieverPool
//===================

namespace {

void initializePathRetriever(PathRetriever* pathRetriever, const QString& caption, const QString& filters) {
    pathRetriever->setCaption(caption);
    pathRetriever->setFilter(QString{"%1 (%2);;All Files (*)"}.arg(caption, filters));
}

}

PathRetrieverPool::PathRetrieverPool(QObject* parent) : QObject{parent} {
    connect(qApp, &QApplication::aboutToQuit, this, &PathRetrieverPool::save);
    initializePathRetriever(get("midi"), "MIDI Files", "*.mid *.midi *.kar");
    initializePathRetriever(get("soundfont"), "SoundFont Files", "*.sf2");
    initializePathRetriever(get("configuration"), "Configuration Files", "*.xml");
    load();
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
