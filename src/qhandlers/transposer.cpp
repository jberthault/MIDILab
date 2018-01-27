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

#include "qhandlers/transposer.h"

namespace {

static constexpr range_t<int> transpositionRange = {-12, 12};
static constexpr size_t transpositionCardinality = 25;

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

//==================
// TransposerEditor
//==================

TransposerEditor::TransposerEditor(Transposer* handler) : HandlerEditor(), mHandler(handler) {

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
    mSlider->setText(channels, number2string(key));
    if (channels)
        mHandler->send_message(Transposer::transpose_event(channels, key));
}

void TransposerEditor::updateText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mKeys, channels))
        mSlider->setText(pair.second, number2string(pair.first));
}
