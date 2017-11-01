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
#include "qcore/editors.h"

//==================
// SoundFontHandler
//==================

class SoundFontHandler : public Handler {

public:
    static Event gain_event(double gain); /*!< must be in range [0, 10] */
    static Event file_event(const std::string& file);

    explicit SoundFontHandler();

    double gain() const;
    std::string file() const;

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
    void setGain(double gain);
    void setFile(const QString& file);

private slots:
    void onClick();
    void updateFile();
    void updateGain(double gain);

private:
    SoundFontReceiver* mReceiver;
    QMovie* mLoadMovie;
    QLabel* mLoadLabel;
    QLineEdit* mFileEditor;
    QToolButton* mFileSelector;
    GainEditor* mGainEditor;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // HANDLERS_SOUNDFONTHANDLER_H
