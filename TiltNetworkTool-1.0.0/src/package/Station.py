
STATION_ID = [b'0', b'1', b'2', b'3', b'4', b'5', b'6']
STATION_ID_NAMES = ['0x100', '0x101', '0x102', '0x103', '0x104', '0x105', '0x106']
STATION_ANGLES = [b'R', b'C', b'O']
STATION_ANGLES_COUNT = STATION_ANGLES.__len__()
STATION_COUNT = STATION_ID.__len__()

class Station:
    def __init__(self, id, roll=0, pitch=0, yaw=0):
        self.id = id
        self.roll = roll
        self.pitch = pitch
        self.yaw = yaw
        self.angles = [self.roll, self.pitch, self.yaw]

    def resetAngles(self):
        self.roll = 0
        self.pitch = 0
        self.yaw = 0

    def assignAngle(self, angleIdentifier, value):
        try:
            if(isinstance(angleIdentifier, bytes)):
                angleIndex = STATION_ANGLES.index(angleIdentifier)
            else:
                angleIndex = angleIdentifier
            self.angles[angleIndex] = value
            self.roll = self.angles[0]
            self.pitch = self.angles[1]
            self.yaw = self.angles[2]
            return True

        except ValueError:
            print("Angle not recognized")

        return False