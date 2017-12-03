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

#ifndef HANDLERS_SOUNDFONTHANDLER_H
#define HANDLERS_SOUNDFONTHANDLER_H

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <QLineEdit>
#include <QToolButton>
#include <QMovie>
#include <QGroupBox>
#include <boost/optional.hpp>
#include "qcore/editors.h"

//==================
// SoundFontHandler
//==================

class SoundFontHandler : public Handler {

public:

    struct reverb_type {
        double roomsize;
        double damp;
        double level;
        double width;
    };

    using optional_reverb_type = boost::optional<reverb_type>;

    static Event gain_event(double gain); /*!< must be in range [0, 10] */
    static Event file_event(const std::string& file);
    static Event reverb_event(const optional_reverb_type& reverb);

    explicit SoundFontHandler();

    double gain() const;
    std::string file() const;
    optional_reverb_type reverb() const;

    families_t handled_families() const override;
    result_type handle_message(const Message& message) override;
    result_type on_open(state_type state) override;
    result_type on_close(state_type state) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;

};

//===============
// MetaSoundFont
//===============

class MetaSoundFont : public MetaHandler {

public:
    explicit MetaSoundFont(QObject* parent);

    Instance instantiate() override;

};

//===================
// SoundFontReceiver
//===================

class SoundFontReceiver : public CustomReceiver {

    Q_OBJECT

public:
    explicit SoundFontReceiver(QObject* parent);

    result_type receive_message(Handler* target, const Message& message) final;

signals:
    void fileHandled();

};

//============
// GainEditor
//============

class GainEditor : public QWidget {

    Q_OBJECT

public:
    explicit GainEditor(QWidget* parent);

public slots:
    void setGain(double gain);

protected slots:
    void onMove(qreal ratio);
    void updateText(qreal ratio);

signals:
    void gainChanged(double gain);

private:
    SimpleSlider* mSlider;

};

// =============
// ReverbEditor
// =============

class ReverbEditor : public QGroupBox {

    Q_OBJECT

public:
    explicit ReverbEditor(QWidget* parent);

    bool isActive() const;
    SoundFontHandler::optional_reverb_type reverb() const;
    SoundFontHandler::reverb_type rawReverb() const;

public slots:
    void setActive(bool active);
    void setRoomSize(double value);
    void setDamp(double value);
    void setLevel(double value);
    void setWidth(double value);
    void setReverb(const SoundFontHandler::optional_reverb_type& reverb);

signals:
    void reverbChanged(SoundFontHandler::optional_reverb_type reverb);

private slots:
    void onRoomsizeChanged(qreal ratio);
    void onRoomsizeMoved(qreal ratio);
    void onDampChanged(qreal ratio);
    void onDampMoved(qreal ratio);
    void onLevelChanged(qreal ratio);
    void onLevelMoved(qreal ratio);
    void onWidthChanged(qreal ratio);
    void onWidthMoved(qreal ratio);
    void onToggle(bool activated);

private:
    SimpleSlider* mRoomsizeSlider;
    SimpleSlider* mDampSlider;
    SimpleSlider* mLevelSlider;
    SimpleSlider* mWidthSlider;
    SoundFontHandler::reverb_type mReverb;

};

//=================
// SoundFontEditor
//=================

class SoundFontEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit SoundFontEditor(SoundFontHandler* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

public slots:
    void setFile(const QString& file);
    void setGain(double gain);
    void setReverb(const SoundFontHandler::optional_reverb_type& reverb);

private slots:
    void onClick();
    void updateFile();
    void sendGain(double gain);
    void sendReverb(const SoundFontHandler::optional_reverb_type& reverb);

private:
    SoundFontHandler* mHandler;
    SoundFontReceiver* mReceiver;
    QMovie* mLoadMovie;
    QLabel* mLoadLabel;
    QLineEdit* mFileEditor;
    QToolButton* mFileSelector;
    GainEditor* mGainEditor;
    ReverbEditor* mReverbEditor;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // HANDLERS_SOUNDFONTHANDLER_H
