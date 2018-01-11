/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Tests for descriptor updates.
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorUpdateTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace BindingModel
{
namespace
{

// Test matches VkPositiveLayerTest.EmptyDescriptorUpdateTest
tcu::TestStatus EmptyDescriptorUpdateCase (Context& context)
{
	const vk::DeviceInterface&				vki					= context.getDeviceInterface();
	const vk::VkDevice						device				= context.getDevice();
	vk::Allocator&							allocator			= context.getDefaultAllocator();

	// Create layout with two uniform buffer descriptors w/ empty binding between them
	vk::DescriptorSetLayoutBuilder			builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL);
	builder.addBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, (vk::VkShaderStageFlags)0, DE_NULL);
	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL);

	vk::Unique<vk::VkDescriptorSetLayout>	layout				(builder.build(vki, device, (vk::VkDescriptorSetLayoutCreateFlags)0));

	// Create descriptor pool
	vk::Unique<vk::VkDescriptorPool>		descriptorPool		(vk::DescriptorPoolBuilder().addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2).build(vki, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));

	// Create descriptor set
	const vk::VkDescriptorSetAllocateInfo	setAllocateInfo		=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		*descriptorPool,									// VkDescriptorPool				descriptorPool
		1,													// deUint32						descriptorSetCount
		&layout.get()										// const VkDescriptorSetLayout*	pSetLayouts
	};

	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(allocateDescriptorSet(vki, device, &setAllocateInfo));

	// Create a buffer to be used for update
	const vk::VkBufferCreateInfo			bufferCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,									// const void*			pNext
		(vk::VkBufferCreateFlags)DE_NULL,			// VkBufferCreateFlags	flags
		256,										// VkDeviceSize			size
		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,		// VkBufferUsageFlags	usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0,											// deUint32				queueFamilyIndexCount
		DE_NULL										// const deUint32*		pQueueFamilyIndices
	};

	vk::Unique<vk::VkBuffer>				buffer				(createBuffer(vki, device, &bufferCreateInfo));
	const vk::VkMemoryRequirements			requirements		= vk::getBufferMemoryRequirements(vki, device, *buffer);
	de::MovePtr<vk::Allocation>				allocation			= allocator.allocate(requirements, vk::MemoryRequirement::Any);

	VK_CHECK(vki.bindBufferMemory(device, *buffer, allocation->getMemory(), allocation->getOffset()));

	// Only update the descriptor at binding 2
	const vk::VkDescriptorBufferInfo		descriptorInfo		=
	{
		*buffer,		// VkBuffer		buffer
		0,				// VkDeviceSize	offset
		VK_WHOLE_SIZE	// VkDeviceSize	range
	};

	const vk::VkWriteDescriptorSet			descriptorWrite		=
	{
		vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureTypes					Type
		DE_NULL,									// const void*						pNext
		*descriptorSet,								// VkDescriptorSet					dstSet
		2,											// deUint32							dstBinding
		0,											// deUint32							dstArrayElement
		1,											// deUint32							descriptorCount
		vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// VkDescriptorType					descriptorType
		DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo
		&descriptorInfo,							// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL										// const VkBufferView*				pTexelBufferView
	};

	vki.updateDescriptorSets(device, 1, &descriptorWrite, 0, DE_NULL);

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}


tcu::TestCaseGroup* createEmptyDescriptorUpdateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "empty_descriptor", "Update last descriptor in a set that includes an empty binding"));

	addFunctionCase(group.get(), "uniform_buffer", "", EmptyDescriptorUpdateCase);

	return group.release();
}

} // anonymous


tcu::TestCaseGroup* createDescriptorUpdateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "descriptor_update", "Update descriptor sets"));

	group->addChild(createEmptyDescriptorUpdateTests(testCtx));

	return group.release();
}

} // BindingModel
} // vkt
