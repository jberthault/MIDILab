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

#include "qhandlers/wheel.h"
#include "handlers/systemhandler.h"

namespace {

constexpr range_t<uint16_t> volumeRange = {0, 0xffff};
constexpr range_t<byte_t> semitonesRange = {0, 24};
constexpr range_t<byte_t> data7Range = {0, 0x7f};
constexpr range_t<uint16_t> data14Range = {0, 0x3fff};
constexpr range_t<int> panRange = {-50, 50};

constexpr byte_t defaultSemitones = 2;
constexpr uint16_t defaultRPN = 0x3fff;
constexpr uint16_t defaultPitch = 0x2000;
constexpr uint16_t pitchBendRangeRPN = 0x0000;

QString stringForRatio(qreal ratio) {
    return QString::number(decay_value<int>(100*ratio)) + "%";
}

}

//===========
// MetaWheel
//===========

MetaWheel::MetaWheel(QObject* parent) : MetaGraphicalHandler(parent) {
    addParameter("orientation", ":orientation", "orientation of the slider", "Vertical");
}

//===============
// AbstractWheel
//===============

AbstractWheel::AbstractWheel(Mode mode) : GraphicalHandler(mode) {

    mSlider = new ChannelsSlider(Qt::Vertical, this);
    mSlider->setTextWidth(40);
    connect(mSlider, &ChannelsSlider::knobChanged, this, &AbstractWheel::updateText);
    connect(mSlider, &ChannelsSlider::knobMoved, this, &AbstractWheel::onValueMove);

    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

ChannelsSlider* AbstractWheel::slider() {
    return mSlider;
}

HandlerView::Parameters AbstractWheel::getParameters() const {
    auto result = GraphicalHandler::getParameters();
    SERIALIZE("orientation", serial::serializeOrientation, mSlider->orientation(), result);
    return result;
}

size_t AbstractWheel::setParameter(const Parameter& parameter) {
    UNSERIALIZE("orientation", serial::parseOrientation, mSlider->setOrientation, parameter);
    return GraphicalHandler::setParameter(parameter);
}

Handler::Result AbstractWheel::handle_close(State state) {
    mSlider->setDefault(channels_t::full());
    return GraphicalHandler::handle_close(state);
}

void AbstractWheel::prepare(qreal defaultRatio) {
    mSlider->setDefaultRatio(defaultRatio);
    mSlider->setDefault(channels_t::full());
}

void AbstractWheel::updateContext(Context* context) {
    mSlider->setChannelEditor(context->channelEditor());
}

void AbstractWheel::onValueMove(channels_t channels, qreal ratio) {
    onMove(channels, ratio);
    updateText(channels);
}

//=====================
// MetaControllerWheel
//=====================

MetaControllerWheel::MetaControllerWheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("ControllerWheel");
    addParameter("controller", ":controller", "controller id(s) reacting over the GUI", "0x00");
}

void MetaControllerWheel::setContent(HandlerProxy& proxy) {
    proxy.setContent(new ControllerWheel);
}

//=================
// ControllerWheel
//=================

ControllerWheel::ControllerWheel() : AbstractWheel(Mode::io()) {
    resetAll();
    mControllerBox = new QComboBox(this);
    for (const auto& value : controller_ns::controller_names())
        if (!controller_ns::is_channel_mode_message(value.first))
            mControllerBox->addItem(QString::fromStdString(value.second), QVariant(value.first));
    connect(mControllerBox, SIGNAL(currentIndexChanged(int)), SLOT(onControlChange()));
    onControlChange(); // update controller
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, mControllerBox);
}

byte_t ControllerWheel::controller() const {
    return mController;
}

void ControllerWheel::setController(byte_t controller) {
    int index = mControllerBox->findData(controller);
    if (index == -1)
        TRACE_WARNING("unknown controller");
    else
        mControllerBox->setCurrentIndex(index); // will update mController
}

HandlerView::Parameters ControllerWheel::getParameters() const {
    auto result = AbstractWheel::getParameters();
    SERIALIZE("controller", serial::serializeByte, mController, result);
    return result;
}

size_t ControllerWheel::setParameter(const Parameter& parameter) {
    UNSERIALIZE("controller", serial::parseByte, setController, parameter);
    return AbstractWheel::setParameter(parameter);
}

families_t ControllerWheel::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::controller, family_t::reset); // channel_pressure;
}

Handler::Result ControllerWheel::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::controller: return handleController(message.event.channels(), message.event.at(1), message.event.at(2));
    case family_t::reset: return handleReset();
    default: return Result::unhandled;
    }
}

Handler::Result ControllerWheel::handle_close(State state) {
    resetAll();
    return AbstractWheel::handle_close(state);
}

void ControllerWheel::onControlChange() {
    mController = mControllerBox->currentData().value<byte_t>();
    channel_map_t<qreal> ratios;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++)
        ratios[c] = data7Range.reduce(mValues[mController][c]);
    slider()->setDefaultRatio(data7Range.reduce(controller_ns::default_value(mController)));
    slider()->setRatios(ratios);
}

void ControllerWheel::onMove(channels_t channels, qreal ratio) {
    auto byte = data7Range.expand(ratio);
    channel_ns::store(mValues[mController], channels, byte);
    if (canGenerate() && channels)
        generate(Event::controller(channels, mController, byte));
}

void ControllerWheel::updateText(channels_t channels) {
    if (controller_ns::default_value(mController) == 0x40) { // Centered
        for (const auto& pair : channel_ns::reverse(mValues[mController], channels))
            slider()->setText(pair.second, number2string(panRange.rescale(data7Range, pair.first)));
    } else {
        for (const auto& pair : channel_ns::reverse(mValues[mController], channels))
            slider()->setText(pair.second, QString::number(pair.first));
    }
}

void ControllerWheel::resetAll() {
    for (byte_t cc=0 ; cc < 0x80 ; cc++)
        mValues[cc].fill(controller_ns::default_value(cc));
}

void ControllerWheel::setControllerValue(channels_t channels, byte_t controller, byte_t value) {
    channel_ns::store(mValues[controller], channels, value);
    if (controller == mController)
        slider()->setRatio(channels, data7Range.reduce(value));
}

Handler::Result ControllerWheel::handleController(channels_t channels, byte_t controller, byte_t value) {
    if (controller == controller_ns::all_controllers_off_controller)
        for (byte_t cc : controller_ns::off_controllers)
            setControllerValue(channels, cc, controller_ns::default_value(cc));
    else
        setControllerValue(channels, controller, value);
    return Result::success;
}

Handler::Result ControllerWheel::handleReset() {
    for (byte_t cc : controller_ns::reset_controllers)
        handleController(channels_t::full(), cc, controller_ns::default_value(cc));
    return Result::success;
}

//================
// MetaPitchWheel
//================

MetaPitchWheel::MetaPitchWheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("PitchWheel");
}

void MetaPitchWheel::setContent(HandlerProxy& proxy) {
    proxy.setContent(new PitchWheel);
}

//============
// PitchWheel
//============

PitchWheel::PitchWheel() : AbstractWheel(Mode::io()) {
    mTypeBox = new QComboBox(this);
    mTypeBox->addItem("Pitch Bend");
    mTypeBox->addItem("Pitch Bend Range");
    connect(mTypeBox, SIGNAL(currentIndexChanged(int)), SLOT(onTypeChange(int)));
    connect(slider(), &ChannelsSlider::knobPressed, this, &PitchWheel::onPress);
    connect(slider(), &ChannelsSlider::knobReleased, this, &PitchWheel::onRelease);
    resetAll();
    prepare(.5);
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, mTypeBox);
}

families_t PitchWheel::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::controller, family_t::pitch_wheel, family_t::reset);
}

Handler::Result PitchWheel::handle_message(const Message& message) {
    /// @note data_entry_fine_controller is ignored
    switch (message.event.family()) {
    case family_t::controller:
        switch (message.event.at(1)) {
        case controller_ns::registered_parameter_controller.coarse: return handleCoarseRPN(message.event.channels(), message.event.at(2));
        case controller_ns::registered_parameter_controller.fine: return handleFineRPN(message.event.channels(), message.event.at(2));
        case controller_ns::non_registered_parameter_controller.coarse: return handleCoarseRPN(message.event.channels(), 0x7f);
        case controller_ns::non_registered_parameter_controller.fine: return handleFineRPN(message.event.channels(), 0x7f);
        case controller_ns::data_entry_controller.coarse: return handleCoarseDataEntry(message.event.channels(), message.event.at(2));
        case controller_ns::all_controllers_off_controller: return handleAllControllersOff(message.event.channels());
        }
        break;
    case family_t::pitch_wheel: return handlePitchValue(message.event.channels(), message.event.get_14bits());
    case family_t::reset: return handleReset();
    }
    return Result::unhandled;
}

Handler::Result PitchWheel::handle_close(State state) {
    resetAll();
    return AbstractWheel::handle_close(state);
}

void PitchWheel::onPress(channels_t channels) {
    if (canGenerate() && rangeDisplayed())
        generateRegisteredParameter(channels, pitchBendRangeRPN);
}

void PitchWheel::onRelease(channels_t channels) {
    if (canGenerate() && rangeDisplayed())
        generateRegisteredParameter(channels, defaultRPN);
}

void PitchWheel::onMove(channels_t channels, qreal ratio) {
    if (rangeDisplayed()) {
        auto semitones = semitonesRange.expand(ratio);
        channel_ns::store(mPitchRanges, channels, semitones);
        if (canGenerate() && channels) {
            channels_t channelsNotReady = channels & ~channel_ns::find(mRegisteredParameters, pitchBendRangeRPN);
            generateRegisteredParameter(channelsNotReady, pitchBendRangeRPN);
            generate(Event::controller(channels, controller_ns::data_entry_controller.coarse, semitones));
            generateRegisteredParameter(channelsNotReady, defaultRPN);
        }
    } else {
        auto value = data14Range.expand(ratio);
        channel_ns::store(mPitchValues, channels, value);
        if (canGenerate() && channels)
            generate(Event::pitch_wheel(channels, value));
    }
}

void PitchWheel::updateText(channels_t channels) {
    if (rangeDisplayed())
        updatePitchRangeText(channels);
    else
        updatePitchValueText(channels);
}

void PitchWheel::onTypeChange(int index) {
    qreal defaultRatio;
    channel_map_t<qreal> ratios;
    if (index == 1) {
        defaultRatio = semitonesRange.reduce(defaultSemitones);
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            ratios[c] = semitonesRange.reduce(mPitchRanges[c]);
    } else {
        defaultRatio = .5;
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            ratios[c] = data14Range.reduce(mPitchValues[c]);
    }
    slider()->setDefaultRatio(defaultRatio);
    slider()->setRatios(ratios);
}

bool PitchWheel::rangeDisplayed() const {
    return mTypeBox->currentIndex() == 1;
}

void PitchWheel::generateRegisteredParameter(channels_t channels, uint16_t value) {
    channel_ns::store(mRegisteredParameters, channels, value);
    if (channels) {
        generate(Event::controller(channels, controller_ns::registered_parameter_controller.coarse, short_ns::coarse(value)));
        generate(Event::controller(channels, controller_ns::registered_parameter_controller.fine, short_ns::fine(value)));
    }
}

void PitchWheel::updatePitchRangeText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mPitchRanges, channels))
        slider()->setText(pair.second, QString::number(pair.first));
}

void PitchWheel::updatePitchValueText(channels_t channels) {
    for (channel_t channel : channels) {
        auto scale = (double)mPitchRanges[channel];
        range_t<double> scaleRange = {-scale, scale};
        double semitones = scaleRange.rescale(data14Range, mPitchValues[channel]);
        slider()->setText(channels_t::wrap(channel), number2string(semitones, 'f', 2));
    }
}

void PitchWheel::resetAll() {
    mRegisteredParameters.fill(defaultRPN);
    mPitchRanges.fill(defaultSemitones);
    mPitchValues.fill(defaultPitch);
}

Handler::Result PitchWheel::handleCoarseRPN(channels_t channels, byte_t byte) {
    for (channel_t c : channels)
        mRegisteredParameters[c] = short_ns::alter_coarse(mRegisteredParameters[c], byte);
    return Result::success;
}

Handler::Result PitchWheel::handleFineRPN(channels_t channels, byte_t byte) {
    for (channel_t c : channels)
        mRegisteredParameters[c] = short_ns::alter_fine(mRegisteredParameters[c], byte);
    return Result::success;
}

Handler::Result PitchWheel::handleCoarseDataEntry(channels_t channels, byte_t byte) {
    channels &= channel_ns::find(mRegisteredParameters, pitchBendRangeRPN);
    if (channels) {
        channel_ns::store(mPitchRanges, channels, byte);
        if (rangeDisplayed())
            slider()->setRatio(channels, semitonesRange.reduce(byte));
        else
            updatePitchValueText(channels);
    }
    return Result::success;
}

Handler::Result PitchWheel::handlePitchValue(channels_t channels, uint16_t value) {
    channel_ns::store(mPitchValues, channels, value);
    if (!rangeDisplayed())
        slider()->setRatio(channels, data14Range.reduce(value));
    return Result::success;
}

Handler::Result PitchWheel::handleAllControllersOff(channels_t channels) {
    channel_ns::store(mRegisteredParameters, channels, defaultRPN);
    return handlePitchValue(channels, defaultPitch);
}

Handler::Result PitchWheel::handleReset() {
    resetAll();
    slider()->setDefault(channels_t::full());
    return Result::success;
}

//==================
// MetaProgramWheel
//==================

MetaProgramWheel::MetaProgramWheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("ProgramWheel");
}

void MetaProgramWheel::setContent(HandlerProxy& proxy) {
    proxy.setContent(new ProgramWheel);
}

//==============
// ProgramWheel
//==============

ProgramWheel::ProgramWheel() : AbstractWheel(Mode::io()) {
    mPrograms.fill(0);
    prepare(.0);
}

families_t ProgramWheel::handled_families() const {
    return families_t::fuse(family_t::extended_system, family_t::program_change);
}

Handler::Result ProgramWheel::handle_message(const Message& message) {
    if (message.event.family() == family_t::program_change) {
        setProgramChange(message.event.channels(), message.event.at(1));
        return Result::success;
    }
    return Result::unhandled;
}

void ProgramWheel::setProgramChange(channels_t channels, byte_t program) {
    channel_ns::store(mPrograms, channels, program);
    slider()->setRatio(channels, data7Range.reduce(program));
}

void ProgramWheel::onMove(channels_t channels, qreal ratio) {
    auto program = data7Range.expand(ratio);
    channel_ns::store(mPrograms, channels, program);
    if (canGenerate() && channels)
        generate(Event::program_change(channels, program));
}

void ProgramWheel::updateText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mPrograms, channels))
        slider()->setText(pair.second, QString::number(pair.first));
}

//==================
// MetaVolume1Wheel
//==================

MetaVolume1Wheel::MetaVolume1Wheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("Volume1Wheel");
}

void MetaVolume1Wheel::setContent(HandlerProxy& proxy) {
    proxy.setContent(new Volume1Wheel);
}

//==============
// Volume1Wheel
//==============

Volume1Wheel::Volume1Wheel() : AbstractWheel(Mode::in()) {
    slider()->setExpanded(false);
    slider()->setOrientation(Qt::Horizontal);
    prepare(.5);
}

void Volume1Wheel::onMove(channels_t /*channels*/, qreal ratio) {
    if (canGenerate())
        generate(Event::master_volume(data14Range.expand(ratio)));
}

void Volume1Wheel::updateText(channels_t channels) {
    slider()->setText(channels, stringForRatio(slider()->ratio()));
}

//==================
// MetaVolume2Wheel
//==================

MetaVolume2Wheel::MetaVolume2Wheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("Volume2Wheel");
}

void MetaVolume2Wheel::setContent(HandlerProxy& proxy) {
    proxy.setContent(new Volume2Wheel);
}

//==============
// Volume2Wheel
//==============

Volume2Wheel::Volume2Wheel() : AbstractWheel(Mode::in()) {
    slider()->setExpanded(false);
    slider()->setOrientation(Qt::Horizontal);
    prepare(.5);
}

void Volume2Wheel::onMove(channels_t /*channels*/, qreal ratio) {
    if (canGenerate()) {
        auto volume = volumeRange.expand(ratio);
        generate(volume_event(volume, volume));
    }
}

void Volume2Wheel::updateText(channels_t channels) {
    slider()->setText(channels, stringForRatio(slider()->ratio()));
}
