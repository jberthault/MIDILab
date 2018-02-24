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

#include "multislider.h"
#include "misc.h"

#include <cmath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QGraphicsSceneMouseEvent>

//=========
// details
//=========

namespace {

QRectF sizeBoundingRect(const QSizeF& size) {
    return {QPointF(-size.width()/2., -size.height()/2.), size};
}

}

//=======
// Scale
//=======

Scale::Scale() : value(0.), range({0., 1.}), cardinality(0), externalRange({0., 1.}), margins({0., 0.}), reversed(false) {

}

range_t<Scale::discrete_type> Scale::ticks() const {
    return {0, (discrete_type)cardinality - 1};
}

Scale::discrete_type Scale::nearest() const {
    return nearest(value);
}

Scale::discrete_type Scale::nearest(internal_type v) const {
    return ticks().rescale(range, v);
}

Scale::internal_type Scale::joint(discrete_type v) const {
    return range.rescale(ticks(), v);
}

range_t<Scale::external_type> Scale::adjusted() const {
    range_t<Scale::external_type> r;
    (reversed ? r.max : r.min) = externalRange.min + margins.min - 1.;
    (reversed ? r.min : r.max) = externalRange.max - margins.max - 1.;
    return r;
}

Scale::external_type Scale::upscale() const {
    return upscale(value);
}

Scale::external_type Scale::upscale(internal_type v) const {
    return adjusted().expand(v);
}

Scale::internal_type Scale::downscale(external_type v) const {
    return adjusted().reduce(v);
}

Scale::external_type Scale::update(external_type v) {
    value = downscale(v);
    if (value > range.max) {
        value = range.max;
        return upscale(range.max);
    } else if (value < range.min) {
        value = range.min;
        return upscale(range.min);
    }
    return v;
}

void Scale::clamp(internal_type v) {
    range = {v, v};
    value = v;
}

//======
// Knob
//======

Knob::Knob() : QGraphicsObject(), mXScale(), mYScale(), mUpdatePosition(true) {
    setFlag(ItemIsMovable);
    setFlag(ItemIsFocusable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
}

const Scale& Knob::xScale() const {
    return mXScale;
}

Scale& Knob::xScale() {
    return mXScale;
}

const Scale& Knob::yScale() const {
    return mYScale;
}

Scale& Knob::yScale() {
    return mYScale;
}

void Knob::transpose() {
    std::swap(mXScale.value, mYScale.value);
    std::swap(mXScale.range, mYScale.range);
    std::swap(mXScale.cardinality, mYScale.cardinality);
    std::swap(mXScale.margins, mYScale.margins);
}

void Knob::moveToFit() {
    mUpdatePosition = false;
    // setPos(std::round(mXScale.upscale()), std::round(mYScale.upscale()));
    setPos(mXScale.upscale(), mYScale.upscale());
    mUpdatePosition = true;
}

void Knob::scroll(int delta) {
    if (flags() & ItemIsMovable) {
        int increment = std::copysign(1, delta);
        qreal xWanted = mXScale.cardinality < 2 || !mXScale.range ? x() + increment : mXScale.upscale(mXScale.joint(mXScale.nearest() + increment));
        qreal yWanted = mYScale.cardinality < 2 || !mYScale.range ? y() - increment : mYScale.upscale(mYScale.joint(mYScale.nearest() + increment));
        setPos(xWanted, yWanted);
    }
}

QVariant Knob::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && mUpdatePosition) {
        auto newValue = value.toPointF();
        newValue.setX(mXScale.update(newValue.x()));
        newValue.setY(mYScale.update(newValue.y()));
        emit knobMoved(mXScale.value, mYScale.value);
        return newValue;
    }
    return QGraphicsItem::itemChange(change, value);
}

void Knob::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    emit knobPressed(event->button());
    QGraphicsObject::mousePressEvent(event);
}

void Knob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    emit knobReleased(event->button());
    QGraphicsObject::mouseReleaseEvent(event);
}

void Knob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    emit knobDoubleClicked(event->button());
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void Knob::wheelEvent(QGraphicsSceneWheelEvent* event) {
    scroll(event->delta());
    QGraphicsObject::wheelEvent(event);
}

//==============
// ParticleKnob
//==============

ParticleKnob::ParticleKnob() :
    Knob(), mShape(shape_type::ellipse), mPen(Qt::NoPen), mColor(Qt::black), mRadius(7.) {

}

ParticleKnob::shape_type ParticleKnob::particleShape() const {
    return mShape;
}

void ParticleKnob::setParticleShape(shape_type shape) {
    mShape = shape;
    update();
}

const QPen& ParticleKnob::pen() const {
    return mPen;
}

void ParticleKnob::setPen(const QPen& pen) {
    prepareGeometryChange();
    mPen = pen;
    update();
}

const QBrush& ParticleKnob::color() const {
    return mColor;
}

void ParticleKnob::setColor(const QBrush& brush) {
    mColor = brush;
    update();
}

qreal ParticleKnob::radius() const {
    return mRadius;
}

void ParticleKnob::setRadius(qreal radius) {
    prepareGeometryChange();
    mRadius = radius;
    update();
}

QRectF ParticleKnob::enclosingRect() const {
    return sizeBoundingRect(QSizeF(2*mRadius, 2*mRadius));
}

QRectF ParticleKnob::boundingRect() const {
    qreal w = mPen.widthF();
    return enclosingRect().adjusted(-w, -w, w, w);
}

void ParticleKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setPen(mPen);
    painter->setBrush(mColor);
    switch (mShape) {
    case shape_type::round_rect : painter->drawRoundRect(enclosingRect(), 50, 50); break;
    case shape_type::rect : painter->drawRect(enclosingRect()); break;
    case shape_type::ellipse : painter->drawEllipse(enclosingRect()); break;
    }
}

//=============
// BracketKnob
//=============

namespace {

QPainterPath makeBracket(QBoxLayout::Direction direction, qreal long_size=8., qreal short_size=2.) {
    QPainterPath path;
    switch (direction) {
    case QBoxLayout::LeftToRight:
        path.moveTo(short_size, -long_size);
        path.lineTo(0., -long_size);
        path.lineTo(0., long_size);
        path.lineTo(short_size, long_size);
        break;
    case QBoxLayout::RightToLeft:
        path.moveTo(-short_size, -long_size);
        path.lineTo(0., -long_size);
        path.lineTo(0., long_size);
        path.lineTo(-short_size, long_size);
        break;
    case QBoxLayout::TopToBottom:
        path.moveTo(-long_size, short_size);
        path.lineTo(-long_size, 0.);
        path.lineTo(long_size, 0.);
        path.lineTo(long_size, short_size);
        break;
    case QBoxLayout::BottomToTop:
        path.moveTo(-long_size, -short_size);
        path.lineTo(-long_size, 0.);
        path.lineTo(long_size, 0.);
        path.lineTo(long_size, -short_size);
        break;
    }
    return path;
}

}

BracketKnob::BracketKnob(QBoxLayout::Direction direction) :
    Knob(), mPen(Qt::black, 1), mDirection(direction), mPath(makeBracket(direction)) {

}

const QPen& BracketKnob::pen() const {
    return mPen;
}

void BracketKnob::setPen(const QPen& pen) {
    prepareGeometryChange();
    mPen = pen;
    update();
}

QBoxLayout::Direction BracketKnob::direction() const {
    return mDirection;
}

void BracketKnob::setDirection(QBoxLayout::Direction direction) {
    prepareGeometryChange();
    mDirection = direction;
    mPath = makeBracket(direction);
    update();
}

QRectF BracketKnob::boundingRect() const {
    static const QRectF minimalRect = sizeBoundingRect(QSizeF(12., 12.));
    qreal w = mPen.widthF();
    QRectF rect = mPath.controlPointRect().adjusted(-w, -w, w, w);
    return rect | minimalRect; /*!< @note extends rect for grabbing */
}

void BracketKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setPen(mPen);
    painter->drawPath(mPath);
}

//===========
// ArrowKnob
//===========

namespace {

QPainterPath makeArrow(QBoxLayout::Direction direction, qreal base=12., qreal altitude=12.) {
    QPainterPath path(QPointF(0., 0.));
    switch (direction) {
    case QBoxLayout::LeftToRight:
        path.lineTo(-altitude, -base / 2.);
        path.lineTo(-altitude, base / 2.);
        break;
    case QBoxLayout::RightToLeft:
        path.lineTo(altitude, -base / 2.);
        path.lineTo(altitude, base / 2.);
        break;
    case QBoxLayout::TopToBottom:
        path.lineTo(-base / 2., -altitude);
        path.lineTo(base / 2., -altitude);
        break;
    case QBoxLayout::BottomToTop:
        path.lineTo(-base / 2., altitude);
        path.lineTo(base / 2., altitude);
        break;
    }
    path.closeSubpath();
    return path;
}

}

ArrowKnob::ArrowKnob(QBoxLayout::Direction direction) :
    Knob(), mColor(Qt::black), mDirection(direction), mPath(makeArrow(direction)) {

}

const QBrush& ArrowKnob::color() const {
    return mColor;
}

void ArrowKnob::setColor(const QBrush& brush) {
    mColor = brush;
    update();
}

QBoxLayout::Direction ArrowKnob::direction() const {
    return mDirection;
}

void ArrowKnob::setDirection(QBoxLayout::Direction direction) {
    prepareGeometryChange();
    mDirection = direction;
    mPath = makeArrow(direction);
    update();
}

QRectF ArrowKnob::boundingRect() const {
    static const QRectF minimalRect = sizeBoundingRect(QSizeF(12., 12.));
    return mPath.controlPointRect() | minimalRect; /*!< @note extends rect for grabbing */
}

void ArrowKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(mColor);
    painter->drawPath(mPath);
}

//==========
// TextKnob
//==========

TextKnob::TextKnob() : Knob(), mText(), mTextSize() {

}

const QString& TextKnob::text() const {
    return mText;
}

void TextKnob::setText(const QString& text) {
    prepareGeometryChange();
    mText = text;
    mTextSize = QSizeF();
    QFont font;
    QFontMetrics fm(font);
    if (!mText.isEmpty())
        mTextSize = fm.boundingRect(mText).size();
    update();
}

QRectF TextKnob::boundingRect() const {
    return sizeBoundingRect(mTextSize).adjusted(-2, -2, 2, 2);
}

void TextKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setPen(Qt::black);
    // painter->setBrush(QBrush(Qt::white));
    painter->drawText(boundingRect(), Qt::AlignCenter, mText);
}

//==========
// KnobView
//==========

KnobView::KnobView(QWidget* parent) : QGraphicsView(parent), mLastKnobScrolled(nullptr) {
    QGraphicsScene* scene = new QGraphicsScene(this);
    scene->setSceneRect(QRectF(0., 0., 200., 50.)); // whatever the value, we just need to set it once for all
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(scene);
    horizontalScrollBar()->blockSignals(true);
    verticalScrollBar()->blockSignals(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);
}

QRectF KnobView::visibleRect() const {
    return mapToScene(viewport()->geometry()).boundingRect();
}

void KnobView::insertKnob(Knob* knob) {
    scene()->addItem(knob);
    setVisibleRect(knob, visibleRect());
}

void KnobView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    auto rect = visibleRect();
    for (auto knob : knobs())
        setVisibleRect(knob, rect);
}

void KnobView::leaveEvent(QEvent* event) {
    QGraphicsView::leaveEvent(event);
    mLastKnobScrolled = nullptr;
}

void KnobView::mouseDoubleClickEvent(QMouseEvent* event) {
    QGraphicsView::mouseDoubleClickEvent(event);
    if (!event->isAccepted())
        emit viewDoubleClicked(event->button());
}

void KnobView::wheelEvent(QWheelEvent* event) {
    /// @note do not forward event to the scene
    auto knob = dynamic_cast<Knob*>(itemAt(event->pos()));
    if (!knob)
        knob = mLastKnobScrolled;
    if (knob)
        knob->scroll(event->delta());
    mLastKnobScrolled = knob;
}

void KnobView::setVisibleRect(Knob* knob, const QRectF& rect) {
    knob->xScale().externalRange = {rect.left(), rect.right()};
    knob->yScale().externalRange = {rect.top(), rect.bottom()};
    knob->moveToFit();
}

//=============
// MultiSlider
//=============

namespace {

void setFixedDimension(QWidget* widget, Qt::Orientation orientation, int size) {
    if (orientation == Qt::Vertical) {
        widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        widget->setMinimumSize(0, size);
        widget->setMaximumSize(QWIDGETSIZE_MAX, size);
    } else {
        widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        widget->setMinimumSize(size, 0);
        widget->setMaximumSize(size, QWIDGETSIZE_MAX);
    }
}

}

MultiSlider::MultiSlider(Qt::Orientation orientation, QWidget* parent) : QWidget(parent), mOrientation(orientation), mTextWidth(0) {
    mParticleSlider = new KnobView(this);
    mTextSlider = new KnobView(this);
    auto layout = new QBoxLayout(orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    fill_box(layout, margin_tag{0}, spacing_tag{0}, mTextSlider, mParticleSlider);
    setLayout(layout);
}

KnobView* MultiSlider::particleSlider() const {
    return mParticleSlider;
}

KnobView* MultiSlider::textSlider() const {
    return mTextSlider;
}

Qt::Orientation MultiSlider::orientation() const {
    return mOrientation;
}

void MultiSlider::setOrientation(Qt::Orientation orientation) {
    if (orientation == mOrientation)
        return;
    mOrientation = orientation;
    transpose();
    updateDimensions();
    updateTextDimensions();
    static_cast<QBoxLayout*>(layout())->setDirection(orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    emit orientationChanged(orientation);
}

int MultiSlider::textWidth() const {
    return mTextWidth;
}

void MultiSlider::setTextWidth(int textWidth) {
    mTextWidth = textWidth;
    updateTextDimensions();
    emit textWidthChanged(textWidth);
}

void MultiSlider::insertKnob(ParticleKnob* particleKnob, TextKnob* textKnob, qreal margin, qreal ratio) {

    const bool isHorizontal = mOrientation == Qt::Horizontal;
    margin += particleKnob->radius();

    auto& particleMainScale = isHorizontal ? particleKnob->xScale() : particleKnob->yScale();
    auto& particleOffScale = isHorizontal ? particleKnob->yScale() : particleKnob->xScale();
    auto& textMainScale = isHorizontal ? textKnob->xScale() : textKnob->yScale();
    auto& textOffScale = isHorizontal ? textKnob->yScale() : textKnob->xScale();

    particleMainScale.margins = {margin, margin};
    particleOffScale.clamp(ratio);
    particleOffScale.margins = {margin, margin};
    particleKnob->yScale().reversed = !isHorizontal;

    textMainScale.value = .5;
    textOffScale.clamp(ratio);
    textOffScale.margins = {margin, margin};
    textKnob->setFlag(TextKnob::ItemIsMovable, false);
    textKnob->setRotation(isHorizontal ? 0 : -90);

    mParticleSlider->insertKnob(particleKnob);
    mTextSlider->insertKnob(textKnob);
}

Scale& MultiSlider::knobScale(Knob* knob) const {
    return mOrientation == Qt::Horizontal ? knob->xScale() : knob->yScale();
}

qreal MultiSlider::knobRatio(Knob* knob) const {
    return knobScale(knob).value;
}

void MultiSlider::setKnobRatio(Knob* knob, qreal ratio) const {
    knobScale(knob).value = ratio;
    knob->moveToFit();
}

void MultiSlider::transpose() {
    for (auto particle : mParticleSlider->knobs()) {
        particle->transpose();
        particle->yScale().reversed = mOrientation == Qt::Vertical;
    }
    for (auto text : mTextSlider->knobs()) {
        text->transpose();
        text->setRotation(mOrientation == Qt::Vertical ? -90 : 0);
    }
}

void MultiSlider::updateDimensions() {
    // minimum size required for non overlapping
    qreal radiusSum = 0.;
    for (auto knob : mParticleSlider->knobs<ParticleKnob>())
        if (knob->isVisible())
            radiusSum += knob->radius();
    const qreal size = 10. + 2. * radiusSum;
    setFixedDimension(this, mOrientation == Qt::Vertical ? Qt::Horizontal : Qt::Vertical, (int)size);
}

void MultiSlider::updateTextDimensions() {
    setFixedDimension(mTextSlider, mOrientation, mTextWidth);
}

//==============
// SimpleSlider
//==============

SimpleSlider::SimpleSlider(Qt::Orientation orientation, QWidget* parent) : MultiSlider(orientation, parent), mDefaultRatio(0.) {
    mParticle = new ParticleKnob();
    mText = new TextKnob();
    mParticle->setRadius(6.);
    connect(mParticle, &ParticleKnob::knobDoubleClicked, this, &SimpleSlider::onKnobClick);
    connect(mParticle, &ParticleKnob::knobMoved, this, &SimpleSlider::onKnobMove);
    connect(mText, &TextKnob::knobDoubleClicked, this, &SimpleSlider::onKnobClick);
    insertKnob(mParticle, mText, 2., .5);
    updateDimensions();
}

size_t SimpleSlider::cardinality() const {
    return knobScale(mParticle).cardinality;
}

void SimpleSlider::setCardinality(size_t cardinality) {
    knobScale(mParticle).cardinality = cardinality;
}

qreal SimpleSlider::defaultRatio() const {
    return mDefaultRatio;
}

void SimpleSlider::setDefaultRatio(qreal ratio) {
    mDefaultRatio = ratio;
}

qreal SimpleSlider::ratio() const {
    return knobRatio(mParticle);
}

void SimpleSlider::setRatio(qreal ratio) {
    setKnobRatio(mParticle, ratio);
    emit knobChanged(ratio);
}

void SimpleSlider::setDefault() {
    setRatio(mDefaultRatio);
}

void SimpleSlider::setText(const QString& text) {
    mText->setText(text);
}

void SimpleSlider::onKnobClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        setKnobRatio(mParticle, mDefaultRatio);
        emit knobMoved(mDefaultRatio);
    }
}

void SimpleSlider::onKnobMove() {
    emit knobMoved(ratio());
}
