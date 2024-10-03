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
    const vk::InstanceInterface &vki;
    vk::VkDevice device;
    const vk::DeviceInterface &vkd;
    vk::VkPhysicalDevice physicalDevice;
    vk::Allocator &allocator;
    uint32_t qfIndex;
    vk::VkQueue queue;
};

class DefaultDevice;

class Context
{
public:
    Context(tcu::TestContext &testCtx, const vk::PlatformInterface &platformInterface,
            vk::BinaryCollection &progCollection, de::SharedPtr<vk::ResourceInterface> resourceInterface);
    ~Context(void);

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
    bool requireDeviceCoreFeature(const DeviceCoreFeature requiredDeviceCoreFeature);

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

protected:
    tcu::TestContext &m_testCtx;
    const vk::PlatformInterface &m_platformInterface;
    vk::BinaryCollection &m_progCollection;

    de::SharedPtr<vk::ResourceInterface> m_resourceInterface;
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
public:
    TestCase(tcu::TestContext &testCtx, const std::string &name);
    virtual ~TestCase(void)
    {
    }

    virtual void delayedInit(void); // non-const init called after checkSupport but before initPrograms
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const = 0;
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

inline TestCase::TestCase(tcu::TestContext &testCtx, const std::string &name) : tcu::TestCase(testCtx, name.c_str(), "")
{
}

#ifndef CTS_USES_VULKANSC

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context &context);

#endif // CTS_USES_VULKANSC

uint32_t findQueueFamilyIndexWithCaps(const vk::InstanceInterface &vkInstance, vk::VkPhysicalDevice physicalDevice,
                                      vk::VkQueueFlags requiredCaps, vk::VkQueueFlags excludedCaps = 0u);

} // namespace vkt

#endif // _VKTTESTCASE_HPP
