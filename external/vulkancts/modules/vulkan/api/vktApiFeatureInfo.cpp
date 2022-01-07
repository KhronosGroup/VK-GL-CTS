/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Api Feature Query tests
 *//*--------------------------------------------------------------------*/

#include "vktApiFeatureInfo.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkApiVersion.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"
#include "deMemory.h"
#include "deMath.h"

#include <vector>
#include <set>
#include <string>
#include <limits>

namespace vkt
{
namespace api
{
namespace
{

#include "vkApiExtensionDependencyInfo.inl"

using namespace vk;
using std::vector;
using std::set;
using std::string;
using tcu::TestLog;
using tcu::ScopedLogSection;

const deUint32 DEUINT32_MAX = std::numeric_limits<deUint32>::max();

enum
{
	GUARD_SIZE								= 0x20,			//!< Number of bytes to check
	GUARD_VALUE								= 0xcd,			//!< Data pattern
};

static const VkDeviceSize MINIMUM_REQUIRED_IMAGE_RESOURCE_SIZE =	(1LLU<<31);	//!< Minimum value for VkImageFormatProperties::maxResourceSize (2GiB)

enum LimitFormat
{
	LIMIT_FORMAT_SIGNED_INT,
	LIMIT_FORMAT_UNSIGNED_INT,
	LIMIT_FORMAT_FLOAT,
	LIMIT_FORMAT_DEVICE_SIZE,
	LIMIT_FORMAT_BITMASK,

	LIMIT_FORMAT_LAST
};

enum LimitType
{
	LIMIT_TYPE_MIN,
	LIMIT_TYPE_MAX,
	LIMIT_TYPE_NONE,

	LIMIT_TYPE_LAST
};

#define LIMIT(_X_)		DE_OFFSET_OF(VkPhysicalDeviceLimits, _X_), (const char*)(#_X_)
#define FEATURE(_X_)	DE_OFFSET_OF(VkPhysicalDeviceFeatures, _X_)

bool validateFeatureLimits(VkPhysicalDeviceProperties* properties, VkPhysicalDeviceFeatures* features, TestLog& log)
{
	bool						limitsOk				= true;
	VkPhysicalDeviceLimits*		limits					= &properties->limits;
	deUint32					shaderStages			= 3;
	deUint32					maxPerStageResourcesMin	= deMin32(128,	limits->maxPerStageDescriptorUniformBuffers		+
																		limits->maxPerStageDescriptorStorageBuffers		+
																		limits->maxPerStageDescriptorSampledImages		+
																		limits->maxPerStageDescriptorStorageImages		+
																		limits->maxPerStageDescriptorInputAttachments	+
																		limits->maxColorAttachments);

	if (features->tessellationShader)
	{
		shaderStages += 2;
	}

	if (features->geometryShader)
	{
		shaderStages++;
	}

	struct FeatureLimitTable
	{
		deUint32		offset;
		const char*		name;
		deUint32		uintVal;			//!< Format is UNSIGNED_INT
		deInt32			intVal;				//!< Format is SIGNED_INT
		deUint64		deviceSizeVal;		//!< Format is DEVICE_SIZE
		float			floatVal;			//!< Format is FLOAT
		LimitFormat		format;
		LimitType		type;
		deInt32			unsuppTableNdx;
	} featureLimitTable[] =   //!< Based on 1.0.28 Vulkan spec
	{
		{ LIMIT(maxImageDimension1D),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxImageDimension2D),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxImageDimension3D),								256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxImageDimensionCube),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxImageArrayLayers),								256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxTexelBufferElements),							65536, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxUniformBufferRange),								16384, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxStorageBufferRange),								134217728, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxPushConstantsSize),								128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxMemoryAllocationCount),							4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxSamplerAllocationCount),							4000, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(bufferImageGranularity),							0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(bufferImageGranularity),							0, 0, 131072, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(sparseAddressSpaceSize),							0, 0, 2UL*1024*1024*1024, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxBoundDescriptorSets),							4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxPerStageDescriptorSamplers),						16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxPerStageDescriptorUniformBuffers),				12, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorStorageBuffers),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorSampledImages),				16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorStorageImages),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorInputAttachments),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageResources),								maxPerStageResourcesMin, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxDescriptorSetSamplers),							shaderStages * 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetUniformBuffers),					shaderStages * 12, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetUniformBuffersDynamic),				8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetStorageBuffers),					shaderStages * 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetStorageBuffersDynamic),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxDescriptorSetSampledImages),						shaderStages * 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetStorageImages),						shaderStages * 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetInputAttachments),					4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxVertexInputAttributes),							16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxVertexInputBindings),							16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxVertexInputAttributeOffset),						2047, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxVertexInputBindingStride),						2048, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxVertexOutputComponents),							64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationGenerationLevel),					64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationPatchSize),							32, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxTessellationControlPerVertexInputComponents),	64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationControlPerVertexOutputComponents),	64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationControlPerPatchOutputComponents),	120, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationControlTotalOutputComponents),		2048, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationEvaluationInputComponents),			64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxTessellationEvaluationOutputComponents),			64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxGeometryShaderInvocations),						32, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxGeometryInputComponents),						64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxGeometryOutputComponents),						64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxGeometryOutputVertices),							256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxGeometryTotalOutputComponents),					1024, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxFragmentInputComponents),						64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxFragmentOutputAttachments),						4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxFragmentDualSrcAttachments),						1, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxFragmentCombinedOutputResources),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxComputeSharedMemorySize),						16384, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxComputeWorkGroupCount[0]),						65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxComputeWorkGroupCount[1]),						65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxComputeWorkGroupCount[2]),						65535,  0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxComputeWorkGroupInvocations),					128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(maxComputeWorkGroupSize[0]),						128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(maxComputeWorkGroupSize[1]),						128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(maxComputeWorkGroupSize[2]),						64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(subPixelPrecisionBits),								4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(subTexelPrecisionBits),								4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(mipmapPrecisionBits),								4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(maxDrawIndexedIndexValue),							(deUint32)~0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDrawIndirectCount),								65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    , -1 },
		{ LIMIT(maxSamplerLodBias),									0, 0, 0, 2.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxSamplerAnisotropy),								0, 0, 0, 16.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxViewports),										16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxViewportDimensions[0]),							4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxViewportDimensions[1]),							4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(viewportBoundsRange[0]),							0, 0, 0, -8192.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(viewportBoundsRange[1]),							0, 0, 0, 8191.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(viewportSubPixelBits),								0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minMemoryMapAlignment),								64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minTexelBufferOffsetAlignment),						0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minTexelBufferOffsetAlignment),						0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minUniformBufferOffsetAlignment),					0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minUniformBufferOffsetAlignment),					0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minStorageBufferOffsetAlignment),					0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minStorageBufferOffsetAlignment),					0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minTexelOffset),									0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxTexelOffset),									7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minTexelGatherOffset),								0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxTexelGatherOffset),								7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minInterpolationOffset),							0, 0, 0, -0.5f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxInterpolationOffset),							0, 0, 0, 0.5f - (1.0f/deFloatPow(2.0f, (float)limits->subPixelInterpolationOffsetBits)), LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(subPixelInterpolationOffsetBits),					4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferWidth),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferHeight),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferLayers),								0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(framebufferColorSampleCounts),						VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(framebufferDepthSampleCounts),						VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(framebufferStencilSampleCounts),					VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(framebufferNoAttachmentsSampleCounts),				VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxColorAttachments),								4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(sampledImageColorSampleCounts),						VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(sampledImageIntegerSampleCounts),					VK_SAMPLE_COUNT_1_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(sampledImageDepthSampleCounts),						VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(sampledImageStencilSampleCounts),					VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(storageImageSampleCounts),							VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxSampleMaskWords),								1, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(timestampComputeAndGraphics),						0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(timestampPeriod),									0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(maxClipDistances),									8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxCullDistances),									8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxCombinedClipAndCullDistances),					8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(discreteQueuePriorities),							2, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeRange[0]),									0, 0, 0, 0.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(pointSizeRange[1]),									0, 0, 0, 64.0f - limits->pointSizeGranularity , LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(lineWidthRange[0]),									0, 0, 0, 0.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(lineWidthRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(lineWidthRange[1]),									0, 0, 0, 8.0f - limits->lineWidthGranularity, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeGranularity),								0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(lineWidthGranularity),								0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(strictLines),										0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(standardSampleLocations),							0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(optimalBufferCopyOffsetAlignment),					0, 0, 0, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(optimalBufferCopyRowPitchAlignment),				0, 0, 0, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_NONE, -1 },
		{ LIMIT(nonCoherentAtomSize),								0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(nonCoherentAtomSize),								0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
	};

	const struct UnsupportedFeatureLimitTable
	{
		deUint32		limitOffset;
		const char*		name;
		deUint32		featureOffset;
		deUint32		uintVal;			//!< Format is UNSIGNED_INT
		deInt32			intVal;				//!< Format is SIGNED_INT
		deUint64		deviceSizeVal;		//!< Format is DEVICE_SIZE
		float			floatVal;			//!< Format is FLOAT
	} unsupportedFeatureTable[] =
	{
		{ LIMIT(sparseAddressSpaceSize),							FEATURE(sparseBinding),					0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationGenerationLevel),					FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationPatchSize),							FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationControlPerVertexInputComponents),	FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationControlPerVertexOutputComponents),	FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationControlPerPatchOutputComponents),	FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationControlTotalOutputComponents),		FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationEvaluationInputComponents),			FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxTessellationEvaluationOutputComponents),			FEATURE(tessellationShader),			0, 0, 0, 0.0f },
		{ LIMIT(maxGeometryShaderInvocations),						FEATURE(geometryShader),				0, 0, 0, 0.0f },
		{ LIMIT(maxGeometryInputComponents),						FEATURE(geometryShader),				0, 0, 0, 0.0f },
		{ LIMIT(maxGeometryOutputComponents),						FEATURE(geometryShader),				0, 0, 0, 0.0f },
		{ LIMIT(maxGeometryOutputVertices),							FEATURE(geometryShader),				0, 0, 0, 0.0f },
		{ LIMIT(maxGeometryTotalOutputComponents),					FEATURE(geometryShader),				0, 0, 0, 0.0f },
		{ LIMIT(maxFragmentDualSrcAttachments),						FEATURE(dualSrcBlend),					0, 0, 0, 0.0f },
		{ LIMIT(maxDrawIndexedIndexValue),							FEATURE(fullDrawIndexUint32),			(1<<24)-1, 0, 0, 0.0f },
		{ LIMIT(maxDrawIndirectCount),								FEATURE(multiDrawIndirect),				1, 0, 0, 0.0f },
		{ LIMIT(maxSamplerAnisotropy),								FEATURE(samplerAnisotropy),				1, 0, 0, 0.0f },
		{ LIMIT(maxViewports),										FEATURE(multiViewport),					1, 0, 0, 0.0f },
		{ LIMIT(minTexelGatherOffset),								FEATURE(shaderImageGatherExtended),		0, 0, 0, 0.0f },
		{ LIMIT(maxTexelGatherOffset),								FEATURE(shaderImageGatherExtended),		0, 0, 0, 0.0f },
		{ LIMIT(minInterpolationOffset),							FEATURE(sampleRateShading),				0, 0, 0, 0.0f },
		{ LIMIT(maxInterpolationOffset),							FEATURE(sampleRateShading),				0, 0, 0, 0.0f },
		{ LIMIT(subPixelInterpolationOffsetBits),					FEATURE(sampleRateShading),				0, 0, 0, 0.0f },
		{ LIMIT(storageImageSampleCounts),							FEATURE(shaderStorageImageMultisample),	VK_SAMPLE_COUNT_1_BIT, 0, 0, 0.0f },
		{ LIMIT(maxClipDistances),									FEATURE(shaderClipDistance),			0, 0, 0, 0.0f },
		{ LIMIT(maxCullDistances),									FEATURE(shaderCullDistance),			0, 0, 0, 0.0f },
		{ LIMIT(maxCombinedClipAndCullDistances),					FEATURE(shaderClipDistance),			0, 0, 0, 0.0f },
		{ LIMIT(pointSizeRange[0]),									FEATURE(largePoints),					0, 0, 0, 1.0f },
		{ LIMIT(pointSizeRange[1]),									FEATURE(largePoints),					0, 0, 0, 1.0f },
		{ LIMIT(lineWidthRange[0]),									FEATURE(wideLines),						0, 0, 0, 1.0f },
		{ LIMIT(lineWidthRange[1]),									FEATURE(wideLines),						0, 0, 0, 1.0f },
		{ LIMIT(pointSizeGranularity),								FEATURE(largePoints),					0, 0, 0, 0.0f },
		{ LIMIT(lineWidthGranularity),								FEATURE(wideLines),						0, 0, 0, 0.0f }
	};

	log << TestLog::Message << *limits << TestLog::EndMessage;

	//!< First build a map from limit to unsupported table index
	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
	{
		for (deUint32 unsuppNdx = 0; unsuppNdx < DE_LENGTH_OF_ARRAY(unsupportedFeatureTable); unsuppNdx++)
		{
			if (unsupportedFeatureTable[unsuppNdx].limitOffset == featureLimitTable[ndx].offset)
			{
				featureLimitTable[ndx].unsuppTableNdx = unsuppNdx;
				break;
			}
		}
	}

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
	{
		switch (featureLimitTable[ndx].format)
		{
			case LIMIT_FORMAT_UNSIGNED_INT:
			{
				deUint32 limitToCheck = featureLimitTable[ndx].uintVal;
				if (featureLimitTable[ndx].unsuppTableNdx != -1)
				{
					if (*((VkBool32*)((deUint8*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].uintVal;
				}

				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{

					if (*((deUint32*)((deUint8*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit Validation failed " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((deUint32*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
				{
					if (*((deUint32*)((deUint8*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed,  " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX - actual is "
							<< *((deUint32*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;
			}

			case LIMIT_FORMAT_FLOAT:
			{
				float limitToCheck = featureLimitTable[ndx].floatVal;
				if (featureLimitTable[ndx].unsuppTableNdx != -1)
				{
					if (*((VkBool32*)((deUint8*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].floatVal;
				}

				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if (*((float*)((deUint8*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((float*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
				{
					if (*((float*)((deUint8*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((float*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;
			}

			case LIMIT_FORMAT_SIGNED_INT:
			{
				deInt32 limitToCheck = featureLimitTable[ndx].intVal;
				if (featureLimitTable[ndx].unsuppTableNdx != -1)
				{
					if (*((VkBool32*)((deUint8*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].intVal;
				}
				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if (*((deInt32*)((deUint8*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message <<  "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deInt32*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
				{
					if (*((deInt32*)((deUint8*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deInt32*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;
			}

			case LIMIT_FORMAT_DEVICE_SIZE:
			{
				deUint64 limitToCheck = featureLimitTable[ndx].deviceSizeVal;
				if (featureLimitTable[ndx].unsuppTableNdx != -1)
				{
					if (*((VkBool32*)((deUint8*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].deviceSizeVal;
				}

				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if (*((deUint64*)((deUint8*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deUint64*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
				{
					if (*((deUint64*)((deUint8*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deUint64*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;
			}

			case LIMIT_FORMAT_BITMASK:
			{
				deUint32 limitToCheck = featureLimitTable[ndx].uintVal;
				if (featureLimitTable[ndx].unsuppTableNdx != -1)
				{
					if (*((VkBool32*)((deUint8*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].uintVal;
				}

				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if ((*((deUint32*)((deUint8*)limits+featureLimitTable[ndx].offset)) & limitToCheck) != limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type bitmask actual is "
							<< *((deUint64*)((deUint8*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;
			}

			default:
				DE_ASSERT(0);
				limitsOk = false;
		}
	}

	if (limits->maxFramebufferWidth > limits->maxViewportDimensions[0] ||
		limits->maxFramebufferHeight > limits->maxViewportDimensions[1])
	{
		log << TestLog::Message << "limit validation failed, maxFramebufferDimension of "
			<< "[" << limits->maxFramebufferWidth << ", " << limits->maxFramebufferHeight << "] "
			<< "is larger than maxViewportDimension of "
			<< "[" << limits->maxViewportDimensions[0] << ", " << limits->maxViewportDimensions[1] << "]" << TestLog::EndMessage;
		limitsOk = false;
	}

	if (limits->viewportBoundsRange[0] > float(-2 * limits->maxViewportDimensions[0]))
	{
		log << TestLog::Message << "limit validation failed, viewPortBoundsRange[0] of " << limits->viewportBoundsRange[0]
			<< "is larger than -2*maxViewportDimension[0] of " << -2*limits->maxViewportDimensions[0] << TestLog::EndMessage;
		limitsOk = false;
	}

	if (limits->viewportBoundsRange[1] < float(2 * limits->maxViewportDimensions[1] - 1))
	{
		log << TestLog::Message << "limit validation failed, viewportBoundsRange[1] of " << limits->viewportBoundsRange[1]
			<< "is less than 2*maxViewportDimension[1] of " << 2*limits->maxViewportDimensions[1] << TestLog::EndMessage;
		limitsOk = false;
	}

	return limitsOk;
}

void validateLimitsCheckSupport (Context& context)
{
	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");
}

typedef struct FeatureLimitTableItem_
{
	const void*		cond;
	const char*		condName;
	const void*		ptr;
	const char*		name;
	deUint32		uintVal;			//!< Format is UNSIGNED_INT
	deInt32			intVal;				//!< Format is SIGNED_INT
	deUint64		deviceSizeVal;		//!< Format is DEVICE_SIZE
	float			floatVal;			//!< Format is FLOAT
	LimitFormat		format;
	LimitType		type;
} FeatureLimitTableItem;

template<typename T>
bool validateNumericLimit (const T limitToCheck, const T reportedValue, const LimitType limitType, const char* limitName, TestLog& log)
{
	if (limitType == LIMIT_TYPE_MIN)
	{
		if (reportedValue < limitToCheck)
		{
			log << TestLog::Message << "Limit validation failed " << limitName
				<< " reported value is " << reportedValue
				<< " expected MIN " << limitToCheck
				<< TestLog::EndMessage;

			return false;
		}

		log << TestLog::Message << limitName
			<< "=" << reportedValue
			<< " (>=" << limitToCheck << ")"
			<< TestLog::EndMessage;
	}
	else if (limitType == LIMIT_TYPE_MAX)
	{
		if (reportedValue > limitToCheck)
		{
			log << TestLog::Message << "Limit validation failed " << limitName
				<< " reported value is " << reportedValue
				<< " expected MAX " << limitToCheck
				<< TestLog::EndMessage;

			return false;
		}

		log << TestLog::Message << limitName
			<< "=" << reportedValue
			<< " (<=" << limitToCheck << ")"
			<< TestLog::EndMessage;
	}

	return true;
}

template<typename T>
bool validateBitmaskLimit (const T limitToCheck, const T reportedValue, const LimitType limitType, const char* limitName, TestLog& log)
{
	if (limitType == LIMIT_TYPE_MIN)
	{
		if ((reportedValue & limitToCheck) != limitToCheck)
		{
			log << TestLog::Message << "Limit validation failed " << limitName
				<< " reported value is " << reportedValue
				<< " expected MIN " << limitToCheck
				<< TestLog::EndMessage;

			return false;
		}

		log << TestLog::Message << limitName
			<< "=" << tcu::toHex(reportedValue)
			<< " (contains " << tcu::toHex(limitToCheck) << ")"
			<< TestLog::EndMessage;
	}

	return true;
}

bool validateLimit (FeatureLimitTableItem limit, TestLog& log)
{
	if (*((VkBool32*)limit.cond) == DE_FALSE)
	{
		log << TestLog::Message
			<< "Limit validation skipped '" << limit.name << "' due to "
			<< limit.condName << " == false'"
			<< TestLog::EndMessage;

		return true;
	}

	switch (limit.format)
	{
		case LIMIT_FORMAT_UNSIGNED_INT:
		{
			const deUint32	limitToCheck	= limit.uintVal;
			const deUint32	reportedValue	= *(deUint32*)limit.ptr;

			return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
		}

		case LIMIT_FORMAT_FLOAT:
		{
			const float		limitToCheck	= limit.floatVal;
			const float		reportedValue	= *(float*)limit.ptr;

			return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
		}

		case LIMIT_FORMAT_SIGNED_INT:
		{
			const deInt32	limitToCheck	= limit.intVal;
			const deInt32	reportedValue	= *(deInt32*)limit.ptr;

			return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
		}

		case LIMIT_FORMAT_DEVICE_SIZE:
		{
			const deUint64	limitToCheck	= limit.deviceSizeVal;
			const deUint64	reportedValue	= *(deUint64*)limit.ptr;

			return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
		}

		case LIMIT_FORMAT_BITMASK:
		{
			const deUint32	limitToCheck	= limit.uintVal;
			const deUint32	reportedValue	= *(deUint32*)limit.ptr;

			return validateBitmaskLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
		}

		default:
			TCU_THROW(InternalError, "Unknown LimitFormat specified");
	}
}

#ifdef PN
#error PN defined
#else
#define PN(_X_)	&(_X_), (const char*)(#_X_)
#endif

#define LIM_MIN_UINT32(X)	deUint32(X),          0,               0,     0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN
#define LIM_MAX_UINT32(X)	deUint32(X),          0,               0,     0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX
#define LIM_NONE_UINT32		          0,          0,               0,     0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE
#define LIM_MIN_INT32(X)	          0, deInt32(X),               0,     0.0f, LIMIT_FORMAT_SIGNED_INT,   LIMIT_TYPE_MIN
#define LIM_MAX_INT32(X)	          0, deInt32(X),               0,     0.0f, LIMIT_FORMAT_SIGNED_INT,   LIMIT_TYPE_MAX
#define LIM_NONE_INT32		          0,          0,               0,     0.0f, LIMIT_FORMAT_SIGNED_INT,   LIMIT_TYPE_NONE
#define LIM_MIN_DEVSIZE(X)	          0,          0, VkDeviceSize(X),     0.0f, LIMIT_FORMAT_DEVICE_SIZE,  LIMIT_TYPE_MIN
#define LIM_MAX_DEVSIZE(X)	          0,          0, VkDeviceSize(X),     0.0f, LIMIT_FORMAT_DEVICE_SIZE,  LIMIT_TYPE_MAX
#define LIM_NONE_DEVSIZE	          0,          0,               0,     0.0f, LIMIT_FORMAT_DEVICE_SIZE,  LIMIT_TYPE_NONE
#define LIM_MIN_FLOAT(X)	          0,          0,               0, float(X), LIMIT_FORMAT_FLOAT,        LIMIT_TYPE_MIN
#define LIM_MAX_FLOAT(X)	          0,          0,               0, float(X), LIMIT_FORMAT_FLOAT,        LIMIT_TYPE_MAX
#define LIM_NONE_FLOAT		          0,          0,               0,     0.0f, LIMIT_FORMAT_FLOAT,        LIMIT_TYPE_NONE
#define LIM_MIN_BITI32(X)	deUint32(X),          0,               0,     0.0f, LIMIT_FORMAT_BITMASK,      LIMIT_TYPE_MIN
#define LIM_MAX_BITI32(X)	deUint32(X),          0,               0,     0.0f, LIMIT_FORMAT_BITMASK,      LIMIT_TYPE_MAX
#define LIM_NONE_BITI32		          0,          0,               0,     0.0f, LIMIT_FORMAT_BITMASK,      LIMIT_TYPE_NONE

tcu::TestStatus validateLimits12 (Context& context)
{
	const VkPhysicalDevice						physicalDevice			= context.getPhysicalDevice();
	const InstanceInterface&					vki						= context.getInstanceInterface();
	TestLog&									log						= context.getTestContext().getLog();
	bool										limitsOk				= true;

	const VkPhysicalDeviceFeatures2&			features2				= context.getDeviceFeatures2();
	const VkPhysicalDeviceFeatures&				features				= features2.features;
	const VkPhysicalDeviceVulkan12Features		features12				= getPhysicalDeviceVulkan12Features(vki, physicalDevice);

	const VkPhysicalDeviceProperties2&			properties2				= context.getDeviceProperties2();
	const VkPhysicalDeviceVulkan12Properties	vulkan12Properties		= getPhysicalDeviceVulkan12Properties(vki, physicalDevice);
	const VkPhysicalDeviceVulkan11Properties	vulkan11Properties		= getPhysicalDeviceVulkan11Properties(vki, physicalDevice);
	const VkPhysicalDeviceLimits&				limits					= properties2.properties.limits;

	const VkBool32								checkAlways				= VK_TRUE;
	const VkBool32								checkVulkan12Limit		= VK_TRUE;

	deUint32									shaderStages			= 3;
	deUint32									maxPerStageResourcesMin	= deMin32(128,	limits.maxPerStageDescriptorUniformBuffers		+
																						limits.maxPerStageDescriptorStorageBuffers		+
																						limits.maxPerStageDescriptorSampledImages		+
																						limits.maxPerStageDescriptorStorageImages		+
																						limits.maxPerStageDescriptorInputAttachments	+
																						limits.maxColorAttachments);

	if (features.tessellationShader)
	{
		shaderStages += 2;
	}

	if (features.geometryShader)
	{
		shaderStages++;
	}

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),								PN(limits.maxImageDimension1D),																	LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxImageDimension2D),																	LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxImageDimension3D),																	LIM_MIN_UINT32(256) },
		{ PN(checkAlways),								PN(limits.maxImageDimensionCube),																LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxImageArrayLayers),																	LIM_MIN_UINT32(256) },
		{ PN(checkAlways),								PN(limits.maxTexelBufferElements),																LIM_MIN_UINT32(65536) },
		{ PN(checkAlways),								PN(limits.maxUniformBufferRange),																LIM_MIN_UINT32(16384) },
		{ PN(checkAlways),								PN(limits.maxStorageBufferRange),																LIM_MIN_UINT32((1<<27)) },
		{ PN(checkAlways),								PN(limits.maxPushConstantsSize),																LIM_MIN_UINT32(128) },
		{ PN(checkAlways),								PN(limits.maxMemoryAllocationCount),															LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxSamplerAllocationCount),															LIM_MIN_UINT32(4000) },
		{ PN(checkAlways),								PN(limits.bufferImageGranularity),																LIM_MIN_DEVSIZE(1) },
		{ PN(checkAlways),								PN(limits.bufferImageGranularity),																LIM_MAX_DEVSIZE(131072) },
		{ PN(features.sparseBinding),					PN(limits.sparseAddressSpaceSize),																LIM_MIN_DEVSIZE((1ull<<31)) },
		{ PN(checkAlways),								PN(limits.maxBoundDescriptorSets),																LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorSamplers),														LIM_MIN_UINT32(16) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorUniformBuffers),													LIM_MIN_UINT32(12) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorStorageBuffers),													LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorSampledImages),													LIM_MIN_UINT32(16) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorStorageImages),													LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxPerStageDescriptorInputAttachments),												LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxPerStageResources),																LIM_MIN_UINT32(maxPerStageResourcesMin) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetSamplers),															LIM_MIN_UINT32(shaderStages * 16) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetUniformBuffers),														LIM_MIN_UINT32(shaderStages * 12) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetUniformBuffersDynamic),												LIM_MIN_UINT32(8) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetStorageBuffers),														LIM_MIN_UINT32(shaderStages * 4) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetStorageBuffersDynamic),												LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetSampledImages),														LIM_MIN_UINT32(shaderStages * 16) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetStorageImages),														LIM_MIN_UINT32(shaderStages * 4) },
		{ PN(checkAlways),								PN(limits.maxDescriptorSetInputAttachments),													LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxVertexInputAttributes),															LIM_MIN_UINT32(16) },
		{ PN(checkAlways),								PN(limits.maxVertexInputBindings),																LIM_MIN_UINT32(16) },
		{ PN(checkAlways),								PN(limits.maxVertexInputAttributeOffset),														LIM_MIN_UINT32(2047) },
		{ PN(checkAlways),								PN(limits.maxVertexInputBindingStride),															LIM_MIN_UINT32(2048) },
		{ PN(checkAlways),								PN(limits.maxVertexOutputComponents),															LIM_MIN_UINT32(64) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationGenerationLevel),														LIM_MIN_UINT32(64) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationPatchSize),															LIM_MIN_UINT32(32) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationControlPerVertexInputComponents),										LIM_MIN_UINT32(64) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationControlPerVertexOutputComponents),										LIM_MIN_UINT32(64) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationControlPerPatchOutputComponents),										LIM_MIN_UINT32(120) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationControlTotalOutputComponents),											LIM_MIN_UINT32(2048) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationEvaluationInputComponents),											LIM_MIN_UINT32(64) },
		{ PN(features.tessellationShader),				PN(limits.maxTessellationEvaluationOutputComponents),											LIM_MIN_UINT32(64) },
		{ PN(features.geometryShader),					PN(limits.maxGeometryShaderInvocations),														LIM_MIN_UINT32(32) },
		{ PN(features.geometryShader),					PN(limits.maxGeometryInputComponents),															LIM_MIN_UINT32(64) },
		{ PN(features.geometryShader),					PN(limits.maxGeometryOutputComponents),															LIM_MIN_UINT32(64) },
		{ PN(features.geometryShader),					PN(limits.maxGeometryOutputVertices),															LIM_MIN_UINT32(256) },
		{ PN(features.geometryShader),					PN(limits.maxGeometryTotalOutputComponents),													LIM_MIN_UINT32(1024) },
		{ PN(checkAlways),								PN(limits.maxFragmentInputComponents),															LIM_MIN_UINT32(64) },
		{ PN(checkAlways),								PN(limits.maxFragmentOutputAttachments),														LIM_MIN_UINT32(4) },
		{ PN(features.dualSrcBlend),					PN(limits.maxFragmentDualSrcAttachments),														LIM_MIN_UINT32(1) },
		{ PN(checkAlways),								PN(limits.maxFragmentCombinedOutputResources),													LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxComputeSharedMemorySize),															LIM_MIN_UINT32(16384) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupCount[0]),															LIM_MIN_UINT32(65535) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupCount[1]),															LIM_MIN_UINT32(65535) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupCount[2]),															LIM_MIN_UINT32(65535) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupInvocations),														LIM_MIN_UINT32(128) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupSize[0]),															LIM_MIN_UINT32(128) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupSize[1]),															LIM_MIN_UINT32(128) },
		{ PN(checkAlways),								PN(limits.maxComputeWorkGroupSize[2]),															LIM_MIN_UINT32(64) },
		{ PN(checkAlways),								PN(limits.subPixelPrecisionBits),																LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.subTexelPrecisionBits),																LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.mipmapPrecisionBits),																	LIM_MIN_UINT32(4) },
		{ PN(features.fullDrawIndexUint32),				PN(limits.maxDrawIndexedIndexValue),															LIM_MIN_UINT32((deUint32)~0) },
		{ PN(features.multiDrawIndirect),				PN(limits.maxDrawIndirectCount),																LIM_MIN_UINT32(65535) },
		{ PN(checkAlways),								PN(limits.maxSamplerLodBias),																	LIM_MIN_FLOAT(2.0f) },
		{ PN(features.samplerAnisotropy),				PN(limits.maxSamplerAnisotropy),																LIM_MIN_FLOAT(16.0f) },
		{ PN(features.multiViewport),					PN(limits.maxViewports),																		LIM_MIN_UINT32(16) },
		{ PN(checkAlways),								PN(limits.maxViewportDimensions[0]),															LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxViewportDimensions[1]),															LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.viewportBoundsRange[0]),																LIM_MAX_FLOAT(-8192.0f) },
		{ PN(checkAlways),								PN(limits.viewportBoundsRange[1]),																LIM_MIN_FLOAT(8191.0f) },
		{ PN(checkAlways),								PN(limits.viewportSubPixelBits),																LIM_MIN_UINT32(0) },
		{ PN(checkAlways),								PN(limits.minMemoryMapAlignment),																LIM_MIN_UINT32(64) },
		{ PN(checkAlways),								PN(limits.minTexelBufferOffsetAlignment),														LIM_MIN_DEVSIZE(1) },
		{ PN(checkAlways),								PN(limits.minTexelBufferOffsetAlignment),														LIM_MAX_DEVSIZE(256) },
		{ PN(checkAlways),								PN(limits.minUniformBufferOffsetAlignment),														LIM_MIN_DEVSIZE(1) },
		{ PN(checkAlways),								PN(limits.minUniformBufferOffsetAlignment),														LIM_MAX_DEVSIZE(256) },
		{ PN(checkAlways),								PN(limits.minStorageBufferOffsetAlignment),														LIM_MIN_DEVSIZE(1) },
		{ PN(checkAlways),								PN(limits.minStorageBufferOffsetAlignment),														LIM_MAX_DEVSIZE(256) },
		{ PN(checkAlways),								PN(limits.minTexelOffset),																		LIM_MAX_INT32(-8) },
		{ PN(checkAlways),								PN(limits.maxTexelOffset),																		LIM_MIN_INT32(7) },
		{ PN(features.shaderImageGatherExtended),		PN(limits.minTexelGatherOffset),																LIM_MAX_INT32(-8) },
		{ PN(features.shaderImageGatherExtended),		PN(limits.maxTexelGatherOffset),																LIM_MIN_INT32(7) },
		{ PN(features.sampleRateShading),				PN(limits.minInterpolationOffset),																LIM_MAX_FLOAT(-0.5f) },
		{ PN(features.sampleRateShading),				PN(limits.maxInterpolationOffset),																LIM_MIN_FLOAT(0.5f - (1.0f/deFloatPow(2.0f, (float)limits.subPixelInterpolationOffsetBits))) },
		{ PN(features.sampleRateShading),				PN(limits.subPixelInterpolationOffsetBits),														LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.maxFramebufferWidth),																	LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxFramebufferHeight),																LIM_MIN_UINT32(4096) },
		{ PN(checkAlways),								PN(limits.maxFramebufferLayers),																LIM_MIN_UINT32(256) },
		{ PN(checkAlways),								PN(limits.framebufferColorSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkVulkan12Limit),						PN(vulkan12Properties.framebufferIntegerColorSampleCounts),										LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT) },
		{ PN(checkAlways),								PN(limits.framebufferDepthSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.framebufferStencilSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.framebufferNoAttachmentsSampleCounts),												LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.maxColorAttachments),																	LIM_MIN_UINT32(4) },
		{ PN(checkAlways),								PN(limits.sampledImageColorSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.sampledImageIntegerSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT) },
		{ PN(checkAlways),								PN(limits.sampledImageDepthSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.sampledImageStencilSampleCounts),														LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(features.shaderStorageImageMultisample),	PN(limits.storageImageSampleCounts),															LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),								PN(limits.maxSampleMaskWords),																	LIM_MIN_UINT32(1) },
		{ PN(checkAlways),								PN(limits.timestampComputeAndGraphics),															LIM_NONE_UINT32 },
		{ PN(checkAlways),								PN(limits.timestampPeriod),																		LIM_NONE_UINT32 },
		{ PN(features.shaderClipDistance),				PN(limits.maxClipDistances),																	LIM_MIN_UINT32(8) },
		{ PN(features.shaderCullDistance),				PN(limits.maxCullDistances),																	LIM_MIN_UINT32(8) },
		{ PN(features.shaderClipDistance),				PN(limits.maxCombinedClipAndCullDistances),														LIM_MIN_UINT32(8) },
		{ PN(checkAlways),								PN(limits.discreteQueuePriorities),																LIM_MIN_UINT32(2) },
		{ PN(features.largePoints),						PN(limits.pointSizeRange[0]),																	LIM_MIN_FLOAT(0.0f) },
		{ PN(features.largePoints),						PN(limits.pointSizeRange[0]),																	LIM_MAX_FLOAT(1.0f) },
		{ PN(features.largePoints),						PN(limits.pointSizeRange[1]),																	LIM_MIN_FLOAT(64.0f - limits.pointSizeGranularity) },
		{ PN(features.wideLines),						PN(limits.lineWidthRange[0]),																	LIM_MIN_FLOAT(0.0f) },
		{ PN(features.wideLines),						PN(limits.lineWidthRange[0]),																	LIM_MAX_FLOAT(1.0f) },
		{ PN(features.wideLines),						PN(limits.lineWidthRange[1]),																	LIM_MIN_FLOAT(8.0f - limits.lineWidthGranularity) },
		{ PN(features.largePoints),						PN(limits.pointSizeGranularity),																LIM_MIN_FLOAT(0.0f) },
		{ PN(features.largePoints),						PN(limits.pointSizeGranularity),																LIM_MAX_FLOAT(1.0f) },
		{ PN(features.wideLines),						PN(limits.lineWidthGranularity),																LIM_MIN_FLOAT(0.0f) },
		{ PN(features.wideLines),						PN(limits.lineWidthGranularity),																LIM_MAX_FLOAT(1.0f) },
		{ PN(checkAlways),								PN(limits.strictLines),																			LIM_NONE_UINT32 },
		{ PN(checkAlways),								PN(limits.standardSampleLocations),																LIM_NONE_UINT32 },
		{ PN(checkAlways),								PN(limits.optimalBufferCopyOffsetAlignment),													LIM_NONE_DEVSIZE },
		{ PN(checkAlways),								PN(limits.optimalBufferCopyRowPitchAlignment),													LIM_NONE_DEVSIZE },
		{ PN(checkAlways),								PN(limits.nonCoherentAtomSize),																	LIM_MIN_DEVSIZE(1) },
		{ PN(checkAlways),								PN(limits.nonCoherentAtomSize),																	LIM_MAX_DEVSIZE(256) },

		// VK_KHR_multiview
		{ PN(checkVulkan12Limit),						PN(vulkan11Properties.maxMultiviewViewCount),													LIM_MIN_UINT32(6) },
		{ PN(checkVulkan12Limit),						PN(vulkan11Properties.maxMultiviewInstanceIndex),												LIM_MIN_UINT32((1<<27) - 1) },

		// VK_KHR_maintenance3
		{ PN(checkVulkan12Limit),						PN(vulkan11Properties.maxPerSetDescriptors),													LIM_MIN_UINT32(1024) },
		{ PN(checkVulkan12Limit),						PN(vulkan11Properties.maxMemoryAllocationSize),													LIM_MIN_DEVSIZE(1<<30) },

		// VK_EXT_descriptor_indexing
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxUpdateAfterBindDescriptorsInAllPools),									LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers),							LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),						LIM_MIN_UINT32(12) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),						LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages),						LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages),						LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments),					LIM_MIN_UINT32(4) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageUpdateAfterBindResources),										LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers),									LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers),							LIM_MIN_UINT32(shaderStages * 12) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),					LIM_MIN_UINT32(8) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers),							LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),					LIM_MIN_UINT32(4) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages),							LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages),							LIM_MIN_UINT32(500000) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments),							LIM_MIN_UINT32(4) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers),							LIM_MIN_UINT32(limits.maxPerStageDescriptorSamplers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),						LIM_MIN_UINT32(limits.maxPerStageDescriptorUniformBuffers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),						LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageBuffers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages),						LIM_MIN_UINT32(limits.maxPerStageDescriptorSampledImages) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages),						LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageImages) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments),					LIM_MIN_UINT32(limits.maxPerStageDescriptorInputAttachments) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxPerStageUpdateAfterBindResources),										LIM_MIN_UINT32(limits.maxPerStageResources) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers),									LIM_MIN_UINT32(limits.maxDescriptorSetSamplers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers),							LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),					LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffersDynamic) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers),							LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffers) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),					LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffersDynamic) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages),							LIM_MIN_UINT32(limits.maxDescriptorSetSampledImages) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages),							LIM_MIN_UINT32(limits.maxDescriptorSetStorageImages) },
		{ PN(features12.descriptorIndexing),			PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments),							LIM_MIN_UINT32(limits.maxDescriptorSetInputAttachments) },

		// timelineSemaphore
		{ PN(checkVulkan12Limit),						PN(vulkan12Properties.maxTimelineSemaphoreValueDifference),										LIM_MIN_DEVSIZE((1ull<<31) - 1) },
	};

	log << TestLog::Message << limits << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limits.maxFramebufferWidth > limits.maxViewportDimensions[0] ||
		limits.maxFramebufferHeight > limits.maxViewportDimensions[1])
	{
		log << TestLog::Message << "limit validation failed, maxFramebufferDimension of "
			<< "[" << limits.maxFramebufferWidth << ", " << limits.maxFramebufferHeight << "] "
			<< "is larger than maxViewportDimension of "
			<< "[" << limits.maxViewportDimensions[0] << ", " << limits.maxViewportDimensions[1] << "]" << TestLog::EndMessage;
		limitsOk = false;
	}

	if (limits.viewportBoundsRange[0] > float(-2 * limits.maxViewportDimensions[0]))
	{
		log << TestLog::Message << "limit validation failed, viewPortBoundsRange[0] of " << limits.viewportBoundsRange[0]
			<< "is larger than -2*maxViewportDimension[0] of " << -2*limits.maxViewportDimensions[0] << TestLog::EndMessage;
		limitsOk = false;
	}

	if (limits.viewportBoundsRange[1] < float(2 * limits.maxViewportDimensions[1] - 1))
	{
		log << TestLog::Message << "limit validation failed, viewportBoundsRange[1] of " << limits.viewportBoundsRange[1]
			<< "is less than 2*maxViewportDimension[1] of " << 2*limits.maxViewportDimensions[1] << TestLog::EndMessage;
		limitsOk = false;
	}

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportKhrPushDescriptor (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_push_descriptor");
}

tcu::TestStatus validateLimitsKhrPushDescriptor (Context& context)
{
	const VkBool32										checkAlways					= VK_TRUE;
	const VkPhysicalDevicePushDescriptorPropertiesKHR&	pushDescriptorPropertiesKHR	= context.getPushDescriptorProperties();
	TestLog&											log							= context.getTestContext().getLog();
	bool												limitsOk					= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(pushDescriptorPropertiesKHR.maxPushDescriptors),	LIM_MIN_UINT32(32) },
	};

	log << TestLog::Message << pushDescriptorPropertiesKHR << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportKhrMultiview (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_multiview");
}

tcu::TestStatus validateLimitsKhrMultiview (Context& context)
{
	const VkBool32								checkAlways			= VK_TRUE;
	const VkPhysicalDeviceMultiviewProperties&	multiviewProperties	= context.getMultiviewProperties();
	TestLog&									log					= context.getTestContext().getLog();
	bool										limitsOk			= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		// VK_KHR_multiview
		{ PN(checkAlways),	PN(multiviewProperties.maxMultiviewViewCount),		LIM_MIN_UINT32(6) },
		{ PN(checkAlways),	PN(multiviewProperties.maxMultiviewInstanceIndex),	LIM_MIN_UINT32((1<<27) - 1) },
	};

	log << TestLog::Message << multiviewProperties << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtDiscardRectangles (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_discard_rectangles");
}

tcu::TestStatus validateLimitsExtDiscardRectangles (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceDiscardRectanglePropertiesEXT&	discardRectanglePropertiesEXT	= context.getDiscardRectanglePropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(discardRectanglePropertiesEXT.maxDiscardRectangles),	LIM_MIN_UINT32(4) },
	};

	log << TestLog::Message << discardRectanglePropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtSampleLocations (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_sample_locations");
}

tcu::TestStatus validateLimitsExtSampleLocations (Context& context)
{
	const VkBool32										checkAlways						= VK_TRUE;
	const VkPhysicalDeviceSampleLocationsPropertiesEXT&	sampleLocationsPropertiesEXT	= context.getSampleLocationsPropertiesEXT();
	TestLog&											log								= context.getTestContext().getLog();
	bool												limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.sampleLocationSampleCounts),		LIM_MIN_BITI32(VK_SAMPLE_COUNT_4_BIT) },
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.maxSampleLocationGridSize.width),	LIM_MIN_FLOAT(0.0f) },
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.maxSampleLocationGridSize.height),	LIM_MIN_FLOAT(0.0f) },
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.sampleLocationCoordinateRange[0]),	LIM_MAX_FLOAT(0.0f) },
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.sampleLocationCoordinateRange[1]),	LIM_MIN_FLOAT(0.9375f) },
		{ PN(checkAlways),	PN(sampleLocationsPropertiesEXT.sampleLocationSubPixelBits),		LIM_MIN_UINT32(4) },
	};

	log << TestLog::Message << sampleLocationsPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtExternalMemoryHost (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_external_memory_host");
}

tcu::TestStatus validateLimitsExtExternalMemoryHost (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceExternalMemoryHostPropertiesEXT&	externalMemoryHostPropertiesEXT	= context.getExternalMemoryHostPropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(externalMemoryHostPropertiesEXT.minImportedHostPointerAlignment),	LIM_MAX_DEVSIZE(65536) },
	};

	log << TestLog::Message << externalMemoryHostPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtBlendOperationAdvanced (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_blend_operation_advanced");
}

tcu::TestStatus validateLimitsExtBlendOperationAdvanced (Context& context)
{
	const VkBool32												checkAlways							= VK_TRUE;
	const VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT&	blendOperationAdvancedPropertiesEXT	= context.getBlendOperationAdvancedPropertiesEXT();
	TestLog&													log									= context.getTestContext().getLog();
	bool														limitsOk							= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(blendOperationAdvancedPropertiesEXT.advancedBlendMaxColorAttachments),	LIM_MIN_UINT32(1) },
	};

	log << TestLog::Message << blendOperationAdvancedPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportKhrMaintenance3 (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_maintenance3");
}

tcu::TestStatus validateLimitsKhrMaintenance3 (Context& context)
{
	const VkBool32									checkAlways				= VK_TRUE;
	const VkPhysicalDeviceMaintenance3Properties&	maintenance3Properties	= context.getMaintenance3Properties();
	TestLog&										log						= context.getTestContext().getLog();
	bool											limitsOk				= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(maintenance3Properties.maxPerSetDescriptors),	LIM_MIN_UINT32(1024) },
		{ PN(checkAlways),	PN(maintenance3Properties.maxMemoryAllocationSize),	LIM_MIN_DEVSIZE(1<<30) },
	};

	log << TestLog::Message << maintenance3Properties << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtConservativeRasterization (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");
}

tcu::TestStatus validateLimitsExtConservativeRasterization (Context& context)
{
	const VkBool32													checkAlways								= VK_TRUE;
	const VkPhysicalDeviceConservativeRasterizationPropertiesEXT&	conservativeRasterizationPropertiesEXT	= context.getConservativeRasterizationPropertiesEXT();
	TestLog&														log										= context.getTestContext().getLog();
	bool															limitsOk								= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(conservativeRasterizationPropertiesEXT.primitiveOverestimationSize),					LIM_MIN_FLOAT(0.0f) },
		{ PN(checkAlways),	PN(conservativeRasterizationPropertiesEXT.maxExtraPrimitiveOverestimationSize),			LIM_MIN_FLOAT(0.0f) },
		{ PN(checkAlways),	PN(conservativeRasterizationPropertiesEXT.extraPrimitiveOverestimationSizeGranularity),	LIM_MIN_FLOAT(0.0f) },
	};

	log << TestLog::Message << conservativeRasterizationPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtDescriptorIndexing (Context& context)
{
	const std::string&							requiredDeviceExtension		= "VK_EXT_descriptor_indexing";
	const VkPhysicalDevice						physicalDevice				= context.getPhysicalDevice();
	const InstanceInterface&					vki							= context.getInstanceInterface();
	const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	if (!isExtensionSupported(deviceExtensionProperties, RequiredExtension(requiredDeviceExtension)))
		TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");

	// Extension string is present, then extension is really supported and should have been added into chain in DefaultDevice properties and features
}

tcu::TestStatus validateLimitsExtDescriptorIndexing (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceProperties2&						properties2						= context.getDeviceProperties2();
	const VkPhysicalDeviceLimits&							limits							= properties2.properties.limits;
	const VkPhysicalDeviceDescriptorIndexingPropertiesEXT&	descriptorIndexingPropertiesEXT	= context.getDescriptorIndexingProperties();
	const VkPhysicalDeviceFeatures&							features						= context.getDeviceFeatures();
	const deUint32											tessellationShaderCount			= (features.tessellationShader) ? 2 : 0;
	const deUint32											geometryShaderCount				= (features.geometryShader) ? 1 : 0;
	const deUint32											shaderStages					= 3 + tessellationShaderCount + geometryShaderCount;
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxUpdateAfterBindDescriptorsInAllPools),				LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindSamplers),			LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindUniformBuffers),		LIM_MIN_UINT32(12) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindStorageBuffers),		LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindSampledImages),		LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindStorageImages),		LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindInputAttachments),	LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageUpdateAfterBindResources),					LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindSamplers),				LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindUniformBuffers),			LIM_MIN_UINT32(shaderStages * 12) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),	LIM_MIN_UINT32(8) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageBuffers),			LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),	LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindSampledImages),			LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageImages),			LIM_MIN_UINT32(500000) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindInputAttachments),		LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindSamplers),			LIM_MIN_UINT32(limits.maxPerStageDescriptorSamplers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindUniformBuffers),		LIM_MIN_UINT32(limits.maxPerStageDescriptorUniformBuffers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindStorageBuffers),		LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageBuffers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindSampledImages),		LIM_MIN_UINT32(limits.maxPerStageDescriptorSampledImages) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindStorageImages),		LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageImages) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageDescriptorUpdateAfterBindInputAttachments),	LIM_MIN_UINT32(limits.maxPerStageDescriptorInputAttachments) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxPerStageUpdateAfterBindResources),					LIM_MIN_UINT32(limits.maxPerStageResources) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindSamplers),				LIM_MIN_UINT32(limits.maxDescriptorSetSamplers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindUniformBuffers),			LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),	LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffersDynamic) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageBuffers),			LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffers) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),	LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffersDynamic) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindSampledImages),			LIM_MIN_UINT32(limits.maxDescriptorSetSampledImages) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindStorageImages),			LIM_MIN_UINT32(limits.maxDescriptorSetStorageImages) },
		{ PN(checkAlways),	PN(descriptorIndexingPropertiesEXT.maxDescriptorSetUpdateAfterBindInputAttachments),		LIM_MIN_UINT32(limits.maxDescriptorSetInputAttachments) },
	};

	log << TestLog::Message << descriptorIndexingPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtInlineUniformBlock (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
}

tcu::TestStatus validateLimitsExtInlineUniformBlock (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceInlineUniformBlockPropertiesEXT&	inlineUniformBlockPropertiesEXT	= context.getInlineUniformBlockPropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(inlineUniformBlockPropertiesEXT.maxInlineUniformBlockSize),									LIM_MIN_UINT32(256) },
		{ PN(checkAlways),	PN(inlineUniformBlockPropertiesEXT.maxPerStageDescriptorInlineUniformBlocks),					LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(inlineUniformBlockPropertiesEXT.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks),	LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(inlineUniformBlockPropertiesEXT.maxDescriptorSetInlineUniformBlocks),						LIM_MIN_UINT32(4) },
		{ PN(checkAlways),	PN(inlineUniformBlockPropertiesEXT.maxDescriptorSetUpdateAfterBindInlineUniformBlocks),			LIM_MIN_UINT32(4) },
	};

	log << TestLog::Message << inlineUniformBlockPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtVertexAttributeDivisor (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_vertex_attribute_divisor");
}

tcu::TestStatus validateLimitsExtVertexAttributeDivisor (Context& context)
{
	const VkBool32												checkAlways							= VK_TRUE;
	const VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT&	vertexAttributeDivisorPropertiesEXT	= context.getVertexAttributeDivisorPropertiesEXT();
	TestLog&													log									= context.getTestContext().getLog();
	bool														limitsOk							= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(vertexAttributeDivisorPropertiesEXT.maxVertexAttribDivisor),	LIM_MIN_UINT32((1<<16) - 1) },
	};

	log << TestLog::Message << vertexAttributeDivisorPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportNvMeshShader (Context& context)
{
	const std::string&							requiredDeviceExtension		= "VK_NV_mesh_shader";
	const VkPhysicalDevice						physicalDevice				= context.getPhysicalDevice();
	const InstanceInterface&					vki							= context.getInstanceInterface();
	const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	if (!isExtensionSupported(deviceExtensionProperties, RequiredExtension(requiredDeviceExtension)))
		TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");
}

tcu::TestStatus validateLimitsNvMeshShader (Context& context)
{
	const VkBool32							checkAlways				= VK_TRUE;
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const InstanceInterface&				vki						= context.getInstanceInterface();
	TestLog&								log						= context.getTestContext().getLog();
	bool									limitsOk				= true;
	VkPhysicalDeviceMeshShaderPropertiesNV	meshShaderPropertiesNV	= initVulkanStructure();
	VkPhysicalDeviceProperties2				properties2				= initVulkanStructure(&meshShaderPropertiesNV);

	vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxDrawMeshTasksCount),		LIM_MIN_UINT32(deUint32((1ull<<16) - 1)) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskWorkGroupInvocations),	LIM_MIN_UINT32(32) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[0]),		LIM_MIN_UINT32(32) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[1]),		LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[2]),		LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskTotalMemorySize),		LIM_MIN_UINT32(16384) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxTaskOutputCount),			LIM_MIN_UINT32((1<<16) - 1) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshWorkGroupInvocations),	LIM_MIN_UINT32(32) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[0]),		LIM_MIN_UINT32(32) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[1]),		LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[2]),		LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshTotalMemorySize),		LIM_MIN_UINT32(16384) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshOutputVertices),		LIM_MIN_UINT32(256) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshOutputPrimitives),		LIM_MIN_UINT32(256) },
		{ PN(checkAlways),	PN(meshShaderPropertiesNV.maxMeshMultiviewViewCount),	LIM_MIN_UINT32(1) },
	};

	log << TestLog::Message << meshShaderPropertiesNV << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtTransformFeedback (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_transform_feedback");
}

tcu::TestStatus validateLimitsExtTransformFeedback (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceTransformFeedbackPropertiesEXT&	transformFeedbackPropertiesEXT	= context.getTransformFeedbackPropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackStreams),				LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBuffers),				LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferSize),			LIM_MIN_DEVSIZE(1ull<<27) },
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackStreamDataSize),		LIM_MIN_UINT32(512) },
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferDataSize),		LIM_MIN_UINT32(512) },
		{ PN(checkAlways),	PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferDataStride),	LIM_MIN_UINT32(512) },
	};

	log << TestLog::Message << transformFeedbackPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtFragmentDensityMap (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_fragment_density_map");
}

tcu::TestStatus validateLimitsExtFragmentDensityMap (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceFragmentDensityMapPropertiesEXT&	fragmentDensityMapPropertiesEXT	= context.getFragmentDensityMapPropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(fragmentDensityMapPropertiesEXT.minFragmentDensityTexelSize.width),							LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(fragmentDensityMapPropertiesEXT.minFragmentDensityTexelSize.height),							LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(fragmentDensityMapPropertiesEXT.maxFragmentDensityTexelSize.width),							LIM_MIN_UINT32(1) },
		{ PN(checkAlways),	PN(fragmentDensityMapPropertiesEXT.maxFragmentDensityTexelSize.height),							LIM_MIN_UINT32(1) },
	};

	log << TestLog::Message << fragmentDensityMapPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportNvRayTracing (Context& context)
{
	const std::string&							requiredDeviceExtension		= "VK_NV_ray_tracing";
	const VkPhysicalDevice						physicalDevice				= context.getPhysicalDevice();
	const InstanceInterface&					vki							= context.getInstanceInterface();
	const std::vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	if (!isExtensionSupported(deviceExtensionProperties, RequiredExtension(requiredDeviceExtension)))
		TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");
}

tcu::TestStatus validateLimitsNvRayTracing (Context& context)
{
	const VkBool32							checkAlways				= VK_TRUE;
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const InstanceInterface&				vki						= context.getInstanceInterface();
	TestLog&								log						= context.getTestContext().getLog();
	bool									limitsOk				= true;
	VkPhysicalDeviceRayTracingPropertiesNV	rayTracingPropertiesNV	= initVulkanStructure();
	VkPhysicalDeviceProperties2				properties2				= initVulkanStructure(&rayTracingPropertiesNV);

	vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.shaderGroupHandleSize),					LIM_MIN_UINT32(16) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.maxRecursionDepth),						LIM_MIN_UINT32(31) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.shaderGroupBaseAlignment),				LIM_MIN_UINT32(64) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.maxGeometryCount),						LIM_MIN_UINT32((1<<24) - 1) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.maxInstanceCount),						LIM_MIN_UINT32((1<<24) - 1) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.maxTriangleCount),						LIM_MIN_UINT32((1<<29) - 1) },
		{ PN(checkAlways),	PN(rayTracingPropertiesNV.maxDescriptorSetAccelerationStructures),	LIM_MIN_UINT32(16) },
	};

	log << TestLog::Message << rayTracingPropertiesNV << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportKhrTimelineSemaphore (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
}

tcu::TestStatus validateLimitsKhrTimelineSemaphore (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceTimelineSemaphorePropertiesKHR&	timelineSemaphorePropertiesKHR	= context.getTimelineSemaphoreProperties();
	bool													limitsOk						= true;
	TestLog&												log								= context.getTestContext().getLog();

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(timelineSemaphorePropertiesKHR.maxTimelineSemaphoreValueDifference),	LIM_MIN_DEVSIZE((1ull<<31) - 1) },
	};

	log << TestLog::Message << timelineSemaphorePropertiesKHR << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportExtLineRasterization (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_line_rasterization");
}

tcu::TestStatus validateLimitsExtLineRasterization (Context& context)
{
	const VkBool32											checkAlways						= VK_TRUE;
	const VkPhysicalDeviceLineRasterizationPropertiesEXT&	lineRasterizationPropertiesEXT	= context.getLineRasterizationPropertiesEXT();
	TestLog&												log								= context.getTestContext().getLog();
	bool													limitsOk						= true;

	FeatureLimitTableItem featureLimitTable[] =
	{
		{ PN(checkAlways),	PN(lineRasterizationPropertiesEXT.lineSubPixelPrecisionBits),	LIM_MIN_UINT32(4) },
	};

	log << TestLog::Message << lineRasterizationPropertiesEXT << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
		limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

	if (limitsOk)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

void checkSupportFeatureBitInfluence (Context& context)
{
	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");
}

void createTestDevice (Context& context, void* pNext, const char* const* ppEnabledExtensionNames, deUint32 enabledExtensionCount)
{
	const PlatformInterface&				platformInterface		= context.getPlatformInterface();
	const auto								validationEnabled		= context.getTestContext().getCommandLine().isValidationEnabled();
	const Unique<VkInstance>				instance				(createDefaultInstance(platformInterface, context.getUsedApiVersion()));
	const InstanceDriver					instanceDriver			(platformInterface, instance.get());
	const VkPhysicalDevice					physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const deUint32							queueFamilyIndex		= 0;
	const deUint32							queueCount				= 1;
	const deUint32							queueIndex				= 0;
	const float								queuePriority			= 1.0f;
	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
	const VkDeviceQueueCreateInfo			deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//  VkStructureType				sType;
		DE_NULL,									//  const void*					pNext;
		(VkDeviceQueueCreateFlags)0u,				//  VkDeviceQueueCreateFlags	flags;
		queueFamilyIndex,							//  deUint32					queueFamilyIndex;
		queueCount,									//  deUint32					queueCount;
		&queuePriority,								//  const float*				pQueuePriorities;
	};
	const VkDeviceCreateInfo				deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		//  VkStructureType					sType;
		pNext,										//  const void*						pNext;
		(VkDeviceCreateFlags)0u,					//  VkDeviceCreateFlags				flags;
		1,											//  deUint32						queueCreateInfoCount;
		&deviceQueueCreateInfo,						//  const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0,											//  deUint32						enabledLayerCount;
		DE_NULL,									//  const char* const*				ppEnabledLayerNames;
		enabledExtensionCount,						//  deUint32						enabledExtensionCount;
		ppEnabledExtensionNames,					//  const char* const*				ppEnabledExtensionNames;
		DE_NULL,									//  const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};
	const Unique<VkDevice>					device					(createCustomDevice(validationEnabled, platformInterface, *instance, instanceDriver, physicalDevice, &deviceCreateInfo));
	const DeviceDriver						deviceDriver			(platformInterface, instance.get(), device.get());
	const VkQueue							queue					= getDeviceQueue(deviceDriver, *device,  queueFamilyIndex, queueIndex);

	VK_CHECK(deviceDriver.queueWaitIdle(queue));
}

void cleanVulkanStruct (void* structPtr, size_t structSize)
{
	struct StructureBase
	{
		VkStructureType		sType;
		void*				pNext;
	};

	VkStructureType		sType = ((StructureBase*)structPtr)->sType;

	deMemset(structPtr, 0, structSize);

	((StructureBase*)structPtr)->sType = sType;
}

tcu::TestStatus featureBitInfluenceOnDeviceCreate (Context& context)
{
#define FEATURE_TABLE_ITEM(CORE, EXT, FIELD, STR) { &(CORE), sizeof(CORE), &(CORE.FIELD), #CORE "." #FIELD, &(EXT), sizeof(EXT), &(EXT.FIELD), #EXT "." #FIELD, STR }
#define DEPENDENCY_DUAL_ITEM(CORE, EXT, FIELD, PARENT) { &(CORE.FIELD), &(CORE.PARENT) }, { &(EXT.FIELD), &(EXT.PARENT) }
#define DEPENDENCY_SINGLE_ITEM(CORE, FIELD, PARENT) { &(CORE.FIELD), &(CORE.PARENT) }

	const VkPhysicalDevice								physicalDevice						= context.getPhysicalDevice();
	const InstanceInterface&							vki									= context.getInstanceInterface();
	TestLog&											log									= context.getTestContext().getLog();
	const std::vector<VkExtensionProperties>			deviceExtensionProperties			= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	VkPhysicalDeviceFeatures2							features2							= initVulkanStructure();
	VkPhysicalDeviceVulkan11Features					vulkan11Features					= initVulkanStructure();
	VkPhysicalDeviceVulkan12Features					vulkan12Features					= initVulkanStructure();
	VkPhysicalDevice16BitStorageFeaturesKHR				sixteenBitStorageFeatures			= initVulkanStructure();
	VkPhysicalDeviceMultiviewFeatures					multiviewFeatures					= initVulkanStructure();
	VkPhysicalDeviceVariablePointersFeatures			variablePointersFeatures			= initVulkanStructure();
	VkPhysicalDeviceProtectedMemoryFeatures				protectedMemoryFeatures				= initVulkanStructure();
	VkPhysicalDeviceSamplerYcbcrConversionFeatures		samplerYcbcrConversionFeatures		= initVulkanStructure();
	VkPhysicalDeviceShaderDrawParametersFeatures		shaderDrawParametersFeatures		= initVulkanStructure();
	VkPhysicalDevice8BitStorageFeatures					eightBitStorageFeatures				= initVulkanStructure();
	VkPhysicalDeviceShaderAtomicInt64Features			shaderAtomicInt64Features			= initVulkanStructure();
	VkPhysicalDeviceShaderFloat16Int8Features			shaderFloat16Int8Features			= initVulkanStructure();
	VkPhysicalDeviceDescriptorIndexingFeatures			descriptorIndexingFeatures			= initVulkanStructure();
	VkPhysicalDeviceScalarBlockLayoutFeatures			scalarBlockLayoutFeatures			= initVulkanStructure();
	VkPhysicalDeviceImagelessFramebufferFeatures		imagelessFramebufferFeatures		= initVulkanStructure();
	VkPhysicalDeviceUniformBufferStandardLayoutFeatures	uniformBufferStandardLayoutFeatures	= initVulkanStructure();
	VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures	shaderSubgroupExtendedTypesFeatures	= initVulkanStructure();
	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures	separateDepthStencilLayoutsFeatures	= initVulkanStructure();
	VkPhysicalDeviceHostQueryResetFeatures				hostQueryResetFeatures				= initVulkanStructure();
	VkPhysicalDeviceTimelineSemaphoreFeatures			timelineSemaphoreFeatures			= initVulkanStructure();
	VkPhysicalDeviceBufferDeviceAddressFeatures			bufferDeviceAddressFeatures			= initVulkanStructure();
	VkPhysicalDeviceVulkanMemoryModelFeatures			vulkanMemoryModelFeatures			= initVulkanStructure();

	struct UnusedExtensionFeatures
	{
		VkStructureType		sType;
		void*				pNext;
		VkBool32			descriptorIndexing;
		VkBool32			samplerFilterMinmax;
	} unusedExtensionFeatures;

	struct FeatureTable
	{
		void*		coreStructPtr;
		size_t		coreStructSize;
		VkBool32*	coreFieldPtr;
		const char*	coreFieldName;
		void*		extStructPtr;
		size_t		extStructSize;
		VkBool32*	extFieldPtr;
		const char*	extFieldName;
		const char*	extString;
	}
	featureTable[] =
	{
		FEATURE_TABLE_ITEM(vulkan11Features,	sixteenBitStorageFeatures,				storageBuffer16BitAccess,							"VK_KHR_16bit_storage"),
		FEATURE_TABLE_ITEM(vulkan11Features,	sixteenBitStorageFeatures,				uniformAndStorageBuffer16BitAccess,					"VK_KHR_16bit_storage"),
		FEATURE_TABLE_ITEM(vulkan11Features,	sixteenBitStorageFeatures,				storagePushConstant16,								"VK_KHR_16bit_storage"),
		FEATURE_TABLE_ITEM(vulkan11Features,	sixteenBitStorageFeatures,				storageInputOutput16,								"VK_KHR_16bit_storage"),
		FEATURE_TABLE_ITEM(vulkan11Features,	multiviewFeatures,						multiview,											"VK_KHR_multiview"),
		FEATURE_TABLE_ITEM(vulkan11Features,	multiviewFeatures,						multiviewGeometryShader,							"VK_KHR_multiview"),
		FEATURE_TABLE_ITEM(vulkan11Features,	multiviewFeatures,						multiviewTessellationShader,						"VK_KHR_multiview"),
		FEATURE_TABLE_ITEM(vulkan11Features,	variablePointersFeatures,				variablePointersStorageBuffer,						"VK_KHR_variable_pointers"),
		FEATURE_TABLE_ITEM(vulkan11Features,	variablePointersFeatures,				variablePointers,									"VK_KHR_variable_pointers"),
		FEATURE_TABLE_ITEM(vulkan11Features,	protectedMemoryFeatures,				protectedMemory,									DE_NULL),
		FEATURE_TABLE_ITEM(vulkan11Features,	samplerYcbcrConversionFeatures,			samplerYcbcrConversion,								"VK_KHR_sampler_ycbcr_conversion"),
		FEATURE_TABLE_ITEM(vulkan11Features,	shaderDrawParametersFeatures,			shaderDrawParameters,								DE_NULL),
		FEATURE_TABLE_ITEM(vulkan12Features,	eightBitStorageFeatures,				storageBuffer8BitAccess,							"VK_KHR_8bit_storage"),
		FEATURE_TABLE_ITEM(vulkan12Features,	eightBitStorageFeatures,				uniformAndStorageBuffer8BitAccess,					"VK_KHR_8bit_storage"),
		FEATURE_TABLE_ITEM(vulkan12Features,	eightBitStorageFeatures,				storagePushConstant8,								"VK_KHR_8bit_storage"),
		FEATURE_TABLE_ITEM(vulkan12Features,	shaderAtomicInt64Features,				shaderBufferInt64Atomics,							"VK_KHR_shader_atomic_int64"),
		FEATURE_TABLE_ITEM(vulkan12Features,	shaderAtomicInt64Features,				shaderSharedInt64Atomics,							"VK_KHR_shader_atomic_int64"),
		FEATURE_TABLE_ITEM(vulkan12Features,	shaderFloat16Int8Features,				shaderFloat16,										"VK_KHR_shader_float16_int8"),
		FEATURE_TABLE_ITEM(vulkan12Features,	shaderFloat16Int8Features,				shaderInt8,											"VK_KHR_shader_float16_int8"),
		FEATURE_TABLE_ITEM(vulkan12Features,	unusedExtensionFeatures,				descriptorIndexing,									DE_NULL),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderInputAttachmentArrayDynamicIndexing,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderUniformTexelBufferArrayDynamicIndexing,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderStorageTexelBufferArrayDynamicIndexing,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderUniformBufferArrayNonUniformIndexing,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderSampledImageArrayNonUniformIndexing,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderStorageBufferArrayNonUniformIndexing,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderStorageImageArrayNonUniformIndexing,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderInputAttachmentArrayNonUniformIndexing,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderUniformTexelBufferArrayNonUniformIndexing,	"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				shaderStorageTexelBufferArrayNonUniformIndexing,	"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingUniformBufferUpdateAfterBind,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingSampledImageUpdateAfterBind,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingStorageImageUpdateAfterBind,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingStorageBufferUpdateAfterBind,		"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingUniformTexelBufferUpdateAfterBind,	"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingStorageTexelBufferUpdateAfterBind,	"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingUpdateUnusedWhilePending,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingPartiallyBound,					"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				descriptorBindingVariableDescriptorCount,			"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	descriptorIndexingFeatures,				runtimeDescriptorArray,								"VK_EXT_descriptor_indexing"),
		FEATURE_TABLE_ITEM(vulkan12Features,	unusedExtensionFeatures,				samplerFilterMinmax,								"VK_EXT_sampler_filter_minmax"),
		FEATURE_TABLE_ITEM(vulkan12Features,	scalarBlockLayoutFeatures,				scalarBlockLayout,									"VK_EXT_scalar_block_layout"),
		FEATURE_TABLE_ITEM(vulkan12Features,	imagelessFramebufferFeatures,			imagelessFramebuffer,								"VK_KHR_imageless_framebuffer"),
		FEATURE_TABLE_ITEM(vulkan12Features,	uniformBufferStandardLayoutFeatures,	uniformBufferStandardLayout,						"VK_KHR_uniform_buffer_standard_layout"),
		FEATURE_TABLE_ITEM(vulkan12Features,	shaderSubgroupExtendedTypesFeatures,	shaderSubgroupExtendedTypes,						"VK_KHR_shader_subgroup_extended_types"),
		FEATURE_TABLE_ITEM(vulkan12Features,	separateDepthStencilLayoutsFeatures,	separateDepthStencilLayouts,						"VK_KHR_separate_depth_stencil_layouts"),
		FEATURE_TABLE_ITEM(vulkan12Features,	hostQueryResetFeatures,					hostQueryReset,										"VK_EXT_host_query_reset"),
		FEATURE_TABLE_ITEM(vulkan12Features,	timelineSemaphoreFeatures,				timelineSemaphore,									"VK_KHR_timeline_semaphore"),
		FEATURE_TABLE_ITEM(vulkan12Features,	bufferDeviceAddressFeatures,			bufferDeviceAddress,								"VK_EXT_buffer_device_address"),
		FEATURE_TABLE_ITEM(vulkan12Features,	bufferDeviceAddressFeatures,			bufferDeviceAddressCaptureReplay,					"VK_EXT_buffer_device_address"),
		FEATURE_TABLE_ITEM(vulkan12Features,	bufferDeviceAddressFeatures,			bufferDeviceAddressMultiDevice,						"VK_EXT_buffer_device_address"),
		FEATURE_TABLE_ITEM(vulkan12Features,	vulkanMemoryModelFeatures,				vulkanMemoryModel,									"VK_KHR_vulkan_memory_model"),
		FEATURE_TABLE_ITEM(vulkan12Features,	vulkanMemoryModelFeatures,				vulkanMemoryModelDeviceScope,						"VK_KHR_vulkan_memory_model"),
		FEATURE_TABLE_ITEM(vulkan12Features,	vulkanMemoryModelFeatures,				vulkanMemoryModelAvailabilityVisibilityChains,		"VK_KHR_vulkan_memory_model"),
	};
	struct FeatureDependencyTable
	{
		VkBool32*	featurePtr;
		VkBool32*	dependOnPtr;
	}
	featureDependencyTable[] =
	{
		DEPENDENCY_DUAL_ITEM	(vulkan11Features,	multiviewFeatures,				multiviewGeometryShader,							multiview),
		DEPENDENCY_DUAL_ITEM	(vulkan11Features,	multiviewFeatures,				multiviewTessellationShader,						multiview),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderInputAttachmentArrayDynamicIndexing,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderUniformTexelBufferArrayDynamicIndexing,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderStorageTexelBufferArrayDynamicIndexing,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderUniformBufferArrayNonUniformIndexing,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderSampledImageArrayNonUniformIndexing,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderStorageBufferArrayNonUniformIndexing,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderStorageImageArrayNonUniformIndexing,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderInputAttachmentArrayNonUniformIndexing,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderUniformTexelBufferArrayNonUniformIndexing,	descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									shaderStorageTexelBufferArrayNonUniformIndexing,	descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingUniformBufferUpdateAfterBind,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingSampledImageUpdateAfterBind,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingStorageImageUpdateAfterBind,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingStorageBufferUpdateAfterBind,		descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingUniformTexelBufferUpdateAfterBind,	descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingStorageTexelBufferUpdateAfterBind,	descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingUpdateUnusedWhilePending,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingPartiallyBound,					descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									descriptorBindingVariableDescriptorCount,			descriptorIndexing),
		DEPENDENCY_SINGLE_ITEM	(vulkan12Features,									runtimeDescriptorArray,								descriptorIndexing),
		DEPENDENCY_DUAL_ITEM	(vulkan12Features,	bufferDeviceAddressFeatures,	bufferDeviceAddressCaptureReplay,					bufferDeviceAddress),
		DEPENDENCY_DUAL_ITEM	(vulkan12Features,	bufferDeviceAddressFeatures,	bufferDeviceAddressMultiDevice,						bufferDeviceAddress),
		DEPENDENCY_DUAL_ITEM	(vulkan12Features,	vulkanMemoryModelFeatures,		vulkanMemoryModelDeviceScope,						vulkanMemoryModel),
		DEPENDENCY_DUAL_ITEM	(vulkan12Features,	vulkanMemoryModelFeatures,		vulkanMemoryModelAvailabilityVisibilityChains,		vulkanMemoryModel),
	};

	deMemset(&unusedExtensionFeatures, 0, sizeof(unusedExtensionFeatures));

	for (size_t featureTableNdx = 0; featureTableNdx < DE_LENGTH_OF_ARRAY(featureTable); ++featureTableNdx)
	{
		FeatureTable&	testedFeature	= featureTable[featureTableNdx];
		VkBool32		coreFeatureState= DE_FALSE;
		VkBool32		extFeatureState	= DE_FALSE;

		// Core test
		{
			void*		structPtr	= testedFeature.coreStructPtr;
			size_t		structSize	= testedFeature.coreStructSize;
			VkBool32*	featurePtr	= testedFeature.coreFieldPtr;

			if (structPtr != &unusedExtensionFeatures)
				features2.pNext	= structPtr;

			vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

			coreFeatureState = featurePtr[0];

			log << TestLog::Message
				<< "Feature status "
				<< testedFeature.coreFieldName << "=" << coreFeatureState
				<< TestLog::EndMessage;

			if (coreFeatureState)
			{
				cleanVulkanStruct(structPtr, structSize);

				featurePtr[0] = DE_TRUE;

				for (size_t featureDependencyTableNdx = 0; featureDependencyTableNdx < DE_LENGTH_OF_ARRAY(featureDependencyTable); ++featureDependencyTableNdx)
					if (featureDependencyTable[featureDependencyTableNdx].featurePtr == featurePtr)
						featureDependencyTable[featureDependencyTableNdx].dependOnPtr[0] = DE_TRUE;

				createTestDevice(context, &features2, DE_NULL, 0u);
			}
		}

		// ext test
		{
			void*		structPtr		= testedFeature.extStructPtr;
			size_t		structSize		= testedFeature.extStructSize;
			VkBool32*	featurePtr		= testedFeature.extFieldPtr;
			const char*	extStringPtr	= testedFeature.extString;

			if (structPtr != &unusedExtensionFeatures)
				features2.pNext	= structPtr;

			if (extStringPtr == DE_NULL || isExtensionSupported(deviceExtensionProperties, RequiredExtension(extStringPtr)))
			{
				vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

				extFeatureState = *featurePtr;

				log << TestLog::Message
					<< "Feature status "
					<< testedFeature.extFieldName << "=" << extFeatureState
					<< TestLog::EndMessage;

				if (extFeatureState)
				{
					cleanVulkanStruct(structPtr, structSize);

					featurePtr[0] = DE_TRUE;

					for (size_t featureDependencyTableNdx = 0; featureDependencyTableNdx < DE_LENGTH_OF_ARRAY(featureDependencyTable); ++featureDependencyTableNdx)
						if (featureDependencyTable[featureDependencyTableNdx].featurePtr == featurePtr)
							featureDependencyTable[featureDependencyTableNdx].dependOnPtr[0] = DE_TRUE;

					createTestDevice(context, &features2, &extStringPtr, (extStringPtr == DE_NULL) ? 0u : 1u );
				}
			}
		}
	}

	return tcu::TestStatus::pass("pass");
}

template<typename T>
class CheckIncompleteResult
{
public:
	virtual			~CheckIncompleteResult	(void) {}
	virtual void	getResult				(Context& context, T* data) = 0;

	void operator() (Context& context, tcu::ResultCollector& results, const std::size_t expectedCompleteSize)
	{
		if (expectedCompleteSize == 0)
			return;

		vector<T>		outputData	(expectedCompleteSize);
		const deUint32	usedSize	= static_cast<deUint32>(expectedCompleteSize / 3);

		ValidateQueryBits::fillBits(outputData.begin(), outputData.end());	// unused entries should have this pattern intact
		m_count		= usedSize;
		m_result	= VK_SUCCESS;

		getResult(context, &outputData[0]);									// update m_count and m_result

		if (m_count != usedSize || m_result != VK_INCOMPLETE || !ValidateQueryBits::checkBits(outputData.begin() + m_count, outputData.end()))
			results.fail("Query didn't return VK_INCOMPLETE");
	}

protected:
	deUint32	m_count;
	VkResult	m_result;
};

struct CheckEnumeratePhysicalDevicesIncompleteResult : public CheckIncompleteResult<VkPhysicalDevice>
{
	void getResult (Context& context, VkPhysicalDevice* data)
	{
		m_result = context.getInstanceInterface().enumeratePhysicalDevices(context.getInstance(), &m_count, data);
	}
};

struct CheckEnumeratePhysicalDeviceGroupsIncompleteResult : public CheckIncompleteResult<VkPhysicalDeviceGroupProperties>
{
	void getResult (Context& context, VkPhysicalDeviceGroupProperties* data)
	{
		m_result = context.getInstanceInterface().enumeratePhysicalDeviceGroups(context.getInstance(), &m_count, data);
	}
};

struct CheckEnumerateInstanceLayerPropertiesIncompleteResult : public CheckIncompleteResult<VkLayerProperties>
{
	void getResult (Context& context, VkLayerProperties* data)
	{
		m_result = context.getPlatformInterface().enumerateInstanceLayerProperties(&m_count, data);
	}
};

struct CheckEnumerateDeviceLayerPropertiesIncompleteResult : public CheckIncompleteResult<VkLayerProperties>
{
	void getResult (Context& context, VkLayerProperties* data)
	{
		m_result = context.getInstanceInterface().enumerateDeviceLayerProperties(context.getPhysicalDevice(), &m_count, data);
	}
};

struct CheckEnumerateInstanceExtensionPropertiesIncompleteResult : public CheckIncompleteResult<VkExtensionProperties>
{
	CheckEnumerateInstanceExtensionPropertiesIncompleteResult (std::string layerName = std::string()) : m_layerName(layerName) {}

	void getResult (Context& context, VkExtensionProperties* data)
	{
		const char* pLayerName = (m_layerName.length() != 0 ? m_layerName.c_str() : DE_NULL);
		m_result = context.getPlatformInterface().enumerateInstanceExtensionProperties(pLayerName, &m_count, data);
	}

private:
	const std::string	m_layerName;
};

struct CheckEnumerateDeviceExtensionPropertiesIncompleteResult : public CheckIncompleteResult<VkExtensionProperties>
{
	CheckEnumerateDeviceExtensionPropertiesIncompleteResult (std::string layerName = std::string()) : m_layerName(layerName) {}

	void getResult (Context& context, VkExtensionProperties* data)
	{
		const char* pLayerName = (m_layerName.length() != 0 ? m_layerName.c_str() : DE_NULL);
		m_result = context.getInstanceInterface().enumerateDeviceExtensionProperties(context.getPhysicalDevice(), pLayerName, &m_count, data);
	}

private:
	const std::string	m_layerName;
};

tcu::TestStatus enumeratePhysicalDevices (Context& context)
{
	TestLog&						log		= context.getTestContext().getLog();
	tcu::ResultCollector			results	(log);
	const vector<VkPhysicalDevice>	devices	= enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

	CheckEnumeratePhysicalDevicesIncompleteResult()(context, results, devices.size());

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumeratePhysicalDeviceGroups (Context& context)
{
	TestLog&											log				= context.getTestContext().getLog();
	tcu::ResultCollector								results			(log);
	const CustomInstance								instance		(createCustomInstanceWithExtension(context, "VK_KHR_device_group_creation"));
	const InstanceDriver&								vki				(instance.getDriver());
	const vector<VkPhysicalDeviceGroupProperties>		devicegroups	= enumeratePhysicalDeviceGroups(vki, instance);

	log << TestLog::Integer("NumDevices", "Number of device groups", "", QP_KEY_TAG_NONE, deInt64(devicegroups.size()));

	for (size_t ndx = 0; ndx < devicegroups.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devicegroups[ndx] << TestLog::EndMessage;

	CheckEnumeratePhysicalDeviceGroupsIncompleteResult()(context, results, devicegroups.size());

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

template<typename T>
void collectDuplicates (set<T>& duplicates, const vector<T>& values)
{
	set<T> seen;

	for (size_t ndx = 0; ndx < values.size(); ndx++)
	{
		const T& value = values[ndx];

		if (!seen.insert(value).second)
			duplicates.insert(value);
	}
}

void checkDuplicates (tcu::ResultCollector& results, const char* what, const vector<string>& values)
{
	set<string> duplicates;

	collectDuplicates(duplicates, values);

	for (set<string>::const_iterator iter = duplicates.begin(); iter != duplicates.end(); ++iter)
	{
		std::ostringstream msg;
		msg << "Duplicate " << what << ": " << *iter;
		results.fail(msg.str());
	}
}

void checkDuplicateExtensions (tcu::ResultCollector& results, const vector<string>& extensions)
{
	checkDuplicates(results, "extension", extensions);
}

void checkDuplicateLayers (tcu::ResultCollector& results, const vector<string>& layers)
{
	checkDuplicates(results, "layer", layers);
}

void checkKhrExtensions (tcu::ResultCollector&		results,
						 const vector<string>&		extensions,
						 const int					numAllowedKhrExtensions,
						 const char* const*			allowedKhrExtensions)
{
	const set<string>	allowedExtSet		(allowedKhrExtensions, allowedKhrExtensions+numAllowedKhrExtensions);

	for (vector<string>::const_iterator extIter = extensions.begin(); extIter != extensions.end(); ++extIter)
	{
		// Only Khronos-controlled extensions are checked
		if (de::beginsWith(*extIter, "VK_KHR_") &&
			!de::contains(allowedExtSet, *extIter))
		{
			results.fail("Unknown extension " + *extIter);
		}
	}
}

void checkInstanceExtensions (tcu::ResultCollector& results, const vector<string>& extensions)
{
#include "vkInstanceExtensions.inl"

	checkKhrExtensions(results, extensions, DE_LENGTH_OF_ARRAY(s_allowedInstanceKhrExtensions), s_allowedInstanceKhrExtensions);
	checkDuplicateExtensions(results, extensions);
}

void checkDeviceExtensions (tcu::ResultCollector& results, const vector<string>& extensions)
{
#include "vkDeviceExtensions.inl"

	checkKhrExtensions(results, extensions, DE_LENGTH_OF_ARRAY(s_allowedDeviceKhrExtensions), s_allowedDeviceKhrExtensions);
	checkDuplicateExtensions(results, extensions);
}

void checkInstanceExtensionDependencies(tcu::ResultCollector&											results,
										int																dependencyLength,
										const std::tuple<deUint32, deUint32, const char*, const char*>*	dependencies,
										deUint32														versionMajor,
										deUint32														versionMinor,
										const vector<VkExtensionProperties>&							extensionProperties)
{
	for (int ndx = 0; ndx < dependencyLength; ndx++)
	{
		deUint32 currentVersionMajor, currentVersionMinor;
		const char* extensionFirst;
		const char* extensionSecond;
		std::tie(currentVersionMajor, currentVersionMinor, extensionFirst, extensionSecond) = dependencies[ndx];
		if (currentVersionMajor != versionMajor || currentVersionMinor != versionMinor)
			continue;
		if (isExtensionSupported(extensionProperties, RequiredExtension(extensionFirst)) &&
			!isExtensionSupported(extensionProperties, RequiredExtension(extensionSecond)))
		{
			results.fail("Extension " + string(extensionFirst) + " is missing dependency: " + string(extensionSecond));
		}
	}
}

void checkDeviceExtensionDependencies(tcu::ResultCollector&												results,
									  int																dependencyLength,
									  const std::tuple<deUint32, deUint32, const char*, const char*>*	dependencies,
									  deUint32															versionMajor,
									  deUint32															versionMinor,
									  const vector<VkExtensionProperties>&								instanceExtensionProperties,
									  const vector<VkExtensionProperties>&								deviceExtensionProperties)
{
	for (int ndx = 0; ndx < dependencyLength; ndx++)
	{
		deUint32 currentVersionMajor, currentVersionMinor;
		const char* extensionFirst;
		const char* extensionSecond;
		std::tie(currentVersionMajor, currentVersionMinor, extensionFirst, extensionSecond) = dependencies[ndx];
		if (currentVersionMajor != versionMajor || currentVersionMinor != versionMinor)
			continue;
		if (isExtensionSupported(deviceExtensionProperties, RequiredExtension(extensionFirst)) &&
			!isExtensionSupported(deviceExtensionProperties, RequiredExtension(extensionSecond)) &&
			!isExtensionSupported(instanceExtensionProperties, RequiredExtension(extensionSecond)))
		{
			results.fail("Extension " + string(extensionFirst) + " is missing dependency: " + string(extensionSecond));
		}
	}
}

tcu::TestStatus enumerateInstanceLayers (Context& context)
{
	TestLog&						log					= context.getTestContext().getLog();
	tcu::ResultCollector			results				(log);
	const vector<VkLayerProperties>	properties			= enumerateInstanceLayerProperties(context.getPlatformInterface());
	vector<string>					layerNames;

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
	{
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

		layerNames.push_back(properties[ndx].layerName);
	}

	checkDuplicateLayers(results, layerNames);
	CheckEnumerateInstanceLayerPropertiesIncompleteResult()(context, results, layerNames.size());

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumerateInstanceExtensions (Context& context)
{
	TestLog&				log		= context.getTestContext().getLog();
	tcu::ResultCollector	results	(log);

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);
		vector<string>						extensionNames;

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
		{
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

			extensionNames.push_back(properties[ndx].extensionName);
		}

		checkInstanceExtensions(results, extensionNames);
		CheckEnumerateInstanceExtensionPropertiesIncompleteResult()(context, results, properties.size());

		for (const auto& version : releasedApiVersions)
		{
			deUint32 versionMajor, versionMinor;
			std::tie(std::ignore, versionMajor, versionMinor) = version;
			if (context.contextSupports(vk::ApiVersion(versionMajor, versionMinor, 0)))
			{
				checkInstanceExtensionDependencies(results,
					DE_LENGTH_OF_ARRAY(instanceExtensionDependencies),
					instanceExtensionDependencies,
					versionMajor,
					versionMinor,
					properties);
				break;
			}
		}
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section				(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties			= enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName);
			vector<string>						extensionNames;

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
			{
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;

				extensionNames.push_back(properties[extNdx].extensionName);
			}

			checkInstanceExtensions(results, extensionNames);
			CheckEnumerateInstanceExtensionPropertiesIncompleteResult(layer->layerName)(context, results, properties.size());
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testNoKhxExtensions (Context& context)
{
	VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const PlatformInterface&	vkp				= context.getPlatformInterface();
	const InstanceInterface&	vki				= context.getInstanceInterface();

	tcu::ResultCollector		results(context.getTestContext().getLog());
	bool						testSucceeded = true;
	deUint32					instanceExtensionsCount;
	deUint32					deviceExtensionsCount;

	// grab number of instance and device extensions
	vkp.enumerateInstanceExtensionProperties(DE_NULL, &instanceExtensionsCount, DE_NULL);
	vki.enumerateDeviceExtensionProperties(physicalDevice, DE_NULL, &deviceExtensionsCount, DE_NULL);
	vector<VkExtensionProperties> extensionsProperties(instanceExtensionsCount + deviceExtensionsCount);

	// grab instance and device extensions into single vector
	if (instanceExtensionsCount)
		vkp.enumerateInstanceExtensionProperties(DE_NULL, &instanceExtensionsCount, &extensionsProperties[0]);
	if (deviceExtensionsCount)
		vki.enumerateDeviceExtensionProperties(physicalDevice, DE_NULL, &deviceExtensionsCount, &extensionsProperties[instanceExtensionsCount]);

	// iterate over all extensions and verify their names
	vector<VkExtensionProperties>::const_iterator extension = extensionsProperties.begin();
	while (extension != extensionsProperties.end())
	{
		// KHX author ID is no longer used, all KHX extensions have been promoted to KHR status
		std::string extensionName(extension->extensionName);
		bool caseFailed = de::beginsWith(extensionName, "VK_KHX_");
		if (caseFailed)
		{
			results.fail("Invalid extension name " + extensionName);
			testSucceeded = false;
		}
		++extension;
	}

	if (testSucceeded)
		return tcu::TestStatus::pass("No extensions begining with \"VK_KHX\"");
	return tcu::TestStatus::fail("One or more extensions begins with \"VK_KHX\"");
}

tcu::TestStatus enumerateDeviceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	tcu::ResultCollector			results		(log);
	const vector<VkLayerProperties>	properties	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());
	vector<string>					layerNames;

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
	{
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

		layerNames.push_back(properties[ndx].layerName);
	}

	checkDuplicateLayers(results, layerNames);
	CheckEnumerateDeviceLayerPropertiesIncompleteResult()(context, results, layerNames.size());

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumerateDeviceExtensions (Context& context)
{
	TestLog&				log		= context.getTestContext().getLog();
	tcu::ResultCollector	results	(log);

	{
		const ScopedLogSection				section						(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	instanceExtensionProperties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);
		const vector<VkExtensionProperties>	deviceExtensionProperties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL);
		vector<string>						deviceExtensionNames;

		for (size_t ndx = 0; ndx < deviceExtensionProperties.size(); ndx++)
		{
			log << TestLog::Message << ndx << ": " << deviceExtensionProperties[ndx] << TestLog::EndMessage;

			deviceExtensionNames.push_back(deviceExtensionProperties[ndx].extensionName);
		}

		checkDeviceExtensions(results, deviceExtensionNames);
		CheckEnumerateDeviceExtensionPropertiesIncompleteResult()(context, results, deviceExtensionProperties.size());

		for (const auto& version : releasedApiVersions)
		{
			deUint32 versionMajor, versionMinor;
			std::tie(std::ignore, versionMajor, versionMinor) = version;
			if (context.contextSupports(vk::ApiVersion(versionMajor, versionMinor, 0)))
			{
				checkDeviceExtensionDependencies(results,
					DE_LENGTH_OF_ARRAY(deviceExtensionDependencies),
					deviceExtensionDependencies,
					versionMajor,
					versionMinor,
					instanceExtensionProperties,
					deviceExtensionProperties);
				break;
			}
		}
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName);
			vector<string>						extensionNames;

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
			{
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;


				extensionNames.push_back(properties[extNdx].extensionName);
			}

			checkDeviceExtensions(results, extensionNames);
			CheckEnumerateDeviceExtensionPropertiesIncompleteResult(layer->layerName)(context, results, properties.size());
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

#define VK_SIZE_OF(STRUCT, MEMBER)					(sizeof(((STRUCT*)0)->MEMBER))
#define OFFSET_TABLE_ENTRY(STRUCT, MEMBER)			{ (size_t)DE_OFFSET_OF(STRUCT, MEMBER), VK_SIZE_OF(STRUCT, MEMBER) }

tcu::TestStatus deviceFeatures (Context& context)
{
	using namespace ValidateQueryBits;

	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceFeatures*		features;
	deUint8							buffer[sizeof(VkPhysicalDeviceFeatures) + GUARD_SIZE];

	const QueryMemberTableEntry featureOffsetTable[] =
	{
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, robustBufferAccess),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, fullDrawIndexUint32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, imageCubeArray),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, independentBlend),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, geometryShader),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, tessellationShader),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sampleRateShading),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, dualSrcBlend),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, logicOp),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, multiDrawIndirect),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, drawIndirectFirstInstance),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, depthClamp),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, depthBiasClamp),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, fillModeNonSolid),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, depthBounds),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, wideLines),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, largePoints),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, alphaToOne),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, multiViewport),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, samplerAnisotropy),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, textureCompressionETC2),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, textureCompressionASTC_LDR),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, textureCompressionBC),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, occlusionQueryPrecise),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, pipelineStatisticsQuery),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, vertexPipelineStoresAndAtomics),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, fragmentStoresAndAtomics),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderTessellationAndGeometryPointSize),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderImageGatherExtended),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageImageExtendedFormats),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageImageMultisample),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageImageReadWithoutFormat),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageImageWriteWithoutFormat),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderUniformBufferArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderSampledImageArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageBufferArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderStorageImageArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderClipDistance),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderCullDistance),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderFloat64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderInt64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderInt16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderResourceResidency),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, shaderResourceMinLod),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseBinding),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidencyBuffer),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidencyImage2D),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidencyImage3D),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidency2Samples),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidency4Samples),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidency8Samples),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidency16Samples),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, sparseResidencyAliased),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, variableMultisampleRate),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceFeatures, inheritedQueries),
		{ 0, 0 }
	};

	deMemset(buffer, GUARD_VALUE, sizeof(buffer));
	features = reinterpret_cast<VkPhysicalDeviceFeatures*>(buffer);

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), features);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *features << TestLog::EndMessage;

	// Requirements and dependencies
	{
		if (!features->robustBufferAccess)
			return tcu::TestStatus::fail("robustBufferAccess is not supported");

		// multiViewport requires MultiViewport (SPIR-V capability) support, which depends on Geometry
		if (features->multiViewport && !features->geometryShader)
			return tcu::TestStatus::fail("multiViewport is supported but geometryShader is not");
	}

	for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceFeatures)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceFeatures - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceFeatures buffer overflow");
		}
	}

	if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceFeatures, context.getInstanceInterface(), featureOffsetTable))
	{
		log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceFeatures not completely initialized" << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceFeatures incomplete initialization");
	}

	return tcu::TestStatus::pass("Query succeeded");
}

static const ValidateQueryBits::QueryMemberTableEntry s_physicalDevicePropertiesOffsetTable[] =
{
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, apiVersion),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, driverVersion),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, vendorID),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, deviceID),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, deviceType),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, pipelineCacheUUID),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxImageDimension1D),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxImageDimension2D),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxImageDimension3D),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxImageDimensionCube),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxImageArrayLayers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTexelBufferElements),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxUniformBufferRange),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxStorageBufferRange),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPushConstantsSize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxMemoryAllocationCount),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxSamplerAllocationCount),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.bufferImageGranularity),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.sparseAddressSpaceSize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxBoundDescriptorSets),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorSamplers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorUniformBuffers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorStorageBuffers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorSampledImages),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorStorageImages),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageDescriptorInputAttachments),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxPerStageResources),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetSamplers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetUniformBuffers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetUniformBuffersDynamic),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetStorageBuffers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetStorageBuffersDynamic),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetSampledImages),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetStorageImages),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDescriptorSetInputAttachments),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxVertexInputAttributes),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxVertexInputBindings),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxVertexInputAttributeOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxVertexInputBindingStride),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxVertexOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationGenerationLevel),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationPatchSize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationControlPerVertexInputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationControlPerVertexOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationControlPerPatchOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationControlTotalOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationEvaluationInputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTessellationEvaluationOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxGeometryShaderInvocations),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxGeometryInputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxGeometryOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxGeometryOutputVertices),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxGeometryTotalOutputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFragmentInputComponents),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFragmentOutputAttachments),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFragmentDualSrcAttachments),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFragmentCombinedOutputResources),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxComputeSharedMemorySize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxComputeWorkGroupCount[3]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxComputeWorkGroupInvocations),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxComputeWorkGroupSize[3]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.subPixelPrecisionBits),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.subTexelPrecisionBits),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.mipmapPrecisionBits),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDrawIndexedIndexValue),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxDrawIndirectCount),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxSamplerLodBias),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxSamplerAnisotropy),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxViewports),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxViewportDimensions[2]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.viewportBoundsRange[2]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.viewportSubPixelBits),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minMemoryMapAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minTexelBufferOffsetAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minUniformBufferOffsetAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minStorageBufferOffsetAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minTexelOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTexelOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minTexelGatherOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxTexelGatherOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.minInterpolationOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxInterpolationOffset),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.subPixelInterpolationOffsetBits),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFramebufferWidth),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFramebufferHeight),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxFramebufferLayers),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.framebufferColorSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.framebufferDepthSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.framebufferStencilSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.framebufferNoAttachmentsSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxColorAttachments),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.sampledImageColorSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.sampledImageIntegerSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.sampledImageDepthSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.sampledImageStencilSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.storageImageSampleCounts),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxSampleMaskWords),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.timestampComputeAndGraphics),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.timestampPeriod),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxClipDistances),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxCullDistances),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.maxCombinedClipAndCullDistances),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.discreteQueuePriorities),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.pointSizeRange[2]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.lineWidthRange[2]),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.pointSizeGranularity),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.lineWidthGranularity),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.strictLines),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.standardSampleLocations),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.optimalBufferCopyOffsetAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.optimalBufferCopyRowPitchAlignment),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, limits.nonCoherentAtomSize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, sparseProperties.residencyStandard2DBlockShape),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, sparseProperties.residencyStandard2DMultisampleBlockShape),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, sparseProperties.residencyStandard3DBlockShape),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, sparseProperties.residencyAlignedMipSize),
	OFFSET_TABLE_ENTRY(VkPhysicalDeviceProperties, sparseProperties.residencyNonResidentStrict),
	{ 0, 0 }
};

tcu::TestStatus deviceProperties (Context& context)
{
	using namespace ValidateQueryBits;

	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceProperties*		props;
	VkPhysicalDeviceFeatures		features;
	deUint8							buffer[sizeof(VkPhysicalDeviceProperties) + GUARD_SIZE];

	props = reinterpret_cast<VkPhysicalDeviceProperties*>(buffer);
	deMemset(props, GUARD_VALUE, sizeof(buffer));

	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), props);
	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *props << TestLog::EndMessage;

	if (!validateFeatureLimits(props, &features, log))
		return tcu::TestStatus::fail("deviceProperties - feature limits failed");

	for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceProperties)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceProperties - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceProperties buffer overflow");
		}
	}

	if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceProperties, context.getInstanceInterface(), s_physicalDevicePropertiesOffsetTable))
	{
		log << TestLog::Message << "deviceProperties - VkPhysicalDeviceProperties not completely initialized" << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceProperties incomplete initialization");
	}

	// Check if deviceName string is properly terminated.
	if (deStrnlen(props->deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE) == VK_MAX_PHYSICAL_DEVICE_NAME_SIZE)
	{
		log << TestLog::Message << "deviceProperties - VkPhysicalDeviceProperties deviceName not properly initialized" << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceProperties incomplete initialization");
	}

	{
		const ApiVersion deviceVersion = unpackVersion(props->apiVersion);
		const ApiVersion deqpVersion = unpackVersion(VK_API_VERSION_1_2);

		if (deviceVersion.majorNum != deqpVersion.majorNum)
		{
			log << TestLog::Message << "deviceProperties - API Major Version " << deviceVersion.majorNum << " is not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceProperties apiVersion not valid");
		}

		if (deviceVersion.minorNum > deqpVersion.minorNum)
		{
			log << TestLog::Message << "deviceProperties - API Minor Version " << deviceVersion.minorNum << " is not valid for this version of dEQP" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceProperties apiVersion not valid");
		}
	}

	return tcu::TestStatus::pass("DeviceProperites query succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties (Context& context)
{
	TestLog&								log					= context.getTestContext().getLog();
	const vector<VkQueueFamilyProperties>	queueProperties		= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

	for (size_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
		log << TestLog::Message << queueNdx << ": " << queueProperties[queueNdx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying queue properties succeeded");
}

tcu::TestStatus deviceMemoryProperties (Context& context)
{
	TestLog&							log			= context.getTestContext().getLog();
	VkPhysicalDeviceMemoryProperties*	memProps;
	deUint8								buffer[sizeof(VkPhysicalDeviceMemoryProperties) + GUARD_SIZE];

	memProps = reinterpret_cast<VkPhysicalDeviceMemoryProperties*>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));

	context.getInstanceInterface().getPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), memProps);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *memProps << TestLog::EndMessage;

	for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceMemoryProperties)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceMemoryProperties - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryProperties buffer overflow");
		}
	}

	if (memProps->memoryHeapCount >= VK_MAX_MEMORY_HEAPS)
	{
		log << TestLog::Message << "deviceMemoryProperties - HeapCount larger than " << (deUint32)VK_MAX_MEMORY_HEAPS << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceMemoryProperties HeapCount too large");
	}

	if (memProps->memoryHeapCount == 1)
	{
		if ((memProps->memoryHeaps[0].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
		{
			log << TestLog::Message << "deviceMemoryProperties - Single heap is not marked DEVICE_LOCAL" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryProperties invalid HeapFlags");
		}
	}

	const VkMemoryPropertyFlags validPropertyFlags[] =
	{
		0,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
	};

	const VkMemoryPropertyFlags requiredPropertyFlags[] =
	{
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	};

	bool requiredFlagsFound[DE_LENGTH_OF_ARRAY(requiredPropertyFlags)];
	std::fill(DE_ARRAY_BEGIN(requiredFlagsFound), DE_ARRAY_END(requiredFlagsFound), false);

	for (deUint32 memoryNdx = 0; memoryNdx < memProps->memoryTypeCount; memoryNdx++)
	{
		bool validPropTypeFound = false;

		if (memProps->memoryTypes[memoryNdx].heapIndex >= memProps->memoryHeapCount)
		{
			log << TestLog::Message << "deviceMemoryProperties - heapIndex " << memProps->memoryTypes[memoryNdx].heapIndex << " larger than heapCount" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryProperties - invalid heapIndex");
		}

		const VkMemoryPropertyFlags bitsToCheck = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

		for (const VkMemoryPropertyFlags* requiredFlagsIterator = DE_ARRAY_BEGIN(requiredPropertyFlags); requiredFlagsIterator != DE_ARRAY_END(requiredPropertyFlags); requiredFlagsIterator++)
			if ((memProps->memoryTypes[memoryNdx].propertyFlags & *requiredFlagsIterator) == *requiredFlagsIterator)
				requiredFlagsFound[requiredFlagsIterator - DE_ARRAY_BEGIN(requiredPropertyFlags)] = true;

		if (de::contains(DE_ARRAY_BEGIN(validPropertyFlags), DE_ARRAY_END(validPropertyFlags), memProps->memoryTypes[memoryNdx].propertyFlags & bitsToCheck))
			validPropTypeFound = true;

		if (!validPropTypeFound)
		{
			log << TestLog::Message << "deviceMemoryProperties - propertyFlags "
				<< memProps->memoryTypes[memoryNdx].propertyFlags << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryProperties propertyFlags not valid");
		}

		if (memProps->memoryTypes[memoryNdx].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		{
			if ((memProps->memoryHeaps[memProps->memoryTypes[memoryNdx].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
			{
				log << TestLog::Message << "deviceMemoryProperties - DEVICE_LOCAL memory type references heap which is not DEVICE_LOCAL" << TestLog::EndMessage;
				return tcu::TestStatus::fail("deviceMemoryProperties inconsistent memoryType and HeapFlags");
			}
		}
		else
		{
			if (memProps->memoryHeaps[memProps->memoryTypes[memoryNdx].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				log << TestLog::Message << "deviceMemoryProperties - non-DEVICE_LOCAL memory type references heap with is DEVICE_LOCAL" << TestLog::EndMessage;
				return tcu::TestStatus::fail("deviceMemoryProperties inconsistent memoryType and HeapFlags");
			}
		}
	}

	bool* requiredFlagsFoundIterator = std::find(DE_ARRAY_BEGIN(requiredFlagsFound), DE_ARRAY_END(requiredFlagsFound), false);
	if (requiredFlagsFoundIterator != DE_ARRAY_END(requiredFlagsFound))
	{
		DE_ASSERT(requiredFlagsFoundIterator - DE_ARRAY_BEGIN(requiredFlagsFound) <= DE_LENGTH_OF_ARRAY(requiredPropertyFlags));
		log << TestLog::Message << "deviceMemoryProperties - required property flags "
			<< getMemoryPropertyFlagsStr(requiredPropertyFlags[requiredFlagsFoundIterator - DE_ARRAY_BEGIN(requiredFlagsFound)]) << " not found" << TestLog::EndMessage;

		return tcu::TestStatus::fail("deviceMemoryProperties propertyFlags not valid");
	}

	return tcu::TestStatus::pass("Querying memory properties succeeded");
}

tcu::TestStatus deviceGroupPeerMemoryFeatures (Context& context)
{
	TestLog&							log						= context.getTestContext().getLog();
	const PlatformInterface&			vkp						= context.getPlatformInterface();
	const CustomInstance				instance				(createCustomInstanceWithExtension(context, "VK_KHR_device_group_creation"));
	const InstanceDriver&				vki						(instance.getDriver());
	const tcu::CommandLine&				cmdLine					= context.getTestContext().getCommandLine();
	const deUint32						devGroupIdx				= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32						deviceIdx				= vk::chooseDeviceIndex(context.getInstanceInterface(), instance, cmdLine);
	const float							queuePriority			= 1.0f;
	VkPhysicalDeviceMemoryProperties	memProps;
	VkPeerMemoryFeatureFlags*			peerMemFeatures;
	deUint8								buffer					[sizeof(VkPeerMemoryFeatureFlags) + GUARD_SIZE];
	deUint32							numPhysicalDevices		= 0;
	deUint32							queueFamilyIndex		= 0;

	const vector<VkPhysicalDeviceGroupProperties>		deviceGroupProps = enumeratePhysicalDeviceGroups(vki, instance);
	std::vector<const char*>							deviceExtensions;
	deviceExtensions.push_back("VK_KHR_device_group");

	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");

	const std::vector<VkQueueFamilyProperties>	queueProps		= getPhysicalDeviceQueueFamilyProperties(vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx]);
	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			queueFamilyIndex = (deUint32)queueNdx;
	}
	const VkDeviceQueueCreateInfo		deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,			//type
		DE_NULL,											//pNext
		(VkDeviceQueueCreateFlags)0u,						//flags
		queueFamilyIndex,									//queueFamilyIndex;
		1u,													//queueCount;
		&queuePriority,										//pQueuePriorities;
	};

	// Need atleast 2 devices for peer memory features
	numPhysicalDevices = deviceGroupProps[devGroupIdx].physicalDeviceCount;
	if (numPhysicalDevices < 2)
		TCU_THROW(NotSupportedError, "Need a device Group with at least 2 physical devices.");

	// Create device groups
	const VkDeviceGroupDeviceCreateInfo						deviceGroupInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO,	//stype
		DE_NULL,											//pNext
		deviceGroupProps[devGroupIdx].physicalDeviceCount,	//physicalDeviceCount
		deviceGroupProps[devGroupIdx].physicalDevices		//physicalDevices
	};

	const VkDeviceCreateInfo								deviceCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//sType;
		&deviceGroupInfo,												//pNext;
		(VkDeviceCreateFlags)0u,										//flags
		1,																//queueRecordCount;
		&deviceQueueCreateInfo,											//pRequestedQueues;
		0,																//layerCount;
		DE_NULL,														//ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								//extensionCount;
		(deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0]),	//ppEnabledExtensionNames;
		DE_NULL,														//pEnabledFeatures;
	};

	Move<VkDevice>		deviceGroup = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &deviceCreateInfo);
	const DeviceDriver	vk	(vkp, instance, *deviceGroup);
	context.getInstanceInterface().getPhysicalDeviceMemoryProperties(deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &memProps);

	peerMemFeatures = reinterpret_cast<VkPeerMemoryFeatureFlags*>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));

	for (deUint32 heapIndex = 0; heapIndex < memProps.memoryHeapCount; heapIndex++)
	{
		for (deUint32 localDeviceIndex = 0; localDeviceIndex < numPhysicalDevices; localDeviceIndex++)
		{
			for (deUint32 remoteDeviceIndex = 0; remoteDeviceIndex < numPhysicalDevices; remoteDeviceIndex++)
			{
				if (localDeviceIndex != remoteDeviceIndex)
				{
					vk.getDeviceGroupPeerMemoryFeatures(deviceGroup.get(), heapIndex, localDeviceIndex, remoteDeviceIndex, peerMemFeatures);

					// Check guard
					for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
					{
						if (buffer[ndx + sizeof(VkPeerMemoryFeatureFlags)] != GUARD_VALUE)
						{
							log << TestLog::Message << "deviceGroupPeerMemoryFeatures - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
							return tcu::TestStatus::fail("deviceGroupPeerMemoryFeatures buffer overflow");
						}
					}

					VkPeerMemoryFeatureFlags requiredFlag = VK_PEER_MEMORY_FEATURE_COPY_DST_BIT;
					VkPeerMemoryFeatureFlags maxValidFlag = VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT|VK_PEER_MEMORY_FEATURE_COPY_DST_BIT|
																VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT|VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT;
					if ((!(*peerMemFeatures & requiredFlag)) ||
						*peerMemFeatures > maxValidFlag)
						return tcu::TestStatus::fail("deviceGroupPeerMemoryFeatures invalid flag");

					log << TestLog::Message << "deviceGroup = " << deviceGroup.get() << TestLog::EndMessage
						<< TestLog::Message << "heapIndex = " << heapIndex << TestLog::EndMessage
						<< TestLog::Message << "localDeviceIndex = " << localDeviceIndex << TestLog::EndMessage
						<< TestLog::Message << "remoteDeviceIndex = " << remoteDeviceIndex << TestLog::EndMessage
						<< TestLog::Message << "PeerMemoryFeatureFlags = " << *peerMemFeatures << TestLog::EndMessage;
				}
			} // remote device
		} // local device
	} // heap Index

	return tcu::TestStatus::pass("Querying deviceGroup peer memory features succeeded");
}

tcu::TestStatus deviceMemoryBudgetProperties (Context& context)
{
	TestLog&							log			= context.getTestContext().getLog();
	deUint8								buffer[sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT) + GUARD_SIZE];

	if (!context.isDeviceFunctionalitySupported("VK_EXT_memory_budget"))
		TCU_THROW(NotSupportedError, "VK_EXT_memory_budget is not supported");

	VkPhysicalDeviceMemoryBudgetPropertiesEXT *budgetProps = reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT *>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));

	budgetProps->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
	budgetProps->pNext = DE_NULL;

	VkPhysicalDeviceMemoryProperties2	memProps;
	deMemset(&memProps, 0, sizeof(memProps));
	memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	memProps.pNext = budgetProps;

	context.getInstanceInterface().getPhysicalDeviceMemoryProperties2(context.getPhysicalDevice(), &memProps);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *budgetProps << TestLog::EndMessage;

	for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceMemoryBudgetProperties - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryBudgetProperties buffer overflow");
		}
	}

	for (deUint32 i = 0; i < memProps.memoryProperties.memoryHeapCount; ++i)
	{
		if (budgetProps->heapBudget[i] == 0)
		{
			log << TestLog::Message << "deviceMemoryBudgetProperties - Supported heaps must report nonzero budget" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid heap budget (zero)");
		}
		if (budgetProps->heapBudget[i] > memProps.memoryProperties.memoryHeaps[i].size)
		{
			log << TestLog::Message << "deviceMemoryBudgetProperties - Heap budget must be less than or equal to heap size" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid heap budget (too large)");
		}
	}

	for (deUint32 i = memProps.memoryProperties.memoryHeapCount; i < VK_MAX_MEMORY_HEAPS; ++i)
	{
		if (budgetProps->heapBudget[i] != 0 || budgetProps->heapUsage[i] != 0)
		{
			log << TestLog::Message << "deviceMemoryBudgetProperties - Unused heaps must report budget/usage of zero" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid unused heaps");
		}
	}

	return tcu::TestStatus::pass("Querying memory budget properties succeeded");
}

namespace
{

#include "vkMandatoryFeatures.inl"

}

tcu::TestStatus deviceMandatoryFeatures(Context& context)
{
	if ( checkMandatoryFeatures(context) )
		return tcu::TestStatus::pass("Passed");
	return tcu::TestStatus::fail("Not all mandatory features are supported ( see: vkspec.html#features-requirements )");
}

VkFormatFeatureFlags getBaseRequiredOptimalTilingFeatures (VkFormat format)
{
	struct Formatpair
	{
		VkFormat				format;
		VkFormatFeatureFlags	flags;
	};

	enum
	{
		SAIM = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
		BLSR = VK_FORMAT_FEATURE_BLIT_SRC_BIT,
		SIFL = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
		COAT = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
		BLDS = VK_FORMAT_FEATURE_BLIT_DST_BIT,
		CABL = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,
		STIM = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
		STIA = VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT,
		DSAT = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
		TRSR = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
		TRDS = VK_FORMAT_FEATURE_TRANSFER_DST_BIT
	};

	static const Formatpair formatflags[] =
	{
		{ VK_FORMAT_B4G4R4A4_UNORM_PACK16,		SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_R5G6B5_UNORM_PACK16,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A1R5G5B5_UNORM_PACK16,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8_UNORM,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8_SNORM,					SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_R8_UINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R8_SINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R8G8_UNORM,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8G8_SNORM,					SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_R8G8_UINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R8G8_SINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R8G8B8A8_UNORM,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | STIM | CABL },
		{ VK_FORMAT_R8G8B8A8_SNORM,				SAIM | BLSR | TRSR | TRDS |               SIFL | STIM },
		{ VK_FORMAT_R8G8B8A8_UINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R8G8B8A8_SINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R8G8B8A8_SRGB,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_B8G8R8A8_UNORM,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_B8G8R8A8_SRGB,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A8B8G8R8_UNORM_PACK32,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A8B8G8R8_SNORM_PACK32,		SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_A8B8G8R8_UINT_PACK32,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_A8B8G8R8_SINT_PACK32,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_A8B8G8R8_SRGB_PACK32,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A2B10G10R10_UNORM_PACK32,	SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A2B10G10R10_UINT_PACK32,	SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R16_UINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R16_SINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R16_SFLOAT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R16G16_UINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R16G16_SINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS },
		{ VK_FORMAT_R16G16_SFLOAT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R16G16B16A16_UINT,			SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R16G16B16A16_SINT,			SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R16G16B16A16_SFLOAT,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | STIM | CABL },
		{ VK_FORMAT_R32_UINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM |        STIA },
		{ VK_FORMAT_R32_SINT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM |        STIA },
		{ VK_FORMAT_R32_SFLOAT,					SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_UINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_SINT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_SFLOAT,				SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_UINT,			SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_SINT,			SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_SFLOAT,		SAIM | BLSR | TRSR | TRDS | COAT | BLDS |        STIM },
		{ VK_FORMAT_B10G11R11_UFLOAT_PACK32,	SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,		SAIM | BLSR | TRSR | TRDS |               SIFL },
		{ VK_FORMAT_D16_UNORM,					SAIM | BLSR | TRSR | TRDS |                                           DSAT },
	};

	size_t formatpairs = sizeof(formatflags) / sizeof(Formatpair);

	for (unsigned int i = 0; i < formatpairs; i++)
		if (formatflags[i].format == format)
			return formatflags[i].flags;
	return 0;
}

VkFormatFeatureFlags getRequiredOptimalExtendedTilingFeatures (Context& context, VkFormat format, VkFormatFeatureFlags queriedFlags)
{
	VkFormatFeatureFlags	flags	= (VkFormatFeatureFlags)0;

	// VK_EXT_sampler_filter_minmax:
	//	If filterMinmaxSingleComponentFormats is VK_TRUE, the following formats must
	//	support the VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT feature with
	//	VK_IMAGE_TILING_OPTIMAL, if they support VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT.

	static const VkFormat s_requiredSampledImageFilterMinMaxFormats[] =
	{
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	if ((queriedFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
	{
		if (de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_EXT_sampler_filter_minmax"))
		{
			if (de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterMinMaxFormats), DE_ARRAY_END(s_requiredSampledImageFilterMinMaxFormats), format))
			{
				VkPhysicalDeviceSamplerFilterMinmaxProperties		physicalDeviceSamplerMinMaxProperties =
				{
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
					DE_NULL,
					DE_FALSE,
					DE_FALSE
				};

				{
					VkPhysicalDeviceProperties2		physicalDeviceProperties;
					physicalDeviceProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					physicalDeviceProperties.pNext	= &physicalDeviceSamplerMinMaxProperties;

					const InstanceInterface&		vk = context.getInstanceInterface();
					vk.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &physicalDeviceProperties);
				}

				if (physicalDeviceSamplerMinMaxProperties.filterMinmaxSingleComponentFormats)
				{
					flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT;
				}
			}
		}
	}

	// VK_EXT_filter_cubic:
	// If cubic filtering is supported, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT must be supported for the following image view types:
	// VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY
	static const VkFormat s_requiredSampledImageFilterCubicFormats[] =
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32
	};

	static const VkFormat s_requiredSampledImageFilterCubicFormatsETC2[] =
	{
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
	};

	if ( (queriedFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_EXT_filter_cubic") )
	{
		if ( de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterCubicFormats), DE_ARRAY_END(s_requiredSampledImageFilterCubicFormats), format) )
			flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT;

		VkPhysicalDeviceFeatures2						coreFeatures;
		deMemset(&coreFeatures, 0, sizeof(coreFeatures));

		coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		coreFeatures.pNext = DE_NULL;
		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);
		if ( coreFeatures.features.textureCompressionETC2 && de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterCubicFormatsETC2), DE_ARRAY_END(s_requiredSampledImageFilterCubicFormatsETC2), format) )
			flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT;
	}

	return flags;
}

VkFormatFeatureFlags getRequiredBufferFeatures (VkFormat format)
{
	static const VkFormat s_requiredVertexBufferFormats[] =
	{
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};
	static const VkFormat s_requiredUniformTexelBufferFormats[] =
	{
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32
	};
	static const VkFormat s_requiredStorageTexelBufferFormats[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};
	static const VkFormat s_requiredStorageTexelBufferAtomicFormats[] =
	{
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT
	};

	VkFormatFeatureFlags	flags	= (VkFormatFeatureFlags)0;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredVertexBufferFormats), DE_ARRAY_END(s_requiredVertexBufferFormats), format))
		flags |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredUniformTexelBufferFormats), DE_ARRAY_END(s_requiredUniformTexelBufferFormats), format))
		flags |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageTexelBufferFormats), DE_ARRAY_END(s_requiredStorageTexelBufferFormats), format))
		flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageTexelBufferAtomicFormats), DE_ARRAY_END(s_requiredStorageTexelBufferAtomicFormats), format))
		flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;

	return flags;
}

VkPhysicalDeviceSamplerYcbcrConversionFeatures getPhysicalDeviceSamplerYcbcrConversionFeatures (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceFeatures2						coreFeatures;
	VkPhysicalDeviceSamplerYcbcrConversionFeatures	ycbcrFeatures;

	deMemset(&coreFeatures, 0, sizeof(coreFeatures));
	deMemset(&ycbcrFeatures, 0, sizeof(ycbcrFeatures));

	coreFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	coreFeatures.pNext		= &ycbcrFeatures;
	ycbcrFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;

	vk.getPhysicalDeviceFeatures2(physicalDevice, &coreFeatures);

	return ycbcrFeatures;
}

void checkYcbcrApiSupport (Context& context)
{
	// check if YCbcr API and are supported by implementation

	// the support for formats and YCbCr may still be optional - see isYcbcrConversionSupported below

	if (!vk::isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_sampler_ycbcr_conversion"))
	{
		if (!context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion"))
			TCU_THROW(NotSupportedError, "VK_KHR_sampler_ycbcr_conversion is not supported");

		// Hard dependency for ycbcr
		TCU_CHECK(de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), "VK_KHR_get_physical_device_properties2"));
	}
}

bool isYcbcrConversionSupported (Context& context)
{
	checkYcbcrApiSupport(context);

	const VkPhysicalDeviceSamplerYcbcrConversionFeatures	ycbcrFeatures	= getPhysicalDeviceSamplerYcbcrConversionFeatures(context.getInstanceInterface(), context.getPhysicalDevice());

	return (ycbcrFeatures.samplerYcbcrConversion == VK_TRUE);
}

VkFormatFeatureFlags getRequiredYcbcrFormatFeatures (Context& context, VkFormat format)
{
	bool req = isYcbcrConversionSupported(context) && (	format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM ||
														format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);

	const VkFormatFeatureFlags	required	= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
											| VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
											| VK_FORMAT_FEATURE_TRANSFER_DST_BIT
											| VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
	return req ? required : (VkFormatFeatureFlags)0;
}

VkFormatFeatureFlags getRequiredOptimalTilingFeatures (Context& context, VkFormat format)
{
	if (isYCbCrFormat(format))
		return getRequiredYcbcrFormatFeatures(context, format);
	else
	{
		VkFormatFeatureFlags ret = getBaseRequiredOptimalTilingFeatures(format);

		// \todo [2017-05-16 pyry] This should be extended to cover for example COLOR_ATTACHMENT for depth formats etc.
		// \todo [2017-05-18 pyry] Any other color conversion related features that can't be supported by regular formats?
		ret |= getRequiredOptimalExtendedTilingFeatures(context, format, ret);

		// Compressed formats have optional support for some features
		// TODO: Is this really correct? It looks like it should be checking the different compressed features
		if (isCompressedFormat(format) && (ret & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			ret |=	VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
					VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
					VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
					VK_FORMAT_FEATURE_BLIT_SRC_BIT;

		return ret;
	}
}

bool requiresYCbCrConversion(VkFormat format)
{
	return isYCbCrFormat(format) &&
			format != VK_FORMAT_R10X6_UNORM_PACK16 && format != VK_FORMAT_R10X6G10X6_UNORM_2PACK16 &&
			format != VK_FORMAT_R12X4_UNORM_PACK16 && format != VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
}

VkFormatFeatureFlags getAllowedOptimalTilingFeatures (VkFormat format)
{
	// YCbCr formats only support a subset of format feature flags
	const VkFormatFeatureFlags ycbcrAllows =
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_IMG |
		VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
		VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
		VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
		VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT |
		VK_FORMAT_FEATURE_DISJOINT_BIT;

	// By default everything is allowed.
	VkFormatFeatureFlags allow = (VkFormatFeatureFlags)~0u;
	// Formats for which SamplerYCbCrConversion is required may not support certain features.
	if (requiresYCbCrConversion(format))
		allow &= ycbcrAllows;
	// single-plane formats *may not* support DISJOINT_BIT
	if (!isYCbCrFormat(format) || getPlaneCount(format) == 1)
		allow &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;

	return allow;
}

VkFormatFeatureFlags getAllowedBufferFeatures (VkFormat format)
{
	// TODO: Do we allow non-buffer flags in the bufferFeatures?
	return requiresYCbCrConversion(format) ? (VkFormatFeatureFlags)0 : (VkFormatFeatureFlags)(~VK_FORMAT_FEATURE_DISJOINT_BIT);
}

tcu::TestStatus formatProperties (Context& context, VkFormat format)
{
	// check if Ycbcr format enums are valid given the version and extensions
	if (isYCbCrFormat(format))
		checkYcbcrApiSupport(context);

	TestLog&					log			= context.getTestContext().getLog();
	const VkFormatProperties	properties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
	bool						allOk		= true;

	const VkFormatFeatureFlags reqImg	= getRequiredOptimalTilingFeatures(context, format);
	const VkFormatFeatureFlags reqBuf	= getRequiredBufferFeatures(format);
	const VkFormatFeatureFlags allowImg	= getAllowedOptimalTilingFeatures(format);
	const VkFormatFeatureFlags allowBuf	= getAllowedBufferFeatures(format);

	const struct feature_req
	{
		const char*				fieldName;
		VkFormatFeatureFlags	supportedFeatures;
		VkFormatFeatureFlags	requiredFeatures;
		VkFormatFeatureFlags	allowedFeatures;
	} fields[] =
	{
		{ "linearTilingFeatures",	properties.linearTilingFeatures,	(VkFormatFeatureFlags)0,	allowImg },
		{ "optimalTilingFeatures",	properties.optimalTilingFeatures,	reqImg,						allowImg },
		{ "bufferFeatures",			properties.bufferFeatures,			reqBuf,						allowBuf }
	};

	log << TestLog::Message << properties << TestLog::EndMessage;

	for (int fieldNdx = 0; fieldNdx < DE_LENGTH_OF_ARRAY(fields); fieldNdx++)
	{
		const char* const			fieldName	= fields[fieldNdx].fieldName;
		const VkFormatFeatureFlags	supported	= fields[fieldNdx].supportedFeatures;
		const VkFormatFeatureFlags	required	= fields[fieldNdx].requiredFeatures;
		const VkFormatFeatureFlags	allowed		= fields[fieldNdx].allowedFeatures;

		if ((supported & required) != required)
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< "  required: " << getFormatFeatureFlagsStr(required) << "\n  "
									<< "  missing: " << getFormatFeatureFlagsStr(~supported & required)
				<< TestLog::EndMessage;
			allOk = false;
		}

		if ((supported & ~allowed) != 0)
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< "  has: " << getFormatFeatureFlagsStr(supported & ~allowed)
				<< TestLog::EndMessage;
			allOk = false;
		}

		if (((supported & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT) != 0) &&
			((supported & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT) == 0))
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< " supports VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT"
									<< " but not VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT"
				<< TestLog::EndMessage;
			allOk = false;
		}
	}

	if (allOk)
		return tcu::TestStatus::pass("Query and validation passed");
	else
		return tcu::TestStatus::fail("Required features not supported");
}

bool optimalTilingFeaturesSupported (Context& context, VkFormat format, VkFormatFeatureFlags features)
{
	const VkFormatProperties	properties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);

	return (properties.optimalTilingFeatures & features) == features;
}

bool optimalTilingFeaturesSupportedForAll (Context& context, const VkFormat* begin, const VkFormat* end, VkFormatFeatureFlags features)
{
	for (const VkFormat* cur = begin; cur != end; ++cur)
	{
		if (!optimalTilingFeaturesSupported(context, *cur, features))
			return false;
	}

	return true;
}

tcu::TestStatus testDepthStencilSupported (Context& context)
{
	if (!optimalTilingFeaturesSupported(context, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
		!optimalTilingFeaturesSupported(context, VK_FORMAT_D32_SFLOAT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		return tcu::TestStatus::fail("Doesn't support one of VK_FORMAT_X8_D24_UNORM_PACK32 or VK_FORMAT_D32_SFLOAT");

	if (!optimalTilingFeaturesSupported(context, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
		!optimalTilingFeaturesSupported(context, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		return tcu::TestStatus::fail("Doesn't support one of VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT");

	return tcu::TestStatus::pass("Required depth/stencil formats supported");
}

tcu::TestStatus testCompressedFormatsSupported (Context& context)
{
	static const VkFormat s_allBcFormats[] =
	{
		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC2_UNORM_BLOCK,
		VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK,
		VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK,
		VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK,
	};
	static const VkFormat s_allEtc2Formats[] =
	{
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
	};
	static const VkFormat s_allAstcLdrFormats[] =
	{
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
	};

	static const struct
	{
		const char*									setName;
		const char*									featureName;
		const VkBool32 VkPhysicalDeviceFeatures::*	feature;
		const VkFormat*								formatsBegin;
		const VkFormat*								formatsEnd;
	} s_compressedFormatSets[] =
	{
		{ "BC",			"textureCompressionBC",			&VkPhysicalDeviceFeatures::textureCompressionBC,		DE_ARRAY_BEGIN(s_allBcFormats),			DE_ARRAY_END(s_allBcFormats)		},
		{ "ETC2",		"textureCompressionETC2",		&VkPhysicalDeviceFeatures::textureCompressionETC2,		DE_ARRAY_BEGIN(s_allEtc2Formats),		DE_ARRAY_END(s_allEtc2Formats)		},
		{ "ASTC LDR",	"textureCompressionASTC_LDR",	&VkPhysicalDeviceFeatures::textureCompressionASTC_LDR,	DE_ARRAY_BEGIN(s_allAstcLdrFormats),	DE_ARRAY_END(s_allAstcLdrFormats)	},
	};

	TestLog&						log					= context.getTestContext().getLog();
	const VkPhysicalDeviceFeatures&	features			= context.getDeviceFeatures();
	int								numSupportedSets	= 0;
	int								numErrors			= 0;
	int								numWarnings			= 0;

	for (int setNdx = 0; setNdx < DE_LENGTH_OF_ARRAY(s_compressedFormatSets); ++setNdx)
	{
		const char* const			setName				= s_compressedFormatSets[setNdx].setName;
		const char* const			featureName			= s_compressedFormatSets[setNdx].featureName;
		const bool					featureBitSet		= features.*s_compressedFormatSets[setNdx].feature == VK_TRUE;
		const VkFormatFeatureFlags	requiredFeatures	=
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
			VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
		const bool			allSupported	= optimalTilingFeaturesSupportedForAll(context,
																				   s_compressedFormatSets[setNdx].formatsBegin,
																				   s_compressedFormatSets[setNdx].formatsEnd,
																				   requiredFeatures);

		if (featureBitSet && !allSupported)
		{
			log << TestLog::Message << "ERROR: " << featureName << " = VK_TRUE but " << setName << " formats not supported" << TestLog::EndMessage;
			numErrors += 1;
		}
		else if (allSupported && !featureBitSet)
		{
			log << TestLog::Message << "WARNING: " << setName << " formats supported but " << featureName << " = VK_FALSE" << TestLog::EndMessage;
			numWarnings += 1;
		}

		if (featureBitSet)
		{
			log << TestLog::Message << "All " << setName << " formats are supported" << TestLog::EndMessage;
			numSupportedSets += 1;
		}
		else
			log << TestLog::Message << setName << " formats are not supported" << TestLog::EndMessage;
	}

	if (numSupportedSets == 0)
	{
		log << TestLog::Message << "No compressed format sets supported" << TestLog::EndMessage;
		numErrors += 1;
	}

	if (numErrors > 0)
		return tcu::TestStatus::fail("Compressed format support not valid");
	else if (numWarnings > 0)
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Found inconsistencies in compressed format support");
	else
		return tcu::TestStatus::pass("Compressed texture format support is valid");
}

void createFormatTests (tcu::TestCaseGroup* testGroup)
{
	DE_STATIC_ASSERT(VK_FORMAT_UNDEFINED == 0);

	static const struct
	{
		VkFormat	begin;
		VkFormat	end;
	} s_formatRanges[] =
	{
		// core formats
		{ (VkFormat)(VK_FORMAT_UNDEFINED+1),		VK_CORE_FORMAT_LAST										},

		// YCbCr formats
		{ VK_FORMAT_G8B8G8R8_422_UNORM,				(VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM+1)	},

		// YCbCr extended formats
		{ VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,	(VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT+1)	},
	};

	for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
	{
		const VkFormat								rangeBegin		= s_formatRanges[rangeNdx].begin;
		const VkFormat								rangeEnd		= s_formatRanges[rangeNdx].end;

		for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format+1))
		{
			const char* const	enumName	= getFormatName(format);
			const string		caseName	= de::toLower(string(enumName).substr(10));

			addFunctionCase(testGroup, caseName, enumName, formatProperties, format);
		}
	}

	addFunctionCase(testGroup, "depth_stencil",			"",	testDepthStencilSupported);
	addFunctionCase(testGroup, "compressed_formats",	"",	testCompressedFormatsSupported);
}

VkImageUsageFlags getValidImageUsageFlags (const VkFormatFeatureFlags supportedFeatures, const bool useKhrMaintenance1Semantics)
{
	VkImageUsageFlags	flags	= (VkImageUsageFlags)0;

	if (useKhrMaintenance1Semantics)
	{
		if ((supportedFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if ((supportedFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0)
			flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	else
	{
		// If format is supported at all, it must be valid transfer src+dst
		if (supportedFeatures != 0)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	if ((supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
		flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if ((supportedFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0)
		flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	if ((supportedFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if ((supportedFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
		flags |= VK_IMAGE_USAGE_STORAGE_BIT;

	return flags;
}

bool isValidImageUsageFlagCombination (VkImageUsageFlags usage)
{
	if ((usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0)
	{
		const VkImageUsageFlags		allowedFlags	= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		// Only *_ATTACHMENT_BIT flags can be combined with TRANSIENT_ATTACHMENT_BIT
		if ((usage & ~allowedFlags) != 0)
			return false;

		// TRANSIENT_ATTACHMENT_BIT is not valid without COLOR_ or DEPTH_STENCIL_ATTACHMENT_BIT
		if ((usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0)
			return false;
	}

	return usage != 0;
}

VkImageCreateFlags getValidImageCreateFlags (const VkPhysicalDeviceFeatures& deviceFeatures, VkFormat format, VkFormatFeatureFlags formatFeatures, VkImageType type, VkImageUsageFlags usage)
{
	VkImageCreateFlags	flags	= (VkImageCreateFlags)0;

	if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
	{
		flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

		if (type == VK_IMAGE_TYPE_2D && !isYCbCrFormat(format))
		{
			flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}
	}

	if (isYCbCrFormat(format) && getPlaneCount(format) > 1)
	{
		if (formatFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT_KHR)
			flags |= VK_IMAGE_CREATE_DISJOINT_BIT_KHR;
	}

	if ((usage & (VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT)) != 0 &&
		(usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) == 0)
	{
		if (deviceFeatures.sparseBinding)
			flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT|VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

		if (deviceFeatures.sparseResidencyAliased)
			flags |= VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
	}

	return flags;
}

bool isValidImageCreateFlagCombination (VkImageCreateFlags)
{
	return true;
}

bool isRequiredImageParameterCombination (const VkPhysicalDeviceFeatures&	deviceFeatures,
										  const VkFormat					format,
										  const VkFormatProperties&			formatProperties,
										  const VkImageType					imageType,
										  const VkImageTiling				imageTiling,
										  const VkImageUsageFlags			usageFlags,
										  const VkImageCreateFlags			createFlags)
{
	DE_UNREF(deviceFeatures);
	DE_UNREF(formatProperties);
	DE_UNREF(createFlags);

	// Linear images can have arbitrary limitations
	if (imageTiling == VK_IMAGE_TILING_LINEAR)
		return false;

	// Support for other usages for compressed formats is optional
	if (isCompressedFormat(format) &&
		(usageFlags & ~(VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
		return false;

	// Support for 1D, and sliced 3D compressed formats is optional
	if (isCompressedFormat(format) && (imageType == VK_IMAGE_TYPE_1D || imageType == VK_IMAGE_TYPE_3D))
		return false;

	// Support for 1D and 3D depth/stencil textures is optional
	if (isDepthStencilFormat(format) && (imageType == VK_IMAGE_TYPE_1D || imageType == VK_IMAGE_TYPE_3D))
		return false;

	DE_ASSERT(deviceFeatures.sparseBinding || (createFlags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT|VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) == 0);
	DE_ASSERT(deviceFeatures.sparseResidencyAliased || (createFlags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT) == 0);

	if (isYCbCrFormat(format) && (createFlags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)))
		return false;

	if (createFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
	{
		if (isCompressedFormat(format))
			return false;

		if (isDepthStencilFormat(format))
			return false;

		if (!deIsPowerOfTwo32(mapVkFormat(format).getPixelSize()))
			return false;

		switch (imageType)
		{
			case VK_IMAGE_TYPE_2D:
				return (deviceFeatures.sparseResidencyImage2D == VK_TRUE);
			case VK_IMAGE_TYPE_3D:
				return (deviceFeatures.sparseResidencyImage3D == VK_TRUE);
			default:
				return false;
		}
	}

	return true;
}

VkSampleCountFlags getRequiredOptimalTilingSampleCounts (const VkPhysicalDeviceLimits&	deviceLimits,
														 const VkFormat					format,
														 const VkImageUsageFlags		usageFlags)
{
	if (isCompressedFormat(format))
		return VK_SAMPLE_COUNT_1_BIT;

	bool		hasDepthComp	= false;
	bool		hasStencilComp	= false;
	const bool	isYCbCr			= isYCbCrFormat(format);
	if (!isYCbCr)
	{
		const tcu::TextureFormat	tcuFormat		= mapVkFormat(format);
		hasDepthComp	= (tcuFormat.order == tcu::TextureFormat::D || tcuFormat.order == tcu::TextureFormat::DS);
		hasStencilComp	= (tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS);
	}

	const bool						isColorFormat	= !hasDepthComp && !hasStencilComp;
	VkSampleCountFlags				sampleCounts	= ~(VkSampleCountFlags)0;

	DE_ASSERT((hasDepthComp || hasStencilComp) != isColorFormat);

	if ((usageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
		sampleCounts &= deviceLimits.storageImageSampleCounts;

	if ((usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
	{
		if (hasDepthComp)
			sampleCounts &= deviceLimits.sampledImageDepthSampleCounts;

		if (hasStencilComp)
			sampleCounts &= deviceLimits.sampledImageStencilSampleCounts;

		if (isColorFormat)
		{
			if (isYCbCr)
				sampleCounts &= deviceLimits.sampledImageColorSampleCounts;
			else
			{
				const tcu::TextureFormat		tcuFormat	= mapVkFormat(format);
				const tcu::TextureChannelClass	chnClass	= tcu::getTextureChannelClass(tcuFormat.type);

				if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ||
					chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
					sampleCounts &= deviceLimits.sampledImageIntegerSampleCounts;
				else
					sampleCounts &= deviceLimits.sampledImageColorSampleCounts;
			}
		}
	}

	if ((usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
		sampleCounts &= deviceLimits.framebufferColorSampleCounts;

	if ((usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
	{
		if (hasDepthComp)
			sampleCounts &= deviceLimits.framebufferDepthSampleCounts;

		if (hasStencilComp)
			sampleCounts &= deviceLimits.framebufferStencilSampleCounts;
	}

	// If there is no usage flag set that would have corresponding device limit,
	// only VK_SAMPLE_COUNT_1_BIT is required.
	if (sampleCounts == ~(VkSampleCountFlags)0)
		sampleCounts &= VK_SAMPLE_COUNT_1_BIT;

	return sampleCounts;
}

struct ImageFormatPropertyCase
{
	typedef tcu::TestStatus (*Function) (Context& context, const VkFormat format, const VkImageType imageType, const VkImageTiling tiling);

	Function		testFunction;
	VkFormat		format;
	VkImageType		imageType;
	VkImageTiling	tiling;

	ImageFormatPropertyCase (Function testFunction_, VkFormat format_, VkImageType imageType_, VkImageTiling tiling_)
		: testFunction	(testFunction_)
		, format		(format_)
		, imageType		(imageType_)
		, tiling		(tiling_)
	{}

	ImageFormatPropertyCase (void)
		: testFunction	((Function)DE_NULL)
		, format		(VK_FORMAT_UNDEFINED)
		, imageType		(VK_CORE_IMAGE_TYPE_LAST)
		, tiling		(VK_CORE_IMAGE_TILING_LAST)
	{}
};

tcu::TestStatus imageFormatProperties (Context& context, const VkFormat format, const VkImageType imageType, const VkImageTiling tiling)
{
	if (isYCbCrFormat(format))
		// check if Ycbcr format enums are valid given the version and extensions
		checkYcbcrApiSupport(context);

	TestLog&						log					= context.getTestContext().getLog();
	const VkPhysicalDeviceFeatures&	deviceFeatures		= context.getDeviceFeatures();
	const VkPhysicalDeviceLimits&	deviceLimits		= context.getDeviceProperties().limits;
	const VkFormatProperties		formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
	const bool						hasKhrMaintenance1	= context.isDeviceFunctionalitySupported("VK_KHR_maintenance1");

	const VkFormatFeatureFlags		supportedFeatures	= tiling == VK_IMAGE_TILING_LINEAR ? formatProperties.linearTilingFeatures : formatProperties.optimalTilingFeatures;
	const VkImageUsageFlags			usageFlagSet		= getValidImageUsageFlags(supportedFeatures, hasKhrMaintenance1);

	tcu::ResultCollector			results				(log, "ERROR: ");

	if (hasKhrMaintenance1 && (supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
	{
		results.check((supportedFeatures & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) != 0,
					  "A sampled image format must have VK_FORMAT_FEATURE_TRANSFER_SRC_BIT and VK_FORMAT_FEATURE_TRANSFER_DST_BIT format feature flags set");
	}

	if (isYcbcrConversionSupported(context) && (format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR || format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR))
	{
		VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR | VK_FORMAT_FEATURE_TRANSFER_DST_BIT_KHR;
		if (tiling == VK_IMAGE_TILING_OPTIMAL)
			requiredFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT_KHR;

		results.check((supportedFeatures & requiredFeatures) == requiredFeatures,
					  getFormatName(format) + string(" must support ") + de::toString(getFormatFeatureFlagsStr(requiredFeatures)));
	}

	for (VkImageUsageFlags curUsageFlags = 0; curUsageFlags <= usageFlagSet; curUsageFlags++)
	{
		if ((curUsageFlags & ~usageFlagSet) != 0 ||
			!isValidImageUsageFlagCombination(curUsageFlags))
			continue;

		const VkImageCreateFlags	createFlagSet		= getValidImageCreateFlags(deviceFeatures, format, supportedFeatures, imageType, curUsageFlags);

		for (VkImageCreateFlags curCreateFlags = 0; curCreateFlags <= createFlagSet; curCreateFlags++)
		{
			if ((curCreateFlags & ~createFlagSet) != 0 ||
				!isValidImageCreateFlagCombination(curCreateFlags))
				continue;

			const bool				isRequiredCombination	= isRequiredImageParameterCombination(deviceFeatures,
																								  format,
																								  formatProperties,
																								  imageType,
																								  tiling,
																								  curUsageFlags,
																								  curCreateFlags);
			VkImageFormatProperties	properties;
			VkResult				queryResult;

			log << TestLog::Message << "Testing " << getImageTypeStr(imageType) << ", "
									<< getImageTilingStr(tiling) << ", "
									<< getImageUsageFlagsStr(curUsageFlags) << ", "
									<< getImageCreateFlagsStr(curCreateFlags)
				<< TestLog::EndMessage;

			// Set return value to known garbage
			deMemset(&properties, 0xcd, sizeof(properties));

			queryResult = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(),
																								format,
																								imageType,
																								tiling,
																								curUsageFlags,
																								curCreateFlags,
																								&properties);

			if (queryResult == VK_SUCCESS)
			{
				const deUint32	fullMipPyramidSize	= de::max(de::max(deLog2Floor32(properties.maxExtent.width),
																	  deLog2Floor32(properties.maxExtent.height)),
															  deLog2Floor32(properties.maxExtent.depth)) + 1;

				log << TestLog::Message << properties << "\n" << TestLog::EndMessage;

				results.check(imageType != VK_IMAGE_TYPE_1D || (properties.maxExtent.width >= 1 && properties.maxExtent.height == 1 && properties.maxExtent.depth == 1), "Invalid dimensions for 1D image");
				results.check(imageType != VK_IMAGE_TYPE_2D || (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 && properties.maxExtent.depth == 1), "Invalid dimensions for 2D image");
				results.check(imageType != VK_IMAGE_TYPE_3D || (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 && properties.maxExtent.depth >= 1), "Invalid dimensions for 3D image");
				results.check(imageType != VK_IMAGE_TYPE_3D || properties.maxArrayLayers == 1, "Invalid maxArrayLayers for 3D image");

				if (tiling == VK_IMAGE_TILING_OPTIMAL && imageType == VK_IMAGE_TYPE_2D && !(curCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
					 (supportedFeatures & (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)))
				{
					const VkSampleCountFlags	requiredSampleCounts	= getRequiredOptimalTilingSampleCounts(deviceLimits, format, curUsageFlags);
					results.check((properties.sampleCounts & requiredSampleCounts) == requiredSampleCounts, "Required sample counts not supported");
				}
				else
					results.check(properties.sampleCounts == VK_SAMPLE_COUNT_1_BIT, "sampleCounts != VK_SAMPLE_COUNT_1_BIT");

				if (isRequiredCombination)
				{
					results.check(imageType != VK_IMAGE_TYPE_1D || (properties.maxExtent.width	>= deviceLimits.maxImageDimension1D),
								  "Reported dimensions smaller than device limits");
					results.check(imageType != VK_IMAGE_TYPE_2D || (properties.maxExtent.width	>= deviceLimits.maxImageDimension2D &&
																	properties.maxExtent.height	>= deviceLimits.maxImageDimension2D),
								  "Reported dimensions smaller than device limits");
					results.check(imageType != VK_IMAGE_TYPE_3D || (properties.maxExtent.width	>= deviceLimits.maxImageDimension3D &&
																	properties.maxExtent.height	>= deviceLimits.maxImageDimension3D &&
																	properties.maxExtent.depth	>= deviceLimits.maxImageDimension3D),
								  "Reported dimensions smaller than device limits");
					results.check((isYCbCrFormat(format) && (properties.maxMipLevels == 1)) || properties.maxMipLevels == fullMipPyramidSize,
					              "Invalid mip pyramid size");
					results.check((isYCbCrFormat(format) && (properties.maxArrayLayers == 1)) || imageType == VK_IMAGE_TYPE_3D ||
					              properties.maxArrayLayers >= deviceLimits.maxImageArrayLayers, "Invalid maxArrayLayers");
				}
				else
				{
					results.check(properties.maxMipLevels == 1 || properties.maxMipLevels == fullMipPyramidSize, "Invalid mip pyramid size");
					results.check(properties.maxArrayLayers >= 1, "Invalid maxArrayLayers");
				}

				results.check(properties.maxResourceSize >= (VkDeviceSize)MINIMUM_REQUIRED_IMAGE_RESOURCE_SIZE,
							  "maxResourceSize smaller than minimum required size");
			}
			else if (queryResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
			{
				log << TestLog::Message << "Got VK_ERROR_FORMAT_NOT_SUPPORTED" << TestLog::EndMessage;

				if (isRequiredCombination)
					results.fail("VK_ERROR_FORMAT_NOT_SUPPORTED returned for required image parameter combination");

				// Specification requires that all fields are set to 0
				results.check(properties.maxExtent.width	== 0, "maxExtent.width != 0");
				results.check(properties.maxExtent.height	== 0, "maxExtent.height != 0");
				results.check(properties.maxExtent.depth	== 0, "maxExtent.depth != 0");
				results.check(properties.maxMipLevels		== 0, "maxMipLevels != 0");
				results.check(properties.maxArrayLayers		== 0, "maxArrayLayers != 0");
				results.check(properties.sampleCounts		== 0, "sampleCounts != 0");
				results.check(properties.maxResourceSize	== 0, "maxResourceSize != 0");
			}
			else
			{
				results.fail("Got unexpected error" + de::toString(queryResult));
			}
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

// VK_KHR_get_physical_device_properties2

string toString(const VkPhysicalDevicePCIBusInfoPropertiesEXT& value)
{
	std::ostringstream  s;
	s << "VkPhysicalDevicePCIBusInfoPropertiesEXT = {\n";
	s << "\tsType = " << value.sType << '\n';
	s << "\tpciDomain = " << value.pciDomain << '\n';
	s << "\tpciBus = " << value.pciBus << '\n';
	s << "\tpciDevice = " << value.pciDevice << '\n';
	s << "\tpciFunction = " << value.pciFunction << '\n';
	s << '}';
	return s.str();
}

bool checkExtension (vector<VkExtensionProperties>& properties, const char* extension)
{
	for (size_t ndx = 0; ndx < properties.size(); ++ndx)
	{
		if (strncmp(properties[ndx].extensionName, extension, VK_MAX_EXTENSION_NAME_SIZE) == 0)
			return true;
	}
	return false;
}

tcu::TestStatus deviceFeatures2 (Context& context)
{
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const CustomInstance		instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&		vki				(instance.getDriver());
	const int					count			= 2u;
	TestLog&					log				= context.getTestContext().getLog();
	VkPhysicalDeviceFeatures	coreFeatures;
	VkPhysicalDeviceFeatures2	extFeatures;

	deMemset(&coreFeatures, 0xcd, sizeof(coreFeatures));
	deMemset(&extFeatures.features, 0xcd, sizeof(extFeatures.features));
	std::vector<std::string> instExtensions = context.getInstanceExtensions();

	extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	extFeatures.pNext = DE_NULL;

	vki.getPhysicalDeviceFeatures(physicalDevice, &coreFeatures);
	vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

	TCU_CHECK(extFeatures.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
	TCU_CHECK(extFeatures.pNext == DE_NULL);

	if (deMemCmp(&coreFeatures, &extFeatures.features, sizeof(VkPhysicalDeviceFeatures)) != 0)
		TCU_FAIL("Mismatch between features reported by vkGetPhysicalDeviceFeatures and vkGetPhysicalDeviceFeatures2");

	log << TestLog::Message << extFeatures << TestLog::EndMessage;

	vector<VkExtensionProperties> properties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

#include "vkDeviceFeatures2.inl"

	return tcu::TestStatus::pass("Querying device features succeeded");
}

tcu::TestStatus deviceProperties2 (Context& context)
{
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const CustomInstance			instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki				(instance.getDriver());
	TestLog&						log				= context.getTestContext().getLog();
	VkPhysicalDeviceProperties		coreProperties;
	VkPhysicalDeviceProperties2		extProperties;

	extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	extProperties.pNext = DE_NULL;

	vki.getPhysicalDeviceProperties(physicalDevice, &coreProperties);
	vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

	TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
	TCU_CHECK(extProperties.pNext == DE_NULL);

	// We can't use memcmp() here because the structs may contain padding bytes that drivers may or may not
	// have written while writing the data and memcmp will compare them anyway, so we iterate through the
	// valid bytes for each field in the struct and compare only the valid bytes for each one.
	for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(s_physicalDevicePropertiesOffsetTable); propNdx++)
	{
		const size_t offset					= s_physicalDevicePropertiesOffsetTable[propNdx].offset;
		const size_t size					= s_physicalDevicePropertiesOffsetTable[propNdx].size;

		const deUint8* corePropertyBytes	= reinterpret_cast<deUint8*>(&coreProperties) + offset;
		const deUint8* extPropertyBytes		= reinterpret_cast<deUint8*>(&extProperties.properties) + offset;

		if (deMemCmp(corePropertyBytes, extPropertyBytes, size) != 0)
			TCU_FAIL("Mismatch between properties reported by vkGetPhysicalDeviceProperties and vkGetPhysicalDeviceProperties2");
	}

	log << TestLog::Message << extProperties.properties << TestLog::EndMessage;

	const int count = 2u;

	vector<VkExtensionProperties> properties		= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
	const bool khr_external_fence_capabilities		= checkExtension(properties, "VK_KHR_external_fence_capabilities")		||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_external_memory_capabilities		= checkExtension(properties, "VK_KHR_external_memory_capabilities")		||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_external_semaphore_capabilities	= checkExtension(properties, "VK_KHR_external_semaphore_capabilities")	||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_multiview						= checkExtension(properties, "VK_KHR_multiview")						||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_device_protected_memory			=																			context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_device_subgroup					=																			context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_maintenance2						= checkExtension(properties, "VK_KHR_maintenance2")						||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_maintenance3						= checkExtension(properties, "VK_KHR_maintenance3")						||	context.contextSupports(vk::ApiVersion(1, 1, 0));
	const bool khr_depth_stencil_resolve			= checkExtension(properties, "VK_KHR_depth_stencil_resolve")			||	context.contextSupports(vk::ApiVersion(1, 2, 0));
	const bool khr_driver_properties				= checkExtension(properties, "VK_KHR_driver_properties")				||	context.contextSupports(vk::ApiVersion(1, 2, 0));
	const bool khr_shader_float_controls			= checkExtension(properties, "VK_KHR_shader_float_controls")			||	context.contextSupports(vk::ApiVersion(1, 2, 0));
	const bool khr_descriptor_indexing				= checkExtension(properties, "VK_EXT_descriptor_indexing")				||	context.contextSupports(vk::ApiVersion(1, 2, 0));
	const bool khr_sampler_filter_minmax			= checkExtension(properties, "VK_EXT_sampler_filter_minmax")			||	context.contextSupports(vk::ApiVersion(1, 2, 0));
	const bool khr_integer_dot_product				= checkExtension(properties, "VK_KHR_shader_integer_dot_product");

	VkPhysicalDeviceIDProperties							idProperties[count];
	VkPhysicalDeviceMultiviewProperties						multiviewProperties[count];
	VkPhysicalDeviceProtectedMemoryProperties				protectedMemoryPropertiesKHR[count];
	VkPhysicalDeviceSubgroupProperties						subgroupProperties[count];
	VkPhysicalDevicePointClippingProperties					pointClippingProperties[count];
	VkPhysicalDeviceMaintenance3Properties					maintenance3Properties[count];
	VkPhysicalDeviceDepthStencilResolveProperties			depthStencilResolveProperties[count];
	VkPhysicalDeviceDriverProperties						driverProperties[count];
	VkPhysicalDeviceFloatControlsProperties					floatControlsProperties[count];
	VkPhysicalDeviceDescriptorIndexingProperties			descriptorIndexingProperties[count];
	VkPhysicalDeviceSamplerFilterMinmaxProperties			samplerFilterMinmaxProperties[count];
	VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR	integerDotProductProperties[count];

	for (int ndx = 0; ndx < count; ++ndx)
	{
		deMemset(&idProperties[ndx],					0xFF*ndx, sizeof(VkPhysicalDeviceIDProperties					));
		deMemset(&multiviewProperties[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceMultiviewProperties			));
		deMemset(&protectedMemoryPropertiesKHR[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceProtectedMemoryProperties		));
		deMemset(&subgroupProperties[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceSubgroupProperties				));
		deMemset(&pointClippingProperties[ndx],			0xFF*ndx, sizeof(VkPhysicalDevicePointClippingProperties		));
		deMemset(&maintenance3Properties[ndx],			0xFF*ndx, sizeof(VkPhysicalDeviceMaintenance3Properties			));
		deMemset(&depthStencilResolveProperties[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceDepthStencilResolveProperties	));
		deMemset(&driverProperties[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceDriverProperties				));
		deMemset(&floatControlsProperties[ndx],			0xFF*ndx, sizeof(VkPhysicalDeviceFloatControlsProperties		));
		deMemset(&descriptorIndexingProperties[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceDescriptorIndexingProperties	));
		deMemset(&samplerFilterMinmaxProperties[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceSamplerFilterMinmaxProperties	));
		deMemset(&integerDotProductProperties[ndx],		0xFF*ndx, sizeof(VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR	));

		idProperties[ndx].sType						= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
		idProperties[ndx].pNext						= &multiviewProperties[ndx];

		multiviewProperties[ndx].sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
		multiviewProperties[ndx].pNext				= &protectedMemoryPropertiesKHR[ndx];

		protectedMemoryPropertiesKHR[ndx].sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
		protectedMemoryPropertiesKHR[ndx].pNext		= &subgroupProperties[ndx];

		subgroupProperties[ndx].sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties[ndx].pNext				= &pointClippingProperties[ndx];

		pointClippingProperties[ndx].sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
		pointClippingProperties[ndx].pNext			= &maintenance3Properties[ndx];

		maintenance3Properties[ndx].sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
		maintenance3Properties[ndx].pNext			= &depthStencilResolveProperties[ndx];

		depthStencilResolveProperties[ndx].sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
		depthStencilResolveProperties[ndx].pNext	= &driverProperties[ndx];

		driverProperties[ndx].sType					= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
		driverProperties[ndx].pNext					= &floatControlsProperties[ndx];

		floatControlsProperties[ndx].sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;
		floatControlsProperties[ndx].pNext			= &descriptorIndexingProperties[ndx];

		descriptorIndexingProperties[ndx].sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
		descriptorIndexingProperties[ndx].pNext		= &samplerFilterMinmaxProperties[ndx];

		samplerFilterMinmaxProperties[ndx].sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
		samplerFilterMinmaxProperties[ndx].pNext	= &integerDotProductProperties[ndx];

		integerDotProductProperties[ndx].sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES_KHR;
		integerDotProductProperties[ndx].pNext		= DE_NULL;

		extProperties.pNext							= &idProperties[ndx];

		vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
	}

	if ( khr_external_fence_capabilities || khr_external_memory_capabilities || khr_external_semaphore_capabilities )
		log << TestLog::Message << idProperties[0]					<< TestLog::EndMessage;
	if (khr_multiview)
		log << TestLog::Message << multiviewProperties[0]			<< TestLog::EndMessage;
	if (khr_device_protected_memory)
		log << TestLog::Message << protectedMemoryPropertiesKHR[0]	<< TestLog::EndMessage;
	if (khr_device_subgroup)
		log << TestLog::Message << subgroupProperties[0]			<< TestLog::EndMessage;
	if (khr_maintenance2)
		log << TestLog::Message << pointClippingProperties[0]		<< TestLog::EndMessage;
	if (khr_maintenance3)
		log << TestLog::Message << maintenance3Properties[0]		<< TestLog::EndMessage;
	if (khr_depth_stencil_resolve)
		log << TestLog::Message << depthStencilResolveProperties[0] << TestLog::EndMessage;
	if (khr_driver_properties)
		log << TestLog::Message << driverProperties[0]				<< TestLog::EndMessage;
	if (khr_shader_float_controls)
		log << TestLog::Message << floatControlsProperties[0]		<< TestLog::EndMessage;
	if (khr_descriptor_indexing)
		log << TestLog::Message << descriptorIndexingProperties[0] << TestLog::EndMessage;
	if (khr_sampler_filter_minmax)
		log << TestLog::Message << samplerFilterMinmaxProperties[0] << TestLog::EndMessage;
	if (khr_integer_dot_product)
		log << TestLog::Message << integerDotProductProperties[0] << TestLog::EndMessage;

	if ( khr_external_fence_capabilities || khr_external_memory_capabilities || khr_external_semaphore_capabilities )
	{
		if ((deMemCmp(idProperties[0].deviceUUID, idProperties[1].deviceUUID, VK_UUID_SIZE) != 0) ||
			(deMemCmp(idProperties[0].driverUUID, idProperties[1].driverUUID, VK_UUID_SIZE) != 0) ||
			(idProperties[0].deviceLUIDValid	!= idProperties[1].deviceLUIDValid))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
		}
		else if (idProperties[0].deviceLUIDValid)
		{
			// If deviceLUIDValid is VK_FALSE, the contents of deviceLUID and deviceNodeMask are undefined
			// so thay can only be compared when deviceLUIDValid is VK_TRUE.
			if ((deMemCmp(idProperties[0].deviceLUID, idProperties[1].deviceLUID, VK_UUID_SIZE) != 0) ||
				(idProperties[0].deviceNodeMask		!= idProperties[1].deviceNodeMask))
			{
				TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
			}
		}
	}
	if (khr_multiview &&
		(multiviewProperties[0].maxMultiviewViewCount		!= multiviewProperties[1].maxMultiviewViewCount ||
		 multiviewProperties[0].maxMultiviewInstanceIndex	!= multiviewProperties[1].maxMultiviewInstanceIndex))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewProperties");
	}
	if (khr_device_protected_memory &&
		(protectedMemoryPropertiesKHR[0].protectedNoFault	!= protectedMemoryPropertiesKHR[1].protectedNoFault))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryProperties");
	}
	if (khr_device_subgroup &&
		(subgroupProperties[0].subgroupSize					!= subgroupProperties[1].subgroupSize ||
		 subgroupProperties[0].supportedStages				!= subgroupProperties[1].supportedStages ||
		 subgroupProperties[0].supportedOperations			!= subgroupProperties[1].supportedOperations ||
		 subgroupProperties[0].quadOperationsInAllStages	!= subgroupProperties[1].quadOperationsInAllStages ))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties");
	}
	if (khr_maintenance2 &&
		(pointClippingProperties[0].pointClippingBehavior	!= pointClippingProperties[1].pointClippingBehavior))
	{
		TCU_FAIL("Mismatch between VkPhysicalDevicePointClippingProperties");
	}
	if (khr_maintenance3 &&
		(maintenance3Properties[0].maxPerSetDescriptors		!= maintenance3Properties[1].maxPerSetDescriptors ||
		 maintenance3Properties[0].maxMemoryAllocationSize	!= maintenance3Properties[1].maxMemoryAllocationSize))
	{
		if (protectedMemoryPropertiesKHR[0].protectedNoFault != protectedMemoryPropertiesKHR[1].protectedNoFault)
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryProperties");
		}
		if ((subgroupProperties[0].subgroupSize					!= subgroupProperties[1].subgroupSize) ||
			(subgroupProperties[0].supportedStages				!= subgroupProperties[1].supportedStages) ||
			(subgroupProperties[0].supportedOperations			!= subgroupProperties[1].supportedOperations) ||
			(subgroupProperties[0].quadOperationsInAllStages	!= subgroupProperties[1].quadOperationsInAllStages))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties");
		}
		TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance3Properties");
	}
	if (khr_depth_stencil_resolve &&
		(depthStencilResolveProperties[0].supportedDepthResolveModes	!= depthStencilResolveProperties[1].supportedDepthResolveModes ||
		 depthStencilResolveProperties[0].supportedStencilResolveModes	!= depthStencilResolveProperties[1].supportedStencilResolveModes ||
		 depthStencilResolveProperties[0].independentResolveNone		!= depthStencilResolveProperties[1].independentResolveNone ||
		 depthStencilResolveProperties[0].independentResolve			!= depthStencilResolveProperties[1].independentResolve))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceDepthStencilResolveProperties");
	}
	if (khr_driver_properties &&
		(driverProperties[0].driverID												!= driverProperties[1].driverID ||
		 strncmp(driverProperties[0].driverName, driverProperties[1].driverName, VK_MAX_DRIVER_NAME_SIZE)	!= 0 ||
		 strncmp(driverProperties[0].driverInfo, driverProperties[1].driverInfo, VK_MAX_DRIVER_INFO_SIZE)		!= 0 ||
		 driverProperties[0].conformanceVersion.major								!= driverProperties[1].conformanceVersion.major ||
		 driverProperties[0].conformanceVersion.minor								!= driverProperties[1].conformanceVersion.minor ||
		 driverProperties[0].conformanceVersion.subminor							!= driverProperties[1].conformanceVersion.subminor ||
		 driverProperties[0].conformanceVersion.patch								!= driverProperties[1].conformanceVersion.patch))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceDriverProperties");
	}
	if (khr_shader_float_controls &&
		(floatControlsProperties[0].denormBehaviorIndependence				!= floatControlsProperties[1].denormBehaviorIndependence ||
		 floatControlsProperties[0].roundingModeIndependence				!= floatControlsProperties[1].roundingModeIndependence ||
		 floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat16	!= floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat16 ||
		 floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat32	!= floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat32 ||
		 floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat64	!= floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat64 ||
		 floatControlsProperties[0].shaderDenormPreserveFloat16				!= floatControlsProperties[1].shaderDenormPreserveFloat16 ||
		 floatControlsProperties[0].shaderDenormPreserveFloat32				!= floatControlsProperties[1].shaderDenormPreserveFloat32 ||
		 floatControlsProperties[0].shaderDenormPreserveFloat64				!= floatControlsProperties[1].shaderDenormPreserveFloat64 ||
		 floatControlsProperties[0].shaderDenormFlushToZeroFloat16			!= floatControlsProperties[1].shaderDenormFlushToZeroFloat16 ||
		 floatControlsProperties[0].shaderDenormFlushToZeroFloat32			!= floatControlsProperties[1].shaderDenormFlushToZeroFloat32 ||
		 floatControlsProperties[0].shaderDenormFlushToZeroFloat64			!= floatControlsProperties[1].shaderDenormFlushToZeroFloat64 ||
		 floatControlsProperties[0].shaderRoundingModeRTEFloat16			!= floatControlsProperties[1].shaderRoundingModeRTEFloat16 ||
		 floatControlsProperties[0].shaderRoundingModeRTEFloat32			!= floatControlsProperties[1].shaderRoundingModeRTEFloat32 ||
		 floatControlsProperties[0].shaderRoundingModeRTEFloat64			!= floatControlsProperties[1].shaderRoundingModeRTEFloat64 ||
		 floatControlsProperties[0].shaderRoundingModeRTZFloat16			!= floatControlsProperties[1].shaderRoundingModeRTZFloat16 ||
		 floatControlsProperties[0].shaderRoundingModeRTZFloat32			!= floatControlsProperties[1].shaderRoundingModeRTZFloat32 ||
		 floatControlsProperties[0].shaderRoundingModeRTZFloat64			!= floatControlsProperties[1].shaderRoundingModeRTZFloat64 ))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceFloatControlsProperties");
	}
	if (khr_descriptor_indexing &&
		(descriptorIndexingProperties[0].maxUpdateAfterBindDescriptorsInAllPools				!= descriptorIndexingProperties[1].maxUpdateAfterBindDescriptorsInAllPools ||
		 descriptorIndexingProperties[0].shaderUniformBufferArrayNonUniformIndexingNative		!= descriptorIndexingProperties[1].shaderUniformBufferArrayNonUniformIndexingNative ||
		 descriptorIndexingProperties[0].shaderSampledImageArrayNonUniformIndexingNative		!= descriptorIndexingProperties[1].shaderSampledImageArrayNonUniformIndexingNative ||
		 descriptorIndexingProperties[0].shaderStorageBufferArrayNonUniformIndexingNative		!= descriptorIndexingProperties[1].shaderStorageBufferArrayNonUniformIndexingNative ||
		 descriptorIndexingProperties[0].shaderStorageImageArrayNonUniformIndexingNative		!= descriptorIndexingProperties[1].shaderStorageImageArrayNonUniformIndexingNative ||
		 descriptorIndexingProperties[0].shaderInputAttachmentArrayNonUniformIndexingNative		!= descriptorIndexingProperties[1].shaderInputAttachmentArrayNonUniformIndexingNative ||
		 descriptorIndexingProperties[0].robustBufferAccessUpdateAfterBind						!= descriptorIndexingProperties[1].robustBufferAccessUpdateAfterBind ||
		 descriptorIndexingProperties[0].quadDivergentImplicitLod								!= descriptorIndexingProperties[1].quadDivergentImplicitLod ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindSamplers			!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindSamplers ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindUniformBuffers		!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindUniformBuffers ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindStorageBuffers		!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindStorageBuffers ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindSampledImages		!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindSampledImages ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindStorageImages		!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindStorageImages ||
		 descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindInputAttachments	!= descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindInputAttachments ||
		 descriptorIndexingProperties[0].maxPerStageUpdateAfterBindResources					!= descriptorIndexingProperties[1].maxPerStageUpdateAfterBindResources ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindSamplers				!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindSamplers ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindUniformBuffers			!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindUniformBuffers ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindUniformBuffersDynamic	!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindUniformBuffersDynamic ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageBuffers			!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageBuffers ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageBuffersDynamic	!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageBuffersDynamic ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindSampledImages			!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindSampledImages ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageImages			!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageImages ||
		 descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindInputAttachments		!= descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindInputAttachments ))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceDescriptorIndexingProperties");
	}
	if (khr_sampler_filter_minmax &&
		(samplerFilterMinmaxProperties[0].filterMinmaxSingleComponentFormats	!= samplerFilterMinmaxProperties[1].filterMinmaxSingleComponentFormats ||
		 samplerFilterMinmaxProperties[0].filterMinmaxImageComponentMapping		!= samplerFilterMinmaxProperties[1].filterMinmaxImageComponentMapping))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerFilterMinmaxProperties");
	}

	if (khr_integer_dot_product &&
		(integerDotProductProperties[0].integerDotProduct8BitUnsignedAccelerated										!= integerDotProductProperties[1].integerDotProduct8BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct8BitSignedAccelerated											!= integerDotProductProperties[1].integerDotProduct8BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct8BitMixedSignednessAccelerated									!= integerDotProductProperties[1].integerDotProduct8BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProduct4x8BitPackedUnsignedAccelerated								!= integerDotProductProperties[1].integerDotProduct4x8BitPackedUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct4x8BitPackedSignedAccelerated									!= integerDotProductProperties[1].integerDotProduct4x8BitPackedSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct4x8BitPackedMixedSignednessAccelerated							!= integerDotProductProperties[1].integerDotProduct4x8BitPackedMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProduct16BitUnsignedAccelerated										!= integerDotProductProperties[1].integerDotProduct16BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct16BitSignedAccelerated											!= integerDotProductProperties[1].integerDotProduct16BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct16BitMixedSignednessAccelerated								!= integerDotProductProperties[1].integerDotProduct16BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProduct32BitUnsignedAccelerated										!= integerDotProductProperties[1].integerDotProduct32BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct32BitSignedAccelerated											!= integerDotProductProperties[1].integerDotProduct32BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct32BitMixedSignednessAccelerated								!= integerDotProductProperties[1].integerDotProduct32BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProduct64BitUnsignedAccelerated										!= integerDotProductProperties[1].integerDotProduct64BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct64BitSignedAccelerated											!= integerDotProductProperties[1].integerDotProduct64BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProduct64BitMixedSignednessAccelerated								!= integerDotProductProperties[1].integerDotProduct64BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitUnsignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitSignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated	!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitUnsignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitSignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitUnsignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitSignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitUnsignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitUnsignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitSignedAccelerated					!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitSignedAccelerated ||
		 integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated			!= integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR");
	}

	if (isExtensionSupported(properties, RequiredExtension("VK_KHR_push_descriptor")))
	{
		VkPhysicalDevicePushDescriptorPropertiesKHR		pushDescriptorProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			deMemset(&pushDescriptorProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDevicePushDescriptorPropertiesKHR));

			pushDescriptorProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
			pushDescriptorProperties[ndx].pNext	= DE_NULL;

			extProperties.pNext = &pushDescriptorProperties[ndx];

			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

			pushDescriptorProperties[ndx].pNext = DE_NULL;
		}

		log << TestLog::Message << pushDescriptorProperties[0] << TestLog::EndMessage;

		if ( pushDescriptorProperties[0].maxPushDescriptors != pushDescriptorProperties[1].maxPushDescriptors )
		{
			TCU_FAIL("Mismatch between VkPhysicalDevicePushDescriptorPropertiesKHR ");
		}
		if (pushDescriptorProperties[0].maxPushDescriptors < 32)
		{
			TCU_FAIL("VkPhysicalDevicePushDescriptorPropertiesKHR.maxPushDescriptors must be at least 32");
		}
	}

	if (isExtensionSupported(properties, RequiredExtension("VK_KHR_performance_query")))
	{
		VkPhysicalDevicePerformanceQueryPropertiesKHR performanceQueryProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			deMemset(&performanceQueryProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDevicePerformanceQueryPropertiesKHR));
			performanceQueryProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR;
			performanceQueryProperties[ndx].pNext = DE_NULL;

			extProperties.pNext = &performanceQueryProperties[ndx];

			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
		}

		log << TestLog::Message << performanceQueryProperties[0] << TestLog::EndMessage;

		if (performanceQueryProperties[0].allowCommandBufferQueryCopies != performanceQueryProperties[0].allowCommandBufferQueryCopies)
		{
			TCU_FAIL("Mismatch between VkPhysicalDevicePerformanceQueryPropertiesKHR");
		}
	}

	if (isExtensionSupported(properties, RequiredExtension("VK_EXT_pci_bus_info", 2, 2)))
	{
		VkPhysicalDevicePCIBusInfoPropertiesEXT pciBusInfoProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			// Each PCI device is identified by an 8-bit domain number, 5-bit
			// device number and 3-bit function number[1][2].
			//
			// In addition, because PCI systems can be interconnected and
			// divided in segments, Linux assigns a 16-bit number to the device
			// as the "domain". In Windows, the segment or domain is stored in
			// the higher 24-bit section of the bus number.
			//
			// This means the maximum unsigned 32-bit integer for these members
			// are invalid values and should change after querying properties.
			//
			// [1] https://en.wikipedia.org/wiki/PCI_configuration_space
			// [2] PCI Express Base Specification Revision 3.0, section 2.2.4.2.
			deMemset(pciBusInfoProperties + ndx, 0xFF * ndx, sizeof(pciBusInfoProperties[ndx]));
			pciBusInfoProperties[ndx].pciDomain   = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciBus      = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciDevice   = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciFunction = DEUINT32_MAX;

			pciBusInfoProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT;
			pciBusInfoProperties[ndx].pNext = DE_NULL;

			extProperties.pNext = pciBusInfoProperties + ndx;
			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
		}

		log << TestLog::Message << toString(pciBusInfoProperties[0]) << TestLog::EndMessage;

		if (pciBusInfoProperties[0].pciDomain	!= pciBusInfoProperties[1].pciDomain ||
			pciBusInfoProperties[0].pciBus		!= pciBusInfoProperties[1].pciBus ||
			pciBusInfoProperties[0].pciDevice	!= pciBusInfoProperties[1].pciDevice ||
			pciBusInfoProperties[0].pciFunction	!= pciBusInfoProperties[1].pciFunction)
		{
			TCU_FAIL("Mismatch between VkPhysicalDevicePCIBusInfoPropertiesEXT");
		}
		if (pciBusInfoProperties[0].pciDomain   == DEUINT32_MAX ||
		    pciBusInfoProperties[0].pciBus      == DEUINT32_MAX ||
		    pciBusInfoProperties[0].pciDevice   == DEUINT32_MAX ||
		    pciBusInfoProperties[0].pciFunction == DEUINT32_MAX)
		{
			TCU_FAIL("Invalid information in VkPhysicalDevicePCIBusInfoPropertiesEXT");
		}
	}

	return tcu::TestStatus::pass("Querying device properties succeeded");
}

string toString (const VkFormatProperties2& value)
{
	std::ostringstream	s;
	s << "VkFormatProperties2 = {\n";
	s << "\tsType = " << value.sType << '\n';
	s << "\tformatProperties = {\n";
	s << "\tlinearTilingFeatures = " << getFormatFeatureFlagsStr(value.formatProperties.linearTilingFeatures) << '\n';
	s << "\toptimalTilingFeatures = " << getFormatFeatureFlagsStr(value.formatProperties.optimalTilingFeatures) << '\n';
	s << "\tbufferFeatures = " << getFormatFeatureFlagsStr(value.formatProperties.bufferFeatures) << '\n';
	s << "\t}";
	s << "}";
	return s.str();
}

tcu::TestStatus deviceFormatProperties2 (Context& context)
{
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const CustomInstance			instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki				(instance.getDriver());
	TestLog&						log				= context.getTestContext().getLog();

	for (int formatNdx = 0; formatNdx < VK_CORE_FORMAT_LAST; ++formatNdx)
	{
		const VkFormat			format			= (VkFormat)formatNdx;
		VkFormatProperties		coreProperties;
		VkFormatProperties2		extProperties;

		deMemset(&coreProperties, 0xcd, sizeof(VkFormatProperties));
		deMemset(&extProperties, 0xcd, sizeof(VkFormatProperties2));

		extProperties.sType	= VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		extProperties.pNext = DE_NULL;

		vki.getPhysicalDeviceFormatProperties(physicalDevice, format, &coreProperties);
		vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &extProperties);

		TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
		TCU_CHECK(extProperties.pNext == DE_NULL);

	if (deMemCmp(&coreProperties, &extProperties.formatProperties, sizeof(VkFormatProperties)) != 0)
		TCU_FAIL("Mismatch between format properties reported by vkGetPhysicalDeviceFormatProperties and vkGetPhysicalDeviceFormatProperties2");

	log << TestLog::Message << toString (extProperties) << TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("Querying device format properties succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties2 (Context& context)
{
	const VkPhysicalDevice			physicalDevice			= context.getPhysicalDevice();
	const CustomInstance			instance				(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki						(instance.getDriver());
	TestLog&						log						= context.getTestContext().getLog();
	deUint32						numCoreQueueFamilies	= ~0u;
	deUint32						numExtQueueFamilies		= ~0u;

	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numCoreQueueFamilies, DE_NULL);
	vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numExtQueueFamilies, DE_NULL);

	TCU_CHECK_MSG(numCoreQueueFamilies == numExtQueueFamilies, "Different number of queue family properties reported");
	TCU_CHECK(numCoreQueueFamilies > 0);

	{
		std::vector<VkQueueFamilyProperties>		coreProperties	(numCoreQueueFamilies);
		std::vector<VkQueueFamilyProperties2>		extProperties	(numExtQueueFamilies);

		deMemset(&coreProperties[0], 0xcd, sizeof(VkQueueFamilyProperties)*numCoreQueueFamilies);
		deMemset(&extProperties[0], 0xcd, sizeof(VkQueueFamilyProperties2)*numExtQueueFamilies);

		for (size_t ndx = 0; ndx < extProperties.size(); ++ndx)
		{
			extProperties[ndx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
			extProperties[ndx].pNext = DE_NULL;
		}

		vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numCoreQueueFamilies, &coreProperties[0]);
		vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numExtQueueFamilies, &extProperties[0]);

		TCU_CHECK((size_t)numCoreQueueFamilies == coreProperties.size());
		TCU_CHECK((size_t)numExtQueueFamilies == extProperties.size());
		DE_ASSERT(numCoreQueueFamilies == numExtQueueFamilies);

		for (size_t ndx = 0; ndx < extProperties.size(); ++ndx)
		{
			TCU_CHECK(extProperties[ndx].sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
			TCU_CHECK(extProperties[ndx].pNext == DE_NULL);

			if (deMemCmp(&coreProperties[ndx], &extProperties[ndx].queueFamilyProperties, sizeof(VkQueueFamilyProperties)) != 0)
				TCU_FAIL("Mismatch between format properties reported by vkGetPhysicalDeviceQueueFamilyProperties and vkGetPhysicalDeviceQueueFamilyProperties2");

			log << TestLog::Message << " queueFamilyNdx = " << ndx <<TestLog::EndMessage
			<< TestLog::Message << extProperties[ndx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Querying device queue family properties succeeded");
}

tcu::TestStatus deviceMemoryProperties2 (Context& context)
{
	const VkPhysicalDevice				physicalDevice	= context.getPhysicalDevice();
	const CustomInstance				instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&				vki				(instance.getDriver());
	TestLog&							log				= context.getTestContext().getLog();
	VkPhysicalDeviceMemoryProperties	coreProperties;
	VkPhysicalDeviceMemoryProperties2	extProperties;

	deMemset(&coreProperties, 0xcd, sizeof(VkPhysicalDeviceMemoryProperties));
	deMemset(&extProperties, 0xcd, sizeof(VkPhysicalDeviceMemoryProperties2));

	extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	extProperties.pNext = DE_NULL;

	vki.getPhysicalDeviceMemoryProperties(physicalDevice, &coreProperties);
	vki.getPhysicalDeviceMemoryProperties2(physicalDevice, &extProperties);

	TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
	TCU_CHECK(extProperties.pNext == DE_NULL);

	if (deMemCmp(&coreProperties, &extProperties.memoryProperties, sizeof(VkPhysicalDeviceMemoryProperties)) != 0)
		TCU_FAIL("Mismatch between properties reported by vkGetPhysicalDeviceMemoryProperties and vkGetPhysicalDeviceMemoryProperties2");

	log << TestLog::Message << extProperties << TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying device memory properties succeeded");
}

tcu::TestStatus deviceFeaturesVulkan12 (Context& context)
{
	using namespace ValidateQueryBits;

	const QueryMemberTableEntry			feature11OffsetTable[] =
	{
		// VkPhysicalDevice16BitStorageFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, uniformAndStorageBuffer16BitAccess),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, storagePushConstant16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, storageInputOutput16),

		// VkPhysicalDeviceMultiviewFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, multiview),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, multiviewGeometryShader),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, multiviewTessellationShader),

		// VkPhysicalDeviceVariablePointersFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, variablePointersStorageBuffer),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, variablePointers),

		// VkPhysicalDeviceProtectedMemoryFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, protectedMemory),

		// VkPhysicalDeviceSamplerYcbcrConversionFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, samplerYcbcrConversion),

		// VkPhysicalDeviceShaderDrawParametersFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Features, shaderDrawParameters),
		{ 0, 0 }
	};
	const QueryMemberTableEntry			feature12OffsetTable[] =
	{
		// None
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, drawIndirectCount),

		// VkPhysicalDevice8BitStorageFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, storageBuffer8BitAccess),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, uniformAndStorageBuffer8BitAccess),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, storagePushConstant8),

		// VkPhysicalDeviceShaderAtomicInt64Features
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderBufferInt64Atomics),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderSharedInt64Atomics),

		// VkPhysicalDeviceShaderFloat16Int8Features
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderInt8),

		// VkPhysicalDeviceDescriptorIndexingFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayDynamicIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderUniformBufferArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderStorageBufferArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayNonUniformIndexing),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingUniformBufferUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageBufferUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingUniformTexelBufferUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageTexelBufferUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray),

		// None
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, samplerFilterMinmax),

		// VkPhysicalDeviceScalarBlockLayoutFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, scalarBlockLayout),

		// VkPhysicalDeviceImagelessFramebufferFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, imagelessFramebuffer),

		// VkPhysicalDeviceUniformBufferStandardLayoutFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, uniformBufferStandardLayout),

		// VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderSubgroupExtendedTypes),

		// VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, separateDepthStencilLayouts),

		// VkPhysicalDeviceHostQueryResetFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, hostQueryReset),

		// VkPhysicalDeviceTimelineSemaphoreFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, timelineSemaphore),

		// VkPhysicalDeviceBufferDeviceAddressFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, bufferDeviceAddressCaptureReplay),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, bufferDeviceAddressMultiDevice),

		// VkPhysicalDeviceVulkanMemoryModelFeatures
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, vulkanMemoryModel),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, vulkanMemoryModelDeviceScope),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, vulkanMemoryModelAvailabilityVisibilityChains),

		// None
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderOutputViewportIndex),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, shaderOutputLayer),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId),
		{ 0, 0 }
	};
	TestLog&											log										= context.getTestContext().getLog();
	const VkPhysicalDevice								physicalDevice							= context.getPhysicalDevice();
	const CustomInstance								instance								(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&								vki										= instance.getDriver();
	const deUint32										vulkan11FeaturesBufferSize				= sizeof(VkPhysicalDeviceVulkan11Features) + GUARD_SIZE;
	const deUint32										vulkan12FeaturesBufferSize				= sizeof(VkPhysicalDeviceVulkan12Features) + GUARD_SIZE;
	VkPhysicalDeviceFeatures2							extFeatures;
	deUint8												buffer11a[vulkan11FeaturesBufferSize];
	deUint8												buffer11b[vulkan11FeaturesBufferSize];
	deUint8												buffer12a[vulkan12FeaturesBufferSize];
	deUint8												buffer12b[vulkan12FeaturesBufferSize];
	const int											count									= 2u;
	VkPhysicalDeviceVulkan11Features*					vulkan11Features[count]					= { (VkPhysicalDeviceVulkan11Features*)(buffer11a), (VkPhysicalDeviceVulkan11Features*)(buffer11b)};
	VkPhysicalDeviceVulkan12Features*					vulkan12Features[count]					= { (VkPhysicalDeviceVulkan12Features*)(buffer12a), (VkPhysicalDeviceVulkan12Features*)(buffer12b)};

	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

	deMemset(buffer11b, GUARD_VALUE, sizeof(buffer11b));
	deMemset(buffer12a, GUARD_VALUE, sizeof(buffer12a));
	deMemset(buffer12b, GUARD_VALUE, sizeof(buffer12b));
	deMemset(buffer11a, GUARD_VALUE, sizeof(buffer11a));

	// Validate all fields initialized
	for (int ndx = 0; ndx < count; ++ndx)
	{
		deMemset(&extFeatures.features, 0x00, sizeof(extFeatures.features));
		extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		extFeatures.pNext = vulkan11Features[ndx];

		deMemset(vulkan11Features[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan11Features));
		vulkan11Features[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		vulkan11Features[ndx]->pNext = vulkan12Features[ndx];

		deMemset(vulkan12Features[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan12Features));
		vulkan12Features[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkan12Features[ndx]->pNext = DE_NULL;

		vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);
	}

	log << TestLog::Message << *vulkan11Features[0] << TestLog::EndMessage;
	log << TestLog::Message << *vulkan12Features[0] << TestLog::EndMessage;

	if (!validateStructsWithGuard(feature11OffsetTable, vulkan11Features, GUARD_VALUE, GUARD_SIZE))
	{
		log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan11Features initialization failure" << TestLog::EndMessage;

		return tcu::TestStatus::fail("VkPhysicalDeviceVulkan11Features initialization failure");
	}

	if (!validateStructsWithGuard(feature12OffsetTable, vulkan12Features, GUARD_VALUE, GUARD_SIZE))
	{
		log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan12Features initialization failure" << TestLog::EndMessage;

		return tcu::TestStatus::fail("VkPhysicalDeviceVulkan12Features initialization failure");
	}

	return tcu::TestStatus::pass("Querying Vulkan 1.2 device features succeeded");
}

tcu::TestStatus devicePropertiesVulkan12 (Context& context)
{
	using namespace ValidateQueryBits;

	const QueryMemberTableEntry			properties11OffsetTable[] =
	{
		// VkPhysicalDeviceIDProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, deviceUUID),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, driverUUID),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, deviceLUID),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, deviceNodeMask),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, deviceLUIDValid),

		// VkPhysicalDeviceSubgroupProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, subgroupSize),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, subgroupSupportedStages),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, subgroupSupportedOperations),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, subgroupQuadOperationsInAllStages),

		// VkPhysicalDevicePointClippingProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, pointClippingBehavior),

		// VkPhysicalDeviceMultiviewProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, maxMultiviewViewCount),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, maxMultiviewInstanceIndex),

		// VkPhysicalDeviceProtectedMemoryProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, protectedNoFault),

		// VkPhysicalDeviceMaintenance3Properties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, maxPerSetDescriptors),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan11Properties, maxMemoryAllocationSize),
		{ 0, 0 }
	};
	const QueryMemberTableEntry			properties12OffsetTable[] =
	{
		// VkPhysicalDeviceDriverProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, driverID),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, conformanceVersion),

		// VkPhysicalDeviceFloatControlsProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, denormBehaviorIndependence),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, roundingModeIndependence),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderSignedZeroInfNanPreserveFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderSignedZeroInfNanPreserveFloat32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderSignedZeroInfNanPreserveFloat64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormPreserveFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormPreserveFloat32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormPreserveFloat64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormFlushToZeroFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormFlushToZeroFloat32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderDenormFlushToZeroFloat64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTEFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTEFloat32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTEFloat64),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTZFloat16),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTZFloat32),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderRoundingModeRTZFloat64),

		// VkPhysicalDeviceDescriptorIndexingProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxUpdateAfterBindDescriptorsInAllPools),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderUniformBufferArrayNonUniformIndexingNative),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderSampledImageArrayNonUniformIndexingNative),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderStorageBufferArrayNonUniformIndexingNative),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderStorageImageArrayNonUniformIndexingNative),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, shaderInputAttachmentArrayNonUniformIndexingNative),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, robustBufferAccessUpdateAfterBind),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, quadDivergentImplicitLod),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindSamplers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindUniformBuffers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindStorageBuffers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindSampledImages),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindStorageImages),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageDescriptorUpdateAfterBindInputAttachments),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxPerStageUpdateAfterBindResources),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindSamplers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindUniformBuffers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindStorageBuffers),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindSampledImages),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindStorageImages),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxDescriptorSetUpdateAfterBindInputAttachments),

		// VkPhysicalDeviceDepthStencilResolveProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, supportedDepthResolveModes),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, supportedStencilResolveModes),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, independentResolveNone),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, independentResolve),

		// VkPhysicalDeviceSamplerFilterMinmaxProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, filterMinmaxSingleComponentFormats),
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, filterMinmaxImageComponentMapping),

		// VkPhysicalDeviceTimelineSemaphoreProperties
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, maxTimelineSemaphoreValueDifference),

		// None
		OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan12Properties, framebufferIntegerColorSampleCounts),
		{ 0, 0 }
	};
	TestLog&										log											= context.getTestContext().getLog();
	const VkPhysicalDevice							physicalDevice								= context.getPhysicalDevice();
	const CustomInstance							instance									(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&							vki											= instance.getDriver();
	const deUint32									vulkan11PropertiesBufferSize				= sizeof(VkPhysicalDeviceVulkan11Properties) + GUARD_SIZE;
	const deUint32									vulkan12PropertiesBufferSize				= sizeof(VkPhysicalDeviceVulkan12Properties) + GUARD_SIZE;
	VkPhysicalDeviceProperties2						extProperties;
	deUint8											buffer11a[vulkan11PropertiesBufferSize];
	deUint8											buffer11b[vulkan11PropertiesBufferSize];
	deUint8											buffer12a[vulkan12PropertiesBufferSize];
	deUint8											buffer12b[vulkan12PropertiesBufferSize];
	const int										count										= 2u;
	VkPhysicalDeviceVulkan11Properties*				vulkan11Properties[count]					= { (VkPhysicalDeviceVulkan11Properties*)(buffer11a), (VkPhysicalDeviceVulkan11Properties*)(buffer11b)};
	VkPhysicalDeviceVulkan12Properties*				vulkan12Properties[count]					= { (VkPhysicalDeviceVulkan12Properties*)(buffer12a), (VkPhysicalDeviceVulkan12Properties*)(buffer12b)};

	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

	deMemset(buffer11a, GUARD_VALUE, sizeof(buffer11a));
	deMemset(buffer11b, GUARD_VALUE, sizeof(buffer11b));
	deMemset(buffer12a, GUARD_VALUE, sizeof(buffer12a));
	deMemset(buffer12b, GUARD_VALUE, sizeof(buffer12b));

	for (int ndx = 0; ndx < count; ++ndx)
	{
		deMemset(&extProperties.properties, 0x00, sizeof(extProperties.properties));
		extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		extProperties.pNext = vulkan11Properties[ndx];

		deMemset(vulkan11Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan11Properties));
		vulkan11Properties[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
		vulkan11Properties[ndx]->pNext = vulkan12Properties[ndx];

		deMemset(vulkan12Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan12Properties));
		vulkan12Properties[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
		vulkan12Properties[ndx]->pNext = DE_NULL;

		vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
	}

	log << TestLog::Message << *vulkan11Properties[0] << TestLog::EndMessage;
	log << TestLog::Message << *vulkan12Properties[0] << TestLog::EndMessage;

	if (!validateStructsWithGuard(properties11OffsetTable, vulkan11Properties, GUARD_VALUE, GUARD_SIZE))
	{
		log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan11Properties initialization failure" << TestLog::EndMessage;

		return tcu::TestStatus::fail("VkPhysicalDeviceVulkan11Properties initialization failure");
	}

	if (!validateStructsWithGuard(properties12OffsetTable, vulkan12Properties, GUARD_VALUE, GUARD_SIZE) ||
		strncmp(vulkan12Properties[0]->driverName, vulkan12Properties[1]->driverName, VK_MAX_DRIVER_NAME_SIZE) != 0 ||
		strncmp(vulkan12Properties[0]->driverInfo, vulkan12Properties[1]->driverInfo, VK_MAX_DRIVER_INFO_SIZE) != 0 )
	{
		log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan12Properties initialization failure" << TestLog::EndMessage;

		return tcu::TestStatus::fail("VkPhysicalDeviceVulkan12Properties initialization failure");
	}

	return tcu::TestStatus::pass("Querying Vulkan 1.2 device properties succeeded");
}

tcu::TestStatus deviceFeatureExtensionsConsistencyVulkan12(Context& context)
{
	TestLog&											log										= context.getTestContext().getLog();
	const VkPhysicalDevice								physicalDevice							= context.getPhysicalDevice();
	const CustomInstance								instance								(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&								vki										= instance.getDriver();

	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

	VkPhysicalDeviceVulkan12Features					vulkan12Features						= initVulkanStructure();
	VkPhysicalDeviceVulkan11Features					vulkan11Features						= initVulkanStructure(&vulkan12Features);
	VkPhysicalDeviceFeatures2							extFeatures								= initVulkanStructure(&vulkan11Features);

	vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

	log << TestLog::Message << vulkan11Features << TestLog::EndMessage;
	log << TestLog::Message << vulkan12Features << TestLog::EndMessage;

	// Validate if proper VkPhysicalDeviceVulkanXXFeatures fields are set when corresponding extensions are present
	std::pair<std::pair<const char*,const char*>, VkBool32> extensions2validate[] =
	{
		{ { "VK_KHR_sampler_mirror_clamp_to_edge",	"VkPhysicalDeviceVulkan12Features.samplerMirrorClampToEdge" },	vulkan12Features.samplerMirrorClampToEdge },
		{ { "VK_KHR_draw_indirect_count",			"VkPhysicalDeviceVulkan12Features.drawIndirectCount" },			vulkan12Features.drawIndirectCount },
		{ { "VK_EXT_descriptor_indexing",			"VkPhysicalDeviceVulkan12Features.descriptorIndexing" },		vulkan12Features.descriptorIndexing },
		{ { "VK_EXT_sampler_filter_minmax",			"VkPhysicalDeviceVulkan12Features.samplerFilterMinmax" },		vulkan12Features.samplerFilterMinmax },
		{ { "VK_EXT_shader_viewport_index_layer",	"VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex" },	vulkan12Features.shaderOutputViewportIndex },
		{ { "VK_EXT_shader_viewport_index_layer",	"VkPhysicalDeviceVulkan12Features.shaderOutputLayer" },			vulkan12Features.shaderOutputLayer }
	};
	vector<VkExtensionProperties> extensionProperties = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
	for (const auto& ext : extensions2validate)
		if (checkExtension(extensionProperties, ext.first.first) && !ext.second)
			TCU_FAIL(string("Mismatch between extension ") + ext.first.first + " and " + ext.first.second);

	// collect all extension features
	{
		VkPhysicalDevice16BitStorageFeatures				device16BitStorageFeatures				= initVulkanStructure();
		VkPhysicalDeviceMultiviewFeatures					deviceMultiviewFeatures					= initVulkanStructure(&device16BitStorageFeatures);
		VkPhysicalDeviceProtectedMemoryFeatures				protectedMemoryFeatures					= initVulkanStructure(&deviceMultiviewFeatures);
		VkPhysicalDeviceSamplerYcbcrConversionFeatures		samplerYcbcrConversionFeatures			= initVulkanStructure(&protectedMemoryFeatures);
		VkPhysicalDeviceShaderDrawParametersFeatures		shaderDrawParametersFeatures			= initVulkanStructure(&samplerYcbcrConversionFeatures);
		VkPhysicalDeviceVariablePointersFeatures			variablePointerFeatures					= initVulkanStructure(&shaderDrawParametersFeatures);
		VkPhysicalDevice8BitStorageFeatures					device8BitStorageFeatures				= initVulkanStructure(&variablePointerFeatures);
		VkPhysicalDeviceShaderAtomicInt64Features			shaderAtomicInt64Features				= initVulkanStructure(&device8BitStorageFeatures);
		VkPhysicalDeviceShaderFloat16Int8Features			shaderFloat16Int8Features				= initVulkanStructure(&shaderAtomicInt64Features);
		VkPhysicalDeviceDescriptorIndexingFeatures			descriptorIndexingFeatures				= initVulkanStructure(&shaderFloat16Int8Features);
		VkPhysicalDeviceScalarBlockLayoutFeatures			scalarBlockLayoutFeatures				= initVulkanStructure(&descriptorIndexingFeatures);
		VkPhysicalDeviceImagelessFramebufferFeatures		imagelessFramebufferFeatures			= initVulkanStructure(&scalarBlockLayoutFeatures);
		VkPhysicalDeviceUniformBufferStandardLayoutFeatures	uniformBufferStandardLayoutFeatures		= initVulkanStructure(&imagelessFramebufferFeatures);
		VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures	shaderSubgroupExtendedTypesFeatures		= initVulkanStructure(&uniformBufferStandardLayoutFeatures);
		VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures	separateDepthStencilLayoutsFeatures		= initVulkanStructure(&shaderSubgroupExtendedTypesFeatures);
		VkPhysicalDeviceHostQueryResetFeatures				hostQueryResetFeatures					= initVulkanStructure(&separateDepthStencilLayoutsFeatures);
		VkPhysicalDeviceTimelineSemaphoreFeatures			timelineSemaphoreFeatures				= initVulkanStructure(&hostQueryResetFeatures);
		VkPhysicalDeviceBufferDeviceAddressFeatures			bufferDeviceAddressFeatures				= initVulkanStructure(&timelineSemaphoreFeatures);
		VkPhysicalDeviceVulkanMemoryModelFeatures			vulkanMemoryModelFeatures				= initVulkanStructure(&bufferDeviceAddressFeatures);
		extFeatures = initVulkanStructure(&vulkanMemoryModelFeatures);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

		log << TestLog::Message << extFeatures << TestLog::EndMessage;
		log << TestLog::Message << device16BitStorageFeatures << TestLog::EndMessage;
		log << TestLog::Message << deviceMultiviewFeatures << TestLog::EndMessage;
		log << TestLog::Message << protectedMemoryFeatures << TestLog::EndMessage;
		log << TestLog::Message << samplerYcbcrConversionFeatures << TestLog::EndMessage;
		log << TestLog::Message << shaderDrawParametersFeatures << TestLog::EndMessage;
		log << TestLog::Message << variablePointerFeatures << TestLog::EndMessage;
		log << TestLog::Message << device8BitStorageFeatures << TestLog::EndMessage;
		log << TestLog::Message << shaderAtomicInt64Features << TestLog::EndMessage;
		log << TestLog::Message << shaderFloat16Int8Features << TestLog::EndMessage;
		log << TestLog::Message << descriptorIndexingFeatures << TestLog::EndMessage;
		log << TestLog::Message << scalarBlockLayoutFeatures << TestLog::EndMessage;
		log << TestLog::Message << imagelessFramebufferFeatures << TestLog::EndMessage;
		log << TestLog::Message << uniformBufferStandardLayoutFeatures << TestLog::EndMessage;
		log << TestLog::Message << shaderSubgroupExtendedTypesFeatures << TestLog::EndMessage;
		log << TestLog::Message << separateDepthStencilLayoutsFeatures << TestLog::EndMessage;
		log << TestLog::Message << hostQueryResetFeatures << TestLog::EndMessage;
		log << TestLog::Message << timelineSemaphoreFeatures << TestLog::EndMessage;
		log << TestLog::Message << bufferDeviceAddressFeatures << TestLog::EndMessage;
		log << TestLog::Message << vulkanMemoryModelFeatures << TestLog::EndMessage;

		if ((	device16BitStorageFeatures.storageBuffer16BitAccess				!= vulkan11Features.storageBuffer16BitAccess ||
				device16BitStorageFeatures.uniformAndStorageBuffer16BitAccess	!= vulkan11Features.uniformAndStorageBuffer16BitAccess ||
				device16BitStorageFeatures.storagePushConstant16				!= vulkan11Features.storagePushConstant16 ||
				device16BitStorageFeatures.storageInputOutput16					!= vulkan11Features.storageInputOutput16 ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDevice16BitStorageFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if ((	deviceMultiviewFeatures.multiview					!= vulkan11Features.multiview ||
				deviceMultiviewFeatures.multiviewGeometryShader		!= vulkan11Features.multiviewGeometryShader ||
				deviceMultiviewFeatures.multiviewTessellationShader	!= vulkan11Features.multiviewTessellationShader ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if (	(protectedMemoryFeatures.protectedMemory	!= vulkan11Features.protectedMemory ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if (	(samplerYcbcrConversionFeatures.samplerYcbcrConversion	!= vulkan11Features.samplerYcbcrConversion ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerYcbcrConversionFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if (	(shaderDrawParametersFeatures.shaderDrawParameters	!= vulkan11Features.shaderDrawParameters ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceShaderDrawParametersFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if ((	variablePointerFeatures.variablePointersStorageBuffer	!= vulkan11Features.variablePointersStorageBuffer ||
				variablePointerFeatures.variablePointers				!= vulkan11Features.variablePointers))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceVariablePointersFeatures and VkPhysicalDeviceVulkan11Features");
		}

		if ((	device8BitStorageFeatures.storageBuffer8BitAccess			!= vulkan12Features.storageBuffer8BitAccess ||
				device8BitStorageFeatures.uniformAndStorageBuffer8BitAccess	!= vulkan12Features.uniformAndStorageBuffer8BitAccess ||
				device8BitStorageFeatures.storagePushConstant8				!= vulkan12Features.storagePushConstant8 ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDevice8BitStorageFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	shaderAtomicInt64Features.shaderBufferInt64Atomics != vulkan12Features.shaderBufferInt64Atomics ||
				shaderAtomicInt64Features.shaderSharedInt64Atomics != vulkan12Features.shaderSharedInt64Atomics ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceShaderAtomicInt64Features and VkPhysicalDeviceVulkan12Features");
		}

		if ((	shaderFloat16Int8Features.shaderFloat16	!= vulkan12Features.shaderFloat16 ||
				shaderFloat16Int8Features.shaderInt8		!= vulkan12Features.shaderInt8 ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceShaderFloat16Int8Features and VkPhysicalDeviceVulkan12Features");
		}

		if ((vulkan12Features.descriptorIndexing) &&
			(	descriptorIndexingFeatures.shaderInputAttachmentArrayDynamicIndexing			!= vulkan12Features.shaderInputAttachmentArrayDynamicIndexing ||
				descriptorIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing			!= vulkan12Features.shaderUniformTexelBufferArrayDynamicIndexing ||
				descriptorIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing			!= vulkan12Features.shaderStorageTexelBufferArrayDynamicIndexing ||
				descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing			!= vulkan12Features.shaderUniformBufferArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing			!= vulkan12Features.shaderSampledImageArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing			!= vulkan12Features.shaderStorageBufferArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing			!= vulkan12Features.shaderStorageImageArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderInputAttachmentArrayNonUniformIndexing			!= vulkan12Features.shaderInputAttachmentArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing		!= vulkan12Features.shaderUniformTexelBufferArrayNonUniformIndexing ||
				descriptorIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing		!= vulkan12Features.shaderStorageTexelBufferArrayNonUniformIndexing ||
				descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind		!= vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind			!= vulkan12Features.descriptorBindingSampledImageUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind			!= vulkan12Features.descriptorBindingStorageImageUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind		!= vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind	!= vulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind	!= vulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind ||
				descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending			!= vulkan12Features.descriptorBindingUpdateUnusedWhilePending ||
				descriptorIndexingFeatures.descriptorBindingPartiallyBound						!= vulkan12Features.descriptorBindingPartiallyBound ||
				descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount				!= vulkan12Features.descriptorBindingVariableDescriptorCount ||
				descriptorIndexingFeatures.runtimeDescriptorArray								!= vulkan12Features.runtimeDescriptorArray ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceDescriptorIndexingFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	scalarBlockLayoutFeatures.scalarBlockLayout != vulkan12Features.scalarBlockLayout ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceScalarBlockLayoutFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	imagelessFramebufferFeatures.imagelessFramebuffer != vulkan12Features.imagelessFramebuffer ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceImagelessFramebufferFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	uniformBufferStandardLayoutFeatures.uniformBufferStandardLayout != vulkan12Features.uniformBufferStandardLayout ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceUniformBufferStandardLayoutFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	shaderSubgroupExtendedTypesFeatures.shaderSubgroupExtendedTypes != vulkan12Features.shaderSubgroupExtendedTypes ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	separateDepthStencilLayoutsFeatures.separateDepthStencilLayouts != vulkan12Features.separateDepthStencilLayouts ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	hostQueryResetFeatures.hostQueryReset != vulkan12Features.hostQueryReset ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceHostQueryResetFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	timelineSemaphoreFeatures.timelineSemaphore != vulkan12Features.timelineSemaphore ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceTimelineSemaphoreFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	bufferDeviceAddressFeatures.bufferDeviceAddress					!= vulkan12Features.bufferDeviceAddress ||
				bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay	!= vulkan12Features.bufferDeviceAddressCaptureReplay ||
				bufferDeviceAddressFeatures.bufferDeviceAddressMultiDevice		!= vulkan12Features.bufferDeviceAddressMultiDevice ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceBufferDeviceAddressFeatures and VkPhysicalDeviceVulkan12Features");
		}

		if ((	vulkanMemoryModelFeatures.vulkanMemoryModel								!= vulkan12Features.vulkanMemoryModel ||
				vulkanMemoryModelFeatures.vulkanMemoryModelDeviceScope					!= vulkan12Features.vulkanMemoryModelDeviceScope ||
				vulkanMemoryModelFeatures.vulkanMemoryModelAvailabilityVisibilityChains	!= vulkan12Features.vulkanMemoryModelAvailabilityVisibilityChains ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceVulkanMemoryModelFeatures and VkPhysicalDeviceVulkan12Features");
		}
	}

	return tcu::TestStatus::pass("Vulkan 1.2 device features are consistent with extensions");
}

tcu::TestStatus devicePropertyExtensionsConsistencyVulkan12(Context& context)
{
	TestLog&										log											= context.getTestContext().getLog();
	const VkPhysicalDevice							physicalDevice								= context.getPhysicalDevice();
	const CustomInstance							instance									(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&							vki											= instance.getDriver();

	if (!context.contextSupports(vk::ApiVersion(1, 2, 0)))
		TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

	VkPhysicalDeviceVulkan12Properties				vulkan12Properties							= initVulkanStructure();
	VkPhysicalDeviceVulkan11Properties				vulkan11Properties							= initVulkanStructure(&vulkan12Properties);
	VkPhysicalDeviceProperties2						extProperties								= initVulkanStructure(&vulkan11Properties);

	vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

	log << TestLog::Message << vulkan11Properties << TestLog::EndMessage;
	log << TestLog::Message << vulkan12Properties << TestLog::EndMessage;

	// Validate all fields initialized matching to extension structures
	{
		VkPhysicalDeviceIDProperties					idProperties								= initVulkanStructure();
		VkPhysicalDeviceSubgroupProperties				subgroupProperties							= initVulkanStructure(&idProperties);
		VkPhysicalDevicePointClippingProperties			pointClippingProperties						= initVulkanStructure(&subgroupProperties);
		VkPhysicalDeviceMultiviewProperties				multiviewProperties							= initVulkanStructure(&pointClippingProperties);
		VkPhysicalDeviceProtectedMemoryProperties		protectedMemoryPropertiesKHR				= initVulkanStructure(&multiviewProperties);
		VkPhysicalDeviceMaintenance3Properties			maintenance3Properties						= initVulkanStructure(&protectedMemoryPropertiesKHR);
		VkPhysicalDeviceDriverProperties				driverProperties							= initVulkanStructure(&maintenance3Properties);
		VkPhysicalDeviceFloatControlsProperties			floatControlsProperties						= initVulkanStructure(&driverProperties);
		VkPhysicalDeviceDescriptorIndexingProperties	descriptorIndexingProperties				= initVulkanStructure(&floatControlsProperties);
		VkPhysicalDeviceDepthStencilResolveProperties	depthStencilResolveProperties				= initVulkanStructure(&descriptorIndexingProperties);
		VkPhysicalDeviceSamplerFilterMinmaxProperties	samplerFilterMinmaxProperties				= initVulkanStructure(&depthStencilResolveProperties);
		VkPhysicalDeviceTimelineSemaphoreProperties		timelineSemaphoreProperties					= initVulkanStructure(&samplerFilterMinmaxProperties);
		extProperties = initVulkanStructure(&timelineSemaphoreProperties);

		vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

		if ((deMemCmp(idProperties.deviceUUID, vulkan11Properties.deviceUUID, VK_UUID_SIZE) != 0) ||
			(deMemCmp(idProperties.driverUUID, vulkan11Properties.driverUUID, VK_UUID_SIZE) != 0) ||
			(idProperties.deviceLUIDValid != vulkan11Properties.deviceLUIDValid))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties and VkPhysicalDeviceVulkan11Properties");
		}
		else if (idProperties.deviceLUIDValid)
		{
			// If deviceLUIDValid is VK_FALSE, the contents of deviceLUID and deviceNodeMask are undefined
			// so thay can only be compared when deviceLUIDValid is VK_TRUE.
			if ((deMemCmp(idProperties.deviceLUID, vulkan11Properties.deviceLUID, VK_UUID_SIZE) != 0) ||
				(idProperties.deviceNodeMask != vulkan11Properties.deviceNodeMask))
			{
				TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties and VkPhysicalDeviceVulkan11Properties");
			}
		}

		if ((subgroupProperties.subgroupSize				!= vulkan11Properties.subgroupSize ||
			 subgroupProperties.supportedStages				!= vulkan11Properties.subgroupSupportedStages ||
			 subgroupProperties.supportedOperations			!= vulkan11Properties.subgroupSupportedOperations ||
			 subgroupProperties.quadOperationsInAllStages	!= vulkan11Properties.subgroupQuadOperationsInAllStages))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties and VkPhysicalDeviceVulkan11Properties");
		}

		if ((pointClippingProperties.pointClippingBehavior	!= vulkan11Properties.pointClippingBehavior))
		{
			TCU_FAIL("Mismatch between VkPhysicalDevicePointClippingProperties and VkPhysicalDeviceVulkan11Properties");
		}

		if ((multiviewProperties.maxMultiviewViewCount		!= vulkan11Properties.maxMultiviewViewCount ||
			 multiviewProperties.maxMultiviewInstanceIndex	!= vulkan11Properties.maxMultiviewInstanceIndex))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewProperties and VkPhysicalDeviceVulkan11Properties");
		}

		if ((protectedMemoryPropertiesKHR.protectedNoFault	!= vulkan11Properties.protectedNoFault))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryProperties and VkPhysicalDeviceVulkan11Properties");
		}

		if ((maintenance3Properties.maxPerSetDescriptors	!= vulkan11Properties.maxPerSetDescriptors ||
			 maintenance3Properties.maxMemoryAllocationSize	!= vulkan11Properties.maxMemoryAllocationSize))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance3Properties and VkPhysicalDeviceVulkan11Properties");
		}

		if ((driverProperties.driverID												!= vulkan12Properties.driverID ||
			 strncmp(driverProperties.driverName, vulkan12Properties.driverName, VK_MAX_DRIVER_NAME_SIZE)	!= 0 ||
			 strncmp(driverProperties.driverInfo, vulkan12Properties.driverInfo, VK_MAX_DRIVER_INFO_SIZE)	!= 0 ||
			 driverProperties.conformanceVersion.major								!= vulkan12Properties.conformanceVersion.major ||
			 driverProperties.conformanceVersion.minor								!= vulkan12Properties.conformanceVersion.minor ||
			 driverProperties.conformanceVersion.subminor							!= vulkan12Properties.conformanceVersion.subminor ||
			 driverProperties.conformanceVersion.patch								!= vulkan12Properties.conformanceVersion.patch))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceDriverProperties and VkPhysicalDeviceVulkan12Properties");
		}

		if ((floatControlsProperties.denormBehaviorIndependence				!= vulkan12Properties.denormBehaviorIndependence ||
			 floatControlsProperties.roundingModeIndependence				!= vulkan12Properties.roundingModeIndependence ||
			 floatControlsProperties.shaderSignedZeroInfNanPreserveFloat16	!= vulkan12Properties.shaderSignedZeroInfNanPreserveFloat16 ||
			 floatControlsProperties.shaderSignedZeroInfNanPreserveFloat32	!= vulkan12Properties.shaderSignedZeroInfNanPreserveFloat32 ||
			 floatControlsProperties.shaderSignedZeroInfNanPreserveFloat64	!= vulkan12Properties.shaderSignedZeroInfNanPreserveFloat64 ||
			 floatControlsProperties.shaderDenormPreserveFloat16			!= vulkan12Properties.shaderDenormPreserveFloat16 ||
			 floatControlsProperties.shaderDenormPreserveFloat32			!= vulkan12Properties.shaderDenormPreserveFloat32 ||
			 floatControlsProperties.shaderDenormPreserveFloat64			!= vulkan12Properties.shaderDenormPreserveFloat64 ||
			 floatControlsProperties.shaderDenormFlushToZeroFloat16			!= vulkan12Properties.shaderDenormFlushToZeroFloat16 ||
			 floatControlsProperties.shaderDenormFlushToZeroFloat32			!= vulkan12Properties.shaderDenormFlushToZeroFloat32 ||
			 floatControlsProperties.shaderDenormFlushToZeroFloat64			!= vulkan12Properties.shaderDenormFlushToZeroFloat64 ||
			 floatControlsProperties.shaderRoundingModeRTEFloat16			!= vulkan12Properties.shaderRoundingModeRTEFloat16 ||
			 floatControlsProperties.shaderRoundingModeRTEFloat32			!= vulkan12Properties.shaderRoundingModeRTEFloat32 ||
			 floatControlsProperties.shaderRoundingModeRTEFloat64			!= vulkan12Properties.shaderRoundingModeRTEFloat64 ||
			 floatControlsProperties.shaderRoundingModeRTZFloat16			!= vulkan12Properties.shaderRoundingModeRTZFloat16 ||
			 floatControlsProperties.shaderRoundingModeRTZFloat32			!= vulkan12Properties.shaderRoundingModeRTZFloat32 ||
			 floatControlsProperties.shaderRoundingModeRTZFloat64			!= vulkan12Properties.shaderRoundingModeRTZFloat64 ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceFloatControlsProperties and VkPhysicalDeviceVulkan12Properties");
		}

		if ((descriptorIndexingProperties.maxUpdateAfterBindDescriptorsInAllPools				!= vulkan12Properties.maxUpdateAfterBindDescriptorsInAllPools ||
			 descriptorIndexingProperties.shaderUniformBufferArrayNonUniformIndexingNative		!= vulkan12Properties.shaderUniformBufferArrayNonUniformIndexingNative ||
			 descriptorIndexingProperties.shaderSampledImageArrayNonUniformIndexingNative		!= vulkan12Properties.shaderSampledImageArrayNonUniformIndexingNative ||
			 descriptorIndexingProperties.shaderStorageBufferArrayNonUniformIndexingNative		!= vulkan12Properties.shaderStorageBufferArrayNonUniformIndexingNative ||
			 descriptorIndexingProperties.shaderStorageImageArrayNonUniformIndexingNative		!= vulkan12Properties.shaderStorageImageArrayNonUniformIndexingNative ||
			 descriptorIndexingProperties.shaderInputAttachmentArrayNonUniformIndexingNative	!= vulkan12Properties.shaderInputAttachmentArrayNonUniformIndexingNative ||
			 descriptorIndexingProperties.robustBufferAccessUpdateAfterBind						!= vulkan12Properties.robustBufferAccessUpdateAfterBind ||
			 descriptorIndexingProperties.quadDivergentImplicitLod								!= vulkan12Properties.quadDivergentImplicitLod ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers			!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindUniformBuffers	!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageBuffers	!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages		!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageImages		!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages ||
			 descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindInputAttachments	!= vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments ||
			 descriptorIndexingProperties.maxPerStageUpdateAfterBindResources					!= vulkan12Properties.maxPerStageUpdateAfterBindResources ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers				!= vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers			!= vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic	!= vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers			!= vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic	!= vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages			!= vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages			!= vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages ||
			 descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindInputAttachments		!= vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments ))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceDescriptorIndexingProperties and VkPhysicalDeviceVulkan12Properties");
		}

		if ((depthStencilResolveProperties.supportedDepthResolveModes	!= vulkan12Properties.supportedDepthResolveModes ||
			 depthStencilResolveProperties.supportedStencilResolveModes	!= vulkan12Properties.supportedStencilResolveModes ||
			 depthStencilResolveProperties.independentResolveNone		!= vulkan12Properties.independentResolveNone ||
			 depthStencilResolveProperties.independentResolve			!= vulkan12Properties.independentResolve))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceDepthStencilResolveProperties and VkPhysicalDeviceVulkan12Properties");
		}

		if ((samplerFilterMinmaxProperties.filterMinmaxSingleComponentFormats	!= vulkan12Properties.filterMinmaxSingleComponentFormats ||
			 samplerFilterMinmaxProperties.filterMinmaxImageComponentMapping	!= vulkan12Properties.filterMinmaxImageComponentMapping))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerFilterMinmaxProperties and VkPhysicalDeviceVulkan12Properties");
		}

		if ((timelineSemaphoreProperties.maxTimelineSemaphoreValueDifference	!= vulkan12Properties.maxTimelineSemaphoreValueDifference))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceTimelineSemaphoreProperties and VkPhysicalDeviceVulkan12Properties");
		}
	}

	return tcu::TestStatus::pass("Vulkan 1.2 device properties are consistent with extension properties");
}

tcu::TestStatus imageFormatProperties2 (Context& context, const VkFormat format, const VkImageType imageType, const VkImageTiling tiling)
{
	if (isYCbCrFormat(format))
		// check if Ycbcr format enums are valid given the version and extensions
		checkYcbcrApiSupport(context);

	TestLog&						log				= context.getTestContext().getLog();

	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const CustomInstance			instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki				(instance.getDriver());

	const VkImageCreateFlags		ycbcrFlags		= isYCbCrFormat(format) ? (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT_KHR : (VkImageCreateFlags)0u;
	const VkImageUsageFlags			allUsageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT
													| VK_IMAGE_USAGE_TRANSFER_DST_BIT
													| VK_IMAGE_USAGE_SAMPLED_BIT
													| VK_IMAGE_USAGE_STORAGE_BIT
													| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	const VkImageCreateFlags		allCreateFlags	= VK_IMAGE_CREATE_SPARSE_BINDING_BIT
													| VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
													| VK_IMAGE_CREATE_SPARSE_ALIASED_BIT
													| VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
													| VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
													| ycbcrFlags;

	for (VkImageUsageFlags curUsageFlags = (VkImageUsageFlags)1; curUsageFlags <= allUsageFlags; curUsageFlags++)
	{
		if (!isValidImageUsageFlagCombination(curUsageFlags))
			continue;

		for (VkImageCreateFlags curCreateFlags = 0; curCreateFlags <= allCreateFlags; curCreateFlags++)
		{
			const VkPhysicalDeviceImageFormatInfo2	imageFormatInfo	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
				DE_NULL,
				format,
				imageType,
				tiling,
				curUsageFlags,
				curCreateFlags
			};

			VkImageFormatProperties						coreProperties;
			VkImageFormatProperties2					extProperties;
			VkResult									coreResult;
			VkResult									extResult;

			deMemset(&coreProperties, 0xcd, sizeof(VkImageFormatProperties));
			deMemset(&extProperties, 0xcd, sizeof(VkImageFormatProperties2));

			extProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
			extProperties.pNext = DE_NULL;

			coreResult	= vki.getPhysicalDeviceImageFormatProperties(physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.tiling, imageFormatInfo.usage, imageFormatInfo.flags, &coreProperties);
			extResult	= vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &extProperties);

			TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);
			TCU_CHECK(extProperties.pNext == DE_NULL);

			if ((coreResult != extResult) ||
				(deMemCmp(&coreProperties, &extProperties.imageFormatProperties, sizeof(VkImageFormatProperties)) != 0))
			{
				log << TestLog::Message << "ERROR: device mismatch with query " << imageFormatInfo << TestLog::EndMessage
					<< TestLog::Message << "vkGetPhysicalDeviceImageFormatProperties() returned " << coreResult << ", " << coreProperties << TestLog::EndMessage
					<< TestLog::Message << "vkGetPhysicalDeviceImageFormatProperties2() returned " << extResult << ", " << extProperties << TestLog::EndMessage;
				TCU_FAIL("Mismatch between image format properties reported by vkGetPhysicalDeviceImageFormatProperties and vkGetPhysicalDeviceImageFormatProperties2");
			}
		}
	}

	return tcu::TestStatus::pass("Querying image format properties succeeded");
}

tcu::TestStatus sparseImageFormatProperties2 (Context& context, const VkFormat format, const VkImageType imageType, const VkImageTiling tiling)
{
	TestLog&						log				= context.getTestContext().getLog();

	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const CustomInstance			instance		(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&			vki				(instance.getDriver());

	const VkImageUsageFlags			allUsageFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT
													| VK_IMAGE_USAGE_TRANSFER_DST_BIT
													| VK_IMAGE_USAGE_SAMPLED_BIT
													| VK_IMAGE_USAGE_STORAGE_BIT
													| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
													| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	for (deUint32 sampleCountBit = VK_SAMPLE_COUNT_1_BIT; sampleCountBit <= VK_SAMPLE_COUNT_64_BIT; sampleCountBit = (sampleCountBit << 1u))
	{
		for (VkImageUsageFlags curUsageFlags = (VkImageUsageFlags)1; curUsageFlags <= allUsageFlags; curUsageFlags++)
		{
			if (!isValidImageUsageFlagCombination(curUsageFlags))
				continue;

			const VkPhysicalDeviceSparseImageFormatInfo2	imageFormatInfo	=
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2,
				DE_NULL,
				format,
				imageType,
				(VkSampleCountFlagBits)sampleCountBit,
				curUsageFlags,
				tiling,
			};

			deUint32	numCoreProperties	= 0u;
			deUint32	numExtProperties	= 0u;

			// Query count
			vki.getPhysicalDeviceSparseImageFormatProperties(physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.samples, imageFormatInfo.usage, imageFormatInfo.tiling, &numCoreProperties, DE_NULL);
			vki.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &imageFormatInfo, &numExtProperties, DE_NULL);

			if (numCoreProperties != numExtProperties)
			{
				log << TestLog::Message << "ERROR: different number of properties reported for " << imageFormatInfo << TestLog::EndMessage;
				TCU_FAIL("Mismatch in reported property count");
			}

			if (!context.getDeviceFeatures().sparseBinding)
			{
				// There is no support for sparse binding, getPhysicalDeviceSparseImageFormatProperties* MUST report no properties
				// Only have to check one of the entrypoints as a mismatch in count is already caught.
				if (numCoreProperties > 0)
				{
					log << TestLog::Message << "ERROR: device does not support sparse binding but claims support for " << numCoreProperties << " properties in vkGetPhysicalDeviceSparseImageFormatProperties with parameters " << imageFormatInfo << TestLog::EndMessage;
					TCU_FAIL("Claimed format properties inconsistent with overall sparseBinding feature");
				}
			}

			if (numCoreProperties > 0)
			{
				std::vector<VkSparseImageFormatProperties>		coreProperties	(numCoreProperties);
				std::vector<VkSparseImageFormatProperties2>		extProperties	(numExtProperties);

				deMemset(&coreProperties[0], 0xcd, sizeof(VkSparseImageFormatProperties)*numCoreProperties);
				deMemset(&extProperties[0], 0xcd, sizeof(VkSparseImageFormatProperties2)*numExtProperties);

				for (deUint32 ndx = 0; ndx < numExtProperties; ++ndx)
				{
					extProperties[ndx].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2;
					extProperties[ndx].pNext = DE_NULL;
				}

				vki.getPhysicalDeviceSparseImageFormatProperties(physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.samples, imageFormatInfo.usage, imageFormatInfo.tiling, &numCoreProperties, &coreProperties[0]);
				vki.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &imageFormatInfo, &numExtProperties, &extProperties[0]);

				TCU_CHECK((size_t)numCoreProperties == coreProperties.size());
				TCU_CHECK((size_t)numExtProperties == extProperties.size());

				for (deUint32 ndx = 0; ndx < numCoreProperties; ++ndx)
				{
					TCU_CHECK(extProperties[ndx].sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2);
					TCU_CHECK(extProperties[ndx].pNext == DE_NULL);

					if ((deMemCmp(&coreProperties[ndx], &extProperties[ndx].properties, sizeof(VkSparseImageFormatProperties)) != 0))
					{
						log << TestLog::Message << "ERROR: device mismatch with query " << imageFormatInfo << " property " << ndx << TestLog::EndMessage
							<< TestLog::Message << "vkGetPhysicalDeviceSparseImageFormatProperties() returned " << coreProperties[ndx] << TestLog::EndMessage
							<< TestLog::Message << "vkGetPhysicalDeviceSparseImageFormatProperties2() returned " << extProperties[ndx] << TestLog::EndMessage;
						TCU_FAIL("Mismatch between image format properties reported by vkGetPhysicalDeviceSparseImageFormatProperties and vkGetPhysicalDeviceSparseImageFormatProperties2");
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Querying sparse image format properties succeeded");
}

tcu::TestStatus execImageFormatTest (Context& context, ImageFormatPropertyCase testCase)
{
	return testCase.testFunction(context, testCase.format, testCase.imageType, testCase.tiling);
}

void createImageFormatTypeTilingTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase params)
{
	DE_ASSERT(params.format == VK_FORMAT_UNDEFINED);

	static const struct
	{
		VkFormat								begin;
		VkFormat								end;
		ImageFormatPropertyCase					params;
	} s_formatRanges[] =
	{
		// core formats
		{ (VkFormat)(VK_FORMAT_UNDEFINED + 1),		VK_CORE_FORMAT_LAST,										params },

		// YCbCr formats
		{ VK_FORMAT_G8B8G8R8_422_UNORM_KHR,			(VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR + 1),	params },

		// YCbCr extended formats
		{ VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT,	(VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT+1),	params },
	};

	for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
	{
		const VkFormat								rangeBegin		= s_formatRanges[rangeNdx].begin;
		const VkFormat								rangeEnd		= s_formatRanges[rangeNdx].end;

		for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format+1))
		{
			const bool			isYCbCr		= isYCbCrFormat(format);
			const bool			isSparse	= (params.testFunction == sparseImageFormatProperties2);

			if (isYCbCr && isSparse)
				continue;

			if (isYCbCr && params.imageType != VK_IMAGE_TYPE_2D)
				continue;

			const char* const	enumName	= getFormatName(format);
			const string		caseName	= de::toLower(string(enumName).substr(10));

			params.format = format;

			addFunctionCase(testGroup, caseName, enumName, execImageFormatTest, params);
		}
	}
}

void createImageFormatTypeTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase params)
{
	DE_ASSERT(params.tiling == VK_CORE_IMAGE_TILING_LAST);

	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "optimal",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_OPTIMAL)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "linear",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_LINEAR)));
}

void createImageFormatTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase::Function testFunction)
{
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "1d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_1D, VK_CORE_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "2d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_2D, VK_CORE_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "3d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_3D, VK_CORE_IMAGE_TILING_LAST)));
}


// Android CTS -specific tests

namespace android
{

void checkExtensions (tcu::ResultCollector& results, const set<string>& allowedExtensions, const vector<VkExtensionProperties>& reportedExtensions)
{
	for (vector<VkExtensionProperties>::const_iterator extension = reportedExtensions.begin(); extension != reportedExtensions.end(); ++extension)
	{
		const string	extensionName	(extension->extensionName);
		const bool		mustBeKnown		= de::beginsWith(extensionName, "VK_GOOGLE_")	||
										  de::beginsWith(extensionName, "VK_ANDROID_");

		if (mustBeKnown && !de::contains(allowedExtensions, extensionName))
			results.fail("Unknown extension: " + extensionName);
	}
}

tcu::TestStatus testNoUnknownExtensions (Context& context)
{
	TestLog&				log					= context.getTestContext().getLog();
	tcu::ResultCollector	results				(log);
	set<string>				allowedInstanceExtensions;
	set<string>				allowedDeviceExtensions;

	// All known extensions should be added to allowedExtensions:
	// allowedExtensions.insert("VK_GOOGLE_extension1");
	allowedDeviceExtensions.insert("VK_ANDROID_external_memory_android_hardware_buffer");
	allowedDeviceExtensions.insert("VK_GOOGLE_display_timing");
	allowedDeviceExtensions.insert("VK_GOOGLE_decorate_string");
	allowedDeviceExtensions.insert("VK_GOOGLE_hlsl_functionality1");

	// Instance extensions
	checkExtensions(results,
					allowedInstanceExtensions,
					enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL));

	// Extensions exposed by instance layers
	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			checkExtensions(results,
							allowedInstanceExtensions,
							enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName));
		}
	}

	// Device extensions
	checkExtensions(results,
					allowedDeviceExtensions,
					enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL));

	// Extensions exposed by device layers
	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			checkExtensions(results,
							allowedDeviceExtensions,
							enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName));
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testNoLayers (Context& context)
{
	TestLog&				log		= context.getTestContext().getLog();
	tcu::ResultCollector	results	(log);

	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
			results.fail(string("Instance layer enumerated: ") + layer->layerName);
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
			results.fail(string("Device layer enumerated: ") + layer->layerName);
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testMandatoryExtensions (Context& context)
{
	TestLog&				log		= context.getTestContext().getLog();
	tcu::ResultCollector	results	(log);

	// Instance extensions
	{
		static const string mandatoryExtensions[]	=
		{
			"VK_KHR_get_physical_device_properties2",
		};

		for (const auto &ext : mandatoryExtensions)
		{
			if (!context.isInstanceFunctionalitySupported(ext))
				results.fail(ext + " is not supported");
		}
	}

	// Device extensions
	{
		static const string mandatoryExtensions[] =
		{
			"VK_KHR_maintenance1",
		};

		for (const auto &ext : mandatoryExtensions)
		{
			if (!context.isDeviceFunctionalitySupported(ext))
				results.fail(ext + " is not supported");
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

} // android

} // anonymous

tcu::TestCaseGroup* createFeatureInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	infoTests->addChild(createTestGroup(testCtx, "format_properties",		"VkGetPhysicalDeviceFormatProperties() Tests",		createFormatTests));
	infoTests->addChild(createTestGroup(testCtx, "image_format_properties",	"VkGetPhysicalDeviceImageFormatProperties() Tests",	createImageFormatTests,	imageFormatProperties));

	{
		de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests (new tcu::TestCaseGroup(testCtx, "get_physical_device_properties2", "VK_KHR_get_physical_device_properties2"));

		addFunctionCase(extendedPropertiesTests.get(), "features",					"Extended Device Features",					deviceFeatures2);
		addFunctionCase(extendedPropertiesTests.get(), "properties",				"Extended Device Properties",				deviceProperties2);
		addFunctionCase(extendedPropertiesTests.get(), "format_properties",			"Extended Device Format Properties",		deviceFormatProperties2);
		addFunctionCase(extendedPropertiesTests.get(), "queue_family_properties",	"Extended Device Queue Family Properties",	deviceQueueFamilyProperties2);
		addFunctionCase(extendedPropertiesTests.get(), "memory_properties",			"Extended Device Memory Properties",		deviceMemoryProperties2);

		infoTests->addChild(extendedPropertiesTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests (new tcu::TestCaseGroup(testCtx, "vulkan1p2", "Vulkan 1.2 related tests"));

		addFunctionCase(extendedPropertiesTests.get(), "features",							"Extended Vulkan 1.2 Device Features",						deviceFeaturesVulkan12);
		addFunctionCase(extendedPropertiesTests.get(), "properties",						"Extended Vulkan 1.2 Device Properties",					devicePropertiesVulkan12);
		addFunctionCase(extendedPropertiesTests.get(), "feature_extensions_consistency",	"Vulkan 1.2 consistency between Features and Extensions",	deviceFeatureExtensionsConsistencyVulkan12);
		addFunctionCase(extendedPropertiesTests.get(), "property_extensions_consistency",	"Vulkan 1.2 consistency between Properties and Extensions", devicePropertyExtensionsConsistencyVulkan12);
		addFunctionCase(extendedPropertiesTests.get(), "feature_bits_influence",			"Validate feature bits influence on feature activation",	checkSupportFeatureBitInfluence, featureBitInfluenceOnDeviceCreate);

		infoTests->addChild(extendedPropertiesTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> limitsValidationTests (new tcu::TestCaseGroup(testCtx, "vulkan1p2_limits_validation", "Vulkan 1.2 and core extensions limits validation"));

		addFunctionCase(limitsValidationTests.get(), "general",							"Vulkan 1.2 Limit validation",							validateLimitsCheckSupport,					validateLimits12);
		addFunctionCase(limitsValidationTests.get(), "khr_push_descriptor",				"VK_KHR_push_descriptor limit validation",				checkSupportKhrPushDescriptor,				validateLimitsKhrPushDescriptor);
		addFunctionCase(limitsValidationTests.get(), "khr_multiview",					"VK_KHR_multiview limit validation",					checkSupportKhrMultiview,					validateLimitsKhrMultiview);
		addFunctionCase(limitsValidationTests.get(), "ext_discard_rectangles",			"VK_EXT_discard_rectangles limit validation",			checkSupportExtDiscardRectangles,			validateLimitsExtDiscardRectangles);
		addFunctionCase(limitsValidationTests.get(), "ext_sample_locations",			"VK_EXT_sample_locations limit validation",				checkSupportExtSampleLocations,				validateLimitsExtSampleLocations);
		addFunctionCase(limitsValidationTests.get(), "ext_external_memory_host",		"VK_EXT_external_memory_host limit validation",			checkSupportExtExternalMemoryHost,			validateLimitsExtExternalMemoryHost);
		addFunctionCase(limitsValidationTests.get(), "ext_blend_operation_advanced",	"VK_EXT_blend_operation_advanced limit validation",		checkSupportExtBlendOperationAdvanced,		validateLimitsExtBlendOperationAdvanced);
		addFunctionCase(limitsValidationTests.get(), "khr_maintenance_3",				"VK_KHR_maintenance3 limit validation",					checkSupportKhrMaintenance3,				validateLimitsKhrMaintenance3);
		addFunctionCase(limitsValidationTests.get(), "ext_conservative_rasterization",	"VK_EXT_conservative_rasterization limit validation",	checkSupportExtConservativeRasterization,	validateLimitsExtConservativeRasterization);
		addFunctionCase(limitsValidationTests.get(), "ext_descriptor_indexing",			"VK_EXT_descriptor_indexing limit validation",			checkSupportExtDescriptorIndexing,			validateLimitsExtDescriptorIndexing);
		addFunctionCase(limitsValidationTests.get(), "ext_inline_uniform_block",		"VK_EXT_inline_uniform_block limit validation",			checkSupportExtInlineUniformBlock,			validateLimitsExtInlineUniformBlock);
		addFunctionCase(limitsValidationTests.get(), "ext_vertex_attribute_divisor",	"VK_EXT_vertex_attribute_divisor limit validation",		checkSupportExtVertexAttributeDivisor,		validateLimitsExtVertexAttributeDivisor);
		addFunctionCase(limitsValidationTests.get(), "nv_mesh_shader",					"VK_NV_mesh_shader limit validation",					checkSupportNvMeshShader,					validateLimitsNvMeshShader);
		addFunctionCase(limitsValidationTests.get(), "ext_transform_feedback",			"VK_EXT_transform_feedback limit validation",			checkSupportExtTransformFeedback,			validateLimitsExtTransformFeedback);
		addFunctionCase(limitsValidationTests.get(), "fragment_density_map",			"VK_EXT_fragment_density_map limit validation",			checkSupportExtFragmentDensityMap,			validateLimitsExtFragmentDensityMap);
		addFunctionCase(limitsValidationTests.get(), "nv_ray_tracing",					"VK_NV_ray_tracing limit validation",					checkSupportNvRayTracing,					validateLimitsNvRayTracing);
		addFunctionCase(limitsValidationTests.get(), "timeline_semaphore",				"VK_KHR_timeline_semaphore limit validation",			checkSupportKhrTimelineSemaphore,			validateLimitsKhrTimelineSemaphore);
		addFunctionCase(limitsValidationTests.get(), "ext_line_rasterization",			"VK_EXT_line_rasterization limit validation",			checkSupportExtLineRasterization,			validateLimitsExtLineRasterization);

		infoTests->addChild(limitsValidationTests.release());
	}

	infoTests->addChild(createTestGroup(testCtx, "image_format_properties2",		"VkGetPhysicalDeviceImageFormatProperties2() Tests",		createImageFormatTests, imageFormatProperties2));
	infoTests->addChild(createTestGroup(testCtx, "sparse_image_format_properties2",	"VkGetPhysicalDeviceSparseImageFormatProperties2() Tests",	createImageFormatTests, sparseImageFormatProperties2));

	{
		de::MovePtr<tcu::TestCaseGroup>	androidTests	(new tcu::TestCaseGroup(testCtx, "android", "Android CTS Tests"));

		addFunctionCase(androidTests.get(),	"mandatory_extensions",		"Test that all mandatory extensions are supported",	android::testMandatoryExtensions);
		addFunctionCase(androidTests.get(), "no_unknown_extensions",	"Test for unknown device or instance extensions",	android::testNoUnknownExtensions);
		addFunctionCase(androidTests.get(), "no_layers",				"Test that no layers are enumerated",				android::testNoLayers);

		infoTests->addChild(androidTests.release());
	}

	return infoTests.release();
}

void createFeatureInfoInstanceTests(tcu::TestCaseGroup* testGroup)
{
	addFunctionCase(testGroup, "physical_devices",					"Physical devices",						enumeratePhysicalDevices);
	addFunctionCase(testGroup, "physical_device_groups",			"Physical devices Groups",				enumeratePhysicalDeviceGroups);
	addFunctionCase(testGroup, "instance_layers",					"Layers",								enumerateInstanceLayers);
	addFunctionCase(testGroup, "instance_extensions",				"Extensions",							enumerateInstanceExtensions);
}

void createFeatureInfoDeviceTests(tcu::TestCaseGroup* testGroup)
{
	addFunctionCase(testGroup, "device_features",					"Device Features",						deviceFeatures);
	addFunctionCase(testGroup, "device_properties",					"Device Properties",					deviceProperties);
	addFunctionCase(testGroup, "device_queue_family_properties",	"Queue family properties",				deviceQueueFamilyProperties);
	addFunctionCase(testGroup, "device_memory_properties",			"Memory properties",					deviceMemoryProperties);
	addFunctionCase(testGroup, "device_layers",						"Layers",								enumerateDeviceLayers);
	addFunctionCase(testGroup, "device_extensions",					"Extensions",							enumerateDeviceExtensions);
	addFunctionCase(testGroup, "device_no_khx_extensions",			"KHX extensions",						testNoKhxExtensions);
	addFunctionCase(testGroup, "device_memory_budget",				"Memory budget",						deviceMemoryBudgetProperties);
	addFunctionCase(testGroup, "device_mandatory_features",			"Mandatory features",					deviceMandatoryFeatures);
}

void createFeatureInfoDeviceGroupTests(tcu::TestCaseGroup* testGroup)
{
	addFunctionCase(testGroup, "device_group_peer_memory_features",	"Device Group peer memory features",	deviceGroupPeerMemoryFeatures);
}

} // api
} // vkt
