// The small slices of Qt that the host logic calls. The QML wrappers (qmlts/qml/)
// pass the real objects; nothing in src/ imports Qt, so every module here is plain,
// typed JavaScript.

/** Data an SCXML event carries: a QVariantMap arrives as a plain object, no data as "". */
export type EventData = { readonly [key: string]: unknown };

/** What the host may do with the QScxmlStateMachine (StateMachineLoader.stateMachine). */
export interface Machine {
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#submitEvent-2
    submitEvent(name: string, data?: EventData): void;
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#activeStateNames
    activeStateNames(compress: boolean): string[];
    readonly initialized: boolean;
}

/** The machine as seen through a late binding: null until it has loaded. */
export type MachineRef = () => Machine | null;

/** A one-shot timer on the Qt event loop (a QML Timer). */
export type Later = (ms: number, fn: () => void) => void;

/** Monotonic-enough milliseconds for the real-time clocks and flash stamps. */
export type Now = () => number;

/** Normalise QScxmlEvent.data: a map stays a map, anything else is no data. */
export function eventData(data: unknown): EventData {
    return data !== null && typeof data === "object" ? (data as EventData) : {};
}
