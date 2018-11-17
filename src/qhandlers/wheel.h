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

#ifndef QHANDLERS_WHEEL_H
#define QHANDLERS_WHEEL_H

#include "qhandlers/common.h"

//===============
// AbstractWheel
//===============

MetaHandler* makeMetaWheel(QObject* parent);

class AbstractWheel : public GraphicalHandler {

    Q_OBJECT

public:
    explicit AbstractWheel(Mode mode);

    ChannelsSlider* slider();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

protected:
    Result handle_open(State state) override;
    Result handle_close(State state) override;
    void updateContext(Context* context) override;

protected slots:
    virtual void onMove(channels_t channels, qreal ratio) = 0;
    virtual void updateText(channels_t channels) = 0;

private slots:
    void onValueMove(channels_t channels, qreal ratio);

private:
    ChannelsSlider* mSlider;

};

//=================
// ControllerWheel
//=================

MetaHandler* makeMetaControllerWheel(QObject* parent);

class ControllerWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit ControllerWheel();

    byte_t controller() const;
    void setController(byte_t controller);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

protected:
    Result handle_close(State state) override;
    Result handle_message(const Message& message) override;
    families_t handled_families() const override;

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

    void onControlChange();

private:
    void resetAll();
    void setControllerValue(channels_t channels, byte_t controller, byte_t value);

    Result handleController(channels_t channels, byte_t controller, byte_t value);
    Result handleReset();

private:
    QComboBox* mControllerBox;
    byte_t mController;
    std::array<channel_map_t<byte_t>, 0x80> mValues;

};

//===========
// PitchWeel
//===========

MetaHandler* makeMetaPitchWheel(QObject* parent);

class PitchWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit PitchWheel();

protected:
    Result handle_close(State state) override;
    Result handle_message(const Message& message) override;
    families_t handled_families() const override;

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
    void resetAll();

    Result handleCoarseRPN(channels_t channels, byte_t byte);
    Result handleFineRPN(channels_t channels, byte_t byte);
    Result handleCoarseDataEntry(channels_t channels, byte_t byte);
    Result handlePitchValue(channels_t channels, uint16_t value);
    Result handleAllControllersOff(channels_t channels);
    Result handleReset();

private:
    QComboBox* mTypeBox;
    channel_map_t<uint16_t> mRegisteredParameters;
    channel_map_t<byte_t> mPitchRanges;
    channel_map_t<uint16_t> mPitchValues;

};

//==============
// ProgramWheel
//==============

MetaHandler* makeMetaProgramWheel(QObject* parent);

class ProgramWheel : public AbstractWheel {

    Q_OBJECT

public:
    explicit ProgramWheel();

    void setProgramChange(channels_t channels, byte_t program);

protected:
    Result handle_message(const Message& message) override;
    families_t handled_families() const override;

protected slots:
    void onMove(channels_t channels, qreal ratio) override;
    void updateText(channels_t channels) override;

private:
    channel_map_t<byte_t> mPrograms;

};

//=============
// VolumeWheel
//=============

MetaHandler* makeMetaVolumeWheel(QObject* parent);

class VolumeWheel : public GraphicalHandler {

    Q_OBJECT

public:
    explicit VolumeWheel();

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

protected:
    Result handle_open(State state) override;
    Result handle_close(State state) override;
    Result handle_message(const Message& message) override;
    families_t handled_families() const override;

private:
    RangedSlider<range_t<uint16_t>>* mSlider;
    bool mGenerateOnChange {true};

};

#endif // QHANDLERS_WHEEL_H
