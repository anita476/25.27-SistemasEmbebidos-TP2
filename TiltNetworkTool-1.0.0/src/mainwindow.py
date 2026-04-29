import time
import logging

from PyQt6.QtWidgets import QMainWindow, QMessageBox
from PyQt6.QtCore import QTimer

import serial
from serial.tools.list_ports import comports
from serial.serialutil import SerialException

from src.ui.mainwindow import Ui_MainWindow
from src.package.Station import Station, STATION_ID, STATION_ID_NAMES, STATION_COUNT, STATION_ANGLES
from src.widgets.station_info_widget import StationInfoWidget
from src.protocol.protocol_handler import ProtocolHandler
from src.widgets.simulation_widget import SimulationWidget
from src.widgets.plot_widget import PlotWidget
from src.themes import LIGHT_THEME, DARK_THEME

IDLE_TIMER_MS = 2500  # 2.5s
RX_TIMER_MS = 10
LAST_TIME_UPDATE_MS = 1000  # 1s
SIMULATION_NAME = "⚔️🛠️⚙️Serial Data Emulator⚙️🛠️⚔️"
HISTORY_LIMIT = 50  # Puntos maximos por plot
PLOT_UPDATE_MS = 100 

class MainWindow(QMainWindow, Ui_MainWindow):
    
    def __init__(self):
        super(MainWindow, self).__init__()
        self.setupUi(self)

        self.serialConnected = False
        self.connection_but.clicked.connect(self.toggleSerialConnection)

        self.actionRefresh_ports.triggered.connect(self.updateAvailablePorts)
        self.updateAvailablePorts()
        self.serial = serial.Serial()
        self.protocol = ProtocolHandler()
        self.simulation_widget = None

        self.stationInfoWidgets = []
        self.actionFRDM_K64F.triggered.connect(self.selectFRDMModel)
        self.actionPlane.triggered.connect(self.selectPlaneModel)
        self.actionAbout.triggered.connect(self.showAbout)

        # New: Lists to store angle histories (timestamps and values for roll, pitch, yaw per station)
        self.angle_histories = [[[], [], []] for _ in range(STATION_COUNT)]  # [station][angle][(timestamp, value)]
        self.plot_widgets = [None] * STATION_COUNT  # To hold plot widget instances
        self.actionToggle_theme.triggered.connect(self.toggleTheme)
        self.current_theme = 'light'

        for i in range(len(STATION_ID)):
            siw = StationInfoWidget(self)
            self.stationInfoLayout.addWidget(siw)
            self.stationInfoWidgets.append(siw)
            # Connect the plot button signal to a slot
            siw.plotButton.clicked.connect(self.togglePlotForStation(i))
        self.timers = []

        self.last_update_times = [None] * STATION_COUNT

        # this section builds the network viewer for each station
        for i, siw in enumerate(self.stationInfoWidgets):
            siw.setEnabled(False)
            siw.setName('Station {}'.format(STATION_ID_NAMES[i])) 
            timer = QTimer()
            timer.setInterval(IDLE_TIMER_MS)
            timer.setSingleShot(True)
            timer.timeout.connect(self.buildStationTimeout(i))
            timer.start()
            self.timers.append(timer)

        self.stations = []
        for x in range(STATION_COUNT):
            self.stations.append(Station(id=STATION_ID[x]))

        # checks RX buffer every RX_TIMER_MS ms
        timer = QTimer()
        timer.setInterval(RX_TIMER_MS)
        timer.setSingleShot(False)
        timer.timeout.connect(self.receive)
        timer.start()
        self.rxTimer = timer

        # checks last update times every LAST_TIME_UPDATE_MS ms
        update_timer = QTimer()
        update_timer.setInterval(LAST_TIME_UPDATE_MS)
        update_timer.setSingleShot(False)
        update_timer.timeout.connect(self.updateLastUpdateLabels)
        update_timer.start()
        self.lastUpdateTimer = update_timer

        # New: Timer para actualizar plots periódicamente
        plot_update_timer = QTimer()
        plot_update_timer.setInterval(PLOT_UPDATE_MS)
        plot_update_timer.setSingleShot(False)
        plot_update_timer.timeout.connect(self.updateOpenPlots)
        plot_update_timer.start()
        self.plotUpdateTimer = plot_update_timer

        self.stationSelector_cb.addItems(STATION_ID_NAMES)
        self.send_pb.clicked.connect(self.sendLEDCommand)
        self.LED_gb.setEnabled(False)
        

    def buildStationTimeout(self, stationIndex):
        return lambda : self.disableStationInfoWidget(stationIndex)

    def disableStationInfoWidget(self, stationIndex):
        self.stationInfoWidgets[stationIndex].setEnabled(False)
        self.oglw.setStationInactive(stationIndex)

    def togglePlotForStation(self, station_index):
        def toggle():
            if self.plot_widgets[station_index] is None:
                self.plot_widgets[station_index] = PlotWidget(station_index, self.angle_histories[station_index])
                self.plot_widgets[station_index].show()
            else:
                self.plot_widgets[station_index].close()
                self.plot_widgets[station_index] = None
        return toggle

    def updateOpenPlots(self):
        for i in range(STATION_COUNT):
            if self.plot_widgets[i]:
                self.plot_widgets[i].updatePlot(self.angle_histories[i])

    def processParsedMessage(self, msg: dict):
        """
        msg dict esperado (lo produce ProtocolHandler.on_bytes):
          - station_index: int
          - angle: int en {0: roll, 1: pitch, 2: yaw}
          - value: float/int
        """
        try:
            station_index = int(msg.get('station_index'))
            angle_id = msg.get('angle')
            value = msg.get('value')
        except Exception as e:
            logging.warning(f"[MainWindow] Mensaje inválido (faltan campos): {msg} ({e})")
            return

        angle_index = self._resolve_angle_index(angle_id)
        if angle_index not in (0, 1, 2):
            logging.warning(f"[MainWindow] 'angle' no reconocido: {angle_id}")
            return

        if station_index < 0 or station_index >= STATION_COUNT:
            logging.warning(f"[MainWindow] 'station_index' fuera de rango: {station_index}") 
            return

        if self.stations[station_index].assignAngle(angle_index, value):
            current_time = time.time()
            self.angle_histories[station_index][angle_index].append((current_time, value))

            if len(self.angle_histories[station_index][angle_index]) > HISTORY_LIMIT:
                self.angle_histories[station_index][angle_index].pop(0)

            self.last_update_times[station_index] = time.time()
            self.timers[station_index].start()
            self.stationInfoWidgets[station_index].setEnabled(True)
            self.stationInfoWidgets[station_index].setAngleLabels(self.stations[station_index].angles)
            self.oglw.setOrientation(
                station_index,
                -self.stations[station_index].roll,
                -self.stations[station_index].pitch,
                +self.stations[station_index].yaw + 90
            )

    def _resolve_angle_index(self, angle_id):
        if isinstance(angle_id, int) and angle_id in (0, 1, 2):
            return angle_id
        return -1

    def receive(self):
        if not self.serialConnected or not self.serial.is_open:
            return
        try:
            while self.serial.in_waiting > 0:
                to_read = self.serial.in_waiting or 1
                chunk = self.serial.read(to_read)
                try:
                    messages = self.protocol.on_bytes(chunk)
                except NotImplementedError as e:
                    logging.warning(f"[MainWindow] ProtocolHandler.on_bytes no implementado aún: {e}")
                    break

                if not messages:
                    continue
                for msg in messages:
                    self.processParsedMessage(msg)
        except (SerialException, OSError) as e:
            logging.error(f"[MainWindow] Error reading from serial port: {e}")
    
    def toggleSerialConnection(self):
        if not self.serialConnected:
            self.port = self.getPort()
            port = self.port
            if port == SIMULATION_NAME:
                self.serialConnected = True
                # Se le pasa self (mainwindow) para poder llamar a processParsedMessage() bypasseando on_bytes.
                self.simulation_widget = SimulationWidget(self.protocol, self)
                self.simulation_widget.show()
            elif port:
                try:
                    self.serial.baudrate = int(self.baudrate_cb.currentText())
                    self.serial.port = port
                    self.serial.open()
                    if self.serial.is_open:
                        self.serialConnected = True
                        logging.info(f"[MainWindow] Connected to serial port: {port}")
                    else:
                        raise SerialException("Failed to open port")
                except (SerialException, ValueError, OSError) as e:
                    logging.error(f"[MainWindow] Failed to connect to serial port {port}: {e}")
                    QMessageBox.warning(self, "Connection Error", f"Could not connect to port {port}.\nError: {e}")
                    self.serialConnected = False
            else:
                QMessageBox.warning(self, "No Port Selected", "Please select a valid serial port.")
                self.serialConnected = False
        else:
            try:
                if self.simulation_widget:
                    self.simulation_widget.close()
                    self.simulation_widget = None
                if self.serial.is_open:
                    self.serial.close()
                    logging.info("[MainWindow] Disconnected from serial port")
            except SerialException as e:
                logging.warning(f"[MainWindow] Error closing serial port: {e}")
            self.serialConnected = False
            self.angle_histories = [[[], [], []] for _ in range(STATION_COUNT)]
        self.configPortSettings(self.serialConnected)
    
    def configPortSettings(self, connected=False):
        self.COM_gb.setEnabled(not connected)
        self.LED_gb.setEnabled(connected)
        if(connected):
            self.connection_but.setText('Disconnect')
        else:
            self.connection_but.setText('Connect')

    def updateAvailablePorts(self):
        self.port_cb.clear()
        for port, desc, hwid in comports():
            self.port_cb.addItem(f"{port} - {desc}")
        self.port_cb.addItem(SIMULATION_NAME)

    def getPort(self):
        text = self.port_cb.currentText()
        if text:
            port = text.split(" - ")[0]
            return port
        return None

    def selectFRDMModel(self):
        if(self.actionFRDM_K64F.isChecked()):
            self.actionPlane.setChecked(False)
            self.oglw.setModelIndex(0)
        else:
            self.actionPlane.setChecked(True)
            self.selectPlaneModel()

    def selectPlaneModel(self):
        if(self.actionPlane.isChecked()):
            self.actionFRDM_K64F.setChecked(False)
            self.oglw.setModelIndex(1)
        else:
            self.actionFRDM_K64F.setChecked(True)
            self.selectFRDMModel()

    def sendLEDCommand(self):
        if (self.port != SIMULATION_NAME) and (not self.serialConnected or not self.serial.is_open):
            QMessageBox.warning(self, "Not Connected", "Please connect to a serial port first.")
            return
        
        self.selected_station_index = STATION_ID_NAMES.index(self.stationSelector_cb.currentText())
        self.selected_r = self.r_checkb.isChecked()
        self.selected_g = self.g_checkb.isChecked()
        self.selected_b = self.b_checkb.isChecked()
        
        try:
            message = self.protocol.build_led_command(
                self.selected_station_index,
                self.selected_r,
                self.selected_g,
                self.selected_b
            )
        except NotImplementedError as e:
            logging.warning(f"[MainWindow] ProtocolHandler.build_led_command no implementado aún: {e}")
            return
        except Exception as e:
            logging.error(f"[MainWindow] Error construyendo LED cmd: {e}")
            QMessageBox.warning(self, "Command Build Error", f"Failed to build LED command: {e}")
            return

        if not message:
            logging.warning("[MainWindow] build_led_command devolvió vacío/None, no se envía nada.")
            QMessageBox.information(self, "No Command", "No command to send.")
            return

        try:
            self.serial.write(message)
            logging.debug(f"[MainWindow] Enviado {len(message)} bytes: {message}")
        except (SerialException, OSError) as e:
            logging.error(f"[MainWindow] Error enviando por serial: {e}")
            QMessageBox.warning(self, "Send Error", f"Failed to send command: {e}")
    
    def showAbout(self):
        about_text = """
        <h2>Tilt Network Tool</h2>
        <p>Tool for monitoring a tilt sensor network.</p>
        <p>This is a support tool for the course 25.27 - Embedded Systems at ITBA (Electronic Engineering degree), for the serial communication practical work, where multiple FRDM-K64F-based stations report their tilt using their accelerometers, through a shared CAN bus.</p>
        <p><b>Credits:</b></p>
        <ul>
            <li>Eng. Juan Francisco Sbruzzi (original TiltNetworkTool, 🏆🏆🏆)</li>
            <li>Alejandro Nahuel Heir (adaptation, current dev)</li>
        </ul>
        <p><b>Source:</b> <a href="https://github.com/alheir/TiltNetworkTool" style="color:green;">github.com/alheir/TiltNetworkTool</a></p>
        """
        QMessageBox.about(self, "About", about_text)

    def updateLastUpdateLabels(self):
        current_time = time.time()
        for i, last_time in enumerate(self.last_update_times):
            if last_time is not None:
                seconds_ago = current_time - last_time
                self.stationInfoWidgets[i].setLastUpdateTime(seconds_ago)
            else:
                self.stationInfoWidgets[i].setLastUpdateTime(None)

    def closeEvent(self, event):
        if self.simulation_widget:
            self.simulation_widget.close()
        try:
            if self.serial.is_open:
                self.serial.close()
        except SerialException as e:
            logging.warning(f"[MainWindow] Error closing serial on exit: {e}")
        event.accept()

    def setTheme(self, theme):
        self.current_theme = theme
        self.oglw.setTheme(theme)

    def toggleTheme(self):
        if self.current_theme == 'light':
            self.app.setStyleSheet(DARK_THEME)
            self.current_theme = 'dark'
        else:
            self.app.setStyleSheet(LIGHT_THEME)
            self.current_theme = 'light'
        self.setTheme(self.current_theme)
