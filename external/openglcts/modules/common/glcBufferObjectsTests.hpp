#ifndef _GLCBUFFEROBJECTSTESTS_HPP
#define _GLCBUFFEROBJECTSTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file  glcBufferObjectsTests.hpp
 * \brief Conformance tests for general buffer objects functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <array>
#include <map>
#include <memory>

namespace glu
{
	class ShaderProgram;
}

namespace glcts
{

/** base class for below cases to handle common de/initialization */
class BufferObjectsTestBase : public deqp::TestCase
{
public:
	BufferObjectsTestBase(deqp::Context& context, const char * name, const char * desc);

	virtual void deinit() override;
	virtual void init() override;

	tcu::TestNode::IterateResult iterate() override;

	virtual tcu::TestNode::IterateResult run_test() = 0;

	bool build_buffers();
	bool release_buffers();

protected:
	/* Protected members */
	std::string m_shader_vert;
	std::string m_shader_frag;

	std::map<std::string, std::string> specializationMap;

	bool m_isContextES;
	bool m_isExtensionSupported;
	bool m_buildBuffers;

	std::unique_ptr<glu::ShaderProgram>	m_program;

	std::vector<glw::GLfloat> m_matProjection;
	std::array<glw::GLfloat, 12> m_triVertexArray;
	std::array<glw::GLfloat, 6>  m_triSubDataVertexArray;
	std::array<glw::GLfloat, 8>  m_pointVertices;
	std::array<glw::GLfloat, 12> m_elementVertices;

	glw::GLint m_window_size[2];

	std::vector<glw::GLuint> m_buffers;
	std::vector<glw::GLuint> m_textures;

	glw::GLuint m_vao;
};

/** class to handle cases related to generation buffer objects */
class BufferObjectsTestGenBuffersCase : public BufferObjectsTestBase
{
public:
	/* Public methods */
	BufferObjectsTestGenBuffersCase(deqp::Context& context);

	tcu::TestNode::IterateResult run_test() override;
};

/** class to handle cases related to rendering triangle with normals, texture, colors */
class BufferObjectsTestTrianglesCase : public BufferObjectsTestBase
{
public:
	/* Public methods */
	BufferObjectsTestTrianglesCase(deqp::Context& context);

	tcu::TestNode::IterateResult run_test() override;
};

/** class to test DrawElements on buffer objects */
class BufferObjectsTestElementsCase : public BufferObjectsTestBase
{
public:
	/* Public methods */
	BufferObjectsTestElementsCase(deqp::Context& context);

	tcu::TestNode::IterateResult run_test() override;
};

/** class to test multiple texture coordinate buffers functionality */
class BufferObjectsTestMultiTexturingCase : public BufferObjectsTestBase
{
public:
	/* Public methods */
	BufferObjectsTestMultiTexturingCase(deqp::Context& context);

	tcu::TestNode::IterateResult run_test() override;
};

/** class to test buffer objects with glBufferSubData functionality */
class BufferObjectsTestSubDataCase : public BufferObjectsTestBase
{
public:
	/* Public methods */
	BufferObjectsTestSubDataCase(deqp::Context& context);

	tcu::TestNode::IterateResult run_test() override;
};

/** Test group which encapsulates all conformance tests */
class BufferObjectsTests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	BufferObjectsTests(deqp::Context& context);

	void init();

private:
	BufferObjectsTests(const BufferObjectsTests& other);
	BufferObjectsTests& operator=(const BufferObjectsTests& other);
};

} /* glcts namespace */

#endif // _GLCBUFFEROBJECTSTESTS_HPP
