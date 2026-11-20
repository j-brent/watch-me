//! What Rust uses from C++: the state machine shim (cpp/shim.h) and QTimer.
//! cxx-qt-lib already wraps QString, QVariant, QVariantMap, the applications and
//! the QML engine; it wraps neither Qt SCXML nor QTimer nor QQuickWindow.

#[cxx_qt::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
        include!("cxx-qt-lib/qstringlist.h");
        type QStringList = cxx_qt_lib::QStringList;
        include!("cxx-qt-lib/qmap.h");
        type QMap_QString_QVariant = cxx_qt_lib::QMap<cxx_qt_lib::QMapPair_QString_QVariant>;
        include!("cxx-qt-lib/qqmlapplicationengine.h");
        type QQmlApplicationEngine = cxx_qt_lib::QQmlApplicationEngine;
    }

    unsafe extern "C++Qt" {
        include!("watch-me-rs/cpp/shim.h");
        /// QScxmlStateMachine loaded from watch.scxml, seen through signals.
        #[qobject]
        type WatchScxml;

        fn load(self: Pin<&mut WatchScxml>, path: &QString) -> QString;
        #[cxx_name = "hasDataModel"]
        fn has_data_model(self: &WatchScxml) -> bool;
        fn start(self: Pin<&mut WatchScxml>);
        fn observe(self: Pin<&mut WatchScxml>, spec: &QString);
        fn submit(self: Pin<&mut WatchScxml>, event: &QString);
        #[cxx_name = "submitWithData"]
        fn submit_with_data(self: Pin<&mut WatchScxml>, event: &QString, data: &QMap_QString_QVariant);
        #[cxx_name = "activeStateNames"]
        fn active_state_names(self: &WatchScxml) -> QStringList;
        #[cxx_name = "isInitialized"]
        fn is_initialized(self: &WatchScxml) -> bool;

        #[qsignal]
        #[cxx_name = "machineEvent"]
        fn machine_event(self: Pin<&mut WatchScxml>, name: &QString, data: &QMap_QString_QVariant);
        #[qsignal]
        fn logged(self: Pin<&mut WatchScxml>, label: &QString, msg: &QString);
        #[qsignal]
        fn stable(self: Pin<&mut WatchScxml>);
        #[qsignal]
        fn initialized(self: Pin<&mut WatchScxml>);
    }

    unsafe extern "C++Qt" {
        include!(<QtCore/QTimer>);
        #[qobject]
        type QTimer;

        #[rust_name = "start_ms"]
        fn start(self: Pin<&mut QTimer>, msec: i32);
        #[cxx_name = "setSingleShot"]
        fn set_single_shot(self: Pin<&mut QTimer>, single: bool);
        #[qsignal]
        pub(self) fn timeout(self: Pin<&mut QTimer>); // private signal (QPrivateSignal)
    }

    unsafe extern "C++" {
        include!("watch-me-rs/cpp/shim.h");
        #[cxx_name = "newWatchScxml"]
        fn new_watch_scxml() -> UniquePtr<WatchScxml>;
        #[cxx_name = "drainMachine"]
        fn drain_machine();
        #[cxx_name = "quitApp"]
        fn quit_app(code: i32);
        #[cxx_name = "grabWindow"]
        fn grab_window(engine: &QQmlApplicationEngine, file: &QString) -> bool;
        #[cxx_name = "resizeWindow"]
        fn resize_window(engine: &QQmlApplicationEngine, w: i32, h: i32);
        #[cxx_name = "setChildProperty"]
        fn set_child_property(
            engine: &QQmlApplicationEngine,
            object_name: &QString,
            property: &QString,
            value: &QString,
        ) -> bool;
    }

    #[namespace = "rust::cxxqtlib1"]
    unsafe extern "C++" {
        include!("cxx-qt-lib/common.h");
        #[cxx_name = "make_unique"]
        fn new_qtimer() -> UniquePtr<QTimer>;
    }
}
