/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Binary Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectBinaryTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"

#include <cmath>

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum QueryType
{
    SAME_SHADER,
    NEW_SHADER,
    SHADER_FROM_BINARY,
    NEW_DEVICE,
    DEVICE_NO_EXTS_FEATURES,
    ALL_FEATURE_COMBINATIONS,
};

struct TestParams
{
    vk::VkShaderStageFlagBits stage;
    bool linked;
    QueryType queryType;
};

enum IncompleteBinaryTestType
{
    HALF_DATA_SIZE,
    GARBAGE_DATA,
    GARBAGE_SECOND_HALF,
    CREATE_FROM_HALF_SIZE,
    CREATE_FROM_HALF_SIZE_GARBAGE,
};

vk::VkShaderStageFlags getNextStage(vk::VkShaderStageFlagBits shaderStage, bool tessellationShaderFeature,
                                    bool geometryShaderFeature)
{
    if (shaderStage == vk::VK_SHADER_STAGE_VERTEX_BIT)
    {
        if (tessellationShaderFeature)
            return vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        else if (geometryShaderFeature)
            return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
        return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        return vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    else if (shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
    {
        if (geometryShaderFeature)
            return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
        return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    else if (shaderStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
    {
        return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    return 0u;
}

vk::Move<vk::VkShaderEXT> createShader(const vk::DeviceInterface &vk, const vk::BinaryCollection &binaries,
                                       const vk::VkDevice device, vk::VkPhysicalDeviceFeatures features,
                                       vk::VkDescriptorSetLayout descriptorSetLayout, bool linked,
                                       vk::VkShaderStageFlagBits stage)
{
    vk::VkShaderEXT shader;

    if (!linked)
    {
        const auto &src                                  = binaries.get(vk::getShaderName(stage));
        const vk::VkShaderCreateInfoEXT shaderCreateInfo = {
            vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,                              // VkStructureType sType;
            nullptr,                                                                   // const void* pNext;
            0u,                                                                        // VkShaderCreateFlagsEXT flags;
            stage,                                                                     // VkShaderStageFlagBits stage;
            getNextStage(stage, features.tessellationShader, features.geometryShader), // VkShaderStageFlags nextStage;
            vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,                                         // VkShaderCodeTypeEXT codeType;
            src.getSize(),                                                             // size_t codeSize;
            src.getBinary(),                                                           // const void* pCode;
            "main",                                                                    // const char* pName;
            (descriptorSetLayout != VK_NULL_HANDLE) ? 1u : 0u,                         // uint32_t setLayoutCount;
            (descriptorSetLayout != VK_NULL_HANDLE) ? &descriptorSetLayout :
                                                      nullptr, // VkDescriptorSetLayout* pSetLayouts;
            0u,                                                // uint32_t pushConstantRangeCount;
            nullptr,                                           // const VkPushConstantRange* pPushConstantRanges;
            nullptr,                                           // const VkSpecializationInfo* pSpecializationInfo;
        };

        vk.createShadersEXT(device, 1u, &shaderCreateInfo, nullptr, &shader);
    }
    else
    {
        const auto &vert = binaries.get("vert");
        const auto &tesc = binaries.get("tesc");
        const auto &tese = binaries.get("tese");
        const auto &geom = binaries.get("geom");
        const auto &frag = binaries.get("frag");

        std::vector<vk::VkShaderCreateInfoEXT> shaderCreateInfos = {
            {
                vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,      // VkShaderCreateFlagsEXT flags;
                vk::VK_SHADER_STAGE_VERTEX_BIT,               // VkShaderStageFlagBits stage;
                getNextStage(vk::VK_SHADER_STAGE_VERTEX_BIT, features.tessellationShader,
                             features.geometryShader), // VkShaderStageFlags nextStage;
                vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,     // VkShaderCodeTypeEXT codeType;
                vert.getSize(),                        // size_t codeSize;
                vert.getBinary(),                      // const void* pCode;
                "main",                                // const char* pName;
                0u,                                    // uint32_t setLayoutCount;
                nullptr,                               // VkDescriptorSetLayout* pSetLayouts;
                0u,                                    // uint32_t pushConstantRangeCount;
                nullptr,                               // const VkPushConstantRange* pPushConstantRanges;
                nullptr,                               // const VkSpecializationInfo* pSpecializationInfo;
            },
            {
                vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,      // VkShaderCreateFlagsEXT flags;
                vk::VK_SHADER_STAGE_FRAGMENT_BIT,             // VkShaderStageFlagBits stage;
                getNextStage(vk::VK_SHADER_STAGE_FRAGMENT_BIT, features.tessellationShader,
                             features.geometryShader), // VkShaderStageFlags nextStage;
                vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,     // VkShaderCodeTypeEXT codeType;
                frag.getSize(),                        // size_t codeSize;
                frag.getBinary(),                      // const void* pCode;
                "main",                                // const char* pName;
                0u,                                    // uint32_t setLayoutCount;
                nullptr,                               // VkDescriptorSetLayout* pSetLayouts;
                0u,                                    // uint32_t pushConstantRangeCount;
                nullptr,                               // const VkPushConstantRange* pPushConstantRanges;
                nullptr,                               // const VkSpecializationInfo* pSpecializationInfo;
            },
        };
        if (features.tessellationShader)
        {
            shaderCreateInfos.push_back({
                vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,      // VkShaderCreateFlagsEXT flags;
                vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, // VkShaderStageFlagBits stage;
                getNextStage(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, features.tessellationShader,
                             features.geometryShader), // VkShaderStageFlags nextStage;
                vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,     // VkShaderCodeTypeEXT codeType;
                tesc.getSize(),                        // size_t codeSize;
                tesc.getBinary(),                      // const void* pCode;
                "main",                                // const char* pName;
                0u,                                    // uint32_t setLayoutCount;
                nullptr,                               // VkDescriptorSetLayout* pSetLayouts;
                0u,                                    // uint32_t pushConstantRangeCount;
                nullptr,                               // const VkPushConstantRange* pPushConstantRanges;
                nullptr,                               // const VkSpecializationInfo* pSpecializationInfo;
            });
            shaderCreateInfos.push_back({
                vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,    // VkStructureType sType;
                nullptr,                                         // const void* pNext;
                vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,         // VkShaderCreateFlagsEXT flags;
                vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, // VkShaderStageFlagBits stage;
                getNextStage(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, features.tessellationShader,
                             features.geometryShader), // VkShaderStageFlags nextStage;
                vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,     // VkShaderCodeTypeEXT codeType;
                tese.getSize(),                        // size_t codeSize;
                tese.getBinary(),                      // const void* pCode;
                "main",                                // const char* pName;
                0u,                                    // uint32_t setLayoutCount;
                nullptr,                               // VkDescriptorSetLayout* pSetLayouts;
                0u,                                    // uint32_t pushConstantRangeCount;
                nullptr,                               // const VkPushConstantRange* pPushConstantRanges;
                nullptr,                               // const VkSpecializationInfo* pSpecializationInfo;
            });
        }
        if (features.geometryShader)
        {
            shaderCreateInfos.push_back({
                vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,      // VkShaderCreateFlagsEXT flags;
                vk::VK_SHADER_STAGE_GEOMETRY_BIT,             // VkShaderStageFlagBits stage;
                getNextStage(vk::VK_SHADER_STAGE_GEOMETRY_BIT, features.tessellationShader,
                             features.geometryShader), // VkShaderStageFlags nextStage;
                vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,     // VkShaderCodeTypeEXT codeType;
                geom.getSize(),                        // size_t codeSize;
                geom.getBinary(),                      // const void* pCode;
                "main",                                // const char* pName;
                0u,                                    // uint32_t setLayoutCount;
                nullptr,                               // VkDescriptorSetLayout* pSetLayouts;
                0u,                                    // uint32_t pushConstantRangeCount;
                nullptr,                               // const VkPushConstantRange* pPushConstantRanges;
                nullptr,                               // const VkSpecializationInfo* pSpecializationInfo;
            });
        }
        std::vector<vk::VkShaderEXT> shaders(shaderCreateInfos.size());
        vk.createShadersEXT(device, (uint32_t)shaderCreateInfos.size(), &shaderCreateInfos[0], nullptr, &shaders[0]);

        for (uint32_t i = 0; i < (uint32_t)shaderCreateInfos.size(); ++i)
        {
            if (shaderCreateInfos[i].stage == stage)
            {
                shader = shaders[i];
            }
            else
            {
                vk.destroyShaderEXT(device, shaders[i], nullptr);
            }
        }
    }

    return vk::Move<vk::VkShaderEXT>(vk::check<vk::VkShaderEXT>(shader),
                                     vk::Deleter<vk::VkShaderEXT>(vk, device, nullptr));
}

class ShaderObjectBinaryQueryInstance : public vkt::TestInstance
{
public:
    ShaderObjectBinaryQueryInstance(Context &context, const TestParams params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~ShaderObjectBinaryQueryInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    TestParams m_params;
};

tcu::TestStatus ShaderObjectBinaryQueryInstance::iterate(void)
{
    const auto &vkp               = m_context.getPlatformInterface();
    const vk::VkInstance instance = m_context.getInstance();
    const vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::DeviceInterface &vk             = m_context.getDeviceInterface();
    const vk::VkDevice device                 = m_context.getDevice();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const bool tessellationSupported          = m_context.getDeviceFeatures().tessellationShader;
    const bool geometrySupported              = m_context.getDeviceFeatures().geometryShader;

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    vk::VkDescriptorSetLayout layout =
        (m_params.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT) ? *descriptorSetLayout : VK_NULL_HANDLE;

    const auto &binaries = m_context.getBinaryCollection();
    vk::Move<vk::VkShaderEXT> shader =
        createShader(vk, binaries, device, m_context.getDeviceFeatures(), layout, m_params.linked, m_params.stage);

    size_t dataSize = 0;
    vk.getShaderBinaryDataEXT(device, *shader, &dataSize, nullptr);
    std::vector<uint8_t> data(dataSize);
    vk.getShaderBinaryDataEXT(device, *shader, &dataSize, data.data());

    for (uint32_t i = 0; i < 10; ++i)
    {
        size_t otherDataSize = 0;
        std::vector<uint8_t> otherData;
        if (m_params.queryType == SAME_SHADER)
        {
            vk.getShaderBinaryDataEXT(device, *shader, &otherDataSize, nullptr);
            otherData.resize(otherDataSize);
            vk.getShaderBinaryDataEXT(device, *shader, &otherDataSize, otherData.data());
        }
        else if (m_params.queryType == NEW_SHADER)
        {
            vk::Move<vk::VkShaderEXT> otherShader = createShader(vk, binaries, device, m_context.getDeviceFeatures(),
                                                                 layout, m_params.linked, m_params.stage);
            vk.getShaderBinaryDataEXT(device, *otherShader, &otherDataSize, nullptr);
            otherData.resize(otherDataSize);
            vk.getShaderBinaryDataEXT(device, *otherShader, &otherDataSize, otherData.data());
        }
        else if (m_params.queryType == SHADER_FROM_BINARY)
        {
            vk::Move<vk::VkShaderEXT> otherShader = vk::createShaderFromBinary(
                vk, device, m_params.stage, dataSize, data.data(), tessellationSupported, geometrySupported, layout);
            vk.getShaderBinaryDataEXT(device, *otherShader, &otherDataSize, nullptr);
            otherData.resize(otherDataSize);
            vk.getShaderBinaryDataEXT(device, *otherShader, &otherDataSize, otherData.data());
        }
        else if (m_params.queryType == NEW_DEVICE || m_params.queryType == DEVICE_NO_EXTS_FEATURES)
        {
            vk::VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = vk::initVulkanStructure();
            shaderObjectFeatures.shaderObject                                = VK_TRUE;
            vk::VkPhysicalDeviceFeatures2 features2;
            std::vector<const char *> extensions;

            if (m_params.queryType == DEVICE_NO_EXTS_FEATURES)
            {
                features2                             = vk::initVulkanStructure(&shaderObjectFeatures);
                features2.features.tessellationShader = tessellationSupported;
                features2.features.geometryShader     = geometrySupported;
                extensions.push_back("VK_EXT_shader_object");
            }
            else
            {
                features2  = m_context.getDeviceFeatures2();
                extensions = m_context.getDeviceCreationExtensions();
            }
            const float queuePriority = 1.0f;

            const vk::VkDeviceQueueCreateInfo deviceQueueCI = {
                vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
                nullptr,                                        // pNext
                (vk::VkDeviceQueueCreateFlags)0u,               // flags
                queueFamilyIndex,                               // queueFamilyIndex;
                1,                                              // queueCount;
                &queuePriority,                                 // pQueuePriorities;
            };

            const vk::VkDeviceCreateInfo deviceCreateInfo = {
                vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // sType;
                &features2,                               // pNext;
                0u,                                       // flags
                1u,                                       // queueCreateInfoCount;
                &deviceQueueCI,                           // pQueueCreateInfos;
                0u,                                       // layerCount;
                nullptr,                                  // ppEnabledLayerNames;
                (uint32_t)extensions.size(),              // uint32_t enabledExtensionCount;
                extensions.data(),                        // const char* const* ppEnabledExtensionNames;
                nullptr,                                  // pEnabledFeatures;
            };

            vk::Move<vk::VkDevice> otherDevice =
                createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance,
                                   instanceDriver, physicalDevice, &deviceCreateInfo);

            const vk::Unique<vk::VkDescriptorSetLayout> otherDescriptorSetLayout(
                vk::DescriptorSetLayoutBuilder()
                    .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
                    .build(vk, *otherDevice));

            vk::VkDescriptorSetLayout otherLayout =
                (m_params.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT) ? *otherDescriptorSetLayout : VK_NULL_HANDLE;

            vk::Move<vk::VkShaderEXT> otherShader = createShader(vk, binaries, *otherDevice, features2.features,
                                                                 otherLayout, m_params.linked, m_params.stage);
            vk.getShaderBinaryDataEXT(*otherDevice, *otherShader, &otherDataSize, nullptr);
            otherData.resize(otherDataSize);
            vk.getShaderBinaryDataEXT(*otherDevice, *otherShader, &otherDataSize, otherData.data());
        }

        if (dataSize != otherDataSize)
            return tcu::TestStatus::fail("Size not matching");

        for (uint32_t j = 0; j < dataSize; ++j)
            if (data[j] != otherData[j])
                return tcu::TestStatus::fail("Data not matching");
    }

    return tcu::TestStatus::pass("Pass");
}

class ShaderObjectBinaryQueryCase : public vkt::TestCase
{
public:
    ShaderObjectBinaryQueryCase(tcu::TestContext &testCtx, const std::string &name, TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~ShaderObjectBinaryQueryCase(void)
    {
    }

    void checkSupport(vkt::Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ShaderObjectBinaryQueryInstance(context, m_params);
    }

private:
    TestParams m_params;
};

void ShaderObjectBinaryQueryCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
        m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
    if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectBinaryQueryCase::initPrograms(vk::SourceCollections &programCollection) const
{
    vk::addBasicShaderObjectShaders(programCollection);
}

class ShaderObjectIncompatibleBinaryInstance : public vkt::TestInstance
{
public:
    ShaderObjectIncompatibleBinaryInstance(Context &context, vk::VkShaderStageFlagBits shaderStage,
                                           const IncompleteBinaryTestType testType)
        : vkt::TestInstance(context)
        , m_shaderStage(shaderStage)
        , m_testType(testType)
    {
    }
    virtual ~ShaderObjectIncompatibleBinaryInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    const vk::VkShaderStageFlagBits m_shaderStage;
    const IncompleteBinaryTestType m_testType;
};

tcu::TestStatus ShaderObjectIncompatibleBinaryInstance::iterate(void)
{
    const vk::VkInstance instance = m_context.getInstance();
    const vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const vk::DeviceInterface &vk    = m_context.getDeviceInterface();
    const vk::VkDevice device        = m_context.getDevice();
    const bool tessellationSupported = m_context.getDeviceFeatures().tessellationShader;
    const bool geometrySupported     = m_context.getDeviceFeatures().geometryShader;

    const auto &binaries = m_context.getBinaryCollection();

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    vk::VkDescriptorSetLayout layout =
        (m_shaderStage == vk::VK_SHADER_STAGE_COMPUTE_BIT) ? *descriptorSetLayout : VK_NULL_HANDLE;

    const auto &src                                  = binaries.get(getShaderName(m_shaderStage));
    const vk::VkShaderCreateInfoEXT shaderCreateInfo = {
        vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        0u,                                           // VkShaderCreateFlagsEXT flags;
        m_shaderStage,                                // VkShaderStageFlagBits stage;
        vk::getShaderObjectNextStages(m_shaderStage, tessellationSupported,
                                      geometrySupported), // VkShaderStageFlags nextStage;
        vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,                // VkShaderCodeTypeEXT codeType;
        src.getSize(),                                    // size_t codeSize;
        src.getBinary(),                                  // const void* pCode;
        "main",                                           // const char* pName;
        (layout != VK_NULL_HANDLE) ? 1u : 0u,             // uint32_t setLayoutCount;
        (layout != VK_NULL_HANDLE) ? &layout : nullptr,   // VkDescriptorSetLayout* pSetLayouts;
        0u,                                               // uint32_t pushConstantRangeCount;
        nullptr,                                          // const VkPushConstantRange* pPushConstantRanges;
        nullptr,                                          // const VkSpecializationInfo* pSpecializationInfo;
    };

    vk::Move<vk::VkShaderEXT> shader = vk::createShader(vk, device, shaderCreateInfo);
    size_t dataSize                  = 0;
    vk.getShaderBinaryDataEXT(device, *shader, &dataSize, nullptr);
    std::vector<uint8_t> data(dataSize, 123);

    if (m_testType == HALF_DATA_SIZE)
    {
        dataSize /= 2;
        vk::VkResult result = vk.getShaderBinaryDataEXT(device, *shader, &dataSize, data.data());

        if (result != vk::VK_INCOMPLETE)
            return tcu::TestStatus::fail("Result was not VK_INCOMPLETE");

        for (const auto &byte : data)
            if (byte != 123)
                return tcu::TestStatus::fail("Data was modified");

        if (dataSize != 0)
            return tcu::TestStatus::fail("Data size was not 0");
    }
    else
    {
        de::Random random(102030);
        // Generate random garbage data
        if (m_testType != CREATE_FROM_HALF_SIZE)
        {
            uint32_t i = m_testType == GARBAGE_DATA ? 0u : (uint32_t)dataSize / 2u;
            for (; i < dataSize; ++i)
                data[i] = random.getUint8();
        }

        if (m_testType == CREATE_FROM_HALF_SIZE || m_testType == CREATE_FROM_HALF_SIZE_GARBAGE)
            dataSize /= 2;

        const vk::VkShaderCreateInfoEXT invalidShaderCreateInfo = {
            vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            0u,                                           // VkShaderCreateFlagsEXT flags;
            m_shaderStage,                                // VkShaderStageFlagBits stage;
            vk::getShaderObjectNextStages(m_shaderStage, tessellationSupported,
                                          geometrySupported), // VkShaderStageFlags nextStage;
            vk::VK_SHADER_CODE_TYPE_BINARY_EXT,               // VkShaderCodeTypeEXT codeType;
            dataSize,                                         // size_t codeSize;
            data.data(),                                      // const void* pCode;
            "main",                                           // const char* pName;
            (layout != VK_NULL_HANDLE) ? 1u : 0u,             // uint32_t setLayoutCount;
            (layout != VK_NULL_HANDLE) ? &layout : nullptr,   // VkDescriptorSetLayout* pSetLayouts;
            0u,                                               // uint32_t pushConstantRangeCount;
            nullptr,                                          // const VkPushConstantRange* pPushConstantRanges;
            nullptr,                                          // const VkSpecializationInfo* pSpecializationInfo;
        };

        vk::VkShaderEXT dstShader;
        vk::VkResult result = vk.createShadersEXT(device, 1u, &invalidShaderCreateInfo, nullptr, &dstShader);

        if (result != vk::VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT)
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class ShaderObjectIncompatibleBinaryCase : public vkt::TestCase
{
public:
    ShaderObjectIncompatibleBinaryCase(tcu::TestContext &testCtx, const std::string &name,
                                       const vk::VkShaderStageFlagBits shaderStage,
                                       const IncompleteBinaryTestType testType)
        : vkt::TestCase(testCtx, name)
        , m_shaderStage(shaderStage)
        , m_testType(testType)
    {
    }
    virtual ~ShaderObjectIncompatibleBinaryCase(void)
    {
    }

    void checkSupport(vkt::Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ShaderObjectIncompatibleBinaryInstance(context, m_shaderStage, m_testType);
    }

private:
    const vk::VkShaderStageFlagBits m_shaderStage;
    const IncompleteBinaryTestType m_testType;
};

void ShaderObjectIncompatibleBinaryCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
        m_shaderStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
    if (m_shaderStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectIncompatibleBinaryCase::initPrograms(vk::SourceCollections &programCollection) const
{
    vk::addBasicShaderObjectShaders(programCollection);
}

class ShaderObjectDeviceFeaturesBinaryInstance : public vkt::TestInstance
{
public:
    ShaderObjectDeviceFeaturesBinaryInstance(Context &context, const bool linked, const vk::VkShaderStageFlagBits stage,
                                             const uint32_t index)
        : vkt::TestInstance(context)
        , m_linked(linked)
        , m_stage(stage)
        , m_index(index)
    {
    }
    virtual ~ShaderObjectDeviceFeaturesBinaryInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

private:
    const bool m_linked;
    const vk::VkShaderStageFlagBits m_stage;
    const uint32_t m_index;
};

const void *findPNext(const void *pNext, vk::VkStructureType sType)
{
    while (pNext != nullptr)
    {
        if (((vk::VkBaseOutStructure *)pNext)->sType == sType)
            return (const void *)pNext;
        pNext = ((vk::VkBaseOutStructure *)pNext)->pNext;
    }
    return nullptr;
}

tcu::TestStatus ShaderObjectDeviceFeaturesBinaryInstance::iterate(void)
{
    const auto &vkp               = m_context.getPlatformInterface();
    const vk::VkInstance instance = m_context.getInstance();
    const vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::DeviceInterface &vk             = m_context.getDeviceInterface();
    const vk::VkDevice device                 = m_context.getDevice();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const auto &binaries                      = m_context.getBinaryCollection();

    const vk::VkPhysicalDeviceFeatures features = m_context.getDeviceFeatures();

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    vk::VkDescriptorSetLayout layout =
        (m_stage == vk::VK_SHADER_STAGE_COMPUTE_BIT) ? *descriptorSetLayout : VK_NULL_HANDLE;

    vk::Move<vk::VkShaderEXT> shader = createShader(vk, binaries, device, features, layout, m_linked, m_stage);

    size_t dataSize = 0;
    vk.getShaderBinaryDataEXT(device, *shader, &dataSize, nullptr);
    std::vector<uint8_t> data(dataSize);
    vk.getShaderBinaryDataEXT(device, *shader, &dataSize, data.data());

    size_t otherDataSize = 0;
    std::vector<uint8_t> otherData;

    const vk::VkPhysicalDeviceFeatures2 features2 = m_context.getDeviceFeatures2();
    vk::VkPhysicalDeviceFeatures2 testFeatures    = m_context.getDeviceFeatures2();
    auto shaderObjectFeatures                     = m_context.getShaderObjectFeaturesEXT();
    std::vector<const char *> extensions          = m_context.getDeviceCreationExtensions();

#include "vkDeviceFeaturesForShaderObject.inl"

    // These features depend on other features being enabled
    fMeshShaderFeaturesEXT.multiviewMeshShader                    = VK_FALSE;
    fMeshShaderFeaturesEXT.primitiveFragmentShadingRateMeshShader = VK_FALSE;

    const float queuePriority = 1.0f;

    const vk::VkDeviceQueueCreateInfo deviceQueueCI = {
        vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                        // pNext
        (vk::VkDeviceQueueCreateFlags)0u,               // flags
        queueFamilyIndex,                               // queueFamilyIndex;
        1,                                              // queueCount;
        &queuePriority,                                 // pQueuePriorities;
    };

    const vk::VkDeviceCreateInfo deviceCreateInfo = {
        vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // sType;
        &testFeatures,                            // pNext;
        0u,                                       // flags
        1u,                                       // queueCreateInfoCount;
        &deviceQueueCI,                           // pQueueCreateInfos;
        0u,                                       // layerCount;
        nullptr,                                  // ppEnabledLayerNames;
        (uint32_t)extensions.size(),              // uint32_t enabledExtensionCount;
        extensions.data(),                        // const char* const* ppEnabledExtensionNames;
        nullptr,                                  // pEnabledFeatures;
    };

    const uint32_t coreFeaturesCount  = 50u;
    const uint32_t pNextFeaturesCount = (uint32_t)pNextFeatures.size();

    // There are too many features to test every combination, so we group them by step = 10
    const uint32_t step1          = 10u;
    const uint32_t step2          = 30u;
    const uint32_t count2         = uint32_t(std::pow(2u, pNextFeaturesCount / step2));
    vk::VkBool32 *coreFeaturesPtr = &testFeatures.features.robustBufferAccess;

    for (uint32_t i = 0; i < count2; ++i)
    {
        // Reset features
        testFeatures.features = features2.features;
        void *pNext           = nullptr;
        for (uint32_t j = 0; j < coreFeaturesCount; ++j)
        {
            if (((m_index >> (j / step1)) & 1) == 0)
                coreFeaturesPtr[j] = VK_FALSE;
        }

        for (uint32_t j = 0; j < pNextFeaturesCount; ++j)
        {
            if (((i >> (j / step2)) & 1) == 1)
            {
                if (findPNext(features2.pNext, ((vk::VkBaseOutStructure *)pNextFeatures[j])->sType))
                {
                    ((vk::VkBaseOutStructure *)pNextFeatures[j])->pNext = (vk::VkBaseOutStructure *)pNext;
                    pNext                                               = pNextFeatures[j];
                }
            }
        }

        shaderObjectFeatures.pNext = pNext;
        testFeatures.pNext         = &shaderObjectFeatures;
        // Geometry and tessellation features must not be modified
        testFeatures.features.tessellationShader = features.tessellationShader;
        testFeatures.features.geometryShader     = features.geometryShader;

        vk::Move<vk::VkDevice> otherDevice =
            createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance,
                               instanceDriver, physicalDevice, &deviceCreateInfo);

        const vk::Unique<vk::VkDescriptorSetLayout> otherDescriptorSetLayout(
            vk::DescriptorSetLayoutBuilder()
                .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
                .build(vk, *otherDevice));

        vk::VkDescriptorSetLayout otherLayout =
            (m_stage == vk::VK_SHADER_STAGE_COMPUTE_BIT) ? *otherDescriptorSetLayout : VK_NULL_HANDLE;

        vk::Move<vk::VkShaderEXT> otherShader =
            createShader(vk, binaries, *otherDevice, features, otherLayout, m_linked, m_stage);
        vk.getShaderBinaryDataEXT(*otherDevice, *otherShader, &otherDataSize, nullptr);
        otherData.resize(otherDataSize);
        vk.getShaderBinaryDataEXT(*otherDevice, *otherShader, &otherDataSize, otherData.data());

        if (dataSize != otherDataSize)
            return tcu::TestStatus::fail("Size not matching");

        for (uint32_t j = 0; j < dataSize; ++j)
            if (data[j] != otherData[j])
                return tcu::TestStatus::fail("Data not matching");
    }

    return tcu::TestStatus::pass("Pass");
}

class ShaderObjectDeviceFeaturesBinaryCase : public vkt::TestCase
{
public:
    ShaderObjectDeviceFeaturesBinaryCase(tcu::TestContext &testCtx, const std::string &name, const bool linked,
                                         const vk::VkShaderStageFlagBits stage, const uint32_t index)
        : vkt::TestCase(testCtx, name)
        , m_linked(linked)
        , m_stage(stage)
        , m_index(index)
    {
    }
    virtual ~ShaderObjectDeviceFeaturesBinaryCase(void)
    {
    }

    void checkSupport(vkt::Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ShaderObjectDeviceFeaturesBinaryInstance(context, m_linked, m_stage, m_index);
    }

private:
    const bool m_linked;
    const vk::VkShaderStageFlagBits m_stage;
    const uint32_t m_index;
};

void ShaderObjectDeviceFeaturesBinaryCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");

    if (m_stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
        m_stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
    if (m_stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectDeviceFeaturesBinaryCase::initPrograms(vk::SourceCollections &programCollection) const
{
    vk::addBasicShaderObjectShaders(programCollection);
}

std::string getName(QueryType queryType)
{
    switch (queryType)
    {
    case SAME_SHADER:
        return "same_shader";
        break;
    case NEW_SHADER:
        return "new_shader";
        break;
    case SHADER_FROM_BINARY:
        return "shader_from_binary";
        break;
    case NEW_DEVICE:
        return "new_device";
    case DEVICE_NO_EXTS_FEATURES:
        return "device_no_exts_features";
    default:
        DE_ASSERT(0);
        break;
    }
    return {};
}

} // namespace

tcu::TestCaseGroup *createShaderObjectBinaryTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> binaryGroup(new tcu::TestCaseGroup(testCtx, "binary"));

    const struct
    {
        vk::VkShaderStageFlagBits stage;
        const char *name;
    } stageTests[] = {
        {vk::VK_SHADER_STAGE_VERTEX_BIT, "vert"},
        {vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc"},
        {vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese"},
        {vk::VK_SHADER_STAGE_GEOMETRY_BIT, "geom"},
        {vk::VK_SHADER_STAGE_FRAGMENT_BIT, "frag"},
        {vk::VK_SHADER_STAGE_COMPUTE_BIT, "comp"},
    };

    const bool linkedTests[] = {
        false,
        true,
    };

    const QueryType queryTypeTests[] = {
        SAME_SHADER, NEW_SHADER, SHADER_FROM_BINARY, NEW_DEVICE, DEVICE_NO_EXTS_FEATURES,
    };

    de::MovePtr<tcu::TestCaseGroup> queryGroup(new tcu::TestCaseGroup(testCtx, "query"));
    for (const auto &stage : stageTests)
    {
        de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stage.name));
        for (const auto &linked : linkedTests)
        {
            if (linked && stage.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)
                continue;

            std::string linkedName = linked ? "linked" : "unlinked";
            de::MovePtr<tcu::TestCaseGroup> linkedGroup(new tcu::TestCaseGroup(testCtx, linkedName.c_str()));
            for (const auto &queryType : queryTypeTests)
            {
                TestParams params = {
                    stage.stage,
                    linked,
                    queryType,
                };
                linkedGroup->addChild(new ShaderObjectBinaryQueryCase(testCtx, getName(queryType), params));
            }
            stageGroup->addChild(linkedGroup.release());
        }
        queryGroup->addChild(stageGroup.release());
    }

    const struct
    {
        IncompleteBinaryTestType type;
        const char *name;
    } incompatibleTests[] = {
        {
            HALF_DATA_SIZE,
            "half_size",
        },
        {
            GARBAGE_DATA,
            "garbage_data",
        },
        {
            GARBAGE_SECOND_HALF,
            "garbage_second_half",
        },
        {CREATE_FROM_HALF_SIZE, "create_from_half_size"},
        {CREATE_FROM_HALF_SIZE_GARBAGE, "create_from_half_size_garbage"},
    };

    de::MovePtr<tcu::TestCaseGroup> incompatibleGroup(new tcu::TestCaseGroup(testCtx, "incompatible"));
    for (const auto &stage : stageTests)
    {
        de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stage.name));
        for (const auto &testType : incompatibleTests)
        {
            stageGroup->addChild(
                new ShaderObjectIncompatibleBinaryCase(testCtx, testType.name, stage.stage, testType.type));
        }
        incompatibleGroup->addChild(stageGroup.release());
    }

    de::MovePtr<tcu::TestCaseGroup> deviceFeaturesGroup(new tcu::TestCaseGroup(testCtx, "device_features"));
    for (const auto &stage : stageTests)
    {
        de::MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stage.name));
        for (const auto &linked : linkedTests)
        {
            if (linked && stage.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)
                continue;

            std::string linkedName = linked ? "linked" : "unlinked";
            de::MovePtr<tcu::TestCaseGroup> linkedGroup(new tcu::TestCaseGroup(testCtx, linkedName.c_str()));
            for (uint32_t i = 0; i < 32; ++i)
            {
                linkedGroup->addChild(
                    new ShaderObjectDeviceFeaturesBinaryCase(testCtx, std::to_string(i), linked, stage.stage, i));
            }
            stageGroup->addChild(linkedGroup.release());
        }
        deviceFeaturesGroup->addChild(stageGroup.release());
    }

    binaryGroup->addChild(queryGroup.release());
    binaryGroup->addChild(incompatibleGroup.release());
    binaryGroup->addChild(deviceFeaturesGroup.release());

    return binaryGroup.release();
}

} // namespace ShaderObject
} // namespace vkt
