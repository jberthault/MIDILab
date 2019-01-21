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

#include <QButtonGroup>
#include <QPushButton>
#include <QColorDialog>
#include "qcore/editors.h"

//=============
// ColorPicker
//=============

ColorPicker::ColorPicker(channel_t channel, QWidget* parent) : QToolButton{parent}, mChannel{channel} {
    connect(this, &ColorPicker::clicked, this, &ColorPicker::selectColor);
    setAutoFillBackground(true);
}

const QColor& ColorPicker::color() const {
    return mColor;
}

void ColorPicker::selectColor() {
    setColor(QColorDialog::getColor(mColor, this));
}

void ColorPicker::setColor(const QColor& color) {
    if (color.isValid()) {
        mColor = color;
        setStyleSheet(QString{"ColorPicker {background : %1}"}.arg(color.name()));
        emit colorChanged(mChannel, mColor);
    }
}

//===============
// ChannelEditor
//===============

const channel_map_t<QColor> defaultColors = {
    "#ff0000", "#2e8b57", "#4169e1", "#ffa500",
    "#00ee22", "#40e0d0", "#da70d6", "#a0522d",
    "#eeee00", "#666666", "#b22222", "#88ff00",
    "#888800", "#ff0088", "#8800ff", "#d2691e"
};

ChannelEditor::ChannelEditor(QWidget* parent) : QWidget{parent} {

    setWindowTitle("Channel Colors");
    setWindowIcon(QIcon{":/data/brush.svg"});
    setWindowFlags(Qt::Dialog);

    setButton(Qt::LeftButton, channels_t::wrap(0x0));
    setButton(Qt::RightButton, channels_t::wrap(0x1));
    setButton(Qt::MidButton, channels_t::drums());
    setButton(Qt::XButton1, channels_t::wrap(0x2));
    setButton(Qt::XButton2, channels_t::wrap(0x3));

    auto* grid = new QGridLayout;
    grid->setMargin(0);
    grid->setSpacing(0);
    for (int n = 0 ; n < 4 ; n++) {
        auto* hor = new QLabel{QString::number(n), this};
        auto* ver = new QLabel{QString::number(4*n), this};
        hor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        ver->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        hor->setAlignment(Qt::AlignCenter);
        ver->setAlignment(Qt::AlignCenter);
        grid->addWidget(hor, 0, n+1);
        grid->addWidget(ver, n+1, 0);
    }
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c) {
        auto* picker = new ColorPicker{c, this};
        mPickers[c] = picker;
        grid->addWidget(picker, 1+c/4, 1+c%4);
        connect(picker, &ColorPicker::colorChanged, this, &ChannelEditor::colorChanged);
    }

    auto* reset = new QPushButton{"Reset", this};
    connect(reset, &QPushButton::clicked, this, &ChannelEditor::resetColors);
    resetColors();

    setLayout(make_vbox(make_hbox(stretch_tag{}, grid, stretch_tag{}), make_hbox(stretch_tag{}, reset)));
}

void ChannelEditor::resetColors() {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        setColor(c, defaultColors[c]);
}

void ChannelEditor::setColor(channel_t channel, const QColor& color) {
    mPickers[channel]->setColor(color);
}

const QColor& ChannelEditor::color(channel_t channel) const {
    return mPickers[channel]->color();
}

void ChannelEditor::setButton(Qt::MouseButton button, channels_t channels) {
    mMouse[button] = channels;
}

channels_t ChannelEditor::channelsFromButtons(Qt::MouseButtons buttons) const {
    channels_t channels;
    for (const auto& pair : mMouse)
        if (buttons & pair.first)
            channels |= pair.second;
    return channels;
}

QBrush ChannelEditor::brush(channels_t channels, Qt::Orientations orientations) const {
    size_t count = channels.size();
    if (count == 0)
        return {};
    if (count == 1)
        return {color(*channels.begin())};
    QLinearGradient gradient{0, 0, (qreal)(bool)(orientations & Qt::Horizontal), (qreal)(bool)(orientations & Qt::Vertical)};
    //gradient.setSpread(QGradient::RepeatSpread);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    int i=0;
    const qreal denum = count-1;
    for (auto channel : channels)
        gradient.setColorAt(i++/denum, color(channel));
    return {gradient};
}

//==================
// ChannelsSelector
//==================

QStringList ChannelsSelector::channelsToStringList(channels_t channels) {
    QStringList stringList;
    for (auto channel : channels)
        stringList.append(QString::number(channel));
    return stringList;
}

QString ChannelsSelector::channelsToString(channels_t channels) {
    QString result;
    if (channels.size() > 10) {
        result += "*";
        const auto stringList = channelsToStringList(~channels);
        if (!stringList.isEmpty())
            result += " \\ {" + stringList.join(", ") + "}";
    } else {
        const auto stringList = channelsToStringList(channels);
        const auto inner = stringList.empty() ? QString{"\u00d8"} : stringList.join(", "); // "\u2205"
        result += "{" + inner + "}";
    }
    return result;
}

/// @todo for consistency, set number around like the ChannelEditor

ChannelsSelector::ChannelsSelector(QWidget* parent) : QWidget{parent} {

    setWindowTitle("Channel Selector");

    auto* gridLayout = new QGridLayout;
    gridLayout->setSpacing(0);
    gridLayout->setMargin(0);

    mGroup = new QButtonGroup{this};
    mGroup->setExclusive(false);

    auto* triState = new TriState{"All", this};

    for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
        auto* checkbox = new QCheckBox{QString::number(c), this};
        triState->addCheckBox(checkbox);
        mGroup->addButton(checkbox);
        mBoxes[c] = checkbox;
        gridLayout->addWidget(checkbox, c/4, c%4);
    }

    connect(triState, &TriState::clicked, this, &ChannelsSelector::updateChannels);
    connect(mGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &ChannelsSelector::updateChannels);

    setLayout(make_vbox(gridLayout, make_hbox(triState, stretch_tag{})));
}

void ChannelsSelector::setChannelEditor(ChannelEditor* editor) {
    if (editor) {
        connect(editor, &ChannelEditor::colorChanged, this, &ChannelsSelector::setChannelColor);
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            setChannelColor(c, editor->color(c));
    }
}

bool ChannelsSelector::isUnique() const {
    return mGroup->exclusive();
}

void ChannelsSelector::setUnique(bool unique) {
    mGroup->setExclusive(unique);
    updateChannels();
}

channels_t ChannelsSelector::channels() const {
    return mChannels;
}

void ChannelsSelector::setChannels(channels_t channels) {
    if (channels != mChannels) {
        mChannels = channels;
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            mBoxes[c]->setChecked(mChannels.test(c));
        emit channelsChanged(mChannels);
    }
}

void ChannelsSelector::updateChannels() {
    const auto previousChannels = std::exchange(mChannels, {});
    for (channel_t c=0 ; c < channels_t::capacity() ; c++)
        if (mBoxes[c]->isChecked())
            mChannels.set(c);
    if (mChannels != previousChannels)
        emit channelsChanged(mChannels);
}

void ChannelsSelector::setChannelColor(channel_t channel, const QColor& color) {
    static const QString colorStyle{"QCheckBox{background-color: %1; padding: 5px;}"};
    mBoxes[channel]->setStyleSheet(colorStyle.arg(color.name()));
}

//=============
// ChannelKnob
//=============

ChannelKnob::ChannelKnob(channels_t channels) : ParticleKnob{6.}, mChannels{channels} {
    connect(this, &ChannelKnob::knobDoubleClicked, this, &ChannelKnob::onClick);
    connect(this, &ChannelKnob::knobMoved, this, &ChannelKnob::onMove);
    connect(this, &ChannelKnob::knobPressed, this, &ChannelKnob::onPress);
    connect(this, &ChannelKnob::knobReleased, this, &ChannelKnob::onRelease);
}

void ChannelKnob::onMove(qreal xratio, qreal yratio) {
    emit moved(mChannels, xratio, yratio);
}

void ChannelKnob::onClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        emit defaulted(mChannels);
    } else if (button == Qt::RightButton) {
        emit surselected(mChannels);
    }
}

void ChannelKnob::onPress(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        emit pressed(mChannels);
    } else if (button == Qt::RightButton) {
        emit selected(mChannels);
    }
}

void ChannelKnob::onRelease(Qt::MouseButton button) {
    if (button == Qt::LeftButton)
        emit released(mChannels);
}

//==================
// ChannelLabelKnob
//==================

ChannelLabelKnob::ChannelLabelKnob(channels_t channels) : mChannels{channels} {
    connect(this, &ChannelLabelKnob::knobDoubleClicked, this, &ChannelLabelKnob::onClick);
    connect(this, &ChannelLabelKnob::knobPressed, this, &ChannelLabelKnob::onPress);
}

void ChannelLabelKnob::onClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton) {
        emit defaulted(mChannels);
    } else if (button == Qt::RightButton) {
        emit surselected(mChannels);
    }
}

void ChannelLabelKnob::onPress(Qt::MouseButton button) {
    if (button == Qt::RightButton)
        emit selected(mChannels);
}

//================
// ChannelsSlider
//================

ChannelsSlider::ChannelsSlider(Qt::Orientation orientation, QWidget* parent) :
    MultiSlider{orientation, parent} {

    connect(particleSlider(), &KnobView::viewDoubleClicked, this, &ChannelsSlider::onViewClick);

    auto* groupKnob = new ChannelKnob{{}};
    groupKnob->setVisible(false);
    connect(groupKnob, &ChannelKnob::defaulted, this, &ChannelsSlider::onDefault);
    connect(groupKnob, &ChannelKnob::moved, this, &ChannelsSlider::onMove);
    connect(groupKnob, &ChannelKnob::pressed, this, &ChannelsSlider::onPress);
    connect(groupKnob, &ChannelKnob::released, this, &ChannelsSlider::onRelease);

    auto* groupLabel = new ChannelLabelKnob{{}};
    connect(groupLabel, &ChannelLabelKnob::defaulted, this, &ChannelsSlider::onDefault);

    mGroupUnit = insertUnit({groupKnob, groupLabel});

    for (channel_t c=0 ; c < channels_t::capacity() ; c++) {
        // knob
        auto* knob = new ChannelKnob{channels_t::wrap(c)};
        knob->setPen(QPen{Qt::black});
        connect(knob, &ChannelKnob::selected, this, &ChannelsSlider::onSelect);
        connect(knob, &ChannelKnob::surselected, this, &ChannelsSlider::onSurselect);
        connect(knob, &ChannelKnob::defaulted, this, &ChannelsSlider::onDefault);
        connect(knob, &ChannelKnob::moved, this, &ChannelsSlider::onMove);
        connect(knob, &ChannelKnob::pressed, this, &ChannelsSlider::onPress);
        connect(knob, &ChannelKnob::released, this, &ChannelsSlider::onRelease);
        // label
        auto* label = new ChannelLabelKnob{channels_t::wrap(c)};
        connect(label, &ChannelLabelKnob::selected, this, &ChannelsSlider::onSelect);
        connect(label, &ChannelLabelKnob::surselected, this, &ChannelsSlider::onSurselect);
        connect(label, &ChannelLabelKnob::defaulted, this, &ChannelsSlider::onDefault);
        mUnits[c] = insertUnit({knob, label}, 2., reduce(range_t<channel_t>{0, 0xf}, c));
    }

    updateDimensions();
}

void ChannelsSlider::setChannelEditor(ChannelEditor* editor) {
    if (editor) {
        connect(editor, &ChannelEditor::colorChanged, this, &ChannelsSlider::updateColor);
        for (channel_t c=0 ; c < channels_t::capacity() ; c++)
            updateColor(c, editor->color(c));
    }
}

qreal ChannelsSlider::defaultRatio() const {
    return mDefaultRatio;
}

void ChannelsSlider::setDefaultRatio(qreal ratio) {
    mDefaultRatio = ratio;
}

channels_t ChannelsSlider::selection() const {
    return mSelection;
}

void ChannelsSlider::setSelection(channels_t channels) {
    mSelection = channels;
    updatePen(channels_t::full());
}

channels_t ChannelsSlider::visibleChannels() const {
    return mVisibleChannels;
}

void ChannelsSlider::setVisibleChannels(channels_t channels) {
    if (mVisibleChannels != channels) {
        mVisibleChannels = channels;
        if (isExpanded()) {
            const range_t<size_t> visibleRange = {0, mVisibleChannels.size()-1};
            size_t v = 0;
            for (channel_t c=0 ; c < channels_t::capacity() ; ++c) {
                const bool visible = mVisibleChannels.test(c);
                if (visible)
                    setUnitRatio(mUnits[c], visibleRange ? reduce(visibleRange, v++) : .5);
                mUnits[c].particle->setVisible(visible);
            }
            updateDimensions();
        }
    }
}

void ChannelsSlider::setCardinality(size_t cardinality) {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        knobMainScale(mUnits[c].particle).cardinality = cardinality;
    knobMainScale(mGroupUnit.particle).cardinality = cardinality;
}

bool ChannelsSlider::isExpanded() const {
    return !mGroupUnit.particle->isVisible();
}

void ChannelsSlider::setExpanded(bool expanded) {
    if (isExpanded() == expanded)
        return;
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        mUnits[c].particle->setVisible(expanded && mVisibleChannels.test(c));
    mGroupUnit.particle->setVisible(!expanded);
    updateDimensions();
}

bool ChannelsSlider::isMovable() const {
    return mGroupUnit.particle->isMovable();
}

void ChannelsSlider::setMovable(bool movable) {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        mUnits[c].particle->setMovable(movable);
    mGroupUnit.particle->setMovable(movable);
}

qreal ChannelsSlider::ratio() const {
    return knobRatio(mGroupUnit.particle);
}

qreal ChannelsSlider::ratio(channel_t channel) const {
    return knobRatio(mUnits[channel].particle);
}

void ChannelsSlider::setRatio(channels_t channels, qreal ratio) {
    for (auto channel : channels)
        setKnobRatio(mUnits[channel].particle, ratio);
    if (matchSelection(channels))
        setKnobRatio(mGroupUnit.particle, ratio);
    emit knobChanged(channels);
}

void ChannelsSlider::setRatios(const channel_map_t<qreal>& ratios) {
    for (channel_t c=0 ; c < channels_t::capacity() ; ++c)
        setKnobRatio(mUnits[c].particle, ratios[c]);
    setKnobRatio(mGroupUnit.particle, mDefaultRatio);
    emit knobChanged(channels_t::full());
}

void ChannelsSlider::setDefault(channels_t channels) {
    setRatio(channels, mDefaultRatio);
}

void ChannelsSlider::setText(channels_t channels, const QString& text) {
    for (auto channel : channels)
        mUnits[channel].text->setText(text);
    if (matchSelection(channels))
        mGroupUnit.text->setText(text);
}

const QBrush& ChannelsSlider::groupColor() const {
    return mGroupUnit.particle->brush();
}

void ChannelsSlider::setGroupColor(const QBrush& brush) {
    mGroupUnit.particle->setBrush(brush);
}

void ChannelsSlider::updateColor(channel_t channel, const QColor& color) {
    mUnits[channel].particle->setBrush(color);
}

void ChannelsSlider::onDefault(channels_t channels) {
    channels = extend(channels);
    for (auto channel : channels)
        setKnobRatio(mUnits[channel].particle, mDefaultRatio);
    setKnobRatio(mGroupUnit.particle, mDefaultRatio);
    emit knobMoved(channels, mDefaultRatio);
}

void ChannelsSlider::onMove(channels_t channels, qreal xratio, qreal yratio) {
    const auto ratio = orientation() == Qt::Horizontal ? xratio : yratio;
    const auto extension = extend(channels);
    for (auto channel : extension & ~channels)
        setKnobRatio(mUnits[channel].particle, ratio);
    if (channels)
        setKnobRatio(mGroupUnit.particle, ratio);
    emit knobMoved(extension, ratio);
}

void ChannelsSlider::onPress(channels_t channels) {
    emit knobPressed(extend(channels));
}

void ChannelsSlider::onRelease(channels_t channels) {
    emit knobReleased(extend(channels));
}

void ChannelsSlider::onSelect(channels_t channels) {
    mSelection ^= channels;
    updatePen(channels);
}

void ChannelsSlider::onSurselect() {
    onSelect(channels_t::full());
}

void ChannelsSlider::onViewClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton)
        setExpanded(!isExpanded());
    else
        setOrientation(orientation() == Qt::Vertical ? Qt::Horizontal : Qt::Vertical);
}

void ChannelsSlider::updatePen(channels_t channels) {
    for (auto channel : channels) {
        QPen pen{Qt::black};
        if (mSelection.test(channel))
            pen.setWidth(2);
        mUnits[channel].particle->setPen(pen);
    }
}

bool ChannelsSlider::matchSelection(channels_t channels) const {
    return !channels || !mSelection || bool(channels & mSelection);
}

channels_t ChannelsSlider::extend(channels_t channels) const {
    return matchSelection(channels) ? channels | mSelection : channels;
}

//================
// FamilySelector
//================

FamilySelector::FamilySelector(QWidget* parent) : QTreeWidget{parent} {
    setWindowTitle("Type Filter");
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    auto* midiItem = makeNode(invisibleRootItem(), families_t::standard(), "MIDI Events");
    auto* voiceItem = makeNode(midiItem, families_t::standard_voice(), "Voice Events");
    auto* noteItem = makeNode(voiceItem, families_t::standard_note(), "Note Events");
    makeLeaves(noteItem, families_t::standard_note());
    makeLeaves(voiceItem, families_t::standard_voice() & ~families_t::standard_note());
    auto* systemItem = makeNode(midiItem, families_t::standard_system(), "System Events");
    makeLeaves(makeNode(systemItem, families_t::standard_system_common(), "System Common Events"), families_t::standard_system_common());
    makeLeaves(makeNode(systemItem, families_t::standard_system_realtime(), "System Realtime Events"), families_t::standard_system_realtime());
    makeLeaves(makeNode(midiItem, families_t::standard_meta(), "Meta Events"), families_t::standard_meta());
    connect(this, &QTreeWidget::itemChanged, this, &FamilySelector::onItemChange);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &FamilySelector::onItemClick);
    updateFamilies();
    midiItem->setExpanded(true);
    voiceItem->setExpanded(true);
    noteItem->setExpanded(true);
}

void FamilySelector::onItemChange(QTreeWidgetItem* item, int /*column*/) {
    auto checkState = item->checkState(0);
    if (checkState == Qt::PartiallyChecked)
        checkState = Qt::Checked;
    updateChildren(item, checkState);
    updateFamilies();
    emit familiesChanged(mFamilies);
}

void FamilySelector::onItemClick(QTreeWidgetItem* item, int column) {
    if (item->childCount() == 0)
        item->setCheckState(column, item->checkState(column) == Qt::Unchecked ? Qt::Checked : Qt::Unchecked);
}

void FamilySelector::setItemState(QTreeWidgetItem* item, Qt::CheckState checkState) {
    QSignalBlocker blocker{this};
    item->setCheckState(0, checkState);
    update(indexFromItem(item));
}

void FamilySelector::updateFamilies() {
    mFamilies = childFamilies(invisibleRootItem()->child(0));
}

void FamilySelector::updateChildren(QTreeWidgetItem* item, Qt::CheckState checkState) {
    setItemState(item, checkState);
    for (auto* child : makeChildRange(item))
        updateChildren(child, checkState);
    updateAncestors(item);
}

void FamilySelector::updateAncestors(QTreeWidgetItem* item) {
    if (auto* parent = item->parent()) {
        bool allChecked = true, anyChecked = false;
        for (auto* child : makeChildRange(parent)) {
            const auto subState = child->checkState(0);
            allChecked = allChecked && subState == Qt::Checked;
            anyChecked = anyChecked || subState != Qt::Unchecked;
        }
        auto parentState = Qt::Checked;
        if (!allChecked)
            parentState = anyChecked ? Qt::PartiallyChecked : Qt::Unchecked;
        setItemState(parent, parentState);
        updateAncestors(parent);
    }
}

QTreeWidgetItem* FamilySelector::makeNode(QTreeWidgetItem* root, families_t families, const QString& name) {
    auto* child = new QTreeWidgetItem{root, QStringList{} << name};
    child->setData(0, Qt::UserRole, static_cast<qulonglong>(families.to_integral()));
    return child;
}

void FamilySelector::makeLeaves(QTreeWidgetItem* root, families_t families) {
    for (auto family : families)
        makeNode(root, families_t::wrap(family), QString::fromLocal8Bit(family_name(family)));
}

families_t FamilySelector::families() const {
    return mFamilies;
}

void FamilySelector::setFamilies(families_t families) {
    if (families != mFamilies) {
        mFamilies = families;
        setChildFamilies(invisibleRootItem()->child(0), families);
        emit familiesChanged(mFamilies);
    }
}

families_t FamilySelector::childFamilies(QTreeWidgetItem* item) const {
    if (item->checkState(0) == Qt::Checked)
        return families_t::from_integral(item->data(0, Qt::UserRole).toULongLong());
    families_t result;
    if (item->checkState(0) == Qt::PartiallyChecked)
        for (auto* child : makeChildRange(item))
            result |= childFamilies(child);
    return result;
}

void FamilySelector::setChildFamilies(QTreeWidgetItem* item, families_t families) {
    const auto item_families = families_t::from_integral(item->data(0, Qt::UserRole).toULongLong());
    const auto intersection = item_families & families;
    auto checkState = Qt::Unchecked;
    if (intersection == item_families)
        checkState = Qt::Checked;
    else if (intersection)
        checkState = Qt::PartiallyChecked;
    setItemState(item, checkState);
    for (auto* child : makeChildRange(item))
        setChildFamilies(child, families);
}

//=================
// HandlerSelector
//=================

HandlerSelector::HandlerSelector(QWidget* parent) : QComboBox{parent} {
    connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &HandlerSelector::changeCurrentHandler);
}

Handler* HandlerSelector::currentHandler() {
    return handlerForIndex(currentIndex());
}

void HandlerSelector::setCurrentHandler(Handler* handler) {
    setCurrentIndex(indexForHandler(handler));
}

void HandlerSelector::changeCurrentHandler(int index) {
    emit handlerChanged(handlerForIndex(index));
}

void HandlerSelector::renameHandler(Handler* handler) {
    const int i = indexForHandler(handler);
    if (i != -1)
        setItemText(i, handlerName(handler));
}

void HandlerSelector::insertHandler(Handler* handler) {
    if (indexForHandler(handler) == -1 && handler != nullptr)
        addItem(handlerName(handler), QVariant::fromValue(handler));
}

void HandlerSelector::removeHandler(Handler* handler) {
    const int i = indexForHandler(handler);
    if (i != -1)
        removeItem(i);
}

int HandlerSelector::indexForHandler(Handler* handler) {
    if (handler != nullptr)
        for (int i=0 ; i < count() ; i++)
            if (handlerForIndex(i) == handler)
                return i;
    return -1;
}

Handler* HandlerSelector::handlerForIndex(int index) {
    if (0 <= index && index < count())
        return itemData(index).value<Handler*>();
    return nullptr;
}

//==============
// FilterEditor
//==============
