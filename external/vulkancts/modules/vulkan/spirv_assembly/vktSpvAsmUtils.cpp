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
#include "vkPlatform.hpp"

#include <limits>

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;

std::string VariableLocation::toString() const
{
	return "set_" + de::toString(set) + "_binding_" + de::toString(binding);
}

std::string VariableLocation::toDescription() const
{
	return "Set " + de::toString(set) + " and Binding " + de::toString(binding);
}

#define IS_CORE_FEATURE_AVAILABLE(CHECKED, AVAILABLE, FEATURE)	\
	if ((CHECKED.FEATURE != DE_FALSE) && (AVAILABLE.FEATURE == DE_FALSE)) { *missingFeature = #FEATURE; return false; }

bool isCoreFeaturesSupported (const Context&						context,
							  const vk::VkPhysicalDeviceFeatures&	toCheck,
							  const char**							missingFeature)
{
	const VkPhysicalDeviceFeatures&	availableFeatures	= context.getDeviceFeatures();

	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, robustBufferAccess)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fullDrawIndexUint32)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, imageCubeArray)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, independentBlend)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, geometryShader)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, tessellationShader)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sampleRateShading)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, dualSrcBlend)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, logicOp)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, multiDrawIndirect)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, drawIndirectFirstInstance)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthClamp)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthBiasClamp)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fillModeNonSolid)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, depthBounds)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, wideLines)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, largePoints)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, alphaToOne)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, multiViewport)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, samplerAnisotropy)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionETC2)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionASTC_LDR)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, textureCompressionBC)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, occlusionQueryPrecise)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, pipelineStatisticsQuery)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, vertexPipelineStoresAndAtomics)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, fragmentStoresAndAtomics)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderTessellationAndGeometryPointSize)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderImageGatherExtended)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageExtendedFormats)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageMultisample)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageReadWithoutFormat)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageWriteWithoutFormat)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderUniformBufferArrayDynamicIndexing)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderSampledImageArrayDynamicIndexing)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageBufferArrayDynamicIndexing)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderStorageImageArrayDynamicIndexing)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderClipDistance)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderCullDistance)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderFloat64)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderInt64)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderInt16)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderResourceResidency)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, shaderResourceMinLod)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseBinding)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyBuffer)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyImage2D)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyImage3D)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency2Samples)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency4Samples)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency8Samples)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidency16Samples)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, sparseResidencyAliased)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, variableMultisampleRate)
	IS_CORE_FEATURE_AVAILABLE(toCheck, availableFeatures, inheritedQueries)

	return true;
}

#define IS_AVAIL(EXT_NAME, FEATURE)	\
	if (toCheck.FEATURE && !extensionFeatures.FEATURE) { *missingFeature = EXT_NAME #FEATURE; return false; }

bool isFloat16Int8FeaturesSupported(const Context& context, const vk::VkPhysicalDeviceShaderFloat16Int8Features& toCheck, const char **missingFeature)
{
	const VkPhysicalDeviceShaderFloat16Int8Features& extensionFeatures = context.getShaderFloat16Int8Features();

	IS_AVAIL("ShaderFloat16Int8.", shaderFloat16);
	IS_AVAIL("ShaderFloat16Int8.", shaderInt8);

	return true;
}

bool is8BitStorageFeaturesSupported(const Context& context, const vk::VkPhysicalDevice8BitStorageFeatures& toCheck, const char **missingFeature)
{
	const VkPhysicalDevice8BitStorageFeaturesKHR& extensionFeatures = context.get8BitStorageFeatures();

	IS_AVAIL("8BitStorage.", storageBuffer8BitAccess);
	IS_AVAIL("8BitStorage.", uniformAndStorageBuffer8BitAccess);
	IS_AVAIL("8BitStorage.", storagePushConstant8);

	return true;
}

bool is16BitStorageFeaturesSupported(const Context& context, const vk::VkPhysicalDevice16BitStorageFeatures& toCheck, const char **missingFeature)
{
	const VkPhysicalDevice16BitStorageFeatures& extensionFeatures = context.get16BitStorageFeatures();

	IS_AVAIL("16BitStorage.", storageBuffer16BitAccess);
	IS_AVAIL("16BitStorage.", uniformAndStorageBuffer16BitAccess);
	IS_AVAIL("16BitStorage.", storagePushConstant16);
	IS_AVAIL("16BitStorage.", storageInputOutput16);

	return true;
}

bool isVariablePointersFeaturesSupported(const Context& context, const vk::VkPhysicalDeviceVariablePointersFeatures& toCheck, const char **missingFeature)
{
	const VkPhysicalDeviceVariablePointersFeatures& extensionFeatures = context.getVariablePointersFeatures();

	IS_AVAIL("VariablePointers.", variablePointersStorageBuffer);
	IS_AVAIL("VariablePointers.", variablePointers);

	return true;
}

bool isVulkanMemoryModelFeaturesSupported(const Context& context, const vk::VkPhysicalDeviceVulkanMemoryModelFeatures& toCheck, const char **missingFeature)
{
	const VkPhysicalDeviceVulkanMemoryModelFeaturesKHR& extensionFeatures = context.getVulkanMemoryModelFeatures();

	IS_AVAIL("VulkanMemoryModel.", vulkanMemoryModel);
	IS_AVAIL("VulkanMemoryModel.", vulkanMemoryModelDeviceScope);
	IS_AVAIL("VulkanMemoryModel.", vulkanMemoryModelAvailabilityVisibilityChains);

	return true;
}

bool isIntegerDotProductFeaturesSupported(const Context& context, const vk::VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR& toCheck, const char **missingFeature)
{
	const VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR& extensionFeatures = context.getShaderIntegerDotProductFeatures();

	IS_AVAIL("ShaderIntegerDotProduct.", shaderIntegerDotProduct);

	return true;
}

#undef IS_AVAIL

bool isFloatControlsFeaturesSupported (const Context& context, const vk::VkPhysicalDeviceFloatControlsProperties& toCheck, const char **missingFeature)
{
	// if all flags are set to false then no float control features are actualy requested by the test
	if ((toCheck.shaderSignedZeroInfNanPreserveFloat16 ||
		 toCheck.shaderSignedZeroInfNanPreserveFloat32 ||
		 toCheck.shaderSignedZeroInfNanPreserveFloat64 ||
		 toCheck.shaderDenormPreserveFloat16 ||
		 toCheck.shaderDenormPreserveFloat32 ||
		 toCheck.shaderDenormPreserveFloat64 ||
		 toCheck.shaderDenormFlushToZeroFloat16 ||
		 toCheck.shaderDenormFlushToZeroFloat32 ||
		 toCheck.shaderDenormFlushToZeroFloat64 ||
		 toCheck.shaderRoundingModeRTEFloat16 ||
		 toCheck.shaderRoundingModeRTEFloat32 ||
		 toCheck.shaderRoundingModeRTEFloat64 ||
		 toCheck.shaderRoundingModeRTZFloat16 ||
		 toCheck.shaderRoundingModeRTZFloat32 ||
		 toCheck.shaderRoundingModeRTZFloat64) == false)
		return true;

	*missingFeature = "Float controls properties";

	// return false when float control features are requested and proper extension is not supported
	if (!context.isDeviceFunctionalitySupported("VK_KHR_shader_float_controls"))
		return false;

	// perform query to get supported float control properties
   vk::VkPhysicalDeviceFloatControlsProperties refControls;
	{
		refControls.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;
		refControls.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 deviceProperties;
		deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties.pNext = &refControls;

		const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
		const vk::InstanceInterface&	instanceInterface	= context.getInstanceInterface();

		instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
	}

	using FCIndependence = VkShaderFloatControlsIndependence;
	FCIndependence fcInd32		= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY_KHR;
	FCIndependence fcIndAll		= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR;
	FCIndependence fcIndNone	= VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR;

	bool requiredDenormBehaviorNotSupported =
		((toCheck.denormBehaviorIndependence == fcIndAll) && (refControls.denormBehaviorIndependence != fcIndAll)) ||
		((toCheck.denormBehaviorIndependence == fcInd32)  && (refControls.denormBehaviorIndependence == fcIndNone));

	bool requiredRoundingModeNotSupported =
		((toCheck.roundingModeIndependence == fcIndAll) && (refControls.roundingModeIndependence != fcIndAll)) ||
		((toCheck.roundingModeIndependence == fcInd32)  && (refControls.roundingModeIndependence == fcIndNone));

	// check if flags needed by the test are not supported by the device
	bool requiredFeaturesNotSupported =
		requiredDenormBehaviorNotSupported ||
		requiredRoundingModeNotSupported ||
		(toCheck.shaderDenormFlushToZeroFloat16			&& !refControls.shaderDenormFlushToZeroFloat16) ||
		(toCheck.shaderDenormPreserveFloat16			&& !refControls.shaderDenormPreserveFloat16) ||
		(toCheck.shaderRoundingModeRTEFloat16			&& !refControls.shaderRoundingModeRTEFloat16) ||
		(toCheck.shaderRoundingModeRTZFloat16			&& !refControls.shaderRoundingModeRTZFloat16) ||
		(toCheck.shaderSignedZeroInfNanPreserveFloat16	&& !refControls.shaderSignedZeroInfNanPreserveFloat16) ||
		(toCheck.shaderDenormFlushToZeroFloat32			&& !refControls.shaderDenormFlushToZeroFloat32) ||
		(toCheck.shaderDenormPreserveFloat32			&& !refControls.shaderDenormPreserveFloat32) ||
		(toCheck.shaderRoundingModeRTEFloat32			&& !refControls.shaderRoundingModeRTEFloat32) ||
		(toCheck.shaderRoundingModeRTZFloat32			&& !refControls.shaderRoundingModeRTZFloat32) ||
		(toCheck.shaderSignedZeroInfNanPreserveFloat32	&& !refControls.shaderSignedZeroInfNanPreserveFloat32) ||
		(toCheck.shaderDenormFlushToZeroFloat64			&& !refControls.shaderDenormFlushToZeroFloat64) ||
		(toCheck.shaderDenormPreserveFloat64			&& !refControls.shaderDenormPreserveFloat64) ||
		(toCheck.shaderRoundingModeRTEFloat64			&& !refControls.shaderRoundingModeRTEFloat64) ||
		(toCheck.shaderRoundingModeRTZFloat64			&& !refControls.shaderRoundingModeRTZFloat64) ||
		(toCheck.shaderSignedZeroInfNanPreserveFloat64	&& !refControls.shaderSignedZeroInfNanPreserveFloat64);

	// we checked if required features are not supported - we need to
	// negate the result to know if all required features are available
	return !requiredFeaturesNotSupported;
}

bool isVulkanFeaturesSupported(const Context& context, const VulkanFeatures& requested, const char **missingFeature)
{
	if (!isCoreFeaturesSupported(context, requested.coreFeatures, missingFeature))
		return false;

	if (!is8BitStorageFeaturesSupported(context, requested.ext8BitStorage, missingFeature))
		return false;

	if (!is16BitStorageFeaturesSupported(context, requested.ext16BitStorage, missingFeature))
		return false;

	if (!isVariablePointersFeaturesSupported(context, requested.extVariablePointers, missingFeature))
		return false;

	if (!isFloat16Int8FeaturesSupported(context, requested.extFloat16Int8, missingFeature))
		return false;

	if (!isVulkanMemoryModelFeaturesSupported(context, requested.extVulkanMemoryModel, missingFeature))
		return false;

	if (!isFloatControlsFeaturesSupported(context, requested.floatControlsProperties, missingFeature))
		return false;

	if (!isIntegerDotProductFeaturesSupported(context, requested.extIntegerDotProduct, missingFeature))
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
	case SPIRV_VERSION_1_4:
		return VK_API_VERSION_1_1;
	case SPIRV_VERSION_1_5:
		return VK_API_VERSION_1_2;
	case SPIRV_VERSION_1_6:
		return VK_API_VERSION_1_3;
	default:
		DE_ASSERT(0);
	}
	return 0u;
}

std::string	getVulkanName (const deUint32 version)
{
	if (version == VK_API_VERSION_1_1)	return "1.1";
	if (version == VK_API_VERSION_1_2)	return "1.2";
	if (version == VK_API_VERSION_1_3)	return "1.3";

	return "1.0";
}

// Generate and return 64-bit integers.
//
// Expected count to be at least 16.
std::vector<deInt64> getInt64s (de::Random& rnd, const deUint32 count)
{
	std::vector<deInt64> data;

	data.reserve(count);

	// Make sure we have boundary numbers.
	data.push_back(deInt64(0x0000000000000000));  // 0
	data.push_back(deInt64(0x0000000000000001));  // 1
	data.push_back(deInt64(0x000000000000002a));  // 42
	data.push_back(deInt64(0x000000007fffffff));  // 2147483647
	data.push_back(deInt64(0x0000000080000000));  // 2147483648
	data.push_back(deInt64(0x00000000ffffffff));  // 4294967295
	data.push_back(deInt64(0x0000000100000000));  // 4294967296
	data.push_back(deInt64(0x7fffffffffffffff));  // 9223372036854775807
	data.push_back(deInt64(0x8000000000000000));  // -9223372036854775808
	data.push_back(deInt64(0x8000000000000001));  // -9223372036854775807
	data.push_back(deInt64(0xffffffff00000000));  // -4294967296
	data.push_back(deInt64(0xffffffff00000001));  // -4294967295
	data.push_back(deInt64(0xffffffff80000000));  // -2147483648
	data.push_back(deInt64(0xffffffff80000001));  // -2147483647
	data.push_back(deInt64(0xffffffffffffffd6));  // -42
	data.push_back(deInt64(0xffffffffffffffff));  // -1

	DE_ASSERT(count >= data.size());

	for (deUint32 numNdx = static_cast<deUint32>(data.size()); numNdx < count; ++numNdx)
		data.push_back(static_cast<deInt64>(rnd.getUint64()));

	return data;
}

// Generate and return 32-bit integers.
//
// Expected count to be at least 16.
std::vector<deInt32> getInt32s (de::Random& rnd, const deUint32 count)
{
	std::vector<deInt32> data;

	data.reserve(count);

	// Make sure we have boundary numbers.
	data.push_back(deInt32(0x00000000));  // 0
	data.push_back(deInt32(0x00000001));  // 1
	data.push_back(deInt32(0x0000002a));  // 42
	data.push_back(deInt32(0x00007fff));  // 32767
	data.push_back(deInt32(0x00008000));  // 32768
	data.push_back(deInt32(0x0000ffff));  // 65535
	data.push_back(deInt32(0x00010000));  // 65536
	data.push_back(deInt32(0x7fffffff));  // 2147483647
	data.push_back(deInt32(0x80000000));  // -2147483648
	data.push_back(deInt32(0x80000001));  // -2147483647
	data.push_back(deInt32(0xffff0000));  // -65536
	data.push_back(deInt32(0xffff0001));  // -65535
	data.push_back(deInt32(0xffff8000));  // -32768
	data.push_back(deInt32(0xffff8001));  // -32767
	data.push_back(deInt32(0xffffffd6));  // -42
	data.push_back(deInt32(0xffffffff));  // -1

	DE_ASSERT(count >= data.size());

	for (deUint32 numNdx = static_cast<deUint32>(data.size()); numNdx < count; ++numNdx)
		data.push_back(static_cast<deInt32>(rnd.getUint32()));

	return data;
}

// Generate and return 16-bit integers.
//
// Expected count to be at least 8.
std::vector<deInt16> getInt16s (de::Random& rnd, const deUint32 count)
{
	std::vector<deInt16> data;

	data.reserve(count);

	// Make sure we have boundary numbers.
	data.push_back(deInt16(0x0000));  // 0
	data.push_back(deInt16(0x0001));  // 1
	data.push_back(deInt16(0x002a));  // 42
	data.push_back(deInt16(0x7fff));  // 32767
	data.push_back(deInt16(0x8000));  // -32868
	data.push_back(deInt16(0x8001));  // -32767
	data.push_back(deInt16(0xffd6));  // -42
	data.push_back(deInt16(0xffff));  // -1

	DE_ASSERT(count >= data.size());

	for (deUint32 numNdx = static_cast<deUint32>(data.size()); numNdx < count; ++numNdx)
		data.push_back(static_cast<deInt16>(rnd.getUint16()));

	return data;
}

// Generate and return 8-bit integers.
//
// Expected count to be at least 8.
std::vector<deInt8> getInt8s (de::Random& rnd, const deUint32 count)
{
	std::vector<deInt8> data;

	data.reserve(count);

	// Make sure we have boundary numbers.
	data.push_back(deInt8(0x00));  // 0
	data.push_back(deInt8(0x01));  // 1
	data.push_back(deInt8(0x2a));  // 42
	data.push_back(deInt8(0x7f));  // 127
	data.push_back(deInt8(0x80));  // -128
	data.push_back(deInt8(0x81));  // -127
	data.push_back(deInt8(0xd6));  // -42
	data.push_back(deInt8(0xff));  // -1

	DE_ASSERT(count >= data.size());

	for (deUint32 numNdx = static_cast<deUint32>(data.size()); numNdx < count; ++numNdx)
		data.push_back(static_cast<deInt8>(rnd.getUint8()));

	return data;
}

// IEEE-754 floating point numbers:
// +--------+------+----------+-------------+
// | binary | sign | exponent | significand |
// +--------+------+----------+-------------+
// | 64-bit |  1   |    11    |     52      |
// +--------+------+----------+-------------+
// | 32-bit |  1   |    8     |     23      |
// +--------+------+----------+-------------+
// | 16-bit |  1   |    5     |     10      |
// +--------+------+----------+-------------+
//
// 64-bit floats:
//
// (0x3FD2000000000000: 0.28125: with exact match in 16-bit normalized)
// (0x3F10060000000000: exact half way within two 16-bit normalized; round to zero: 0x0401)
// (0xBF10060000000000: exact half way within two 16-bit normalized; round to zero: 0x8402)
// (0x3F100C0000000000: not exact half way within two 16-bit normalized; round to zero: 0x0403)
// (0xBF100C0000000000: not exact half way within two 16-bit normalized; round to zero: 0x8404)

// Generate and return 64-bit floats
//
// The first 24 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 24 (numPicks).
std::vector<double> getFloat64s (de::Random& rnd, deUint32 count)
{
	std::vector<double> float64;

	float64.reserve(count);

	if (count >= 24)
	{
		// Zero
		float64.push_back(0.f);
		float64.push_back(-0.f);
		// Infinity
		float64.push_back(std::numeric_limits<double>::infinity());
		float64.push_back(-std::numeric_limits<double>::infinity());
		// SNaN
		float64.push_back(std::numeric_limits<double>::signaling_NaN());
		float64.push_back(-std::numeric_limits<double>::signaling_NaN());
		// QNaN
		float64.push_back(std::numeric_limits<double>::quiet_NaN());
		float64.push_back(-std::numeric_limits<double>::quiet_NaN());

		// Denormalized 64-bit float matching 0 in 16-bit
		float64.push_back(ldexp((double)1.f, -1023));
		float64.push_back(-ldexp((double)1.f, -1023));

		// Normalized 64-bit float matching 0 in 16-bit
		float64.push_back(ldexp((double)1.f, -100));
		float64.push_back(-ldexp((double)1.f, -100));
		// Normalized 64-bit float with exact denormalized match in 16-bit
		float64.push_back(bitwiseCast<double>(deUint64(0x3B0357C299A88EA8)));
		float64.push_back(bitwiseCast<double>(deUint64(0xBB0357C299A88EA8)));

		// Normalized 64-bit float with exact normalized match in 16-bit
		float64.push_back(ldexp((double)1.f, -14));  // 2e-14: minimum 16-bit positive normalized
		float64.push_back(-ldexp((double)1.f, -14)); // 2e-14: maximum 16-bit negative normalized
		// Normalized 64-bit float falling above half way within two 16-bit normalized
		float64.push_back(bitwiseCast<double>(deUint64(0x3FD2000000000000)));
		float64.push_back(bitwiseCast<double>(deUint64(0xBFD2000000000000)));
		// Normalized 64-bit float falling exact half way within two 16-bit normalized
		float64.push_back(bitwiseCast<double>(deUint64(0x3F100C0000000000)));
		float64.push_back(bitwiseCast<double>(deUint64(0xBF100C0000000000)));
		// Some number
		float64.push_back((double)0.28125f);
		float64.push_back((double)-0.28125f);
		// Normalized 64-bit float matching infinity in 16-bit
		float64.push_back(ldexp((double)1.f, 100));
		float64.push_back(-ldexp((double)1.f, 100));
	}

	const deUint32		numPicks	= static_cast<deUint32>(float64.size());

	DE_ASSERT(count >= numPicks);
	count -= numPicks;

	for (deUint32 numNdx = 0; numNdx < count; ++numNdx)
	{
		double randValue = rnd.getDouble();
		float64.push_back(randValue);
	}

	return float64;
}

// IEEE-754 floating point numbers:
// +--------+------+----------+-------------+
// | binary | sign | exponent | significand |
// +--------+------+----------+-------------+
// | 16-bit |  1   |    5     |     10      |
// +--------+------+----------+-------------+
// | 32-bit |  1   |    8     |     23      |
// +--------+------+----------+-------------+
//
// 16-bit floats:
//
// 0   000 00   00 0000 0001 (0x0001: 2e-24:         minimum positive denormalized)
// 0   000 00   11 1111 1111 (0x03ff: 2e-14 - 2e-24: maximum positive denormalized)
// 0   000 01   00 0000 0000 (0x0400: 2e-14:         minimum positive normalized)
//
// 32-bit floats:
//
// 0   011 1110 1   001 0000 0000 0000 0000 0000 (0x3e900000: 0.28125: with exact match in 16-bit normalized)
// 0   011 1000 1   000 0000 0011 0000 0000 0000 (0x38803000: exact half way within two 16-bit normalized; round to zero: 0x0401)
// 1   011 1000 1   000 0000 0011 0000 0000 0000 (0xb8803000: exact half way within two 16-bit normalized; round to zero: 0x8402)
// 0   011 1000 1   000 0000 1111 1111 0000 0000 (0x3880ff00: not exact half way within two 16-bit normalized; round to zero: 0x0403)
// 1   011 1000 1   000 0000 1111 1111 0000 0000 (0xb880ff00: not exact half way within two 16-bit normalized; round to zero: 0x8404)

// Generate and return 32-bit floats
//
// The first 24 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 24 (numPicks).
std::vector<float> getFloat32s (de::Random& rnd, deUint32 count)
{
	std::vector<float> float32;

	float32.reserve(count);

	// Zero
	float32.push_back(0.f);
	float32.push_back(-0.f);
	// Infinity
	float32.push_back(std::numeric_limits<float>::infinity());
	float32.push_back(-std::numeric_limits<float>::infinity());
	// SNaN
	float32.push_back(std::numeric_limits<float>::signaling_NaN());
	float32.push_back(-std::numeric_limits<float>::signaling_NaN());
	// QNaN
	float32.push_back(std::numeric_limits<float>::quiet_NaN());
	float32.push_back(-std::numeric_limits<float>::quiet_NaN());

	// Denormalized 32-bit float matching 0 in 16-bit
	float32.push_back(deFloatLdExp(1.f, -127));
	float32.push_back(-deFloatLdExp(1.f, -127));

	// Normalized 32-bit float matching 0 in 16-bit
	float32.push_back(deFloatLdExp(1.f, -100));
	float32.push_back(-deFloatLdExp(1.f, -100));
	// Normalized 32-bit float with exact denormalized match in 16-bit
	float32.push_back(deFloatLdExp(1.f, -24));  // 2e-24: minimum 16-bit positive denormalized
	float32.push_back(-deFloatLdExp(1.f, -24)); // 2e-24: maximum 16-bit negative denormalized
	// Normalized 32-bit float with exact normalized match in 16-bit
	float32.push_back(deFloatLdExp(1.f, -14));  // 2e-14: minimum 16-bit positive normalized
	float32.push_back(-deFloatLdExp(1.f, -14)); // 2e-14: maximum 16-bit negative normalized
	// Normalized 32-bit float falling above half way within two 16-bit normalized
	float32.push_back(bitwiseCast<float>(deUint32(0x3880ff00)));
	float32.push_back(bitwiseCast<float>(deUint32(0xb880ff00)));
	// Normalized 32-bit float falling exact half way within two 16-bit normalized
	float32.push_back(bitwiseCast<float>(deUint32(0x38803000)));
	float32.push_back(bitwiseCast<float>(deUint32(0xb8803000)));
	// Some number
	float32.push_back(0.28125f);
	float32.push_back(-0.28125f);
	// Normalized 32-bit float matching infinity in 16-bit
	float32.push_back(deFloatLdExp(1.f, 100));
	float32.push_back(-deFloatLdExp(1.f, 100));

	const deUint32		numPicks	= static_cast<deUint32>(float32.size());

	DE_ASSERT(count >= numPicks);
	count -= numPicks;

	for (deUint32 numNdx = 0; numNdx < count; ++numNdx)
		float32.push_back(rnd.getFloat());

	return float32;
}

// IEEE-754 floating point numbers:
// +--------+------+----------+-------------+
// | binary | sign | exponent | significand |
// +--------+------+----------+-------------+
// | 16-bit |  1   |    5     |     10      |
// +--------+------+----------+-------------+
// | 32-bit |  1   |    8     |     23      |
// +--------+------+----------+-------------+
//
// 16-bit floats:
//
// 0   000 00   00 0000 0001 (0x0001: 2e-24:         minimum positive denormalized)
// 0   000 00   11 1111 1111 (0x03ff: 2e-14 - 2e-24: maximum positive denormalized)
// 0   000 01   00 0000 0000 (0x0400: 2e-14:         minimum positive normalized)
//
// 0   000 00   00 0000 0000 (0x0000: +0)
// 0   111 11   00 0000 0000 (0x7c00: +Inf)
// 0   000 00   11 1111 0000 (0x03f0: +Denorm)
// 0   000 01   00 0000 0001 (0x0401: +Norm)
// 0   111 11   00 0000 1111 (0x7c0f: +SNaN)
// 0   111 11   00 1111 0000 (0x7c0f: +QNaN)

// Generate and return 16-bit floats and their corresponding 32-bit values.
//
// The first 14 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 14 (numPicks).
std::vector<deFloat16> getFloat16s (de::Random& rnd, deUint32 count)
{
	std::vector<deFloat16> float16;

	float16.reserve(count);

	// Zero
	float16.push_back(deUint16(0x0000));
	float16.push_back(deUint16(0x8000));
	// Infinity
	float16.push_back(deUint16(0x7c00));
	float16.push_back(deUint16(0xfc00));
	// SNaN
	float16.push_back(deUint16(0x7c0f));
	float16.push_back(deUint16(0xfc0f));
	// QNaN
	float16.push_back(deUint16(0x7cf0));
	float16.push_back(deUint16(0xfcf0));

	// Denormalized
	float16.push_back(deUint16(0x03f0));
	float16.push_back(deUint16(0x83f0));
	// Normalized
	float16.push_back(deUint16(0x0401));
	float16.push_back(deUint16(0x8401));
	// Some normal number
	float16.push_back(deUint16(0x14cb));
	float16.push_back(deUint16(0x94cb));

	const deUint32		numPicks	= static_cast<deUint32>(float16.size());

	DE_ASSERT(count >= numPicks);
	count -= numPicks;

	for (deUint32 numIdx = 0; numIdx < count; ++numIdx)
		float16.push_back(rnd.getUint16());

	return float16;
}

std::string getOpCapabilityShader()
{
	return	"OpCapability Shader\n";
}

std::string getUnusedEntryPoint()
{
	return	"OpEntryPoint Vertex %unused_func \"unused_func\"\n";
}

std::string getUnusedDecorations(const VariableLocation& location)
{
	return	"OpMemberDecorate %UnusedBufferType 0 Offset 0\n"
            "OpMemberDecorate %UnusedBufferType 1 Offset 4\n"
            "OpDecorate %UnusedBufferType BufferBlock\n"
            "OpDecorate %unused_buffer DescriptorSet " + de::toString(location.set) + "\n"
            "OpDecorate %unused_buffer Binding " + de::toString(location.binding) + "\n";
}

std::string getUnusedTypesAndConstants()
{
	return	"%c_f32_101 = OpConstant %f32 101\n"
			"%c_i32_201 = OpConstant %i32 201\n"
			"%UnusedBufferType = OpTypeStruct %f32 %i32\n"
			"%unused_ptr_Uniform_UnusedBufferType = OpTypePointer Uniform %UnusedBufferType\n"
			"%unused_ptr_Uniform_float = OpTypePointer Uniform %f32\n"
			"%unused_ptr_Uniform_int = OpTypePointer Uniform %i32\n";
}

std::string getUnusedBuffer()
{
	return	"%unused_buffer = OpVariable %unused_ptr_Uniform_UnusedBufferType Uniform\n";
}

std::string getUnusedFunctionBody()
{
	return	"%unused_func = OpFunction %void None %voidf\n"
			"%unused_func_label = OpLabel\n"
			"%unused_out_float_ptr = OpAccessChain %unused_ptr_Uniform_float %unused_buffer %c_i32_0\n"
            "OpStore %unused_out_float_ptr %c_f32_101\n"
			"%unused_out_int_ptr = OpAccessChain %unused_ptr_Uniform_int %unused_buffer %c_i32_1\n"
            "OpStore %unused_out_int_ptr %c_i32_201\n"
            "OpReturn\n"
            "OpFunctionEnd\n";
}

} // SpirVAssembly
} // vkt
