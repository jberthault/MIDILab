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

#include "qhandlers/soundfont.h"

#ifdef MIDILAB_FLUIDSYNTH_VERSION

namespace {

template<typename RangeT, typename T>
auto makeSlider(RangeT range, T defaultValue, const std::string& format, QWidget* parent) {
    auto* slider = new RangedSlider<RangeT>{range, Qt::Horizontal, parent};
    slider->setFormat(format);
    slider->setTextWidth(35);
    slider->setDefaultValue(defaultValue);
    return slider;
}

}

//===============
// MetaSoundFont
//===============

MetaSoundFont::MetaSoundFont(QObject* parent) : OpenMetaHandler{parent} {
    setIdentifier("SoundFont");
}

void MetaSoundFont::setContent(HandlerProxy& proxy) {
    proxy.setContent(new SoundFontEditor);
}

//======================
// SoundFontInterceptor
//======================

void SoundFontInterceptor::seize_messages(Handler* target, const Messages& messages) {
    const bool fileSeized = std::any_of(messages.begin(), messages.end(), [](const auto& message) {
        return message.event.family() == family_t::custom && message.event.has_custom_key("SoundFont.file");
    });
    seizeAll(target, messages);
    if (fileSeized)
        emit fileHandled();
}

//============
// GainEditor
//============

GainEditor::GainEditor(QWidget* parent) : QWidget{parent} {
    mSlider = makeSlider(make_exp_range(0., 1., 10.), .2, "%|.2f|", this);
    connect(mSlider, &SimpleSlider::knobMoved, this, &GainEditor::notify);
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

void GainEditor::setGain(double gain) {
    mSlider->setValue(gain);
    emit gainChanged(gain);
}

void GainEditor::notify() {
    emit gainChanged(mSlider->value());
}

// =============
// ReverbEditor
// =============

ReverbEditor::ReverbEditor(QWidget* parent) : QWidget{parent} {
    const auto reverb = SoundFontHandler::default_reverb();
    mRoomsizeSlider = makeSlider(make_range(0., 1.2), reverb.roomsize, "%|.2f|", this);
    mDampSlider = makeSlider(make_range(0., 1.), reverb.damp, "%|.2f|", this);
    mLevelSlider = makeSlider(make_range(0., 1.), reverb.level, "%|.2f|", this);
    mWidthSlider = makeSlider(make_exp_range(0., 10., 100.), reverb.width, "%|.2f|", this);
    connect(mRoomsizeSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::notify);
    connect(mDampSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::notify);
    connect(mLevelSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::notify);
    connect(mWidthSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::notify);
    auto* form = new QFormLayout;
    form->setVerticalSpacing(5);
    form->addRow("Room Size", mRoomsizeSlider);
    form->addRow("Damp", mDampSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Width", mWidthSlider);
    setLayout(form);
}

SoundFontHandler::reverb_type ReverbEditor::reverb() const {
    return SoundFontHandler::reverb_type {
        mRoomsizeSlider->value(),
        mDampSlider->value(),
        mLevelSlider->value(),
        mWidthSlider->value()
    };
}

void ReverbEditor::setRoomSize(double value) {
    mRoomsizeSlider->setValue(value);
    notify();
}

void ReverbEditor::setDamp(double value) {
    mDampSlider->setValue(value);
    notify();
}

void ReverbEditor::setLevel(double value) {
    mLevelSlider->setValue(value);
    notify();
}

void ReverbEditor::setWidth(double value) {
    mWidthSlider->setValue(value);
    notify();
}

void ReverbEditor::setReverb(const SoundFontHandler::reverb_type& reverb) {
    mRoomsizeSlider->setValue(reverb.roomsize);
    mDampSlider->setValue(reverb.damp);
    mLevelSlider->setValue(reverb.level);
    mWidthSlider->setValue(reverb.width);
    emit reverbChanged(reverb);
}

void ReverbEditor::notify() {
    emit reverbChanged(reverb());
}

// =====================
// OptionalReverbEditor
// =====================

OptionalReverbEditor::OptionalReverbEditor(QWidget* parent) : FoldableGroupBox{"Reverb", parent} {
    setCheckable(true);
    setChecked(true);
    mReverbEditor = new ReverbEditor{this};
    setWidget(mReverbEditor);
    connect(this, &OptionalReverbEditor::toggled, this, &OptionalReverbEditor::notify);
    connect(mReverbEditor, &ReverbEditor::reverbChanged, this, &OptionalReverbEditor::notifyChecked);
}

SoundFontHandler::optional_reverb_type OptionalReverbEditor::reverb() const {
    if (isChecked())
        return {mReverbEditor->reverb()};
    return {};
}

ReverbEditor* OptionalReverbEditor::editor() {
    return mReverbEditor;
}

void OptionalReverbEditor::setReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    if (reverb) {
        {
            QSignalBlocker guard{this};
            setChecked(true);
        }
        mReverbEditor->setReverb(*reverb);
    } else {
        setChecked(false);
    }
}

void OptionalReverbEditor::notifyChecked() {
    if (auto value = reverb())
        emit reverbChanged(value);
}

void OptionalReverbEditor::notify() {
    emit reverbChanged(reverb());
}

//==============
// ChorusEditor
//==============

ChorusEditor::ChorusEditor(QWidget* parent) : QWidget{parent} {

    const auto chorus = SoundFontHandler::default_chorus();

    mTypeBox = new QComboBox(this);
    mTypeBox->addItem("Sine Wave");
    mTypeBox->addItem("Triangle Wave");
    mTypeBox->setCurrentIndex(chorus.type);

    mNrSlider = makeSlider(make_range(0, 99), chorus.nr, "%1%", this);
    mLevelSlider = makeSlider(make_exp_range(0., 1., 10.), chorus.level, "%|.2f|", this);
    mSpeedSlider = makeSlider(make_range(.3, 5.), chorus.speed, "%|.2f|", this);
    mDepthSlider = makeSlider(make_range(0., 10.), chorus.depth, "%|.2f|", this);

    connect(mTypeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(notify()));
    connect(mNrSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::notify);
    connect(mLevelSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::notify);
    connect(mSpeedSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::notify);
    connect(mDepthSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::notify);

    auto* form = new QFormLayout;
    form->setVerticalSpacing(5);
    form->addRow("Type", mTypeBox);
    form->addRow("NR", mNrSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Speed (Hz)", mSpeedSlider);
    form->addRow("Depth (ms)", mDepthSlider);
    setLayout(form);

}

SoundFontHandler::chorus_type ChorusEditor::chorus() const {
    return SoundFontHandler::chorus_type {
        mTypeBox->currentIndex(),
        mNrSlider->value(),
        mLevelSlider->value(),
        mSpeedSlider->value(),
        mDepthSlider->value()
    };
}

void ChorusEditor::setType(int value) {
    mTypeBox->setCurrentIndex(value);
}

void ChorusEditor::setNr(int value) {
    mNrSlider->setValue(value);
    notify();
}

void ChorusEditor::setLevel(double value) {
    mLevelSlider->setValue(value);
    notify();
}

void ChorusEditor::setSpeed(double value) {
    mSpeedSlider->setValue(value);
    notify();
}

void ChorusEditor::setDepth(double value) {
    mDepthSlider->setValue(value);
    notify();
}

void ChorusEditor::setChorus(const SoundFontHandler::chorus_type& chorus) {
    {
        QSignalBlocker guard{mTypeBox};
        mTypeBox->setCurrentIndex(chorus.type);
    }
    mNrSlider->setValue(chorus.nr);
    mLevelSlider->setValue(chorus.level);
    mSpeedSlider->setValue(chorus.speed);
    mDepthSlider->setValue(chorus.depth);
    emit chorusChanged(chorus);
}

void ChorusEditor::notify() {
    emit chorusChanged(chorus());
}

//======================
// OptionalChorusEditor
//======================

OptionalChorusEditor::OptionalChorusEditor(QWidget* parent) : FoldableGroupBox{"Chorus", parent} {
    setCheckable(true);
    setChecked(true);
    mChorusEditor = new ChorusEditor{this};
    setWidget(mChorusEditor);
    connect(this, &OptionalChorusEditor::toggled, this, &OptionalChorusEditor::notify);
    connect(mChorusEditor, &ChorusEditor::chorusChanged, this, &OptionalChorusEditor::notifyChecked);
}

SoundFontHandler::optional_chorus_type OptionalChorusEditor::chorus() const {
    if (isChecked())
        return {mChorusEditor->chorus()};
    return {};
}

ChorusEditor* OptionalChorusEditor::editor() {
    return mChorusEditor;
}

void OptionalChorusEditor::setChorus(const SoundFontHandler::optional_chorus_type& chorus) {
    if (chorus) {
        {
            QSignalBlocker guard{this};
            setChecked(true);
        }
        mChorusEditor->setChorus(*chorus);
    } else {
        setChecked(false);
    }
}

void OptionalChorusEditor::notifyChecked() {
    if (auto value = chorus())
        emit chorusChanged(value);
}

void OptionalChorusEditor::notify() {
    emit chorusChanged(chorus());
}

//=================
// SoundFontEditor
//=================

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
    mFileEditor->setText(QString::fromStdString(mHandler.file()));

    auto* fileSelector = new QToolButton{this};
    fileSelector->setToolTip("Browse SoundFonts");
    fileSelector->setAutoRaise(true);
    fileSelector->setIcon(QIcon{":/data/file.svg"});
    connect(fileSelector, &QToolButton::clicked, this, &SoundFontEditor::onClick);

    mGainEditor = new GainEditor{this};
    connect(mGainEditor, &GainEditor::gainChanged, this, &SoundFontEditor::sendGain);

    mReverbEditor = new OptionalReverbEditor{this};
    connect(mReverbEditor, &OptionalReverbEditor::reverbChanged, this, &SoundFontEditor::sendReverb);

    mChorusEditor = new OptionalChorusEditor{this};
    connect(mChorusEditor, &OptionalChorusEditor::chorusChanged, this, &SoundFontEditor::sendChorus);

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
    SERIALIZE("gain", serial::serializeNumber, mHandler.gain(), result);
    SERIALIZE("reverb.active", serial::serializeBool, mReverbEditor->isChecked(), result);
    SERIALIZE("reverb.folded", serial::serializeBool, mReverbEditor->isFolded(), result);
    SERIALIZE("reverb.roomsize", serial::serializeNumber, mReverbEditor->editor()->reverb().roomsize, result);
    SERIALIZE("reverb.damp", serial::serializeNumber, mReverbEditor->editor()->reverb().damp, result);
    SERIALIZE("reverb.level", serial::serializeNumber, mReverbEditor->editor()->reverb().level, result);
    SERIALIZE("reverb.width", serial::serializeNumber, mReverbEditor->editor()->reverb().width, result);
    SERIALIZE("chorus.active", serial::serializeBool, mChorusEditor->isChecked(), result);
    SERIALIZE("chorus.folded", serial::serializeBool, mChorusEditor->isFolded(), result);
    SERIALIZE("chorus.type", serial::serializeNumber, mChorusEditor->editor()->chorus().type, result);
    SERIALIZE("chorus.nr", serial::serializeNumber, mChorusEditor->editor()->chorus().nr, result);
    SERIALIZE("chorus.level", serial::serializeNumber, mChorusEditor->editor()->chorus().level, result);
    SERIALIZE("chorus.speed", serial::serializeNumber, mChorusEditor->editor()->chorus().speed, result);
    SERIALIZE("chorus.depth", serial::serializeNumber, mChorusEditor->editor()->chorus().depth, result);
    return result;
}

size_t SoundFontEditor::setParameter(const Parameter& parameter) {
    if (parameter.name == "file") {
        setFile(parameter.value);
        return 1;
    }
    UNSERIALIZE("gain", serial::parseDouble, setGain, parameter);
    UNSERIALIZE("reverb.active", serial::parseBool, mReverbEditor->setChecked, parameter);
    UNSERIALIZE("reverb.folded", serial::parseBool, mReverbEditor->setFolded, parameter);
    UNSERIALIZE("reverb.roomsize", serial::parseDouble, mReverbEditor->editor()->setRoomSize, parameter);
    UNSERIALIZE("reverb.damp", serial::parseDouble, mReverbEditor->editor()->setDamp, parameter);
    UNSERIALIZE("reverb.level", serial::parseDouble, mReverbEditor->editor()->setLevel, parameter);
    UNSERIALIZE("reverb.width", serial::parseDouble, mReverbEditor->editor()->setWidth, parameter);
    UNSERIALIZE("chorus.active", serial::parseBool, mChorusEditor->setChecked, parameter);
    UNSERIALIZE("chorus.folded", serial::parseBool, mChorusEditor->setFolded, parameter);
    UNSERIALIZE("chorus.type", serial::parseInt, mChorusEditor->editor()->setType, parameter);
    UNSERIALIZE("chorus.nr", serial::parseInt, mChorusEditor->editor()->setNr, parameter);
    UNSERIALIZE("chorus.level", serial::parseDouble, mChorusEditor->editor()->setLevel, parameter);
    UNSERIALIZE("chorus.speed", serial::parseDouble, mChorusEditor->editor()->setSpeed, parameter);
    UNSERIALIZE("chorus.depth", serial::parseDouble, mChorusEditor->editor()->setDepth, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* SoundFontEditor::getHandler() {
    return &mHandler;
}

void SoundFontEditor::setFile(const QString& file) {
    mHandler.send_message(SoundFontHandler::file_event(file.toStdString()));
    mLoadMovie->start();
    mLoadLabel->show();
}

void SoundFontEditor::setGain(double gain) {
    mGainEditor->setGain(gain);
}

void SoundFontEditor::setReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    mReverbEditor->setReverb(reverb);
}

void SoundFontEditor::setChorus(const SoundFontHandler::optional_chorus_type& chorus) {
    mChorusEditor->setChorus(chorus);
}

void SoundFontEditor::updateFile() {
    const QFileInfo fileInfo{QString::fromStdString(mHandler.file())};
    mFileEditor->setText(fileInfo.completeBaseName());
    mFileEditor->setToolTip(fileInfo.absoluteFilePath());
    mLoadMovie->stop();
    mLoadLabel->hide();
}

void SoundFontEditor::sendGain(double gain) {
    mHandler.send_message(SoundFontHandler::gain_event(gain));
}

void SoundFontEditor::sendReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    mHandler.send_message(SoundFontHandler::reverb_event(reverb));
}

void SoundFontEditor::sendChorus(const SoundFontHandler::optional_chorus_type& chorus) {
    mHandler.send_message(SoundFontHandler::chorus_event(chorus));
}

void SoundFontEditor::onClick() {
    const auto file = context()->pathRetrieverPool()->get("soundfont")->getReadFile(this);
    if (!file.isNull())
        setFile(file);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
