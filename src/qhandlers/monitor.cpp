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

#include <QPushButton>
#include "monitor.h"
#include "qcore/editors.h"
#include "qtools/misc.h"

//=========
// details
//=========

namespace {

QString specialText(const QString& text) {
    return QString("<span style=\"background-color : black;color : white\">%1</span>").arg(text);
}

}

//=============
// MetaMonitor
//=============

MetaMonitor::MetaMonitor(QObject* parent) : MetaHandler(parent) {
    setIdentifier("Monitor");
}

MetaHandler::Instance MetaMonitor::instantiate() {
    return Instance(new Monitor, nullptr);
}

//=========
// Monitor
//=========

Monitor::Monitor() : GraphicalHandler(handler_ns::out_mode) {

    mFamilySelector = new FamilySelector(this);
    mFamilySelector->setFamilies(~families_t::merge(family_t::active_sense));
    mFamilySelector->setWindowFlags(Qt::Dialog);
    mFamilySelector->setVisible(false);

    mEditor = new QTextEdit(this);
    mEditor->setReadOnly(true);
    mEditor->setAcceptRichText(true);

    QPushButton* clearButton = new QPushButton(tr("Clear"), this);
    connect(clearButton, &QPushButton::clicked, mEditor, &QTextEdit::clear);

    QPushButton* selectFamilyButton = new QPushButton(tr("Filter"), this);
    connect(selectFamilyButton, &QPushButton::clicked, this, &Monitor::onFilterClick);

    setLayout(make_vbox(mEditor, make_hbox(stretch_tag{}, clearButton, selectFamilyButton)));

}

void Monitor::setFamilies(families_t families) {
    mFamilySelector->setFamilies(families);
}

Monitor::result_type Monitor::handle_message(const Message& message) {
    MIDI_HANDLE_OPEN;
    MIDI_CHECK_OPEN_RECEIVE;

    if (!message.event.is(mFamilySelector->families()))
        return result_type::unhandled;

    QString name = QString::fromStdString(message.event.name());
    name += " ";

    channels_t channels = message.event.channels();
    if (channels) {
        std::string str = channel_ns::channels_string(channels);
        name += QString("[%1] ").arg(QString::fromStdString(str));
    }
    name = name.leftJustified(20, '.');
    name += "&nbsp;";
    name = QString("<span style=\"font-weight:bold;\">%1</span>").arg(name);

    QString description = QString::fromStdString(message.event.description());
    /// @todo replace all other non printable characters by a hex code (use a dedicated algorithm)
    description.replace("\n", specialText("\\n"));
    description.replace("\r", specialText("\\r"));
    description.replace("\t", specialText("\\t"));

    mEditor->append(name + description);

    return result_type::success;
}

void Monitor::onFilterClick() {
    mFamilySelector->setVisible(!mFamilySelector->isVisible());
}
