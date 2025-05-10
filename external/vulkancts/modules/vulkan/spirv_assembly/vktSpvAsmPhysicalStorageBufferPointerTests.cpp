/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for PhysicalStorageBuffer.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmPhysicalStorageBufferPointerTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include <iterator>

using namespace vk;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

enum class PassMethod
{
    PUSH_CONSTANTS,
    PUSH_CONSTANTS_FUNCTION,
    ADDRESSES_IN_SSBO
};

struct TestParams
{
    PassMethod method;
    uint32_t elements;
};

typedef SharedPtr<const TestParams> TestParamsPtr;

namespace ut
{

class Buffer
{
public:
    Buffer(Context &ctx, VkBufferUsageFlags usage, VkDeviceSize size, bool address = false);
    Buffer(const Buffer &src) = delete;

    VkBuffer getBuffer(void) const
    {
        return **m_buffer;
    }
    VkDeviceSize getSize(void) const
    {
        return m_size;
    }
    void *getData(void) const
    {
        return (*m_bufferMemory)->getHostPtr();
    }
    uint64_t getDeviceAddress(void) const;
    void zero(bool flushAfter = false);
    void flush(void);
    void invalidate(void);

protected:
    Context &m_context;
    const VkDeviceSize m_size;
    const bool m_address;
    SharedPtr<Move<VkBuffer>> m_buffer;
    SharedPtr<MovePtr<Allocation>> m_bufferMemory;
};

template <class X>
class TypedBuffer : public Buffer
{
public:
    TypedBuffer(Context &ctx, VkBufferUsageFlags usage, uint32_t nelements, bool address = false);
    TypedBuffer(Context &ctx, VkBufferUsageFlags usage, std::initializer_list<X> items, bool address = false);
    TypedBuffer(const TypedBuffer &src);
    TypedBuffer(const Buffer &src);

    uint32_t getElements(void) const
    {
        return m_elements;
    }
    X *getData(void) const
    {
        return reinterpret_cast<X *>(Buffer::getData());
    }
    void iota(X start, bool flushAfter = false);
    X &operator[](uint32_t at);

    struct iterator;
    iterator begin()
    {
        return iterator(getData());
    }
    iterator end()
    {
        return iterator(&getData()[m_elements]);
    }

private:
    const uint32_t m_elements;
};

template <class X>
SharedPtr<Move<X>> makeShared(Move<X> move)
{
    return SharedPtr<Move<X>>(new Move<X>(move));
}

template <class X>
SharedPtr<MovePtr<X>> makeShared(MovePtr<X> move)
{
    return SharedPtr<MovePtr<X>>(new MovePtr<X>(move));
}

Buffer::Buffer(Context &ctx, VkBufferUsageFlags usage, VkDeviceSize size, bool address)
    : m_context(ctx)
    , m_size(size)
    , m_address(address)
{
    const DeviceInterface &vki      = m_context.getDeviceInterface();
    const VkDevice dev              = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const VkBufferUsageFlags bufferUsageFlags = address ? (usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) : usage;
    const MemoryRequirement requirements      = MemoryRequirement::Coherent | MemoryRequirement::HostVisible |
                                           (address ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any);

    const VkBufferCreateInfo bufferCreateInfo{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        size,                                 // VkDeviceSize size;
        bufferUsageFlags,                     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
    };

    m_buffer       = makeShared(createBuffer(vki, dev, &bufferCreateInfo));
    m_bufferMemory = makeShared(allocator.allocate(getBufferMemoryRequirements(vki, dev, **m_buffer), requirements));

    VK_CHECK(vki.bindBufferMemory(dev, **m_buffer, (*m_bufferMemory)->getMemory(), (*m_bufferMemory)->getOffset()));
}

uint64_t Buffer::getDeviceAddress(void) const
{
    DE_ASSERT(m_address);

    const DeviceInterface &vki = m_context.getDeviceInterface();
    const VkDevice dev         = m_context.getDevice();
    const VkBufferDeviceAddressInfo info{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        **m_buffer                                    // VkBuffer buffer;
    };

    return vki.getBufferDeviceAddress(dev, &info);
}

void Buffer::zero(bool flushAfter)
{
    deMemset(getData(), 0, static_cast<size_t>(m_size));
    if (flushAfter)
        flush();
}

void Buffer::flush(void)
{
    const DeviceInterface &vki = m_context.getDeviceInterface();
    const VkDevice dev         = m_context.getDevice();
    flushAlloc(vki, dev, **m_bufferMemory);
}

void Buffer::invalidate(void)
{
    const DeviceInterface &vki = m_context.getDeviceInterface();
    const VkDevice dev         = m_context.getDevice();
    invalidateAlloc(vki, dev, **m_bufferMemory);
}

template <class X>
struct TypedBuffer<X>::iterator
{
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;
    typedef X value_type;
    typedef X &reference;
    typedef X *pointer;

    iterator(pointer p) : m_p(p)
    {
        DE_ASSERT(p);
    }
    reference operator*()
    {
        return *m_p;
    }
    iterator &operator++()
    {
        ++m_p;
        return *this;
    }
    iterator operator++(int)
    {
        return iterator(m_p++);
    }
    bool operator==(const iterator &other) const
    {
        return (m_p == other.m_p);
    }
    bool operator!=(const iterator &other) const
    {
        return (m_p != other.m_p);
    }

private:
    pointer m_p;
};

template <class X>
TypedBuffer<X>::TypedBuffer(Context &ctx, VkBufferUsageFlags usage, uint32_t nelements, bool address)
    : Buffer(ctx, usage, (nelements * sizeof(X)), address)
    , m_elements(nelements)
{
}

template <class X>
TypedBuffer<X>::TypedBuffer(Context &ctx, VkBufferUsageFlags usage, std::initializer_list<X> items, bool address)
    : Buffer(ctx, usage, (items.size() * sizeof(X)), address)
    , m_elements(static_cast<uint32_t>(items.size()))
{
    std::copy(items.begin(), items.end(), begin());
}

template <class X>
TypedBuffer<X>::TypedBuffer(const TypedBuffer &src) : Buffer(src)
                                                    , m_elements(src.m_elements)
{
}

template <class X>
void TypedBuffer<X>::iota(X start, bool flushAfter)
{
    X *data = getData();
    for (uint32_t i = 0; i < m_elements; ++i)
        data[i] = start++;
    if (flushAfter)
        flush();
}

template <class X>
X &TypedBuffer<X>::operator[](uint32_t at)
{
    DE_ASSERT(at < m_elements);
    return getData()[at];
}

} // namespace ut

class SpvAsmPhysicalStorageBufferTestInstance : public TestInstance
{
public:
    SpvAsmPhysicalStorageBufferTestInstance(Context &ctx) : TestInstance(ctx)
    {
    }
};

class SpvAsmPhysicalStorageBufferPushConstantsTestInstance : public SpvAsmPhysicalStorageBufferTestInstance
{
public:
    SpvAsmPhysicalStorageBufferPushConstantsTestInstance(Context &ctx, const TestParamsPtr params)
        : SpvAsmPhysicalStorageBufferTestInstance(ctx)
        , m_params(params)
    {
    }
    tcu::TestStatus iterate(void);
    static void initPrograms(vk::SourceCollections &programCollection, const TestParamsPtr params);

private:
    const TestParamsPtr m_params;
};

class SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance : public SpvAsmPhysicalStorageBufferTestInstance
{
public:
    SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance(Context &ctx, const TestParamsPtr params)
        : SpvAsmPhysicalStorageBufferTestInstance(ctx)
        , m_params(params)
    {
    }
    tcu::TestStatus iterate(void);
    static void initPrograms(vk::SourceCollections &programCollection, const TestParamsPtr params);

private:
    const TestParamsPtr m_params;
};

class SpvAsmPhysicalStorageBufferTestCase : public TestCase
{
public:
    SpvAsmPhysicalStorageBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParamsPtr params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &ctx) const;

private:
    const TestParamsPtr m_params;
};

void SpvAsmPhysicalStorageBufferTestCase::checkSupport(Context &context) const
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

    if (!context.isBufferDeviceAddressSupported())
        TCU_THROW(NotSupportedError, "Request physical storage buffer feature not supported");

    if (m_params->method == PassMethod::ADDRESSES_IN_SSBO)
    {
        if (!context.getDeviceFeatures().shaderInt64)
            TCU_THROW(NotSupportedError, "Int64 not supported");
    }
}

TestInstance *SpvAsmPhysicalStorageBufferTestCase::createInstance(Context &ctx) const
{
    switch (m_params->method)
    {
    case PassMethod::PUSH_CONSTANTS:
    case PassMethod::PUSH_CONSTANTS_FUNCTION:
        return new SpvAsmPhysicalStorageBufferPushConstantsTestInstance(ctx, m_params);

    case PassMethod::ADDRESSES_IN_SSBO:
        return new SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance(ctx, m_params);
    }

    DE_ASSERT(false);
    return nullptr;
}

void SpvAsmPhysicalStorageBufferTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    switch (m_params->method)
    {
    case PassMethod::PUSH_CONSTANTS:
    case PassMethod::PUSH_CONSTANTS_FUNCTION:
        SpvAsmPhysicalStorageBufferPushConstantsTestInstance::initPrograms(programCollection, m_params);
        break;

    case PassMethod::ADDRESSES_IN_SSBO:
        SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::initPrograms(programCollection, m_params);
        break;
    }
}

void SpvAsmPhysicalStorageBufferPushConstantsTestInstance::initPrograms(vk::SourceCollections &programCollection,
                                                                        const TestParamsPtr params)
{
    DE_UNREF(params);

    const std::string program(R"(
    OpCapability Shader
    OpCapability PhysicalStorageBufferAddresses

    OpExtension "SPV_KHR_physical_storage_buffer"
    OpMemoryModel PhysicalStorageBuffer64 GLSL450

    OpEntryPoint GLCompute %main "main" %id %str

    OpExecutionMode %main LocalSize 1 1 1
    OpSource GLSL 450
    OpName %main    "main"
    OpName %id        "gl_GlobalInvocationID"
    OpName %src        "source"
    OpName %dst        "destination"
    OpName %src_buf    "source"
    OpName %dst_buf    "destination"
    OpDecorate %id BuiltIn GlobalInvocationId

    OpDecorate %str_t Block
    OpMemberDecorate %str_t 0 Offset 0
    OpMemberDecorate %str_t 1 Offset 8
    OpMemberDecorate %str_t 2 Offset 16
    OpMemberDecorate %str_t 3 Offset 20

    OpDecorate %src_buf Restrict
    OpDecorate %dst_buf Restrict

    OpDecorate %int_arr ArrayStride 4

            %int = OpTypeInt 32 1
        %int_ptr = OpTypePointer PhysicalStorageBuffer %int
       %int_fptr = OpTypePointer Function %int
           %zero = OpConstant %int 0
            %one = OpConstant %int 1
            %two = OpConstant %int 2
          %three = OpConstant %int 3

           %uint = OpTypeInt 32 0
       %uint_ptr = OpTypePointer Input %uint
      %uint_fptr = OpTypePointer Function %uint
          %uvec3 = OpTypeVector %uint 3
      %uvec3ptr  = OpTypePointer Input %uvec3
          %uzero = OpConstant %uint 0
             %id = OpVariable %uvec3ptr Input

        %int_arr = OpTypeRuntimeArray %int

        %buf_ptr = OpTypePointer PhysicalStorageBuffer %int_arr
          %str_t = OpTypeStruct %buf_ptr %buf_ptr %int %int
        %str_ptr = OpTypePointer PushConstant %str_t
            %str = OpVariable %str_ptr PushConstant
    %buf_ptr_fld = OpTypePointer PushConstant %buf_ptr
        %int_fld = OpTypePointer PushConstant %int

           %bool = OpTypeBool
           %void = OpTypeVoid
          %voidf = OpTypeFunction %void
       %cpbuffsf = OpTypeFunction %void %buf_ptr %buf_ptr %int

        %cpbuffs = OpFunction %void None %cpbuffsf
        %src_buf = OpFunctionParameter %buf_ptr
        %dst_buf = OpFunctionParameter %buf_ptr
       %elements = OpFunctionParameter %int
       %cp_begin = OpLabel
              %j = OpVariable %int_fptr Function
                   OpStore %j %zero
                   OpBranch %for
            %for = OpLabel
             %vj = OpLoad %int %j
             %cj = OpULessThan %bool %vj %elements
                   OpLoopMerge %for_end %incj None
                   OpBranchConditional %cj %for_body %for_end
       %for_body = OpLabel
     %src_el_lnk = OpAccessChain %int_ptr %src_buf %vj
     %dst_el_lnk = OpAccessChain %int_ptr %dst_buf %vj
         %src_el = OpLoad %int %src_el_lnk Aligned 4
                   OpStore %dst_el_lnk %src_el Aligned 4
                   OpBranch %incj
           %incj = OpLabel
             %nj = OpIAdd %int %vj %one
                   OpStore %j %nj
                   OpBranch %for
        %for_end = OpLabel
                   OpReturn
                   OpFunctionEnd

           %main = OpFunction %void None %voidf
          %begin = OpLabel
              %i = OpVariable %int_fptr Function
                   OpStore %i %zero
        %src_lnk = OpAccessChain %buf_ptr_fld %str %zero
        %dst_lnk = OpAccessChain %buf_ptr_fld %str %one
        %cnt_lnk = OpAccessChain %int_fld %str %two
    %use_fun_lnk = OpAccessChain %int_fld %str %three
            %src = OpLoad %buf_ptr %src_lnk
            %dst = OpLoad %buf_ptr %dst_lnk
            %cnt = OpLoad %int %cnt_lnk
        %use_fun = OpLoad %int %use_fun_lnk

            %cuf = OpINotEqual %bool %use_fun %zero
                   OpSelectionMerge %use_fun_end None
                   OpBranchConditional %cuf %copy %loop
           %copy = OpLabel
         %unused = OpFunctionCall %void %cpbuffs %src %dst %cnt
                   OpBranch %use_fun_end
           %loop = OpLabel
             %vi = OpLoad %int %i
             %ci = OpSLessThan %bool %vi %cnt
                   OpLoopMerge %loop_end %inci None
                   OpBranchConditional %ci %loop_body %loop_end
      %loop_body = OpLabel
     %src_px_lnk = OpAccessChain %int_ptr %src %vi
     %dst_px_lnk = OpAccessChain %int_ptr %dst %vi
         %src_px = OpLoad %int %src_px_lnk Aligned 4
                   OpStore %dst_px_lnk %src_px Aligned 4
                   OpBranch %inci
           %inci = OpLabel
             %ni = OpIAdd %int %vi %one
                   OpStore %i %ni
                   OpBranch %loop
       %loop_end = OpLabel
                   OpBranch %use_fun_end
    %use_fun_end = OpLabel

                   OpReturn
                   OpFunctionEnd
    )");

    programCollection.spirvAsmSources.add("comp")
        << program << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

tcu::TestStatus SpvAsmPhysicalStorageBufferPushConstantsTestInstance::iterate(void)
{
    const DeviceInterface &vki      = m_context.getDeviceInterface();
    const VkDevice dev              = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    Move<VkCommandPool> cmdPool = createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer   = allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkShaderModule> shaderModule = createShaderModule(vki, dev, m_context.getBinaryCollection().get("comp"), 0);

    struct PushConstant
    {
        uint64_t src;
        uint64_t dst;
        int32_t cnt;
        bool use_fun;
    };

    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0,                           // uint32_t offset;
        sizeof(PushConstant)         // uint32_t size;
    };

    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vki, dev, 0, nullptr, 1, &pushConstantRange);
    Move<VkPipeline> pipeline             = makeComputePipeline(vki, dev, *pipelineLayout, *shaderModule);

    ut::TypedBuffer<int32_t> src(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);
    ut::TypedBuffer<int32_t> dst(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);

    src.iota(m_params->elements, true);
    dst.zero(true);

    const PushConstant pc = {src.getDeviceAddress(), dst.getDeviceAddress(), int32_t(m_params->elements),
                             m_params->method == PassMethod::PUSH_CONSTANTS_FUNCTION};

    beginCommandBuffer(vki, *cmdBuffer);
    vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vki.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vki.cmdDispatch(*cmdBuffer, 1, 1, 1);
    endCommandBuffer(vki, *cmdBuffer);

    submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

    dst.invalidate();

    return std::equal(src.begin(), src.end(), dst.begin()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

void SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::initPrograms(vk::SourceCollections &programCollection,
                                                                      const TestParamsPtr params)
{
    DE_UNREF(params);

    const std::string comp(R"(
    OpCapability Shader
    OpCapability Int64
    OpCapability PhysicalStorageBufferAddresses

    OpExtension "SPV_KHR_physical_storage_buffer"
    OpMemoryModel PhysicalStorageBuffer64 GLSL450

    OpEntryPoint GLCompute %comp "main" %id %ssbo

    OpExecutionMode %comp LocalSize 1 1 1
    OpDecorate %id BuiltIn GlobalInvocationId

    OpDecorate %sssbo Block
    OpMemberDecorate %sssbo 0 Offset 0
    OpMemberDecorate %sssbo 1 Offset 8
    OpMemberDecorate %sssbo 2 Offset 16
    OpMemberDecorate %sssbo 3 Offset 24

    OpDecorate %ssbo DescriptorSet 0
    OpDecorate %ssbo Binding 0

    OpDecorate %rta ArrayStride 4

    %bool = OpTypeBool
    %int = OpTypeInt 32 1
    %uint = OpTypeInt 32 0
    %ulong = OpTypeInt 64 0

    %zero = OpConstant %int 0
    %one = OpConstant %int 1
    %two = OpConstant %int 2
    %three = OpConstant %int 3

    %uvec3 = OpTypeVector %uint 3
    %rta = OpTypeRuntimeArray %int

    %rta_psb = OpTypePointer PhysicalStorageBuffer %rta
    %sssbo = OpTypeStruct %rta_psb %ulong %rta_psb %ulong
    %sssbo_buf = OpTypePointer StorageBuffer %sssbo
    %ssbo = OpVariable %sssbo_buf StorageBuffer
    %rta_psb_sb = OpTypePointer StorageBuffer %rta_psb
    %int_psb = OpTypePointer PhysicalStorageBuffer %int
    %ulong_sb = OpTypePointer StorageBuffer %ulong

    %uvec3_in = OpTypePointer Input %uvec3
    %id = OpVariable %uvec3_in Input
    %uint_in = OpTypePointer Input %uint

    %void = OpTypeVoid
    %voidf = OpTypeFunction %void

    %comp = OpFunction %void None %voidf
    %comp_begin = OpLabel

        %pgid_x = OpAccessChain %uint_in %id %zero
        %gid_x = OpLoad %uint %pgid_x
        %mod2 = OpSMod %int %gid_x %two
        %even = OpIEqual %bool %mod2 %zero

        %psrc_buff_p = OpAccessChain %rta_psb_sb %ssbo %zero
        %pdst_buff_p = OpAccessChain %rta_psb_sb %ssbo %two
        %src_buff_p = OpLoad %rta_psb %psrc_buff_p
        %dst_buff_p = OpLoad %rta_psb %pdst_buff_p

        %psrc_buff_u = OpAccessChain %ulong_sb %ssbo %one
        %psrc_buff_v = OpLoad %ulong %psrc_buff_u
        %src_buff_v = OpConvertUToPtr %rta_psb %psrc_buff_v
        %pdst_buff_u = OpAccessChain %ulong_sb %ssbo %three
        %pdst_buff_v = OpLoad %ulong %pdst_buff_u
        %dst_buff_v = OpConvertUToPtr %rta_psb %pdst_buff_v

        %src = OpSelect %rta_psb %even %src_buff_p %src_buff_v
        %dst = OpSelect %rta_psb %even %dst_buff_v %dst_buff_p

        %psrc_color = OpAccessChain %int_psb %src %gid_x
        %src_color = OpLoad %int %psrc_color Aligned 4
        %pdst_color = OpAccessChain %int_psb %dst %gid_x
        OpStore %pdst_color %src_color Aligned 4

    OpReturn
    OpFunctionEnd
    )");

    programCollection.spirvAsmSources.add("comp")
        << comp << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

/*
 Below test does not add anything new. The main purpose of this test is to show that both PhysicalStorageBuffer
 and 64-bit integer value can coexist in one array one next to the other. In the both cases, when the one address
 has its own dedicated storage class and the other is regular integer, the shader is responsible for how to interpret
 and use input addresses. Regardless of the shader, the application always passes them as 64-bit integers.
*/
tcu::TestStatus SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::iterate(void)
{
    const DeviceInterface &vki      = m_context.getDeviceInterface();
    const VkDevice dev              = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    Move<VkCommandPool> cmdPool = createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer   = allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkShaderModule> shaderModule = createShaderModule(vki, dev, m_context.getBinaryCollection().get("comp"), 0);

    Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vki, dev);
    Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
                                                .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                                .build(vki, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vki, dev, *descriptorPool, *descriptorSetLayout);
    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vki, dev, 1u, &descriptorSetLayout.get());
    Move<VkPipeline> pipeline             = makeComputePipeline(vki, dev, *pipelineLayout, *shaderModule);

    ut::TypedBuffer<int32_t> src(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);
    ut::TypedBuffer<int32_t> dst(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);

    struct SSBO
    {
        uint64_t srcAsBuff;
        uint64_t srcAsUint;
        uint64_t dstAsBuff;
        uint64_t dstAsUint;
    };
    ut::TypedBuffer<SSBO> ssbo(
        m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        {{src.getDeviceAddress(), src.getDeviceAddress(), dst.getDeviceAddress(), dst.getDeviceAddress()}});
    VkDescriptorBufferInfo ssboBufferInfo = makeDescriptorBufferInfo(ssbo.getBuffer(), 0, ssbo.getSize());
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboBufferInfo)
        .update(vki, dev);

    src.iota(m_params->elements, true);
    dst.zero(true);

    beginCommandBuffer(vki, *cmdBuffer);
    vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vki.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(),
                              0u, nullptr);
    vki.cmdDispatch(*cmdBuffer, m_params->elements, 1, 1);
    endCommandBuffer(vki, *cmdBuffer);

    submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

    dst.invalidate();

    return std::equal(src.begin(), src.end(), dst.begin()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

} // namespace

tcu::TestCaseGroup *createPhysicalStorageBufferTestGroup(tcu::TestContext &testCtx)
{
    struct
    {
        PassMethod method;
        std::string testName;
    } const methods[] = {
        {PassMethod::PUSH_CONSTANTS, "push_constants"},
        {PassMethod::PUSH_CONSTANTS_FUNCTION, "push_constants_function"},
        {PassMethod::ADDRESSES_IN_SSBO, "addrs_in_ssbo"},
    };

    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(testCtx, "physical_storage_buffer");

    for (const auto &method : methods)
    {
        group->addChild(new SpvAsmPhysicalStorageBufferTestCase(testCtx, method.testName,
                                                                TestParamsPtr(new TestParams({method.method, 64}))));
    }

    return group;
}

} // namespace SpirVAssembly
} // namespace vkt
