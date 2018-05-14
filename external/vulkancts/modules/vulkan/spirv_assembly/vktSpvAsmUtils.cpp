/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Utilities for Vulkan SPIR-V assembly tests
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmUtils.hpp"

#include "deMemory.h"
#include "deSTLUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;

bool is8BitStorageFeaturesSupported (const Context& context, Extension8BitStorageFeatures toCheck)
{
	VkPhysicalDevice8BitStorageFeaturesKHR extensionFeatures = context.get8BitStorageFeatures();

	if ((toCheck & EXT8BITSTORAGEFEATURES_STORAGE_BUFFER) != 0 && extensionFeatures.storageBuffer8BitAccess == VK_FALSE)
		TCU_FAIL("storageBuffer8BitAccess has to be supported");

	if ((toCheck & EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER) != 0 && extensionFeatures.uniformAndStorageBuffer8BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT8BITSTORAGEFEATURES_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant8 == VK_FALSE)
		return false;

	return true;
}

#define IS_CORE_FEATURE_AVAILABLE(CHECKED, AVAILABLE, FEATURE)	\
	if ((CHECKED.FEATURE != DE_FALSE) && (AVAILABLE.FEATURE == DE_FALSE)) { *missingFeature = #FEATURE; return false; }

bool isCoreFeaturesSupported (const Context&						context,
							  const vk::VkPhysicalDeviceFeatures&	toCheck,
							  const char**							missingFeature)
{
	const VkPhysicalDeviceFeatures&	availableFeatures	= context.getDeviceFeatures();

	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, robustBufferAccess);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fullDrawIndexUint32);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, imageCubeArray);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, independentBlend);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, geometryShader);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, tessellationShader);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sampleRateShading);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, dualSrcBlend);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, logicOp);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, multiDrawIndirect);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, drawIndirectFirstInstance);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthClamp);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthBiasClamp);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fillModeNonSolid);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthBounds);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, wideLines);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, largePoints);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, alphaToOne);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, multiViewport);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, samplerAnisotropy);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionETC2);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionASTC_LDR);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionBC);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, occlusionQueryPrecise);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, pipelineStatisticsQuery);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, vertexPipelineStoresAndAtomics);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fragmentStoresAndAtomics);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderTessellationAndGeometryPointSize);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderImageGatherExtended);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageExtendedFormats);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageMultisample);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageReadWithoutFormat);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageWriteWithoutFormat);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderUniformBufferArrayDynamicIndexing);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderSampledImageArrayDynamicIndexing);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageBufferArrayDynamicIndexing);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageArrayDynamicIndexing);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderClipDistance);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderCullDistance);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderFloat64);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderInt64);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderInt16);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderResourceResidency);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderResourceMinLod);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseBinding);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyBuffer);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyImage2D);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyImage3D);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency2Samples);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency4Samples);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency8Samples);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency16Samples);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyAliased);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, variableMultisampleRate);
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, inheritedQueries);

	return true;
}

bool is16BitStorageFeaturesSupported (const Context& context, Extension16BitStorageFeatures toCheck)
{
	const VkPhysicalDevice16BitStorageFeatures& extensionFeatures = context.get16BitStorageFeatures();

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK) != 0 && extensionFeatures.storageBuffer16BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM) != 0 && extensionFeatures.uniformAndStorageBuffer16BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant16 == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_INPUT_OUTPUT) != 0 && extensionFeatures.storageInputOutput16 == VK_FALSE)
		return false;

	return true;
}

bool isVariablePointersFeaturesSupported (const Context& context, ExtensionVariablePointersFeatures toCheck)
{
	const VkPhysicalDeviceVariablePointerFeatures& extensionFeatures = context.getVariablePointerFeatures();

	if ((toCheck & EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER) != 0 && extensionFeatures.variablePointersStorageBuffer == VK_FALSE)
		return false;

	if ((toCheck & EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS) != 0 && extensionFeatures.variablePointers == VK_FALSE)
		return false;

	return true;
}

bool isFloat16Int8FeaturesSupported (const Context& context, ExtensionFloat16Int8Features toCheck)
{
	const VkPhysicalDeviceFloat16Int8FeaturesKHR& extensionFeatures = context.getFloat16Int8Features();

	if ((toCheck & EXTFLOAT16INT8FEATURES_FLOAT16) != 0 && extensionFeatures.shaderFloat16 == VK_FALSE)
		return false;

	if ((toCheck & EXTFLOAT16INT8FEATURES_INT8) != 0 && extensionFeatures.shaderInt8 == VK_FALSE)
		return false;

	return true;
}

deUint32 getMinRequiredVulkanVersion (const SpirvVersion version)
{
	switch(version)
	{
	case SPIRV_VERSION_1_0:
		return VK_API_VERSION_1_0;
	case SPIRV_VERSION_1_1:
	case SPIRV_VERSION_1_2:
	case SPIRV_VERSION_1_3:
		return VK_API_VERSION_1_1;
	default:
		DE_ASSERT(0);
	}
	return 0u;
}

std::string	getVulkanName (const deUint32 version)
{
	return std::string(version == VK_API_VERSION_1_1 ? "1.1" : "1.0");
}

} // SpirVAssembly
} // vkt
