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
		{ LIMIT(maxCullDistances),									FEATURE(shaderClipDistance),			0, 0, 0, 0.0f },
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
	const PlatformInterface&							vkp				= context.getPlatformInterface();
	const Unique<VkInstance>							instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_device_group_creation"));
	const InstanceDriver								vki				(vkp, *instance);
	const vector<VkPhysicalDeviceGroupProperties>		devicegroups	= enumeratePhysicalDeviceGroups(vki, *instance);

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
			results.fail("Unknown  extension " + *extIter);
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

void checkInstanceExtensionDependencies(tcu::ResultCollector& results,
										int dependencyLength,
										const std::pair<const char*, const char*>* dependencies,
										const vector<VkExtensionProperties>& extensionProperties)
{
	for (int ndx = 0; ndx < dependencyLength; ndx++)
	{
		if (isExtensionSupported(extensionProperties, RequiredExtension(dependencies[ndx].first)) &&
			!isExtensionSupported(extensionProperties, RequiredExtension(dependencies[ndx].second)))
		{
			results.fail("Extension " + string(dependencies[ndx].first) + " is missing dependency: " + string(dependencies[ndx].second));
		}
	}
}

void checkDeviceExtensionDependencies(tcu::ResultCollector& results,
									  int dependencyLength,
									  const std::pair<const char*, const char*>* dependencies,
									  const vector<VkExtensionProperties>& instanceExtensionProperties,
									  const vector<VkExtensionProperties>& deviceExtensionProperties)
{
	for (int ndx = 0; ndx < dependencyLength; ndx++)
	{
		if (isExtensionSupported(deviceExtensionProperties, RequiredExtension(dependencies[ndx].first)) &&
			!isExtensionSupported(deviceExtensionProperties, RequiredExtension(dependencies[ndx].second)) &&
			!isExtensionSupported(instanceExtensionProperties, RequiredExtension(dependencies[ndx].second)))
		{
			results.fail("Extension " + string(dependencies[ndx].first) + " is missing dependency: " + string(dependencies[ndx].second));
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

		if (context.contextSupports(vk::ApiVersion(1, 1, 0)))
		{
			checkInstanceExtensionDependencies(results,
											   DE_LENGTH_OF_ARRAY(instanceExtensionDependencies_1_1),
											   instanceExtensionDependencies_1_1, properties);
		}
		else if (context.contextSupports(vk::ApiVersion(1, 0, 0)))
		{
			checkInstanceExtensionDependencies(results,
											   DE_LENGTH_OF_ARRAY(instanceExtensionDependencies_1_0),
											   instanceExtensionDependencies_1_0, properties);
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

		if (context.contextSupports(vk::ApiVersion(1, 1, 0)))
		{
			checkDeviceExtensionDependencies(results,
											 DE_LENGTH_OF_ARRAY(deviceExtensionDependencies_1_1),
											 deviceExtensionDependencies_1_1,
											 instanceExtensionProperties,
											 deviceExtensionProperties);
		}
		else if (context.contextSupports(vk::ApiVersion(1, 0, 0)))
		{
			checkDeviceExtensionDependencies(results,
											 DE_LENGTH_OF_ARRAY(deviceExtensionDependencies_1_0),
											 deviceExtensionDependencies_1_0,
											 instanceExtensionProperties,
											 deviceExtensionProperties);
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
		const ApiVersion deqpVersion = unpackVersion(VK_API_VERSION_1_1);

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
	const Unique<VkInstance>			instance				(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_device_group_creation"));
	const InstanceDriver				vki						(vkp, *instance);
	const tcu::CommandLine&				cmdLine					= context.getTestContext().getCommandLine();
	const deUint32						devGroupIdx				= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32						deviceIdx				= vk::chooseDeviceIndex(context.getInstanceInterface(), *instance, cmdLine);
	const float							queuePriority			= 1.0f;
	VkPhysicalDeviceMemoryProperties	memProps;
	VkPeerMemoryFeatureFlags*			peerMemFeatures;
	deUint8								buffer					[sizeof(VkPeerMemoryFeatureFlags) + GUARD_SIZE];
	deUint32							numPhysicalDevices		= 0;
	deUint32							queueFamilyIndex		= 0;

	const vector<VkPhysicalDeviceGroupProperties>		deviceGroupProps = enumeratePhysicalDeviceGroups(vki, *instance);
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

	Move<VkDevice>		deviceGroup = createDevice(vkp, *instance, vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &deviceCreateInfo);
	const DeviceDriver	vk	(vkp, *instance, *deviceGroup);
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

	if (!vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_memory_budget"))
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
	if( checkMandatoryFeatures(context) )
		return tcu::TestStatus::pass("Passed");
	return tcu::TestStatus::fail("Not all mandatory features are supported ( see: chapter 35.1 )");
}

VkFormatFeatureFlags getRequiredOptimalTilingFeatures (VkFormat format)
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
		DSAT = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	};

	static const Formatpair formatflags[] =
	{
		{ VK_FORMAT_B4G4R4A4_UNORM_PACK16,		SAIM | BLSR |               SIFL },
		{ VK_FORMAT_R5G6B5_UNORM_PACK16,		SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A1R5G5B5_UNORM_PACK16,		SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8_UNORM,					SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8_SNORM,					SAIM | BLSR |               SIFL },
		{ VK_FORMAT_R8_UINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R8_SINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R8G8_UNORM,					SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R8G8_SNORM,					SAIM | BLSR |               SIFL },
		{ VK_FORMAT_R8G8_UINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R8G8_SINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R8G8B8A8_UNORM,				SAIM | BLSR | COAT | BLDS | SIFL | STIM | CABL },
		{ VK_FORMAT_R8G8B8A8_SNORM,				SAIM | BLSR |               SIFL | STIM },
		{ VK_FORMAT_R8G8B8A8_UINT,				SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R8G8B8A8_SINT,				SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R8G8B8A8_SRGB,				SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_B8G8R8A8_UNORM,				SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_B8G8R8A8_SRGB,				SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A8B8G8R8_UNORM_PACK32,		SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A8B8G8R8_SNORM_PACK32,		SAIM | BLSR |               SIFL },
		{ VK_FORMAT_A8B8G8R8_UINT_PACK32,		SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_A8B8G8R8_SINT_PACK32,		SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_A8B8G8R8_SRGB_PACK32,		SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A2B10G10R10_UNORM_PACK32,	SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_A2B10G10R10_UINT_PACK32,	SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R16_UINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R16_SINT,					SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R16_SFLOAT,					SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R16G16_UINT,				SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R16G16_SINT,				SAIM | BLSR | COAT | BLDS },
		{ VK_FORMAT_R16G16_SFLOAT,				SAIM | BLSR | COAT | BLDS | SIFL |        CABL },
		{ VK_FORMAT_R16G16B16A16_UINT,			SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R16G16B16A16_SINT,			SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R16G16B16A16_SFLOAT,		SAIM | BLSR | COAT | BLDS | SIFL | STIM | CABL },
		{ VK_FORMAT_R32_UINT,					SAIM | BLSR | COAT | BLDS |        STIM |        STIA },
		{ VK_FORMAT_R32_SINT,					SAIM | BLSR | COAT | BLDS |        STIM |        STIA },
		{ VK_FORMAT_R32_SFLOAT,					SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_UINT,				SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_SINT,				SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32_SFLOAT,				SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_UINT,			SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_SINT,			SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_R32G32B32A32_SFLOAT,		SAIM | BLSR | COAT | BLDS |        STIM },
		{ VK_FORMAT_B10G11R11_UFLOAT_PACK32,	SAIM | BLSR |               SIFL },
		{ VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,		SAIM | BLSR |               SIFL },
		{ VK_FORMAT_D16_UNORM,					SAIM | BLSR |                                           DSAT },
		{ VK_FORMAT_D32_SFLOAT,					SAIM | BLSR }
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
				VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT	physicalDeviceSamplerMinMaxProperties =
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

tcu::TestStatus formatProperties (Context& context, VkFormat format)
{
	TestLog&					log					= context.getTestContext().getLog();
	const VkFormatProperties	properties			= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
	bool						allOk				= true;

	// \todo [2017-05-16 pyry] This should be extended to cover for example COLOR_ATTACHMENT for depth formats etc.
	// \todo [2017-05-18 pyry] Any other color conversion related features that can't be supported by regular formats?
	const VkFormatFeatureFlags	extOptimalFeatures	= getRequiredOptimalExtendedTilingFeatures(context, format, properties.optimalTilingFeatures);

	const VkFormatFeatureFlags	notAllowedFeatures	= VK_FORMAT_FEATURE_DISJOINT_BIT;

	const struct
	{
		VkFormatFeatureFlags VkFormatProperties::*	field;
		const char*									fieldName;
		VkFormatFeatureFlags						requiredFeatures;
	} fields[] =
	{
		{ &VkFormatProperties::linearTilingFeatures,	"linearTilingFeatures",		(VkFormatFeatureFlags)0											},
		{ &VkFormatProperties::optimalTilingFeatures,	"optimalTilingFeatures",	getRequiredOptimalTilingFeatures(format) | extOptimalFeatures	},
		{ &VkFormatProperties::bufferFeatures,			"bufferFeatures",			getRequiredBufferFeatures(format)								}
	};

	log << TestLog::Message << properties << TestLog::EndMessage;

	for (int fieldNdx = 0; fieldNdx < DE_LENGTH_OF_ARRAY(fields); fieldNdx++)
	{
		const char* const				fieldName	= fields[fieldNdx].fieldName;
		const VkFormatFeatureFlags		supported	= properties.*fields[fieldNdx].field;
		const VkFormatFeatureFlags		required	= fields[fieldNdx].requiredFeatures;

		if ((supported & required) != required)
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< "  required: " << getFormatFeatureFlagsStr(required) << "\n  "
									<< "  missing: " << getFormatFeatureFlagsStr(~supported & required)
				<< TestLog::EndMessage;
			allOk = false;
		}

		if ((supported & notAllowedFeatures) != 0)
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< "  has: " << getFormatFeatureFlagsStr(supported & notAllowedFeatures)
				<< TestLog::EndMessage;
			allOk = false;
		}
	}

	if (allOk)
		return tcu::TestStatus::pass("Query and validation passed");
	else
		return tcu::TestStatus::fail("Required features not supported");
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
		if (!vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_sampler_ycbcr_conversion"))
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

VkFormatFeatureFlags getAllowedYcbcrFormatFeatures (VkFormat format)
{
	DE_ASSERT(isYCbCrFormat(format));

	VkFormatFeatureFlags	flags	= (VkFormatFeatureFlags)0;

	// all formats *may* support these
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	flags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
	flags |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
	flags |= VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;
	flags |= VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT;
	flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT;

	// multi-plane formats *may* support DISJOINT_BIT
	if (getPlaneCount(format) >= 2)
		flags |= VK_FORMAT_FEATURE_DISJOINT_BIT;

	if (isChromaSubsampled(format))
		flags |= VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;

	return flags;
}

tcu::TestStatus ycbcrFormatProperties (Context& context, VkFormat format)
{
	DE_ASSERT(isYCbCrFormat(format));
	// check if Ycbcr format enums are valid given the version and extensions
	checkYcbcrApiSupport(context);

	TestLog&					log						= context.getTestContext().getLog();
	const VkFormatProperties	properties				= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
	bool						allOk					= true;
	const VkFormatFeatureFlags	allowedImageFeatures	= getAllowedYcbcrFormatFeatures(format);

	const struct
	{
		VkFormatFeatureFlags VkFormatProperties::*	field;
		const char*									fieldName;
		bool										requiredFeatures;
		VkFormatFeatureFlags						allowedFeatures;
	} fields[] =
	{
		{ &VkFormatProperties::linearTilingFeatures,	"linearTilingFeatures",		false,	allowedImageFeatures	},
		{ &VkFormatProperties::optimalTilingFeatures,	"optimalTilingFeatures",	true,	allowedImageFeatures	},
		{ &VkFormatProperties::bufferFeatures,			"bufferFeatures",			false,	(VkFormatFeatureFlags)0	}
	};
	static const VkFormat		s_requiredBaseFormats[]	=
	{
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM
	};
	const bool					isRequiredBaseFormat	= isYcbcrConversionSupported(context) &&
														  de::contains(DE_ARRAY_BEGIN(s_requiredBaseFormats), DE_ARRAY_END(s_requiredBaseFormats), format);

	log << TestLog::Message << properties << TestLog::EndMessage;

	for (int fieldNdx = 0; fieldNdx < DE_LENGTH_OF_ARRAY(fields); fieldNdx++)
	{
		const char* const				fieldName	= fields[fieldNdx].fieldName;
		const VkFormatFeatureFlags		supported	= properties.*fields[fieldNdx].field;
		const VkFormatFeatureFlags		allowed		= fields[fieldNdx].allowedFeatures;

		if (isRequiredBaseFormat && fields[fieldNdx].requiredFeatures)
		{
			const VkFormatFeatureFlags	required	= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
													| VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
													| VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

			if ((supported & required) != required)
			{
				log << TestLog::Message << "ERROR in " << fieldName << ":\n"
										<< "  required: " << getFormatFeatureFlagsStr(required) << "\n  "
										<< "  missing: " << getFormatFeatureFlagsStr(~supported & required)
					<< TestLog::EndMessage;
				allOk = false;
			}

			if ((supported & (VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0)
			{
				log << TestLog::Message << "ERROR in " << fieldName << ":\n"
										<< "  Either VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT or VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT required"
					<< TestLog::EndMessage;
				allOk = false;
			}
		}

		if ((supported & ~allowed) != 0)
		{
			log << TestLog::Message << "ERROR in " << fieldName << ":\n"
									<< "  has: " << getFormatFeatureFlagsStr(supported & ~allowed)
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
		const char* const	setName			= s_compressedFormatSets[setNdx].setName;
		const char* const	featureName		= s_compressedFormatSets[setNdx].featureName;
		const bool			featureBitSet	= features.*s_compressedFormatSets[setNdx].feature == VK_TRUE;
		const bool			allSupported	= optimalTilingFeaturesSupportedForAll(context,
																				   s_compressedFormatSets[setNdx].formatsBegin,
																				   s_compressedFormatSets[setNdx].formatsEnd,
																				   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

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
		VkFormat								begin;
		VkFormat								end;
		FunctionInstance1<VkFormat>::Function	testFunction;
	} s_formatRanges[] =
	{
		// core formats
		{ (VkFormat)(VK_FORMAT_UNDEFINED+1),	VK_CORE_FORMAT_LAST,										formatProperties },

		// YCbCr formats
		{ VK_FORMAT_G8B8G8R8_422_UNORM,			(VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM+1),	ycbcrFormatProperties },
	};

	for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
	{
		const VkFormat								rangeBegin		= s_formatRanges[rangeNdx].begin;
		const VkFormat								rangeEnd		= s_formatRanges[rangeNdx].end;
		const FunctionInstance1<VkFormat>::Function	testFunction	= s_formatRanges[rangeNdx].testFunction;

		for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format+1))
		{
			const char* const	enumName	= getFormatName(format);
			const string		caseName	= de::toLower(string(enumName).substr(10));

			addFunctionCase(testGroup, caseName, enumName, testFunction, format);
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
		, imageType		(VK_IMAGE_TYPE_LAST)
		, tiling		(VK_IMAGE_TILING_LAST)
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
	const bool						hasKhrMaintenance1	= isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_maintenance1");

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
				const deUint32	fullMipPyramidSize	= de::max(de::max(deLog2Ceil32(properties.maxExtent.width),
																	  deLog2Ceil32(properties.maxExtent.height)),
															  deLog2Ceil32(properties.maxExtent.depth)) + 1;

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
		if (strcmp(properties[ndx].extensionName, extension) == 0)
			return true;
	}
	return false;
}

tcu::TestStatus deviceFeatures2 (Context& context)
{
	const PlatformInterface&	vkp				= context.getPlatformInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>	instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver		vki				(vkp, *instance);
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

	vector<VkExtensionProperties>	properties = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
	const bool khr_8bit_storage				= checkExtension(properties,"VK_KHR_8bit_storage");
	const bool ext_conditional_rendering	= checkExtension(properties,"VK_EXT_conditional_rendering");
	const bool scalar_block_layout			= checkExtension(properties,"VK_EXT_scalar_block_layout");
	bool khr_16bit_storage					= true;
	bool khr_multiview						= true;
	bool deviceProtectedMemory				= true;
	bool sampler_ycbcr_conversion			= true;
	bool variable_pointers					= true;
	if (getPhysicalDeviceProperties(vki, physicalDevice).apiVersion < VK_API_VERSION_1_1)
	{
		khr_16bit_storage = checkExtension(properties,"VK_KHR_16bit_storage");
		khr_multiview = checkExtension(properties,"VK_KHR_multiview");
		deviceProtectedMemory = false;
		sampler_ycbcr_conversion = checkExtension(properties,"VK_KHR_sampler_ycbcr_conversion");
		variable_pointers = checkExtension(properties,"VK_KHR_variable_pointers");
	}

	const int count = 2u;
	VkPhysicalDevice8BitStorageFeaturesKHR				device8BitStorageFeatures[count];
	VkPhysicalDeviceConditionalRenderingFeaturesEXT		deviceConditionalRenderingFeatures[count];
	VkPhysicalDevice16BitStorageFeatures				device16BitStorageFeatures[count];
	VkPhysicalDeviceMultiviewFeatures					deviceMultiviewFeatures[count];
	VkPhysicalDeviceProtectedMemoryFeatures				protectedMemoryFeatures[count];
	VkPhysicalDeviceSamplerYcbcrConversionFeatures		samplerYcbcrConversionFeatures[count];
	VkPhysicalDeviceVariablePointersFeatures			variablePointerFeatures[count];
	VkPhysicalDeviceScalarBlockLayoutFeaturesEXT		scalarBlockLayoutFeatures[count];

	for (int ndx = 0; ndx < count; ++ndx)
	{
		deMemset(&device8BitStorageFeatures[ndx],			0xFF*ndx, sizeof(VkPhysicalDevice8BitStorageFeaturesKHR));
		deMemset(&deviceConditionalRenderingFeatures[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceConditionalRenderingFeaturesEXT));
		deMemset(&device16BitStorageFeatures[ndx],			0xFF*ndx, sizeof(VkPhysicalDevice16BitStorageFeatures));
		deMemset(&deviceMultiviewFeatures[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceMultiviewFeatures));
		deMemset(&protectedMemoryFeatures[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceProtectedMemoryFeatures));
		deMemset(&samplerYcbcrConversionFeatures[ndx],		0xFF*ndx, sizeof(VkPhysicalDeviceSamplerYcbcrConversionFeatures));
		deMemset(&variablePointerFeatures[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceVariablePointersFeatures));
		deMemset(&scalarBlockLayoutFeatures[ndx],			0xFF*ndx, sizeof(VkPhysicalDeviceScalarBlockLayoutFeaturesEXT));

		device8BitStorageFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR;
		device8BitStorageFeatures[ndx].pNext = &deviceConditionalRenderingFeatures[ndx];

		deviceConditionalRenderingFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
		deviceConditionalRenderingFeatures[ndx].pNext = &device16BitStorageFeatures[ndx];

		device16BitStorageFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
		device16BitStorageFeatures[ndx].pNext = &deviceMultiviewFeatures[ndx];

		deviceMultiviewFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
		deviceMultiviewFeatures[ndx].pNext = &protectedMemoryFeatures[ndx];

		protectedMemoryFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
		protectedMemoryFeatures[ndx].pNext = &samplerYcbcrConversionFeatures[ndx];

		samplerYcbcrConversionFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
		samplerYcbcrConversionFeatures[ndx].pNext = &variablePointerFeatures[ndx];

		variablePointerFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES;
		variablePointerFeatures[ndx].pNext = &scalarBlockLayoutFeatures[ndx];

		scalarBlockLayoutFeatures[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
		scalarBlockLayoutFeatures[ndx].pNext = DE_NULL;

		deMemset(&extFeatures.features, 0xcd, sizeof(extFeatures.features));
		extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		extFeatures.pNext = &device8BitStorageFeatures[ndx];

		vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);
	}

	if ( khr_8bit_storage &&
		(device8BitStorageFeatures[0].storageBuffer8BitAccess				!= device8BitStorageFeatures[1].storageBuffer8BitAccess ||
		device8BitStorageFeatures[0].uniformAndStorageBuffer8BitAccess		!= device8BitStorageFeatures[1].uniformAndStorageBuffer8BitAccess ||
		device8BitStorageFeatures[0].storagePushConstant8					!= device8BitStorageFeatures[1].storagePushConstant8 )
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDevice8BitStorageFeatures");
	}

	if ( ext_conditional_rendering &&
		(deviceConditionalRenderingFeatures[0].conditionalRendering				!= deviceConditionalRenderingFeatures[1].conditionalRendering ||
		deviceConditionalRenderingFeatures[0].inheritedConditionalRendering		!= deviceConditionalRenderingFeatures[1].inheritedConditionalRendering )
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceConditionalRenderingFeaturesEXT");
	}

	if ( khr_16bit_storage &&
		(device16BitStorageFeatures[0].storageBuffer16BitAccess				!= device16BitStorageFeatures[1].storageBuffer16BitAccess ||
		device16BitStorageFeatures[0].uniformAndStorageBuffer16BitAccess	!= device16BitStorageFeatures[1].uniformAndStorageBuffer16BitAccess ||
		device16BitStorageFeatures[0].storagePushConstant16					!= device16BitStorageFeatures[1].storagePushConstant16 ||
		device16BitStorageFeatures[0].storageInputOutput16					!= device16BitStorageFeatures[1].storageInputOutput16)
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDevice16BitStorageFeatures");
	}

	if (khr_multiview &&
		(deviceMultiviewFeatures[0].multiview					!= deviceMultiviewFeatures[1].multiview ||
		deviceMultiviewFeatures[0].multiviewGeometryShader		!= deviceMultiviewFeatures[1].multiviewGeometryShader ||
		deviceMultiviewFeatures[0].multiviewTessellationShader	!= deviceMultiviewFeatures[1].multiviewTessellationShader)
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewFeatures");
	}

	if (deviceProtectedMemory && protectedMemoryFeatures[0].protectedMemory != protectedMemoryFeatures[1].protectedMemory)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryFeatures");
	}

	if (sampler_ycbcr_conversion && samplerYcbcrConversionFeatures[0].samplerYcbcrConversion != samplerYcbcrConversionFeatures[1].samplerYcbcrConversion)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerYcbcrConversionFeatures");
	}

	if (variable_pointers &&
		(variablePointerFeatures[0].variablePointersStorageBuffer	!= variablePointerFeatures[1].variablePointersStorageBuffer ||
		variablePointerFeatures[0].variablePointers					!= variablePointerFeatures[1].variablePointers)
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceVariablePointersFeatures");
	}
	if (scalar_block_layout &&
		(scalarBlockLayoutFeatures[0].scalarBlockLayout	!= scalarBlockLayoutFeatures[1].scalarBlockLayout))
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceScalarBlockLayoutFeaturesEXT");
	}
	if (khr_8bit_storage)
		log << TestLog::Message << device8BitStorageFeatures[0]		<< TestLog::EndMessage;
	if (ext_conditional_rendering)
		log << TestLog::Message << deviceConditionalRenderingFeatures[0]		<< TestLog::EndMessage;
	if (khr_16bit_storage)
		log << TestLog::Message << device16BitStorageFeatures[0]		<< TestLog::EndMessage;
	if (khr_multiview)
		log << TestLog::Message << deviceMultiviewFeatures[0]			<< TestLog::EndMessage;
	if (deviceProtectedMemory)
		log << TestLog::Message << protectedMemoryFeatures[0]			<< TestLog::EndMessage;
	if (sampler_ycbcr_conversion)
		log << TestLog::Message << samplerYcbcrConversionFeatures[0]	<< TestLog::EndMessage;
	if (variable_pointers)
		log << TestLog::Message << variablePointerFeatures[0]			<< TestLog::EndMessage;
	if (scalar_block_layout)
		log << TestLog::Message << scalarBlockLayoutFeatures[0]			<< TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying device features succeeded");
}


tcu::TestStatus deviceProperties2 (Context& context)
{
	const PlatformInterface&		vkp				= context.getPlatformInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>		instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki				(vkp, *instance);
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

	bool khr_external_memory_capabilities		= true;
	bool khr_multiview							= true;
	bool khr_maintenance2						= true;
	bool khr_maintenance3						= true;
	bool apiVersionSmallerThen_1_1				= (getPhysicalDeviceProperties(vki, physicalDevice).apiVersion < VK_API_VERSION_1_1);
	if (apiVersionSmallerThen_1_1)
	{
		vector<VkExtensionProperties> properties	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);
		khr_external_memory_capabilities			= checkExtension(properties,"VK_KHR_external_memory_capabilities");
		khr_multiview								= checkExtension(properties,"VK_KHR_multiview");
		khr_maintenance2							= checkExtension(properties,"VK_KHR_maintenance2");
		khr_maintenance3							= checkExtension(properties,"VK_KHR_maintenance3");
	}

	VkPhysicalDeviceIDProperties				IDProperties[count];
	VkPhysicalDeviceMaintenance3Properties		maintenance3Properties[count];
	VkPhysicalDeviceMultiviewProperties			multiviewProperties[count];
	VkPhysicalDevicePointClippingProperties		pointClippingProperties[count];
	VkPhysicalDeviceProtectedMemoryProperties	protectedMemoryPropertiesKHR[count];
	VkPhysicalDeviceSubgroupProperties			subgroupProperties[count];

	for (int ndx = 0; ndx < count; ++ndx)
	{
		deMemset(&IDProperties[ndx],					0xFF*ndx, sizeof(VkPhysicalDeviceIDProperties				));
		deMemset(&maintenance3Properties[ndx],			0xFF*ndx, sizeof(VkPhysicalDeviceMaintenance3Properties		));
		deMemset(&multiviewProperties[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceMultiviewProperties		));
		deMemset(&pointClippingProperties[ndx],			0xFF*ndx, sizeof(VkPhysicalDevicePointClippingProperties	));
		deMemset(&protectedMemoryPropertiesKHR[ndx],	0xFF*ndx, sizeof(VkPhysicalDeviceProtectedMemoryProperties	));
		deMemset(&subgroupProperties[ndx],				0xFF*ndx, sizeof(VkPhysicalDeviceSubgroupProperties			));

		IDProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
		IDProperties[ndx].pNext = &maintenance3Properties[ndx];

		maintenance3Properties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
		maintenance3Properties[ndx].pNext = &multiviewProperties[ndx];

		multiviewProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
		multiviewProperties[ndx].pNext = &pointClippingProperties[ndx];

		pointClippingProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
		pointClippingProperties[ndx].pNext = &protectedMemoryPropertiesKHR[ndx];

		protectedMemoryPropertiesKHR[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
		protectedMemoryPropertiesKHR[ndx].pNext = &subgroupProperties[ndx];

		subgroupProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties[ndx].pNext = DE_NULL;

		extProperties.pNext = &IDProperties[ndx];

		vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

		IDProperties[ndx].pNext						= DE_NULL;
		maintenance3Properties[ndx].pNext			= DE_NULL;
		multiviewProperties[ndx].pNext				= DE_NULL;
		pointClippingProperties[ndx].pNext			= DE_NULL;
		protectedMemoryPropertiesKHR[ndx].pNext		= DE_NULL;
		subgroupProperties[ndx].pNext				= DE_NULL;
	}

	if (khr_external_memory_capabilities)
	{
		if ((deMemCmp(IDProperties[0].deviceUUID, IDProperties[1].deviceUUID, VK_UUID_SIZE) != 0) ||
			(deMemCmp(IDProperties[0].driverUUID, IDProperties[1].driverUUID, VK_UUID_SIZE) != 0) ||
			(IDProperties[0].deviceLUIDValid	!= IDProperties[1].deviceLUIDValid))
		{
			TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
		}
		else if (IDProperties[0].deviceLUIDValid)
		{
			// If deviceLUIDValid is VK_FALSE, the contents of deviceLUID and deviceNodeMask are undefined
			// so thay can only be compared when deviceLUIDValid is VK_TRUE.
			if ((deMemCmp(IDProperties[0].deviceLUID, IDProperties[1].deviceLUID, VK_UUID_SIZE) != 0) ||
				(IDProperties[0].deviceNodeMask		!= IDProperties[1].deviceNodeMask))
			{
				TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
			}
		}
	}
	if (khr_maintenance3 &&
		((maintenance3Properties[0].maxPerSetDescriptors	!= maintenance3Properties[1].maxPerSetDescriptors) ||
		(maintenance3Properties[0].maxMemoryAllocationSize	!= maintenance3Properties[1].maxMemoryAllocationSize))
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance3Properties");
	}
	if (khr_multiview &&
		((multiviewProperties[0].maxMultiviewViewCount		!= multiviewProperties[1].maxMultiviewViewCount) ||
		(multiviewProperties[0].maxMultiviewInstanceIndex	!= multiviewProperties[1].maxMultiviewInstanceIndex))
		)
	{
		TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewProperties");
	}
	if (khr_maintenance2 &&
		(pointClippingProperties[0].pointClippingBehavior != pointClippingProperties[1].pointClippingBehavior))
	{
		TCU_FAIL("Mismatch between VkPhysicalDevicePointClippingProperties");
	}
	if (!apiVersionSmallerThen_1_1)
	{
		if(protectedMemoryPropertiesKHR[0].protectedNoFault != protectedMemoryPropertiesKHR[1].protectedNoFault)
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
	}

	if (khr_external_memory_capabilities)
		log << TestLog::Message << IDProperties[0]					<< TestLog::EndMessage;
	if (khr_maintenance3)
		log << TestLog::Message << maintenance3Properties[0]			<< TestLog::EndMessage;
	if (khr_multiview)
		log << TestLog::Message << multiviewProperties[0]				<< TestLog::EndMessage;
	if (khr_maintenance2)
		log << TestLog::Message << pointClippingProperties[0]			<< TestLog::EndMessage;
	if (!apiVersionSmallerThen_1_1)
	{
		log << TestLog::Message << protectedMemoryPropertiesKHR[0]	<< TestLog::EndMessage
			<< TestLog::Message << subgroupProperties[0]				<< TestLog::EndMessage;
	}

	const vector<VkExtensionProperties>	extensions = enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	if (isExtensionSupported(extensions, RequiredExtension("VK_KHR_push_descriptor")))
	{
		VkPhysicalDevicePushDescriptorPropertiesKHR		pushDescriptorProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			deMemset(&pushDescriptorProperties[ndx], 0, sizeof(VkPhysicalDevicePushDescriptorPropertiesKHR));

			pushDescriptorProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
			pushDescriptorProperties[ndx].pNext	= DE_NULL;

			extProperties.pNext = &pushDescriptorProperties[ndx];

			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

			pushDescriptorProperties[ndx].pNext = DE_NULL;
		}

		if (deMemCmp(&pushDescriptorProperties[0], &pushDescriptorProperties[1], sizeof(VkPhysicalDevicePushDescriptorPropertiesKHR)) != 0)
		{
			TCU_FAIL("Mismatch in vkGetPhysicalDeviceProperties2 in VkPhysicalDevicePushDescriptorPropertiesKHR ");
		}

		log << TestLog::Message << pushDescriptorProperties[0] << TestLog::EndMessage;

		if (pushDescriptorProperties[0].maxPushDescriptors < 32)
		{
			TCU_FAIL("VkPhysicalDevicePushDescriptorPropertiesKHR.maxPushDescriptors must be at least 32");
		}
	}
	if (isExtensionSupported(extensions, RequiredExtension("VK_KHR_shader_float_controls")))
	{
		VkPhysicalDeviceFloatControlsPropertiesKHR floatControlsProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			deMemset(&floatControlsProperties[ndx], 0xFF, sizeof(VkPhysicalDeviceFloatControlsPropertiesKHR));
			floatControlsProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR;
			floatControlsProperties[ndx].pNext = DE_NULL;

			extProperties.pNext = &floatControlsProperties[ndx];

			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
		}

		if (deMemCmp(&floatControlsProperties[0], &floatControlsProperties[1], sizeof(VkPhysicalDeviceFloatControlsPropertiesKHR)) != 0)
		{
			TCU_FAIL("Mismatch in VkPhysicalDeviceFloatControlsPropertiesKHR");
		}

		log << TestLog::Message << floatControlsProperties[0] << TestLog::EndMessage;
	}

	if (isExtensionSupported(extensions, RequiredExtension("VK_KHR_depth_stencil_resolve")))
	{
		VkPhysicalDeviceDepthStencilResolvePropertiesKHR  dsResolveProperties[count];

		for (int ndx = 0; ndx < count; ++ndx)
		{
			deMemset(&dsResolveProperties[ndx], 0xFF, sizeof(VkPhysicalDeviceDepthStencilResolvePropertiesKHR));
			dsResolveProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR;
			dsResolveProperties[ndx].pNext = DE_NULL;

			extProperties.pNext = &dsResolveProperties[ndx];

			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
		}

		if (deMemCmp(&dsResolveProperties[0], &dsResolveProperties[1], sizeof(VkPhysicalDeviceDepthStencilResolvePropertiesKHR)) != 0)
		{
			TCU_FAIL("Mismatch in VkPhysicalDeviceDepthStencilResolvePropertiesKHR");
		}

		log << TestLog::Message << dsResolveProperties[0] << TestLog::EndMessage;
	}

	if (isExtensionSupported(extensions, RequiredExtension("VK_EXT_pci_bus_info", 2, 2)))
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
			deMemset(pciBusInfoProperties + ndx, 0, sizeof(pciBusInfoProperties[ndx]));
			pciBusInfoProperties[ndx].pciDomain   = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciBus      = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciDevice   = DEUINT32_MAX;
			pciBusInfoProperties[ndx].pciFunction = DEUINT32_MAX;

			pciBusInfoProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT;
			pciBusInfoProperties[ndx].pNext = DE_NULL;

			extProperties.pNext = pciBusInfoProperties + ndx;
			vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
		}

		if (deMemCmp(pciBusInfoProperties + 0, pciBusInfoProperties + 1, sizeof(pciBusInfoProperties[0])) != 0)
		{
			TCU_FAIL("Mismatch in VkPhysicalDevicePCIBusInfoPropertiesEXT");
		}

		log << TestLog::Message << toString(pciBusInfoProperties[0]) << TestLog::EndMessage;

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
	const PlatformInterface&		vkp				= context.getPlatformInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>		instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki				(vkp, *instance);
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
	const PlatformInterface&		vkp						= context.getPlatformInterface();
	const VkPhysicalDevice			physicalDevice			= context.getPhysicalDevice();
	const Unique<VkInstance>		instance				(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki						(vkp, *instance);
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
	const PlatformInterface&			vkp				= context.getPlatformInterface();
	const VkPhysicalDevice				physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>			instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver				vki				(vkp, *instance);
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

tcu::TestStatus imageFormatProperties2 (Context& context, const VkFormat format, const VkImageType imageType, const VkImageTiling tiling)
{
	if (isYCbCrFormat(format))
		// check if Ycbcr format enums are valid given the version and extensions
		checkYcbcrApiSupport(context);

	TestLog&						log				= context.getTestContext().getLog();

	const PlatformInterface&		vkp				= context.getPlatformInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>		instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki				(vkp, *instance);

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

	const PlatformInterface&		vkp				= context.getPlatformInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const Unique<VkInstance>		instance		(createInstanceWithExtension(vkp, context.getUsedApiVersion(), "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki				(vkp, *instance);

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

			deUint32										numCoreProperties	= ~0u;
			deUint32										numExtProperties	= ~0u;

			// Query count
			vki.getPhysicalDeviceSparseImageFormatProperties(physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.samples, imageFormatInfo.usage, imageFormatInfo.tiling, &numCoreProperties, DE_NULL);
			vki.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &imageFormatInfo, &numExtProperties, DE_NULL);

			if (numCoreProperties != numExtProperties)
			{
				log << TestLog::Message << "ERROR: different number of properties reported for " << imageFormatInfo << TestLog::EndMessage;
				TCU_FAIL("Mismatch in reported property count");
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
		{ (VkFormat)(VK_FORMAT_UNDEFINED + 1),	VK_CORE_FORMAT_LAST,										params },

		// YCbCr formats
		{ VK_FORMAT_G8B8G8R8_422_UNORM_KHR,		(VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR + 1),	params }
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
	DE_ASSERT(params.tiling == VK_IMAGE_TILING_LAST);

	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "optimal",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_OPTIMAL)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "linear",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_LINEAR)));
}

void createImageFormatTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase::Function testFunction)
{
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "1d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "2d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "3d", "", createImageFormatTypeTests, ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_LAST)));
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
		static const char*					mandatoryExtensions[]	=
		{
			"VK_KHR_get_physical_device_properties2",
		};
		const vector<VkExtensionProperties>	extensions				= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(mandatoryExtensions); ++ndx)
		{
			if (!isInstanceExtensionSupported(context.getUsedApiVersion(), extensions, RequiredExtension(mandatoryExtensions[ndx])))
				results.fail(string(mandatoryExtensions[ndx]) + " is not supported");
		}
	}

	// Device extensions
	{
		static const char*					mandatoryExtensions[]	=
		{
			"VK_KHR_maintenance1",
		};
		const vector<VkExtensionProperties>	extensions				= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(mandatoryExtensions); ++ndx)
		{
			if (!isDeviceExtensionSupported(context.getUsedApiVersion(), extensions, RequiredExtension(mandatoryExtensions[ndx])))
				results.fail(string(mandatoryExtensions[ndx]) + " is not supported");
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

} // android

} // anonymous

tcu::TestCaseGroup* createFeatureInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	{
		de::MovePtr<tcu::TestCaseGroup> instanceInfoTests	(new tcu::TestCaseGroup(testCtx, "instance", "Instance Information Tests"));

		addFunctionCase(instanceInfoTests.get(), "physical_devices",		"Physical devices",			enumeratePhysicalDevices);
		addFunctionCase(instanceInfoTests.get(), "physical_device_groups",	"Physical devices Groups",	enumeratePhysicalDeviceGroups);
		addFunctionCase(instanceInfoTests.get(), "layers",					"Layers",					enumerateInstanceLayers);
		addFunctionCase(instanceInfoTests.get(), "extensions",				"Extensions",				enumerateInstanceExtensions);

		infoTests->addChild(instanceInfoTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> deviceInfoTests	(new tcu::TestCaseGroup(testCtx, "device", "Device Information Tests"));

		addFunctionCase(deviceInfoTests.get(), "features",					"Device Features",			deviceFeatures);
		addFunctionCase(deviceInfoTests.get(), "properties",				"Device Properties",		deviceProperties);
		addFunctionCase(deviceInfoTests.get(), "queue_family_properties",	"Queue family properties",	deviceQueueFamilyProperties);
		addFunctionCase(deviceInfoTests.get(), "memory_properties",			"Memory properties",		deviceMemoryProperties);
		addFunctionCase(deviceInfoTests.get(), "layers",					"Layers",					enumerateDeviceLayers);
		addFunctionCase(deviceInfoTests.get(), "extensions",				"Extensions",				enumerateDeviceExtensions);
		addFunctionCase(deviceInfoTests.get(), "no_khx_extensions",			"KHX extensions",			testNoKhxExtensions);
		addFunctionCase(deviceInfoTests.get(), "memory_budget",				"Memory budget",			deviceMemoryBudgetProperties);
		addFunctionCase(deviceInfoTests.get(), "mandatory_features",		"Mandatory features",		deviceMandatoryFeatures);

		infoTests->addChild(deviceInfoTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> deviceGroupInfoTests(new tcu::TestCaseGroup(testCtx, "device_group", "Device Group Information Tests"));

		addFunctionCase(deviceGroupInfoTests.get(), "peer_memory_features",	"Device Group peer memory features",				deviceGroupPeerMemoryFeatures);

		infoTests->addChild(deviceGroupInfoTests.release());
	}

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

} // api
} // vkt
