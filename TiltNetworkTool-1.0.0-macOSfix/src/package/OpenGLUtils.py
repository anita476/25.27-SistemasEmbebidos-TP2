from OpenGL.GL.shaders import compileShader
from OpenGL.GL import *

def create_shader(vertex_filepath: str, fragment_filepath: str) -> int:
    with open(vertex_filepath, 'r') as f:
        vertex_src = f.readlines()
    with open(fragment_filepath, 'r') as f:
        fragment_src = f.readlines()

    vert = compileShader(vertex_src, GL_VERTEX_SHADER)
    frag = compileShader(fragment_src, GL_FRAGMENT_SHADER)

    program = glCreateProgram()
    glAttachShader(program, vert)
    glAttachShader(program, frag)
    glLinkProgram(program)

    if not glGetProgramiv(program, GL_LINK_STATUS):
        log = glGetProgramInfoLog(program)
        raise RuntimeError(f"Shader link error: {log}")

    return program