/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief GPL Cache Collision Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineCacheGplTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktPipelineImageUtil.hpp"

#include <vector>
#include <memory>

namespace vkt
{
namespace pipeline
{

namespace
{
using namespace vk;

VkImageCreateInfo makeColorImageCreateInfo(const VkFormat format, const uint32_t width, const uint32_t height)
{
    const VkImageUsageFlags usage     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                             //  const void* pNext;
        (VkImageCreateFlags)0,               //  VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType imageType;
        format,                              //  VkFormat format;
        makeExtent3D(width, height, 1),      //  VkExtent3D extent;
        1u,                                  //  uint32_t mipLevels;
        1u,                                  //  uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling tiling;
        usage,                               //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode sharingMode;
        0u,                                  //  uint32_t queueFamilyIndexCount;
        nullptr,                             //  const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout initialLayout;
    };

    return imageInfo;
}

class GplCacheCollisionTestInstance : public TestInstance
{
public:
    GplCacheCollisionTestInstance(Context &context, PipelineConstructionType constructionType)
        : TestInstance(context)
        , m_constructionType(constructionType)
    {
    }

    virtual tcu::TestStatus iterate(void) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        const tcu::UVec2 renderSize(16u, 16u);
        const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

        // Create command pool for this instance
        Move<VkCommandPool> cmdPool =
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

        // 1. Create Cache
        VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
        Move<VkPipelineCache> cache               = createPipelineCache(vk, device, &cacheCreateInfo);

        // 2. Create Samplers
        // Make sure both samplers use the exact same settings (other than pNext) so that if the driver
        // ignores pNext during hashing, they will collide in the cache.
        VkSamplerCreateInfo baseSamplerInfo = initVulkanStructure();
        baseSamplerInfo.magFilter           = VK_FILTER_NEAREST;
        baseSamplerInfo.minFilter           = VK_FILTER_NEAREST;
        baseSamplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        baseSamplerInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        baseSamplerInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        baseSamplerInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        baseSamplerInfo.mipLodBias          = 0.0f;
        baseSamplerInfo.compareOp           = VK_COMPARE_OP_NEVER;
        baseSamplerInfo.minLod              = 0.0f;
        baseSamplerInfo.maxLod              = 1.0f;
        baseSamplerInfo.maxAnisotropy       = 1.0f;
        baseSamplerInfo.anisotropyEnable    = VK_FALSE;
        baseSamplerInfo.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        // YUV Sampler with YCbCr conversion
        VkSamplerYcbcrConversionCreateInfo ycbcrInfo = initVulkanStructure();
        ycbcrInfo.format                             = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

        ycbcrInfo.ycbcrModel    = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
        ycbcrInfo.ycbcrRange    = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
        ycbcrInfo.components    = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        ycbcrInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;
        ycbcrInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;
        ycbcrInfo.chromaFilter  = VK_FILTER_NEAREST;

        Move<VkSamplerYcbcrConversion> ycbcrConversion = createSamplerYcbcrConversion(vk, device, &ycbcrInfo);

        VkSamplerYcbcrConversionInfo samplerYcbcrInfo = initVulkanStructure();
        samplerYcbcrInfo.conversion                   = *ycbcrConversion;

        VkSamplerCreateInfo yuvSamplerInfo = baseSamplerInfo;
        yuvSamplerInfo.pNext               = &samplerYcbcrInfo;

        Move<VkSampler> yuvSampler = createSampler(vk, device, &yuvSamplerInfo);

        // RGB Sampler
        VkSamplerCreateInfo rgbSamplerInfo = baseSamplerInfo;
        Move<VkSampler> rgbSampler         = createSampler(vk, device, &rgbSamplerInfo);

        // 3. Create Descriptor Set Layouts
        DescriptorSetLayoutBuilder rgbSamplerLayoutBuilder;
        rgbSamplerLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, &rgbSampler.get());
        Move<VkDescriptorSetLayout> rgbSamplerLayout = rgbSamplerLayoutBuilder.build(vk, device);

        DescriptorSetLayoutBuilder yuvSamplerLayoutBuilder;
        yuvSamplerLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, &yuvSampler.get());
        Move<VkDescriptorSetLayout> yuvSamplerLayout = yuvSamplerLayoutBuilder.build(vk, device);

        // 4. Create Pipeline Layouts
        VkPipelineLayoutCreateInfo rgbLayoutInfo = initVulkanStructure();
        VkDescriptorSetLayout rgbLayoutRaw       = *rgbSamplerLayout;
        rgbLayoutInfo.setLayoutCount             = 1;
        rgbLayoutInfo.pSetLayouts                = &rgbLayoutRaw;

        VkPipelineLayoutCreateInfo yuvLayoutInfo = initVulkanStructure();
        VkDescriptorSetLayout yuvLayoutRaw       = *yuvSamplerLayout;
        yuvLayoutInfo.setLayoutCount             = 1;
        yuvLayoutInfo.pSetLayouts                = &yuvLayoutRaw;

        PipelineLayoutWrapper rgbPipelineLayout(m_constructionType, vk, device, &rgbLayoutInfo);
        PipelineLayoutWrapper yuvPipelineLayout(m_constructionType, vk, device, &yuvLayoutInfo);

        // 5. Create Render Pass
        RenderPassWrapper renderPass(m_constructionType, vk, device, colorFormat);

        m_context.getTestContext().getLog() << tcu::TestLog::Message << "RGB Sampler Handle: " << *rgbSampler << "\n"
                                            << "YUV Sampler Handle: " << *yuvSampler << "\n"
                                            << "RGB DS Layout Handle: " << *rgbSamplerLayout << "\n"
                                            << "YUV DS Layout Handle: " << *yuvSamplerLayout << "\n"
                                            << "RGB Pipeline Layout Handle: " << *rgbPipelineLayout << "\n"
                                            << "YUV Pipeline Layout Handle: " << *yuvPipelineLayout << "\n"
                                            << tcu::TestLog::EndMessage;

        // 7. Create Pipelines (GPL)
        std::vector<VkViewport> viewports(1, makeViewport(renderSize));
        std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

        Move<VkPipeline> libraryRGB, execRGB;
        Move<VkPipeline> libraryYUV, execYUV;

        // 6. Create Shaders
        Move<VkShaderModule> vertModule =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("color_vert"), 0);
        Move<VkShaderModule> fragModule =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("color_frag"), 0);

        // Helper to create a GPL library with pre-rasterization and fragment shaders, and link it into an executable pipeline.
        auto setupPipeline = [&](Move<VkPipeline> &outLibrary, Move<VkPipeline> &outExec,
                                 PipelineLayoutWrapper &libLayout, PipelineLayoutWrapper &linkLayout)
        {
            VkPipelineShaderStageCreateInfo stages[2] = {};
            stages[0]                                 = initVulkanStructure();
            stages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module                          = *vertModule;
            stages[0].pName                           = "main";

            stages[1]        = initVulkanStructure();
            stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = *fragModule;
            stages[1].pName  = "main";

            // Part 1: Create combined Pre-Raster + Frag Shader library
            VkGraphicsPipelineLibraryCreateInfoEXT library_info = initVulkanStructure();
            library_info.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
                                 VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

            VkPipelineViewportStateCreateInfo viewport_state = initVulkanStructure();
            viewport_state.viewportCount                     = 1;
            viewport_state.pViewports                        = viewports.data();
            viewport_state.scissorCount                      = 1;
            viewport_state.pScissors                         = scissors.data();

            VkPipelineRasterizationStateCreateInfo rasterization_state = initVulkanStructure();
            rasterization_state.polygonMode                            = VK_POLYGON_MODE_FILL;
            rasterization_state.cullMode                               = VK_CULL_MODE_NONE;
            rasterization_state.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization_state.lineWidth                              = 1.0f;

            VkPipelineDepthStencilStateCreateInfo depth_stencil_state = initVulkanStructure();

            VkPipelineMultisampleStateCreateInfo multisample_state = initVulkanStructure();
            multisample_state.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

            VkDynamicState dynamic_states[]               = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic_info = initVulkanStructure();
            dynamic_info.dynamicStateCount                = 2;
            dynamic_info.pDynamicStates                   = dynamic_states;

            VkGraphicsPipelineCreateInfo library_create_info = initVulkanStructure(&library_info);
            library_create_info.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
            if (m_constructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
                library_create_info.flags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
            library_create_info.stageCount          = 2;
            library_create_info.pStages             = stages;
            library_create_info.layout              = *libLayout;
            library_create_info.renderPass          = *renderPass;
            library_create_info.pViewportState      = &viewport_state;
            library_create_info.pRasterizationState = &rasterization_state;
            library_create_info.pDepthStencilState  = &depth_stencil_state;
            library_create_info.pMultisampleState   = &multisample_state;
            library_create_info.pDynamicState       = &dynamic_info;

            VkPipeline libraryHandle = VK_NULL_HANDLE;
            VK_CHECK(vk.createGraphicsPipelines(device, *cache, 1, &library_create_info, nullptr, &libraryHandle));
            outLibrary = Move<VkPipeline>(check<VkPipeline>(libraryHandle), Deleter<VkPipeline>(vk, device, nullptr));

            // Part 2: Executable Pipeline
            VkPipelineLibraryCreateInfoKHR linking_info = initVulkanStructure();
            linking_info.libraryCount                   = 1;
            VkPipeline lib                              = *outLibrary;
            linking_info.pLibraries                     = &lib;

            VkGraphicsPipelineLibraryCreateInfoEXT exec_library_info = initVulkanStructure(&linking_info);
            exec_library_info.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

            VkPipelineVertexInputStateCreateInfo vertex_input_state     = initVulkanStructure();
            VkPipelineInputAssemblyStateCreateInfo input_assembly_state = initVulkanStructure();
            input_assembly_state.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineColorBlendAttachmentState blend_attachment_state = {};
            blend_attachment_state.colorWriteMask                      = 0xf;
            VkPipelineColorBlendStateCreateInfo color_blend_state      = initVulkanStructure();
            color_blend_state.attachmentCount                          = 1;
            color_blend_state.pAttachments                             = &blend_attachment_state;

            VkGraphicsPipelineCreateInfo executable_pipeline_create_info = initVulkanStructure(&exec_library_info);
            if (m_constructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
                executable_pipeline_create_info.flags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
            executable_pipeline_create_info.layout              = *linkLayout;
            executable_pipeline_create_info.renderPass          = *renderPass;
            executable_pipeline_create_info.pVertexInputState   = &vertex_input_state;
            executable_pipeline_create_info.pInputAssemblyState = &input_assembly_state;
            executable_pipeline_create_info.pColorBlendState    = &color_blend_state;
            executable_pipeline_create_info.pMultisampleState   = &multisample_state;
            executable_pipeline_create_info.pDynamicState       = &dynamic_info;

            VkPipeline execHandle = VK_NULL_HANDLE;
            VK_CHECK(
                vk.createGraphicsPipelines(device, *cache, 1, &executable_pipeline_create_info, nullptr, &execHandle));
            outExec = Move<VkPipeline>(check<VkPipeline>(execHandle), Deleter<VkPipeline>(vk, device, nullptr));
        };

        // Create Pipeline 1 (RGB) -> Expect Cache Miss
        setupPipeline(libraryRGB, execRGB, rgbPipelineLayout, rgbPipelineLayout);

        // Create Pipeline 2 (YUV) -> Expect Cache Miss (Spec Compliant, No Collision)
        // We compile the libraries using YUV layout, and link using YUV layout.
        setupPipeline(libraryYUV, execYUV, yuvPipelineLayout, yuvPipelineLayout);

        // 8. Create Images and Views
        // Color output image
        VkImageCreateInfo colorImageInfo = makeColorImageCreateInfo(colorFormat, renderSize.x(), renderSize.y());
        ImageWithMemory colorImage(vk, device, allocator, colorImageInfo, MemoryRequirement::Any);
        Move<VkImageView> colorImageView =
            makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat,
                          makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

        // Create staging buffer to initialize YUV image
        const VkDeviceSize stagingBufferSize = 16 * 16 + 8 * 8 * 2; // 256 (Y) + 128 (UV)
        VkBufferCreateInfo stagingBufferInfo = initVulkanStructure();
        stagingBufferInfo.size               = stagingBufferSize;
        stagingBufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        Move<VkBuffer> stagingBuffer         = createBuffer(vk, device, &stagingBufferInfo);
        de::MovePtr<Allocation> stagingBufferAlloc =
            allocator.allocate(getBufferMemoryRequirements(vk, device, *stagingBuffer), MemoryRequirement::HostVisible);
        vk.bindBufferMemory(device, *stagingBuffer, stagingBufferAlloc->getMemory(), stagingBufferAlloc->getOffset());

        // Zero-fill staging buffer
        void *stagingPtr = stagingBufferAlloc->getHostPtr();
        deMemset(stagingPtr, 0, (size_t)stagingBufferSize);
        flushAlloc(vk, device, *stagingBufferAlloc);

        // Dummy YUV Image (Zero-filled to result in Green)
        VkImageCreateInfo yuvImageInfo = initVulkanStructure();
        yuvImageInfo.imageType         = VK_IMAGE_TYPE_2D;
        yuvImageInfo.format            = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        yuvImageInfo.extent            = {16u, 16u, 1u};
        yuvImageInfo.mipLevels         = 1u;
        yuvImageInfo.arrayLayers       = 1u;
        yuvImageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        yuvImageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        yuvImageInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ImageWithMemory yuvImage(vk, device, allocator, yuvImageInfo, MemoryRequirement::Any);

        VkImageViewCreateInfo yuvViewInfo = initVulkanStructure();
        yuvViewInfo.pNext                 = &samplerYcbcrInfo;
        yuvViewInfo.image                 = *yuvImage;
        yuvViewInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        yuvViewInfo.format                = yuvImageInfo.format;
        yuvViewInfo.components            = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        yuvViewInfo.subresourceRange      = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        Move<VkImageView> yuvImageView = createImageView(vk, device, &yuvViewInfo);

        // 9. Fill YUV Image (Transition to TRANSFER_DST_OPTIMAL, copy data, and transition to SHADER_READ_ONLY_OPTIMAL)
        {
            Move<VkCommandBuffer> cmd = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            beginCommandBuffer(vk, *cmd);

            // Transition to TRANSFER_DST_OPTIMAL
            VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
            VkImageMemoryBarrier barrier =
                makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *yuvImage, range);
            vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                  nullptr, 0, nullptr, 1, &barrier);

            // Copy regions
            VkBufferImageCopy regions[2] = {};
            // Plane 0 (Y)
            regions[0].bufferOffset                = 0;
            regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
            regions[0].imageSubresource.layerCount = 1;
            regions[0].imageExtent                 = {16u, 16u, 1u};
            // Plane 1 (UV)
            regions[1].bufferOffset                = 256;
            regions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
            regions[1].imageSubresource.layerCount = 1;
            regions[1].imageExtent                 = {8u, 8u, 1u};

            vk.cmdCopyBufferToImage(*cmd, *stagingBuffer, *yuvImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2u, regions);

            // Transition to SHADER_READ_ONLY_OPTIMAL
            barrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *yuvImage, range);
            vk.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                  nullptr, 0, nullptr, 1, &barrier);

            endCommandBuffer(vk, *cmd);
            submitCommandsAndWait(vk, device, queue, *cmd);
        }

        // 10. Create Framebuffer
        renderPass.createFramebuffer(vk, device, *colorImage, *colorImageView, renderSize.x(), renderSize.y());

        // 11. Create Descriptor Pool and Sets
        Move<VkDescriptorPool> descriptorPool =
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3u)
                .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        Move<VkDescriptorSet> yuvSamplerSet = makeDescriptorSet(vk, device, *descriptorPool, *yuvSamplerLayout);

        DescriptorSetUpdateBuilder builder;
        VkDescriptorImageInfo yuvImageInfoDesc =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *yuvImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        builder.writeSingle(*yuvSamplerSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &yuvImageInfoDesc);

        builder.update(vk, device);

        // 12. Draw with Pipeline 2 (YUV)
        {
            Move<VkCommandBuffer> cmd = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            beginCommandBuffer(vk, *cmd);

            renderPass.begin(vk, *cmd, makeRect2D(renderSize), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

            auto setDynamicStates = [&](VkCommandBuffer commandBuffer)
            {
                vk.cmdSetViewport(commandBuffer, 0, 1, viewports.data());
                vk.cmdSetScissor(commandBuffer, 0, 1, scissors.data());
            };

            m_context.getTestContext().getLog() << tcu::TestLog::Message << "RGB Pipeline Handle: " << *execRGB << "\n"
                                                << "YUV Pipeline Handle: " << *execYUV << "\n"
                                                << tcu::TestLog::EndMessage;

            // Draw YUV Quad Second
            vk.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *execYUV);
            VkDescriptorSet rawYuvSet = *yuvSamplerSet;
            vk.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, yuvPipelineLayout.get(), 0u, 1u, &rawYuvSet,
                                     0, nullptr);
            setDynamicStates(*cmd);
            vk.cmdDraw(*cmd, 3u, 1u, 0u, 0u);

            renderPass.end(vk, *cmd);
            endCommandBuffer(vk, *cmd);
            submitCommandsAndWait(vk, device, queue, *cmd);
        }

        // 13. Verify Result
        // Read back pixels
        de::MovePtr<tcu::TextureLevel> result =
            readColorAttachment(vk, device, queue, queueFamilyIndex, allocator, *colorImage, colorFormat, renderSize);

        const tcu::ConstPixelBufferAccess &access = result->getAccess();
        tcu::Vec4 pixel                           = access.getPixel(8, 8);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Image("Result", "Rendered Image", access) << tcu::TestLog::Message
            << "Expected pixel color: Green (~0.0, 0.53, 0.0)\n"
            << "Actual sampled pixel color at (8,8): " << pixel << tcu::TestLog::EndMessage;

        bool passed = (pixel.y() >= 0.2f);

        if (!passed)
        {
            return tcu::TestStatus::fail(
                "Pipeline Cache Collision Detected! The YUV Pipeline produced Black instead of Green.");
        }
        return tcu::TestStatus::pass("Pass");
    }

private:
    PipelineConstructionType m_constructionType;
};

class GplCacheCollisionTestCase : public TestCase
{
public:
    GplCacheCollisionTestCase(tcu::TestContext &testCtx, const std::string &name,
                              PipelineConstructionType constructionType)
        : TestCase(testCtx, name)
        , m_constructionType(constructionType)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const override
    {
        // Add simple shaders
        // Vertex shader generates a full screen triangle
        programCollection.glslSources.add("color_vert")
            << glu::VertexSource("#version 450\n"
                                 "layout (location = 0) out vec2 outUV;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
                                 "    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);\n"
                                 "}\n");

        programCollection.glslSources.add("color_frag")
            << glu::FragmentSource("#version 450\n"
                                   "layout (location = 0) in vec2 inUV;\n"
                                   "layout (set = 0, binding = 0) uniform sampler2D tex;\n"
                                   "layout (location = 0) out vec4 fragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    fragColor = texture(tex, inUV) * 0.99;\n"
                                   "}\n");
    }

    virtual void checkSupport(Context &context) const override
    {
        checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                              m_constructionType);

        // Check for standard YUV format support
        VkFormatProperties formatProps;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            context.getPhysicalDevice(), VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, &formatProps);
        const VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;
        bool hasStandardYuv = (formatProps.optimalTilingFeatures & requiredFeatures) == requiredFeatures;

        if (!hasStandardYuv)
            TCU_THROW(NotSupportedError, "Device does not support standard YUV format for sampling");

        context.requireDeviceFunctionality("VK_KHR_sampler_ycbcr_conversion");
        const auto &features = context.getSamplerYcbcrConversionFeatures();
        if (features.samplerYcbcrConversion == VK_FALSE)
            TCU_THROW(NotSupportedError, "samplerYcbcrConversion feature is not supported");
    }

    virtual TestInstance *createInstance(Context &context) const override
    {
        return new GplCacheCollisionTestInstance(context, m_constructionType);
    }

private:
    PipelineConstructionType m_constructionType;
};

} // anonymous namespace

tcu::TestCaseGroup *createGplCacheCollisionTests(tcu::TestContext &testCtx, PipelineConstructionType constructionType)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "gpl_collision"));

    group->addChild(new GplCacheCollisionTestCase(testCtx, "identical_shaders_different_samplers", constructionType));

    return group.release();
}

} // namespace pipeline
} // namespace vkt
