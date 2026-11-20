// The QML/JavaScript host of watch-me (docs/HOST-CONTRACT.md, DESIGN-NOTES D-25):
// no compiled code, run by Qt's `qml` tool (see qmljs/watch-me-qmljs). The port
// of cpp/src/main.cpp: load the statechart, create the host objects, create
// shared/qml/Main.qml with them (unless --no-gui), and run the command line.
//
// Only QtQml and QtScxml are imported here, so the same file runs in a core
// application (`qml --apptype core`, no platform plugin) for --no-gui.
import QtQml
import QtScxml
import "files.mjs" as Files

QtObject {
    id: host

    // ---- command line (§5). The launcher puts the app's own options after "--".
    readonly property var argv: {
        const a = Qt.application.arguments
        const i = a.indexOf("--")
        return i < 0 ? [] : a.slice(i + 1)
    }
    readonly property var opts: _parseArgs(argv)

    function _parseArgs(args) {
        const withValue = ["time", "script", "grab", "grab-delay", "size", "cwd"]
        const flags = ["no-gui", "auto-insert", "manual-time", "exit", "help"]
        const o = { "grab-delay": "800", errors: [] }
        for (let i = 0; i < args.length; ++i) {
            const arg = args[i]
            if (!arg.startsWith("--")) { o.errors.push("unexpected argument '" + arg + "'"); continue }
            const eq = arg.indexOf("=")
            const name = eq < 0 ? arg.slice(2) : arg.slice(2, eq)
            if (flags.includes(name) && eq < 0) {
                o[name] = true
            } else if (withValue.includes(name)) {
                if (eq >= 0) o[name] = arg.slice(eq + 1)
                else if (i + 1 < args.length) o[name] = args[++i]
                else o.errors.push("missing value for '--" + name + "'")
            } else {
                o.errors.push("unknown option '" + arg + "'")
            }
        }
        return o
    }

    readonly property string usage:
        "Usage: watch-me-qmljs [options]\n" +
        "Harel's 1987 watch statechart, executable (QML/JavaScript host).\n\n" +
        "  --no-gui          No window: statechart, clock, input, log and --script only.\n" +
        "  --auto-insert     Insert the battery at start-up.\n" +
        "  --time ISO        Clock value set when the battery is inserted (e.g. 1987-06-01T10:08:30).\n" +
        "  --manual-time     Time does not follow the real clock; only 'clock:ms' and 'advance:ms' move it.\n" +
        "  --script STEPS    Semicolon separated steps run after start-up.\n" +
        "  --exit            Quit when the --script has run.\n" +
        "  --grab FILE       Save a screenshot of the window to FILE and quit.\n" +
        "  --grab-delay MS   Delay before --grab (800).\n" +
        "  --size WxH        Window size, e.g. 1920x1080.\n"

    function _toInt(s) { const n = parseInt(String(s).trim(), 10); return /^\s*[+-]?\d+\s*$/.test(String(s)) ? n : 0 }
    function _toNumber(s) { const n = Number(String(s).trim()); return String(s).trim() !== "" && isFinite(n) ? n : 0 }

    // "1987-06-01T10:08:30" -> ms of that wall-clock time in UTC; null if invalid.
    function _parseIsoWall(s) {
        const m = /^(\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{1,3}))?)?(Z|[+-]\d{2}:?\d{2})?$/.exec(String(s).trim())
        if (!m) return null
        return Date.UTC(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], m[6] ? +m[6] : 0, m[7] ? +(m[7] + "00").slice(0, 3) : 0)
    }

    // ---- shared files, relative to this file
    readonly property url scxmlUrl: Qt.resolvedUrl("../shared/statechart/watch.scxml")
    readonly property url layoutUrl: Qt.resolvedUrl("../shared/statechart/layout.json")
    readonly property url specUrl: Qt.resolvedUrl("../docs/STATECHART-SPEC.md")
    readonly property url mainUrl: Qt.resolvedUrl("../shared/qml/Main.qml")

    // ---- the state machine. StateMachineLoader parses watch.scxml at run time
    // (https://doc.qt.io/qt-6/qml-qtscxml-statemachineloader.html) and starts it.
    property StateMachineLoader loader: StateMachineLoader { source: host.scxmlUrl }
    readonly property QtObject machine: loader.stateMachine

    // ---- the host objects (HOST-CONTRACT §3)
    property ScxmlTreeModel tree: ScxmlTreeModel {}
    property SpecTags specTags: SpecTags {}
    property TransitionTracer tracer: TransitionTracer { machine: host.machine; tree: host.tree; cwd: host.opts.cwd || "" }
    property WatchModel watch: WatchModel {
        machine: host.machine
        running: !host.opts["manual-time"]
    }
    property TimeoutService timeouts: TimeoutService {
        machine: host.machine
        running: !host.opts["manual-time"]
    }
    property ButtonRouter buttons: ButtonRouter { machine: host.machine; tracer: host.tracer }
    property Beeper beeper: Beeper {}
    property Connections _beep: Connections {
        target: host.watch
        function onShortBeep() { host.beeper.shortBeep() }
    }

    property QtObject window: null       // shared/qml/Main.qml
    property bool _failed: false

    // Qt.exit before the event loop runs does nothing; defer it.
    function fail(message, code) {
        console.error("watch-me-qmljs: " + message)
        _failed = true
        Qt.callLater(() => Qt.exit(code))
    }

    // ---- timers from JavaScript: a one-shot Timer per call
    property Component _timerComponent: Component { Timer { repeat: false } }
    function later(ms, fn) {
        const t = _timerComponent.createObject(host, { interval: Math.max(0, ms) })
        t.triggered.connect(() => { t.destroy(); fn() })
        t.start()
    }

    // Quit once every file write has finished (exportLog writes asynchronously).
    function quit(code) {
        if (Files.pendingWrites() > 0) {
            later(10, () => quit(code))
            return
        }
        Qt.exit(code || 0)
    }

    Component.onCompleted: {
        const o = opts
        if (o.help) { console.log(usage); Qt.callLater(() => Qt.exit(0)); _failed = true; return }
        if (o.errors.length) { fail(o.errors.join("; ") + "\n" + usage, 1); return }
        const noGui = !!o["no-gui"]
        if (noGui && (o.grab !== undefined || o.size !== undefined)) {
            fail("--grab and --size need the window; they cannot be used with --no-gui.", 1)
            return
        }
        if (!machine) {
            fail("could not load " + scxmlUrl + " (see the parse errors above)", 2)
            return
        }
        if (machine.parseErrors && machine.parseErrors.length) {
            fail("parse errors in " + scxmlUrl, 2)
            return
        }
        // The ecmascript datamodel is a Qt SCXML plugin; without it every condition
        // and timer in watch.scxml fails.
        if (!machine.dataModel) {
            fail("Qt SCXML's ECMAScript datamodel plugin (plugins/scxmldatamodel) was not found.", 2)
            return
        }
        if (!tree.load(scxmlUrl)) {
            fail("cannot read " + scxmlUrl + ": " + tree.errorString
                 + " (is QML_XHR_ALLOW_FILE_READ=1 set?)", 2)
            return
        }
        specTags.load(specUrl)
        if (o.time !== undefined) {
            const t = _parseIsoWall(o.time)
            if (t === null) console.warn("watch-me-qmljs: --time '" + o.time + "' is not ISO 8601; ignored")
            watch.resetTime = t
        }

        if (!noGui) {
            const layoutText = Files.readText(layoutUrl)
            let layout = ({})
            try { layout = JSON.parse(layoutText || "{}") } catch (e) { console.warn("layout.json: " + e) }
            const c = Qt.createComponent(mainUrl)
            if (c.status !== Component.Ready) {
                fail("cannot load Main.qml: " + c.errorString(), 1)
                return
            }
            // Main.qml's required properties: the host contract.
            window = c.createObject(null, { tree: tree, tracer: tracer, buttons: buttons, watch: watch,
                                            beeper: beeper, specTags: specTags, appLayout: layout })
            if (!window) {
                fail("cannot create Main.qml: " + c.errorString(), 1)
                return
            }
            if (o.size !== undefined) {
                const wh = String(o.size).split("x")
                if (wh.length === 2) { window.width = _toInt(wh[0]); window.height = _toInt(wh[1]) }
            }
        }
        // The loader has started the machine (a queued start). The rest follows.
        if (o["auto-insert"])
            Qt.callLater(() => buttons.submit("battery.inserted"))
        _steps = String(o.script || "").split(";").filter(s => s.length > 0)
        later(200, runNext)
    }

    // ---- --script (§5.1): each step 60 ms after the previous one
    property var _steps: []

    function runNext() {
        if (_failed)
            return
        if (!_steps.length) {
            const delay = _toInt(opts["grab-delay"])
            if (opts.exit && opts.grab === undefined)
                later(delay, () => quit(0))
            if (opts.grab !== undefined)
                later(delay, grab)
            return
        }
        const s = _steps.shift().trim()
        let gap = 60
        const parts = s.split(":")
        if (s.startsWith("wait:")) {
            gap = _toInt(s.slice(5))
        } else if (parts[0] === "alarm" && parts.length === 4) {        // alarm:1:07:15
            watch.setAlarm(_toInt(parts[1]), _toInt(parts[2]) * 60 + _toInt(parts[3]))
        } else if (parts[0] === "jump" && parts.length === 3) {         // jump:1:5 (T1 - 5 s)
            watch.jumpBeforeAlarm(_toInt(parts[1]), _toInt(parts[2]))
        } else if (parts[0] === "settime" && parts.length === 4) {      // settime:10:59:58
            watch.setTime(_toInt(parts[1]), _toInt(parts[2]), _toInt(parts[3]))
        } else if (parts[0] === "advance" && parts.length === 2) {      // timeout clock, ms
            timeouts.advance(_toInt(parts[1]))
        } else if (parts[0] === "clock" && parts.length === 2) {        // watch time T, ms
            watch.advance(_toInt(parts[1]))
        } else if (parts[0] === "speed" && parts.length === 2) {
            watch.speed = _toNumber(parts[1])
        } else if (s.startsWith("export:")) {
            // the rest of the step, whole: a Windows path has a ':' of its own
            if (!tracer.exportLog(s.slice(7)))
                console.warn("could not export the log to " + s.slice(7))
        } else if (parts[0] === "select" && parts.length === 2) {       // select:time
            const v = window ? _findObject(window.contentItem, "statechartView") : null
            if (!window)
                console.warn("watch-me-qmljs: 'select:' needs the window; ignored with --no-gui")
            else if (v)
                v.selected = parts[1]
        } else if (parts[0] === "set" && parts.length === 3) {          // set:harelAnomalies:false
            const v = parts[2] === "true" ? true : parts[2] === "false" ? false : _toNumber(parts[2])
            buttons.setDataValue(parts[1], v)
        } else if (s.length === 1) {
            buttons.press(s)
            later(30, () => buttons.release(s))
        } else if (s.length === 2 && s[0] === "+") {
            buttons.press(s.slice(1))
        } else if (s.length === 2 && s[0] === "-") {
            buttons.release(s.slice(1))
        } else {
            buttons.submit(s)
        }
        later(gap, runNext)
    }

    // QML has no findChild(): walk the visual tree.
    function _findObject(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        const kids = item.children || []
        for (let i = 0; i < kids.length; ++i) {
            const r = _findObject(kids[i], name)
            if (r) return r
        }
        return null
    }

    // --grab: Item.grabToImage on the window's root item
    // (https://doc.qt.io/qt-6/qml-qtquick-item.html#grabToImage-method).
    function grab() {
        const file = String(opts.grab)
        const item = window ? window.contentItem.parent || window.contentItem : null
        if (!item) { console.warn("could not save " + file); quit(0); return }
        // An item grab does not include the window's clear colour (QQuickWindow::grabWindow
        // does, but it is not callable from QML): put the colour behind the content.
        // Created here, so Host.qml itself needs no QtQuick import (--no-gui).
        Qt.createQmlObject("import QtQuick; Rectangle { anchors.fill: parent; z: -1000 }",
                           item, "backdrop").color = window.color
        const path = /^([A-Za-z]:)?[\/\\]/.test(file) || !opts.cwd ? file : opts.cwd + "/" + file
        const ok = item.grabToImage(result => {
            if (!result.saveToFile(path))
                console.warn("could not save " + file)
            quit(0)
        })
        if (!ok) { console.warn("could not save " + file); quit(0) }
    }
}
