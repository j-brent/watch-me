// File access through QML's XMLHttpRequest, the only file API a QML-only host has.
//   read:  GET  file:…  (needs QML_XHR_ALLOW_FILE_READ=1), synchronous
//   write: PUT  file:…  (needs QML_XHR_ALLOW_FILE_WRITE=1), asynchronous: in Qt 6.8
//          a synchronous PUT creates the file but leaves it empty.
// XHR reports status 0 for file: URLs whether or not the request worked, so a
// write is checked by reading the file back.
// https://doc.qt.io/qt-6/qml-qtqml-xmlhttprequest.html

/** file:// URL for a local path; relative paths are taken against `cwd`. */
export function fileUrl(path: string, cwd: string): string {
    if (/^file:/.test(path))
        return path;
    let p = path.replace(/\\/g, "/");
    if (/^[A-Za-z]:\//.test(p))
        p = "/" + p;                                   // C:/x -> /C:/x (file:///C:/x)
    else if (!p.startsWith("/"))
        p = cwd.replace(/\\/g, "/").replace(/\/?$/, "/") + p;
    if (!p.startsWith("/"))
        p = "/" + p;
    return "file://" + p.split("/").map((seg, i) => (i === 1 && /^[A-Za-z]:$/.test(seg) ? seg : encodeURIComponent(seg))).join("/");
}

/** The whole text of a local file, or null if it cannot be read. */
export function readText(url: string): string | null {
    const xhr = new XMLHttpRequest();
    try {
        xhr.open("GET", url, false);
        xhr.send();
    } catch (e) {
        return null;
    }
    // file: GET gives 200 on success; 0 or 404 when it is missing or not allowed.
    return xhr.readyState === 4 && xhr.status === 200 ? xhr.responseText : null;
}

/** Write `text` (UTF-8) to a local file; `done(ok)` once it is written and read back. */
export function writeText(url: string, text: string, done: (ok: boolean) => void): void {
    const xhr = new XMLHttpRequest();
    let finished = false;
    xhr.onreadystatechange = () => {
        if (xhr.readyState !== 4 || finished)
            return;
        finished = true;
        done(readText(url) === text);
    };
    try {
        xhr.open("PUT", url, true);
        xhr.setRequestHeader("Content-Type", "text/plain;charset=utf-8");
        xhr.send(text);
    } catch (e) {
        if (!finished) {
            finished = true;
            done(false);
        }
    }
}
