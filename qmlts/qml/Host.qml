import QtQml
import QtQuick
import QtScxml
import "../build/cli.mjs" as Cli
import "../build/files.mjs" as Files
import "../build/machineGate.mjs" as Gate
import "../build/script.mjs" as Script

// The TypeScript host's root (docs/HOST-CONTRACT.md): loads the shared statechart,
// creates the host objects, creates shared/qml/Main.qml unless --no-gui, and runs
// the command line. Start it with qmlts/watch-me-qmlts. All logic is in src/*.mts.
QtObject {
    id: host

    readonly property url sharedDir: Qt.resolvedUrl("../../shared/")
    readonly property url docsDir: Qt.resolvedUrl("../../docs/")
    readonly property var parsed: Cli.parseOptions(Cli.appArguments(Qt.application.arguments))
    readonly property var options: parsed.ok ? parsed.options : null
    property var window: null                   // Main.qml, kept referenced
    property bool started: false

    // ---- one-shot timers on the event loop (the `Later` port of src/ports.mts)
    property Component timerComponent: Component { Timer {} }
    function later(ms, fn) {
        const t = timerComponent.createObject(host, { interval: Math.max(0, ms) })
        t.triggered.connect(() => { t.destroy(); fn() })
        t.start()
    }
    function fail(code, message) {
        console.error(message)
        Qt.exit(code)
    }

    // ---- the statechart, loaded at run time (https://doc.qt.io/qt-6/qml-qtscxml-statemachineloader.html).
    // The loader starts the machine by itself once the file is parsed.
    property StateMachineLoader loader: StateMachineLoader {
        source: host.options && !host.options.help ? host.sharedDir + "statechart/watch.scxml" : ""
    }
    readonly property QtObject machine: loader.stateMachine
    readonly property var gate: new Gate.MachineGate(() => host.loader.stateMachine,
                                                     (ms, fn) => host.later(ms, fn))
    property Connections machineSignals: Connections {
        target: host.machine
        function onReachedStableState() { host.gate.onStable() }
        function onRunningChanged() { host.begin() }
    }

    // ---- the host objects (HOST-CONTRACT §3)
    property ScxmlTreeModel tree: ScxmlTreeModel {}
    property SpecTags specTags: SpecTags {}
    property Beeper beeper: Beeper {}
    property TransitionTracer tracer: TransitionTracer {
        gate: host.gate; stateMachine: host.machine; treeCore: host.tree.core
        cwd: host.options ? host.options.cwd : ""
    }
    property ButtonRouter buttons: ButtonRouter { gate: host.gate; stateMachine: host.machine; tracer: host.tracer }
    property WatchModel watch: WatchModel {
        gate: host.gate; stateMachine: host.machine
        running: host.options !== null && !host.options.manualTime
    }
    property TimeoutService timeouts: TimeoutService {
        gate: host.gate; stateMachine: host.machine
        running: host.options !== null && !host.options.manualTime
    }

    Component.onCompleted: {
        if (!parsed.ok)
            return fail(1, parsed.error + "\n\n" + Cli.HELP)
        if (options.help) {
            console.log(Cli.HELP)
            return Qt.exit(0)
        }
        if (!machine)
            return fail(2, "watch-me: could not load " + sharedDir + "statechart/watch.scxml")
        if (machine.parseErrors.length > 0)
            return fail(2, "watch-me: " + machine.parseErrors.join("\n"))
        if (!machine.dataModel)
            return fail(2, "watch-me: Qt SCXML's ECMAScript datamodel plugin was not found")

        const scxml = Files.readText(sharedDir + "statechart/watch.scxml")
        if (scxml === null)
            return fail(2, "watch-me: cannot read the statechart; run with QML_XHR_ALLOW_FILE_READ=1 (qmlts/watch-me-qmlts sets it)")
        if (!tree.load(scxml))
            return fail(2, "watch-me: " + tree.errorString)
        specTags.parse(Files.readText(docsDir + "STATECHART-SPEC.md") || "")

        watch.core.setResetTime(options.time)
        watch.shortBeep.connect(beeper.shortBeep)

        if (!options.noGui && !createWindow())
            return
        if (machine.running)
            begin()
    }

    function createWindow() {
        let layout = {}
        try {
            layout = JSON.parse(Files.readText(sharedDir + "statechart/layout.json") || "{}")
        } catch (e) {
            console.warn("watch-me: layout.json: " + e)
        }
        // Main.qml's required properties: the host contract. createObject sets them
        // before completion, like QQmlApplicationEngine::setInitialProperties.
        // https://doc.qt.io/qt-6/qml-qtqml-component.html#createObject-method
        const component = Qt.createComponent(sharedDir + "qml/Main.qml")
        if (component.status !== Component.Ready) {
            fail(1, "watch-me: " + component.errorString())
            return false
        }
        window = component.createObject(null, {
            tree: tree, tracer: tracer, buttons: buttons, watch: watch,
            beeper: beeper, specTags: specTags, appLayout: layout,
        })
        if (!window) {
            fail(1, "watch-me: could not create Main.qml: " + component.errorString())
            return false
        }
        if (options.size) {
            window.width = options.size.width
            window.height = options.size.height
        }
        return true
    }

    // Once the machine runs: --auto-insert, then the script (§5).
    function begin() {
        if (started || !machine || !machine.running || !tree.root.id)
            return
        started = true
        if (options.autoInsert)
            later(0, () => buttons.submit("battery.inserted"))
        new Script.ScriptRunner(options.script, {
            later: (ms, fn) => host.later(ms, fn),
            press: (b) => buttons.press(b),
            release: (b) => buttons.release(b),
            submit: (e) => buttons.submit(e),
            setDataValue: (n, v) => buttons.setDataValue(n, v),
            setAlarm: (w, m) => watch.setAlarm(w, m),
            jumpBeforeAlarm: (w, s) => watch.jumpBeforeAlarm(w, s),
            setTime: (h, m, s) => watch.setTime(h, m, s),
            advanceClock: (ms) => watch.advance(ms),
            advanceTimeouts: (ms, done) => timeouts.advance(ms, done),
            setSpeed: (x) => { watch.speed = x },
            exportLog: (path, done) => { if (!tracer.exportLog(path, done)) done(false) },
            select: (id) => {
                const view = window ? Script.findByObjectName(window.contentItem, "statechartView") : null
                if (view) view.selected = id
                return window !== null
            },
            finished: () => finish(),
        }).start()
    }

    function finish() {
        const delay = options.grabDelay
        if (options.grab !== "")
            later(delay, () => grab(String(options.grab).replace(/^file:\/\//, "")))
        else if (options.exit)
            later(delay, () => Qt.quit())
    }

    // --grab: QML cannot grab a window (QQuickWindow::grabWindow is C++ only), and the
    // window's content item was created by C++, so its grabToImage fails ("item has
    // no QML engine"). Move the content into a QML-created item that paints the
    // window colour, and grab that. We quit right after, so nothing is moved back.
    // https://doc.qt.io/qt-6/qml-qtquick-item.html#grabToImage-method
    property Component grabFrame: Component {
        Rectangle { anchors.fill: parent }
    }
    function grab(path) {
        const content = window ? window.contentItem : null
        if (!content) {
            console.warn("could not grab the window")
            return Qt.quit()
        }
        const frame = grabFrame.createObject(content, { color: window.color })
        const kids = []
        for (let i = 0; i < content.children.length; ++i)
            if (content.children[i] !== frame)
                kids.push(content.children[i])
        for (const k of kids)
            k.parent = frame
        const ok = frame.grabToImage((result) => {
            if (!result.saveToFile(path))
                console.warn("could not save " + path)
            Qt.quit()
        })
        if (!ok) {
            console.warn("could not grab the window")
            Qt.quit()
        }
    }
}
