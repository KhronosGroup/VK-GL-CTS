# -*- coding: utf-8 -*-

from src_util import *

def genExtensions (registry, iface, api):
    for extName in EXTENSIONS:
        extIface = getInterface(registry, api, version=False, profile='core', extensionNames=[extName])
        if not extIface.commands:
            continue

        yield ""
        yield "if (de::contains(extSet, \"%s\"))" % extName
        yield "{"

        def genInit (command):
            coreName = getCoreName(command.name)
            ifaceName = coreName if coreName in iface.commands else command.name
            return "gl->%s\t= (%s)\tloader->get(\"%s\");" % (
                getFunctionMemberName(ifaceName),
                getFunctionTypeName(ifaceName),
                command.name)

        for line in indentLines(genInit(command) for command in extIface.commands):
            yield "\t" + line

        yield "}"

def genExtInit (registry, iface):
    writeInlFile(os.path.join(OPENGL_INC_DIR, "glwInitExtES.inl"), genExtensions(registry, iface, 'gles2'))
    writeInlFile(os.path.join(OPENGL_INC_DIR, "glwInitExtGL.inl"), genExtensions(registry, iface, 'gl'))

if __name__ == '__main__':
    genExtInit(getGLRegistry(), getHybridInterface())
