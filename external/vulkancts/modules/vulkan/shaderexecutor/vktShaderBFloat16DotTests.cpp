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
 * \brief Tests for types introduced in VK_KHR_shader_bfloat16.
 *//*--------------------------------------------------------------------*/

#include "vktShaderBFloat16Tests.hpp"
#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "tcuFloat.hpp"
#include "deRandom.hpp"
#include "tcuStringTemplate.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <type_traits>
#include <sstream>

namespace vkt
{
namespace shaderexecutor
{
template <class, uint32_t>
const char *Bf16VecTypeName;

namespace
{
using namespace vk;

// Do simple trick, just change the right side of below equation
// to switch whole stuff to work with regular float16 type.
using BFloat16 = tcu::BrainFloat16;

enum class InTypes : uint32_t
{
    VEC2 = 1,
    VEC3,
    VEC4
};

struct Params
{
    uint32_t seed;
    InTypes type;
};

class BFloat16OpDotCase : public TestCase
{
    const Params m_params;

public:
    BFloat16OpDotCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~BFloat16OpDotCase() = default;

    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override;
    virtual void checkSupport(Context &context) const override
    {
        if (!context.get16BitStorageFeatures().storageBuffer16BitAccess)
        {
            TCU_THROW(NotSupportedError, "16-bit floats not supported for storage buffers");
        }

        if constexpr (std::is_same_v<BFloat16, tcu::BrainFloat16>)
        {
            if ((context.getShaderBfloat16Features().shaderBFloat16Type != VK_TRUE) ||
                (context.getShaderBfloat16Features().shaderBFloat16DotProduct != VK_TRUE))
            {
                TCU_THROW(NotSupportedError, "shaderBFloat16DotProduct not supported by device");
            }
        }
        else if constexpr (std::is_same_v<BFloat16, tcu::Float16>)
        {
            if (!context.getShaderFloat16Int8Features().shaderFloat16)
            {
                TCU_THROW(NotSupportedError, "16-bit floats not supported in shader code");
            }
        }
        else
        {
            TCU_THROW(NotSupportedError, "Unknown float type");
        }
    }
};

void BFloat16OpDotCase::initPrograms(SourceCollections &programCollection) const
{
    const tcu::StringTemplate glslCodeTemplate(
        R"(
    #version 450
    #extension ${EXTENSION}: require
    layout(binding=0) buffer InBufferX { ${VEC4} x[]; };
    layout(binding=1) buffer InBufferY { ${VEC4} y[]; };
    layout(binding=2) buffer OutBuffer { ${VEC1} z[]; };
    layout(push_constant) uniform PC { uint mode; };
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    void main() {
        uint id = gl_WorkGroupID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y
            + gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
        switch (mode) {
            case ${CASE4}:
                 z[id] = dot(${VEC4}(x[id]), ${VEC4}(y[id]));
                 break;
            case ${CASE3}:
                 z[id] = dot(${VEC3}(x[id]), ${VEC3}(y[id]));
                 break;
            case ${CASE2}:
                 z[id] = dot(${VEC2}(x[id]), ${VEC2}(y[id]));
                 break;
            default:
                 z[id] = ${VEC1}(1.0);
        }
    }
    )");

    const std::map<std::string, std::string> substs{
        {"EXTENSION", bf16::getExtensionName<BFloat16>()},  {"CASE4", std::to_string(uint32_t(InTypes::VEC4))},
        {"CASE3", std::to_string(uint32_t(InTypes::VEC3))}, {"CASE2", std::to_string(uint32_t(InTypes::VEC2))},
        {"VEC4", bf16::getVecTypeName<BFloat16, 4>()},      {"VEC3", bf16::getVecTypeName<BFloat16, 3>()},
        {"VEC2", bf16::getVecTypeName<BFloat16, 2>()},      {"VEC1", bf16::getVecTypeName<BFloat16, 1>()},
    };

    const std::string glslCode = glslCodeTemplate.specialize(substs);
    programCollection.glslSources.add("test") << glu::ComputeSource(glslCode);
}

class BFloat16OpDotInstance : public TestInstance
{
    const Params m_params;
    const uint32_t m_alwaysExistent;

public:
    using BFloat16Vec4 = std::array<BFloat16, 4>;
    static_assert(sizeof(BFloat16) == sizeof(BFloat16::StorageType), "???");
    static_assert(sizeof(BFloat16Vec4) == sizeof(BFloat16::StorageType) * 4u, "???");

    BFloat16OpDotInstance(Context &context, const Params &params)
        : TestInstance(context)
        , m_params(params)
        , m_alwaysExistent(5u)
    {
    }
    virtual tcu::TestStatus iterate() override;

protected:
    void generateInputData(BufferWithMemory &where, uint32_t count, de::Random &rnd, bool leftArgument);
    uint32_t verifyResults(const BufferWithMemory &lhs, const BufferWithMemory &rhs, const BufferWithMemory &res,
                           uint32_t count);
};

TestInstance *BFloat16OpDotCase::createInstance(Context &context) const
{
    return new BFloat16OpDotInstance(context, m_params);
}

tcu::TestStatus BFloat16OpDotInstance::iterate()
{
    const DeviceInterface &di = m_context.getDeviceInterface();
    const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue       = m_context.getUniversalQueue();
    const VkDevice dev        = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    de::Random rnd(m_params.seed);

    const std::vector<uint32_t> queueIndices{queueIndex};
    const uint32_t ioCount          = (rnd.getUint32() + m_alwaysExistent) % 64u;
    const VkDeviceSize inBytesSize  = ioCount * sizeof(BFloat16Vec4);
    const VkDeviceSize outBytesSize = ioCount * sizeof(BFloat16);
    const VkBufferCreateInfo inBufferCI =
        makeBufferCreateInfo(inBytesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT /*, queueIndices*/);
    BufferWithMemory inBufferX(di, dev, allocator, inBufferCI, MemoryRequirement::HostVisible);
    BufferWithMemory inBufferY(di, dev, allocator, inBufferCI, MemoryRequirement::HostVisible);
    const VkBufferCreateInfo outBufferCI =
        makeBufferCreateInfo(outBytesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT /*, queueIndices*/);
    BufferWithMemory outBuffer(di, dev, allocator, outBufferCI, MemoryRequirement::HostVisible);
    const VkDescriptorBufferInfo inBufferXDBI = makeDescriptorBufferInfo(*inBufferX, 0u, inBytesSize);
    const VkDescriptorBufferInfo inBufferYDBI = makeDescriptorBufferInfo(*inBufferY, 0u, inBytesSize);
    const VkDescriptorBufferInfo outBufferDBI = makeDescriptorBufferInfo(*outBuffer, 0u, outBytesSize);
    Move<VkDescriptorPool> dsPool             = DescriptorPoolBuilder()
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
                                        .build(di, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> dsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(di, dev);
    Move<VkDescriptorSet> ds = makeDescriptorSet(di, dev, *dsPool, *dsLayout);
    DescriptorSetUpdateBuilder()
        .writeSingle(*ds, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &inBufferXDBI)
        .writeSingle(*ds, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &inBufferYDBI)
        .writeSingle(*ds, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &outBufferDBI)
        .update(di, dev);
    struct PushConstant
    {
        uint32_t mode;
    } const pushConstant{uint32_t(m_params.type)};
    const VkPushConstantRange range{
        VK_SHADER_STAGE_COMPUTE_BIT,   //
        0u,                            //
        (uint32_t)sizeof(pushConstant) //
    };
    Move<VkShaderModule> shader           = createShaderModule(di, dev, m_context.getBinaryCollection().get("test"), 0);
    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(di, dev, *dsLayout, &range);
    Move<VkPipeline> pipeline             = makeComputePipeline(di, dev, *pipelineLayout, *shader);
    Move<VkCommandPool> cmdPool           = makeCommandPool(di, dev, queueIndex);
    Move<VkCommandBuffer> cmd             = allocateCommandBuffer(di, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    generateInputData(inBufferX, ioCount, rnd, true);
    generateInputData(inBufferY, ioCount, rnd, false);

    beginCommandBuffer(di, *cmd);
    di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &ds.get(), 0u, nullptr);
    di.cmdPushConstants(*cmd, *pipelineLayout, range.stageFlags, range.offset, range.size, &pushConstant);
    di.cmdDispatch(*cmd, ioCount, 1u, 1u);
    endCommandBuffer(di, *cmd);
    submitCommandsAndWait(di, dev, queue, *cmd);

    invalidateAlloc(di, dev, outBuffer.getAllocation());

    const uint32_t mismatch = verifyResults(inBufferX, inBufferY, outBuffer, ioCount);

    if (mismatch == 0u)
    {
        return tcu::TestStatus::pass(std::string());
    }

    std::ostringstream os;
    os << "Mismatches " << mismatch << " from " << ioCount;
    os.flush();

    return tcu::TestStatus::fail(os.str());
}
void BFloat16OpDotInstance::generateInputData(BufferWithMemory &where, uint32_t count, de::Random &rnd,
                                              bool leftArgument)
{
    DE_UNREF(leftArgument);
    std::vector<BFloat16Vec4> v(count);

    const uint32_t nan = rnd.getUint32() % count;
    const uint32_t inf = rnd.getUint32() % count;

    for (uint32_t c = 0u; c < count; ++c)
    {
        BFloat16Vec4 x;

        for (uint32_t k = 0u; k < 4; ++k)
        {
            x[k] = (2 == k && nan == c) ? BFloat16::nan() :
                   (2 == k && inf == c) ? BFloat16::inf((inf % 2) ? 1 : (-1)) :
                                          BFloat16(float(int(rnd.getUint32() % 15u) - 7) / 2.f);
        }

        v[c] = x;
    }

    auto range = bf16::makeStdBeginEnd<BFloat16Vec4>(where.getAllocation().getHostPtr(), count);
    std::copy(v.begin(), v.end(), range.first);
    flushAlloc(m_context.getDeviceInterface(), m_context.getDevice(), where.getAllocation());
}

uint32_t BFloat16OpDotInstance::verifyResults(const BufferWithMemory &lhs, const BufferWithMemory &rhs,
                                              const BufferWithMemory &res, uint32_t count)
{
    const BFloat16Vec4 *left  = reinterpret_cast<const BFloat16Vec4 *>(lhs.getAllocation().getHostPtr());
    const BFloat16Vec4 *right = reinterpret_cast<const BFloat16Vec4 *>(rhs.getAllocation().getHostPtr());
    const BFloat16 *dot       = reinterpret_cast<const BFloat16 *>(res.getAllocation().getHostPtr());

    const auto asFloat = [](const BFloat16 &bValue) -> float { return bValue.asFloat(); };

    const auto inputHasNan = [&](const uint32_t at)
    {
        for (uint32_t j = 0u; j < (uint32_t(m_params.type) + 1u); ++j)
        {
            if (left[at][j].isNaN() || right[at][j].isNaN())
                return true;
        }
        return false;
    };

    uint32_t mismatch = 0u;

    for (uint32_t i = 0u; i < count; ++i)
    {
        if (inputHasNan(i))
        {
            if (false == dot[i].isNaN())
            {
                mismatch = mismatch + 1u;
            }
        }
        else
        {
            std::array<float, 4> leftArg, rightArg;
            std::transform(left[i].cbegin(), left[i].cend(), leftArg.begin(), asFloat);
            std::transform(right[i].cbegin(), right[i].cend(), rightArg.begin(), asFloat);
            const float prod = std::inner_product(
                leftArg.cbegin(), std::next(leftArg.cbegin(), uint32_t(m_params.type) + 1u), rightArg.cbegin(), 0.0f);
            if (prod != asFloat(dot[i]))
            {
                mismatch = mismatch + 1u;
            }
        }
    }

    return mismatch;
}

} // unnamed namespace

void createBFloat16DotTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *bfloat16)
{
    const std::pair<const std::string, InTypes> cases[]{
        {"vec2", InTypes::VEC2},
        {"vec3", InTypes::VEC3},
        {"vec4", InTypes::VEC4},
    };
    uint32_t seed = 19u;
    de::MovePtr<tcu::TestCaseGroup> dot(new tcu::TestCaseGroup(testCtx, "dot", "Dot tests for bfloat16 type"));

    for (const std::pair<const std::string, InTypes> &aCase : cases)
    {
        Params p{seed++, aCase.second};
        dot->addChild(new BFloat16OpDotCase(testCtx, aCase.first, p));
    }

    bfloat16->addChild(dot.release());
}

} // namespace shaderexecutor
} // namespace vkt
