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

#include "guitar.h"

#include <QPainter>
#include <QToolTip>
#include "qcore/editors.h"

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

//============
// MetaGuitar
//============

MetaGuitar::MetaGuitar(QObject* parent) : MetaInstrument(parent) {
    setIdentifier("Guitar");
    setDescription("Interactive Guitar Fretboard");
    addParameter("tuning", ":Notes", "list of notes separated by ';' from lower string to higher string", serial::serializeNotes(defaultTuning));
    addParameter("capo", ":ULong", "capo position, no capo if 0", "0");
}

MetaHandler::Instance MetaGuitar::instantiate() {
    return Instance(new Guitar, nullptr);
}

//========
// Guitar
//========

Guitar::Guitar() : Instrument(handler_ns::io_mode), mTuning(defaultTuning), mCapo(0), mState(defaultTuning.size()), mLastLocation(-1, 0), mBackground(":/data/wood.jpg") {
    installEventFilter(this);
    clearNotes();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
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
        mLastLocation = {-1, 0};
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
        mLastLocation = {-1, 0};
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

void Guitar::enterEvent(QEvent*) {
    if (canGenerate()) {
        grabMouse();
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }
}

bool Guitar::eventFilter(QObject* obj, QEvent* event) {

    Q_ASSERT(obj == this);

    /// handle tooltip event
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        auto loc = locationAt(helpEvent->pos());
        if (isValid(loc)) {
            QToolTip::showText(helpEvent->globalPos(), QString::fromStdString(toNote(loc).string()));
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }

    /// try to get mouse events
    QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if (mouseEvent == nullptr)
        return Instrument::eventFilter(obj, event);

    auto loc = locationAt(mouseEvent->pos());

    /// stop tracking on leave
    if (!isValid(loc) && hasMouseTracking() && mouseEvent->buttons() == Qt::NoButton) {
        releaseMouse();
        setMouseTracking(false);
        unsetCursor();
    }

    /// don't process events if closed or disable
    if (!canGenerate())
        return true;

    /// check mouse event type
    switch (mouseEvent->type()) {
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
        generateFretOn(loc, mouseEvent->button());
        break;
    case QEvent::MouseButtonRelease:
        generateFretOff(loc, mouseEvent->button());
        break;
    case QEvent::MouseMove:
        if (mLastLocation != loc && mouseEvent->buttons() != Qt::NoButton) {
            generateFretOff(mLastLocation, mouseEvent->buttons());
            generateFretOn(loc, mouseEvent->buttons());
        }
        break;
    }

    mLastLocation = loc;
    return true;
}

void Guitar::paintEvent(QPaintEvent* event) {

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
    for (auto pos : fretPositions) {
        auto x = xrange.expand(pos);
        painter.drawLine(x, yrange.min, x, yrange.max);
    }
    // draw marks
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::black));
    for (auto mark : singleMarkPositions) {
        auto x = xrange.expand(mark);
        painter.drawEllipse({x, ycenter}, markRadius, markRadius);
    }
    for (auto mark : doubleMarkPositions) {
        auto x = xrange.expand(mark);
        painter.drawEllipse({x, r.top() + h/4}, markRadius, markRadius);
        painter.drawEllipse({x, r.bottom() - h/4}, markRadius, markRadius);
    }
    // draw strings
    painter.setPen(QPen(QColor("#86865d"), 2.));
    for (size_t n = 0 ; n < stringsCount ; n++) {
        auto y = yrange.expand(stringPosition(n, stringsCount));
        painter.drawLine(xrange.min, y, r.right(), y);
    }
    // draw capo
    if (mCapo > 0) {
        painter.setPen(Qt::NoPen);
        auto x = xrange.expand(fretCenters[mCapo]);
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
            int y = yrange.expand(stringPosition(n, stringsCount));
            size_t fret = 0;
            for (channels_t cs : mState[n]) {
                if (cs) {
                    auto x = xrange.expand(fretCenters[fret]);
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
    ChannelEditor* editor = channelEditor();
    channels_t channels = editor ? editor->channelsFromButtons(buttons) : channels_t::merge(0);
    if (isValid(loc) && channels) {
        generateNoteOn(channels, toNote(loc));
        activate(loc, channels);
    }
}

void Guitar::generateFretOff(const Location& loc, Qt::MouseButtons buttons) {
    ChannelEditor* editor = channelEditor();
    channels_t channels = editor ? editor->channelsFromButtons(buttons) : channels_t::merge(0);
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
    return {(int)stringRange.rescale(yrange, point.y()), nearestFretCenter(xrange.reduce(point.x()))};
}