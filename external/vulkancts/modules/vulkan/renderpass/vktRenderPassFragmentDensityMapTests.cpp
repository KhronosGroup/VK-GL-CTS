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
 * \brief Tests fragment density map extension ( VK_EXT_fragment_density_map )
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassFragmentDensityMapTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "deMath.h"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "tcuTestLog.hpp"
#include <sstream>
#include <vector>

// Each test generates an image with a color gradient where all colors should be unique when rendered without density map
// ( and for multi_view tests - the quantity of each color in a histogram should be 2 instead of 1 ).
// The whole density map has the same values defined by input fragment area ( one of the test input parameters ).
// With density map enabled - the number of each color in a histogram should be [ fragmentArea.x * fragmentArea.y ]
// ( that value will be doubled for multi_view case ).
//
// Additionally test checks if gl_FragSizeEXT shader variable has proper value ( as defined by fragmentArea input parameter ).
//
// Test variations:
// - multi_view tests check if density map also works when VK_KHR_multiview extension is in use
// - render_copy tests check if it's possible to copy results using input attachment descriptor ( this simulates deferred rendering behaviour )
// - non_divisible_density_size tests check if subsampled images work when its dimension is not divisible by minFragmentDensityTexelSize
// - N_samples tests check if multisampling works with VK_EXT_fragment_density_map extension
// - static_* tests use density map loaded from CPU.
// - dynamic_* tests use density map rendered on a GPU in a separate render pass
// - *_nonsubsampled tests check if it's possible to use nonsubsampled images instead of subsampled ones

// There are 3 render passes performed during the test:
//  - render pass that produces density map ( this rp is skipped when density map is static )
//  - render pass that produces subsampled image using density map and eventually copies results to different image ( render_copy )
//  - render pass that copies subsampled image to traditional image using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT flag.
//    ( because subsampled images cannot be retrieved to CPU in any other way ).

namespace vkt
{

namespace renderpass
{

using namespace vk;

namespace
{

// set value of DRY_RUN_WITHOUT_FDM_EXTENSION to 1 if you want to check the correctness of the code without using VK_EXT_fragment_density_map extension
#define DRY_RUN_WITHOUT_FDM_EXTENSION 0

struct TestParams
{
    TestParams(bool dynamicDensity, bool nonSubsampled, bool multiview, bool copy, float renderSizeMultiplier,
               VkSampleCountFlagBits samples, const tcu::UVec2 &area)
        : dynamicDensityMap{dynamicDensity}
        , nonSubsampledImages{nonSubsampled}
        , multiView{multiview}
        , makeCopy{copy}
        , densityMapSize{16, 16}
        , colorSamples{samples}
        , fragmentArea{area}
        , densityMapFormat{VK_FORMAT_R8G8_UNORM}
    {
        renderSize = tcu::UVec2(deFloorFloatToInt32(renderSizeMultiplier * static_cast<float>(densityMapSize.x())),
                                deFloorFloatToInt32(renderSizeMultiplier * static_cast<float>(densityMapSize.y())));
    }
    bool dynamicDensityMap;
    bool nonSubsampledImages;
    bool multiView;
    bool makeCopy;
    tcu::UVec2 densityMapSize;
    tcu::UVec2 renderSize;
    VkSampleCountFlagBits colorSamples;
    tcu::UVec2 fragmentArea;
    VkFormat densityMapFormat;
};

struct Vertex4RGBA
{
    tcu::Vec4 position;
    tcu::Vec4 uv;
    tcu::Vec4 color;
};

std::vector<Vertex4RGBA> createFullscreenQuadRG(void)
{
    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
                                          tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperLeftVertex  = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
                                          tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                                          tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperRightVertex = {tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
                                          tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)};

    return {lowerLeftVertex, lowerRightVertex, upperLeftVertex, upperLeftVertex, lowerRightVertex, upperRightVertex};
}

std::vector<Vertex4RGBA> createFullscreenQuadDensity(float densityX, float densityY)
{
    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
                                          tcu::Vec4(densityX, densityY, 0.0f, 1.0f)};
    const Vertex4RGBA upperLeftVertex  = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
                                          tcu::Vec4(densityX, densityY, 0.0f, 1.0f)};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                                          tcu::Vec4(densityX, densityY, 0.0f, 1.0f)};
    const Vertex4RGBA upperRightVertex = {tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
                                          tcu::Vec4(densityX, densityY, 0.0f, 1.0f)};

    return {lowerLeftVertex, lowerRightVertex, upperLeftVertex, upperLeftVertex, lowerRightVertex, upperRightVertex};
};

std::vector<Vertex4RGBA> createFullscreenMeshOutput(bool isMultiview)
{
    float midX = isMultiview ? 0.0f : 1.0f;

    const Vertex4RGBA lowerLeftVertex0  = {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperLeftVertex0  = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA lowerRightVertex0 = {tcu::Vec4(midX, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperRightVertex0 = {tcu::Vec4(midX, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};

    const Vertex4RGBA lowerLeftVertex1  = {tcu::Vec4(midX, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperLeftVertex1  = {tcu::Vec4(midX, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA lowerRightVertex1 = {tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};
    const Vertex4RGBA upperRightVertex1 = {tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
                                           tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)};

    if (isMultiview)
        return {lowerLeftVertex0,  lowerRightVertex0, upperLeftVertex0,  upperLeftVertex0,
                lowerRightVertex0, upperRightVertex0, lowerLeftVertex1,  lowerRightVertex1,
                upperLeftVertex1,  upperLeftVertex1,  lowerRightVertex1, upperRightVertex1};
    return {lowerLeftVertex0, lowerRightVertex0, upperLeftVertex0,
            upperLeftVertex0, lowerRightVertex0, upperRightVertex0};
}

template <typename T>
void createVertexBuffer(const DeviceInterface &vk, VkDevice vkDevice, const uint32_t &queueFamilyIndex,
                        SimpleAllocator &memAlloc, const std::vector<T> &vertices, Move<VkBuffer> &vertexBuffer,
                        de::MovePtr<Allocation> &vertexAlloc)
{
    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,        // VkStructureType sType;
        DE_NULL,                                     // const void* pNext;
        0u,                                          // VkBufferCreateFlags flags;
        (VkDeviceSize)(sizeof(T) * vertices.size()), // VkDeviceSize size;
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,           // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode sharingMode;
        1u,                                          // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                            // const uint32_t* pQueueFamilyIndices;
    };

    vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);
    vertexAlloc =
        memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

    // Upload vertex data
    deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(T));
    flushAlloc(vk, vkDevice, *vertexAlloc);
}

void prepareImageAndImageView(const DeviceInterface &vk, VkDevice vkDevice, SimpleAllocator &memAlloc,
                              VkImageCreateFlags imageCreateFlags, VkFormat format, VkExtent3D extent,
                              uint32_t arrayLayers, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                              uint32_t queueFamilyIndex, VkImageViewCreateFlags viewFlags, VkImageViewType viewType,
                              const VkComponentMapping &channels, const VkImageSubresourceRange &subresourceRange,
                              Move<VkImage> &image, de::MovePtr<Allocation> &imageAlloc, Move<VkImageView> &imageView)
{
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                             // const void* pNext;
        imageCreateFlags,                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        extent,                              // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        arrayLayers,                         // uint32_t arrayLayers;
        samples,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    image = createImage(vk, vkDevice, &imageCreateInfo);

    // Allocate and bind color image memory
    imageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
    VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

    // create image view for subsampled image
    const VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                  // const void* pNext;
        viewFlags,                                // VkImageViewCreateFlags flags;
        *image,                                   // VkImage image;
        viewType,                                 // VkImageViewType viewType;
        format,                                   // VkFormat format;
        channels,                                 // VkChannelMapping channels;
        subresourceRange                          // VkImageSubresourceRange subresourceRange;
    };

    imageView = createImageView(vk, vkDevice, &imageViewCreateInfo);
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassProduceDynamicDensityMap(const DeviceInterface &vk, VkDevice vkDevice,
                                                            const TestParams &testParams)
{
    VkImageLayout densityPassFinalLayout               = testParams.dynamicDensityMap ?
                                                             VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT :
                                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::vector<AttachmentDesc> attachmentDescriptions = {{
        DE_NULL,                          // const void*                        pNext
        (VkAttachmentDescriptionFlags)0,  // VkAttachmentDescriptionFlags        flags
        testParams.densityMapFormat,      // VkFormat                            format
        VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,      // VkAttachmentLoadOp                loadOp
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp                storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp                stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp                stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout                    initialLayout
        densityPassFinalLayout            // VkImageLayout                    finalLayout
    }};

    std::vector<AttachmentRef> colorAttachmentRefs{
        {DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    std::vector<SubpassDesc> subpassDescriptions{{
        DE_NULL,
        (VkSubpassDescriptionFlags)0,                      // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                   // VkPipelineBindPoint                pipelineBindPoint
        testParams.multiView ? 0x3u : 0x0u,                // uint32_t                            viewMask
        0u,                                                // uint32_t                            inputAttachmentCount
        DE_NULL,                                           // const VkAttachmentReference*        pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs.size()), // uint32_t                            colorAttachmentCount
        colorAttachmentRefs.data(),                        // const VkAttachmentReference*        pColorAttachments
        DE_NULL,                                           // const VkAttachmentReference*        pResolveAttachments
        DE_NULL, // const VkAttachmentReference*        pDepthStencilAttachment
        0u,      // uint32_t                            preserveAttachmentCount
        DE_NULL  // const uint32_t*                    pPreserveAttachments
    }};

    std::vector<SubpassDep> subpassDependencies;
    if (testParams.dynamicDensityMap)
    {
        subpassDependencies.emplace_back(
            SubpassDep(DE_NULL,                                            // const void*                pNext
                       0u,                                                 // uint32_t                    srcSubpass
                       VK_SUBPASS_EXTERNAL,                                // uint32_t                    dstSubpass
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,      // VkPipelineStageFlags        srcStageMask
                       VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, // VkPipelineStageFlags        dstStageMask
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,               // VkAccessFlags            srcAccessMask
                       VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,        // VkAccessFlags            dstAccessMask
                       VK_DEPENDENCY_BY_REGION_BIT,                        // VkDependencyFlags        dependencyFlags
                       0u                                                  // int32_t                    viewOffset
                       ));
    };

    vk::VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo;
    void *renderPassInfoPNext = DE_NULL;
    std::vector<uint32_t> viewMasks(subpassDescriptions.size(), 0x3u);

    if (testParams.multiView)
    {

        renderPassMultiviewCreateInfo.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
        renderPassMultiviewCreateInfo.pNext                = renderPassInfoPNext;
        renderPassMultiviewCreateInfo.subpassCount         = static_cast<uint32_t>(viewMasks.size());
        renderPassMultiviewCreateInfo.pViewMasks           = viewMasks.data();
        renderPassMultiviewCreateInfo.dependencyCount      = 0;
        renderPassMultiviewCreateInfo.pViewOffsets         = DE_NULL;
        renderPassMultiviewCreateInfo.correlationMaskCount = 0U;
        renderPassMultiviewCreateInfo.pCorrelationMasks    = DE_NULL;
        renderPassInfoPNext                                = (void *)&renderPassMultiviewCreateInfo;
    }

    const RenderPassCreateInfo renderPassInfo(
        renderPassInfoPNext,                                  // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        static_cast<uint32_t>(subpassDependencies.size()),    // uint32_t                            dependencyCount
        subpassDependencies.empty() ? DE_NULL :
                                      subpassDependencies.data(), // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        DE_NULL // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassProduceSubsampledImage(const DeviceInterface &vk, VkDevice vkDevice,
                                                          const TestParams &testParams)
{
    // add color image
    std::vector<AttachmentDesc> attachmentDescriptions{
        // Output color attachment
        {
            DE_NULL,                                 // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
            VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
            testParams.colorSamples,                 // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
        }};
    uint32_t multisampleAttachmentIndex = 0;
    uint32_t copyAttachmentIndex        = 0;
    uint32_t densityMapAttachmentIndex  = 0;

    // add resolve image when we use more than one sample per fragment
    if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
    {
        multisampleAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
        attachmentDescriptions.emplace_back(
            AttachmentDesc(DE_NULL,                                 // const void*                        pNext
                           (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
                           VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
                           VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
                           VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
                           VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
                           VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
                           VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
                           ));
    }

    // add color image copy ( when render_copy is used )
    if (testParams.makeCopy)
    {
        copyAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
        attachmentDescriptions.emplace_back(
            AttachmentDesc(DE_NULL,                                 // const void*                        pNext
                           (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
                           VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
                           testParams.colorSamples,                 // VkSampleCountFlagBits            samples
                           VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
                           VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
                           VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
                           VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
                           ));
    }

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    // add density map
    {
        densityMapAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
        attachmentDescriptions.emplace_back(AttachmentDesc(
            DE_NULL,                                          // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,                  // VkAttachmentDescriptionFlags        flags
            testParams.densityMapFormat,                      // VkFormat                            format
            VK_SAMPLE_COUNT_1_BIT,                            // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                  // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT  // VkImageLayout                    finalLayout
            ));
    }
#endif

    std::vector<AttachmentRef> colorAttachmentRefs0{
        {DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    std::vector<AttachmentRef> resolveAttachmentRefs0;

    // for multisampled scenario without copying results - we need to add resolve attachment
    if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT && !testParams.makeCopy)
    {
        resolveAttachmentRefs0.push_back(
            {DE_NULL, multisampleAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT});
    };

    std::vector<SubpassDesc> subpassDescriptions{{
        DE_NULL,
        (VkSubpassDescriptionFlags)0,                       // VkSubpassDescriptionFlags    flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                    // VkPipelineBindPoint            pipelineBindPoint
        testParams.multiView ? 0x3u : 0x0u,                 // uint32_t                        viewMask
        0u,                                                 // uint32_t                        inputAttachmentCount
        DE_NULL,                                            // const VkAttachmentReference*    pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs0.size()), // uint32_t                        colorAttachmentCount
        colorAttachmentRefs0.data(),                        // const VkAttachmentReference*    pColorAttachments
        resolveAttachmentRefs0.empty() ?
            DE_NULL :
            resolveAttachmentRefs0.data(), // const VkAttachmentReference*    pResolveAttachments
        DE_NULL,                           // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                // uint32_t                        preserveAttachmentCount
        DE_NULL                            // const uint32_t*                pPreserveAttachments
    }};

    std::vector<AttachmentRef> inputAttachmentRefs1{
        {DE_NULL, 0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};
    std::vector<AttachmentRef> colorAttachmentRefs1{
        {DE_NULL, copyAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};
    std::vector<AttachmentRef> resolveAttachmentRefs1;
    if (testParams.makeCopy)
    {
        if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
            resolveAttachmentRefs1.push_back({DE_NULL, multisampleAttachmentIndex,
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT});

        subpassDescriptions.push_back({
            DE_NULL,
            (VkSubpassDescriptionFlags)0,                       // VkSubpassDescriptionFlags    flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,                    // VkPipelineBindPoint            pipelineBindPoint
            testParams.multiView ? 0x3u : 0x0u,                 // uint32_t                        viewMask
            static_cast<uint32_t>(inputAttachmentRefs1.size()), // uint32_t                        inputAttachmentCount
            inputAttachmentRefs1.data(),                        // const VkAttachmentReference*    pInputAttachments
            static_cast<uint32_t>(colorAttachmentRefs1.size()), // uint32_t                        colorAttachmentCount
            colorAttachmentRefs1.data(),                        // const VkAttachmentReference*    pColorAttachments
            resolveAttachmentRefs1.empty() ?
                DE_NULL :
                resolveAttachmentRefs1.data(), // const VkAttachmentReference*    pResolveAttachments
            DE_NULL,                           // const VkAttachmentReference*    pDepthStencilAttachment
            0u,                                // uint32_t                        preserveAttachmentCount
            DE_NULL                            // const uint32_t*                pPreserveAttachments
        });
    }

    std::vector<SubpassDep> subpassDependencies;
    if (testParams.makeCopy)
    {
        VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        if (testParams.multiView)
            dependencyFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;

        subpassDependencies.push_back({
            DE_NULL,                                       // const void*                pNext
            0u,                                            // uint32_t                    srcSubpass
            1u,                                            // uint32_t                    dstSubpass
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags        dstStageMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags            dstAccessMask
            dependencyFlags,                               // VkDependencyFlags        dependencyFlags
            0u                                             // int32_t                    viewOffset
        });
    }
    subpassDependencies.push_back({
        DE_NULL,                                       // const void*                pNext
        testParams.makeCopy ? 1u : 0u,                 // uint32_t                    srcSubpass
        VK_SUBPASS_EXTERNAL,                           // uint32_t                    dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags        dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,                     // VkAccessFlags            dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT,                   // VkDependencyFlags        dependencyFlags
        0u                                             // int32_t                    viewOffset
    });

    VkRenderPassFragmentDensityMapCreateInfoEXT renderPassFragmentDensityMap;
    renderPassFragmentDensityMap.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
    renderPassFragmentDensityMap.pNext = DE_NULL;
    renderPassFragmentDensityMap.fragmentDensityMapAttachment = {densityMapAttachmentIndex,
                                                                 VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT};

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    void *renderPassInfoPNext = (void *)&renderPassFragmentDensityMap;
#else
    void *renderPassInfoPNext = DE_NULL;
#endif

    vk::VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo;
    std::vector<uint32_t> viewMasks(subpassDescriptions.size(), 0x3u);
    int32_t pViewOffsets = 0;

    if (testParams.multiView)
    {

        renderPassMultiviewCreateInfo.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
        renderPassMultiviewCreateInfo.pNext                = renderPassInfoPNext;
        renderPassMultiviewCreateInfo.subpassCount         = static_cast<uint32_t>(viewMasks.size());
        renderPassMultiviewCreateInfo.pViewMasks           = viewMasks.data();
        renderPassMultiviewCreateInfo.dependencyCount      = testParams.makeCopy ? 1 : 0;
        renderPassMultiviewCreateInfo.pViewOffsets         = testParams.makeCopy ? &pViewOffsets : DE_NULL;
        renderPassMultiviewCreateInfo.correlationMaskCount = 0U;
        renderPassMultiviewCreateInfo.pCorrelationMasks    = DE_NULL;
        renderPassInfoPNext                                = (void *)&renderPassMultiviewCreateInfo;
    }

    const RenderPassCreateInfo renderPassInfo(
        renderPassInfoPNext,                                  // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        static_cast<uint32_t>(subpassDependencies.size()),    // uint32_t                            dependencyCount
        subpassDependencies.data(),                           // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        DE_NULL // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassOutputSubsampledImage(const DeviceInterface &vk, VkDevice vkDevice,
                                                         const TestParams &testParams)
{
    DE_UNREF(testParams);
    // copy subsampled image to ordinary image - you cannot retrieve subsampled image to CPU in any way. You must first convert it into plain image through rendering
    std::vector<AttachmentDesc> attachmentDescriptions = {
        // output attachment
        AttachmentDesc(DE_NULL,                                 // const void*                        pNext
                       (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
                       VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
                       VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
                       VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
                       VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
                       VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
                       VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
                       ),
    };

    std::vector<AttachmentRef> colorAttachmentRefs{
        {DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    std::vector<SubpassDesc> subpassDescriptions = {{
        DE_NULL,
        (VkSubpassDescriptionFlags)0,                      // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                   // VkPipelineBindPoint                pipelineBindPoint
        0u,                                                // uint32_t                            viewMask
        0u,                                                // uint32_t                            inputAttachmentCount
        DE_NULL,                                           // const VkAttachmentReference*        pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs.size()), // uint32_t                            colorAttachmentCount
        colorAttachmentRefs.data(),                        // const VkAttachmentReference*        pColorAttachments
        DE_NULL,                                           // const VkAttachmentReference*        pResolveAttachments
        DE_NULL, // const VkAttachmentReference*        pDepthStencilAttachment
        0u,      // uint32_t                            preserveAttachmentCount
        DE_NULL  // const uint32_t*                    pPreserveAttachments
    }};

    const RenderPassCreateInfo renderPassInfo(
        DE_NULL,                                              // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        0,                                                    // uint32_t                            dependencyCount
        DE_NULL,                                              // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        DE_NULL // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
}

Move<VkFramebuffer> createFrameBuffer(const DeviceInterface &vk, VkDevice vkDevice, VkRenderPass renderPass,
                                      uint32_t width, uint32_t height, const std::vector<VkImageView> &imageViews)
{
    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                   // const void* pNext;
        0u,                                        // VkFramebufferCreateFlags flags;
        renderPass,                                // VkRenderPass renderPass;
        static_cast<uint32_t>(imageViews.size()),  // uint32_t attachmentCount;
        imageViews.data(),                         // const VkImageView* pAttachments;
        width,                                     // uint32_t width;
        height,                                    // uint32_t height;
        1u                                         // uint32_t layers;
    };

    return createFramebuffer(vk, vkDevice, &framebufferParams);
}

class FragmentDensityMapTest : public vkt::TestCase
{
public:
    FragmentDensityMapTest(tcu::TestContext &testContext, const std::string &name, const std::string &description,
                           const TestParams &testParams);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const TestParams m_testParams;
};

class FragmentDensityMapTestInstance : public vkt::TestInstance
{
public:
    FragmentDensityMapTestInstance(Context &context, const TestParams &testParams);
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);

    TestParams m_testParams;

    VkPhysicalDeviceFragmentDensityMapPropertiesEXT m_fragmentDensityMapProperties;

    Move<VkCommandPool> m_cmdPool;

    Move<VkImage> m_densityMapImage;
    de::MovePtr<Allocation> m_densityMapImageAlloc;
    Move<VkImageView> m_densityMapImageView;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorImageView;

    Move<VkImage> m_colorCopyImage;
    de::MovePtr<Allocation> m_colorCopyImageAlloc;
    Move<VkImageView> m_colorCopyImageView;

    Move<VkImage> m_colorResolvedImage;
    de::MovePtr<Allocation> m_colorResolvedImageAlloc;
    Move<VkImageView> m_colorResolvedImageView;

    Move<VkImage> m_outputImage;
    de::MovePtr<Allocation> m_outputImageAlloc;
    Move<VkImageView> m_outputImageView;

    Move<VkSampler> m_colorSampler;

    Move<VkRenderPass> m_renderPassProduceDynamicDensityMap;
    Move<VkRenderPass> m_renderPassProduceSubsampledImage;
    Move<VkRenderPass> m_renderPassOutputSubsampledImage;
    Move<VkFramebuffer> m_framebufferProduceDynamicDensityMap;
    Move<VkFramebuffer> m_framebufferProduceSubsampledImage;
    Move<VkFramebuffer> m_framebufferOutputSubsampledImage;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutProduceSubsampled;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutCopySubsampledImage;
    Move<VkDescriptorPool> m_descriptorPoolCopySubsampledImage;
    Move<VkDescriptorSet> m_descriptorSetCopySubsampledImage;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutOutputSubsampledImage;
    Move<VkDescriptorPool> m_descriptorPoolOutputSubsampledImage;
    Move<VkDescriptorSet> m_descriptorSetOutputSubsampledImage;

    Move<VkShaderModule> m_vertexCommonShaderModule;
    Move<VkShaderModule> m_fragmentShaderModuleProduceSubsampledImage;
    Move<VkShaderModule> m_fragmentShaderModuleCopySubsampledImage;
    Move<VkShaderModule> m_fragmentShaderModuleOutputSubsampledImage;

    std::vector<Vertex4RGBA> m_verticesDDM;
    Move<VkBuffer> m_vertexBufferDDM;
    de::MovePtr<Allocation> m_vertexBufferAllocDDM;

    std::vector<Vertex4RGBA> m_vertices;
    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    std::vector<Vertex4RGBA> m_verticesOutput;
    Move<VkBuffer> m_vertexBufferOutput;
    de::MovePtr<Allocation> m_vertexBufferOutputAlloc;

    Move<VkPipelineLayout> m_pipelineLayoutNoDescriptors;
    Move<VkPipelineLayout> m_pipelineLayoutCopySubsampledImage;
    Move<VkPipelineLayout> m_pipelineLayoutOutputSubsampledImage;
    Move<VkPipeline> m_graphicsPipelineProduceDynamicDensityMap;
    Move<VkPipeline> m_graphicsPipelineProduceSubsampledImage;
    Move<VkPipeline> m_graphicsPipelineCopySubsampledImage;
    Move<VkPipeline> m_graphicsPipelineOutputSubsampledImage;

    Move<VkCommandBuffer> m_cmdBuffer;
};

FragmentDensityMapTest::FragmentDensityMapTest(tcu::TestContext &testContext, const std::string &name,
                                               const std::string &description, const TestParams &testParams)
    : vkt::TestCase(testContext, name, description)
    , m_testParams(testParams)
{
}

void FragmentDensityMapTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream densityVertexGLSL;
    densityVertexGLSL << "#version 450\n"
                         "#extension GL_EXT_multiview : enable\n"
                         "layout(location = 0) in  vec4 inPosition;\n"
                         "layout(location = 1) in  vec4 inUV;\n"
                         "layout(location = 2) in  vec4 inColor;\n"
                         "layout(location = 0) out vec4 outUV;\n"
                         "layout(location = 1) out vec4 outColor;\n"
                         "void main(void)\n"
                         "{\n"
                         "    gl_Position = inPosition;\n"
                         "    outUV = inUV;\n"
                         "    outColor = inColor;\n"
                         "}\n";
    sourceCollections.glslSources.add("densitymap_vert") << glu::VertexSource(densityVertexGLSL.str());

    std::ostringstream densityFragmentProduceGLSL;
    densityFragmentProduceGLSL
        << "#version 450\n"
           "#extension GL_EXT_fragment_invocation_density : enable\n"
           "#extension GL_EXT_multiview : enable\n"
           "layout(location = 0) in vec4 inUV;\n"
           "layout(location = 1) in vec4 inColor;\n"
           "layout(location = 0) out vec4 fragColor;\n"
           "void main(void)\n"
           "{\n"
           "    fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
           "}\n";
    sourceCollections.glslSources.add("densitymap_frag_produce")
        << glu::FragmentSource(densityFragmentProduceGLSL.str());

    std::ostringstream densityFragmentCopyGLSL;
    densityFragmentCopyGLSL
        << "#version 450\n"
           "#extension GL_EXT_fragment_invocation_density : enable\n"
           "#extension GL_EXT_multiview : enable\n"
           "layout(location = 0) in vec4 inUV;\n"
           "layout(location = 1) in vec4 inColor;\n"
           "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputAtt;\n"
           "layout(location = 0) out vec4 fragColor;\n"
           "void main(void)\n"
           "{\n"
           "    fragColor = subpassLoad(inputAtt);\n"
           "}\n";
    sourceCollections.glslSources.add("densitymap_frag_copy") << glu::FragmentSource(densityFragmentCopyGLSL.str());

    std::ostringstream densityFragmentCopyMultisampledGLSL;
    densityFragmentCopyMultisampledGLSL
        << "#version 450\n"
           "#extension GL_EXT_fragment_invocation_density : enable\n"
           "#extension GL_EXT_multiview : enable\n"
           "layout(location = 0) in vec4 inUV;\n"
           "layout(location = 1) in vec4 inColor;\n"
           "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInputMS inputAtt;\n"
           "layout(location = 0) out vec4 fragColor;\n"
           "void main(void)\n"
           "{\n"
           "    fragColor = subpassLoad(inputAtt, gl_SampleID);\n"
           "}\n";
    sourceCollections.glslSources.add("densitymap_frag_copy_ms")
        << glu::FragmentSource(densityFragmentCopyMultisampledGLSL.str());

    std::ostringstream densityFragmentOutput2DGLSL;
    densityFragmentOutput2DGLSL << "#version 450\n"
                                   "layout(location = 0) in vec4 inUV;\n"
                                   "layout(location = 1) in vec4 inColor;\n"
                                   "layout(binding = 0)  uniform sampler2D subsampledImage;\n"
                                   "layout(location = 0) out vec4 fragColor;\n"
                                   "void main(void)\n"
                                   "{\n"
                                   "    fragColor = texture(subsampledImage, inUV.xy);\n"
                                   "}\n";
    sourceCollections.glslSources.add("densitymap_frag_output_2d")
        << glu::FragmentSource(densityFragmentOutput2DGLSL.str());

    std::ostringstream densityFragmentOutput2DArrayGLSL;
    densityFragmentOutput2DArrayGLSL << "#version 450\n"
                                        "layout(location = 0) in vec4 inUV;\n"
                                        "layout(location = 1) in vec4 inColor;\n"
                                        "layout(binding = 0)  uniform sampler2DArray subsampledImage;\n"
                                        "layout(location = 0) out vec4 fragColor;\n"
                                        "void main(void)\n"
                                        "{\n"
                                        "    fragColor = texture(subsampledImage, inUV.xyz);\n"
                                        "}\n";
    sourceCollections.glslSources.add("densitymap_frag_output_2darray")
        << glu::FragmentSource(densityFragmentOutput2DArrayGLSL.str());
}

TestInstance *FragmentDensityMapTest::createInstance(Context &context) const
{
    return new FragmentDensityMapTestInstance(context, m_testParams);
}

void FragmentDensityMapTest::checkSupport(Context &context) const
{
    const InstanceInterface &vki            = context.getInstanceInterface();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    context.requireDeviceFunctionality("VK_EXT_fragment_density_map");

    VkPhysicalDeviceFeatures2 features;
    deMemset(&features, 0, sizeof(VkPhysicalDeviceFeatures2));
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures;
    deMemset(&fragmentDensityMapFeatures, 0, sizeof(VkPhysicalDeviceFragmentDensityMapFeaturesEXT));
    fragmentDensityMapFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
    features.pNext                   = &fragmentDensityMapFeatures;

    vki.getPhysicalDeviceFeatures2(vkPhysicalDevice, &features);

    if (!fragmentDensityMapFeatures.fragmentDensityMap)
        TCU_THROW(NotSupportedError, "fragmentDensityMap feature is not supported");
    if (m_testParams.dynamicDensityMap && !fragmentDensityMapFeatures.fragmentDensityMapDynamic)
        TCU_THROW(NotSupportedError, "fragmentDensityMapDynamic feature is not supported");
    if (m_testParams.nonSubsampledImages && !fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages)
        TCU_THROW(NotSupportedError, "fragmentDensityMapNonSubsampledImages feature is not supported");
#endif
    if (m_testParams.multiView)
    {
        context.requireDeviceFunctionality("VK_KHR_multiview");
        const vk::VkPhysicalDeviceMultiviewFeatures &multiviewFeatures = context.getMultiviewFeatures();
        if (!multiviewFeatures.multiview)
            TCU_THROW(NotSupportedError, "Implementation does not support multiview feature");
    }
    {
        vk::VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (m_testParams.makeCopy)
            colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
        uint32_t colorImageCreateFlags =
            m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
#else
        uint32_t colorImageCreateFlags = 0u;
#endif
        VkImageFormatProperties imageFormatProperties(
            getPhysicalDeviceImageFormatProperties(vki, vkPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
                                                   VK_IMAGE_TILING_OPTIMAL, colorImageUsage, colorImageCreateFlags));
        if ((imageFormatProperties.sampleCounts & m_testParams.colorSamples) == 0)
            TCU_THROW(NotSupportedError, "Color image type not supported");
    }
}

FragmentDensityMapTestInstance::FragmentDensityMapTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
{
    const DeviceInterface &vk               = m_context.getDeviceInterface();
    const VkDevice vkDevice                 = m_context.getDevice();
    const VkPhysicalDevice vkPhysicalDevice = m_context.getPhysicalDevice();
    const uint32_t queueFamilyIndex         = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(vk, vkDevice,
                             getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), vkPhysicalDevice));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    {
        VkPhysicalDeviceProperties2 properties;
        deMemset(&properties, 0, sizeof(VkPhysicalDeviceProperties2));
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        deMemset(&m_fragmentDensityMapProperties, 0, sizeof(VkPhysicalDeviceFragmentDensityMapPropertiesEXT));
        m_fragmentDensityMapProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
        properties.pNext                     = &m_fragmentDensityMapProperties;

        context.getInstanceInterface().getPhysicalDeviceProperties2(vkPhysicalDevice, &properties);
    }
#else
    {
        m_fragmentDensityMapProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
        m_fragmentDensityMapProperties.minFragmentDensityTexelSize.width  = 1u;
        m_fragmentDensityMapProperties.maxFragmentDensityTexelSize.width  = 1u;
        m_fragmentDensityMapProperties.minFragmentDensityTexelSize.height = 1u;
        m_fragmentDensityMapProperties.maxFragmentDensityTexelSize.height = 1u;
        m_fragmentDensityMapProperties.fragmentDensityInvocations         = false;
        m_testParams.fragmentArea.x()                                     = 1u;
        m_testParams.fragmentArea.y()                                     = 1u;
    }
#endif

    // calculate all image sizes, image usage flags, view types etc.
    VkExtent3D densityMapImageSize{m_testParams.densityMapSize.x(), m_testParams.densityMapSize.y(), 1};
    uint32_t densityMapImageLayers       = m_testParams.multiView ? 2 : 1;
    VkImageViewType densityImageViewType = m_testParams.multiView ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    vk::VkImageUsageFlags densityMapImageUsage =
        m_testParams.dynamicDensityMap ?
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT :
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
#else
    vk::VkImageUsageFlags densityMapImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
#endif
    uint32_t densityMapImageViewCreateFlags =
        m_testParams.dynamicDensityMap ? (uint32_t)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT : 0u;

    VkExtent3D colorMapImageSize       = m_testParams.multiView ?
                                             VkExtent3D{m_testParams.renderSize.x() / 2, m_testParams.renderSize.y(), 1} :
                                             VkExtent3D{m_testParams.renderSize.x(), m_testParams.renderSize.y(), 1};
    uint32_t colorImageLayers          = m_testParams.multiView ? 2 : 1;
    VkImageViewType colorImageViewType = m_testParams.multiView ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    vk::VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_testParams.makeCopy)
        colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    uint32_t colorImageCreateFlags =
        m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
#else
    uint32_t colorImageCreateFlags             = 0u;
#endif

    VkExtent3D outputMapImageSize{m_testParams.renderSize.x(), m_testParams.renderSize.y(), 1};

    // Create subsampled color image
    prepareImageAndImageView(
        vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM, colorMapImageSize, colorImageLayers,
        m_testParams.colorSamples, colorImageUsage, queueFamilyIndex, 0u, colorImageViewType, componentMappingRGBA,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers}, m_colorImage, m_colorImageAlloc, m_colorImageView);

    // Create subsampled color image for resolve operation ( when multisampling is used )
    if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
    {
        prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM,
                                 colorMapImageSize, colorImageLayers, VK_SAMPLE_COUNT_1_BIT,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u,
                                 colorImageViewType, componentMappingRGBA,
                                 {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers}, m_colorResolvedImage,
                                 m_colorResolvedImageAlloc, m_colorResolvedImageView);
    }

    // create subsampled image copy
    if (m_testParams.makeCopy)
    {
        prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM,
                                 colorMapImageSize, colorImageLayers, m_testParams.colorSamples,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u,
                                 colorImageViewType, componentMappingRGBA,
                                 {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers}, m_colorCopyImage,
                                 m_colorCopyImageAlloc, m_colorCopyImageView);
    }

    // Create output image ( data from subsampled color image will be copied into it using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT )
    prepareImageAndImageView(
        vk, vkDevice, memAlloc, 0u, VK_FORMAT_R8G8B8A8_UNORM, outputMapImageSize, 1u, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, queueFamilyIndex, 0u,
        VK_IMAGE_VIEW_TYPE_2D, componentMappingRGBA, {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, m_outputImage,
        m_outputImageAlloc, m_outputImageView);

    // Create density map image
    prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, m_testParams.densityMapFormat, densityMapImageSize,
                             densityMapImageLayers, VK_SAMPLE_COUNT_1_BIT, densityMapImageUsage, queueFamilyIndex,
                             densityMapImageViewCreateFlags, densityImageViewType, componentMappingRGBA,
                             {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, densityMapImageLayers}, m_densityMapImage,
                             m_densityMapImageAlloc, m_densityMapImageView);

    // create and fill staging buffer, copy its data to density map image
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    if (!m_testParams.dynamicDensityMap)
    {
        tcu::TextureFormat densityMapTextureFormat = vk::mapVkFormat(m_testParams.densityMapFormat);
        VkDeviceSize stagingBufferSize = tcu::getPixelSize(densityMapTextureFormat) * densityMapImageSize.width *
                                         densityMapImageSize.height * densityMapImageLayers;
        const vk::VkBufferCreateInfo stagingBufferCreateInfo = {
            vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            DE_NULL,
            0u,                               // flags
            stagingBufferSize,                // size
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage
            vk::VK_SHARING_MODE_EXCLUSIVE,    // sharingMode
            0u,                               // queueFamilyCount
            DE_NULL,                          // pQueueFamilyIndices
        };
        vk::Move<vk::VkBuffer> stagingBuffer = vk::createBuffer(vk, vkDevice, &stagingBufferCreateInfo);
        const vk::VkMemoryRequirements stagingRequirements =
            vk::getBufferMemoryRequirements(vk, vkDevice, *stagingBuffer);
        de::MovePtr<vk::Allocation> stagingAllocation =
            memAlloc.allocate(stagingRequirements, MemoryRequirement::HostVisible);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *stagingBuffer, stagingAllocation->getMemory(),
                                     stagingAllocation->getOffset()));
        tcu::PixelBufferAccess stagingBufferAccess =
            tcu::PixelBufferAccess(densityMapTextureFormat, densityMapImageSize.width, densityMapImageSize.height,
                                   densityMapImageLayers, stagingAllocation->getHostPtr());

        tcu::Vec4 fragmentArea{1.0f / static_cast<float>(testParams.fragmentArea.x()),
                               1.0f / static_cast<float>(testParams.fragmentArea.y()), 0.0f, 1.0f};
        for (int z = 0; z < stagingBufferAccess.getDepth(); z++)
            for (int y = 0; y < stagingBufferAccess.getHeight(); y++)
                for (int x = 0; x < stagingBufferAccess.getWidth(); x++)
                    stagingBufferAccess.setPixel(fragmentArea, x, y, z);
        flushAlloc(vk, vkDevice, *stagingAllocation);

        std::vector<VkBufferImageCopy> copyRegions = {{
            0, // VkDeviceSize                    bufferOffset
            0, // uint32_t                        bufferRowLength
            0, // uint32_t                        bufferImageHeight
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
             densityMapImageLayers}, // VkImageSubresourceLayers        imageSubresource
            {0, 0, 0},               // VkOffset3D                    imageOffset
            densityMapImageSize      // VkExtent3D                    imageExtent
        }};

        vk::copyBufferToImage(
            vk, vkDevice, m_context.getUniversalQueue(), queueFamilyIndex, *stagingBuffer, stagingBufferSize,
            copyRegions, DE_NULL, VK_IMAGE_ASPECT_COLOR_BIT, 1, densityMapImageLayers, *m_densityMapImage,
            VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT);
    }
#endif

    // create a sampler that is able to read from subsampled image
    {
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
        uint32_t samplerCreateFlags =
            m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
#else
        uint32_t samplerCreateFlags = 0u;
#endif
        const struct VkSamplerCreateInfo samplerInfo
        {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,        // sType
                DE_NULL,                                  // pNext
                (VkSamplerCreateFlags)samplerCreateFlags, // flags
                VK_FILTER_NEAREST,                        // magFilter
                VK_FILTER_NEAREST,                        // minFilter
                VK_SAMPLER_MIPMAP_MODE_NEAREST,           // mipmapMode
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeU
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeV
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeW
                0.0f,                                     // mipLodBias
                VK_FALSE,                                 // anisotropyEnable
                1.0f,                                     // maxAnisotropy
                false,                                    // compareEnable
                VK_COMPARE_OP_ALWAYS,                     // compareOp
                0.0f,                                     // minLod
                0.0f,                                     // maxLod
                VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // borderColor
                VK_FALSE,                                 // unnormalizedCoords
        };
        m_colorSampler = createSampler(vk, vkDevice, &samplerInfo);
    }

    // Create render passes
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    if (testParams.dynamicDensityMap)
#endif
        m_renderPassProduceDynamicDensityMap =
            createRenderPassProduceDynamicDensityMap<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                                     SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice,
                                                                                                testParams);
    m_renderPassProduceSubsampledImage =
        createRenderPassProduceSubsampledImage<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                               SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);
    m_renderPassOutputSubsampledImage =
        createRenderPassOutputSubsampledImage<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                              SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);

    // Create framebuffers
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    if (testParams.dynamicDensityMap)
#endif
        m_framebufferProduceDynamicDensityMap =
            createFrameBuffer(vk, vkDevice, *m_renderPassProduceDynamicDensityMap, densityMapImageSize.width,
                              densityMapImageSize.height, {*m_densityMapImageView});

    std::vector<VkImageView> imageViewsProduceSubsampledImage;
    imageViewsProduceSubsampledImage.push_back(*m_colorImageView);
    if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
        imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
    if (testParams.makeCopy)
        imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    imageViewsProduceSubsampledImage.push_back(*m_densityMapImageView);
#endif
    m_framebufferProduceSubsampledImage =
        createFrameBuffer(vk, vkDevice, *m_renderPassProduceSubsampledImage, colorMapImageSize.width,
                          colorMapImageSize.height, imageViewsProduceSubsampledImage);

    m_framebufferOutputSubsampledImage =
        createFrameBuffer(vk, vkDevice, *m_renderPassOutputSubsampledImage, outputMapImageSize.width,
                          outputMapImageSize.height, {*m_outputImageView});

    // Create pipeline layout for subpasses that do not use any descriptors
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            DE_NULL,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayoutNoDescriptors = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Create pipeline layout for subpass that copies data
    if (m_testParams.makeCopy)
    {
        m_descriptorSetLayoutCopySubsampledImage =
            DescriptorSetLayoutBuilder()
                .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL)
                .build(vk, vkDevice);

        // Create and bind descriptor set
        m_descriptorPoolCopySubsampledImage =
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
                .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        m_pipelineLayoutCopySubsampledImage =
            makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutCopySubsampledImage);

        m_descriptorSetCopySubsampledImage = makeDescriptorSet(vk, vkDevice, *m_descriptorPoolCopySubsampledImage,
                                                               *m_descriptorSetLayoutCopySubsampledImage);

        const VkDescriptorImageInfo inputImageInfo = {
            DE_NULL,                                 // VkSampleri sampler;
            *m_colorImageView,                       // VkImageView imageView;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
        };
        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSetCopySubsampledImage, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &inputImageInfo)
            .update(vk, vkDevice);
    }

    // Create pipeline layout for last render pass ( output subsampled image )
    {
        m_descriptorSetLayoutOutputSubsampledImage =
            DescriptorSetLayoutBuilder()
                .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         &(m_colorSampler.get()))
                .build(vk, vkDevice);

        // Create and bind descriptor set
        m_descriptorPoolOutputSubsampledImage =
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u)
                .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        m_pipelineLayoutOutputSubsampledImage =
            makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutOutputSubsampledImage);

        m_descriptorSetOutputSubsampledImage = makeDescriptorSet(vk, vkDevice, *m_descriptorPoolOutputSubsampledImage,
                                                                 *m_descriptorSetLayoutOutputSubsampledImage);

        VkImageView srcImageView                   = (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT) ?
                                                         *m_colorResolvedImageView :
                                                         ((m_testParams.makeCopy) ? *m_colorCopyImageView : *m_colorImageView);
        const VkDescriptorImageInfo inputImageInfo = {
            DE_NULL,                                 // VkSampleri sampler;
            srcImageView,                            // VkImageView imageView;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
        };
        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSetOutputSubsampledImage, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &inputImageInfo)
            .update(vk, vkDevice);
    }

    // load vertex and fragment shaders

    m_vertexCommonShaderModule =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_vert"), 0);
    m_fragmentShaderModuleProduceSubsampledImage =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_produce"), 0);
    if (m_testParams.makeCopy)
    {
        if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
            m_fragmentShaderModuleCopySubsampledImage =
                createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_copy_ms"), 0);
        else
            m_fragmentShaderModuleCopySubsampledImage =
                createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_copy"), 0);
    }
    if (m_testParams.multiView)
        m_fragmentShaderModuleOutputSubsampledImage =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_output_2darray"), 0);
    else
        m_fragmentShaderModuleOutputSubsampledImage =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_output_2d"), 0);

    // Create pipelines
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
        };

        std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
            {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
            {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)(sizeof(float) * 4)},
            {2u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)(sizeof(float) * 8)}};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            static_cast<uint32_t>(vertexInputAttributeDescriptions.size()), // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions
                .data() // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            DE_NULL,                                   // const void*                                pNext
            (VkPipelineMultisampleStateCreateFlags)0u, // VkPipelineMultisampleStateCreateFlags    flags
            (VkSampleCountFlagBits)
                m_testParams.colorSamples, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,                      // VkBool32                                    sampleShadingEnable
            1.0f,                          // float                                    minSampleShading
            DE_NULL,                       // const VkSampleMask*                        pSampleMask
            VK_FALSE,                      // VkBool32                                    alphaToCoverageEnable
            VK_FALSE                       // VkBool32                                    alphaToOneEnable
        };

        const std::vector<VkViewport> viewportsProduceDynamicDensityMap{
            makeViewport(densityMapImageSize.width, densityMapImageSize.height)};
        const std::vector<VkRect2D> scissorsProduceDynamicDensityMap{
            makeRect2D(densityMapImageSize.width, densityMapImageSize.height)};
        const std::vector<VkViewport> viewportsProduceSubsampledImage{
            makeViewport(colorMapImageSize.width, colorMapImageSize.height)};
        const std::vector<VkRect2D> scissorsProduceSubsampledImage{
            makeRect2D(colorMapImageSize.width, colorMapImageSize.height)};
        const std::vector<VkViewport> viewportsCopySubsampledImage{
            makeViewport(colorMapImageSize.width, colorMapImageSize.height)};
        const std::vector<VkRect2D> scissorsCopySubsampledImage{
            makeRect2D(colorMapImageSize.width, colorMapImageSize.height)};
        const std::vector<VkViewport> viewportsOutputSubsampledImage{
            makeViewport(outputMapImageSize.width, outputMapImageSize.height)};
        const std::vector<VkRect2D> scissorsOutputSubsampledImage{
            makeRect2D(outputMapImageSize.width, outputMapImageSize.height)};

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
        if (testParams.dynamicDensityMap)
#endif
            m_graphicsPipelineProduceDynamicDensityMap = makeGraphicsPipeline(
                vk,                             // const DeviceInterface&                            vk
                vkDevice,                       // const VkDevice                                    device
                *m_pipelineLayoutNoDescriptors, // const VkPipelineLayout                            pipelineLayout
                *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
                DE_NULL, // const VkShaderModule                                tessellationControlModule
                DE_NULL, // const VkShaderModule                                tessellationEvalModule
                DE_NULL, // const VkShaderModule                                geometryShaderModule
                *m_fragmentShaderModuleProduceSubsampledImage, // const VkShaderModule                                fragmentShaderModule
                *m_renderPassProduceDynamicDensityMap, // const VkRenderPass                                renderPass
                viewportsProduceDynamicDensityMap,     // const std::vector<VkViewport>&                    viewports
                scissorsProduceDynamicDensityMap,      // const std::vector<VkRect2D>&                        scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,   // const VkPrimitiveTopology                        topology
                0u,                                    // const uint32_t                                    subpass
                0u,                       // const uint32_t                                    patchControlPoints
                &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo

        m_graphicsPipelineProduceSubsampledImage = makeGraphicsPipeline(
            vk,                             // const DeviceInterface&                            vk
            vkDevice,                       // const VkDevice                                    device
            *m_pipelineLayoutNoDescriptors, // const VkPipelineLayout                            pipelineLayout
            *m_vertexCommonShaderModule,    // const VkShaderModule                                vertexShaderModule
            DE_NULL, // const VkShaderModule                                tessellationControlModule
            DE_NULL, // const VkShaderModule                                tessellationEvalModule
            DE_NULL, // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModuleProduceSubsampledImage, // const VkShaderModule                                fragmentShaderModule
            *m_renderPassProduceSubsampledImage, // const VkRenderPass                                renderPass
            viewportsProduceSubsampledImage,     // const std::vector<VkViewport>&                    viewports
            scissorsProduceSubsampledImage,      // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
            0u,                                  // const uint32_t                                    subpass
            0u,                                  // const uint32_t                                    patchControlPoints
            &vertexInputStateParams, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
            DE_NULL,                 // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo,
            &multisampleStateCreateInfo); // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo

        if (m_testParams.makeCopy)
            m_graphicsPipelineCopySubsampledImage = makeGraphicsPipeline(
                vk,                                   // const DeviceInterface&                            vk
                vkDevice,                             // const VkDevice                                    device
                *m_pipelineLayoutCopySubsampledImage, // const VkPipelineLayout                            pipelineLayout
                *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
                DE_NULL, // const VkShaderModule                                tessellationControlModule
                DE_NULL, // const VkShaderModule                                tessellationEvalModule
                DE_NULL, // const VkShaderModule                                geometryShaderModule
                *m_fragmentShaderModuleCopySubsampledImage, // const VkShaderModule                                fragmentShaderModule
                *m_renderPassProduceSubsampledImage, // const VkRenderPass                                renderPass
                viewportsProduceSubsampledImage,     // const std::vector<VkViewport>&                    viewports
                scissorsProduceSubsampledImage,      // const std::vector<VkRect2D>&                        scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
                1u,                                  // const uint32_t                                    subpass
                0u,                      // const uint32_t                                    patchControlPoints
                &vertexInputStateParams, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
                DE_NULL, // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo,
                &multisampleStateCreateInfo); // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo

        m_graphicsPipelineOutputSubsampledImage = makeGraphicsPipeline(
            vk,                                     // const DeviceInterface&                            vk
            vkDevice,                               // const VkDevice                                    device
            *m_pipelineLayoutOutputSubsampledImage, // const VkPipelineLayout                            pipelineLayout
            *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
            DE_NULL, // const VkShaderModule                                tessellationControlModule
            DE_NULL, // const VkShaderModule                                tessellationEvalModule
            DE_NULL, // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModuleOutputSubsampledImage, // const VkShaderModule                                fragmentShaderModule
            *m_renderPassOutputSubsampledImage,  // const VkRenderPass                                renderPass
            viewportsOutputSubsampledImage,      // const std::vector<VkViewport>&                    viewports
            scissorsOutputSubsampledImage,       // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
            0u,                                  // const uint32_t                                    subpass
            0u,                                  // const uint32_t                                    patchControlPoints
            &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
    }

    // Create vertex buffers
    m_vertices       = createFullscreenQuadRG();
    m_verticesDDM    = createFullscreenQuadDensity(1.0f / static_cast<float>(m_testParams.fragmentArea.x()),
                                                   1.0f / static_cast<float>(m_testParams.fragmentArea.y()));
    m_verticesOutput = createFullscreenMeshOutput(m_testParams.multiView);

    createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_vertices, m_vertexBuffer, m_vertexBufferAlloc);
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    if (testParams.dynamicDensityMap)
#endif
        createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesDDM, m_vertexBufferDDM,
                           m_vertexBufferAllocDDM);
    createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesOutput, m_vertexBufferOutput,
                       m_vertexBufferOutputAlloc);

    // Create command pool and command buffer
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const typename RenderpassSubpass2::SubpassBeginInfo subpassBeginInfo(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass2::SubpassEndInfo subpassEndInfo(DE_NULL);
    const VkDeviceSize vertexBufferOffset              = 0;
    std::vector<VkClearValue> attachmentClearValuesDDM = {makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f)};
    std::vector<VkClearValue> attachmentClearValues    = {makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f)};
    if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
        attachmentClearValues.push_back(makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));
    if (testParams.makeCopy)
        attachmentClearValues.push_back(makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    // first render pass - render dynamic density map
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    if (testParams.dynamicDensityMap)
#endif
    {
        const VkRenderPassBeginInfo renderPassBeginInfoProduceDynamicDensityMap = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                          // VkStructureType sType;
            DE_NULL,                                                           // const void* pNext;
            *m_renderPassProduceDynamicDensityMap,                             // VkRenderPass renderPass;
            *m_framebufferProduceDynamicDensityMap,                            // VkFramebuffer framebuffer;
            makeRect2D(densityMapImageSize.width, densityMapImageSize.height), // VkRect2D renderArea;
            static_cast<uint32_t>(attachmentClearValuesDDM.size()),            // uint32_t clearValueCount;
            attachmentClearValuesDDM.data()                                    // const VkClearValue* pClearValues;
        };
        RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceDynamicDensityMap,
                                               &subpassBeginInfo);
        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceDynamicDensityMap);
        vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBufferDDM.get(), &vertexBufferOffset);
        vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_verticesDDM.size(), 1, 0, 0);
        RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
    }

    // render subsampled image
    const VkRenderPassBeginInfo renderPassBeginInfoProduceSubsampledImage = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                      // VkStructureType sType;
        DE_NULL,                                                       // const void* pNext;
        *m_renderPassProduceSubsampledImage,                           // VkRenderPass renderPass;
        *m_framebufferProduceSubsampledImage,                          // VkFramebuffer framebuffer;
        makeRect2D(colorMapImageSize.width, colorMapImageSize.height), // VkRect2D renderArea;
        static_cast<uint32_t>(attachmentClearValues.size()),           // uint32_t clearValueCount;
        attachmentClearValues.data()                                   // const VkClearValue* pClearValues;
    };

    RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceSubsampledImage,
                                           &subpassBeginInfo);
    vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceSubsampledImage);
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
    if (testParams.makeCopy)
    {
        RenderpassSubpass2::cmdNextSubpass(vk, *m_cmdBuffer, &subpassBeginInfo, &subpassEndInfo);
        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineCopySubsampledImage);
        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutCopySubsampledImage, 0,
                                 1, &m_descriptorSetCopySubsampledImage.get(), 0, DE_NULL);
        vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
        vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
    }
    RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

    // copy subsampled image to ordinary image using sampler that is able to read from subsampled images( subsampled image cannot be copied using vkCmdCopyImageToBuffer )
    const VkRenderPassBeginInfo renderPassBeginInfoOutputSubsampledImage = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                        // VkStructureType sType;
        DE_NULL,                                                         // const void* pNext;
        *m_renderPassOutputSubsampledImage,                              // VkRenderPass renderPass;
        *m_framebufferOutputSubsampledImage,                             // VkFramebuffer framebuffer;
        makeRect2D(outputMapImageSize.width, outputMapImageSize.height), // VkRect2D renderArea;
        static_cast<uint32_t>(attachmentClearValues.size()),             // uint32_t clearValueCount;
        attachmentClearValues.data()                                     // const VkClearValue* pClearValues;
    };
    RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoOutputSubsampledImage,
                                           &subpassBeginInfo);
    vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineOutputSubsampledImage);
    vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOutputSubsampledImage, 0,
                             1, &m_descriptorSetOutputSubsampledImage.get(), 0, DE_NULL);
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBufferOutput.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_verticesOutput.size(), 1, 0, 0);
    RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

    endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus FragmentDensityMapTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

struct Vec4Sorter
{
    bool operator()(const tcu::Vec4 &lhs, const tcu::Vec4 &rhs) const
    {
        if (lhs.x() != rhs.x())
            return lhs.x() < rhs.x();
        if (lhs.y() != rhs.y())
            return lhs.y() < rhs.y();
        if (lhs.z() != rhs.z())
            return lhs.z() < rhs.z();
        return lhs.w() < rhs.w();
    }
};

tcu::TestStatus FragmentDensityMapTestInstance::verifyImage(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    tcu::UVec2 renderSize{m_testParams.renderSize.x(), m_testParams.renderSize.y()};
    de::UniquePtr<tcu::TextureLevel> outputImage(pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex,
                                                                               memAlloc, *m_outputImage,
                                                                               VK_FORMAT_R8G8B8A8_UNORM, renderSize)
                                                     .release());
    const tcu::ConstPixelBufferAccess &outputAccess = outputImage->getAccess();
    tcu::TestLog &log                               = m_context.getTestContext().getLog();

    // log images
    log << tcu::TestLog::ImageSet("Result", "Result images")
        << tcu::TestLog::Image("Rendered", "Rendered output image", outputAccess) << tcu::TestLog::EndImageSet;

    uint32_t colorCountCoeff = m_testParams.multiView ? 2u : 1u;
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
    uint32_t estimatedColorCount = colorCountCoeff * m_testParams.fragmentArea.x() * m_testParams.fragmentArea.y();
#else
    uint32_t estimatedColorCount = colorCountCoeff;
#endif
    tcu::Vec2 density{1.0f / static_cast<float>(m_testParams.fragmentArea.x()),
                      1.0f / static_cast<float>(m_testParams.fragmentArea.y())};
    float densityMult = density.x() * density.y();

    // create histogram of all image colors, check the value of inverted FragSizeEXT
    std::map<tcu::Vec4, uint32_t, Vec4Sorter> colorCount;
    for (int y = 0; y < outputAccess.getHeight(); y++)
    {
        for (int x = 0; x < outputAccess.getWidth(); x++)
        {
            tcu::Vec4 outputColor = outputAccess.getPixel(x, y);
            float densityClamped  = outputColor.z() * outputColor.w();
            if ((densityClamped + 0.01) < densityMult)
                return tcu::TestStatus::fail("Wrong value of FragSizeEXT variable");
            auto it = colorCount.find(outputColor);
            if (it == end(colorCount))
                it = colorCount.insert({outputColor, 0u}).first;
            it->second++;
        }
    }

    // check if color count is the same as estimated one
    for (const auto &color : colorCount)
    {
        if (color.second > estimatedColorCount)
            return tcu::TestStatus::fail("Wrong color count");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createFragmentDensityMapTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> fdmTests(
        new tcu::TestCaseGroup(testCtx, "fragment_density_map", "VK_EXT_fragment_density_map extension tests"));

    const struct
    {
        std::string name;
        bool multiview;
    } views[] = {{"single_view", false}, {"multi_view", true}};

    const struct
    {
        std::string name;
        bool makeCopy;
    } renders[] = {{"render", false}, {"render_copy", true}};

    const struct
    {
        std::string name;
        float renderSizeToDensitySize;
    } sizes[] = {{"divisible_density_size", 4.0f}, {"non_divisible_density_size", 3.75f}};

    const struct
    {
        std::string name;
        VkSampleCountFlagBits samples;
    } samples[] = {{"1_sample", VK_SAMPLE_COUNT_1_BIT},
                   {"2_samples", VK_SAMPLE_COUNT_2_BIT},
                   {"4_samples", VK_SAMPLE_COUNT_4_BIT},
                   {"8_samples", VK_SAMPLE_COUNT_8_BIT}};

    std::vector<tcu::UVec2> fragmentArea{{1, 2}, {2, 1}, {2, 2}};

    for (const auto &view : views)
    {
        de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, view.name.c_str(), ""));
        for (const auto &render : renders)
        {
            de::MovePtr<tcu::TestCaseGroup> renderGroup(new tcu::TestCaseGroup(testCtx, render.name.c_str(), ""));
            for (const auto &size : sizes)
            {
                de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, size.name.c_str(), ""));
                for (const auto &sample : samples)
                {
                    de::MovePtr<tcu::TestCaseGroup> sampleGroup(
                        new tcu::TestCaseGroup(testCtx, sample.name.c_str(), ""));
                    for (const auto &area : fragmentArea)
                    {
                        std::stringstream str;
                        str << "_" << area.x() << "_" << area.y();
                        sampleGroup->addChild(
                            new FragmentDensityMapTest(testCtx, std::string("static_subsampled") + str.str(), "",
                                                       TestParams(false, false, view.multiview, render.makeCopy,
                                                                  size.renderSizeToDensitySize, sample.samples, area)));
                        sampleGroup->addChild(
                            new FragmentDensityMapTest(testCtx, std::string("dynamic_subsampled") + str.str(), "",
                                                       TestParams(true, false, view.multiview, render.makeCopy,
                                                                  size.renderSizeToDensitySize, sample.samples, area)));
                        sampleGroup->addChild(
                            new FragmentDensityMapTest(testCtx, std::string("static_nonsubsampled") + str.str(), "",
                                                       TestParams(false, true, view.multiview, render.makeCopy,
                                                                  size.renderSizeToDensitySize, sample.samples, area)));
                        sampleGroup->addChild(
                            new FragmentDensityMapTest(testCtx, std::string("dynamic_nonsubsampled") + str.str(), "",
                                                       TestParams(true, true, view.multiview, render.makeCopy,
                                                                  size.renderSizeToDensitySize, sample.samples, area)));
                    }
                    sizeGroup->addChild(sampleGroup.release());
                }
                renderGroup->addChild(sizeGroup.release());
            }
            viewGroup->addChild(renderGroup.release());
        }
        fdmTests->addChild(viewGroup.release());
    }
    return fdmTests.release();
}

} // namespace renderpass

} // namespace vkt
