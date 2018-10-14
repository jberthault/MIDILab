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

#ifndef QHANDLERS_COMMON_H
#define QHANDLERS_COMMON_H

#include "qcore/editors.h"

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

QString serializeRange(const std::pair<Note, Note>& range);
bool parseRange(const QString& data, std::pair<Note, Note>& range);

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

//==================
// GraphicalHandler
//==================

MetaHandler* makeMetaGraphicalHandler(QObject* parent);

class GraphicalHandler : public EditableHandler {

    Q_OBJECT

public:
    using EditableHandler::EditableHandler;

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    track_t track() const;
    void setTrack(track_t track);

    bool canGenerate() const;
    void generate(Event event);

private:
    track_t mTrack {Message::no_track};

};

//============
// Instrument
//============

MetaHandler* makeMetaInstrument(QObject* parent);

class Instrument : public GraphicalHandler {

    Q_OBJECT

public:
    using GraphicalHandler::GraphicalHandler;

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    byte_t velocity() const;
    void setVelocity(byte_t velocity);

protected:
    Result handle_close(State state) final;
    Result handle_message(const Message& message) final;
    families_t handled_families() const final;

    virtual void receiveClose();
    virtual void receiveReset();
    virtual void receiveNotesOff(channels_t channels);
    virtual void receiveNoteOn(channels_t channels, const Note& note);
    virtual void receiveNoteOff(channels_t channels, const Note& note);

    void generateNoteOn(channels_t channels, const Note& note);
    void generateNoteOff(channels_t channels, const Note& note);

    channels_t channelsFromButtons(Qt::MouseButtons buttons);

private:
    byte_t mVelocity {0x7f};

};

#endif // QHANDLERS_COMMON_H
