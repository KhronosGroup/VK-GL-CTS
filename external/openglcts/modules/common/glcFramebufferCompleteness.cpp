/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 Google Inc.
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \file glcFramebuffercompleteness.cpp
 * \brief Tests for OpenGL ES frame buffer completeness
 *//*--------------------------------------------------------------------*/
#include "glcFramebufferCompleteness.hpp"
#include "deInt32.h"
#include "gluDefs.hpp"
#include "gluContextInfo.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace glcts
{
namespace
{

using namespace glw;
using namespace std;

struct TestContext
{
	const glu::RenderContext&	renderContext;
	const glw::Functions&		gl;
	vector<GLuint>&				fboIds;
	vector<GLuint>&				texIds;
	vector<GLuint>&				rboIds;

	void texParameteri					(GLuint texId, GLenum target, GLenum pname, GLint parameter);
	void bindTexture					(GLenum target, GLuint texId);
	void texImage2D						(GLenum target, GLuint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border,
										 GLenum format, GLenum type, const void* data);
	void texImage2D						(GLuint texId, GLenum target, GLuint level, GLint internalFormat, GLsizei width, GLsizei height,
										 GLint border, GLenum format, GLenum type, const void* data);
	void texImage3D						(GLuint texId, GLenum target, GLuint level, GLint internalFormat, GLsizei width, GLsizei height,
										 GLsizei depth, GLint border, GLenum format, GLenum type, const void* data);
	void renderbufferStorage			(GLuint rboId, GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
	void renderbufferStorageMultisample	(GLuint rboId, GLenum target, GLsizei samples, GLenum internalFormat,
										 GLsizei width, GLsizei height);
	void bindFramebuffer				(GLenum target, GLuint fboId);
	void framebufferTexture2D			(GLenum target, GLenum attachment, GLenum textarget, GLuint texId, GLint level);
	void framebufferTextureLayer		(GLenum target, GLenum attachment, GLuint texId, GLint level, GLint layer);
	void framebufferRenderbuffer		(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint rboId);
};

void TestContext::texParameteri(GLuint texId, GLenum target, GLenum pname, GLint parameter)
{
	bindTexture(target, texId);
	gl.texParameteri(target, pname, parameter);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	bindTexture(target, 0);
}

void TestContext::bindTexture(GLenum target, GLuint texId)
{
	gl.bindTexture(target, texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");
}

void TestContext::texImage2D(GLenum target, GLuint level, GLint internalFormat, GLsizei width, GLsizei height,
							 GLint border, GLenum format, GLenum type, const void* data)
{
	gl.texImage2D(target, level, internalFormat, width, height, border, format, type, data);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D() failed");
}

void TestContext::texImage2D(GLuint texId, GLenum target, GLuint level, GLint internalFormat, GLsizei width,
							 GLsizei height, GLint border, GLenum format, GLenum type, const void* data)
{
	bindTexture(target, texId);
	texImage2D(target, level, internalFormat, width, height, border, format, type, data);
	bindTexture(target, 0);
}

void TestContext::texImage3D(GLuint texId, GLenum target, GLuint level, GLint internalFormat, GLsizei width,
							 GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data)
{
	bindTexture(target, texId);
	gl.texImage3D(target, level, internalFormat, width, height, depth, border, format, type, data);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage3D() failed");
	bindTexture(target, 0);
}

void TestContext::renderbufferStorage(GLuint rboId, GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
{
	gl.bindRenderbuffer(GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");
	gl.renderbufferStorage(target, internalFormat, width, height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glRenderbufferStorage() failed");
	gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");
}

void TestContext::renderbufferStorageMultisample(GLuint rboId, GLenum target, GLsizei samples, GLenum internalFormat,
												 GLsizei width, GLsizei height)
{
	gl.bindRenderbuffer(GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");
	gl.renderbufferStorageMultisample(target, samples, internalFormat, width, height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glRenderbufferStorageMultisample() failed");
	gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");
}

void TestContext::bindFramebuffer(GLenum target, GLuint fboId)
{
	gl.bindFramebuffer(target, fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer() failed");
}

void TestContext::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texId, GLint level)
{
	gl.framebufferTexture2D(target, attachment, textarget, texId, level);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferTexture2D() failed");
}

void TestContext::framebufferTextureLayer(GLenum target, GLenum attachment, GLuint texId, GLint level, GLint layer)
{
	gl.framebufferTextureLayer(target, attachment, texId, level, layer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferTextureLayer() failed");
}

void TestContext::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint rboId)
{
	gl.framebufferRenderbuffer(target, attachment, renderbuffertarget, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferRenderbuffer() failed");
}

typedef function<GLenum(const glu::ContextInfo& contextInfo)>		ExpectedStatusFn;
typedef function<void(TestContext& context)>						TestFn;

ExpectedStatusFn expectedStatusConstant(GLenum expectedStatus)
{
	return [expectedStatus] (const glu::ContextInfo&) { return expectedStatus; };
}

ExpectedStatusFn expectedStatusWithExtension(const string& extension, GLenum statusIfSupported, GLenum statusIfNotSupported)
{
	return [extension, statusIfSupported, statusIfNotSupported](const glu::ContextInfo &contextInfo)
	{
		if (contextInfo.isExtensionSupported(extension.c_str()))
			return statusIfSupported;
		else
			return statusIfNotSupported;
	};
}

struct TestStep
{
	TestFn				testFn;
	ExpectedStatusFn	expectedFbStatus;
};

typedef function<void(vector<TestStep>&, TestContext& context)>		StepsGeneratorFn;

struct ExtensionEntry
{
	string				name;
	bool				supportedStatus;
};

struct TestParams
{
	string					name;
	string					description;
	glu::ApiType			apiType;
	size_t					numFboIds;
	size_t					numTexIds;
	size_t					numRboIds;
	vector<TestStep>		initialSteps;
	StepsGeneratorFn		stepsGenerator;
};

const GLuint			TEXTURE_WIDTH			= 16;
const GLuint			TEXTURE_HEIGHT			= 16;
const GLuint			TEXTURE_DEPTH			= 16;

static const GLenum		cubemapTextureTargets[]	=
{
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z
};

const glu::ApiType		apiES30					= glu::ApiType::es(3, 0);
const glu::ApiType		apiES31					= glu::ApiType::es(3, 1);

bool isDifferentRboSampleCountsSupported(TestContext& testContext, GLint& maxSamples)
{
	const auto&	gl				= testContext.gl;
	gl.getIntegerv(GL_MAX_SAMPLES, &maxSamples);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed");

	if (maxSamples < 4)
		TCU_FAIL("GL_MAX_SAMPLES needs to be >= 4");

	testContext.renderbufferStorageMultisample(testContext.rboIds[0], GL_RENDERBUFFER, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
	gl.bindRenderbuffer(GL_RENDERBUFFER, testContext.rboIds[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");

	GLint		minSamplesRbo;
	gl.getRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &minSamplesRbo);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getRenderbufferParameteriv() failed");
	gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");

	return minSamplesRbo < maxSamples;
}

bool isDifferentTextureSampleCountsSupported(TestContext& testContext, GLint& maxSamples)
{
	if (glu::contextSupports(testContext.renderContext.getType(), apiES31))
	{
		const auto&	gl				= testContext.gl;
		GLint	maxColorSamples;
		gl.getIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorSamples);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed");

		GLint	maxDepthSamples;
		gl.getIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthSamples);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed");

		maxSamples = min(maxColorSamples, maxDepthSamples);

		GLuint	tempTexId;
		gl.genTextures(1, &tempTexId);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

		testContext.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, tempTexId);
		gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_TRUE);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2DMultisample() failed");

		GLint	minSamplesTex;
		gl.getTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &minSamplesTex);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTexLevelParameteriv() failed");

		testContext.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
		gl.deleteTextures(1, &tempTexId);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");

		return minSamplesTex < maxSamples;
	}
	return false;
}

/* Tests are defined as ordered series of steps that each expect a specific current framebuffer status
   after being executed. A new TestContext instance (parameter) is created for each test but all steps
   within a test use the same context. No code in addition to the framebuffer status check is executed
   between steps. */
const TestParams		tests[]					=
{
	{
		"incomplete_missing_attachment",														// string					name
		"No attachments",																		// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		0,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context) {
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"incomplete_image_zero_width",															// string					name
		"Zero width attachment image",															// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 0, TEXTURE_HEIGHT, 0,
									   GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, DE_NULL);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"incomplete_image_zero_height",															// string					name
		"Zero height attachment image",															// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 0, TEXTURE_WIDTH, 0,
									   GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, DE_NULL);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"incomplete_texture_3d_layer_oob",														// string					name
		"3D texture layer out of bounds",														// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texImage3D(context.texIds[0], GL_TEXTURE_3D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT,
									   TEXTURE_DEPTH, 0, GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.framebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, context.texIds[0], 0,
													TEXTURE_DEPTH + 1);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"incomplete_texture_2d_layer_oob",														// string					name
		"2D texture layer out of bounds",														// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 1);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"incomplete_texture_2d_mm_layer_oob",													// string					name
		"2D mipmapped texture layer out of bounds",												// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
										  GL_LINEAR_MIPMAP_LINEAR);
					context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 1);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				[](TestContext& context)
				{
					const deUint32 maxMipmapLevel = deLog2Floor32(de::max(TEXTURE_WIDTH, TEXTURE_HEIGHT));
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0],
												 maxMipmapLevel + 2);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"mutable_nbl_texture_expect_mipmap_complete",											// string					name
		"Mutable non base level texture as framebuffer attachment must be mipmap complete",		// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					DE_ASSERT(TEXTURE_WIDTH >= 8 && TEXTURE_HEIGHT >= 8);

					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_WIDTH >> 1,
									   TEXTURE_HEIGHT >> 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 3, GL_RGBA8, TEXTURE_WIDTH >> 3,
									   TEXTURE_HEIGHT >> 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
					context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 1);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 2, GL_RGBA8, TEXTURE_WIDTH >> 2,
									   TEXTURE_HEIGHT >> 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"mutable_nbl_texture_expect_cube_complete",												// string					name
		"Mutable non base level texture as framebuffer attachment must be cube complete",		// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.texParameteri(context.texIds[0], GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 1);
					context.texParameteri(context.texIds[0], GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
					for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(cubemapTextureTargets); ++i)
					{
						if (i % 2)
							continue;
						context.texImage2D(cubemapTextureTargets[i], 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA,
										   GL_UNSIGNED_BYTE, DE_NULL);
						context.texImage2D(cubemapTextureTargets[i], 1, GL_RGBA8, TEXTURE_WIDTH >> 1, TEXTURE_HEIGHT >> 1, 0, GL_RGBA,
										   GL_UNSIGNED_BYTE, DE_NULL);
					}
					context.bindTexture(GL_TEXTURE_CUBE_MAP, 0);

					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cubemapTextureTargets[0],
												 context.texIds[0], 1);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
					for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(cubemapTextureTargets); ++i)
					{
						if (i % 2 == 0)
							continue;
						context.texImage2D(cubemapTextureTargets[i], 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA,
										   GL_UNSIGNED_BYTE, DE_NULL);
						context.texImage2D(cubemapTextureTargets[i], 1, GL_RGBA8, TEXTURE_WIDTH >> 1, TEXTURE_HEIGHT >> 1, 0, GL_RGBA,
										   GL_UNSIGNED_BYTE, DE_NULL);
					}
					context.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"expect_renderable_internal_format",													// string					name
		"Color/Depth/Stencil attachment texture must have a color/depth/stencil"				// string					description
		" renderable internal format",
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		3,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.texImage2D(context.texIds[1], GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH,
									   TEXTURE_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, DE_NULL);
					context.texImage2D(context.texIds[2], GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH, TEXTURE_HEIGHT,
									   0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, DE_NULL);
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, context.texIds[1], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, context.texIds[2], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			}
		},
		[](vector<TestStep>& steps, TestContext& testContext)									// StepsGeneratorFn			stepsGenerator
		{
			GLint	maxColorAttachmentsCount;
			testContext.gl.getIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachmentsCount);
			GLU_EXPECT_NO_ERROR(testContext.gl.getError(), "glGetInteger() failed");

			steps.reserve(steps.size() + 2 * maxColorAttachmentsCount);
			for (GLint i = 0; i < maxColorAttachmentsCount; ++i)
			{
				steps.push_back(
					{
						[i](TestContext& context)
						{
							context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
														 context.texIds[1], 0);
						},
						expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
					});
				steps.push_back(
					{
						[i](TestContext& context)
						{
							context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
														 context.texIds[0], 0);
						},
						expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
					});
			}
		},
	},
	{
		"all_rbos_expect_same_numsamples",														// string					name
		"Same value of FRAMEBUFFER_SAMPLES for all attached render buffers",					// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		0,																						// size_t					numTexIds
		2,																						// size_t					numRboIds
		{},																						// vector<TestStep>			initialSteps
		[](vector<TestStep>& steps, TestContext& testContext)									// StepsGeneratorFn			stepsGenerator
		{
			GLint	maxSamples;
			if (!isDifferentRboSampleCountsSupported(testContext, maxSamples))
				return;

			steps.push_back(
				{
					[](TestContext& context)
					{
						context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.renderbufferStorage(context.rboIds[0], GL_RENDERBUFFER, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
						context.renderbufferStorage(context.rboIds[1], GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH,
													TEXTURE_HEIGHT);

						context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
														context.rboIds[0]);
						context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
														context.rboIds[1]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
				});
			steps.push_back(
				{
					[maxSamples](TestContext& context)
					{
						context.renderbufferStorageMultisample(context.rboIds[0], GL_RENDERBUFFER, maxSamples, GL_RGBA8, TEXTURE_WIDTH,
															   TEXTURE_HEIGHT);
						context.renderbufferStorageMultisample(context.rboIds[1], GL_RENDERBUFFER, 1, GL_DEPTH24_STENCIL8,
															   TEXTURE_WIDTH, TEXTURE_HEIGHT);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
				});
		},
	},
	{
		"rbo_and_texture_expect_zero_numsamples",												// string					name
		"When using mixed renderbuffer and texture attachments, the value of"					// string					description
		" FRAMEBUFFER_SAMPLES needs to be zero for all attached renderbuffers",
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		2,																						// size_t					numTexIds
		2,																						// size_t	u				numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				[](TestContext& context)
				{
					context.renderbufferStorage(context.rboIds[0], GL_RENDERBUFFER, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
					context.texImage2D(context.texIds[1], GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH,
									   TEXTURE_HEIGHT, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, DE_NULL);

					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
													context.rboIds[0]);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
												 context.texIds[1], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				[](TestContext& context)
				{
					context.renderbufferStorage(context.rboIds[1], GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH,
												TEXTURE_HEIGHT);
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);

					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
													context.rboIds[1]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				[](TestContext& context)
				{
					context.renderbufferStorageMultisample(context.rboIds[1], GL_RENDERBUFFER, 2, GL_DEPTH24_STENCIL8,
														   TEXTURE_WIDTH, TEXTURE_HEIGHT);
				},
				expectedStatusWithExtension("GL_NV_framebuffer_mixed_samples", GL_FRAMEBUFFER_COMPLETE, GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
			},
			{
				[](TestContext& context)
				{
					context.renderbufferStorageMultisample(context.rboIds[0], GL_RENDERBUFFER, 3, GL_RGBA8, TEXTURE_WIDTH,
														   TEXTURE_HEIGHT);

					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
													context.rboIds[0]);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
												 context.texIds[1], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
			},
			{
				[](TestContext& context)
				{
					context.renderbufferStorageMultisample(context.rboIds[0], GL_RENDERBUFFER, 0, GL_RGBA8, TEXTURE_WIDTH,
														   TEXTURE_HEIGHT);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"expect_equal_numsamples",																// string					name
		"The value of samples for each attached target must be equal",							// string					description
		apiES31,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		4,																						// size_t					numTexIds
		2,																						// size_t					numRboIds
		{},																						// vector<TestStep>			initialSteps
		[](vector<TestStep>& steps, TestContext& testContext)									// StepsGeneratorFn			stepsGenerator
		{
			GLint	maxRboSamples, maxTextureSamples;
			if (!isDifferentRboSampleCountsSupported(testContext, maxRboSamples) || !isDifferentTextureSampleCountsSupported(testContext, maxTextureSamples))
				return;

			steps.push_back(
				{
					[maxRboSamples, maxTextureSamples](TestContext& context)
					{
						// Set up textures and renderbuffers for all following steps, complete = (tex0, rbo1) or (tex1, rbo0) */
						context.renderbufferStorageMultisample(context.rboIds[0], GL_RENDERBUFFER, maxRboSamples, GL_RGBA8,
															   TEXTURE_WIDTH, TEXTURE_HEIGHT);
						context.renderbufferStorageMultisample(context.rboIds[1], GL_RENDERBUFFER, 1, GL_DEPTH24_STENCIL8,
															   TEXTURE_WIDTH, TEXTURE_HEIGHT);

						const auto&	gl	= context.gl;
						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, context.texIds[0]);
						gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_TRUE);
						GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2DMultisample() failed");

						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, context.texIds[1]);
						gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_TRUE);
						GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2DMultisample() failed");

						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, context.texIds[2]);
						gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, maxTextureSamples, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_TRUE);
						GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2DMultisample() failed");

						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, context.texIds[3]);
						gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, maxTextureSamples, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_TRUE);
						GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2DMultisample() failed");

						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

						// Framebuffer binding for rest of this test
						context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[0], 0);
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[1], 0);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[3], 0);
					},
					expectedStatusWithExtension("GL_NV_framebuffer_mixed_samples", GL_FRAMEBUFFER_COMPLETE, GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
														context.rboIds[1]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
														context.rboIds[0]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[2], 0);
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[3], 0);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						const auto&	gl	= context.gl;
						gl.deleteTextures(1, &context.texIds[0]);
						GLU_EXPECT_NO_ERROR(context.gl.getError(), "glDeleteTextures() failed");
						gl.genTextures(1, &context.texIds[0]);
						GLU_EXPECT_NO_ERROR(context.gl.getError(), "glGenTextures() failed");
						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, context.texIds[0]);
						gl.texStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_FALSE);
						GLU_EXPECT_NO_ERROR(context.gl.getError(), "glTexStorage2DMultisample() failed");
						context.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[0], 0);
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE,
													 context.texIds[1], 0);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
				});
		},																						// StepsGeneratorFn			stepsGenerator
	},
	{
		"status_tracking",																		// string					name
		"Modifying framebuffer attached objects correctly updates the fbo status",				// string					description
		apiES30,																				// glu::ApiType				apiType
		3,																						// size_t					numFboIds
		2,																						// size_t					numTexIds
		1,																						// size_t					numRboIds
		{																						// vector<TestStep>			initialSteps
			{
				// Initial status -> missing_attachment
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				// Allocate and attach texture -> complete
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
									   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Detach texture from fbo -> missing_attachment
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				// Allocate and attach renderbuffer -> complete
				[](TestContext& context)
				{
					context.renderbufferStorage(context.rboIds[0], GL_RENDERBUFFER, GL_RGBA8, TEXTURE_WIDTH,
												TEXTURE_HEIGHT);
					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
													context.rboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Detach renderbuffer -> incomplete
				[](TestContext& context)
				{
					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				// Switch to incomplete fb -> missing_attachment
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[1]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				// Attach texture to fbo -> complete
				[](TestContext& context)
				{
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Change image format of attached texture -> incomplete_attachment
				[](TestContext& context)
				{
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH,
									   TEXTURE_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, DE_NULL);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				// Change image format (tex storage) -> complete
				[](TestContext& context)
				{
					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
					context.gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
					GLU_EXPECT_NO_ERROR(context.gl.getError(), "glTexStorage2D() failed");
					context.bindTexture(GL_TEXTURE_2D, 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Delete image -> missing_attachment
				[](TestContext& context)
				{
					context.gl.deleteTextures(1, &context.texIds[0]);
					GLU_EXPECT_NO_ERROR(context.gl.getError(), "glDeleteTextures() failed");
					context.texIds.erase(context.texIds.begin());
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			},
			{
				// Recreate image in wrong format, attach to color attachment -> incomplete_attachment
				[](TestContext& context)
				{
					const auto&	gl		= context.gl;
					GLuint		texId;
					gl.genTextures(1, &texId);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
					context.texIds.push_back(texId);

					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
					context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH,
									   TEXTURE_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, DE_NULL);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				// Format to rgba8 using copyTexImage2D from compatible fbo -> framebuffer_complete
				[](TestContext& context)
				{
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[2]);
					context.texImage2D(context.texIds[1], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
					context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[1], 0);

					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
					const auto&	gl	= context.gl;
					gl.copyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCopyTexImage2D() failed");

					context.bindTexture(GL_TEXTURE_2D, 0);
					context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Change currently attached texture's format to compressed tex image -> incomplete_attachment (non color renderable)
				[](TestContext& context)
				{
					DE_ASSERT(TEXTURE_WIDTH == 16 && TEXTURE_HEIGHT == 16);
					static const glw::GLubyte textureDataETC2[] = // 16x16 all black RGBA8 texture in ETC2 format
					{
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x00
					};
					const auto&	gl	= context.gl;
					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);

					gl.compressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, TEXTURE_WIDTH, TEXTURE_HEIGHT,
											0, DE_LENGTH_OF_ARRAY(textureDataETC2), textureDataETC2);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");

					context.bindTexture(GL_TEXTURE_2D, 0);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				// Re-attach rbo0 -> complete
				[](TestContext& context)
				{
					context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
													context.rboIds[0]);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE)
			},
			{
				// Rbo storage to non renderable format -> incomplete_attachment
				[](TestContext& context)
				{
					context.renderbufferStorage(context.rboIds[0], GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH,
												TEXTURE_HEIGHT);
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
			},
			{
				// Delete rbo -> missing_attachment
				[](TestContext& context)
				{
					context.gl.deleteRenderbuffers(1, &context.rboIds[0]);
					GLU_EXPECT_NO_ERROR(context.gl.getError(), "glDeleteRenderbuffers() failed");
					context.rboIds.erase(context.rboIds.begin());
				},
				expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
			}
		},
		DE_NULL,																				// StepsGeneratorFn			stepsGenerator
	},
	{
		"mutable_texture_missing_attachment_level",												// string					name
		"Attaching a mutable texture with undefined image for attachment level"
		" should be invalid",																	// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{},																						// vector<TestStep>			initialSteps
		[](vector<TestStep>& steps, TestContext&)												// StepsGeneratorFn			stepsGenerator
		{
			DE_ASSERT(TEXTURE_WIDTH >= 16 && TEXTURE_HEIGHT >= 16);
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{

						context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
										   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
						context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_WIDTH >> 1, TEXTURE_HEIGHT >> 1, 0,
										   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);
						context.texImage2D(context.texIds[0], GL_TEXTURE_2D, 3, GL_RGBA8, TEXTURE_WIDTH >> 3, TEXTURE_HEIGHT >> 3, 0,
										   GL_RGBA, GL_UNSIGNED_BYTE, DE_NULL);

						context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 2);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
				});
		}
	},
	{
		"immutable_texture_any_level_as_attachment",											// string					name
		"Any level of immutable texture as attachment should be valid",							// string					description
		apiES30,																				// glu::ApiType				apiType
		1,																						// size_t					numFboIds
		1,																						// size_t					numTexIds
		0,																						// size_t					numRboIds
		{},																						// vector<TestStep>			initialSteps
		[](vector<TestStep>& steps, TestContext&)												// StepsGeneratorFn			stepsGenerator
		{
			DE_ASSERT(TEXTURE_WIDTH >= 8 && TEXTURE_HEIGHT >= 8);
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.bindFramebuffer(GL_FRAMEBUFFER, context.fboIds[0]);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
						const auto&	gl	= context.gl;
						gl.texStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
						GLU_EXPECT_NO_ERROR(gl.getError(), "glTexStorage2D() failed");
						context.bindTexture(GL_TEXTURE_2D, 0);

						context.texParameteri(context.texIds[0], GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 2);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE),
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 1);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE),
				});
			steps.push_back(
				{
					[](TestContext& context)
					{
						context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, context.texIds[0], 0);
					},
					expectedStatusConstant(GL_FRAMEBUFFER_COMPLETE),
				});
		}
	},
};

class FramebufferCompletenessTestCase : public deqp::TestCase
{
public:
	FramebufferCompletenessTestCase				(deqp::Context& context, const TestParams& params);
	virtual ~FramebufferCompletenessTestCase	();

	virtual void			init				(void);
	virtual void			deinit				(void);
	TestNode::IterateResult iterate				(void);

private:
	bool verifyFramebufferStatus				(const glw::Functions& gl, const ExpectedStatusFn expectedStatusFn, const size_t stepIndex);

	const TestParams		m_params;
	vector<GLuint>			m_fboIds;
	vector<GLuint>			m_texIds;
	vector<GLuint>			m_rboIds;
};

FramebufferCompletenessTestCase::FramebufferCompletenessTestCase(deqp::Context& context, const TestParams& params)
	: deqp::TestCase(context, params.name.c_str(), params.description.c_str()), m_params(params)
{
}

FramebufferCompletenessTestCase::~FramebufferCompletenessTestCase()
{
}

void FramebufferCompletenessTestCase::init(void)
{
	const auto& renderContext	= m_context.getRenderContext();
	const auto& gl				= renderContext.getFunctions();

	if (m_params.numFboIds > 0)
	{
		m_fboIds.resize(m_params.numFboIds);
		gl.genFramebuffers(m_params.numFboIds, m_fboIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenFramebuffers() failed");
	}
	if (m_params.numTexIds > 0)
	{
		m_texIds.resize(m_params.numTexIds);
		gl.genTextures(m_params.numTexIds, m_texIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
	}
	if (m_params.numRboIds > 0)
	{
		m_rboIds.resize(m_params.numRboIds);
		gl.genRenderbuffers(m_params.numRboIds, m_rboIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenRenderbuffers() failed");
	}
}

void FramebufferCompletenessTestCase::deinit(void)
{
	const auto& renderContext	= m_context.getRenderContext();
	const auto& gl				= renderContext.getFunctions();

	if (!m_rboIds.empty())
	{
		gl.deleteRenderbuffers(m_params.numRboIds, m_rboIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteRenderbuffers() failed");
		m_rboIds.clear();
	}
	if (!m_texIds.empty())
	{
		gl.deleteTextures(m_params.numTexIds, m_texIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");
		m_texIds.clear();
	}
	if (!m_fboIds.empty())
	{
		gl.deleteFramebuffers(m_params.numFboIds, m_fboIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteFramebufers() failed");
		m_fboIds.clear();
	}
}

tcu::TestNode::IterateResult FramebufferCompletenessTestCase::iterate(void)
{
	const auto& renderContext	= m_context.getRenderContext();
	const auto& gl				= renderContext.getFunctions();
	TestContext context			=
	{
		renderContext,	// const glu::RenderContext&	renderContext
		gl,				// const glw::Functions&		gl
		m_fboIds,		// vector<GLuint>&				fboIds
		m_texIds,		// vector<GLuint>&				texIds
		m_rboIds		// vector<GLuint>&				rboIds
	};
	auto steps = vector<TestStep>(m_params.initialSteps);
	if (m_params.stepsGenerator != DE_NULL)
		m_params.stepsGenerator(steps, context);

	if (steps.empty())
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

	size_t stepIndex = 0;
	for (const auto& step : steps)
	{
		step.testFn(context);

		if (!verifyFramebufferStatus(gl, step.expectedFbStatus, stepIndex++))
			return STOP;
	}
	return STOP;
}

bool FramebufferCompletenessTestCase::verifyFramebufferStatus(const glw::Functions& gl, const ExpectedStatusFn expectedStatusFn,
												  const size_t stepIndex)
{
	static const map<GLenum, string>	statusNames	=
	{
		{ GL_FRAMEBUFFER_COMPLETE						, "GL_FRAMEBUFFER_COMPLETE" },
		{ GL_FRAMEBUFFER_UNDEFINED						, "GL_FRAMEBUFFER_UNDEFINED" },
		{ GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT			, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" },
		{ GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT	, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" },
		{ GL_FRAMEBUFFER_UNSUPPORTED					, "GL_FRAMEBUFFER_UNSUPPORTED" },
		{ GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE			, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE" },
		{ GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS			, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS" }
	};
	const auto expectedStatus	= expectedStatusFn(m_context.getContextInfo());
	const auto fboStatus		= gl.checkFramebufferStatus(GL_FRAMEBUFFER);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCheckFramebufferStatus() failed");
	if (fboStatus != expectedStatus)
	{
		ostringstream					msg;
		const auto&						fboStatusName		= statusNames.find(fboStatus);
		const auto&						expectedStatusName	= statusNames.find(expectedStatus);
		msg << "Frame buffer status ("
			<< ((fboStatusName != statusNames.end()) ? fboStatusName->second : std::to_string(fboStatus))
			<< ") does not match the expected status ("
			<< ((expectedStatusName != statusNames.end()) ? expectedStatusName->second : std::to_string(expectedStatus))
			<< ") after step " << stepIndex;
		TCU_FAIL(msg.str().c_str());
		return false;
	}
	m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return true;
}

} // namespace

FramebufferCompletenessTests::FramebufferCompletenessTests(deqp::Context& context)
	: deqp::TestCaseGroup(context, "framebuffer_completeness", "Tests for frame buffer completeness")
{
}

void FramebufferCompletenessTests::init(void)
{
	const auto& renderContext	= m_context.getRenderContext();
	for (const auto& test : tests)
	{
		if (!glu::contextSupports(renderContext.getType(), test.apiType))
			continue;

		addChild(new FramebufferCompletenessTestCase(m_context, test));
	}
}

} // namespace es3cts
