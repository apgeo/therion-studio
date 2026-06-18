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

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Rotation speed")
                            color: "#202020"
                        }

                        Slider {
                            id: rotationSpeedSlider
                            Layout.fillWidth: true
                            from: 5
                            to: 90
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.autoRotationSpeed : 30
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.autoRotationSpeed = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°/s").arg(Math.round(rotationSpeedSlider.value))
                            color: "#202020"
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
                            required property int rowIndex
                            required property int indent
                            required property bool hasChildren
                            required property string label
                            required property int layerChecked

                            x: indent * 34
                            width: parent.width - x
                            tristate: hasChildren
                            checkState: layerChecked
                            text: label
                            nextCheckState: function() {
                                return checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked
                            }
                            onClicked: {
                                if (layerModel) {
                                    layerModel.setLayerVisible(rowIndex, checkState === Qt.Checked)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
