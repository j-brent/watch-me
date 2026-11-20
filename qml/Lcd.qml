import QtQuick

// The LCD. What it shows is a pure function of the active configuration
// (tracer.activeSet) and WatchModel's values — spec §4. No mode logic here.
Rectangle {
    id: lcd
    property var a: tracer.activeSet          // active state ids -> true
    function on(id) { return a[id] === true }

    // 2 Hz field blink (update), 1 Hz weak-battery blink, 2 Hz alarm flash
    property bool phase2: true
    property bool phase1: true
    Timer { interval: 250; running: true; repeat: true; onTriggered: lcd.phase2 = !lcd.phase2 }
    Timer { interval: 500; running: true; repeat: true; onTriggered: lcd.phase1 = !lcd.phase1 }

    readonly property bool dead: !on("alive")
    readonly property bool lit: on("light_on")
    readonly property bool beeping: on("alarms_beep") || on("chime_beep") || on("bt_beep_on") || shortBeep
    property bool shortBeep: false
    // audio follows the beeping states (optional, off by default)
    Binding { target: beeper; property: "continuous"; value: lcd.on("alarms_beep") || lcd.on("chime_beep") || lcd.on("bt_beep_on") }
    Connections {
        target: watch
        function onShortBeep() { lcd.shortBeep = true; shortBeepTimer.restart() }
    }
    Timer { id: shortBeepTimer; interval: 500; onTriggered: lcd.shortBeep = false }

    // whole-LCD visibility: weak battery blinks at 1 Hz, alarms-beep flashes at 2 Hz
    readonly property bool contentVisible: !dead && !(on("power_blink") && !phase1)
                                           && !(on("alarms_beep") && !phase2)

    color: lit ? "#9fe6d6" : "#b9c4a6"
    border.color: "#3a3f36"
    border.width: 3
    radius: 10
    implicitWidth: 330
    implicitHeight: 190

    Behavior on color { ColorAnimation { duration: 120 } }

    readonly property var days: ["MO", "TU", "WE", "TH", "FR", "SA", "SU"]
    function pad2(n, ch) { const s = "" + n; return s.length >= 2 ? s.slice(-2) : (ch === undefined ? "0" : ch) + s }
    function hour12(h) { return watch.is24h ? h : ((h % 12) === 0 ? 12 : h % 12) }

    // The display, derived from the configuration (spec §4).
    readonly property var view: {
        const v = { label: "", main: "    ", sep: ":", small: "  ", blinkMain: [], blinkSmall: false,
                    blinkLabel: false, status: "", lap: false, ampm: "", ampmBlink: false };
        if (lcd.dead) return v;
        const blink = !lcd.phase2;
        const timeMain = (h, m) => pad2(hour12(h), " ") + pad2(m);
        const ampm = h => watch.is24h ? "24H" : (h < 12 ? "AM" : "PM");
        if (on("update") && (on("update_mon") || on("update_date") || on("update_day") || on("update_year"))) {
            v.label = days[watch.weekday - 1];
            if (on("update_year")) {
                v.main = "" + watch.year; v.sep = " ";
                if (blink) v.blinkMain = [0, 1, 2, 3];
            } else {
                v.main = pad2(watch.month, " ") + pad2(watch.day, " "); v.sep = "-";
                if (blink && on("update_mon")) v.blinkMain = [0, 1];
                if (blink && on("update_date")) v.blinkMain = [2, 3];
                v.blinkLabel = blink && on("update_day");
            }
            return v;
        }
        if (on("time") || on("wait") || on("update") || on("alarms_beep")) {
            v.label = on("update") ? "SET" : days[watch.weekday - 1];
            v.main = timeMain(watch.hour, watch.minute);
            v.small = pad2(watch.second);
            v.ampm = ampm(watch.hour);
            if (blink) {
                if (on("update_sec")) v.blinkSmall = true;
                if (on("update_1min")) v.blinkMain = [3];
                if (on("update_10min")) v.blinkMain = [2];
                if (on("update_hr")) v.blinkMain = [0, 1];
                if (on("update_mode")) v.ampmBlink = true;
            }
            return v;
        }
        if (on("date")) {
            v.label = days[watch.weekday - 1];
            v.main = pad2(watch.month, " ") + pad2(watch.day, " "); v.sep = "-";
            return v;
        }
        for (const n of [1, 2]) {
            if (on("alarm" + n) || on("update" + n)) {
                const t = n === 1 ? watch.t1 : watch.t2;
                v.label = "AL" + n;
                v.main = timeMain(Math.floor(t / 60), t % 60);
                v.ampm = ampm(Math.floor(t / 60));
                v.status = on("alarm" + n + "_on") ? "ON" : on("alarm" + n + "_off") ? "OFF" : "";
                if (blink && on("update" + n + "_hr")) v.blinkMain = [0, 1];
                if (blink && on("update" + n + "_10min")) v.blinkMain = [2];
                if (blink && on("update" + n + "_1min")) v.blinkMain = [3];
                return v;
            }
        }
        if (on("chime")) {
            v.label = "CH"; v.main = "--00";
            v.status = on("chime_on") ? "ON" : "OFF";
            return v;
        }
        if (on("stopwatch")) {
            const cs = on("sw_lap") ? watch.lapCentis : watch.stopwatchCentis;
            const tot = Math.floor(cs / 100);
            v.label = "ST";
            v.main = pad2(Math.floor(tot / 60) % 60) + pad2(tot % 60);
            v.small = pad2(cs % 100);
            v.lap = on("sw_lap");
            return v;
        }
        return v;
    }

    readonly property color ink: "#1b1f1a"
    readonly property color ghost: lit ? "#1b1f1a14" : "#1b1f1a12"

    Item {
        anchors.fill: parent
        anchors.margins: 12
        visible: lcd.contentVisible

        // top row: four indicator windows (chime, alarm 1, alarm 2, stopwatch), cf. Fig. 7
        Row {
            id: icons
            spacing: 8
            Repeater {
                model: [
                    { txt: "CHIME", show: lcd.on("chime_enabled") },
                    { txt: "AL 1", show: lcd.on("alarm1_enabled") },
                    { txt: "AL 2", show: lcd.on("alarm2_enabled") },
                    { txt: "STW", show: watch.stopwatchRunning }
                ]
                delegate: Rectangle {
                    width: 70; height: 30; radius: 4
                    color: "transparent"
                    border.color: "#3a3f3655"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.txt
                        visible: modelData.show
                        color: lcd.ink
                        font.pixelSize: 16; font.bold: true
                    }
                }
            }
        }

        Text {
            id: label
            anchors.top: icons.bottom; anchors.topMargin: 10
            text: lcd.view.label
            visible: !lcd.view.blinkLabel
            color: lcd.ink
            font.pixelSize: 20; font.bold: true; font.family: "monospace"
        }
        Text {
            anchors.top: label.bottom; anchors.topMargin: 2
            text: lcd.view.status
            color: lcd.ink
            font.pixelSize: 14; font.bold: true
        }
        Text {
            anchors.top: icons.bottom; anchors.topMargin: 10
            anchors.right: parent.right
            text: lcd.view.lap ? "LAP" : ""
            color: lcd.ink
            font.pixelSize: 14; font.bold: true
        }

        Row {
            id: digits
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            spacing: 6
            LcdDigits {
                text: lcd.view.main.substr(0, 2)
                blank: lcd.view.blinkMain.filter(i => i < 2)
                digitWidth: 38; digitHeight: 70
                onColor: lcd.ink; offColor: lcd.ghost
            }
            Item {
                width: 14; height: 70
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    visible: lcd.view.sep === ":"
                    Rectangle { width: 7; height: 7; color: lcd.ink }
                    Rectangle { width: 7; height: 7; color: lcd.ink }
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: lcd.view.sep === "-"
                    width: 12; height: 6; color: lcd.ink
                }
            }
            LcdDigits {
                text: lcd.view.main.substr(2, 2)
                blank: lcd.view.blinkMain.filter(i => i >= 2).map(i => i - 2)
                digitWidth: 38; digitHeight: 70
                onColor: lcd.ink; offColor: lcd.ghost
            }
            Column {
                anchors.bottom: parent.bottom
                spacing: 4
                Text {
                    text: lcd.view.ampm
                    visible: !lcd.view.ampmBlink
                    color: lcd.ink
                    font.pixelSize: 13; font.bold: true
                }
                LcdDigits {
                    text: lcd.view.small
                    blank: lcd.view.blinkSmall ? [0, 1] : []
                    digitWidth: 20; digitHeight: 36
                    onColor: lcd.ink; offColor: lcd.ghost
                }
            }
        }
    }

    // beeper indicator (visual stand-in for the two-tone beeper)
    Rectangle {
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: -10
        width: 40; height: 40; radius: 20
        color: lcd.beeping ? "#e0542f" : "transparent"
        border.color: lcd.beeping ? "#7a2510" : "transparent"
        Text { anchors.centerIn: parent; text: "BEEP"; color: "white"; font.pixelSize: 9; font.bold: true; visible: lcd.beeping }
        SequentialAnimation on scale {
            running: lcd.beeping; loops: Animation.Infinite
            NumberAnimation { to: 1.2; duration: 150 }
            NumberAnimation { to: 1.0; duration: 150 }
        }
    }
}
