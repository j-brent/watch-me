// File I/O for a host without compiled code: QML's XMLHttpRequest on file: URLs.
// Qt refuses both unless the environment allows them (the launcher sets these):
//   QML_XHR_ALLOW_FILE_READ=1   GET  file:...   https://doc.qt.io/qt-6/qml-qtqml-xmlhttprequest.html
//   QML_XHR_ALLOW_FILE_WRITE=1  PUT  file:...

// A local path (absolute, relative to `cwd`, "file://..." or Windows "C:\...") -> file: URL.
export function toUrl(path, cwd) {
    let p = String(path);
    if (p.startsWith("file:"))
        return p;
    p = p.replace(/\\/g, "/");
    const absolute = p.startsWith("/") || /^[A-Za-z]:\//.test(p);
    if (!absolute && cwd)
        p = cwd.replace(/\\/g, "/").replace(/\/$/, "") + "/" + p;
    if (/^[A-Za-z]:\//.test(p))
        p = "/" + p;
    // keep '/' and ':' (drive letters); escape what a URL parser would treat specially
    return "file://" + p.split("/").map(seg => encodeURIComponent(seg).replace(/%3A/g, ":")).join("/");
}

// Synchronous read (Qt's XHR supports synchronous GET). Returns the text, or null.
export function readText(url) {
    const x = new XMLHttpRequest();
    try {
        x.open("GET", url, false);
        x.send();
    } catch (e) {
        return null;
    }
    return x.status === 200 ? x.responseText : null;
}

let pending = 0;
// Number of writes not finished yet; the host waits for 0 before it quits.
export function pendingWrites() { return pending; }

// Writes `text` (UTF-8) to `url` and calls `done(ok)` when it is finished.
// Returns false only if the request cannot be started.
//
// Qt 6.8's file: PUT cannot report failure synchronously: a synchronous PUT
// creates the file but writes no body, and every PUT ends with status 0, written
// or not. So the content goes out asynchronously, and `ok` comes from reading
// the file back (a synchronous GET) once the PUT is done.
export function writeText(url, text, done) {
    const x = new XMLHttpRequest();
    try {
        x.open("PUT", url);
    } catch (e) {
        if (done) done(false);
        return false;
    }
    ++pending;
    x.onreadystatechange = function () {
        if (x.readyState !== XMLHttpRequest.DONE)
            return;
        const ok = readText(url) === text;
        --pending;
        if (done) done(ok);
    };
    x.send(text);
    return true;
}
