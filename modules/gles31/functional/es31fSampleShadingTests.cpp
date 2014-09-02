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
 * \brief Sample shading tests
 *//*--------------------------------------------------------------------*/

#include "es31fSampleShadingTests.hpp"
#include "es31fMultisampleShaderRenderCase.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "glsStateQueryUtil.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "gluPixelTransfer.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include <map>

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

class SampleShadingStateCase : public TestCase
{
public:
	enum VerifierType
	{
		TYPE_IS_ENABLED = 0,
		TYPE_GET_BOOLEAN,
		TYPE_GET_INTEGER,
		TYPE_GET_FLOAT,
		TYPE_GET_INTEGER64,
		TYPE_LAST
	};

						SampleShadingStateCase	(Context& ctx, const char* name, const char* desc, VerifierType);

	void				init					(void);
	IterateResult		iterate					(void);

private:
	bool				verify					(bool v);

	const VerifierType	m_verifier;
};

SampleShadingStateCase::SampleShadingStateCase (Context& ctx, const char* name, const char* desc, VerifierType type)
	: TestCase		(ctx, name, desc)
	, m_verifier	(type)
{
	DE_ASSERT(m_verifier < TYPE_LAST);
}

void SampleShadingStateCase::init (void)
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_sample_shading"))
		throw tcu::NotSupportedError("Test requires GL_OES_sample_shading extension");
}

SampleShadingStateCase::IterateResult SampleShadingStateCase::iterate (void)
{
	bool				allOk	= true;
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gl.enableLogging(true);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	// initial
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying initial value" << tcu::TestLog::EndMessage;
		allOk &= verify(false);
	}

	// true and false too
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying random values" << tcu::TestLog::EndMessage;

		gl.glEnable(GL_SAMPLE_SHADING);
		allOk &= verify(true);

		gl.glDisable(GL_SAMPLE_SHADING);
		allOk &= verify(false);
	}

	if (!allOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected value");

	return STOP;
}

bool SampleShadingStateCase::verify (bool v)
{
	glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gl.enableLogging(true);

	switch (m_verifier)
	{
		case TYPE_IS_ENABLED:
		{
			const glw::GLboolean retVal = gl.glIsEnabled(GL_SAMPLE_SHADING);

			if ((v && retVal==GL_TRUE) || (!v && retVal==GL_FALSE))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v) ? ("GL_TRUE") : ("GL_FALSE")) << ", got " << ((retVal == GL_TRUE) ? ("GL_TRUE") : (retVal == GL_FALSE) ? ("GL_FALSE") : ("not-a-boolean")) << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_BOOLEAN:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLboolean> state;
			gl.glGetBooleanv(GL_SAMPLE_SHADING, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v && state==GL_TRUE) || (!v && state==GL_FALSE))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v) ? ("GL_TRUE") : ("GL_FALSE")) << ", got " << ((state == GL_TRUE) ? ("GL_TRUE") : (state == GL_FALSE) ? ("GL_FALSE") : ("not-a-boolean")) << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_INTEGER:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint> state;
			gl.glGetIntegerv(GL_SAMPLE_SHADING, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v && state==1) || (!v && state==0))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v) ? ("1") : ("0")) << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_FLOAT:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLfloat> state;
			gl.glGetFloatv(GL_SAMPLE_SHADING, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v && state==1.0f) || (!v && state==0.0f))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v) ? ("1.0") : ("0.0")) << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_INTEGER64:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint64> state;
			gl.glGetInteger64v(GL_SAMPLE_SHADING, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v && state==1) || (!v && state==0))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v) ? ("1") : ("0")) << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		default:
		{
			DE_ASSERT(false);
			return false;
		}
	}
}

class MinSampleShadingValueCase : public TestCase
{
public:
	enum VerifierType
	{
		TYPE_GET_BOOLEAN = 0,
		TYPE_GET_INTEGER,
		TYPE_GET_FLOAT,
		TYPE_GET_INTEGER64,
		TYPE_LAST
	};

						MinSampleShadingValueCase	(Context& ctx, const char* name, const char* desc, VerifierType);

	void				init						(void);
	IterateResult		iterate						(void);

private:
	bool				verify						(float v);

	const VerifierType	m_verifier;
};

MinSampleShadingValueCase::MinSampleShadingValueCase (Context& ctx, const char* name, const char* desc, VerifierType type)
	: TestCase		(ctx, name, desc)
	, m_verifier	(type)
{
	DE_ASSERT(m_verifier < TYPE_LAST);
}

void MinSampleShadingValueCase::init (void)
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_sample_shading"))
		throw tcu::NotSupportedError("Test requires GL_OES_sample_shading extension");
}

MinSampleShadingValueCase::IterateResult MinSampleShadingValueCase::iterate (void)
{
	bool				allOk	= true;
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gl.enableLogging(true);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	// initial
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying initial value" << tcu::TestLog::EndMessage;
		allOk &= verify(0.0);
	}

	// special values
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying special values" << tcu::TestLog::EndMessage;

		gl.glMinSampleShading(0.0f);
		allOk &= verify(0.0);

		gl.glMinSampleShading(1.0f);
		allOk &= verify(1.0);

		gl.glMinSampleShading(0.5f);
		allOk &= verify(0.5);
	}

	// random values
	{
		const int	numRandomTests	= 10;
		de::Random	rnd				(0xde123);

		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying random values" << tcu::TestLog::EndMessage;

		for (int randNdx = 0; randNdx < numRandomTests; ++randNdx)
		{
			const float value = rnd.getFloat();

			gl.glMinSampleShading(value);
			allOk &= verify(value);
		}
	}

	if (!allOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected value");

	return STOP;
}

bool MinSampleShadingValueCase::verify (float v)
{
	glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gl.enableLogging(true);

	switch (m_verifier)
	{
		case TYPE_GET_BOOLEAN:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLboolean> state;
			gl.glGetBooleanv(GL_MIN_SAMPLE_SHADING_VALUE, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v!=0.0f && state==GL_TRUE) || (v==0.0f && state==GL_FALSE))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v!=0.0f) ? ("GL_TRUE") : ("GL_FALSE")) << ", got " << ((state == GL_TRUE) ? ("GL_TRUE") : (state == GL_FALSE) ? ("GL_FALSE") : ("not-a-boolean")) << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_INTEGER:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint> state;
			gl.glGetIntegerv(GL_MIN_SAMPLE_SHADING_VALUE, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v>=0.5f && state==1) || (v<=0.5f && state==0))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v==0.5) ? ("0 or 1") : (v<0.5) ? ("0") : ("1")) << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_FLOAT:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLfloat> state;
			gl.glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if (v == state)
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << v << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		case TYPE_GET_INTEGER64:
		{
			gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint64> state;
			gl.glGetInteger64v(GL_MIN_SAMPLE_SHADING_VALUE, &state);

			if (!state.verifyValidity(m_testCtx))
				return false;

			if ((v>=0.5f && state==1) || (v<=0.5f && state==0))
				return true;

			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << ((v==0.5) ? ("0 or 1") : (v<0.5) ? ("0") : ("1")) << ", got " << state << tcu::TestLog::EndMessage;
			return false;
		}

		default:
		{
			DE_ASSERT(false);
			return false;
		}
	}
}

class MinSampleShadingValueClampingCase : public TestCase
{
public:
						MinSampleShadingValueClampingCase	(Context& ctx, const char* name, const char* desc);

	void				init								(void);
	IterateResult		iterate								(void);

private:
	bool				verify								(float v);
};

MinSampleShadingValueClampingCase::MinSampleShadingValueClampingCase (Context& ctx, const char* name, const char* desc)
	: TestCase(ctx, name, desc)
{
}

void MinSampleShadingValueClampingCase::init (void)
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_sample_shading"))
		throw tcu::NotSupportedError("Test requires GL_OES_sample_shading extension");
}

MinSampleShadingValueClampingCase::IterateResult MinSampleShadingValueClampingCase::iterate (void)
{
	bool				allOk	= true;
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gl.enableLogging(true);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	// special values
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying clamped values. Value is clamped when specified." << tcu::TestLog::EndMessage;

		gl.glMinSampleShading(-0.5f);
		allOk &= verify(0.0);

		gl.glMinSampleShading(-1.0f);
		allOk &= verify(0.0);

		gl.glMinSampleShading(-1.5f);
		allOk &= verify(0.0);

		gl.glMinSampleShading(1.5f);
		allOk &= verify(1.0);

		gl.glMinSampleShading(2.0f);
		allOk &= verify(1.0);

		gl.glMinSampleShading(2.5f);
		allOk &= verify(1.0);
	}

	if (!allOk && m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected value");

	return STOP;
}

bool MinSampleShadingValueClampingCase::verify (float v)
{
	glu::CallLogWrapper												gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLfloat>	state;

	gl.enableLogging(true);

	gl.glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &state);

	if (!state.verifyValidity(m_testCtx))
		return false;

	if (v == state)
		return true;

	m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << v << ", got " << state << tcu::TestLog::EndMessage;
	return false;
}

class SampleShadingRenderingCase : public MultisampleShaderRenderUtil::MultisampleRenderCase
{
public:
	enum TestType
	{
		TEST_DISCARD = 0,
		TEST_COLOR,

		TEST_LAST
	};
						SampleShadingRenderingCase	(Context& ctx, const char* name, const char* desc, RenderTarget target, int numSamples, TestType type);
						~SampleShadingRenderingCase	(void);

	void				init						(void);
private:
	void				setShadingValue				(int sampleCount);

	void				preDraw						(void);
	void				postDraw					(void);
	std::string			getIterationDescription		(int iteration) const;

	bool				verifyImage					(const tcu::Surface& resultImage);

	std::string			genFragmentSource			(int numSamples) const;

	enum
	{
		RENDER_SIZE = 128
	};

	const TestType		m_type;
};

SampleShadingRenderingCase::SampleShadingRenderingCase (Context& ctx, const char* name, const char* desc, RenderTarget target, int numSamples, TestType type)
	: MultisampleShaderRenderUtil::MultisampleRenderCase	(ctx, name, desc, numSamples, target, RENDER_SIZE)
	, m_type												(type)
{
	DE_ASSERT(type < TEST_LAST);
}

SampleShadingRenderingCase::~SampleShadingRenderingCase (void)
{
	deinit();
}

void SampleShadingRenderingCase::init (void)
{
	// requirements

	if (!m_context.getContextInfo().isExtensionSupported("GL_OES_sample_shading"))
		throw tcu::NotSupportedError("Test requires GL_OES_sample_shading extension");
	if (m_renderTarget == TARGET_DEFAULT && m_context.getRenderTarget().getNumSamples() <= 1)
		throw tcu::NotSupportedError("Multisampled default framebuffer required");

	// test purpose and expectations
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that a varying is given at least N different values for different samples within a single pixel.\n"
		<< "	Render high-frequency function, map result to black/white. Modify N with glMinSampleShading().\n"
		<< "	=> Resulting image should contain N+1 shades of gray.\n"
		<< tcu::TestLog::EndMessage;

	// setup resources

	MultisampleShaderRenderUtil::MultisampleRenderCase::init();

	// set iterations

	m_numIterations = m_numTargetSamples + 1;
}

void SampleShadingRenderingCase::setShadingValue (int sampleCount)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	if (sampleCount == 0)
	{
		gl.disable(GL_SAMPLE_SHADING);
		gl.minSampleShading(1.0f);
		GLU_EXPECT_NO_ERROR(gl.getError(), "set ratio");
	}
	else
	{
		// Minimum number of samples is max(ceil(<mss> * <samples>),1). Decrease mss with epsilon to prevent
		// ceiling to a too large sample count.
		const float epsilon	= 0.25f / (float)m_numTargetSamples;
		const float ratio	= (sampleCount / (float)m_numTargetSamples) - epsilon;

		gl.enable(GL_SAMPLE_SHADING);
		gl.minSampleShading(ratio);
		GLU_EXPECT_NO_ERROR(gl.getError(), "set ratio");

		m_testCtx.getLog()
			<< tcu::TestLog::Message
			<< "Setting MIN_SAMPLE_SHADING_VALUE = " << ratio << "\n"
			<< "Requested sample count: shadingValue * numSamples = " << ratio << " * " << m_numTargetSamples << " = " << (ratio * m_numTargetSamples) << "\n"
			<< "Minimum sample count: ceil(shadingValue * numSamples) = ceil(" << (ratio * m_numTargetSamples) << ") = " << sampleCount
			<< tcu::TestLog::EndMessage;

		// can't fail with reasonable values of numSamples
		DE_ASSERT(deFloatCeil(ratio * m_numTargetSamples) == float(sampleCount));
	}
}

void SampleShadingRenderingCase::preDraw (void)
{
	setShadingValue(m_iteration);
}

void SampleShadingRenderingCase::postDraw (void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.disable(GL_SAMPLE_SHADING);
	gl.minSampleShading(1.0f);
}

std::string	SampleShadingRenderingCase::getIterationDescription (int iteration) const
{
	if (iteration == 0)
		return "Disabled SAMPLE_SHADING";
	else
		return "Samples per pixel: " + de::toString(iteration);
}

bool SampleShadingRenderingCase::verifyImage (const tcu::Surface& resultImage)
{
	const int				numShadesRequired	= (m_iteration == 0) ? (2) : (m_iteration + 1);
	const int				rareThreshold		= 100;
	int						rareCount			= 0;
	std::map<deUint32, int>	shadeFrequency;

	// we should now have n+1 different shades of white, n = num samples

	m_testCtx.getLog()
		<< tcu::TestLog::Image("ResultImage", "Result Image", resultImage.getAccess())
		<< tcu::TestLog::Message
		<< "Verifying image has (at least) " << numShadesRequired << " different shades.\n"
		<< "Excluding pixels with no full coverage (pixels on the shared edge of the triangle pair)."
		<< tcu::TestLog::EndMessage;

	for (int y = 0; y < RENDER_SIZE; ++y)
	for (int x = 0; x < RENDER_SIZE; ++x)
	{
		const tcu::RGBA	color	= resultImage.getPixel(x, y);
		const deUint32	packed	= ((deUint32)color.getRed()) + ((deUint32)color.getGreen() << 8) + ((deUint32)color.getGreen() << 16);

		// on the triangle edge, skip
		if (x == y)
			continue;

		if (shadeFrequency.find(packed) == shadeFrequency.end())
			shadeFrequency[packed] = 1;
		else
			shadeFrequency[packed] = shadeFrequency[packed] + 1;
	}

	for (std::map<deUint32, int>::const_iterator it = shadeFrequency.begin(); it != shadeFrequency.end(); ++it)
		if (it->second < rareThreshold)
			rareCount++;

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Found " << (int)shadeFrequency.size() << " different shades.\n"
		<< "\tRare (less than " << rareThreshold << " pixels): " << rareCount << "\n"
		<< "\tCommon: " << (int)shadeFrequency.size() - rareCount << "\n"
		<< tcu::TestLog::EndMessage;

	if ((int)shadeFrequency.size() < numShadesRequired)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Image verification failed." << tcu::TestLog::EndMessage;
		return false;
	}
	return true;
}

std::string SampleShadingRenderingCase::genFragmentSource (int numSamples) const
{
	DE_UNREF(numSamples);

	std::ostringstream buf;

	buf <<	"#version 310 es\n"
			"in highp vec4 v_position;\n"
			"layout(location = 0) out mediump vec4 fragColor;\n"
			"void main (void)\n"
			"{\n"
			"	highp float field = dot(v_position.xy, v_position.xy) + dot(21.0 * v_position.xx, sin(3.1 * v_position.xy));\n"
			"	fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
			"\n"
			"	if (fract(field) > 0.5)\n";

	if (m_type == TEST_DISCARD)
		buf <<	"		discard;\n";
	else if (m_type == TEST_COLOR)
		buf <<	"		fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n";
	else
		DE_ASSERT(false);

	buf <<	"}";

	return buf.str();
}

} // anonymous

SampleShadingTests::SampleShadingTests (Context& context)
	: TestCaseGroup(context, "sample_shading", "Test sample shading")
{
}

SampleShadingTests::~SampleShadingTests (void)
{
}

void SampleShadingTests::init (void)
{
	tcu::TestCaseGroup* const stateQueryGroup = new tcu::TestCaseGroup(m_testCtx, "state_query", "State query tests.");
	tcu::TestCaseGroup* const minSamplesGroup = new tcu::TestCaseGroup(m_testCtx, "min_sample_shading", "Min sample shading tests.");

	addChild(stateQueryGroup);
	addChild(minSamplesGroup);

	// .state query
	{
		stateQueryGroup->addChild(new SampleShadingStateCase			(m_context, "sample_shading_is_enabled",				"test SAMPLE_SHADING",						SampleShadingStateCase::TYPE_IS_ENABLED));
		stateQueryGroup->addChild(new SampleShadingStateCase			(m_context, "sample_shading_get_boolean",				"test SAMPLE_SHADING",						SampleShadingStateCase::TYPE_GET_BOOLEAN));
		stateQueryGroup->addChild(new SampleShadingStateCase			(m_context, "sample_shading_get_integer",				"test SAMPLE_SHADING",						SampleShadingStateCase::TYPE_GET_INTEGER));
		stateQueryGroup->addChild(new SampleShadingStateCase			(m_context, "sample_shading_get_float",					"test SAMPLE_SHADING",						SampleShadingStateCase::TYPE_GET_FLOAT));
		stateQueryGroup->addChild(new SampleShadingStateCase			(m_context, "sample_shading_get_integer64",				"test SAMPLE_SHADING",						SampleShadingStateCase::TYPE_GET_INTEGER64));
		stateQueryGroup->addChild(new MinSampleShadingValueCase			(m_context, "min_sample_shading_value_get_boolean",		"test MIN_SAMPLE_SHADING_VALUE",			MinSampleShadingValueCase::TYPE_GET_BOOLEAN));
		stateQueryGroup->addChild(new MinSampleShadingValueCase			(m_context, "min_sample_shading_value_get_integer",		"test MIN_SAMPLE_SHADING_VALUE",			MinSampleShadingValueCase::TYPE_GET_INTEGER));
		stateQueryGroup->addChild(new MinSampleShadingValueCase			(m_context, "min_sample_shading_value_get_float",		"test MIN_SAMPLE_SHADING_VALUE",			MinSampleShadingValueCase::TYPE_GET_FLOAT));
		stateQueryGroup->addChild(new MinSampleShadingValueCase			(m_context, "min_sample_shading_value_get_integer64",	"test MIN_SAMPLE_SHADING_VALUE",			MinSampleShadingValueCase::TYPE_GET_INTEGER64));
		stateQueryGroup->addChild(new MinSampleShadingValueClampingCase	(m_context, "min_sample_shading_value_clamping",		"test MIN_SAMPLE_SHADING_VALUE clamping"));
	}

	// .min_sample_count
	{
		static const struct Target
		{
			SampleShadingRenderingCase::RenderTarget	target;
			int											numSamples;
			const char*									name;
		} targets[] =
		{
			{ SampleShadingRenderingCase::TARGET_DEFAULT,			0,	"default_framebuffer"					},
			{ SampleShadingRenderingCase::TARGET_TEXTURE,			2,	"multisample_texture_samples_2"			},
			{ SampleShadingRenderingCase::TARGET_TEXTURE,			4,	"multisample_texture_samples_4"			},
			{ SampleShadingRenderingCase::TARGET_TEXTURE,			8,	"multisample_texture_samples_8"			},
			{ SampleShadingRenderingCase::TARGET_TEXTURE,			16,	"multisample_texture_samples_16"		},
			{ SampleShadingRenderingCase::TARGET_RENDERBUFFER,		2,	"multisample_renderbuffer_samples_2"	},
			{ SampleShadingRenderingCase::TARGET_RENDERBUFFER,		4,	"multisample_renderbuffer_samples_4"	},
			{ SampleShadingRenderingCase::TARGET_RENDERBUFFER,		8,	"multisample_renderbuffer_samples_8"	},
			{ SampleShadingRenderingCase::TARGET_RENDERBUFFER,		16,	"multisample_renderbuffer_samples_16"	},
		};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(targets); ++ndx)
		{
			minSamplesGroup->addChild(new SampleShadingRenderingCase(m_context, (std::string(targets[ndx].name) + "_color").c_str(),	"Test multiple samples per pixel with color",	targets[ndx].target, targets[ndx].numSamples, SampleShadingRenderingCase::TEST_COLOR));
			minSamplesGroup->addChild(new SampleShadingRenderingCase(m_context, (std::string(targets[ndx].name) + "_discard").c_str(),	"Test multiple samples per pixel with",			targets[ndx].target, targets[ndx].numSamples, SampleShadingRenderingCase::TEST_DISCARD));
		}
	}
}

} // Functional
} // gles31
} // deqp
