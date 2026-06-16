#ifndef _VKTDATAGRAPHTESTUTIL_HPP
#define _VKTDATAGRAPHTESTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 Arm Ltd.
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
 */
/*!
 * \file
 * \brief DataGraph test utilities
 */
/*--------------------------------------------------------------------*/

#include "../tensor/vktTensorTestsUtil.hpp"

#include "vkTensorMemoryUtil.hpp"
#include "vkTensorUtil.hpp"
#include "vkDataGraphUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"

#include <bitset>
#include <cmath>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <type_traits>

namespace vkt
{
namespace dataGraph
{

using namespace vk;

#define VK_CHECK_INCOMPLETE(EXPR) checkExpectedResult<VK_INCOMPLETE>((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_COMPILE_REQUIRED(EXPR) \
    checkExpectedResult<VK_PIPELINE_COMPILE_REQUIRED_EXT>((EXPR), #EXPR, __FILE__, __LINE__)

/**
 * @brief Checks that a Vulkan operation's result matches the expected value.
 *
 * @tparam ExpectedResult The expected VkResult value to compare against.
 * @param result The VkResult returned from a Vulkan operation.
 * @param msg Optional custom error message to prepend to the error details.
 * @param file The source file name where this check is performed.
 * @param line The line number in the source file corresponding to this check.
 *
 * @throws Error if result does not equal ExpectedResult.
 */
template <VkResult ExpectedResult>
void checkExpectedResult(VkResult result, const char *msg, const char *file, int line)
{
    if (result != ExpectedResult)
    {
        std::ostringstream msgStr;
        if (msg)
            msgStr << msg << ": ";

        msgStr << getResultStr(result);

        throw Error(result, msgStr.str().c_str(), nullptr, file, line);
    }
}

/**
 * @brief Checks if the provided Vulkan handle is a null handle.
 *
 * @tparam T The type of the object (typically a Vulkan handle) to check.
 * @param object The Vulkan handle to validate; expected to be VK_NULL_HANDLE.
 *
 * @throws TestError Thrown if object is not equal to VK_NULL_HANDLE.
 */
template <typename T>
void checkIsNull(T object)
{
    if (object != VK_NULL_HANDLE)
    {
        throw tcu::TestError("Object check() failed", (std::string(getTypeName<T>()) + " != VK_NULL_HANDLE").c_str(),
                             __FILE__, __LINE__);
    }
}

enum ResourceType
{
    RESOURCE_TYPE_INPUT = 0,
    RESOURCE_TYPE_OUTPUT,
    RESOURCE_TYPE_CONSTANT,
    RESOURCE_TYPE_COUNT
};

enum ResourceCardinality
{
    NONE,
    ONE,
    MANY,
};

struct ResourcesCardinalities
{
    ResourceCardinality inputs;
    ResourceCardinality outputs;
    ResourceCardinality constants;
};

enum StrideModes
{
    TENSOR_STRIDES_IMPLICIT,
    TENSOR_STRIDES_PACKED,
    TENSOR_STRIDES_NOT_PACKED,
    TENSOR_STRIDES_IMAGE_ALIASING, // special strides that depend on image requirements
};

struct ResourcesStrideModes
{
    StrideModes inputs;
    StrideModes outputs;
    StrideModes constants;
};

// skip combinations where output cardinalities is none
const std::vector<ResourcesCardinalities> allResourceCardinalityCombinations = {
    {NONE, ONE, NONE}, {NONE, ONE, ONE}, {NONE, ONE, MANY}, {NONE, MANY, NONE}, {NONE, MANY, ONE}, {NONE, MANY, MANY},
    {ONE, ONE, NONE},  {ONE, ONE, ONE},  {ONE, ONE, MANY},  {ONE, MANY, NONE},  {ONE, MANY, ONE},  {ONE, MANY, MANY},
    {MANY, ONE, NONE}, {MANY, ONE, ONE}, {MANY, ONE, MANY}, {MANY, MANY, NONE}, {MANY, MANY, ONE}, {MANY, MANY, MANY},
};

const std::vector<ResourcesStrideModes> allStrideModesCombinations = {
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
};

std::string getImageAliasingFillShaderName(VkFormat format);
std::string getImageAliasingVerifyShaderName(VkFormat format);

struct ImageAliasingPushConstants
{
    uint32_t dim0;
    uint32_t dim1;
    uint32_t dim2;
    uint32_t dim3;
    uint32_t elementCount;
};

template <typename T>
using BufferType = std::conditional_t<std::is_same_v<T, int8_t> || std::is_same_v<T, int32_t>, int32_t, float>;

template <typename T>
void fillImageAliasingValueBuffer(const DeviceInterface &vk, const VkDevice device, const BufferWithMemory &buffer,
                                  const tensor::StridedMemoryUtils<T> &hostData)
{
    BufferType<T> *const values = static_cast<BufferType<T> *>(buffer.getAllocation().getHostPtr());
    for (size_t elementIdx = 0; elementIdx < hostData.elementCount(); ++elementIdx)
    {
        values[elementIdx] = static_cast<BufferType<T>>(hostData[elementIdx]);
    }
    flushAlloc(vk, device, buffer.getAllocation());
}

inline ImageAliasingPushConstants getImageAliasingPushConstants(const tensor::TensorParameters &tensorParams)
{
    DE_ASSERT(tensorParams.dimensions.size() == 4u);

    const size_t elementCount =
        static_cast<size_t>(std::accumulate(tensorParams.dimensions.cbegin(), tensorParams.dimensions.cend(),
                                            static_cast<int64_t>(1u), std::multiplies<int64_t>()));

    return {static_cast<uint32_t>(tensorParams.dimensions[0]), static_cast<uint32_t>(tensorParams.dimensions[1]),
            static_cast<uint32_t>(tensorParams.dimensions[2]), static_cast<uint32_t>(tensorParams.dimensions[3]),
            static_cast<uint32_t>(elementCount)};
}

inline Move<VkPipelineLayout> makeImageAliasingPipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                                              VkDescriptorSetLayout setLayout)
{
    const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                                                   static_cast<uint32_t>(sizeof(ImageAliasingPushConstants))};
    return makePipelineLayout(vk, device, setLayout, &pushConstantRange);
}

inline void prepareAliasedImageForGeneralLayout(const Context &context, const TensorWithMemory &tensor)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    const VkQueue queue       = context.getUniversalQueue();
    const uint32_t qfi        = context.getUniversalQueueFamilyIndex();

    const VkImage image = tensor.getAliasedImage();
    DE_ASSERT(image != VK_NULL_HANDLE);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, qfi));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    const VkImageSubresourceRange fullRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier toGeneral =
        makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, image, fullRange);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &toGeneral);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

template <typename T>
void uploadToImageAliasedTensor(const Context &context, const DeviceInterface &vk, const VkDevice device,
                                vk::Allocator &allocator, const VkQueue queue, const uint32_t queueFamilyIndex,
                                const TensorWithMemory &tensor, const tensor::StridedMemoryUtils<T> &hostData,
                                const tensor::TensorParameters &tensorParams)
{
    const size_t elementCount          = hostData.elementCount();
    const VkDeviceSize valueBufferSize = static_cast<VkDeviceSize>(elementCount * sizeof(BufferType<T>));

    const VkBufferCreateInfo valueBufferCreateInfo =
        makeBufferCreateInfo(valueBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const vk::BufferWithMemory valueBuffer(vk, device, allocator, valueBufferCreateInfo,
                                           vk::MemoryRequirement::HostVisible);

    fillImageAliasingValueBuffer(vk, device, valueBuffer, hostData);

    const VkImage image = tensor.getAliasedImage();
    DE_ASSERT(image != VK_NULL_HANDLE);
    const Unique<VkImageView> imageView(
        makeImageView(vk, device, image, VK_IMAGE_VIEW_TYPE_2D, tensorParams.format,
                      makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));

    const Unique<VkDescriptorSetLayout> setLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));
    const Unique<VkDescriptorPool> pool(DescriptorPoolBuilder()
                                            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSet> set(makeDescriptorSet(vk, device, *pool, *setLayout));

    const VkDescriptorBufferInfo valueInfo =
        makeDescriptorBufferInfo(valueBuffer.get(), 0u, valueBuffer.getBufferSize());
    const VkDescriptorImageInfo imgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*set, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &valueInfo)
        .writeSingle(*set, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &imgInfo)
        .update(vk, device);

    const Unique<VkShaderModule> module(createShaderModule(
        vk, device, context.getBinaryCollection().get(getImageAliasingFillShaderName(tensorParams.format)), 0u));
    const Unique<VkPipelineLayout> layout(makeImageAliasingPipelineLayout(vk, device, *setLayout));
    const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *layout, *module));

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    const VkImageSubresourceRange fullRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier toGeneral    = makeImageMemoryBarrier(
        0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, image, fullRange);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &toGeneral);

    const ImageAliasingPushConstants pushConstants = getImageAliasingPushConstants(tensorParams);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *layout, 0u, 1u, &set.get(), 0u, nullptr);
    vk.cmdPushConstants(*cmdBuffer, *layout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(pushConstants), &pushConstants);
    vk.cmdDispatch(*cmdBuffer, (pushConstants.elementCount + 63u) / 64u, 1u, 1u);

    const VkMemoryBarrier shaderToReadBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 1u,
                          &shaderToReadBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

template <typename T>
tcu::TestStatus verifyImageAliasedTensorWithShader(const Context &context, const TensorWithMemory &tensor,
                                                   const tensor::StridedMemoryUtils<T> &hostData,
                                                   const tensor::TensorParameters &tensorParams)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const size_t elementCount       = hostData.elementCount();
    const VkDeviceSize valuesSize   = static_cast<VkDeviceSize>(elementCount * sizeof(BufferType<T>));

    const VkBufferCreateInfo valueBufferCreateInfo =
        makeBufferCreateInfo(valuesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vk::BufferWithMemory valueBuffer(vk, device, allocator, valueBufferCreateInfo, vk::MemoryRequirement::HostVisible);
    const std::vector<uint32_t> mismatchCount(1u, 0u);
    const std::unique_ptr<vk::BufferWithMemory> mismatchBuffer =
        makeBufferWithMemory(vk, device, allocator, mismatchCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    fillImageAliasingValueBuffer(vk, device, valueBuffer, hostData);

    prepareAliasedImageForGeneralLayout(context, tensor);

    const VkImage image = tensor.getAliasedImage();
    DE_ASSERT(image != VK_NULL_HANDLE);
    const Unique<VkImageView> imageView(
        makeImageView(vk, device, image, VK_IMAGE_VIEW_TYPE_2D, tensorParams.format,
                      makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));

    const Unique<VkDescriptorSetLayout> setLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));
    const Unique<VkDescriptorPool> pool(DescriptorPoolBuilder()
                                            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
                                            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSet> set(makeDescriptorSet(vk, device, *pool, *setLayout));

    const VkDescriptorBufferInfo valueInfo =
        makeDescriptorBufferInfo(valueBuffer.get(), 0u, valueBuffer.getBufferSize());
    const VkDescriptorBufferInfo mismatchInfo =
        makeDescriptorBufferInfo(mismatchBuffer->get(), 0u, mismatchBuffer->getBufferSize());
    const VkDescriptorImageInfo imgInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*set, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &valueInfo)
        .writeSingle(*set, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &mismatchInfo)
        .writeSingle(*set, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &imgInfo)
        .update(vk, device);

    const Unique<VkShaderModule> module(createShaderModule(
        vk, device, context.getBinaryCollection().get(getImageAliasingVerifyShaderName(tensorParams.format)), 0u));
    const Unique<VkPipelineLayout> layout(makeImageAliasingPipelineLayout(vk, device, *setLayout));
    const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *layout, *module));

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    const VkImageSubresourceRange fullRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageMemoryBarrier imageToVerifyBarrier =
        makeImageMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_GENERAL, image, fullRange);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &imageToVerifyBarrier);

    const ImageAliasingPushConstants pushConstants = getImageAliasingPushConstants(tensorParams);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *layout, 0u, 1u, &set.get(), 0u, nullptr);
    vk.cmdPushConstants(*cmdBuffer, *layout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(pushConstants), &pushConstants);
    vk.cmdDispatch(*cmdBuffer, (pushConstants.elementCount + 63u) / 64u, 1u, 1u);

    const VkMemoryBarrier verifyToHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                          &verifyToHostBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    const Allocation &mismatchAlloc = mismatchBuffer->getAllocation();
    invalidateAlloc(vk, device, mismatchAlloc);
    const uint32_t mismatches = *static_cast<const uint32_t *>(mismatchAlloc.getHostPtr());

    if (mismatches != 0u)
        return tcu::TestStatus::fail("Verification failed (mismatches=" + de::toString(mismatches) + ")");

    return tcu::TestStatus::pass("verified OK");
}

enum class SparsityVariation
{
    NONE,
    VARIATION_1, /* Different sparsity across dimensions. */
    VARIATION_2, /* Same sparsity across dimensions. */
    VARIATION_3  /* 2,4 sparsity for the innermost dimension, non-sparse elsewhere. */
};

const std::vector<SparsityVariation> defaultSparsityVariations = {SparsityVariation::NONE,
                                                                  SparsityVariation::VARIATION_1};

const std::vector<SparsityVariation> allSparsityVariations = {SparsityVariation::NONE, SparsityVariation::VARIATION_1,
                                                              SparsityVariation::VARIATION_2,
                                                              SparsityVariation::VARIATION_3};

struct DataGraphTestResource
{
    tensor::TensorDimensions dimensions;
    tensor::TensorStrides strides;
    VkTensorDescriptionARM desc;
    de::MovePtr<TensorWithMemory> tensor;
    Move<VkTensorViewARM> view;
    Move<VkImage> image;
    VkWriteDescriptorSetTensorARM writeDesc;
};

enum class SpecConstantTest
{
    NONE,
    BASIC,
    BOOL,
    COMPOSITE,
    COMPOSITE_REPLICATED,
    OP,
};

struct TestParams
{
public:
    // test options
    std::string instructionSet{};
    bool sessionMemory;
    ResourcesCardinalities cardinalities{};
    ResourcesStrideModes strides{};
    bool shuffleBindings{false};
    VkTensorTilingARM tiling{VK_TENSOR_TILING_LINEAR_ARM};
    SparsityVariation sparsity{SparsityVariation::NONE};

    // optional test options
    bool imageAliasing{false};

    SpecConstantTest specConstants{SpecConstantTest::NONE};

    std::string formats{};

    bool packedInputs() const
    {
        return strides.inputs != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedConstants() const
    {
        return strides.constants != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedOutputs() const
    {
        return strides.outputs != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedResources() const
    {
        return (strides.inputs == TENSOR_STRIDES_IMPLICIT || strides.inputs == TENSOR_STRIDES_PACKED) &&
               (strides.outputs == TENSOR_STRIDES_IMPLICIT || strides.outputs == TENSOR_STRIDES_PACKED) &&
               (strides.constants == TENSOR_STRIDES_IMPLICIT || strides.constants == TENSOR_STRIDES_PACKED);
    }

    bool explictStrides() const
    {
        return strides.inputs != TENSOR_STRIDES_IMPLICIT || strides.outputs != TENSOR_STRIDES_IMPLICIT ||
               strides.constants != TENSOR_STRIDES_IMPLICIT;
    }

    bool valid() const;

    static void checkSupport(Context &ctx, TestParams params)
    {
        ctx.requireDeviceFunctionality("VK_ARM_data_graph");
        ctx.requireDeviceFunctionality("VK_ARM_tensors");

        const auto &vki           = ctx.getInstanceInterface();
        const auto physicalDevice = ctx.getPhysicalDevice();

        VkPhysicalDeviceDataGraphFeaturesARM dataGraphFeaturesProp = initVulkanStructure();
        VkPhysicalDeviceTensorFeaturesARM tensorFeaturesProp       = initVulkanStructure(&dataGraphFeaturesProp);
        VkPhysicalDeviceFeatures2 featuresProp                     = initVulkanStructure(&tensorFeaturesProp);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

        if (!dataGraphFeaturesProp.dataGraph)
        {
            TCU_THROW(NotSupportedError, "dataGraph feature not present");
        }

        if (!dataGraphFeaturesProp.dataGraphShaderModule)
        {
            TCU_THROW(NotSupportedError, "dataGraphShaderModule feature not present");
        }

        if (!tensorFeaturesProp.tensors)
        {
            TCU_THROW(NotSupportedError, "tensors feature not present");
        }

        if (!tensorFeaturesProp.shaderTensorAccess)
        {
            TCU_THROW(NotSupportedError, "shaderTensorAccess feature not present");
        }

        if (!params.packedResources() && !tensorFeaturesProp.tensorNonPacked)
        {
            TCU_THROW(NotSupportedError, "tensorNonPacked feature not present");
        }
    }
};

struct ResourceInformation
{
public:
    ResourceType type;
    tensor::TensorParameters params;
    uint32_t binding;
    uint32_t descriptorSet;
    uint32_t id;
    void *hostData;
    std::vector<DataGraphConstantSparsityHint> sparsityInfo;
    std::string label;

    bool isTensor() const
    {
        return (type == RESOURCE_TYPE_INPUT || type == RESOURCE_TYPE_OUTPUT);
    }

    bool isInput() const
    {
        return (type == RESOURCE_TYPE_INPUT);
    }

    bool isOutput() const
    {
        return (type == RESOURCE_TYPE_OUTPUT);
    }

    bool isConstant() const
    {
        return (type == RESOURCE_TYPE_CONSTANT);
    }

    bool requiresVerify() const
    {
        return (type == RESOURCE_TYPE_OUTPUT);
    }
};

std::ostream &operator<<(std::ostream &os, TestParams params);
std::ostream &operator<<(std::ostream &os, ResourceType type);
std::ostream &operator<<(std::ostream &os, ResourceInformation resInfo);

struct InitDataOptions
{
    uint8_t startingValue;
    std::vector<DataGraphConstantSparsityHint> sparsityInfo;
};

class DataGraphTest
{
public:
    struct SpecConstant
    {
        SpecConstant() = default;

        /* Helper function to create a SpecConstant from a type and a value.
         * T is intentionally made non-deducible, to force the user to be
         * explicit about the type of the value. fromValue must be called in
         * this way: fromValue<type>(...).
         */
        template <typename T>
        struct identity
        {
            typedef T type;
        };
        template <typename T>
        static SpecConstant fromValue(const uint32_t specId, const typename identity<T>::type specValue)
        {
            return SpecConstant{specId, specValue};
        }

        uint32_t id = 0;
        std::vector<char> data;

    private:
        template <typename T>
        SpecConstant(const uint32_t specId, const T specValue) : id(specId)
        {
            data.resize(sizeof(specValue));
            std::memcpy(data.data(), &specValue, sizeof(specValue));
        }
    };

    DataGraphTest(Context &context, size_t numResources) : m_context{context}
    {
        m_resInfo.resize(numResources);
    }

    DataGraphTest(Context &context, std::vector<ResourceInformation> resInfo) : m_context{context}, m_resInfo{resInfo}
    {
    }

    virtual ~DataGraphTest(){};

    virtual void initData(size_t id, TensorWithMemory *tensor = nullptr, InitDataOptions options = {0, {{}}}) = 0;
    virtual tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor)                                   = 0;

    /* data graph, in form of a pointer to pNext structure to append at pipeline creation */
    virtual std::vector<uint32_t> spirvBinary() = 0;

    Move<VkShaderModule> shaderModule()
    {
        const DeviceInterface &vk = m_context.getDeviceInterface();
        const VkDevice device     = m_context.getDevice();

        const auto binary = spirvBinary();
        de::MovePtr<ProgramBinary> programBinary =
            de::MovePtr<ProgramBinary>(new ProgramBinary(vk::PROGRAM_FORMAT_SPIRV, sizeof(uint32_t) * binary.size(),
                                                         reinterpret_cast<const uint8_t *>(binary.data())));
        return createShaderModule(vk, device, *programBinary);
    }

    size_t numTensors() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isTensor(); });
    }

    size_t numInputs() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isInput(); });
    }

    size_t numOutputs() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isOutput(); });
    }

    size_t numConstants() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isConstant(); });
    }

    size_t numResources()
    {
        return m_resInfo.size();
    }

    ResourceInformation resourceInfo(size_t id)
    {
        return m_resInfo.at(id);
    }

    const std::vector<ResourceInformation> &resourceInfos() const
    {
        return m_resInfo;
    }

    std::vector<ResourceInformation> &resourceInfos()
    {
        return m_resInfo;
    }

    const std::vector<SpecConstant> &specializationConstants() const
    {
        return m_specializationConstants;
    }

    /* Compile-time lookup of VkFormat properties */
    template <VkFormat T>
    struct vkFormatInfo;

protected:
    template <typename T>
    tcu::TestStatus verifyTensor(const tensor::StridedMemoryUtils<T> &outData,
                                 const tensor::StridedMemoryUtils<T> &refData)
    {
        if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, vk::Float16>)
        {
            // for floating point types use `SNR` rather than direct comparison
            float signalPower = 0.0;
            float noisePower  = 0.0;

            for (size_t i = 0; i < outData.elementCount(); i++)
            {
                float noise = outData[i] - refData[i];
                signalPower += std::pow(refData[i], 2.0f);
                noisePower += std::pow(noise, 2.0f);
            }

            if (noisePower > 0.0)
            {
                /* we do not divide signalPower and noisePower by N as we are only interested in the ratio */
                const float snr    = 10.0f * std::log10(signalPower / noisePower);
                const float minSnr = 140.0f;

                if (snr < minSnr)
                {
                    std::ostringstream msg;
                    msg << "Elements in tensor has too low SNR (min=" << minSnr << " dB, actual=" << snr << " dB)\n";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
        else
        {
            for (size_t i = 0; i < outData.elementCount(); i++)
            {
                if (outData[i] != refData[i])
                {
                    std::ostringstream msg;
                    msg << "Comparison failed at index " << i << ": tensor = " << outData[i]
                        << ", reference = " << refData[i];
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
        return tcu::TestStatus::pass("");
    }

    const Context &m_context;
    std::vector<ResourceInformation> m_resInfo;

    size_t m_numTensors{0};
    size_t m_numConstants{0};

    std::vector<SpecConstant> m_specializationConstants;
};

std::vector<TestParams> getTestParamsVariations(
    const std::vector<std::string> instructionSets = {"TOSA"}, const std::vector<bool> sessionMemories = {false, true},
    const std::vector<ResourcesCardinalities> resourcesCardinalities = {allResourceCardinalityCombinations},
    const std::vector<ResourcesStrideModes> strideModes              = {allStrideModesCombinations},
    const std::vector<bool> shuffledBindings                         = {false, true},
    const std::vector<VkTensorTilingARM> tilings = {VK_TENSOR_TILING_LINEAR_ARM, VK_TENSOR_TILING_OPTIMAL_ARM},
    const std::vector<SparsityVariation> sparsityVariations = defaultSparsityVariations,
    const bool imageAliasing = false, const std::vector<SpecConstantTest> specConstants = {SpecConstantTest::NONE});

/**
 * @brief Returns host type corresponding to the VkFormat
 */
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_BOOL_ARM>
{
    using hostType = uint8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_UINT>
{
    using hostType = uint8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_SINT>
{
    using hostType = int8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_UINT>
{
    using hostType = uint16_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_SINT>
{
    using hostType = int16_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_SFLOAT>
{
    using hostType = vk::Float16;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_UINT>
{
    using hostType = uint32_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_SINT>
{
    using hostType = int32_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_SFLOAT>
{
    using hostType = float;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R64_UINT>
{
    using hostType = uint64_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R64_SINT>
{
    using hostType = int64_t;
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTESTUTIL_HPP
