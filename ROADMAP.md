# Roadmap

Here is a list of things I want to do with MidiLab.
Some of them may never be implemented, yet this is roughly the plan.

## About Configuration

With a richer **command-line** interface, MidiLab would be easier to use on Linux.
Things like choosing the configuration file should be possible.

Configuration files should not contain properties describing default values.
MidiLab should tell the user any illformed property instead of passing silently.
Version checking should also be introduced to prevent regressions when the format evolves.

## About Synthesizers & Devices

SoundFont, via fluidsynth, provides many features inaccessible within MidiLab.
The editor should let the user configure **banks** and **tuning** for instance.

Apart from SoundFonts, the **VSTi** tecnhology is a must-have and should definitely be part of MidiLab.

Dealing with multiple devices with different latencies is almost impossible.
MidiLab should provide some **latency compensation**.

On Linux, Alsa has its own mechanism of channels, it may be intersesting for MIDILab to fit in this model.

## About Playback

MIDI files are great but cannot carry information such as **guitar tabs**.
Adding a basic support of "Guitar Pro" files or an equivalent could solve this problem.

The **pianoroll** is a common feature providing a good overview of notes played in addition to beeing a good editor.

Improve the algorithm behind the playback. By making transactions of multiple events, playing notes close in time, ...

Many MIDI files embed lyrics. Adding a **karaoke** feature will take adavantage of it.

The user may want to force some settings that are often overwritten by the playback.
There should be a way to lock these settings, namely controllers and programs.

## About Handlers

With a stable interface, It should be possible to write handlers as **plugins**.
Allowing developers to extend MidiLab apart from its core development.

A first version of the guitar fretboard was introduced in v0.3 but there are still plenty of things to do about it.
For example, make the engine aware of the strings assigned to notes.
It could be done using the track number of events.
The current **fingering algorithm** is also rudimentary and yields weird behaviors.
There are many ways of improvement.

The current version of the Harmonica is ugly and unusable.
It should be as easy to use as the Piano or the Guitar.

A handler dedicated to **Drums** is currently missing.

The **keyboard** is not exploited. It could be used by having a dedicated handler
generating events on keystrokes, by adding shortcuts, by controlling handlers, ...

The current **Recorder** interface is poor and does not fit well with the Player.

Instruments could easily filter incoming events for specific channels.
By making this configurable, it will avoid using ChannelMappers or Filters.

## About GUI

In its current state, MidiLab is not user-friendly and may be hard to configure.
I'm focused on improving its **usability** by changing the general ergonomy.
Paired with tips and a good **documentation**, it may become more accessible.

For any sequencer, a good **song editor** is mandatory.
However, it will take time to implement.

Editors containing too much information should provide a basic mode and an **advanced mode**.
This could be done by folding whole sections or by a general property.
Good examples are the reverb settings of SoundFonts or the playlist of Players.

The connection's graph is where magic happens. It needs to be easy to use and provide powerful features:
Edit filters, Remap connections, ...

The handler's catalog should group handlers by category and show a detailed description of each one.

Qt supports **internationalization**. In the future, providing translations would help users.

MidiLab has a multi-window interface. It has a few drawbacks that need to be addressed.
This could be done by allowing windows to be **anchored** to others for example.

## About Internals

Practices regarding Qt change. For example, using Qt containers is not recommended.
Using an automated tool like **clazy** will help spotting these kind of flaws.

Rework extended events to minimize space.
Using strings as identifiers is secure but needs more space, there must be a better way,
like using specific custom status bytes (in range [0, 0x80)).
