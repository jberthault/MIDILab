# MIDILab

A Versatile MIDI Controller, Player, Recorder, Editor ... for Windows &amp; Linux.

<img align="right" width="150" src="https://github.com/jberthault/MIDILab/blob/master/src/data/logo.png">

This application provides a GUI that lets you:
- plug multiple devices and tweak them in realtime
- play any MIDI file using connected devices or locally on a SoundFont synthesizer
- record any stream and save it as MIDI files for later
- edit files (*well not now, but I'm on it*)
- customize interface &amp; behavior via xml configurations

The development started out as a toy project few years ago.
And now that I use it more often than other widespread MIDI players, I want to share it as an open-source project.

So I hope you will enjoy and experiment the wonderful world of MIDI !

## Disclaimer

By now, MIDILab is clearly not user-friendly, in addition to being poorly tested and documented.
I'm working on the latter but you should not consider any of this work to be stable or production-ready.

This if the reason why I need feeback to improve the app.
So let me know if you experience a bug or any incomprehensible behavior.

Please also keep in mind that I develop MIDILab on my spare time.
Fixing flaws is a really time-consuming task.
Don't expect too much too quickly.

## INSTALL

If you want to give it a try, you may take a look at the prebuilt binaries.
I will try to update Windows installers for the latest [release](https://github.com/jberthault/MIDILab/releases/).

Unfortunately, for other platforms, you will have to build it from the sources.
Any help to provide packaging for major Linux distributions is welcome.

First of all, you will need a C++ toolchain supporting C++14.
A recent version of [CMake](https://cmake.org/) is also required.

MIDILab depends on the following libraries:
- [Qt5](http://doc.qt.io/qt-5/index.html)
- [Boost](http://www.boost.org/)
- [fluidsynth](https://github.com/FluidSynth/fluidsynth)

Make sure you have those dependencies installed before running CMake.

Most icons come from [Open Iconic](https://github.com/iconic/open-iconic) but no additional installation is required.

On Windows, the simplest way to go is to use [MSYS2](http://www.msys2.org/) together with MinGW.
Once installed, use `pacman` in order to install the toolchain, CMake, Qt5, Boost &amp; fluidsynth.
See [this](https://github.com/orlp/dev-on-windows/wiki/Installing-GCC--&-MSYS2) guide for further information.

On Linux, your favorite package manager should do the job as well.
Just make sure you install the development version of these packages.

However, I did not try other platforms (e.g. macOS).
So any feedback would be appreciated.

Once you managed to get your environnement ready, the rest is straightforward as long as the dependencies are in your path:
```
git clone https://github.com/jberthault/MIDILab.git
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../MIDILab
make
```

Moreover, make sure your path contains the runtime dependencies, namely Qt5 and fluidsynth, before launching the application.
The packages directory contains the dll required at runtime on Windows.

On Linux, fluidsynth will try to start jackd. It implies that it is installed on your system.
It will provide low-latency audio, but the downside is that the process will reserve your audio output for its own.
More about jack: [http://jackaudio.org/](http://jackaudio.org/)

## Quick Start

*work in progress ...*

## Future Work

There are some features that I plan to introduce, here is a brief sample:
- Pianoroll
- Metronome
- Virtual instruments such as Guitar and Drums
- Command-line configuration
- Internationalization
- VSTi support
- Plugins
- ...

But for now, I'm more focused on improving what exists, mostly usability.
