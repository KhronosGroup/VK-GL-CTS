#ifndef _GLCSUBGROUPSTESTS_HPP
#define _GLCSUBGROUPSTESTS_HPP
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

#include "tcuDefs.hpp"
#include "glcTestCase.hpp"

namespace glc
{
namespace subgroups
{

/** Test group which encapsulates all subgroup conformance tests */
class GlSubgroupTests : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	GlSubgroupTests(deqp::Context& context);

	void init();

private:
	GlSubgroupTests(const GlSubgroupTests& other);
	GlSubgroupTests& operator=(const GlSubgroupTests& other);
};

} // subgroups
} // glc

#endif // _GLCSUBGROUPSTESTS_HPP
