from PyQt6 import QtCore, QtGui, QtWidgets

class StationInfoWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        grid = QtWidgets.QGridLayout()
        self.groupBox = QtWidgets.QGroupBox(self)
        self.groupBox.setLayout(QtWidgets.QGridLayout())


        labelRoll = QtWidgets.QLabel(text="Roll")
        labelPitch = QtWidgets.QLabel(text="Pitch")
        labelYaw = QtWidgets.QLabel(text="Yaw")
        self.groupBox.layout().addWidget(labelRoll, 0, 0)
        self.groupBox.layout().addWidget(labelPitch, 1, 0)
        self.groupBox.layout().addWidget(labelYaw, 2, 0)
        
        self.labelRollValue = QtWidgets.QLabel(text="")
        self.labelPitchValue = QtWidgets.QLabel(text="")
        self.labelYawValue = QtWidgets.QLabel(text="")
        self.groupBox.layout().addWidget(self.labelRollValue, 0, 1)
        self.groupBox.layout().addWidget(self.labelPitchValue, 1, 1)
        self.groupBox.layout().addWidget(self.labelYawValue, 2, 1)
        
        self.labelLastUpdate = QtWidgets.QLabel(text="Last Update: N/A")
        self.groupBox.layout().addWidget(self.labelLastUpdate, 3, 0, 1, 2)
        
        self.plotButton = QtWidgets.QPushButton("Plot")
        self.groupBox.layout().addWidget(self.plotButton, 4, 0, 1, 2)
        
        grid.addWidget(self.groupBox,0,0)
        self.setLayout(grid)

    def setName(self, name):
        self.groupBox.setTitle(name)

    def setAngleLabels(self, roll, pitch, yaw):
        self.labelRollValue.setText("{}°".format(int(round(roll))))
        self.labelPitchValue.setText("{}°".format(int(round(pitch))))
        self.labelYawValue.setText("{}°".format(int(round(yaw))))

    def setAngleLabels(self, angles):
        self.labelRollValue.setText("{}°".format(int(round(angles[0]))))
        self.labelPitchValue.setText("{}°".format(int(round(angles[1]))))
        self.labelYawValue.setText("{}°".format(int(round(angles[2]))))

    def setEnabled(self, enabled=True):
        self.groupBox.setEnabled(enabled)

    def setLastUpdateTime(self, seconds_ago):
        if seconds_ago is None:
            self.labelLastUpdate.setText("Last Update: N/A")
        elif seconds_ago < 60:
            self.labelLastUpdate.setText(f"Last Update: {int(seconds_ago)} s")
        else:
            minutes = int(seconds_ago // 60)
            self.labelLastUpdate.setText(f"Last Update: {minutes} m")

