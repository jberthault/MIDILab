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

#ifndef QHANDLERS_CHANNELMAPPER_H
#define QHANDLERS_CHANNELMAPPER_H

#include "handlers/channelmapper.h"
#include "qcore/core.h"

//===================
// MetaChannelMapper
//===================

class MetaChannelMapper : public OpenMetaHandler {

public:
    explicit MetaChannelMapper(QObject* parent);

    void setContent(HandlerProxy& proxy) override;

};

//=====================
// ChannelMapperEditor
//=====================

class ChannelMapperEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit ChannelMapperEditor();

    Handler* getHandler() override;

public slots:
    void updateMapper();
    void updateFromMapper();
    void resetMapper();

private:
    ChannelMapper mHandler;
    channel_map_t<channel_map_t<QCheckBox*>> mCheckBoxes;

};

#endif // QHANDLERS_CHANNELMAPPER_H
