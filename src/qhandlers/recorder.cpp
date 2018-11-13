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

#include <QToolButton>
#include "qhandlers/recorder.h"
#include "qtools/misc.h"

//================
// RecorderEditor
//================

MetaHandler* makeMetaRecorder(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("Recorder");
    meta->setDescription("Creates sequences from incoming events that can be saved or played");
    meta->setFactory(new OpenProxyFactory<RecorderEditor>);
    return meta;
}

RecorderEditor::RecorderEditor() : HandlerEditor{} {

    QIcon recordIcon;
    recordIcon.addFile(":/data/light-red.svg", QSize{}, QIcon::Normal, QIcon::On);
    recordIcon.addFile(":/data/light-gray.svg", QSize{}, QIcon::Normal, QIcon::Off);

    auto* recordAction = makeAction(recordIcon, "Record", this);
    recordAction->setCheckable(true);
    connect(recordAction, &QAction::triggered, this, &RecorderEditor::setHandlerRecording);

    auto* recordButton = new QToolButton{this};
    recordButton->setAutoRaise(true);
    recordButton->setDefaultAction(recordAction);

    mLabel = new QLabel{this};
    mLabel->setText("");

    setLayout(make_hbox(margin_tag{0}, recordButton, mLabel));
}

Handler* RecorderEditor::getHandler() {
    return &mHandler;
}

void RecorderEditor::updateContext(Context* context) {
    context->quickToolBar()->addActions(actions());
}

void RecorderEditor::setHandlerRecording(bool recording) {
    if (recording) {
        mHandler.start_recording();
        mLabel->setText("Recording ...");
    } else {
        mHandler.stop_recording();
        mLabel->setText("");
    }
}
