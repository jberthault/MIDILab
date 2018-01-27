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

#include "qhandlers/recorder.h"
#include "qtools/misc.h"

//==============
// MetaRecorder
//==============

MetaRecorder::MetaRecorder(QObject* parent) : MetaHandler(parent) {
    setIdentifier("Recorder");
}

MetaHandler::Instance MetaRecorder::instantiate() {
    auto handler = new SequenceWriter;
    return Instance(handler, new RecorderEditor(handler));
}

//================
// RecorderEditor
//================

RecorderEditor::RecorderEditor(SequenceWriter* handler) : HandlerEditor(), mWriter(handler) {

    mRecordButton = new QPushButton("Status", this);
    QIcon icon;
    icon.addFile(":/data/light-red.svg", QSize(), QIcon::Normal, QIcon::On);
    icon.addFile(":/data/light-gray.svg", QSize(), QIcon::Normal, QIcon::Off);
    mRecordButton->setIcon(icon);
    mRecordButton->setCheckable(true);
    mRecordButton->setFlat(true);
    connect(mRecordButton, SIGNAL(clicked(bool)), SLOT(setHandlerRecording(bool)));

    mLabel = new QLabel(this);
    mLabel->setText("");

    setLayout(make_vbox(mRecordButton, mLabel));
}

void RecorderEditor::setRecording(bool recording) {
    mRecordButton->setChecked(recording);
}

void RecorderEditor::setHandlerRecording(bool recording) {
    if (recording)
        startRecording();
    else
        stopRecording();
}

void RecorderEditor::startRecording() {
    mWriter->start_recording();
    mLabel->setText("Recording ...");
}

void RecorderEditor::stopRecording() {
    mWriter->stop_recording();
    mLabel->setText("");
}
