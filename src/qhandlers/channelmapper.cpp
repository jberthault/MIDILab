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

#include <QGridLayout>
#include <QPushButton>
#include "qhandlers/channelmapper.h"
#include "qtools/misc.h"

//===================
// MetaChannelMapper
//===================

MetaChannelMapper::MetaChannelMapper(QObject* parent) : MetaHandler(parent) {
    setIdentifier("ChannelMapper");
}

MetaChannelMapper::Instance MetaChannelMapper::instantiate() {
    auto handler = new ChannelMapper;
    return Instance(handler, new ChannelMapperEditor(handler));
}

//=====================
// ChannelMapperEditor
//=====================

ChannelMapperEditor::ChannelMapperEditor(ChannelMapper* handler) : HandlerEditor(), mHandler(handler) {

    auto checkBoxLayout = new QGridLayout;
    checkBoxLayout->setMargin(0);
    checkBoxLayout->setSpacing(0);

    channel_map_t<TriState*> in, out;

    static const int offset = 3;

    for (channel_t c=0 ; c < 0x10 ; ++c) {
        TriState* inCheck = new TriState(this);
        TriState* outCheck = new TriState(this);
        QLabel* inLabel = new QLabel(QString::number(c), this);
        QLabel* outLabel = new QLabel(QString::number(c), this);
        checkBoxLayout->addWidget(inLabel, offset + c, 0);
        checkBoxLayout->addWidget(inCheck, offset + c, 1);
        checkBoxLayout->addWidget(outLabel, 0, offset + c);
        checkBoxLayout->addWidget(outCheck, 1, offset + c);
        in[c] = inCheck;
        out[c] = outCheck;
    }

    auto hline = new QFrame(this);
    auto vline = new QFrame(this);
    hline->setFrameShadow(QFrame::Sunken);
    vline->setFrameShadow(QFrame::Sunken);
    hline->setFrameShape(QFrame::HLine);
    vline->setFrameShape(QFrame::VLine);
    checkBoxLayout->addWidget(hline, 2, offset, 1, -1);
    checkBoxLayout->addWidget(vline, offset, 2, -1, 1);

    for (channel_t ic=0 ; ic < 0x10 ; ++ic) {
        for (channel_t oc=0 ; oc < 0x10 ; ++oc) {
            auto check = new QCheckBox(this);
            mCheckBoxes[ic][oc] = check;
            in[ic]->addCheckBox(check);
            out[oc]->addCheckBox(check);
            checkBoxLayout->addWidget(check, offset + ic, offset + oc);
        }
    }

    auto dgroup = new TriState(this);
    checkBoxLayout->addWidget(dgroup, 1, 1);
    for (channel_t c=0 ; c < 0x10 ; ++c)
        dgroup->addCheckBox(mCheckBoxes[c][c]);

    auto applyButton = new QPushButton("Apply", this);
    connect(applyButton, SIGNAL(clicked()), this, SLOT(updateMapper()));

    auto resetButton = new QPushButton("Reset", this);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(resetMapper()));

    auto discardButton = new QPushButton("Discard", this);
    connect(discardButton, SIGNAL(clicked()), this, SLOT(updateFromMapper()));

    setLayout(make_vbox(checkBoxLayout, stretch_tag{}, make_hbox(stretch_tag{}, applyButton, resetButton, discardButton)));

    updateFromMapper();
}

void ChannelMapperEditor::updateMapper() {
    channel_map_t<channels_t> mapping;
    for (channel_t ic=0 ; ic < 0x10 ; ++ic) {
        channels_t ocs;
        for (channel_t oc=0 ; oc < 0x10 ; ++oc)
            if (mCheckBoxes[ic][oc]->isChecked())
                ocs.set(oc);
        mapping[ic] = ocs;
    }
    mHandler->set_mapping(std::move(mapping));
}

void ChannelMapperEditor::updateFromMapper() {
    auto mapping = mHandler->mapping();
    for (channel_t ic=0 ; ic < 0x10 ; ++ic)
        for (channel_t oc=0 ; oc < 0x10 ; ++oc)
            mCheckBoxes[ic][oc]->setChecked(mapping[ic].contains(oc));
}

void ChannelMapperEditor::resetMapper() {
    mHandler->reset_mapping();
    updateFromMapper();
}
