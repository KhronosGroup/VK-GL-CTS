/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief State Query test utils.
 *//*--------------------------------------------------------------------*/
#include "glsStateQueryUtil.hpp"
#include "tcuTestContext.hpp"
#include "tcuFormatUtil.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"

namespace deqp
{
namespace gls
{
namespace StateQueryUtil
{

static bool checkError (tcu::ResultCollector& result, glu::CallLogWrapper& gl, const char* msg)
{
	const glw::GLenum errorCode = gl.glGetError();

	if (errorCode == GL_NO_ERROR)
		return true;

	result.fail(std::string(msg) + ": glGetError() returned " + glu::getErrorStr(errorCode).toString());
	return false;
}

QueriedState::QueriedState (void)
	: m_type(DATATYPE_LAST)
{
}

QueriedState::QueriedState (glw::GLint v)
	: m_type(DATATYPE_INTEGER)
{
	m_v.vInt = v;
}

QueriedState::QueriedState (glw::GLint64 v)
	: m_type(DATATYPE_INTEGER64)
{
	m_v.vInt64 = v;
}

QueriedState::QueriedState (glw::GLboolean v)
	: m_type(DATATYPE_BOOLEAN)
{
	m_v.vBool = v;
}

QueriedState::QueriedState (glw::GLfloat v)
	: m_type(DATATYPE_FLOAT)
{
	m_v.vFloat = v;
}

QueriedState::QueriedState (glw::GLuint v)
	: m_type(DATATYPE_UNSIGNED_INTEGER)
{
	m_v.vUint = v;
}

QueriedState::QueriedState (const GLIntVec3& v)
	: m_type(DATATYPE_INTEGER_VEC3)
{
	m_v.vIntVec3[0] = v[0];
	m_v.vIntVec3[1] = v[1];
	m_v.vIntVec3[2] = v[2];
}

bool QueriedState::isUndefined (void) const
{
	return m_type == DATATYPE_LAST;
}

DataType QueriedState::getType (void) const
{
	return m_type;
}

glw::GLint& QueriedState::getIntAccess (void)
{
	DE_ASSERT(m_type == DATATYPE_INTEGER);
	return m_v.vInt;
}

glw::GLint64& QueriedState::getInt64Access (void)
{
	DE_ASSERT(m_type == DATATYPE_INTEGER64);
	return m_v.vInt64;
}

glw::GLboolean& QueriedState::getBoolAccess (void)
{
	DE_ASSERT(m_type == DATATYPE_BOOLEAN);
	return m_v.vBool;
}

glw::GLfloat& QueriedState::getFloatAccess (void)
{
	DE_ASSERT(m_type == DATATYPE_FLOAT);
	return m_v.vFloat;
}

glw::GLuint& QueriedState::getUintAccess (void)
{
	DE_ASSERT(m_type == DATATYPE_UNSIGNED_INTEGER);
	return m_v.vUint;
}

QueriedState::GLIntVec3& QueriedState::getIntVec3Access (void)
{
	DE_ASSERT(m_type == DATATYPE_INTEGER_VEC3);
	return m_v.vIntVec3;
}

// query

void queryState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, QueriedState& state)
{
	switch (type)
	{
		case QUERY_ISENABLED:
		{
			const glw::GLboolean value = gl.glIsEnabled(target);

			if (!checkError(result, gl, "glIsEnabled"))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_BOOLEAN:
		{
			StateQueryMemoryWriteGuard<glw::GLboolean> value;
			gl.glGetBooleanv(target, &value);

			if (!checkError(result, gl, "glGetBooleanv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetIntegerv(target, &value);

			if (!checkError(result, gl, "glGetIntegerv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_INTEGER64:
		{
			StateQueryMemoryWriteGuard<glw::GLint64> value;
			gl.glGetInteger64v(target, &value);

			if (!checkError(result, gl, "glGetInteger64v"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_FLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetFloatv(target, &value);

			if (!checkError(result, gl, "glGetFloatv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void queryIndexedState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, int index, QueriedState& state)
{
	switch (type)
	{
		case QUERY_INDEXED_BOOLEAN:
		{
			StateQueryMemoryWriteGuard<glw::GLboolean> value;
			gl.glGetBooleani_v(target, index, &value);

			if (!checkError(result, gl, "glGetBooleani_v"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_INDEXED_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetIntegeri_v(target, index, &value);

			if (!checkError(result, gl, "glGetIntegeri_v"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		case QUERY_INDEXED_INTEGER64:
		{
			StateQueryMemoryWriteGuard<glw::GLint64> value;
			gl.glGetInteger64i_v(target, index, &value);

			if (!checkError(result, gl, "glGetInteger64i_v"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void queryAttributeState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, int index, QueriedState& state)
{
	switch (type)
	{
		case QUERY_ATTRIBUTE_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetVertexAttribiv(index, target, &value);

			if (!checkError(result, gl, "glGetVertexAttribiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_ATTRIBUTE_FLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetVertexAttribfv(index, target, &value);

			if (!checkError(result, gl, "glGetVertexAttribfv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_ATTRIBUTE_PURE_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetVertexAttribIiv(index, target, &value);

			if (!checkError(result, gl, "glGetVertexAttribIiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_ATTRIBUTE_PURE_UNSIGNED_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLuint> value;
			gl.glGetVertexAttribIuiv(index, target, &value);

			if (!checkError(result, gl, "glGetVertexAttribIuiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

void queryFramebufferState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, glw::GLenum pname, QueriedState& state)
{
	switch (type)
	{
		case QUERY_FRAMEBUFFER_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetFramebufferParameteriv(target, pname, &value);

			if (!checkError(result, gl, "glGetVertexAttribiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

void queryProgramState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLuint program, glw::GLenum pname, QueriedState& state)
{
	switch (type)
	{
		case QUERY_PROGRAM_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetProgramiv(program, pname, &value);

			if (!checkError(result, gl, "glGetProgramiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_PROGRAM_INTEGER_VEC3:
		{
			StateQueryMemoryWriteGuard<glw::GLint[3]> value;
			gl.glGetProgramiv(program, pname, value);

			if (!checkError(result, gl, "glGetProgramiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

void queryPipelineState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLuint pipeline, glw::GLenum pname, QueriedState& state)
{
	switch (type)
	{
		case QUERY_PIPELINE_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetProgramPipelineiv(pipeline, pname, &value);

			if (!checkError(result, gl, "glGetProgramiv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

void queryTextureParamState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, glw::GLenum pname, QueriedState& state)
{
	switch (type)
	{
		case QUERY_TEXTURE_PARAM_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetTexParameteriv(target, pname, &value);

			if (!checkError(result, gl, "glGetTexParameteriv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_TEXTURE_PARAM_FLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetTexParameterfv(target, pname, &value);

			if (!checkError(result, gl, "glGetTexParameterfv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

void queryTextureLevelState (tcu::ResultCollector& result, glu::CallLogWrapper& gl, QueryType type, glw::GLenum target, int level, glw::GLenum pname, QueriedState& state)
{
	switch (type)
	{
		case QUERY_TEXTURE_LEVEL_INTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetTexLevelParameteriv(target, level, pname, &value);

			if (!checkError(result, gl, "glGetTexLevelParameteriv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		case QUERY_TEXTURE_LEVEL_FLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetTexLevelParameterfv(target, level, pname, &value);

			if (!checkError(result, gl, "glGetTexLevelParameterfv"))
				return;

			if (!value.verifyValidity(result))
				return;

			state = QueriedState(value);
			break;
		}
		default:
			DE_ASSERT(false);
	}
}

// verify

void verifyBoolean (tcu::ResultCollector& result, QueriedState& state, bool expected)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			const glw::GLboolean reference = expected ? GL_TRUE : GL_FALSE;
			if (state.getBoolAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << glu::getBooleanStr(reference) << ", got " << glu::getBooleanStr(state.getBoolAccess());
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER:
		{
			const glw::GLint reference = expected ? 1 : 0;
			if (state.getIntAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << ", got " << state.getIntAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			const glw::GLint64 reference = expected ? 1 : 0;
			if (state.getInt64Access() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << ", got " << state.getInt64Access();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			const glw::GLfloat reference = expected ? 1.0f : 0.0f;
			if (state.getFloatAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyInteger (tcu::ResultCollector& result, QueriedState& state, int expected)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			const glw::GLboolean reference = (expected == 0) ? (GL_FALSE) : (GL_TRUE);
			if (state.getBoolAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << glu::getBooleanStr(reference) << ", got " << glu::getBooleanStr(state.getBoolAccess());
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER:
		{
			const glw::GLint reference = expected;
			if (state.getIntAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference))
					<< ") , got " << state.getIntAccess() << "(" << de::toString(tcu::Format::Hex<8>(state.getIntAccess())) << ")";
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			const glw::GLint64 reference = (glw::GLint64)expected;
			if (state.getInt64Access() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference)) << "), got "
					<< state.getInt64Access() << "(" << de::toString(tcu::Format::Hex<8>(state.getInt64Access())) << ")";
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			const glw::GLint64 reference = (glw::GLfloat)expected;
			if (state.getFloatAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_UNSIGNED_INTEGER:
		{
			const glw::GLuint reference = (glw::GLuint)expected;
			if (state.getUintAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << reference << "(" << de::toString(tcu::Format::Hex<8>(reference)) << "), got "
					<< state.getInt64Access() << "(" << de::toString(tcu::Format::Hex<8>(state.getInt64Access())) << ")";
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyIntegerMin (tcu::ResultCollector& result, QueriedState& state, int minValue)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			if (minValue > 0 && state.getBoolAccess() != GL_TRUE)
			{
				std::ostringstream buf;
				buf << "Expected GL_TRUE, got GL_FALSE";
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER:
		{
			if (state.getIntAccess() < minValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << minValue << ", got " << state.getIntAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			if (state.getInt64Access() < minValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << minValue << ", got " << state.getInt64Access();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			if (state.getFloatAccess() < minValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << minValue << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyIntegerMax (tcu::ResultCollector& result, QueriedState& state, int maxValue)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			if (maxValue < 0 && state.getBoolAccess() != GL_TRUE)
			{
				std::ostringstream buf;
				buf << "Expected GL_TRUE, got GL_FALSE";
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER:
		{
			if (state.getIntAccess() > maxValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << maxValue << ", got " << state.getIntAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			if (state.getInt64Access() > maxValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << maxValue << ", got " << state.getInt64Access();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			if (state.getFloatAccess() > maxValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << maxValue << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyFloat (tcu::ResultCollector& result, QueriedState& state, float expected)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			const glw::GLboolean reference = (expected == 0.0f) ? (GL_FALSE) : (GL_TRUE);
			if (state.getBoolAccess() != reference)
			{
				std::ostringstream buf;
				buf << "Expected " << glu::getBooleanStr(reference) << ", got " << glu::getBooleanStr(state.getBoolAccess());
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER:
		{
			const glw::GLint refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(expected);
			const glw::GLint refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(expected);

			if (state.getIntAccess() < refValueMin ||
				state.getIntAccess() > refValueMax)
			{
				std::ostringstream buf;

				if (refValueMin == refValueMax)
					buf << "Expected " << refValueMin << ", got " << state.getIntAccess();
				else
					buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got " << state.getIntAccess();

				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			if (state.getFloatAccess() != expected)
			{
				std::ostringstream buf;
				buf << "Expected " << expected << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			const glw::GLint64 refValueMin = roundGLfloatToNearestIntegerHalfDown<glw::GLint64>(expected);
			const glw::GLint64 refValueMax = roundGLfloatToNearestIntegerHalfUp<glw::GLint64>(expected);

			if (state.getInt64Access() < refValueMin ||
				state.getInt64Access() > refValueMax)
			{
				std::ostringstream buf;

				if (refValueMin == refValueMax)
					buf << "Expected " << refValueMin << ", got " << state.getInt64Access();
				else
					buf << "Expected in range [" << refValueMin << ", " << refValueMax << "], got " << state.getInt64Access();

				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyFloatMin (tcu::ResultCollector& result, QueriedState& state, float minValue)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			if (minValue > 0.0f && state.getBoolAccess() != GL_TRUE)
				result.fail("expected GL_TRUE, got GL_FALSE");
			break;
		}

		case DATATYPE_INTEGER:
		{
			const glw::GLint refValue = roundGLfloatToNearestIntegerHalfDown<glw::GLint>(minValue);

			if (state.getIntAccess() < refValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << refValue << ", got " << state.getIntAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			if (state.getFloatAccess() < minValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << minValue << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			const glw::GLint64 refValue = roundGLfloatToNearestIntegerHalfDown<glw::GLint64>(minValue);

			if (state.getInt64Access() < refValue)
			{
				std::ostringstream buf;
				buf << "Expected greater or equal to " << refValue << ", got " << state.getInt64Access();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyFloatMax (tcu::ResultCollector& result, QueriedState& state, float maxValue)
{
	switch (state.getType())
	{
		case DATATYPE_BOOLEAN:
		{
			if (maxValue < 0.0f && state.getBoolAccess() != GL_TRUE)
				result.fail("expected GL_TRUE, got GL_FALSE");
			break;
		}

		case DATATYPE_INTEGER:
		{
			const glw::GLint refValue = roundGLfloatToNearestIntegerHalfUp<glw::GLint>(maxValue);

			if (state.getIntAccess() > refValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << refValue << ", got " << state.getIntAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_FLOAT:
		{
			if (state.getFloatAccess() > maxValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << maxValue << ", got " << state.getFloatAccess();
				result.fail(buf.str());
			}
			break;
		}

		case DATATYPE_INTEGER64:
		{
			const glw::GLint64 refValue = roundGLfloatToNearestIntegerHalfUp<glw::GLint64>(maxValue);

			if (state.getInt64Access() > refValue)
			{
				std::ostringstream buf;
				buf << "Expected less or equal to " << refValue << ", got " << state.getInt64Access();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyIntegerVec3 (tcu::ResultCollector& result, QueriedState& state, const tcu::IVec3& expected)
{
	switch (state.getType())
	{
		case DATATYPE_INTEGER_VEC3:
		{
			if (state.getIntVec3Access()[0] != expected[0] ||
				state.getIntVec3Access()[1] != expected[1] ||
				state.getIntVec3Access()[2] != expected[2])
			{
				std::ostringstream buf;
				buf << "Expected " << expected << ", got " << state.getIntVec3Access();
				result.fail(buf.str());
			}
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

// helpers

void verifyStateBoolean (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, bool refValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyBoolean(result, state, refValue);
}

void verifyStateInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int refValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyInteger(result, state, refValue);
}

void verifyStateIntegerMin (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int minValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyIntegerMin(result, state, minValue);
}

void verifyStateIntegerMax (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int maxValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyIntegerMax(result, state, maxValue);
}

void verifyStateIntegerEqualToOther (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, glw::GLenum other, QueryType type)
{
	QueriedState stateA;
	QueriedState stateB;

	queryState(result, gl, type, target, stateA);
	queryState(result, gl, type, other, stateB);

	if (stateA.isUndefined() || stateB.isUndefined())
		return;

	switch (type)
	{
		case QUERY_BOOLEAN:
		{
			if (stateA.getBoolAccess() != stateB.getBoolAccess())
				result.fail("expected equal results");
			break;
		}

		case QUERY_INTEGER:
		{
			if (stateA.getIntAccess() != stateB.getIntAccess())
				result.fail("expected equal results");
			break;
		}

		case QUERY_INTEGER64:
		{
			if (stateA.getInt64Access() != stateB.getInt64Access())
				result.fail("expected equal results");
			break;
		}

		case QUERY_FLOAT:
		{
			if (stateA.getFloatAccess() != stateB.getFloatAccess())
				result.fail("expected equal results");
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
			break;
	}
}

void verifyStateFloat (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, float reference, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyFloat(result, state, reference);
}

void verifyStateFloatMin (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, float minValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyFloatMin(result, state, minValue);
}

void verifyStateFloatMax (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, float maxValue, QueryType type)
{
	QueriedState state;

	queryState(result, gl, type, target, state);

	if (!state.isUndefined())
		verifyFloatMax(result, state, maxValue);
}

void verifyStateIndexedBoolean (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int index, bool expected, QueryType type)
{
	QueriedState state;

	queryIndexedState(result, gl, type, target, index, state);

	if (!state.isUndefined())
		verifyBoolean(result, state, expected);
}

void verifyStateIndexedInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int index, int expected, QueryType type)
{
	QueriedState state;

	queryIndexedState(result, gl, type, target, index, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

void verifyStateIndexedIntegerMin (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int index, int minValue, QueryType type)
{
	QueriedState state;

	queryIndexedState(result, gl, type, target, index, state);

	if (!state.isUndefined())
		verifyIntegerMin(result, state, minValue);
}

void verifyStateAttributeInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, int index, int expected, QueryType type)
{
	QueriedState state;

	queryAttributeState(result, gl, type, target, index, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

void verifyStateFramebufferInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, glw::GLenum pname, int expected, QueryType type)
{
	QueriedState state;

	queryFramebufferState(result, gl, type, target, pname, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

void verifyStateFramebufferIntegerMin (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, glw::GLenum pname, int minValue, QueryType type)
{
	QueriedState state;

	queryFramebufferState(result, gl, type, target, pname, state);

	if (!state.isUndefined())
		verifyIntegerMin(result, state, minValue);
}

void verifyStateProgramInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLuint program, glw::GLenum pname, int expected, QueryType type)
{
	QueriedState state;

	queryProgramState(result, gl, type, program, pname, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

void verifyStateProgramIntegerVec3 (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLuint program, glw::GLenum pname, const tcu::IVec3& expected, QueryType type)
{
	QueriedState state;

	queryProgramState(result, gl, type, program, pname, state);

	if (!state.isUndefined())
		verifyIntegerVec3(result, state, expected);
}

void verifyStatePipelineInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLuint pipeline, glw::GLenum pname, int expected, QueryType type)
{
	QueriedState state;

	queryPipelineState(result, gl, type, pipeline, pname, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

void verifyStateTextureParamInteger (tcu::ResultCollector& result, glu::CallLogWrapper& gl, glw::GLenum target, glw::GLenum pname, int expected, QueryType type)
{
	QueriedState state;

	queryTextureParamState(result, gl, type, target, pname, state);

	if (!state.isUndefined())
		verifyInteger(result, state, expected);
}

} // StateQueryUtil
} // gls
} // deqp
