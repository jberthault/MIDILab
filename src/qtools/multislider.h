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

#ifndef QTOOLS_MULTISLIDER_H
#define QTOOLS_MULTISLIDER_H

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QBoxLayout>
#include "tools/bytes.h"


struct Scale {

    using discrete_type = int;
    using internal_type = qreal;
    using external_type = qreal;

    Scale();

    range_t<discrete_type> ticks() const; /*!< compute the range [0, cardinality) */
    discrete_type nearest() const; /*!< get the index of the nearest tick from current value */
    discrete_type nearest(internal_type v) const; /*!< get the index of the nearest tick */
    internal_type joint(discrete_type v) const; /*!< compute internal value from a tick index */

    range_t<external_type> adjusted() const; /*!< external range adjusted by reversed and margins */
    external_type upscale() const; /*!< upscale current value */
    external_type upscale(internal_type v) const; /*!< rescale value from internal to external range */
    internal_type downscale(external_type v) const; /*!< rescale value from external to internal range */
    external_type update(external_type v); /*!< update internal value and coerce external value to bounds */
    void clamp(internal_type v); /*!< set internal range fixed on the given value */

    internal_type value; /*!< current value (should be within the range) */
    range_t<internal_type> range; /*!< range in which value may evolve */
    size_t cardinality; /*!< if 0, continuous range, else, number of elements allowed in the range */

    range_t<external_type> externalRange; /*!< range in which value should be rescaled from and to */
    range_t<external_type> margins; /*!< reduced space within external range when rescaling */
    bool reversed; /*!< if true, the external range is reversed when rescaling */

};

//======
// Knob
//======

class Knob : public QGraphicsObject {

    Q_OBJECT

public:
    explicit Knob();

    const Scale& xScale() const;
    Scale& xScale();

    const Scale& yScale() const;
    Scale& yScale();

    void transpose();
    void moveToFit();

signals:
    void knobMoved(qreal xvalue, qreal yvalue);
    void knobPressed(Qt::MouseButton button);
    void knobReleased(Qt::MouseButton button);
    void knobDoubleClicked(Qt::MouseButton button);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;

private:
    Scale mXScale;
    Scale mYScale;
    bool mUpdatePosition;

};

//==============
// ParticleKnob
//==============

class ParticleKnob : public Knob {

    Q_OBJECT

    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QBrush color READ color WRITE setColor)
    Q_PROPERTY(QSizeF size READ size WRITE setSize)

public:

    enum class shape_type {
        round_rect,
        rect,
        ellipse
    };

    explicit ParticleKnob();

    shape_type particleShape() const;
    void setParticleShape(shape_type shape);

    const QPen& pen() const;
    void setPen(const QPen& pen);

    const QBrush& color() const;
    void setColor(const QBrush& brush);

    const QSizeF& size() const;
    void setSize(const QSizeF& size);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    shape_type mShape;
    QPen mPen;
    QBrush mColor;
    QSizeF mSize;

};

//=============
// BracketKnob
//=============

class BracketKnob : public Knob {

    Q_OBJECT

    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QBoxLayout::Direction direction READ direction WRITE setDirection)

public:
    explicit BracketKnob(QBoxLayout::Direction direction = QBoxLayout::LeftToRight);

    const QPen& pen() const;
    void setPen(const QPen& pen);

    QBoxLayout::Direction direction() const;
    void setDirection(QBoxLayout::Direction direction);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QPen mPen;
    QBoxLayout::Direction mDirection;
    QPainterPath mPath;

};

//===========
// ArrowKnob
//===========

class ArrowKnob : public Knob {

    Q_OBJECT

    Q_PROPERTY(QBrush color READ color WRITE setColor)
    Q_PROPERTY(QBoxLayout::Direction direction READ direction WRITE setDirection)

public:
    explicit ArrowKnob(QBoxLayout::Direction direction = QBoxLayout::LeftToRight);

    const QBrush& color() const;
    void setColor(const QBrush& brush);

    QBoxLayout::Direction direction() const;
    void setDirection(QBoxLayout::Direction direction);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QBrush mColor;
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

public:
    explicit KnobView(QWidget* parent);

    QRectF visibleRect() const;

    template<typename T = Knob>
    QList<T*> knobs() const {
        QList<T*> knobs;
        for (QGraphicsItem* item : scene()->items()) {
            T* knob = dynamic_cast<T*>(item);
            if (knob)
                knobs.append(knob);
        }
        return knobs;
    }

    void insertKnob(Knob* knob);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void setVisibleRect(Knob* knob, const QRectF& rect);

signals:
    void viewDoubleClicked(Qt::MouseButton button);

};


//================
// ChannelsSlider
//================

class MultiSlider : public QWidget {

    Q_OBJECT

public:
    explicit MultiSlider(QWidget* parent);

    KnobView* particleSlider() const;
    KnobView* textSlider() const;

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    int textWidth() const;
    void setTextWidth(int textWidth);

    Scale& knobScale(Knob* knob) const;
    qreal knobRatio(Knob* knob) const;
    void setKnobRatio(Knob* knob, qreal ratio) const;
    void insertKnob(ParticleKnob* particleKnob, TextKnob* textKnob);

signals:
    void orientationChanged(Qt::Orientation orientation);
    void textWidthChanged(int textWidth);

protected slots:
    void updateOrientation();
    void updateDimensions();
    void updateTextDimensions();

private:
    Qt::Orientation mOrientation;
    int mTextWidth;
    KnobView* mParticleSlider;
    KnobView* mTextSlider;

};

//==============
// SimpleSlider
//==============

class SimpleSlider : public MultiSlider {

    Q_OBJECT

public:
    explicit SimpleSlider(QWidget* parent);

    qreal defaultRatio() const;
    void setDefaultRatio(qreal ratio);

    qreal ratio() const;
    void setRatio(qreal ratio);
    void setDefault();
    void setText(const QString& text);

protected slots:
    void onKnobClick(Qt::MouseButton button);
    void onKnobMove();

signals:
    void knobChanged(qreal ratio);
    void knobMoved(qreal ratio);

private:
    qreal mDefaultRatio;
    ParticleKnob* mParticle;
    TextKnob* mText;

};

#endif // QTOOLS_MULTISLIDER_H
