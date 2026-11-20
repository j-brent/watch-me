// A small pull parser for the XML that watch.scxml uses: elements, attributes,
// comments, CDATA, processing instructions and a DOCTYPE. QML has no XML reader
// for local files (XmlListModel is for lists, not trees), so the host reads the
// SCXML itself, as QXmlStreamReader does for the C++ host.

export type XmlToken =
    | { kind: "start"; name: string; attributes: Map<string, string>; selfClosing: boolean }
    | { kind: "end"; name: string }
    | { kind: "comment"; text: string }
    | { kind: "text"; text: string };

export class XmlError extends Error {
    constructor(message: string, readonly offset: number) {
        super(`${message} at offset ${offset}`);
    }
}

const ENTITIES: { readonly [name: string]: string } = { amp: "&", lt: "<", gt: ">", quot: "\"", apos: "'" };

export function decodeEntities(s: string, offset = 0): string {
    return s.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[A-Za-z]+);/g, (whole: string, ref: string) => {
        if (ref[0] === "#") {
            const code = ref[1] === "x" ? parseInt(ref.slice(2), 16) : parseInt(ref.slice(1), 10);
            return String.fromCodePoint(code);
        }
        const v = ENTITIES[ref];
        if (v === undefined)
            throw new XmlError(`unknown entity ${whole}`, offset);
        return v;
    });
}

/** The local part of a qualified name: scxml for scxml or sc:scxml. */
function localName(qname: string): string {
    const i = qname.indexOf(":");
    return i < 0 ? qname : qname.slice(i + 1);
}

const NAME = /[A-Za-z_:][-A-Za-z0-9_:.]*/y;
const ATTRIBUTE = /\s*([A-Za-z_:][-A-Za-z0-9_:.]*)\s*=\s*("([^"]*)"|'([^']*)')/y;

/** Tokenise `xml`; throws XmlError on malformed input or unbalanced tags. */
export function* tokens(xml: string): Generator<XmlToken, void, undefined> {
    const open: string[] = [];
    let i = 0;
    const expect = (lit: string, from: number): number => {
        const j = xml.indexOf(lit, from);
        if (j < 0)
            throw new XmlError(`unterminated construct, expected '${lit}'`, from);
        return j;
    };
    while (i < xml.length) {
        const lt = xml.indexOf("<", i);
        if (lt < 0) {
            if (xml.slice(i).trim() !== "")
                yield { kind: "text", text: decodeEntities(xml.slice(i), i) };
            break;
        }
        if (lt > i)
            yield { kind: "text", text: decodeEntities(xml.slice(i, lt), i) };
        if (xml.startsWith("<!--", lt)) {
            const end = expect("-->", lt + 4);
            yield { kind: "comment", text: xml.slice(lt + 4, end) };
            i = end + 3;
        } else if (xml.startsWith("<![CDATA[", lt)) {
            const end = expect("]]>", lt + 9);
            yield { kind: "text", text: xml.slice(lt + 9, end) };
            i = end + 3;
        } else if (xml.startsWith("<?", lt)) {
            i = expect("?>", lt + 2) + 2;
        } else if (xml.startsWith("<!", lt)) {
            i = expect(">", lt + 2) + 1;          // DOCTYPE without an internal subset
        } else if (xml.startsWith("</", lt)) {
            NAME.lastIndex = lt + 2;
            const m = NAME.exec(xml);
            if (!m)
                throw new XmlError("bad end tag", lt);
            const end = expect(">", NAME.lastIndex);
            if (xml.slice(NAME.lastIndex, end).trim() !== "")
                throw new XmlError("bad end tag", lt);
            const name = m[0];
            if (open.pop() !== name)
                throw new XmlError(`mismatched end tag </${name}>`, lt);
            yield { kind: "end", name: localName(name) };
            i = end + 1;
        } else {
            NAME.lastIndex = lt + 1;
            const m = NAME.exec(xml);
            if (!m)
                throw new XmlError("bad start tag", lt);
            const name = m[0];
            let j = NAME.lastIndex;
            const attributes = new Map<string, string>();
            for (;;) {
                ATTRIBUTE.lastIndex = j;
                const a = ATTRIBUTE.exec(xml);
                if (!a)
                    break;
                const [, attr = "", , dq, sq] = a;
                attributes.set(localName(attr), decodeEntities(dq ?? sq ?? "", j));
                j = ATTRIBUTE.lastIndex;
            }
            const rest = /\s*(\/?)>/y;
            rest.lastIndex = j;
            const r = rest.exec(xml);
            if (!r)
                throw new XmlError(`bad start tag <${name}>`, lt);
            const selfClosing = r[1] === "/";
            yield { kind: "start", name: localName(name), attributes, selfClosing };
            if (selfClosing)
                yield { kind: "end", name: localName(name) };
            else
                open.push(name);
            i = rest.lastIndex;
        }
    }
    if (open.length > 0)
        throw new XmlError(`unclosed element <${open[open.length - 1]}>`, xml.length);
}
