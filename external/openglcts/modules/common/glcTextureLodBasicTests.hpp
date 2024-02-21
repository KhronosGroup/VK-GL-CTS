#ifndef _GLCTEXTURELODBASICTESTS_HPP
#define _GLCTEXTURELODBASICTESTS_HPP
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
 * \file  glcTextureLodBasicTests.hpp
 * \brief Conformance tests for basic texture lod operations.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>

namespace glu
{
	class ShaderProgram;
}

namespace glcts
{

/** class to handle LOD selection test, former sgis_texture_lod_basic_lod_selection */
class TextureLodSelectionTestCase : public deqp::TestCase
{
public:
	/* Public methods */
	TextureLodSelectionTestCase(deqp::Context& context);

	void						 deinit();
	void						 init();
	tcu::TestNode::IterateResult iterate();

private:
	/* Private methods */
	void createLodTexture(const glw::GLenum target);
	void releaseTexture();
	void setBuffers(const glu::ShaderProgram& program);
	void releaseBuffers();

	void createSolidTexture(const glw::GLenum, const int, const int, const glw::GLubyte* const);
	bool drawAndVerify(const glw::GLint, const float, const int, const int, const int, const float, const float,
					   const float, const glw::GLenum, const glw::GLenum, const bool);
	bool doComparison(const int size, const glw::GLubyte* const expectedcolor);

	/* Private members */
	static const glw::GLchar* m_shader_basic_vert;
	static const glw::GLchar* m_shader_basic_1d_frag;
	static const glw::GLchar* m_shader_basic_2d_frag;

	std::map<std::string, std::string> specializationMap;

	glw::GLuint m_texture;
	glw::GLuint m_vao;
	glw::GLuint m_vbo;

	bool m_isContextES;
};

/** Test group which encapsulates all conformance tests */
class TextureLodBasicTests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	TextureLodBasicTests(deqp::Context& context);

	void init();

private:
	TextureLodBasicTests(const TextureLodBasicTests& other);
	TextureLodBasicTests& operator=(const TextureLodBasicTests& other);
};

} /* glcts namespace */

#endif // _GLCTEXTURELODBASICTESTS_HPP
