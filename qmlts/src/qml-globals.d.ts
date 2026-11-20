// The globals Qt's QML engine gives every script, as far as this host uses them.
// tsconfig has no DOM library, so anything not declared here is a type error.
// https://doc.qt.io/qt-6/qtqml-javascript-hostenvironment.html

declare const console: {
    log(...args: unknown[]): void;
    warn(...args: unknown[]): void;
    error(...args: unknown[]): void;
};

// QML's XMLHttpRequest. On file: URLs, GET needs QML_XHR_ALLOW_FILE_READ=1 and PUT
// needs QML_XHR_ALLOW_FILE_WRITE=1. A synchronous PUT creates the file but writes
// nothing (Qt 6.8), so writes go asynchronously.
// https://doc.qt.io/qt-6/qml-qtqml-xmlhttprequest.html
declare class XMLHttpRequest {
    readonly readyState: number;
    readonly status: number;
    readonly responseText: string;
    onreadystatechange: (() => void) | null;
    open(method: string, url: string, async?: boolean): void;
    setRequestHeader(name: string, value: string): void;
    send(body?: string): void;
    static readonly DONE: number;
}
