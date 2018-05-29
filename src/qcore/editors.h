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

#ifndef QCORE_EDITORS_H
#define QCORE_EDITORS_H

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTreeWidget>
#include <QToolButton>
#include "qcore/core.h"
#include "qtools/multislider.h"

//=============
// ColorPicker
//=============

class ColorPicker : public QToolButton {

    Q_OBJECT

public:
    explicit ColorPicker(channel_t channel, QWidget* parent);

    const QColor& color() const;
    void setColor(const QColor& color);

signals:
    void colorChanged(channel_t channel, const QColor& color);

private slots:
    void selectColor();

private:
    QColor mColor;
    channel_t mChannel;

};

//===============
// ChannelEditor
//===============

class ChannelEditor : public QWidget {

    Q_OBJECT

public:
    explicit ChannelEditor(QWidget* parent);

public slots:
    // channels relationship with colors
    void resetColors();
    void setColor(channel_t channel, const QColor& color);
    const QColor& color(channel_t channel) const;
    QBrush brush(channels_t channels, Qt::Orientations orientations = Qt::Vertical) const;

    // channels relationship with buttons
    void setButton(Qt::MouseButton button, channels_t channels);
    channels_t channelsFromButtons(Qt::MouseButtons buttons) const;

signals:
    void colorChanged(channel_t channel, const QColor& color);

private:
    channel_map_t<ColorPicker*> mPickers;
    QMap<Qt::MouseButton, channels_t> mMouse;

};

//==================
// ChannelsSelector
//==================

class ChannelsSelector : public QWidget {

    Q_OBJECT

public:
    static QStringList channelsToStringList(channels_t channels);
    static QString channelsToString(channels_t channels);

    explicit ChannelsSelector(QWidget* parent);

    void setChannelEditor(ChannelEditor* editor);

    bool isUnique() const;
    void setUnique(bool unique); /*!< default is false */

    channels_t channels() const;
    void setChannels(channels_t channels);

signals:
    void channelsChanged(channels_t channels);

private slots:
    void updateChannels();
    void setChannelColor(channel_t channel, const QColor& color);

private:
    channel_map_t<QCheckBox*> mBoxes;
    QButtonGroup* mGroup;
    channels_t mChannels;

};

//=============
// ChannelKnob
//=============

class ChannelKnob : public ParticleKnob {

    Q_OBJECT

public:
    explicit ChannelKnob(channels_t channels);

signals:
    void defaulted(channels_t channels);
    void pressed(channels_t channels);
    void moved(channels_t channels, qreal xratio, qreal yratio);
    void released(channels_t channels);
    void selected(channels_t channels);
    void surselected(channels_t channels);

private slots:
    void onMove(qreal xratio, qreal yratio);
    void onClick(Qt::MouseButton button);
    void onPress(Qt::MouseButton button);
    void onRelease(Qt::MouseButton button);

private:
    channels_t mChannels;

};

//==================
// ChannelLabelKnob
//==================

class ChannelLabelKnob : public TextKnob {

    Q_OBJECT

public:
    explicit ChannelLabelKnob(channels_t channels);

signals:
    void defaulted(channels_t channels);
    void selected(channels_t channels);
    void surselected(channels_t channels);

private slots:
    void onClick(Qt::MouseButton button);
    void onPress(Qt::MouseButton button);

private:
    channels_t mChannels;

};

//================
// ChannelsSlider
//================

class ChannelsSlider : public MultiSlider {

    Q_OBJECT

public:
    explicit ChannelsSlider(Qt::Orientation orientation, QWidget* parent);

    void setChannelEditor(ChannelEditor* editor);

    qreal defaultRatio() const;
    void setDefaultRatio(qreal ratio);

    channels_t selection() const;
    void setSelection(channels_t channels);

    void setCardinality(size_t cardinality);

    bool isExpanded() const;
    void setExpanded(bool expanded);

    qreal ratio() const;
    qreal ratio(channel_t channel) const;
    void setRatio(channels_t channels, qreal ratio);
    void setRatios(const channel_map_t<qreal>& ratios);
    void setDefault(channels_t channels);
    void setText(channels_t channels, const QString& text);

protected slots:
    void updateColor(channel_t channel, const QColor& color);
    void onDefault(channels_t channels);
    void onMove(channels_t channels, qreal xratio, qreal yratio);
    void onPress(channels_t channels);
    void onRelease(channels_t channels);
    void onSelect(channels_t channels);
    void onSurselect();
    void onViewClick(Qt::MouseButton button);
    void updatePen(channels_t channels);

protected:
    bool matchSelection(channels_t channels) const;
    channels_t extend(channels_t channels) const;

signals:
    void knobChanged(channels_t channels);
    void knobPressed(channels_t channels);
    void knobMoved(channels_t channels, qreal ratio);
    void knobReleased(channels_t channels);

private:
    ChannelEditor* mChannelEditor;
    qreal mDefaultRatio;
    channels_t mSelection;
    channel_map_t<ChannelKnob*> mKnobs;
    channel_map_t<ChannelLabelKnob*> mLabels;
    ChannelKnob* mGroupKnob;
    ChannelLabelKnob* mGroupLabel;

};

//================
// FamilySelector
//================

class FamilySelector : public QTreeWidget {

    Q_OBJECT

public:
    explicit FamilySelector(QWidget* parent);

public slots:
    families_t families() const;
    void setFamilies(families_t families);

signals:
    void familiesChanged(families_t families);

private slots:
    void onItemChange(QTreeWidgetItem* item, int column);
    void updateAncestors(QTreeWidgetItem* item);
    void updateChildren(QTreeWidgetItem* item, Qt::CheckState checkState);
    void setItemState(QTreeWidgetItem* item, Qt::CheckState checkState); // no signal
    void updateFamilies();

    families_t childFamilies(QTreeWidgetItem* item) const;
    void setChildFamilies(QTreeWidgetItem* item, families_t families);

private:
    QTreeWidgetItem* makeNode(QTreeWidgetItem* root, families_t families, const QString& name);
    void makeLeaves(QTreeWidgetItem* root, families_t families);

    families_t mFamilies;

};

//=================
// HandlerSelector
//=================

class HandlerSelector : public QComboBox {

    Q_OBJECT

public:
    explicit HandlerSelector(QWidget* parent);

public slots:
    Handler* currentHandler();
    void setCurrentHandler(Handler* handler);

    void renameHandler(Handler* handler);
    void insertHandler(Handler* handler);
    void removeHandler(Handler* handler);

signals:
    void handlerChanged(Handler* handler);

private slots:
    void changeCurrentHandler(int index);

private:
    int indexForHandler(Handler* handler);
    Handler* handlerForIndex(int index);

};

//=====================
// HandlerConfigurator
//=====================

/**
 * @todo ...
 * * add validator based on expected types
 * * use this class to reconfigure an existing handler
 *
 */

class HandlerConfigurator : public QWidget {

    Q_OBJECT

public:
    explicit HandlerConfigurator(MetaHandler* meta, QWidget* parent);

    QString name() const;
    HandlerView::Parameters parameters() const;

    void setFixedName(const QString& name);

private:
    QLineEdit* addField(const MetaHandler::MetaParameter& param);
    QLineEdit* addLine(const QString& label, const QString& tooltip, const QString& placeHolder);

    QFormLayout* mEditorsLayout;
    QLineEdit* mNameEditor;
    QMap<QString, QLineEdit*> mEditors;

};

//==============
// FilterEditor
//==============

/// @todo

#endif // QCORE_EDITORS_H
