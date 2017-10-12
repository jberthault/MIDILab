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

#include <QGridLayout>
#include <QPushButton>
#include "channelmapper.h"
#include "qtools/misc.h"

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

//===============
// ChannelMapper
//===============

Event ChannelMapper::remap_event(channels_t channels, channels_t mapped_channels) {
    return Event::custom(channels, "ChannelMapping.remap", marshall(mapped_channels));
}

Event ChannelMapper::unmap_event(channels_t channels) {
    return Event::custom(channels, "ChannelMapping.unmap");
}

ChannelMapper::ChannelMapper() : Handler(thru_mode) {
    for (channel_t c=0 ; c < 0x10 ; ++c)
        m_mapping[c] = channels_t::from_bit(c);
}

channel_map_t<channels_t> ChannelMapper::mapping() const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_mapping;
}

void ChannelMapper::set_mapping(channel_map_t<channels_t> mapping) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_mapping = std::move(mapping);
    m_corruption.tick();
}

void ChannelMapper::reset_mapping(channels_t channels) {
    std::lock_guard<std::mutex> guard(m_mutex);
    for (channel_t channel : channels)
        m_mapping[channel] = channels_t::from_bit(channel);
    m_corruption.tick();
}

Handler::result_type ChannelMapper::handle_message(const Message& message) {

    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_FORWARD_RECEIVE;

    std::lock_guard<std::mutex> guard(m_mutex);

    if (message.event.is(custom_family)) {
        auto k = message.event.get_custom_key();
        if (k == "ChannelMapping.remap") {
            auto mapped_channels = unmarshall<channels_t>(message.event.get_custom_value());
            channel_ns::store(m_mapping, message.event.channels(), mapped_channels);
            m_corruption.tick();
            return success_result;
        } else if (k == "ChannelMapping.unmap") {
            for (channel_t channel : message.event.channels())
                m_mapping[channel] = channels_t::from_bit(channel);
            m_corruption.tick();
            return success_result;
        }
    }

    /// clean if another note comes in
    if (message.event.is(note_families))
        clean_corrupted(message.source, message.track);

    Message copy(message);

     /// @todo get all relevant families and don't forward if no channels
    if (message.event.is(voice_families))
        copy.event.set_channels(remap(message.event.channels()));

    feed_forward(copy);
    return success_result;
}

void ChannelMapper::feed_forward(const Message& message) {
    m_corruption.feed(message.event);
    forward_message(message);
}

void ChannelMapper::clean_corrupted(Handler* source, track_t track) {
    channels_t channels = m_corruption.reset();
    if (channels)
        feed_forward({Event::controller(channels, all_notes_off_controller), source, track});
}

channels_t ChannelMapper::remap(channels_t channels) const {
    channels_t result;
    for (channel_t old_channel : channels)
        result |= m_mapping[old_channel];
    return result;
}

//===================
// MetaChannelMapper
//===================

MetaChannelMapper::MetaChannelMapper(QObject* parent) : MetaHandler(parent) {
    setIdentifier("ChannelMapper");
}

MetaChannelMapper::instance_type MetaChannelMapper::instantiate(const QString& name, QWidget* parent) {
    ChannelMapper* handler = new ChannelMapper;
    handler->set_name(qstring2name(name));
    return instance_type(handler, new ChannelMapperEditor(handler, parent));
}

//=====================
// ChannelMapperEditor
//=====================

ChannelMapperEditor::ChannelMapperEditor(ChannelMapper* handler, QWidget* parent) :
    HandlerEditor(handler, parent) {

    auto checkBoxLayout = new QGridLayout;
    checkBoxLayout->setMargin(0);
    checkBoxLayout->setSpacing(0);

    channel_map_t<TriState*> in, out;

    static const int offset = 3;

    for (channel_t c=0 ; c < 0x10 ; ++c) {
        TriState* inCheck = new TriState(this);
        TriState* outCheck = new TriState(this);
        QLabel* inLabel = new QLabel(QString::number(c), this);
        QLabel* outLabel = new QLabel(QString::number(c), this);
        checkBoxLayout->addWidget(inLabel, offset + c, 0);
        checkBoxLayout->addWidget(inCheck, offset + c, 1);
        checkBoxLayout->addWidget(outLabel, 0, offset + c);
        checkBoxLayout->addWidget(outCheck, 1, offset + c);
        in[c] = inCheck;
        out[c] = outCheck;
    }

    auto hline = new QFrame(this);
    auto vline = new QFrame(this);
    hline->setFrameShadow(QFrame::Sunken);
    vline->setFrameShadow(QFrame::Sunken);
    hline->setFrameShape(QFrame::HLine);
    vline->setFrameShape(QFrame::VLine);
    checkBoxLayout->addWidget(hline, 2, offset, 1, -1);
    checkBoxLayout->addWidget(vline, offset, 2, -1, 1);

    for (channel_t ic=0 ; ic < 0x10 ; ++ic) {
        for (channel_t oc=0 ; oc < 0x10 ; ++oc) {
            auto check = new QCheckBox(this);
            mCheckBoxes[ic][oc] = check;
            in[ic]->addCheckBox(check);
            out[oc]->addCheckBox(check);
            checkBoxLayout->addWidget(check, offset + ic, offset + oc);
        }
    }

    auto dgroup = new TriState(this);
    checkBoxLayout->addWidget(dgroup, 1, 1);
    for (channel_t c=0 ; c < 0x10 ; ++c)
        dgroup->addCheckBox(mCheckBoxes[c][c]);

    auto applyButton = new QPushButton("Apply", this);
    connect(applyButton, SIGNAL(clicked()), this, SLOT(updateMapper()));

    auto resetButton = new QPushButton("Reset", this);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetMapper()));

    auto discardButton = new QPushButton("Discard", this);
    connect(discardButton, SIGNAL(clicked()), this, SLOT(updateFromMapper()));

    setLayout(make_vbox(checkBoxLayout, stretch_tag{}, make_hbox(stretch_tag{}, applyButton, resetButton, discardButton)));

    updateFromMapper();
}

void ChannelMapperEditor::updateMapper() {
    channel_map_t<channels_t> mapping;
    for (channel_t ic=0 ; ic < 0x10 ; ++ic) {
        channels_t ocs;
        for (channel_t oc=0 ; oc < 0x10 ; ++oc)
            if (mCheckBoxes[ic][oc]->isChecked())
                ocs.set(oc);
        mapping[ic] = ocs;
    }
    static_cast<ChannelMapper*>(handler())->set_mapping(std::move(mapping));
}

void ChannelMapperEditor::updateFromMapper() {
    auto mapping = static_cast<ChannelMapper*>(handler())->mapping();
    for (channel_t ic=0 ; ic < 0x10 ; ++ic)
        for (channel_t oc=0 ; oc < 0x10 ; ++oc)
            mCheckBoxes[ic][oc]->setChecked(mapping[ic].contains(oc));
}

void ChannelMapperEditor::resetMapper() {
    static_cast<ChannelMapper*>(handler())->reset_mapping();
    updateFromMapper();
}
