// `beeper` (docs/HOST-CONTRACT.md §3). Plain QML cannot synthesise a tone (the C++
// host writes a WAV into memory and plays it with Qt Multimedia), so this host has
// no audio: `available` is false and the sim panel's "sound" box stays disabled.
import QtQml

QtObject {
    property bool enabled: false
    readonly property bool available: false
    property bool continuous: false       // written by Main.qml: a beeping state is active
    function shortBeep() {}               // T-56's half-second beep: silent here
}
