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

#include <sstream>
#include <QPluginLoader>
#include <QApplication>
#include <QMetaEnum>
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
    return QString::fromStdString(event.name());
}

QString metaHandlerName(const MetaHandler* meta) {
    return meta ? meta->identifier() : "Unknown MetaHandler";
}

//=============
// Persistence
//=============

namespace serial {

// numeric types

QString serializeBool(bool value) {
    return value ? "true" : "false";
}

bool parseBool(const QString& data, bool& value) {
    if (data == "true") {
        value = true;
        return true;
    }
    if (data == "false") {
        value = false;
        return true;
    }
    return false;
}

QString serializeByte(byte_t byte) {
    return QString::fromStdString(byte_string(byte));
}

bool parseByte(const QString& data, byte_t& byte) {
    bool ok;
    uint value = data.toUInt(&ok, 0);
    byte = value & 0xff;
    return ok && value < 0x100;
}

#define PARSE_NUMBER(method, data, value) do { \
        bool ok;                               \
        value = data.method(&ok);              \
        return ok;                             \
    } while (false)

bool parseShort(const QString& data, short& value) { PARSE_NUMBER(toShort, data, value); }
bool parseUShort(const QString& data, ushort& value) { PARSE_NUMBER(toUShort, data, value); }
bool parseInt(const QString& data, int& value) { PARSE_NUMBER(toInt, data, value); }
bool parseUInt(const QString& data, uint& value) { PARSE_NUMBER(toUInt, data, value); }
bool parseLong(const QString& data, long& value) { PARSE_NUMBER(toLong, data, value); }
bool parseULong(const QString& data, ulong& value) { PARSE_NUMBER(toULong, data, value); }
bool parseLongLong(const QString& data, qlonglong& value) { PARSE_NUMBER(toLongLong, data, value); }
bool parseULongLong(const QString& data, qulonglong& value) { PARSE_NUMBER(toULongLong, data, value); }
bool parseFloat(const QString& data, float& value) { PARSE_NUMBER(toFloat, data, value); }
bool parseDouble(const QString& data, double& value) { PARSE_NUMBER(toDouble, data, value); }

// note types

QString serializeNote(const Note& note) {
    return QString::fromStdString(note.string());
}

bool parseNote(const QString& data, Note& note) {
    note = Note::from_string(data.toStdString());
    return (bool)note;
}

QString serializeRange(const QPair<Note, Note>& range) {
    return QString("%1:%2").arg(serializeNote(range.first), serializeNote(range.second));
}

bool parseRange(const QString& data, QPair<Note, Note>& range) {
    char separator = '\0';
    std::istringstream stream(data.toStdString());
    try {
        stream >> range.first >> separator >> range.second;
        return separator == ':';
    } catch (const std::exception&) {
        return false;
    }
}

QString serializeNotes(const std::vector<Note>& notes) {
    QStringList stringList;
    for (const Note& note : notes)
        stringList.append(QString::fromStdString(note.string()));
    return stringList.join(";");
}

bool parseNotes(const QString& data, std::vector<Note>& notes) {
    for (const QString string : data.split(';')) {
        auto note = Note::from_string(string.toStdString());
        if (!note)
            return false;
        notes.push_back(note);
    }
    return true;
}

// other types

QString serializeOrientation(Qt::Orientation orientation) {
    return QMetaEnum::fromType<Qt::Orientation>().valueToKey(orientation);
}

bool parseOrientation(const QString& data, Qt::Orientation& orientation) {
    bool ok;
    orientation = static_cast<Qt::Orientation>(QMetaEnum::fromType<Qt::Orientation>().keyToValue(data.toLocal8Bit().constData(), &ok));
    return ok;
}

}

//==============
// StorageEvent
//==============

const QEvent::Type StorageEvent::family = (QEvent::Type)QEvent::registerEventType();

StorageEvent::StorageEvent(Handler* target, Message message) :
    QEvent(family), target(target), message(std::move(message)) {

}

//=================
// GraphicalHolder
//=================

GraphicalHolder::GraphicalHolder(QObject* parent) : QObject(parent), Holder() {

}

bool GraphicalHolder::hold_message(Handler* target, const Message& message) {
    QApplication::postEvent(this, new StorageEvent(target, message), Qt::HighEventPriority);
    return true;
}

bool GraphicalHolder::event(QEvent* e) {
    if (e->type() == StorageEvent::family) {
        auto storageEvent = static_cast<StorageEvent*>(e);
        storageEvent->target->receive_message(storageEvent->message);
        return true;
    }
    return QObject::event(e);
}

//==========
// Observer
//==========

Handler::result_type Observer::handleMessage(Handler* target, const Message& message) {
    auto result = target->handle_message(message);
    if (result == Handler::result_type::success && message.event.is(~family_ns::note_families))
        QApplication::postEvent(this, new StorageEvent(target, message));
    return result;
}

bool Observer::event(QEvent* e) {
    if (e->type() == StorageEvent::family) {
        auto storageEvent = static_cast<StorageEvent*>(e);
        emit messageHandled(storageEvent->target, storageEvent->message);
        return true;
    }
    return QObject::event(e);
}

//=================
// DefaultReceiver
//=================

DefaultReceiver::DefaultReceiver(QObject* parent) : Observer(parent), Receiver() {

}

Handler::result_type DefaultReceiver::receive_message(Handler* target, const Message& message) {
    return handleMessage(target, message);
}

//================
// CustomObserver
//================

CustomReceiver::CustomReceiver(QObject* parent) : QObject(parent), Receiver(), mObserver(nullptr) {

}

Observer* CustomReceiver::observer() {
    return mObserver;
}

void CustomReceiver::setObserver(Observer* observer) {
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

EditableHandler::EditableHandler(mode_type mode) : HandlerView(), Handler(mode) {

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

void HandlerProxy::setReceiver(DefaultReceiver* receiver) const {
    if (mHandler) {
        auto customReceiver = dynamic_cast<CustomReceiver*>(mHandler->receiver());
        if (customReceiver)
            customReceiver->setObserver(receiver);
        else
            mHandler->set_receiver(receiver);
    }
}

QString HandlerProxy::name() const {
    return handlerName(mHandler);
}

void HandlerProxy::setName(const QString& name) const {
    if (mHandler)
        mHandler->set_name(qstring2name(name));
}

Handler::state_type HandlerProxy::currentState() const {
    return mHandler ? mHandler->state() : Handler::state_type{};
}

Handler::state_type HandlerProxy::supportedState() const {
    Handler::state_type state;
    if (mHandler) {
        if (mHandler->mode().any(handler_ns::forward_mode))
            state |= handler_ns::forward_state;
        if (mHandler->mode().any(handler_ns::receive_mode))
            state |= handler_ns::receive_state;
    }
    return state;
}

void HandlerProxy::setState(bool open, Handler::state_type state) const {
    if (mHandler) {
        if (mHandler->mode().any(handler_ns::thru_mode)) {
            // thru mode should process all states at once
            state = handler_ns::endpoints_state;
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

void HandlerProxy::toggleState(Handler::state_type state) const {
    bool isOpen = currentState().all(state & supportedState());
    setState(!isOpen, state);
}

HandlerProxy::Parameters HandlerProxy::getParameters() const {
    return mView ? mView->getParameters() : Parameters{};
}

void HandlerProxy::setParameter(const Parameter& parameter) const {
    if (mView && !mView->setParameter(parameter))
        TRACE_WARNING("unable to set parameter " << parameter.name);
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
    addParameter(MetaParameter{name, type, description, defaultValue});
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

//======================
// MetaHandlerCollector
//======================

const QList<MetaHandler*>& MetaHandlerCollector::metaHandlers() const {
    return mMetaHandlers;
}

MetaHandler* MetaHandlerCollector::metaHandler(const QString& type) {
    for (MetaHandler* meta : mMetaHandlers)
        if (meta->identifier() == type)
            return meta;
    return nullptr;
}

size_t MetaHandlerCollector::addMetaHandler(MetaHandler* meta) {
    if (meta) {
        meta->setParent(this);
        mMetaHandlers << meta;
        return 1;
    }
    return 0;
}

size_t MetaHandlerCollector::addFactory(MetaHandlerFactory* factory) {
    QList<MetaHandler*> newMetaHandlers;
    if (factory)
        newMetaHandlers = factory->spawn();
    mMetaHandlers << newMetaHandlers;
    return newMetaHandlers.size();
}

size_t MetaHandlerCollector::addPlugin(const QString& filename) {
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

size_t MetaHandlerCollector::addPlugins(const QDir& dir) {
    size_t count = 0;
    for (const QString& filename : dir.entryList(QDir::Files))
        if (QLibrary::isLibrary(filename))
            count += addPlugin(dir.absoluteFilePath(filename));
    if (count == 0)
        TRACE_WARNING("can't find any plugin in " << dir.absolutePath());
    return count;
}

//=========
// Context
//=========

HandlerProxy Context::getProxy(const Handler* handler) const {
    const auto& proxies = getProxies();
    auto it = std::find_if(proxies.begin(), proxies.end(), [=](const auto& proxy) { return proxy.handler() == handler; });
    return it != proxies.end() ? *it : HandlerProxy{};
}
