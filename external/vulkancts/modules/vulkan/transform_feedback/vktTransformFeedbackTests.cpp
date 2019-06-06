/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Transform Feedback Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackTests.hpp"
#include "vktTransformFeedbackSimpleTests.hpp"
#include "vktTransformFeedbackFuzzLayoutTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"


namespace vkt
{
namespace TransformFeedback
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> transformFeedbackGroup (new tcu::TestCaseGroup(testCtx, "transform_feedback", "Transform Feedback tests"));

	transformFeedbackGroup->addChild(createTransformFeedbackSimpleTests(testCtx));
	transformFeedbackGroup->addChild(createTransformFeedbackFuzzLayoutTests(testCtx));

	return transformFeedbackGroup.release();
}

} // TransformFeedback
} // vkt
