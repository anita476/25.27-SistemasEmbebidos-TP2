from PyQt6 import QtWidgets, QtCore
from src.protocol.protocol_handler import ProtocolHandler
from src.package.Station import STATION_COUNT, STATION_ANGLES_COUNT, STATION_ID
import math
import time

AUTOSEND_INTERVAL_MS = 200

class SimulationWidget(QtWidgets.QWidget):
    def __init__(self, protocol: ProtocolHandler, main_window, parent=None):
        super(SimulationWidget, self).__init__(parent)
        self.protocol = protocol
        self.main_window = main_window
        self.setWindowTitle("Serial Data Emulator")
        self.setGeometry(100, 100, 400, 300)

        self.message_le = QtWidgets.QLineEdit()
        self.format_cb = QtWidgets.QComboBox()
        self.format_cb.addItems(["ASCII", "Hex", "Binary", "Raw Bytes"])
        self.format_cb.currentTextChanged.connect(self.update_placeholder)
        self.send_btn = QtWidgets.QPushButton("Send", clicked=self.send_simulated_data)
        self.auto_mode_cb = QtWidgets.QCheckBox("Toggle autosend mode, bypassing protocol_handler", toggled=self.toggle_auto_mode)

        self.station_checkboxes = []
        station_layout = QtWidgets.QHBoxLayout()
        station_layout.addWidget(QtWidgets.QLabel("Stations:"))
        for i, sid in enumerate(STATION_ID):
            cb = QtWidgets.QCheckBox(sid.decode('utf-8'))
            cb.setChecked(True if i == 0 else False)  # Simula solo estación 0 por defecto
            cb.setEnabled(False) 
            self.station_checkboxes.append(cb)
            station_layout.addWidget(cb)

        self.output_te = QtWidgets.QTextEdit(readOnly=True)
        self.close_btn = QtWidgets.QPushButton("Close", clicked=self.close)

        lay = QtWidgets.QVBoxLayout(self)
        lay.addWidget(QtWidgets.QLabel("Message simulating your protocol from K64F:"))
        input_layout = QtWidgets.QHBoxLayout()
        input_layout.addWidget(self.message_le)
        input_layout.addWidget(QtWidgets.QLabel("Format:"))
        input_layout.addWidget(self.format_cb)
        lay.addLayout(input_layout)
        lay.addWidget(self.send_btn)
        lay.addWidget(self.auto_mode_cb)
        lay.addLayout(station_layout)
        lay.addWidget(QtWidgets.QLabel("Logs:"))
        lay.addWidget(self.output_te)
        lay.addWidget(self.close_btn)

        self.auto_timer = QtCore.QTimer()
        self.auto_timer.setInterval(AUTOSEND_INTERVAL_MS)
        self.auto_timer.timeout.connect(self.send_auto_data)
        self.start_time = time.time()
        self.station_count = STATION_COUNT
        self.angle_count = STATION_ANGLES_COUNT

        self.update_placeholder()

    @QtCore.pyqtSlot()
    def send_simulated_data(self):
        text = self.message_le.text().strip()
        if not text:
            return
        format_type = self.format_cb.currentText()
        try:
            if format_type == "ASCII":
                data = text.encode('utf-8')
            elif format_type == "Hex":
                data = bytes.fromhex(text.replace(' ', ''))
            elif format_type == "Binary":
                bin_values = []
                for b in text.split():
                    val = int(b, 2)
                    if val > 255 or val < 0:
                        raise ValueError(f'Binary value {b} exceeds byte range (0-255)')
                    bin_values.append(val)
                data = bytes(bin_values)
            elif format_type == "Raw Bytes":
                raw_bytes = []
                for b in text.split():
                    val = int(b)
                    if not (0 <= val <= 255):
                        raise ValueError(f'Raw byte value {b} must be 0-255')
                    raw_bytes.append(val)
                data = bytes(raw_bytes)
            else:
                raise ValueError("Unsupported format")
        except ValueError as e:
            self.output_te.append(f"Error parsing input: {e}")
            return
        
        self.output_te.append(f"Sending raw bytes (hex): {data.hex()}")
        try:
            messages = self.protocol.on_bytes(data)  # Parsea el mensaje según protocol_handler
            self.output_te.append(f"Parsed {len(messages)} messages: {messages}")
            for msg in messages:
                self.main_window.processParsedMessage(msg)  # Envia a mainwindow
        except Exception as e:
            self.output_te.append(f"Error in on_bytes: {e}")
        self.message_le.clear()

    @QtCore.pyqtSlot(bool)
    def toggle_auto_mode(self, checked):
        for cb in self.station_checkboxes:
            cb.setEnabled(checked)
        if checked:
            self.start_time = time.time()
            self.auto_timer.start()
            self.output_te.append(f"Autosend ON, sending sinusoidal data every {AUTOSEND_INTERVAL_MS} ms")
        else:
            self.auto_timer.stop()
            self.output_te.append("Autosend OFF")

    @QtCore.pyqtSlot()
    def send_auto_data(self):
        current_time = time.time() - self.start_time
        messages = []
        for station_idx in range(self.station_count):
            if not self.station_checkboxes[station_idx].isChecked():
                continue
            for angle_idx in range(self.angle_count):
                # sin(tiempo + offset) * 90
                offset = (station_idx * 0.5) + (angle_idx * 0.3)  # Offset por estación/ángulo
                value = int(math.sin(current_time + offset) * 90)
                msg = {
                    'station_index': station_idx,
                    'angle': angle_idx,
                    'value': value
                }
                messages.append(msg)

                # Bypass ProtocolHandler
                self.main_window.processParsedMessage(msg)
        self.output_te.append(f"Autosend {len(messages)} messages.")

    @QtCore.pyqtSlot(str)
    def update_placeholder(self, format_type=None):
        if format_type is None:
            format_type = self.format_cb.currentText()
        if format_type == "ASCII":
            self.message_le.setPlaceholderText("(e.g., Hello World!)")
        elif format_type == "Hex":
            self.message_le.setPlaceholderText("(e.g., 48 65 6C 6C 6F)")
        elif format_type == "Binary":
            self.message_le.setPlaceholderText("(e.g., 01001000 01100101)")
        elif format_type == "Raw Bytes":
            self.message_le.setPlaceholderText("(e.g., 72 101 108 108 111)")

    def closeEvent(self, event):
        self.auto_timer.stop()
        self.main_window.toggleSerialConnection()
        super().closeEvent(event)
