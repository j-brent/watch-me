//! watch-me-rs: the Rust host of watch-me (docs/HOST-CONTRACT.md).
//! Command line (§5), start-up, the QML window and the `--script` runner.

#[allow(dead_code)] // cxx-qt generates emitters for the shim's signals; only C++ emits them
mod ffi;
mod host;
mod qobjects;
mod qt;
mod tags;
mod tree;
mod watch;

use crate::ffi::ffi as shim;
use crate::host::{host, Host};
use crate::qobjects::qobj;
use crate::qt::{json_to_variant, qmap, qs};
use cxx_qt::casting::Upcast;
use cxx_qt_lib::{
    DateFormat, QCoreApplication, QDateTime, QGuiApplication, QObjectMutPtr, QQmlApplicationEngine,
    QUrl, QVariant,
};
use std::path::{Path, PathBuf};
use std::pin::Pin;

const HELP: &str = "\
Usage: watch-me-rs [options]
Harel's 1987 watch statechart, executable (Rust host).

Options:
  -h, --help          Displays help on commandline options.
  --auto-insert       Insert the battery at start-up.
  --time <datetime>   Clock value set when the battery is inserted (ISO 8601, e.g. 1987-06-01T10:08:30).
  --size <WxH>        Window size, e.g. 1920x1080.
  --script <steps>    Semicolon separated steps run after start-up: 'a' (click), '+a'/'-a'
                      (hold/release), 'wait:ms', or any SCXML event name.
  --manual-time       Time does not follow the real clock; only --script steps 'clock:ms' and
                      'advance:ms' move the watch time and the statechart's timeouts.
  --exit              Quit when the --script has run.
  --grab <file>       Save a screenshot of the window to <file> and quit.
  --grab-delay <ms>   Delay before --grab (ms). Default 800.
  --no-gui            No window: run the statechart, input path, event log and --script only.

The shared files (shared/, docs/) are found under $WATCHME_ROOT, else next to the
executable's repository checkout (rust/target/<profile>/../../..).";

#[derive(Default)]
struct Options {
    no_gui: bool,
    auto_insert: bool,
    time: Option<String>,
    manual_time: bool,
    script: Option<String>,
    exit: bool,
    grab: Option<String>,
    grab_delay: i64,
    size: Option<String>,
}

fn fail(msg: &str) -> ! {
    eprintln!("watch-me-rs: {msg}");
    std::process::exit(1);
}

/// The options of docs/HOST-CONTRACT.md §5, `--opt value` or `--opt=value`.
fn parse_args(args: &[String]) -> Options {
    let mut o = Options { grab_delay: 800, ..Default::default() };
    let mut i = 0;
    while i < args.len() {
        let a = &args[i];
        i += 1;
        let (name, inline) = match a.split_once('=') {
            Some((n, v)) if a.starts_with("--") => (n.to_string(), Some(v.to_string())),
            _ => (a.clone(), None),
        };
        let mut value = || -> String {
            if let Some(v) = inline.clone() {
                return v;
            }
            let v = args.get(i).cloned().unwrap_or_else(|| fail(&format!("missing value after '{name}'")));
            i += 1;
            v
        };
        match name.as_str() {
            "-h" | "--help" | "-help" => {
                println!("{HELP}");
                std::process::exit(0);
            }
            "--no-gui" => o.no_gui = true,
            "--auto-insert" => o.auto_insert = true,
            "--manual-time" => o.manual_time = true,
            "--exit" => o.exit = true,
            "--time" => o.time = Some(value()),
            "--script" => o.script = Some(value()),
            "--grab" => o.grab = Some(value()),
            "--grab-delay" => o.grab_delay = value().trim().parse().unwrap_or(0),
            "--size" => o.size = Some(value()),
            // Qt's own options, already consumed by Q(Gui)Application.
            "-platform" | "-style" | "-qmljsdebugger" => {
                let _ = value();
            }
            _ if name.starts_with('-') => fail(&format!("Unknown option '{}'.", name.trim_start_matches('-'))),
            _ => {} // positional arguments are ignored
        }
    }
    o
}

/// The repository root: shared/statechart/watch.scxml and docs/STATECHART-SPEC.md.
fn find_root() -> PathBuf {
    let marker = |p: &Path| p.join("shared/statechart/watch.scxml").is_file();
    if let Some(r) = std::env::var_os("WATCHME_ROOT").map(PathBuf::from) {
        return r;
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(r) = exe.ancestors().skip(1).find(|p| marker(p)) {
            return r.to_path_buf();
        }
    }
    let built = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    if marker(&built) {
        return built;
    }
    fail("cannot find shared/statechart/watch.scxml; set WATCHME_ROOT to the repository root")
}

fn quit(code: i32) {
    shim::quit_app(code);
}

/// One script step (§5.1); returns the gap before the next one.
fn run_step(s: &str) -> i64 {
    let h = host();
    let parts: Vec<&str> = s.split(':').collect();
    let int = |i: usize| parts[i].trim().parse::<i32>().unwrap_or(0);
    let chars: Vec<char> = s.chars().collect();
    match (parts[0], parts.len()) {
        _ if s.starts_with("wait:") => return s[5..].trim().parse().unwrap_or(0),
        ("alarm", 4) => h.set_alarm(int(1), int(2) * 60 + int(3)), // alarm:1:07:15
        ("jump", 3) => h.jump_before_alarm(int(1), int(2)),        // jump:1:5 (T1 - 5 s)
        ("settime", 4) => h.set_time(int(1), int(2), int(3)),      // settime:10:59:58
        ("advance", 2) => h.advance_timeouts(parts[1].trim().parse().unwrap_or(0)), // timeout clock, ms
        ("clock", 2) => h.advance_watch(parts[1].trim().parse().unwrap_or(0)),      // watch time T, ms
        ("speed", 2) => h.set_speed(parts[1].trim().parse().unwrap_or(0.0)),
        _ if s.starts_with("export:") => {
            // the rest of the step, whole: a Windows path has a ':' of its own (C:\...)
            if !h.tracer.borrow().export_log(&s[7..]) {
                eprintln!("could not export the log to {}", &s[7..]);
            }
        }
        ("select", 2) => {
            let engine = h.engine.get();
            if engine.is_null() {
                eprintln!("watch-me-rs: 'select:' needs the window; ignored with --no-gui");
            } else {
                // SAFETY: the engine lives until app.exec() returns (main).
                let engine = unsafe { &*engine };
                shim::set_child_property(engine, &qs("statechartView"), &qs("selected"), &qs(parts[1]));
            }
        }
        ("set", 3) => {
            // set:harelAnomalies:false
            let v = match parts[2] {
                "true" => QVariant::from(&true),
                "false" => QVariant::from(&false),
                x => QVariant::from(&x.trim().parse::<f64>().unwrap_or(0.0)),
            };
            h.set_data_value(parts[1], &v);
        }
        _ if chars.len() == 1 => {
            h.press(s);
            let b = s.to_string();
            h.schedule(30, move || host().release(&b));
        }
        _ if chars.len() == 2 && chars[0] == '+' => h.press(&s[1..]),
        _ if chars.len() == 2 && chars[0] == '-' => h.release(&s[1..]),
        _ => h.submit_input(s),
    }
    60
}

fn run_next() {
    let h = host();
    let step = h.script.borrow_mut().steps.pop_front();
    match step {
        Some(s) => {
            let gap = run_step(s.trim());
            h.schedule(gap, run_next);
        }
        None => {
            let (exit, grab, delay) = {
                let s = h.script.borrow();
                (s.exit, s.grab.clone(), s.grab_delay)
            };
            if let Some(file) = grab {
                h.schedule(delay, move || {
                    let engine = host().engine.get();
                    // SAFETY: as in run_step.
                    if engine.is_null() || !shim::grab_window(unsafe { &*engine }, &qs(&file)) {
                        eprintln!("could not save {file}");
                    }
                    quit(0);
                });
            } else if exit {
                h.schedule(delay, || quit(0));
            }
        }
    }
}

fn as_qobject<T: Upcast<cxx_qt::QObject>>(p: &cxx::UniquePtr<T>) -> QVariant
where
    T: cxx::memory::UniquePtrTarget,
{
    // SAFETY: the objects outlive the QML engine (main drops the engine first).
    let ptr = unsafe { T::upcast_ptr(p.as_ref().unwrap() as *const T) } as *mut cxx_qt::QObject;
    QVariant::from(&unsafe { QObjectMutPtr::from_raw(ptr) })
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let opts = parse_args(&args);
    if opts.no_gui && (opts.grab.is_some() || opts.size.is_some()) {
        eprintln!("watch-me-rs: --grab and --size need the window; they cannot be used with --no-gui.");
        std::process::exit(1);
    }
    let root = find_root();

    // https://doc.qt.io/qt-6/qcoreapplication.html (no GUI) vs. QGuiApplication
    let mut core_app = None;
    let mut gui_app = None;
    if opts.no_gui {
        core_app = Some(QCoreApplication::new());
    } else {
        gui_app = Some(QGuiApplication::new());
    }
    if let Some(app) = core_app.as_mut().and_then(|a| a.as_mut()) {
        app.set_application_name(&qs("watch-me"));
    }
    if let Some(app) = gui_app.as_mut().and_then(|a| a.as_mut()) {
        app.set_application_name(&qs("watch-me"));
    }

    // The state machine (C++ shim around QScxmlStateMachine).
    let mut machine = shim::new_watch_scxml();
    let scxml = root.join("shared/statechart/watch.scxml");
    let errors = machine.pin_mut().load(&qs(&scxml.to_string_lossy())).to_string();
    if !errors.is_empty() {
        eprintln!("watch-me-rs: {} does not parse:\n{errors}", scxml.display());
        std::process::exit(2);
    }
    if !machine.has_data_model() {
        eprintln!(
            "watch-me-rs: Qt SCXML's ECMAScript datamodel plugin (plugins/scxmldatamodel) \
             was not found; the installation is incomplete."
        );
        std::process::exit(2);
    }

    let tree = tree::Tree::load(&scxml);
    let tags = tags::Tags::load(&root.join("docs/STATECHART-SPEC.md"));
    let h = Host::install(machine.as_mut_ptr(), tree, tags);

    if let Some(t) = &opts.time {
        match QDateTime::from_string(&qs(t), DateFormat::ISODate) {
            Some(dt) if dt.is_valid() => {
                let (d, tm) = (dt.date(), dt.time());
                h.watch.borrow_mut().reset_time =
                    Some((d.year(), d.month(), d.day(), tm.hour(), tm.minute(), tm.second()));
            }
            _ => eprintln!("watch-me-rs: --time '{t}' is not an ISO 8601 date and time; ignored"),
        }
    }

    // The host objects (docs/HOST-CONTRACT.md §3).
    let q_watch = qobj::new_watch_model();
    let q_tracer = qobj::new_tracer();
    let q_buttons = qobj::new_button_router();
    let q_tree = qobj::new_tree_model();
    let q_tags = qobj::new_spec_tags();
    let q_beeper = qobj::new_beeper();
    h.q_watch.set(q_watch.as_mut_ptr());
    h.q_tracer.set(q_tracer.as_mut_ptr());
    h.q_buttons.set(q_buttons.as_mut_ptr());

    // Machine -> host. The closures must be Send + 'static (cxx-qt), so they
    // reach the host through host() instead of capturing it.
    {
        let mut m: Pin<&mut shim::WatchScxml> = machine.pin_mut();
        m.as_mut()
            .on_machine_event(|_, name, data| host().on_machine_event(&name.to_string(), data))
            .release();
        m.as_mut().on_logged(|_, label, msg| host().on_log(&label.to_string(), &msg.to_string())).release();
        m.as_mut().on_stable(|_| host().on_stable()).release();
        m.as_mut().on_initialized(|_| host().push_alarms()).release();
        for spec in ["model.*", "sw.*", "beep.*", "timer.*", "host.data"] {
            m.as_mut().observe(&qs(spec));
        }
    }
    h.on_stable();
    if !opts.manual_time {
        h.start_running();
    }

    let mut engine: Option<cxx::UniquePtr<QQmlApplicationEngine>> = None;
    if !opts.no_gui {
        let mut e = QQmlApplicationEngine::new();
        let layout = std::fs::read_to_string(root.join("shared/statechart/layout.json"))
            .ok()
            .and_then(|s| serde_json::from_str::<serde_json::Value>(&s).ok())
            .map(|v| json_to_variant(&v))
            .unwrap_or_default();
        // Main.qml's required properties: the host contract.
        // https://doc.qt.io/qt-6/qqmlapplicationengine.html#setInitialProperties
        let props = qmap([
            ("tree", as_qobject(&q_tree)),
            ("tracer", as_qobject(&q_tracer)),
            ("buttons", as_qobject(&q_buttons)),
            ("watch", as_qobject(&q_watch)),
            ("beeper", as_qobject(&q_beeper)),
            ("specTags", as_qobject(&q_tags)),
            ("appLayout", layout),
        ]);
        {
            let mut ep = e.pin_mut();
            ep.as_mut().set_initial_properties(&props);
            ep.as_mut().on_object_creation_failed(|_, _| quit(1)).release();
            let main_qml = root.join("shared/qml/Main.qml");
            ep.as_mut().load(&QUrl::from_local_file(&qs(&main_qml.to_string_lossy())));
        }
        if let Some((w, hgt)) = opts.size.as_deref().and_then(|s| s.split_once('x')) {
            shim::resize_window(&e, w.trim().parse().unwrap_or(0), hgt.trim().parse().unwrap_or(0));
        }
        h.engine.set(e.as_mut_ptr());
        engine = Some(e);
    }

    machine.pin_mut().start();
    if opts.auto_insert {
        h.schedule(0, || host().submit_input("battery.inserted"));
    }

    {
        let mut s = h.script.borrow_mut();
        s.steps = opts
            .script
            .as_deref()
            .unwrap_or("")
            .split(';')
            .filter(|x| !x.is_empty())
            .map(String::from)
            .collect();
        s.exit = opts.exit;
        s.grab = opts.grab.clone();
        s.grab_delay = opts.grab_delay;
    }
    h.schedule(200, run_next);

    let code = if let Some(app) = gui_app.as_mut().and_then(|a| a.as_mut()) {
        app.exec()
    } else {
        core_app.as_mut().and_then(|a| a.as_mut()).map(|a| a.exec()).unwrap_or(1)
    };

    // Tear down in order: the window refers to the host objects, they and the
    // machine to the application. (The host and its QTimers are leaked on purpose.)
    h.engine.set(std::ptr::null_mut());
    drop(engine);
    h.q_watch.set(std::ptr::null_mut());
    h.q_tracer.set(std::ptr::null_mut());
    h.q_buttons.set(std::ptr::null_mut());
    drop((q_watch, q_tracer, q_buttons, q_tree, q_tags, q_beeper));
    drop(machine);
    drop(gui_app);
    drop(core_app);
    std::process::exit(code);
}
