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

#include "deDefs.h"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
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
#include <optional>
#include <cstring>

namespace vkt
{
namespace api
{
namespace
{

#include "vkApiExtensionDependencyInfo.inl"

using namespace vk;
using std::set;
using std::string;
using std::vector;
using tcu::ScopedLogSection;
using tcu::TestLog;

const uint32_t DEUINT32_MAX = std::numeric_limits<uint32_t>::max();

enum
{
    GUARD_SIZE  = 0x20, //!< Number of bytes to check
    GUARD_VALUE = 0xcd, //!< Data pattern
};

static const VkDeviceSize MINIMUM_REQUIRED_IMAGE_RESOURCE_SIZE =
    (1LLU << 31); //!< Minimum value for VkImageFormatProperties::maxResourceSize (2GiB)

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

#define LIMIT(_X_) offsetof(VkPhysicalDeviceLimits, _X_), (const char *)(#_X_)
#define FEATURE(_X_) offsetof(VkPhysicalDeviceFeatures, _X_)

bool validateFeatureLimits(VkPhysicalDeviceProperties *properties, VkPhysicalDeviceFeatures *features, TestLog &log)
{
    bool limitsOk                  = true;
    VkPhysicalDeviceLimits *limits = &properties->limits;
    uint32_t shaderStages          = 3;
    uint32_t maxPerStageResourcesMin =
        deMin32(128, limits->maxPerStageDescriptorUniformBuffers + limits->maxPerStageDescriptorStorageBuffers +
                         limits->maxPerStageDescriptorSampledImages + limits->maxPerStageDescriptorStorageImages +
                         limits->maxPerStageDescriptorInputAttachments + limits->maxColorAttachments);

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
        uint32_t offset;
        const char *name;
        uint32_t uintVal;       //!< Format is UNSIGNED_INT
        int32_t intVal;         //!< Format is SIGNED_INT
        uint64_t deviceSizeVal; //!< Format is DEVICE_SIZE
        float floatVal;         //!< Format is FLOAT
        LimitFormat format;
        LimitType type;
        int32_t unsuppTableNdx;
        bool pot;
    } featureLimitTable[] = //!< Based on 1.0.28 Vulkan spec
        {
            {LIMIT(maxImageDimension1D), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxImageDimension2D), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxImageDimension3D), 256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxImageDimensionCube), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxImageArrayLayers), 256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTexelBufferElements), 65536, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxUniformBufferRange), 16384, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxStorageBufferRange), 134217728, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxPushConstantsSize), 128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxMemoryAllocationCount), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxSamplerAllocationCount), 4000, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(bufferImageGranularity), 0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(bufferImageGranularity), 0, 0, 131072, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(sparseAddressSpaceSize), 0, 0, 2UL * 1024 * 1024 * 1024, 0.0f, LIMIT_FORMAT_DEVICE_SIZE,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxBoundDescriptorSets), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxPerStageDescriptorSamplers), 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageDescriptorUniformBuffers), 12, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageDescriptorStorageBuffers), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageDescriptorSampledImages), 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageDescriptorStorageImages), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageDescriptorInputAttachments), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxPerStageResources), maxPerStageResourcesMin, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDescriptorSetSamplers), shaderStages * 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN,
             -1, false},
            {LIMIT(maxDescriptorSetUniformBuffers), shaderStages * 12, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDescriptorSetUniformBuffersDynamic), 8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxDescriptorSetStorageBuffers), shaderStages * 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDescriptorSetStorageBuffersDynamic), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxDescriptorSetSampledImages), shaderStages * 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDescriptorSetStorageImages), shaderStages * 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDescriptorSetInputAttachments), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxVertexInputAttributes), 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxVertexInputBindings), 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxVertexInputAttributeOffset), 2047, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxVertexInputBindingStride), 2048, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxVertexOutputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationGenerationLevel), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxTessellationPatchSize), 32, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationControlPerVertexInputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationControlPerVertexOutputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationControlPerPatchOutputComponents), 120, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationControlTotalOutputComponents), 2048, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxTessellationEvaluationInputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN,
             -1, false},
            {LIMIT(maxTessellationEvaluationOutputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxGeometryShaderInvocations), 32, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxGeometryInputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxGeometryOutputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxGeometryOutputVertices), 256, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxGeometryTotalOutputComponents), 1024, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxFragmentInputComponents), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxFragmentOutputAttachments), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxFragmentDualSrcAttachments), 1, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxFragmentCombinedOutputResources), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeSharedMemorySize), 16384, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeWorkGroupCount[0]), 65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeWorkGroupCount[1]), 65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeWorkGroupCount[2]), 65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeWorkGroupInvocations), 128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxComputeWorkGroupSize[0]), 128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxComputeWorkGroupSize[1]), 128, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxComputeWorkGroupSize[2]), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(subPixelPrecisionBits), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(subTexelPrecisionBits), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(mipmapPrecisionBits), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxDrawIndexedIndexValue), (uint32_t)~0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxDrawIndirectCount), 65535, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxSamplerLodBias), 0, 0, 0, 2.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxSamplerAnisotropy), 0, 0, 0, 16.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxViewports), 16, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxViewportDimensions[0]), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxViewportDimensions[1]), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(viewportBoundsRange[0]), 0, 0, 0, -8192.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(viewportBoundsRange[1]), 0, 0, 0, 8191.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(viewportSubPixelBits), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(minMemoryMapAlignment), 64, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, true},
            {LIMIT(minTexelBufferOffsetAlignment), 0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1, true},
            {LIMIT(minTexelBufferOffsetAlignment), 0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1, true},
            {LIMIT(minUniformBufferOffsetAlignment), 0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1, true},
            {LIMIT(minUniformBufferOffsetAlignment), 0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1,
             true},
            {LIMIT(minStorageBufferOffsetAlignment), 0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1, true},
            {LIMIT(minStorageBufferOffsetAlignment), 0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1,
             true},
            {LIMIT(minTexelOffset), 0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(maxTexelOffset), 7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(minTexelGatherOffset), 0, -8, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(maxTexelGatherOffset), 7, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(minInterpolationOffset), 0, 0, 0, -0.5f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(maxInterpolationOffset), 0, 0, 0,
             0.5f - (1.0f / deFloatPow(2.0f, (float)limits->subPixelInterpolationOffsetBits)), LIMIT_FORMAT_FLOAT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(subPixelInterpolationOffsetBits), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(maxFramebufferWidth), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxFramebufferHeight), 4096, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxFramebufferLayers), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(framebufferColorSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(framebufferDepthSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(framebufferStencilSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(framebufferNoAttachmentsSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxColorAttachments), 4, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(sampledImageColorSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(sampledImageIntegerSampleCounts), VK_SAMPLE_COUNT_1_BIT, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(sampledImageDepthSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(sampledImageStencilSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(storageImageSampleCounts), VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT, 0, 0, 0.0f,
             LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxSampleMaskWords), 1, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(timestampComputeAndGraphics), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1, false},
            {LIMIT(timestampPeriod), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1, false},
            {LIMIT(maxClipDistances), 8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxCullDistances), 8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(maxCombinedClipAndCullDistances), 8, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1,
             false},
            {LIMIT(discreteQueuePriorities), 2, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(pointSizeRange[0]), 0, 0, 0, 0.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(pointSizeRange[0]), 0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(pointSizeRange[1]), 0, 0, 0, 64.0f - limits->pointSizeGranularity, LIMIT_FORMAT_FLOAT,
             LIMIT_TYPE_MIN, -1, false},
            {LIMIT(lineWidthRange[0]), 0, 0, 0, 0.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN, -1, false},
            {LIMIT(lineWidthRange[0]), 0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(lineWidthRange[1]), 0, 0, 0, 8.0f - limits->lineWidthGranularity, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN,
             -1, false},
            {LIMIT(pointSizeGranularity), 0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(lineWidthGranularity), 0, 0, 0, 1.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX, -1, false},
            {LIMIT(strictLines), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1, false},
            {LIMIT(standardSampleLocations), 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE, -1, false},
            {LIMIT(optimalBufferCopyOffsetAlignment), 0, 0, 0, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_NONE, -1,
             true},
            {LIMIT(optimalBufferCopyRowPitchAlignment), 0, 0, 0, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_NONE, -1,
             true},
            {LIMIT(nonCoherentAtomSize), 0, 0, 1, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN, -1, true},
            {LIMIT(nonCoherentAtomSize), 0, 0, 256, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX, -1, true},
        };

    const struct UnsupportedFeatureLimitTable
    {
        uint32_t limitOffset;
        const char *name;
        uint32_t featureOffset;
        uint32_t uintVal;       //!< Format is UNSIGNED_INT
        int32_t intVal;         //!< Format is SIGNED_INT
        uint64_t deviceSizeVal; //!< Format is DEVICE_SIZE
        float floatVal;         //!< Format is FLOAT
    } unsupportedFeatureTable[] = {
        {LIMIT(sparseAddressSpaceSize), FEATURE(sparseBinding), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationGenerationLevel), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationPatchSize), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationControlPerVertexInputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationControlPerVertexOutputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationControlPerPatchOutputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationControlTotalOutputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationEvaluationInputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxTessellationEvaluationOutputComponents), FEATURE(tessellationShader), 0, 0, 0, 0.0f},
        {LIMIT(maxGeometryShaderInvocations), FEATURE(geometryShader), 0, 0, 0, 0.0f},
        {LIMIT(maxGeometryInputComponents), FEATURE(geometryShader), 0, 0, 0, 0.0f},
        {LIMIT(maxGeometryOutputComponents), FEATURE(geometryShader), 0, 0, 0, 0.0f},
        {LIMIT(maxGeometryOutputVertices), FEATURE(geometryShader), 0, 0, 0, 0.0f},
        {LIMIT(maxGeometryTotalOutputComponents), FEATURE(geometryShader), 0, 0, 0, 0.0f},
        {LIMIT(maxFragmentDualSrcAttachments), FEATURE(dualSrcBlend), 0, 0, 0, 0.0f},
        {LIMIT(maxDrawIndexedIndexValue), FEATURE(fullDrawIndexUint32), (1 << 24) - 1, 0, 0, 0.0f},
        {LIMIT(maxDrawIndirectCount), FEATURE(multiDrawIndirect), 1, 0, 0, 0.0f},
        {LIMIT(maxSamplerAnisotropy), FEATURE(samplerAnisotropy), 1, 0, 0, 0.0f},
        {LIMIT(maxViewports), FEATURE(multiViewport), 1, 0, 0, 0.0f},
        {LIMIT(minTexelGatherOffset), FEATURE(shaderImageGatherExtended), 0, 0, 0, 0.0f},
        {LIMIT(maxTexelGatherOffset), FEATURE(shaderImageGatherExtended), 0, 0, 0, 0.0f},
        {LIMIT(minInterpolationOffset), FEATURE(sampleRateShading), 0, 0, 0, 0.0f},
        {LIMIT(maxInterpolationOffset), FEATURE(sampleRateShading), 0, 0, 0, 0.0f},
        {LIMIT(subPixelInterpolationOffsetBits), FEATURE(sampleRateShading), 0, 0, 0, 0.0f},
        {LIMIT(storageImageSampleCounts), FEATURE(shaderStorageImageMultisample), VK_SAMPLE_COUNT_1_BIT, 0, 0, 0.0f},
        {LIMIT(maxClipDistances), FEATURE(shaderClipDistance), 0, 0, 0, 0.0f},
        {LIMIT(maxCullDistances), FEATURE(shaderCullDistance), 0, 0, 0, 0.0f},
        {LIMIT(maxCombinedClipAndCullDistances), FEATURE(shaderClipDistance), 0, 0, 0, 0.0f},
        {LIMIT(pointSizeRange[0]), FEATURE(largePoints), 0, 0, 0, 1.0f},
        {LIMIT(pointSizeRange[1]), FEATURE(largePoints), 0, 0, 0, 1.0f},
        {LIMIT(lineWidthRange[0]), FEATURE(wideLines), 0, 0, 0, 1.0f},
        {LIMIT(lineWidthRange[1]), FEATURE(wideLines), 0, 0, 0, 1.0f},
        {LIMIT(pointSizeGranularity), FEATURE(largePoints), 0, 0, 0, 0.0f},
        {LIMIT(lineWidthGranularity), FEATURE(wideLines), 0, 0, 0, 0.0f}};

    log << TestLog::Message << *limits << TestLog::EndMessage;

    //!< First build a map from limit to unsupported table index
    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
    {
        for (uint32_t unsuppNdx = 0; unsuppNdx < DE_LENGTH_OF_ARRAY(unsupportedFeatureTable); unsuppNdx++)
        {
            if (unsupportedFeatureTable[unsuppNdx].limitOffset == featureLimitTable[ndx].offset)
            {
                featureLimitTable[ndx].unsuppTableNdx = unsuppNdx;
                break;
            }
        }
    }

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
    {
        switch (featureLimitTable[ndx].format)
        {
        case LIMIT_FORMAT_UNSIGNED_INT:
        {
            uint32_t limitToCheck = featureLimitTable[ndx].uintVal;
            if (featureLimitTable[ndx].unsuppTableNdx != -1)
            {
                if (*((VkBool32 *)((uint8_t *)features +
                                   unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) ==
                    VK_FALSE)
                    limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].uintVal;
            }

            if (featureLimitTable[ndx].pot)
            {
                if (*((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) == 0 ||
                    !deIntIsPow2(*((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset))))
                {
                    log << TestLog::Message << "limit Validation failed " << featureLimitTable[ndx].name
                        << " is not a power of two." << TestLog::EndMessage;
                    limitsOk = false;
                }
            }

            if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
            {
                if (*((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) < limitToCheck)
                {
                    log << TestLog::Message << "limit Validation failed " << featureLimitTable[ndx].name
                        << " not valid-limit type MIN - actual is "
                        << *((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
            {
                if (*((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) > limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed,  " << featureLimitTable[ndx].name
                        << " not valid-limit type MAX - actual is "
                        << *((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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
                if (*((VkBool32 *)((uint8_t *)features +
                                   unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) ==
                    VK_FALSE)
                    limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].floatVal;
            }

            if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
            {
                if (*((float *)((uint8_t *)limits + featureLimitTable[ndx].offset)) < limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MIN - actual is "
                        << *((float *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
            {
                if (*((float *)((uint8_t *)limits + featureLimitTable[ndx].offset)) > limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MAX actual is "
                        << *((float *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            break;
        }

        case LIMIT_FORMAT_SIGNED_INT:
        {
            int32_t limitToCheck = featureLimitTable[ndx].intVal;
            if (featureLimitTable[ndx].unsuppTableNdx != -1)
            {
                if (*((VkBool32 *)((uint8_t *)features +
                                   unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) ==
                    VK_FALSE)
                    limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].intVal;
            }
            if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
            {
                if (*((int32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) < limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MIN actual is "
                        << *((int32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
            {
                if (*((int32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) > limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MAX actual is "
                        << *((int32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            break;
        }

        case LIMIT_FORMAT_DEVICE_SIZE:
        {
            uint64_t limitToCheck = featureLimitTable[ndx].deviceSizeVal;
            if (featureLimitTable[ndx].unsuppTableNdx != -1)
            {
                if (*((VkBool32 *)((uint8_t *)features +
                                   unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) ==
                    VK_FALSE)
                    limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].deviceSizeVal;
            }

            if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
            {
                if (*((uint64_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) < limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MIN actual is "
                        << *((uint64_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            else if (featureLimitTable[ndx].type == LIMIT_TYPE_MAX)
            {
                if (*((uint64_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) > limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type MAX actual is "
                        << *((uint64_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
                    limitsOk = false;
                }
            }
            break;
        }

        case LIMIT_FORMAT_BITMASK:
        {
            uint32_t limitToCheck = featureLimitTable[ndx].uintVal;
            if (featureLimitTable[ndx].unsuppTableNdx != -1)
            {
                if (*((VkBool32 *)((uint8_t *)features +
                                   unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].featureOffset)) ==
                    VK_FALSE)
                    limitToCheck = unsupportedFeatureTable[featureLimitTable[ndx].unsuppTableNdx].uintVal;
            }

            if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
            {
                if ((*((uint32_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) & limitToCheck) != limitToCheck)
                {
                    log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
                        << " not valid-limit type bitmask actual is "
                        << *((uint64_t *)((uint8_t *)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
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
            << "[" << limits->maxViewportDimensions[0] << ", " << limits->maxViewportDimensions[1] << "]"
            << TestLog::EndMessage;
        limitsOk = false;
    }

    if (limits->viewportBoundsRange[0] > float(-2 * limits->maxViewportDimensions[0]))
    {
        log << TestLog::Message << "limit validation failed, viewPortBoundsRange[0] of "
            << limits->viewportBoundsRange[0] << "is larger than -2*maxViewportDimension[0] of "
            << -2 * limits->maxViewportDimensions[0] << TestLog::EndMessage;
        limitsOk = false;
    }

    if (limits->viewportBoundsRange[1] < float(2 * limits->maxViewportDimensions[1] - 1))
    {
        log << TestLog::Message << "limit validation failed, viewportBoundsRange[1] of "
            << limits->viewportBoundsRange[1] << "is less than 2*maxViewportDimension[1] of "
            << 2 * limits->maxViewportDimensions[1] << TestLog::EndMessage;
        limitsOk = false;
    }

    return limitsOk;
}

template <uint32_t MAJOR, uint32_t MINOR>
void checkApiVersionSupport(Context &context)
{
    if (!context.contextSupports(vk::ApiVersion(0, MAJOR, MINOR, 0)))
        TCU_THROW(NotSupportedError, std::string("At least Vulkan ") + std::to_string(MAJOR) + "." +
                                         std::to_string(MINOR) + " required to run test");
}

typedef struct FeatureLimitTableItem_
{
    const VkBool32 *cond;
    const char *condName;
    const void *ptr;
    const char *name;
    uint32_t uintVal;       //!< Format is UNSIGNED_INT
    int32_t intVal;         //!< Format is SIGNED_INT
    uint64_t deviceSizeVal; //!< Format is DEVICE_SIZE
    float floatVal;         //!< Format is FLOAT
    LimitFormat format;
    LimitType type;
} FeatureLimitTableItem;

template <typename T>
bool validateNumericLimit(const T limitToCheck, const T reportedValue, const LimitType limitType, const char *limitName,
                          TestLog &log)
{
    if (limitType == LIMIT_TYPE_MIN)
    {
        if (reportedValue < limitToCheck)
        {
            log << TestLog::Message << "Limit validation failed " << limitName << " reported value is " << reportedValue
                << " expected MIN " << limitToCheck << TestLog::EndMessage;

            return false;
        }

        log << TestLog::Message << limitName << "=" << reportedValue << " (>=" << limitToCheck << ")"
            << TestLog::EndMessage;
    }
    else if (limitType == LIMIT_TYPE_MAX)
    {
        if (reportedValue > limitToCheck)
        {
            log << TestLog::Message << "Limit validation failed " << limitName << " reported value is " << reportedValue
                << " expected MAX " << limitToCheck << TestLog::EndMessage;

            return false;
        }

        log << TestLog::Message << limitName << "=" << reportedValue << " (<=" << limitToCheck << ")"
            << TestLog::EndMessage;
    }

    return true;
}

template <typename T>
bool validateBitmaskLimit(const T limitToCheck, const T reportedValue, const LimitType limitType, const char *limitName,
                          TestLog &log)
{
    if (limitType == LIMIT_TYPE_MIN)
    {
        if ((reportedValue & limitToCheck) != limitToCheck)
        {
            log << TestLog::Message << "Limit validation failed " << limitName << " reported value is " << reportedValue
                << " expected MIN " << limitToCheck << TestLog::EndMessage;

            return false;
        }

        log << TestLog::Message << limitName << "=" << tcu::toHex(reportedValue) << " (contains "
            << tcu::toHex(limitToCheck) << ")" << TestLog::EndMessage;
    }

    return true;
}

bool validateLimit(FeatureLimitTableItem limit, TestLog &log)
{
    if (*limit.cond == VK_FALSE)
    {
        log << TestLog::Message << "Limit validation skipped '" << limit.name << "' due to " << limit.condName
            << " == VK_FALSE'" << TestLog::EndMessage;

        return true;
    }

    switch (limit.format)
    {
    case LIMIT_FORMAT_UNSIGNED_INT:
    {
        const uint32_t limitToCheck  = limit.uintVal;
        const uint32_t reportedValue = *(uint32_t *)limit.ptr;

        return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
    }

    case LIMIT_FORMAT_FLOAT:
    {
        const float limitToCheck  = limit.floatVal;
        const float reportedValue = *(float *)limit.ptr;

        return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
    }

    case LIMIT_FORMAT_SIGNED_INT:
    {
        const int32_t limitToCheck  = limit.intVal;
        const int32_t reportedValue = *(int32_t *)limit.ptr;

        return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
    }

    case LIMIT_FORMAT_DEVICE_SIZE:
    {
        const uint64_t limitToCheck  = limit.deviceSizeVal;
        const uint64_t reportedValue = *(uint64_t *)limit.ptr;

        return validateNumericLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
    }

    case LIMIT_FORMAT_BITMASK:
    {
        const uint32_t limitToCheck  = limit.uintVal;
        const uint32_t reportedValue = *(uint32_t *)limit.ptr;

        return validateBitmaskLimit(limitToCheck, reportedValue, limit.type, limit.name, log);
    }

    default:
        TCU_THROW(InternalError, "Unknown LimitFormat specified");
    }
}

#ifdef PN
#error PN defined
#else
#define PN(_X_) &(_X_), (const char *)(#_X_)
#endif

#define LIM_MIN_UINT32(X) uint32_t(X), 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN
#define LIM_MAX_UINT32(X) uint32_t(X), 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX
#define LIM_NONE_UINT32 0, 0, 0, 0.0f, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_NONE
#define LIM_MIN_INT32(X) 0, int32_t(X), 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MIN
#define LIM_MAX_INT32(X) 0, int32_t(X), 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX
#define LIM_NONE_INT32 0, 0, 0, 0.0f, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_NONE
#define LIM_MIN_DEVSIZE(X) 0, 0, VkDeviceSize(X), 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN
#define LIM_MAX_DEVSIZE(X) 0, 0, VkDeviceSize(X), 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX
#define LIM_NONE_DEVSIZE 0, 0, 0, 0.0f, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_NONE
#define LIM_MIN_FLOAT(X) 0, 0, 0, float(X), LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN
#define LIM_MAX_FLOAT(X) 0, 0, 0, float(X), LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX
#define LIM_NONE_FLOAT 0, 0, 0, 0.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_NONE
#define LIM_MIN_BITI32(X) uint32_t(X), 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MIN
#define LIM_MAX_BITI32(X) uint32_t(X), 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_MAX
#define LIM_NONE_BITI32 0, 0, 0, 0.0f, LIMIT_FORMAT_BITMASK, LIMIT_TYPE_NONE

static constexpr int E071 = 71; //  VK_KHR_device_group_creation
static constexpr int E060 = 60; //  VK_KHR_get_physical_device_properties2

template <int InstanceExtensionNumber>
struct CustomInstanceDeterminant;
template <>
struct CustomInstanceDeterminant<E071>
{
    static const char *getExtension()
    {
        return "VK_KHR_device_group_creation";
    }
    static const char *getDeterminant()
    {
        return "custom-instance-that-needs-VK_KHR_device_group_creation";
    }
};
template <>
struct CustomInstanceDeterminant<E060>
{
    static const char *getExtension()
    {
        return "VK_KHR_get_physical_device_properties2";
    }
    static const char *getDeterminant()
    {
        return "custom-instance-that-needs-VK_KHR_get_physical_device_properties2";
    }
};

typedef InstanceFactory1<FunctionInstance0, FunctionInstance0::Function> CustomInstanceTestBase;
template <int InstanceExtensionNumber>
struct CustomInstanceTest : public CustomInstanceTestBase
{
    using CustomInstanceTestBase::CustomInstanceTestBase;
    virtual std::string getInstanceCapabilitiesId() const override
    {
        return CustomInstanceDeterminant<InstanceExtensionNumber>::getDeterminant();
    }
    virtual void initInstanceCapabilities(InstCaps &caps) override
    {
        const std::string extension(CustomInstanceDeterminant<InstanceExtensionNumber>::getExtension());
        if (!caps.addExtension(extension))
            TCU_THROW(NotSupportedError, extension + " not supported by device");
    }
};

typedef InstanceFactory1WithSupport<FunctionInstance0, FunctionInstance0::Function, FunctionSupport0>
    CustomInstanceWithSupportTestBase;
template <int InstanceExtensionNumber>
struct CustomInstanceWithSupportTest : public CustomInstanceWithSupportTestBase
{
    using CustomInstanceWithSupportTestBase::CustomInstanceWithSupportTestBase;
    virtual std::string getInstanceCapabilitiesId() const override
    {
        return CustomInstanceDeterminant<InstanceExtensionNumber>::getDeterminant();
    }
    virtual void initInstanceCapabilities(InstCaps &caps) override
    {
        const std::string extension(CustomInstanceDeterminant<InstanceExtensionNumber>::getExtension());
        if (!caps.addExtension(extension))
            TCU_THROW(NotSupportedError, extension + " not supported by device");
    }
};

template <typename Arg0>
using CustomInstanceArg0TestBase = InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args>;
template <typename Arg0, int InstanceExtensionNumber>
struct CustomInstanceArg0Test : public CustomInstanceArg0TestBase<Arg0>
{
    using CustomInstanceArg0TestBase<Arg0>::CustomInstanceArg0TestBase;
    virtual std::string getInstanceCapabilitiesId() const override
    {
        return CustomInstanceDeterminant<InstanceExtensionNumber>::getDeterminant();
    }
    virtual void initInstanceCapabilities(InstCaps &caps) override
    {
        const std::string extension(CustomInstanceDeterminant<InstanceExtensionNumber>::getExtension());
        if (!caps.addExtension(extension))
            TCU_THROW(NotSupportedError, extension + " not supported by device");
    }
};

tcu::TestStatus validateLimits12(Context &context)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();
    TestLog &log                          = context.getTestContext().getLog();
    bool limitsOk                         = true;

    const VkPhysicalDeviceFeatures2 &features2 = context.getDeviceFeatures2();
    const VkPhysicalDeviceFeatures &features   = features2.features;
#ifdef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkan11Features features11 = getPhysicalDeviceVulkan11Features(vki, physicalDevice);
#endif // CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkan12Features features12 = getPhysicalDeviceVulkan12Features(vki, physicalDevice);

    const VkPhysicalDeviceProperties2 &properties2 = context.getDeviceProperties2();
    const VkPhysicalDeviceVulkan12Properties vulkan12Properties =
        getPhysicalDeviceVulkan12Properties(vki, physicalDevice);
    const VkPhysicalDeviceVulkan11Properties vulkan11Properties =
        getPhysicalDeviceVulkan11Properties(vki, physicalDevice);
#ifdef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkanSC10Properties vulkanSC10Properties =
        getPhysicalDeviceVulkanSC10Properties(vki, physicalDevice);
#endif // CTS_USES_VULKANSC
    const VkPhysicalDeviceLimits &limits = properties2.properties.limits;

    const VkBool32 checkAlways        = VK_TRUE;
    const VkBool32 checkVulkan12Limit = VK_TRUE;
#ifdef CTS_USES_VULKANSC
    const VkBool32 checkVulkanSC10Limit = VK_TRUE;
#endif // CTS_USES_VULKANSC

    uint32_t shaderStages = 3;
    uint32_t maxPerStageResourcesMin =
        deMin32(128, limits.maxPerStageDescriptorUniformBuffers + limits.maxPerStageDescriptorStorageBuffers +
                         limits.maxPerStageDescriptorSampledImages + limits.maxPerStageDescriptorStorageImages +
                         limits.maxPerStageDescriptorInputAttachments + limits.maxColorAttachments);
    uint32_t maxFramebufferLayers = 256;

    if (features.tessellationShader)
    {
        shaderStages += 2;
    }

    if (features.geometryShader)
    {
        shaderStages++;
    }

    // Vulkan SC
#ifdef CTS_USES_VULKANSC
    if (features.geometryShader == VK_FALSE && features12.shaderOutputLayer == VK_FALSE)
    {
        maxFramebufferLayers = 1;
    }
#endif // CTS_USES_VULKANSC

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(limits.maxImageDimension1D), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxImageDimension2D), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxImageDimension3D), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.maxImageDimensionCube), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxImageArrayLayers), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.maxTexelBufferElements), LIM_MIN_UINT32(65536)},
        {PN(checkAlways), PN(limits.maxUniformBufferRange), LIM_MIN_UINT32(16384)},
        {PN(checkAlways), PN(limits.maxStorageBufferRange), LIM_MIN_UINT32((1 << 27))},
        {PN(checkAlways), PN(limits.maxPushConstantsSize), LIM_MIN_UINT32(128)},
        {PN(checkAlways), PN(limits.maxMemoryAllocationCount), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxSamplerAllocationCount), LIM_MIN_UINT32(4000)},
        {PN(checkAlways), PN(limits.bufferImageGranularity), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.bufferImageGranularity), LIM_MAX_DEVSIZE(131072)},
        {PN(features.sparseBinding), PN(limits.sparseAddressSpaceSize), LIM_MIN_DEVSIZE((1ull << 31))},
        {PN(checkAlways), PN(limits.maxBoundDescriptorSets), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorSamplers), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorUniformBuffers), LIM_MIN_UINT32(12)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorStorageBuffers), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorSampledImages), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorStorageImages), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorInputAttachments), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxPerStageResources), LIM_MIN_UINT32(maxPerStageResourcesMin)},
        {PN(checkAlways), PN(limits.maxDescriptorSetSamplers), LIM_MIN_UINT32(shaderStages * 16)},
        {PN(checkAlways), PN(limits.maxDescriptorSetUniformBuffers), LIM_MIN_UINT32(shaderStages * 12)},
        {PN(checkAlways), PN(limits.maxDescriptorSetUniformBuffersDynamic), LIM_MIN_UINT32(8)},
        {PN(checkAlways), PN(limits.maxDescriptorSetStorageBuffers), LIM_MIN_UINT32(shaderStages * 4)},
        {PN(checkAlways), PN(limits.maxDescriptorSetStorageBuffersDynamic), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxDescriptorSetSampledImages), LIM_MIN_UINT32(shaderStages * 16)},
        {PN(checkAlways), PN(limits.maxDescriptorSetStorageImages), LIM_MIN_UINT32(shaderStages * 4)},
        {PN(checkAlways), PN(limits.maxDescriptorSetInputAttachments), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxVertexInputAttributes), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxVertexInputBindings), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxVertexInputAttributeOffset), LIM_MIN_UINT32(2047)},
        {PN(checkAlways), PN(limits.maxVertexInputBindingStride), LIM_MIN_UINT32(2048)},
        {PN(checkAlways), PN(limits.maxVertexOutputComponents), LIM_MIN_UINT32(64)},
        {PN(features.tessellationShader), PN(limits.maxTessellationGenerationLevel), LIM_MIN_UINT32(64)},
        {PN(features.tessellationShader), PN(limits.maxTessellationPatchSize), LIM_MIN_UINT32(32)},
        {PN(features.tessellationShader), PN(limits.maxTessellationControlPerVertexInputComponents),
         LIM_MIN_UINT32(64)},
        {PN(features.tessellationShader), PN(limits.maxTessellationControlPerVertexOutputComponents),
         LIM_MIN_UINT32(64)},
        {PN(features.tessellationShader), PN(limits.maxTessellationControlPerPatchOutputComponents),
         LIM_MIN_UINT32(120)},
        {PN(features.tessellationShader), PN(limits.maxTessellationControlTotalOutputComponents), LIM_MIN_UINT32(2048)},
        {PN(features.tessellationShader), PN(limits.maxTessellationEvaluationInputComponents), LIM_MIN_UINT32(64)},
        {PN(features.tessellationShader), PN(limits.maxTessellationEvaluationOutputComponents), LIM_MIN_UINT32(64)},
        {PN(features.geometryShader), PN(limits.maxGeometryShaderInvocations), LIM_MIN_UINT32(32)},
        {PN(features.geometryShader), PN(limits.maxGeometryInputComponents), LIM_MIN_UINT32(64)},
        {PN(features.geometryShader), PN(limits.maxGeometryOutputComponents), LIM_MIN_UINT32(64)},
        {PN(features.geometryShader), PN(limits.maxGeometryOutputVertices), LIM_MIN_UINT32(256)},
        {PN(features.geometryShader), PN(limits.maxGeometryTotalOutputComponents), LIM_MIN_UINT32(1024)},
        {PN(checkAlways), PN(limits.maxFragmentInputComponents), LIM_MIN_UINT32(64)},
        {PN(checkAlways), PN(limits.maxFragmentOutputAttachments), LIM_MIN_UINT32(4)},
        {PN(features.dualSrcBlend), PN(limits.maxFragmentDualSrcAttachments), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(limits.maxFragmentCombinedOutputResources), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxComputeSharedMemorySize), LIM_MIN_UINT32(16384)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupCount[0]), LIM_MIN_UINT32(65535)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupCount[1]), LIM_MIN_UINT32(65535)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupCount[2]), LIM_MIN_UINT32(65535)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupInvocations), LIM_MIN_UINT32(128)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[0]), LIM_MIN_UINT32(128)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[1]), LIM_MIN_UINT32(128)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[2]), LIM_MIN_UINT32(64)},
        {PN(checkAlways), PN(limits.subPixelPrecisionBits), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.subTexelPrecisionBits), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.mipmapPrecisionBits), LIM_MIN_UINT32(4)},
        {PN(features.fullDrawIndexUint32), PN(limits.maxDrawIndexedIndexValue), LIM_MIN_UINT32((uint32_t)~0)},
        {PN(features.multiDrawIndirect), PN(limits.maxDrawIndirectCount), LIM_MIN_UINT32(65535)},
        {PN(checkAlways), PN(limits.maxSamplerLodBias), LIM_MIN_FLOAT(2.0f)},
        {PN(features.samplerAnisotropy), PN(limits.maxSamplerAnisotropy), LIM_MIN_FLOAT(16.0f)},
        {PN(features.multiViewport), PN(limits.maxViewports), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxViewportDimensions[0]), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxViewportDimensions[1]), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.viewportBoundsRange[0]), LIM_MAX_FLOAT(-8192.0f)},
        {PN(checkAlways), PN(limits.viewportBoundsRange[1]), LIM_MIN_FLOAT(8191.0f)},
        {PN(checkAlways), PN(limits.viewportSubPixelBits), LIM_MIN_UINT32(0)},
        {PN(checkAlways), PN(limits.minMemoryMapAlignment), LIM_MIN_UINT32(64)},
        {PN(checkAlways), PN(limits.minTexelBufferOffsetAlignment), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.minTexelBufferOffsetAlignment), LIM_MAX_DEVSIZE(256)},
        {PN(checkAlways), PN(limits.minUniformBufferOffsetAlignment), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.minUniformBufferOffsetAlignment), LIM_MAX_DEVSIZE(256)},
        {PN(checkAlways), PN(limits.minStorageBufferOffsetAlignment), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.minStorageBufferOffsetAlignment), LIM_MAX_DEVSIZE(256)},
        {PN(checkAlways), PN(limits.minTexelOffset), LIM_MAX_INT32(-8)},
        {PN(checkAlways), PN(limits.maxTexelOffset), LIM_MIN_INT32(7)},
        {PN(features.shaderImageGatherExtended), PN(limits.minTexelGatherOffset), LIM_MAX_INT32(-8)},
        {PN(features.shaderImageGatherExtended), PN(limits.maxTexelGatherOffset), LIM_MIN_INT32(7)},
        {PN(features.sampleRateShading), PN(limits.minInterpolationOffset), LIM_MAX_FLOAT(-0.5f)},
        {PN(features.sampleRateShading), PN(limits.maxInterpolationOffset),
         LIM_MIN_FLOAT(0.5f - (1.0f / deFloatPow(2.0f, (float)limits.subPixelInterpolationOffsetBits)))},
        {PN(features.sampleRateShading), PN(limits.subPixelInterpolationOffsetBits), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.maxFramebufferWidth), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxFramebufferHeight), LIM_MIN_UINT32(4096)},
        {PN(checkAlways), PN(limits.maxFramebufferLayers), LIM_MIN_UINT32(maxFramebufferLayers)},
        {PN(checkAlways), PN(limits.framebufferColorSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkVulkan12Limit), PN(vulkan12Properties.framebufferIntegerColorSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT)},
        {PN(checkAlways), PN(limits.framebufferDepthSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.framebufferStencilSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.framebufferNoAttachmentsSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.maxColorAttachments), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(limits.sampledImageColorSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.sampledImageIntegerSampleCounts), LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT)},
        {PN(checkAlways), PN(limits.sampledImageDepthSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.sampledImageStencilSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(features.shaderStorageImageMultisample), PN(limits.storageImageSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(limits.maxSampleMaskWords), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(limits.timestampComputeAndGraphics), LIM_NONE_UINT32},
        {PN(checkAlways), PN(limits.timestampPeriod), LIM_NONE_UINT32},
        {PN(features.shaderClipDistance), PN(limits.maxClipDistances), LIM_MIN_UINT32(8)},
        {PN(features.shaderCullDistance), PN(limits.maxCullDistances), LIM_MIN_UINT32(8)},
        {PN(features.shaderClipDistance), PN(limits.maxCombinedClipAndCullDistances), LIM_MIN_UINT32(8)},
        {PN(checkAlways), PN(limits.discreteQueuePriorities), LIM_MIN_UINT32(2)},
        {PN(features.largePoints), PN(limits.pointSizeRange[0]), LIM_MIN_FLOAT(0.0f)},
        {PN(features.largePoints), PN(limits.pointSizeRange[0]), LIM_MAX_FLOAT(1.0f)},
        {PN(features.largePoints), PN(limits.pointSizeRange[1]), LIM_MIN_FLOAT(64.0f - limits.pointSizeGranularity)},
        {PN(features.wideLines), PN(limits.lineWidthRange[0]), LIM_MIN_FLOAT(0.0f)},
        {PN(features.wideLines), PN(limits.lineWidthRange[0]), LIM_MAX_FLOAT(1.0f)},
        {PN(features.wideLines), PN(limits.lineWidthRange[1]), LIM_MIN_FLOAT(8.0f - limits.lineWidthGranularity)},
        {PN(features.largePoints), PN(limits.pointSizeGranularity), LIM_MIN_FLOAT(0.0f)},
        {PN(features.largePoints), PN(limits.pointSizeGranularity), LIM_MAX_FLOAT(1.0f)},
        {PN(features.wideLines), PN(limits.lineWidthGranularity), LIM_MIN_FLOAT(0.0f)},
        {PN(features.wideLines), PN(limits.lineWidthGranularity), LIM_MAX_FLOAT(1.0f)},
        {PN(checkAlways), PN(limits.strictLines), LIM_NONE_UINT32},
        {PN(checkAlways), PN(limits.standardSampleLocations), LIM_NONE_UINT32},
        {PN(checkAlways), PN(limits.optimalBufferCopyOffsetAlignment), LIM_NONE_DEVSIZE},
        {PN(checkAlways), PN(limits.optimalBufferCopyRowPitchAlignment), LIM_NONE_DEVSIZE},
        {PN(checkAlways), PN(limits.nonCoherentAtomSize), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.nonCoherentAtomSize), LIM_MAX_DEVSIZE(256)},

    // VK_KHR_multiview
#ifndef CTS_USES_VULKANSC
        {PN(checkVulkan12Limit), PN(vulkan11Properties.maxMultiviewViewCount), LIM_MIN_UINT32(6)},
        {PN(checkVulkan12Limit), PN(vulkan11Properties.maxMultiviewInstanceIndex), LIM_MIN_UINT32((1 << 27) - 1)},
#else
        {PN(features11.multiview), PN(vulkan11Properties.maxMultiviewViewCount), LIM_MIN_UINT32(6)},
        {PN(features11.multiview), PN(vulkan11Properties.maxMultiviewInstanceIndex), LIM_MIN_UINT32((1 << 27) - 1)},
#endif // CTS_USES_VULKANSC

        // VK_KHR_maintenance3
        {PN(checkVulkan12Limit), PN(vulkan11Properties.maxPerSetDescriptors), LIM_MIN_UINT32(1024)},
        {PN(checkVulkan12Limit), PN(vulkan11Properties.maxMemoryAllocationSize), LIM_MIN_DEVSIZE(1 << 30)},

        // VK_EXT_descriptor_indexing
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxUpdateAfterBindDescriptorsInAllPools),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(12)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(4)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageUpdateAfterBindResources),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(shaderStages * 12)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),
         LIM_MIN_UINT32(8)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),
         LIM_MIN_UINT32(4)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(500000)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(4)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorSamplers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorUniformBuffers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageBuffers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorSampledImages)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageImages)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorInputAttachments)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxPerStageUpdateAfterBindResources),
         LIM_MIN_UINT32(limits.maxPerStageResources)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers),
         LIM_MIN_UINT32(limits.maxDescriptorSetSamplers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),
         LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffersDynamic)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffers)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffersDynamic)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(limits.maxDescriptorSetSampledImages)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageImages)},
        {PN(features12.descriptorIndexing), PN(vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(limits.maxDescriptorSetInputAttachments)},

    // timelineSemaphore
#ifndef CTS_USES_VULKANSC
        {PN(checkVulkan12Limit), PN(vulkan12Properties.maxTimelineSemaphoreValueDifference),
         LIM_MIN_DEVSIZE((1ull << 31) - 1)},
#else
        // VkPhysicalDeviceVulkan12Features::timelineSemaphore is optional in Vulkan SC
        {PN(features12.timelineSemaphore), PN(vulkan12Properties.maxTimelineSemaphoreValueDifference),
         LIM_MIN_DEVSIZE((1ull << 31) - 1)},
#endif // CTS_USES_VULKANSC

    // Vulkan SC
#ifdef CTS_USES_VULKANSC
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxRenderPassSubpasses), LIM_MIN_UINT32(1)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxRenderPassDependencies), LIM_MIN_UINT32(18)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxSubpassInputAttachments), LIM_MIN_UINT32(0)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxSubpassPreserveAttachments), LIM_MIN_UINT32(0)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxFramebufferAttachments), LIM_MIN_UINT32(9)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxDescriptorSetLayoutBindings), LIM_MIN_UINT32(64)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxQueryFaultCount), LIM_MIN_UINT32(16)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxCallbackFaultCount), LIM_MIN_UINT32(1)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxCommandPoolCommandBuffers), LIM_MIN_UINT32(256)},
        {PN(checkVulkanSC10Limit), PN(vulkanSC10Properties.maxCommandBufferSize), LIM_MIN_UINT32(1048576)},
#endif // CTS_USES_VULKANSC
    };

    log << TestLog::Message << limits << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limits.maxFramebufferWidth > limits.maxViewportDimensions[0] ||
        limits.maxFramebufferHeight > limits.maxViewportDimensions[1])
    {
        log << TestLog::Message << "limit validation failed, maxFramebufferDimension of "
            << "[" << limits.maxFramebufferWidth << ", " << limits.maxFramebufferHeight << "] "
            << "is larger than maxViewportDimension of "
            << "[" << limits.maxViewportDimensions[0] << ", " << limits.maxViewportDimensions[1] << "]"
            << TestLog::EndMessage;
        limitsOk = false;
    }

    if (limits.viewportBoundsRange[0] > float(-2 * limits.maxViewportDimensions[0]))
    {
        log << TestLog::Message << "limit validation failed, viewPortBoundsRange[0] of "
            << limits.viewportBoundsRange[0] << "is larger than -2*maxViewportDimension[0] of "
            << -2 * limits.maxViewportDimensions[0] << TestLog::EndMessage;
        limitsOk = false;
    }

    if (limits.viewportBoundsRange[1] < float(2 * limits.maxViewportDimensions[1] - 1))
    {
        log << TestLog::Message << "limit validation failed, viewportBoundsRange[1] of "
            << limits.viewportBoundsRange[1] << "is less than 2*maxViewportDimension[1] of "
            << 2 * limits.maxViewportDimensions[1] << TestLog::EndMessage;
        limitsOk = false;
    }

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#ifndef CTS_USES_VULKANSC

tcu::TestStatus validateLimits14(Context &context)
{
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    const auto &features2 = context.getDeviceFeatures2();
    const auto &features  = features2.features;

    const auto vk11Properties = context.getDeviceVulkan11Properties();
    const auto vk12Properties = context.getDeviceVulkan12Properties();
    const auto vk12Features   = context.getDeviceVulkan12Features();
    const auto &properties2   = context.getDeviceProperties2();
    const auto &limits        = properties2.properties.limits;

    const VkBool32 checkAlways                = VK_TRUE;
    const VkBool32 checkShaderSZINPreserve    = vk11Properties.subgroupSize > 1;
    const VkBool32 checkShaderSZINPreserveF16 = (checkShaderSZINPreserve && vk12Features.shaderFloat16);

    VkShaderStageFlags subgroupSupportedStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkSubgroupFeatureFlags subgroupFeatureFlags =
        VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
        VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT;
    if (vk11Properties.subgroupSize > 3)
        subgroupFeatureFlags |= VK_SUBGROUP_FEATURE_QUAD_BIT;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(limits.maxImageDimension1D), LIM_MIN_UINT32(8192)},
        {PN(checkAlways), PN(limits.maxImageDimension2D), LIM_MIN_UINT32(8192)},
        {PN(checkAlways), PN(limits.maxImageDimension3D), LIM_MIN_UINT32(512)},
        {PN(checkAlways), PN(limits.maxImageDimensionCube), LIM_MIN_UINT32(8192)},
        {PN(checkAlways), PN(limits.maxImageArrayLayers), LIM_MIN_UINT32(2048)},
        {PN(checkAlways), PN(limits.maxUniformBufferRange), LIM_MIN_UINT32(65536)},
        {PN(checkAlways), PN(limits.maxPushConstantsSize), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.bufferImageGranularity), LIM_MIN_DEVSIZE(1)},
        {PN(checkAlways), PN(limits.bufferImageGranularity), LIM_MAX_DEVSIZE(4096)},
        {PN(checkAlways), PN(limits.maxBoundDescriptorSets), LIM_MIN_UINT32(7)},
        {PN(checkAlways), PN(limits.maxPerStageDescriptorUniformBuffers), LIM_MIN_UINT32(15)},
        {PN(checkAlways), PN(limits.maxPerStageResources), LIM_MIN_UINT32(200)},
        {PN(checkAlways), PN(limits.maxDescriptorSetUniformBuffers), LIM_MIN_UINT32(90)},
        {PN(checkAlways), PN(limits.maxDescriptorSetStorageBuffers), LIM_MIN_UINT32(96)},
        {PN(checkAlways), PN(limits.maxDescriptorSetStorageImages), LIM_MIN_UINT32(144)},
        {PN(checkAlways), PN(limits.maxFragmentCombinedOutputResources), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupInvocations), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[0]), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[1]), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(limits.maxComputeWorkGroupSize[2]), LIM_MIN_UINT32(64)},
        {PN(checkAlways), PN(limits.subTexelPrecisionBits), LIM_MIN_UINT32(8)},
        {PN(checkAlways), PN(limits.mipmapPrecisionBits), LIM_MIN_UINT32(6)},
        {PN(checkAlways), PN(limits.maxSamplerLodBias), LIM_MIN_FLOAT(14.0f)},
        {PN(checkAlways), PN(limits.maxViewportDimensions[0]), LIM_MIN_UINT32(7680)},
        {PN(checkAlways), PN(limits.maxViewportDimensions[1]), LIM_MIN_UINT32(7680)},
        {PN(checkAlways), PN(limits.viewportBoundsRange[0]), LIM_MAX_FLOAT(-15360.0f)},
        {PN(checkAlways), PN(limits.viewportBoundsRange[1]), LIM_MIN_FLOAT(15359.0f)},
        {PN(checkAlways), PN(limits.maxFramebufferWidth), LIM_MIN_UINT32(7680)},
        {PN(checkAlways), PN(limits.maxFramebufferHeight), LIM_MIN_UINT32(7680)},
        {PN(checkAlways), PN(limits.maxColorAttachments), LIM_MIN_UINT32(8)},
        {PN(checkAlways), PN(limits.timestampComputeAndGraphics), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(limits.standardSampleLocations), LIM_MIN_UINT32(1)},
        {PN(features.largePoints), PN(limits.pointSizeRange[0]), LIM_MIN_FLOAT(0.0f)},
        {PN(features.largePoints), PN(limits.pointSizeRange[0]), LIM_MAX_FLOAT(1.0f)},
        {PN(features.largePoints), PN(limits.pointSizeRange[1]), LIM_MIN_FLOAT(256.0f - limits.pointSizeGranularity)},
        {PN(features.largePoints), PN(limits.pointSizeGranularity), LIM_MIN_FLOAT(0.0f)},
        {PN(features.largePoints), PN(limits.pointSizeGranularity), LIM_MAX_FLOAT(0.125f)},
        {PN(features.wideLines), PN(limits.lineWidthGranularity), LIM_MIN_FLOAT(0.0f)},
        {PN(features.wideLines), PN(limits.lineWidthGranularity), LIM_MAX_FLOAT(0.5f)},
        {PN(checkAlways), PN(vk11Properties.subgroupSupportedStages), LIM_MIN_UINT32(subgroupSupportedStages)},
        {PN(checkAlways), PN(vk11Properties.subgroupSupportedOperations), LIM_MIN_UINT32(subgroupFeatureFlags)},
        {PN(checkShaderSZINPreserveF16), PN(vk12Properties.shaderSignedZeroInfNanPreserveFloat16), LIM_MIN_UINT32(1)},
        {PN(checkShaderSZINPreserve), PN(vk12Properties.shaderSignedZeroInfNanPreserveFloat32), LIM_MIN_UINT32(1)},
    };

    log << TestLog::Message << limits << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
    {
        if (!validateLimit(featureLimitTable[ndx], log))
            limitsOk = false;
    }

    if (limitsOk)
        return tcu::TestStatus::pass("pass");

    return tcu::TestStatus::fail("fail");
}

void checkSupportKhrPushDescriptor(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_push_descriptor");
}

tcu::TestStatus validateLimitsKhrPushDescriptor(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDevicePushDescriptorPropertiesKHR &pushDescriptorPropertiesKHR =
        context.getPushDescriptorProperties();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(pushDescriptorPropertiesKHR.maxPushDescriptors), LIM_MIN_UINT32(32)},
    };

    log << TestLog::Message << pushDescriptorPropertiesKHR << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#endif // CTS_USES_VULKANSC

void checkSupportKhrMultiview(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_multiview");
}

tcu::TestStatus validateLimitsKhrMultiview(Context &context)
{
    const VkBool32 checkAlways                                     = VK_TRUE;
    const VkPhysicalDeviceMultiviewProperties &multiviewProperties = context.getMultiviewProperties();
    TestLog &log                                                   = context.getTestContext().getLog();
    bool limitsOk                                                  = true;

    FeatureLimitTableItem featureLimitTable[] = {
        // VK_KHR_multiview
        {PN(checkAlways), PN(multiviewProperties.maxMultiviewViewCount), LIM_MIN_UINT32(6)},
        {PN(checkAlways), PN(multiviewProperties.maxMultiviewInstanceIndex), LIM_MIN_UINT32((1 << 27) - 1)},
    };

    log << TestLog::Message << multiviewProperties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtDiscardRectangles(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_discard_rectangles");
}

tcu::TestStatus validateLimitsExtDiscardRectangles(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceDiscardRectanglePropertiesEXT &discardRectanglePropertiesEXT =
        context.getDiscardRectanglePropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(discardRectanglePropertiesEXT.maxDiscardRectangles), LIM_MIN_UINT32(4)},
    };

    log << TestLog::Message << discardRectanglePropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtSampleLocations(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_sample_locations");
}

tcu::TestStatus validateLimitsExtSampleLocations(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceSampleLocationsPropertiesEXT &sampleLocationsPropertiesEXT =
        context.getSampleLocationsPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.sampleLocationSampleCounts),
         LIM_MIN_BITI32(VK_SAMPLE_COUNT_4_BIT)},
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.maxSampleLocationGridSize.width), LIM_MIN_FLOAT(0.0f)},
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.maxSampleLocationGridSize.height), LIM_MIN_FLOAT(0.0f)},
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.sampleLocationCoordinateRange[0]), LIM_MAX_FLOAT(0.0f)},
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.sampleLocationCoordinateRange[1]), LIM_MIN_FLOAT(0.9375f)},
        {PN(checkAlways), PN(sampleLocationsPropertiesEXT.sampleLocationSubPixelBits), LIM_MIN_UINT32(4)},
    };

    log << TestLog::Message << sampleLocationsPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtExternalMemoryHost(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_external_memory_host");
}

tcu::TestStatus validateLimitsExtExternalMemoryHost(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceExternalMemoryHostPropertiesEXT &externalMemoryHostPropertiesEXT =
        context.getExternalMemoryHostPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(externalMemoryHostPropertiesEXT.minImportedHostPointerAlignment), LIM_MAX_DEVSIZE(65536)},
    };

    log << TestLog::Message << externalMemoryHostPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtBlendOperationAdvanced(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_blend_operation_advanced");
}

tcu::TestStatus validateLimitsExtBlendOperationAdvanced(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT &blendOperationAdvancedPropertiesEXT =
        context.getBlendOperationAdvancedPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(blendOperationAdvancedPropertiesEXT.advancedBlendMaxColorAttachments), LIM_MIN_UINT32(1)},
    };

    log << TestLog::Message << blendOperationAdvancedPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportKhrMaintenance3(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance3");
}

#ifndef CTS_USES_VULKANSC
void checkSupportKhrMaintenance4(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_maintenance4");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus validateLimitsKhrMaintenance3(Context &context)
{
    const VkBool32 checkAlways                                           = VK_TRUE;
    const VkPhysicalDeviceMaintenance3Properties &maintenance3Properties = context.getMaintenance3Properties();
    TestLog &log                                                         = context.getTestContext().getLog();
    bool limitsOk                                                        = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(maintenance3Properties.maxPerSetDescriptors), LIM_MIN_UINT32(1024)},
        {PN(checkAlways), PN(maintenance3Properties.maxMemoryAllocationSize), LIM_MIN_DEVSIZE(1 << 30)},
    };

    log << TestLog::Message << maintenance3Properties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus validateLimitsKhrMaintenance4(Context &context)
{
    const VkBool32 checkAlways                                           = VK_TRUE;
    const VkPhysicalDeviceMaintenance4Properties &maintenance4Properties = context.getMaintenance4Properties();
    TestLog &log                                                         = context.getTestContext().getLog();
    bool limitsOk                                                        = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(maintenance4Properties.maxBufferSize), LIM_MIN_DEVSIZE(1 << 30)},
    };

    log << TestLog::Message << maintenance4Properties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}
#endif // CTS_USES_VULKANSC

void checkSupportExtConservativeRasterization(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");
}

tcu::TestStatus validateLimitsExtConservativeRasterization(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceConservativeRasterizationPropertiesEXT &conservativeRasterizationPropertiesEXT =
        context.getConservativeRasterizationPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(conservativeRasterizationPropertiesEXT.primitiveOverestimationSize), LIM_MIN_FLOAT(0.0f)},
        {PN(checkAlways), PN(conservativeRasterizationPropertiesEXT.maxExtraPrimitiveOverestimationSize),
         LIM_MIN_FLOAT(0.0f)},
        {PN(checkAlways), PN(conservativeRasterizationPropertiesEXT.extraPrimitiveOverestimationSizeGranularity),
         LIM_MIN_FLOAT(0.0f)},
    };

    log << TestLog::Message << conservativeRasterizationPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtDescriptorIndexing(Context &context)
{
    const std::string &requiredDeviceExtension = "VK_EXT_descriptor_indexing";

    if (!context.isDeviceFunctionalitySupported(requiredDeviceExtension))
        TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");

    // Extension string is present, then extension is really supported and should have been added into chain in DefaultDevice properties and features
}

tcu::TestStatus validateLimitsExtDescriptorIndexing(Context &context)
{
    const VkBool32 checkAlways                     = VK_TRUE;
    const VkPhysicalDeviceProperties2 &properties2 = context.getDeviceProperties2();
    const VkPhysicalDeviceLimits &limits           = properties2.properties.limits;
    const VkPhysicalDeviceDescriptorIndexingProperties &descriptorIndexingProperties =
        context.getDescriptorIndexingProperties();
    const VkPhysicalDeviceFeatures &features = context.getDeviceFeatures();
    const uint32_t tessellationShaderCount   = (features.tessellationShader) ? 2 : 0;
    const uint32_t geometryShaderCount       = (features.geometryShader) ? 1 : 0;
    const uint32_t shaderStages              = 3 + tessellationShaderCount + geometryShaderCount;
    TestLog &log                             = context.getTestContext().getLog();
    bool limitsOk                            = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(descriptorIndexingProperties.maxUpdateAfterBindDescriptorsInAllPools),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(12)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageUpdateAfterBindResources), LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(shaderStages * 12)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),
         LIM_MIN_UINT32(8)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),
         LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(500000)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorSamplers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorUniformBuffers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageBuffers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorSampledImages)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorStorageImages)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(limits.maxPerStageDescriptorInputAttachments)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxPerStageUpdateAfterBindResources),
         LIM_MIN_UINT32(limits.maxPerStageResources)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers),
         LIM_MIN_UINT32(limits.maxDescriptorSetSamplers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers),
         LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic),
         LIM_MIN_UINT32(limits.maxDescriptorSetUniformBuffersDynamic)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffers)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageBuffersDynamic)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages),
         LIM_MIN_UINT32(limits.maxDescriptorSetSampledImages)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages),
         LIM_MIN_UINT32(limits.maxDescriptorSetStorageImages)},
        {PN(checkAlways), PN(descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindInputAttachments),
         LIM_MIN_UINT32(limits.maxDescriptorSetInputAttachments)},
    };

    log << TestLog::Message << descriptorIndexingProperties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#ifndef CTS_USES_VULKANSC

void checkSupportExtInlineUniformBlock(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
}

tcu::TestStatus validateLimitsExtInlineUniformBlock(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceInlineUniformBlockProperties &inlineUniformBlockPropertiesEXT =
        context.getInlineUniformBlockProperties();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(inlineUniformBlockPropertiesEXT.maxInlineUniformBlockSize), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(inlineUniformBlockPropertiesEXT.maxPerStageDescriptorInlineUniformBlocks),
         LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(inlineUniformBlockPropertiesEXT.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks),
         LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(inlineUniformBlockPropertiesEXT.maxDescriptorSetInlineUniformBlocks), LIM_MIN_UINT32(4)},
        {PN(checkAlways), PN(inlineUniformBlockPropertiesEXT.maxDescriptorSetUpdateAfterBindInlineUniformBlocks),
         LIM_MIN_UINT32(4)},
    };

    log << TestLog::Message << inlineUniformBlockPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#endif // CTS_USES_VULKANSC

void checkSupportExtVertexAttributeDivisorEXT(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_vertex_attribute_divisor");
}

void checkSupportExtVertexAttributeDivisorKHR(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_vertex_attribute_divisor");
}

tcu::TestStatus validateLimitsExtVertexAttributeDivisorEXT(Context &context)
{
    const VkBool32 checkAlways            = VK_TRUE;
    const InstanceInterface &vk           = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    vk::VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vertexAttributeDivisorPropertiesEXT =
        vk::initVulkanStructure();
    vk::VkPhysicalDeviceProperties2 properties2 = vk::initVulkanStructure(&vertexAttributeDivisorPropertiesEXT);
    TestLog &log                                = context.getTestContext().getLog();
    bool limitsOk                               = true;

    vk.getPhysicalDeviceProperties2(physicalDevice, &properties2);

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(vertexAttributeDivisorPropertiesEXT.maxVertexAttribDivisor),
         LIM_MIN_UINT32((1 << 16) - 1)},
    };

    log << TestLog::Message << vertexAttributeDivisorPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

tcu::TestStatus validateLimitsExtVertexAttributeDivisorKHR(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;

    vk::VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR vertexAttributeDivisorProperties =
        context.getVertexAttributeDivisorProperties();
#ifndef CTS_USES_VULKANSC
    const InstanceInterface &vki                = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice       = context.getPhysicalDevice();
    vk::VkPhysicalDeviceProperties2 properties2 = vk::initVulkanStructure(&vertexAttributeDivisorProperties);
    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
#endif
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(vertexAttributeDivisorProperties.maxVertexAttribDivisor), LIM_MIN_UINT32((1 << 16) - 1)},
    };

    log << TestLog::Message << vertexAttributeDivisorProperties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#ifndef CTS_USES_VULKANSC

void checkSupportNvMeshShader(Context &context)
{
    const std::string &requiredDeviceExtension = "VK_NV_mesh_shader";

    if (!context.isDeviceFunctionalitySupported(requiredDeviceExtension))
        TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");
}

tcu::TestStatus validateLimitsNvMeshShader(Context &context)
{
    const VkBool32 checkAlways                                    = VK_TRUE;
    const VkPhysicalDevice physicalDevice                         = context.getPhysicalDevice();
    const InstanceInterface &vki                                  = context.getInstanceInterface();
    TestLog &log                                                  = context.getTestContext().getLog();
    bool limitsOk                                                 = true;
    VkPhysicalDeviceMeshShaderPropertiesNV meshShaderPropertiesNV = initVulkanStructure();
    VkPhysicalDeviceProperties2 properties2                       = initVulkanStructure(&meshShaderPropertiesNV);

    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxDrawMeshTasksCount), LIM_MIN_UINT32(uint32_t((1ull << 16) - 1))},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskWorkGroupInvocations), LIM_MIN_UINT32(32)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[0]), LIM_MIN_UINT32(32)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[1]), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskWorkGroupSize[2]), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskTotalMemorySize), LIM_MIN_UINT32(16384)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxTaskOutputCount), LIM_MIN_UINT32((1 << 16) - 1)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshWorkGroupInvocations), LIM_MIN_UINT32(32)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[0]), LIM_MIN_UINT32(32)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[1]), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshWorkGroupSize[2]), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshTotalMemorySize), LIM_MIN_UINT32(16384)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshOutputVertices), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshOutputPrimitives), LIM_MIN_UINT32(256)},
        {PN(checkAlways), PN(meshShaderPropertiesNV.maxMeshMultiviewViewCount), LIM_MIN_UINT32(1)},
    };

    log << TestLog::Message << meshShaderPropertiesNV << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtTransformFeedback(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_transform_feedback");
}

tcu::TestStatus validateLimitsExtTransformFeedback(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceTransformFeedbackPropertiesEXT &transformFeedbackPropertiesEXT =
        context.getTransformFeedbackPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackStreams), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBuffers), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferSize),
         LIM_MIN_DEVSIZE(1ull << 27)},
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackStreamDataSize), LIM_MIN_UINT32(512)},
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferDataSize), LIM_MIN_UINT32(512)},
        {PN(checkAlways), PN(transformFeedbackPropertiesEXT.maxTransformFeedbackBufferDataStride), LIM_MIN_UINT32(512)},
    };

    log << TestLog::Message << transformFeedbackPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtFragmentDensityMap(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_fragment_density_map");
}

tcu::TestStatus validateLimitsExtFragmentDensityMap(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceFragmentDensityMapPropertiesEXT &fragmentDensityMapPropertiesEXT =
        context.getFragmentDensityMapPropertiesEXT();
    TestLog &log  = context.getTestContext().getLog();
    bool limitsOk = true;

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(fragmentDensityMapPropertiesEXT.minFragmentDensityTexelSize.width), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(fragmentDensityMapPropertiesEXT.minFragmentDensityTexelSize.height), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(fragmentDensityMapPropertiesEXT.maxFragmentDensityTexelSize.width), LIM_MIN_UINT32(1)},
        {PN(checkAlways), PN(fragmentDensityMapPropertiesEXT.maxFragmentDensityTexelSize.height), LIM_MIN_UINT32(1)},
    };

    log << TestLog::Message << fragmentDensityMapPropertiesEXT << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportNvRayTracing(Context &context)
{
    const std::string &requiredDeviceExtension = "VK_NV_ray_tracing";

    if (!context.isDeviceFunctionalitySupported(requiredDeviceExtension))
        TCU_THROW(NotSupportedError, requiredDeviceExtension + " is not supported");
}

tcu::TestStatus validateLimitsNvRayTracing(Context &context)
{
    const VkBool32 checkAlways                                    = VK_TRUE;
    const VkPhysicalDevice physicalDevice                         = context.getPhysicalDevice();
    const InstanceInterface &vki                                  = context.getInstanceInterface();
    TestLog &log                                                  = context.getTestContext().getLog();
    bool limitsOk                                                 = true;
    VkPhysicalDeviceRayTracingPropertiesNV rayTracingPropertiesNV = initVulkanStructure();
    VkPhysicalDeviceProperties2 properties2                       = initVulkanStructure(&rayTracingPropertiesNV);

    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(rayTracingPropertiesNV.shaderGroupHandleSize), LIM_MIN_UINT32(16)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.maxRecursionDepth), LIM_MIN_UINT32(31)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.shaderGroupBaseAlignment), LIM_MIN_UINT32(64)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.maxGeometryCount), LIM_MIN_UINT32((1 << 24) - 1)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.maxInstanceCount), LIM_MIN_UINT32((1 << 24) - 1)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.maxTriangleCount), LIM_MIN_UINT32((1 << 29) - 1)},
        {PN(checkAlways), PN(rayTracingPropertiesNV.maxDescriptorSetAccelerationStructures), LIM_MIN_UINT32(16)},
    };

    log << TestLog::Message << rayTracingPropertiesNV << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#endif // CTS_USES_VULKANSC

void checkSupportKhrTimelineSemaphore(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");
}

tcu::TestStatus validateLimitsKhrTimelineSemaphore(Context &context)
{
    const VkBool32 checkAlways = VK_TRUE;
    const VkPhysicalDeviceTimelineSemaphoreProperties &timelineSemaphoreProperties =
        context.getTimelineSemaphoreProperties();
    bool limitsOk = true;
    TestLog &log  = context.getTestContext().getLog();

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(timelineSemaphoreProperties.maxTimelineSemaphoreValueDifference),
         LIM_MIN_DEVSIZE((1ull << 31) - 1)},
    };

    log << TestLog::Message << timelineSemaphoreProperties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportExtLineRasterization(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_line_rasterization");
}

void checkSupportKhrLineRasterization(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_line_rasterization");
}

tcu::TestStatus validateLimitsLineRasterization(Context &context)
{
    const VkBool32 checkAlways                  = VK_TRUE;
    TestLog &log                                = context.getTestContext().getLog();
    bool limitsOk                               = true;
    auto lineRasterizationProperties            = context.getLineRasterizationProperties();
    vk::VkPhysicalDeviceProperties2 properties2 = vk::initVulkanStructure(&lineRasterizationProperties);
    const InstanceInterface &vk                 = context.getInstanceInterface();

    vk.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(lineRasterizationProperties.lineSubPixelPrecisionBits), LIM_MIN_UINT32(4)},
    };

    log << TestLog::Message << lineRasterizationProperties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

void checkSupportRobustness2(Context &context)
{
    if (!context.isDeviceFunctionalitySupported("VK_EXT_robustness2") &&
        !context.isDeviceFunctionalitySupported("VK_KHR_robustness2"))

        TCU_THROW(NotSupportedError, "VK_EXT_robustness2 and VK_KHR_robustness2 are not supported");
}

tcu::TestStatus validateLimitsRobustness2(Context &context)
{
    const InstanceInterface &vki                                          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                                 = context.getPhysicalDevice();
    const VkPhysicalDeviceRobustness2PropertiesEXT &robustness2Properties = context.getRobustness2Properties();
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features            = initVulkanStructure();
    VkPhysicalDeviceFeatures2 features2                                   = initVulkanStructure(&robustness2Features);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (robustness2Features.robustBufferAccess2 && !features2.features.robustBufferAccess)
        return tcu::TestStatus::fail("If robustBufferAccess2 is enabled then robustBufferAccess must also be enabled");

    if (robustness2Properties.robustStorageBufferAccessSizeAlignment != 1 &&
        robustness2Properties.robustStorageBufferAccessSizeAlignment != 4)
        return tcu::TestStatus::fail(
            "robustness2Properties.robustStorageBufferAccessSizeAlignment value must be either 1 or 4.");

    if (!de::inRange(robustness2Properties.robustUniformBufferAccessSizeAlignment, (VkDeviceSize)1u,
                     (VkDeviceSize)256u) ||
        !deIsPowerOfTwo64(robustness2Properties.robustUniformBufferAccessSizeAlignment))
        return tcu::TestStatus::fail("robustness2Properties.robustUniformBufferAccessSizeAlignment must be a power "
                                     "of two in the range [1, 256]");

    return tcu::TestStatus::pass("pass");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus validateLimitsMaxInlineUniformTotalSize(Context &context)
{
    const VkBool32 checkAlways                                   = VK_TRUE;
    const VkPhysicalDeviceVulkan13Properties &vulkan13Properties = context.getDeviceVulkan13Properties();
    bool limitsOk                                                = true;
    TestLog &log                                                 = context.getTestContext().getLog();

    FeatureLimitTableItem featureLimitTable[] = {
        {PN(checkAlways), PN(vulkan13Properties.maxInlineUniformTotalSize), LIM_MIN_DEVSIZE(256)},
    };

    log << TestLog::Message << vulkan13Properties << TestLog::EndMessage;

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
        limitsOk = validateLimit(featureLimitTable[ndx], log) && limitsOk;

    if (limitsOk)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

#define ROADMAP_FEATURE_ITEM(STRUC, FIELD)          \
    {                                               \
        &(STRUC), &(STRUC.FIELD), #STRUC "." #FIELD \
    }

struct FeatureEntry
{
    void *structPtr;
    VkBool32 *fieldPtr;
    const char *fieldName;
};

struct FormatEntry
{
    VkFormat format;
    const std::string name;
    VkFormatProperties properties;
};

struct ProfileEntry
{
    std::string name;
    void (*checkSupportFunction)(Context &);
    tcu::TestStatus (*validateFunction)(Context &);
};

#include "vkProfileTests.inl"

#endif // CTS_USES_VULKANSC

void createTestDevice(Context &context, void *pNext, const char *const *ppEnabledExtensionNames,
                      uint32_t enabledExtensionCount)
{
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const auto validationEnabled               = context.getTestContext().getCommandLine().isValidationEnabled();
    const Unique<VkInstance> instance(createDefaultInstance(platformInterface, context.getUsedApiVersion(),
                                                            context.getTestContext().getCommandLine()));
    const InstanceDriver instanceDriver(platformInterface, instance.get());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
    const uint32_t queueFamilyIndex = 0;
    const uint32_t queueCount       = 1;
    const uint32_t queueIndex       = 0;
    const float queuePriority       = 1.0f;
    const vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                    //  const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               //  VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           //  uint32_t queueFamilyIndex;
        queueCount,                                 //  uint32_t queueCount;
        &queuePriority,                             //  const float* pQueuePriorities;
    };
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (context.getTestContext().getCommandLine().isSubProcess())
    {
        if (context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                context.getResourceInterface()->getCacheDataSize(),       // uintptr_t initialDataSize;
                context.getResourceInterface()->getCacheData()            // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //  VkStructureType sType;
        pNext,                                //  const void* pNext;
        (VkDeviceCreateFlags)0u,              //  VkDeviceCreateFlags flags;
        1,                                    //  uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfo,               //  const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0,                                    //  uint32_t enabledLayerCount;
        nullptr,                              //  const char* const* ppEnabledLayerNames;
        enabledExtensionCount,                //  uint32_t enabledExtensionCount;
        ppEnabledExtensionNames,              //  const char* const* ppEnabledExtensionNames;
        nullptr,                              //  const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };
    const Unique<VkDevice> device(createCustomDevice(validationEnabled, platformInterface, *instance, instanceDriver,
                                                     physicalDevice, &deviceCreateInfo));
    const DeviceDriver deviceDriver(platformInterface, instance.get(), device.get(), context.getUsedApiVersion(),
                                    context.getTestContext().getCommandLine());
    const VkQueue queue = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);

    VK_CHECK(deviceDriver.queueWaitIdle(queue));
}

void cleanVulkanStruct(void *structPtr, size_t structSize)
{
    struct StructureBase
    {
        VkStructureType sType;
        void *pNext;
    };

    VkStructureType sType = ((StructureBase *)structPtr)->sType;

    deMemset(structPtr, 0, structSize);

    ((StructureBase *)structPtr)->sType = sType;
}

template <uint32_t VK_API_VERSION>
tcu::TestStatus featureBitInfluenceOnDeviceCreate(Context &context)
{
#define FEATURE_TABLE_ITEM(CORE, EXT, FIELD, STR)                                                                   \
    {                                                                                                               \
        &(CORE), sizeof(CORE), &(CORE.FIELD), #CORE "." #FIELD, &(EXT), sizeof(EXT), &(EXT.FIELD), #EXT "." #FIELD, \
            STR                                                                                                     \
    }
#define DEPENDENCY_DUAL_ITEM(CORE, EXT, FIELD, PARENT) \
    {&(CORE.FIELD), &(CORE.PARENT)},                   \
    {                                                  \
        &(EXT.FIELD), &(EXT.PARENT)                    \
    }
#define DEPENDENCY_SINGLE_ITEM(CORE, FIELD, PARENT) \
    {                                               \
        &(CORE.FIELD), &(CORE.PARENT)               \
    }

    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();
    TestLog &log                          = context.getTestContext().getLog();
    const std::vector<VkExtensionProperties> deviceExtensionProperties =
        enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();

    VkPhysicalDeviceVulkan11Features vulkan11Features                                       = initVulkanStructure();
    VkPhysicalDeviceVulkan12Features vulkan12Features                                       = initVulkanStructure();
    VkPhysicalDevice16BitStorageFeatures sixteenBitStorageFeatures                          = initVulkanStructure();
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures                                     = initVulkanStructure();
    VkPhysicalDeviceVariablePointersFeatures variablePointersFeatures                       = initVulkanStructure();
    VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeatures                         = initVulkanStructure();
    VkPhysicalDeviceSamplerYcbcrConversionFeatures samplerYcbcrConversionFeatures           = initVulkanStructure();
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures               = initVulkanStructure();
    VkPhysicalDevice8BitStorageFeatures eightBitStorageFeatures                             = initVulkanStructure();
    VkPhysicalDeviceShaderAtomicInt64Features shaderAtomicInt64Features                     = initVulkanStructure();
    VkPhysicalDeviceShaderFloat16Int8Features shaderFloat16Int8Features                     = initVulkanStructure();
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures                   = initVulkanStructure();
    VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures                     = initVulkanStructure();
    VkPhysicalDeviceImagelessFramebufferFeatures imagelessFramebufferFeatures               = initVulkanStructure();
    VkPhysicalDeviceUniformBufferStandardLayoutFeatures uniformBufferStandardLayoutFeatures = initVulkanStructure();
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures shaderSubgroupExtendedTypesFeatures = initVulkanStructure();
    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilLayoutsFeatures = initVulkanStructure();
    VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures                           = initVulkanStructure();
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures                     = initVulkanStructure();
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures                 = initVulkanStructure();
    VkPhysicalDeviceVulkanMemoryModelFeatures vulkanMemoryModelFeatures                     = initVulkanStructure();

#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceVulkan13Features vulkan13Features                                         = initVulkanStructure();
    VkPhysicalDeviceImageRobustnessFeatures imageRobustnessFeatures                           = initVulkanStructure();
    VkPhysicalDeviceInlineUniformBlockFeatures inlineUniformBlockFeatures                     = initVulkanStructure();
    VkPhysicalDevicePipelineCreationCacheControlFeatures pipelineCreationCacheControlFeatures = initVulkanStructure();
    VkPhysicalDevicePrivateDataFeatures privateDataFeatures                                   = initVulkanStructure();
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures shaderDemoteToHelperInvocationFeatures =
        initVulkanStructure();
    VkPhysicalDeviceShaderTerminateInvocationFeatures shaderTerminateInvocationFeatures         = initVulkanStructure();
    VkPhysicalDeviceSubgroupSizeControlFeatures subgroupSizeControlFeatures                     = initVulkanStructure();
    VkPhysicalDeviceSynchronization2Features synchronization2Features                           = initVulkanStructure();
    VkPhysicalDeviceTextureCompressionASTCHDRFeatures textureCompressionASTCHDRFeatures         = initVulkanStructure();
    VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures zeroInitializeWorkgroupMemoryFeatures = initVulkanStructure();
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures                           = initVulkanStructure();
    VkPhysicalDeviceShaderIntegerDotProductFeatures shaderIntegerDotProductFeatures             = initVulkanStructure();
    VkPhysicalDeviceMaintenance4Features maintenance4Features                                   = initVulkanStructure();
#endif // CTS_USES_VULKANSC

    struct UnusedExtensionFeatures
    {
        VkStructureType sType;
        void *pNext;
        VkBool32 descriptorIndexing;
        VkBool32 samplerFilterMinmax;
    } unusedExtensionFeatures;

    struct FeatureTable
    {
        void *coreStructPtr;
        size_t coreStructSize;
        VkBool32 *coreFieldPtr;
        const char *coreFieldName;
        void *extStructPtr;
        size_t extStructSize;
        VkBool32 *extFieldPtr;
        const char *extFieldName;
        const char *extString;
    };
    struct FeatureDependencyTable
    {
        VkBool32 *featurePtr;
        VkBool32 *dependOnPtr;
    };

    std::vector<FeatureTable> featureTable;
    std::vector<FeatureDependencyTable> featureDependencyTable;

    if (VK_API_VERSION == VK_API_VERSION_1_2)
    {
        featureTable = {
            FEATURE_TABLE_ITEM(vulkan11Features, sixteenBitStorageFeatures, storageBuffer16BitAccess,
                               "VK_KHR_16bit_storage"),
            FEATURE_TABLE_ITEM(vulkan11Features, sixteenBitStorageFeatures, uniformAndStorageBuffer16BitAccess,
                               "VK_KHR_16bit_storage"),
            FEATURE_TABLE_ITEM(vulkan11Features, sixteenBitStorageFeatures, storagePushConstant16,
                               "VK_KHR_16bit_storage"),
            FEATURE_TABLE_ITEM(vulkan11Features, sixteenBitStorageFeatures, storageInputOutput16,
                               "VK_KHR_16bit_storage"),
            FEATURE_TABLE_ITEM(vulkan11Features, multiviewFeatures, multiview, "VK_KHR_multiview"),
            FEATURE_TABLE_ITEM(vulkan11Features, multiviewFeatures, multiviewGeometryShader, "VK_KHR_multiview"),
            FEATURE_TABLE_ITEM(vulkan11Features, multiviewFeatures, multiviewTessellationShader, "VK_KHR_multiview"),
            FEATURE_TABLE_ITEM(vulkan11Features, variablePointersFeatures, variablePointersStorageBuffer,
                               "VK_KHR_variable_pointers"),
            FEATURE_TABLE_ITEM(vulkan11Features, variablePointersFeatures, variablePointers,
                               "VK_KHR_variable_pointers"),
            FEATURE_TABLE_ITEM(vulkan11Features, protectedMemoryFeatures, protectedMemory, nullptr),
            FEATURE_TABLE_ITEM(vulkan11Features, samplerYcbcrConversionFeatures, samplerYcbcrConversion,
                               "VK_KHR_sampler_ycbcr_conversion"),
            FEATURE_TABLE_ITEM(vulkan11Features, shaderDrawParametersFeatures, shaderDrawParameters, nullptr),
            FEATURE_TABLE_ITEM(vulkan12Features, eightBitStorageFeatures, storageBuffer8BitAccess,
                               "VK_KHR_8bit_storage"),
            FEATURE_TABLE_ITEM(vulkan12Features, eightBitStorageFeatures, uniformAndStorageBuffer8BitAccess,
                               "VK_KHR_8bit_storage"),
            FEATURE_TABLE_ITEM(vulkan12Features, eightBitStorageFeatures, storagePushConstant8, "VK_KHR_8bit_storage"),
            FEATURE_TABLE_ITEM(vulkan12Features, shaderAtomicInt64Features, shaderBufferInt64Atomics,
                               "VK_KHR_shader_atomic_int64"),
            FEATURE_TABLE_ITEM(vulkan12Features, shaderAtomicInt64Features, shaderSharedInt64Atomics,
                               "VK_KHR_shader_atomic_int64"),
            FEATURE_TABLE_ITEM(vulkan12Features, shaderFloat16Int8Features, shaderFloat16,
                               "VK_KHR_shader_float16_int8"),
            FEATURE_TABLE_ITEM(vulkan12Features, shaderFloat16Int8Features, shaderInt8, "VK_KHR_shader_float16_int8"),
            FEATURE_TABLE_ITEM(vulkan12Features, unusedExtensionFeatures, descriptorIndexing, nullptr),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, shaderInputAttachmentArrayDynamicIndexing,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               shaderUniformTexelBufferArrayDynamicIndexing, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               shaderStorageTexelBufferArrayDynamicIndexing, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, shaderUniformBufferArrayNonUniformIndexing,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, shaderSampledImageArrayNonUniformIndexing,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, shaderStorageBufferArrayNonUniformIndexing,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, shaderStorageImageArrayNonUniformIndexing,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               shaderInputAttachmentArrayNonUniformIndexing, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               shaderUniformTexelBufferArrayNonUniformIndexing, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               shaderStorageTexelBufferArrayNonUniformIndexing, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingUniformBufferUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingSampledImageUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingStorageImageUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingStorageBufferUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingUniformTexelBufferUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures,
                               descriptorBindingStorageTexelBufferUpdateAfterBind, "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, descriptorBindingUpdateUnusedWhilePending,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, descriptorBindingPartiallyBound,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, descriptorBindingVariableDescriptorCount,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, descriptorIndexingFeatures, runtimeDescriptorArray,
                               "VK_EXT_descriptor_indexing"),
            FEATURE_TABLE_ITEM(vulkan12Features, unusedExtensionFeatures, samplerFilterMinmax,
                               "VK_EXT_sampler_filter_minmax"),
            FEATURE_TABLE_ITEM(vulkan12Features, scalarBlockLayoutFeatures, scalarBlockLayout,
                               "VK_EXT_scalar_block_layout"),
            FEATURE_TABLE_ITEM(vulkan12Features, imagelessFramebufferFeatures, imagelessFramebuffer,
                               "VK_KHR_imageless_framebuffer"),
            FEATURE_TABLE_ITEM(vulkan12Features, uniformBufferStandardLayoutFeatures, uniformBufferStandardLayout,
                               "VK_KHR_uniform_buffer_standard_layout"),
            FEATURE_TABLE_ITEM(vulkan12Features, shaderSubgroupExtendedTypesFeatures, shaderSubgroupExtendedTypes,
                               "VK_KHR_shader_subgroup_extended_types"),
            FEATURE_TABLE_ITEM(vulkan12Features, separateDepthStencilLayoutsFeatures, separateDepthStencilLayouts,
                               "VK_KHR_separate_depth_stencil_layouts"),
            FEATURE_TABLE_ITEM(vulkan12Features, hostQueryResetFeatures, hostQueryReset, "VK_EXT_host_query_reset"),
            FEATURE_TABLE_ITEM(vulkan12Features, timelineSemaphoreFeatures, timelineSemaphore,
                               "VK_KHR_timeline_semaphore"),
            FEATURE_TABLE_ITEM(vulkan12Features, bufferDeviceAddressFeatures, bufferDeviceAddress,
                               "VK_EXT_buffer_device_address"),
            FEATURE_TABLE_ITEM(vulkan12Features, bufferDeviceAddressFeatures, bufferDeviceAddressCaptureReplay,
                               "VK_EXT_buffer_device_address"),
            FEATURE_TABLE_ITEM(vulkan12Features, bufferDeviceAddressFeatures, bufferDeviceAddressMultiDevice,
                               "VK_EXT_buffer_device_address"),
            FEATURE_TABLE_ITEM(vulkan12Features, vulkanMemoryModelFeatures, vulkanMemoryModel,
                               "VK_KHR_vulkan_memory_model"),
            FEATURE_TABLE_ITEM(vulkan12Features, vulkanMemoryModelFeatures, vulkanMemoryModelDeviceScope,
                               "VK_KHR_vulkan_memory_model"),
            FEATURE_TABLE_ITEM(vulkan12Features, vulkanMemoryModelFeatures,
                               vulkanMemoryModelAvailabilityVisibilityChains, "VK_KHR_vulkan_memory_model"),
        };

        featureDependencyTable = {
            DEPENDENCY_DUAL_ITEM(vulkan11Features, multiviewFeatures, multiviewGeometryShader, multiview),
            DEPENDENCY_DUAL_ITEM(vulkan11Features, multiviewFeatures, multiviewTessellationShader, multiview),
            DEPENDENCY_DUAL_ITEM(vulkan11Features, variablePointersFeatures, variablePointers,
                                 variablePointersStorageBuffer),
            DEPENDENCY_DUAL_ITEM(vulkan12Features, bufferDeviceAddressFeatures, bufferDeviceAddressCaptureReplay,
                                 bufferDeviceAddress),
            DEPENDENCY_DUAL_ITEM(vulkan12Features, bufferDeviceAddressFeatures, bufferDeviceAddressMultiDevice,
                                 bufferDeviceAddress),
            DEPENDENCY_DUAL_ITEM(vulkan12Features, vulkanMemoryModelFeatures, vulkanMemoryModelDeviceScope,
                                 vulkanMemoryModel),
            DEPENDENCY_DUAL_ITEM(vulkan12Features, vulkanMemoryModelFeatures,
                                 vulkanMemoryModelAvailabilityVisibilityChains, vulkanMemoryModel),
        };
    }
#ifndef CTS_USES_VULKANSC
    else // if (VK_API_VERSION == VK_API_VERSION_1_3)
    {
        featureTable = {
            FEATURE_TABLE_ITEM(vulkan13Features, imageRobustnessFeatures, robustImageAccess, "VK_EXT_image_robustness"),
            FEATURE_TABLE_ITEM(vulkan13Features, inlineUniformBlockFeatures, inlineUniformBlock,
                               "VK_EXT_inline_uniform_block"),
            FEATURE_TABLE_ITEM(vulkan13Features, inlineUniformBlockFeatures,
                               descriptorBindingInlineUniformBlockUpdateAfterBind, "VK_EXT_inline_uniform_block"),
            FEATURE_TABLE_ITEM(vulkan13Features, pipelineCreationCacheControlFeatures, pipelineCreationCacheControl,
                               "VK_EXT_pipeline_creation_cache_control"),
            FEATURE_TABLE_ITEM(vulkan13Features, privateDataFeatures, privateData, "VK_EXT_private_data"),
            FEATURE_TABLE_ITEM(vulkan13Features, shaderDemoteToHelperInvocationFeatures, shaderDemoteToHelperInvocation,
                               "VK_EXT_shader_demote_to_helper_invocation"),
            FEATURE_TABLE_ITEM(vulkan13Features, shaderTerminateInvocationFeatures, shaderTerminateInvocation,
                               "VK_KHR_shader_terminate_invocation"),
            FEATURE_TABLE_ITEM(vulkan13Features, subgroupSizeControlFeatures, subgroupSizeControl,
                               "VK_EXT_subgroup_size_control"),
            FEATURE_TABLE_ITEM(vulkan13Features, subgroupSizeControlFeatures, computeFullSubgroups,
                               "VK_EXT_subgroup_size_control"),
            FEATURE_TABLE_ITEM(vulkan13Features, synchronization2Features, synchronization2, "VK_KHR_synchronization2"),
            FEATURE_TABLE_ITEM(vulkan13Features, textureCompressionASTCHDRFeatures, textureCompressionASTC_HDR,
                               "VK_EXT_texture_compression_astc_hdr"),
            FEATURE_TABLE_ITEM(vulkan13Features, zeroInitializeWorkgroupMemoryFeatures,
                               shaderZeroInitializeWorkgroupMemory, "VK_KHR_zero_initialize_workgroup_memory"),
            FEATURE_TABLE_ITEM(vulkan13Features, dynamicRenderingFeatures, dynamicRendering,
                               "VK_KHR_dynamic_rendering"),
            FEATURE_TABLE_ITEM(vulkan13Features, shaderIntegerDotProductFeatures, shaderIntegerDotProduct,
                               "VK_KHR_shader_integer_dot_product"),
            FEATURE_TABLE_ITEM(vulkan13Features, maintenance4Features, maintenance4, "VK_KHR_maintenance4"),
        };
    }
#endif // CTS_USES_VULKANSC

    deMemset(&unusedExtensionFeatures, 0, sizeof(unusedExtensionFeatures));

    for (FeatureTable &testedFeature : featureTable)
    {
        VkBool32 coreFeatureState = false;
        VkBool32 extFeatureState  = false;

        // Core test
        {
            void *structPtr      = testedFeature.coreStructPtr;
            size_t structSize    = testedFeature.coreStructSize;
            VkBool32 *featurePtr = testedFeature.coreFieldPtr;

            if (structPtr != &unusedExtensionFeatures)
                features2.pNext = structPtr;

            vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

            coreFeatureState = featurePtr[0];

            log << TestLog::Message << "Feature status " << testedFeature.coreFieldName << "=" << coreFeatureState
                << TestLog::EndMessage;

            if (coreFeatureState)
            {
                cleanVulkanStruct(structPtr, structSize);

                featurePtr[0] = true;

                for (FeatureDependencyTable featureDependency : featureDependencyTable)
                    if (featureDependency.featurePtr == featurePtr)
                        featureDependency.dependOnPtr[0] = true;

                createTestDevice(context, &features2, nullptr, 0u);
            }
        }

        // ext test
        {
            void *structPtr          = testedFeature.extStructPtr;
            size_t structSize        = testedFeature.extStructSize;
            VkBool32 *featurePtr     = testedFeature.extFieldPtr;
            const char *extStringPtr = testedFeature.extString;

            if (structPtr != &unusedExtensionFeatures)
                features2.pNext = structPtr;

            if (extStringPtr == nullptr ||
                isExtensionStructSupported(deviceExtensionProperties, RequiredExtension(extStringPtr)))
            {
                vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

                extFeatureState = *featurePtr;

                log << TestLog::Message << "Feature status " << testedFeature.extFieldName << "=" << extFeatureState
                    << TestLog::EndMessage;

                if (extFeatureState)
                {
                    cleanVulkanStruct(structPtr, structSize);

                    featurePtr[0] = true;

                    for (FeatureDependencyTable &featureDependency : featureDependencyTable)
                        if (featureDependency.featurePtr == featurePtr)
                            featureDependency.dependOnPtr[0] = true;

                    createTestDevice(context, &features2, &extStringPtr, (extStringPtr == nullptr) ? 0u : 1u);
                }
            }
        }
    }

    return tcu::TestStatus::pass("pass");
}

template <typename T>
class CheckIncompleteResult
{
public:
    virtual ~CheckIncompleteResult(void)
    {
    }
    virtual void getResult(Context &context, T *data) = 0;

    void operator()(Context &context, tcu::ResultCollector &results, const std::size_t expectedCompleteSize)
    {
        if (expectedCompleteSize == 0)
            return;

        vector<T> outputData(expectedCompleteSize);
        const uint32_t usedSize = static_cast<uint32_t>(expectedCompleteSize / 3);

        ValidateQueryBits::fillBits(outputData.begin(),
                                    outputData.end()); // unused entries should have this pattern intact
        m_count  = usedSize;
        m_result = VK_SUCCESS;

        getResult(context, &outputData[0]); // update m_count and m_result

        if (m_count != usedSize || m_result != VK_INCOMPLETE ||
            !ValidateQueryBits::checkBits(outputData.begin() + m_count, outputData.end()))
            results.fail("Query didn't return VK_INCOMPLETE");
    }

protected:
    uint32_t m_count;
    VkResult m_result;
};

struct CheckEnumeratePhysicalDevicesIncompleteResult : public CheckIncompleteResult<VkPhysicalDevice>
{
    void getResult(Context &context, VkPhysicalDevice *data)
    {
        m_result = context.getInstanceInterface().enumeratePhysicalDevices(context.getInstance(), &m_count, data);
    }
};

struct CheckEnumeratePhysicalDeviceGroupsIncompleteResult
    : public CheckIncompleteResult<VkPhysicalDeviceGroupProperties>
{
    CheckEnumeratePhysicalDeviceGroupsIncompleteResult(const InstanceInterface &vki, const VkInstance instance)
        : m_vki(vki)
        , m_instance(instance)
    {
    }

    void getResult(Context &, VkPhysicalDeviceGroupProperties *data)
    {
        for (uint32_t idx = 0u; idx < m_count; ++idx)
            data[idx] = initVulkanStructure();
        m_result = m_vki.enumeratePhysicalDeviceGroups(m_instance, &m_count, data);
    }

protected:
    const InstanceInterface &m_vki;
    const VkInstance m_instance;
};

struct CheckEnumerateInstanceLayerPropertiesIncompleteResult : public CheckIncompleteResult<VkLayerProperties>
{
    void getResult(Context &context, VkLayerProperties *data)
    {
        m_result = context.getPlatformInterface().enumerateInstanceLayerProperties(&m_count, data);
    }
};

struct CheckEnumerateDeviceLayerPropertiesIncompleteResult : public CheckIncompleteResult<VkLayerProperties>
{
    void getResult(Context &context, VkLayerProperties *data)
    {
        m_result =
            context.getInstanceInterface().enumerateDeviceLayerProperties(context.getPhysicalDevice(), &m_count, data);
    }
};

struct CheckEnumerateInstanceExtensionPropertiesIncompleteResult : public CheckIncompleteResult<VkExtensionProperties>
{
    CheckEnumerateInstanceExtensionPropertiesIncompleteResult(std::string layerName = std::string())
        : m_layerName(layerName)
    {
    }

    void getResult(Context &context, VkExtensionProperties *data)
    {
        const char *pLayerName = (m_layerName.length() != 0 ? m_layerName.c_str() : nullptr);
        m_result = context.getPlatformInterface().enumerateInstanceExtensionProperties(pLayerName, &m_count, data);
    }

private:
    const std::string m_layerName;
};

struct CheckEnumerateDeviceExtensionPropertiesIncompleteResult : public CheckIncompleteResult<VkExtensionProperties>
{
    CheckEnumerateDeviceExtensionPropertiesIncompleteResult(std::string layerName = std::string())
        : m_layerName(layerName)
    {
    }

    void getResult(Context &context, VkExtensionProperties *data)
    {
        const char *pLayerName = (m_layerName.length() != 0 ? m_layerName.c_str() : nullptr);
        m_result = context.getInstanceInterface().enumerateDeviceExtensionProperties(context.getPhysicalDevice(),
                                                                                     pLayerName, &m_count, data);
    }

private:
    const std::string m_layerName;
};

tcu::TestStatus enumeratePhysicalDevices(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);
    const vector<VkPhysicalDevice> devices =
        enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

    log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, int64_t(devices.size()));

    for (size_t ndx = 0; ndx < devices.size(); ndx++)
        log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

    CheckEnumeratePhysicalDevicesIncompleteResult()(context, results, devices.size());

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumeratePhysicalDeviceGroups(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);
    const VkInstance instance    = context.getInstance(); // "VK_KHR_device_group_creation"
    const InstanceInterface &vki = context.getInstanceInterface();
    const vector<VkPhysicalDeviceGroupProperties> devicegroups = enumeratePhysicalDeviceGroups(vki, instance);

    log << TestLog::Integer("NumDevices", "Number of device groups", "", QP_KEY_TAG_NONE, int64_t(devicegroups.size()));

    for (size_t ndx = 0; ndx < devicegroups.size(); ndx++)
        log << TestLog::Message << ndx << ": " << devicegroups[ndx] << TestLog::EndMessage;

    CheckEnumeratePhysicalDeviceGroupsIncompleteResult(vki, instance)(context, results, devicegroups.size());

    context.collectAndReportDebugMessages();
    return tcu::TestStatus(results.getResult(), results.getMessage());
}

template <typename T>
void collectDuplicates(set<T> &duplicates, const vector<T> &values)
{
    set<T> seen;

    for (size_t ndx = 0; ndx < values.size(); ndx++)
    {
        const T &value = values[ndx];

        if (!seen.insert(value).second)
            duplicates.insert(value);
    }
}

void checkDuplicates(tcu::ResultCollector &results, const char *what, const vector<string> &values)
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

void checkDuplicateExtensions(tcu::ResultCollector &results, const vector<string> &extensions)
{
    checkDuplicates(results, "extension", extensions);
}

void checkDuplicateLayers(tcu::ResultCollector &results, const vector<string> &layers)
{
    checkDuplicates(results, "layer", layers);
}

void checkKhrExtensions(tcu::ResultCollector &results, const vector<string> &extensions,
                        const int numAllowedKhrExtensions, const char *const *allowedKhrExtensions)
{
    const set<string> allowedExtSet(allowedKhrExtensions, allowedKhrExtensions + numAllowedKhrExtensions);

    for (vector<string>::const_iterator extIter = extensions.begin(); extIter != extensions.end(); ++extIter)
    {
        // Only Khronos-controlled extensions are checked
        if (de::beginsWith(*extIter, "VK_KHR_") && !de::contains(allowedExtSet, *extIter))
        {
            results.fail("Unknown extension " + *extIter);
        }
    }
}

void checkInstanceExtensions(tcu::ResultCollector &results, const vector<string> &extensions)
{
#include "vkInstanceExtensions.inl"

    checkKhrExtensions(results, extensions, DE_LENGTH_OF_ARRAY(s_allowedInstanceKhrExtensions),
                       s_allowedInstanceKhrExtensions);
    checkDuplicateExtensions(results, extensions);
}

void checkDeviceExtensions(tcu::ResultCollector &results, const vector<string> &extensions)
{
#include "vkDeviceExtensions.inl"

    checkKhrExtensions(results, extensions, DE_LENGTH_OF_ARRAY(s_allowedDeviceKhrExtensions),
                       s_allowedDeviceKhrExtensions);
    checkDuplicateExtensions(results, extensions);
}

#ifndef CTS_USES_VULKANSC

void checkExtensionDependencies(tcu::ResultCollector &results, const DependencyCheckVect &dependencies,
                                uint32_t versionMajor, uint32_t versionMinor,
                                const ExtPropVect &instanceExtensionProperties,
                                const ExtPropVect &deviceExtensionProperties)
{
    tcu::UVec2 v(versionMajor, versionMinor);
    for (const auto &dependency : dependencies)
    {
        // call function that will check all extension dependencies
        if (!dependency.second(v, instanceExtensionProperties, deviceExtensionProperties))
        {
            results.fail("Extension " + string(dependency.first) + " is missing dependency");
        }
    }
}

#endif // CTS_USES_VULKANSC

tcu::TestStatus enumerateInstanceLayers(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);
    const vector<VkLayerProperties> properties = enumerateInstanceLayerProperties(context.getPlatformInterface());
    vector<string> layerNames;

    for (size_t ndx = 0; ndx < properties.size(); ndx++)
    {
        log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

        layerNames.push_back(properties[ndx].layerName);
    }

    checkDuplicateLayers(results, layerNames);
    CheckEnumerateInstanceLayerPropertiesIncompleteResult()(context, results, layerNames.size());

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumerateInstanceExtensions(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    {
        const ScopedLogSection section(log, "Global", "Global Extensions");
        const vector<VkExtensionProperties> properties =
            enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr);
        const vector<VkExtensionProperties> unused;
        vector<string> extensionNames;

        for (size_t ndx = 0; ndx < properties.size(); ndx++)
        {
            log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

            extensionNames.push_back(properties[ndx].extensionName);
        }

        checkInstanceExtensions(results, extensionNames);
        CheckEnumerateInstanceExtensionPropertiesIncompleteResult()(context, results, properties.size());

#ifndef CTS_USES_VULKANSC
        for (const auto &version : releasedApiVersions)
        {
            uint32_t apiVariant, versionMajor, versionMinor;
            std::tie(std::ignore, apiVariant, versionMajor, versionMinor) = version;
            if (context.contextSupports(vk::ApiVersion(apiVariant, versionMajor, versionMinor, 0)))
            {
                checkExtensionDependencies(results, instanceExtensionDependencies, versionMajor, versionMinor,
                                           properties, unused);
                break;
            }
        }
#endif // CTS_USES_VULKANSC
    }

    {
        const vector<VkLayerProperties> layers = enumerateInstanceLayerProperties(context.getPlatformInterface());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
        {
            const ScopedLogSection section(log, layer->layerName, string("Layer: ") + layer->layerName);
            const vector<VkExtensionProperties> properties =
                enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName);
            vector<string> extensionNames;

            for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
            {
                log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;

                extensionNames.push_back(properties[extNdx].extensionName);
            }

            checkInstanceExtensions(results, extensionNames);
            CheckEnumerateInstanceExtensionPropertiesIncompleteResult(layer->layerName)(context, results,
                                                                                        properties.size());
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus validateDeviceLevelEntryPointsFromInstanceExtensions(Context &context)
{

#include "vkEntryPointValidation.inl"

    TestLog &log(context.getTestContext().getLog());
    tcu::ResultCollector results(log);
    const DeviceInterface &vk(context.getDeviceInterface());
    const VkDevice device(context.getDevice());

    for (const auto &keyValue : instExtDeviceFun)
    {
        const std::string &extensionName = keyValue.first;
        if (!context.isInstanceFunctionalitySupported(extensionName))
            continue;

        for (const auto &deviceEntryPoint : keyValue.second)
        {
            if (!vk.getDeviceProcAddr(device, deviceEntryPoint.c_str()))
                results.fail("Missing " + deviceEntryPoint);
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testNoKhxExtensions(Context &context)
{
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const PlatformInterface &vkp    = context.getPlatformInterface();
    const InstanceInterface &vki    = context.getInstanceInterface();

    tcu::ResultCollector results(context.getTestContext().getLog());
    bool testSucceeded = true;
    uint32_t instanceExtensionsCount;
    uint32_t deviceExtensionsCount;

    // grab number of instance and device extensions
    vkp.enumerateInstanceExtensionProperties(nullptr, &instanceExtensionsCount, nullptr);
    vki.enumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionsCount, nullptr);
    vector<VkExtensionProperties> extensionsProperties(instanceExtensionsCount + deviceExtensionsCount);

    // grab instance and device extensions into single vector
    if (instanceExtensionsCount)
        vkp.enumerateInstanceExtensionProperties(nullptr, &instanceExtensionsCount, &extensionsProperties[0]);
    if (deviceExtensionsCount)
        vki.enumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionsCount,
                                               &extensionsProperties[instanceExtensionsCount]);

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

tcu::TestStatus enumerateDeviceLayers(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);
    const vector<VkLayerProperties> properties =
        enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());
    vector<string> layerNames;

    for (size_t ndx = 0; ndx < properties.size(); ndx++)
    {
        log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

        layerNames.push_back(properties[ndx].layerName);
    }

    checkDuplicateLayers(results, layerNames);
    CheckEnumerateDeviceLayerPropertiesIncompleteResult()(context, results, layerNames.size());

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus enumerateDeviceExtensions(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    {
        const ScopedLogSection section(log, "Global", "Global Extensions");
        const vector<VkExtensionProperties> instanceExtensionProperties =
            enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr);
        const vector<VkExtensionProperties> deviceExtensionProperties =
            enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), nullptr);
        vector<string> deviceExtensionNames;

        for (size_t ndx = 0; ndx < deviceExtensionProperties.size(); ndx++)
        {
            log << TestLog::Message << ndx << ": " << deviceExtensionProperties[ndx] << TestLog::EndMessage;

            deviceExtensionNames.push_back(deviceExtensionProperties[ndx].extensionName);
        }

        checkDeviceExtensions(results, deviceExtensionNames);
        CheckEnumerateDeviceExtensionPropertiesIncompleteResult()(context, results, deviceExtensionProperties.size());

#ifndef CTS_USES_VULKANSC
        for (const auto &version : releasedApiVersions)
        {
            uint32_t apiVariant, versionMajor, versionMinor;
            std::tie(std::ignore, apiVariant, versionMajor, versionMinor) = version;
            if (context.contextSupports(vk::ApiVersion(apiVariant, versionMajor, versionMinor, 0)))
            {
                checkExtensionDependencies(results, deviceExtensionDependencies, versionMajor, versionMinor,
                                           instanceExtensionProperties, deviceExtensionProperties);
                break;
            }
        }
#endif // CTS_USES_VULKANSC
    }

    {
        const vector<VkLayerProperties> layers =
            enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
        {
            const ScopedLogSection section(log, layer->layerName, string("Layer: ") + layer->layerName);
            const vector<VkExtensionProperties> properties = enumerateDeviceExtensionProperties(
                context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName);
            vector<string> extensionNames;

            for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
            {
                log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;

                extensionNames.push_back(properties[extNdx].extensionName);
            }

            checkDeviceExtensions(results, extensionNames);
            CheckEnumerateDeviceExtensionPropertiesIncompleteResult(layer->layerName)(context, results,
                                                                                      properties.size());
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus extensionCoreVersions(Context &context)
{
    uint32_t major;
    uint32_t minor;
    const char *extName;

    auto &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    const auto instanceExtensionProperties =
        enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr);
    const auto deviceExtensionProperties =
        enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), nullptr);

    for (const auto &majorMinorName : extensionRequiredCoreVersion)
    {
        std::tie(major, minor, extName) = majorMinorName;
        const RequiredExtension reqExt(extName);

        if ((isExtensionStructSupported(instanceExtensionProperties, reqExt) ||
             isExtensionStructSupported(deviceExtensionProperties, reqExt)) &&
            !context.contextSupports(vk::ApiVersion(0u, major, minor, 0u)))
        {
            results.fail("Required core version for " + std::string(extName) + " not met (" + de::toString(major) +
                         "." + de::toString(minor) + ")");
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

#define VK_SIZE_OF(STRUCT, MEMBER) (sizeof(((STRUCT *)0)->MEMBER))
#define OFFSET_TABLE_ENTRY(STRUCT, MEMBER)                            \
    {                                                                 \
        (size_t) offsetof(STRUCT, MEMBER), VK_SIZE_OF(STRUCT, MEMBER) \
    }

tcu::TestStatus deviceFeatures(Context &context)
{
    using namespace ValidateQueryBits;

    TestLog &log = context.getTestContext().getLog();
    VkPhysicalDeviceFeatures *features;
    uint8_t buffer[sizeof(VkPhysicalDeviceFeatures) + GUARD_SIZE];

    const QueryMemberTableEntry featureOffsetTable[] = {
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
        {0, 0}};

    deMemset(buffer, GUARD_VALUE, sizeof(buffer));
    features = reinterpret_cast<VkPhysicalDeviceFeatures *>(buffer);

    context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), features);

    log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage << TestLog::Message
        << *features << TestLog::EndMessage;

    // Requirements and dependencies
    {
        if (!features->robustBufferAccess)
            return tcu::TestStatus::fail("robustBufferAccess is not supported");
    }

    for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
    {
        if (buffer[ndx + sizeof(VkPhysicalDeviceFeatures)] != GUARD_VALUE)
        {
            log << TestLog::Message << "deviceFeatures - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceFeatures buffer overflow");
        }
    }

    if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceFeatures,
                              context.getInstanceInterface(), featureOffsetTable))
    {
        log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceFeatures not completely initialized"
            << TestLog::EndMessage;
        return tcu::TestStatus::fail("deviceFeatures incomplete initialization");
    }

    return tcu::TestStatus::pass("Query succeeded");
}

static const ValidateQueryBits::QueryMemberTableEntry s_physicalDevicePropertiesOffsetTable[] = {
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
    {0, 0}};

tcu::TestStatus deviceProperties(Context &context)
{
    using namespace ValidateQueryBits;

    TestLog &log = context.getTestContext().getLog();
    VkPhysicalDeviceProperties *props;
    VkPhysicalDeviceFeatures features;
    uint8_t buffer[sizeof(VkPhysicalDeviceProperties) + GUARD_SIZE];

    props = reinterpret_cast<VkPhysicalDeviceProperties *>(buffer);
    deMemset(props, GUARD_VALUE, sizeof(buffer));

    context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), props);
    context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

    log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage << TestLog::Message
        << *props << TestLog::EndMessage;

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

    if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceProperties,
                              context.getInstanceInterface(), s_physicalDevicePropertiesOffsetTable))
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceProperties not completely initialized"
            << TestLog::EndMessage;
        return tcu::TestStatus::fail("deviceProperties incomplete initialization");
    }

    // Check if deviceName string is properly terminated.
    if (memchr(props->deviceName, '\0', VK_MAX_PHYSICAL_DEVICE_NAME_SIZE) == nullptr)
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceProperties deviceName not properly initialized"
            << TestLog::EndMessage;
        return tcu::TestStatus::fail("deviceProperties incomplete initialization");
    }

    {
        const ApiVersion deviceVersion = unpackVersion(props->apiVersion);
#ifndef CTS_USES_VULKANSC
        const ApiVersion deqpVersion = unpackVersion(VK_API_MAX_FRAMEWORK_VERSION);
#else
        const ApiVersion deqpVersion = unpackVersion(VK_API_VERSION_1_2);
#endif // CTS_USES_VULKANSC

        if (deviceVersion.majorNum != deqpVersion.majorNum)
        {
            log << TestLog::Message << "deviceProperties - API Major Version " << deviceVersion.majorNum
                << " is not valid" << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceProperties apiVersion not valid");
        }

        if (deviceVersion.minorNum > deqpVersion.minorNum)
        {
            log << TestLog::Message << "deviceProperties - API Minor Version " << deviceVersion.minorNum
                << " is not valid for this version of dEQP" << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceProperties apiVersion not valid");
        }
    }

    return tcu::TestStatus::pass("DeviceProperites query succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    const vector<VkQueueFamilyProperties> queueProperties =
        getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

    log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

    for (size_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
        log << TestLog::Message << queueNdx << ": " << queueProperties[queueNdx] << TestLog::EndMessage;

    return tcu::TestStatus::pass("Querying queue properties succeeded");
}

tcu::TestStatus deviceMemoryProperties(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    VkPhysicalDeviceMemoryProperties *memProps;
    uint8_t buffer[sizeof(VkPhysicalDeviceMemoryProperties) + GUARD_SIZE];

    memProps = reinterpret_cast<VkPhysicalDeviceMemoryProperties *>(buffer);
    deMemset(buffer, GUARD_VALUE, sizeof(buffer));

    context.getInstanceInterface().getPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), memProps);

    log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage << TestLog::Message
        << *memProps << TestLog::EndMessage;

    for (int32_t ndx = 0; ndx < GUARD_SIZE; ndx++)
    {
        if (buffer[ndx + sizeof(VkPhysicalDeviceMemoryProperties)] != GUARD_VALUE)
        {
            log << TestLog::Message << "deviceMemoryProperties - Guard offset " << ndx << " not valid"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryProperties buffer overflow");
        }
    }

    if (memProps->memoryHeapCount >= VK_MAX_MEMORY_HEAPS)
    {
        log << TestLog::Message << "deviceMemoryProperties - HeapCount larger than " << (uint32_t)VK_MAX_MEMORY_HEAPS
            << TestLog::EndMessage;
        return tcu::TestStatus::fail("deviceMemoryProperties HeapCount too large");
    }

    if (memProps->memoryHeapCount == 1)
    {
        if ((memProps->memoryHeaps[0].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
        {
            log << TestLog::Message << "deviceMemoryProperties - Single heap is not marked DEVICE_LOCAL"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryProperties invalid HeapFlags");
        }
    }

    const VkMemoryPropertyFlags validPropertyFlags[] = {
        0,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT};

    const VkMemoryPropertyFlags requiredPropertyFlags[] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    bool requiredFlagsFound[DE_LENGTH_OF_ARRAY(requiredPropertyFlags)];
    std::fill(DE_ARRAY_BEGIN(requiredFlagsFound), DE_ARRAY_END(requiredFlagsFound), false);

    for (uint32_t memoryNdx = 0; memoryNdx < memProps->memoryTypeCount; memoryNdx++)
    {
        bool validPropTypeFound = false;

        if (memProps->memoryTypes[memoryNdx].heapIndex >= memProps->memoryHeapCount)
        {
            log << TestLog::Message << "deviceMemoryProperties - heapIndex "
                << memProps->memoryTypes[memoryNdx].heapIndex << " larger than heapCount" << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryProperties - invalid heapIndex");
        }

        const VkMemoryPropertyFlags bitsToCheck =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
            VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

        for (const VkMemoryPropertyFlags *requiredFlagsIterator = DE_ARRAY_BEGIN(requiredPropertyFlags);
             requiredFlagsIterator != DE_ARRAY_END(requiredPropertyFlags); requiredFlagsIterator++)
            if ((memProps->memoryTypes[memoryNdx].propertyFlags & *requiredFlagsIterator) == *requiredFlagsIterator)
                requiredFlagsFound[requiredFlagsIterator - DE_ARRAY_BEGIN(requiredPropertyFlags)] = true;

        if (de::contains(DE_ARRAY_BEGIN(validPropertyFlags), DE_ARRAY_END(validPropertyFlags),
                         memProps->memoryTypes[memoryNdx].propertyFlags & bitsToCheck))
            validPropTypeFound = true;

        if (!validPropTypeFound)
        {
            log << TestLog::Message << "deviceMemoryProperties - propertyFlags "
                << memProps->memoryTypes[memoryNdx].propertyFlags << " not valid" << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryProperties propertyFlags not valid");
        }

        if (memProps->memoryTypes[memoryNdx].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            if ((memProps->memoryHeaps[memProps->memoryTypes[memoryNdx].heapIndex].flags &
                 VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
            {
                log << TestLog::Message
                    << "deviceMemoryProperties - DEVICE_LOCAL memory type references heap which is not DEVICE_LOCAL"
                    << TestLog::EndMessage;
                return tcu::TestStatus::fail("deviceMemoryProperties inconsistent memoryType and HeapFlags");
            }
        }
        else
        {
            if (memProps->memoryHeaps[memProps->memoryTypes[memoryNdx].heapIndex].flags &
                VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                log << TestLog::Message
                    << "deviceMemoryProperties - non-DEVICE_LOCAL memory type references heap with is DEVICE_LOCAL"
                    << TestLog::EndMessage;
                return tcu::TestStatus::fail("deviceMemoryProperties inconsistent memoryType and HeapFlags");
            }
        }
    }

    bool *requiredFlagsFoundIterator =
        std::find(DE_ARRAY_BEGIN(requiredFlagsFound), DE_ARRAY_END(requiredFlagsFound), false);
    if (requiredFlagsFoundIterator != DE_ARRAY_END(requiredFlagsFound))
    {
        DE_ASSERT(requiredFlagsFoundIterator - DE_ARRAY_BEGIN(requiredFlagsFound) <=
                  DE_LENGTH_OF_ARRAY(requiredPropertyFlags));
        log << TestLog::Message << "deviceMemoryProperties - required property flags "
            << getMemoryPropertyFlagsStr(
                   requiredPropertyFlags[requiredFlagsFoundIterator - DE_ARRAY_BEGIN(requiredFlagsFound)])
            << " not found" << TestLog::EndMessage;

        return tcu::TestStatus::fail("deviceMemoryProperties propertyFlags not valid");
    }

    return tcu::TestStatus::pass("Querying memory properties succeeded");
}

tcu::TestStatus deviceGroupPeerMemoryFeatures(Context &context)
{
    TestLog &log                    = context.getTestContext().getLog();
    const PlatformInterface &vkp    = context.getPlatformInterface();
    const VkInstance instance       = context.getInstance(); // "VK_KHR_device_group_creation"
    const InstanceInterface &vki    = context.getInstanceInterface();
    const tcu::CommandLine &cmdLine = context.getTestContext().getCommandLine();
    const uint32_t devGroupIdx      = cmdLine.getVKDeviceGroupId() - 1;
    const uint32_t deviceIdx        = vk::chooseDeviceIndex(vki, instance, cmdLine);
    const float queuePriority       = 1.0f;
    const char *deviceGroupExtName  = "VK_KHR_device_group";
    VkPhysicalDeviceMemoryProperties memProps;
    VkPeerMemoryFeatureFlags *peerMemFeatures;
    uint8_t buffer[sizeof(VkPeerMemoryFeatureFlags) + GUARD_SIZE];
    uint32_t queueFamilyIndex = 0;

    const vector<VkPhysicalDeviceGroupProperties> deviceGroupProps = enumeratePhysicalDeviceGroups(vki, instance);
    std::vector<const char *> deviceExtensions;

    if (static_cast<size_t>(devGroupIdx) >= deviceGroupProps.size())
    {
        std::ostringstream msg;
        msg << "Chosen device group index " << devGroupIdx << " too big: found " << deviceGroupProps.size()
            << " device groups";
        TCU_THROW(NotSupportedError, msg.str());
    }

    const auto numPhysicalDevices = deviceGroupProps[devGroupIdx].physicalDeviceCount;

    if (deviceIdx >= numPhysicalDevices)
    {
        std::ostringstream msg;
        msg << "Chosen device index " << deviceIdx << " too big: chosen device group " << devGroupIdx << " has "
            << numPhysicalDevices << " devices";
        TCU_THROW(NotSupportedError, msg.str());
    }

    // Need at least 2 devices for peer memory features.
    if (numPhysicalDevices < 2)
        TCU_THROW(NotSupportedError, "Need a device group with at least 2 physical devices");

    if (!isCoreDeviceExtension(context.getUsedApiVersion(), deviceGroupExtName))
        deviceExtensions.push_back(deviceGroupExtName);

    const std::vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(vki, deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx]);
    for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
    {
        if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamilyIndex = (uint32_t)queueNdx;
    }
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //type
        nullptr,                                    //pNext
        (VkDeviceQueueCreateFlags)0u,               //flags
        queueFamilyIndex,                           //queueFamilyIndex;
        1u,                                         //queueCount;
        &queuePriority,                             //pQueuePriorities;
    };

    // Create device groups
    VkDeviceGroupDeviceCreateInfo deviceGroupInfo = {
        VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO, //stype
        nullptr,                                           //pNext
        deviceGroupProps[devGroupIdx].physicalDeviceCount, //physicalDeviceCount
        deviceGroupProps[devGroupIdx].physicalDevices      //physicalDevices
    };

    void *pNext = &deviceGroupInfo;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (context.getTestContext().getCommandLine().isSubProcess())
    {
        if (context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                context.getResourceInterface()->getCacheDataSize(),       // uintptr_t initialDataSize;
                context.getResourceInterface()->getCacheData()            // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
        pNext,                                //pNext;
        (VkDeviceCreateFlags)0u,              //flags
        1,                                    //queueRecordCount;
        &deviceQueueCreateInfo,               //pRequestedQueues;
        0,                                    //layerCount;
        nullptr,                              //ppEnabledLayerNames;
        uint32_t(deviceExtensions.size()),    //extensionCount;
        de::dataOrNull(deviceExtensions),     //ppEnabledExtensionNames;
        nullptr,                              //pEnabledFeatures;
    };

    Move<VkDevice> deviceGroup =
        createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki,
                           deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &deviceCreateInfo);
    const DeviceDriver vk(vkp, instance, *deviceGroup, context.getUsedApiVersion(),
                          context.getTestContext().getCommandLine());
    context.getInstanceInterface().getPhysicalDeviceMemoryProperties(
        deviceGroupProps[devGroupIdx].physicalDevices[deviceIdx], &memProps);

    peerMemFeatures = reinterpret_cast<VkPeerMemoryFeatureFlags *>(buffer);
    deMemset(buffer, GUARD_VALUE, sizeof(buffer));

    for (uint32_t heapIndex = 0; heapIndex < memProps.memoryHeapCount; heapIndex++)
    {
        for (uint32_t localDeviceIndex = 0; localDeviceIndex < numPhysicalDevices; localDeviceIndex++)
        {
            for (uint32_t remoteDeviceIndex = 0; remoteDeviceIndex < numPhysicalDevices; remoteDeviceIndex++)
            {
                if (localDeviceIndex != remoteDeviceIndex)
                {
                    vk.getDeviceGroupPeerMemoryFeatures(deviceGroup.get(), heapIndex, localDeviceIndex,
                                                        remoteDeviceIndex, peerMemFeatures);

                    // Check guard
                    for (int32_t ndx = 0; ndx < GUARD_SIZE; ndx++)
                    {
                        if (buffer[ndx + sizeof(VkPeerMemoryFeatureFlags)] != GUARD_VALUE)
                        {
                            log << TestLog::Message << "deviceGroupPeerMemoryFeatures - Guard offset " << ndx
                                << " not valid" << TestLog::EndMessage;
                            return tcu::TestStatus::fail("deviceGroupPeerMemoryFeatures buffer overflow");
                        }
                    }

                    VkPeerMemoryFeatureFlags requiredFlag = VK_PEER_MEMORY_FEATURE_COPY_DST_BIT;
                    VkPeerMemoryFeatureFlags maxValidFlag =
                        VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT | VK_PEER_MEMORY_FEATURE_COPY_DST_BIT |
                        VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT | VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT;
                    if ((!(*peerMemFeatures & requiredFlag)) || *peerMemFeatures > maxValidFlag)
                        return tcu::TestStatus::fail("deviceGroupPeerMemoryFeatures invalid flag");

                    log << TestLog::Message << "deviceGroup = " << deviceGroup.get() << TestLog::EndMessage
                        << TestLog::Message << "heapIndex = " << heapIndex << TestLog::EndMessage << TestLog::Message
                        << "localDeviceIndex = " << localDeviceIndex << TestLog::EndMessage << TestLog::Message
                        << "remoteDeviceIndex = " << remoteDeviceIndex << TestLog::EndMessage << TestLog::Message
                        << "PeerMemoryFeatureFlags = " << *peerMemFeatures << TestLog::EndMessage;
                }
            } // remote device
        }     // local device
    }         // heap Index

    return tcu::TestStatus::pass("Querying deviceGroup peer memory features succeeded");
}

tcu::TestStatus deviceMemoryBudgetProperties(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    uint8_t buffer[sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT) + GUARD_SIZE];

    if (!context.isDeviceFunctionalitySupported("VK_EXT_memory_budget"))
        TCU_THROW(NotSupportedError, "VK_EXT_memory_budget is not supported");

    VkPhysicalDeviceMemoryBudgetPropertiesEXT *budgetProps =
        reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT *>(buffer);
    deMemset(buffer, GUARD_VALUE, sizeof(buffer));

    budgetProps->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    budgetProps->pNext = nullptr;

    VkPhysicalDeviceMemoryProperties2 memProps;
    deMemset(&memProps, 0, sizeof(memProps));
    memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProps.pNext = budgetProps;

    context.getInstanceInterface().getPhysicalDeviceMemoryProperties2(context.getPhysicalDevice(), &memProps);

    log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage << TestLog::Message
        << *budgetProps << TestLog::EndMessage;

    for (int32_t ndx = 0; ndx < GUARD_SIZE; ndx++)
    {
        if (buffer[ndx + sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT)] != GUARD_VALUE)
        {
            log << TestLog::Message << "deviceMemoryBudgetProperties - Guard offset " << ndx << " not valid"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryBudgetProperties buffer overflow");
        }
    }

    for (uint32_t i = 0; i < memProps.memoryProperties.memoryHeapCount; ++i)
    {
        if (budgetProps->heapBudget[i] == 0)
        {
            log << TestLog::Message << "deviceMemoryBudgetProperties - Supported heaps must report nonzero budget"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid heap budget (zero)");
        }
        if (budgetProps->heapBudget[i] > memProps.memoryProperties.memoryHeaps[i].size)
        {
            log << TestLog::Message
                << "deviceMemoryBudgetProperties - Heap budget must be less than or equal to heap size"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid heap budget (too large)");
        }
    }

    for (uint32_t i = memProps.memoryProperties.memoryHeapCount; i < VK_MAX_MEMORY_HEAPS; ++i)
    {
        if (budgetProps->heapBudget[i] != 0 || budgetProps->heapUsage[i] != 0)
        {
            log << TestLog::Message << "deviceMemoryBudgetProperties - Unused heaps must report budget/usage of zero"
                << TestLog::EndMessage;
            return tcu::TestStatus::fail("deviceMemoryBudgetProperties invalid unused heaps");
        }
    }

    return tcu::TestStatus::pass("Querying memory budget properties succeeded");
}

namespace
{

#include "vkMandatoryFeatures.inl"

}

tcu::TestStatus deviceMandatoryFeatures(Context &context)
{
    bool result = checkBasicMandatoryFeatures(context);

#if defined(CTS_USES_VULKAN)
    // for vulkan 1.4+ we need to check complex cases that were not generated in vkMandatoryFeatures.inl
    if (context.contextSupports(vk::ApiVersion(0, 1, 4, 0)))
    {
        const InstanceInterface &vki    = context.getInstanceInterface();
        VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
        const auto &cmdLine             = context.getTestContext().getCommandLine();
        const auto &vulkan14Features    = context.getDeviceVulkan14Features();
        tcu::TestLog &log               = context.getTestContext().getLog();

        if (!cmdLine.isComputeOnly() && (vulkan14Features.hostImageCopy == VK_FALSE))
        {
            // find graphics and transfer queues
            std::optional<size_t> graphicsQueueNdx;
            std::optional<size_t> transferQueueNdx;
            const auto queuePropsVect = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
            for (size_t queueNdx = 0; queueNdx < queuePropsVect.size(); queueNdx++)
            {
                uint32_t queueFlags = queuePropsVect[queueNdx].queueFlags;
                if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) && !graphicsQueueNdx.has_value())
                    graphicsQueueNdx = (uint32_t)queueNdx;
                else if ((queueFlags & VK_QUEUE_TRANSFER_BIT) && !transferQueueNdx.has_value())
                    transferQueueNdx = (uint32_t)queueNdx;
            }

            if (!graphicsQueueNdx.has_value() || !transferQueueNdx.has_value())
            {
                log << tcu::TestLog::Message
                    << "Implementation that has a VK_QUEUE_GRAPHICS_BIT queue must support "
                       "either the hostImageCopy feature or an additional queue that supports VK_QUEUE_TRANSFER_BIT"
                    << tcu::TestLog::EndMessage;
                result = false;
            }
        }
    }
#endif // defined(CTS_USES_VULKAN)

    if (result)
        return tcu::TestStatus::pass("Passed");

    return tcu::TestStatus::fail("Not all mandatory features are supported ( see: vkspec.html#features-requirements )");
}

VkFormatFeatureFlags getBaseRequiredOptimalTilingFeatures(VkFormat format)
{
    struct Formatpair
    {
        VkFormat format;
        VkFormatFeatureFlags flags;
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

    static const Formatpair formatflags[] = {
        {VK_FORMAT_B4G4R4A4_UNORM_PACK16, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_R5G6B5_UNORM_PACK16, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_A1R5G5B5_UNORM_PACK16, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_R8_UNORM, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_R8_SNORM, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_R8_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R8_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R8G8_UNORM, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_R8G8_SNORM, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_R8G8_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R8G8_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R8G8B8A8_UNORM, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | STIM | CABL},
        {VK_FORMAT_R8G8B8A8_SNORM, SAIM | BLSR | TRSR | TRDS | SIFL | STIM},
        {VK_FORMAT_R8G8B8A8_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R8G8B8A8_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R8G8B8A8_SRGB, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_B8G8R8A8_UNORM, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_B8G8R8A8_SRGB, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_A8B8G8R8_UNORM_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_A8B8G8R8_SNORM_PACK32, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_A8B8G8R8_UINT_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_A8B8G8R8_SINT_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_A2B10G10R10_UINT_PACK32, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R16_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R16_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R16_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_R16G16_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R16G16_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS},
        {VK_FORMAT_R16G16_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | CABL},
        {VK_FORMAT_R16G16B16A16_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R16G16B16A16_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R16G16B16A16_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | SIFL | STIM | CABL},
        {VK_FORMAT_R32_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM | STIA},
        {VK_FORMAT_R32_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM | STIA},
        {VK_FORMAT_R32_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32B32A32_UINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32B32A32_SINT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_R32G32B32A32_SFLOAT, SAIM | BLSR | TRSR | TRDS | COAT | BLDS | STIM},
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, SAIM | BLSR | TRSR | TRDS | SIFL},
        {VK_FORMAT_D16_UNORM, SAIM | BLSR | TRSR | TRDS | DSAT},
    };

    size_t formatpairs = sizeof(formatflags) / sizeof(Formatpair);

    for (unsigned int i = 0; i < formatpairs; i++)
        if (formatflags[i].format == format)
            return formatflags[i].flags;
    return 0;
}

VkFormatFeatureFlags getRequiredOptimalExtendedTilingFeatures(Context &context, VkFormat format,
                                                              VkFormatFeatureFlags queriedFlags)
{
    VkFormatFeatureFlags flags = (VkFormatFeatureFlags)0;

    // VK_EXT_sampler_filter_minmax:
    //    If filterMinmaxSingleComponentFormats is VK_TRUE, the following formats must
    //    support the VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT feature with
    //    VK_IMAGE_TILING_OPTIMAL, if they support VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT.

    static const VkFormat s_requiredSampledImageFilterMinMaxFormats[] = {
        VK_FORMAT_R8_UNORM,   VK_FORMAT_R8_SNORM,          VK_FORMAT_R16_UNORM,         VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_SFLOAT, VK_FORMAT_R32_SFLOAT,        VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32,
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if ((queriedFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
    {
        if (de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(),
                         "VK_EXT_sampler_filter_minmax"))
        {
            if (de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterMinMaxFormats),
                             DE_ARRAY_END(s_requiredSampledImageFilterMinMaxFormats), format))
            {
                VkPhysicalDeviceSamplerFilterMinmaxProperties physicalDeviceSamplerMinMaxProperties = {
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES, nullptr, false, false};

                {
                    VkPhysicalDeviceProperties2 physicalDeviceProperties;
                    physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                    physicalDeviceProperties.pNext = &physicalDeviceSamplerMinMaxProperties;

                    const InstanceInterface &vk = context.getInstanceInterface();
                    vk.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &physicalDeviceProperties);
                }

                if (physicalDeviceSamplerMinMaxProperties.filterMinmaxSingleComponentFormats)
                {
                    flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
                }
            }
        }
    }

    // VK_EXT_filter_cubic:
    // If cubic filtering is supported, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT must be supported for the following image view types:
    // VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY
    static const VkFormat s_requiredSampledImageFilterCubicFormats[] = {VK_FORMAT_R4G4_UNORM_PACK8,
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
                                                                        VK_FORMAT_A8B8G8R8_SRGB_PACK32};

    static const VkFormat s_requiredSampledImageFilterCubicFormatsETC2[] = {
        VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,  VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,    VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK};

    if ((queriedFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 &&
        de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_EXT_filter_cubic"))
    {
        if (de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterCubicFormats),
                         DE_ARRAY_END(s_requiredSampledImageFilterCubicFormats), format))
            flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT;

        const auto &coreFeatures = context.getDeviceFeatures();
        if (coreFeatures.textureCompressionETC2 &&
            de::contains(DE_ARRAY_BEGIN(s_requiredSampledImageFilterCubicFormatsETC2),
                         DE_ARRAY_END(s_requiredSampledImageFilterCubicFormatsETC2), format))
            flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT;
    }

    return flags;
}

VkFormatFeatureFlags getRequiredBufferFeatures(VkFormat format)
{
    static const VkFormat s_requiredVertexBufferFormats[]             = {VK_FORMAT_R8_UNORM,
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
                                                                         VK_FORMAT_R32G32B32A32_SFLOAT};
    static const VkFormat s_requiredUniformTexelBufferFormats[]       = {VK_FORMAT_R8_UNORM,
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
                                                                         VK_FORMAT_B10G11R11_UFLOAT_PACK32};
    static const VkFormat s_requiredStorageTexelBufferFormats[]       = {VK_FORMAT_R8G8B8A8_UNORM,
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
                                                                         VK_FORMAT_R32G32B32A32_SFLOAT};
    static const VkFormat s_requiredStorageTexelBufferAtomicFormats[] = {VK_FORMAT_R32_UINT, VK_FORMAT_R32_SINT};

    VkFormatFeatureFlags flags = (VkFormatFeatureFlags)0;

    if (de::contains(DE_ARRAY_BEGIN(s_requiredVertexBufferFormats), DE_ARRAY_END(s_requiredVertexBufferFormats),
                     format))
        flags |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;

    if (de::contains(DE_ARRAY_BEGIN(s_requiredUniformTexelBufferFormats),
                     DE_ARRAY_END(s_requiredUniformTexelBufferFormats), format))
        flags |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;

    if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageTexelBufferFormats),
                     DE_ARRAY_END(s_requiredStorageTexelBufferFormats), format))
        flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;

    if (de::contains(DE_ARRAY_BEGIN(s_requiredStorageTexelBufferAtomicFormats),
                     DE_ARRAY_END(s_requiredStorageTexelBufferAtomicFormats), format))
        flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;

    return flags;
}

VkPhysicalDeviceSamplerYcbcrConversionFeatures getPhysicalDeviceSamplerYcbcrConversionFeatures(
    const InstanceInterface &vk, VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceFeatures2 coreFeatures;
    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures;

    deMemset(&coreFeatures, 0, sizeof(coreFeatures));
    deMemset(&ycbcrFeatures, 0, sizeof(ycbcrFeatures));

    coreFeatures.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    coreFeatures.pNext  = &ycbcrFeatures;
    ycbcrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;

    vk.getPhysicalDeviceFeatures2(physicalDevice, &coreFeatures);

    return ycbcrFeatures;
}

void checkYcbcrApiSupport(Context &context)
{
    // check if YCbcr API and are supported by implementation

    // the support for formats and YCbCr may still be optional - see isYcbcrConversionSupported below

    if (!vk::isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_sampler_ycbcr_conversion"))
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion"))
            TCU_THROW(NotSupportedError, "VK_KHR_sampler_ycbcr_conversion is not supported");

        // Hard dependency for ycbcr
        TCU_CHECK(de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(),
                               "VK_KHR_get_physical_device_properties2"));
    }
}

bool isYcbcrConversionSupported(Context &context)
{
    checkYcbcrApiSupport(context);

    const VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures =
        getPhysicalDeviceSamplerYcbcrConversionFeatures(context.getInstanceInterface(), context.getPhysicalDevice());

    return (ycbcrFeatures.samplerYcbcrConversion == VK_TRUE);
}

VkFormatFeatureFlags getRequiredYcbcrFormatFeatures(Context &context, VkFormat format)
{
    bool req = isYcbcrConversionSupported(context) &&
               (format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM || format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);

    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                          VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                          VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
    return req ? required : (VkFormatFeatureFlags)0;
}

VkFormatFeatureFlags getRequiredOptimalTilingFeatures(Context &context, VkFormat format)
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
            ret |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        return ret;
    }
}

bool requiresYCbCrConversion(Context &context, VkFormat format)
{
#ifndef CTS_USES_VULKANSC
    if (format == VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)
    {
        if (!context.isDeviceFunctionalitySupported("VK_EXT_rgba10x6_formats"))
            return true;
        VkPhysicalDeviceFeatures2 coreFeatures;
        VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT rgba10x6features;

        deMemset(&coreFeatures, 0, sizeof(coreFeatures));
        deMemset(&rgba10x6features, 0, sizeof(rgba10x6features));

        coreFeatures.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        coreFeatures.pNext     = &rgba10x6features;
        rgba10x6features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT;

        const InstanceInterface &vk = context.getInstanceInterface();
        vk.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);

        return !rgba10x6features.formatRgba10x6WithoutYCbCrSampler;
    }
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC

    return isYCbCrFormat(format) && format != VK_FORMAT_R10X6_UNORM_PACK16 &&
           format != VK_FORMAT_R10X6G10X6_UNORM_2PACK16 && format != VK_FORMAT_R12X4_UNORM_PACK16 &&
           format != VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
}

VkFormatFeatureFlags getAllowedOptimalTilingFeatures(Context &context, VkFormat format)
{

    VkFormatFeatureFlags vulkanOnlyFeatureFlags = 0;
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME))
        vulkanOnlyFeatureFlags |=
            VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR | VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR;
    if (context.isDeviceFunctionalitySupported(VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME))
        vulkanOnlyFeatureFlags |=
            VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR | VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR;
#endif

    // YCbCr formats only support a subset of format feature flags
    const VkFormatFeatureFlags ycbcrAllows =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
        VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT | VK_FORMAT_FEATURE_DISJOINT_BIT | vulkanOnlyFeatureFlags;

    // By default everything is allowed.
    VkFormatFeatureFlags allow = (VkFormatFeatureFlags)~0u;
    // Formats for which SamplerYCbCrConversion is required may not support certain features.
    if (requiresYCbCrConversion(context, format))
        allow &= ycbcrAllows;
    // single-plane formats *may not* support DISJOINT_BIT
    if (!isYCbCrFormat(format) || getPlaneCount(format) == 1)
        allow &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;

    return allow;
}

VkFormatFeatureFlags getAllowedBufferFeatures(Context &context, VkFormat format)
{
    // TODO: Do we allow non-buffer flags in the bufferFeatures?
    return requiresYCbCrConversion(context, format) ? (VkFormatFeatureFlags)0 :
                                                      (VkFormatFeatureFlags)(~VK_FORMAT_FEATURE_DISJOINT_BIT);
}

tcu::TestStatus formatProperties(Context &context, VkFormat format)
{
    // check if Ycbcr format enums are valid given the version and extensions
    if (isYCbCrFormat(format))
        checkYcbcrApiSupport(context);

    TestLog &log = context.getTestContext().getLog();
    const VkFormatProperties properties =
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
    const bool apiVersion10WithoutKhrMaintenance1 =
        isApiVersionEqual(context.getUsedApiVersion(), VK_API_VERSION_1_0) &&
        !context.isDeviceFunctionalitySupported("VK_KHR_maintenance1");

    const VkFormatFeatureFlags reqImg   = getRequiredOptimalTilingFeatures(context, format);
    const VkFormatFeatureFlags reqBuf   = getRequiredBufferFeatures(format);
    const VkFormatFeatureFlags allowImg = getAllowedOptimalTilingFeatures(context, format);
    const VkFormatFeatureFlags allowBuf = getAllowedBufferFeatures(context, format);
    tcu::ResultCollector results(log, "ERROR: ");

    const struct feature_req
    {
        const char *fieldName;
        VkFormatFeatureFlags supportedFeatures;
        VkFormatFeatureFlags requiredFeatures;
        VkFormatFeatureFlags allowedFeatures;
    } fields[] = {{"linearTilingFeatures", properties.linearTilingFeatures, (VkFormatFeatureFlags)0, allowImg},
                  {"optimalTilingFeatures", properties.optimalTilingFeatures, reqImg, allowImg},
                  {"bufferFeatures", properties.bufferFeatures, reqBuf, allowBuf}};

    log << TestLog::Message << properties << TestLog::EndMessage;

    if (format == vk::VK_FORMAT_UNDEFINED)
    {
        VkFormatProperties formatUndefProperties;
        deMemset(&formatUndefProperties, 0xcd, sizeof(VkFormatProperties));
        formatUndefProperties.bufferFeatures        = 0;
        formatUndefProperties.linearTilingFeatures  = 0;
        formatUndefProperties.optimalTilingFeatures = 0;
        results.check((deMemCmp(&formatUndefProperties, &properties, sizeof(VkFormatProperties)) == 0),
                      "vkGetPhysicalDeviceFormatProperties, with VK_FORMAT_UNDEFINED as input format, is returning "
                      "non-zero properties");
    }
    else
    {
        for (int fieldNdx = 0; fieldNdx < DE_LENGTH_OF_ARRAY(fields); fieldNdx++)
        {
            const char *const fieldName         = fields[fieldNdx].fieldName;
            VkFormatFeatureFlags supported      = fields[fieldNdx].supportedFeatures;
            const VkFormatFeatureFlags required = fields[fieldNdx].requiredFeatures;
            const VkFormatFeatureFlags allowed  = fields[fieldNdx].allowedFeatures;

            if (apiVersion10WithoutKhrMaintenance1 && supported)
            {
                supported |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
            }

            results.check((supported & required) == required,
                          de::toString(fieldName) + ": required: " + de::toString(getFormatFeatureFlagsStr(required)) +
                              "  missing: " + de::toString(getFormatFeatureFlagsStr(~supported & required)));

            results.check((supported & ~allowed) == 0,
                          de::toString(fieldName) +
                              ": has: " + de::toString(getFormatFeatureFlagsStr(supported & ~allowed)));

            if (((supported & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT) !=
                 0) &&
                ((supported &
                  VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT) == 0))
            {
                results.addResult(
                    QP_TEST_RESULT_FAIL,
                    de::toString(fieldName) +
                        " supports VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT "
                        "but not "
                        "VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_"
                        "BIT");
            }

            if (!isYCbCrFormat(format) && !isCompressedFormat(format))
            {
                const tcu::TextureFormat tcuFormat = mapVkFormat(format);
                if (tcu::getNumUsedChannels(tcuFormat.order) != 1 &&
                    (supported & (VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT |
                                  VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)) != 0)
                {
                    results.addResult(
                        QP_TEST_RESULT_QUALITY_WARNING,
                        "VK_FORMAT_FEATURE_STORAGE_*_ATOMIC_BIT is only defined for single-component images");
                }
            }
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

bool optimalTilingFeaturesSupported(Context &context, VkFormat format, VkFormatFeatureFlags features)
{
    const VkFormatProperties properties =
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
    const bool apiVersion10WithoutKhrMaintenance1 =
        isApiVersionEqual(context.getUsedApiVersion(), VK_API_VERSION_1_0) &&
        !context.isDeviceFunctionalitySupported("VK_KHR_maintenance1");
    VkFormatFeatureFlags supported = properties.optimalTilingFeatures;

    if (apiVersion10WithoutKhrMaintenance1 && supported)
    {
        supported |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    }

    return (supported & features) == features;
}

bool optimalTilingFeaturesSupportedForAll(Context &context, const VkFormat *begin, const VkFormat *end,
                                          VkFormatFeatureFlags features)
{
    for (const VkFormat *cur = begin; cur != end; ++cur)
    {
        if (!optimalTilingFeaturesSupported(context, *cur, features))
            return false;
    }

    return true;
}

tcu::TestStatus testDepthStencilSupported(Context &context)
{
    if (!optimalTilingFeaturesSupported(context, VK_FORMAT_X8_D24_UNORM_PACK32,
                                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
        !optimalTilingFeaturesSupported(context, VK_FORMAT_D32_SFLOAT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        return tcu::TestStatus::fail("Doesn't support one of VK_FORMAT_X8_D24_UNORM_PACK32 or VK_FORMAT_D32_SFLOAT");

    if (!optimalTilingFeaturesSupported(context, VK_FORMAT_D24_UNORM_S8_UINT,
                                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
        !optimalTilingFeaturesSupported(context, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        return tcu::TestStatus::fail(
            "Doesn't support one of VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT");

    return tcu::TestStatus::pass("Required depth/stencil formats supported");
}

tcu::TestStatus testCompressedFormatsSupported(Context &context)
{
    static const VkFormat s_allBcFormats[] = {
        VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK,    VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,     VK_FORMAT_BC3_SRGB_BLOCK,     VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,     VK_FORMAT_BC5_UNORM_BLOCK,    VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC6H_UFLOAT_BLOCK,   VK_FORMAT_BC6H_SFLOAT_BLOCK,  VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
    };
    static const VkFormat s_allEtc2Formats[] = {
        VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,  VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,    VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        VK_FORMAT_EAC_R11_UNORM_BLOCK,      VK_FORMAT_EAC_R11_SNORM_BLOCK,       VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
        VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
    };
    static const VkFormat s_allAstcLdrFormats[] = {
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,   VK_FORMAT_ASTC_4x4_SRGB_BLOCK,    VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,    VK_FORMAT_ASTC_5x5_UNORM_BLOCK,   VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK,   VK_FORMAT_ASTC_6x5_SRGB_BLOCK,    VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,    VK_FORMAT_ASTC_8x5_UNORM_BLOCK,   VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK,   VK_FORMAT_ASTC_8x6_SRGB_BLOCK,    VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,    VK_FORMAT_ASTC_10x5_UNORM_BLOCK,  VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK,  VK_FORMAT_ASTC_10x6_SRGB_BLOCK,   VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,   VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK,  VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
    };

    static const struct
    {
        const char *setName;
        const char *featureName;
        const VkBool32 VkPhysicalDeviceFeatures::*feature;
        const VkFormat *formatsBegin;
        const VkFormat *formatsEnd;
    } s_compressedFormatSets[] = {
        {"BC", "textureCompressionBC", &VkPhysicalDeviceFeatures::textureCompressionBC, DE_ARRAY_BEGIN(s_allBcFormats),
         DE_ARRAY_END(s_allBcFormats)},
        {"ETC2", "textureCompressionETC2", &VkPhysicalDeviceFeatures::textureCompressionETC2,
         DE_ARRAY_BEGIN(s_allEtc2Formats), DE_ARRAY_END(s_allEtc2Formats)},
        {"ASTC LDR", "textureCompressionASTC_LDR", &VkPhysicalDeviceFeatures::textureCompressionASTC_LDR,
         DE_ARRAY_BEGIN(s_allAstcLdrFormats), DE_ARRAY_END(s_allAstcLdrFormats)},
    };

    TestLog &log                             = context.getTestContext().getLog();
    const VkPhysicalDeviceFeatures &features = context.getDeviceFeatures();
    int numSupportedSets                     = 0;
    int numErrors                            = 0;
    int numWarnings                          = 0;

    for (int setNdx = 0; setNdx < DE_LENGTH_OF_ARRAY(s_compressedFormatSets); ++setNdx)
    {
        const char *const setName     = s_compressedFormatSets[setNdx].setName;
        const char *const featureName = s_compressedFormatSets[setNdx].featureName;
        const bool featureBitSet      = features.*s_compressedFormatSets[setNdx].feature == VK_TRUE;
        const VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
            VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        const bool allSupported =
            optimalTilingFeaturesSupportedForAll(context, s_compressedFormatSets[setNdx].formatsBegin,
                                                 s_compressedFormatSets[setNdx].formatsEnd, requiredFeatures);

        if (featureBitSet && !allSupported)
        {
            log << TestLog::Message << "ERROR: " << featureName << " = VK_TRUE but " << setName
                << " formats not supported" << TestLog::EndMessage;
            numErrors += 1;
        }
        else if (allSupported && !featureBitSet)
        {
            log << TestLog::Message << "WARNING: " << setName << " formats supported but " << featureName
                << " = VK_FALSE" << TestLog::EndMessage;
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

void createFormatTests(tcu::TestCaseGroup *testGroup)
{
    DE_STATIC_ASSERT(VK_FORMAT_UNDEFINED == 0);

    static const struct
    {
        VkFormat begin;
        VkFormat end;
    } s_formatRanges[] = {
        // core formats
        {(VkFormat)(VK_FORMAT_UNDEFINED), VK_CORE_FORMAT_LAST},

        // YCbCr formats
        {VK_FORMAT_G8B8G8R8_422_UNORM, (VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM + 1)},

        // YCbCr extended formats
        {VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT, (VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT + 1)},
    };

    for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
    {
        const VkFormat rangeBegin = s_formatRanges[rangeNdx].begin;
        const VkFormat rangeEnd   = s_formatRanges[rangeNdx].end;

        for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format + 1))
        {
            const char *const enumName = getFormatName(format);
            const string caseName      = de::toLower(string(enumName).substr(10));

            addFunctionCase(testGroup, caseName, formatProperties, format);
        }
    }

    addFunctionCase(testGroup, "depth_stencil", testDepthStencilSupported);
    addFunctionCase(testGroup, "compressed_formats", testCompressedFormatsSupported);
}

VkImageUsageFlags getValidImageUsageFlags(const VkFormatFeatureFlags supportedFeatures,
                                          const bool useKhrMaintenance1Semantics)
{
    VkImageUsageFlags flags = (VkImageUsageFlags)0;

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
            flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if ((supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if ((supportedFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0)
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    if ((supportedFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if ((supportedFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;

    if ((supportedFeatures &
         (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
        flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    return flags;
}

bool isValidImageUsageFlagCombination(VkImageUsageFlags usage)
{
    if ((usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0)
    {
        const VkImageUsageFlags allowedFlags =
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        // Only *_ATTACHMENT_BIT flags can be combined with TRANSIENT_ATTACHMENT_BIT
        if ((usage & ~allowedFlags) != 0)
            return false;

        // TRANSIENT_ATTACHMENT_BIT is not valid without COLOR_ or DEPTH_STENCIL_ATTACHMENT_BIT
        if ((usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0)
            return false;
    }

    return usage != 0;
}

VkImageCreateFlags getValidImageCreateFlags(const VkPhysicalDeviceFeatures &deviceFeatures, VkFormat format,
                                            VkFormatFeatureFlags formatFeatures, VkImageType type,
                                            VkImageUsageFlags usage)
{
    VkImageCreateFlags flags = (VkImageCreateFlags)0;

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
        if (formatFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT)
            flags |= VK_IMAGE_CREATE_DISJOINT_BIT;
    }

    if ((usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)) != 0 &&
        (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) == 0)
    {
        if (deviceFeatures.sparseBinding)
            flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

        if (deviceFeatures.sparseResidencyAliased)
            flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
    }

    return flags;
}

bool isValidImageCreateFlagCombination(VkImageCreateFlags createFlags)
{
    bool isValid = true;

    if (((createFlags & (VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)) != 0) &&
        ((createFlags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) == 0))
    {
        isValid = false;
    }

    return isValid;
}

bool isRequiredImageParameterCombination(const VkPhysicalDeviceFeatures &deviceFeatures, const VkFormat format,
                                         const VkFormatProperties &formatProperties, const VkImageType imageType,
                                         const VkImageTiling imageTiling, const VkImageUsageFlags usageFlags,
                                         const VkImageCreateFlags createFlags)
{
    DE_UNREF(deviceFeatures);
    DE_UNREF(formatProperties);
    DE_UNREF(createFlags);

    // Linear images can have arbitrary limitations
    if (imageTiling == VK_IMAGE_TILING_LINEAR)
        return false;

    // Support for other usages for compressed formats is optional
    if (isCompressedFormat(format) && (usageFlags & ~(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
        return false;

    // Support for 1D, and sliced 3D compressed formats is optional
    if (isCompressedFormat(format) && (imageType == VK_IMAGE_TYPE_1D || imageType == VK_IMAGE_TYPE_3D))
        return false;

    // Support for 1D and 3D depth/stencil textures is optional
    if (isDepthStencilFormat(format) && (imageType == VK_IMAGE_TYPE_1D || imageType == VK_IMAGE_TYPE_3D))
        return false;

    DE_ASSERT(deviceFeatures.sparseBinding ||
              (createFlags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) == 0);
    DE_ASSERT(deviceFeatures.sparseResidencyAliased || (createFlags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT) == 0);

    if (isYCbCrFormat(format) &&
        (createFlags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT |
                        VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)))
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

VkSampleCountFlags getRequiredOptimalTilingSampleCounts(const VkPhysicalDeviceLimits &deviceLimits, bool hasVulkan12,
                                                        const VkPhysicalDeviceVulkan12Properties &vulkan12Properties,
                                                        const VkPhysicalDeviceFeatures &deviceFeatures,
                                                        const VkImageCreateFlags createFlags, const VkFormat format,
                                                        const VkImageUsageFlags usageFlags)
{
    if (isCompressedFormat(format))
        return VK_SAMPLE_COUNT_1_BIT;

    bool hasDepthComp   = false;
    bool hasStencilComp = false;
    const bool isYCbCr  = isYCbCrFormat(format);
    if (!isYCbCr)
    {
        const tcu::TextureFormat tcuFormat = mapVkFormat(format);
        hasDepthComp   = (tcuFormat.order == tcu::TextureFormat::D || tcuFormat.order == tcu::TextureFormat::DS);
        hasStencilComp = (tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS);
    }

    const bool isColorFormat        = !hasDepthComp && !hasStencilComp;
    VkSampleCountFlags sampleCounts = ~(VkSampleCountFlags)0;

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
                const tcu::TextureFormat tcuFormat      = mapVkFormat(format);
                const tcu::TextureChannelClass chnClass = tcu::getTextureChannelClass(tcuFormat.type);

                if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ||
                    chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
                    sampleCounts &= deviceLimits.sampledImageIntegerSampleCounts;
                else
                    sampleCounts &= deviceLimits.sampledImageColorSampleCounts;
            }
        }
    }

    if ((usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
    {
        const tcu::TextureFormat tcuFormat      = mapVkFormat(format);
        const tcu::TextureChannelClass chnClass = tcu::getTextureChannelClass(tcuFormat.type);

        if (hasVulkan12 && (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ||
                            chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER))
            sampleCounts &= vulkan12Properties.framebufferIntegerColorSampleCounts;
        else
            sampleCounts &= deviceLimits.framebufferColorSampleCounts;
    }

    if ((usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    {
        if (hasDepthComp)
            sampleCounts &= deviceLimits.framebufferDepthSampleCounts;

        if (hasStencilComp)
            sampleCounts &= deviceLimits.framebufferStencilSampleCounts;
    }

    if ((createFlags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) != 0)
    {
        const VkSampleCountFlags sparseSampleCounts =
            VK_SAMPLE_COUNT_1_BIT | (deviceFeatures.sparseResidency2Samples ? VK_SAMPLE_COUNT_2_BIT : 0) |
            (deviceFeatures.sparseResidency4Samples ? VK_SAMPLE_COUNT_4_BIT : 0) |
            (deviceFeatures.sparseResidency8Samples ? VK_SAMPLE_COUNT_8_BIT : 0) |
            (deviceFeatures.sparseResidency16Samples ? VK_SAMPLE_COUNT_16_BIT : 0);

        sampleCounts &= sparseSampleCounts;
    }

    // If there is no usage flag set that would have corresponding device limit,
    // only VK_SAMPLE_COUNT_1_BIT is required.
    if (sampleCounts == ~(VkSampleCountFlags)0)
        sampleCounts &= VK_SAMPLE_COUNT_1_BIT;

    return sampleCounts;
}

struct ImageFormatPropertyCase
{
    typedef tcu::TestStatus (*Function)(Context &context, const VkFormat format, const VkImageType imageType,
                                        const VkImageTiling tiling);

    Function testFunction;
    VkFormat format;
    VkImageType imageType;
    VkImageTiling tiling;

    ImageFormatPropertyCase(Function testFunction_, VkFormat format_, VkImageType imageType_, VkImageTiling tiling_)
        : testFunction(testFunction_)
        , format(format_)
        , imageType(imageType_)
        , tiling(tiling_)
    {
    }

    ImageFormatPropertyCase(void)
        : testFunction(nullptr)
        , format(VK_FORMAT_UNDEFINED)
        , imageType(VK_CORE_IMAGE_TYPE_LAST)
        , tiling(VK_CORE_IMAGE_TILING_LAST)
    {
    }
};

tcu::TestStatus imageFormatProperties(Context &context, const VkFormat format, const VkImageType imageType,
                                      const VkImageTiling tiling)
{
    if (isYCbCrFormat(format))
        // check if Ycbcr format enums are valid given the version and extensions
        checkYcbcrApiSupport(context);

    TestLog &log                                                 = context.getTestContext().getLog();
    const VkPhysicalDeviceFeatures &deviceFeatures               = context.getDeviceFeatures();
    const VkPhysicalDeviceLimits &deviceLimits                   = context.getDeviceProperties().limits;
    const VkPhysicalDeviceVulkan12Properties &vulkan12Properties = context.getDeviceVulkan12Properties();
    const VkFormatProperties formatProperties =
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
    const bool hasKhrMaintenance1 = context.isDeviceFunctionalitySupported("VK_KHR_maintenance1");
    const bool hasVulkan12        = context.contextSupports(VK_API_VERSION_1_2);

    const VkFormatFeatureFlags supportedFeatures = tiling == VK_IMAGE_TILING_LINEAR ?
                                                       formatProperties.linearTilingFeatures :
                                                       formatProperties.optimalTilingFeatures;
    const VkImageUsageFlags usageFlagSet         = getValidImageUsageFlags(supportedFeatures, hasKhrMaintenance1);

    tcu::ResultCollector results(log, "ERROR: ");

    if (hasKhrMaintenance1 && (supportedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
    {
        results.check((supportedFeatures & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) !=
                          0,
                      "A sampled image format must have VK_FORMAT_FEATURE_TRANSFER_SRC_BIT and "
                      "VK_FORMAT_FEATURE_TRANSFER_DST_BIT format feature flags set");
    }

    if (isYcbcrConversionSupported(context) &&
        (format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM || format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM))
    {
        VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if (tiling == VK_IMAGE_TILING_OPTIMAL)
            requiredFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;

        results.check((supportedFeatures & requiredFeatures) == requiredFeatures,
                      getFormatName(format) + string(" must support ") +
                          de::toString(getFormatFeatureFlagsStr(requiredFeatures)));
    }

    for (VkImageUsageFlags curUsageFlags = 0; curUsageFlags <= usageFlagSet; curUsageFlags++)
    {
        if ((curUsageFlags & ~usageFlagSet) != 0 || !isValidImageUsageFlagCombination(curUsageFlags))
            continue;

        const VkImageCreateFlags createFlagSet =
            getValidImageCreateFlags(deviceFeatures, format, supportedFeatures, imageType, curUsageFlags);

        for (VkImageCreateFlags curCreateFlags = 0; curCreateFlags <= createFlagSet; curCreateFlags++)
        {
            if ((curCreateFlags & ~createFlagSet) != 0 || !isValidImageCreateFlagCombination(curCreateFlags))
                continue;

            const bool isRequiredCombination = isRequiredImageParameterCombination(
                deviceFeatures, format, formatProperties, imageType, tiling, curUsageFlags, curCreateFlags);
            VkImageFormatProperties properties;
            VkResult queryResult;

            log << TestLog::Message << "Testing " << getImageTypeStr(imageType) << ", " << getImageTilingStr(tiling)
                << ", " << getImageUsageFlagsStr(curUsageFlags) << ", " << getImageCreateFlagsStr(curCreateFlags)
                << TestLog::EndMessage;

            // Set return value to known garbage
            deMemset(&properties, 0xcd, sizeof(properties));

            queryResult = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), format, imageType, tiling, curUsageFlags, curCreateFlags, &properties);

            if (queryResult == VK_SUCCESS)
            {
                const uint32_t fullMipPyramidSize = de::max(de::max(deLog2Floor32(properties.maxExtent.width),
                                                                    deLog2Floor32(properties.maxExtent.height)),
                                                            deLog2Floor32(properties.maxExtent.depth)) +
                                                    1;

                log << TestLog::Message << properties << "\n" << TestLog::EndMessage;

                results.check(imageType != VK_IMAGE_TYPE_1D ||
                                  (properties.maxExtent.width >= 1 && properties.maxExtent.height == 1 &&
                                   properties.maxExtent.depth == 1),
                              "Invalid dimensions for 1D image");
                results.check(imageType != VK_IMAGE_TYPE_2D ||
                                  (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 &&
                                   properties.maxExtent.depth == 1),
                              "Invalid dimensions for 2D image");
                results.check(imageType != VK_IMAGE_TYPE_3D ||
                                  (properties.maxExtent.width >= 1 && properties.maxExtent.height >= 1 &&
                                   properties.maxExtent.depth >= 1),
                              "Invalid dimensions for 3D image");
                results.check(imageType != VK_IMAGE_TYPE_3D || properties.maxArrayLayers == 1,
                              "Invalid maxArrayLayers for 3D image");

                if (tiling == VK_IMAGE_TILING_OPTIMAL && imageType == VK_IMAGE_TYPE_2D &&
                    !(curCreateFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
                    (supportedFeatures &
                     (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)))
                {
                    const VkSampleCountFlags requiredSampleCounts =
                        getRequiredOptimalTilingSampleCounts(deviceLimits, hasVulkan12, vulkan12Properties,
                                                             deviceFeatures, curCreateFlags, format, curUsageFlags);
                    results.check((properties.sampleCounts & requiredSampleCounts) == requiredSampleCounts,
                                  "Required sample counts not supported");
                }
                else
                    results.check(properties.sampleCounts == VK_SAMPLE_COUNT_1_BIT,
                                  "sampleCounts != VK_SAMPLE_COUNT_1_BIT");

                if (isRequiredCombination)
                {
                    results.check(imageType != VK_IMAGE_TYPE_1D ||
                                      (properties.maxExtent.width >= deviceLimits.maxImageDimension1D),
                                  "Reported dimensions smaller than device limits");
                    results.check(imageType != VK_IMAGE_TYPE_2D ||
                                      (properties.maxExtent.width >= deviceLimits.maxImageDimension2D &&
                                       properties.maxExtent.height >= deviceLimits.maxImageDimension2D),
                                  "Reported dimensions smaller than device limits");
                    results.check(imageType != VK_IMAGE_TYPE_3D ||
                                      (properties.maxExtent.width >= deviceLimits.maxImageDimension3D &&
                                       properties.maxExtent.height >= deviceLimits.maxImageDimension3D &&
                                       properties.maxExtent.depth >= deviceLimits.maxImageDimension3D),
                                  "Reported dimensions smaller than device limits");
                    results.check((isYCbCrFormat(format) && (properties.maxMipLevels == 1)) ||
                                      properties.maxMipLevels == fullMipPyramidSize,
                                  "Invalid mip pyramid size");
                    results.check((isYCbCrFormat(format) && (properties.maxArrayLayers == 1)) ||
                                      imageType == VK_IMAGE_TYPE_3D ||
                                      properties.maxArrayLayers >= deviceLimits.maxImageArrayLayers,
                                  "Invalid maxArrayLayers");
                }
                else
                {
                    results.check(properties.maxMipLevels == 1 || properties.maxMipLevels == fullMipPyramidSize,
                                  "Invalid mip pyramid size");
                    results.check(properties.maxArrayLayers >= 1, "Invalid maxArrayLayers");
                }

                results.check(properties.maxResourceSize >= (VkDeviceSize)MINIMUM_REQUIRED_IMAGE_RESOURCE_SIZE,
                              "maxResourceSize smaller than minimum required size");

                if (format == VK_FORMAT_UNDEFINED)
                    results.fail("VK_SUCCESS returned for VK_FORMAT_UNDEFINED format");
            }
            else if (queryResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                log << TestLog::Message << "Got VK_ERROR_FORMAT_NOT_SUPPORTED" << TestLog::EndMessage;

                if (isRequiredCombination)
                    results.fail("VK_ERROR_FORMAT_NOT_SUPPORTED returned for required image parameter combination");

                // Specification requires that all fields are set to 0
                results.check(properties.maxExtent.width == 0, "maxExtent.width != 0");
                results.check(properties.maxExtent.height == 0, "maxExtent.height != 0");
                results.check(properties.maxExtent.depth == 0, "maxExtent.depth != 0");
                results.check(properties.maxMipLevels == 0, "maxMipLevels != 0");
                results.check(properties.maxArrayLayers == 0, "maxArrayLayers != 0");
                results.check(properties.sampleCounts == 0, "sampleCounts != 0");
                results.check(properties.maxResourceSize == 0, "maxResourceSize != 0");
            }
            else
            {
                if (format == VK_FORMAT_UNDEFINED)
                    results.fail(de::toString(queryResult) + " returned for VK_FORMAT_UNDEFINED format");

                results.fail("Got unexpected error" + de::toString(queryResult));
            }
        }
    }
    return tcu::TestStatus(results.getResult(), results.getMessage());
}

struct ImageUsagePropertyCase
{
    typedef tcu::TestStatus (*Function)(Context &context, const VkFormat format, const VkImageUsageFlags usage,
                                        const VkImageTiling tiling);

    Function testFunction;
    VkFormat format;
    VkImageUsageFlags usage;
    VkImageTiling tiling;

    ImageUsagePropertyCase(Function testFunction_, VkFormat format_, VkImageUsageFlags usage_, VkImageTiling tiling_)
        : testFunction(testFunction_)
        , format(format_)
        , usage(usage_)
        , tiling(tiling_)
    {
    }

    ImageUsagePropertyCase(void)
        : testFunction(nullptr)
        , format(VK_FORMAT_UNDEFINED)
        , usage(0)
        , tiling(VK_CORE_IMAGE_TILING_LAST)
    {
    }
};

tcu::TestStatus unsupportedImageUsage(Context &context, const VkFormat format, const VkImageUsageFlags imageUsage,
                                      const VkImageTiling tiling)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
#ifndef CTS_USES_VULKANSC
    const VkFormatProperties3 formatProperties    = context.getFormatProperties(format);
    const VkFormatFeatureFlags2 supportedFeatures = tiling == VK_IMAGE_TILING_LINEAR ?
                                                        formatProperties.linearTilingFeatures :
                                                        formatProperties.optimalTilingFeatures;
#else
    const VkFormatProperties formatProperties =
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
    const VkFormatFeatureFlags supportedFeatures = tiling == VK_IMAGE_TILING_LINEAR ?
                                                       formatProperties.linearTilingFeatures :
                                                       formatProperties.optimalTilingFeatures;
#endif
    TestLog &log = context.getTestContext().getLog();

    VkFormatFeatureFlags2 usageRequiredFeatures = 0u;
    switch (imageUsage)
    {
    case VK_IMAGE_USAGE_SAMPLED_BIT:
        usageRequiredFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        break;
    case VK_IMAGE_USAGE_STORAGE_BIT:
        usageRequiredFeatures = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        break;
    case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
#ifndef CTS_USES_VULKANSC
        if (tiling == VK_IMAGE_TILING_LINEAR && context.getLinearColorAttachmentFeatures().linearColorAttachment)
        {
            usageRequiredFeatures = VK_FORMAT_FEATURE_2_LINEAR_COLOR_ATTACHMENT_BIT_NV;
        }
        else
        {
            usageRequiredFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        }
#else
        usageRequiredFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
#endif
        break;
    case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
        usageRequiredFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
        usageRequiredFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    case VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR:
        usageRequiredFeatures = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        break;
    default:
        DE_ASSERT(0);
    }

    VkImageFormatProperties imageFormatProperties;
    VkResult res = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, tiling,
                                                              imageUsage, 0u, &imageFormatProperties);

    if ((supportedFeatures & usageRequiredFeatures) == 0 && res != VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        log << TestLog::Message << "Required format features for usage " << imageUsage
            << " are not supported. Format features are " << supportedFeatures
            << ", but vkGetPhysicalDeviceImageFormatProperties with usage " << imageUsage << " returned " << res
            << TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

// VK_KHR_get_physical_device_properties2

string toString(const VkPhysicalDevicePCIBusInfoPropertiesEXT &value)
{
    std::ostringstream s;
    s << "VkPhysicalDevicePCIBusInfoPropertiesEXT = {\n";
    s << "\tsType = " << value.sType << '\n';
    s << "\tpciDomain = " << value.pciDomain << '\n';
    s << "\tpciBus = " << value.pciBus << '\n';
    s << "\tpciDevice = " << value.pciDevice << '\n';
    s << "\tpciFunction = " << value.pciFunction << '\n';
    s << '}';
    return s.str();
}

bool checkExtension(vector<VkExtensionProperties> &properties, const char *extension)
{
    for (size_t ndx = 0; ndx < properties.size(); ++ndx)
    {
        if (strncmp(properties[ndx].extensionName, extension, VK_MAX_EXTENSION_NAME_SIZE) == 0)
            return true;
    }
    return false;
}

#include "vkDeviceFeatures2.inl"

tcu::TestStatus deviceFeatures2(Context &context)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    TestLog &log                          = context.getTestContext().getLog();
    VkPhysicalDeviceFeatures coreFeatures;
    VkPhysicalDeviceFeatures2 extFeatures;

    deMemset(&coreFeatures, 0xcd, sizeof(coreFeatures));
    deMemset(&extFeatures.features, 0xcd, sizeof(extFeatures.features));
    std::vector<std::string> instExtensions = context.getInstanceExtensions();

    extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    extFeatures.pNext = nullptr;

    vki.getPhysicalDeviceFeatures(physicalDevice, &coreFeatures);
    vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

    TCU_CHECK(extFeatures.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    TCU_CHECK(extFeatures.pNext == nullptr);

    if (deMemCmp(&coreFeatures, &extFeatures.features, sizeof(VkPhysicalDeviceFeatures)) != 0)
        TCU_FAIL("Mismatch between features reported by vkGetPhysicalDeviceFeatures and vkGetPhysicalDeviceFeatures2");

    log << TestLog::Message << extFeatures << TestLog::EndMessage;

    return tcu::TestStatus::pass("Querying device features succeeded");
}

tcu::TestStatus deviceProperties2(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    TestLog &log                          = context.getTestContext().getLog();
    VkPhysicalDeviceProperties coreProperties;
    VkPhysicalDeviceProperties2 extProperties;

    extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    extProperties.pNext = nullptr;

    vki.getPhysicalDeviceProperties(physicalDevice, &coreProperties);
    vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

    TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    TCU_CHECK(extProperties.pNext == nullptr);

    // We can't use memcmp() here because the structs may contain padding bytes that drivers may or may not
    // have written while writing the data and memcmp will compare them anyway, so we iterate through the
    // valid bytes for each field in the struct and compare only the valid bytes for each one.
    for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(s_physicalDevicePropertiesOffsetTable); propNdx++)
    {
        const size_t offset = s_physicalDevicePropertiesOffsetTable[propNdx].offset;
        const size_t size   = s_physicalDevicePropertiesOffsetTable[propNdx].size;

        const uint8_t *corePropertyBytes = reinterpret_cast<uint8_t *>(&coreProperties) + offset;
        const uint8_t *extPropertyBytes  = reinterpret_cast<uint8_t *>(&extProperties.properties) + offset;

        if (deMemCmp(corePropertyBytes, extPropertyBytes, size) != 0)
            TCU_FAIL("Mismatch between properties reported by vkGetPhysicalDeviceProperties and "
                     "vkGetPhysicalDeviceProperties2");
    }

    log << TestLog::Message << extProperties.properties << TestLog::EndMessage;

    const int count = 2u;

    vector<VkExtensionProperties> properties   = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
    const bool khr_external_fence_capabilities = checkExtension(properties, "VK_KHR_external_fence_capabilities") ||
                                                 context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_external_memory_capabilities = checkExtension(properties, "VK_KHR_external_memory_capabilities") ||
                                                  context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_external_semaphore_capabilities =
        checkExtension(properties, "VK_KHR_external_semaphore_capabilities") ||
        context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_multiview =
        checkExtension(properties, "VK_KHR_multiview") || context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_device_protected_memory = context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_device_subgroup         = context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_maintenance2 =
        checkExtension(properties, "VK_KHR_maintenance2") || context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_maintenance3 =
        checkExtension(properties, "VK_KHR_maintenance3") || context.contextSupports(vk::ApiVersion(0, 1, 1, 0));
    const bool khr_depth_stencil_resolve = checkExtension(properties, "VK_KHR_depth_stencil_resolve") ||
                                           context.contextSupports(vk::ApiVersion(0, 1, 2, 0));
    const bool khr_driver_properties =
        checkExtension(properties, "VK_KHR_driver_properties") || context.contextSupports(vk::ApiVersion(0, 1, 2, 0));
    const bool khr_shader_float_controls = checkExtension(properties, "VK_KHR_shader_float_controls") ||
                                           context.contextSupports(vk::ApiVersion(0, 1, 2, 0));
    const bool khr_descriptor_indexing =
        checkExtension(properties, "VK_EXT_descriptor_indexing") || context.contextSupports(vk::ApiVersion(0, 1, 2, 0));
    const bool khr_sampler_filter_minmax = checkExtension(properties, "VK_EXT_sampler_filter_minmax") ||
                                           context.contextSupports(vk::ApiVersion(0, 1, 2, 0));
#ifndef CTS_USES_VULKANSC
    const bool khr_acceleration_structure = checkExtension(properties, "VK_KHR_acceleration_structure");
    const bool khr_integer_dot_product    = checkExtension(properties, "VK_KHR_shader_integer_dot_product") ||
                                         context.contextSupports(vk::ApiVersion(0, 1, 3, 0));
    const bool khr_inline_uniform_block = checkExtension(properties, "VK_EXT_inline_uniform_block") ||
                                          context.contextSupports(vk::ApiVersion(0, 1, 3, 0));
    const bool khr_maintenance4 =
        checkExtension(properties, "VK_KHR_maintenance4") || context.contextSupports(vk::ApiVersion(0, 1, 3, 0));
    const bool khr_subgroup_size_control = checkExtension(properties, "VK_EXT_subgroup_size_control") ||
                                           context.contextSupports(vk::ApiVersion(0, 1, 3, 0));
    const bool khr_texel_buffer_alignment = checkExtension(properties, "VK_EXT_texel_buffer_alignment") ||
                                            context.contextSupports(vk::ApiVersion(0, 1, 3, 0));
#endif // CTS_USES_VULKANSC

    VkPhysicalDeviceIDProperties idProperties[count];
    VkPhysicalDeviceMultiviewProperties multiviewProperties[count];
    VkPhysicalDeviceProtectedMemoryProperties protectedMemoryPropertiesKHR[count];
    VkPhysicalDeviceSubgroupProperties subgroupProperties[count];
    VkPhysicalDevicePointClippingProperties pointClippingProperties[count];
    VkPhysicalDeviceMaintenance3Properties maintenance3Properties[count];
    VkPhysicalDeviceDepthStencilResolveProperties depthStencilResolveProperties[count];
    VkPhysicalDeviceDriverProperties driverProperties[count];
    VkPhysicalDeviceFloatControlsProperties floatControlsProperties[count];
    VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties[count];
    VkPhysicalDeviceSamplerFilterMinmaxProperties samplerFilterMinmaxProperties[count];
#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR integerDotProductProperties[count];
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties[count];
    VkPhysicalDeviceInlineUniformBlockProperties inlineUniformBlockProperties[count];
    VkPhysicalDeviceMaintenance4Properties maintenance4Properties[count];
    VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties[count];
    VkPhysicalDeviceTexelBufferAlignmentProperties texelBufferAlignmentProperties[count];
#endif // CTS_USES_VULKANSC
    for (int ndx = 0; ndx < count; ++ndx)
    {
        deMemset(&idProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceIDProperties));
        deMemset(&multiviewProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceMultiviewProperties));
        deMemset(&protectedMemoryPropertiesKHR[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceProtectedMemoryProperties));
        deMemset(&subgroupProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceSubgroupProperties));
        deMemset(&pointClippingProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDevicePointClippingProperties));
        deMemset(&maintenance3Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceMaintenance3Properties));
        deMemset(&depthStencilResolveProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceDepthStencilResolveProperties));
        deMemset(&driverProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceDriverProperties));
        deMemset(&floatControlsProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceFloatControlsProperties));
        deMemset(&descriptorIndexingProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceDescriptorIndexingProperties));
        deMemset(&samplerFilterMinmaxProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceSamplerFilterMinmaxProperties));
#ifndef CTS_USES_VULKANSC
        deMemset(&integerDotProductProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR));
        deMemset(&accelerationStructureProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceAccelerationStructurePropertiesKHR));
        deMemset(&inlineUniformBlockProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceInlineUniformBlockProperties));
        deMemset(&maintenance4Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceMaintenance4Properties));
        deMemset(&subgroupSizeControlProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceSubgroupSizeControlProperties));
        deMemset(&texelBufferAlignmentProperties[ndx], 0xFF * ndx,
                 sizeof(VkPhysicalDeviceTexelBufferAlignmentProperties));
#endif // CTS_USES_VULKANSC

        void *prev = 0;

        if (khr_external_fence_capabilities || khr_external_memory_capabilities || khr_external_semaphore_capabilities)
        {
            idProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            idProperties[ndx].pNext = prev;
            prev                    = &idProperties[ndx];
        }

        if (khr_multiview)
        {
            multiviewProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
            multiviewProperties[ndx].pNext = prev;
            prev                           = &multiviewProperties[ndx];
        }

        if (khr_device_protected_memory)
        {
            protectedMemoryPropertiesKHR[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
            protectedMemoryPropertiesKHR[ndx].pNext = prev;
            prev                                    = &protectedMemoryPropertiesKHR[ndx];
        }

        if (khr_device_subgroup)
        {
            subgroupProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
            subgroupProperties[ndx].pNext = prev;
            prev                          = &subgroupProperties[ndx];
        }

        if (khr_maintenance2)
        {
            pointClippingProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
            pointClippingProperties[ndx].pNext = prev;
            prev                               = &pointClippingProperties[ndx];
        }

        if (khr_maintenance3)
        {
            maintenance3Properties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
            maintenance3Properties[ndx].pNext = prev;
            prev                              = &maintenance3Properties[ndx];
        }

        if (khr_depth_stencil_resolve)
        {
            depthStencilResolveProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
            depthStencilResolveProperties[ndx].pNext = prev;
            prev                                     = &depthStencilResolveProperties[ndx];
        }

        if (khr_driver_properties)
        {
            driverProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            driverProperties[ndx].pNext = prev;
            prev                        = &driverProperties[ndx];
        }

        if (khr_shader_float_controls)
        {
            floatControlsProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
            floatControlsProperties[ndx].pNext = prev;
            prev                               = &floatControlsProperties[ndx];
        }

        if (khr_descriptor_indexing)
        {
            descriptorIndexingProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
            descriptorIndexingProperties[ndx].pNext = prev;
            prev                                    = &descriptorIndexingProperties[ndx];
        }

        if (khr_sampler_filter_minmax)
        {
            samplerFilterMinmaxProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
            samplerFilterMinmaxProperties[ndx].pNext = prev;
            prev                                     = &samplerFilterMinmaxProperties[ndx];
        }

#ifndef CTS_USES_VULKANSC
        if (khr_integer_dot_product)
        {
            integerDotProductProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES_KHR;
            integerDotProductProperties[ndx].pNext = prev;
            prev                                   = &integerDotProductProperties[ndx];
        }

        if (khr_acceleration_structure)
        {
            accelerationStructureProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            accelerationStructureProperties[ndx].pNext = prev;
            prev                                       = &accelerationStructureProperties[ndx];
        }

        if (khr_inline_uniform_block)
        {
            inlineUniformBlockProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES;
            inlineUniformBlockProperties[ndx].pNext = prev;
            prev                                    = &inlineUniformBlockProperties[ndx];
        }

        if (khr_maintenance4)
        {
            maintenance4Properties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES;
            maintenance4Properties[ndx].pNext = prev;
            prev                              = &maintenance4Properties[ndx];
        }

        if (khr_subgroup_size_control)
        {
            subgroupSizeControlProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
            subgroupSizeControlProperties[ndx].pNext = prev;
            prev                                     = &subgroupSizeControlProperties[ndx];
        }

        if (khr_texel_buffer_alignment)
        {
            texelBufferAlignmentProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES;
            texelBufferAlignmentProperties[ndx].pNext = prev;
            prev                                      = &texelBufferAlignmentProperties[ndx];
        }
#endif // CTS_USES_VULKANSC

        if (prev == 0)
            TCU_THROW(NotSupportedError, "No supported structures found");

        extProperties.pNext = prev;

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
    }

    if (khr_external_fence_capabilities || khr_external_memory_capabilities || khr_external_semaphore_capabilities)
        log << TestLog::Message << idProperties[0] << TestLog::EndMessage;
    if (khr_multiview)
        log << TestLog::Message << multiviewProperties[0] << TestLog::EndMessage;
    if (khr_device_protected_memory)
        log << TestLog::Message << protectedMemoryPropertiesKHR[0] << TestLog::EndMessage;
    if (khr_device_subgroup)
        log << TestLog::Message << subgroupProperties[0] << TestLog::EndMessage;
    if (khr_maintenance2)
        log << TestLog::Message << pointClippingProperties[0] << TestLog::EndMessage;
    if (khr_maintenance3)
        log << TestLog::Message << maintenance3Properties[0] << TestLog::EndMessage;
    if (khr_depth_stencil_resolve)
        log << TestLog::Message << depthStencilResolveProperties[0] << TestLog::EndMessage;
    if (khr_driver_properties)
        log << TestLog::Message << driverProperties[0] << TestLog::EndMessage;
    if (khr_shader_float_controls)
        log << TestLog::Message << floatControlsProperties[0] << TestLog::EndMessage;
    if (khr_descriptor_indexing)
        log << TestLog::Message << descriptorIndexingProperties[0] << TestLog::EndMessage;
    if (khr_sampler_filter_minmax)
        log << TestLog::Message << samplerFilterMinmaxProperties[0] << TestLog::EndMessage;
#ifndef CTS_USES_VULKANSC
    if (khr_integer_dot_product)
        log << TestLog::Message << integerDotProductProperties[0] << TestLog::EndMessage;
    if (khr_acceleration_structure)
        log << TestLog::Message << accelerationStructureProperties[0] << TestLog::EndMessage;
    if (khr_inline_uniform_block)
        log << TestLog::Message << inlineUniformBlockProperties[0] << TestLog::EndMessage;
    if (khr_maintenance4)
        log << TestLog::Message << maintenance4Properties[0] << TestLog::EndMessage;
    if (khr_subgroup_size_control)
        log << TestLog::Message << subgroupSizeControlProperties[0] << TestLog::EndMessage;
    if (khr_texel_buffer_alignment)
        log << TestLog::Message << texelBufferAlignmentProperties[0] << TestLog::EndMessage;
#endif // CTS_USES_VULKANSC

    if (khr_external_fence_capabilities || khr_external_memory_capabilities || khr_external_semaphore_capabilities)
    {
        if ((deMemCmp(idProperties[0].deviceUUID, idProperties[1].deviceUUID, VK_UUID_SIZE) != 0) ||
            (deMemCmp(idProperties[0].driverUUID, idProperties[1].driverUUID, VK_UUID_SIZE) != 0) ||
            (idProperties[0].deviceLUIDValid != idProperties[1].deviceLUIDValid))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
        }
        else if (idProperties[0].deviceLUIDValid)
        {
            // If deviceLUIDValid is VK_FALSE, the contents of deviceLUID and deviceNodeMask are undefined
            // so thay can only be compared when deviceLUIDValid is VK_TRUE.
            if ((deMemCmp(idProperties[0].deviceLUID, idProperties[1].deviceLUID, VK_LUID_SIZE) != 0) ||
                (idProperties[0].deviceNodeMask != idProperties[1].deviceNodeMask))
            {
                TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties");
            }
        }
    }
    if (khr_multiview &&
        (multiviewProperties[0].maxMultiviewViewCount != multiviewProperties[1].maxMultiviewViewCount ||
         multiviewProperties[0].maxMultiviewInstanceIndex != multiviewProperties[1].maxMultiviewInstanceIndex))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewProperties");
    }
    if (khr_device_protected_memory &&
        (protectedMemoryPropertiesKHR[0].protectedNoFault != protectedMemoryPropertiesKHR[1].protectedNoFault))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryProperties");
    }
    if (khr_device_subgroup &&
        (subgroupProperties[0].subgroupSize != subgroupProperties[1].subgroupSize ||
         subgroupProperties[0].supportedStages != subgroupProperties[1].supportedStages ||
         subgroupProperties[0].supportedOperations != subgroupProperties[1].supportedOperations ||
         subgroupProperties[0].quadOperationsInAllStages != subgroupProperties[1].quadOperationsInAllStages))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties");
    }
    if (khr_maintenance2 &&
        (pointClippingProperties[0].pointClippingBehavior != pointClippingProperties[1].pointClippingBehavior))
    {
        TCU_FAIL("Mismatch between VkPhysicalDevicePointClippingProperties");
    }
    if (khr_maintenance3 &&
        (maintenance3Properties[0].maxPerSetDescriptors != maintenance3Properties[1].maxPerSetDescriptors ||
         maintenance3Properties[0].maxMemoryAllocationSize != maintenance3Properties[1].maxMemoryAllocationSize))
    {
        if (protectedMemoryPropertiesKHR[0].protectedNoFault != protectedMemoryPropertiesKHR[1].protectedNoFault)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryProperties");
        }
        if ((subgroupProperties[0].subgroupSize != subgroupProperties[1].subgroupSize) ||
            (subgroupProperties[0].supportedStages != subgroupProperties[1].supportedStages) ||
            (subgroupProperties[0].supportedOperations != subgroupProperties[1].supportedOperations) ||
            (subgroupProperties[0].quadOperationsInAllStages != subgroupProperties[1].quadOperationsInAllStages))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties");
        }
        TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance3Properties");
    }
    if (khr_depth_stencil_resolve &&
        (depthStencilResolveProperties[0].supportedDepthResolveModes !=
             depthStencilResolveProperties[1].supportedDepthResolveModes ||
         depthStencilResolveProperties[0].supportedStencilResolveModes !=
             depthStencilResolveProperties[1].supportedStencilResolveModes ||
         depthStencilResolveProperties[0].independentResolveNone !=
             depthStencilResolveProperties[1].independentResolveNone ||
         depthStencilResolveProperties[0].independentResolve != depthStencilResolveProperties[1].independentResolve))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceDepthStencilResolveProperties");
    }
    if (khr_driver_properties &&
        (driverProperties[0].driverID != driverProperties[1].driverID ||
         strncmp(driverProperties[0].driverName, driverProperties[1].driverName, VK_MAX_DRIVER_NAME_SIZE) != 0 ||
         strncmp(driverProperties[0].driverInfo, driverProperties[1].driverInfo, VK_MAX_DRIVER_INFO_SIZE) != 0 ||
         driverProperties[0].conformanceVersion.major != driverProperties[1].conformanceVersion.major ||
         driverProperties[0].conformanceVersion.minor != driverProperties[1].conformanceVersion.minor ||
         driverProperties[0].conformanceVersion.subminor != driverProperties[1].conformanceVersion.subminor ||
         driverProperties[0].conformanceVersion.patch != driverProperties[1].conformanceVersion.patch))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceDriverProperties");
    }
    if (khr_shader_float_controls &&
        (floatControlsProperties[0].denormBehaviorIndependence !=
             floatControlsProperties[1].denormBehaviorIndependence ||
         floatControlsProperties[0].roundingModeIndependence != floatControlsProperties[1].roundingModeIndependence ||
         floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat16 !=
             floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat16 ||
         floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat32 !=
             floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat32 ||
         floatControlsProperties[0].shaderSignedZeroInfNanPreserveFloat64 !=
             floatControlsProperties[1].shaderSignedZeroInfNanPreserveFloat64 ||
         floatControlsProperties[0].shaderDenormPreserveFloat16 !=
             floatControlsProperties[1].shaderDenormPreserveFloat16 ||
         floatControlsProperties[0].shaderDenormPreserveFloat32 !=
             floatControlsProperties[1].shaderDenormPreserveFloat32 ||
         floatControlsProperties[0].shaderDenormPreserveFloat64 !=
             floatControlsProperties[1].shaderDenormPreserveFloat64 ||
         floatControlsProperties[0].shaderDenormFlushToZeroFloat16 !=
             floatControlsProperties[1].shaderDenormFlushToZeroFloat16 ||
         floatControlsProperties[0].shaderDenormFlushToZeroFloat32 !=
             floatControlsProperties[1].shaderDenormFlushToZeroFloat32 ||
         floatControlsProperties[0].shaderDenormFlushToZeroFloat64 !=
             floatControlsProperties[1].shaderDenormFlushToZeroFloat64 ||
         floatControlsProperties[0].shaderRoundingModeRTEFloat16 !=
             floatControlsProperties[1].shaderRoundingModeRTEFloat16 ||
         floatControlsProperties[0].shaderRoundingModeRTEFloat32 !=
             floatControlsProperties[1].shaderRoundingModeRTEFloat32 ||
         floatControlsProperties[0].shaderRoundingModeRTEFloat64 !=
             floatControlsProperties[1].shaderRoundingModeRTEFloat64 ||
         floatControlsProperties[0].shaderRoundingModeRTZFloat16 !=
             floatControlsProperties[1].shaderRoundingModeRTZFloat16 ||
         floatControlsProperties[0].shaderRoundingModeRTZFloat32 !=
             floatControlsProperties[1].shaderRoundingModeRTZFloat32 ||
         floatControlsProperties[0].shaderRoundingModeRTZFloat64 !=
             floatControlsProperties[1].shaderRoundingModeRTZFloat64))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceFloatControlsProperties");
    }
    if (khr_descriptor_indexing &&
        (descriptorIndexingProperties[0].maxUpdateAfterBindDescriptorsInAllPools !=
             descriptorIndexingProperties[1].maxUpdateAfterBindDescriptorsInAllPools ||
         descriptorIndexingProperties[0].shaderUniformBufferArrayNonUniformIndexingNative !=
             descriptorIndexingProperties[1].shaderUniformBufferArrayNonUniformIndexingNative ||
         descriptorIndexingProperties[0].shaderSampledImageArrayNonUniformIndexingNative !=
             descriptorIndexingProperties[1].shaderSampledImageArrayNonUniformIndexingNative ||
         descriptorIndexingProperties[0].shaderStorageBufferArrayNonUniformIndexingNative !=
             descriptorIndexingProperties[1].shaderStorageBufferArrayNonUniformIndexingNative ||
         descriptorIndexingProperties[0].shaderStorageImageArrayNonUniformIndexingNative !=
             descriptorIndexingProperties[1].shaderStorageImageArrayNonUniformIndexingNative ||
         descriptorIndexingProperties[0].shaderInputAttachmentArrayNonUniformIndexingNative !=
             descriptorIndexingProperties[1].shaderInputAttachmentArrayNonUniformIndexingNative ||
         descriptorIndexingProperties[0].robustBufferAccessUpdateAfterBind !=
             descriptorIndexingProperties[1].robustBufferAccessUpdateAfterBind ||
         descriptorIndexingProperties[0].quadDivergentImplicitLod !=
             descriptorIndexingProperties[1].quadDivergentImplicitLod ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindSamplers !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindSamplers ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindUniformBuffers !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindUniformBuffers ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindStorageBuffers !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindStorageBuffers ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindSampledImages !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindSampledImages ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindStorageImages !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindStorageImages ||
         descriptorIndexingProperties[0].maxPerStageDescriptorUpdateAfterBindInputAttachments !=
             descriptorIndexingProperties[1].maxPerStageDescriptorUpdateAfterBindInputAttachments ||
         descriptorIndexingProperties[0].maxPerStageUpdateAfterBindResources !=
             descriptorIndexingProperties[1].maxPerStageUpdateAfterBindResources ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindSamplers !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindSamplers ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindUniformBuffers !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindUniformBuffers ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindUniformBuffersDynamic !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindUniformBuffersDynamic ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageBuffers !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageBuffers ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageBuffersDynamic !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageBuffersDynamic ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindSampledImages !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindSampledImages ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindStorageImages !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindStorageImages ||
         descriptorIndexingProperties[0].maxDescriptorSetUpdateAfterBindInputAttachments !=
             descriptorIndexingProperties[1].maxDescriptorSetUpdateAfterBindInputAttachments))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceDescriptorIndexingProperties");
    }
    if (khr_sampler_filter_minmax && (samplerFilterMinmaxProperties[0].filterMinmaxSingleComponentFormats !=
                                          samplerFilterMinmaxProperties[1].filterMinmaxSingleComponentFormats ||
                                      samplerFilterMinmaxProperties[0].filterMinmaxImageComponentMapping !=
                                          samplerFilterMinmaxProperties[1].filterMinmaxImageComponentMapping))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerFilterMinmaxProperties");
    }

#ifndef CTS_USES_VULKANSC

    if (khr_integer_dot_product &&
        (integerDotProductProperties[0].integerDotProduct8BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct8BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct8BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct8BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct8BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProduct8BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProduct4x8BitPackedUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct4x8BitPackedUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct4x8BitPackedSignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct4x8BitPackedSignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct4x8BitPackedMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProduct4x8BitPackedMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProduct16BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct16BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct16BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct16BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct16BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProduct16BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProduct32BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct32BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct32BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct32BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct32BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProduct32BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProduct64BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct64BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct64BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProduct64BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProduct64BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProduct64BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated !=
             integerDotProductProperties[1]
                 .integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitUnsignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitUnsignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitSignedAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitSignedAccelerated ||
         integerDotProductProperties[0].integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated !=
             integerDotProductProperties[1].integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceShaderIntegerDotProductPropertiesKHR");
    }

    if (khr_texel_buffer_alignment)
    {
        if (texelBufferAlignmentProperties[0].storageTexelBufferOffsetAlignmentBytes !=
                texelBufferAlignmentProperties[1].storageTexelBufferOffsetAlignmentBytes ||
            texelBufferAlignmentProperties[0].storageTexelBufferOffsetSingleTexelAlignment !=
                texelBufferAlignmentProperties[1].storageTexelBufferOffsetSingleTexelAlignment ||
            texelBufferAlignmentProperties[0].uniformTexelBufferOffsetAlignmentBytes !=
                texelBufferAlignmentProperties[1].uniformTexelBufferOffsetAlignmentBytes ||
            texelBufferAlignmentProperties[0].uniformTexelBufferOffsetSingleTexelAlignment !=
                texelBufferAlignmentProperties[1].uniformTexelBufferOffsetSingleTexelAlignment)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT");
        }

        if (texelBufferAlignmentProperties[0].storageTexelBufferOffsetAlignmentBytes == 0 ||
            !deIntIsPow2((int)texelBufferAlignmentProperties[0].storageTexelBufferOffsetAlignmentBytes))
        {
            TCU_FAIL("limit Validation failed storageTexelBufferOffsetAlignmentBytes is not a power of two.");
        }

        if (texelBufferAlignmentProperties[0].uniformTexelBufferOffsetAlignmentBytes == 0 ||
            !deIntIsPow2((int)texelBufferAlignmentProperties[0].uniformTexelBufferOffsetAlignmentBytes))
        {
            TCU_FAIL("limit Validation failed uniformTexelBufferOffsetAlignmentBytes is not a power of two.");
        }
    }

    if (khr_inline_uniform_block &&
        (inlineUniformBlockProperties[0].maxInlineUniformBlockSize !=
             inlineUniformBlockProperties[1].maxInlineUniformBlockSize ||
         inlineUniformBlockProperties[0].maxPerStageDescriptorInlineUniformBlocks !=
             inlineUniformBlockProperties[1].maxPerStageDescriptorInlineUniformBlocks ||
         inlineUniformBlockProperties[0].maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks !=
             inlineUniformBlockProperties[1].maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks ||
         inlineUniformBlockProperties[0].maxDescriptorSetInlineUniformBlocks !=
             inlineUniformBlockProperties[1].maxDescriptorSetInlineUniformBlocks ||
         inlineUniformBlockProperties[0].maxDescriptorSetUpdateAfterBindInlineUniformBlocks !=
             inlineUniformBlockProperties[1].maxDescriptorSetUpdateAfterBindInlineUniformBlocks))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceInlineUniformBlockProperties");
    }
    if (khr_maintenance4 && (maintenance4Properties[0].maxBufferSize != maintenance4Properties[1].maxBufferSize))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance4Properties");
    }
    if (khr_subgroup_size_control &&
        (subgroupSizeControlProperties[0].minSubgroupSize != subgroupSizeControlProperties[1].minSubgroupSize ||
         subgroupSizeControlProperties[0].maxSubgroupSize != subgroupSizeControlProperties[1].maxSubgroupSize ||
         subgroupSizeControlProperties[0].maxComputeWorkgroupSubgroups !=
             subgroupSizeControlProperties[1].maxComputeWorkgroupSubgroups ||
         subgroupSizeControlProperties[0].requiredSubgroupSizeStages !=
             subgroupSizeControlProperties[1].requiredSubgroupSizeStages))
    {
        TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupSizeControlProperties");
    }

    if (khr_acceleration_structure)
    {
        if (accelerationStructureProperties[0].maxGeometryCount !=
                accelerationStructureProperties[1].maxGeometryCount ||
            accelerationStructureProperties[0].maxInstanceCount !=
                accelerationStructureProperties[1].maxInstanceCount ||
            accelerationStructureProperties[0].maxPrimitiveCount !=
                accelerationStructureProperties[1].maxPrimitiveCount ||
            accelerationStructureProperties[0].maxPerStageDescriptorAccelerationStructures !=
                accelerationStructureProperties[1].maxPerStageDescriptorAccelerationStructures ||
            accelerationStructureProperties[0].maxPerStageDescriptorUpdateAfterBindAccelerationStructures !=
                accelerationStructureProperties[1].maxPerStageDescriptorUpdateAfterBindAccelerationStructures ||
            accelerationStructureProperties[0].maxDescriptorSetAccelerationStructures !=
                accelerationStructureProperties[1].maxDescriptorSetAccelerationStructures ||
            accelerationStructureProperties[0].maxDescriptorSetUpdateAfterBindAccelerationStructures !=
                accelerationStructureProperties[1].maxDescriptorSetUpdateAfterBindAccelerationStructures ||
            accelerationStructureProperties[0].minAccelerationStructureScratchOffsetAlignment !=
                accelerationStructureProperties[1].minAccelerationStructureScratchOffsetAlignment)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceAccelerationStructurePropertiesKHR");
        }

        if (accelerationStructureProperties[0].minAccelerationStructureScratchOffsetAlignment == 0 ||
            !deIntIsPow2(accelerationStructureProperties[0].minAccelerationStructureScratchOffsetAlignment))
        {
            TCU_FAIL("limit Validation failed minAccelerationStructureScratchOffsetAlignment is not a power of two.");
        }
    }

    if (isExtensionStructSupported(properties, RequiredExtension("VK_KHR_push_descriptor")))
    {
        VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties[count];

        for (int ndx = 0; ndx < count; ++ndx)
        {
            deMemset(&pushDescriptorProperties[ndx], 0xFF * ndx, sizeof(VkPhysicalDevicePushDescriptorPropertiesKHR));

            pushDescriptorProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
            pushDescriptorProperties[ndx].pNext = nullptr;

            extProperties.pNext = &pushDescriptorProperties[ndx];

            vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

            pushDescriptorProperties[ndx].pNext = nullptr;
        }

        log << TestLog::Message << pushDescriptorProperties[0] << TestLog::EndMessage;

        if (pushDescriptorProperties[0].maxPushDescriptors != pushDescriptorProperties[1].maxPushDescriptors)
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePushDescriptorPropertiesKHR ");
        }
        if (pushDescriptorProperties[0].maxPushDescriptors < 32)
        {
            TCU_FAIL("VkPhysicalDevicePushDescriptorPropertiesKHR.maxPushDescriptors must be at least 32");
        }
    }

    if (isExtensionStructSupported(properties, RequiredExtension("VK_KHR_performance_query")))
    {
        VkPhysicalDevicePerformanceQueryPropertiesKHR performanceQueryProperties[count];

        for (int ndx = 0; ndx < count; ++ndx)
        {
            deMemset(&performanceQueryProperties[ndx], 0xFF * ndx,
                     sizeof(VkPhysicalDevicePerformanceQueryPropertiesKHR));
            performanceQueryProperties[ndx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR;
            performanceQueryProperties[ndx].pNext = nullptr;

            extProperties.pNext = &performanceQueryProperties[ndx];

            vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
        }

        log << TestLog::Message << performanceQueryProperties[0] << TestLog::EndMessage;

        if (performanceQueryProperties[0].allowCommandBufferQueryCopies !=
            performanceQueryProperties[1].allowCommandBufferQueryCopies)
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePerformanceQueryPropertiesKHR");
        }
    }

#endif // CTS_USES_VULKANSC

    if (isExtensionStructSupported(properties, RequiredExtension("VK_EXT_pci_bus_info", 2, 2)))
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
            pciBusInfoProperties[ndx].pNext = nullptr;

            extProperties.pNext = pciBusInfoProperties + ndx;
            vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
        }

        log << TestLog::Message << toString(pciBusInfoProperties[0]) << TestLog::EndMessage;

        if (pciBusInfoProperties[0].pciDomain != pciBusInfoProperties[1].pciDomain ||
            pciBusInfoProperties[0].pciBus != pciBusInfoProperties[1].pciBus ||
            pciBusInfoProperties[0].pciDevice != pciBusInfoProperties[1].pciDevice ||
            pciBusInfoProperties[0].pciFunction != pciBusInfoProperties[1].pciFunction)
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePCIBusInfoPropertiesEXT");
        }
        if (pciBusInfoProperties[0].pciDomain == DEUINT32_MAX || pciBusInfoProperties[0].pciBus == DEUINT32_MAX ||
            pciBusInfoProperties[0].pciDevice == DEUINT32_MAX || pciBusInfoProperties[0].pciFunction == DEUINT32_MAX)
        {
            TCU_FAIL("Invalid information in VkPhysicalDevicePCIBusInfoPropertiesEXT");
        }
    }

#ifndef CTS_USES_VULKANSC
    if (isExtensionStructSupported(properties, RequiredExtension("VK_KHR_portability_subset")))
    {
        VkPhysicalDevicePortabilitySubsetPropertiesKHR portabilitySubsetProperties[count];

        for (int ndx = 0; ndx < count; ++ndx)
        {
            deMemset(&portabilitySubsetProperties[ndx], 0xFF * ndx,
                     sizeof(VkPhysicalDevicePortabilitySubsetPropertiesKHR));
            portabilitySubsetProperties[ndx].sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR;
            portabilitySubsetProperties[ndx].pNext = nullptr;

            extProperties.pNext = &portabilitySubsetProperties[ndx];

            vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
        }

        log << TestLog::Message << portabilitySubsetProperties[0] << TestLog::EndMessage;

        if (portabilitySubsetProperties[0].minVertexInputBindingStrideAlignment !=
            portabilitySubsetProperties[1].minVertexInputBindingStrideAlignment)
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePortabilitySubsetPropertiesKHR");
        }

        if (portabilitySubsetProperties[0].minVertexInputBindingStrideAlignment == 0 ||
            !deIntIsPow2(portabilitySubsetProperties[0].minVertexInputBindingStrideAlignment))
        {
            TCU_FAIL("limit Validation failed minVertexInputBindingStrideAlignment is not a power of two.");
        }
    }
#endif // CTS_USES_VULKANSC

    return tcu::TestStatus::pass("Querying device properties succeeded");
}

string toString(const VkFormatProperties2 &value)
{
    std::ostringstream s;
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

tcu::TestStatus deviceFormatProperties2(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    TestLog &log                          = context.getTestContext().getLog();

    for (int formatNdx = 0; formatNdx < VK_CORE_FORMAT_LAST; ++formatNdx)
    {
        const VkFormat format = (VkFormat)formatNdx;
        VkFormatProperties coreProperties;
        VkFormatProperties2 extProperties;

        deMemset(&coreProperties, 0xcd, sizeof(VkFormatProperties));
        deMemset(&extProperties, 0xcd, sizeof(VkFormatProperties2));

        extProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        extProperties.pNext = nullptr;

        vki.getPhysicalDeviceFormatProperties(physicalDevice, format, &coreProperties);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &extProperties);

        TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);

        if (format == vk::VK_FORMAT_UNDEFINED)
        {
            VkFormatProperties2 formatUndefProperties2;

            deMemset(&formatUndefProperties2, 0xcd, sizeof(VkFormatProperties2));
            formatUndefProperties2.sType                                  = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            formatUndefProperties2.pNext                                  = nullptr;
            formatUndefProperties2.formatProperties.bufferFeatures        = 0;
            formatUndefProperties2.formatProperties.linearTilingFeatures  = 0;
            formatUndefProperties2.formatProperties.optimalTilingFeatures = 0;

            if (deMemCmp(&formatUndefProperties2, &extProperties, sizeof(VkFormatProperties2)) != 0)
                TCU_FAIL("vkGetPhysicalDeviceFormatProperties2, with VK_FORMAT_UNDEFINED as input format, is returning "
                         "non-zero properties");
        }
        else
            TCU_CHECK(extProperties.pNext == nullptr);

        if (deMemCmp(&coreProperties, &extProperties.formatProperties, sizeof(VkFormatProperties)) != 0)
            TCU_FAIL("Mismatch between format properties reported by vkGetPhysicalDeviceFormatProperties and "
                     "vkGetPhysicalDeviceFormatProperties2");

        log << TestLog::Message << toString(extProperties) << TestLog::EndMessage;
    }

    return tcu::TestStatus::pass("Querying device format properties succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties2(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    TestLog &log                          = context.getTestContext().getLog();
    uint32_t numCoreQueueFamilies         = ~0u;
    uint32_t numExtQueueFamilies          = ~0u;

    vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numCoreQueueFamilies, nullptr);
    vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numExtQueueFamilies, nullptr);

    TCU_CHECK_MSG(numCoreQueueFamilies == numExtQueueFamilies, "Different number of queue family properties reported");
    TCU_CHECK(numCoreQueueFamilies > 0);

    {
        std::vector<VkQueueFamilyProperties> coreProperties(numCoreQueueFamilies);
        std::vector<VkQueueFamilyProperties2> extProperties(numExtQueueFamilies);

        deMemset(&coreProperties[0], 0xcd, sizeof(VkQueueFamilyProperties) * numCoreQueueFamilies);
        deMemset(&extProperties[0], 0xcd, sizeof(VkQueueFamilyProperties2) * numExtQueueFamilies);

        for (size_t ndx = 0; ndx < extProperties.size(); ++ndx)
        {
            extProperties[ndx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            extProperties[ndx].pNext = nullptr;
        }

        vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numCoreQueueFamilies, &coreProperties[0]);
        vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numExtQueueFamilies, &extProperties[0]);

        TCU_CHECK((size_t)numCoreQueueFamilies == coreProperties.size());
        TCU_CHECK((size_t)numExtQueueFamilies == extProperties.size());
        DE_ASSERT(numCoreQueueFamilies == numExtQueueFamilies);

        for (size_t ndx = 0; ndx < extProperties.size(); ++ndx)
        {
            TCU_CHECK(extProperties[ndx].sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
            TCU_CHECK(extProperties[ndx].pNext == nullptr);

            if (deMemCmp(&coreProperties[ndx], &extProperties[ndx].queueFamilyProperties,
                         sizeof(VkQueueFamilyProperties)) != 0)
                TCU_FAIL("Mismatch between format properties reported by vkGetPhysicalDeviceQueueFamilyProperties and "
                         "vkGetPhysicalDeviceQueueFamilyProperties2");

            log << TestLog::Message << " queueFamilyNdx = " << ndx << TestLog::EndMessage << TestLog::Message
                << extProperties[ndx] << TestLog::EndMessage;
        }
    }

    return tcu::TestStatus::pass("Querying device queue family properties succeeded");
}

tcu::TestStatus deviceMemoryProperties2(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    TestLog &log                          = context.getTestContext().getLog();
    VkPhysicalDeviceMemoryProperties coreProperties;
    VkPhysicalDeviceMemoryProperties2 extProperties;

    deMemset(&coreProperties, 0xcd, sizeof(VkPhysicalDeviceMemoryProperties));
    deMemset(&extProperties, 0xcd, sizeof(VkPhysicalDeviceMemoryProperties2));

    extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    extProperties.pNext = nullptr;

    vki.getPhysicalDeviceMemoryProperties(physicalDevice, &coreProperties);
    vki.getPhysicalDeviceMemoryProperties2(physicalDevice, &extProperties);

    TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
    TCU_CHECK(extProperties.pNext == nullptr);

    if (coreProperties.memoryTypeCount != extProperties.memoryProperties.memoryTypeCount)
        TCU_FAIL("Mismatch between memoryTypeCount reported by vkGetPhysicalDeviceMemoryProperties and "
                 "vkGetPhysicalDeviceMemoryProperties2");
    if (coreProperties.memoryHeapCount != extProperties.memoryProperties.memoryHeapCount)
        TCU_FAIL("Mismatch between memoryHeapCount reported by vkGetPhysicalDeviceMemoryProperties and "
                 "vkGetPhysicalDeviceMemoryProperties2");
    for (uint32_t i = 0; i < coreProperties.memoryTypeCount; i++)
    {
        const VkMemoryType *coreType = &coreProperties.memoryTypes[i];
        const VkMemoryType *extType  = &extProperties.memoryProperties.memoryTypes[i];
        if (coreType->propertyFlags != extType->propertyFlags || coreType->heapIndex != extType->heapIndex)
            TCU_FAIL("Mismatch between memoryTypes reported by vkGetPhysicalDeviceMemoryProperties and "
                     "vkGetPhysicalDeviceMemoryProperties2");
    }
    for (uint32_t i = 0; i < coreProperties.memoryHeapCount; i++)
    {
        const VkMemoryHeap *coreHeap = &coreProperties.memoryHeaps[i];
        const VkMemoryHeap *extHeap  = &extProperties.memoryProperties.memoryHeaps[i];
        if (coreHeap->size != extHeap->size || coreHeap->flags != extHeap->flags)
            TCU_FAIL("Mismatch between memoryHeaps reported by vkGetPhysicalDeviceMemoryProperties and "
                     "vkGetPhysicalDeviceMemoryProperties2");
    }

    log << TestLog::Message << extProperties << TestLog::EndMessage;

    return tcu::TestStatus::pass("Querying device memory properties succeeded");
}

tcu::TestStatus deviceFeaturesVulkan12(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry feature11OffsetTable[] = {
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
        {0, 0}};
    const QueryMemberTableEntry feature12OffsetTable[] = {
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
        {0, 0}};
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const uint32_t vulkan11FeaturesBufferSize = sizeof(VkPhysicalDeviceVulkan11Features) + GUARD_SIZE;
    const uint32_t vulkan12FeaturesBufferSize = sizeof(VkPhysicalDeviceVulkan12Features) + GUARD_SIZE;
    VkPhysicalDeviceFeatures2 extFeatures;
    uint8_t buffer11a[vulkan11FeaturesBufferSize];
    uint8_t buffer11b[vulkan11FeaturesBufferSize];
    uint8_t buffer12a[vulkan12FeaturesBufferSize];
    uint8_t buffer12b[vulkan12FeaturesBufferSize];
    const int count                                           = 2u;
    VkPhysicalDeviceVulkan11Features *vulkan11Features[count] = {(VkPhysicalDeviceVulkan11Features *)(buffer11a),
                                                                 (VkPhysicalDeviceVulkan11Features *)(buffer11b)};
    VkPhysicalDeviceVulkan12Features *vulkan12Features[count] = {(VkPhysicalDeviceVulkan12Features *)(buffer12a),
                                                                 (VkPhysicalDeviceVulkan12Features *)(buffer12b)};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 2, 0)))
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
        vulkan12Features[ndx]->pNext = nullptr;

        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);
    }

    log << TestLog::Message << *vulkan11Features[0] << TestLog::EndMessage;
    log << TestLog::Message << *vulkan12Features[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(feature11OffsetTable, vulkan11Features, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan11Features initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan11Features initialization failure");
    }

    if (!validateStructsWithGuard(feature12OffsetTable, vulkan12Features, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan12Features initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan12Features initialization failure");
    }

    return tcu::TestStatus::pass("Querying Vulkan 1.2 device features succeeded");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus deviceFeaturesVulkan13(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry feature13OffsetTable[] = {
        // VkPhysicalDeviceImageRobustnessFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, robustImageAccess),

        // VkPhysicalDeviceInlineUniformBlockFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, inlineUniformBlock),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, descriptorBindingInlineUniformBlockUpdateAfterBind),

        // VkPhysicalDevicePipelineCreationCacheControlFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, pipelineCreationCacheControl),

        // VkPhysicalDevicePrivateDataFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, privateData),

        // VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, shaderDemoteToHelperInvocation),

        // VkPhysicalDeviceShaderTerminateInvocationFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, shaderTerminateInvocation),

        // VkPhysicalDeviceSubgroupSizeControlFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, subgroupSizeControl),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, computeFullSubgroups),

        // VkPhysicalDeviceSynchronization2Features
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, synchronization2),

        // VkPhysicalDeviceTextureCompressionASTCHDRFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, textureCompressionASTC_HDR),

        // VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, shaderZeroInitializeWorkgroupMemory),

        // VkPhysicalDeviceDynamicRenderingFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, dynamicRendering),

        // VkPhysicalDeviceShaderIntegerDotProductFeatures
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, shaderIntegerDotProduct),

        // VkPhysicalDeviceMaintenance4Features
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Features, maintenance4),
        {0, 0}};
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const uint32_t vulkan13FeaturesBufferSize = sizeof(VkPhysicalDeviceVulkan13Features) + GUARD_SIZE;
    VkPhysicalDeviceFeatures2 extFeatures;
    uint8_t buffer13a[vulkan13FeaturesBufferSize];
    uint8_t buffer13b[vulkan13FeaturesBufferSize];
    const int count                                           = 2u;
    VkPhysicalDeviceVulkan13Features *vulkan13Features[count] = {(VkPhysicalDeviceVulkan13Features *)(buffer13a),
                                                                 (VkPhysicalDeviceVulkan13Features *)(buffer13b)};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 3, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.3 required to run test");

    deMemset(buffer13a, GUARD_VALUE, sizeof(buffer13a));
    deMemset(buffer13b, GUARD_VALUE, sizeof(buffer13b));

    // Validate all fields initialized
    for (int ndx = 0; ndx < count; ++ndx)
    {
        deMemset(&extFeatures.features, 0x00, sizeof(extFeatures.features));
        extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        extFeatures.pNext = vulkan13Features[ndx];

        deMemset(vulkan13Features[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan13Features));
        vulkan13Features[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features[ndx]->pNext = nullptr;

        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);
    }

    log << TestLog::Message << *vulkan13Features[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(feature13OffsetTable, vulkan13Features, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan13Features initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan13Features initialization failure");
    }

    return tcu::TestStatus::pass("Querying Vulkan 1.3 device features succeeded");
}

tcu::TestStatus deviceFeaturesVulkan14(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry feature14OffsetTable[] = {

        // VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, dynamicRenderingLocalRead),

        // VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, globalPriorityQuery),

        // VkPhysicalDeviceIndexTypeUint8FeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, indexTypeUint8),

        // VkPhysicalDeviceLineRasterizationFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, rectangularLines),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, bresenhamLines),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, smoothLines),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, stippledRectangularLines),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, stippledBresenhamLines),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, stippledSmoothLines),

        // VkPhysicalDeviceMaintenance5FeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, maintenance5),

        // VkPhysicalDeviceMaintenance6FeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, maintenance6),

        // VkPhysicalDeviceShaderExpectAssumeFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, shaderExpectAssume),

        // VkPhysicalDeviceShaderFloatControls2FeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, shaderFloatControls2),

        // VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, shaderSubgroupRotate),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, shaderSubgroupRotateClustered),

        // VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, vertexAttributeInstanceRateDivisor),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, vertexAttributeInstanceRateZeroDivisor),

        // VkPhysicalDeviceHostImageCopyFeaturesEXT
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, hostImageCopy),

        // VkPhysicalDevicePipelineProtectedAccessFeaturesEXT
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, pipelineProtectedAccess),

        // VkPhysicalDevicePipelineRobustnessFeaturesEXT
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Features, pipelineRobustness),
        {0, 0}};
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const uint32_t vulkan14FeaturesBufferSize = sizeof(VkPhysicalDeviceVulkan14Features) + GUARD_SIZE;
    VkPhysicalDeviceFeatures2 extFeatures;
    uint8_t buffer14a[vulkan14FeaturesBufferSize];
    uint8_t buffer14b[vulkan14FeaturesBufferSize];
    const int count                                           = 2u;
    VkPhysicalDeviceVulkan14Features *vulkan14Features[count] = {(VkPhysicalDeviceVulkan14Features *)(buffer14a),
                                                                 (VkPhysicalDeviceVulkan14Features *)(buffer14b)};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 4, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.4 required to run test");

    deMemset(buffer14a, GUARD_VALUE, sizeof(buffer14a));
    deMemset(buffer14b, GUARD_VALUE, sizeof(buffer14b));

    // Validate all fields initialized
    for (int ndx = 0; ndx < count; ++ndx)
    {
        deMemset(&extFeatures.features, 0x00, sizeof(extFeatures.features));
        extFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        extFeatures.pNext = vulkan14Features[ndx];

        deMemset(vulkan14Features[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan14Features));
        vulkan14Features[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vulkan14Features[ndx]->pNext = nullptr;

        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);
    }

    log << TestLog::Message << *vulkan14Features[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(feature14OffsetTable, vulkan14Features, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceVulkan14Features initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan14Features initialization failure");
    }

    return tcu::TestStatus::pass("Querying Vulkan 1.4 device features succeeded");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus devicePropertiesVulkan12(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry properties11OffsetTable[] = {
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
        {0, 0}};
    const QueryMemberTableEntry properties12OffsetTable[] = {
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
        {0, 0}};
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const uint32_t vulkan11PropertiesBufferSize = sizeof(VkPhysicalDeviceVulkan11Properties) + GUARD_SIZE;
    const uint32_t vulkan12PropertiesBufferSize = sizeof(VkPhysicalDeviceVulkan12Properties) + GUARD_SIZE;
    VkPhysicalDeviceProperties2 extProperties;
    uint8_t buffer11a[vulkan11PropertiesBufferSize];
    uint8_t buffer11b[vulkan11PropertiesBufferSize];
    uint8_t buffer12a[vulkan12PropertiesBufferSize];
    uint8_t buffer12b[vulkan12PropertiesBufferSize];
    const int count                                               = 2u;
    VkPhysicalDeviceVulkan11Properties *vulkan11Properties[count] = {(VkPhysicalDeviceVulkan11Properties *)(buffer11a),
                                                                     (VkPhysicalDeviceVulkan11Properties *)(buffer11b)};
    VkPhysicalDeviceVulkan12Properties *vulkan12Properties[count] = {(VkPhysicalDeviceVulkan12Properties *)(buffer12a),
                                                                     (VkPhysicalDeviceVulkan12Properties *)(buffer12b)};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 2, 0)))
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
        vulkan12Properties[ndx]->pNext = nullptr;

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
    }

    log << TestLog::Message << *vulkan11Properties[0] << TestLog::EndMessage;
    log << TestLog::Message << *vulkan12Properties[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(properties11OffsetTable, vulkan11Properties, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan11Properties initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan11Properties initialization failure");
    }

    if (!validateStructsWithGuard(properties12OffsetTable, vulkan12Properties, GUARD_VALUE, GUARD_SIZE) ||
        strncmp(vulkan12Properties[0]->driverName, vulkan12Properties[1]->driverName, VK_MAX_DRIVER_NAME_SIZE) != 0 ||
        strncmp(vulkan12Properties[0]->driverInfo, vulkan12Properties[1]->driverInfo, VK_MAX_DRIVER_INFO_SIZE) != 0)
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan12Properties initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan12Properties initialization failure");
    }

    return tcu::TestStatus::pass("Querying Vulkan 1.2 device properties succeeded");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus devicePropertiesVulkan13(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry properties13OffsetTable[] = {
        // VkPhysicalDeviceSubgroupSizeControlProperties
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, minSubgroupSize),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxSubgroupSize),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxComputeWorkgroupSubgroups),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, requiredSubgroupSizeStages),

        // VkPhysicalDeviceInlineUniformBlockProperties
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxInlineUniformBlockSize),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxPerStageDescriptorInlineUniformBlocks),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxDescriptorSetInlineUniformBlocks),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxDescriptorSetUpdateAfterBindInlineUniformBlocks),

        // None
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxInlineUniformTotalSize),

        // VkPhysicalDeviceShaderIntegerDotProductProperties
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct8BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct8BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct8BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct4x8BitPackedUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct4x8BitPackedSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct4x8BitPackedMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct16BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct16BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct16BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct32BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct32BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct32BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct64BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct64BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, integerDotProduct64BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating8BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating8BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating16BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating16BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating32BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating32BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating64BitUnsignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating64BitSignedAccelerated),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties,
                           integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated),

        // VkPhysicalDeviceTexelBufferAlignmentProperties
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, storageTexelBufferOffsetAlignmentBytes),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, storageTexelBufferOffsetSingleTexelAlignment),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, uniformTexelBufferOffsetAlignmentBytes),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, uniformTexelBufferOffsetSingleTexelAlignment),

        // VkPhysicalDeviceMaintenance4Properties
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan13Properties, maxBufferSize),
        {0, 0}};

    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const uint32_t vulkan13PropertiesBufferSize = sizeof(VkPhysicalDeviceVulkan13Properties) + GUARD_SIZE;
    VkPhysicalDeviceProperties2 extProperties;
    uint8_t buffer13a[vulkan13PropertiesBufferSize];
    uint8_t buffer13b[vulkan13PropertiesBufferSize];
    const int count                                               = 2u;
    VkPhysicalDeviceVulkan13Properties *vulkan13Properties[count] = {(VkPhysicalDeviceVulkan13Properties *)(buffer13a),
                                                                     (VkPhysicalDeviceVulkan13Properties *)(buffer13b)};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 3, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.3 required to run test");

    deMemset(buffer13a, GUARD_VALUE, sizeof(buffer13a));
    deMemset(buffer13b, GUARD_VALUE, sizeof(buffer13b));

    for (int ndx = 0; ndx < count; ++ndx)
    {
        deMemset(&extProperties.properties, 0x00, sizeof(extProperties.properties));
        extProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        extProperties.pNext = vulkan13Properties[ndx];

        deMemset(vulkan13Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan13Properties));
        vulkan13Properties[ndx]->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
        vulkan13Properties[ndx]->pNext = nullptr;

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
    }

    log << TestLog::Message << *vulkan13Properties[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(properties13OffsetTable, vulkan13Properties, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan13Properties initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan13Properties initialization failure");
    }

    return tcu::TestStatus::pass("Querying Vulkan 1.3 device properties succeeded");
}

tcu::TestStatus devicePropertiesVulkan14(Context &context)
{
    using namespace ValidateQueryBits;

    const QueryMemberTableEntry properties14OffsetTable[] = {

        // VkPhysicalDeviceLineRasterizationPropertiesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, lineSubPixelPrecisionBits),

        // VkPhysicalDeviceMaintenance5PropertiesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, earlyFragmentMultisampleCoverageAfterSampleCounting),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, earlyFragmentSampleMaskTestBeforeSampleCounting),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, depthStencilSwizzleOneSupport),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, polygonModePointSize),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, nonStrictSinglePixelWideLinesUseParallelogram),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, nonStrictWideLinesUseParallelogram),

        // VkPhysicalDeviceMaintenance6PropertiesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, blockTexelViewCompatibleMultipleLayers),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, maxCombinedImageSamplerDescriptorCount),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, fragmentShadingRateClampCombinerInputs),

        // VkPhysicalDevicePushDescriptorPropertiesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, maxPushDescriptors),

        // VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, maxVertexAttribDivisor),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, supportsNonZeroFirstInstance),

        // VkPhysicalDeviceHostImageCopyPropertiesEXT
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, copySrcLayoutCount),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, copyDstLayoutCount),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, optimalTilingLayoutUUID),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, identicalMemoryTypeRequirements),

        // VkPhysicalDevicePipelineRobustnessPropertiesEXT
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, defaultRobustnessStorageBuffers),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, defaultRobustnessUniformBuffers),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, defaultRobustnessVertexInputs),
        OFFSET_TABLE_ENTRY(VkPhysicalDeviceVulkan14Properties, defaultRobustnessImages),
        {0, 0}};

    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const uint32_t vulkan14PropertiesBufferSize = sizeof(VkPhysicalDeviceVulkan14Properties) + GUARD_SIZE;
    VkPhysicalDeviceProperties2 extProperties;
    uint8_t buffer14a[vulkan14PropertiesBufferSize];
    uint8_t buffer14b[vulkan14PropertiesBufferSize];
    const int count                                               = 2u;
    VkPhysicalDeviceVulkan14Properties *vulkan14Properties[count] = {(VkPhysicalDeviceVulkan14Properties *)(buffer14a),
                                                                     (VkPhysicalDeviceVulkan14Properties *)(buffer14b)};
    std::vector<VkImageLayout> copyLayouts[count];

    if (!context.contextSupports(vk::ApiVersion(0, 1, 4, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.4 required to run test");

    deMemset(buffer14a, GUARD_VALUE, sizeof(buffer14a));
    deMemset(buffer14b, GUARD_VALUE, sizeof(buffer14b));

    for (int ndx = 0; ndx < count; ++ndx)
    {
        deMemset(vulkan14Properties[ndx], 0xFF * ndx, sizeof(VkPhysicalDeviceVulkan14Properties));
        vulkan14Properties[ndx]->sType           = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
        vulkan14Properties[ndx]->pNext           = nullptr;
        vulkan14Properties[ndx]->pCopySrcLayouts = nullptr;
        vulkan14Properties[ndx]->pCopyDstLayouts = nullptr;

        extProperties = initVulkanStructure(vulkan14Properties[ndx]);

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

        // safety check in case large array counts are returned
        if ((vulkan14Properties[ndx]->copyDstLayoutCount + vulkan14Properties[ndx]->copySrcLayoutCount) > GUARD_VALUE)
            return tcu::TestStatus::fail("Wrong layouts count");

        // set pCopySrcLayouts / pCopyDstLayouts to allocated array and query again
        copyLayouts[ndx].resize(vulkan14Properties[ndx]->copyDstLayoutCount +
                                vulkan14Properties[ndx]->copySrcLayoutCount);
        vulkan14Properties[ndx]->pCopySrcLayouts = copyLayouts[ndx].data();
        vulkan14Properties[ndx]->pCopyDstLayouts =
            copyLayouts[ndx].data() + vulkan14Properties[ndx]->copySrcLayoutCount;
        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);
    }

    log << TestLog::Message << *vulkan14Properties[0] << TestLog::EndMessage;

    if (!validateStructsWithGuard(properties14OffsetTable, vulkan14Properties, GUARD_VALUE, GUARD_SIZE))
    {
        log << TestLog::Message << "deviceProperties - VkPhysicalDeviceVulkan14Properties initialization failure"
            << TestLog::EndMessage;

        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan14Properties initialization failure");
    }

    // validation of pCopySrcLayouts / pCopyDstLayouts needs to be done separately
    // because the size of those arrays is not know at compile time
    if ((deMemCmp(vulkan14Properties[0]->pCopySrcLayouts, vulkan14Properties[1]->pCopySrcLayouts,
                  vulkan14Properties[0]->copySrcLayoutCount * sizeof(VkImageLayout)) != 0) ||
        (deMemCmp(vulkan14Properties[0]->pCopyDstLayouts, vulkan14Properties[1]->pCopyDstLayouts,
                  vulkan14Properties[0]->copyDstLayoutCount * sizeof(VkImageLayout)) != 0))
        return tcu::TestStatus::fail("VkPhysicalDeviceVulkan14Properties initialization failure");

    return tcu::TestStatus::pass("Querying Vulkan 1.4 device properties succeeded");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus deviceFeatureExtensionsConsistencyVulkan12(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 2, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure();
    VkPhysicalDeviceVulkan11Features vulkan11Features = initVulkanStructure(&vulkan12Features);
    VkPhysicalDeviceFeatures2 extFeatures             = initVulkanStructure(&vulkan11Features);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

    log << TestLog::Message << vulkan11Features << TestLog::EndMessage;
    log << TestLog::Message << vulkan12Features << TestLog::EndMessage;

    // Where promoted extensions didn't originally have feature structs, validate that the feature bits are set
    // when the extension is supported. (These checks could go in the extension's mandatory_features check, but
    // they're checked here because the extensions came first, so adding the extra dependency there seems odd.
    std::pair<std::pair<const char *, const char *>, VkBool32> extensions2validate[] = {
        {{"VK_KHR_sampler_mirror_clamp_to_edge", "VkPhysicalDeviceVulkan12Features.samplerMirrorClampToEdge"},
         vulkan12Features.samplerMirrorClampToEdge},
        {{"VK_KHR_draw_indirect_count", "VkPhysicalDeviceVulkan12Features.drawIndirectCount"},
         vulkan12Features.drawIndirectCount},
        {{"VK_EXT_descriptor_indexing", "VkPhysicalDeviceVulkan12Features.descriptorIndexing"},
         vulkan12Features.descriptorIndexing},
        {{"VK_EXT_sampler_filter_minmax", "VkPhysicalDeviceVulkan12Features.samplerFilterMinmax"},
         vulkan12Features.samplerFilterMinmax},
        {{"VK_EXT_shader_viewport_index_layer", "VkPhysicalDeviceVulkan12Features.shaderOutputViewportIndex"},
         vulkan12Features.shaderOutputViewportIndex},
        {{"VK_EXT_shader_viewport_index_layer", "VkPhysicalDeviceVulkan12Features.shaderOutputLayer"},
         vulkan12Features.shaderOutputLayer}};
    vector<VkExtensionProperties> extensionProperties =
        enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
    for (const auto &ext : extensions2validate)
        if (checkExtension(extensionProperties, ext.first.first) && !ext.second)
            TCU_FAIL(string("Mismatch between extension ") + ext.first.first + " and " + ext.first.second);

    // collect all extension features
    {
        VkPhysicalDevice16BitStorageFeatures device16BitStorageFeatures = initVulkanStructure();
        VkPhysicalDeviceMultiviewFeatures deviceMultiviewFeatures = initVulkanStructure(&device16BitStorageFeatures);
        VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeatures = initVulkanStructure(&deviceMultiviewFeatures);
        VkPhysicalDeviceSamplerYcbcrConversionFeatures samplerYcbcrConversionFeatures =
            initVulkanStructure(&protectedMemoryFeatures);
        VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures =
            initVulkanStructure(&samplerYcbcrConversionFeatures);
        VkPhysicalDeviceVariablePointersFeatures variablePointerFeatures =
            initVulkanStructure(&shaderDrawParametersFeatures);
        VkPhysicalDevice8BitStorageFeatures device8BitStorageFeatures = initVulkanStructure(&variablePointerFeatures);
        VkPhysicalDeviceShaderAtomicInt64Features shaderAtomicInt64Features =
            initVulkanStructure(&device8BitStorageFeatures);
        VkPhysicalDeviceShaderFloat16Int8Features shaderFloat16Int8Features =
            initVulkanStructure(&shaderAtomicInt64Features);
        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures =
            initVulkanStructure(&shaderFloat16Int8Features);
        VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures =
            initVulkanStructure(&descriptorIndexingFeatures);
        VkPhysicalDeviceImagelessFramebufferFeatures imagelessFramebufferFeatures =
            initVulkanStructure(&scalarBlockLayoutFeatures);
        VkPhysicalDeviceUniformBufferStandardLayoutFeatures uniformBufferStandardLayoutFeatures =
            initVulkanStructure(&imagelessFramebufferFeatures);
        VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures shaderSubgroupExtendedTypesFeatures =
            initVulkanStructure(&uniformBufferStandardLayoutFeatures);
        VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilLayoutsFeatures =
            initVulkanStructure(&shaderSubgroupExtendedTypesFeatures);
        VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures =
            initVulkanStructure(&separateDepthStencilLayoutsFeatures);
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures =
            initVulkanStructure(&hostQueryResetFeatures);
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures =
            initVulkanStructure(&timelineSemaphoreFeatures);
        VkPhysicalDeviceVulkanMemoryModelFeatures vulkanMemoryModelFeatures =
            initVulkanStructure(&bufferDeviceAddressFeatures);
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

        // First verify the 1.1 feature struct consistency because that struct is newly added in 1.2
        if ((device16BitStorageFeatures.storageBuffer16BitAccess != vulkan11Features.storageBuffer16BitAccess ||
             device16BitStorageFeatures.uniformAndStorageBuffer16BitAccess !=
                 vulkan11Features.uniformAndStorageBuffer16BitAccess ||
             device16BitStorageFeatures.storagePushConstant16 != vulkan11Features.storagePushConstant16 ||
             device16BitStorageFeatures.storageInputOutput16 != vulkan11Features.storageInputOutput16))
        {
            TCU_FAIL("Mismatch between VkPhysicalDevice16BitStorageFeatures and VkPhysicalDeviceVulkan11Features");
        }

        if ((deviceMultiviewFeatures.multiview != vulkan11Features.multiview ||
             deviceMultiviewFeatures.multiviewGeometryShader != vulkan11Features.multiviewGeometryShader ||
             deviceMultiviewFeatures.multiviewTessellationShader != vulkan11Features.multiviewTessellationShader))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewFeatures and VkPhysicalDeviceVulkan11Features");
        }

        if ((protectedMemoryFeatures.protectedMemory != vulkan11Features.protectedMemory))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceProtectedMemoryFeatures and VkPhysicalDeviceVulkan11Features");
        }

        if ((samplerYcbcrConversionFeatures.samplerYcbcrConversion != vulkan11Features.samplerYcbcrConversion))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceSamplerYcbcrConversionFeatures and VkPhysicalDeviceVulkan11Features");
        }

        if ((shaderDrawParametersFeatures.shaderDrawParameters != vulkan11Features.shaderDrawParameters))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceShaderDrawParametersFeatures and VkPhysicalDeviceVulkan11Features");
        }

        if ((variablePointerFeatures.variablePointersStorageBuffer != vulkan11Features.variablePointersStorageBuffer ||
             variablePointerFeatures.variablePointers != vulkan11Features.variablePointers))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceVariablePointersFeatures and VkPhysicalDeviceVulkan11Features");
        }

        // Now check the consistency of the 1.2 feature struct
        if ((device8BitStorageFeatures.storageBuffer8BitAccess != vulkan12Features.storageBuffer8BitAccess ||
             device8BitStorageFeatures.uniformAndStorageBuffer8BitAccess !=
                 vulkan12Features.uniformAndStorageBuffer8BitAccess ||
             device8BitStorageFeatures.storagePushConstant8 != vulkan12Features.storagePushConstant8))
        {
            TCU_FAIL("Mismatch between VkPhysicalDevice8BitStorageFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((shaderAtomicInt64Features.shaderBufferInt64Atomics != vulkan12Features.shaderBufferInt64Atomics ||
             shaderAtomicInt64Features.shaderSharedInt64Atomics != vulkan12Features.shaderSharedInt64Atomics))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderAtomicInt64Features and VkPhysicalDeviceVulkan12Features");
        }

        if ((shaderFloat16Int8Features.shaderFloat16 != vulkan12Features.shaderFloat16 ||
             shaderFloat16Int8Features.shaderInt8 != vulkan12Features.shaderInt8))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderFloat16Int8Features and VkPhysicalDeviceVulkan12Features");
        }

        if (descriptorIndexingFeatures.shaderInputAttachmentArrayDynamicIndexing !=
                vulkan12Features.shaderInputAttachmentArrayDynamicIndexing ||
            descriptorIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing !=
                vulkan12Features.shaderUniformTexelBufferArrayDynamicIndexing ||
            descriptorIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing !=
                vulkan12Features.shaderStorageTexelBufferArrayDynamicIndexing ||
            descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing !=
                vulkan12Features.shaderUniformBufferArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing !=
                vulkan12Features.shaderSampledImageArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing !=
                vulkan12Features.shaderStorageBufferArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing !=
                vulkan12Features.shaderStorageImageArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderInputAttachmentArrayNonUniformIndexing !=
                vulkan12Features.shaderInputAttachmentArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing !=
                vulkan12Features.shaderUniformTexelBufferArrayNonUniformIndexing ||
            descriptorIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing !=
                vulkan12Features.shaderStorageTexelBufferArrayNonUniformIndexing ||
            descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind !=
                vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind !=
                vulkan12Features.descriptorBindingSampledImageUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind !=
                vulkan12Features.descriptorBindingStorageImageUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind !=
                vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind !=
                vulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind !=
                vulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind ||
            descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending !=
                vulkan12Features.descriptorBindingUpdateUnusedWhilePending ||
            descriptorIndexingFeatures.descriptorBindingPartiallyBound !=
                vulkan12Features.descriptorBindingPartiallyBound ||
            descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount !=
                vulkan12Features.descriptorBindingVariableDescriptorCount ||
            descriptorIndexingFeatures.runtimeDescriptorArray != vulkan12Features.runtimeDescriptorArray)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceDescriptorIndexingFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((scalarBlockLayoutFeatures.scalarBlockLayout != vulkan12Features.scalarBlockLayout))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceScalarBlockLayoutFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((imagelessFramebufferFeatures.imagelessFramebuffer != vulkan12Features.imagelessFramebuffer))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceImagelessFramebufferFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((uniformBufferStandardLayoutFeatures.uniformBufferStandardLayout !=
             vulkan12Features.uniformBufferStandardLayout))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceUniformBufferStandardLayoutFeatures and "
                     "VkPhysicalDeviceVulkan12Features");
        }

        if ((shaderSubgroupExtendedTypesFeatures.shaderSubgroupExtendedTypes !=
             vulkan12Features.shaderSubgroupExtendedTypes))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures and "
                     "VkPhysicalDeviceVulkan12Features");
        }

        if ((separateDepthStencilLayoutsFeatures.separateDepthStencilLayouts !=
             vulkan12Features.separateDepthStencilLayouts))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures and "
                     "VkPhysicalDeviceVulkan12Features");
        }

        if ((hostQueryResetFeatures.hostQueryReset != vulkan12Features.hostQueryReset))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceHostQueryResetFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((timelineSemaphoreFeatures.timelineSemaphore != vulkan12Features.timelineSemaphore))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceTimelineSemaphoreFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((bufferDeviceAddressFeatures.bufferDeviceAddress != vulkan12Features.bufferDeviceAddress ||
             bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay !=
                 vulkan12Features.bufferDeviceAddressCaptureReplay ||
             bufferDeviceAddressFeatures.bufferDeviceAddressMultiDevice !=
                 vulkan12Features.bufferDeviceAddressMultiDevice))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceBufferDeviceAddressFeatures and VkPhysicalDeviceVulkan12Features");
        }

        if ((vulkanMemoryModelFeatures.vulkanMemoryModel != vulkan12Features.vulkanMemoryModel ||
             vulkanMemoryModelFeatures.vulkanMemoryModelDeviceScope != vulkan12Features.vulkanMemoryModelDeviceScope ||
             vulkanMemoryModelFeatures.vulkanMemoryModelAvailabilityVisibilityChains !=
                 vulkan12Features.vulkanMemoryModelAvailabilityVisibilityChains))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceVulkanMemoryModelFeatures and VkPhysicalDeviceVulkan12Features");
        }
    }

    return tcu::TestStatus::pass("Vulkan 1.2 device features are consistent with extensions");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus deviceFeatureExtensionsConsistencyVulkan14(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 4, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.4 required to run test");

    VkPhysicalDeviceVulkan14Features vulkan14Features = initVulkanStructure();
    VkPhysicalDeviceFeatures2 extFeatures             = initVulkanStructure(&vulkan14Features);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

    log << TestLog::Message << vulkan14Features << TestLog::EndMessage;

    // collect all extension features
    {
        VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR dynamicRenderingLocalReadFeatures = initVulkanStructure();
        VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR globalPriorityQueryFeatures =
            initVulkanStructure(&dynamicRenderingLocalReadFeatures);
        VkPhysicalDeviceIndexTypeUint8FeaturesKHR indexTypeUint8Features =
            initVulkanStructure(&globalPriorityQueryFeatures);
        VkPhysicalDeviceLineRasterizationFeaturesKHR lineRasterizationFeatures =
            initVulkanStructure(&indexTypeUint8Features);
        VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features = initVulkanStructure(&lineRasterizationFeatures);
        VkPhysicalDeviceMaintenance6FeaturesKHR maintenance6Features = initVulkanStructure(&maintenance5Features);
        VkPhysicalDeviceShaderExpectAssumeFeaturesKHR shaderExpectAssumeFeatures =
            initVulkanStructure(&maintenance6Features);
        VkPhysicalDeviceShaderFloatControls2FeaturesKHR shaderFloatControls2Features =
            initVulkanStructure(&shaderExpectAssumeFeatures);
        VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR shaderSubgroupRotateFeatures =
            initVulkanStructure(&shaderFloatControls2Features);
        VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR vertexAttributeDivisorFeatures =
            initVulkanStructure(&shaderSubgroupRotateFeatures);
        VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures =
            initVulkanStructure(&vertexAttributeDivisorFeatures);
        VkPhysicalDevicePipelineProtectedAccessFeaturesEXT pipelineProtectedAccessFeatures =
            initVulkanStructure(&hostImageCopyFeatures);
        VkPhysicalDevicePipelineRobustnessFeaturesEXT pipelineRobustnessFeatures =
            initVulkanStructure(&pipelineProtectedAccessFeatures);

        extFeatures = initVulkanStructure(&pipelineRobustnessFeatures);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

        log << TestLog::Message << extFeatures << TestLog::EndMessage;
        log << TestLog::Message << dynamicRenderingLocalReadFeatures << TestLog::EndMessage;
        log << TestLog::Message << globalPriorityQueryFeatures << TestLog::EndMessage;
        log << TestLog::Message << indexTypeUint8Features << TestLog::EndMessage;
        log << TestLog::Message << lineRasterizationFeatures << TestLog::EndMessage;
        log << TestLog::Message << maintenance5Features << TestLog::EndMessage;
        log << TestLog::Message << maintenance6Features << TestLog::EndMessage;
        log << TestLog::Message << shaderExpectAssumeFeatures << TestLog::EndMessage;
        log << TestLog::Message << shaderFloatControls2Features << TestLog::EndMessage;
        log << TestLog::Message << shaderSubgroupRotateFeatures << TestLog::EndMessage;
        log << TestLog::Message << vertexAttributeDivisorFeatures << TestLog::EndMessage;
        log << TestLog::Message << hostImageCopyFeatures << TestLog::EndMessage;
        log << TestLog::Message << pipelineProtectedAccessFeatures << TestLog::EndMessage;
        log << TestLog::Message << pipelineRobustnessFeatures << TestLog::EndMessage;

        if (dynamicRenderingLocalReadFeatures.dynamicRenderingLocalRead != vulkan14Features.dynamicRenderingLocalRead)
            TCU_FAIL("Mismatch between VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR and "
                     "VkPhysicalDeviceVulkan14Features");

        if (globalPriorityQueryFeatures.globalPriorityQuery != vulkan14Features.globalPriorityQuery)
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR and VkPhysicalDeviceVulkan14Features");

        if (indexTypeUint8Features.indexTypeUint8 != vulkan14Features.indexTypeUint8)
            TCU_FAIL("Mismatch between VkPhysicalDeviceIndexTypeUint8FeaturesKHR and VkPhysicalDeviceVulkan14Features");

        if ((lineRasterizationFeatures.rectangularLines != vulkan14Features.rectangularLines) ||
            (lineRasterizationFeatures.bresenhamLines != vulkan14Features.bresenhamLines) ||
            (lineRasterizationFeatures.smoothLines != vulkan14Features.smoothLines) ||
            (lineRasterizationFeatures.stippledRectangularLines != vulkan14Features.stippledRectangularLines) ||
            (lineRasterizationFeatures.stippledBresenhamLines != vulkan14Features.stippledBresenhamLines) ||
            (lineRasterizationFeatures.stippledSmoothLines != vulkan14Features.stippledSmoothLines))
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceLineRasterizationFeaturesKHR and VkPhysicalDeviceVulkan14Features");

        if (maintenance5Features.maintenance5 != vulkan14Features.maintenance5)
            TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance5FeaturesKHR and VkPhysicalDeviceVulkan14Features");

        if (maintenance6Features.maintenance6 != vulkan14Features.maintenance6)
            TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance6FeaturesKHR and VkPhysicalDeviceVulkan14Features");

        if (shaderExpectAssumeFeatures.shaderExpectAssume != vulkan14Features.shaderExpectAssume)
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceShaderExpectAssumeFeatures and VkPhysicalDeviceVulkan14Features");

        if (shaderFloatControls2Features.shaderFloatControls2 != vulkan14Features.shaderFloatControls2)
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderFloatControls2FeaturesKHR and "
                     "VkPhysicalDeviceVulkan14Features");

        if ((shaderSubgroupRotateFeatures.shaderSubgroupRotate != vulkan14Features.shaderSubgroupRotate) ||
            (shaderSubgroupRotateFeatures.shaderSubgroupRotateClustered !=
             vulkan14Features.shaderSubgroupRotateClustered))
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR and "
                     "VkPhysicalDeviceVulkan14Features");

        if ((vertexAttributeDivisorFeatures.vertexAttributeInstanceRateDivisor !=
             vulkan14Features.vertexAttributeInstanceRateDivisor) ||
            (vertexAttributeDivisorFeatures.vertexAttributeInstanceRateZeroDivisor !=
             vulkan14Features.vertexAttributeInstanceRateZeroDivisor))
            TCU_FAIL("Mismatch between VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR and "
                     "VkPhysicalDeviceVulkan14Features");

        if (hostImageCopyFeatures.hostImageCopy != vulkan14Features.hostImageCopy)
            TCU_FAIL("Mismatch between VkPhysicalDeviceHostImageCopyFeaturesEXT and VkPhysicalDeviceVulkan14Features");

        if (pipelineProtectedAccessFeatures.pipelineProtectedAccess != vulkan14Features.pipelineProtectedAccess)
            TCU_FAIL("Mismatch between VkPhysicalDevicePipelineProtectedAccessFeaturesEXT and "
                     "VkPhysicalDeviceVulkan14Features");

        if (pipelineRobustnessFeatures.pipelineRobustness != vulkan14Features.pipelineRobustness)
            TCU_FAIL(
                "Mismatch between VkPhysicalDevicePipelineRobustnessFeaturesEXT and VkPhysicalDeviceVulkan14Features");
    }

    return tcu::TestStatus::pass("Vulkan 1.4 device features are consistent with extensions");
}

tcu::TestStatus deviceFeatureExtensionsConsistencyVulkan13(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 3, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.3 required to run test");

    VkPhysicalDeviceVulkan13Features vulkan13Features = initVulkanStructure();
    VkPhysicalDeviceFeatures2 extFeatures             = initVulkanStructure(&vulkan13Features);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

    log << TestLog::Message << vulkan13Features << TestLog::EndMessage;

    // collect all extension features
    {
        VkPhysicalDeviceImageRobustnessFeatures imageRobustnessFeatures = initVulkanStructure();
        VkPhysicalDeviceInlineUniformBlockFeatures inlineUniformBlockFeatures =
            initVulkanStructure(&imageRobustnessFeatures);
        VkPhysicalDevicePipelineCreationCacheControlFeatures pipelineCreationCacheControlFeatures =
            initVulkanStructure(&inlineUniformBlockFeatures);
        VkPhysicalDevicePrivateDataFeatures privateDataFeatures =
            initVulkanStructure(&pipelineCreationCacheControlFeatures);
        VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures shaderDemoteToHelperInvocationFeatures =
            initVulkanStructure(&privateDataFeatures);
        VkPhysicalDeviceShaderTerminateInvocationFeatures shaderTerminateInvocationFeatures =
            initVulkanStructure(&shaderDemoteToHelperInvocationFeatures);
        VkPhysicalDeviceSubgroupSizeControlFeatures subgroupSizeControlFeatures =
            initVulkanStructure(&shaderTerminateInvocationFeatures);
        VkPhysicalDeviceSynchronization2Features synchronization2Features =
            initVulkanStructure(&subgroupSizeControlFeatures);
        VkPhysicalDeviceTextureCompressionASTCHDRFeatures textureCompressionASTCHDRFeatures =
            initVulkanStructure(&synchronization2Features);
        VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures zeroInitializeWorkgroupMemoryFeatures =
            initVulkanStructure(&textureCompressionASTCHDRFeatures);
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures =
            initVulkanStructure(&zeroInitializeWorkgroupMemoryFeatures);
        VkPhysicalDeviceShaderIntegerDotProductFeatures shaderIntegerDotProductFeatures =
            initVulkanStructure(&dynamicRenderingFeatures);
        VkPhysicalDeviceMaintenance4Features maintenance4Features =
            initVulkanStructure(&shaderIntegerDotProductFeatures);

        extFeatures = initVulkanStructure(&maintenance4Features);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

        log << TestLog::Message << extFeatures << TestLog::EndMessage;
        log << TestLog::Message << imageRobustnessFeatures << TestLog::EndMessage;
        log << TestLog::Message << inlineUniformBlockFeatures << TestLog::EndMessage;
        log << TestLog::Message << pipelineCreationCacheControlFeatures << TestLog::EndMessage;
        log << TestLog::Message << privateDataFeatures << TestLog::EndMessage;
        log << TestLog::Message << shaderDemoteToHelperInvocationFeatures << TestLog::EndMessage;
        log << TestLog::Message << shaderTerminateInvocationFeatures << TestLog::EndMessage;
        log << TestLog::Message << subgroupSizeControlFeatures << TestLog::EndMessage;
        log << TestLog::Message << synchronization2Features << TestLog::EndMessage;
        log << TestLog::Message << textureCompressionASTCHDRFeatures << TestLog::EndMessage;
        log << TestLog::Message << zeroInitializeWorkgroupMemoryFeatures << TestLog::EndMessage;
        log << TestLog::Message << dynamicRenderingFeatures << TestLog::EndMessage;
        log << TestLog::Message << shaderIntegerDotProductFeatures << TestLog::EndMessage;
        log << TestLog::Message << maintenance4Features << TestLog::EndMessage;

        if (imageRobustnessFeatures.robustImageAccess != vulkan13Features.robustImageAccess)
            TCU_FAIL("Mismatch between VkPhysicalDeviceImageRobustnessFeatures and VkPhysicalDeviceVulkan13Features");

        if ((inlineUniformBlockFeatures.inlineUniformBlock != vulkan13Features.inlineUniformBlock) ||
            (inlineUniformBlockFeatures.descriptorBindingInlineUniformBlockUpdateAfterBind !=
             vulkan13Features.descriptorBindingInlineUniformBlockUpdateAfterBind))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceInlineUniformBlockFeatures and VkPhysicalDeviceVulkan13Features");
        }

        if (pipelineCreationCacheControlFeatures.pipelineCreationCacheControl !=
            vulkan13Features.pipelineCreationCacheControl)
            TCU_FAIL("Mismatch between VkPhysicalDevicePipelineCreationCacheControlFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if (privateDataFeatures.privateData != vulkan13Features.privateData)
            TCU_FAIL("Mismatch between VkPhysicalDevicePrivateDataFeatures and VkPhysicalDeviceVulkan13Features");

        if (shaderDemoteToHelperInvocationFeatures.shaderDemoteToHelperInvocation !=
            vulkan13Features.shaderDemoteToHelperInvocation)
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if (shaderTerminateInvocationFeatures.shaderTerminateInvocation != vulkan13Features.shaderTerminateInvocation)
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderTerminateInvocationFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if ((subgroupSizeControlFeatures.subgroupSizeControl != vulkan13Features.subgroupSizeControl) ||
            (subgroupSizeControlFeatures.computeFullSubgroups != vulkan13Features.computeFullSubgroups))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceSubgroupSizeControlFeatures and VkPhysicalDeviceVulkan13Features");
        }

        if (synchronization2Features.synchronization2 != vulkan13Features.synchronization2)
            TCU_FAIL("Mismatch between VkPhysicalDeviceSynchronization2Features and VkPhysicalDeviceVulkan13Features");

        if (textureCompressionASTCHDRFeatures.textureCompressionASTC_HDR != vulkan13Features.textureCompressionASTC_HDR)
            TCU_FAIL("Mismatch between VkPhysicalDeviceTextureCompressionASTCHDRFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if (zeroInitializeWorkgroupMemoryFeatures.shaderZeroInitializeWorkgroupMemory !=
            vulkan13Features.shaderZeroInitializeWorkgroupMemory)
            TCU_FAIL("Mismatch between VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if (dynamicRenderingFeatures.dynamicRendering != vulkan13Features.dynamicRendering)
            TCU_FAIL("Mismatch between VkPhysicalDeviceDynamicRenderingFeatures and VkPhysicalDeviceVulkan13Features");

        if (shaderIntegerDotProductFeatures.shaderIntegerDotProduct != vulkan13Features.shaderIntegerDotProduct)
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderIntegerDotProductFeatures and "
                     "VkPhysicalDeviceVulkan13Features");

        if (maintenance4Features.maintenance4 != vulkan13Features.maintenance4)
            TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance4Features and VkPhysicalDeviceVulkan13Features");
    }

    return tcu::TestStatus::pass("Vulkan 1.3 device features are consistent with extensions");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus devicePropertyExtensionsConsistencyVulkan12(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 2, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.2 required to run test");

    VkPhysicalDeviceVulkan12Properties vulkan12Properties = initVulkanStructure();
    VkPhysicalDeviceVulkan11Properties vulkan11Properties = initVulkanStructure(&vulkan12Properties);
    VkPhysicalDeviceProperties2 extProperties             = initVulkanStructure(&vulkan11Properties);

    vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

    log << TestLog::Message << vulkan11Properties << TestLog::EndMessage;
    log << TestLog::Message << vulkan12Properties << TestLog::EndMessage;

    // Validate all fields initialized matching to extension structures
    {
        VkPhysicalDeviceIDProperties idProperties                       = initVulkanStructure();
        VkPhysicalDeviceSubgroupProperties subgroupProperties           = initVulkanStructure(&idProperties);
        VkPhysicalDevicePointClippingProperties pointClippingProperties = initVulkanStructure(&subgroupProperties);
        VkPhysicalDeviceMultiviewProperties multiviewProperties         = initVulkanStructure(&pointClippingProperties);
        VkPhysicalDeviceProtectedMemoryProperties protectedMemoryPropertiesKHR =
            initVulkanStructure(&multiviewProperties);
        VkPhysicalDeviceMaintenance3Properties maintenance3Properties =
            initVulkanStructure(&protectedMemoryPropertiesKHR);
        VkPhysicalDeviceDriverProperties driverProperties               = initVulkanStructure(&maintenance3Properties);
        VkPhysicalDeviceFloatControlsProperties floatControlsProperties = initVulkanStructure(&driverProperties);
        VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties =
            initVulkanStructure(&floatControlsProperties);
        VkPhysicalDeviceDepthStencilResolveProperties depthStencilResolveProperties =
            initVulkanStructure(&descriptorIndexingProperties);
        VkPhysicalDeviceSamplerFilterMinmaxProperties samplerFilterMinmaxProperties =
            initVulkanStructure(&depthStencilResolveProperties);
        VkPhysicalDeviceTimelineSemaphoreProperties timelineSemaphoreProperties =
            initVulkanStructure(&samplerFilterMinmaxProperties);
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
            if ((deMemCmp(idProperties.deviceLUID, vulkan11Properties.deviceLUID, VK_LUID_SIZE) != 0) ||
                (idProperties.deviceNodeMask != vulkan11Properties.deviceNodeMask))
            {
                TCU_FAIL("Mismatch between VkPhysicalDeviceIDProperties and VkPhysicalDeviceVulkan11Properties");
            }
        }

        if ((subgroupProperties.subgroupSize != vulkan11Properties.subgroupSize ||
             subgroupProperties.supportedStages != vulkan11Properties.subgroupSupportedStages ||
             subgroupProperties.supportedOperations != vulkan11Properties.subgroupSupportedOperations ||
             subgroupProperties.quadOperationsInAllStages != vulkan11Properties.subgroupQuadOperationsInAllStages))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupProperties and VkPhysicalDeviceVulkan11Properties");
        }

        if ((pointClippingProperties.pointClippingBehavior != vulkan11Properties.pointClippingBehavior))
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePointClippingProperties and VkPhysicalDeviceVulkan11Properties");
        }

        if ((multiviewProperties.maxMultiviewViewCount != vulkan11Properties.maxMultiviewViewCount ||
             multiviewProperties.maxMultiviewInstanceIndex != vulkan11Properties.maxMultiviewInstanceIndex))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceMultiviewProperties and VkPhysicalDeviceVulkan11Properties");
        }

        if ((protectedMemoryPropertiesKHR.protectedNoFault != vulkan11Properties.protectedNoFault))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceProtectedMemoryProperties and VkPhysicalDeviceVulkan11Properties");
        }

        if ((maintenance3Properties.maxPerSetDescriptors != vulkan11Properties.maxPerSetDescriptors ||
             maintenance3Properties.maxMemoryAllocationSize != vulkan11Properties.maxMemoryAllocationSize))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance3Properties and VkPhysicalDeviceVulkan11Properties");
        }

        if ((driverProperties.driverID != vulkan12Properties.driverID ||
             strncmp(driverProperties.driverName, vulkan12Properties.driverName, VK_MAX_DRIVER_NAME_SIZE) != 0 ||
             strncmp(driverProperties.driverInfo, vulkan12Properties.driverInfo, VK_MAX_DRIVER_INFO_SIZE) != 0 ||
             driverProperties.conformanceVersion.major != vulkan12Properties.conformanceVersion.major ||
             driverProperties.conformanceVersion.minor != vulkan12Properties.conformanceVersion.minor ||
             driverProperties.conformanceVersion.subminor != vulkan12Properties.conformanceVersion.subminor ||
             driverProperties.conformanceVersion.patch != vulkan12Properties.conformanceVersion.patch))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceDriverProperties and VkPhysicalDeviceVulkan12Properties");
        }

        if ((floatControlsProperties.denormBehaviorIndependence != vulkan12Properties.denormBehaviorIndependence ||
             floatControlsProperties.roundingModeIndependence != vulkan12Properties.roundingModeIndependence ||
             floatControlsProperties.shaderSignedZeroInfNanPreserveFloat16 !=
                 vulkan12Properties.shaderSignedZeroInfNanPreserveFloat16 ||
             floatControlsProperties.shaderSignedZeroInfNanPreserveFloat32 !=
                 vulkan12Properties.shaderSignedZeroInfNanPreserveFloat32 ||
             floatControlsProperties.shaderSignedZeroInfNanPreserveFloat64 !=
                 vulkan12Properties.shaderSignedZeroInfNanPreserveFloat64 ||
             floatControlsProperties.shaderDenormPreserveFloat16 != vulkan12Properties.shaderDenormPreserveFloat16 ||
             floatControlsProperties.shaderDenormPreserveFloat32 != vulkan12Properties.shaderDenormPreserveFloat32 ||
             floatControlsProperties.shaderDenormPreserveFloat64 != vulkan12Properties.shaderDenormPreserveFloat64 ||
             floatControlsProperties.shaderDenormFlushToZeroFloat16 !=
                 vulkan12Properties.shaderDenormFlushToZeroFloat16 ||
             floatControlsProperties.shaderDenormFlushToZeroFloat32 !=
                 vulkan12Properties.shaderDenormFlushToZeroFloat32 ||
             floatControlsProperties.shaderDenormFlushToZeroFloat64 !=
                 vulkan12Properties.shaderDenormFlushToZeroFloat64 ||
             floatControlsProperties.shaderRoundingModeRTEFloat16 != vulkan12Properties.shaderRoundingModeRTEFloat16 ||
             floatControlsProperties.shaderRoundingModeRTEFloat32 != vulkan12Properties.shaderRoundingModeRTEFloat32 ||
             floatControlsProperties.shaderRoundingModeRTEFloat64 != vulkan12Properties.shaderRoundingModeRTEFloat64 ||
             floatControlsProperties.shaderRoundingModeRTZFloat16 != vulkan12Properties.shaderRoundingModeRTZFloat16 ||
             floatControlsProperties.shaderRoundingModeRTZFloat32 != vulkan12Properties.shaderRoundingModeRTZFloat32 ||
             floatControlsProperties.shaderRoundingModeRTZFloat64 != vulkan12Properties.shaderRoundingModeRTZFloat64))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceFloatControlsProperties and VkPhysicalDeviceVulkan12Properties");
        }

        if ((descriptorIndexingProperties.maxUpdateAfterBindDescriptorsInAllPools !=
                 vulkan12Properties.maxUpdateAfterBindDescriptorsInAllPools ||
             descriptorIndexingProperties.shaderUniformBufferArrayNonUniformIndexingNative !=
                 vulkan12Properties.shaderUniformBufferArrayNonUniformIndexingNative ||
             descriptorIndexingProperties.shaderSampledImageArrayNonUniformIndexingNative !=
                 vulkan12Properties.shaderSampledImageArrayNonUniformIndexingNative ||
             descriptorIndexingProperties.shaderStorageBufferArrayNonUniformIndexingNative !=
                 vulkan12Properties.shaderStorageBufferArrayNonUniformIndexingNative ||
             descriptorIndexingProperties.shaderStorageImageArrayNonUniformIndexingNative !=
                 vulkan12Properties.shaderStorageImageArrayNonUniformIndexingNative ||
             descriptorIndexingProperties.shaderInputAttachmentArrayNonUniformIndexingNative !=
                 vulkan12Properties.shaderInputAttachmentArrayNonUniformIndexingNative ||
             descriptorIndexingProperties.robustBufferAccessUpdateAfterBind !=
                 vulkan12Properties.robustBufferAccessUpdateAfterBind ||
             descriptorIndexingProperties.quadDivergentImplicitLod != vulkan12Properties.quadDivergentImplicitLod ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindUniformBuffers !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageBuffers !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageImages !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages ||
             descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindInputAttachments !=
                 vulkan12Properties.maxPerStageDescriptorUpdateAfterBindInputAttachments ||
             descriptorIndexingProperties.maxPerStageUpdateAfterBindResources !=
                 vulkan12Properties.maxPerStageUpdateAfterBindResources ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffers ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffers ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages ||
             descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindInputAttachments !=
                 vulkan12Properties.maxDescriptorSetUpdateAfterBindInputAttachments))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceDescriptorIndexingProperties and VkPhysicalDeviceVulkan12Properties");
        }

        if ((depthStencilResolveProperties.supportedDepthResolveModes !=
                 vulkan12Properties.supportedDepthResolveModes ||
             depthStencilResolveProperties.supportedStencilResolveModes !=
                 vulkan12Properties.supportedStencilResolveModes ||
             depthStencilResolveProperties.independentResolveNone != vulkan12Properties.independentResolveNone ||
             depthStencilResolveProperties.independentResolve != vulkan12Properties.independentResolve))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceDepthStencilResolveProperties and "
                     "VkPhysicalDeviceVulkan12Properties");
        }

        if ((samplerFilterMinmaxProperties.filterMinmaxSingleComponentFormats !=
                 vulkan12Properties.filterMinmaxSingleComponentFormats ||
             samplerFilterMinmaxProperties.filterMinmaxImageComponentMapping !=
                 vulkan12Properties.filterMinmaxImageComponentMapping))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceSamplerFilterMinmaxProperties and "
                     "VkPhysicalDeviceVulkan12Properties");
        }

        if ((timelineSemaphoreProperties.maxTimelineSemaphoreValueDifference !=
             vulkan12Properties.maxTimelineSemaphoreValueDifference))
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceTimelineSemaphoreProperties and VkPhysicalDeviceVulkan12Properties");
        }
    }

    return tcu::TestStatus::pass("Vulkan 1.2 device properties are consistent with extension properties");
}

#ifndef CTS_USES_VULKANSC
void checkSupportKhrShaderSubgroupRotate(Context &context)
{
    context.requireDeviceFunctionality("VK_KHR_shader_subgroup_rotate");
}

tcu::TestStatus subgroupRotatePropertyExtensionFeatureConsistency(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkPhysicalDeviceSubgroupProperties &subgroupProperties           = context.getSubgroupProperties();
    VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR subgroupRotateFeatures = initVulkanStructure();
    VkPhysicalDeviceFeatures2 extFeatures = initVulkanStructure(&subgroupRotateFeatures);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &extFeatures);

    // Need access to VkSubgroupFeatureFlagBits which are provided by Vulkan 1.1
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.1 required to run test");

    // Ensure "VK_KHR_shader_subgroup_rotate" extension's spec version is at least 2
    {
        const std::string extensionName = "VK_KHR_shader_subgroup_rotate";
        const std::vector<VkExtensionProperties> deviceExtensionProperties =
            enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);

        for (const auto &property : deviceExtensionProperties)
        {
            if (property.extensionName == extensionName && property.specVersion < 2)
            {
                TCU_FAIL(extensionName + " is version 1. Need version 2 or higher");
            }
        }
    }

    // Validate all fields initialized matching to extension structures
    {
        if (subgroupRotateFeatures.shaderSubgroupRotate !=
                ((subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ROTATE_BIT_KHR) != 0) ||
            subgroupRotateFeatures.shaderSubgroupRotateClustered !=
                ((subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ROTATE_CLUSTERED_BIT_KHR) != 0))
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR and "
                     "VkPhysicalDeviceVulkan11Properties");
        }
    }
    return tcu::TestStatus::pass(
        "Vulkan device properties are consistent with VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR");
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
tcu::TestStatus devicePropertyExtensionsConsistencyVulkan13(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 3, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.3 required to run test");

    VkPhysicalDeviceVulkan13Properties vulkan13Properties = initVulkanStructure();
    VkPhysicalDeviceProperties2 extProperties             = initVulkanStructure(&vulkan13Properties);

    vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

    log << TestLog::Message << vulkan13Properties << TestLog::EndMessage;

    // Validate all fields initialized matching to extension structures
    {
        VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties = initVulkanStructure();
        VkPhysicalDeviceInlineUniformBlockProperties inlineUniformBlockProperties =
            initVulkanStructure(&subgroupSizeControlProperties);
        VkPhysicalDeviceShaderIntegerDotProductProperties shaderIntegerDotProductProperties =
            initVulkanStructure(&inlineUniformBlockProperties);
        VkPhysicalDeviceTexelBufferAlignmentProperties texelBufferAlignmentProperties =
            initVulkanStructure(&shaderIntegerDotProductProperties);
        VkPhysicalDeviceMaintenance4Properties maintenance4Properties =
            initVulkanStructure(&texelBufferAlignmentProperties);
        extProperties = initVulkanStructure(&maintenance4Properties);

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

        if (subgroupSizeControlProperties.minSubgroupSize != vulkan13Properties.minSubgroupSize ||
            subgroupSizeControlProperties.maxSubgroupSize != vulkan13Properties.maxSubgroupSize ||
            subgroupSizeControlProperties.maxComputeWorkgroupSubgroups !=
                vulkan13Properties.maxComputeWorkgroupSubgroups ||
            subgroupSizeControlProperties.requiredSubgroupSizeStages != vulkan13Properties.requiredSubgroupSizeStages)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceSubgroupSizeControlProperties and "
                     "VkPhysicalDeviceVulkan13Properties");
        }

        if (inlineUniformBlockProperties.maxInlineUniformBlockSize != vulkan13Properties.maxInlineUniformBlockSize ||
            inlineUniformBlockProperties.maxPerStageDescriptorInlineUniformBlocks !=
                vulkan13Properties.maxPerStageDescriptorInlineUniformBlocks ||
            inlineUniformBlockProperties.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks !=
                vulkan13Properties.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks ||
            inlineUniformBlockProperties.maxDescriptorSetInlineUniformBlocks !=
                vulkan13Properties.maxDescriptorSetInlineUniformBlocks ||
            inlineUniformBlockProperties.maxDescriptorSetUpdateAfterBindInlineUniformBlocks !=
                vulkan13Properties.maxDescriptorSetUpdateAfterBindInlineUniformBlocks)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceInlineUniformBlockProperties and VkPhysicalDeviceVulkan13Properties");
        }

        if (shaderIntegerDotProductProperties.integerDotProduct8BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProduct8BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct8BitSignedAccelerated !=
                vulkan13Properties.integerDotProduct8BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct8BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProduct8BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct4x8BitPackedUnsignedAccelerated !=
                vulkan13Properties.integerDotProduct4x8BitPackedUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct4x8BitPackedSignedAccelerated !=
                vulkan13Properties.integerDotProduct4x8BitPackedSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct4x8BitPackedMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProduct4x8BitPackedMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct16BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProduct16BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct16BitSignedAccelerated !=
                vulkan13Properties.integerDotProduct16BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct16BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProduct16BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct32BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProduct32BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct32BitSignedAccelerated !=
                vulkan13Properties.integerDotProduct32BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct32BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProduct32BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct64BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProduct64BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct64BitSignedAccelerated !=
                vulkan13Properties.integerDotProduct64BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProduct64BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProduct64BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating8BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating8BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating8BitSignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating8BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated ||
            shaderIntegerDotProductProperties
                    .integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating16BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating16BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating16BitSignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating16BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating32BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating32BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating32BitSignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating32BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating64BitUnsignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating64BitUnsignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating64BitSignedAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating64BitSignedAccelerated ||
            shaderIntegerDotProductProperties.integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated !=
                vulkan13Properties.integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceShaderIntegerDotProductProperties and "
                     "VkPhysicalDeviceVulkan13Properties");
        }

        if (texelBufferAlignmentProperties.storageTexelBufferOffsetAlignmentBytes !=
                vulkan13Properties.storageTexelBufferOffsetAlignmentBytes ||
            texelBufferAlignmentProperties.storageTexelBufferOffsetSingleTexelAlignment !=
                vulkan13Properties.storageTexelBufferOffsetSingleTexelAlignment ||
            texelBufferAlignmentProperties.uniformTexelBufferOffsetAlignmentBytes !=
                vulkan13Properties.uniformTexelBufferOffsetAlignmentBytes ||
            texelBufferAlignmentProperties.uniformTexelBufferOffsetSingleTexelAlignment !=
                vulkan13Properties.uniformTexelBufferOffsetSingleTexelAlignment)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceTexelBufferAlignmentProperties and "
                     "VkPhysicalDeviceVulkan13Properties");
        }

        if (maintenance4Properties.maxBufferSize != vulkan13Properties.maxBufferSize)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceMaintenance4Properties and VkPhysicalDeviceVulkan13Properties");
        }
    }

    return tcu::TestStatus::pass("Vulkan 1.3 device properties are consistent with extension properties");
}

tcu::TestStatus devicePropertyExtensionsConsistencyVulkan14(Context &context)
{
    TestLog &log                          = context.getTestContext().getLog();
    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    if (!context.contextSupports(vk::ApiVersion(0, 1, 4, 0)))
        TCU_THROW(NotSupportedError, "At least Vulkan 1.4 required to run test");

    VkPhysicalDeviceVulkan14Properties vulkan14Properties = initVulkanStructure();
    VkPhysicalDeviceProperties2 extProperties             = initVulkanStructure(&vulkan14Properties);
    std::vector<VkImageLayout> vulkan14CopyLayouts;
    std::vector<VkImageLayout> extCopyLayouts;

    vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

    vulkan14CopyLayouts.resize(vulkan14Properties.copySrcLayoutCount + vulkan14Properties.copyDstLayoutCount);
    vulkan14Properties.pCopySrcLayouts = vulkan14CopyLayouts.data();
    vulkan14Properties.pCopyDstLayouts = vulkan14CopyLayouts.data() + vulkan14Properties.copySrcLayoutCount;
    vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

    log << TestLog::Message << vulkan14Properties << TestLog::EndMessage;

    // Validate all fields initialized matching to extension structures
    {
        VkPhysicalDeviceLineRasterizationPropertiesKHR lineRasterizationProperties = initVulkanStructure();
        VkPhysicalDeviceMaintenance5PropertiesKHR maintenance5Properties =
            initVulkanStructure(&lineRasterizationProperties);
        VkPhysicalDeviceMaintenance6PropertiesKHR maintenance6Properties = initVulkanStructure(&maintenance5Properties);
        VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties =
            initVulkanStructure(&maintenance6Properties);
        VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR vertexAttributeDivisorProperties =
            initVulkanStructure(&pushDescriptorProperties);
        VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties =
            initVulkanStructure(&vertexAttributeDivisorProperties);
        VkPhysicalDevicePipelineRobustnessPropertiesEXT pipelineRobustnessProperties =
            initVulkanStructure(&hostImageCopyProperties);
        extProperties = initVulkanStructure(&pipelineRobustnessProperties);

        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

        // alocate and fill pCopySrcLayouts/pCopyDstLayouts data
        extCopyLayouts.resize(hostImageCopyProperties.copySrcLayoutCount + hostImageCopyProperties.copyDstLayoutCount);
        hostImageCopyProperties.pCopySrcLayouts = extCopyLayouts.data();
        hostImageCopyProperties.pCopyDstLayouts = extCopyLayouts.data() + hostImageCopyProperties.copySrcLayoutCount;
        hostImageCopyProperties.pNext           = nullptr;
        extProperties.pNext                     = &hostImageCopyProperties;
        vki.getPhysicalDeviceProperties2(physicalDevice, &extProperties);

        if (lineRasterizationProperties.lineSubPixelPrecisionBits != vulkan14Properties.lineSubPixelPrecisionBits)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceLineRasterizationPropertiesKHR and "
                     "VkPhysicalDeviceVulkan14Properties");
        }

        if (maintenance5Properties.earlyFragmentMultisampleCoverageAfterSampleCounting !=
                vulkan14Properties.earlyFragmentMultisampleCoverageAfterSampleCounting ||
            maintenance5Properties.earlyFragmentSampleMaskTestBeforeSampleCounting !=
                vulkan14Properties.earlyFragmentSampleMaskTestBeforeSampleCounting ||
            maintenance5Properties.depthStencilSwizzleOneSupport != vulkan14Properties.depthStencilSwizzleOneSupport ||
            maintenance5Properties.polygonModePointSize != vulkan14Properties.polygonModePointSize ||
            maintenance5Properties.nonStrictSinglePixelWideLinesUseParallelogram !=
                vulkan14Properties.nonStrictSinglePixelWideLinesUseParallelogram ||
            maintenance5Properties.nonStrictWideLinesUseParallelogram !=
                vulkan14Properties.nonStrictWideLinesUseParallelogram)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceMaintenance5PropertiesKHR and VkPhysicalDeviceVulkan14Properties");
        }

        if (maintenance6Properties.blockTexelViewCompatibleMultipleLayers !=
                vulkan14Properties.blockTexelViewCompatibleMultipleLayers ||
            maintenance6Properties.maxCombinedImageSamplerDescriptorCount !=
                vulkan14Properties.maxCombinedImageSamplerDescriptorCount ||
            maintenance6Properties.fragmentShadingRateClampCombinerInputs !=
                vulkan14Properties.fragmentShadingRateClampCombinerInputs)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceMaintenance6PropertiesKHR and VkPhysicalDeviceVulkan14Properties");
        }

        if (pushDescriptorProperties.maxPushDescriptors != vulkan14Properties.maxPushDescriptors)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDevicePushDescriptorPropertiesKHR and VkPhysicalDeviceVulkan14Properties");
        }

        if (vertexAttributeDivisorProperties.maxVertexAttribDivisor != vulkan14Properties.maxVertexAttribDivisor ||
            vertexAttributeDivisorProperties.supportsNonZeroFirstInstance !=
                vulkan14Properties.supportsNonZeroFirstInstance)
        {
            TCU_FAIL("Mismatch between VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR and "
                     "VkPhysicalDeviceVulkan14Properties");
        }

        if (hostImageCopyProperties.copySrcLayoutCount != vulkan14Properties.copySrcLayoutCount ||
            (deMemCmp(hostImageCopyProperties.pCopySrcLayouts, vulkan14Properties.pCopySrcLayouts,
                      vulkan14Properties.copySrcLayoutCount) != 0) ||
            hostImageCopyProperties.copyDstLayoutCount != vulkan14Properties.copyDstLayoutCount ||
            (deMemCmp(hostImageCopyProperties.pCopyDstLayouts, vulkan14Properties.pCopyDstLayouts,
                      vulkan14Properties.copyDstLayoutCount) != 0) ||
            (deMemCmp(hostImageCopyProperties.optimalTilingLayoutUUID, vulkan14Properties.optimalTilingLayoutUUID,
                      VK_LUID_SIZE) != 0) ||
            hostImageCopyProperties.identicalMemoryTypeRequirements !=
                vulkan14Properties.identicalMemoryTypeRequirements)
        {
            TCU_FAIL(
                "Mismatch between VkPhysicalDeviceHostImageCopyPropertiesEXT and VkPhysicalDeviceVulkan14Properties");
        }

        if (pipelineRobustnessProperties.defaultRobustnessStorageBuffers !=
                vulkan14Properties.defaultRobustnessStorageBuffers ||
            pipelineRobustnessProperties.defaultRobustnessUniformBuffers !=
                vulkan14Properties.defaultRobustnessUniformBuffers ||
            pipelineRobustnessProperties.defaultRobustnessVertexInputs !=
                vulkan14Properties.defaultRobustnessVertexInputs ||
            pipelineRobustnessProperties.defaultRobustnessImages != vulkan14Properties.defaultRobustnessImages)
        {
            TCU_FAIL("Mismatch between VkPhysicalDevicePipelineRobustnessPropertiesEXT and "
                     "VkPhysicalDeviceVulkan14Properties");
        }
    }

    return tcu::TestStatus::pass("Vulkan 1.4 device properties are consistent with extension properties");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus imageFormatProperties2(Context &context, const VkFormat format, const VkImageType imageType,
                                       const VkImageTiling tiling)
{
    if (isYCbCrFormat(format))
        // check if Ycbcr format enums are valid given the version and extensions
        checkYcbcrApiSupport(context);

    TestLog &log = context.getTestContext().getLog();

    const InstanceInterface &vki          = context.getInstanceInterface(); // "VK_KHR_get_physical_device_properties2"
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkImageCreateFlags ycbcrFlags =
        isYCbCrFormat(format) ? (VkImageCreateFlags)VK_IMAGE_CREATE_DISJOINT_BIT : (VkImageCreateFlags)0u;
    const VkImageUsageFlags allUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    const VkImageCreateFlags allCreateFlags =
        VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT |
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT | ycbcrFlags;

    for (VkImageUsageFlags curUsageFlags = (VkImageUsageFlags)1; curUsageFlags <= allUsageFlags; curUsageFlags++)
    {
        if (!isValidImageUsageFlagCombination(curUsageFlags))
            continue;

        for (VkImageCreateFlags curCreateFlags = 0; curCreateFlags <= allCreateFlags; curCreateFlags++)
        {
            const VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                nullptr,
                format,
                imageType,
                tiling,
                curUsageFlags,
                curCreateFlags};

            VkImageFormatProperties coreProperties;
            VkImageFormatProperties2 extProperties;
            VkResult coreResult;
            VkResult extResult;

            deMemset(&coreProperties, 0xcd, sizeof(VkImageFormatProperties));
            deMemset(&extProperties, 0xcd, sizeof(VkImageFormatProperties2));

            extProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
            extProperties.pNext = nullptr;

            coreResult = vki.getPhysicalDeviceImageFormatProperties(
                physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.tiling,
                imageFormatInfo.usage, imageFormatInfo.flags, &coreProperties);
            extResult = vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &extProperties);

            TCU_CHECK(extProperties.sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);
            TCU_CHECK(extProperties.pNext == nullptr);

            if ((coreResult != extResult) ||
                (deMemCmp(&coreProperties, &extProperties.imageFormatProperties, sizeof(VkImageFormatProperties)) != 0))
            {
                log << TestLog::Message << "ERROR: device mismatch with query " << imageFormatInfo
                    << TestLog::EndMessage << TestLog::Message << "vkGetPhysicalDeviceImageFormatProperties() returned "
                    << coreResult << ", " << coreProperties << TestLog::EndMessage << TestLog::Message
                    << "vkGetPhysicalDeviceImageFormatProperties2() returned " << extResult << ", " << extProperties
                    << TestLog::EndMessage;
                TCU_FAIL("Mismatch between image format properties reported by "
                         "vkGetPhysicalDeviceImageFormatProperties and vkGetPhysicalDeviceImageFormatProperties2");
            }
        }
    }

    return tcu::TestStatus::pass("Querying image format properties succeeded");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus sparseImageFormatProperties2(Context &context, const VkFormat format, const VkImageType imageType,
                                             const VkImageTiling tiling)
{
    TestLog &log = context.getTestContext().getLog();

    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkImageUsageFlags allUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    for (uint32_t sampleCountBit = VK_SAMPLE_COUNT_1_BIT; sampleCountBit <= VK_SAMPLE_COUNT_64_BIT;
         sampleCountBit          = (sampleCountBit << 1u))
    {
        for (VkImageUsageFlags curUsageFlags = (VkImageUsageFlags)1; curUsageFlags <= allUsageFlags; curUsageFlags++)
        {
            if (!isValidImageUsageFlagCombination(curUsageFlags))
                continue;

            const VkPhysicalDeviceSparseImageFormatInfo2 imageFormatInfo = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2,
                nullptr,
                format,
                imageType,
                (VkSampleCountFlagBits)sampleCountBit,
                curUsageFlags,
                tiling,
            };

            uint32_t numCoreProperties = 0u;
            uint32_t numExtProperties  = 0u;

            // Query count
            vki.getPhysicalDeviceSparseImageFormatProperties(
                physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.samples,
                imageFormatInfo.usage, imageFormatInfo.tiling, &numCoreProperties, nullptr);
            vki.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &imageFormatInfo, &numExtProperties,
                                                              nullptr);

            if (numCoreProperties != numExtProperties)
            {
                log << TestLog::Message << "ERROR: different number of properties reported for " << imageFormatInfo
                    << TestLog::EndMessage;
                TCU_FAIL("Mismatch in reported property count");
            }

            if (!context.getDeviceFeatures().sparseBinding)
            {
                // There is no support for sparse binding, getPhysicalDeviceSparseImageFormatProperties* MUST report no properties
                // Only have to check one of the entrypoints as a mismatch in count is already caught.
                if (numCoreProperties > 0)
                {
                    log << TestLog::Message << "ERROR: device does not support sparse binding but claims support for "
                        << numCoreProperties
                        << " properties in vkGetPhysicalDeviceSparseImageFormatProperties with parameters "
                        << imageFormatInfo << TestLog::EndMessage;
                    TCU_FAIL("Claimed format properties inconsistent with overall sparseBinding feature");
                }
            }

            if (numCoreProperties > 0)
            {
                std::vector<VkSparseImageFormatProperties> coreProperties(numCoreProperties);
                std::vector<VkSparseImageFormatProperties2> extProperties(numExtProperties);

                deMemset(&coreProperties[0], 0xcd, sizeof(VkSparseImageFormatProperties) * numCoreProperties);
                deMemset(&extProperties[0], 0xcd, sizeof(VkSparseImageFormatProperties2) * numExtProperties);

                for (uint32_t ndx = 0; ndx < numExtProperties; ++ndx)
                {
                    extProperties[ndx].sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2;
                    extProperties[ndx].pNext = nullptr;
                }

                vki.getPhysicalDeviceSparseImageFormatProperties(
                    physicalDevice, imageFormatInfo.format, imageFormatInfo.type, imageFormatInfo.samples,
                    imageFormatInfo.usage, imageFormatInfo.tiling, &numCoreProperties, &coreProperties[0]);
                vki.getPhysicalDeviceSparseImageFormatProperties2(physicalDevice, &imageFormatInfo, &numExtProperties,
                                                                  &extProperties[0]);

                TCU_CHECK((size_t)numCoreProperties == coreProperties.size());
                TCU_CHECK((size_t)numExtProperties == extProperties.size());

                for (uint32_t ndx = 0; ndx < numCoreProperties; ++ndx)
                {
                    TCU_CHECK(extProperties[ndx].sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2);
                    TCU_CHECK(extProperties[ndx].pNext == nullptr);

                    if ((deMemCmp(&coreProperties[ndx], &extProperties[ndx].properties,
                                  sizeof(VkSparseImageFormatProperties)) != 0))
                    {
                        log << TestLog::Message << "ERROR: device mismatch with query " << imageFormatInfo
                            << " property " << ndx << TestLog::EndMessage << TestLog::Message
                            << "vkGetPhysicalDeviceSparseImageFormatProperties() returned " << coreProperties[ndx]
                            << TestLog::EndMessage << TestLog::Message
                            << "vkGetPhysicalDeviceSparseImageFormatProperties2() returned " << extProperties[ndx]
                            << TestLog::EndMessage;
                        TCU_FAIL("Mismatch between image format properties reported by "
                                 "vkGetPhysicalDeviceSparseImageFormatProperties and "
                                 "vkGetPhysicalDeviceSparseImageFormatProperties2");
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Querying sparse image format properties succeeded");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus execImageFormatTest(Context &context, ImageFormatPropertyCase testCase)
{
    return testCase.testFunction(context, testCase.format, testCase.imageType, testCase.tiling);
}

void createImageFormatTypeTilingTests(tcu::TestCaseGroup *testGroup, ImageFormatPropertyCase params)
{
    DE_ASSERT(params.format == VK_FORMAT_UNDEFINED);

    static const struct
    {
        VkFormat begin;
        VkFormat end;
        ImageFormatPropertyCase params;
    } s_formatRanges[] = {
        // core formats
        {(VkFormat)(VK_FORMAT_UNDEFINED), VK_CORE_FORMAT_LAST, params},

        // YCbCr formats
        {VK_FORMAT_G8B8G8R8_422_UNORM, (VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM + 1), params},

        // YCbCr extended formats
        {VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT, (VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT + 1), params},
    };

    for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
    {
        const VkFormat rangeBegin = s_formatRanges[rangeNdx].begin;
        const VkFormat rangeEnd   = s_formatRanges[rangeNdx].end;

        for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format + 1))
        {
            const bool isYCbCr = isYCbCrFormat(format);
#ifndef CTS_USES_VULKANSC
            const bool isSparse = (params.testFunction == sparseImageFormatProperties2);
#else
            const bool isSparse = false;
#endif // CTS_USES_VULKANSC

            if (isYCbCr && isSparse)
                continue;

            if (isYCbCr && params.imageType != VK_IMAGE_TYPE_2D)
                continue;

            const char *const enumName = getFormatName(format);
            const string caseName      = de::toLower(string(enumName).substr(10));

            params.format = format;

            addFunctionCase<ImageFormatPropertyCase, CustomInstanceArg0Test<ImageFormatPropertyCase, E060>>(
                testGroup, caseName, execImageFormatTest, params);
        }
    }
}

void createImageFormatTypeTests(tcu::TestCaseGroup *testGroup, ImageFormatPropertyCase params)
{
    DE_ASSERT(params.tiling == VK_CORE_IMAGE_TILING_LAST);

    testGroup->addChild(createTestGroup(
        testGroup->getTestContext(), "optimal", createImageFormatTypeTilingTests,
        ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_OPTIMAL)));
    testGroup->addChild(createTestGroup(
        testGroup->getTestContext(), "linear", createImageFormatTypeTilingTests,
        ImageFormatPropertyCase(params.testFunction, VK_FORMAT_UNDEFINED, params.imageType, VK_IMAGE_TILING_LINEAR)));
}

void createImageFormatTests(tcu::TestCaseGroup *testGroup, ImageFormatPropertyCase::Function testFunction)
{
    testGroup->addChild(createTestGroup(
        testGroup->getTestContext(), "1d", createImageFormatTypeTests,
        ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_1D, VK_CORE_IMAGE_TILING_LAST)));
    testGroup->addChild(createTestGroup(
        testGroup->getTestContext(), "2d", createImageFormatTypeTests,
        ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_2D, VK_CORE_IMAGE_TILING_LAST)));
    testGroup->addChild(createTestGroup(
        testGroup->getTestContext(), "3d", createImageFormatTypeTests,
        ImageFormatPropertyCase(testFunction, VK_FORMAT_UNDEFINED, VK_IMAGE_TYPE_3D, VK_CORE_IMAGE_TILING_LAST)));
}

tcu::TestStatus execImageUsageTest(Context &context, ImageUsagePropertyCase testCase)
{
    return testCase.testFunction(context, testCase.format, testCase.usage, testCase.tiling);
}

void checkSupportImageUsage(Context &context, ImageUsagePropertyCase params)
{
    if (params.usage == VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)
        context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");
}

void createImageUsageTilingTests(tcu::TestCaseGroup *testGroup, ImageUsagePropertyCase params)
{
    struct UsageTest
    {
        const char *name;
        VkImageUsageFlags usage;
    } usageTests[] = {
        {"sampled", VK_IMAGE_USAGE_SAMPLED_BIT},
        {"storage", VK_IMAGE_USAGE_STORAGE_BIT},
        {"color_attachment", VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
        {"depth_stencil_attachment", VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        {"input_attachment", VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT},
        {"fragment_shading_rate_attachment", VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR},
    };

    for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(usageTests); ++rangeNdx)
    {
        const auto usage = usageTests[rangeNdx];

        const VkFormat rangeBegin = VK_FORMAT_UNDEFINED;
        const VkFormat rangeEnd   = VK_CORE_FORMAT_LAST;

        for (VkFormat format = rangeBegin; format != rangeEnd; format = (VkFormat)(format + 1))
        {
            const char *const enumName = getFormatName(format);
            const string caseName      = std::string(usage.name) + "_" + de::toLower(string(enumName).substr(10));

            params.usage  = usage.usage;
            params.format = format;

            addFunctionCase(testGroup, caseName, checkSupportImageUsage, execImageUsageTest, params);
        }
    }
}

void createImageUsageTests(tcu::TestCaseGroup *testGroup, ImageUsagePropertyCase::Function testFunction)
{
    testGroup->addChild(
        createTestGroup(testGroup->getTestContext(), "optimal", createImageUsageTilingTests,
                        ImageUsagePropertyCase(testFunction, VK_FORMAT_UNDEFINED, 0u, VK_IMAGE_TILING_OPTIMAL)));
    testGroup->addChild(
        createTestGroup(testGroup->getTestContext(), "linear", createImageUsageTilingTests,
                        ImageUsagePropertyCase(testFunction, VK_FORMAT_UNDEFINED, 0u, VK_IMAGE_TILING_LINEAR)));
}

// Android CTS -specific tests

namespace android
{

void checkSupportAndroid(Context &)
{
#if (DE_OS != DE_OS_ANDROID)
    TCU_THROW(NotSupportedError, "Test is only for Android");
#endif
}

void checkExtensions(tcu::ResultCollector &results, const set<string> &allowedExtensions,
                     const vector<VkExtensionProperties> &reportedExtensions)
{
    for (vector<VkExtensionProperties>::const_iterator extension = reportedExtensions.begin();
         extension != reportedExtensions.end(); ++extension)
    {
        const string extensionName(extension->extensionName);
        const bool mustBeKnown =
            de::beginsWith(extensionName, "VK_GOOGLE_") || de::beginsWith(extensionName, "VK_ANDROID_");

        if (mustBeKnown && !de::contains(allowedExtensions, extensionName))
            results.fail("Unknown extension: " + extensionName);
    }
}

tcu::TestStatus testNoUnknownExtensions(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);
    set<string> allowedInstanceExtensions;
    set<string> allowedDeviceExtensions;

    // All known extensions should be added to allowedExtensions:
    // allowedExtensions.insert("VK_GOOGLE_extension1");
    allowedDeviceExtensions.insert("VK_ANDROID_external_format_resolve");
    allowedDeviceExtensions.insert("VK_ANDROID_external_memory_android_hardware_buffer");
    allowedDeviceExtensions.insert("VK_GOOGLE_display_timing");
    allowedDeviceExtensions.insert("VK_GOOGLE_decorate_string");
    allowedDeviceExtensions.insert("VK_GOOGLE_hlsl_functionality1");
    allowedDeviceExtensions.insert("VK_GOOGLE_user_type");
    allowedInstanceExtensions.insert("VK_GOOGLE_surfaceless_query");

    // Instance extensions
    checkExtensions(results, allowedInstanceExtensions,
                    enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr));

    // Extensions exposed by instance layers
    {
        const vector<VkLayerProperties> layers = enumerateInstanceLayerProperties(context.getPlatformInterface());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
        {
            checkExtensions(results, allowedInstanceExtensions,
                            enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName));
        }
    }

    // Device extensions
    checkExtensions(
        results, allowedDeviceExtensions,
        enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), nullptr));

    // Extensions exposed by device layers
    {
        const vector<VkLayerProperties> layers =
            enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
        {
            checkExtensions(results, allowedDeviceExtensions,
                            enumerateDeviceExtensionProperties(context.getInstanceInterface(),
                                                               context.getPhysicalDevice(), layer->layerName));
        }
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testNoLayers(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    {
        const vector<VkLayerProperties> layers = enumerateInstanceLayerProperties(context.getPlatformInterface());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
            results.fail(string("Instance layer enumerated: ") + layer->layerName);
    }

    {
        const vector<VkLayerProperties> layers =
            enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

        for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
            results.fail(string("Device layer enumerated: ") + layer->layerName);
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testMandatoryExtensions(Context &context)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    // Instance extensions
    {
        static const string mandatoryExtensions[] = {
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
        static const string mandatoryExtensions[] = {
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

} // namespace android

enum FormatPropsPNextFlagBits
{
    PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST   = 2,
    PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2 = 1,
    PNEXT_FORMAT_PROPERTIES_3                   = 4,
#ifndef CTS_USES_VULKANSC
    PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY = 8,
#endif // CTS_USES_VULKANSC
};

using FormatPropsPNextFlags = uint32_t;

struct FormatPropsPNextParams
{
    VkFormat format;
    FormatPropsPNextFlags pNextFlags;
};

class FormatPropsTest : public vkt::TestInstance
{
public:
    FormatPropsTest(Context &context, const FormatPropsPNextParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~FormatPropsTest(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const FormatPropsPNextParams m_params;
};

class FormatPropsCase : public vkt::TestCase
{
public:
    FormatPropsCase(tcu::TestContext &testCtx, const std::string &name, const FormatPropsPNextParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~FormatPropsCase(void) = default;

    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new FormatPropsTest(context, m_params);
    }

protected:
    const FormatPropsPNextParams m_params;
};

void FormatPropsCase::checkSupport(Context &context) const
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

    if ((m_params.pNextFlags & PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST) ||
        (m_params.pNextFlags & PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2))
        context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");

    if ((m_params.pNextFlags & PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2) ||
        (m_params.pNextFlags & PNEXT_FORMAT_PROPERTIES_3))
        context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

#ifndef CTS_USES_VULKANSC
    if (m_params.pNextFlags & PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY)
        context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_single_sampled");
#endif // CTS_USES_VULKANSC
}

tcu::TestStatus FormatPropsTest::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    VkFormatProperties2 basicProps = initVulkanStructure();

    ctx.vki.getPhysicalDeviceFormatProperties2(ctx.physicalDevice, m_params.format, &basicProps);

    VkFormatProperties2 retryProps = initVulkanStructure();
    const auto addProperties       = makeStructChainAdder(&retryProps);

    VkDrmFormatModifierPropertiesListEXT drmModProps   = initVulkanStructure();
    VkDrmFormatModifierPropertiesList2EXT drmModProps2 = initVulkanStructure();
    VkFormatProperties3 props3                         = initVulkanStructure();
#ifndef CTS_USES_VULKANSC
    VkSubpassResolvePerformanceQueryEXT subpassResolveProps = initVulkanStructure();
#endif // CTS_USES_VULKANSC

    if (m_params.pNextFlags & PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST)
        addProperties(&drmModProps);

    if (m_params.pNextFlags & PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2)
        addProperties(&drmModProps2);

    if (m_params.pNextFlags & PNEXT_FORMAT_PROPERTIES_3)
        addProperties(&props3);

#ifndef CTS_USES_VULKANSC
    if (m_params.pNextFlags & PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY)
        addProperties(&subpassResolveProps);
#endif // CTS_USES_VULKANSC

    ctx.vki.getPhysicalDeviceFormatProperties2(ctx.physicalDevice, m_params.format, &retryProps);

    if (basicProps.formatProperties.bufferFeatures != retryProps.formatProperties.bufferFeatures)
        TCU_FAIL("Mismatch in bufferFeatures");

    if (basicProps.formatProperties.linearTilingFeatures != retryProps.formatProperties.linearTilingFeatures)
        TCU_FAIL("Mismatch in linearTilingFeatures");

    if (basicProps.formatProperties.optimalTilingFeatures != retryProps.formatProperties.optimalTilingFeatures)
        TCU_FAIL("Mismatch in optimalTilingFeatures");

    if (m_params.pNextFlags & PNEXT_FORMAT_PROPERTIES_3)
    {
        const auto basicBufferFeatures2 =
            static_cast<VkFormatFeatureFlags2>(basicProps.formatProperties.bufferFeatures);
        const auto basicLinearTilingFeatures2 =
            static_cast<VkFormatFeatureFlags2>(basicProps.formatProperties.linearTilingFeatures);
        const auto basicOptimalTilingFeatures2 =
            static_cast<VkFormatFeatureFlags2>(basicProps.formatProperties.optimalTilingFeatures);

        if ((basicBufferFeatures2 & props3.bufferFeatures) != basicBufferFeatures2)
            TCU_FAIL("Mismatch in bufferFeatures from VkFormatProperties3");

        if ((basicLinearTilingFeatures2 & props3.linearTilingFeatures) != basicLinearTilingFeatures2)
            TCU_FAIL("Mismatch in linearTilingFeatures from VkFormatProperties3");

        if ((basicOptimalTilingFeatures2 & props3.optimalTilingFeatures) != basicOptimalTilingFeatures2)
            TCU_FAIL("Mismatch in optimalTilingFeatures from VkFormatProperties3");
    }

    return tcu::TestStatus::pass("Pass");
}

std::string getFormatSimpleName(VkFormat format)
{
    static size_t prefixLen    = std::strlen("VK_FORMAT_");
    const std::string fullName = getFormatName(format);
    return de::toLower(fullName.substr(prefixLen));
}

} // namespace

static inline void addFunctionCaseInNewSubgroup(tcu::TestContext &testCtx, tcu::TestCaseGroup *group,
                                                const std::string &subgroupName, FunctionInstance0::Function testFunc,
                                                bool needsCustomInstance = true)
{
    de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, subgroupName.c_str()));
    // Actually all the tests that are registered with using this function needs a cusom instance.
    // It is hard to say what scenario it will be used with in the future so this is a reason for
    // adding of needsCustomInstance parameter.
    if (needsCustomInstance)
        addFunctionCase<CustomInstanceTest<E071>>(subgroup.get(), "basic", testFunc);
    else
        addFunctionCase(subgroup.get(), "basic", testFunc);
    group->addChild(subgroup.release());
}

tcu::TestCaseGroup *createFeatureInfoTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> infoTests(new tcu::TestCaseGroup(testCtx, "info"));

    infoTests->addChild(createTestGroup(testCtx, "format_properties", createFormatTests));
    infoTests->addChild(
        createTestGroup(testCtx, "image_format_properties", createImageFormatTests, imageFormatProperties));
    infoTests->addChild(
        createTestGroup(testCtx, "unsupported_image_usage", createImageUsageTests, unsupportedImageUsage));

    {
        de::MovePtr<tcu::TestCaseGroup> extCoreVersionGrp(new tcu::TestCaseGroup(testCtx, "extension_core_versions"));

        addFunctionCase(extCoreVersionGrp.get(), "extension_core_versions", extensionCoreVersions);

        infoTests->addChild(extCoreVersionGrp.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests(
            new tcu::TestCaseGroup(testCtx, "get_physical_device_properties2"));

        {
            de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, "features"));
            // Extended Device Features
            addFunctionCase<CustomInstanceTest<E060>>(subgroup.get(), "core", deviceFeatures2);
            addSeparateFeatureTests(subgroup.get());
#ifndef CTS_USES_VULKANSC
            addFunctionCase<CustomInstanceWithSupportTest<E060>>(
                subgroup.get(), "shader_subgroup_rotate_property_consistency_khr", checkSupportKhrShaderSubgroupRotate,
                subgroupRotatePropertyExtensionFeatureConsistency);
#endif // CTS_USES_VULKANSC
            extendedPropertiesTests->addChild(subgroup.release());
        }
        addFunctionCaseInNewSubgroup(testCtx, extendedPropertiesTests.get(), "properties", deviceProperties2);
        addFunctionCaseInNewSubgroup(testCtx, extendedPropertiesTests.get(), "format_properties",
                                     deviceFormatProperties2);
        addFunctionCaseInNewSubgroup(testCtx, extendedPropertiesTests.get(), "queue_family_properties",
                                     deviceQueueFamilyProperties2);
        addFunctionCaseInNewSubgroup(testCtx, extendedPropertiesTests.get(), "memory_properties",
                                     deviceMemoryProperties2);

        {
            de::MovePtr<tcu::TestCaseGroup> formatPropertiesPNextGroup(
                new tcu::TestCaseGroup(testCtx, "pnext_format_properties"));

            // Test all the basic formats that do not require extensions.
            const VkFormat lastFomat = VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
            VkFormat testFormat      = VK_FORMAT_UNDEFINED;

            const struct
            {
                FormatPropsPNextFlags pNextFlags;
                const char *flagsCaseName;
            } flagsCases[] = {
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST), "drm_format_mod_1"},
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2), "drm_format_mod_2"},
                {(PNEXT_FORMAT_PROPERTIES_3), "format_props_3"},
#ifndef CTS_USES_VULKANSC
                {(PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY), "subpass_resolve_query"},
#endif // CTS_USES_VULKANSC
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST | PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2),
                 "drm_format_mod_1_and_2"},
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST | PNEXT_FORMAT_PROPERTIES_3),
                 "drm_format_mod_1_and_format_props_3"},
#ifndef CTS_USES_VULKANSC
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST | PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY),
                 "drm_format_mod_1_and_subpass_resolve_query"},
#endif // CTS_USES_VULKANSC
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2 | PNEXT_FORMAT_PROPERTIES_3),
                 "drm_format_mod_2_and_format_props_3"},
#ifndef CTS_USES_VULKANSC
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2 | PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY),
                 "drm_format_mod_2_and_subpass_resolve_query"},
                {(PNEXT_FORMAT_PROPERTIES_3 | PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY),
                 "format_props_3_and_subpass_resolve_query"},
#endif // CTS_USES_VULKANSC
                {(PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST | PNEXT_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2 |
                  PNEXT_FORMAT_PROPERTIES_3
#ifndef CTS_USES_VULKANSC
                  | PNEXT_SUBPASS_RESOLVE_PERFORMANCE_QUERY
#endif // CTS_USES_VULKANSC
                  ),
                 "all_format_props"},
            };

            for (;;)
            {
                // Pick the next format.
                testFormat = static_cast<VkFormat>(static_cast<int>(testFormat) + 1);
                if (testFormat > lastFomat)
                    break;

                const std::string formatName = getFormatSimpleName(testFormat);
                de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str()));

                for (const auto &flagsCase : flagsCases)
                {
                    const FormatPropsPNextParams params{testFormat, flagsCase.pNextFlags};
                    formatGroup->addChild(new FormatPropsCase(testCtx, flagsCase.flagsCaseName, params));
                }

                formatPropertiesPNextGroup->addChild(formatGroup.release());
            }

            extendedPropertiesTests->addChild(formatPropertiesPNextGroup.release());
        }

        infoTests->addChild(extendedPropertiesTests.release());
    }

    {
        // Vulkan 1.2 related tests
        de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests(new tcu::TestCaseGroup(testCtx, "vulkan1p2"));

        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "features", deviceFeaturesVulkan12);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "properties",
                                                  devicePropertiesVulkan12);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "feature_extensions_consistency",
                                                  deviceFeatureExtensionsConsistencyVulkan12);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "property_extensions_consistency",
                                                  devicePropertyExtensionsConsistencyVulkan12);
        addFunctionCase(extendedPropertiesTests.get(), "feature_bits_influence", checkApiVersionSupport<1, 2>,
                        featureBitInfluenceOnDeviceCreate<VK_API_VERSION_1_2>);

        infoTests->addChild(extendedPropertiesTests.release());
    }

#ifndef CTS_USES_VULKANSC
    {
        // Vulkan 1.3 related tests
        de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests(new tcu::TestCaseGroup(testCtx, "vulkan1p3"));

        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "features", deviceFeaturesVulkan13);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "properties",
                                                  devicePropertiesVulkan13);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "feature_extensions_consistency",
                                                  deviceFeatureExtensionsConsistencyVulkan13);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "property_extensions_consistency",
                                                  devicePropertyExtensionsConsistencyVulkan13);
        addFunctionCase(extendedPropertiesTests.get(), "feature_bits_influence", checkApiVersionSupport<1, 3>,
                        featureBitInfluenceOnDeviceCreate<VK_API_VERSION_1_3>);

        infoTests->addChild(extendedPropertiesTests.release());
    }
    {
        // Vulkan 1.4 related tests
        de::MovePtr<tcu::TestCaseGroup> extendedPropertiesTests(new tcu::TestCaseGroup(testCtx, "vulkan1p4"));

        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "features", deviceFeaturesVulkan14);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "properties",
                                                  devicePropertiesVulkan14);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "feature_extensions_consistency",
                                                  deviceFeatureExtensionsConsistencyVulkan14);
        addFunctionCase<CustomInstanceTest<E060>>(extendedPropertiesTests.get(), "property_extensions_consistency",
                                                  devicePropertyExtensionsConsistencyVulkan14);
        addFunctionCase(extendedPropertiesTests.get(), "feature_bits_influence", checkApiVersionSupport<1, 4>,
                        featureBitInfluenceOnDeviceCreate<VK_API_VERSION_1_4>);

        infoTests->addChild(extendedPropertiesTests.release());
    }
#endif // CTS_USES_VULKANSC

    {
        de::MovePtr<tcu::TestCaseGroup> limitsValidationTests(
            new tcu::TestCaseGroup(testCtx, "vulkan1p2_limits_validation"));

        addFunctionCase(limitsValidationTests.get(), "general", checkApiVersionSupport<1, 2>, validateLimits12);
#ifndef CTS_USES_VULKANSC
        // Removed from Vulkan SC test set: VK_KHR_push_descriptor extension removed from Vulkan SC
        addFunctionCase(limitsValidationTests.get(), "khr_push_descriptor", checkSupportKhrPushDescriptor,
                        validateLimitsKhrPushDescriptor);
#endif // CTS_USES_VULKANSC
        addFunctionCase(limitsValidationTests.get(), "khr_multiview", checkSupportKhrMultiview,
                        validateLimitsKhrMultiview);
        addFunctionCase(limitsValidationTests.get(), "ext_discard_rectangles", checkSupportExtDiscardRectangles,
                        validateLimitsExtDiscardRectangles);
        addFunctionCase(limitsValidationTests.get(), "ext_sample_locations", checkSupportExtSampleLocations,
                        validateLimitsExtSampleLocations);
        addFunctionCase(limitsValidationTests.get(), "ext_external_memory_host", checkSupportExtExternalMemoryHost,
                        validateLimitsExtExternalMemoryHost);
        addFunctionCase(limitsValidationTests.get(), "ext_blend_operation_advanced",
                        checkSupportExtBlendOperationAdvanced, validateLimitsExtBlendOperationAdvanced);
        addFunctionCase(limitsValidationTests.get(), "khr_maintenance_3", checkSupportKhrMaintenance3,
                        validateLimitsKhrMaintenance3);
        addFunctionCase(limitsValidationTests.get(), "ext_conservative_rasterization",
                        checkSupportExtConservativeRasterization, validateLimitsExtConservativeRasterization);
        addFunctionCase(limitsValidationTests.get(), "ext_descriptor_indexing", checkSupportExtDescriptorIndexing,
                        validateLimitsExtDescriptorIndexing);
#ifndef CTS_USES_VULKANSC
        // Removed from Vulkan SC test set: VK_EXT_inline_uniform_block extension removed from Vulkan SC
        addFunctionCase(limitsValidationTests.get(), "ext_inline_uniform_block", checkSupportExtInlineUniformBlock,
                        validateLimitsExtInlineUniformBlock);
#endif // CTS_USES_VULKANSC
        addFunctionCase(limitsValidationTests.get(), "ext_vertex_attribute_divisor",
                        checkSupportExtVertexAttributeDivisorEXT, validateLimitsExtVertexAttributeDivisorEXT);
        addFunctionCase(limitsValidationTests.get(), "khr_vertex_attribute_divisor",
                        checkSupportExtVertexAttributeDivisorKHR, validateLimitsExtVertexAttributeDivisorKHR);
#ifndef CTS_USES_VULKANSC
        // Removed from Vulkan SC test set: extensions VK_NV_mesh_shader, VK_EXT_transform_feedback, VK_EXT_fragment_density_map, VK_NV_ray_tracing extension removed from Vulkan SC
        addFunctionCase(limitsValidationTests.get(), "nv_mesh_shader", checkSupportNvMeshShader,
                        validateLimitsNvMeshShader);
        addFunctionCase(limitsValidationTests.get(), "ext_transform_feedback", checkSupportExtTransformFeedback,
                        validateLimitsExtTransformFeedback);
        addFunctionCase(limitsValidationTests.get(), "fragment_density_map", checkSupportExtFragmentDensityMap,
                        validateLimitsExtFragmentDensityMap);
        addFunctionCase(limitsValidationTests.get(), "nv_ray_tracing", checkSupportNvRayTracing,
                        validateLimitsNvRayTracing);
#endif
        addFunctionCase(limitsValidationTests.get(), "timeline_semaphore", checkSupportKhrTimelineSemaphore,
                        validateLimitsKhrTimelineSemaphore);
        addFunctionCase(limitsValidationTests.get(), "ext_line_rasterization", checkSupportExtLineRasterization,
                        validateLimitsLineRasterization);
        addFunctionCase(limitsValidationTests.get(), "khr_line_rasterization", checkSupportKhrLineRasterization,
                        validateLimitsLineRasterization);
        addFunctionCase(limitsValidationTests.get(), "robustness2", checkSupportRobustness2, validateLimitsRobustness2);

        infoTests->addChild(limitsValidationTests.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> limitsValidationTests(
            new tcu::TestCaseGroup(testCtx, "vulkan1p3_limits_validation"));

#ifndef CTS_USES_VULKANSC
        addFunctionCase(limitsValidationTests.get(), "khr_maintenance4", checkSupportKhrMaintenance4,
                        validateLimitsKhrMaintenance4);
        addFunctionCase(limitsValidationTests.get(), "max_inline_uniform_total_size", checkApiVersionSupport<1, 3>,
                        validateLimitsMaxInlineUniformTotalSize);
#endif // CTS_USES_VULKANSC

        infoTests->addChild(limitsValidationTests.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> limitsValidationTests(
            new tcu::TestCaseGroup(testCtx, "vulkan1p4_limits_validation"));

#ifndef CTS_USES_VULKANSC
        addFunctionCase(limitsValidationTests.get(), "general", checkApiVersionSupport<1, 4>, validateLimits14);
#endif // CTS_USES_VULKANSC

        infoTests->addChild(limitsValidationTests.release());
    }

    infoTests->addChild(
        createTestGroup(testCtx, "image_format_properties2", createImageFormatTests, imageFormatProperties2));
#ifndef CTS_USES_VULKANSC
    infoTests->addChild(createTestGroup(testCtx, "sparse_image_format_properties2", createImageFormatTests,
                                        sparseImageFormatProperties2));

    {
        de::MovePtr<tcu::TestCaseGroup> profilesValidationTests(new tcu::TestCaseGroup(testCtx, "profiles"));

        for (const auto &[name, checkSupportFun, validateFun] : profileEntries)
            addFunctionCase(profilesValidationTests.get(), name, *checkSupportFun, *validateFun);

        infoTests->addChild(profilesValidationTests.release());
    }
#endif // CTS_USES_VULKANSC

    {
        de::MovePtr<tcu::TestCaseGroup> androidTests(new tcu::TestCaseGroup(testCtx, "android"));

        // Test that all mandatory extensions are supported
        addFunctionCase(androidTests.get(), "mandatory_extensions", android::checkSupportAndroid,
                        android::testMandatoryExtensions);
        // Test for unknown device or instance extensions
        addFunctionCase(androidTests.get(), "no_unknown_extensions", android::checkSupportAndroid,
                        android::testNoUnknownExtensions);
        // Test that no layers are enumerated
        addFunctionCase(androidTests.get(), "no_layers", android::checkSupportAndroid, android::testNoLayers);

        infoTests->addChild(androidTests.release());
    }

    return infoTests.release();
}

void createFeatureInfoInstanceTests(tcu::TestCaseGroup *testGroup)
{
    addFunctionCase(testGroup, "physical_devices", enumeratePhysicalDevices);
    addFunctionCase<CustomInstanceTest<E071>>(testGroup, "physical_device_groups", enumeratePhysicalDeviceGroups);
    addFunctionCase(testGroup, "instance_layers", enumerateInstanceLayers);
    addFunctionCase(testGroup, "instance_extensions", enumerateInstanceExtensions);
    addFunctionCase(testGroup, "instance_extension_device_functions",
                    validateDeviceLevelEntryPointsFromInstanceExtensions);
}

void createFeatureInfoDeviceTests(tcu::TestCaseGroup *testGroup)
{
    addFunctionCase(testGroup, "device_features", deviceFeatures);
    addFunctionCase(testGroup, "device_properties", deviceProperties);
    addFunctionCase(testGroup, "device_queue_family_properties", deviceQueueFamilyProperties);
    addFunctionCase(testGroup, "device_memory_properties", deviceMemoryProperties);
    addFunctionCase(testGroup, "device_layers", enumerateDeviceLayers);
    addFunctionCase(testGroup, "device_extensions", enumerateDeviceExtensions);
    addFunctionCase(testGroup, "device_no_khx_extensions", testNoKhxExtensions);
    addFunctionCase(testGroup, "device_memory_budget", deviceMemoryBudgetProperties);
    addFunctionCase(testGroup, "device_mandatory_features", deviceMandatoryFeatures);
}

void createFeatureInfoDeviceGroupTests(tcu::TestCaseGroup *testGroup)
{
    addFunctionCase<CustomInstanceTest<E071>>(testGroup, "device_group_peer_memory_features",
                                              deviceGroupPeerMemoryFeatures);
}

} // namespace api
} // namespace vkt
