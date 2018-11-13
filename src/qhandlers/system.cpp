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

#include "handlers/systemhandler.h"
#include "qhandlers/system.h"

class SystemProxyFactory final : public ClosedProxyFactory {

public:
    using ClosedProxyFactory::ClosedProxyFactory;

    QStringList instantiables() override {
        QStringList result;
        mFactory.update();
        for (const auto& name : mFactory.available())
            result.append(QString::fromStdString(name));
        return result;
    }

    HandlerProxy instantiate(const QString& name) override {
        HandlerProxy proxy;
        proxy.setContent(mFactory.instantiate(name.toStdString()).release());
        return proxy;
    }

private:
    SystemHandlerFactory mFactory;

};

MetaHandler* makeMetaSystem(QObject* parent) {
    auto* meta = new MetaHandler{parent};
    meta->setIdentifier("System");
    meta->setDescription("Represents all connected devices");
    meta->setFactory(new SystemProxyFactory);
    return meta;
}
