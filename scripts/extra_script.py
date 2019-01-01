from os.path import join, isfile, basename
from os import listdir, system
import json
from pprint import pprint
import re
import requests
import subprocess
import sys

Import("env")

# Install pre-requisites
npm_installed = (0 == system("npm --version"))

#
# Dump build environment (for debug)
# print env.Dump()
#print("Current build targets", map(str, BUILD_TARGETS))
#

def get_c_name(source_file):
    return basename(source_file).upper().replace('.', '_').replace('-', '_')

def text_to_header(source_file):
    with open(source_file) as source_fh:
        original = source_fh.read().decode('utf-8')
    filename = get_c_name(source_file)
    output = "static const char CONTENT_{}[] PROGMEM = ".format(filename)
    for line in original.splitlines():
        output += u"\n  \"{}\\n\"".format(line.replace('\\', '\\\\').replace('"', '\\"'))
    output += ";\n"
    return output

def binary_to_header(source_file):
    filename = get_c_name(source_file)
    output = "static const char CONTENT_"+filename+"[] PROGMEM = {\n  "
    count = 0

    with open(source_file, "rb") as source_fh:
        byte = source_fh.read(1)
        while byte != "":
            output += "0x{:02x}, ".format(ord(byte))
            count += 1
            if 16 == count:
                output += "\n  "
                count = 0

            byte = source_fh.read(1)

    output += "0x00 };\n"
    return output

def data_to_header(env, target, source):
    output = ""
    for source_file in source:
        #print("Reading {}".format(source_file))
        file = source_file.get_abspath()
        if file.endswith(".css") or file.endswith(".js") or file.endswith(".htm") or file.endswith(".html"):
            output += text_to_header(file)
        else:
            output += binary_to_header(file)
    target_file = target[0].get_abspath()
    print("Generating {}".format(target_file))
    with open(target_file, "w") as output_file:
        output_file.write(output.encode('utf-8'))

def make_static(env, target, source):
    output = ""

    out_files = []
    for file in listdir(data_src):
        if isfile(join(data_src, file)):
          out_files.append(file)

    # Sort files to make sure the order is constant
    out_files = sorted(out_files)

    # include the files
    for out_file in out_files:
        filename = "web_server."+out_file+".h"
        output += "#include \"{}\"\n".format(filename)

    output += "StaticFile staticFiles[] = {\n"

    for out_file in out_files:
        filetype = "TEXT"
        if out_file.endswith(".css"):
            filetype = "CSS"
        elif out_file.endswith(".js"):
            filetype = "JS"
        elif out_file.endswith(".htm") or out_file.endswith(".html"):
            filetype = "HTML"
        elif out_file.endswith(".jpg"):
            filetype = "JPEG"
        elif out_file.endswith(".png"):
            filetype = "PNG"

        c_name = get_c_name(out_file)
        output += "  { \"/"+out_file+"\", CONTENT_"+c_name+", sizeof(CONTENT_"+c_name+") - 1, _CONTENT_TYPE_"+filetype+" },\n"

    output += "};\n"

    target_file = target[0].get_abspath()
    print("Generating {}".format(target_file))
    with open(target_file, "w") as output_file:
        output_file.write(output.encode('utf-8'))

def process_html_app(source, dest, env):
    web_server_static_files = join(dest, "web_server_static_files.h")
    web_server_static = join("$BUILDSRC_DIR", "web_server_static.cpp.o")

    for file in sorted(listdir(source)):
        if isfile(join(source, file)):
            data_file = join(source, file)
            header_file = join(dest, "web_server."+file+".h")
            env.Command(header_file, data_file, data_to_header)
            env.Depends(web_server_static_files, header_file)

    env.Depends(web_server_static, env.Command(web_server_static_files, source, make_static))

#
# Generate Web app resources
#
headers_src = join(env.subst("$PROJECTSRC_DIR"), "web_static")
data_src = join(env.subst("$PROJECT_DIR"), "gui", "dist")
process_html_app(data_src, headers_src, env)
