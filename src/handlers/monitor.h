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

#ifndef HANDLERS_MONITOR_H
#define HANDLERS_MONITOR_H

#include <QObject>
#include <QtPlugin>
#include <QTextEdit>
#include "qcore/editors.h"

//=============
// MetaMonitor
//=============

class MetaMonitor : public MetaHandler {

public:
    MetaMonitor(QObject* parent);

    Instance instantiate() override;

};

//=========
// Monitor
//=========

class Monitor : public GraphicalHandler {

    Q_OBJECT

public:
    explicit Monitor();

    void setFamilies(families_t families);

    result_type handle_message(const Message& message) override;

protected slots:
    void onFilterClick();

private:
    QTextEdit* mEditor;
    FamilySelector* mFamilySelector;

};

#endif // HANDLERS_MONITOR_H
