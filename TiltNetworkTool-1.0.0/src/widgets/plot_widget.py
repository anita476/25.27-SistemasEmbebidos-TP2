import pyqtgraph as pg
from PyQt6.QtWidgets import QWidget, QVBoxLayout
import PyQt6.QtCore as QtCore
import numpy as np
import time

class PlotWidget(QWidget):
    def __init__(self, station_index, angle_histories):
        super().__init__()
        self.station_index = station_index
        self.angle_histories = angle_histories  # [roll_history, pitch_history, yaw_history], cada una con [(timestamp, value), ...]
        self.start_time = time.time()
        self.setWindowTitle(f"Station {station_index} Angle Plot")
        self.setGeometry(100, 100, 600, 300)

        pg.setConfigOption('background', 'w')  # Fondo blanco
        pg.setConfigOption('foreground', 'k')  # Texto negro
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setLabel('left', 'Angle (°)')
        self.plot_widget.setLabel('bottom', 'Time (s)')
        self.plot_widget.setTitle(f'Station {station_index} Angles Over Time')
        self.plot_widget.addLegend()
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)

        self.curves = []
        colors = ['r', 'g', 'b']
        labels = ['Roll', 'Pitch', 'Yaw']
        for color, label in zip(colors, labels):
            curve = self.plot_widget.plot(
                pen=pg.mkPen(color=color, width=1, style=QtCore.Qt.PenStyle.DashLine),
                symbol='o',
                symbolSize=6,
                symbolPen=pg.mkPen(color=color),
                symbolBrush=pg.mkBrush(color=color),
                name=label
            )
            self.curves.append(curve)

        layout = QVBoxLayout()
        layout.addWidget(self.plot_widget)
        self.setLayout(layout)

        self.updatePlot()

    def updatePlot(self, angle_histories=None):
        if angle_histories:
            self.angle_histories = angle_histories
        for i, (history, curve) in enumerate(zip(self.angle_histories, self.curves)):
            if history:
                times, values = zip(*history)
                relative_times = np.array(times) - self.start_time # Para empezar en t=0
                curve.setData(x=relative_times, y=np.array(values))
            else:
                curve.setData(x=[], y=[])
