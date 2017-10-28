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

#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include "editors.h"

using namespace family_ns;

//=============
// ColorPicker
//=============

ColorPicker::ColorPicker(channel_t channel, QWidget* parent) :
    QToolButton(parent), mChannel(channel) {

    connect(this, SIGNAL(clicked()), SLOT(selectColor()));
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
        setStyleSheet(QString("ColorPicker {background : %1}").arg(color.name()));
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

ChannelEditor::ChannelEditor(QWidget* parent) : QWidget(parent) {

    setWindowTitle("Channel Colors");
    setWindowIcon(QIcon(":/data/brush.svg"));

    setButton(Qt::LeftButton, channels_t::merge(0x0));
    setButton(Qt::RightButton, channels_t::merge(0x1));
    setButton(Qt::MidButton, drum_channels);
    setButton(Qt::XButton1, channels_t::merge(0x2));
    setButton(Qt::XButton2, channels_t::merge(0x3));

    QGridLayout* grid = new QGridLayout;
    grid->setMargin(0);
    grid->setSpacing(0);
    for (int n = 0 ; n < 4 ; n++) {
        QLabel* hor = new QLabel(QString::number(n), this);
        QLabel* ver = new QLabel(QString::number(4*n), this);
        hor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        ver->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        hor->setAlignment(Qt::AlignCenter);
        ver->setAlignment(Qt::AlignCenter);
        grid->addWidget(hor, 0, n+1);
        grid->addWidget(ver, n+1, 0);
    }
    for (channel_t c=0 ; c < 0x10 ; ++c) {
        ColorPicker* picker = new ColorPicker(c, this);
        mPickers[c] = picker;
        grid->addWidget(picker, 1+c/4, 1+c%4);
        connect(picker, &ColorPicker::colorChanged, this, &ChannelEditor::colorChanged);
    }

    QPushButton* reset = new QPushButton("Reset", this);
    connect(reset, SIGNAL(clicked()), SLOT(resetColors()));
    resetColors();

    setLayout(make_vbox(make_hbox(stretch_tag{}, grid, stretch_tag{}), make_hbox(stretch_tag{}, reset)));
}

void ChannelEditor::resetColors() {
    for (channel_t c=0 ; c < 0x10 ; ++c)
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
    QMapIterator<Qt::MouseButton, channels_t> it(mMouse);
    while (it.hasNext()) {
        it.next();
        if (buttons & it.key())
            channels |= it.value();
    }
    return channels;
}

QBrush ChannelEditor::brush(channels_t channels, Qt::Orientations orientations) const {
    size_t count = channels.size();
    if (count == 0)
        return {};
    if (count == 1)
        return {color(*channels.begin())};
    QLinearGradient gradient(0, 0, (qreal)(bool)(orientations & Qt::Horizontal), (qreal)(bool)(orientations & Qt::Vertical));
    //gradient.setSpread(QGradient::RepeatSpread);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    int i=0;
    qreal denum = count-1;
    for (channel_t channel : channels)
        gradient.setColorAt(i++/denum, color(channel));
    return {gradient};
}

//==================
// ChannelsSelector
//==================

QStringList ChannelsSelector::channelsToStringList(channels_t channels) {
    QStringList stringList;
    for (channel_t channel : channels)
        stringList.append(QString::number(channel));
    return stringList;
}

QString ChannelsSelector::channelsToString(channels_t channels) {
    QString result;
    if (channels.size() > 10) {
        result += "*";
        QStringList stringList = channelsToStringList(~channels);
        if (!stringList.isEmpty())
            result += " \\ {" + stringList.join(", ") + "}";
    } else {
        QStringList stringList = channelsToStringList(channels);
        QString inner = stringList.empty() ? QString("\u00d8") : stringList.join(", "); // "\u2205"
        result += "{" + inner + "}";
    }
    return result;
}

/// @todo for consistency, set number around like the ChannelEditor

ChannelsSelector::ChannelsSelector(QWidget* parent) : QWidget(parent) {

    setWindowTitle("Channel Selector");

    QGridLayout* gridLayout = new QGridLayout;
    gridLayout->setSpacing(0);
    gridLayout->setMargin(0);

    mGroup = new QButtonGroup(this);
    mGroup->setExclusive(false);

    TriState* triState = new TriState("All", this);

    for (channel_t c=0 ; c < 0x10 ; c++) {
        QCheckBox* checkbox = new QCheckBox(QString::number(c), this);
        triState->addCheckBox(checkbox);
        mGroup->addButton(checkbox);
        mBoxes[c] = checkbox;
        gridLayout->addWidget(checkbox, c/4, c%4);
    }

    connect(triState, &TriState::clicked, this, &ChannelsSelector::updateChannels);
    connect(mGroup, SIGNAL(buttonClicked(int)), SLOT(updateChannels()));

    setLayout(make_vbox(gridLayout, make_hbox(triState, stretch_tag{})));
}

void ChannelsSelector::setChannelEditor(ChannelEditor* editor) {
    if (editor) {
        connect(editor, &ChannelEditor::colorChanged, this, &ChannelsSelector::setChannelColor);
        for (channel_t c=0 ; c < 0x10 ; c++)
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
        for (channel_t c=0 ; c < 0x10 ; c++)
            mBoxes[c]->setChecked(mChannels.contains(c));
        emit channelsChanged(mChannels);
    }
}

void ChannelsSelector::updateChannels() {
    channels_t previousChannels = mChannels;
    mChannels = channels_t{};
    for (channel_t c=0 ; c < 0x10 ; c++)
        if (mBoxes[c]->isChecked())
            mChannels.set(c);
    if (mChannels != previousChannels)
        emit channelsChanged(mChannels);
}

void ChannelsSelector::setChannelColor(channel_t channel, const QColor& color) {
    static const QString colorStyle("QCheckBox{background-color: %1; padding: 5px;}");
    mBoxes[channel]->setStyleSheet(colorStyle.arg(color.name()));
}

//=============
// ChannelKnob
//=============

ChannelKnob::ChannelKnob(channels_t channels) : mChannels(channels) {
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

ChannelLabelKnob::ChannelLabelKnob(channels_t channels) : mChannels(channels) {
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
    MultiSlider(parent), mChannelEditor(nullptr), mDefaultRatio(0.) {

    connect(particleSlider(), &KnobView::viewDoubleClicked, this, &ChannelsSlider::onViewClick);

    mGroupKnob = new ChannelKnob({});
    mGroupKnob->setVisible(false);

    mGroupLabel = new ChannelLabelKnob({});
    mGroupLabel->setVisible(false);

    for (channel_t c=0 ; c < 0x10 ; c++) {
        // handle
        auto knob = new ChannelKnob(channels_t::merge(c));
        knob->setPen(QPen(Qt::black));
        connect(knob, &ChannelKnob::selected, this, &ChannelsSlider::onSelect);
        connect(knob, &ChannelKnob::surselected, this, &ChannelsSlider::onSurselect);
        // label
        auto label = new ChannelLabelKnob(channels_t::merge(c));
        connect(label, &ChannelLabelKnob::selected, this, &ChannelsSlider::onSelect);
        connect(label, &ChannelLabelKnob::surselected, this, &ChannelsSlider::onSurselect);
        mKnobs[c] = knob;
        mLabels[c] = label;
    }

    configure(mGroupKnob, mGroupLabel, .5);
    for (channel_t c=0 ; c < 0x10 ; c++)
        configure(mKnobs[c], mLabels[c], (qreal)c / 0xf);

    if (orientation == Qt::Vertical)
        setOrientation(Qt::Vertical);
    else
        updateDimensions();
}

void ChannelsSlider::setChannelEditor(ChannelEditor* editor) {
    mChannelEditor = editor;
    if (editor) {
        for (channel_t c=0 ; c < 0x10 ; c++) {
            connect(editor, &ChannelEditor::colorChanged, this, &ChannelsSlider::updateColor);
            updateColor(c, editor->color(c));
        }
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
    updatePen(all_channels);
}

void ChannelsSlider::setCardinality(size_t cardinality) {
    for (channel_t c=0 ; c < 0x10 ; ++c)
        knobScale(mKnobs[c]).cardinality = cardinality;
    knobScale(mGroupKnob).cardinality = cardinality;
}

bool ChannelsSlider::isExpanded() const {
    return !mGroupKnob->isVisible();
}

void ChannelsSlider::setExpanded(bool expanded) {
    if (isExpanded() == expanded)
        return;
    for (channel_t c=0 ; c < 0x10 ; ++c) {
        mKnobs[c]->setVisible(expanded);
        mLabels[c]->setVisible(expanded);
    }
    mGroupKnob->setVisible(!expanded);
    mGroupLabel->setVisible(!expanded);
    updateDimensions();
}

qreal ChannelsSlider::ratio() const {
    return knobRatio(mGroupKnob);
}

qreal ChannelsSlider::ratio(channel_t channel) const {
    return knobRatio(mKnobs[channel]);
}

void ChannelsSlider::setRatio(channels_t channels, qreal ratio) {
    for (channel_t channel : channels)
        setKnobRatio(mKnobs[channel], ratio);
    if (matchSelection(channels))
        setKnobRatio(mGroupKnob, ratio);
    emit knobChanged(channels);
}

void ChannelsSlider::setRatios(const channel_map_t<qreal>& ratios) {
    for (channel_t c=0 ; c < 0x10 ; ++c)
       setKnobRatio(mKnobs[c], ratios[c]);
    setKnobRatio(mGroupKnob, mDefaultRatio);
    emit knobChanged(all_channels);
}

void ChannelsSlider::setDefault(channels_t channels) {
    setRatio(channels, mDefaultRatio);
}

void ChannelsSlider::setText(channels_t channels, const QString& text) {
    for (channel_t channel : channels)
        mLabels[channel]->setText(text);
    if (matchSelection(channels))
        mGroupLabel->setText(text);
}

void ChannelsSlider::updateColor(channel_t channel, const QColor& color) {
    mKnobs[channel]->setColor(color);
}

void ChannelsSlider::onDefault(channels_t channels) {
    channels = extend(channels);
    for (channel_t channel : channels)
        setKnobRatio(mKnobs[channel], mDefaultRatio);
    setKnobRatio(mGroupKnob, mDefaultRatio);
    emit knobMoved(channels, mDefaultRatio);
}

void ChannelsSlider::onMove(channels_t channels, qreal xratio, qreal yratio) {
    qreal ratio = orientation() == Qt::Horizontal ? xratio : yratio;
    channels_t extension = extend(channels);
    for (channel_t channel : extension & ~channels)
        setKnobRatio(mKnobs[channel], ratio);
    if (channels)
        setKnobRatio(mGroupKnob, ratio);
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
    onSelect(all_channels);
}

void ChannelsSlider::onViewClick(Qt::MouseButton button) {
    if (button == Qt::LeftButton)
        setExpanded(!isExpanded());
    else
        setOrientation(orientation() == Qt::Vertical ? Qt::Horizontal : Qt::Vertical);
}

void ChannelsSlider::updatePen(channels_t channels) {
    for (channel_t channel : channels) {
        QPen pen(Qt::black);
        if (mSelection.contains(channel))
            pen.setWidth(2);
        mKnobs[channel]->setPen(pen);
    }
}

bool ChannelsSlider::matchSelection(channels_t channels) const {
    return !channels || !mSelection || bool(channels & mSelection);
}

channels_t ChannelsSlider::extend(channels_t channels) const {
    return matchSelection(channels) ? channels | mSelection : channels;
}

void ChannelsSlider::configure(ChannelKnob* knob, ChannelLabelKnob* label, qreal fixedRatio) {
    static const qreal radius = 6.;
    static const qreal margin = 8.; // 6 from radius + 2 from offset
    knob->setSize(QSizeF(2*radius, 2*radius));
    knob->xScale().margins = {margin, margin};
    knob->yScale().margins = {margin, margin};
    knob->yScale().clamp(fixedRatio);
    label->xScale().value = .5;
    label->yScale().margins = {margin, margin};
    label->yScale().clamp(fixedRatio);
    insertKnob(knob, label);
    connect(knob, &ChannelKnob::defaulted, this, &ChannelsSlider::onDefault);
    connect(knob, &ChannelKnob::moved, this, &ChannelsSlider::onMove);
    connect(knob, &ChannelKnob::pressed, this, &ChannelsSlider::onPress);
    connect(knob, &ChannelKnob::released, this, &ChannelsSlider::onRelease);
    connect(label, &ChannelLabelKnob::defaulted, this, &ChannelsSlider::onDefault);
}

//================
// FamilySelector
//================

FamilySelector::FamilySelector(QWidget* parent) : QTreeWidget(parent) {

    setWindowTitle("Type Filter");
    setAlternatingRowColors(true);
    setHeaderHidden(true);

    QTreeWidgetItem* midiItem = makeNode(invisibleRootItem(), midi_families, "MIDI Events");
    QTreeWidgetItem* voiceItem = makeNode(midiItem, voice_families, "Voice Events");
    QTreeWidgetItem* noteItem = makeNode(voiceItem, note_families, "Note Events");
    makeLeaf(noteItem, family_t::note_off);
    makeLeaf(noteItem, family_t::note_on);
    makeLeaf(noteItem, family_t::aftertouch);
    makeLeaf(voiceItem, family_t::controller);
    makeLeaf(voiceItem, family_t::program_change);
    makeLeaf(voiceItem, family_t::channel_pressure);
    makeLeaf(voiceItem, family_t::pitch_wheel);
    QTreeWidgetItem* systemItem = makeNode(midiItem, system_families, "System Events");
    QTreeWidgetItem* systemCommonItem = makeNode(systemItem, system_common_families, "System Common Events");
    makeLeaf(systemCommonItem, family_t::sysex);
    makeLeaf(systemCommonItem, family_t::mtc_frame);
    makeLeaf(systemCommonItem, family_t::song_position);
    makeLeaf(systemCommonItem, family_t::song_select);
    makeLeaf(systemCommonItem, family_t::xf4);
    makeLeaf(systemCommonItem, family_t::xf5);
    makeLeaf(systemCommonItem, family_t::tune_request);
    makeLeaf(systemCommonItem, family_t::end_of_sysex);
    QTreeWidgetItem* systemRealtimeItem = makeNode(systemItem, system_realtime_families, "System Realtime Events");
    makeLeaf(systemRealtimeItem, family_t::clock);
    makeLeaf(systemRealtimeItem, family_t::tick);
    makeLeaf(systemRealtimeItem, family_t::start);
    makeLeaf(systemRealtimeItem, family_t::continue_);
    makeLeaf(systemRealtimeItem, family_t::stop);
    makeLeaf(systemRealtimeItem, family_t::xfd);
    makeLeaf(systemRealtimeItem, family_t::active_sense);
    makeLeaf(systemRealtimeItem, family_t::reset);
    QTreeWidgetItem* metaItem = makeNode(midiItem, meta_families, "Meta Events");
    makeLeaf(metaItem, family_t::sequence_number);
    makeLeaf(metaItem, family_t::text);
    makeLeaf(metaItem, family_t::copyright);
    makeLeaf(metaItem, family_t::track_name);
    makeLeaf(metaItem, family_t::instrument_name);
    makeLeaf(metaItem, family_t::lyrics);
    makeLeaf(metaItem, family_t::marker);
    makeLeaf(metaItem, family_t::cue_point);
    makeLeaf(metaItem, family_t::program_name);
    makeLeaf(metaItem, family_t::device_name);
    makeLeaf(metaItem, family_t::channel_prefix);
    makeLeaf(metaItem, family_t::port);
    makeLeaf(metaItem, family_t::end_of_track);
    makeLeaf(metaItem, family_t::tempo);
    makeLeaf(metaItem, family_t::smpte_offset);
    makeLeaf(metaItem, family_t::time_signature);
    makeLeaf(metaItem, family_t::key_signature);
    makeLeaf(metaItem, family_t::proprietary);
    makeLeaf(metaItem, family_t::default_meta);

    connect(this, &QTreeWidget::itemChanged, this, &FamilySelector::onItemChange);

    updateFamilies();
}

void FamilySelector::onItemChange(QTreeWidgetItem* item, int /*column*/) {
    Qt::CheckState checkState = item->checkState(0);
    if (checkState == Qt::PartiallyChecked)
        checkState = Qt::Checked;
    updateChildren(item, checkState);
    updateFamilies();
    emit familiesChanged(mFamilies);
}

void FamilySelector::setItemState(QTreeWidgetItem* item, Qt::CheckState checkState) {
    QSignalBlocker blocker(this);
    item->setCheckState(0, checkState);
    update(indexFromItem(item));
}

void FamilySelector::updateFamilies() {
    mFamilies = childFamilies(invisibleRootItem()->child(0));
}

void FamilySelector::updateChildren(QTreeWidgetItem* item, Qt::CheckState checkState) {
    setItemState(item, checkState);
    for (int row=0 ; row < item->childCount() ; row++)
        updateChildren(item->child(row), checkState);
    updateAncestors(item);
}

void FamilySelector::updateAncestors(QTreeWidgetItem* item) {
    QTreeWidgetItem* parent = item->parent();
    if (!parent)
        return;
    bool allChecked = true, anyChecked = false;
    for (int row=0 ; row < parent->childCount() ; row++) {
        Qt::CheckState subState = parent->child(row)->checkState(0);
        allChecked = allChecked && subState == Qt::Checked;
        anyChecked = anyChecked || subState != Qt::Unchecked;
    }
    Qt::CheckState parentState = Qt::Checked;
    if (!allChecked)
        parentState = anyChecked ? Qt::PartiallyChecked : Qt::Unchecked;
    setItemState(parent, parentState);
    updateAncestors(parent);
}

QTreeWidgetItem* FamilySelector::makeNode(QTreeWidgetItem* root, families_t families, const QString& name) {
    QStringList texts;
    texts << name;
    QTreeWidgetItem* child = new QTreeWidgetItem(root, texts);
    child->setData(0, Qt::UserRole, families.to_integral());
    return child;
}

QTreeWidgetItem* FamilySelector::makeLeaf(QTreeWidgetItem* root, family_t family) {
    return makeNode(root, families_t::merge(family), QString::fromStdString(family_tools::info(family).name));
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
        for (int row = 0 ; row < item->childCount() ; row++)
            result |= childFamilies(item->child(row));
    return result;
}

void FamilySelector::setChildFamilies(QTreeWidgetItem* item, families_t families) {
    auto item_families = families_t::from_integral(item->data(0, Qt::UserRole).toULongLong());
    auto intersection = item_families & families;
    Qt::CheckState checkState = Qt::Unchecked;
    if (intersection == item_families)
        checkState = Qt::Checked;
    else if (intersection)
        checkState = Qt::PartiallyChecked;
    setItemState(item, checkState);
    for (int row = 0 ; row < item->childCount() ; row++)
        setChildFamilies(item->child(row), families);
}

//=================
// HandlerSelector
//=================

HandlerSelector::HandlerSelector(QWidget* parent) : QComboBox(parent) {
    connect(this, SIGNAL(currentIndexChanged(int)), SLOT(changeCurrentHandler(int)));
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
    int i = indexForHandler(handler);
    if (i != -1)
        setItemText(i, handlerName(handler));
}

void HandlerSelector::insertHandler(Handler* handler) {
    if (indexForHandler(handler) == -1 && handler != nullptr)
        addItem(handlerName(handler), QVariant::fromValue(handler));
}

void HandlerSelector::removeHandler(Handler* handler) {
    int i = indexForHandler(handler);
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

//=====================
// HandlerConfigurator
//=====================

HandlerConfigurator::HandlerConfigurator(MetaHandler* meta, QWidget* parent) : QWidget(parent) {
    setWindowTitle("Hander Configurator");

    QString identifier = metaHandlerName(meta);

    QLabel* label = new QLabel(QString("<b>%1</b>").arg(identifier), this);
    label->setAlignment(Qt::AlignHCenter);
    label->setToolTip(meta->description());

    mEditorsLayout = new QFormLayout;

    mNameEditor = addLine("name", "handler's name", identifier);
    mGroupEditor = addLine("group", "thread attribution", "default");
    for (const auto& param : meta->parameters())
        addField(param);

    setLayout(make_vbox(label, mEditorsLayout));
}

QString HandlerConfigurator::name() const {
    auto result = mNameEditor->text();
    if (result.isEmpty())
        return mNameEditor->placeholderText();
    return result;
}

QString HandlerConfigurator::group() const {
    auto result = mGroupEditor->text();
    if (result.isEmpty())
        return mGroupEditor->placeholderText();
    return result;
}

HandlerView::Parameters HandlerConfigurator::parameters() const {
    HandlerView::Parameters result;
    QMapIterator<QString, QLineEdit*> it(mEditors);
    while (it.hasNext()) {
        it.next();
        auto text = it.value()->text();
        if (!text.isEmpty())
            result.push_back(HandlerView::Parameter{it.key(), text});
    }
    return result;
}

QLineEdit* HandlerConfigurator::addField(const MetaHandler::Parameter& param) {
    auto editor = addLine(param.name, param.description, param.defaultValue);
    mEditors.insert(param.name, editor);
    return editor;
}

QLineEdit* HandlerConfigurator::addLine(const QString& label, const QString& tooltip, const QString& placeHolder) {
    auto editor = new QLineEdit(this);
    editor->setPlaceholderText(placeHolder);
    editor->setToolTip(tooltip);
    mEditorsLayout->addRow(label, editor);
    return editor;
}

//==============
// FilterEditor
//==============
