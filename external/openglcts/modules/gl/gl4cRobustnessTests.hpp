#ifndef _GL4CROBUSTNESSTESTS_HPP
#define _GL4CROBUSTNESSTESTS_HPP
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
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl4cRobustnessTests.hpp
 * \brief Conformance tests for the Robustness feature functionality.
 */ /*-----------------------------------------------------------------------------*/

/* Includes. */

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include "gluDefs.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "glcContext.hpp"
#include "glcTestCase.hpp"
#include "glcTestPackage.hpp"

#include "glcRobustBufferAccessBehaviorTests.hpp"

namespace gl4cts
{
namespace RobustnessRobustBufferAccessBehavior
{
/** Represents buffer instance
 * Provides basic buffer functionality
 **/
class Buffer
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	Buffer(deqp::Context& context);
	~Buffer();

	/* Init & Release */
	void InitData(glw::GLenum target, glw::GLenum usage, glw::GLsizeiptr size, const glw::GLvoid* data);

	void Release();

	/* Functionality */
	void Bind() const;
	void BindBase(glw::GLuint index) const;

	/* Public static routines */
	/* Functionality */
	static void Bind(const glw::Functions& gl, glw::GLuint id, glw::GLenum target);

	static void BindBase(const glw::Functions& gl, glw::GLuint id, glw::GLenum target, glw::GLuint index);

	static void Data(const glw::Functions& gl, glw::GLenum target, glw::GLenum usage, glw::GLsizeiptr size,
					 const glw::GLvoid* data);

	static void Generate(const glw::Functions& gl, glw::GLuint& out_id);

	static void SubData(const glw::Functions& gl, glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr size,
						glw::GLvoid* data);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;
	static const glw::GLuint m_n_targets = 13;
	static const glw::GLenum m_targets[m_n_targets];

private:
	/* Private enums */

	/* Private fields */
	deqp::Context& m_context;
	glw::GLenum	m_target;
};

/** Represents framebuffer
 * Provides basic functionality
 **/
class Framebuffer
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	Framebuffer(deqp::Context& context);
	~Framebuffer();

	/* Init & Release */
	void Release();

	/* Public static routines */
	static void AttachTexture(const glw::Functions& gl, glw::GLenum target, glw::GLenum attachment,
							  glw::GLuint texture_id, glw::GLint level, glw::GLuint width, glw::GLuint height);

	static void Bind(const glw::Functions& gl, glw::GLenum target, glw::GLuint id);

	static void Generate(const glw::Functions& gl, glw::GLuint& out_id);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;

private:
	/* Private fields */
	deqp::Context& m_context;
};

/** Represents shader instance.
 * Provides basic functionality for shaders.
 **/
class Shader
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	Shader(deqp::Context& context);
	~Shader();

	/* Init & Realese */
	void Init(glw::GLenum stage, const std::string& source);
	void Release();

	/* Public static routines */
	/* Functionality */
	static void Compile(const glw::Functions& gl, glw::GLuint id);

	static void Create(const glw::Functions& gl, glw::GLenum stage, glw::GLuint& out_id);

	static void Source(const glw::Functions& gl, glw::GLuint id, const std::string& source);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;

private:
	/* Private fields */
	deqp::Context& m_context;
};

/** Represents program instance.
 * Provides basic functionality
 **/
class Program
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	Program(deqp::Context& context);
	~Program();

	/* Init & Release */
	void Init(const std::string& compute_shader, const std::string& fragment_shader, const std::string& geometry_shader,
			  const std::string& tesselation_control_shader, const std::string& tesselation_evaluation_shader,
			  const std::string& vertex_shader);

	void Release();

	/* Functionality */
	void Use() const;

	/* Public static routines */
	/* Functionality */
	static void Attach(const glw::Functions& gl, glw::GLuint program_id, glw::GLuint shader_id);

	static void Create(const glw::Functions& gl, glw::GLuint& out_id);

	static void Link(const glw::Functions& gl, glw::GLuint id);

	static void Use(const glw::Functions& gl, glw::GLuint id);

	/* Public fields */
	glw::GLuint m_id;

	Shader m_compute;
	Shader m_fragment;
	Shader m_geometry;
	Shader m_tess_ctrl;
	Shader m_tess_eval;
	Shader m_vertex;

	/* Public constants */
	static const glw::GLuint m_invalid_id;

private:
	/* Private fields */
	deqp::Context& m_context;
};

/** Represents texture instance
 **/
class Texture
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	Texture(deqp::Context& context);
	~Texture();

	/* Init & Release */
	void Release();

	/* Public static routines */
	/* Functionality */
	static void Bind(const glw::Functions& gl, glw::GLuint id, glw::GLenum target);

	static void CompressedImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level,
								glw::GLenum internal_format, glw::GLuint width, glw::GLuint height, glw::GLuint depth,
								glw::GLsizei image_size, const glw::GLvoid* data);

	static void Generate(const glw::Functions& gl, glw::GLuint& out_id);

	static void GetData(const glw::Functions& gl, glw::GLint level, glw::GLenum target, glw::GLenum format,
						glw::GLenum type, glw::GLvoid* out_data);

	static void GetData(const glw::Functions& gl, glw::GLuint id, glw::GLint level, glw::GLuint width,
						glw::GLuint height, glw::GLenum format, glw::GLenum type, glw::GLvoid* out_data);

	static void GetLevelParameter(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLenum pname,
								  glw::GLint* param);

	static void Image(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLenum internal_format,
					  glw::GLuint width, glw::GLuint height, glw::GLuint depth, glw::GLenum format, glw::GLenum type,
					  const glw::GLvoid* data);

	static void Storage(const glw::Functions& gl, glw::GLenum target, glw::GLsizei levels, glw::GLenum internal_format,
						glw::GLuint width, glw::GLuint height, glw::GLuint depth);

	static void SubImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLint x, glw::GLint y,
						 glw::GLint z, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format,
						 glw::GLenum type, const glw::GLvoid* pixels);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;

private:
	/* Private fields */
	deqp::Context& m_context;
};

/** Represents Vertex array object
 * Provides basic functionality
 **/
class VertexArray
{
public:
	/* Public methods */
	/* Ctr & Dtr */
	VertexArray(deqp::Context& Context);
	~VertexArray();

	/* Init & Release */
	void Release();

	/* Public static methods */
	static void Bind(const glw::Functions& gl, glw::GLuint id);

	static void Generate(const glw::Functions& gl, glw::GLuint& out_id);

	/* Public fields */
	glw::GLuint m_id;

	/* Public constants */
	static const glw::GLuint m_invalid_id;

private:
	/* Private fields */
	deqp::Context& m_context;
};

/** Implementation of test GetnUniformTest. Description follows:
 *
 * This test verifies if read uniform variables to the buffer with bufSize less than expected result with GL_INVALID_OPERATION error;
 **/
class GetnUniformTest : public deqp::TestCase
{
public:
	/* Public methods */
	GetnUniformTest(deqp::Context& context);
	virtual ~GetnUniformTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

private:
	/* Private methods */
	std::string getComputeShader();

	bool verifyResult(const void* inputData, const void* resultData, int size, const char* method);
	bool verifyError(glw::GLint error, glw::GLint expectedError, const char* method);

	/* Private GL funtion pointers. */
	typedef void(GLW_APIENTRY* PFNGLGETNUNIFORMFV)(glw::GLuint program, glw::GLint location, glw::GLsizei bufSize,
												   glw::GLfloat* params);
	typedef void(GLW_APIENTRY* PFNGLGETNUNIFORMIV)(glw::GLuint program, glw::GLint location, glw::GLsizei bufSize,
												   glw::GLint* params);
	typedef void(GLW_APIENTRY* PFNGLGETNUNIFORMUIV)(glw::GLuint program, glw::GLint location, glw::GLsizei bufSize,
													glw::GLuint* params);

	PFNGLGETNUNIFORMFV  m_pGetnUniformfv;
	PFNGLGETNUNIFORMIV  m_pGetnUniformiv;
	PFNGLGETNUNIFORMUIV m_pGetnUniformuiv;
};

/** Implementation of test ReadnPixelsTest. Description follows:
 *
 * This test verifies if read pixels to the buffer with bufSize less than expected result with GL_INVALID_OPERATION error;
 **/
class ReadnPixelsTest : public deqp::TestCase
{
public:
	/* Public methods */
	ReadnPixelsTest(deqp::Context& context);
	virtual ~ReadnPixelsTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

private:
	/* Private methods */
	void cleanTexture(glw::GLuint texture_id);
	bool verifyResults();
	bool verifyError(glw::GLint error, glw::GLint expectedError, const char* method);

	/* Private GL funtion pointers. */
	typedef void(GLW_APIENTRY* PFNGLREADNPIXELS)(glw::GLint x, glw::GLint y, glw::GLsizei width, glw::GLsizei height,
												 glw::GLenum format, glw::GLenum type, glw::GLsizei bufSize,
												 glw::GLvoid* data);

	PFNGLREADNPIXELS m_pReadnPixels;
};

} // RobustBufferAccessBehavior namespace

class RobustnessTests : public deqp::TestCaseGroup
{
public:
	RobustnessTests(deqp::Context& context);
	//virtual ~RobustnessTests(void);

	virtual void init(void);

private:
	RobustnessTests(const RobustnessTests& other);
	RobustnessTests& operator=(const RobustnessTests& other);
};

} // namespace gl4cts

#endif // _GL4CROBUSTNESSTESTS_HPP
