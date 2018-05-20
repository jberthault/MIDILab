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

#include <QApplication>
#include <QMainWindow>
#include "qcore/manager.h"
#include "qtools/displayer.h"

namespace {

//=====================
// ConfigurationPuller
//=====================

class ConfigurationPuller {

public:
    void addConfiguration(const Configuration& configuration) {
        // add frames
        auto mainDisplayer = Manager::instance->mainDisplayer();
        if (!configuration.frames.empty())
            setFrame(mainDisplayer, configuration.frames[0], true);
        for (int i=1 ; i < configuration.frames.size() ; i++)
            addFrame(mainDisplayer, configuration.frames[i], true);
        // add handlers
        for (const auto& handler : configuration.handlers)
            addHandler(handler);
        // add connections
        for (const auto& connection : configuration.connections)
            addConnection(connection);
        // set colors
        for (channel_t c=0 ; c < std::min(channels_t::capacity(), (size_t)configuration.colors.size()) ; ++c)
            Manager::instance->channelEditor()->setColor(c, configuration.colors.at(c));
        // display visible frames created
        for (auto* displayer : mVisibleDisplayers)
            displayer->show();
    }

    void addConnection(const Configuration::Connection& connection) {
        bool hasSource = !connection.source.isEmpty();
        Handler* tail = mHandlersReferences.value(connection.tail, nullptr);
        Handler* head = mHandlersReferences.value(connection.head, nullptr);
        Handler* source = hasSource ? mHandlersReferences.value(connection.source, nullptr) : nullptr;
        if (!tail || !head || (hasSource && !source)) {
            TRACE_WARNING("wrong connection handlers: " << handlerName(tail) << ' ' << handlerName(head) << ' ' << handlerName(source));
            return;
        }
        Manager::instance->insertConnection(tail, head, hasSource ? Filter::handler(source) : Filter());
    }

    void addHandler(const Configuration::Handler& handler) {
        // create the handler
        auto host = mViewReferences.value(handler.id, nullptr);
        auto proxy = Manager::instance->loadHandler(handler.type, handler.name, host);
        if (proxy.handler()) {
            // register handler
            mHandlersReferences[handler.id] = proxy.handler();
            // set handler parameters
            for (const auto& prop : handler.properties)
                proxy.setParameter({prop.key, prop.value});
            // if the view does not belong to a frame, make it visible
            if (!host && proxy.view())
                mVisibleDisplayers.push_back(proxy.view()->window());
        } else {
            TRACE_ERROR("Unable to build handler " << handler.type << "(\"" << handler.name << "\")");
        }
    }

    void addWidget(MultiDisplayer* parent, const Configuration::Widget& widget) {
        if (widget.isFrame)
            addFrame(parent, widget.frame, false);
        else
            addView(parent, widget.view);
    }

    void addFrame(MultiDisplayer* parent, const Configuration::Frame& frame, bool isTopLevel) {
        MultiDisplayer* displayer = isTopLevel ? parent->insertDetached() : parent->insertMulti();
        setFrame(displayer, frame, isTopLevel);
        if (isTopLevel && frame.visible)
            mVisibleDisplayers.push_back(displayer);
    }

    void setFrame(MultiDisplayer* displayer, const Configuration::Frame& frame, bool isTopLevel) {
        displayer->setOrientation(frame.layout);
        for (const auto& widget : frame.widgets)
            addWidget(displayer, widget);
        if (isTopLevel) {
            auto window = displayer->window();
            window->setWindowTitle(frame.name);
            if (frame.size.isValid())
                window->resize(frame.size);
            if (!frame.pos.isNull())
                window->move(frame.pos);
        }
    }

    void addView(MultiDisplayer* parent, const Configuration::View& view) {
        mViewReferences[view.ref] = parent->insertSingle();
    }

private:
    QMap<QString, Handler*> mHandlersReferences;
    QMap<QString, SingleDisplayer*> mViewReferences;
    std::vector<QWidget*> mVisibleDisplayers;

};

//=====================
// ConfigurationPuller
//=====================

class ConfigurationPusher {

public:

    struct Info {
        HandlerProxy proxy;
        Configuration::Handler parsingData;
    };

    ConfigurationPusher() {
        int id=0;
        const auto& proxies = Manager::instance->handlerProxies();
        auto it = proxies.begin();
        mCache.resize(proxies.size());
        for (auto& info : mCache) {
            info.proxy = *it;
            info.parsingData.type = info.proxy.metaHandler()->identifier();
            info.parsingData.id = QString("#%1").arg(id++);
            info.parsingData.name = info.proxy.name();
            for (const auto& parameter : info.proxy.getParameters())
                info.parsingData.properties.push_back(Configuration::Property{parameter.name, parameter.value});
            ++it;
        }
    }

    auto getSource(const Filter& filter) const {
        QStringList sources;
        if (boost::logic::indeterminate(filter.match_nothing()))
            for (const auto& info : mCache)
                if (filter.match_handler(info.proxy.handler()))
                    sources.append(info.parsingData.id);
        return sources.join("|");
    }

    const Info& getInfo(const Handler* handler) const {
        return *std::find_if(mCache.begin(), mCache.end(), [=](const auto& info) { return handler == info.proxy.handler(); });
    }

    Configuration::Connection getConnection(const QString& tailId, const Listener& listener) const {
        return {tailId, getInfo(listener.handler).parsingData.id, getSource(listener.filter)};
    }

    Configuration::View getView(SingleDisplayer* displayer) const {
        for (const auto& info : mCache)
            if (displayer->widget() == info.proxy.view())
                return {info.parsingData.id};
        return {};
    }

    Configuration::Frame getFrame(MultiDisplayer* displayer) const {
        auto window = displayer->window();
        Configuration::Frame frame;
        frame.layout = displayer->orientation();
        frame.name = window->windowTitle();
        frame.pos = window->pos();
        frame.size = window->size();
        frame.visible = window->isVisible();
        for (auto* child : displayer->directChildren())
            frame.widgets.push_back(getWidget(child));
        return frame;
    }

    Configuration::Widget getWidget(Displayer* displayer) const {
        if (auto* multiDisplayer = dynamic_cast<MultiDisplayer*>(displayer))
            return {true, getFrame(multiDisplayer), {}};
        else
            return {false, {}, getView(static_cast<SingleDisplayer*>(displayer))};
    }

    auto getConfiguration() const {
        Configuration config;
        // handlers
        for (const auto& info : mCache)
            config.handlers.push_back(info.parsingData);
        // connections
        for (const auto& info : mCache)
            for (const auto& listener : info.proxy.handler()->listeners())
                config.connections.push_back(getConnection(info.parsingData.id, listener));
        // frames
        config.frames.push_back(getFrame(Manager::instance->mainDisplayer()));
        for (auto* displayer : MultiDisplayer::topLevelDisplayers())
            config.frames.push_back(getFrame(displayer));
        // colors
        for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
            config.colors.push_back(Manager::instance->channelEditor()->color(c));
        return config;
    }

private:
     std::vector<Info> mCache;

};

}

//=========
// Deleter
//=========

void Deleter::addProxy(const HandlerProxy& proxy) {
    mProxies.push_back(proxy);
    startDeletion();
}

void Deleter::addProxies(const HandlerProxies& proxies) {
    mProxies.insert(mProxies.end(), proxies.begin(), proxies.end());
    startDeletion();
}

void Deleter::startDeletion() {
    if (mTimerId == 0)
        mTimerId = startTimer(20); // 50 Hz
}

void Deleter::stopDeletion() {
    if (mTimerId != 0) {
        killTimer(mTimerId);
        mTimerId = 0;
    }
}

void Deleter::timerEvent(QTimerEvent*) {
    if (deleteProxies()) {
        stopDeletion();
        emit deleted();
    }
}

bool Deleter::deleteProxies() {
    while (true) {
        auto px  = takeProxyIf(mProxies, [](const auto& proxy) { return proxy.handler()->is_consumed(); });
        if (px.view())
            delete px.view();
        else if (px.handler())
            delete px.handler();
        else
            break;
    }
    return mProxies.empty();
}

//=========
// Manager
//=========

Manager* Manager::instance = nullptr;

Manager::Manager(QObject* parent) : Context{parent} {

    Q_ASSERT(instance == nullptr);
    instance = this;

    mChannelEditor = new ChannelEditor{static_cast<QWidget*>(parent)};
    mChannelEditor->setWindowFlags(Qt::Dialog);

    mGUISynchronizer = new GraphicalSynchronizer{this};
    mPathRetrieverPool = new PathRetrieverPool{this};
    mMetaHandlerPool = new MetaHandlerPool{this};
    mObserver = new Observer{this};
    mDeleter = new Deleter{this};

    qApp->setQuitOnLastWindowClosed(false);
    connect(qApp, &QApplication::lastWindowClosed, this, &Manager::clearConfiguration);
    connect(mDeleter, &Deleter::deleted, this, &Manager::onDeletion);

}

Manager::~Manager() {
    Q_ASSERT(mHandlerProxies.empty());
    instance = nullptr;
}

MultiDisplayer* Manager::mainDisplayer() const {
    return dynamic_cast<MultiDisplayer*>(static_cast<QMainWindow*>(parent())->centralWidget());
}

Observer* Manager::observer() const {
    return mObserver;
}

MetaHandlerPool* Manager::metaHandlerPool() const {
    return mMetaHandlerPool;
}

ChannelEditor* Manager::channelEditor() {
    return mChannelEditor;
}

const HandlerProxies& Manager::handlerProxies() const {
    return mHandlerProxies;
}

PathRetrieverPool* Manager::pathRetrieverPool() {
    return mPathRetrieverPool;
}

Configuration Manager::getConfiguration() {
    TRACE_MEASURE("get configuration");
    ConfigurationPusher pusher;
    return pusher.getConfiguration();
}

void Manager::setConfiguration(const Configuration& configuration) {
    TRACE_MEASURE("set configuration");
    ConfigurationPuller puller;
    puller.addConfiguration(configuration);
}

void Manager::clearConfiguration() {
    TRACE_MEASURE("clear configuration");
    HandlerProxies proxies;
    // clear proxies
    mHandlerProxies.swap(proxies);
    // clear listeners
    for (const auto& proxy : proxies)
        setListeners(proxy.handler(), {});
    // notify listening slots
    for (const auto& proxy : proxies)
        emit handlerRemoved(proxy.handler());
    // schedule handlers deletion
    mDeleter->addProxies(proxies);
}

HandlerProxy Manager::loadHandler(MetaHandler* meta, const QString& name, SingleDisplayer* host) {
    auto proxy = meta ? meta->instantiate(name) : HandlerProxy{};
    // set view's parent
    if (auto* view = proxy.view()) {
        if (!host)
            host = mainDisplayer()->insertDetached()->insertSingle();
        host->setWidget(view);
    }
    // insert the handler
    if (proxy.handler()) {
        Q_ASSERT(!proxy.handler()->synchronizer());
        if (proxy.editable())
            proxy.handler()->set_synchronizer(mGUISynchronizer);
        else
            proxy.handler()->set_synchronizer(&mDefaultSynchronizer);
        proxy.setObserver(mObserver);
        proxy.setContext(this);
        proxy.setState(true);
        mHandlerProxies.push_back(proxy);
        emit handlerInserted(proxy.handler());
    }
    return proxy;
}

HandlerProxy Manager::loadHandler(const QString& type, const QString& name, SingleDisplayer* host) {
    return loadHandler(mMetaHandlerPool->get(type), name, host);
}

void Manager::removeHandler(Handler* handler) {
    Q_ASSERT(handler);
    // take proxy
    auto px = takeProxy(mHandlerProxies, handler);
    // clear related listeners
    setListeners(handler, {});
    for (const auto& proxy : mHandlerProxies) {
        auto listeners = proxy.handler()->listeners();
        if (listeners.remove_usage(handler))
            setListeners(proxy.handler(), std::move(listeners));
    }
    if (px.handler()) {
        // notify listening slots
        emit handlerRemoved(handler);
        // schedule deletion
        mDeleter->addProxy(px);
    }
}

void Manager::renameHandler(Handler* handler, const QString& name) {
    Q_ASSERT(handler);
    handler->set_name(qstring2name(name));
    emit handlerRenamed(handler);
}

void Manager::setListeners(Handler* handler, Listeners listeners) {
    handler->set_listeners(std::move(listeners));
    emit handlerListenersChanged(handler);
}

void Manager::insertConnection(Handler* tail, Handler* head, const Filter& filter) {
    Q_ASSERT(tail && head);
    if (tail == head) {
        TRACE_ERROR("insertConnection fails: the tail can't be the head");
        return;
    }
    auto listeners = tail->listeners();
    if (listeners.insert(head, filter))
        setListeners(tail, std::move(listeners));
}

void Manager::removeConnection(Handler* tail, Handler* head) {
    Q_ASSERT(tail && head);
    auto listeners = tail->listeners();
    if (listeners.erase(head))
        setListeners(tail, std::move(listeners));
}

void Manager::removeConnection(Handler* tail, Handler* head, Handler* source) {
    Q_ASSERT(tail && head && source);
    auto listeners = tail->listeners();
    if (listeners.remove_usage(head, source))
        setListeners(tail, std::move(listeners));
}

void Manager::onDeletion() {
    TRACE_DEBUG("deletion done");
    if (static_cast<QWidget*>(parent())->isHidden())
        qApp->quit();
}
