import QtQml

// HOST-CONTRACT §3 `beeper`. No audio in this host: QML alone cannot synthesise a
// tone without Qt Multimedia's SoundEffect and a WAV file on disk, so it reports
// available = false and the "sound" box stays disabled.
QtObject {
    property bool enabled: false
    readonly property bool available: false
    property bool continuous: false
    function shortBeep() {}
}
