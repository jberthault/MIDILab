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

#ifndef QHANDLERS_TRANSPOSER_H
#define QHANDLERS_TRANSPOSER_H

#include "handlers/transposer.h"
#include "qcore/editors.h"
#include "qtools/misc.h"

//================
// MetaTransposer
//================

class MetaTransposer : public MetaHandler {

public:
    explicit MetaTransposer(QObject* parent);

    Instance instantiate() override;

};

//==================
// TransposerEditor
//==================

class TransposerEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit TransposerEditor(Transposer* handler);

    Parameters getParameters() const override;
    size_t setParameter(const Parameter& parameter) override;

protected:
    void updateContext(Context* context) override;

private slots:
    void onMove(channels_t channels, qreal ratio);
    void updateText(channels_t channels);

private:
    Transposer* mHandler;
    ChannelsSlider* mSlider;
    channel_map_t<int> mKeys;

};

#endif // QHANDLERS_TRANSPOSER_H