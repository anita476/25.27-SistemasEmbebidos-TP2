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
        Recibe bytes crudos desde el puerto serie y devuelve a la GUI una lista de mensajes parseados.

        Formato esperado :
        - angleId: 1 byte ('R', 'C', 'O')
        - angleVal: 1-4 bytes con el valor en ASCII (ej: "-34", "0", "67", "+138")
        Sin terminador '\0' !!

        Ejemplos válidos: "R-34C0O67", "R+138", "C-072O000"

        Debe devolver: lista de mensajes. Cada mensaje es un dict con:
          - 'station_index': int (0..N-1)
          - 'angle': int en {0: roll, 1: pitch, 2: yaw}
          - 'value': float|int
        """
        logging.debug(f"[ProtocolHandler] RX chunk: {data}")
        
        messages = []
        
        # Acumular bytes en el buffer
        self.rx_buffer += data

        # estamos confiando que vienen ok los msgs, sino no va a encontrar nada
        # busco patrón angleId (R/C/O) seguido de valor numérico
        pattern = rb'([RCO])([-+]?\d{1,4})'
        matches = re.finditer(pattern, self.rx_buffer)
        
        parsed_positions = []
        for match in matches:
            try:
                angle_id = chr(match.group(1)[0])
                angle_val = int(match.group(2))
                
                messages.append({
                    'station_index': 0,  # Por defecto asumimos estación 0 si no viene en el mensaje
                    'angle': self.angle_map[angle_id],
                    'value': angle_val
                })
                logging.debug(f"[ProtocolHandler] Parsed message: angle={angle_id}, value={angle_val}")
                parsed_positions.append(match.end())
            except (ValueError, KeyError) as e:
                logging.warning(f"[ProtocolHandler] Error al parsear: {e}")
        
        if parsed_positions:
            last_end = max(parsed_positions)
            self.rx_buffer = self.rx_buffer[last_end:]
        
        return messages

    def build_led_command(self, station_index: int, r: bool, g: bool, b: bool) -> bytes:
        """
        Construye los bytes a enviar por serial para comandar LEDs de una estación.

        Formato CAN especificado en TP2: 1JKL 0RGB (en binario)
        - Bit 7: 1 (fijo)
        - Bits 6-4: JKL = número de estación (3 bits)
        - Bit 3: 0 (fijo)
        - Bits 2-0: RGB = estado de LEDs (3 bits)

        Ejemplo: station_index=2, r=True, g=False, b=True
        -> Binary: 1|010|0|101 = 10100101 = 0xA5

        Parámetros:
          - station_index: int (0..7)
          - r, g, b: bools que indican encendido de cada color

        Debe devolver:
          - bytes listos para write() del puerto serie.
        """
        logging.info(f"[ProtocolHandler] Build LED cmd -> station={station_index}, R={r}, G={g}, B={b}")
        
        # Validar rango de estación
        if not (0 <= station_index <= 7):
            logging.error(f"[ProtocolHandler] station_index fuera de rango (0-7): {station_index}")
            station_index = station_index & 0x7  # Limitar a 3 bits
        
        # Construir el byte: 1|JKL|0|RGB
        # Bit 7: 1
        # Bits 6-4: station_index (3 bits)
        # Bit 3: 0
        # Bits 2-0: RGB
        byte_value = 0x80  # 1 en bit 7
        byte_value |= (station_index & 0x7) << 4  # Colocar station_index en bits 6-4
        byte_value |= (int(r) << 2)  # R en bit 2
        byte_value |= (int(g) << 1)  # G en bit 1
        byte_value |= (int(b) << 0)  # B en bit 0
        
        logging.debug(f"[ProtocolHandler] LED byte generado: 0x{byte_value:02X} ({bin(byte_value)})")
        
        return bytes([byte_value])
