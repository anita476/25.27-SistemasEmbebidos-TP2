from typing import List, Dict, Any
import logging
import re

class ProtocolHandler:
    """
    Clase intermedia para manejar el protocolo de comunicación serial.

    Qué se debe implementar:
      - Framing y parseo en on_bytes(): acumular bytes, detectar fin de mensaje,
        validar y convertir a una estructura uniforme para la GUI según lo especificado.
      - Construcción de mensajes salientes en build_led_command().
    """
    def __init__(self) -> None:
        # Buffer/s, constantes, ...
        self.rx_buffer = b""  # Buffer para acumular bytes del puerto serie
        self.angle_map = {'R': 0, 'C': 1, 'O': 2}  # Mapeo de caracteres a índices de ángulo
        logging.info("[ProtocolHandler] Inicializado. Listo para recibir bytes del puerto serie.")

    def on_bytes(self, data: bytes) -> List[Dict[str, Any]]:
        """

        Angle frame  : "107R-34\r\n"    3 hex CAN ID + angleId + ASCII value + \r\n
        LED forward  : 'L' <byte> "\r\n"  — another group's LED cmd, log only

        Returns list of dicts: {station_index, angle, value}
        """
        logging.debug(f"[ProtocolHandler] RX chunk: {data}")

        self.rx_buffer += data
        messages = []

        while True:
            crlf = self.rx_buffer.find(b'\r\n')
            if crlf == -1:
                break  # Wait for more bytes

            frame = self.rx_buffer[:crlf]
            self.rx_buffer = self.rx_buffer[crlf + 2:]

            if len(frame) == 0:
                continue  # Empty line, skip

            # forwarded LED frame
            if frame[0:1] == b'L' and len(frame) == 2:
                led_byte = frame[1]
                station_idx = (led_byte >> 4) & 0x07
                logging.info(
                    f"[ProtocolHandler] Forwarded LED cmd for station {station_idx}: "
                    f"0x{led_byte:02X}  (not for us — discarding)"
                )
                continue  # No message appended

            # angle frame: 3 hex digits + R/C/O +  value
            if len(frame) < 5:
                logging.debug(f"[ProtocolHandler] Frame too short, discarding: {frame!r}")
                continue

            try:
                # First 3 bytes are the CAN ID in hex ASCII (e.g. b'107')
                station_id = int(frame[0:3], 16)
            except ValueError:
                logging.debug(f"[ProtocolHandler] Bad CAN ID field, discarding: {frame!r}")
                continue

            if not (0x100 <= station_id <= 0x107):
                logging.debug(f"[ProtocolHandler] CAN ID out of range: 0x{station_id:X}")
                continue

            angle_char = chr(frame[3])
            if angle_char not in self.angle_map:
                logging.debug(f"[ProtocolHandler] Unknown angleId '{angle_char}', discarding: {frame!r}")
                continue

            value_bytes = frame[4:]
            if len(value_bytes) == 0 or len(value_bytes) > 4:
                logging.debug(f"[ProtocolHandler] Value field bad length: {frame!r}")
                continue

            try:
                angle_val = int(value_bytes)
            except ValueError:
                logging.debug(f"[ProtocolHandler] Bad value field, discarding: {frame!r}")
                continue

            station_index = station_id - 0x100
            msg = {
                'station_index': station_index,
                'angle': self.angle_map[angle_char],
                'value': angle_val,
            }
            messages.append(msg)
            logging.debug(
                f"[ProtocolHandler] Parsed: station=0x{station_id:X} "
                f"angle={angle_char} value={angle_val}"
            )

        return messages


    def build_led_command(self, station_index: int, r: bool, g: bool, b: bool) -> bytes:
        """
        TX protocol : 1JKL 0RGB
        R = (led_byte >> 2) & 1
        G = (led_byte >> 1) & 1
        B = (led_byte >> 0) & 1
        """
        logging.info(
            f"[ProtocolHandler] Build LED cmd -> station={station_index}, "
            f"R={r}, G={g}, B={b}"
        )

        if not (0 <= station_index <= 7):
            logging.warning(
                f"[ProtocolHandler] station_index {station_index} out of range, "
                f"clamping to 3 bits"
            )
            station_index = station_index & 0x07

        byte_value = (
            0x80                        # bit 7 = 1
            | (station_index & 0x07) << 4  # bits 6-4 = JKL
            # bit 3 = 0
            | (int(r) << 2)             # bit 2 = R
            | (int(g) << 1)             # bit 1 = G
            | (int(b) << 0)             # bit 0 = B
        )
        logging.debug(
            f"[ProtocolHandler] LED byte: 0x{byte_value:02X}  ({byte_value:08b})"
        )
        return bytes([byte_value])
