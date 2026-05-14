#ifndef _VKTCUSTOMINSTANCESDEVICES_HPP
#define _VKTCUSTOMINSTANCESDEVICES_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Auxiliary functions to help create custom devices and instances.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"

#include <vector>
#include <memory>

namespace vk
{
class PlatformInterface;
class InstanceInterface;
} // namespace vk

namespace tcu
{
class CommandLine;
}

namespace vkt
{

std::vector<const char *> getValidationLayers(const vk::PlatformInterface &vkp);

std::vector<const char *> getValidationLayers(const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice);

class CustomInstance
{
public:
    CustomInstance();
#ifndef CTS_USES_VULKANSC
    CustomInstance(Context &context, vk::Move<vk::VkInstance> instance,
                   std::unique_ptr<vk::DebugReportRecorder> &recorder);
#else
    CustomInstance(Context &context, vk::Move<vk::VkInstance> instance);
#endif // CTS_USES_VULKANSC
    CustomInstance(CustomInstance &&other);
    ~CustomInstance();
    CustomInstance &operator=(CustomInstance &&other);
    operator vk::VkInstance() const;
    vk::VkInstance operator*() const;
    void swap(CustomInstance &other);
    const vk::InstanceDriver &getDriver() const;
    vk::VkPhysicalDevice getPhysicalDevice() const;
    void collectMessages();

    CustomInstance(const CustomInstance &other)            = delete;
    CustomInstance &operator=(const CustomInstance &other) = delete;

private:
    friend class InstanceWrapper;

    Context *m_context;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::DebugReportRecorder> m_recorder;
#endif // CTS_USES_VULKANSC
    vk::Move<vk::VkInstance> m_instance;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::InstanceDriver> m_driver;
    vk::Move<vk::VkDebugUtilsMessengerEXT> m_callback;
#else
    std::unique_ptr<vk::InstanceDriverSC> m_driver;
#endif // CTS_USES_VULKANSC
};

class UncheckedInstance
{
public:
    UncheckedInstance();
#ifndef CTS_USES_VULKANSC
    UncheckedInstance(Context &context, vk::VkInstance instance, const vk::VkAllocationCallbacks *pAllocator,
                      std::unique_ptr<vk::DebugReportRecorder> &recorder);
#else
    UncheckedInstance(Context &context, vk::VkInstance instance, const vk::VkAllocationCallbacks *pAllocator);
#endif // CTS_USES_VULKANSC
    UncheckedInstance(UncheckedInstance &&other);
    ~UncheckedInstance();
    UncheckedInstance &operator=(UncheckedInstance &&other);
    operator vk::VkInstance() const;
    vk::VkInstance operator*() const;
    operator bool() const;
    void swap(UncheckedInstance &other);
    const vk::InstanceDriver &getDriver() const;
    vk::VkPhysicalDevice getPhysicalDevice() const;

    UncheckedInstance(const UncheckedInstance &other)            = delete;
    UncheckedInstance &operator=(const UncheckedInstance &other) = delete;

private:
    friend class InstanceWrapper;

    Context *m_context;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::DebugReportRecorder> m_recorder;
#endif // CTS_USES_VULKANSC
    const vk::VkAllocationCallbacks *m_allocator;
    vk::VkInstance m_instance;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::InstanceDriver> m_driver;
    vk::Move<vk::VkDebugUtilsMessengerEXT> m_callback;
#else
    std::unique_ptr<vk::InstanceDriverSC> m_driver;
#endif // CTS_USES_VULKANSC
};

class InstanceWrapper;

class CustomDevice
{
public:
    CustomDevice();
    CustomDevice(CustomDevice &&other);
    ~CustomDevice();
    CustomDevice &operator=(CustomDevice &&other);
    operator vk::VkDevice() const;
    vk::VkDevice operator*() const;
    void swap(CustomDevice &other);
    vk::VkPhysicalDevice getPhysicalDevice() const;
    const vk::InstanceInterface &getInstanceDriver() const;
    const vk::DeviceDriver &getDriver() const;
    vk::Allocator &getAllocator() const;

    CustomDevice(const CustomDevice &other)            = delete;
    CustomDevice &operator=(const CustomDevice &other) = delete;

private:
    friend class DeviceWrapper;

    // Custom devices must always be created through the InstanceWrapper wrapper
    friend class InstanceWrapper;
    CustomDevice(Context &context, const InstanceWrapper &instance, vk::VkPhysicalDevice physicalDevice,
                 vk::Move<vk::VkDevice> device);

    Context *m_context;
    const vk::InstanceInterface *m_instanceDriver;
    vk::VkPhysicalDevice m_physicalDevice;
    vk::Move<vk::VkDevice> m_device;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::DeviceDriver> m_driver;
#else
    std::unique_ptr<vk::DeviceDriverSC> m_driver;
#endif // CTS_USES_VULKANSC
    std::unique_ptr<vk::SimpleAllocator> m_allocator;
};

class UncheckedDevice
{
public:
    UncheckedDevice();
    UncheckedDevice(UncheckedDevice &&other);
    ~UncheckedDevice();
    UncheckedDevice &operator=(UncheckedDevice &&other);
    operator vk::VkDevice() const;
    vk::VkDevice operator*() const;
    operator bool() const;
    void swap(UncheckedDevice &other);
    vk::VkPhysicalDevice getPhysicalDevice() const;
    const vk::InstanceInterface &getInstanceDriver() const;
    const vk::DeviceDriver &getDriver() const;
    vk::Allocator &getAllocator() const;

    UncheckedDevice(const UncheckedDevice &other)            = delete;
    UncheckedDevice &operator=(const UncheckedDevice &other) = delete;

private:
    friend class DeviceWrapper;

    // Unchecked devices must always be created through the InstanceWrapper wrapper
    friend class InstanceWrapper;
    UncheckedDevice(Context &context, const InstanceWrapper &instance, vk::VkPhysicalDevice physicalDevice,
                    vk::VkDevice device, const vk::VkAllocationCallbacks *pAllocator);

    Context *m_context;
    const vk::InstanceInterface *m_instanceDriver;
    vk::VkPhysicalDevice m_physicalDevice;
    const vk::VkAllocationCallbacks *m_allocationCallbacks;
    vk::Move<vk::VkDevice> m_device;
#ifndef CTS_USES_VULKANSC
    std::unique_ptr<vk::DeviceDriver> m_driver;
#else
    std::unique_ptr<vk::DeviceDriverSC> m_driver;
#endif // CTS_USES_VULKANSC
    std::unique_ptr<vk::SimpleAllocator> m_allocator;
};

// This helper is a special wrapper for an instance that is capable of creating of a custom device.
// This can hold or refer to either a custom instance, unchecked instance or the default instance.
// In case of Vulkan SC, due to some implementations supporting only a single VkDevice per VkInstance
// a custom instance is necessary for each custom device.
//
// Previously, there were two problems when dealing with custom devices:
// - some cases did not create a custom instance, making the test incompatible with such SC implementations
// - some cases create a custom instance even when unnecessary, penalizing Vulkan CTS execution time
//
// This wrapper exists to solve this by wrapping the default instance or a custom/unchecked instance as follows:
// - if a custom/unchecked instance is specified when constructing it, then that custom instance will be used
// - in case of Vulkan SC, if a custom/unchecked instance is not specified, one is created automatically with the
//   default parameters for compatibility with SC implementations supporting a single VkDevice per VkInstance
// - in case of Vulkan, if a custom/unchecked instance is not specified, then the default instance will be used
//   therefore improving performance by avoiding having to create a custom instance
class InstanceWrapper
{
public:
    InstanceWrapper();
    InstanceWrapper(Context &context);
    InstanceWrapper(CustomInstance &&customInstance);
    InstanceWrapper(const CustomInstance &customInstance);
    InstanceWrapper(UncheckedInstance &&uncheckedInstance);
    InstanceWrapper(const UncheckedInstance &uncheckedInstance);
    InstanceWrapper(InstanceWrapper &&other);
    ~InstanceWrapper();
    InstanceWrapper &operator=(InstanceWrapper &&other);
    operator vk::VkInstance() const;
    vk::VkInstance operator*() const;
    void swap(InstanceWrapper &other);
    const vk::InstanceInterface &getDriver() const;
    vk::VkPhysicalDevice getPhysicalDevice() const;

    CustomDevice createCustomDevice(const vk::VkDeviceCreateInfo *pCreateInfo,
                                    const vk::VkAllocationCallbacks *pAllocator = nullptr) const;
    CustomDevice createCustomDevice(vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo *pCreateInfo,
                                    const vk::VkAllocationCallbacks *pAllocator = nullptr) const;
    vk::VkResult createUncheckedDevice(const vk::VkDeviceCreateInfo *pCreateInfo,
                                       const vk::VkAllocationCallbacks *pAllocator, UncheckedDevice *pDevice) const;
    vk::VkResult createUncheckedDevice(vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo *pCreateInfo,
                                       const vk::VkAllocationCallbacks *pAllocator, UncheckedDevice *pDevice) const;

    InstanceWrapper(const InstanceWrapper &other)            = delete;
    InstanceWrapper &operator=(const InstanceWrapper &other) = delete;

private:
    vk::VkResult createDeviceInternal(vk::VkPhysicalDevice physicalDevice, const vk::VkDeviceCreateInfo *pCreateInfo,
                                      const vk::VkAllocationCallbacks *pAllocator, vk::VkDevice *pDevice) const;

    Context *m_context;

    // These are the custom/unchecked instance objects stored when they are passed by move semantics.
    // When the constructors taking custom/unchecked instance objects by reference are called, these remain
    // empty and the custom/unchecked instance objects passed in by reference are expected to remain alive
    // for the duration of the lifetime of this wrapper.
    CustomInstance m_customInstance;
    UncheckedInstance m_uncheckedInstance;

    // Note that these are intentionally not "owned" values (i.e. not Move<VkInstance> and not unique_ptr<>)
    // as they are either owned by the context (if default instance) or by the custom/unchecked instance.
    vk::VkInstance m_instance;
    const vk::InstanceInterface *m_driver;
};

// This helper is a special wrapper for a device.
// This can hold or refer to either a custom device, unchecked device or the default device.
class DeviceWrapper
{
public:
    DeviceWrapper();
    DeviceWrapper(Context &context);
    DeviceWrapper(CustomDevice &&customDevice);
    DeviceWrapper(const CustomDevice &customDevice);
    DeviceWrapper(UncheckedDevice &&uncheckedDevice);
    DeviceWrapper(const UncheckedDevice &uncheckedDevice);
    DeviceWrapper(DeviceWrapper &&other);
    ~DeviceWrapper();
    DeviceWrapper &operator=(DeviceWrapper &&other);
    operator vk::VkDevice() const;
    vk::VkDevice operator*() const;
    void swap(DeviceWrapper &other);
    vk::VkPhysicalDevice getPhysicalDevice() const;
    const vk::InstanceInterface &getInstanceDriver() const;
    const vk::DeviceInterface &getDriver() const;
    vk::Allocator &getAllocator() const;

    DeviceWrapper(const DeviceWrapper &other)            = delete;
    DeviceWrapper &operator=(const DeviceWrapper &other) = delete;

private:
    Context *m_context;

    const vk::InstanceInterface *m_instanceDriver;
    vk::VkPhysicalDevice m_physicalDevice;

    // These are the custom/unchecked device objects stored when they are passed by move semantics.
    // When the constructors taking custom/unchecked device objects by reference are called, these remain
    // empty and the custom/unchecked device objects passed in by reference are expected to remain alive
    // for the duration of the lifetime of this wrapper.
    CustomDevice m_customDevice;
    UncheckedDevice m_uncheckedDevice;

    // Note that these are intentionally not "owned" values (i.e. not Move<VkDevice> and not unique_ptr<>)
    // as they are either owned by the context (if default device) or by the custom/unchecked device.
    vk::VkDevice m_device;
    const vk::DeviceInterface *m_driver;
    vk::Allocator *m_allocator;
};

// Custom instances.

CustomInstance createCustomInstanceWithExtensions(Context &context, const std::vector<std::string> &extension,
                                                  const vk::VkAllocationCallbacks *pAllocator = nullptr,
                                                  bool allowLayers                            = true);

CustomInstance createCustomInstanceWithExtension(Context &context, const std::string &extension,
                                                 const vk::VkAllocationCallbacks *pAllocator = nullptr,
                                                 bool allowLayers                            = true);

CustomInstance createCustomInstanceFromContext(Context &context, const vk::VkAllocationCallbacks *pAllocator = nullptr,
                                               bool allowLayers = true);

CustomInstance createCustomInstanceFromInfo(Context &context, const vk::VkInstanceCreateInfo *instanceCreateInfo,
                                            const vk::VkAllocationCallbacks *pAllocator = nullptr,
                                            bool allowLayers                            = true);

// Unchecked instance: creation allowed to fail.

vk::VkResult createUncheckedInstance(Context &context, const vk::VkInstanceCreateInfo *instanceCreateInfo,
                                     const vk::VkAllocationCallbacks *pAllocator, UncheckedInstance *instance,
                                     bool allowLayers = true);

class VideoDevice
{
public:
#ifndef CTS_USES_VULKANSC
    typedef vk::VkVideoCodecOperationFlagsKHR VideoCodecOperationFlags;
#else
    typedef uint32_t VideoCodecOperationFlags;
#endif // CTS_USES_VULKANSC

    enum VideoDeviceFlagBits
    {
        VIDEO_DEVICE_FLAG_NONE                                 = 0,
        VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_DECODE_SUPPORT = 0x00000001,
        VIDEO_DEVICE_FLAG_REQUIRE_YCBCR_OR_NOT_SUPPORTED       = 0x00000002,
        VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED       = 0x00000004,
        VIDEO_DEVICE_FLAG_REQUIRE_TIMELINE_OR_NOT_SUPPORTED    = 0x00000008,
        VIDEO_DEVICE_FLAG_QUERY_WITH_STATUS_FOR_ENCODE_SUPPORT = 0x00000010,
        VIDEO_DEVICE_FLAG_REQUIRE_MAINTENANCE_1                = 0x00000020,
        VIDEO_DEVICE_FLAG_REQUIRE_QUANTIZATION_MAP             = 0x00000040,
        VIDEO_DEVICE_FLAG_REQUIRE_MAINTENANCE_2                = 0x00000080,
        VIDEO_DEVICE_FLAG_REQUIRE_DECODE_VP9                   = 0x00000100,
        VIDEO_DEVICE_FLAG_REQUIRE_INTRA_REFRESH                = 0x00000200,
        VIDEO_DEVICE_FLAG_REQUIRE_UNIFIED_IMAGE_LAYOUTS        = 0x00000400,
    };

    typedef uint32_t VideoDeviceFlags;

    static void checkSupport(Context &context, const VideoCodecOperationFlags videoCodecOperation);
    static vk::VkQueueFlags getQueueFlags(const VideoCodecOperationFlags videoCodecOperationFlags);

    static void addVideoDeviceExtensions(std::vector<const char *> &deviceExtensions, const uint32_t apiVersion,
                                         const vk::VkQueueFlags queueFlagsRequired,
                                         const VideoCodecOperationFlags videoCodecOperationFlags);
    static bool isVideoEncodeOperation(const VideoCodecOperationFlags videoCodecOperationFlags);
    static bool isVideoDecodeOperation(const VideoCodecOperationFlags videoCodecOperationFlags);
    static bool isVideoOperation(const VideoCodecOperationFlags videoCodecOperationFlags);

    VideoDevice(Context &context);
    VideoDevice(Context &context, const VideoCodecOperationFlags videoCodecOperation,
                const VideoDeviceFlags videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
    virtual ~VideoDevice(void);

    vk::VkDevice getDeviceSupportingQueue(const vk::VkQueueFlags queueFlagsRequired               = 0,
                                          const VideoCodecOperationFlags videoCodecOperationFlags = 0,
                                          const VideoDeviceFlags videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
    bool createDeviceSupportingQueue(const vk::VkQueueFlags queueFlagsRequired,
                                     const VideoCodecOperationFlags videoCodecOperationFlags,
                                     const VideoDeviceFlags videoDeviceFlags = VIDEO_DEVICE_FLAG_NONE);
    const vk::DeviceInterface &getDeviceDriver(void);
    uint32_t getQueueFamilyIndexTransfer(void) const;
    uint32_t getQueueFamilyIndexDecode(void) const;
    uint32_t getQueueFamilyIndexEncode(void) const;
    uint32_t getQueueFamilyVideo(void) const;
    vk::Allocator &getAllocator(void);

protected:
    Context &m_context;

    const InstanceWrapper m_instance;
    DeviceWrapper m_logicalDevice;
    uint32_t m_queueFamilyTransfer;
    uint32_t m_queueFamilyDecode;
    uint32_t m_queueFamilyEncode;
    VideoCodecOperationFlags m_videoCodecOperation;
};

} // namespace vkt

#endif // _VKTCUSTOMINSTANCESDEVICES_HPP
