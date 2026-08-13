/*------------------------------------------------------------------------
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
* \brief VK_KHR_device_fault extension tests.
*//*--------------------------------------------------------------------*/

#include "vktPostmortemCoreDeviceFaultTests.hpp"

#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"

#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "tcuCommandLine.hpp"
#include "vktTestCaseUtil.hpp"

#include <cassert>
#include <future>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace vkt
{
namespace postmortem
{
namespace
{

using namespace vk;

static const uint64_t SHORT_FAULT_REPORT_WAIT = 1;             // 1ns
static const uint64_t LONG_FAULT_REPORT_WAIT  = 1000000000ull; // 1s
static const uint32_t DUMMY_VALUE             = 0xBADF47E1;

enum TestFlagBits
{
    USE_NONE             = 0,
    USE_SHADER_ABORT     = 1 << 0,
    USE_FAULT_BINARY     = 1 << 1,
    USE_DEVICE_WAIT_IDLE = 1 << 2,
};

using TestFlags = uint32_t;

static std::vector<std::string> getRequiredExtensions(const bool useValidation, const uint32_t instanceVersion)
{
    std::vector<std::string> instanceExtensions;

    if (!vk::isCoreInstanceExtension(instanceVersion, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    if (useValidation && !vk::isCoreInstanceExtension(instanceVersion, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return instanceExtensions;
}

static void initDeviceFaultReportStructs(std::vector<VkDeviceFaultInfoKHR> &faultReports)
{
    for (auto &report : faultReports)
    {
        report.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_KHR;
        report.pNext = nullptr;
        strncpy(report.description, "Empty initialized data by test", 32);
    }
}

static VkPhysicalDeviceFaultFeaturesKHR getPhysicalDeviceFaultFeatures(Context &ctx)
{
    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures = {};
    deviceFaultFeatures.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR;
    deviceFaultFeatures.pNext                            = nullptr;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();
    deviceFeatures2.pNext                     = &deviceFaultFeatures;

    ctx.getInstanceInterface().getPhysicalDeviceFeatures2(ctx.getPhysicalDevice(), &deviceFeatures2);

    return deviceFaultFeatures;
}

static VkPhysicalDeviceFaultPropertiesKHR getPhysicalDeviceFaultProperties(Context &ctx)
{
    VkPhysicalDeviceFaultPropertiesKHR deviceFaultProperties = {};
    deviceFaultProperties.sType                              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_PROPERTIES_KHR;
    deviceFaultProperties.pNext                              = nullptr;

    VkPhysicalDeviceProperties2 deviceProperties2 = initVulkanStructure();
    deviceProperties2.pNext                       = &deviceFaultProperties;

    ctx.getInstanceInterface().getPhysicalDeviceProperties2(ctx.getPhysicalDevice(), &deviceProperties2);

    return deviceFaultProperties;
}

static VkPhysicalDeviceShaderAbortFeaturesKHR getPhysicalDeviceShaderAbortFeatures(Context &ctx)
{
    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures = {};
    shaderAbortFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ABORT_FEATURES_KHR;
    shaderAbortFeatures.pNext = nullptr;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();
    deviceFeatures2.pNext                     = &shaderAbortFeatures;

    ctx.getInstanceInterface().getPhysicalDeviceFeatures2(ctx.getPhysicalDevice(), &deviceFeatures2);

    return shaderAbortFeatures;
}

static uint32_t convertNsToMs(uint64_t nanoseconds)
{
    std::chrono::nanoseconds ns(nanoseconds);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(ns).count();

    return static_cast<uint32_t>(ms);
}

class ContextWrapper
{
public:
    ContextWrapper(Context &ctx, TestFlags testOptions)
        : m_context(ctx)
        , m_testOptions(testOptions)
        , m_faultFeatures(getPhysicalDeviceFaultFeatures(ctx))
        , m_faultProperties(getPhysicalDeviceFaultProperties(ctx))
        , m_shaderAbortFeatures(getPhysicalDeviceShaderAbortFeatures(ctx))
        , m_instance(ctx)
    {
        (void)m_testOptions;
        createDeviceFaultDevice(testOptions);
        logDeviceFaultConfig();
    }

    [[nodiscard]] VkResult getDeviceFaultReportsKHR(uint64_t timeout, uint32_t *pFaultCounts,
                                                    VkDeviceFaultInfoKHR *pFaultInfo) const
    {
        return m_customDevice.getDriver().getDeviceFaultReportsKHR(*m_customDevice, timeout, pFaultCounts, pFaultInfo);
    }

    [[nodiscard]] VkResult getDeviceFaultDebugInfoKHR(VkDeviceFaultDebugInfoKHR *pDebugInfo) const
    {
        return m_customDevice.getDriver().getDeviceFaultDebugInfoKHR(*m_customDevice, pDebugInfo);
    }

    const VkPhysicalDeviceFaultFeaturesKHR &faultFeatures() const
    {
        return m_faultFeatures;
    }

    const VkPhysicalDeviceFaultPropertiesKHR &faultProperties() const
    {
        return m_faultProperties;
    }

    const VkPhysicalDeviceShaderAbortFeaturesKHR &shaderAbortFeatures() const
    {
        return m_shaderAbortFeatures;
    }

    void logText(const std::string &msg) const;
    void logDeviceFaultConfig() const;
    void logDeviceFaultReport(const std::vector<VkDeviceFaultInfoKHR> &faultReports) const;
    void logDeviceFaultVendorBinaryHeader(const std::vector<uint8_t> &vendorBinaryData) const;

    VkResult submitShaderAbortDeviceFault() const;

private:
    void createDeviceFaultDevice(TestFlags testOptions);

    Context &m_context;
    TestFlags m_testOptions;
    VkPhysicalDeviceFaultFeaturesKHR m_faultFeatures;
    VkPhysicalDeviceFaultPropertiesKHR m_faultProperties;
    VkPhysicalDeviceShaderAbortFeaturesKHR m_shaderAbortFeatures;

    InstanceWrapper m_instance;
    DeviceWrapper m_customDevice;
};

void ContextWrapper::createDeviceFaultDevice(TestFlags testOptions)
{
    const bool useValidation = m_context.getTestContext().getCommandLine().isValidationEnabled();

    m_instance = createCustomInstanceWithExtensions(
        m_context, getRequiredExtensions(useValidation, m_context.getUsedApiVersion()));

    const VkPhysicalDevice physicalDevice = m_instance.getPhysicalDevice();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();
    const float queuePriority             = 1.0f;

    const VkDeviceQueueCreateInfo queueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0,                                          // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures = m_faultFeatures;
    deviceFaultFeatures.pNext                            = nullptr;

    VkPhysicalDeviceFeatures2 deviceFeatures2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // VkStructureType sType;
        &deviceFaultFeatures,                         // void* pNext;
        {}                                            // VkPhysicalDeviceFeatures features;
    };

    std::vector<const char *> deviceExtensions = {
        VK_KHR_DEVICE_FAULT_EXTENSION_NAME,
    };

    VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures;
    shaderAbortFeatures.sType       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ABORT_FEATURES_KHR;
    shaderAbortFeatures.pNext       = nullptr;
    shaderAbortFeatures.shaderAbort = VK_TRUE;

    if (testOptions & USE_SHADER_ABORT)
    {
        deviceExtensions.push_back(VK_KHR_SHADER_ABORT_EXTENSION_NAME);
        deviceFaultFeatures.pNext = &shaderAbortFeatures;
    }

    const VkDeviceCreateInfo deviceCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           // VkStructureType sType;
        &deviceFeatures2,                               // const void* pNext;
        0u,                                             // VkDeviceCreateFlags flags;
        1,                                              // uint32_t queueCreateInfoCount;
        &queueCreateInfo,                               // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                             // uint32_t enabledLayerCount;
        nullptr,                                        // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(deviceExtensions.size()), // uint32_t enabledExtensionCount;
        deviceExtensions.data(),                        // const char* const* ppEnabledExtensionNames;
        nullptr                                         // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_customDevice = m_instance.createCustomDevice(physicalDevice, &deviceCreateInfo);
}

void ContextWrapper::logText(const std::string &msg) const
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Section("deviceFaultConfig", "Device Fault configuration used") << tcu::TestLog::Message << msg
        << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;
}

void ContextWrapper::logDeviceFaultConfig() const
{
    // Log out the device fault properties and features that were returned by the driver
    // and were used during testing.
    tcu::TestLog &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Section("deviceFaultConfig", "Device Fault configuration used") << tcu::TestLog::Message
        << m_faultFeatures << "\n"
        << m_faultProperties << "\n"
        << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;
}

void ContextWrapper::logDeviceFaultReport(const std::vector<VkDeviceFaultInfoKHR> &faultReports) const
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Section("deviceFaultInfoList", "Returned device fault info structures");

    if (faultReports.size() > 0)
    {
        const char *nl = "\n";
        for (size_t idx = 0; idx < faultReports.size(); idx++)
        {
            auto &report = faultReports[idx];

            auto msg = log << tcu::TestLog::Message;
            msg << "Device fault info idx #" << std::to_string(idx) << ":" << nl << report << nl
                << tcu::TestLog::EndMessage;
        }
    }
    else
    {
        log << tcu::TestLog::Message << "No Device fault info structues returned" << tcu::TestLog::EndMessage;
    }

    log << tcu::TestLog::EndSection;
}

void ContextWrapper::logDeviceFaultVendorBinaryHeader(const std::vector<uint8_t> &vendorBinaryData) const
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    log << tcu::TestLog::Section("vendorBinaryData", "");
    auto msg = log << tcu::TestLog::Message;

    if (vendorBinaryData.size() > 0)
    {
        DE_ASSERT(vendorBinaryData.size() >= sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneKHR));

        auto *pHeader = reinterpret_cast<VkDeviceFaultVendorBinaryHeaderVersionOneKHR const *>(vendorBinaryData.data());

        msg << *pHeader;
    }
    else
    {
        msg << "No vendor binary returned";
    }

    log << tcu::TestLog::EndSection;
}

VkResult ContextWrapper::submitShaderAbortDeviceFault() const
{
    const VkPhysicalDevice physicalDevice      = m_context.getPhysicalDevice();
    const InstanceInterface &instanceInterface = m_context.getInstanceInterface();

    uint32_t queueFamilyIndex(0);
    vk::VkQueue queue(getDeviceQueue(m_customDevice.getDriver(), *m_customDevice, queueFamilyIndex, 0));
    vk::SimpleAllocator allocator(m_customDevice.getDriver(), *m_customDevice,
                                  getPhysicalDeviceMemoryProperties(instanceInterface, physicalDevice));

    // create output buffer
    const auto outBufferInfo =
        makeBufferCreateInfo(sizeof(uint32_t), (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    de::MovePtr<BufferWithMemory> outBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        m_customDevice.getDriver(), *m_customDevice, allocator, outBufferInfo, MemoryRequirement::HostVisible));

    // create descriptor set layout
    auto descriptorSetLayout = DescriptorSetLayoutBuilder()
                                   .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                   .build(m_customDevice.getDriver(), *m_customDevice);

    // create descriptor pool
    auto descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(m_customDevice.getDriver(), *m_customDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // create and update descriptor set
    const VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                *descriptorPool, 1u, &(*descriptorSetLayout)};
    auto descriptorSet = allocateDescriptorSet(m_customDevice.getDriver(), *m_customDevice, &allocInfo);
    const VkDescriptorBufferInfo descriptorInfo =
        makeDescriptorBufferInfo(**outBuffer, (VkDeviceSize)0u, sizeof(uint32_t));
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(m_customDevice.getDriver(), *m_customDevice);

    // create compute pipeline
    const Unique<VkShaderModule> shaderModule(createShaderModule(m_customDevice.getDriver(), *m_customDevice,
                                                                 m_context.getBinaryCollection().get("comp"), 0u));
    const Unique<VkPipelineLayout> pipelineLayout(
        makePipelineLayout(m_customDevice.getDriver(), *m_customDevice, 1u, &(*descriptorSetLayout), 0, nullptr));
    const Unique<VkPipeline> pipeline(
        makeComputePipeline(m_customDevice.getDriver(), *m_customDevice, *pipelineLayout, *shaderModule));

    // create command buffer
    const Unique<VkCommandPool> cmdPool(makeCommandPool(m_customDevice.getDriver(), *m_customDevice, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(m_customDevice.getDriver(), *m_customDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(m_customDevice.getDriver(), *cmdBuffer, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    m_customDevice.getDriver().cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    m_customDevice.getDriver().cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1,
                                                     &descriptorSet.get(), 0, 0);
    m_customDevice.getDriver().cmdDispatch(*cmdBuffer, 1, 1, 1);

    endCommandBuffer(m_customDevice.getDriver(), *cmdBuffer);

    const VkSubmitInfo submitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        nullptr,                       // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &*cmdBuffer,                   // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    const auto fence = createFence(m_customDevice.getDriver(), *m_customDevice);
    VK_CHECK(m_customDevice.getDriver().queueSubmit(queue, 1u, &submitInfo, *fence));
    m_customDevice.getDriver().waitForFences(*m_customDevice, 1, &*fence, VK_TRUE, (~0ull));

    if ((m_testOptions & USE_DEVICE_WAIT_IDLE) != 0)
        return m_customDevice.getDriver().deviceWaitIdle(*m_customDevice);
    else
        return VK_ERROR_DEVICE_LOST;
}

void checkDeviceFaultReportSupport(Context &context, TestFlags testOptions)
{
    context.requireInstanceFunctionality(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    context.requireDeviceFunctionality(VK_KHR_DEVICE_FAULT_EXTENSION_NAME);

    VkPhysicalDeviceFaultFeaturesKHR deviceFaultFeatures = getPhysicalDeviceFaultFeatures(context);

    if (VK_FALSE == deviceFaultFeatures.deviceFault)
    {
        TCU_THROW(NotSupportedError, "VK_KHR_device_fault extension exists but device fault feature is not available");
    }

    if (testOptions & USE_FAULT_BINARY)
    {
        // If the test case requires vendor binary then it must be available/enabled
        if (VK_FALSE == deviceFaultFeatures.deviceFaultVendorBinary)
            TCU_THROW(NotSupportedError, "VK_KHR_device_fault extension exists but vendor binary is not available");
    }

    if (testOptions & USE_SHADER_ABORT)
    {
        context.requireDeviceFunctionality(VK_KHR_SHADER_ABORT_EXTENSION_NAME);

        VkPhysicalDeviceShaderAbortFeaturesKHR shaderAbortFeatures = getPhysicalDeviceShaderAbortFeatures(context);

        if (VK_FALSE == shaderAbortFeatures.shaderAbort)
        {
            TCU_THROW(NotSupportedError,
                      "VK_KHR_shader_abort extension exists but shader abort feature is not available");
        }
    }
}

void initProgram(SourceCollections &programCollection, TestFlags testOptions)
{
    // If the shader abort is not used by the test ignore the shader
    if ((testOptions & USE_SHADER_ABORT) == 0)
        return;

    programCollection.glslSources.add("comp")
        << glu::ComputeSource("#version 450\n"
                              "#extension GL_EXT_abort : require\n"
                              "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1)\n"
                              "layout(std430, set = 0, binding = 0) writeonly buffer Data { uint outp[]; } data;\n"
                              "void main()\n"
                              "{\n"
                              "  data.outp[0] = 1; \n"
                              "  abortEXT(\"Manually producing device faults!\");\n"
                              "}\n");
}

// Tests per functions

tcu::TestStatus deviceFaultMaxFaultCountAtleastOne(Context &context, TestFlags testFlags)
{
    // Basic test: with shader execution, no device lost, just query if the
    // device returns a sane number for maxDeviceFaultCount
    const ContextWrapper contextWrapper(context, testFlags);

    VkPhysicalDeviceFaultPropertiesKHR faultProperties = contextWrapper.faultProperties();
    if (faultProperties.maxDeviceFaultCount < 1)
    {
        return tcu::TestStatus::fail(
            "maxDeviceFaultCount returned in VkPhysicalDeviceFaultPropertiesKHR must be greater than or equal to 1");
    }

    return tcu::TestStatus::pass("At least a single a single device fault report is supported");
}

tcu::TestStatus deviceFaultReportCountQueryCase(Context &context, TestFlags testOptions)
{
    // Basic test: with no shader execution, no device lost, query if
    // the device fault returns a timeout and zeroes the faultCount with zero timeout.
    const ContextWrapper contextWrapper(context, testOptions);

    uint32_t faultCounts = DUMMY_VALUE;

    // with zero timeot it should report VK_TIMEOUT
    VkResult result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, nullptr);
    if (VK_TIMEOUT != result)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR should return VK_TIMEOUT if there are no reported "
                                     "faults even if the timeout is 0");

    if (faultCounts == DUMMY_VALUE)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultReportsKHR should write faultCounts even if there are no faults to report");

    // Even if there was a VK_TIMEOUT (with timeout==0) it should at least report zero faults
    // as the device was idle.
    if (faultCounts != 0)
        return tcu::TestStatus::fail(
            "with no device lost vkGetDeviceFaultReportsKHR(timeout=0) did not set the faultCounts to 0");

    return tcu::TestStatus::pass(
        "with no device lost vkGetDeviceFaultReportsKHR(timeout=0) returned VK_TIMEOUT and faultCount was set to 0");
}

tcu::TestStatus deviceFaultReportQueryVeryShortTimeoutCase(Context &context, TestFlags testOptions)
{
    // Basic test: with no shader execution, no device lost, query if
    // the device fault returns a timeout and zeroes the faultCount with a bit of timeout.
    const ContextWrapper contextWrapper(context, testOptions);

    uint32_t faultCounts = DUMMY_VALUE;

    // With a very small timeout it should report VK_TIMEOUT as there was nothing executed on the GPU
    VkResult result = contextWrapper.getDeviceFaultReportsKHR(SHORT_FAULT_REPORT_WAIT, &faultCounts, nullptr);
    if (VK_TIMEOUT != result)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultReportsKHR should return VK_TIMEOUT if no report is available in the "
            "specified time period (1ns)");

    if (faultCounts != 0)
        return tcu::TestStatus::fail(
            "with no device lost vkGetDeviceFaultReportsKHR(timeout=1ns) did not set the faultCounts to 0");

    return tcu::TestStatus::pass(
        "with no device lost vkGetDeviceFaultReportsKHR(timeout=1ns) returned VK_TIMEOUT and faultCount was set to 0");
}

tcu::TestStatus deviceFaultReportBlockingQueryCase(Context &context, TestFlags testOptions)
{
    // Simple test: trigger a device lost with shader abort and query the fault count/data
    assert((testOptions & USE_SHADER_ABORT) == USE_SHADER_ABORT);
    const ContextWrapper contextWrapper(context, testOptions);

    VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    uint32_t faultCounts = DUMMY_VALUE;
    uint32_t tryCounter  = 100;
    VkResult result      = VK_NOT_READY;

    for (; tryCounter > 0; tryCounter--)
    {
        result = contextWrapper.getDeviceFaultReportsKHR(LONG_FAULT_REPORT_WAIT, &faultCounts, nullptr);

        if (result == VK_SUCCESS)
            break;
    }

    // If the fault count did not changed, that is weird
    if (faultCounts == DUMMY_VALUE)
    {
        if (tryCounter == 0)
            return tcu::TestStatus::fail("Device fault count was not updated even after multiple tries");

        return tcu::TestStatus::fail("Device fault count was not updated");
    }

    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Incorrect return value for vkGetDeviceFaultReportsKHR in case of device lost");

    if (faultCounts == 0)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR returned VK_SUCCESS but faultCount was set to 0, "
                                     "but there should be a report");

    tcu::TestLog &log = context.getTestContext().getLog();
    log << tcu::TestLog::Section("deviceFaultConfig", "Device Fault configuration used") << tcu::TestLog::Message
        << "Result: " << result << " tryCount: " << tryCounter << " "
        << "Fault counts: " << faultCounts << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;

    // Ok there is at least a single fault report, retrive all
    std::vector<VkDeviceFaultInfoKHR> faultReports(faultCounts);
    initDeviceFaultReportStructs(faultReports);

    result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, faultReports.data());
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail(
            "if faultCount > 0, then vkGetDeviceFaultReportsKHR should return VK_SUCCESS when querying the faults");

    contextWrapper.logDeviceFaultReport(faultReports);

    return tcu::TestStatus::pass("Basic usecase: in case of device lost fault information was returned");
}

tcu::TestStatus deviceFaultReportQueryZero(Context &context, TestFlags testOptions)
{
    // Simple test: trigger a device lost with shader abort and query the fault count/data
    assert((testOptions & USE_SHADER_ABORT) == USE_SHADER_ABORT);
    const ContextWrapper contextWrapper(context, testOptions);

    VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    uint32_t faultCounts = DUMMY_VALUE;
    uint32_t tryCounter  = 100;
    VkResult result      = VK_NOT_READY;

    for (; tryCounter > 0; tryCounter--)
    {
        result = contextWrapper.getDeviceFaultReportsKHR(LONG_FAULT_REPORT_WAIT, &faultCounts, nullptr);

        if (result == VK_SUCCESS)
            break;
    }

    // If we tried multiple times, but the faultCount was not updated, there is something fishy.
    if ((tryCounter == 0) && (faultCounts == DUMMY_VALUE))
        return tcu::TestStatus::fail("Device fault count was updateds even after multiple tries");

    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Incorrect return value for vkGetDeviceFaultReportsKHR in case of device lost");

    if (faultCounts == 0)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR returned VK_SUCCESS but faultCount was set to 0");

    // There is a fault report, query zero from it expecting VK_INCOMPLETE
    uint32_t zeroCount               = 0;
    VkDeviceFaultInfoKHR faultReport = initVulkanStructure();

    const VkResult zeroQueryResult = contextWrapper.getDeviceFaultReportsKHR(0, &zeroCount, &faultReport);

    if (zeroQueryResult != VK_INCOMPLETE)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR should return VK_INCOMPLETE if there is a report, "
                                     "but requested count is 0");

    return tcu::TestStatus::pass("Basic usecase: in case of device lost fault information was returned");
}

tcu::TestStatus deviceFaultReportBlockingQueryNoMoreFaultsAfterDeviceLostCase(Context &context, TestFlags testOptions)
{
    // If a fault report is returned there must be only a single report with
    // VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR set and it must be the last one.
    const ContextWrapper contextWrapper(context, testOptions);

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    uint32_t faultCounts = 0;
    uint32_t tryCounter  = 100;
    VkResult result      = VK_NOT_READY;

    for (; tryCounter > 0; tryCounter--)
    {
        result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, nullptr);

        if (result == VK_SUCCESS)
            break;
    }

    // If we tried multiple times, but the faultCount was not updated, there is something fishy.
    if ((tryCounter == 0) && (faultCounts == DUMMY_VALUE))
        return tcu::TestStatus::fail("Device fault count was updateds even after multiple tries");

    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Incorrect return value for vkGetDeviceFaultReportsKHR in case of device lost");

    if (faultCounts == 0)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR returned VK_SUCCESS but faultCount was set to 0");

    // Ok there is at least a single fault report, retrive all
    std::vector<VkDeviceFaultInfoKHR> faultReports(faultCounts);
    initDeviceFaultReportStructs(faultReports);

    result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, faultReports.data());
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail(
            "if faultCount > 0, then vkGetDeviceFaultReportsKHR should return VK_SUCCESS when querying the faults");

    contextWrapper.logDeviceFaultReport(faultReports);

    const uint64_t deviceFaultFlagCount =
        std::count_if(std::begin(faultReports), std::end(faultReports),
                      [](const VkDeviceFaultInfoKHR &report) -> bool
                      { return ((report.flags & VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR) != 0); });

    if (deviceFaultFlagCount > 1)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultReportsKHR should only report a single element with device lost flag");

    if (deviceFaultFlagCount == 1 && (faultReports.back().flags & VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR) == 0)
        return tcu::TestStatus::fail("If vkGetDeviceFaultReportsKHR returned a report flagged with "
                                     "VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR then it should be the last one");

    if (deviceFaultFlagCount == 0)
        return tcu::TestStatus::fail(
            "If device was lost, then one report must have the VK_DEVICE_FAULT_FLAG_DEVICE_LOST_KHR flag");

    return tcu::TestStatus::pass("In case of device lost vkGetDeviceFaultReportsKHR returned at least one report and "
                                 "the last report was correctly flagged with device lost");
}

tcu::TestStatus deviceFaultReportBlockingQueryMoreThanAvailableCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    uint32_t faultCounts = DUMMY_VALUE;
    VkResult result      = VK_NOT_READY;

    for (uint32_t tryCounter = 100; tryCounter > 0; tryCounter--)
    {
        result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, nullptr);

        if (result == VK_SUCCESS)
            break;
    }

    if ((faultCounts == DUMMY_VALUE) || (faultCounts == 0))
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR did not returned at least one fault reports");

    if (result != VK_SUCCESS)
    {
        std::stringstream msg;
        msg << "Incorrect return value for vkGetDeviceFaultReportsKHR in case of device lost. "
            << "Returned: " << result;
        return tcu::TestStatus::fail(msg.str());
    }

    // Querying more faults than available shouldn't cause a crash.
    const uint32_t increaseAmount = 5;
    uint32_t increasedFaultCounts = faultCounts + increaseAmount;

    std::vector<VkDeviceFaultInfoKHR> faultReports(increasedFaultCounts);
    initDeviceFaultReportStructs(faultReports);

    std::vector<VkDeviceFaultInfoKHR> emptyReports = faultReports;

    result = contextWrapper.getDeviceFaultReportsKHR(0, &increasedFaultCounts, faultReports.data());
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultReportsKHR should return VK_SUCCESS even if more was queried than reported");

    // Log the original set of reports
    contextWrapper.logDeviceFaultReport(
        std::vector<VkDeviceFaultInfoKHR>(faultReports.begin(), faultReports.begin() + faultCounts));

    // Check the excess report array elements that should not change
    for (uint32_t idx = faultCounts; idx < increasedFaultCounts; idx++)
    {
        if (deMemCmp(&faultReports[idx], &emptyReports[idx], sizeof(VkDeviceFaultInfoKHR)) != 0)
            return tcu::TestStatus::fail(
                "vkGetDeviceFaultReportsKHR shouldn't write more reports than originally reported");
    }

    return tcu::TestStatus::pass("vkGetDeviceFaultReportsKHR did returned only the available fault reports not more");
}

tcu::TestStatus deviceFaultReportDrainQueryCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    uint32_t faultCounts = DUMMY_VALUE;
    for (uint32_t tryCounter = 100; tryCounter > 0; tryCounter--)
    {
        VkResult result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, nullptr);

        if (result == VK_SUCCESS || faultCounts == 0)
            break;
    }

    if (faultCounts == DUMMY_VALUE)
        return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR did not updated the faultCounts");

    if (faultCounts == 0)
        return tcu::TestStatus::fail("Device fault info was not returned, but there should have been at least one");

    uint32_t processedReports = 0;
    bool queryMore            = true;

    // Only query a single fault
    while (queryMore)
    {
        VkDeviceFaultInfoKHR faultReport{
            VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_KHR, // sType
            nullptr,                                 // pNext
            static_cast<VkDeviceFaultFlagsKHR>(0),   // flags
            0u,                                      // groupId
            "sad",                                   // message
            {},                                      // faultAddress
            {},                                      // instructionAddress
            {}                                       // vendor
        };

        uint32_t oneByOneFaultCount = 1;

        // Every call should decrement the reported addressInfo counts by 1.
        const VkResult result = contextWrapper.getDeviceFaultReportsKHR(0, &oneByOneFaultCount, &faultReport);

        {
            // TODO: remove
            std::stringstream msg;
            msg << "Result: " << result << " count: " << oneByOneFaultCount;
            contextWrapper.logText(msg.str());
        }
        if (result == VK_SUCCESS || result == VK_INCOMPLETE)
        {
            // Ok, there was only one and it was queried
            processedReports++;

            contextWrapper.logDeviceFaultReport({faultReport});

            // Is there still more to query?
            queryMore = (result == VK_INCOMPLETE);
        }
        else if (result == VK_TIMEOUT)
        {
            queryMore = true;
        }
        else if (processedReports == 0)
        {
            // A non-success was returned and nothing was processed previously
            return tcu::TestStatus::fail("No Device fault report was found");
        }

        // Query the leftovers.
        uint32_t remainingFaultCounts  = DUMMY_VALUE;
        const VkResult remainingResult = contextWrapper.getDeviceFaultReportsKHR(0, &remainingFaultCounts, nullptr);

        {
            // TODO: remove
            std::stringstream msg;
            msg << "Remaining: " << remainingResult << " count: " << remainingFaultCounts;
            contextWrapper.logText(msg.str());
        }

        if ((remainingResult == VK_SUCCESS || remainingResult == VK_TIMEOUT) && (remainingFaultCounts == DUMMY_VALUE))
            return tcu::TestStatus::fail("Device fault counts were not updated");

        if ((!queryMore && remainingFaultCounts > 0) || (queryMore && remainingFaultCounts == 0))
        {
            // When queried first there was nothing to query, but second time
            // more faults are reported. Or the other way around
            return tcu::TestStatus::fail("Mismatch during device fault report draining");
        }

        // If there is nothing else to process then exit
        if ((remainingResult == VK_TIMEOUT) && (remainingFaultCounts == 0) && !queryMore)
            break;
    }

    if (processedReports == 0)
        return tcu::TestStatus::fail("No Device fault report was processed");

    if (faultCounts != processedReports)
        return tcu::TestStatus::fail("Was unable to query all fault reports");

    return tcu::TestStatus::pass("vkGetDeviceFaultReportsKHR can be used to query reports one by one");
}

tcu::TestStatus deviceFaultReportAsyncQueryCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);

    bool queryForFaults = true;
    auto asyncQueryFn   = [&contextWrapper, &queryForFaults]() -> tcu::TestStatus
    {
        while (queryForFaults)
        {
            uint32_t faultCounts = DUMMY_VALUE;
            const VkResult queryResult =
                contextWrapper.getDeviceFaultReportsKHR(LONG_FAULT_REPORT_WAIT, &faultCounts, nullptr);

            if (queryResult == VK_SUCCESS && faultCounts > 0)
            {
                std::vector<VkDeviceFaultInfoKHR> faultReports(faultCounts);
                initDeviceFaultReportStructs(faultReports);

                const VkResult result = contextWrapper.getDeviceFaultReportsKHR(0, &faultCounts, faultReports.data());
                if (result == VK_SUCCESS)
                {
                    contextWrapper.logDeviceFaultReport(faultReports);
                    return tcu::TestStatus::pass("Async usage of vkGetDeviceFaultReportsKHR returned fault reports");
                }
                else
                    return tcu::TestStatus::fail("vkGetDeviceFaultReportsKHR should return VK_SUCCESS");
            }
        }

        return tcu::TestStatus::fail("No faults were returned in the specified amount of time");
    };

    std::future<tcu::TestStatus> testStatus = std::async(std::launch::async, asyncQueryFn);

    deSleep(convertNsToMs(2 * LONG_FAULT_REPORT_WAIT));

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    // Wait ample time so the thread can catch the device lost
    deSleep(convertNsToMs(3 * LONG_FAULT_REPORT_WAIT));

    queryForFaults = false;

    return testStatus.get();
}

// Vendor binary tests

tcu::TestStatus deviceFaultVendorBinarySizeQueryCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);
    const bool inFaultMode = testOptions & USE_SHADER_ABORT;

    assert(contextWrapper.faultFeatures().deviceFaultVendorBinary && "Vendor binary support must be enabled");

    // TODO: should this be allowed without device abort/lost?
    if (inFaultMode)
    {
        const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();

        if (faultForceResult != VK_ERROR_DEVICE_LOST)
            return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");
    }

    VkDeviceFaultDebugInfoKHR debugInfo = initVulkanStructure();
    debugInfo.vendorBinarySize          = DUMMY_VALUE;

    const VkResult result = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("vkGetDeviceFaultDebugInfoKHR should return VK_SUCCESS");

    if (debugInfo.vendorBinarySize == DUMMY_VALUE)
    {
        const std::string msg =
            inFaultMode ? "vkGetDeviceFaultDebugInfoKHR should correctly set vendorBinarySize" :
                          "vkGetDeviceFaultDebugInfoKHR should set vendorBinarySize to zero if there is no device lost";

        return tcu::TestStatus::fail(msg);
    }

    return tcu::TestStatus::pass("vkGetDeviceFaultDebugInfoKHR correctly updated vendorBinarySize");
}

tcu::TestStatus deviceFaultVendorBinaryCase(Context &context, TestFlags testOptions)
{
    assert((testOptions & (USE_FAULT_BINARY | USE_SHADER_ABORT)) == (USE_FAULT_BINARY | USE_SHADER_ABORT));
    const ContextWrapper contextWrapper(context, testOptions);

    assert(contextWrapper.faultFeatures().deviceFaultVendorBinary && "Vendor binary support must be enabled");

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    // Get the vendor binary in device lost state
    VkDeviceFaultDebugInfoKHR debugInfo = initVulkanStructure();

    const VkResult sizeQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (sizeQueryResult != VK_SUCCESS)
        return tcu::TestStatus::fail("vkGtDeviceFaultDebugInfoKHR should return VK_SUCCESS");

    if (debugInfo.vendorBinarySize == 0)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "No vendor binary data size returned, nothing to check");

    std::vector<uint8_t> vendorBinary(debugInfo.vendorBinarySize);

    debugInfo.pVendorBinaryData = vendorBinary.data();

    const VkResult dataQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (dataQueryResult != VK_SUCCESS)
        return tcu::TestStatus::fail("vkGetDeviceFaultDebugInfoKHR should return VK_SUCCESS");

    contextWrapper.logDeviceFaultVendorBinaryHeader(vendorBinary);

    // Check some binary header info
    const VkDeviceFaultVendorBinaryHeaderVersionOneKHR *pBinaryHeader =
        reinterpret_cast<VkDeviceFaultVendorBinaryHeaderVersionOneKHR const *>(debugInfo.pVendorBinaryData);

    if (pBinaryHeader->headerVersion != VK_DEVICE_FAULT_VENDOR_BINARY_HEADER_VERSION_ONE_KHR)
        return tcu::TestStatus::fail("vkGetDeviceFaultDebugInfoKHR did not set binary header version");

    // TODO: add checks for vendorID, deviceID, etc?
    return tcu::TestStatus::pass("vkGetDeviceFaultDebugInfoKHR reported binary data");
}

tcu::TestStatus deviceFaultVendorBinaryNotEnoughSpaceCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);

    assert(contextWrapper.faultFeatures().deviceFaultVendorBinary && "Vendor binary support must be enabled");

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    VkDeviceFaultDebugInfoKHR debugInfo = initVulkanStructure();

    const VkResult sizeQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (sizeQueryResult != VK_SUCCESS)
        return tcu::TestStatus::fail("vkGtDeviceFaultDebugInfoKHR should return VK_SUCCESS");

    if (debugInfo.vendorBinarySize == 0)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "No vendor binary data returned, nothing to check");

    const uint32_t newBinarySize = debugInfo.vendorBinarySize;
    const uint8_t binaryTemp     = 0xFE;
    std::vector<uint8_t> vendorBinary(newBinarySize, binaryTemp);

    // Try to query a smaller vendor binary
    debugInfo.vendorBinarySize  = newBinarySize - 1;
    debugInfo.pVendorBinaryData = vendorBinary.data();

    const VkResult dataQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (dataQueryResult != VK_ERROR_NOT_ENOUGH_SPACE_KHR)
        return tcu::TestStatus::fail("vkGetDeviceFaultDebugInfoKHR should return VK_ERROR_NOUT_ENOUGH_SPACE_KHR "
                                     "if the given binary size is smaller than reported");

    // Check if binary blob was not written
    const uint64_t writtenValuesCount = std::count_if(std::begin(vendorBinary), std::end(vendorBinary),
                                                      [=](const uint32_t &byte) { return byte != binaryTemp; });

    if (writtenValuesCount != 0)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultDebugInfoKHR returned binary data even when a smaller vendorBinarySize was set");

    return tcu::TestStatus::pass(
        "vkGetDeviceFaultDebugInfoKHR did not write any binary data when smaller vendorBinarySize was set");
}

tcu::TestStatus deviceFaultVendorBinaryMoreSpaceCase(Context &context, TestFlags testOptions)
{
    const ContextWrapper contextWrapper(context, testOptions);

    assert(contextWrapper.faultFeatures().deviceFaultVendorBinary && "Vendor binary support must be enabled");

    const VkResult faultForceResult = contextWrapper.submitShaderAbortDeviceFault();
    if (faultForceResult != VK_ERROR_DEVICE_LOST)
        return tcu::TestStatus::fail("Unable to trigger VK_ERROR_DEVICE_LOST");

    VkDeviceFaultDebugInfoKHR debugInfo = initVulkanStructure();

    const VkResult sizeQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (sizeQueryResult != VK_SUCCESS)
        return tcu::TestStatus::fail("vkGtDeviceFaultDebugInfoKHR should return VK_SUCCESS");

    if (debugInfo.vendorBinarySize == 0)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "No vendor binary data returned, nothing to check");

    const uint32_t extraSize = 16;

    // Check if there could be any overflow
    if (debugInfo.vendorBinarySize >= (std::numeric_limits<uint32_t>::max() - extraSize))
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "Reported vendor binary size is already too big, can't test correct");

    const uint32_t originalBinarySize = debugInfo.vendorBinarySize;
    const uint32_t newBinarySize      = debugInfo.vendorBinarySize + extraSize;
    const uint8_t binaryTemp          = 0xFE;
    std::vector<uint8_t> vendorBinary(newBinarySize, binaryTemp);

    // Try to query a vendor binary with more space
    debugInfo.vendorBinarySize  = newBinarySize;
    debugInfo.pVendorBinaryData = vendorBinary.data();

    const VkResult dataQueryResult = contextWrapper.getDeviceFaultDebugInfoKHR(&debugInfo);
    if (dataQueryResult != VK_SUCCESS)
        return tcu::TestStatus::fail(
            "vkGetDeviceFaultDebugInfoKHR should return VK_SUCCESS even if a bigger binary size is specified");

    // Check extra data range if it was changed
    for (uint32_t idx = originalBinarySize; idx < newBinarySize; idx++)
    {
        if (vendorBinary[idx] != binaryTemp)
            return tcu::TestStatus::fail("vkGetDeviceFaultDebugInfoKHR wrote more data than it reported");
    }
    return tcu::TestStatus::pass("vkGetDeviceFaultDebugInfoKHR did not overwrite additional buffer data");
}

} // namespace

tcu::TestCaseGroup *createDeviceFaultTestsKHR(tcu::TestContext &testCtx)
{
    struct TestFlagCombination
    {
        TestFlags flags;
        const std::string name;
    } baseFlagCombinations[] = {{0, "base"}, {USE_DEVICE_WAIT_IDLE, "waitidle"}};

    auto rootGroup = new tcu::TestCaseGroup(testCtx, "device_fault");
    // Test that does not required device lost state
    {
        auto noDeviceLostGroup = new tcu::TestCaseGroup(testCtx, "no_fault");
        // vkGetDeviceFaultReportsKHR test cases
        addFunctionCase(noDeviceLostGroup, "max_fault_property_atleast_one", checkDeviceFaultReportSupport,
                        deviceFaultMaxFaultCountAtleastOne, static_cast<TestFlags>(USE_NONE));
        addFunctionCase(noDeviceLostGroup, "counts_query", checkDeviceFaultReportSupport,
                        deviceFaultReportCountQueryCase, static_cast<TestFlags>(USE_NONE));
        addFunctionCase(noDeviceLostGroup, "very_short_timeout", checkDeviceFaultReportSupport,
                        deviceFaultReportQueryVeryShortTimeoutCase, static_cast<TestFlags>(USE_NONE));

        addFunctionCase(noDeviceLostGroup, "vendor_binary_size_query", checkDeviceFaultReportSupport,
                        deviceFaultVendorBinarySizeQueryCase, static_cast<TestFlags>(USE_FAULT_BINARY));
        rootGroup->addChild(noDeviceLostGroup);
    }

    // Tests that require device lost state
    for (const auto &baseOption : baseFlagCombinations)
    {
        auto withDeviceLostGroup = new tcu::TestCaseGroup(testCtx, ("with_fault_" + baseOption.name).c_str());

        addFunctionCaseWithPrograms(withDeviceLostGroup, "blocking_query", checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultReportBlockingQueryCase,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "zero_report_query", checkDeviceFaultReportSupport,
                                    initProgram, deviceFaultReportQueryZero,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "blocking_query_no_more_faults_after_device_lost",
                                    checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultReportBlockingQueryNoMoreFaultsAfterDeviceLostCase,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);
        addFunctionCaseWithPrograms(withDeviceLostGroup, "blocking_query_more_than_available",
                                    checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultReportBlockingQueryMoreThanAvailableCase,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "drain_query", checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultReportDrainQueryCase,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);
        addFunctionCaseWithPrograms(withDeviceLostGroup, "async_query", checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultReportAsyncQueryCase,
                                    static_cast<TestFlags>(USE_SHADER_ABORT) | baseOption.flags);

        // vkGetDeviceFaultVendorBinaryKHR test cases
        addFunctionCaseWithPrograms(withDeviceLostGroup, "vendor_binary_size_query", checkDeviceFaultReportSupport,
                                    initProgram, deviceFaultVendorBinarySizeQueryCase,
                                    static_cast<TestFlags>(USE_FAULT_BINARY | USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "vendor_binary_data", checkDeviceFaultReportSupport,
                                    initProgram, deviceFaultVendorBinaryCase,
                                    static_cast<TestFlags>(USE_FAULT_BINARY | USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "vendor_binary_not_enough_space",
                                    checkDeviceFaultReportSupport, initProgram,
                                    deviceFaultVendorBinaryNotEnoughSpaceCase,
                                    static_cast<TestFlags>(USE_FAULT_BINARY | USE_SHADER_ABORT) | baseOption.flags);

        addFunctionCaseWithPrograms(withDeviceLostGroup, "vendor_binary_more_enough_space",
                                    checkDeviceFaultReportSupport, initProgram, deviceFaultVendorBinaryMoreSpaceCase,
                                    static_cast<TestFlags>(USE_FAULT_BINARY | USE_SHADER_ABORT) | baseOption.flags);

        rootGroup->addChild(withDeviceLostGroup);
    }

    return rootGroup;
}

} // namespace postmortem
} // namespace vkt
