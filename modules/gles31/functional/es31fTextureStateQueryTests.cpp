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
 * \brief Texture Param State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es31fTextureStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluObjectWrapper.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

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
		case QUERY_TEXTURE_PARAM_FLOAT:		return "get_tex_parameterfv";
		case QUERY_TEXTURE_PARAM_INTEGER:	return "get_tex_parameteriv";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

class DepthStencilModeCase : public TestCase
{
public:
						DepthStencilModeCase	(Context& context, QueryType verifier, const char* name, const char* desc);
	IterateResult		iterate					(void);

private:
	const QueryType		m_verifier;
};

DepthStencilModeCase::DepthStencilModeCase (Context& context, QueryType verifier, const char* name, const char* desc)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
}

DepthStencilModeCase::IterateResult DepthStencilModeCase::iterate (void)
{
	glu::Texture			texture	(m_context.getRenderContext());
	glu::CallLogWrapper		gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result	(m_testCtx.getLog(), " // ERROR: ");

	gl.enableLogging(true);

	gl.glBindTexture(GL_TEXTURE_2D, *texture);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "bind");

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial");
		verifyStateTextureParamInteger(result, gl, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT, m_verifier);
	}

	{
		const tcu::ScopedLogSection	section				(m_testCtx.getLog(), "Toggle", "Toggle");
		const glw::GLint			depthComponentInt	= GL_DEPTH_COMPONENT;
		const glw::GLfloat			depthComponentFloat	= (glw::GLfloat)GL_DEPTH_COMPONENT;

		gl.glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "set state");
		verifyStateTextureParamInteger(result, gl, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX, m_verifier);

		gl.glTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &depthComponentInt);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "set state");
		verifyStateTextureParamInteger(result, gl, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT, m_verifier);

		gl.glTexParameterf(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "set state");
		verifyStateTextureParamInteger(result, gl, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX, m_verifier);

		gl.glTexParameterfv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &depthComponentFloat);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "set state");
		verifyStateTextureParamInteger(result, gl, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT, m_verifier);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

} // anonymous

TextureStateQueryTests::TextureStateQueryTests (Context& context)
	: TestCaseGroup(context, "texture", "Texture State Query tests")
{
}

TextureStateQueryTests::~TextureStateQueryTests (void)
{
}

void TextureStateQueryTests::init (void)
{
	static const QueryType verifiers[] =
	{
		QUERY_TEXTURE_PARAM_INTEGER,
		QUERY_TEXTURE_PARAM_FLOAT,
	};

#define FOR_EACH_VERIFIER(X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)	\
	{																						\
		const char* verifierSuffix = getVerifierSuffix(verifiers[verifierNdx]);				\
		const QueryType verifier = verifiers[verifierNdx];									\
		this->addChild(X);																	\
	}

	FOR_EACH_VERIFIER(new DepthStencilModeCase(m_context, verifier, (std::string("depth_stencil_mode_case_") + verifierSuffix).c_str(), "Test DEPTH_STENCIL_TEXTURE_MODE"));

#undef FOR_EACH_VERIFIER
}

} // Functional
} // gles31
} // deqp
