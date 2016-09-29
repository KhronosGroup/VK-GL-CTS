/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Descriptor pool tests
 *//*--------------------------------------------------------------------*/

#include "vktApiDescriptorPoolTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDeviceUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deInt32.h"

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

tcu::TestStatus resetDescriptorPoolTest (Context& context, deUint32 numIterations)
{
	const deUint32				numDescriptorSetsPerIter = 2048;
	const DeviceInterface&		vkd						 = context.getDeviceInterface();
	const VkDevice				device					 = context.getDevice();

	const VkDescriptorPoolSize descriptorPoolSize =
	{
		VK_DESCRIPTOR_TYPE_SAMPLER, // type
		numDescriptorSetsPerIter	// descriptorCount
	};

	// \todo [2016-05-24 collinbaker] Test with flag VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
	const VkDescriptorPoolCreateInfo descriptorPoolInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,	// sType
		NULL,											// pNext
		0,												// flags
		numDescriptorSetsPerIter,						// maxSets
		1,												// poolSizeCount
		&descriptorPoolSize								// pPoolSizes
	};

	{
		const Unique<VkDescriptorPool> descriptorPool(
			createDescriptorPool(vkd, device,
								 &descriptorPoolInfo));

		const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding =
		{
			0,							// binding
			VK_DESCRIPTOR_TYPE_SAMPLER, // descriptorType
			1,							// descriptorCount
			VK_SHADER_STAGE_ALL,		// stageFlags
			NULL						// pImmutableSamplers
		};

		const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// sType
			NULL,													// pNext
			0,														// flags
			1,														// bindingCount
			&descriptorSetLayoutBinding								// pBindings
		};

		{
			typedef de::SharedPtr<Unique<VkDescriptorSetLayout> > DescriptorSetLayoutPtr;

			vector<DescriptorSetLayoutPtr> descriptorSetLayouts;
			descriptorSetLayouts.reserve(numDescriptorSetsPerIter);

			for (deUint32 ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
			{
				descriptorSetLayouts.push_back(
					DescriptorSetLayoutPtr(
						new Unique<VkDescriptorSetLayout>(
							createDescriptorSetLayout(vkd, device,
													  &descriptorSetLayoutInfo))));
			}

			vector<VkDescriptorSetLayout> descriptorSetLayoutsRaw(numDescriptorSetsPerIter);

			for (deUint32 ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
			{
				descriptorSetLayoutsRaw[ndx] = **descriptorSetLayouts[ndx];
			}

			const VkDescriptorSetAllocateInfo descriptorSetInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
				NULL,											// pNext
				*descriptorPool,								// descriptorPool
				numDescriptorSetsPerIter,						// descriptorSetCount
				&descriptorSetLayoutsRaw[0]						// pSetLayouts
			};

			vector<VkDescriptorSet> testSets(numDescriptorSetsPerIter);

			for (deUint32 ndx = 0; ndx < numIterations; ++ndx)
			{
				// The test should crash in this loop at some point if there is a memory leak
				VK_CHECK(vkd.allocateDescriptorSets(device, &descriptorSetInfo, &testSets[0]));
				VK_CHECK(vkd.resetDescriptorPool(device, *descriptorPool, 0));
			}

		}
	}

	// If it didn't crash, pass
	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDescriptorPoolTests (tcu::TestContext& testCtx)
{
	const deUint32 numIterationsHigh = 4096;

	de::MovePtr<tcu::TestCaseGroup> descriptorPoolTests(
		new tcu::TestCaseGroup(testCtx, "descriptor_pool", "Descriptor Pool Tests"));

	addFunctionCase(descriptorPoolTests.get(),
					"repeated_reset_short",
					"Test 2 cycles of vkAllocateDescriptorSets and vkResetDescriptorPool (should pass)",
					resetDescriptorPoolTest, 2U);
	addFunctionCase(descriptorPoolTests.get(),
					"repeated_reset_long",
					"Test many cycles of vkAllocateDescriptorSets and vkResetDescriptorPool",
					resetDescriptorPoolTest, numIterationsHigh);

	return descriptorPoolTests.release();
}

} // api
} // vkt
