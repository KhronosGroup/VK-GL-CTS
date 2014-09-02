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
 * \brief Integer state query tests
 *//*--------------------------------------------------------------------*/

#include "es31fIntegerStateQueryTests.hpp"
#include "tcuTestLog.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deRandom.hpp"
#include "glsStateQueryUtil.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

using gls::StateQueryUtil::StateQueryMemoryWriteGuard;

enum VerifierType
{
	VERIFIER_GETBOOLEAN = 0,
	VERIFIER_GETINTEGER,
	VERIFIER_GETINTEGER64,
	VERIFIER_GETFLOAT
};

static const char* getVerifierSuffix (VerifierType type)
{
	switch (type)
	{
		case VERIFIER_GETBOOLEAN:	return "getboolean";
		case VERIFIER_GETINTEGER:	return "getinteger";
		case VERIFIER_GETINTEGER64:	return "getinteger64";
		case VERIFIER_GETFLOAT:		return "getfloat";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static bool verifyValue (glu::CallLogWrapper& gl, glw::GLenum target, int refValue, VerifierType type)
{
	switch (type)
	{
		case VERIFIER_GETBOOLEAN:
		{
			StateQueryMemoryWriteGuard<glw::GLboolean> value;
			gl.glGetBooleanv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetBooleanv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value != ((refValue == 0) ? (GL_FALSE) : (GL_TRUE)))
			{
				gl.getLog() << tcu::TestLog::Message << "Expected " << ((refValue == 0) ? (GL_FALSE) : (GL_TRUE)) << ", got " << ((value == 0) ? (GL_FALSE) : (GL_TRUE)) << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetIntegerv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value != refValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected " << refValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER64:
		{
			StateQueryMemoryWriteGuard<glw::GLint64> value;
			gl.glGetInteger64v(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetInteger64v");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value != (glw::GLint64)refValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected " << refValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETFLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetFloatv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetFloatv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value != (glw::GLfloat)refValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected " << refValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static bool verifyMinValue (glu::CallLogWrapper& gl, glw::GLenum target, int minValue, VerifierType type)
{
	switch (type)
	{
		case VERIFIER_GETBOOLEAN:
		{
			StateQueryMemoryWriteGuard<glw::GLboolean> value;
			gl.glGetBooleanv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetBooleanv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (minValue > 0 && value == GL_FALSE)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected GL_TRUE, got GL_FALSE" << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetIntegerv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value < minValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected greater or equal to " << minValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER64:
		{
			StateQueryMemoryWriteGuard<glw::GLint64> value;
			gl.glGetInteger64v(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetInteger64v");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value < minValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected greater or equal to " << minValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETFLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetFloatv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetFloatv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value < minValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected greater or equal to " << minValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static bool verifyMaxValue (glu::CallLogWrapper& gl, glw::GLenum target, int maxValue, VerifierType type)
{
	switch (type)
	{
		case VERIFIER_GETBOOLEAN:
		{
			StateQueryMemoryWriteGuard<glw::GLboolean> value;
			gl.glGetBooleanv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetBooleanv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (maxValue < 0 && value == GL_FALSE)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected GL_TRUE, got GL_FALSE" << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER:
		{
			StateQueryMemoryWriteGuard<glw::GLint> value;
			gl.glGetIntegerv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value > maxValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected less or equal to " << maxValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETINTEGER64:
		{
			StateQueryMemoryWriteGuard<glw::GLint64> value;
			gl.glGetInteger64v(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetInteger64v");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value > maxValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected less or equal to " << maxValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		case VERIFIER_GETFLOAT:
		{
			StateQueryMemoryWriteGuard<glw::GLfloat> value;
			gl.glGetFloatv(target, &value);

			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetFloatv");

			if (value.isUndefined())
			{
				gl.getLog() << tcu::TestLog::Message << "Get* did not return a value." << tcu::TestLog::EndMessage;
				return false;
			}
			else if (value > maxValue)
			{
				gl.getLog() << tcu::TestLog::Message << "Expected less or equal to " << maxValue << ", got " << value << tcu::TestLog::EndMessage;
				return false;
			}
			else
				return true;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

class SampleMaskCase : public TestCase
{
public:
					SampleMaskCase	(Context& context, const char* name, const char* desc);

private:
	void			init			(void);
	IterateResult	iterate			(void);

	int				m_maxSampleMaskWords;
};

SampleMaskCase::SampleMaskCase (Context& context, const char* name, const char* desc)
	: TestCase				(context, name, desc)
	, m_maxSampleMaskWords	(-1)
{
}

void SampleMaskCase::init (void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.getIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &m_maxSampleMaskWords);
	m_testCtx.getLog() << tcu::TestLog::Message << "GL_MAX_SAMPLE_MASK_WORDS = " << m_maxSampleMaskWords << tcu::TestLog::EndMessage;
}

SampleMaskCase::IterateResult SampleMaskCase::iterate (void)
{
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	bool				error	= false;

	gl.enableLogging(true);

	// mask word count ok?
	if (m_maxSampleMaskWords <= 0)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Minimum value of GL_MAX_SAMPLE_MASK_WORDS is 1. Got " << m_maxSampleMaskWords << tcu::TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid limit value");
		return STOP;
	}

	// initial values
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int ndx = 0; ndx < m_maxSampleMaskWords; ++ndx)
		{
			glw::GLint word = 0;
			gl.glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, ndx, &word);

			if (word != -1)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "ERROR: Expected all bits set (-1), got " << word << tcu::TestLog::EndMessage;
				error = true;
			}
		}
	}

	// random masks
	{
		const int					numRandomTest	= 20;
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0x4312);

		for (int testNdx = 0; testNdx < numRandomTest; ++testNdx)
		{
			const glw::GLint	maskIndex		= (glw::GLint)(rnd.getUint32() % m_maxSampleMaskWords);
			glw::GLint			mask			= (glw::GLint)(rnd.getUint32());
			glw::GLint			queriedMask		= 0;

			gl.glSampleMaski(maskIndex, mask);
			gl.glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, maskIndex, &queriedMask);

			if (mask != queriedMask)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "ERROR: Expected " << mask << ", got " << queriedMask << tcu::TestLog::EndMessage;
				error = true;
			}
		}
	}

	if (!error)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid mask value");

	return STOP;
}

class MaxSamplesCase : public TestCase
{
public:
						MaxSamplesCase	(Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType);
private:
	IterateResult		iterate			(void);

	const glw::GLenum	m_target;
	const int			m_minValue;
	const VerifierType	m_verifierType;
};

MaxSamplesCase::MaxSamplesCase (Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType)
	: TestCase			(context, name, desc)
	, m_target			(target)
	, m_minValue		(minValue)
	, m_verifierType	(verifierType)
{
}

MaxSamplesCase::IterateResult MaxSamplesCase::iterate (void)
{
	glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());

	gl.enableLogging(true);

	if (verifyMinValue(gl, m_target, m_minValue, m_verifierType))
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Value not in legal range");
	return STOP;
}

class TexBindingCase : public TestCase
{
public:
						TexBindingCase	(Context& context, const char* name, const char* desc, glw::GLenum texTarget, glw::GLenum bindTarget, VerifierType verifierType);
private:
	void				init			(void);
	IterateResult		iterate			(void);

	const glw::GLenum	m_texTarget;
	const glw::GLenum	m_bindTarget;
	const VerifierType	m_verifierType;
};

TexBindingCase::TexBindingCase (Context& context, const char* name, const char* desc, glw::GLenum texTarget, glw::GLenum bindTarget, VerifierType verifierType)
	: TestCase			(context, name, desc)
	, m_texTarget		(texTarget)
	, m_bindTarget		(bindTarget)
	, m_verifierType	(verifierType)
{
}

void TexBindingCase::init (void)
{
	if (m_texTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY && !m_context.getContextInfo().isExtensionSupported("GL_OES_texture_storage_multisample_2d_array"))
		throw tcu::NotSupportedError("Test requires OES_texture_storage_multisample_2d_array extension");
}

TexBindingCase::IterateResult TexBindingCase::iterate (void)
{
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	bool				allOk	= true;

	gl.enableLogging(true);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial value");

		allOk &= verifyValue(gl, m_bindTarget, 0, m_verifierType);
	}

	// bind
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "bind", "After bind");

		glw::GLuint texture;

		gl.glGenTextures(1, &texture);
		gl.glBindTexture(m_texTarget, texture);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind texture");

		allOk &= verifyValue(gl, m_bindTarget, texture, m_verifierType);

		gl.glDeleteTextures(1, &texture);
	}

	// after delete
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "bind", "After delete");

		allOk &= verifyValue(gl, m_bindTarget, 0, m_verifierType);
	}

	if (allOk)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	return STOP;
}

class MinimumValueCase : public TestCase
{
public:
						MinimumValueCase	(Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType);
private:
	IterateResult		iterate				(void);

	const glw::GLenum	m_target;
	const int			m_minValue;
	const VerifierType	m_verifierType;
};

MinimumValueCase::MinimumValueCase (Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType)
	: TestCase			(context, name, desc)
	, m_target			(target)
	, m_minValue		(minValue)
	, m_verifierType	(verifierType)
{
}

MinimumValueCase::IterateResult MinimumValueCase::iterate (void)
{
	glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());

	gl.enableLogging(true);

	if (verifyMinValue(gl, m_target, m_minValue, m_verifierType))
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	return STOP;
}

class AlignmentCase : public TestCase
{
public:
						AlignmentCase	(Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType);
private:
	IterateResult		iterate			(void);

	const glw::GLenum	m_target;
	const int			m_minValue;
	const VerifierType	m_verifierType;
};

AlignmentCase::AlignmentCase (Context& context, const char* name, const char* desc, glw::GLenum target, int minValue, VerifierType verifierType)
	: TestCase			(context, name, desc)
	, m_target			(target)
	, m_minValue		(minValue)
	, m_verifierType	(verifierType)
{
}

AlignmentCase::IterateResult AlignmentCase::iterate (void)
{
	glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());

	gl.enableLogging(true);

	if (verifyMaxValue(gl, m_target, m_minValue, m_verifierType))
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	return STOP;
}

} // anonymous

IntegerStateQueryTests::IntegerStateQueryTests (Context& context)
	: TestCaseGroup(context, "integer", "Integer state query tests")
{
}

IntegerStateQueryTests::~IntegerStateQueryTests (void)
{
}

void IntegerStateQueryTests::init (void)
{
	// Verifiers
	const VerifierType verifiers[] = { VERIFIER_GETBOOLEAN, VERIFIER_GETINTEGER, VERIFIER_GETINTEGER64, VERIFIER_GETFLOAT };

#define FOR_EACH_VERIFIER(X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)	\
	{																						\
		const char* verifierSuffix = getVerifierSuffix(verifiers[verifierNdx]);				\
		const VerifierType verifier = verifiers[verifierNdx];								\
		this->addChild(X);																	\
	}

	// No additional verifiers for sample_mask_value
	this->addChild(new SampleMaskCase(m_context, "sample_mask_value", "Test sample mask value"));

	FOR_EACH_VERIFIER(new MaxSamplesCase(m_context,		(std::string() + "max_color_texture_samples_" + verifierSuffix).c_str(),				"Test GL_MAX_COLOR_TEXTURE_SAMPLES",			GL_MAX_COLOR_TEXTURE_SAMPLES,		1,	verifier))
	FOR_EACH_VERIFIER(new MaxSamplesCase(m_context,		(std::string() + "max_depth_texture_samples_" + verifierSuffix).c_str(),				"Test GL_MAX_DEPTH_TEXTURE_SAMPLES",			GL_MAX_DEPTH_TEXTURE_SAMPLES,		1,	verifier))
	FOR_EACH_VERIFIER(new MaxSamplesCase(m_context,		(std::string() + "max_integer_samples_" + verifierSuffix).c_str(),						"Test GL_MAX_INTEGER_SAMPLES",					GL_MAX_INTEGER_SAMPLES,				1,	verifier))

	FOR_EACH_VERIFIER(new TexBindingCase(m_context,		(std::string() + "texture_binding_2d_multisample_" + verifierSuffix).c_str(),			"Test TEXTURE_BINDING_2D_MULTISAMPLE",			GL_TEXTURE_2D_MULTISAMPLE,			GL_TEXTURE_BINDING_2D_MULTISAMPLE,			verifier))
	FOR_EACH_VERIFIER(new TexBindingCase(m_context,		(std::string() + "texture_binding_2d_multisample_array_" + verifierSuffix).c_str(),		"Test TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY",	GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY,	verifier))

	FOR_EACH_VERIFIER(new MinimumValueCase(m_context,	(std::string() + "max_vertex_attrib_relative_offset_" + verifierSuffix).c_str(),		"Test MAX_VERTEX_ATTRIB_RELATIVE_OFFSET",		GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET,	2047,	verifier))
	FOR_EACH_VERIFIER(new MinimumValueCase(m_context,	(std::string() + "max_vertex_attrib_bindings_" + verifierSuffix).c_str(),				"Test MAX_VERTEX_ATTRIB_BINDINGS",				GL_MAX_VERTEX_ATTRIB_BINDINGS,			16,		verifier))
	FOR_EACH_VERIFIER(new MinimumValueCase(m_context,	(std::string() + "max_vertex_attrib_stride_" + verifierSuffix).c_str(),					"Test MAX_VERTEX_ATTRIB_STRIDE",				GL_MAX_VERTEX_ATTRIB_STRIDE,			2048,	verifier))

	FOR_EACH_VERIFIER(new AlignmentCase(m_context,		(std::string() + "shader_storage_buffer_offset_alignment_" + verifierSuffix).c_str(),	"Test SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT",	GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,	256,	verifier))

#undef FOR_EACH_VERIFIER
}

} // Functional
} // gles31
} // deqp
