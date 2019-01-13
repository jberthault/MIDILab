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

#include "qhandlers/transposer.h"

namespace {

constexpr range_t<int> transpositionRange = {-12, 12};

}

//==================
// TransposerEditor
//==================

MetaHandler* makeMetaTransposer(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("Transposer");
    meta->setDescription("A collection of sliders used to change the height of incoming notes");
    meta->addParameter({"orientation", "orientation of the slider", "Horizontal", MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"expanded", "display one knob per channel", "false", MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"selection", "bitmask of selected channels", serial::serializeChannels(channels_t::melodic()), MetaHandler::MetaParameter::Visibility::advanced});
    meta->addParameter({"visibility", "bitmask of visible channels", serial::serializeChannels(channels_t::full()), MetaHandler::MetaParameter::Visibility::advanced});
    meta->setFactory(new OpenProxyFactory<TransposerEditor>);
    return meta;
}

TransposerEditor::TransposerEditor() : HandlerEditor{} {

    mKeys.fill(0);

    mSlider = new ChannelsSlider{Qt::Horizontal, this};
    mSlider->setTextWidth(25);
    mSlider->setExpanded(false);
    mSlider->setSelection(channels_t::melodic());
    mSlider->setDefaultRatio(.5);
    mSlider->setCardinality(span(transpositionRange) + 1);
    connect(mSlider, &ChannelsSlider::knobChanged, this, &TransposerEditor::updateText);
    connect(mSlider, &ChannelsSlider::knobMoved, this, &TransposerEditor::onMove);
    mSlider->setDefault(channels_t::full()); // will call updateText

    setLayout(make_hbox(margin_tag{0}, mSlider));
}

void TransposerEditor::updateContext(Context* context) {
    mSlider->setChannelEditor(context->channelEditor());
    connect(static_cast<Observer*>(mHandler.interceptor()), &Observer::messageHandled, this, &TransposerEditor::onMessageHandled);
}

void TransposerEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (handler == &mHandler && message.event.is(family_t::extended_system)) {
        if (Handler::open_ext.affects(message.event)) {
            const auto state = Handler::open_ext.decode(message.event);
            if (state.any(Handler::State::forward()))
                mSlider->setMovable(true);
        } else if (Handler::close_ext.affects(message.event)) {
            const auto state = Handler::close_ext.decode(message.event);
            if (state.any(Handler::State::forward()))
                mSlider->setMovable(false);
        }
    }
}

HandlerView::Parameters TransposerEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("orientation", serial::serializeOrientation, mSlider->orientation(), result);
    SERIALIZE("expanded", serial::serializeBool, mSlider->isExpanded(), result);
    SERIALIZE("selection", serial::serializeChannels, mSlider->selection(), result);
    SERIALIZE("visibility", serial::serializeChannels, mSlider->visibleChannels(), result);
    return result;
}

size_t TransposerEditor::setParameter(const Parameter& parameter) {
    UNSERIALIZE("orientation", serial::parseOrientation, mSlider->setOrientation, parameter);
    UNSERIALIZE("expanded", serial::parseBool, mSlider->setExpanded, parameter);
    UNSERIALIZE("selection", serial::parseChannels, mSlider->setSelection, parameter);
    UNSERIALIZE("visibility", serial::parseChannels, mSlider->setVisibleChannels, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* TransposerEditor::getHandler() {
    return &mHandler;
}

void TransposerEditor::onMove(channels_t channels, qreal ratio) {
    const auto key = expand(ratio, transpositionRange);
    channel_ns::store(mKeys, channels, key);
    mSlider->setText(channels, number2string(key));
    if (channels)
        mHandler.send_message(Transposer::transpose_ext(channels, key));
}

void TransposerEditor::updateText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mKeys, channels))
        mSlider->setText(pair.second, number2string(pair.first));
}
