/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Texture State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fSamplerStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"
#include "deMath.h"

using namespace glw; // GLint and other GL types
using namespace deqp::gls;
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;


namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace SamplerParamVerifiers
{

class SamplerParamVerifier : protected glu::CallLogWrapper
{
public:
						SamplerParamVerifier	(const glw::Functions& gl, tcu::TestLog& log, const char* testNamePostfix);
	virtual				~SamplerParamVerifier	(); // make GCC happy

	const char*			getTestNamePostfix		(void) const;

	virtual void		verifyInteger			(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLint reference)	= DE_NULL;
	virtual void		verifyFloat				(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLfloat reference)	= DE_NULL;
private:
	const char*	const	m_testNamePostfix;
};

SamplerParamVerifier::SamplerParamVerifier (const glw::Functions& gl, tcu::TestLog& log, const char* testNamePostfix)
	: glu::CallLogWrapper	(gl, log)
	, m_testNamePostfix		(testNamePostfix)
{
	enableLogging(true);
}
SamplerParamVerifier::~SamplerParamVerifier ()
{
}

const char* SamplerParamVerifier::getTestNamePostfix (void) const
{
	return m_testNamePostfix;
}

class GetSamplerParameterIVerifier : public SamplerParamVerifier
{
public:
			GetSamplerParameterIVerifier	(const glw::Functions& gl, tcu::TestLog& log);

	void	verifyInteger							(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLint reference);
	void	verifyFloat								(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLfloat reference);
};

GetSamplerParameterIVerifier::GetSamplerParameterIVerifier (const glw::Functions& gl, tcu::TestLog& log)
	: SamplerParamVerifier(gl, log, "_getsamplerparameteri")
{
}

void GetSamplerParameterIVerifier::verifyInteger (tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLint reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLint> state;
	glGetSamplerParameteriv(sampler, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != reference)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid sampler param value");
	}
}

void GetSamplerParameterIVerifier::verifyFloat (tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLfloat reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLint> state;
	glGetSamplerParameteriv(sampler, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	const GLint expectedGLStateMax = StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint>(reference);
	const GLint expectedGLStateMin = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(reference);

	if (state < expectedGLStateMin || state > expectedGLStateMax)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << expectedGLStateMin << ", " << expectedGLStateMax << "]; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got sampler texture param value");
	}
}

class GetSamplerParameterFVerifier : public SamplerParamVerifier
{
public:
			GetSamplerParameterFVerifier	(const glw::Functions& gl, tcu::TestLog& log);

	void	verifyInteger							(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLint reference);
	void	verifyFloat								(tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLfloat reference);
};

GetSamplerParameterFVerifier::GetSamplerParameterFVerifier (const glw::Functions& gl, tcu::TestLog& log)
	: SamplerParamVerifier(gl, log, "_getsamplerparameterf")
{
}

void GetSamplerParameterFVerifier::verifyInteger (tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLint reference)
{
	using tcu::TestLog;

	const GLfloat referenceAsFloat = GLfloat(reference);
	DE_ASSERT(reference == GLint(referenceAsFloat)); // reference integer must have 1:1 mapping to float for this to work. Reference value is always such value in these tests

	StateQueryMemoryWriteGuard<GLfloat> state;
	glGetSamplerParameterfv(sampler, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != referenceAsFloat)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsFloat << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
	}
}

void GetSamplerParameterFVerifier::verifyFloat (tcu::TestContext& testCtx, GLuint sampler, GLenum name, GLfloat reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLfloat> state;
	glGetSamplerParameterfv(sampler, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != reference)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
	}
}

} // SamplerParamVerifiers

namespace
{

using namespace SamplerParamVerifiers;

// Tests

class SamplerCase : public ApiCase
{
public:
	SamplerCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description)
		: ApiCase			(context, name, description)
		, m_sampler			(0)
		, m_verifier		(verifier)
	{
	}

	virtual void testSampler (void) = DE_NULL;

	void test (void)
	{
		glGenSamplers(1, &m_sampler);
		expectError(GL_NO_ERROR);

		testSampler();

		glDeleteTextures(1, &m_sampler);
		expectError(GL_NO_ERROR);
	}

protected:
	GLuint					m_sampler;
	SamplerParamVerifier*	m_verifier;
};

class SamplerWrapCase : public SamplerCase
{
public:
	SamplerWrapCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description, GLenum valueName)
		: SamplerCase	(context, verifier, name, description)
		, m_valueName	(valueName)
	{
	}

	void testSampler (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_sampler, m_valueName, GL_REPEAT);
		expectError(GL_NO_ERROR);

		const GLenum wrapValues[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
		{
			glSamplerParameteri(m_sampler, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
		{
			glSamplerParameterf(m_sampler, m_valueName, (GLfloat)wrapValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}

private:
	GLenum	m_valueName;
};

class SamplerMagFilterCase : public SamplerCase
{
public:
	SamplerMagFilterCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description)
		: SamplerCase(context, verifier, name, description)
	{
	}

	void testSampler (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		expectError(GL_NO_ERROR);

		const GLenum magValues[] = {GL_NEAREST, GL_LINEAR};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
		{
			glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
		{
			glSamplerParameterf(m_sampler, GL_TEXTURE_MAG_FILTER, (GLfloat)magValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class SamplerMinFilterCase : public SamplerCase
{
public:
	SamplerMinFilterCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description)
		: SamplerCase(context, verifier, name, description)
	{
	}

	void testSampler (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		expectError(GL_NO_ERROR);

		const GLenum minValues[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
		{
			glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
		{
			glSamplerParameterf(m_sampler, GL_TEXTURE_MIN_FILTER, (GLfloat)minValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class SamplerLODCase : public SamplerCase
{
public:
	SamplerLODCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description, GLenum lodTarget, int initialValue)
		: SamplerCase		(context, verifier, name, description)
		, m_lodTarget		(lodTarget)
		, m_initialValue	(initialValue)
	{
	}

	void testSampler (void)
	{
		de::Random rnd(0xabcdef);

		m_verifier->verifyInteger(m_testCtx, m_sampler, m_lodTarget, m_initialValue);
		expectError(GL_NO_ERROR);

		const int numIterations = 60;
		for (int ndx = 0; ndx < numIterations; ++ndx)
		{
			const GLfloat ref = rnd.getFloat(-64000, 64000);

			glSamplerParameterf(m_sampler, m_lodTarget, ref);
			expectError(GL_NO_ERROR);

			m_verifier->verifyFloat(m_testCtx, m_sampler, m_lodTarget, ref);
			expectError(GL_NO_ERROR);
		}

		// check unit conversions with int

		for (int ndx = 0; ndx < numIterations; ++ndx)
		{
			const GLint ref = rnd.getInt(-64000, 64000);

			glSamplerParameteri(m_sampler, m_lodTarget, ref);
			expectError(GL_NO_ERROR);

			m_verifier->verifyFloat(m_testCtx, m_sampler, m_lodTarget, (GLfloat)ref);
			expectError(GL_NO_ERROR);
		}
	}
private:
	GLenum	m_lodTarget;
	int		m_initialValue;
};

class SamplerCompareModeCase : public SamplerCase
{
public:
	SamplerCompareModeCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description)
		: SamplerCase(context, verifier, name, description)
	{
	}

	void testSampler (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		expectError(GL_NO_ERROR);

		const GLenum modes[] = {GL_COMPARE_REF_TO_TEXTURE, GL_NONE};
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
		{
			glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);
		}

		// with float too

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
		{
			glSamplerParameterf(m_sampler, GL_TEXTURE_COMPARE_MODE, (GLfloat)modes[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class SamplerCompareFuncCase : public SamplerCase
{
public:
	SamplerCompareFuncCase (Context& context, SamplerParamVerifier* verifier, const char* name, const char* description)
		: SamplerCase(context, verifier, name, description)
	{
	}

	void testSampler (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		expectError(GL_NO_ERROR);

		const GLenum compareFuncs[] = {GL_LEQUAL, GL_GEQUAL, GL_LESS, GL_GREATER, GL_EQUAL, GL_NOTEQUAL, GL_ALWAYS, GL_NEVER};
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
		{
			glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);
		}

		// with float too

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
		{
			glSamplerParameterf(m_sampler, GL_TEXTURE_COMPARE_FUNC, (GLfloat)compareFuncs[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_sampler, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};



} // anonymous

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)												\
	for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++)	\
	{																							\
		SamplerParamVerifier* verifier = VERIFIERS[_verifierNdx];								\
		CODE_BLOCK;																				\
	}

SamplerStateQueryTests::SamplerStateQueryTests (Context& context)
	: TestCaseGroup		(context, "sampler", "Sampler State Query tests")
	, m_verifierInt		(DE_NULL)
	, m_verifierFloat	(DE_NULL)
{
}

SamplerStateQueryTests::~SamplerStateQueryTests (void)
{
	deinit();
}

void SamplerStateQueryTests::init (void)
{
	using namespace SamplerParamVerifiers;

	DE_ASSERT(m_verifierInt == DE_NULL);
	DE_ASSERT(m_verifierFloat == DE_NULL);

	m_verifierInt		= new GetSamplerParameterIVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
	m_verifierFloat		= new GetSamplerParameterFVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
	SamplerParamVerifier* verifiers[] = {m_verifierInt, m_verifierFloat};

	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerWrapCase		(m_context, verifier,	(std::string("sampler_texture_wrap_s")			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_S",			GL_TEXTURE_WRAP_S)));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerWrapCase		(m_context, verifier,	(std::string("sampler_texture_wrap_t")			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_T",			GL_TEXTURE_WRAP_T)));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerWrapCase		(m_context, verifier,	(std::string("sampler_texture_wrap_r")			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_R",			GL_TEXTURE_WRAP_R)));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerMagFilterCase	(m_context, verifier,	(std::string("sampler_texture_mag_filter")		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MAG_FILTER")));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerMinFilterCase	(m_context, verifier,	(std::string("sampler_texture_min_filter")		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MIN_FILTER")));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerLODCase		(m_context, verifier,	(std::string("sampler_texture_min_lod")			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MIN_LOD",			GL_TEXTURE_MIN_LOD, -1000)));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerLODCase		(m_context, verifier,	(std::string("sampler_texture_max_lod")			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MAX_LOD",			GL_TEXTURE_MAX_LOD,  1000)));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerCompareModeCase(m_context, verifier,	(std::string("sampler_texture_compare_mode")	+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_COMPARE_MODE")));
	FOR_EACH_VERIFIER(verifiers, addChild(new SamplerCompareFuncCase(m_context, verifier,	(std::string("sampler_texture_compare_func")	+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_COMPARE_FUNC")));
}

void SamplerStateQueryTests::deinit (void)
{
	if (m_verifierInt)
	{
		delete m_verifierInt;
		m_verifierInt = NULL;
	}
	if (m_verifierFloat)
	{
		delete m_verifierFloat;
		m_verifierFloat = NULL;
	}

	this->TestCaseGroup::deinit();
}

} // Functional
} // gles3
} // deqp
