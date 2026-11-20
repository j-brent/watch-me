// Parses watch.scxml into a plain tree for the statechart view (HOST-CONTRACT §3
// `tree`; port of cpp/src/ScxmlTreeModel.cpp). Nothing is hand-written per state:
// the view is generated from the same file the state machine runs.
import { tokens, XmlError } from "./xml.mjs";

export type NodeKind = "root" | "compound" | "parallel" | "atomic" | "history" | "final";

export interface Transition {
    source: string;      // id of the state the <transition> is a child of
    event: string;       // space separated descriptors, may be empty (eventless)
    cond: string;
    targets: string[];   // empty for targetless transitions
    type: string;        // "external" (default) or "internal"
    specId: string;      // T-xx from <log label="T-xx"/> inside, else a following comment
}

interface Node {
    id: string;
    kind: NodeKind;
    historyType: string; // shallow | deep (history nodes only)
    parent: string;
    initial: string;
    children: string[];
    transitions: number[];   // indices into transitions
    depth: number;
}

/** The plain-object form the QML reads (a QVariantMap in the C++ host). */
export interface NodeView {
    id: string;
    kind: NodeKind;
    historyType: string;
    parent: string;
    initial: string;
    depth: number;
    children: NodeView[];
    transitions: Transition[];
}

const SPEC_ID = /\bT-\d+\b/;
const STATE_ELEMENTS = new Set(["state", "parallel", "final", "history"]);

function copy(t: Transition): Transition {
    return { source: t.source, event: t.event, cond: t.cond, targets: t.targets.slice(), type: t.type, specId: t.specId };
}

/** T-2 < T-10: the numeric order QCollator::setNumericMode gives the C++ host. */
export function compareSpecIds(a: string, b: string): number {
    const na = parseInt(a.slice(2), 10), nb = parseInt(b.slice(2), 10);
    return na !== nb ? na - nb : a < b ? -1 : a > b ? 1 : 0;
}

export class ScxmlTree {
    private readonly nodes = new Map<string, Node>();
    private readonly list: Transition[] = [];
    private readonly order: string[] = [];
    private rootIdValue = "";
    private error = "";

    get errorString(): string { return this.error; }
    get rootId(): string { return this.rootIdValue; }
    get stateIds(): string[] { return this.order.slice(); }
    get transitions(): readonly Transition[] { return this.list; }

    /** Parse an SCXML document. False (and errorString) if it is not well-formed. */
    load(xml: string): boolean {
        this.nodes.clear();
        this.list.length = 0;
        this.order.length = 0;
        this.rootIdValue = "";
        this.error = "";
        try {
            this.parse(xml);
        } catch (e) {
            this.error = e instanceof XmlError ? e.message : String(e);
            return false;
        }
        this.classify();
        return true;
    }

    private node(id: string): Node {
        const n = this.nodes.get(id);
        if (n === undefined)
            throw new Error(`no state '${id}'`);
        return n;
    }

    private parse(xml: string): void {
        const stack: string[] = [];        // open state ids
        let openTransition = -1;           // index of the <transition> being read
        let anon = 0;
        const addNode = (id: string, kind: NodeKind) => {
            const parent = stack.length > 0 ? stack[stack.length - 1] ?? "" : "";
            if (parent !== "")
                this.node(parent).children.push(id);
            this.nodes.set(id, { id, kind, historyType: "", parent, initial: "", children: [], transitions: [], depth: stack.length });
            this.order.push(id);
        };
        for (const tok of tokens(xml)) {
            if (tok.kind === "start") {
                const a = tok.attributes;
                if (tok.name === "scxml") {
                    this.rootIdValue = a.get("name") ?? "scxml";
                    addNode(this.rootIdValue, "root");
                    this.node(this.rootIdValue).initial = a.get("initial") ?? "";
                    stack.push(this.rootIdValue);
                } else if (STATE_ELEMENTS.has(tok.name)) {
                    let id = a.get("id") ?? "";
                    if (id === "")
                        id = `_anon${++anon}`;
                    const kind: NodeKind = tok.name === "state" ? "atomic" : (tok.name as NodeKind);
                    addNode(id, kind);
                    const n = this.node(id);
                    if (tok.name === "history")
                        n.historyType = a.get("type") || "shallow";
                    const initial = a.get("initial");
                    if (initial !== undefined)
                        n.initial = initial;
                    stack.push(id);
                } else if (tok.name === "transition" && stack.length > 0) {
                    const owner = this.node(stack[stack.length - 1] ?? "");
                    // the default transition of a <history> is part of the H connector (D-22)
                    if (owner.kind === "history")
                        continue;
                    const event = a.get("event") ?? "";
                    // host-boundary plumbing (D-24) is not part of the chart: a transition
                    // whose events are all host.* events
                    const events = event.split(/\s+/).filter((e) => e.length > 0);
                    if (events.length > 0 && events.every((e) => e.startsWith("host.")))
                        continue;
                    this.list.push({
                        source: owner.id,
                        event,
                        cond: a.get("cond") ?? "",
                        targets: (a.get("target") ?? "").split(" ").filter((s) => s !== ""),
                        type: a.get("type") ?? "external",
                        specId: "",
                    });
                    openTransition = this.list.length - 1;
                    owner.transitions.push(openTransition);
                } else if (tok.name === "log" && openTransition >= 0) {
                    const m = SPEC_ID.exec(a.get("label") ?? "");
                    const t = this.list[openTransition];
                    if (m && t && t.specId === "")
                        t.specId = m[0];
                }
            } else if (tok.kind === "end") {
                if (STATE_ELEMENTS.has(tok.name) || tok.name === "scxml")
                    stack.pop();
                else if (tok.name === "transition")
                    openTransition = -1;
            } else if (tok.kind === "comment" && this.list.length > 0) {
                // Fallback: "<transition .../> <!-- T-xx -->" right after the element.
                const t = this.list[this.list.length - 1];
                const m = SPEC_ID.exec(tok.text);
                if (t && t.specId === "" && m)
                    t.specId = m[0];
            }
        }
    }

    /** Atomic states with state children are compound; resolve implicit initial children. */
    private classify(): void {
        for (const n of this.nodes.values()) {
            if (n.kind === "atomic" && n.children.some((c) => this.nodes.get(c)?.kind !== "history"))
                n.kind = "compound";
            if ((n.kind === "compound" || n.kind === "root") && n.initial === "") {
                const first = n.children.find((c) => this.nodes.get(c)?.kind !== "history");
                if (first !== undefined)
                    n.initial = first;
            }
        }
    }

    private view(id: string): NodeView {
        const n = this.node(id);
        return {
            id: n.id, kind: n.kind, historyType: n.historyType, parent: n.parent, initial: n.initial, depth: n.depth,
            children: n.children.map((c) => this.view(c)),
            transitions: n.transitions.map((i) => this.list[i]).filter((t): t is Transition => t !== undefined).map(copy),
        };
    }

    /** The whole tree from the <scxml> element, or null before a successful load. */
    root(): NodeView | null {
        return this.rootIdValue === "" ? null : this.view(this.rootIdValue);
    }

    nodeView(id: string): NodeView | null {
        return this.nodes.has(id) ? this.view(id) : null;
    }

    kindOf(id: string): NodeKind | undefined { return this.nodes.get(id)?.kind; }
    parentOf(id: string): string { return this.nodes.get(id)?.parent ?? ""; }

    transitionsFrom(id: string): Transition[] {
        const n = this.nodes.get(id);
        if (n === undefined)
            return [];
        return n.transitions.map((i) => this.list[i]).filter((t): t is Transition => t !== undefined).map(copy);
    }

    transitionsInto(id: string): Transition[] {
        return this.list.filter((t) => t.targets.indexOf(id) >= 0).map(copy);
    }

    /** Nearest first. */
    ancestors(id: string): string[] {
        const out: string[] = [];
        let p = this.parentOf(id);
        while (p !== "") {
            out.push(p);
            p = this.parentOf(p);
        }
        return out;
    }

    /** Every T-xx that a transition carries, unique, in numeric order. */
    specIds(): string[] {
        const ids: string[] = [];
        for (const t of this.list)
            if (t.specId !== "" && ids.indexOf(t.specId) < 0)
                ids.push(t.specId);
        return ids.sort(compareSpecIds);
    }
}
