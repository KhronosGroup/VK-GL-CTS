/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Default vertex array tests
 *//*--------------------------------------------------------------------*/

#include "es31fDefaultVertexArrayObjectTests.hpp"

#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"

#include "glwEnums.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

class VertexAttributeDivisorCase : public TestCase
{
public:
					VertexAttributeDivisorCase	(Context& context, const char* name, const char* description);
	IterateResult	iterate						(void);
};

VertexAttributeDivisorCase::VertexAttributeDivisorCase (Context& context, const char* name, const char* description)
	: TestCase(context, name, description)
{
}

VertexAttributeDivisorCase::IterateResult VertexAttributeDivisorCase::iterate (void)
{
	glu::CallLogWrapper gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	deUint32			error;

	m_testCtx.getLog()	<< tcu::TestLog::Message
						<< "Using VertexAttribDivisor with default VAO.\n"
						<< "Expecting INVALID_OPERATION."
						<< tcu::TestLog::EndMessage;

	gl.enableLogging(true);
	gl.glBindVertexArray(0);

	// Using vertexAttribDivisor with default vao is an error in ES 3.1, but not
	// in ES 3.0. See Khronos bug 13564 for details.
	gl.glVertexAttribDivisor(0, 3);
	error = gl.glGetError();

	if (error == GL_INVALID_OPERATION)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect error value");

	return STOP;
}

} // anonymous

DefaultVertexArrayObjectTests::DefaultVertexArrayObjectTests (Context& context)
	: TestCaseGroup(context, "default_vertex_array_object", "Default vertex array object")
{
}

void DefaultVertexArrayObjectTests::init (void)
{
	addChild(new VertexAttributeDivisorCase(m_context, "vertex_attrib_divisor", "Use VertexAttribDivisor with default VAO"));
}

} // Functional
} // gles31
} // deqp
