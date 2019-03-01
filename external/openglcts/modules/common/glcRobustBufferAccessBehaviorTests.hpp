#ifndef _GLCROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
#define _GLCROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
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
 * \file  glcRobustBufferAccessBehaviorTests.hpp
 * \brief Declares test classes for "Robust Buffer Access Behavior" functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace RobustBufferAccessBehavior
{
/** Replace first occurance of <token> with <text> in <string> starting at <search_posistion>
 **/
void replaceToken(const glw::GLchar* token, size_t& search_position, const glw::GLchar* text, std::string& string);

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

/** Implementation of test VertexBufferObjects. Description follows:
 *
 * This test verifies that any "out-of-bound" read from vertex buffer result with abnormal program exit
 *
 * Steps:
 * - prepare vertex buffer with the following vertices:
 *   * 0 - [ 0,  0, 0],
 *   * 1 - [-1,  0, 0],
 *   * 2 - [-1,  1, 0],
 *   * 3 - [ 0,  1, 0],
 *   * 4 - [ 1,  1, 0],
 *   * 5 - [ 1,  0, 0],
 *   * 6 - [ 1, -1, 0],
 *   * 7 - [ 0, -1, 0],
 *   * 8 - [-1, -1, 0];
 * - prepare element buffer:
 *   * valid:
 *     0, 1, 2,
 *     0, 2, 3,
 *     0, 3, 4,
 *     0, 4, 5,
 *     0, 5, 6,
 *     0, 6, 7,
 *     0, 7, 8,
 *     0, 8, 1;
 *   * invalid:
 *      9, 1, 2,
 *     10, 2, 3,
 *     11, 3, 4,
 *     12, 4, 5,
 *     13, 5, 6,
 *     14, 6, 7,
 *     15, 7, 8,
 *     16, 8, 1;
 * - prepare program consisting of vertex and fragment shader that will output
 * value 1;
 * - prepare framebuffer with R8UI texture attached as color 0, filled with
 * value 128;
 * - execute draw call with invalid element buffer;
 * - inspect contents of framebuffer, it is expected that it is filled with
 * value 1;
 * - clean framebuffer to value 128;
 * - execute draw call with valid element buffer;
 * - inspect contents of framebuffer, it is expected that it is filled with
 * value 1.
 **/
class VertexBufferObjectsTest : public TestCase
{
public:
	/* Public methods */
	VertexBufferObjectsTest(deqp::Context& context);
	VertexBufferObjectsTest(deqp::Context& context, const char* name, const char* description);
	virtual ~VertexBufferObjectsTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected methods */
	virtual std::string getFragmentShader();
	virtual std::string getVertexShader();
	virtual void cleanTexture(glw::GLuint texture_id);
	virtual bool verifyInvalidResults(glw::GLuint texture_id);
	virtual bool verifyValidResults(glw::GLuint texture_id);
	virtual bool verifyResults(glw::GLuint texture_id);
};

/** Implementation of test TexelFetch. Description follows:
 *
 * This test verifies that any "out-of-bound" fetch from texture result in
 * "zero".
 *
 * Steps:
 * - prepare program consisting of vertex, geometry and fragment shader that
 * will output full-screen quad; Each fragment should receive value of
 * corresponding texel from source texture; Use texelFetch function;
 * - prepare 16x16 2D R8UI source texture filled with unique values;
 * - prepare framebuffer with 16x16 R8UI texture as color attachment, filled
 * with value 0;
 * - execute draw call;
 * - inspect contents of framebuffer, it is expected to match source texture;
 * - modify program so it will fetch invalid texels;
 * - execute draw call;
 * - inspect contents of framebuffer, it is expected that it will be filled
 * with value 0 for RGB channels and with 0, 1 or the biggest representable
 * integral number for alpha channel.
 *
 * Repeat steps for:
 * - R8 texture;
 * - RG8_SNORM texture;
 * - RGBA32F texture;
 * - mipmap at level 1;
 * - a texture with 4 samples.
 **/

class TexelFetchTest : public TestCase
{
public:
	/* Public methods */
	TexelFetchTest(deqp::Context& context);
	TexelFetchTest(deqp::Context& context, const glw::GLchar* name, const glw::GLchar* description);
	virtual ~TexelFetchTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected enums */
	enum TEST_CASES
	{
		R8,
		RG8_SNORM,
		R32UI_MULTISAMPLE,
		RGBA32F,
		R32UI_MIPMAP,
		/* */
		LAST
	};

	enum VERSION
	{
		VALID,
		SOURCE_INVALID,
		DESTINATION_INVALID,
	};

	/* Protected methods */
	virtual const glw::GLchar* getTestCaseName() const;
	virtual void prepareTexture(bool is_source, glw::GLuint texture_id);

	/* Protected fields */
	TEST_CASES m_test_case;

protected:
	/* Protected methods */
	virtual std::string getFragmentShader(bool is_case_valid);
	virtual std::string getGeometryShader();
	virtual std::string getVertexShader();
	virtual bool verifyInvalidResults(glw::GLuint texture_id);
	virtual bool verifyValidResults(glw::GLuint texture_id);
};

/** Implementation of test ImageLoadStore. Description follows:
 *
 * This test verifies that any "out-of-bound" access to image result in "zero"
 * or is discarded.
 *
 * Modify TexelFetch test in the following aspects:
 * - use compute shader instead of "draw" pipeline;
 * - use imageLoad instead of texelFetch;
 * - use destination image instead of framebuffer; Store texel with imageStore;
 * - for each case from TexelFetch verify:
 *   * valid coordinates for source and destination images;
 *   * invalid coordinates for destination and valid ones for source image;
 *   * valid coordinates for destination and invalid ones for source image.
 **/
class ImageLoadStoreTest : public TexelFetchTest
{
public:
	/* Public methods */
	ImageLoadStoreTest(deqp::Context& context);
	ImageLoadStoreTest(deqp::Context& context, const char* name, const char* description);
	virtual ~ImageLoadStoreTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected methods */
	virtual std::string getComputeShader(VERSION version);
	virtual void setTextures(glw::GLuint id_destination, glw::GLuint id_source);
	virtual bool verifyInvalidResults(glw::GLuint texture_id);
	virtual bool verifyValidResults(glw::GLuint texture_id);
};

/** Implementation of test StorageBuffer. Description follows:
 *
 * This test verifies that any "out-of-bound" access to buffer result in zero
 * or is discarded.
 *
 * Steps:
 * - prepare compute shader based on the following code snippet:
 *
 *     uint dst_index         = gl_LocalInvocationID.x;
 *     uint src_index         = gl_LocalInvocationID.x;
 *     destination[dst_index] = source[src_index];
 *
 * where source and destination are storage buffers, defined as unsized arrays
 * of floats;
 * - prepare two buffers of 4 floats:
 *   * destination filled with value 1;
 *   * source filled with unique values;
 * - dispatch program to copy all 4 values;
 * - inspect program to verify that contents of source buffer were copied to
 * destination;
 * - repeat steps for the following cases:
 *   * value of dst_index is equal to gl_LocalInvocationID.x + 16; It is
 *   expected that destination buffer will not be modified;
 *   * value of src_index is equal to gl_LocalInvocationID.x + 16; It is
 *   expected that destination buffer will be filled with value 0.
 **/
class StorageBufferTest : public TestCase
{
public:
	/* Public methods */
	StorageBufferTest(deqp::Context& context);
	StorageBufferTest(deqp::Context& context, const char* name, const char* description);
	virtual ~StorageBufferTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected enums */
	enum VERSION
	{
		VALID,
		SOURCE_INVALID,
		DESTINATION_INVALID,
		/* */
		LAST
	};

	/* Private methods */
	virtual std::string getComputeShader();
	virtual bool verifyResults(glw::GLfloat* buffer_data);

	/* Protected fields */
	VERSION m_test_case;
	bool m_hasKhrRobustBufferAccess;

	/* Protected constants */
	static const glw::GLfloat m_destination_data[4];
	static const glw::GLfloat m_source_data[4];
};

/** Implementation of test UniformBuffer. Description follows:
 *
 * This test verifies that any "out-of-bound" read from uniform buffer result
 * in zero;
 *
 * Modify StorageBuffer test in the following aspects:
 * - use uniform buffer for source instead of storage buffer;
 * - ignore the case with invalid value of dst_index.
 **/
class UniformBufferTest : public TestCase
{
public:
	/* Public methods */
	UniformBufferTest(deqp::Context& context);
	UniformBufferTest(deqp::Context& context, const char* name, const char* description);
	virtual ~UniformBufferTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected enums */
	enum VERSION
	{
		VALID,
		SOURCE_INVALID,
		/* */
		LAST
	};

	/* Protected methods */
	virtual std::string getComputeShader();
	virtual bool verifyResults(glw::GLfloat* buffer_data);

	/* Protected fields */
	VERSION m_test_case;
};
} /* RobustBufferAccessBehavior */

/** Group class for multi bind conformance tests */
class RobustBufferAccessBehaviorTests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	RobustBufferAccessBehaviorTests(deqp::Context& context);
	virtual ~RobustBufferAccessBehaviorTests(void)
	{
	}

	virtual void init(void);

private:
	/* Private methods */
	RobustBufferAccessBehaviorTests(const RobustBufferAccessBehaviorTests& other);
	RobustBufferAccessBehaviorTests& operator=(const RobustBufferAccessBehaviorTests& other);
};

} /* deqp */

#endif // _GLCROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
