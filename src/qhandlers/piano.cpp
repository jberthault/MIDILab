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

#include <QHelpEvent>
#include <QToolTip>
#include "qhandlers/piano.h"

//==========
// PianoKey
//==========

PianoKey::PianoKey(const Note& note, Piano* parent) :
    QWidget(parent), mNote(note), mChannels(), mParent(parent) {

    setToolTip(QString::fromStdString(note.string()));
}

void PianoKey::setChannels(channels_t channels) {
    if (mChannels != channels) {
        mChannels = channels;
        update();
    }
}

void PianoKey::activate(channels_t channels) {
    setChannels(mChannels | channels);
}

void PianoKey::deactivate(channels_t channels) {
    setChannels(mChannels & ~channels);
}

const Note& PianoKey::note() const {
    return mNote;
}

bool PianoKey::isBlack() const {
    return mNote.is_black();
}

void PianoKey::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setPen(QColor("#444")); // border color
    painter.setRenderHint(QPainter::Antialiasing); // rendering
    // coloration
    ChannelEditor* editor = mParent->channelEditor();
    if (mChannels && editor)
        painter.setBrush(editor->brush(mChannels));
    else
        painter.setBrush(QBrush(isBlack() ? Qt::black : Qt::white));
    // border radius
    painter.drawRoundedRect(rect(), 50, 5, Qt::RelativeSize);
}

//=============
// PianoLayout
//=============

namespace {

constexpr int whiteHeightForWidth(double width) {
    return decay_value<int>(PianoKey::whiteRatio * width);
}

}

PianoLayout::PianoLayout(QWidget* parent) : QLayout(parent), mFirstBlack(false), mLastBlack(false) {

}

PianoLayout::~PianoLayout() {
    while (QLayoutItem* item = takeAt(0))
        delete item;
}

void PianoLayout::addKey(PianoKey* key) {
    mLastBlack = key->isBlack();
    if (mLastBlack) {
        key->raise();
        mBlack.push_back({new QWidgetItem(key), mWhite.size()});
        if (mWhite.empty())
            mFirstBlack = true;
    } else {
        key->lower();
        mWhite.push_back(new QWidgetItem(key));
    }
}

void PianoLayout::addItem(QLayoutItem*) {
    TRACE_DEBUG("Can't add item for this layout");
}

Qt::Orientations PianoLayout::expandingDirections() const {
    return Qt::Horizontal;
}

QLayoutItem* PianoLayout::itemAt(int index) const {
    if (index >= 0) {
        if (index < mWhite.size())
            return mWhite.at(index);
        if (index < mWhite.size() + mBlack.size())
            return mBlack.at(index - mWhite.size()).first;
    }
    return nullptr;
}

QLayoutItem* PianoLayout::takeAt(int index) {
    if (index >= 0) {
        if (index < mWhite.size())
            return mWhite.takeAt(index);
        if (index < mWhite.size() + mBlack.size())
            return mBlack.takeAt(index-mWhite.size()).first;
    }
    return nullptr;
}

int PianoLayout::count() const {
    return mWhite.size() + mBlack.size();
}

void PianoLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);

    double blackBounds = 0;
    if (mFirstBlack)
        blackBounds += .5;
    if (mLastBlack)
        blackBounds += .5;

    double count = mWhite.size() + blackBounds;
    if (count <= 0.5)
        return;

    // compute size
    int whiteWidth = (int)(rect.width() / count);
    int whiteHeight = qMin(rect.height(), whiteHeightForWidth(whiteWidth));
    int blackWidth = decay_value<int>(PianoKey::blackWidthRatio * whiteWidth);
    int blackHeight = decay_value<int>(PianoKey::blackHeightRatio * whiteHeight);
    // compute offset
    double totalWidth = mWhite.size() * whiteWidth + blackBounds;
    QPoint whiteOffset = rect.topLeft() + QPoint((rect.width() - (int)totalWidth)/2, (rect.height() - whiteHeight)/2);
    // update white keys position
    QRect whiteRect(whiteOffset, QSize(whiteWidth, whiteHeight));
    for (QLayoutItem* item : mWhite) {
        item->setGeometry(whiteRect);
        whiteRect.moveLeft(whiteRect.left() + whiteWidth);
    }
    // update black keys position
    QRect blackRect(whiteOffset, QSize(blackWidth, blackHeight));
    for (const BlackItem& pair : mBlack) {
        blackRect.moveLeft(whiteOffset.x() + whiteWidth * pair.second - blackWidth / 2);
        pair.first->setGeometry(blackRect);
    }
}

bool PianoLayout::hasHeightForWidth() const {
    return true;
}

int PianoLayout::heightForWidth(int width) const {
    return mWhite.empty() ? 0 : whiteHeightForWidth((double)width / mWhite.size());
}

QSize PianoLayout::sizeHint() const {
    return {600, whiteHeightForWidth(600. / 52)};
}

//=======
// Piano
//=======

MetaHandler* makeMetaPiano(QObject* parent) {
    auto* meta = makeMetaInstrument(parent);
    meta->setIdentifier("Piano");
    meta->setDescription("Interactive Piano Keyboard");
    meta->addParameter("range", ":NoteRange", "closed range \"<first_note>:<last_note>\" of notes composing the keyboard", "A0:C8");
    meta->setFactory(new OpenProxyFactory<Piano>);
    return meta;
}

Piano::Piano() : Instrument{Mode::io()} {
    mKeys.fill(nullptr);
    buildKeys();
}

HandlerView::Parameters Piano::getParameters() const {
    auto result = Instrument::getParameters();
    SERIALIZE("range", serial::serializeRange, mRange, result);
    return result;
}

size_t Piano::setParameter(const Parameter& parameter) {
    UNSERIALIZE("range", serial::parseRange, setRange, parameter);
    return Instrument::setParameter(parameter);
}

const std::pair<Note, Note>& Piano::range() const {
    return mRange;
}

void Piano::setRange(const std::pair<Note, Note>& range) {
    if (range != mRange && 0 <= range.first.code() && range.second.code() < 0x80) {
        mRange = range;
        clearKeys();
        buildKeys();
    }
}

void Piano::clearKeys() {
    for (PianoKey* key : mKeys)
        if (key != nullptr)
            key->deleteLater();
    mKeys.fill(nullptr);
    mActiveKey = nullptr;
    delete layout();
}

void Piano::buildKeys() {
    auto* pianoLayout = new PianoLayout{nullptr};
    pianoLayout->setMargin(0);
    for (int code = mRange.first.code() ; code <= mRange.second.code() ; code++) {
        auto* key = new PianoKey{Note::from_code(code), this};
        mKeys[code] = key;
        pianoLayout->addKey(key);
    }
    setLayout(pianoLayout);
}

void Piano::receiveNotesOff(channels_t channels) {
    for (PianoKey* key : mKeys)
        if (key != nullptr)
            key->deactivate(channels);
}

void Piano::receiveNoteOn(channels_t channels, const Note& note) {
    if (auto* key = mKeys[note.code()])
        key->activate(channels);
}

void Piano::receiveNoteOff(channels_t channels, const Note& note) {
    if (auto* key = mKeys[note.code()])
        key->deactivate(channels);
}

bool Piano::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto helpEvent = static_cast<QHelpEvent*>(event);
        auto key = qobject_cast<PianoKey*>(childAt(helpEvent->pos()));
        if (key) {
            QToolTip::showText(helpEvent->globalPos(), key->toolTip());
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return Instrument::event(event);
}

void Piano::enterEvent(QEvent*) {
    if (canGenerate())
        setCursor(Qt::PointingHandCursor);
}

void Piano::leaveEvent(QEvent*) {
    unsetCursor();
}

void Piano::mouseDoubleClickEvent(QMouseEvent* event) {
    mousePressEvent(event);
}

void Piano::mousePressEvent(QMouseEvent* event) {
    if (canGenerate()) {
        auto key = qobject_cast<PianoKey*>(childAt(event->pos()));
        generateKeyOn(key, event->button());
        mActiveKey = key;
    }
}

void Piano::mouseReleaseEvent(QMouseEvent* event) {
    if (canGenerate())
        generateKeyOff(qobject_cast<PianoKey*>(childAt(event->pos())), event->button());
}

void Piano::mouseMoveEvent(QMouseEvent* event) {
    if (canGenerate()) {
        auto key = qobject_cast<PianoKey*>(childAt(event->pos()));
        if (mActiveKey != key) {
            generateKeyOff(mActiveKey, event->buttons());
            generateKeyOn(key, event->buttons());
            mActiveKey = key;
        }
    }
}

void Piano::generateKeyOn(PianoKey* key, Qt::MouseButtons buttons) {
    if (key) {
        if (auto channels = channelsFromButtons(buttons)) {
            generateNoteOn(channels, key->note());
            key->activate(channels);
        }
    }
}

void Piano::generateKeyOff(PianoKey* key, Qt::MouseButtons buttons) {
    if (key) {
        if (auto channels = channelsFromButtons(buttons)) {
            generateNoteOff(channels, key->note());
            key->deactivate(channels);
        }
    }
}
