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

#include "wheel.h"
//#include "editors.h"
#include "systemhandler.h"

#include <QGroupBox>
#include <QPushButton>

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

static constexpr range_t<uint16_t> volumeRange = {0, 0xffff};
static constexpr range_t<byte_t> semitonesRange = {0, 24};
static constexpr range_t<byte_t> data7Range = {0, 0x7f};
static constexpr range_t<uint16_t> data14Range = {0, 0x3fff};

namespace {

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

AbstractWheel::AbstractWheel(mode_type mode, const QString& name, QWidget* parent) :
    GraphicalHandler(mode, name, parent) {

    mSlider = new ChannelsSlider(Qt::Vertical, this);
    mSlider->setTextWidth(40);
    connect(mSlider, &ChannelsSlider::knobChanged, this, &AbstractWheel::updateText);
    connect(mSlider, &ChannelsSlider::knobMoved, this, &AbstractWheel::onValueMove);

    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

ChannelsSlider* AbstractWheel::slider() {
    return mSlider;
}

QMap<QString, QString> AbstractWheel::getParameters() const {
    auto result = GraphicalHandler::getParameters();
    result["orientation"] = serial::serializeOrientation(mSlider->orientation());
    return result;
}

size_t AbstractWheel::setParameter(const QString& key, const QString& value) {
    if (key == "orientation")
        UNSERIALIZE(mSlider->setOrientation, serial::parseOrientation, value);
    return GraphicalHandler::setParameter(key, value);
}

Handler::result_type AbstractWheel::on_close(state_type state) {
    mSlider->setDefault(all_channels);
    return GraphicalHandler::on_close(state);
}

void AbstractWheel::prepare(qreal defaultRatio) {
    mSlider->setDefaultRatio(defaultRatio);
    mSlider->setDefault(all_channels);
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

MetaHandler::instance_type MetaControllerWheel::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new ControllerWheel(name, parent), nullptr);
}

//=================
// ControllerWheel
//=================

ControllerWheel::ControllerWheel(const QString& name, QWidget* parent) :
    AbstractWheel(io_mode, name, parent) {

    mDefaultValues.fill(0);
    for (const auto& value : controller_tools::info())
        mDefaultValues[value.first] = value.second.default_value;

    for (byte_t b=0 ; b < 0x80 ; b++)
        mValues[b].fill(mDefaultValues[b]);

    mControllerBox = new QComboBox(this);
    for (const auto& value : controller_tools::info())
        if (!value.second.is_action)
            mControllerBox->addItem(QString::fromStdString(value.second.name), QVariant(value.first));
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
        qWarning() << "unknown controller";
    else
        mControllerBox->setCurrentIndex(index); // will update mController
}

QMap<QString, QString> ControllerWheel::getParameters() const {
    auto result = AbstractWheel::getParameters();
    result["controller"] = serial::serializeByte(mController);
    return result;
}

size_t ControllerWheel::setParameter(const QString& key, const QString& value) {
    if (key == "controller") {
        UNSERIALIZE(setController, serial::parseByte, value);
        return 1;
    }
    return AbstractWheel::setParameter(key, value);
}

families_t ControllerWheel::handled_families() const {
    return families_t::merge(family_t::custom, family_t::controller, family_t::reset); // channel_pressure;
}

Handler::result_type ControllerWheel::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    switch (message.event.family()) {
    case family_t::controller: receiveController(message.event.channels(), message.event.at(1), message.event.at(2)); return result_type::success;
    case family_t::reset: resetController(all_channels); return result_type::success;
    }
    return result_type::unhandled;
}

void ControllerWheel::onControlChange() {
    mController = mControllerBox->currentData().value<byte_t>();
    channel_map_t<qreal> ratios;
    for (channel_t c=0 ; c < 0x10 ; c++)
        ratios[c] = data7Range.reduce(mValues[mController][c]);
    slider()->setDefaultRatio(data7Range.reduce(mDefaultValues[mController]));
    slider()->setRatios(ratios);
}

void ControllerWheel::onMove(channels_t channels, qreal ratio) {
    if (is_msb_cleared(mController)) {
        auto byte = data7Range.expand(ratio);
        channel_ns::store(mValues[mController], channels, byte);
        if (canGenerate() && channels)
            generate(Event::controller(channels, mController, byte));
    }
}

void ControllerWheel::updateText(channels_t channels) {
    for (channel_t channel : channels)
        slider()->setText(channels_t::merge(channel), stringForRatio(slider()->ratio(channel)));
}

void ControllerWheel::receiveController(channels_t channels, byte_t controller, byte_t value) {
    if (controller == all_controllers_off_controller) {
        resetController(channels);
    } else {
        channel_ns::store(mValues[controller], channels, value);
        if (controller == mController)
            slider()->setRatio(channels, data7Range.reduce(value));
    }
}

void ControllerWheel::resetController(channels_t channels) {
    for (byte_t b=0 ; b < 0x80 ; b++)
        channel_ns::store(mValues[b], channels, mDefaultValues[b]);
    slider()->setDefault(channels);
}

//================
// MetaPitchWheel
//================

MetaPitchWheel::MetaPitchWheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("PitchWheel");
}

MetaHandler::instance_type MetaPitchWheel::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new PitchWheel(name, parent), nullptr);
}

//============
// PitchWheel
//============

PitchWheel::PitchWheel(const QString& name, QWidget* parent) : AbstractWheel(io_mode, name, parent) {

    mRegisteredParameters.fill(0x3fff);
    mPitchRanges.fill(2);
    mPitchValues.fill(0x2000);

    mTypeBox = new QComboBox(this);
    mTypeBox->addItem("Pitch Bend");
    mTypeBox->addItem("Pitch Bend Range");
    connect(mTypeBox, SIGNAL(currentIndexChanged(int)), SLOT(onTypeChange(int)));

    connect(slider(), &ChannelsSlider::knobPressed, this, &PitchWheel::onPress);
    connect(slider(), &ChannelsSlider::knobReleased, this, &PitchWheel::onRelease);

    prepare(.5);

    static_cast<QVBoxLayout*>(layout())->insertWidget(0, mTypeBox);
}

families_t PitchWheel::handled_families() const {
    return families_t::merge(family_t::custom, family_t::controller, family_t::pitch_wheel, family_t::reset);
}

Handler::result_type PitchWheel::handle_message(const Message& message) {
    /// @note data_entry_fine_controller is ignored
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    switch (message.event.family()) {
    case family_t::controller:
        switch (message.event.at(1)) {
        case registered_parameter_coarse_controller:
            receiveCoarseRPN(message.event.channels(), message.event.at(2));
            return result_type::success;
        case registered_parameter_fine_controller:
            receiveFineRPN(message.event.channels(), message.event.at(2));
            return result_type::success;
        case data_entry_coarse_controller:
            receivePitchRange(message.event.channels() & channel_ns::find(mRegisteredParameters, 0x0000), message.event.at(2));
            return result_type::success;
        }
        break;
    case family_t::pitch_wheel:
        receivePitchValue(message.event.channels(), message.event.get_14bits());
        return result_type::success;
    case family_t::reset:
        resetPitch(all_channels);
        return result_type::success;
    }
    return result_type::unhandled;
}

Handler::result_type PitchWheel::on_close(state_type state) {
    mRegisteredParameters.fill(0x3fff);
    mPitchRanges.fill(2);
    mPitchValues.fill(0x2000);
    return AbstractWheel::on_close(state);
}

void PitchWheel::onPress(channels_t channels) {
    /// registered parameter 0x0000 is the Pitch Bend Range
    /// @fixme does not work while scrolling
    generateRegisteredParameter(channels, 0x0000);
}

void PitchWheel::onRelease(channels_t channels) {
    /// registered parameter 0x3fff reset
    /// @fixme does not work while scrolling
    generateRegisteredParameter(channels, 0x3fff);
}

void PitchWheel::onMove(channels_t channels, qreal ratio) {
    if (rangeDisplayed()) {
        auto semitones = semitonesRange.expand(ratio);
        channel_ns::store(mPitchRanges, channels, semitones);
        if (canGenerate() && channels)
            generate(Event::controller(channels, data_entry_coarse_controller, semitones));
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
        defaultRatio = semitonesRange.reduce(2);
        for (channel_t c=0 ; c < 0x10 ; c++)
            ratios[c] = semitonesRange.reduce(mPitchRanges[c]);
    } else {
        defaultRatio = .5;
        for (channel_t c=0 ; c < 0x10 ; c++)
            ratios[c] = data14Range.reduce(mPitchValues[c]);
    }
    slider()->setDefaultRatio(defaultRatio);
    slider()->setRatios(ratios);
}

bool PitchWheel::rangeDisplayed() const {
    return mTypeBox->currentIndex() == 1;
}

void PitchWheel::generateRegisteredParameter(channels_t channels, uint16_t value) {
    if (canGenerate() && channels && rangeDisplayed()) {
        generate(Event::controller(channels, registered_parameter_coarse_controller, short_tools::coarse(value)));
        generate(Event::controller(channels, registered_parameter_fine_controller, short_tools::fine(value)));
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
        QString repr = QString::number(semitones, 'f', 2);
        if (semitones > 0)
            repr.prepend("+");
        slider()->setText(channels_t::merge(channel), repr);
    }
}

void PitchWheel::receiveCoarseRPN(channels_t channels, byte_t byte) {
    for (channel_t c : channels)
        mRegisteredParameters[c] = short_tools::alter_coarse(mRegisteredParameters[c], byte);
}

void PitchWheel::receiveFineRPN(channels_t channels, byte_t byte) {
    for (channel_t c : channels)
        mRegisteredParameters[c] = short_tools::alter_fine(mRegisteredParameters[c], byte);
}

void PitchWheel::receivePitchRange(channels_t channels, byte_t semitones) {
    if (channels) {
        channel_ns::store(mPitchRanges, channels, semitones);
        if (rangeDisplayed())
            slider()->setRatio(channels, semitonesRange.reduce(semitones));
        else
            updateText(channels);
    }
}

void PitchWheel::receivePitchValue(channels_t channels, uint16_t value) {
    channel_ns::store(mPitchValues, channels, value);
    if (!rangeDisplayed())
        slider()->setRatio(channels, data14Range.reduce(value));
}

void PitchWheel::resetPitch(channels_t channels) {
    channel_ns::store(mPitchValues, channels, 0x2000);
    if (!rangeDisplayed())
        slider()->setDefault(channels);
}

//==================
// MetaProgramWheel
//==================

MetaProgramWheel::MetaProgramWheel(QObject* parent) : MetaWheel(parent) {
    setIdentifier("ProgramWheel");
}

MetaHandler::instance_type MetaProgramWheel::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new ProgramWheel(name, parent), nullptr);
}

//==============
// ProgramWheel
//==============

ProgramWheel::ProgramWheel(const QString& name, QWidget* parent) : AbstractWheel(io_mode, name, parent) {
    mPrograms.fill(0);
    prepare(.0);
}

families_t ProgramWheel::handled_families() const {
    return families_t::merge(family_t::custom, family_t::program_change);
}

Handler::result_type ProgramWheel::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    if (message.event.family() == family_t::program_change) {
        setProgramChange(message.event.channels(), message.event.at(1));
        return result_type::success;
    }
    return result_type::unhandled;
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

MetaHandler::instance_type MetaVolume1Wheel::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new Volume1Wheel(name, parent), nullptr);
}

//==============
// Volume1Wheel
//==============

Volume1Wheel::Volume1Wheel(const QString& name, QWidget* parent) : AbstractWheel(in_mode, name, parent) {
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

MetaHandler::instance_type MetaVolume2Wheel::instantiate(const QString& name, QWidget* parent) {
    return instance_type(new Volume2Wheel(name, parent), nullptr);
}

//==============
// Volume2Wheel
//==============

Volume2Wheel::Volume2Wheel(const QString& name, QWidget* parent) :
    AbstractWheel(in_mode, name, parent) {

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
