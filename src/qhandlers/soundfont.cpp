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

constexpr exp_range_t<double> gainRange = {{0., 10.}, 1.};

constexpr range_t<double> reverbRoomsizeRange = {0., 1.2};
constexpr range_t<double> reverbDampRange = {0., 1.};
constexpr range_t<double> reverbLevelRange = {0., 1.};
constexpr exp_range_t<double> reverbWidthRange = {{0., 100.}, 10.};

constexpr range_t<int> chorusNrRange = {0, 99};
constexpr exp_range_t<double> chorusLevelRange = {{0., 10.}, 1.};
constexpr range_t<double> chorusSpeedRange = {0.30, 5.};
constexpr range_t<double> chorusDepthRange = {0., 10.};

}

//===============
// MetaSoundFont
//===============

MetaSoundFont::MetaSoundFont(QObject* parent) : OpenMetaHandler(parent) {
    setIdentifier("SoundFont");
}

void MetaSoundFont::setContent(HandlerProxy& proxy) {
    proxy.setContent(new SoundFontEditor);
}

//===================
// SoundFontReceiver
//===================

SoundFontReceiver::SoundFontReceiver(QObject* parent) : CustomReceiver(parent) {

}

Receiver::result_type SoundFontReceiver::receive_message(Handler* target, const Message& message) {
    auto result = observer()->handleMessage(target, message);
    if (message.event.family() == family_t::custom && message.event.get_custom_key() == "SoundFont.file")
        emit fileHandled();
    return result;
}

//============
// GainEditor
//============

GainEditor::GainEditor(QWidget* parent) : QWidget(parent) {
    mSlider = new SimpleSlider(Qt::Horizontal, this);
    mSlider->setTextWidth(35);
    mSlider->setDefaultRatio(gainRange.reduce(.2));
    connect(mSlider, &SimpleSlider::knobChanged, this, &GainEditor::updateText);
    connect(mSlider, &SimpleSlider::knobMoved, this, &GainEditor::onMove);
    setLayout(make_vbox(margin_tag{0}, spacing_tag{0}, mSlider));
}

void GainEditor::setGain(double gain) {
    mSlider->setRatio(gainRange.reduce(gain));
    emit gainChanged(gain);
}

void GainEditor::onMove(qreal ratio) {
    auto gain = gainRange.expand(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
    emit gainChanged(gain);
}

void GainEditor::updateText(qreal ratio) {
    auto gain = gainRange.expand(ratio);
    mSlider->setText(QString::number(gain, 'f', 2));
}

// =============
// ReverbEditor
// =============

ReverbEditor::ReverbEditor(QWidget* parent) : QGroupBox("Reverb", parent), mReverb(SoundFontHandler::default_reverb()) {

    setCheckable(true);
    setChecked(true);
    connect(this, &ReverbEditor::toggled, this, &ReverbEditor::onToggle);

    mRoomsizeSlider = new SimpleSlider(Qt::Horizontal, this);
    mRoomsizeSlider->setTextWidth(35);
    mRoomsizeSlider->setDefaultRatio(reverbRoomsizeRange.reduce(mReverb.roomsize));
    connect(mRoomsizeSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onRoomsizeChanged);
    connect(mRoomsizeSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onRoomsizeMoved);

    mDampSlider = new SimpleSlider(Qt::Horizontal, this);
    mDampSlider->setTextWidth(35);
    mDampSlider->setDefaultRatio(reverbDampRange.reduce(mReverb.damp));
    connect(mDampSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onDampChanged);
    connect(mDampSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onDampMoved);

    mLevelSlider = new SimpleSlider(Qt::Horizontal, this);
    mLevelSlider->setTextWidth(35);
    mLevelSlider->setDefaultRatio(reverbLevelRange.reduce(mReverb.level));
    connect(mLevelSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onLevelChanged);
    connect(mLevelSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onLevelMoved);

    mWidthSlider = new SimpleSlider(Qt::Horizontal, this);
    mWidthSlider->setTextWidth(35);
    mWidthSlider->setDefaultRatio(reverbWidthRange.reduce(mReverb.width));
    connect(mWidthSlider, &SimpleSlider::knobChanged, this, &ReverbEditor::onWidthChanged);
    connect(mWidthSlider, &SimpleSlider::knobMoved, this, &ReverbEditor::onWidthMoved);

    mRoomsizeSlider->setDefault();
    mDampSlider->setDefault();
    mLevelSlider->setDefault();
    mWidthSlider->setDefault();

    auto form = new QFormLayout;
    form->setVerticalSpacing(5);
    form->addRow("Room Size", mRoomsizeSlider);
    form->addRow("Damp", mDampSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Width", mWidthSlider);
    setLayout(form);

}

bool ReverbEditor::isActive() const {
    return isChecked();
}

SoundFontHandler::optional_reverb_type ReverbEditor::reverb() const {
    if (isActive())
        return mReverb;
    return {};
}

SoundFontHandler::reverb_type ReverbEditor::rawReverb() const {
    return mReverb;
}

void ReverbEditor::setActive(bool active) {
    setChecked(active);
    emit reverbChanged(reverb());
}

void ReverbEditor::setRoomSize(double value) {
    mReverb.roomsize = value;
    mRoomsizeSlider->setRatio(reverbRoomsizeRange.reduce(mReverb.roomsize));
    if (isActive())
        emit reverbChanged(mReverb);
}

void ReverbEditor::setDamp(double value) {
    mReverb.damp = value;
    mDampSlider->setRatio(reverbDampRange.reduce(mReverb.damp));
    if (isActive())
        emit reverbChanged(mReverb);
}

void ReverbEditor::setLevel(double value) {
    mReverb.level = value;
    mLevelSlider->setRatio(reverbLevelRange.reduce(mReverb.level));
    if (isActive())
        emit reverbChanged(mReverb);
}

void ReverbEditor::setWidth(double value) {
    mReverb.width = value;
    mWidthSlider->setRatio(reverbWidthRange.reduce(mReverb.width));
    if (isActive())
        emit reverbChanged(mReverb);
}

void ReverbEditor::setReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    if (reverb) {
        mReverb = *reverb;
        mRoomsizeSlider->setRatio(reverbRoomsizeRange.reduce(mReverb.roomsize));
        mDampSlider->setRatio(reverbDampRange.reduce(mReverb.damp));
        mLevelSlider->setRatio(reverbLevelRange.reduce(mReverb.level));
        mWidthSlider->setRatio(reverbWidthRange.reduce(mReverb.width));
        setChecked(true);
    } else {
        setChecked(false);
    }
    emit reverbChanged(reverb);
}

void ReverbEditor::onRoomsizeChanged(qreal ratio) {
    mRoomsizeSlider->setText(QString::number(reverbRoomsizeRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onRoomsizeMoved(qreal ratio) {
    mReverb.roomsize = reverbRoomsizeRange.expand(ratio);
    mRoomsizeSlider->setText(QString::number(mReverb.roomsize, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onDampChanged(qreal ratio) {
    mDampSlider->setText(QString::number(reverbDampRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onDampMoved(qreal ratio)  {
    mReverb.damp = reverbDampRange.expand(ratio);
    mDampSlider->setText(QString::number(mReverb.damp, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onLevelChanged(qreal ratio) {
    mLevelSlider->setText(QString::number(reverbLevelRange.expand(ratio), 'f', 2));
}

void ReverbEditor::onLevelMoved(qreal ratio) {
    mReverb.level = reverbLevelRange.expand(ratio);
    mLevelSlider->setText(QString::number(mReverb.level, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onWidthChanged(qreal ratio) {
    mWidthSlider->setText(QString::number(reverbWidthRange.expand(ratio), 'f', 2));
}
void ReverbEditor::onWidthMoved(qreal ratio) {
    mReverb.width = reverbWidthRange.expand(ratio);
    mWidthSlider->setText(QString::number(mReverb.width, 'f', 2));
    emit reverbChanged(mReverb);
}

void ReverbEditor::onToggle(bool activated) {
    SoundFontHandler::optional_reverb_type optional_reverb;
    if (activated)
        optional_reverb = mReverb;
    emit reverbChanged(optional_reverb);
}

//==============
// ChorusEditor
//==============

ChorusEditor::ChorusEditor(QWidget* parent) : QGroupBox("Chorus", parent), mChorus(SoundFontHandler::default_chorus()) {

    setCheckable(true);
    setChecked(true);
    connect(this, &ChorusEditor::toggled, this, &ChorusEditor::onToggle);

    mTypeBox = new QComboBox(this);
    mTypeBox->addItem("Sine Wave");
    mTypeBox->addItem("Triangle Wave");
    connect(mTypeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onTypeMoved(int)));

    mNrSlider = new SimpleSlider(Qt::Horizontal, this);
    mNrSlider->setTextWidth(35);
    mNrSlider->setDefaultRatio(chorusNrRange.reduce(mChorus.nr));
    connect(mNrSlider, &SimpleSlider::knobChanged, this, &ChorusEditor::onNrChanged);
    connect(mNrSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::onNrMoved);

    mLevelSlider = new SimpleSlider(Qt::Horizontal, this);
    mLevelSlider->setTextWidth(35);
    mLevelSlider->setDefaultRatio(chorusLevelRange.reduce(mChorus.level));
    connect(mLevelSlider, &SimpleSlider::knobChanged, this, &ChorusEditor::onLevelChanged);
    connect(mLevelSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::onLevelMoved);

    mSpeedSlider = new SimpleSlider(Qt::Horizontal, this);
    mSpeedSlider->setTextWidth(35);
    mSpeedSlider->setDefaultRatio(chorusSpeedRange.reduce(mChorus.speed));
    connect(mSpeedSlider, &SimpleSlider::knobChanged, this, &ChorusEditor::onSpeedChanged);
    connect(mSpeedSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::onSpeedMoved);

    mDepthSlider = new SimpleSlider(Qt::Horizontal, this);
    mDepthSlider->setTextWidth(35);
    mDepthSlider->setDefaultRatio(chorusDepthRange.reduce(mChorus.depth));
    connect(mDepthSlider, &SimpleSlider::knobChanged, this, &ChorusEditor::onDepthChanged);
    connect(mDepthSlider, &SimpleSlider::knobMoved, this, &ChorusEditor::onDepthMoved);

    mTypeBox->setCurrentIndex(mChorus.type);
    mNrSlider->setDefault();
    mLevelSlider->setDefault();
    mSpeedSlider->setDefault();
    mDepthSlider->setDefault();

    auto form = new QFormLayout;
    form->setVerticalSpacing(5);
    form->addRow("Type", mTypeBox);
    form->addRow("NR", mNrSlider);
    form->addRow("Level", mLevelSlider);
    form->addRow("Speed (Hz)", mSpeedSlider);
    form->addRow("Depth (ms)", mDepthSlider);
    setLayout(form);

}

bool ChorusEditor::isActive() const {
    return isChecked();
}

void ChorusEditor::setActive(bool active) {
    setChecked(active);
    emit chorusChanged(chorus());
}

SoundFontHandler::optional_chorus_type ChorusEditor::chorus() const {
    if (isActive())
        return mChorus;
    return {};
}

SoundFontHandler::chorus_type ChorusEditor::rawChorus() const {
    return mChorus;
}

void ChorusEditor::setType(int value) {
    mTypeBox->setCurrentIndex(value);
}

void ChorusEditor::setNr(int value) {
    mChorus.nr = value;
    mNrSlider->setRatio(chorusNrRange.reduce(mChorus.nr));
    if (isActive())
        emit chorusChanged(mChorus);
}

void ChorusEditor::setLevel(double value) {
    mChorus.level = value;
    mLevelSlider->setRatio(chorusLevelRange.reduce(mChorus.level));
    if (isActive())
        emit chorusChanged(mChorus);
}

void ChorusEditor::setSpeed(double value) {
    mChorus.speed = value;
    mSpeedSlider->setRatio(chorusSpeedRange.reduce(mChorus.speed));
    if (isActive())
        emit chorusChanged(mChorus);
}

void ChorusEditor::setDepth(double value) {
    mChorus.depth = value;
    mDepthSlider->setRatio(chorusDepthRange.reduce(mChorus.depth));
    if (isActive())
        emit chorusChanged(mChorus);
}

void ChorusEditor::setChorus(const SoundFontHandler::optional_chorus_type& chorus) {
    if (chorus) {
        QSignalBlocker guard(mTypeBox);
        mChorus = *chorus;
        mTypeBox->setCurrentIndex(mChorus.type);
        mNrSlider->setRatio(chorusNrRange.reduce(mChorus.nr));
        mLevelSlider->setRatio(chorusLevelRange.reduce(mChorus.level));
        mSpeedSlider->setRatio(chorusSpeedRange.reduce(mChorus.speed));
        mDepthSlider->setRatio(chorusDepthRange.reduce(mChorus.depth));
        setChecked(true);
    } else {
        setChecked(false);
    }
    emit chorusChanged(chorus);
}

void ChorusEditor::onTypeMoved(int type) {
    mChorus.type = type;
    if (isActive())
        emit chorusChanged(mChorus);
}

void ChorusEditor::onNrChanged(qreal ratio) {
    mNrSlider->setText(QString::number(chorusNrRange.expand(ratio)));
}

void ChorusEditor::onNrMoved(qreal ratio) {
    mChorus.nr = chorusNrRange.expand(ratio);
    mNrSlider->setText(QString::number(mChorus.nr));
    emit chorusChanged(mChorus);
}

void ChorusEditor::onLevelChanged(qreal ratio) {
    mLevelSlider->setText(QString::number(chorusLevelRange.expand(ratio), 'f', 2));
}

void ChorusEditor::onLevelMoved(qreal ratio) {
    mChorus.level = chorusLevelRange.expand(ratio);
    mLevelSlider->setText(QString::number(mChorus.level, 'f', 2));
    emit chorusChanged(mChorus);
}

void ChorusEditor::onSpeedChanged(qreal ratio) {
    mSpeedSlider->setText(QString::number(chorusSpeedRange.expand(ratio), 'f', 2));
}

void ChorusEditor::onSpeedMoved(qreal ratio) {
    mChorus.speed = chorusSpeedRange.expand(ratio);
    mSpeedSlider->setText(QString::number(mChorus.speed, 'f', 2));
    emit chorusChanged(mChorus);
}

void ChorusEditor::onDepthChanged(qreal ratio) {
    mDepthSlider->setText(QString::number(chorusDepthRange.expand(ratio), 'f', 2));
}

void ChorusEditor::onDepthMoved(qreal ratio) {
    mChorus.depth = chorusDepthRange.expand(ratio);
    mDepthSlider->setText(QString::number(mChorus.depth, 'f', 2));
    emit chorusChanged(mChorus);
}

void ChorusEditor::onToggle(bool activated) {
    SoundFontHandler::optional_chorus_type optional_chorus;
    if (activated)
        optional_chorus = mChorus;
    emit chorusChanged(optional_chorus);
}


//=================
// SoundFontEditor
//=================

SoundFontEditor::SoundFontEditor() : HandlerEditor(), mHandler(std::make_unique<SoundFontHandler>()) {

    mReceiver = new SoundFontReceiver(this);
    mHandler->set_receiver(mReceiver);
    connect(mReceiver, &SoundFontReceiver::fileHandled, this, &SoundFontEditor::updateFile);

    mLoadMovie = new QMovie(":/data/load.gif", QByteArray(), this);
    mLoadLabel = new QLabel(this);
    mLoadLabel->setMovie(mLoadMovie);
    mLoadLabel->hide();

    /// @todo add menu option to set path from a previous one (keep history)
    mFileEditor = new QLineEdit(this);
    mFileEditor->setMinimumWidth(200);
    mFileEditor->setReadOnly(true);
    mFileEditor->setText(QString::fromStdString(mHandler->file()));

    mFileSelector = new QToolButton(this);
    mFileSelector->setToolTip("Browse SoundFonts");
    mFileSelector->setAutoRaise(true);
    mFileSelector->setIcon(QIcon(":/data/file.svg"));
    connect(mFileSelector, &QToolButton::clicked, this, &SoundFontEditor::onClick);

    mGainEditor = new GainEditor(this);
    mGainEditor->setGain(mHandler->gain());
    connect(mGainEditor, &GainEditor::gainChanged, this, &SoundFontEditor::sendGain);

    mReverbEditor = new ReverbEditor(this);
    mReverbEditor->setReverb(mHandler->reverb());
    connect(mReverbEditor, &ReverbEditor::reverbChanged, this, &SoundFontEditor::sendReverb);

    mChorusEditor = new ChorusEditor(this);
    mChorusEditor->setChorus(mHandler->chorus());
    connect(mChorusEditor, &ChorusEditor::chorusChanged, this, &SoundFontEditor::sendChorus);

    auto form = new QFormLayout;
    form->setMargin(0);
    form->addRow("File", make_hbox(spacing_tag{0}, mFileSelector, mFileEditor, mLoadLabel));
    form->addRow("Gain", mGainEditor);
    setLayout(make_vbox(form, mReverbEditor, mChorusEditor));
}

HandlerView::Parameters SoundFontEditor::getParameters() const {
    auto result = HandlerEditor::getParameters();
    SERIALIZE("file", QString::fromStdString, mHandler->file(), result);
    SERIALIZE("gain", serial::serializeNumber, mHandler->gain(), result);
    SERIALIZE("reverb", serial::serializeBool, mReverbEditor->isActive(), result);
    SERIALIZE("reverb.roomsize", serial::serializeNumber, mReverbEditor->rawReverb().roomsize, result);
    SERIALIZE("reverb.damp", serial::serializeNumber, mReverbEditor->rawReverb().damp, result);
    SERIALIZE("reverb.level", serial::serializeNumber, mReverbEditor->rawReverb().level, result);
    SERIALIZE("reverb.width", serial::serializeNumber, mReverbEditor->rawReverb().width, result);
    SERIALIZE("chorus", serial::serializeBool, mChorusEditor->isActive(), result);
    SERIALIZE("chorus.type", serial::serializeNumber, mChorusEditor->rawChorus().type, result);
    SERIALIZE("chorus.nr", serial::serializeNumber, mChorusEditor->rawChorus().nr, result);
    SERIALIZE("chorus.level", serial::serializeNumber, mChorusEditor->rawChorus().level, result);
    SERIALIZE("chorus.speed", serial::serializeNumber, mChorusEditor->rawChorus().speed, result);
    SERIALIZE("chorus.depth", serial::serializeNumber, mChorusEditor->rawChorus().depth, result);
    return result;
}

size_t SoundFontEditor::setParameter(const Parameter& parameter) {
    if (parameter.name == "file") {
        setFile(parameter.value);
        return 1;
    }
    UNSERIALIZE("gain", serial::parseDouble, setGain, parameter);
    UNSERIALIZE("reverb", serial::parseBool, mReverbEditor->setActive, parameter);
    UNSERIALIZE("reverb.roomsize", serial::parseDouble, mReverbEditor->setRoomSize, parameter);
    UNSERIALIZE("reverb.damp", serial::parseDouble, mReverbEditor->setDamp, parameter);
    UNSERIALIZE("reverb.level", serial::parseDouble, mReverbEditor->setLevel, parameter);
    UNSERIALIZE("reverb.width", serial::parseDouble, mReverbEditor->setWidth, parameter);
    UNSERIALIZE("chorus", serial::parseBool, mChorusEditor->setActive, parameter);
    UNSERIALIZE("chorus.type", serial::parseInt, mChorusEditor->setType, parameter);
    UNSERIALIZE("chorus.nr", serial::parseInt, mChorusEditor->setNr, parameter);
    UNSERIALIZE("chorus.level", serial::parseDouble, mChorusEditor->setLevel, parameter);
    UNSERIALIZE("chorus.speed", serial::parseDouble, mChorusEditor->setSpeed, parameter);
    UNSERIALIZE("chorus.depth", serial::parseDouble, mChorusEditor->setDepth, parameter);
    return HandlerEditor::setParameter(parameter);
}

Handler* SoundFontEditor::getHandler() const {
    return mHandler.get();
}

void SoundFontEditor::setFile(const QString& file) {
    if (mHandler->send_message(SoundFontHandler::file_event(file.toStdString()))) {
        mLoadMovie->start();
        mLoadLabel->show();
    }
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
    QFileInfo fileInfo(QString::fromStdString(mHandler->file()));
    mFileEditor->setText(fileInfo.completeBaseName());
    mFileEditor->setToolTip(fileInfo.absoluteFilePath());
    mLoadMovie->stop();
    mLoadLabel->hide();
}

void SoundFontEditor::sendGain(double gain) {
    mHandler->send_message(SoundFontHandler::gain_event(gain));
}

void SoundFontEditor::sendReverb(const SoundFontHandler::optional_reverb_type& reverb) {
    mHandler->send_message(SoundFontHandler::reverb_event(reverb));
}

void SoundFontEditor::sendChorus(const SoundFontHandler::optional_chorus_type& chorus) {
    mHandler->send_message(SoundFontHandler::chorus_event(chorus));
}

void SoundFontEditor::onClick() {
    auto file = context()->pathRetriever("soundfont")->getReadFile(this);
    if (!file.isNull())
        setFile(file);
}

#endif // MIDILAB_FLUIDSYNTH_VERSION
