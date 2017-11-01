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

#ifndef HANDLERS_TRANSPOSER_H
#define HANDLERS_TRANSPOSER_H

#include "qcore/editors.h"
#include "qtools/misc.h"

//================
// MetaTransposer
//================

class MetaTransposer : public MetaHandler {

public:
    MetaTransposer(QObject* parent);

    Instance instantiate() override;

};

//============
// Transposer
//============

/**
 *
 * Concerning note bound to more than one channel, if transposition key
 * differs, note will be duplicated for each different key
 *
 * To Change a key : send a handler_event with key "Transpose" and an int value
 * representing the offset to apply to each note. the key will be set for each channel
 * specified.
 *
 */

class Transposer : public Handler {

public:
    static Event transpose_event(channels_t channels, int key);

    Transposer();

    result_type handle_message(const Message& message) override;

private:
    void feed_forward(const Message& message); /*!< forward a message after feeding memory */
    void clean_corrupted(Handler* source, track_t track); /*!< forward a message cleaning corrupted channels */
    void set_key(channels_t channels, int key); /*!< set the transposition key for the given channels */

    channel_map_t<int> m_keys; /*!< number of semi-tones shifted by channel */
    Corruption m_corruption;
    bool m_bypass;

};

//==================
// TransposerEditor
//==================

class TransposerEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit TransposerEditor(Transposer* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

protected:
    void updateContext(Context* context) override;

private slots:
    void onMove(channels_t channels, qreal ratio);
    void updateText(channels_t channels);

private:
    ChannelsSlider* mSlider;
    channel_map_t<int> mKeys;

};

#endif // HANDLERS_TRANSPOSER_H
