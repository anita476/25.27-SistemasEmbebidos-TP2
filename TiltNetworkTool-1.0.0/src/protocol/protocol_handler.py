from typing import List, Dict, Any
import logging

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
        logging.info("[ProtocolHandler] Inicializado. Listo para recibir bytes del puerto serie.")

    def on_bytes(self, data: bytes) -> List[Dict[str, Any]]:
        """
        Recibe bytes crudos desde el puerto serie y devuelve a la GUI una lista de mensajes parseados.

        Debe devolver: lista de mensajes. Cada mensaje es un dict con:
          - 'station_index': int (0..N-1)
          - 'angle': int en {0: roll, 1: pitch, 2: yaw}
          - 'value': float|int

        Ejemplos de retorno:
          # Un solo mensaje:
          # return [{'station_index': 0, 'angle': 0, 'value': 12.5}]  # roll de grupo 0
          # Varios mensajes:
          # return [
          #   {'station_index': 1, 'angle': 1, 'value': 20.0},  # pitch de grupo 1
          #   {'station_index': 1, 'angle': 2, 'value': -7.2},  # yaw de grupo 1
          # ]
          # Lista vacía si no hay frames completos:
          # return []
        """
        logging.debug(f"[ProtocolHandler] RX chunk: {data}")

        #
        #
        #

        raise NotImplementedError("Implementar on_bytes(data) con framing y parseo de su protocolo.")

    def build_led_command(self, station_index: int, r: bool, g: bool, b: bool) -> bytes:
        """
        Construye los bytes a enviar por serial para comandar LEDs de una estación.

        Parámetros:
          - station_index: int (0..N-1)
          - r, g, b: bools que indican encendido de cada color

        Debe devolver:
          - bytes listos para write() del puerto serie.
        """
        logging.info(f"[ProtocolHandler] Build LED cmd -> station={station_index}, R={r}, G={g}, B={b}")

        #
        #
        #
        
        # return b'Prender LED rojo del grupo 5'

        raise NotImplementedError("Implementar build_led_command(...) para su protocolo.")
