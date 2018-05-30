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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include "qcore/managereditor.h"
#include "qcore/programeditor.h"

// ======
// macros
// ======

#define MIDILAB_TOKENIZE_(a) #a
#define MIDILAB_TOKENIZE(a) MIDILAB_TOKENIZE_(a)

#ifdef MIDILAB_FLUIDSYNTH_VERSION
# define MIDILAB_FLUIDSYNTH_VERSION_STRING MIDILAB_TOKENIZE(MIDILAB_FLUIDSYNTH_VERSION)
#else
# define MIDILAB_FLUIDSYNTH_VERSION_STRING "unspecified"
#endif

#ifdef MIDILAB_VERSION
# define MIDILAB_VERSION_STRING MIDILAB_TOKENIZE(MIDILAB_VERSION)
#else
# define MIDILAB_VERSION_STRING "unspecified"
#endif

#ifdef NDEBUG
# define MIDILAB_MODE "Release"
#else
# define MIDILAB_MODE "Debug"
#endif

#ifdef _WIN32
    #define MIDILAB_PLATFORM "Windows"
#elif defined(__linux__)
    #define MIDILAB_PLATFORM "Linux"
#endif

#if INTPTR_MAX == INT64_MAX
    #define MIDILAB_SIZE "64 bits"
#elif INTPTR_MAX == INT32_MAX
    #define MIDILAB_SIZE "32 bits"
#endif

// ============
// AboutWindow
// ============

class AboutWindow : public QDialog {

    Q_OBJECT

public:
    explicit AboutWindow(QWidget* parent);

};

// ===========
// MainWindow
// ===========

class MainWindow : public QMainWindow {

    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent);

public slots:
    QStringList getConfigs() const;
    void unloadConfig();
    void loadConfig();
    void saveConfig();
    void clearConfig();
    void readLastConfig(bool clear);
    void readConfig(const QString& fileName, bool raise, bool select, bool clear);
    void writeConfig(const QString& fileName);
    void raiseConfig(const QString& fileName);
    void updateMenu(const QStringList &configs);

    void about();
    void panic();
    void newDisplayer();
    void unimplemented();
    void addFiles(const QStringList& files); /*!< add files to an existing playlist */

private:
    void setupMenu();

private slots:
    void onConfigSelection(QAction* action);
    void onLockStateChange(int state);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    ProgramEditor* mProgramEditor;
    ManagerEditor* mManagerEditor;
    QMenu* mConfigMenu;

};

#endif // MAIN_WINDOW_H
