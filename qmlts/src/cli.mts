// The command line (HOST-CONTRACT §5) and the --script steps (§5.1), parsed into
// typed values. Mirrors the QCommandLineParser set-up of cpp/src/main.cpp.
import { Wall, parseIso } from "./wallclock.mjs";
import { Button, asButton } from "./buttonRouter.mjs";

export interface Options {
    noGui: boolean;
    autoInsert: boolean;
    time: Wall | null;          // --time; null: the host's current time
    manualTime: boolean;
    script: string;
    exit: boolean;
    grab: string;               // "" when not set
    grabDelay: number;
    size: { width: number; height: number } | null;
    help: boolean;
    cwd: string;                // set by the launcher (--host-cwd): relative paths resolve here
}

export type ParseResult = { ok: true; options: Options } | { ok: false; error: string };

export const HELP = `Usage: watch-me-qmlts [options]
Harel's 1987 watch statechart, executable (TypeScript/QML host).

Options:
  -h, --help          Displays help.
  --auto-insert       Insert the battery at start-up.
  --script <steps>    Semicolon separated steps run after start-up: 'a' (click),
                      '+a'/'-a' (hold/release), 'wait:ms', or any SCXML event name.
  --time <datetime>   Clock value set when the battery is inserted (ISO 8601,
                      e.g. 1987-06-01T10:08:30).
  --size <WxH>        Window size, e.g. 1920x1080.
  --manual-time       Time does not follow the real clock; only --script steps
                      'clock:ms' and 'advance:ms' move the watch time and the
                      statechart's timeouts.
  --exit              Quit when the --script has run.
  --grab <file>       Save a screenshot of the window to <file> and quit.
  --grab-delay <ms>   Delay before --grab (ms). Default: 800.
  --no-gui            No window: run the statechart, input path, event log and
                      --script only (used by tools/verify.py). Not with --grab,
                      --size or 'select:' steps.`;

const FLAGS = ["no-gui", "auto-insert", "manual-time", "exit", "help"] as const;
const VALUED = ["time", "script", "grab", "grab-delay", "size", "host-cwd"] as const;
type Flag = typeof FLAGS[number];
type Valued = typeof VALUED[number];

const isFlag = (s: string): s is Flag => (FLAGS as readonly string[]).indexOf(s) >= 0;
const isValued = (s: string): s is Valued => (VALUED as readonly string[]).indexOf(s) >= 0;

/**
 * The application's own arguments: Qt.application.arguments holds the qml tool's
 * command line too; ours follow the first "--".
 */
export function appArguments(all: readonly string[]): string[] {
    const i = all.indexOf("--");
    return i < 0 ? [] : all.slice(i + 1);
}

export function parseOptions(args: readonly string[]): ParseResult {
    const flags = new Set<Flag>();
    const values = new Map<Valued, string>();
    for (let i = 0; i < args.length; ++i) {
        const arg = args[i] ?? "";
        if (arg === "-h" || arg === "-?") {
            flags.add("help");
            continue;
        }
        const m = /^--?([a-z-]+)(?:=([\s\S]*))?$/.exec(arg);
        if (!m)
            return { ok: false, error: `Unexpected argument '${arg}'.` };
        const name = m[1] ?? "";
        if (isFlag(name) && m[2] === undefined) {
            flags.add(name);
        } else if (isValued(name)) {
            let v = m[2];
            if (v === undefined) {
                v = args[++i];
                if (v === undefined)
                    return { ok: false, error: `Missing value after '${arg}'.` };
            }
            values.set(name, v);
        } else {
            return { ok: false, error: `Unknown option '${name}'.` };
        }
    }
    const timeText = values.get("time");
    let size: Options["size"] = null;
    const sizeText = values.get("size");
    if (sizeText !== undefined) {
        const wh = /^(\d+)x(\d+)$/.exec(sizeText);
        if (wh)
            size = { width: parseInt(wh[1] ?? "0", 10), height: parseInt(wh[2] ?? "0", 10) };
    }
    const options: Options = {
        noGui: flags.has("no-gui"),
        autoInsert: flags.has("auto-insert"),
        time: timeText === undefined ? null : parseIso(timeText),
        manualTime: flags.has("manual-time"),
        script: values.get("script") ?? "",
        exit: flags.has("exit"),
        grab: values.get("grab") ?? "",
        grabDelay: parseInt(values.get("grab-delay") ?? "800", 10) || 0,
        size,
        help: flags.has("help"),
        cwd: values.get("host-cwd") ?? "",
    };
    if (options.noGui && (options.grab !== "" || sizeText !== undefined))
        return { ok: false, error: "watch-me: --grab and --size need the window; they cannot be used with --no-gui." };
    return { ok: true, options };
}

// ---- script steps (§5.1)

export type Step =
    | { kind: "wait"; ms: number }
    | { kind: "alarm"; which: number; minutes: number }
    | { kind: "jump"; which: number; secs: number }
    | { kind: "settime"; h: number; m: number; s: number }
    | { kind: "advance"; ms: number }
    | { kind: "clock"; ms: number }
    | { kind: "speed"; value: number }
    | { kind: "export"; path: string }
    | { kind: "select"; id: string }
    | { kind: "set"; name: string; value: boolean | number }
    | { kind: "click"; button: Button }
    | { kind: "press"; button: Button }
    | { kind: "release"; button: Button }
    | { kind: "ignored"; text: string }         // 'x', '+x', '-x' for a non-button x: press() ignores it
    | { kind: "submit"; event: string };

/** QString::toInt: whole decimal number or 0. */
const toInt = (s: string | undefined): number => (s !== undefined && /^\s*[+-]?\d+\s*$/.test(s) ? parseInt(s, 10) : 0);
/** QString::toDouble: a number or 0. */
const toDouble = (s: string | undefined): number => {
    const v = s === undefined || s.trim() === "" ? NaN : Number(s);
    return Number.isFinite(v) ? v : 0;
};

export function splitSteps(script: string): string[] {
    return script.split(";").filter((s) => s !== "").map((s) => s.trim());
}

export function parseStep(s: string): Step {
    const parts = s.split(":");
    const head = parts[0];
    if (s.startsWith("wait:"))
        return { kind: "wait", ms: toInt(s.slice(5)) };
    if (head === "alarm" && parts.length === 4)
        return { kind: "alarm", which: toInt(parts[1]), minutes: toInt(parts[2]) * 60 + toInt(parts[3]) };
    if (head === "jump" && parts.length === 3)
        return { kind: "jump", which: toInt(parts[1]), secs: toInt(parts[2]) };
    if (head === "settime" && parts.length === 4)
        return { kind: "settime", h: toInt(parts[1]), m: toInt(parts[2]), s: toInt(parts[3]) };
    if (head === "advance" && parts.length === 2)
        return { kind: "advance", ms: toInt(parts[1]) };
    if (head === "clock" && parts.length === 2)
        return { kind: "clock", ms: toInt(parts[1]) };
    if (head === "speed" && parts.length === 2)
        return { kind: "speed", value: toDouble(parts[1]) };
    if (s.startsWith("export:"))                 // the rest of the step: C:\… keeps its ':'
        return { kind: "export", path: s.slice(7) };
    if (head === "select" && parts.length === 2)
        return { kind: "select", id: parts[1] ?? "" };
    if (head === "set" && parts.length === 3) {
        const v = parts[2];
        return { kind: "set", name: parts[1] ?? "", value: v === "true" ? true : v === "false" ? false : toDouble(v) };
    }
    if (s.length === 1) {
        const b = asButton(s);
        return b === null ? { kind: "ignored", text: s } : { kind: "click", button: b };
    }
    if (s.length === 2 && (s[0] === "+" || s[0] === "-")) {
        const b = asButton(s[1]);
        if (b === null)
            return { kind: "ignored", text: s };
        return s[0] === "+" ? { kind: "press", button: b } : { kind: "release", button: b };
    }
    return { kind: "submit", event: s };
}
