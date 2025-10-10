#ifndef _GLCMESHSHADERTESTSUTILS_HPP
#define _GLCMESHSHADERTESTSUTILS_HPP

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

#include "deDefs.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "glcTestCase.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "gluShaderUtil.hpp"
#include "gluContextInfo.hpp"

#include <string>

namespace glc
{
namespace meshShader
{

#ifndef GL_EXT_mesh_shader
#define GL_EXT_mesh_shader 1
#define GL_MESH_SHADER_EXT 0x9559
#define GL_TASK_SHADER_EXT 0x955A
#define GL_MAX_MESH_UNIFORM_BLOCKS_EXT 0x8E60
#define GL_MAX_MESH_TEXTURE_IMAGE_UNITS_EXT 0x8E61
#define GL_MAX_MESH_IMAGE_UNIFORMS_EXT 0x8E62
#define GL_MAX_MESH_UNIFORM_COMPONENTS_EXT 0x8E63
#define GL_MAX_MESH_ATOMIC_COUNTER_BUFFERS_EXT 0x8E64
#define GL_MAX_MESH_ATOMIC_COUNTERS_EXT 0x8E65
#define GL_MAX_MESH_SHADER_STORAGE_BLOCKS_EXT 0x8E66
#define GL_MAX_COMBINED_MESH_UNIFORM_COMPONENTS_EXT 0x8E67
#define GL_MAX_TASK_UNIFORM_BLOCKS_EXT 0x8E68
#define GL_MAX_TASK_TEXTURE_IMAGE_UNITS_EXT 0x8E69
#define GL_MAX_TASK_IMAGE_UNIFORMS_EXT 0x8E6A
#define GL_MAX_TASK_UNIFORM_COMPONENTS_EXT 0x8E6B
#define GL_MAX_TASK_ATOMIC_COUNTER_BUFFERS_EXT 0x8E6C
#define GL_MAX_TASK_ATOMIC_COUNTERS_EXT 0x8E6D
#define GL_MAX_TASK_SHADER_STORAGE_BLOCKS_EXT 0x8E6E
#define GL_MAX_COMBINED_TASK_UNIFORM_COMPONENTS_EXT 0x8E6F
#define GL_MAX_TASK_WORK_GROUP_TOTAL_COUNT_EXT 0x9740
#define GL_MAX_MESH_WORK_GROUP_TOTAL_COUNT_EXT 0x9741
#define GL_MAX_MESH_WORK_GROUP_INVOCATIONS_EXT 0x9757
#define GL_MAX_TASK_WORK_GROUP_INVOCATIONS_EXT 0x9759
#define GL_MAX_TASK_PAYLOAD_SIZE_EXT 0x9742
#define GL_MAX_TASK_SHARED_MEMORY_SIZE_EXT 0x9743
#define GL_MAX_MESH_SHARED_MEMORY_SIZE_EXT 0x9744
#define GL_MAX_TASK_PAYLOAD_AND_SHARED_MEMORY_SIZE_EXT 0x9745
#define GL_MAX_MESH_PAYLOAD_AND_SHARED_MEMORY_SIZE_EXT 0x9746
#define GL_MAX_MESH_OUTPUT_MEMORY_SIZE_EXT 0x9747
#define GL_MAX_MESH_PAYLOAD_AND_OUTPUT_MEMORY_SIZE_EXT 0x9748
#define GL_MAX_MESH_OUTPUT_VERTICES_EXT 0x9538
#define GL_MAX_MESH_OUTPUT_PRIMITIVES_EXT 0x9756
#define GL_MAX_MESH_OUTPUT_COMPONENTS_EXT 0x9749
#define GL_MAX_MESH_OUTPUT_LAYERS_EXT 0x974A
#define GL_MAX_MESH_MULTIVIEW_VIEW_COUNT_EXT 0x9557
#define GL_MESH_OUTPUT_PER_VERTEX_GRANULARITY_EXT 0x92DF
#define GL_MESH_OUTPUT_PER_PRIMITIVE_GRANULARITY_EXT 0x9543
#define GL_MAX_PREFERRED_TASK_WORK_GROUP_INVOCATIONS_EXT 0x974B
#define GL_MAX_PREFERRED_MESH_WORK_GROUP_INVOCATIONS_EXT 0x974C
#define GL_MESH_PREFERS_LOCAL_INVOCATION_VERTEX_OUTPUT_EXT 0x974D
#define GL_MESH_PREFERS_LOCAL_INVOCATION_PRIMITIVE_OUTPUT_EXT 0x974E
#define GL_MESH_PREFERS_COMPACT_VERTEX_OUTPUT_EXT 0x974F
#define GL_MESH_PREFERS_COMPACT_PRIMITIVE_OUTPUT_EXT 0x9750
#define GL_MAX_TASK_WORK_GROUP_COUNT_EXT 0x9751
#define GL_MAX_MESH_WORK_GROUP_COUNT_EXT 0x9752
#define GL_MAX_MESH_WORK_GROUP_SIZE_EXT 0x9758
#define GL_MAX_TASK_WORK_GROUP_SIZE_EXT 0x975A
#define GL_MESH_WORK_GROUP_SIZE_EXT 0x953E
#define GL_TASK_WORK_GROUP_SIZE_EXT 0x953F
#define GL_MESH_VERTICES_OUT_EXT 0x9579
#define GL_MESH_PRIMITIVES_OUT_EXT 0x957A
#define GL_MESH_OUTPUT_TYPE_EXT 0x957B
#define GL_UNIFORM_BLOCK_REFERENCED_BY_MESH_SHADER_EXT 0x959C
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TASK_SHADER_EXT 0x959D
#define GL_REFERENCED_BY_MESH_SHADER_EXT 0x95A0
#define GL_REFERENCED_BY_TASK_SHADER_EXT 0x95A1
#define GL_TASK_SHADER_INVOCATIONS_EXT 0x9753
#define GL_MESH_SHADER_INVOCATIONS_EXT 0x9754
#define GL_MESH_PRIMITIVES_GENERATED_EXT 0x9755
#define GL_MESH_SHADER_BIT_EXT 0x00000040
#define GL_TASK_SHADER_BIT_EXT 0x00000080
#define GL_MESH_SUBROUTINE_EXT 0x957C
#define GL_TASK_SUBROUTINE_EXT 0x957D
#define GL_MESH_SUBROUTINE_UNIFORM_EXT 0x957E
#define GL_TASK_SUBROUTINE_UNIFORM_EXT 0x957F
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_MESH_SHADER_EXT 0x959E
#define GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TASK_SHADER_EXT 0x959F
typedef void(GLW_APIENTRY *DrawMeshTasksEXT_ProcAddress)(glw::GLuint num_groups_x, glw::GLuint num_groups_y,
                                                         glw::GLuint num_groups_z);
typedef void(GLW_APIENTRY *DrawMeshTasksIndirectEXT_ProcAddress)(glw::GLintptr indirect);
typedef void(GLW_APIENTRY *MultiDrawMeshTasksIndirectEXT_ProcAddress)(glw::GLintptr indirect, glw::GLsizei drawcount,
                                                                      glw::GLsizei stride);
typedef void(GLW_APIENTRY *MultiDrawMeshTasksIndirectCountEXT_ProcAddress)(glw::GLintptr indirect,
                                                                           glw::GLintptr drawcount,
                                                                           glw::GLsizei maxdrawcount,
                                                                           glw::GLsizei stride);

typedef struct DrawMeshTasksIndirectCommandStruct
{
    glw::GLuint x;
    glw::GLuint y;
    glw::GLuint z;
} DrawMeshTasksIndirectCommand;

#endif /* GL_EXT_mesh_shader */

glw::GLuint createProgram(const char *taskStr, const char *meshStr, const char *fragStr);

class ExtFunctions
{
public:
    ExtFunctions(glu::RenderContext &renderContext);
    ~ExtFunctions()
    {
    }
    glw::GLvoid Init();

    // EXT_mesh_shader
    DrawMeshTasksEXT_ProcAddress DrawMeshTasksEXT;
    DrawMeshTasksIndirectEXT_ProcAddress DrawMeshTasksIndirectEXT;
    MultiDrawMeshTasksIndirectEXT_ProcAddress MultiDrawMeshTasksIndirectEXT;
    MultiDrawMeshTasksIndirectCountEXT_ProcAddress MultiDrawMeshTasksIndirectCountEXT;

private:
    glu::RenderContext &m_renderContext;
};

} // namespace meshShader
} // namespace glc

#endif // _GLCMESHSHADERTESTSUTILS_HPP
