//! ScxmlTreeModel (docs/HOST-CONTRACT.md §3 `tree`): watch.scxml parsed with an
//! XML reader (not Qt SCXML) into a plain tree for StatechartView.qml and the
//! tracer. Port of cpp/src/ScxmlTreeModel.cpp.

use crate::qt::{qmap, qv_str, qv_strings, QVariantList, QVariantMap};
use cxx_qt_lib::QVariant;
use std::collections::HashMap;

#[derive(Clone, Debug, Default)]
pub struct Transition {
    pub source: String,
    pub event: String,
    pub cond: String,
    pub targets: Vec<String>,
    pub kind: String, // "external" | "internal"
    pub spec_id: String,
}

#[derive(Clone, Debug, Default)]
pub struct Node {
    pub id: String,
    pub kind: String, // atomic | compound | parallel | history | final | root
    pub history_type: String,
    pub parent: String,
    pub initial: String,
    pub children: Vec<String>,
    pub transitions: Vec<usize>,
    pub depth: i32,
}

#[derive(Default)]
pub struct Tree {
    pub nodes: HashMap<String, Node>,
    pub transitions: Vec<Transition>,
    pub order: Vec<String>,
    pub root_id: String,
    pub error: String,
}

/// The first `T-<digits>` with word boundaries on both sides (regex `\bT-\d+\b`).
pub fn find_spec_id(s: &str) -> Option<String> {
    let b = s.as_bytes();
    let word = |c: u8| c.is_ascii_alphanumeric() || c == b'_';
    let mut i = 0;
    while let Some(p) = s[i..].find("T-") {
        let start = i + p;
        let mut end = start + 2;
        while end < b.len() && b[end].is_ascii_digit() {
            end += 1;
        }
        let left_ok = start == 0 || !word(b[start - 1]);
        let right_ok = end == b.len() || !word(b[end]);
        if end > start + 2 && left_ok && right_ok {
            return Some(s[start..end].to_string());
        }
        i = start + 1;
    }
    None
}

struct Parser<'t> {
    tree: &'t mut Tree,
    stack: Vec<String>,
    open_transition: Option<usize>,
    anon: usize,
}

impl Parser<'_> {
    fn add_node(&mut self, id: &str, kind: &str) {
        let mut n = Node { id: id.into(), kind: kind.into(), depth: self.stack.len() as i32, ..Default::default() };
        if let Some(parent) = self.stack.last() {
            n.parent = parent.clone();
            self.tree.nodes.get_mut(parent).unwrap().children.push(id.into());
        }
        self.tree.nodes.insert(id.into(), n);
        self.tree.order.push(id.into());
    }

    // Document order, start and end of every element, like QXmlStreamReader.
    fn walk(&mut self, node: roxmltree::Node) {
        if node.is_comment() {
            // Fallback: "<transition .../> <!-- T-xx -->" right after the element.
            if let Some(t) = self.tree.transitions.last_mut() {
                if t.spec_id.is_empty() {
                    if let Some(id) = find_spec_id(node.text().unwrap_or("")) {
                        t.spec_id = id;
                    }
                }
            }
            return;
        }
        if !node.is_element() {
            return;
        }
        let name = node.tag_name().name();
        let attr = |a: &str| node.attribute(a).unwrap_or("").to_string();
        let mut pushed = false;
        let mut opened = false;
        match name {
            "scxml" => {
                let id = node.attribute("name").unwrap_or("scxml").to_string();
                self.tree.root_id = id.clone();
                self.add_node(&id, "root");
                self.tree.nodes.get_mut(&id).unwrap().initial = attr("initial");
                self.stack.push(id);
                pushed = true;
            }
            "state" | "parallel" | "final" | "history" => {
                let mut id = attr("id");
                if id.is_empty() {
                    self.anon += 1;
                    id = format!("_anon{}", self.anon);
                }
                self.add_node(&id, if name == "state" { "atomic" } else { name });
                let n = self.tree.nodes.get_mut(&id).unwrap();
                if name == "history" {
                    n.history_type = node.attribute("type").filter(|t| !t.is_empty()).unwrap_or("shallow").into();
                }
                if let Some(init) = node.attribute("initial") {
                    n.initial = init.into();
                }
                self.stack.push(id);
                pushed = true;
            }
            "transition" if !self.stack.is_empty() => {
                let owner = self.stack.last().unwrap().clone();
                // Not transitions: a <history>'s default (D-22) and host.* plumbing
                // (D-24): a transition all of whose events are host.* events.
                let event = attr("event");
                let events: Vec<&str> = event.split(' ').filter(|e| !e.is_empty()).collect();
                let host_only = !events.is_empty() && events.iter().all(|e| e.starts_with("host."));
                let skip = self.tree.nodes[&owner].kind == "history" || host_only;
                if !skip {
                    let t = Transition {
                        source: owner.clone(),
                        event: attr("event"),
                        cond: attr("cond"),
                        targets: attr("target").split(' ').filter(|s| !s.is_empty()).map(String::from).collect(),
                        kind: node.attribute("type").unwrap_or("external").into(),
                        spec_id: String::new(),
                    };
                    self.tree.transitions.push(t);
                    let index = self.tree.transitions.len() - 1;
                    self.open_transition = Some(index);
                    self.tree.nodes.get_mut(&owner).unwrap().transitions.push(index);
                    opened = true;
                }
            }
            "log" => {
                if let Some(i) = self.open_transition {
                    if let Some(id) = find_spec_id(&attr("label")) {
                        if self.tree.transitions[i].spec_id.is_empty() {
                            self.tree.transitions[i].spec_id = id;
                        }
                    }
                }
            }
            _ => {}
        }
        for child in node.children() {
            self.walk(child);
        }
        if pushed {
            self.stack.pop();
        }
        if opened || name == "transition" {
            self.open_transition = None;
        }
    }
}

impl Tree {
    pub fn load(path: &std::path::Path) -> Tree {
        let mut tree = Tree::default();
        let text = match std::fs::read_to_string(path) {
            Ok(t) => t,
            Err(_) => {
                tree.error = format!("cannot open {}", path.display());
                return tree;
            }
        };
        let doc = match roxmltree::Document::parse(&text) {
            Ok(d) => d,
            Err(e) => {
                tree.error = e.to_string();
                return tree;
            }
        };
        let mut p = Parser { tree: &mut tree, stack: vec![], open_transition: None, anon: 0 };
        p.walk(doc.root_element());

        // Classify states and resolve implicit initial children (first child state).
        let kinds: HashMap<String, String> = tree.nodes.iter().map(|(k, n)| (k.clone(), n.kind.clone())).collect();
        for n in tree.nodes.values_mut() {
            let non_history = |c: &String| kinds.get(c).map(|k| k != "history").unwrap_or(true);
            if n.kind == "atomic" && n.children.iter().any(non_history) {
                n.kind = "compound".into();
            }
            if (n.kind == "compound" || n.kind == "root") && n.initial.is_empty() {
                if let Some(c) = n.children.iter().find(|c| non_history(c)) {
                    n.initial = c.clone();
                }
            }
        }
        tree
    }

    fn transition_variant(t: &Transition) -> QVariant {
        QVariant::from(&qmap([
            ("source", qv_str(&t.source)),
            ("event", qv_str(&t.event)),
            ("cond", qv_str(&t.cond)),
            ("targets", qv_strings(&t.targets)),
            ("type", qv_str(&t.kind)),
            ("specId", qv_str(&t.spec_id)),
        ]))
    }

    fn to_variant(&self, id: &str) -> QVariantMap {
        let n = self.nodes.get(id).cloned().unwrap_or_default();
        let kids: QVariantList = n.children.iter().map(|c| QVariant::from(&self.to_variant(c))).collect();
        let trans: QVariantList = n.transitions.iter().map(|&i| Self::transition_variant(&self.transitions[i])).collect();
        qmap([
            ("id", qv_str(&n.id)),
            ("kind", qv_str(&n.kind)),
            ("historyType", qv_str(&n.history_type)),
            ("parent", qv_str(&n.parent)),
            ("initial", qv_str(&n.initial)),
            ("depth", QVariant::from(&n.depth)),
            ("children", QVariant::from(&kids)),
            ("transitions", QVariant::from(&trans)),
        ])
    }

    pub fn root_variant(&self) -> QVariantMap {
        if self.root_id.is_empty() { QVariantMap::default() } else { self.to_variant(&self.root_id) }
    }

    pub fn node_variant(&self, id: &str) -> QVariantMap {
        if self.nodes.contains_key(id) { self.to_variant(id) } else { QVariantMap::default() }
    }

    pub fn transitions_from(&self, id: &str) -> QVariantList {
        self.nodes.get(id).map(|n| n.transitions.iter().map(|&i| Self::transition_variant(&self.transitions[i])).collect())
            .unwrap_or_default()
    }

    pub fn transitions_into(&self, id: &str) -> QVariantList {
        self.transitions.iter().filter(|t| t.targets.iter().any(|x| x == id)).map(Self::transition_variant).collect()
    }

    /// Nearest first.
    pub fn ancestors(&self, id: &str) -> Vec<String> {
        let mut out = vec![];
        let mut p = self.nodes.get(id).map(|n| n.parent.clone()).unwrap_or_default();
        while !p.is_empty() {
            out.push(p.clone());
            p = self.nodes.get(&p).map(|n| n.parent.clone()).unwrap_or_default();
        }
        out
    }

    /// Sorted (numeric), unique.
    pub fn spec_ids(&self) -> Vec<String> {
        let mut ids: Vec<String> = vec![];
        for t in &self.transitions {
            if !t.spec_id.is_empty() && !ids.contains(&t.spec_id) {
                ids.push(t.spec_id.clone());
            }
        }
        ids.sort_by_key(|s| s[2..].parse::<u32>().unwrap_or(0));
        ids
    }
}
