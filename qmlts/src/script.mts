// Runs the --script steps (HOST-CONTRACT §5.1): the first 200 ms after start-up,
// each later one 60 ms after the previous (or after wait:ms). A step that is
// asynchronous in this host (advance:, export:) finishes before the gap starts,
// so later steps see its effect as they would in the C++ host.
import { Later } from "./ports.mjs";
import { Step, parseStep, splitSteps } from "./cli.mjs";
import { Button } from "./buttonRouter.mjs";

export const FIRST_STEP_MS = 200;
export const STEP_GAP_MS = 60;
export const CLICK_MS = 30;

/** What the steps act on: the host objects, through their public members. */
export interface ScriptTarget {
    later: Later;
    press(b: Button): void;
    release(b: Button): void;
    submit(event: string): void;
    setDataValue(name: string, value: boolean | number): void;
    setAlarm(which: number, minutes: number): void;
    jumpBeforeAlarm(which: number, secs: number): void;
    setTime(h: number, m: number, s: number): void;
    advanceClock(ms: number): void;
    advanceTimeouts(ms: number, done: () => void): void;
    setSpeed(x: number): void;
    exportLog(path: string, done: (ok: boolean) => void): void;
    /** GUI only: set `selected` on the statechart view; false without a window. */
    select(id: string): boolean;
    /** Called once, after the last step. */
    finished(): void;
}

/** The QQuickItem members findByObjectName walks. */
export interface ItemNode {
    readonly objectName: string;
    readonly children: ArrayLike<ItemNode> | null | undefined;
}

/**
 * QML has no QObject::findChild: search the visual item tree, depth first, for
 * the item named `name` (the statechart view of a select: step).
 */
export function findByObjectName(root: ItemNode | null | undefined, name: string): ItemNode | null {
    if (!root)
        return null;
    if (root.objectName === name)
        return root;
    const kids = root.children;
    for (let i = 0; kids && i < kids.length; ++i) {
        const hit = findByObjectName(kids[i], name);
        if (hit !== null)
            return hit;
    }
    return null;
}

export class ScriptRunner {
    private readonly steps: Step[];

    constructor(script: string, private readonly target: ScriptTarget) {
        this.steps = splitSteps(script).map(parseStep);
    }

    start(): void {
        this.target.later(FIRST_STEP_MS, () => this.runNext());
    }

    private runNext(): void {
        const step = this.steps.shift();
        if (step === undefined) {
            this.target.finished();
            return;
        }
        const t = this.target;
        const next = (gap: number) => t.later(gap, () => this.runNext());
        switch (step.kind) {
        case "wait":
            next(step.ms);
            return;
        case "advance":
            t.advanceTimeouts(step.ms, () => next(STEP_GAP_MS));
            return;
        case "export":
            t.exportLog(step.path, () => next(STEP_GAP_MS));
            return;
        case "alarm": t.setAlarm(step.which, step.minutes); break;
        case "jump": t.jumpBeforeAlarm(step.which, step.secs); break;
        case "settime": t.setTime(step.h, step.m, step.s); break;
        case "clock": t.advanceClock(step.ms); break;
        case "speed": t.setSpeed(step.value); break;
        case "set": t.setDataValue(step.name, step.value); break;
        case "select":
            if (!t.select(step.id))
                console.warn("watch-me: 'select:' needs the window; ignored with --no-gui");
            break;
        case "click": {
            const b = step.button;
            t.press(b);
            t.later(CLICK_MS, () => t.release(b));
            break;
        }
        case "press": t.press(step.button); break;
        case "release": t.release(step.button); break;
        case "ignored": break;
        case "submit": t.submit(step.event); break;
        default: {
            const never: never = step;
            return never;
        }
        }
        next(STEP_GAP_MS);
    }
}
