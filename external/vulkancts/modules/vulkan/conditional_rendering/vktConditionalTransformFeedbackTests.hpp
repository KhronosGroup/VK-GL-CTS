#ifndef _VKTCONDITIONALTRANSFORMFEEDBACKTESTS_HPP
#define _VKTCONDITIONALTRANSFORMFEEDBACKTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Google LLC
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
 * \brief Test for conditional rendering of vkCmdDraw* functions
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace conditional
{

class ConditionalTransformFeedbackTests : public tcu::TestCaseGroup
{
public:
						ConditionalTransformFeedbackTests	(tcu::TestContext &testCtx);
						~ConditionalTransformFeedbackTests	(void);
	void				init								(void);

private:
	ConditionalTransformFeedbackTests						(const ConditionalTransformFeedbackTests &other);
};

} // conditional
} // vkt

#endif // _VKTCONDITIONALTRANSFORMFEEDBACKTESTS_HPP
