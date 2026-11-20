//! SpecTags (docs/HOST-CONTRACT.md §3): the confidence tag [P]/[F]/[A] of every
//! `| T-xx | …` row of docs/STATECHART-SPEC.md. Port of cpp/src/SpecTags.cpp.

use std::collections::BTreeMap;

#[derive(Default)]
pub struct Tags {
    pub tags: BTreeMap<String, String>,
}

impl Tags {
    pub fn load(path: &std::path::Path) -> Tags {
        let mut t = Tags::default();
        if let Ok(text) = std::fs::read_to_string(path) {
            t.parse(&text);
        }
        t
    }

    /// Each line `^\|\s*(T-\d+)\s*\|.*\[(P|F|A)\]` (greedy: the last tag on the line).
    pub fn parse(&mut self, markdown: &str) {
        for line in markdown.lines() {
            let Some(rest) = line.strip_prefix('|') else { continue };
            let rest = rest.trim_start();
            let Some(after) = rest.strip_prefix("T-") else { continue };
            let digits = after.chars().take_while(|c| c.is_ascii_digit()).count();
            if digits == 0 {
                continue;
            }
            let id = format!("T-{}", &after[..digits]);
            let Some(tail) = after[digits..].trim_start().strip_prefix('|') else { continue };
            let tag = ["[P]", "[F]", "[A]"]
                .iter()
                .filter_map(|t| tail.rfind(t).map(|p| (p, &t[1..2])))
                .max_by_key(|(p, _)| *p);
            if let Some((_, tag)) = tag {
                self.tags.insert(id, tag.to_string());
            }
        }
    }

    pub fn tag_for(&self, id: &str) -> &str {
        self.tags.get(id).map(String::as_str).unwrap_or("")
    }

    /// The weakest of the given rows: A < F < P ("" if none is known).
    pub fn weakest(&self, ids: &[String]) -> &str {
        let rank = |t: &str| match t { "A" => 0, "F" => 1, "P" => 2, _ => 3 };
        ids.iter().map(|id| self.tag_for(id)).filter(|t| !t.is_empty()).min_by_key(|t| rank(t)).unwrap_or("")
    }
}
