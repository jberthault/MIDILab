/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2019 Julien Berthault

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
#include <QLabel>
#include <QToolButton>
#include <QButtonGroup>
#include "qhandlers/harmonica.h"

/// @todo print the holes numbers on the screen

static constexpr auto defaultChannels = channels_t::wrap(0);

//===========
// Harmonica
//===========

MetaHandler* makeMetaHarmonica(QObject* parent) {
    auto* meta = makeMetaInstrument(parent);
    meta->setIdentifier("Harmonica");
    meta->setDescription("Interactive layout based on a diatonic harmonica");
    meta->addParameter({"tonality", "tonality of the harmonica with the octave, the harmonica is tuned with the richter system", "C3", MetaHandler::MetaParameter::Visibility::basic});
    meta->setFactory(new OpenProxyFactory<Harmonica>);
    return meta;
}

const QMap<Harmonica::Index, int> Harmonica::defaultTuning = {
   // blow alterations                    blow          aspirate      aspirate alterations ...
                                          {{0, 0}, 0},  {{1, 0}, 2},  {{2, 0}, 2 - 1},
                                          {{0, 1}, 4},  {{1, 1}, 7},  {{2, 1}, 7 - 1},  {{3, 1}, 7 - 2},
                                          {{0, 2}, 7},  {{1, 2}, 11}, {{2, 2}, 11 - 1}, {{3, 2}, 11 - 2}, {{4, 2}, 11 - 3},
                                          {{0, 3}, 12}, {{1, 3}, 14}, {{2, 3}, 14 - 1},
                                          {{0, 4}, 16}, {{1, 4}, 17},
                                          {{0, 5}, 19}, {{1, 5}, 21}, {{2, 5}, 21 - 1},
                                          {{0, 6}, 24}, {{1, 6}, 23},
                       {{-1, 7}, 28 - 1}, {{0, 7}, 28}, {{1, 7}, 26},
                       {{-1, 8}, 31 - 1}, {{0, 8}, 31}, {{1, 8}, 29},
    {{-2, 9}, 36 - 2}, {{-1, 9}, 36 - 1}, {{0, 9}, 36}, {{1, 9}, 33},
};

Harmonica::Harmonica() : Instrument{Mode::io()} {

    mGroup = new QButtonGroup{this};
    connect(mGroup, SIGNAL(buttonPressed(QAbstractButton*)), SLOT(onPress(QAbstractButton*)));
    connect(mGroup, SIGNAL(buttonReleased(QAbstractButton*)), SLOT(onRelease(QAbstractButton*)));

    auto* layout = new QGridLayout;
    setLayout(layout);
    layout->setMargin(0);
    layout->setSpacing(0);

    mReversed = true;
    mTuning = defaultTuning;
    mOffset.first = 2;
    mOffset.second = 1;

    addElement(new QLabel{"+ Blow", this}, getTrueRow(0), 0);
    addElement(new QLabel{"- Aspirate", this}, getTrueRow(1), 0);

    build(-2, 9);
    for (int i=7 ; i < 10 ; i++)
        build(-1, i);
    for (int i=0 ; i < 10 ; i++) {
        build(0, i); // blow
        build(1, i); // aspirate
    }
    for (int i=0 ; i < 4 ; i++)
        build(2, i);
    build(2, 5);
    build(3, 1);
    build(3, 2);
    build(4, 2);

    setTonality(note_ns::C(3));

}

HandlerView::Parameters Harmonica::getParameters() const {
    auto result = Instrument::getParameters();
    SERIALIZE("tonality", serial::serializeNote, mTonality, result);
    return result;
}

size_t Harmonica::setParameter(const Parameter& parameter) {
    UNSERIALIZE("tonality", serial::parseNote, setTonality, parameter);
    return Instrument::setParameter(parameter);
}

void Harmonica::onPress(QAbstractButton* button) {
    const auto note = buttonNote(button);
    if (note && canGenerate())
        generateNoteOn(defaultChannels, note);
}

void Harmonica::onRelease(QAbstractButton* button) {
    const auto note = buttonNote(button);
    if (note && canGenerate())
        generateNoteOff(defaultChannels, note);
}

void Harmonica::addElement(QWidget* widget, int true_row, int true_col) {
    auto* grid = static_cast<QGridLayout*>(layout());
    grid->addWidget(widget, true_row, true_col);
}

int Harmonica::getTrueRow(int row) const {
    static const int interception = (-2) + (4); // min + max rows
    if (mReversed)
        row = interception - row;
    return row + mOffset.first;
}

int Harmonica::getTrueCol(int col) const {
    return col + mOffset.second;
}

void Harmonica::build(int row, int col) {
    auto* button = new QToolButton{this};
    mGroup->addButton(button);
    button->setFixedSize(30, 30);
    mButtons[Index{row, col}] = button;
    addElement(button, getTrueRow(row), getTrueCol(col));
}

Note Harmonica::buttonNote(int row, int col) {
    return buttonNote(mButtons[Index{row, col}]);
}

Note Harmonica::buttonNote(QAbstractButton* button) {
    return mButtonsNotes.value(button);
}

void Harmonica::setTonality(const Note& note) {
    mTonality = note;
    mForwardNotes.clear();
    mButtonsNotes.clear();
    QMapIterator<Index, int> it{mTuning};
    while (it.hasNext()) {
        it.next();
        auto* button = mButtons[it.key()];
        const int offset = it.value();
        /// @todo change based on the scale
        const int code = mTonality.code() + offset;
        const auto note = Note::from_code(code);
        // register note <-> button (assuming button is not assigned)
        mForwardNotes.emplace(code, button);
        mButtonsNotes[button] = note;
        // change button text
        button->setText(QString::fromStdString(note.string()));
    }
}

void Harmonica::receiveNotesOff(channels_t /*channels*/) {
    QMapIterator<Index, QAbstractButton*> it{mButtons};
    while (it.hasNext()) {
        it.next();
        it.value()->setDown(false);
    }
}

void Harmonica::receiveNoteOn(channels_t /*channels*/, const Note& note) {
    auto it = mForwardNotes.equal_range(note.code());
    for ( ; it.first != it.second ; ++it.first) {
        auto* button = it.first->second;
        if (!button->isDown()) {
            button->setDown(true);
            break;
        }
    }
}

void Harmonica::receiveNoteOff(channels_t /*channels*/, const Note& note) {
    auto it = mForwardNotes.equal_range(note.code());
    for ( ; it.first != it.second ; ++it.first) {
        auto* button = it.first->second;
        button->setDown(false);
    }
}
