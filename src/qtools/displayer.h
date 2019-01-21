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

#ifndef QTOOLS_DISPLAYER_H
#define QTOOLS_DISPLAYER_H

#include <QWidget>
#include <QFrame>
#include <QMouseEvent>
#include <QBoxLayout>
#include <QToolButton>
#include <QScrollArea>

//==============
// DragDetector
//==============

class DragDetector : public QObject {

    Q_OBJECT

public:
    using QObject::QObject;

    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void dragRequest();

private:
    bool farEnough(const QPoint& point);

    QPoint mStartPosition;
    bool mHasPosition {false};

};

//==========
// Scroller
//==========

class Scroller : public QWidget {

    Q_OBJECT

public:
    explicit Scroller(QWidget* widget, QWidget* parent);
    ~Scroller();

public:
    bool isScrolling() const;
    QWidget* widget();
    QScrollArea* scrollArea();

public slots:
    void setScrolling(bool scrolling);

private:
    QWidget* mWidget;
    QScrollArea* mArea;
    bool mScrolling;

};

//============
// Receptacle
//============

/**
 * A receptacle is a widget able to receive drops of widgets
 */

class Receptacle : public QWidget {

    Q_OBJECT

public:
    static const int nullPosition = -1;

    explicit Receptacle(Qt::Orientation orientation, QWidget* parent);
    ~Receptacle();

    const QStringList& mimeTypes() const;
    void setMimeTypes(const QStringList& mimeTypes);
    bool isSupported(const QMimeData* mimeData) const;

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    template<typename T = QWidget>
    auto widgets() const {
        std::vector<T*> result;
        result.reserve(mLayout->count());
        for (int i=0 ; i < mLayout->count() ; i++)
            if (auto* widget = dynamic_cast<T*>(mLayout->itemAt(i)->widget()))
                result.push_back(widget);
        return result;
    }

    bool insertWidget(QWidget* widget, int position = nullPosition);

signals:
    void widgetInserted(QWidget* widget, int position);

protected:
    void dropEvent(QDropEvent* de) override;
    void dragEnterEvent(QDragEnterEvent* de) override;
    void dragMoveEvent(QDragMoveEvent* de) override;
    void dragLeaveEvent(QDragLeaveEvent* de) override;

private:
    void clearPosition();
    void updatePosition(const QPoint& cursor);
    void moveLine(int position);
    void removeLine();
    void insertLine(int position);

private:
    QStringList mMimeTypes;
    QBoxLayout* mLayout;
    QFrame* mLine;
    Qt::Orientation mOrientation;
    int mPosition {nullPosition};

};

//===========
// Displayer
//===========

/**
 * @brief The Displayer class wraps Widget
 * so they can be rearranged by user on gui.
 * This is the common base for widgets that can be moved on a layer
 */

class Displayer : public QFrame {

    Q_OBJECT

public:
    static const QString mimeType;

    using QFrame::QFrame;

    virtual bool isLocked() const = 0;
    virtual void setLocked(bool locked) = 0;

    bool isEmpty() const; /*!< true if there is no living child of type SingleDisplayer */
    bool isEmbedded() const; /*!< true if one of its parent is a displayer */
    bool isIndependent() const; /*!< true if this is a window or if embedded */
    Displayer* nearestAncestor() const;

signals:
    void lockChanged(bool locked);

public slots:
    void drag();
    void deleteLaterRecursive();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    bool mDying {false};

};

//=================
// SingleDisplayer
//=================

class SingleDisplayer : public Displayer {

    Q_OBJECT

public:
    explicit SingleDisplayer(QWidget* parent);

    bool isLocked() const override;
    void setLocked(bool locked) override;

    QWidget* widget();
    void setWidget(QWidget* widget);

private slots:
    void onPress();

private:
    QWidget* mWidget {nullptr};
    QToolButton* mMove;

};

//================
// MultiDisplayer
//================

class MultiDisplayer : public Displayer {

    Q_OBJECT

public:
    static std::vector<MultiDisplayer*> topLevelDisplayers();

    explicit MultiDisplayer(Qt::Orientation orientation, QWidget* parent);
    ~MultiDisplayer();

    bool isLocked() const override;
    void setLocked(bool locked) override;

    bool isStreched() const;
    void setStretched(bool streched);

    bool isScrolling() const;
    void setScrolling(bool scrolling);

    Qt::Orientation orthogonal() const; /*!< returns the orthogonal orientation */
    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    SingleDisplayer* insertSingle(int position = Receptacle::nullPosition);
    MultiDisplayer* insertMulti(int position = Receptacle::nullPosition);
    MultiDisplayer* insertDetached(Qt::Orientation orientation = Qt::Vertical); /*!< @note lifetime management is up to the caller */

    std::vector<Displayer*> directChildren();

private slots:
    void toggleStretched();
    void toggleScrolling();
    void toggleOrientation();
    void onWidgetInsertion(QWidget* widget);
    void onInsertionRequest();
    void changeTitle();
    void onDeleteRequest();

private:
    void updateLocked(bool locked);

private:
    Scroller* mScroller;
    QHBoxLayout* mStretchLayout;
    Receptacle* mReceptacle;
    bool mStretched {false};

};

#endif // QTOOLS_DISPLAYER_H
