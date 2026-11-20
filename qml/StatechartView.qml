import QtQuick

Item {
    id: view
    clip: true
    StateBox {
        x: 8; y: 8
        node: tree.root
    }
}
