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

#ifndef HANDLERS_RECORDER_H
#define HANDLERS_RECORDER_H

#include <QPushButton>
#include <QLabel>
#include "qcore/core.h"
#include "handlers/sequencehandlers.h"

//==============
// MetaRecorder
//==============

class MetaRecorder : public MetaHandler {

public:
    MetaRecorder(QObject* parent);

    Instance instantiate() override;

};

//================
// RecorderEditor
//================

class RecorderEditor : public HandlerEditor {

    Q_OBJECT

public:
    explicit RecorderEditor(SequenceWriter* handler);

public slots:
    void setRecording(bool recording);

private slots:
    void setHandlerRecording(bool recording);
    void startRecording();
    void stopRecording();

private:
    SequenceWriter* mWriter;
    QPushButton* mRecordButton;
    QLabel* mLabel;

};

#endif // HANDLERS_RECORDER_H
