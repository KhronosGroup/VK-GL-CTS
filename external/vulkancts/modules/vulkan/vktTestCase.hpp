#ifndef _VKTTESTCASE_HPP
#define _VKTTESTCASE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkDefs.hpp"
#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vkApiVersion.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkPlatform.hpp"
#include "vkResourceInterface.hpp"
#include "vktTestCaseDefs.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vktContextManager.hpp"
#include <vector>
#include <string>
#ifdef CTS_USES_VULKANSC
#include <mutex>
#endif // CTS_USES_VULKANSC

namespace glu
{
struct ProgramSources;
}

namespace vk
{
class PlatformInterface;
class Allocator;
struct SourceCollections;
} // namespace vk

namespace vkt
{

struct ContextCommonData
{
    const vk::PlatformInterface &vkp;
    const vk::InstanceInterface &vki;
    const vk::DeviceInterface &vkd;
    vk::VkInstance instance;
    vk::VkPhysicalDevice physicalDevice;
    vk::VkDevice device;
    vk::Allocator &allocator;
    uint32_t qfIndex;
    vk::VkQueue queue;
};

class DefaultDevice;

class Context
{
public:
    // Constructor retained for compatibility with legacy code,
    // only called in createServerVKSC() and VKSC pipeline compiler.
    Context(tcu::TestContext &testCtx, const vk::PlatformInterface &platformInterface,
            vk::BinaryCollection &progCollection, de::SharedPtr<vk::ResourceInterface> resourceInterface);
    Context(tcu::TestContext &testCtx, const vk::PlatformInterface &platformInterface,
            vk::BinaryCollection &progCollection, de::SharedPtr<const ContextManager> ctxmgr,
            vk::Move<vk::VkDevice> suggestedDevice, const std::string &deviceID,
            de::SharedPtr<DevCaps::RuntimeData> pRuntimeData, const std::vector<std::string> *pDeviceExtensions);
    virtual ~Context(void);

    tcu::TestContext &getTestContext(void) const
    {
        return m_testCtx;
    }
    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_platformInterface;
    }
    vk::BinaryCollection &getBinaryCollection(void) const
    {
        return m_progCollection;
    }
    de::SharedPtr<const ContextManager> getContextManager() const;

    // Default instance & device, selected with --deqp-vk-device-id=N
    uint32_t getMaximumFrameworkVulkanVersion(void) const;
    uint32_t getAvailableInstanceVersion(void) const;
    const std::vector<std::string> &getInstanceExtensions(void) const;
    vk::VkInstance getInstance(void) const;
    const vk::InstanceInterface &getInstanceInterface(void) const;
    vk::VkPhysicalDevice getPhysicalDevice(void) const;
    uint32_t getDeviceVersion(void) const;
    bool isDeviceFeatureInitialized(vk::VkStructureType sType) const;
    const vk::VkPhysicalDeviceFeatures &getDeviceFeatures(void) const;
    const vk::VkPhysicalDeviceFeatures2 &getDeviceFeatures2(void) const;
    const vk::VkPhysicalDeviceVulkan11Features &getDeviceVulkan11Features(void) const;
    const vk::VkPhysicalDeviceVulkan12Features &getDeviceVulkan12Features(void) const;
#ifndef CTS_USES_VULKANSC
    const vk::VkPhysicalDeviceVulkan13Features &getDeviceVulkan13Features(void) const;
    const vk::VkPhysicalDeviceVulkan14Features &getDeviceVulkan14Features(void) const;
#endif
#ifdef CTS_USES_VULKANSC
    const vk::VkPhysicalDeviceVulkanSC10Features &getDeviceVulkanSC10Features(void) const;
#endif // CTS_USES_VULKANSC

    bool isInstanceFunctionalitySupported(const std::string &extension) const;
    bool isDeviceFunctionalitySupported(const std::string &extension) const;

#include "vkDeviceFeaturesForContextDecl.inl"

    bool isDevicePropertyInitialized(vk::VkStructureType sType) const;
    const vk::VkPhysicalDeviceProperties &getDeviceProperties(void) const;
    const vk::VkPhysicalDeviceProperties2 &getDeviceProperties2(void) const;
    const vk::VkPhysicalDeviceVulkan11Properties &getDeviceVulkan11Properties(void) const;
    const vk::VkPhysicalDeviceVulkan12Properties &getDeviceVulkan12Properties(void) const;
#ifndef CTS_USES_VULKANSC
    const vk::VkPhysicalDeviceVulkan13Properties &getDeviceVulkan13Properties(void) const;
    const vk::VkPhysicalDeviceVulkan14Properties &getDeviceVulkan14Properties(void) const;
#endif
#ifdef CTS_USES_VULKANSC
    const vk::VkPhysicalDeviceVulkanSC10Properties &getDeviceVulkanSC10Properties(void) const;
#endif // CTS_USES_VULKANSC

#include "vkDevicePropertiesForContextDecl.inl"

    const std::vector<std::string> &getDeviceExtensions(void) const;
    const std::vector<const char *> &getDeviceCreationExtensions(void) const;
    vk::VkDevice getDevice(void) const;
    const vk::DeviceInterface &getDeviceInterface(void) const;
    uint32_t getUniversalQueueFamilyIndex(void) const;
    vk::VkQueue getUniversalQueue(void) const;
    uint32_t getUsedApiVersion(void) const;
    uint32_t getSparseQueueFamilyIndex(void) const;
    vk::VkQueue getSparseQueue(void) const;
    int getComputeQueueFamilyIndex(void) const;
    vk::VkQueue getComputeQueue(void) const;
    int getTransferQueueFamilyIndex(void) const;
    vk::VkQueue getTransferQueue(void) const;

    de::SharedPtr<vk::ResourceInterface> getResourceInterface(void) const;
    vk::Allocator &getDefaultAllocator(void) const;
    bool contextSupports(const uint32_t variantNum, const uint32_t majorNum, const uint32_t minorNum,
                         const uint32_t patchNum) const;
    bool contextSupports(const vk::ApiVersion version) const;
    bool contextSupports(const uint32_t requiredApiVersionBits) const;
    bool requireDeviceFunctionality(const std::string &required) const;
    bool requireInstanceFunctionality(const std::string &required) const;
    bool requireDeviceCoreFeature(const DeviceCoreFeature requiredDeviceCoreFeature) const;

#ifndef CTS_USES_VULKANSC
    vk::VkFormatProperties3 getFormatProperties(const vk::VkFormat &format) const;
    vk::VkFormatProperties3 getRequiredFormatProperties(const vk::VkFormat &format) const;
#endif // CTS_USES_VULKANSC

    void *getInstanceProcAddr();

    bool isBufferDeviceAddressSupported(void) const;

    bool resultSetOnValidation() const
    {
        return m_resultSetOnValidation;
    }
    void resultSetOnValidation(bool value)
    {
        m_resultSetOnValidation = value;
    }

#ifndef CTS_USES_VULKANSC
    bool hasDebugReportRecorder() const;
    vk::DebugReportRecorder &getDebugReportRecorder() const;
#endif // CTS_USES_VULKANSC

    void checkPipelineConstructionRequirements(const vk::PipelineConstructionType pipelineConstructionType);
    void resetCommandPoolForVKSC(const vk::VkDevice device, const vk::VkCommandPool commandPool);
    ContextCommonData getContextCommonData() const;

#ifdef CTS_USES_VULKANSC
    static std::vector<VkFaultData> m_faultData;
    static std::mutex m_faultDataMutex;
    static VKAPI_ATTR void VKAPI_CALL faultCallbackFunction(VkBool32 unrecordedFaults, uint32_t faultCount,
                                                            const VkFaultData *pFaults);
#endif // CTS_USES_VULKANSC

    bool isDefaultContext() const;
    std::string getDeviceID() const;
    DevCaps::QueueInfo getDeviceQueueInfo(uint32_t queueIndex);

    void collectAndReportDebugMessages();

protected:
    tcu::TestContext &m_testCtx;
    const vk::PlatformInterface &m_platformInterface;
    const de::SharedPtr<const ContextManager> m_contextManagerPtr;
    const de::WeakPtr<const ContextManager> m_contextManager;
    vk::BinaryCollection &m_progCollection;

    de::SharedPtr<vk::ResourceInterface> m_resourceInterface;
    de::SharedPtr<DevCaps::RuntimeData> m_deviceRuntimeData;
    const de::UniquePtr<DefaultDevice> m_device;
    const de::UniquePtr<vk::Allocator> m_allocator;

    bool m_resultSetOnValidation;

private:
    Context(const Context &);            // Not allowed
    Context &operator=(const Context &); // Not allowed
};

class TestInstance;

class TestCase : public tcu::TestCase
{
    friend class ContextManager;
    de::WeakPtr<const ContextManager> m_contextManager;
    void setContextManager(de::SharedPtr<const ContextManager>);

public:
    TestCase(tcu::TestContext &testCtx, const std::string &name);
    virtual ~TestCase(void) = default;

    // Override this function if the test requires a custom device. The framework
    // invokes this function to determine whether one of the recently created
    // devices can be reused or if a new custom device needs to be created with
    // the capabilities defined in initDeviceCapabilities.
    virtual std::string getRequiredCapabilitiesId() const;

    // Override this function if test requires new custom device.
    // Requirements for the new device should be recorded to DevCaps.
    virtual void initDeviceCapabilities(DevCaps &caps);

    // Override this function if the test requires a custom instance. The framework
    // invokes this function to determine whether one of the recently created
    // instances can be reused or if a new custom instance needs to be created with
    // the capabilities defined in initInstanceCapabilities.
    virtual std::string getInstanceCapabilitiesId() const;

    // Override this function if test requires new custom instance.
    // Requirements for the new instance should be recorded to InstCaps.
    virtual void initInstanceCapabilities(InstCaps &caps);

    // Returns the ContextManager on which the currently executing test was run.
    // ContextManager acts as a Vulkan instance with a physical device and the
    // currently executing test can use the information contained in it for the
    // time when the logical device has not been created in methods that do not
    // have access to Context, such as checkSupport() or delayedInit().
    de::SharedPtr<const ContextManager> getContextManager() const;

    virtual void delayedInit(void); // non-const init called after checkSupport but before initPrograms
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

    IterateResult iterate(void)
    {
        DE_ASSERT(false);
        return STOP;
    } // Deprecated in this module
};

class TestInstance
{
public:
    TestInstance(Context &context) : m_context(context)
    {
    }
    virtual ~TestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void) = 0;

protected:
    Context &m_context;

private:
    TestInstance(const TestInstance &);
    TestInstance &operator=(const TestInstance &);
};

enum QueueCapabilities
{
    GRAPHICS_QUEUE = 0,
    COMPUTE_QUEUE,
    TRANSFER_QUEUE,
};

struct QueueData
{
    vk::VkQueue handle;
    uint32_t familyIndex;

    QueueData(vk::VkQueue queue, uint32_t ndx) : handle(queue), familyIndex(ndx)
    {
    }
};

class MultiQueueRunnerTestInstance : public TestInstance
{
public:
    MultiQueueRunnerTestInstance(Context &context, QueueCapabilities queueCaps);
    virtual ~MultiQueueRunnerTestInstance(void)
    {
    }

    virtual tcu::TestStatus queuePass(const QueueData &queueData) = 0;

private:
    virtual tcu::TestStatus iterate(void) override;

protected:
    std::vector<QueueData> m_queues;
    QueueCapabilities m_queueCaps;
};

inline TestCase::TestCase(tcu::TestContext &testCtx, const std::string &name)
    : tcu::TestCase(testCtx, name.c_str(), "")
    , m_contextManager()
{
}

#ifndef CTS_USES_VULKANSC

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context &context);

#endif // CTS_USES_VULKANSC

uint32_t findQueueFamilyIndexWithCaps(const vk::InstanceInterface &vkInstance, vk::VkPhysicalDevice physicalDevice,
                                      vk::VkQueueFlags requiredCaps, vk::VkQueueFlags excludedCaps = 0u,
                                      uint32_t *availableCount = nullptr);

} // namespace vkt

#endif // _VKTTESTCASE_HPP
