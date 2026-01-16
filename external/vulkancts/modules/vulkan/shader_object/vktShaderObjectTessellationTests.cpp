/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 LunarG, Inc.
 * Copyright (c) 2024 Nintendo
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
 * \brief Shader Object Tessellation Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectCreateTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkObjUtil.hpp"
#include "deRandom.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum SourceType
{
    GLSL,
    HLSL,
};

enum class TestType : int
{
    ORIENTATION_CCW,
    ORIENTATION_CW,
    SPACING_EQUAL,
    SPACING_FRACTIONAL_ODD,
    PATCH_VERTICES_4,
    PATCH_VERTICES_5,
    PRIMITIVE_QUADS,
    PRIMITIVE_TRIANGLES,
    POINT_MODE,
};

class ShaderObjectTessellationInstance : public vkt::TestInstance
{
public:
    ShaderObjectTessellationInstance(Context &context, const TestType testType)
        : vkt::TestInstance(context)
        , m_testType(testType)
    {
    }
    virtual ~ShaderObjectTessellationInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    const TestType m_testType;
};

tcu::TestStatus ShaderObjectTessellationInstance::iterate(void)
{
    const vk::VkInstance instance = m_context.getInstance();
    const vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const vk::VkQueue queue         = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();
    const auto deviceExtensions     = vk::removeUnsupportedShaderObjectExtensions(
        m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());
    const bool tessellationSupported = m_context.getDeviceFeatures().tessellationShader;
    const bool geometrySupported     = m_context.getDeviceFeatures().geometryShader;
    const bool taskSupported         = m_context.getMeshShaderFeaturesEXT().taskShader;
    const bool meshSupported         = m_context.getMeshShaderFeaturesEXT().meshShader;

    vk::VkFormat colorAttachmentFormat = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const auto subresourceRange        = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    const uint32_t size = 32u;

    const vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
        nullptr,                                 // const void*                pNext
        0u,                                      // VkImageCreateFlags        flags
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
        colorAttachmentFormat,                   // VkFormat                    format
        {size, size, 1},                         // VkExtent3D                extent
        1u,                                      // uint32_t                    mipLevels
        1u,                                      // uint32_t                    arrayLayers
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
        vk::VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode            sharingMode
        0,                             // uint32_t                    queueFamilyIndexCount
        nullptr,                       // const uint32_t*            pQueueFamilyIndices
        vk::VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout            initialLayout
    };

    de::MovePtr<vk::ImageWithMemory> image = de::MovePtr<vk::ImageWithMemory>(
        new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
    const auto imageView =
        vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
    const vk::VkRect2D renderArea = vk::makeRect2D(0, 0, size, size);

    const vk::VkDeviceSize colorOutputBufferSize =
        renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
    de::MovePtr<vk::BufferWithMemory> colorOutputBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible));

    const auto &binaries = m_context.getBinaryCollection();

    const auto vertCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"),
                                                         tessellationSupported, geometrySupported);
    const auto tescCreateInfo = vk::makeShaderCreateInfo(
        vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported);
    const auto teseCreateInfo =
        vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"),
                                 tessellationSupported, geometrySupported);
    const auto fragCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"),
                                                         tessellationSupported, geometrySupported);

    const auto vertShader = vk::createShader(vk, device, vertCreateInfo);
    const auto tescShader = vk::createShader(vk, device, tescCreateInfo);
    const auto teseShader = vk::createShader(vk, device, teseCreateInfo);
    const auto fragShader = vk::createShader(vk, device, fragCreateInfo);

    const vk::VkCommandPoolCreateInfo cmdPoolInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType
        nullptr,                                             // pNext
        vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags
        queueFamilyIndex,                                    // queuefamilyindex
    };
    const vk::Move<vk::VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolInfo));
    const vk::Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

    vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(
        vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
        vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, nullptr, 0u,
                          nullptr, 1u, &preImageBarrier);

    vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, VK_NULL_HANDLE, *fragShader,
                            taskSupported, meshSupported);
    vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
                                            false);

    vk.cmdSetPolygonModeEXT(*cmdBuffer, vk::VK_POLYGON_MODE_LINE);
    if (m_testType == TestType::ORIENTATION_CCW || m_testType == TestType::ORIENTATION_CW)
        vk.cmdSetCullMode(*cmdBuffer, vk::VK_CULL_MODE_BACK_BIT);

    const vk::VkClearValue clearValue = vk::makeClearValueColor({0.0f, 0.0f, 0.0f, 1.0f});
    vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL,
                       vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

    vk::VkViewport viewport = {0, 0, size, size, 0.0f, 1.0f};
    vk.cmdSetViewportWithCount(*cmdBuffer, 1u, &viewport);
    vk::VkRect2D scissor = {{0, 0}, {size, size}};
    vk.cmdSetScissorWithCount(*cmdBuffer, 1u, &scissor);
    uint32_t vertexCount = 4u;
    if (m_testType == TestType::PATCH_VERTICES_4 || m_testType == TestType::PATCH_VERTICES_5)
    {
        vk.cmdSetPatchControlPointsEXT(*cmdBuffer, 5u);
        vertexCount = 5u;
    }
    vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
    vk::endRendering(vk, *cmdBuffer);

    vk::VkImageMemoryBarrier postImageBarrier =
        vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u,
                          &postImageBarrier);

    const vk::VkBufferImageCopy copyRegion = {
        0u, // VkDeviceSize bufferOffset;
        0u, // uint32_t bufferRowLength;
        0u, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                     // VkImageAspectFlags aspect;
            0u,                                                // uint32_t mipLevel;
            0u,                                                // uint32_t baseArrayLayer;
            1u,                                                // uint32_t layerCount;
        },                                                     // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                                             // VkOffset3D imageOffset;
        {renderArea.extent.width, renderArea.extent.height, 1} // VkExtent3D imageExtent;
    };
    vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

    vk::endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
    tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(
        vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1,
        (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    const tcu::Vec4 black = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 white = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    const uint32_t testSize = 17u;
    // clang-format off
    const bool basic[testSize][testSize] = {
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1},
        {1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

    const bool fractionalOdd[testSize][testSize] = {
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1},
        {1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1},
        {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1},
        {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1},
        {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1},
        {1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1},
        {1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1},
        {1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

    const bool triangles[testSize][testSize] = {
        {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1},
        {1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0},
        {1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0},
        {1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
        {1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

    const bool pointMode[testSize][testSize] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}};
    // clang-format on

    for (uint32_t j = 0; j < testSize; ++j)
    {
        for (uint32_t i = 0; i < testSize; ++i)
        {
            int x = i + 7;
            int y = j + 7;
            if (m_testType == TestType::PATCH_VERTICES_5)
                y += 5;
            const tcu::Vec4 color = resultBuffer.getPixel(x, y).asFloat();

            bool shouldBeSet = false;
            if (m_testType == TestType::SPACING_FRACTIONAL_ODD)
                shouldBeSet = fractionalOdd[j][i];
            else if (m_testType == TestType::PRIMITIVE_TRIANGLES)
                shouldBeSet = triangles[j][i];
            else if (m_testType == TestType::POINT_MODE)
                shouldBeSet = pointMode[j][i];
            else if (m_testType != TestType::ORIENTATION_CW)
                shouldBeSet = basic[j][i];
            if (shouldBeSet)
            {
                if (color != white)
                {
                    log << tcu::TestLog::Message << "Color at (" << i << ", " << j
                        << ") is expected to be (1.0, 1.0, 1.0, 1.0), but was (" << color << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
            else
            {
                if (color != black)
                {
                    log << tcu::TestLog::Message << "Color at (" << i << ", " << j
                        << ") is expected to be (0.0, 0.0, 0.0, 1.0), but was (" << color << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class ShaderObjectTessellationCase : public vkt::TestCase
{
public:
    ShaderObjectTessellationCase(tcu::TestContext &testCtx, const std::string &name, const SourceType sourceType,
                                 const TestType testType)
        : vkt::TestCase(testCtx, name)
        , m_sourceType(sourceType)
        , m_testType(testType)
    {
    }
    virtual ~ShaderObjectTessellationCase(void)
    {
    }

    void checkSupport(vkt::Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ShaderObjectTessellationInstance(context, m_testType);
    }

private:
    const SourceType m_sourceType;
    const TestType m_testType;
};

void ShaderObjectTessellationCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");
    if (!context.getDeviceFeatures().tessellationShader)
        throw tcu::NotSupportedError("Tessellation shaders are not supported");
}

void ShaderObjectTessellationCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string outputVertices;
    std::string primitive;
    std::string spacing;
    std::string orientation;
    std::string pointMode;

    if (m_testType == TestType::PATCH_VERTICES_5)
        outputVertices = "               OpExecutionMode %main OutputVertices 5\n";
    else
        outputVertices = "               OpExecutionMode %main OutputVertices 4\n";
    if (m_testType == TestType::PRIMITIVE_TRIANGLES)
        primitive = "               OpExecutionMode %main Triangles\n";
    else
        primitive = "               OpExecutionMode %main Quads\n";
    if (m_testType == TestType::SPACING_FRACTIONAL_ODD)
        spacing = "               OpExecutionMode %main SpacingFractionalOdd\n";
    else
        spacing = "               OpExecutionMode %main SpacingEqual\n";
    if (m_testType == TestType::ORIENTATION_CW)
        orientation = "               OpExecutionMode %main VertexOrderCw\n";
    else
        orientation = "               OpExecutionMode %main VertexOrderCcw\n";
    if (m_testType == TestType::POINT_MODE)
        pointMode = "               OpExecutionMode %main PointMode\n";
    else
        pointMode = "";

    // #version 450
    // void main()
    // {
    //     vec2 pos    = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
    //     gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);
    // }

    std::string vert = "               OpCapability Shader\n"
                       "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
                       "               OpMemoryModel Logical GLSL450\n"
                       "               OpEntryPoint Vertex %main \"main\" %gl_VertexIndex %_\n"
                       "               OpSource GLSL 450\n"
                       "               OpName %main \"main\"\n"
                       "               OpName %pos \"pos\"\n"
                       "               OpName %gl_VertexIndex \"gl_VertexIndex\"\n"
                       "               OpName %gl_PerVertex \"gl_PerVertex\"\n"
                       "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n"
                       "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n"
                       "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n"
                       "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n"
                       "               OpName %_ \"\"\n"
                       "               OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n"
                       "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n"
                       "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n"
                       "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n"
                       "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n"
                       "               OpDecorate %gl_PerVertex Block\n"
                       "       %void = OpTypeVoid\n"
                       "          %3 = OpTypeFunction %void\n"
                       "      %float = OpTypeFloat 32\n"
                       "    %v2float = OpTypeVector %float 2\n"
                       "%_ptr_Function_v2float = OpTypePointer Function %v2float\n"
                       "        %int = OpTypeInt 32 1\n"
                       "%_ptr_Input_int = OpTypePointer Input %int\n"
                       "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n"
                       "      %int_1 = OpConstant %int 1\n"
                       "    %v4float = OpTypeVector %float 4\n"
                       "       %uint = OpTypeInt 32 0\n"
                       "     %uint_1 = OpConstant %uint 1\n"
                       "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
                       "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
                       "%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n"
                       "          %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n"
                       "      %int_0 = OpConstant %int 0\n"
                       "  %float_0_5 = OpConstant %float 0.5\n"
                       "    %float_0 = OpConstant %float 0\n"
                       "    %float_1 = OpConstant %float 1\n"
                       "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
                       "       %main = OpFunction %void None %3\n"
                       "          %5 = OpLabel\n"
                       "        %pos = OpVariable %_ptr_Function_v2float Function\n"
                       "         %13 = OpLoad %int %gl_VertexIndex\n"
                       "         %15 = OpBitwiseAnd %int %13 %int_1\n"
                       "         %16 = OpConvertSToF %float %15\n"
                       "         %17 = OpLoad %int %gl_VertexIndex\n"
                       "         %18 = OpShiftRightArithmetic %int %17 %int_1\n"
                       "         %19 = OpBitwiseAnd %int %18 %int_1\n"
                       "         %20 = OpConvertSToF %float %19\n"
                       "         %21 = OpCompositeConstruct %v2float %16 %20\n"
                       "               OpStore %pos %21\n"
                       "         %30 = OpLoad %v2float %pos\n"
                       "         %32 = OpCompositeConstruct %v2float %float_0_5 %float_0_5\n"
                       "         %33 = OpFSub %v2float %30 %32\n"
                       "         %36 = OpCompositeExtract %float %33 0\n"
                       "         %37 = OpCompositeExtract %float %33 1\n"
                       "         %38 = OpCompositeConstruct %v4float %36 %37 %float_0 %float_1\n"
                       "         %40 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n"
                       "               OpStore %40 %38\n"
                       "               OpReturn\n"
                       "               OpFunctionEnd\n";

    // #version 450
    //
    // layout(vertices = 4) out;
    //
    // void main (void) {
    //     if (gl_InvocationID == 0) {
    //         gl_TessLevelInner[0] = 2.0;
    //         gl_TessLevelInner[1] = 2.0;
    //         gl_TessLevelOuter[0] = 2.0;
    //         gl_TessLevelOuter[1] = 2.0;
    //         gl_TessLevelOuter[2] = 2.0;
    //         gl_TessLevelOuter[3] = 2.0;
    //     }
    //     gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    // }

    std::string tesc = "               OpCapability Tessellation\n"
                       "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
                       "               OpMemoryModel Logical GLSL450\n"
                       "               OpEntryPoint TessellationControl %main \"main\" %gl_InvocationID "
                       "%gl_TessLevelInner %gl_TessLevelOuter %gl_out %gl_in\n";
    if (m_sourceType == GLSL)
    {
        tesc += outputVertices;
    }
    else
    {
        tesc += outputVertices;
        tesc += primitive;
        tesc += spacing;
        tesc += orientation;
        tesc += pointMode;
    }
    tesc += "\n"
            "               ; Debug Information\n"
            "               OpSource GLSL 450\n"
            "               OpName %main \"main\"  ; id %4\n"
            "               OpName %gl_InvocationID \"gl_InvocationID\"  ; id %8\n"
            "               OpName %gl_TessLevelInner \"gl_TessLevelInner\"  ; id %20\n"
            "               OpName %gl_TessLevelOuter \"gl_TessLevelOuter\"  ; id %29\n"
            "               OpName %gl_PerVertex \"gl_PerVertex\"  ; id %39\n"
            "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n"
            "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n"
            "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n"
            "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n"
            "               OpName %gl_out \"gl_out\"  ; id %42\n"
            "               OpName %gl_PerVertex_0 \"gl_PerVertex\"  ; id %44\n"
            "               OpMemberName %gl_PerVertex_0 0 \"gl_Position\"\n"
            "               OpMemberName %gl_PerVertex_0 1 \"gl_PointSize\"\n"
            "               OpMemberName %gl_PerVertex_0 2 \"gl_ClipDistance\"\n"
            "               OpMemberName %gl_PerVertex_0 3 \"gl_CullDistance\"\n"
            "               OpName %gl_in \"gl_in\"  ; id %48\n"
            "\n"
            "               ; Annotations\n"
            "               OpDecorate %gl_InvocationID BuiltIn InvocationId\n"
            "               OpDecorate %gl_TessLevelInner Patch\n"
            "               OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner\n"
            "               OpDecorate %gl_TessLevelOuter Patch\n"
            "               OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter\n"
            "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n"
            "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n"
            "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n"
            "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n"
            "               OpDecorate %gl_PerVertex Block\n"
            "               OpMemberDecorate %gl_PerVertex_0 0 BuiltIn Position\n"
            "               OpMemberDecorate %gl_PerVertex_0 1 BuiltIn PointSize\n"
            "               OpMemberDecorate %gl_PerVertex_0 2 BuiltIn ClipDistance\n"
            "               OpMemberDecorate %gl_PerVertex_0 3 BuiltIn CullDistance\n"
            "               OpDecorate %gl_PerVertex_0 Block\n"
            "\n"
            "               ; Types, variables and constants\n"
            "       %void = OpTypeVoid\n"
            "          %3 = OpTypeFunction %void\n"
            "        %int = OpTypeInt 32 1\n"
            "%_ptr_Input_int = OpTypePointer Input %int\n"
            "%gl_InvocationID = OpVariable %_ptr_Input_int Input\n"
            "      %int_0 = OpConstant %int 0\n"
            "       %bool = OpTypeBool\n"
            "      %float = OpTypeFloat 32\n"
            "       %uint = OpTypeInt 32 0\n"
            "     %uint_2 = OpConstant %uint 2\n"
            "%_arr_float_uint_2 = OpTypeArray %float %uint_2\n"
            "%_ptr_Output__arr_float_uint_2 = OpTypePointer Output %_arr_float_uint_2\n"
            "%gl_TessLevelInner = OpVariable %_ptr_Output__arr_float_uint_2 Output\n"
            "    %float_2 = OpConstant %float 2\n"
            "%_ptr_Output_float = OpTypePointer Output %float\n"
            "      %int_1 = OpConstant %int 1\n"
            "     %uint_4 = OpConstant %uint 4\n"
            "%_arr_float_uint_4 = OpTypeArray %float %uint_4\n"
            "%_ptr_Output__arr_float_uint_4 = OpTypePointer Output %_arr_float_uint_4\n"
            "%gl_TessLevelOuter = OpVariable %_ptr_Output__arr_float_uint_4 Output\n"
            "      %int_2 = OpConstant %int 2\n"
            "      %int_3 = OpConstant %int 3\n"
            "    %v4float = OpTypeVector %float 4\n"
            "     %uint_1 = OpConstant %uint 1\n"
            "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
            "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
            "%_arr_gl_PerVertex_uint_4 = OpTypeArray %gl_PerVertex %uint_4\n"
            "%_ptr_Output__arr_gl_PerVertex_uint_4 = OpTypePointer Output %_arr_gl_PerVertex_uint_4\n"
            "     %gl_out = OpVariable %_ptr_Output__arr_gl_PerVertex_uint_4 Output\n"
            "%gl_PerVertex_0 = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
            "    %uint_32 = OpConstant %uint 32\n"
            "%_arr_gl_PerVertex_0_uint_32 = OpTypeArray %gl_PerVertex_0 %uint_32\n"
            "%_ptr_Input__arr_gl_PerVertex_0_uint_32 = OpTypePointer Input %_arr_gl_PerVertex_0_uint_32\n"
            "      %gl_in = OpVariable %_ptr_Input__arr_gl_PerVertex_0_uint_32 Input\n"
            "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
            "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
            "\n"
            "               ; Function main\n"
            "       %main = OpFunction %void None %3\n"
            "          %5 = OpLabel\n"
            "          %9 = OpLoad %int %gl_InvocationID\n"
            "         %12 = OpIEqual %bool %9 %int_0\n"
            "               OpSelectionMerge %14 None\n"
            "               OpBranchConditional %12 %13 %14\n"
            "         %13 = OpLabel\n"
            "         %23 = OpAccessChain %_ptr_Output_float %gl_TessLevelInner %int_0\n"
            "               OpStore %23 %float_2\n"
            "         %25 = OpAccessChain %_ptr_Output_float %gl_TessLevelInner %int_1\n"
            "               OpStore %25 %float_2\n"
            "         %30 = OpAccessChain %_ptr_Output_float %gl_TessLevelOuter %int_0\n"
            "               OpStore %30 %float_2\n"
            "         %31 = OpAccessChain %_ptr_Output_float %gl_TessLevelOuter %int_1\n"
            "               OpStore %31 %float_2\n"
            "         %33 = OpAccessChain %_ptr_Output_float %gl_TessLevelOuter %int_2\n"
            "               OpStore %33 %float_2\n"
            "         %35 = OpAccessChain %_ptr_Output_float %gl_TessLevelOuter %int_3\n"
            "               OpStore %35 %float_2\n"
            "               OpBranch %14\n"
            "         %14 = OpLabel\n"
            "         %43 = OpLoad %int %gl_InvocationID\n"
            "         %49 = OpLoad %int %gl_InvocationID\n"
            "         %51 = OpAccessChain %_ptr_Input_v4float %gl_in %49 %int_0\n"
            "         %52 = OpLoad %v4float %51\n"
            "         %54 = OpAccessChain %_ptr_Output_v4float %gl_out %43 %int_0\n"
            "               OpStore %54 %52\n"
            "               OpReturn\n"
            "               OpFunctionEnd\n";

    // #version 450
    //
    // layout(quads, equal_spacing) in;
    //
    // void main (void) {
    //     float u = gl_TessCoord.x;
    //     float v = gl_TessCoord.y;
    //     float omu = 1.0f - u;
    //     float omv = 1.0f - v;
    //     gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;
    //     if (gl_PatchVerticesIn > 4) {
    //         gl_Position.y += 0.3f;
    //     }
    // }

    std::string tese = "               OpCapability Tessellation\n"
                       "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
                       "               OpMemoryModel Logical GLSL450\n"
                       "               OpEntryPoint TessellationEvaluation %main \"main\" %gl_TessCoord %_ %gl_in "
                       "%gl_PatchVerticesIn\n";
    if (m_sourceType == GLSL)
    {
        tese += primitive;
        tese += spacing;
        tese += orientation;
        tese += pointMode;
    }
    else
    {
        tese += primitive;
    }
    tese += "\n"
            "               ; Debug Information\n"
            "               OpSource GLSL 450\n"
            "               OpName %main \"main\"  ; id %4\n"
            "               OpName %u \"u\"  ; id %8\n"
            "               OpName %gl_TessCoord \"gl_TessCoord\"  ; id %11\n"
            "               OpName %v \"v\"  ; id %17\n"
            "               OpName %omu \"omu\"  ; id %21\n"
            "               OpName %omv \"omv\"  ; id %25\n"
            "               OpName %gl_PerVertex \"gl_PerVertex\"  ; id %30\n"
            "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n"
            "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n"
            "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n"
            "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n"
            "               OpName %_ \"\"  ; id %32\n"
            "               OpName %gl_PerVertex_0 \"gl_PerVertex\"  ; id %38\n"
            "               OpMemberName %gl_PerVertex_0 0 \"gl_Position\"\n"
            "               OpMemberName %gl_PerVertex_0 1 \"gl_PointSize\"\n"
            "               OpMemberName %gl_PerVertex_0 2 \"gl_ClipDistance\"\n"
            "               OpMemberName %gl_PerVertex_0 3 \"gl_CullDistance\"\n"
            "               OpName %gl_in \"gl_in\"  ; id %42\n"
            "               OpName %gl_PatchVerticesIn \"gl_PatchVerticesIn\"  ; id %74\n"
            "\n"
            "               ; Annotations\n"
            "               OpDecorate %gl_TessCoord BuiltIn TessCoord\n"
            "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n"
            "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n"
            "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n"
            "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n"
            "               OpDecorate %gl_PerVertex Block\n"
            "               OpMemberDecorate %gl_PerVertex_0 0 BuiltIn Position\n"
            "               OpMemberDecorate %gl_PerVertex_0 1 BuiltIn PointSize\n"
            "               OpMemberDecorate %gl_PerVertex_0 2 BuiltIn ClipDistance\n"
            "               OpMemberDecorate %gl_PerVertex_0 3 BuiltIn CullDistance\n"
            "               OpDecorate %gl_PerVertex_0 Block\n"
            "               OpDecorate %gl_PatchVerticesIn BuiltIn PatchVertices\n"
            "\n"
            "               ; Types, variables and constants\n"
            "       %void = OpTypeVoid\n"
            "          %3 = OpTypeFunction %void\n"
            "      %float = OpTypeFloat 32\n"
            "%_ptr_Function_float = OpTypePointer Function %float\n"
            "    %v3float = OpTypeVector %float 3\n"
            "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
            "%gl_TessCoord = OpVariable %_ptr_Input_v3float Input\n"
            "       %uint = OpTypeInt 32 0\n"
            "     %uint_0 = OpConstant %uint 0\n"
            "%_ptr_Input_float = OpTypePointer Input %float\n"
            "     %uint_1 = OpConstant %uint 1\n"
            "    %float_1 = OpConstant %float 1\n"
            "    %v4float = OpTypeVector %float 4\n"
            "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
            "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
            "%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n"
            "          %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n"
            "        %int = OpTypeInt 32 1\n"
            "      %int_0 = OpConstant %int 0\n"
            "%gl_PerVertex_0 = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
            "    %uint_32 = OpConstant %uint 32\n"
            "%_arr_gl_PerVertex_0_uint_32 = OpTypeArray %gl_PerVertex_0 %uint_32\n"
            "%_ptr_Input__arr_gl_PerVertex_0_uint_32 = OpTypePointer Input %_arr_gl_PerVertex_0_uint_32\n"
            "      %gl_in = OpVariable %_ptr_Input__arr_gl_PerVertex_0_uint_32 Input\n"
            "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
            "      %int_2 = OpConstant %int 2\n"
            "      %int_3 = OpConstant %int 3\n"
            "      %int_1 = OpConstant %int 1\n"
            "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
            "%_ptr_Input_int = OpTypePointer Input %int\n"
            "%gl_PatchVerticesIn = OpVariable %_ptr_Input_int Input\n"
            "      %int_4 = OpConstant %int 4\n"
            "       %bool = OpTypeBool\n"
            "%float_0_300000012 = OpConstant %float 0.300000012\n"
            "%_ptr_Output_float = OpTypePointer Output %float\n"
            "\n"
            "               ; Function main\n"
            "       %main = OpFunction %void None %3\n"
            "          %5 = OpLabel\n"
            "          %u = OpVariable %_ptr_Function_float Function\n"
            "          %v = OpVariable %_ptr_Function_float Function\n"
            "        %omu = OpVariable %_ptr_Function_float Function\n"
            "        %omv = OpVariable %_ptr_Function_float Function\n"
            "         %15 = OpAccessChain %_ptr_Input_float %gl_TessCoord %uint_0\n"
            "         %16 = OpLoad %float %15\n"
            "               OpStore %u %16\n"
            "         %19 = OpAccessChain %_ptr_Input_float %gl_TessCoord %uint_1\n"
            "         %20 = OpLoad %float %19\n"
            "               OpStore %v %20\n"
            "         %23 = OpLoad %float %u\n"
            "         %24 = OpFSub %float %float_1 %23\n"
            "               OpStore %omu %24\n"
            "         %26 = OpLoad %float %v\n"
            "         %27 = OpFSub %float %float_1 %26\n"
            "               OpStore %omv %27\n"
            "         %35 = OpLoad %float %omu\n"
            "         %36 = OpLoad %float %omv\n"
            "         %37 = OpFMul %float %35 %36\n"
            "         %44 = OpAccessChain %_ptr_Input_v4float %gl_in %int_0 %int_0\n"
            "         %45 = OpLoad %v4float %44\n"
            "         %46 = OpVectorTimesScalar %v4float %45 %37\n"
            "         %47 = OpLoad %float %u\n"
            "         %48 = OpLoad %float %omv\n"
            "         %49 = OpFMul %float %47 %48\n"
            "         %51 = OpAccessChain %_ptr_Input_v4float %gl_in %int_2 %int_0\n"
            "         %52 = OpLoad %v4float %51\n"
            "         %53 = OpVectorTimesScalar %v4float %52 %49\n"
            "         %54 = OpFAdd %v4float %46 %53\n"
            "         %55 = OpLoad %float %u\n"
            "         %56 = OpLoad %float %v\n"
            "         %57 = OpFMul %float %55 %56\n"
            "         %59 = OpAccessChain %_ptr_Input_v4float %gl_in %int_3 %int_0\n"
            "         %60 = OpLoad %v4float %59\n"
            "         %61 = OpVectorTimesScalar %v4float %60 %57\n"
            "         %62 = OpFAdd %v4float %54 %61\n"
            "         %63 = OpLoad %float %omu\n"
            "         %64 = OpLoad %float %v\n"
            "         %65 = OpFMul %float %63 %64\n"
            "         %67 = OpAccessChain %_ptr_Input_v4float %gl_in %int_1 %int_0\n"
            "         %68 = OpLoad %v4float %67\n"
            "         %69 = OpVectorTimesScalar %v4float %68 %65\n"
            "         %70 = OpFAdd %v4float %62 %69\n"
            "         %72 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n"
            "               OpStore %72 %70\n"
            "         %75 = OpLoad %int %gl_PatchVerticesIn\n"
            "         %78 = OpSGreaterThan %bool %75 %int_4\n"
            "               OpSelectionMerge %80 None\n"
            "               OpBranchConditional %78 %79 %80\n"
            "         %79 = OpLabel\n"
            "         %83 = OpAccessChain %_ptr_Output_float %_ %int_0 %uint_1\n"
            "         %84 = OpLoad %float %83\n"
            "         %85 = OpFAdd %float %84 %float_0_300000012\n"
            "         %86 = OpAccessChain %_ptr_Output_float %_ %int_0 %uint_1\n"
            "               OpStore %86 %85\n"
            "               OpBranch %80\n"
            "         %80 = OpLabel\n"
            "               OpReturn\n"
            "               OpFunctionEnd\n";

    // #version 450
    // layout (location=0) out vec4 outColor;
    // void main() {
    //     outColor = vec4(1.0f);
    // }

    std::string frag = "               OpCapability Shader\n"
                       "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
                       "               OpMemoryModel Logical GLSL450\n"
                       "               OpEntryPoint Fragment %main \"main\" %outColor\n"
                       "               OpExecutionMode %main OriginUpperLeft\n"
                       "               OpSource GLSL 450\n"
                       "               OpName %main \"main\"\n"
                       "               OpName %outColor \"outColor\"\n"
                       "               OpDecorate %outColor Location 0\n"
                       "       %void = OpTypeVoid\n"
                       "          %3 = OpTypeFunction %void\n"
                       "      %float = OpTypeFloat 32\n"
                       "    %v4float = OpTypeVector %float 4\n"
                       "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
                       "   %outColor = OpVariable %_ptr_Output_v4float Output\n"
                       "    %float_1 = OpConstant %float 1\n"
                       "         %11 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1\n"
                       "       %main = OpFunction %void None %3\n"
                       "          %5 = OpLabel\n"
                       "               OpStore %outColor %11\n"
                       "               OpReturn\n"
                       "               OpFunctionEnd\n";

    programCollection.spirvAsmSources.add("vert") << vert;
    programCollection.spirvAsmSources.add("tesc") << tesc;
    programCollection.spirvAsmSources.add("tese") << tese;
    programCollection.spirvAsmSources.add("frag") << frag;
}

} // namespace

tcu::TestCaseGroup *createShaderObjectTessellationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> tessellationGroup(new tcu::TestCaseGroup(testCtx, "tessellation"));

    const struct Source
    {
        SourceType sourceType;
        const char *name;
    } sourceTypeTests[] = {
        {GLSL, "glsl"},
        {HLSL, "hlsl"},
    };

    const struct Test
    {
        TestType testType;
        const char *name;
    } testTypes[] = {
        {TestType::ORIENTATION_CCW, "orientation_ccw"},
        {TestType::ORIENTATION_CW, "orientation_cw"},
        {TestType::SPACING_EQUAL, "spacing_equal"},
        {TestType::SPACING_FRACTIONAL_ODD, "spacing_fractional_odd"},
        {TestType::PATCH_VERTICES_4, "patch_vertices_4"},
        {TestType::PATCH_VERTICES_5, "patch_vertices_5"},
        {TestType::PRIMITIVE_QUADS, "primitive_quads"},
        {TestType::PRIMITIVE_TRIANGLES, "primitive_triangles"},
        {TestType::POINT_MODE, "point_mode"},
    };

    for (const auto &sourceType : sourceTypeTests)
    {
        de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, sourceType.name));

        for (const auto &testType : testTypes)
        {
            stageGroup->addChild(
                new ShaderObjectTessellationCase(testCtx, testType.name, sourceType.sourceType, testType.testType));
        }

        tessellationGroup->addChild(stageGroup.release());
    }

    return tessellationGroup.release();
}

} // namespace ShaderObject
} // namespace vkt
