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

#ifndef HANDLERS_STANDARD_H
#define HANDLERS_STANDARD_H

#include "qcore/core.h"

//=================
// StandardFactory
//=================

class StandardFactory : public QObject, public MetaHandlerFactory {

    Q_OBJECT
    Q_PLUGIN_METADATA(IID MetaHandlerFactory_iid FILE "standard.json")
    Q_INTERFACES(MetaHandlerFactory)

public:
    StandardFactory(QObject* parent = nullptr);
    const QList<MetaHandler*>& spawn() const override;

private:
    QList<MetaHandler*> mMetaHandlers;

};

//=================
// Pattern Handler
//=================

//class PatternHandler : public Handler {

//public:

//    static const int ignored = 0;
//    static const int good = 1;
//    static const int bad = -1;

//    PatternHandler(Event target = Event());

//    result_type handle_message(const Message& message) override;
//    result_type on_open(state_t state) override;

//private:

//    int advance(const Event& event);

//    int current_state;
//    int target_state;
//    Event target_event; /*!< event emitted when target state is reached */

//};

#endif // HANDLERS_STANDARD_H
