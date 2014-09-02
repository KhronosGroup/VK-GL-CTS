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
 * \brief Multisample interpolation state query tests
 *//*--------------------------------------------------------------------*/

#include "es31fShaderMultisampleInterpolationStateQueryTests.hpp"
#include "tcuTestLog.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "glsStateQueryUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"


namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

enum VerifierType
{
	VERIFIER_GET_BOOLEAN = 0,
	VERIFIER_GET_INTEGER,
	VERIFIER_GET_FLOAT,
	VERIFIER_GET_INTEGER64,
};

static void verifyGreaterOrEqual (VerifierType verifier, glw::GLenum target, float minValue, Context& context)
{
	glu::CallLogWrapper gl(context.getRenderContext().getFunctions(), context.getTestContext().getLog());

	gl.enableLogging(true);

	switch (verifier)
	{
		case VERIFIER_GET_BOOLEAN:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLboolean>	value;

			gl.glGetBooleanv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getBoolean");

			if (!value.verifyValidity(context.getTestContext()))
				return;
			if (value != GL_TRUE && value != GL_FALSE)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Returned value is not a boolean"<< tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean");
				return;
			}

			if (value > 0.0f && value == GL_FALSE)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Expected GL_TRUE, got GL_FALSE" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_INTEGER:
		{
			const glw::GLint											refValue = (glw::GLint)deFloatFloor(minValue);
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint>	value;

			gl.glGetIntegerv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getInteger");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting greater or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value < refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_FLOAT:
		{
			const float														refValue = minValue;
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLfloat>	value;

			gl.glGetFloatv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getFloat");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting greater or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value < refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_INTEGER64:
		{
			const glw::GLint64												refValue = (glw::GLint64)deFloatFloor(minValue);
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint64>	value;

			gl.glGetInteger64v(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getInteger64");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting greater or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value < refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
}

static void verifyLessOrEqual (VerifierType verifier, glw::GLenum target, float minValue, Context& context)
{
	glu::CallLogWrapper gl(context.getRenderContext().getFunctions(), context.getTestContext().getLog());

	gl.enableLogging(true);

	switch (verifier)
	{
		case VERIFIER_GET_BOOLEAN:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLboolean>	value;

			gl.glGetBooleanv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getBoolean");

			if (!value.verifyValidity(context.getTestContext()))
				return;
			if (value != GL_TRUE && value != GL_FALSE)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Returned value is not a boolean"<< tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean");
				return;
			}

			if (value < 0.0f && value == GL_FALSE)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Expected GL_TRUE, got GL_FALSE" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_INTEGER:
		{
			const glw::GLint											refValue = (glw::GLint)deFloatCeil(minValue);
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint>	value;

			gl.glGetIntegerv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getInteger");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting less or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value > refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_FLOAT:
		{
			const float														refValue = minValue;
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLfloat>	value;

			gl.glGetFloatv(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getFloat");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting greater or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value > refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		case VERIFIER_GET_INTEGER64:
		{
			const glw::GLint64												refValue = (glw::GLint64)deFloatCeil(minValue);
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint64>	value;

			gl.glGetInteger64v(target, &value);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "getInteger64");

			if (!value.verifyValidity(context.getTestContext()))
				return;

			context.getTestContext().getLog() << tcu::TestLog::Message << "Expecting greater or equal to " << refValue << ", got " << value << tcu::TestLog::EndMessage;
			if (value > refValue)
			{
				context.getTestContext().getLog() << tcu::TestLog::Message << "Value not in valid range" << tcu::TestLog::EndMessage;
				context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
			}
			break;
		}

		default:
			DE_ASSERT(false);
	}
}

class InterpolationOffsetCase : public TestCase
{
public:
	enum TestType
	{
		TEST_MIN_OFFSET = 0,
		TEST_MAX_OFFSET,

		TEST_LAST
	};

						InterpolationOffsetCase		(Context& context, const char* name, const char* desc, VerifierType verifier, TestType testType);
						~InterpolationOffsetCase	(void);

	void				init						(void);
	IterateResult		iterate						(void);

private:
	const VerifierType	m_verifier;
	const TestType		m_testType;
};

InterpolationOffsetCase::InterpolationOffsetCase (Context& context, const char* name, const char* desc, VerifierType verifier, TestType testType)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
	, m_testType	(testType)
{
	DE_ASSERT(m_testType < TEST_LAST);
}

InterpolationOffsetCase::~InterpolationOffsetCase (void)
{
}

void InterpolationOffsetCase::init (void)
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_shader_multisample_interpolation"))
		throw tcu::NotSupportedError("Test requires GL_OES_shader_multisample_interpolation extension");
}

InterpolationOffsetCase::IterateResult InterpolationOffsetCase::iterate (void)
{
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	if (m_testType == TEST_MAX_OFFSET)
		verifyGreaterOrEqual(m_verifier, GL_MAX_FRAGMENT_INTERPOLATION_OFFSET, 0.5, m_context);
	else if (m_testType == TEST_MIN_OFFSET)
		verifyLessOrEqual(m_verifier, GL_MIN_FRAGMENT_INTERPOLATION_OFFSET, -0.5, m_context);
	else
		DE_ASSERT(false);

	return STOP;
}

class FragmentInterpolationOffsetBitsCase : public TestCase
{
public:
						FragmentInterpolationOffsetBitsCase		(Context& context, const char* name, const char* desc, VerifierType verifier);
						~FragmentInterpolationOffsetBitsCase	(void);

	void				init									(void);
	IterateResult		iterate									(void);

private:
	const VerifierType	m_verifier;
};

FragmentInterpolationOffsetBitsCase::FragmentInterpolationOffsetBitsCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
}

FragmentInterpolationOffsetBitsCase::~FragmentInterpolationOffsetBitsCase (void)
{
}

void FragmentInterpolationOffsetBitsCase::init (void)
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_shader_multisample_interpolation"))
		throw tcu::NotSupportedError("Test requires GL_OES_shader_multisample_interpolation extension");
}

FragmentInterpolationOffsetBitsCase::IterateResult FragmentInterpolationOffsetBitsCase::iterate (void)
{
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	verifyGreaterOrEqual(m_verifier, GL_FRAGMENT_INTERPOLATION_OFFSET_BITS, 4.0, m_context);
	return STOP;
}

} // anonymous

ShaderMultisampleInterpolationStateQueryTests::ShaderMultisampleInterpolationStateQueryTests (Context& context)
	: TestCaseGroup(context, "multisample_interpolation", "Test multisample interpolation states")
{
}

ShaderMultisampleInterpolationStateQueryTests::~ShaderMultisampleInterpolationStateQueryTests (void)
{
}

void ShaderMultisampleInterpolationStateQueryTests::init (void)
{
	static const struct Verifier
	{
		VerifierType	verifier;
		const char*		name;
		const char*		desc;
	} verifiers[] =
	{
		{ VERIFIER_GET_BOOLEAN,		"get_boolean",		"Test using getBoolean"		},
		{ VERIFIER_GET_INTEGER,		"get_integer",		"Test using getInteger"		},
		{ VERIFIER_GET_FLOAT,		"get_float",		"Test using getFloat"		},
		{ VERIFIER_GET_INTEGER64,	"get_integer64",	"Test using getInteger64"	},
	};

	// .min_fragment_interpolation_offset
	{
		tcu::TestCaseGroup* const group = new tcu::TestCaseGroup(m_testCtx, "min_fragment_interpolation_offset", "Test MIN_FRAGMENT_INTERPOLATION_OFFSET");
		addChild(group);

		for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)
			group->addChild(new InterpolationOffsetCase(m_context, verifiers[verifierNdx].name, verifiers[verifierNdx].desc, verifiers[verifierNdx].verifier, InterpolationOffsetCase::TEST_MIN_OFFSET));
	}

	// .max_fragment_interpolation_offset
	{
		tcu::TestCaseGroup* const group = new tcu::TestCaseGroup(m_testCtx, "max_fragment_interpolation_offset", "Test MAX_FRAGMENT_INTERPOLATION_OFFSET");
		addChild(group);

		for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)
			group->addChild(new InterpolationOffsetCase(m_context, verifiers[verifierNdx].name, verifiers[verifierNdx].desc, verifiers[verifierNdx].verifier, InterpolationOffsetCase::TEST_MAX_OFFSET));
	}

	// .fragment_interpolation_offset_bits
	{
		tcu::TestCaseGroup* const group = new tcu::TestCaseGroup(m_testCtx, "fragment_interpolation_offset_bits", "Test FRAGMENT_INTERPOLATION_OFFSET_BITS");
		addChild(group);

		for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)
			group->addChild(new FragmentInterpolationOffsetBitsCase(m_context, verifiers[verifierNdx].name, verifiers[verifierNdx].desc, verifiers[verifierNdx].verifier));
	}
}

} // Functional
} // gles31
} // deqp
