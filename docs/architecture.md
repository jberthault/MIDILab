# Architecture

```
                                  +---------+
                                  | MidiLab |
                                  +---------+
                                       |
                                       v
                   +----------+  +-----------+  |
                 +-| handlers |<-| qhandlers |  | plugins
                 | +----------+  +-----------+  |
                 |      |              |
 +============+  |      v              v
 | fluidsynth |<-+   +------+      +-------+    |
 +============+  |   | core |<-----| qcore |    | business code
                 |   +------+      +-------+    |
   +=======+     |      |              |
   | WinMM |<----+      v              v
   +=======+     |  +-------+     +--------+    |
                 |  | tools |<----| qtools |    | generic code
    +======+     |  +-------+     +--------+    |
    | Alsa |<----+      |              |
    +======+            v              v
                    +=======+       +====+      |
                    | boost |       | Qt |      | third-party dependencies
                    +=======+       +====+      |
                   
  --------------  -------------  -------------
     plugins         engine           GUI
   dependencies 
```
