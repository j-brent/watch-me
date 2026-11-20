// Observes the state machine and turns it into view data (HOST-CONTRACT §3
// `tracer` and §4; port of cpp/src/TransitionTracer.cpp):
//  - the active configuration (activeSet),
//  - the states touched by the last macrostep (flashSet),
//  - the event log ("▶ a.down", "◀ sw.lap", "T-10 a.down: time → alarm1").
import { EventData, MachineRef, Now } from "./ports.mjs";
import { ScxmlTree, Transition, compareSpecIds } from "./scxmlTree.mjs";

export interface TracerSink {
    activeChanged(): void;
    flashChanged(): void;
    entriesChanged(): void;
}

/** Writes the exported log; calls done(ok) when the write has finished. */
export type TextWriter = (path: string, text: string, done: (ok: boolean) => void) => void;

export const MAX_ENTRIES = 500;
const RECENT = 4;                       // stable configurations kept for attribution
const SPEC_LABEL = /^T-\d+$/;
const IDENTIFIER = /^[A-Za-z_][A-Za-z0-9_]*$/;
const OUTGOING = /^(model|sw|beep)\./;  // logged as "◀ name key=value …" (§4.2)

export type FlagMap = { [id: string]: true };
export type StampMap = { [id: string]: number };

/** QVariant::toString for the values an event carries. */
function variantText(v: unknown): string {
    if (v === null || v === undefined)
        return "";
    if (typeof v === "number" || typeof v === "boolean" || typeof v === "string")
        return String(v);
    return "";
}

export class TransitionTracer {
    private active = new Set<string>();
    private prevActive = new Set<string>();
    private readonly pendingFlash = new Set<string>();
    private readonly recent: Set<string>[] = [];   // newest first
    private activeList: string[] = [];
    private activeMap: FlagMap = {};
    private flashMap: StampMap = {};
    private entryList: string[] = [];
    private readonly fired = new Set<string>();
    private data: EventData = {};                   // last host.data (D-24)
    private lastSpec = "";
    private lastStamp = 0;

    constructor(private readonly machine: MachineRef, private readonly tree: ScxmlTree,
                private readonly now: Now, private readonly sink: TracerSink) {}

    get activeStates(): string[] { return this.activeList; }
    get activeSet(): FlagMap { return this.activeMap; }
    get flashSet(): StampMap { return this.flashMap; }
    get entries(): string[] { return this.entryList; }
    get lastSpecId(): string { return this.lastSpec; }
    get firedSpecIds(): string[] {
        const ids: string[] = [];
        this.fired.forEach((id) => ids.push(id));
        return ids.sort(compareSpecIds);
    }
    isActive(id: string): boolean { return this.active.has(id); }

    /** A value that changes every time: a state that flashes again gets a new stamp. */
    private stamp(): number {
        this.lastStamp = Math.max(this.now(), this.lastStamp + 1);
        return this.lastStamp;
    }

    // ---- inputs from the machine
    onMachineEvent(name: string, data: EventData): void {
        if (name === "host.data") {
            this.data = data;
            return;
        }
        if (!OUTGOING.test(name))
            return;
        let line = "◀ " + name;
        for (const key of Object.keys(data).sort())
            line += ` ${key}=${variantText(data[key])}`;
        this.append(line);
    }

    /** reachedStableState and runningChanged. */
    onStable(): void {
        const m = this.machine();
        const names = m === null ? [] : m.activeStateNames(false);
        const now = new Set(names);
        for (const s of names)
            if (!this.prevActive.has(s))
                this.pendingFlash.add(s);          // newly entered in this macrostep
        this.active = now;
        this.activeList = names.slice();
        const map: FlagMap = {};
        for (const s of names)
            map[s] = true;
        this.activeMap = map;
        this.prevActive = now;
        // only distinct configurations (HOST-CONTRACT §4.3, DESIGN-NOTES D-26)
        const newest = this.recent[0];
        const same = newest !== undefined && newest.size === now.size && [...now].every((s) => newest.has(s));
        if (!same)
            this.recent.unshift(now);
        if (this.recent.length > RECENT)
            this.recent.length = RECENT;
        this.sink.activeChanged();
        if (this.pendingFlash.size > 0) {
            const stamp = this.stamp();
            const flash: StampMap = {};
            this.pendingFlash.forEach((s) => { flash[s] = stamp; });
            this.pendingFlash.clear();
            this.flashMap = flash;
            this.sink.flashChanged();
        }
    }

    /**
     * QScxmlStateMachine::log. Qt delivers it after the macrostep (D-18), so the
     * transition is found in the tree: its source was active in one of the last few
     * stable configurations and its targets are active now (§4.3).
     */
    onLog(label: string, msg: string): void {
        if (!SPEC_LABEL.test(label))
            return;
        const best = this.attribute(label);
        const source = best?.source ?? "";
        const targets = best?.targets ?? [];
        this.fired.add(label);
        this.lastSpec = label;
        const stamp = this.stamp();
        const flash: StampMap = {};
        for (const k of Object.keys(this.flashMap))
            flash[k] = this.flashMap[k] ?? stamp;
        if (source !== "")
            flash[source] = stamp;
        for (const t of targets)
            flash[t] = stamp;
        this.flashMap = flash;
        this.sink.flashChanged();
        const to = targets.length === 0 ? "(no state change)" : targets.join(", ");
        this.append(`${label} ${msg === "" ? "·" : msg}: ${source} → ${to}`);
    }

    private attribute(label: string): Transition | undefined {
        const wasActive = (id: string) => this.recent.some((c) => c.has(id));
        const landed = (t: Transition) => t.targets.every((tg) => {
            const where = this.tree.kindOf(tg) === "history" ? this.tree.parentOf(tg) : tg;
            return this.active.has(where);
        });
        const rows = this.tree.transitions.filter((t) => t.specId === label && wasActive(t.source));
        let cands = rows.filter(landed);
        if (cands.length === 0)
            cands = rows;
        if (cands.length <= 1)
            return cands[0];
        // Same row, same source: the clh(main*) pairs "[hv_x] → x_H" / "→ x". hv_x only
        // changes when x is exited, so its last host.data value tells the branch apart.
        // (The last match of each kind, as in the C++ loop.)
        const reversed = cands.slice().reverse();
        const taken = reversed.find((t) => t.cond !== "" && IDENTIFIER.test(t.cond) && this.data[t.cond] === true);
        const uncond = reversed.find((t) => t.cond === "");
        return taken ?? uncond ?? cands[0];
    }

    // ---- the log
    /** An input submitted through ButtonRouter (§4.1). */
    noteInput(event: string): void {
        this.append("▶ " + event);
    }

    private append(line: string): void {
        const next = this.entryList.concat([line]);
        this.entryList = next.length > MAX_ENTRIES ? next.slice(next.length - MAX_ENTRIES) : next;
        this.sink.entriesChanged();
    }

    clearLog(): void {
        this.entryList = [];
        this.sink.entriesChanged();
    }

    clearFlash(): void {
        this.flashMap = {};
        this.sink.flashChanged();
    }

    logText(): string {
        return this.entryList.join("\n");
    }

    /** The export format of §4.4 (entries, then "# active: …"). */
    exportText(): string {
        return this.logText() + "\n# active: " + this.activeList.join(" ") + "\n";
    }

    /**
     * §4.4. The write is asynchronous (files.mts), so the result says whether it was
     * started; `done(ok)` reports whether the file was written.
     */
    exportLog(path: string, write: TextWriter, done?: (ok: boolean) => void): boolean {
        const p = path.startsWith("file://") ? path.slice(7) : path;
        if (p === "") {
            done?.(false);
            return false;
        }
        write(p, this.exportText(), (ok) => {
            if (!ok)
                console.warn(`could not export the log to ${p}`);
            done?.(ok);
        });
        return true;
    }
}
