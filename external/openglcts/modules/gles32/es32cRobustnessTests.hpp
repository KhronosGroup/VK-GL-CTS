#ifndef _ES32CROBUSTNESSTESTS_HPP
#define _ES32CROBUSTNESSTESTS_HPP
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

#include "gluDefs.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "glcContext.hpp"
#include "glcRobustBufferAccessBehaviorTests.hpp"
#include "glcTestCase.hpp"
#include "glcTestPackage.hpp"

namespace es32cts
{
namespace RobustBufferAccessBehavior
{
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
}

#endif // _ES32CROBUSTNESSTESTS_HPP
