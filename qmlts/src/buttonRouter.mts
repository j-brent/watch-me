// Turns presses/releases of the four watch buttons into SCXML events (a.down /
// a.up …) and forwards other inputs (HOST-CONTRACT §3 `buttons`; port of
// cpp/src/ButtonRouter.cpp). Keeps track of what is held, so keyboard auto-repeat
// and mouse+keyboard combinations never send a second "down" without an "up".
import { EventData, MachineRef } from "./ports.mjs";

export type Button = "a" | "b" | "c" | "d";

export interface RouterSink {
    heldChanged(): void;
    /** host.data changed `name`; "" for the first host.data (everything is new). */
    dataValueChanged(name: string): void;
    /** Every input is logged ("▶ event") before it is submitted. */
    noteInput(event: string): void;
}

export function asButton(b: unknown): Button | null {
    const s = String(b).toLowerCase();
    return s === "a" || s === "b" || s === "c" || s === "d" ? s : null;
}

/** QVariant comparison for the scalar values host.data carries. */
function sameValue(a: unknown, b: unknown): boolean {
    return a === b || (typeof a === "number" && typeof b === "number" && isNaN(a) && isNaN(b));
}

export class ButtonRouter {
    private readonly heldSet = new Set<Button>();
    private data: EventData | null = null;          // last host.data

    constructor(private readonly machine: MachineRef, private readonly sink: RouterSink) {}

    /** button -> true for every button held. */
    get held(): { [b: string]: true } {
        const m: { [b: string]: true } = {};
        this.heldSet.forEach((b) => { m[b] = true; });
        return m;
    }

    press(button: unknown): void {
        const b = asButton(button);
        if (b === null || this.heldSet.has(b))
            return;
        this.heldSet.add(b);
        this.sink.heldChanged();
        this.submit(b + ".down");
    }

    release(button: unknown): void {
        const b = asButton(button);
        if (b === null || !this.heldSet.delete(b))
            return;
        this.sink.heldChanged();
        this.submit(b + ".up");
    }

    releaseAll(): void {
        const held: Button[] = [];
        this.heldSet.forEach((b) => held.push(b));
        for (const b of held)
            this.release(b);
    }

    submit(event: string): void {
        this.sink.noteInput(event);
        this.machine()?.submitEvent(event);
    }

    /** The value of `name` in the last host.data; undefined before the first. */
    dataValue(name: string): unknown {
        return this.data === null ? undefined : this.data[name];
    }

    /** Only "harelAnomalies" can be set: host.anomalies{value}; host.data answers (D-24). */
    setDataValue(name: string, value: unknown): void {
        if (name === "harelAnomalies")
            this.machine()?.submitEvent("host.anomalies", { value: !!value });
        else
            console.warn(`ButtonRouter: '${name}' cannot be set (DESIGN-NOTES D-24)`);
    }

    onMachineEvent(name: string, data: EventData): void {
        if (name !== "host.data")
            return;
        const before = this.data;
        this.data = data;
        if (before === null) {
            this.sink.dataValueChanged("");
            return;
        }
        for (const key of Object.keys(data).sort())
            if (!sameValue(before[key], data[key]))
                this.sink.dataValueChanged(key);
    }
}
