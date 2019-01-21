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

#include "multislider.h"
#include "misc.h"

#include <cmath>
#include <QApplication>
#include <QResizeEvent>
#include <QScrollBar>
#include <QGraphicsSceneMouseEvent>

//=========
// details
//=========

namespace {

auto sizeBoundingRect(const QSizeF& size) {
    return QRectF{QPointF{-size.width()/2., -size.height()/2.}, size};
}

auto scaleSpan(const Scale& scale) {
    return std::abs(span(scale.adjusted()));
}

const auto minimalRect = sizeBoundingRect({12., 12.});

}

//=======
// Scale
//=======

range_t<Scale::discrete_type> Scale::ticks() const {
    return {0, static_cast<discrete_type>(cardinality) - 1};
}

Scale::discrete_type Scale::nearest() const {
    return nearest(value);
}

Scale::discrete_type Scale::nearest(internal_type v) const {
    return rescale(range, v, ticks());
}

Scale::internal_type Scale::joint(discrete_type v) const {
    return rescale(ticks(), v, range);
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
    return expand(v, adjusted());
}

Scale::internal_type Scale::downscale(external_type v) const {
    return reduce(adjusted(), v);
}

void Scale::pin(internal_type v) {
    range = {v, v};
    value = v;
}

//======
// Knob
//======

Knob::Knob() : QGraphicsObject{} {
    setFlag(ItemIsMovable);
    setFlag(ItemIsFocusable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
}

QPointF Knob::expectedPos() const {
    return {mXScale.upscale(), mYScale.upscale()};
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

const QPen& Knob::pen() const {
    return mPen;
}

void Knob::setPen(const QPen& pen) {
    prepareGeometryChange();
    mPen = pen;
    update();
}

const QBrush& Knob::brush() const {
    return mBrush;
}

void Knob::setBrush(const QBrush& brush) {
    mBrush = brush;
    update();
}

bool Knob::isMovable() const {
    return flags() & ItemIsMovable;
}

void Knob::setMovable(bool movable) {
    setFlag(ItemIsMovable, movable);
}

void Knob::transpose() {
    prepareGeometryChange();
    std::swap(mXScale.value, mYScale.value);
    std::swap(mXScale.range, mYScale.range);
    std::swap(mXScale.cardinality, mYScale.cardinality);
    std::swap(mXScale.margins, mYScale.margins);
}

void Knob::moveToFit() {
    mUpdatePosition = false;
    setPos(expectedPos());
    mUpdatePosition = true;
}

void Knob::scroll(int delta) {
    if (isMovable()) {
        const auto increment = static_cast<int>(std::copysign(1, delta));
        const qreal xWanted = mXScale.cardinality < 2 || !mXScale.range ? x() + increment : mXScale.upscale(mXScale.joint(mXScale.nearest() + increment));
        const qreal yWanted = mYScale.cardinality < 2 || !mYScale.range ? y() - increment : mYScale.upscale(mYScale.joint(mYScale.nearest() + increment));
        setPos(xWanted, yWanted);
    }
}

void Knob::setVisibleRect(const QRectF& rect) {
    prepareGeometryChange();
    mXScale.externalRange = {rect.left(), rect.right()};
    mYScale.externalRange = {rect.top(), rect.bottom()};
    moveToFit();
}

QVariant Knob::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && mUpdatePosition) {
        // get requested position
        auto request = value.toPointF();
        // get previous position
        const auto previousPos = expectedPos();
        // if the shift modifier is pressed, change request
        if (static_cast<bool>(qApp->keyboardModifiers() & Qt::ShiftModifier) && !mPreviousRequest.isNull()) {
            // get request direction
            const auto offset = QPointF{std::copysign(.1, request.x() - mPreviousRequest.x()),
                                        std::copysign(.1, request.y() - mPreviousRequest.y())};
            // update previous request
            mPreviousRequest = request;
            // change request
            request = previousPos + offset;
        }
        // alter position from request
        mXScale.value = clamp(mXScale.range, mXScale.downscale(request.x()));
        mYScale.value = clamp(mYScale.range, mYScale.downscale(request.y()));
        // get corresponding point
        const auto newPos = expectedPos();
        // signal any alteration of position
        if (previousPos != newPos)
            emit knobMoved(mXScale.value, mYScale.value);
        // coerce the computed position
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

void Knob::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    mPreviousRequest = event->pos();
    emit knobPressed(event->button());
    QGraphicsObject::mousePressEvent(event);
}

void Knob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    mPreviousRequest = {};
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

void Knob::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    emit knobEntered();
    QGraphicsObject::hoverEnterEvent(event);
}


//==============
// ParticleKnob
//==============

ParticleKnob::ParticleKnob(qreal radius) : Knob{}, mRadius{radius} {
    setBrush({Qt::black});
}

ParticleKnob::shape_type ParticleKnob::particleShape() const {
    return mShape;
}

void ParticleKnob::setParticleShape(shape_type shape) {
    prepareGeometryChange();
    mShape = shape;
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
    return sizeBoundingRect({2*radius(), 2*radius()});
}

QRectF ParticleKnob::boundingRect() const {
    const auto w = pen().widthF();
    return enclosingRect().adjusted(-w, -w, w, w);
}

void ParticleKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->setBrush(brush());
    switch (mShape) {
    case shape_type::round_rect : painter->drawRoundRect(enclosingRect(), 50, 50); break;
    case shape_type::rect : painter->drawRect(enclosingRect()); break;
    case shape_type::ellipse : painter->drawEllipse(enclosingRect()); break;
    }
}

// ===========
// GutterKnob
// ===========

GutterKnob::GutterKnob(qreal radius) : Knob{}, mRadius{radius} {
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsFocusable, false);
    setFlag(ItemSendsGeometryChanges, false);
    setFlag(ItemStacksBehindParent);
    setAcceptHoverEvents(true);
}

qreal GutterKnob::radius() const {
    return mRadius;
}

void GutterKnob::setRadius(qreal radius) {
    prepareGeometryChange();
    mRadius = radius;
    update();
}

QRectF GutterKnob::boundingRect() const {
    const auto boundingSize = xScale().range ? QSizeF{scaleSpan(xScale()) + 2*radius(), 2*radius()} : QSizeF{2*radius(), scaleSpan(yScale()) + 2*radius()};
    return sizeBoundingRect(boundingSize);
}

void GutterKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawRoundedRect(boundingRect(), radius(), radius());
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

BracketKnob::BracketKnob(QBoxLayout::Direction direction) : Knob{}, mDirection{direction}, mPath{makeBracket(direction)} {
    setPen({Qt::black});
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
    const auto w = pen().widthF();
    const auto rect = mPath.controlPointRect().adjusted(-w, -w, w, w);
    return rect | minimalRect; /*!< @note extends rect for grabbing */
}

void BracketKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->drawPath(mPath);
}

//===========
// ArrowKnob
//===========

namespace {

QPainterPath makeArrow(QBoxLayout::Direction direction, qreal base=12., qreal altitude=12.) {
    QPainterPath path(QPointF{0., 0.});
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

ArrowKnob::ArrowKnob(QBoxLayout::Direction direction) : Knob{}, mDirection{direction}, mPath{makeArrow(direction)} {
    setBrush({Qt::black});
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
    return mPath.controlPointRect() | minimalRect; /*!< @note extends rect for grabbing */
}

void ArrowKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPath(mPath);
}

//==========
// TextKnob
//==========

TextKnob::TextKnob() : Knob{} {
    setPen({Qt::black});
    setMovable(false);
}

const QString& TextKnob::text() const {
    return mText;
}

void TextKnob::setText(const QString& text) {
    prepareGeometryChange();
    mText = text;
    mTextSize = QSizeF{};
    QFont font;
    const QFontMetrics fm{font};
    if (!mText.isEmpty())
        mTextSize = fm.boundingRect(mText).size();
    update();
}

QRectF TextKnob::boundingRect() const {
    return sizeBoundingRect(mTextSize).adjusted(-2, -2, 2, 2);
}

void TextKnob::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawText(boundingRect(), Qt::AlignCenter, mText);
}

//==========
// KnobView
//==========

KnobView::KnobView(QWidget* parent) : QGraphicsView{parent} {
    auto* scene = new QGraphicsScene{this};
    scene->setSceneRect(QRectF{0., 0., 200., 50.}); // whatever the value, we just need to set it once for all
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

void KnobView::updateVisibleRect() {
    const auto rect = visibleRect();
    for (auto* knob : knobs())
        knob->setVisibleRect(rect);
}

void KnobView::insertKnob(Knob* knob) {
    scene()->addItem(knob);
    knob->setVisibleRect(visibleRect());
}

const QBrush& KnobView::particleColor() const {
    return mParticleColor;
}

void KnobView::setParticleColor(const QBrush& brush) {
    mParticleColor = brush;
    for (auto* knob : knobs<ParticleKnob>())
        knob->setBrush(brush);
}

const QBrush& KnobView::gutterColor() const {
    return mGutterColor;
}

void KnobView::setGutterColor(const QBrush& brush) {
    mGutterColor = brush;
    for (auto* knob : knobs<GutterKnob>())
        knob->setBrush(brush);
}

const QBrush& KnobView::textColor() const {
    return mTextColor;
}

void KnobView::setTextColor(const QBrush& brush) {
    mTextColor = brush;
    for (auto* knob : knobs<TextKnob>()) {
        auto pen = knob->pen();
        pen.setBrush(brush);
        knob->setPen(pen);
    }
}

void KnobView::setScrolledKnob(Knob* knob) {
    if (!knob || knob->isMovable())
        mLastKnobScrolled = knob;
}

void KnobView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    updateVisibleRect();
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
    auto* knob = dynamic_cast<Knob*>(itemAt(event->pos()));
    if (!knob || !knob->isMovable())
        knob = mLastKnobScrolled;
    if (knob)
        knob->scroll(event->delta());
    mLastKnobScrolled = knob;
}

//=============
// MultiSlider
//=============

namespace {

void setDimensions(QWidget* widget, const range_t<int>& width, const range_t<int>& height) {
    widget->setSizePolicy(width ? QSizePolicy::Preferred : QSizePolicy::Fixed,
                          height ? QSizePolicy::Preferred : QSizePolicy::Fixed);
    widget->setMinimumSize(width.min, height.min);
    widget->setMaximumSize(width.max, height.max);
}

}

MultiSlider::MultiSlider(Qt::Orientation orientation, QWidget* parent) : QWidget{parent}, mOrientation{orientation} {
    mParticleSlider = new KnobView{this};
    mParticleSlider->setObjectName("ParticleSlider");
    mTextSlider = new KnobView{this};
    mTextSlider->setObjectName("TextSlider");
    auto* layout = new QBoxLayout{orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight};
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
    static_cast<QBoxLayout*>(layout())->setDirection(orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    emit orientationChanged(orientation);
}

int MultiSlider::textWidth() const {
    return mTextWidth;
}

void MultiSlider::setTextWidth(int textWidth) {
    mTextWidth = textWidth;
    updateDimensions();
    emit textWidthChanged(textWidth);
}

MultiSlider::Unit MultiSlider::insertUnit(Unit unit, qreal margin, qreal ratio) {

    if (!unit.particle)
        unit.particle = new ParticleKnob{6.};
    if (!unit.text)
        unit.text = new TextKnob;
    if (!unit.gutter)
        unit.gutter = new GutterKnob{.5 * unit.particle->radius()};

    connect(unit.gutter, &Knob::knobEntered, mParticleSlider, [=] { mParticleSlider->setScrolledKnob(unit.particle); });

    connect(unit.particle, &Knob::visibleChanged, unit.gutter, [=] { unit.gutter->setVisible(unit.particle->isVisible()); });
    connect(unit.particle, &Knob::visibleChanged, unit.text, [=] { unit.text->setVisible(unit.particle->isVisible()); });
    unit.gutter->setVisible(unit.particle->isVisible());
    unit.text->setVisible(unit.particle->isVisible());

    knobMainScale(unit.text).value = .5;
    knobMainScale(unit.gutter).value = .5;

    const bool isHorizontal = mOrientation == Qt::Horizontal;
    unit.particle->yScale().reversed = !isHorizontal;
    unit.text->setRotation(isHorizontal ? 0 : -90);

    setUnitMargin(unit, margin);
    setUnitRatio(unit, ratio);

    mParticleSlider->insertKnob(unit.particle);
    mTextSlider->insertKnob(unit.text);
    mParticleSlider->insertKnob(unit.gutter);

    return unit;

}

void MultiSlider::setUnitMargin(Unit unit, qreal margin) {
    margin += unit.particle->radius();
    unit.particle->xScale().margins = {margin, margin};
    unit.particle->yScale().margins = {margin, margin};
    unit.gutter->xScale().margins = {margin, margin};
    unit.gutter->yScale().margins = {margin, margin};
    knobOffScale(unit.text).margins = {margin, margin};
}

void MultiSlider::setUnitRatio(Unit unit, qreal ratio) {
    knobOffScale(unit.particle).pin(ratio);
    knobOffScale(unit.text).pin(ratio);
    knobOffScale(unit.gutter).pin(ratio);
}

Scale& MultiSlider::knobMainScale(Knob* knob) const {
    return mOrientation == Qt::Horizontal ? knob->xScale() : knob->yScale();
}

Scale& MultiSlider::knobOffScale(Knob* knob) const {
    return mOrientation == Qt::Horizontal ? knob->yScale() : knob->xScale();
}

qreal MultiSlider::knobRatio(Knob* knob) const {
    return knobMainScale(knob).value;
}

void MultiSlider::setKnobRatio(Knob* knob, qreal ratio) const {
    knobMainScale(knob).value = ratio;
    knob->moveToFit();
}

void MultiSlider::transpose() {
    for (auto* particle : mParticleSlider->knobs()) {
        particle->transpose();
        particle->yScale().reversed = mOrientation == Qt::Vertical;
    }
    for (auto* text : mTextSlider->knobs()) {
        text->transpose();
        text->setRotation(mOrientation == Qt::Vertical ? -90 : 0);
    }
}

void MultiSlider::updateDimensions() {
    // minimum size required for non overlapping
    qreal radiusSum = 0.;
    for (auto* knob : mParticleSlider->knobs<ParticleKnob>())
        if (knob->isVisible())
            radiusSum += knob->radius();
    const auto size = decay_value<int>(10. + 2. * radiusSum);
    if (mOrientation == Qt::Horizontal) {
        setDimensions(mParticleSlider, {0, QWIDGETSIZE_MAX}, {size, size});
        setDimensions(mTextSlider, {mTextWidth, mTextWidth}, {size, size});
    } else {
        setDimensions(mParticleSlider, {size, size}, {0, QWIDGETSIZE_MAX});
        setDimensions(mTextSlider, {size, size}, {mTextWidth, mTextWidth});
    }
    // force update of knobs even if no resizeEvent is thrown
    mParticleSlider->updateVisibleRect();
    mTextSlider->updateVisibleRect();
}

//==============
// SimpleSlider
//==============

SimpleSlider::SimpleSlider(Qt::Orientation orientation, QWidget* parent) : MultiSlider{orientation, parent} {
    mUnit = insertUnit({});
    connect(mUnit.particle, &ParticleKnob::knobDoubleClicked, this, &SimpleSlider::onKnobClick);
    connect(mUnit.particle, &ParticleKnob::knobMoved, this, &SimpleSlider::onKnobMove);
    connect(mUnit.text, &TextKnob::knobDoubleClicked, this, &SimpleSlider::onKnobClick);
    updateDimensions();
}

ParticleKnob* SimpleSlider::particle() {
    return mUnit.particle;
}

size_t SimpleSlider::cardinality() const {
    return knobMainScale(mUnit.particle).cardinality;
}

void SimpleSlider::setCardinality(size_t cardinality) {
    knobMainScale(mUnit.particle).cardinality = cardinality;
}

qreal SimpleSlider::defaultRatio() const {
    return mDefaultRatio;
}

void SimpleSlider::setDefaultRatio(qreal ratio) {
    mDefaultRatio = ratio;
}

qreal SimpleSlider::ratio() const {
    return knobRatio(mUnit.particle);
}

void SimpleSlider::setRatio(qreal ratio) {
    setKnobRatio(mUnit.particle, ratio);
    emit knobChanged(ratio);
}

void SimpleSlider::setClampedRatio(qreal ratio) {
    setRatio(range_ns::clamp({0., 1.}, ratio));
}

void SimpleSlider::setDefault() {
    setRatio(mDefaultRatio);
}

void SimpleSlider::setText(const QString& text) {
    mUnit.text->setText(text);
}

void SimpleSlider::onKnobClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        setKnobRatio(mUnit.particle, mDefaultRatio);
        emit knobMoved(mDefaultRatio);
    }
}

void SimpleSlider::onKnobMove() {
    emit knobMoved(ratio());
}
