/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google LLC
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

/*!
 * \file  esextDisjointTimerQueryHandleReuse.cpp
 * \brief Timer query handle reuse tests
 */ /*-------------------------------------------------------------------*/

#include "esextcDisjointTimerQueryHandleReuse.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include <vector>

namespace glcts
{

/** Constructor
 *
 *  @param context     Test context
 *  @param name        Test case's name
 *  @param description Test case's description
 **/
DisjointTimerQueryHandleReuse::DisjointTimerQueryHandleReuse (Context& context, const ExtParameters& extParams,
														      const char* name, const char* description)
	: DisjointTimerQueryBase(context, extParams, name, description)
{
}

/** Initializes GLES objects used during the test */
void DisjointTimerQueryHandleReuse::initTest (void)
{
	if (!isExtensionSupported("GL_EXT_disjoint_timer_query"))
	{
		throw tcu::NotSupportedError(DISJOINT_TIMER_QUERY_NOT_SUPPORTED);
	}
}

/** Executes the test.
 *
 *  Sets the test result to QP_TEST_RESULT_FAIL if the test failed, QP_TEST_RESULT_PASS otherwise.
 *
 *  Note the function throws exception should an error occur!
 *
 *  @return STOP if the test has finished, CONTINUE to indicate iterate should be called once again.
 **/
tcu::TestNode::IterateResult DisjointTimerQueryHandleReuse::iterate(void)
{
	/* Initialize */
	initTest();

	/* Get Gl entry points */
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();
		/* Running tests. */
	bool is_ok = true;

	glw::GLuint query_id_a = 0;
	glw::GLuint query_id_b = 0;
	/* Allocate query object */
	glGenQueriesEXT(1, &query_id_a);
	/* Associate object with GL_TIMESTAMP */
	glQueryCounterEXT(query_id_a, GL_TIMESTAMP);
	/* Deallocate query object */
	glDeleteQueriesEXT(1, &query_id_a);

	/* Allocate query object again - should result in the same id */
	glGenQueriesEXT(1, &query_id_b);
	/* Use the id with something else */
	glBeginQueryEXT(GL_TIME_ELAPSED, query_id_b);
	if (gl.getError() != 0) /* Crash was reported here. */
		is_ok = false;
	glEndQueryEXT(GL_TIME_ELAPSED);
	/* Clean up */
	glDeleteQueriesEXT(1, &query_id_b);

	if (query_id_a != query_id_b)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Note: Queries got different id:s, so no actual reuse occurred."
			<< tcu::TestLog::EndMessage;
	}

	/* Result's setup. */
	if (is_ok)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}
	else
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}
	return STOP;
}

} // namespace glcts
