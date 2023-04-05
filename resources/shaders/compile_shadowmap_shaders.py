import os
import subprocess
import pathlib
import sys

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"
    shader_list = ["simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", "blur.comp", "square_stack.frag"]
    shader_path = sys.path[0] # now you can launch the script from anywhere!
    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", f"{shader_path}/{shader}", "-o", f"{shader_path}/{shader}.spv"])