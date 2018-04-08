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
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include "mainwindow.h"
#include "qhandlers/handlers.h"
#include "qtools/displayer.h"

const QVariant discardConfigsData = 1;

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
    Manager::instance->metaHandlerPool()->addFactory(new StandardFactory(this));
    mManagerEditor = new ManagerEditor(this);
    mProgramEditor = new ProgramEditor(Manager::instance->channelEditor(), this);
    setupMenu();
    setupMainDisplayer();
}

QStringList MainWindow::getConfigs() const {
    QSettings settings;
    return settings.value("config").toStringList();
}

void MainWindow::unloadConfig() {
    if (QMessageBox::question(this, {}, "Do you want to unload the current configuration ?") == QMessageBox::Yes)
        clearConfig();
}

void MainWindow::loadConfig() {
    auto fileName = Manager::instance->pathRetrieverPool()->get("configuration")->getReadFile(this);
    if (!fileName.isEmpty())
        readConfig(fileName, true, false);
}

void MainWindow::saveConfig() {
    auto fileName = Manager::instance->pathRetrieverPool()->get("configuration")->getWriteFile(this);
    if (!fileName.isEmpty())
        writeConfig(fileName);
}

void MainWindow::clearConfig() {
    Manager::instance->clearConfiguration();
    // we need a new main displayer after clearing configuration
    setupMainDisplayer();
}

void MainWindow::readLastConfig() {
    auto configurations = getConfigs();
    auto fileName = configurations.empty() ? QString(":/data/config.xml") : configurations.front();
    readConfig(fileName, false, !configurations.empty());
}

void MainWindow::readConfig(const QString& fileName, bool raise, bool select) {
    Configuration config;
    // read configuration
    try {
        QFile file(fileName);
        config = Configuration::read(&file);
    } catch (const QString& error) {
        QMessageBox::critical(this, {}, QString("Failed reading configuration file\n%1\n\n%2").arg(fileName, error));
        return;
    }
    // clear previous configuration
    clearConfig();
    // set the new configuration
    Manager::instance->setConfiguration(config);
    // redo the layout
    mManagerEditor->graphEditor()->graph()->doLayout();
    // update config order and retriever
    if (raise)
        raiseConfig(fileName);
    if (select)
        Manager::instance->pathRetrieverPool()->get("configuration")->setSelection(fileName);
}

void MainWindow::writeConfig(const QString& fileName) {
    auto config = Manager::instance->getConfiguration();
    QSaveFile saveFile(fileName);
    saveFile.open(QSaveFile::WriteOnly);
    Configuration::write(&saveFile, config);
    if (saveFile.commit())
        raiseConfig(fileName);
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
    // update menu
    updateMenu(configurations);
}

void MainWindow::updateMenu(const QStringList& configs) {
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

void MainWindow::about() {
    auto aboutWindow = new AboutWindow(this);
    aboutWindow->setAttribute(Qt::WA_DeleteOnClose);
    aboutWindow->exec();
}

void MainWindow::panic() {
    for (const auto& proxy : Manager::instance->handlerProxies())
        proxy.setState(false);
}

void MainWindow::newDisplayer() {
    Manager::instance->mainDisplayer()->insertDetached()->show();
}

void MainWindow::unimplemented() {
    QMessageBox::warning(this, {}, "Feature not implemented yet");
}

void MainWindow::addFiles(const QStringList& files) {
    if (files.empty())
        return;
    for (const auto& proxy : Manager::instance->handlerProxies()) {
        if (proxy.metaHandler()->identifier() == "Player") {
            proxy.setParameter({"playlist", files.join(";")});
            return;
        }
    }
}

void MainWindow::setupMainDisplayer() {
    auto mainDisplayer = new MultiDisplayer(Qt::Horizontal, this);
    mainDisplayer->setLocked(mLockAction->isChecked());
    setCentralWidget(mainDisplayer);
    connect(mLockAction, &QAction::toggled, mainDisplayer, &MultiDisplayer::setLocked);
}

void MainWindow::setupMenu() {

//    QToolBar* toolbar = addToolBar("quick actions");

    QMenuBar* menu = new QMenuBar(this);
    setMenuBar(menu);

    // extends menu
    QMenu* fileMenu = menu->addMenu("File");
    fileMenu->addAction(QIcon(":/data/rain.svg"), "Unload configuration", this, SLOT(unloadConfig()));
    fileMenu->addAction(QIcon(":/data/cloud-download.svg"), "Load configuration", this, SLOT(loadConfig()));
    fileMenu->addAction(QIcon(":/data/cloud-upload.svg"), "Save configuration", this, SLOT(saveConfig()));
    mConfigMenu = fileMenu->addMenu(QIcon(":/data/cloud.svg"), "Recent configurations");
    mConfigMenu->setToolTipsVisible(true);
    connect(mConfigMenu, &QMenu::triggered, this, &MainWindow::onConfigSelection);
    updateMenu(getConfigs());

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
    mLockAction = new QAction(lockIcon, "Lock Layout", this);
    mLockAction->setCheckable(true);
    mLockAction->setChecked(true);
    interfaceMenu->addAction(mLockAction);

    interfaceMenu->addAction(QIcon(":/data/plus.svg"), "Add Container", this, SLOT(newDisplayer()));

    QMenu* helpMenu = menu->addMenu("Help");
    helpMenu->addAction(QIcon(":/data/question-mark.svg"), "Help", this, SLOT(unimplemented()));
    // helpMenu->addAction("What's new ?", this, SLOT(unimplemented()));
    helpMenu->addSeparator();
    helpMenu->addAction(QIcon(":/data/info.svg"), "About", this, SLOT(about()));

}

void MainWindow::onConfigSelection(QAction* action) {
    auto fileName = action->data().toString();
    if (fileName.isNull()) { // clear action triggerred
        if (QMessageBox::question(this, {}, "Do you want to clear configurations ?") == QMessageBox::Yes) {
            QSettings settings;
            settings.remove("config");
            updateMenu({});
        }
    } else {
        if (QMessageBox::question(this, {}, "Do you want to load this configuration ?") == QMessageBox::Yes)
            readConfig(fileName, true, true);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    for (auto* displayer : MultiDisplayer::topLevelDisplayers())
        displayer->close();
    QMainWindow::closeEvent(event);
}
