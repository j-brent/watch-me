// `specTags` (docs/HOST-CONTRACT.md §3): the confidence badge of every spec row,
// read from docs/STATECHART-SPEC.md ("| T-xx | … [P|F|A] …").
import QtQml
import "files.mjs" as Files

QtObject {
    property var tags: ({})          // "T-xx" -> "P" | "F" | "A"

    function load(url) {
        const md = Files.readText(url)
        if (md === null)
            return false
        parse(md)
        return true
    }
    function parse(markdown) {
        const t = {}
        const row = /^\|\s*(T-\d+)\s*\|.*\[(P|F|A)\]/gm
        let m
        while ((m = row.exec(markdown)) !== null)
            t[m[1]] = m[2]
        tags = t
    }
    function tagFor(id) { return tags[id] || "" }
    // The weakest tag among `ids` (A < F < P), "" if none is known.
    function weakest(ids) {
        const rank = { A: 0, F: 1, P: 2 }
        let best = ""
        for (const id of ids) {
            const t = tagFor(id)
            if (t && (!best || rank[t] < rank[best]))
                best = t
        }
        return best
    }
}
