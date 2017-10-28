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

#include <fluidsynth.h>
#include "qcore/manager.h"
#include "qtools/misc.h"

using namespace family_ns;
using namespace controller_ns;
using namespace handler_ns;

namespace {

static constexpr range_t<double> gainRange = {0., 10.};
static constexpr double gainPivot = 1.; /*!< gain value that will be at 50% on the scale */
static constexpr double gainFactor = 2. * std::log((gainRange.max - gainPivot) / (gainPivot - gainRange.min));
static constexpr range_t<double> gainExpRange = {1., std::exp(gainFactor)};

double gainForRatio(qreal ratio) {
    return gainRange.rescale(gainExpRange, std::exp(gainFactor*ratio));
}

qreal ratioForGain(double gain) {
    return std::log(gainExpRange.rescale(gainRange, gain))/gainFactor;
}

}

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
    }

    ~Impl() {
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    double gain() const {
        return (double)fluid_synth_get_gain(synth);
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
            auto v = event.get_custom_value();
            if (k == "SoundFont.gain") {
                fluid_synth_set_gain(synth, (float)unmarshall<double>(v));
                return result_type::success;
            } else if (k == "SoundFont.file") {
                if (fluid_synth_sfload(synth, v.c_str(), 1) == -1)
                    return result_type::fail;
                file = std::move(v);
                return result_type::success;
            }
            break;
        }
        }
        return result_type::unhandled;
    }

    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
    std::string file;

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

SoundFontHandler::SoundFontHandler() : Handler(out_mode), m_pimpl(std::make_unique<Impl>()) {

}

double SoundFontHandler::gain() const {
    return m_pimpl->gain();
}

std::string SoundFontHandler::file() const {
    return m_pimpl->file;
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

MetaHandler::instance_type MetaSoundFont::instantiate(const QString& name, QWidget* parent) {
    SoundFontHandler* handler = new SoundFontHandler;
    handler->set_name(name.toStdString());
    return instance_type(handler, new SoundFontEditor(handler, parent));
}

//============
// GainEditor
//============

GainEditor::GainEditor(QWidget* parent) : QWidget(parent) {
    mSlider = new SimpleSlider(this);
    mSlider->setTextWidth(35);
    mSlider->setDefaultRatio(ratioForGain(.2));
    connect(mSlider, &SimpleSlider::knobChanged, this, &GainEditor::updateText);
    connect(mSlider, &SimpleSlider::knobMoved, this, &GainEditor::onMove);
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

void GainEditor::setGain(double gain) {
    mSlider->setRatio(ratioForGain(gain));
    emit gainChanged(gain);
}

void GainEditor::onMove(qreal ratio) {
    auto gain = gainForRatio(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
    emit gainChanged(gain);
}

void GainEditor::updateText(qreal ratio) {
    auto gain = gainForRatio(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
}

//=================
// SoundFontEditor
//=================

SoundFontEditor::SoundFontEditor(SoundFontHandler* handler, QWidget* parent) :
    HandlerEditor(handler, parent) {

    /// @todo add menu option to set path from a previous one (keep history)
    mFileEditor = new QLineEdit(this);
    mFileEditor->setMinimumWidth(200);
    mFileEditor->setReadOnly(true);
    mFileEditor->setText(QString::fromStdString(handler->file()));

    mFileSelector = new QToolButton(this);
    mFileSelector->setToolTip("Browse SoundFonts");
    mFileSelector->setAutoRaise(true);
    mFileSelector->setIcon(QIcon(":/data/file.svg"));
    connect(mFileSelector, SIGNAL(clicked()), SLOT(onClick()));

    mGainEditor = new GainEditor(this);
    mGainEditor->setGain(handler->gain());
    connect(mGainEditor, &GainEditor::gainChanged, this, &SoundFontEditor::updateGain);

    connect(Manager::instance->observer(), &Observer::messageHandled, this, &SoundFontEditor::onMessageHandled);

    QFormLayout* form = new QFormLayout;
    form->setMargin(0);
    form->addRow("File", make_hbox(spacing_tag{0}, mFileSelector, mFileEditor));
    form->addRow("Gain", mGainEditor);
    setLayout(form);
}

HandlerView::Parameters SoundFontEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("file", QString::fromStdString, static_cast<SoundFontHandler*>(handler())->file(), result);
    SERIALIZE("gain", serial::serializeDouble, static_cast<SoundFontHandler*>(handler())->gain(), result);
    return result;
}

size_t SoundFontEditor::setParameter(const Parameter& parameter) {
    if (parameter.name == "file") {
        setFile(parameter.value);
        return 1;
    }
    UNSERIALIZE("gain", serial::parseDouble, setGain, parameter);
    return HandlerEditor::setParameter(parameter);
}

void SoundFontEditor::onClick() {
    QString file = context()->pathRetriever("soundfont")->getReadFile(this);
    if (!file.isNull())
        setFile(file);
}

void SoundFontEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (handler == this->handler() && message.event.family() == family_t::custom && message.event.get_custom_key() == "SoundFont.file") {
        QFileInfo fileInfo(QString::fromStdString(message.event.get_custom_value()));
        mFileEditor->setText(fileInfo.completeBaseName());
        mFileEditor->setToolTip(fileInfo.absoluteFilePath());
    }
}

void SoundFontEditor::setGain(double gain) {
    mGainEditor->setGain(gain);
}

void SoundFontEditor::updateGain(double gain) {
    handler()->send_message(SoundFontHandler::gain_event(gain));
}

void SoundFontEditor::setFile(const QString& file) {
    handler()->send_message(SoundFontHandler::file_event(file.toStdString()));
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
