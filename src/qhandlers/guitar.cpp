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

#include <QPainter>
#include <QToolTip>
#include <QHelpEvent>
#include <QMouseEvent>
#include "qhandlers/guitar.h"

namespace {

constexpr size_t fretCount = 25u;

constexpr std::array<double, fretCount> fretPositions = {0., 0.0748347, 0.145468, 0.212139, 0.275065, 0.334461, 0.390524, 0.44344, 0.493385, 0.540528, 0.585025, 0.627024, 0.666667, 0.704084, 0.739401, 0.772736, 0.8042, 0.833897, 0.861929, 0.888387, 0.91336, 0.936931, 0.959179, 0.980179, 1};

constexpr std::array<double, fretCount> fretCenters = {0, 0.0374174, 0.110151, 0.178804, 0.243602, 0.304763, 0.362492, 0.416982, 0.468413, 0.516957, 0.562777, 0.606024, 0.646845, 0.685376, 0.721742, 0.756069, 0.788468, 0.819049, 0.847913, 0.875158, 0.900873, 0.925145, 0.948055, 0.969679, 0.99009};

size_t nearestFretCenter(double ratio) {
    auto it = std::min_element(fretCenters.begin(), fretCenters.end(), [=](double lhs, double rhs) {
        return (lhs-ratio)*(lhs-ratio) < (rhs-ratio)*(rhs-ratio);
    });
    return std::distance(fretCenters.begin(), it);
}

constexpr double stringPosition(size_t i, size_t n) {
    return (i + .5) / n;
}

constexpr double singleMarkPositions[] = {fretCenters[3], fretCenters[5], fretCenters[7], fretCenters[9], fretCenters[15], fretCenters[17], fretCenters[19], fretCenters[21]};
constexpr double doubleMarkPositions[] = {fretCenters[12], fretCenters[24]};

const Guitar::Tuning defaultTuning = {note_ns::E(2), note_ns::A(2), note_ns::D(3), note_ns::G(3), note_ns::B(3), note_ns::E(4)};

}

//========
// Guitar
//========

MetaHandler* makeMetaGuitar(QObject* parent) {
    auto* meta = makeMetaInstrument(parent);
    meta->setIdentifier("Guitar");
    meta->setDescription("Interactive Guitar Fretboard");
    meta->addParameter({"tuning", "list of notes separated by ';' from lower string to higher string", serial::serializeNotes(defaultTuning), MetaHandler::MetaParameter::Visibility::basic});
    meta->addParameter({"capo", "capo position, no capo if 0", "0", MetaHandler::MetaParameter::Visibility::basic});
    meta->setFactory(new OpenProxyFactory<Guitar>);
    return meta;
}

Guitar::Guitar() : Instrument(Mode::io()), mTuning(defaultTuning), mCapo(0), mState(defaultTuning.size()), mActiveLocation(-1, 0), mBackground(":/data/wood.jpg") {
    clearNotes();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

HandlerView::Parameters Guitar::getParameters() const {
    auto result = Instrument::getParameters();
    SERIALIZE("tuning", serial::serializeNotes, mTuning, result);
    SERIALIZE("capo", serial::serializeNumber, mCapo, result);
    return result;
}

size_t Guitar::setParameter(const Parameter& parameter) {
    UNSERIALIZE("tuning", serial::parseNotes, setTuning, parameter);
    UNSERIALIZE("capo", serial::parseULong, setCapo, parameter);
    return Instrument::setParameter(parameter);
}

const Guitar::Tuning& Guitar::tuning() const {
    return mTuning;
}

void Guitar::setTuning(const Tuning& tuning) {
    if (!tuning.empty()) {
        mTuning = tuning;
        mState.resize(mTuning.size());
        mActiveLocation = {-1, 0};
        clearNotes();
        update();
    }
}

size_t Guitar::capo() const {
    return mCapo;
}

void Guitar::setCapo(size_t capo) {
    if (capo < fretCount) {
        mCapo = capo;
        mActiveLocation = {-1, 0};
        clearNotes();
        update();
    }
}

QSize Guitar::sizeHint() const {
    return {750, 75};
}

void Guitar::receiveNotesOff(channels_t channels) {
    for (auto& stringState : mState)
        channel_ns::clear(stringState, channels);
    update();
}

void Guitar::receiveNoteOn(channels_t channels, const Note& note) {

    struct Candidate { int occupied; Location loc; };

    std::vector<Candidate> candidates;
    candidates.reserve(mTuning.size());

    for (int i=0 ; i < (int)mTuning.size() ; ++i) {
        auto loc = fromNote(i, note);
        if (isValid(loc))
            candidates.push_back({channel_ns::contains(mState[i], channels), loc});
    }
    auto it = std::min_element(candidates.begin(), candidates.end(), [](const auto& c1, const auto& c2) {
        return std::tie(c1.occupied, c1.loc.second) < std::tie(c2.occupied, c2.loc.second);
    });
    if (it != candidates.end())
        activate(it->loc, channels);
}

void Guitar::receiveNoteOff(channels_t channels, const Note& note) {
    for (int i=0 ; i < (int)mTuning.size() ; ++i) {
        auto loc = fromNote(i, note);
        if (isValid(loc))
            deactivate(loc, channels);
    }
}

bool Guitar::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto helpEvent = static_cast<QHelpEvent*>(event);
        auto loc = locationAt(helpEvent->pos());
        if (isValid(loc)) {
            QToolTip::showText(helpEvent->globalPos(), QString::fromStdString(toNote(loc).string()));
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return Instrument::event(event);
}

void Guitar::enterEvent(QEvent*) {
    if (canGenerate())
        setCursor(Qt::PointingHandCursor);
}

void Guitar::leaveEvent(QEvent*) {
    unsetCursor();
}

void Guitar::mouseDoubleClickEvent(QMouseEvent* event) {
    mousePressEvent(event);
}

void Guitar::mousePressEvent(QMouseEvent* event) {
    if (canGenerate()) {
        auto loc = locationAt(event->pos());
        generateFretOn(loc, event->button());
        mActiveLocation = loc;
    }
}

void Guitar::mouseReleaseEvent(QMouseEvent* event) {
    if (canGenerate())
        generateFretOff(locationAt(event->pos()), event->button());
}

void Guitar::mouseMoveEvent(QMouseEvent* event) {
    if (canGenerate()) {
        auto loc = locationAt(event->pos());
        if (mActiveLocation != loc) {
            generateFretOff(mActiveLocation, event->buttons());
            generateFretOn(loc, event->buttons());
            mActiveLocation = loc;
        }
    }
}

void Guitar::paintEvent(QPaintEvent*) {

    const auto stringsCount = mTuning.size();
    const auto r = rect();
    const int h = r.height();
    const int markRadius = h / 18;
    const int channelRadius = h / (2*stringsCount);
    static const double capoRadius = 4.;
    static const double capoWidth = 12.;
    const int ycenter = r.center().y();

    const range_t<int> xrange = {r.left() + 10, r.right() - 5};
    const range_t<int> yrange = {r.bottom(), r.top()};

    //----------
    // Fretboard
    //----------

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // draw background
    painter.drawPixmap(r, mBackground);
    // draw frets
    painter.setPen(QPen(QColor("#444444"), 3.));
    for (const auto pos : fretPositions) {
        const auto x = expand(pos, xrange);
        painter.drawLine(x, yrange.min, x, yrange.max);
    }
    // draw marks
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::black));
    for (const auto mark : singleMarkPositions) {
        const auto x = expand(mark, xrange);
        painter.drawEllipse({x, ycenter}, markRadius, markRadius);
    }
    for (const auto mark : doubleMarkPositions) {
        const auto x = expand(mark, xrange);
        painter.drawEllipse({x, r.top() + h/4}, markRadius, markRadius);
        painter.drawEllipse({x, r.bottom() - h/4}, markRadius, markRadius);
    }
    // draw strings
    painter.setPen(QPen(QColor("#86865d"), 2.));
    for (size_t n = 0 ; n < stringsCount ; n++) {
        const auto y = expand(stringPosition(n, stringsCount), yrange);
        painter.drawLine(xrange.min, y, r.right(), y);
    }
    // draw capo
    if (mCapo > 0) {
        painter.setPen(Qt::NoPen);
        const auto x = expand(fretCenters[mCapo], xrange);
        QRectF capoRect({}, QSizeF(capoWidth, h));
        capoRect.moveCenter({(qreal)x, (qreal)ycenter});
        painter.drawRoundedRect(capoRect, capoRadius, capoRadius);
    }

    //------
    // Notes
    //------

    if (channelEditor()) {
        painter.setPen(QColor(Qt::black));
        for (size_t n = 0 ; n < stringsCount ; n++) {
            const int y = expand(stringPosition(n, stringsCount), yrange);
            size_t fret = 0;
            for (channels_t cs : mState[n]) {
                if (cs) {
                    const auto x = expand(fretCenters[fret], xrange);
                    painter.setBrush(channelEditor()->brush(cs));
                    painter.drawEllipse({x, y}, channelRadius, channelRadius);
                }
                ++fret;
            }
        }
    }
}

void Guitar::clearNotes() {
    for (auto& stringState : mState)
        channel_ns::clear(stringState);
}

void Guitar::generateFretOn(const Location& loc, Qt::MouseButtons buttons) {
    auto channels = channelsFromButtons(buttons);
    if (isValid(loc) && channels) {
        generateNoteOn(channels, toNote(loc));
        activate(loc, channels);
    }
}

void Guitar::generateFretOff(const Location& loc, Qt::MouseButtons buttons) {
    auto channels = channelsFromButtons(buttons);
    if (isValid(loc) && channels) {
        generateNoteOff(channels, toNote(loc));
        deactivate(loc, channels);
    }
}

void Guitar::activate(const Location& loc, channels_t channels) {
    mState[loc.first][loc.second] |= channels;
    update();
}

void Guitar::deactivate(const Location& loc, channels_t channels) {
    mState[loc.first][loc.second] &= ~channels;
    update();
}

bool Guitar::isValid(const Location& loc) const {
    return 0 <= loc.first && loc.first < (int)mTuning.size() && (int)mCapo <= loc.second && loc.second < (int)fretCount;
}

Note Guitar::toNote(const Location& loc) const {
    return Note::from_code(mTuning[loc.first].code() + loc.second);
}

Guitar::Location Guitar::fromNote(int string, const Note& note) const {
    return {string, note.code() - mTuning[string].code()};
}

Guitar::Location Guitar::locationAt(const QPoint& point) const {
    auto r = rect();
    const range_t<int> xrange = {r.left() + 10, r.right() - 5};
    const range_t<int> yrange = {r.bottom(), r.top()};
    const range_t<double> stringRange = {0., (double)mTuning.size()};
    int fret = -1;
    if (r.left() <= point.x() && point.x() <= r.right())
        fret = nearestFretCenter(reduce(xrange, point.x()));
    return {static_cast<int>(rescale(yrange, point.y(), stringRange)), fret};
}
