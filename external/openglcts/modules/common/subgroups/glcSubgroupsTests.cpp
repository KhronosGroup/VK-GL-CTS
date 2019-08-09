/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "glcSubgroupsTests.hpp"
#include "glcSubgroupsBuiltinVarTests.hpp"
#include "glcSubgroupsBuiltinMaskVarTests.hpp"
#include "glcSubgroupsBasicTests.hpp"
#include "glcSubgroupsVoteTests.hpp"
#include "glcSubgroupsBallotTests.hpp"
#include "glcSubgroupsBallotBroadcastTests.hpp"
#include "glcSubgroupsBallotOtherTests.hpp"
#include "glcSubgroupsArithmeticTests.hpp"
#include "glcSubgroupsClusteredTests.hpp"
#include "glcSubgroupsPartitionedTests.hpp"
#include "glcSubgroupsShuffleTests.hpp"
#include "glcSubgroupsQuadTests.hpp"
#include "glcSubgroupsShapeTests.hpp"
//#include "glcTestGroupUtil.hpp"

namespace glc
{
namespace subgroups
{

/** Constructor.
 *
 *  @param context Rendering context.
 */
GlSubgroupTests::GlSubgroupTests(deqp::Context& context)
	: TestCaseGroup(context, "subgroups", "Shader Subgroup Operation tests")
{
}

/** Initializes the test group contents. */
void GlSubgroupTests::init()
{
	addChild(createSubgroupsBuiltinVarTests(m_context));
	addChild(createSubgroupsBuiltinMaskVarTests(m_context));
	addChild(createSubgroupsBasicTests(m_context));
	addChild(createSubgroupsVoteTests(m_context));
	addChild(createSubgroupsBallotTests(m_context));
	addChild(createSubgroupsBallotBroadcastTests(m_context));
	addChild(createSubgroupsBallotOtherTests(m_context));
	addChild(createSubgroupsArithmeticTests(m_context));
	addChild(createSubgroupsClusteredTests(m_context));
	addChild(createSubgroupsPartitionedTests(m_context));
	addChild(createSubgroupsShuffleTests(m_context));
	addChild(createSubgroupsQuadTests(m_context));
	addChild(createSubgroupsShapeTests(m_context));
}

} // subgroups
} // glc
