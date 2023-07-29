#ifndef _ES31FPROGRAMINTERFACEDEFINITION_HPP
#define _ES31FPROGRAMINTERFACEDEFINITION_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Program interface
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes31TestCase.hpp"
#include "gluShaderUtil.hpp"
#include "gluVarType.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{

enum ProgramInterface
{
    PROGRAMINTERFACE_UNIFORM = 0,
    PROGRAMINTERFACE_UNIFORM_BLOCK,
    PROGRAMINTERFACE_ATOMIC_COUNTER_BUFFER,
    PROGRAMINTERFACE_PROGRAM_INPUT,
    PROGRAMINTERFACE_PROGRAM_OUTPUT,
    PROGRAMINTERFACE_TRANSFORM_FEEDBACK_VARYING,
    PROGRAMINTERFACE_BUFFER_VARIABLE,
    PROGRAMINTERFACE_SHADER_STORAGE_BLOCK,

    PROGRAMINTERFACE_LAST
};

namespace ProgramInterfaceDefinition
{

class Program;

struct DefaultBlock
{
    std::vector<glu::VariableDeclaration> variables;
    std::vector<glu::InterfaceBlock> interfaceBlocks;
};

class Shader
{
public:
    glu::ShaderType getType(void) const
    {
        return m_shaderType;
    }
    glu::GLSLVersion getVersion(void) const
    {
        return m_version;
    }
    bool isValid(void) const;

    DefaultBlock &getDefaultBlock(void)
    {
        return m_defaultBlock;
    }
    const DefaultBlock &getDefaultBlock(void) const
    {
        return m_defaultBlock;
    }

private:
    Shader(glu::ShaderType type, glu::GLSLVersion version);
    ~Shader(void);

    Shader(const Shader &);
    Shader &operator=(const Shader &);

    const glu::ShaderType m_shaderType;
    const glu::GLSLVersion m_version;
    DefaultBlock m_defaultBlock;

    friend class Program;
};

class Program
{
public:
    Program(void);
    ~Program(void);

    Shader *addShader(glu::ShaderType type, glu::GLSLVersion version);

    void setSeparable(bool separable);
    bool isSeparable(void) const;

    const std::vector<Shader *> &getShaders(void) const;
    glu::ShaderType getFirstStage(void) const;
    glu::ShaderType getLastStage(void) const;
    bool hasStage(glu::ShaderType stage) const;

    void addTransformFeedbackVarying(const std::string &varName);
    const std::vector<std::string> &getTransformFeedbackVaryings(void) const;
    void setTransformFeedbackMode(uint32_t mode);
    uint32_t getTransformFeedbackMode(void) const;

    uint32_t getGeometryNumOutputVertices(void) const;
    void setGeometryNumOutputVertices(uint32_t);
    uint32_t getTessellationNumOutputPatchVertices(void) const;
    void setTessellationNumOutputPatchVertices(uint32_t);

    bool isValid(void) const;

private:
    Program &operator=(const Program &);
    Program(const Program &);

    bool m_separable;
    std::vector<Shader *> m_shaders;
    std::vector<std::string> m_xfbVaryings;
    uint32_t m_xfbMode;
    uint32_t m_geoNumOutputVertices;
    uint32_t m_tessNumOutputVertices;
};

} // namespace ProgramInterfaceDefinition

} // namespace Functional
} // namespace gles31
} // namespace deqp

#endif // _ES31FPROGRAMINTERFACEDEFINITION_HPP
