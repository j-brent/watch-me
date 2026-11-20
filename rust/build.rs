// Builds the cxx-qt bridges and the C++ shim (cpp/shim.*) with cargo alone.
// Qt is found through qmake: set QMAKE=/path/to/qmake6 (rust/README.md).
use cxx_qt_build::CxxQtBuilder;

fn main() {
    CxxQtBuilder::new()
        .qt_module("Gui")
        .qt_module("Qml")
        .qt_module("Quick")
        .qt_module("Scxml")
        .files(["src/ffi.rs", "src/qobjects.rs"])
        .cpp_file("cpp/shim.h")
        .cpp_file("cpp/shim.cpp")
        .build();

    // Run from target/ without LD_LIBRARY_PATH: an RPATH to Qt's libraries
    // (cxx-qt-build only adds one for macOS frameworks).
    println!("cargo::rerun-if-env-changed=QMAKE");
    let qmake = std::env::var("QMAKE").unwrap_or_else(|_| "qmake6".into());
    if let Ok(out) = std::process::Command::new(qmake).args(["-query", "QT_INSTALL_LIBS"]).output() {
        let libs = String::from_utf8_lossy(&out.stdout).trim().to_string();
        if out.status.success() && !libs.is_empty() && cfg!(target_os = "linux") {
            println!("cargo::rustc-link-arg=-Wl,-rpath,{libs}");
        }
    }
}
