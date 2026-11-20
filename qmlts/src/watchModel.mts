// Values only (HOST-CONTRACT §3 `watch`, port of cpp/src/WatchModel.cpp): the
// simulated clock T, the alarm times T1/T2, the stopwatch counter and the 12/24 h
// flag. No modes: what is being edited, whether the stopwatch is shown, etc. all
// come from the statechart.
//
// SCXML -> host: model.reset, model.increment{field}, sw.start/stop/lap/reset, beep.short
// host -> SCXML: T.hits.T1, T.hits.T2, T.hits.hour and host.alarms{T1, T2} (D-24)
import { EventData, MachineRef } from "./ports.mjs";
import {
    Wall, wall, fields, fromFields, addMs, dayOfWeek, daysInMonth, secsSinceEpoch, localNow,
    MS_PER_MIN, MS_PER_SEC, MS_PER_DAY,
} from "./wallclock.mjs";

/** Change notifications, one per group of QML properties (the C++ NOTIFY signals). */
export interface WatchSink {
    timeChanged(): void;
    settingsChanged(): void;
    stopwatchChanged(): void;
    speedChanged(): void;
    shortBeep(): void;
}

export type Which = 1 | 2;
export const MAX_SPEED = 3600;
const MINUTES_PER_DAY = 1440;
const NOON = 12 * 60;

const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

/** Add 1 to one decimal digit of `value` (place 1 or 10) modulo `modulo`, without carry. */
export function incDigit(value: number, place: 1 | 10, modulo: number): number {
    const digit = Math.floor(value / place) % 10;
    return value - digit * place + ((digit + 1) % modulo) * place;
}

export class WatchModel {
    private nowT: Wall = wall(1987, 6, 1, 12, 0, 0);
    private resetTime: Wall | null = null;
    private weekdayOffset = 0;       // the 'day' update field moves the weekday on its own
    private is24: boolean = false;
    private alarm1 = NOON;
    private alarm2 = NOON;
    private swAccumMs = 0;           // simulated ms of the finished run segments
    private swStartMs = 0;           // simulated ms at the last start
    private simMs = 0;               // simulated ms since construction (monotonic)
    private lap = 0;
    private swRunning = false;
    private speedValue = 1;
    private lastReal: number | null = null;
    private carry = 0;

    constructor(private readonly machine: MachineRef, private readonly sink: WatchSink) {}

    // ---- read side (the QML properties)
    get hour(): number { return fields(this.nowT).hour; }
    get minute(): number { return fields(this.nowT).minute; }
    get second(): number { return fields(this.nowT).second; }
    get year(): number { return fields(this.nowT).year; }
    get month(): number { return fields(this.nowT).month; }
    get day(): number { return fields(this.nowT).day; }
    /** 1 = Monday … 7 = Sunday, including the offset set with update_day. */
    get weekday(): number { return (dayOfWeek(this.nowT) - 1 + this.weekdayOffset) % 7 + 1; }
    get is24h(): boolean { return this.is24; }
    get t1(): number { return this.alarm1; }
    get t2(): number { return this.alarm2; }
    get stopwatchCentis(): number {
        return Math.floor((this.swAccumMs + (this.swRunning ? this.simMs - this.swStartMs : 0)) / 10);
    }
    get lapCentis(): number { return this.lap; }
    get stopwatchRunning(): boolean { return this.swRunning; }
    get speed(): number { return this.speedValue; }
    get now(): Wall { return this.nowT; }

    setSpeed(s: number): void {
        const v = clamp(Number(s) || 0, 0, MAX_SPEED);
        if (Math.abs(v - this.speedValue) <= 1e-12 * Math.max(Math.abs(v), Math.abs(this.speedValue)))
            return;
        this.speedValue = v;
        this.sink.speedChanged();
    }

    /** model.reset uses this instead of the host's current time (--time). */
    setResetTime(t: Wall | null): void { this.resetTime = t; }

    // ---- the statechart's outgoing events (D-11)
    onMachineEvent(name: string, data: EventData): void {
        switch (name) {
        case "model.reset":
            this.reset();
            break;
        case "model.increment":
            this.increment(String(data["field"] ?? ""));
            break;
        case "sw.start":
            if (!this.swRunning) {
                this.swRunning = true;
                this.swStartMs = this.simMs;
                this.sink.stopwatchChanged();
            }
            break;
        case "sw.stop":
            if (this.swRunning) {
                this.swAccumMs += this.simMs - this.swStartMs;   // keep ms: no rounding per segment
                this.swRunning = false;
                this.sink.stopwatchChanged();
            }
            break;
        case "sw.lap":
            this.lap = this.stopwatchCentis;
            this.sink.stopwatchChanged();
            break;
        case "sw.reset":
            this.swAccumMs = 0;
            this.lap = 0;
            this.swRunning = false;
            this.sink.stopwatchChanged();
            break;
        case "beep.short":
            this.sink.shortBeep();
            break;
        }
    }

    // ---- time
    /** Real-time drive: call about every 20 ms with a real clock in ms. */
    tick(realMs: number): void {
        if (this.lastReal === null) {
            this.lastReal = realMs;
            return;
        }
        const sim = (realMs - this.lastReal) * this.speedValue + this.carry;
        this.lastReal = realMs;
        const whole = Math.trunc(sim);
        this.carry = sim - whole;
        if (whole > 0)
            this.advance(whole);
    }

    /** Move T forward; T.hits.* for every whole minute crossed in (before, now]. */
    advance(simMs: number): void {
        if (!(simMs > 0))
            return;
        const before = this.nowT;
        this.nowT = addMs(this.nowT, simMs);
        this.simMs += simMs;
        const b = fields(before);
        const first = addMs(wall(b.year, b.month, b.day, b.hour, b.minute), MS_PER_MIN);
        for (let t = first; t <= this.nowT; t = addMs(t, MS_PER_MIN)) {
            const f = fields(t);
            const mod = f.hour * 60 + f.minute;
            if (mod === this.alarm1)
                this.submit("T.hits.T1");
            if (mod === this.alarm2)
                this.submit("T.hits.T2");
            if (f.minute === 0)
                this.submit("T.hits.hour");
        }
        if (secsSinceEpoch(before) !== secsSinceEpoch(this.nowT))
            this.sink.timeChanged();
        if (this.swRunning)
            this.sink.stopwatchChanged();
    }

    /** Clamp to 0–23 / 0–59 / 0–59, keep the date; no T.hits.* */
    setTime(h: number, m: number, s: number): void {
        const f = fields(this.nowT);
        this.nowT = wall(f.year, f.month, f.day, clamp(Math.trunc(h), 0, 23), clamp(Math.trunc(m), 0, 59),
                         clamp(Math.trunc(s), 0, 59));
        this.sink.timeChanged();
    }

    /** Whole seconds of `t` (drops the milliseconds), like WatchModel::setDateTime. */
    private setDateTime(t: Wall): void {
        const f = fields(t);
        this.nowT = wall(f.year, f.month, f.day, f.hour, f.minute, f.second);
        this.sink.timeChanged();
    }

    /** Minutes after midnight, modulo 1440 (−1 is 23:59), then host.alarms. */
    setAlarm(which: number, minutes: number): void {
        const v = ((Math.trunc(minutes) % MINUTES_PER_DAY) + MINUTES_PER_DAY) % MINUTES_PER_DAY;
        if (which === 2)
            this.alarm2 = v;
        else
            this.alarm1 = v;
        this.pushAlarms();
        this.sink.settingsChanged();
    }

    /** T := next occurrence of t<which> − secs (today if not in the past, else tomorrow). */
    jumpBeforeAlarm(which: number, secs: number): void {
        const target = which === 2 ? this.alarm2 : this.alarm1;
        const f = fields(this.nowT);
        let t = wall(f.year, f.month, f.day, Math.floor(target / 60), target % 60);
        if (addMs(t, -secs * MS_PER_SEC) < this.nowT)
            t = addMs(t, MS_PER_DAY);
        this.setDateTime(addMs(t, -secs * MS_PER_SEC));
    }

    /** Host boundary (D-24): the statechart assigns T1/T2 on host.alarms. */
    pushAlarms(): void {
        const m = this.machine();
        if (!m || !m.initialized)
            return;
        m.submitEvent("host.alarms", { T1: this.alarm1, T2: this.alarm2 });
    }

    reset(): void {
        this.setDateTime(this.resetTime ?? localNow());
        this.weekdayOffset = 0;
        this.is24 = false;
        this.alarm1 = this.alarm2 = NOON;
        this.swAccumMs = 0;
        this.lap = 0;
        this.swRunning = false;
        this.pushAlarms();
        this.sink.settingsChanged();
        this.sink.stopwatchChanged();
        this.sink.timeChanged();
    }

    increment(field: string): void {
        const f = fields(this.nowT);
        const clampDay = () => { f.day = Math.min(f.day, daysInMonth(f.year, f.month)); };
        switch (field) {
        case "update_sec": f.second = 0; f.ms = 0; break;
        case "update_1min": f.minute = incDigit(f.minute, 1, 10); f.ms = 0; break;
        case "update_10min": f.minute = incDigit(f.minute, 10, 6); f.ms = 0; break;
        case "update_hr": f.hour = (f.hour + 1) % 24; f.ms = 0; break;
        case "update_mon": f.month = f.month % 12 + 1; clampDay(); break;
        case "update_date": f.day = f.day % daysInMonth(f.year, f.month) + 1; break;
        case "update_day": this.weekdayOffset = (this.weekdayOffset + 1) % 7; break;
        case "update_year": f.year = f.year >= 2099 ? 1980 : f.year + 1; clampDay(); break;
        case "update_mode":
            this.is24 = !this.is24;
            this.sink.settingsChanged();
            return;
        default: {
            const m = /^update([12])_(hr|10min|1min)$/.exec(field);
            if (!m) {
                console.warn(`WatchModel: unknown field '${field}'`);
                return;
            }
            const which = m[1] === "2" ? 2 : 1;
            const a = which === 2 ? this.alarm2 : this.alarm1;
            let h = Math.floor(a / 60), min = a % 60;
            if (m[2] === "hr")
                h = (h + 1) % 24;
            else if (m[2] === "10min")
                min = incDigit(min, 10, 6);
            else
                min = incDigit(min, 1, 10);
            if (which === 2)
                this.alarm2 = h * 60 + min;
            else
                this.alarm1 = h * 60 + min;
            this.pushAlarms();
            this.sink.settingsChanged();
            return;
        }
        }
        this.nowT = fromFields(f);
        this.sink.timeChanged();
    }

    private submit(event: string): void {
        this.machine()?.submitEvent(event);
    }
}
