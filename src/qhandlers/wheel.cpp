/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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

constexpr range_t<byte_t> semitonesRange = {0, 24};
constexpr range_t<byte_t> data7Range = {0, 0x7f};
constexpr range_t<uint16_t> data14Range = {0, 0x3fff};
constexpr range_t<int> panRange = {-64, 63};
constexpr range_t<int> percentRange {0, 100};

constexpr byte_t defaultSemitones = 2;
constexpr uint16_t defaultRPN = 0x3fff;
constexpr uint16_t defaultPitch = 0x2000;
constexpr uint16_t pitchBendRangeRPN = 0x0000;

}

//===============
// AbstractWheel
//===============

MetaHandler* makeMetaWheel(QObject* parent) {
    auto* meta = makeMetaGraphicalHandler(parent);
    meta->addParameter({"orientation", "orientation of the slider", "Horizontal", MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"expanded", "display one knob per channel", "true", MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"selection", "bitmask of selected channels", serial::serializeChannels({}), MetaHandler::MetaParameter::Visibility::advanced});
    meta->addParameter({"visibility", "bitmask of visible channels", serial::serializeChannels(channels_t::full()), MetaHandler::MetaParameter::Visibility::advanced});
    return meta;
}

AbstractWheel::AbstractWheel(Mode mode) : GraphicalHandler{mode} {
    mSlider = new ChannelsSlider{Qt::Horizontal, this};
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
    SERIALIZE("expanded", serial::serializeBool, mSlider->isExpanded(), result);
    SERIALIZE("selection", serial::serializeChannels, mSlider->selection(), result);
    SERIALIZE("visibility", serial::serializeChannels, mSlider->visibleChannels(), result);
    return result;
}

size_t AbstractWheel::setParameter(const Parameter& parameter) {
    UNSERIALIZE("orientation", serial::parseOrientation, mSlider->setOrientation, parameter);
    UNSERIALIZE("expanded", serial::parseBool, mSlider->setExpanded, parameter);
    UNSERIALIZE("selection", serial::parseChannels, mSlider->setSelection, parameter);
    UNSERIALIZE("visibility", serial::parseChannels, mSlider->setVisibleChannels, parameter);
    return GraphicalHandler::setParameter(parameter);
}

Handler::Result AbstractWheel::handle_open(State state) {
    if (state.any(Handler::State::forward()))
        mSlider->setMovable(true);
    return GraphicalHandler::handle_open(state);
}

Handler::Result AbstractWheel::handle_close(State state) {
    if (state.any(Handler::State::receive()))
        mSlider->setDefault(channels_t::full());
    if (state.any(Handler::State::forward()))
        mSlider->setMovable(false);
    return GraphicalHandler::handle_close(state);
}

void AbstractWheel::updateContext(Context* context) {
    mSlider->setChannelEditor(context->channelEditor());
}

void AbstractWheel::onValueMove(channels_t channels, qreal ratio) {
    onMove(channels, ratio);
    updateText(channels);
}

//=================
// ControllerWheel
//=================

MetaHandler* makeMetaControllerWheel(QObject* parent) {
    auto* meta = makeMetaWheel(parent);
    meta->setIdentifier("ControllerWheel");
    meta->setDescription("A collection of sliders used to change control settings such as modulation, volume, ...");
    meta->addParameter({"controller", "controller id(s) reacting over the GUI", "0x00", MetaHandler::MetaParameter::Visibility::basic});
    meta->setFactory(new OpenProxyFactory<ControllerWheel>);
    return meta;
}

ControllerWheel::ControllerWheel() : AbstractWheel{Mode::io()} {
    mControllerBox = new QComboBox{this};
    for (const auto& value : controller_ns::controller_names())
        if (!controller_ns::is_channel_mode_message(value.first))
            mControllerBox->addItem(QString::fromStdString(value.second), QVariant{value.first});
    connect(mControllerBox, SIGNAL(currentIndexChanged(int)), SLOT(onControlChange()));
    resetAll();
    onControlChange(); // update controller
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, mControllerBox);
}

byte_t ControllerWheel::controller() const {
    return mController;
}

void ControllerWheel::setController(byte_t controller) {
    const int index = mControllerBox->findData(controller);
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
    return families_t::fuse(family_t::controller, family_t::reset); // channel_pressure;
}

Handler::Result ControllerWheel::handle_message(const Message& message) {
    switch (message.event.family()) {
    case family_t::controller: return handleController(message.event.channels(), extraction_ns::controller(message.event), extraction_ns::controller_value(message.event));
    case family_t::reset: return handleReset();
    default: return Result::unhandled;
    }
}

Handler::Result ControllerWheel::handle_close(State state) {
    if (state.any(Handler::State::receive()))
        resetAll();
    return AbstractWheel::handle_close(state);
}

void ControllerWheel::onControlChange() {
    mController = mControllerBox->currentData().value<byte_t>();
    channel_map_t<qreal> ratios;
    for (channel_t c=0 ; c < channels_t::capacity() ; c++)
        ratios[c] = reduce(data7Range, mValues[mController][c]);
    slider()->setCardinality(span(data7Range) + 1);
    slider()->setDefaultRatio(reduce(data7Range, controller_ns::default_value(mController)));
    slider()->setRatios(ratios);
}

void ControllerWheel::onMove(channels_t channels, qreal ratio) {
    const auto byte = expand(ratio, data7Range);
    channel_ns::store(mValues[mController], channels, byte);
    if (canGenerate() && channels)
        generate(Event::controller(channels, mController, byte));
}

void ControllerWheel::updateText(channels_t channels) {
    if (controller_ns::default_value(mController) == 0x40) { // Centered
        for (const auto& pair : channel_ns::reverse(mValues[mController], channels))
            slider()->setText(pair.second, number2string(rescale(data7Range, pair.first, panRange)));
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
        slider()->setRatio(channels, reduce(data7Range, value));
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

//============
// PitchWheel
//============

MetaHandler* makeMetaPitchWheel(QObject* parent) {
    auto* meta = makeMetaWheel(parent);
    meta->setIdentifier("PitchWheel");
    meta->setDescription("A collection of sliders used to tweak the pitch in addition to the pitch sensitivity");
    meta->setFactory(new OpenProxyFactory<PitchWheel>);
    return meta;
}

PitchWheel::PitchWheel() : AbstractWheel{Mode::io()} {
    mTypeBox = new QComboBox{this};
    mTypeBox->addItem("Pitch Bend");
    mTypeBox->addItem("Pitch Bend Range");
    connect(mTypeBox, SIGNAL(currentIndexChanged(int)), SLOT(onTypeChange(int)));
    connect(slider(), &ChannelsSlider::knobPressed, this, &PitchWheel::onPress);
    connect(slider(), &ChannelsSlider::knobReleased, this, &PitchWheel::onRelease);
    resetAll();
    onTypeChange(0);
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, mTypeBox);
}

families_t PitchWheel::handled_families() const {
    return families_t::fuse(family_t::controller, family_t::pitch_wheel, family_t::reset);
}

Handler::Result PitchWheel::handle_message(const Message& message) {
    /// @note data_entry_fine_controller is ignored
    switch (message.event.family()) {
    case family_t::controller:
        switch (extraction_ns::controller(message.event)) {
        case controller_ns::registered_parameter_controller.coarse: return handleCoarseRPN(message.event.channels(), extraction_ns::controller_value(message.event));
        case controller_ns::registered_parameter_controller.fine: return handleFineRPN(message.event.channels(), extraction_ns::controller_value(message.event));
        case controller_ns::non_registered_parameter_controller.coarse: return handleCoarseRPN(message.event.channels(), 0x7f);
        case controller_ns::non_registered_parameter_controller.fine: return handleFineRPN(message.event.channels(), 0x7f);
        case controller_ns::data_entry_controller.coarse: return handleCoarseDataEntry(message.event.channels(), extraction_ns::controller_value(message.event));
        case controller_ns::all_controllers_off_controller: return handleAllControllersOff(message.event.channels());
        }
        break;
    case family_t::pitch_wheel: return handlePitchValue(message.event.channels(), extraction_ns::get_14bits(message.event));
    case family_t::reset: return handleReset();
    }
    return Result::unhandled;
}

Handler::Result PitchWheel::handle_close(State state) {
    if (state.any(Handler::State::receive()))
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
        const auto semitones = expand(ratio, semitonesRange);
        channel_ns::store(mPitchRanges, channels, semitones);
        if (canGenerate() && channels) {
            const auto channelsNotReady = channels & ~channel_ns::find(mRegisteredParameters, pitchBendRangeRPN);
            generateRegisteredParameter(channelsNotReady, pitchBendRangeRPN);
            generate(Event::controller(channels, controller_ns::data_entry_controller.coarse, semitones));
            generateRegisteredParameter(channelsNotReady, defaultRPN);
        }
    } else {
        const auto value = expand(ratio, data14Range);
        channel_ns::store(mPitchValues, channels, value);
        if (canGenerate() && channels)
            generate(Event::pitch_wheel(channels, short_ns::cut(value)));
    }
}

void PitchWheel::updateText(channels_t channels) {
    if (rangeDisplayed())
        updatePitchRangeText(channels);
    else
        updatePitchValueText(channels);
}

void PitchWheel::onTypeChange(int index) {
    size_t cardinality;
    qreal defaultRatio;
    channel_map_t<qreal> ratios;
    if (index == 1) {
        cardinality = span(semitonesRange) + 1;
        defaultRatio = reduce(semitonesRange, defaultSemitones);
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            ratios[c] = reduce(semitonesRange, mPitchRanges[c]);
    } else {
        cardinality = 0; // continuous range
        defaultRatio = .5;
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            ratios[c] = reduce(data14Range, mPitchValues[c]);
    }
    slider()->setCardinality(cardinality);
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
        const auto scale = static_cast<double>(mPitchRanges[channel]);
        const auto scaleRange = range_ns::from_bounds(-scale, scale);
        const auto semitones = rescale(data14Range, mPitchValues[channel], scaleRange);
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
            slider()->setRatio(channels, reduce(semitonesRange, byte));
        else
            updatePitchValueText(channels);
    }
    return Result::success;
}

Handler::Result PitchWheel::handlePitchValue(channels_t channels, uint16_t value) {
    channel_ns::store(mPitchValues, channels, value);
    if (!rangeDisplayed())
        slider()->setRatio(channels, reduce(data14Range, value));
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

//==============
// ProgramWheel
//==============

MetaHandler* makeMetaProgramWheel(QObject* parent) {
    auto* meta = makeMetaWheel(parent);
    meta->setIdentifier("ProgramWheel");
    meta->setDescription("A collection of sliders used to change quickly the selected programs");
    meta->setFactory(new OpenProxyFactory<ProgramWheel>);
    return meta;
}

ProgramWheel::ProgramWheel() : AbstractWheel{Mode::io()} {
    mPrograms.fill(0);
    slider()->setCardinality(span(data7Range) + 1);
    slider()->setDefaultRatio(0.);
    slider()->setDefault(channels_t::full());
}

families_t ProgramWheel::handled_families() const {
    return families_t::fuse(family_t::program_change);
}

Handler::Result ProgramWheel::handle_message(const Message& message) {
    if (message.event.is(family_t::program_change)) {
        setProgramChange(message.event.channels(), extraction_ns::program(message.event));
        return Result::success;
    }
    return Result::unhandled;
}

void ProgramWheel::setProgramChange(channels_t channels, byte_t program) {
    channel_ns::store(mPrograms, channels, program);
    slider()->setRatio(channels, reduce(data7Range, program));
}

void ProgramWheel::onMove(channels_t channels, qreal ratio) {
    const auto program = expand(ratio, data7Range);
    channel_ns::store(mPrograms, channels, program);
    if (canGenerate() && channels)
        generate(Event::program_change(channels, program));
}

void ProgramWheel::updateText(channels_t channels) {
    for (const auto& pair : channel_ns::reverse(mPrograms, channels))
        slider()->setText(pair.second, QString::number(pair.first));
}

//=============
// VolumeWheel
//=============

MetaHandler* makeMetaVolumeWheel(QObject* parent) {
    auto* meta = makeMetaGraphicalHandler(parent);
    meta->setIdentifier("VolumeWheel");
    meta->setDescription("A simple slider using sysex messages to control the master volume");
    meta->addParameter({"orientation", "orientation of the slider", "Horizontal", MetaHandler::MetaParameter::Visibility::basic});
    meta->setFactory(new OpenProxyFactory<VolumeWheel>);
    return meta;
}

VolumeWheel::VolumeWheel() : GraphicalHandler{Mode::io()} {
    mSlider = makeHorizontalSlider(data14Range, expand(.5, data14Range), this);
    mSlider->setFormatter([](auto value) {
        return QString::number(rescale(data14Range, value, percentRange)) + "%";
    });
    mSlider->setDefault();
    mSlider->setNotifier([this](auto value) {
        if (canGenerate() && mGenerateOnChange)
            generate(Event::master_volume(short_ns::cut(value)));
    });
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

HandlerView::Parameters VolumeWheel::getParameters() const {
    auto result = GraphicalHandler::getParameters();
    SERIALIZE("orientation", serial::serializeOrientation, mSlider->orientation(), result);
    return result;
}

size_t VolumeWheel::setParameter(const Parameter& parameter) {
    UNSERIALIZE("orientation", serial::parseOrientation, mSlider->setOrientation, parameter);
    return GraphicalHandler::setParameter(parameter);
}

Handler::Result VolumeWheel::handle_open(State state) {
    if (state.any(Handler::State::forward()))
        mSlider->particle()->setMovable(true);
    return GraphicalHandler::handle_open(state);
}

Handler::Result VolumeWheel::handle_close(State state) {
    if (state.any(Handler::State::receive()))
        mSlider->setDefault();
    if (state.any(Handler::State::forward()))
        mSlider->particle()->setMovable(false);
    return GraphicalHandler::handle_close(state);
}

Handler::Result VolumeWheel::handle_message(const Message& message) {
    if (message.event.is(family_t::sysex)) {
        short_ns::uint14_t volume;
        byte_t sysex_channel;
        if (extraction_ns::get_master_volume(message.event, volume, sysex_channel)) {
            mGenerateOnChange = false;
            mSlider->setValue(short_ns::glue(volume));
            mGenerateOnChange = true;
            return Result::success;
        }
    }
    return Result::unhandled;
}

families_t VolumeWheel::handled_families() const {
    return families_t::wrap(family_t::sysex);
}
