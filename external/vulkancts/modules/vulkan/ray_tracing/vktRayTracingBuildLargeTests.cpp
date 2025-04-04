/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Ray Tracing Build Large Shader Set tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuildLargeTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deClock.h"

#include <limits>

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace std;

static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

struct CaseDef
{
    uint32_t width;
    uint32_t height;
    uint32_t squaresGroupCount;
    uint32_t geometriesGroupCount;
    uint32_t instancesGroupCount;
    bool deferredOperation;
    VkAccelerationStructureBuildTypeKHR buildType;
    uint32_t workerThreadsCount;
};

uint32_t getShaderGroupSize(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

    return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

uint32_t getShaderGroupBaseAlignment(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

    return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

Move<VkPipeline> makePipeline(const DeviceInterface &vkd, const VkDevice device, vk::BinaryCollection &collection,
                              de::MovePtr<RayTracingPipeline> &rayTracingPipeline, VkPipelineLayout pipelineLayout,
                              const uint32_t groupCount, const bool deferredOperation, const uint32_t threadCount)
{
    Move<VkShaderModule> raygenShader = createShaderModule(vkd, device, collection.get("rgen"), 0);

    rayTracingPipeline->setDeferredOperation(deferredOperation, threadCount);
    rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, 0);

    for (uint32_t groupNdx = 0; groupNdx < groupCount; ++groupNdx)
    {
        const std::string shaderName    = "call" + de::toString(groupNdx);
        Move<VkShaderModule> callShader = createShaderModule(vkd, device, collection.get(shaderName), 0);

        rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, callShader, 1 + groupNdx);
    }

    Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

    return pipeline;
}

VkImageCreateInfo makeImageCreateInfo(uint32_t width, uint32_t height, VkFormat format)
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(width, height, 1u),     // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

class RayTracingBuildLargeTestInstance : public TestInstance
{
public:
    RayTracingBuildLargeTestInstance(Context &context, const CaseDef &data);
    ~RayTracingBuildLargeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    uint32_t iterateNoWorkers(void);
    uint32_t iterateWithWorkers(void);
    void checkSupportInInstance(void) const;
    de::MovePtr<BufferWithMemory> runTest(const uint32_t threadCount);
    uint32_t validateBuffer(de::MovePtr<BufferWithMemory> buffer);
    de::SharedPtr<TopLevelAccelerationStructure> initTopAccelerationStructure(
        VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure);
    de::SharedPtr<BottomLevelAccelerationStructure> initBottomAccelerationStructure(VkCommandBuffer cmdBuffer);

private:
    CaseDef m_data;
};

RayTracingBuildLargeTestInstance::RayTracingBuildLargeTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

RayTracingBuildLargeTestInstance::~RayTracingBuildLargeTestInstance(void)
{
}

class RayTracingTestCase : public TestCase
{
public:
    RayTracingTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~RayTracingTestCase(void);

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    std::string generateDummyWork(const uint32_t shaderNdx) const;
    CaseDef m_data;
};

RayTracingTestCase::RayTracingTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
    DE_ASSERT((m_data.width * m_data.height) ==
              (m_data.squaresGroupCount * m_data.geometriesGroupCount * m_data.instancesGroupCount));
}

RayTracingTestCase::~RayTracingTestCase(void)
{
}

void RayTracingTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
    context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
        context.getRayTracingPipelineFeatures();
    if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
        TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
        context.getAccelerationStructureFeatures();
    if (accelerationStructureFeaturesKHR.accelerationStructure == false)
        TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                             "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

    if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR &&
        accelerationStructureFeaturesKHR.accelerationStructureHostCommands == false)
        TCU_THROW(NotSupportedError,
                  "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

    if (m_data.deferredOperation)
        context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
}

std::string RayTracingTestCase::generateDummyWork(const uint32_t shaderNdx) const
{
    std::string result;

    for (uint32_t n = 0; n < shaderNdx % 256; ++n)
    {
        result += "  color.b = color.b + 2 * " + de::toString(n) + ";\n";
        result += "  color.g = color.g + 3 * " + de::toString(n) + ";\n";
        result += "  color.b = color.b ^ color.g;\n";
        result += "  color.b = color.b % 223;\n";
        result += "  color.g = color.g % 227;\n";
        result += "  color.g = color.g ^ color.b;\n";
    }

    return result;
}

void RayTracingTestCase::initPrograms(SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_ray_tracing : require\n"
               "layout(location = 0) callableDataEXT float dummy;"
               "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
               "\n"
               "void main()\n"
               "{\n"
               "  uint n = "
            << m_data.width
            << " * gl_LaunchIDEXT.y + gl_LaunchIDEXT.x;\n"
               "  executeCallableEXT(n, 0);\n"
               "}\n";

        programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
    }

    for (uint32_t y = 0; y < m_data.height; ++y)
        for (uint32_t x = 0; x < m_data.width; ++x)
        {
            const uint32_t shaderNdx = m_data.width * y + x;
            const bool dummyWork     = (shaderNdx % 43 == 0);
            std::stringstream css;
            css << "#version 460 core\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) callableDataInEXT float dummy;\n"
                   "layout(r32ui, set = 0, binding = 0) uniform uimage2D image0_0;\n"
                   "void main()\n"
                   "{\n"
                   "  uint r = ("
                << m_data.width << " * " << y / 3 << " + " << x
                << ") % 199;\n"
                   "  uvec4 color = uvec4(r,0,0,1);\n"
                << (dummyWork ? generateDummyWork(shaderNdx) : "")
                << "  imageStore(image0_0, ivec2(gl_LaunchIDEXT.xy), color);\n"
                   "}\n";

            programCollection.glslSources.add("call" + de::toString(shaderNdx))
                << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
        }
}

TestInstance *RayTracingTestCase::createInstance(Context &context) const
{
    return new RayTracingBuildLargeTestInstance(context, m_data);
}

de::SharedPtr<TopLevelAccelerationStructure> RayTracingBuildLargeTestInstance::initTopAccelerationStructure(
    VkCommandBuffer cmdBuffer, de::SharedPtr<BottomLevelAccelerationStructure> &bottomLevelAccelerationStructure)
{
    const DeviceInterface &vkd                        = m_context.getDeviceInterface();
    const VkDevice device                             = m_context.getDevice();
    Allocator &allocator                              = m_context.getDefaultAllocator();
    de::MovePtr<TopLevelAccelerationStructure> result = makeTopLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setInstanceCount(1);
    result->setBuildType(m_data.buildType);
    result->setDeferredOperation(m_data.deferredOperation);
    result->addInstance(bottomLevelAccelerationStructure);

    result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

    return de::SharedPtr<TopLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildLargeTestInstance::initBottomAccelerationStructure(
    VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vkd                           = m_context.getDeviceInterface();
    const VkDevice device                                = m_context.getDevice();
    Allocator &allocator                                 = m_context.getDefaultAllocator();
    tcu::UVec2 startPos                                  = tcu::UVec2(0u, 0u);
    de::MovePtr<BottomLevelAccelerationStructure> result = makeBottomLevelAccelerationStructure();

    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;

    result->setBuildType(m_data.buildType);
    result->setDeferredOperation(m_data.deferredOperation);
    result->setGeometryCount(m_data.geometriesGroupCount);

    for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
    {
        std::vector<tcu::Vec3> geometryData;

        geometryData.reserve(m_data.squaresGroupCount * 3u);

        for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
        {
            const uint32_t n = m_data.width * startPos.y() + startPos.x();
            const uint32_t m = (13 * (n + 1)) % (m_data.width * m_data.height);
            const float x0   = float(startPos.x() + 0) / float(m_data.width);
            const float y0   = float(startPos.y() + 0) / float(m_data.height);
            const float x1   = float(startPos.x() + 1) / float(m_data.width);
            const float y1   = float(startPos.y() + 1) / float(m_data.height);
            const float xm   = (x0 + x1) / 2.0f;
            const float ym   = (y0 + y1) / 2.0f;

            geometryData.push_back(tcu::Vec3(x0, y0, -1.0f));
            geometryData.push_back(tcu::Vec3(xm, y1, -1.0f));
            geometryData.push_back(tcu::Vec3(x1, ym, -1.0f));

            startPos.y() = m / m_data.width;
            startPos.x() = m % m_data.width;
        }

        result->addGeometry(geometryData, true);
    }

    result->createAndBuild(vkd, device, cmdBuffer, allocator, bufferProps);

    return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::MovePtr<BufferWithMemory> RayTracingBuildLargeTestInstance::runTest(const uint32_t threadCount)
{
    const InstanceInterface &vki            = m_context.getInstanceInterface();
    const DeviceInterface &vkd              = m_context.getDeviceInterface();
    const VkDevice device                   = m_context.getDevice();
    const VkPhysicalDevice physicalDevice   = m_context.getPhysicalDevice();
    const uint32_t queueFamilyIndex         = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue                     = m_context.getUniversalQueue();
    Allocator &allocator                    = m_context.getDefaultAllocator();
    const VkFormat format                   = VK_FORMAT_R32_UINT;
    const uint32_t pixelCount               = m_data.width * m_data.height;
    const uint32_t callableShaderCount      = m_data.width * m_data.height;
    const uint32_t shaderGroupHandleSize    = getShaderGroupSize(vki, physicalDevice);
    const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
            .build(vkd, device);
    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());
    const Move<VkCommandPool> cmdPool           = createCommandPool(vkd, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
    const Move<VkPipeline> pipeline =
        makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline, *pipelineLayout,
                     callableShaderCount, m_data.deferredOperation, threadCount);
    const de::MovePtr<BufferWithMemory> raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1u);
    const de::MovePtr<BufferWithMemory> callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
        vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, callableShaderCount);
    const VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    const VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion  = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    const VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize * callableShaderCount);

    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(m_data.width, m_data.height, format);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
    const de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    const Move<VkImageView> imageView =
        makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);

    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(pixelCount * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkImageSubresourceLayers bufferImageSubresourceLayers =
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const VkBufferImageCopy bufferImageRegion =
        makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1u), bufferImageSubresourceLayers);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    const VkDescriptorImageInfo descriptorImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);

    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
    const VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);
    const VkMemoryBarrier postTraceMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkMemoryBarrier postCopyMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const VkClearValue clearValue = makeClearValueColorU32(5u, 5u, 5u, 255u);

    de::SharedPtr<BottomLevelAccelerationStructure> bottomLevelAccelerationStructure;
    de::SharedPtr<TopLevelAccelerationStructure> topLevelAccelerationStructure;

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
        vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1,
                               &imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &postImageBarrier);

        bottomLevelAccelerationStructure = initBottomAccelerationStructure(*cmdBuffer);
        topLevelAccelerationStructure    = initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructure);

        const TopLevelAccelerationStructure *topLevelAccelerationStructurePtr = topLevelAccelerationStructure.get();
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            nullptr,                                                           //  const void* pNext;
            1u,                                                                //  uint32_t accelerationStructureCount;
            topLevelAccelerationStructurePtr->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
            .update(vkd, device);

        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1,
                                  &descriptorSet.get(), 0, nullptr);

        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

        cmdTraceRays(vkd, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                     &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, m_data.width, m_data.height, 1);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

        vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopyMemoryBarrier);
    }
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

    invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(),
                                pixelCount * sizeof(uint32_t));

    return buffer;
}

void RayTracingBuildLargeTestInstance::checkSupportInInstance(void) const
{
    const InstanceInterface &vki                     = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice            = m_context.getPhysicalDevice();
    const vk::VkPhysicalDeviceProperties &properties = m_context.getDeviceProperties();
    const uint32_t requiredAllocations =
        8u + TopLevelAccelerationStructure::getRequiredAllocationCount() +
        m_data.instancesGroupCount * BottomLevelAccelerationStructure::getRequiredAllocationCount();
    de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);

    if (rayTracingProperties->getMaxPrimitiveCount() < m_data.squaresGroupCount)
        TCU_THROW(NotSupportedError, "Triangles required more than supported");

    if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
        TCU_THROW(NotSupportedError, "Geometries required more than supported");

    if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
        TCU_THROW(NotSupportedError, "Instances required more than supported");

    if (properties.limits.maxMemoryAllocationCount < requiredAllocations)
        TCU_THROW(NotSupportedError, "Test requires more allocations allowed");
}

uint32_t RayTracingBuildLargeTestInstance::validateBuffer(de::MovePtr<BufferWithMemory> buffer)
{
    const uint32_t *bufferPtr = (uint32_t *)buffer->getAllocation().getHostPtr();
    uint32_t failures         = 0;
    uint32_t pos              = 0;

    for (uint32_t y = 0; y < m_data.height; ++y)
        for (uint32_t x = 0; x < m_data.width; ++x)
        {
            const uint32_t expectedValue = (m_data.width * (y / 3) + x) % 199;

            if (bufferPtr[pos] != expectedValue)
                failures++;

            ++pos;
        }

    return failures;
}

uint32_t RayTracingBuildLargeTestInstance::iterateNoWorkers(void)
{
    de::MovePtr<BufferWithMemory> buffer = runTest(0);
    const uint32_t failures              = validateBuffer(buffer);

    return failures;
}

uint32_t RayTracingBuildLargeTestInstance::iterateWithWorkers(void)
{
    de::MovePtr<BufferWithMemory> singleThreadBuffer = runTest(0);
    const uint32_t singleThreadFailures              = validateBuffer(singleThreadBuffer);
    de::MovePtr<BufferWithMemory> multiThreadBuffer  = runTest(m_data.workerThreadsCount);
    const uint32_t multiThreadFailures               = validateBuffer(multiThreadBuffer);
    const uint32_t failures                          = singleThreadFailures + multiThreadFailures;

    return failures;
}

tcu::TestStatus RayTracingBuildLargeTestInstance::iterate(void)
{
    checkSupportInInstance();

    const uint32_t failures = m_data.workerThreadsCount == 0 ? iterateNoWorkers() : iterateWithWorkers();

    if (failures == 0)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

} // namespace

tcu::TestCaseGroup *createBuildLargeShaderSetTests(tcu::TestContext &testCtx)
{
    // Build large shader set using CPU host threading
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "large_shader_set"));

    const uint32_t sizes[] = {8, 16, 32, 64};
    const struct
    {
        const char *buildTypeName;
        bool deferredOperation;
        const VkAccelerationStructureBuildTypeKHR buildType;
    } buildTypes[] = {
        {"gpu", false, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR},
        {"cpu_ht", true, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR},
    };
    const uint32_t threads[] = {1, 2, 3, 4, 8, std::numeric_limits<uint32_t>::max()};

    for (size_t buildNdx = 0; buildNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(
            new tcu::TestCaseGroup(testCtx, buildTypes[buildNdx].buildTypeName));

        for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
        {
            const uint32_t largestGroup         = sizes[sizesNdx] * sizes[sizesNdx];
            const uint32_t squaresGroupCount    = largestGroup;
            const uint32_t geometriesGroupCount = 1;
            const uint32_t instancesGroupCount  = 1;
            const CaseDef caseDef               = {
                sizes[sizesNdx],                        //  uint32_t width;
                sizes[sizesNdx],                        //  uint32_t height;
                squaresGroupCount,                      //  uint32_t squaresGroupCount;
                geometriesGroupCount,                   //  uint32_t geometriesGroupCount;
                instancesGroupCount,                    //  uint32_t instancesGroupCount;
                buildTypes[buildNdx].deferredOperation, //  bool deferredOperation;
                buildTypes[buildNdx].buildType,         //  VkAccelerationStructureBuildTypeKHR buildType;
                0,                                      //  uint32_t threadsCount;
            };
            const std::string testName = de::toString(largestGroup);

            buildTypeGroup->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
        }

        group->addChild(buildTypeGroup.release());
    }

    for (size_t threadsNdx = 0; threadsNdx < DE_LENGTH_OF_ARRAY(threads); ++threadsNdx)
    {
        for (size_t buildNdx = 0; buildNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildNdx)
        {
            if (buildTypes[buildNdx].buildType != VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR)
                continue;

            const std::string suffix =
                threads[threadsNdx] == std::numeric_limits<uint32_t>::max() ? "max" : de::toString(threads[threadsNdx]);
            const std::string buildTypeGroupName = std::string(buildTypes[buildNdx].buildTypeName) + '_' + suffix;
            de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(testCtx, buildTypeGroupName.c_str()));

            for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
            {
                const uint32_t largestGroup         = sizes[sizesNdx] * sizes[sizesNdx];
                const uint32_t squaresGroupCount    = largestGroup;
                const uint32_t geometriesGroupCount = 1;
                const uint32_t instancesGroupCount  = 1;
                const CaseDef caseDef               = {
                    sizes[sizesNdx],                        //  uint32_t width;
                    sizes[sizesNdx],                        //  uint32_t height;
                    squaresGroupCount,                      //  uint32_t squaresGroupCount;
                    geometriesGroupCount,                   //  uint32_t geometriesGroupCount;
                    instancesGroupCount,                    //  uint32_t instancesGroupCount;
                    buildTypes[buildNdx].deferredOperation, //  bool deferredOperation;
                    buildTypes[buildNdx].buildType, //  VkAccelerationStructureBuildTypeKHR buildType;
                    threads[threadsNdx],            //  uint32_t workerThreadsCount;
                };
                const std::string testName = de::toString(largestGroup);

                buildTypeGroup->addChild(new RayTracingTestCase(testCtx, testName.c_str(), caseDef));
            }

            group->addChild(buildTypeGroup.release());
        }
    }

    return group.release();
}

} // namespace RayTracing
} // namespace vkt
