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

#include "qhandlers/soundfont.h"

#ifdef MIDILAB_FLUIDSYNTH_VERSION

namespace {

template<typename T>
QString formatDefault(T value) {
    return QString::number(value);
}

template<>
QString formatDefault<double>(double value) {
    return QString::number(value, 'f', 2);
}

template<typename T>
auto makeSliderFromExtension(const SoundFontBoundedExtension<T>& ext, QWidget* parent) {
    auto* slider = makeHorizontalSlider(ext.range, ext.default_value, parent);
    slider->setFormatter(formatDefault<T>);
    slider->setDefault();
    return slider;
}

template<typename T>
auto makeExpSliderFromExtension(const SoundFontBoundedExtension<T>& ext, T pivot, QWidget* parent) {
    auto* slider = makeHorizontalSlider(exp_range_t<T>{ext.range, pivot}, ext.default_value, parent);
    slider->setFormatter(formatDefault<T>);
    slider->setDefault();
    return slider;
}

}

//======================
// SoundFontInterceptor
//======================

void SoundFontInterceptor::seize_messages(Handler* target, const Messages& messages) {
    const bool fileSeized = std::any_of(messages.begin(), messages.end(), [](const auto& message) {
        return message.event.is(family_t::extended_system) && SoundFontHandler::ext.file.affects(message.event);
    });
    seizeAll(target, messages);
    if (fileSeized)
        emit fileHandled();
}

//============
// GainEditor
//============

GainEditor::GainEditor(QWidget* parent) : QWidget{parent} {
    mSlider = makeExpSliderFromExtension(SoundFontHandler::ext.gain, 1., this);
    mSlider->setNotifier([this](double value) { emit gainChanged(value); });
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

double GainEditor::gain() const {
    return mSlider->value();
}

void GainEditor::setGain(double gain) {
    mSlider->setValue(gain);
}

// =============
// ReverbEditor
// =============

ReverbEditor::ReverbEditor(QWidget* parent) : FoldableGroupBox{"Reverb", parent} {

    setCheckable(true);
    setActivated(SoundFontHandler::ext.reverb.activated.default_value);

    mRoomsizeSlider = makeSliderFromExtension(SoundFontHandler::ext.reverb.roomsize, this);
    mDampSlider = makeSliderFromExtension(SoundFontHandler::ext.reverb.damp, this);
    mLevelSlider = makeSliderFromExtension(SoundFontHandler::ext.reverb.level, this);
    mWidthSlider = makeExpSliderFromExtension(SoundFontHandler::ext.reverb.width, 10., this);

    auto* form = new QFormLayout;
    form->setVerticalSpacing(0);
    form->addRow("Room Size", mRoomsizeSlider);
    form->addRow("Damp", mDampSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Width", mWidthSlider);

    auto* subWidget = new QWidget{this};
    subWidget->setLayout(form);
    setWidget(subWidget);

    connect(this, &ReverbEditor::toggled, this, &ReverbEditor::activatedChanged);
    mRoomsizeSlider->setNotifier([this](double value) { emit roomSizeChanged(value); });
    mDampSlider->setNotifier([this](double value) { emit dampChanged(value); });
    mLevelSlider->setNotifier([this](double value) { emit levelChanged(value); });
    mWidthSlider->setNotifier([this](double value) { emit widthChanged(value); });

}

bool ReverbEditor::activated() const {
    return isChecked();
}

double ReverbEditor::roomSize() const {
    return mRoomsizeSlider->value();
}

double ReverbEditor::damp() const {
    return mDampSlider->value();
}

double ReverbEditor::level() const {
    return mLevelSlider->value();
}

double ReverbEditor::width() const {
    return mWidthSlider->value();
}

void ReverbEditor::setActivated(bool value) {
    setChecked(value);
}

void ReverbEditor::setRoomSize(double value) {
    mRoomsizeSlider->setValue(value);
}

void ReverbEditor::setDamp(double value) {
    mDampSlider->setValue(value);
}

void ReverbEditor::setLevel(double value) {
    mLevelSlider->setValue(value);
}

void ReverbEditor::setWidth(double value) {
    mWidthSlider->setValue(value);
}

//==============
// ChorusEditor
//==============

ChorusEditor::ChorusEditor(QWidget* parent) : FoldableGroupBox{"Chorus", parent} {

    setCheckable(true);
    setActivated(SoundFontHandler::ext.chorus.activated.default_value);

    mTypeBox = new QComboBox{this};
    mTypeBox->addItem("Sine Wave");
    mTypeBox->addItem("Triangle Wave");
    setType(SoundFontHandler::ext.chorus.type.default_value);

    mNrSlider = makeSliderFromExtension(SoundFontHandler::ext.chorus.nr, this);
    mLevelSlider= makeExpSliderFromExtension(SoundFontHandler::ext.chorus.level, 1., this);
    mSpeedSlider= makeSliderFromExtension(SoundFontHandler::ext.chorus.speed, this);
    mDepthSlider= makeSliderFromExtension(SoundFontHandler::ext.chorus.depth, this);

    auto* form = new QFormLayout;
    form->setVerticalSpacing(0);
    form->addRow("Type", mTypeBox);
    form->addRow("NR", mNrSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Speed", mSpeedSlider);
    form->addRow("Depth", mDepthSlider);

    auto* subWidget = new QWidget{this};
    subWidget->setLayout(form);
    setWidget(subWidget);

    connect(this, &ChorusEditor::toggled, this, &ChorusEditor::activatedChanged);
    connect(mTypeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int value) { emit typeChanged(value); });
    mNrSlider->setNotifier([this](int value) { emit nrChanged(value); });
    mLevelSlider->setNotifier([this](double value) { emit levelChanged(value); });
    mSpeedSlider->setNotifier([this](double value) { emit speedChanged(value); });
    mDepthSlider->setNotifier([this](double value) { emit depthChanged(value); });

}

bool ChorusEditor::activated() const {
    return isChecked();
}

int ChorusEditor::type() const {
    return mTypeBox->currentIndex();
}

int ChorusEditor::nr() const {
    return mNrSlider->value();
}

double ChorusEditor::level() const {
    return mLevelSlider->value();
}

double ChorusEditor::speed() const {
    return mSpeedSlider->value();
}

double ChorusEditor::depth() const {
    return mDepthSlider->value();
}

void ChorusEditor::setActivated(bool value) {
    setChecked(value);
}

void ChorusEditor::setType(int value) {
    mTypeBox->setCurrentIndex(value);
}

void ChorusEditor::setNr(int value) {
    mNrSlider->setValue(value);
}

void ChorusEditor::setLevel(double value) {
    mLevelSlider->setValue(value);
}

void ChorusEditor::setSpeed(double value) {
    mSpeedSlider->setValue(value);
}

void ChorusEditor::setDepth(double value) {
    mDepthSlider->setValue(value);
}

//=================
// SoundFontEditor
//=================

MetaHandler* makeMetaSoundFont(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("SoundFont");
    meta->setFactory(new OpenProxyFactory<SoundFontEditor>);
    return meta;
}

SoundFontEditor::SoundFontEditor() : HandlerEditor{} {

    mInterceptor = new SoundFontInterceptor{this};
    mHandler.set_interceptor(mInterceptor);
    connect(mInterceptor, &SoundFontInterceptor::fileHandled, this, &SoundFontEditor::updateFile);

    mLoadMovie = new QMovie{":/data/load.gif", QByteArray{}, this};
    mLoadLabel = new QLabel{this};
    mLoadLabel->setMovie(mLoadMovie);
    mLoadLabel->hide();

    /// @todo add menu option to set path from a previous one (keep history)
    mFileEditor = new QLineEdit{this};
    mFileEditor->setMinimumWidth(200);
    mFileEditor->setReadOnly(true);
    mFileEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* fileSelector = new QToolButton{this};
    fileSelector->setToolTip("Browse SoundFonts");
    fileSelector->setAutoRaise(true);
    fileSelector->setIcon(QIcon{":/data/file.svg"});
    connect(fileSelector, &QToolButton::clicked, this, &SoundFontEditor::onClick);

    mGainEditor = new GainEditor{this};
    connect(mGainEditor, &GainEditor::gainChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.gain(value)); });

    mReverbEditor = new ReverbEditor{this};
    connect(mReverbEditor, &ReverbEditor::activatedChanged, this, [this](bool value) { mHandler.send_message(SoundFontHandler::ext.reverb.activated(value)); });
    connect(mReverbEditor, &ReverbEditor::roomSizeChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.reverb.roomsize(value)); });
    connect(mReverbEditor, &ReverbEditor::dampChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.reverb.damp(value)); });
    connect(mReverbEditor, &ReverbEditor::levelChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.reverb.level(value)); });
    connect(mReverbEditor, &ReverbEditor::widthChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.reverb.width(value)); });

    mChorusEditor = new ChorusEditor{this};
    connect(mChorusEditor, &ChorusEditor::activatedChanged, this, [this](bool value) { mHandler.send_message(SoundFontHandler::ext.chorus.activated(value)); });
    connect(mChorusEditor, &ChorusEditor::typeChanged, this, [this](int value) { mHandler.send_message(SoundFontHandler::ext.chorus.type(value)); });
    connect(mChorusEditor, &ChorusEditor::nrChanged, this, [this](int value) { mHandler.send_message(SoundFontHandler::ext.chorus.nr(value)); });
    connect(mChorusEditor, &ChorusEditor::levelChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.chorus.level(value)); });
    connect(mChorusEditor, &ChorusEditor::speedChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.chorus.speed(value)); });
    connect(mChorusEditor, &ChorusEditor::depthChanged, this, [this](double value) { mHandler.send_message(SoundFontHandler::ext.chorus.depth(value)); });

    auto* form = new QFormLayout;
    form->setMargin(0);
    form->addRow("File", make_hbox(spacing_tag{0}, fileSelector, mFileEditor, mLoadLabel));
    form->addRow("Gain", mGainEditor);
    setLayout(make_vbox(margin_tag{0}, form, mReverbEditor, mChorusEditor, stretch_tag{}));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum); // avoid vertical expansion

}

HandlerView::Parameters SoundFontEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("file", QString::fromStdString, mHandler.file(), result);
    SERIALIZE("gain", serial::serializeNumber, mGainEditor->gain(), result);
    SERIALIZE("reverb.active", serial::serializeBool, mReverbEditor->activated(), result);
    SERIALIZE("reverb.folded", serial::serializeBool, mReverbEditor->isFolded(), result);
    SERIALIZE("reverb.roomsize", serial::serializeNumber, mReverbEditor->roomSize(), result);
    SERIALIZE("reverb.damp", serial::serializeNumber, mReverbEditor->damp(), result);
    SERIALIZE("reverb.level", serial::serializeNumber, mReverbEditor->level(), result);
    SERIALIZE("reverb.width", serial::serializeNumber, mReverbEditor->width(), result);
    SERIALIZE("chorus.active", serial::serializeBool, mChorusEditor->activated(), result);
    SERIALIZE("chorus.folded", serial::serializeBool, mChorusEditor->isFolded(), result);
    SERIALIZE("chorus.type", serial::serializeNumber, mChorusEditor->type(), result);
    SERIALIZE("chorus.nr", serial::serializeNumber, mChorusEditor->nr(), result);
    SERIALIZE("chorus.level", serial::serializeNumber, mChorusEditor->level(), result);
    SERIALIZE("chorus.speed", serial::serializeNumber, mChorusEditor->speed(), result);
    SERIALIZE("chorus.depth", serial::serializeNumber, mChorusEditor->depth(), result);
    return result;
}

size_t SoundFontEditor::setParameter(const Parameter& parameter) {
    if (parameter.name == "file") {
        setFile(parameter.value);
        return 1;
    }
    UNSERIALIZE("gain", serial::parseDouble, mGainEditor->setGain, parameter);
    UNSERIALIZE("reverb.active", serial::parseBool, mReverbEditor->setActivated, parameter);
    UNSERIALIZE("reverb.folded", serial::parseBool, mReverbEditor->setFolded, parameter);
    UNSERIALIZE("reverb.roomsize", serial::parseDouble, mReverbEditor->setRoomSize, parameter);
    UNSERIALIZE("reverb.damp", serial::parseDouble, mReverbEditor->setDamp, parameter);
    UNSERIALIZE("reverb.level", serial::parseDouble, mReverbEditor->setLevel, parameter);
    UNSERIALIZE("reverb.width", serial::parseDouble, mReverbEditor->setWidth, parameter);
    UNSERIALIZE("chorus.active", serial::parseBool, mChorusEditor->setActivated, parameter);
    UNSERIALIZE("chorus.folded", serial::parseBool, mChorusEditor->setFolded, parameter);
    UNSERIALIZE("chorus.type", serial::parseInt, mChorusEditor->setType, parameter);
    UNSERIALIZE("chorus.nr", serial::parseInt, mChorusEditor->setNr, parameter);
    UNSERIALIZE("chorus.level", serial::parseDouble, mChorusEditor->setLevel, parameter);
    UNSERIALIZE("chorus.speed", serial::parseDouble, mChorusEditor->setSpeed, parameter);
    UNSERIALIZE("chorus.depth", serial::parseDouble, mChorusEditor->setDepth, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* SoundFontEditor::getHandler() {
    return &mHandler;
}

void SoundFontEditor::setFile(const QString& file) {
    mHandler.send_message(SoundFontHandler::ext.file(file.toStdString()));
    mLoadMovie->start();
    mLoadLabel->show();
}

void SoundFontEditor::updateFile() {
    const auto fileInfo = QFileInfo{QString::fromStdString(mHandler.file())};
    mFileEditor->setText(fileInfo.completeBaseName());
    mFileEditor->setToolTip(fileInfo.absoluteFilePath());
    mLoadMovie->stop();
    mLoadLabel->hide();
}

void SoundFontEditor::onClick() {
    const auto file = context()->pathRetrieverPool()->get("soundfont")->getReadFile(this);
    if (!file.isNull())
        setFile(file);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
