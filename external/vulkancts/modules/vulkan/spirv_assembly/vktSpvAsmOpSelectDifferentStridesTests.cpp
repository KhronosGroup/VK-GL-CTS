/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for OpSelect function
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmOpSelectDifferentStridesTests.hpp"
#include "vkPrograms.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace vkt::SpirVAssembly
{
using namespace vk;
namespace
{
struct Params
{
};

class OpSelectDifferentStridesInstance : public TestInstance
{
    const Params m_params;
    struct PushConstant
    {
        uint32_t x, y, z, w;
    };
    bool verifyResult(BufferWithMemory const &fooBuffer, BufferWithMemory const &barBuffer, PushConstant const &pcFoo,
                      PushConstant const &pcBar, uint32_t elementCount, std::string &errorMessage) const;

public:
    OpSelectDifferentStridesInstance(Context &context, Params const &params) : TestInstance(context), m_params(params)
    {
    }
    virtual tcu::TestStatus iterate() override;
};

class OpSelectDifferentStridesCase : public TestCase
{
    const Params m_params;

    static inline const char *spvasm = R"spirv(
               OpCapability Shader
               OpCapability Int64
               OpCapability VariablePointers
               OpCapability VariablePointersStorageBuffer
               OpCapability PhysicalStorageBufferAddresses
               OpExtension "SPV_KHR_variable_pointers"
               OpExtension "SPV_KHR_physical_storage_buffer"
               OpExtension "SPV_EXT_physical_storage_buffer"
               ;OpExtension "SPV_EXT_scalar_block_layout"
          %1 = OpExtInstImport "GLSL.std.450"
               ; OpMemoryModel Logical GLSL450
               OpMemoryModel PhysicalStorageBuffer64 GLSL450
               OpEntryPoint GLCompute %main "main" %_ %__0 %__1
               OpExecutionMode %main LocalSize 1 1 1

               ; Annotations
               OpDecorate %PC Block
               OpMemberDecorate %PC 0 Offset 0
               OpMemberDecorate %PC 1 Offset 4
               OpMemberDecorate %PC 2 Offset 8
               OpMemberDecorate %PC 3 Offset 12
               OpDecorate %_runtimearr_v3uint ArrayStride 12
               OpDecorate %_runtimearr_v4uint ArrayStride 16
               OpDecorate %FooBuffer Block
               OpDecorate %BarBuffer Block
               OpDecorate %FooStruct Block
               OpDecorate %BarStruct Block
               OpMemberDecorate %FooBuffer 0 Offset 0  ; uint64_t
               OpMemberDecorate %FooBuffer 1 Offset 16 ; uivec4
               OpMemberDecorate %BarBuffer 0 Offset 0  ; uint64_t
               OpMemberDecorate %BarBuffer 1 Offset 16 ; uivec3
               OpMemberDecorate %FooStruct 0 Offset 0  ; uint64_t
               OpMemberDecorate %FooStruct 1 Offset 16 ; uivec4
               OpMemberDecorate %BarStruct 0 Offset 0  ; uint64_t
               OpMemberDecorate %BarStruct 1 Offset 16 ; uivec3
               OpDecorate %__0 Binding 0
               OpDecorate %__0 DescriptorSet 0
               OpDecorate %__1 Binding 1
               OpDecorate %__1 DescriptorSet 0
               OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize

               ; Types, variables and constants
       %bool = OpTypeBool
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
      %ulong = OpTypeInt 64 0
     %v3uint = OpTypeVector %uint 3
     %v4uint = OpTypeVector %uint 4
         %PC = OpTypeStruct %uint %uint %uint %uint     ; Block
%_ptr_PushConstant_PC = OpTypePointer PushConstant %PC
          %_ = OpVariable %_ptr_PushConstant_PC PushConstant
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %int_2 = OpConstant %int 2
      %int_3 = OpConstant %int 3
%_ptr_PushConstant_uint = OpTypePointer PushConstant %uint
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
%_runtimearr_v3uint = OpTypeRuntimeArray %v3uint   ; ArrayStride 12
%_runtimearr_v4uint = OpTypeRuntimeArray %v4uint   ; ArrayStride 16
%_ptr_address = OpTypePointer StorageBuffer %ulong

%_ptr_runtimearr_v3uint = OpTypePointer StorageBuffer %_runtimearr_v3uint
%_ptr_runtimearr_v4uint = OpTypePointer StorageBuffer %_runtimearr_v4uint
  %FooBuffer = OpTypeStruct %ulong %_runtimearr_v4uint    ; Block
  %BarBuffer = OpTypeStruct %ulong %_runtimearr_v3uint    ; Block
%_ptr_StorageBuffer_FooBuffer = OpTypePointer StorageBuffer %FooBuffer
%_ptr_StorageBuffer_BarBuffer = OpTypePointer StorageBuffer %BarBuffer
        %__0 = OpVariable %_ptr_StorageBuffer_FooBuffer StorageBuffer   ; Binding 0, DescriptorSet 0
        %__1 = OpVariable %_ptr_StorageBuffer_BarBuffer StorageBuffer   ; Binding 1, DescriptorSet 0
%_ptr_StorageBuffer_v3uint = OpTypePointer StorageBuffer %v3uint
%_ptr_StorageBuffer_v4uint = OpTypePointer StorageBuffer %v4uint

%_ptr_v3uint = OpTypePointer PhysicalStorageBuffer %v3uint
%_ptr_v4uint = OpTypePointer PhysicalStorageBuffer %v4uint
%_ptr_unsizedarr_v3uint = OpTypePointer PhysicalStorageBuffer %_runtimearr_v3uint
%_ptr_unsizedarr_v4uint = OpTypePointer PhysicalStorageBuffer %_runtimearr_v4uint
  %FooStruct = OpTypeStruct %ulong %_runtimearr_v4uint
  %BarStruct = OpTypeStruct %ulong %_runtimearr_v3uint
%_ptr_PhysicalBuffer_FooBuffer = OpTypePointer PhysicalStorageBuffer %FooStruct
%_ptr_PhysicalBuffer_BarBuffer = OpTypePointer PhysicalStorageBuffer %BarStruct

%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1     ; BuiltIn WorkgroupSize

               ; Function main
       %main = OpFunction %void None %3
          %5 = OpLabel
         %13 = OpAccessChain %_ptr_PushConstant_uint %_ %int_3
         %14 = OpLoad %uint %13
         %15 = OpAccessChain %_ptr_PushConstant_uint %_ %int_2
         %16 = OpLoad %uint %15
         %17 = OpAccessChain %_ptr_PushConstant_uint %_ %int_1
         %18 = OpLoad %uint %17
         %19 = OpAccessChain %_ptr_PushConstant_uint %_ %int_0
         %20 = OpLoad %uint %19
         %21 = OpIAdd %uint %20 %18
         %22 = OpIAdd %uint %21 %16
         ; %23 holds value to store which is the sum of x,y,z,w from push constant struct
         %23 = OpIAdd %uint %22 %14
         ; make condition
         %200 = OpIEqual %bool %20 %uint_0

         ; If above condition is satisfied then
         ; an even Result <id> indicates struct with vec4,
         ; and an odd indicates struct with vec3.

         %300 = OpAccessChain %_ptr_address %__0 %int_0
         %301 = OpAccessChain %_ptr_address %__1 %int_0
         %302 = OpSelect %_ptr_address %200 %300 %301
         %303 = OpSelect %_ptr_address %200 %301 %300
         %304 = OpLoad %ulong %302
         %305 = OpLoad %ulong %303
         %306 = OpConvertUToPtr %_ptr_PhysicalBuffer_FooBuffer %304
         %307 = OpConvertUToPtr %_ptr_PhysicalBuffer_BarBuffer %305
         %308 = OpAccessChain %_ptr_unsizedarr_v4uint %306 %int_1
         %309 = OpAccessChain %_ptr_unsizedarr_v3uint %307 %int_1

         ; make access to vec4
         %310 = OpAccessChain %_ptr_v4uint %308 %18
         ; make access to vec3
         %311 = OpAccessChain %_ptr_v3uint %309 %18
         ; compose vec4 to store
         %312 = OpCompositeConstruct %v4uint %23 %18 %16 %14
         ; compose vec3 to store
         %313 = OpCompositeConstruct %v3uint %23 %18 %16
         ; final storing
         OpStore %310 %312 Aligned 16
         OpStore %311 %313 Aligned 16
               OpReturn
               OpFunctionEnd
    )spirv";

public:
    OpSelectDifferentStridesCase(tcu::TestContext &testCtx, std::string const &name, Params const &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    static Move<VkShaderModule> createShader(const DeviceInterface &deviceInterface, VkDevice device,
                                             const ProgramBinary &binary, VkShaderModuleCreateFlags flags = 0);
    virtual std::string getRequiredCapabilitiesId() const override
    {
        return std::type_index(typeid(this)).name();
    }
    virtual void initDeviceCapabilities(DevCaps &caps) override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new OpSelectDifferentStridesInstance(context, m_params);
    }
};

void OpSelectDifferentStridesCase::initDeviceCapabilities(DevCaps &caps)
{
    auto trowNotSupported = [](const char *msg)
    { TCU_THROW(NotSupportedError, (msg + std::string(" not supported by device"))); };

    if (caps.getContextManager().getUsedApiVersion() < VK_API_VERSION_1_2)
    {
        if (!caps.addFeature(&VkPhysicalDeviceBufferDeviceAddressFeaturesEXT::bufferDeviceAddress))
            trowNotSupported("bufferDeviceAddress");
        if (!caps.addFeature(&VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout))
            trowNotSupported("scalarBlockLayout");
        if (!(caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false) ||
              caps.addExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false)))
            trowNotSupported(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }
    else
    {
        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::bufferDeviceAddress))
            trowNotSupported("bufferDeviceAddress");
        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::scalarBlockLayout))
            trowNotSupported("scalarBlockLayout");
        if (!(caps.addExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false) ||
              caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false)))
            trowNotSupported(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }

    if (!caps.addFeature(&VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT::mutableDescriptorType))
        trowNotSupported("mutableDescriptorType");
    if (!caps.addExtension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME))
        trowNotSupported(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);

    if (!caps.addExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME))
        trowNotSupported(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

    if (!caps.addFeature(&VkPhysicalDeviceVariablePointerFeaturesKHR::variablePointers))
        trowNotSupported("variablePointers");
    if (!caps.addFeature(&VkPhysicalDeviceVariablePointerFeaturesKHR::variablePointersStorageBuffer))
        trowNotSupported("variablePointersStorageBuffer");
    if (!caps.addExtension(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME))
        trowNotSupported(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME);

    if (!caps.addFeature(&VkPhysicalDeviceFeatures::shaderInt64))
        trowNotSupported("shaderInt64");
}

void OpSelectDifferentStridesCase::initPrograms(SourceCollections &programCollection) const
{
    SpirVAsmBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4);
    SpirvValidatorOptions validatorOptions = buildOptions.getSpirvValidatorOptions();
    validatorOptions.blockLayout           = SpirvValidatorOptions::BlockLayoutRules::kScalarBlockLayout;
    programCollection.spirvAsmSources.add("compute") << spvasm << (buildOptions << validatorOptions);
}

Move<VkShaderModule> OpSelectDifferentStridesCase::createShader(const DeviceInterface &deviceInterface, VkDevice device,
                                                                const ProgramBinary &binary,
                                                                VkShaderModuleCreateFlags flags)
{
    return createShaderModule(deviceInterface, device, binary, flags);
}

VkDeviceAddress getBufferAddress(const DeviceInterface &di, VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo addrInfo = initVulkanStructure();
    addrInfo.buffer                    = buffer;
    return di.getBufferDeviceAddress(device, &addrInfo);
}

tcu::TestStatus OpSelectDifferentStridesInstance::iterate()
{
    const DeviceInterface &di       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const uint32_t pc0set[4]{0u, 3u, 7u, 5u};
    const uint32_t pc1set[4]{1u, 8u, 5u, 11u};
    const uint32_t elementCount = [](uint32_t value, uint32_t multiple) { //
        return ((value + multiple - 1) / multiple) * multiple;
    }(std::max(pc0set[1], pc1set[1]) + 10u, 16u);

    const VkBufferCreateInfo bci =
        makeBufferCreateInfo((sizeof(VkDeviceAddress) + 8u + elementCount * sizeof(tcu::UVec4)),
                             (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
    BufferWithMemory fooBuffer(
        di, device, allocator, bci,
        (MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    BufferWithMemory barBuffer(
        di, device, allocator, bci,
        (MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    const VkDeviceAddress fooAddress = getBufferAddress(di, device, *fooBuffer);
    const VkDeviceAddress barAddress = getBufferAddress(di, device, *barBuffer);

    Move<VkDescriptorPool> dsFooPool = DescriptorPoolBuilder()
                                           .addType(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 2u)
                                           .build(di, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorPool> dsBarPool = DescriptorPoolBuilder()
                                           .addType(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 2u)
                                           .build(di, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder dsFooLayoutBuilder, dsBarLayoutBuilder;
    dsFooLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_SHADER_STAGE_COMPUTE_BIT);
    dsFooLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_SHADER_STAGE_COMPUTE_BIT);
    dsBarLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_SHADER_STAGE_COMPUTE_BIT);
    dsBarLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_SHADER_STAGE_COMPUTE_BIT);

    const VkDescriptorType mutableTypes[]{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    const VkMutableDescriptorTypeListEXT mutableLists[]{{1u, mutableTypes}, {1u, mutableTypes}};
    const VkMutableDescriptorTypeCreateInfoEXT mutableInfo{
        VkMutableDescriptorTypeCreateInfoEXT(initVulkanStructure()).sType, nullptr, 2u, mutableLists};

    Move<VkDescriptorSetLayout> dsFooLayout = dsFooLayoutBuilder.build(di, device, 0u, &mutableInfo);
    Move<VkDescriptorSetLayout> dsBarLayout = dsBarLayoutBuilder.build(di, device, 0u, &mutableInfo);

    Move<VkDescriptorSet> descSetFoo = makeDescriptorSet(di, device, *dsFooPool, *dsFooLayout);
    Move<VkDescriptorSet> descSetBar = makeDescriptorSet(di, device, *dsBarPool, *dsBarLayout);
    const VkDescriptorSet descriptorSets[2]{*descSetFoo, *descSetBar};

    DescriptorSetUpdateBuilder dsUpdateBuilder;
    const VkDescriptorBufferInfo fooBufferInfo = makeDescriptorBufferInfo(*fooBuffer, 0u, fooBuffer.getBufferSize());
    const VkDescriptorBufferInfo barBufferInfo = makeDescriptorBufferInfo(*barBuffer, 0u, barBuffer.getBufferSize());

    dsUpdateBuilder.writeSingle(*descSetFoo, DescriptorSetUpdateBuilder::Location::binding(0u),
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &fooBufferInfo);
    dsUpdateBuilder.writeSingle(*descSetFoo, DescriptorSetUpdateBuilder::Location::binding(1u),
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &barBufferInfo);

    dsUpdateBuilder.writeSingle(*descSetBar, DescriptorSetUpdateBuilder::Location::binding(0u),
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &barBufferInfo);
    dsUpdateBuilder.writeSingle(*descSetBar, DescriptorSetUpdateBuilder::Location::binding(1u),
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &fooBufferInfo);
    dsUpdateBuilder.update(di, device);

    const VkPushConstantRange pcRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0u, uint32_t(sizeof(PushConstant))};
    Move<VkPipelineLayout> plFoo      = makePipelineLayout(di, device, *dsFooLayout, &pcRange);
    Move<VkPipelineLayout> plBar      = makePipelineLayout(di, device, *dsBarLayout, &pcRange);

    Move<VkShaderModule> compShaderModule =
        OpSelectDifferentStridesCase::createShader(di, device, m_context.getBinaryCollection().get("compute"));

    Move<VkPipeline> pipelineFoo = makeComputePipeline(di, device, *plFoo, *compShaderModule);
    Move<VkPipeline> pipelineBar = makeComputePipeline(di, device, *plBar, *compShaderModule);

    Move<VkCommandPool> cmdPool =
        createCommandPool(di, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmd = allocateCommandBuffer(di, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkBufferMemoryBarrier barrierFoo = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, *fooBuffer, 0u, fooBuffer.getBufferSize());
    const VkBufferMemoryBarrier barrierBar = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, *barBuffer, 0u, barBuffer.getBufferSize());
    const VkBufferMemoryBarrier barriers[]{barrierFoo, barrierBar};

    const PushConstant pcFoo{pc0set[0], pc0set[1], pc0set[2], pc0set[3]};
    const PushConstant pcBar{pc1set[0], pc1set[1], pc1set[2], pc1set[3]};

    deMemset(fooBuffer.getAllocation().getHostPtr(), 0, size_t(fooBuffer.getBufferSize()));
    deMemset(barBuffer.getAllocation().getHostPtr(), 0, size_t(barBuffer.getBufferSize()));
    *reinterpret_cast<VkDeviceAddress *>(fooBuffer.getAllocation().getHostPtr()) = fooAddress;
    *reinterpret_cast<VkDeviceAddress *>(barBuffer.getAllocation().getHostPtr()) = barAddress;
    flushAlloc(di, device, fooBuffer.getAllocation());
    flushAlloc(di, device, barBuffer.getAllocation());

    beginCommandBuffer(di, *cmd);

    di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineFoo);
    di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *plFoo, 0u, 1u, &descriptorSets[0], 0u, nullptr);
    di.cmdPushConstants(*cmd, *plFoo, VK_SHADER_STAGE_COMPUTE_BIT, 0u, pcRange.size, &pcFoo);
    di.cmdDispatch(*cmd, 1u, 1u, 1u);

    di.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 2u, barriers, 0u, nullptr);

    di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineBar);
    di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *plBar, 0u, 1u, &descriptorSets[1], 0u, nullptr);
    di.cmdPushConstants(*cmd, *plBar, VK_SHADER_STAGE_COMPUTE_BIT, 0u, pcRange.size, &pcBar);
    di.cmdDispatch(*cmd, 1u, 1u, 1u);

    endCommandBuffer(di, *cmd);
    submitCommandsAndWait(di, device, queue, *cmd);

    invalidateAlloc(di, device, fooBuffer.getAllocation());
    invalidateAlloc(di, device, barBuffer.getAllocation());

    std::string errorMessages;
    if (const bool ok = verifyResult(fooBuffer, barBuffer, pcFoo, pcBar, elementCount, errorMessages); false == ok)
    {
        tcu::TestLog &log = m_context.getTestContext().getLog();
        log << tcu::TestLog::Message << errorMessages << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail({});
    }

    return tcu::TestStatus::pass({});
}

template <class T, class N, class P = const T (*)[1], class R = decltype(std::cbegin(*std::declval<const P>()))>
static auto makeStdBeginEnd(void const *p, N &&n) -> std::pair<R, R>
{
    auto tmp   = std::cbegin(*P(p));
    auto begin = tmp;
    std::advance(tmp, std::forward<N>(n));
    return {begin, tmp};
}

bool OpSelectDifferentStridesInstance::verifyResult(BufferWithMemory const &fooBuffer,
                                                    BufferWithMemory const &barBuffer, PushConstant const &pcFoo,
                                                    PushConstant const &pcBar, uint32_t elementCount,
                                                    std::string &errorMessage) const
{
    bool condFoo = false;
    bool condBar = false;
    std::ostringstream log;
    log << std::endl;
    const uint32_t sumFoo = pcFoo.x + pcFoo.y + pcFoo.z + pcFoo.w;
    const uint32_t sumBar = pcBar.x + pcBar.y + pcBar.z + pcBar.w;
    const uint32_t count  = std::min(std::max(pcFoo.y, pcBar.y) + 1u, elementCount);
    {
        std::vector<tcu::UVec4> foo;
        const std::byte *beginning =
            reinterpret_cast<std::byte const *>(fooBuffer.getAllocation().getHostPtr()) + sizeof(uint64_t) + 8u;
        auto range = makeStdBeginEnd<tcu::UVec4>(beginning, count);
        std::copy(range.first, range.second, std::back_inserter(foo));

        const tcu::UVec4 cmp0(sumFoo, pcFoo.y, pcFoo.z, pcFoo.w);
        const tcu::UVec4 cmp1(sumBar, pcBar.y, pcBar.z, pcBar.w);
        condFoo = foo[pcFoo.y] == cmp0 && foo[pcBar.y] == cmp1;

        if (false == condFoo)
        {
            log << "Result Foo buffer:" << std::endl;
            for (uint32_t i = 0; i < count; ++i)
                log << foo[i] << ' ';
            log << std::endl;

            std::vector<tcu::UVec4> tmp(count);
            tmp[pcFoo.y] = cmp0;
            tmp[pcBar.y] = cmp1;
            log << "Expected Foo buffer:" << std::endl;
            for (uint32_t i = 0; i < count; ++i)
                log << tmp[i] << ' ';
            log << std::endl;
        }
        else
        {
            log << "Foo buffer matches." << std::endl;
        }
    }
    {
        std::vector<tcu::UVec3> bar;
        const std::byte *beginning =
            reinterpret_cast<std::byte const *>(barBuffer.getAllocation().getHostPtr()) + sizeof(uint64_t) + 8u;
        auto range = makeStdBeginEnd<tcu::UVec3>(beginning, count);
        std::copy(range.first, range.second, std::back_inserter(bar));

        const tcu::UVec3 cmp0(sumFoo, pcFoo.y, pcFoo.z);
        const tcu::UVec3 cmp1(sumBar, pcBar.y, pcBar.z);
        condBar = bar[pcFoo.y] == cmp0 && bar[pcBar.y] == cmp1;

        if (false == condBar)
        {
            log << "Result Bar buffer:" << std::endl;
            for (uint32_t i = 0; i < count; ++i)
                log << bar[i] << ' ';
            log << std::endl;

            std::vector<tcu::UVec3> tmp(count);
            tmp[pcFoo.y] = cmp0;
            tmp[pcBar.y] = cmp1;
            log << "Expected Bar buffer" << std::endl;
            for (uint32_t i = 0; i < count; ++i)
                log << tmp[i] << ' ';
            log << std::endl;
        }
        else
        {
            log << "Bar buffer matches." << std::endl;
        }
    }

    log.flush();
    errorMessage = log.str();

    Params params(m_params);
    DE_UNREF(params);

    return (condFoo && condBar);
}

} // unnamed namespace

void addOpSelectDifferentStridesTest(tcu::TestCaseGroup *group)
{
    group->addChild(new OpSelectDifferentStridesCase(group->getTestContext(), "opselect_different_strides", {}));
}

} // namespace vkt::SpirVAssembly
