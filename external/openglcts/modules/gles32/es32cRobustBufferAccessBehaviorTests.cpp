/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 */ /*!
 * \file  es32cRobustBufferAccessBehaviorTests.cpp
 * \brief Implements conformance tests for "Robust Buffer Access Behavior" functionality.
 */ /*-------------------------------------------------------------------*/

#include "es32cRobustBufferAccessBehaviorTests.hpp"

#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#include <cstring>
#include <string>

using namespace glw;
using namespace deqp::RobustBufferAccessBehavior;

namespace es32cts
{
namespace RobustBufferAccessBehavior
{
/** Constructor
 *
 * @param context Test context
 **/
VertexBufferObjectsTest::VertexBufferObjectsTest(deqp::Context& context)
	: deqp::RobustBufferAccessBehavior::VertexBufferObjectsTest(
		  context, "vertex_buffer_objects", "Verifies that out-of-bound reads from VB result in zero")
{
	/* Nothing to be done */
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string VertexBufferObjectsTest::getFragmentShader()
{
	return std::string("#version 320 es\n"
					   "\n"
					   "layout (location = 0) out lowp uvec4 out_fs_color;\n"
					   "\n"
					   "void main()\n"
					   "{\n"
					   "    out_fs_color = uvec4(1, 255, 255, 255);\n"
					   "}\n"
					   "\n");
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string VertexBufferObjectsTest::getVertexShader()
{
	return std::string("#version 320 es\n"
					   "\n"
					   "layout (location = 0) in vec4 in_vs_position;\n"
					   "\n"
					   "void main()\n"
					   "{\n"
					   "    gl_Position = in_vs_position;\n"
					   "}\n"
					   "\n");
}

/** No verification because of undefined out-of-bound behavior in OpenGL ES
 *
 * @param texture_id Id of texture
 *
 * @return true
 **/
bool VertexBufferObjectsTest::verifyInvalidResults(glw::GLuint texture_id)
{
	(void)texture_id;
	return true;
}

/** Verifies that texutre is filled with 1
 *
 * @param texture_id Id of texture
 *
 * @return true when image is filled with 1, false otherwise
 **/
bool VertexBufferObjectsTest::verifyResults(glw::GLuint texture_id)
{
	static const GLuint height	 = 8;
	static const GLuint width	  = 8;
	static const GLuint pixel_size = 4 * sizeof(GLuint);

	const Functions& gl = m_context.getRenderContext().getFunctions();

	const GLint buf_size = width * height * pixel_size;
	GLubyte		pixels[buf_size];
	deMemset(pixels, 0, buf_size);

	Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

	Texture::GetData(gl, texture_id, 0 /* level */, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixels);

	/* Unbind */
	Texture::Bind(gl, 0, GL_TEXTURE_2D);

	/* Verify */
	for (GLuint i = 0; i < buf_size; i += pixel_size)
	{
		if (1 != pixels[i])
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid value: " << (GLuint)pixels[i]
												<< " at offset: " << i << tcu::TestLog::EndMessage;

			return false;
		}
	}

	return true;
}

/** Constructor
 *
 * @param context Test context
 **/
TexelFetchTest::TexelFetchTest(deqp::Context& context)
	: deqp::RobustBufferAccessBehavior::TexelFetchTest(context, "texel_fetch",
													   "Verifies that out-of-bound fetches from texture result in zero")
{
	/* Nothing to be done */
}

TexelFetchTest::TexelFetchTest(deqp::Context& context, const glw::GLchar* name, const glw::GLchar* description)
	: deqp::RobustBufferAccessBehavior::TexelFetchTest(context, name, description)
{
	/* Nothing to be done */
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string TexelFetchTest::getGeometryShader()
{
	return std::string("#version 320 es\n"
					   "\n"
					   "layout(points)                           in;\n"
					   "layout(triangle_strip, max_vertices = 4) out;\n"
					   "\n"
					   "out vec2 gs_fs_tex_coord;\n"
					   "\n"
					   "void main()\n"
					   "{\n"
					   "    gs_fs_tex_coord = vec2(0, 0);\n"
					   "    gl_Position     = vec4(-1, -1, 0, 1);\n"
					   "    EmitVertex();\n"
					   "\n"
					   "    gs_fs_tex_coord = vec2(0, 1);\n"
					   "    gl_Position     = vec4(-1, 1, 0, 1);\n"
					   "    EmitVertex();\n"
					   "\n"
					   "    gs_fs_tex_coord = vec2(1, 0);\n"
					   "    gl_Position     = vec4(1, -1, 0, 1);\n"
					   "    EmitVertex();\n"
					   "\n"
					   "    gs_fs_tex_coord = vec2(1, 1);\n"
					   "    gl_Position     = vec4(1, 1, 0, 1);\n"
					   "    EmitVertex();\n"
					   "}\n"
					   "\n");
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string TexelFetchTest::getVertexShader()
{
	return std::string("#version 320 es\n"
					   "\n"
					   "void main()\n"
					   "{\n"
					   "    gl_Position = vec4(0, 0, 0, 1);\n"
					   "}\n"
					   "\n");
}

/** Prepare a texture
 *
 * @param is_source  Selects if texutre will be used as source or destination
 * @param texture_id Id of texutre
 **/
void TexelFetchTest::prepareTexture(bool is_source, glw::GLuint texture_id)
{
	/* Image size */
	static const GLuint image_height = 16;
	static const GLuint image_width  = 16;

	/* GL entry points */
	const Functions& gl = m_context.getRenderContext().getFunctions();

	/* Texture storage parameters */
	GLuint  height			= image_height;
	GLenum  internal_format = 0;
	GLsizei n_levels		= 1;
	GLenum  target			= GL_TEXTURE_2D;
	GLuint  width			= image_width;

	/* Prepare texture storage parameters */
	switch (m_test_case)
	{
	case R8:
		internal_format = GL_R8;
		break;
	case RG8_SNORM:
		internal_format = GL_RG8_SNORM;
		break;
	case RGBA32F:
		internal_format = GL_RGBA32F;
		break;
	case R32UI_MIPMAP:
		height			= 2 * image_height;
		internal_format = GL_R32UI;
		n_levels		= 2;
		width			= 2 * image_width;
		break;
	case R32UI_MULTISAMPLE:
		internal_format = GL_R32UI;
		n_levels		= 4;
		target			= GL_TEXTURE_2D_MULTISAMPLE;
		break;
	default:
		TCU_FAIL("Invalid enum");
	}

	/* Prepare storage */
	Texture::Bind(gl, texture_id, target);
	Texture::Storage(gl, target, n_levels, internal_format, width, height, 0);

	/* Set samplers to NEAREST/NEAREST if required */
	if (R32UI_MULTISAMPLE != m_test_case)
	{
		gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	/* Destination image can be left empty */
	if (false == is_source)
	{
		Texture::Bind(gl, 0, target);
		return;
	}

	/* Prepare texture */
	if (R8 == m_test_case)
	{
		GLubyte source_pixels[image_width * image_height];
		for (GLuint i = 0; i < image_width * image_height; ++i)
		{
			source_pixels[i] = (GLubyte)i;
		}

		Texture::SubImage(gl, GL_TEXTURE_2D, 0 /* level */, 0 /* x */, 0 /* y */, 0 /* z */, width, height,
						  0 /* depth */, GL_RED, GL_UNSIGNED_BYTE, source_pixels);
	}
	else if (RG8_SNORM == m_test_case)
	{
		static const GLuint n_components = 2;

		GLbyte source_pixels[image_width * image_height * n_components];
		for (GLuint i = 0; i < image_width * image_height; ++i)
		{
			source_pixels[i * n_components + 0] = (GLubyte)((i % 16));
			source_pixels[i * n_components + 1] = (GLubyte)((i / 16));
		}

		Texture::SubImage(gl, GL_TEXTURE_2D, 0 /* level */, 0 /* x */, 0 /* y */, 0 /* z */, width, height,
						  0 /* depth */, GL_RG, GL_BYTE, source_pixels);
	}
	else if (RGBA32F == m_test_case)
	{
		static const GLuint n_components = 4;

		GLfloat source_pixels[image_width * image_height * n_components];
		for (GLuint i = 0; i < image_width * image_height; ++i)
		{
			source_pixels[i * n_components + 0] = (GLfloat)(i % 16) / 16.0f;
			source_pixels[i * n_components + 1] = (GLfloat)(i / 16) / 16.0f;
			source_pixels[i * n_components + 2] = (GLfloat)i / 256.0f;
			source_pixels[i * n_components + 3] = 1.0f;
		}

		Texture::SubImage(gl, GL_TEXTURE_2D, 0 /* level */, 0 /* x */, 0 /* y */, 0 /* z */, width, height,
						  0 /* depth */, GL_RGBA, GL_FLOAT, source_pixels);
	}
	else if (R32UI_MIPMAP == m_test_case)
	{
		GLuint source_pixels[image_width * image_height];
		for (GLuint i = 0; i < image_width * image_height; ++i)
		{
			source_pixels[i] = i;
		}

		Texture::SubImage(gl, GL_TEXTURE_2D, 1 /* level */, 0 /* x */, 0 /* y */, 0 /* z */, image_width, image_height,
						  0 /* depth */, GL_RED_INTEGER, GL_UNSIGNED_INT, source_pixels);

		/* texelFetch() undefined if the computed level of detail is not the texture’s base level and the texture’s
			minification filter is NEAREST or LINEAR */
		gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	}
	else if (R32UI_MULTISAMPLE == m_test_case)
	{
		/* Compute Shader */
		static const GLchar* cs = "#version 320 es\n"
								  "\n"
								  "layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
								  "\n"
								  "layout (binding = 0, r32ui) writeonly uniform highp uimage2DMS uni_image;\n"
								  "\n"
								  "void main()\n"
								  "{\n"
								  "    ivec2 point = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y);\n"
								  "    uint  index = gl_WorkGroupID.y * 16U + gl_WorkGroupID.x;\n"
								  "\n"
								  "    imageStore(uni_image, point, 0, uvec4(index + 0U, 0, 0, 0));\n"
								  "    imageStore(uni_image, point, 1, uvec4(index + 1U, 0, 0, 0));\n"
								  "    imageStore(uni_image, point, 2, uvec4(index + 2U, 0, 0, 0));\n"
								  "    imageStore(uni_image, point, 3, uvec4(index + 3U, 0, 0, 0));\n"
								  "}\n"
								  "\n";

		Program program(m_context);
		program.Init(cs, "", "", "", "", "");
		program.Use();

		gl.bindImageTexture(0 /* unit */, texture_id, 0 /* level */, GL_FALSE /* layered */, 0 /* layer */,
							GL_WRITE_ONLY, GL_R32UI);
		GLU_EXPECT_NO_ERROR(gl.getError(), "BindImageTexture");

		gl.dispatchCompute(16, 16, 1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");
	}

	Texture::Bind(gl, 0, target);
}

/** No verification because of undefined out-of-bound behavior in OpenGL ES
 *
 * @param texture_id Id of texture
 *
 * @return true
 **/
bool TexelFetchTest::verifyInvalidResults(glw::GLuint texture_id)
{
	(void)texture_id;
	return true;
}

/** Verifies that texutre is filled with increasing values
 *
 * @param texture_id Id of texture
 *
 * @return true when image is filled with increasing values, false otherwise
 **/
bool TexelFetchTest::verifyValidResults(glw::GLuint texture_id)
{
	static const GLuint height   = 16;
	static const GLuint width	= 16;
	static const GLuint n_pixels = height * width;

	const Functions& gl = m_context.getRenderContext().getFunctions();

	bool result = true;

	if (R8 == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLubyte> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			pixels[i] = (GLubyte)i;
		}

		Texture::GetData(gl, texture_id, 0 /* level */, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLubyte expected_red = (GLubyte)i;
			const GLubyte drawn_red	= pixels[i * n_channels];

			if (expected_red != drawn_red)
			{
				m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid value: " << (GLuint)drawn_red
													<< ". Expected value: " << (GLuint)expected_red
													<< " at offset: " << i << tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}
	else if (RG8_SNORM == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLubyte> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			pixels[i * n_channels + 0] = (GLubyte)i;
			pixels[i * n_channels + 1] = (GLubyte)i;
		}

		Texture::GetData(gl, texture_id, 0 /* level */, width, height, GL_RGBA, GL_BYTE, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLbyte expected_red   = (GLubyte)(i % 16);
			const GLbyte expected_green = (GLubyte)(i / 16);
			const GLbyte drawn_red		= pixels[i * n_channels + 0];
			const GLbyte drawn_green	= pixels[i * n_channels + 1];

			if ((expected_red != drawn_red) || (expected_green != drawn_green))
			{
				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << "Invalid value: " << (GLint)drawn_red << ", " << (GLint)drawn_green
					<< ". Expected value: " << (GLint)expected_red << ", " << (GLint)expected_green
					<< ". At offset: " << i << tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}
	else if (RGBA32F == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLfloat> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			pixels[i * n_channels + 0] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 1] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 2] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 3] = (GLfloat)i / (GLfloat)n_pixels;
		}

		Texture::GetData(gl, texture_id, 0 /* level */, width, height, GL_RGBA, GL_FLOAT, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLfloat expected_red   = (GLfloat)(i % 16) / 16.0f;
			const GLfloat expected_green = (GLfloat)(i / 16) / 16.0f;
			const GLfloat expected_blue  = (GLfloat)i / 256.0f;
			const GLfloat expected_alpha = 1.0f;
			const GLfloat drawn_red		 = pixels[i * n_channels + 0];
			const GLfloat drawn_green	= pixels[i * n_channels + 1];
			const GLfloat drawn_blue	 = pixels[i * n_channels + 2];
			const GLfloat drawn_alpha	= pixels[i * n_channels + 3];

			if ((expected_red != drawn_red) || (expected_green != drawn_green) || (expected_blue != drawn_blue) ||
				(expected_alpha != drawn_alpha))
			{
				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << "Invalid value: " << drawn_red << ", " << drawn_green << ", "
					<< drawn_blue << ", " << drawn_alpha << ". Expected value: " << expected_red << ", "
					<< expected_green << ", " << expected_blue << ", " << expected_alpha << ". At offset: " << i
					<< tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}
	else if (R32UI_MIPMAP == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLuint> pixels;
		pixels.resize(n_pixels * n_channels);
		deMemset(&pixels[0], 0, n_pixels * n_channels * sizeof(GLuint));

		Texture::GetData(gl, texture_id, 1 /* level */, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLuint expected_red = i;
			const GLuint drawn_red	= pixels[i * n_channels];

			if (expected_red != drawn_red)
			{
				m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid value: " << drawn_red
													<< ". Expected value: " << expected_red << " at offset: " << i
													<< tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}
	else if (R32UI_MULTISAMPLE == m_test_case)
	{
		static const GLuint n_channels = 4;

		/* Compute Shader */
		static const GLchar* cs =
			"#version 320 es\n"
			"\n"
			"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"\n"
			"layout (binding = 1, r32ui) writeonly uniform lowp uimage2D   uni_destination_image;\n"
			"layout (binding = 0, r32ui) readonly  uniform lowp uimage2DMS uni_source_image;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    ivec2 point = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y);\n"
			"    uint  index = gl_WorkGroupID.y * 16U + gl_WorkGroupID.x;\n"
			"\n"
			"    uvec4 color_0 = imageLoad(uni_source_image, point, 0);\n"
			"    uvec4 color_1 = imageLoad(uni_source_image, point, 1);\n"
			"    uvec4 color_2 = imageLoad(uni_source_image, point, 2);\n"
			"    uvec4 color_3 = imageLoad(uni_source_image, point, 3);\n"
			"\n"
			"    if (any(equal(uvec4(color_0.r, color_1.r, color_2.r, color_3.r), uvec4(index + 3U))))\n"
			"    {\n"
			"        imageStore(uni_destination_image, point, uvec4(1U));\n"
			"    }\n"
			"    else\n"
			"    {\n"
			"        imageStore(uni_destination_image, point, uvec4(0U));\n"
			"    }\n"
			"}\n"
			"\n";

		Program program(m_context);
		Texture destination_texture(m_context);

		Texture::Generate(gl, destination_texture.m_id);
		Texture::Bind(gl, destination_texture.m_id, GL_TEXTURE_2D);
		Texture::Storage(gl, GL_TEXTURE_2D, 1, GL_R32UI, width, height, 0 /* depth */);

		program.Init(cs, "", "", "", "", "");
		program.Use();
		gl.bindImageTexture(0 /* unit */, texture_id, 0 /* level */, GL_FALSE /* layered */, 0 /* layer */,
							GL_READ_ONLY, GL_R32UI);
		GLU_EXPECT_NO_ERROR(gl.getError(), "BindImageTexture");
		gl.bindImageTexture(1 /* unit */, destination_texture.m_id, 0 /* level */, GL_FALSE /* layered */,
							0 /* layer */, GL_WRITE_ONLY, GL_R32UI);
		GLU_EXPECT_NO_ERROR(gl.getError(), "BindImageTexture");

		gl.dispatchCompute(16, 16, 1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");

		/* Pixels buffer initialization */
		std::vector<GLuint> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels * n_channels; ++i)
		{
			pixels[i] = i;
		}

		Texture::GetData(gl, destination_texture.m_id, 0 /* level */, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
						 &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLuint expected_red = 1;
			const GLuint drawn_red	= pixels[i * n_channels];

			if (expected_red != drawn_red)
			{
				m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid value: " << drawn_red
													<< ". Expected value: " << expected_red << " at offset: " << i
													<< tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}

	return result;
}

/** Constructor
 *
 * @param context Test context
 **/
ImageLoadStoreTest::ImageLoadStoreTest(deqp::Context& context)
	: TexelFetchTest(context, "image_load_store", "Verifies that out-of-bound to image result in zero or is discarded")
{
	/* start from RGBA32F as R8, R32UI_MULTISAMPLE and R8_SNORM are not supported under GLES */
	m_test_case = RGBA32F;
}

/** Execute test
 *
 * @return tcu::TestNode::STOP
 **/
tcu::TestNode::IterateResult ImageLoadStoreTest::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_KHR_robust_buffer_access_behavior"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
		return STOP;
	}

	/* Constants */
	static const GLuint height = 16;
	static const GLuint width  = 16;

	/* GL entry points */
	const Functions& gl = m_context.getRenderContext().getFunctions();

	const unsigned int coord_offsets[] = {
		16, 512, 1024, 2048,
	};

	/* Test result indicator */
	bool test_result = true;

	/* Iterate over all cases */
	while (LAST != m_test_case)
	{
		/* Test case result indicator */
		bool case_result = true;

		/* Test case objects */
		Texture destination_texture(m_context);
		Texture source_texture(m_context);
		Program program(m_context);

		/* Prepare textures */
		Texture::Generate(gl, destination_texture.m_id);
		Texture::Generate(gl, source_texture.m_id);

		prepareTexture(false, destination_texture.m_id);
		prepareTexture(true, source_texture.m_id);

		/* Test invalid source cases */
		for (GLuint i = 0; i < DE_LENGTH_OF_ARRAY(coord_offsets); ++i)
		{
			const std::string& cs = getComputeShader(SOURCE_INVALID, coord_offsets[i]);
			program.Init(cs, "", "", "", "", "");
			program.Use();

			/* Set texture */
			setTextures(destination_texture.m_id, source_texture.m_id);

			/* Dispatch */
			gl.dispatchCompute(width, height, 1 /* depth */);
			GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");

			/* Verification */
			if (false == verifyInvalidResults(destination_texture.m_id))
			{
				case_result = false;
			}
		}

		/* Test valid case */
		program.Init(getComputeShader(VALID), "", "", "", "", "");
		program.Use();

		/* Set texture */
		setTextures(destination_texture.m_id, source_texture.m_id);

		/* Dispatch */
		gl.dispatchCompute(width, height, 1 /* depth */);
		GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");

		/* Verification */
		if (false == verifyValidResults(destination_texture.m_id))
		{
			case_result = false;
		}

		/* Test invalid destination cases */
		for (GLuint i = 0; i < DE_LENGTH_OF_ARRAY(coord_offsets); ++i)
		{
			const std::string& cs = getComputeShader(DESTINATION_INVALID, coord_offsets[i]);
			program.Init(cs, "", "", "", "", "");
			program.Use();

			/* Set texture */
			setTextures(destination_texture.m_id, source_texture.m_id);

			/* Dispatch */
			gl.dispatchCompute(width, height, 1 /* depth */);
			GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");

			/* Verification */
			if (false == verifyValidResults(destination_texture.m_id))
			{
				case_result = false;
			}
		}

		/* Set test result */
		if (false == case_result)
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Test case: " << getTestCaseName()
												<< " failed" << tcu::TestLog::EndMessage;

			test_result = false;
		}

		/* Increment */
		m_test_case = (TEST_CASES)((GLuint)m_test_case + 1);
	}

	/* Set result */
	if (true == test_result)
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}
	else
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}

	/* Done */
	return tcu::TestNode::STOP;
}

/** Prepare shader for current test case
 *
 * @param version Specify which version should be prepared
 *
 * @return Source
 **/
std::string ImageLoadStoreTest::getComputeShader(VERSION version, GLuint coord_offset)
{
	static const GLchar* template_code =
		"#version 320 es\n"
		"\n"
		"layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"\n"
		"layout (binding = 1, FORMAT) writeonly uniform highp IMAGE uni_destination_image;\n"
		"layout (binding = 0, FORMAT) readonly  uniform highp IMAGE uni_source_image;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    ivec2 point_destination = ivec2(gl_WorkGroupID.xy) + ivec2(COORD_OFFSETU);\n"
		"    ivec2 point_source      = ivec2(gl_WorkGroupID.xy) + ivec2(COORD_OFFSETU);\n"
		"\n"
		"COPY"
		"}\n"
		"\n";

	static const GLchar* copy_regular = "    TYPE color = imageLoad(uni_source_image, point_source);\n"
										"    imageStore(uni_destination_image, point_destination, color);\n";

	static const GLchar* format_rgba32f = "rgba32f";
	static const GLchar* format_r32ui   = "r32ui";

	static const GLchar* image_vec4 = "image2D";

	static const GLchar* image_uvec4 = "uimage2D";

	static const GLchar* type_vec4  = "vec4";
	static const GLchar* type_uvec4 = "uvec4";

	const GLchar* copy				 = copy_regular;
	const GLchar* format			 = format_rgba32f;
	const GLchar* image				 = image_vec4;
	const GLchar* type				 = type_vec4;
	const GLchar* src_coord_offset   = "0";
	const GLchar* dst_coord_offset   = "0";

	std::stringstream coord_offset_stream;
	coord_offset_stream << coord_offset;
	std::string coord_offset_str = coord_offset_stream.str();

	if (version == SOURCE_INVALID)
		src_coord_offset = coord_offset_str.c_str();
	else if (version == DESTINATION_INVALID)
		dst_coord_offset = coord_offset_str.c_str();

	switch (m_test_case)
	{
	case RGBA32F:
		format = format_rgba32f;
		break;
	case R32UI_MIPMAP:
		format = format_r32ui;
		image  = image_uvec4;
		type   = type_uvec4;
		break;
	default:
		TCU_FAIL("Invalid enum");
	};

	size_t		position = 0;
	std::string source   = template_code;

	replaceToken("FORMAT", position, format, source);
	replaceToken("IMAGE", position, image, source);
	replaceToken("FORMAT", position, format, source);
	replaceToken("IMAGE", position, image, source);
	replaceToken("COORD_OFFSET", position, dst_coord_offset, source);
	replaceToken("COORD_OFFSET", position, src_coord_offset, source);

	size_t temp_position = position;
	replaceToken("COPY", position, copy, source);
	position = temp_position;

	switch (m_test_case)
	{
	case RGBA32F:
	case R32UI_MIPMAP:
		replaceToken("TYPE", position, type, source);
		break;
	default:
		TCU_FAIL("Invalid enum");
	}

	return source;
}

/** Set textures as images
 *
 * @param id_destination Id of texture used as destination
 * @param id_source      Id of texture used as source
 **/
void ImageLoadStoreTest::setTextures(glw::GLuint id_destination, glw::GLuint id_source)
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	GLenum format = 0;
	GLint  level  = 0;

	switch (m_test_case)
	{
	case RGBA32F:
		format = GL_RGBA32F;
		break;
	case R32UI_MIPMAP:
		format = GL_R32UI;
		level  = 1;
		break;
	default:
		TCU_FAIL("Invalid enum");
	}

	gl.bindImageTexture(0 /* unit */, id_source, level, GL_FALSE /* layered */, 0 /* layer */, GL_READ_ONLY, format);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindImageTexture");

	gl.bindImageTexture(1 /* unit */, id_destination, level, GL_FALSE /* layered */, 0 /* layer */, GL_WRITE_ONLY,
						format);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindImageTexture");
}

/** No verification because of undefined out-of-bound behavior in OpenGL ES
 *
 * @param texture_id Id of texture
 *
 * @return true
 **/
bool ImageLoadStoreTest::verifyInvalidResults(glw::GLuint texture_id)
{
	(void)texture_id;
	return true;
}

/** Verifies that texutre is filled with increasing values
 *
 * @param texture_id Id of texture
 *
 * @return true when image is filled with increasing values, false otherwise
 **/
bool ImageLoadStoreTest::verifyValidResults(glw::GLuint texture_id)
{
	static const GLuint height   = 16;
	static const GLuint width	= 16;
	static const GLuint n_pixels = height * width;

	const Functions& gl = m_context.getRenderContext().getFunctions();
	gl.memoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "MemoryBarrier");

	bool result = true;

	if (RGBA32F == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLfloat> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			pixels[i * n_channels + 0] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 1] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 2] = (GLfloat)i / (GLfloat)n_pixels;
			pixels[i * n_channels + 3] = (GLfloat)i / (GLfloat)n_pixels;
		}

		Texture::GetData(gl, texture_id, 0 /* level */, width, height, GL_RGBA, GL_FLOAT, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLfloat expected_red   = (GLfloat)(i % 16) / 16.0f;
			const GLfloat expected_green = (GLfloat)(i / 16) / 16.0f;
			const GLfloat expected_blue  = (GLfloat)i / 256.0f;
			const GLfloat expected_alpha = 1.0f;
			const GLfloat drawn_red		 = pixels[i * n_channels + 0];
			const GLfloat drawn_green	= pixels[i * n_channels + 1];
			const GLfloat drawn_blue	 = pixels[i * n_channels + 2];
			const GLfloat drawn_alpha	= pixels[i * n_channels + 3];

			if ((expected_red != drawn_red) || (expected_green != drawn_green) || (expected_blue != drawn_blue) ||
				(expected_alpha != drawn_alpha))
			{
				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << "Invalid value: " << drawn_red << ", " << drawn_green << ", "
					<< drawn_blue << ", " << drawn_alpha << ". Expected value: " << expected_red << ", "
					<< expected_green << ", " << expected_blue << ", " << expected_alpha << ". At offset: " << i
					<< tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}
	else if (R32UI_MIPMAP == m_test_case)
	{
		static const GLuint n_channels = 4;

		Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

		std::vector<GLuint> pixels;
		pixels.resize(n_pixels * n_channels);
		for (GLuint i = 0; i < n_pixels * n_channels; ++i)
		{
			pixels[i] = 0;
		}

		Texture::GetData(gl, texture_id, 1 /* level */, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, &pixels[0]);

		/* Unbind */
		Texture::Bind(gl, 0, GL_TEXTURE_2D);

		/* Verify */
		for (GLuint i = 0; i < n_pixels; ++i)
		{
			const GLuint expected_red = i;
			const GLuint drawn_red	= pixels[i * n_channels];

			if (expected_red != drawn_red)
			{
				m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid value: " << drawn_red
													<< ". Expected value: " << expected_red << " at offset: " << i
													<< tcu::TestLog::EndMessage;

				result = false;
				break;
			}
		}
	}

	return result;
}

/** Constructor
 *
 * @param context Test context
 **/
StorageBufferTest::StorageBufferTest(deqp::Context& context)
	: deqp::RobustBufferAccessBehavior::StorageBufferTest(
		  context, "storage_buffer", "Verifies that out-of-bound access to SSBO results with no error")
{
	/* Nothing to be done here */
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string StorageBufferTest::getComputeShader(glw::GLuint offset)
{
	static const GLchar* cs = "#version 320 es\n"
							  "\n"
							  "layout (local_size_x = 4, local_size_y = 1, local_size_z = 1) in;\n"
							  "\n"
							  "layout (binding = 1) buffer Source {\n"
							  "    float data[];\n"
							  "} source;\n"
							  "\n"
							  "layout (binding = 0) buffer Destination {\n"
							  "    float data[];\n"
							  "} destination;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "    uint index_destination = gl_LocalInvocationID.x + OFFSETU;\n"
							  "    uint index_source      = gl_LocalInvocationID.x + OFFSETU;\n"
							  "\n"
							  "    destination.data[index_destination] = source.data[index_source];\n"
							  "}\n"
							  "\n";

	std::string   destination_offset("0");
	std::string   source_offset("0");
	size_t		  position = 0;
	std::string   source   = cs;

	std::stringstream offset_stream;
	offset_stream << offset;

	if (m_test_case == SOURCE_INVALID)
		source_offset = offset_stream.str();
	else if (m_test_case == DESTINATION_INVALID)
		destination_offset = offset_stream.str();

	replaceToken("OFFSET", position, destination_offset.c_str(), source);
	replaceToken("OFFSET", position, source_offset.c_str(), source);

	return source;
}

/** Verify test case results
 *
 * @param buffer_data Buffer data to verify
 *
 * @return true if buffer_data is as expected, false othrewise
 **/
bool StorageBufferTest::verifyResults(GLfloat* buffer_data)
{
	static const GLfloat expected_data_valid[4]				  = { 2.0f, 3.0f, 4.0f, 5.0f };
	static const GLfloat expected_data_invalid_destination[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const GLfloat expected_data_invalid_source[4]	  = { 0.0f, 0.0f, 0.0f, 0.0f };

	int size = sizeof(GLfloat) * 4;

	/* Prepare expected data const for proper case*/
	const GLfloat* expected_data = 0;
	const GLchar*  name			 = 0;
	switch (m_test_case)
	{
	case VALID:
		expected_data = expected_data_valid;
		name		  = "valid indices";
		break;
	case SOURCE_INVALID:
		expected_data = expected_data_invalid_source;
		name		  = "invalid source indices";
		break;
	case DESTINATION_INVALID:
		expected_data = expected_data_invalid_destination;
		name		  = "invalid destination indices";
		break;
	default:
		TCU_FAIL("Invalid enum");
	}

	/* Verify buffer data */
	if (m_test_case == VALID && memcmp(expected_data, buffer_data, size) != 0)
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Test case: " << name << " failed"
											<< tcu::TestLog::EndMessage;
		return false;
	}

	return true;
}

/** Constructor
 *
 * @param context Test context
 **/
UniformBufferTest::UniformBufferTest(deqp::Context& context)
	: deqp::RobustBufferAccessBehavior::UniformBufferTest(
		  context, "uniform_buffer", "Verifies that out-of-bound access to UBO resutls with no error")
{
	/* Nothing to be done here */
}

/** Prepare shader for current test case
 *
 * @return Source
 **/
std::string UniformBufferTest::getComputeShader(GLuint offset)
{
	static const GLchar* cs = "#version 320 es\n"
							  "\n"
							  "layout (local_size_x = 4, local_size_y = 1, local_size_z = 1) in;\n"
							  "\n"
							  "layout (binding = 0, std140) uniform Source {\n"
							  "    float data[16];\n"
							  "} source;\n"
							  "\n"
							  "layout (binding = 0, std430) buffer Destination {\n"
							  "    float data[];\n"
							  "} destination;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "    uint index_destination = gl_LocalInvocationID.x + OFFSETU;\n"
							  "    uint index_source      = gl_LocalInvocationID.x + OFFSETU;\n"
							  "\n"
							  "    destination.data[index_destination] = source.data[index_source];\n"
							  "}\n"
							  "\n";

	const GLchar* destination_offset = "0";
	std::string   source_offset("0");
	size_t		  position = 0;
	std::string   source   = cs;

	std::stringstream offset_stream;
	offset_stream << offset;

	if (m_test_case == SOURCE_INVALID)
		source_offset = offset_stream.str();

	replaceToken("OFFSET", position, destination_offset, source);
	replaceToken("OFFSET", position, source_offset.c_str(), source);

	return source;
}

} /* RobustBufferAccessBehavior */

/** Constructor.
 *
 *  @param context Rendering context.
 **/
RobustBufferAccessBehaviorTests::RobustBufferAccessBehaviorTests(deqp::Context& context)
	: deqp::RobustBufferAccessBehaviorTests(context)
{
	/* Left blank on purpose */
}

/** Initializes a multi_bind test group.
 *
 **/
void RobustBufferAccessBehaviorTests::init(void)
{
	addChild(new RobustBufferAccessBehavior::VertexBufferObjectsTest(m_context));
	addChild(new RobustBufferAccessBehavior::TexelFetchTest(m_context));
	addChild(new RobustBufferAccessBehavior::ImageLoadStoreTest(m_context));
	addChild(new RobustBufferAccessBehavior::StorageBufferTest(m_context));
	addChild(new RobustBufferAccessBehavior::UniformBufferTest(m_context));
}

} /* es32cts namespace */
