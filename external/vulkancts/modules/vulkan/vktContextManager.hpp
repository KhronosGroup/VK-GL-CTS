#ifndef _VKTCONTEXTMANAGER_HPP
#define _VKTCONTEXTMANAGER_HPP
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
 * \brief Vulkan test ContextManager class declaration file.
 *//*--------------------------------------------------------------------*/

#include "vkTypeUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDeviceFeatures.hpp"
#include "vkDeviceProperties.hpp"
#include "vkResourceInterface.hpp"
#include "vkPrograms.hpp"
#include "vktTestCaseDefs.hpp"
#ifndef CTS_USES_VULKANSC
#include "vkDebugReportUtil.hpp"
#endif

#include <deque>
#include <string>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace vk
{

template <class>
struct DevFeaturesAndPropertiesImpl
{
    const DeviceFeatures &m_deviceFeatures;
    const DeviceProperties &m_deviceProperties;
    DevFeaturesAndPropertiesImpl(const DeviceFeatures &features, const DeviceProperties &properties)
        : m_deviceFeatures(features)
        , m_deviceProperties(properties)
    {
    }
#include "vkDeviceFeaturesForDefaultDeviceDefs.inl"
#include "vkDevicePropertiesForDefaultDeviceDefs.inl"
    bool isDeviceFeatureInitialized(VkStructureType sType) const
    {
        return m_deviceFeatures.isDeviceFeatureInitialized(sType);
    }
    const VkPhysicalDeviceFeatures &getDeviceFeatures(void) const
    {
        return m_deviceFeatures.getCoreFeatures2().features;
    }
    const VkPhysicalDeviceFeatures2 &getDeviceFeatures2(void) const
    {
        return m_deviceFeatures.getCoreFeatures2();
    }
    const VkPhysicalDeviceVulkan11Features &getVulkan11Features(void) const
    {
        return m_deviceFeatures.getVulkan11Features();
    }
    const VkPhysicalDeviceVulkan12Features &getVulkan12Features(void) const
    {
        return m_deviceFeatures.getVulkan12Features();
    }
#ifndef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkan13Features &getVulkan13Features(void) const
    {
        return m_deviceFeatures.getVulkan13Features();
    }
    const VkPhysicalDeviceVulkan14Features &getVulkan14Features(void) const
    {
        return m_deviceFeatures.getVulkan14Features();
    }
#endif
    bool isDevicePropertyInitialized(VkStructureType sType) const
    {
        return m_deviceProperties.isDevicePropertyInitialized(sType);
    }
    const VkPhysicalDeviceProperties &getDeviceProperties(void) const
    {
        return m_deviceProperties.getCoreProperties2().properties;
    }
    const VkPhysicalDeviceProperties2 &getDeviceProperties2(void) const
    {
        return m_deviceProperties.getCoreProperties2();
    }
    const VkPhysicalDeviceVulkan11Properties &getDeviceVulkan11Properties(void) const
    {
        return m_deviceProperties.getVulkan11Properties();
    }
    const VkPhysicalDeviceVulkan12Properties &getDeviceVulkan12Properties(void) const
    {
        return m_deviceProperties.getVulkan12Properties();
    }
#ifndef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkan13Properties &getDeviceVulkan13Properties(void) const
    {
        return m_deviceProperties.getVulkan13Properties();
    }
    const VkPhysicalDeviceVulkan14Properties &getDeviceVulkan14Properties(void) const
    {
        return m_deviceProperties.getVulkan14Properties();
    }
#else
    const VkPhysicalDeviceVulkanSC10Properties &getDeviceVulkanSC10Properties(void) const
    {
        return m_deviceProperties.getVulkanSC10Properties();
    }
#endif // CTS_USES_VULKANSC
};

namespace dc
{
#include "vkDeviceFeaturesVariantDecl.inl"
}

} // namespace vk

namespace tcu
{
class TestContext;
class TestLog;
}; // namespace tcu

namespace vkt
{

class TestCase;
class Context;
class ContextManager;
class TestCaseExecutor;

// The DevCaps class encapsulates the requirements for creating a new device.
// A key attribute is the DevCaps::id field, which the framework relies on to
// distinguish between a default device and a custom device. By default, this
// field is set to DefDevId, signaling that the device is a default one.

class DevCaps
{
    using strings = std::vector<std::string>;

    // Helper structures
    struct ExtensionsRef
    {
        const strings &extensions;
        ExtensionsRef(const strings &exts) : extensions(exts)
        {
        }
    };
    struct QueueInfo_
    {
        vk::VkQueue queue;
        uint32_t familyIndex;
    };
    struct QueueCreateInfo_
    {
        vk::VkQueueFlags required;
        vk::VkQueueFlags excluded;
        uint32_t count;
        float priority;
    };
    struct RuntimeData_
    {
        friend class ContextManager;
        void verify() const;
        QueueInfo_ getQueue(const vk::DeviceInterface &, vk::VkDevice, uint32_t queueIndex,
                            bool isDefaultContext) const;
        RuntimeData_() = default;
        RuntimeData_(const DevCaps &caps); // calls resetQueues

    private:
        void resetQueues(const DevCaps &, std::vector<vk::VkDeviceQueueCreateInfo> &, std::vector<float> &);
        // index in familyToQueueIndices refers to programmer queue index
        // familyToQueueIndices[] refers to a pair of {queueFamilyIndex, queueIndex in family}
        std::vector<std::pair<uint32_t, uint32_t>> familyToQueueIndices;
    };
    struct FeatureInfo_
    {
        vk::VkStructureType sType;
        void *address;
        uint32_t index;
        uint32_t size;
        FeatureInfo_();
        void reset();
    };

    // Helper container for two lambdas
    struct Caller
    {
        virtual bool compareExchange(void *pExpected, void *pDesired) = 0;
        virtual void *addFeature()                                    = 0;
    };
    template <class CompareExchange, class AddFeature>
    struct LambdaCaller : Caller
    {
        AddFeature fnAddFeature;
        CompareExchange fnCompareExchange;
        LambdaCaller(CompareExchange compareExchange, AddFeature addFeature)
            : fnAddFeature(addFeature)
            , fnCompareExchange(compareExchange)
        {
        }
        virtual bool compareExchange(void *pExpected, void *pDesired) override
        {
            return fnCompareExchange(pExpected, pDesired);
        }
        virtual void *addFeature() override
        {
            return fnAddFeature();
        }
    };

    strings m_extensions;
    ExtensionsRef m_extensionsRef;
    const ContextManager *const m_contextManager;
    using FeaturesVar_ = vk::dc::FullFeaturesVariant;
    using Features_    = std::vector<FeaturesVar_>;
    Features_ m_features;
    std::vector<QueueCreateInfo_> m_queueCreateInfos;
    bool m_hasInheritedExtensions;
    tcu::TestContext &m_testContext;

    template <class FeatureStruct>
    void prepareFeature(FeatureStruct &feature, vk::VkStructureType sType)
    {
        static_cast<void>(sType);
        static_cast<void>(feature);
        if constexpr (vk::dc::hasPnextOfVoidPtr<FeatureStruct>::value)
        {
            feature.pNext = nullptr;
            feature.sType = sType;
        }
    }

    template <class FeatureStruct, class FieldType>
    bool _addFeatureSet(const FeatureStruct *pSource, FieldType FeatureStruct::*pField, const FieldType &setToValue,
                        const FieldType &expectedValue = {}, bool enableExpected = false)
    {
        FeatureStruct expected          = FeatureStruct{};
        FeatureStruct desired           = pSource ? *pSource : expected;
        const vk::VkStructureType sType = vk::dc::getFeatureSType<FeatureStruct>();

        // make sure sType has correct value and pNext is nullptr
        prepareFeature(desired, sType);
        prepareFeature(expected, sType);

        // lambda function that updates a specified field in a feature struct to a
        // desired value and returns success upon completion
        auto compareExchange = [&](void *pExpected, void *pDesired) -> bool
        {
            if (pField ?
                    (enableExpected ? (expectedValue == reinterpret_cast<FeatureStruct *>(pExpected)->*pField) : true) :
                    false)
            {
                if (pDesired)
                {
                    reinterpret_cast<FeatureStruct *>(pDesired)->*pField = setToValue;
                }
                return true;
            }
            return false;
        };

        // addFeature lambda is executed when compareExchange is successfull;
        // it verifies feature structure against blob feature structures and
        // appends it to the vector of features structs
        auto addFeature = [&]() -> void *
        {
            verifyFeature(sType, true);
            prepareFeature(desired, sType);
            m_features.emplace_back(desired);
            return std::get_if<FeatureStruct>(&m_features.back());
        };

        LambdaCaller caller(compareExchange, addFeature);
        return addUpdateFeature(sType, (pField ? &expected : nullptr), pSource,
                                static_cast<uint32_t>(sizeof(FeatureStruct)), caller);
    }

    template <class FeatureStruct>
    bool getFeature(FeatureStruct &feature) const
    {
        const vk::VkStructureType sType = vk::dc::getFeatureSType<FeatureStruct>();
        FeatureInfo_ info               = getFeatureInfo(sType, m_features);
        if (info.address)
        {
            feature = *reinterpret_cast<FeatureStruct *>(info.address);
            return true;
        }
        return false;
    }

    template <class FeatureStruct>
    bool hasFeature() const
    {
        const vk::VkStructureType sType = vk::dc::getFeatureSType<FeatureStruct>();
        FeatureInfo_ info               = getFeatureInfo(sType, m_features);
        return info.address != nullptr;
    }

    bool addUpdateFeature(vk::VkStructureType sType, void *pExpected, const void *pSource, uint32_t featureSize,
                          Caller &caller);
    void updateDeviceCreateInfo(vk::VkDeviceCreateInfo &createInfo, vk::VkPhysicalDeviceFeatures2 *opt, Features_ &aux,
                                void *pNext) const;
    FeatureInfo_ getFeatureInfo(vk::VkStructureType sType, const Features_ &others) const;
    void fillFeatureFromInstance(void *pNext, bool isVkPhysicalDeviceFeatures10) const;
    void resetQueues(const QueueCreateInfo_ *pInfos, uint32_t infoCount);

public:
    // public field to differentiate vkt::TestCase derivative types
    const std::string id;

    // name of default device that will be used in transition
    // period until all test groups use custom devices
    static const inline std::string DefDevId = "DEFAULT";

    using QueueInfo       = QueueInfo_;
    using RuntimeData     = RuntimeData_;
    using QueueCreateInfo = QueueCreateInfo_;
    using FeatureInfo     = FeatureInfo_;
    using FeaturesVar     = FeaturesVar_;
    using Features        = Features_;

    friend class ContextManager;
    const ContextManager &getContextManager() const;
    const strings &getPhysicalDeviceExtensions() const;
    const std::vector<QueueCreateInfo> &getQueueCreateInfos() const;

    DevCaps(const std::string &id_, const ContextManager *mgr, tcu::TestContext &testContext);
    DevCaps(const DevCaps &caps);
    DevCaps(DevCaps &&caps) noexcept;

    void verifyFeature(vk::VkStructureType feature, bool checkRuntimeApiVersion = true) const;

    // Add extension only if it is supported on the device.
    bool addExtension(std::string &&extension, bool checkIfInCore = true);
    bool addExtension(const std::string &extension, bool checkIfInCore = true);

    // Add all extensions that are available on the device.
    void setInheritedExtensions();

    // Do not inherit any extensions from device - this is default behaviour.
    void setOwnExtensions();

    // Returns true if all available extensions will be enabled.
    bool hasInheritedExtensions() const;

    // Add a feature structure and fill it automaticaly with all supported fields.
    // Supported fields are obtained via vkGetPhysicalDeviceFeatures2.
    template <class FeatureStruct>
    bool addFeature()
    {
        return addFeature<FeatureStruct, std::nullptr_t>(nullptr, nullptr, nullptr);
    }

    // Add a feature structure if the specified field can be true on the current device
    // (this is verified via vkGetPhysicalDeviceFeatures2).
    // If the field can't be set to true, then the feature structure won't be added.
    // If the structure was added previously, then calling this function again with
    // a different field will just enable that field, but only when it is possible
    // for the current device.
    template <class FeatureStruct>
    bool addFeature(vk::VkBool32 FeatureStruct::*pField)
    {
        return _addFeatureSet<FeatureStruct>(nullptr, pField, VK_TRUE, VK_TRUE, true);
    }

    // Add a feature structure with a copy of the fields from the structure passed in the argument.
    // Note that content of feature.pNext will be discarded and treated as if it is set to nullptr.
    template <class FeatureStruct>
    bool addFeature(const FeatureStruct &feature)
    {
        return _addFeatureSet<FeatureStruct, std::nullptr_t>(&feature, nullptr, nullptr);
    }

    // Add feature field that is not boolean.
    template <class FeatureStruct, class FieldType>
    bool addFeature(FieldType FeatureStruct::*pField, const std::decay_t<FieldType> &setToValue,
                    const std::decay_t<FieldType> &expectedValue = {}, bool enableExpectd = false)
    {
        return _addFeatureSet<FeatureStruct>(nullptr, pField, setToValue, expectedValue, enableExpectd);
    }

    // Clear extensions, features and reset whole DevCaps instance to initial values.
    void reset();

    // used by findQueueFamilyIndexWithCaps
    template <uint32_t N>
    void resetQueues(const QueueCreateInfo (&infos)[N])
    {
        resetQueues(infos, N);
    }
};

struct InstCaps
{
    // Creates InstCaps class with id=DefInstId. This default behavior mostly used in existing code.
    InstCaps(const vk::PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine);
    // Creates InstCaps class with id=id_ parameter. This allows the ContextManager class to distinguish
    // whether the test needs a different instance than the default one.
    InstCaps(const vk::PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine, const std::string &id_);

    // All fields below are initialized in the same way as in the default instance.
    const uint32_t maximumFrameworkVulkanVersion;
    const uint32_t availableInstanceVersion;
    const uint32_t usedInstanceVersion;
    const std::pair<uint32_t, uint32_t> deviceVersions;
    const uint32_t usedApiVersion;
    const std::vector<std::string> coreExtensions;

    // This InstCaps identity.
    const std::string id;
    static const inline std::string DefInstId = "DEFAULT";

    // Adds instance extension to internal list.
    // If the extension is not available in the Core list, the method returns false.
    bool addExtension(const std::string &extension);

    // Returns a list of extensions required to create a new instance,
    // concatenated from Core and the internal extension list.
    std::vector<std::string> getExtensions() const;

private:
    std::vector<std::string> m_extensions;
};

typedef vk::DevFeaturesAndPropertiesImpl<void> DevFeaturesAndProperties;

// The ContextManager handles the creation and storage of Context instances needed for tests.
// It maintains a number of contexts as specified by m_maxCustomDevices. The ContextManager
// either provides the required Context to a TestInstance or generates and stores
// a new Context with capabilities defined by the TestCase through a DevCaps object.

class ContextManager
{
    const uint32_t m_maximumFrameworkVulkanVersion;
    const vk::PlatformInterface &m_platformInterface;
    const tcu::CommandLine &m_commandLine;
    const de::SharedPtr<vk::ResourceInterface> m_resourceInterface;
    const uint32_t m_availableInstanceVersion;
    const uint32_t m_usedInstanceVersion;
    const std::pair<uint32_t, uint32_t> m_deviceVersions;
    const uint32_t m_usedApiVersion;
    const std::vector<std::string> m_instanceExtensions;
#ifndef CTS_USES_VULKANSC
    using DebugReportRecorderPtr = de::SharedPtr<vk::DebugReportRecorder>;
    using DebugReportCallbackPtr = de::SharedPtr<vk::VkDebugUtilsMessengerEXT>;
    const DebugReportRecorderPtr m_debugReportRecorder;
#endif // CTS_USES_VULKANSC
    vk::VkInstance m_instanceHandle;
    const de::SharedPtr<vk::VkInstance> m_instance;
#ifndef CTS_USES_VULKANSC
    const de::SharedPtr<vk::InstanceDriver> m_instanceInterface;
    vk::VkDebugUtilsMessengerEXT m_debugReportCallbackHandle;
    const DebugReportCallbackPtr m_debugReportCallback;
#else
    const de::SharedPtr<InstanceDriver> m_instanceInterface;
#endif // CTS_USES_VULKANSC
    const vk::VkPhysicalDevice m_physicalDevice;
    const uint32_t m_deviceVersion;
    const int m_maxCustomDevices;
    const std::vector<std::string> m_deviceExtensions;
    const std::vector<const char *> m_creationExtensions;
    const de::SharedPtr<vk::DeviceFeatures> m_deviceFeaturesPtr;
    const de::SharedPtr<vk::DeviceProperties> m_devicePropertiesPtr;
    const de::SharedPtr<DevFeaturesAndProperties> m_deviceFeaturesAndProperties;
    using Item = std::pair<de::SharedPtr<Context>, de::SharedPtr<DevCaps>>;
    std::vector<Item> m_contexts;
    std::deque<de::SharedPtr<ContextManager>> m_customManagers;

    friend class TestCaseExecutor;
    typedef std::tuple<int, int, int> Det_;
    ContextManager(const vk::PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine,
                   de::SharedPtr<vk::ResourceInterface> resourceInterface, int maxCustomDevices, const InstCaps &,
                   Det_ icapsDet_);
    ContextManager(const vk::PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine,
                   de::SharedPtr<vk::ResourceInterface> resourceInterface, int maxCustomDevices, const InstCaps &icaps);
    void keepMaxCustomDeviceCount();
    void print(tcu::TestLog &log, const vk::VkDeviceCreateInfo &createInfo) const;
    void setContextManager(de::SharedPtr<const ContextManager> cm, vkt::TestCase *testCase);

public:
    const std::string id;
    static const inline std::string DefMgrId = "DEFAULT";

    virtual ~ContextManager() = default;

    static de::SharedPtr<ContextManager> create(const vk::PlatformInterface &vkPlatform,
                                                const tcu::CommandLine &commandLine,
                                                de::SharedPtr<vk::ResourceInterface> resourceInterface,
                                                int maxCustomDevices, const InstCaps &icaps);

    uint32_t getMaximumFrameworkVulkanVersion() const
    {
        return m_maximumFrameworkVulkanVersion;
    }
    auto getPlatformInterface() const -> const vk::PlatformInterface &
    {
        return m_platformInterface;
    }
    auto getCommandLine() const -> const tcu::CommandLine &
    {
        return m_commandLine;
    }
    auto getResourceInterface() const -> de::SharedPtr<vk::ResourceInterface>
    {
        return m_resourceInterface;
    }
    uint32_t getAvailableInstanceVersion() const
    {
        return m_availableInstanceVersion;
    }
    uint32_t getUsedInstanceVersion() const
    {
        return m_usedInstanceVersion;
    }
    uint32_t getUsedApiVersion() const
    {
        return m_usedApiVersion;
    }
    auto getInstanceExtensions() const -> const std::vector<std::string> &
    {
        return m_instanceExtensions;
    }
    auto getInstanceHandle() const -> vk::VkInstance
    {
        return m_instanceHandle;
    }
    auto getInstance() const -> de::SharedPtr<vk::VkInstance>
    {
        return m_instance;
    }
    auto getInstanceDriver() const -> de::SharedPtr<vk::InstanceDriver>
    {
        return m_instanceInterface;
    }
    auto getInstanceInterface() const -> const vk::InstanceInterface &
    {
        return *m_instanceInterface;
    }
    auto getDeviceVersions() const -> std::pair<uint32_t, uint32_t>
    {
        return m_deviceVersions;
    }
    uint32_t getDeviceVersion() const
    {
        return m_deviceVersion;
    }
    auto getPhysicalDevice() const -> vk::VkPhysicalDevice
    {
        return m_physicalDevice;
    }
    auto getDeviceExtensions() const -> const std::vector<std::string> &
    {
        return m_deviceExtensions;
    }
    auto getDeviceCreationExtensions() const -> const std::vector<const char *> &
    {
        return m_creationExtensions;
    }
    auto getDeviceFeaturesAndProperties() const -> const DevFeaturesAndProperties &
    {
        return *m_deviceFeaturesAndProperties;
    }
    auto getDeviceFeaturesPtr() const -> de::SharedPtr<vk::DeviceFeatures>
    {
        return m_deviceFeaturesPtr;
    }
    auto getDevicePropertiesPtr() const -> de::SharedPtr<vk::DeviceProperties>
    {
        return m_devicePropertiesPtr;
    }
    auto createDevice(const DevCaps &caps, DevCaps::RuntimeData &data) const -> vk::Move<vk::VkDevice>;
    auto findContext(de::SharedPtr<const ContextManager> thiz, vkt::TestCase *testCase,
                     de::SharedPtr<Context> &defaultContext, vk::BinaryCollection &programs) -> de::SharedPtr<Context>;
    auto findCustomManager(vkt::TestCase *testCase, de::SharedPtr<ContextManager> defaultContextManager)
        -> de::SharedPtr<ContextManager>;
#ifndef CTS_USES_VULKANSC
    auto getDebugReportRecorder() const -> DebugReportRecorderPtr
    {
        return m_debugReportRecorder;
    }
    auto getDebugReportCallbackHandle() const -> vk::VkDebugUtilsMessengerEXT
    {
        return m_debugReportCallbackHandle;
    }
    auto getDebugReportCallback() const -> DebugReportCallbackPtr
    {
        return m_debugReportCallback;
    }
#endif
    auto getMaxCustomDevices() const -> int
    {
        return m_maxCustomDevices;
    }
};

} // namespace vkt

#endif // _VKTCONTEXTMANAGER_HPP
