// What the LCD shows: a pure function of the active configuration and the
// watch's values (STATECHART-SPEC §4). No mode logic: every branch reads the
// configuration through `on(id)`. Kept out of Lcd.qml so tests/tst_lcd.cpp can
// check it with QJSEngine::importModule
// (https://doc.qt.io/qt-6/qjsengine.html#importModule).

export const DAYS = ["MO", "TU", "WE", "TH", "FR", "SA", "SU"];

// Two characters: the last two digits of n, padded on the left with `fill`.
export function pad2(n, fill) {
    const s = "" + n;
    return s.length >= 2 ? s.slice(-2) : (fill === undefined ? "0" : fill) + s;
}

// The hour digits of HH:MM. 24-hour mode: 00..23, zero-padded. 12-hour mode:
// 1..12 with a blank instead of a leading zero (0 and 12 show as 12).
export function hourDigits(h, is24h) {
    if (is24h)
        return pad2(h, "0");
    return pad2(h % 12 === 0 ? 12 : h % 12, " ");
}

export function ampm(h, is24h) {
    return is24h ? "24H" : (h < 12 ? "AM" : "PM");
}

// Stopwatch digits MM SS cc for a counter in centiseconds. The display has two
// minute digits: after 59'59"99 it wraps to 00'00"00 and keeps counting.
export function stopwatchDigits(centis) {
    const tot = Math.floor(centis / 100);
    return { main: pad2(Math.floor(tot / 60) % 60) + pad2(tot % 60), small: pad2(centis % 100) };
}

// on(id) -> bool for the active configuration; w: the watch values (WatchModel's
// properties); blink: true in the half of the 2 Hz cycle where blinking fields
// are hidden. Returns the fields Lcd.qml renders.
export function view(on, w, blink) {
    const v = { label: "", main: "    ", sep: ":", small: "  ", blinkMain: [], blinkSmall: false,
                blinkLabel: false, status: "", lap: false, ampm: "", ampmBlink: false };
    if (!on("alive"))
        return v;
    const timeMain = (h, m) => hourDigits(h, w.is24h) + pad2(m);
    if (on("update") && (on("update_mon") || on("update_date") || on("update_day") || on("update_year"))) {
        v.label = DAYS[w.weekday - 1];
        if (on("update_year")) {
            v.main = "" + w.year; v.sep = " ";
            if (blink) v.blinkMain = [0, 1, 2, 3];
        } else {
            v.main = pad2(w.month, " ") + pad2(w.day, " "); v.sep = "-";
            if (blink && on("update_mon")) v.blinkMain = [0, 1];
            if (blink && on("update_date")) v.blinkMain = [2, 3];
            v.blinkLabel = blink && on("update_day");
        }
        return v;
    }
    if (on("time") || on("wait") || on("update") || on("alarms_beep")) {
        v.label = on("update") ? "SET" : DAYS[w.weekday - 1];
        v.main = timeMain(w.hour, w.minute);
        v.small = pad2(w.second);
        v.ampm = ampm(w.hour, w.is24h);
        if (blink) {
            if (on("update_sec")) v.blinkSmall = true;
            if (on("update_1min")) v.blinkMain = [3];
            if (on("update_10min")) v.blinkMain = [2];
            if (on("update_hr")) v.blinkMain = [0, 1];
            if (on("update_mode")) v.ampmBlink = true;
        }
        return v;
    }
    if (on("date")) {
        v.label = DAYS[w.weekday - 1];
        v.main = pad2(w.month, " ") + pad2(w.day, " "); v.sep = "-";
        return v;
    }
    for (const n of [1, 2]) {
        if (on("alarm" + n) || on("update" + n)) {
            const t = n === 1 ? w.t1 : w.t2;
            v.label = "AL" + n;
            v.main = timeMain(Math.floor(t / 60), t % 60);
            v.ampm = ampm(Math.floor(t / 60), w.is24h);
            v.status = on("alarm" + n + "_on") ? "ON" : on("alarm" + n + "_off") ? "OFF" : "";
            if (blink && on("update" + n + "_hr")) v.blinkMain = [0, 1];
            if (blink && on("update" + n + "_10min")) v.blinkMain = [2];
            if (blink && on("update" + n + "_1min")) v.blinkMain = [3];
            return v;
        }
    }
    if (on("chime")) {
        v.label = "CH"; v.main = "--00";
        v.status = on("chime_on") ? "ON" : "OFF";
        return v;
    }
    if (on("stopwatch")) {
        const d = stopwatchDigits(on("sw_lap") ? w.lapCentis : w.stopwatchCentis);
        v.label = "ST";
        v.main = d.main;
        v.small = d.small;
        v.lap = on("sw_lap");
        return v;
    }
    return v;
}
