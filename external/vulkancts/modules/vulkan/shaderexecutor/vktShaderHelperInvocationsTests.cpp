/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Helper invocations tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderHelperInvocationsTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include <map>
#include <array>
#include <vector>
#include <string>
#include <string_view>

namespace vkt::shaderexecutor
{

namespace
{

using namespace vk;

enum class TestType
{
    LOAD_SSBO = 0,
    LOAD_ADDRESS,
    LOAD_UBO,
    LOAD_IMAGE,
    LOAD_TEXTURE,
    OUTPUT_VARIABLES,
};

struct TestParam
{
    TestType testType;
};

class HelperInvocationsTestInstance : public TestInstance
{
public:
    HelperInvocationsTestInstance(Context &context, const TestParam &testParam);
    ~HelperInvocationsTestInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    Move<VkRenderPass> setupRenderPass(const DeviceInterface &vk, VkDevice device, VkFormat colorFormat) const;

private:
    const TestParam m_testParam;
    bool m_usingBuffer;
    bool m_usingSampler;
    bool m_usingDescriptorSet;
    bool m_usingDeviceAddress;
    bool m_usingSecondSubpass;
    VkImageUsageFlags m_testedImageUsage;
    VkBufferUsageFlags m_testedBufferUsage;
    VkDescriptorType m_testedDescriptorType;
    std::string m_fragInputShaderName;
    uint32_t m_expectedColor;
};

HelperInvocationsTestInstance::HelperInvocationsTestInstance(Context &context, const TestParam &testParam)
    : TestInstance(context)
    , m_testParam(testParam)
{
    // configuration of code executed in iterate method is done in constructor

    // set default values that are also true for TestType::LOAD_SSBO
    m_usingBuffer          = true;
    m_usingSampler         = false;
    m_usingDescriptorSet   = true;
    m_usingDeviceAddress   = false;
    m_usingSecondSubpass   = false;
    m_testedImageUsage     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    m_testedBufferUsage    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    m_testedDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_expectedColor        = 63;

    if (m_testParam.testType == TestType::LOAD_ADDRESS)
    {
        m_usingDescriptorSet = false;
        m_usingDeviceAddress = true;
        m_testedBufferUsage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    else if (m_testParam.testType == TestType::LOAD_UBO)
    {
        m_testedBufferUsage    = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        m_testedDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    else if (m_testParam.testType == TestType::LOAD_IMAGE)
    {
        m_usingBuffer          = false;
        m_testedImageUsage     = VK_IMAGE_USAGE_STORAGE_BIT;
        m_testedDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    else if (m_testParam.testType == TestType::LOAD_TEXTURE)
    {
        m_usingSampler         = true;
        m_usingBuffer          = false;
        m_testedImageUsage     = VK_IMAGE_USAGE_SAMPLED_BIT;
        m_testedDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else if (m_testParam.testType == TestType::OUTPUT_VARIABLES)
    {
        m_usingBuffer          = false;
        m_usingSecondSubpass   = true;
        m_testedImageUsage     = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        m_testedDescriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    }
}

tcu::TestStatus HelperInvocationsTestInstance::iterate(void)
{
    // For LOAD_* cases we draw the same triangle twice. Result of the first draw is
    // copied to buffer (when testing ssbo) and passed as input to the second draw.
    // With the first draw we just want to know which fragments are under the triangle.
    // In the second draw fwidth is called on the value read from the input buffer
    // (result of first draw). Test expects to get one of four allowed values - clear
    // color around the triangle, zero inside triangle and one of two values on triangle
    // edges that depend on clear and render values used in the first draw. By verifying
    // all four values we will know if values that were grabbed(from input buffer) in
    // helper invocations were read correctly.

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkDevice device           = m_context.getDevice();
    Allocator &alloc                = m_context.getDefaultAllocator();

    const tcu::UVec2 renderSize(32);
    const VkFormat colorFormat{VK_FORMAT_R32_UINT};
    const std::vector<VkViewport> viewports{makeViewport(renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderSize)};
    const VkPushConstantRange pushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 2 * sizeof(uint32_t)};
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const tcu::UVec4 inputClearColor(21);
    const tcu::UVec4 finalClearColor(30);

    // vertex for triangle are generated in vs
    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkImageCreateInfo imageCreateInfo      = initVulkanStructure();
    imageCreateInfo.imageType              = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                 = colorFormat;
    imageCreateInfo.extent                 = {renderSize.x(), renderSize.y(), 1u};
    imageCreateInfo.mipLevels              = 1;
    imageCreateInfo.arrayLayers            = 1;
    imageCreateInfo.samples                = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | m_testedImageUsage;

    const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const VkBufferImageCopy copyRegion     = makeBufferImageCopy(imageCreateInfo.extent, colorSL);

    // create color attachments that will be used as input in second draw
    ImageWithMemory inputImage(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    auto inputImageView = makeImageView(vk, device, *inputImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // create second image that will be used for verification
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageWithMemory finalImage(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    auto finalImageView = makeImageView(vk, device, *finalImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // create buffer that will be used to read first rendered image
    uint32_t fragmentsCount        = renderSize.x() * renderSize.y();
    const VkDeviceSize bufferSize  = (VkDeviceSize)fragmentsCount * sizeof(uint32_t);
    VkBufferUsageFlags srcDstUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferUsageFlags bufferUsage = srcDstUsage | m_testedBufferUsage;
    auto bufferInfo                = makeBufferCreateInfo(bufferSize, bufferUsage);
    auto memReq(m_usingDeviceAddress ? MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                       MemoryRequirement::HostVisible);
    BufferWithMemory inputBuffer(vk, device, alloc, bufferInfo, memReq);

    // create buffer that will be used to read result of second draw and used for verification
    bufferInfo.usage = srcDstUsage;
    BufferWithMemory finalBuffer(vk, device, alloc, bufferInfo, MemoryRequirement::HostVisible);

    // get device address of input buffer if this is needed by tested case
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, *inputBuffer};
    VkDeviceAddress inputBufferAddress(m_usingDeviceAddress ? vk.getBufferDeviceAddress(device, &addressInfo) : 0);

    // create smapler if it is needed by tested case
    const VkSamplerCreateInfo samplerCreateInfo = initVulkanStructure();
    auto sampler(m_usingSampler ? createSampler(vk, device, &samplerCreateInfo) : Move<VkSampler>());

    // create descriptor set if it is needed by tested case
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    Move<VkDescriptorSetLayout> descriptorSetLayout;
    if (m_usingDescriptorSet)
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(m_testedDescriptorType);
        descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(m_testedDescriptorType, VK_SHADER_STAGE_FRAGMENT_BIT);
        descriptorSetLayout = layoutBuilder.build(vk, device);
        descriptorSet       = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

        // update descriptor set
        using DSUB = DescriptorSetUpdateBuilder;
        DSUB updater;
        if (m_usingBuffer)
        {
            auto descriptorBufferInfo = makeDescriptorBufferInfo(*inputBuffer, 0, bufferSize);
            updater.writeSingle(*descriptorSet, DSUB::Location::binding(0u), m_testedDescriptorType,
                                &descriptorBufferInfo);
        }
        else
        {
            auto descriptorImageInfo = makeDescriptorImageInfo(*sampler, *inputImageView, VK_IMAGE_LAYOUT_GENERAL);
            updater.writeSingle(*descriptorSet, DSUB::Location::binding(0u), m_testedDescriptorType,
                                &descriptorImageInfo);
        }
        updater.update(vk, device);
    }

    // create shader modules
    auto &bc                   = m_context.getBinaryCollection();
    auto vertShaderModule      = createShaderModule(vk, device, bc.get("vert"));
    auto fragWriteShaderModule = createShaderModule(vk, device, bc.get("frag_write"));
    auto fragReadShaderModule  = createShaderModule(vk, device, bc.get("frag_read"));

    // create renderpass and framebuffers for both pipelines
    const uint32_t attCount = 1 + m_usingSecondSubpass;
    VkImageView views[]{*inputImageView, *finalImageView};
    auto renderPass       = setupRenderPass(vk, device, colorFormat);
    auto writeFramebuffer = makeFramebuffer(vk, device, *renderPass, attCount, views, renderSize.x(), renderSize.y());
    Move<VkFramebuffer> readFramebuffer;
    if (!m_usingSecondSubpass)
        readFramebuffer = makeFramebuffer(vk, device, *renderPass, attCount, &views[1], renderSize.x(), renderSize.y());

    // create write and read pipelines
    auto writePipelineLayout = makePipelineLayout(vk, device);
    auto writePipeline =
        makeGraphicsPipeline(vk, device, *writePipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             VK_NULL_HANDLE, *fragWriteShaderModule, *renderPass, viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0, &vertexInputState);
    auto readPipelineLayout = makePipelineLayout(vk, device, m_usingDescriptorSet, &*descriptorSetLayout,
                                                 m_usingDeviceAddress, &pushConstantRange);
    auto readPipeline =
        makeGraphicsPipeline(vk, device, *readPipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             VK_NULL_HANDLE, *fragReadShaderModule, *renderPass, viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, m_usingSecondSubpass, 0, &vertexInputState);

    VkCommandPoolCreateFlags poolFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    auto cmdPool                       = createCommandPool(vk, device, poolFlags, queueFamilyIndex);
    auto cmdBuffer                     = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    if (m_usingSecondSubpass)
    {
        VkClearValue clearValues[2];
        memcpy(clearValues[0].color.uint32, inputClearColor.getPtr(), 4 * sizeof(uint32_t));
        memcpy(clearValues[1].color.uint32, finalClearColor.getPtr(), 4 * sizeof(uint32_t));
        beginRenderPass(vk, *cmdBuffer, *renderPass, *writeFramebuffer, scissors[0], 2, clearValues);

        // draw single triangle with diferent color for each fragment
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writePipeline);
        vk.cmdDraw(*cmdBuffer, 3u, 1u, 0, 0);

        vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // draw same triangle once again but using values from first subpass as inputs
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readPipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *readPipelineLayout, 0u, 1u, &*descriptorSet, 0u, 0);
        vk.cmdDraw(*cmdBuffer, 3u, 1u, 0, 0);

        endRenderPass(vk, *cmdBuffer);
    }
    else
    {
        // draw single triangle, we use this to identify which fragments are under triangle
        beginRenderPass(vk, *cmdBuffer, *renderPass, *writeFramebuffer, scissors[0], inputClearColor);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writePipeline);
        vk.cmdDraw(*cmdBuffer, 3u, 1u, 0, 0);

        endRenderPass(vk, *cmdBuffer);

        if (m_usingBuffer)
        {
            // wait for color attachment to be filled
            auto memoryBarrier = makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &memoryBarrier, 0u, 0u, 0u, 0u);

            // copy color image to buffer
            vk.cmdCopyImageToBuffer(*cmdBuffer, *inputImage, VK_IMAGE_LAYOUT_GENERAL, *inputBuffer, 1u, &copyRegion);

            // wait for buffer to have data from color image
            memoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u,
                                  1u, &memoryBarrier, 0u, 0u, 0u, 0u);
        }
        else
        {
            // wait for color attachment to be filled
            auto memoryBarrier = makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u, 1u, &memoryBarrier, 0u, 0u, 0u, 0u);
        }

        // draw same triangle once again but this time data with previous rendering result is passed as input to FS
        beginRenderPass(vk, *cmdBuffer, *renderPass, *readFramebuffer, scissors[0], finalClearColor);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readPipeline);
        if (m_usingDescriptorSet)
        {
            vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *readPipelineLayout, 0u, 1u, &*descriptorSet, 0u, 0);
        }
        if (m_usingDeviceAddress)
        {
            const auto stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            vk.cmdPushConstants(*cmdBuffer, *readPipelineLayout, stage, 0, sizeof(uint64_t), &inputBufferAddress);
        }
        vk.cmdDraw(*cmdBuffer, 3u, 1u, 0, 0);

        endRenderPass(vk, *cmdBuffer);
    }

    // wait for color image
    auto memoryBarrier = makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          1u, &memoryBarrier, 0u, 0u, 0u, 0u);

    // read back color image with data written by the second FS
    vk.cmdCopyImageToBuffer(*cmdBuffer, *finalImage, VK_IMAGE_LAYOUT_GENERAL, *finalBuffer, 1u, &copyRegion);

    // wait for buffer to have data from color image
    memoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                          &memoryBarrier, 0u, 0u, 0u, 0u);

    endCommandBuffer(vk, *cmdBuffer);

    VkQueue queue;
    vk.getDeviceQueue(device, queueFamilyIndex, 0, &queue);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // get input buffer
    auto &inputAllocation = inputBuffer.getAllocation();
    invalidateAlloc(vk, device, inputAllocation);
    uint32_t *inputData = reinterpret_cast<uint32_t *>(inputAllocation.getHostPtr());

    // get final buffer
    auto &finalAllocation = finalBuffer.getAllocation();
    invalidateAlloc(vk, device, finalAllocation);
    uint32_t *finalData = reinterpret_cast<uint32_t *>(finalAllocation.getHostPtr());

    // verification of final buffer
    if (m_testParam.testType == TestType::OUTPUT_VARIABLES)
    {
        bool testPassed = true;
        for (uint32_t y = 0; testPassed && (y < renderSize.y()); ++y)
        {
            for (uint32_t x = 0; testPassed && (x < renderSize.x()); ++x)
            {
                uint32_t i        = x + y * renderSize.x();
                uint32_t expected = finalClearColor[0];

                // if fragment color is not equal to clear color
                // then calculate the same value as in the shader
                if (finalData[i] != expected)
                    expected = x + y * 32 + x * y;

                testPassed &= (expected == finalData[i]);
            }
        }

        if (testPassed)
            return tcu::TestStatus::pass("Pass");
    }
    else
    {
        uint32_t fragmentsWithZeroColor     = 0; // fragments that are part of quads that are fully in triangle
        uint32_t fragmentsWithClearColor    = 0; // fragments that are not part of any quads that are in triangle
        uint32_t fragmentsWithExpectedColor = 0;
        uint32_t fragmentsWithExpectedColorTimesTwo = 0;
        for (uint32_t i = 0; i < fragmentsCount; ++i)
        {
            uint32_t v = finalData[i];
            fragmentsWithZeroColor += (v == 0);
            fragmentsWithClearColor += (v == finalClearColor[0]);
            fragmentsWithExpectedColor += (v == m_expectedColor);
            fragmentsWithExpectedColorTimesTwo += (v == 2 * m_expectedColor);
        }

        uint32_t checkedSum = fragmentsWithZeroColor + fragmentsWithClearColor + fragmentsWithExpectedColor +
                              fragmentsWithExpectedColorTimesTwo;

        // make sure that helper invocations didn't write to final color attachment
        bool helperWroteColor = false;
        for (uint32_t i = 0; i < fragmentsCount; ++i)
        {
            uint32_t input = inputData[i];
            uint32_t final = finalData[i];
            helperWroteColor |= ((input == inputClearColor[0]) && (final != finalClearColor[0]));
        }

        // expect that result contains only four specified colors;
        // expect that counters of all four colours are greater than minimal values produced by all implementations;
        // expect that coresponding fragments have clear color in input and in final attachment
        if ((fragmentsCount == checkedSum) && (fragmentsWithZeroColor > 120) && (fragmentsWithExpectedColor > 30) &&
            (fragmentsWithExpectedColorTimesTwo > 3) && !helperWroteColor)
            return tcu::TestStatus::pass(std::to_string(fragmentsWithExpectedColor));
    }

    const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess inputAccess(resultFormat, renderSize.x(), renderSize.y(), 1u, inputData);
    tcu::ConstPixelBufferAccess finalAccess(resultFormat, renderSize.x(), renderSize.y(), 1u, finalData);
    m_context.getTestContext().getLog() << tcu::TestLog::Image("Input", "", inputAccess)
                                        << tcu::TestLog::Image("Final", "", finalAccess);
    return tcu::TestStatus::fail("Fail");
}

Move<VkRenderPass> HelperInvocationsTestInstance::setupRenderPass(const DeviceInterface &vk, VkDevice device,
                                                                  VkFormat colorFormat) const
{
    std::array<VkAttachmentDescription, 2> colorAttachmentDescriptions;
    colorAttachmentDescriptions[0] = {
        (VkAttachmentDescriptionFlags)0,  // VkAttachmentDescriptionFlags    flags
        colorFormat,                      // VkFormat                        format
        VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits           samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,      // VkAttachmentLoadOp              loadOp
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp             storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout                   initialLayout
        VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout                   finalLayout
    };
    colorAttachmentDescriptions[1] = colorAttachmentDescriptions[0];

    std::array<VkAttachmentReference, 2> colorAttachmentRefs;
    colorAttachmentRefs[0] = {0u, VK_IMAGE_LAYOUT_GENERAL};
    colorAttachmentRefs[1] = {1u, VK_IMAGE_LAYOUT_GENERAL};

    std::array<VkSubpassDescription, 2> subpassDescriptions;
    memset(subpassDescriptions.data(), 0, 2 * sizeof(VkSubpassDescription));
    subpassDescriptions[0].colorAttachmentCount    = 1;
    subpassDescriptions[0].pColorAttachments       = &colorAttachmentRefs[0];
    subpassDescriptions[0].preserveAttachmentCount = m_usingSecondSubpass;
    subpassDescriptions[0].pPreserveAttachments    = &colorAttachmentRefs[1].attachment;

    subpassDescriptions[1].colorAttachmentCount = 1;
    subpassDescriptions[1].pColorAttachments    = &colorAttachmentRefs[1];
    subpassDescriptions[1].inputAttachmentCount = 1;
    subpassDescriptions[1].pInputAttachments    = &colorAttachmentRefs[0];

    VkSubpassDependency subpassDependency{0,
                                          1,
                                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                          VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                          VK_DEPENDENCY_BY_REGION_BIT};

    const uint32_t subpassCount = 1 + m_usingSecondSubpass;
    const VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType
        nullptr,                                   // const void*                       pNext
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags           flags
        subpassCount,                              // uint32_t                          attachmentCount
        colorAttachmentDescriptions.data(),        // const VkAttachmentDescription*    pAttachments
        subpassCount,                              // uint32_t                          subpassCount
        subpassDescriptions.data(),                // const VkSubpassDescription*       pSubpasses
        m_usingSecondSubpass,                      // uint32_t                          dependencyCount
        &subpassDependency                         // const VkSubpassDependency*        pDependencies
    };

    return createRenderPass(vk, device, &renderPassInfo);
}

class HelperInvocationsTestCase : public TestCase
{
public:
    HelperInvocationsTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParam &testParam)
        : TestCase(testCtx, name.c_str())
        , m_testParam(testParam)
    {
    }
    ~HelperInvocationsTestCase() = default;

    void checkSupport(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &ctx) const override;

private:
    const TestParam m_testParam;
};

void HelperInvocationsTestCase::checkSupport(Context &context) const
{
    if (m_testParam.testType == TestType::LOAD_ADDRESS)
        context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
}

void HelperInvocationsTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert")
        << glu::VertexSource("#version 450\n"
                             "void main(void)\n"
                             "{\n"
                             "\tgl_Position = vec4(float(-0.6 + 0.6 * gl_VertexIndex),\n"
                             "\t                   float( 0.8 - 1.5 * (gl_VertexIndex % 2)), 0.0, 1.0);\n"
                             "}\n");

    std::string writeFragmentSource("#version 450\n"
                                    "layout(location = 0) out uint outColor;\n"
                                    "void main (void)\n"
                                    "{\n"
                                    "\toutColor = 84;\n");
    if (m_testParam.testType == TestType::OUTPUT_VARIABLES)
        writeFragmentSource += "\toutColor = uint(gl_FragCoord.y)*32+uint(gl_FragCoord.x);\n";
    writeFragmentSource += "}\n";
    programCollection.glslSources.add("frag_write") << glu::FragmentSource(writeFragmentSource);

    const std::map<TestType, std::string_view> readFragmentSourceMap{
        {TestType::LOAD_SSBO, "#version 450\n"
                              "layout(location = 0) out uint outColor;\n"
                              "layout(std430, binding=0) readonly buffer Input { uint v[]; };\n"
                              "void main (void)\n"
                              "{\n"
                              "\tuint i = uint(gl_FragCoord.y)*32+uint(gl_FragCoord.x);\n" // 32 is attachment size
                              "\toutColor = uint(fwidth(v[i]));\n"
                              "}\n"},
        {TestType::LOAD_ADDRESS, "#version 450\n"
                                 "#extension GL_EXT_buffer_reference : require\n"
                                 "layout(location = 0) out uint outColor;\n"
                                 "layout(std430, buffer_reference, buffer_reference_align = 4) readonly buffer Data\n"
                                 "{ uint v[]; };\n"
                                 "layout(std430, push_constant) uniform Input { Data data; };\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "\tuint i = uint(gl_FragCoord.y)*32+uint(gl_FragCoord.x);\n"
                                 "\toutColor = uint(fwidth(data.v[i]));\n"
                                 "}\n"},
        {TestType::LOAD_UBO, "#version 450\n"
                             "layout(location = 0) out uint outColor;\n"
                             "layout(binding=0) uniform Input { uvec4 v[32*8]; };\n" // 32*32 = 32 * 8 * 4components
                             "void main (void)\n"
                             "{\n"
                             "\tuint i = uint(gl_FragCoord.y)*8+uint(gl_FragCoord.x) / 4;\n"
                             "\tuvec4 color = v[i];\n"
                             "\toutColor = uint(fwidth(color[uint(gl_FragCoord.x) % 4]));\n"
                             "}\n"},
        {TestType::LOAD_IMAGE, "#version 450\n"
                               "layout(location = 0) out uint outColor;\n"
                               "layout(binding=0, r32ui) readonly uniform uimage2D image;\n"
                               "void main (void)\n"
                               "{\n"
                               "\tuint c = imageLoad(image, ivec2(uint(gl_FragCoord.x), uint(gl_FragCoord.y))).x;\n"
                               "\toutColor = uint(fwidth(c));\n"
                               "}\n"},
        {TestType::LOAD_TEXTURE, "#version 450\n"
                                 "layout(location = 0) out uint outColor;\n"
                                 "layout(binding=0) uniform usampler2D samp;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "\tvec2 uv = vec2(gl_FragCoord.x, gl_FragCoord.y) / 32;\n"
                                 "\tfloat c = texture(samp, uv).r;\n"
                                 "\toutColor = uint(fwidth(c));\n"
                                 "}\n"},
        {TestType::OUTPUT_VARIABLES, "#version 450\n"
                                     "layout(location = 0) out uint outColor;\n"
                                     "layout(input_attachment_index=0, binding=0) uniform usubpassInput image;\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "\tuint c = subpassLoad(image).x;\n"
                                     "\toutColor = c + uint(gl_FragCoord.y) * uint(gl_FragCoord.x);\n"
                                     "}\n"}};

    std::string fs(readFragmentSourceMap.at(m_testParam.testType));
    programCollection.glslSources.add("frag_read") << glu::FragmentSource(fs);
}

vkt::TestInstance *HelperInvocationsTestCase::createInstance(Context &ctx) const
{
    return new HelperInvocationsTestInstance(ctx, m_testParam);
}

void addShaderHelperInvocationsTests(tcu::TestCaseGroup *testGroup)
{
    tcu::TestContext &testCtx = testGroup->getTestContext();

    std::vector<std::pair<std::string, TestType>> testCases{
        {"load_from_ssbo", TestType::LOAD_SSBO},       {"load_from_address", TestType::LOAD_ADDRESS},
        {"load_from_ubo", TestType::LOAD_UBO},         {"load_from_image", TestType::LOAD_IMAGE},
        {"load_from_texture", TestType::LOAD_TEXTURE}, {"output_variables", TestType::OUTPUT_VARIABLES}};

    TestParam testParams;
    for (auto &[name, type] : testCases)
    {
        testParams.testType = type;
        testGroup->addChild(new HelperInvocationsTestCase(testCtx, name, testParams));
    }
}

} // namespace

tcu::TestCaseGroup *createShaderHelperInvocationsTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "helper_invocations", addShaderHelperInvocationsTests);
}

} // namespace vkt::shaderexecutor
