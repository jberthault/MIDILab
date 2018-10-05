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

#ifndef QHANDLERS_SOUNDFONT_H
#define QHANDLERS_SOUNDFONT_H

#ifdef MIDILAB_FLUIDSYNTH_VERSION

#include <QLineEdit>
#include <QToolButton>
#include <QMovie>
#include <QGroupBox>
#include "handlers/soundfont.h"
#include "qhandlers/common.h"

//===============
// MetaSoundFont
//===============

class MetaSoundFont : public OpenMetaHandler {

public:
    explicit MetaSoundFont(QObject* parent);

    void setContent(HandlerProxy& proxy) override;

};

//======================
// SoundFontInterceptor
//======================

class SoundFontInterceptor : public ObservableInterceptor {

    Q_OBJECT

public:
    using ObservableInterceptor::ObservableInterceptor;

    void seize_messages(Handler* target, const Messages& messages) final;

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

    double gain() const;

public slots:
    void setGain(double gain);

signals:
    void gainChanged(double gain);

private:
    ExpSlider* mSlider;

};

// =============
// ReverbEditor
// =============

class ReverbEditor : public FoldableGroupBox {

    Q_OBJECT

public:
    explicit ReverbEditor(QWidget* parent);

    bool activated() const;
    double roomSize() const;
    double damp() const;
    double level() const;
    double width() const;

public slots:
    void setActivated(bool value);
    void setRoomSize(double value);
    void setDamp(double value);
    void setLevel(double value);
    void setWidth(double value);

signals:
    void activatedChanged(bool value);
    void roomSizeChanged(double value);
    void dampChanged(double value);
    void levelChanged(double value);
    void widthChanged(double value);

private:
    ContinuousSlider* mRoomsizeSlider;
    ContinuousSlider* mDampSlider;
    ContinuousSlider* mLevelSlider;
    ExpSlider* mWidthSlider;

};

//==============
// ChorusEditor
//==============

class ChorusEditor : public FoldableGroupBox {

    Q_OBJECT

public:
    explicit ChorusEditor(QWidget* parent);

    bool activated() const;
    int type() const;
    int nr() const;
    double level() const;
    double speed() const;
    double depth() const;

public slots:
    void setActivated(bool value);
    void setType(int value);
    void setNr(int value);
    void setLevel(double value);
    void setSpeed(double value);
    void setDepth(double value);

signals:
    void activatedChanged(bool value);
    void typeChanged(int value);
    void nrChanged(int value);
    void levelChanged(double value);
    void speedChanged(double value);
    void depthChanged(double value);

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

    Handler* getHandler() override;

public slots:
    void setFile(const QString& file);

private slots:
    void onClick();
    void updateFile();

private:
    SoundFontHandler mHandler;
    SoundFontInterceptor* mInterceptor;
    QMovie* mLoadMovie;
    QLabel* mLoadLabel;
    QLineEdit* mFileEditor;
    GainEditor* mGainEditor;
    ReverbEditor* mReverbEditor;
    ChorusEditor* mChorusEditor;

};

#endif // MIDILAB_FLUIDSYNTH_VERSION

#endif // QHANDLERS_SOUNDFONT_H
