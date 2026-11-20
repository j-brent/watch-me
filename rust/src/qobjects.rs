//! The host objects Main.qml needs (docs/HOST-CONTRACT.md §3), as QObjects
//! defined in Rust. They are facades: their state lives in `crate::host::Host`
//! (one RefCell per component), so a call that re-enters the host while the
//! machine runs (a signal handler, QML reading a property during a signal)
//! never aliases a `&mut` held by the caller.

#[cxx_qt::bridge]
pub mod qobj {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
        include!("cxx-qt-lib/qstringlist.h");
        type QStringList = cxx_qt_lib::QStringList;
        include!("cxx-qt-lib/qvariant.h");
        type QVariant = cxx_qt_lib::QVariant;
        include!("cxx-qt-lib/qmap.h");
        type QMap_QString_QVariant = cxx_qt_lib::QMap<cxx_qt_lib::QMapPair_QString_QVariant>;
        include!("cxx-qt-lib/qlist.h");
        type QList_QVariant = cxx_qt_lib::QList<cxx_qt_lib::QVariant>;
    }

    // ---- WatchModel -------------------------------------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(i32, hour, READ = get_hour, NOTIFY = time_changed)]
        #[qproperty(i32, minute, READ = get_minute, NOTIFY = time_changed)]
        #[qproperty(i32, second, READ = get_second, NOTIFY = time_changed)]
        #[qproperty(i32, year, READ = get_year, NOTIFY = time_changed)]
        #[qproperty(i32, month, READ = get_month, NOTIFY = time_changed)]
        #[qproperty(i32, day, READ = get_day, NOTIFY = time_changed)]
        #[qproperty(i32, weekday, READ = get_weekday, NOTIFY = time_changed)]
        #[qproperty(bool, is24h, READ = get_is24h, NOTIFY = settings_changed)]
        #[qproperty(i32, t1, READ = get_t1, NOTIFY = settings_changed)]
        #[qproperty(i32, t2, READ = get_t2, NOTIFY = settings_changed)]
        #[qproperty(i64, stopwatch_centis, cxx_name = "stopwatchCentis", READ = get_stopwatch_centis, NOTIFY = stopwatch_changed)]
        #[qproperty(i64, lap_centis, cxx_name = "lapCentis", READ = get_lap_centis, NOTIFY = stopwatch_changed)]
        #[qproperty(bool, stopwatch_running, cxx_name = "stopwatchRunning", READ = get_stopwatch_running, NOTIFY = stopwatch_changed)]
        #[qproperty(f64, speed, READ = get_speed, WRITE = set_speed, NOTIFY = speed_changed)]
        type WatchModel = super::WatchModelRust;

        fn get_hour(self: &WatchModel) -> i32;
        fn get_minute(self: &WatchModel) -> i32;
        fn get_second(self: &WatchModel) -> i32;
        fn get_year(self: &WatchModel) -> i32;
        fn get_month(self: &WatchModel) -> i32;
        fn get_day(self: &WatchModel) -> i32;
        fn get_weekday(self: &WatchModel) -> i32;
        fn get_is24h(self: &WatchModel) -> bool;
        fn get_t1(self: &WatchModel) -> i32;
        fn get_t2(self: &WatchModel) -> i32;
        fn get_stopwatch_centis(self: &WatchModel) -> i64;
        fn get_lap_centis(self: &WatchModel) -> i64;
        fn get_stopwatch_running(self: &WatchModel) -> bool;
        fn get_speed(self: &WatchModel) -> f64;
        fn set_speed(self: Pin<&mut WatchModel>, speed: f64);

        #[qinvokable]
        #[cxx_name = "setTime"]
        fn set_time(self: &WatchModel, h: i32, m: i32, s: i32);
        #[qinvokable]
        #[cxx_name = "setDate"]
        fn set_date(self: &WatchModel, y: i32, mo: i32, d: i32);
        #[qinvokable]
        #[cxx_name = "setAlarm"]
        fn set_alarm(self: &WatchModel, which: i32, minutes: i32);
        #[qinvokable]
        #[cxx_name = "jumpBeforeAlarm"]
        fn jump_before_alarm(self: &WatchModel, which: i32, seconds: i32);

        #[qsignal]
        #[cxx_name = "timeChanged"]
        fn time_changed(self: Pin<&mut WatchModel>);
        #[qsignal]
        #[cxx_name = "settingsChanged"]
        fn settings_changed(self: Pin<&mut WatchModel>);
        #[qsignal]
        #[cxx_name = "stopwatchChanged"]
        fn stopwatch_changed(self: Pin<&mut WatchModel>);
        #[qsignal]
        #[cxx_name = "speedChanged"]
        fn speed_changed(self: Pin<&mut WatchModel>);
        #[qsignal]
        #[cxx_name = "shortBeep"]
        fn short_beep(self: Pin<&mut WatchModel>);
    }

    // ---- TransitionTracer -------------------------------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(QStringList, active_states, cxx_name = "activeStates", READ = get_active_states, NOTIFY = active_changed)]
        #[qproperty(QMap_QString_QVariant, active_set, cxx_name = "activeSet", READ = get_active_set, NOTIFY = active_changed)]
        #[qproperty(QMap_QString_QVariant, flash_set, cxx_name = "flashSet", READ = get_flash_set, NOTIFY = flash_changed)]
        #[qproperty(QStringList, entries, READ = get_entries, NOTIFY = entries_changed)]
        #[qproperty(QStringList, fired_spec_ids, cxx_name = "firedSpecIds", READ = get_fired_spec_ids, NOTIFY = entries_changed)]
        #[qproperty(QString, last_spec_id, cxx_name = "lastSpecId", READ = get_last_spec_id, NOTIFY = entries_changed)]
        type TransitionTracer = super::TransitionTracerRust;

        fn get_active_states(self: &TransitionTracer) -> QStringList;
        fn get_active_set(self: &TransitionTracer) -> QMap_QString_QVariant;
        fn get_flash_set(self: &TransitionTracer) -> QMap_QString_QVariant;
        fn get_entries(self: &TransitionTracer) -> QStringList;
        fn get_fired_spec_ids(self: &TransitionTracer) -> QStringList;
        fn get_last_spec_id(self: &TransitionTracer) -> QString;

        #[qinvokable]
        #[cxx_name = "isActive"]
        fn is_active(self: &TransitionTracer, id: &QString) -> bool;
        #[qinvokable]
        #[cxx_name = "noteInput"]
        fn note_input(self: &TransitionTracer, event: &QString);
        #[qinvokable]
        #[cxx_name = "clearLog"]
        fn clear_log(self: &TransitionTracer);
        #[qinvokable]
        #[cxx_name = "logText"]
        fn log_text(self: &TransitionTracer) -> QString;
        #[qinvokable]
        #[cxx_name = "exportLog"]
        fn export_log(self: &TransitionTracer, path: &QString) -> bool;
        #[qinvokable]
        #[cxx_name = "clearFlash"]
        fn clear_flash(self: &TransitionTracer);

        #[qsignal]
        #[cxx_name = "activeChanged"]
        fn active_changed(self: Pin<&mut TransitionTracer>);
        #[qsignal]
        #[cxx_name = "flashChanged"]
        fn flash_changed(self: Pin<&mut TransitionTracer>);
        #[qsignal]
        #[cxx_name = "entriesChanged"]
        fn entries_changed(self: Pin<&mut TransitionTracer>);
    }

    // ---- ButtonRouter -----------------------------------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(QMap_QString_QVariant, held, READ = get_held, NOTIFY = held_changed)]
        type ButtonRouter = super::ButtonRouterRust;

        fn get_held(self: &ButtonRouter) -> QMap_QString_QVariant;
        #[qinvokable]
        fn press(self: &ButtonRouter, button: &QString);
        #[qinvokable]
        fn release(self: &ButtonRouter, button: &QString);
        #[qinvokable]
        #[cxx_name = "releaseAll"]
        fn release_all(self: &ButtonRouter);
        #[qinvokable]
        fn submit(self: &ButtonRouter, event: &QString);
        #[qinvokable]
        #[cxx_name = "setDataValue"]
        fn set_data_value(self: &ButtonRouter, name: &QString, value: &QVariant);
        #[qinvokable]
        #[cxx_name = "dataValue"]
        fn data_value(self: &ButtonRouter, name: &QString) -> QVariant;

        #[qsignal]
        #[cxx_name = "heldChanged"]
        fn held_changed(self: Pin<&mut ButtonRouter>);
        #[qsignal]
        #[cxx_name = "dataValueChanged"]
        fn data_value_changed(self: Pin<&mut ButtonRouter>, name: &QString);
    }

    // ---- ScxmlTreeModel ---------------------------------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(QMap_QString_QVariant, root, READ = get_root, CONSTANT)]
        #[qproperty(QStringList, state_ids, cxx_name = "stateIds", READ = get_state_ids, CONSTANT)]
        #[qproperty(QString, error_string, cxx_name = "errorString", READ = get_error_string, CONSTANT)]
        type ScxmlTreeModel = super::ScxmlTreeModelRust;

        fn get_root(self: &ScxmlTreeModel) -> QMap_QString_QVariant;
        fn get_state_ids(self: &ScxmlTreeModel) -> QStringList;
        fn get_error_string(self: &ScxmlTreeModel) -> QString;
        #[qinvokable]
        fn node(self: &ScxmlTreeModel, id: &QString) -> QMap_QString_QVariant;
        #[qinvokable]
        #[cxx_name = "transitionsFrom"]
        fn transitions_from(self: &ScxmlTreeModel, id: &QString) -> QList_QVariant;
        #[qinvokable]
        #[cxx_name = "transitionsInto"]
        fn transitions_into(self: &ScxmlTreeModel, id: &QString) -> QList_QVariant;
        #[qinvokable]
        fn ancestors(self: &ScxmlTreeModel, id: &QString) -> QStringList;
        #[qinvokable]
        #[cxx_name = "specIds"]
        fn spec_ids(self: &ScxmlTreeModel) -> QStringList;
    }

    // ---- SpecTags ---------------------------------------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(QMap_QString_QVariant, tags, READ = get_tags, CONSTANT)]
        type SpecTags = super::SpecTagsRust;

        fn get_tags(self: &SpecTags) -> QMap_QString_QVariant;
        #[qinvokable]
        #[cxx_name = "tagFor"]
        fn tag_for(self: &SpecTags, spec_id: &QString) -> QString;
        #[qinvokable]
        fn weakest(self: &SpecTags, spec_ids: &QStringList) -> QString;
    }

    // ---- Beeper (stub: no audio in this host) -----------------------------
    extern "RustQt" {
        #[qobject]
        #[qproperty(bool, enabled, READ = get_enabled, WRITE = set_enabled, NOTIFY = enabled_changed)]
        #[qproperty(bool, continuous, READ = get_continuous, WRITE = set_continuous, NOTIFY = continuous_changed)]
        #[qproperty(bool, available, READ = get_available, CONSTANT)]
        type Beeper = super::BeeperRust;

        fn get_enabled(self: &Beeper) -> bool;
        fn set_enabled(self: Pin<&mut Beeper>, enabled: bool);
        fn get_continuous(self: &Beeper) -> bool;
        fn set_continuous(self: Pin<&mut Beeper>, continuous: bool);
        fn get_available(self: &Beeper) -> bool;
        #[qinvokable]
        #[cxx_name = "shortBeep"]
        fn short_beep(self: &Beeper);

        #[qsignal]
        #[cxx_name = "enabledChanged"]
        fn enabled_changed(self: Pin<&mut Beeper>);
        #[qsignal]
        #[cxx_name = "continuousChanged"]
        fn continuous_changed(self: Pin<&mut Beeper>);
    }

    // Instantiated from Rust (main.rs), not from QML.
    #[namespace = "rust::cxxqtlib1"]
    unsafe extern "C++" {
        include!("cxx-qt-lib/common.h");
        #[cxx_name = "make_unique"]
        fn new_watch_model() -> UniquePtr<WatchModel>;
        #[cxx_name = "make_unique"]
        fn new_tracer() -> UniquePtr<TransitionTracer>;
        #[cxx_name = "make_unique"]
        fn new_button_router() -> UniquePtr<ButtonRouter>;
        #[cxx_name = "make_unique"]
        fn new_tree_model() -> UniquePtr<ScxmlTreeModel>;
        #[cxx_name = "make_unique"]
        fn new_spec_tags() -> UniquePtr<SpecTags>;
        #[cxx_name = "make_unique"]
        fn new_beeper() -> UniquePtr<Beeper>;
    }
}

use crate::host::host;
use crate::qt::{qs, to_qstringlist};
use cxx_qt_lib::{QList, QString, QStringList, QVariant};
use qobj::*;
use std::pin::Pin;

// The Rust side of every facade: empty, the state is in `Host`.
#[derive(Default)]
pub struct WatchModelRust;
#[derive(Default)]
pub struct TransitionTracerRust;
#[derive(Default)]
pub struct ButtonRouterRust;
#[derive(Default)]
pub struct ScxmlTreeModelRust;
#[derive(Default)]
pub struct SpecTagsRust;
#[derive(Default)]
pub struct BeeperRust;

impl WatchModel {
    fn get_hour(&self) -> i32 { host().watch.borrow().hour() }
    fn get_minute(&self) -> i32 { host().watch.borrow().minute() }
    fn get_second(&self) -> i32 { host().watch.borrow().second() }
    fn get_year(&self) -> i32 { host().watch.borrow().year() }
    fn get_month(&self) -> i32 { host().watch.borrow().month() }
    fn get_day(&self) -> i32 { host().watch.borrow().day() }
    fn get_weekday(&self) -> i32 { host().watch.borrow().weekday() }
    fn get_is24h(&self) -> bool { host().watch.borrow().is24h }
    fn get_t1(&self) -> i32 { host().watch.borrow().t1 }
    fn get_t2(&self) -> i32 { host().watch.borrow().t2 }
    fn get_stopwatch_centis(&self) -> i64 { host().watch.borrow().stopwatch_centis() }
    fn get_lap_centis(&self) -> i64 { host().watch.borrow().lap }
    fn get_stopwatch_running(&self) -> bool { host().watch.borrow().sw_running }
    fn get_speed(&self) -> f64 { host().watch.borrow().speed }
    fn set_speed(self: Pin<&mut Self>, speed: f64) { host().set_speed(speed) }
    fn set_time(&self, h: i32, m: i32, s: i32) { host().set_time(h, m, s) }
    fn set_date(&self, y: i32, mo: i32, d: i32) { host().set_date(y, mo, d) }
    fn set_alarm(&self, which: i32, minutes: i32) { host().set_alarm(which, minutes) }
    fn jump_before_alarm(&self, which: i32, seconds: i32) { host().jump_before_alarm(which, seconds) }
}

impl TransitionTracer {
    fn get_active_states(&self) -> QStringList { to_qstringlist(&host().tracer.borrow().active_list) }
    fn get_active_set(&self) -> QMap_QString_QVariant { host().tracer.borrow().active_map.clone() }
    fn get_flash_set(&self) -> QMap_QString_QVariant { host().tracer.borrow().flash_map.clone() }
    fn get_entries(&self) -> QStringList { to_qstringlist(host().tracer.borrow().entries.iter()) }
    fn get_fired_spec_ids(&self) -> QStringList { to_qstringlist(&host().tracer.borrow().fired_sorted()) }
    fn get_last_spec_id(&self) -> QString { qs(&host().tracer.borrow().last_spec_id) }
    fn is_active(&self, id: &QString) -> bool { host().tracer.borrow().active.contains(&id.to_string()) }
    fn note_input(&self, event: &QString) { host().note_input(&event.to_string()) }
    fn clear_log(&self) { host().clear_log() }
    fn log_text(&self) -> QString { qs(&host().tracer.borrow().log_text()) }
    fn export_log(&self, path: &QString) -> bool { host().tracer.borrow().export_log(&path.to_string()) }
    fn clear_flash(&self) { host().clear_flash() }
}

impl ButtonRouter {
    fn get_held(&self) -> QMap_QString_QVariant {
        host().buttons.borrow().held.iter().map(|b| (qs(b), QVariant::from(&true))).collect()
    }
    fn press(&self, button: &QString) { host().press(&button.to_string()) }
    fn release(&self, button: &QString) { host().release(&button.to_string()) }
    fn release_all(&self) { host().release_all() }
    fn submit(&self, event: &QString) { host().submit_input(&event.to_string()) }
    fn set_data_value(&self, name: &QString, value: &QVariant) { host().set_data_value(&name.to_string(), value) }
    fn data_value(&self, name: &QString) -> QVariant { host().buttons.borrow().data.get_or_default(name) }
}

impl ScxmlTreeModel {
    fn get_root(&self) -> QMap_QString_QVariant { host().tree_root.clone() }
    fn get_state_ids(&self) -> QStringList { to_qstringlist(&host().tree.order) }
    fn get_error_string(&self) -> QString { qs(&host().tree.error) }
    fn node(&self, id: &QString) -> QMap_QString_QVariant { host().tree.node_variant(&id.to_string()) }
    fn transitions_from(&self, id: &QString) -> QList_QVariant { host().tree.transitions_from(&id.to_string()) }
    fn transitions_into(&self, id: &QString) -> QList_QVariant { host().tree.transitions_into(&id.to_string()) }
    fn ancestors(&self, id: &QString) -> QStringList { to_qstringlist(&host().tree.ancestors(&id.to_string())) }
    fn spec_ids(&self) -> QStringList { to_qstringlist(&host().tree.spec_ids()) }
}

impl SpecTags {
    fn get_tags(&self) -> QMap_QString_QVariant {
        host().tags.tags.iter().map(|(k, v)| (qs(k), QVariant::from(&qs(v)))).collect()
    }
    fn tag_for(&self, spec_id: &QString) -> QString { qs(host().tags.tag_for(&spec_id.to_string())) }
    fn weakest(&self, spec_ids: &QStringList) -> QString {
        let ids: Vec<String> = QList::<QString>::from(spec_ids).iter().map(|s| s.to_string()).collect();
        qs(host().tags.weakest(&ids))
    }
}

impl Beeper {
    fn get_enabled(&self) -> bool { host().beeper.borrow().0 }
    fn set_enabled(self: Pin<&mut Self>, enabled: bool) {
        let changed = std::mem::replace(&mut host().beeper.borrow_mut().0, enabled) != enabled;
        if changed {
            self.enabled_changed();
        }
    }
    fn get_continuous(&self) -> bool { host().beeper.borrow().1 }
    fn set_continuous(self: Pin<&mut Self>, continuous: bool) {
        let changed = std::mem::replace(&mut host().beeper.borrow_mut().1, continuous) != continuous;
        if changed {
            self.continuous_changed();
        }
    }
    /// This host plays no sound (docs/HOST-CONTRACT.md §3: available = false).
    fn get_available(&self) -> bool { false }
    fn short_beep(&self) {}
}
