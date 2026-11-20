import QtQml
import "../build/specTags.mjs" as Impl

// HOST-CONTRACT §3 `specTags` (src/specTags.mts).
QtObject {
    id: self
    readonly property var core: new Impl.SpecTags()
    property var tags: ({})

    function parse(markdown) { core.parse(markdown); tags = core.tags }
    function tagFor(id) { return core.tagFor(id) }
    function weakest(ids) { return core.weakest(Array.from(ids || [])) }
}
