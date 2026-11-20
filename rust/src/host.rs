//! The glue: WatchModel, TimeoutService, ButtonRouter and TransitionTracer
//! behaviour (docs/HOST-CONTRACT.md §2–§5), on one `Host` per process.
//!
//! Why one global: the machine calls back into the host from inside Qt (signal
//! handlers run while a send executes, D-21), and cxx-qt requires those
//! closures to be `Send + 'static`, so they cannot capture an `Rc`. They reach
//! the host through `host()`. Each component's state is in its own `RefCell`;
//! a borrow is never held across a call that can re-enter the host (submitting
//! is queued, but draining the machine and emitting signals to QML are not).

use crate::ffi::ffi::{self as shim, QTimer, WatchScxml};
use crate::qobjects::qobj::{ButtonRouter, TransitionTracer, WatchModel};
use crate::qt::{qmap, qs, variant_string, QVariantMap};
use crate::tags::Tags;
use crate::tree::{Transition, Tree};
use crate::watch::Watch;
use cxx_qt_lib::{QDateTime, QQmlApplicationEngine, QVariant};
use std::cell::{Cell, RefCell};
use std::collections::{BTreeMap, BTreeSet, HashSet, VecDeque};
use std::pin::Pin;
use std::time::Instant;

pub const MAX_ENTRIES: usize = 500;

#[derive(Default)]
pub struct Tracer {
    pub active: HashSet<String>,
    prev_active: HashSet<String>,
    pending_flash: HashSet<String>,
    recent: VecDeque<HashSet<String>>, // last stable configurations, newest first
    pub active_list: Vec<String>,
    pub active_map: QVariantMap,
    pub flash_map: QVariantMap,
    pub entries: VecDeque<String>,
    fired: HashSet<String>,
    data: QVariantMap, // last host.data: the hv_* flags (D-24)
    pub last_spec_id: String,
}

impl Tracer {
    pub fn fired_sorted(&self) -> Vec<String> {
        let mut l: Vec<String> = self.fired.iter().cloned().collect();
        l.sort_by_key(|s| s[2..].parse::<u32>().unwrap_or(0));
        l
    }

    pub fn log_text(&self) -> String {
        self.entries.iter().map(String::as_str).collect::<Vec<_>>().join("\n")
    }

    /// docs/HOST-CONTRACT.md §4.4
    pub fn export_log(&self, path: &str) -> bool {
        let path = path.strip_prefix("file://").unwrap_or(path);
        let text = format!("{}\n# active: {}\n", self.log_text(), self.active_list.join(" "));
        std::fs::write(path, text).is_ok()
    }
}

#[derive(Default)]
pub struct Buttons {
    pub held: BTreeSet<String>,
    pub data: QVariantMap, // last host.data
}

#[derive(Default)]
pub struct Timeouts {
    now: i64,
    pending: BTreeMap<String, (i64, QVariant)>, // id -> (due, gen)
}

/// What the script runner needs (main.rs fills it in).
#[derive(Default)]
pub struct Script {
    pub steps: VecDeque<String>,
    pub exit: bool,
    pub grab: Option<String>,
    pub grab_delay: i64,
}

type Job = Box<dyn FnOnce()>;

/// QTimer::singleShot for Rust closures: one QTimer, re-armed for the earliest job.
#[derive(Default)]
struct Scheduler {
    jobs: Vec<(Instant, u64, Job)>,
    seq: u64,
}

pub struct Host {
    machine: *mut WatchScxml,
    pub tree: Tree,
    pub tree_root: QVariantMap,
    pub tags: Tags,
    pub watch: RefCell<Watch>,
    pub timeouts: RefCell<Timeouts>,
    advancing: Cell<bool>,
    pub buttons: RefCell<Buttons>,
    pub tracer: RefCell<Tracer>,
    pub beeper: RefCell<(bool, bool)>, // (enabled, continuous)
    pub script: RefCell<Script>,
    pub q_watch: Cell<*mut WatchModel>,
    pub q_tracer: Cell<*mut TransitionTracer>,
    pub q_buttons: Cell<*mut ButtonRouter>,
    pub engine: Cell<*mut QQmlApplicationEngine>,
    clock: Instant,
    scheduler: RefCell<Scheduler>,
    sched_timer: Cell<*mut QTimer>,
    watch_last_real: Cell<Instant>,
    timeouts_last_real: Cell<Instant>,
}

thread_local! {
    static HOST: Cell<Option<&'static Host>> = const { Cell::new(None) };
}

/// The host of this process (the Qt main thread).
pub fn host() -> &'static Host {
    HOST.with(|h| h.get()).expect("host not installed")
}

fn emit<T>(p: &Cell<*mut T>, f: impl FnOnce(Pin<&mut T>)) {
    let ptr = p.get();
    if !ptr.is_null() {
        // SAFETY: the QObjects outlive the event loop (main.rs) and hold no Rust state.
        f(unsafe { Pin::new_unchecked(&mut *ptr) });
    }
}

fn leak_timer(interval_or_single: Option<i32>, on_timeout: fn()) -> *mut QTimer {
    let mut timer = shim::new_qtimer();
    let mut t = timer.pin_mut();
    if interval_or_single.is_none() {
        t.as_mut().set_single_shot(true);
    }
    t.as_mut().on_timeout(move |_| on_timeout()).release();
    if let Some(ms) = interval_or_single {
        t.as_mut().start_ms(ms);
    }
    timer.into_raw() // lives as long as the process; never deleted after QCoreApplication
}

impl Host {
    /// Installs the host for this thread. `machine` must outlive the event loop.
    pub fn install(machine: *mut WatchScxml, tree: Tree, tags: Tags) -> &'static Host {
        let tree_root = tree.root_variant();
        let h: &'static Host = Box::leak(Box::new(Host {
            machine,
            tree,
            tree_root,
            tags,
            watch: Default::default(),
            timeouts: Default::default(),
            advancing: Cell::new(false),
            buttons: Default::default(),
            tracer: Default::default(),
            beeper: Default::default(),
            script: Default::default(),
            q_watch: Cell::new(std::ptr::null_mut()),
            q_tracer: Cell::new(std::ptr::null_mut()),
            q_buttons: Cell::new(std::ptr::null_mut()),
            engine: Cell::new(std::ptr::null_mut()),
            clock: Instant::now(),
            scheduler: Default::default(),
            sched_timer: Cell::new(std::ptr::null_mut()),
            watch_last_real: Cell::new(Instant::now()),
            timeouts_last_real: Cell::new(Instant::now()),
        }));
        HOST.with(|c| c.set(Some(h)));
        h.sched_timer.set(leak_timer(None, || host().run_due_job()));
        h
    }

    fn machine(&self) -> Pin<&mut WatchScxml> {
        // SAFETY: main.rs keeps the machine alive; WatchScxml has no Rust-side state.
        unsafe { Pin::new_unchecked(&mut *self.machine) }
    }

    pub fn machine_ref(&self) -> &WatchScxml {
        unsafe { &*self.machine }
    }

    fn submit(&self, event: &str) {
        self.machine().submit(&qs(event));
    }

    fn submit_data(&self, event: &str, data: &QVariantMap) {
        self.machine().submit_with_data(&qs(event), data);
    }

    // ---- scheduling -------------------------------------------------------

    /// Runs `f` after `ms` on the event loop (QTimer::singleShot).
    pub fn schedule(&self, ms: i64, f: impl FnOnce() + 'static) {
        {
            let mut s = self.scheduler.borrow_mut();
            s.seq += 1;
            let seq = s.seq;
            let due = Instant::now() + std::time::Duration::from_millis(ms.max(0) as u64);
            s.jobs.push((due, seq, Box::new(f)));
        }
        self.rearm();
    }

    fn rearm(&self) {
        let next = self.scheduler.borrow().jobs.iter().map(|j| j.0).min();
        if let Some(due) = next {
            let ms = due.saturating_duration_since(Instant::now()).as_millis() as i32;
            emit(&self.sched_timer, |t| t.start_ms(ms));
        }
    }

    // One job per timer expiry, so Qt delivers its posted events in between,
    // as with separate QTimer::singleShot calls.
    fn run_due_job(&self) {
        let job = {
            let mut s = self.scheduler.borrow_mut();
            let i = (0..s.jobs.len()).min_by_key(|&i| (s.jobs[i].0, s.jobs[i].1));
            i.map(|i| s.jobs.remove(i).2)
        };
        if let Some(job) = job {
            job();
        }
        self.rearm();
    }

    /// Real time drives the watch and the timeouts (not --manual-time).
    pub fn start_running(&self) {
        self.watch_last_real.set(Instant::now());
        self.timeouts_last_real.set(Instant::now());
        leak_timer(Some(20), || host().watch_tick());
        leak_timer(Some(20), || host().timeouts_tick());
    }

    fn watch_tick(&self) {
        let now = Instant::now();
        let real = now.duration_since(self.watch_last_real.replace(now)).as_secs_f64() * 1000.0;
        let whole = {
            let mut w = self.watch.borrow_mut();
            let sim = real * w.speed + w.carry;
            let whole = sim.floor();
            w.carry = sim - whole;
            whole as i64
        };
        if whole > 0 {
            self.advance_watch(whole);
        }
    }

    fn timeouts_tick(&self) {
        let now = Instant::now();
        let last = self.timeouts_last_real.get();
        let ms = now.duration_since(last).as_millis() as i64;
        // keep the sub-millisecond rest for the next tick
        self.timeouts_last_real.set(last + std::time::Duration::from_millis(ms as u64));
        self.advance_timeouts(ms);
    }

    // ---- outgoing events of the machine (§2) ------------------------------

    pub fn on_machine_event(&self, name: &str, data: &QVariantMap) {
        // TransitionTracer, §4.2: "◀ name key=value…" in key order.
        if ["model.", "sw.", "beep."].iter().any(|p| name.starts_with(p)) {
            let mut line = format!("◀ {name}");
            for (k, v) in data.iter() {
                line += &format!(" {}={}", k, variant_string(v));
            }
            self.append(line);
        }
        let field = |key: &str| data.get(&qs(key)).unwrap_or_default();
        match name {
            "host.data" => {
                self.tracer.borrow_mut().data = data.clone();
                self.on_host_data(data);
            }
            "model.reset" => self.reset(),
            "model.increment" => self.increment(&variant_string(&field("field"))),
            "sw.start" => {
                if self.watch.borrow_mut().sw_start() {
                    emit(&self.q_watch, |w| w.stopwatch_changed());
                }
            }
            "sw.stop" => {
                if self.watch.borrow_mut().sw_stop() {
                    emit(&self.q_watch, |w| w.stopwatch_changed());
                }
            }
            "sw.lap" => {
                self.watch.borrow_mut().sw_lap();
                emit(&self.q_watch, |w| w.stopwatch_changed());
            }
            "sw.reset" => {
                self.watch.borrow_mut().sw_reset();
                emit(&self.q_watch, |w| w.stopwatch_changed());
            }
            // WatchModel.shortBeep; QML (Lcd.qml) and the Beeper stub listen.
            "beep.short" => emit(&self.q_watch, |w| w.short_beep()),
            "timer.start" => {
                let id = variant_string(&field("id"));
                let secs = field("secs").value_or_default::<f64>();
                let mut t = self.timeouts.borrow_mut();
                let due = t.now + (secs * 1000.0).round() as i64;
                t.pending.insert(id, (due, field("gen")));
            }
            "timer.cancel" => {
                self.timeouts.borrow_mut().pending.remove(&variant_string(&field("id")));
            }
            _ => {}
        }
    }

    // ---- WatchModel -------------------------------------------------------

    pub fn push_alarms(&self) {
        // Host boundary (D-24); queued like every input.
        if !self.machine_ref().is_initialized() {
            return;
        }
        let (t1, t2) = {
            let w = self.watch.borrow();
            (w.t1, w.t2)
        };
        self.submit_data("host.alarms", &qmap([("T1", QVariant::from(&t1)), ("T2", QVariant::from(&t2))]));
    }

    fn reset(&self) {
        let now = QDateTime::current_date_time();
        let (d, t) = (now.date(), now.time());
        let current = (d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second());
        self.watch.borrow_mut().reset(current);
        self.push_alarms();
        emit(&self.q_watch, |mut w| {
            w.as_mut().settings_changed();
            w.as_mut().stopwatch_changed();
            w.as_mut().time_changed();
        });
    }

    fn increment(&self, field: &str) {
        use crate::watch::Changed;
        let changed = self.watch.borrow_mut().increment(field);
        match changed {
            Changed::Time => emit(&self.q_watch, |w| w.time_changed()),
            Changed::Settings => emit(&self.q_watch, |w| w.settings_changed()),
            Changed::Alarms => {
                self.push_alarms();
                emit(&self.q_watch, |w| w.settings_changed());
            }
            Changed::Nothing => {}
        }
    }

    /// Moves T forward; T.hits.T1, T.hits.T2, T.hits.hour per minute crossed.
    pub fn advance_watch(&self, ms: i64) {
        let (hits, shown_changed) = self.watch.borrow_mut().advance(ms);
        for event in hits {
            self.submit(event);
        }
        if shown_changed {
            emit(&self.q_watch, |w| w.time_changed());
        }
        if self.watch.borrow().sw_running {
            emit(&self.q_watch, |w| w.stopwatch_changed());
        }
    }

    pub fn set_time(&self, h: i32, m: i32, s: i32) {
        self.watch.borrow_mut().set_time(h, m, s);
        emit(&self.q_watch, |w| w.time_changed());
    }

    pub fn set_date(&self, y: i32, mo: i32, d: i32) {
        self.watch.borrow_mut().set_date(y, mo, d);
        emit(&self.q_watch, |w| w.time_changed());
    }

    pub fn set_alarm(&self, which: i32, minutes: i32) {
        self.watch.borrow_mut().set_alarm(which, minutes);
        self.push_alarms();
        emit(&self.q_watch, |w| w.settings_changed());
    }

    pub fn jump_before_alarm(&self, which: i32, seconds: i32) {
        self.watch.borrow_mut().jump_before_alarm(which, seconds);
        emit(&self.q_watch, |w| w.time_changed());
    }

    pub fn set_speed(&self, speed: f64) {
        let s = speed.clamp(0.0, 3600.0);
        {
            let mut w = self.watch.borrow_mut();
            // qFuzzyCompare
            if (s - w.speed).abs() * 1e12 <= s.abs().min(w.speed.abs()) {
                return;
            }
            w.speed = s;
        }
        emit(&self.q_watch, |w| w.speed_changed());
    }

    // ---- TimeoutService (§5.2) --------------------------------------------

    pub fn advance_timeouts(&self, ms: i64) {
        if ms < 0 || self.advancing.replace(true) {
            return;
        }
        // Timers just requested (e.g. by a button) start at the current time.
        shim::drain_machine();
        let end = self.timeouts.borrow().now + ms;
        loop {
            let next = {
                let mut t = self.timeouts.borrow_mut();
                let id = t
                    .pending
                    .iter()
                    .filter(|(_, (due, _))| *due <= end)
                    .min_by_key(|(id, (due, _))| (*due, (*id).clone()))
                    .map(|(id, _)| id.clone());
                id.map(|id| {
                    let (due, gen) = t.pending.remove(&id).unwrap();
                    t.now = due;
                    (id, gen)
                })
            };
            let Some((id, gen)) = next else { break };
            self.submit_data(&format!("timeout.{id}"), &qmap([("gen", gen)]));
            // Handled at its own deadline: a timer it starts counts from there (D-21).
            shim::drain_machine();
        }
        self.timeouts.borrow_mut().now = end;
        self.advancing.set(false);
    }

    // ---- ButtonRouter -----------------------------------------------------

    pub fn press(&self, button: &str) {
        let b = button.to_lowercase();
        if !matches!(b.as_str(), "a" | "b" | "c" | "d") || !self.buttons.borrow_mut().held.insert(b.clone()) {
            return;
        }
        emit(&self.q_buttons, |r| r.held_changed());
        self.submit_input(&format!("{b}.down"));
    }

    pub fn release(&self, button: &str) {
        let b = button.to_lowercase();
        if !self.buttons.borrow_mut().held.remove(&b) {
            return;
        }
        emit(&self.q_buttons, |r| r.held_changed());
        self.submit_input(&format!("{b}.up"));
    }

    pub fn release_all(&self) {
        let held = self.buttons.borrow().held.clone();
        for b in held {
            self.release(&b);
        }
    }

    /// ButtonRouter.submit: logs "▶ event" and submits it.
    pub fn submit_input(&self, event: &str) {
        self.note_input(event);
        self.submit(event);
    }

    pub fn set_data_value(&self, name: &str, value: &QVariant) {
        // The statechart answers with host.data, which updates dataValue().
        if name == "harelAnomalies" {
            let v = value.value_or_default::<bool>();
            self.submit_data("host.anomalies", &qmap([("value", QVariant::from(&v))]));
        } else {
            eprintln!("ButtonRouter: '{name}' cannot be set (DESIGN-NOTES D-24)");
        }
    }

    fn on_host_data(&self, data: &QVariantMap) {
        let changed: Vec<String> = {
            let mut b = self.buttons.borrow_mut();
            let before = std::mem::replace(&mut b.data, data.clone());
            if before.is_empty() {
                vec![String::new()] // the first host.data: everything is new
            } else {
                data.iter()
                    .filter(|(k, v)| before.get_or_default(k) != **v)
                    .map(|(k, _)| k.to_string())
                    .collect()
            }
        };
        for name in changed {
            emit(&self.q_buttons, |r| r.data_value_changed(&qs(&name)));
        }
    }

    // ---- TransitionTracer (§4) --------------------------------------------

    pub fn note_input(&self, event: &str) {
        self.append(format!("▶ {event}"));
    }

    fn append(&self, line: String) {
        {
            let mut t = self.tracer.borrow_mut();
            t.entries.push_back(line);
            while t.entries.len() > MAX_ENTRIES {
                t.entries.pop_front();
            }
        }
        emit(&self.q_tracer, |t| t.entries_changed());
    }

    pub fn clear_log(&self) {
        self.tracer.borrow_mut().entries.clear();
        emit(&self.q_tracer, |t| t.entries_changed());
    }

    pub fn clear_flash(&self) {
        self.tracer.borrow_mut().flash_map.clear();
        emit(&self.q_tracer, |t| t.flash_changed());
    }

    fn stamp(&self) -> QVariant {
        QVariant::from(&(self.clock.elapsed().as_millis() as i64))
    }

    /// §4.3: one log line per fired T-xx, attributed from the parsed tree.
    pub fn on_log(&self, label: &str, msg: &str) {
        let is_row = label.len() > 2 && label.starts_with("T-") && label[2..].bytes().all(|c| c.is_ascii_digit());
        if !is_row {
            return;
        }
        let (source, targets) = {
            let t = self.tracer.borrow();
            match self.attribute(&t, label) {
                Some(tr) => (tr.source.clone(), tr.targets.clone()),
                None => (String::new(), vec![]),
            }
        };
        {
            let mut t = self.tracer.borrow_mut();
            t.fired.insert(label.to_string());
            t.last_spec_id = label.to_string();
            let stamp = self.stamp();
            if !source.is_empty() {
                t.flash_map.insert(qs(&source), stamp.clone());
            }
            for x in &targets {
                t.flash_map.insert(qs(x), stamp.clone());
            }
        }
        emit(&self.q_tracer, |t| t.flash_changed());
        let to = if targets.is_empty() { "(no state change)".to_string() } else { targets.join(", ") };
        let msg = if msg.is_empty() { "·" } else { msg };
        self.append(format!("{label} {msg}: {source} → {to}"));
    }

    // Qt delivers the log signal after the macrostep (D-18): the source was active
    // in one of the last stable configurations, the targets are active now.
    fn attribute<'t>(&'t self, t: &Tracer, label: &str) -> Option<&'t Transition> {
        let was_active = |id: &str| t.recent.iter().any(|c| c.contains(id));
        let landed = |tr: &Transition| {
            tr.targets.iter().all(|tg| {
                let n = self.tree.nodes.get(tg);
                let where_ = match n {
                    Some(n) if n.kind == "history" => &n.parent,
                    _ => tg,
                };
                t.active.contains(where_)
            })
        };
        let rows = || self.tree.transitions.iter().filter(|tr| tr.spec_id == label && was_active(&tr.source));
        let mut cands: Vec<&Transition> = rows().filter(|tr| landed(tr)).collect();
        if cands.is_empty() {
            cands = rows().collect();
        }
        if cands.len() > 1 {
            // The clh(main*) pairs "[hv_x] -> x_H" / "-> x": hv_x (from host.data) tells
            // which branch fired; it only changes when x is exited.
            let ident = |c: &str| {
                let mut ch = c.chars();
                matches!(ch.next(), Some(f) if f.is_ascii_alphabetic() || f == '_')
                    && ch.all(|c| c.is_ascii_alphanumeric() || c == '_')
            };
            let taken = cands.iter().rev().find(|tr| {
                !tr.cond.is_empty() && ident(&tr.cond) && t.data.get_or_default(&qs(&tr.cond)).value_or_default::<bool>()
            });
            let uncond = cands.iter().rev().find(|tr| tr.cond.is_empty());
            return taken.or(uncond).or(cands.first()).copied();
        }
        cands.first().copied()
    }

    pub fn on_stable(&self) {
        let names: Vec<String> =
            cxx_qt_lib::QList::<cxx_qt_lib::QString>::from(&self.machine_ref().active_state_names())
                .iter()
                .map(|s| s.to_string())
                .collect();
        let flash = {
            let mut t = self.tracer.borrow_mut();
            let now: HashSet<String> = names.iter().cloned().collect();
            // States newly entered in this macrostep flash as well.
            let entered: Vec<String> = now.iter().filter(|s| !t.prev_active.contains(*s)).cloned().collect();
            t.pending_flash.extend(entered);
            t.active = now.clone();
            t.active_map = names.iter().map(|s| (qs(s), QVariant::from(&true))).collect();
            t.active_list = names;
            t.prev_active = now.clone();
            // Only distinct configurations: a macrostep that changes no state
            // (host.data, timer.*, model.*) would push out the one a late log
            // line needs (HOST-CONTRACT §4.3, DESIGN-NOTES D-26).
            if t.recent.front() != Some(&now) {
                t.recent.push_front(now);
            }
            t.recent.truncate(4);
            !t.pending_flash.is_empty()
        };
        emit(&self.q_tracer, |t| t.active_changed());
        if flash {
            {
                let mut t = self.tracer.borrow_mut();
                t.flash_map.clear();
                let stamp = self.stamp();
                let pending: Vec<String> = t.pending_flash.drain().collect();
                for s in pending {
                    t.flash_map.insert(qs(&s), stamp.clone());
                }
            }
            emit(&self.q_tracer, |t| t.flash_changed());
        }
    }
}
