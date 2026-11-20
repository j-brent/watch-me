import QtQml
import "../build/scxmlTree.mjs" as Impl

// HOST-CONTRACT §3 `tree` (src/scxmlTree.mts). `core` is shared with the tracer.
QtObject {
    id: self
    readonly property var core: new Impl.ScxmlTree()
    property var root: ({})
    property string errorString: ""

    /** Parse the SCXML text; false (and errorString) if it is not well-formed. */
    function load(xml) {
        const ok = core.load(xml)
        errorString = core.errorString
        root = core.root() || ({})
        return ok
    }
    function node(id) { return core.nodeView(id) || ({}) }
    function transitionsFrom(id) { return core.transitionsFrom(id) }
    function transitionsInto(id) { return core.transitionsInto(id) }
    function ancestors(id) { return core.ancestors(id) }
    function specIds() { return core.specIds() }
}
