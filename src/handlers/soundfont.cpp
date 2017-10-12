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
#include <QFileDialog>
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
        Handler::result_type result;
        result |= push(Event::controller(all_channels, all_controllers_off_controller));
        result |= push(Event::controller(all_channels, all_sound_off_controller));
        // result |= push(Event::controller(all_channels, volume_coarse_controller, 100));
        return result;
    }

    Handler::result_type push(const Event& event) {
        switch (+event.family()) {
        case note_off_family: for (channel_t channel : event.channels()) fluid_synth_noteoff(synth, channel, event.at(1)); return success_result;
        case note_on_family: for (channel_t channel : event.channels()) fluid_synth_noteon(synth, channel, event.at(1), event.at(2)); return success_result;
        case program_change_family: for (channel_t channel : event.channels()) fluid_synth_program_change(synth, channel, event.at(1)); return success_result;
        case controller_family: for (channel_t channel : event.channels()) fluid_synth_cc(synth, channel, event.at(1), event.at(2)); return success_result;
        case channel_pressure_family: for (channel_t channel : event.channels()) fluid_synth_channel_pressure(synth, channel, event.at(1)); return success_result;
        case pitch_wheel_family: for (channel_t channel : event.channels()) fluid_synth_pitch_bend(synth, channel, event.get_14bits()); return success_result;
        case reset_family: return reset_system();
        case sysex_family: {
            /// @note master volume does not seem to be handled correctly
            std::vector<char> sys_ex_data(event.begin()+1, event.end()-1);
            fluid_synth_sysex(synth, sys_ex_data.data(), sys_ex_data.size(), nullptr, nullptr, nullptr, 0);
            return success_result;
        }
        case custom_family: {
            auto k = event.get_custom_key();
            auto v = event.get_custom_value();
            if (k == "SoundFont.gain") {
                fluid_synth_set_gain(synth, (float)unmarshall<double>(v));
                return success_result;
            } else if (k == "SoundFont.file") {
                if (fluid_synth_sfload(synth, v.c_str(), 1) == -1)
                    return fail_result;
                file = std::move(v);
                return success_result;
            }
            break;
        }
        }
        return unhandled_result;
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

family_t SoundFontHandler::handled_families() const {
    return (custom_family | voice_families | reset_family) & ~aftertouch_family;
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

QMap<QString, QString> SoundFontEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    result["file"] = QString::fromStdString(static_cast<SoundFontHandler*>(handler())->file());
    result["gain"] = serial::serializeDouble(static_cast<SoundFontHandler*>(handler())->gain());
    return result;
}

size_t SoundFontEditor::setParameter(const QString& key, const QString& value) {
    if (key == "file") {
        setFile(value);
        return 1;
    } else if (key == "gain") {
        UNSERIALIZE(setGain, serial::parseDouble, value);
    }
    return HandlerEditor::setParameter(key, value);
}

void SoundFontEditor::onClick() {
    QString file = QFileDialog::getOpenFileName(this, "Select a SoundFont", QString(), "SoundFont (*.sf2)");
    if (!file.isNull())
        setFile(file);
}

void SoundFontEditor::onMessageHandled(Handler* handler, const Message& message) {
    if (handler == this->handler() && message.event.is(custom_family) && message.event.get_custom_key() == "SoundFont.file") {
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
