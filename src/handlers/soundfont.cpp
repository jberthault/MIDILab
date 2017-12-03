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

#include "soundfont.h"

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <sstream>
#include <fluidsynth.h>
#include "qcore/manager.h"
#include "qtools/misc.h"

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

namespace {

static constexpr exp_range_t<double> gainRange = {{0., 10.}, 1.};
static constexpr range_t<double> roomsizeRange = {0., 1.2};
static constexpr range_t<double> dampRange = {0., 1.};
static constexpr range_t<double> levelRange = {0., 1.};
static constexpr exp_range_t<double> widthRange = {{0., 100.}, 10.};


static constexpr SoundFontHandler::reverb_type defaultReverb = {
    FLUID_REVERB_DEFAULT_ROOMSIZE,
    FLUID_REVERB_DEFAULT_DAMP,
    FLUID_REVERB_DEFAULT_LEVEL,
    FLUID_REVERB_DEFAULT_WIDTH,
};

}

template<>
struct marshalling_traits<SoundFontHandler::reverb_type> {
    auto operator()(const SoundFontHandler::reverb_type& reverb) {
        std::stringstream ss;
        ss << reverb.roomsize << ' ' << reverb.damp << ' ' << reverb.level << ' ' << reverb.width;
        return ss.str();
    }
};

template<>
struct unmarshalling_traits<SoundFontHandler::reverb_type> {
    auto operator()(const std::string& string) {
        SoundFontHandler::reverb_type reverb;
        std::stringstream ss(string);
        ss >> reverb.roomsize >> reverb.damp  >> reverb.level >> reverb.width;
        return reverb;
    }
};


//======
// Impl
//======

struct SoundFontHandler::Impl {

    Impl() {
        settings = new_fluid_settings();
        fluid_settings_setint(settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
        fluid_settings_setstr(settings, "audio.jack.id", "MIDILab");
        synth = new_fluid_synth(settings);
        adriver = new_fluid_audio_driver(settings, synth);
        has_reverb = true;
    }

    ~Impl() {
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    double gain() const {
        return (double)fluid_synth_get_gain(synth);
    }

    SoundFontHandler::reverb_type reverb() const {
        SoundFontHandler::reverb_type reverb;
        reverb.roomsize = fluid_synth_get_reverb_roomsize(synth);
        reverb.damp = fluid_synth_get_reverb_damp(synth);
        reverb.level = fluid_synth_get_reverb_level(synth);
        reverb.width = fluid_synth_get_reverb_width(synth);
        return reverb;
    }

    void close() {
        fluid_synth_system_reset(synth);
    }

    Handler::result_type reset_system() {
        push(Event::controller(all_channels, all_controllers_off_controller));
        push(Event::controller(all_channels, all_sound_off_controller));
        // push(Event::controller(all_channels, volume_coarse_controller, 100));
        return result_type::success;
    }

    Handler::result_type push(const Event& event) {
        switch (event.family()) {
        case family_t::note_off: for (channel_t channel : event.channels()) fluid_synth_noteoff(synth, channel, event.at(1)); return result_type::success;
        case family_t::note_on: for (channel_t channel : event.channels()) fluid_synth_noteon(synth, channel, event.at(1), event.at(2)); return result_type::success;
        case family_t::program_change: for (channel_t channel : event.channels()) fluid_synth_program_change(synth, channel, event.at(1)); return result_type::success;
        case family_t::controller: for (channel_t channel : event.channels()) fluid_synth_cc(synth, channel, event.at(1), event.at(2)); return result_type::success;
        case family_t::channel_pressure: for (channel_t channel : event.channels()) fluid_synth_channel_pressure(synth, channel, event.at(1)); return result_type::success;
        case family_t::pitch_wheel: for (channel_t channel : event.channels()) fluid_synth_pitch_bend(synth, channel, event.get_14bits()); return result_type::success;
        case family_t::reset: return reset_system();
        case family_t::sysex: {
            /// @note master volume does not seem to be handled correctly
            std::vector<char> sys_ex_data(event.begin()+1, event.end()-1);
            fluid_synth_sysex(synth, sys_ex_data.data(), sys_ex_data.size(), nullptr, nullptr, nullptr, 0);
            return result_type::success;
        }
        case family_t::custom: {
            auto k = event.get_custom_key();
            if (k == "SoundFont.gain") return process_gain(event.get_custom_value());
            if (k == "SoundFont.reverb") return process_reverb(event.get_custom_value());
            if (k == "SoundFont.file") return process_file(event.get_custom_value());
            break;
        }
        }
        return result_type::unhandled;
    }

    Handler::result_type process_gain(std::string string) {
        fluid_synth_set_gain(synth, (float)unmarshall<double>(string));
        return result_type::success;
    }

    Handler::result_type process_reverb(std::string string) {
        has_reverb = !string.empty();
        if (has_reverb) {
            auto reverb = unmarshall<SoundFontHandler::reverb_type>(string);
            fluid_synth_set_reverb(synth, reverb.roomsize, reverb.damp, reverb.width, reverb.level);
        }
        fluid_synth_set_reverb_on(synth, (int)has_reverb);
        return result_type::success;
    }

    Handler::result_type process_file(std::string string) {
        if (fluid_synth_sfload(synth, string.c_str(), 1) == -1)
            return result_type::fail;
        file = std::move(string);
        return result_type::success;
    }

    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    std::string file;
    bool has_reverb;

};

//==================
// SoundFontHandler
//==================

Event SoundFontHandler::gain_event(double gain) {
    return Event::custom({}, "SoundFont.gain", marshall(gain));
}

Event SoundFontHandler::file_event(const std::string& file) {
    return Event::custom({}, "SoundFont.file", file);
}

Event SoundFontHandler::reverb_event(const optional_reverb_type &reverb) {
    if (reverb)
        return Event::custom({}, "SoundFont.reverb", marshall(*reverb));
    else
        return Event::custom({}, "SoundFont.reverb");
}

SoundFontHandler::SoundFontHandler() : Handler(out_mode), m_pimpl(std::make_unique<Impl>()) {

}

double SoundFontHandler::gain() const {
    return m_pimpl->gain();
}

std::string SoundFontHandler::file() const {
    return m_pimpl->file;
}

SoundFontHandler::optional_reverb_type SoundFontHandler::reverb() const {
    SoundFontHandler::optional_reverb_type result;
    if (m_pimpl->has_reverb)
        result = m_pimpl->reverb();
    return result;
}

families_t SoundFontHandler::handled_families() const {
    return families_t::merge(family_t::custom, family_t::sysex, family_t::reset, family_t::note_off, family_t::note_on, family_t::controller, family_t::program_change, family_t::channel_pressure, family_t::pitch_wheel);
}

SoundFontHandler::result_type SoundFontHandler::on_open(state_type state) {
    TRACE_INFO("Sound Font thread priority: " << get_thread_priority());
    return Handler::on_open(state);
}

SoundFontHandler::result_type SoundFontHandler::on_close(state_type state) {
    if (state & receive_state)
        m_pimpl->close();
    return Handler::on_close(state);
}

SoundFontHandler::result_type SoundFontHandler::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;
    return m_pimpl->push(message.event);
}

//===============
// MetaSoundFont
//===============

MetaSoundFont::MetaSoundFont(QObject* parent) : MetaHandler(parent) {
    setIdentifier("SoundFont");
}

MetaHandler::Instance MetaSoundFont::instantiate() {
    auto handler = new SoundFontHandler;
    return Instance(handler, new SoundFontEditor(handler));
}

//===================
// SoundFontReceiver
//===================

SoundFontReceiver::SoundFontReceiver(QObject* parent) : CustomReceiver(parent) {

}

Receiver::result_type SoundFontReceiver::receive_message(Handler* target, const Message& message) {
    auto result = observer()->handleMessage(target, message);
    if (message.event.family() == family_t::custom && message.event.get_custom_key() == "SoundFont.file")
        emit fileHandled();
    return result;
}

//============
// GainEditor
//============

GainEditor::GainEditor(QWidget* parent) : QWidget(parent) {
    mSlider = new SimpleSlider(this);
    mSlider->setTextWidth(35);
    mSlider->setDefaultRatio(gainRange.reduce(.2));
    connect(mSlider, &SimpleSlider::knobChanged, this, &GainEditor::updateText);
    connect(mSlider, &SimpleSlider::knobMoved, this, &GainEditor::onMove);
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

void GainEditor::setGain(double gain) {
    mSlider->setRatio(gainRange.reduce(gain));
    emit gainChanged(gain);
}

void GainEditor::onMove(qreal ratio) {
    auto gain = gainRange.expand(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
    emit gainChanged(gain);
}

void GainEditor::updateText(qreal ratio) {
    auto gain = gainRange.expand(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
}

// =============
// ReverbEditor
// =============

ReverbEditor::ReverbEditor(QWidget* parent) : QGroupBox("Reverb", parent), mReverb(defaultReverb) {

    setCheckable(true);
    setChecked(true);
    connect(this, &ReverbEditor::toggled, this, &ReverbEditor::onToggle);

    mRoomsizeSlider = new SimpleSlider(this);
    mRoomsizeSlider->setTextWidth(35);
    mRoomsizeSlider->setDefaultRatio(roomsizeRange.reduce(defaultReverb.roomsize));
    connect(mRoomsizeSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onRoomsizeChanged);
    connect(mRoomsizeSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onRoomsizeMoved);

    mDampSlider = new SimpleSlider(this);
    mDampSlider->setTextWidth(35);
    mDampSlider->setDefaultRatio(dampRange.reduce(defaultReverb.damp));
    connect(mDampSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onDampChanged);
    connect(mDampSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onDampMoved);

    mLevelSlider = new SimpleSlider(this);
    mLevelSlider->setTextWidth(35);
    mLevelSlider->setDefaultRatio(levelRange.reduce(defaultReverb.level));
    connect(mLevelSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onLevelChanged);
    connect(mLevelSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onLevelMoved);

    mWidthSlider = new SimpleSlider(this);
    mWidthSlider->setTextWidth(35);
    mWidthSlider->setDefaultRatio(widthRange.reduce(defaultReverb.width));
    connect(mWidthSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onWidthChanged);
    connect(mWidthSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onWidthMoved);

    QFormLayout* form = new QFormLayout;
    form->setVerticalSpacing(5);
    form->addRow("Room Size", mRoomsizeSlider);
    form->addRow("Damp", mDampSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Width", mWidthSlider);
    setLayout(form);

}

bool ReverbEditor::isActive() const {
    return isChecked();
}

SoundFontHandler::optional_reverb_type ReverbEditor::reverb() const {
    if (isActive())
        return mReverb;
    return {};
}

SoundFontHandler::reverb_type ReverbEditor::rawReverb() const {
    return mReverb;
}

void ReverbEditor::setActive(bool active) {
    setChecked(active);
    emit reverbChanged(reverb());
}

void ReverbEditor::setRoomSize(double value) {
    mReverb.roomsize = value;
    mRoomsizeSlider->setRatio(roomsizeRange.reduce(mReverb.roomsize));
    if (isActive())
        emit reverbChanged(reverb());
}

void ReverbEditor::setDamp(double value) {
    mReverb.damp = value;
    mDampSlider->setRatio(dampRange.reduce(mReverb.damp));
    if (isActive())
        emit reverbChanged(reverb());
}

void ReverbEditor::setLevel(double value) {
    mReverb.level = value;
    mLevelSlider->setRatio(levelRange.reduce(mReverb.level));
    if (isActive())
        emit reverbChanged(reverb());
}

void ReverbEditor::setWidth(double value) {
    mReverb.width = value;
    mWidthSlider->setRatio(widthRange.reduce(mReverb.width));
    if (isActive())
        emit reverbChanged(reverb());
}

void ReverbEditor::setReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    if (reverb) {
        mReverb = *reverb;
        mRoomsizeSlider->setRatio(roomsizeRange.reduce(mReverb.roomsize));
        mDampSlider->setRatio(dampRange.reduce(mReverb.damp));
        mLevelSlider->setRatio(levelRange.reduce(mReverb.level));
        mWidthSlider->setRatio(widthRange.reduce(mReverb.width));
        setChecked(true);
    } else {
        setChecked(false);
    }
    emit reverbChanged(reverb);
}

void ReverbEditor::onRoomsizeChanged(qreal ratio) {
    mRoomsizeSlider->setText(QString::number(roomsizeRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onRoomsizeMoved(qreal ratio) {
    mReverb.roomsize = roomsizeRange.expand(ratio);
    mRoomsizeSlider->setText(QString::number(mReverb.roomsize, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onDampChanged(qreal ratio) {
    mDampSlider->setText(QString::number(dampRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onDampMoved(qreal ratio)  {
    mReverb.damp = dampRange.expand(ratio);
    mDampSlider->setText(QString::number(mReverb.damp, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onLevelChanged(qreal ratio) {
    mLevelSlider->setText(QString::number(levelRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onLevelMoved(qreal ratio) {
    mReverb.level = levelRange.expand(ratio);
    mLevelSlider->setText(QString::number(mReverb.level, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onWidthChanged(qreal ratio) {
    mWidthSlider->setText(QString::number(widthRange.expand(ratio), 'f', 2));
}
void ReverbEditor::onWidthMoved(qreal ratio) {
    mReverb.width = widthRange.expand(ratio);
    mWidthSlider->setText(QString::number(mReverb.width, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onToggle(bool activated) {
    SoundFontHandler::optional_reverb_type optional_reverb;
    if (activated)
        optional_reverb = mReverb;
    emit reverbChanged(optional_reverb);
}

//=================
// SoundFontEditor
//=================

SoundFontEditor::SoundFontEditor(SoundFontHandler* handler) : HandlerEditor(), mHandler(handler) {

    mReceiver = new SoundFontReceiver(this);
    handler->set_receiver(mReceiver);
    connect(mReceiver, &SoundFontReceiver::fileHandled, this, &SoundFontEditor::updateFile);

    mLoadMovie = new QMovie(":/data/load.gif", QByteArray(), this);
    mLoadLabel = new QLabel(this);
    mLoadLabel->setMovie(mLoadMovie);
    mLoadLabel->hide();

    /// @todo add menu option to set path from a previous one (keep history)
    mFileEditor = new QLineEdit(this);
    mFileEditor->setMinimumWidth(200);
    mFileEditor->setReadOnly(true);
    mFileEditor->setText(QString::fromStdString(handler->file()));

    mFileSelector = new QToolButton(this);
    mFileSelector->setToolTip("Browse SoundFonts");
    mFileSelector->setAutoRaise(true);
    mFileSelector->setIcon(QIcon(":/data/file.svg"));
    connect(mFileSelector, &QToolButton::clicked, this, &SoundFontEditor::onClick);

    mGainEditor = new GainEditor(this);
    mGainEditor->setGain(handler->gain());
    connect(mGainEditor, &GainEditor::gainChanged, this, &SoundFontEditor::sendGain);

    mReverbEditor = new ReverbEditor(this);
    mReverbEditor->setReverb(handler->reverb());
    connect(mReverbEditor, &ReverbEditor::reverbChanged, this, &SoundFontEditor::sendReverb);

    QFormLayout* form = new QFormLayout;
    form->setMargin(0);
    form->addRow("File", make_hbox(spacing_tag{0}, mFileSelector, mFileEditor, mLoadLabel));
    form->addRow("Gain", mGainEditor);
    setLayout(make_vbox(form, mReverbEditor));
}

HandlerView::Parameters SoundFontEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("file", QString::fromStdString, mHandler->file(), result);
    SERIALIZE("gain", serial::serializeDouble, mHandler->gain(), result);
    SERIALIZE("reverb", serial::serializeBool, mReverbEditor->isActive(), result);
    SERIALIZE("reverb.roomsize", serial::serializeDouble, mReverbEditor->rawReverb().roomsize, result);
    SERIALIZE("reverb.damp", serial::serializeDouble, mReverbEditor->rawReverb().damp, result);
    SERIALIZE("reverb.level", serial::serializeDouble, mReverbEditor->rawReverb().level, result);
    SERIALIZE("reverb.width", serial::serializeDouble, mReverbEditor->rawReverb().width, result);
    return result;
}

size_t SoundFontEditor::setParameter(const Parameter& parameter) {
    if (parameter.name == "file") {
        setFile(parameter.value);
        return 1;
    }
    UNSERIALIZE("gain", serial::parseDouble, setGain, parameter);
    UNSERIALIZE("reverb", serial::parseBool, mReverbEditor->setActive, parameter);
    UNSERIALIZE("reverb.roomsize", serial::parseDouble, mReverbEditor->setRoomSize, parameter);
    UNSERIALIZE("reverb.damp", serial::parseDouble, mReverbEditor->setDamp, parameter);
    UNSERIALIZE("reverb.level", serial::parseDouble, mReverbEditor->setLevel, parameter);
    UNSERIALIZE("reverb.width", serial::parseDouble, mReverbEditor->setWidth, parameter);
    return HandlerEditor::setParameter(parameter);
}

void SoundFontEditor::setFile(const QString& file) {
    if (mHandler->send_message(SoundFontHandler::file_event(file.toStdString()))) {
        mLoadMovie->start();
        mLoadLabel->show();
    }
}

void SoundFontEditor::setGain(double gain) {
    mGainEditor->setGain(gain);
}

void SoundFontEditor::setReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    mReverbEditor->setReverb(reverb);
}

void SoundFontEditor::updateFile() {
    QFileInfo fileInfo(QString::fromStdString(mHandler->file()));
    mFileEditor->setText(fileInfo.completeBaseName());
    mFileEditor->setToolTip(fileInfo.absoluteFilePath());
    mLoadMovie->stop();
    mLoadLabel->hide();
}

void SoundFontEditor::sendGain(double gain) {
    mHandler->send_message(SoundFontHandler::gain_event(gain));
}

void SoundFontEditor::sendReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    mHandler->send_message(SoundFontHandler::reverb_event(reverb));
}

void SoundFontEditor::onClick() {
    auto file = context()->pathRetriever("soundfont")->getReadFile(this);
    if (!file.isNull())
        setFile(file);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
