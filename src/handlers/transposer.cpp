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

#include "transposer.h"
#include "qtools/misc.h"

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

namespace {

static constexpr range_t<int> transpositionRange = {-12, 12};
static constexpr size_t transpositionCardinality = 25;

QString keyToString(int key) {
    auto text = QString::number(key);
    if (key > 0)
        text.prepend('+');
    return text;
}

}

//================
// MetaTransposer
//================

MetaTransposer::MetaTransposer(QObject* parent) : MetaHandler(parent) {
    setIdentifier("Transposer");
}

MetaHandler::Instance MetaTransposer::instantiate() {
    auto handler = new Transposer;
    return Instance(handler, new TransposerEditor(handler));
}

//============
// Transposer
//============

Event Transposer::transpose_event(channels_t channels, int key) {
    return Event::custom(channels, "Transpose", marshall(key));
}

Transposer::Transposer() : Handler(thru_mode), m_bypass(true) {
    m_keys.fill(0);
}

Handler::result_type Transposer::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;

    if (message.event.family() == family_t::custom) {
        if (message.event.get_custom_key() == "Transpose") {
            set_key(message.event.channels(), unmarshall<int>(message.event.get_custom_value()));
            return result_type::success;
        }
    } else if (message.event.is(note_families)) {
        clean_corrupted(message.source, message.track);
        if (!m_bypass) {
            for (const auto& pair : channel_ns::reverse(m_keys, message.event.channels())) {
                Message copy(message);
                Event::iterator note_it = ++copy.event.begin(); // second byte contains the note
                *note_it = to_data_byte(*note_it + pair.first); // shift note
                copy.event.set_channels(pair.second);
                feed_forward(copy);
            }
            return result_type::success;
        }
    }

    feed_forward(message); // to feed controller events
    return result_type::success;
}

void Transposer::feed_forward(const Message& message) {
    m_corruption.feed(message.event);
    forward_message(message);
}

void Transposer::clean_corrupted(Handler* source, track_t track) {
    channels_t channels = m_corruption.reset();
    if (channels)
        feed_forward({Event::controller(channels, all_notes_off_controller), source, track});
}

void Transposer::set_key(channels_t channels, int key) {
    channels_t channels_changed;
    // registered key for each channel
    for (channel_t channel : channels) {
        if (m_keys[channel] != key) {
            m_keys[channel] = key;
            channels_changed.set(channel);
        }
    }
    // bypass handler if no keys is shifted
    m_bypass = channel_ns::find(m_keys, 0) == all_channels;
    // mark channels as corrupted
    m_corruption.tick(channels_changed);

    /// @todo call clean_corrupted here instead of waiting for a note event
    /// forward with the transposer as se source
    /// but it means sinks may accept transposer as a source
    /// or we could generate using the last source
}

//==================
// TransposerEditor
//==================

TransposerEditor::TransposerEditor(Transposer* handler) : HandlerEditor(handler) {

    mKeys.fill(0);

    mSlider = new ChannelsSlider(Qt::Horizontal, this);
    mSlider->setTextWidth(25);
    mSlider->setExpanded(false);
    mSlider->setSelection(all_channels & ~drum_channels);
    mSlider->setDefaultRatio(.5);
    mSlider->setCardinality(transpositionCardinality);
    connect(mSlider, &ChannelsSlider::knobChanged, this, &TransposerEditor::updateText);
    connect(mSlider, &ChannelsSlider::knobMoved, this, &TransposerEditor::onMove);
    mSlider->setDefault(all_channels); // will call updateText

    setLayout(make_hbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

void TransposerEditor::updateContext(Context* context) {
    mSlider->setChannelEditor(context->channelEditor());
}

HandlerView::Parameters TransposerEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("orientation", serial::serializeOrientation, mSlider->orientation(), result);
    return result;
}

size_t TransposerEditor::setParameter(const Parameter& parameter) {
    UNSERIALIZE("orientation", serial::parseOrientation, mSlider->setOrientation, parameter);
    return HandlerEditor::setParameter(parameter);
}

void TransposerEditor::onMove(channels_t channels, qreal ratio) {
    auto key = transpositionRange.expand(ratio);
    channel_ns::store(mKeys, channels, key);
    mSlider->setText(channels, keyToString(key));
    if (channels)
        static_cast<Transposer*>(handler())->send_message(Transposer::transpose_event(channels, key));
}

void TransposerEditor::updateText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mKeys, channels))
        mSlider->setText(pair.second, keyToString(pair.first));
}
