from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import *
import numpy as np
import pyrr

from src.package.Mesh import Mesh
from src.package.Entity import Entity
from src.package.Material import Material
from src.package.Station import STATION_COUNT        
from src.package.OpenGLUtils import create_shader


class FrdmViewerWidget(QOpenGLWidget):
    """
        For now, the app will be handling everything.
        Later on we'll break it into subcomponents.
    """

    def __init__(self, parent=None):
        super().__init__()
        self.stations = []
        self.stations_mesh = []
        self.stations_active = [False]*STATION_COUNT
        self.modelIndex = 0
        self.theme = 'light'

    def initializeGL(self) -> None:
        """
            Configure any desired OpenGL options
        """
        for x in range(STATION_COUNT):
            self.stations.append(Entity(
                position = [-STATION_COUNT*100/2+100*x,0,-100],
                eulers = [0,0,90]
            ))
            self.stations_mesh.append([
                Mesh("src/models/frdm.obj"),
                Mesh("src/models/plane.obj")
            ])
            
        self.enabled_texture = [
            Material("src/models/frdm_en.png"),
            Material("src/models/plane_en.png")
        ]

        self.disabled_texture = [
            Material("src/models/frdm_dis.png"),
            Material("src/models/plane_dis.png")
        ]

        self.shader = create_shader(
            vertex_filepath = "src/shaders/vertex.txt", 
            fragment_filepath = "src/shaders/fragment.txt"
        )

        glUseProgram(self.shader)
        glUniform1i(glGetUniformLocation(self.shader, "imageTexture"), 0)

        projection_transform = pyrr.matrix44.create_orthogonal_projection(
            left=-STATION_COUNT*100/2 - 50,right=STATION_COUNT*100/2 - 50,
            top=100, bottom=-100,
            near=1, far=1000, dtype=np.float32
        )
        glUniformMatrix4fv(
            glGetUniformLocation(self.shader,"projection"),
            1, GL_FALSE, projection_transform
        )
        glUseProgram(self.shader)
        self.modelMatrixLocation = glGetUniformLocation(self.shader,"model")
        glEnable(GL_TEXTURE_2D)
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_CULL_FACE)
        glCullFace(GL_BACK)
        

    def paintGL(self):
        for x in range(STATION_COUNT):
            self.stations[x].update()
        
        if self.theme == 'dark':
            glClearColor(0.2, 0.2, 0.2, 1.0)
        else:
            glClearColor(0.941, 0.941, 0.941, 1.0)
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glUseProgram(self.shader)

        for x in range(STATION_COUNT):
            if(self.stations_active[x]):
                self.enabled_texture[self.modelIndex].use()
            else:
                self.disabled_texture[self.modelIndex].use()
            glUniformMatrix4fv(
                self.modelMatrixLocation,1,GL_FALSE,
                self.stations[x].get_model_transform())
            self.stations_mesh[x][self.modelIndex].arm_for_drawing()
            self.stations_mesh[x][self.modelIndex].draw()

    def setOrientation(self, index, x,y,z):
        self.stations[index].eulers = [x,y,z]
        self.stations_active[index] = True
        self.update()

    def setStationInactive(self, index):
        self.stations_active[index] = False
        self.update()
    
    def setModelIndex(self, index):
        self.modelIndex = index
        self.update()
        
    def setTheme(self, theme):
        self.theme = theme
        self.update()  # Solo actualiza, no llames glClearColor

    def quit(self) -> None:
        """ cleanup the app, run exit code """

        for x in range(STATION_COUNT):
            self.stations_mesh[x].destroy()
        self.enabled_texture.destroy()
        self.disabled_texture.destroy()
        glDeleteProgram(self.shader)
