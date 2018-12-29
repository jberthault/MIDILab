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

#ifndef QTOOLS_MULTISLIDER_H
#define QTOOLS_MULTISLIDER_H

#include <functional>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QBoxLayout>
#include "tools/bytes.h"

//=======
// Scale
//=======

struct Scale {

    using discrete_type = int;
    using internal_type = qreal;
    using external_type = qreal;

    range_t<discrete_type> ticks() const; /*!< compute the range [0, cardinality) */
    discrete_type nearest() const; /*!< get the index of the nearest tick from current value */
    discrete_type nearest(internal_type v) const; /*!< get the index of the nearest tick */
    internal_type joint(discrete_type v) const; /*!< compute internal value from a tick index */

    range_t<external_type> adjusted() const; /*!< external range adjusted by reversed and margins */
    external_type upscale() const; /*!< upscale current value */
    external_type upscale(internal_type v) const; /*!< rescale value from internal to external range */
    internal_type downscale(external_type v) const; /*!< rescale value from external to internal range */

    void pin(internal_type v); /*!< set internal range fixed on the given value */

    internal_type value {0.}; /*!< current value (should be within the range) */
    range_t<internal_type> range {0., 1.}; /*!< range in which value may evolve */
    size_t cardinality {0}; /*!< if 0, continuous range, else, number of elements allowed in the range */

    range_t<external_type> externalRange {0., 1.}; /*!< range in which value should be rescaled from and to */
    range_t<external_type> margins {0., 0.}; /*!< reduced space within external range when rescaling */
    bool reversed {false}; /*!< if true, the external range is reversed when rescaling */

};

//======
// Knob
//======

class Knob : public QGraphicsObject {

    Q_OBJECT

public:
    explicit Knob();

    QPointF expectedPos() const;

    const Scale& xScale() const;
    Scale& xScale();

    const Scale& yScale() const;
    Scale& yScale();

    const QPen& pen() const;
    void setPen(const QPen& pen);

    const QBrush& brush() const;
    void setBrush(const QBrush& brush);

    bool isMovable() const;
    void setMovable(bool movable);

    void transpose();
    void moveToFit();
    void scroll(int delta);
    void setVisibleRect(const QRectF& rect);

signals:
    void knobMoved(qreal xvalue, qreal yvalue);
    void knobPressed(Qt::MouseButton button);
    void knobReleased(Qt::MouseButton button);
    void knobDoubleClicked(Qt::MouseButton button);
    void knobEntered();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;


private:
    Scale mXScale;
    Scale mYScale;
    QPen mPen {Qt::NoPen};
    QBrush mBrush {Qt::NoBrush};
    bool mUpdatePosition {true};
    QPointF mPreviousRequest;

};

//==============
// ParticleKnob
//==============

class ParticleKnob : public Knob {

    Q_OBJECT

public:

    enum class shape_type {
        round_rect,
        rect,
        ellipse
    };

    explicit ParticleKnob(qreal radius);

    shape_type particleShape() const;
    void setParticleShape(shape_type shape);

    qreal radius() const;
    void setRadius(qreal radius);

    QRectF enclosingRect() const;
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    shape_type mShape {shape_type::ellipse};
    qreal mRadius;

};

// ===========
// GutterKnob
// ===========

class GutterKnob : public Knob {

    Q_OBJECT

public:
    explicit GutterKnob(qreal radius);

    qreal radius() const;
    void setRadius(qreal radius);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    qreal mRadius;

};

//=============
// BracketKnob
//=============

class BracketKnob : public Knob {

    Q_OBJECT

public:
    explicit BracketKnob(QBoxLayout::Direction direction = QBoxLayout::LeftToRight);

    QBoxLayout::Direction direction() const;
    void setDirection(QBoxLayout::Direction direction);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QBoxLayout::Direction mDirection;
    QPainterPath mPath;

};

//===========
// ArrowKnob
//===========

class ArrowKnob : public Knob {

    Q_OBJECT

public:
    explicit ArrowKnob(QBoxLayout::Direction direction = QBoxLayout::LeftToRight);

    QBoxLayout::Direction direction() const;
    void setDirection(QBoxLayout::Direction direction);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QBoxLayout::Direction mDirection;
    QPainterPath mPath;

};

//==========
// TextKnob
//==========

class TextKnob : public Knob {

    Q_OBJECT

public:
    explicit TextKnob();

    const QString& text() const;
    void setText(const QString& text);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QString mText;
    QSizeF mTextSize;

};

//==========
// KnobView
//==========

class KnobView : public QGraphicsView {

    Q_OBJECT

    Q_PROPERTY(QBrush particleColor READ particleColor WRITE setParticleColor)
    Q_PROPERTY(QBrush gutterColor READ gutterColor WRITE setGutterColor)
    Q_PROPERTY(QBrush textColor READ textColor WRITE setTextColor)

public:
    explicit KnobView(QWidget* parent);

    QRectF visibleRect() const;
    void updateVisibleRect();

    template<typename T = Knob>
    auto knobs() const {
        std::vector<T*> result;
        const auto items = scene()->items();
        result.reserve(items.size());
        for (auto* item : items)
            if (auto* knob = dynamic_cast<T*>(item))
                result.push_back(knob);
        return result;
    }

    void insertKnob(Knob* knob);

    const QBrush& particleColor() const;
    void setParticleColor(const QBrush& brush);

    const QBrush& gutterColor() const;
    void setGutterColor(const QBrush& brush);

    const QBrush& textColor() const;
    void setTextColor(const QBrush& brush);

    void setScrolledKnob(Knob* knob);

signals:
    void viewDoubleClicked(Qt::MouseButton button);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QBrush mParticleColor;
    QBrush mGutterColor;
    QBrush mTextColor;
    Knob* mLastKnobScrolled {nullptr};

};

//=============
// MultiSlider
//=============

class MultiSlider : public QWidget {

    Q_OBJECT

public:

    struct Unit {
        ParticleKnob* particle {nullptr};
        TextKnob* text {nullptr};
        GutterKnob* gutter {nullptr};
    };

    explicit MultiSlider(Qt::Orientation orientation, QWidget* parent);

    KnobView* particleSlider() const;
    KnobView* textSlider() const;

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    int textWidth() const;
    void setTextWidth(int textWidth);

    Scale& knobMainScale(Knob* knob) const;
    Scale& knobOffScale(Knob* knob) const;

    qreal knobRatio(Knob* knob) const;
    void setKnobRatio(Knob* knob, qreal ratio) const;

    Unit insertUnit(Unit unit, qreal margin = 2., qreal ratio = .5);
    void setUnitMargin(Unit unit, qreal margin);
    void setUnitRatio(Unit unit, qreal ratio);

signals:
    void orientationChanged(Qt::Orientation orientation);
    void textWidthChanged(int textWidth);

protected slots:
    void transpose();
    void updateDimensions();

private:
    KnobView* mParticleSlider;
    KnobView* mTextSlider;
    Qt::Orientation mOrientation;
    int mTextWidth {0};

};

//==============
// SimpleSlider
//==============

class SimpleSlider : public MultiSlider {

    Q_OBJECT

public:
    explicit SimpleSlider(Qt::Orientation orientation, QWidget* parent);

    ParticleKnob* particle();

    size_t cardinality() const;
    void setCardinality(size_t cardinality);

    qreal defaultRatio() const;
    void setDefaultRatio(qreal ratio);

    qreal ratio() const;
    void setRatio(qreal ratio);
    void setClampedRatio(qreal ratio);
    void setDefault();
    void setText(const QString& text);

protected slots:
    void onKnobClick(Qt::MouseButton button);
    void onKnobMove();

signals:
    void knobChanged(qreal ratio);
    void knobMoved(qreal ratio);

private:
    qreal mDefaultRatio {0.};
    Unit mUnit;

};

//==============
// RangedSlider
//==============

template<typename RangeT>
class RangedSlider : public SimpleSlider {

public:
    using value_type = typename RangeT::value_type;
    using formatter_type = std::function<QString(value_type)>;
    using notifier_type = std::function<void(value_type)>;

    explicit RangedSlider(RangeT range, Qt::Orientation orientation, QWidget* parent) :
        SimpleSlider{orientation, parent}, mRange{std::move(range)} {

        if (std::is_integral<value_type>::value)
            setCardinality(span(mRange) + 1);

        connect(this, &RangedSlider::knobChanged, this, &RangedSlider::onKnobChange);
        connect(this, &RangedSlider::knobMoved, this, &RangedSlider::onKnobChange);
    }

    const formatter_type& formatter() const {
        return mFormatter;
    }

    void setFormatter(formatter_type formatter) {
        mFormatter = std::move(formatter);
    }

    const notifier_type& notifier() const {
        return mNotifier;
    }

    void setNotifier(notifier_type notifier) {
        mNotifier = std::move(notifier);
    }

    value_type defaultValue() const {
        return expand(defaultRatio(), mRange);
    }

    void setDefaultValue(value_type value) {
        setDefaultRatio(reduce(mRange, value));
    }

    value_type value() const {
        return expand(ratio(), mRange);
    }

    void setValue(value_type value) {
        setRatio(reduce(mRange, value));
    }

    void setClampedValue(value_type value) {
        setClampedRatio(reduce(mRange, value));
    }

    void onKnobChange(qreal ratio) {
        const auto value = expand(ratio, mRange);
        if (mFormatter)
            setText(mFormatter(value));
        if (mNotifier)
            mNotifier(value);
    }

private:
    const RangeT mRange;
    formatter_type mFormatter;
    notifier_type mNotifier;

};

template<typename RangeT, typename ValueT>
auto* makeHorizontalSlider(RangeT range, ValueT defaultValue, QWidget* parent) {
    auto* slider = new RangedSlider<RangeT>{std::move(range), Qt::Horizontal, parent};
    slider->setTextWidth(35);
    slider->setDefaultValue(defaultValue);
    return slider;
}

using DiscreteSlider = RangedSlider<range_t<int>>;
using ContinuousSlider = RangedSlider<range_t<double>>;
using ExpSlider = RangedSlider<exp_range_t<double>>;

#endif // QTOOLS_MULTISLIDER_H
