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
 * \brief Shader state query tests
 *//*--------------------------------------------------------------------*/

#include "es31fShaderStateQueryTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
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

class SamplerTypeCase : public TestCase
{
public:
					SamplerTypeCase	(Context& ctx, const char* name, const char* desc);

private:
	IterateResult	iterate			(void);
};

SamplerTypeCase::SamplerTypeCase (Context& ctx, const char* name, const char* desc)
	: TestCase(ctx, name, desc)
{
}

SamplerTypeCase::IterateResult SamplerTypeCase::iterate (void)
{
	static const struct SamplerType
	{
		glw::GLenum	glType;
		const char*	typeStr;
		const char*	samplePosStr;
		bool		isArray;
	} samplerTypes[] =
	{
		{ GL_SAMPLER_2D_MULTISAMPLE,						"sampler2DMS",			"ivec2(gl_FragCoord.xy)",	false	},
		{ GL_INT_SAMPLER_2D_MULTISAMPLE,					"isampler2DMS",			"ivec2(gl_FragCoord.xy)",	false	},
		{ GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE,			"usampler2DMS",			"ivec2(gl_FragCoord.xy)",	false	},
		{ GL_SAMPLER_2D_MULTISAMPLE_ARRAY,					"sampler2DMSArray",		"ivec3(gl_FragCoord.xyz)",	true	},
		{ GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,				"isampler2DMSArray",	"ivec3(gl_FragCoord.xyz)",	true	},
		{ GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,		"usampler2DMSArray",	"ivec3(gl_FragCoord.xyz)",	true	},
	};

	static const char* const	vertexSource			=	"#version 310 es\n"
															"in highp vec4 a_position;\n"
															"void main(void)\n"
															"{\n"
															"	gl_Position = a_position;\n"
															"}\n";
	static const char* const	fragmentSourceTemplate	=	"#version 310 es\n"
															"${EXTENSIONSTATEMENT}"
															"uniform highp ${SAMPLERTYPE} u_sampler;\n"
															"layout(location = 0) out highp vec4 dEQP_FragColor;\n"
															"void main(void)\n"
															"{\n"
															"	dEQP_FragColor = vec4(texelFetch(u_sampler, ${POSITION}, 0));\n"
															"}\n";
	const bool					textureArraySupported	=	m_context.getContextInfo().isExtensionSupported("GL_OES_texture_storage_multisample_2d_array");
	bool						error					=	false;

	for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(samplerTypes); ++typeNdx)
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), std::string(samplerTypes[typeNdx].typeStr), std::string() + "Sampler type " + samplerTypes[typeNdx].typeStr);

		if (samplerTypes[typeNdx].isArray && !textureArraySupported)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "GL_OES_texture_storage_multisample_2d_array not supported, skipping type " << samplerTypes[typeNdx].typeStr << tcu::TestLog::EndMessage;
		}
		else
		{
			std::map<std::string, std::string>	shaderArgs;
			shaderArgs["SAMPLERTYPE"]			= samplerTypes[typeNdx].typeStr;
			shaderArgs["POSITION"]				= samplerTypes[typeNdx].samplePosStr;
			shaderArgs["EXTENSIONSTATEMENT"]	= (samplerTypes[typeNdx].isArray) ? ("#extension GL_OES_texture_storage_multisample_2d_array : require\n") : ("");

			{
				const std::string			fragmentSource	= tcu::StringTemplate(fragmentSourceTemplate).specialize(shaderArgs);
				const glw::Functions&		gl				= m_context.getRenderContext().getFunctions();
				glu::ShaderProgram			program			(m_context.getRenderContext(), glu::ProgramSources() << glu::VertexSource(vertexSource) << glu::FragmentSource(fragmentSource));

				m_testCtx.getLog() << tcu::TestLog::Message << "Building program with uniform sampler of type " << samplerTypes[typeNdx].typeStr << tcu::TestLog::EndMessage;

				if (!program.isOk())
				{
					m_testCtx.getLog() << program;
					throw tcu::TestError("could not build shader");
				}

				// only one uniform -- uniform at index 0
				{
					int uniforms = 0;
					gl.getProgramiv(program.getProgram(), GL_ACTIVE_UNIFORMS, &uniforms);

					if (uniforms != 1)
						throw tcu::TestError("Unexpected GL_ACTIVE_UNIFORMS, expected 1");
				}

				m_testCtx.getLog() << tcu::TestLog::Message << "Verifying uniform type." << tcu::TestLog::EndMessage;

				// check type
				{
					const glw::GLuint	uniformIndex	= 0;
					glw::GLint			type			= 0;

					gl.getActiveUniformsiv(program.getProgram(), 1, &uniformIndex, GL_UNIFORM_TYPE, &type);

					if (type != (glw::GLint)samplerTypes[typeNdx].glType)
					{
						m_testCtx.getLog() << tcu::TestLog::Message << "Invalid type, expected " << samplerTypes[typeNdx].glType << ", got " << type << tcu::TestLog::EndMessage;
						error = true;
					}
				}

				GLU_EXPECT_NO_ERROR(gl.getError(), "");
			}
		}
	}

	if (!error)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid uniform type");

	return STOP;
}

} // anonymous

ShaderStateQueryTests::ShaderStateQueryTests (Context& context)
	: TestCaseGroup(context, "shader", "Shader state query tests")
{
}

ShaderStateQueryTests::~ShaderStateQueryTests (void)
{
}

void ShaderStateQueryTests::init (void)
{
	// sampler type query
	addChild(new SamplerTypeCase(m_context, "sampler_type", "Sampler type cases"));
}

} // Functional
} // gles31
} // deqp
