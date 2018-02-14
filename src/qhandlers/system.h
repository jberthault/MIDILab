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

#ifndef QHANDLERS_SYSTEM_H
#define QHANDLERS_SYSTEM_H

#include "handlers/systemhandler.h"
#include "qcore/editors.h"

//============
// MetaSystem
//============

class MetaSystem : public ClosedMetaHandler {

public:
    explicit MetaSystem(QObject* parent);

    QStringList instantiables() override;

    HandlerProxy instantiate(const QString& name) override;

private:
    SystemHandlerFactory mFactory;

};

#endif // QHANDLERS_SYSTEM_H