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

#ifndef HANDLERS_WHEEL_H
#define HANDLERS_WHEEL_H

#include <QDialog>
#include "qcore/editors.h"
#include "qtools/misc.h"

//===========
// MetaWheel
//===========

class MetaWheel : public MetaGraphicalHandler {

public:
    MetaWheel(QObject* parent);

};

//===============
// AbstractWheel
//===============

class AbstractWheel : public GraphicalHandler {

    Q_OBJECT

public:
    explicit AbstractWheel(mode_type mode, const QString& name, QWidget* parent);

    ChannelsSlider* slider();

    QMap<QString, QString> getParameters() const override;
    size_t setParameter(const QString& key, const QString& value) override;

    result_type on_close(state_type state) override;

protected:
    void prepare(qreal defaultRatio);
    void updateContext(Context* context) override;

protected slots:
    virtual void onMove(channels_t channels, qreal ratio) = 0;
    virtual void updateText(channels_t channels) = 0;

private slots:
    void onValueMove(channels_t channels, qreal ratio);

private:
    ChannelsSlider* mSlider;

};

//=====================
// MetaControllerWheel
//=====================

class MetaControllerWheel : public MetaWheel {

public:
    explicit MetaControllerWheel(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;

};

//=================
// ControllerWheel
//=================

class ControllerWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit ControllerWheel(const QString& name, QWidget* parent);

    byte_t controller() const;
    void setController(byte_t controller);

    QMap<QString, QString> getParameters() const override;
    size_t setParameter(const QString& key, const QString& value) override;

    family_t handled_families() const override;
    result_type handle_message(const Message& message) override;

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

    void onControlChange();

private:
    void receiveController(channels_t channels, byte_t controller, byte_t value);
    void resetController(channels_t channels);

private:
    QComboBox* mControllerBox;
    byte_t mController;
    std::array<channel_map_t<byte_t>, 0x80> mValues;
    std::array<byte_t, 0x80> mDefaultValues;

};

//================
// MetaPitchWheel
//================

class MetaPitchWheel : public MetaWheel {

public:
    explicit MetaPitchWheel(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;

};

//===========
// PitchWeel
//===========

class PitchWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit PitchWheel(const QString& name, QWidget* parent);

    family_t handled_families() const override;
    result_type handle_message(const Message& message) override;
    result_type on_close(state_type state) override;

protected slots:
    void onPress(channels_t channels);
    void onRelease(channels_t channels);
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;
    void onTypeChange(int index);

private:
    bool rangeDisplayed() const;
    void generateRegisteredParameter(channels_t channels, uint16_t value);
    void updatePitchRangeText(channels_t channels);
    void updatePitchValueText(channels_t channels);
    void receiveCoarseRPN(channels_t channels, byte_t byte);
    void receiveFineRPN(channels_t channels, byte_t byte);
    void receivePitchRange(channels_t channels, byte_t semitones); /*!< maximum pitch displayed, the minimum is the opposite (default is 2). */
    void receivePitchValue(channels_t channels, uint16_t value);
    void resetPitch(channels_t channels);

private:
    QComboBox* mTypeBox;
    channel_map_t<uint16_t> mRegisteredParameters;
    channel_map_t<byte_t> mPitchRanges;
    channel_map_t<uint16_t> mPitchValues;

};

//==================
// MetaProgramWheel
//==================

class MetaProgramWheel : public MetaWheel {

public:
    explicit MetaProgramWheel(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;
};

//==============
// ProgramWheel
//==============

class ProgramWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit ProgramWheel(const QString& name, QWidget* parent);

    family_t handled_families() const override;
    result_type handle_message(const Message& message) override;

    void setProgramChange(channels_t channels, byte_t program);

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

private:
    channel_map_t<byte_t> mPrograms;

};

//==================
// MetaVolume1Wheel
//==================

class MetaVolume1Wheel : public MetaWheel {

public:
    explicit MetaVolume1Wheel(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;

};

//==============
// Volume1Wheel
//==============

class Volume1Wheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit Volume1Wheel(const QString& name, QWidget* parent);

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

};

//==================
// MetaVolume2Wheel
//==================

class MetaVolume2Wheel : public MetaWheel {

public:
    explicit MetaVolume2Wheel(QObject* parent);

    instance_type instantiate(const QString& name, QWidget* parent) override;

};

//==============
// Volume2Wheel
//==============

class Volume2Wheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit Volume2Wheel(const QString& name, QWidget* parent);

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

};

#endif // HANDLERS_WHEEL_H
