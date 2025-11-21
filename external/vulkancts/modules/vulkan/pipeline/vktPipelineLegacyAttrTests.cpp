/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Tests for VK_EXT_legacy_vertex_attributes
 *//*--------------------------------------------------------------------*/

#include "vktPipelineLegacyAttrTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"

#include <functional>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <set>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

constexpr uint32_t k32BitsInBytes = 4u;

enum class ShaderFormat
{
    FLOAT        = 0,
    SIGNED_INT   = 1,
    UNSIGNED_INT = 2,
    INVALID      = 3, // For assertions and default initializations.
};

struct BindingParams
{
    const VkFormat format;
    const ShaderFormat shaderFormat;
    const uint32_t bindingStride;
    const uint32_t attributeOffset;
    const uint32_t memoryOffset;

    BindingParams(VkFormat format_, ShaderFormat shaderFormat_, uint32_t bindingStride_, uint32_t attributeOffset_,
                  uint32_t memoryOffset_)
        : format(format_)
        , shaderFormat(shaderFormat_)
        , bindingStride(bindingStride_)
        , attributeOffset(attributeOffset_)
        , memoryOffset(memoryOffset_)
    {
    }

    uint32_t getRandomSeed(void) const
    {
        // shaderFormat:    2 bits
        // bindingStride:   5 bits
        // attributeOffset: 5 bits
        // memoryOffset:    5 bits
        return ((format << 17) | (bindingStride << 10) | (attributeOffset << 5) | memoryOffset);
    }

    std::string getShaderType(void) const
    {
        const auto tcuFormat    = mapVkFormat(format);
        const auto channelCount = tcu::getNumUsedChannels(tcuFormat.order);

        if (channelCount == 1)
        {
            std::string scalarType;

            if (shaderFormat == ShaderFormat::SIGNED_INT)
                scalarType = "int";
            else if (shaderFormat == ShaderFormat::UNSIGNED_INT)
                scalarType = "uint";
            else if (shaderFormat == ShaderFormat::FLOAT)
                scalarType = "float";
            else
                DE_ASSERT(false);

            return scalarType;
        }

        std::string prefix;
        if (shaderFormat == ShaderFormat::SIGNED_INT)
            prefix = "i";
        else if (shaderFormat == ShaderFormat::UNSIGNED_INT)
            prefix = "u";

        return prefix + "vec" + std::to_string(channelCount);
    }

    bool useScalarLayout(void) const
    {
        const auto tcuFormat       = mapVkFormat(format);
        const auto channelCount    = tcu::getNumUsedChannels(tcuFormat.order);
        const auto useScalarLayout = (channelCount == 3); // scalar allows us to avoid the padding bytes in vec3

        return useScalarLayout;
    }
};

using BindingParamsVec = std::vector<BindingParams>;

struct LegacyVertexAttributesParams
{
    const PipelineConstructionType constructionType;
    BindingParamsVec bindings;

    LegacyVertexAttributesParams(PipelineConstructionType constructionType_, BindingParamsVec bindings_)
        : constructionType(constructionType_)
        , bindings()
    {
        bindings.swap(bindings_);
    }

    uint32_t getRandomSeed(void) const
    {
        DE_ASSERT(!bindings.empty());

        uint32_t seed = bindings.at(0u).getRandomSeed();
        for (size_t i = 1; i < bindings.size(); ++i)
            seed = (seed ^ bindings.at(i).getRandomSeed());

        return (0x80000000u | seed);
    }

    bool useScalarLayout(void) const
    {
        return std::any_of(begin(bindings), end(bindings), [](const BindingParams &b) { return b.useScalarLayout(); });
    }
};

using BytesVector = std::vector<uint8_t>;

// Reinterprets an input vector expanding the components to 32-bits as used in the shader, and returns the expected output data.
BytesVector getOutputData(const BytesVector &inputData, const BindingParams &params, uint32_t numPoints)
{
    const auto tcuFormat    = mapVkFormat(params.format);
    const auto channelClass = tcu::getTextureChannelClass(tcuFormat.type);
    const auto channelCount = tcu::getNumUsedChannels(tcuFormat.order);
    const tcu::IVec3 size(static_cast<int>(numPoints), 1, 1);
    const tcu::IVec3 pitch(static_cast<int>(params.bindingStride), 1, 1);

    // We use a ConstPixelBufferAccess to easily intepret the input data according to the right format and extracting values from
    // there as we would do from an image. We also take advantage of the pitch parameter, which is seldomly used, to take the
    // binding stride into account. The pitch is used by the ConstPixelBufferAccess to calculate the memory address of the pixel to
    // read. Note the attribute offset is also used to calculate the start of each pixel.
    tcu::ConstPixelBufferAccess memoryAccess(tcuFormat, size, pitch,
                                             de::dataOrNull(inputData) + params.attributeOffset);

    tcu::Vec4 floatPixel(0.0f);
    tcu::IVec4 intPixel(0);
    tcu::UVec4 uintPixel(0u);
    uint8_t *pixelData = nullptr;

    // We will read pixels using 3 different methods of memoryAccess, storing the result in any of these 3 variables.
    switch (channelClass)
    {
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        pixelData = reinterpret_cast<uint8_t *>(&floatPixel);
        break;
    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        pixelData = reinterpret_cast<uint8_t *>(&intPixel);
        break;
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        pixelData = reinterpret_cast<uint8_t *>(&uintPixel);
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    // Read pixels and store the component bytes (for the used components) in the output data vector.
    // Note pixel component values in the output data vector are always stored as 32-bit values (float, int or uint).
    // See the shader for more details.
    BytesVector outputData;
    outputData.reserve(numPoints * channelCount * k32BitsInBytes);

    for (uint32_t i = 0u; i < numPoints; ++i)
    {
        switch (channelClass)
        {
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            floatPixel = memoryAccess.getPixel(static_cast<int>(i), 0);
            break;
        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            intPixel = memoryAccess.getPixelInt(static_cast<int>(i), 0);
            break;
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            uintPixel = memoryAccess.getPixelUint(static_cast<int>(i), 0);
            break;
        default:
            DE_ASSERT(false);
            break;
        }

        for (int j = 0; j < channelCount; ++j)
        {
            for (uint32_t k = 0; k < k32BitsInBytes; ++k)
            {
                const uint8_t *bytePtr = pixelData + j * k32BitsInBytes + k;
                outputData.push_back(*bytePtr);
            }
        }
    }

    return outputData;
}

BytesVector genInputData(const BindingParams &params, uint32_t numPoints, de::Random &rnd)
{
    DE_ASSERT(numPoints > 0u);

    const auto tcuFormat    = mapVkFormat(params.format);
    const auto channelClass = tcu::getTextureChannelClass(tcuFormat.type);
    const bool floatsUsed =
        (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT || params.shaderFormat == ShaderFormat::FLOAT);
    const int pixelSizeBytes = tcu::getPixelSize(tcuFormat);
    const auto totalBytes    = params.bindingStride * (numPoints - 1u) + params.attributeOffset + pixelSizeBytes;

    BytesVector inputData;
    inputData.reserve(totalBytes);

    for (;;)
    {
        // Should we regenerate the pseudorandom input data vector?
        bool badInputData = false;

        inputData.clear();
        for (uint32_t i = 0; i < totalBytes; ++i)
            inputData.push_back(rnd.getUint8());

        // Floats: we'd like to avoid infs, zeros, nans and denorms to make sure we get identical values back.
        if (floatsUsed)
        {
            // Iterate over the output raw vector as if it was a float vector.
            const auto outputData = getOutputData(inputData, params, numPoints);
            for (size_t i = 0u; i < outputData.size(); i += k32BitsInBytes)
            {
                const auto floatPtr = reinterpret_cast<const float *>(&outputData.at(i));
                tcu::Float32 value(*floatPtr);
                if (value.isNaN() || value.isInf() || value.isDenorm() || value.isZero())
                {
                    badInputData = true;
                    break;
                }
            }
        }

        if (badInputData)
            continue;

        break;
    }

    return inputData;
}

class LegacyVertexAttributesInstance : public vkt::TestInstance
{
public:
    LegacyVertexAttributesInstance(Context &context, const LegacyVertexAttributesParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~LegacyVertexAttributesInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const LegacyVertexAttributesParams m_params;
};

class LegacyVertexAttributesCase : public vkt::TestCase
{
public:
    LegacyVertexAttributesCase(tcu::TestContext &testCtx, const std::string &name,
                               const LegacyVertexAttributesParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    ~LegacyVertexAttributesCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new LegacyVertexAttributesInstance(context, m_params);
    }

protected:
    const LegacyVertexAttributesParams m_params;
};

void LegacyVertexAttributesCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
    context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
    context.requireDeviceFunctionality("VK_EXT_legacy_vertex_attributes");

    // We want to use the scalar layout for *vec3 because that way we avoid the 4 bytes of padding introduced in the output with the
    // std430 layout. The reasons to avoid the padding are varied:
    // 1) Taking the padding into account when generating the expected output data means a bit more code in there, potentially
    //    confusing.
    // 2) The typical padding bytes used are zeros, but we're making sure zeros are not involved (due to sign preservation concerns)
    //    when generating input data (see the checks in genInputData). We'd need to make that check a more complicated and
    //    confusing.
    // 3) Scalar is widely supported anyway, so the number of unsupported tests would still be low and they wouldn't be critical.
    if (m_params.useScalarLayout())
        context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");

    // Format feature support.
    for (const auto &binding : m_params.bindings)
    {
        const auto formatProperties = getPhysicalDeviceFormatProperties(ctx.vki, ctx.physicalDevice, binding.format);
        if ((formatProperties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0u)
            TCU_THROW(NotSupportedError, "Format does not support VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT");
    }
}

void LegacyVertexAttributesCase::initPrograms(SourceCollections &dst) const
{
    const auto useScalarLayout = m_params.useScalarLayout();
    const auto bufferLayout    = (useScalarLayout ? "scalar" : "std430");

    std::ostringstream inOutVert;
    std::ostringstream copyVert;
    std::ostringstream inSetFrag;
    std::ostringstream copyFrag;

    for (size_t i = 0; i < m_params.bindings.size(); ++i)
    {
        const auto &binding   = m_params.bindings.at(i);
        const auto shaderType = binding.getShaderType();

        inOutVert << "layout (location=" << (i + 1) << ") in " << shaderType << " inData" << i << ";\n"
                  << "layout (location=" << i << ") out flat " << shaderType << " outData" << i << ";\n";
        copyVert << "    outData" << i << " = inData" << i << ";\n";
        inSetFrag << "layout (location=" << i << ") in flat " << shaderType << " inData" << i << ";\n"
                  << "layout (set=0, binding=" << i << ", " << bufferLayout << ") buffer VerificationBlock" << i
                  << " {\n"
                  << "    " << shaderType << " value[];\n"
                  << "} verificationBuffer" << i << ";\n";
        copyFrag << "    verificationBuffer" << i << ".value[index] = inData" << i << ";\n";
    }

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << inOutVert.str() << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << copyVert.str() << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << (useScalarLayout ? "#extension GL_EXT_scalar_block_layout : require\n" : "")
         << "layout (location=0) out vec4 outColor;\n"
         << inSetFrag.str() << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "    const int index = int(gl_FragCoord.x);\n"
         << copyFrag.str() << "}\n";

    const auto allowScalars     = static_cast<uint32_t>(ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
    const auto buildOptionFlags = (useScalarLayout ? allowScalars : 0u);
    const ShaderBuildOptions buildOptions(dst.usedVulkanVersion, SPIRV_VERSION_1_0, buildOptionFlags);

    dst.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
}

tcu::TestStatus LegacyVertexAttributesInstance::iterate(void)
{
    const auto &ctx        = m_context.getContextCommonData();
    const int pixelCount   = 16;
    const auto pixelCountU = static_cast<uint32_t>(pixelCount);
    const tcu::IVec3 fbExtent(pixelCount, 1, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto fbFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto fbTcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f);  // Must match fragment shader.
    const tcu::Vec4 colorThres(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto dataStages = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices.
    std::vector<tcu::Vec4> vertices;

    for (int i = 0; i < pixelCount; ++i)
    {
        const float xCoord = ((static_cast<float>(i) + 0.5f) / static_cast<float>(pixelCount) * 2.0f) - 1.0f;
        const tcu::Vec4 position(xCoord, 0.0f, 0.0f, 1.0f);

        vertices.push_back(position);
    }

    const auto seed = m_params.getRandomSeed();
    de::Random rnd(seed);

    std::vector<BytesVector> byteInputs;
    byteInputs.reserve(m_params.bindings.size());
    for (const auto &binding : m_params.bindings)
        byteInputs.emplace_back(genInputData(binding, pixelCountU, rnd));

    // Vertex buffers
    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    using BufferWithMemoryVec = std::vector<BufferWithMemoryPtr>;

    BufferWithMemoryVec vertexBuffers;
    vertexBuffers.reserve(m_params.bindings.size() + 1); // Extra buffer for the positions.

    // Positions.
    {
        const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        vertexBuffers.emplace_back(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible));

        const auto vbAlloc = vertexBuffers.back()->getAllocation();
        void *vbData       = vbAlloc.getHostPtr();

        deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, vbAlloc);
    }

    // Extra data. We use a dedicated allocator for these buffers in order to apply the memory offset. Note we lie about the
    // noncoherent atom size since we want to apply the offset exactly and the non-coheret atom size is irrelevant in this case:
    // we'll flush the whole allocation.
    for (size_t i = 0; i < m_params.bindings.size(); ++i)
    {
        const auto &binding   = m_params.bindings.at(i);
        const auto &inputData = byteInputs.at(i);

        SimpleAllocator offsetAllocator(
            ctx.vkd, ctx.device, getPhysicalDeviceMemoryProperties(ctx.vki, ctx.physicalDevice),
            tcu::just(SimpleAllocator::OffsetParams{VkDeviceSize{1u}, VkDeviceSize{binding.memoryOffset}}));

        const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(inputData));
        const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        vertexBuffers.emplace_back(
            new BufferWithMemory(ctx.vkd, ctx.device, offsetAllocator, vbInfo, MemoryRequirement::HostVisible));

        const auto vbAlloc = vertexBuffers.back()->getAllocation();
        void *vbData       = vbAlloc.getHostPtr();

        deMemcpy(vbData, de::dataOrNull(inputData), de::dataSize(inputData));
        // We can't use flushAlloc() here because the offset may not be a multiple of the non-coherent atom size.
        // Just flush the whole allocation.
        flushMappedMemoryRange(ctx.vkd, ctx.device, vbAlloc.getMemory(), 0, VK_WHOLE_SIZE);
    }

    // Data buffer for verification.
    const auto verifBufferOffset = static_cast<VkDeviceSize>(0);
    BufferWithMemoryVec verifBuffers;
    std::vector<BytesVector> referenceVecs;

    verifBuffers.reserve(byteInputs.size());
    referenceVecs.reserve(byteInputs.size());

    for (size_t i = 0; i < byteInputs.size(); ++i)
    {
        const auto &binding   = m_params.bindings.at(i);
        const auto &inputData = byteInputs.at(i);

        referenceVecs.emplace_back(getOutputData(inputData, binding, pixelCountU));
        const auto &refData = referenceVecs.back();

        const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(refData));
        const auto createInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        verifBuffers.emplace_back(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

        const auto allocation = verifBuffers.back()->getAllocation();
        void *bufferData      = allocation.getHostPtr();

        deMemset(bufferData, 0, de::dataSize(refData));
        flushAlloc(ctx.vkd, ctx.device, allocation);
    }

    // Descriptor pool, set, layout, etc.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, de::sizeU32(verifBuffers));
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder layoutBuilder;
    for (size_t i = 0; i < verifBuffers.size(); ++i)
        layoutBuilder.addSingleBinding(descType, dataStages);
    const auto setLayout     = layoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    for (size_t i = 0; i < verifBuffers.size(); ++i)
    {
        const auto &buffer    = *verifBuffers.at(i);
        const auto dbDescInfo = makeDescriptorBufferInfo(buffer.get(), verifBufferOffset, buffer.getBufferSize());
        updateBuilder.writeSingle(*descriptorSet,
                                  DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)), descType,
                                  &dbDescInfo);
    }
    updateBuilder.update(ctx.vkd, ctx.device);

    const auto pipelineLayout = PipelineLayoutWrapper(m_params.constructionType, ctx.vkd, ctx.device, *setLayout);
    auto renderPass           = RenderPassWrapper(m_params.constructionType, ctx.vkd, ctx.device, fbFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_EXT};
    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.constructionType);
    pipeline.setMonolithicPipelineLayout(pipelineLayout);
    pipeline.setDynamicState(&dynamicStateCreateInfo);
    pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    pipeline.setDefaultRasterizationState();
    pipeline.setDefaultColorBlendState();
    pipeline.setDefaultDepthStencilState();
    pipeline.setDefaultMultisampleState();
    pipeline.setDefaultPatchControlPoints(0u);
    pipeline.setupVertexInputState(&vertexInputStateCreateInfo);
    pipeline.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule);
    pipeline.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule);
    pipeline.setupFragmentOutputState(*renderPass, 0u);
    pipeline.buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
    bindingDescriptions.reserve(vertexBuffers.size());

    {
        // Positions binding.
        bindingDescriptions.emplace_back(VkVertexInputBindingDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, // VkStructureType sType;
            nullptr,                                                  // void* pNext;
            0u,                                                       // uint32_t binding;
            static_cast<uint32_t>(sizeof(tcu::Vec4)),                 // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX,                              // VkVertexInputRate inputRate;
            0u,                                                       // uint32_t divisor;
        });
    }

    for (size_t i = 0; i < m_params.bindings.size(); ++i)
    {
        const auto &binding = m_params.bindings.at(i);

        // Extra data bindings.
        bindingDescriptions.emplace_back(VkVertexInputBindingDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, // VkStructureType sType;
            nullptr,                                                  // void* pNext;
            static_cast<uint32_t>(i + 1),                             // uint32_t binding;
            binding.bindingStride,                                    // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX,                              // VkVertexInputRate inputRate;
            0u,                                                       // uint32_t divisor;
        });
    };

    std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;
    {
        // Position.
        attributeDescriptions.emplace_back(VkVertexInputAttributeDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, // VkStructureType sType;
            nullptr,                                                    // void* pNext;
            0u,                                                         // uint32_t location;
            0u,                                                         // uint32_t binding;
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,                          // VkFormat format;
            0u,                                                         // uint32_t offset;
        });
    }

    for (size_t i = 0; i < m_params.bindings.size(); ++i)
    {
        const auto &binding = m_params.bindings.at(i);
        const auto idx      = static_cast<uint32_t>(i + 1);

        // Extra data attributes.
        attributeDescriptions.emplace_back(VkVertexInputAttributeDescription2EXT{
            VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, // VkStructureType sType;
            nullptr,                                                    // void* pNext;
            idx,                                                        // uint32_t location;
            idx,                                                        // uint32_t binding;
            binding.format,                                             // VkFormat format;
            binding.attributeOffset,                                    // uint32_t offset;
        });
    };

    std::vector<VkBuffer> rawVertexBuffers;
    rawVertexBuffers.reserve(vertexBuffers.size());
    std::transform(begin(vertexBuffers), end(vertexBuffers), std::back_inserter(rawVertexBuffers),
                   [](const BufferWithMemoryPtr &buffer) { return buffer->get(); });

    std::vector<VkDeviceSize> rawVertexBufferOffsets(rawVertexBuffers.size(), static_cast<VkDeviceSize>(0));

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    DE_ASSERT(rawVertexBuffers.size() == rawVertexBufferOffsets.size());
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, de::sizeU32(rawVertexBuffers), de::dataOrNull(rawVertexBuffers),
                                 de::dataOrNull(rawVertexBufferOffsets));
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdSetVertexInputEXT(cmdBuffer, de::sizeU32(bindingDescriptions), de::dataOrNull(bindingDescriptions),
                                 de::sizeU32(attributeDescriptions), de::dataOrNull(attributeDescriptions));
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(fbTcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(fbTcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, geomColor);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, colorThres,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    // Check storage buffers.
    for (size_t idx = 0; idx < m_params.bindings.size(); ++idx)
    {
        const auto &binding = m_params.bindings.at(idx);

        // Related to the vertex format.
        const auto tcuVertexFormat = mapVkFormat(binding.format);
        const auto vertexBitWidth  = tcu::getTextureFormatBitDepth(tcuVertexFormat);
        const auto channelClass    = tcu::getTextureChannelClass(tcuVertexFormat.type);
        const auto channelCount    = tcu::getNumUsedChannels(tcuVertexFormat.order);

        const auto &buffer = *verifBuffers.at(idx);
        invalidateAlloc(ctx.vkd, ctx.device, buffer.getAllocation());

        const auto &refData    = referenceVecs.at(idx);
        const void *bufferData = buffer.getAllocation().getHostPtr();

        BytesVector resultData(refData.size());
        deMemcpy(de::dataOrNull(resultData), bufferData, de::dataSize(resultData));

        DE_ASSERT(resultData.size() == refData.size());
        bool dataOK = true;

        // Used for floating point conversion checks.
        tcu::Vec4 vertexThres(0.0f);
        {
            // Note these thresholds are much larger than the precision requested in section "Floating-Point Format Conversions", which
            // requires that finite values falling between two representable finite values use either of them as the conversion result.
            if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
            {
                for (int i = 0; i < channelCount; ++i)
                    vertexThres[i] = 1.0f / static_cast<float>((1 << (vertexBitWidth[i] + 1)) - 1);
            }
            else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT)
            {
                for (int i = 0; i < channelCount; ++i)
                    vertexThres[i] = 1.0f / static_cast<float>((1 << vertexBitWidth[i]) - 1);
            }
        }

        const auto channelCountU32 = static_cast<uint32_t>(channelCount);
        DE_ASSERT(resultData.size() > 0);
        DE_ASSERT(resultData.size() % (k32BitsInBytes * channelCountU32) == 0u);

        // We'll reinterpret output data in differnt formats.
        const uint32_t *resU32Ptr = reinterpret_cast<uint32_t *>(resultData.data());
        const uint32_t *refU32Ptr = reinterpret_cast<const uint32_t *>(refData.data());

        const int32_t *resI32Ptr = reinterpret_cast<int32_t *>(resultData.data());
        const int32_t *refI32Ptr = reinterpret_cast<const int32_t *>(refData.data());

        const float *resF32Ptr = reinterpret_cast<float *>(resultData.data());
        const float *refF32Ptr = reinterpret_cast<const float *>(refData.data());

        for (uint32_t pointIdx = 0u; pointIdx < pixelCount; ++pointIdx)
        {
            for (uint32_t chIdx = 0u; chIdx < channelCountU32; ++chIdx)
            {
                const auto scalarIdx = pointIdx * channelCountU32 + chIdx;

                if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
                    channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                    channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
                {
                    const auto res = resF32Ptr[scalarIdx];
                    const auto ref = refF32Ptr[scalarIdx];
                    const auto thr = vertexThres[chIdx];

                    if (de::abs(res - ref) > thr)
                    {
                        dataOK = false;
                        log << tcu::TestLog::Message << "Unexpected result in point " << pointIdx << " channel "
                            << chIdx << ": found " << res << " but expected " << ref << " (threshold " << thr << ")"
                            << tcu::TestLog::EndMessage;
                    }
                }
                else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
                {
                    const auto res = resI32Ptr[scalarIdx];
                    const auto ref = refI32Ptr[scalarIdx];

                    if (res != ref)
                    {
                        dataOK = false;
                        log << tcu::TestLog::Message << "Unexpected result in point " << pointIdx << " channel "
                            << chIdx << ": found " << res << " but expected " << ref << tcu::TestLog::EndMessage;
                    }
                }
                else if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
                {
                    const auto res = resU32Ptr[scalarIdx];
                    const auto ref = refU32Ptr[scalarIdx];

                    if (res != ref)
                    {
                        dataOK = false;
                        log << tcu::TestLog::Message << "Unexpected result in point " << pointIdx << " channel "
                            << chIdx << ": found " << res << " but expected " << ref << tcu::TestLog::EndMessage;
                    }
                }
                else
                    DE_ASSERT(false);
            }
        }

        if (!dataOK)
            return tcu::TestStatus::fail("Unexpected result in output buffer; check log for details");
    }

    return tcu::TestStatus::pass("Pass");
}

using FormatVec = std::vector<VkFormat>;
std::string getFormatShortName(const FormatVec &formats)
{
    std::string concat;
    for (const auto format : formats)
        concat += (concat.empty() ? "" : "_") + getFormatSimpleName(format);
    return concat;
}

// Auxiliar, used to check channel bit widths below.
bool checkAny(const tcu::IVec4 values, int channelCount, const std::function<bool(int)> &condition)
{
    const auto count = std::min(channelCount, static_cast<int>(tcu::IVec4::SIZE));
    for (int i = 0; i < count; ++i)
        if (condition(values[i]))
            return true;
    return false;
}

} // namespace

void createLegacyVertexAttributesTests(tcu::TestCaseGroup *group, PipelineConstructionType constructionType)
{
    auto &testContext = group->getTestContext();

    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
    GroupPtr singleGroup(new tcu::TestCaseGroup(testContext, "single_binding"));
    GroupPtr multiGroup(new tcu::TestCaseGroup(testContext, "multi_binding"));

    const VkFormat formatsToTest[] = {
        // Formats with mandatory vertex input support.
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_B8G8R8A8_UNORM,        // weird
        VK_FORMAT_A8B8G8R8_UNORM_PACK32, // pack?
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32, // interesting, pack
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R32G32B32_SINT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,

        // 3-component formats do not have that feature as mandatory, but we should still try.
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_R8G8B8_SINT,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R16G16B16_UINT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16G16B16_SFLOAT,
    };

    const struct
    {
        ShaderFormat shaderFormat;
        const char *desc;
    } shaderFormats[] = {
        {ShaderFormat::SIGNED_INT, "shader_int"},
        {ShaderFormat::UNSIGNED_INT, "shader_uint"},
        {ShaderFormat::FLOAT, "shader_float"},
    };

    const auto lessThan32Bits = [](int width) { return width < 32 /*bits*/; };

    // Single binding tests.
    for (const auto &format : formatsToTest)
    {
        const auto tcuFormat      = mapVkFormat(format);
        const int formatSize      = tcu::getPixelSize(tcuFormat);
        const auto fmtClass       = tcu::getTextureChannelClass(tcuFormat.type);
        const auto vertexBitWidth = tcu::getTextureFormatBitDepth(tcuFormat);
        const auto channelCount   = tcu::getNumUsedChannels(tcuFormat.order);

        const std::set<uint32_t> strides{
            0u,
            1u,
            static_cast<uint32_t>(formatSize),
            static_cast<uint32_t>(formatSize + formatSize - 1),
        };

        for (const uint32_t stride : strides)
            for (const auto shaderFormat : shaderFormats)
            {
                const bool isFloatFormat   = (fmtClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                                            fmtClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                                            fmtClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT);
                const bool isIntegerFormat = !isFloatFormat;

                // Float-like formats do not need to be reinterpreted as both signed and unsigned integers in the shader, one of
                // them is enough.
                if (isFloatFormat)
                {
                    const auto fmtId  = static_cast<int>(format);
                    const auto fmtMod = fmtId % 2;

                    if (fmtMod == 0 && shaderFormat.shaderFormat == ShaderFormat::SIGNED_INT)
                        continue;

                    if (fmtMod == 1 && shaderFormat.shaderFormat == ShaderFormat::UNSIGNED_INT)
                        continue;
                }

                if (isIntegerFormat && shaderFormat.shaderFormat == ShaderFormat::FLOAT)
                {
                    // Integer formats with less than 4 bytes in any channel should not go through the shader as floats because,
                    // when the values are expanded to 32-bits, the upper byte(s) will be zeros and, if they're to be interpreted as
                    // floats, it's likely the mantissa is nonzero and the exponent zero, so it doesn't pass the denorm check we
                    // run in genInputData. Note for 24-bit channels this wouldn't always be true but it's true for half the values,
                    // which would make it unlikely that we could generate 16 inputs without wasting a lot of time.
                    bool skip = checkAny(vertexBitWidth, channelCount, lessThan32Bits);
                    if (skip)
                        continue;
                }

                for (const auto attributeOffset : {0u, 1u})
                    for (const auto memoryOffset : {0u, 1u})
                    {
                        if (attributeOffset != 0u || memoryOffset != 0u)
                        {
                            // Skip tests that do not produce unaligned access despite attempting to use attributeOffset and memoryOffset.
                            bool aligned =
                                !checkAny(vertexBitWidth, channelCount, [](int width) { return width > 8 /*bits*/; });
                            if (aligned)
                                continue;
                        }

                        const auto shortName = getFormatSimpleName(format);
                        const auto aoSuffix  = ((attributeOffset > 0u) ?
                                                    std::string("_attribute_offset_") + std::to_string(attributeOffset) :
                                                    "");
                        const auto moSuffix =
                            ((memoryOffset > 0u) ? std::string("_memory_offset_") + std::to_string(memoryOffset) : "");
                        const auto testName = shortName + "_" + shaderFormat.desc + "_stride_" +
                                              std::to_string(stride) + aoSuffix + moSuffix;

                        // Single binding.
                        const BindingParams bindingParams{format, shaderFormat.shaderFormat, stride, attributeOffset,
                                                          memoryOffset};

                        const LegacyVertexAttributesParams params{constructionType,
                                                                  BindingParamsVec(1u, bindingParams)};
                        singleGroup->addChild(new LegacyVertexAttributesCase(testContext, testName, params));
                    }
            }
    }

    // Tests using multiple bindings.
    {
        // We don't want many of these tests so the selected formats are a mix of components, numeric formats and bitwidth.
        const std::vector<VkFormat> formatTuples[] = {
            {VK_FORMAT_R8_UNORM, VK_FORMAT_R16G16_UINT, VK_FORMAT_R32G32B32A32_SINT},
            {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R8G8_UINT},
            {VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16_SINT, VK_FORMAT_R8G8_UNORM},
        };

        for (const auto &tuple : formatTuples)
        {
            for (const bool singleByteStride : {false, true})
                for (const auto attributeOffset : {0u, 1u})
                    for (const auto memoryOffset : {0u, 1u})
                    {
                        BindingParamsVec bindingParams;
                        for (const auto format : tuple)
                        {
                            const auto tcuFormat      = mapVkFormat(format);
                            const int formatSize      = tcu::getPixelSize(tcuFormat);
                            const auto fmtClass       = tcu::getTextureChannelClass(tcuFormat.type);
                            const auto vertexBitWidth = tcu::getTextureFormatBitDepth(tcuFormat);
                            const auto channelCount   = tcu::getNumUsedChannels(tcuFormat.order);

                            ShaderFormat shaderFormat = ShaderFormat::INVALID;
                            const bool isFloatFormat  = (fmtClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                                                        fmtClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                                                        fmtClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT);

                            if (isFloatFormat)
                            {
                                // Use a signed or unsigned format in the shader.
                                const auto fmtId  = static_cast<int>(format);
                                const auto fmtMod = fmtId % 2;
                                const std::vector<ShaderFormat> options{ShaderFormat::SIGNED_INT,
                                                                        ShaderFormat::UNSIGNED_INT};

                                shaderFormat = options.at(fmtMod);
                            }
                            else
                            {
                                // For integer formats use floats if possible in the shader, or the alternative signed/unsigned
                                // variant if not.
                                const bool signedClass = (fmtClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
                                const ShaderFormat integerAlternative =
                                    (signedClass ? ShaderFormat::UNSIGNED_INT : ShaderFormat::SIGNED_INT);
                                const bool hasSmallChannels = checkAny(vertexBitWidth, channelCount, lessThan32Bits);

                                shaderFormat = (hasSmallChannels ? integerAlternative : ShaderFormat::FLOAT);
                            }

                            DE_ASSERT(shaderFormat != ShaderFormat::INVALID);

                            const auto stride = (singleByteStride ? 1u : static_cast<uint32_t>(formatSize));

                            bindingParams.emplace_back(format, shaderFormat, stride, attributeOffset, memoryOffset);
                        }

                        const LegacyVertexAttributesParams testParams(constructionType, bindingParams);

                        const auto shortName    = getFormatShortName(tuple);
                        const auto strideSuffix = (singleByteStride ? "_stride_1_byte" : "_stride_normal");
                        const auto aoSuffix     = ((attributeOffset > 0u) ?
                                                       std::string("_attribute_offset_") + std::to_string(attributeOffset) :
                                                       "");
                        const auto moSuffix =
                            ((memoryOffset > 0u) ? std::string("_memory_offset_") + std::to_string(memoryOffset) : "");
                        const auto testName = shortName + strideSuffix + aoSuffix + moSuffix;

                        multiGroup->addChild(new LegacyVertexAttributesCase(testContext, testName, testParams));
                    }
        }
    }

    group->addChild(singleGroup.release());
    group->addChild(multiGroup.release());
}

} // namespace pipeline
} // namespace vkt
