/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2018 The Khronos Group Inc.
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
 **/ /*!
 * \file glcLayoutLocationTests.cpp
 * \brief
 */ /*-------------------------------------------------------------------*/
#include "glcLayoutLocationTests.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"

#include "deStringUtil.hpp"

#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "gluDrawUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"

using namespace glw;

namespace glcts
{

static const GLuint WIDTH  = 2;
static const GLuint HEIGHT = 2;

// Helper function used to set texture parameters
void setTexParameters(const Functions& gl, GLenum target, bool depthTexture)
{
	gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if (depthTexture)
	{
		gl.texParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		gl.texParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	}
}

// Helper function used to create texture data
template <typename DATA_TYPE>
std::vector<DATA_TYPE> generateData(std::size_t width, std::size_t height, std::size_t components)
{
	DE_ASSERT((components == 1) || (components == 4));
	std::size_t			   size = width * height * components;
	std::vector<DATA_TYPE> data(size, 0);
	for (std::size_t i = 0; i < size; i += components)
		data[i]		   = static_cast<DATA_TYPE>(255);
	return data;
}

// Structure used to return id of created object it had to be defined to support
// GL_TEXTURE_BUFFER cases which require creation of both texture and buffer
struct ResultData
{
	deUint32 textureId; // id of created texture
	deUint32 bufferId;  // used only by GL_TEXTURE_BUFFER

	ResultData(deUint32 tId) : textureId(tId), bufferId(0)
	{
	}

	ResultData(deUint32 tId, deUint32 bId) : textureId(tId), bufferId(bId)
	{
	}
};

template <typename DATA_TYPE>
ResultData createTexture1D(const Functions& gl, std::size_t components, GLenum internalFormat, GLenum format,
						   GLenum type)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, 1, components);

	deUint32 id;
	gl.genTextures(1, &id);
	gl.bindTexture(GL_TEXTURE_1D, id);
	gl.texImage1D(GL_TEXTURE_1D, 0, internalFormat, WIDTH, 0, format, type, &data[0]);
	setTexParameters(gl, GL_TEXTURE_1D, components == 1);
	return id;
}

template <typename DATA_TYPE>
ResultData createTexture2D(const Functions& gl, std::size_t components, GLenum target, GLenum internalFormat,
						   GLenum format, GLenum type)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, HEIGHT, components);

	deUint32 id;
	gl.genTextures(1, &id);
	gl.bindTexture(target, id);
	gl.texStorage2D(target, 1, internalFormat, WIDTH, HEIGHT);
	gl.texSubImage2D(target, 0, 0, 0, WIDTH, HEIGHT, format, type, &data[0]);
	setTexParameters(gl, target, components == 1);
	return id;
}

template <typename DATA_TYPE>
ResultData createTexture3D(const Functions& gl, std::size_t components, GLenum internalFormat, GLenum format,
						   GLenum type)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, HEIGHT, components);

	deUint32 id;
	gl.genTextures(1, &id);
	gl.bindTexture(GL_TEXTURE_3D, id);
	gl.texStorage3D(GL_TEXTURE_3D, 1, internalFormat, WIDTH, HEIGHT, 1);
	gl.texSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, WIDTH, HEIGHT, 1, format, type, &data[0]);
	setTexParameters(gl, GL_TEXTURE_3D, components == 1);
	return id;
}

template <typename DATA_TYPE>
ResultData createCubeMap(const Functions& gl, std::size_t components, GLenum internalFormat, GLenum format, GLenum type)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, HEIGHT, components);

	deUint32 id;
	gl.genTextures(1, &id);
	gl.bindTexture(GL_TEXTURE_CUBE_MAP, id);
	GLenum faces[] = { GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
					   GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };
	gl.texStorage2D(GL_TEXTURE_CUBE_MAP, 1, internalFormat, WIDTH, HEIGHT);
	for (int i = 0; i < 6; ++i)
		gl.texSubImage2D(faces[i], 0, 0, 0, WIDTH, HEIGHT, format, type, &data[0]);
	setTexParameters(gl, GL_TEXTURE_CUBE_MAP, components == 1);
	return id;
}

template <typename DATA_TYPE>
ResultData createTexture2DArray(const Functions& gl, std::size_t components, GLenum internalFormat, GLenum format,
								GLenum type)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, HEIGHT, components);

	deUint32 id;
	gl.genTextures(1, &id);
	gl.bindTexture(GL_TEXTURE_2D_ARRAY, id);
	gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1, internalFormat, WIDTH, HEIGHT, 1);
	gl.texSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, WIDTH, HEIGHT, 1, format, type, &data[0]);
	setTexParameters(gl, GL_TEXTURE_2D_ARRAY, components == 1);
	return id;
}

template <typename DATA_TYPE>
ResultData createTextureBuffer(const Functions& gl, GLenum internalFormat)
{
	std::vector<DATA_TYPE> data = generateData<DATA_TYPE>(WIDTH, HEIGHT, 4);

	deUint32 bufferId;
	gl.genBuffers(1, &bufferId);
	gl.bindBuffer(GL_TEXTURE_BUFFER, bufferId);
	gl.bufferData(GL_TEXTURE_BUFFER, WIDTH * HEIGHT * 4 * sizeof(DATA_TYPE), &data[0], GL_STATIC_DRAW);

	deUint32 textureId;
	gl.genTextures(1, &textureId);
	gl.bindTexture(GL_TEXTURE_BUFFER, textureId);
	gl.texBuffer(GL_TEXTURE_BUFFER, internalFormat, bufferId);
	return ResultData(textureId, bufferId);
}

// create function was implemented for convinience. Specializations of this
// template simplify definition of test data by reducting the number of
// attributes which were moved to create fn implementation. This aproach
// also simplyfies texture creation in the test as create takes just a single
// parameter for all test cases.
template <GLenum, GLenum>
ResultData create(const Functions& gl)
{
	(void)gl;
	TCU_FAIL("Missing specialization implementation.");
}

template <>
ResultData create<GL_TEXTURE_2D, GL_RGBA8>(const Functions& gl)
{
	return createTexture2D<unsigned char>(gl, 4, GL_TEXTURE_2D, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_3D, GL_RGBA8>(const Functions& gl)
{
	return createTexture3D<unsigned char>(gl, 4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_CUBE_MAP, GL_RGBA8>(const Functions& gl)
{
	return createCubeMap<unsigned char>(gl, 4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_CUBE_MAP, GL_DEPTH_COMPONENT16>(const Functions& gl)
{
	return createCubeMap<short>(gl, 1, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
}

template <>
ResultData create<GL_TEXTURE_2D, GL_DEPTH_COMPONENT16>(const Functions& gl)
{
	return createTexture2D<short>(gl, 1, GL_TEXTURE_2D, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
}

template <>
ResultData create<GL_TEXTURE_2D_ARRAY, GL_RGBA8>(const Functions& gl)
{
	return createTexture2DArray<unsigned char>(gl, 4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_2D_ARRAY, GL_DEPTH_COMPONENT16>(const Functions& gl)
{
	return createTexture2DArray<short>(gl, 1, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
}

template <>
ResultData create<GL_TEXTURE_2D, GL_RGBA32I>(const Functions& gl)
{
	return createTexture2D<int>(gl, 4, GL_TEXTURE_2D, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
}

template <>
ResultData create<GL_TEXTURE_3D, GL_RGBA32I>(const Functions& gl)
{
	return createTexture3D<int>(gl, 4, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
}

template <>
ResultData create<GL_TEXTURE_CUBE_MAP, GL_RGBA32I>(const Functions& gl)
{
	return createCubeMap<int>(gl, 4, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
}

template <>
ResultData create<GL_TEXTURE_2D_ARRAY, GL_RGBA32I>(const Functions& gl)
{
	return createTexture2DArray<int>(gl, 4, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT);
}

template <>
ResultData create<GL_TEXTURE_2D, GL_RGBA32UI>(const Functions& gl)
{
	return createTexture2D<unsigned int>(gl, 4, GL_TEXTURE_2D, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
}

template <>
ResultData create<GL_TEXTURE_3D, GL_RGBA32UI>(const Functions& gl)
{
	return createTexture3D<unsigned int>(gl, 4, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
}

template <>
ResultData create<GL_TEXTURE_CUBE_MAP, GL_RGBA32UI>(const Functions& gl)
{
	return createCubeMap<unsigned int>(gl, 4, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
}

template <>
ResultData create<GL_TEXTURE_2D_ARRAY, GL_RGBA32UI>(const Functions& gl)
{
	return createTexture2DArray<unsigned int>(gl, 4, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
}

template <>
ResultData create<GL_TEXTURE_1D, GL_RGBA8>(const Functions& gl)
{
	return createTexture1D<unsigned char>(gl, 4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_1D, GL_DEPTH_COMPONENT16>(const Functions& gl)
{
	return createTexture1D<unsigned short>(gl, 1, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
}

template <>
ResultData create<GL_TEXTURE_1D_ARRAY, GL_RGBA8>(const Functions& gl)
{
	return createTexture2D<unsigned char>(gl, 4, GL_TEXTURE_1D_ARRAY, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
}

template <>
ResultData create<GL_TEXTURE_1D_ARRAY, GL_DEPTH_COMPONENT16>(const Functions& gl)
{
	return createTexture2D<short>(gl, 1, GL_TEXTURE_1D_ARRAY, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
								  GL_UNSIGNED_SHORT);
}

template <>
ResultData create<GL_TEXTURE_BUFFER, GL_RGBA32F>(const Functions& gl)
{
	return createTextureBuffer<float>(gl, GL_RGBA32F);
}

template <>
ResultData create<GL_TEXTURE_BUFFER, GL_RGBA32I>(const Functions& gl)
{
	return createTextureBuffer<int>(gl, GL_RGBA32I);
}

template <>
ResultData create<GL_TEXTURE_BUFFER, GL_RGBA32UI>(const Functions& gl)
{
	return createTextureBuffer<unsigned int>(gl, GL_RGBA32UI);
}

// Structure used to define all test case data
struct SamplerCaseData
{
	typedef ResultData (*CreateFnPtr)(const Functions& gl);

	CreateFnPtr	create;						// pointer to function that will create texture
	const char*	name;						// test case name
	const char*	opaqueType;					// sampler or image
	const char*	outAssignment;				// operation that determines fragment color
	const int	num_frag_image_uniforms;	// the number of required fragment image uniform
};

class SpecifiedLocationCase : public deqp::TestCase
{
public:
	SpecifiedLocationCase(deqp::Context& context, const SamplerCaseData& data);
	virtual ~SpecifiedLocationCase();

	tcu::TestNode::IterateResult iterate();

private:
	ResultData (*m_createFn)(const Functions& gl);
	std::map<std::string, std::string> m_specializationMap;

	bool		m_isImageCase;
	GLenum		m_imageFormat;
	std::string	m_imageFormatQualifier;
	int			m_num_frag_image_uniform;
};

SpecifiedLocationCase::SpecifiedLocationCase(deqp::Context& context, const SamplerCaseData& data)
	: deqp::TestCase(context, data.name, ""), m_createFn(data.create)
{
	std::string type(data.opaqueType);
	m_specializationMap["OPAQUE_TYPE"]	= type;
	m_specializationMap["OUT_ASSIGNMENT"] = data.outAssignment;

	m_isImageCase = (type.find("sampler") == std::string::npos);
	if (m_isImageCase)
	{
		m_specializationMap["OPAQUE_TYPE_NAME"] = "image";
		m_specializationMap["ACCESS"]			= "readonly";

		if (type.find("iimage") != std::string::npos)
		{
			m_imageFormatQualifier = "rgba32i";
			m_imageFormat		   = GL_RGBA32I;
		}
		else if (type.find("uimage") != std::string::npos)
		{
			m_imageFormatQualifier = "rgba32ui";
			m_imageFormat		   = GL_RGBA32UI;
		}
		else
		{
			m_imageFormatQualifier = "rgba8";
			m_imageFormat		   = GL_RGBA8;
		}
	}
	else
	{
		m_specializationMap["OPAQUE_TYPE_NAME"] = "sampler";
		m_specializationMap["ACCESS"]			= "";
	}
	m_num_frag_image_uniform = data.num_frag_image_uniforms;
}

SpecifiedLocationCase::~SpecifiedLocationCase()
{
}

tcu::TestNode::IterateResult SpecifiedLocationCase::iterate(void)
{
	static const deUint16 quadIndices[] = { 0, 1, 2, 2, 1, 3 };
	static const float	positions[]   = { -1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, -1.0 };

	static const char* vsTemplate = "${VERSION}\n"
									"precision highp float;\n"
									"layout(location=0) in highp vec2 inPosition;\n"
									"layout(location=0) out highp vec2 coords;\n"
									"void main(void)\n"
									"{\n"
									"  coords = vec2(max(0.0, inPosition.x), max(0.0, inPosition.y));\n"
									"  gl_Position = vec4(inPosition, 0.0, 1.0);\n"
									"}\n";

	static const char* fsTemplate =
		"${VERSION}\n"
		"precision highp float;\n"
		"layout(location=0) in vec2 coords;\n"
		"layout(location=0) out vec4 fragColor;\n"
		"layout(${OPAQUE_TYPE_QUALIFIERS}) ${ACCESS} uniform highp ${OPAQUE_TYPE} ${OPAQUE_TYPE_NAME};\n"
		"void main(void)\n"
		"{\n"
		"  fragColor = ${OUT_ASSIGNMENT};\n"
		"}\n";

	glu::RenderContext& renderContext = m_context.getRenderContext();
	glu::ContextType	contextType   = renderContext.getType();
	glu::GLSLVersion	glslVersion   = glu::getContextTypeGLSLVersion(contextType);
	const Functions&	gl			  = renderContext.getFunctions();
	bool				contextTypeES = glu::isContextTypeES(contextType);
	bool				contextES32	  = glu::contextSupports(contextType, glu::ApiType::es(3, 2));
	if (contextTypeES && !contextES32 && !m_context.getContextInfo().isExtensionSupported("GL_ANDROID_extension_pack_es31a"))
		if (m_context.getContextInfo().getInt(GL_MAX_FRAGMENT_IMAGE_UNIFORMS) < m_num_frag_image_uniform)
			throw tcu::NotSupportedError("The number of required fragment image uniform is larger than GL_MAX_FRAGMENT_IMAGE_UNIFORMS");

	const int expectedLocation = 2;
	const int definedBinding   = 1;

	std::ostringstream layoutSpecification;
	layoutSpecification << "location=" << expectedLocation;
	if (m_isImageCase)
	{
		if (contextTypeES)
			layoutSpecification << ", binding=" << definedBinding;
		layoutSpecification << ", " << m_imageFormatQualifier;
	}

	m_specializationMap["VERSION"]				  = glu::getGLSLVersionDeclaration(glslVersion);
	m_specializationMap["OPAQUE_TYPE_QUALIFIERS"] = layoutSpecification.str();

	std::string		   vs = tcu::StringTemplate(vsTemplate).specialize(m_specializationMap);
	std::string		   fs = tcu::StringTemplate(fsTemplate).specialize(m_specializationMap);
	glu::ShaderProgram program(gl, glu::makeVtxFragSources(vs.c_str(), fs.c_str()));

	m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	if (!program.isOk())
	{
		m_testCtx.getLog() << program << tcu::TestLog::Message << "Creation of program failed."
						   << tcu::TestLog::EndMessage;
		return STOP;
	}

	deUint32 programId = program.getProgram();
	int		 location  = gl.getUniformLocation(programId, m_specializationMap["OPAQUE_TYPE_NAME"].c_str());
	if (location != expectedLocation)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Expected uniform to be at location " << expectedLocation
						   << ", not at " << location << "." << tcu::TestLog::EndMessage;
		return STOP;
	}

	gl.useProgram(programId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

	// Prepare texture/buffer
	gl.activeTexture(GL_TEXTURE1);
	ResultData resultData = (*m_createFn)(gl);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GL object creation failed.");

	if (m_isImageCase)
	{
		gl.bindImageTexture(definedBinding, resultData.textureId, 0, GL_TRUE, 0, GL_READ_ONLY, m_imageFormat);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture");
	}

	// in ES image uniforms cannot be updated
	// through any of the glUniform* commands
	if (!(contextTypeES && m_isImageCase))
	{
		gl.uniform1i(expectedLocation, definedBinding);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i");
	}

	// Create FBO with RBO
	deUint32 rboId;
	deUint32 fboId;
	gl.genRenderbuffers(1, &rboId);
	gl.bindRenderbuffer(GL_RENDERBUFFER, rboId);
	gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, WIDTH, HEIGHT);
	gl.genFramebuffers(1, &fboId);
	gl.bindFramebuffer(GL_FRAMEBUFFER, fboId);
	gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboId);

	// Render
	gl.viewport(0, 0, WIDTH, HEIGHT);
	const glu::VertexArrayBinding vertexArrays[] = { glu::va::Float("inPosition", 2, 4, 0, positions) };
	glu::draw(renderContext, programId, DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
			  glu::pr::TriangleStrip(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices));

	// Grab surface
	tcu::Surface resultFrame(WIDTH, HEIGHT);
	glu::readPixels(renderContext, 0, 0, resultFrame.getAccess());

	// Verify color of just first pixel
	const tcu::RGBA expectedColor(255, 0, 0, 0);
	tcu::RGBA		pixel = resultFrame.getPixel(0, 0);
	if (pixel != expectedColor)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Incorrect color was generated, expected: ["
						   << expectedColor.getRed() << ", " << expectedColor.getGreen() << ", "
						   << expectedColor.getBlue() << ", " << expectedColor.getAlpha() << "], got ["
						   << pixel.getRed() << ", " << pixel.getGreen() << ", " << pixel.getBlue() << ", "
						   << pixel.getAlpha() << "]" << tcu::TestLog::EndMessage;
	}
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	// Cleanup
	if (resultData.bufferId)
		gl.deleteBuffers(1, &resultData.bufferId);
	gl.deleteFramebuffers(1, &fboId);
	gl.deleteRenderbuffers(1, &rboId);
	gl.deleteTextures(1, &resultData.textureId);

	return STOP;
}

class NegativeLocationCase : public deqp::TestCase
{
public:
	NegativeLocationCase(deqp::Context& context);
	virtual ~NegativeLocationCase();

	tcu::TestNode::IterateResult iterate();
};

NegativeLocationCase::NegativeLocationCase(deqp::Context& context) : deqp::TestCase(context, "invalid_cases", "")
{
}

NegativeLocationCase::~NegativeLocationCase()
{
}

tcu::TestNode::IterateResult NegativeLocationCase::iterate()
{
	glu::RenderContext& renderContext = m_context.getRenderContext();
	glu::ContextType	contextType   = renderContext.getType();
	glu::GLSLVersion	glslVersion   = glu::getContextTypeGLSLVersion(contextType);
	const Functions&	gl			  = renderContext.getFunctions();

	m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	static const char* csTemplate = "${VERSION}\n"
									"layout(location=2, binding=0) uniform atomic_uint u_atomic;\n"
									"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
									"layout(binding=0) buffer Output {\n  uint value;\n} sb_out;\n\n"
									"void main (void) {\n"
									"  sb_out.value = atomicCounterIncrement(u_atomic);\n"
									"}";

	std::map<std::string, std::string> specializationMap;
	specializationMap["VERSION"] = glu::getGLSLVersionDeclaration(glslVersion);
	std::string cs				 = tcu::StringTemplate(csTemplate).specialize(specializationMap);

	m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	glu::ProgramSources sourcesCompute;
	sourcesCompute.sources[glu::SHADERTYPE_COMPUTE].push_back(cs);
	glu::ShaderProgram program(gl, sourcesCompute);
	if (program.isOk())
	{
		m_testCtx.getLog() << program << tcu::TestLog::Message
						   << "layout(location = N) is not allowed for atomic counters" << tcu::TestLog::EndMessage;
		return STOP;
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

LayoutLocationTests::LayoutLocationTests(Context& context) : TestCaseGroup(context, "layout_location", "")
{
}

LayoutLocationTests::~LayoutLocationTests(void)
{
}

void LayoutLocationTests::init(void)
{
	const SamplerCaseData commonArguments[] =
	{
		{ &create<GL_TEXTURE_2D, GL_RGBA8>,						"sampler_2d",				"sampler2D",			"texture(sampler, coords)",											0 },
		{ &create<GL_TEXTURE_3D, GL_RGBA8>,						"sampler_3d",				"sampler3D",			"texture(sampler, vec3(coords, 0.0))",								0 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA8>,				"sampler_cube",				"samplerCube",			"texture(sampler, vec3(coords, 0.0))",								0 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_DEPTH_COMPONENT16>,	"sampler_cube_shadow",		"samplerCubeShadow",	"vec4(texture(sampler, vec4(coords, 0.0, 0.0)), 0.0, 0.0, 0.0)",	0 },
		{ &create<GL_TEXTURE_2D, GL_DEPTH_COMPONENT16>,			"sampler_2d_shadow",		"sampler2DShadow",		"vec4(texture(sampler, vec3(coords, 0.0)), 0.0, 0.0, 0.0)",			0 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA8>,				"sampler_2d_array",			"sampler2DArray",		"texture(sampler, vec3(coords, 0.0))",								0 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_DEPTH_COMPONENT16>,	"sampler_2d_array_shadow",	"sampler2DArrayShadow",	"vec4(texture(sampler, vec4(coords, 0.0, 0.0)), 0.0, 0.0, 0.0)",	0 },
		{ &create<GL_TEXTURE_2D, GL_RGBA32I>,					"isampler_2d",				"isampler2D",			"vec4(texture(sampler, coords))/255.0",								0 },
		{ &create<GL_TEXTURE_3D, GL_RGBA32I>,					"isampler_3d",				"isampler3D",			"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA32I>,				"isampler_cube",			"isamplerCube",			"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA32I>,				"isampler_2d_array",		"isampler2DArray",		"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },
		{ &create<GL_TEXTURE_2D, GL_RGBA32UI>,					"usampler_2d",				"usampler2D",			"vec4(texture(sampler, coords))/255.0",								0 },
		{ &create<GL_TEXTURE_3D, GL_RGBA32UI>,					"usampler_3d",				"usampler3D",			"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA32UI>,			"usampler_cube",			"usamplerCube",			"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA32UI>,			"usampler_2d_array",		"usampler2DArray",		"vec4(texture(sampler, vec3(coords, 0.0)))/255.0",					0 },

		{ &create<GL_TEXTURE_2D, GL_RGBA8>,						"image_2d",					"image2D",				"imageLoad(image, ivec2(0, 0))",									1 },
		{ &create<GL_TEXTURE_2D, GL_RGBA32I>,					"iimage_2d",				"iimage2D",				"vec4(imageLoad(image, ivec2(0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_2D, GL_RGBA32UI>,					"uimage_2d",				"uimage2D",				"vec4(imageLoad(image, ivec2(0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_3D, GL_RGBA8>,						"image_3d",					"image3D",				"imageLoad(image, ivec3(0, 0, 0))",									1 },
		{ &create<GL_TEXTURE_3D, GL_RGBA32I>,					"iimage_3d",				"iimage3D",				"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_3D, GL_RGBA32UI>,					"uimage_3d",				"uimage3D",				"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA8>,				"image_cube",				"imageCube",			"imageLoad(image, ivec3(0, 0, 0))",									1 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA32I>,				"iimage_cube",				"iimageCube",			"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_CUBE_MAP, GL_RGBA32UI>,			"uimage_cube",				"uimageCube",			"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA8>,				"image_2d_array",			"image2DArray",			"imageLoad(image, ivec3(0, 0, 0))",									1 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA32I>,				"iimage_2d_array",			"iimage2DArray",		"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
		{ &create<GL_TEXTURE_2D_ARRAY, GL_RGBA32UI>,			"uimage_2d_array",			"uimage2DArray",		"vec4(imageLoad(image, ivec3(0, 0, 0)))/255.0",						1 },
	};

	// Additional array containing entries for core gl
	const SamplerCaseData coreArguments[] =
	{
		{ &create<GL_TEXTURE_BUFFER, GL_RGBA32F>,				"sampler_buffer",			"samplerBuffer",		"texelFetch(sampler, 1)",											0 },
		{ &create<GL_TEXTURE_BUFFER, GL_RGBA32I>,				"isampler_buffer",			"isamplerBuffer",		"vec4(texelFetch(sampler, 1))/255.0",								0 },
		{ &create<GL_TEXTURE_BUFFER, GL_RGBA32UI>,				"usampler_buffer",			"usamplerBuffer",		"vec4(texelFetch(sampler, 1))/255.0",								0 },
		{ &create<GL_TEXTURE_1D, GL_RGBA8>,						"sampler_1d",				"sampler1D",			"texture(sampler, coords.x)",										0 },
		{ &create<GL_TEXTURE_1D, GL_DEPTH_COMPONENT16>,			"sampler_1d_shadow",		"sampler1DShadow",		"vec4(texture(sampler, vec3(coords, 0.0)), 0.0, 0.0, 0.0)",			0 },
		{ &create<GL_TEXTURE_1D_ARRAY, GL_RGBA8>,				"sampler_1d_array",			"sampler1DArray",		"texture(sampler, coords, 0.0)",									0 },
		{ &create<GL_TEXTURE_1D_ARRAY, GL_DEPTH_COMPONENT16>,	"sampler_1d_array_shadow",	"sampler1DArrayShadow",	"vec4(texture(sampler, vec3(coords, 0.0)), 0.0, 0.0, 0.0)",			0 },
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(commonArguments); ++i)
		addChild(new SpecifiedLocationCase(m_context, commonArguments[i]));

	glu::RenderContext& renderContext = m_context.getRenderContext();
	glu::ContextType	contextType   = renderContext.getType();
	if (!glu::isContextTypeES(contextType))
	{
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(coreArguments); ++i)
			addChild(new SpecifiedLocationCase(m_context, coreArguments[i]));
	}

	addChild(new NegativeLocationCase(m_context));
}

} // glcts
