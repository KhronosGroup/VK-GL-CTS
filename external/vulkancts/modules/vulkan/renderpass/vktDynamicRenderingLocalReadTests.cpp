/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Vulkan Dynamic Rendering Local Read Tests
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktDynamicRenderingLocalReadTests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"

#include <iostream>
#include <algorithm>

namespace vkt
{
namespace renderpass
{
namespace
{

using namespace vk;

enum class TestType
{
    // Test maximum number of attachments(color + depth + stencil) readback as input
    MAX_INPUT_ATTACHMENTS = 0,

    // Test maximum attachments remapped repeatedly
    MAX_ATTACHMENTS_REMAPPED_REPEATEDLY,

    // Use input attachments without explicitly setting up any mappings
    INPUT_ATTACHMENTS_WITHOUT_MAPPING,

    // Test that color attachment locations set to ATTACHMENT_UNUSED are not written, and that writes to unmapped locations are discarded
    UNUSED_WRITEN_DISCARDED,

    // Test mapping depth + stencil to no index
    DEPTH_STENCIL_MAPPING_TO_NO_INDEX,

    // Test mapping depth + stencil to no index and DS attachment having only the depth aspect.
    // The test clears depth and reads from it in the shader.
    DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR,

    // Test mapping depth + stencil to no index and DS attachment having only the stencil aspect.
    // The test clears stencil and reads from it in the shader.
    DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR,

    // Test mapping depth + stencil to the same index
    DEPTH_STENCIL_MAPPING_TO_SAME_INDEX,

    // Test mapping depth + stencil to large index
    DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX,

    // Test mapping depth + stencil so only one takes an index, the other does not
    DEPTH_MAPPING_STENCIL_NOT,

    // Test that blend state is using unmapped indexes
    MAPPING_NOT_AFFECTING_BLEND_STATE,

    // Test interaction with VK_EXT_color_write_enable
    INTERACTION_WITH_COLOR_WRITE_ENABLE,

    // Test interaction with VK_EXT_graphics_pipeline_library
    INTERACTION_WITH_GRAPHICS_PIPELINE_LIBRARY,

    // Test interaction with VK_EXT_extended_dynamic_state3
    INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3,

    // Test interaction with VK_EXT_shader_object
    INTERACTION_WITH_SHADER_OBJECT,

    // One subpass input attachment, where that input is also the color attachment
    FEEDBACK_LOOP,
};

// During test creation we dont know what is the maximal number of input attachments.
// To be able to test maximal number of attachments we need to construct shaders for all possible
// numbers of input attachments. This number must also not be greater then maxColorAttachments.
static uint32_t inputAttachmentsPossibleValues[] = {4, 5, 6, 7, 8, 9, 10, 16, 17, 18};

using ImageWithMemorySp  = de::SharedPtr<ImageWithMemory>;
using BufferWithMemorySp = de::SharedPtr<BufferWithMemory>;
using VkImageViewSp      = de::SharedPtr<Move<VkImageView>>;

ImageWithMemorySp createImage(Context &context, uint32_t renderSize, VkFormat format, VkImageUsageFlags usage)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    VkDevice device           = context.getDevice();
    Allocator &memAlloc       = context.getDefaultAllocator();
    VkExtent3D extent         = makeExtent3D(renderSize, renderSize, 1u);

    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        extent,                              // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    return ImageWithMemorySp(new ImageWithMemory(vk, device, memAlloc, imageCreateInfo, MemoryRequirement::Any));
}

class BasicLocalReadTestInstance : public vkt::TestInstance
{
public:
    BasicLocalReadTestInstance(Context &context, TestType testType);
    ~BasicLocalReadTestInstance() = default;

protected:
    tcu::TestStatus iterate(void) override;

private:
    void CalculateExpectedValues(void);
    bool UseColorWriteEnable(void) const;
    uint32_t GetRemappedColorAttachment(uint32_t inputDrawId, uint32_t location) const;
    uint32_t GetRemappedColorInputAttachment(uint32_t outputDrawId, uint32_t inputAttachmentIdx) const;

    const TestType m_testType;
    const uint32_t m_renderSize;
    VkFormat m_dsFormat;
    VkImageAspectFlags m_dsAspectMask;
    VkClearValue m_dsClearValue;

    uint32_t m_colorAttachmentCount;
    uint32_t m_inputDrawsCount;
    uint32_t m_outputDrawsCount;
    std::string m_writeFragName;
    std::string m_readFragName;
    std::vector<std::vector<uint32_t>> m_colorAttachmentLocations;
    std::vector<std::vector<uint32_t>> m_colorAttachmentInputIndices;
    uint32_t m_depthInputAttachmentIndex;
    uint32_t m_stencilInputAttachmentIndex;
    const VkBool32 m_colorWriteEnables[4];
    VkBool32 m_useMapping;
    VkBool32 m_useDepthInReadFrag;
    VkBool32 m_useStencilInReadFrag;
    std::vector<uint32_t> m_expectedValues;
};

BasicLocalReadTestInstance::BasicLocalReadTestInstance(Context &context, TestType testType)
    : vkt::TestInstance(context)
    , m_testType(testType)
    , m_renderSize(16)
    , m_dsFormat(VK_FORMAT_D24_UNORM_S8_UINT)
    , m_dsAspectMask(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    , m_dsClearValue(makeClearValueDepthStencil(0.0f, 0))
    , m_colorAttachmentCount(4)
    , m_inputDrawsCount(1)
    , m_outputDrawsCount(1)
    , m_writeFragName("frag0")
    , m_readFragName("frag1")
    , m_colorAttachmentLocations{{0, 1, 2, 3}}
    , m_colorAttachmentInputIndices{{0, 1, 2, 3}}
    , m_depthInputAttachmentIndex(4)
    , m_stencilInputAttachmentIndex(5)
    , m_colorWriteEnables{0, 1, 0, 1}
    , m_useMapping(true)
    , m_useDepthInReadFrag(true)
    , m_useStencilInReadFrag(true)
    , m_expectedValues{0}
{
    const InstanceInterface &vki                = m_context.getInstanceInterface();
    VkPhysicalDevice physicalDevice             = m_context.getPhysicalDevice();
    const VkPhysicalDeviceProperties properties = getPhysicalDeviceProperties(vki, physicalDevice);

    // pick depth stencil format (one of those two has to be supported)
    VkImageFormatProperties imageFormatProperties;
    if (vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_dsFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 0,
            &imageFormatProperties))
        m_dsFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    // setup test configuration - all test cases use same test instance code but with diferent parameters
    switch (m_testType)
    {
    case TestType::MAX_INPUT_ATTACHMENTS:
    {
        m_colorAttachmentCount = deMinu32(properties.limits.maxColorAttachments,
                                          properties.limits.maxPerStageDescriptorInputAttachments - 2u);

        // if this assert is trigered then shader for number m_colorAttachmentCount+2 was not prepared;
        // to fix this just add value of m_colorAttachmentCount+2 to the inputAttachmentsPossibleValues array on top of this file
        DE_ASSERT(std::find(std::begin(inputAttachmentsPossibleValues), std::end(inputAttachmentsPossibleValues),
                            m_colorAttachmentCount + 2) != std::end(inputAttachmentsPossibleValues));

        m_writeFragName += "_" + std::to_string(m_colorAttachmentCount);
        m_readFragName +=
            "_" + std::to_string(m_colorAttachmentCount + 2u); // +2 because depth and stencil are read too
        m_depthInputAttachmentIndex   = m_colorAttachmentCount;
        m_stencilInputAttachmentIndex = m_colorAttachmentCount + 1;

        m_colorAttachmentLocations[0].resize(m_colorAttachmentCount);
        m_colorAttachmentInputIndices[0].resize(m_colorAttachmentCount);
        for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
        {
            m_colorAttachmentLocations[0][attIndex]    = attIndex;
            m_colorAttachmentInputIndices[0][attIndex] = attIndex;
        }
        break;
    }
    case TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY:
    {
        m_colorAttachmentCount = deMinu32(properties.limits.maxColorAttachments,
                                          properties.limits.maxPerStageDescriptorInputAttachments - 2u);
        m_inputDrawsCount      = m_colorAttachmentCount / 2u;
        m_colorAttachmentCount = m_inputDrawsCount * 2u;
        m_outputDrawsCount     = 3;
        m_readFragName +=
            "_" + std::to_string(m_colorAttachmentCount + 2u); // +2 because depth and stencil are read too
        m_depthInputAttachmentIndex   = m_colorAttachmentCount;
        m_stencilInputAttachmentIndex = m_colorAttachmentCount + 1;

        // each input draw uses just two color attachments; each time its different pair that is always mapped to location 0 and 1
        m_colorAttachmentLocations.clear();
        m_colorAttachmentLocations.resize(m_inputDrawsCount,
                                          std::vector<uint32_t>(m_colorAttachmentCount, VK_ATTACHMENT_UNUSED));
        for (uint32_t inputDraw = 0; inputDraw < m_inputDrawsCount; ++inputDraw)
        {
            auto &colorAttachmentLocations       = m_colorAttachmentLocations[inputDraw];
            uint32_t index                       = 2u * inputDraw;
            colorAttachmentLocations[index]      = 0u;
            colorAttachmentLocations[index + 1u] = 1u;
        }

        // allocate space for all different remappings
        m_colorAttachmentInputIndices.resize(m_outputDrawsCount);
        for (auto &inputIndices : m_colorAttachmentInputIndices)
            inputIndices.resize(m_colorAttachmentCount);

        // each output draw will use all input attachments but remapped differently
        // first remapping: reversed order, from greatest index to smallest
        // second remapping: alternately takes the smallest not used index and then the largest not used index
        // third remapping: same order as index, from smallest index to greatest
        for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
        {
            m_colorAttachmentInputIndices[0][attIndex] = m_colorAttachmentCount - 1 - attIndex;
            m_colorAttachmentInputIndices[1][attIndex] =
                (attIndex % 2) ? (m_colorAttachmentCount - 1 - attIndex / 2u) : (attIndex / 2u);
            m_colorAttachmentInputIndices[2][attIndex] = attIndex;
        }
        break;
    }
    case TestType::INPUT_ATTACHMENTS_WITHOUT_MAPPING:
    {
        m_useStencilInReadFrag        = false;
        m_colorAttachmentCount        = 3;
        m_useMapping                  = false;
        m_stencilInputAttachmentIndex = VK_ATTACHMENT_UNUSED;
        break;
    }
    case TestType::UNUSED_WRITEN_DISCARDED:
    {
        m_useStencilInReadFrag           = true;
        m_colorAttachmentCount           = 4;
        m_colorAttachmentLocations[0][0] = VK_ATTACHMENT_UNUSED;
        m_colorAttachmentLocations[0][2] = VK_ATTACHMENT_UNUSED;
        break;
    }
    case TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX:
    {
        m_useStencilInReadFrag        = true;
        m_colorAttachmentCount        = 2;
        m_depthInputAttachmentIndex   = VK_ATTACHMENT_UNUSED;
        m_stencilInputAttachmentIndex = VK_ATTACHMENT_UNUSED;
        break;
    }
    case TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR:
    {
        m_dsFormat             = VK_FORMAT_D32_SFLOAT;
        m_dsAspectMask         = VK_IMAGE_ASPECT_DEPTH_BIT;
        m_dsClearValue         = makeClearValueDepthStencil(0.6f, 1);
        m_inputDrawsCount      = 0;
        m_useStencilInReadFrag = false;
        m_colorAttachmentCount = 0;
        break;
    }
    case TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR:
    {
        m_dsFormat             = VK_FORMAT_S8_UINT;
        m_dsAspectMask         = VK_IMAGE_ASPECT_STENCIL_BIT;
        m_dsClearValue         = makeClearValueDepthStencil(0.6f, 1);
        m_inputDrawsCount      = 0;
        m_useDepthInReadFrag   = false;
        m_colorAttachmentCount = 0;
        break;
    }
    case TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX:
    {
        m_useStencilInReadFrag        = true;
        m_colorAttachmentCount        = 2;
        m_depthInputAttachmentIndex   = 2;
        m_stencilInputAttachmentIndex = 2;
        break;
    }
    case TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX:
    {
        m_useStencilInReadFrag        = true;
        m_colorAttachmentCount        = 2;
        m_depthInputAttachmentIndex   = 20;
        m_stencilInputAttachmentIndex = 21;
        break;
    }
    case TestType::DEPTH_MAPPING_STENCIL_NOT:
    {
        m_useStencilInReadFrag        = false;
        m_colorAttachmentCount        = 4;
        m_depthInputAttachmentIndex   = 4;
        m_stencilInputAttachmentIndex = VK_ATTACHMENT_UNUSED;
        break;
    }
    case TestType::INTERACTION_WITH_COLOR_WRITE_ENABLE:
    {
        m_useStencilInReadFrag        = false;
        m_colorAttachmentCount        = 4;
        m_colorAttachmentLocations    = {{0, 3, 1, 2}};
        m_depthInputAttachmentIndex   = 4;
        m_stencilInputAttachmentIndex = VK_ATTACHMENT_UNUSED;
        break;
    }
    case TestType::INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3:
    {
        m_useStencilInReadFrag        = false;
        m_colorAttachmentCount        = 4;
        m_colorAttachmentLocations    = {{0, 3, 1, 2}};
        m_depthInputAttachmentIndex   = 4;
        m_stencilInputAttachmentIndex = VK_ATTACHMENT_UNUSED;
        break;
    }
    default:
        DE_ASSERT(false);
        break;
    }

    CalculateExpectedValues();
}

uint32_t BasicLocalReadTestInstance::GetRemappedColorAttachment(uint32_t inputDrawId, uint32_t location) const
{
    uint32_t attIndexRemaped = VK_ATTACHMENT_UNUSED;

    // Find the remapped attachment index of a output color with decorator location = colorIdx in generateWriteFragSource
    for (uint32_t i = 0; i < m_colorAttachmentCount; i++)
    {
        if (location == m_colorAttachmentLocations[inputDrawId][i])
        {
            attIndexRemaped = i;
            break;
        }
    }

    return attIndexRemaped;
}

uint32_t BasicLocalReadTestInstance::GetRemappedColorInputAttachment(uint32_t outputDrawId,
                                                                     uint32_t inputAttachmentIdx) const
{
    // perform same operation as in frag1_* shader to calculate final expected value
    uint32_t attIndexRemaped = VK_ATTACHMENT_UNUSED;

    for (uint32_t i = 0; i < m_colorAttachmentCount; i++)
    {
        if (inputAttachmentIdx == m_colorAttachmentInputIndices[outputDrawId][i])
        {
            attIndexRemaped = i;
            break;
        }
    }

    DE_ASSERT(attIndexRemaped != VK_ATTACHMENT_UNUSED);

    return attIndexRemaped;
}

void BasicLocalReadTestInstance::CalculateExpectedValues()
{
    // generate same valueas for each attachment as in frag0_* shader
    std::vector<uint32_t> valuesPerColorAttachment(m_colorAttachmentCount, 0);

    for (uint32_t inputDraw = 0; inputDraw < m_inputDrawsCount; ++inputDraw)
    {
        for (uint32_t colorIdx = 0; colorIdx < m_colorAttachmentCount / m_inputDrawsCount; colorIdx++)
        {
            uint32_t outColor        = 0;
            uint32_t attIndexRemaped = GetRemappedColorAttachment(inputDraw, colorIdx);

            // Calculate the shader ouput in generateWriteFragSource
            if ((UseColorWriteEnable() == false) || (m_colorWriteEnables[attIndexRemaped] == 1))
            {
                outColor = (2u * inputDraw + colorIdx + 1u) * (2u * inputDraw + colorIdx + 1u);
            }

            // Write color output to the remapped attachment
            if (attIndexRemaped != VK_ATTACHMENT_UNUSED)
            {
                valuesPerColorAttachment[attIndexRemaped] = outColor;
            }
        }
    }

    // calculate expected values for all three output draws, same as it will be done in frag1_* shader
    m_expectedValues.resize(m_outputDrawsCount);

    for (uint32_t outputDraw = 0; outputDraw < m_outputDrawsCount; ++outputDraw)
    {
        // Depth read is 0.6 and stencil read is 1
        const uint32_t depthRead   = static_cast<uint32_t>(0.6f * 1000);
        const uint32_t stencilRead = 1;

        if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX)
        {
            // Depth read and stencil read are both enabled
            m_expectedValues[outputDraw] = depthRead + stencilRead * 100;
        }
        else if ((m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX) ||
                 (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX) ||
                 (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR) ||
                 (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR))
        {
            m_expectedValues[outputDraw] = m_useDepthInReadFrag * depthRead + m_useStencilInReadFrag * stencilRead;
        }
        else
        {
            m_expectedValues[outputDraw] = depthRead + ((m_useStencilInReadFrag) ? (stencilRead * 1000) : 0);
        }

        // each output draw uses all attachments but remaped differently
        for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
        {
            uint32_t attIndexRemaped = GetRemappedColorInputAttachment(outputDraw, attIndex);

            if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX)
            {
                // Accumulate inColor_i, in which inColor_i is the color attachment with location = i in shader
                m_expectedValues[outputDraw] += valuesPerColorAttachment[attIndexRemaped];
            }
            else if ((m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX) ||
                     (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX))
            {
                // No color is read
                break;
            }
            else
            {
                // Accumulate i * inColor_i, in which inColor_i is the color attachment with location = i in shader
                m_expectedValues[outputDraw] += (attIndex + 1u) * valuesPerColorAttachment[attIndexRemaped];
            }
        }
    }
}

bool BasicLocalReadTestInstance::UseColorWriteEnable() const
{
    return (m_testType == TestType::INTERACTION_WITH_COLOR_WRITE_ENABLE) ? true : false;
}

tcu::TestStatus BasicLocalReadTestInstance::iterate(void)
{
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    Allocator &memAlloc                    = m_context.getDefaultAllocator();
    VkQueue queue                          = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex        = m_context.getUniversalQueueFamilyIndex();
    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceRange dSRR     = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceRange sSRR     = makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceRange dsSRR    = makeImageSubresourceRange(m_dsAspectMask, 0u, 1u, 0u, 1u);

    const std::vector<VkViewport> viewports{makeViewport(m_renderSize, m_renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize, m_renderSize)};

    const bool useDepthAspect      = (m_dsAspectMask & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool useStencilAspect    = (m_dsAspectMask & VK_IMAGE_ASPECT_STENCIL_BIT);
    const bool useColorWriteEnable = UseColorWriteEnable();
    const bool useUseExtendedDynamicState3(m_testType == TestType::INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3);

    // define few structures that will be modified and reused in multiple places
    VkImageMemoryBarrier colorImageBarrier =
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, VK_NULL_HANDLE, colorSRR);
    VkImageMemoryBarrier dsImageBarrier =
        makeImageMemoryBarrier(0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, VK_NULL_HANDLE, dsSRR);
    VkRenderingAttachmentInfo depthStencilAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        VK_NULL_HANDLE,                              // VkImageView imageView;
        VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,    // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                              // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                 // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp storeOp;
        m_dsClearValue                               // VkClearValue clearValue;
    };
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkDescriptorImageInfo depthImageDescriptor(
        makeDescriptorImageInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR));
    VkDescriptorImageInfo stencilImageDescriptor(depthImageDescriptor);

    // construct required containers
    std::vector<ImageWithMemorySp> images(m_colorAttachmentCount + 1,
                                          ImageWithMemorySp()); // +1 for depth+stencil image
    std::vector<VkImageViewSp> imageViews(m_colorAttachmentCount + 3,
                                          VkImageViewSp()); // +3 for separate depth and stencil and depth+stencil
    std::vector<VkFormat> colorImageFormats(m_colorAttachmentCount, VK_FORMAT_R32_UINT);
    std::vector<VkImageMemoryBarrier> colorImageBarriers(m_colorAttachmentCount, colorImageBarrier);
    std::vector<VkRenderingAttachmentInfo> colorAttachments(m_colorAttachmentCount, depthStencilAttachment);
    std::vector<VkDescriptorImageInfo> colorImageDescriptors(m_colorAttachmentCount, depthImageDescriptor);
    std::vector<BufferWithMemorySp> outputBuffers(m_outputDrawsCount, BufferWithMemorySp());
    std::vector<Move<VkPipeline>> writeGraphicsPipelines(m_inputDrawsCount);
    std::vector<Move<VkPipeline>> readGraphicsPipelines(m_outputDrawsCount);

    auto setupImageViewSp = [&](VkImage image, VkFormat format, const VkImageSubresourceRange &srr)
    {
        return VkImageViewSp(
            new vk::Move<VkImageView>(makeImageView(vk, device, image, VK_IMAGE_VIEW_TYPE_2D, format, srr)));
    };

    // create images and image views for color attachments and use same loop to fill other needed containers
    for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
    {
        images[attIndex]     = createImage(m_context, m_renderSize, colorImageFormats[attIndex], imageUsage);
        imageViews[attIndex] = setupImageViewSp(**images[attIndex], colorImageFormats[attIndex], colorSRR);

        colorImageBarriers[attIndex].image        = **images[attIndex];
        colorAttachments[attIndex].imageView      = **imageViews[attIndex];
        colorImageDescriptors[attIndex].imageView = **imageViews[attIndex];
    }

    // create image and image views for depth/stencil attachments
    uint32_t dsIndex = m_colorAttachmentCount;
    images[dsIndex]  = createImage(m_context, m_renderSize, m_dsFormat,
                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (useDepthAspect)
    {
        imageViews[dsIndex]            = setupImageViewSp(**images[dsIndex], m_dsFormat, dSRR);
        depthImageDescriptor.imageView = **imageViews[dsIndex];
    }
    if (useStencilAspect)
    {
        imageViews[dsIndex + useDepthAspect] = setupImageViewSp(**images[dsIndex], m_dsFormat, sSRR);
        stencilImageDescriptor.imageView     = **imageViews[dsIndex + useDepthAspect];
    }
    imageViews[dsIndex + useDepthAspect + useStencilAspect] = setupImageViewSp(**images[dsIndex], m_dsFormat, dsSRR);
    dsImageBarrier.image                                    = **images[dsIndex];
    depthStencilAttachment.imageView                        = **imageViews[dsIndex + useDepthAspect + useStencilAspect];

    // define buffers for output
    const VkDeviceSize outputBufferSize = static_cast<VkDeviceSize>(m_renderSize * m_renderSize * sizeof(uint32_t));
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    for (uint32_t buffIndex = 0; buffIndex < m_outputDrawsCount; ++buffIndex)
        outputBuffers[buffIndex] = BufferWithMemorySp(
            new BufferWithMemory(vk, device, memAlloc, bufferCreateInfo, MemoryRequirement::HostVisible));

    // create descriptors, they are needed just for read pipelines (usually there is just one read pipeline)
    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, (m_colorAttachmentCount + 2u) * m_outputDrawsCount)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_outputDrawsCount)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u * m_outputDrawsCount);

    // first descriptor set contains all input attachments
    DescriptorSetLayoutBuilder descriptorSetLayoutABuilder;
    for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount + 2; ++attIndex)
        descriptorSetLayoutABuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
    Move<VkDescriptorSetLayout> descriptorSetLayoutA = descriptorSetLayoutABuilder.build(vk, device);

    // second and following descriptor sets contain just single output buffer each
    Move<VkDescriptorSetLayout> descriptorSetLayoutB =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);

    std::vector<Move<VkDescriptorSet>> bufferDescriptorSets(m_outputDrawsCount);
    std::vector<Move<VkDescriptorSet>> inputAttachmentsDescriptorSets(m_outputDrawsCount);
    for (uint32_t i = 0; i < m_outputDrawsCount; ++i)
    {
        inputAttachmentsDescriptorSets[i] = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutA);
        bufferDescriptorSets[i]           = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutB);
    }

    // set descriptor sets for all input attachments
    using DSLocation = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    for (uint32_t i = 0; i < m_outputDrawsCount; ++i)
    {
        // content of the descriptor set with input attachment bindings must be consistent with the remapping
        for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
        {
            uint32_t remapedIndex = GetRemappedColorInputAttachment(i, attIndex);

            descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i], DSLocation::binding(attIndex),
                                                   VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                   &colorImageDescriptors[remapedIndex]);
        }
        if (useDepthAspect)
            descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i],
                                                   DSLocation::binding(m_colorAttachmentCount),
                                                   VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &depthImageDescriptor);
        if (useStencilAspect)
            descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i],
                                                   DSLocation::binding(m_colorAttachmentCount + useDepthAspect),
                                                   VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &stencilImageDescriptor);
    }

    // set descriptor sets for all output buffers
    VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(VK_NULL_HANDLE, 0u, VK_WHOLE_SIZE);
    for (uint32_t buffIndex = 0; buffIndex < m_outputDrawsCount; ++buffIndex)
    {
        bufferInfo.buffer = **outputBuffers[buffIndex];
        descriptorSetUpdateBuilder.writeSingle(*bufferDescriptorSets[buffIndex], DSLocation::binding(0),
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    }

    // update descriptor sets
    descriptorSetUpdateBuilder.update(vk, device);

    // reuse MAX_INPUT_ATTACHMENTS case to also test pColorAttachmentInputIndices set to NULL
    if (m_testType == TestType::MAX_INPUT_ATTACHMENTS)
        m_colorAttachmentInputIndices[0].clear();

    // create components for pipelines
    const VkPushConstantRange pushConstantRange             = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4};
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {*descriptorSetLayoutA, *descriptorSetLayoutB};
    Move<VkPipelineLayout> writePipelineLayout = makePipelineLayout(vk, device, 0, nullptr, 1u, &pushConstantRange);
    Move<VkPipelineLayout> readPipelineLayout  = makePipelineLayout(vk, device, descriptorSetLayouts);
    auto &bc                                   = m_context.getBinaryCollection();
    Move<VkShaderModule> vertShaderModule      = createShaderModule(vk, device, bc.get("vert"), 0);
    Move<VkShaderModule> writeFragShaderModule = createShaderModule(vk, device, bc.get(m_writeFragName), 0);
    Move<VkShaderModule> readFragShaderModule  = createShaderModule(vk, device, bc.get(m_readFragName), 0);

    // define empty VertexInputState, full screen quad will be generated in vertex shader
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    // define ColorBlendState so that we can write to multiple color attachments
    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState{0,
                                                                        VK_BLEND_FACTOR_ZERO,
                                                                        VK_BLEND_FACTOR_ZERO,
                                                                        VK_BLEND_OP_ADD,
                                                                        VK_BLEND_FACTOR_ZERO,
                                                                        VK_BLEND_FACTOR_ZERO,
                                                                        VK_BLEND_OP_ADD,
                                                                        0xf};
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(m_colorAttachmentCount,
                                                                                colorBlendAttachmentState);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = (uint32_t)colorBlendAttachmentStates.size();
    colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates.data();

    // define MultisampleState, it is only needed to test if CmdSetRasterizationSamplesEXT does not affect local_read remappings
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = initVulkanStructure();
    multisampleStateCreateInfo.rasterizationSamples =
        useUseExtendedDynamicState3 ? VK_SAMPLE_COUNT_2_BIT : VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.minSampleShading = 1.0f;

    // define DepthStencilState so that we can write to depth and stencil attachments
    const VkStencilOpState stencilOpState{
        VK_STENCIL_OP_KEEP,                // VkStencilOp                                 failOp
        VK_STENCIL_OP_INCREMENT_AND_CLAMP, // VkStencilOp                                 passOp
        VK_STENCIL_OP_KEEP,                // VkStencilOp                                 depthFailOp
        VK_COMPARE_OP_ALWAYS,              // VkCompareOp                                 compareOp
        0xffu,                             // uint32_t                                    compareMask
        0xffu,                             // uint32_t                                    writeMask
        0                                  // uint32_t                                    reference
    };
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                        sType
        nullptr,                                                    // const void*                            pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags
        useDepthAspect,        // VkBool32                               depthTestEnable
        VK_TRUE,               // VkBool32                               depthWriteEnable
        VK_COMPARE_OP_GREATER, // VkCompareOp                            depthCompareOp
        VK_FALSE,              // VkBool32                               depthBoundsTestEnable
        useStencilAspect,      // VkBool32                               stencilTestEnable
        stencilOpState,        // VkStencilOpState                       front
        stencilOpState,        // VkStencilOpState                       back
        0.0f,                  // float                                  minDepthBounds
        1.0f,                  // float                                  maxDepthBounds
    };

    VkDynamicState dynamicState = VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT;
    if (useUseExtendedDynamicState3)
        dynamicState = VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT;
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType
        nullptr,                                              // const void*                          pNext
        0u,                                                   // VkPipelineDynamicStateCreateFlags    flags
        1u,                                                   // uint32_t                             dynamicStateCount
        &dynamicState                                         // const VkDynamicState*                pDynamicStates
    };
    VkPipelineDynamicStateCreateInfo *writeDynamicStateCreateInfo = nullptr;
    if (useColorWriteEnable || useUseExtendedDynamicState3)
        writeDynamicStateCreateInfo = &dynamicStateCreateInfo;

    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR, nullptr, m_colorAttachmentCount, nullptr};
    bool useDSInputAttachmentIndex(useDepthAspect && useStencilAspect);
    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo{
        VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
        nullptr,
        m_colorAttachmentCount,
        nullptr,
        (useDSInputAttachmentIndex) ? &m_depthInputAttachmentIndex : nullptr,
        (useDSInputAttachmentIndex) ? &m_stencilInputAttachmentIndex : nullptr};
    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                      nullptr,
                                                      0u,
                                                      (uint32_t)colorImageFormats.size(),
                                                      colorImageFormats.data(),
                                                      useDepthAspect ? m_dsFormat : VK_FORMAT_UNDEFINED,
                                                      useStencilAspect ? m_dsFormat : VK_FORMAT_UNDEFINED};

    // set attachment locations remapping for write pipelines
    if (m_useMapping)
        renderingCreateInfo.pNext = &renderingAttachmentLocationInfo;

    // create write pipelines that writes to color attachments
    for (uint32_t pipelineIndex = 0; pipelineIndex < m_inputDrawsCount; ++pipelineIndex)
    {
        renderingAttachmentLocationInfo.pColorAttachmentLocations = m_colorAttachmentLocations[pipelineIndex].data();
        writeGraphicsPipelines[pipelineIndex]                     = makeGraphicsPipeline(
            vk, device, *writePipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            *writeFragShaderModule, VK_NULL_HANDLE, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
            &vertexInputState, nullptr, &multisampleStateCreateInfo, &depthStencilStateCreateInfo,
            &colorBlendStateCreateInfo, writeDynamicStateCreateInfo, &renderingCreateInfo);

        // writte to depth and stencil only in first pipeline
        depthStencilStateCreateInfo.depthTestEnable   = false;
        depthStencilStateCreateInfo.stencilTestEnable = false;
    }

    // set input attachments remapping for read pipelines
    if (m_useMapping)
        renderingCreateInfo.pNext = &renderingInputAttachmentIndexInfo;

    // read pipelines don't write to the color attachments
    for (auto &cb : colorBlendAttachmentStates)
        cb.colorWriteMask = 0;

    // Per spec, if either of pDepthInputAttachmentIndex or pStencilInputAttachmentIndex are
    // set to NULL it means that these are only accessible in the shader if the shader does not
    // associate these input attachments with an InputAttachmentIndex.
    if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX)
    {
        renderingInputAttachmentIndexInfo.pDepthInputAttachmentIndex   = NULL;
        renderingInputAttachmentIndexInfo.pStencilInputAttachmentIndex = NULL;
    }

    uint32_t subpass = (m_inputDrawsCount > 0);
    for (uint32_t pipelineIndex = 0; pipelineIndex < m_outputDrawsCount; ++pipelineIndex)
    {
        renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices =
            de::dataOrNull(m_colorAttachmentInputIndices[pipelineIndex]);
        readGraphicsPipelines[pipelineIndex] = makeGraphicsPipeline(
            vk, device, *readPipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            *readFragShaderModule, VK_NULL_HANDLE, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, subpass,
            0, &vertexInputState, nullptr, nullptr, nullptr, &colorBlendStateCreateInfo, nullptr, &renderingCreateInfo);
    }

    Move<VkCommandPool> commandPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> commandBuffer =
        allocateCommandBuffer(vk, device, *commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBuffer cmdBuffer = *commandBuffer;

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.renderArea           = scissors[0];
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = (uint32_t)colorAttachments.size();
    renderingInfo.pColorAttachments    = colorAttachments.data();
    if (useDepthAspect)
        renderingInfo.pDepthAttachment = &depthStencilAttachment;
    if (useStencilAspect)
        renderingInfo.pStencilAttachment = &depthStencilAttachment;

    // record commands
    beginCommandBuffer(vk, cmdBuffer);

    // transition all images to proper layouts
    vk.cmdPipelineBarrier(cmdBuffer, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                          (uint32_t)colorImageBarriers.size(), colorImageBarriers.data());
    vk.cmdPipelineBarrier(cmdBuffer, 0, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                          &dsImageBarrier);

    vk.cmdBeginRendering(cmdBuffer, &renderingInfo);

    // draw using write pipelines
    for (uint32_t pipelineIndex = 0; pipelineIndex < m_inputDrawsCount; ++pipelineIndex)
    {
        renderingAttachmentLocationInfo.pColorAttachmentLocations = m_colorAttachmentLocations[pipelineIndex].data();

        vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writeGraphicsPipelines[pipelineIndex]);
        vk.cmdPushConstants(cmdBuffer, *writePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &pipelineIndex);

        if (m_useMapping)
            vk.cmdSetRenderingAttachmentLocationsKHR(cmdBuffer, &renderingAttachmentLocationInfo);
        if (useColorWriteEnable)
            vk.cmdSetColorWriteEnableEXT(cmdBuffer, 4u, m_colorWriteEnables);
        if (useUseExtendedDynamicState3)
            vk.cmdSetRasterizationSamplesEXT(cmdBuffer, VK_SAMPLE_COUNT_1_BIT);

        vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    // reuse existing barrier structures to finish rendering before next subpass
    dsImageBarrier.oldLayout     = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    dsImageBarrier.newLayout     = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    dsImageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dsImageBarrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    for (auto &barrier : colorImageBarriers)
    {
        barrier.oldLayout     = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
        barrier.newLayout     = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                          (uint32_t)colorImageBarriers.size(), colorImageBarriers.data());
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1u, &dsImageBarrier);

    // draw using read pipelines
    for (uint32_t pipelineIndex = 0; pipelineIndex < m_outputDrawsCount; ++pipelineIndex)
    {
        VkDescriptorSet descriptorSets[] = {*inputAttachmentsDescriptorSets[pipelineIndex],
                                            *bufferDescriptorSets[pipelineIndex]};
        renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices =
            de::dataOrNull(m_colorAttachmentInputIndices[pipelineIndex]);

        vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readGraphicsPipelines[pipelineIndex]);
        vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readPipelineLayout, 0u, 2u,
                                 descriptorSets, 0u, nullptr);

        if (m_useMapping)
            vk.cmdSetRenderingInputAttachmentIndicesKHR(cmdBuffer, &renderingInputAttachmentIndexInfo);

        vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    vk.cmdEndRendering(cmdBuffer);

    endCommandBuffer(vk, cmdBuffer);
    submitCommandsAndWait(vk, device, queue, cmdBuffer);

    // verify all output buffers
    bool allOk = true;
    auto &log  = m_context.getTestContext().getLog();
    DE_ASSERT(m_expectedValues.size() == m_outputDrawsCount);
    for (uint32_t buffIndex = 0; buffIndex < m_outputDrawsCount; ++buffIndex)
    {
        // get output buffer
        const Allocation &bufferAllocation = outputBuffers[buffIndex]->getAllocation();
        invalidateAlloc(vk, device, bufferAllocation);

        // validate result
        const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        const uint32_t expected   = m_expectedValues[buffIndex];
        for (uint32_t i = 0; i < m_renderSize * m_renderSize; ++i)
        {
            if (bufferPtr[i] != expected)
            {
                log << tcu::TestLog::Message << "Result for buffer " << buffIndex << ": expected " << expected
                    << " got " << bufferPtr[i] << " at index " << i << tcu::TestLog::EndMessage;
                allOk = false;
                break;
            }
        }
    }

    if (allOk)
        return tcu::TestStatus::pass("Pass");

    // if there are no color attachments then we can't log them
    if (!m_colorAttachmentCount)
        return tcu::TestStatus::fail("Fail");

    const VkBufferCreateInfo attBufferCreateInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemorySp attBuffer(
        new BufferWithMemory(vk, device, memAlloc, attBufferCreateInfo, MemoryRequirement::HostVisible));
    vk::VkBufferImageCopy region{0, 0, 0, {1, 0, 0, 1}, {0, 0, 0}, {1, 1, 1}};
    auto &barrier = colorImageBarriers[0];

    // reuse first barrier structure
    barrier.oldLayout     = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    // log content of each color attachment
    for (uint32_t attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
    {
        barrier.image = **images[attIndex];
        commandBuffer = allocateCommandBuffer(vk, device, *commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        cmdBuffer     = *commandBuffer;

        beginCommandBuffer(vk, cmdBuffer);
        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0,
                              0, 1, &barrier);
        vk.cmdCopyImageToBuffer(cmdBuffer, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **attBuffer, 1,
                                &region);
        endCommandBuffer(vk, cmdBuffer);

        submitCommandsAndWait(vk, device, queue, cmdBuffer);

        const Allocation &bufferAllocation = attBuffer->getAllocation();
        invalidateAlloc(vk, device, bufferAllocation);
        const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        log << tcu::TestLog::Message << "Color attachment " << attIndex << " contains: " << bufferPtr[0]
            << tcu::TestLog::EndMessage;
    }

    return tcu::TestStatus::fail("Fail");
}

class MappingWithBlendStateTestInstance : public vkt::TestInstance
{
public:
    MappingWithBlendStateTestInstance(Context &context, const TestType testType);
    ~MappingWithBlendStateTestInstance() = default;

protected:
    tcu::TestStatus iterate(void) override;

private:
    const uint32_t m_renderSize;
};

MappingWithBlendStateTestInstance::MappingWithBlendStateTestInstance(Context &context, const TestType testType)
    : vkt::TestInstance(context)
    , m_renderSize(16)
{
    DE_UNREF(testType);
}

tcu::TestStatus MappingWithBlendStateTestInstance::iterate()
{
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    Allocator &memAlloc                    = m_context.getDefaultAllocator();
    VkQueue queue                          = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex        = m_context.getUniversalQueueFamilyIndex();
    const VkFormat imageFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::TextureFormat textureFormat = mapVkFormat(imageFormat);
    const uint32_t colorAttachmentCount    = 4u;

    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const VkBufferImageCopy copyRegion     = makeBufferImageCopy({m_renderSize, m_renderSize, 1u}, colorSL);

    Move<VkCommandPool> commandPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> commandBuffer =
        allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBuffer cmdBuffer = *commandBuffer;

    const std::vector<VkViewport> viewports{makeViewport(m_renderSize, m_renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize, m_renderSize)};

    const tcu::Vec4 clearValues[] // blend src color
        {
            {0.5f, 0.5f, 0.4f, 1.0f},
            {0.2f, 0.4f, 0.3f, 0.1f},
            {0.2f, 1.0f, 0.5f, 0.8f},
            {1.0f, 0.1f, 0.3f, 0.5f},
        };
    DE_ASSERT(DE_LENGTH_OF_ARRAY(clearValues) == colorAttachmentCount);

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[]{
        {1, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA,
         VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MAX, 0xf},
        {1, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_OP_SUBTRACT,
         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_ADD, 0xf},
        {1, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_MIN, VK_BLEND_FACTOR_SRC_ALPHA,
         VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MAX, 0xf},
        {1, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_OP_MAX, VK_BLEND_FACTOR_SRC_ALPHA,
         VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MIN, 0xf},
    };
    DE_ASSERT(DE_LENGTH_OF_ARRAY(colorBlendAttachmentStates) == colorAttachmentCount);

    const uint32_t colorAttachmentLocations[]{3, 0, 2, 1};
    DE_ASSERT(DE_LENGTH_OF_ARRAY(colorAttachmentLocations) == colorAttachmentCount);

    VkRenderingAttachmentInfo colorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,           // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        VK_NULL_HANDLE,                                        // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,              // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                                  // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                        // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                             // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                           // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                          // VkAttachmentStoreOp storeOp;
        makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) // VkClearValue clearValue;
    };
    VkImageMemoryBarrier imageMemoryBarrier =
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_NULL_HANDLE, colorSRR);
    const VkDeviceSize outputBufferSize = (VkDeviceSize)m_renderSize * m_renderSize * tcu::getPixelSize(textureFormat);
    const VkBufferCreateInfo outputBufferInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // construct required containers
    std::vector<ImageWithMemorySp> images(colorAttachmentCount, ImageWithMemorySp());
    std::vector<VkImageViewSp> imageViews(colorAttachmentCount, VkImageViewSp());
    std::vector<VkFormat> colorImageFormats(colorAttachmentCount, imageFormat);
    std::vector<VkRenderingAttachmentInfo> colorAttachments(colorAttachmentCount, colorAttachment);
    std::vector<VkImageMemoryBarrier> colorImageBarriers(colorAttachmentCount, imageMemoryBarrier);
    std::vector<BufferWithMemorySp> outputBuffers(colorAttachmentCount, BufferWithMemorySp());

    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
    {
        // create images and image views for input attachments
        images[i]     = createImage(m_context, m_renderSize, imageFormat,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        imageViews[i] = VkImageViewSp(new vk::Move<VkImageView>(
            makeImageView(vk, device, **images[i], VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR)));

        colorAttachments[i].imageView  = **imageViews[i];
        colorAttachments[i].clearValue = makeClearValueColor(clearValues[i]);
        colorImageBarriers[i].image    = **images[i];

        // create output buffers that will be used to get attachments data
        outputBuffers[i] = BufferWithMemorySp(
            new BufferWithMemory(vk, device, memAlloc, outputBufferInfo, MemoryRequirement::HostVisible));
    }

    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, VK_NULL_HANDLE);
    Move<VkShaderModule> vertShaderModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
    Move<VkShaderModule> fragShaderModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

    // define empty VertexInputState, full screen quad will be generated in vertex shader
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    // define ColorBlendState so that we can write to multiple color attachments
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = colorAttachmentCount;
    colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates;

    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocations{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR, nullptr, colorAttachmentCount,
        colorAttachmentLocations};
    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                      &renderingAttachmentLocations,
                                                      0u,
                                                      colorAttachmentCount,
                                                      colorImageFormats.data(),
                                                      VK_FORMAT_UNDEFINED,
                                                      VK_FORMAT_UNDEFINED};

    Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        *fragShaderModule, VK_NULL_HANDLE, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
        &vertexInputState, nullptr, nullptr, nullptr, &colorBlendStateCreateInfo, nullptr, &renderingCreateInfo);

    VkRenderingInfo renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0u,                      // VkRenderingFlags                        flags
        scissors[0],             // VkRect2D                                renderArea
        1u,                      // uint32_t                                layerCount
        0u,                      // uint32_t                                viewMask
        colorAttachmentCount,    // uint32_t                                colorAttachmentCount
        colorAttachments.data(), // const VkRenderingAttachmentInfo*        pColorAttachments
        nullptr,                 // const VkRenderingAttachmentInfo*        pDepthAttachment
        nullptr                  // const VkRenderingAttachmentInfo*        pStencilAttachment
    };

    // record primary command buffer
    beginCommandBuffer(vk, cmdBuffer);

    // transfer layout to color attachment optimal
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, 0u,
                          0u, 0u, colorAttachmentCount, colorImageBarriers.data());

    vk.cmdBeginRendering(cmdBuffer, &renderingInfo);

    // remaping should affect to which attachments shader writes but not blend state
    vk.cmdSetRenderingAttachmentLocationsKHR(cmdBuffer, &renderingAttachmentLocations);

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    vk.cmdEndRendering(cmdBuffer);

    // transition colorbuffers layout to transfer source optimal
    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
    {
        colorImageBarriers[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorImageBarriers[i].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        colorImageBarriers[i].oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorImageBarriers[i].newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, 0u, 0u, 0u, colorAttachmentCount, colorImageBarriers.data());

    // read back color images
    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        vk.cmdCopyImageToBuffer(cmdBuffer, **images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **outputBuffers[i], 1u,
                                &copyRegion);

    endCommandBuffer(vk, cmdBuffer);
    submitCommandsAndWait(vk, device, queue, cmdBuffer);

    const tcu::Vec4 expectedLeftSideColors[]{
        {0.50f, 0.98f, 0.72f, 1.00f},
        {0.42f, 0.44f, 0.63f, 0.17f},
        {0.10f, 0.30f, 0.50f, 0.80f},
        {1.00f, 0.40f, 0.30f, 0.50f},
    };
    DE_ASSERT(DE_LENGTH_OF_ARRAY(expectedLeftSideColors) == colorAttachmentCount);

    bool testPassed(true);
    const tcu::Vec4 colorPrecision(0.05f);
    auto &log(m_context.getTestContext().getLog());

    // verify result
    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
    {
        bool colorIsCorrect = true;
        auto &allocation    = outputBuffers[i]->getAllocation();
        invalidateAlloc(vk, device, allocation);

        tcu::ConstPixelBufferAccess outputAccess(textureFormat, m_renderSize, m_renderSize, 1u,
                                                 allocation.getHostPtr());

        // check one fragment from the left side of image (after blending)
        tcu::Vec4 fragmentColorOnLeftSide = outputAccess.getPixel(i, i, 0);
        if (tcu::boolAny(
                tcu::greaterThan(tcu::absDiff(fragmentColorOnLeftSide, expectedLeftSideColors[i]), colorPrecision)))
            colorIsCorrect = false;

        // check one fragment from the right side of image (clear color should be there)
        tcu::Vec4 fragmentColorOnRightSide(outputAccess.getPixel(m_renderSize - 1 - i, m_renderSize - 1 - i, 0));
        if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(fragmentColorOnRightSide, clearValues[i]), colorPrecision)))
            colorIsCorrect = false;

        if (!colorIsCorrect)
        {
            std::string imageName = std::string("Attachment") + std::to_string(i);
            log << tcu::TestLog::Image(imageName, imageName, outputAccess);
            testPassed = false;
        }
    }

    if (testPassed)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

class MappingWithGraphicsPipelineLibraryTestInstance : public vkt::TestInstance
{
public:
    MappingWithGraphicsPipelineLibraryTestInstance(Context &context, const TestType testType);
    ~MappingWithGraphicsPipelineLibraryTestInstance() = default;

protected:
    tcu::TestStatus iterate(void) override;
};

MappingWithGraphicsPipelineLibraryTestInstance::MappingWithGraphicsPipelineLibraryTestInstance(Context &context,
                                                                                               const TestType testType)
    : vkt::TestInstance(context)
{
    DE_UNREF(testType);
}

tcu::TestStatus MappingWithGraphicsPipelineLibraryTestInstance::iterate()
{
    // test using GPL and only providing valid formats in VkPipelineRenderingCreateInfo to the fragment output interface

    const auto &vk       = m_context.getDeviceInterface();
    const auto device    = m_context.getDevice();
    Allocator &allocator = m_context.getDefaultAllocator();

    const auto imageSize   = 8u;
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto inputFormat = VK_FORMAT_B8G8R8A8_UNORM;
    const auto extent      = makeExtent3D(imageSize, imageSize, 1u);
    const auto isrr        = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    const uint32_t inputsCount = 3;
    VkClearColorValue inputColors[inputsCount]{
        {{0.0f, 0.0f, 0.0f, 1.0}}, {{1.0f, 0.0f, 0.0f, 0.0}}, {{1.0f, 1.0f, 0.0f, 0.0}}};

    // create color attachment
    const VkImageUsageFlags colorImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ImageWithBuffer colorImageWithBuffer(vk, device, allocator, extent, colorFormat, colorImageUsage, VK_IMAGE_TYPE_2D,
                                         isrr, 1u);
    auto colorImageView =
        makeImageView(vk, device, colorImageWithBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, isrr);

    // create input attachments
    VkImageCreateInfo inputImageCreateInfo = initVulkanStructure();
    inputImageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    inputImageCreateInfo.format            = inputFormat;
    inputImageCreateInfo.extent            = extent;
    inputImageCreateInfo.mipLevels         = 1u;
    inputImageCreateInfo.arrayLayers       = 1u;
    inputImageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    inputImageCreateInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ImageWithMemory inputImagesWithMemory[]{{vk, device, allocator, inputImageCreateInfo, MemoryRequirement::Any},
                                            {vk, device, allocator, inputImageCreateInfo, MemoryRequirement::Any},
                                            {vk, device, allocator, inputImageCreateInfo, MemoryRequirement::Any}};
    Move<VkImageView> inputImageViews[]{
        makeImageView(vk, device, *inputImagesWithMemory[0], VK_IMAGE_VIEW_TYPE_2D, inputFormat, isrr),
        makeImageView(vk, device, *inputImagesWithMemory[1], VK_IMAGE_VIEW_TYPE_2D, inputFormat, isrr),
        makeImageView(vk, device, *inputImagesWithMemory[2], VK_IMAGE_VIEW_TYPE_2D, inputFormat, isrr),
    };

    // setup descriptor set
    const auto descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(descriptorType, inputsCount)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    DescriptorSetLayoutBuilder descriptorSetLayoutBuilder;
    for (uint32_t i = 0; i < inputsCount; ++i)
        descriptorSetLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT);
    Move<VkDescriptorSetLayout> descriptorSetLayout = descriptorSetLayoutBuilder.build(vk, device);
    Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    auto di = makeDescriptorImageInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    for (uint32_t i = 0; i < inputsCount; ++i)
    {
        di.imageView = *inputImageViews[i];
        descriptorSetUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i),
                                               descriptorType, &di);
    }
    descriptorSetUpdateBuilder.update(vk, device);

    const auto pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    // fill structures that are needed for pipeline creation
    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo   = initVulkanStructure();
    inputAssemblyStateInfo.topology                                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = initVulkanStructure();
    const auto viewport                                                 = makeViewport(extent);
    const auto scissor                                                  = makeRect2D(extent);
    VkPipelineViewportStateCreateInfo viewportStateInfo                 = initVulkanStructure();
    viewportStateInfo.viewportCount                                     = 1u;
    viewportStateInfo.pViewports                                        = &viewport;
    viewportStateInfo.scissorCount                                      = 1u;
    viewportStateInfo.pScissors                                         = &scissor;

    VkPipelineMultisampleStateCreateInfo multisampleStateInfo         = initVulkanStructure();
    multisampleStateInfo.rasterizationSamples                         = VK_SAMPLE_COUNT_1_BIT;
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();
    const VkPipelineTessellationStateCreateInfo tessellationStateInfo = initVulkanStructure();

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(1 + inputsCount,
                                                                                colorBlendAttachmentState);
    // render to last attachment
    colorBlendAttachmentStates[inputsCount].colorWriteMask = 0xFu;

    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = initVulkanStructure();
    colorBlendStateInfo.attachmentCount                     = 1 + inputsCount;
    colorBlendStateInfo.pAttachments                        = colorBlendAttachmentStates.data();

    auto &bc(m_context.getBinaryCollection());
    const auto vertModule(createShaderModule(vk, device, bc.get("vert")));
    const auto fragModule(createShaderModule(vk, device, bc.get("frag")));
    VkPipelineShaderStageCreateInfo stages[]{
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule),
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule)};

    // provide 0 formats in VkPipelineRenderingCreateInfo for shader stages in the pipeline libraries
    const std::vector<VkFormat> undefinedColorFormats(1 + inputsCount, VK_FORMAT_UNDEFINED);
    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    renderingCreateInfo.colorAttachmentCount          = 1 + inputsCount;
    renderingCreateInfo.pColorAttachmentFormats       = undefinedColorFormats.data();

    // create all shader pipeline part
    VkGraphicsPipelineLibraryCreateInfoEXT shdersLibraryInfo = initVulkanStructure(&renderingCreateInfo);
    shdersLibraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                              VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
                              VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
    VkGraphicsPipelineCreateInfo shadersPipelineInfo = initVulkanStructure(&shdersLibraryInfo);
    shadersPipelineInfo.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    shadersPipelineInfo.layout                       = *pipelineLayout;
    shadersPipelineInfo.pVertexInputState            = &vertexInputStateInfo;
    shadersPipelineInfo.pInputAssemblyState          = &inputAssemblyStateInfo;
    shadersPipelineInfo.pViewportState               = &viewportStateInfo;
    shadersPipelineInfo.pRasterizationState          = &rasterizationStateInfo;
    shadersPipelineInfo.pTessellationState           = &tessellationStateInfo;
    shadersPipelineInfo.pMultisampleState            = &multisampleStateInfo;
    shadersPipelineInfo.pDepthStencilState           = &depthStencilStateInfo;
    shadersPipelineInfo.pColorBlendState             = &colorBlendStateInfo;
    shadersPipelineInfo.stageCount                   = 2u;
    shadersPipelineInfo.pStages                      = stages;
    auto shadersPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &shadersPipelineInfo);

    // provide valid formats in VkPipelineRenderingCreateInfo for fragment output library
    std::vector<VkFormat> attachmentFormats(1 + inputsCount, inputFormat);
    attachmentFormats[inputsCount]              = colorFormat;
    renderingCreateInfo.pColorAttachmentFormats = attachmentFormats.data();

    // create fragment output part
    VkGraphicsPipelineLibraryCreateInfoEXT fragOutLibInfo = initVulkanStructure(&renderingCreateInfo);
    fragOutLibInfo.flags                             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    VkGraphicsPipelineCreateInfo fragOutPipelineInfo = initVulkanStructure(&fragOutLibInfo);
    fragOutPipelineInfo.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    fragOutPipelineInfo.layout                       = *pipelineLayout;
    fragOutPipelineInfo.pMultisampleState            = &multisampleStateInfo;
    fragOutPipelineInfo.pColorBlendState             = &colorBlendStateInfo;
    auto fragmentOutPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &fragOutPipelineInfo);

    // merge pipelines
    const VkPipeline libraryHandles[]{
        *shadersPipelinePart,
        *fragmentOutPipelinePart,
    };
    VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
    linkedPipelineLibraryInfo.libraryCount                   = 2u;
    linkedPipelineLibraryInfo.pLibraries                     = libraryHandles;
    VkGraphicsPipelineCreateInfo linkedPipelineInfo          = initVulkanStructure(&linkedPipelineLibraryInfo);
    linkedPipelineInfo.layout                                = *pipelineLayout;
    const auto pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &linkedPipelineInfo);

    const auto initialColorBarrier(makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                                          colorImageWithBuffer.getImage(), isrr));
    std::vector<VkImageMemoryBarrier> initialInputBarriers(inputsCount, initialColorBarrier);
    for (uint32_t i = 0; i < inputsCount; ++i)
    {
        auto &b         = initialInputBarriers[i];
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.image         = *inputImagesWithMemory[i];
    }
    const auto afterClearBarrier(makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT));
    const auto beforeCopyBarrier(makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT));

    const auto srl(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    const auto copyRegion(makeBufferImageCopy(extent, srl));

    VkRenderingAttachmentInfo colorAttachment = initVulkanStructure();
    colorAttachment.imageView                 = *colorImageView;
    colorAttachment.imageLayout               = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.clearValue                = makeClearValueColorF32(0.0f, 0.0f, 1.0f, 0.0f);

    std::vector<VkRenderingAttachmentInfo> attachments(1 + inputsCount, colorAttachment);
    for (uint32_t i = 0; i < inputsCount; ++i)
    {
        auto &input     = attachments[i];
        input.loadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
        input.imageView = *inputImageViews[i];
    }

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.renderArea           = scissor;
    renderingInfo.layerCount           = 1u;
    renderingInfo.colorAttachmentCount = (uint32_t)attachments.size();
    renderingInfo.pColorAttachments    = attachments.data();

    uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    // transition layout of image used as color attachment
    vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, 0, 0, 0, 1,
                          &initialColorBarrier);

    // transition layout of images used as input attachments
    vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, 0, 0, 0, inputsCount,
                          initialInputBarriers.data());

    // clear input attachments
    for (uint32_t i = 0; i < inputsCount; ++i)
        vk.cmdClearColorImage(*cmdBuffer, *inputImagesWithMemory[i], VK_IMAGE_LAYOUT_GENERAL, &inputColors[i], 1u,
                              &isrr);

    // wait for clears before loading inputs
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u, 1u,
                          &afterClearBarrier, 0, 0, 0, 0);

    // render full screen quad
    vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1, &*descriptorSet, 0u,
                             nullptr);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    vk.cmdEndRendering(*cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1,
                          &beforeCopyBarrier, 0, 0, 0, 0);
    vk.cmdCopyImageToBuffer(*cmdBuffer, colorImageWithBuffer.getImage(), VK_IMAGE_LAYOUT_GENERAL,
                            colorImageWithBuffer.getBuffer(), 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);
    const auto &bufferAllocation = colorImageWithBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    // check just one fragment - at location (1,1) - expect color (1,1,0,1) while clear color was (0,0,1,0)
    uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr());
    uint8_t *fragment  = bufferPtr + 4 * imageSize + 4;
    if ((fragment[0] > 253) && (fragment[1] > 253) && (fragment[2] < 2) && (fragment[3] > 253))
        return tcu::TestStatus::pass("Pass");

    tcu::PixelBufferAccess resultAccess(mapVkFormat(colorFormat), imageSize, imageSize, 1, bufferPtr);
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "")
                                        << tcu::TestLog::Image("Output", "", resultAccess) << tcu::TestLog::EndImageSet;

    return tcu::TestStatus::fail("Fail");
}

class MappingWithShaderObjectTestInstance : public vkt::TestInstance
{
public:
    MappingWithShaderObjectTestInstance(Context &context, const TestType testType);
    ~MappingWithShaderObjectTestInstance() = default;

protected:
    tcu::TestStatus iterate(void) override;
};

MappingWithShaderObjectTestInstance::MappingWithShaderObjectTestInstance(Context &context, const TestType testType)
    : vkt::TestInstance(context)
{
    DE_UNREF(testType);
}

tcu::TestStatus MappingWithShaderObjectTestInstance::iterate()
{
    const auto &vki             = m_context.getInstanceInterface();
    const auto &vk              = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    const auto physicalDevice   = m_context.getPhysicalDevice();
    const auto deviceExtensions = m_context.getDeviceExtensions();
    Allocator &allocator        = m_context.getDefaultAllocator();

    const auto imageSize(8u);
    const auto imageCount(3u);
    const auto colorFormat(VK_FORMAT_R8G8B8A8_UNORM);
    const auto drawWidth(imageSize / 4);
    const auto extent(makeExtent3D(imageSize, imageSize, 1u));
    const auto srl(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, imageCount));
    const auto copyRegion(makeBufferImageCopy(extent, srl));
    const auto isrrFull(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, imageCount));

    // create color attachment with four layers
    const VkImageUsageFlags colorImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ImageWithBuffer imageWithBuffer(vk, device, allocator, extent, colorFormat, colorImageUsage, VK_IMAGE_TYPE_2D,
                                    isrrFull, 4u);

    VkImageSubresourceRange isrr[imageCount];
    Move<VkImageView> imageViews[imageCount];
    for (uint32_t i = 0u; i < imageCount; i++)
    {
        isrr[i] = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, i, 1u);
        imageViews[i] =
            makeImageView(vk, device, imageWithBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, isrr[i]);
    }

    uint32_t colorAttachmentLocations[][3]{{2, 0, 1}, {0, 1, 2}, {1, 2, 0}};
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocations = initVulkanStructure();
    renderingAttachmentLocations.colorAttachmentCount                 = imageCount;
    renderingAttachmentLocations.pColorAttachmentLocations            = colorAttachmentLocations[0];

    // use GraphicsPipelineWrapper for shader object
    PipelineConstructionType pipelineType = PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY;
    const std::vector<VkViewport> viewport{makeViewport(imageSize, imageSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(drawWidth, 0, drawWidth, imageSize)};

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    VkPipelineColorBlendAttachmentState colorBlendState;
    deMemset(&colorBlendState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendState.colorWriteMask = 0xf;
    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(imageCount, colorBlendState);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = imageCount;
    colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates.data();

    const std::vector<VkFormat> colorFormats(imageCount, colorFormat);
    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    renderingCreateInfo.colorAttachmentCount          = imageCount;
    renderingCreateInfo.pColorAttachmentFormats       = colorFormats.data();

    auto &bc(m_context.getBinaryCollection());
    ShaderWrapper vertShader(vk, device, bc.get("vert"));
    ShaderWrapper fragShader(vk, device, bc.get("frag"));
    PipelineLayoutWrapper pipelineLayout(pipelineType, vk, device);

    GraphicsPipelineWrapper pipelineWrapper(vki, vk, physicalDevice, device, deviceExtensions, pipelineType);
    pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setMonolithicPipelineLayout(pipelineLayout)
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, VK_NULL_HANDLE, 0u, vertShader, 0, {}, {},
                                          {}, 0, nullptr, &renderingCreateInfo)
        .setupFragmentShaderState(pipelineLayout, VK_NULL_HANDLE, 0u, fragShader)
        .setupFragmentOutputState(VK_NULL_HANDLE, 0u, &colorBlendStateCreateInfo)
        .buildPipeline();

    const auto initialColorBarrier(makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                                          imageWithBuffer.getImage(), isrrFull));
    const auto beforeCopyBarrier(makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT));

    VkRenderingAttachmentInfo colorAttachment = initVulkanStructure();
    colorAttachment.imageLayout               = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.clearValue                = makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f);

    std::vector<VkRenderingAttachmentInfo> attachments(imageCount, colorAttachment);
    for (uint32_t i = 0; i < imageCount; ++i)
        attachments[i].imageView = *imageViews[i];

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.renderArea           = makeRect2D(imageSize, imageSize);
    renderingInfo.layerCount           = 1u;
    renderingInfo.colorAttachmentCount = (uint32_t)attachments.size();
    renderingInfo.pColorAttachments    = attachments.data();

    uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    // transition layout of image to general
    vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, 0, 0, 0, 1,
                          &initialColorBarrier);

    // render fullscreen quad three times but use scissor to limit it to fragment of framebuffer
    vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
    pipelineWrapper.bind(*cmdBuffer);

    vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocations);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    VkRect2D localScissor(makeRect2D(2 * drawWidth, 0, drawWidth, imageSize));
    renderingAttachmentLocations.pColorAttachmentLocations = colorAttachmentLocations[1];
    vk.cmdSetScissorWithCount(*cmdBuffer, 1u, &localScissor);
    vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocations);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    localScissor.offset.x += drawWidth;
    renderingAttachmentLocations.pColorAttachmentLocations = colorAttachmentLocations[2];
    vk.cmdSetScissorWithCount(*cmdBuffer, 1u, &localScissor);
    vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocations);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    vk.cmdEndRendering(*cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1,
                          &beforeCopyBarrier, 0, 0, 0, 0);
    vk.cmdCopyImageToBuffer(*cmdBuffer, imageWithBuffer.getImage(), VK_IMAGE_LAYOUT_GENERAL,
                            imageWithBuffer.getBuffer(), 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);
    const auto &bufferAllocation = imageWithBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    // verify attachments
    bool testPassed = true;
    uint8_t colors[][4]{
        {255, 255, 255, 255}, // white, clear color, used always in first column
        {255, 0, 255, 255},   // pink, order of this and following colors is same as in shader
        {255, 255, 0, 255},   // yellow
        {0, 255, 255, 255}    // teal
    };
    uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr());
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
    {
        uint8_t *fragment = bufferPtr + imageIndex * imageSize * imageSize * 4;

        // check just first row from each image
        for (uint32_t fragmentIndex = 0; fragmentIndex < imageSize; ++fragmentIndex)
        {
            uint32_t quarterIndex = fragmentIndex / drawWidth;
            uint32_t colorIndex   = 0;
            if (quarterIndex > 0)
                colorIndex = colorAttachmentLocations[imageIndex][quarterIndex - 1] + 1;

            testPassed = (deMemCmp(fragment, colors[colorIndex], 4) == 0);
            if (!testPassed)
                break;

            // move to next fragment
            fragment += 4;
        }
        if (!testPassed)
            break;
    }

    if (testPassed)
        return tcu::TestStatus::pass("Pass");

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::ImageSet("Result", "");
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        tcu::PixelBufferAccess resultAccess(mapVkFormat(colorFormat), imageSize, imageSize, 1,
                                            bufferPtr + i * 4 * imageSize * imageSize);
        log << tcu::TestLog::Image("Image " + std::to_string(i), "", resultAccess);
    }
    log << tcu::TestLog::EndImageSet;

    return tcu::TestStatus::fail("Fail");
}

class FeedbackLoopTestInstance : public vkt::TestInstance
{
public:
    FeedbackLoopTestInstance(Context &context, const TestType testType);
    ~FeedbackLoopTestInstance() = default;

protected:
    tcu::TestStatus iterate(void) override;
};

FeedbackLoopTestInstance::FeedbackLoopTestInstance(Context &context, const TestType testType)
    : vkt::TestInstance(context)
{
    DE_UNREF(testType);
}

tcu::TestStatus FeedbackLoopTestInstance::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    Allocator &alloc                = m_context.getDefaultAllocator();
    VkQueue queue                   = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkFormat imageFormat      = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t imageSize        = 1024; // 1024 size is required to trigger delta color compression
    const auto extent               = makeExtent3D(imageSize, imageSize, 1u);

    const VkImageSubresourceRange srr = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceLayers sl = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const std::vector<VkBufferImageCopy> copyRegions(1u, makeBufferImageCopy(extent, sl));

    // generate noize that will be used as input attachment
    de::Random rnd(1234);
    std::vector<uint8_t> proceduralData(imageSize * imageSize * 4);
    for (auto &pd : proceduralData)
    {
        pd = rnd.getUint8();
        // make sure there is no 0.5 in shader so we have easier verification;
        // if v is too close to 127 then just substract 50 from it
        pd = uint8_t(pd - 50 * ((pd > 125) && (pd < 129)));
    }

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = imageFormat;
    imageCreateInfo.extent            = extent;
    imageCreateInfo.mipLevels         = 1u;
    imageCreateInfo.arrayLayers       = 1u;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageWithMemory imageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    auto imageView = makeImageView(vk, device, *imageWithMemory, VK_IMAGE_VIEW_TYPE_2D, imageFormat, srr);

    const auto bufferSize = imageSize * imageSize * 4;
    const auto bufferCreateInfo =
        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory bufferWithMemory(vk, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);
    auto &allocation = bufferWithMemory.getAllocation();
    void *inputData  = allocation.getHostPtr();
    deMemcpy(inputData, proceduralData.data(), de::dataSize(proceduralData));
    flushAlloc(vk, device, allocation);

    auto commandPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    auto commandBuffer = allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer     = *commandBuffer;

    const std::vector<VkViewport> viewports{makeViewport(imageSize, imageSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(imageSize, imageSize)};

    // setup descriptor set with single input attachment
    const auto descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    auto descriptorPool       = DescriptorPoolBuilder()
                              .addType(descriptorType, 1)
                              .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    auto descriptorSetLayout =
        DescriptorSetLayoutBuilder().addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT).build(vk, device);
    auto descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
    auto di            = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), descriptorType, &di)
        .update(vk, device);

    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    auto &bc                              = m_context.getBinaryCollection();
    Move<VkShaderModule> vertModule       = createShaderModule(vk, device, bc.get("vert"));
    Move<VkShaderModule> fragModule       = createShaderModule(vk, device, bc.get("frag"));

    // define empty VertexInputState, full screen quad will be generated in vertex shader
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask                      = 0xf;
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = 1;
    colorBlendStateCreateInfo.pAttachments                        = &colorBlendAttachmentState;

    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    renderingCreateInfo.colorAttachmentCount          = 1;
    renderingCreateInfo.pColorAttachmentFormats       = &imageFormat;

    Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        VK_NULL_HANDLE, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0, &vertexInputState, nullptr,
        nullptr, nullptr, &colorBlendStateCreateInfo, nullptr, &renderingCreateInfo);

    const auto selfDependencyBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    // record primary command buffer
    beginCommandBuffer(vk, cmdBuffer);

    copyBufferToImage(vk, cmdBuffer, *bufferWithMemory, bufferSize, copyRegions, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                      *imageWithMemory, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    beginRendering(vk, cmdBuffer, *imageView, scissors[0], {}, VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_LOAD);

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);

    // before writing to color attachment wait for input attachment read
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u,
                          &selfDependencyBarrier, 0u, 0, 0u, 0);

    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    endRendering(vk, cmdBuffer);

    copyImageToBuffer(vk, cmdBuffer, *imageWithMemory, *bufferWithMemory, tcu::IVec2(imageSize),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vk, cmdBuffer);
    submitCommandsAndWait(vk, device, queue, cmdBuffer);

    // verify result by repeating logic from the shader;
    // in shader we add/substract 51 but in verification precision is not that important so we check range (40; 60)
    uint32_t failureCount = 0;
    uint8_t *bufferPtr    = static_cast<uint8_t *>(bufferWithMemory.getAllocation().getHostPtr());
    for (uint32_t fragmentIndex = 0; fragmentIndex < proceduralData.size(); ++fragmentIndex)
    {
        uint8_t input  = proceduralData.at(fragmentIndex);
        uint8_t output = *(bufferPtr + fragmentIndex);
        if (input > 127)
            failureCount += ((output < input - 60) || (output > input - 40));
        else
            failureCount += ((output < input + 40) || (output > input + 60));
    }

    if (failureCount == 0)
        return tcu::TestStatus::pass("Pass");

    tcu::PixelBufferAccess resultAccess(mapVkFormat(imageFormat), imageSize, imageSize, 1, bufferPtr);
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "")
                                        << tcu::TestLog::Image("Output", "", resultAccess) << tcu::TestLog::EndImageSet;

    std::string failString =
        std::string("Fail (" + std::to_string(100.0f * float(failureCount) / imageSize / imageSize) +
                    "% of fragments had wrong value)");
    return tcu::TestStatus::fail(failString);
}

class LocalReadTestCase : public vkt::TestCase
{
public:
    LocalReadTestCase(tcu::TestContext &context, const std::string &name, TestType testType);
    virtual ~LocalReadTestCase(void) = default;

protected:
    void checkSupport(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    TestType m_testType;
};

LocalReadTestCase::LocalReadTestCase(tcu::TestContext &context, const std::string &name, TestType testType)
    : vkt::TestCase(context, name)
    , m_testType(testType)
{
}

void LocalReadTestCase::checkSupport(Context &context) const
{
    const InstanceInterface &vki    = context.getInstanceInterface();
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");

    if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX)
    {
        const auto &limits = getPhysicalDeviceProperties(vki, physicalDevice).limits;
        if (limits.maxPerStageDescriptorInputAttachments < 21)
            TCU_THROW(NotSupportedError, "maxPerStageDescriptorInputAttachments is too small");
    }
    else if ((m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR) ||
             (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR))
    {
        VkImageFormatProperties imageFormatProperties;
        VkFormat format = VK_FORMAT_D32_SFLOAT;
        if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR)
            format = VK_FORMAT_S8_UINT;
        if (vki.getPhysicalDeviceImageFormatProperties(
                physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 0,
                &imageFormatProperties))
            TCU_THROW(NotSupportedError, "Depth/stencil format not supported");
    }
    else if (m_testType == TestType::INTERACTION_WITH_COLOR_WRITE_ENABLE)
        context.requireDeviceFunctionality("VK_EXT_color_write_enable");
    else if (m_testType == TestType::INTERACTION_WITH_GRAPHICS_PIPELINE_LIBRARY)
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");
    else if (m_testType == TestType::INTERACTION_WITH_SHADER_OBJECT)
        context.requireDeviceFunctionality("VK_EXT_shader_object");
    else if (m_testType == TestType::INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3)
    {
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state3");
        if (!context.getExtendedDynamicState3FeaturesEXT().extendedDynamicState3RasterizationSamples)
            TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
    }
}

void LocalReadTestCase::initPrograms(SourceCollections &programCollection) const
{
    // vertex shader generates fullscreen quad
    std::string vertSrc("#version 450\n"
                        "void main (void)\n"
                        "{\n"
                        "  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
                        "  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
                        "  gl_Position = vec4(x, y, 0.6, 1.0);\n"
                        "}\n");
    auto &glslSources = programCollection.glslSources;
    glslSources.add("vert") << glu::VertexSource(vertSrc);

    // helper lambda that generates fragment shader that writes to specified number of color attachments
    auto generateWriteFragSource = [](uint32_t colorAttachmentCount)
    {
        std::stringstream fragSrc;
        fragSrc << "#version 450\n"
                   "layout(push_constant) uniform InputDraw {\n"
                   "  uint count;\n"
                   "} inputDraw;\n";
        for (uint32_t i = 0; i < colorAttachmentCount; ++i)
            fragSrc << "layout(location=" << i << ") out uint outColor" << i << ";\n";
        fragSrc << "void main()\n{\n";
        for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        {
            fragSrc << "  outColor" << i << " = 2u * inputDraw.count + " << i + 1 << ";\n";
            fragSrc << "  outColor" << i << " *= "
                    << "outColor" << i << ";\n";
        }
        fragSrc << "}\n";
        return fragSrc.str();
    };

    // helper lambda that generates fragment shader that reads from specified number of input attachments
    auto generateReadFragSource = [](uint32_t inputAttachmentCount, bool useStencil = true)
    {
        uint32_t colorInputAttachmentCount = inputAttachmentCount - 1u - useStencil;
        std::stringstream fragSrc;
        fragSrc << "#version 450\n";
        for (uint32_t i = 0; i < colorInputAttachmentCount; ++i)
            fragSrc << "layout(input_attachment_index=" << i << ", binding=" << i << ") uniform usubpassInput inColor"
                    << i << ";\n";

        fragSrc << "layout(input_attachment_index = " << colorInputAttachmentCount
                << ", binding = " << colorInputAttachmentCount << ") uniform subpassInput inDepth;\n";
        if (useStencil)
            fragSrc << "layout(input_attachment_index = " << colorInputAttachmentCount + 1
                    << ", binding = " << colorInputAttachmentCount + 1 << ") uniform usubpassInput inStencil;\n";

        fragSrc << "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
                   "  uint v[];\n"
                   "} outBuffer;\n"
                   "void main()\n{\n"
                   "  uint result = 0;\n";
        for (uint32_t i = 0; i < colorInputAttachmentCount; ++i)
            fragSrc << "  result = result + " << (i + 1) << " * subpassLoad(inColor" << i << ").x; \n";
        fragSrc << "  result = result + uint(subpassLoad(inDepth).x * 1000);\n"; // 0.6*1000
        if (useStencil)
            fragSrc << "  result = result + uint(subpassLoad(inStencil).x * 1000);\n"; // 1 * 1000
        fragSrc << "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
                   "  outBuffer.v[i.x+i.y*16] = result;\n"
                   "}\n";
        return fragSrc.str();
    };

    if (m_testType == TestType::MAX_INPUT_ATTACHMENTS)
    {
        // generate fragment shader for maximal number of input attachments;
        // during test execution proper shader will be picked
        for (uint32_t inputAttachmentCount : inputAttachmentsPossibleValues)
        {
            // subtract 2 because we will write to depth and stencil and those
            // attachments will later consume 2 input attachment slots
            uint32_t colorAttachmentCount = inputAttachmentCount - 2u;
            glslSources.add(std::string("frag0_") + std::to_string(colorAttachmentCount))
                << glu::FragmentSource(generateWriteFragSource(colorAttachmentCount));
        }
    }
    else if (m_testType == TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY)
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));
    }
    else if (m_testType == TestType::UNUSED_WRITEN_DISCARDED)
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(4));
        glslSources.add("frag1") << glu::FragmentSource(generateReadFragSource(6));
    }
    else if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX)
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));

        // Per spec, if either of pDepthInputAttachmentIndex or pStencilInputAttachmentIndex are set to
        // NULL it means that these are only accessible in the shader if the shader does not associate
        // these input attachments with an InputAttachmentIndex.

        // NOTE at the memoment glslang doesn't support input attachments without
        // input_attachment_index qualifiers

        //std::string fragSrc(
        //    "#version 450\n"
        //    "layout(input_attachment_index=0, binding=0) uniform usubpassInput inColor0;\n"
        //    "layout(input_attachment_index=1, binding=1) uniform usubpassInput inColor1;\n"
        //    "layout(binding=2) uniform  subpassInput inDepth;\n"
        //    "layout(binding=3) uniform usubpassInput inStencil;\n"
        //    "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
        //    "  uint v[];\n"
        //    "} outBuffer;\n"
        //    "void main()\n{\n"
        //    "  uint result = subpassLoad(inColor0).x + subpassLoad(inColor1).x;\n"   // 1 + 2
        //    "  result = result + uint(subpassLoad(inDepth).x * 1000);\n"             // 0.6*1000
        //    "  result = result + uint(subpassLoad(inStencil).x * 100);\n"            // 1*100
        //    "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
        //    "  outBuffer.v[i.x+i.y*16] = result;\n"
        // "}\n");
        //glslSources.add("frag1") << glu::FragmentSource(fragSrc);

        programCollection.spirvAsmSources.add("frag1") << "OpCapability Shader\n"
                                                          "OpCapability InputAttachment\n"
                                                          "%1 = OpExtInstImport \"GLSL.std.450\";\n"
                                                          "OpMemoryModel Logical GLSL450\n"
                                                          "OpEntryPoint Fragment %4 \"main\" %51\n"
                                                          "OpExecutionMode %4 OriginUpperLeft\n"
                                                          "OpDecorate %11 DescriptorSet 0\n"
                                                          "OpDecorate %11 Binding 0\n"
                                                          "OpDecorate %11 InputAttachmentIndex 0\n"
                                                          "OpDecorate %21 DescriptorSet 0\n"
                                                          "OpDecorate %21 Binding 1\n"
                                                          "OpDecorate %21 InputAttachmentIndex 1\n"
                                                          "OpDecorate %30 DescriptorSet 0\n"
                                                          "OpDecorate %30 Binding 2\n"
                                                          //"OpDecorate %30 InputAttachmentIndex 2\n"
                                                          "OpDecorate %40 DescriptorSet 0\n"
                                                          "OpDecorate %40 Binding 3\n"
                                                          //"OpDecorate %40 InputAttachmentIndex 3\n"
                                                          "OpDecorate %51 BuiltIn FragCoord\n"
                                                          "OpDecorate %57 ArrayStride 4\n"
                                                          "OpMemberDecorate %58 0 NonReadable\n"
                                                          "OpMemberDecorate %58 0 Offset 0\n"
                                                          "OpDecorate %58 BufferBlock\n"
                                                          "OpDecorate %60 DescriptorSet 1\n"
                                                          "OpDecorate %60 Binding 0\n"
                                                          "%2 = OpTypeVoid\n"
                                                          "%3 = OpTypeFunction %2\n"
                                                          "%6 = OpTypeInt 32 0\n"
                                                          "%7 = OpTypePointer Function %6\n"
                                                          "%9 = OpTypeImage %6 SubpassData 0 0 0 2 Unknown\n"
                                                          "%10 = OpTypePointer UniformConstant %9\n"
                                                          "%11 = OpVariable %10 UniformConstant\n"
                                                          "%13 = OpTypeInt 32 1\n"
                                                          "%14 = OpConstant %13 0\n"
                                                          "%15 = OpTypeVector %13 2\n"
                                                          "%16 = OpConstantComposite %15 %14 %14\n"
                                                          "%17 = OpTypeVector %6 4\n"
                                                          "%19 = OpConstant %6 0\n"
                                                          "%21 = OpVariable %10 UniformConstant\n"
                                                          "%27 = OpTypeFloat 32\n"
                                                          "%28 = OpTypeImage %27 SubpassData 0 0 0 2 Unknown\n"
                                                          "%29 = OpTypePointer UniformConstant %28\n"
                                                          "%30 = OpVariable %29 UniformConstant\n"
                                                          "%32 = OpTypeVector %27 4\n"
                                                          "%35 = OpConstant %27 1000\n"
                                                          "%40 = OpVariable %10 UniformConstant\n"
                                                          "%44 = OpConstant %6 100\n"
                                                          "%47 = OpTypeVector %6 2\n"
                                                          "%48 = OpTypePointer Function %47\n"
                                                          "%50 = OpTypePointer Input %32\n"
                                                          "%51 = OpVariable %50 Input\n"
                                                          "%52 = OpTypeVector %27 2\n"
                                                          "%57 = OpTypeRuntimeArray %6\n"
                                                          "%58 = OpTypeStruct %57\n"
                                                          "%59 = OpTypePointer Uniform %58\n"
                                                          "%60 = OpVariable %59 Uniform\n"
                                                          "%63 = OpConstant %6 1\n"
                                                          "%66 = OpConstant %6 16\n"
                                                          "%70 = OpTypePointer Uniform %6\n"
                                                          "%4 = OpFunction %2 None %3\n"
                                                          "%5 = OpLabel\n"
                                                          "%8 = OpVariable %7 Function\n"
                                                          "%49 = OpVariable %48 Function\n"
                                                          "%12 = OpLoad %9 %11\n"
                                                          "%18 = OpImageRead %17 %12 %16\n"
                                                          "%20 = OpCompositeExtract %6 %18 0\n"
                                                          "%22 = OpLoad %9 %21\n"
                                                          "%23 = OpImageRead %17 %22 %16\n"
                                                          "%24 = OpCompositeExtract %6 %23 0\n"
                                                          "%25 = OpIAdd %6 %20 %24\n"
                                                          "OpStore %8 %25\n"
                                                          "%26 = OpLoad %6 %8\n"
                                                          "%31 = OpLoad %28 %30\n"
                                                          "%33 = OpImageRead %32 %31 %16\n"
                                                          "%34 = OpCompositeExtract %27 %33 0\n"
                                                          "%36 = OpFMul %27 %34 %35\n"
                                                          "%37 = OpConvertFToU %6 %36\n"
                                                          "%38 = OpIAdd %6 %26 %37\n"
                                                          "OpStore %8 %38\n"
                                                          "%39 = OpLoad %6 %8\n"
                                                          "%41 = OpLoad %9 %40\n"
                                                          "%42 = OpImageRead %17 %41 %16\n"
                                                          "%43 = OpCompositeExtract %6 %42 0\n"
                                                          "%45 = OpIMul %6 %43 %44\n"
                                                          "%46 = OpIAdd %6 %39 %45\n"
                                                          "OpStore %8 %46\n"
                                                          "%53 = OpLoad %32 %51\n"
                                                          "%54 = OpVectorShuffle %52 %53 %53 0 1\n"
                                                          "%55 = OpExtInst %52 %1 Trunc %54\n"
                                                          "%56 = OpConvertFToU %47 %55\n"
                                                          "OpStore %49 %56\n"
                                                          "%61 = OpAccessChain %7 %49 %19\n"
                                                          "%62 = OpLoad %6 %61\n"
                                                          "%64 = OpAccessChain %7 %49 %63\n"
                                                          "%65 = OpLoad %6 %64\n"
                                                          "%67 = OpIMul %6 %65 %66\n"
                                                          "%68 = OpIAdd %6 %62 %67\n"
                                                          "%69 = OpLoad %6 %8\n"
                                                          "%71 = OpAccessChain %70 %60 %14 %68\n"
                                                          "OpStore %71 %69\n"
                                                          "OpReturn\n"
                                                          "OpFunctionEnd\n";
    }
    else if ((m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR) ||
             (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR))
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(0));

        // NOTE at the memoment glslang doesn't support input attachments without
        // input_attachment_index qualifiers

        //std::map<std::string, std::string> specMap{{"SUBPASS_INPUT", "subpassInput inDepth"},
        //                                           {"SUBPASS_LOAD", "subpassLoad(inDepth).x * 1000"}};
        //if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR)
        //{
        //    specMap["SUBPASS_INPUT"] = "usubpassInput inStencil";
        //    specMap["SUBPASS_LOAD"]  = "subpassLoad(inStencil).x";
        //}
        //
        //std::string fragSrc("#version 450\n"
        //                    "layout(input_attachment_index=0, binding=0) uniform ${SUBPASS_INPUT};\n"
        //                    "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
        //                    "  uint v[];\n"
        //                    "} outBuffer;\n"
        //                    "void main()\n{\n"
        //                    "  uint result = uint(${SUBPASS_LOAD});\n"
        //                    "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
        //                    "  outBuffer.v[i.x+i.y*16] = result;\n"
        //                    "}\n");
        //glslSources.add("frag1") << glu::FragmentSource(tcu::StringTemplate(fragSrc).specialize(specMap));

        std::string subpassType = "%10 = OpTypeImage %9 SubpassData 0 0 0 2 Unknown\n";
        std::string saveResult  = "%19 = OpImageRead %18 %13 %17\n"
                                  "%21 = OpCompositeExtract %9 %19 0\n"
                                  "%23 = OpFMul %9 %21 %22\n"
                                  "%24 = OpConvertFToU %6 %23\n"
                                  "OpStore %8 %24\n";
        if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR)
        {
            subpassType = "%10 = OpTypeImage %6 SubpassData 0 0 0 2 Unknown\n";
            saveResult  = "%19 = OpImageRead %50 %13 %17\n"
                          "%21 = OpCompositeExtract %6 %19 0\n"
                          "OpStore %8 %21\n";
        }

        programCollection.spirvAsmSources.add("frag1") << "OpCapability Shader\n"
                                                          "OpCapability InputAttachment\n"
                                                          "%1 = OpExtInstImport \"GLSL.std.450\"\n"
                                                          "OpMemoryModel Logical GLSL450\n"
                                                          "OpEntryPoint Fragment %4 \"main\" %29\n"
                                                          "OpExecutionMode %4 OriginUpperLeft\n"
                                                          "OpDecorate %12 Binding 0\n"
                                                          "OpDecorate %12 DescriptorSet 0\n"
                                                          //"OpDecorate %12 InputAttachmentIndex 0\n"
                                                          "OpDecorate %29 BuiltIn FragCoord\n"
                                                          "OpDecorate %35 ArrayStride 4\n"
                                                          "OpDecorate %36 BufferBlock\n"
                                                          "OpMemberDecorate %36 0 NonReadable\n"
                                                          "OpMemberDecorate %36 0 Offset 0\n"
                                                          "OpDecorate %38 NonReadable\n"
                                                          "OpDecorate %38 Binding 0\n"
                                                          "OpDecorate %38 DescriptorSet 1\n"
                                                          "%2 = OpTypeVoid\n"
                                                          "%3 = OpTypeFunction %2\n"
                                                          "%6 = OpTypeInt 32 0\n"
                                                          "%7 = OpTypePointer Function %6\n"
                                                          "%9 = OpTypeFloat 32\n"
                                                       << subpassType
                                                       << "%11 = OpTypePointer UniformConstant %10\n"
                                                          "%12 = OpVariable %11 UniformConstant\n"
                                                          "%14 = OpTypeInt 32 1\n"
                                                          "%15 = OpConstant %14 0\n"
                                                          "%16 = OpTypeVector %14 2\n"
                                                          "%17 = OpConstantComposite %16 %15 %15\n"
                                                          "%18 = OpTypeVector %9 4\n"
                                                          "%50 = OpTypeVector %6 4\n"
                                                          "%20 = OpConstant %6 0\n"
                                                          "%22 = OpConstant %9 1000\n"
                                                          "%25 = OpTypeVector %6 2\n"
                                                          "%26 = OpTypePointer Function %25\n"
                                                          "%28 = OpTypePointer Input %18\n"
                                                          "%29 = OpVariable %28 Input\n"
                                                          "%30 = OpTypeVector %9 2\n"
                                                          "%35 = OpTypeRuntimeArray %6\n"
                                                          "%36 = OpTypeStruct %35\n"
                                                          "%37 = OpTypePointer Uniform %36\n"
                                                          "%38 = OpVariable %37 Uniform\n"
                                                          "%41 = OpConstant %6 1\n"
                                                          "%44 = OpConstant %6 16\n"
                                                          "%48 = OpTypePointer Uniform %6\n"
                                                          "%4 = OpFunction %2 None %3\n"
                                                          "%5 = OpLabel\n"
                                                          "%8 = OpVariable %7 Function\n"
                                                          "%27 = OpVariable %26 Function\n"
                                                          "%13 = OpLoad %10 %12\n"
                                                       << saveResult
                                                       << "%31 = OpLoad %18 %29\n"
                                                          "%32 = OpVectorShuffle %30 %31 %31 0 1\n"
                                                          "%33 = OpExtInst %30 %1 Trunc %32\n"
                                                          "%34 = OpConvertFToU %25 %33\n"
                                                          "OpStore %27 %34\n"
                                                          "%39 = OpAccessChain %7 %27 %20\n"
                                                          "%40 = OpLoad %6 %39\n"
                                                          "%42 = OpAccessChain %7 %27 %41\n"
                                                          "%43 = OpLoad %6 %42\n"
                                                          "%45 = OpIMul %6 %43 %44\n"
                                                          "%46 = OpIAdd %6 %40 %45\n"
                                                          "%47 = OpLoad %6 %8\n"
                                                          "%49 = OpAccessChain %48 %38 %15 %46\n"
                                                          "OpStore %49 %47\n"
                                                          "OpReturn\n"
                                                          "OpFunctionEnd\n";
    }
    else if ((m_testType == TestType::DEPTH_MAPPING_STENCIL_NOT) ||
             (m_testType == TestType::INTERACTION_WITH_COLOR_WRITE_ENABLE) ||
             (m_testType == TestType::INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3))
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(4));
        glslSources.add("frag1") << glu::FragmentSource(generateReadFragSource(5, false));
    }
    else if (m_testType == TestType::INPUT_ATTACHMENTS_WITHOUT_MAPPING)
    {
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(3));

        // NOTE at the memoment glslang doesn't support input attachments without
        // input_attachment_index qualifiers

        //std::string fragSrc(
        //    "#version 450\n"
        //    "layout(input_attachment_index = 0, binding = 0) uniform usubpassInput inColor0;\n"
        //    "layout(input_attachment_index = 1, binding = 1) uniform usubpassInput inColor1;\n"
        //    "layout(input_attachment_index = 2, binding = 2) uniform usubpassInput inColor2;\n"
        //    "layout(input_attachment_index = 3, binding = 3) uniform subpassInput inDepth;\n"
        //    "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
        //    "  uint v[];\n"
        //    "} outBuffer;\n"
        //    "void main()\n{\n"
        //    "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
        //    "  uint result = subpassLoad(inColor0).x + 2*subpassLoad(inColor1).x + 3*subpassLoad(inColor2).x;\n"
        //    "  outBuffer.v[i.x+i.y*16] = result + uint(subpassLoad(inDepth).x * 1000);\n"
        //    "}\n");
        //glslSources.add("frag1") << glu::FragmentSource(fragSrc);

        programCollection.spirvAsmSources.add("frag1") << "OpCapability Shader\n"
                                                          "OpCapability InputAttachment\n"
                                                          "%1 = OpExtInstImport \"GLSL.std.450\"\n"
                                                          "OpMemoryModel Logical GLSL450\n"
                                                          "OpEntryPoint Fragment %4 \"main\" %13\n"
                                                          "OpExecutionMode %4 OriginUpperLeft\n"
                                                          "OpDecorate %13 BuiltIn FragCoord\n"
                                                          "OpDecorate %23 DescriptorSet 0\n"
                                                          "OpDecorate %23 Binding 0\n"
                                                          "OpDecorate %23 InputAttachmentIndex 0\n"
                                                          "OpDecorate %34 DescriptorSet 0\n"
                                                          "OpDecorate %34 Binding 1\n"
                                                          "OpDecorate %34 InputAttachmentIndex 1\n"
                                                          "OpDecorate %41 DescriptorSet 0\n"
                                                          "OpDecorate %41 Binding 2\n"
                                                          "OpDecorate %41 InputAttachmentIndex 2\n"
                                                          "OpDecorate %47 ArrayStride 4\n"
                                                          "OpMemberDecorate %48 0 NonReadable\n"
                                                          "OpMemberDecorate %48 0 Offset 0\n"
                                                          "OpDecorate %48 BufferBlock\n"
                                                          "OpDecorate %50 DescriptorSet 1\n"
                                                          "OpDecorate %50 Binding 0\n"
                                                          "OpDecorate %62 DescriptorSet 0\n"
                                                          "OpDecorate %62 Binding 3\n"
                                                          //"OpDecorate %62 InputAttachmentIndex 3\n"
                                                          "%2 = OpTypeVoid\n"
                                                          "%3 = OpTypeFunction %2\n"
                                                          "%6 = OpTypeInt 32 0\n"
                                                          "%7 = OpTypeVector %6 2\n"
                                                          "%8 = OpTypePointer Function %7\n"
                                                          "%10 = OpTypeFloat 32\n"
                                                          "%11 = OpTypeVector %10 4\n"
                                                          "%12 = OpTypePointer Input %11\n"
                                                          "%13 = OpVariable %12 Input\n"
                                                          "%14 = OpTypeVector %10 2\n"
                                                          "%19 = OpTypePointer Function %6\n"
                                                          "%21 = OpTypeImage %6 SubpassData 0 0 0 2 Unknown\n"
                                                          "%22 = OpTypePointer UniformConstant %21\n"
                                                          "%23 = OpVariable %22 UniformConstant\n"
                                                          "%25 = OpTypeInt 32 1\n"
                                                          "%26 = OpConstant %25 0\n"
                                                          "%27 = OpTypeVector %25 2\n"
                                                          "%28 = OpConstantComposite %27 %26 %26\n"
                                                          "%29 = OpTypeVector %6 4\n"
                                                          "%31 = OpConstant %6 0\n"
                                                          "%33 = OpConstant %6 2\n"
                                                          "%34 = OpVariable %22 UniformConstant\n"
                                                          "%40 = OpConstant %6 3\n"
                                                          "%41 = OpVariable %22 UniformConstant\n"
                                                          "%47 = OpTypeRuntimeArray %6\n"
                                                          "%48 = OpTypeStruct %47\n"
                                                          "%49 = OpTypePointer Uniform %48\n"
                                                          "%50 = OpVariable %49 Uniform\n"
                                                          "%53 = OpConstant %6 1\n"
                                                          "%56 = OpConstant %6 16\n"
                                                          "%60 = OpTypeImage %10 SubpassData 0 0 0 2 Unknown\n"
                                                          "%61 = OpTypePointer UniformConstant %60\n"
                                                          "%62 = OpVariable %61 UniformConstant\n"
                                                          "%66 = OpConstant %10 1000\n"
                                                          "%70 = OpTypePointer Uniform %6\n"
                                                          "%4 = OpFunction %2 None %3\n"
                                                          "%5 = OpLabel\n"
                                                          "%9 = OpVariable %8 Function\n"
                                                          "%20 = OpVariable %19 Function\n"
                                                          "%15 = OpLoad %11 %13\n"
                                                          "%16 = OpVectorShuffle %14 %15 %15 0 1\n"
                                                          "%17 = OpExtInst %14 %1 Trunc %16\n"
                                                          "%18 = OpConvertFToU %7 %17\n"
                                                          "OpStore %9 %18\n"
                                                          "%24 = OpLoad %21 %23\n"
                                                          "%30 = OpImageRead %29 %24 %28\n"
                                                          "%32 = OpCompositeExtract %6 %30 0\n"
                                                          "%35 = OpLoad %21 %34\n"
                                                          "%36 = OpImageRead %29 %35 %28\n"
                                                          "%37 = OpCompositeExtract %6 %36 0\n"
                                                          "%38 = OpIMul %6 %33 %37\n"
                                                          "%39 = OpIAdd %6 %32 %38\n"
                                                          "%42 = OpLoad %21 %41\n"
                                                          "%43 = OpImageRead %29 %42 %28\n"
                                                          "%44 = OpCompositeExtract %6 %43 0\n"
                                                          "%45 = OpIMul %6 %40 %44\n"
                                                          "%46 = OpIAdd %6 %39 %45\n"
                                                          "OpStore %20 %46\n"
                                                          "%51 = OpAccessChain %19 %9 %31\n"
                                                          "%52 = OpLoad %6 %51\n"
                                                          "%54 = OpAccessChain %19 %9 %53\n"
                                                          "%55 = OpLoad %6 %54\n"
                                                          "%57 = OpIMul %6 %55 %56\n"
                                                          "%58 = OpIAdd %6 %52 %57\n"
                                                          "%59 = OpLoad %6 %20\n"
                                                          "%63 = OpLoad %60 %62\n"
                                                          "%64 = OpImageRead %11 %63 %28\n"
                                                          "%65 = OpCompositeExtract %10 %64 0\n"
                                                          "%67 = OpFMul %10 %65 %66\n"
                                                          "%68 = OpConvertFToU %6 %67\n"
                                                          "%69 = OpIAdd %6 %59 %68\n"
                                                          "%71 = OpAccessChain %70 %50 %26 %58\n"
                                                          "OpStore %71 %69\n"
                                                          "OpReturn\n"
                                                          "OpFunctionEnd\n";
    }
    else if ((m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX) ||
             (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX))
    {
        bool useLargeIndex       = (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX);
        std::string depthIndex   = useLargeIndex ? "20" : "2";
        std::string stencilIndex = useLargeIndex ? "21" : "2";

        std::stringstream fragSrc;
        fragSrc << "#version 450\n"
                   "layout(input_attachment_index = 0, binding = 0) uniform usubpassInput inColor0;\n"
                   "layout(input_attachment_index = 1, binding = 1) uniform usubpassInput inColor1;\n"
                   "layout(input_attachment_index = "
                << depthIndex
                << ", binding = 2) uniform  subpassInput inDepth;\n"
                   "layout(input_attachment_index = "
                << stencilIndex
                << ", binding = 3) uniform usubpassInput inStencil;\n"
                   "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
                   "  uint v[];\n"
                   "} outBuffer;\n"
                   "void main()\n{\n"
                   "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
                   "  outBuffer.v[i.x+i.y*16] = uint(subpassLoad(inDepth).x * 1000) + subpassLoad(inStencil).x;\n"
                   "}\n";
        glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));
        glslSources.add("frag1") << glu::FragmentSource(fragSrc.str());
    }
    else if (m_testType == TestType::MAPPING_NOT_AFFECTING_BLEND_STATE)
    {
        std::string fragSrc("#version 450\n"
                            "layout(location = 0) out vec4 outColor0;\n"
                            "layout(location = 1) out vec4 outColor1;\n"
                            "layout(location = 2) out vec4 outColor2;\n"
                            "layout(location = 3) out vec4 outColor3;\n"
                            "void main()\n{\n"
                            "  if (gl_FragCoord.x > 8.0)\n"
                            "    discard;\n"
                            "  outColor0 = vec4(0.6, 0.8, 0.9, 0.2);\n" // used for attachment 1
                            "  outColor1 = vec4(0.6, 0.4, 0.2, 0.6);\n" // used for attachment 3
                            "  outColor2 = vec4(0.1, 0.3, 0.6, 0.2);\n" // used for attachment 2
                            "  outColor3 = vec4(0.0, 0.6, 0.4, 0.8);\n" // used for attachment 0
                            "}\n");
        glslSources.add("frag") << glu::FragmentSource(fragSrc);
    }
    else if (m_testType == TestType::INTERACTION_WITH_GRAPHICS_PIPELINE_LIBRARY)
    {
        std::string fragSrc("#version 450\n"
                            "layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor0;\n"
                            "layout(input_attachment_index = 1, binding = 1) uniform subpassInput inColor1;\n"
                            "layout(input_attachment_index = 2, binding = 2) uniform subpassInput inColor2;\n"
                            "layout(location = 3) out vec4 outColor;\n"
                            "void main()\n{\n"
                            "  outColor = subpassLoad(inColor0) + subpassLoad(inColor2);\n"
                            "}\n");
        glslSources.add("frag") << glu::FragmentSource(fragSrc);
    }
    else if (m_testType == TestType::INTERACTION_WITH_SHADER_OBJECT)
    {
        std::string fragSrc("#version 450\n"
                            "layout(location = 0) out vec4 outColor0;\n"
                            "layout(location = 1) out vec4 outColor1;\n"
                            "layout(location = 2) out vec4 outColor2;\n"
                            "void main()\n{\n"
                            "  outColor0 = vec4(1.0, 0.0, 1.0, 1.0);\n"
                            "  outColor1 = vec4(1.0, 1.0, 0.0, 1.0);\n"
                            "  outColor2 = vec4(0.0, 1.0, 1.0, 1.0);\n"
                            "}\n");
        glslSources.add("frag") << glu::FragmentSource(fragSrc);
    }
    else if (m_testType == TestType::FEEDBACK_LOOP)
    {
        std::string fragSrc("#version 450\n"
                            "layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor;\n"
                            "layout(location = 0) out vec4 outColor;\n"
                            "void main()\n{\n"
                            "  vec2 uvNormalized = gl_FragCoord.xy / vec2(1024);\n"
                            "  vec4 color = subpassLoad(inColor);\n"
                            // add or substract depending on value in inColor
                            "  vec4 select = step(vec4(0.5), color);\n"
                            "  color += mix(vec4(0.2), vec4(-0.2), select);\n"
                            "  outColor = color;\n"
                            "}\n");
        glslSources.add("frag") << glu::FragmentSource(fragSrc);
    }

    if ((m_testType == TestType::MAX_INPUT_ATTACHMENTS) ||
        (m_testType == TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY))
    {
        // generate fragment shaders for all posible number of input attachments;
        // during test execution proper shader will be picked
        for (uint32_t inputAttachmentCount : inputAttachmentsPossibleValues)
            glslSources.add(std::string("frag1_") + std::to_string(inputAttachmentCount))
                << glu::FragmentSource(generateReadFragSource(inputAttachmentCount));
    }
}

TestInstance *LocalReadTestCase::createInstance(Context &context) const
{
    if (m_testType == TestType::MAPPING_NOT_AFFECTING_BLEND_STATE)
        return new MappingWithBlendStateTestInstance(context, m_testType);
    if (m_testType == TestType::INTERACTION_WITH_GRAPHICS_PIPELINE_LIBRARY)
        return new MappingWithGraphicsPipelineLibraryTestInstance(context, m_testType);
    if (m_testType == TestType::FEEDBACK_LOOP)
        return new FeedbackLoopTestInstance(context, m_testType);

    if (m_testType == TestType::INTERACTION_WITH_SHADER_OBJECT)
        return new MappingWithShaderObjectTestInstance(context, m_testType);

    return new BasicLocalReadTestInstance(context, m_testType);
}

} // namespace

tcu::TestCaseGroup *createDynamicRenderingLocalReadTests(tcu::TestContext &testCtx)
{
    struct TestConfig
    {
        std::string name;
        TestType testType;
    };
    std::vector<TestConfig> testConfigs{
        {"max_input_attachments", TestType::MAX_INPUT_ATTACHMENTS},
        {"max_attachments_remapped_repeatedly", TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY},
        {"input_attachments_without_mapping", TestType::INPUT_ATTACHMENTS_WITHOUT_MAPPING},
        {"unused_writen_discarded", TestType::UNUSED_WRITEN_DISCARDED},
        {"depth_stencil_mapping_to_no_index", TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX},
        {"depth_stencil_mapping_to_no_index_depth_clear", TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_DEPTH_CLEAR},
        {"depth_stencil_mapping_to_no_index_stencil_clear", TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX_STENCIL_CLEAR},
        {"depth_stencil_mapping_to_same_index", TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX},
        {"depth_stencil_mapping_to_large_index", TestType::DEPTH_STENCIL_MAPPING_TO_LARGE_INDEX},
        {"depth_mapping_stencil_not", TestType::DEPTH_MAPPING_STENCIL_NOT},
        {"mapping_not_affecting_blend_state", TestType::MAPPING_NOT_AFFECTING_BLEND_STATE},
        {"interaction_with_color_write_enable", TestType::INTERACTION_WITH_COLOR_WRITE_ENABLE},
        {"interaction_with_graphics_pipeline_library", TestType::INTERACTION_WITH_GRAPHICS_PIPELINE_LIBRARY},
        {"interaction_with_extended_dynamic_state3", TestType::INTERACTION_WITH_EXTENDED_DYNAMIC_STATE3},
        {"interaction_with_shader_object", TestType::INTERACTION_WITH_SHADER_OBJECT},
        {"feedback_loop", TestType::FEEDBACK_LOOP},
    };

    de::MovePtr<tcu::TestCaseGroup> mainGroup(
        new tcu::TestCaseGroup(testCtx, "local_read", "Test dynamic rendering local read"));

    for (const auto &testConfig : testConfigs)
        mainGroup->addChild(new LocalReadTestCase(testCtx, testConfig.name, testConfig.testType));

    return mainGroup.release();
}

} // namespace renderpass
} // namespace vkt
