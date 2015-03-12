/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Compiler test case.
 *//*--------------------------------------------------------------------*/

#include "glsShaderLibraryCase.hpp"

#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"

#include "tcuStringTemplate.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluStrUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deRandom.hpp"
#include "deInt32.h"
#include "deMath.h"
#include "deString.h"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"

#include <map>
#include <vector>
#include <string>
#include <sstream>

using namespace std;
using namespace tcu;
using namespace glu;

namespace deqp
{
namespace gls
{
namespace sl
{

enum
{
	VIEWPORT_WIDTH		= 128,
	VIEWPORT_HEIGHT		= 128
};

static inline bool usesShaderInoutQualifiers (glu::GLSLVersion version)
{
	switch (version)
	{
		case glu::GLSL_VERSION_100_ES:
		case glu::GLSL_VERSION_130:
		case glu::GLSL_VERSION_140:
		case glu::GLSL_VERSION_150:
			return false;

		default:
			return true;
	}
}

static inline bool supportsFragmentHighp (glu::GLSLVersion version)
{
	return version != glu::GLSL_VERSION_100_ES;
}

ShaderCase::ValueBlock::ValueBlock (void)
	: arrayLength(0)
{
}

ShaderCase::CaseRequirement::CaseRequirement (void)
	: m_type						(REQUIREMENTTYPE_LAST)
	, m_supportedExtensionNdx		(-1)
	, m_effectiveShaderStageFlags	(-1)
	, m_enumName					(-1)
	, m_referenceValue				(-1)
{
}

ShaderCase::CaseRequirement ShaderCase::CaseRequirement::createAnyExtensionRequirement (const std::vector<std::string>& requirements, deUint32 effectiveShaderStageFlags)
{
	CaseRequirement retVal;

	retVal.m_type = REQUIREMENTTYPE_EXTENSION;
	retVal.m_extensions = requirements;
	retVal.m_effectiveShaderStageFlags = effectiveShaderStageFlags;

	return retVal;
}

ShaderCase::CaseRequirement ShaderCase::CaseRequirement::createLimitRequirement (deUint32 enumName, int ref)
{
	CaseRequirement retVal;

	retVal.m_type = REQUIREMENTTYPE_IMPLEMENTATION_LIMIT;
	retVal.m_enumName = enumName;
	retVal.m_referenceValue = ref;

	return retVal;
}

ShaderCase::CaseRequirement ShaderCase::CaseRequirement::createFullGLSLES100SpecificationRequirement (void)
{
	CaseRequirement retVal;

	retVal.m_type = REQUIREMENTTYPE_FULL_GLSL_ES_100_SPEC;

	return retVal;
}

void ShaderCase::CaseRequirement::checkRequirements (glu::RenderContext& renderCtx, const glu::ContextInfo& contextInfo)
{
	DE_UNREF(renderCtx);

	switch (m_type)
	{
		case REQUIREMENTTYPE_EXTENSION:
		{
			for (int ndx = 0; ndx < (int)m_extensions.size(); ++ndx)
			{
				if (contextInfo.isExtensionSupported(m_extensions[ndx].c_str()))
				{
					m_supportedExtensionNdx = ndx;
					return;
				}
			}

			// no extension(s). Make a nice output
			{
				std::ostringstream extensionList;

				for (int ndx = 0; ndx < (int)m_extensions.size(); ++ndx)
				{
					if (!extensionList.str().empty())
						extensionList << ", ";
					extensionList << m_extensions[ndx];
				}

				if (m_extensions.size() == 1)
					throw tcu::NotSupportedError("Test requires extension " + extensionList.str());
				else
					throw tcu::NotSupportedError("Test requires any extension of " + extensionList.str());
			}

			// cannot be reached
		}

		case REQUIREMENTTYPE_IMPLEMENTATION_LIMIT:
		{
			const glw::Functions&	gl		= renderCtx.getFunctions();
			glw::GLint				value	= 0;
			glw::GLenum				error;

			gl.getIntegerv(m_enumName, &value);
			error = gl.getError();

			if (error != GL_NO_ERROR)
				throw tcu::TestError("Query for " + de::toString(glu::getGettableStateStr(m_enumName)) +  " generated " + de::toString(glu::getErrorStr(error)));

			if (!(value > m_referenceValue))
				throw tcu::NotSupportedError("Test requires " + de::toString(glu::getGettableStateStr(m_enumName)) + " (" + de::toString(value) + ") > " + de::toString(m_referenceValue));

			return;
		}

		case REQUIREMENTTYPE_FULL_GLSL_ES_100_SPEC:
		{
			// cannot be queried
			return;
		}

		default:
			DE_ASSERT(false);
	}
}

ShaderCase::ShaderCaseSpecification::ShaderCaseSpecification (void)
	: expectResult	(EXPECT_LAST)
	, targetVersion	(glu::GLSL_VERSION_LAST)
	, caseType		(CASETYPE_COMPLETE)
{
}

ShaderCase::ShaderCaseSpecification ShaderCase::ShaderCaseSpecification::generateSharedSourceVertexCase (ExpectResult expectResult_, glu::GLSLVersion targetVersion_, const std::vector<ValueBlock>& values, const std::string& sharedSource)
{
	ShaderCaseSpecification retVal;
	retVal.expectResult		= expectResult_;
	retVal.targetVersion	= targetVersion_;
	retVal.caseType			= CASETYPE_VERTEX_ONLY;
	retVal.valueBlocks		= values;
	retVal.vertexSources.push_back(sharedSource);
	return retVal;
}

ShaderCase::ShaderCaseSpecification ShaderCase::ShaderCaseSpecification::generateSharedSourceFragmentCase (ExpectResult expectResult_, glu::GLSLVersion targetVersion_, const std::vector<ValueBlock>& values, const std::string& sharedSource)
{
	ShaderCaseSpecification retVal;
	retVal.expectResult		= expectResult_;
	retVal.targetVersion	= targetVersion_;
	retVal.caseType			= CASETYPE_FRAGMENT_ONLY;
	retVal.valueBlocks		= values;
	retVal.fragmentSources.push_back(sharedSource);
	return retVal;
}

class BeforeDrawValidator : public glu::DrawUtilCallback
{
public:
	enum TargetType
	{
		TARGETTYPE_PROGRAM = 0,
		TARGETTYPE_PIPELINE,

		TARGETTYPE_LAST
	};

							BeforeDrawValidator	(const glw::Functions& gl, glw::GLuint target, TargetType targetType);

	void					beforeDrawCall		(void);

	const std::string&		getInfoLog			(void) const;
	glw::GLint				getValidateStatus	(void) const;

private:
	const glw::Functions&	m_gl;
	const glw::GLuint		m_target;
	const TargetType		m_targetType;

	glw::GLint				m_validateStatus;
	std::string				m_logMessage;
};

BeforeDrawValidator::BeforeDrawValidator (const glw::Functions& gl, glw::GLuint target, TargetType targetType)
	: m_gl				(gl)
	, m_target			(target)
	, m_targetType		(targetType)
	, m_validateStatus	(-1)
{
	DE_ASSERT(targetType < TARGETTYPE_LAST);
}

void BeforeDrawValidator::beforeDrawCall (void)
{
	glw::GLint					bytesWritten	= 0;
	glw::GLint					infoLogLength;
	std::vector<glw::GLchar>	logBuffer;
	int							stringLength;

	// validate
	if (m_targetType == TARGETTYPE_PROGRAM)
		m_gl.validateProgram(m_target);
	else if (m_targetType == TARGETTYPE_PIPELINE)
		m_gl.validateProgramPipeline(m_target);
	else
		DE_ASSERT(false);

	GLU_EXPECT_NO_ERROR(m_gl.getError(), "validate");

	// check status
	m_validateStatus = -1;

	if (m_targetType == TARGETTYPE_PROGRAM)
		m_gl.getProgramiv(m_target, GL_VALIDATE_STATUS, &m_validateStatus);
	else if (m_targetType == TARGETTYPE_PIPELINE)
		m_gl.getProgramPipelineiv(m_target, GL_VALIDATE_STATUS, &m_validateStatus);
	else
		DE_ASSERT(false);

	GLU_EXPECT_NO_ERROR(m_gl.getError(), "get validate status");
	TCU_CHECK(m_validateStatus == GL_TRUE || m_validateStatus == GL_FALSE);

	// read log

	infoLogLength = 0;

	if (m_targetType == TARGETTYPE_PROGRAM)
		m_gl.getProgramiv(m_target, GL_INFO_LOG_LENGTH, &infoLogLength);
	else if (m_targetType == TARGETTYPE_PIPELINE)
		m_gl.getProgramPipelineiv(m_target, GL_INFO_LOG_LENGTH, &infoLogLength);
	else
		DE_ASSERT(false);

	GLU_EXPECT_NO_ERROR(m_gl.getError(), "get info log length");

	if (infoLogLength <= 0)
	{
		m_logMessage.clear();
		return;
	}

	logBuffer.resize(infoLogLength + 2, '0'); // +1 for zero terminator (infoLogLength should include it, but better play it safe), +1 to make sure buffer is always larger

	if (m_targetType == TARGETTYPE_PROGRAM)
		m_gl.getProgramInfoLog(m_target, infoLogLength + 1, &bytesWritten, &logBuffer[0]);
	else if (m_targetType == TARGETTYPE_PIPELINE)
		m_gl.getProgramPipelineInfoLog(m_target, infoLogLength + 1, &bytesWritten, &logBuffer[0]);
	else
		DE_ASSERT(false);

	// just ignore bytesWritten to be safe, find the null terminator
	stringLength = (int)(std::find(logBuffer.begin(), logBuffer.end(), '0') - logBuffer.begin());
	m_logMessage.assign(&logBuffer[0], stringLength);
}

const std::string& BeforeDrawValidator::getInfoLog (void) const
{
	return m_logMessage;
}

glw::GLint BeforeDrawValidator::getValidateStatus (void) const
{
	return m_validateStatus;
}

// ShaderCase.

ShaderCase::ShaderCase (tcu::TestContext& testCtx, RenderContext& renderCtx, const glu::ContextInfo& contextInfo, const char* name, const char* description, const ShaderCaseSpecification& specification)
	: tcu::TestCase				(testCtx, name, description)
	, m_renderCtx				(renderCtx)
	, m_contextInfo				(contextInfo)
	, m_caseType				(specification.caseType)
	, m_expectResult			(specification.expectResult)
	, m_targetVersion			(specification.targetVersion)
	, m_separatePrograms		(false)
	, m_valueBlocks				(specification.valueBlocks)
{
	if (m_caseType == CASETYPE_VERTEX_ONLY)
	{
		// case generated from "both" target, vertex case
		DE_ASSERT(specification.vertexSources.size() == 1);
		DE_ASSERT(specification.fragmentSources.empty());
		DE_ASSERT(specification.tessCtrlSources.empty());
		DE_ASSERT(specification.tessEvalSources.empty());
		DE_ASSERT(specification.geometrySources.empty());
	}
	else if (m_caseType == CASETYPE_FRAGMENT_ONLY)
	{
		// case generated from "both" target, fragment case
		DE_ASSERT(specification.vertexSources.empty());
		DE_ASSERT(specification.fragmentSources.size() == 1);
		DE_ASSERT(specification.tessCtrlSources.empty());
		DE_ASSERT(specification.tessEvalSources.empty());
		DE_ASSERT(specification.geometrySources.empty());
	}

	if (m_expectResult == EXPECT_BUILD_SUCCESSFUL)
	{
		// Shader is never executed. Presense of input/output values is likely an error
		DE_ASSERT(m_valueBlocks.empty());
	}

	// single program object
	{
		ProgramObject program;
		program.spec.requirements		= specification.requirements;
		program.spec.vertexSources		= specification.vertexSources;
		program.spec.fragmentSources	= specification.fragmentSources;
		program.spec.tessCtrlSources	= specification.tessCtrlSources;
		program.spec.tessEvalSources	= specification.tessEvalSources;
		program.spec.geometrySources	= specification.geometrySources;

		m_programs.push_back(program);
	}
}

ShaderCase::ShaderCase (tcu::TestContext& testCtx, RenderContext& renderCtx, const glu::ContextInfo& contextInfo, const char* name, const char* description, const PipelineCaseSpecification& specification)
	: tcu::TestCase				(testCtx, name, description)
	, m_renderCtx				(renderCtx)
	, m_contextInfo				(contextInfo)
	, m_caseType				(specification.caseType)
	, m_expectResult			(specification.expectResult)
	, m_targetVersion			(specification.targetVersion)
	, m_separatePrograms		(true)
	, m_valueBlocks				(specification.valueBlocks)
{
	deUint32 totalActiveMask = 0;

	DE_ASSERT(m_caseType == CASETYPE_COMPLETE);

	// validate

	for (int pipelineProgramNdx = 0; pipelineProgramNdx < (int)specification.programs.size(); ++pipelineProgramNdx)
	{
		// program with an active stage must contain executable code for that stage
		DE_ASSERT(((specification.programs[pipelineProgramNdx].activeStageBits & (1 << glu::SHADERTYPE_VERTEX))						== 0) || !specification.programs[pipelineProgramNdx].vertexSources.empty());
		DE_ASSERT(((specification.programs[pipelineProgramNdx].activeStageBits & (1 << glu::SHADERTYPE_FRAGMENT))					== 0) || !specification.programs[pipelineProgramNdx].fragmentSources.empty());
		DE_ASSERT(((specification.programs[pipelineProgramNdx].activeStageBits & (1 << glu::SHADERTYPE_TESSELLATION_CONTROL))		== 0) || !specification.programs[pipelineProgramNdx].tessCtrlSources.empty());
		DE_ASSERT(((specification.programs[pipelineProgramNdx].activeStageBits & (1 << glu::SHADERTYPE_TESSELLATION_EVALUATION))	== 0) || !specification.programs[pipelineProgramNdx].tessEvalSources.empty());
		DE_ASSERT(((specification.programs[pipelineProgramNdx].activeStageBits & (1 << glu::SHADERTYPE_GEOMETRY))					== 0) || !specification.programs[pipelineProgramNdx].geometrySources.empty());

		// no two programs with with the same stage active
		DE_ASSERT((totalActiveMask & specification.programs[pipelineProgramNdx].activeStageBits) == 0);
		totalActiveMask |= specification.programs[pipelineProgramNdx].activeStageBits;
	}

	// create ProgramObjects

	for (int pipelineProgramNdx = 0; pipelineProgramNdx < (int)specification.programs.size(); ++pipelineProgramNdx)
	{
		ProgramObject program;
		program.spec = specification.programs[pipelineProgramNdx];
		m_programs.push_back(program);
	}
}

ShaderCase::~ShaderCase (void)
{
}

void ShaderCase::init (void)
{
	// If no value blocks given, use an empty one.
	if (m_valueBlocks.empty())
		m_valueBlocks.push_back(ValueBlock());

	// Use first value block to specialize shaders.
	const ValueBlock& valueBlock = m_valueBlocks[0];

	// \todo [2010-04-01 petri] Check that all value blocks have matching values.

	// prepare programs
	for (int programNdx = 0; programNdx < (int)m_programs.size(); ++programNdx)
	{
		// Check requirements
		for (int ndx = 0; ndx < (int)m_programs[programNdx].spec.requirements.size(); ++ndx)
			m_programs[programNdx].spec.requirements[ndx].checkRequirements(m_renderCtx, m_contextInfo);

		// Generate specialized shader sources.
		if (m_caseType == CASETYPE_COMPLETE)
		{
			// all shaders specified separately
			specializeVertexShaders		(m_programs[programNdx].programSources,	m_programs[programNdx].spec.vertexSources,		valueBlock,	m_programs[programNdx].spec.requirements);
			specializeFragmentShaders	(m_programs[programNdx].programSources,	m_programs[programNdx].spec.fragmentSources,	valueBlock,	m_programs[programNdx].spec.requirements);
			specializeGeometryShaders	(m_programs[programNdx].programSources,	m_programs[programNdx].spec.geometrySources,	valueBlock,	m_programs[programNdx].spec.requirements);
			specializeTessControlShaders(m_programs[programNdx].programSources,	m_programs[programNdx].spec.tessCtrlSources,	valueBlock,	m_programs[programNdx].spec.requirements);
			specializeTessEvalShaders	(m_programs[programNdx].programSources,	m_programs[programNdx].spec.tessEvalSources,	valueBlock,	m_programs[programNdx].spec.requirements);
		}
		else if (m_caseType == CASETYPE_VERTEX_ONLY)
		{
			DE_ASSERT(m_programs.size() == 1);
			DE_ASSERT(!m_separatePrograms);

			// case generated from "both" target, vertex case
			m_programs[0].programSources << glu::VertexSource(specializeVertexShader(m_programs[0].spec.vertexSources[0].c_str(), valueBlock));
			m_programs[0].programSources << glu::FragmentSource(genFragmentShader(valueBlock));
		}
		else if (m_caseType == CASETYPE_FRAGMENT_ONLY)
		{
			DE_ASSERT(m_programs.size() == 1);
			DE_ASSERT(!m_separatePrograms);

			// case generated from "both" target, fragment case
			m_programs[0].programSources << glu::VertexSource(genVertexShader(valueBlock));
			m_programs[0].programSources << glu::FragmentSource(specializeFragmentShader(m_programs[0].spec.fragmentSources[0].c_str(), valueBlock));
		}

		m_programs[programNdx].programSources << glu::ProgramSeparable(m_separatePrograms);
	}

	// log the expected result
	switch (m_expectResult)
	{
		case EXPECT_PASS:
			// Don't write anything
			break;

		case EXPECT_COMPILE_FAIL:
			m_testCtx.getLog() << tcu::TestLog::Message << "Expecting shader compilation to fail." << tcu::TestLog::EndMessage;
			break;

		case EXPECT_LINK_FAIL:
			m_testCtx.getLog() << tcu::TestLog::Message << "Expecting program linking to fail." << tcu::TestLog::EndMessage;
			break;

		case EXPECT_COMPILE_LINK_FAIL:
			m_testCtx.getLog() << tcu::TestLog::Message << "Expecting either shader compilation or program linking to fail." << tcu::TestLog::EndMessage;
			break;

		case EXPECT_VALIDATION_FAIL:
			m_testCtx.getLog() << tcu::TestLog::Message << "Expecting program validation to fail." << tcu::TestLog::EndMessage;
			break;

		case EXPECT_BUILD_SUCCESSFUL:
			m_testCtx.getLog() << tcu::TestLog::Message << "Expecting shader compilation and program linking to succeed. Resulting program will not be executed." << tcu::TestLog::EndMessage;
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	// sanity of arguments

	if (anyProgramRequiresFullGLSLES100Specification())
	{
		// makes only sense in tests where shader compiles
		DE_ASSERT(m_expectResult == EXPECT_PASS				||
				  m_expectResult == EXPECT_VALIDATION_FAIL	||
				  m_expectResult == EXPECT_BUILD_SUCCESSFUL);

		// only makes sense for ES 100 programs
		DE_ASSERT(m_targetVersion == glu::GLSL_VERSION_100_ES);
	}
}

static void setUniformValue (const glw::Functions& gl, const std::vector<deUint32>& pipelinePrograms, const std::string& name, const ShaderCase::Value& val, int arrayNdx, tcu::TestLog& log)
{
	bool foundAnyMatch = false;

	for (int programNdx = 0; programNdx < (int)pipelinePrograms.size(); ++programNdx)
	{
		const int scalarSize	= getDataTypeScalarSize(val.dataType);
		const int loc			= gl.getUniformLocation(pipelinePrograms[programNdx], name.c_str());
		const int elemNdx		= (val.arrayLength == 1) ? (0) : (arrayNdx * scalarSize);

		if (loc == -1)
			continue;

		foundAnyMatch = true;

		DE_STATIC_ASSERT(sizeof(ShaderCase::Value::Element) == sizeof(glw::GLfloat));
		DE_STATIC_ASSERT(sizeof(ShaderCase::Value::Element) == sizeof(glw::GLint));

		gl.useProgram(pipelinePrograms[programNdx]);

		switch (val.dataType)
		{
			case TYPE_FLOAT:		gl.uniform1fv(loc, 1, &val.elements[elemNdx].float32);						break;
			case TYPE_FLOAT_VEC2:	gl.uniform2fv(loc, 1, &val.elements[elemNdx].float32);						break;
			case TYPE_FLOAT_VEC3:	gl.uniform3fv(loc, 1, &val.elements[elemNdx].float32);						break;
			case TYPE_FLOAT_VEC4:	gl.uniform4fv(loc, 1, &val.elements[elemNdx].float32);						break;
			case TYPE_FLOAT_MAT2:	gl.uniformMatrix2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);		break;
			case TYPE_FLOAT_MAT3:	gl.uniformMatrix3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);		break;
			case TYPE_FLOAT_MAT4:	gl.uniformMatrix4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);		break;
			case TYPE_INT:			gl.uniform1iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_INT_VEC2:		gl.uniform2iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_INT_VEC3:		gl.uniform3iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_INT_VEC4:		gl.uniform4iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_BOOL:			gl.uniform1iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_BOOL_VEC2:	gl.uniform2iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_BOOL_VEC3:	gl.uniform3iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_BOOL_VEC4:	gl.uniform4iv(loc, 1, &val.elements[elemNdx].int32);						break;
			case TYPE_UINT:			gl.uniform1uiv(loc, 1, (const deUint32*)&val.elements[elemNdx].int32);		break;
			case TYPE_UINT_VEC2:	gl.uniform2uiv(loc, 1, (const deUint32*)&val.elements[elemNdx].int32);		break;
			case TYPE_UINT_VEC3:	gl.uniform3uiv(loc, 1, (const deUint32*)&val.elements[elemNdx].int32);		break;
			case TYPE_UINT_VEC4:	gl.uniform4uiv(loc, 1, (const deUint32*)&val.elements[elemNdx].int32);		break;
			case TYPE_FLOAT_MAT2X3:	gl.uniformMatrix2x3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;
			case TYPE_FLOAT_MAT2X4:	gl.uniformMatrix2x4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;
			case TYPE_FLOAT_MAT3X2:	gl.uniformMatrix3x2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;
			case TYPE_FLOAT_MAT3X4:	gl.uniformMatrix3x4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;
			case TYPE_FLOAT_MAT4X2:	gl.uniformMatrix4x2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;
			case TYPE_FLOAT_MAT4X3:	gl.uniformMatrix4x3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);	break;

			case TYPE_SAMPLER_2D:
			case TYPE_SAMPLER_CUBE:
				DE_ASSERT(!"implement!");
				break;

			default:
				DE_ASSERT(false);
		}
	}

	if (!foundAnyMatch)
		log << tcu::TestLog::Message << "WARNING // Uniform \"" << name << "\" location is not valid, location = -1. Cannot set value to the uniform." << tcu::TestLog::EndMessage;
}

bool ShaderCase::isTessellationPresent (void) const
{
	if (m_separatePrograms)
	{
		const deUint32 tessellationBits =	(1 << glu::SHADERTYPE_TESSELLATION_CONTROL)		|
											(1 << glu::SHADERTYPE_TESSELLATION_EVALUATION);

		for (int programNdx = 0; programNdx < (int)m_programs.size(); ++programNdx)
			if (m_programs[programNdx].spec.activeStageBits & tessellationBits)
				return true;
		return false;
	}
	else
		return	!m_programs[0].programSources.sources[glu::SHADERTYPE_TESSELLATION_CONTROL].empty() ||
				!m_programs[0].programSources.sources[glu::SHADERTYPE_TESSELLATION_EVALUATION].empty();
}

bool ShaderCase::anyProgramRequiresFullGLSLES100Specification (void) const
{
	for (int programNdx = 0; programNdx < (int)m_programs.size(); ++programNdx)
	for (int requirementNdx = 0; requirementNdx < (int)m_programs[programNdx].spec.requirements.size(); ++requirementNdx)
	{
		if (m_programs[programNdx].spec.requirements[requirementNdx].getType() == CaseRequirement::REQUIREMENTTYPE_FULL_GLSL_ES_100_SPEC)
			return true;
	}
	return false;
}

bool ShaderCase::checkPixels (Surface& surface, int minX, int maxX, int minY, int maxY)
{
	TestLog&	log				= m_testCtx.getLog();
	bool		allWhite		= true;
	bool		allBlack		= true;
	bool		anyUnexpected	= false;

	DE_ASSERT((maxX > minX) && (maxY > minY));

	for (int y = minY; y <= maxY; y++)
	{
		for (int x = minX; x <= maxX; x++)
		{
			RGBA		pixel		 = surface.getPixel(x, y);
			// Note: we really do not want to involve alpha in the check comparison
			// \todo [2010-09-22 kalle] Do we know that alpha would be one? If yes, could use color constants white and black.
			bool		isWhite		 = (pixel.getRed() == 255) && (pixel.getGreen() == 255) && (pixel.getBlue() == 255);
			bool		isBlack		 = (pixel.getRed() == 0) && (pixel.getGreen() == 0) && (pixel.getBlue() == 0);

			allWhite		= allWhite && isWhite;
			allBlack		= allBlack && isBlack;
			anyUnexpected	= anyUnexpected || (!isWhite && !isBlack);
		}
	}

	if (!allWhite)
	{
		if (anyUnexpected)
			log << TestLog::Message << "WARNING: expecting all rendered pixels to be white or black, but got other colors as well!" << TestLog::EndMessage;
		else if (!allBlack)
			log << TestLog::Message << "WARNING: got inconsistent results over the image, when all pixels should be the same color!" << TestLog::EndMessage;

		return false;
	}
	return true;
}

bool ShaderCase::execute (void)
{
	const float										quadSize				= 1.0f;
	static const float								s_positions[4*4]		=
	{
		-quadSize, -quadSize, 0.0f, 1.0f,
		-quadSize, +quadSize, 0.0f, 1.0f,
		+quadSize, -quadSize, 0.0f, 1.0f,
		+quadSize, +quadSize, 0.0f, 1.0f
	};

	static const deUint16							s_indices[2*3]			=
	{
		0, 1, 2,
		1, 3, 2
	};

	TestLog&										log						= m_testCtx.getLog();
	const glw::Functions&							gl						= m_renderCtx.getFunctions();

	// Compute viewport.
	const tcu::RenderTarget&						renderTarget			= m_renderCtx.getRenderTarget();
	de::Random										rnd						(deStringHash(getName()));
	const int										width					= deMin32(renderTarget.getWidth(),	VIEWPORT_WIDTH);
	const int										height					= deMin32(renderTarget.getHeight(),	VIEWPORT_HEIGHT);
	const int										viewportX				= rnd.getInt(0, renderTarget.getWidth()  - width);
	const int										viewportY				= rnd.getInt(0, renderTarget.getHeight() - height);
	const int										numVerticesPerDraw		= 4;
	const bool										tessellationPresent		= isTessellationPresent();
	const bool										requiresFullGLSLES100	= anyProgramRequiresFullGLSLES100Specification();

	bool											allCompilesOk			= true;
	bool											allLinksOk				= true;
	const char*										failReason				= DE_NULL;

	deUint32										vertexProgramID			= -1;
	std::vector<deUint32>							pipelineProgramIDs;
	std::vector<de::SharedPtr<glu::ShaderProgram> >	programs;
	de::SharedPtr<glu::ProgramPipeline>				programPipeline;

	GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderCase::execute(): start");

	if (!m_separatePrograms)
	{
		de::SharedPtr<glu::ShaderProgram>	program		(new glu::ShaderProgram(m_renderCtx, m_programs[0].programSources));

		vertexProgramID = program->getProgram();
		pipelineProgramIDs.push_back(program->getProgram());
		programs.push_back(program);

		// Check that compile/link results are what we expect.

		DE_STATIC_ASSERT(glu::SHADERTYPE_VERTEX == 0);
		for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
			if (program->hasShader((glu::ShaderType)stage) && !program->getShaderInfo((glu::ShaderType)stage).compileOk)
				allCompilesOk = false;

		if (!program->getProgramInfo().linkOk)
			allLinksOk = false;

		log << *program;
	}
	else
	{
		// Separate programs
		for (int programNdx = 0; programNdx < (int)m_programs.size(); ++programNdx)
		{
			de::SharedPtr<glu::ShaderProgram> program(new glu::ShaderProgram(m_renderCtx, m_programs[programNdx].programSources));

			if (m_programs[programNdx].spec.activeStageBits & (1 << glu::SHADERTYPE_VERTEX))
				vertexProgramID = program->getProgram();

			pipelineProgramIDs.push_back(program->getProgram());
			programs.push_back(program);

			// Check that compile/link results are what we expect.

			DE_STATIC_ASSERT(glu::SHADERTYPE_VERTEX == 0);
			for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
				if (program->hasShader((glu::ShaderType)stage) && !program->getShaderInfo((glu::ShaderType)stage).compileOk)
					allCompilesOk = false;

			if (!program->getProgramInfo().linkOk)
				allLinksOk = false;

			// Log program and active stages
			{
				const tcu::ScopedLogSection	section		(log, "Program", "Program " + de::toString(programNdx+1));
				tcu::MessageBuilder			builder		(&log);
				bool						firstStage	= true;

				builder << "Pipeline uses stages: ";
				for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
				{
					if (m_programs[programNdx].spec.activeStageBits & (1 << stage))
					{
						if (!firstStage)
							builder << ", ";
						builder << glu::getShaderTypeName((glu::ShaderType)stage);
						firstStage = true;
					}
				}
				builder << tcu::TestLog::EndMessage;

				log << *program;
			}
		}
	}

	switch (m_expectResult)
	{
		case EXPECT_PASS:
		case EXPECT_VALIDATION_FAIL:
		case EXPECT_BUILD_SUCCESSFUL:
			if (!allCompilesOk)
				failReason = "expected shaders to compile and link properly, but failed to compile.";
			else if (!allLinksOk)
				failReason = "expected shaders to compile and link properly, but failed to link.";
			break;

		case EXPECT_COMPILE_FAIL:
			if (allCompilesOk && !allLinksOk)
				failReason = "expected compilation to fail, but shaders compiled and link failed.";
			else if (allCompilesOk)
				failReason = "expected compilation to fail, but shaders compiled correctly.";
			break;

		case EXPECT_LINK_FAIL:
			if (!allCompilesOk)
				failReason = "expected linking to fail, but unable to compile.";
			else if (allLinksOk)
				failReason = "expected linking to fail, but passed.";
			break;

		case EXPECT_COMPILE_LINK_FAIL:
			if (allCompilesOk && allLinksOk)
				failReason = "expected compile or link to fail, but passed.";
			break;

		default:
			DE_ASSERT(false);
			return false;
	}

	if (failReason != DE_NULL)
	{
		// \todo [2010-06-07 petri] These should be handled in the test case?
		log << TestLog::Message << "ERROR: " << failReason << TestLog::EndMessage;

		if (requiresFullGLSLES100)
		{
			log	<< TestLog::Message
				<< "Assuming build failure is caused by implementation not supporting full GLSL ES 100 specification, which is not required."
				<< TestLog::EndMessage;

			if (allCompilesOk && !allLinksOk)
			{
				// Used features are detectable at compile time. If implementation parses shader
				// at link time, report it as quality warning.
				m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, failReason);
			}
			else
				m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Full GLSL ES 100 is not supported");
		}
		else if (m_expectResult == EXPECT_COMPILE_FAIL && allCompilesOk && !allLinksOk)
		{
			// If implementation parses shader at link time, report it as quality warning.
			m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, failReason);
		}
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, failReason);
		return false;
	}

	// Return if shader is not intended to be run
	if (m_expectResult == EXPECT_COMPILE_FAIL		||
		m_expectResult == EXPECT_COMPILE_LINK_FAIL	||
		m_expectResult == EXPECT_LINK_FAIL			||
		m_expectResult == EXPECT_BUILD_SUCCESSFUL)
		return true;

	// Setup viewport.
	gl.viewport(viewportX, viewportY, width, height);

	if (m_separatePrograms)
	{
		programPipeline = de::SharedPtr<glu::ProgramPipeline>(new glu::ProgramPipeline(m_renderCtx));

		// Setup pipeline
		gl.bindProgramPipeline(programPipeline->getPipeline());
		for (int programNdx = 0; programNdx < (int)m_programs.size(); ++programNdx)
		{
			deUint32 shaderFlags = 0;
			for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
				if (m_programs[programNdx].spec.activeStageBits & (1 << stage))
					shaderFlags |= glu::getGLShaderTypeBit((glu::ShaderType)stage);

			programPipeline->useProgramStages(shaderFlags, pipelineProgramIDs[programNdx]);
		}

		programPipeline->activeShaderProgram(vertexProgramID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "setup pipeline");
	}
	else
	{
		// Start using program
		gl.useProgram(vertexProgramID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
	}

	// Fetch location for positions positions.
	int positionLoc = gl.getAttribLocation(vertexProgramID, "dEQP_Position");
	if (positionLoc == -1)
	{
		string errStr = string("no location found for attribute 'dEQP_Position'");
		TCU_FAIL(errStr.c_str());
	}

	// Iterate all value blocks.
	for (int blockNdx = 0; blockNdx < (int)m_valueBlocks.size(); blockNdx++)
	{
		const ValueBlock&	valueBlock		= m_valueBlocks[blockNdx];

		// always render at least one pass even if there is no input/output data
		const int			numRenderPasses	= (valueBlock.arrayLength == 0) ? (1) : (valueBlock.arrayLength);

		// Iterate all array sub-cases.
		for (int arrayNdx = 0; arrayNdx < numRenderPasses; arrayNdx++)
		{
			int							numValues			= (int)valueBlock.values.size();
			vector<VertexArrayBinding>	vertexArrays;
			int							attribValueNdx		= 0;
			vector<vector<float> >		attribValues		(numValues);
			glw::GLenum					postDrawError;
			BeforeDrawValidator			beforeDrawValidator	(gl,
															 (m_separatePrograms) ? (programPipeline->getPipeline())			: (vertexProgramID),
															 (m_separatePrograms) ? (BeforeDrawValidator::TARGETTYPE_PIPELINE)	: (BeforeDrawValidator::TARGETTYPE_PROGRAM));

			vertexArrays.push_back(va::Float(positionLoc, 4, numVerticesPerDraw, 0, &s_positions[0]));

			// Collect VA pointer for inputs
			for (int valNdx = 0; valNdx < numValues; valNdx++)
			{
				const ShaderCase::Value&	val			= valueBlock.values[valNdx];
				const char* const			valueName	= val.valueName.c_str();
				const DataType				dataType	= val.dataType;
				const int					scalarSize	= getDataTypeScalarSize(val.dataType);

				if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
				{
					// Replicate values four times.
					std::vector<float>& scalars = attribValues[attribValueNdx++];
					scalars.resize(numVerticesPerDraw * scalarSize);
					if (isDataTypeFloatOrVec(dataType) || isDataTypeMatrix(dataType))
					{
						for (int repNdx = 0; repNdx < numVerticesPerDraw; repNdx++)
							for (int ndx = 0; ndx < scalarSize; ndx++)
								scalars[repNdx*scalarSize + ndx] = val.elements[arrayNdx*scalarSize + ndx].float32;
					}
					else
					{
						// convert to floats.
						for (int repNdx = 0; repNdx < numVerticesPerDraw; repNdx++)
						{
							for (int ndx = 0; ndx < scalarSize; ndx++)
							{
								float v = (float)val.elements[arrayNdx*scalarSize + ndx].int32;
								DE_ASSERT(val.elements[arrayNdx*scalarSize + ndx].int32 == (int)v);
								scalars[repNdx*scalarSize + ndx] = v;
							}
						}
					}

					// Attribute name prefix.
					string attribPrefix = "";
					// \todo [2010-05-27 petri] Should latter condition only apply for vertex cases (or actually non-fragment cases)?
					if ((m_caseType == CASETYPE_FRAGMENT_ONLY) || (getDataTypeScalarType(dataType) != TYPE_FLOAT))
						attribPrefix = "a_";

					// Input always given as attribute.
					string attribName = attribPrefix + valueName;
					int attribLoc = gl.getAttribLocation(vertexProgramID, attribName.c_str());
					if (attribLoc == -1)
					{
						log << TestLog::Message << "Warning: no location found for attribute '" << attribName << "'" << TestLog::EndMessage;
						continue;
					}

					if (isDataTypeMatrix(dataType))
					{
						int numCols = getDataTypeMatrixNumColumns(dataType);
						int numRows = getDataTypeMatrixNumRows(dataType);
						DE_ASSERT(scalarSize == numCols*numRows);

						for (int i = 0; i < numCols; i++)
							vertexArrays.push_back(va::Float(attribLoc + i, numRows, numVerticesPerDraw, scalarSize*sizeof(float), &scalars[i * numRows]));
					}
					else
					{
						DE_ASSERT(isDataTypeFloatOrVec(dataType) || isDataTypeIntOrIVec(dataType) || isDataTypeUintOrUVec(dataType) || isDataTypeBoolOrBVec(dataType));
						vertexArrays.push_back(va::Float(attribLoc, scalarSize, numVerticesPerDraw, 0, &scalars[0]));
					}

					GLU_EXPECT_NO_ERROR(gl.getError(), "set vertex attrib array");
				}
			}

			GLU_EXPECT_NO_ERROR(gl.getError(), "before set uniforms");

			// set uniform values for outputs (refs).
			for (int valNdx = 0; valNdx < numValues; valNdx++)
			{
				const ShaderCase::Value&	val			= valueBlock.values[valNdx];
				const char* const			valueName	= val.valueName.c_str();

				if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
				{
					// Set reference value.
					string refName = string("ref_") + valueName;
					setUniformValue(gl, pipelineProgramIDs, refName, val, arrayNdx, m_testCtx.getLog());
					GLU_EXPECT_NO_ERROR(gl.getError(), "set reference uniforms");
				}
				else if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM)
				{
					setUniformValue(gl, pipelineProgramIDs, valueName, val, arrayNdx, m_testCtx.getLog());
					GLU_EXPECT_NO_ERROR(gl.getError(), "set uniforms");
				}
			}

			// Clear.
			gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "clear buffer");

			// Use program or pipeline
			if (m_separatePrograms)
				gl.useProgram(0);
			else
				gl.useProgram(vertexProgramID);

			// Draw.
			if (tessellationPresent)
			{
				gl.patchParameteri(GL_PATCH_VERTICES, 3);
				GLU_EXPECT_NO_ERROR(gl.getError(), "set patchParameteri(PATCH_VERTICES, 3)");
			}

			draw(m_renderCtx,
				 vertexProgramID,
				 (int)vertexArrays.size(),
				 &vertexArrays[0],
				 (tessellationPresent) ?
					(pr::Patches(DE_LENGTH_OF_ARRAY(s_indices), &s_indices[0])) :
					(pr::Triangles(DE_LENGTH_OF_ARRAY(s_indices), &s_indices[0])),
				 (m_expectResult == EXPECT_VALIDATION_FAIL) ?
					(&beforeDrawValidator) :
					(DE_NULL));

			postDrawError = gl.getError();

			if (m_expectResult == EXPECT_PASS)
			{
				// Read back results.
				Surface			surface			(width, height);
				const float		w				= s_positions[3];
				const int		minY			= deCeilFloatToInt32 (((-quadSize / w) * 0.5f + 0.5f) * height + 1.0f);
				const int		maxY			= deFloorFloatToInt32(((+quadSize / w) * 0.5f + 0.5f) * height - 0.5f);
				const int		minX			= deCeilFloatToInt32 (((-quadSize / w) * 0.5f + 0.5f) * width + 1.0f);
				const int		maxX			= deFloorFloatToInt32(((+quadSize / w) * 0.5f + 0.5f) * width - 0.5f);

				GLU_EXPECT_NO_ERROR(postDrawError, "draw");

				glu::readPixels(m_renderCtx, viewportX, viewportY, surface.getAccess());
				GLU_EXPECT_NO_ERROR(gl.getError(), "read pixels");

				if (!checkPixels(surface, minX, maxX, minY, maxY))
				{
					log << TestLog::Message << "INCORRECT RESULT for (value block " << (blockNdx+1) << " of " <<  (int)m_valueBlocks.size()
											<< ", sub-case " << arrayNdx+1 << " of " << valueBlock.arrayLength << "):"
						<< TestLog::EndMessage;

					log << TestLog::Message << "Failing shader input/output values:" << TestLog::EndMessage;
					dumpValues(valueBlock, arrayNdx);

					// Dump image on failure.
					log << TestLog::Image("Result", "Rendered result image", surface);

					gl.useProgram(0);
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
					return false;
				}
			}
			else if (m_expectResult == EXPECT_VALIDATION_FAIL)
			{
				log	<< TestLog::Message
					<< "Draw call generated error: "
					<< glu::getErrorStr(postDrawError) << " "
					<< ((postDrawError == GL_INVALID_OPERATION) ? ("(expected)") : ("(unexpected)")) << "\n"
					<< "Validate status: "
					<< glu::getBooleanStr(beforeDrawValidator.getValidateStatus()) << " "
					<< ((beforeDrawValidator.getValidateStatus() == GL_FALSE) ? ("(expected)") : ("(unexpected)")) << "\n"
					<< "Info log: "
					<< ((beforeDrawValidator.getInfoLog().empty()) ? ("[empty string]") : (beforeDrawValidator.getInfoLog())) << "\n"
					<< TestLog::EndMessage;

				// test result

				if (postDrawError != GL_NO_ERROR && postDrawError != GL_INVALID_OPERATION)
				{
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, ("Draw: got unexpected error: " + de::toString(glu::getErrorStr(postDrawError))).c_str());
					return false;
				}

				if (beforeDrawValidator.getValidateStatus() == GL_TRUE)
				{
					if (postDrawError == GL_NO_ERROR)
						m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "expected validation and rendering to fail but validation and rendering succeeded");
					else if (postDrawError == GL_INVALID_OPERATION)
						m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "expected validation and rendering to fail but validation succeeded (rendering failed as expected)");
					else
						DE_ASSERT(false);
					return false;
				}
				else if (beforeDrawValidator.getValidateStatus() == GL_FALSE && postDrawError == GL_NO_ERROR)
				{
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "expected validation and rendering to fail but rendering succeeded (validation failed as expected)");
					return false;
				}
				else if (beforeDrawValidator.getValidateStatus() == GL_FALSE && postDrawError == GL_INVALID_OPERATION)
				{
					// Validation does not depend on input values, no need to test all values
					return true;
				}
				else
					DE_ASSERT(false);
			}
			else
				DE_ASSERT(false);
		}
	}

	gl.useProgram(0);
	if (m_separatePrograms)
		gl.bindProgramPipeline(0);

	GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderCase::execute(): end");
	return true;
}

TestCase::IterateResult ShaderCase::iterate (void)
{
	// Initialize state to pass.
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	bool executeOk = execute();

	DE_ASSERT(executeOk ? m_testCtx.getTestResult() == QP_TEST_RESULT_PASS : m_testCtx.getTestResult() != QP_TEST_RESULT_PASS);
	DE_UNREF(executeOk);
	return TestCase::STOP;
}

static void generateExtensionStatements (std::ostringstream& buf, const std::vector<ShaderCase::CaseRequirement>& requirements, glu::ShaderType type)
{
	for (int ndx = 0; ndx < (int)requirements.size(); ++ndx)
		if (requirements[ndx].getType() == ShaderCase::CaseRequirement::REQUIREMENTTYPE_EXTENSION &&
			(requirements[ndx].getAffectedExtensionStageFlags() & (1 << (deUint32)type)) != 0)
			buf << "#extension " << requirements[ndx].getSupportedExtension() << " : require\n";
}

// Injects #extension XXX : require lines after the last preprocessor directive in the shader code. Does not support line continuations
static std::string injectExtensionRequirements (const std::string& baseCode, glu::ShaderType shaderType, const std::vector<ShaderCase::CaseRequirement>& requirements)
{
	std::istringstream	baseCodeBuf(baseCode);
	std::ostringstream	resultBuf;
	std::string			line;
	bool				firstNonPreprocessorLine = true;
	std::ostringstream	extensions;

	generateExtensionStatements(extensions, requirements, shaderType);

	// skip if no requirements
	if (extensions.str().empty())
		return baseCode;

	while (std::getline(baseCodeBuf, line))
	{
		// begins with '#'?
		const std::string::size_type	firstNonWhitespace		= line.find_first_not_of("\t ");
		const bool						isPreprocessorDirective	= (firstNonWhitespace != std::string::npos && line.at(firstNonWhitespace) == '#');

		// Inject #extensions
		if (!isPreprocessorDirective && firstNonPreprocessorLine)
		{
			firstNonPreprocessorLine = false;
			resultBuf << extensions.str();
		}

		resultBuf << line << "\n";
	}

	return resultBuf.str();
}

// This functions builds a matching vertex shader for a 'both' case, when
// the fragment shader is being tested.
// We need to build attributes and varyings for each 'input'.
string ShaderCase::genVertexShader (const ValueBlock& valueBlock) const
{
	ostringstream	res;
	const bool		usesInout	= usesShaderInoutQualifiers(m_targetVersion);
	const char*		vtxIn		= usesInout ? "in"	: "attribute";
	const char*		vtxOut		= usesInout ? "out"	: "varying";

	res << glu::getGLSLVersionDeclaration(m_targetVersion) << "\n";

	// Declarations (position + attribute/varying for each input).
	res << "precision highp float;\n";
	res << "precision highp int;\n";
	res << "\n";
	res << vtxIn << " highp vec4 dEQP_Position;\n";
	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value& val = valueBlock.values[ndx];
		if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
		{
			DataType	floatType	= getDataTypeFloatScalars(val.dataType);
			const char*	typeStr		= getDataTypeName(floatType);
			res << vtxIn << " " << typeStr << " a_" << val.valueName << ";\n";

			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				res << vtxOut << " " << typeStr << " " << val.valueName << ";\n";
			else
				res << vtxOut << " " << typeStr << " v_" << val.valueName << ";\n";
		}
	}
	res << "\n";

	// Main function.
	// - gl_Position = dEQP_Position;
	// - for each input: write attribute directly to varying
	res << "void main()\n";
	res << "{\n";
	res << "	gl_Position = dEQP_Position;\n";
	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value& val = valueBlock.values[ndx];
		if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
		{
			const string& name = val.valueName;
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				res << "	" << name << " = a_" << name << ";\n";
			else
				res << "	v_" << name << " = a_" << name << ";\n";
		}
	}

	res << "}\n";
	return res.str();
}

static void genCompareFunctions (ostringstream& stream, const ShaderCase::ValueBlock& valueBlock, bool useFloatTypes)
{
	bool cmpTypeFound[TYPE_LAST];
	for (int i = 0; i < TYPE_LAST; i++)
		cmpTypeFound[i] = false;

	for (int valueNdx = 0; valueNdx < (int)valueBlock.values.size(); valueNdx++)
	{
		const ShaderCase::Value& val = valueBlock.values[valueNdx];
		if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
			cmpTypeFound[(int)val.dataType] = true;
	}

	if (useFloatTypes)
	{
		if (cmpTypeFound[TYPE_BOOL])		stream << "bool isOk (float a, bool b) { return ((a > 0.5) == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC2])	stream << "bool isOk (vec2 a, bvec2 b) { return (greaterThan(a, vec2(0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC3])	stream << "bool isOk (vec3 a, bvec3 b) { return (greaterThan(a, vec3(0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC4])	stream << "bool isOk (vec4 a, bvec4 b) { return (greaterThan(a, vec4(0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_INT])			stream << "bool isOk (float a, int b)  { float atemp = a+0.5; return (float(b) <= atemp && atemp <= float(b+1)); }\n";
		if (cmpTypeFound[TYPE_INT_VEC2])	stream << "bool isOk (vec2 a, ivec2 b) { return (ivec2(floor(a + 0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_INT_VEC3])	stream << "bool isOk (vec3 a, ivec3 b) { return (ivec3(floor(a + 0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_INT_VEC4])	stream << "bool isOk (vec4 a, ivec4 b) { return (ivec4(floor(a + 0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_UINT])		stream << "bool isOk (float a, uint b) { float atemp = a+0.5; return (float(b) <= atemp && atemp <= float(b+1u)); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC2])	stream << "bool isOk (vec2 a, uvec2 b) { return (uvec2(floor(a + 0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC3])	stream << "bool isOk (vec3 a, uvec3 b) { return (uvec3(floor(a + 0.5)) == b); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC4])	stream << "bool isOk (vec4 a, uvec4 b) { return (uvec4(floor(a + 0.5)) == b); }\n";
	}
	else
	{
		if (cmpTypeFound[TYPE_BOOL])		stream << "bool isOk (bool a, bool b)   { return (a == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC2])	stream << "bool isOk (bvec2 a, bvec2 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC3])	stream << "bool isOk (bvec3 a, bvec3 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_BOOL_VEC4])	stream << "bool isOk (bvec4 a, bvec4 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_INT])			stream << "bool isOk (int a, int b)     { return (a == b); }\n";
		if (cmpTypeFound[TYPE_INT_VEC2])	stream << "bool isOk (ivec2 a, ivec2 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_INT_VEC3])	stream << "bool isOk (ivec3 a, ivec3 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_INT_VEC4])	stream << "bool isOk (ivec4 a, ivec4 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_UINT])		stream << "bool isOk (uint a, uint b)   { return (a == b); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC2])	stream << "bool isOk (uvec2 a, uvec2 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC3])	stream << "bool isOk (uvec3 a, uvec3 b) { return (a == b); }\n";
		if (cmpTypeFound[TYPE_UINT_VEC4])	stream << "bool isOk (uvec4 a, uvec4 b) { return (a == b); }\n";
	}

	if (cmpTypeFound[TYPE_FLOAT])		stream << "bool isOk (float a, float b, float eps) { return (abs(a-b) <= (eps*abs(b) + eps)); }\n";
	if (cmpTypeFound[TYPE_FLOAT_VEC2])	stream << "bool isOk (vec2 a, vec2 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_VEC3])	stream << "bool isOk (vec3 a, vec3 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_VEC4])	stream << "bool isOk (vec4 a, vec4 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";

	if (cmpTypeFound[TYPE_FLOAT_MAT2])		stream << "bool isOk (mat2 a, mat2 b, float eps) { vec2 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return all(lessThanEqual(diff, vec2(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT2X3])	stream << "bool isOk (mat2x3 a, mat2x3 b, float eps) { vec3 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return all(lessThanEqual(diff, vec3(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT2X4])	stream << "bool isOk (mat2x4 a, mat2x4 b, float eps) { vec4 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return all(lessThanEqual(diff, vec4(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT3X2])	stream << "bool isOk (mat3x2 a, mat3x2 b, float eps) { vec2 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), abs(a[2]-b[2])); return all(lessThanEqual(diff, vec2(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT3])		stream << "bool isOk (mat3 a, mat3 b, float eps) { vec3 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), abs(a[2]-b[2])); return all(lessThanEqual(diff, vec3(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT3X4])	stream << "bool isOk (mat3x4 a, mat3x4 b, float eps) { vec4 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), abs(a[2]-b[2])); return all(lessThanEqual(diff, vec4(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT4X2])	stream << "bool isOk (mat4x2 a, mat4x2 b, float eps) { vec2 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec2(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT4X3])	stream << "bool isOk (mat4x3 a, mat4x3 b, float eps) { vec3 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec3(eps))); }\n";
	if (cmpTypeFound[TYPE_FLOAT_MAT4])		stream << "bool isOk (mat4 a, mat4 b, float eps) { vec4 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec4(eps))); }\n";
}

static void genCompareOp (ostringstream& output, const char* dstVec4Var, const ShaderCase::ValueBlock& valueBlock, const char* nonFloatNamePrefix, const char* checkVarName)
{
	bool isFirstOutput = true;

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val			= valueBlock.values[ndx];
		const char*					valueName	= val.valueName.c_str();

		if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
		{
			// Check if we're only interested in one variable (then skip if not the right one).
			if (checkVarName && !deStringEqual(valueName, checkVarName))
				continue;

			// Prefix.
			if (isFirstOutput)
			{
				output << "bool RES = ";
				isFirstOutput = false;
			}
			else
				output << "RES = RES && ";

			// Generate actual comparison.
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				output << "isOk(" << valueName << ", ref_" << valueName << ", 0.05);\n";
			else
				output << "isOk(" << nonFloatNamePrefix << valueName << ", ref_" << valueName << ");\n";
		}
		// \note Uniforms are already declared in shader.
	}

	if (isFirstOutput)
		output << dstVec4Var << " = vec4(1.0);\n";	// \todo [petri] Should we give warning if not expect-failure case?
	else
		output << dstVec4Var << " = vec4(RES, RES, RES, 1.0);\n";
}

string ShaderCase::genFragmentShader (const ValueBlock& valueBlock) const
{
	ostringstream	shader;
	const bool		usesInout		= usesShaderInoutQualifiers(m_targetVersion);
	const bool		customColorOut	= usesInout;
	const char*		fragIn			= usesInout ? "in" : "varying";
	const char*		prec			= supportsFragmentHighp(m_targetVersion) ? "highp" : "mediump";

	shader << glu::getGLSLVersionDeclaration(m_targetVersion) << "\n";

	shader << "precision " << prec << " float;\n";
	shader << "precision " << prec << " int;\n";
	shader << "\n";

	if (customColorOut)
	{
		shader << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";
		shader << "\n";
	}

	genCompareFunctions(shader, valueBlock, true);
	shader << "\n";

	// Declarations (varying, reference for each output).
	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value& val = valueBlock.values[ndx];
		DataType	floatType		= getDataTypeFloatScalars(val.dataType);
		const char*	floatTypeStr	= getDataTypeName(floatType);
		const char*	refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
		{
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				shader << fragIn << " " << floatTypeStr << " " << val.valueName << ";\n";
			else
				shader << fragIn << " " << floatTypeStr << " v_" << val.valueName << ";\n";

			shader << "uniform " << refTypeStr << " ref_" << val.valueName << ";\n";
		}
	}

	shader << "\n";
	shader << "void main()\n";
	shader << "{\n";

	shader << "	";
	genCompareOp(shader, customColorOut ? "dEQP_FragColor" : "gl_FragColor", valueBlock, "v_", DE_NULL);

	shader << "}\n";
	return shader.str();
}

// Specialize a shader for the vertex shader test case.
string ShaderCase::specializeVertexShader (const char* src, const ValueBlock& valueBlock) const
{
	ostringstream	decl;
	ostringstream	setup;
	ostringstream	output;
	const bool		usesInout	= usesShaderInoutQualifiers(m_targetVersion);
	const char*		vtxIn		= usesInout ? "in"	: "attribute";
	const char*		vtxOut		= usesInout ? "out"	: "varying";

	// generated from "both" case
	DE_ASSERT(m_caseType == CASETYPE_VERTEX_ONLY);

	// Output (write out position).
	output << "gl_Position = dEQP_Position;\n";

	// Declarations (position + attribute for each input, varying for each output).
	decl << vtxIn << " highp vec4 dEQP_Position;\n";
	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value& val = valueBlock.values[ndx];
		const char*	valueName		= val.valueName.c_str();
		DataType	floatType		= getDataTypeFloatScalars(val.dataType);
		const char*	floatTypeStr	= getDataTypeName(floatType);
		const char*	refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
		{
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
			{
				decl << vtxIn << " " << floatTypeStr << " " << valueName << ";\n";
			}
			else
			{
				decl << vtxIn << " " << floatTypeStr << " a_" << valueName << ";\n";
				setup << refTypeStr << " " << valueName << " = " << refTypeStr << "(a_" << valueName << ");\n";
			}
		}
		else if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
		{
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				decl << vtxOut << " " << floatTypeStr << " " << valueName << ";\n";
			else
			{
				decl << vtxOut << " " << floatTypeStr << " v_" << valueName << ";\n";
				decl << refTypeStr << " " << valueName << ";\n";

				output << "v_" << valueName << " = " << floatTypeStr << "(" << valueName << ");\n";
			}
		}
	}

	// Shader specialization.
	map<string, string> params;
	params.insert(pair<string, string>("DECLARATIONS", decl.str()));
	params.insert(pair<string, string>("SETUP", setup.str()));
	params.insert(pair<string, string>("OUTPUT", output.str()));
	params.insert(pair<string, string>("POSITION_FRAG_COLOR", "gl_Position"));

	StringTemplate	tmpl	(src);
	const string	baseSrc	= tmpl.specialize(params);
	const string	withExt	= injectExtensionRequirements(baseSrc, SHADERTYPE_VERTEX, m_programs[0].spec.requirements);

	return withExt;
}

// Specialize a shader for the fragment shader test case.
string ShaderCase::specializeFragmentShader (const char* src, const ValueBlock& valueBlock) const
{
	ostringstream	decl;
	ostringstream	setup;
	ostringstream	output;

	const bool		usesInout		= usesShaderInoutQualifiers(m_targetVersion);
	const bool		customColorOut	= usesInout;
	const char*		fragIn			= usesInout			? "in"				: "varying";
	const char*		fragColor		= customColorOut	? "dEQP_FragColor"	: "gl_FragColor";

	// generated from "both" case
	DE_ASSERT(m_caseType == CASETYPE_FRAGMENT_ONLY);

	genCompareFunctions(decl, valueBlock, false);
	genCompareOp(output, fragColor, valueBlock, "", DE_NULL);

	if (customColorOut)
		decl << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[ndx];
		const char*					valueName		= val.valueName.c_str();
		DataType					floatType		= getDataTypeFloatScalars(val.dataType);
		const char*					floatTypeStr	= getDataTypeName(floatType);
		const char*					refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
		{
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
				decl << fragIn << " " << floatTypeStr << " " << valueName << ";\n";
			else
			{
				decl << fragIn << " " << floatTypeStr << " v_" << valueName << ";\n";
				std::string offset = isDataTypeIntOrIVec(val.dataType) ? " * 1.0025" : ""; // \todo [petri] bit of a hack to avoid errors in chop() due to varying interpolation
				setup << refTypeStr << " " << valueName << " = " << refTypeStr << "(v_" << valueName << offset << ");\n";
			}
		}
		else if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
		{
			decl << "uniform " << refTypeStr << " ref_" << valueName << ";\n";
			decl << refTypeStr << " " << valueName << ";\n";
		}
	}

	/* \todo [2010-04-01 petri] Check all outputs. */

	// Shader specialization.
	map<string, string> params;
	params.insert(pair<string, string>("DECLARATIONS", decl.str()));
	params.insert(pair<string, string>("SETUP", setup.str()));
	params.insert(pair<string, string>("OUTPUT", output.str()));
	params.insert(pair<string, string>("POSITION_FRAG_COLOR", fragColor));

	StringTemplate	tmpl	(src);
	const string	baseSrc	= tmpl.specialize(params);
	const string	withExt	= injectExtensionRequirements(baseSrc, SHADERTYPE_FRAGMENT, m_programs[0].spec.requirements);

	return withExt;
}

static map<string, string> generateVertexSpecialization (glu::GLSLVersion targetVersion, const ShaderCase::ValueBlock& valueBlock)
{
	const bool				usesInout	= usesShaderInoutQualifiers(targetVersion);
	const char*				vtxIn		= usesInout ? "in" : "attribute";
	ostringstream			decl;
	ostringstream			setup;
	map<string, string>		params;

	decl << vtxIn << " highp vec4 dEQP_Position;\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val		= valueBlock.values[ndx];
		const char*					typeStr	= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_INPUT)
		{
			if (getDataTypeScalarType(val.dataType) == TYPE_FLOAT)
			{
				decl << vtxIn << " " << typeStr << " " << val.valueName << ";\n";
			}
			else
			{
				DataType	floatType		= getDataTypeFloatScalars(val.dataType);
				const char*	floatTypeStr	= getDataTypeName(floatType);

				decl << vtxIn << " " << floatTypeStr << " a_" << val.valueName << ";\n";
				setup << typeStr << " " << val.valueName << " = " << typeStr << "(a_" << val.valueName << ");\n";
			}
		}
		else if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM &&
					val.valueName.find('.') == string::npos)
			decl << "uniform " << typeStr << " " << val.valueName << ";\n";
	}

	params.insert(pair<string, string>("VERTEX_DECLARATIONS",		decl.str()));
	params.insert(pair<string, string>("VERTEX_SETUP",				setup.str()));
	params.insert(pair<string, string>("VERTEX_OUTPUT",				string("gl_Position = dEQP_Position;\n")));
	return params;
}

static map<string, string> generateFragmentSpecialization (glu::GLSLVersion targetVersion, const ShaderCase::ValueBlock& valueBlock)
{
	const bool			usesInout		= usesShaderInoutQualifiers(targetVersion);
	const bool			customColorOut	= usesInout;
	const char*			fragColor		= customColorOut ? "dEQP_FragColor"	: "gl_FragColor";
	ostringstream		decl;
	ostringstream		output;
	map<string, string>	params;

	genCompareFunctions(decl, valueBlock, false);
	genCompareOp(output, fragColor, valueBlock, "", DE_NULL);

	if (customColorOut)
		decl << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[ndx];
		const char*					valueName		= val.valueName.c_str();
		const char*					refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_OUTPUT)
		{
			decl << "uniform " << refTypeStr << " ref_" << valueName << ";\n";
			decl << refTypeStr << " " << valueName << ";\n";
		}
		else if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM &&
					val.valueName.find('.') == string::npos)
		{
			decl << "uniform " << refTypeStr << " " << valueName << ";\n";
		}
	}

	params.insert(pair<string, string>("FRAGMENT_DECLARATIONS",		decl.str()));
	params.insert(pair<string, string>("FRAGMENT_OUTPUT",			output.str()));
	params.insert(pair<string, string>("FRAG_COLOR",				fragColor));
	return params;
}

static map<string, string> generateGeometrySpecialization (glu::GLSLVersion targetVersion, const ShaderCase::ValueBlock& valueBlock)
{
	ostringstream		decl;
	map<string, string>	params;

	DE_UNREF(targetVersion);

	decl << "layout (triangles) in;\n";
	decl << "layout (triangle_strip, max_vertices=3) out;\n";
	decl << "\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[ndx];
		const char*					valueName		= val.valueName.c_str();
		const char*					refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM &&
			val.valueName.find('.') == string::npos)
		{
			decl << "uniform " << refTypeStr << " " << valueName << ";\n";
		}
	}

	params.insert(pair<string, string>("GEOMETRY_DECLARATIONS",		decl.str()));
	return params;
}

static map<string, string> generateTessControlSpecialization (glu::GLSLVersion targetVersion, const ShaderCase::ValueBlock& valueBlock)
{
	ostringstream		decl;
	ostringstream		output;
	map<string, string>	params;

	DE_UNREF(targetVersion);

	decl << "layout (vertices=3) out;\n";
	decl << "\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[ndx];
		const char*					valueName		= val.valueName.c_str();
		const char*					refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM &&
			val.valueName.find('.') == string::npos)
		{
			decl << "uniform " << refTypeStr << " " << valueName << ";\n";
		}
	}

	output <<	"gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"gl_TessLevelInner[0] = 2.0;\n"
				"gl_TessLevelInner[1] = 2.0;\n"
				"gl_TessLevelOuter[0] = 2.0;\n"
				"gl_TessLevelOuter[1] = 2.0;\n"
				"gl_TessLevelOuter[2] = 2.0;\n"
				"gl_TessLevelOuter[3] = 2.0;";

	params.insert(pair<string, string>("TESSELLATION_CONTROL_DECLARATIONS",	decl.str()));
	params.insert(pair<string, string>("TESSELLATION_CONTROL_OUTPUT",		output.str()));
	return params;
}

static map<string, string> generateTessEvalSpecialization (glu::GLSLVersion targetVersion, const ShaderCase::ValueBlock& valueBlock)
{
	ostringstream		decl;
	ostringstream		output;
	map<string, string>	params;

	DE_UNREF(targetVersion);

	decl << "layout (triangles) in;\n";
	decl << "\n";

	for (int ndx = 0; ndx < (int)valueBlock.values.size(); ndx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[ndx];
		const char*					valueName		= val.valueName.c_str();
		const char*					refTypeStr		= getDataTypeName(val.dataType);

		if (val.storageType == ShaderCase::Value::STORAGE_UNIFORM &&
			val.valueName.find('.') == string::npos)
		{
			decl << "uniform " << refTypeStr << " " << valueName << ";\n";
		}
	}

	output <<	"gl_Position = gl_TessCoord[0] * gl_in[0].gl_Position + gl_TessCoord[1] * gl_in[1].gl_Position + gl_TessCoord[2] * gl_in[2].gl_Position;\n";

	params.insert(pair<string, string>("TESSELLATION_EVALUATION_DECLARATIONS",	decl.str()));
	params.insert(pair<string, string>("TESSELLATION_EVALUATION_OUTPUT",		output.str()));
	return params;
}

static void specializeShaders (glu::ProgramSources& dst, glu::ShaderType shaderType, const std::vector<std::string>& sources, const ShaderCase::ValueBlock& valueBlock, glu::GLSLVersion targetVersion, const std::vector<ShaderCase::CaseRequirement>& requirements, std::map<std::string, std::string> (*specializationGenerator)(glu::GLSLVersion, const ShaderCase::ValueBlock&))
{
	if (!sources.empty())
	{
		const std::map<std::string, std::string> specializationParams = specializationGenerator(targetVersion, valueBlock);

		for (int ndx = 0; ndx < (int)sources.size(); ++ndx)
		{
			const StringTemplate	tmpl			(sources[ndx]);
			const std::string		baseGLSLCode	= tmpl.specialize(specializationParams);
			const std::string		glslSource		= injectExtensionRequirements(baseGLSLCode, shaderType, requirements);

			dst << glu::ShaderSource(shaderType, glslSource);
		}
	}
}

void ShaderCase::specializeVertexShaders (glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const
{
	specializeShaders(dst, glu::SHADERTYPE_VERTEX, sources, valueBlock, m_targetVersion, requirements, generateVertexSpecialization);
}

void ShaderCase::specializeFragmentShaders (glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const
{
	specializeShaders(dst, glu::SHADERTYPE_FRAGMENT, sources, valueBlock, m_targetVersion, requirements, generateFragmentSpecialization);
}

void ShaderCase::specializeGeometryShaders (glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const
{
	specializeShaders(dst, glu::SHADERTYPE_GEOMETRY, sources, valueBlock, m_targetVersion, requirements, generateGeometrySpecialization);
}

void ShaderCase::specializeTessControlShaders (glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const
{
	specializeShaders(dst, glu::SHADERTYPE_TESSELLATION_CONTROL, sources, valueBlock, m_targetVersion, requirements, generateTessControlSpecialization);
}

void ShaderCase::specializeTessEvalShaders (glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const
{
	specializeShaders(dst, glu::SHADERTYPE_TESSELLATION_EVALUATION, sources, valueBlock, m_targetVersion, requirements, generateTessEvalSpecialization);
}

void ShaderCase::dumpValues (const ValueBlock& valueBlock, int arrayNdx)
{
	int numValues = (int)valueBlock.values.size();
	for (int valNdx = 0; valNdx < numValues; valNdx++)
	{
		const ShaderCase::Value&	val				= valueBlock.values[valNdx];
		const char*					valueName		= val.valueName.c_str();
		DataType					dataType		= val.dataType;
		int							scalarSize		= getDataTypeScalarSize(val.dataType);
		ostringstream				result;

		result << "    ";
		if (val.storageType == Value::STORAGE_INPUT)
			result << "input ";
		else if (val.storageType == Value::STORAGE_UNIFORM)
			result << "uniform ";
		else if (val.storageType == Value::STORAGE_OUTPUT)
			result << "expected ";

		result << getDataTypeName(dataType) << " " << valueName << ":";

		if (isDataTypeScalar(dataType))
			result << " ";
		if (isDataTypeVector(dataType))
			result << " [ ";
		else if (isDataTypeMatrix(dataType))
			result << "\n";

		if (isDataTypeScalarOrVector(dataType))
		{
			for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
			{
				int						elemNdx	= (val.arrayLength == 1) ? 0 : arrayNdx;
				const Value::Element&	e		= val.elements[elemNdx*scalarSize + scalarNdx];
				result << ((scalarNdx != 0) ? ", " : "");

				if (isDataTypeFloatOrVec(dataType))
					result << e.float32;
				else if (isDataTypeIntOrIVec(dataType))
					result << e.int32;
				else if (isDataTypeUintOrUVec(dataType))
					result << (deUint32)e.int32;
				else if (isDataTypeBoolOrBVec(dataType))
					result << (e.bool32 ? "true" : "false");
			}
		}
		else if (isDataTypeMatrix(dataType))
		{
			int numRows = getDataTypeMatrixNumRows(dataType);
			int numCols = getDataTypeMatrixNumColumns(dataType);
			for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
			{
				result << "       [ ";
				for (int colNdx = 0; colNdx < numCols; colNdx++)
				{
					int		elemNdx = (val.arrayLength == 1) ? 0 : arrayNdx;
					float	v		= val.elements[elemNdx*scalarSize + rowNdx*numCols + colNdx].float32;
					result << ((colNdx==0) ? "" : ", ") << v;
				}
				result << " ]\n";
			}
		}

		if (isDataTypeScalar(dataType))
			result << "\n";
		else if (isDataTypeVector(dataType))
			result << " ]\n";

		m_testCtx.getLog() << TestLog::Message << result.str() << TestLog::EndMessage;
	}
}

} // sl
} // gls
} // deqp
