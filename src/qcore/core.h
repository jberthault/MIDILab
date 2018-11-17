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

#ifndef QCORE_CORE_H
#define QCORE_CORE_H

#include <QEvent>
#include <QFormLayout>
#include <QDir>
#include <QToolBar>
#include "core/handler.h"
#include "qtools/misc.h"

/*!< enable QVariant to accept those types */
Q_DECLARE_METATYPE(Message)
Q_DECLARE_METATYPE(Handler*)

/*!< enable QString support for std::stream */
using namespace qoperators;

//=================
// Name Conversion
//=================

class MetaHandler;

std::string qstring2name(const QString& string);
QString name2qstring(const std::string& name);

QString handlerName(const Handler* handler);
QString eventName(const Event& event);
QString metaHandlerName(const MetaHandler* meta);

//=======================
// GraphicalSynchronizer
//=======================

/// This synchronizer is designed to use Qt Event loop to distribute messages in a thread-safe way
/// This object and client Handlers (EditableHandler) are supposed to live in the same thread

class GraphicalSynchronizer : public QObject, public Synchronizer {

    Q_OBJECT

public:
    explicit GraphicalSynchronizer(QObject* parent);

    void sync_handler(Handler* target) final;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    boost::lockfree::queue<Handler*> mQueue {128};

};

//==========
// Observer
//==========

class Observer : public QObject, public Interceptor {

    Q_OBJECT

public:
    explicit Observer(QObject* parent);

    Result seizeOne(Handler* target, const Message& message);
    void seizeAll(Handler* target, const Messages& messages);

    void seize_messages(Handler* target, const Messages& messages) final;

protected:
    void timerEvent(QTimerEvent* event) override;

signals:
    void messageHandled(Handler* handler, const Message& message);

private:
    using Item = std::pair<Handler*, Message>;
    Queue<std::vector<Item>> mQueue;

};

//=======================
// ObservableInterceptor
//=======================

class ObservableInterceptor : public QObject, public Interceptor {

    Q_OBJECT

public:
    explicit ObservableInterceptor(QObject* parent);

    Result seizeOne(Handler* target, const Message& message);
    void seizeAll(Handler* target, const Messages& messages);

    Observer* observer();
    void setObserver(Observer* observer);

private:
    Observer* mObserver;

};

//=============
// HandlerView
//=============

class Context;
class ChannelEditor;

class HandlerView : public QWidget {

    Q_OBJECT

public:
    struct Parameter {
        QString name;
        QString value;
    };

    using Parameters = std::vector<Parameter>;

    explicit HandlerView();

    ChannelEditor* channelEditor(); /*!< shortcut for context()->channelEditor() */

    Context* context();
    void setContext(Context* context);

    virtual Parameters getParameters() const;
    virtual size_t setParameter(const Parameter& parameter);

protected:
    virtual void updateContext(Context* context);

private:
    Context* mContext;

};

//=================
// EditableHandler
//=================

class EditableHandler : public HandlerView, public Handler {

    Q_OBJECT

public:
    explicit EditableHandler(Mode mode);

};

//===============
// HandlerEditor
//===============

class HandlerEditor : public HandlerView {

    Q_OBJECT

public:
    using HandlerView::HandlerView;

    virtual Handler* getHandler() = 0;

};

//==============
// HandlerProxy
//==============

class MetaHandler;

class HandlerProxy {

public:
    using Mode = Handler::Mode;
    using State = Handler::State;

    using Parameter = HandlerView::Parameter;
    using Parameters = HandlerView::Parameters;

    enum class Command {
        Open,
        Close,
        Toggle
    };

    Handler* handler() const;
    HandlerView* view() const;
    EditableHandler* editable() const;
    HandlerEditor* editor() const;
    MetaHandler* metaHandler() const;

    void setContent(Handler* handler);
    void setContent(HandlerEditor* editor);
    void setMetaHandler(MetaHandler* metaHandler);

    void setObserver(Observer* observer) const;

    QString name() const;
    void setName(const QString& name) const;

    void sendCommand(Command command, State state = State::duplex()) const;

    Parameters getParameters() const;
    size_t setParameter(const Parameter& parameter, bool notify = true) const;
    size_t setParameters(const Parameters& parameters, bool notify = true) const;
    size_t resetParameter(const QString& name, bool notify = true) const;
    size_t resetParameters(bool notify = true) const;
    void notifyParameters() const;

    Context* context() const;
    void setContext(Context* context) const;

    void show() const;

private:
    Handler* mHandler {nullptr};
    HandlerView* mView {nullptr};
    MetaHandler* mMetaHandler {nullptr};

};

using HandlerProxies = std::vector<HandlerProxy>;

template<typename PredicateT>
auto getProxyIf(const HandlerProxies& proxies, PredicateT predicate) {
    auto it = std::find_if(proxies.begin(), proxies.end(), predicate);
    return it != proxies.end() ? *it : HandlerProxy{};
}

template<typename PredicateT>
auto takeProxyIf(HandlerProxies& proxies, PredicateT predicate) {
    HandlerProxy proxy;
    auto it = std::find_if(proxies.begin(), proxies.end(), predicate);
    if (it != proxies.end()) {
        proxy = *it;
        proxies.erase(it);
    }
    return proxy;
}

HandlerProxy getProxy(const HandlerProxies& proxies, const Handler* handler);
HandlerProxy takeProxy(HandlerProxies& proxies, const Handler* handler);

//=====================
// HandlerProxyFactory
//=====================

class HandlerProxyFactory : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    virtual HandlerProxy instantiate(const QString& name) = 0;

};

//==================
// OpenProxyFactory
//==================

template<typename ContentT>
class OpenProxyFactory final : public HandlerProxyFactory {

public:
    using HandlerProxyFactory::HandlerProxyFactory;

    HandlerProxy instantiate(const QString& name) override {
        HandlerProxy proxy;
        proxy.setContent(new ContentT);
        proxy.setName(name);
        return proxy;
    }

};

//====================
// ClosedProxyFactory
//====================

class ClosedProxyFactory : public HandlerProxyFactory {

    Q_OBJECT

public:
    using HandlerProxyFactory::HandlerProxyFactory;

    virtual QStringList instantiables() = 0;

};

//=============
// MetaHandler
//=============

class MetaHandler final : public QObject {

    Q_OBJECT

public:

    struct MetaParameter {

        enum class Visibility { basic, advanced, hidden };

        QString name; /*!< identifier of this parameter */
        QString description; /*!< description of whatever is represented by this parameter */
        QString defaultValue; /*!< value considered if not specified, empty means N/A */
        Visibility visibility; /*!< control acessibility of the parameter */
    };

    using MetaParameters = std::vector<MetaParameter>;

    using QObject::QObject;

    const QString& identifier() const;
    const QString& description() const;
    const MetaParameters& parameters() const;
    HandlerProxyFactory* factory();

    void setIdentifier(const QString& identifier);
    void setDescription(const QString& description);
    void addParameters(const MetaParameters& parameters);
    void addParameter(MetaParameter parameter);
    void setFactory(HandlerProxyFactory* factory);

    HandlerProxy instantiate(const QString& name);

private:
    QString mIdentifier;
    QString mDescription;
    MetaParameters mParameters;
    HandlerProxyFactory* mFactory {nullptr};

};

using MetaHandlers = std::vector<MetaHandler*>;

//====================
// MetaHandlerFactory
//====================

#define MetaHandlerFactory_iid "midi.MetaHandlerFactory"

class MetaHandlerFactory {

public:
    virtual ~MetaHandlerFactory() = default;

    virtual const MetaHandlers& spawn() const = 0;

};

Q_DECLARE_INTERFACE(MetaHandlerFactory, MetaHandlerFactory_iid)

//=================
// MetaHandlerPool
//=================

class MetaHandlerPool : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    const MetaHandlers& metaHandlers() const;

    MetaHandler* get(const QString& identifier); /*!< return the meta handler that has the given type identifier, nullptr if not exists */

    size_t addMetaHandler(MetaHandler* meta);
    size_t addFactory(MetaHandlerFactory* factory);
    size_t addPlugin(const QString& filename);
    size_t addPlugins(const QDir& dir);

private:
    MetaHandlers mMetaHandlers;

};

//===================
// PathRetrieverPool
//===================

class PathRetrieverPool : public QObject {

    Q_OBJECT

public:
    explicit PathRetrieverPool(QObject* parent);

    void load();
    void save();

    PathRetriever* get(const QString& type);

private:
    std::map<QString, PathRetriever*> mPathRetrievers;

};

//=========
// Context
//=========

class Context : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    virtual ChannelEditor* channelEditor() = 0;
    virtual const HandlerProxies& handlerProxies() const = 0;
    virtual PathRetrieverPool* pathRetrieverPool() = 0;
    virtual QToolBar* quickToolBar() = 0;

signals:
    void handlerInserted(Handler* handler);
    void handlerRenamed(Handler* handler);
    void handlerRemoved(Handler* handler);
    void handlerListenersChanged(Handler* handler);
    void handlerParametersChanged(Handler* handler);

};

#endif // QCORE_CORE_H
