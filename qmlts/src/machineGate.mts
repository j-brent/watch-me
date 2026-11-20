// The one door to the state machine for every host object. It forwards events and
// knows when the machine has caught up with them, which the C++ host gets from
// QCoreApplication::sendPostedEvents and JavaScript cannot do synchronously.
//
// Observed with Qt 6.8 (qmlts/README.md): submitEvent queues the event and posts
// one call that processes it later on the event loop; that call drains the whole
// external queue (including events submitted by handlers while it runs) and then
// emits reachedStableState. So after any reachedStableState, everything submitted
// before it has been processed.
// https://doc.qt.io/qt-6/qscxmlstatemachine.html#reachedStableState
import { EventData, Later, Machine } from "./ports.mjs";

/** The QScxmlStateMachine members the gate calls (StateMachineLoader.stateMachine). */
export interface ScxmlStateMachine {
    submitEvent(name: string, data?: EventData): void;
    activeStateNames(compress: boolean): string[];
    readonly initialized: boolean;
    readonly running: boolean;
}

/** If no reachedStableState comes (machine stopped), continue anyway after this long. */
export const SETTLE_WATCHDOG_MS = 1000;

export class MachineGate implements Machine {
    private unsettled = 0;               // events submitted since the last stable state
    private waiting: Array<() => void> = [];
    private watchdogArmed = false;
    private stables = 0;

    constructor(private readonly raw: () => ScxmlStateMachine | null, private readonly later: Later) {}

    get initialized(): boolean {
        return this.raw()?.initialized ?? false;
    }

    submitEvent(name: string, data?: EventData): void {
        const m = this.raw();
        if (m === null)
            return;
        ++this.unsettled;
        if (data === undefined)
            m.submitEvent(name);
        else
            m.submitEvent(name, data);
    }

    activeStateNames(compress: boolean): string[] {
        const m = this.raw();
        return m === null ? [] : m.activeStateNames(compress);
    }

    /** Call `fn` once the machine has processed every event submitted so far. */
    settle(fn: () => void): void {
        if (this.unsettled === 0) {
            this.later(0, fn);
            return;
        }
        this.waiting.push(fn);
        if (!this.watchdogArmed)
            this.armWatchdog();
    }

    private armWatchdog(): void {
        this.watchdogArmed = true;
        const seen = this.stables;
        this.later(SETTLE_WATCHDOG_MS, () => {
            this.watchdogArmed = false;
            if (this.waiting.length === 0)
                return;
            if (this.stables !== seen) {
                this.armWatchdog();         // the machine is alive; these waits are newer
                return;
            }
            console.warn("qmlts: the state machine did not reach a stable state; continuing");
            this.onStable();
        });
    }

    /** Connect to reachedStableState. */
    onStable(): void {
        ++this.stables;
        this.unsettled = 0;
        const run = this.waiting;
        this.waiting = [];
        for (const fn of run)
            fn();
    }
}
