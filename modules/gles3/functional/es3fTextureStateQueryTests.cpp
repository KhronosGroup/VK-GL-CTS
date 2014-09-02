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

#include "es3fTextureStateQueryTests.hpp"
#include "es3fApiCase.hpp"
#include "glsStateQueryUtil.hpp"
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
namespace TextureParamVerifiers
{

// TexParamVerifier

class TexParamVerifier : protected glu::CallLogWrapper
{
public:
						TexParamVerifier	(const glw::Functions& gl, tcu::TestLog& log, const char* testNamePostfix);
	virtual				~TexParamVerifier	(); // make GCC happy

	const char*			getTestNamePostfix	(void) const;

	virtual void		verifyInteger		(tcu::TestContext& testCtx, GLenum target, GLenum name, GLint reference)	= DE_NULL;
	virtual void		verifyFloat			(tcu::TestContext& testCtx, GLenum target, GLenum name, GLfloat reference)	= DE_NULL;
private:
	const char*	const	m_testNamePostfix;
};

TexParamVerifier::TexParamVerifier (const glw::Functions& gl, tcu::TestLog& log, const char* testNamePostfix)
	: glu::CallLogWrapper	(gl, log)
	, m_testNamePostfix		(testNamePostfix)
{
	enableLogging(true);
}
TexParamVerifier::~TexParamVerifier ()
{
}

const char* TexParamVerifier::getTestNamePostfix (void) const
{
	return m_testNamePostfix;
}

class GetTexParameterIVerifier : public TexParamVerifier
{
public:
			GetTexParameterIVerifier	(const glw::Functions& gl, tcu::TestLog& log);

	void	verifyInteger				(tcu::TestContext& testCtx, GLenum target, GLenum name, GLint reference);
	void	verifyFloat					(tcu::TestContext& testCtx, GLenum target, GLenum name, GLfloat reference);
};

GetTexParameterIVerifier::GetTexParameterIVerifier (const glw::Functions& gl, tcu::TestLog& log)
	: TexParamVerifier(gl, log, "_gettexparameteri")
{
}

void GetTexParameterIVerifier::verifyInteger (tcu::TestContext& testCtx, GLenum target, GLenum name, GLint reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLint> state;
	glGetTexParameteriv(target, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != reference)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid texture param value");
	}
}

void GetTexParameterIVerifier::verifyFloat (tcu::TestContext& testCtx, GLenum target, GLenum name, GLfloat reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLint> state;
	glGetTexParameteriv(target, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	const GLint expectedGLStateMax = StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint>(reference);
	const GLint expectedGLStateMin = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(reference);

	if (state < expectedGLStateMin || state > expectedGLStateMax)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << expectedGLStateMin << ", " << expectedGLStateMax << "]; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid texture param value");
	}
}

class GetTexParameterFVerifier : public TexParamVerifier
{
public:
			GetTexParameterFVerifier	(const glw::Functions& gl, tcu::TestLog& log);

	void	verifyInteger				(tcu::TestContext& testCtx, GLenum target, GLenum name, GLint reference);
	void	verifyFloat					(tcu::TestContext& testCtx, GLenum target, GLenum name, GLfloat reference);
};

GetTexParameterFVerifier::GetTexParameterFVerifier (const glw::Functions& gl, tcu::TestLog& log)
	: TexParamVerifier(gl, log, "_gettexparameterf")
{
}

void GetTexParameterFVerifier::verifyInteger (tcu::TestContext& testCtx, GLenum target, GLenum name, GLint reference)
{
	using tcu::TestLog;

	const GLfloat referenceAsFloat = GLfloat(reference);
	DE_ASSERT(reference == GLint(referenceAsFloat)); // reference integer must have 1:1 mapping to float for this to work. Reference value is always such value in these tests

	StateQueryMemoryWriteGuard<GLfloat> state;
	glGetTexParameterfv(target, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != referenceAsFloat)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsFloat << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
	}
}

void GetTexParameterFVerifier::verifyFloat (tcu::TestContext& testCtx, GLenum target, GLenum name, GLfloat reference)
{
	using tcu::TestLog;

	StateQueryMemoryWriteGuard<GLfloat> state;
	glGetTexParameterfv(target, name, &state);

	if (!state.verifyValidity(testCtx))
		return;

	if (state != reference)
	{
		testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state << TestLog::EndMessage;

		if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
			testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
	}
}

} // TextureParamVerifiers

namespace
{

using namespace TextureParamVerifiers;

// Tests

class IsTextureCase : public ApiCase
{
public:
	IsTextureCase (Context& context, const char* name, const char* description, GLenum textureTarget)
		: ApiCase			(context, name, description)
		, m_textureTarget	(textureTarget)
	{
	}

	void test (void)
	{
		using tcu::TestLog;

		GLuint textureId = 0;
		glGenTextures(1, &textureId);
		glBindTexture(m_textureTarget, textureId);
		expectError(GL_NO_ERROR);

		checkBooleans(glIsTexture(textureId), GL_TRUE);

		glDeleteTextures(1, &textureId);
		expectError(GL_NO_ERROR);

		checkBooleans(glIsTexture(textureId), GL_FALSE);
	}

protected:
	GLenum										m_textureTarget;
};

class TextureCase : public ApiCase
{
public:
	TextureCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: ApiCase			(context, name, description)
		, m_textureTarget	(textureTarget)
		, m_verifier		(verifier)
	{
	}

	virtual void testTexture (void) = DE_NULL;

	void test (void)
	{
		GLuint textureId = 0;
		glGenTextures(1, &textureId);
		glBindTexture(m_textureTarget, textureId);
		expectError(GL_NO_ERROR);

		testTexture();

		glDeleteTextures(1, &textureId);
		expectError(GL_NO_ERROR);
	}

protected:
	GLenum				m_textureTarget;
	TexParamVerifier*	m_verifier;
};

class TextureSwizzleCase : public TextureCase
{
public:
	TextureSwizzleCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget, GLenum valueName, GLenum initialValue)
		: TextureCase		(context, verifier, name, description, textureTarget)
		, m_valueName		(valueName)
		, m_initialValue	(initialValue)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, m_initialValue);
		expectError(GL_NO_ERROR);

		const GLenum swizzleValues[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(swizzleValues); ++ndx)
		{
			glTexParameteri(m_textureTarget, m_valueName, swizzleValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, swizzleValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(swizzleValues); ++ndx)
		{
			glTexParameterf(m_textureTarget, m_valueName, (GLfloat)swizzleValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, swizzleValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}

private:
	GLenum m_valueName;
	GLenum m_initialValue;
};

class TextureWrapCase : public TextureCase
{
public:
	TextureWrapCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget, GLenum valueName)
		: TextureCase	(context, verifier, name, description, textureTarget)
		, m_valueName	(valueName)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, GL_REPEAT);
		expectError(GL_NO_ERROR);

		const GLenum wrapValues[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
		{
			glTexParameteri(m_textureTarget, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
		{
			glTexParameterf(m_textureTarget, m_valueName, (GLfloat)wrapValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, wrapValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}

private:
	GLenum	m_valueName;
};

class TextureMagFilterCase : public TextureCase
{
public:
	TextureMagFilterCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		expectError(GL_NO_ERROR);

		const GLenum magValues[] = {GL_NEAREST, GL_LINEAR};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
		{
			glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
		{
			glTexParameterf(m_textureTarget, GL_TEXTURE_MAG_FILTER, (GLfloat)magValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class TextureMinFilterCase : public TextureCase
{
public:
	TextureMinFilterCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		expectError(GL_NO_ERROR);

		const GLenum minValues[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
		{
			glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);
		}

		//check unit conversions with float

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
		{
			glTexParameterf(m_textureTarget, GL_TEXTURE_MIN_FILTER, (GLfloat)minValues[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class TextureLODCase : public TextureCase
{
public:
	TextureLODCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget, GLenum lodTarget, int initialValue)
		: TextureCase		(context, verifier, name, description, textureTarget)
		, m_lodTarget		(lodTarget)
		, m_initialValue	(initialValue)
	{
	}

	void testTexture (void)
	{
		de::Random rnd(0xabcdef);

		m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_lodTarget, m_initialValue);
		expectError(GL_NO_ERROR);

		const int numIterations = 60;
		for (int ndx = 0; ndx < numIterations; ++ndx)
		{
			const GLfloat ref = rnd.getFloat(-64000, 64000);

			glTexParameterf(m_textureTarget, m_lodTarget, ref);
			expectError(GL_NO_ERROR);

			m_verifier->verifyFloat(m_testCtx, m_textureTarget, m_lodTarget, ref);
			expectError(GL_NO_ERROR);
		}

		// check unit conversions with int

		for (int ndx = 0; ndx < numIterations; ++ndx)
		{
			const GLint ref = rnd.getInt(-64000, 64000);

			glTexParameteri(m_textureTarget, m_lodTarget, ref);
			expectError(GL_NO_ERROR);

			m_verifier->verifyFloat(m_testCtx, m_textureTarget, m_lodTarget, (GLfloat)ref);
			expectError(GL_NO_ERROR);
		}
	}
private:
	GLenum	m_lodTarget;
	int		m_initialValue;
};

class TextureLevelCase : public TextureCase
{
public:
	TextureLevelCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget, GLenum levelTarget, int initialValue)
		: TextureCase		(context, verifier, name, description, textureTarget)
		, m_levelTarget		(levelTarget)
		, m_initialValue	(initialValue)
	{
	}

	void testTexture (void)
	{
		de::Random rnd(0xabcdef);

		m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_levelTarget, m_initialValue);
		expectError(GL_NO_ERROR);

		const int numIterations = 60;
		for (int ndx = 0; ndx < numIterations; ++ndx)
		{
			const GLint ref = rnd.getInt(0, 64000);

			glTexParameteri(m_textureTarget, m_levelTarget, ref);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_levelTarget, ref);
			expectError(GL_NO_ERROR);
		}

		// check unit conversions with float

		const float nonSignificantOffsets[] = {-0.45f, -0.25f, 0, 0.45f}; // offsets O so that for any integers z in Z, o in O roundToClosestInt(z+o)==z

		const int numConversionIterations = 30;
		for (int ndx = 0; ndx < numConversionIterations; ++ndx)
		{
			const GLint ref = rnd.getInt(0, 64000);

			for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(nonSignificantOffsets); ++offsetNdx)
			{
				glTexParameterf(m_textureTarget, m_levelTarget, ((GLfloat)ref) + nonSignificantOffsets[offsetNdx]);
				expectError(GL_NO_ERROR);

				m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_levelTarget, ref);
				expectError(GL_NO_ERROR);
			}
		}
	}
private:
	GLenum	m_levelTarget;
	int		m_initialValue;
};

class TextureCompareModeCase : public TextureCase
{
public:
	TextureCompareModeCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		expectError(GL_NO_ERROR);

		const GLenum modes[] = {GL_COMPARE_REF_TO_TEXTURE, GL_NONE};
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
		{
			glTexParameteri(m_textureTarget, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);
		}

		// with float too

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(modes); ++ndx)
		{
			glTexParameterf(m_textureTarget, GL_TEXTURE_COMPARE_MODE, (GLfloat)modes[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_MODE, modes[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class TextureCompareFuncCase : public TextureCase
{
public:
	TextureCompareFuncCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		expectError(GL_NO_ERROR);

		const GLenum compareFuncs[] = {GL_LEQUAL, GL_GEQUAL, GL_LESS, GL_GREATER, GL_EQUAL, GL_NOTEQUAL, GL_ALWAYS, GL_NEVER};
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
		{
			glTexParameteri(m_textureTarget, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);
		}

		// with float too

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compareFuncs); ++ndx)
		{
			glTexParameterf(m_textureTarget, GL_TEXTURE_COMPARE_FUNC, (GLfloat)compareFuncs[ndx]);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_COMPARE_FUNC, compareFuncs[ndx]);
			expectError(GL_NO_ERROR);
		}
	}
};

class TextureImmutableLevelsCase : public TextureCase
{
public:
	TextureImmutableLevelsCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_IMMUTABLE_LEVELS, 0);
		expectError(GL_NO_ERROR);

		for (int level = 1; level <= 8; ++level)
		{
			GLuint textureID = 0;
			glGenTextures(1, &textureID);
			glBindTexture(m_textureTarget, textureID);
			expectError(GL_NO_ERROR);

			if (m_textureTarget == GL_TEXTURE_2D_ARRAY || m_textureTarget == GL_TEXTURE_3D)
				glTexStorage3D(m_textureTarget, level, GL_RGB8, 256, 256, 256);
			else
				glTexStorage2D(m_textureTarget, level, GL_RGB8, 256, 256);
			expectError(GL_NO_ERROR);

			m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_IMMUTABLE_LEVELS, level);
			glDeleteTextures(1, &textureID);
			expectError(GL_NO_ERROR);
		}
	}
};

class TextureImmutableFormatCase : public TextureCase
{
public:
	TextureImmutableFormatCase (Context& context, TexParamVerifier* verifier, const char* name, const char* description, GLenum textureTarget)
		: TextureCase(context, verifier, name, description, textureTarget)
	{
	}

	void testTexture (void)
	{
		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_IMMUTABLE_FORMAT, 0);
		expectError(GL_NO_ERROR);

		const GLenum formats[] =
		{
			GL_RGBA32I, GL_RGBA32UI, GL_RGBA16I, GL_RGBA16UI, GL_RGBA8, GL_RGBA8I,
			GL_RGBA8UI, GL_SRGB8_ALPHA8, GL_RGB10_A2, GL_RGB10_A2UI, GL_RGBA4,
			GL_RGB5_A1, GL_RGB8, GL_RGB565, GL_RG32I, GL_RG32UI, GL_RG16I, GL_RG16UI,
			GL_RG8, GL_RG8I, GL_RG8UI, GL_R32I, GL_R32UI, GL_R16I, GL_R16UI, GL_R8,
			GL_R8I, GL_R8UI,

			GL_RGBA32F, GL_RGBA16F, GL_RGBA8_SNORM, GL_RGB32F,
			GL_RGB32I, GL_RGB32UI, GL_RGB16F, GL_RGB16I, GL_RGB16UI, GL_RGB8_SNORM,
			GL_RGB8I, GL_RGB8UI, GL_SRGB8, GL_R11F_G11F_B10F, GL_RGB9_E5, GL_RG32F,
			GL_RG16F, GL_RG8_SNORM, GL_R32F, GL_R16F, GL_R8_SNORM,
		};

		const GLenum non3dFormats[] =
		{
			GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16,
			GL_DEPTH32F_STENCIL8, GL_DEPTH24_STENCIL8
		};

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); ++formatNdx)
			testSingleFormat(formats[formatNdx]);

		if (m_textureTarget != GL_TEXTURE_3D)
			for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(non3dFormats); ++formatNdx)
				testSingleFormat(non3dFormats[formatNdx]);
	}

	void testSingleFormat (GLenum format)
	{
		GLuint textureID = 0;
		glGenTextures(1, &textureID);
		glBindTexture(m_textureTarget, textureID);
		expectError(GL_NO_ERROR);

		if (m_textureTarget == GL_TEXTURE_2D_ARRAY || m_textureTarget == GL_TEXTURE_3D)
			glTexStorage3D(m_textureTarget, 1, format, 32, 32, 32);
		else
			glTexStorage2D(m_textureTarget, 1, format, 32, 32);
		expectError(GL_NO_ERROR);

		m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_IMMUTABLE_FORMAT, 1);
		glDeleteTextures(1, &textureID);
		expectError(GL_NO_ERROR);
	}
};


} // anonymous

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)												\
	for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++)	\
	{																							\
		TexParamVerifier* verifier = VERIFIERS[_verifierNdx];									\
		CODE_BLOCK;																				\
	}

TextureStateQueryTests::TextureStateQueryTests (Context& context)
	: TestCaseGroup		(context, "texture", "Texture State Query tests")
	, m_verifierInt		(DE_NULL)
	, m_verifierFloat	(DE_NULL)
{
}

TextureStateQueryTests::~TextureStateQueryTests (void)
{
	deinit();
}

void TextureStateQueryTests::init (void)
{
	using namespace TextureParamVerifiers;

	DE_ASSERT(m_verifierInt == DE_NULL);
	DE_ASSERT(m_verifierFloat == DE_NULL);

	m_verifierInt		= new GetTexParameterIVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
	m_verifierFloat		= new GetTexParameterFVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());

	TexParamVerifier* verifiers[] = {m_verifierInt, m_verifierFloat};

	const struct
	{
		const char*	name;
		GLenum		textureTarget;
	} textureTargets[] =
	{
		{ "texture_2d",			GL_TEXTURE_2D},
		{ "texture_3d",			GL_TEXTURE_3D},
		{ "texture_2d_array",	GL_TEXTURE_2D_ARRAY},
		{ "texture_cube_map",	GL_TEXTURE_CUBE_MAP}
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(textureTargets); ++ndx)
	{
		addChild(new IsTextureCase(m_context, (std::string(textureTargets[ndx].name) + "_is_texture").c_str(), "IsTexture",	textureTargets[ndx].textureTarget));

		FOR_EACH_VERIFIER(verifiers, addChild(new TextureSwizzleCase	(m_context, verifier, (std::string(textureTargets[ndx].name)	+ "_texture_swizzle_r"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_SWIZZLE_R",		textureTargets[ndx].textureTarget, GL_TEXTURE_SWIZZLE_R, GL_RED)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureSwizzleCase	(m_context, verifier, (std::string(textureTargets[ndx].name)	+ "_texture_swizzle_g"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_SWIZZLE_G",		textureTargets[ndx].textureTarget, GL_TEXTURE_SWIZZLE_G, GL_GREEN)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureSwizzleCase	(m_context, verifier, (std::string(textureTargets[ndx].name)	+ "_texture_swizzle_b"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_SWIZZLE_B",		textureTargets[ndx].textureTarget, GL_TEXTURE_SWIZZLE_B, GL_BLUE)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureSwizzleCase	(m_context, verifier, (std::string(textureTargets[ndx].name)	+ "_texture_swizzle_a"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_SWIZZLE_A",		textureTargets[ndx].textureTarget, GL_TEXTURE_SWIZZLE_A, GL_ALPHA)));

		FOR_EACH_VERIFIER(verifiers, addChild(new TextureWrapCase		(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_wrap_s"			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_S",		textureTargets[ndx].textureTarget, GL_TEXTURE_WRAP_S)));
		if (textureTargets[ndx].textureTarget == GL_TEXTURE_2D ||
			textureTargets[ndx].textureTarget == GL_TEXTURE_3D ||
			textureTargets[ndx].textureTarget == GL_TEXTURE_CUBE_MAP)
			FOR_EACH_VERIFIER(verifiers, addChild(new TextureWrapCase	(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_wrap_t"			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_T",		textureTargets[ndx].textureTarget, GL_TEXTURE_WRAP_T)));

		if (textureTargets[ndx].textureTarget == GL_TEXTURE_3D)
			FOR_EACH_VERIFIER(verifiers, addChild(new TextureWrapCase	(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_wrap_r"			+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_WRAP_R",		textureTargets[ndx].textureTarget, GL_TEXTURE_WRAP_R)));

		FOR_EACH_VERIFIER(verifiers, addChild(new TextureMagFilterCase	(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_mag_filter"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MAG_FILTER",	textureTargets[ndx].textureTarget)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureMinFilterCase	(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_min_filter"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MIN_FILTER",	textureTargets[ndx].textureTarget)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureLODCase		(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_min_lod"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MIN_LOD",		textureTargets[ndx].textureTarget, GL_TEXTURE_MIN_LOD, -1000)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureLODCase		(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_max_lod"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MAX_LOD",		textureTargets[ndx].textureTarget, GL_TEXTURE_MAX_LOD,  1000)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureLevelCase		(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_base_level"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_BASE_LEVEL",	textureTargets[ndx].textureTarget, GL_TEXTURE_BASE_LEVEL, 0)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureLevelCase		(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_max_level"		+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_MAX_LEVEL",		textureTargets[ndx].textureTarget, GL_TEXTURE_MAX_LEVEL, 1000)));

		FOR_EACH_VERIFIER(verifiers, addChild(new TextureCompareModeCase(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_compare_mode"	+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_COMPARE_MODE",	textureTargets[ndx].textureTarget)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureCompareFuncCase(m_context, verifier,	(std::string(textureTargets[ndx].name)	+ "_texture_compare_func"	+ verifier->getTestNamePostfix()).c_str(), "TEXTURE_COMPARE_FUNC",	textureTargets[ndx].textureTarget)));

		FOR_EACH_VERIFIER(verifiers, addChild(new TextureImmutableLevelsCase(m_context, verifier,	(std::string(textureTargets[ndx].name) + "_texture_immutable_levels" + verifier->getTestNamePostfix()).c_str(), "TEXTURE_IMMUTABLE_LEVELS",	textureTargets[ndx].textureTarget)));
		FOR_EACH_VERIFIER(verifiers, addChild(new TextureImmutableFormatCase(m_context, verifier,	(std::string(textureTargets[ndx].name) + "_texture_immutable_format" + verifier->getTestNamePostfix()).c_str(), "TEXTURE_IMMUTABLE_FORMAT",	textureTargets[ndx].textureTarget)));
	}
}

void TextureStateQueryTests::deinit (void)
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
