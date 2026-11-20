// The confidence tag ([P]/[F]/[A]) of every T-xx row of docs/STATECHART-SPEC.md,
// for the badges in the statechart view (HOST-CONTRACT §3 `specTags`; port of
// cpp/src/SpecTags.cpp).

export type Tag = "P" | "F" | "A";

const RANK: { readonly [t in Tag]: number } = { A: 0, F: 1, P: 2 };

export class SpecTags {
    private readonly map = new Map<string, Tag>();

    /** Read every table row `| T-xx | … [P|F|A] …` (the last tag on the line wins). */
    parse(markdown: string): void {
        const row = /^\|\s*(T-\d+)\s*\|.*\[(P|F|A)\]/gm;
        for (let m = row.exec(markdown); m !== null; m = row.exec(markdown)) {
            const [, id, tag] = m;
            if (id !== undefined && (tag === "P" || tag === "F" || tag === "A"))
                this.map.set(id, tag);
        }
    }

    /** Plain object T-xx -> tag, for QML. */
    get tags(): { [id: string]: Tag } {
        const o: { [id: string]: Tag } = {};
        for (const [id, t] of this.map)
            o[id] = t;
        return o;
    }

    tagFor(specId: string): Tag | "" {
        return this.map.get(specId) ?? "";
    }

    /** The weakest tag among `ids` (A < F < P), "" if none is known. */
    weakest(ids: readonly string[]): Tag | "" {
        let best: Tag | "" = "";
        for (const id of ids) {
            const t = this.tagFor(id);
            if (t !== "" && (best === "" || RANK[t] < RANK[best]))
                best = t;
        }
        return best;
    }
}
