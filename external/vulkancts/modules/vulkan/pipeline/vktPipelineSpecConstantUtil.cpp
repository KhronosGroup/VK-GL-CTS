/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Pipeline specialization constants test utilities
 *//*--------------------------------------------------------------------*/

#include "vktPipelineSpecConstantUtil.hpp"
#include "vkTypeUtil.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{
using namespace vk;

VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,							// VkImageType              imageType;
		format,										// VkFormat                 format;
		makeExtent3D(size.x(), size.y(), 1),		// VkExtent3D               extent;
		1u,											// uint32_t                 mipLevels;
		1u,											// uint32_t                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits    samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling            tiling;
		usage,										// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
		0u,											// uint32_t                 queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
	};
	return imageInfo;
}

void requireFeatures (Context& context, const FeatureFlags flags)
{
	if (flags & FEATURE_TESSELLATION_SHADER)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (flags & FEATURE_GEOMETRY_SHADER)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (flags & FEATURE_SHADER_FLOAT_64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);

	if (flags & FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

	if (flags & FEATURE_FRAGMENT_STORES_AND_ATOMICS)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

	if (flags & FEATURE_SHADER_INT_64)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

	if (flags & FEATURE_SHADER_INT_16)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

	if (flags & (FEATURE_SHADER_FLOAT_16 | FEATURE_SHADER_INT_8))
	{
		const auto extraFeatures = context.getShaderFloat16Int8Features();

		if ((flags & FEATURE_SHADER_INT_8) != 0u && !extraFeatures.shaderInt8)
			TCU_THROW(NotSupportedError, "8-bit integers not supported in shaders");

		if ((flags & FEATURE_SHADER_FLOAT_16) != 0u && !extraFeatures.shaderFloat16)
			TCU_THROW(NotSupportedError, "16-bit floats not supported in shaders");
	}

	// Check needed storage features.
	if (flags & (FEATURE_SHADER_INT_16 | FEATURE_SHADER_FLOAT_16))
	{
		const auto features = context.get16BitStorageFeatures();
		if (!features.storageBuffer16BitAccess)
			TCU_THROW(NotSupportedError, "16-bit access in storage buffers not supported");
	}

	if (flags & FEATURE_SHADER_INT_8)
	{
		const auto features = context.get8BitStorageFeatures();
		if (!features.storageBuffer8BitAccess)
			TCU_THROW(NotSupportedError, "8-bit access in storage buffers not supported");
	}
}

} // pipeline
} // vkt
