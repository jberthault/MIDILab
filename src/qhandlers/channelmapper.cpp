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

#include <QGridLayout>
#include <QPushButton>
#include "qhandlers/channelmapper.h"
#include "qcore/editors.h"
#include "qtools/misc.h"

namespace {

static const int offset = 4; // color, number, checkbox, line

static const QSize labelSize {16, 16};

}

//=====================
// ChannelMapperEditor
//=====================

MetaHandler* makeMetaChannelMapper(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("ChannelMapper");
    meta->setFactory(new OpenProxyFactory<ChannelMapperEditor>);
    return meta;
}

ChannelMapperEditor::ChannelMapperEditor() : HandlerEditor{} {

    auto* checkBoxLayout = new QGridLayout;
    checkBoxLayout->setMargin(0);
    checkBoxLayout->setSpacing(0);

    channel_map_t<TriState*> in, out;
    makeHeader(Qt::Vertical, checkBoxLayout, in, mVerticalColorBoxes);
    makeHeader(Qt::Horizontal, checkBoxLayout, out, mHorizontalColorBoxes);
    for (channel_t ic=0 ; ic < channels_t::capacity() ; ++ic) {
        for (channel_t oc=0 ; oc < channels_t::capacity() ; ++oc) {
            auto* check = new QCheckBox{this};
            mCheckBoxes[ic][oc] = check;
            in[ic]->addCheckBox(check);
            out[oc]->addCheckBox(check);
            checkBoxLayout->addWidget(check, offset + ic, offset + oc);
        }
    }

    auto* dgroup = new TriState{this};
    checkBoxLayout->addWidget(dgroup, 2, 2);
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        dgroup->addCheckBox(mCheckBoxes[c][c]);

    auto* applyButton = new QPushButton{"Apply", this};
    connect(applyButton, &QPushButton::clicked, this, &ChannelMapperEditor::updateMapper);

    auto* resetButton = new QPushButton{"Reset", this};
    connect(resetButton, &QPushButton::clicked, this, &ChannelMapperEditor::resetMapper);

    auto* discardButton = new QPushButton{"Discard", this};
    connect(discardButton, &QPushButton::clicked, this, &ChannelMapperEditor::updateFromMapper);

    setLayout(make_vbox(margin_tag{0}, checkBoxLayout, make_hbox(stretch_tag{}, applyButton, resetButton, discardButton)));
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    updateFromMapper();
}

Handler* ChannelMapperEditor::getHandler() {
    return &mHandler;
}

void ChannelMapperEditor::updateContext(Context* context) {
    if (auto* editor = context->channelEditor()) {
        connect(editor, &ChannelEditor::colorChanged, this, &ChannelMapperEditor::updateColor);
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            updateColor(c, editor->color(c));
    }
}

void ChannelMapperEditor::updateColor(channel_t channel, const QColor& color) {
    const auto styleSheet = QString{"background : %1"}.arg(color.name());
    mVerticalColorBoxes[channel]->setStyleSheet(styleSheet);
    mHorizontalColorBoxes[channel]->setStyleSheet(styleSheet);
}

void ChannelMapperEditor::updateMapper() {
    channel_map_t<channels_t> mapping;
    for (channel_t ic=0 ; ic < channels_t::capacity() ; ++ic)
        for (channel_t oc=0 ; oc < channels_t::capacity() ; ++oc)
            if (mCheckBoxes[ic][oc]->isChecked())
                mapping[ic].set(oc);
    mHandler.set_mapping(mapping);
}

void ChannelMapperEditor::updateFromMapper() {
    const auto mapping = mHandler.mapping();
    for (channel_t ic=0 ; ic < channels_t::capacity() ; ++ic)
        for (channel_t oc=0 ; oc < channels_t::capacity() ; ++oc)
            mCheckBoxes[ic][oc]->setChecked(mapping[ic].test(oc));
}

void ChannelMapperEditor::resetMapper() {
    mHandler.reset_mapping();
    updateFromMapper();
}

void ChannelMapperEditor::makeHeader(Qt::Orientation orientation, QGridLayout* gridLayout, channel_map_t<TriState*>& groups, channel_map_t<QWidget*>& colors) {
    const int di = orientation == Qt::Vertical ? 0 : 1;
    const int dj = 1 - di;
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c) {
        // color
        auto* colorLabel = new QLabel{this};
        colors[c] = colorLabel;
        colorLabel->setFixedSize(labelSize);
        // number
        auto* numberLabel = new QLabel{QString::number(c), this};
        numberLabel->setFixedSize(labelSize);
        numberLabel->setAlignment(Qt::AlignCenter);
        // checkbox
        auto* triState = new TriState{this};
        groups[c] = triState;
        // layout
        gridLayout->addWidget(colorLabel , 0*di + (offset+c)*dj, 0*dj + (offset+c)*di);
        gridLayout->addWidget(numberLabel, 1*di + (offset+c)*dj, 1*dj + (offset+c)*di);
        gridLayout->addWidget(triState   , 2*di + (offset+c)*dj, 2*dj + (offset+c)*di);
    }
    // line
    auto* line = new QFrame{this};
    line->setFrameShadow(QFrame::Sunken);
    line->setFrameShape(orientation == Qt::Vertical ? QFrame::VLine : QFrame::HLine);
    // layout
    gridLayout->addWidget(line, 3*di + offset*dj, 3*dj + offset*di, 1*di - 1*dj, 1*dj - 1*di);
}
