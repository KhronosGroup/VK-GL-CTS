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
#include "vktPrimitivesGeneratedQueryTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"


namespace vkt
{
namespace TransformFeedback
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> transformFeedbackGroup (new tcu::TestCaseGroup(testCtx, name.c_str(), "Transform Feedback tests"));

	{
		// For simple tests, we're going to run them with different GPL construction types.
		const vk::PipelineConstructionType constructionTypes[]
		{
			vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC,
			vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY,
			vk::PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY,
		};
		for (const auto& constructionType : constructionTypes)
			transformFeedbackGroup->addChild(createTransformFeedbackSimpleTests(testCtx, constructionType));
	}

	transformFeedbackGroup->addChild(createTransformFeedbackFuzzLayoutTests(testCtx));
	transformFeedbackGroup->addChild(createPrimitivesGeneratedQueryTests(testCtx));

	return transformFeedbackGroup.release();
}

} // TransformFeedback
} // vkt
