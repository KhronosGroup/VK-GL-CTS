#ifndef _VKTCONDITIONALIGNORETESTS_HPP
#define _VKTCONDITIONALIGNORETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief Test for conditional rendering with commands that ignore conditions
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace conditional
{

class ConditionalIgnoreTests : public tcu::TestCaseGroup
{
public:
						ConditionalIgnoreTests	(tcu::TestContext &testCtx);
						~ConditionalIgnoreTests(void);

	void init (void);

private:
	ConditionalIgnoreTests					(const ConditionalIgnoreTests &other);
	ConditionalIgnoreTests&	operator=		(const ConditionalIgnoreTests &other);

};

} // conditional
} // vkt

#endif // _VKTCONDITIONALIGNORETESTS_HPP
