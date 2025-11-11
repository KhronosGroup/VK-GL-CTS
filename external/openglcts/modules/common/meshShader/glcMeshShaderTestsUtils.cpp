/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 AMD Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief Mesh shader tests utility classes
 */ /*--------------------------------------------------------------------*/

#include "glcMeshShaderTestsUtils.hpp"
#include "glw.h"
#include <iostream>

namespace glc
{
namespace meshShader
{

using namespace glw;

ExtFunctions::ExtFunctions(glu::RenderContext &renderContext) : m_renderContext(renderContext)
{
    Init();
}

static bool checkCompileErrors(GLuint shader, std::string type)
{
    GLint success = false;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    return success;
}

GLuint createProgram(const char *taskStr, const char *meshStr, const char *fragStr)
{
    GLuint taskShader     = 0;
    GLuint meshShader     = 0;
    GLuint fragmentShader = 0;
    GLuint program        = 0;

    if (taskStr != nullptr)
    {
        taskShader = glCreateShader(GL_TASK_SHADER_EXT);
        glShaderSource(taskShader, 1, &taskStr, nullptr);
        glCompileShader(taskShader);
        checkCompileErrors(taskShader, "TASK_SHADER");
    }
    meshShader = glCreateShader(GL_MESH_SHADER_EXT);
    glShaderSource(meshShader, 1, &meshStr, nullptr);
    glCompileShader(meshShader);
    checkCompileErrors(meshShader, "MESH_SHADER");

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragStr, nullptr);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT_SHADER");

    program = glCreateProgram();
    if (taskStr != nullptr)
    {
        glAttachShader(program, taskShader);
    }

    glAttachShader(program, meshShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    if (taskStr != nullptr)
    {
        glDeleteShader(taskShader);
    }
    glDeleteShader(meshShader);
    glDeleteShader(fragmentShader);
    if (!checkCompileErrors(program, "PROGRAM"))
    {
        return 0;
    }
    return program;
}

glw::GLvoid ExtFunctions::Init()
{
#define GET_FUNCTION(func) func = (func##_ProcAddress)m_renderContext.getProcAddress("gl" #func)

    // EXT_mesh_shader
    GET_FUNCTION(DrawMeshTasksEXT);
    GET_FUNCTION(DrawMeshTasksIndirectEXT);
    GET_FUNCTION(MultiDrawMeshTasksIndirectEXT);
    GET_FUNCTION(MultiDrawMeshTasksIndirectCountEXT);

#undef GET_FUNCTION
}

} // namespace meshShader
} // namespace glc
