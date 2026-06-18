import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f6f6"

    function distanceToSlider(distance) {
        return Math.log(Math.max(4, distance)) / Math.LN10
    }

    function sliderToDistance(sliderValue) {
        return Math.pow(10, sliderValue)
    }

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
                title: qsTr("Camera")

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Compass")
                            color: "#202020"
                        }

                        Slider {
                            id: cameraFacingSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 359
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraFacingDegrees : 0
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraFacingDegrees = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°").arg(Math.round(cameraFacingSlider.value))
                            color: "#202020"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Tilt")
                            color: "#202020"
                        }

                        Slider {
                            id: cameraTiltSlider
                            Layout.fillWidth: true
                            from: -90
                            to: 90
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraTiltDegrees : 0
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraTiltDegrees = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°").arg(Math.round(cameraTiltSlider.value))
                            color: "#202020"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Distance")
                            color: "#202020"
                        }

                        Slider {
                            id: cameraDistanceSlider
                            Layout.fillWidth: true
                            from: root.distanceToSlider(4)
                            to: root.distanceToSlider(inspectorState ? Math.max(500, inspectorState.cameraDistanceMeters * 2) : 500)
                            stepSize: 0.001
                            live: true
                            value: root.distanceToSlider(inspectorState ? inspectorState.cameraDistanceMeters : 120)
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraDistanceMeters = root.sliderToDistance(value)
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1 m").arg(Math.round(root.sliderToDistance(cameraDistanceSlider.value)))
                            color: "#202020"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        opacity: inspectorState && inspectorState.orthographicProjection ? 0.45 : 1.0

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Focal Length")
                            color: "#202020"
                        }

                        Slider {
                            id: cameraFocalLengthSlider
                            Layout.fillWidth: true
                            enabled: inspectorState ? !inspectorState.orthographicProjection : true
                            from: 10
                            to: 80
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraFocalLengthMm : 35
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraFocalLengthMm = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            enabled: cameraFocalLengthSlider.enabled
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1 mm").arg(Math.round(cameraFocalLengthSlider.value))
                            color: "#202020"
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 120
                        visible: inspectorState ? inspectorState.orthographicProjection : false
                        text: qsTr("Focal length is disabled in orthographic projection.")
                        wrapMode: Text.WordWrap
                        color: "#606060"
                        font.pointSize: Math.max(8, Qt.application.font.pointSize - 1)
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

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("Scene Settings")

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Model Coloring")
                            color: "#202020"
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Altitude"), qsTr("None")]
                            currentIndex: inspectorState ? inspectorState.meshColorMode : 0
                            onActivated: {
                                if (inspectorState) {
                                    inspectorState.meshColorMode = currentIndex
                                }
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Bounding Box")
                        checked: inspectorState ? inspectorState.showBoundingBox : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showBoundingBox = checked
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Head-Up Display")
                        checked: inspectorState ? inspectorState.showHud : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showHud = checked
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Title & Stats")
                        checked: inspectorState ? inspectorState.showInfo : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showInfo = checked
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Rotation Speed")
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
        }
    }
}
