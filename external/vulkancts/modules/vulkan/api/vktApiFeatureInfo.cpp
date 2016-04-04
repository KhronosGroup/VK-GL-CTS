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
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"
#include "deMemory.h"
#include "deMath.h"

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;
using std::vector;
using std::string;
using tcu::TestLog;
using tcu::ScopedLogSection;

enum
{
	GUARD_SIZE	= 0x20,			//!< Number of bytes to check
	GUARD_VALUE	= 0xcd,			//!< Data pattern
};

enum LimitFormat
{
	LIMIT_FORMAT_SIGNED_INT,
	LIMIT_FORMAT_UNSIGNED_INT,
	LIMIT_FORMAT_FLOAT,
	LIMIT_FORMAT_DEVICE_SIZE,

	LIMIT_FORMAT_LAST
};

enum LimitType
{
	LIMIT_TYPE_MIN,
	LIMIT_TYPE_MAX,

	LIMIT_TYPE_LAST
};

#define LIMIT(_X_)		DE_OFFSET_OF(VkPhysicalDeviceLimits, _X_),(char*)(#_X_)
#define FEATURE(_X_)	DE_OFFSET_OF(VkPhysicalDeviceFeatures, _X_)

bool validateFeatureLimits(VkPhysicalDeviceProperties* properties, VkPhysicalDeviceFeatures* features, TestLog& log)
{
	bool					limitsOk	= true;
	VkPhysicalDeviceLimits* limits		= &properties->limits;
	struct FeatureLimitTable
	{
		deUint32		offset;
		char*			name;
		deUint32		uintVal;			//!< Format is UNSIGNED_INT
		deInt32			intVal;				//!< Format is SIGNED_INT
		deUint64		deviceSizeVal;		//!< Format is DEVICE_SIZE
		float			floatVal;			//!< Format is FLOAT
		LimitFormat		format;
		LimitType		type;
		deInt32			unsuppTableNdx;
	} featureLimitTable[] =   //!< From gitlab.khronos.org/vulkan/vulkan.git:doc/specs/vulkan/chapters/features.txt@63b23f3bb3ecd211cd6e448e2001ce1088dacd35
	{
		{ LIMIT(maxImageDimension1D),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxImageDimension2D),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxImageDimension3D),								256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxImageDimensionCube),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxImageArrayLayers),								256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   , -1 },
		{ LIMIT(maxTexelBufferElements),							65536, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxUniformBufferRange),								16384, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxPushConstantsSize),								128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxMemoryAllocationCount),							4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(bufferImageGranularity),							0, 0, 131072, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(sparseAddressSpaceSize),							0, 0, 2UL*1024*1024*1024, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxBoundDescriptorSets),							4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxPerStageDescriptorSamplers),						16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxPerStageDescriptorUniformBuffers),				12, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorStorageBuffers),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorSampledImages),				16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorStorageImages),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxPerStageDescriptorInputAttachments),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxDescriptorSetSamplers),							96, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetUniformBuffers),					72, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxDescriptorSetUniformBuffersDynamic),				8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxDescriptorSetStorageBuffers),					24, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN , -1 },
		{ LIMIT(maxDescriptorSetStorageBuffersDynamic),				4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxDescriptorSetSampledImages),						96, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
		{ LIMIT(maxDescriptorSetStorageImages),						24, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  , -1 },
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
		{ LIMIT(minTexelBufferOffsetAlignment),						256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minUniformBufferOffsetAlignment),					256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minStorageBufferOffsetAlignment),					256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(minTexelOffset),									0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxTexelOffset),									7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minTexelGatherOffset),								0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxTexelGatherOffset),								7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(minInterpolationOffset),							0, 0, 0, -0.5f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(maxInterpolationOffset),							0, 0, 0, 0.5f - (1.0f/deFloatPow(2.0f, (float)limits->subPixelInterpolationOffsetBits)), LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(subPixelInterpolationOffsetBits),					4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferWidth),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferHeight),								4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxFramebufferLayers),								256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxColorAttachments),								4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxSampleMaskWords),								1, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxClipDistances),									8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxCullDistances),									8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(maxCombinedClipAndCullDistances),					8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(pointSizeRange[1]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(pointSizeRange[1]),									0, 0, 0, 64.0f - limits->pointSizeGranularity , LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(lineWidthRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(lineWidthRange[1]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(lineWidthRange[0]),									0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(lineWidthRange[1]),									0, 0, 0, 8.0f - limits->lineWidthGranularity, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1 },
		{ LIMIT(pointSizeGranularity),								0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(lineWidthGranularity),								0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1 },
		{ LIMIT(nonCoherentAtomSize),								0, 0, 128, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1 },
	};

	struct UnsupportedFeatureLimitTable
	{
		deUint32		limitOffset;
		char*			name;
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
		{ LIMIT(storageImageSampleCounts),							FEATURE(shaderStorageImageMultisample),	0, 0, 0, 0.0f },
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
					if (*((VkBool32*)((char*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].uintVal;
				}

				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{

					if (*((deUint32*)((char*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit Validation failed " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((deUint32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deUint32*)((char*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed,  " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX - actual is "
							<< *((deUint32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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
					if (*((VkBool32*)((char*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].floatVal;
				}

				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{
					if (*((float*)((char*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((float*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((float*)((char*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((float*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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
					if (*((VkBool32*)((char*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].intVal;
				}
				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if (*((deInt32*)((char*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message <<  "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deInt32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deInt32*)((char*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deInt32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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
					if (*((VkBool32*)((char*)features+unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) == VK_FALSE)
						limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].deviceSizeVal;
				}

				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{
					if (*((deUint64*)((char*)limits+featureLimitTable[ndx].offset)) < limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deUint64*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deUint64*)((char*)limits+featureLimitTable[ndx].offset)) > limitToCheck)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deUint64*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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

	return limitsOk;
}

tcu::TestStatus enumeratePhysicalDevices (Context& context)
{
	TestLog&						log		= context.getTestContext().getLog();
	const vector<VkPhysicalDevice>	devices	= enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestStatus enumerateInstanceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= enumerateInstanceLayerProperties(context.getPlatformInterface());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateInstanceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus enumerateDeviceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= vk::enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateDeviceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus deviceFeatures (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceFeatures*		features;
	deUint8							buffer[sizeof(VkPhysicalDeviceFeatures) + GUARD_SIZE];

	deMemset(buffer, GUARD_VALUE, sizeof(buffer));
	features = reinterpret_cast<VkPhysicalDeviceFeatures*>(buffer);

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), features);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *features << TestLog::EndMessage;

	for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceFeatures)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceFeatures - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceFeatures buffer overflow");
		}
	}

	return tcu::TestStatus::pass("Query succeeded");
}

tcu::TestStatus deviceProperties (Context& context)
{
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

	return tcu::TestStatus::pass("Querying memory properties succeeded");
}

// \todo [2016-01-22 pyry] Optimize by doing format -> flags mapping instead

VkFormatFeatureFlags getRequiredOptimalTilingFeatures (VkFormat format)
{
	static const VkFormat s_requiredSampledImageBlitSrcFormats[] =
	{
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
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
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
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
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_D32_SFLOAT
	};
	static const VkFormat s_requiredStorageImageFormats[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
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
	static const VkFormat s_requiredStorageImageAtomicFormats[] =
	{
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT
	};
	static const VkFormat s_requiredColorAttachmentBlitDstFormats[] =
	{
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
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
		VK_FORMAT_R32G32B32A32_SFLOAT
	};
	static const VkFormat s_requiredColorAttachmentBlendFormats[] =
	{
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_SFLOAT
	};
	static const VkFormat s_requiredDepthStencilAttachmentFormats[] =
	{
		VK_FORMAT_D16_UNORM
	};

	VkFormatFeatureFlags	flags	= (VkFormatFeatureFlags)0;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageBlitSrcFormats), DE_ARRAY_END(s_requiredSampledImageBlitSrcFormats), format))
		flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageImageFormats), DE_ARRAY_END(s_requiredStorageImageFormats), format))
		flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageImageAtomicFormats), DE_ARRAY_END(s_requiredStorageImageAtomicFormats), format))
		flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredColorAttachmentBlitDstFormats), DE_ARRAY_END(s_requiredColorAttachmentBlitDstFormats), format))
		flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredColorAttachmentBlendFormats), DE_ARRAY_END(s_requiredColorAttachmentBlendFormats), format))
		flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

	if (de::contains(DE_ARRAY_BEGIN(s_requiredDepthStencilAttachmentFormats), DE_ARRAY_END(s_requiredDepthStencilAttachmentFormats), format))
		flags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

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
	TestLog&					log				= context.getTestContext().getLog();
	const VkFormatProperties	properties		= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
	bool						allOk			= true;

	const struct
	{
		VkFormatFeatureFlags VkFormatProperties::*	field;
		const char*									fieldName;
		VkFormatFeatureFlags						requiredFeatures;
	} fields[] =
	{
		{ &VkFormatProperties::linearTilingFeatures,	"linearTilingFeatures",		(VkFormatFeatureFlags)0						},
		{ &VkFormatProperties::optimalTilingFeatures,	"optimalTilingFeatures",	getRequiredOptimalTilingFeatures(format)	},
		{ &VkFormatProperties::bufferFeatures,			"buffeFeatures",			getRequiredBufferFeatures(format)			}
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
	static const VkFormat s_allEtcEacFormats[] =
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
	static const VkFormat s_allAstcFormats[] =
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

	const bool	bcFormatsSupported		= optimalTilingFeaturesSupportedForAll(context,
																			   DE_ARRAY_BEGIN(s_allBcFormats),
																			   DE_ARRAY_END(s_allBcFormats),
																			   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	const bool	etcEacFormatsSupported	= optimalTilingFeaturesSupportedForAll(context,
																			   DE_ARRAY_BEGIN(s_allEtcEacFormats),
																			   DE_ARRAY_END(s_allEtcEacFormats),
																			   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	const bool	astcFormatsSupported	= optimalTilingFeaturesSupportedForAll(context,
																			   DE_ARRAY_BEGIN(s_allAstcFormats),
																			   DE_ARRAY_END(s_allAstcFormats),
																			   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	TestLog&	log						= context.getTestContext().getLog();

	log << TestLog::Message << "All BC* formats supported: " << (bcFormatsSupported ? "true" : "false") << TestLog::EndMessage;
	log << TestLog::Message << "All ETC2/EAC formats supported: " << (etcEacFormatsSupported ? "true" : "false") << TestLog::EndMessage;
	log << TestLog::Message << "All ASTC formats supported: " << (astcFormatsSupported ? "true" : "false") << TestLog::EndMessage;

	if (bcFormatsSupported || etcEacFormatsSupported || astcFormatsSupported)
		return tcu::TestStatus::pass("At least one set of compressed formats supported");
	else
		return tcu::TestStatus::fail("Compressed formats not supported");
}

void createFormatTests (tcu::TestCaseGroup* testGroup)
{
	DE_STATIC_ASSERT(VK_FORMAT_UNDEFINED == 0);

	for (deUint32 formatNdx = VK_FORMAT_UNDEFINED+1; formatNdx < VK_FORMAT_LAST; ++formatNdx)
	{
		const VkFormat		format			= (VkFormat)formatNdx;
		const char* const	enumName		= getFormatName(format);
		const string		caseName		= de::toLower(string(enumName).substr(10));

		addFunctionCase(testGroup, caseName, enumName, formatProperties, format);
	}

	addFunctionCase(testGroup, "depth_stencil",			"",	testDepthStencilSupported);
	addFunctionCase(testGroup, "compressed_formats",	"",	testCompressedFormatsSupported);
}

VkImageUsageFlags getValidImageUsageFlags (VkFormat, VkFormatFeatureFlags supportedFeatures)
{
	VkImageUsageFlags	flags	= (VkImageUsageFlags)0;

	// If format is supported at all, it must be valid transfer src+dst
	if (supportedFeatures != 0)
		flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
	return usage != 0;
}

VkImageCreateFlags getValidImageCreateFlags (const VkPhysicalDeviceFeatures& deviceFeatures, VkFormat, VkFormatFeatureFlags, VkImageType type, VkImageUsageFlags usage)
{
	VkImageCreateFlags	flags	= (VkImageCreateFlags)0;

	if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
	{
		flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

		if (type == VK_IMAGE_TYPE_2D)
			flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
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

struct ImageFormatPropertyCase
{
	VkFormat		format;
	VkImageType		imageType;
	VkImageTiling	tiling;

	ImageFormatPropertyCase (VkFormat format_, VkImageType imageType_, VkImageTiling tiling_)
		: format	(format_)
		, imageType	(imageType_)
		, tiling	(tiling_)
	{}

	ImageFormatPropertyCase (void)
		: format	(VK_FORMAT_LAST)
		, imageType	(VK_IMAGE_TYPE_LAST)
		, tiling	(VK_IMAGE_TILING_LAST)
	{}
};

tcu::TestStatus imageFormatProperties (Context& context, ImageFormatPropertyCase params)
{
	TestLog&						log					= context.getTestContext().getLog();
	const VkFormat					format				= params.format;
	const VkImageType				imageType			= params.imageType;
	const VkImageTiling				tiling				= params.tiling;
	const VkPhysicalDeviceFeatures&	deviceFeatures		= context.getDeviceFeatures();
	const VkFormatProperties		formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);

	const VkFormatFeatureFlags		supportedFeatures	= tiling == VK_IMAGE_TILING_LINEAR ? formatProperties.linearTilingFeatures : formatProperties.optimalTilingFeatures;
	const VkImageUsageFlags			usageFlagSet		= getValidImageUsageFlags(format, supportedFeatures);

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

			log << TestLog::Message << "Testing " << getImageTypeStr(imageType) << ", "
									<< getImageTilingStr(tiling) << ", "
									<< getImageUsageFlagsStr(curUsageFlags) << ", "
									<< getImageCreateFlagsStr(curCreateFlags)
				<< TestLog::EndMessage;

			try
			{
				const VkImageFormatProperties	properties	= getPhysicalDeviceImageFormatProperties(context.getInstanceInterface(),
																										context.getPhysicalDevice(),
																										format,
																										imageType,
																										tiling,
																										curUsageFlags,
																										curCreateFlags);
				log << TestLog::Message << properties << "\n" << TestLog::EndMessage;

				// \todo [2016-01-24 pyry] Expand validation
				TCU_CHECK((properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0);
				TCU_CHECK(imageType != VK_IMAGE_TYPE_1D || (properties.maxExtent.width >= 1 && properties.maxExtent.height == 1 && properties.maxExtent.depth == 1));
				TCU_CHECK(imageType != VK_IMAGE_TYPE_2D || (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 && properties.maxExtent.depth == 1));
				TCU_CHECK(imageType != VK_IMAGE_TYPE_3D || (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 && properties.maxExtent.depth >= 1));
			}
			catch (const Error& error)
			{
				// \todo [2016-01-22 pyry] Check if this is indeed optional image type / flag combination
				if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
					log << TestLog::Message << "Got VK_ERROR_FORMAT_NOT_SUPPORTED" << TestLog::EndMessage;
				else
					throw;
			}
		}
	}

	return tcu::TestStatus::pass("All queries succeeded");
}

void createImageFormatTypeTilingTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase params)
{
	DE_ASSERT(params.format == VK_FORMAT_LAST);

	for (deUint32 formatNdx = VK_FORMAT_UNDEFINED+1; formatNdx < VK_FORMAT_LAST; ++formatNdx)
	{
		const VkFormat		format			= (VkFormat)formatNdx;
		const char* const	enumName		= getFormatName(format);
		const string		caseName		= de::toLower(string(enumName).substr(10));

		params.format = format;

		addFunctionCase(testGroup, caseName, enumName, imageFormatProperties, params);
	}
}

void createImageFormatTypeTests (tcu::TestCaseGroup* testGroup, ImageFormatPropertyCase params)
{
	DE_ASSERT(params.tiling == VK_IMAGE_TILING_LAST);

	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "optimal",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(VK_FORMAT_LAST, params.imageType, VK_IMAGE_TILING_OPTIMAL)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "linear",	"",	createImageFormatTypeTilingTests, ImageFormatPropertyCase(VK_FORMAT_LAST, params.imageType, VK_IMAGE_TILING_LINEAR)));
}

void createImageFormatTests (tcu::TestCaseGroup* testGroup)
{
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "1d", "", createImageFormatTypeTests, ImageFormatPropertyCase(VK_FORMAT_LAST, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "2d", "", createImageFormatTypeTests, ImageFormatPropertyCase(VK_FORMAT_LAST, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LAST)));
	testGroup->addChild(createTestGroup(testGroup->getTestContext(), "3d", "", createImageFormatTypeTests, ImageFormatPropertyCase(VK_FORMAT_LAST, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_LAST)));
}

} // anonymous

tcu::TestCaseGroup* createFeatureInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	{
		de::MovePtr<tcu::TestCaseGroup> instanceInfoTests	(new tcu::TestCaseGroup(testCtx, "instance", "Instance Information Tests"));

		addFunctionCase(instanceInfoTests.get(), "physical_devices",		"Physical devices",			enumeratePhysicalDevices);
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

		infoTests->addChild(deviceInfoTests.release());
	}

	infoTests->addChild(createTestGroup(testCtx, "format_properties",		"VkGetPhysicalDeviceFormatProperties() Tests",		createFormatTests));
	infoTests->addChild(createTestGroup(testCtx, "image_format_properties",	"VkGetPhysicalDeviceImageFormatProperties() Tests",	createImageFormatTests));

	return infoTests.release();
}

} // api
} // vkt
