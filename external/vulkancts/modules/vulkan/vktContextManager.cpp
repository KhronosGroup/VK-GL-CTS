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
 * \brief Vulkan test ContextManager class implementation file.
 *//*--------------------------------------------------------------------*/

#include "vktContextManager.hpp"
#include "vkDeviceFeatures.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "tcuTestCase.hpp"
#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#include "vkAppParamsUtil.hpp"
#include "deMemory.h"
#endif // CTS_USES_VULKANSC

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <tuple>
#include <typeindex>
#include <variant>
#include <vector>

namespace vkt
{
using namespace vk;
namespace fs = std::filesystem;

constexpr uint32_t INVALID_UINT32 = std::numeric_limits<uint32_t>::max();

DevCaps::DevCaps(const std::string &id_, const ContextManager *mgr, tcu::TestContext &testContext)
    : m_extensions()
    , m_extensionsRef(mgr->getDeviceExtensions())
    , m_contextManager(mgr)
    , m_features()
    , m_queueCreateInfos()
    , m_hasInheritedExtensions(false) // don't add all extensions that are available on the device
    , m_testContext(testContext)
    , id(id_)
{
    reset();
}

DevCaps::DevCaps(const DevCaps &caps)
    : m_extensions(caps.m_extensions)
    , m_extensionsRef(caps.m_hasInheritedExtensions ? caps.m_extensionsRef.extensions : m_extensions)
    , m_contextManager(caps.m_contextManager)
    , m_features(caps.m_features)
    , m_queueCreateInfos(caps.m_queueCreateInfos)
    , m_hasInheritedExtensions(caps.m_hasInheritedExtensions)
    , m_testContext(caps.m_testContext)
    , id(caps.id)
{
}

DevCaps::DevCaps(DevCaps &&caps) noexcept
    : m_extensions(std::move(caps.m_extensions))
    , m_extensionsRef(caps.m_hasInheritedExtensions ? caps.m_extensionsRef.extensions : m_extensions)
    , m_contextManager(caps.m_contextManager)
    , m_features(std::move(caps.m_features))
    , m_queueCreateInfos(std::move(caps.m_queueCreateInfos))
    , m_hasInheritedExtensions(caps.m_hasInheritedExtensions)
    , m_testContext(caps.m_testContext)
    , id(caps.id)
{
}

void DevCaps::RuntimeData_::verify() const
{
}

const ContextManager &DevCaps::getContextManager() const
{
    return *m_contextManager;
}

const DevCaps::strings &DevCaps::getPhysicalDeviceExtensions() const
{
    return m_extensionsRef.extensions;
}

const std::vector<DevCaps::QueueCreateInfo> &DevCaps::getQueueCreateInfos() const
{
    return m_queueCreateInfos;
}

bool DevCaps::addExtension(std::string &&extension, bool checkIfInCore)
{
    // check if extension is in core and there is no need to add it
    if (checkIfInCore)
    {
        const uint32_t usedApiVersion = m_contextManager->getUsedApiVersion();
        if (isCoreDeviceExtension(usedApiVersion, extension))
            return true;
    }

    // check if extension is available on the device
    auto &exts = m_contextManager->getDeviceExtensions();
    if (exts.end() == std::find(exts.begin(), exts.end(), extension))
        return false;

    m_extensions.push_back(std::move(extension));

    // if DevCap were configured to include all available extensions,
    // switch it to mode with list of added extensions
    if (m_hasInheritedExtensions)
        setOwnExtensions();

    return true;
}

bool DevCaps::addExtension(const std::string &extension, bool checkIfInCore)
{
    // check if extension is in core and there is no need to add it
    if (checkIfInCore)
    {
        const uint32_t usedApiVersion = m_contextManager->getUsedApiVersion();
        if (isCoreDeviceExtension(usedApiVersion, extension))
            return true;
    }

    // check if extension is available on the device
    auto &exts = m_contextManager->getDeviceExtensions();
    if (exts.end() == std::find(exts.begin(), exts.end(), extension))
        return false;

    m_extensions.push_back(extension);

    // if DevCap were configured to include all available extensions,
    // switch it to mode with list of added extensions
    if (m_hasInheritedExtensions)
        setOwnExtensions();

    return true;
}

void DevCaps::setInheritedExtensions()
{
    m_extensions.clear();
    new (&m_extensionsRef) ExtensionsRef(m_contextManager->getDeviceExtensions());
    m_hasInheritedExtensions = true;
}

void DevCaps::setOwnExtensions()
{
    new (&m_extensionsRef) ExtensionsRef(m_extensions);
    m_hasInheritedExtensions = false;
}

bool DevCaps::hasInheritedExtensions() const
{
    return m_hasInheritedExtensions;
}

void DevCaps::reset()
{
    m_extensions.clear();
    new (&m_extensionsRef) ExtensionsRef(m_contextManager->getDeviceExtensions());
    m_features.clear();
    setOwnExtensions();

    const VkQueueFlags requiredFlags = m_contextManager->getCommandLine().isComputeOnly() ?
                                           VkQueueFlags(VK_QUEUE_COMPUTE_BIT) :
                                           (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    resetQueues({{requiredFlags, 0, 1u, 1.0f}});
}

DevCaps::FeatureInfo_::FeatureInfo_()
{
    reset();
}

void DevCaps::FeatureInfo_::reset()
{
    // VkAtructureType that is not feture structure type enumeration.
    sType   = vk::VK_STRUCTURE_TYPE_APPLICATION_INFO;
    address = nullptr;
    index   = INVALID_UINT32;
    size    = 0u;
}

namespace
{

// Helper visitor to traverse vector of variants.
// It works in few modes - it can search for specified feature in the list
// or build a chain out of all feature structures that were added.
struct FeatureVisitor
{
    enum Mode
    {
        Searching,
        Chaining,
        Iterate
    };

    using FeatureInfo = DevCaps::FeatureInfo;
    using FeaturesVar = DevCaps::FeaturesVar;
    using Comparer    = std::function<bool(vk::VkStructureType, vk::VkStructureType, uint32_t, const void *, bool)>;

    Comparer m_doContinue; // determines if we need to go to the next element
    const DevCaps::Features &m_features;
    const Mode m_mode;
    VkStructureType m_breakType;
    bool &m_continueFlag;
    const uint32_t &m_featureIndex;
    FeatureInfo &m_featureInfo;
    DevCaps::Features m_chain;

    template <class FeatureStruct>
    void processFeature(const FeatureStruct &feature)
    {
        const VkStructureType sType = vk::dc::getFeatureSType<FeatureStruct>();
        const bool hasPnext         = dc::hasPnextOfVoidPtr<FeatureStruct>::value;

        if (m_mode == Mode::Chaining)
        {
            m_continueFlag          = true;
            FeaturesVar &var        = m_chain.emplace_back(feature);
            FeatureStruct *pFeature = std::get_if<FeatureStruct>(&var);

            DE_UNREF(pFeature);

            if constexpr (hasPnext)
            {
                DE_ASSERT(sType == pFeature->sType);
                pFeature->pNext       = m_featureInfo.address;
                m_featureInfo.address = pFeature;
            }
        }
        else if (m_mode == Mode::Searching)
        {
            if (m_continueFlag = (m_breakType != sType); (false == m_continueFlag))
            {
                m_featureInfo.address = (void *)(&feature);
                m_featureInfo.size    = uint32_t(sizeof(feature));
                m_featureInfo.index   = m_featureIndex;
                m_featureInfo.sType   = sType;
            }
        }
        else
        {
            m_continueFlag = m_doContinue ? m_doContinue(m_breakType, sType, m_featureIndex, &feature, hasPnext) : true;
        }
    }

#ifdef CTS_USES_VULKANSC
    void operator()(const vk::VkPhysicalDeviceVulkanSC10Features &feature)
    {
        processFeature(feature);
    }
#endif

    template <class FeatureStruct>
    void operator()(const FeatureStruct &feature)
    {
        processFeature(feature);
    }

    FeatureVisitor(Comparer doContinue, const DevCaps::Features &features, Mode mode, VkStructureType breakType,
                   const uint32_t &featureIndex, bool &continueFlag, DevCaps::FeatureInfo &featureInfo)
        : m_doContinue(doContinue)
        , m_features(features)
        , m_mode(mode)
        , m_breakType(breakType)
        , m_continueFlag(continueFlag)
        , m_featureIndex(featureIndex)
        , m_featureInfo(featureInfo)
        , m_chain()
    {
        featureInfo.reset();
        if (Mode::Chaining == m_mode)
        {
            m_chain.reserve(features.size());
        }
    }
};

DevCaps::Features traverseFeatures(
    const FeatureVisitor::Mode mode, const DevCaps::Features &features,
    VkStructureType breakType, // sType to searching for, it is first parameter of doContinue
    FeatureVisitor::FeatureInfo &featureInfo,
    FeatureVisitor::Comparer doContinue = nullptr) // if lhs == rhs return false to break looking for
{
    bool continueFlag     = false;
    uint32_t featureIndex = 0;

    FeatureVisitor visitor(doContinue, features, mode, breakType, featureIndex, continueFlag, featureInfo);

    for (auto begin = features.cbegin(), var = begin; var != features.cend(); ++var)
    {
        featureIndex = static_cast<uint32_t>(std::distance(begin, var));
        std::visit(visitor, *var);
        if (false == continueFlag)
            break;
    }

    if (mode == FeatureVisitor::Mode::Chaining)
    {
        return std::move(visitor.m_chain);
    }

    return {};
}

} // namespace

template <class Stream>
Stream &printPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures &features, Stream &str, uint32_t indent);
template <class Stream>
Stream &printDeviceCreateInfo(Stream &str, const VkDeviceCreateInfo &createInfo);

bool DevCaps::addUpdateFeature(vk::VkStructureType sType, void *pExpected, const void *pSource, uint32_t featureSize,
                               Caller &caller)
{
    const bool isVkPhysicalDeviceFeatures10 = (VK_STRUCTURE_TYPE_MAX_ENUM == sType);

    if (pExpected)
    {
        fillFeatureFromInstance(pExpected, isVkPhysicalDeviceFeatures10);

        if (caller.compareExchange(pExpected, nullptr))
        {
            const FeatureInfo fi = getFeatureInfo(sType, m_features);
            if (fi.size)
            {
                DE_ASSERT(fi.size == featureSize);
                caller.compareExchange(pExpected, fi.address);
            }
            else
            {
                void *pNewFeature = caller.addFeature();
                DE_ASSERT(pNewFeature); // Should never happen
                caller.compareExchange(pExpected, pNewFeature);
            }
            return true;
        }
    }
    else
    {
        const FeatureInfo fi = getFeatureInfo(sType, m_features);
        if (fi.size)
        {
            DE_UNREF(featureSize);
            DE_ASSERT(fi.size == featureSize);
            if (nullptr == pSource)
                fillFeatureFromInstance(fi.address, isVkPhysicalDeviceFeatures10);
            else
                deMemcpy(fi.address, pSource, size_t(featureSize));
        }
        else
        {
            void *pNewFeature = caller.addFeature();
            DE_ASSERT(pNewFeature); // Should never happen
            if (nullptr == pSource)
                fillFeatureFromInstance(pNewFeature, isVkPhysicalDeviceFeatures10);
            else
                deMemcpy(pNewFeature, pSource, size_t(featureSize));
        }
        return true;
    }

    return false;
}

void DevCaps::fillFeatureFromInstance(void *pNext, bool isVkPhysicalDeviceFeatures10) const
{
    if (isVkPhysicalDeviceFeatures10)
        m_contextManager->getInstanceInterface().getPhysicalDeviceFeatures(
            m_contextManager->getPhysicalDevice(), reinterpret_cast<vk::VkPhysicalDeviceFeatures *>(pNext));
    else
    {
        vk::VkPhysicalDeviceFeatures2 f2 = initVulkanStructure(pNext);
        m_contextManager->getInstanceInterface().getPhysicalDeviceFeatures2(m_contextManager->getPhysicalDevice(), &f2);
    }
}

void DevCaps::verifyFeature(vk::VkStructureType sType, bool checkRuntimeApiVersion) const
{
    // method that identifies scenarios where a feature from a blob is added followed
    // by the addition of the corresponding feature structure from the blob;
    // the reverse sequence is also detected

    const std::map<uint32_t, vk::VkStructureType> apiToBlob{
        {VK_API_VERSION_1_1, vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
        {VK_API_VERSION_1_2, vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES},
#ifdef CTS_USES_VULKANSC
        {VK_API_VERSION_1_0, vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES}
#else
        {VK_API_VERSION_1_3, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES},
        {VK_API_VERSION_1_4, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES},
#endif
    };
    const uint32_t newFeatureBlobVersion = vk::DeviceFeatures::getBlobFeatureVersion(sType);
    const bool newFeatureIsBlob          = newFeatureBlobVersion == 0u;
    auto blobToApi                       = [&](vk::VkStructureType blob) -> uint32_t
    {
        for (const std::pair<const uint32_t, vk::VkStructureType> &item : apiToBlob)
        {
            if (item.second == blob)
                return item.first;
        }
        return 0u;
    };
    const std::set<vk::VkStructureType> blobFeatures(newFeatureIsBlob ?
                                                         vk::DeviceFeatures::getVersionBlobFeatures(blobToApi(sType)) :
                                                         std::set<vk::VkStructureType>());

    enum class Status
    {
        Ok,
        AlreadyExists,
        WrongApiVersion,
        BlobInFeatures,
        FeatureInBlob
    };
    auto StatusToText = [](const Status &st) -> const char *
    {
        switch (st)
        {
        case Status::Ok:
            return "Ok";
        case Status::AlreadyExists:
            return "AlreadyExists";
        case Status::WrongApiVersion:
            return "WrongApiVersion";
        case Status::BlobInFeatures:
            return "BlobInFeatures";
        case Status::FeatureInBlob:
            return "FeatureInBlob";
        }
        return "<unknown>";
    };
    Status status                            = Status::Ok;
    vk::VkStructureType existenFeatureInBlob = vk::VK_STRUCTURE_TYPE_MAX_ENUM;

    auto featureInBlob = [&](vk::VkStructureType, vk::VkStructureType existing, uint32_t, const void *, bool) -> bool
    {
        if (auto i = blobFeatures.find(existing); i != blobFeatures.end())
        {
            existenFeatureInBlob = *i;
            status               = Status::FeatureInBlob;
            return false;
        }
        return true;
    };

    auto blobInFeatures = [&](vk::VkStructureType blob, vk::VkStructureType existing, uint32_t, const void *,
                              bool) -> bool
    {
        if (blob == existing)
        {
            status = Status::BlobInFeatures;
            return false;
        }
        return true;
    };

    auto maybeWrongApiVersion = [&]() -> void
    {
        const uint32_t runApiVersion = m_contextManager->getUsedApiVersion();
        if (const uint32_t minApiVersion = newFeatureIsBlob ? blobToApi(sType) : newFeatureBlobVersion;
            minApiVersion < runApiVersion)
        {
            status = Status::WrongApiVersion;
        }
    };

    auto alreadyExists = [&](vk::VkStructureType insert, vk::VkStructureType existing, uint32_t, const void *,
                             bool) -> bool
    {
        if (insert == existing)
        {
            if (false == checkRuntimeApiVersion)
                status = Status::AlreadyExists;
            else
                maybeWrongApiVersion();
            return false;
        }
        return true;
    };

    DevCaps::FeatureInfo fi;
    traverseFeatures(FeatureVisitor::Iterate, m_features, sType, fi, alreadyExists);

    if (newFeatureIsBlob && Status::Ok == status)
    {
        DevCaps::FeatureInfo irr;
        traverseFeatures(FeatureVisitor::Iterate, m_features, sType, irr, featureInBlob);
    }
    else if (Status::Ok == status)
    {
        DevCaps::FeatureInfo irr;
        auto blob = apiToBlob.find(newFeatureBlobVersion);
        DE_ASSERT(apiToBlob.end() != blob);
        traverseFeatures(FeatureVisitor::Iterate, m_features, blob->second, irr, blobInFeatures);
    }

    if (Status::Ok != status)
    {
        TCU_THROW(NotSupportedError, StatusToText(status));
    }
}

DevCaps::FeatureInfo_ DevCaps::getFeatureInfo(VkStructureType sType, const Features_ &others) const
{
    FeatureInfo_ info;
    traverseFeatures(FeatureVisitor::Mode::Searching, others, sType, info);
    return info;
}

void DevCaps::updateDeviceCreateInfo(vk::VkDeviceCreateInfo &createInfo, VkPhysicalDeviceFeatures2 *opt, Features &aux,
                                     void *pNext) const
{

    const FeatureInfo fi10 = getFeatureInfo(VK_STRUCTURE_TYPE_MAX_ENUM, aux);
    const bool hasF10      = 0u != fi10.size;

    const FeatureInfo fi11 = getFeatureInfo(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, aux);
    const bool hasF11      = 0u != fi11.size;

    // Handle the special case involving VkPhysicalDeviceFeatures, which lacks an sType,
    // and VkPhysicalDeviceFeatures2, which encapsulates VkPhysicalDeviceFeatures.
    // If both structures are added in DevCaps, then this code merges their features.

    if (hasF10)
    {
        VkPhysicalDeviceFeatures2 *pf2 = hasF11 ? reinterpret_cast<VkPhysicalDeviceFeatures2 *>(fi11.address) : opt;
        const VkBool32 *src            = reinterpret_cast<const VkBool32 *>(fi10.address);
        VkBool32 *dst                  = reinterpret_cast<VkBool32 *>(&pf2->features);
        const uint32_t N               = fi10.size / uint32_t(sizeof(VkBool32));
        for (uint32_t i = 0u; i < N; ++i)
        {
            // UNASSIGNED-GeneralParameterError-UnrecognizedBool32
            // Applications MUST not pass any other values than VK_TRUE or VK_FALSE
            // into a Vulkan implementation where a VkBool32 is expected.

            // rewrite VkPysicalDeviceFeature2 struct content
            if (dst[i])
                dst[i] = VK_TRUE;

            // merge VkPysicalDeviceFeature struct content
            if (src[i])
                dst[i] = VK_TRUE;
        }
    }

    createInfo.pNext            = hasF11 ? pNext : static_cast<void *>(opt);
    opt->pNext                  = hasF11 ? nullptr : pNext;
    createInfo.pEnabledFeatures = nullptr;
}

void DevCaps::resetQueues(const QueueCreateInfo_ *pInfos, uint32_t infoCount)
{
    m_queueCreateInfos.resize(infoCount);
    for (uint32_t i = 0u; i < infoCount; ++i)
        m_queueCreateInfos[i] = pInfos[i];
}

de::SharedPtr<ContextManager> ContextManager::create(const vk::PlatformInterface &vkPlatform,
                                                     const tcu::CommandLine &commandLine,
                                                     de::SharedPtr<vk::ResourceInterface> resourceInterface,
                                                     int maxCustomDevices, const InstCaps &icaps)
{
    DE_ASSERT(maxCustomDevices > 0);
    return de::SharedPtr<ContextManager>(
        new ContextManager(vkPlatform, commandLine, resourceInterface, maxCustomDevices, icaps));
}

ContextManager::ContextManager(const PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine,
                               [[maybe_unused]] de::SharedPtr<vk::ResourceInterface> resourceInterface,
                               int maxCustomDevices, const InstCaps &icaps)
    : ContextManager(vkPlatform, commandLine, resourceInterface, maxCustomDevices, icaps, {})
{
}

void ContextManager::keepMaxCustomDeviceCount()
{
    auto isDef = [](const Item &item) { return item.first->isDefaultContext(); };
    auto def   = std::find_if(m_contexts.begin(), m_contexts.end(), isDef);
    DE_ASSERT(m_contexts.end() != def);
    DE_ASSERT(m_maxCustomDevices > 0);
    while (m_contexts.size() >= uint32_t(m_maxCustomDevices + 1))
    {
        auto beg = m_contexts.begin();
        def      = std::find_if(beg, m_contexts.end(), isDef);
        auto rem = (def != beg) ? beg : std::next(def);
        m_contexts.erase(rem);
    }
}

InstCaps::InstCaps(const PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine)
    : InstCaps(vkPlatform, commandLine, InstCaps::DefInstId)
{
}

bool InstCaps::addExtension(const std::string &extension)
{
    if (isInstanceExtensionSupported(usedApiVersion, coreExtensions, extension))
    {
        m_extensions.push_back(extension);
        return true;
    }
    return false;
}

std::vector<std::string> InstCaps::getExtensions() const
{
    std::vector<std::string> exts(coreExtensions);
    exts.insert(exts.end(), m_extensions.begin(), m_extensions.end());
    return exts;
}

de::SharedPtr<ContextManager> ContextManager::findCustomManager(vkt::TestCase *testCase,
                                                                de::SharedPtr<ContextManager> defaultContextManager)
{
    const std::string instCapsId = testCase->getInstanceCapabilitiesId();
    if (instCapsId != InstCaps::DefInstId)
    {
        for (auto mgr : m_customManagers)
        {
            if (mgr->id == instCapsId)
                return mgr;
        }

        const PlatformInterface &platformInterface         = defaultContextManager->getPlatformInterface();
        const tcu::CommandLine &commandLine                = defaultContextManager->getCommandLine();
        de::SharedPtr<ResourceInterface> resourceInterface = defaultContextManager->getResourceInterface();
        const int maxCustomDevices                         = defaultContextManager->getMaxCustomDevices();

        InstCaps icaps(platformInterface, commandLine, instCapsId);
        testCase->initInstanceCapabilities(icaps);
        de::SharedPtr<ContextManager> customContextManager =
            ContextManager::create(platformInterface, commandLine, resourceInterface, maxCustomDevices, icaps);

        if (m_customManagers.size() > static_cast<decltype(m_customManagers.size())>(maxCustomDevices))
            m_customManagers.pop_front();
        m_customManagers.push_back(customContextManager);

        return customContextManager;
    }

    return defaultContextManager;
}

de::SharedPtr<Context> ContextManager::findContext(de::SharedPtr<const ContextManager> thiz, TestCase *testCase,
                                                   de::SharedPtr<Context> &defaultContext,
                                                   vk::BinaryCollection &programs)
{
    de::SharedPtr<Context> checkContext;

    tcu::TestContext &testContext = testCase->getTestContext();

    try
    {
        // Create context with default device for compatibility with existing code.
        // If any of the calls throws an exception, the context with the default
        // device will be returned from this function.
        {
            auto isDef = [](const Item &item) { return item.first->isDefaultContext(); };
            auto def   = std::find_if(m_contexts.begin(), m_contexts.end(), isDef);
            if (m_contexts.end() == def)
            {
                de::SharedPtr<DevCaps> caps(new DevCaps(DevCaps::DefDevId, this, testContext));
                de::SharedPtr<DevCaps::RuntimeData> runtimeData(new DevCaps::RuntimeData(*caps));
                de::SharedPtr<Context> ctx(new Context(testContext, m_platformInterface, programs, thiz,
                                                       vk::Move<vk::VkDevice>(), caps->id, runtimeData,
                                                       &getDeviceExtensions()));
                m_contexts.emplace_back(std::make_pair(ctx, caps));
                def = std::find_if(m_contexts.begin(), m_contexts.end(), isDef);
            }

            DE_ASSERT(m_contexts.end() != def);

            defaultContext = def->first;
            checkContext   = def->first;
        }

        // check if context with specified capabilities id already exists
        const auto searchedId = testCase->getRequiredCapabilitiesId();
        for (Item &ctx : m_contexts)
        {
            if (ctx.second->id == searchedId)
            {
                checkContext = ctx.first;
                testCase->delayedInit();
                testCase->checkSupport(*checkContext);
                return checkContext;
            }
        }

        testCase->delayedInit();
        testCase->checkSupport(*checkContext);

        de::SharedPtr<DevCaps::RuntimeData> runtimeData(new DevCaps::RuntimeData);
        de::SharedPtr<DevCaps> caps(new DevCaps(searchedId, this, testContext));

        // Default implementation of TestCase::initDeviceCapabilities() throws
        // in order to enforce creation of DefaultDevice.
        testCase->initDeviceCapabilities(*caps);

        // If we need to create new device with specified capabilities then
        // also we need to make sure that we dont exceed m_maxCustomDevices limit.
        if (vk::Move<vk::VkDevice> dev = createDevice(*caps, *runtimeData); (vk::VkDevice(VK_NULL_HANDLE) != *dev))
        {
            runtimeData->verify();

            de::SharedPtr<Context> ctx(new Context(testContext, m_platformInterface, programs, thiz, dev, caps->id,
                                                   runtimeData, &caps->getPhysicalDeviceExtensions()));
            keepMaxCustomDeviceCount();
            m_contexts.emplace_back(std::make_pair(ctx, caps));

            return m_contexts.back().first;
        }
    }
    catch (const tcu::EnforceDefaultContext &edc)
    {
        DE_UNREF(edc);
        defaultContext = checkContext;
    }
    catch (const tcu::Exception &)
    {
        defaultContext = checkContext;
        throw;
    }

    return checkContext;
}

DevCaps::RuntimeData_::RuntimeData_(const DevCaps &caps)
{
    std::vector<float> queuePriorities;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    resetQueues(caps, queueInfos, queuePriorities);
}

void DevCaps::RuntimeData_::resetQueues(const DevCaps &caps, std::vector<VkDeviceQueueCreateInfo> &infos,
                                        std::vector<float> &priorities)
{
    const ContextManager &mgr = caps.getContextManager();
    uint32_t allQueueCount    = 0;

    const auto &queueCreateInfos = caps.getQueueCreateInfos();
    for (const auto &qci : queueCreateInfos)
        allQueueCount += qci.count;

    infos.clear();
    priorities.clear();
    infos.reserve(queueCreateInfos.size());
    priorities.reserve(allQueueCount);

    familyToQueueIndices.clear();
    familyToQueueIndices.reserve(allQueueCount);

    uint32_t whatever = 0u;
    std::multimap<uint32_t, uint32_t> familyToQueueIndicesMap;

    for (const DevCaps::QueueCreateInfo &qci : queueCreateInfos)
    {
        const uint32_t queueFamilyIndex = findQueueFamilyIndexWithCaps(
            mgr.getInstanceInterface(), mgr.getPhysicalDevice(), qci.required, qci.excluded);

        priorities.emplace_back(qci.priority);

        infos.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, (VkDeviceQueueCreateFlags)0,
                         queueFamilyIndex, qci.count, &priorities.back()});

        familyToQueueIndicesMap.insert({queueFamilyIndex, whatever++});
        uint32_t queueIndexInFamily = uint32_t(familyToQueueIndicesMap.count(queueFamilyIndex) - 1u);
        familyToQueueIndices.emplace_back(queueFamilyIndex, queueIndexInFamily);
        for (uint32_t p = 1u; p < qci.count; ++p)
        {
            priorities.emplace_back(qci.priority);
            familyToQueueIndices.emplace_back(queueFamilyIndex, queueIndexInFamily++);
        }
    }

    DE_ASSERT(priorities.size() == allQueueCount);
    DE_ASSERT(familyToQueueIndices.size() == allQueueCount);
}

#ifdef CTS_USES_VULKANSC
Move<VkDevice> ContextManager::createDevice(const DevCaps &caps, DevCaps::RuntimeData &data) const
{
    const tcu::CommandLine &cmdLine                        = getCommandLine();
    const PlatformInterface &vkp                           = getPlatformInterface();
    de::SharedPtr<vk::ResourceInterface> resourceInterface = getResourceInterface();
    const InstanceInterface &vki                           = getInstanceInterface();
    const VkPhysicalDevice physicalDevice                  = getPhysicalDevice();
    const VkInstance instance                              = getInstanceHandle();
    const uint32_t universalQueueIndex                     = findQueueFamilyIndexWithCaps(
        vki, physicalDevice,
        cmdLine.isComputeOnly() ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    // queues block
    std::vector<float> queuePriorities;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    data.resetQueues(caps, queueInfos, queuePriorities);

    // extensions block
    const DevCaps::strings &physExtensions = caps.getPhysicalDeviceExtensions();
    std::vector<const char *> extensions(physExtensions.size());
    std::transform(physExtensions.begin(), physExtensions.end(), extensions.begin(), std::mem_fn(&std::string::c_str));

    // device creation block
    VkDeviceCreateInfo deviceParams = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       nullptr, // pNext
                                       (VkDeviceCreateFlags)0,
                                       de::sizeU32(queueInfos),
                                       de::dataOrNull(queueInfos),
                                       0u,
                                       nullptr,
                                       de::sizeU32(extensions),
                                       de::dataOrNull(extensions),
                                       nullptr}; // pEnabledFeatures

    // features block
    DevCaps createCaps(caps);

    // devices created for Vulkan SC must have VkDeviceObjectReservationCreateInfo
    // structure defined in VkDeviceCreateInfo::pNext chain
    VkDeviceObjectReservationCreateInfo dorCI = resetDeviceObjectReservationCreateInfo();
    const bool hasReservationCreateInfo       = createCaps.getFeature(dorCI);
    if (hasReservationCreateInfo == false)
        createCaps.addFeature(dorCI);

    VkPipelineCacheCreateInfo pcCI = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
        0U,                                                       // uintptr_t initialDataSize;
        nullptr                                                   // const void* pInitialData;
    };

    std::vector<VkPipelinePoolSize> poolSizes;
    if (cmdLine.isSubProcess())
    {
        resourceInterface->importPipelineCacheData(vkp, instance, vki, physicalDevice, universalQueueIndex);

        if (hasReservationCreateInfo == false)
        {
            dorCI = resourceInterface->getStatMax();
            createCaps.addFeature(dorCI);
        }

        if (resourceInterface->getCacheDataSize() > 0)
        {
            pcCI.initialDataSize = resourceInterface->getCacheDataSize();
            pcCI.pInitialData    = resourceInterface->getCacheData();
            if (hasReservationCreateInfo == false)
            {
                createCaps.addFeature(&VkDeviceObjectReservationCreateInfo::pipelineCacheCreateInfoCount, 1u);
                createCaps.addFeature(&VkDeviceObjectReservationCreateInfo::pPipelineCacheCreateInfos, &pcCI);
            }
        }

        poolSizes = resourceInterface->getPipelinePoolSizes();
        if (!poolSizes.empty() && (hasReservationCreateInfo == false))
        {
            createCaps.addFeature(&VkDeviceObjectReservationCreateInfo::pipelinePoolSizeCount,
                                  uint32_t(poolSizes.size()));
            createCaps.addFeature(&VkDeviceObjectReservationCreateInfo::pPipelinePoolSizes, poolSizes.data());
        }
    }

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    if (false == createCaps.getFeature(sc10Features))
        createCaps.addFeature(sc10Features);

    if (cmdLine.isSubProcess() && false == createCaps.hasFeature<VkFaultCallbackInfo>())
    {
        const VkFaultCallbackInfo faultCallbackInfo{
            VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO, // VkStructureType sType;
            nullptr,                               // void* pNext;
            0U,                                    // uint32_t faultCount;
            nullptr,                               // VkFaultData* pFaults;
            Context::faultCallbackFunction         // PFN_vkFaultCallbackFunction pfnFaultCallback;
        };
        createCaps.addFeature(faultCallbackInfo);
    }

    DevCaps::FeatureInfo chain;
    VkPhysicalDeviceFeatures2 opt = initVulkanStructure();

    DevCaps::Features features = traverseFeatures(FeatureVisitor::Chaining, createCaps.m_features, chain.sType, chain);

    caps.updateDeviceCreateInfo(deviceParams, &opt, features, chain.address);

    std::vector<VkApplicationParametersEXT> appParams;
    if (readApplicationParameters(appParams, cmdLine, false))
    {
        appendStructurePtrToVulkanChain(&deviceParams.pNext, appParams.data());
    }

    print(caps.m_testContext.getLog(), deviceParams);

    return ::createDevice(vkp, instance, vki, physicalDevice, &deviceParams);
}
#else
Move<VkDevice> ContextManager::createDevice(const DevCaps &caps, DevCaps::RuntimeData &data) const
{
    // queues block
    std::vector<float> queuePriorities;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    data.resetQueues(caps, queueInfos, queuePriorities);

    // extensions block
    const DevCaps::strings &physExtensions = caps.getPhysicalDeviceExtensions();
    std::vector<const char *> extensions(physExtensions.size());
    std::transform(physExtensions.begin(), physExtensions.end(), extensions.begin(), std::mem_fn(&std::string::c_str));

    // device creation block
    VkDeviceCreateInfo deviceParams = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       nullptr, // pNext
                                       (VkDeviceCreateFlags)0,
                                       de::sizeU32(queueInfos),
                                       de::dataOrNull(queueInfos),
                                       0u,
                                       nullptr,
                                       de::sizeU32(extensions),
                                       de::dataOrNull(extensions),
                                       nullptr}; // pEnabledFeatures

    // features block
    DevCaps::FeatureInfo chain;
    VkPhysicalDeviceFeatures2 opt = initVulkanStructure();

    DevCaps::Features features = traverseFeatures(FeatureVisitor::Chaining, caps.m_features, chain.sType, chain);

    caps.updateDeviceCreateInfo(deviceParams, &opt, features, chain.address);

    print(caps.m_testContext.getLog(), deviceParams);

    return createCustomDevice(getCommandLine().isValidationEnabled(), getPlatformInterface(), getInstanceHandle(),
                              getInstanceInterface(), getPhysicalDevice(), &deviceParams, nullptr);
}
#endif // CTS_USES_VULKANSC

DevCaps::QueueInfo DevCaps::RuntimeData_::getQueue(const DeviceInterface &di, vk::VkDevice device, uint32_t queueIndex,
                                                   bool isDefaultContext) const
{
    DE_UNREF(isDefaultContext);
    DE_ASSERT(queueIndex < familyToQueueIndices.size());
    const auto [queueFamilyIndex, queueIndexInFamily] = familyToQueueIndices.at(queueIndex);
    DE_ASSERT(queueIndexInFamily < familyToQueueIndices.size());

    DevCaps::QueueInfo info{VK_NULL_HANDLE, queueFamilyIndex};
    di.getDeviceQueue(device, queueFamilyIndex, queueIndexInFamily, &info.queue);

    return info;
}

void ContextManager::print(tcu::TestLog &log, const VkDeviceCreateInfo &createInfo) const
{
    const std::string logFile = fs::path(m_commandLine.getLogFileName()).filename().string();
    if (logFile.find("devcaps") != std::string::npos)
    {
        auto msg = log << tcu::TestLog::Section("DevCaps", std::string()) << tcu::TestLog::Message;
        printDeviceCreateInfo(msg, createInfo);
        msg << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;
    }
}

template <class Stream>
Stream &printPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures &features, Stream &str, uint32_t indent)
{
    const char endl = '\n';
    const std::string si(indent, ' ');
    if (features.robustBufferAccess)
        str << si << "robustBufferAccess: true" << endl;
    if (features.fullDrawIndexUint32)
        str << si << "fullDrawIndexUint32: true" << endl;
    if (features.imageCubeArray)
        str << si << "imageCubeArray: true" << endl;
    if (features.independentBlend)
        str << si << "independentBlend: true" << endl;
    if (features.geometryShader)
        str << si << "geometryShader: true" << endl;
    if (features.tessellationShader)
        str << si << "tessellationShader: true" << endl;
    if (features.sampleRateShading)
        str << si << "sampleRateShading: true" << endl;
    if (features.dualSrcBlend)
        str << si << "dualSrcBlend: true" << endl;
    if (features.logicOp)
        str << si << "logicOp: true" << endl;
    if (features.multiDrawIndirect)
        str << si << "multiDrawIndirect: true" << endl;
    if (features.drawIndirectFirstInstance)
        str << si << "drawIndirectFirstInstance: true" << endl;
    if (features.depthClamp)
        str << si << "depthClamp: true" << endl;
    if (features.depthBiasClamp)
        str << si << "depthBiasClamp: true" << endl;
    if (features.fillModeNonSolid)
        str << si << "fillModeNonSolid: true" << endl;
    if (features.depthBounds)
        str << si << "depthBounds: true" << endl;
    if (features.wideLines)
        str << si << "wideLines: true" << endl;
    if (features.largePoints)
        str << si << "largePoints: true" << endl;
    if (features.alphaToOne)
        str << si << "alphaToOne: true" << endl;
    if (features.multiViewport)
        str << si << "multiViewport: true" << endl;
    if (features.samplerAnisotropy)
        str << si << "samplerAnisotropy: true" << endl;
    if (features.textureCompressionETC2)
        str << si << "textureCompressionETC2: true" << endl;
    if (features.textureCompressionASTC_LDR)
        str << si << "textureCompressionASTC_LDR: true" << endl;
    if (features.textureCompressionBC)
        str << si << "textureCompressionBC: true" << endl;
    if (features.occlusionQueryPrecise)
        str << si << "occlusionQueryPrecise: true" << endl;
    if (features.pipelineStatisticsQuery)
        str << si << "pipelineStatisticsQuery: true" << endl;
    if (features.vertexPipelineStoresAndAtomics)
        str << si << "vertexPipelineStoresAndAtomics: true" << endl;
    if (features.fragmentStoresAndAtomics)
        str << si << "fragmentStoresAndAtomics: true" << endl;
    if (features.shaderTessellationAndGeometryPointSize)
        str << si << "shaderTessellationAndGeometryPointSize: true" << endl;
    if (features.shaderImageGatherExtended)
        str << si << "shaderImageGatherExtended: true" << endl;
    if (features.shaderStorageImageExtendedFormats)
        str << si << "shaderStorageImageExtendedFormats: true" << endl;
    if (features.shaderStorageImageMultisample)
        str << si << "shaderStorageImageMultisample: true" << endl;
    if (features.shaderStorageImageReadWithoutFormat)
        str << si << "shaderStorageImageReadWithoutFormat: true" << endl;
    if (features.shaderStorageImageWriteWithoutFormat)
        str << si << "shaderStorageImageWriteWithoutFormat: true" << endl;
    if (features.shaderUniformBufferArrayDynamicIndexing)
        str << si << "shaderUniformBufferArrayDynamicIndexing: true" << endl;
    if (features.shaderSampledImageArrayDynamicIndexing)
        str << si << "shaderSampledImageArrayDynamicIndexing: true" << endl;
    if (features.shaderStorageBufferArrayDynamicIndexing)
        str << si << "shaderStorageBufferArrayDynamicIndexing: true" << endl;
    if (features.shaderStorageImageArrayDynamicIndexing)
        str << si << "shaderStorageImageArrayDynamicIndexing: true" << endl;
    if (features.shaderClipDistance)
        str << si << "shaderClipDistance: true" << endl;
    if (features.shaderCullDistance)
        str << si << "shaderCullDistance: true" << endl;
    if (features.shaderFloat64)
        str << si << "shaderFloat64: true" << endl;
    if (features.shaderInt64)
        str << si << "shaderInt64: true" << endl;
    if (features.shaderInt16)
        str << si << "shaderInt16: true" << endl;
    if (features.shaderResourceResidency)
        str << si << "shaderResourceResidency: true" << endl;
    if (features.shaderResourceMinLod)
        str << si << "shaderResourceMinLod: true" << endl;
    if (features.sparseBinding)
        str << si << "sparseBinding: true" << endl;
    if (features.sparseResidencyBuffer)
        str << si << "sparseResidencyBuffer: true" << endl;
    if (features.sparseResidencyImage2D)
        str << si << "sparseResidencyImage2D: true" << endl;
    if (features.sparseResidencyImage3D)
        str << si << "sparseResidencyImage3D: true" << endl;
    if (features.sparseResidency2Samples)
        str << si << "sparseResidency2Samples: true" << endl;
    if (features.sparseResidency4Samples)
        str << si << "sparseResidency4Samples: true" << endl;
    if (features.sparseResidency8Samples)
        str << si << "sparseResidency8Samples: true" << endl;
    if (features.sparseResidency16Samples)
        str << si << "sparseResidency16Samples: true" << endl;
    if (features.sparseResidencyAliased)
        str << si << "sparseResidencyAliased: true" << endl;
    if (features.variableMultisampleRate)
        str << si << "variableMultisampleRate: true" << endl;
    if (features.inheritedQueries)
        str << si << "inheritedQueries: true" << endl;
    return str;
}

template <typename Stream>
Stream &printDeviceCreateInfo(Stream &str, const VkDeviceCreateInfo &createInfo)
{
    const char endl = '\n';
    str << "Trying to create logical device" << endl;
    str << "      enabledLayerCount:     " << createInfo.enabledLayerCount << endl;
    for (uint32_t i = 0; i < createInfo.enabledLayerCount; ++i)
    {
        str << "        " << i << ": " << createInfo.ppEnabledLayerNames[i] << endl;
    }
    str << "      enabledExtensionCount: " << createInfo.enabledExtensionCount << endl;
    for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
    {
        str << "        " << i << ": " << createInfo.ppEnabledExtensionNames[i] << endl;
    }
    if (createInfo.pEnabledFeatures)
    {
        str << "      pEnabledFeatures: VkPhysicalDeviceFeatures" << endl;
        printPhysicalDeviceFeatures(*createInfo.pEnabledFeatures, str, 10);
    }
    else
    {
        str << "      pEnabledFeatures: nullptr" << endl;
    }
    if (createInfo.pNext)
    {
        int indent        = 0;
        const void *pNext = createInfo.pNext;
        str << "      pNext: ";
        while (pNext)
        {
            const VkBaseOutStructure *base = static_cast<const VkBaseOutStructure *>(pNext);
            if (indent)
                str << std::string(13, ' ');
            str << getStructureTypeName(base->sType) << endl;
            if (base->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            {
                const VkPhysicalDeviceFeatures2 *f20 = static_cast<const VkPhysicalDeviceFeatures2 *>(pNext);
                const VkPhysicalDeviceFeatures &f10  = f20->features;
                str << "             features: {\n";
                printPhysicalDeviceFeatures(f10, str, 15);
                str << "             )\n";
            }

            pNext  = base->pNext;
            indent = 1;
        }
    }
    else
    {
        str << "      pNext: nullptr" << endl;
    }

    return str;
}

} // namespace vkt
