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
 * \file  esextcDisjointTimerQueryTests.cpp
 * \brief Base test group for disjoint timer query tests
 */ /*-------------------------------------------------------------------*/

#include "esextcDisjointTimerQueryTests.hpp"
#include "esextcDisjointTimerQueryHandleReuse.hpp"

namespace glcts
{

/** Constructor
 *
 * @param context       Test context
 * @param glslVersion   GLSL version
 **/
DisjointTimerQueryTests::DisjointTimerQueryTests (glcts::Context& context, const ExtParameters& extParams)
	: TestCaseGroupBase(context, extParams, "disjoint_timer_query", "Disjoint timer query tests")
{
	/* No implementation needed */
}

/** Initializes test cases for texture buffer tests
 **/
void DisjointTimerQueryTests::init (void)
{
	/* Initialize base class */
	TestCaseGroupBase::init();


	addChild(new DisjointTimerQueryHandleReuse(m_context, m_extParams,
											   "handle_reuse", "handle reuse"));
}

} // namespace glcts
