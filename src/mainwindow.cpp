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

#include <QApplication>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QtGlobal>
#include "mainwindow.h"
#include "qcore/core.h"
#include "qcore/parsing.h"
#include "qcore/managereditor.h"
#include "qtools/displayer.h"
#include "qtools/misc.h"
#include "handlers/systemhandler.h"
#include "qhandlers/handlers.h"

namespace {

//=====================
// ConfigurationPuller
//=====================

struct ConfigurationPuller {

    void addConfiguration(MultiDisplayer* root, const parsing::Configuration& configuration) {
        // add frames
        if (!configuration.frames.empty())
            setFrame(root, configuration.frames[0], true);
        for (int i=1 ; i < configuration.frames.size() ; i++)
            addFrame(root, configuration.frames[i], true);
        // add handlers
        for (const parsing::Handler& handler : configuration.handlers)
            addHandler(handler);
        // add connections
        for (const parsing::Connection& connection : configuration.connections)
            addConnection(connection);
        // set colors
        for (channel_t c=0 ; c < qMin(0x10, configuration.colors.size()) ; ++c)
            Manager::instance->channelEditor()->setColor(c, configuration.colors.at(c));
        // display visible frames created
        for(auto displayer : visibleDisplayers)
            displayer->show();
    }

    void addConnection(const parsing::Connection& connection) {
        bool hasSource = !connection.source.isEmpty();
        Handler* tail = handlersReferences.value(connection.tail, nullptr);
        Handler* head = handlersReferences.value(connection.head, nullptr);
        Handler* source = hasSource ? handlersReferences.value(connection.source, nullptr) : nullptr;
        if (!tail || !head || hasSource && !source) {
            qWarning() << "wrong connection handlers: " << handlerName(tail) << handlerName(head) << handlerName(source);
            return;
        }
        if (hasSource)
            Manager::instance->insertConnection(tail, head, source);
        else
            Manager::instance->insertConnection(tail, head);
    }

    void addHandler(const parsing::Handler& handler) {
        // special treatment for system handlers (just register the id)
        if (handler.type == "System") {
            for (Handler* h : Manager::instance->getHandlers()) {
                if (handlerName(h) == handler.name) {
                    handlersReferences[handler.id] = h;
                    return;
                }
            }
            qWarning() << "Unknown system handler";
            return;
        }
        // prepare config
        HandlerConfiguration hc(handler.name);
        hc.host = viewReferences.value(handler.id, nullptr);
        for (const parsing::Property& prop : handler.properties)
            hc.parameters.push_back(HandlerView::Parameter{prop.key, prop.value});
        hc.group = handler.group;
        // create the handler
        Handler* h = Manager::instance->loadHandler(handler.type, hc);
        if (h)
            handlersReferences[handler.id] = h;
    }

    void addWidget(MultiDisplayer* parent, const parsing::Widget& widget) {
        if (widget.isFrame)
            addFrame(parent, widget.frame);
        else
            addView(parent, widget.view);
    }

    void addFrame(MultiDisplayer* parent, const parsing::Frame& frame, bool detached=false) {
        MultiDisplayer* displayer = detached ? parent->insertDetached() : parent->insertMulti();
        setFrame(displayer, frame, detached);
        if (detached && frame.visible)
            visibleDisplayers.push_back(displayer);
    }

    void setFrame(MultiDisplayer* displayer, const parsing::Frame& frame, bool isTopLevel) {
        displayer->setOrientation(frame.layout);
        displayer->setWindowTitle(frame.name);
        for (const parsing::Widget& widget : frame.widgets)
            addWidget(displayer, widget);
        if (isTopLevel) {
            QWidget* window = displayer->window();
            if (frame.size.isValid())
                window->resize(frame.size);
            if (!frame.pos.isNull())
                window->move(frame.pos);
        }
    }

    void addView(MultiDisplayer* parent, const parsing::View& view) {
        viewReferences[view.ref] = parent->insertSingle();
    }

    QMap<QString, Handler*> handlersReferences;
    QMap<QString, SingleDisplayer*> viewReferences;
    std::vector<MultiDisplayer*> visibleDisplayers;

};

//=====================
// ConfigurationPuller
//=====================

struct ConfigurationPusher {

    void prepareHandlers() {
        storage = Manager::instance->storage();
        int id=0;
        QMapIterator<Handler*, Manager::Data> storageIt(storage);
        while (storageIt.hasNext()) {
            storageIt.next();
            auto handler = storageIt.key();
            const auto& data = storageIt.value();
            auto holder = dynamic_cast<StandardHolder*>(handler->holder());
            auto& handlerConfig = phandlers[handler];
            HandlerView* view = dynamic_cast<GraphicalHandler*>(handler);
            if (!view)
                view = dynamic_cast<HandlerView*>(data.editor);
            handlerConfig.type = data.type;
            handlerConfig.id = QString("#%1").arg(id++);
            handlerConfig.name = handlerName(handler);
            handlerConfig.group = holder ? QString::fromStdString(holder->name()) : QString();
            if (view)
                for (const auto& parameter : view->getParameters())
                    handlerConfig.properties.push_back(parsing::Property{parameter.name, parameter.value});
        }
    }

    void prepareConnections() {
        QMapIterator<Handler*, parsing::Handler> phandlerIt(phandlers);
        while (phandlerIt.hasNext()) {
            phandlerIt.next();
            auto handler = phandlerIt.key();
            const auto& phandler = phandlerIt.value();
            config.handlers.append(phandler);
            for (const auto& sink : handler->sinks()) {
                const auto& filter = sink.second;
                parsing::Connection connection;
                connection.tail = phandler.id;
                connection.head = phandlers[sink.first].id;
                if (boost::logic::indeterminate(filter.match_nothing())) {
                    QStringList sources;
                    QMapIterator<Handler*, parsing::Handler> phandlerIt2(phandlers);
                    while (phandlerIt2.hasNext()) {
                        phandlerIt2.next();
                        if (filter.match_handler(phandlerIt2.key()))
                            sources.append(phandlerIt2.value().id);
                    }
                    connection.source = sources.join("|");
                }
                config.connections.append(connection);
            }
        }
    }

    void prepareFrames(MultiDisplayer* mainDisplayer) {
        config.frames.append(displayerToFrame(mainDisplayer));
        QWidget* mainWindow = mainDisplayer->window();
        config.frames[0].pos = mainWindow->pos();
        config.frames[0].size = mainWindow->size();
        for (auto displayer : MultiDisplayer::topLevelDisplayers())
            config.frames.append(displayerToFrame(displayer));
    }

    parsing::Frame displayerToFrame(MultiDisplayer* displayer) {
        parsing::Frame frame;
        frame.layout = displayer->orientation();
        frame.name = displayer->windowTitle();
        frame.pos = displayer->pos();
        frame.size = displayer->size();
        frame.visible = displayer->isVisible();
        for (Displayer* child : displayer->directChildren()) {
            parsing::Widget widget;
            auto node = dynamic_cast<MultiDisplayer*>(child);
            if (node) {
                widget.isFrame = true;
                widget.frame = displayerToFrame(node);
            } else {
                auto leaf = static_cast<SingleDisplayer*>(child);
                widget.isFrame = false;
                QMapIterator<Handler*, parsing::Handler> phandlersIt(phandlers);
                while (phandlersIt.hasNext()) {
                    phandlersIt.next();
                    auto handler = phandlersIt.key();
                    const auto phandler = phandlersIt.value();
                    if (leaf->widget() == dynamic_cast<GraphicalHandler*>(handler) || leaf->widget() == storage[handler].editor) {
                        widget.view.ref = phandler.id;
                        break;
                    }
                }
            }
            frame.widgets.append(widget);
        }
        return frame;
    }

    void prepareColors() {
        for (channel_t c=0 ; c < 0x10 ; ++c)
            config.colors.append(Manager::instance->channelEditor()->color(c));
    }

    parsing::Configuration config;
    QMap<Handler*, Manager::Data> storage;
    QMap<Handler*, parsing::Handler> phandlers;

};

const QVariant discardConfigsData = 1;

}

// ============
// AboutWindow
// ============

static const QString aboutText =
    "<p><b>MIDILab</b> (version " MIDILAB_VERSION_STRING ")</p>"
    "<p>A versatile MIDI laboratory</p>"
    "<ul>"
    " <li>MIDI controller: connect multiple devices</li>"
    " <li>MIDI player: play files or previous records</li>"
    " <li>MIDI recorder: save your playing sessions (<i>not quite finished</i>)</li>"
    " <li>MIDI editor: make or edit songs (<i>not quite started</i>)</li>"
    " <li>...</li>"
    "</ul>"
    "<p>This program is free software.<br/>"
    "It is licensed under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL v3</a>.<br/>"
    "The project is hosted on <a href=\"https://github.com/jberthault/MIDILab\">Github</a></p>"
    "<p>This project uses:</p>"
    "<ul>"
    " <li>Qt Project: see related About</li>"
    " <li><a href=\"http://www.fluidsynth.org\">fluidsynth " MIDILAB_FLUIDSYNTH_VERSION_STRING "</a>: SoundFont Synthetizer</li>"
    " <li><a href=\"https://github.com/iconic/open-iconic\">Open Iconic 1.1.1</a>: A great icon set</li>"
    "</ul>"
    "<p>Copyright \u00a9 2017 Julien Berthault</p>"
    "<p><i> " MIDILAB_MODE " " MIDILAB_PLATFORM " " MIDILAB_SIZE " </i></p>";

AboutWindow::AboutWindow(QWidget* parent) :
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint) {

    setWindowTitle("About");

    auto textLabel = new QLabel(aboutText, this);
    textLabel->setOpenExternalLinks(true);

    auto iconLabel = new QLabel(this);
    QIcon icon = windowIcon();
    QSize size = icon.actualSize(QSize(64, 64));
    iconLabel->setPixmap(icon.pixmap(size));

    auto aboutQtButton = new QPushButton("About Qt", this);
    auto okButton = new QPushButton("OK");
    connect(aboutQtButton, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    connect(okButton, &QPushButton::clicked, this, &AboutWindow::close);

    setLayout(make_vbox(make_hbox(make_vbox(iconLabel, stretch_tag{}), textLabel), make_hbox(stretch_tag{}, aboutQtButton, okButton)));
}

//============
// MainWindow
//============

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    new Manager(this);
    Manager::instance->collector()->addFactory(new StandardFactory(this));
    mManagerEditor = new ManagerEditor(this);
    mProgramEditor = new ProgramEditor(Manager::instance->channelEditor(), this);
    setCentralWidget(new MultiDisplayer(Qt::Horizontal, this));
    setupMenu();
}

QStringList MainWindow::getConfigs() const {
    QSettings settings;
    return settings.value("config").toStringList();
}

void MainWindow::readLastConfig() {
    auto configurations = getConfigs();
    auto fileName = configurations.empty() ? QString(":/data/config.xml") : configurations.front();
    if (readConfig(fileName) && !configurations.empty())
        Manager::instance->pathRetriever("configuration")->setSelection(fileName);
}

bool MainWindow::readConfig(const QString& fileName) {
    TRACE_MEASURE("read config");
    // make system handlers
    auto system_handlers = create_system();
    for (Handler* handler : system_handlers)
        Manager::instance->insertHandler(handler, nullptr, "System", "default");
    parsing::Configuration config;
    try {
        // read file
        QFile configFile(fileName);
        if (!configFile.open(QIODevice::ReadOnly))
            throw QString("unable to open file");
        auto configData = configFile.readAll();
        configFile.close();
        // parse file
        config = parsing::readConfiguration(configData);
    } catch (const QString& error) {
        QMessageBox::critical(this, QString(), QString("Failed reading configuration file\n%1\n\n%2").arg(fileName, error));
        return false;
    }
    // fill interface
    ConfigurationPuller puller;
    puller.addConfiguration(static_cast<MultiDisplayer*>(centralWidget()), config);
    // redo the layout
    mManagerEditor->graphEditor()->graph()->doLayout();
    return true;
}

void MainWindow::loadConfig() {
    QString fileName = Manager::instance->pathRetriever("configuration")->getReadFile(this);
    if (!fileName.isEmpty())
        loadConfig(fileName);
}

void MainWindow::loadConfig(const QString& fileName) {
    raiseConfig(fileName);
    close();
    QProcess::startDetached(QString("\"%1\"").arg(qApp->arguments()[0]));
}

void MainWindow::writeConfig() {

    QString fileName = Manager::instance->pathRetriever("configuration")->getWriteFile(this);
    if (fileName.isEmpty())
        return;

    ConfigurationPusher pusher;
    pusher.prepareHandlers();
    pusher.prepareConnections();
    pusher.prepareFrames(static_cast<MultiDisplayer*>(centralWidget()));
    pusher.prepareColors();

    QSaveFile saveFile(fileName);
    saveFile.open(QSaveFile::WriteOnly);

    QXmlStreamWriter stream(&saveFile);
    parsing::Writer writer(stream);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();
    writer.writeConfiguration(pusher.config);
    stream.writeEndDocument();

    if (saveFile.commit()) {
        raiseConfig(fileName);
        updateConfigs();
    }
}

void MainWindow::raiseConfig(const QString& fileName) {
    QSettings settings;
    QVariant variant = settings.value("config");
    QStringList configurations = variant.toStringList();
    // move or insert file at the top
    configurations.removeAll(fileName);
    configurations.prepend(fileName);
    // restrict configuration size
    while (configurations.size() > 8)
        configurations.removeLast();
    settings.setValue("config", configurations);
}

void MainWindow::updateConfigs() {
    auto configs = getConfigs();
    mConfigMenu->clear();
    if (!configs.isEmpty()) {
        for (const QString& configurationFile : configs) {
            QFileInfo fileInfo(configurationFile);
            QAction* fileAction = mConfigMenu->addAction(QIcon(":/data/grid-three-up.svg"), fileInfo.completeBaseName());
            fileAction->setData(configurationFile);
            fileAction->setToolTip(configurationFile);
        }
        mConfigMenu->addSeparator();
    }
    QAction* clearAction = mConfigMenu->addAction(QIcon(":/data/trash.svg"), "Clear");
    clearAction->setEnabled(!configs.isEmpty());
}

void MainWindow::onConfigSelection(QAction* action) {
    auto fileName = action->data().toString();
    if (fileName.isNull()) { // clear action triggerred
        auto result = QMessageBox::question(this, QString(), "Do you want to clear configurations ?");
        if (result == QMessageBox::Yes) {
            QSettings settings;
            settings.remove("config");
            updateConfigs();
         }
    } else {
        auto result = QMessageBox::question(this, QString(), "Do you want to reload the application ?");
        if (result == QMessageBox::Yes)
            loadConfig(fileName);
    }
}

void MainWindow::setupMenu() {

    MultiDisplayer* displayer = static_cast<MultiDisplayer*>(centralWidget());

//    QToolBar* toolbar = addToolBar("quick actions");

    QMenuBar* menu = new QMenuBar(this);
    setMenuBar(menu);

    // extends menu
    QMenu* fileMenu = menu->addMenu("File");
    fileMenu->addAction(QIcon(":/data/cloud-download.svg"), "Load configuration", this, SLOT(loadConfig()));
    fileMenu->addAction(QIcon(":/data/cloud-upload.svg"), "Save configuration", this, SLOT(writeConfig()));
    mConfigMenu = fileMenu->addMenu(QIcon(":/data/cloud.svg"), "Recent configurations");
    mConfigMenu->setToolTipsVisible(true);
    connect(mConfigMenu, &QMenu::triggered, this, &MainWindow::onConfigSelection);
    updateConfigs();

    // add recent config / midi files loaded
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon(":/data/power-standby.svg"), "Exit", this, SLOT(close()));

    QMenu* handlersMenu = menu->addMenu("Handlers");
    handlersMenu->addAction(mManagerEditor->windowIcon(), "Handlers", mManagerEditor, SLOT(show()));
    handlersMenu->addAction(mProgramEditor->windowIcon(), "Programs", mProgramEditor, SLOT(show()));
    handlersMenu->addSeparator();
    handlersMenu->addAction(QIcon(":/data/target.svg"), "Panic", this, SLOT(panic()));

    QMenu* interfaceMenu = menu->addMenu("Interface");
    interfaceMenu->addAction(Manager::instance->channelEditor()->windowIcon(), Manager::instance->channelEditor()->windowTitle(), Manager::instance->channelEditor(), SLOT(show()));
    // interfaceMenu->addAction(QIcon(":/data/cog.svg"), "Settings", this, SLOT(unimplemented()));

    QIcon lockIcon;
    lockIcon.addFile(":/data/lock-locked.svg", QSize(), QIcon::Normal, QIcon::On);
    lockIcon.addFile(":/data/lock-unlocked.svg", QSize(), QIcon::Normal, QIcon::Off);
    QAction* lockAction = new QAction(lockIcon, "Lock Layout", this);
    lockAction->setCheckable(true);
    lockAction->setChecked(displayer->isLocked());
    connect(lockAction, &QAction::toggled, displayer, &MultiDisplayer::setLocked);
    interfaceMenu->addAction(lockAction);

    interfaceMenu->addAction(QIcon(":/data/plus.svg"), "Add Container", this, SLOT(newDisplayer()));

    /// show windows opened ...

    QMenu* helpMenu = menu->addMenu("Help");
    helpMenu->addAction(QIcon(":/data/question-mark.svg"), "Help", this, SLOT(unimplemented()));
    // helpMenu->addAction("What's new ?", this, SLOT(unimplemented()));
    helpMenu->addSeparator();
    helpMenu->addAction(QIcon(":/data/info.svg"), "About", this, SLOT(about()));

}

void MainWindow::addFiles(const QStringList& files) {
    if (files.empty())
        return;
    QMapIterator<Handler*, Manager::Data> it(Manager::instance->storage());
    while (it.hasNext()) {
        it.next();
        if (it.value().type == "Player") {
            Manager::instance->setParameters(it.key(), {HandlerView::Parameter{"playlist", files.join(";")}});
            return;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    for (auto displayer : MultiDisplayer::topLevelDisplayers())
        displayer->close();
    QMainWindow::closeEvent(event);
}

void MainWindow::newDisplayer() {
    static_cast<MultiDisplayer*>(centralWidget())->insertDetached()->show();
}

void MainWindow::about() {
    auto aboutWindow = new AboutWindow(this);
    aboutWindow->setAttribute(Qt::WA_DeleteOnClose);
    aboutWindow->exec();
}

void MainWindow::panic() {
    for (Handler* handler : Manager::instance->getHandlers())
        Manager::instance->setHandlerOpen(handler, false);
}

void MainWindow::unimplemented() {
    QMessageBox::warning(this, QString(), "Feature not implemented yet");
}
