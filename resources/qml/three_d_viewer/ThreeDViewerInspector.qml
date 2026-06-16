import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f6f6"

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true

        Column {
            id: contentColumn
            width: flick.width - 24
            x: 12
            y: 12
            spacing: 18

            Column {
                width: parent.width
                spacing: 6

                Text {
                    text: qsTr("Scene")
                    font.bold: true
                    font.pixelSize: 18
                    color: "#202020"
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                    text: inspectorState && inspectorState.filePath.length > 0
                        ? inspectorState.filePath
                        : qsTr("No file loaded.")
                    color: "#404040"
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 4
                    width: parent.width

                    Text { text: qsTr("Surveys"); color: "#202020" }
                    Text { text: inspectorState ? String(inspectorState.surveyCount) : "0"; color: "#202020" }

                    Text { text: qsTr("Stations"); color: "#202020" }
                    Text { text: inspectorState ? String(inspectorState.stationCount) : "0"; color: "#202020" }

                    Text { text: qsTr("Shots"); color: "#202020" }
                    Text { text: inspectorState ? String(inspectorState.shotCount) : "0"; color: "#202020" }

                    Text { text: qsTr("Meshes"); color: "#202020" }
                    Text { text: inspectorState ? String(inspectorState.meshCount) : "0"; color: "#202020" }

                    Text { text: qsTr("Surfaces"); color: "#202020" }
                    Text { text: inspectorState ? String(inspectorState.surfaceCount) : "0"; color: "#202020" }
                }
            }

            Column {
                width: parent.width
                spacing: 8

                Text {
                    text: qsTr("Layers")
                    font.bold: true
                    font.pixelSize: 18
                    color: "#202020"
                }

                Repeater {
                    model: layerModel

                    delegate: CheckBox {
                        required property int layerIndex
                        required property string label
                        required property int layerChecked

                        width: contentColumn.width
                        checked: layerChecked === Qt.Checked
                        text: label
                        onToggled: {
                            if (layerModel) {
                                layerModel.setLayerVisible(layerIndex, checked)
                            }
                        }
                    }
                }
            }
        }
    }
}
