/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
 * \brief Tests for VK_EXT_fragment_shader_interlock.
 * These tests render a set of overlapping full-screen quads that use image
 * or buffer reads and writes to accumulate values into a result image/buffer.
 * They use fragment shader interlock to avoid race conditions on the read/write
 * and validate that the final result includes all the writes.
 * Each fragment shader invocation computes a coordinate, and does a read/modify/write
 * into the image or buffer, inside the interlock. The value in memory accumulates a bitmask
 * indicating which primitives or samples have already run through the interlock. e.g.
 * for single sample, PIXEL_UNORDERED mode, there is one bit in the bitmask for each primitive
 * and each primitive ORs in its own bit. For PIXEL_ORDERED mode, each invocation also tests
 * that all the previous primitives (less significant bits) are also set, else it clobbers the
 * value. Sample and shading_rate interlock are variants of this where there is one value per
 * sample or per coarse fragment location, respectively. When there are multiple samples per
 * fragment, we merge in the whole sample mask. But within a pixel, we don't try to distinguish
 * primitive order between samples on the internal diagonal of the quad (triangle strip).
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShaderInterlockBasic.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace FragmentShaderInterlock
{
namespace
{
using namespace vk;
using namespace std;

typedef enum
{
    RES_SSBO = 0,
    RES_IMAGE,
} Resource;

typedef enum
{
    INT_PIXEL_ORDERED = 0,
    INT_PIXEL_UNORDERED,
    INT_SAMPLE_ORDERED,
    INT_SAMPLE_UNORDERED,
    INT_SHADING_RATE_ORDERED,
    INT_SHADING_RATE_UNORDERED,
} Interlock;

struct CaseDef
{
    uint32_t dim;
    Resource resType;
    Interlock interlock;
    VkSampleCountFlagBits samples;
    bool killOdd;
    bool sampleShading;

    bool isSampleInterlock() const
    {
        return sampleShading || interlock == INT_SAMPLE_ORDERED || interlock == INT_SAMPLE_UNORDERED;
    }
    bool isOrdered() const
    {
        return interlock == INT_PIXEL_ORDERED || interlock == INT_SAMPLE_ORDERED ||
               interlock == INT_SHADING_RATE_ORDERED;
    }
};

class FSITestInstance : public TestInstance
{
public:
    FSITestInstance(Context &context, const CaseDef &data);
    ~FSITestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef m_data;
};

FSITestInstance::FSITestInstance(Context &context, const CaseDef &data) : vkt::TestInstance(context), m_data(data)
{
}

FSITestInstance::~FSITestInstance(void)
{
}

class FSITestCase : public TestCase
{
public:
    FSITestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~FSITestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_data;
};

FSITestCase::FSITestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

FSITestCase::~FSITestCase(void)
{
}

void FSITestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_fragment_shader_interlock");

    if ((m_data.interlock == INT_SAMPLE_ORDERED || m_data.interlock == INT_SAMPLE_UNORDERED) &&
        !context.getFragmentShaderInterlockFeaturesEXT().fragmentShaderSampleInterlock)
    {
        TCU_THROW(NotSupportedError, "Fragment shader sample interlock not supported");
    }

    if ((m_data.interlock == INT_PIXEL_ORDERED || m_data.interlock == INT_PIXEL_UNORDERED) &&
        !context.getFragmentShaderInterlockFeaturesEXT().fragmentShaderPixelInterlock)
    {
        TCU_THROW(NotSupportedError, "Fragment shader pixel interlock not supported");
    }

#ifndef CTS_USES_VULKANSC
    if ((m_data.interlock == INT_SHADING_RATE_ORDERED || m_data.interlock == INT_SHADING_RATE_UNORDERED) &&
        !context.getFragmentShaderInterlockFeaturesEXT().fragmentShaderShadingRateInterlock)
    {
        TCU_THROW(NotSupportedError, "Fragment shader shading rate interlock not supported");
    }
    if ((m_data.interlock == INT_SHADING_RATE_ORDERED || m_data.interlock == INT_SHADING_RATE_UNORDERED) &&
        (!context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate ||
         !context.getFragmentShadingRateProperties().fragmentShadingRateWithFragmentShaderInterlock))
    {
        TCU_THROW(NotSupportedError, "fragment shading rate not supported");
    }
#endif // CTS_USES_VULKANSC

    if (m_data.isSampleInterlock())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

static int bitsPerQuad(const CaseDef &c)
{
    uint32_t bpq = c.samples;

    if (c.isSampleInterlock())
        bpq = 1;
    else if (c.interlock == INT_SHADING_RATE_ORDERED || c.interlock == INT_SHADING_RATE_UNORDERED)
        bpq *= 4;

    return bpq;
}

void FSITestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vss;

    vss << "#version 450 core\n"
           "layout(location = 0) out int primID;\n"
           "void main()\n"
           "{\n"
           "  primID = gl_InstanceIndex;\n"
           // full-viewport quad
           "  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * "
           "float(gl_VertexIndex&1), 1);\n"
           "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

    std::stringstream fss;

    fss << "#version 450 core\n"
           "#extension GL_ARB_fragment_shader_interlock : enable\n"
           "#extension GL_NV_shading_rate_image : enable\n"
           "layout(r32ui, set = 0, binding = 0) coherent uniform uimage2D image0;\n"
           "layout(std430, set = 0, binding = 1) coherent buffer B1 { uint x[]; } buf1;\n"
           "layout(location = 0) flat in int primID;\n";

    switch (m_data.interlock)
    {
    default:
        DE_ASSERT(0); // fallthrough
    case INT_PIXEL_ORDERED:
        fss << "layout(pixel_interlock_ordered) in;\n";
        break;
    case INT_PIXEL_UNORDERED:
        fss << "layout(pixel_interlock_unordered) in;\n";
        break;
    case INT_SAMPLE_ORDERED:
        fss << "layout(sample_interlock_ordered) in;\n";
        break;
    case INT_SAMPLE_UNORDERED:
        fss << "layout(sample_interlock_unordered) in;\n";
        break;
    case INT_SHADING_RATE_ORDERED:
        fss << "layout(shading_rate_interlock_ordered) in;\n";
        break;
    case INT_SHADING_RATE_UNORDERED:
        fss << "layout(shading_rate_interlock_unordered) in;\n";
        break;
    }

    // Each fragment shader invocation computes a coordinate, and does a read/modify/write
    // into the image or buffer, inside the interlock. The value in memory accumulates a bitmask
    // indicating which primitives or samples have already run through the interlock. e.g.
    // for single sample, PIXEL_UNORDERED mode, there is one bit in the bitmask for each primitive
    // and each primitive ORs in its own bit. For PIXEL_ORDERED mode, each invocation also tests
    // that all the previous primitives (less significant bits) are also set, else it clobbers the
    // value. Sample and shading_rate interlock are variants of this where there is one value per
    // sample or per coarse fragment location, respectively. When there are multiple samples per
    // fragment, we merge in the whole sample mask. But within a pixel, we don't try to distinguish
    // primitive order between samples on the internal diagonal of the quad (triangle strip).

    fss << "void main()\n"
           "{\n"
           "  ivec2 coordxy = ivec2(gl_FragCoord.xy);\n"
           "  uint stride = "
        << m_data.dim
        << ";\n"
           "  uint bitsPerQuad = "
        << bitsPerQuad(m_data) << ";\n";

    // Compute the coordinate
    if (m_data.isSampleInterlock())
    {
        // Spread samples out in the x dimension
        fss << "  coordxy.x = coordxy.x * " << m_data.samples << " + gl_SampleID;\n";
        fss << "  stride *= " << m_data.samples << ";\n";
    }
    else if (m_data.interlock == INT_SHADING_RATE_ORDERED || m_data.interlock == INT_SHADING_RATE_UNORDERED)
    {
        // shading rate is 2x2. Divide xy by 2
        fss << "  coordxy /= 2;\n";
        fss << "  stride /= 2;\n";
    }

    if (m_data.isSampleInterlock())
    {
        // sample interlock runs per-sample, and stores one bit per sample
        fss << "  uint mask = 1 << primID;\n";
        fss << "  uint previousMask = (1 << primID)-1;\n";
    }
    else
    {
        // pixel and shading_rate interlock run per-fragment, and store the sample mask
        fss << "  uint mask = gl_SampleMaskIn[0] << (primID * bitsPerQuad);\n";
        fss << "  uint previousMask = (1 << (primID * bitsPerQuad))-1;\n";
    }

    // Exercise discard before and during the interlock
    if (m_data.killOdd)
        fss << "  if (coordxy.y < " << m_data.dim / 4 << " && (coordxy.x & 1) != 0) discard;\n";

    fss << "  beginInvocationInterlockARB();\n";

    if (m_data.killOdd)
        fss << "  if ((coordxy.x & 1) != 0) discard;\n";

    // Read the current value from the image or buffer
    if (m_data.resType == RES_IMAGE)
        fss << "  uint temp = imageLoad(image0, coordxy).x;\n";
    else
    {
        fss << "  uint coord = coordxy.y * stride + coordxy.x;\n";
        fss << "  uint temp = buf1.x[coord];\n";
    }

    // Update the value. For "ordered" modes, check that all the previous primitives'
    // bits are already set
    if (m_data.isOrdered())
        fss << "  if ((temp & previousMask) == previousMask) temp |= mask; else temp = 0;\n";
    else
        fss << "  temp |= mask;\n";

    // Store out the new value
    if (m_data.resType == RES_IMAGE)
        fss << "  imageStore(image0, coordxy, uvec4(temp, 0, 0, 0));\n";
    else
        fss << "  buf1.x[coord] = temp;\n";

    fss << "  endInvocationInterlockARB();\n";

    if (m_data.killOdd)
        fss << "  discard;\n";

    fss << "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource(fss.str());
}

TestInstance *FSITestCase::createInstance(Context &context) const
{
    return new FSITestInstance(context, m_data);
}

tcu::TestStatus FSITestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    VkFlags allShaderStages   = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkFlags allPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    Move<vk::VkDescriptorSetLayout> descriptorSetLayout;
    Move<vk::VkDescriptorPool> descriptorPool;
    Move<vk::VkDescriptorSet> descriptorSet;

    VkDescriptorPoolCreateFlags poolCreateFlags        = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

    const VkDescriptorSetLayoutBinding bindings[2] = {
        {
            0u,                               // binding
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // descriptorType
            1u,                               // descriptorCount
            allShaderStages,                  // stageFlags
            nullptr,                          // pImmutableSamplers
        },
        {
            1u,                                // binding
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
            1u,                                // descriptorCount
            allShaderStages,                   // stageFlags
            nullptr,                           // pImmutableSamplers
        },
    };

    // Create a layout and allocate a descriptor set for it.
    const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
        nullptr,                                                 // pNext
        layoutCreateFlags,                                       // flags
        2u,                                                      // bindingCount
        &bindings[0]                                             // pBindings
    };

    descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(bindings[0].descriptorType, 1);
    poolBuilder.addType(bindings[1].descriptorType, 1);

    descriptorPool = poolBuilder.build(vk, device, poolCreateFlags, 1u);
    descriptorSet  = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    // one uint per sample (max of 4 samples)
    VkDeviceSize bufferSize = m_data.dim * m_data.dim * sizeof(uint32_t) * 4;

    de::MovePtr<BufferWithMemory> buffer;
    buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    flushAlloc(vk, device, buffer->getAllocation());

    const VkQueue queue             = getDeviceQueue(vk, device, m_context.getUniversalQueueFamilyIndex(), 0);
    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        1,                          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        0u,                         // pushConstantRangeCount
        nullptr,                    // pPushConstantRanges
    };

    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

    de::MovePtr<BufferWithMemory> copyBuffer;
    copyBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             MemoryRequirement::HostVisible | MemoryRequirement::Cached));

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_R32_UINT,                  // VkFormat format;
        {
            m_data.dim * m_data.samples, // uint32_t width;
            m_data.dim,                  // uint32_t height;
            1u                           // uint32_t depth;
        },                               // VkExtent3D extent;
        1u,                              // uint32_t mipLevels;
        1u,                              // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,           // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,         // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        VK_NULL_HANDLE,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        VK_FORMAT_R32_UINT,                       // VkFormat format;
        {
            VK_COMPONENT_SWIZZLE_R, // VkComponentSwizzle r;
            VK_COMPONENT_SWIZZLE_G, // VkComponentSwizzle g;
            VK_COMPONENT_SWIZZLE_B, // VkComponentSwizzle b;
            VK_COMPONENT_SWIZZLE_A  // VkComponentSwizzle a;
        },                          // VkComponentMapping  components;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t baseMipLevel;
            1u,                        // uint32_t levelCount;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        }                              // VkImageSubresourceRange subresourceRange;
    };

    de::MovePtr<ImageWithMemory> image;
    Move<VkImageView> imageView;

    image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    imageViewCreateInfo.image = **image;
    imageView                 = createImageView(vk, device, &imageViewCreateInfo, NULL);

    VkDescriptorImageInfo imageInfo   = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(**buffer, 0, bufferSize);

    VkWriteDescriptorSet w = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType
        nullptr,                                // pNext
        *descriptorSet,                         // dstSet
        (uint32_t)0,                            // dstBinding
        0,                                      // dstArrayElement
        1u,                                     // descriptorCount
        bindings[0].descriptorType,             // descriptorType
        &imageInfo,                             // pImageInfo
        &bufferInfo,                            // pBufferInfo
        nullptr,                                // pTexelBufferView
    };
    vk.updateDescriptorSets(device, 1, &w, 0, NULL);

    w.dstBinding     = 1;
    w.descriptorType = bindings[1].descriptorType;
    vk.updateDescriptorSets(device, 1, &w, 0, NULL);

    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, nullptr);

    VkBool32 shadingRateEnable =
        m_data.interlock == INT_SHADING_RATE_ORDERED || m_data.interlock == INT_SHADING_RATE_UNORDERED ? VK_TRUE :
                                                                                                         VK_FALSE;

    Move<VkPipeline> pipeline;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    {
        const vk::VkSubpassDescription subpassDesc = {
            (vk::VkSubpassDescriptionFlags)0,
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
            0u,                                  // inputCount
            nullptr,                             // pInputAttachments
            0u,                                  // colorCount
            nullptr,                             // pColorAttachments
            nullptr,                             // pResolveAttachments
            nullptr,                             // depthStencilAttachment
            0u,                                  // preserveCount
            nullptr,                             // pPreserveAttachments
        };
        const vk::VkRenderPassCreateInfo renderPassParams = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkRenderPassCreateFlags)0,
            0u,           // attachmentCount
            nullptr,      // pAttachments
            1u,           // subpassCount
            &subpassDesc, // pSubpasses
            0u,           // dependencyCount
            nullptr,      // pDependencies
        };

        renderPass = createRenderPass(vk, device, &renderPassParams);

        const vk::VkFramebufferCreateInfo framebufferParams = {
            vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkFramebufferCreateFlags)0,
            *renderPass, // renderPass
            0u,          // attachmentCount
            nullptr,     // pAttachments
            m_data.dim,  // width
            m_data.dim,  // height
            1u,          // layers
        };

        framebuffer = createFramebuffer(vk, device, &framebufferParams);

        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
            0u,                                                        // uint32_t vertexBindingDescriptionCount;
            nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            0u,      // uint32_t vertexAttributeDescriptionCount;
            nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,       // VkPrimitiveTopology topology;
            VK_FALSE                                    // VkBool32 primitiveRestartEnable;
        };

        const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            (VkPipelineRasterizationStateCreateFlags)0, // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                                   // VkBool32 depthClampEnable;
            VK_FALSE,                                   // VkBool32 rasterizerDiscardEnable;
            VK_POLYGON_MODE_FILL,                       // VkPolygonMode polygonMode;
            VK_CULL_MODE_NONE,                          // VkCullModeFlags cullMode;
            VK_FRONT_FACE_CLOCKWISE,                    // VkFrontFace frontFace;
            VK_FALSE,                                   // VkBool32 depthBiasEnable;
            0.0f,                                       // float depthBiasConstantFactor;
            0.0f,                                       // float depthBiasClamp;
            0.0f,                                       // float depthBiasSlopeFactor;
            1.0f                                        // float lineWidth;
        };

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                               // const void*                                pNext
            0u,                                    // VkPipelineMultisampleStateCreateFlags    flags
            (VkSampleCountFlagBits)m_data.samples, // VkSampleCountFlagBits                    rasterizationSamples
            m_data.sampleShading ? VK_TRUE :
                                   VK_FALSE, // VkBool32                                    sampleShadingEnable
            1.0f,                            // float                                    minSampleShading
            nullptr,                         // const VkSampleMask*                        pSampleMask
            VK_FALSE,                        // VkBool32                                    alphaToCoverageEnable
            VK_FALSE                         // VkBool32                                    alphaToOneEnable
        };

        VkViewport viewport = makeViewport(m_data.dim, m_data.dim);
        VkRect2D scissor    = makeRect2D(m_data.dim, m_data.dim);

        VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            {2, 2},                                                                 // VkExtent2D fragmentSize;
            {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
             VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR}, // VkFragmentShadingRateCombinerOpKHR combinerOps[2];
        };

        const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                                               // const void*                                pNext
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags        flags
            1u,        // uint32_t                                    viewportCount
            &viewport, // const VkViewport*                        pViewports
            1u,        // uint32_t                                    scissorCount
            &scissor   // const VkRect2D*                            pScissors
        };

        Move<VkShaderModule> fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);
        Move<VkShaderModule> vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
        uint32_t numStages      = 2u;

        const VkPipelineShaderStageCreateInfo shaderCreateInfo[2] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_VERTEX_BIT, // stage
                *vs,                        // shader
                "main",
                nullptr, // pSpecializationInfo
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_FRAGMENT_BIT, // stage
                *fs,                          // shader
                "main",
                nullptr, // pSpecializationInfo
            }};

        const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,           // VkStructureType sType;
            shadingRateEnable ? &shadingRateStateCreateInfo : nullptr, // const void* pNext;
            (VkPipelineCreateFlags)0,                                  // VkPipelineCreateFlags flags;
            numStages,                                                 // uint32_t stageCount;
            &shaderCreateInfo[0],          // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                       // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            nullptr,                       // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            nullptr,                       // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout.get(),          // VkPipelineLayout layout;
            renderPass.get(),              // VkRenderPass renderPass;
            0u,                            // uint32_t subpass;
            VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
            0                              // int basePipelineIndex;
        };

        pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
    }

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                               nullptr,                                // const void*            pNext
                                               0u,                           // VkAccessFlags        srcAccessMask
                                               VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        dstAccessMask
                                               VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout        oldLayout
                                               VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout        newLayout
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                               **image,                 // VkImage                image
                                               {
                                                   VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                   0u,                        // uint32_t                baseMipLevel
                                                   1u,                        // uint32_t                mipLevels,
                                                   0u,                        // uint32_t                baseArray
                                                   1u,                        // uint32_t                arraySize
                                               }};

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

    VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkClearValue clearColor       = makeClearValueColorU32(0, 0, 0, 0);

    VkMemoryBarrier memBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
        nullptr,                          // pNext
        0u,                               // srcAccessMask
        0u,                               // dstAccessMask
    };

    vk.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

    vk.cmdFillBuffer(*cmdBuffer, **buffer, 0, bufferSize, 0);

    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages, 0, 1, &memBarrier, 0, nullptr,
                          0, nullptr);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_data.dim, m_data.dim), 0, nullptr,
                    VK_SUBPASS_CONTENTS_INLINE);

    // Draw N fullscreen "quads", one per instance.
    uint32_t N             = 32 / bitsPerQuad(m_data);
    uint32_t expectedValue = 0xFFFFFFFF;
    vk.cmdDraw(*cmdBuffer, 4u, N, 0u, 0u);

    endRenderPass(vk, *cmdBuffer);

    memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memBarrier, 0, nullptr,
                          0, nullptr);

    uint32_t copyDimX = m_data.dim;
    uint32_t copyDimY = m_data.dim;

    if (m_data.isSampleInterlock())
        copyDimX *= m_data.samples;

    if (shadingRateEnable)
    {
        copyDimX /= 2;
        copyDimY /= 2;
    }

    if (m_data.resType == RES_IMAGE)
    {
        const VkBufferImageCopy copyRegion = makeBufferImageCopy(
            makeExtent3D(copyDimX, copyDimY, 1u), makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
        vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);
    }
    else
    {
        const VkBufferCopy copyRegion = makeBufferCopy(0u, 0u, copyDimX * copyDimY * sizeof(uint32_t));
        vk.cmdCopyBuffer(*cmdBuffer, **buffer, **copyBuffer, 1, &copyRegion);
    }

    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memBarrier, 0,
                          nullptr, 0, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    uint32_t *ptr = (uint32_t *)copyBuffer->getAllocation().getHostPtr();
    invalidateAlloc(vk, device, copyBuffer->getAllocation());

    qpTestResult res = QP_TEST_RESULT_PASS;

    for (uint32_t i = 0; i < copyDimX * copyDimY; ++i)
    {
        if (m_data.killOdd && (i & 1))
        {
            if (ptr[i] != 0)
                res = QP_TEST_RESULT_FAIL;
        }
        else if (ptr[i] != expectedValue)
            res = QP_TEST_RESULT_FAIL;
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

} // namespace

tcu::TestCaseGroup *createBasicTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "basic"));

    typedef struct
    {
        uint32_t count;
        const char *name;
    } TestGroupCase;

    TestGroupCase dimCases[] = {
        {8, "8x8"},       {16, "16x16"},    {32, "32x32"},    {64, "64x64"},
        {128, "128x128"}, {256, "256x256"}, {512, "512x512"}, {1024, "1024x1024"},
    };

    TestGroupCase resCases[] = {
        {RES_IMAGE, "image"},
        {RES_SSBO, "ssbo"},
    };

    TestGroupCase killCases[] = {
        {0, "nodiscard"},
        {1, "discard"},
    };

    TestGroupCase sampCases[] = {
        {1, "1xaa"},
        {4, "4xaa"},
    };

    TestGroupCase ssCases[] = {
        {0, "no_sample_shading"},
        {1, "sample_shading"},
    };

    TestGroupCase intCases[] = {
        {INT_PIXEL_ORDERED, "pixel_ordered"},
        {INT_PIXEL_UNORDERED, "pixel_unordered"},
        {INT_SAMPLE_ORDERED, "sample_ordered"},
        {INT_SAMPLE_UNORDERED, "sample_unordered"},
#ifndef CTS_USES_VULKANSC
        {INT_SHADING_RATE_ORDERED, "shading_rate_ordered"},
        {INT_SHADING_RATE_UNORDERED, "shading_rate_unordered"},
#endif // CTS_USES_VULKANSC
    };

    for (int killNdx = 0; killNdx < DE_LENGTH_OF_ARRAY(killCases); killNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> killGroup(new tcu::TestCaseGroup(testCtx, killCases[killNdx].name));
        for (int resNdx = 0; resNdx < DE_LENGTH_OF_ARRAY(resCases); resNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> resGroup(new tcu::TestCaseGroup(testCtx, resCases[resNdx].name));
            for (int intNdx = 0; intNdx < DE_LENGTH_OF_ARRAY(intCases); intNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> intGroup(new tcu::TestCaseGroup(testCtx, intCases[intNdx].name));
                for (int sampNdx = 0; sampNdx < DE_LENGTH_OF_ARRAY(sampCases); sampNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> sampGroup(new tcu::TestCaseGroup(testCtx, sampCases[sampNdx].name));
                    for (int ssNdx = 0; ssNdx < DE_LENGTH_OF_ARRAY(ssCases); ssNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> ssGroup(new tcu::TestCaseGroup(testCtx, ssCases[ssNdx].name));
                        for (int dimNdx = 0; dimNdx < DE_LENGTH_OF_ARRAY(dimCases); dimNdx++)
                        {
                            CaseDef c = {
                                dimCases[dimNdx].count,                          // uint32_t set;
                                (Resource)resCases[resNdx].count,                // Resource resType;
                                (Interlock)intCases[intNdx].count,               // Interlock interlock;
                                (VkSampleCountFlagBits)sampCases[sampNdx].count, // VkSampleCountFlagBits samples;
                                (bool)killCases[killNdx].count,                  // bool killOdd;
                                (bool)ssCases[ssNdx].count,                      // bool sampleShading;
                            };

                            if (c.sampleShading && c.samples == 1)
                                continue;

                            ssGroup->addChild(new FSITestCase(testCtx, dimCases[dimNdx].name, c));
                        }
                        sampGroup->addChild(ssGroup.release());
                    }
                    intGroup->addChild(sampGroup.release());
                }
                resGroup->addChild(intGroup.release());
            }
            killGroup->addChild(resGroup.release());
        }
        group->addChild(killGroup.release());
    }
    return group.release();
}

} // namespace FragmentShaderInterlock
} // namespace vkt
