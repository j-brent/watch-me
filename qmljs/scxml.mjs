// SCXML -> plain tree for the statechart view and the event-log attribution
// (docs/HOST-CONTRACT.md §3 `tree`). A port of cpp/src/ScxmlTreeModel.cpp.
//
// QML's XMLHttpRequest.responseXML is a read-only DOM without comments, and the
// spec-row fallback needs the <!-- T-xx --> comments, so this file carries its
// own small XML tokenizer: start/end/empty elements with attributes, comments,
// processing instructions, CDATA and text. It does not validate, which is enough
// for a file that Qt SCXML has already loaded without parse errors.

const ENTITIES = { lt: "<", gt: ">", amp: "&", quot: "\"", apos: "'" };

function decode(s) {
    return s.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+);/g, (m, e) => {
        if (e[0] === "#")
            return String.fromCodePoint(e[1] === "x" ? parseInt(e.slice(2), 16) : parseInt(e.slice(1), 10));
        return ENTITIES[e] !== undefined ? ENTITIES[e] : m;
    });
}

// Calls sink.start(name, attrs), sink.end(name), sink.comment(text) in document order.
// Throws an Error with a position on malformed markup.
export function tokenize(xml, sink) {
    let i = 0;
    const n = xml.length;
    const attrRe = /([^\s=\/>]+)\s*=\s*("([^"]*)"|'([^']*)')/g;
    while (i < n) {
        const lt = xml.indexOf("<", i);
        if (lt < 0)
            break;
        if (xml.startsWith("<!--", lt)) {
            const e = xml.indexOf("-->", lt + 4);
            if (e < 0) throw new Error("unterminated comment at " + lt);
            sink.comment(xml.slice(lt + 4, e));
            i = e + 3;
        } else if (xml.startsWith("<![CDATA[", lt)) {
            const e = xml.indexOf("]]>", lt);
            if (e < 0) throw new Error("unterminated CDATA at " + lt);
            i = e + 3;
        } else if (xml.startsWith("<?", lt)) {
            const e = xml.indexOf("?>", lt);
            if (e < 0) throw new Error("unterminated processing instruction at " + lt);
            i = e + 2;
        } else if (xml.startsWith("<!", lt)) {          // <!DOCTYPE ...>
            const e = xml.indexOf(">", lt);
            if (e < 0) throw new Error("unterminated declaration at " + lt);
            i = e + 1;
        } else if (xml[lt + 1] === "/") {
            const e = xml.indexOf(">", lt);
            if (e < 0) throw new Error("unterminated end tag at " + lt);
            sink.end(xml.slice(lt + 2, e).trim());
            i = e + 1;
        } else {
            // find the closing '>' outside quoted attribute values
            let j = lt + 1, q = "";
            for (; j < n; ++j) {
                const c = xml[j];
                if (q) { if (c === q) q = ""; }
                else if (c === "\"" || c === "'") q = c;
                else if (c === ">") break;
            }
            if (j >= n) throw new Error("unterminated start tag at " + lt);
            let body = xml.slice(lt + 1, j);
            const empty = body.endsWith("/");
            if (empty) body = body.slice(0, -1);
            const m = /^\s*([^\s\/>]+)/.exec(body);
            if (!m) throw new Error("bad tag at " + lt);
            const name = m[1];
            const attrs = {};
            attrRe.lastIndex = m[0].length;
            let a;
            while ((a = attrRe.exec(body)) !== null)
                attrs[a[1]] = decode(a[3] !== undefined ? a[3] : a[4]);
            sink.start(name, attrs);
            if (empty) sink.end(name);
            i = j + 1;
        }
    }
}

const STATE_TAGS = ["state", "parallel", "final", "history"];

// Returns {nodes: {id: node}, transitions: [t], order: [id], rootId, error}.
// node: {id, kind, historyType, parent, initial, children: [id], transitions: [index], depth}
// transition: {source, event, cond, targets: [id], type, specId}
export function parse(xml) {
    const nodes = {}, transitions = [], order = [];
    let rootId = "";
    const stack = [];
    let openTransition = -1;
    let anon = 0;
    const specRe = /\bT-\d+\b/;

    function addNode(id, kind) {
        const nd = { id, kind, historyType: "", parent: "", initial: "", children: [], transitions: [], depth: stack.length };
        if (stack.length) {
            nd.parent = stack[stack.length - 1];
            nodes[nd.parent].children.push(id);
        }
        nodes[id] = nd;
        order.push(id);
    }

    try {
        tokenize(xml, {
            start(name, a) {
                if (name === "scxml") {
                    rootId = a.name !== undefined ? a.name : "scxml";
                    addNode(rootId, "root");
                    nodes[rootId].initial = a.initial || "";
                    stack.push(rootId);
                } else if (STATE_TAGS.includes(name)) {
                    let id = a.id || "";
                    if (!id) id = "_anon" + (++anon);
                    addNode(id, name === "state" ? "atomic" : name);
                    if (name === "history")
                        nodes[id].historyType = a.type ? a.type : "shallow";
                    if (a.initial !== undefined)
                        nodes[id].initial = a.initial;
                    stack.push(id);
                } else if (name === "transition" && stack.length) {
                    const owner = stack[stack.length - 1];
                    if (nodes[owner].kind === "history")          // default transition of a <history> (D-22)
                        return;
                    if ((a.event || "").startsWith("host."))      // host boundary plumbing (D-24)
                        return;
                    transitions.push({
                        source: owner,
                        event: a.event || "",
                        cond: a.cond || "",
                        targets: (a.target || "").split(" ").filter(s => s.length > 0),
                        type: a.type !== undefined ? a.type : "external",
                        specId: "",
                    });
                    openTransition = transitions.length - 1;
                    nodes[owner].transitions.push(openTransition);
                } else if (name === "log" && openTransition >= 0) {
                    const m = specRe.exec(a.label || "");
                    if (m && !transitions[openTransition].specId)
                        transitions[openTransition].specId = m[0];
                }
            },
            end(name) {
                if (STATE_TAGS.includes(name) || name === "scxml")
                    stack.pop();
                else if (name === "transition")
                    openTransition = -1;
            },
            comment(text) {
                // Fallback: "<transition .../> <!-- T-xx -->" right after the element.
                if (!transitions.length) return;
                const t = transitions[transitions.length - 1];
                if (!t.specId) {
                    const m = specRe.exec(text);
                    if (m) t.specId = m[0];
                }
            },
        });
    } catch (e) {
        return { nodes: {}, transitions: [], order: [], rootId: "", error: String(e.message || e) };
    }

    // Classify states and resolve implicit initial children (first child state).
    for (const id of order) {
        const nd = nodes[id];
        if (nd.kind === "atomic" && nd.children.some(c => nodes[c].kind !== "history"))
            nd.kind = "compound";
        if ((nd.kind === "compound" || nd.kind === "root") && !nd.initial) {
            const first = nd.children.find(c => nodes[c].kind !== "history");
            if (first) nd.initial = first;
        }
    }
    return { nodes, transitions, order, rootId, error: "" };
}
