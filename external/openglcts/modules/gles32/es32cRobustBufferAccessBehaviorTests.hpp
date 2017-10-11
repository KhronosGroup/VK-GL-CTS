#ifndef _ES32CROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
#define _ES32CROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
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
 * \file  es32cRobustBufferAccessBehaviorTests.hpp
 * \brief Declares test classes for "Robust Buffer Access Behavior" functionality.
 */ /*-------------------------------------------------------------------*/

#include "glcRobustBufferAccessBehaviorTests.hpp"
#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

namespace es32cts
{
namespace RobustBufferAccessBehavior
{
/** Implementation of test VertexBufferObjects. Description follows:
 *
 * This test verifies that any "out-of-bound" read from vertex buffer result in
 * abnormal program exit.
 **/
class VertexBufferObjectsTest : public deqp::RobustBufferAccessBehavior::VertexBufferObjectsTest
{
public:
	/* Public methods */
	VertexBufferObjectsTest(deqp::Context& context);
	virtual ~VertexBufferObjectsTest()
	{
	}

protected:
	/* Protected methods */
	std::string getFragmentShader();
	std::string getVertexShader();
	bool verifyInvalidResults(glw::GLuint texture_id);
	bool verifyResults(glw::GLuint texture_id);
};

/** Implementation of test TexelFetch. Description follows:
 *
 * This test verifies that any "out-of-bound" fetch from texture result in
 * abnormal program exit.
 **/
class TexelFetchTest : public deqp::RobustBufferAccessBehavior::TexelFetchTest
{
public:
	/* Public methods */
	TexelFetchTest(deqp::Context& context);
	TexelFetchTest(deqp::Context& context, const glw::GLchar* name, const glw::GLchar* description);
	virtual ~TexelFetchTest()
	{
	}

protected:
	/* Protected methods */
	void prepareTexture(bool is_source, glw::GLuint texture_id);

protected:
	/* Protected methods */
	std::string getGeometryShader();
	std::string getVertexShader();
	bool verifyInvalidResults(glw::GLuint texture_id);
	bool verifyValidResults(glw::GLuint texture_id);
};

/** Implementation of test ImageLoadStore. Description follows:
 *
 * This test verifies that any "out-of-bound" access to image result in abnormal program exit.
 **/
class ImageLoadStoreTest : public TexelFetchTest
{
public:
	/* Public methods */
	ImageLoadStoreTest(deqp::Context& context);
	virtual ~ImageLoadStoreTest()
	{
	}

	/* Public methods inherited from TestCase */
	virtual tcu::TestNode::IterateResult iterate(void);

protected:
	/* Protected methods */
	std::string getComputeShader(VERSION version, glw::GLuint coord_offset = 0);
	void setTextures(glw::GLuint id_destination, glw::GLuint id_source);
	bool verifyInvalidResults(glw::GLuint texture_id);
	bool verifyValidResults(glw::GLuint texture_id);
};

/** Implementation of test StorageBuffer. Description follows:
 *
 * This test verifies that any "out-of-bound" access to buffer result in abnormal program exit.
 **/
class StorageBufferTest : public deqp::RobustBufferAccessBehavior::StorageBufferTest
{
public:
	/* Public methods */
	StorageBufferTest(deqp::Context& context);
	virtual ~StorageBufferTest()
	{
	}

protected:
	/* Protected methods */
	std::string getComputeShader(glw::GLuint offset);
	bool verifyResults(glw::GLfloat* buffer_data);
};

/** Implementation of test UniformBuffer. Description follows:
 *
 * This test verifies that any "out-of-bound" read from uniform buffer result
 * in abnormal program exit.
 **/
class UniformBufferTest : public deqp::RobustBufferAccessBehavior::UniformBufferTest
{
public:
	/* Public methods */
	UniformBufferTest(deqp::Context& context);
	virtual ~UniformBufferTest()
	{
	}

protected:
	/* Protected methods */
	std::string getComputeShader(glw::GLuint offset);
};

} /* RobustBufferAccessBehavior */

/** Group class for multi bind conformance tests */
class RobustBufferAccessBehaviorTests : public deqp::RobustBufferAccessBehaviorTests
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

} /* es32cts */

#endif // _ES32CROBUSTBUFFERACCESSBEHAVIORTESTS_HPP
