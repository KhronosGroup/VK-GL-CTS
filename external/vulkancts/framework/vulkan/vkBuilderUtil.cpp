/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan object builder utilities.
 *//*--------------------------------------------------------------------*/

#include "vkBuilderUtil.hpp"

#include "vkRefUtil.hpp"

namespace vk
{

// DescriptorSetLayoutBuilder

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder (void)
{
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addBinding (VkDescriptorType	descriptorType,
																	deUint32			arraySize,
																	VkShaderStageFlags	stageFlags,
																	const VkSampler*	pImmutableSamplers)
{
	const VkDescriptorSetLayoutBinding binding =
	{
		descriptorType,			//!< descriptorType
		arraySize,				//!< arraySize
		stageFlags,				//!< stageFlags
		pImmutableSamplers,		//!< pImmutableSamplers
	};
	m_bindings.push_back(binding);
	return *this;
}

Move<VkDescriptorSetLayout> DescriptorSetLayoutBuilder::build (const DeviceInterface& vk, VkDevice device) const
{
	const VkDescriptorSetLayoutBinding* const	bindingPtr	= (m_bindings.empty()) ? (DE_NULL) : (&m_bindings[0]);
	const VkDescriptorSetLayoutCreateInfo		createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(deUint32)m_bindings.size(),		//!< count
		bindingPtr,							//!< pBinding
	};

	return createDescriptorSetLayout(vk, device, &createInfo);
}

// DescriptorPoolBuilder

DescriptorPoolBuilder::DescriptorPoolBuilder (void)
{
}

DescriptorPoolBuilder& DescriptorPoolBuilder::addType (VkDescriptorType type, deUint32 numDescriptors)
{
	if (numDescriptors == 0u)
	{
		// nothing to do
		return *this;
	}
	else
	{
		for (size_t ndx = 0; ndx < m_counts.size(); ++ndx)
		{
			if (m_counts[ndx].type == type)
			{
				// augment existing requirement
				m_counts[ndx].count += numDescriptors;
				return *this;
			}
		}

		{
			// new requirement
			const VkDescriptorTypeCount typeCount =
			{
				type,			//!< type
				numDescriptors,	//!< count
			};

			m_counts.push_back(typeCount);
			return *this;
		}
	}
}

Move<VkDescriptorPool> DescriptorPoolBuilder::build (const DeviceInterface& vk, VkDevice device, VkDescriptorPoolUsage poolUsage, deUint32 maxSets) const
{
	const VkDescriptorTypeCount* const	typeCountPtr	= (m_counts.empty()) ? (DE_NULL) : (&m_counts[0]);
	const VkDescriptorPoolCreateInfo	createInfo		=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(deUint32)m_counts.size(),		//!< count
		typeCountPtr,					//!< pTypeCount
	};

	return createDescriptorPool(vk, device, poolUsage, maxSets, &createInfo);
}

// DescriptorSetUpdateBuilder

DescriptorSetUpdateBuilder::DescriptorSetUpdateBuilder (void)
{
}

DescriptorSetUpdateBuilder& DescriptorSetUpdateBuilder::write (VkDescriptorSet			destSet,
															   deUint32					destBinding,
															   deUint32					destArrayElement,
															   deUint32					count,
															   VkDescriptorType			descriptorType,
															   const VkDescriptorInfo*	pDescriptors)
{
	const VkWriteDescriptorSet writeParams =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		DE_NULL,
		destSet,			//!< destSet
		destBinding,		//!< destBinding
		destArrayElement,	//!< destArrayElement
		count,				//!< count
		descriptorType,		//!< descriptorType
		pDescriptors,		//!< pDescriptors
	};
	m_writes.push_back(writeParams);
	return *this;
}

DescriptorSetUpdateBuilder& DescriptorSetUpdateBuilder::copy (VkDescriptorSet	srcSet,
															  deUint32			srcBinding,
															  deUint32			srcArrayElement,
															  VkDescriptorSet	destSet,
															  deUint32			destBinding,
															  deUint32			destArrayElement,
															  deUint32			count)
{
	const VkCopyDescriptorSet copyParams =
	{
		VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
		DE_NULL,
		srcSet,				//!< srcSet
		srcBinding,			//!< srcBinding
		srcArrayElement,	//!< srcArrayElement
		destSet,			//!< destSet
		destBinding,		//!< destBinding
		destArrayElement,	//!< destArrayElement
		count,				//!< count
	};
	m_copies.push_back(copyParams);
	return *this;
}

void DescriptorSetUpdateBuilder::update (const DeviceInterface& vk, VkDevice device) const
{
	const VkWriteDescriptorSet* const	writePtr	= (m_writes.empty()) ? (DE_NULL) : (&m_writes[0]);
	const VkCopyDescriptorSet* const	copyPtr		= (m_copies.empty()) ? (DE_NULL) : (&m_copies[0]);

	VK_CHECK(vk.updateDescriptorSets(device, (deUint32)m_writes.size(), writePtr, (deUint32)m_copies.size(), copyPtr));
}

} // vk
