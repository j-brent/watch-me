// The clock behind the statechart's "n sec in X" timers (HOST-CONTRACT §5.2,
// DESIGN-NOTES D-20/D-21; port of cpp/src/TimeoutService.cpp).
//
// watch.scxml decides every timer: timer.start{id, secs, gen} arms one,
// timer.cancel{id} disarms it. This only counts time and submits timeout.<id>{gen}
// when a deadline passes; the statechart ignores a stale gen.
//
// The C++ host drains the machine's queue synchronously between two deadlines
// (QCoreApplication::sendPostedEvents). JavaScript cannot: Qt SCXML processes
// submitted events from a queued call on the event loop. So advance() is
// asynchronous: it hands each step to `settle`, which calls back once the machine
// has processed everything submitted so far (the next reachedStableState), and
// `done` runs when the whole step is over.
import { EventData, MachineRef } from "./ports.mjs";

/** Calls `fn` once the machine has processed every event submitted before this call. */
export type Settle = (fn: () => void) => void;

interface Pending {
    due: number;
    gen: unknown;
}

interface QueuedStep {
    ms: number;
    done: (() => void) | undefined;
}

export class TimeoutService {
    private nowMs = 0;
    private readonly pending = new Map<string, Pending>();
    private advancing = false;
    private readonly queue: QueuedStep[] = [];
    private lastReal: number | null = null;

    constructor(private readonly machine: MachineRef, private readonly settle: Settle) {}

    get now(): number { return this.nowMs; }
    get busy(): boolean { return this.advancing; }

    /** Pending deadline of a timer in ms from now, or -1 if it is not armed. */
    remaining(id: string): number {
        const p = this.pending.get(id);
        return p === undefined ? -1 : p.due - this.nowMs;
    }

    onMachineEvent(name: string, data: EventData): void {
        if (name === "timer.start") {
            const secs = Number(data["secs"]);
            const ms = Math.round((Number.isFinite(secs) ? secs : 0) * 1000);
            this.pending.set(String(data["id"]), { due: this.nowMs + ms, gen: data["gen"] });
        } else if (name === "timer.cancel") {
            this.pending.delete(String(data["id"]));
        }
    }

    /**
     * Move the clock forward by `ms`, firing every deadline reached, earliest first
     * (ties: smaller id), each handled by the machine before the next is looked for.
     * A call made while a step is running waits for it (advance never re-enters).
     */
    advance(ms: number, done?: () => void): void {
        if (!(ms >= 0)) {
            done?.();
            return;
        }
        this.queue.push({ ms, done });
        if (!this.advancing)
            this.nextStep();
    }

    /** Real-time drive (not --manual-time): call about every 20 ms with a real clock in ms. */
    tick(realMs: number): void {
        if (this.lastReal === null) {
            this.lastReal = realMs;
            return;
        }
        if (this.advancing || this.queue.length > 0)
            return;                 // elapsed time is kept for the next tick
        const elapsed = realMs - this.lastReal;
        this.lastReal = realMs;
        if (elapsed > 0)
            this.advance(elapsed);
    }

    private nextStep(): void {
        const step = this.queue.shift();
        if (step === undefined)
            return;
        this.advancing = true;
        // 1. Timers requested by input submitted just before count from the current now.
        this.settle(() => this.fireUntil(this.nowMs + step.ms, step.done));
    }

    private earliest(end: number): string | null {
        let id: string | null = null;
        let due = Infinity;
        for (const [key, p] of this.pending) {
            if (p.due <= end && (p.due < due || (p.due === due && id !== null && key < id))) {
                due = p.due;
                id = key;
            }
        }
        return id;
    }

    private fireUntil(end: number, done: (() => void) | undefined): void {
        // 2. The earliest deadline within the step, handled at its own time (D-21).
        const id = this.earliest(end);
        const p = id === null ? undefined : this.pending.get(id);
        const m = this.machine();
        if (id === null || p === undefined || m === null) {
            // 3. The end of the step.
            this.nowMs = end;
            this.advancing = false;
            done?.();
            this.nextStep();
            return;
        }
        this.nowMs = p.due;
        this.pending.delete(id);
        m.submitEvent("timeout." + id, { gen: p.gen });
        this.settle(() => this.fireUntil(end, done));
    }
}
