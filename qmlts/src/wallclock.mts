// A wall clock without a time zone (HOST-CONTRACT §3 `watch`): a moment is the
// number of milliseconds since 1970-01-01 00:00 read as UTC, so adding time never
// jumps over a daylight-saving gap. Mirrors the QDateTime(…, Qt::UTC) arithmetic
// of cpp/src/WatchModel.cpp.

export const MS_PER_SEC = 1000;
export const MS_PER_MIN = 60 * MS_PER_SEC;
export const MS_PER_DAY = 24 * 60 * MS_PER_MIN;

/** Wall-clock milliseconds. A distinct name so a duration is not passed by mistake. */
export type Wall = number & { readonly __wall: unique symbol };

export interface Fields {
    year: number;
    month: number;   // 1–12
    day: number;     // 1–31
    hour: number;
    minute: number;
    second: number;
    ms: number;
}

export function wall(year: number, month: number, day: number, hour = 0, minute = 0, second = 0, ms = 0): Wall {
    // Date.UTC treats years 0–99 as 1900–1999; setUTCFullYear does not. Month and
    // day may overflow (day 0 is the last day of the month before), as with Date.
    const d = new Date(Date.UTC(2000, 0, 1, hour, minute, second, ms));
    d.setUTCFullYear(year, month - 1, day);
    return d.getTime() as Wall;
}

export function fields(t: Wall): Fields {
    const d = new Date(t);
    return {
        year: d.getUTCFullYear(), month: d.getUTCMonth() + 1, day: d.getUTCDate(),
        hour: d.getUTCHours(), minute: d.getUTCMinutes(), second: d.getUTCSeconds(), ms: d.getUTCMilliseconds(),
    };
}

export function fromFields(f: Fields): Wall {
    return wall(f.year, f.month, f.day, f.hour, f.minute, f.second, f.ms);
}

export function addMs(t: Wall, ms: number): Wall {
    return (t + ms) as Wall;
}

/** 1 = Monday … 7 = Sunday, like QDate::dayOfWeek. */
export function dayOfWeek(t: Wall): number {
    return (new Date(t).getUTCDay() + 6) % 7 + 1;
}

export function daysInMonth(year: number, month: number): number {
    return new Date(wall(year, month + 1, 0)).getUTCDate();
}

/** Whole seconds since the epoch (floor), like QDateTime::toSecsSinceEpoch. */
export function secsSinceEpoch(t: Wall): number {
    return Math.floor(t / MS_PER_SEC);
}

/** The host's current local time, read as a wall clock (the fields a person sees). */
export function localNow(): Wall {
    const d = new Date();
    return wall(d.getFullYear(), d.getMonth() + 1, d.getDate(), d.getHours(), d.getMinutes(), d.getSeconds());
}

/**
 * Parse an ISO 8601 date-time such as 1987-06-01T10:08:30 into the wall clock,
 * taking the fields as written (a zone suffix is ignored, as WatchModel does when
 * it copies the date and time of the parsed QDateTime). Null if it is not one.
 */
export function parseIso(text: string): Wall | null {
    const m = /^(\d{4})-(\d{2})-(\d{2})(?:[T ](\d{2}):(\d{2})(?::(\d{2})(?:[.,](\d{1,3})\d*)?)?)?(?:Z|[+-]\d{2}(?::?\d{2})?)?$/
        .exec(text.trim());
    if (!m)
        return null;
    const num = (s: string | undefined) => (s === undefined ? 0 : parseInt(s, 10));
    const year = num(m[1]), month = num(m[2]), day = num(m[3]);
    const hour = num(m[4]), minute = num(m[5]), second = num(m[6]);
    if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)
        || hour > 23 || minute > 59 || second > 59)
        return null;
    const ms = m[7] === undefined ? 0 : parseInt((m[7] + "00").slice(0, 3), 10);
    return wall(year, month, day, hour, minute, second, ms);
}
