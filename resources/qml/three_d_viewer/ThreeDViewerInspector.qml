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

        ColumnLayout {
            id: contentColumn
            width: flick.width - 24
            x: 12
            y: 12
            spacing: 12

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("Scene")

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Model coloring")
                            color: "#202020"
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Survey"), qsTr("Depth")]
                            currentIndex: inspectorState ? inspectorState.meshColorMode : 0
                            onActivated: {
                                if (inspectorState) {
                                    inspectorState.meshColorMode = currentIndex
                                }
                            }
                        }
                    }
                }
            }

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("Layers")

                Column {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: layerModel

                        delegate: CheckBox {
                            required property int layerIndex
                            required property string label
                            required property int layerChecked

                            width: parent.width
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
}
