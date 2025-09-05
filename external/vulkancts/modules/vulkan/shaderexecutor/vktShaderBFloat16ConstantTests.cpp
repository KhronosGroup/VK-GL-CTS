/*-------------------------------------------------------------------------
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
 * \brief Tests of constant_id for bfloat16 type.
 *//*--------------------------------------------------------------------*/

#include "vktShaderBFloat16Tests.hpp"
#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuFloat.hpp"
#include "deRandom.hpp"
#include "tcuStringTemplate.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <sstream>
#include <tuple>
#include <type_traits>

namespace vkt
{
using namespace vk;

namespace shaderexecutor
{

namespace
{

// Do simple trick, just change the right side of below equation
// to switch whole stuff to work with regular float16 type.
using BFloat16 = tcu::BrainFloat16;

struct Params
{
    uint32_t seed   = 13;
    uint32_t width  = 64;
    uint32_t height = 64;
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
};

struct SpecializationInfo
{
    size_t size;
    std::vector<char> data;
    std::vector<vk::VkSpecializationMapEntry> entries;

    SpecializationInfo() : size(0), data(), entries()
    {
        data.reserve(256);
    }

    void assertEntryExists(uint32_t id)
    {
        for (const vk::VkSpecializationMapEntry &entry : entries)
            if (entry.constantID == id)
            {
                DE_ASSERT(entry.constantID != id);
            }
    }

    vk::VkSpecializationInfo get() const
    {
        return {
            uint32_t(entries.size()),        // mapEntryCount
            size ? entries.data() : nullptr, // pMapEntries
            size,                            // dataSize
            size ? data.data() : nullptr     // pData
        };
    }

    template <typename X>
    void addEntry(const X entry, int id = (-1))
    {
        const size_t entrySize = size_t(sizeof(X));
        const uint32_t entryId = (id < 0) ? uint32_t(entries.size()) : uint32_t(id);
        assertEntryExists(entryId);
        // { uint32_t constantID; uint32_t offset; size_t size }
        entries.push_back({entryId, uint32_t(size), entrySize});

        data.resize(size + entrySize);
        new (&data.data()[size]) X(entry);

        size += entrySize;
    }
};

// XXX TODO Rename "BFloat*" after the original change is merged
template <VkShaderStageFlagBits, typename FloatTy>
class BFloat16ConstantCaseT;

template <typename FloatTy>
class BFloat16ConstantCase : public TestCase
{
protected:
    const Params m_params;

public:
    BFloat16ConstantCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~BFloat16ConstantCase() = default;
    virtual void checkSupport(Context &context) const override
    {
        if (!context.get16BitStorageFeatures().storageBuffer16BitAccess)
        {
            TCU_THROW(NotSupportedError, "16-bit floats not supported for storage buffers");
        }

        if constexpr (std::is_same_v<FloatTy, tcu::BrainFloat16>)
        {
            if (context.getShaderBfloat16Features().shaderBFloat16Type != VK_TRUE)
            {
                TCU_THROW(NotSupportedError, "Brain float not supported by device");
            }
        }
        if constexpr (std::is_same_v<FloatTy, tcu::FloatE5M2> || std::is_same_v<FloatTy, tcu::FloatE4M3>)
        {
            if (!context.getShaderFloat8FeaturesEXT().shaderFloat8)
            {
                TCU_THROW(NotSupportedError, "shaderFloat8 not supported by device");
            }
        }
    }
};

template <typename FloatTy>
class BFloat16ComputeInstance;

template <typename FloatTy>
class BFloat16ConstantInstance : public TestInstance
{
public:
    using FloatTyVec4 = std::array<FloatTy, 4>;
    static_assert(sizeof(FloatTy) == sizeof(typename FloatTy::StorageType), "???");
    static_assert(sizeof(FloatTyVec4) == sizeof(typename FloatTy::StorageType) * 4u, "???");

    BFloat16ConstantInstance(Context &context, const Params &params, VkShaderStageFlags shaderStages)
        : TestInstance(context)
        , m_params(params)
        , m_shaderStages(shaderStages)
        , m_initialized(false)
    {
    }

    virtual void prepareBuffers()
    {
        const DeviceInterface &di = m_context.getDeviceInterface();
        const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
        const VkDevice dev        = m_context.getDevice();
        Allocator &allocator      = m_context.getDefaultAllocator();

        const VkBufferUsageFlags usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const std::vector<uint32_t> queueIndices{queueIndex};
        const uint32_t ioCount              = 1024u;
        const VkDeviceSize ioBytesSize      = ioCount * sizeof(FloatTyVec4);
        const VkBufferCreateInfo ioBufferCI = makeBufferCreateInfo(ioBytesSize, usage, queueIndices);
        bf16::makeMovePtr(m_inBufferX, di, dev, allocator, ioBufferCI, MemoryRequirement::HostVisible);
        bf16::makeMovePtr(m_inBufferY, di, dev, allocator, ioBufferCI, MemoryRequirement::HostVisible);
        bf16::makeMovePtr(m_outBufferZ, di, dev, allocator, ioBufferCI, MemoryRequirement::HostVisible);
    }

    virtual void prepareDescriptorSet()
    {
        DE_MULTI_ASSERT(m_inBufferX, m_inBufferY, m_outBufferZ);

        const DeviceInterface &di = m_context.getDeviceInterface();
        const VkDevice dev        = m_context.getDevice();

        const VkDescriptorBufferInfo inBufferXDBI =
            makeDescriptorBufferInfo(**m_inBufferX, 0u, m_inBufferX->getBufferSize());
        const VkDescriptorBufferInfo inBufferYDBI =
            makeDescriptorBufferInfo(**m_inBufferY, 0u, m_inBufferY->getBufferSize());
        const VkDescriptorBufferInfo outBufferDBI =
            makeDescriptorBufferInfo(**m_outBufferZ, 0u, m_outBufferZ->getBufferSize());
        m_dsPool = DescriptorPoolBuilder()
                       .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
                       .build(di, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        m_dsLayout = DescriptorSetLayoutBuilder()
                         .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_shaderStages)
                         .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_shaderStages)
                         .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_shaderStages)
                         .build(di, dev);
        m_descriptorSet = makeDescriptorSet(di, dev, *m_dsPool, *m_dsLayout);
        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inBufferXDBI)
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inBufferYDBI)
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outBufferDBI)
            .update(di, dev);
    }

    virtual void preparePipelineLayout()
    {
        DE_ASSERT(m_dsLayout);

        const DeviceInterface &di = m_context.getDeviceInterface();
        const VkDevice dev        = m_context.getDevice();

        m_pipelineLayout = makePipelineLayout(di, dev, *m_dsLayout, nullptr);
    }

    virtual void prepareCommandBuffer()
    {
        DE_ASSERT(m_pipelineLayout);

        const DeviceInterface &di = m_context.getDeviceInterface();
        const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
        const VkDevice dev        = m_context.getDevice();

        m_cmdPool = makeCommandPool(di, dev, queueIndex);
        m_cmd     = allocateCommandBuffer(di, dev, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    virtual auto prepareShaders() const -> std::vector<Move<VkShaderModule>> = 0;

    virtual auto preparePipeline() const -> Move<VkPipeline> = 0;

    virtual void initCommonMembers()
    {
        if (false == m_initialized)
        {
            prepareBuffers();
            prepareDescriptorSet();
            preparePipelineLayout();
            prepareCommandBuffer();
            m_shaders     = prepareShaders();
            m_pipeline    = preparePipeline();
            m_initialized = true;
        }
    }

protected:
    const Params m_params;
    const VkShaderStageFlags m_shaderStages;
    bool m_initialized;
    de::MovePtr<BufferWithMemory> m_inBufferX;
    de::MovePtr<BufferWithMemory> m_inBufferY;
    de::MovePtr<BufferWithMemory> m_outBufferZ;
    Move<VkDescriptorPool> m_dsPool;
    Move<VkDescriptorSetLayout> m_dsLayout;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmd;
    std::vector<Move<VkShaderModule>> m_shaders;
    Move<VkPipeline> m_pipeline;
};

template <typename FloatTy>
class BFloat16ComputeInstance : public BFloat16ConstantInstance<FloatTy>
{
public:
    using Super        = BFloat16ConstantInstance<FloatTy>;
    using ReferenceSet = std::pair<std::array<FloatTy, 20>, uint32_t>;
    BFloat16ComputeInstance(Context &context, const Params &params)
        : Super(context, params, VK_SHADER_STAGE_COMPUTE_BIT)
        , m_referenceSet(prepareReferenceSet())
    {
        Super::initCommonMembers();
    }
    virtual auto prepareShaders() const -> std::vector<Move<VkShaderModule>> override;
    virtual auto preparePipeline() const -> Move<VkPipeline> override;
    virtual auto iterate() -> tcu::TestStatus override;
    auto prepareReferenceSet() const -> ReferenceSet;
    auto verifyResult() const -> bool;

private:
    auto asLocalSize(const FloatTy &value) const -> uint32_t;
    const ReferenceSet m_referenceSet;
};

template <typename FloatTy>
class BFloat16GraphicsInstance : public BFloat16ConstantInstance<FloatTy>
{
public:
    using Vertices = std::vector<FloatTy>;
    using Super    = BFloat16ConstantInstance<FloatTy>;
    BFloat16GraphicsInstance(Context &context, const Params &params, VkShaderStageFlags shaderStages,
                             VkPrimitiveTopology topology)
        : Super(context, params, shaderStages)
        , m_topology(topology)
        , m_vertices(prepareVertices(topology))
        , m_initialized(false)
    {
    }
    virtual void initCommonMembers() override;
    virtual void prepareBuffers() override;
    virtual auto prepareShaders() const -> std::vector<Move<VkShaderModule>> override;
    virtual auto iterate() -> tcu::TestStatus override;
    virtual bool verifyResult() const                                         = 0;
    virtual auto prepareVertexBuffer() const -> de::MovePtr<BufferWithMemory> = 0;
    auto getVertexCount() const -> uint32_t;
    auto prepareVertices(VkPrimitiveTopology topology) const -> Vertices;
    void copyImageToResultBuffer();

protected:
    const VkPrimitiveTopology m_topology;
    const Vertices m_vertices;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
    de::MovePtr<BufferWithMemory> m_resultBuffer;
    de::MovePtr<ImageWithMemory> m_image;
    Move<VkImageView> m_imageView;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

private:
    bool m_initialized;
};

template <typename FloatTy>
class BFloat16VertexInstance : public BFloat16GraphicsInstance<FloatTy>
{
public:
    using Super = BFloat16GraphicsInstance<FloatTy>;
    BFloat16VertexInstance(Context &context, const Params &params)
        : Super(context, params, VK_SHADER_STAGE_VERTEX_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
    {
        Super::initCommonMembers();
    }
    virtual bool verifyResult() const override;
    virtual auto preparePipeline() const -> Move<VkPipeline> override;
    virtual auto prepareVertexBuffer() const -> de::MovePtr<BufferWithMemory> override;
};

template <typename FloatTy>
class BFloat16FragmentInstance : public BFloat16GraphicsInstance<FloatTy>
{
public:
    using Super = BFloat16GraphicsInstance<FloatTy>;
    BFloat16FragmentInstance(Context &context, const Params &params)
        : Super(context, params, VK_SHADER_STAGE_FRAGMENT_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
    {
        Super::initCommonMembers();
    }
    virtual bool verifyResult() const override;
    virtual auto preparePipeline() const -> Move<VkPipeline> override;
    virtual auto prepareVertexBuffer() const -> de::MovePtr<BufferWithMemory> override;
};

template <typename FloatTy>
class BFloat16ConstantCaseT<VK_SHADER_STAGE_COMPUTE_BIT, FloatTy> : public BFloat16ConstantCase<FloatTy>
{
    using BFloat16ConstantCase<FloatTy>::BFloat16ConstantCase;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new BFloat16ComputeInstance<FloatTy>(context, BFloat16ConstantCase<FloatTy>::m_params);
    }
};

template <typename FloatTy>
class BFloat16ConstantCaseT<VK_SHADER_STAGE_VERTEX_BIT, FloatTy> : public BFloat16ConstantCase<FloatTy>
{
    using BFloat16ConstantCase<FloatTy>::BFloat16ConstantCase;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new BFloat16VertexInstance<FloatTy>(context, BFloat16ConstantCase<FloatTy>::m_params);
    }
};

template <typename FloatTy>
class BFloat16ConstantCaseT<VK_SHADER_STAGE_FRAGMENT_BIT, FloatTy> : public BFloat16ConstantCase<FloatTy>
{
    using BFloat16ConstantCase<FloatTy>::BFloat16ConstantCase;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new BFloat16FragmentInstance<FloatTy>(context, BFloat16ConstantCase<FloatTy>::m_params);
    }
};

template <typename FloatTy>
void BFloat16ConstantCaseT<VK_SHADER_STAGE_COMPUTE_BIT, FloatTy>::initPrograms(
    SourceCollections &programCollection) const
{
    const tcu::StringTemplate glslCodeTemplate(R"(
#version 450
#extension ${EXTENSION}: require
layout(binding=0) buffer InBufferX { ${VEC4} x[]; };
layout(binding=1) buffer InBufferY { ${VEC4} y[]; };
layout(binding=2) buffer OutBuffer { ${VEC4} z[]; };
layout(local_size_x_id = 0, local_size_y_id = 2, local_size_z_id = 4) in;
// local_size_x_id
layout(constant_id = 1)  const ${FLOAT_TYPE} c1 = ${FLOAT_TYPE}(0.0);
// local_size_y_id
layout(constant_id = 3)  const ${FLOAT_TYPE} c3 = ${FLOAT_TYPE}(0.0);
// local_size_z_id
layout(constant_id = 5)  const ${FLOAT_TYPE} c5 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 6)  const ${FLOAT_TYPE} c6 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 7)  const float c7 = 0.0;
layout(constant_id = 8)  const ${FLOAT_TYPE} c8 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 9)  const ${FLOAT_TYPE} c9 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 10) const ${FLOAT_TYPE} c10 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 11) const float c11 = 0.0;
const ${FLOAT_TYPE} c12 = ${FLOAT_TYPE}(c11);
const float c13 = float(c12);
void main() {
    z[0].x = ${FLOAT_TYPE}(float(gl_WorkGroupSize.x));
    z[0].y = ${FLOAT_TYPE}(c1);
    z[0].z = ${FLOAT_TYPE}(float(gl_WorkGroupSize.y));
    z[0].w = ${FLOAT_TYPE}(c3);
    z[1].x = ${FLOAT_TYPE}(float(gl_WorkGroupSize.z));
    z[1].y = ${FLOAT_TYPE}(c5);
    z[1].z = ${FLOAT_TYPE}(c6);
    z[1].w = ${FLOAT_TYPE}(c7);
    z[2].x = ${FLOAT_TYPE}(c8);
    z[2].y = ${FLOAT_TYPE}(c9);
    z[2].z = ${FLOAT_TYPE}(c10);
    z[2].w = ${FLOAT_TYPE}(c13);
}
    )");

    const std::map<std::string, std::string> substs{{"EXTENSION", bf16::getExtensionName<FloatTy>()},
                                                    {"FLOAT_TYPE", bf16::getVecTypeName<FloatTy, 1>()},
                                                    {"VEC4", bf16::getVecTypeName<FloatTy, 4>()}};

    const std::string glslCode = glslCodeTemplate.specialize(substs);
    programCollection.glslSources.add("test") << glu::ComputeSource(glslCode);
}

template <typename FloatTy>
void BFloat16ConstantCaseT<VK_SHADER_STAGE_VERTEX_BIT, FloatTy>::initPrograms(
    SourceCollections &programCollection) const
{
    const tcu::StringTemplate vertCodeTemplate(R"(
#version 450
#extension ${EXTENSION}: require
layout(binding=0) buffer InBufferX { ${VEC4} x[]; };
layout(binding=1) buffer InBufferY { ${VEC4} y[]; };
layout(binding=2) buffer OutBuffer { ${VEC4} z[]; };
layout(constant_id = 0)  const float c0 = 0.0;
layout(constant_id = 1)  const ${FLOAT_TYPE} c1 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 2)  const float c2 = 0.0;
layout(constant_id = 3)  const ${FLOAT_TYPE} c3 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 4)  const float c4 = 0.0;
layout(constant_id = 5)  const ${FLOAT_TYPE} c5 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 6)  const ${FLOAT_TYPE} c6 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 7)  const float c7 = 0.0;
layout(constant_id = 8)  const ${FLOAT_TYPE} c8 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 9)  const ${FLOAT_TYPE} c9 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 10) const ${FLOAT_TYPE} c10 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 11) const float c11 = 0.0;
layout(constant_id = 12) const ${FLOAT_TYPE} c12 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 13) const ${FLOAT_TYPE} c13 = ${FLOAT_TYPE}(0.0);
void main() {
    z[0]  = ${VEC4}(${FLOAT_TYPE}(c0));
    z[1]  = ${VEC4}(${FLOAT_TYPE}(c1));
    z[2]  = ${VEC4}(${FLOAT_TYPE}(c2));
    z[3]  = ${VEC4}(${FLOAT_TYPE}(c3));
    z[4]  = ${VEC4}(${FLOAT_TYPE}(c4));
    z[5]  = ${VEC4}(${FLOAT_TYPE}(c5));
    z[6]  = ${VEC4}(${FLOAT_TYPE}(c6));
    z[7]  = ${VEC4}(${FLOAT_TYPE}(c7));
    z[8]  = ${VEC4}(${FLOAT_TYPE}(c8));
    z[9]  = ${VEC4}(${FLOAT_TYPE}(c9));
    z[10] = ${VEC4}(${FLOAT_TYPE}(c10));
    z[11] = ${VEC4}(${FLOAT_TYPE}(c11));
    z[12] = ${VEC4}(${FLOAT_TYPE}(c12));
    z[13] = ${VEC4}(${FLOAT_TYPE}(c13));

    gl_Position = vec4(float(z[gl_VertexIndex * 2].x), float(z[gl_VertexIndex * 2 + 1].y), 0, 1);
}
    )");

    const std::map<std::string, std::string> substs{{"EXTENSION", bf16::getExtensionName<FloatTy>()},
                                                    {"FLOAT_TYPE", bf16::getVecTypeName<FloatTy, 1>()},
                                                    {"VEC4", bf16::getVecTypeName<FloatTy, 4>()}};
    const std::string vertCode = vertCodeTemplate.specialize(substs);

    const std::string fragCode(R"(
#version 450
layout(location = 0) out vec4 color;
void main() {
    color = vec4(1);
}
    )");

    programCollection.glslSources.add("vert") << glu::VertexSource(vertCode);
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragCode);
}

template <typename FloatTy>
void BFloat16ConstantCaseT<VK_SHADER_STAGE_FRAGMENT_BIT, FloatTy>::initPrograms(
    SourceCollections &programCollection) const
{
    const tcu::StringTemplate fragCodeTemplate(R"(
#version 450
#extension ${EXTENSION}: require
layout(binding=0) buffer InBufferX { ${VEC4} x[]; };
layout(binding=1) buffer InBufferY { ${VEC4} y[]; };
layout(binding=2) buffer OutBuffer { ${VEC4} z[]; };
layout(location = 0) out vec4 color;
layout(constant_id = 0)  const float c0 = 0.0;
layout(constant_id = 1)  const ${FLOAT_TYPE} c1 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 2)  const float c2 = 0.0;
layout(constant_id = 3)  const ${FLOAT_TYPE} c3 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 4)  const float c4 = 0.0;
layout(constant_id = 5)  const ${FLOAT_TYPE} c5 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 6)  const ${FLOAT_TYPE} c6 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 7)  const float c7 = 0.0;
layout(constant_id = 8)  const ${FLOAT_TYPE} c8 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 9)  const ${FLOAT_TYPE} c9 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 10) const ${FLOAT_TYPE} c10 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 11) const float c11 = 0.0;
layout(constant_id = 12) const ${FLOAT_TYPE} c12 = ${FLOAT_TYPE}(0.0);
layout(constant_id = 13) const ${FLOAT_TYPE} c13 = ${FLOAT_TYPE}(0.0);
void copy() {
    z[0]  = ${VEC4}(${FLOAT_TYPE}(c0));
    z[1]  = ${VEC4}(${FLOAT_TYPE}(c1));
    z[2]  = ${VEC4}(${FLOAT_TYPE}(c2));
    z[3]  = ${VEC4}(${FLOAT_TYPE}(c3));
    z[4]  = ${VEC4}(${FLOAT_TYPE}(c4));
    z[5]  = ${VEC4}(${FLOAT_TYPE}(c5));
    z[6]  = ${VEC4}(${FLOAT_TYPE}(c6));
    z[7]  = ${VEC4}(${FLOAT_TYPE}(c7));
    z[8]  = ${VEC4}(${FLOAT_TYPE}(c8));
    z[9]  = ${VEC4}(${FLOAT_TYPE}(c9));
    z[10] = ${VEC4}(${FLOAT_TYPE}(c10));
    z[11] = ${VEC4}(${FLOAT_TYPE}(c11));
    z[12] = ${VEC4}(${FLOAT_TYPE}(c12));
    z[13] = ${VEC4}(${FLOAT_TYPE}(c13));
}
void main() {
    copy();
    const float c = float(gl_PrimitiveID + 1);
    color = vec4(c, c, c, 1.0);
}
    )");

    const std::map<std::string, std::string> substs{{"EXTENSION", bf16::getExtensionName<FloatTy>()},
                                                    {"FLOAT_TYPE", bf16::getVecTypeName<FloatTy, 1>()},
                                                    {"VEC4", bf16::getVecTypeName<FloatTy, 4>()}};
    const std::string fragCode = fragCodeTemplate.specialize(substs);

    const std::string vertCode(R"(
#version 450
layout(location = 0) in vec4 pos;
void main() {
    gl_Position = pos;
}
    )");

    programCollection.glslSources.add("vert") << glu::VertexSource(vertCode);
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragCode);
}

// BFloat16ComputeInstance
template <typename FloatTy>
auto BFloat16ComputeInstance<FloatTy>::prepareShaders() const -> std::vector<Move<VkShaderModule>>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    std::vector<Move<VkShaderModule>> modules;

    auto module =
        createShaderModule(di, dev, this->m_context.getBinaryCollection().get("test"), VkSamplerCreateFlags(0));
    modules.emplace_back(std::move(module));

    return modules;
}

template <typename FloatTy>
auto BFloat16ComputeInstance<FloatTy>::asLocalSize(const FloatTy &value) const -> uint32_t
{
    return static_cast<uint32_t>(std::abs(value.asFloat()) + 1.0f);
}

template <typename FloatTy>
auto BFloat16ComputeInstance<FloatTy>::prepareReferenceSet() const -> ReferenceSet
{
    ReferenceSet data{};
    de::Random rnd(this->m_params.seed);
    for (FloatTy &ref : data.first)
    {
        const float in  = float(int(rnd.getUint32() % 15u) - 7) / 2.f;
        ref             = FloatTy(in);
        const float out = ref.asFloat();
        const bool same = in == out;
        DE_MULTI_UNREF(in, out, same);
    }
    for (const FloatTy &ref : data.first)
    {
        if (ref.isZero())
            ++data.second;
    }
    return data;
}

template <typename FloatTy>
Move<VkPipeline> BFloat16ComputeInstance<FloatTy>::preparePipeline() const
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();

    SpecializationInfo i;
    i.addEntry(asLocalSize(m_referenceSet.first[0])); // local_size_x
    i.addEntry(m_referenceSet.first[1]);              // c1
    i.addEntry(asLocalSize(m_referenceSet.first[2])); // local_size_y
    i.addEntry(m_referenceSet.first[3]);              // c3
    i.addEntry(asLocalSize(m_referenceSet.first[4])); // local_size_z
    i.addEntry(m_referenceSet.first[5]);              // c5
    i.addEntry(m_referenceSet.first[6]);              // c6
    i.addEntry(m_referenceSet.first[7].asFloat());    // c7
    i.addEntry(m_referenceSet.first[8]);              // c8
    i.addEntry(m_referenceSet.first[9]);              // c9
    i.addEntry(m_referenceSet.first[10]);             // c10
    i.addEntry(m_referenceSet.first[11].asFloat());   // c11

    const vk::VkSpecializationInfo si = i.get();

    return makeComputePipeline(di, dev, *this->m_pipelineLayout, VkPipelineCreateFlags(0), nullptr, *this->m_shaders[0],
                               VkPipelineShaderStageCreateFlags(0), &si, VK_NULL_HANDLE, 0u);
}

template <typename FloatTy>
auto BFloat16ComputeInstance<FloatTy>::verifyResult() const -> bool
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();

    invalidateAlloc(di, dev, this->m_outBufferZ->getAllocation());
    const typename Super::FloatTyVec4 *result =
        reinterpret_cast<typename Super::FloatTyVec4 *>(this->m_outBufferZ->getAllocation().getHostPtr());

    const typename Super::FloatTyVec4 reference[]{
        {FloatTy(float(asLocalSize(m_referenceSet.first[0]))), m_referenceSet.first[1],
         FloatTy(float(asLocalSize(m_referenceSet.first[2]))), m_referenceSet.first[3]},
        {FloatTy(float(asLocalSize(m_referenceSet.first[4]))), m_referenceSet.first[5], m_referenceSet.first[6],
         m_referenceSet.first[7]},
        {m_referenceSet.first[8], m_referenceSet.first[9], m_referenceSet.first[10], m_referenceSet.first[11]}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(reference); ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            const float a = reference[i][j].asFloat();
            const float b = result[i][j].asFloat();
            if (a != b)
                return false;
        }
    }

    return true;
}

template <typename FloatTy>
tcu::TestStatus BFloat16ComputeInstance<FloatTy>::iterate()
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkQueue queue       = this->m_context.getUniversalQueue();
    const VkDevice dev        = this->m_context.getDevice();

    beginCommandBuffer(di, *this->m_cmd);
    di.cmdBindPipeline(*this->m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *this->m_pipeline);
    di.cmdBindDescriptorSets(*this->m_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *this->m_pipelineLayout, 0u, 1u,
                             bf16::fwd_as_ptr(*this->m_descriptorSet), 0u, nullptr);
    di.cmdDispatch(*this->m_cmd, 1u, 1u, 1u);
    endCommandBuffer(di, *this->m_cmd);
    submitCommandsAndWait(di, dev, queue, *this->m_cmd);

    const bool res = verifyResult();

    return (res ? tcu::TestStatus::pass : tcu::TestStatus::fail)(std::string());
}

// BFloat16GraphicsInstance
template <typename FloatTy>
void BFloat16GraphicsInstance<FloatTy>::initCommonMembers()
{
    if (m_initialized)
        return;

    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    const uint32_t queueIndex = this->m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator      = this->m_context.getDefaultAllocator();

    const vk::VkImageCreateInfo ici{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void *pNext;
        0,                                                                     // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        this->m_params.format,                                                 // VkFormat format;
        {this->m_params.width, this->m_params.height, 1u},                     // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        1u,                                                                    // uint32_t queueFamilyIndexCount;
        &queueIndex,                                                           // const uint32_t *pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED                                              // VkImageLayout initialLayout;
    };
    bf16::makeMovePtr(m_image, di, dev, allocator, ici, MemoryRequirement::Any);

    const vk::VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    m_imageView = makeImageView(di, dev, **m_image, VK_IMAGE_VIEW_TYPE_2D, this->m_params.format, range);

    m_renderPass = makeRenderPass(di, dev, this->m_params.format);

    m_framebuffer = makeFramebuffer(di, dev, *m_renderPass, *m_imageView, this->m_params.width, this->m_params.height);

    Super::initCommonMembers();

    m_initialized = true;
}

template <typename FloatTy>
auto BFloat16GraphicsInstance<FloatTy>::prepareShaders() const -> std::vector<Move<VkShaderModule>>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    std::vector<Move<VkShaderModule>> modules;

    auto vert = createShaderModule(di, dev, this->m_context.getBinaryCollection().get("vert"), VkSamplerCreateFlags(0));
    auto frag = createShaderModule(di, dev, this->m_context.getBinaryCollection().get("frag"), VkSamplerCreateFlags(0));

    modules.emplace_back(std::move(vert));
    modules.emplace_back(std::move(frag));

    return modules;
}

template <typename FloatTy>
auto BFloat16GraphicsInstance<FloatTy>::prepareVertices(VkPrimitiveTopology topology) const -> Vertices
{
    DE_UNREF(topology);
    DE_ASSERT(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN == topology);

    //const tcu::Vec2 clockwise[]{{-1, +1}, {-1, -1}, {+1, -1}, {+1, -0.5}, {+1, 0}, {+1, +0.5}, {+1, +1}};
    const tcu::Vec2 ccw[]{{+1, +1}, {+1, -1}, {-1, -1}, {-1, -0.5}, {-1, 0}, {-1, +0.5}, {-1, +1}};

    Vertices result;
    result.reserve(DE_LENGTH_OF_ARRAY(ccw) * 2);
    for (const tcu::Vec2 &v : ccw)
    {
        result.emplace_back(v.x());
        result.emplace_back(v.y());
    }

    return result;
}

template <typename FloatTy>
void BFloat16GraphicsInstance<FloatTy>::prepareBuffers()
{
    Super::prepareBuffers();

    m_vertexBuffer = prepareVertexBuffer();

    {
        const DeviceInterface &di = this->m_context.getDeviceInterface();
        const uint32_t queueIndex = this->m_context.getUniversalQueueFamilyIndex();
        const VkDevice dev        = this->m_context.getDevice();
        Allocator &allocator      = this->m_context.getDefaultAllocator();
        const VkDeviceSize size =
            mapVkFormat(this->m_params.format).getPixelSize() * this->m_params.width * this->m_params.height;
        const VkBufferUsageFlags usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const VkBufferCreateInfo bci = makeBufferCreateInfo(size, usage, std::vector<uint32_t>{queueIndex});

        bf16::makeMovePtr(this->m_resultBuffer, di, dev, allocator, bci, MemoryRequirement::HostVisible);
    }
}

template <typename FloatTy>
auto BFloat16GraphicsInstance<FloatTy>::getVertexCount() const -> uint32_t
{
    return m_vertexBuffer ? uint32_t(m_vertexBuffer->getBufferSize() / sizeof(tcu::Vec4)) : 0u;
}

template <typename FloatTy>
void BFloat16GraphicsInstance<FloatTy>::copyImageToResultBuffer()
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const uint32_t queueIndex = this->m_context.getUniversalQueueFamilyIndex();

    const auto range   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto iBefore = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **this->m_image, range, queueIndex, queueIndex);
    const auto bBefore = makeBufferMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, **this->m_resultBuffer,
                                                 0u, VK_WHOLE_SIZE, queueIndex, queueIndex);
    const auto iAfter =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_IMAGE_LAYOUT_GENERAL, **this->m_image, range, queueIndex, queueIndex);
    const auto bAfter = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE, **this->m_resultBuffer,
                                                0u, VK_WHOLE_SIZE, queueIndex, queueIndex);

    const auto bicRegion = makeBufferImageCopy({this->m_params.width, this->m_params.height, 1u},
                                               makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    di.cmdPipelineBarrier(*this->m_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 1u, &bBefore, 1u, &iBefore);
    di.cmdCopyImageToBuffer(*this->m_cmd, **this->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **this->m_resultBuffer,
                            1u, &bicRegion);
    di.cmdPipelineBarrier(*this->m_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 1u, &bAfter, 1u, &iAfter);
}

template <typename FloatTy>
auto BFloat16GraphicsInstance<FloatTy>::iterate() -> tcu::TestStatus
{
    const DeviceInterface &di   = this->m_context.getDeviceInterface();
    const VkQueue queue         = this->m_context.getUniversalQueue();
    const VkDevice dev          = this->m_context.getDevice();
    const VkBuffer vertexbuffer = **m_vertexBuffer;
    const uint32_t vertexCount  = this->getVertexCount();

    beginCommandBuffer(di, *this->m_cmd);
    di.cmdBindPipeline(*this->m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *this->m_pipeline);
    di.cmdBindDescriptorSets(*this->m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *this->m_pipelineLayout, 0u, 1u,
                             bf16::fwd_as_ptr(*this->m_descriptorSet), 0u, nullptr);
    di.cmdBindVertexBuffers(*this->m_cmd, 0u, 1u, &vertexbuffer, bf16::fwd_as_ptr<VkDeviceSize>(0));
    beginRenderPass(di, *this->m_cmd, *this->m_renderPass, *this->m_framebuffer,
                    makeRect2D(this->m_params.width, this->m_params.height), 1u, bf16::fwd_as_ptr<VkClearValue>({}));
    di.cmdDraw(*this->m_cmd, vertexCount, 1u, 0u, 0u);
    endRenderPass(di, *this->m_cmd);
    copyImageToResultBuffer();
    endCommandBuffer(di, *this->m_cmd);
    submitCommandsAndWait(di, dev, queue, *this->m_cmd);

    const bool res = verifyResult();

    return (res ? tcu::TestStatus::pass : tcu::TestStatus::fail)(std::string());
}

// BFloat16VertexInstance
template <typename FloatTy>
auto BFloat16VertexInstance<FloatTy>::preparePipeline() const -> Move<VkPipeline>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    DE_ASSERT(*this->m_renderPass != VK_NULL_HANDLE);

    SpecializationInfo i;

    i.addEntry(this->m_vertices[0].asFloat());  // c0
    i.addEntry(this->m_vertices[1]);            // c1
    i.addEntry(this->m_vertices[2].asFloat());  // c2
    i.addEntry(this->m_vertices[3]);            // c3
    i.addEntry(this->m_vertices[4].asFloat());  // c4
    i.addEntry(this->m_vertices[5]);            // c5
    i.addEntry(this->m_vertices[6]);            // c6
    i.addEntry(this->m_vertices[7].asFloat());  // c7
    i.addEntry(this->m_vertices[8]);            // c8
    i.addEntry(this->m_vertices[9]);            // c9
    i.addEntry(this->m_vertices[10]);           // c10
    i.addEntry(this->m_vertices[11].asFloat()); // c11
    i.addEntry(this->m_vertices[12]);           // c12
    i.addEntry(this->m_vertices[13]);           // c13

    const vk::VkSpecializationInfo si = i.get();

    const VkPipelineShaderStageCreateInfo vertShaderCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits               stage
        *this->m_shaders[0],                                 // VkShaderModule                      module
        "main",                                              // const char*                         pName
        &si                                                  // const VkSpecializationInfo*         pSpecializationInfo
    };
    const VkPipelineShaderStageCreateInfo fragShaderCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits               stage
        *this->m_shaders[1],                                 // VkShaderModule                      module
        "main",                                              // const char*                         pName
        nullptr                                              // const VkSpecializationInfo*         pSpecializationInfo
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        this->m_topology, // VkPrimitiveTopology                        topology
        VK_FALSE          // VkBool32                                   primitiveRestartEnable
    };

    return makeGraphicsPipeline(di, dev, VK_NULL_HANDLE, *this->m_pipelineLayout, VkPipelineCreateFlags(0),
                                {vertShaderCreateInfo, fragShaderCreateInfo}, *this->m_renderPass,
                                {makeViewport(this->m_params.width, this->m_params.height)},
                                {makeRect2D(this->m_params.width, this->m_params.height)}, 0u, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr, &inputAssemblyCreateInfo);
}

template <typename FloatTy>
auto BFloat16VertexInstance<FloatTy>::prepareVertexBuffer() const -> de::MovePtr<BufferWithMemory>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const uint32_t queueIndex = this->m_context.getUniversalQueueFamilyIndex();
    const VkDevice dev        = this->m_context.getDevice();
    Allocator &allocator      = this->m_context.getDefaultAllocator();

    const uint32_t vertexCount = uint32_t(this->m_vertices.size() / 2);
    const VkDeviceSize size    = mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT).getPixelSize() * vertexCount;
    const VkBufferCreateInfo bci =
        makeBufferCreateInfo(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, std::vector<uint32_t>{queueIndex});

    de::MovePtr<BufferWithMemory> vertexBuffer;
    bf16::makeMovePtr(vertexBuffer, di, dev, allocator, bci, MemoryRequirement::HostVisible);

    deMemset(vertexBuffer->getAllocation().getHostPtr(), 0, size_t(size));
    flushAlloc(di, dev, vertexBuffer->getAllocation());

    return vertexBuffer;
}

template <typename FloatTy>
bool BFloat16VertexInstance<FloatTy>::verifyResult() const
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    const tcu::Vec4 white     = tcu::Vec4(1, 1, 1, 1);

    invalidateAlloc(di, dev, this->m_outBufferZ->getAllocation());
    invalidateAlloc(di, dev, this->m_resultBuffer->getAllocation());

    const typename Super::FloatTyVec4 *outBufferZ =
        reinterpret_cast<typename Super::FloatTyVec4 *>(this->m_outBufferZ->getAllocation().getHostPtr());
    const tcu::ConstPixelBufferAccess resultBuffer(mapVkFormat(this->m_params.format), int(this->m_params.width),
                                                   int(this->m_params.height), 1,
                                                   this->m_resultBuffer->getAllocation().getHostPtr());

    auto barycentrumColor = [&](const tcu::Vec2 &a, const tcu::Vec2 &b, const tcu::Vec2 &c) -> tcu::Vec4
    {
        const float fx        = (a.x() + b.x() + c.x()) / 3.0f;
        const float fy        = (a.y() + b.y() + c.y()) / 3.0f;
        const int ix          = int(((fx + 1.0f) / 2.0f) * float(this->m_params.width));
        const int iy          = int(((fy + 1.0f) / 2.0f) * float(this->m_params.height));
        const tcu::Vec4 color = resultBuffer.getPixel(ix, iy);
        return color;
    };

    uint32_t triangles = 0;

    for (uint32_t i = 0u; i < uint32_t(this->m_vertices.size()); ++i)
    {
        const float ref = this->m_vertices[i].asFloat();
        const float out = outBufferZ[i][1].asFloat();
        if (ref != out)
            return false;

        if ((i >= 5u) && (i % 2u))
        {
            ++triangles;

            const tcu::Vec2 a(this->m_vertices[0u].asFloat(), this->m_vertices[1u].asFloat());
            const tcu::Vec2 b(this->m_vertices[i - 1u].asFloat(), this->m_vertices[i].asFloat());
            const tcu::Vec2 c(this->m_vertices[i - 3u].asFloat(), this->m_vertices[i - 2u].asFloat());

            if (white != barycentrumColor(a, b, c))
                return false;
        }
    }

    return ((this->getVertexCount() - 2u) == triangles);
}

// BFloat16FragmentInstance
template <typename FloatTy>
auto BFloat16FragmentInstance<FloatTy>::preparePipeline() const -> Move<VkPipeline>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();
    DE_ASSERT(*this->m_renderPass != VK_NULL_HANDLE);

    SpecializationInfo i;

    i.addEntry(this->m_vertices[0].asFloat());  // c0
    i.addEntry(this->m_vertices[1]);            // c1
    i.addEntry(this->m_vertices[2].asFloat());  // c2
    i.addEntry(this->m_vertices[3]);            // c3
    i.addEntry(this->m_vertices[4].asFloat());  // c4
    i.addEntry(this->m_vertices[5]);            // c5
    i.addEntry(this->m_vertices[6]);            // c6
    i.addEntry(this->m_vertices[7].asFloat());  // c7
    i.addEntry(this->m_vertices[8]);            // c8
    i.addEntry(this->m_vertices[9]);            // c9
    i.addEntry(this->m_vertices[10]);           // c10
    i.addEntry(this->m_vertices[11].asFloat()); // c11
    i.addEntry(this->m_vertices[12]);           // c12
    i.addEntry(this->m_vertices[13]);           // c13

    const vk::VkSpecializationInfo si = i.get();

    const VkPipelineShaderStageCreateInfo vertShaderCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits               stage
        *this->m_shaders[0],                                 // VkShaderModule                      module
        "main",                                              // const char*                         pName
        nullptr                                              // const VkSpecializationInfo*         pSpecializationInfo
    };
    const VkPipelineShaderStageCreateInfo fragShaderCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                     sType
        nullptr,                                             // const void*                         pNext
        0u,                                                  // VkPipelineShaderStageCreateFlags    flags
        VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits               stage
        *this->m_shaders[1],                                 // VkShaderModule                      module
        "main",                                              // const char*                         pName
        &si                                                  // const VkSpecializationInfo*         pSpecializationInfo
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        this->m_topology, // VkPrimitiveTopology                        topology
        VK_FALSE          // VkBool32                                   primitiveRestartEnable
    };

    return makeGraphicsPipeline(di, dev, VK_NULL_HANDLE, *this->m_pipelineLayout, VkPipelineCreateFlags(0),
                                {vertShaderCreateInfo, fragShaderCreateInfo}, *this->m_renderPass,
                                {makeViewport(this->m_params.width, this->m_params.height)},
                                {makeRect2D(this->m_params.width, this->m_params.height)}, 0u, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr, &inputAssemblyCreateInfo);
}

template <typename FloatTy>
auto BFloat16FragmentInstance<FloatTy>::prepareVertexBuffer() const -> de::MovePtr<BufferWithMemory>
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const uint32_t queueIndex = this->m_context.getUniversalQueueFamilyIndex();
    const VkDevice dev        = this->m_context.getDevice();
    Allocator &allocator      = this->m_context.getDefaultAllocator();

    const uint32_t vertexCount = uint32_t(this->m_vertices.size() / 2);
    const VkDeviceSize size    = mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT).getPixelSize() * vertexCount;
    const VkBufferCreateInfo bci =
        makeBufferCreateInfo(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, std::vector<uint32_t>{queueIndex});

    de::MovePtr<BufferWithMemory> vertexBuffer;
    bf16::makeMovePtr(vertexBuffer, di, dev, allocator, bci, MemoryRequirement::HostVisible);

    std::vector<tcu::Vec4> vertexData(vertexCount);
    for (uint32_t i = 0u; i < uint32_t(this->m_vertices.size()); ++i)
    {
        if (i % 2u)
        {
            const tcu::Vec4 v(this->m_vertices[i - 1u].asFloat(), this->m_vertices[i].asFloat(), 0.0f, 1.0f);
            vertexData[i / 2u] = v;
        }
    }

    deMemcpy(vertexBuffer->getAllocation().getHostPtr(), vertexData.data(), size_t(size));
    flushAlloc(di, dev, vertexBuffer->getAllocation());

    return vertexBuffer;
}

template <typename FloatTy>
bool BFloat16FragmentInstance<FloatTy>::verifyResult() const
{
    const DeviceInterface &di = this->m_context.getDeviceInterface();
    const VkDevice dev        = this->m_context.getDevice();

    invalidateAlloc(di, dev, this->m_outBufferZ->getAllocation());
    invalidateAlloc(di, dev, this->m_resultBuffer->getAllocation());

    const typename Super::FloatTyVec4 *outBufferZ =
        reinterpret_cast<typename Super::FloatTyVec4 *>(this->m_outBufferZ->getAllocation().getHostPtr());
    const tcu::ConstPixelBufferAccess resultBuffer(mapVkFormat(this->m_params.format), int(this->m_params.width),
                                                   int(this->m_params.height), 1,
                                                   this->m_resultBuffer->getAllocation().getHostPtr());

    auto barycentrumColor = [&](const tcu::Vec2 &a, const tcu::Vec2 &b, const tcu::Vec2 &c) -> tcu::Vec4
    {
        const float fx        = (a.x() + b.x() + c.x()) / 3.0f;
        const float fy        = (a.y() + b.y() + c.y()) / 3.0f;
        const int ix          = int(((fx + 1.0f) / 2.0f) * float(this->m_params.width));
        const int iy          = int(((fy + 1.0f) / 2.0f) * float(this->m_params.height));
        const tcu::Vec4 color = resultBuffer.getPixel(ix, iy);
        return color;
    };

    uint32_t triangles = 0;

    for (uint32_t i = 0u; i < uint32_t(this->m_vertices.size()); ++i)
    {
        const float ref = this->m_vertices[i].asFloat();
        const float out = outBufferZ[i][1].asFloat();
        if (ref != out)
            return false;

        if ((i >= 5u) && (i % 2u))
        {
            ++triangles;

            const tcu::Vec2 a(this->m_vertices[0u].asFloat(), this->m_vertices[1u].asFloat());
            const tcu::Vec2 b(this->m_vertices[i - 1u].asFloat(), this->m_vertices[i].asFloat());
            const tcu::Vec2 c(this->m_vertices[i - 3u].asFloat(), this->m_vertices[i - 2u].asFloat());

            const float d = float(triangles);

            if (tcu::Vec4(d, d, d, 1.0f) != barycentrumColor(a, b, c))
                return false;
        }
    }

    return ((this->getVertexCount() - 2u) == triangles);
}

template <typename TestClass>
TestCase *createTest(tcu::TestContext &ctx, const std::string &name, const Params &p)
{
    return new TestClass(ctx, name, p);
}

} // unnamed namespace

void createBFloat16ConstantTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16)
{
    // XXX TODO change test grouping after rebase
    std::tuple<const std::string, vkt::TestCase *(&)(tcu::TestContext &, const std::string &, const Params &)> ooo[]{
        {"computebf16", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_COMPUTE_BIT, BFloat16>>},
        {"vertexbf16", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_VERTEX_BIT, BFloat16>>},
        {"fragmentbf16", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_FRAGMENT_BIT, BFloat16>>},
        {"computefe5m2", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_COMPUTE_BIT, tcu::FloatE5M2>>},
        {"vertexfe5m2", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_VERTEX_BIT, tcu::FloatE5M2>>},
        {"fragmentfe5m2", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_FRAGMENT_BIT, tcu::FloatE5M2>>},
        {"computefe4m3", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_COMPUTE_BIT, tcu::FloatE4M3>>},
        {"vertexfe4m3", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_VERTEX_BIT, tcu::FloatE4M3>>},
        {"fragmentfe4m3", createTest<BFloat16ConstantCaseT<VK_SHADER_STAGE_FRAGMENT_BIT, tcu::FloatE4M3>>},
    };

    de::MovePtr<tcu::TestCaseGroup> constant(
        new tcu::TestCaseGroup(testCtx, "constant", "Tests of constant_id for bfloat16 type"));

    Params p;
    for (const auto &test : ooo)
    {
        constant->addChild(std::get<1>(test)(testCtx, std::get<0>(test), p));
    }

    return bfloat16->addChild(constant.release());
}

} // namespace shaderexecutor
} // namespace vkt
