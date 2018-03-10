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

#ifndef QHANDLERS_SOUNDFONT_H
#define QHANDLERS_SOUNDFONT_H

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <QLineEdit>
#include <QToolButton>
#include <QMovie>
#include <QGroupBox>
#include "handlers/soundfont.h"
#include "qcore/editors.h"

//===============
// MetaSoundFont
//===============

class MetaSoundFont : public OpenMetaHandler {

public:
    explicit MetaSoundFont(QObject* parent);

    void setContent(HandlerProxy& proxy) override;

};

//=====================
// SoundFontInterceptor
//=====================

class SoundFontInterceptor : public ObservableInterceptor {

    Q_OBJECT

public:
    using ObservableInterceptor::ObservableInterceptor;

    Result seize_message(Handler* target, const Message& message) final;

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
    void notify();

signals:
    void gainChanged(double gain);

private:
    ExpSlider* mSlider;

};

// =============
// ReverbEditor
// =============

class ReverbEditor : public QGroupBox {

    Q_OBJECT

public:
    explicit ReverbEditor(QWidget* parent);

    SoundFontHandler::optional_reverb_type reverb() const;
    SoundFontHandler::reverb_type rawReverb() const;

public slots:
    void setRoomSize(double value);
    void setDamp(double value);
    void setLevel(double value);
    void setWidth(double value);
    void setReverb(const SoundFontHandler::optional_reverb_type& reverb);

signals:
    void reverbChanged(SoundFontHandler::optional_reverb_type reverb);

private slots:
    void notifyChecked();
    void notify();

private:
    ContinuousSlider* mRoomsizeSlider;
    ContinuousSlider* mDampSlider;
    ContinuousSlider* mLevelSlider;
    ExpSlider* mWidthSlider;

};

//==============
// ChorusEditor
//==============

class ChorusEditor : public QGroupBox {

    Q_OBJECT

public:
    explicit ChorusEditor(QWidget* parent);

    SoundFontHandler::optional_chorus_type chorus() const;
    SoundFontHandler::chorus_type rawChorus() const;

public slots:
    void setType(int value);
    void setNr(int value);
    void setLevel(double value);
    void setSpeed(double value);
    void setDepth(double value);
    void setChorus(const SoundFontHandler::optional_chorus_type& chorus);

signals:
    void chorusChanged(SoundFontHandler::optional_chorus_type chorus);

private slots:
    void notifyChecked();
    void notify();

private:
    QComboBox* mTypeBox;
    DiscreteSlider* mNrSlider;
    ExpSlider* mLevelSlider;
    ContinuousSlider* mSpeedSlider;
    ContinuousSlider* mDepthSlider;

};

//=================
// SoundFontEditor
//=================

class SoundFontEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit SoundFontEditor();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

    Handler* getHandler() const override;

public slots:
    void setFile(const QString& file);
    void setGain(double gain);
    void setReverb(const SoundFontHandler::optional_reverb_type& reverb);
    void setChorus(const SoundFontHandler::optional_chorus_type& chorus);

private slots:
    void onClick();
    void updateFile();
    void sendGain(double gain);
    void sendReverb(const SoundFontHandler::optional_reverb_type& reverb);
    void sendChorus(const SoundFontHandler::optional_chorus_type& chorus);

private:
    std::unique_ptr<SoundFontHandler> mHandler;
    SoundFontInterceptor* mInterceptor;
    QMovie* mLoadMovie;
    QLabel* mLoadLabel;
    QLineEdit* mFileEditor;
    QToolButton* mFileSelector;
    GainEditor* mGainEditor;
    ReverbEditor* mReverbEditor;
    ChorusEditor* mChorusEditor;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // QHANDLERS_SOUNDFONT_H
