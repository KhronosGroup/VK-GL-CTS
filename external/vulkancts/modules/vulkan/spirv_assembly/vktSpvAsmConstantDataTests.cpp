/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Tests for SPV_KHR_constant_data
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmConstantDataTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include <memory>
#include <sstream>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;

constexpr uint32_t kDataAlignment = 4u;      // 32-bits in bytes. Data is padded to this.
constexpr uint32_t kMaxDataBytes  = 262128u; // 65532 words.

uint32_t getPaddingByteCount(uint32_t byteCount)
{
    const auto mod = byteCount % kDataAlignment;
    if (mod == 0u)
        return 0u;
    return kDataAlignment - mod;
}

struct TestParams
{
    VkShaderStageFlagBits shaderStage; // Compute or fragment.
    uint32_t itemSizeBytes;            // 1, 2, 4 or 8 bytes.
    uint32_t itemCount;
    tcu::Maybe<uint32_t> specializedItemCount;
    bool specializeData;
    bool useEncodeDecoration;
    bool forceSpecializedCountDeclaration; // Even if the item count is not specialized, force declaration as such.
    bool forceSpecializedDataDeclaration;  // Even if specializeData is false, declare the spec constant version.

    uint32_t getOutItemCount() const
    {
        return (!!specializedItemCount ? specializedItemCount.get() : itemCount);
    }

    bool specializeCountDeclaration() const
    {
        return (!!specializedItemCount || forceSpecializedCountDeclaration);
    }

    bool specializeDataDeclaration() const
    {
        return (specializeData || forceSpecializedDataDeclaration);
    }

    bool isCompute() const
    {
        return (shaderStage == VK_SHADER_STAGE_COMPUTE_BIT);
    }
};
using TestParamsPtr = std::shared_ptr<const TestParams>;

void checkSupport(Context &context, TestParamsPtr params)
{
    context.requireDeviceFunctionality("VK_KHR_shader_constant_data");

    if (context.getUsedApiVersion() < VK_API_VERSION_1_2)
        TCU_THROW(NotSupportedError, "Vulkan 1.2 required");

    const auto &vk12Features = context.getDeviceVulkan12Features();
    const auto &vk11Features = context.getDeviceVulkan11Features();

    switch (params->itemSizeBytes)
    {
    case 1:
    {
        const auto &i8Features = context.getShaderFloat16Int8Features();

        if (!i8Features.shaderInt8)
            TCU_THROW(NotSupportedError, "shaderInt8 not supported");

        if (!vk12Features.storageBuffer8BitAccess)
            TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");
    }
    break;
    case 2:
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

        if (!vk11Features.storageBuffer16BitAccess)
            TCU_THROW(NotSupportedError, "storageBuffer16BitAccess not supported");
    }
    break;
    case 4:
        break;
    case 8:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    if (params->shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
    else if (params->shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
        ;
    else
        DE_ASSERT(false);
}

// Returns the byte pattern that repeats in the constant data.
std::string getBaseValues(bool alt)
{
    std::string values = "abcdefghijklmnopqrstuvwxyz";
    return (alt ? de::toUpper(values) : values);
}

// Modifies the given string to pad data to the aligned length with '\0' bytes.
void padString(std::string &value)
{
    const auto paddingByteCount = getPaddingByteCount(static_cast<uint32_t>(value.size()));
    if (paddingByteCount != 0u)
        value.insert(value.end(), paddingByteCount, '\0');
}

// Returns the base values, repeated if needed, to fill the given length in bytes.
std::string getConstantDataValues(uint32_t len, bool alt)
{
    std::string val;
    val.reserve(len);

    const auto baseValues = getBaseValues(alt);

    uint32_t remaining = len;
    while (remaining > 0u)
    {
        const auto chunkSize = std::min(de::sizeU32(baseValues), remaining);
        val += baseValues.substr(0, chunkSize);
        remaining -= chunkSize;
    }

    return val;
}

// Makes a vector of constant data with the given string of values. Pads if needed.
std::vector<uint32_t> asU32Vec(const std::string &values)
{
    std::string paddedString = values;
    padString(paddedString);

    DE_ASSERT(paddedString.size() % sizeof(uint32_t) == 0);
    const auto u32ItemCount = de::sizeU32(paddedString) / DE_SIZEOF32(uint32_t);
    std::vector<uint32_t> u32Values(u32ItemCount, 0u);
    memcpy(de::dataOrNull(u32Values), paddedString.c_str(), paddedString.size());
    return u32Values;
}

// Gets the SPIR-V assembly declaration of the provided constant data.
std::string asString(const std::vector<uint32_t> &values)
{
    std::ostringstream decl;

    for (size_t i = 0; i < values.size(); ++i)
        decl << (i == 0 ? "" : " ") << "0x" << std::hex << std::setw(8) << std::setfill('0') << values.at(i);

    return decl.str();
}

void initPrograms(vk::SourceCollections &dst, TestParamsPtr params)
{
    const bool specializeItemCount = params->specializeCountDeclaration();
    const bool specializeDataDecl  = params->specializeDataDeclaration();
    const auto bitWidth            = params->itemSizeBytes * 8u;

    // Declaration of constant data items.
    const auto dataValues   = getConstantDataValues(params->itemCount * params->itemSizeBytes, params->specializeData);
    const auto itemsDecl    = asString(asU32Vec(dataValues));
    const auto dataItemType = "%int" + std::to_string(bitWidth) + "_t";

    // Shader parameters
    const auto isComp     = params->isCompute();
    const auto epType     = (isComp ? "GLCompute" : "Fragment");
    const auto exeMode    = (isComp ? "LocalSize 1 1 1" : "OriginUpperLeft");
    const auto shaderName = (isComp ? "comp" : "frag");

    std::ostringstream shader;
    shader << "               OpCapability Shader\n"
           << "               OpCapability ConstantDataKHR\n"
           << (params->itemSizeBytes == 1 ? "OpCapability Int8\nOpCapability StorageBuffer8BitAccess\n" : "")
           << (params->itemSizeBytes == 2 ? "OpCapability Int16\nOpCapability StorageBuffer16BitAccess\n" : "")
           << (params->itemSizeBytes == 8 ? "OpCapability Int64\n" : "")
           << "               OpExtension \"SPV_KHR_constant_data\"\n"
           << (params->itemSizeBytes == 1 ? "OpExtension \"SPV_KHR_8bit_storage\"\n" : "")
           << (params->itemSizeBytes == 2 ? "OpExtension \"SPV_KHR_16bit_storage\"\n" : "")
           << "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
           << "               OpMemoryModel Logical GLSL450\n"
           << "               OpEntryPoint " << epType << " %main \"main\" %ssbo\n"
           << "               OpExecutionMode %main " << exeMode << "\n"
           << "               OpDecorate %strided_array_type ArrayStride " << params->itemSizeBytes << "\n"
           << "               OpDecorate %SSBO_Block Block\n"
           << "               OpMemberDecorate %SSBO_Block 0 Offset 0\n"
           << "               OpDecorate %ssbo Binding 0\n"
           << "               OpDecorate %ssbo DescriptorSet 0\n"
           << (params->useEncodeDecoration ? "OpDecorate %data_array_type UTFEncodedKHR\n" : "")
           << (specializeItemCount ? "OpDecorate %item_count SpecId 0\n" : "") // ID 0 for the item count.
           << (specializeDataDecl ? "OpDecorate %data SpecId 1\n" : "")        // ID 1 for the data.
           << "       %void = OpTypeVoid\n"
           << "       %uint = OpTypeInt 32 0\n"
           << dataItemType << " = OpTypeInt " << bitWidth << " 1\n"
           << "  %void_func = OpTypeFunction %void\n"
           << " %item_count = " << (specializeItemCount ? "OpSpecConstant" : "OpConstant") << " %uint "
           << params->itemCount << "\n"
           << "     %uint_0 = OpConstant %uint 0\n"
           << "%data_array_type = OpTypeArray " << dataItemType << " %item_count\n"
           << "%strided_array_type = OpTypeArray " << dataItemType << " %item_count\n"
           << "       %data = " << (specializeDataDecl ? "OpSpecConstantDataKHR" : "OpConstantDataKHR")
           << " %data_array_type " << itemsDecl << "\n"
           << "%SSBO_Block = OpTypeStruct %strided_array_type\n"
           << "%ssbo_ptr_type = OpTypePointer StorageBuffer %SSBO_Block\n"
           << "%ssbo_array_ptr_type = OpTypePointer StorageBuffer %strided_array_type\n"
           << "       %ssbo = OpVariable %ssbo_ptr_type StorageBuffer\n"
           << "       %main = OpFunction %void None %void_func\n"
           << " %main_label = OpLabel\n"
           << "%strided_data = OpCopyLogical %strided_array_type %data\n"
           << "%ssbo_array_ptr = OpAccessChain %ssbo_array_ptr_type %ssbo %uint_0\n"
           << "               OpStore %ssbo_array_ptr %strided_data\n"
           << "               OpReturn\n"
           << "               OpFunctionEnd\n";

    const SpirVAsmBuildOptions spvOptions(dst.usedVulkanVersion, SPIRV_VERSION_1_5);
    dst.spirvAsmSources.add(shaderName) << shader.str() << spvOptions;

    if (!isComp)
    {
        // Draws a full-screen large triangle.
        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "        vec2(-1.0, -1.0),"
             << "        vec2(3.0, -1.0),"
             << "        vec2(-1.0, 3.0)"
             << ");\n"
             << "void main() {\n"
             << "        gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
             << "}";
        dst.glslSources.add("vert") << glu::VertexSource(vert.str());
    }
}

// Verifies that the output data matches the input data.
template <class ITEM_TYPE>
bool verifyData(tcu::TestLog &log, const std::string &dataBytes, const std::vector<uint8_t> &outBytes)
{
    const auto itemSizeBytes = sizeof(ITEM_TYPE);
    DE_ASSERT(dataBytes.size() == outBytes.size());
    DE_ASSERT(dataBytes.size() % itemSizeBytes == 0);

    const auto itemCount = dataBytes.size() / itemSizeBytes;

    std::vector<ITEM_TYPE> inItems(itemCount, 0);
    std::vector<ITEM_TYPE> outItems(itemCount, 0);

    memcpy(inItems.data(), dataBytes.data(), de::dataSize(inItems));
    memcpy(outItems.data(), outBytes.data(), de::dataSize(outItems));

    bool ok = true;
    for (size_t i = 0; i < itemCount; ++i)
    {
        const auto inItem  = static_cast<uint64_t>(inItems.at(i));
        const auto outItem = static_cast<uint64_t>(outItems.at(i));

        if (inItem != outItem)
        {
            ok = false;
            std::ostringstream msg;
            msg << "Item " << i << " out of " << itemCount << std::hex << ": expected 0x" << inItem << " but found 0x"
                << outItem;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    }

    return ok;
}

tcu::TestStatus runTest(Context &context, TestParamsPtr params)
{
    const auto ctx            = context.getContextCommonData();
    const auto descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags     = static_cast<VkShaderStageFlags>(params->shaderStage);
    const auto outItemCount   = params->getOutItemCount();
    const auto isComp         = params->isCompute();
    const auto isFrag         = !isComp;

    // Only for the fragment case.
    const auto extent = makeExtent3D(1u, 1u, 1u);
    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const auto outBufferSize = outItemCount * params->itemSizeBytes;
    const auto bufferUsage   = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto outBufferInfo = makeBufferCreateInfo(outBufferSize, bufferUsage);
    BufferWithMemory outBuffer(ctx.vkd, ctx.device, ctx.allocator, outBufferInfo, HostIntent::R);
    auto &outBufferAlloc = outBuffer.getAllocation();
    {
        const auto dataPtr = outBufferAlloc.getHostPtr();
        memset(dataPtr, 0, static_cast<size_t>(outBufferSize));
        flushAlloc(ctx.vkd, ctx.device, outBufferAlloc);
    }

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descriptorType, stageFlags);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descriptorType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto outBufferDescInfo = makeDescriptorBufferInfo(*outBuffer, 0ull, VK_WHOLE_SIZE);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                              &outBufferDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    const auto &binaries = context.getBinaryCollection();
    Move<VkShaderModule> compShader;
    Move<VkShaderModule> vertShader;
    Move<VkShaderModule> fragShader;

    if (isComp)
        compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    else
    {
        vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
        fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    }

    std::vector<VkSpecializationMapEntry> specMapEntries;
    std::vector<uint8_t> specData;

    if (!!params->specializedItemCount)
    {
        const auto count = params->specializedItemCount.get();

        specData.resize(sizeof(count));
        memcpy(specData.data(), &count, sizeof(count));

        const VkSpecializationMapEntry entry{
            0u,
            0u,
            sizeof(count),
        };
        specMapEntries.push_back(entry);
    }

    const auto dataBytes = getConstantDataValues(params->getOutItemCount() * params->itemSizeBytes, false);

    if (params->specializeData)
    {
        const auto prevSize = specData.size();

        specData.resize(prevSize + dataBytes.size());
        memcpy(&specData.at(prevSize), dataBytes.c_str(), dataBytes.size());

        const VkSpecializationMapEntry entry{
            1u,
            static_cast<uint32_t>(prevSize),
            static_cast<uint32_t>(dataBytes.size()),
        };
        specMapEntries.push_back(entry);
    }

    const VkSpecializationInfo specializationInfo = {
        de::sizeU32(specMapEntries),
        de::dataOrNull(specMapEntries),
        de::dataSize(specData),
        de::dataOrNull(specData),
    };
    const auto pSpecializationInfo = (specData.empty() ? nullptr : &specializationInfo);

    Move<VkPipeline> pipeline;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (isFrag)
    {
        renderPass = makeRenderPass(ctx.vkd, ctx.device);
        framebuffer =
            makeFramebuffer(ctx.vkd, ctx.device, *renderPass, 0u, nullptr, extent.width, extent.height, extent.depth);
    }

    if (isComp)
    {
        pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, 0u, nullptr, *compShader, 0u,
                                       pSpecializationInfo);
    }
    else
    {
        const std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos{
            VkPipelineShaderStageCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0u,
                VK_SHADER_STAGE_VERTEX_BIT,
                *vertShader,
                "main",
                nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0u,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                *fragShader,
                "main",
                pSpecializationInfo,
            },
        };

        const VkPipelineColorBlendStateCreateInfo colorBlendState   = initVulkanStructureConst();
        const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructureConst();

        pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, *pipelineLayout, 0u, shaderCreateInfos,
                                        *renderPass, viewports, scissors, 0u, nullptr, nullptr, nullptr,
                                        &colorBlendState, nullptr, nullptr, &vertexInputState);
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto bindPoint = (isComp ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS);
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (isFrag)
    {
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.front());
    }
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    if (isFrag)
    {
        ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    else
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    {
        const auto srcStage = (isComp ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        const auto dstStage = VK_PIPELINE_STAGE_HOST_BIT;
        const auto barrier  = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, outBufferAlloc);
    std::vector<uint8_t> outBytes(outBufferSize, 0);
    memcpy(de::dataOrNull(outBytes), outBufferAlloc.getHostPtr(), de::dataSize(outBytes));

    // Verify results using the appropriate data types.
    bool ok   = false;
    auto &log = context.getTestContext().getLog();

    switch (params->itemSizeBytes)
    {
    case 1:
        ok = verifyData<uint8_t>(log, dataBytes, outBytes);
        break;
    case 2:
        ok = verifyData<uint16_t>(log, dataBytes, outBytes);
        break;
    case 4:
        ok = verifyData<uint32_t>(log, dataBytes, outBytes);
        break;
    case 8:
        ok = verifyData<uint64_t>(log, dataBytes, outBytes);
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    if (!ok)
        TCU_FAIL("Unexpected values in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

uint32_t getDataWordCount(uint32_t itemSize, uint32_t itemCount)
{
    const auto arrayBytes    = itemCount * itemSize;
    const auto dataBytes     = arrayBytes + getPaddingByteCount(arrayBytes);
    const auto dataWordCount = dataBytes / kDataAlignment;
    return dataWordCount;
}

} // anonymous namespace

tcu::TestCaseGroup *createConstantDataTests(tcu::TestContext &testCtx)
{
    return createTestGroup(
        testCtx, "constant_data",
        [](tcu::TestCaseGroup *mainGroup)
        {
            // Basic tests, no specialization.
            addTestGroup(
                mainGroup, "basic",
                [](tcu::TestCaseGroup *basicGroup)
                {
                    const auto kMax = std::numeric_limits<uint32_t>::max();
                    for (const auto itemSizeBytes : {4u, 2u, 8u, 1u})
                        for (const auto maybeItemCount : {1u, 2u, 32u, 33u, 64u, 65u, 127u, 128u, 129u, 130u, kMax})
                        {
                            const auto itemCount =
                                (maybeItemCount == kMax ? kMaxDataBytes / itemSizeBytes : maybeItemCount);
                            for (const bool useEncodeDecoration : {false, true})
                                for (const bool forceSpecializedCountDeclaration : {false, true})
                                    for (const bool forceSpecializedDataDeclaration : {false, true})
                                        for (const auto stage :
                                             {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_FRAGMENT_BIT})
                                        {
                                            const auto isComp = (stage == VK_SHADER_STAGE_COMPUTE_BIT);
                                            TestParamsPtr params(new TestParams{
                                                stage,
                                                itemSizeBytes,
                                                itemCount,
                                                tcu::Nothing,
                                                false,
                                                useEncodeDecoration,
                                                forceSpecializedCountDeclaration,
                                                forceSpecializedDataDeclaration,
                                            });
                                            const auto testName =
                                                "width_" + std::to_string(itemSizeBytes) + "_count_" +
                                                std::to_string(itemCount) +
                                                (useEncodeDecoration ? "_encode_decoration" : "") +
                                                (forceSpecializedCountDeclaration ? "_force_spec_count" : "") +
                                                (forceSpecializedDataDeclaration ? "_force_spec_data" : "") +
                                                (isComp ? "_comp" : "_frag");
                                            addFunctionCaseWithPrograms(basicGroup, testName, checkSupport,
                                                                        initPrograms, runTest, params);
                                        }
                        }
                });

            // Specialization tests.
            addTestGroup(
                mainGroup, "specialization",
                [](tcu::TestCaseGroup *specializationGroup)
                {
                    addTestGroup(specializationGroup, "count_only",
                                 [](tcu::TestCaseGroup *countOnlyGroup)
                                 {
                                     for (const auto itemSizeBytes : {4u, 2u, 8u, 1u})
                                         for (const auto declItemCount : {1u, 2u, 31u, 32u, 33u, 63u, 64u, 65u, 66u})
                                         {
                                             // When specializing the count only, anything is valid as long as the number of words in the
                                             // declared count and the specialized count is exactly the same, so we're going to take the
                                             // declared count and sometimes remove items as far as possible.
                                             //
                                             // We could also test adding a few bytes in some situations, but that would require small
                                             // changes in the way we generate the expected data (because the new bytes would be zero and
                                             // not valid string bytes as they are generated now).
                                             const auto declWordCount = getDataWordCount(itemSizeBytes, declItemCount);
                                             auto itemCount           = declItemCount;
                                             for (;;)
                                             {
                                                 const auto dataWordCount = getDataWordCount(itemSizeBytes, itemCount);
                                                 if (dataWordCount == 0u || dataWordCount != declWordCount)
                                                     break;

                                                 for (const auto stage :
                                                      {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_FRAGMENT_BIT})
                                                 {
                                                     const auto isComp = (stage == VK_SHADER_STAGE_COMPUTE_BIT);

                                                     TestParamsPtr params(new TestParams{
                                                         stage,
                                                         itemSizeBytes,
                                                         declItemCount,
                                                         tcu::just(itemCount),
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                     });

                                                     const auto testName = "width_" + std::to_string(itemSizeBytes) +
                                                                           "_decl_" + std::to_string(declItemCount) +
                                                                           "_actual_" + std::to_string(itemCount) +
                                                                           (isComp ? "_comp" : "_frag");
                                                     addFunctionCaseWithPrograms(countOnlyGroup, testName, checkSupport,
                                                                                 initPrograms, runTest, params);
                                                 }

                                                 --itemCount;
                                             }
                                         }
                                 });

                    // Modifying the data only is a simple way to test the basic constant data replacement mechanism works.
                    addTestGroup(
                        specializationGroup, "data_only",
                        [](tcu::TestCaseGroup *dataOnlyGroup)
                        {
                            const auto kMax = std::numeric_limits<uint32_t>::max();
                            for (const auto itemSizeBytes : {4u, 2u, 8u, 1u})
                                for (const auto maybeItemCount :
                                     {1u, 2u, 32u, 33u, 64u, 65u, 127u, 128u, 129u, 130u, kMax})
                                {
                                    const auto itemCount =
                                        (maybeItemCount == kMax ? kMaxDataBytes / itemSizeBytes : maybeItemCount);

                                    for (const auto stage : {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_FRAGMENT_BIT})
                                    {
                                        const auto isComp = (stage == VK_SHADER_STAGE_COMPUTE_BIT);

                                        TestParamsPtr params(new TestParams{
                                            stage,
                                            itemSizeBytes,
                                            itemCount,
                                            tcu::Nothing,
                                            true, // Specialize data.
                                            false,
                                            false,
                                            false,
                                        });

                                        const auto testName = "width_" + std::to_string(itemSizeBytes) + "_count_" +
                                                              std::to_string(itemCount) + (isComp ? "_comp" : "_frag");
                                        addFunctionCaseWithPrograms(dataOnlyGroup, testName, checkSupport, initPrograms,
                                                                    runTest, params);
                                    }
                                }
                        });

                    // The most complex cases are the ones involving a full replacement of the constant data.
                    addTestGroup(specializationGroup, "count_and_data",
                                 [](tcu::TestCaseGroup *bothGroup)
                                 {
                                     const auto kMax = std::numeric_limits<uint32_t>::max();
                                     for (const auto itemSizeBytes : {4u, 2u, 8u, 1u})
                                         for (const auto maybeDeclItemCount :
                                              {1u, 2u, 32u, 33u, 64u, 65u, 127u, 128u, 129u, 130u, kMax})
                                             for (const auto maybeActualItemCount :
                                                  {1u, 2u, 32u, 33u, 64u, 65u, 127u, 128u, 129u, 130u, kMax})
                                             {
                                                 const auto declItemCount =
                                                     (maybeDeclItemCount == kMax ? kMaxDataBytes / itemSizeBytes :
                                                                                   maybeDeclItemCount);
                                                 const auto actualItemCount =
                                                     (maybeActualItemCount == kMax ? kMaxDataBytes / itemSizeBytes :
                                                                                     maybeActualItemCount);

                                                 for (const auto stage :
                                                      {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_FRAGMENT_BIT})
                                                 {
                                                     const auto isComp = (stage == VK_SHADER_STAGE_COMPUTE_BIT);

                                                     TestParamsPtr params(new TestParams{
                                                         stage,
                                                         itemSizeBytes,
                                                         declItemCount,
                                                         tcu::just(actualItemCount),
                                                         true, // Specialize data too.
                                                         false,
                                                         false,
                                                         false,
                                                     });

                                                     const auto testName =
                                                         "width_" + std::to_string(itemSizeBytes) + "_decl_" +
                                                         std::to_string(declItemCount) + "_actual_" +
                                                         std::to_string(actualItemCount) + (isComp ? "_comp" : "_frag");
                                                     addFunctionCaseWithPrograms(bothGroup, testName, checkSupport,
                                                                                 initPrograms, runTest, params);
                                                 }
                                             }
                                 });
                });
        });
}

} // namespace SpirVAssembly
} // namespace vkt
