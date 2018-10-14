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

#ifndef QHANDLERS_RECORDER_H
#define QHANDLERS_RECORDER_H

#include <QLabel>
#include "handlers/sequencewriter.h"
#include "qcore/core.h"

//================
// RecorderEditor
//================

MetaHandler* makeMetaRecorder(QObject* parent);

class RecorderEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit RecorderEditor();

    Handler* getHandler() override;

protected:
    void updateContext(Context* context) override;

private slots:
    void setHandlerRecording(bool recording);

private:
    SequenceWriter mHandler;
    QLabel* mLabel;

};

#endif // QHANDLERS_RECORDER_H
