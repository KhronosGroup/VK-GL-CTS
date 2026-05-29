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
 * \brief Robustness out of bounds access tests.
 *
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessOOBAccessTests.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseDefs.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuVectorType.hpp"

namespace vkt
{
namespace robustness
{

using namespace vk;
using namespace vkt;
using namespace tcu;

enum OOBAccessType
{
    OFF_BY_ONE = 0,
    OFF
};

enum TexelBufferType
{
    TEXEL_BUFFER_UNIFORM = 0,
    TEXEL_BUFFER_STORAGE,
};

enum RobustnessLevel
{
    ROBUST_BUFFER_NONE = 0,
    ROBUST_BUFFER_ACCESS,
    ROBUST_BUFFER_ACCESS2,
};

struct OOBCommonParams
{
    VkFormat format;
    bool isRobust;
    bool isRead;
    OOBAccessType oobAccess;
};

struct OOBBufferParams
{
    RobustnessLevel robustnessLevel;
    TexelBufferType bufferType;
    uint32_t backingSize;
};

struct OOBImageParams
{
    UVec2 imageExtent;
};

struct OOBBufferPushConstants
{
    uint32_t accessIndex;
};

struct OOBImagePushConstants
{
    IVec2 accessIndex;
};

bool formatIsR64(const VkFormat &format)
{
    switch (format)
    {
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_UINT:
        return true;
    default:
        return false;
    }
}

class OOBBufferTestCase : public TestCase
{
public:
    OOBBufferTestCase(TestContext &context, const std::string &name, const OOBCommonParams &params,
                      const OOBBufferParams &buffParams);
    ~OOBBufferTestCase(void);

    void checkSupport(Context &context) const override;
    std::string getRequiredCapabilitiesId() const override;
    void initDeviceCapabilities(DevCaps &caps) override;
    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    const OOBCommonParams m_params;
    const OOBBufferParams m_bufferParams;
};

OOBBufferTestCase::OOBBufferTestCase(TestContext &context, const std::string &name, const OOBCommonParams &params,
                                     const OOBBufferParams &buffParams)
    : TestCase(context, name)
    , m_params(params)
    , m_bufferParams(buffParams)
{
}

OOBBufferTestCase::~OOBBufferTestCase(void)
{
}

void commonCheckSupport(Context &context, OOBCommonParams params)
{
    if (formatIsR64(params.format))
    {
        context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

        if (!context.getDeviceFeatures().shaderInt64 ||
            !context.getShaderAtomicInt64Features().shaderBufferInt64Atomics)
            TCU_THROW(NotSupportedError, "64-bit integers not supported in shaders");

        if (context.getShaderImageAtomicInt64FeaturesEXT().shaderImageInt64Atomics == VK_FALSE)
        {
            TCU_THROW(NotSupportedError, "shaderImageInt64Atomics is not supported");
        }
    }
}

void OOBBufferTestCase::checkSupport(Context &context) const
{
    const auto &vki                = context.getInstanceInterface();
    const auto physicalDevice      = context.getPhysicalDevice();
    const uint32_t maxDeviceTexels = getPhysicalDeviceProperties(vki, physicalDevice).limits.maxTexelBufferElements;
    const uint32_t numTexels       = (m_bufferParams.backingSize / 2u) / getPixelSize(mapVkFormat(m_params.format));

    if (m_params.isRobust)
    {
        if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
            !getPhysicalDeviceFeatures(vki, physicalDevice).robustBufferAccess)
            TCU_THROW(NotSupportedError,
                      "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");

        if (m_bufferParams.robustnessLevel == ROBUST_BUFFER_ACCESS2)
        {
            VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
            VkPhysicalDeviceFeatures2 features2                        = initVulkanStructure(&robustness2Features);

            vki.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

            context.requireDeviceFunctionality("VK_EXT_robustness2");

            if (!robustness2Features.robustBufferAccess2)
                TCU_THROW(NotSupportedError, "robustBufferAccess2 not supported");
        }
    }

#ifndef CTS_USES_VULKANSC
    const VkFormatProperties3 formatProperties(context.getFormatProperties(m_params.format));
    if ((m_bufferParams.bufferType == TexelBufferType::TEXEL_BUFFER_UNIFORM) &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for uniform texel buffers");

    if ((m_bufferParams.bufferType == TexelBufferType::TEXEL_BUFFER_STORAGE) &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");
#endif

    if (numTexels > maxDeviceTexels)
        TCU_THROW(NotSupportedError, "maxTexelBufferElements not large enough");

    commonCheckSupport(context, m_params);
}

std::string OOBBufferTestCase::getRequiredCapabilitiesId() const
{
    return typeid(OOBBufferTestCase).name() +
           std::string((m_bufferParams.robustnessLevel == ROBUST_BUFFER_ACCESS) ? "-RBA" : "-RBA2") +
           std::string(formatIsR64(m_params.format) ? "-R64" : "-R32");
}

void OOBBufferTestCase::initDeviceCapabilities(DevCaps &caps)
{
    if (m_params.isRobust)
    {
        if (m_bufferParams.robustnessLevel == ROBUST_BUFFER_ACCESS)
            caps.addFeature(&VkPhysicalDeviceFeatures::robustBufferAccess);
        else
        {
            caps.addExtension("VK_EXT_robustness2");
            caps.addFeature(&VkPhysicalDeviceFeatures::robustBufferAccess);
            caps.addFeature(&VkPhysicalDeviceRobustness2FeaturesEXT::robustBufferAccess2);
        }
    }

    if (formatIsR64(m_params.format))
    {
        caps.addExtension("VK_EXT_shader_image_atomic_int64");
        caps.addFeature(&VkPhysicalDeviceFeatures::shaderInt64);
        caps.addFeature(&VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT::shaderImageInt64Atomics);
    }
}

void OOBBufferTestCase::initPrograms(SourceCollections &programCollection) const
{
    const bool isTexelUniformBuffer = (m_bufferParams.bufferType == TEXEL_BUFFER_UNIFORM);
    const bool isformat64b          = formatIsR64(m_params.format);

    const char *const versionDecl = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450);
    const std::string glslOpStr = m_params.isRead ? (isTexelUniformBuffer ? "texelFetch" : "imageLoad") : "imageStore";

    const std::string glslBufferTypeStr =
        isTexelUniformBuffer ? "utextureBuffer" : (isformat64b ? "u64imageBuffer" : "uimageBuffer");
    const std::string glslBufferFmtStr = isTexelUniformBuffer ? "" : (isformat64b ? ", r64ui" : ", r32ui");

    std::ostringstream prog;

    prog << versionDecl << "\n";

    if (isformat64b)
    {
        prog << "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
             << "#extension GL_EXT_shader_image_int64 : require\n";
    }

    if (m_params.isRead)
    {
        const std::string glslOutTypeStr = isformat64b ? "uint64_t" : "uint";

        prog << "layout(set = 0, binding = 0" << glslBufferFmtStr << ") uniform " << glslBufferTypeStr
             << " texelBuffer;\n"
             << "layout(set = 0, binding = 1, std430) buffer OutputBuffer { \n"
             << "    " << glslOutTypeStr << " outputData; \n"
             << "} outBuffer;\n"
             << "layout(push_constant) uniform PushConstants {\n"
             << "    int index;\n"
             << "} pc;\n"
             << "layout(local_size_x = 1) in;\n"

             << "void main (void)\n"
             << "{\n"
             << "    " << glslOutTypeStr << " value = " << glslOpStr << "(texelBuffer, pc.index).x; \n"
             << "    outBuffer.outputData = value; \n"
             << "}\n";
    }
    else
    {
        // write
        const std::string glslInDataTypeStr = isformat64b ? "uint64_t" : "uint";
        const std::string glslInTypeStr     = isformat64b ? "u64vec4" : "uvec4";

        prog << "layout(set = 0, binding = 0" << glslBufferFmtStr << ") uniform " << glslBufferTypeStr
             << " texelBuffer;\n"
             << "layout(set = 0, binding = 1, std430) buffer InputBuffer { \n"
             << "    " << glslInDataTypeStr << " inputData; \n"
             << "} inBuffer;\n"
             << "layout(push_constant) uniform PushConstants {\n"
             << "    int index;\n"
             << "} pc;\n"
             << "layout(local_size_x = 1) in;\n"

             << "void main (void)\n"
             << "{\n"
             << "    " << glslInTypeStr << " value = " << glslInTypeStr << "(inBuffer.inputData, 0u, 0u, 0u);\n"
             << "    " << glslOpStr << "(texelBuffer, pc.index, value); \n"
             << "}\n";
    }

    programCollection.glslSources.add("comp")
        << glu::ComputeSource(prog.str())
        << ShaderBuildOptions(programCollection.usedVulkanVersion, isformat64b ? SPIRV_VERSION_1_3 : SPIRV_VERSION_1_0,
                              0u, true);
}

class OOBBufferTestInstance : public TestInstance
{
public:
    OOBBufferTestInstance(Context &context, const OOBCommonParams &params, const OOBBufferParams &buffParams);
    ~OOBBufferTestInstance(void);

    virtual TestStatus iterate(void);

private:
    const OOBCommonParams m_params;
    const OOBBufferParams m_bufferParams;
};

OOBBufferTestInstance::OOBBufferTestInstance(Context &context, const OOBCommonParams &params,
                                             const OOBBufferParams &buffParams)
    : TestInstance(context)
    , m_params(params)
    , m_bufferParams(buffParams)
{
    // create buffer
}

OOBBufferTestInstance::~OOBBufferTestInstance(void)
{
}

TestStatus OOBBufferTestInstance::iterate(void)
{
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    const auto queue            = m_context.getUniversalQueue();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &allocator             = m_context.getDefaultAllocator();

    const bool isTexelUniformBuffer = (m_bufferParams.bufferType == TEXEL_BUFFER_UNIFORM);

    const uint32_t bufferViewSizeInBytes  = m_bufferParams.backingSize / 2u; // half of backing size
    const uint32_t texelSizeInBytes       = getPixelSize(mapVkFormat(m_params.format));
    const uint32_t bufferViewSizeInTexels = bufferViewSizeInBytes / texelSizeInBytes;
    const uint32_t offByOneTexelIdx       = bufferViewSizeInTexels;
    const uint32_t offTexelIdx            = bufferViewSizeInTexels + (bufferViewSizeInTexels / 2u);
    const uint32_t accessTexelIndex       = ((m_params.oobAccess == OFF_BY_ONE) ? offByOneTexelIdx : offTexelIdx);
    const uint32_t numOfWorkgroups        = 1u; // access only 1 texel

    const VkBufferUsageFlags bufferUsageFlags =
        (isTexelUniformBuffer ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);

    // Create texel buffer
    const auto texelBufferInfo =
        makeBufferCreateInfo(m_bufferParams.backingSize,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | bufferUsageFlags);

    const BufferWithMemory texelBuffer{vkd, device, allocator, texelBufferInfo, MemoryRequirement::HostVisible};

    // Create a texel buffer view
    Move<VkBufferView> texelBufferView =
        makeBufferView(vkd, device, *texelBuffer, m_params.format, 0u, bufferViewSizeInBytes);

    // Create initalization buffer - input or output
    const auto initBufferInfo = makeBufferCreateInfo(
        m_bufferParams.backingSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const BufferWithMemory initBuffer{vkd, device, allocator, initBufferInfo, MemoryRequirement::HostVisible};

    // Initialize initalization buffer
    std::vector<uint8_t> referenceData;
    {
        referenceData.resize(m_bufferParams.backingSize);
        for (uint32_t i = 0; i < m_bufferParams.backingSize; ++i)
            referenceData[i] = static_cast<uint8_t>((i % 255u) + 1u);

        const auto initBufferAlloc = initBuffer.getAllocation();
        deMemcpy(initBufferAlloc.getHostPtr(), &referenceData[0], m_bufferParams.backingSize);
        flushAlloc(vkd, device, initBufferAlloc);
    }

    // Create in-out buffer for read/write values
    const auto ioBufferInfo =
        makeBufferCreateInfo(texelSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const BufferWithMemory ioBuffer{vkd, device, allocator, ioBufferInfo, MemoryRequirement::HostVisible};

    // Initialize in-out buffer with all 0xFF
    {
        std::vector<uint8_t> randomData(texelSizeInBytes, 0xFF);
        const auto ioBufferAlloc = ioBuffer.getAllocation();
        deMemcpy(ioBufferAlloc.getHostPtr(), &randomData[0], texelSizeInBytes);
        flushAlloc(vkd, device, ioBufferAlloc);
    }

    // Push constant range
    const VkPushConstantRange pcRange = {
        VK_SHADER_STAGE_COMPUTE_BIT,                           // VkShaderStageFlags stageFlags;
        0u,                                                    // uint32_t offset;
        static_cast<uint32_t>(sizeof(OOBBufferPushConstants)), // uint32_t size;
    };

    // Push constant data
    const OOBBufferPushConstants pushConstants = {accessTexelIndex};

    // Now bind the buffer as a uniform/storage texel buffer in a descriptor set
    VkDescriptorType descriptorType =
        (isTexelUniformBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);

    // Create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(descriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vkd, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descriptorType, 1u)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo ioBufferDescInfo = makeDescriptorBufferInfo(*ioBuffer, 0ull, texelSizeInBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                     &texelBufferView.get())
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ioBufferDescInfo)
        .update(vkd, device);

    // Create compute pipeline
    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vkd, device, *descriptorSetLayout, &pcRange));
    const Unique<VkPipeline> computePipeline(makeComputePipeline(vkd, device, *pipelineLayout, *shaderModule));

    // Create command buffer
    const Unique<VkCommandPool> commandPool(makeCommandPool(vkd, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> commandBufferPtr(
        allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto commandBuffer = *commandBufferPtr;

    // Start recording command buffer
    beginCommandBuffer(vkd, commandBuffer);

    // Create barrier to update buffers
    {
        const VkBufferMemoryBarrier initBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *initBuffer, 0ull, m_bufferParams.backingSize);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                               nullptr, 1u, &initBufferBarrier, 0u, nullptr);

        const VkBufferMemoryBarrier ioBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, m_params.isRead ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
            *ioBuffer, 0ull, texelSizeInBytes);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                               nullptr, 1u, &ioBufferBarrier, 0u, nullptr);
    }

    // Copy initial data to texel buffer
    VkBufferCopy copyRegion = makeBufferCopy(0u, 0u, m_bufferParams.backingSize);
    vkd.cmdCopyBuffer(commandBuffer, *initBuffer, *texelBuffer, 1u, &copyRegion);

    vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);

    vkd.cmdPushConstants(commandBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                         static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);

    {
        const VkBufferMemoryBarrier texelBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, m_params.isRead ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
            *texelBuffer, 0ull, m_bufferParams.backingSize);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
                               0u, nullptr, 1u, &texelBufferBarrier, 0u, nullptr);
    }

    vkd.cmdDispatch(commandBuffer, numOfWorkgroups, 1u, 1u);

    if (m_params.isRead)
    {
        const VkBufferMemoryBarrier ioBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *ioBuffer, 0ull, texelSizeInBytes);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                               nullptr, 1u, &ioBufferBarrier, 0u, nullptr);
    }
    else
    {
        const VkBufferMemoryBarrier texelBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *texelBuffer, 0ull, m_bufferParams.backingSize);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                               nullptr, 1u, &texelBufferBarrier, 0u, nullptr);
    }

    endCommandBuffer(vkd, commandBuffer);

    // Submit commands
    submitCommandsAndWait(vkd, device, queue, commandBuffer);

    // Retrieve data from output buffer to host memory
    const auto outputBufferAlloc = m_params.isRead ? ioBuffer.getAllocation() : texelBuffer.getAllocation();
    invalidateAlloc(vkd, device, outputBufferAlloc);

    if (m_bufferParams.robustnessLevel == ROBUST_BUFFER_ACCESS2)
    {
        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc.getHostPtr());

        if (m_params.isRead)
        {
            // check that the OOB read (but inside the backing memory)
            // reads 0 at the OOB index
            const std::vector<uint8_t> refData(texelSizeInBytes, 0u);

            if (deMemCmp(de::dataOrNull(refData), outputData, texelSizeInBytes) != 0)
                return tcu::TestStatus::fail("Failed");
        }
        else
        {
            // Check that OOB write did not change anything inside the buffer
            const uint8_t *refData = &referenceData[0];

            if (deMemCmp(refData, outputData, m_bufferParams.backingSize) != 0)
                return tcu::TestStatus::fail("Failed");
        }
    }

    return TestStatus::pass("Pass");
}

TestInstance *OOBBufferTestCase::createInstance(Context &context) const
{
    return new OOBBufferTestInstance(context, m_params, m_bufferParams);
}

class OOBImageTestCase : public TestCase
{
public:
    OOBImageTestCase(TestContext &context, const std::string &name, const OOBCommonParams &params,
                     const OOBImageParams imageParams);
    ~OOBImageTestCase(void);

    void checkSupport(Context &context) const override;
    std::string getRequiredCapabilitiesId() const override;
    void initDeviceCapabilities(DevCaps &caps) override;
    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    const OOBCommonParams m_params;
    const OOBImageParams m_imageParams;
};

OOBImageTestCase::OOBImageTestCase(TestContext &context, const std::string &name, const OOBCommonParams &params,
                                   const OOBImageParams imageParams)
    : TestCase(context, name)
    , m_params(params)
    , m_imageParams(imageParams)
{
}

OOBImageTestCase::~OOBImageTestCase(void)
{
}

void OOBImageTestCase::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    if (m_params.isRobust)
    {
        context.requireDeviceFunctionality("VK_EXT_image_robustness");
    }

#ifndef CTS_USES_VULKANSC
    const VkFormatProperties3 formatProperties(context.getFormatProperties(m_params.format));
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");
#endif

    VkImageFormatProperties imageFormatProperties;
    VkImageUsageFlags usage =
        (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    VkResult result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_params.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0u, &imageFormatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Image format is not supported for required usage");
    }

    commonCheckSupport(context, m_params);
}

std::string OOBImageTestCase::getRequiredCapabilitiesId() const
{
    return typeid(OOBImageTestCase).name() + std::string(m_params.isRobust ? "-RobustOn" : "-RobutOff") +
           std::string(formatIsR64(m_params.format) ? "-R64" : "-R32");
}

void OOBImageTestCase::initDeviceCapabilities(DevCaps &caps)
{
    if (m_params.isRobust)
    {
        caps.addExtension("VK_EXT_image_robustness");
        caps.addFeature(&VkPhysicalDeviceImageRobustnessFeaturesEXT::robustImageAccess);
    }

    if (formatIsR64(m_params.format))
    {
        caps.addExtension("VK_EXT_shader_image_atomic_int64");
        caps.addFeature(&VkPhysicalDeviceFeatures::shaderInt64);
        caps.addFeature(&VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT::shaderImageInt64Atomics);
    }
}

void OOBImageTestCase::initPrograms(SourceCollections &programCollection) const
{
    const bool isformat64b = formatIsR64(m_params.format);

    const char *const versionDecl      = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450);
    const std::string glslImageTypeStr = isformat64b ? "u64image2D" : "uimage2D";
    const std::string glslOpStr        = m_params.isRead ? "imageLoad" : "imageStore";
    const std::string glslImageFmtStr  = isformat64b ? ", r64ui" : ", r32ui";

    std::ostringstream prog;

    prog << versionDecl << "\n";

    if (isformat64b)
    {
        prog << "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
             << "#extension GL_EXT_shader_image_int64 : require\n";
    }

    if (m_params.isRead)
    {
        const std::string glslOutTypeStr = isformat64b ? "uint64_t" : "uint";

        prog << "layout(set = 0, binding = 0" << glslImageFmtStr << ") uniform " << glslImageTypeStr
             << " storageImage;\n"
             << "layout(set = 0, binding = 1, std430) buffer OutputBuffer { \n"
             << "    " << glslOutTypeStr << " outputData; \n"
             << "} outBuffer;\n"
             << "layout(push_constant) uniform PushConstants {\n"
             << "    ivec2 index;\n"
             << "} pc;\n"
             << "layout(local_size_x = 1) in;\n"

             << "void main (void)\n"
             << "{\n"
             << "    " << glslOutTypeStr << " value = " << glslOpStr << "(storageImage, pc.index).x; \n"
             << "    outBuffer.outputData = value; \n"
             << "}\n";
    }
    else
    {
        // write
        const std::string glslInDataTypeStr = isformat64b ? "uint64_t" : "uint";
        const std::string glslInTypeStr     = isformat64b ? "u64vec4" : "uvec4";

        prog << "layout(set = 0, binding = 0" << glslImageFmtStr << ") uniform " << glslImageTypeStr
             << " storageImage;\n"
             << "layout(set = 0, binding = 1, std430) buffer InputBuffer { \n"
             << "    " << glslInDataTypeStr << " inputData; \n"
             << "} inBuffer;\n"
             << "layout(push_constant) uniform PushConstants {\n"
             << "    ivec2 index;\n"
             << "} pc;\n"
             << "layout(local_size_x = 1) in;\n"

             << "void main (void)\n"
             << "{\n"
             << "    " << glslInTypeStr << " value = " << glslInTypeStr << "(inBuffer.inputData, 0u, 0u, 0u);\n"
             << "    " << glslOpStr << "(storageImage, pc.index, value); \n"
             << "}\n";
    }

    programCollection.glslSources.add("comp")
        << glu::ComputeSource(prog.str())
        << ShaderBuildOptions(programCollection.usedVulkanVersion, isformat64b ? SPIRV_VERSION_1_3 : SPIRV_VERSION_1_0,
                              0u, true);
}

class OOBImageTestInstance : public TestInstance
{
public:
    OOBImageTestInstance(Context &context, const OOBCommonParams &params, const OOBImageParams &imageParams);
    ~OOBImageTestInstance(void);

    virtual TestStatus iterate(void);

private:
    const OOBCommonParams m_params;
    const OOBImageParams m_imageParams;
};

OOBImageTestInstance::OOBImageTestInstance(Context &context, const OOBCommonParams &params,
                                           const OOBImageParams &imageParams)
    : TestInstance(context)
    , m_params(params)
    , m_imageParams(imageParams)
{
}

OOBImageTestInstance::~OOBImageTestInstance(void)
{
}

VkImageCreateInfo makeImageCreateInfo(const VkImageType &imageType, const UVec2 imageSize, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkImageCreateFlags flags,
                                      const VkImageTiling tiling)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        flags,                                          // VkImageCreateFlags flags;
        imageType,                                      // VkImageType imageType;
        format,                                         // VkFormat format;
        makeExtent3D(imageSize.x(), imageSize.y(), 1u), // VkExtent3D extent;
        1u,                                             // uint32_t mipLevels;
        1u,                                             // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                          // VkSampleCountFlagBits samples;
        tiling,                                         // VkImageTiling tiling;
        usage,                                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
        0u,                                             // uint32_t queueFamilyIndexCount;
        nullptr,                                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                      // VkImageLayout initialLayout;
    };
    return imageParams;
}

TestStatus OOBImageTestInstance::iterate(void)
{
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    const auto queue            = m_context.getUniversalQueue();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &allocator             = m_context.getDefaultAllocator();

    const uint32_t texelSizeInBytes   = getPixelSize(mapVkFormat(m_params.format));
    const UVec2 imageViewSizeInTexels = m_imageParams.imageExtent;
    const uint32_t imageSizeInBytes   = imageViewSizeInTexels.x() * imageViewSizeInTexels.y() * texelSizeInBytes;
    const UVec2 offByOneTexelIdx      = imageViewSizeInTexels;
    const UVec2 offTexelIdx           = imageViewSizeInTexels + UVec2(64u, 64u);
    const UVec2 accessTexelIndex      = ((m_params.oobAccess == OFF_BY_ONE) ? offByOneTexelIdx : offTexelIdx);
    const uint32_t numOfWorkgroups    = 1u; // access only 1 texel

    const VkBufferUsageFlags imageUsageFlags =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Create image
    const auto storageImageInfo = makeImageCreateInfo(VK_IMAGE_TYPE_2D, imageViewSizeInTexels, m_params.format,
                                                      imageUsageFlags, 0u, VK_IMAGE_TILING_OPTIMAL);

    const ImageWithMemory storageImage{vkd, device, allocator, storageImageInfo, MemoryRequirement::Any};

    // Create image view
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    Move<VkImageView> storageImageView =
        makeImageView(vkd, device, *storageImage, VK_IMAGE_VIEW_TYPE_2D, m_params.format, colorSubresourceRange);

    // Create buffer (for initialization and result image in case of write)
    const auto imageBufferInfo =
        makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const BufferWithMemory imageBuffer{vkd, device, allocator, imageBufferInfo, MemoryRequirement::HostVisible};

    // Initialize image buffer
    std::vector<uint8_t> referenceData;
    {
        referenceData.resize(imageSizeInBytes);
        for (uint32_t i = 0; i < imageSizeInBytes; ++i)
            referenceData[i] = static_cast<uint8_t>((i % 255u) + 1u);

        const auto imageBufferAlloc = imageBuffer.getAllocation();
        deMemcpy(imageBufferAlloc.getHostPtr(), &referenceData[0], imageSizeInBytes);
        flushAlloc(vkd, device, imageBufferAlloc);
    }

    // Create in-out buffer for read/write values
    const auto ioBufferInfo =
        makeBufferCreateInfo(texelSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const BufferWithMemory ioBuffer{vkd, device, allocator, ioBufferInfo, MemoryRequirement::HostVisible};

    // Initialize output buffer with all 0xFF
    {
        std::vector<uint8_t> randomData(texelSizeInBytes, 0xFF);
        const auto ioBufferAlloc = ioBuffer.getAllocation();
        deMemcpy(ioBufferAlloc.getHostPtr(), &randomData[0], texelSizeInBytes);
        flushAlloc(vkd, device, ioBufferAlloc);
    }

    // Push constant range
    const VkPushConstantRange pcRange = {
        VK_SHADER_STAGE_COMPUTE_BIT,                          // VkShaderStageFlags stageFlags;
        0u,                                                   // uint32_t offset;
        static_cast<uint32_t>(sizeof(OOBImagePushConstants)), // uint32_t size;
    };

    // Push constant data
    const OOBImagePushConstants pushConstants = {IVec2(accessTexelIndex.x(), accessTexelIndex.y())};

    // Now bind the storage image in a descriptor set
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    // Create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(descriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vkd, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descriptorType, 1u)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorImageInfo storageImageDescInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *storageImageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo ioBufferDescInfo = makeDescriptorBufferInfo(*ioBuffer, 0ull, texelSizeInBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                     &storageImageDescInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ioBufferDescInfo)
        .update(vkd, device);

    // Create compute pipeline
    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vkd, device, *descriptorSetLayout, &pcRange));
    const Unique<VkPipeline> computePipeline(makeComputePipeline(vkd, device, *pipelineLayout, *shaderModule));

    const auto imageSubresourceLayers  = makeDefaultImageSubresourceLayers();
    const VkBufferImageCopy copyRegion = makeBufferImageCopy(
        makeExtent3D(imageViewSizeInTexels.x(), imageViewSizeInTexels.y(), 1u), imageSubresourceLayers);
    const std::vector<VkBufferImageCopy> copyRegions(1u, copyRegion);

    // Create command buffer
    const Unique<VkCommandPool> commandPool(makeCommandPool(vkd, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> commandBufferPtr(
        allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto commandBuffer = *commandBufferPtr;

    // Start recording command buffer
    beginCommandBuffer(vkd, commandBuffer);

    // Create barrier to update buffers
    {
        const VkBufferMemoryBarrier imageBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *imageBuffer, 0ull, imageSizeInBytes);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                               nullptr, 1u, &imageBufferBarrier, 0u, nullptr);

        const VkBufferMemoryBarrier ioBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, m_params.isRead ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
            *ioBuffer, 0ull, texelSizeInBytes);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                               nullptr, 1u, &ioBufferBarrier, 0u, nullptr);
    }

    // Copy initial data to image
    copyBufferToImage(vkd, commandBuffer, *imageBuffer, imageSizeInBytes, copyRegions, VK_IMAGE_ASPECT_COLOR_BIT, 1u,
                      1u, *storageImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      m_params.isRead ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT);

    vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

    vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);

    vkd.cmdPushConstants(commandBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                         static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);

    vkd.cmdDispatch(commandBuffer, numOfWorkgroups, 1u, 1u);

    if (m_params.isRead)
    {
        const VkBufferMemoryBarrier ioBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *ioBuffer, 0ull, texelSizeInBytes);

        vkd.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                               nullptr, 1u, &ioBufferBarrier, 0u, nullptr);
    }
    else
    {
        copyImageToBuffer(vkd, commandBuffer, *storageImage, *imageBuffer,
                          IVec2(imageViewSizeInTexels.x(), imageViewSizeInTexels.y()), VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    endCommandBuffer(vkd, commandBuffer);

    // Submit commands
    submitCommandsAndWait(vkd, device, queue, commandBuffer);

    // Retrieve data from output buffer to host memory
    const auto outputBufferAlloc = m_params.isRead ? ioBuffer.getAllocation() : imageBuffer.getAllocation();
    invalidateAlloc(vkd, device, outputBufferAlloc);

    if (m_params.isRobust)
    {
        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc.getHostPtr());

        if (m_params.isRead)
        {
            // Check that the OOB read at the OOB index reads 0
            const std::vector<uint8_t> refData(texelSizeInBytes, 0u);

            if (deMemCmp(de::dataOrNull(refData), outputData, texelSizeInBytes) != 0)
                return tcu::TestStatus::fail("Failed");
        }
        else
        {
            // Check that OOB write did not change anything inside the image
            const uint8_t *refData = &referenceData[0];

            if (deMemCmp(refData, outputData, imageSizeInBytes) != 0)
                return tcu::TestStatus::fail("Failed");
        }
    }

    return TestStatus::pass("Pass");
}

TestInstance *OOBImageTestCase::createInstance(Context &context) const
{
    return new OOBImageTestInstance(context, m_params, m_imageParams);
}

std::string getFormatShortString(const VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

TestCaseGroup *createOOBAccessTests(TestContext &testCtx)
{
    de::MovePtr<TestCaseGroup> OOBAccessTests(new TestCaseGroup(testCtx, "oob_access"));

    const std::string texelBufferName[]     = {"texel_buffer_uniform", "texel_buffer_storage"};
    const std::string oobAccessName[]       = {"off_by_one", "off"};
    const std::string robustnessLevelName[] = {"none", "rba", "rba2"};

    for (const auto &isRobust : {true, false})
    {
        const std::string grpName = std::string("robust_") + (isRobust ? "on" : "off");

        de::MovePtr<TestCaseGroup> robustTests(new TestCaseGroup(testCtx, grpName.c_str()));

        for (uint32_t accessTypeIdx = static_cast<uint32_t>(OOBAccessType::OFF_BY_ONE);
             accessTypeIdx <= static_cast<uint32_t>(OOBAccessType::OFF); accessTypeIdx++)
        {
            // Texel buffer tests
            for (uint32_t texBuffTypeIdx = static_cast<uint32_t>(TexelBufferType::TEXEL_BUFFER_UNIFORM);
                 texBuffTypeIdx <= static_cast<uint32_t>(TexelBufferType::TEXEL_BUFFER_STORAGE); texBuffTypeIdx++)
            {
                // OOB access on texel buffers without robustness is undefined behavior
                if (!isRobust)
                    continue;

                for (const auto &isRead : {true, false})
                {
                    if (!isRead && texBuffTypeIdx == static_cast<uint32_t>(TexelBufferType::TEXEL_BUFFER_UNIFORM))
                        continue;

                    for (const auto &format : {VK_FORMAT_R32_UINT, VK_FORMAT_R64_UINT})
                    {
                        for (const auto &size : {256u, 1024u, 4096u}) // In bytes
                        {
                            for (uint32_t robLevelIdx = static_cast<uint32_t>(RobustnessLevel::ROBUST_BUFFER_ACCESS);
                                 robLevelIdx <= static_cast<uint32_t>(RobustnessLevel::ROBUST_BUFFER_ACCESS2);
                                 robLevelIdx++)
                            {
                                const OOBCommonParams params = {
                                    format,                                   // VkFormat format;
                                    isRobust,                                 // bool isRobust;
                                    isRead,                                   // bool isRead;
                                    static_cast<OOBAccessType>(accessTypeIdx) // OOBAccessType oobAccess;
                                };

                                const OOBBufferParams buffParams = {
                                    static_cast<RobustnessLevel>(robLevelIdx),    // RobustnessLevel robustnessLevel;
                                    static_cast<TexelBufferType>(texBuffTypeIdx), // TexelBufferType bufferType;
                                    size                                          // uint32_t backingSize;
                                };

                                const std::string testName =
                                    robustnessLevelName[robLevelIdx] + std::string("_") +
                                    texelBufferName[texBuffTypeIdx] + std::string("_") + getFormatShortString(format) +
                                    std::string("_") + (isRead ? "read" : "write") + std::string("_") +
                                    (oobAccessName[accessTypeIdx]) + std::string("_") + de::toString(size);
                                robustTests->addChild(new OOBBufferTestCase(testCtx, testName, params, buffParams));
                            }
                        }
                    }
                }
            }

            // Storage image tests

            for (const auto &isRead : {true, false})
            {
                for (const auto &format : {VK_FORMAT_R32_UINT, VK_FORMAT_R64_UINT})
                {
                    for (const auto &extent : {UVec2(16u, 16u), UVec2(64u, 64u), UVec2(128u, 128u)})
                    {
                        const OOBCommonParams params = {
                            format,                                   // VkFormat format;
                            isRobust,                                 // bool isRobust;
                            isRead,                                   // bool isRead;
                            static_cast<OOBAccessType>(accessTypeIdx) // OOBAccessType oobAccess;
                        };

                        const OOBImageParams imageParams = {
                            extent // UVec2 imageExtent;
                        };

                        const std::string testName = "storage_image" + std::string("_") + getFormatShortString(format) +
                                                     std::string("_") + (isRead ? "read" : "write") + std::string("_") +
                                                     (oobAccessName[accessTypeIdx]) + std::string("_") +
                                                     de::toString(extent.x()) + "x" + de::toString(extent.y());
                        robustTests->addChild(new OOBImageTestCase(testCtx, testName, params, imageParams));
                    }
                }
            }
        }

        OOBAccessTests->addChild(robustTests.release());
    }

    return OOBAccessTests.release();
}

} // namespace robustness
} // namespace vkt
