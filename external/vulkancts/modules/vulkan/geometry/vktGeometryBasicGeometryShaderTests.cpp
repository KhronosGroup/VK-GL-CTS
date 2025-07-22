/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Basic Geometry Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktGeometryBasicGeometryShaderTests.hpp"
#include "vktGeometryBasicClass.hpp"
#include "vktGeometryTestsUtil.hpp"

#include "gluTextureUtil.hpp"
#include "glwEnums.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace geometry
{
namespace
{
using de::MovePtr;
using std::string;
using std::vector;
using tcu::TestCaseGroup;
using tcu::TestContext;
using tcu::TestStatus;

enum VaryingSource
{
    READ_ATTRIBUTE = 0,
    READ_UNIFORM,
    READ_TEXTURE,

    READ_LAST
};
enum ShaderInstancingMode
{
    MODE_WITHOUT_INSTANCING = 0,
    MODE_WITH_INSTANCING,

    MODE_LAST
};
enum
{
    EMIT_COUNT_VERTEX_0 = 6,
    EMIT_COUNT_VERTEX_1 = 0,
    EMIT_COUNT_VERTEX_2 = -1,
    EMIT_COUNT_VERTEX_3 = 10,
};
enum VariableTest
{
    TEST_POINT_SIZE = 0,
    TEST_PRIMITIVE_ID_IN,
    TEST_PRIMITIVE_ID,
    TEST_LAST
};

void uploadImage(Context &context, const tcu::ConstPixelBufferAccess &access, VkImage destImage)
{
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkDevice device               = context.getDevice();
    const uint32_t queueFamilyIndex     = context.getUniversalQueueFamilyIndex();
    const VkQueue queue                 = context.getUniversalQueue();
    Allocator &memAlloc                 = context.getDefaultAllocator();
    const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    const uint32_t bufferSize =
        access.getWidth() * access.getHeight() * access.getDepth() * access.getFormat().getPixelSize();
    Move<VkBuffer> buffer;
    de::MovePtr<Allocation> bufferAlloc;
    Move<VkCommandPool> cmdPool;
    Move<VkCommandBuffer> cmdBuffer;
    Move<VkFence> fence;

    // Create source buffer
    {
        const VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };
        buffer = createBuffer(vk, device, &bufferParams);
        bufferAlloc =
            memAlloc.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
        VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
    }

    // Get copy regions and write buffer data
    const VkBufferImageCopy copyRegion = {
        0u,                           // VkDeviceSize bufferOffset;
        (uint32_t)access.getWidth(),  // uint32_t bufferRowLength;
        (uint32_t)access.getHeight(), // uint32_t bufferImageHeight;
        {
            // VkImageSubresourceLayers imageSubresource;
            aspectMask,   // VkImageAspectFlags aspectMask;
            (uint32_t)0u, // uint32_t mipLevel;
            (uint32_t)0u, // uint32_t baseArrayLayer;
            1u            // uint32_t layerCount;
        },
        {0u, 0u, 0u}, // VkOffset3D imageOffset;
        {             // VkExtent3D imageExtent;
         (uint32_t)access.getWidth(), (uint32_t)access.getHeight(), (uint32_t)access.getDepth()}};

    vector<VkBufferImageCopy> copyRegions(1, copyRegion);

    {
        const tcu::PixelBufferAccess destAccess(access.getFormat(), access.getSize(), bufferAlloc->getHostPtr());
        tcu::copy(destAccess, access);
        flushAlloc(vk, device, *bufferAlloc);
    }

    // Copy buffer to image
    copyBufferToImage(vk, device, queue, queueFamilyIndex, *buffer, bufferSize, copyRegions, nullptr, aspectMask, 1, 1,
                      destImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

class GeometryOutputCountTestInstance : public GeometryExpanderRenderTestInstance
{
public:
    GeometryOutputCountTestInstance(Context &context, const VkPrimitiveTopology primitiveType, const int primitiveCount,
                                    const char *name);
    void genVertexAttribData(void);

private:
    const int m_primitiveCount;
};

GeometryOutputCountTestInstance::GeometryOutputCountTestInstance(Context &context,
                                                                 const VkPrimitiveTopology primitiveType,
                                                                 const int primitiveCount, const char *name)
    : GeometryExpanderRenderTestInstance(context, primitiveType, name)
    , m_primitiveCount(primitiveCount)

{
    genVertexAttribData();
}

void GeometryOutputCountTestInstance::genVertexAttribData(void)
{
    m_vertexPosData.resize(m_primitiveCount);
    m_vertexAttrData.resize(m_primitiveCount);

    for (int ndx = 0; ndx < m_primitiveCount; ++ndx)
    {
        m_vertexPosData[ndx]  = tcu::Vec4(-1.0f, ((float)ndx) / (float)m_primitiveCount * 2.0f - 1.0f, 0.0f, 1.0f);
        m_vertexAttrData[ndx] = (ndx % 2 == 0) ? tcu::Vec4(1, 1, 1, 1) : tcu::Vec4(1, 0, 0, 1);
    }
    m_numDrawVertices = m_primitiveCount;
}

class VaryingOutputCountTestInstance : public GeometryExpanderRenderTestInstance
{
public:
    VaryingOutputCountTestInstance(Context &context, const char *name, const VkPrimitiveTopology primitiveType,
                                   const VaryingSource test, const ShaderInstancingMode mode);
    void genVertexAttribData(void);

protected:
    Move<VkPipelineLayout> createPipelineLayout(const DeviceInterface &vk, const VkDevice device);
    void bindDescriptorSets(const DeviceInterface &vk, const VkDevice device, Allocator &memAlloc,
                            const VkCommandBuffer &cmdBuffer, const VkPipelineLayout &pipelineLayout);

private:
    void genVertexDataWithoutInstancing(void);
    void genVertexDataWithInstancing(void);

    const VaryingSource m_test;
    const ShaderInstancingMode m_mode;
    const int32_t m_maxEmitCount;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkBuffer> m_buffer;
    Move<VkImage> m_texture;
    Move<VkImageView> m_imageView;
    Move<VkSampler> m_sampler;
    de::MovePtr<Allocation> m_allocation;
};

VaryingOutputCountTestInstance::VaryingOutputCountTestInstance(Context &context, const char *name,
                                                               const VkPrimitiveTopology primitiveType,
                                                               const VaryingSource test,
                                                               const ShaderInstancingMode mode)
    : GeometryExpanderRenderTestInstance(context, primitiveType, name)
    , m_test(test)
    , m_mode(mode)
    , m_maxEmitCount(128)
{
    genVertexAttribData();
}

void VaryingOutputCountTestInstance::genVertexAttribData(void)
{
    if (m_mode == MODE_WITHOUT_INSTANCING)
        genVertexDataWithoutInstancing();
    else if (m_mode == MODE_WITH_INSTANCING)
        genVertexDataWithInstancing();
    else
        DE_ASSERT(false);
}

Move<VkPipelineLayout> VaryingOutputCountTestInstance::createPipelineLayout(const DeviceInterface &vk,
                                                                            const VkDevice device)
{
    if (m_test == READ_UNIFORM)
    {
        m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                    .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT)
                                    .build(vk, device);
        m_descriptorPool = DescriptorPoolBuilder()
                               .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                               .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

        return makePipelineLayout(vk, device, *m_descriptorSetLayout);
    }
    else if (m_test == READ_TEXTURE)
    {
        const tcu::Vec4 data[4] = {tcu::Vec4(255, 0, 0, 0), tcu::Vec4(0, 255, 0, 0), tcu::Vec4(0, 0, 255, 0),
                                   tcu::Vec4(0, 0, 0, 255)};
        const tcu::UVec2 viewportSize(4, 1);
        const tcu::TextureFormat texFormat      = glu::mapGLInternalFormat(GL_RGBA8);
        const VkFormat format                   = mapTextureFormat(texFormat);
        const VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        Allocator &memAlloc                     = m_context.getDefaultAllocator();
        tcu::TextureLevel texture(texFormat, static_cast<int>(viewportSize.x()), static_cast<int>(viewportSize.y()));

        // Fill with data
        {
            tcu::PixelBufferAccess access = texture.getAccess();
            for (int x = 0; x < texture.getWidth(); ++x)
                access.setPixel(data[x], x, 0);
        }
        // Create image
        const VkImageCreateInfo imageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0,                                   // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            format,                              // VkFormat format;
            {
                // VkExtent3D extent;
                viewportSize.x(),
                viewportSize.y(),
                1u,
            },
            1u,                        // uint32_t mipLevels;
            1u,                        // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,     // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,   // VkImageTiling tiling;
            imageUsageFlags,           // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            0u,                        // uint32_t queueFamilyIndexCount;
            nullptr,                   // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout initialLayout;
        };

        m_texture    = createImage(vk, device, &imageParams);
        m_allocation = memAlloc.allocate(getImageMemoryRequirements(vk, device, *m_texture), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(device, *m_texture, m_allocation->getMemory(), m_allocation->getOffset()));
        uploadImage(m_context, texture.getAccess(), *m_texture);

        m_descriptorSetLayout =
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_GEOMETRY_BIT)
                .build(vk, device);
        m_descriptorPool = DescriptorPoolBuilder()
                               .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                               .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

        return makePipelineLayout(vk, device, *m_descriptorSetLayout);
    }
    else
        return makePipelineLayout(vk, device);
}

void VaryingOutputCountTestInstance::bindDescriptorSets(const DeviceInterface &vk, const VkDevice device,
                                                        Allocator &memAlloc, const VkCommandBuffer &cmdBuffer,
                                                        const VkPipelineLayout &pipelineLayout)
{
    if (m_test == READ_UNIFORM)
    {
        const int32_t emitCount[4] = {6, 0, m_maxEmitCount, 10};
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(sizeof(emitCount), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        m_buffer = createBuffer(vk, device, &bufferCreateInfo);
        m_allocation =
            memAlloc.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
        {
            deMemcpy(m_allocation->getHostPtr(), &emitCount[0], sizeof(emitCount));
            flushAlloc(vk, device, *m_allocation);

            const VkDescriptorBufferInfo bufferDescriptorInfo =
                makeDescriptorBufferInfo(*m_buffer, 0ull, sizeof(emitCount));

            DescriptorSetUpdateBuilder()
                .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferDescriptorInfo)
                .update(vk, device);
            vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u,
                                     &*m_descriptorSet, 0u, nullptr);
        }
    }
    else if (m_test == READ_TEXTURE)
    {
        const tcu::TextureFormat texFormat      = glu::mapGLInternalFormat(GL_RGBA8);
        const VkFormat format                   = mapTextureFormat(texFormat);
        const VkSamplerCreateInfo samplerParams = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0u,                                      // VkSamplerCreateFlags flags;
            VK_FILTER_NEAREST,                       // VkFilter magFilter;
            VK_FILTER_NEAREST,                       // VkFilter minFilter;
            VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
            0.0f,                                    // float mipLodBias;
            VK_FALSE,                                // VkBool32 anisotropyEnable;
            1.0f,                                    // float maxAnisotropy;
            false,                                   // VkBool32 compareEnable;
            VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
            0.0f,                                    // float minLod;
            0.0f,                                    // float maxLod;
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
            false                                    // VkBool32 unnormalizedCoordinates;
        };
        m_sampler                              = createSampler(vk, device, &samplerParams);
        const VkImageViewCreateInfo viewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            NULL,                                     // const voide* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_texture,                               // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            format,                                   // VkFormat format;
            makeComponentMappingRGBA(),               // VkChannelMapping channels;
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t baseMipLevel;
                1u,                        // uint32_t mipLevels;
                0,                         // uint32_t baseArraySlice;
                1u                         // uint32_t arraySize;
            },                             // VkImageSubresourceRange subresourceRange;
        };
        m_imageView = createImageView(vk, device, &viewParams);
        const VkDescriptorImageInfo descriptorImageInfo =
            makeDescriptorImageInfo(*m_sampler, *m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo)
            .update(vk, device);
        vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u, &*m_descriptorSet,
                                 0u, nullptr);
    }
}

void VaryingOutputCountTestInstance::genVertexDataWithoutInstancing(void)
{
    m_numDrawVertices = 4;
    m_vertexPosData.resize(m_numDrawVertices);
    m_vertexAttrData.resize(m_numDrawVertices);

    m_vertexPosData[0] = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    m_vertexPosData[1] = tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f);
    m_vertexPosData[2] = tcu::Vec4(-0.7f, -0.1f, 0.0f, 1.0f);
    m_vertexPosData[3] = tcu::Vec4(-0.1f, -0.7f, 0.0f, 1.0f);

    if (m_test == READ_ATTRIBUTE)
    {
        m_vertexAttrData[0] = tcu::Vec4(
            ((EMIT_COUNT_VERTEX_0 == -1) ? ((float)m_maxEmitCount) : ((float)EMIT_COUNT_VERTEX_0)), 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[1] = tcu::Vec4(
            ((EMIT_COUNT_VERTEX_1 == -1) ? ((float)m_maxEmitCount) : ((float)EMIT_COUNT_VERTEX_1)), 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[2] = tcu::Vec4(
            ((EMIT_COUNT_VERTEX_2 == -1) ? ((float)m_maxEmitCount) : ((float)EMIT_COUNT_VERTEX_2)), 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[3] = tcu::Vec4(
            ((EMIT_COUNT_VERTEX_3 == -1) ? ((float)m_maxEmitCount) : ((float)EMIT_COUNT_VERTEX_3)), 0.0f, 0.0f, 0.0f);
    }
    else
    {
        m_vertexAttrData[0] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[2] = tcu::Vec4(2.0f, 0.0f, 0.0f, 0.0f);
        m_vertexAttrData[3] = tcu::Vec4(3.0f, 0.0f, 0.0f, 0.0f);
    }
}

void VaryingOutputCountTestInstance::genVertexDataWithInstancing(void)
{
    m_numDrawVertices = 1;
    m_vertexPosData.resize(m_numDrawVertices);
    m_vertexAttrData.resize(m_numDrawVertices);

    m_vertexPosData[0] = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

    if (m_test == READ_ATTRIBUTE)
    {
        const int emitCounts[] = {
            (EMIT_COUNT_VERTEX_0 == -1) ? (m_maxEmitCount) : (EMIT_COUNT_VERTEX_0),
            (EMIT_COUNT_VERTEX_1 == -1) ? (m_maxEmitCount) : (EMIT_COUNT_VERTEX_1),
            (EMIT_COUNT_VERTEX_2 == -1) ? (m_maxEmitCount) : (EMIT_COUNT_VERTEX_2),
            (EMIT_COUNT_VERTEX_3 == -1) ? (m_maxEmitCount) : (EMIT_COUNT_VERTEX_3),
        };

        m_vertexAttrData[0] =
            tcu::Vec4((float)emitCounts[0], (float)emitCounts[1], (float)emitCounts[2], (float)emitCounts[3]);
    }
    else
    {
        // not used
        m_vertexAttrData[0] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

class BuiltinVariableRenderTestInstance : public GeometryExpanderRenderTestInstance
{
public:
    BuiltinVariableRenderTestInstance(Context &context, const char *name, const VariableTest test,
                                      const bool indicesTest);
    void genVertexAttribData(void);
    void createIndicesBuffer(void);

protected:
    void drawCommand(const VkCommandBuffer &cmdBuffer);

private:
    const bool m_indicesTest;
    std::vector<uint16_t> m_indices;
    Move<vk::VkBuffer> m_indicesBuffer;
    MovePtr<Allocation> m_allocation;
};

BuiltinVariableRenderTestInstance::BuiltinVariableRenderTestInstance(Context &context, const char *name,
                                                                     const VariableTest test, const bool indicesTest)
    : GeometryExpanderRenderTestInstance(
          context, (test == TEST_PRIMITIVE_ID_IN) ? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
          name)
    , m_indicesTest(indicesTest)
{
    genVertexAttribData();
}

void BuiltinVariableRenderTestInstance::genVertexAttribData(void)
{
    m_numDrawVertices = 5;

    m_vertexPosData.resize(m_numDrawVertices);
    m_vertexPosData[0] = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    m_vertexPosData[1] = tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f);
    m_vertexPosData[2] = tcu::Vec4(-0.7f, -0.1f, 0.0f, 1.0f);
    m_vertexPosData[3] = tcu::Vec4(-0.1f, -0.7f, 0.0f, 1.0f);
    m_vertexPosData[4] = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);

    m_vertexAttrData.resize(m_numDrawVertices);
    m_vertexAttrData[0] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[2] = tcu::Vec4(2.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[3] = tcu::Vec4(3.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[4] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (m_indicesTest)
    {
        // Only used by primitive ID restart test
        m_indices.resize(m_numDrawVertices);
        m_indices[0] = 1;
        m_indices[1] = 4;
        m_indices[2] = 0xFFFF; // restart
        m_indices[3] = 2;
        m_indices[4] = 1;
        createIndicesBuffer();
    }
}

void BuiltinVariableRenderTestInstance::createIndicesBuffer(void)
{
    // Create vertex indices buffer
    const DeviceInterface &vk                  = m_context.getDeviceInterface();
    const VkDevice device                      = m_context.getDevice();
    Allocator &memAlloc                        = m_context.getDefaultAllocator();
    const VkDeviceSize indexBufferSize         = m_indices.size() * sizeof(uint16_t);
    const VkBufferCreateInfo indexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        indexBufferSize,                      // VkDeviceSize size;
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyCount;
        nullptr                               // const uint32_t* pQueueFamilyIndices;
    };

    m_indicesBuffer = createBuffer(vk, device, &indexBufferParams);
    m_allocation =
        memAlloc.allocate(getBufferMemoryRequirements(vk, device, *m_indicesBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(device, *m_indicesBuffer, m_allocation->getMemory(), m_allocation->getOffset()));
    // Load indices into buffer
    deMemcpy(m_allocation->getHostPtr(), &m_indices[0], (size_t)indexBufferSize);
    flushAlloc(vk, device, *m_allocation);
}

void BuiltinVariableRenderTestInstance::drawCommand(const VkCommandBuffer &cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    if (m_indicesTest)
    {
        vk.cmdBindIndexBuffer(cmdBuffer, *m_indicesBuffer, 0, VK_INDEX_TYPE_UINT16);
        vk.cmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
    }
    else
        vk.cmdDraw(cmdBuffer, static_cast<uint32_t>(m_numDrawVertices), 1u, 0u, 0u);
}

class GeometryOutputCountTest : public TestCase
{
public:
    GeometryOutputCountTest(TestContext &testCtx, const char *name, const vector<int> pattern);

    void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const vector<int> m_pattern;
};

GeometryOutputCountTest::GeometryOutputCountTest(TestContext &testCtx, const char *name, const vector<int> pattern)
    : TestCase(testCtx, name)
    , m_pattern(pattern)
{
}

void GeometryOutputCountTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void GeometryOutputCountTest::initPrograms(SourceCollections &sourceCollections) const
{
    {
        std::ostringstream src;
        src << "#version 310 es\n"
            << "layout(location = 0) in highp vec4 a_position;\n"
            << "layout(location = 1) in highp vec4 a_color;\n"
            << "layout(location = 0) out highp vec4 v_geom_FragColor;\n"
            << "void main (void)\n"
            << "{\n"
            << "    gl_Position = a_position;\n"
            << "    v_geom_FragColor = a_color;\n"
            << "}\n";
        sourceCollections.glslSources.add("vertex") << glu::VertexSource(src.str());
    }

    {
        const int max_vertices = m_pattern.size() == 2 ? std::max(m_pattern[0], m_pattern[1]) : m_pattern[0];

        std::ostringstream src;
        src << "#version 310 es\n"
            << "#extension GL_EXT_geometry_shader : require\n"
            << "#extension GL_OES_texture_storage_multisample_2d_array : require\n"
            << "layout(points) in;\n"
            << "layout(triangle_strip, max_vertices = " << max_vertices << ") out;\n"
            << "layout(location = 0) in highp vec4 v_geom_FragColor[];\n"
            << "layout(location = 0) out highp vec4 v_frag_FragColor;\n"
            << "out gl_PerVertex\n"
            << "{\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "void main (void)\n"
            << "{\n"
            << "    const highp float rowHeight = 2.0 / float(" << m_pattern.size() << ");\n"
            << "    const highp float colWidth = 2.0 / float(" << max_vertices << ");\n";

        if (m_pattern.size() == 2)
            src << "    highp int emitCount = (gl_PrimitiveIDIn == 0) ? (" << m_pattern[0] << ") : (" << m_pattern[1]
                << ");\n";
        else
            src << "    highp int emitCount = " << m_pattern[0] << ";\n";
        src << "    for (highp int ndx = 0; ndx < emitCount / 2; ndx++)\n"
            << "    {\n"
            << "        gl_Position = gl_in[0].gl_Position + vec4(float(ndx) * 2.0 * colWidth, 0.0, 0.0, 0.0);\n"
            << "        v_frag_FragColor = v_geom_FragColor[0];\n"
            << "        EmitVertex();\n"

            << "        gl_Position = gl_in[0].gl_Position + vec4(float(ndx) * 2.0 * colWidth, rowHeight, 0.0, 0.0);\n"
            << "        v_frag_FragColor = v_geom_FragColor[0];\n"
            << "        EmitVertex();\n"

            << "    }\n"
            << "}\n";
        sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
    }

    {
        std::ostringstream src;
        src << "#version 310 es\n"
            << "layout(location = 0) out mediump vec4 fragColor;\n"
            << "layout(location = 0) in highp vec4 v_frag_FragColor;\n"
            << "void main (void)\n"
            << "{\n"
            << "    fragColor = v_frag_FragColor;\n"
            << "}\n";
        sourceCollections.glslSources.add("fragment") << glu::FragmentSource(src.str());
    }
}

TestInstance *GeometryOutputCountTest::createInstance(Context &context) const
{
    return new GeometryOutputCountTestInstance(context, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                               static_cast<int>(m_pattern.size()), getName());
}

class VaryingOutputCountCase : public TestCase
{
public:
    VaryingOutputCountCase(TestContext &testCtx, const char *name, const VaryingSource test,
                           const ShaderInstancingMode mode);
    void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const VaryingSource m_test;
    const ShaderInstancingMode m_mode;
};

VaryingOutputCountCase::VaryingOutputCountCase(TestContext &testCtx, const char *name, const VaryingSource test,
                                               const ShaderInstancingMode mode)
    : TestCase(testCtx, name)
    , m_test(test)
    , m_mode(mode)
{
}

void VaryingOutputCountCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void VaryingOutputCountCase::initPrograms(SourceCollections &sourceCollections) const
{
    {
        std::ostringstream src;
        switch (m_test)
        {
        case READ_ATTRIBUTE:
        case READ_TEXTURE:
            src << "#version 310 es\n"
                << "layout(location = 0) in highp vec4 a_position;\n"
                << "layout(location = 1) in highp vec4 a_emitCount;\n"
                << "layout(location = 0) out highp vec4 v_geom_emitCount;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    v_geom_emitCount = a_emitCount;\n"
                << "}\n";
            break;
        case READ_UNIFORM:
            src << "#version 310 es\n"
                << "layout(location = 0) in highp vec4 a_position;\n"
                << "layout(location = 1) in highp vec4 a_vertexNdx;\n"
                << "layout(location = 0) out highp vec4 v_geom_vertexNdx;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    v_geom_vertexNdx = a_vertexNdx;\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("vertex") << glu::VertexSource(src.str());
    }

    {
        const bool instanced = MODE_WITH_INSTANCING == m_mode;
        std::ostringstream src;
        src << "#version 310 es\n"
            << "#extension GL_EXT_geometry_shader : require\n"
            << "#extension GL_OES_texture_storage_multisample_2d_array : require\n";
        if (instanced)
            src << "layout(points, invocations=4) in;\n";
        else
            src << "layout(points) in;\n";

        switch (m_test)
        {
        case READ_ATTRIBUTE:
            src << "layout(triangle_strip, max_vertices = 128) out;\n"
                << "layout(location = 0) in highp vec4 v_geom_emitCount[];\n"
                << "layout(location = 0) out highp vec4 v_frag_FragColor;\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "};\n"
                << "void main (void)\n"
                << "{\n"
                << "    highp vec4 attrEmitCounts = v_geom_emitCount[0];\n"
                << "    mediump int emitCount = int(attrEmitCounts[" << ((instanced) ? ("gl_InvocationID") : ("0"))
                << "]);\n"
                << "    highp vec4 color = vec4((emitCount < 10) ? (0.0) : (1.0), (emitCount > 10) ? (0.0) : (1.0), "
                   "1.0, 1.0);\n"
                << "    highp vec4 basePos = "
                << ((instanced) ? ("gl_in[0].gl_Position + 0.5 * vec4(cos(float(gl_InvocationID)), "
                                   "sin(float(gl_InvocationID)), 0.0, 0.0)") :
                                  ("gl_in[0].gl_Position"))
                << ";\n"
                << "    for (mediump int i = 0; i < emitCount / 2; i++)\n"
                << "    {\n"
                << "        highp float angle = (float(i) + 0.5) / float(emitCount / 2) * 3.142;\n"
                << "        gl_Position = basePos + vec4(cos(angle),  sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "        gl_Position = basePos + vec4(cos(angle), -sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "    }\n"
                << "}\n";
            break;
        case READ_UNIFORM:
            src << "layout(triangle_strip, max_vertices = 128) out;\n"
                << "layout(location = 0) in highp vec4 v_geom_vertexNdx[];\n"
                << "layout(binding = 0) readonly uniform Input {\n"
                << "    ivec4 u_emitCount;\n"
                << "} emit;\n"
                << "layout(location = 0) out highp vec4 v_frag_FragColor;\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "};\n"
                << "void main (void)\n"
                << "{\n"
                << "    mediump int primitiveNdx = "
                << ((instanced) ? ("gl_InvocationID") : ("int(v_geom_vertexNdx[0].x)")) << ";\n"
                << "    mediump int emitCount = emit.u_emitCount[primitiveNdx];\n"
                << "\n"
                << "    const highp vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const highp vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const highp vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const highp vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const highp vec4 colors[4] = vec4[4](red, green, blue, yellow);\n"
                << "    highp vec4 color = colors[int(primitiveNdx)];\n"
                << "\n"
                << "    highp vec4 basePos = "
                << ((instanced) ? ("gl_in[0].gl_Position + 0.5 * vec4(cos(float(gl_InvocationID)), "
                                   "sin(float(gl_InvocationID)), 0.0, 0.0)") :
                                  ("gl_in[0].gl_Position"))
                << ";\n"
                << "    for (mediump int i = 0; i < emitCount / 2; i++)\n"
                << "    {\n"
                << "        highp float angle = (float(i) + 0.5) / float(emitCount / 2) * 3.142;\n"
                << "        gl_Position = basePos + vec4(cos(angle),  sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "        gl_Position = basePos + vec4(cos(angle), -sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "    }\n"
                << "}\n";
            break;
        case READ_TEXTURE:
            src << "layout(triangle_strip, max_vertices = 128) out;\n"
                << "layout(location = 0) in highp vec4 v_geom_vertexNdx[];\n"
                << "layout(binding = 0) uniform highp sampler2D u_sampler;\n"
                << "layout(location = 0) out highp vec4 v_frag_FragColor;\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "};\n"
                << "void main (void)\n"
                << "{\n"
                << "    highp float primitiveNdx = "
                << ((instanced) ? ("float(gl_InvocationID)") : ("v_geom_vertexNdx[0].x")) << ";\n"
                << "    highp vec2 texCoord = vec2(1.0 / 8.0 + primitiveNdx / 4.0, 0.5);\n"
                << "    highp vec4 texColor = texture(u_sampler, texCoord);\n"
                << "    mediump int emitCount = 0;\n"
                << "    if (texColor.x > 0.0)\n"
                << "        emitCount += 6;\n"
                << "    if (texColor.y > 0.0)\n"
                << "        emitCount += 0;\n"
                << "    if (texColor.z > 0.0)\n"
                << "        emitCount += 128;\n"
                << "    if (texColor.w > 0.0)\n"
                << "        emitCount += 10;\n"
                << "    const highp vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const highp vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const highp vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const highp vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const highp vec4 colors[4] = vec4[4](red, green, blue, yellow);\n"
                << "    highp vec4 color = colors[int(primitiveNdx)];\n"
                << "    highp vec4 basePos = "
                << ((instanced) ? ("gl_in[0].gl_Position + 0.5 * vec4(cos(float(gl_InvocationID)), "
                                   "sin(float(gl_InvocationID)), 0.0, 0.0)") :
                                  ("gl_in[0].gl_Position"))
                << ";\n"
                << "    for (mediump int i = 0; i < emitCount / 2; i++)\n"
                << "    {\n"
                << "        highp float angle = (float(i) + 0.5) / float(emitCount / 2) * 3.142;\n"
                << "        gl_Position = basePos + vec4(cos(angle),  sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "        gl_Position = basePos + vec4(cos(angle), -sin(angle), 0.0, 0.0) * 0.15;\n"
                << "        v_frag_FragColor = color;\n"
                << "        EmitVertex();\n"
                << "    }\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
    }

    {
        std::ostringstream src;
        src << "#version 310 es\n"
            << "layout(location = 0) out mediump vec4 fragColor;\n"
            << "layout(location = 0) in highp vec4 v_frag_FragColor;\n"
            << "void main (void)\n"
            << "{\n"
            << "    fragColor = v_frag_FragColor;\n"
            << "}\n";
        sourceCollections.glslSources.add("fragment") << glu::FragmentSource(src.str());
    }
}

TestInstance *VaryingOutputCountCase::createInstance(Context &context) const
{
    return new VaryingOutputCountTestInstance(context, getName(), VK_PRIMITIVE_TOPOLOGY_POINT_LIST, m_test, m_mode);
}

class BuiltinVariableRenderTest : public TestCase
{
public:
    BuiltinVariableRenderTest(TestContext &testCtx, const char *name, const VariableTest test, const bool flag = false);
    void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const VariableTest m_test;
    const bool m_flag;
};

BuiltinVariableRenderTest::BuiltinVariableRenderTest(TestContext &testCtx, const char *name, const VariableTest test,
                                                     const bool flag)
    : TestCase(testCtx, name)
    , m_test(test)
    , m_flag(flag)
{
}

void BuiltinVariableRenderTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_test == TEST_POINT_SIZE)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE);
}

void BuiltinVariableRenderTest::initPrograms(SourceCollections &sourceCollections) const
{
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "out gl_PerVertex\n"
            << " {\n"
            << "    vec4 gl_Position;\n"
            << "    float gl_PointSize;\n"
            << "};\n"
            << "layout(location = 0) in vec4 a_position;\n";
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << "layout(location = 1) in vec4 a_pointSize;\n"
                << "layout(location = 0) out vec4 v_geom_pointSize;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    gl_PointSize = 1.0;\n"
                << "    v_geom_pointSize = a_pointSize;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID:
            src << "layout(location = 1) in vec4 a_primitiveID;\n"
                << "layout(location = 0) out vec4 v_geom_primitiveID;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    v_geom_primitiveID = a_primitiveID;\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("vertex") << glu::VertexSource(src.str());
    }

    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "in gl_PerVertex\n"
            << "{\n"
            << "    vec4 gl_Position;\n"
            << "    float gl_PointSize;\n"
            << "} gl_in[];\n"
            << "out gl_PerVertex\n"
            << "{\n"
            << "    vec4 gl_Position;\n"
            << "    float gl_PointSize;\n"
            << "};\n";
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << "#extension GL_EXT_geometry_point_size : require\n"
                << "layout(points) in;\n"
                << "layout(points, max_vertices = 1) out;\n"
                << "layout(location = 0) in vec4 v_geom_pointSize[];\n"
                << "layout(location = 0) out vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = gl_in[0].gl_Position;\n"
                << "    gl_PointSize = v_geom_pointSize[0].x + 1.0;\n"
                << "    v_frag_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                << "    EmitVertex();\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << "layout(lines) in;\n"
                << "layout(triangle_strip, max_vertices = 10) out;\n"
                << "layout(location = 0) out vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 colors[4] = vec4[4](red, green, blue, yellow);\n"
                << "    for (int counter = 0; counter < 3; ++counter)\n"
                << "    {\n"
                << "        float percent = 0.1 * counter;\n"
                << "        gl_Position = gl_in[0].gl_Position * vec4(1.0 + percent, 1.0 + percent, 1.0, 1.0);\n"
                << "        v_frag_FragColor = colors[gl_PrimitiveIDIn % 4];\n"
                << "        EmitVertex();\n"
                << "        gl_Position = gl_in[1].gl_Position * vec4(1.0 + percent, 1.0 + percent, 1.0, 1.0);\n"
                << "        v_frag_FragColor = colors[gl_PrimitiveIDIn % 4];\n"
                << "        EmitVertex();\n"
                << "    }\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID:
            src << "layout(points, invocations=1) in;\n"
                << "layout(triangle_strip, max_vertices = 3) out;\n"
                << "layout(location = 0) in vec4 v_geom_primitiveID[];\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = gl_in[0].gl_Position + vec4(0.05, 0.0, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "    gl_Position = gl_in[0].gl_Position - vec4(0.05, 0.0, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "    gl_Position = gl_in[0].gl_Position + vec4(0.0, 0.05, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
    }

    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "layout(location = 0) in vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = v_frag_FragColor;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "layout(location = 0) in vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = v_frag_FragColor;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 colors[4] = vec4[4](yellow, red, green, blue);\n"
                << "    fragColor = colors[gl_PrimitiveID % 4];\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("fragment") << glu::FragmentSource(src.str());
    }
}

TestInstance *BuiltinVariableRenderTest::createInstance(Context &context) const
{
    return new BuiltinVariableRenderTestInstance(context, getName(), m_test, m_flag);
}

inline vector<int> createPattern(int count)
{
    vector<int> pattern;
    pattern.push_back(count);
    return pattern;
}

inline vector<int> createPattern(int count0, int count1)
{
    vector<int> pattern;
    pattern.push_back(count0);
    pattern.push_back(count1);
    return pattern;
}

enum class SideEffectCase
{
    CONDITION = 0,
    DEGENERATE,
};

struct SideEffectParams
{
    SideEffectCase sideEffectCase;
};

void sideEffectSupportCheck(Context &context, SideEffectParams)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
}

void sideEffectInitPrograms(vk::SourceCollections &dst, SideEffectParams params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());

    // Passthrough geometry shader.
    std::ostringstream geom;
    geom << "#version 460\n"
         << "layout (triangles) in;\n"
         << "layout (triangle_strip, max_vertices=3) out;\n"
         << "in gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "} gl_in[3];\n"
         << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "layout (set=0, binding=0, std430) buffer SSBO_Block {\n"
         << "    uint condition;\n"
         << "    uint value;\n"
         << "} ssbo;\n"
         << "void main() {\n";

    if (params.sideEffectCase == SideEffectCase::CONDITION)
    {
        geom << "    ssbo.value = 777u;\n"
             << "    if (ssbo.condition != 0u) {\n"
             << "        for (uint i = 0; i < 3; ++i) {\n"
             << "            gl_Position = gl_in[i].gl_Position;\n"
             << "            EmitVertex();\n"
             << "        }\n"
             << "        EndPrimitive();\n"
             << "    }\n";
    }
    else if (params.sideEffectCase == SideEffectCase::DEGENERATE)
    {
        geom << "    ssbo.value = 777u;\n"
             << "    gl_Position = gl_in[0].gl_Position;\n"
             << "    EmitVertex();\n"
             << "    gl_Position = gl_in[1].gl_Position;\n"
             << "    EmitVertex();\n"
             << "    EndPrimitive();\n";
    }
    else
        DE_ASSERT(false);

    geom << "}\n";
    dst.glslSources.add("geom") << glu::GeometrySource(geom.str());
}

tcu::TestStatus sideEffectTest(Context &context, SideEffectParams)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 extent(1, 1, 1);
    const auto extentVk   = makeExtent3D(extent);
    const auto imgFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto imgUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto descStages = VK_SHADER_STAGE_GEOMETRY_BIT;
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, imgFormat, imgUsage, VK_IMAGE_TYPE_2D);

    struct
    {
        uint32_t condition;
        uint32_t value;
    } ssbo;

    const auto ssboBufferSize  = static_cast<VkDeviceSize>(sizeof(ssbo));
    const auto ssboBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto ssboBufferInfo  = makeBufferCreateInfo(ssboBufferSize, ssboBufferUsage);
    BufferWithMemory ssboBuffer(ctx.vkd, ctx.device, ctx.allocator, ssboBufferInfo, MemoryRequirement::HostVisible);
    auto &ssboAlloc = ssboBuffer.getAllocation();
    {
        memset(ssboAlloc.getHostPtr(), 0, sizeof(ssbo)); // Note this also sets the condition value to zero.
        flushAlloc(ctx.vkd, ctx.device, ssboAlloc);
    }

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, descStages);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    const auto &binaries  = context.getBinaryCollection();
    const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto geomShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("geom"));

    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, imgFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), extentVk.width, extentVk.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const std::vector<tcu::Vec4> vertices{
        // clang-format off
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f,  3.0f, 0.0f, 1.0f),
        tcu::Vec4( 3.0f, -1.0f, 0.0f, 1.0f),
        // clang-format on
    };
    const auto vertexBufferSize           = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo           = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    const VkDeviceSize vertexBufferOffset = 0ull;
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    const auto pipeline =
        makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *geomShader, *fragShader, *renderPass, viewports, scissors);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto ssboDescInfo = makeDescriptorBufferInfo(*ssboBuffer, 0ull, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*descSet, binding(0u), descType, &ssboDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Must be different from the color set in the frag shader.

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Validate color buffer has not been written to and the SSBO value is as we expect.

    {
        invalidateAlloc(ctx.vkd, ctx.device, ssboAlloc);
        memcpy(&ssbo, ssboAlloc.getHostPtr(), sizeof(ssbo));
    }
    const uint32_t expected = 777u; // Must match geometry shader.
    if (ssbo.value != expected)
    {
        std::ostringstream msg;
        msg << "Unexpected value found in SSBO: expected " << expected << " but found " << ssbo.value;
        TCU_FAIL(msg.str());
    }

    {
        const auto tcuFormat = mapVkFormat(imgFormat);

        tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
        tcu::PixelBufferAccess reference = refLevel.getAccess();
        tcu::clear(reference, clearColor);

        invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
        tcu::ConstPixelBufferAccess result(tcuFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());

        auto &log = context.getTestContext().getLog();
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
            TCU_FAIL("Unexpected results in color buffer; check log for details --");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

TestCaseGroup *createBasicGeometryShaderTests(TestContext &testCtx)
{
    MovePtr<TestCaseGroup> basicGroup(new tcu::TestCaseGroup(testCtx, "basic"));

    // Output N vertices
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_10", createPattern(10)));
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_128", createPattern(128)));
    // Output N, M vertices in two invocations
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_10_and_100", createPattern(10, 100)));
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_100_and_10", createPattern(100, 10)));
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_0_and_128", createPattern(0, 128)));
    basicGroup->addChild(new GeometryOutputCountTest(testCtx, "output_128_and_0", createPattern(128, 0)));

    // Output varying number of vertices
    basicGroup->addChild(
        new VaryingOutputCountCase(testCtx, "output_vary_by_attribute", READ_ATTRIBUTE, MODE_WITHOUT_INSTANCING));
    basicGroup->addChild(
        new VaryingOutputCountCase(testCtx, "output_vary_by_uniform", READ_UNIFORM, MODE_WITHOUT_INSTANCING));
    basicGroup->addChild(
        new VaryingOutputCountCase(testCtx, "output_vary_by_texture", READ_TEXTURE, MODE_WITHOUT_INSTANCING));
    basicGroup->addChild(new VaryingOutputCountCase(testCtx, "output_vary_by_attribute_instancing", READ_ATTRIBUTE,
                                                    MODE_WITH_INSTANCING));
    basicGroup->addChild(
        new VaryingOutputCountCase(testCtx, "output_vary_by_uniform_instancing", READ_UNIFORM, MODE_WITH_INSTANCING));
    basicGroup->addChild(
        new VaryingOutputCountCase(testCtx, "output_vary_by_texture_instancing", READ_TEXTURE, MODE_WITH_INSTANCING));

    // test gl_PointSize
    basicGroup->addChild(new BuiltinVariableRenderTest(testCtx, "point_size", TEST_POINT_SIZE));
    // test gl_PrimitiveIDIn
    basicGroup->addChild(new BuiltinVariableRenderTest(testCtx, "primitive_id_in", TEST_PRIMITIVE_ID_IN));
    // test gl_PrimitiveIDIn with primitive restart
    basicGroup->addChild(
        new BuiltinVariableRenderTest(testCtx, "primitive_id_in_restarted", TEST_PRIMITIVE_ID_IN, true));
    // test gl_PrimitiveID
    basicGroup->addChild(new BuiltinVariableRenderTest(testCtx, "primitive_id", TEST_PRIMITIVE_ID));

    {
        const struct
        {
            SideEffectCase sideEffectCase;
            const char *name;
        } sideEffectCases[] = {
            {SideEffectCase::CONDITION, "condition"},
            {SideEffectCase::DEGENERATE, "degenerate"},
        };

        for (const auto &sideEffectCase : sideEffectCases)
        {
            const auto testName = std::string("side_effect_with_") + sideEffectCase.name;
            const SideEffectParams params{sideEffectCase.sideEffectCase};
            addFunctionCaseWithPrograms(basicGroup.get(), testName, sideEffectSupportCheck, sideEffectInitPrograms,
                                        sideEffectTest, params);
        }
    }

    return basicGroup.release();
}

} // namespace geometry
} // namespace vkt
