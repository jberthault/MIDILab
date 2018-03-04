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

#ifndef QHANDLERS_COMMON_H
#define QHANDLERS_COMMON_H

#include "qcore/editors.h"

//======================
// MetaGraphicalHandler
//======================

class MetaGraphicalHandler : public OpenMetaHandler {

    Q_OBJECT

public:
    explicit MetaGraphicalHandler(QObject* parent);

};

//==================
// GraphicalHandler
//==================

class GraphicalHandler : public EditableHandler {

    Q_OBJECT

public:
    explicit GraphicalHandler(Mode mode);

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
    explicit Instrument(Mode mode);

    families_t handled_families() const override;
    Result handle_message(const Message& message) override;
    Result on_close(State state) override;

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

#endif // QHANDLERS_COMMON_H
