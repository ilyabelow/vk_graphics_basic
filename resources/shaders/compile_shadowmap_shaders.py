import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "quad3_vert.vert", "simple_shadow.frag"]
    shader_path = os.path.dirname(os.path.realpath(__file__)) # brought to you by copilot
    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", f"{shader_path}/{shader}", "-o", f"{shader_path}/{shader}.spv"])

