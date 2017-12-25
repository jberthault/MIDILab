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

#ifndef QCORE_CORE_H
#define QCORE_CORE_H

#include <QEvent>
#include <QWidget>
#include <QTime>
#include <QDialog>
#include <QString>
#include <QApplication>
#include <QTextStream>
#include <QDebug>
#include <QComboBox>
#include <QFormLayout>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QTreeWidget>
#include <QMap>
#include <QList>
#include <QDir>
#include "core/event.h"
#include "core/handler.h"
#include "core/misc.h"
#include "qtools/misc.h"

/*!< enable QVariant to accept those types */
Q_DECLARE_METATYPE(Message)
Q_DECLARE_METATYPE(Handler*)

//=================
// Name Conversion
//=================

class MetaHandler;

std::string qstring2name(const QString& string);
QString name2qstring(const std::string& name);

QString handlerName(const Handler* handler);
QString eventName(const Event& event);
QString metaHandlerName(const MetaHandler* meta);

//=============
// Persistence
//=============

namespace serial {

// numeric types

QString serializeBool(bool value);
bool parseBool(const QString& data, bool& value);

QString serializeByte(byte_t byte);
bool parseByte(const QString& data, byte_t& byte);

template<typename T>
QString serializeNumber(T value) {
    return QString::number(value);
}

bool parseShort(const QString& data, short& value);
bool parseUShort(const QString& data, ushort& value);
bool parseInt(const QString& data, int& value);
bool parseUInt(const QString& data, uint& value);
bool parseLong(const QString& data, long& value);
bool parseULong(const QString& data, ulong& value);
bool parseLongLong(const QString& data, qlonglong& value);
bool parseULongLong(const QString& data, qulonglong& value);
bool parseFloat(const QString& data, float& value);
bool parseDouble(const QString& data, double& value);

// note types

QString serializeNote(const Note& note);
bool parseNote(const QString& data, Note& note);

QString serializeRange(const QPair<Note, Note>& range);
bool parseRange(const QString& data, QPair<Note, Note>& range);

QString serializeNotes(const std::vector<Note>& notes);
bool parseNotes(const QString& data, std::vector<Note>& notes);

// other types

QString serializeOrientation(Qt::Orientation orientation);
bool parseOrientation(const QString& data, Qt::Orientation& orientation);

template<typename T>
struct parser_traits;

template<typename T>
struct parser_traits<bool(const QString&, T&)> {
    using type = T;
};

#define UNSERIALIZE(key, parser, setter, param) do {           \
    if (param.name == key) {                                   \
        serial::parser_traits<decltype(parser)>::type __value; \
        if (!parser(param.value, __value))                     \
            return 0;                                          \
        setter(__value);                                       \
        return 1;                                              \
    }                                                          \
} while (false)

#define SERIALIZE(key, serializer, value, params) do {   \
    params.push_back(Parameter{key, serializer(value)}); \
} while (false)

}

//==============
// StorageEvent
//==============

class StorageEvent : public QEvent {

public:
    static const QEvent::Type family;

    StorageEvent(Handler* target, Message message);

    Handler* target;
    Message message;

};

//=================
// GraphicalHolder
//=================

/// This holder is designed to use Qt Event loop to distribute messages in a thread-safe way
/// This object and client Handlers (GraphicalHandlers) are supposed to live in the same thread

class GraphicalHolder : public QObject, public Holder {

    Q_OBJECT

public:
    explicit GraphicalHolder(QObject* parent);

    bool hold_message(Handler* target, const Message& message) final;

protected:
    bool event(QEvent* e) override;

};

//==========
// Observer
//==========

class Observer : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    Handler::result_type handleMessage(Handler* target, const Message& message);

protected:
    bool event(QEvent* e) override;

signals:
    void messageHandled(Handler* handler, const Message& message);

};

//=================
// DefaultReceiver
//=================

class DefaultReceiver : public Observer, public Receiver {

    Q_OBJECT

public:
    explicit DefaultReceiver(QObject* parent);

    result_type receive_message(Handler* target, const Message& message) final;

};

//================
// CustomReceiver
//================

class CustomReceiver : public QObject, public Receiver {

    Q_OBJECT

public:
    explicit CustomReceiver(QObject* parent);

    Observer* observer();
    void setObserver(Observer* observer);

private:
    Observer* mObserver;

};

//=============
// MetaHandler
//=============

class HandlerEditor;

class MetaHandler : public QObject {

    Q_OBJECT

public:

    struct MetaParameter {
        QString name; /*!< identifier of this parameter */
        QString type; /*!< the serializable type identifier encapsulating this parameter */
        QString description; /*!< description of whatever is represented by this parameter */
        QString defaultValue; /*!< value considered if not specified, empty means N/A */
    };

    using MetaParameters = std::vector<MetaParameter>;

    struct Parameter {
        QString name; /*!< identifier of the supported MetaParameter */
        QString value; /*!< encoded value of the parameter (depending on its MetaParameter's type) */
    };

    using Parameters = std::vector<Parameter>;

    using Instance = std::pair<Handler*, HandlerEditor*>;

    using QObject::QObject;

    const QString& identifier() const;
    const QString& description() const;
    const MetaParameters& parameters() const;

    virtual Instance instantiate() = 0;

protected:
    void setIdentifier(const QString& identifier);
    void setDescription(const QString& description);
    void addParameters(const MetaParameters& parameters);
    void addParameter(const MetaParameter& parameter);
    void addParameter(const QString& name, const QString& type, const QString& description, const QString& defaultValue);

private:
    QString mIdentifier;
    QString mDescription;
    MetaParameters mParameters;

};

//====================
// MetaHandlerFactory
//====================

#define MetaHandlerFactory_iid "midi.MetaHandlerFactory"

class MetaHandlerFactory {

public:
    virtual ~MetaHandlerFactory() = default;

    virtual const QList<MetaHandler*>& spawn() const = 0;

};

Q_DECLARE_INTERFACE(MetaHandlerFactory, MetaHandlerFactory_iid)

//======================
// MetaHandlerCollector
//======================

class MetaHandlerCollector : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    const QList<MetaHandler*>& metaHandlers() const;

    MetaHandler* metaHandler(const QString& type); /*!< return the meta handler that has the given type identifier, nullptr if not exists */

    size_t addMetaHandler(MetaHandler* meta);
    size_t addFactory(MetaHandlerFactory* factory);
    size_t addPlugin(const QString& filename);
    size_t addPlugins(const QDir& dir);

private:
    QList<MetaHandler*> mMetaHandlers;

};

//=========
// Context
//=========

class ChannelEditor;

class Context : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    virtual ChannelEditor* channelEditor() = 0;
    virtual QList<Handler*> getHandlers() = 0;
    virtual PathRetriever* pathRetriever(const QString& type) = 0;

};

//=============
// HandlerView
//=============

class HandlerView : public QWidget {

    Q_OBJECT

public:

    using Parameter = MetaHandler::Parameter;
    using Parameters = MetaHandler::Parameters;

    explicit HandlerView();

    ChannelEditor* channelEditor(); /*!< shortcut for context()->channelEditor() */

    Context* context();
    void setContext(Context* context);

    virtual Parameters getParameters() const;
    virtual size_t setParameter(const Parameter& Parameter);
    size_t setParameters(const Parameters& parameters);

protected:
    virtual void updateContext(Context* context);

private:
    Context* mContext;

};

//===============
// HandlerEditor
//===============

class HandlerEditor : public HandlerView {

    Q_OBJECT

public:
    explicit HandlerEditor() = default;

};

//======================
// MetaGraphicalHandler
//======================

class MetaGraphicalHandler : public MetaHandler {

    Q_OBJECT

public:
    explicit MetaGraphicalHandler(QObject* parent);

};

//==================
// GraphicalHandler
//==================

class GraphicalHandler : public HandlerView, public Handler {

    Q_OBJECT

public:
    explicit GraphicalHandler(mode_type mode);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    track_t track() const;
    void setTrack(track_t track);

    bool canGenerate() const;
    void generate(Event event);

private:
    track_t mTrack;

};

//================
// MetaInstrument
//================

class MetaInstrument : public MetaGraphicalHandler {

    Q_OBJECT

public:
    explicit MetaInstrument(QObject* parent);

};

//============
// Instrument
//============

class Instrument : public GraphicalHandler {

    Q_OBJECT

public:
    explicit Instrument(mode_type mode);

    families_t handled_families() const override;
    result_type handle_message(const Message& message) override;
    result_type on_close(state_type state) override;

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    byte_t velocity() const;
    void setVelocity(byte_t velocity);

protected:
    virtual void receiveClose();
    virtual void receiveReset();
    virtual void receiveNotesOff(channels_t channels);
    virtual void receiveNoteOn(channels_t channels, const Note& note);
    virtual void receiveNoteOff(channels_t channels, const Note& note);

    void generateNoteOn(channels_t channels, const Note& note);
    void generateNoteOff(channels_t channels, const Note& note);

private:
    byte_t mVelocity;

};

#endif // QCORE_CORE_H
