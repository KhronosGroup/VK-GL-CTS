/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Wrapper for GL program object.
 *//*--------------------------------------------------------------------*/

#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuTestLog.hpp"
#include "deClock.h"

#include <cstring>

using std::string;

namespace glu
{

// Shader

Shader::Shader (const RenderContext& renderCtx, ShaderType shaderType)
	: m_renderCtx	(renderCtx)
	, m_shader		(0)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	m_info.type	= shaderType;
	m_shader	= gl.createShader(getGLShaderType(shaderType));
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCreateShader()");
	TCU_CHECK(m_shader);
}

Shader::~Shader (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	gl.deleteShader(m_shader);
}

void Shader::setSources (int numSourceStrings, const char* const* sourceStrings, const int* lengths)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.shaderSource(m_shader, numSourceStrings, sourceStrings, lengths);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glShaderSource()");

	m_info.source.clear();
	for (int ndx = 0; ndx < numSourceStrings; ndx++)
	{
		const size_t length = lengths && lengths[ndx] >= 0 ? lengths[ndx] : strlen(sourceStrings[ndx]);
		m_info.source += std::string(sourceStrings[ndx], length);
	}
}

void Shader::compile (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	m_info.compileOk		= false;
	m_info.compileTimeUs	= 0;
	m_info.infoLog.clear();

	{
		deUint64 compileStart = deGetMicroseconds();
		gl.compileShader(m_shader);
		m_info.compileTimeUs = deGetMicroseconds() - compileStart;
	}

	GLU_EXPECT_NO_ERROR(gl.getError(), "glCompileShader()");

	// Query status & log.
	{
		int	compileStatus	= 0;
		int	infoLogLen		= 0;
		int	unusedLen;

		gl.getShaderiv(m_shader, GL_COMPILE_STATUS,		&compileStatus);
		gl.getShaderiv(m_shader, GL_INFO_LOG_LENGTH,	&infoLogLen);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderiv()");

		m_info.compileOk = compileStatus != GL_FALSE;

		if (infoLogLen > 0)
		{
			std::vector<char> infoLog(infoLogLen);
			gl.getShaderInfoLog(m_shader, (int)infoLog.size(), &unusedLen, &infoLog[0]);
			m_info.infoLog = std::string(&infoLog[0], infoLogLen);
		}
	}
}

// Program

static bool getProgramLinkStatus (const RenderContext& renderCtx, deUint32 program)
{
	const glw::Functions& gl	= renderCtx.getFunctions();
	int	linkStatus				= 0;

	gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");
	return (linkStatus != GL_FALSE);
}

static std::string getProgramInfoLog (const RenderContext& renderCtx, deUint32 program)
{
	const glw::Functions& gl = renderCtx.getFunctions();

	int	infoLogLen	= 0;
	int	unusedLen;

	gl.getProgramiv(program, GL_INFO_LOG_LENGTH,	&infoLogLen);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");

	if (infoLogLen > 0)
	{
		std::vector<char> infoLog(infoLogLen);
		gl.getProgramInfoLog(program, (int)infoLog.size(), &unusedLen, &infoLog[0]);
		return std::string(&infoLog[0], infoLogLen);
	}
	return std::string();
}

Program::Program (const RenderContext& renderCtx)
	: m_renderCtx	(renderCtx)
	, m_program		(0)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	m_program = gl.createProgram();
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCreateProgram()");
}

Program::Program (const RenderContext& renderCtx, deUint32 program)
	: m_renderCtx	(renderCtx)
	, m_program		(program)
{
	m_info.linkOk	= getProgramLinkStatus(renderCtx, program);
	m_info.infoLog	= getProgramInfoLog(renderCtx, program);
}

Program::~Program (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	gl.deleteProgram(m_program);
}

void Program::attachShader (deUint32 shader)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.attachShader(m_program, shader);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glAttachShader()");
}

void Program::detachShader (deUint32 shader)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.detachShader(m_program, shader);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDetachShader()");
}

void Program::bindAttribLocation (deUint32 location, const char* name)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.bindAttribLocation(m_program, location, name);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindAttribLocation()");
}

void Program::transformFeedbackVaryings (int count, const char* const* varyings, deUint32 bufferMode)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.transformFeedbackVaryings(m_program, count, varyings, bufferMode);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTransformFeedbackVaryings()");
}

void Program::link (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	m_info.linkOk		= false;
	m_info.linkTimeUs	= 0;
	m_info.infoLog.clear();

	{
		deUint64 linkStart = deGetMicroseconds();
		gl.linkProgram(m_program);
		m_info.linkTimeUs = deGetMicroseconds() - linkStart;
	}
	GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

	m_info.linkOk	= getProgramLinkStatus(m_renderCtx, m_program);
	m_info.infoLog	= getProgramInfoLog(m_renderCtx, m_program);
}

bool Program::isSeparable (void) const
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	int separable = GL_FALSE;

	gl.getProgramiv(m_program, GL_PROGRAM_SEPARABLE, &separable);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");

	return (separable != GL_FALSE);
}

void Program::setSeparable (bool separable)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.programParameteri(m_program, GL_PROGRAM_SEPARABLE, separable);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glProgramParameteri()");
}

// ProgramPipeline

ProgramPipeline::ProgramPipeline (const RenderContext& renderCtx)
	: m_renderCtx	(renderCtx)
	, m_pipeline	(0)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.genProgramPipelines(1, &m_pipeline);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenProgramPipelines()");
}

ProgramPipeline::~ProgramPipeline (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	gl.deleteProgramPipelines(1, &m_pipeline);
}

void ProgramPipeline::useProgramStages (deUint32 stages, deUint32 program)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.useProgramStages(m_pipeline, stages, program);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgramStages()");
}

void ProgramPipeline::activeShaderProgram (deUint32 program)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();

	gl.activeShaderProgram(m_pipeline, program);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glActiveShaderProgram()");
}

bool ProgramPipeline::isValid (void)
{
	const glw::Functions& gl = m_renderCtx.getFunctions();
	glw::GLint status = GL_FALSE;
	gl.validateProgramPipeline(m_pipeline);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glValidateProgramPipeline()");

	gl.getProgramPipelineiv(m_pipeline, GL_VALIDATE_STATUS, &status);

	return (status != GL_FALSE);
}

// ShaderProgram

ShaderProgram::ShaderProgram (const RenderContext& renderCtx, const ProgramSources& sources)
	: m_program(renderCtx)
{
	try
	{
		bool shadersOk = true;

		for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
		{
			for (int shaderNdx = 0; shaderNdx < (int)sources.sources[shaderType].size(); ++shaderNdx)
			{
				const char* source	= sources.sources[shaderType][shaderNdx].c_str();
				const int	length	= (int)sources.sources[shaderType][shaderNdx].size();

				m_shaders[shaderType].reserve(m_shaders[shaderType].size() + 1);

				m_shaders[shaderType].push_back(new Shader(renderCtx, ShaderType(shaderType)));
				m_shaders[shaderType].back()->setSources(1, &source, &length);
				m_shaders[shaderType].back()->compile();

				shadersOk = shadersOk && m_shaders[shaderType].back()->getCompileStatus();
			}
		}

		if (shadersOk)
		{
			for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
				for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
					m_program.attachShader(m_shaders[shaderType][shaderNdx]->getShader());

			for (std::vector<AttribLocationBinding>::const_iterator binding = sources.attribLocationBindings.begin(); binding != sources.attribLocationBindings.end(); ++binding)
				m_program.bindAttribLocation(binding->location, binding->name.c_str());

			DE_ASSERT((sources.transformFeedbackBufferMode == GL_NONE) == sources.transformFeedbackVaryings.empty());
			if (sources.transformFeedbackBufferMode != GL_NONE)
			{
				std::vector<const char*> tfVaryings(sources.transformFeedbackVaryings.size());
				for (int ndx = 0; ndx < (int)tfVaryings.size(); ndx++)
					tfVaryings[ndx] = sources.transformFeedbackVaryings[ndx].c_str();

				m_program.transformFeedbackVaryings((int)tfVaryings.size(), &tfVaryings[0], sources.transformFeedbackBufferMode);
			}

			if (sources.separable)
				m_program.setSeparable(true);

			m_program.link();
		}
	}
	catch (...)
	{
		for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
			for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
				delete m_shaders[shaderType][shaderNdx];
		throw;
	}
}

ShaderProgram::~ShaderProgram (void)
{
	for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
		for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
			delete m_shaders[shaderType][shaderNdx];
}

// Utilities

deUint32 getGLShaderType (ShaderType shaderType)
{
	static const deUint32 s_typeMap[] =
	{
		GL_VERTEX_SHADER,
		GL_FRAGMENT_SHADER,
		GL_GEOMETRY_SHADER,
		GL_TESS_CONTROL_SHADER,
		GL_TESS_EVALUATION_SHADER,
		GL_COMPUTE_SHADER
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typeMap) == SHADERTYPE_LAST);
	DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typeMap)));
	return s_typeMap[shaderType];
}

deUint32 getGLShaderTypeBit (ShaderType shaderType)
{
	static const deUint32 s_typebitMap[] =
	{
		GL_VERTEX_SHADER_BIT,
		GL_FRAGMENT_SHADER_BIT,
		GL_GEOMETRY_SHADER_BIT,
		GL_TESS_CONTROL_SHADER_BIT,
		GL_TESS_EVALUATION_SHADER_BIT,
		GL_COMPUTE_SHADER_BIT
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typebitMap) == SHADERTYPE_LAST);
	DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typebitMap)));
	return s_typebitMap[shaderType];
}

qpShaderType getLogShaderType (ShaderType shaderType)
{
	static const qpShaderType s_typeMap[] =
	{
		QP_SHADER_TYPE_VERTEX,
		QP_SHADER_TYPE_FRAGMENT,
		QP_SHADER_TYPE_GEOMETRY,
		QP_SHADER_TYPE_TESS_CONTROL,
		QP_SHADER_TYPE_TESS_EVALUATION,
		QP_SHADER_TYPE_COMPUTE
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typeMap) == SHADERTYPE_LAST);
	DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typeMap)));
	return s_typeMap[shaderType];
}

static tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderInfo& shaderInfo)
{
	return log << tcu::TestLog::Shader(getLogShaderType(shaderInfo.type), shaderInfo.source, shaderInfo.compileOk, shaderInfo.infoLog);
}

tcu::TestLog& operator<< (tcu::TestLog& log, const Shader& shader)
{
	return log << tcu::TestLog::ShaderProgram(false, "Plain shader") << shader.getInfo() << tcu::TestLog::EndShaderProgram;
}

tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderProgram& program)
{
	const ProgramInfo& progInfo = program.getProgramInfo();

	log << tcu::TestLog::ShaderProgram(progInfo.linkOk, progInfo.infoLog);
	try
	{
		for (int shaderTypeNdx = 0; shaderTypeNdx < SHADERTYPE_LAST; shaderTypeNdx++)
		{
			const glu::ShaderType shaderType = (glu::ShaderType)shaderTypeNdx;

			for (int shaderNdx = 0; shaderNdx < program.getNumShaders(shaderType); ++shaderNdx)
				log << program.getShaderInfo(shaderType, shaderNdx);
		}
	}
	catch (...)
	{
		log << tcu::TestLog::EndShaderProgram;
		throw;
	}
	log << tcu::TestLog::EndShaderProgram;

	// Write statistics.
	{
		static const struct
		{
			const char*		name;
			const char*		description;
		} s_compileTimeDesc[] =
		{
			{ "VertexCompileTime",			"Vertex shader compile time"					},
			{ "FragmentCompileTime",		"Fragment shader compile time"					},
			{ "GeometryCompileTime",		"Geometry shader compile time"					},
			{ "TessControlCompileTime",		"Tesselation control shader compile time"		},
			{ "TessEvaluationCompileTime",	"Tesselation evaluation shader compile time"	},
			{ "ComputeCompileTime",			"Compute shader compile time"					},
		};
		DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_compileTimeDesc) == SHADERTYPE_LAST);

		bool allShadersOk = true;

		for (int shaderTypeNdx = 0; shaderTypeNdx < SHADERTYPE_LAST; shaderTypeNdx++)
		{
			const glu::ShaderType shaderType = (glu::ShaderType)shaderTypeNdx;

			for (int shaderNdx = 0; shaderNdx < program.getNumShaders(shaderType); ++shaderNdx)
			{
				const ShaderInfo& shaderInfo = program.getShaderInfo(shaderType, shaderNdx);
				log << tcu::TestLog::Float(s_compileTimeDesc[shaderType].name, s_compileTimeDesc[shaderType].description, "ms", QP_KEY_TAG_TIME, (float)shaderInfo.compileTimeUs / 1000.0f);
				allShadersOk = allShadersOk && shaderInfo.compileOk;
			}
		}

		if (allShadersOk)
			log << tcu::TestLog::Float("LinkTime", "Link time", "ms", QP_KEY_TAG_TIME, (float)progInfo.linkTimeUs / 1000.0f);
	}

	return log;
}

} // glu
