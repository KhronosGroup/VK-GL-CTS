/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
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
 * \brief Indexed state query tests
 *//*--------------------------------------------------------------------*/

#include "es31fIndexedStateQueryTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluStrUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "glsStateQueryUtil.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

using namespace gls::StateQueryUtil;

static const char* getVerifierSuffix (QueryType type)
{
	switch (type)
	{
		case QUERY_INDEXED_BOOLEAN:		return "getbooleani_v";
		case QUERY_INDEXED_INTEGER:		return "getintegeri_v";
		case QUERY_INDEXED_INTEGER64:	return "getinteger64i_v";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

class SampleMaskCase : public TestCase
{
public:
						SampleMaskCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	void				init			(void);
	IterateResult		iterate			(void);

	const QueryType		m_verifierType;
	int					m_maxSampleMaskWords;
};

SampleMaskCase::SampleMaskCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase				(context, name, desc)
	, m_verifierType		(verifierType)
	, m_maxSampleMaskWords	(-1)
{
}

void SampleMaskCase::init (void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.getIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &m_maxSampleMaskWords);
	GLU_EXPECT_NO_ERROR(gl.getError(), "query sample mask words");

	// mask word count ok?
	if (m_maxSampleMaskWords <= 0)
		throw tcu::TestError("Minimum value of GL_MAX_SAMPLE_MASK_WORDS is 1. Got " + de::toString(m_maxSampleMaskWords));

	m_testCtx.getLog() << tcu::TestLog::Message << "GL_MAX_SAMPLE_MASK_WORDS = " << m_maxSampleMaskWords << tcu::TestLog::EndMessage;
}

SampleMaskCase::IterateResult SampleMaskCase::iterate (void)
{
	glu::CallLogWrapper		gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result	(m_testCtx.getLog(), " // ERROR: ");

	gl.enableLogging(true);

	// initial values
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int ndx = 0; ndx < m_maxSampleMaskWords; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_SAMPLE_MASK_VALUE, ndx, -1, m_verifierType);
	}

	// fixed values
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "fixed", "Fixed values");

		for (int ndx = 0; ndx < m_maxSampleMaskWords; ++ndx)
		{
			gl.glSampleMaski(ndx, 0);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glSampleMaski");

			verifyStateIndexedInteger(result, gl, GL_SAMPLE_MASK_VALUE, ndx, 0, m_verifierType);
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

			gl.glSampleMaski(maskIndex, mask);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glSampleMaski");

			verifyStateIndexedInteger(result, gl, GL_SAMPLE_MASK_VALUE, maskIndex, mask, m_verifierType);
		}
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class MinValueIndexed3Case : public TestCase
{
public:
						MinValueIndexed3Case	(Context& context, const char* name, const char* desc, glw::GLenum target, const tcu::IVec3& ref, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const glw::GLenum	m_target;
	const tcu::IVec3	m_ref;
	const QueryType		m_verifierType;
};

MinValueIndexed3Case::MinValueIndexed3Case (Context& context, const char* name, const char* desc, glw::GLenum target, const tcu::IVec3& ref, QueryType verifierType)
	: TestCase				(context, name, desc)
	, m_target				(target)
	, m_ref					(ref)
	, m_verifierType		(verifierType)
{
}

MinValueIndexed3Case::IterateResult MinValueIndexed3Case::iterate (void)
{
	glu::CallLogWrapper		gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result	(m_testCtx.getLog(), " // ERROR: ");

	gl.enableLogging(true);

	for (int ndx = 0; ndx < 3; ++ndx)
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Element", "Element " + de::toString(ndx));

		verifyStateIndexedIntegerMin(result, gl, m_target, ndx, m_ref[ndx], m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class BufferBindingCase : public TestCase
{
public:
						BufferBindingCase	(Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType);

private:
	IterateResult		iterate				(void);

	const glw::GLenum	m_queryTarget;
	const glw::GLenum	m_bufferTarget;
	const glw::GLenum	m_numBindingsTarget;
	const QueryType		m_verifierType;
};

BufferBindingCase::BufferBindingCase (Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType)
	: TestCase				(context, name, desc)
	, m_queryTarget			(queryTarget)
	, m_bufferTarget		(bufferTarget)
	, m_numBindingsTarget	(numBindingsTarget)
	, m_verifierType		(verifierType)
{
}

BufferBindingCase::IterateResult BufferBindingCase::iterate (void)
{
	glu::CallLogWrapper 	gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxBindings	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(m_numBindingsTarget, &maxBindings);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxBindings; ++ndx)
			verifyStateIndexedInteger(result, gl, m_queryTarget, ndx, 0, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Buffer					bufferA			(m_context.getRenderContext());
		glu::Buffer					bufferB			(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxBindings / 2;

		{
			const tcu::ScopedLogSection section(m_testCtx.getLog(), "Generic", "After setting generic binding point");

			gl.glBindBuffer(m_bufferTarget, *bufferA);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glBindBuffer");

			verifyStateIndexedInteger(result, gl, m_queryTarget, 0, 0, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferBase");

			gl.glBindBufferBase(m_bufferTarget, ndxA, *bufferA);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glBindBufferBase");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, *bufferA, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferRange");

			gl.glBindBufferRange(m_bufferTarget, ndxB, *bufferB, 0, 8);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "glBindBufferRange");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxB, *bufferB, m_verifierType);
		}
		if (ndxA != ndxB)
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "DifferentStates", "Original state did not change");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, *bufferA, m_verifierType);
		}
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class BufferStartCase : public TestCase
{
public:
						BufferStartCase		(Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType);

private:
	IterateResult		iterate				(void);

	const glw::GLenum	m_queryTarget;
	const glw::GLenum	m_bufferTarget;
	const glw::GLenum	m_numBindingsTarget;
	const QueryType		m_verifierType;
};

BufferStartCase::BufferStartCase (Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType)
	: TestCase				(context, name, desc)
	, m_queryTarget			(queryTarget)
	, m_bufferTarget		(bufferTarget)
	, m_numBindingsTarget	(numBindingsTarget)
	, m_verifierType		(verifierType)
{
}

BufferStartCase::IterateResult BufferStartCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxBindings	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(m_numBindingsTarget, &maxBindings);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxBindings; ++ndx)
			verifyStateIndexedInteger(result, gl, m_queryTarget, ndx, 0, m_verifierType);
	}


	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Buffer					bufferA			(m_context.getRenderContext());
		glu::Buffer					bufferB			(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxBindings / 2;
		int							offset			= -1;

		if (m_bufferTarget == GL_ATOMIC_COUNTER_BUFFER)
			offset = 4;
		else if (m_bufferTarget == GL_SHADER_STORAGE_BUFFER)
		{
			gl.glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &offset);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "get align");
		}

		DE_ASSERT(offset != -1);

		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Generic", "After setting generic binding point");

			gl.glBindBuffer(m_bufferTarget, *bufferA);
			gl.glBufferData(m_bufferTarget, 16, DE_NULL, GL_DYNAMIC_READ);
			gl.glBindBuffer(m_bufferTarget, *bufferB);
			gl.glBufferData(m_bufferTarget, 32, DE_NULL, GL_DYNAMIC_READ);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen bufs");

			verifyStateIndexedInteger(result, gl, m_queryTarget, 0, 0, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferBase");

			gl.glBindBufferBase(m_bufferTarget, ndxA, *bufferA);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind buf");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, 0, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferRange");

			gl.glBindBufferRange(m_bufferTarget, ndxB, *bufferB, offset, 8);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind buf");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxB, offset, m_verifierType);
		}
		if (ndxA != ndxB)
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "DifferentStates", "Original state did not change");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, 0, m_verifierType);
		}
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class BufferSizeCase : public TestCase
{
public:
						BufferSizeCase	(Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType);

private:
	IterateResult		iterate			(void);

	const glw::GLenum	m_queryTarget;
	const glw::GLenum	m_bufferTarget;
	const glw::GLenum	m_numBindingsTarget;
	const QueryType		m_verifierType;
};

BufferSizeCase::BufferSizeCase (Context& context, const char* name, const char* desc, glw::GLenum queryTarget, glw::GLenum bufferTarget, glw::GLenum numBindingsTarget, QueryType verifierType)
	: TestCase				(context, name, desc)
	, m_queryTarget			(queryTarget)
	, m_bufferTarget		(bufferTarget)
	, m_numBindingsTarget	(numBindingsTarget)
	, m_verifierType		(verifierType)
{
}

BufferSizeCase::IterateResult BufferSizeCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxBindings	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(m_numBindingsTarget, &maxBindings);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxBindings; ++ndx)
			verifyStateIndexedInteger(result, gl, m_queryTarget, ndx, 0, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Buffer					bufferA			(m_context.getRenderContext());
		glu::Buffer					bufferB			(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxBindings / 2;

		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Generic", "After setting generic binding point");

			gl.glBindBuffer(m_bufferTarget, *bufferA);
			gl.glBufferData(m_bufferTarget, 16, DE_NULL, GL_DYNAMIC_READ);
			gl.glBindBuffer(m_bufferTarget, *bufferB);
			gl.glBufferData(m_bufferTarget, 32, DE_NULL, GL_DYNAMIC_READ);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen bufs");

			verifyStateIndexedInteger(result, gl, m_queryTarget, 0, 0, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferBase");

			gl.glBindBufferBase(m_bufferTarget, ndxA, *bufferA);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind buf");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, 0, m_verifierType);
		}
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "Indexed", "After setting with glBindBufferRange");

			gl.glBindBufferRange(m_bufferTarget, ndxB, *bufferB, 0, 8);
			GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind buf");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxB, 8, m_verifierType);
		}
		if (ndxA != ndxB)
		{
			const tcu::ScopedLogSection	section(m_testCtx.getLog(), "DifferentStates", "Original state did not change");

			verifyStateIndexedInteger(result, gl, m_queryTarget, ndxA, 0, m_verifierType);
		}
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingNameCase : public TestCase
{
public:
						ImageBindingNameCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingNameCase::ImageBindingNameCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingNameCase::IterateResult ImageBindingNameCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_NAME, ndx, 0, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D_ARRAY, *textureB);
		gl.glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 32, 32, 4);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 0, GL_FALSE, 2, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_NAME, ndxA, *textureA, m_verifierType);
		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_NAME, ndxB, *textureB, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingLevelCase : public TestCase
{
public:
						ImageBindingLevelCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingLevelCase::ImageBindingLevelCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingLevelCase::IterateResult ImageBindingLevelCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LEVEL, ndx, 0, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D, *textureB);
		gl.glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 2, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LEVEL, ndxA, 0, m_verifierType);
		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LEVEL, ndxB, 2, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingLayeredCase : public TestCase
{
public:
						ImageBindingLayeredCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingLayeredCase::ImageBindingLayeredCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingLayeredCase::IterateResult ImageBindingLayeredCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedBoolean(result, gl, GL_IMAGE_BINDING_LAYERED, ndx, false, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D_ARRAY, *textureB);
		gl.glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 32, 32, 4);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 0, GL_TRUE, 2, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedBoolean(result, gl, GL_IMAGE_BINDING_LAYERED, ndxA, false, m_verifierType);
		verifyStateIndexedBoolean(result, gl, GL_IMAGE_BINDING_LAYERED, ndxB, true, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingLayerCase : public TestCase
{
public:
						ImageBindingLayerCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingLayerCase::ImageBindingLayerCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingLayerCase::IterateResult ImageBindingLayerCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LAYER, ndx, 0, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D_ARRAY, *textureB);
		gl.glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 32, 32, 4);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 0, GL_TRUE, 2, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LAYER, ndxA, 0, m_verifierType);
		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_LAYER, ndxB, 2, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingAccessCase : public TestCase
{
public:
						ImageBindingAccessCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingAccessCase::ImageBindingAccessCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingAccessCase::IterateResult ImageBindingAccessCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_ACCESS, ndx, GL_READ_ONLY, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D_ARRAY, *textureB);
		gl.glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 32, 32, 4);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 0, GL_TRUE, 2, GL_READ_WRITE, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_ACCESS, ndxA, GL_READ_ONLY, m_verifierType);
		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_ACCESS, ndxB, GL_READ_WRITE, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ImageBindingFormatCase : public TestCase
{
public:
						ImageBindingFormatCase	(Context& context, const char* name, const char* desc, QueryType verifierType);

private:
	IterateResult		iterate					(void);

	const QueryType		m_verifierType;
};

ImageBindingFormatCase::ImageBindingFormatCase (Context& context, const char* name, const char* desc, QueryType verifierType)
	: TestCase			(context, name, desc)
	, m_verifierType	(verifierType)
{
}

ImageBindingFormatCase::IterateResult ImageBindingFormatCase::iterate (void)
{
	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	int						maxImages	= -1;

	gl.enableLogging(true);

	gl.glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImages);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

		for (int ndx = 0; ndx < maxImages; ++ndx)
			verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_FORMAT, ndx, GL_R32UI, m_verifierType);
	}

	{
		const tcu::ScopedLogSection	superSection	(m_testCtx.getLog(), "AfterSetting", "After setting");
		glu::Texture				textureA		(m_context.getRenderContext());
		glu::Texture				textureB		(m_context.getRenderContext());
		const int					ndxA			= 0;
		const int					ndxB			= maxImages / 2;

		gl.glBindTexture(GL_TEXTURE_2D, *textureA);
		gl.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxA, *textureA, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		gl.glBindTexture(GL_TEXTURE_2D_ARRAY, *textureB);
		gl.glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32F, 32, 32, 4);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

		gl.glBindImageTexture(ndxB, *textureB, 0, GL_TRUE, 2, GL_READ_WRITE, GL_R32F);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind unit");

		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_FORMAT, ndxA, GL_RGBA8UI, m_verifierType);
		verifyStateIndexedInteger(result, gl, GL_IMAGE_BINDING_FORMAT, ndxB, GL_R32F, m_verifierType);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

} // anonymous

IndexedStateQueryTests::IndexedStateQueryTests (Context& context)
	: TestCaseGroup(context, "indexed", "Indexed state queries")
{
}

IndexedStateQueryTests::~IndexedStateQueryTests (void)
{
}

void IndexedStateQueryTests::init (void)
{
	static const QueryType verifiers[] = { QUERY_INDEXED_BOOLEAN, QUERY_INDEXED_INTEGER, QUERY_INDEXED_INTEGER64 };

#define FOR_EACH_VERIFIER(X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)	\
	{																						\
		const char* verifierSuffix = getVerifierSuffix(verifiers[verifierNdx]);				\
		const QueryType verifier = verifiers[verifierNdx];									\
		this->addChild(X);																	\
	}

	FOR_EACH_VERIFIER(new SampleMaskCase			(m_context, (std::string() + "sample_mask_value_" + verifierSuffix).c_str(), 				"Test SAMPLE_MASK_VALUE", verifier))

	FOR_EACH_VERIFIER(new MinValueIndexed3Case		(m_context, (std::string() + "max_compute_work_group_count_" + verifierSuffix).c_str(),		"Test MAX_COMPUTE_WORK_GROUP_COUNT",	GL_MAX_COMPUTE_WORK_GROUP_COUNT,	tcu::IVec3(65535,65535,65535),	verifier))
	FOR_EACH_VERIFIER(new MinValueIndexed3Case		(m_context, (std::string() + "max_compute_work_group_size_" + verifierSuffix).c_str(),		"Test MAX_COMPUTE_WORK_GROUP_SIZE",		GL_MAX_COMPUTE_WORK_GROUP_SIZE,		tcu::IVec3(128, 128, 64),		verifier))

	FOR_EACH_VERIFIER(new BufferBindingCase			(m_context, (std::string() + "atomic_counter_buffer_binding_" + verifierSuffix).c_str(),	"Test ATOMIC_COUNTER_BUFFER_BINDING",	GL_ATOMIC_COUNTER_BUFFER_BINDING,	GL_ATOMIC_COUNTER_BUFFER,	GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, verifier))
	FOR_EACH_VERIFIER(new BufferStartCase			(m_context, (std::string() + "atomic_counter_buffer_start_" + verifierSuffix).c_str(),		"Test ATOMIC_COUNTER_BUFFER_START",		GL_ATOMIC_COUNTER_BUFFER_START,		GL_ATOMIC_COUNTER_BUFFER,	GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,	verifier))
	FOR_EACH_VERIFIER(new BufferSizeCase			(m_context, (std::string() + "atomic_counter_buffer_size_" + verifierSuffix).c_str(),		"Test ATOMIC_COUNTER_BUFFER_SIZE",		GL_ATOMIC_COUNTER_BUFFER_SIZE,		GL_ATOMIC_COUNTER_BUFFER,	GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,	verifier))

	FOR_EACH_VERIFIER(new BufferBindingCase			(m_context, (std::string() + "shader_storager_buffer_binding_" + verifierSuffix).c_str(),	"Test SHADER_STORAGE_BUFFER_BINDING",	GL_SHADER_STORAGE_BUFFER_BINDING,	GL_SHADER_STORAGE_BUFFER,	GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,	verifier))
	FOR_EACH_VERIFIER(new BufferStartCase			(m_context, (std::string() + "shader_storager_buffer_start_" + verifierSuffix).c_str(),		"Test SHADER_STORAGE_BUFFER_START",		GL_SHADER_STORAGE_BUFFER_START,		GL_SHADER_STORAGE_BUFFER,	GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,	verifier))
	FOR_EACH_VERIFIER(new BufferSizeCase			(m_context, (std::string() + "shader_storager_buffer_size_" + verifierSuffix).c_str(),		"Test SHADER_STORAGE_BUFFER_SIZE",		GL_SHADER_STORAGE_BUFFER_SIZE,		GL_SHADER_STORAGE_BUFFER,	GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,	verifier))

	FOR_EACH_VERIFIER(new ImageBindingNameCase		(m_context, (std::string() + "image_binding_name_" + verifierSuffix).c_str(),				"Test IMAGE_BINDING_NAME",				verifier))
	FOR_EACH_VERIFIER(new ImageBindingLevelCase		(m_context, (std::string() + "image_binding_level_" + verifierSuffix).c_str(),				"Test IMAGE_BINDING_LEVEL",				verifier))
	FOR_EACH_VERIFIER(new ImageBindingLayeredCase	(m_context, (std::string() + "image_binding_layered_" + verifierSuffix).c_str(),			"Test IMAGE_BINDING_LAYERED",			verifier))
	FOR_EACH_VERIFIER(new ImageBindingLayerCase		(m_context, (std::string() + "image_binding_layer_" + verifierSuffix).c_str(),				"Test IMAGE_BINDING_LAYER",				verifier))
	FOR_EACH_VERIFIER(new ImageBindingAccessCase	(m_context, (std::string() + "image_binding_access_" + verifierSuffix).c_str(),				"Test IMAGE_BINDING_ACCESS",			verifier))
	FOR_EACH_VERIFIER(new ImageBindingFormatCase	(m_context, (std::string() + "image_binding_format_" + verifierSuffix).c_str(),				"Test IMAGE_BINDING_FORMAT",			verifier))

#undef FOR_EACH_VERIFIER
}

} // Functional
} // gles31
} // deqp
