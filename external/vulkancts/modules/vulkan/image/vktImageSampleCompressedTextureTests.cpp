/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
 *
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
 * \brief Tests that images using a block-compressed format are sampled
 * correctly
 *
 * These tests create a storage image using a 128-bit or a 64-bit
 * block-compressed image format and an ImageView using an uncompressed
 * format. Each test case then fills the storage image with compressed
 * color values in a compute shader and samples the storage image. If the
 * sampled values are pure blue, the test passes.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "tcuVectorType.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{
using de::MovePtr;
using std::vector;
using tcu::ConstPixelBufferAccess;
using tcu::IVec3;
using tcu::PixelBufferAccess;
using tcu::TextureLevel;
using tcu::Vec2;
using tcu::Vec4;

const VkDeviceSize BUFFERSIZE = 100u * 1024;
const int WIDTH               = 80;
const int HEIGHT              = 80;

inline VkImageCreateInfo makeImageCreateInfo(const IVec3 &size, const VkFormat &format, bool storageImage)
{
    VkImageUsageFlags usageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateFlags createFlags = DE_NULL;

    if (storageImage)
    {
        usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT;
        createFlags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT |
                      VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
    }

    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,  //  VkStructureType         sType;
        DE_NULL,                              //  const void*             pNext;
        createFlags,                          //  VkImageCreateFlags      flags;
        VK_IMAGE_TYPE_2D,                     //  VkImageType             imageType;
        format,                               //  VkFormat                format;
        makeExtent3D(size.x(), size.y(), 1u), //  VkExtent3D              extent;
        1u,                                   //  uint32_t                mipLevels;
        1u,                                   //  uint32_t                arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                //  VkSampleCountFlagBits   samples;
        VK_IMAGE_TILING_OPTIMAL,              //  VkImageTiling           tiling;
        usageFlags,                           //  VkImageUsageFlags       usage;
        VK_SHARING_MODE_EXCLUSIVE,            //  VkSharingMode           sharingMode;
        0u,                                   //  uint32_t                queueFamilyIndexCount;
        DE_NULL,                              //  const uint32_t*         pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,            //  VkImageLayout           initialLayout;
    };

    return imageParams;
}

Move<VkBuffer> makeVertexBuffer(const DeviceInterface &vk, const VkDevice device, const uint32_t queueFamilyIndex)
{
    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType      sType;
        DE_NULL,                              // const void*          pNext;
        0u,                                   // VkBufferCreateFlags  flags;
        BUFFERSIZE,                           // VkDeviceSize         size;
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags   usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode;
        1u,                                   // uint32_t             queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t*      pQueueFamilyIndices;
    };

    Move<VkBuffer> vertexBuffer = createBuffer(vk, device, &vertexBufferParams);
    ;
    return vertexBuffer;
}

class SampleDrawnTextureTestInstance : public TestInstance
{
public:
    SampleDrawnTextureTestInstance(Context &context, const VkFormat imageFormat, const VkFormat imageViewFormat,
                                   const bool twoSamplers);
    tcu::TestStatus iterate(void);

private:
    const VkFormat m_imageFormat;
    const VkFormat m_imageViewFormat;
    const bool m_twoSamplers;
};

SampleDrawnTextureTestInstance::SampleDrawnTextureTestInstance(Context &context, const VkFormat imageFormat,
                                                               const VkFormat imageViewFormat, const bool twoSamplers)
    : TestInstance(context)
    , m_imageFormat(imageFormat)
    , m_imageViewFormat(imageViewFormat)
    , m_twoSamplers(twoSamplers)
{
}

template <typename T>
inline size_t sizeInBytes(const vector<T> &vec)
{
    return vec.size() * sizeof(vec[0]);
}

Move<VkSampler> makeSampler(const DeviceInterface &vk, const VkDevice &device)
{
    const VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType          sType;
        DE_NULL,                                 // const void*              pNext;
        (VkSamplerCreateFlags)0,                 // VkSamplerCreateFlags     flags;
        VK_FILTER_NEAREST,                       // VkFilter                 magFilter;
        VK_FILTER_NEAREST,                       // VkFilter                 minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode      mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode     addressModeU;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode     addressModeV;
        VK_SAMPLER_ADDRESS_MODE_REPEAT,          // VkSamplerAddressMode     addressModeW;
        0.0f,                                    // float                    mipLodBias;
        VK_FALSE,                                // VkBool32                 anisotropyEnable;
        1.0f,                                    // float                    maxAnisotropy;
        VK_FALSE,                                // VkBool32                 compareEnable;
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp              compareOp;
        0.0f,                                    // float                    minLod;
        0.0f,                                    // float                    maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor            borderColor;
        VK_FALSE,                                // VkBool32                 unnormalizedCoordinates;
    };

    return createSampler(vk, device, &samplerParams);
}

struct Vertex
{
    Vertex(Vec4 position_, Vec2 uv_) : position(position_), uv(uv_)
    {
    }
    Vec4 position;
    Vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription(void);
    static vector<VkVertexInputAttributeDescription> getAttributeDescriptions(void);
};

VkVertexInputBindingDescription Vertex::getBindingDescription(void)
{
    static const VkVertexInputBindingDescription desc = {
        0u,                                    // uint32_t             binding;
        static_cast<uint32_t>(sizeof(Vertex)), // uint32_t             stride;
        VK_VERTEX_INPUT_RATE_VERTEX,           // VkVertexInputRate    inputRate;
    };

    return desc;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions(void)
{
    static const vector<VkVertexInputAttributeDescription> desc = {
        {
            0u,                                                // uint32_t    location;
            0u,                                                // uint32_t    binding;
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,                 // VkFormat    format;
            static_cast<uint32_t>(offsetof(Vertex, position)), // uint32_t    offset;
        },
        {
            1u,                                          // uint32_t    location;
            0u,                                          // uint32_t    binding;
            vk::VK_FORMAT_R32G32_SFLOAT,                 // VkFormat    format;
            static_cast<uint32_t>(offsetof(Vertex, uv)), // uint32_t    offset;
        },
    };

    return desc;
}

// Generates the vertices of a full quad and texture coordinates of each vertex
vector<Vertex> generateVertices(void)
{
    vector<Vertex> vertices;
    vertices.push_back(Vertex(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec2(0.0f, 0.0f)));
    vertices.push_back(Vertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
    vertices.push_back(Vertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));
    vertices.push_back(Vertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f)));
    vertices.push_back(Vertex(Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec2(1.0f, 1.0f)));
    vertices.push_back(Vertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f)));

    return vertices;
}

// Generates a reference image filled with pure blue
TextureLevel makeReferenceImage(const VkFormat format, int width, int height)
{
    TextureLevel referenceImage(mapVkFormat(format), width, height, 1);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            referenceImage.getAccess().setPixel(tcu::IVec4(0, 0, 255, 255), x, y, 0);

    return referenceImage;
}

tcu::TestStatus SampleDrawnTextureTestInstance::iterate(void)
{
    DE_ASSERT(m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK || m_imageFormat == VK_FORMAT_BC3_UNORM_BLOCK);

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    Allocator &allocator            = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const IVec3 imageSize       = {static_cast<int>(WIDTH), HEIGHT, 1};
    const VkExtent2D renderSize = {uint32_t(WIDTH), uint32_t(HEIGHT)};
    const VkRect2D renderArea   = makeRect2D(makeExtent3D(WIDTH, HEIGHT, 1u));
    const vector<VkRect2D> scissors(1u, renderArea);
    const vector<VkViewport> viewports(1u, makeViewport(makeExtent3D(WIDTH, HEIGHT, 1u)));

    const Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 3u));
    const VkFormat renderedImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // Create a storage image. The first pipeline fills it with pure blue and the second pipeline
    // uses it as a sampling source.
    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(imageSize, m_imageFormat, true);
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
    const ImageWithMemory storageImage(vk, device, m_context.getDefaultAllocator(), imageCreateInfo,
                                       MemoryRequirement::Any);
    Move<VkImageView> storageImageImageView =
        makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, imageSubresourceRange);

    Move<VkShaderModule> computeShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u);

    // Build descriptors for the storage image
    const auto descriptorSetLayout1(DescriptorSetLayoutBuilder()
                                        .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                        .build(vk, device));
    const Unique<VkDescriptorSet> descriptorSet1(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout1));

    const VkDescriptorImageInfo storageImageDscrInfo =
        makeDescriptorImageInfo(DE_NULL, *storageImageImageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet1, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImageDscrInfo)
        .update(vk, device);

    // Create a compute pipeline
    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags    stageFlags;
        0u,                          // uint32_t              offset;
        (uint32_t)sizeof(uint32_t),  // uint32_t              size;
    };

    const Move<VkPipelineLayout> computePipelineLayout =
        makePipelineLayout(vk, device, 1, &(*descriptorSetLayout1), 1, &pushConstantRange);
    Move<VkPipeline> computePipeline = makeComputePipeline(vk, device, *computePipelineLayout, *computeShader);

    // The first sampler uses an uncompressed format
    const Unique<VkSampler> sampler(makeSampler(vk, device));
    Move<VkImageView> sampledImageView =
        makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageViewFormat, imageSubresourceRange);

    // The second sampler uses the same format as the image
    const Unique<VkSampler> sampler2(makeSampler(vk, device));

    VkImageUsageFlags usageFlags                  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageViewUsageCreateInfo imageViewUsageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, //VkStructureType sType;
        DE_NULL,                                        //const void* pNext;
        usageFlags,                                     //VkImageUsageFlags usage;
    };

    Move<VkImageView> sampledImageView2 = makeImageView(vk, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_imageFormat,
                                                        imageSubresourceRange, &imageViewUsageInfo);

    // Sampled values will be rendered on this image
    const VkImageSubresourceRange targetSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
    const VkImageCreateInfo targetImageCreateInfo = makeImageCreateInfo(imageSize, renderedImageFormat, false);
    const ImageWithMemory targetImage(vk, device, m_context.getDefaultAllocator(), targetImageCreateInfo,
                                      MemoryRequirement::Any);
    Move<VkImageView> targetImageView =
        makeImageView(vk, device, *targetImage, VK_IMAGE_VIEW_TYPE_2D, renderedImageFormat, targetSubresourceRange);

    // Clear the render target image as black and do a layout transition
    clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
                    targetImage.get(), Vec4(0, 0, 0, 0), VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // Build descriptors for the samplers
    const VkDescriptorImageInfo samplerDscrImageInfo =
        makeDescriptorImageInfo(sampler.get(), *sampledImageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorImageInfo samplerDscrImageInfo2 =
        makeDescriptorImageInfo(sampler2.get(), *sampledImageView2, VK_IMAGE_LAYOUT_GENERAL);

    const auto descriptorSetLayout2(DescriptorSetLayoutBuilder()
                                        .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                 VK_SHADER_STAGE_FRAGMENT_BIT, &sampler.get())
                                        .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                 VK_SHADER_STAGE_FRAGMENT_BIT, &sampler2.get())
                                        .build(vk, device));

    const Unique<VkDescriptorSet> descriptorSet2(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout2));

    if (m_twoSamplers)
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet2, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerDscrImageInfo2)
            .writeSingle(*descriptorSet2, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerDscrImageInfo)
            .update(vk, device);
    }
    else
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet2, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerDscrImageInfo2)
            .update(vk, device);
    }

    // Create a graphics pipeline layout
    const VkPushConstantRange pushConstantRange2 = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags    stageFlags;
        0u,                           // uint32_t              offset;
        (uint32_t)sizeof(uint32_t),   // uint32_t              size;
    };

    const Move<VkPipelineLayout> graphicsPipelineLayout =
        makePipelineLayout(vk, device, 1, &(*descriptorSetLayout2), 1, &pushConstantRange2);

    // Vertices for a full quad and texture coordinates for each vertex
    vector<Vertex> vertices     = generateVertices();
    Move<VkBuffer> vertexBuffer = makeVertexBuffer(vk, device, queueFamilyIndex);
    de::MovePtr<Allocation> vertexBufferAlloc =
        bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
    const VkDeviceSize vertexBufferOffset = 0ull;
    deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], sizeInBytes(vertices));
    flushAlloc(vk, device, *vertexBufferAlloc);

    const auto vtxBindingDescription = Vertex::getBindingDescription();
    const auto vtxAttrDescriptions   = Vertex::getAttributeDescriptions();

    const VkPipelineVertexInputStateCreateInfo vtxInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        0u,                                                        // VkPipelineVertexInputStateCreateFlags       flags
        1u,                     // uint32_t                                    vertexBindingDescriptionCount
        &vtxBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        static_cast<uint32_t>(
            vtxAttrDescriptions.size()), // uint32_t                                    vertexAttributeDescriptionCount
        vtxAttrDescriptions.data(),      // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    Move<VkShaderModule> vertexShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
    Move<VkShaderModule> fragmentShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Create a render pass, a framebuffer, and the second pipeline
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, renderedImageFormat, VK_FORMAT_UNDEFINED,
                                                   VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, targetImageView.get(), renderSize.width, renderSize.height);
    const Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, graphicsPipelineLayout.get(), vertexShader.get(), DE_NULL, DE_NULL, DE_NULL, fragmentShader.get(),
        renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vtxInputInfo);

    // Create a result buffer
    const VkBufferCreateInfo resultBufferCreateInfo =
        makeBufferCreateInfo(BUFFERSIZE, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Move<VkBuffer> resultBuffer = createBuffer(vk, device, &resultBufferCreateInfo);
    MovePtr<Allocation> resultBufferMemory =
        allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);
    TextureLevel resultImage(mapVkFormat(renderedImageFormat), renderSize.width, renderSize.height, 1);
    VK_CHECK(
        vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

    // Generate a reference image
    TextureLevel expectedImage = makeReferenceImage(renderedImageFormat, WIDTH, HEIGHT);

    beginCommandBuffer(vk, *cmdBuffer);

    // Do a layout transition for the storage image
    const auto barrier1 =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), imageSubresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                          DE_NULL, 0, DE_NULL, 1u, &barrier1);

    // Bind the descriptors and vertices
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                             &descriptorSet1.get(), 0u, DE_NULL);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0u, 1u,
                             &descriptorSet2.get(), 0u, DE_NULL);
    vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

    // Fill the storage image and sample it. The second pass should sample pure blue.
    for (int pass = 0; pass < 2; pass++)
    {
        // If both samplers are enabled, it's not necessary to run the compute shader twice since it already writes
        // pure blue on the first pass. The first sampler uses an uncompressed image format so the result image
        // will contain garbage if the second sampler doesn't work properly.
        if (!m_twoSamplers || pass == 0)
        {
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
            vk.cmdPushConstants(*cmdBuffer, *computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int32_t),
                                &pass);

            vk.cmdDispatch(*cmdBuffer, WIDTH, HEIGHT, 1u);

            const auto barrier2 =
                makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), imageSubresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier2);
        }

        vk.cmdPushConstants(*cmdBuffer, *graphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t),
                            &pass);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, imageSize.x(), imageSize.y()), 0u,
                        DE_NULL);
        vk.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
        endRenderPass(vk, *cmdBuffer);

        if (pass == 0)
        {
            const auto barrier3 =
                makeImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), imageSubresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &barrier3);

            const auto barrier4 =
                makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       targetImage.get(), targetSubresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u,
                                  &barrier4);
        }
    }

    // Copy the sampled values from the target image into the result image
    copyImageToBuffer(vk, *cmdBuffer, *targetImage, *resultBuffer, tcu::IVec2(WIDTH, HEIGHT),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, *resultBufferMemory);

    clear(resultImage.getAccess(), tcu::IVec4(0.));
    copy(resultImage.getAccess(),
         ConstPixelBufferAccess(resultImage.getFormat(), resultImage.getSize(), resultBufferMemory->getHostPtr()));

    // Each test case should render pure blue as the result
    bool result = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Image Comparison", "",
                                             expectedImage.getAccess(), resultImage.getAccess(), tcu::Vec4(0.01f),
                                             tcu::COMPARE_LOG_RESULT);

    if (result)
        return tcu::TestStatus::pass("pass");
    else
        return tcu::TestStatus::fail("fail");
}

class SampleDrawnTextureTest : public TestCase
{
public:
    SampleDrawnTextureTest(tcu::TestContext &testCtx, const std::string &name, const std::string &description,
                           const VkFormat imageFormat, const VkFormat imageViewFormat, const bool twoSamplers);

    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const VkFormat m_imageFormat;
    const VkFormat m_imageViewFormat;
    const bool m_twoSamplers;
};

SampleDrawnTextureTest::SampleDrawnTextureTest(tcu::TestContext &testCtx, const std::string &name,
                                               const std::string &description, const VkFormat imageFormat,
                                               const VkFormat imageViewFormat, const bool twoSamplers)
    : TestCase(testCtx, name, description)
    , m_imageFormat(imageFormat)
    , m_imageViewFormat(imageViewFormat)
    , m_twoSamplers(twoSamplers)
{
}

void SampleDrawnTextureTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();
    const auto usageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const bool haveMaintenance2 = context.isDeviceFunctionalitySupported("VK_KHR_maintenance2");

    // Check that:
    // - An image can be created with usage flags that are not supported by the image format
    //   but are supported by an image view created for the image.
    // - VkImageViewUsageCreateInfo can be used to override implicit usage flags derived from the image.
    if (!haveMaintenance2)
        TCU_THROW(NotSupportedError,
                  "Device does not support extended image usage flags nor overriding implicit usage flags");

    VkImageFormatProperties imageFormatProperties;

    if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_IMAGE_TYPE_2D,
                                                   VK_IMAGE_TILING_OPTIMAL, usageFlags, (VkImageCreateFlags)0,
                                                   &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "BC1 compressed texture formats not supported.");

    if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_BC3_UNORM_BLOCK, VK_IMAGE_TYPE_2D,
                                                   VK_IMAGE_TILING_OPTIMAL, usageFlags, (VkImageCreateFlags)0,
                                                   &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "BC3 compressed texture formats not supported.");
}

void SampleDrawnTextureTest::initPrograms(SourceCollections &programCollection) const
{
    // Pure blue and pure red compressed with the BC1 and BC3 algorithms.
    std::string bc1_red  = " uvec4(4160813056u, 0u, 4160813056u, 0u);\n";
    std::string bc1_blue = "uvec4(2031647, 0u, 2031647, 0u);\n";
    std::string bc3_red  = " uvec4(4294967295u, 4294967295u, 4160813056u, 0u);\n";
    std::string bc3_blue = "uvec4(4294967295u, 4294967295u, 2031647, 0u);\n";

    std::ostringstream computeSrc;
    computeSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
               << "layout(set = 0, binding = 0, rgba32ui) uniform highp uimage2D img;\n"
               << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
    if (!m_twoSamplers)
    {
        computeSrc << "layout(push_constant) uniform constants {\n"
                   << "     int pass;\n"
                   << "} pc;\n";
    }
    computeSrc << "void main() {\n";
    if (m_twoSamplers)
    {
        computeSrc << "    uvec4 color = ";
        m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK ? computeSrc << bc1_blue : computeSrc << bc3_blue;
    }
    else
    {
        computeSrc << "    uvec4 color = ";
        m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK ? computeSrc << bc1_red : computeSrc << bc3_red;

        computeSrc << "     if (pc.pass == 1) { \n";
        computeSrc << "        color = ";
        m_imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK ? computeSrc << bc1_blue : computeSrc << bc3_blue;
        computeSrc << "    }\n";
    }
    computeSrc << "    for (int x = 0; x < " << WIDTH << "; x++) {\n"
               << "        for (int y = 0; y < " << HEIGHT << "; y++) {\n"
               << "            imageStore(img, ivec2(x, y), color);\n"
               << "        }\n"
               << "    }\n"
               << "}\n";

    std::ostringstream vertexSrc;
    vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
              << "layout(location = 0) in highp vec4 a_position;\n"
              << "layout(location = 1) in vec2 inTexCoord;\n"
              << "layout(location = 1) out vec2 fragTexCoord;\n"
              << "void main (void) {\n"
              << "    gl_Position = a_position;\n"
              << "    fragTexCoord = inTexCoord;\n"
              << "}\n";

    std::ostringstream fragmentSrc;
    fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(location = 0) out vec4 outColor;\n"
                << "layout(location = 1) in vec2 fragTexCoord;\n"
                << "layout(binding = 0) uniform sampler2D compressedTexSampler;\n";
    if (m_twoSamplers)
    {
        fragmentSrc << "layout(binding = 1) uniform usampler2D texSampler;\n"
                    << "layout(push_constant) uniform constants {\n"
                    << "     int pass;\n"
                    << "} pc;\n";
    }
    fragmentSrc << "void main() {\n";
    if (m_twoSamplers)
    {
        fragmentSrc << "     if (pc.pass == 1) { \n"
                    << "         outColor = texture(compressedTexSampler, fragTexCoord);\n"
                    << "     } else {\n"
                    << "         outColor = texture(texSampler, fragTexCoord);\n"
                    << "     }";
    }
    else
        fragmentSrc << "outColor = texture(compressedTexSampler, fragTexCoord);\n";

    fragmentSrc << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(computeSrc.str());
    programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance *SampleDrawnTextureTest::createInstance(Context &context) const
{
    return new SampleDrawnTextureTestInstance(context, m_imageFormat, m_imageViewFormat, m_twoSamplers);
}

} // namespace

tcu::TestCaseGroup *createImageSampleDrawnTextureTests(tcu::TestContext &testCtx)
{
    /* If both samplers are enabled, the test works as follows:
     *
     * Pass 0:
     * - Compute shader fills a storage image with values that are pure blue compressed with
     *   either the BC1 or BC3 algorithm.
     * - Fragment shader samples the image and draws the values on a target image.
     * - As the sampled values are accessed through an image view using an uncompressed
     *   format, they remain compressed and the drawn image ends up being garbage.
     * Pass 1:
     * - Fragment shader samples the image. On this pass, the image view uses
     *   a block-compressed format and correctly interprets the sampled values.
     * - As the values are uncompressed now, the target image is filled
     *   with pure blue and the test passes.

     * Only one sampler enabled:
     * Pass 0:
     * - Compute shader fills a storage image with values that are pure red compressed
     *   with either the BC1 or BC3 algorithm.
     * - Fragment shader samples the image through an image view which interprets the values
     *   correctly. The values are drawn on a target image. The test doesn't pass yet
     *   since the image is red.
     * Pass 1:
     * - Compute shader fills the storage image with values that are pure blue compressed
     *   with the same algorithm as on the previous pass.
     * - Fragment shader samples the image through a image view which interprets the values
     *   correctly. The values are drawn on the target image and the test passes.
     */

    const bool twoSamplers = true;

    de::MovePtr<tcu::TestCaseGroup> testGroup(
        new tcu::TestCaseGroup(testCtx, "sample_texture", "Sample texture that has been rendered to tests"));

    testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format", "",
                                                   VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, !twoSamplers));
    testGroup->addChild(new SampleDrawnTextureTest(testCtx, "64_bit_compressed_format_two_samplers", "",
                                                   VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT, twoSamplers));
    testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format", "", VK_FORMAT_BC3_UNORM_BLOCK,
                                                   VK_FORMAT_R32G32B32A32_UINT, !twoSamplers));
    testGroup->addChild(new SampleDrawnTextureTest(testCtx, "128_bit_compressed_format_two_samplers", "",
                                                   VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT,
                                                   twoSamplers));

    return testGroup.release();
}

} // namespace image
} // namespace vkt
