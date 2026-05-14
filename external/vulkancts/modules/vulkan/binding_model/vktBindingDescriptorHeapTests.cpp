/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 NVIDIA Corporation.
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
 * \file vktBindingDescriptorHeapTests.cpp
 * \brief Descriptor heap (extension) tests
 *//*--------------------------------------------------------------------*/

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <functional>
#include <list>

#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktBindingDescriptorBufferTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace BindingModel
{
namespace
{

using namespace vk;
using de::MovePtr;

const int kMaxDescriptor    = 128;
const int kShaderRecordSize = 256;

const int kRayTraceDefaultHitValue = 0xcccc;

struct Buffer
{
    Move<VkBuffer> buffer;
    MovePtr<Allocation> memory;
    VkDeviceAddress address{};
};

struct Image
{
    Move<VkImage> image;
    MovePtr<Allocation> memory;
};

struct ShaderBinding
{
    VkDescriptorType descriptorType             = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    int descriptorSet                           = 0;
    int firstBinding                            = 0;
    int imageBindingUid                         = 0;
    int imageBindingArrayIndex                  = -1; // <0 is non-arrayed
    int samplerHeapIndex                        = 0;
    bool arrayed                                = false;
    bool nullDescriptor                         = false;
    bool samplerIsNull                          = false; // Signal sampled images not to expect a non-zero result
    bool combinedImageSamplerHandle             = false;
    bool shiftSamplerResult                     = false;
    VkDescriptorSetAndBindingMappingEXT mapping = initVulkanStructure();
    int heapIndex                               = -1; // <0 is firstBinding
    int queue                                   = 0;
};

struct TestParams
{
    bool enableRayTracing                                   = false;
    bool enableRayQuery                                     = false;
    bool enableAccelerationStructures                       = false;
    bool enableAccelerationStructuresCaptureReplay          = false;
    bool enableSamplerYcbcrConversion                       = false;
    bool enableGraphicsPipelineLibrary                      = false;
    bool enableShaderObject                                 = false;
    bool enableDynamicRendering                             = false;
    bool enableVertexPipelineStoresAndAtomics               = false;
    bool enableFragmentStoresAndAtomics                     = false;
    bool enableNVCommandBufferInheritance                   = false;
    bool enableCaptureReplay                                = false;
    bool enableCustomBorderColor                            = false;
    bool enableNullDescriptor                               = false;
    bool enableSampledImageArrayNonUniformIndexing          = false;
    bool enableStorageImageArrayNonUniformIndexing          = false;
    bool enableUniformTexelBufferArrayNonUniformIndexing    = false;
    bool enableStorageTexelBufferArrayNonUniformIndexing    = false;
    bool enableUniformBufferArrayNonUniformIndexing         = false;
    bool enableStorageBufferArrayNonUniformIndexing         = false;
    bool enableTessellationShader                           = false;
    bool enableGeometryShader                               = false;
    bool enableMeshShader                                   = false;
    bool enableTaskShader                                   = false;
    bool enableMaintenance4                                 = false;
    bool enableRuntimeDescriptorArray                       = false;
    bool enablePushDescriptors                              = false;
    bool enableSampleRateShading                            = false;
    bool enableVariablePointers                             = false;
    bool shaderImageInt64Atomics                            = false;
    bool enableSparseHeap                                   = false;
    bool enableProtectedHeap                                = false;
    bool enableShader64bitIndexing                          = false;
    bool enableShaderUniformTexelBufferArrayDynamicIndexing = false;
    bool enableShaderStorageTexelBufferArrayDynamicIndexing = false;
    VkQueueFlagBits queue                                   = VK_QUEUE_COMPUTE_BIT;
    uint32_t seed                                           = 0;
    uint32_t queueCount                                     = 1;
};

struct TestParamsBasic : TestParams
{
    VkShaderStageFlagBits stage{};
    int dimension{};
    std::vector<ShaderBinding> bindings;
    std::vector<std::pair<uint32_t, uint32_t>> pushData;
    bool embeddedSamplers{};
    bool bindSamplerHeap = true;
    bool inputAttachments{};
    bool scaledMappingStrides      = true;
    int32_t overrideResourceStride = -1;
    int32_t overrideSamplerStride  = -1;
};

struct TestParamsGPL : TestParams
{
    bool unbindFragShader = false;
};

struct TestParamsWithDescriptorType : TestParams
{
    VkDescriptorType descriptorType{};
};

struct TestParamsGraphics : TestParams
{
    bool useFragmentShader         = false;
    bool useSecondaryCommandBuffer = false;
    bool useVectors                = false;
};

enum class SpirvTestType
{
    SizeOf,
    SizeOf64,
    UntypedStorageBuffer,
    UntypedArrayLength,
    SimpleStorageTexelBuffer,
    UntypedImageTexelPointer,
    SimpleSamplerHeap,
    FunctionCallBinding,
    FunctionCallBindingForward,
    StorageTexelBufferAtomic64,
    SimpleVariablePointers,
    ArrayVariablePointers,
    AtomicImageWithinFunction,
};

struct TestParamsSpirv : TestParams
{
    SpirvTestType spirvTestType{};
};

enum class CopyMethod
{
    BufferToImage,
    CopyImage,
    ImageToBuffer,
    ClearColorImage,
    BlitImage,
};

struct TestParamsReservedHeap
{
    uint32_t seed         = 0;
    uint32_t imageExtent  = 16;
    CopyMethod copyMethod = CopyMethod::BufferToImage;
    bool bindResourceHeap = false;
    bool bindSamplerHeap  = false;
};

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    DE_ASSERT(deIsPowerOfTwo64(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}

VkDeviceSize getBufferDescriptorStride(const VkPhysicalDeviceDescriptorHeapPropertiesEXT &properties)
{
    return alignUp(properties.bufferDescriptorSize, properties.bufferDescriptorAlignment);
}

VkDeviceSize getImageDescriptorStride(const VkPhysicalDeviceDescriptorHeapPropertiesEXT &properties)
{
    return alignUp(properties.imageDescriptorSize, properties.imageDescriptorAlignment);
}

VkDeviceSize getSamplerDescriptorStride(const VkPhysicalDeviceDescriptorHeapPropertiesEXT &properties)
{
    return alignUp(properties.samplerDescriptorSize, properties.samplerDescriptorAlignment);
}

VkDeviceSize getResourceDescriptorStride(const VkPhysicalDeviceDescriptorHeapPropertiesEXT &properties)
{
    return de::max(getBufferDescriptorStride(properties), getImageDescriptorStride(properties));
}

std::unique_ptr<Buffer> createBufferAndMemory(const vk::InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                              const vk::DeviceInterface &vkd,
                                              const VkPhysicalDeviceMemoryProperties &memoryProperties, VkDevice device,
                                              VkDeviceSize size, VkBufferUsageFlags2KHR usage)
{
    VkBufferUsageFlags2CreateInfoKHR createInfoUsageFlags2 = initVulkanStructure();
    createInfoUsageFlags2.usage                            = usage;

    VkBufferCreateInfo createInfo    = initVulkanStructure();
    createInfo.pNext                 = &createInfoUsageFlags2;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = static_cast<VkBufferUsageFlags>(usage);
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    auto handle        = createBuffer(vkd, device, &createInfo);
    auto bufferMemReqs = getBufferMemoryRequirements(vkd, device, *handle);

    auto memReqs    = MemoryRequirement::HostVisible;
    auto compatMask = bufferMemReqs.memoryTypeBits & getCompatibleMemoryTypes(memoryProperties, memReqs);
    DE_ASSERT(compatMask != 0);
    static_cast<void>(compatMask);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = initVulkanStructure();
    allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    auto memory = allocateExtended(vki, vkd, physicalDevice, device, bufferMemReqs, memReqs, &allocFlagsInfo);
    vkd.bindBufferMemory(device, *handle, memory->getMemory(), memory->getOffset());

    VkDeviceAddress address = 0;
    if ((usage & VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR) != 0)
    {
        VkBufferDeviceAddressInfo bdaInfo = initVulkanStructure();
        bdaInfo.buffer                    = *handle;
        address                           = vkd.getBufferDeviceAddress(device, &bdaInfo);
    }

    auto result     = std::make_unique<Buffer>();
    result->buffer  = std::move(handle);
    result->memory  = std::move(memory);
    result->address = address;
    return result;
}

std::unique_ptr<Image> createImageAndMemory(const vk::InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                            const vk::DeviceInterface &vkd,
                                            const VkPhysicalDeviceMemoryProperties &memoryProperties, VkDevice device,
                                            const VkImageCreateInfo &createInfo)
{
    const bool captureReplay = (createInfo.flags & VK_IMAGE_CREATE_DESCRIPTOR_HEAP_CAPTURE_REPLAY_BIT_EXT) != 0;

    auto handle       = createImage(vkd, device, &createInfo);
    auto imageMemReqs = getImageMemoryRequirements(vkd, device, *handle);
    auto memReqs      = captureReplay ? MemoryRequirement::DeviceAddressCaptureReplay : MemoryRequirement::Any;
    auto compatMask   = imageMemReqs.memoryTypeBits & getCompatibleMemoryTypes(memoryProperties, memReqs);
    DE_ASSERT(compatMask != 0);
    static_cast<void>(compatMask);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = initVulkanStructure();
    if (captureReplay)
    {
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
    }

    auto memory = allocateExtended(vki, vkd, physicalDevice, device, imageMemReqs, memReqs, &allocFlagsInfo);

    vkd.bindImageMemory(device, *handle, memory->getMemory(), memory->getOffset());

    auto result    = std::make_unique<Image>();
    result->image  = std::move(handle);
    result->memory = std::move(memory);
    return result;
}

class DescriptorHeapTestInstanceBase : public TestInstance
{
public:
    explicit DescriptorHeapTestInstanceBase(Context &context, const TestParams &params);

protected:
    const ProgramBinary &getShaderBinary(const std::string &name) const
    {
        return m_context.getBinaryCollection().get(name);
    }

    std::unique_ptr<Buffer> createBufferAndMemory(VkDeviceSize size, VkBufferUsageFlags2KHR usage)
    {
        return vkt::BindingModel::createBufferAndMemory(m_instance.getDriver(), m_physDevice, m_device.getDriver(),
                                                        m_memoryProperties, *m_device, size, usage);
    }

    std::unique_ptr<Image> createImageAndMemory(const VkImageCreateInfo &createInfo)
    {
        return vkt::BindingModel::createImageAndMemory(m_instance.getDriver(), m_physDevice, m_device.getDriver(),
                                                       m_memoryProperties, *m_device, createInfo);
    }

    const InstanceWrapper m_instance;
    VkPhysicalDevice m_physDevice{};
    DeviceWrapper m_device;
    std::vector<VkQueue> m_queues;
    uint32_t m_queueFamilyIndex{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    VkPhysicalDeviceDescriptorHeapPropertiesEXT m_descriptorHeapProperties{};
};

class DescriptorHeapTestInstanceBasic final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceBasic(Context &context, const TestParamsBasic &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    std::vector<ShaderBinding> mapShaderBindings(uint32_t resourceStride, uint32_t samplerStride, uint32_t queueIndex);
    void writeEmbeddedSamplers(std::vector<ShaderBinding> &bindings, const VkSamplerCreateInfo *samplerCreateInfo);
    void setupDescriptors(VkCommandBuffer cmdBuf, const ShaderBinding &binding, uint32_t resourceStride,
                          uint32_t heapIndexStride, uint32_t samplerStride, VkImage prePassImage,
                          const std::vector<uint32_t> &swizzlerData, uint32_t prePassClearColor,
                          char *resourceDescriptorHeapHostPtr, char *samplerDescriptorHeapHostPtr, de::Random &rnd,
                          std::vector<int32_t> &expectedResult);

    VkPipeline initComputePipeline(const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
                                   uint32_t queueIndex);
    VkPipeline initGraphicsPipeline(const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
                                    VkRenderPass renderPass, uint32_t queueIndex);
    VkPipeline initRayTracingPipeline(const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
                                      uint32_t queueIndex);

    VkRenderPass initRenderPass();
    std::pair<VkImage, VkImageView> initPrePassRenderTarget();
    std::pair<VkImage, VkImageView> initRenderTarget();
    VkFramebuffer initFramebuffer(VkRenderPass renderPass, VkImageView renderTargetImageView,
                                  VkImageView prePassImageView);

    void addRayTracingShader(const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
                             RayTracingPipeline *rayTracingPipeline, VkShaderStageFlagBits stage,
                             const std::string &name, uint32_t group);
    de::MovePtr<BufferWithMemory> createShaderBindingTable(const InstanceInterface &vki, const DeviceInterface &vkd,
                                                           VkDevice device, VkPhysicalDevice physicalDevice,
                                                           VkPipeline pipeline, Allocator &allocator,
                                                           RayTracingPipeline *rayTracingPipeline, uint32_t group,
                                                           uint32_t shaderGroupCount,
                                                           VkStridedDeviceAddressRegionKHR &shaderBindingTableRegion);

    VkImageViewCreateInfo createTestImageViewInfo(int32_t value, VkCommandBuffer cmdBuf);

    TestParamsBasic m_params{};

    std::unique_ptr<Buffer> m_indirectAddressBuffer;
    std::vector<char> m_deferredIndirectAddressBuffer;

    std::list<de::MovePtr<RayTracingPipeline>> m_rayTracingPipelines;
    std::list<de::MovePtr<BufferWithMemory>> m_stagingSBTBuffers;

    VkStridedDeviceAddressRegionKHR m_raygenShaderBindingTableRegion{};
    VkStridedDeviceAddressRegionKHR m_missShaderBindingTableRegion{};
    VkStridedDeviceAddressRegionKHR m_hitShaderBindingTableRegion{};
    VkStridedDeviceAddressRegionKHR m_callableShaderBindingTableRegion{};

    std::list<VkShaderDescriptorSetAndBindingMappingInfoEXT> m_rayTracingMappingInfos;

    std::array<char, kShaderRecordSize> m_shaderRecordData{};
    std::vector<uint32_t> m_registeredBorderColors;
    std::list<std::unique_ptr<Buffer>> m_stagingBuffers;
    std::list<std::unique_ptr<Image>> m_stagingImages;
    std::list<Move<VkImageView>> m_stagingImageViews;
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> m_rtBlases;
    std::vector<MovePtr<TopLevelAccelerationStructure>> m_rtTlases;
    std::list<Move<VkRenderPass>> m_stagingRenderPasses;
    std::list<Move<VkFramebuffer>> m_stagingFramebuffers;
    std::list<Move<VkPipeline>> m_stagingPipelines;
    std::list<VkSamplerCustomBorderColorIndexCreateInfoEXT> m_stagingCustomBorderColorIndexCreateInfos;
    std::list<VkSamplerCustomBorderColorCreateInfoEXT> m_stagingCustomBorderColorCreateInfos;
    std::list<VkDeviceAddressRangeEXT> m_stagingDeviceAddressRanges;
    std::list<VkImageDescriptorInfoEXT> m_stagingImageDescriptorInfos;
    std::list<VkImageViewCreateInfo> m_stagingImageViewCreateInfos;
    std::list<VkTexelBufferDescriptorInfoEXT> m_stagingTexelBufferDescriptorInfos;

    std::vector<VkResourceDescriptorInfoEXT> m_deferredResourceDescriptors;
    std::vector<VkHostAddressRangeEXT> m_deferredResourceHostDescriptors;

    std::vector<VkSamplerCreateInfo> m_deferredSamplerDescriptors;
    std::vector<VkHostAddressRangeEXT> m_deferredSamplerHostDescriptors;
};

class DescriptorHeapTestInstanceInvariance final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceInvariance(Context &context, const TestParamsWithDescriptorType &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    VkResult createInvarianceResources(bool capture, bool replay);

    VkResult writeInvarianceDescriptor(std::vector<char> &descriptorData);

    TestParamsWithDescriptorType m_params{};

    VkDeviceAddress m_memoryCaptureAddress{};

    MovePtr<Allocation> m_imageMemory;
    Move<VkImage> m_image;

    std::unique_ptr<Buffer> m_buffer;
    de::SharedPtr<BottomLevelAccelerationStructure> m_rtBlas;
    MovePtr<TopLevelAccelerationStructure> m_rtTlas;
    Move<VkSamplerYcbcrConversion> m_samplerYcbcrConversion;
    std::array<char, 256> m_captureData{};
    VkDeviceAddress m_bufferCaptureAddress{};
    VkDeviceAddress m_captureBlasAddress{};
    VkDeviceAddress m_captureTlasAddress{};
    VkClearColorValue m_customBorderColor{};
    uint32_t m_customBorderColorIndex{};
};

class DescriptorHeapTestInstanceReservedHeap final : public TestInstance
{
public:
    explicit DescriptorHeapTestInstanceReservedHeap(Context &context, const TestParamsReservedHeap &params);

    tcu::TestStatus iterate() override;

private:
    void recordQueueCommandBuffer(VkCommandBuffer cmdBuf, uint32_t globalQueueIndex, uint32_t queueFamily);

    TestParamsReservedHeap m_params{};

    const InstanceWrapper m_instance;
    VkPhysicalDevice m_physDevice{};
    DeviceWrapper m_device;
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    VkPhysicalDeviceDescriptorHeapPropertiesEXT m_descriptorHeapProperties{};

    de::Random m_rnd;

    std::vector<VkQueue> m_queues;
    std::vector<uint32_t> m_queueFamilies;
    std::vector<uint32_t> m_queueCounts;

    std::vector<Move<VkCommandPool>> m_commandPools;
    std::vector<Move<VkCommandBuffer>> m_queueCommandBuffers;

    std::unique_ptr<Image> m_dstImage;
    std::unique_ptr<Buffer> m_srcBuffer;
    std::vector<uint8_t> m_expectedResult;
    VkBindHeapInfoEXT m_resourceDescriptorHeap{};
    VkBindHeapInfoEXT m_samplerDescriptorHeap{};

    Move<VkPipeline> m_atomicCounterPipeline;

    std::unique_ptr<Image> m_colorImage;
    std::list<std::unique_ptr<Buffer>> m_stagingBuffers;
    std::list<std::unique_ptr<Image>> m_stagingImages;
};

class DescriptorHeapTestCaseBase : public TestCase
{
public:
    explicit DescriptorHeapTestCaseBase(tcu::TestContext &testCtx, const std::string &name, TestParams const &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

        if (m_params.enableRayQuery)
        {
            context.requireDeviceFunctionality(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        }
        if (m_params.enableAccelerationStructures)
        {
            context.requireDeviceFunctionality(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        }
        if (m_params.enableAccelerationStructuresCaptureReplay)
        {
            if (!context.getAccelerationStructureFeatures().accelerationStructureCaptureReplay)
            {
                TCU_THROW(NotSupportedError, "accelerationStructureCaptureReplay feature is not supported");
            }
        }
        if (m_params.enableRayTracing)
        {
            context.requireDeviceFunctionality(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        }
        if (m_params.enableSamplerYcbcrConversion)
        {
            context.requireDeviceFunctionality(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        }
        if (m_params.enableGraphicsPipelineLibrary)
        {
            context.requireDeviceFunctionality(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
        }
        if (m_params.enableShaderObject)
        {
            context.requireDeviceFunctionality(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
        }
        if (m_params.enableDynamicRendering)
        {
            context.requireDeviceFunctionality(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
        if (m_params.enableVertexPipelineStoresAndAtomics)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
        }
        if (m_params.enableFragmentStoresAndAtomics)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
        }
        if (m_params.enableNVCommandBufferInheritance)
        {
            context.requireDeviceFunctionality(VK_NV_COMMAND_BUFFER_INHERITANCE_EXTENSION_NAME);
        }
        if (m_params.enableCaptureReplay)
        {
            if (!context.getDescriptorHeapFeaturesEXT().descriptorHeapCaptureReplay)
            {
                TCU_THROW(NotSupportedError, "descriptorHeapCaptureReplay feature is not supported");
            }
        }
        if (m_params.enableCustomBorderColor)
        {
            context.requireDeviceFunctionality(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);

            if (!context.getCustomBorderColorFeaturesEXT().customBorderColors)
            {
                TCU_THROW(NotSupportedError, "customBorderColors feature is not supported");
            }
        }
        if (m_params.enableNullDescriptor)
        {
            context.requireDeviceFunctionality(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);

            // getRobustness2Features queries the wrong extension.
            // if (!context.getRobustness2Features().nullDescriptor)
            // {
            //     TCU_THROW(NotSupportedError, "nullDescriptor feature is not supported");
            // }
        }
        if (m_params.enableSampledImageArrayNonUniformIndexing)
        {
            context.requireDeviceCoreFeature(
                DeviceCoreFeature::DEVICE_CORE_FEATURE_SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING);
        }
        if (m_params.enableStorageImageArrayNonUniformIndexing)
        {
            context.requireDeviceCoreFeature(
                DeviceCoreFeature::DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING);
        }
        if (m_params.enableUniformTexelBufferArrayNonUniformIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderUniformTexelBufferArrayNonUniformIndexing)
            {
                TCU_THROW(NotSupportedError,
                          "shaderUniformTexelBufferArrayNonUniformIndexing feature is not supported");
            }
        }
        if (m_params.enableStorageTexelBufferArrayNonUniformIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderStorageTexelBufferArrayNonUniformIndexing)
            {
                TCU_THROW(NotSupportedError,
                          "shaderStorageTexelBufferArrayNonUniformIndexing feature is not supported");
            }
        }
        if (m_params.enableUniformBufferArrayNonUniformIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderUniformBufferArrayNonUniformIndexing)
            {
                TCU_THROW(NotSupportedError, "shaderUniformBufferArrayNonUniformIndexing feature is not supported");
            }
        }
        if (m_params.enableStorageBufferArrayNonUniformIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderStorageBufferArrayNonUniformIndexing)
            {
                TCU_THROW(NotSupportedError, "shaderStorageBufferArrayNonUniformIndexing feature is not supported");
            }
        }
        if (m_params.enableTessellationShader)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
        }
        if (m_params.enableGeometryShader)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
        }
        if (m_params.enableMeshShader)
        {
            context.requireDeviceFunctionality(VK_EXT_MESH_SHADER_EXTENSION_NAME);

            if (m_params.enableTaskShader)
            {
                if (!context.getMeshShaderFeaturesEXT().taskShader)
                {
                    TCU_THROW(NotSupportedError, "taskShader feature is not supported");
                }
            }
        }
        if (m_params.enableMaintenance4)
        {
            context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
        }
        if (m_params.enableRuntimeDescriptorArray)
        {
            if (!context.getDeviceVulkan12Features().runtimeDescriptorArray)
            {
                TCU_THROW(NotSupportedError, "runtimeDescriptorArray feature is not supported");
            }
        }
        if (m_params.enablePushDescriptors)
        {
            context.requireDeviceFunctionality(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
        }
        if (m_params.enableSampleRateShading)
        {
            context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
        }
        if (m_params.enableVariablePointers)
        {
            context.requireDeviceFunctionality(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME);

            if (!context.getVariablePointersFeatures().variablePointers)
            {
                TCU_THROW(NotSupportedError, "variablePointers feature is not supported");
            }
            if (!context.getVariablePointersFeatures().variablePointersStorageBuffer)
            {
                TCU_THROW(NotSupportedError, "variablePointersStorageBuffer feature is not supported");
            }
        }
        if (m_params.shaderImageInt64Atomics)
        {
            context.requireDeviceFunctionality(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
            if (!context.getShaderImageAtomicInt64FeaturesEXT().shaderImageInt64Atomics)
            {
                TCU_THROW(NotSupportedError, "shaderImageInt64Atomics feature is not supported");
            }
        }
        if (m_params.enableSparseHeap)
        {
            if (!context.getDescriptorHeapPropertiesEXT().sparseDescriptorHeaps)
            {
                TCU_THROW(NotSupportedError, "sparseDescriptorHeaps is not supported");
            }
        }
        if (m_params.enableProtectedHeap)
        {
            if (!context.getDescriptorHeapPropertiesEXT().protectedDescriptorHeaps)
            {
                TCU_THROW(NotSupportedError, "protectedDescriptorHeaps is not supported");
            }
        }
        if (m_params.enableShader64bitIndexing)
        {
            context.requireDeviceFunctionality(VK_EXT_SHADER_64BIT_INDEXING_EXTENSION_NAME);
        }
        if (m_params.enableShaderUniformTexelBufferArrayDynamicIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderUniformTexelBufferArrayDynamicIndexing)
            {
                TCU_THROW(NotSupportedError, "shaderUniformTexelBufferArrayDynamicIndexing feature is not supported");
            }
        }
        if (m_params.enableShaderStorageTexelBufferArrayDynamicIndexing)
        {
            if (!context.getDeviceVulkan12Features().shaderStorageTexelBufferArrayDynamicIndexing)
            {
                TCU_THROW(NotSupportedError, "shaderStorageTexelBufferArrayDynamicIndexing feature is not supported");
            }
        }
    }

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseBasic final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseBasic(tcu::TestContext &testCtx, const std::string &name,
                                         const TestParamsBasic &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceBasic(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    void initQueuePrograms(vk::SourceCollections &programCollection, uint32_t queueIndex) const;

    TestParamsBasic m_params{};
};

class DescriptorHeapTestCaseInvariance final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseInvariance(tcu::TestContext &testCtx, const std::string &name,
                                              const TestParamsWithDescriptorType &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceInvariance(context, m_params);
    }

private:
    TestParamsWithDescriptorType m_params{};
};

class DescriptorHeapTestCaseReservedHeap final : public TestCase
{
public:
    explicit DescriptorHeapTestCaseReservedHeap(tcu::TestContext &testCtx, const std::string &name,
                                                TestParamsReservedHeap const &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        context.requireDeviceFunctionality(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceReservedHeap(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsReservedHeap m_params{};
};

tcu::TestStatus testLimits(Context &context)
{
    if (!context.isDeviceFunctionalitySupported(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME))
    {
        TCU_THROW(NotSupportedError, "VK_EXT_descriptor_heap is not supported");
    }

    const auto &features = *findStructure<VkPhysicalDeviceDescriptorHeapFeaturesEXT>(&context.getDeviceFeatures2());
    const auto &properties =
        *findStructure<VkPhysicalDeviceDescriptorHeapPropertiesEXT>(&context.getDeviceProperties2());

    if (!features.descriptorHeap)
    {
        TCU_THROW(NotSupportedError, "descriptorHeap is not supported");
    }

    struct Limit
    {
        const char *name;
        uint64_t value;
        uint64_t requirement;
    };

    const uint64_t applicationDescriptors = uint64_t{(1U << 20) - (1U << 15)};

    const Limit minLimits[] = {
        {"maxSamplerHeapSize", properties.maxSamplerHeapSize, 4080 * properties.samplerDescriptorSize},
        {"maxResourceHeapSize", properties.maxResourceHeapSize,
         applicationDescriptors * properties.imageDescriptorSize},
        {"maxPushDataSize", properties.maxPushDataSize, 256},
        {"maxDescriptorHeapEmbeddedSamplers ", properties.maxDescriptorHeapEmbeddedSamplers, (int64_t{1} << 11) - 16},
    };
    const Limit maxLimits[] = {
        {"samplerHeapAlignment", properties.samplerHeapAlignment, 65536},
        {"resourceHeapAlignment", properties.resourceHeapAlignment, 65536},
        {"minSamplerHeapReservedRange", properties.minSamplerHeapReservedRange,
         properties.maxSamplerHeapSize - (4000 * properties.samplerDescriptorSize)},
        {"minSamplerHeapReservedRangeWithEmbedded", properties.minSamplerHeapReservedRangeWithEmbedded,
         properties.maxSamplerHeapSize - (2048 * properties.samplerDescriptorSize)},
        {"minResourceHeapReservedRange", properties.minResourceHeapReservedRange,
         properties.maxResourceHeapSize - applicationDescriptors * properties.imageDescriptorSize},
        {"samplerDescriptorSize", properties.samplerDescriptorSize, 256},
        {"imageDescriptorSize", properties.imageDescriptorSize, 256},
        {"bufferDescriptorSize", properties.bufferDescriptorSize, 256},
        {"samplerDescriptorAlignment", properties.samplerDescriptorAlignment, 256},
        {"imageDescriptorAlignment", properties.imageDescriptorAlignment, 256},
        {"bufferDescriptorAlignment", properties.bufferDescriptorAlignment, 256},
        {"samplerYcbcrConversionCount", properties.samplerYcbcrConversionCount, 3},
    };

    for (auto const &minLimit : minLimits)
    {
        if (minLimit.value < minLimit.requirement)
        {
            std::string message = std::string(minLimit.name) + " (" + std::to_string(minLimit.value) +
                                  ") is less than " + std::to_string(minLimit.requirement);
            TCU_THROW(TestError, message);
        }
    }
    for (auto const &maxLimit : maxLimits)
    {
        if (maxLimit.value > maxLimit.requirement)
        {
            std::string message = std::string(maxLimit.name) + " (" + std::to_string(maxLimit.value) +
                                  ") is greater than " + std::to_string(maxLimit.requirement);
            TCU_THROW(TestError, message);
        }
    }

    const auto &vki = context.getInstanceInterface();

    const auto checkSize =
        [&](VkDescriptorType type, const char *descriptorName, VkDeviceSize propertySize, const char *propertyName)
    {
        const VkDeviceSize size = vki.getPhysicalDeviceDescriptorSizeEXT(context.getPhysicalDevice(), type);
        if (size > propertySize)
        {
            std::string message = std::string(descriptorName) + " size (" + std::to_string(size) +
                                  ") is greater than " + propertyName + " (" + std::to_string(propertySize) + ")";
            TCU_THROW(TestError, message);
        }
    };
    checkSize(VK_DESCRIPTOR_TYPE_SAMPLER, "VK_DESCRIPTOR_TYPE_SAMPLER", properties.samplerDescriptorSize,
              "samplerDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE", properties.imageDescriptorSize,
              "imageDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE", properties.imageDescriptorSize,
              "imageDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
              properties.imageDescriptorSize, "imageDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
              properties.imageDescriptorSize, "imageDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER", properties.bufferDescriptorSize,
              "bufferDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER", properties.bufferDescriptorSize,
              "bufferDescriptorSize");
    checkSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT",
              properties.imageDescriptorSize, "imageDescriptorSize");

    if (context.isDeviceFunctionalitySupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
    {
        checkSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR",
                  properties.bufferDescriptorSize, "bufferDescriptorSize");
    }
    if (context.isDeviceFunctionalitySupported(VK_NV_RAY_TRACING_EXTENSION_NAME))
    {
        checkSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV",
                  properties.bufferDescriptorSize, "bufferDescriptorSize");
    }
    if (context.isDeviceFunctionalitySupported(VK_ARM_TENSORS_EXTENSION_NAME))
    {
        const auto tensorProperties =
            findStructure<VkPhysicalDeviceDescriptorHeapTensorPropertiesARM>(&context.getDeviceProperties2());
        if (!tensorProperties)
            TCU_THROW(TestError, "VkPhysicalDeviceDescriptorHeapTensorPropertiesARM not found");
        checkSize(VK_DESCRIPTOR_TYPE_TENSOR_ARM, "VK_DESCRIPTOR_TYPE_TENSOR_ARM",
                  tensorProperties->tensorDescriptorSize, "tensorDescriptorSize");
    }
    if (context.isDeviceFunctionalitySupported(VK_NV_PARTITIONED_ACCELERATION_STRUCTURE_EXTENSION_NAME))
    {
        checkSize(VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV,
                  "VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV", properties.bufferDescriptorSize,
                  "bufferDescriptorSize");
    }

    const auto physDevProps2 = context.getDeviceProperties2();
    if (properties.maxPushDataSize < physDevProps2.properties.limits.maxPushConstantsSize)
    {
        std::string message = "maxPushDataSize (" + std::to_string(properties.maxPushDataSize) +
                              ") is less than maxPushConstantsSize (" +
                              std::to_string(physDevProps2.properties.limits.maxPushConstantsSize) + ")";
        TCU_THROW(TestError, message);
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::UVec2 getExpectedData_G8_B8R8(uint32_t seed)
{
    // Hash the input data to achieve "randomness" of components.
    const uint32_t data = deUint32Hash(seed);

    return tcu::UVec2((data >> 16) & 0xff, data & 0xffff);
}

// Convert G8_B8R8_UNORM to float components.
tcu::Vec4 toVec4_G8_B8R8(tcu::UVec2 input)
{
    return tcu::Vec4(float(((input.y() >> 8) & 0xff)) / 255.0f, float(input.x()) / 255.0f,
                     float((input.y() & 0xff)) / 255.0f, 1.0f);
}

class DescriptorHeapTestInstanceYcbcr final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceYcbcr(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseYcbcr final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseYcbcr(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceYcbcr(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseYcbcr::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string source = "#version 450\n"
                         "layout (set = 0, binding = 0) uniform sampler2D tex;\n"
                         "layout (set = 1, binding = 0) buffer Output {\n"
                         "  vec4 result[];\n"
                         "};\n"
                         "layout(local_size_x = 2, local_size_y = 2) in;\n"
                         "void main() {\n"
                         "  vec4 color = textureLod(tex, vec2(gl_GlobalInvocationID.xy), 0);\n"
                         "  result[gl_LocalInvocationIndex] = color;\n"
                         "}\n";
    programCollection.glslSources.add("compute") << glu::ComputeSource(source);
}

tcu::TestStatus DescriptorHeapTestInstanceYcbcr::iterate()
{
    const auto &vki = m_instance.getDriver();
    const auto &vkd = m_device.getDriver();

    const VkFormat format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    VkSamplerYcbcrConversionImageFormatProperties ycbcrFormatProps = initVulkanStructure();
    VkImageFormatProperties2 imageFormatProps                      = initVulkanStructure(&ycbcrFormatProps);

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = initVulkanStructure();
    imageFormatInfo.format                           = format;
    imageFormatInfo.type                             = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling                           = VK_IMAGE_TILING_OPTIMAL;
    imageFormatInfo.usage                            = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VK_CHECK(vki.getPhysicalDeviceImageFormatProperties2(m_physDevice, &imageFormatInfo, &imageFormatProps));
    const uint32_t combinedImageSamplerDescriptorCount = ycbcrFormatProps.combinedImageSamplerDescriptorCount;

    const uint32_t resourceStride = static_cast<uint32_t>(getResourceDescriptorStride(m_descriptorHeapProperties));

    const VkDeviceSize resourceHeapReservedOffset = VkDeviceSize{5U * resourceStride};
    const VkDeviceSize resourceHeapSize =
        resourceHeapReservedOffset + m_descriptorHeapProperties.minResourceHeapReservedRange;

    const VkDeviceSize samplerHeapSize = std::max(m_descriptorHeapProperties.samplerDescriptorSize,
                                                  m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded);

    const auto heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    const auto resourceHeap = createBufferAndMemory(resourceHeapSize, heapFlags);
    const auto samplerHeap  = createBufferAndMemory(samplerHeapSize, heapFlags);

    const uint32_t imageSize        = 2U;
    const uint32_t numPixels        = imageSize * imageSize; // plane 0
    const uint32_t outputBufferSize = numPixels * sizeof(tcu::UVec4);
    auto outputBuffer               = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeap->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = resourceHeapReservedOffset;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerHeapBindInfo   = initVulkanStructure();
    samplerHeapBindInfo.heapRange.address   = samplerHeap->address;
    samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
    samplerHeapBindInfo.reservedRangeOffset = 0;
    samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;

    VkSamplerYcbcrConversionCreateInfo samplerYcbcrConvCreateInfo = initVulkanStructure();
    samplerYcbcrConvCreateInfo.format                             = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    samplerYcbcrConvCreateInfo.ycbcrModel                         = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
    samplerYcbcrConvCreateInfo.ycbcrRange                         = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
    samplerYcbcrConvCreateInfo.components.r                       = VK_COMPONENT_SWIZZLE_IDENTITY;
    samplerYcbcrConvCreateInfo.components.g                       = VK_COMPONENT_SWIZZLE_IDENTITY;
    samplerYcbcrConvCreateInfo.components.b                       = VK_COMPONENT_SWIZZLE_IDENTITY;
    samplerYcbcrConvCreateInfo.components.a                       = VK_COMPONENT_SWIZZLE_IDENTITY;
    samplerYcbcrConvCreateInfo.xChromaOffset                      = VK_CHROMA_LOCATION_COSITED_EVEN;
    samplerYcbcrConvCreateInfo.yChromaOffset                      = VK_CHROMA_LOCATION_COSITED_EVEN;
    samplerYcbcrConvCreateInfo.chromaFilter                       = VK_FILTER_NEAREST;
    samplerYcbcrConvCreateInfo.forceExplicitReconstruction        = VK_FALSE;

    Move<VkSamplerYcbcrConversion> samplerYcbcrConversion =
        createSamplerYcbcrConversion(vkd, *m_device, &samplerYcbcrConvCreateInfo);

    VkSamplerYcbcrConversionInfo ycbcrConversionInfo = initVulkanStructure();
    ycbcrConversionInfo.conversion                   = *samplerYcbcrConversion;

    VkImageCreateInfo imageCreateInfo     = initVulkanStructure();
    imageCreateInfo.flags                 = 0;
    imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                = format;
    imageCreateInfo.extent                = {imageSize, imageSize, 1U};
    imageCreateInfo.mipLevels             = 1U;
    imageCreateInfo.arrayLayers           = 1U;
    imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage                 = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices   = nullptr;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    const auto sourceImage                = createImageAndMemory(imageCreateInfo);
    const auto sourceBuffer =
        createBufferAndMemory(imageSize * imageSize * 2 * sizeof(tcu::UVec2), VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR);

    VkImageViewCreateInfo imageViewCreateInfo = initVulkanStructure(&ycbcrConversionInfo);
    imageViewCreateInfo.flags                 = 0;
    imageViewCreateInfo.image                 = *sourceImage->image;
    imageViewCreateInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    imageViewCreateInfo.components            = makeComponentMappingIdentity();
    imageViewCreateInfo.subresourceRange      = makeDefaultImageSubresourceRange();

    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.pView                    = &imageViewCreateInfo;
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkResourceDescriptorInfoEXT imageResourceDescriptorInfo = initVulkanStructure();
    imageResourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageResourceDescriptorInfo.data.pImage                 = &imageDescriptorInfo;

    VkHostAddressRangeEXT imageDescriptor{};
    imageDescriptor.address = resourceHeap->memory->getHostPtr();
    imageDescriptor.size    = combinedImageSamplerDescriptorCount * resourceStride;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &imageResourceDescriptorInfo, &imageDescriptor));

    VkDeviceAddressRangeEXT bufferDeviceAddressRange{};
    bufferDeviceAddressRange.address = outputBuffer->address;
    bufferDeviceAddressRange.size    = outputBufferSize;

    VkResourceDescriptorInfoEXT bufferDescriptorInfo = initVulkanStructure();
    bufferDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferDescriptorInfo.data.pAddressRange          = &bufferDeviceAddressRange;

    VkHostAddressRangeEXT bufferDescriptor{};
    bufferDescriptor.address = reinterpret_cast<char *>(resourceHeap->memory->getHostPtr()) + 4U * resourceStride;
    bufferDescriptor.size    = resourceStride;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &bufferDescriptorInfo, &bufferDescriptor));

    const tcu::UVec2 ycbcrData   = getExpectedData_G8_B8R8(m_params.seed);
    const auto expectedDataFloat = toVec4_G8_B8R8(ycbcrData);

    const auto pPlane0 = static_cast<uint8_t *>(sourceBuffer->memory->getHostPtr());
    const auto pPlane1 = reinterpret_cast<uint16_t *>(pPlane0 + numPixels);
    std::fill(pPlane0, pPlane0 + numPixels, ycbcrData.x());
    std::fill(pPlane1, pPlane1 + numPixels / 4, ycbcrData.y());

    VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
    samplerCreateInfo.pNext                   = &ycbcrConversionInfo;
    samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias              = 0.0f;
    samplerCreateInfo.anisotropyEnable        = VK_FALSE;
    samplerCreateInfo.maxAnisotropy           = 1.0f;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                  = 0.0f;
    samplerCreateInfo.maxLod                  = 0.0f;
    samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    const auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    std::array<VkDescriptorSetAndBindingMappingEXT, 2> mappings{};
    mappings[0]                                            = initVulkanStructure();
    mappings[0].descriptorSet                              = 0;
    mappings[0].firstBinding                               = 0;
    mappings[0].bindingCount                               = 1;
    mappings[0].resourceMask                               = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[0].source                                     = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[0].sourceData.constantOffset.heapOffset       = 0U * resourceStride;
    mappings[0].sourceData.constantOffset.heapArrayStride  = resourceStride;
    mappings[0].sourceData.constantOffset.pEmbeddedSampler = &samplerCreateInfo;
    mappings[0].sourceData.constantOffset.samplerHeapOffset      = 0;
    mappings[0].sourceData.constantOffset.samplerHeapArrayStride = 0;

    mappings[1]                                            = initVulkanStructure();
    mappings[1].descriptorSet                              = 1;
    mappings[1].firstBinding                               = 0;
    mappings[1].bindingCount                               = 1;
    mappings[1].resourceMask                               = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[1].source                                     = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[1].sourceData.constantOffset.heapOffset       = 4U * resourceStride;
    mappings[1].sourceData.constantOffset.heapArrayStride  = 0xcafe00; // This value is not used.
    mappings[1].sourceData.constantOffset.pEmbeddedSampler = NULL;
    mappings[1].sourceData.constantOffset.samplerHeapOffset      = 0;
    mappings[1].sourceData.constantOffset.samplerHeapArrayStride = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    const auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    const auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    std::array<VkBufferImageCopy, 2> copyRegions{};
    copyRegions[0].bufferOffset     = 0;
    copyRegions[0].imageExtent      = {imageSize, imageSize, 1U};
    copyRegions[0].imageSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_PLANE_0_BIT, 0U, 0U, 1U);

    copyRegions[1].bufferOffset     = numPixels;
    copyRegions[1].imageExtent      = {imageSize / 2, imageSize / 2, 1U};
    copyRegions[1].imageSubresource = makeImageSubresourceLayers(VK_IMAGE_ASPECT_PLANE_1_BIT, 0U, 0U, 1U);

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.size         = sizeof(uint64_t);
    pushDataInfo.data.address      = &outputBuffer->address;

    const auto bothPlanes =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT, 0U, 1U, 0U, 1U);
    const VkImageMemoryBarrier prepareImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *sourceImage->image, bothPlanes);
    const VkImageMemoryBarrier doneTransferBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *sourceImage->image, bothPlanes);

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &prepareImageBarrier);
    vkd.cmdCopyBufferToImage(cmdBuffer, *sourceBuffer->buffer, *sourceImage->image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()),
                             copyRegions.data());
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                           nullptr, 0, nullptr, 1, &doneTransferBarrier);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);
    vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(cmdBuffer, 1U, 1U, 1U);

    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers    = &cmdBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    const auto outputBufferPtr = reinterpret_cast<char *>(outputBuffer->memory->getHostPtr());
    for (uint32_t i = 0; i < numPixels; ++i)
    {
        tcu::Vec4 result{};
        deMemcpy(&result, outputBufferPtr + i * sizeof(tcu::Vec4), sizeof(result));

        const float tolerance = 0.005f;

        if ((std::abs(expectedDataFloat.x() - result.x()) > tolerance) ||
            (std::abs(expectedDataFloat.y() - result.y()) > tolerance) ||
            (std::abs(expectedDataFloat.z() - result.z()) > tolerance) || (result.w() != 1.0f))
        {
            std::stringstream stream;
            stream << "At index " << i << ", expected " << expectedDataFloat << " but got " << result;
            return tcu::TestStatus::fail(stream.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceDifferentMappingsPerShader final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceDifferentMappingsPerShader(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseDifferentMappingsPerShader final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseDifferentMappingsPerShader(tcu::TestContext &testCtx, const std::string &name,
                                                              const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceDifferentMappingsPerShader(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseDifferentMappingsPerShader::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string vertex = "#version 450\n"
                         "layout(set = 0, binding = 0) uniform VertexInput {\n"
                         "  float vertexInput;\n"
                         "};\n"
                         "layout(location = 0) out float value;\n"
                         "void main() {\n"
                         "  gl_Position = vec4(0, 0, 0, 1);\n"
                         "  value = vertexInput;\n"
                         "}\n";

    std::string fragment = "#version 450\n"
                           "layout(set = 0, binding = 0) uniform FragmentInput {\n"
                           "  float fragmentInput;\n"
                           "};\n"
                           "layout(set = 0, binding = 0, r32f) uniform imageBuffer outputImage;\n"
                           "layout(location = 0) in float vertexValue;\n"
                           "void main() {\n"
                           "  imageStore(outputImage, 0, vec4(vertexValue + fragmentInput, 0, 0, 0));\n"
                           "}\n";

    programCollection.glslSources.add("vertex") << glu::VertexSource(vertex);
    programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment);
}

tcu::TestStatus DescriptorHeapTestInstanceDifferentMappingsPerShader::iterate()
{
    const auto &vkd = m_device.getDriver();

    VkDescriptorSetAndBindingMappingEXT vertexMapping = initVulkanStructure();
    vertexMapping.descriptorSet                       = 0;
    vertexMapping.firstBinding                        = 0;
    vertexMapping.bindingCount                        = 1;
    vertexMapping.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
    vertexMapping.source                              = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
    vertexMapping.sourceData.pushDataOffset           = 0 * sizeof(float);

    std::array<VkDescriptorSetAndBindingMappingEXT, 2> fragmentMappings = {
        initVulkanStructure(),
        initVulkanStructure(),
    };
    fragmentMappings[0].descriptorSet             = 0;
    fragmentMappings[0].firstBinding              = 0;
    fragmentMappings[0].bindingCount              = 1;
    fragmentMappings[0].resourceMask              = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
    fragmentMappings[0].source                    = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
    fragmentMappings[0].sourceData.pushDataOffset = 1 * sizeof(float);

    fragmentMappings[1].descriptorSet = 0;
    fragmentMappings[1].firstBinding  = 0;
    fragmentMappings[1].bindingCount  = 1;
    fragmentMappings[1].resourceMask  = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
    fragmentMappings[1].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    fragmentMappings[1].sourceData.constantOffset.heapOffset             = 0;
    fragmentMappings[1].sourceData.constantOffset.heapArrayStride        = 0;
    fragmentMappings[1].sourceData.constantOffset.pEmbeddedSampler       = nullptr;
    fragmentMappings[1].sourceData.constantOffset.samplerHeapOffset      = 0;
    fragmentMappings[1].sourceData.constantOffset.samplerHeapArrayStride = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT vertexMappingInfo = initVulkanStructure();
    vertexMappingInfo.mappingCount                                  = 1;
    vertexMappingInfo.pMappings                                     = &vertexMapping;

    VkShaderDescriptorSetAndBindingMappingInfoEXT fragmentMappingInfo = initVulkanStructure();
    fragmentMappingInfo.mappingCount                                  = static_cast<uint32_t>(fragmentMappings.size());
    fragmentMappingInfo.pMappings                                     = fragmentMappings.data();

    VkSubpassDescription subpassDescription{};

    VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
    renderPassCreateInfo.subpassCount           = 1;
    renderPassCreateInfo.pSubpasses             = &subpassDescription;
    Move<VkRenderPass> renderPass               = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineTessellationStateCreateInfo tessellationState = initVulkanStructure();

    const VkViewport viewport = makeViewport(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    const VkRect2D scissor    = makeRect2D(0, 0, 1, 1);

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
    rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    auto vertexModule   = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));
    auto fragmentModule = createShaderModule(vkd, *m_device, getShaderBinary("fragment"));

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {
        initVulkanStructure(),
        initVulkanStructure(),
    };
    stages[0].pNext  = &vertexMappingInfo;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = *vertexModule;
    stages[0].pName  = "main";

    stages[1].pNext  = &fragmentMappingInfo;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = *fragmentModule;
    stages[1].pName  = "main";

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                        = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    pipelineCreateInfo.stageCount                   = static_cast<uint32_t>(stages.size());
    pipelineCreateInfo.pStages                      = stages.data();
    pipelineCreateInfo.pVertexInputState            = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineCreateInfo.pTessellationState           = &tessellationState;
    pipelineCreateInfo.pViewportState               = &viewportState;
    pipelineCreateInfo.pRasterizationState          = &rasterizationState;
    pipelineCreateInfo.pMultisampleState            = &multisampleState;
    pipelineCreateInfo.pDepthStencilState           = &depthStencilState;
    pipelineCreateInfo.pColorBlendState             = &colorBlendState;
    pipelineCreateInfo.pDynamicState                = nullptr;
    pipelineCreateInfo.layout                       = VK_NULL_HANDLE;
    pipelineCreateInfo.renderPass                   = *renderPass;
    pipelineCreateInfo.subpass                      = 0;
    pipelineCreateInfo.basePipelineHandle           = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex            = 0;

    Move<VkPipeline> pipeline       = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);
    Move<VkFramebuffer> framebuffer = makeFramebuffer(vkd, *m_device, *renderPass, 0, nullptr, 1, 1);

    auto outputBuffer = createBufferAndMemory(sizeof(float), VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    const VkDeviceSize imageDescriptorStride = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize resourceHeapUserSize =
        alignUp(1U * imageDescriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceHeapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto const heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    auto resourceHeapMemory = createBufferAndMemory(resourceHeapSize, heapFlags);

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeapMemory->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = resourceHeapUserSize;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkTexelBufferDescriptorInfoEXT texelBufferInfo = initVulkanStructure();
    texelBufferInfo.addressRange.address           = outputBuffer->address;
    texelBufferInfo.addressRange.size              = sizeof(float);
    texelBufferInfo.format                         = VK_FORMAT_R32_SFLOAT;

    VkResourceDescriptorInfoEXT resourceDescriptorInfo = initVulkanStructure();
    resourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    resourceDescriptorInfo.data.pTexelBuffer           = &texelBufferInfo;

    VkHostAddressRangeEXT resourceDescriptor{};
    resourceDescriptor.address = resourceHeapMemory->memory->getHostPtr();
    resourceDescriptor.size    = static_cast<size_t>(resourceHeapUserSize);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceDescriptorInfo, &resourceDescriptor));

    de::Random rnd(m_params.seed);

    const std::array<float, 2> pushData = {
        rnd.getFloat(0.0f, 1.0f),
        rnd.getFloat(0.0f, 1.0f),
    };

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.data.size         = sizeof(pushData);
    pushDataInfo.data.address      = &pushData;

    const auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    VkRenderPassBeginInfo renderPassBeginInfo = initVulkanStructure();
    renderPassBeginInfo.renderPass            = *renderPass;
    renderPassBeginInfo.framebuffer           = *framebuffer;
    renderPassBeginInfo.renderArea            = makeRect2D(0, 0, 1, 1);
    renderPassBeginInfo.clearValueCount       = 0;
    renderPassBeginInfo.pClearValues          = nullptr;

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vkd.cmdDraw(cmdBuffer, 1, 1, 0, 0);
    vkd.cmdEndRenderPass(cmdBuffer);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers    = &cmdBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    float result{};
    deMemcpy(&result, outputBuffer->memory->getHostPtr(), sizeof(result));

    const float expectedResult = pushData[0] + pushData[1];

    if (std::abs(result - expectedResult) > 0.01)
    {
        std::stringstream stream;
        stream << "Expected " << expectedResult << " but got " << result;
        return tcu::TestStatus::fail(stream.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceGPL final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceGPL(Context &context, const TestParamsGPL &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsGPL m_params;
};

class DescriptorHeapTestCaseGPL final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseGPL(tcu::TestContext &testCtx, const std::string &name, const TestParamsGPL &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceGPL(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsGPL m_params;
};

void DescriptorHeapTestCaseGPL::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string vertex = "#version 450\n"
                         "layout (set = 0, binding = 6) uniform textureBuffer positionTexBuf;\n"
                         "layout (set = 0, binding = 7) uniform textureBuffer colorTexBuf;\n"
                         "layout(location = 0) out vec4 outColor;\n"
                         "layout(location = 1) out flat int outIndex;\n"
                         "void main() {\n"
                         "  gl_Position = texelFetch(positionTexBuf, gl_VertexIndex);\n"
                         "  outColor = texelFetch(colorTexBuf, gl_VertexIndex);\n"
                         "  outIndex = gl_VertexIndex;\n"
                         "}\n";

    std::string fragment = "#version 450\n"
                           "layout (set = 0, binding = 8, rgba32f) uniform imageBuffer outputImgBuf;\n"
                           "layout(location = 0) in vec4 inColor;\n"
                           "layout(location = 1) in flat int inIndex;\n"
                           "void main() {\n"
                           "  imageStore(outputImgBuf, inIndex, inColor);\n"
                           "}\n";

    std::string vertexOutput     = "#version 450\n"
                                   "layout (set = 0, binding = 6) uniform textureBuffer positionTexBuf;\n"
                                   "layout (set = 0, binding = 7) uniform textureBuffer colorTexBuf;\n"
                                   "layout (set = 0, binding = 8, rgba32f) uniform imageBuffer outputImgBuf;\n"
                                   "void main() {\n"
                                   "  gl_Position = texelFetch(positionTexBuf, gl_VertexIndex);\n"
                                   "  imageStore(outputImgBuf, gl_VertexIndex, texelFetch(colorTexBuf, gl_VertexIndex));\n"
                                   "}\n";
    std::string fragmentNoOutput = "#version 450\n"
                                   "void main() {\n"
                                   "}\n";

    if (m_params.unbindFragShader)
    {
        programCollection.glslSources.add("vertex") << glu::VertexSource(vertexOutput);
        programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentNoOutput);
    }
    else
    {
        programCollection.glslSources.add("vertex") << glu::VertexSource(vertex);
        programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment);
    }
}

tcu::TestStatus DescriptorHeapTestInstanceGPL::iterate()
{
    const auto &vkd = m_device.getDriver();

    DE_ASSERT(m_params.enableGraphicsPipelineLibrary != m_params.enableShaderObject);
    DE_ASSERT(m_params.enableGraphicsPipelineLibrary || m_params.enableShaderObject);

    const bool useShaderObject = m_params.enableShaderObject;

    const VkDeviceSize imageDescriptorStride = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize imageHeapUserSize     = 3U * imageDescriptorStride;
    const VkDeviceSize imageHeapSize = imageHeapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    const auto heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    const auto imageHeap = createBufferAndMemory(imageHeapSize, heapFlags);

    const uint32_t vertexCount        = 4U;
    const uint32_t positionBufferSize = vertexCount * sizeof(tcu::Vec4);
    const uint32_t colorBufferSize    = vertexCount * sizeof(tcu::Vec4);
    const uint32_t outputBufferSize   = vertexCount * sizeof(tcu::Vec4);

    auto positionBuffer = createBufferAndMemory(positionBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                        VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT);
    auto colorBuffer    = createBufferAndMemory(colorBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                     VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT);
    auto outputBuffer   = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                      VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT);

    const std::array<tcu::Vec4, 4> positions = {
        tcu::Vec4{0.25f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{0.25f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{0.25f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{0.25f, 0.25f, 0.0f, 1.0f},
    };
    const std::array<tcu::Vec4, 4> colors = {
        tcu::Vec4{0.25f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{0.50f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{0.75f, 0.25f, 0.0f, 1.0f},
        tcu::Vec4{1.00f, 0.25f, 0.0f, 1.0f},
    };
    deMemcpy(positionBuffer->memory->getHostPtr(), &positions, sizeof(positions));
    deMemcpy(colorBuffer->memory->getHostPtr(), &colors, sizeof(colors));

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = imageHeap->address;
    resourceHeapBindInfo.heapRange.size      = imageHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = imageHeapUserSize;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkTexelBufferDescriptorInfoEXT positionTexelInfo = initVulkanStructure();
    positionTexelInfo.format                         = VK_FORMAT_R32G32B32A32_SFLOAT;
    positionTexelInfo.addressRange.address           = positionBuffer->address;
    positionTexelInfo.addressRange.size              = positionBufferSize;

    VkTexelBufferDescriptorInfoEXT colorTexelInfo = initVulkanStructure();
    colorTexelInfo.format                         = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorTexelInfo.addressRange.address           = colorBuffer->address;
    colorTexelInfo.addressRange.size              = colorBufferSize;

    VkTexelBufferDescriptorInfoEXT outputTexelInfo = initVulkanStructure();
    outputTexelInfo.format                         = VK_FORMAT_R32G32B32A32_SFLOAT;
    outputTexelInfo.addressRange.address           = outputBuffer->address;
    outputTexelInfo.addressRange.size              = outputBufferSize;

    std::array<VkResourceDescriptorInfoEXT, 3> imageDescriptorInfos{};
    imageDescriptorInfos[0]                   = initVulkanStructure();
    imageDescriptorInfos[0].type              = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    imageDescriptorInfos[0].data.pTexelBuffer = &positionTexelInfo;
    imageDescriptorInfos[1]                   = initVulkanStructure();
    imageDescriptorInfos[1].type              = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    imageDescriptorInfos[1].data.pTexelBuffer = &colorTexelInfo;
    imageDescriptorInfos[2]                   = initVulkanStructure();
    imageDescriptorInfos[2].type              = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    imageDescriptorInfos[2].data.pTexelBuffer = &outputTexelInfo;

    std::array<VkHostAddressRangeEXT, 3> imageDescriptorRanges{};
    for (size_t index = 0; index < imageDescriptorRanges.size(); ++index)
    {
        imageDescriptorRanges[index].address =
            static_cast<char *>(imageHeap->memory->getHostPtr()) + index * imageDescriptorStride;
        imageDescriptorRanges[index].size = static_cast<size_t>(imageDescriptorStride);
    }

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, static_cast<uint32_t>(imageDescriptorInfos.size()),
                                             imageDescriptorInfos.data(), imageDescriptorRanges.data()));

    const auto vertexModule   = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));
    const auto fragmentModule = createShaderModule(vkd, *m_device, getShaderBinary("fragment"));

    std::array<VkDescriptorSetAndBindingMappingEXT, 3> vertexMappings{};
    vertexMappings[0]                                      = initVulkanStructure();
    vertexMappings[0].descriptorSet                        = 0;
    vertexMappings[0].firstBinding                         = 6;
    vertexMappings[0].bindingCount                         = 1;
    vertexMappings[0].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    vertexMappings[0].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    vertexMappings[0].sourceData.constantOffset.heapOffset = 0;
    vertexMappings[1]                                      = initVulkanStructure();
    vertexMappings[1].descriptorSet                        = 0;
    vertexMappings[1].firstBinding                         = 7;
    vertexMappings[1].bindingCount                         = 1;
    vertexMappings[1].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    vertexMappings[1].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    vertexMappings[1].sourceData.constantOffset.heapOffset = static_cast<int32_t>(1 * imageDescriptorStride);
    vertexMappings[2]                                      = initVulkanStructure();
    vertexMappings[2].descriptorSet                        = 0;
    vertexMappings[2].firstBinding                         = 8;
    vertexMappings[2].bindingCount                         = 1;
    vertexMappings[2].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    vertexMappings[2].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    vertexMappings[2].sourceData.constantOffset.heapOffset = static_cast<int32_t>(2 * imageDescriptorStride);

    std::array<VkDescriptorSetAndBindingMappingEXT, 1> fragmentMappings{};
    fragmentMappings[0]               = initVulkanStructure();
    fragmentMappings[0].descriptorSet = 0;
    fragmentMappings[0].firstBinding  = 8;
    fragmentMappings[0].bindingCount  = 1;
    fragmentMappings[0].resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    fragmentMappings[0].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    fragmentMappings[0].sourceData.constantOffset.heapOffset = static_cast<int32_t>(2 * imageDescriptorStride);

    VkShaderDescriptorSetAndBindingMappingInfoEXT vertexMappingInfo = initVulkanStructure();
    vertexMappingInfo.mappingCount                                  = m_params.unbindFragShader ? 3u : 2u;
    vertexMappingInfo.pMappings                                     = vertexMappings.data();

    VkShaderDescriptorSetAndBindingMappingInfoEXT fragmentMappingInfo = initVulkanStructure();
    fragmentMappingInfo.mappingCount                                  = de::sizeU32(fragmentMappings);
    fragmentMappingInfo.pMappings                                     = fragmentMappings.data();

    const uint32_t renderingSize = 100;
    const VkViewport viewport    = makeViewport(0.0f, 0.0f, renderingSize, renderingSize, 0.0f, 1.0f);
    const VkRect2D scissor       = makeRect2D(0, 0, renderingSize, renderingSize);

    Move<VkPipeline> vertexPipeline;
    Move<VkPipeline> fragmentPipeline;
    Move<VkPipeline> pipeline;
    Move<VkFramebuffer> framebuffer;
    Move<VkRenderPass> renderPass;

    Move<VkShaderEXT> vertexShader;
    Move<VkShaderEXT> fragmentShader;

    if (useShaderObject)
    {
        const auto &vertexBinary   = getShaderBinary("vertex");
        const auto &fragmentBinary = getShaderBinary("fragment");

        VkShaderCreateInfoEXT vertexShaderCreateInfo  = initVulkanStructure();
        vertexShaderCreateInfo.pNext                  = &vertexMappingInfo;
        vertexShaderCreateInfo.flags                  = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;
        vertexShaderCreateInfo.stage                  = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderCreateInfo.nextStage              = VK_SHADER_STAGE_FRAGMENT_BIT;
        vertexShaderCreateInfo.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT;
        vertexShaderCreateInfo.codeSize               = vertexBinary.getSize();
        vertexShaderCreateInfo.pCode                  = vertexBinary.getBinary();
        vertexShaderCreateInfo.pName                  = "main";
        vertexShaderCreateInfo.setLayoutCount         = 0;
        vertexShaderCreateInfo.pSetLayouts            = nullptr;
        vertexShaderCreateInfo.pushConstantRangeCount = 0;
        vertexShaderCreateInfo.pPushConstantRanges    = nullptr;
        vertexShaderCreateInfo.pSpecializationInfo    = nullptr;

        VkShaderCreateInfoEXT fragmentShaderCreateInfo = initVulkanStructure();
        if (!m_params.unbindFragShader)
        {
            fragmentShaderCreateInfo.pNext = &fragmentMappingInfo;
            fragmentShaderCreateInfo.flags = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;
        }
        fragmentShaderCreateInfo.stage                  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentShaderCreateInfo.nextStage              = 0;
        fragmentShaderCreateInfo.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT;
        fragmentShaderCreateInfo.codeSize               = fragmentBinary.getSize();
        fragmentShaderCreateInfo.pCode                  = fragmentBinary.getBinary();
        fragmentShaderCreateInfo.pName                  = "main";
        fragmentShaderCreateInfo.setLayoutCount         = 0;
        fragmentShaderCreateInfo.pSetLayouts            = nullptr;
        fragmentShaderCreateInfo.pushConstantRangeCount = 0;
        fragmentShaderCreateInfo.pPushConstantRanges    = nullptr;
        fragmentShaderCreateInfo.pSpecializationInfo    = nullptr;

        vertexShader   = createShader(vkd, *m_device, vertexShaderCreateInfo);
        fragmentShader = createShader(vkd, *m_device, fragmentShaderCreateInfo);
    }
    else
    {
        VkSubpassDescription subpassDescription{};

        VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
        renderPassCreateInfo.subpassCount           = 1;
        renderPassCreateInfo.pSubpasses             = &subpassDescription;
        renderPass                                  = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

        VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
        inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

        VkPipelineTessellationStateCreateInfo tessellationState = initVulkanStructure();

        VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
        viewportState.viewportCount                     = 1;
        viewportState.pViewports                        = &viewport;
        viewportState.scissorCount                      = 1;
        viewportState.pScissors                         = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
        rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
        rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.lineWidth                              = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
        multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();

        VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

        VkGraphicsPipelineCreateInfo basePipelineCreateInfo = initVulkanStructure();
        basePipelineCreateInfo.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        basePipelineCreateInfo.stageCount                   = 0;
        basePipelineCreateInfo.pStages                      = nullptr;
        basePipelineCreateInfo.pVertexInputState            = &vertexInputState;
        basePipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
        basePipelineCreateInfo.pTessellationState           = &tessellationState;
        basePipelineCreateInfo.pViewportState               = &viewportState;
        basePipelineCreateInfo.pRasterizationState          = &rasterizationState;
        basePipelineCreateInfo.pMultisampleState            = &multisampleState;
        basePipelineCreateInfo.pDepthStencilState           = &depthStencilState;
        basePipelineCreateInfo.pColorBlendState             = &colorBlendState;
        basePipelineCreateInfo.pDynamicState                = nullptr;
        basePipelineCreateInfo.layout                       = VK_NULL_HANDLE;
        basePipelineCreateInfo.renderPass                   = *renderPass;
        basePipelineCreateInfo.subpass                      = 0;
        basePipelineCreateInfo.basePipelineHandle           = VK_NULL_HANDLE;
        basePipelineCreateInfo.basePipelineIndex            = 0;

        VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
        pipelineCreateFlags2CreateInfo.flags =
            VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

        VkGraphicsPipelineLibraryCreateInfoEXT vertexGPLCreateInfo = initVulkanStructure();
        vertexGPLCreateInfo.pNext                                  = &pipelineCreateFlags2CreateInfo;
        vertexGPLCreateInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                    VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

        VkGraphicsPipelineLibraryCreateInfoEXT fragmentGPLCreateInfo = initVulkanStructure();
        fragmentGPLCreateInfo.pNext                                  = &pipelineCreateFlags2CreateInfo;
        fragmentGPLCreateInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

        VkPipelineShaderStageCreateInfo vertexStage = initVulkanStructure();
        vertexStage.pNext                           = &vertexMappingInfo;
        vertexStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module                          = *vertexModule;
        vertexStage.pName                           = "main";

        VkPipelineShaderStageCreateInfo fragmentStage = initVulkanStructure();
        fragmentStage.pNext                           = &fragmentMappingInfo;
        fragmentStage.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module                          = *fragmentModule;
        fragmentStage.pName                           = "main";

        auto vertexPipelineCreateInfo       = basePipelineCreateInfo;
        vertexPipelineCreateInfo.pNext      = &vertexGPLCreateInfo;
        vertexPipelineCreateInfo.stageCount = 1;
        vertexPipelineCreateInfo.pStages    = &vertexStage;

        auto fragmentPipelineCreateInfo       = basePipelineCreateInfo;
        fragmentPipelineCreateInfo.pNext      = &fragmentGPLCreateInfo;
        fragmentPipelineCreateInfo.stageCount = 1;
        fragmentPipelineCreateInfo.pStages    = &fragmentStage;

        vertexPipeline   = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &vertexPipelineCreateInfo);
        fragmentPipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &fragmentPipelineCreateInfo);

        const std::array<VkPipeline, 2> gplPipelines = {
            *vertexPipeline,
            *fragmentPipeline,
        };

        pipelineCreateFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

        VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure(&pipelineCreateFlags2CreateInfo);
        linkedPipelineLibraryInfo.libraryCount                   = de::sizeU32(gplPipelines);
        linkedPipelineLibraryInfo.pLibraries                     = gplPipelines.data();

        VkGraphicsPipelineCreateInfo linkedPipelineInfo = initVulkanStructure(&linkedPipelineLibraryInfo);

        pipeline    = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &linkedPipelineInfo);
        framebuffer = makeFramebuffer(vkd, *m_device, *renderPass, 0, nullptr, renderingSize, renderingSize);
    }

    const std::array<VkShaderStageFlagBits, 2> stages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    const std::array<VkShaderEXT, 2> shaders = {
        *vertexShader,
        *fragmentShader,
    };

    const auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    VkRenderPassBeginInfo renderPassBeginInfo = initVulkanStructure();
    renderPassBeginInfo.renderPass            = *renderPass;
    renderPassBeginInfo.framebuffer           = *framebuffer;
    renderPassBeginInfo.renderArea            = makeRect2D(0, 0, renderingSize, renderingSize);
    renderPassBeginInfo.clearValueCount       = 0;
    renderPassBeginInfo.pClearValues          = nullptr;

    VkRenderingInfo renderingInfo      = initVulkanStructure();
    renderingInfo.flags                = 0;
    renderingInfo.renderArea           = makeRect2D(0, 0, renderingSize, renderingSize);
    renderingInfo.layerCount           = 1U;
    renderingInfo.viewMask             = 0;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments    = nullptr;
    renderingInfo.pDepthAttachment     = nullptr;
    renderingInfo.pStencilAttachment   = nullptr;

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);

    if (useShaderObject)
    {
        const uint32_t sampleMask = 0xffffffff;
        vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        vkd.cmdBindShadersEXT(cmdBuffer, 2, stages.data(), shaders.data());
        vkd.cmdSetRasterizerDiscardEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetCullMode(cmdBuffer, VK_CULL_MODE_NONE);
        vkd.cmdSetDepthTestEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetDepthWriteEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetStencilTestEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetDepthBiasEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetPolygonModeEXT(cmdBuffer, VK_POLYGON_MODE_FILL);
        vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, VK_SAMPLE_COUNT_1_BIT);
        vkd.cmdSetSampleMaskEXT(cmdBuffer, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
        vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, VK_FALSE);
        vkd.cmdSetViewportWithCount(cmdBuffer, 1, &viewport);
        vkd.cmdSetScissorWithCount(cmdBuffer, 1, &scissor);
        vkd.cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);
        vkd.cmdSetVertexInputEXT(cmdBuffer, 0, nullptr, 0, nullptr);
    }
    else
    {
        vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    }

    if (m_params.unbindFragShader)
    {
        const std::array<VkShaderEXT, 2> vertOnlyShaders = {
            *vertexShader,
            VK_NULL_HANDLE,
        };
        vkd.cmdBindShadersEXT(cmdBuffer, 2, stages.data(), vertOnlyShaders.data());
    }

    vkd.cmdDraw(cmdBuffer, vertexCount, 1, 0, 0);

    if (useShaderObject)
    {
        vkd.cmdEndRendering(cmdBuffer);
    }
    else
    {
        vkd.cmdEndRenderPass(cmdBuffer);
    }

    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers    = &cmdBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    std::array<tcu::Vec4, 4> result;
    deMemcpy(&result, outputBuffer->memory->getHostPtr(), sizeof(result));

    for (size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] != colors[i])
        {
            std::stringstream stream;
            stream << "At index " << i << ", expected " << std::hex << colors[i] << " but got " << result[i];
            return tcu::TestStatus::fail(stream.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceSwitchHeaps final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceSwitchHeaps(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    struct BufferDescriptorTracking
    {
        VkDescriptorType descriptorType;
        int buffer      = -1;
        uint32_t offset = 0;
        uint32_t size   = 0;
    };

    struct Pipeline
    {
        Move<VkDescriptorSetLayout> setLayout;
        Move<VkPipelineLayout> pipelineLayout;
        Move<VkPipeline> defaultPipeline;
        Move<VkPipeline> heapPipeline;
    };

    void setup();
    tcu::TestStatus check();

    void nextCommandBuffer();

    void bindDefaultHeaps();
    void bindResourceHeap(int heap);
    // void bindSamplerHeap(int heap);

    void fillStorageBuffer(uint32_t dstHeapIndex, uint32_t value, uint32_t size, bool useShader);
    void copyStorageBuffer(uint32_t dstHeapIndex, uint32_t srcHeapIndex, uint32_t size, bool useShader);
    void fillTexelBuffer(uint32_t dstHeapIndex, uint32_t value, uint32_t size);

    void writeBufferDescriptor(int heap, int heapIndex, int buffer, int offset, int size);
    void writeTexelDescriptor(int heap, int heapIndex, int buffer, int offset, int size);

    std::unique_ptr<Pipeline> compilePipeline(const char *name,
                                              const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
                                              const std::vector<VkDescriptorSetLayoutBinding> &bindings);

    VkDescriptorSet allocateDefaultDescriptor(VkDescriptorSetLayout setLayout);
    VkDescriptorBufferInfo createInlineUniformBuffer(size_t dataSize, const void *data);
    VkBufferView makeTexelBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

    TestParams m_params;

    uint32_t m_resourceStride = 0;

    VkDeviceSize m_resourceHeapUserSize = 0;
    VkDeviceSize m_resourceHeapSize     = 0;

    VkDeviceSize m_samplerHeapUserSize = 0;
    VkDeviceSize m_samplerHeapSize     = 0;

    std::vector<std::unique_ptr<Buffer>> m_resourceHeaps;
    std::vector<std::unique_ptr<Buffer>> m_samplerHeaps;

    std::vector<std::vector<uint32_t>> m_expectedDataBuffer;
    std::vector<std::unique_ptr<Buffer>> m_dataBuffers;

    Move<VkCommandPool> m_commandPool;
    std::vector<Move<VkCommandBuffer>> m_commandBuffers;
    VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;

    std::unique_ptr<Pipeline> m_fillBufferPipeline;
    std::unique_ptr<Pipeline> m_copyBufferPipeline;
    std::unique_ptr<Pipeline> m_fillTexelPipeline;
    std::unique_ptr<Pipeline> m_copyTexelPipeline;

    bool m_useDefaultHeaps    = true;
    int m_currentResourceHeap = -1;
    std::vector<std::vector<BufferDescriptorTracking>> m_resourceHeapDescriptorTracking;

    std::list<Move<VkDescriptorPool>> m_descriptorPools;
    std::list<Move<VkDescriptorSet>> m_descriptorSets;
    std::list<std::unique_ptr<Buffer>> m_helperBuffer;
    std::list<Move<VkBufferView>> m_helperBufferViews;
};

class DescriptorHeapTestCaseSwitchHeaps final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseSwitchHeaps(tcu::TestContext &testCtx, const std::string &name,
                                               const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceSwitchHeaps(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseSwitchHeaps::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string fillBuffer = "#version 450\n"
                             "layout(local_size_x = 1) in;\n"
                             "layout(binding = 0, std430) buffer OutputBuffer {\n"
                             "  uint outputBuffer[];\n"
                             "};\n"
                             "layout(binding = 1, std140) uniform InputBuffer {\n"
                             "  uint value;\n"
                             "};\n"
                             "void main() {\n"
                             "  outputBuffer[gl_GlobalInvocationID.x] = value;\n"
                             "}\n";

    std::string copyBuffer = "#version 450\n"
                             "layout(local_size_x = 1) in;\n"
                             "layout(binding = 0, std430) buffer InputBuffer {\n"
                             "  uint inputBuffer[];\n"
                             "};\n"
                             "layout(binding = 1, std430) buffer OutputBuffer {\n"
                             "  uint outputBuffer[];\n"
                             "};\n"
                             "void main() {\n"
                             "  outputBuffer[gl_GlobalInvocationID.x] = inputBuffer[gl_GlobalInvocationID.x];\n"
                             "}\n";

    std::string fillTexel = "#version 450\n"
                            "layout(local_size_x = 1) in;\n"
                            "layout(binding = 0, r32ui) uniform uimageBuffer outputBuffer;\n"
                            "layout(binding = 1, std140) uniform InputBuffer {\n"
                            "  uint value;\n"
                            "};\n"
                            "void main() {\n"
                            "  imageStore(outputBuffer, int(gl_GlobalInvocationID.x), uvec4(value, 0, 0, 0));\n"
                            "}\n";

    std::string copyTexel = "#version 450\n"
                            "layout(local_size_x = 1) in;\n"
                            "layout(binding = 0, r32ui) uniform uimageBuffer inputBuffer;\n"
                            "layout(binding = 1, r32ui) uniform uimageBuffer outputBuffer;\n"
                            "void main() {\n"
                            "  uvec4 value = imageLoad(inputBuffer, int(gl_GlobalInvocationID.x));\n"
                            "  imageStore(outputBuffer, int(gl_GlobalInvocationID.x), value);\n"
                            "}\n";

    programCollection.glslSources.add("fill_buffer") << glu::ComputeSource(fillBuffer);
    programCollection.glslSources.add("copy_buffer") << glu::ComputeSource(copyBuffer);
    programCollection.glslSources.add("fill_texel") << glu::ComputeSource(fillTexel);
    programCollection.glslSources.add("copy_texel") << glu::ComputeSource(copyTexel);
}

tcu::TestStatus DescriptorHeapTestInstanceSwitchHeaps::iterate()
{
    tcu::TestStatus result = tcu::TestStatus::pass("Pass");

    setup();

    bindDefaultHeaps();

    // Setup descriptor data from the CPU.
    // Heap 0:
    writeBufferDescriptor(0, 0, 0, 0, 512);
    writeBufferDescriptor(0, 1, 1, 0, 512);
    writeTexelDescriptor(0, 2, 0, 0, 512);

    // Heap 1:
    writeTexelDescriptor(1, 0, 0, 128, 128);
    writeTexelDescriptor(1, 1, 0, 256, 128);
    writeTexelDescriptor(1, 2, 0, 384, 128);

    // Heap 2:
    writeTexelDescriptor(2, 0, 0, 384, 128);
    writeTexelDescriptor(2, 1, 0, 128, 128);
    writeTexelDescriptor(2, 2, 0, 256, 128);

    fillStorageBuffer(0, 0x11111111, 16, true);
    fillStorageBuffer(1, 0x22222222, 16, true);

    bindResourceHeap(0);

    copyStorageBuffer(0, 1, 8, true);

    fillTexelBuffer(2, 0xcccccccc, 4);
    bindDefaultHeaps();
    fillTexelBuffer(2, 0xbbbbbbbb, 2);
    bindResourceHeap(0);
    fillTexelBuffer(2, 0xaaaaaaaa, 1);

    bindResourceHeap(1);
    fillTexelBuffer(0, 0x11111111, 10);
    fillTexelBuffer(1, 0x22222222, 9);
    fillTexelBuffer(2, 0x33333333, 8);

    bindResourceHeap(2);
    fillTexelBuffer(0, 0x44444444, 7);

    if (m_params.enableNVCommandBufferInheritance)
    {
        if ((result = check()).isFail())
            return result;
    }

    fillTexelBuffer(1, 0x55555555, 6);
    fillTexelBuffer(2, 0x66666666, 5);

    bindDefaultHeaps();

    if (m_params.enableNVCommandBufferInheritance)
    {
        if ((result = check()).isFail())
            return result;
    }

    fillTexelBuffer(2, 0x77777777, 4);

    if ((result = check()).isFail())
        return result;

    return result;
}

void DescriptorHeapTestInstanceSwitchHeaps::setup()
{
    const auto &vkd = m_device.getDriver();

    const int numResourceHeaps = 3;
    const int numSamplerHeaps  = 1;

    const int userResourcesPerHeap = 32;
    const int userSamplersPerHeap  = 4;

    const int numDataBuffers      = 8;
    const int dataBufferSizeWords = 512;
    const auto dataBufferFlags =
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
        VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR;

    m_resourceStride = static_cast<uint32_t>(getResourceDescriptorStride(m_descriptorHeapProperties));

    m_resourceHeapUserSize = alignUp(VkDeviceSize{userResourcesPerHeap * m_resourceStride},
                                     m_descriptorHeapProperties.resourceHeapAlignment);

    m_resourceHeapSize = m_resourceHeapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    m_samplerHeapUserSize = userSamplersPerHeap * m_descriptorHeapProperties.samplerDescriptorSize;
    m_samplerHeapSize     = m_samplerHeapUserSize + m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;

    const auto heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    m_resourceHeaps.resize(numResourceHeaps);
    m_samplerHeaps.resize(numSamplerHeaps);

    std::generate(m_resourceHeaps.begin(), m_resourceHeaps.end(),
                  [&] { return createBufferAndMemory(m_resourceHeapSize, heapFlags); });
    std::generate(m_samplerHeaps.begin(), m_samplerHeaps.end(),
                  [&] { return createBufferAndMemory(m_samplerHeapSize, heapFlags); });

    m_expectedDataBuffer.resize(numDataBuffers, std::vector<uint32_t>(dataBufferSizeWords));
    m_dataBuffers.resize(numDataBuffers);
    std::generate(m_dataBuffers.begin(), m_dataBuffers.end(),
                  [&] { return createBufferAndMemory(dataBufferSizeWords * sizeof(uint32_t), dataBufferFlags); });

    for (auto &dataBuffer : m_dataBuffers)
    {
        deMemset(dataBuffer->memory->getHostPtr(), 0, dataBufferSizeWords * sizeof(uint32_t));
    }

    m_resourceHeapDescriptorTracking.resize(numResourceHeaps,
                                            std::vector<BufferDescriptorTracking>(userResourcesPerHeap));

    m_commandPool = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);

    std::vector<VkDescriptorSetLayoutBinding> fillBufferBindings{
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr),
        makeDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr),
    };

    std::vector<VkDescriptorSetAndBindingMappingEXT> fillMappings(2);
    fillMappings[0]                                      = initVulkanStructure();
    fillMappings[0].descriptorSet                        = 0;
    fillMappings[0].firstBinding                         = 0;
    fillMappings[0].bindingCount                         = 1;
    fillMappings[0].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    fillMappings[0].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    fillMappings[0].sourceData.pushIndex.heapOffset      = 0;
    fillMappings[0].sourceData.pushIndex.pushOffset      = 0;
    fillMappings[0].sourceData.pushIndex.heapIndexStride = m_resourceStride;
    fillMappings[0].sourceData.pushIndex.heapArrayStride = m_resourceStride;
    fillMappings[1]                                      = initVulkanStructure();
    fillMappings[1].descriptorSet                        = 0;
    fillMappings[1].firstBinding                         = 1;
    fillMappings[1].bindingCount                         = 1;
    fillMappings[1].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    fillMappings[1].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
    fillMappings[1].sourceData.pushDataOffset            = 4;

    m_fillBufferPipeline = compilePipeline("fill_buffer", fillMappings, fillBufferBindings);

    std::vector<VkDescriptorSetLayoutBinding> bufferCopyBindings = {
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr),
        makeDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr),
    };

    std::vector<VkDescriptorSetAndBindingMappingEXT> copyMappings(2);
    copyMappings[0]                                      = initVulkanStructure();
    copyMappings[0].descriptorSet                        = 0;
    copyMappings[0].firstBinding                         = 0;
    copyMappings[0].bindingCount                         = 1;
    copyMappings[0].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    copyMappings[0].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    copyMappings[0].sourceData.pushIndex.heapOffset      = 0;
    copyMappings[0].sourceData.pushIndex.pushOffset      = 0;
    copyMappings[0].sourceData.pushIndex.heapIndexStride = m_resourceStride;
    copyMappings[0].sourceData.pushIndex.heapArrayStride = m_resourceStride;
    copyMappings[1]                                      = initVulkanStructure();
    copyMappings[1].descriptorSet                        = 0;
    copyMappings[1].firstBinding                         = 1;
    copyMappings[1].bindingCount                         = 1;
    copyMappings[1].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    copyMappings[1].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    copyMappings[1].sourceData.pushIndex.heapOffset      = 0;
    copyMappings[1].sourceData.pushIndex.pushOffset      = 4;
    copyMappings[1].sourceData.pushIndex.heapIndexStride = m_resourceStride;
    copyMappings[1].sourceData.pushIndex.heapArrayStride = m_resourceStride;

    m_copyBufferPipeline = compilePipeline("copy_buffer", copyMappings, bufferCopyBindings);

    std::vector<VkDescriptorSetLayoutBinding> texelFillBindings{
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr),
        makeDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr),
    };

    m_fillTexelPipeline = compilePipeline("fill_texel", fillMappings, texelFillBindings);

    std::vector<VkDescriptorSetLayoutBinding> texelCopyBindings{
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr),
        makeDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr),
    };

    m_copyTexelPipeline = compilePipeline("copy_texel", copyMappings, texelCopyBindings);

    nextCommandBuffer();
}

tcu::TestStatus DescriptorHeapTestInstanceSwitchHeaps::check()
{
    const auto &vkd = m_device.getDriver();

    if (m_cmdBuf)
    {
        VK_CHECK(vkd.endCommandBuffer(m_cmdBuf));
    }

    if (!m_commandBuffers.empty())
    {
        std::vector<VkCommandBuffer> cmdBufs(m_commandBuffers.size());
        std::transform(m_commandBuffers.begin(), m_commandBuffers.end(), cmdBufs.begin(),
                       [](Move<VkCommandBuffer> &x) { return *x; });

        VkSubmitInfo submitInfo       = initVulkanStructure();
        submitInfo.commandBufferCount = de::sizeU32(cmdBufs);
        submitInfo.pCommandBuffers    = cmdBufs.data();

        VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK(vkd.deviceWaitIdle(*m_device));
    }

    for (size_t buffer = 0; buffer < m_dataBuffers.size(); ++buffer)
    {
        const size_t size = m_expectedDataBuffer[buffer].size();
        std::vector<uint32_t> gpuMemory(size);
        deMemcpy(gpuMemory.data(), m_dataBuffers[buffer]->memory->getHostPtr(), size * sizeof(uint32_t));

        for (size_t i = 0; i < size; ++i)
        {
            const uint32_t expectedValue = m_expectedDataBuffer[buffer][i];
            if (gpuMemory[i] != expectedValue)
            {
                std::stringstream stream;
                stream << "At buffer " << buffer << " index " << i << ", expected 0x" << std::hex << expectedValue
                       << " but got 0x" << gpuMemory[i];

                return tcu::TestStatus::fail(stream.str());
            }
        }
    }

    m_commandBuffers.clear();
    m_cmdBuf = VK_NULL_HANDLE;

    nextCommandBuffer();

    return tcu::TestStatus::pass("Pass");
}

void DescriptorHeapTestInstanceSwitchHeaps::nextCommandBuffer()
{
    const auto &vkd = m_device.getDriver();

    if (m_cmdBuf)
    {
        VK_CHECK(vkd.endCommandBuffer(m_cmdBuf));
    }

    m_commandBuffers.push_back(
        allocateCommandBuffer(vkd, *m_device, m_commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    m_cmdBuf = *m_commandBuffers.back();

    if (!m_params.enableNVCommandBufferInheritance)
    {
        bindDefaultHeaps();
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = initVulkanStructure();
    commandBufferBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkd.beginCommandBuffer(m_cmdBuf, &commandBufferBeginInfo);
}

void DescriptorHeapTestInstanceSwitchHeaps::bindDefaultHeaps()
{
    m_useDefaultHeaps     = true;
    m_currentResourceHeap = 0;
}

void DescriptorHeapTestInstanceSwitchHeaps::bindResourceHeap(int heap)
{
    const auto &vkd = m_device.getDriver();

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = m_resourceHeaps[heap]->address;
    bindHeapInfo.heapRange.size      = m_resourceHeapSize;
    bindHeapInfo.reservedRangeOffset = m_resourceHeapUserSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    vkd.cmdBindResourceHeapEXT(m_cmdBuf, &bindHeapInfo);

    m_currentResourceHeap = heap;
    m_useDefaultHeaps     = false;
}

// void DescriptorHeapTestInstanceSwitchHeaps::bindSamplerHeap(int heap)
// {
//     const auto &vkd = m_device.getDriver();
//
//     VkBindHeapInfoEXT bindHeapInfo = initVulkanStructure();
//     bindHeapInfo.heapRange.address = m_samplerHeaps[heap]->address;
//     bindHeapInfo.heapRange.size = m_samplerHeapSize;
//     bindHeapInfo.reservedRangeOffset = m_samplerHeapUserSize;
//
//     vkd.cmdBindSamplerHeapEXT(m_cmdBuf, &bindHeapInfo);
// }

void DescriptorHeapTestInstanceSwitchHeaps::writeBufferDescriptor(int heap, int heapIndex, int buffer, int offset,
                                                                  int size)
{
    const auto &vkd = m_device.getDriver();

    VkHostAddressRangeEXT descriptor{};
    descriptor.address =
        static_cast<char *>(m_resourceHeaps[heap]->memory->getHostPtr()) + heapIndex * m_resourceStride;
    descriptor.size = m_resourceStride;

    VkDeviceAddressRangeEXT bufferAddressRange{};
    bufferAddressRange.address = m_dataBuffers[buffer]->address + offset * sizeof(uint32_t);
    bufferAddressRange.size    = size * sizeof(uint32_t);

    VkResourceDescriptorInfoEXT resourceDescriptorInfo = initVulkanStructure();
    resourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resourceDescriptorInfo.data.pAddressRange          = &bufferAddressRange;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceDescriptorInfo, &descriptor));

    BufferDescriptorTracking &tracking = m_resourceHeapDescriptorTracking[heap][heapIndex];
    tracking.descriptorType            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tracking.buffer                    = buffer;
    tracking.offset                    = offset;
    tracking.size                      = size;
}

void DescriptorHeapTestInstanceSwitchHeaps::writeTexelDescriptor(int heap, int heapIndex, int buffer, int offset,
                                                                 int size)
{
    const auto &vkd = m_device.getDriver();

    VkHostAddressRangeEXT descriptor{};
    descriptor.address =
        static_cast<char *>(m_resourceHeaps[heap]->memory->getHostPtr()) + heapIndex * m_resourceStride;
    descriptor.size = m_resourceStride;

    VkTexelBufferDescriptorInfoEXT texelBufferInfo = initVulkanStructure();
    texelBufferInfo.format                         = VK_FORMAT_R32_UINT;
    texelBufferInfo.addressRange.address           = m_dataBuffers[buffer]->address + offset * sizeof(uint32_t);
    texelBufferInfo.addressRange.size              = size * sizeof(uint32_t);

    VkResourceDescriptorInfoEXT resourceDescriptorInfo = initVulkanStructure();
    resourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    resourceDescriptorInfo.data.pTexelBuffer           = &texelBufferInfo;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceDescriptorInfo, &descriptor));

    BufferDescriptorTracking &tracking = m_resourceHeapDescriptorTracking[heap][heapIndex];
    tracking.descriptorType            = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    tracking.buffer                    = buffer;
    tracking.offset                    = offset;
    tracking.size                      = size;
}

void DescriptorHeapTestInstanceSwitchHeaps::fillStorageBuffer(uint32_t dstHeapIndex, uint32_t value, uint32_t size,
                                                              bool useShader)
{
    const auto &vkd = m_device.getDriver();

    const auto &dstTracking = m_resourceHeapDescriptorTracking[m_currentResourceHeap][dstHeapIndex];

    DE_ASSERT(dstTracking.buffer >= 0);

    std::fill_n(m_expectedDataBuffer[dstTracking.buffer].begin() + dstTracking.offset, size, value);

    if (useShader)
    {
        if (m_useDefaultHeaps)
        {
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            if (!m_params.enablePushDescriptors)
            {
                descriptorSet = allocateDefaultDescriptor(*m_fillBufferPipeline->setLayout);
            }

            auto dstBufferInfo =
                makeDescriptorBufferInfo(*m_dataBuffers[dstTracking.buffer]->buffer,
                                         dstTracking.offset * sizeof(uint32_t), dstTracking.size * sizeof(uint32_t));
            auto inlineUniformBuffer = createInlineUniformBuffer(sizeof(value), &value);

            std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dstBufferInfo, nullptr},
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &inlineUniformBuffer, nullptr},
            };

            if (m_params.enablePushDescriptors)
            {
                vkd.cmdPushDescriptorSet(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         *m_fillBufferPipeline->pipelineLayout, 0, de::sizeU32(writeDescriptorSets),
                                         writeDescriptorSets.data());
            }
            else
            {
                vkd.updateDescriptorSets(*m_device, de::sizeU32(writeDescriptorSets), writeDescriptorSets.data(), 0,
                                         nullptr);
                vkd.cmdBindDescriptorSets(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                          *m_fillBufferPipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            }

            vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillBufferPipeline->defaultPipeline);
        }
        else
        {
            const std::array<uint32_t, 2> pushData = {dstHeapIndex, value};

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = 0;
            pushDataInfo.data.size         = sizeof(pushData);
            pushDataInfo.data.address      = pushData.data();

            vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillBufferPipeline->heapPipeline);
            vkd.cmdPushDataEXT(m_cmdBuf, &pushDataInfo);
        }

        vkd.cmdDispatch(m_cmdBuf, size, 1, 1);
    }
    else
    {
        vkd.cmdFillBuffer(m_cmdBuf, *m_dataBuffers[dstTracking.buffer]->buffer, dstTracking.offset * sizeof(uint32_t),
                          size * sizeof(uint32_t), value);
    }

    VkMemoryBarrier memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

    vkd.cmdPipelineBarrier(m_cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                           &memoryBarrier, 0, nullptr, 0, nullptr);
}

void DescriptorHeapTestInstanceSwitchHeaps::copyStorageBuffer(uint32_t dstHeapIndex, uint32_t srcHeapIndex,
                                                              uint32_t size, bool useShader)
{
    const auto &vkd = m_device.getDriver();

    const auto &dstTracking = m_resourceHeapDescriptorTracking[m_currentResourceHeap][dstHeapIndex];
    const auto &srcTracking = m_resourceHeapDescriptorTracking[m_currentResourceHeap][srcHeapIndex];

    DE_ASSERT(dstTracking.buffer >= 0);
    DE_ASSERT(srcTracking.buffer >= 0);

    const bool outOfBoundsRead = size > srcTracking.size;
    const int copySize         = outOfBoundsRead ? srcTracking.size : size;
    const int zeroSize         = outOfBoundsRead ? (size - srcTracking.size) : 0;

    std::copy_n(m_expectedDataBuffer[srcTracking.buffer].begin() + srcTracking.offset, copySize,
                m_expectedDataBuffer[dstTracking.buffer].begin() + dstTracking.offset);

    if (zeroSize > 0)
    {
        std::fill_n(m_expectedDataBuffer[dstTracking.buffer].begin() + srcTracking.size, zeroSize, 0);
    }

    if (useShader)
    {
        if (m_useDefaultHeaps)
        {
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            if (!m_params.enablePushDescriptors)
            {
                descriptorSet = allocateDefaultDescriptor(*m_copyBufferPipeline->setLayout);
            }

            auto srcBufferInfo =
                makeDescriptorBufferInfo(*m_dataBuffers[srcTracking.buffer]->buffer,
                                         srcTracking.offset * sizeof(uint32_t), srcTracking.size * sizeof(uint32_t));
            auto dstBufferInfo =
                makeDescriptorBufferInfo(*m_dataBuffers[dstTracking.buffer]->buffer,
                                         dstTracking.offset * sizeof(uint32_t), dstTracking.size * sizeof(uint32_t));

            std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &srcBufferInfo, nullptr},
                {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dstBufferInfo, nullptr},
            };

            if (m_params.enablePushDescriptors)
            {
                vkd.cmdPushDescriptorSet(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         *m_copyBufferPipeline->pipelineLayout, 0, de::sizeU32(writeDescriptorSets),
                                         writeDescriptorSets.data());
            }
            else
            {
                vkd.updateDescriptorSets(*m_device, de::sizeU32(writeDescriptorSets), writeDescriptorSets.data(), 0,
                                         nullptr);
                vkd.cmdBindDescriptorSets(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                          *m_copyBufferPipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            }

            vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_copyBufferPipeline->defaultPipeline);
        }
        else
        {
            const std::array<uint32_t, 2> pushData = {srcHeapIndex, dstHeapIndex};

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = 0;
            pushDataInfo.data.size         = sizeof(pushData);
            pushDataInfo.data.address      = pushData.data();

            vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_copyBufferPipeline->heapPipeline);
            vkd.cmdPushDataEXT(m_cmdBuf, &pushDataInfo);
        }

        vkd.cmdDispatch(m_cmdBuf, size, 1, 1);
    }
    else
    {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = srcTracking.offset * sizeof(uint32_t);
        copyRegion.dstOffset = dstTracking.offset * sizeof(uint32_t);
        copyRegion.size      = size * sizeof(uint32_t);

        vkd.cmdCopyBuffer(m_cmdBuf, *m_dataBuffers[srcTracking.buffer]->buffer,
                          *m_dataBuffers[dstTracking.buffer]->buffer, 1, &copyRegion);
    }

    VkMemoryBarrier memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

    vkd.cmdPipelineBarrier(m_cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                           &memoryBarrier, 0, nullptr, 0, nullptr);
}

void DescriptorHeapTestInstanceSwitchHeaps::fillTexelBuffer(uint32_t dstHeapIndex, uint32_t value, uint32_t size)
{
    const auto &vkd = m_device.getDriver();

    const auto &dstTracking = m_resourceHeapDescriptorTracking[m_currentResourceHeap][dstHeapIndex];

    DE_ASSERT(dstTracking.buffer >= 0);

    std::fill_n(m_expectedDataBuffer[dstTracking.buffer].begin() + dstTracking.offset, size, value);

    if (m_useDefaultHeaps)
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (!m_params.enablePushDescriptors)
        {
            descriptorSet = allocateDefaultDescriptor(*m_fillTexelPipeline->setLayout);
        }

        auto texelBuffer         = makeTexelBuffer(*m_dataBuffers[dstTracking.buffer]->buffer,
                                                   dstTracking.offset * sizeof(uint32_t), dstTracking.size * sizeof(uint32_t));
        auto inlineUniformBuffer = createInlineUniformBuffer(sizeof(value), &value);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1,
             VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, nullptr, nullptr, &texelBuffer},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             nullptr, &inlineUniformBuffer, nullptr},
        };

        if (m_params.enablePushDescriptors)
        {
            vkd.cmdPushDescriptorSet(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillTexelPipeline->pipelineLayout, 0,
                                     de::sizeU32(writeDescriptorSets), writeDescriptorSets.data());
        }
        else
        {
            vkd.updateDescriptorSets(*m_device, de::sizeU32(writeDescriptorSets), writeDescriptorSets.data(), 0,
                                     nullptr);
            vkd.cmdBindDescriptorSets(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillTexelPipeline->pipelineLayout, 0,
                                      1, &descriptorSet, 0, nullptr);
        }

        vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillTexelPipeline->defaultPipeline);
    }
    else
    {
        const std::array<uint32_t, 2> pushData = {dstHeapIndex, value};

        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.offset            = 0;
        pushDataInfo.data.size         = sizeof(pushData);
        pushDataInfo.data.address      = pushData.data();

        vkd.cmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_fillTexelPipeline->heapPipeline);
        vkd.cmdPushDataEXT(m_cmdBuf, &pushDataInfo);
    }
    vkd.cmdDispatch(m_cmdBuf, size, 1, 1);

    VkMemoryBarrier memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

    vkd.cmdPipelineBarrier(m_cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                           &memoryBarrier, 0, nullptr, 0, nullptr);
}

std::unique_ptr<DescriptorHeapTestInstanceSwitchHeaps::Pipeline> DescriptorHeapTestInstanceSwitchHeaps::compilePipeline(
    const char *name, const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings,
    const std::vector<VkDescriptorSetLayoutBinding> &bindings)
{
    const auto &vkd = m_device.getDriver();

    const auto shaderModule = createShaderModule(vkd, *m_device, getShaderBinary(name));

    VkPipelineCreateFlags2CreateInfoKHR createFlags2CreateInfo = initVulkanStructure();
    createFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingsInfo = initVulkanStructure();
    mappingsInfo.mappingCount                                  = de::sizeU32(mappings);
    mappingsInfo.pMappings                                     = mappings.data();

    VkComputePipelineCreateInfo heapPipelineCreateInfo = initVulkanStructure();
    heapPipelineCreateInfo.pNext                       = &createFlags2CreateInfo;
    heapPipelineCreateInfo.stage.sType                 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    heapPipelineCreateInfo.stage.pNext                 = &mappingsInfo;
    heapPipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    heapPipelineCreateInfo.stage.module                = *shaderModule;
    heapPipelineCreateInfo.stage.pName                 = "main";
    auto heapPipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &heapPipelineCreateInfo);

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = initVulkanStructure();
    if (m_params.enablePushDescriptors)
        descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    descriptorSetLayoutCreateInfo.bindingCount = de::sizeU32(bindings);
    descriptorSetLayoutCreateInfo.pBindings    = bindings.data();
    auto descriptorSetLayout = createDescriptorSetLayout(vkd, *m_device, &descriptorSetLayoutCreateInfo);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.setLayoutCount             = 1;
    pipelineLayoutCreateInfo.pSetLayouts                = &descriptorSetLayout.get();
    auto pipelineLayout = createPipelineLayout(vkd, *m_device, &pipelineLayoutCreateInfo);

    VkComputePipelineCreateInfo defaultPipelineCreateInfo = initVulkanStructure();
    defaultPipelineCreateInfo.stage.sType                 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    defaultPipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    defaultPipelineCreateInfo.stage.module                = *shaderModule;
    defaultPipelineCreateInfo.stage.pName                 = "main";
    defaultPipelineCreateInfo.layout                      = *pipelineLayout;
    auto defaultPipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &defaultPipelineCreateInfo);

    auto result             = std::make_unique<Pipeline>();
    result->setLayout       = std::move(descriptorSetLayout);
    result->pipelineLayout  = std::move(pipelineLayout);
    result->defaultPipeline = std::move(defaultPipeline);
    result->heapPipeline    = std::move(heapPipeline);
    return result;
}

VkDescriptorSet DescriptorHeapTestInstanceSwitchHeaps::allocateDefaultDescriptor(VkDescriptorSetLayout setLayout)
{
    const auto &vkd                             = m_device.getDriver();
    std::vector<VkDescriptorPoolSize> poolSizes = {
        makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2),
        makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 2),
        makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
    };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = initVulkanStructure();
    descriptorPoolCreateInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolCreateInfo.maxSets                    = 1U;
    descriptorPoolCreateInfo.poolSizeCount              = de::sizeU32(poolSizes);
    descriptorPoolCreateInfo.pPoolSizes                 = poolSizes.data();
    auto &descriptorPool =
        m_descriptorPools.emplace_back(createDescriptorPool(vkd, *m_device, &descriptorPoolCreateInfo));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = initVulkanStructure();
    descriptorSetAllocateInfo.descriptorPool              = *descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount          = 1;
    descriptorSetAllocateInfo.pSetLayouts                 = &setLayout;

    return *m_descriptorSets.emplace_back(allocateDescriptorSet(vkd, *m_device, &descriptorSetAllocateInfo));
}

VkDescriptorBufferInfo DescriptorHeapTestInstanceSwitchHeaps::createInlineUniformBuffer(size_t dataSize,
                                                                                        const void *data)
{
    auto &buffer =
        m_helperBuffer.emplace_back(createBufferAndMemory(dataSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR));
    deMemcpy(buffer->memory->getHostPtr(), data, dataSize);
    return makeDescriptorBufferInfo(*buffer->buffer, 0, VK_WHOLE_SIZE);
}

VkBufferView DescriptorHeapTestInstanceSwitchHeaps::makeTexelBuffer(VkBuffer buffer, VkDeviceSize offset,
                                                                    VkDeviceSize range)
{
    const auto &vkd = m_device.getDriver();
    return *m_helperBufferViews.emplace_back(makeBufferView(vkd, *m_device, buffer, VK_FORMAT_R32_UINT, offset, range));
}

class DescriptorHeapTestInstanceConcurrentHeapSet final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceConcurrentHeapSet(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseConcurrentHeapSet final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseConcurrentHeapSet(tcu::TestContext &testCtx, const std::string &name,
                                                     const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceConcurrentHeapSet(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseConcurrentHeapSet::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0) uniform isamplerBuffer inputBuffer;
layout(binding = 1, r32i) uniform iimageBuffer outputBuffer;
void main() {
  ivec4 value = texelFetch(inputBuffer, int(gl_GlobalInvocationID.x));
  imageStore(outputBuffer, int(gl_GlobalInvocationID.x), value);
}
)";
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceConcurrentHeapSet::iterate()
{
    const auto &vk                           = m_device.getDriver();
    const uint32_t bufferSize                = 256;
    const VkDeviceSize bufferMemorySize      = bufferSize * sizeof(int32_t);
    const VkDeviceSize imageDescriptorStride = getImageDescriptorStride(m_descriptorHeapProperties);

    // Create input and output buffers for both descriptor heap and regular descriptor sets
    auto inputBufferHeap     = createBufferAndMemory(bufferMemorySize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    auto outputBufferHeap    = createBufferAndMemory(bufferMemorySize, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    auto inputBufferRegular  = createBufferAndMemory(bufferMemorySize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    auto outputBufferRegular = createBufferAndMemory(bufferMemorySize, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    // Initialize input buffers with random data
    de::Random rnd(m_params.seed);
    std::vector<int32_t> inputData(bufferSize);
    for (auto &value : inputData)
    {
        value = rnd.getInt32();
    }

    deMemcpy(inputBufferHeap->memory->getHostPtr(), inputData.data(), bufferMemorySize);
    deMemcpy(inputBufferRegular->memory->getHostPtr(), inputData.data(), bufferMemorySize);

    // Create texel buffer views
    VkBufferViewCreateInfo inputBufferViewCreateInfo = {};
    inputBufferViewCreateInfo.sType                  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    inputBufferViewCreateInfo.pNext                  = nullptr;
    inputBufferViewCreateInfo.flags                  = 0;
    inputBufferViewCreateInfo.buffer                 = *inputBufferRegular->buffer;
    inputBufferViewCreateInfo.format                 = VK_FORMAT_R32_SINT;
    inputBufferViewCreateInfo.offset                 = 0;
    inputBufferViewCreateInfo.range                  = VK_WHOLE_SIZE;

    VkBufferViewCreateInfo outputBufferViewCreateInfo = {};
    outputBufferViewCreateInfo.sType                  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    outputBufferViewCreateInfo.pNext                  = nullptr;
    outputBufferViewCreateInfo.flags                  = 0;
    outputBufferViewCreateInfo.buffer                 = *outputBufferRegular->buffer;
    outputBufferViewCreateInfo.format                 = VK_FORMAT_R32_SINT;
    outputBufferViewCreateInfo.offset                 = 0;
    outputBufferViewCreateInfo.range                  = VK_WHOLE_SIZE;

    auto inputTexelBufferViewRegular  = createBufferView(vk, *m_device, &inputBufferViewCreateInfo);
    auto outputTexelBufferViewRegular = createBufferView(vk, *m_device, &outputBufferViewCreateInfo);

    // Create descriptor heap
    const VkDeviceSize userDescriptorHeapSize =
        alignUp(imageDescriptorStride * 2, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize descriptorHeapSize =
        userDescriptorHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto descriptorHeap = createBufferAndMemory(descriptorHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    // Write descriptors to the descriptor heap
    VkHostAddressRangeEXT inputDescriptorHeap{};
    inputDescriptorHeap.address = descriptorHeap->memory->getHostPtr();
    inputDescriptorHeap.size    = static_cast<size_t>(imageDescriptorStride);

    VkTexelBufferDescriptorInfoEXT inputTexelBufferInfo = initVulkanStructure();
    inputTexelBufferInfo.addressRange.address           = inputBufferHeap->address;
    inputTexelBufferInfo.addressRange.size              = bufferMemorySize;
    inputTexelBufferInfo.format                         = VK_FORMAT_R32_SINT;

    VkResourceDescriptorInfoEXT inputDescriptorInfo = initVulkanStructure();
    inputDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    inputDescriptorInfo.data.pTexelBuffer           = &inputTexelBufferInfo;

    VK_CHECK(vk.writeResourceDescriptorsEXT(*m_device, 1, &inputDescriptorInfo, &inputDescriptorHeap));

    VkHostAddressRangeEXT outputDescriptorHeap{};
    outputDescriptorHeap.address = static_cast<char *>(descriptorHeap->memory->getHostPtr()) + imageDescriptorStride;
    outputDescriptorHeap.size    = static_cast<size_t>(imageDescriptorStride);

    VkTexelBufferDescriptorInfoEXT outputTexelBufferInfo = initVulkanStructure();
    outputTexelBufferInfo.addressRange.address           = outputBufferHeap->address;
    outputTexelBufferInfo.addressRange.size              = bufferMemorySize;
    outputTexelBufferInfo.format                         = VK_FORMAT_R32_SINT;

    VkResourceDescriptorInfoEXT outputDescriptorInfo = initVulkanStructure();
    outputDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    outputDescriptorInfo.data.pTexelBuffer           = &outputTexelBufferInfo;

    VK_CHECK(vk.writeResourceDescriptorsEXT(*m_device, 1, &outputDescriptorInfo, &outputDescriptorHeap));

    // Create descriptor set layout and pipeline layout for regular descriptor sets
    VkDescriptorSetLayoutBinding bindings[2] = {
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr),
        makeDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr),
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = initVulkanStructure();
    descriptorSetLayoutCreateInfo.bindingCount                    = 2;
    descriptorSetLayoutCreateInfo.pBindings                       = bindings;

    auto descriptorSetLayout = createDescriptorSetLayout(vk, *m_device, &descriptorSetLayoutCreateInfo);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.setLayoutCount             = 1;
    pipelineLayoutCreateInfo.pSetLayouts                = &descriptorSetLayout.get();

    auto pipelineLayout = createPipelineLayout(vk, *m_device, &pipelineLayoutCreateInfo);

    // Allocate and write descriptor sets
    VkDescriptorPoolSize poolSizes[2] = {
        makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2),
        makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 2),
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = initVulkanStructure();
    descriptorPoolCreateInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolCreateInfo.maxSets                    = 1;
    descriptorPoolCreateInfo.poolSizeCount              = 2;
    descriptorPoolCreateInfo.pPoolSizes                 = poolSizes;

    auto descriptorPool = createDescriptorPool(vk, *m_device, &descriptorPoolCreateInfo);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = initVulkanStructure();
    descriptorSetAllocateInfo.descriptorPool              = *descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount          = 1;
    descriptorSetAllocateInfo.pSetLayouts                 = &descriptorSetLayout.get();

    auto descriptorSet = allocateDescriptorSet(vk, *m_device, &descriptorSetAllocateInfo);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // sType
            nullptr,                                 // pNext
            *descriptorSet,                          // dstSet
            0,                                       // dstBinding
            0,                                       // dstArrayElement
            1,                                       // descriptorCount
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, // descriptorType
            nullptr,                                 // pImageInfo
            nullptr,                                 // pBufferInfo
            &inputTexelBufferViewRegular.get()       // pTexelBufferView
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  // sType
            nullptr,                                 // pNext
            *descriptorSet,                          // dstSet
            1,                                       // dstBinding
            0,                                       // dstArrayElement
            1,                                       // descriptorCount
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, // descriptorType
            nullptr,                                 // pImageInfo
            nullptr,                                 // pBufferInfo
            &outputTexelBufferViewRegular.get()      // pTexelBufferView
        },
    };

    vk.updateDescriptorSets(*m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0,
                            nullptr);

    // Create compute pipeline
    auto computeShaderModule = createShaderModule(vk, *m_device, getShaderBinary("compute"));

    // Define descriptor set and binding mappings with constant offsets
    std::array<VkDescriptorSetAndBindingMappingEXT, 2> descriptorHeapMappings{};
    descriptorHeapMappings[0]               = initVulkanStructure();
    descriptorHeapMappings[0].descriptorSet = 0;
    descriptorHeapMappings[0].firstBinding  = 0;
    descriptorHeapMappings[0].bindingCount  = 1;
    descriptorHeapMappings[0].resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    descriptorHeapMappings[0].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    descriptorHeapMappings[0].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(0 * imageDescriptorStride);
    descriptorHeapMappings[0].sourceData.constantOffset.heapArrayStride = 0;

    descriptorHeapMappings[1]               = initVulkanStructure();
    descriptorHeapMappings[1].descriptorSet = 0;
    descriptorHeapMappings[1].firstBinding  = 1;
    descriptorHeapMappings[1].bindingCount  = 1;
    descriptorHeapMappings[1].resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    descriptorHeapMappings[1].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    descriptorHeapMappings[1].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(1 * imageDescriptorStride);
    descriptorHeapMappings[1].sourceData.constantOffset.heapArrayStride = 0;

    // Attach the descriptor heap mappings to the pipeline
    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(descriptorHeapMappings.size());
    mappingInfo.pMappings                                     = descriptorHeapMappings.data();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeShaderModule;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.layout                      = *pipelineLayout;

    // Regular pipeline
    auto regularPipeline = createComputePipeline(vk, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Attach the descriptor heap pNext chains for the descriptor heap pipeline
    pipelineCreateInfo.pNext       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage.pNext = &mappingInfo;
    pipelineCreateInfo.layout      = VK_NULL_HANDLE;
    auto descriptorHeapPipeline    = createComputePipeline(vk, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Record command buffers
    auto cmdPool          = makeCommandPool(vk, *m_device, m_queueFamilyIndex);
    auto cmdBufferHeap    = allocateCommandBuffer(vk, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBufferRegular = allocateCommandBuffer(vk, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Command buffer for descriptor heap
    beginCommandBuffer(vk, *cmdBufferHeap);

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = descriptorHeap->address;
    bindHeapInfo.heapRange.size      = descriptorHeapSize;
    bindHeapInfo.reservedRangeOffset = userDescriptorHeapSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;
    vk.cmdBindResourceHeapEXT(*cmdBufferHeap, &bindHeapInfo);

    vk.cmdBindPipeline(*cmdBufferHeap, VK_PIPELINE_BIND_POINT_COMPUTE, *descriptorHeapPipeline);
    vk.cmdDispatch(*cmdBufferHeap, bufferSize, 1, 1);
    endCommandBuffer(vk, *cmdBufferHeap);

    // Command buffer for regular descriptor sets
    beginCommandBuffer(vk, *cmdBufferRegular);
    vk.cmdBindPipeline(*cmdBufferRegular, VK_PIPELINE_BIND_POINT_COMPUTE, *regularPipeline);
    vk.cmdBindDescriptorSets(*cmdBufferRegular, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1,
                             &descriptorSet.get(), 0, nullptr);
    vk.cmdDispatch(*cmdBufferRegular, bufferSize, 1, 1);
    endCommandBuffer(vk, *cmdBufferRegular);

    // Submit command buffers to different queues
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;

    submitInfo.pCommandBuffers = &cmdBufferHeap.get();
    VK_CHECK(vk.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));

    submitInfo.pCommandBuffers = &cmdBufferRegular.get();
    VK_CHECK(vk.queueSubmit(m_queues[1], 1, &submitInfo, VK_NULL_HANDLE));

    // Wait for completion
    VK_CHECK(vk.deviceWaitIdle(*m_device));

    // Verify results
    std::vector<int32_t> outputDataHeap(bufferSize);
    std::vector<int32_t> outputDataRegular(bufferSize);

    deMemcpy(outputDataHeap.data(), outputBufferHeap->memory->getHostPtr(), bufferMemorySize);
    deMemcpy(outputDataRegular.data(), outputBufferRegular->memory->getHostPtr(), bufferMemorySize);

    if ((outputDataHeap != inputData) || (outputDataRegular != inputData))
    {
        return tcu::TestStatus::fail("Output data does not match input data");
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceStateInvalidation final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceStateInvalidation(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    void recordLegacyWrite(VkCommandBuffer cmdBuffer, uint32_t offset, uint32_t value);
    void recordHeapWrite(VkCommandBuffer cmdBuffer, uint32_t offset, uint32_t value);

    TestParams m_params{};

    std::vector<uint32_t> m_expectedOutput;

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;

    VkBindHeapInfoEXT m_bindHeapInfo;

    Move<VkPipeline> m_legacyPipeline;
    Move<VkPipeline> m_heapPipeline;
};

class DescriptorHeapTestCaseStateInvalidation final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseStateInvalidation(tcu::TestContext &testCtx, const std::string &name,
                                                     const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceStateInvalidation(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseStateInvalidation::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0, std430) buffer OutputBuffer {
    uint outputBuffer[];
};
layout(push_constant) uniform InputData {
    uint offset;
    uint value;
};
void main() {
    outputBuffer[offset] = value;
}
)";

    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceStateInvalidation::iterate()
{
    const auto &vk = m_device.getDriver();

    const uint32_t bufferSize         = 256;
    const uint32_t descriptorSetCount = 16;

    // Create buffers for old descriptor sets and new descriptor heaps
    auto outputBuffer =
        createBufferAndMemory(bufferSize * sizeof(uint32_t),
                              VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    // Initialize buffers
    deMemset(outputBuffer->memory->getHostPtr(), 0, bufferSize * sizeof(uint32_t));
    m_expectedOutput.resize(bufferSize, 0);

    // Create descriptor set layout and pipeline layout for old descriptor sets
    VkDescriptorSetLayoutBinding setLayoutBinding =
        makeDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initVulkanStructure();
    setLayoutCreateInfo.bindingCount                    = 1;
    setLayoutCreateInfo.pBindings                       = &setLayoutBinding;
    m_descriptorSetLayout = createDescriptorSetLayout(vk, *m_device, &setLayoutCreateInfo);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(std::array<uint32_t, 2>);

    VkPipelineLayoutCreateInfo pipelineLayout = initVulkanStructure();
    pipelineLayout.setLayoutCount             = 1;
    pipelineLayout.pSetLayouts                = &m_descriptorSetLayout.get();
    pipelineLayout.pushConstantRangeCount     = 1;
    pipelineLayout.pPushConstantRanges        = &pushConstantRange;
    m_pipelineLayout                          = createPipelineLayout(vk, *m_device, &pipelineLayout);

    // Allocate and write old descriptor set
    VkDescriptorPoolSize poolSize       = makeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorSetCount);
    VkDescriptorPoolCreateInfo poolInfo = initVulkanStructure();
    poolInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets                    = descriptorSetCount;
    poolInfo.poolSizeCount              = 1;
    poolInfo.pPoolSizes                 = &poolSize;
    m_descriptorPool                    = createDescriptorPool(vk, *m_device, &poolInfo);

    VkDescriptorSetAllocateInfo allocInfo = initVulkanStructure();
    allocInfo.descriptorPool              = *m_descriptorPool;
    allocInfo.descriptorSetCount          = 1;
    allocInfo.pSetLayouts                 = &m_descriptorSetLayout.get();

    m_descriptorSet = allocateDescriptorSet(vk, *m_device, &allocInfo);

    VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(*outputBuffer->buffer, 0, VK_WHOLE_SIZE);
    VkWriteDescriptorSet write        = initVulkanStructure();
    write.dstSet                      = *m_descriptorSet;
    write.dstBinding                  = 0;
    write.descriptorCount             = 1;
    write.descriptorType              = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo                 = &bufferInfo;
    vk.updateDescriptorSets(*m_device, 1, &write, 0, nullptr);

    // Create descriptor heap
    int64_t descriptorHeapSize     = m_descriptorHeapProperties.bufferDescriptorSize;
    descriptorHeapSize             = deAlign64(descriptorHeapSize, m_descriptorHeapProperties.resourceHeapAlignment);
    int64_t userDescriptorHeapSize = descriptorHeapSize;
    descriptorHeapSize += m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto descriptorHeap = createBufferAndMemory(descriptorHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    VkHostAddressRangeEXT descriptorHeapRange{};
    descriptorHeapRange.address = descriptorHeap->memory->getHostPtr();
    descriptorHeapRange.size    = static_cast<size_t>(descriptorHeapSize);

    VkDeviceAddressRangeEXT outputBufferAddrRange{};
    outputBufferAddrRange.address = outputBuffer->address;
    outputBufferAddrRange.size    = bufferSize * sizeof(uint32_t);

    VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
    resourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resourceInfo.data.pAddressRange          = &outputBufferAddrRange;

    m_bindHeapInfo                     = initVulkanStructure();
    m_bindHeapInfo.heapRange.address   = descriptorHeap->address;
    m_bindHeapInfo.heapRange.size      = descriptorHeapSize;
    m_bindHeapInfo.reservedRangeOffset = userDescriptorHeapSize;
    m_bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VK_CHECK(vk.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &descriptorHeapRange));

    // Setup descriptor mappings
    VkDescriptorSetAndBindingMappingEXT descriptorHeapMapping = initVulkanStructure();
    descriptorHeapMapping.descriptorSet                       = 0;
    descriptorHeapMapping.firstBinding                        = 0;
    descriptorHeapMapping.bindingCount                        = 1;
    descriptorHeapMapping.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    descriptorHeapMapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &descriptorHeapMapping;

    // Create legacy compute pipeline
    auto computeShaderModule = createShaderModule(vk, *m_device, getShaderBinary("compute"));

    VkComputePipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.stage                       = initVulkanStructure();
    pipelineInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module                = *computeShaderModule;
    pipelineInfo.stage.pName                 = "main";
    pipelineInfo.layout                      = *m_pipelineLayout;
    m_legacyPipeline                         = createComputePipeline(vk, *m_device, VK_NULL_HANDLE, &pipelineInfo);

    // Create heap compute pipeline
    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    pipelineInfo.pNext       = &pipelineCreateFlags2CreateInfo;
    pipelineInfo.stage.pNext = &mappingInfo;
    pipelineInfo.layout      = VK_NULL_HANDLE;

    m_heapPipeline = createComputePipeline(vk, *m_device, VK_NULL_HANDLE, &pipelineInfo);

    // Record command buffer
    auto cmdPool   = makeCommandPool(vk, *m_device, m_queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    de::Random rng(m_params.seed);

    beginCommandBuffer(vk, *cmdBuffer);

    // A -> B
    recordLegacyWrite(*cmdBuffer, 0, rng.getUint32());
    recordHeapWrite(*cmdBuffer, 1, rng.getUint32());

    // B -> A
    recordHeapWrite(*cmdBuffer, 2, rng.getUint32());
    recordLegacyWrite(*cmdBuffer, 3, rng.getUint32());

    // A -> B -> A
    recordLegacyWrite(*cmdBuffer, 4, rng.getUint32());
    recordHeapWrite(*cmdBuffer, 5, rng.getUint32());
    recordLegacyWrite(*cmdBuffer, 6, rng.getUint32());

    // Balance
    recordHeapWrite(*cmdBuffer, 7, rng.getUint32());
    recordHeapWrite(*cmdBuffer, 8, rng.getUint32());

    // B -> A -> B
    recordHeapWrite(*cmdBuffer, 9, rng.getUint32());
    recordLegacyWrite(*cmdBuffer, 10, rng.getUint32());
    recordHeapWrite(*cmdBuffer, 11, rng.getUint32());

    endCommandBuffer(vk, *cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer.get();
    VK_CHECK(vk.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vk.deviceWaitIdle(*m_device));

    // Verify results
    std::vector<uint32_t> outputData(bufferSize);
    deMemcpy(outputData.data(), outputBuffer->memory->getHostPtr(), bufferSize * sizeof(uint32_t));
    if (outputData != m_expectedOutput)
    {
        return tcu::TestStatus::fail("Output data does not match expected data");
    }

    return tcu::TestStatus::pass("Pass");
}

void DescriptorHeapTestInstanceStateInvalidation::recordLegacyWrite(VkCommandBuffer cmdBuffer, uint32_t offset,
                                                                    uint32_t value)
{
    const auto &vk                         = m_device.getDriver();
    const std::array<uint32_t, 2> pushData = {offset, value};

    vk.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushData), &pushData);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(),
                             0, nullptr);
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_legacyPipeline);
    vk.cmdDispatch(cmdBuffer, 1, 1, 1);

    m_expectedOutput[offset] = value;
}

void DescriptorHeapTestInstanceStateInvalidation::recordHeapWrite(VkCommandBuffer cmdBuffer, uint32_t offset,
                                                                  uint32_t value)
{
    const auto &vk                         = m_device.getDriver();
    const std::array<uint32_t, 2> pushData = {offset, value};

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.address      = &pushData;
    pushDataInfo.data.size         = sizeof(pushData);

    vk.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vk.cmdBindResourceHeapEXT(cmdBuffer, &m_bindHeapInfo);
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_heapPipeline);
    vk.cmdDispatch(cmdBuffer, 1, 1, 1);

    m_expectedOutput[offset] = value;
}

class DescriptorHeapTestInstanceWriteAfterRecord final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceWriteAfterRecord(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params{};
};

class DescriptorHeapTestCaseWriteAfterRecord final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseWriteAfterRecord(tcu::TestContext &testCtx, const std::string &name,
                                                    const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceWriteAfterRecord(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseWriteAfterRecord::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0, r32ui) uniform uimageBuffer outputBuffer;
layout(push_constant) uniform PushConstant {
  uint value;
};

void main()
{
    imageStore(outputBuffer, 0, uvec4(value, 0, 0, 0));
}
)";

    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceWriteAfterRecord::iterate()
{
    const auto &vkd = m_device.getDriver();

    // Create a uint32_t-sized Vulkan buffer for the output
    const VkDeviceSize bufferSize = sizeof(uint32_t);
    auto outputBuffer             = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT |
                                                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    // Create a resource heap that can hold 1 image descriptor
    int64_t resourceHeapSizeI64 = m_descriptorHeapProperties.imageDescriptorSize;
    resourceHeapSizeI64         = deAlign64(resourceHeapSizeI64, m_descriptorHeapProperties.resourceHeapAlignment);
    resourceHeapSizeI64 += m_descriptorHeapProperties.minResourceHeapReservedRange;
    const VkDeviceSize resourceHeapSize = static_cast<VkDeviceSize>(resourceHeapSizeI64);

    auto resourceHeap = createBufferAndMemory(resourceHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);

    // Define the descriptor heap mapping structures
    VkDescriptorSetAndBindingMappingEXT mapping       = initVulkanStructure();
    mapping.descriptorSet                             = 0;
    mapping.firstBinding                              = 0;
    mapping.bindingCount                              = 1;
    mapping.resourceMask                              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mapping.source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mapping.sourceData.constantOffset.heapOffset      = 0;
    mapping.sourceData.constantOffset.heapArrayStride = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &mapping;

    // Compile the descriptor heap pipeline
    const auto computeShaderModule = createShaderModule(vkd, *m_device, m_context.getBinaryCollection().get("compute"));
    VkPipelineCreateFlags2CreateInfoKHR pipelineFlagsInfo = initVulkanStructure();
    pipelineFlagsInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineFlagsInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = computeShaderModule.get();
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Record a command buffer using the pipeline and do a 1,1,1 compute dispatch
    auto commandPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto commandBufferPtr = allocateCommandBuffer(vkd, *m_device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto commandBuffer    = commandBufferPtr.get();

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeap->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = 0;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkHostAddressRangeEXT bufferDescriptor{};
    bufferDescriptor.address = resourceHeap->memory->getHostPtr();
    bufferDescriptor.size    = static_cast<size_t>(resourceHeapSize);

    de::Random rnd(m_params.seed);
    const uint32_t expectedValue = rnd.getUint32();

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.size         = sizeof(expectedValue);
    pushDataInfo.data.address      = &expectedValue;

    beginCommandBuffer(vkd, commandBuffer);
    vkd.cmdBindResourceHeapEXT(commandBuffer, &resourceHeapBindInfo);
    vkd.cmdPushDataEXT(commandBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
    vkd.cmdDispatch(commandBuffer, 1, 1, 1);
    endCommandBuffer(vkd, commandBuffer);

    // Write the descriptor heaps
    VkTexelBufferDescriptorInfoEXT texelBufferDescriptorInfo = initVulkanStructure();
    texelBufferDescriptorInfo.addressRange.address           = outputBuffer->address;
    texelBufferDescriptorInfo.addressRange.size              = bufferSize;
    texelBufferDescriptorInfo.format                         = VK_FORMAT_R32_UINT;

    VkResourceDescriptorInfoEXT bufferDescriptorInfo = initVulkanStructure();
    bufferDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    bufferDescriptorInfo.data.pTexelBuffer           = &texelBufferDescriptorInfo;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &bufferDescriptorInfo, &bufferDescriptor));

    // Submit the command buffer
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues.back(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    // Check the results
    uint32_t result = 0;
    deMemcpy(&result, outputBuffer->memory->getHostPtr(), sizeof(result));
    if (result != expectedValue)
    {
        throw tcu::TestError("Unexpected result in output buffer");
    }

    return tcu::TestStatus::pass("Pass");
}

DescriptorHeapTestInstanceBase::DescriptorHeapTestInstanceBase(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_instance(context)
{
    auto &inst      = m_instance.getDriver();
    auto physDevice = m_instance.getPhysicalDevice();
    auto queueProps = getPhysicalDeviceQueueFamilyProperties(inst, physDevice);

    m_queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    uint32_t graphicsComputeQueue = VK_QUEUE_FAMILY_IGNORED;

    for (uint32_t i = 0; i < queueProps.size(); ++i)
    {
        // Not enough queues for the test.
        if (queueProps[i].queueCount < params.queueCount)
        {
            continue;
        }

        // Only expose queues that support sparse binding if requested.
        if (params.enableSparseHeap && (queueProps[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == 0)
        {
            continue;
        }
        // Only expose protected memory capable queues if requested.
        if (params.enableProtectedHeap && (queueProps[i].queueFlags & VK_QUEUE_PROTECTED_BIT) == 0)
        {
            continue;
        }

        if (static_cast<int>(params.queue) == VK_QUEUE_GRAPHICS_BIT)
        {
            if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                m_queueFamilyIndex = i;

                break;
            }
        }
        else if (static_cast<int>(params.queue) == VK_QUEUE_COMPUTE_BIT)
        {
            if (((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
                ((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0))
            {
                m_queueFamilyIndex = i;
            }
            else if (((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) &&
                     ((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0))
            {
                graphicsComputeQueue = i;
            }
        }
    }

    // If a compute only queue could not be found, fall back to a graphics & compute one.
    if (static_cast<int>(params.queue) == VK_QUEUE_COMPUTE_BIT && m_queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
        m_queueFamilyIndex = graphicsComputeQueue;
    }

    if (m_queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
        TCU_THROW(NotSupportedError, "Queue not supported");
    }

    const std::vector<float> priority(params.queueCount, 0.5f);

    VkDeviceQueueCreateInfo queueInfo = initVulkanStructure();
    queueInfo.queueFamilyIndex        = m_queueFamilyIndex;
    queueInfo.queueCount              = params.queueCount;
    queueInfo.pQueuePriorities        = priority.data();

    VkPhysicalDeviceFeatures2 features2   = initVulkanStructure();
    features2.features.robustBufferAccess = VK_TRUE;

    VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures = initVulkanStructure();
    descriptorHeapFeatures.descriptorHeap                            = VK_TRUE;
    descriptorHeapFeatures.descriptorHeapCaptureReplay               = params.enableCaptureReplay;

    VkPhysicalDeviceShaderUntypedPointersFeaturesKHR shaderUntypedPointersFeatures = initVulkanStructure();
    shaderUntypedPointersFeatures.shaderUntypedPointers                            = VK_TRUE;

    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features = initVulkanStructure();
    maintenance5Features.maintenance5                            = VK_TRUE;

    VkPhysicalDeviceMaintenance4FeaturesKHR maintenance4Features = initVulkanStructure();
    maintenance4Features.maintenance4                            = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = initVulkanStructure();
    accelerationStructureFeatures.accelerationStructure                            = VK_TRUE;
    accelerationStructureFeatures.accelerationStructureCaptureReplay = params.enableAccelerationStructuresCaptureReplay;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = initVulkanStructure();
    rayQueryFeatures.rayQuery                            = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = initVulkanStructure();
    rayTracingPipelineFeatures.rayTracingPipeline                            = VK_TRUE;

    VkPhysicalDeviceSamplerYcbcrConversionFeatures samplerYcbcrConversionFeatures = initVulkanStructure();
    samplerYcbcrConversionFeatures.samplerYcbcrConversion                         = VK_TRUE;

    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures = initVulkanStructure();
    graphicsPipelineLibraryFeatures.graphicsPipelineLibrary                            = VK_TRUE;

    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = initVulkanStructure();
    shaderObjectFeatures.shaderObject                            = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = initVulkanStructure();
    dynamicRenderingFeatures.dynamicRendering                         = VK_TRUE;

    VkPhysicalDeviceCommandBufferInheritanceFeaturesNV commandBufferInheritanceFeaturesNV = initVulkanStructure();
    commandBufferInheritanceFeaturesNV.commandBufferInheritance                           = VK_TRUE;

    VkPhysicalDeviceCustomBorderColorFeaturesEXT customBorderColorEXT = initVulkanStructure();
    customBorderColorEXT.customBorderColors                           = VK_TRUE;
    customBorderColorEXT.customBorderColorWithoutFormat               = VK_FALSE;

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2EXT = initVulkanStructure();
    robustness2EXT.nullDescriptor                         = VK_TRUE;

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features = initVulkanStructure();
    synchronization2Features.synchronization2                            = VK_TRUE;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderEXT = initVulkanStructure();
    meshShaderEXT.meshShader                            = VK_TRUE;

    VkPhysicalDeviceVariablePointerFeaturesKHR variablePointers = initVulkanStructure();
    variablePointers.variablePointersStorageBuffer              = VK_TRUE;
    variablePointers.variablePointers                           = VK_TRUE;

    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT shaderImageAtomicInt64 = initVulkanStructure();
    shaderImageAtomicInt64.shaderImageInt64Atomics                           = VK_TRUE;

    VkPhysicalDeviceProtectedMemoryFeatures protectedMemory = initVulkanStructure();
    protectedMemory.protectedMemory                         = VK_TRUE;

    VkPhysicalDeviceShader64BitIndexingFeaturesEXT shader64BitIndexing = initVulkanStructure();
    shader64BitIndexing.shader64BitIndexing                            = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12                = initVulkanStructure();
    features12.shaderSampledImageArrayNonUniformIndexing       = params.enableSampledImageArrayNonUniformIndexing;
    features12.shaderStorageImageArrayNonUniformIndexing       = params.enableStorageImageArrayNonUniformIndexing;
    features12.shaderUniformTexelBufferArrayNonUniformIndexing = params.enableUniformTexelBufferArrayNonUniformIndexing;
    features12.shaderStorageTexelBufferArrayNonUniformIndexing = params.enableStorageTexelBufferArrayNonUniformIndexing;
    features12.shaderUniformBufferArrayNonUniformIndexing      = params.enableUniformBufferArrayNonUniformIndexing;
    features12.shaderStorageBufferArrayNonUniformIndexing      = params.enableStorageBufferArrayNonUniformIndexing;
    features12.bufferDeviceAddress                             = VK_TRUE;
    features12.bufferDeviceAddressCaptureReplay                = params.enableCaptureReplay;

    void **nextPtr = &features2.pNext;
    addToChainVulkanStructure(&nextPtr, descriptorHeapFeatures);
    addToChainVulkanStructure(&nextPtr, shaderUntypedPointersFeatures);
    addToChainVulkanStructure(&nextPtr, maintenance5Features);
    addToChainVulkanStructure(&nextPtr, synchronization2Features);
    addToChainVulkanStructure(&nextPtr, features12);

    std::vector<const char *> extensions = {
        VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,   VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_5_EXTENSION_NAME,     VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    };

    if (params.enableRayQuery)
    {
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, rayQueryFeatures);
    }
    if (params.enableAccelerationStructures)
    {
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, accelerationStructureFeatures);
    }
    if (params.enableRayTracing)
    {
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, rayTracingPipelineFeatures);
    }
    if (params.enableSamplerYcbcrConversion)
    {
        extensions.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, samplerYcbcrConversionFeatures);
    }
    if (params.enableGraphicsPipelineLibrary)
    {
        extensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, graphicsPipelineLibraryFeatures);
    }
    if (params.enableShaderObject)
    {
        extensions.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, shaderObjectFeatures);
    }
    if (params.enableDynamicRendering)
    {
        extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, dynamicRenderingFeatures);
    }
    if (params.enableVertexPipelineStoresAndAtomics)
    {
        features2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
    }
    if (params.enableFragmentStoresAndAtomics)
    {
        features2.features.fragmentStoresAndAtomics = VK_TRUE;
    }
    if (params.enableNVCommandBufferInheritance)
    {
        extensions.push_back(VK_NV_COMMAND_BUFFER_INHERITANCE_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, commandBufferInheritanceFeaturesNV);
    }
    if (params.enableCustomBorderColor)
    {
        extensions.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, customBorderColorEXT);
    }
    if (params.enableNullDescriptor)
    {
        extensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, robustness2EXT);
    }
    if (params.enableTessellationShader)
    {
        features2.features.tessellationShader = VK_TRUE;
    }
    if (params.enableGeometryShader)
    {
        features2.features.geometryShader = VK_TRUE;
    }
    if (params.enableMeshShader)
    {
        extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, meshShaderEXT);

        if (params.enableTaskShader)
        {
            DE_ASSERT(params.enableMeshShader);
            meshShaderEXT.taskShader = VK_TRUE;
        }
    }
    if (params.enableMaintenance4)
    {
        extensions.push_back(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, maintenance4Features);
    }
    if (params.enableRuntimeDescriptorArray)
    {
        features12.runtimeDescriptorArray = VK_TRUE;
    }
    if (params.enablePushDescriptors)
    {
        extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    }
    if (params.enableSampleRateShading)
    {
        features2.features.sampleRateShading = VK_TRUE;
    }
    if (params.enableVariablePointers)
    {
        extensions.push_back(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, variablePointers);
    }
    if (params.shaderImageInt64Atomics)
    {
        features2.features.shaderInt64 = VK_TRUE;
        extensions.push_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
        addToChainVulkanStructure(&nextPtr, shaderImageAtomicInt64);
    }
    if (params.enableSparseHeap)
    {
        features2.features.sparseBinding = VK_TRUE;
    }
    if (params.enableProtectedHeap)
    {
        addToChainVulkanStructure(&nextPtr, protectedMemory);
    }
    if (params.enableShader64bitIndexing)
    {
        extensions.push_back(VK_EXT_SHADER_64BIT_INDEXING_EXTENSION_NAME);
        features2.features.shaderInt64 = VK_TRUE;
        addToChainVulkanStructure(&nextPtr, shader64BitIndexing);
    }
    if (params.enableShaderUniformTexelBufferArrayDynamicIndexing)
    {
        features12.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
    }
    if (params.enableShaderStorageTexelBufferArrayDynamicIndexing)
    {
        features12.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
    }

    VkDeviceCreateInfo createInfo      = initVulkanStructure(&features2);
    createInfo.pEnabledFeatures        = nullptr;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = &queueInfo;

    m_device = m_instance.createCustomDevice(physDevice, &createInfo);

    m_descriptorHeapProperties =
        *findStructure<VkPhysicalDeviceDescriptorHeapPropertiesEXT>(&context.getDeviceProperties2());

    for (uint32_t index = 0; index < params.queueCount; ++index)
    {
        VkQueue queue = VK_NULL_HANDLE;
        m_device.getDriver().getDeviceQueue(*m_device, m_queueFamilyIndex, index, &queue);
        m_queues.push_back(queue);
    }

    m_physDevice = physDevice;
}

void DescriptorHeapTestCaseBasic::initPrograms(vk::SourceCollections &programCollection) const
{
    for (uint32_t queueIndex = 0; queueIndex < m_params.queueCount; ++queueIndex)
    {
        initQueuePrograms(programCollection, queueIndex);
    }
}

void DescriptorHeapTestCaseBasic::initQueuePrograms(vk::SourceCollections &programCollection, uint32_t queueIndex) const
{
    std::ostringstream str;
    str << "#version 460\n"
           "#extension GL_EXT_samplerless_texture_functions : require\n"
           "#extension GL_EXT_nonuniform_qualifier : require\n";

    if (m_params.enableRayQuery)
    {
        str << "#extension GL_EXT_ray_query : require\n";
    }

    if (m_params.enableRayTracing)
    {
        str << "#extension GL_EXT_ray_tracing : require\n";
    }

    if (m_params.stage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        str << "layout(local_size_x = " << m_params.dimension << ") in;\n";
    }
    else
    {
        DE_ASSERT(m_params.dimension == 1 || m_params.stage != VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    if (m_params.enableRayQuery)
    {
        // Code taken from vktBindingDescriptorBufferTests.cpp
        str << "int queryAS(accelerationStructureEXT rayQueryTopLevelAccelerationStructure) {\n"
               "    const uint  rayFlags = gl_RayFlagsNoOpaqueEXT;\n"
               "    const uint  cullMask = 0xFF;\n"
               "    const float tmin     = 0.0f;\n"
               "    const float tmax     = 524288.0f; // 2^^19\n"
               "    const vec3  origin   = vec3(0.0f, 0.0f, 0.0f);\n"
               "    const vec3  direct   = vec3(0.0f, 0.0f, 1.0f);\n"
               "    rayQueryEXT rayQuery;\n"
               "\n"
               "    rayQueryInitializeEXT(rayQuery, rayQueryTopLevelAccelerationStructure, rayFlags, cullMask, origin, "
               "tmin, direct, tmax);\n"
               "\n"
               "    if (rayQueryProceedEXT(rayQuery)) {\n"
               "        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == "
               "gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
               "            return int(uint(round(rayQueryGetIntersectionTEXT(rayQuery, false))));\n"
               "        }\n"
               "        return 0;\n"
               "    }\n"
               "    return 0;\n"
               "}\n";
    }

    int uid                = 0;
    bool hasArrayedBinding = false;

    for (const auto &binding : m_params.bindings)
    {
        if (binding.queue != static_cast<int>(queueIndex))
        {
            continue;
        }
        if (binding.arrayed)
        {
            hasArrayedBinding = true;
        }

        str << "layout(set = " << binding.descriptorSet << ", binding = " << binding.firstBinding;
        if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
            (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
        {
            str << ", r32i";
        }
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
            str << ", input_attachment_index = 0";
        }
        str << ") ";

        switch (binding.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            str << "uniform sampler desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            str << "uniform isampler1D desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            str << "uniform itexture1D desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            str << "uniform iimage1D desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            str << "uniform itextureBuffer desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            str << "uniform iimageBuffer desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            str << "uniform UniformBuffer" << uid << " { int data; } desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            str << "buffer StorageBuffer" << uid << " { int data; } desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            str << "uniform isubpassInput desc" << uid;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            str << "uniform accelerationStructureEXT desc" << uid;
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        if (binding.arrayed)
        {
            str << '[' << m_params.dimension << ']';
        }
        str << ";\n";

        ++uid;
    }

    if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        str << "layout(location = 0) out int result;\n";
        if (hasArrayedBinding)
            str << "layout(set = 0, binding = 0) readonly buffer Swizzler { int swizzler[]; };\n";
    }
    else
    {
        str << "layout(set = 0, binding = 0) buffer Swizzler { int swizzler[]; };\n"
               "layout(set = 0, binding = 1) buffer Output { int result[]; };\n";
    }

    if (m_params.stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
    {
        str << "layout(location = 0) rayPayloadEXT int hitValue;\n";
    }

    str << "void main() {\n"
           "int temp = 0;\n";

    if (m_params.stage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        str << "int invocationId = int(gl_LocalInvocationID.x);\n";
    }
    if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT && hasArrayedBinding)
    {
        str << "int invocationId = int(gl_FragCoord.x);\n";
    }
    if (m_params.stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
    {
        str << "int invocationId = int(gl_LaunchIDEXT.x);\n"
               "hitValue = "
            << kRayTraceDefaultHitValue << ";\n";
    }

    uid = 0;

    for (const auto &binding : m_params.bindings)
    {
        if (binding.queue != static_cast<int>(queueIndex))
        {
            continue;
        }

        std::string indexing;
        if (binding.arrayed)
        {
            indexing = "[nonuniformEXT(swizzler[invocationId])]";
        }

        switch (binding.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            str << "temp ^= textureLod(isampler1D(desc" << binding.imageBindingUid;
            if (binding.imageBindingArrayIndex >= 0)
                str << "[nonuniformEXT(" << binding.imageBindingArrayIndex << ")]";
            str << ", desc" << uid << indexing << "), -1.0, 0.0).r";
            if (binding.shiftSamplerResult)
            {
                str << " << swizzler[invocationId]";
            }
            str << ";\n";
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            // Sample twice, once for the border color and another one for the texel color.
            str << "temp ^= textureLod(desc" << uid << indexing
                << ", -1.0, 0.0).r;\n"
                   "temp ^= textureLod(desc"
                << uid << indexing << ", 0.0, 0.0).r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            str << "temp ^= texelFetch(desc" << uid << indexing << ", 0, 0).r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            str << "temp ^= imageLoad(desc" << uid << indexing << ", 0).r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            str << "temp ^= texelFetch(desc" << uid << indexing << ", 0).r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            str << "temp ^= imageLoad(desc" << uid << indexing << ", 0).r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            str << "temp ^= desc" << uid << indexing << ".data;\n";
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            str << "temp ^= subpassLoad(desc" << uid << ").r;\n";
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            if (m_params.stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            {
                str << "uint  rayFlags = 0;\n"
                       "uint  cullMask = 0xFF;\n"
                       "float tmin     = 0.0f;\n"
                       "float tmax     = 524288.0f; // 2^^19\n"
                       "vec3  origin   = vec3(0.0f, 0.0f, 0.0f);\n"
                       "vec3  direct   = vec3(0.0f, 0.0f, 1.0f);\n"
                       "traceRayEXT(desc"
                    << uid << indexing
                    << ", rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
                       "temp ^= hitValue;\n";
            }
            else
            {
                str << "temp ^= queryAS(desc" << uid << indexing << ");\n";
            }
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        ++uid;
    }

    if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        str << "result = temp;\n";
    }
    else
    {
        str << "result[invocationId] = temp;\n";
    }

    str << "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    std::string qiStr = std::to_string(queueIndex);
    switch (m_params.stage)
    {
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        programCollection.glslSources.add("vertex" + qiStr)
            << glu::VertexSource("#version 450\nvoid main() { gl_Position = vec4(vec3(0),1); }") << buildOptions;
        programCollection.glslSources.add("fragment" + qiStr) << glu::FragmentSource(str.str()) << buildOptions;
        break;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        programCollection.glslSources.add("compute" + qiStr) << glu::ComputeSource(str.str()) << buildOptions;
        break;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
    {
        const std::string missPassthrough = "#version 460\n"
                                            "#extension GL_EXT_ray_tracing : require\n"
                                            "layout(location = 0) rayPayloadInEXT int hitValue;\n"
                                            "void main() {}\n";
        const std::string hitPassthrough  = "#version 460\n"
                                            "#extension GL_EXT_ray_tracing : require\n"
                                            "layout(location = 0) rayPayloadInEXT int hitValue;\n"
                                            "void main() {\n"
                                            "hitValue = int(uint(gl_HitTEXT));\n"
                                            "}\n";

        programCollection.glslSources.add("raygen" + qiStr) << glu::RaygenSource(str.str()) << buildOptions;

        programCollection.glslSources.add("miss" + qiStr) << glu::MissSource(missPassthrough) << buildOptions;
        programCollection.glslSources.add("closesthit" + qiStr)
            << glu::ClosestHitSource(hitPassthrough) << buildOptions;
        programCollection.glslSources.add("anyhit" + qiStr) << glu::AnyHitSource(hitPassthrough) << buildOptions;
        break;
    }
    default:
        DE_ASSERT(0);
    }
}

VkDeviceAddress getAccelerationStructureDeviceAddress(const DeviceInterface &deviceDriver, VkDevice device,
                                                      VkAccelerationStructureKHR accelerationStructure)
{
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = initVulkanStructure();
    addressInfo.accelerationStructure                       = accelerationStructure;

    const VkDeviceAddress deviceAddress = deviceDriver.getAccelerationStructureDeviceAddressKHR(device, &addressInfo);

    DE_ASSERT(deviceAddress != 0);

    return deviceAddress;
}

VkSamplerCreateInfo makeBorderCodedSamplerCreateInfo(bool useBlack)
{
    VkSamplerCreateInfo samplerCreateInfo = initVulkanStructure();
    samplerCreateInfo.flags               = 0;
    samplerCreateInfo.magFilter           = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter           = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.mipLodBias          = 0.0f;
    samplerCreateInfo.anisotropyEnable    = VK_FALSE;
    samplerCreateInfo.compareEnable       = VK_FALSE;
    samplerCreateInfo.compareOp           = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod              = 0.0f;
    samplerCreateInfo.maxLod              = 1000.0f;
    samplerCreateInfo.borderColor = useBlack ? VK_BORDER_COLOR_INT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    return samplerCreateInfo;
}

VkSamplerCreateInfo makeDefaultSamplerCreateInfo()
{
    return makeBorderCodedSamplerCreateInfo(false);
}

tcu::TestStatus DescriptorHeapTestInstanceBasic::iterate()
{
    const auto &vki = m_instance.getDriver();
    const auto &vk  = m_device.getDriver();

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(vki, m_physDevice);

    uint32_t resourceStride = static_cast<uint32_t>(getResourceDescriptorStride(m_descriptorHeapProperties));
    uint32_t samplerStride  = static_cast<uint32_t>(getSamplerDescriptorStride(m_descriptorHeapProperties));

    if (m_params.overrideResourceStride >= 0)
    {
        resourceStride = m_params.overrideResourceStride;
    }
    if (m_params.overrideSamplerStride >= 0)
    {
        samplerStride = m_params.overrideSamplerStride;
    }

    const VkDeviceSize numResourceDescriptors          = kMaxDescriptor;
    const VkDeviceSize userResourceDescriptorsHeapSize = numResourceDescriptors * resourceStride;
    const VkDeviceSize resourceDescriptorHeapSize =
        userResourceDescriptorsHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    const VkDeviceSize numSamplerDescriptors          = kMaxDescriptor;
    const VkDeviceSize userSamplerDescriptorsHeapSize = numSamplerDescriptors * samplerStride;
    const VkDeviceSize samplerDescriptorHeapReservedSize =
        m_params.embeddedSamplers ? m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded :
                                    m_descriptorHeapProperties.minSamplerHeapReservedRange;
    const VkDeviceSize samplerDescriptorHeapSize = userSamplerDescriptorsHeapSize + samplerDescriptorHeapReservedSize;

    const VkDeviceSize outputBufferSize   = alignUp(static_cast<VkDeviceSize>(m_params.dimension * sizeof(uint32_t)),
                                                    physDevProps.limits.minStorageBufferOffsetAlignment);
    const VkDeviceSize swizzlerBufferSize = alignUp(static_cast<VkDeviceSize>(m_params.dimension * sizeof(uint32_t)),
                                                    physDevProps.limits.minStorageBufferOffsetAlignment);

    auto swizzlerBuffer = createBufferAndMemory(
        swizzlerBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    m_deferredIndirectAddressBuffer.resize(128 * sizeof(uint64_t));
    m_indirectAddressBuffer =
        createBufferAndMemory(m_deferredIndirectAddressBuffer.size(), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    de::Random rnd(m_params.seed);

    // Randomize swizzler
    std::vector<uint32_t> swizzlerData(m_params.dimension);
    std::iota(swizzlerData.begin(), swizzlerData.end(), 0);
    for (int index = m_params.dimension; index-- > 0;)
    {
        const int randomIndex = rnd.getInt(0, index);
        std::swap(swizzlerData[index], swizzlerData[randomIndex]);
    }
    deMemcpy(swizzlerBuffer->memory->getHostPtr(), swizzlerData.data(), m_params.dimension * sizeof(uint32_t));

    std::vector<std::unique_ptr<Buffer>> outputBuffers;
    std::vector<std::vector<int32_t>> expectedResults;

    Move<VkCommandPool> cmdPool = makeCommandPool(vk, *m_device, m_queueFamilyIndex);
    std::vector<Move<VkCommandBuffer>> cmdBuffers;

    auto createDescriptorHeap = [this, &vk, &vki](VkDeviceSize heapSize, uint32_t queueIndex)
    {
        std::unique_ptr<Buffer> descriptorHeap = std::make_unique<Buffer>();
        std::unique_ptr<Buffer> unprotectedBuffer;

        auto descriptorHeapUsage =
            VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

        if (m_params.enableSparseHeap || m_params.enableProtectedHeap)
            descriptorHeapUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VkBufferUsageFlags2CreateInfoKHR createInfoUsageFlags2 = initVulkanStructure();
        createInfoUsageFlags2.usage                            = descriptorHeapUsage;

        VkBufferCreateInfo createInfo = initVulkanStructure();
        createInfo.pNext              = &createInfoUsageFlags2;
        createInfo.size               = heapSize;
        createInfo.usage              = 0;
        createInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        if (m_params.enableSparseHeap)
            createInfo.flags |= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
        if (m_params.enableProtectedHeap)
            createInfo.flags |= VK_BUFFER_CREATE_PROTECTED_BIT;

        descriptorHeap->buffer = createBuffer(vk, *m_device, &createInfo);

        const auto bufferMemReqs = getBufferMemoryRequirements(vk, *m_device, *descriptorHeap->buffer);

        const auto memReqs    = m_params.enableSparseHeap ? MemoryRequirement::Any : MemoryRequirement::HostVisible;
        const auto compatMask = bufferMemReqs.memoryTypeBits & getCompatibleMemoryTypes(m_memoryProperties, memReqs);
        DE_ASSERT(compatMask != 0);
        static_cast<void>(compatMask);

        VkMemoryAllocateFlagsInfo allocFlagsInfo = initVulkanStructure();
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        void *allocatePNext = nullptr;
        if (!m_params.enableSparseHeap)
            allocatePNext = &allocFlagsInfo;

        descriptorHeap->memory =
            allocateExtended(vki, vk, m_physDevice, *m_device, bufferMemReqs, memReqs, allocatePNext);

        if (m_params.enableSparseHeap || m_params.enableProtectedHeap)
        {
            unprotectedBuffer = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT);
        }

        if (m_params.enableSparseHeap)
        {
            // Bind the sparse memory to the heap.
            VkSparseMemoryBind sparseBind{};
            sparseBind.resourceOffset = 0;
            sparseBind.size           = bufferMemReqs.size;
            sparseBind.memory         = descriptorHeap->memory->getMemory();
            sparseBind.memoryOffset   = descriptorHeap->memory->getOffset();
            sparseBind.flags          = 0;

            VkSparseBufferMemoryBindInfo bufferBindInfo{};
            bufferBindInfo.bindCount = 1;
            bufferBindInfo.buffer    = *descriptorHeap->buffer;
            bufferBindInfo.pBinds    = &sparseBind;

            VkBindSparseInfo bindSparseInfo = initVulkanStructure();
            bindSparseInfo.bufferBindCount  = 1;
            bindSparseInfo.pBufferBinds     = &bufferBindInfo;

            VK_CHECK(vk.queueBindSparse(m_queues[queueIndex], 1, &bindSparseInfo, VK_NULL_HANDLE));
        }
        else
        {
            vk.bindBufferMemory(*m_device, *descriptorHeap->buffer, descriptorHeap->memory->getMemory(),
                                descriptorHeap->memory->getOffset());
        }

        VkBufferDeviceAddressInfo bdaInfo = initVulkanStructure();
        bdaInfo.buffer                    = *descriptorHeap->buffer;

        descriptorHeap->address = vk.getBufferDeviceAddress(*m_device, &bdaInfo);

        return std::make_tuple(std::move(descriptorHeap), std::move(unprotectedBuffer));
    };

    std::vector<std::unique_ptr<Buffer>> resourceHeapBuffers;
    std::vector<std::unique_ptr<Buffer>> samplerHeapBuffers;

    std::vector<std::unique_ptr<Buffer>> unprotectedResourceHeapBuffers;
    std::vector<std::unique_ptr<Buffer>> unprotectedSamplerHeapBuffers;

    std::vector<std::vector<char>> resourceHeapDataVectors;
    std::vector<std::vector<char>> samplerHeapDataVectors;

    for (uint32_t queueIndex = 0; queueIndex < m_params.queueCount; ++queueIndex)
    {
        auto [resourceHeapBuffer, unprotectedResourceHeap] =
            createDescriptorHeap(resourceDescriptorHeapSize, queueIndex);
        auto [samplerHeapBuffer, unprotectedSamplerHeap] = createDescriptorHeap(samplerDescriptorHeapSize, queueIndex);

        std::vector<char> &resourceHeapData = resourceHeapDataVectors.emplace_back(userResourceDescriptorsHeapSize);
        std::vector<char> &samplerHeapData  = samplerHeapDataVectors.emplace_back(userSamplerDescriptorsHeapSize);

        VkBindHeapInfoEXT resourceHeap   = initVulkanStructure();
        resourceHeap.heapRange.address   = resourceHeapBuffer->address;
        resourceHeap.heapRange.size      = resourceDescriptorHeapSize;
        resourceHeap.reservedRangeOffset = userResourceDescriptorsHeapSize;
        resourceHeap.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

        VkBindHeapInfoEXT samplerHeap   = initVulkanStructure();
        samplerHeap.heapRange.address   = samplerHeapBuffer->address;
        samplerHeap.heapRange.size      = samplerDescriptorHeapSize;
        samplerHeap.reservedRangeOffset = userSamplerDescriptorsHeapSize;
        samplerHeap.reservedRangeSize   = samplerDescriptorHeapReservedSize;

        // Write swizzler descriptor
        auto &swizzlerAddressRange = m_stagingDeviceAddressRanges.emplace_back(VkDeviceAddressRangeEXT{
            swizzlerBuffer->address,
            swizzlerBufferSize,
        });

        auto &swizzlerDescriptorHeapHostPtr   = m_deferredResourceHostDescriptors.emplace_back();
        swizzlerDescriptorHeapHostPtr.address = resourceHeapData.data() + 0 * resourceStride;
        swizzlerDescriptorHeapHostPtr.size    = resourceStride;

        auto &swizzlerDescriptorInfo              = m_deferredResourceDescriptors.emplace_back();
        swizzlerDescriptorInfo                    = initVulkanStructure();
        swizzlerDescriptorInfo.type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        swizzlerDescriptorInfo.data.pAddressRange = &swizzlerAddressRange;

        // Write output buffer descriptors.
        VkBufferUsageFlags2KHR outputBufferUsage =
            VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
        if (m_params.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            outputBufferUsage |= VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR;
        }
        Buffer &outputBuffer = *outputBuffers.emplace_back(createBufferAndMemory(outputBufferSize, outputBufferUsage));

        auto &outputDescriptor   = m_deferredResourceHostDescriptors.emplace_back();
        outputDescriptor.address = resourceHeapData.data() + (1 + queueIndex) * resourceStride;
        outputDescriptor.size    = resourceStride;

        auto &outputAddressRange                = m_stagingDeviceAddressRanges.emplace_back(VkDeviceAddressRangeEXT{
            outputBuffer.address,
            outputBufferSize,
        });
        auto &outputDescriptorInfo              = m_deferredResourceDescriptors.emplace_back();
        outputDescriptorInfo                    = initVulkanStructure();
        outputDescriptorInfo.type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputDescriptorInfo.data.pAddressRange = &outputAddressRange;

        std::vector<int32_t> &expectedResult = expectedResults.emplace_back(m_params.dimension);

        // Begin command buffer
        cmdBuffers.push_back(allocateCommandBuffer(vk, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const VkCommandBuffer cmdBuf = *cmdBuffers.back();

        beginCommandBuffer(vk, cmdBuf);

        vk.cmdBindResourceHeapEXT(cmdBuf, &resourceHeap);
        if (m_params.bindSamplerHeap)
        {
            vk.cmdBindSamplerHeapEXT(cmdBuf, &samplerHeap);
        }

        if (m_params.enableSparseHeap || m_params.enableProtectedHeap)
        {
            VkBufferCopy resourceBufferCopy{};
            resourceBufferCopy.dstOffset = 0;
            resourceBufferCopy.srcOffset = 0;
            resourceBufferCopy.size      = userResourceDescriptorsHeapSize;

            VkBufferCopy samplerBufferCopy{};
            samplerBufferCopy.dstOffset = 0;
            samplerBufferCopy.srcOffset = 0;
            samplerBufferCopy.size      = userSamplerDescriptorsHeapSize;

            vk.cmdCopyBuffer(cmdBuf, *unprotectedResourceHeap->buffer, *resourceHeapBuffer->buffer, 1,
                             &resourceBufferCopy);
            vk.cmdCopyBuffer(cmdBuf, *unprotectedSamplerHeap->buffer, *samplerHeapBuffer->buffer, 1,
                             &samplerBufferCopy);

            VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
            memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            memoryBarrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            memoryBarrier.dstAccessMask =
                VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT | VK_ACCESS_2_SAMPLER_HEAP_READ_BIT_EXT;

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &memoryBarrier;

            vk.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
        }

        for (const auto &push : m_params.pushData)
        {
            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = push.first;
            pushDataInfo.data.address      = &push.second;
            pushDataInfo.data.size         = sizeof(push.second);

            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
        }

        VkImage prePassImage         = VK_NULL_HANDLE;
        VkImageView prePassImageView = VK_NULL_HANDLE;
        if (m_params.inputAttachments)
        {
            std::tie(prePassImage, prePassImageView) = initPrePassRenderTarget();
        }

        std::vector<ShaderBinding> bindings;
        if (m_params.scaledMappingStrides)
        {
            bindings = mapShaderBindings(resourceStride, samplerStride, queueIndex);
        }
        else
        {
            bindings = mapShaderBindings(1U, 1U, queueIndex);
        }

        // Write embedded samplers.
        VkSamplerCreateInfo embeddedSamplerCreateInfo{};
        if (m_params.embeddedSamplers)
        {
            embeddedSamplerCreateInfo = makeDefaultSamplerCreateInfo();
            writeEmbeddedSamplers(bindings, &embeddedSamplerCreateInfo);
        }

        const uint32_t heapIndexStride = m_params.scaledMappingStrides ? 1U : resourceStride;

        // Write descriptors
        uint32_t const prePassClearColor = rnd.getUint32();
        for (const ShaderBinding &binding : bindings)
        {
            setupDescriptors(cmdBuf, binding, resourceStride, samplerStride, heapIndexStride, prePassImage,
                             swizzlerData, prePassClearColor, resourceHeapData.data(), samplerHeapData.data(), rnd,
                             expectedResult);
        }

        // Fill modified mappings
        std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
        for (const ShaderBinding &binding : bindings)
        {
            mappings.push_back(binding.mapping);
        }

        // Write output buffer and swizzler buffer mappings.
        VkDescriptorSetAndBindingMappingEXT &swizzlerMapping = mappings.emplace_back();
        swizzlerMapping                                      = initVulkanStructure();
        swizzlerMapping.descriptorSet                        = 0;
        swizzlerMapping.firstBinding                         = 0;
        swizzlerMapping.bindingCount                         = 1;
        swizzlerMapping.resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        swizzlerMapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        swizzlerMapping.sourceData.constantOffset.heapOffset      = 0 * resourceStride;
        swizzlerMapping.sourceData.constantOffset.heapArrayStride = resourceStride;

        VkDescriptorSetAndBindingMappingEXT &outputMapping = mappings.emplace_back();
        outputMapping                                      = initVulkanStructure();
        outputMapping.descriptorSet                        = 0;
        outputMapping.firstBinding                         = 1;
        outputMapping.bindingCount                         = 1;
        outputMapping.resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        outputMapping.source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        outputMapping.sourceData.constantOffset.heapOffset = (1 + queueIndex) * resourceStride;
        outputMapping.sourceData.constantOffset.heapArrayStride = resourceStride;

        VkRenderPass renderPass           = VK_NULL_HANDLE;
        VkImage renderTargetImage         = VK_NULL_HANDLE;
        VkImageView renderTargetImageView = VK_NULL_HANDLE;
        VkFramebuffer framebuffer         = VK_NULL_HANDLE;
        VkPipeline pipeline               = VK_NULL_HANDLE;

        switch (m_params.stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            renderPass                                         = initRenderPass();
            pipeline                                           = initGraphicsPipeline(mappings, renderPass, queueIndex);
            std::tie(renderTargetImage, renderTargetImageView) = initRenderTarget();
            framebuffer = initFramebuffer(renderPass, renderTargetImageView, prePassImageView);
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            pipeline = initComputePipeline(mappings, queueIndex);
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            pipeline = initRayTracingPipeline(mappings, queueIndex);
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        switch (m_params.stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
        {
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color.uint32[0] = 0xcccccccc;
            clearValues[1].color.uint32[0] = 0xcccccccc;

            if (m_params.inputAttachments)
            {
                clearValues[0].color.uint32[0] = prePassClearColor;
            }

            VkRenderPassBeginInfo renderPassBeginInfo = initVulkanStructure();
            renderPassBeginInfo.renderPass            = renderPass;
            renderPassBeginInfo.framebuffer           = framebuffer;
            renderPassBeginInfo.renderArea            = {{0, 0}, {1, 1}};
            renderPassBeginInfo.clearValueCount       = m_params.inputAttachments ? 2 : 1;
            renderPassBeginInfo.pClearValues          = clearValues.data();

            vk.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vk.cmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            if (m_params.inputAttachments)
            {
                vk.cmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_INLINE);
            }
            vk.cmdDraw(cmdBuf, 1, 1, 0, 0);
            vk.cmdEndRenderPass(cmdBuf);

            VkImageMemoryBarrier imageMemoryBarrier            = initVulkanStructure();
            imageMemoryBarrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            imageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image                           = renderTargetImage;
            imageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
            imageMemoryBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            vk.cmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

            VkBufferImageCopy region{};
            region.bufferOffset                    = 0;
            region.bufferRowLength                 = 0;
            region.bufferImageHeight               = 0;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;
            region.imageOffset                     = {0, 0, 0};
            region.imageExtent                     = {1, 1, 1};
            vk.cmdCopyImageToBuffer(cmdBuf, renderTargetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    outputBuffer.buffer.get(), 1, &region);
            break;
        }
        case VK_SHADER_STAGE_COMPUTE_BIT:
            vk.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vk.cmdDispatch(cmdBuf, 1, 1, 1);
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            vk.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
            cmdTraceRays(vk, cmdBuf, &m_raygenShaderBindingTableRegion, &m_missShaderBindingTableRegion,
                         &m_hitShaderBindingTableRegion, &m_callableShaderBindingTableRegion, m_params.dimension, 1, 1);
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        endCommandBuffer(vk, cmdBuf);

        resourceHeapBuffers.push_back(std::move(resourceHeapBuffer));
        samplerHeapBuffers.push_back(std::move(samplerHeapBuffer));

        if (unprotectedResourceHeap)
            unprotectedResourceHeapBuffers.push_back(std::move(unprotectedResourceHeap));
        if (unprotectedSamplerHeap)
            unprotectedSamplerHeapBuffers.push_back(std::move(unprotectedSamplerHeap));
    }

    // Execute deferred operations.
    DE_ASSERT(m_deferredSamplerDescriptors.size() == m_deferredSamplerHostDescriptors.size());
    if (!m_deferredSamplerDescriptors.empty())
    {
        VK_CHECK(vk.writeSamplerDescriptorsEXT(*m_device, de::sizeU32(m_deferredSamplerDescriptors),
                                               m_deferredSamplerDescriptors.data(),
                                               m_deferredSamplerHostDescriptors.data()));
    }

    DE_ASSERT(m_deferredResourceDescriptors.size() == m_deferredResourceHostDescriptors.size());
    if (!m_deferredResourceDescriptors.empty())
    {
        VK_CHECK(vk.writeResourceDescriptorsEXT(*m_device, de::sizeU32(m_deferredResourceDescriptors),
                                                m_deferredResourceDescriptors.data(),
                                                m_deferredResourceHostDescriptors.data()));
    }

    // Copy heap data from malloc memory to the GPU buffers.
    for (uint32_t queueIndex = 0; queueIndex < m_params.queueCount; ++queueIndex)
    {
        void *resourceDstAddress = nullptr;
        void *samplerDstAddress  = nullptr;

        if (m_params.enableSparseHeap || m_params.enableProtectedHeap)
        {
            resourceDstAddress = unprotectedResourceHeapBuffers[queueIndex]->memory->getHostPtr();
            samplerDstAddress  = unprotectedSamplerHeapBuffers[queueIndex]->memory->getHostPtr();
        }
        else
        {
            resourceDstAddress = resourceHeapBuffers[queueIndex]->memory->getHostPtr();
            samplerDstAddress  = samplerHeapBuffers[queueIndex]->memory->getHostPtr();
        }

        deMemcpy(resourceDstAddress, resourceHeapDataVectors[queueIndex].data(),
                 resourceHeapDataVectors[queueIndex].size());
        deMemcpy(samplerDstAddress, samplerHeapDataVectors[queueIndex].data(),
                 samplerHeapDataVectors[queueIndex].size());
    }

    // Ensure sparse binds have finished.
    if (m_params.enableSparseHeap)
    {
        VK_CHECK(vk.deviceWaitIdle(*m_device));
    }

    deMemcpy(m_indirectAddressBuffer->memory->getHostPtr(), m_deferredIndirectAddressBuffer.data(),
             m_deferredIndirectAddressBuffer.size());

    for (uint32_t index = 0; index < m_params.queueCount; ++index)
    {
        const VkQueue queue           = m_queues[index];
        VkSubmitInfo submitInfo       = initVulkanStructure();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmdBuffers[index].get();
        VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    }

    VK_CHECK(vk.deviceWaitIdle(*m_device));

    for (int32_t borderIndex : m_registeredBorderColors)
    {
        vk.unregisterCustomBorderColorEXT(*m_device, borderIndex);
    }

    for (uint32_t queueIndex = 0; queueIndex < m_params.queueCount; ++queueIndex)
    {
        std::vector<int32_t> result(m_params.dimension);
        deMemcpy(result.data(), outputBuffers[queueIndex]->memory->getHostPtr(), result.size() * sizeof(int32_t));

        for (size_t i = 0; i < result.size(); ++i)
        {
            if (result[i] != expectedResults[queueIndex][i])
            {
                std::stringstream stream;
                stream << "At index " << i << ", ";
                if (m_params.queueCount > 1)
                {
                    stream << "queue " << queueIndex << ", ";
                }
                stream << "expected 0x" << std::hex << expectedResults[queueIndex][i] << " but got 0x" << result[i];
                return tcu::TestStatus::fail(stream.str());
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

std::vector<ShaderBinding> DescriptorHeapTestInstanceBasic::mapShaderBindings(uint32_t resourceStride,
                                                                              uint32_t samplerStride,
                                                                              uint32_t queueIndex)
{
    // Copy input mappings so they can be modified.
    std::vector<ShaderBinding> bindings;

    // Patch mappings translating from index to offset and adding the explicit stride.
    for (ShaderBinding binding : m_params.bindings)
    {
        if (binding.queue != static_cast<int>(queueIndex))
        {
            continue;
        }

        const uint32_t stride = (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ? samplerStride : resourceStride;

        VkDescriptorMappingSourceDataEXT &sourceData = binding.mapping.sourceData;
        switch (binding.mapping.source)
        {
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
            sourceData.constantOffset.heapOffset *= stride;
            sourceData.constantOffset.heapArrayStride *= stride;
            sourceData.constantOffset.samplerHeapOffset *= samplerStride;
            sourceData.constantOffset.samplerHeapArrayStride *= samplerStride;
            break;
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
            sourceData.pushIndex.heapOffset *= stride;
            sourceData.pushIndex.heapIndexStride *= stride;
            sourceData.pushIndex.heapArrayStride *= stride;
            sourceData.pushIndex.samplerHeapOffset *= samplerStride;
            sourceData.pushIndex.samplerHeapIndexStride *= samplerStride;
            sourceData.pushIndex.samplerHeapArrayStride *= samplerStride;
            break;
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
            sourceData.indirectIndex.heapOffset *= stride;
            sourceData.indirectIndex.heapIndexStride *= stride;
            sourceData.indirectIndex.heapArrayStride *= stride;
            sourceData.indirectIndex.samplerHeapOffset *= samplerStride;
            sourceData.indirectIndex.samplerHeapIndexStride *= samplerStride;
            sourceData.indirectIndex.samplerHeapArrayStride *= samplerStride;
            break;
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
            sourceData.shaderRecordIndex.heapOffset *= stride;
            sourceData.shaderRecordIndex.heapIndexStride *= stride;
            sourceData.shaderRecordIndex.heapArrayStride *= stride;
            sourceData.shaderRecordIndex.samplerHeapOffset *= samplerStride;
            sourceData.shaderRecordIndex.samplerHeapIndexStride *= samplerStride;
            sourceData.shaderRecordIndex.samplerHeapArrayStride *= samplerStride;
            break;
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
            sourceData.indirectIndexArray.heapOffset *= stride;
            sourceData.indirectIndexArray.heapIndexStride *= stride;
            sourceData.indirectIndexArray.samplerHeapOffset *= samplerStride;
            sourceData.indirectIndexArray.samplerHeapIndexStride *= samplerStride;
            break;
        case VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT:
        case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT:
        case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT:
        case VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT:
        case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT:
        case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT:
            break;
        default:
            DE_ASSERT(0);
        }

        bindings.push_back(binding);
    }

    return bindings;
}

void DescriptorHeapTestInstanceBasic::writeEmbeddedSamplers(std::vector<ShaderBinding> &bindings,
                                                            const VkSamplerCreateInfo *samplerCreateInfo)
{
    for (ShaderBinding &binding : bindings)
    {
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            switch (binding.mapping.source)
            {
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
                binding.mapping.sourceData.constantOffset.pEmbeddedSampler = samplerCreateInfo;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
                binding.mapping.sourceData.pushIndex.pEmbeddedSampler = samplerCreateInfo;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
                binding.mapping.sourceData.indirectIndex.pEmbeddedSampler = samplerCreateInfo;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
                binding.mapping.sourceData.shaderRecordIndex.pEmbeddedSampler = samplerCreateInfo;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
                binding.mapping.sourceData.indirectIndexArray.pEmbeddedSampler = samplerCreateInfo;
                break;
            default:
                break;
            }
        }
    }
}

void DescriptorHeapTestInstanceBasic::setupDescriptors(VkCommandBuffer cmdBuf, const ShaderBinding &binding,
                                                       uint32_t resourceStride, uint32_t samplerStride,
                                                       uint32_t heapIndexStride, VkImage prePassImage,
                                                       const std::vector<uint32_t> &swizzlerData,
                                                       uint32_t prePassClearColor, char *resourceDescriptorHeapHostPtr,
                                                       char *samplerDescriptorHeapHostPtr, de::Random &rnd,
                                                       std::vector<int32_t> &expectedResult)
{
    const auto &vk                          = m_device.getDriver();
    const auto indirectAddressBufferHostPtr = m_deferredIndirectAddressBuffer.data();

    const int heapIndex = binding.heapIndex < 0 ? binding.firstBinding : binding.heapIndex;

    if (binding.descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        const VkDescriptorMappingSourceDataEXT &sourceData = binding.mapping.sourceData;

        const uint32_t stride = (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ? samplerStride : resourceStride;

        switch (binding.mapping.source)
        {
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
        {
            const uint32_t pushData = (binding.heapIndex - sourceData.pushIndex.heapOffset / stride) * heapIndexStride;
            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = sourceData.pushIndex.pushOffset;
            pushDataInfo.data.size         = sizeof(pushData);
            pushDataInfo.data.address      = &pushData;
            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
            break;
        }
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
        {
            const uint32_t indirectData =
                (binding.heapIndex - sourceData.indirectIndex.heapOffset / stride) * heapIndexStride;
            deMemcpy(indirectAddressBufferHostPtr + sourceData.indirectIndex.addressOffset, &indirectData,
                     sizeof(indirectData));

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = sourceData.indirectIndex.pushOffset;
            pushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
            pushDataInfo.data.address      = &m_indirectAddressBuffer->address;
            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
            break;
        }
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
        {
            const uint32_t shaderRecordData =
                (binding.heapIndex - sourceData.shaderRecordIndex.heapOffset / stride) * heapIndexStride;
            deMemcpy(m_shaderRecordData.data() + sourceData.shaderRecordIndex.shaderRecordOffset, &shaderRecordData,
                     sizeof(shaderRecordData));
            break;
        }
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
        {
            std::vector<uint32_t> indirect(m_params.dimension);
            std::iota(indirect.begin(), indirect.end(), 0);

            for (uint32_t &value : indirect)
            {
                value += binding.heapIndex - sourceData.indirectIndexArray.heapOffset / stride;
            }

            deMemcpy(indirectAddressBufferHostPtr + sourceData.indirectIndexArray.addressOffset, indirect.data(),
                     indirect.size() * sizeof(uint32_t));

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = sourceData.indirectIndexArray.pushOffset;
            pushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
            pushDataInfo.data.address      = &m_indirectAddressBuffer->address;
            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
            break;
        }
        default:
            break;
        }
    }
    else
    {
        if (!m_params.embeddedSamplers)
        {
            m_deferredSamplerDescriptors.emplace_back(makeDefaultSamplerCreateInfo());

            VkHostAddressRangeEXT &samplerDescriptor = m_deferredSamplerHostDescriptors.emplace_back();
            samplerDescriptor.address = samplerDescriptorHeapHostPtr + binding.samplerHeapIndex * samplerStride;
            samplerDescriptor.size    = samplerStride;

            DE_ASSERT(!binding.nullDescriptor);
        }

        switch (binding.mapping.source)
        {
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
        {
            const auto &sourceData          = binding.mapping.sourceData.indirectIndex;
            const uint32_t baseHandleOffset = sourceData.heapOffset / resourceStride;
            uint32_t indirectData           = binding.heapIndex - baseHandleOffset;
            if (sourceData.useCombinedImageSamplerIndex)
            {
                const uint32_t samplerBaseHandleOffset = sourceData.samplerHeapOffset / samplerStride;
                indirectData |= (binding.samplerHeapIndex - samplerBaseHandleOffset) << 20;
            }

            deMemcpy(indirectAddressBufferHostPtr + sourceData.addressOffset, &indirectData, sizeof(indirectData));

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = sourceData.pushOffset;
            pushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
            pushDataInfo.data.address      = &m_indirectAddressBuffer->address;
            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);

            if (!sourceData.useCombinedImageSamplerIndex && !m_params.embeddedSamplers)
            {
                const uint32_t samplerIndirectData =
                    binding.samplerHeapIndex - sourceData.samplerHeapOffset / samplerStride;
                deMemcpy(indirectAddressBufferHostPtr + sourceData.samplerAddressOffset, &samplerIndirectData,
                         sizeof(samplerIndirectData));

                VkPushDataInfoEXT samplerPushDataInfo = initVulkanStructure();
                samplerPushDataInfo.offset            = sourceData.samplerPushOffset;
                samplerPushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
                samplerPushDataInfo.data.address      = &m_indirectAddressBuffer->address;
                vk.cmdPushDataEXT(cmdBuf, &samplerPushDataInfo);
            }
            break;
        }
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
        {
            const VkDescriptorMappingSourceDataEXT &sourceData = binding.mapping.sourceData;

            const uint32_t baseHandleOffset = sourceData.shaderRecordIndex.heapOffset / resourceStride;
            uint32_t sbtData                = binding.heapIndex - baseHandleOffset;
            if (sourceData.shaderRecordIndex.useCombinedImageSamplerIndex)
            {
                const uint32_t samplerBaseHandleOffset = sourceData.shaderRecordIndex.samplerHeapOffset / samplerStride;
                sbtData |= (binding.samplerHeapIndex - samplerBaseHandleOffset) << 20;
            }

            deMemcpy(m_shaderRecordData.data() + sourceData.shaderRecordIndex.shaderRecordOffset, &sbtData,
                     sizeof(sbtData));

            if (!sourceData.shaderRecordIndex.useCombinedImageSamplerIndex && !m_params.embeddedSamplers)
            {
                const uint32_t samplerSbtData =
                    binding.samplerHeapIndex - sourceData.shaderRecordIndex.samplerHeapOffset / samplerStride;
                deMemcpy(m_shaderRecordData.data() + sourceData.shaderRecordIndex.samplerShaderRecordOffset,
                         &samplerSbtData, sizeof(samplerSbtData));
            }
            break;
        }
        case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
        {
            const auto &sourceData                 = binding.mapping.sourceData.indirectIndexArray;
            const uint32_t baseHandleOffset        = sourceData.heapOffset / resourceStride;
            const uint32_t samplerBaseHandleOffset = sourceData.samplerHeapOffset / samplerStride;

            std::vector<uint32_t> indirect(m_params.dimension);
            std::iota(indirect.begin(), indirect.end(), 0);

            for (uint32_t &indirectData : indirect)
            {
                indirectData += binding.heapIndex - baseHandleOffset;

                if (sourceData.useCombinedImageSamplerIndex)
                {
                    indirectData |= (binding.samplerHeapIndex - samplerBaseHandleOffset) << 20;
                }
            }

            deMemcpy(indirectAddressBufferHostPtr + sourceData.addressOffset, indirect.data(),
                     indirect.size() * sizeof(uint32_t));

            VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
            pushDataInfo.offset            = sourceData.pushOffset;
            pushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
            pushDataInfo.data.address      = &m_indirectAddressBuffer->address;
            vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);

            if (!sourceData.useCombinedImageSamplerIndex && !m_params.embeddedSamplers)
            {
                std::vector<uint32_t> samplerIndirect(m_params.dimension, 0);
                for (uint32_t &indirectData : samplerIndirect)
                {
                    indirectData = binding.samplerHeapIndex - samplerBaseHandleOffset;
                }

                deMemcpy(indirectAddressBufferHostPtr + sourceData.samplerAddressOffset, samplerIndirect.data(),
                         samplerIndirect.size() * sizeof(uint32_t));

                VkPushDataInfoEXT samplerPushDataInfo = initVulkanStructure();
                samplerPushDataInfo.offset            = sourceData.samplerPushOffset;
                samplerPushDataInfo.data.size         = sizeof(m_indirectAddressBuffer->address);
                samplerPushDataInfo.data.address      = &m_indirectAddressBuffer->address;
                vk.cmdPushDataEXT(cmdBuf, &samplerPushDataInfo);
            }
            break;
        }
        default:
            break;
        }
    }

    const int arraySize = binding.arrayed ? m_params.dimension : 1;
    std::vector<int32_t> descriptorValues;

    for (int arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
    {
        int32_t descriptorValue = 0;
        const int bindingIndex  = heapIndex + arrayIndex;
        DE_ASSERT(bindingIndex >= 2);

        switch (binding.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        {
            VkHostAddressRangeEXT &descriptor = m_deferredSamplerHostDescriptors.emplace_back();
            descriptor.address                = samplerDescriptorHeapHostPtr + bindingIndex * samplerStride;
            descriptor.size                   = samplerStride;

            int32_t borderColor = 0;

            if (m_params.enableCustomBorderColor)
            {
                borderColor = rnd.getInt32() | 3;
            }
            else
            {
                const bool useBlack = (arraySize > 1) && ((rnd.getInt32() & 1) != 0);
                borderColor         = useBlack ? 0 : 1;
            }

            descriptorValue = borderColor;

            if (binding.shiftSamplerResult)
            {
                descriptorValue <<= arrayIndex;
            }

            VkSamplerCreateInfo &samplerCreateInfo = m_deferredSamplerDescriptors.emplace_back();
            samplerCreateInfo                      = makeBorderCodedSamplerCreateInfo(borderColor == 0);

            if (m_params.enableCustomBorderColor)
            {
                auto &borderColorIndex       = m_stagingCustomBorderColorIndexCreateInfos.emplace_back();
                auto &customBorderCreateInfo = m_stagingCustomBorderColorCreateInfos.emplace_back();

                customBorderCreateInfo                            = initVulkanStructure();
                customBorderCreateInfo.customBorderColor.int32[0] = borderColor;
                customBorderCreateInfo.format                     = VK_FORMAT_R32_SINT;

                uint32_t borderIndex = 0;
                VK_CHECK(vk.registerCustomBorderColorEXT(*m_device, &customBorderCreateInfo, VK_FALSE, &borderIndex));
                m_registeredBorderColors.push_back(borderIndex);

                borderColorIndex       = initVulkanStructure();
                borderColorIndex.pNext = &customBorderCreateInfo;
                borderColorIndex.index = borderIndex;

                samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
                samplerCreateInfo.pNext       = &borderColorIndex;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        {
            auto &imageViewCreateInfo = m_stagingImageViewCreateInfos.emplace_back();
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            {
                descriptorValue = prePassClearColor;

                DE_ASSERT(arraySize == 1);
                DE_ASSERT(m_params.inputAttachments);

                imageViewCreateInfo                                 = initVulkanStructure();
                imageViewCreateInfo.flags                           = 0;
                imageViewCreateInfo.image                           = prePassImage;
                imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
                imageViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
                imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
                imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
                imageViewCreateInfo.subresourceRange.levelCount     = 1;
                imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
                imageViewCreateInfo.subresourceRange.layerCount     = 1;
            }
            else
            {
                descriptorValue = rnd.getInt32();

                imageViewCreateInfo = createTestImageViewInfo(descriptorValue, cmdBuf);
            }

            auto &descriptor   = m_deferredResourceHostDescriptors.emplace_back();
            descriptor.address = resourceDescriptorHeapHostPtr + bindingIndex * resourceStride;
            descriptor.size    = static_cast<size_t>(m_descriptorHeapProperties.imageDescriptorSize);

            auto &imageDescriptorInfo  = m_stagingImageDescriptorInfos.emplace_back();
            imageDescriptorInfo        = initVulkanStructure();
            imageDescriptorInfo.pView  = &imageViewCreateInfo;
            imageDescriptorInfo.layout = VK_IMAGE_LAYOUT_GENERAL;

            auto &resourceInfo       = m_deferredResourceDescriptors.emplace_back();
            resourceInfo             = initVulkanStructure();
            resourceInfo.type        = binding.descriptorType;
            resourceInfo.data.pImage = &imageDescriptorInfo;

            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
                resourceInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                descriptorValue ^= 1;
            }
            if (binding.nullDescriptor)
            {
                resourceInfo.data.pImage = nullptr;
                descriptorValue          = 0;
            }
            if (binding.samplerIsNull)
            {
                descriptorValue = 0;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                bool usesCustomImplementation = true;

                switch (binding.mapping.source)
                {
                case VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT:
                {
                    descriptorValue = rnd.getInt32();

                    // This can be anything, as long as it's a multiple of 4.
                    const int32_t customPushOffset = 16;

                    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                    pushDataInfo.offset            = binding.mapping.sourceData.heapData.pushOffset;
                    pushDataInfo.data.size         = sizeof(customPushOffset);
                    pushDataInfo.data.address      = &customPushOffset;
                    vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);

                    deMemcpy(resourceDescriptorHeapHostPtr + binding.mapping.sourceData.heapData.heapOffset +
                                 customPushOffset,
                             &descriptorValue, sizeof(descriptorValue));
                    break;
                }
                case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT:
                {
                    descriptorValue = rnd.getInt32();

                    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                    pushDataInfo.offset            = binding.mapping.sourceData.pushDataOffset;
                    pushDataInfo.data.size         = sizeof(descriptorValue);
                    pushDataInfo.data.address      = &descriptorValue;
                    vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
                    break;
                }
                case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT:
                {
                    descriptorValue = rnd.getInt32();
                    deMemcpy(m_shaderRecordData.data() + binding.mapping.sourceData.shaderRecordDataOffset,
                             &descriptorValue, sizeof(descriptorValue));
                    break;
                }
                default:
                    usesCustomImplementation = false;
                    break;
                }
                if (usesCustomImplementation)
                {
                    DE_ASSERT(!binding.nullDescriptor);
                    break;
                }
            }

            const VkDeviceSize bufferSize    = 256;
            VkBufferUsageFlags2KHR usageBits = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                usageBits |= VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT_KHR;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                usageBits |= VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT_KHR;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                usageBits |= VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                usageBits |= VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR;
                break;
            default:
                break;
            }

            const auto &buffer = m_stagingBuffers.emplace_back(createBufferAndMemory(bufferSize, usageBits));
            std::memset(buffer->memory->getHostPtr(), 0, bufferSize);

            const int32_t randomValue = rnd.getInt32();
            deMemcpy(buffer->memory->getHostPtr(), &randomValue, sizeof(randomValue));
            descriptorValue = randomValue;

            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
                auto &texelBufferDescriptorInfo                = m_stagingTexelBufferDescriptorInfos.emplace_back();
                texelBufferDescriptorInfo                      = initVulkanStructure();
                texelBufferDescriptorInfo.format               = VK_FORMAT_R32_SINT;
                texelBufferDescriptorInfo.addressRange.address = buffer->address;
                texelBufferDescriptorInfo.addressRange.size    = bufferSize;

                auto &resourceInfo             = m_deferredResourceDescriptors.emplace_back();
                resourceInfo                   = initVulkanStructure();
                resourceInfo.type              = binding.descriptorType;
                resourceInfo.data.pTexelBuffer = &texelBufferDescriptorInfo;

                auto &descriptor   = m_deferredResourceHostDescriptors.emplace_back();
                descriptor.address = resourceDescriptorHeapHostPtr + bindingIndex * resourceStride;
                descriptor.size    = resourceStride;

                if (binding.nullDescriptor)
                {
                    descriptorValue                = 0;
                    resourceInfo.data.pTexelBuffer = nullptr;
                }
                if (binding.samplerIsNull)
                {
                    descriptorValue = 0;
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                auto &bufferDescriptorInfo   = m_stagingDeviceAddressRanges.emplace_back();
                bufferDescriptorInfo.address = buffer->address;
                bufferDescriptorInfo.size    = bufferSize;

                if ((binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT) ||
                    (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT) ||
                    (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT))
                {
                    const auto &bufferData = m_stagingBuffers.emplace_back(
                        createBufferAndMemory(256, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));
                    deMemcpy(bufferData->memory->getHostPtr(), &randomValue, sizeof(randomValue));

                    if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT)
                    {
                        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                        pushDataInfo.offset            = binding.mapping.sourceData.pushAddressOffset;
                        pushDataInfo.data.size         = sizeof(bufferData->address);
                        pushDataInfo.data.address      = &bufferData->address;
                        vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
                    }
                    else if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT)
                    {
                        auto &indirectBuffer = m_stagingBuffers.emplace_back(
                            createBufferAndMemory(256, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));
                        auto indirectBufferHostPtr = static_cast<char *>(indirectBuffer->memory->getHostPtr()) +
                                                     binding.mapping.sourceData.indirectAddress.addressOffset;
                        deMemcpy(indirectBufferHostPtr, &bufferData->address, sizeof(bufferData->address));

                        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                        pushDataInfo.offset            = binding.mapping.sourceData.indirectAddress.pushOffset;
                        pushDataInfo.data.size         = sizeof(indirectBuffer->address);
                        pushDataInfo.data.address      = &indirectBuffer->address;
                        vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
                    }
                    else if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT)
                    {
                        deMemcpy(m_shaderRecordData.data() + binding.mapping.sourceData.shaderRecordAddressOffset,
                                 &bufferData->address, sizeof(bufferData->address));
                    }
                    else
                    {
                        DE_ASSERT(0);
                    }
                }
                else
                {
                    auto &resourceInfo              = m_deferredResourceDescriptors.emplace_back();
                    resourceInfo                    = initVulkanStructure();
                    resourceInfo.type               = binding.descriptorType;
                    resourceInfo.data.pAddressRange = &bufferDescriptorInfo;

                    auto &descriptor   = m_deferredResourceHostDescriptors.emplace_back();
                    descriptor.address = resourceDescriptorHeapHostPtr + bindingIndex * resourceStride;
                    descriptor.size    = resourceStride;

                    if (binding.nullDescriptor)
                    {
                        resourceInfo.data.pAddressRange = nullptr;
                        descriptorValue                 = 0;
                    }
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        {
            // Code taken from vktBindingDescriptorBufferTests.cpp
            const int32_t randomValue = static_cast<int32_t>(rnd.getUint32() & 0xfff);
            descriptorValue           = randomValue;

            const float zDepth = static_cast<float>(randomValue);
            const std::vector<tcu::Vec3> vertices{
                tcu::Vec3(-1.0f, -1.0f, zDepth), tcu::Vec3(-1.0f, 1.0f, zDepth), tcu::Vec3(1.0f, -1.0f, zDepth),
                tcu::Vec3(-1.0f, 1.0f, zDepth),  tcu::Vec3(1.0f, 1.0f, zDepth),  tcu::Vec3(1.0f, -1.0f, zDepth),
            };

            const auto createFlags = VkAccelerationStructureCreateFlagsKHR{0};
            const auto memoryReqs  = MemoryRequirement::Any;
            AccelerationStructBufferProperties bufferProps;

            auto &rtBlas = m_rtBlases.emplace_back(de::SharedPtr(makeBottomLevelAccelerationStructure().release()));
            rtBlas->setCreateFlags(createFlags);
            rtBlas->setGeometryData(vertices, true);
            rtBlas->create(vk, *m_device, m_device.getAllocator(), bufferProps, 0, 0, 0, 0, nullptr, memoryReqs);

            auto &rtTlas = m_rtTlases.emplace_back(MovePtr(makeTopLevelAccelerationStructure().release()));
            rtTlas->addInstance(rtBlas);
            rtTlas->setCreateFlags(createFlags);
            rtTlas->create(vk, *m_device, m_device.getAllocator(), bufferProps, 0, 0, 0, 0, nullptr, memoryReqs);

            uint64_t accelerationStructureAddress =
                getAccelerationStructureDeviceAddress(vk, *m_device, *rtTlas->getPtr());

            if (binding.nullDescriptor)
            {
                accelerationStructureAddress = 0;

                if (m_params.stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
                {
                    descriptorValue = kRayTraceDefaultHitValue;
                }
                else
                {
                    descriptorValue = 0;
                }
            }

            if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT)
            {
                VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                pushDataInfo.offset            = binding.mapping.sourceData.pushAddressOffset;
                pushDataInfo.data.size         = sizeof(accelerationStructureAddress);
                pushDataInfo.data.address      = &accelerationStructureAddress;
                vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
            }
            else if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT)
            {
                auto &indirectBuffer = m_stagingBuffers.emplace_back(
                    createBufferAndMemory(256, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));
                auto indirectBufferHostPtr = static_cast<char *>(indirectBuffer->memory->getHostPtr()) +
                                             binding.mapping.sourceData.indirectAddress.addressOffset;
                deMemcpy(indirectBufferHostPtr, &accelerationStructureAddress, sizeof(accelerationStructureAddress));

                VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
                pushDataInfo.offset            = binding.mapping.sourceData.indirectAddress.pushOffset;
                pushDataInfo.data.size         = sizeof(indirectBuffer->address);
                pushDataInfo.data.address      = &indirectBuffer->address;
                vk.cmdPushDataEXT(cmdBuf, &pushDataInfo);
            }
            else if (binding.mapping.source == VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT)
            {
                deMemcpy(m_shaderRecordData.data() + binding.mapping.sourceData.shaderRecordAddressOffset,
                         &accelerationStructureAddress, sizeof(accelerationStructureAddress));
            }
            else
            {
                auto &descriptor   = m_deferredResourceHostDescriptors.emplace_back();
                descriptor.address = resourceDescriptorHeapHostPtr + bindingIndex * resourceStride;
                descriptor.size    = resourceStride;

                auto &bufferDescriptorInfo   = m_stagingDeviceAddressRanges.emplace_back();
                bufferDescriptorInfo.address = accelerationStructureAddress;
                bufferDescriptorInfo.size    = 0;

                auto &resourceInfo              = m_deferredResourceDescriptors.emplace_back();
                resourceInfo                    = initVulkanStructure();
                resourceInfo.type               = binding.descriptorType;
                resourceInfo.data.pAddressRange = &bufferDescriptorInfo;

                if (binding.nullDescriptor)
                {
                    resourceInfo.data.pAddressRange = nullptr;
                }
            }

            rtBlas->build(vk, *m_device, cmdBuf);
            rtTlas->build(vk, *m_device, cmdBuf);
            break;
        }
        default:
            DE_ASSERT(0);
            break;
        }
        if (binding.arrayed)
        {
            descriptorValues.push_back(descriptorValue);
        }
        else
        {
            // Broadcast
            for (auto &value : expectedResult)
            {
                value ^= descriptorValue;
            }
        }
    }

    if (binding.arrayed)
    {
        for (int index = 0; index < arraySize; ++index)
        {
            const int swizzledIndex = swizzlerData[index];
            expectedResult[index] ^= descriptorValues[swizzledIndex];
        }
    }
}

VkPipeline DescriptorHeapTestInstanceBasic::initComputePipeline(
    const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings, uint32_t queueIndex)
{
    auto &shaderBinary      = getShaderBinary("compute" + std::to_string(queueIndex));
    const auto shaderModule = createShaderModule(m_device.getDriver(), *m_device, shaderBinary);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = initVulkanStructure();
    pipelineShaderStageCreateInfo.pNext                           = &mappingInfo;
    pipelineShaderStageCreateInfo.flags                           = 0;
    pipelineShaderStageCreateInfo.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineShaderStageCreateInfo.module                          = *shaderModule;
    pipelineShaderStageCreateInfo.pName                           = "main";
    pipelineShaderStageCreateInfo.pSpecializationInfo             = nullptr;

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.flags                       = 0;
    pipelineCreateInfo.stage                       = pipelineShaderStageCreateInfo;
    pipelineCreateInfo.layout                      = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineHandle          = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex           = 0;

    return *m_stagingPipelines.emplace_back(
        createComputePipeline(m_device.getDriver(), *m_device, VK_NULL_HANDLE, &pipelineCreateInfo));
}

VkRenderPass DescriptorHeapTestInstanceBasic::initRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R32_SINT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference colorReference0{};
    colorReference0.attachment = 0;
    colorReference0.layout     = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference colorReference1{};
    colorReference1.attachment = 1;
    colorReference1.layout     = VK_IMAGE_LAYOUT_GENERAL;

    if (m_params.inputAttachments)
    {
        std::array<VkSubpassDescription, 2> subpasses{};
        subpasses[0].pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[0].inputAttachmentCount = 0;
        subpasses[0].pInputAttachments    = nullptr;
        subpasses[0].colorAttachmentCount = 1;
        subpasses[0].pColorAttachments    = &colorReference0;

        subpasses[1].pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[1].inputAttachmentCount = 1;
        subpasses[1].pInputAttachments    = &colorReference0;
        subpasses[1].colorAttachmentCount = 1;
        subpasses[1].pColorAttachments    = &colorReference1;

        std::array<VkAttachmentDescription, 2> attachments{colorAttachment, colorAttachment};

        std::array<VkSubpassDependency, 1> dependencies{};
        dependencies[0].srcSubpass      = 0;
        dependencies[0].dstSubpass      = 1;
        dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;

        VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
        renderPassCreateInfo.attachmentCount        = de::sizeU32(attachments);
        renderPassCreateInfo.pAttachments           = attachments.data();
        renderPassCreateInfo.subpassCount           = de::sizeU32(subpasses);
        renderPassCreateInfo.pSubpasses             = subpasses.data();
        renderPassCreateInfo.dependencyCount        = de::sizeU32(dependencies);
        renderPassCreateInfo.pDependencies          = dependencies.data();

        return *m_stagingRenderPasses.emplace_back(
            createRenderPass(m_device.getDriver(), *m_device, &renderPassCreateInfo));
    }
    else
    {
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments    = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorReference0;

        VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
        renderPassCreateInfo.attachmentCount        = 1;
        renderPassCreateInfo.pAttachments           = &colorAttachment;
        renderPassCreateInfo.subpassCount           = 1;
        renderPassCreateInfo.pSubpasses             = &subpass;

        return *m_stagingRenderPasses.emplace_back(
            createRenderPass(m_device.getDriver(), *m_device, &renderPassCreateInfo));
    }
}

VkPipeline DescriptorHeapTestInstanceBasic::initGraphicsPipeline(
    const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings, VkRenderPass renderPass, uint32_t queueIndex)
{
    auto &vertBinary = getShaderBinary("vertex" + std::to_string(queueIndex));
    auto &fragBinary = getShaderBinary("fragment" + std::to_string(queueIndex));

    const auto vertModule = createShaderModule(m_device.getDriver(), *m_device, vertBinary);
    const auto fragModule = createShaderModule(m_device.getDriver(), *m_device, fragBinary);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStagesCreateInfo = {
        initVulkanStructure(),
        initVulkanStructure(),
    };
    pipelineShaderStagesCreateInfo[0].pNext  = &mappingInfo;
    pipelineShaderStagesCreateInfo[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineShaderStagesCreateInfo[0].module = *vertModule;
    pipelineShaderStagesCreateInfo[0].pName  = "main";

    pipelineShaderStagesCreateInfo[1].pNext  = &mappingInfo;
    pipelineShaderStagesCreateInfo[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineShaderStagesCreateInfo[1].module = *fragModule;
    pipelineShaderStagesCreateInfo[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    vertexInputState.vertexBindingDescriptionCount        = 0;
    vertexInputState.pVertexBindingDescriptions           = nullptr;
    vertexInputState.vertexAttributeDescriptionCount      = 0;
    vertexInputState.pVertexAttributeDescriptions         = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    VkRect2D scissor    = {{0, 0}, {1, 1}};

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.depthClampEnable                       = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable                = VK_FALSE;
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
    rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.depthBiasEnable                        = VK_FALSE;
    rasterizationState.depthBiasConstantFactor                = 0.0f;
    rasterizationState.depthBiasClamp                         = 0.0f;
    rasterizationState.depthBiasSlopeFactor                   = 0.0f;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
    depthStencilState.maxDepthBounds                        = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = 1;
    colorBlendState.pAttachments                        = &colorBlendAttachment;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    VkSampleMask sampleMask                               = 0xffffffff;
    multisampleState.pSampleMask                          = &sampleMask;

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkGraphicsPipelineCreateInfo createInfo = initVulkanStructure();
    createInfo.pNext                        = &pipelineCreateFlags2CreateInfo;
    createInfo.flags                        = 0;
    createInfo.stageCount                   = static_cast<uint32_t>(pipelineShaderStagesCreateInfo.size());
    createInfo.pStages                      = pipelineShaderStagesCreateInfo.data();
    createInfo.pVertexInputState            = &vertexInputState;
    createInfo.pInputAssemblyState          = &inputAssemblyState;
    createInfo.pTessellationState           = nullptr;
    createInfo.pViewportState               = &viewportState;
    createInfo.pRasterizationState          = &rasterizationState;
    createInfo.pMultisampleState            = &multisampleState;
    createInfo.pDepthStencilState           = &depthStencilState;
    createInfo.pColorBlendState             = &colorBlendState;
    createInfo.pDynamicState                = nullptr;
    createInfo.layout                       = VK_NULL_HANDLE;
    createInfo.renderPass                   = renderPass;
    createInfo.subpass                      = m_params.inputAttachments ? 1 : 0;
    createInfo.basePipelineHandle           = VK_NULL_HANDLE;
    createInfo.basePipelineIndex            = 0;
    return *m_stagingPipelines.emplace_back(
        createGraphicsPipeline(m_device.getDriver(), *m_device, VK_NULL_HANDLE, &createInfo));
}

uint32_t getShaderGroupHandleSize(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

    return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

uint32_t getShaderGroupBaseAlignment(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

    rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

    return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

VkPipeline DescriptorHeapTestInstanceBasic::initRayTracingPipeline(
    const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings, uint32_t queueIndex)
{
    // Code taken from vktBindingDescriptorBufferTests.cpp
    const InstanceInterface &vki          = m_instance.getDriver();
    const DeviceInterface &vkd            = m_device.getDriver();
    const VkDevice device                 = *m_device;
    const VkPhysicalDevice physicalDevice = m_physDevice;
    vk::BinaryCollection &collection      = m_context.getBinaryCollection();
    Allocator &allocator                  = m_device.getAllocator();
    const VkShaderStageFlags hitStages =
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    std::string raygenName       = "raygen" + std::to_string(queueIndex);
    std::string anyhitName       = "anyhit" + std::to_string(queueIndex);
    std::string closesthitName   = "closesthit" + std::to_string(queueIndex);
    std::string missName         = "miss" + std::to_string(queueIndex);
    std::string intersectionName = "intersection" + std::to_string(queueIndex);
    std::string callableName     = "callable" + std::to_string(queueIndex);

    uint32_t raygenShaderGroup   = ~0u;
    uint32_t missShaderGroup     = ~0u;
    uint32_t hitShaderGroup      = ~0u;
    uint32_t callableShaderGroup = ~0u;
    uint32_t shaderGroupCount    = 0;

    uint32_t shaders = 0;
    if (collection.contains(raygenName))
        shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (collection.contains(anyhitName))
        shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (collection.contains(closesthitName))
        shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (collection.contains(missName))
        shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
    if (collection.contains(intersectionName))
        shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    if (collection.contains(callableName))
        shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    if (0 != (shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
        raygenShaderGroup = shaderGroupCount++;

    if (0 != (shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
        missShaderGroup = shaderGroupCount++;

    if (0 != (shaders & hitStages))
        hitShaderGroup = shaderGroupCount++;

    if (0 != (shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
        callableShaderGroup = shaderGroupCount++;

    RayTracingPipeline *rayTracingPipeline =
        m_rayTracingPipelines.emplace_back(de::newMovePtr<RayTracingPipeline>()).get();

    rayTracingPipeline->setCreateFlags2(VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT);

    if (0 != (shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenName,
                            raygenShaderGroup);
    }
    if (0 != (shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyhitName, hitShaderGroup);
    }
    if (0 != (shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closesthitName,
                            hitShaderGroup);
    }
    if (0 != (shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_MISS_BIT_KHR, missName, missShaderGroup);
    }
    if (0 != (shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionName,
                            hitShaderGroup);
    }
    if (0 != (shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
    {
        addRayTracingShader(mappings, rayTracingPipeline, VK_SHADER_STAGE_CALLABLE_BIT_KHR, callableName,
                            callableShaderGroup);
    }

    VkPipeline pipeline =
        *m_stagingPipelines.emplace_back(rayTracingPipeline->createPipeline(vkd, device, VK_NULL_HANDLE));

    m_stagingSBTBuffers.push_back(createShaderBindingTable(vki, vkd, device, physicalDevice, pipeline, allocator,
                                                           rayTracingPipeline, raygenShaderGroup, shaderGroupCount,
                                                           m_raygenShaderBindingTableRegion));
    m_stagingSBTBuffers.push_back(createShaderBindingTable(vki, vkd, device, physicalDevice, pipeline, allocator,
                                                           rayTracingPipeline, missShaderGroup, shaderGroupCount,
                                                           m_missShaderBindingTableRegion));
    m_stagingSBTBuffers.push_back(createShaderBindingTable(vki, vkd, device, physicalDevice, pipeline, allocator,
                                                           rayTracingPipeline, hitShaderGroup, shaderGroupCount,
                                                           m_hitShaderBindingTableRegion));
    m_stagingSBTBuffers.push_back(createShaderBindingTable(vki, vkd, device, physicalDevice, pipeline, allocator,
                                                           rayTracingPipeline, callableShaderGroup, shaderGroupCount,
                                                           m_callableShaderBindingTableRegion));

    return pipeline;
}

std::pair<VkImage, VkImageView> DescriptorHeapTestInstanceBasic::initPrePassRenderTarget()
{
    VkImageCreateInfo createInfo = initVulkanStructure();
    createInfo.imageType         = VK_IMAGE_TYPE_2D;
    createInfo.format            = VK_FORMAT_R32_SINT;
    createInfo.extent            = {1, 1, 1};
    createInfo.mipLevels         = 1;
    createInfo.arrayLayers       = 1;
    createInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage prePassImage     = *m_stagingImages.emplace_back(createImageAndMemory(createInfo))->image;

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = prePassImage;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
    imageViewCreateInfo.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;
    VkImageView prePassImageView =
        *m_stagingImageViews.emplace_back(createImageView(m_device.getDriver(), *m_device, &imageViewCreateInfo));

    return {prePassImage, prePassImageView};
}

std::pair<VkImage, VkImageView> DescriptorHeapTestInstanceBasic::initRenderTarget()
{
    VkImageCreateInfo createInfo = initVulkanStructure();
    createInfo.imageType         = VK_IMAGE_TYPE_2D;
    createInfo.format            = VK_FORMAT_R32_SINT;
    createInfo.extent            = {1, 1, 1};
    createInfo.mipLevels         = 1;
    createInfo.arrayLayers       = 1;
    createInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling            = VK_IMAGE_TILING_LINEAR;
    createInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage renderTargetImage    = *m_stagingImages.emplace_back(createImageAndMemory(createInfo))->image;

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = renderTargetImage;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
    imageViewCreateInfo.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;
    VkImageView renderTargetImageView =
        *m_stagingImageViews.emplace_back(createImageView(m_device.getDriver(), *m_device, &imageViewCreateInfo));

    return {renderTargetImage, renderTargetImageView};
}

VkFramebuffer DescriptorHeapTestInstanceBasic::initFramebuffer(VkRenderPass renderPass,
                                                               VkImageView renderTargetImageView,
                                                               VkImageView prePassImageView)
{
    std::vector<VkImageView> renderTargetAttachments;
    if (m_params.inputAttachments)
    {
        DE_ASSERT(prePassImageView);
        renderTargetAttachments = {prePassImageView, renderTargetImageView};
    }
    else
    {
        renderTargetAttachments = {renderTargetImageView};
    }

    VkFramebufferCreateInfo framebufferCreateInfo = initVulkanStructure();
    framebufferCreateInfo.attachmentCount         = static_cast<uint32_t>(renderTargetAttachments.size());
    framebufferCreateInfo.pAttachments            = renderTargetAttachments.data();
    framebufferCreateInfo.renderPass              = renderPass;
    framebufferCreateInfo.width                   = 1;
    framebufferCreateInfo.height                  = 1;
    framebufferCreateInfo.layers                  = 1;
    return *m_stagingFramebuffers.emplace_back(
        createFramebuffer(m_device.getDriver(), *m_device, &framebufferCreateInfo));
}

void DescriptorHeapTestInstanceBasic::addRayTracingShader(
    const std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings, RayTracingPipeline *rayTracingPipeline,
    VkShaderStageFlagBits stage, const std::string &name, uint32_t group)
{
    DE_ASSERT(rayTracingPipeline);

    auto &mappingInfo        = m_rayTracingMappingInfos.emplace_back(initVulkanStructure());
    mappingInfo.mappingCount = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings    = mappings.data();

    rayTracingPipeline->addShader(stage, createShaderModule(m_device.getDriver(), *m_device, getShaderBinary(name), 0),
                                  group, nullptr, 0, &mappingInfo);
}

de::MovePtr<BufferWithMemory> DescriptorHeapTestInstanceBasic::createShaderBindingTable(
    const InstanceInterface &vki, const DeviceInterface &vkd, VkDevice device, VkPhysicalDevice physicalDevice,
    VkPipeline pipeline, Allocator &allocator, RayTracingPipeline *rayTracingPipeline, uint32_t group,
    uint32_t shaderGroupCount, VkStridedDeviceAddressRegionKHR &shaderBindingTableRegion)
{
    de::MovePtr<BufferWithMemory> shaderBindingTable;

    if (group < shaderGroupCount)
    {
        const uint32_t shaderGroupHandleSize    = getShaderGroupHandleSize(vki, physicalDevice);
        const uint32_t shaderGroupBaseAlignment = getShaderGroupBaseAlignment(vki, physicalDevice);

        const auto alignedSize = de::roundUp(shaderGroupHandleSize + kShaderRecordSize, shaderGroupHandleSize);

        shaderBindingTable = rayTracingPipeline->createShaderBindingTable(
            vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u, 0, 0,
            MemoryRequirement::Any, 0, 0, kShaderRecordSize);

        shaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
            getBufferDeviceAddress(vkd, device, shaderBindingTable->get(), 0), alignedSize, alignedSize);

        auto dataPtr =
            reinterpret_cast<uint8_t *>(shaderBindingTable->getAllocation().getHostPtr()) + shaderGroupHandleSize;
        deMemcpy(dataPtr, m_shaderRecordData.data(), m_shaderRecordData.size());
    }

    return shaderBindingTable;
}

VkImageViewCreateInfo DescriptorHeapTestInstanceBasic::createTestImageViewInfo(int32_t value, VkCommandBuffer cmdBuf)
{
    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.flags             = 0;
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_1D;
    imageCreateInfo.format            = VK_FORMAT_R32_SINT;
    imageCreateInfo.extent            = {1, 1, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices   = nullptr;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    auto &image                           = m_stagingImages.emplace_back(createImageAndMemory(imageCreateInfo));

    auto &stagingBuffer =
        m_stagingBuffers.emplace_back(createBufferAndMemory(sizeof(int32_t), VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR));
    deMemcpy(stagingBuffer->memory->getHostPtr(), &value, sizeof(value));

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {1, 1, 1};

    VkImageMemoryBarrier2 undefined2transferImageMemoryBarrier           = initVulkanStructure();
    undefined2transferImageMemoryBarrier.srcStageMask                    = 0;
    undefined2transferImageMemoryBarrier.srcAccessMask                   = 0;
    undefined2transferImageMemoryBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    undefined2transferImageMemoryBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    undefined2transferImageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    undefined2transferImageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    undefined2transferImageMemoryBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2transferImageMemoryBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2transferImageMemoryBarrier.image                           = *image->image;
    undefined2transferImageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    undefined2transferImageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
    undefined2transferImageMemoryBarrier.subresourceRange.levelCount     = 1;
    undefined2transferImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    undefined2transferImageMemoryBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo undefined2transferDependencyInfo        = initVulkanStructure();
    undefined2transferDependencyInfo.imageMemoryBarrierCount = 1;
    undefined2transferDependencyInfo.pImageMemoryBarriers    = &undefined2transferImageMemoryBarrier;

    VkImageMemoryBarrier2 transfer2sampleImageMemoryBarrier           = initVulkanStructure();
    transfer2sampleImageMemoryBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    transfer2sampleImageMemoryBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    transfer2sampleImageMemoryBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    transfer2sampleImageMemoryBarrier.dstAccessMask                   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    transfer2sampleImageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transfer2sampleImageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transfer2sampleImageMemoryBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    transfer2sampleImageMemoryBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    transfer2sampleImageMemoryBarrier.image                           = *image->image;
    transfer2sampleImageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    transfer2sampleImageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
    transfer2sampleImageMemoryBarrier.subresourceRange.levelCount     = 1;
    transfer2sampleImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    transfer2sampleImageMemoryBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo transfer2sampleDependencyInfo        = initVulkanStructure();
    transfer2sampleDependencyInfo.imageMemoryBarrierCount = 1;
    transfer2sampleDependencyInfo.pImageMemoryBarriers    = &transfer2sampleImageMemoryBarrier;

    auto &vk = m_device.getDriver();
    vk.cmdPipelineBarrier2(cmdBuf, &undefined2transferDependencyInfo);
    vk.cmdCopyBufferToImage(cmdBuf, *stagingBuffer->buffer, *image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                            &region);
    vk.cmdPipelineBarrier2(cmdBuf, &transfer2sampleDependencyInfo);

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.flags                           = 0;
    imageViewCreateInfo.image                           = *image->image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_1D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
    imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;
    return imageViewCreateInfo;
}

tcu::TestStatus DescriptorHeapTestInstanceInvariance::iterate()
{
    const auto &vki = m_instance.getDriver();
    const auto &vkd = m_device.getDriver();

    de::Random rnd(m_params.seed);
    for (int i = 0; i < 4; ++i)
    {
        m_customBorderColor.float32[0] = rnd.getFloat(0.0f, 100.0f);
    }

    VkDeviceSize descriptorSize = vki.getPhysicalDeviceDescriptorSizeEXT(m_physDevice, m_params.descriptorType);

    if (m_params.enableSamplerYcbcrConversion)
    {
        descriptorSize *= m_context.getDescriptorHeapPropertiesEXT().samplerYcbcrConversionCount;
    }

    VK_CHECK(createInvarianceResources(m_params.enableCaptureReplay, false));

    // Increase the reference size a bit to catch out of bound writes.
    std::vector<char> referenceData(static_cast<size_t>(descriptorSize) * 16);
    VK_CHECK(writeInvarianceDescriptor(referenceData));

    if (m_params.enableCaptureReplay)
    {
        // Reset allocations before replay.
        m_buffer = {};
        m_image  = {};
        m_imageMemory.clear();
        m_rtBlas.clear();
        m_rtTlas.clear();
        m_samplerYcbcrConversion = {};

        if (m_params.enableCustomBorderColor)
        {
            vkd.unregisterCustomBorderColorEXT(*m_device, m_customBorderColorIndex);
        }

        VK_CHECK(createInvarianceResources(false, true));
    }

    int maxIterations = m_params.enableCaptureReplay ? 1 : 50000;

    for (int iteration = 0; iteration < maxIterations; ++iteration)
    {
        const char fill = static_cast<char>((iteration + 0xcc) & 0xff);

        std::vector<char> descriptorData(referenceData.size(), fill);
        std::vector<char> fillReference(referenceData.size(), fill);

        VK_CHECK(writeInvarianceDescriptor(descriptorData));

        if (std::memcmp(descriptorData.data(), referenceData.data(), static_cast<size_t>(descriptorSize)) != 0)
        {
            return tcu::TestStatus::fail("Descriptor data is not invariant");
        }
        if (descriptorSize < fillReference.size())
        {
            if (std::memcmp(descriptorData.data() + descriptorSize, fillReference.data() + descriptorSize,
                            static_cast<size_t>(fillReference.size() - descriptorSize)) != 0)
            {
                return tcu::TestStatus::fail("Descriptor write wrote more than advertised");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

VkResult DescriptorHeapTestInstanceInvariance::createInvarianceResources(bool capture, bool replay)
{
    auto &vki = m_instance.getDriver();
    auto &vkd = m_device.getDriver();

    VkMemoryOpaqueCaptureAddressAllocateInfo opaqueCaptureAddressAllocateInfo = initVulkanStructure();
    VkMemoryAllocateFlagsInfo allocFlagsInfo                                  = initVulkanStructure();
    void *memoryPNext                                                         = nullptr;
    if (capture || replay)
    {
        if (replay)
        {
            opaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = m_memoryCaptureAddress;
        }

        allocFlagsInfo.pNext = &opaqueCaptureAddressAllocateInfo;
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
        memoryPNext = &allocFlagsInfo;
    }

    switch (m_params.descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
        if (m_params.enableSamplerYcbcrConversion)
        {
            VkSamplerYcbcrConversionCreateInfo samplerYcbcrConvCreateInfo = initVulkanStructure();
            samplerYcbcrConvCreateInfo.format                             = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
            samplerYcbcrConvCreateInfo.ycbcrModel                  = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY;
            samplerYcbcrConvCreateInfo.ycbcrRange                  = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
            samplerYcbcrConvCreateInfo.components.r                = VK_COMPONENT_SWIZZLE_IDENTITY;
            samplerYcbcrConvCreateInfo.components.g                = VK_COMPONENT_SWIZZLE_IDENTITY;
            samplerYcbcrConvCreateInfo.components.b                = VK_COMPONENT_SWIZZLE_IDENTITY;
            samplerYcbcrConvCreateInfo.components.a                = VK_COMPONENT_SWIZZLE_IDENTITY;
            samplerYcbcrConvCreateInfo.xChromaOffset               = VK_CHROMA_LOCATION_COSITED_EVEN;
            samplerYcbcrConvCreateInfo.yChromaOffset               = VK_CHROMA_LOCATION_COSITED_EVEN;
            samplerYcbcrConvCreateInfo.chromaFilter                = VK_FILTER_NEAREST;
            samplerYcbcrConvCreateInfo.forceExplicitReconstruction = VK_FALSE;

            m_samplerYcbcrConversion = createSamplerYcbcrConversion(vkd, *m_device, &samplerYcbcrConvCreateInfo);
        }

        if (m_params.enableCustomBorderColor)
        {
            VkSamplerCustomBorderColorCreateInfoEXT customBorderColorCreateInfo = initVulkanStructure();
            customBorderColorCreateInfo.customBorderColor                       = m_customBorderColor;
            customBorderColorCreateInfo.format                                  = VK_FORMAT_R32G32B32A32_SFLOAT;

            VK_CHECK(vkd.registerCustomBorderColorEXT(*m_device, &customBorderColorCreateInfo, replay,
                                                      &m_customBorderColorIndex));
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {
        VkHostAddressRangeEXT captureDataRange{};
        captureDataRange.address = m_captureData.data();
        captureDataRange.size    = m_context.getDescriptorHeapPropertiesEXT().imageCaptureReplayOpaqueDataSize;

        VkHostAddressRangeConstEXT captureDataRangeConst{};
        captureDataRangeConst.address = m_captureData.data();
        captureDataRangeConst.size    = m_context.getDescriptorHeapPropertiesEXT().imageCaptureReplayOpaqueDataSize;

        VkOpaqueCaptureDataCreateInfoEXT captureDataCreateInfo = initVulkanStructure();
        captureDataCreateInfo.pData                            = &captureDataRangeConst;

        VkImageCreateInfo imageCreateInfo = initVulkanStructure();
        imageCreateInfo.pNext             = replay ? &captureDataCreateInfo : nullptr;
        imageCreateInfo.flags       = (capture || replay) ? VK_IMAGE_CREATE_DESCRIPTOR_HEAP_CAPTURE_REPLAY_BIT_EXT : 0;
        imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format      = VK_FORMAT_R8_UNORM;
        imageCreateInfo.extent      = {1, 1, 1};
        imageCreateInfo.mipLevels   = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        m_image = createImage(vkd, *m_device, &imageCreateInfo);

        auto imageMemReqs = getImageMemoryRequirements(vkd, *m_device, *m_image);
        auto memReqs    = (capture || replay) ? MemoryRequirement::DeviceAddressCaptureReplay : MemoryRequirement::Any;
        auto compatMask = imageMemReqs.memoryTypeBits & getCompatibleMemoryTypes(m_memoryProperties, memReqs);
        DE_ASSERT(compatMask != 0);
        static_cast<void>(compatMask);

        m_imageMemory = allocateExtended(vki, vkd, m_physDevice, *m_device, imageMemReqs, memReqs, memoryPNext);

        vkd.bindImageMemory(*m_device, *m_image, m_imageMemory->getMemory(), m_imageMemory->getOffset());

        if (capture)
        {
            VkDeviceMemoryOpaqueCaptureAddressInfo opaqueCaptureAddressInfo = initVulkanStructure();
            opaqueCaptureAddressInfo.memory                                 = m_imageMemory->getMemory();
            m_memoryCaptureAddress = vkd.getDeviceMemoryOpaqueCaptureAddress(*m_device, &opaqueCaptureAddressInfo);

            VK_CHECK(vkd.getImageOpaqueCaptureDataEXT(*m_device, 1, &m_image.get(), &captureDataRange));
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    {
        VkBufferOpaqueCaptureAddressCreateInfo opaqueCaptureAddressCreateInfo = initVulkanStructure();
        opaqueCaptureAddressCreateInfo.opaqueCaptureAddress                   = m_bufferCaptureAddress;

        VkBufferCreateInfo bufferCreateInfo = initVulkanStructure();
        bufferCreateInfo.pNext              = replay ? &opaqueCaptureAddressCreateInfo : nullptr;
        bufferCreateInfo.flags = (capture || replay) ? (int)VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT : 0;
        bufferCreateInfo.size  = 256;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.queueFamilyIndexCount = 0;
        bufferCreateInfo.pQueueFamilyIndices   = nullptr;

        m_buffer         = std::make_unique<Buffer>();
        m_buffer->buffer = createBuffer(vkd, *m_device, &bufferCreateInfo);

        auto bufferMemReqs = getBufferMemoryRequirements(vkd, *m_device, *m_buffer->buffer);

        VkPhysicalDevice physicalDevice                   = m_physDevice;
        VkPhysicalDeviceMemoryProperties memoryProperties = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
        auto memReqs = (capture || replay) ? MemoryRequirement::DeviceAddressCaptureReplay : MemoryRequirement::Any;
        uint32_t compatMask = bufferMemReqs.memoryTypeBits & getCompatibleMemoryTypes(memoryProperties, memReqs);
        DE_ASSERT(compatMask != 0);
        static_cast<void>(compatMask);

        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        m_buffer->memory = allocateExtended(m_instance.getDriver(), vkd, physicalDevice, *m_device, bufferMemReqs,
                                            memReqs, &allocFlagsInfo);
        vkd.bindBufferMemory(*m_device, *m_buffer->buffer, m_buffer->memory->getMemory(),
                             m_buffer->memory->getOffset());
        m_buffer->address = getBufferDeviceAddress(vkd, *m_device, *m_buffer->buffer, 0);

        if (capture)
        {
            VkDeviceMemoryOpaqueCaptureAddressInfo opaqueCaptureAddressInfo = initVulkanStructure();
            opaqueCaptureAddressInfo.memory                                 = m_buffer->memory->getMemory();
            m_memoryCaptureAddress = vkd.getDeviceMemoryOpaqueCaptureAddress(*m_device, &opaqueCaptureAddressInfo);

            VkBufferDeviceAddressInfo addressInfo = initVulkanStructure();
            addressInfo.buffer                    = *m_buffer->buffer;
            m_bufferCaptureAddress                = vkd.getBufferOpaqueCaptureAddress(*m_device, &addressInfo);
        }
        break;
    }
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
        const std::vector<tcu::Vec3> vertices{
            tcu::Vec3(-1.0f, -1.0f, 0.0f), tcu::Vec3(-1.0f, 1.0f, 0.0f), tcu::Vec3(1.0f, -1.0f, 0.0f),
            tcu::Vec3(-1.0f, 1.0f, 0.0f),  tcu::Vec3(1.0f, 1.0f, 0.0f),  tcu::Vec3(1.0f, -1.0f, 0.0f),
        };

        auto createFlags = VkAccelerationStructureCreateFlagsKHR{0};
        auto memoryReqs  = (capture || replay) ?
                               (MemoryRequirement::Any | MemoryRequirement::DeviceAddressCaptureReplay) :
                               MemoryRequirement::Any;
        if (capture || replay)
        {
            createFlags = VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR;
        }

        AccelerationStructBufferProperties bufferProps;
        const VkDeviceAddress captureBlasAddress = replay ? m_captureBlasAddress : 0;
        const VkDeviceAddress captureTlasAddress = replay ? m_captureTlasAddress : 0;

        m_rtBlas = de::SharedPtr(makeBottomLevelAccelerationStructure().release());
        m_rtBlas->setCreateFlags(createFlags);
        m_rtBlas->setGeometryData(vertices, true);
        m_rtBlas->create(vkd, *m_device, m_device.getAllocator(), bufferProps, 0, captureBlasAddress, 0, 0, nullptr,
                         memoryReqs);

        m_rtTlas = makeTopLevelAccelerationStructure();
        m_rtTlas->addInstance(m_rtBlas);
        m_rtTlas->setCreateFlags(createFlags);
        m_rtTlas->create(vkd, *m_device, m_device.getAllocator(), bufferProps, 0, captureTlasAddress, 0, 0, nullptr,
                         memoryReqs);

        if (capture)
        {
            VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = initVulkanStructure();
            blasAddressInfo.accelerationStructure                       = *m_rtBlas->getPtr();

            VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo = initVulkanStructure();
            tlasAddressInfo.accelerationStructure                       = *m_rtTlas->getPtr();

            m_captureBlasAddress = vkd.getAccelerationStructureDeviceAddressKHR(*m_device, &blasAddressInfo);
            m_captureTlasAddress = vkd.getAccelerationStructureDeviceAddressKHR(*m_device, &tlasAddressInfo);
        }
        break;
    }
    default:
        break;
    }

    return VK_SUCCESS;
}

VkResult DescriptorHeapTestInstanceInvariance::writeInvarianceDescriptor(std::vector<char> &descriptorData)
{
    auto &vk = m_device.getDriver();

    VkHostAddressRangeEXT hostAddressRange{};
    hostAddressRange.address = descriptorData.data();
    hostAddressRange.size    = descriptorData.size();

    switch (m_params.descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
        VkSamplerYcbcrConversionInfo ycbcrConversionInfo = initVulkanStructure();
        ycbcrConversionInfo.conversion                   = *m_samplerYcbcrConversion;

        VkSamplerCustomBorderColorCreateInfoEXT customBorderColorCreateInfo = initVulkanStructure();
        customBorderColorCreateInfo.customBorderColor                       = m_customBorderColor;
        customBorderColorCreateInfo.format                                  = VK_FORMAT_R32G32B32A32_SFLOAT;

        VkSamplerCustomBorderColorIndexCreateInfoEXT customBorderColorIndexCreateInfo = initVulkanStructure();
        customBorderColorIndexCreateInfo.pNext                                        = &customBorderColorCreateInfo;
        customBorderColorIndexCreateInfo.index                                        = m_customBorderColorIndex;

        VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
        samplerCreateInfo.flags                   = 0;
        samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.mipLodBias              = 0.0f;
        samplerCreateInfo.anisotropyEnable        = VK_FALSE;
        samplerCreateInfo.maxAnisotropy           = 0.0f;
        samplerCreateInfo.compareEnable           = VK_FALSE;
        samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod                  = 0.0f;
        samplerCreateInfo.maxLod                  = 1000.0f;
        samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

        const void **pNext = &samplerCreateInfo.pNext;

        if (m_params.enableSamplerYcbcrConversion)
        {
            *pNext = &ycbcrConversionInfo;
            pNext  = &ycbcrConversionInfo.pNext;
        }

        if (m_params.enableCustomBorderColor)
        {
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
            *pNext                        = &customBorderColorIndexCreateInfo;
            pNext                         = &customBorderColorIndexCreateInfo.pNext;
        }

        return vk.writeSamplerDescriptorsEXT(*m_device, 1, &samplerCreateInfo, &hostAddressRange);
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {
        VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
        imageViewCreateInfo.flags                           = 0;
        imageViewCreateInfo.image                           = m_image ? *m_image : VK_NULL_HANDLE;
        imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format                          = VK_FORMAT_R8_UNORM;
        imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = 1;

        VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
        resourceInfo.type                        = m_params.descriptorType;

        VkTexelBufferDescriptorInfoEXT texelBufferDescriptorInfo = initVulkanStructure();
        texelBufferDescriptorInfo.format                         = VK_FORMAT_R8_UNORM;
        texelBufferDescriptorInfo.addressRange.address           = m_buffer ? m_buffer->address : 0;
        texelBufferDescriptorInfo.addressRange.size              = m_buffer && m_buffer->address ? 256 : 0;

        VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
        imageDescriptorInfo.pView                    = &imageViewCreateInfo;
        imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;

        if ((m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) ||
            (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
        {
            resourceInfo.data.pTexelBuffer = &texelBufferDescriptorInfo;
        }
        else
        {
            resourceInfo.data.pImage = &imageDescriptorInfo;
        }

        return vk.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostAddressRange);
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
        VkDeviceAddressRangeEXT deviceAddressRange{};
        if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            deviceAddressRange.address = getAccelerationStructureDeviceAddress(vk, *m_device, *m_rtTlas->getPtr());
            deviceAddressRange.size    = 0;
        }
        else
        {
            deviceAddressRange.address = m_buffer->address;
            deviceAddressRange.size    = m_buffer->address ? 256 : 0;
        }

        VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
        resourceInfo.type                        = m_params.descriptorType;
        resourceInfo.data.pAddressRange          = &deviceAddressRange;

        return vk.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostAddressRange);
    }
    default:
        DE_ASSERT(0);
        return VK_ERROR_UNKNOWN;
    }
}

void DescriptorHeapTestCaseReservedHeap::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string atomicCounterShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0, r32ui) uniform uimageBuffer imgBuffer;
layout(binding = 0, std140) uniform P
{
    uvec2 data;
};
void main()
{
    imageAtomicAdd(imgBuffer, int(data[0]), data[1]);
}
)";
    programCollection.glslSources.add("atomic_counter") << glu::ComputeSource(atomicCounterShader);
}

DescriptorHeapTestInstanceReservedHeap::DescriptorHeapTestInstanceReservedHeap(Context &context,
                                                                               const TestParamsReservedHeap &params)
    : TestInstance(context)
    , m_params{params}
    , m_instance(context)
    , m_rnd(params.seed)
{
    auto &inst      = m_instance.getDriver();
    auto physDevice = m_instance.getPhysicalDevice();
    auto queueProps = getPhysicalDeviceQueueFamilyProperties(inst, physDevice);

    for (uint32_t i = 0; i < queueProps.size(); ++i)
    {
        if ((queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
            continue;

        if (m_params.copyMethod == CopyMethod::BlitImage)
        {
            if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                continue;
        }

        m_queueFamilies.push_back(i);
        m_queueCounts.push_back(queueProps[i].queueCount);
    }

    if (m_queueFamilies.empty())
    {
        TCU_THROW(NotSupportedError, "No compatible queues available");
    }

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<std::vector<float>> priorities;

    for (size_t i = 0; i < m_queueFamilies.size(); ++i)
    {
        auto &queueInfo            = queueInfos.emplace_back(initVulkanStructure());
        queueInfo.queueFamilyIndex = m_queueFamilies[i];
        queueInfo.queueCount       = m_queueCounts[i];
        queueInfo.pQueuePriorities = priorities.emplace_back(m_queueCounts[i], 0.5f).data();
    }

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();

    VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures = initVulkanStructure();
    descriptorHeapFeatures.descriptorHeap                            = VK_TRUE;
    descriptorHeapFeatures.descriptorHeapCaptureReplay               = VK_FALSE;

    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features = initVulkanStructure();
    maintenance5Features.maintenance5                            = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures = initVulkanStructure();
    bufferDeviceAddressFeatures.bufferDeviceAddress                            = VK_TRUE;

    VkPhysicalDeviceSynchronization2Features synchronization2Features = initVulkanStructure();
    synchronization2Features.synchronization2                         = VK_TRUE;

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures = initVulkanStructure();
    timelineSemaphoreFeatures.timelineSemaphore                         = VK_TRUE;

    void **nextPtr = &features2.pNext;
    addToChainVulkanStructure(&nextPtr, descriptorHeapFeatures);
    addToChainVulkanStructure(&nextPtr, maintenance5Features);
    addToChainVulkanStructure(&nextPtr, bufferDeviceAddressFeatures);
    addToChainVulkanStructure(&nextPtr, synchronization2Features);
    addToChainVulkanStructure(&nextPtr, timelineSemaphoreFeatures);

    std::vector<const char *> extensions = {
        VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,   VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_5_EXTENSION_NAME,     VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo      = initVulkanStructure(&features2);
    createInfo.pEnabledFeatures        = nullptr;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();

    m_device = m_instance.createCustomDevice(physDevice, &createInfo);

    m_memoryProperties = vk::getPhysicalDeviceMemoryProperties(inst, physDevice);
    m_descriptorHeapProperties =
        *findStructure<VkPhysicalDeviceDescriptorHeapPropertiesEXT>(&context.getDeviceProperties2());

    for (size_t i = 0; i < m_queueFamilies.size(); ++i)
    {
        for (uint32_t j = 0; j < m_queueCounts[i]; ++j)
        {
            VkQueue queue = VK_NULL_HANDLE;
            m_device.getDriver().getDeviceQueue(*m_device, m_queueFamilies[i], j, &queue);
            m_queues.push_back(queue);
        }
    }

    m_physDevice = physDevice;
}

tcu::TestStatus DescriptorHeapTestInstanceReservedHeap::iterate()
{
    if ((m_descriptorHeapProperties.minResourceHeapReservedRange == 0) &&
        (m_descriptorHeapProperties.minSamplerHeapReservedRange == 0) &&
        (m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded == 0))
    {
        // Implementations without reserved heap ranges are not required to run this test.
        return tcu::TestStatus::pass("Heap reserved ranges are zero");
    }

    const InstanceInterface &vki = m_instance.getDriver();
    const DeviceInterface &vk    = m_device.getDriver();

    auto shaderModule = createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("atomic_counter"));

    std::array<VkDescriptorSetAndBindingMappingEXT, 2> mappings{};
    mappings[0]                                            = initVulkanStructure();
    mappings[0].descriptorSet                              = 0;
    mappings[0].firstBinding                               = 0;
    mappings[0].bindingCount                               = 1;
    mappings[0].resourceMask                               = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
    mappings[0].source                                     = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[0].sourceData.constantOffset.heapOffset       = 0;
    mappings[0].sourceData.constantOffset.heapArrayStride  = 0;
    mappings[0].sourceData.constantOffset.pEmbeddedSampler = nullptr;
    mappings[0].sourceData.constantOffset.samplerHeapOffset      = 0;
    mappings[0].sourceData.constantOffset.samplerHeapArrayStride = 0;
    mappings[1]                                                  = initVulkanStructure();
    mappings[1].descriptorSet                                    = 0;
    mappings[1].firstBinding                                     = 0;
    mappings[1].bindingCount                                     = 1;
    mappings[1].resourceMask                                     = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
    mappings[1].source                                           = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
    mappings[1].sourceData.pushDataOffset                        = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = de::sizeU32(mappings);
    mappingInfo.pMappings                                     = mappings.data();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo atomicCounterPipelineCreateInfo = initVulkanStructure();
    atomicCounterPipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    atomicCounterPipelineCreateInfo.flags                       = 0;
    atomicCounterPipelineCreateInfo.stage                       = initVulkanStructure();
    atomicCounterPipelineCreateInfo.stage.pNext                 = &mappingInfo;
    atomicCounterPipelineCreateInfo.stage.flags                 = 0;
    atomicCounterPipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    atomicCounterPipelineCreateInfo.stage.module                = *shaderModule;
    atomicCounterPipelineCreateInfo.stage.pName                 = "main";
    atomicCounterPipelineCreateInfo.stage.pSpecializationInfo   = nullptr;
    atomicCounterPipelineCreateInfo.layout                      = VK_NULL_HANDLE;
    atomicCounterPipelineCreateInfo.basePipelineHandle          = VK_NULL_HANDLE;
    atomicCounterPipelineCreateInfo.basePipelineIndex           = 0;

    m_atomicCounterPipeline = createComputePipeline(vk, *m_device, VK_NULL_HANDLE, &atomicCounterPipelineCreateInfo);

    const VkDeviceSize atomicCounterBufferSize = m_queues.size() * sizeof(uint32_t);
    auto atomicCounterBuffer =
        createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, atomicCounterBufferSize,
                              VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    deMemset(atomicCounterBuffer->memory->getHostPtr(), 0, static_cast<size_t>(atomicCounterBufferSize));

    const uint32_t imageExtent = m_params.imageExtent;

    VkImageCreateInfo dstImageCreateInfo     = initVulkanStructure();
    dstImageCreateInfo.flags                 = 0;
    dstImageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
    dstImageCreateInfo.format                = VK_FORMAT_R8_UINT;
    dstImageCreateInfo.extent                = {imageExtent, imageExtent, 1};
    dstImageCreateInfo.mipLevels             = 1;
    dstImageCreateInfo.arrayLayers           = static_cast<uint32_t>(m_queues.size());
    dstImageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    dstImageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    dstImageCreateInfo.usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    dstImageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    dstImageCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(m_queueFamilies.size());
    dstImageCreateInfo.pQueueFamilyIndices   = m_queueFamilies.data();
    dstImageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    m_dstImage = createImageAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, dstImageCreateInfo);

    const VkDeviceSize dstBufferSize = VkDeviceSize{imageExtent * imageExtent} * m_queues.size() * sizeof(uint8_t);
    auto dstBuffer = createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, dstBufferSize,
                                           VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR);
    m_srcBuffer =
        createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, dstBufferSize,
                              VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR);
    m_expectedResult.resize(static_cast<size_t>(dstBufferSize));

    const VkDeviceSize userResourceHeapSize = alignUp(16 * getImageDescriptorStride(m_descriptorHeapProperties),
                                                      m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        userResourceHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto resourceDescriptorHeapBuffer = createBufferAndMemory(
        vki, m_physDevice, vk, m_memoryProperties, *m_device, resourceHeapSize,
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT);

    m_resourceDescriptorHeap                     = initVulkanStructure();
    m_resourceDescriptorHeap.heapRange.address   = resourceDescriptorHeapBuffer->address;
    m_resourceDescriptorHeap.heapRange.size      = resourceHeapSize;
    m_resourceDescriptorHeap.reservedRangeOffset = userResourceHeapSize;
    m_resourceDescriptorHeap.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    const VkDeviceSize dummySamplerDescriptorsSize = alignUp(
        16 * getSamplerDescriptorStride(m_descriptorHeapProperties), m_descriptorHeapProperties.samplerHeapAlignment);
    const VkDeviceSize samplerDescriptorHeapBufferSize =
        dummySamplerDescriptorsSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;
    auto samplerDescriptorHeapBuffer = createBufferAndMemory(
        vki, m_physDevice, vk, m_memoryProperties, *m_device, samplerDescriptorHeapBufferSize,
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT);

    m_samplerDescriptorHeap                     = initVulkanStructure();
    m_samplerDescriptorHeap.heapRange.address   = samplerDescriptorHeapBuffer->address;
    m_samplerDescriptorHeap.heapRange.size      = samplerDescriptorHeapBufferSize;
    m_samplerDescriptorHeap.reservedRangeOffset = dummySamplerDescriptorsSize;
    m_samplerDescriptorHeap.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;

    // Fill application region of the image and sampler heaps with garbage.
    deMemset(resourceDescriptorHeapBuffer->memory->getHostPtr(), 0xcc, static_cast<size_t>(userResourceHeapSize));
    deMemset(samplerDescriptorHeapBuffer->memory->getHostPtr(), 0xcc, static_cast<size_t>(dummySamplerDescriptorsSize));

    // Write atomic buffer descriptor
    {
        VkTexelBufferDescriptorInfoEXT texelBufferDescriptorInfo = initVulkanStructure();
        texelBufferDescriptorInfo.format                         = VK_FORMAT_R32_UINT;
        texelBufferDescriptorInfo.addressRange.address           = atomicCounterBuffer->address;
        texelBufferDescriptorInfo.addressRange.size              = atomicCounterBufferSize;

        VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
        resourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        resourceInfo.data.pTexelBuffer           = &texelBufferDescriptorInfo;

        VkHostAddressRangeEXT hostAddressRange{};
        hostAddressRange.address = resourceDescriptorHeapBuffer->memory->getHostPtr();
        hostAddressRange.size    = static_cast<size_t>(m_descriptorHeapProperties.imageDescriptorSize);
        VK_CHECK(vk.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostAddressRange));
    }

    std::unique_ptr<Buffer> colorBuffer;

    if ((m_params.copyMethod == CopyMethod::CopyImage) || (m_params.copyMethod == CopyMethod::BlitImage))
    {
        VkImageCreateInfo colorImageCreateInfo = initVulkanStructure();
        colorImageCreateInfo.flags             = 0;
        colorImageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
        colorImageCreateInfo.format            = VK_FORMAT_R8_UINT;
        colorImageCreateInfo.extent            = {256, 1, 1};
        colorImageCreateInfo.mipLevels         = 1;
        colorImageCreateInfo.arrayLayers       = 1;
        colorImageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        colorImageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        colorImageCreateInfo.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        colorImageCreateInfo.sharingMode =
            m_queueFamilies.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        colorImageCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(m_queueFamilies.size());
        colorImageCreateInfo.pQueueFamilyIndices   = m_queueFamilies.data();
        colorImageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        m_colorImage = createImageAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, colorImageCreateInfo);

        colorBuffer = createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, 256,
                                            VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR);

        uint8_t *const colorBufferPtr = reinterpret_cast<uint8_t *>(colorBuffer->memory->getHostPtr());
        for (int i = 0; i < 256; ++i)
        {
            colorBufferPtr[i] = static_cast<uint8_t>(i);
        }
    }

    auto genericCmdPool = makeCommandPool(vk, *m_device, m_queueFamilies.front());
    auto setupCmdBuf    = allocateCommandBuffer(vk, *m_device, *genericCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto readCmdBuf     = allocateCommandBuffer(vk, *m_device, *genericCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Setup destination image.
    {
        const VkCommandBuffer cmdBuf = *setupCmdBuf;
        beginCommandBuffer(vk, cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        std::vector<VkImageMemoryBarrier2> preBarriers;
        VkImageMemoryBarrier2 &preBarrier          = preBarriers.emplace_back(initVulkanStructure());
        preBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
        preBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
        preBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        preBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        preBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        preBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        preBarrier.srcQueueFamilyIndex             = m_queueFamilies.front();
        preBarrier.dstQueueFamilyIndex             = m_queueFamilies.front();
        preBarrier.image                           = *m_dstImage->image;
        preBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        preBarrier.subresourceRange.baseMipLevel   = 0;
        preBarrier.subresourceRange.levelCount     = 1;
        preBarrier.subresourceRange.baseArrayLayer = 0;
        preBarrier.subresourceRange.layerCount     = static_cast<uint32_t>(m_queues.size());

        std::vector<VkImageMemoryBarrier2> postBarriers(m_queueFamilies.size());
        uint32_t baseLayer = 0;
        for (size_t i = 0; i < m_queueFamilies.size(); ++i)
        {
            auto &postBarrier                           = postBarriers[i];
            postBarrier                                 = initVulkanStructure();
            postBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            postBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            postBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            postBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            postBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            postBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            postBarrier.srcQueueFamilyIndex             = m_queueFamilies.front();
            postBarrier.dstQueueFamilyIndex             = m_queueFamilies[i];
            postBarrier.image                           = *m_dstImage->image;
            postBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            postBarrier.subresourceRange.baseMipLevel   = 0;
            postBarrier.subresourceRange.levelCount     = 1;
            postBarrier.subresourceRange.baseArrayLayer = baseLayer;
            postBarrier.subresourceRange.layerCount     = m_queueCounts[i];
            baseLayer += m_queueCounts[i];
        }

        if (m_colorImage && m_colorImage->image)
        {
            VkImageMemoryBarrier2 &preColorImageBarrier          = preBarriers.emplace_back(initVulkanStructure());
            preColorImageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
            preColorImageBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
            preColorImageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            preColorImageBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            preColorImageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
            preColorImageBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            preColorImageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            preColorImageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            preColorImageBarrier.image                           = *m_colorImage->image;
            preColorImageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            preColorImageBarrier.subresourceRange.baseMipLevel   = 0;
            preColorImageBarrier.subresourceRange.levelCount     = 1;
            preColorImageBarrier.subresourceRange.baseArrayLayer = 0;
            preColorImageBarrier.subresourceRange.layerCount     = 1;

            VkImageMemoryBarrier2 &postColorImageBarrier          = postBarriers.emplace_back(initVulkanStructure());
            postColorImageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            postColorImageBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            postColorImageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            postColorImageBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_READ_BIT;
            postColorImageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            postColorImageBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
            postColorImageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            postColorImageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            postColorImageBarrier.image                           = *m_colorImage->image;
            postColorImageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            postColorImageBarrier.subresourceRange.baseMipLevel   = 0;
            postColorImageBarrier.subresourceRange.levelCount     = 1;
            postColorImageBarrier.subresourceRange.baseArrayLayer = 0;
            postColorImageBarrier.subresourceRange.layerCount     = 1;
        }

        VkClearColorValue clearColor{};
        clearColor.uint32[0] = 0xcc;

        VkImageSubresourceRange clearRange{};
        clearRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        clearRange.baseMipLevel   = 0;
        clearRange.levelCount     = 1;
        clearRange.baseArrayLayer = 0;
        clearRange.layerCount     = static_cast<uint32_t>(m_queues.size());

        VkDependencyInfo preDependencyInfo        = initVulkanStructure();
        preDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(preBarriers.size());
        preDependencyInfo.pImageMemoryBarriers    = preBarriers.data();

        VkDependencyInfo postDependencyInfo        = initVulkanStructure();
        postDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(postBarriers.size());
        postDependencyInfo.pImageMemoryBarriers    = postBarriers.data();

        vk.cmdPipelineBarrier2(cmdBuf, &preDependencyInfo);

        vk.cmdClearColorImage(cmdBuf, *m_dstImage->image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &clearRange);

        if (m_colorImage && m_colorImage->image)
        {
            VkBufferImageCopy copyRegion{};
            copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel       = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount     = 1;
            copyRegion.imageExtent                     = {256, 1, 1};
            vk.cmdCopyBufferToImage(cmdBuf, *colorBuffer->buffer, *m_colorImage->image, VK_IMAGE_LAYOUT_GENERAL, 1,
                                    &copyRegion);
        }

        vk.cmdPipelineBarrier2(cmdBuf, &postDependencyInfo);

        endCommandBuffer(vk, cmdBuf);
    }

    // Synchronize and read contents.
    {
        const VkCommandBuffer cmdBuf = *readCmdBuf;
        beginCommandBuffer(vk, cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkBufferMemoryBarrier2 postBarrier = initVulkanStructure();
        postBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        postBarrier.srcAccessMask          = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        postBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
        postBarrier.dstAccessMask          = VK_ACCESS_2_HOST_READ_BIT;
        postBarrier.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.buffer                 = *dstBuffer->buffer;
        postBarrier.offset                 = 0;
        postBarrier.size                   = dstBufferSize;

        VkDependencyInfo postDependencyInfo         = initVulkanStructure();
        postDependencyInfo.bufferMemoryBarrierCount = 1;
        postDependencyInfo.pBufferMemoryBarriers    = &postBarrier;

        VkBufferImageCopy readbackRegion{};
        readbackRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        readbackRegion.imageSubresource.mipLevel       = 0;
        readbackRegion.imageSubresource.baseArrayLayer = 0;
        readbackRegion.imageSubresource.layerCount     = static_cast<uint32_t>(m_queues.size());
        readbackRegion.imageExtent                     = {imageExtent, imageExtent, 1};

        vk.cmdCopyImageToBuffer(cmdBuf, *m_dstImage->image, VK_IMAGE_LAYOUT_GENERAL, *dstBuffer->buffer, 1,
                                &readbackRegion);
        vk.cmdPipelineBarrier2(cmdBuf, &postDependencyInfo);

        endCommandBuffer(vk, cmdBuf);
    }

    for (size_t i = 0; i < m_queueFamilies.size(); ++i)
    {
        const VkCommandPool commandPool =
            m_commandPools.emplace_back(makeCommandPool(vk, *m_device, m_queueFamilies[i])).get();

        VkCommandBufferAllocateInfo allocateInfo = initVulkanStructure();
        allocateInfo.commandPool                 = commandPool;
        allocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount          = 1;

        for (uint32_t j = 0; j < m_queueCounts[i]; ++j)
        {
            auto cmdBuf = allocateCommandBuffer(vk, *m_device, &allocateInfo);

            const uint32_t globalQueueIndex = static_cast<uint32_t>(m_queueCommandBuffers.size());
            recordQueueCommandBuffer(*cmdBuf, globalQueueIndex, m_queueFamilies[i]);
            m_queueCommandBuffers.push_back(std::move(cmdBuf));
        }
    }

    VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = initVulkanStructure();
    semaphoreTypeCreateInfo.initialValue              = 0;
    semaphoreTypeCreateInfo.semaphoreType             = VK_SEMAPHORE_TYPE_TIMELINE;

    VkSemaphoreCreateInfo semaphoreCreateInfo = initVulkanStructure();
    semaphoreCreateInfo.pNext                 = &semaphoreTypeCreateInfo;

    auto setupSemaphore = createSemaphore(vk, *m_device, &semaphoreCreateInfo);

    std::vector<Move<VkSemaphore>> queueSemaphores;
    for (size_t i = 0; i < m_queues.size(); ++i)
    {
        queueSemaphores.push_back(createSemaphore(vk, *m_device, &semaphoreCreateInfo));
    }

    std::list<VkSemaphoreSubmitInfo> semaphoreSubmitInfos;
    std::list<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;

    std::vector<VkSubmitInfo2> queueSubmits;
    for (size_t i = 0; i < m_queues.size(); ++i)
    {
        auto &waitSemaphoreInfo     = semaphoreSubmitInfos.emplace_back(initVulkanStructure());
        waitSemaphoreInfo.semaphore = setupSemaphore.get();
        waitSemaphoreInfo.value     = 1;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        auto &signalSemaphoreInfo     = semaphoreSubmitInfos.emplace_back(initVulkanStructure());
        signalSemaphoreInfo.semaphore = queueSemaphores[i].get();
        signalSemaphoreInfo.value     = 1;
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        auto &cmdInfo         = commandBufferSubmitInfos.emplace_back(initVulkanStructure());
        cmdInfo.commandBuffer = m_queueCommandBuffers[i].get();

        VkSubmitInfo2 &queueSubmitInfo           = queueSubmits.emplace_back(initVulkanStructure());
        queueSubmitInfo.waitSemaphoreInfoCount   = 1;
        queueSubmitInfo.pWaitSemaphoreInfos      = &waitSemaphoreInfo;
        queueSubmitInfo.commandBufferInfoCount   = 1;
        queueSubmitInfo.pCommandBufferInfos      = &cmdInfo;
        queueSubmitInfo.signalSemaphoreInfoCount = 1;
        queueSubmitInfo.pSignalSemaphoreInfos    = &signalSemaphoreInfo;
    }

    VkCommandBufferSubmitInfo setupCmdInfo = initVulkanStructure();
    setupCmdInfo.commandBuffer             = setupCmdBuf.get();

    VkSemaphoreSubmitInfo setupSemaphoreSignal = initVulkanStructure();
    setupSemaphoreSignal.semaphore             = setupSemaphore.get();
    setupSemaphoreSignal.value                 = 1;
    setupSemaphoreSignal.stageMask             = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 setupSubmitInfo            = initVulkanStructure();
    setupSubmitInfo.commandBufferInfoCount   = 1;
    setupSubmitInfo.pCommandBufferInfos      = &setupCmdInfo;
    setupSubmitInfo.signalSemaphoreInfoCount = 1;
    setupSubmitInfo.pSignalSemaphoreInfos    = &setupSemaphoreSignal;

    VkCommandBufferSubmitInfo readCmdInfo = initVulkanStructure();
    readCmdInfo.commandBuffer             = readCmdBuf.get();

    std::vector<VkSemaphoreSubmitInfo> readQueuesSemaphoreSubmitInfos;
    for (size_t i = 0; i < m_queues.size(); ++i)
    {
        auto &waitSemaphoreInfo     = readQueuesSemaphoreSubmitInfos.emplace_back(initVulkanStructure());
        waitSemaphoreInfo.semaphore = queueSemaphores[i].get();
        waitSemaphoreInfo.value     = 1;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

    VkSubmitInfo2 readSubmitInfo          = initVulkanStructure();
    readSubmitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(readQueuesSemaphoreSubmitInfos.size());
    readSubmitInfo.pWaitSemaphoreInfos    = readQueuesSemaphoreSubmitInfos.data();
    readSubmitInfo.commandBufferInfoCount = 1;
    readSubmitInfo.pCommandBufferInfos    = &readCmdInfo;

    VK_CHECK(vk.queueSubmit2(m_queues.front(), 1, &setupSubmitInfo, VK_NULL_HANDLE));

    for (size_t i = 0; i < m_queues.size(); ++i)
    {
        VK_CHECK(vk.queueSubmit2(m_queues[i], 1, &queueSubmits[i], VK_NULL_HANDLE));
    }

    VK_CHECK(vk.queueSubmit2(m_queues.front(), 1, &readSubmitInfo, VK_NULL_HANDLE));

    VK_CHECK(vk.deviceWaitIdle(*m_device));

    // The atomic counter is only used when resource heaps are bound.
    if (m_params.bindResourceHeap)
    {
        const auto atomicCounterData = static_cast<const uint8_t *>(atomicCounterBuffer->memory->getHostPtr());
        for (size_t queue = 0; queue < m_queues.size(); ++queue)
        {
            uint32_t atomicCounterValue = 0;
            deMemcpy(&atomicCounterValue, atomicCounterData + queue * sizeof(uint32_t), sizeof(uint32_t));

            if (atomicCounterValue != 0x3)
            {
                std::stringstream stream;
                stream << "Atomic counter value for queue " << queue << " is 0x" << std::hex << atomicCounterValue
                       << ", expected 0x3";
                return tcu::TestStatus::fail(stream.str());
            }
        }
    }

    const auto result = static_cast<const uint8_t *>(dstBuffer->memory->getHostPtr());
    for (size_t i = 0; i < m_expectedResult.size(); ++i)
    {
        if (result[i] != m_expectedResult[i])
        {
            std::stringstream stream;
            stream << "At index " << i << ", expected 0x" << std::hex << int{m_expectedResult[i]} << " but got 0x"
                   << int{result[i]};
            return tcu::TestStatus::fail(stream.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void DescriptorHeapTestInstanceReservedHeap::recordQueueCommandBuffer(VkCommandBuffer cmdBuf, uint32_t globalQueueIndex,
                                                                      uint32_t queueFamily)
{
    const auto &vki = m_instance.getDriver();
    const auto &vk  = m_device.getDriver();

    const uint32_t area = m_params.imageExtent * m_params.imageExtent;

    const std::array<uint32_t, 2> pushData1 = {globalQueueIndex, 1};
    const std::array<uint32_t, 1> pushData2 = {2};

    VkPushDataInfoEXT pushDataInfo1 = initVulkanStructure();
    pushDataInfo1.offset            = 0;
    pushDataInfo1.data.address      = &pushData1;
    pushDataInfo1.data.size         = sizeof(pushData1);

    VkPushDataInfoEXT pushDataInfo2 = initVulkanStructure();
    pushDataInfo2.offset            = sizeof(uint32_t);
    pushDataInfo2.data.address      = &pushData2;
    pushDataInfo2.data.size         = sizeof(pushData2);

    VkMemoryBarrier2 atomicMemoryBarrier = initVulkanStructure();
    atomicMemoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    atomicMemoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    atomicMemoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT;
    atomicMemoryBarrier.dstAccessMask =
        VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_HOST_READ_BIT;

    VkImageMemoryBarrier2 dstImageBarrier           = initVulkanStructure();
    dstImageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    dstImageBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstImageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    dstImageBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_READ_BIT;
    dstImageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    dstImageBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    dstImageBarrier.srcQueueFamilyIndex             = queueFamily;
    dstImageBarrier.dstQueueFamilyIndex             = m_queueFamilies.front();
    dstImageBarrier.image                           = *m_dstImage->image;
    dstImageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    dstImageBarrier.subresourceRange.baseArrayLayer = globalQueueIndex;
    dstImageBarrier.subresourceRange.baseMipLevel   = 0;
    dstImageBarrier.subresourceRange.levelCount     = 1;
    dstImageBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo preDependencyInfo   = initVulkanStructure();
    preDependencyInfo.memoryBarrierCount = 1;
    preDependencyInfo.pMemoryBarriers    = &atomicMemoryBarrier;

    VkDependencyInfo postDependencyInfo        = initVulkanStructure();
    postDependencyInfo.memoryBarrierCount      = 1;
    postDependencyInfo.pMemoryBarriers         = &atomicMemoryBarrier;
    postDependencyInfo.imageMemoryBarrierCount = 1;
    postDependencyInfo.pImageMemoryBarriers    = &dstImageBarrier;

    beginCommandBuffer(vk, cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (m_params.bindResourceHeap)
    {
        vk.cmdBindResourceHeapEXT(cmdBuf, &m_resourceDescriptorHeap);
    }
    if (m_params.bindSamplerHeap)
    {
        vk.cmdBindSamplerHeapEXT(cmdBuf, &m_samplerDescriptorHeap);
    }

    vk.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_atomicCounterPipeline);
    vk.cmdPushDataEXT(cmdBuf, &pushDataInfo1);

    // Only do the atomic counting if we are using a resource heap.
    if (m_params.bindResourceHeap)
        vk.cmdDispatch(cmdBuf, 1, 1, 1);
    vk.cmdPipelineBarrier2(cmdBuf, &preDependencyInfo);

    switch (m_params.copyMethod)
    {
    case CopyMethod::BufferToImage:
    {
        std::vector<VkBufferImageCopy> copyRegions;
        for (uint32_t i = 0; i < area; ++i)
        {
            const int32_t y = i / m_params.imageExtent;
            const int32_t x = i % m_params.imageExtent;

            VkBufferImageCopy &copyRegion              = copyRegions.emplace_back();
            copyRegion.bufferOffset                    = (globalQueueIndex * area + i) * sizeof(uint8_t);
            copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel       = 0;
            copyRegion.imageSubresource.baseArrayLayer = globalQueueIndex;
            copyRegion.imageSubresource.layerCount     = 1;
            copyRegion.imageOffset                     = {x, y, 0};
            copyRegion.imageExtent                     = {1, 1, 1};

            const int setValue = m_rnd.getUint32() & 0xFF;
            deMemset(&m_expectedResult[static_cast<size_t>(copyRegion.bufferOffset)], setValue, sizeof(uint8_t));
            deMemset(reinterpret_cast<char *>(m_srcBuffer->memory->getHostPtr()) + copyRegion.bufferOffset, setValue,
                     sizeof(uint8_t));
        }

        vk.cmdCopyBufferToImage(cmdBuf, *m_srcBuffer->buffer, *m_dstImage->image, VK_IMAGE_LAYOUT_GENERAL,
                                static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
        break;
    }
    case CopyMethod::CopyImage:
    {
        std::vector<VkImageCopy> copyRegions;
        for (uint32_t i = 0; i < area; ++i)
        {
            const int32_t y                               = i / m_params.imageExtent;
            const int32_t x                               = i % m_params.imageExtent;
            const int setValue                            = m_rnd.getUint32() & 0xFF;
            m_expectedResult[globalQueueIndex * area + i] = static_cast<uint8_t>(setValue);

            VkImageCopy &copyRegion                  = copyRegions.emplace_back();
            copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel       = 0;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount     = 1;
            copyRegion.srcOffset                     = {setValue, 0, 0};
            copyRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.mipLevel       = 0;
            copyRegion.dstSubresource.baseArrayLayer = globalQueueIndex;
            copyRegion.dstSubresource.layerCount     = 1;
            copyRegion.dstOffset                     = {x, y, 0};
            copyRegion.extent                        = {1, 1, 1};
        }

        vk.cmdCopyImage(cmdBuf, *m_colorImage->image, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage->image,
                        VK_IMAGE_LAYOUT_GENERAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
        break;
    }
    case CopyMethod::ImageToBuffer:
    {
        VkImageCreateInfo stagingImageCreateInfo = initVulkanStructure();
        stagingImageCreateInfo.flags             = 0;
        stagingImageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
        stagingImageCreateInfo.format            = VK_FORMAT_R8_UINT;
        stagingImageCreateInfo.extent            = {m_params.imageExtent, m_params.imageExtent, 1};
        stagingImageCreateInfo.mipLevels         = 1;
        stagingImageCreateInfo.arrayLayers       = 1;
        stagingImageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        stagingImageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        stagingImageCreateInfo.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        stagingImageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        stagingImageCreateInfo.queueFamilyIndexCount = 0;
        stagingImageCreateInfo.pQueueFamilyIndices   = nullptr;
        stagingImageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        auto &interImage                             = m_stagingImages.emplace_back(
            createImageAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, stagingImageCreateInfo));

        std::vector<uint8_t> contents(area);
        for (uint8_t &texel : contents)
        {
            texel = m_rnd.getUint8();
        }

        const auto usageBits = VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR;
        auto &srcBuffer      = m_stagingBuffers.emplace_back(
            createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, area, usageBits));
        auto &interBuffer = m_stagingBuffers.emplace_back(
            createBufferAndMemory(vki, m_physDevice, vk, m_memoryProperties, *m_device, area, usageBits));

        deMemcpy(srcBuffer->memory->getHostPtr(), contents.data(), contents.size());
        deMemcpy(&m_expectedResult[area * globalQueueIndex], contents.data(), contents.size());

        VkBufferImageCopy initCopyRegion{};
        initCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        initCopyRegion.imageSubresource.mipLevel       = 0;
        initCopyRegion.imageSubresource.baseArrayLayer = 0;
        initCopyRegion.imageSubresource.layerCount     = 1;
        initCopyRegion.imageExtent                     = {m_params.imageExtent, m_params.imageExtent, 1};

        VkBufferImageCopy outCopyRegion{};
        outCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        outCopyRegion.imageSubresource.mipLevel       = 0;
        outCopyRegion.imageSubresource.baseArrayLayer = globalQueueIndex;
        outCopyRegion.imageSubresource.layerCount     = 1;
        outCopyRegion.imageExtent                     = {m_params.imageExtent, m_params.imageExtent, 1};

        VkImageMemoryBarrier2 preImageBarrier           = initVulkanStructure();
        preImageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
        preImageBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
        preImageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        preImageBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        preImageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        preImageBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        preImageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        preImageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        preImageBarrier.image                           = *interImage->image;
        preImageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        preImageBarrier.subresourceRange.baseMipLevel   = 0;
        preImageBarrier.subresourceRange.levelCount     = 1;
        preImageBarrier.subresourceRange.baseArrayLayer = 0;
        preImageBarrier.subresourceRange.layerCount     = 1;

        VkImageMemoryBarrier2 interImageBarrier           = initVulkanStructure();
        interImageBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        interImageBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        interImageBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        interImageBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_READ_BIT;
        interImageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        interImageBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        interImageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        interImageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        interImageBarrier.image                           = *interImage->image;
        interImageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        interImageBarrier.subresourceRange.baseMipLevel   = 0;
        interImageBarrier.subresourceRange.levelCount     = 1;
        interImageBarrier.subresourceRange.baseArrayLayer = 0;
        interImageBarrier.subresourceRange.layerCount     = 1;

        VkBufferMemoryBarrier2 midBufferBarrier = initVulkanStructure();
        midBufferBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        midBufferBarrier.srcAccessMask          = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        midBufferBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        midBufferBarrier.dstAccessMask          = VK_ACCESS_2_TRANSFER_READ_BIT;
        midBufferBarrier.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
        midBufferBarrier.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
        midBufferBarrier.buffer                 = *interBuffer->buffer;
        midBufferBarrier.offset                 = 0;
        midBufferBarrier.size                   = area;

        VkDependencyInfo imagePreDependencyInfo        = initVulkanStructure();
        imagePreDependencyInfo.imageMemoryBarrierCount = 1;
        imagePreDependencyInfo.pImageMemoryBarriers    = &preImageBarrier;

        VkDependencyInfo interDependencyInfo        = initVulkanStructure();
        interDependencyInfo.imageMemoryBarrierCount = 1;
        interDependencyInfo.pImageMemoryBarriers    = &interImageBarrier;

        VkDependencyInfo bufferPostDependencyInfo         = initVulkanStructure();
        bufferPostDependencyInfo.bufferMemoryBarrierCount = 1;
        bufferPostDependencyInfo.pBufferMemoryBarriers    = &midBufferBarrier;

        std::vector<VkBufferImageCopy> copyRegions;
        for (uint32_t i = 0; i < area; ++i)
        {
            const int32_t y = i / m_params.imageExtent;
            const int32_t x = i % m_params.imageExtent;

            VkBufferImageCopy &copyRegion              = copyRegions.emplace_back();
            copyRegion.bufferOffset                    = i;
            copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel       = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount     = 1;
            copyRegion.imageExtent                     = {1, 1, 1};
            copyRegion.imageOffset                     = {x, y, 0};
        }

        vk.cmdPipelineBarrier2(cmdBuf, &imagePreDependencyInfo);
        vk.cmdCopyBufferToImage(cmdBuf, *srcBuffer->buffer, *interImage->image, VK_IMAGE_LAYOUT_GENERAL, 1,
                                &initCopyRegion);
        vk.cmdPipelineBarrier2(cmdBuf, &interDependencyInfo);
        vk.cmdCopyImageToBuffer(cmdBuf, *interImage->image, VK_IMAGE_LAYOUT_GENERAL, *interBuffer->buffer,
                                static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
        vk.cmdPipelineBarrier2(cmdBuf, &bufferPostDependencyInfo);
        vk.cmdCopyBufferToImage(cmdBuf, *interBuffer->buffer, *m_dstImage->image, VK_IMAGE_LAYOUT_GENERAL, 1,
                                &outCopyRegion);
        break;
    }
    case CopyMethod::ClearColorImage:
    {
        const uint8_t clearValue = m_rnd.getUint32() & 0xFF;
        deMemset(&m_expectedResult[globalQueueIndex * area], clearValue, area);

        VkClearColorValue clearColor{};
        clearColor.uint32[0] = clearValue;

        VkImageSubresourceRange range{};
        range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseArrayLayer = globalQueueIndex;
        range.layerCount     = 1;
        range.levelCount     = 1;

        vk.cmdClearColorImage(cmdBuf, *m_dstImage->image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
        break;
    }
    case CopyMethod::BlitImage:
    {
        std::vector<VkImageBlit> copyRegions;
        for (uint32_t i = 0; i < area; ++i)
        {
            const int32_t y                               = i / m_params.imageExtent;
            const int32_t x                               = i % m_params.imageExtent;
            const int setValue                            = m_rnd.getUint32() & 0xFF;
            m_expectedResult[globalQueueIndex * area + i] = static_cast<uint8_t>(setValue);

            VkImageBlit &copyRegion                  = copyRegions.emplace_back();
            copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel       = 0;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount     = 1;
            copyRegion.srcOffsets[0]                 = {setValue, 0, 0};
            copyRegion.srcOffsets[1]                 = {setValue + 1, 1, 1};
            copyRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.mipLevel       = 0;
            copyRegion.dstSubresource.baseArrayLayer = globalQueueIndex;
            copyRegion.dstSubresource.layerCount     = 1;
            copyRegion.dstOffsets[0]                 = {x, y, 0};
            copyRegion.dstOffsets[1]                 = {x + 1, y + 1, 1};
        }

        vk.cmdBlitImage(cmdBuf, *m_colorImage->image, VK_IMAGE_LAYOUT_GENERAL, *m_dstImage->image,
                        VK_IMAGE_LAYOUT_GENERAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data(),
                        VK_FILTER_NEAREST);
        break;
    }
    default:
        DE_ASSERT(0);
        break;
    }

    // Increase the atomic counter again. Only increment the counter if we are using a resource heap.
    vk.cmdPushDataEXT(cmdBuf, &pushDataInfo2);
    if (m_params.bindResourceHeap)
        vk.cmdDispatch(cmdBuf, 1, 1, 1);
    vk.cmdPipelineBarrier2(cmdBuf, &postDependencyInfo);

    endCommandBuffer(vk, cmdBuf);
}

class DescriptorHeapTestInstanceSpirv final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceSpirv(Context &context, const TestParamsSpirv &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate();

private:
    TestParamsSpirv m_params;
};

class DescriptorHeapTestCaseSpirv final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseSpirv(tcu::TestContext &testCtx, const std::string &name,
                                         const TestParamsSpirv &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceSpirv(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsSpirv m_params;
};

void DescriptorHeapTestCaseSpirv::initPrograms(vk::SourceCollections &programCollection) const
{
    char const *assembly = "";

    switch (m_params.spirvTestType)
    {
    case SpirvTestType::SizeOf:
        assembly = R"(
               OpCapability Shader
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_EXT_descriptor_heap"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpMemberDecorate %4 0 Offset 0
               OpMemberDecorate %4 1 Offset 4
               OpMemberDecorate %4 2 Offset 8
               OpMemberDecorate %4 3 Offset 12
               OpDecorate %4 Block
               OpDecorate %3 Binding 0
               OpDecorate %3 DescriptorSet 0
          %5 = OpTypeVoid
          %6 = OpTypeFunction %5
          %7 = OpTypeInt 32 0
          %4 = OpTypeStruct %7 %7 %7 %7
          %8 = OpTypePointer StorageBuffer %4
          %3 = OpVariable %8 StorageBuffer
          %9 = OpTypeInt 32 1
         %10 = OpConstant %9 0
         %11 = OpConstant %9 1
         %12 = OpConstant %9 2
         %13 = OpConstant %9 3
         %14 = OpTypePointer StorageBuffer %7
         %15 = OpTypeBufferEXT StorageBuffer
         %16 = OpTypeBufferEXT Uniform
         %17 = OpTypeImage %7 2D 0 0 0 1 Unknown
         %18 = OpTypeSampler
         %19 = OpConstantSizeOfEXT %7 %15
         %20 = OpConstantSizeOfEXT %7 %16
         %21 = OpConstantSizeOfEXT %7 %17
         %22 = OpConstantSizeOfEXT %7 %18
          %2 = OpFunction %5 None %6
         %23 = OpLabel
         %24 = OpAccessChain %14 %3 %10
         %25 = OpAccessChain %14 %3 %11
         %26 = OpAccessChain %14 %3 %12
         %27 = OpAccessChain %14 %3 %13
               OpStore %24 %19
               OpStore %25 %20
               OpStore %26 %21
               OpStore %27 %22
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::SizeOf64:
        assembly = R"(
               OpCapability Shader
               OpCapability DescriptorHeapEXT
               OpCapability Int64
               OpCapability Shader64BitIndexingEXT
               OpExtension "SPV_EXT_descriptor_heap"
               OpExtension "SPV_EXT_shader_64bit_indexing"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpMemberDecorate %4 0 Offset 0
               OpMemberDecorate %4 1 Offset 4
               OpMemberDecorate %4 2 Offset 8
               OpMemberDecorate %4 3 Offset 12
               OpDecorate %4 Block
               OpDecorate %3 Binding 0
               OpDecorate %3 DescriptorSet 0
          %5 = OpTypeVoid
          %6 = OpTypeFunction %5
          %7 = OpTypeInt 32 0
          %4 = OpTypeStruct %7 %7 %7 %7
          %8 = OpTypePointer StorageBuffer %4
          %3 = OpVariable %8 StorageBuffer
          %9 = OpTypeInt 32 1
         %10 = OpConstant %9 0
         %11 = OpConstant %9 1
         %12 = OpConstant %9 2
         %13 = OpConstant %9 3
         %14 = OpTypePointer StorageBuffer %7
         %15 = OpTypeBufferEXT StorageBuffer
         %16 = OpTypeBufferEXT Uniform
         %17 = OpTypeImage %7 2D 0 0 0 1 Unknown
         %18 = OpTypeSampler
         %19 = OpConstantSizeOfEXT %7 %15
         %20 = OpConstantSizeOfEXT %7 %16
         %21 = OpConstantSizeOfEXT %7 %17
         %22 = OpConstantSizeOfEXT %7 %18
          %2 = OpFunction %5 None %6
         %23 = OpLabel
         %24 = OpAccessChain %14 %3 %10
         %25 = OpAccessChain %14 %3 %11
         %26 = OpAccessChain %14 %3 %12
         %27 = OpAccessChain %14 %3 %13
               OpStore %24 %19
               OpStore %25 %20
               OpStore %26 %21
               OpStore %27 %22
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::UntypedStorageBuffer:
        assembly = R"(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_KHR_untyped_pointers"
               OpExtension "SPV_EXT_descriptor_heap"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main" %2
               OpExecutionMode %1 LocalSize 1 1 1
               OpDecorate %3 SpecId 0
               OpDecorate %2 BuiltIn ResourceHeapEXT
               OpMemberDecorateIdEXT %4 0 OffsetIdEXT %3
               OpMemberDecorate %5 0 Offset 0
               OpMemberDecorate %5 1 Offset 4
               OpMemberDecorate %5 2 Offset 8
               OpMemberDecorate %5 3 Offset 12
               OpDecorate %5 Block
          %6 = OpTypeVoid
          %7 = OpTypeFunction %6
          %8 = OpTypeInt 32 0
          %9 = OpConstant %8 51966
         %10 = OpConstant %8 0
         %11 = OpConstant %8 3
          %3 = OpSpecConstant %8 64
         %12 = OpTypeUntypedPointerKHR UniformConstant
         %13 = OpTypeUntypedPointerKHR StorageBuffer
         %14 = OpTypeVector %8 4
         %15 = OpTypeRuntimeArray %14
          %5 = OpTypeStruct %8 %8 %8 %8
         %16 = OpTypePointer StorageBuffer %5
         %17 = OpTypePointer StorageBuffer %14
         %18 = OpTypeBufferEXT StorageBuffer
          %4 = OpTypeStruct %18
          %2 = OpUntypedVariableKHR %12 UniformConstant
          %1 = OpFunction %6 None %7
         %19 = OpLabel
         %20 = OpUntypedAccessChainKHR %12 %4 %2 %10
         %21 = OpBufferPointerEXT %16 %20
         %22 = OpUntypedAccessChainKHR %13 %5 %21 %11
               OpStore %22 %9
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::UntypedArrayLength:
        assembly = R"(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_KHR_untyped_pointers"
               OpExtension "SPV_EXT_descriptor_heap"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main" %2
               OpExecutionMode %1 LocalSize 1 1 1
               OpDecorate %3 SpecId 0
               OpDecorate %2 BuiltIn ResourceHeapEXT
               OpMemberDecorateIdEXT %4 0 OffsetIdEXT %3
               OpDecorate %5 ArrayStride 16
               OpMemberDecorate %6 0 Offset 0
               OpMemberDecorate %6 1 Offset 16
               OpDecorate %6 Block
          %7 = OpTypeVoid
          %8 = OpTypeFunction %7
          %9 = OpTypeInt 32 0
         %10 = OpConstant %9 51966
         %11 = OpConstant %9 0
          %3 = OpSpecConstant %9 64
         %12 = OpTypeUntypedPointerKHR UniformConstant
         %13 = OpTypeUntypedPointerKHR StorageBuffer
         %14 = OpTypeVector %9 4
          %5 = OpTypeRuntimeArray %14
          %6 = OpTypeStruct %14 %5
         %15 = OpTypePointer StorageBuffer %6
         %16 = OpTypePointer StorageBuffer %14
         %17 = OpTypeBufferEXT StorageBuffer
          %4 = OpTypeStruct %17
          %2 = OpUntypedVariableKHR %12 UniformConstant
          %1 = OpFunction %7 None %8
         %18 = OpLabel
         %19 = OpUntypedAccessChainKHR %12 %4 %2 %11
         %20 = OpBufferPointerEXT %15 %19
         %21 = OpUntypedAccessChainKHR %13 %6 %20
         %22 = OpUntypedArrayLengthKHR %9 %6 %21 1
         %23 = OpCompositeConstruct %14 %22 %11 %11 %11
         %24 = OpAccessChain %16 %20 %11
               OpStore %24 %23
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::SimpleStorageTexelBuffer:
        assembly = R"(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpCapability ImageBuffer
               OpExtension "SPV_KHR_untyped_pointers"
               OpExtension "SPV_EXT_descriptor_heap"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main" %2
               OpExecutionMode %1 LocalSize 1 1 1
               OpDecorate %2 BuiltIn ResourceHeapEXT
               OpDecorate %4 SpecId 0
               OpMemberDecorate %3 0 Offset 0
               OpMemberDecorateIdEXT %3 1 OffsetIdEXT %4
               OpMemberDecorateIdEXT %3 2 OffsetIdEXT %5
          %6 = OpTypeVoid
          %7 = OpTypeFunction %6
          %8 = OpTypeInt 32 0
          %9 = OpConstant %8 0
         %10 = OpConstant %8 2
         %11 = OpConstant %8 51966
         %12 = OpConstant %8 0
          %4 = OpSpecConstant %8 0
          %5 = OpSpecConstantOp %8 IMul %4 %10
         %13 = OpTypeUntypedPointerKHR UniformConstant
          %2 = OpUntypedVariableKHR %13 UniformConstant
         %14 = OpTypeImage %8 Buffer 0 0 0 2 R32ui
          %3 = OpTypeStruct %14 %14 %14
          %1 = OpFunction %6 None %7
         %15 = OpLabel
         %16 = OpUntypedAccessChainKHR %13 %3 %2 %10
         %17 = OpLoad %14 %16
               OpImageWrite %17 %9 %11
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::UntypedImageTexelPointer:
        assembly = R"(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpCapability ImageBuffer
               OpExtension "SPV_KHR_untyped_pointers"
               OpExtension "SPV_EXT_descriptor_heap"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main" %2
               OpExecutionMode %1 LocalSize 1 1 1
               OpDecorate %2 BuiltIn ResourceHeapEXT
               OpDecorate %3 SpecId 0
               OpDecorateId %4 ArrayStrideIdEXT %3
          %5 = OpTypeVoid
          %6 = OpTypeFunction %5
          %7 = OpTypeInt 32 0
          %8 = OpConstant %7 0
          %9 = OpConstant %7 1
         %10 = OpConstant %7 2
         %11 = OpConstant %7 51966
          %3 = OpSpecConstant %7 0
         %12 = OpTypeImage %7 Buffer 0 0 0 2 R32ui
          %4 = OpTypeRuntimeArray %12
         %13 = OpTypeUntypedPointerKHR UniformConstant
         %14 = OpTypeUntypedPointerKHR Image
          %2 = OpUntypedVariableKHR %13 UniformConstant
          %1 = OpFunction %5 None %6
         %15 = OpLabel
         %16 = OpUntypedAccessChainKHR %13 %4 %2 %10
         %17 = OpUntypedImageTexelPointerEXT %14 %12 %16 %9 %8
         %18 = OpAtomicIAdd %7 %17 %9 %8 %11
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::SimpleSamplerHeap:
        assembly = R"(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpCapability Sampled1D
               OpExtension "SPV_KHR_untyped_pointers"
               OpExtension "SPV_EXT_descriptor_heap"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %1 "main" %2 %3
               OpExecutionMode %1 LocalSize 1 1 1
               OpDecorate %4 SpecId 0
               OpDecorate %5 SpecId 1
               OpDecorate %6 SpecId 2
               OpDecorate %2 BuiltIn ResourceHeapEXT
               OpDecorate %3 BuiltIn SamplerHeapEXT
               OpMemberDecorateIdEXT %7 0 OffsetIdEXT %4
               OpMemberDecorateIdEXT %7 1 OffsetIdEXT %5
               OpMemberDecorateIdEXT %8 0 OffsetIdEXT %6
               OpMemberDecorate %9 0 Offset 0
         %10 = OpTypeVoid
         %11 = OpTypeFunction %10
         %12 = OpTypeInt 32 0
         %13 = OpTypeVector %12 4
         %14 = OpTypeFloat 32
         %15 = OpConstant %12 0
         %16 = OpConstant %12 1
         %17 = OpConstant %14 0
         %18 = OpConstant %14 1
          %4 = OpSpecConstant %12 0
          %6 = OpSpecConstant %12 0
          %5 = OpSpecConstant %12 0
         %19 = OpTypeImage %12 1D 0 0 0 1 Unknown
         %20 = OpTypeBufferEXT StorageBuffer
         %21 = OpTypeSampler
         %22 = OpTypeSampledImage %19
          %7 = OpTypeStruct %19 %20
          %8 = OpTypeStruct %21
          %9 = OpTypeStruct %12
         %23 = OpTypePointer StorageBuffer %12
         %24 = OpTypeUntypedPointerKHR UniformConstant
          %2 = OpUntypedVariableKHR %24 UniformConstant
          %3 = OpUntypedVariableKHR %24 UniformConstant
          %1 = OpFunction %10 None %11
         %25 = OpLabel
         %26 = OpUntypedAccessChainKHR %24 %7 %2 %15
         %27 = OpLoad %19 %26
         %28 = OpUntypedAccessChainKHR %24 %8 %3 %15
         %29 = OpLoad %21 %28
         %30 = OpSampledImage %22 %27 %29
         %31 = OpImageSampleExplicitLod %13 %30 %18 Lod %17
         %32 = OpCompositeExtract %12 %31 0
         %33 = OpUntypedAccessChainKHR %24 %7 %2 %16
         %34 = OpBufferPointerEXT %23 %33
               OpStore %34 %32
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::FunctionCallBinding:
        assembly = R"(
               OpCapability Shader
               OpCapability SampledBuffer
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_EXT_descriptor_heap"
               OpExtension "SPV_KHR_untyped_pointers"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3 %4 %5
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpSourceExtension "GL_EXT_descriptor_heap"
               OpDecorate %3 BuiltIn ResourceHeapEXT
               OpDecorateId %6 ArrayStrideIdEXT %7
               OpDecorate %4 Binding 0
               OpDecorate %4 DescriptorSet 0
               OpDecorate %8 Block
               OpMemberDecorate %8 0 Offset 0
               OpDecorate %5 Binding 1
               OpDecorate %5 DescriptorSet 0
          %9 = OpTypeVoid
         %10 = OpTypeFunction %9
         %11 = OpTypeInt 32 0
         %12 = OpTypeImage %11 Buffer 0 0 0 1 Unknown
         %13 = OpTypePointer UniformConstant %12
         %14 = OpTypeUntypedPointerKHR UniformConstant
         %15 = OpTypeFunction %11 %14
         %16 = OpConstant %11 0
         %17 = OpTypeVector %11 4
         %18 = OpTypeVector %11 2
          %3 = OpUntypedVariableKHR %14 UniformConstant
          %7 = OpConstantSizeOfEXT %11 %12
          %6 = OpTypeRuntimeArray %12
         %19 = OpTypePointer Function %11
          %4 = OpVariable %13 UniformConstant
         %20 = OpConstant %11 1
          %8 = OpTypeStruct %18
         %21 = OpTypePointer StorageBuffer %8
          %5 = OpVariable %21 StorageBuffer
         %22 = OpTypePointer StorageBuffer %18
         %23 = OpTypeVector %11 3
         %24 = OpConstantComposite %23 %20 %20 %20
         %25 = OpFunction %11 None %15
         %26 = OpFunctionParameter %14
         %27 = OpLabel
         %28 = OpLoad %12 %26
         %29 = OpImageFetch %17 %28 %16 ZeroExtend
         %30 = OpCompositeExtract %11 %29 0
               OpReturnValue %30
               OpFunctionEnd
          %2 = OpFunction %9 None %10
         %31 = OpLabel
         %32 = OpUntypedAccessChainKHR %14 %6 %3 %16
         %33 = OpUntypedAccessChainKHR %14 %12 %4
         %34 = OpFunctionCall %11 %25 %32
         %35 = OpFunctionCall %11 %25 %33
         %36 = OpCompositeConstruct %18 %34 %35
         %37 = OpAccessChain %22 %5 %16
               OpStore %37 %36
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::FunctionCallBindingForward:
        assembly = R"(
               OpCapability Shader
               OpCapability SampledBuffer
               OpCapability UntypedPointersKHR
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_EXT_descriptor_heap"
               OpExtension "SPV_KHR_untyped_pointers"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3 %4 %5
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpSourceExtension "GL_EXT_descriptor_heap"
               OpDecorate %3 BuiltIn ResourceHeapEXT
               OpDecorateId %6 ArrayStrideIdEXT %7
               OpDecorate %4 Binding 0
               OpDecorate %4 DescriptorSet 0
               OpDecorate %8 Block
               OpMemberDecorate %8 0 Offset 0
               OpDecorate %5 Binding 1
               OpDecorate %5 DescriptorSet 0
          %9 = OpTypeVoid
         %10 = OpTypeFunction %9
         %11 = OpTypeInt 32 0
         %12 = OpTypeImage %11 Buffer 0 0 0 1 Unknown
         %13 = OpTypePointer UniformConstant %12
         %14 = OpTypeUntypedPointerKHR UniformConstant
         %15 = OpTypeFunction %11 %14
         %16 = OpConstant %11 0
         %17 = OpTypeVector %11 4
         %18 = OpTypeVector %11 2
          %3 = OpUntypedVariableKHR %14 UniformConstant
          %7 = OpConstantSizeOfEXT %11 %12
          %6 = OpTypeRuntimeArray %12
         %19 = OpTypePointer Function %11
          %4 = OpVariable %13 UniformConstant
         %20 = OpConstant %11 1
          %8 = OpTypeStruct %18
         %21 = OpTypePointer StorageBuffer %8
          %5 = OpVariable %21 StorageBuffer
         %22 = OpTypePointer StorageBuffer %18
         %23 = OpTypeVector %11 3
         %24 = OpConstantComposite %23 %20 %20 %20
          %2 = OpFunction %9 None %10
         %25 = OpLabel
         %26 = OpUntypedAccessChainKHR %14 %6 %3 %16
         %27 = OpUntypedAccessChainKHR %14 %12 %4
         %28 = OpFunctionCall %11 %29 %26
         %30 = OpFunctionCall %11 %29 %27
         %31 = OpCompositeConstruct %18 %28 %30
         %32 = OpAccessChain %22 %5 %16
               OpStore %32 %31
               OpReturn
               OpFunctionEnd
         %29 = OpFunction %11 None %15
         %33 = OpFunctionParameter %14
         %34 = OpLabel
         %35 = OpLoad %12 %33
         %36 = OpImageFetch %17 %35 %16 ZeroExtend
         %37 = OpCompositeExtract %11 %36 0
               OpReturnValue %37
               OpFunctionEnd
)";
        break;
    case SpirvTestType::StorageTexelBufferAtomic64:
        assembly = R"(
               OpCapability Shader
               OpCapability Int64
               OpCapability Int64Atomics
               OpCapability ImageBuffer
               OpCapability UntypedPointersKHR
               OpCapability Int64ImageEXT
               OpCapability DescriptorHeapEXT
               OpExtension "SPV_EXT_descriptor_heap"
               OpExtension "SPV_EXT_shader_image_int64"
               OpExtension "SPV_KHR_untyped_pointers"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpDecorate %3 BuiltIn ResourceHeapEXT
               OpDecorateId %4 ArrayStrideIdEXT %5
               OpDecorate %6 BuiltIn WorkgroupSize
          %7 = OpTypeVoid
          %8 = OpTypeFunction %7
          %9 = OpTypeUntypedPointerKHR UniformConstant
          %3 = OpUntypedVariableKHR %9 UniformConstant
         %10 = OpTypeInt 32 1
         %11 = OpConstant %10 0
         %12 = OpTypeInt 64 0
         %13 = OpTypeImage %12 Buffer 0 0 0 2 R64ui
          %5 = OpConstantSizeOfEXT %10 %13
          %4 = OpTypeRuntimeArray %13
         %14 = OpConstant %12 14627351835219836655
         %15 = OpTypeInt 32 0
         %16 = OpConstant %15 0
         %17 = OpTypePointer Image %12
         %18 = OpTypeUntypedPointerKHR Image
         %19 = OpConstant %15 1
         %20 = OpTypeVector %15 3
          %6 = OpConstantComposite %20 %19 %19 %19
          %2 = OpFunction %7 None %8
         %21 = OpLabel
         %22 = OpUntypedAccessChainKHR %9 %4 %3 %11
         %23 = OpUntypedImageTexelPointerEXT %18 %13 %22 %11 %16
         %24 = OpAtomicIAdd %12 %23 %19 %16 %14
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::SimpleVariablePointers:
        assembly = R"(
               OpCapability Shader
               OpCapability VariablePointers
               OpExtension "SPV_KHR_variable_pointers"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3 %4 %5 %6
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpDecorate %7 Block
               OpMemberDecorate %7 0 NonWritable
               OpMemberDecorate %7 0 Offset 0
               OpDecorate %3 NonWritable
               OpDecorate %3 Binding 2
               OpDecorate %3 DescriptorSet 1
               OpDecorate %8 Block
               OpMemberDecorate %8 0 NonWritable
               OpMemberDecorate %8 0 Offset 0
               OpDecorate %4 NonWritable
               OpDecorate %4 Binding 0
               OpDecorate %4 DescriptorSet 1
               OpDecorate %5 NonWritable
               OpDecorate %5 Binding 1
               OpDecorate %5 DescriptorSet 1
               OpDecorate %6 Binding 3
               OpDecorate %6 DescriptorSet 1
          %9 = OpTypeVoid
         %10 = OpTypeFunction %9
         %11 = OpTypeInt 32 0
          %7 = OpTypeStruct %11
         %12 = OpTypePointer StorageBuffer %7
          %3 = OpVariable %12 StorageBuffer
         %13 = OpTypeInt 32 1
         %14 = OpConstant %13 0
         %15 = OpTypePointer StorageBuffer %11
         %16 = OpConstant %11 0
         %17 = OpTypeBool
         %18 = OpTypePointer Function %11
          %8 = OpTypeStruct %11
         %19 = OpTypePointer StorageBuffer %8
          %4 = OpVariable %19 StorageBuffer
          %5 = OpVariable %19 StorageBuffer
          %6 = OpVariable %19 StorageBuffer
          %2 = OpFunction %9 None %10
         %20 = OpLabel
         %21 = OpVariable %18 Function
         %22 = OpAccessChain %15 %3 %14
         %23 = OpLoad %11 %22
         %24 = OpINotEqual %17 %23 %16
         %25 = OpAccessChain %15 %4 %14
         %26 = OpAccessChain %15 %5 %14
         %27 = OpSelect %15 %24 %25 %26
         %28 = OpLoad %11 %27
         %29 = OpAccessChain %15 %6 %14
               OpStore %29 %28
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::ArrayVariablePointers:
        assembly = R"(
               OpCapability Shader
               OpCapability VariablePointers
               OpExtension "SPV_KHR_variable_pointers"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3
               OpExecutionMode %2 LocalSize 1 1 1
               OpSource GLSL 450
               OpDecorate %4 Block
               OpMemberDecorate %4 0 NonWritable
               OpMemberDecorate %4 0 Offset 0
               OpDecorate %3 NonWritable
               OpDecorate %3 Binding 0
               OpDecorate %3 DescriptorSet 0
          %5 = OpTypeVoid
          %6 = OpTypeFunction %5
          %7 = OpTypeInt 32 0
          %8 = OpConstant %7 0
          %9 = OpConstant %7 1
         %10 = OpConstant %7 2
         %11 = OpConstant %7 3
         %12 = OpConstant %7 4
         %13 = OpTypePointer StorageBuffer %7
         %14 = OpConstant %7 0
         %15 = OpTypeBool
         %16 = OpTypePointer Function %7
          %4 = OpTypeStruct %7
         %17 = OpTypeArray %4 %12
         %18 = OpTypePointer StorageBuffer %17
          %3 = OpVariable %18 StorageBuffer
          %2 = OpFunction %5 None %6
         %19 = OpLabel
         %20 = OpVariable %16 Function
         %21 = OpAccessChain %13 %3 %10 %8
         %22 = OpLoad %7 %21
         %23 = OpINotEqual %15 %22 %14
         %24 = OpAccessChain %13 %3 %8 %8
         %25 = OpAccessChain %13 %3 %9 %8
         %26 = OpSelect %13 %23 %24 %25
         %27 = OpLoad %7 %26
         %28 = OpAccessChain %13 %3 %11 %8
               OpStore %28 %27
               OpReturn
               OpFunctionEnd
)";
        break;
    case SpirvTestType::AtomicImageWithinFunction:
        assembly = R"(
               OpCapability Shader
               OpCapability Image1D
               OpCapability ShaderNonUniform
               OpCapability RuntimeDescriptorArray
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %2 "main" %3 %4
               OpExecutionMode %2 LocalSize 1 1 1
               OpDecorate %3 Binding 0
               OpDecorate %3 DescriptorSet 0
               OpDecorate %4 BuiltIn GlobalInvocationId
               OpDecorate %5 NonUniform
          %6 = OpTypeVoid
          %7 = OpTypeFunction %6
          %8 = OpTypeInt 32 0
          %9 = OpTypeImage %8 1D 0 0 0 2 R32ui
         %10 = OpTypeRuntimeArray %9
         %11 = OpTypePointer UniformConstant %10
          %3 = OpVariable %11 UniformConstant
         %12 = OpTypePointer UniformConstant %9
         %13 = OpTypeFunction %6 %12
         %14 = OpConstant %8 3405692655
         %15 = OpConstant %8 0
         %16 = OpTypePointer Image %8
         %17 = OpConstant %8 1
         %18 = OpTypeVector %8 3
         %19 = OpTypePointer Input %18
          %4 = OpVariable %19 Input
         %20 = OpTypePointer Input %8
          %2 = OpFunction %6 None %7
         %21 = OpLabel
         %22 = OpAccessChain %20 %4 %15
          %5 = OpLoad %8 %22
         %23 = OpAccessChain %12 %3 %5
         %24 = OpFunctionCall %6 %25 %23
               OpReturn
               OpFunctionEnd
         %25 = OpFunction %6 None %13
         %26 = OpFunctionParameter %12
         %27 = OpLabel
         %28 = OpImageTexelPointer %16 %26 %15 %15
         %29 = OpAtomicOr %8 %28 %17 %15 %14
               OpReturn
               OpFunctionEnd
)";
        break;
    default:
        DE_ASSERT(0);
        break;
    }

    vk::SpirVAsmBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6);
    programCollection.spirvAsmSources.add("compute") << assembly << options;
}

tcu::TestStatus DescriptorHeapTestInstanceSpirv::iterate()
{
    const auto &vki                 = m_instance.getDriver();
    const auto &vkd                 = m_device.getDriver();
    const VkDevice device           = *m_device;
    const uint32_t queueFamilyIndex = m_queueFamilyIndex;
    const VkQueue queue             = m_queues[0];

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(vki, m_physDevice);

    // Descriptor heap buffer
    const VkDeviceSize storageBufferStride =
        alignUp(vki.getPhysicalDeviceDescriptorSizeEXT(m_physDevice, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                m_descriptorHeapProperties.bufferDescriptorAlignment);
    const VkDeviceSize storageTexelBufferStride =
        alignUp(vki.getPhysicalDeviceDescriptorSizeEXT(m_physDevice, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER),
                m_descriptorHeapProperties.imageDescriptorAlignment);
    const VkDeviceSize bufferDescriptorStride   = getBufferDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize imageDescriptorStride    = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize samplerDescriptorStride  = getSamplerDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize resourceDescriptorStride = getResourceDescriptorStride(m_descriptorHeapProperties);

    std::vector<uint32_t> expectedOutput;
    VkDeviceSize heapUserSize        = 0;
    VkDeviceSize samplerHeapUserSize = 0;

    uint32_t dispatchWidth = 1;

    switch (m_params.spirvTestType)
    {
    case SpirvTestType::SizeOf:
    case SpirvTestType::SizeOf64:
        expectedOutput = {
            static_cast<uint32_t>(m_descriptorHeapProperties.bufferDescriptorSize),
            static_cast<uint32_t>(m_descriptorHeapProperties.bufferDescriptorSize),
            static_cast<uint32_t>(m_descriptorHeapProperties.imageDescriptorSize),
            static_cast<uint32_t>(m_descriptorHeapProperties.samplerDescriptorSize),
        };
        break;
    case SpirvTestType::UntypedStorageBuffer:
        expectedOutput = {0, 0, 0, 0xcafe};
        heapUserSize   = 4 * storageBufferStride;
        break;
    case SpirvTestType::UntypedArrayLength:
        expectedOutput.resize(8 * 4);
        expectedOutput[0] = static_cast<uint32_t>(expectedOutput.size() / 4 - 1);
        heapUserSize      = 4 * storageBufferStride;
        break;
    case SpirvTestType::SimpleStorageTexelBuffer:
        expectedOutput.resize(16);
        expectedOutput[0] = 0xcafe;
        heapUserSize      = 3 * imageDescriptorStride;
        break;
    case SpirvTestType::UntypedImageTexelPointer:
        expectedOutput.resize(16);
        expectedOutput[1] = 0xcafe;
        heapUserSize      = 3 * imageDescriptorStride;
        break;
    case SpirvTestType::SimpleSamplerHeap:
        expectedOutput      = {1};
        heapUserSize        = 2 * resourceDescriptorStride;
        samplerHeapUserSize = 4 * samplerDescriptorStride;
        break;
    case SpirvTestType::FunctionCallBinding:
    case SpirvTestType::FunctionCallBindingForward:
        expectedOutput = {0x1111, 0x2222};
        heapUserSize   = 2 * resourceDescriptorStride;
        break;
    case SpirvTestType::StorageTexelBufferAtomic64:
        expectedOutput = {0xbeefbeef, 0xcafecafe};
        heapUserSize   = 1 * resourceDescriptorStride;
        break;
    case SpirvTestType::SimpleVariablePointers:
    case SpirvTestType::ArrayVariablePointers:
        expectedOutput = {0x5};
        heapUserSize   = 4 * resourceDescriptorStride;
        break;
    case SpirvTestType::AtomicImageWithinFunction:
        dispatchWidth = 128;
        expectedOutput.resize(dispatchWidth, 0xcafebeef);
        heapUserSize = dispatchWidth * resourceDescriptorStride;
        break;
    default:
        DE_ASSERT(0);
        break;
    }

    // Add reserved range
    const VkDeviceSize resourceAlignment = std::max({
        m_descriptorHeapProperties.bufferDescriptorAlignment,
        m_descriptorHeapProperties.imageDescriptorAlignment,
        m_descriptorHeapProperties.resourceHeapAlignment,
    });
    const VkDeviceSize samplerAlignment  = std::max({
        m_descriptorHeapProperties.samplerDescriptorAlignment,
        m_descriptorHeapProperties.samplerHeapAlignment,
    });

    heapUserSize        = alignUp(heapUserSize, resourceAlignment);
    samplerHeapUserSize = alignUp(samplerHeapUserSize, samplerAlignment);

    const VkDeviceSize heapSize        = heapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    const VkDeviceSize samplerHeapSize = samplerHeapUserSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;

    const auto heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    std::unique_ptr<Buffer> resourceHeap;
    std::unique_ptr<Buffer> samplerHeap;

    if (heapUserSize > 0)
    {
        resourceHeap = createBufferAndMemory(heapSize, heapFlags);
    }
    if (samplerHeapUserSize > 0)
    {
        samplerHeap = createBufferAndMemory(samplerHeapSize, heapFlags);
    }

    // Write the test integers.
    uint32_t *heapContents = nullptr;
    if (resourceHeap)
    {
        heapContents = static_cast<uint32_t *>(resourceHeap->memory->getHostPtr());
        deMemset(heapContents, 0xcc, static_cast<size_t>(heapUserSize));
    }

    const VkDeviceSize outputBufferSize = alignUp(static_cast<VkDeviceSize>(de::dataSize(expectedOutput)),
                                                  physDevProps.limits.minStorageBufferOffsetAlignment);
    auto outputBuffer                   = createBufferAndMemory(
        outputBufferSize, VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    deMemset(outputBuffer->memory->getHostPtr(), 0, static_cast<size_t>(outputBufferSize));

    auto computeModule = createShaderModule(vkd, device, getShaderBinary("compute"));

    auto cmdPool      = makeCommandPool(vkd, device, queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();
    beginCommandBuffer(vkd, cmdBuffer);

    std::vector<VkSpecializationMapEntry> specializationMapEntries;
    std::vector<uint32_t> specializationData;
    std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
    std::vector<std::unique_ptr<Buffer>> stagingBuffers;
    std::vector<std::unique_ptr<Image>> stagingImages;
    std::vector<char> pushData;
    std::function<void()> postDispatch;

    switch (m_params.spirvTestType)
    {
    case SpirvTestType::SizeOf:
    case SpirvTestType::SizeOf64:
    {
        mappings.resize(1);
        mappings[0]                              = initVulkanStructure();
        mappings[0].firstBinding                 = 0;
        mappings[0].bindingCount                 = 1;
        mappings[0].resourceMask                 = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        mappings[0].source                       = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
        mappings[0].sourceData.pushAddressOffset = 0;

        pushData.resize(sizeof(VkDeviceAddress));
        deMemcpy(pushData.data(), &outputBuffer->address, sizeof(VkDeviceAddress));
        break;
    }
    case SpirvTestType::UntypedStorageBuffer:
    case SpirvTestType::UntypedArrayLength:
    {
        const uint32_t ssboOffset = static_cast<uint32_t>(3 * storageBufferStride);

        specializationMapEntries.push_back({0, 0, sizeof(uint32_t)});
        specializationData.push_back(ssboOffset);

        VkHostAddressRangeEXT hostAddressRange{};
        hostAddressRange.address = reinterpret_cast<char *>(heapContents) + ssboOffset;
        hostAddressRange.size    = static_cast<size_t>(storageBufferStride);

        VkDeviceAddressRangeEXT ssboAddressRange{};
        ssboAddressRange.address = outputBuffer->address;
        ssboAddressRange.size    = de::dataSize(expectedOutput);

        VkResourceDescriptorInfoEXT ssboInfo = initVulkanStructure();
        ssboInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboInfo.data.pAddressRange          = &ssboAddressRange;

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &ssboInfo, &hostAddressRange));
        break;
    }
    case SpirvTestType::SimpleStorageTexelBuffer:
    case SpirvTestType::UntypedImageTexelPointer:
    {
        specializationMapEntries.push_back({0, 0, sizeof(uint32_t)});
        specializationData.push_back(static_cast<uint32_t>(imageDescriptorStride));

        VkHostAddressRangeEXT hostAddressRange{};
        hostAddressRange.address = reinterpret_cast<char *>(heapContents) + 2 * imageDescriptorStride;
        hostAddressRange.size    = static_cast<size_t>(storageTexelBufferStride);

        VkTexelBufferDescriptorInfoEXT texelBuffer = initVulkanStructure();
        texelBuffer.addressRange.address           = outputBuffer->address;
        texelBuffer.addressRange.size              = outputBufferSize;
        texelBuffer.format                         = VK_FORMAT_R32_UINT;

        VkResourceDescriptorInfoEXT tboInfo = initVulkanStructure();
        tboInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        tboInfo.data.pTexelBuffer           = &texelBuffer;

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &tboInfo, &hostAddressRange));
        break;
    }
    case SpirvTestType::SimpleSamplerHeap:
    {
        const uint32_t imageOffset   = 0;
        const uint32_t bufferOffset  = static_cast<uint32_t>(resourceDescriptorStride);
        const uint32_t samplerOffset = static_cast<uint32_t>(3 * samplerDescriptorStride);
        specializationMapEntries.push_back({0, 0 * sizeof(uint32_t), sizeof(uint32_t)});
        specializationMapEntries.push_back({1, 1 * sizeof(uint32_t), sizeof(uint32_t)});
        specializationMapEntries.push_back({2, 2 * sizeof(uint32_t), sizeof(uint32_t)});
        specializationData = {imageOffset, bufferOffset, samplerOffset};

        VkImageCreateInfo image1DCreateInfo = initVulkanStructure();
        image1DCreateInfo.imageType         = VK_IMAGE_TYPE_1D;
        image1DCreateInfo.format            = VK_FORMAT_R32_UINT;
        image1DCreateInfo.extent            = {1, 1, 1};
        image1DCreateInfo.mipLevels         = 1;
        image1DCreateInfo.arrayLayers       = 1;
        image1DCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        image1DCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        image1DCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image1DCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        image1DCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        auto &image1D                       = stagingImages.emplace_back(createImageAndMemory(image1DCreateInfo));

        VkImageViewCreateInfo image1DView = initVulkanStructure();
        image1DView.image                 = *image1D->image;
        image1DView.viewType              = VK_IMAGE_VIEW_TYPE_1D;
        image1DView.format                = image1DCreateInfo.format;
        image1DView.components            = makeComponentMappingIdentity();
        image1DView.subresourceRange      = makeDefaultImageSubresourceRange();

        auto initMemoryBarrier =
            makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   *image1D->image, makeDefaultImageSubresourceRange());
        auto clearColorMemoryBarrier =
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_IMAGE_LAYOUT_GENERAL, *image1D->image, makeDefaultImageSubresourceRange());

        VkClearColorValue black{};
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &initMemoryBarrier);
        vkd.cmdClearColorImage(cmdBuffer, *image1D->image, VK_IMAGE_LAYOUT_GENERAL, &black, 1,
                               &image1DView.subresourceRange);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &clearColorMemoryBarrier);

        std::array<VkHostAddressRangeEXT, 2> hostAddressRanges{};
        hostAddressRanges[0].address = reinterpret_cast<char *>(heapContents) + imageOffset;
        hostAddressRanges[0].size    = static_cast<size_t>(imageDescriptorStride);
        hostAddressRanges[1].address = reinterpret_cast<char *>(heapContents) + bufferOffset;
        hostAddressRanges[1].size    = static_cast<size_t>(bufferDescriptorStride);

        VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
        imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
        imageDescriptorInfo.pView                    = &image1DView;

        VkDeviceAddressRangeEXT ssboAddressRange{};
        ssboAddressRange.address = outputBuffer->address;
        ssboAddressRange.size    = outputBufferSize;

        std::array<VkResourceDescriptorInfoEXT, 2> resources{};
        resources[0]                    = initVulkanStructure();
        resources[0].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        resources[0].data.pImage        = &imageDescriptorInfo;
        resources[1]                    = initVulkanStructure();
        resources[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        resources[1].data.pAddressRange = &ssboAddressRange;

        VkHostAddressRangeEXT hostSamplerRange{};
        hostSamplerRange.address = reinterpret_cast<char *>(samplerHeap->memory->getHostPtr()) + samplerOffset;
        hostSamplerRange.size    = static_cast<size_t>(samplerDescriptorStride);

        VkSamplerCreateInfo samplerCreateInfo = makeDefaultSamplerCreateInfo();

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, de::sizeU32(hostAddressRanges), resources.data(),
                                                 hostAddressRanges.data()));
        VK_CHECK(vkd.writeSamplerDescriptorsEXT(*m_device, 1, &samplerCreateInfo, &hostSamplerRange));
        break;
    }
    case SpirvTestType::FunctionCallBinding:
    case SpirvTestType::FunctionCallBindingForward:
    {
        mappings.resize(2);

        mappings[0]                                      = initVulkanStructure();
        mappings[0].firstBinding                         = 0;
        mappings[0].bindingCount                         = 1;
        mappings[0].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        mappings[0].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        mappings[0].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(1 * resourceDescriptorStride);

        mappings[1]                              = initVulkanStructure();
        mappings[1].firstBinding                 = 1;
        mappings[1].bindingCount                 = 1;
        mappings[1].resourceMask                 = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
        mappings[1].source                       = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
        mappings[1].sourceData.pushAddressOffset = 0;

        pushData.resize(sizeof(VkDeviceAddress));
        deMemcpy(pushData.data(), &outputBuffer->address, sizeof(VkDeviceAddress));

        auto &inputBuffer       = stagingBuffers.emplace_back(createBufferAndMemory(
            256, VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT));
        auto inputBufferHostPtr = static_cast<char *>(inputBuffer->memory->getHostPtr());

        deMemset(inputBufferHostPtr, 0xee, 256);

        std::array<VkHostAddressRangeEXT, 2> hostAddressRanges{};
        std::array<VkResourceDescriptorInfoEXT, 2> resourceInfos{};
        std::array<VkTexelBufferDescriptorInfoEXT, 2> texelBufferDescriptorInfo{};

        for (size_t i = 0; i < 2; ++i)
        {
            hostAddressRanges[i].address = reinterpret_cast<char *>(heapContents) + i * resourceDescriptorStride;
            hostAddressRanges[i].size    = static_cast<size_t>(resourceDescriptorStride);

            deMemcpy(inputBufferHostPtr + i * 128, &expectedOutput[i], sizeof(uint32_t));
            texelBufferDescriptorInfo[i]                      = initVulkanStructure();
            texelBufferDescriptorInfo[i].addressRange.address = inputBuffer->address + VkDeviceSize{i} * 128;
            texelBufferDescriptorInfo[i].addressRange.size    = sizeof(uint32_t);
            texelBufferDescriptorInfo[i].format               = VK_FORMAT_R32_UINT;

            resourceInfos[i]                   = initVulkanStructure();
            resourceInfos[i].type              = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            resourceInfos[i].data.pTexelBuffer = &texelBufferDescriptorInfo[i];
        }
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 2, resourceInfos.data(), hostAddressRanges.data()));
        break;
    }
    case SpirvTestType::StorageTexelBufferAtomic64:
    {
        VkHostAddressRangeEXT hostAddressRange{};
        hostAddressRange.address = heapContents;
        hostAddressRange.size    = static_cast<size_t>(resourceDescriptorStride);

        VkTexelBufferDescriptorInfoEXT texelBuffer = initVulkanStructure();
        texelBuffer.addressRange.address           = outputBuffer->address;
        texelBuffer.addressRange.size              = outputBufferSize;
        texelBuffer.format                         = VK_FORMAT_R64_UINT;

        VkResourceDescriptorInfoEXT resourceDescriptorInfo = initVulkanStructure();
        resourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        resourceDescriptorInfo.data.pTexelBuffer           = &texelBuffer;
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceDescriptorInfo, &hostAddressRange));
        break;
    }
    case SpirvTestType::SimpleVariablePointers:
    case SpirvTestType::ArrayVariablePointers:
    {
        const std::array<uint32_t, 3> values = {0x5, 0xc, 0x1};
        std::array<VkHostAddressRangeEXT, 4> hostAddressRanges{};
        std::array<VkResourceDescriptorInfoEXT, 4> resourceInfos{};
        std::array<VkDeviceAddressRangeEXT, 4> addressRanges{};

        if (m_params.spirvTestType == SpirvTestType::ArrayVariablePointers)
        {
            mappings.resize(1);
            mappings.back()               = initVulkanStructure();
            mappings.back().firstBinding  = 0;
            mappings.back().descriptorSet = 0;
            mappings.back().bindingCount  = 4;
            mappings.back().resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
            mappings.back().source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
            mappings.back().sourceData.constantOffset.heapOffset      = 0;
            mappings.back().sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(resourceDescriptorStride);
        }
        else
        {
            mappings.resize(4);
            for (size_t i = 0; i < 4; ++i)
            {
                auto &mapping         = mappings[i];
                mapping               = initVulkanStructure();
                mapping.firstBinding  = static_cast<uint32_t>(i);
                mapping.descriptorSet = 1;
                mapping.bindingCount  = 1;
                mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                mapping.sourceData.constantOffset.heapOffset = static_cast<uint32_t>(i * resourceDescriptorStride);
            }
        }
        for (size_t i = 0; i < 4; ++i)
        {
            hostAddressRanges[i].address = reinterpret_cast<char *>(heapContents) + i * resourceDescriptorStride;
            hostAddressRanges[i].size    = static_cast<size_t>(resourceDescriptorStride);

            resourceInfos[i]                    = initVulkanStructure();
            resourceInfos[i].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            resourceInfos[i].data.pAddressRange = &addressRanges[i];
        }
        for (size_t i = 0; i < 3; ++i)
        {
            const VkDeviceSize inputBufferSize =
                alignUp(VkDeviceSize{sizeof(uint32_t)}, physDevProps.limits.minStorageBufferOffsetAlignment);
            auto &inputBuffer = stagingBuffers.emplace_back(createBufferAndMemory(
                inputBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT));
            deMemcpy(inputBuffer->memory->getHostPtr(), &values[i], sizeof(uint32_t));

            addressRanges[i].address = inputBuffer->address;
            addressRanges[i].size    = inputBufferSize;
        }
        addressRanges[3].address = outputBuffer->address;
        addressRanges[3].size    = outputBufferSize;

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 4, resourceInfos.data(), hostAddressRanges.data()));
        break;
    }
    case SpirvTestType::AtomicImageWithinFunction:
    {
        mappings.resize(1);
        mappings[0]              = initVulkanStructure();
        mappings[0].firstBinding = 0;
        mappings[0].bindingCount = 1;
        mappings[0].resourceMask = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        mappings[0].source       = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        mappings[0].sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(resourceDescriptorStride);

        VkImageCreateInfo image1DCreateInfo = initVulkanStructure();
        image1DCreateInfo.imageType         = VK_IMAGE_TYPE_1D;
        image1DCreateInfo.format            = VK_FORMAT_R32_UINT;
        image1DCreateInfo.extent            = {1, 1, 1};
        image1DCreateInfo.mipLevels         = 1;
        image1DCreateInfo.arrayLayers       = dispatchWidth;
        image1DCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        image1DCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        image1DCreateInfo.usage =
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image1DCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        image1DCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        const VkImage image1D = *stagingImages.emplace_back(createImageAndMemory(image1DCreateInfo))->image;

        for (uint32_t i = 0; i < dispatchWidth; ++i)
        {
            VkImageViewCreateInfo image1DView           = initVulkanStructure();
            image1DView.image                           = image1D;
            image1DView.viewType                        = VK_IMAGE_VIEW_TYPE_1D;
            image1DView.format                          = image1DCreateInfo.format;
            image1DView.components                      = makeComponentMappingIdentity();
            image1DView.subresourceRange                = makeDefaultImageSubresourceRange();
            image1DView.subresourceRange.layerCount     = 1;
            image1DView.subresourceRange.baseArrayLayer = static_cast<uint32_t>(i);

            VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
            imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
            imageDescriptorInfo.pView                    = &image1DView;

            VkResourceDescriptorInfoEXT resource = initVulkanStructure();
            resource.type                        = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            resource.data.pImage                 = &imageDescriptorInfo;

            VkHostAddressRangeEXT hostRange{};
            hostRange.address = static_cast<char *>(resourceHeap->memory->getHostPtr()) + i * resourceDescriptorStride;
            hostRange.size    = static_cast<size_t>(resourceDescriptorStride);

            VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resource, &hostRange));
        }

        VkImageSubresourceRange fullSubresource{};
        fullSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        fullSubresource.baseArrayLayer = 0;
        fullSubresource.baseMipLevel   = 0;
        fullSubresource.layerCount     = dispatchWidth;
        fullSubresource.levelCount     = 1;

        auto initMemoryBarrier = makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image1D, fullSubresource);
        auto clearColorMemoryBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, image1D, fullSubresource);

        VkClearColorValue black{};
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &initMemoryBarrier);
        vkd.cmdClearColorImage(cmdBuffer, image1D, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &fullSubresource);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &clearColorMemoryBarrier);

        postDispatch = [image1D, dispatchWidth, fullSubresource, cmdBuffer, &vkd, &outputBuffer]()
        {
            auto imageBarrier =
                makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image1D, fullSubresource);
            auto copyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_HOST_READ_BIT);

            VkBufferImageCopy copy{};
            copy.imageExtent                     = {1, 1, 1};
            copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount     = dispatchWidth;
            copy.imageSubresource.mipLevel       = 0;

            vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                   0, nullptr, 0, nullptr, 1, &imageBarrier);
            vkd.cmdCopyImageToBuffer(cmdBuffer, image1D, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outputBuffer->buffer, 1,
                                     &copy);
            vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                                   &copyBarrier, 0, nullptr, 0, nullptr);
        };
        break;
    }
    default:
        break;
    }

    VkSpecializationInfo specializationInfo{};
    specializationInfo.mapEntryCount = de::sizeU32(specializationMapEntries);
    specializationInfo.pMapEntries   = de::dataOrNull(specializationMapEntries);
    specializationInfo.dataSize      = de::dataSize(specializationData);
    specializationInfo.pData         = de::dataOrNull(specializationData);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = de::sizeU32(mappings);
    mappingInfo.pMappings                                     = de::dataOrNull(mappings);

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.stage.pSpecializationInfo   = &specializationInfo;

    auto pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    VkBindHeapInfoEXT resourceHeapBindInfo = initVulkanStructure();
    if (resourceHeap)
    {
        resourceHeapBindInfo.heapRange.address   = resourceHeap->address;
        resourceHeapBindInfo.heapRange.size      = heapSize;
        resourceHeapBindInfo.reservedRangeOffset = heapUserSize;
        resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;
    }

    VkBindHeapInfoEXT samplerHeapBindInfo = initVulkanStructure();
    if (samplerHeap)
    {
        samplerHeapBindInfo.heapRange.address   = samplerHeap->address;
        samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
        samplerHeapBindInfo.reservedRangeOffset = samplerHeapUserSize;
        samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;
    }

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.address      = de::dataOrNull(pushData);
    pushDataInfo.data.size         = de::dataSize(pushData);

    if (resourceHeap)
        vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    if (samplerHeap)
        vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);
    if (pushDataInfo.data.size > 0)
        vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(cmdBuffer, dispatchWidth, 1, 1);
    if (postDispatch)
        postDispatch();
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(device));

    // Read contents
    std::vector<uint32_t> result(expectedOutput.size());
    deMemcpy(result.data(), outputBuffer->memory->getHostPtr(), de::dataSize(expectedOutput));

    // Compare contents
    for (size_t index = 0; index < result.size(); ++index)
    {
        if (result[index] != expectedOutput[index])
        {
            std::stringstream msg;
            msg << "At index " << index << ", expected 0x" << std::hex << expectedOutput[index] << " but got 0x"
                << result[index];
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceResourceMasking : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceResourceMasking(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
    {
    }

    tcu::TestStatus iterate() override;
};

class DescriptorHeapTestCaseResourceMasking : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseResourceMasking(tcu::TestContext &testCtx, const std::string &name,
                                                   const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params(params)
    {
    }

    TestInstance *createInstance(Context &context) const
    {
        return new DescriptorHeapTestInstanceResourceMasking(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseResourceMasking::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0) uniform sampler freeSampler;
layout(binding = 0) uniform itexture1D texSampled;
layout(binding = 0, r32i) uniform readonly iimage1D readonlyImgBuffer;
layout(binding = 0, r32i) uniform iimage1D imgBuffer;
layout(binding = 0) uniform isampler1D combinedImage;
layout(binding = 0, std140) uniform A { int uniformData; };
layout(binding = 0) readonly buffer B { int readonlyData; };
layout(binding = 0) buffer C { int resultData; };

void main()
{
    int result = 0;
    result |= int(texture(isampler1D(texSampled, freeSampler), 0.5));
    result |= int(imageLoad(readonlyImgBuffer, 0));
    result |= int(imageLoad(imgBuffer, 0));
    result |= int(texture(combinedImage, 0.5));
    result |= uniformData;
    result |= readonlyData;
    resultData = result;
}
)";
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceResourceMasking::iterate()
{
    static constexpr int numResourceDescriptors = 7;

    static constexpr std::array<VkSpirvResourceTypeFlagsEXT, numResourceDescriptors> resourceTypes = {
        VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT,
        VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT,
    };

    const auto &vkd                 = m_device.getDriver();
    const VkDevice device           = *m_device;
    const uint32_t queueFamilyIndex = m_queueFamilyIndex;
    const VkQueue queue             = m_queues[0];

    const VkDeviceSize resourceDescriptorSize = getResourceDescriptorStride(m_descriptorHeapProperties);

    const auto heapFlags = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    const VkDeviceSize resourceHeapUserSize =
        alignUp(numResourceDescriptors * resourceDescriptorSize, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceHeapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto resourceHeapBuffer      = createBufferAndMemory(resourceHeapSize, heapFlags);
    const auto heapBufferHostPtr = static_cast<char *>(resourceHeapBuffer->memory->getHostPtr());

    const VkDeviceSize samplerHeapSize = m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;
    std::unique_ptr<Buffer> samplerHeapBuffer;
    if (samplerHeapSize > 0)
    {
        samplerHeapBuffer = createBufferAndMemory(samplerHeapSize, heapFlags);
    }

    const VkSamplerCreateInfo embeddedSampler = makeDefaultSamplerCreateInfo();

    std::vector<VkDescriptorSetAndBindingMappingEXT> mappings;
    std::vector<VkHostAddressRangeEXT> descriptorHostRanges;

    for (int i = 0; i < numResourceDescriptors; ++i)
    {
        VkDescriptorSetAndBindingMappingEXT mapping       = initVulkanStructure();
        mapping.descriptorSet                             = 0;
        mapping.firstBinding                              = 0;
        mapping.bindingCount                              = 1;
        mapping.resourceMask                              = resourceTypes[i];
        mapping.source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        mapping.sourceData.constantOffset.heapArrayStride = 0;
        mapping.sourceData.constantOffset.heapOffset      = static_cast<uint32_t>(i * resourceDescriptorSize);

        VkHostAddressRangeEXT descriptorHostRange{};
        descriptorHostRange.address = heapBufferHostPtr + i * resourceDescriptorSize;
        descriptorHostRange.size    = static_cast<size_t>(resourceDescriptorSize);

        if ((resourceTypes[i] == VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT) ||
            (resourceTypes[i] == VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT))
        {
            mapping.sourceData.constantOffset.pEmbeddedSampler = &embeddedSampler;
        }

        mappings.push_back(mapping);
        descriptorHostRanges.push_back(descriptorHostRange);
    }

    {
        VkDescriptorSetAndBindingMappingEXT &mapping       = mappings.emplace_back();
        mapping                                            = initVulkanStructure();
        mapping.descriptorSet                              = 0;
        mapping.firstBinding                               = 0;
        mapping.bindingCount                               = 1;
        mapping.resourceMask                               = VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
        mapping.source                                     = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        mapping.sourceData.constantOffset.pEmbeddedSampler = &embeddedSampler;
    }

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.flags             = 0;
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_1D;
    imageCreateInfo.format            = VK_FORMAT_R32_SINT;
    imageCreateInfo.extent            = {1, 1, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 4;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices   = nullptr;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    auto image = createImageAndMemory(imageCreateInfo);

    std::array<VkImageViewCreateInfo, 4> imageViews;
    for (size_t i = 0; i < imageViews.size(); ++i)
    {
        VkImageViewCreateInfo &imageView          = imageViews[i];
        imageView                                 = initVulkanStructure();
        imageView.flags                           = 0;
        imageView.image                           = *image->image;
        imageView.viewType                        = VK_IMAGE_VIEW_TYPE_1D;
        imageView.format                          = VK_FORMAT_R32_SINT;
        imageView.components                      = makeComponentMappingRGBA();
        imageView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageView.subresourceRange.baseMipLevel   = 0;
        imageView.subresourceRange.levelCount     = 1;
        imageView.subresourceRange.baseArrayLayer = static_cast<uint32_t>(i);
        imageView.subresourceRange.layerCount     = 1;
    }

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);

    const VkDeviceSize uniformBufferSize =
        alignUp(VkDeviceSize{sizeof(int32_t)}, physDevProps.limits.minUniformBufferOffsetAlignment);
    const VkDeviceSize storageBufferSize =
        alignUp(VkDeviceSize{sizeof(int32_t)}, physDevProps.limits.minStorageBufferOffsetAlignment);

    auto uniformBuffer = createBufferAndMemory(uniformBufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT |
                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    auto readonlyStorageBuffer = createBufferAndMemory(
        storageBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    const uint32_t outputValue = (1 << 6) - 1;
    auto outputBuffer = createBufferAndMemory(storageBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                     VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);

    const int32_t uniformBufferValue  = 1 << 4;
    const int32_t readonlyBufferValue = 1 << 5;

    deMemcpy(uniformBuffer->memory->getHostPtr(), &uniformBufferValue, sizeof(uniformBufferValue));
    deMemcpy(readonlyStorageBuffer->memory->getHostPtr(), &readonlyBufferValue, sizeof(readonlyBufferValue));
    deMemset(outputBuffer->memory->getHostPtr(), 0, static_cast<size_t>(storageBufferSize));

    VkImageDescriptorInfoEXT sampledImageDescriptorInfo = initVulkanStructure();
    sampledImageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    sampledImageDescriptorInfo.pView                    = &imageViews[0];

    VkImageDescriptorInfoEXT readonlyImageDescriptorInfo = initVulkanStructure();
    readonlyImageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    readonlyImageDescriptorInfo.pView                    = &imageViews[1];

    VkImageDescriptorInfoEXT readwriteImageDescriptorInfo = initVulkanStructure();
    readwriteImageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    readwriteImageDescriptorInfo.pView                    = &imageViews[2];

    VkImageDescriptorInfoEXT combinedImageDescriptorInfo = initVulkanStructure();
    combinedImageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    combinedImageDescriptorInfo.pView                    = &imageViews[3];

    VkDeviceAddressRangeEXT uniformBufferAddressRange{};
    uniformBufferAddressRange.address = uniformBuffer->address;
    uniformBufferAddressRange.size    = uniformBufferSize;

    VkDeviceAddressRangeEXT readonlyStorageBufferAddressRange{};
    readonlyStorageBufferAddressRange.address = readonlyStorageBuffer->address;
    readonlyStorageBufferAddressRange.size    = storageBufferSize;

    VkDeviceAddressRangeEXT readwriteStorageBufferAddressRange{};
    readwriteStorageBufferAddressRange.address = outputBuffer->address;
    readwriteStorageBufferAddressRange.size    = storageBufferSize;

    std::array<VkResourceDescriptorInfoEXT, numResourceDescriptors> resources{};
    for (auto &resource : resources)
    {
        resource = initVulkanStructure();
    }
    resources[0].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resources[0].data.pImage        = &sampledImageDescriptorInfo;
    resources[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resources[1].data.pImage        = &readonlyImageDescriptorInfo;
    resources[2].type               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resources[2].data.pImage        = &readwriteImageDescriptorInfo;
    resources[3].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resources[3].data.pImage        = &combinedImageDescriptorInfo;
    resources[4].type               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resources[4].data.pAddressRange = &uniformBufferAddressRange;
    resources[5].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resources[5].data.pAddressRange = &readonlyStorageBufferAddressRange;
    resources[6].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resources[6].data.pAddressRange = &readwriteStorageBufferAddressRange;

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, numResourceDescriptors, resources.data(),
                                             descriptorHostRanges.data()));

    auto computeModule = createShaderModule(m_device.getDriver(), device, getShaderBinary("compute"));

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.stage.pSpecializationInfo   = nullptr;

    auto pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    auto cmdPool              = makeCommandPool(vkd, device, queueFamilyIndex);
    auto cmdBufferPtr         = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBuffer cmdBuffer = cmdBufferPtr.get();

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeapBuffer->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = resourceHeapUserSize;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerHeapBindInfo   = initVulkanStructure();
    samplerHeapBindInfo.heapRange.address   = samplerHeapBuffer ? samplerHeapBuffer->address : 0;
    samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
    samplerHeapBindInfo.reservedRangeOffset = 0;
    samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;

    VkImageMemoryBarrier2 undefined2TransferBarrier           = initVulkanStructure();
    undefined2TransferBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
    undefined2TransferBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    undefined2TransferBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
    undefined2TransferBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    undefined2TransferBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    undefined2TransferBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    undefined2TransferBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2TransferBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2TransferBarrier.image                           = *image->image;
    undefined2TransferBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    undefined2TransferBarrier.subresourceRange.baseMipLevel   = 0;
    undefined2TransferBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    undefined2TransferBarrier.subresourceRange.baseArrayLayer = 0;
    undefined2TransferBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo undefined2TransferDependencyInfo        = initVulkanStructure();
    undefined2TransferDependencyInfo.imageMemoryBarrierCount = 1;
    undefined2TransferDependencyInfo.pImageMemoryBarriers    = &undefined2TransferBarrier;

    VkImageMemoryBarrier2 transfer2GeneralBarrier         = initVulkanStructure();
    transfer2GeneralBarrier.srcStageMask                  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    transfer2GeneralBarrier.dstStageMask                  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    transfer2GeneralBarrier.srcAccessMask                 = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    transfer2GeneralBarrier.dstAccessMask                 = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    transfer2GeneralBarrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transfer2GeneralBarrier.newLayout                     = VK_IMAGE_LAYOUT_GENERAL;
    transfer2GeneralBarrier.srcQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
    transfer2GeneralBarrier.dstQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
    transfer2GeneralBarrier.image                         = *image->image;
    transfer2GeneralBarrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    transfer2GeneralBarrier.subresourceRange.baseMipLevel = 0;
    transfer2GeneralBarrier.subresourceRange.levelCount   = VK_REMAINING_MIP_LEVELS;
    transfer2GeneralBarrier.subresourceRange.baseArrayLayer = 0;
    transfer2GeneralBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo transfer2GeneralDependencyInfo        = initVulkanStructure();
    transfer2GeneralDependencyInfo.imageMemoryBarrierCount = 1;
    transfer2GeneralDependencyInfo.pImageMemoryBarriers    = &transfer2GeneralBarrier;

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);
    vkd.cmdPipelineBarrier2(cmdBuffer, &undefined2TransferDependencyInfo);
    for (int i = 0; i < 4; ++i)
    {
        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel   = 0;
        subresourceRange.levelCount     = 1;
        subresourceRange.baseArrayLayer = static_cast<uint32_t>(i);
        subresourceRange.layerCount     = 1;

        VkClearColorValue clearColor{};
        clearColor.int32[0] = 1 << i;

        vkd.cmdClearColorImage(cmdBuffer, *image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                               &subresourceRange);
    }
    vkd.cmdPipelineBarrier2(cmdBuffer, &transfer2GeneralDependencyInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(device));

    int32_t result = 0;
    deMemcpy(&result, outputBuffer->memory->getHostPtr(), sizeof(result));
    if (result != outputValue)
    {
        std::stringstream msg;
        msg << "Expected 0x" << std::hex << outputValue << " but got 0x" << result;
        return tcu::TestStatus::fail(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceNullImageQueries : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceNullImageQueries(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
    {
    }

    tcu::TestStatus iterate() override;
};

class DescriptorHeapTestCaseNullImageQueries final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseNullImageQueries(tcu::TestContext &testCtx, const std::string &name,
                                                    const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params(params)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceNullImageQueries(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseNullImageQueries::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeGLSL = R"(#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(local_size_x = 1) in;
layout(binding = 0) uniform texture2D nonNullTexture;
layout(binding = 1) uniform texture2D nullTexture;
layout(binding = 0, r32i) readonly uniform iimage2D nonNullImage;
layout(binding = 1, r32i) readonly uniform iimage2D nullImage;
layout(binding = 0, std430) buffer O {
    ivec2 nonNullTextureSizeLevel0;
    ivec2 nonNullTextureSizeLevel1;
    ivec2 nonNullImageSize;
    ivec2 nullTextureSizeLevel0;
    ivec2 nullTextureSizeLevel1;
    ivec2 nullImageSize;
    int nonNullLevels;
    int nullLevels;
};
void main() {
    nonNullTextureSizeLevel0 = textureSize(nonNullTexture, 0);
    nonNullTextureSizeLevel1 = textureSize(nonNullTexture, 1);
    nonNullImageSize = imageSize(nonNullImage);
    nullTextureSizeLevel0 = textureSize(nullTexture, 0);
    nullTextureSizeLevel1 = textureSize(nullTexture, 1);
    nullImageSize = imageSize(nullImage);
    nonNullLevels = textureQueryLevels(nonNullTexture);
    nullLevels = textureQueryLevels(nullTexture);
}
)";
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeGLSL);
}

tcu::TestStatus DescriptorHeapTestInstanceNullImageQueries::iterate()
{
    struct OutputData
    {
        tcu::IVec2 nonNullTextureSizeLevel0;
        tcu::IVec2 nonNullTextureSizeLevel1;
        tcu::IVec2 nonNullImageSize;
        tcu::IVec2 nullTextureSizeLevel0;
        tcu::IVec2 nullTextureSizeLevel1;
        tcu::IVec2 nullImageSize;
        int32_t nonNullLevels;
        int32_t nullLevels;
    };

    const auto &vkd                 = m_device.getDriver();
    const VkDevice device           = *m_device;
    const uint32_t queueFamilyIndex = m_queueFamilyIndex;
    const VkQueue queue             = m_queues[0];

    const VkDeviceSize imageDescriptorSize  = m_descriptorHeapProperties.imageDescriptorSize;
    const VkDeviceSize bufferDescriptorSize = m_descriptorHeapProperties.bufferDescriptorSize;

    const int64_t bufferAlignment       = static_cast<int64_t>(m_descriptorHeapProperties.bufferDescriptorAlignment);
    const int64_t resourceHeapAlignment = static_cast<int64_t>(m_descriptorHeapProperties.resourceHeapAlignment);

    const VkDeviceSize imageStride = getImageDescriptorStride(m_descriptorHeapProperties);

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);

    const VkDeviceSize outputBufferSize =
        alignUp(VkDeviceSize{sizeof(OutputData)}, physDevProps.limits.minStorageBufferOffsetAlignment);
    auto outputBuffer = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                    VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);

    const VkDeviceSize userHeapSize =
        alignUp(alignUp(4 * imageStride, bufferAlignment) + bufferDescriptorSize, resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto heap                   = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    const tcu::IVec2 imageSize = tcu::IVec2(64, 32);
    const uint32_t imageLevels = 4;

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = VK_FORMAT_R32_SINT;
    imageCreateInfo.extent            = {static_cast<uint32_t>(imageSize.x()), static_cast<uint32_t>(imageSize.y()), 1};
    imageCreateInfo.mipLevels         = imageLevels;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    auto image                        = createImageAndMemory(imageCreateInfo);

    VkImageViewCreateInfo textureViewCreateInfo           = initVulkanStructure();
    textureViewCreateInfo.image                           = *image->image;
    textureViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    textureViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
    textureViewCreateInfo.components                      = makeComponentMappingRGBA();
    textureViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    textureViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    textureViewCreateInfo.subresourceRange.levelCount     = imageLevels;
    textureViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    textureViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = *image->image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32_SINT;
    imageViewCreateInfo.components                      = makeComponentMappingRGBA();
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageDescriptorInfoEXT textureDescriptorInfo = initVulkanStructure();
    textureDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    textureDescriptorInfo.pView                    = &textureViewCreateInfo;

    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
    imageDescriptorInfo.pView                    = &imageViewCreateInfo;

    VkDeviceAddressRangeEXT outputBufferAddressRange{};
    outputBufferAddressRange.address = outputBuffer->address;
    outputBufferAddressRange.size    = outputBufferSize;

    std::array<VkResourceDescriptorInfoEXT, 5> resources{};
    resources[0]                    = initVulkanStructure();
    resources[0].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resources[0].data.pImage        = &textureDescriptorInfo;
    resources[1]                    = initVulkanStructure();
    resources[1].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resources[1].data.pImage        = nullptr;
    resources[2]                    = initVulkanStructure();
    resources[2].type               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resources[2].data.pImage        = &imageDescriptorInfo;
    resources[3]                    = initVulkanStructure();
    resources[3].type               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resources[3].data.pImage        = nullptr;
    resources[4]                    = initVulkanStructure();
    resources[4].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resources[4].data.pAddressRange = &outputBufferAddressRange;

    char *const heapHostPtr = static_cast<char *>(heap->memory->getHostPtr());

    std::array<VkHostAddressRangeEXT, 5> resourceHostRanges{};
    resourceHostRanges[0].address = heapHostPtr + 0 * imageStride;
    resourceHostRanges[0].size    = static_cast<size_t>(imageDescriptorSize);
    resourceHostRanges[1].address = heapHostPtr + 1 * imageStride;
    resourceHostRanges[1].size    = static_cast<size_t>(imageDescriptorSize);
    resourceHostRanges[2].address = heapHostPtr + 2 * imageStride;
    resourceHostRanges[2].size    = static_cast<size_t>(imageDescriptorSize);
    resourceHostRanges[3].address = heapHostPtr + 3 * imageStride;
    resourceHostRanges[3].size    = static_cast<size_t>(imageDescriptorSize);
    resourceHostRanges[4].address = heapHostPtr + alignUp(4 * imageStride, bufferAlignment);
    resourceHostRanges[4].size    = static_cast<size_t>(bufferDescriptorSize);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, de::sizeU32(resources), resources.data(),
                                             resourceHostRanges.data()));

    std::array<VkDescriptorSetAndBindingMappingEXT, 5> mappings{};
    mappings[0]                                      = initVulkanStructure();
    mappings[0].descriptorSet                        = 0;
    mappings[0].firstBinding                         = 0;
    mappings[0].bindingCount                         = 1;
    mappings[0].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
    mappings[0].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[0].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(0 * imageStride);
    mappings[1]                                      = initVulkanStructure();
    mappings[1].descriptorSet                        = 0;
    mappings[1].firstBinding                         = 1;
    mappings[1].bindingCount                         = 1;
    mappings[1].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
    mappings[1].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[1].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(1 * imageStride);
    mappings[2]                                      = initVulkanStructure();
    mappings[2].descriptorSet                        = 0;
    mappings[2].firstBinding                         = 0;
    mappings[2].bindingCount                         = 1;
    mappings[2].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT;
    mappings[2].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[2].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(2 * imageStride);
    mappings[3]                                      = initVulkanStructure();
    mappings[3].descriptorSet                        = 0;
    mappings[3].firstBinding                         = 1;
    mappings[3].bindingCount                         = 1;
    mappings[3].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT;
    mappings[3].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[3].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(3 * imageStride);
    mappings[4]                                      = initVulkanStructure();
    mappings[4].descriptorSet                        = 0;
    mappings[4].firstBinding                         = 0;
    mappings[4].bindingCount                         = 1;
    mappings[4].resourceMask                         = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
    mappings[4].source                               = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[4].sourceData.constantOffset.heapOffset = static_cast<uint32_t>(alignUp(4 * imageStride, bufferAlignment));

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = heap->address;
    heapBindInfo.heapRange.size      = heapSize;
    heapBindInfo.reservedRangeOffset = userHeapSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto computeModule = createShaderModule(vkd, device, getShaderBinary("compute"));

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = static_cast<uint32_t>(mappings.size());
    mappingInfo.pMappings                                     = mappings.data();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    auto cmdPool      = makeCommandPool(vkd, device, queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkCommandBuffer cmdBuffer = cmdBufferPtr.get();
    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(device));

    OutputData *const outputData = static_cast<OutputData *>(outputBuffer->memory->getHostPtr());
    if (outputData->nonNullTextureSizeLevel0 != imageSize)
    {
        std::stringstream msg;
        msg << "Expected non-null texture size level 0 to be " << imageSize << " but got "
            << outputData->nonNullTextureSizeLevel0;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nonNullTextureSizeLevel1 != imageSize / 2)
    {
        std::stringstream msg;
        msg << "Expected non-null texture size level 1 to be " << imageSize / 2 << " but got "
            << outputData->nonNullTextureSizeLevel1;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nonNullImageSize != imageSize)
    {
        std::stringstream msg;
        msg << "Expected non-null image size to be " << imageSize << " but got " << outputData->nonNullImageSize;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nullTextureSizeLevel0 != tcu::IVec2(0, 0))
    {
        std::stringstream msg;
        msg << "Expected null texture size level 0 to be (0, 0) but got " << outputData->nullTextureSizeLevel0;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nullTextureSizeLevel1 != tcu::IVec2(0, 0))
    {
        std::stringstream msg;
        msg << "Expected null texture size level 1 to be (0, 0) but got " << outputData->nullTextureSizeLevel1;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nullImageSize != tcu::IVec2(0, 0))
    {
        std::stringstream msg;
        msg << "Expected null image size to be (0, 0) but got " << outputData->nullImageSize;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nonNullLevels != static_cast<int32_t>(imageLevels))
    {
        std::stringstream msg;
        msg << "Expected non-null texture levels to be " << imageLevels << " but got " << outputData->nonNullLevels;
        return tcu::TestStatus::fail(msg.str());
    }
    if (outputData->nullLevels != 0)
    {
        std::stringstream msg;
        msg << "Expected null texture levels to be 0 but got " << outputData->nullLevels;
        return tcu::TestStatus::fail(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceGraphics : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceGraphics(Context &context, const TestParamsGraphics &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params(params)
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsGraphics m_params;
};

class DescriptorHeapTestCaseGraphics final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseGraphics(tcu::TestContext &testCtx, const std::string &name,
                                            const TestParamsGraphics &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params(params)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceGraphics(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsGraphics m_params;
};

void DescriptorHeapTestCaseGraphics::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string vecDecl = "\n";
    if (m_params.useVectors)
        vecDecl = R"(layout(descriptor_heap) uniform UM { mat4 inputData; }  uboMat[];
layout(descriptor_heap)  buffer OV { vec4 outputData; } ssboVec[];
)";
    std::string vecWriteStart = "    ssboVec[";
    std::string vecWriteEnd   = "].outputData = uboMat[10].inputData * vec4(1, 2, 3, 4);\n";

    std::ostringstream vertex;
    vertex << R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    vertex << vecDecl;
    vertex << R"(void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    ssbo[0].outputData = ubo[1].inputData ^ pushData[0];
)";
    if (m_params.useVectors)
        vertex << vecWriteStart << "11" << vecWriteEnd;
    vertex << R"(}
)";

    std::ostringstream tcs;
    tcs <<
        R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(vertices = 1) out;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    tcs << vecDecl;
    tcs << R"(void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[0].gl_Position;
    gl_TessLevelInner[0] = 1.0;
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = 1.0;
    gl_TessLevelOuter[2] = 1.0;
    gl_TessLevelOuter[3] = 1.0;
    ssbo[2].outputData = ubo[3].inputData ^ pushData[1];
)";
    if (m_params.useVectors)
        tcs << vecWriteStart << "12" << vecWriteEnd;
    tcs << R"(}
)";

    std::ostringstream tes;
    tes << R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(triangles, equal_spacing, cw) in;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    tes << vecDecl;
    tes << R"(void main()
{
    gl_Position = vec4(gl_TessCoord.xy, 0, 1);
    ssbo[4].outputData = ubo[5].inputData ^ pushData[2];
)";
    if (m_params.useVectors)
        tes << vecWriteStart << "13" << vecWriteEnd;
    tes << R"(}
)";

    std::ostringstream geometryWithoutTess;
    geometryWithoutTess << R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(points) in;
layout(points, max_vertices = 1) out;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    geometryWithoutTess << vecDecl;
    geometryWithoutTess << R"(void main()
{
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();
    ssbo[6].outputData = ubo[7].inputData ^ pushData[3];
)";
    if (m_params.useVectors)
        geometryWithoutTess << vecWriteStart << "14" << vecWriteEnd;
    geometryWithoutTess << R"(}
)";

    std::ostringstream geometryWithTess;
    geometryWithTess << R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(triangles) in;
layout(points, max_vertices = 1) out;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    geometryWithTess << vecDecl;
    geometryWithTess << R"(void main()
{
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();
    ssbo[6].outputData = ubo[7].inputData ^ pushData[3];
)";
    if (m_params.useVectors)
        geometryWithTess << vecWriteStart << "14" << vecWriteEnd;
    geometryWithTess << R"(}
)";

    std::ostringstream fragment;
    fragment << R"(#version 450
#extension GL_EXT_descriptor_heap: require

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    fragment << vecDecl;
    fragment << R"(void main()
{
    ssbo[8].outputData = ubo[9].inputData ^ pushData[4];
}
)";

    std::ostringstream mesh;
    mesh << R"(#version 460
#extension GL_EXT_descriptor_heap: require
#extension GL_EXT_mesh_shader: enable

layout(local_size_x = 1) in;
layout(points) out;
layout(max_vertices = 1, max_primitives = 1) out;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    mesh << vecDecl;
    mesh << R"(void main()
{
    SetMeshOutputsEXT(1, 1);
    gl_MeshVerticesEXT[0].gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    gl_PrimitivePointIndicesEXT[0] = 0;
    ssbo[2].outputData = ubo[3].inputData ^ pushData[1];
)";
    if (m_params.useVectors)
        mesh << vecWriteStart << "15" << vecWriteEnd;
    mesh << R"(}
)";

    std::ostringstream task;
    task << R"(#version 460
#extension GL_EXT_mesh_shader: enable
#extension GL_EXT_descriptor_heap: require

layout(local_size_x = 1) in;

layout(push_constant, std430) uniform X { uint pushData[5]; };
layout(descriptor_heap) uniform U { uint  inputData; }  ubo[];
layout(descriptor_heap)  buffer O { uint outputData; } ssbo[];
)";
    task << vecDecl;
    task << R"(void main() {
    ssbo[0].outputData = ubo[1].inputData ^ pushData[0];
    EmitMeshTasksEXT(1, 1, 1);
)";
    if (m_params.useVectors)
        task << vecWriteStart << "16" << vecWriteEnd;
    task << R"(}
)";

    vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);

    if (m_params.enableMeshShader)
    {
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << options;

        if (m_params.enableTaskShader)
        {
            programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << options;
        }
    }
    else
    {
        programCollection.glslSources.add("vertex") << glu::VertexSource(vertex.str()) << options;

        if (m_params.enableTessellationShader)
        {
            programCollection.glslSources.add("tcs") << glu::TessellationControlSource(tcs.str()) << options;
            programCollection.glslSources.add("tes") << glu::TessellationEvaluationSource(tes.str()) << options;
        }
        if (m_params.enableGeometryShader)
        {
            if (m_params.enableTessellationShader)
            {
                programCollection.glslSources.add("geometry") << glu::GeometrySource(geometryWithTess.str()) << options;
            }
            else
            {
                programCollection.glslSources.add("geometry")
                    << glu::GeometrySource(geometryWithoutTess.str()) << options;
            }
        }
    }
    if (m_params.useFragmentShader)
    {
        programCollection.glslSources.add("fragment") << glu::FragmentSource(fragment.str()) << options;
    }
}

tcu::TestStatus DescriptorHeapTestInstanceGraphics::iterate()
{
    auto &vkd = m_device.getDriver();

    const VkDeviceSize bufferDescriptorStride = getBufferDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize resourceHeapAlignment  = m_descriptorHeapProperties.resourceHeapAlignment;
    const VkDeviceSize userHeapSize =
        alignUp((m_params.useVectors ? 17 : 12) * bufferDescriptorStride, resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto heap               = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    char *const heapHostPtr = static_cast<char *>(heap->memory->getHostPtr());

    const VkDeviceSize userSamplerHeapSize =
        alignUp(m_descriptorHeapProperties.samplerDescriptorSize, m_descriptorHeapProperties.samplerHeapAlignment);
    const VkDeviceSize samplerHeapSize = userSamplerHeapSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;
    auto samplerHeap = createBufferAndMemory(samplerHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);

    de::Random rng(m_params.seed);

    std::array<std::unique_ptr<Buffer>, 10> buffers;
    std::array<VkHostAddressRangeEXT, 10> resourceHostRanges{};
    std::array<VkDeviceAddressRangeEXT, 10> resourceDeviceAddressRanges{};
    std::array<VkResourceDescriptorInfoEXT, 10> resourceDescriptorInfos{};
    std::array<uint32_t, 5> expectedOutput{};
    std::unique_ptr<Buffer> matrixBuffer;
    std::array<std::unique_ptr<Buffer>, 6> vectorBuffers;

    for (size_t i = 0; i < resourceHostRanges.size(); ++i)
    {
        const bool isOutput = (i % 2 == 0);

        VkBufferUsageFlagBits2 usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
        if (isOutput)
        {
            usage |= VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR;
        }
        else
        {
            usage |= VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR;
        }

        const VkDeviceSize bufferSize =
            alignUp(alignUp(VkDeviceSize{sizeof(uint32_t)}, physDevProps.limits.minUniformBufferOffsetAlignment),
                    physDevProps.limits.minStorageBufferOffsetAlignment);

        buffers[i] = createBufferAndMemory(bufferSize, usage);
        deMemset(buffers[i]->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));

        if (!isOutput)
        {
            const uint32_t value = rng.getUint32();
            deMemcpy(buffers[i]->memory->getHostPtr(), &value, sizeof(value));
            expectedOutput[i / 2] = value;
        }

        resourceHostRanges[i].address = heapHostPtr + i * bufferDescriptorStride;
        resourceHostRanges[i].size    = static_cast<size_t>(bufferDescriptorStride);

        resourceDeviceAddressRanges[i].address = buffers[i]->address;
        resourceDeviceAddressRanges[i].size    = bufferSize;

        resourceDescriptorInfos[i] = initVulkanStructure();
        resourceDescriptorInfos[i].type =
            isOutput ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        resourceDescriptorInfos[i].data.pAddressRange = &resourceDeviceAddressRanges[i];
    }
    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, de::sizeU32(buffers), resourceDescriptorInfos.data(),
                                             resourceHostRanges.data()));

    if (m_params.useVectors)
    {
        const size_t heapIndex          = resourceHostRanges.size();
        const uint32_t matrixBufferSize = sizeof(float) * 16;
        matrixBuffer      = createBufferAndMemory(matrixBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR);
        float *matrixData = reinterpret_cast<float *>(matrixBuffer->memory->getHostPtr());
        for (uint32_t i = 0; i < 16; ++i)
            matrixData[i] = static_cast<float>(i);

        VkDeviceAddressRangeEXT matrixAddressRange{};
        matrixAddressRange.address = matrixBuffer->address;
        matrixAddressRange.size    = matrixBufferSize;

        VkResourceDescriptorInfoEXT matrixDescriptorInfo = initVulkanStructure();
        matrixDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        matrixDescriptorInfo.data.pAddressRange          = &matrixAddressRange;

        VkHostAddressRangeEXT matrixHostRange = {};
        matrixHostRange.address               = heapHostPtr + heapIndex * bufferDescriptorStride;
        matrixHostRange.size                  = static_cast<size_t>(bufferDescriptorStride);

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1u, &matrixDescriptorInfo, &matrixHostRange));

        const uint32_t vectorBufferSize = sizeof(float) * 4;
        for (uint32_t i = 0; i < vectorBuffers.size(); ++i)
        {
            vectorBuffers[i] = createBufferAndMemory(vectorBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                           VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR);
            deMemset(vectorBuffers[i]->memory->getHostPtr(), 0, static_cast<size_t>(vectorBufferSize));

            VkDeviceAddressRangeEXT vectorAddressRange{};
            vectorAddressRange.address = vectorBuffers[i]->address;
            vectorAddressRange.size    = vectorBufferSize;

            VkResourceDescriptorInfoEXT vectorDescriptorInfo = initVulkanStructure();
            vectorDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            vectorDescriptorInfo.data.pAddressRange          = &vectorAddressRange;

            VkHostAddressRangeEXT vectorHostRange = {};
            vectorHostRange.address               = heapHostPtr + (heapIndex + 1 + i) * bufferDescriptorStride;
            vectorHostRange.size                  = static_cast<size_t>(bufferDescriptorStride);

            VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1u, &vectorDescriptorInfo, &vectorHostRange));
        }
    }

    std::array<uint32_t, 5> pushData{};
    for (size_t i = 0; i < pushData.size(); ++i)
    {
        const uint32_t value = rng.getUint32();

        pushData[i] = value;
        expectedOutput[i] ^= value;
    }

    Move<VkShaderModule> vertexModule;
    Move<VkShaderModule> tcsModule;
    Move<VkShaderModule> tesModule;
    Move<VkShaderModule> geometryModule;
    Move<VkShaderModule> fragmentModule;
    Move<VkShaderModule> taskModule;
    Move<VkShaderModule> meshModule;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    if (m_params.enableMeshShader)
    {
        if (m_params.enableTaskShader)
        {
            taskModule = createShaderModule(vkd, *m_device, getShaderBinary("task"));

            VkPipelineShaderStageCreateInfo &taskStage = shaderStages.emplace_back();
            taskStage                                  = initVulkanStructure();
            taskStage.module                           = *taskModule;
            taskStage.pName                            = "main";
            taskStage.stage                            = VK_SHADER_STAGE_TASK_BIT_EXT;
        }
        meshModule = createShaderModule(vkd, *m_device, getShaderBinary("mesh"));

        VkPipelineShaderStageCreateInfo &meshStage = shaderStages.emplace_back();
        meshStage                                  = initVulkanStructure();
        meshStage.module                           = *meshModule;
        meshStage.pName                            = "main";
        meshStage.stage                            = VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    else
    {
        vertexModule = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));

        VkPipelineShaderStageCreateInfo &vertexStage = shaderStages.emplace_back();
        vertexStage                                  = initVulkanStructure();
        vertexStage.module                           = *vertexModule;
        vertexStage.pName                            = "main";
        vertexStage.stage                            = VK_SHADER_STAGE_VERTEX_BIT;

        if (m_params.enableTessellationShader)
        {
            tcsModule = createShaderModule(vkd, *m_device, getShaderBinary("tcs"));
            tesModule = createShaderModule(vkd, *m_device, getShaderBinary("tes"));

            VkPipelineShaderStageCreateInfo &tcsStage = shaderStages.emplace_back();
            tcsStage                                  = initVulkanStructure();
            tcsStage.module                           = *tcsModule;
            tcsStage.pName                            = "main";
            tcsStage.stage                            = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

            VkPipelineShaderStageCreateInfo &tesStage = shaderStages.emplace_back();
            tesStage                                  = initVulkanStructure();
            tesStage.module                           = *tesModule;
            tesStage.pName                            = "main";
            tesStage.stage                            = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        if (m_params.enableGeometryShader)
        {
            geometryModule = createShaderModule(vkd, *m_device, getShaderBinary("geometry"));

            VkPipelineShaderStageCreateInfo &geometryStage = shaderStages.emplace_back();
            geometryStage                                  = initVulkanStructure();
            geometryStage.module                           = *geometryModule;
            geometryStage.pName                            = "main";
            geometryStage.stage                            = VK_SHADER_STAGE_GEOMETRY_BIT;
        }
    }
    if (m_params.useFragmentShader)
    {
        fragmentModule = createShaderModule(vkd, *m_device, getShaderBinary("fragment"));

        VkPipelineShaderStageCreateInfo &fragmentStage = shaderStages.emplace_back();
        fragmentStage                                  = initVulkanStructure();
        fragmentStage.module                           = *fragmentModule;
        fragmentStage.pName                            = "main";
        fragmentStage.stage                            = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
    renderPassCreateInfo.subpassCount           = 1;
    renderPassCreateInfo.pSubpasses             = &subpass;

    Move<VkRenderPass> renderPass = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

    const uint32_t renderTargetSize               = 16;
    VkFramebufferCreateInfo framebufferCreateInfo = initVulkanStructure();
    framebufferCreateInfo.renderPass              = *renderPass;
    framebufferCreateInfo.width                   = renderTargetSize;
    framebufferCreateInfo.height                  = renderTargetSize;
    framebufferCreateInfo.layers                  = 1;

    Move<VkFramebuffer> framebuffer = createFramebuffer(vkd, *m_device, &framebufferCreateInfo);

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology =
        m_params.enableTessellationShader ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineTessellationStateCreateInfo tessellationState = initVulkanStructure();
    tessellationState.patchControlPoints                    = 1;

    const VkViewport viewport = {
        0.0f, 0.0f, static_cast<float>(renderTargetSize), static_cast<float>(renderTargetSize), 0.0f, 1.0f,
    };
    const VkRect2D scissor = {
        {0, 0},
        {renderTargetSize, renderTargetSize},
    };

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
    rasterizationState.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable                  = VK_FALSE;
    multisampleState.minSampleShading                     = 0.0f;
    multisampleState.pSampleMask                          = nullptr;
    multisampleState.alphaToCoverageEnable                = VK_FALSE;
    multisampleState.alphaToOneEnable                     = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineCreateFlags2CreateInfo);
    pipelineCreateInfo.flags                        = 0;
    pipelineCreateInfo.stageCount                   = de::sizeU32(shaderStages);
    pipelineCreateInfo.pStages                      = de::dataOrNull(shaderStages);
    pipelineCreateInfo.pVertexInputState            = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineCreateInfo.pTessellationState           = m_params.enableTessellationShader ? &tessellationState : NULL;
    pipelineCreateInfo.pViewportState               = &viewportState;
    pipelineCreateInfo.pRasterizationState          = &rasterizationState;
    pipelineCreateInfo.pMultisampleState            = &multisampleState;
    pipelineCreateInfo.pDepthStencilState           = &depthStencilState;
    pipelineCreateInfo.pColorBlendState             = &colorBlendState;
    pipelineCreateInfo.pDynamicState                = nullptr;
    pipelineCreateInfo.layout                       = VK_NULL_HANDLE;
    pipelineCreateInfo.renderPass                   = renderPass.get();

    Move<VkPipeline> pipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    Move<VkCommandPool> cmdPool = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    Move<VkCommandBuffer> cmdBufferPtr =
        allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const VkCommandBuffer cmdBuffer = cmdBufferPtr.get();
    Move<VkCommandBuffer> cmdBufferSecondaryPtr;

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = heap->address;
    heapBindInfo.heapRange.size      = heapSize;
    heapBindInfo.reservedRangeOffset = userHeapSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerHeapBindInfo   = initVulkanStructure();
    samplerHeapBindInfo.heapRange.address   = samplerHeap->address;
    samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
    samplerHeapBindInfo.reservedRangeOffset = userSamplerHeapSize;
    samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;

    VkRenderPassBeginInfo renderPassBeginInfo    = initVulkanStructure();
    renderPassBeginInfo.renderPass               = *renderPass;
    renderPassBeginInfo.framebuffer              = *framebuffer;
    renderPassBeginInfo.renderArea.extent.width  = renderTargetSize;
    renderPassBeginInfo.renderArea.extent.height = renderTargetSize;

    VkMemoryBarrier memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT;

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.size         = sizeof(pushData);
    pushDataInfo.data.address      = &pushData;

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);

    auto draw = [&](VkCommandBuffer commandBuffer)
    {
        vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vkd.cmdPushDataEXT(commandBuffer, &pushDataInfo);
        if (m_params.enableMeshShader)
            vkd.cmdDrawMeshTasksEXT(commandBuffer, 1, 1, 1);
        else
            vkd.cmdDraw(commandBuffer, 1, 1, 0, 0);
    };

    if (m_params.useSecondaryCommandBuffer)
    {
        cmdBufferSecondaryPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        const VkCommandBuffer cmdBufferSecondary = cmdBufferSecondaryPtr.get();

        VkCommandBufferInheritanceDescriptorHeapInfoEXT inheritanceHeapInfo = initVulkanStructure();
        inheritanceHeapInfo.pResourceHeapBindInfo                           = &heapBindInfo;
        inheritanceHeapInfo.pSamplerHeapBindInfo                            = &samplerHeapBindInfo;

        VkCommandBufferInheritanceInfo inheritanceInfo = initVulkanStructure();
        inheritanceInfo.pNext                          = &inheritanceHeapInfo;
        inheritanceInfo.renderPass                     = *renderPass;
        inheritanceInfo.subpass                        = 0;
        inheritanceInfo.framebuffer                    = *framebuffer;

        VkCommandBufferBeginInfo beginInfo = initVulkanStructure();
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo         = &inheritanceInfo;

        vkd.beginCommandBuffer(cmdBufferSecondary, &beginInfo);
        draw(cmdBufferSecondary);
        endCommandBuffer(vkd, cmdBufferSecondary);

        vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkd.cmdExecuteCommands(cmdBuffer, 1, &cmdBufferSecondary);
    }
    else
    {
        vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        draw(cmdBuffer);
    }
    vkd.cmdEndRenderPass(cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                           &memoryBarrier, 0, nullptr, 0, nullptr);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.queueWaitIdle(m_queues.front()));

    for (size_t i = 0; i < expectedOutput.size(); ++i)
    {
        static constexpr const char *vtgStages[] = {
            "vertex", "tessellation control", "tessellation evaluation", "geometry", "fragment",
        };
        static constexpr const char *meshStages[] = {
            "task", "mesh", "", "", "fragment",
        };
        const char *const *stages = m_params.enableMeshShader ? meshStages : vtgStages;

        if (m_params.enableMeshShader)
        {
            if (i == 0 && !m_params.enableTaskShader)
                continue;
            if (i == 2 || i == 3)
                continue;
        }
        else
        {
            if ((i == 1 || i == 2) && !m_params.enableTessellationShader)
                continue;
            if (i == 3 && !m_params.enableGeometryShader)
                continue;
        }
        if (i == 4 && !m_params.useFragmentShader)
            continue;

        uint32_t outputValue{};
        deMemcpy(&outputValue, buffers[i * 2]->memory->getHostPtr(), sizeof(outputValue));

        if (outputValue != expectedOutput[i])
        {
            std::stringstream msg;
            msg << std::hex << "Output value for the " << stages[i] << " shader is 0x" << outputValue
                << " but expected 0x" << expectedOutput[i];
            return tcu::TestStatus::fail(msg.str());
        }
    }
    if (m_params.useVectors)
    {
        for (uint32_t j = 0; j < 6; ++j)
        {
            if ((j == 1 || j == 2) && !m_params.enableTessellationShader)
                continue;
            if ((j == 3) && !m_params.enableGeometryShader)
                continue;
            if ((j == 4) && !m_params.enableMeshShader)
                continue;
            if ((j == 5) && !m_params.enableTaskShader)
                continue;
            float *vectorOutput = reinterpret_cast<float *>(vectorBuffers[j]->memory->getHostPtr());
            for (uint32_t i = 0; i < 4; ++i)
            {
                float expected = static_cast<float>(i) * 10.0f + 80.0f;
                float result   = vectorOutput[i];
                if (de::abs(result - expected) > 0.001f)
                {
                    std::stringstream msg;
                    msg << std::fixed << std::setprecision(2) << "Vector output is (" << vectorOutput[0] << ", "
                        << vectorOutput[1] << ", " << vectorOutput[2] << ", " << vectorOutput[3] << ") but expected ("
                        << expected << ", " << expected * 2 << ", " << expected * 3 << ", " << expected * 4 << ")";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceGraphicsAndCompute : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceGraphicsAndCompute(Context &context, const TestParamsBasic &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params(params)
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsBasic m_params;
};

class DescriptorHeapTestCaseGraphicsAndCompute final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseGraphicsAndCompute(tcu::TestContext &testCtx, const std::string &name,
                                                      const TestParamsBasic &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params(params)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceGraphicsAndCompute(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsBasic m_params;
};

void DescriptorHeapTestCaseGraphicsAndCompute::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string vertex = R"(#version 450
#extension GL_EXT_descriptor_heap: require
layout(descriptor_heap) buffer Output { uint data; } descriptors[];
void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    descriptors[0].data = 42;
}
)";

    std::string compute = R"(#version 450
#extension GL_EXT_descriptor_heap: require
layout(descriptor_heap) buffer Output { uint data; } descriptors[];
layout(local_size_x = 1) in;
void main()
{
    descriptors[1].data = 41;
}
)";

    vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);
    programCollection.glslSources.add("vertex") << glu::VertexSource(vertex) << options;
    programCollection.glslSources.add("compute") << glu::ComputeSource(compute) << options;
}

tcu::TestStatus DescriptorHeapTestInstanceGraphicsAndCompute::iterate()
{
    auto &vkd = m_device.getDriver();

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);

    const VkDeviceSize bufferDescriptorSize  = m_descriptorHeapProperties.bufferDescriptorSize;
    const VkDeviceSize bufferDescriptorAlign = m_descriptorHeapProperties.bufferDescriptorAlignment;

    const VkDeviceSize heapUserSize =
        alignUp(2 * bufferDescriptorAlign, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = heapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto heap = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    const VkDeviceSize bufferSize =
        alignUp(VkDeviceSize{sizeof(uint32_t)}, physDevProps.limits.minStorageBufferOffsetAlignment);

    auto outputBufferGraphics = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    auto outputBufferCompute  = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    std::array<VkDeviceAddressRangeEXT, 2> storageBuffers{};
    storageBuffers[0].address = outputBufferGraphics->address;
    storageBuffers[0].size    = bufferSize;
    storageBuffers[1].address = outputBufferCompute->address;
    storageBuffers[1].size    = bufferSize;

    std::array<VkResourceDescriptorInfoEXT, 2> resources{};
    resources[0]                    = initVulkanStructure();
    resources[0].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resources[0].data.pAddressRange = &storageBuffers[0];
    resources[1]                    = initVulkanStructure();
    resources[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resources[1].data.pAddressRange = &storageBuffers[1];

    char *const heapHostPtr = static_cast<char *>(heap->memory->getHostPtr());
    std::array<VkHostAddressRangeEXT, 2> descriptors{};
    descriptors[0].address = heapHostPtr;
    descriptors[0].size    = static_cast<size_t>(bufferDescriptorSize);
    descriptors[1].address = heapHostPtr + getBufferDescriptorStride(m_descriptorHeapProperties);
    descriptors[1].size    = static_cast<size_t>(bufferDescriptorSize);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 2, resources.data(), descriptors.data()));

    auto vertex  = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));
    auto compute = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
    renderPassCreateInfo.subpassCount           = 1;
    renderPassCreateInfo.pSubpasses             = &subpass;
    Move<VkRenderPass> renderPass               = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

    VkFramebufferCreateInfo framebufferCreateInfo = initVulkanStructure();
    framebufferCreateInfo.width                   = 1;
    framebufferCreateInfo.height                  = 1;
    framebufferCreateInfo.layers                  = 1;
    framebufferCreateInfo.renderPass              = *renderPass;
    Move<VkFramebuffer> framebuffer               = createFramebuffer(vkd, *m_device, &framebufferCreateInfo);

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineTessellationStateCreateInfo tessellationState = initVulkanStructure();

    const VkViewport viewport = makeViewport(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    const VkRect2D scissor    = makeRect2D(0, 0, 1, 1);

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.rasterizerDiscardEnable                = VK_TRUE;
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
    rasterizationState.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    auto vertexModule = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));

    VkPipelineShaderStageCreateInfo vertexStage = initVulkanStructure();
    vertexStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module                          = *vertexModule;
    vertexStage.pName                           = "main";

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                        = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stageCount                   = 1;
    pipelineCreateInfo.pStages                      = &vertexStage;
    pipelineCreateInfo.pVertexInputState            = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineCreateInfo.pTessellationState           = &tessellationState;
    pipelineCreateInfo.pViewportState               = &viewportState;
    pipelineCreateInfo.pRasterizationState          = &rasterizationState;
    pipelineCreateInfo.pMultisampleState            = &multisampleState;
    pipelineCreateInfo.pDepthStencilState           = &depthStencilState;
    pipelineCreateInfo.pColorBlendState             = &colorBlendState;
    pipelineCreateInfo.pDynamicState                = nullptr;
    pipelineCreateInfo.layout                       = VK_NULL_HANDLE;
    pipelineCreateInfo.renderPass                   = *renderPass;
    pipelineCreateInfo.subpass                      = 0;
    pipelineCreateInfo.basePipelineHandle           = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex            = 0;

    Move<VkPipeline> graphicsPipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    VkComputePipelineCreateInfo computePipelineCreateInfo = initVulkanStructure();
    computePipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    computePipelineCreateInfo.stage                       = initVulkanStructure();
    computePipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module                = *compute;
    computePipelineCreateInfo.stage.pName                 = "main";

    Move<VkPipeline> computePipeline =
        createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineCreateInfo);

    auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto &cmdBuffer   = *cmdBufferPtr;

    VkCommandBufferBeginInfo commandBufferBeginInfo = initVulkanStructure();
    commandBufferBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = heap->address;
    heapBindInfo.heapRange.size      = heapSize;
    heapBindInfo.reservedRangeOffset = heapUserSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkRenderPassBeginInfo renderPassBeginInfo    = initVulkanStructure();
    renderPassBeginInfo.renderPass               = *renderPass;
    renderPassBeginInfo.framebuffer              = *framebuffer;
    renderPassBeginInfo.renderArea.extent.width  = 1;
    renderPassBeginInfo.renderArea.extent.height = 1;

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;

    VkDependencyInfo dependencyInfo   = initVulkanStructure();
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers    = &memoryBarrier;

    VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &commandBufferBeginInfo));
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);
    vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkd.cmdDraw(cmdBuffer, 1, 1, 0, 0);
    vkd.cmdEndRenderPass(cmdBuffer);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    VK_CHECK(vkd.endCommandBuffer(cmdBuffer));

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.queueWaitIdle(m_queues.front()));

    uint32_t graphicsOutputValue = 0;
    uint32_t computeOutputValue  = 0;

    deMemcpy(&graphicsOutputValue, outputBufferGraphics->memory->getHostPtr(), sizeof(uint32_t));
    deMemcpy(&computeOutputValue, outputBufferCompute->memory->getHostPtr(), sizeof(uint32_t));

    if (graphicsOutputValue != 42)
    {
        std::stringstream msg;
        msg << std::hex << "Output value for the graphics pipeline is 0x" << graphicsOutputValue << " but expected 42";
        return tcu::TestStatus::fail(msg.str());
    }
    if (computeOutputValue != 41)
    {
        std::stringstream msg;
        msg << std::hex << "Output value for the compute pipeline is 0x" << computeOutputValue << " but expected 41";
        return tcu::TestStatus::fail(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceDifferentMappingsSameShader final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceDifferentMappingsSameShader(Context &context, const TestParamsBasic &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params(params)
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsBasic m_params;
};

class DescriptorHeapTestCaseDifferentMappingsSameShader final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseDifferentMappingsSameShader(tcu::TestContext &testCtx, const std::string &name,
                                                               const TestParamsBasic &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params(params)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceDifferentMappingsSameShader(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsBasic m_params;
};

void DescriptorHeapTestCaseDifferentMappingsSameShader::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
#extension GL_EXT_descriptor_heap: require
layout(local_size_x = 1) in;
layout(binding = 0) buffer Output { uint data; };
void main()
{
    data = 99;
}
)";
    vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << options;
}

tcu::TestStatus DescriptorHeapTestInstanceDifferentMappingsSameShader::iterate()
{
    const auto &vkd = m_device.getDriver();

    auto bufferA = createBufferAndMemory(sizeof(uint32_t), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                               VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    auto bufferB = createBufferAndMemory(sizeof(uint32_t), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                               VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkDescriptorSetAndBindingMappingEXT mappingA = initVulkanStructure();
    mappingA.descriptorSet                       = 0;
    mappingA.firstBinding                        = 0;
    mappingA.bindingCount                        = 1;
    mappingA.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappingA.source                              = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    mappingA.sourceData.pushAddressOffset        = 0;

    VkDescriptorSetAndBindingMappingEXT mappingB = initVulkanStructure();
    mappingB.descriptorSet                       = 0;
    mappingB.firstBinding                        = 0;
    mappingB.bindingCount                        = 1;
    mappingB.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappingB.source                              = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    mappingB.sourceData.pushAddressOffset        = 8;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfoA = initVulkanStructure();
    mappingInfoA.mappingCount                                  = 1;
    mappingInfoA.pMappings                                     = &mappingA;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfoB = initVulkanStructure();
    mappingInfoB.mappingCount                                  = 1;
    mappingInfoB.pMappings                                     = &mappingB;

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlagsInfo = initVulkanStructure();
    pipelineFlagsInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineFlagsInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfoA;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipelineA = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    pipelineCreateInfo.stage.pNext = &mappingInfoB;
    auto pipelineB                 = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    std::array<VkDeviceAddress, 2> pushData = {
        bufferA->address,
        bufferB->address,
    };

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.data.address      = &pushData;
    pushDataInfo.data.size         = sizeof(pushData);
    pushDataInfo.offset            = 0;

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;

    VkDependencyInfo dependencyInfo   = initVulkanStructure();
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers    = &memoryBarrier;

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineA);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineB);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    uint32_t resultA = 0, resultB = 0;
    deMemcpy(&resultA, bufferA->memory->getHostPtr(), sizeof(resultA));
    deMemcpy(&resultB, bufferB->memory->getHostPtr(), sizeof(resultB));

    if (resultA != 99 || resultB != 99)
    {
        std::stringstream msg;
        msg << "Expected A=99 and B=99, got A=" << resultA << " B=" << resultB;
        return tcu::TestStatus::fail(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceNonUniformMappings final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceNonUniformMappings(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseNonUniformMappings final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseNonUniformMappings(tcu::TestContext &testCtx, const std::string &name,
                                                      const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceNonUniformMappings(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseNonUniformMappings::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(
        #version 450
        #extension GL_EXT_nonuniform_qualifier : require
        layout(local_size_x = 8) in;

        layout(set = 0, binding = 0) buffer Output { uint data; } buffers[];

        void main() {
            buffers[nonuniformEXT(gl_LocalInvocationID.x)].data = gl_LocalInvocationID.x + 0x1000;
        }
    )";

    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceNonUniformMappings::iterate()
{
    const auto &vkd = m_device.getDriver();

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);

    const uint32_t workgroupSize              = 8;
    const VkDeviceSize bufferDescriptorStride = getBufferDescriptorStride(m_descriptorHeapProperties);

    const VkDeviceSize userHeapSize =
        alignUp(workgroupSize * bufferDescriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto heap = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    std::vector<std::unique_ptr<Buffer>> outputBuffers;
    std::vector<VkDeviceAddressRangeEXT> addressRanges;
    std::vector<VkResourceDescriptorInfoEXT> resourceInfos;
    std::vector<VkHostAddressRangeEXT> hostRanges;
    outputBuffers.reserve(workgroupSize);
    addressRanges.reserve(workgroupSize);
    resourceInfos.reserve(workgroupSize);
    hostRanges.reserve(workgroupSize);

    for (uint32_t i = 0; i < workgroupSize; ++i)
    {
        auto &buffer = outputBuffers.emplace_back();

        const VkDeviceSize bufferSize =
            alignUp(VkDeviceSize{sizeof(uint32_t)}, physDevProps.limits.minStorageBufferOffsetAlignment);
        buffer = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                       VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
        deMemset(buffer->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));

        VkDeviceAddressRangeEXT &addressRange = addressRanges.emplace_back();
        addressRange.address                  = buffer->address;
        addressRange.size                     = bufferSize;

        VkResourceDescriptorInfoEXT &resourceInfo = resourceInfos.emplace_back();
        resourceInfo                              = initVulkanStructure();
        resourceInfo.type                         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        resourceInfo.data.pAddressRange           = &addressRange;

        VkHostAddressRangeEXT &hostRange = hostRanges.emplace_back();
        hostRange.address                = static_cast<char *>(heap->memory->getHostPtr()) + i * bufferDescriptorStride;
        hostRange.size                   = static_cast<size_t>(bufferDescriptorStride);
    }
    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, workgroupSize, resourceInfos.data(), hostRanges.data()));

    VkDescriptorSetAndBindingMappingEXT mapping       = initVulkanStructure();
    mapping.descriptorSet                             = 0;
    mapping.firstBinding                              = 0;
    mapping.bindingCount                              = workgroupSize;
    mapping.resourceMask                              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mapping.source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mapping.sourceData.constantOffset.heapOffset      = 0;
    mapping.sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(bufferDescriptorStride);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &mapping;

    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    auto cmdPool              = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr         = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBuffer cmdBuffer = cmdBufferPtr.get();

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = heap->address;
    heapBindInfo.heapRange.size      = heapSize;
    heapBindInfo.reservedRangeOffset = userHeapSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo dependencyInfo   = initVulkanStructure();
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers    = &memoryBarrier;

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1); // Single workgroup with 8 invocations
    vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    endCommandBuffer(vkd, cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    for (uint32_t i = 0; i < workgroupSize; ++i)
    {
        uint32_t result = 0;
        deMemcpy(&result, outputBuffers[i]->memory->getHostPtr(), sizeof(result));

        // Expected value is invocation index + 0x1000 (from shader)
        uint32_t expectedValue = i + 0x1000;

        if (result != expectedValue)
        {
            std::stringstream msg;
            msg << "Invocation " << i << ": expected 0x" << std::hex << expectedValue << " but got 0x" << result;
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceMSAAImageRead final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceMSAAImageRead(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseMSAAImageRead final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseMSAAImageRead(tcu::TestContext &testCtx, const std::string &name,
                                                 const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceMSAAImageRead(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseMSAAImageRead::initPrograms(vk::SourceCollections &programCollection) const
{
    // Vertex shader for writing MSAA samples
    std::string writeVertexShader = R"(#version 450
void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    gl_PointSize = 4.0;
}
)";

    // Fragment shader for writing MSAA samples
    std::string writeFragmentShader = R"(#version 450
layout(push_constant) uniform PushConstants {
    uvec4 sampleData[4];
};
layout(location = 0) out uvec4 outColor;
void main()
{
    uvec4 data[4];
    data[0] = sampleData[0];
    data[1] = sampleData[1];
    data[2] = sampleData[2];
    data[3] = sampleData[3];
    outColor = data[gl_SampleID];
}
)";

    // Compute shader for reading MSAA samples
    std::string computeShader = R"(#version 450
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_descriptor_heap : require
layout(local_size_x = 1) in;
layout(descriptor_heap) uniform utexture2DMS msaaImage[];
layout(binding = 0) buffer OutputBuffer {
    uvec4 samples[];
};
void main()
{
    ivec2 coord = ivec2(0, 0);
    samples[0] = texelFetch(msaaImage[0], coord, 0);
    samples[1] = texelFetch(msaaImage[0], coord, 1);
    samples[2] = texelFetch(msaaImage[0], coord, 2);
    samples[3] = texelFetch(msaaImage[0], coord, 3);
}
)";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0u, true);

    programCollection.glslSources.add("write_vert") << glu::VertexSource(writeVertexShader) << buildOptions;
    programCollection.glslSources.add("write_frag") << glu::FragmentSource(writeFragmentShader) << buildOptions;
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << buildOptions;
}

tcu::TestStatus DescriptorHeapTestInstanceMSAAImageRead::iterate()
{
    // Write 4 samples to an MSAA image and read them back in a compute shader via a descriptor heap.

    const auto &vkd = m_device.getDriver();

    const uint32_t sampleCount = 4;

    // Create MSAA image
    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.flags             = 0;
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = VK_FORMAT_R32G32B32A32_UINT;
    imageCreateInfo.extent            = {1, 1, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_4_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

    auto msaaImage = createImageAndMemory(imageCreateInfo);

    // Create output buffer for sample data
    const VkDeviceSize outputBufferSize = sampleCount * sizeof(tcu::UVec4);
    auto outputBuffer = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR |
                                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    deMemset(outputBuffer->memory->getHostPtr(), 0, static_cast<size_t>(outputBufferSize));

    // Create descriptor heap
    const VkDeviceSize imageDescriptorStride  = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize bufferDescriptorStride = getBufferDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize userHeapSize =
        alignUp(imageDescriptorStride + bufferDescriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto heap = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    char *const heapHostPtr = static_cast<char *>(heap->memory->getHostPtr());

    // Write image descriptor
    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = *msaaImage->image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32G32B32A32_UINT;
    imageViewCreateInfo.components                      = makeComponentMappingRGBA();
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageDescriptorInfo.pView                    = &imageViewCreateInfo;

    VkResourceDescriptorInfoEXT imageResourceInfo = initVulkanStructure();
    imageResourceInfo.type                        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageResourceInfo.data.pImage                 = &imageDescriptorInfo;

    VkHostAddressRangeEXT imageDescriptorRange{};
    imageDescriptorRange.address = heapHostPtr;
    imageDescriptorRange.size    = static_cast<size_t>(imageDescriptorStride);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &imageResourceInfo, &imageDescriptorRange));

    // Write buffer descriptor for compute shader
    VkDeviceAddressRangeEXT bufferAddressRange{};
    bufferAddressRange.address = outputBuffer->address;
    bufferAddressRange.size    = outputBufferSize;

    VkResourceDescriptorInfoEXT bufferResourceInfo = initVulkanStructure();
    bufferResourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferResourceInfo.data.pAddressRange          = &bufferAddressRange;

    VkHostAddressRangeEXT bufferDescriptorRange{};
    bufferDescriptorRange.address = heapHostPtr + imageDescriptorStride;
    bufferDescriptorRange.size    = static_cast<size_t>(bufferDescriptorStride);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &bufferResourceInfo, &bufferDescriptorRange));

    // Generate sample data
    de::Random rng(m_params.seed);
    std::vector<tcu::UVec4> sampleData(sampleCount);
    for (auto &sample : sampleData)
    {
        sample = tcu::UVec4(rng.getUint32(), rng.getUint32(), rng.getUint32(), rng.getUint32());
    }

    // Create render pass for writing to MSAA image
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R32G32B32A32_UINT;
    colorAttachment.samples        = static_cast<VkSampleCountFlagBits>(sampleCount);
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorReference;

    VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
    renderPassCreateInfo.attachmentCount        = 1;
    renderPassCreateInfo.pAttachments           = &colorAttachment;
    renderPassCreateInfo.subpassCount           = 1;
    renderPassCreateInfo.pSubpasses             = &subpass;

    auto renderPass = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

    // Create framebuffer
    auto colorImageView = createImageView(vkd, *m_device, &imageViewCreateInfo);

    VkFramebufferCreateInfo framebufferCreateInfo = initVulkanStructure();
    framebufferCreateInfo.renderPass              = *renderPass;
    framebufferCreateInfo.attachmentCount         = 1;
    framebufferCreateInfo.pAttachments            = &colorImageView.get();
    framebufferCreateInfo.width                   = 1;
    framebufferCreateInfo.height                  = 1;
    framebufferCreateInfo.layers                  = 1;

    auto framebuffer = createFramebuffer(vkd, *m_device, &framebufferCreateInfo);

    // Create graphics pipeline
    auto vertexModule   = createShaderModule(vkd, *m_device, getShaderBinary("write_vert"));
    auto fragmentModule = createShaderModule(vkd, *m_device, getShaderBinary("write_frag"));

    VkPipelineShaderStageCreateInfo vertexStage = initVulkanStructure();
    vertexStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module                          = *vertexModule;
    vertexStage.pName                           = "main";

    VkPipelineShaderStageCreateInfo fragmentStage = initVulkanStructure();
    fragmentStage.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module                          = *fragmentModule;
    fragmentStage.pName                           = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertexStage, fragmentStage};

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    VkRect2D scissor    = {{0, 0}, {1, 1}};

    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                               = VK_CULL_MODE_NONE;
    rasterizationState.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = static_cast<VkSampleCountFlagBits>(sampleCount);
    multisampleState.sampleShadingEnable                  = VK_TRUE;
    multisampleState.minSampleShading                     = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = 1;
    colorBlendState.pAttachments                        = &colorBlendAttachment;

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                        = &pipelineCreateFlags2CreateInfo;
    pipelineCreateInfo.stageCount                   = de::sizeU32(shaderStages);
    pipelineCreateInfo.pStages                      = shaderStages.data();
    pipelineCreateInfo.pVertexInputState            = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineCreateInfo.pViewportState               = &viewportState;
    pipelineCreateInfo.pRasterizationState          = &rasterizationState;
    pipelineCreateInfo.pMultisampleState            = &multisampleState;
    pipelineCreateInfo.pColorBlendState             = &colorBlendState;
    pipelineCreateInfo.layout                       = VK_NULL_HANDLE;
    pipelineCreateInfo.renderPass                   = *renderPass;
    pipelineCreateInfo.subpass                      = 0;

    auto graphicsPipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Setup descriptor mappings for compute shader
    std::array<VkDescriptorSetAndBindingMappingEXT, 1> computeMappings{};
    computeMappings[0]                              = initVulkanStructure();
    computeMappings[0].descriptorSet                = 0;
    computeMappings[0].firstBinding                 = 0;
    computeMappings[0].bindingCount                 = 1;
    computeMappings[0].resourceMask                 = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    computeMappings[0].source                       = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    computeMappings[0].sourceData.pushAddressOffset = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT computeMappingInfo = initVulkanStructure();
    computeMappingInfo.mappingCount                                  = de::sizeU32(computeMappings);
    computeMappingInfo.pMappings                                     = computeMappings.data();

    // Create compute pipeline
    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkComputePipelineCreateInfo computePipelineCreateInfo = initVulkanStructure();
    computePipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    computePipelineCreateInfo.stage                       = initVulkanStructure();
    computePipelineCreateInfo.stage.pNext                 = &computeMappingInfo;
    computePipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module                = *computeModule;
    computePipelineCreateInfo.stage.pName                 = "main";

    auto computePipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineCreateInfo);

    // Create command buffer
    auto cmdPool              = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr         = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBuffer cmdBuffer = cmdBufferPtr.get();

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = heap->address;
    heapBindInfo.heapRange.size      = heapSize;
    heapBindInfo.reservedRangeOffset = userHeapSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Record commands
    VkImageMemoryBarrier2 undefined2ColorAttachmentBarrier           = initVulkanStructure();
    undefined2ColorAttachmentBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
    undefined2ColorAttachmentBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
    undefined2ColorAttachmentBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    undefined2ColorAttachmentBarrier.dstAccessMask                   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    undefined2ColorAttachmentBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    undefined2ColorAttachmentBarrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    undefined2ColorAttachmentBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2ColorAttachmentBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    undefined2ColorAttachmentBarrier.image                           = *msaaImage->image;
    undefined2ColorAttachmentBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    undefined2ColorAttachmentBarrier.subresourceRange.baseMipLevel   = 0;
    undefined2ColorAttachmentBarrier.subresourceRange.levelCount     = 1;
    undefined2ColorAttachmentBarrier.subresourceRange.baseArrayLayer = 0;
    undefined2ColorAttachmentBarrier.subresourceRange.layerCount     = 1;

    VkImageMemoryBarrier2 colorAttachment2ShaderReadBarrier           = initVulkanStructure();
    colorAttachment2ShaderReadBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorAttachment2ShaderReadBarrier.srcAccessMask                   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorAttachment2ShaderReadBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    colorAttachment2ShaderReadBarrier.dstAccessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    colorAttachment2ShaderReadBarrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment2ShaderReadBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorAttachment2ShaderReadBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    colorAttachment2ShaderReadBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    colorAttachment2ShaderReadBarrier.image                           = *msaaImage->image;
    colorAttachment2ShaderReadBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttachment2ShaderReadBarrier.subresourceRange.baseMipLevel   = 0;
    colorAttachment2ShaderReadBarrier.subresourceRange.levelCount     = 1;
    colorAttachment2ShaderReadBarrier.subresourceRange.baseArrayLayer = 0;
    colorAttachment2ShaderReadBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo undefined2ColorAttachmentDependency        = initVulkanStructure();
    undefined2ColorAttachmentDependency.imageMemoryBarrierCount = 1;
    undefined2ColorAttachmentDependency.pImageMemoryBarriers    = &undefined2ColorAttachmentBarrier;

    VkDependencyInfo colorAttachment2ShaderReadDependency        = initVulkanStructure();
    colorAttachment2ShaderReadDependency.imageMemoryBarrierCount = 1;
    colorAttachment2ShaderReadDependency.pImageMemoryBarriers    = &colorAttachment2ShaderReadBarrier;

    VkMemoryBarrier2 computeBarrier = initVulkanStructure();
    computeBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    computeBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    computeBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    computeBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo computeDependency   = initVulkanStructure();
    computeDependency.memoryBarrierCount = 1;
    computeDependency.pMemoryBarriers    = &computeBarrier;

    VkRenderPassBeginInfo renderPassBeginInfo = initVulkanStructure();
    renderPassBeginInfo.renderPass            = *renderPass;
    renderPassBeginInfo.framebuffer           = *framebuffer;
    renderPassBeginInfo.renderArea            = {{0, 0}, {1, 1}};

    VkPushDataInfoEXT graphicsPushDataInfo = initVulkanStructure();
    graphicsPushDataInfo.data.address      = de::dataOrNull(sampleData);
    graphicsPushDataInfo.data.size         = de::dataSize(sampleData);
    graphicsPushDataInfo.offset            = 0;

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.data.address      = &outputBuffer->address;
    pushDataInfo.data.size         = sizeof(outputBuffer->address);
    pushDataInfo.offset            = 0;

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);

    // Transition image for color attachment
    vkd.cmdPipelineBarrier2(cmdBuffer, &undefined2ColorAttachmentDependency);

    // Render pass to write MSAA samples
    vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    // Draw one point for each sample, pushing sample data as push data
    vkd.cmdPushDataEXT(cmdBuffer, &graphicsPushDataInfo);
    vkd.cmdDraw(cmdBuffer, 1, 1, 0, 0);

    vkd.cmdEndRenderPass(cmdBuffer);

    // Transition image for shader read
    vkd.cmdPipelineBarrier2(cmdBuffer, &colorAttachment2ShaderReadDependency);

    // Read samples using compute shader
    vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    vkd.cmdPipelineBarrier2(cmdBuffer, &computeDependency);

    endCommandBuffer(vkd, cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    // Verify results
    std::array<tcu::UVec4, 4> result;
    deMemcpy(&result, outputBuffer->memory->getHostPtr(), sizeof(result));
    for (uint32_t i = 0; i < sampleCount; ++i)
    {
        if (result[i] != sampleData[i])
        {
            std::stringstream msg;
            msg << "Sample " << i << " mismatch: expected (" << sampleData[i].x() << ", " << sampleData[i].y() << ", "
                << sampleData[i].z() << ", " << sampleData[i].w() << ") but got (" << result[i].x() << ", "
                << result[i].y() << ", " << result[i].z() << ", " << result[i].w() << ")";
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceResourceHeapAccess final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceResourceHeapAccess(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseResourceHeapAccess final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseResourceHeapAccess(tcu::TestContext &testCtx, const std::string &name,
                                                      const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceResourceHeapAccess(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseResourceHeapAccess::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0, r32ui) uniform uimageBuffer outputBuffer[1];
layout(push_constant) uniform PushConstants {
    uint data;
};
void main()
{
    imageStore(outputBuffer[0], 0, uvec4(data, 0, 0, 0));
}
)";
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0u, true);
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << buildOptions;
}

tcu::TestStatus DescriptorHeapTestInstanceResourceHeapAccess::iterate()
{
    const auto &vkd = m_device.getDriver();

    const size_t numBuffers       = 16;
    const VkDeviceSize bufferSize = sizeof(uint32_t);

    // Create 16 distinct output buffers
    std::vector<std::unique_ptr<Buffer>> outputBuffers;
    std::vector<uint32_t> expectedValues;

    de::Random rnd(m_params.seed);

    for (size_t i = 0; i < numBuffers; ++i)
    {
        auto buffer = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT_KHR |
                                                            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

        // Initialize buffer to zero
        deMemset(buffer->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));

        outputBuffers.push_back(std::move(buffer));
        expectedValues.push_back(rnd.getUint32());
    }

    // Create resource heap with space for 1 image descriptor and for it to be aligned to 4 bytes.
    const VkDeviceSize imageDescriptorSize   = m_descriptorHeapProperties.imageDescriptorSize;
    const VkDeviceSize imageDescriptorStride = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize userHeapSize =
        alignUp(imageDescriptorStride + 4, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize totalHeapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto resourceHeap = createBufferAndMemory(totalHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                 VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                 VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR);

    VkBindHeapInfoEXT heapBindInfo   = initVulkanStructure();
    heapBindInfo.heapRange.address   = resourceHeap->address;
    heapBindInfo.heapRange.size      = totalHeapSize;
    heapBindInfo.reservedRangeOffset = userHeapSize;
    heapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Create descriptor mapping
    VkDescriptorSetAndBindingMappingEXT mapping       = initVulkanStructure();
    mapping.descriptorSet                             = 0;
    mapping.firstBinding                              = 0;
    mapping.bindingCount                              = 1;
    mapping.resourceMask                              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mapping.source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mapping.sourceData.constantOffset.heapOffset      = 0;
    mapping.sourceData.constantOffset.heapArrayStride = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &mapping;

    // Create compute pipeline
    const auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();
    pipelineFlags.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineFlags;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Record command buffer
    auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &heapBindInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    // Loop through each buffer
    for (size_t i = 0; i < numBuffers; ++i)
    {
        // Ensure the previous iteration's dispatch is complete
        if (i > 0)
        {
            VkMemoryBarrier2 barrier = initVulkanStructure();
            barrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &barrier;
            vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        }

        // Create texel buffer descriptor
        VkTexelBufferDescriptorInfoEXT texelInfo = initVulkanStructure();
        texelInfo.format                         = VK_FORMAT_R32_UINT;
        texelInfo.addressRange.address           = outputBuffers[i]->address;
        texelInfo.addressRange.size              = bufferSize;

        VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
        resourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        resourceInfo.data.pTexelBuffer           = &texelInfo;

        // Write descriptor to temporary buffer
        std::vector<char> descriptorData(static_cast<size_t>(alignUp(imageDescriptorSize, 4)));
        VkHostAddressRangeEXT hostRange{};
        hostRange.address = descriptorData.data();
        hostRange.size    = descriptorData.size();

        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostRange));

        // Use vkCmdUpdateBuffer to write descriptor into resource heap
        vkd.cmdUpdateBuffer(cmdBuffer, *resourceHeap->buffer, 0, VkDeviceSize{descriptorData.size()},
                            descriptorData.data());

        // Barrier to ensure update completes before use
        {
            VkMemoryBarrier2 barrier = initVulkanStructure();
            barrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask    = VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT;

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &barrier;
            vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        }

        // Push the expected value
        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.offset            = 0;
        pushDataInfo.data.size         = sizeof(expectedValues[i]);
        pushDataInfo.data.address      = &expectedValues[i];

        vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);

        // Dispatch
        vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    }

    // Wait for compute shader to finish and flush its results
    VkMemoryBarrier2 compute2HostMemoryBarrier = initVulkanStructure();
    compute2HostMemoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    compute2HostMemoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    compute2HostMemoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    compute2HostMemoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo compute2hostDependencyInfo   = initVulkanStructure();
    compute2hostDependencyInfo.memoryBarrierCount = 1;
    compute2hostDependencyInfo.pMemoryBarriers    = &compute2HostMemoryBarrier;
    vkd.cmdPipelineBarrier2(cmdBuffer, &compute2hostDependencyInfo);

    endCommandBuffer(vkd, cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    // Verify results
    for (size_t i = 0; i < numBuffers; ++i)
    {
        uint32_t result = 0;
        deMemcpy(&result, outputBuffers[i]->memory->getHostPtr(), sizeof(result));

        if (result != expectedValues[i])
        {
            std::stringstream stream;
            stream << "Buffer " << i << ": expected 0x" << std::hex << expectedValues[i] << " but got 0x" << result;
            return tcu::TestStatus::fail(stream.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceSamplerHeapAccess final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceSamplerHeapAccess(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCaseSamplerHeapAccess final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseSamplerHeapAccess(tcu::TestContext &testCtx, const std::string &name,
                                                     const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceSamplerHeapAccess(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseSamplerHeapAccess::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) buffer OutputBuffer {
    vec4 result;
};

void main()
{
    result = textureLod(tex, vec2(-1.0, 0.5), 0);
}
)";
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

tcu::TestStatus DescriptorHeapTestInstanceSamplerHeapAccess::iterate()
{
    const auto &vkd = m_device.getDriver();

    const VkDeviceSize bufferSize = 4 * sizeof(float);
    const size_t numSamplers      = 3;

    // Array of border colors to test (built-in floating point border colors)
    static constexpr std::array<VkBorderColor, 4> borderColors = {
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    };
    static constexpr std::array<std::array<float, 4>, 4> expectedValues = {
        std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}, // OPAQUE_WHITE
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}, // TRANSPARENT_BLACK
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}, // OPAQUE_BLACK
    };

    // Create distinct output buffers
    std::vector<std::unique_ptr<Buffer>> outputBuffers;

    for (size_t i = 0; i < numSamplers; ++i)
    {
        auto buffer = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT_KHR |
                                                            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

        // Initialize buffer to zero
        deMemset(buffer->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));

        outputBuffers.push_back(std::move(buffer));
    }

    // Create sampler heap with space for 1 sampler descriptor
    const VkDeviceSize samplerDescriptorSize   = m_descriptorHeapProperties.samplerDescriptorSize;
    const VkDeviceSize samplerDescriptorStride = getSamplerDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize userHeapSize =
        alignUp(samplerDescriptorStride + 4, m_descriptorHeapProperties.samplerHeapAlignment);
    const VkDeviceSize totalHeapSize = userHeapSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;

    auto samplerHeap = createBufferAndMemory(totalHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR);

    // Create resource heap with space for 1 image descriptor
    const VkDeviceSize imageDescriptorStride = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize resourceUserHeapSize =
        alignUp(imageDescriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceUserHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    auto resourceHeap = createBufferAndMemory(resourceHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                    VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkBindHeapInfoEXT samplerHeapBindInfo   = initVulkanStructure();
    samplerHeapBindInfo.heapRange.address   = samplerHeap->address;
    samplerHeapBindInfo.heapRange.size      = totalHeapSize;
    samplerHeapBindInfo.reservedRangeOffset = userHeapSize;
    samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = resourceHeap->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = resourceUserHeapSize;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Create a 1x1 image to sample from
    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageCreateInfo.extent            = {1, 1, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

    auto image = createImageAndMemory(imageCreateInfo);

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = *image->image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageViewCreateInfo.components                      = makeComponentMappingRGBA();
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    // Write image descriptor to resource heap
    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.pView                    = &imageViewCreateInfo;
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
    resourceInfo.type                        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resourceInfo.data.pImage                 = &imageDescriptorInfo;

    VkHostAddressRangeEXT resourceHostRange{};
    resourceHostRange.address = resourceHeap->memory->getHostPtr();
    resourceHostRange.size    = static_cast<size_t>(imageDescriptorStride);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &resourceHostRange));

    // Create descriptor mapping - combined image sampler
    VkDescriptorSetAndBindingMappingEXT mapping         = initVulkanStructure();
    mapping.descriptorSet                               = 0;
    mapping.firstBinding                                = 0;
    mapping.bindingCount                                = 1;
    mapping.resourceMask                                = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mapping.source                                      = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mapping.sourceData.constantOffset.heapOffset        = 0;
    mapping.sourceData.constantOffset.heapArrayStride   = 0;
    mapping.sourceData.constantOffset.samplerHeapOffset = 0;
    mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;

    VkDescriptorSetAndBindingMappingEXT outputMapping = initVulkanStructure();
    outputMapping.descriptorSet                       = 0;
    outputMapping.firstBinding                        = 1;
    outputMapping.bindingCount                        = 1;
    outputMapping.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    outputMapping.source                              = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    outputMapping.sourceData.pushAddressOffset        = 0;

    std::array<VkDescriptorSetAndBindingMappingEXT, 2> mappings = {mapping, outputMapping};

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = de::sizeU32(mappings);
    mappingInfo.pMappings                                     = mappings.data();

    // Create compute pipeline
    const auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();
    pipelineFlags.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &pipelineFlags;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *computeModule;
    pipelineCreateInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Record command buffer
    auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Transition image to transfer destination layout
    {
        VkImageMemoryBarrier imageBarrier            = initVulkanStructure();
        imageBarrier.srcAccessMask                   = 0;
        imageBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.image                           = *image->image;
        imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.baseMipLevel   = 0;
        imageBarrier.subresourceRange.levelCount     = 1;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount     = 1;

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1, &imageBarrier);
    }

    // Clear the image contents
    {
        VkImageSubresourceRange imageSubresourceRange{};
        imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageSubresourceRange.layerCount = 1;
        imageSubresourceRange.levelCount = 1;

        VkClearColorValue clearColor{};
        vkd.cmdClearColorImage(cmdBuffer, *image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                               &imageSubresourceRange);
    }

    // Transition image to shader read layout
    {
        VkImageMemoryBarrier2 barrier           = initVulkanStructure();
        barrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = *image->image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkDependencyInfo dependencyInfo        = initVulkanStructure();
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers    = &barrier;

        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    }

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    // Loop through each sampler/output buffer
    for (size_t i = 0; i < numSamplers; ++i)
    {
        // Ensure the previous iteration's dispatch is complete
        if (i > 0)
        {
            VkMemoryBarrier2 barrier = initVulkanStructure();
            barrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &barrier;
            vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        }

        // Create sampler with different border color each iteration
        VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
        samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
        samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerCreateInfo.mipLodBias              = 0.0f;
        samplerCreateInfo.anisotropyEnable        = VK_FALSE;
        samplerCreateInfo.maxAnisotropy           = 0.0f;
        samplerCreateInfo.compareEnable           = VK_FALSE;
        samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerCreateInfo.minLod                  = 0.0f;
        samplerCreateInfo.maxLod                  = 1000.0f;
        samplerCreateInfo.borderColor             = borderColors[i];
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

        // Write sampler descriptor to temporary buffer
        std::vector<char> descriptorData(static_cast<size_t>(samplerDescriptorSize));
        VkHostAddressRangeEXT hostRange{};
        hostRange.address = descriptorData.data();
        hostRange.size    = descriptorData.size();

        VK_CHECK(vkd.writeSamplerDescriptorsEXT(*m_device, 1, &samplerCreateInfo, &hostRange));

        // Use vkCmdUpdateBuffer to write descriptor into sampler heap
        vkd.cmdUpdateBuffer(cmdBuffer, *samplerHeap->buffer, 0, alignUp(samplerDescriptorSize, 4),
                            descriptorData.data());

        // Barrier to ensure update completes before use
        {
            VkMemoryBarrier2 barrier = initVulkanStructure();
            barrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask    = VK_ACCESS_2_SAMPLER_HEAP_READ_BIT_EXT;

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1;
            dependencyInfo.pMemoryBarriers    = &barrier;
            vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        }

        // Push the output buffer address
        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.offset            = 0;
        pushDataInfo.data.size         = sizeof(VkDeviceAddress);
        pushDataInfo.data.address      = &outputBuffers[i]->address;

        vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);

        // Dispatch
        vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    }

    // Wait for compute shader to finish and flush its results
    VkMemoryBarrier2 compute2HostMemoryBarrier = initVulkanStructure();
    compute2HostMemoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    compute2HostMemoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    compute2HostMemoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    compute2HostMemoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo compute2hostDependencyInfo   = initVulkanStructure();
    compute2hostDependencyInfo.memoryBarrierCount = 1;
    compute2hostDependencyInfo.pMemoryBarriers    = &compute2HostMemoryBarrier;
    vkd.cmdPipelineBarrier2(cmdBuffer, &compute2hostDependencyInfo);

    endCommandBuffer(vkd, cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;

    VK_CHECK(vkd.queueSubmit(m_queues[0], 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    // Verify results
    for (size_t i = 0; i < numSamplers; ++i)
    {
        std::array<float, 4> result{};
        deMemcpy(&result, outputBuffers[i]->memory->getHostPtr(), sizeof(result));

        if (result != expectedValues[i])
        {
            std::stringstream stream;
            stream << "Buffer " << i << ": expected (" << expectedValues[i][0] << ", " << expectedValues[i][1] << ", "
                   << expectedValues[i][2] << ", " << expectedValues[i][3] << ") but got (" << result[0] << ", "
                   << result[1] << ", " << result[2] << ", " << result[3] << ")";
            return tcu::TestStatus::fail(stream.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

enum class SecondaryCopyType
{
    NONE,
    RESOURCE_HEAP_COMMAND_COPY,
    RESOURCE_HEAP_SHADER_COPY,
    SAMPLER_HEAP_COMMAND_COPY,
    SAMPLER_HEAP_SHADER_COPY,
};

struct SecondaryTestParams : TestParams
{
    SecondaryCopyType testType;
    bool copyInSecondary;
};

class DescriptorHeapTestInstanceSecondary final : public DescriptorHeapTestInstanceBase
{
    const uint32_t m_imageHeapIndex          = 16u;
    const uint32_t m_samplerHeapIndex        = 29u;
    const uint32_t m_imageHeapCopySrcIndex   = 4u;
    const uint32_t m_samplerHeapCopySrcIndex = 5u;
    const uint32_t m_resultBufferIndex       = 0u;
    const uint32_t m_copyBufferIndex         = 1u;
    const uint32_t m_dstCopyIndex            = 2u;

    std::unique_ptr<Buffer> m_resourceHeap;
    std::unique_ptr<Buffer> m_samplerHeap;
    std::unique_ptr<Buffer> m_srcCopyBuffer;
    Move<VkPipeline> m_copyPipeline;

public:
    explicit DescriptorHeapTestInstanceSecondary(Context &context, const SecondaryTestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    void copyDescriptor(VkCommandBuffer commandBuffer);
    tcu::TestStatus iterate() override;

private:
    SecondaryTestParams m_params;
};

void DescriptorHeapTestInstanceSecondary::copyDescriptor(VkCommandBuffer commandBuffer)
{
    if (m_params.testType == SecondaryCopyType::NONE)
        return;

    const auto &vkd                   = m_device.getDriver();
    const VkDeviceSize resourceStride = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize samplerStride  = getSamplerDescriptorStride(m_descriptorHeapProperties);

    if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY ||
        m_params.testType == SecondaryCopyType::SAMPLER_HEAP_COMMAND_COPY)
    {
        VkMemoryBarrier2 preTransferBarrier = initVulkanStructure();
        preTransferBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
        preTransferBarrier.srcAccessMask    = VK_ACCESS_2_HOST_WRITE_BIT;
        preTransferBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        preTransferBarrier.dstAccessMask    = VK_ACCESS_2_TRANSFER_READ_BIT;

        VkDependencyInfo dependencyInfo   = initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;

        dependencyInfo.pMemoryBarriers = &preTransferBarrier;
        vkd.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        VkBufferCopy descriptorCopy;
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY)
        {
            descriptorCopy.srcOffset = m_imageHeapCopySrcIndex * resourceStride;
            descriptorCopy.dstOffset = m_imageHeapIndex * resourceStride;
            descriptorCopy.size      = resourceStride;
            vkd.cmdCopyBuffer(commandBuffer, *m_srcCopyBuffer->buffer, *m_resourceHeap->buffer, 1u, &descriptorCopy);
        }
        else
        {
            descriptorCopy.srcOffset = m_samplerHeapCopySrcIndex * samplerStride;
            descriptorCopy.dstOffset = m_samplerHeapIndex * samplerStride;
            descriptorCopy.size      = samplerStride;
            vkd.cmdCopyBuffer(commandBuffer, *m_srcCopyBuffer->buffer, *m_samplerHeap->buffer, 1u, &descriptorCopy);
        }

        VkMemoryBarrier2 postTransferBarrier = initVulkanStructure();
        postTransferBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        postTransferBarrier.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        postTransferBarrier.dstStageMask     = m_params.queue == VK_QUEUE_COMPUTE_BIT ?
                                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT :
                                                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY)
            postTransferBarrier.dstAccessMask = VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT;
        else
            postTransferBarrier.dstAccessMask = VK_ACCESS_2_SAMPLER_HEAP_READ_BIT_EXT;

        dependencyInfo.pMemoryBarriers = &postTransferBarrier;
        vkd.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
    else
    {
        const bool resourceCopy = m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY;
        const uint32_t copyStride =
            static_cast<uint32_t>(resourceCopy ? resourceStride : samplerStride) / sizeof(uint32_t);

        VkMemoryBarrier2 preCopyBarrier = initVulkanStructure();
        preCopyBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
        preCopyBarrier.srcAccessMask    = VK_ACCESS_2_HOST_WRITE_BIT;
        preCopyBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        preCopyBarrier.dstAccessMask    = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        VkDependencyInfo dependencyInfo   = initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;

        dependencyInfo.pMemoryBarriers = &preCopyBarrier;
        vkd.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_copyPipeline);

        uint32_t indices[2];
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY)
        {
            indices[0] = m_imageHeapCopySrcIndex * copyStride;
            indices[1] = m_imageHeapIndex * copyStride;
        }
        else
        {
            indices[0] = m_samplerHeapCopySrcIndex * copyStride;
            indices[1] = m_samplerHeapIndex * copyStride;
        }

        VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
        pushDataInfo.offset            = 0u;
        pushDataInfo.data.address      = indices;
        pushDataInfo.data.size         = sizeof(uint32_t) * 2u;
        vkd.cmdPushDataEXT(commandBuffer, &pushDataInfo);
        vkd.cmdDispatch(commandBuffer, copyStride, 1u, 1u);

        VkMemoryBarrier2 postCopyBarrier = initVulkanStructure();
        postCopyBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        postCopyBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        postCopyBarrier.dstStageMask = m_params.queue == VK_QUEUE_COMPUTE_BIT ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT :
                                                                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY)
            postCopyBarrier.dstAccessMask = VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT;
        else
            postCopyBarrier.dstAccessMask = VK_ACCESS_2_SAMPLER_HEAP_READ_BIT_EXT;

        dependencyInfo.pMemoryBarriers = &postCopyBarrier;
        vkd.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
}

tcu::TestStatus DescriptorHeapTestInstanceSecondary::iterate()
{
    const auto &vkd = m_device.getDriver();

    const VkFormat imageFormat          = VK_FORMAT_R8G8B8A8_UNORM;
    const VkDeviceSize bufferStride     = getBufferDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize imageStride      = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize samplerStride    = getSamplerDescriptorStride(m_descriptorHeapProperties);
    const float expectedColor[4]        = {0.8f, 0.4f, 0.2f, 0.6f};
    const uint8_t expectedColorUint8[4] = {
        static_cast<uint8_t>(expectedColor[0] * 255), static_cast<uint8_t>(expectedColor[1] * 255),
        static_cast<uint8_t>(expectedColor[2] * 255), static_cast<uint8_t>(expectedColor[3] * 255)};
    const uint32_t imageSize                            = 32u;
    const VkImageSubresourceRange imageSubresourceRange = makeDefaultImageSubresourceRange();

    const VkDeviceSize resourceDescriptorCount = m_imageHeapIndex + 1u;
    const VkDeviceSize resourceUserHeapSize =
        alignUp(resourceDescriptorCount * imageStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize =
        resourceUserHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    const VkBufferUsageFlags2KHR heapUsage =
        VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR;
    m_resourceHeap = createBufferAndMemory(resourceHeapSize, heapUsage);

    const VkDeviceSize samplerDescriptorCount = m_samplerHeapIndex + 1u;
    const VkDeviceSize samplerUserHeapSize =
        alignUp(samplerDescriptorCount * samplerStride, m_descriptorHeapProperties.samplerHeapAlignment);
    const VkDeviceSize samplerHeapSize = samplerUserHeapSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;
    m_samplerHeap                      = createBufferAndMemory(samplerHeapSize, heapUsage);

    uint8_t *srcCopyBufferPtr   = nullptr;
    VkDeviceSize copyBufferSize = 0u;
    if (m_params.testType != SecondaryCopyType::NONE)
    {
        const bool copyResource = m_params.testType == SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY ||
                                  m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY;

        copyBufferSize  = copyResource ? imageStride * (m_imageHeapCopySrcIndex + 1) :
                                         samplerStride * (m_samplerHeapCopySrcIndex + 1);
        m_srcCopyBuffer = createBufferAndMemory(copyBufferSize, VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                                                    VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT |
                                                                    VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);

        srcCopyBufferPtr = static_cast<uint8_t *>(m_srcCopyBuffer->memory->getHostPtr());

        VkDeviceAddressRangeEXT dstAddressRange;
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY)
            dstAddressRange.address = m_resourceHeap->address;
        else
            dstAddressRange.address = m_samplerHeap->address;
        dstAddressRange.size = copyResource ? imageStride : samplerStride;

        VkResourceDescriptorInfoEXT bufferResourceInfo = initVulkanStructure();
        bufferResourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferResourceInfo.data.pAddressRange          = &dstAddressRange;

        VkHostAddressRangeEXT hostRange;
        hostRange.address = srcCopyBufferPtr;
        hostRange.size    = static_cast<size_t>(bufferStride);
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &bufferResourceInfo, &hostRange));
    }

    VkBindHeapInfoEXT resourceHeapBindInfo   = initVulkanStructure();
    resourceHeapBindInfo.heapRange.address   = m_resourceHeap->address;
    resourceHeapBindInfo.heapRange.size      = resourceHeapSize;
    resourceHeapBindInfo.reservedRangeOffset = resourceUserHeapSize;
    resourceHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerHeapBindInfo   = initVulkanStructure();
    samplerHeapBindInfo.heapRange.address   = m_samplerHeap->address;
    samplerHeapBindInfo.heapRange.size      = samplerHeapSize;
    samplerHeapBindInfo.reservedRangeOffset = samplerUserHeapSize;
    samplerHeapBindInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;

    VkImageCreateInfo sampledImageCreateInfo = initVulkanStructure();
    sampledImageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    sampledImageCreateInfo.format            = imageFormat;
    sampledImageCreateInfo.extent            = {imageSize, imageSize, 1u};
    sampledImageCreateInfo.mipLevels         = 1u;
    sampledImageCreateInfo.arrayLayers       = 1u;
    sampledImageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    sampledImageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    sampledImageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    auto sampledImage                        = createImageAndMemory(sampledImageCreateInfo);

    VkImageViewCreateInfo sampledImageViewCreateInfo = initVulkanStructure();
    sampledImageViewCreateInfo.image                 = *sampledImage->image;
    sampledImageViewCreateInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
    sampledImageViewCreateInfo.format                = imageFormat;
    sampledImageViewCreateInfo.components            = makeComponentMappingRGBA();
    sampledImageViewCreateInfo.subresourceRange      = imageSubresourceRange;

    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.pView                    = &sampledImageViewCreateInfo;
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;

    VkResourceDescriptorInfoEXT sampledImageResourceInfo = initVulkanStructure();
    sampledImageResourceInfo.type                        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sampledImageResourceInfo.data.pImage                 = &imageDescriptorInfo;

    uint8_t *resourceHeapHostPtr = static_cast<uint8_t *>(m_resourceHeap->memory->getHostPtr());

    VkHostAddressRangeEXT sampledImageHostRange{};
    if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY ||
        m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY)
    {
        sampledImageHostRange.address = srcCopyBufferPtr + m_imageHeapCopySrcIndex * imageStride;
    }
    else
    {
        sampledImageHostRange.address = resourceHeapHostPtr + m_imageHeapIndex * imageStride;
    }
    sampledImageHostRange.size = static_cast<size_t>(imageStride);
    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &sampledImageResourceInfo, &sampledImageHostRange));

    VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
    samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias              = 0.0f;
    samplerCreateInfo.anisotropyEnable        = VK_FALSE;
    samplerCreateInfo.maxAnisotropy           = 1.0f;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                  = 0.0f;
    samplerCreateInfo.maxLod                  = 0.0f;
    samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    uint8_t *samplerHeapHostPtr = static_cast<uint8_t *>(m_samplerHeap->memory->getHostPtr());

    VkHostAddressRangeEXT samplerHostRange{};
    if (m_params.testType == SecondaryCopyType::SAMPLER_HEAP_COMMAND_COPY ||
        m_params.testType == SecondaryCopyType::SAMPLER_HEAP_SHADER_COPY)
    {
        samplerHostRange.address = srcCopyBufferPtr + m_samplerHeapCopySrcIndex * samplerStride;
    }
    else
    {
        samplerHostRange.address = samplerHeapHostPtr + m_samplerHeapIndex * samplerStride;
    }
    samplerHostRange.size = static_cast<size_t>(samplerStride);
    VK_CHECK(vkd.writeSamplerDescriptorsEXT(*m_device, 1u, &samplerCreateInfo, &samplerHostRange));

    if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY ||
        m_params.testType == SecondaryCopyType::SAMPLER_HEAP_SHADER_COPY)
    {
        VkDeviceAddressRangeEXT addressRanges[2];
        addressRanges[0].address = m_srcCopyBuffer->address;
        addressRanges[0].size    = copyBufferSize;
        if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY)
        {
            addressRanges[1].address = m_resourceHeap->address;
            addressRanges[1].size    = resourceUserHeapSize;
        }
        else
        {
            addressRanges[1].address = m_samplerHeap->address;
            addressRanges[1].size    = samplerUserHeapSize;
        }

        VkResourceDescriptorInfoEXT bufferResourceInfos[2];
        bufferResourceInfos[0]                    = initVulkanStructure();
        bufferResourceInfos[0].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferResourceInfos[0].data.pAddressRange = &addressRanges[0];
        bufferResourceInfos[1]                    = initVulkanStructure();
        bufferResourceInfos[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferResourceInfos[1].data.pAddressRange = &addressRanges[1];

        VkHostAddressRangeEXT hostRanges[2];
        hostRanges[0].address = resourceHeapHostPtr + bufferStride * m_copyBufferIndex;
        hostRanges[0].size    = static_cast<size_t>(bufferStride);
        hostRanges[1].address = resourceHeapHostPtr + bufferStride * m_dstCopyIndex;
        hostRanges[1].size    = static_cast<size_t>(bufferStride);
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 2, bufferResourceInfos, hostRanges));

        auto copyModule = createShaderModule(vkd, *m_device, getShaderBinary("copy"));

        VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();
        pipelineFlags.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

        VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineFlags);
        pipelineCreateInfo.stage                       = initVulkanStructure();
        pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.module                = *copyModule;
        pipelineCreateInfo.stage.pName                 = "main";
        m_copyPipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    flushAlloc(vkd, *m_device, *m_resourceHeap->memory);
    flushAlloc(vkd, *m_device, *m_samplerHeap->memory);
    if (srcCopyBufferPtr)
        flushAlloc(vkd, *m_device, *m_srcCopyBuffer->memory);

    auto cmdPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();
    auto secondaryCmdBufferPtr =
        allocateCommandBuffer(vkd, *m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    auto secondaryCmdBuffer = secondaryCmdBufferPtr.get();

    VkCommandBufferInheritanceDescriptorHeapInfoEXT inheritance_heap_info = initVulkanStructure();
    inheritance_heap_info.pResourceHeapBindInfo                           = &resourceHeapBindInfo;
    inheritance_heap_info.pSamplerHeapBindInfo                            = &samplerHeapBindInfo;
    VkCommandBufferInheritanceInfo inheritance_info                       = initVulkanStructure(&inheritance_heap_info);
    VkCommandBufferBeginInfo begin_info                                   = initVulkanStructure();
    begin_info.flags                                                      = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo                                           = &inheritance_info;

    beginCommandBuffer(vkd, cmdBuffer);

    vkd.cmdBindResourceHeapEXT(cmdBuffer, &resourceHeapBindInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapBindInfo);

    {
        VkImageMemoryBarrier2 barrier = initVulkanStructure();
        barrier.srcStageMask          = VK_PIPELINE_STAGE_2_NONE;
        barrier.srcAccessMask         = 0u;
        barrier.dstStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                 = *sampledImage->image;
        barrier.subresourceRange      = imageSubresourceRange;

        VkDependencyInfo dependencyInfo        = initVulkanStructure();
        dependencyInfo.imageMemoryBarrierCount = 1u;
        dependencyInfo.pImageMemoryBarriers    = &barrier;
        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);

        VkClearColorValue clearColor{};
        clearColor.float32[0] = expectedColor[0];
        clearColor.float32[1] = expectedColor[1];
        clearColor.float32[2] = expectedColor[2];
        clearColor.float32[3] = expectedColor[3];

        vkd.cmdClearColorImage(cmdBuffer, *sampledImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u,
                               &imageSubresourceRange);

        VkPipelineStageFlags2 dstStageMask = (m_params.queue == VK_QUEUE_COMPUTE_BIT) ?
                                                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT :
                                                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask  = dstStageMask;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
    }

    if (m_params.queue == VK_QUEUE_COMPUTE_BIT)
    {
        const uint32_t bufferSize = sizeof(float) * 4u;
        auto outputBuffer         = createBufferAndMemory(bufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                          VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
        deMemset(outputBuffer->memory->getHostPtr(), 0, bufferSize);

        VkDeviceAddressRangeEXT outputBufferAddressRange{};
        outputBufferAddressRange.address                     = outputBuffer->address;
        outputBufferAddressRange.size                        = bufferSize;
        VkResourceDescriptorInfoEXT outputBufferResourceInfo = initVulkanStructure();
        outputBufferResourceInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outputBufferResourceInfo.data.pAddressRange          = &outputBufferAddressRange;

        VkHostAddressRangeEXT outputBufferHostRange{};
        outputBufferHostRange.address = resourceHeapHostPtr + bufferStride * m_resultBufferIndex;
        outputBufferHostRange.size    = static_cast<size_t>(bufferStride);
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1u, &outputBufferResourceInfo, &outputBufferHostRange));
        flushAlloc(vkd, *m_device, *m_resourceHeap->memory);

        auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

        VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();
        pipelineFlags.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

        VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
        pipelineCreateInfo.pNext                       = &pipelineFlags;
        pipelineCreateInfo.stage                       = initVulkanStructure();
        pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.module                = *computeModule;
        pipelineCreateInfo.stage.pName                 = "main";

        auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

        VK_CHECK(vkd.beginCommandBuffer(secondaryCmdBuffer, &begin_info));
        if (m_params.copyInSecondary)
            copyDescriptor(secondaryCmdBuffer);
        vkd.cmdBindPipeline(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vkd.cmdDispatch(secondaryCmdBuffer, 1u, 1u, 1u);
        VK_CHECK(vkd.endCommandBuffer(secondaryCmdBuffer));

        if (!m_params.copyInSecondary)
            copyDescriptor(cmdBuffer);
        vkd.cmdExecuteCommands(cmdBuffer, 1u, &secondaryCmdBuffer);

        VkMemoryBarrier2 barrier = initVulkanStructure();
        barrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
        barrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

        VkDependencyInfo dependencyInfo   = initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;
        dependencyInfo.pMemoryBarriers    = &barrier;
        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);

        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, *m_device, m_queues[0], cmdBuffer);
        invalidateAlloc(vkd, *m_device, *outputBuffer->memory);

        float *result = static_cast<float *>(outputBuffer->memory->getHostPtr());
        const float e = 0.01f;
        if (std::abs(result[0] - expectedColor[0]) > e || std::abs(result[1] - expectedColor[1]) > e ||
            std::abs(result[2] - expectedColor[2]) > e || std::abs(result[3] - expectedColor[3]) > e)
        {
            std::stringstream stream;
            stream << "Expected " << tcu::Vec4(expectedColor[0], expectedColor[1], expectedColor[2], expectedColor[3])
                   << ", but result is " << tcu::Vec4(result[0], result[1], result[2], result[3]);
            return tcu::TestStatus::fail(stream.str());
        }
    }
    else
    {
        VkImageCreateInfo colorImageCreateInfo = initVulkanStructure();
        colorImageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
        colorImageCreateInfo.format            = imageFormat;
        colorImageCreateInfo.extent            = {imageSize, imageSize, 1u};
        colorImageCreateInfo.mipLevels         = 1u;
        colorImageCreateInfo.arrayLayers       = 1u;
        colorImageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        colorImageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        colorImageCreateInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        colorImageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        colorImageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

        auto colorImage = createImageAndMemory(colorImageCreateInfo);

        VkImageViewCreateInfo colorImageViewCreateInfo = initVulkanStructure();
        colorImageViewCreateInfo.image                 = *colorImage->image;
        colorImageViewCreateInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        colorImageViewCreateInfo.format                = imageFormat;
        colorImageViewCreateInfo.components            = makeComponentMappingRGBA();
        colorImageViewCreateInfo.subresourceRange      = imageSubresourceRange;

        auto colorImageView = createImageView(vkd, *m_device, &colorImageViewCreateInfo);

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = imageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0u;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1u;
        subpass.pColorAttachments    = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassCreateInfo = initVulkanStructure();
        renderPassCreateInfo.attachmentCount        = 1u;
        renderPassCreateInfo.pAttachments           = &colorAttachment;
        renderPassCreateInfo.subpassCount           = 1u;
        renderPassCreateInfo.pSubpasses             = &subpass;

        auto renderPass = createRenderPass(vkd, *m_device, &renderPassCreateInfo);

        VkFramebufferCreateInfo framebufferCreateInfo = initVulkanStructure();
        framebufferCreateInfo.renderPass              = *renderPass;
        framebufferCreateInfo.attachmentCount         = 1u;
        framebufferCreateInfo.pAttachments            = &colorImageView.get();
        framebufferCreateInfo.width                   = imageSize;
        framebufferCreateInfo.height                  = imageSize;
        framebufferCreateInfo.layers                  = 1u;

        auto framebuffer = createFramebuffer(vkd, *m_device, &framebufferCreateInfo);

        auto vertexModule   = createShaderModule(vkd, *m_device, getShaderBinary("vertex"));
        auto fragmentModule = createShaderModule(vkd, *m_device, getShaderBinary("fragment"));

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {{
            initVulkanStructure(),
            initVulkanStructure(),
        }};
        shaderStages[0].stage                                       = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module                                      = *vertexModule;
        shaderStages[0].pName                                       = "main";
        shaderStages[1].stage                                       = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module                                      = *fragmentModule;
        shaderStages[1].pName                                       = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
        inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        VkViewport viewport = makeViewport(imageSize, imageSize);
        VkRect2D scissor    = makeRect2D(0, 0, imageSize, imageSize);

        VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
        viewportState.viewportCount                     = 1u;
        viewportState.pViewports                        = &viewport;
        viewportState.scissorCount                      = 1u;
        viewportState.pScissors                         = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
        rasterizationState.lineWidth                              = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
        multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
        colorBlendState.attachmentCount                     = 1u;
        colorBlendState.pAttachments                        = &colorBlendAttachment;

        VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();
        pipelineFlags.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineFlags);
        pipelineCreateInfo.stageCount                   = de::sizeU32(shaderStages);
        pipelineCreateInfo.pStages                      = shaderStages.data();
        pipelineCreateInfo.pVertexInputState            = &vertexInputState;
        pipelineCreateInfo.pInputAssemblyState          = &inputAssemblyState;
        pipelineCreateInfo.pViewportState               = &viewportState;
        pipelineCreateInfo.pRasterizationState          = &rasterizationState;
        pipelineCreateInfo.pMultisampleState            = &multisampleState;
        pipelineCreateInfo.pColorBlendState             = &colorBlendState;
        pipelineCreateInfo.renderPass                   = *renderPass;

        auto pipeline = createGraphicsPipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

        inheritance_info.renderPass  = *renderPass;
        inheritance_info.framebuffer = *framebuffer;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

        VK_CHECK(vkd.beginCommandBuffer(secondaryCmdBuffer, &begin_info));
        if (m_params.copyInSecondary)
            copyDescriptor(secondaryCmdBuffer);
        vkd.cmdBindPipeline(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vkd.cmdDraw(secondaryCmdBuffer, 4u, 1u, 0u, 0u);
        vkd.endCommandBuffer(secondaryCmdBuffer);

        uint32_t outputBufferSize = imageSize * imageSize * 4;
        auto outputBuffer         = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR);
        deMemset(outputBuffer->memory->getHostPtr(), 0, outputBufferSize);

        VkRenderPassBeginInfo renderPassBeginInfo = initVulkanStructure();
        renderPassBeginInfo.renderPass            = *renderPass;
        renderPassBeginInfo.framebuffer           = *framebuffer;
        renderPassBeginInfo.renderArea            = makeRect2D(0, 0, imageSize, imageSize);

        VkClearValue clearValue{};
        renderPassBeginInfo.clearValueCount = 1u;
        renderPassBeginInfo.pClearValues    = &clearValue;

        if (!m_params.copyInSecondary)
            copyDescriptor(cmdBuffer);
        vkd.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkd.cmdExecuteCommands(cmdBuffer, 1u, &secondaryCmdBuffer);
        vkd.cmdEndRenderPass(cmdBuffer);

        VkImageMemoryBarrier2 imageMemoryBarrier = initVulkanStructure();
        imageMemoryBarrier.srcStageMask          = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageMemoryBarrier.srcAccessMask         = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.dstStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
        imageMemoryBarrier.dstAccessMask         = VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
        imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image                 = *colorImage->image;
        imageMemoryBarrier.subresourceRange      = makeDefaultImageSubresourceRange();

        VkDependencyInfo dependencyInfo        = initVulkanStructure();
        dependencyInfo.imageMemoryBarrierCount = 1u;
        dependencyInfo.pImageMemoryBarriers    = &imageMemoryBarrier;
        vkd.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource = makeDefaultImageSubresourceLayers();
        copyRegion.imageExtent      = {imageSize, imageSize, 1u};

        vkd.cmdCopyImageToBuffer(cmdBuffer, *colorImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 *outputBuffer->buffer, 1u, &copyRegion);

        endCommandBuffer(vkd, cmdBuffer);
        submitCommandsAndWait(vkd, *m_device, m_queues[0], cmdBuffer);
        invalidateAlloc(vkd, *m_device, *outputBuffer->memory);

        uint8_t *result = static_cast<uint8_t *>(outputBuffer->memory->getHostPtr());
        const uint8_t e = 1u;

        for (uint32_t i = 0; i < imageSize * imageSize; ++i)
        {
            uint8_t r = result[i * 4 + 0];
            uint8_t g = result[i * 4 + 1];
            uint8_t b = result[i * 4 + 2];
            uint8_t a = result[i * 4 + 3];
            if (std::abs(r - expectedColorUint8[0]) > e || std::abs(g - expectedColorUint8[1]) > e ||
                std::abs(b - expectedColorUint8[2]) > e || std::abs(a - expectedColorUint8[3]) > e)
            {
                tcu::TestLog &log = m_context.getTestContext().getLog();
                std::stringstream errorMsg;
                errorMsg << "image is in resource heap at index " << m_imageHeapIndex << "\n"
                         << "sampler is in sampler heap at index " << m_samplerHeapIndex << "\n";
                if (m_params.testType != SecondaryCopyType::NONE)
                {
                    errorMsg << "image was copied from second resource heap at index " << m_imageHeapCopySrcIndex
                             << "\n"
                             << "sampler was copied from second resource heap at index " << m_samplerHeapCopySrcIndex
                             << "\n"
                             << "copy buffer is in second resource heap at index " << m_copyBufferIndex << "\n"
                             << "dst heap buffer is in second resource heap at index " << m_dstCopyIndex;
                }
                errorMsg << "result buffer is in resouce heap at index " << m_resultBufferIndex << "\n";
                log << tcu::TestLog::Message << errorMsg.str() << tcu::TestLog::EndMessage;

                std::stringstream stream;
                stream << "Pixel " << i << ": expected (" << (uint32_t)expectedColorUint8[0] << ", "
                       << (uint32_t)expectedColorUint8[1] << ", " << (uint32_t)expectedColorUint8[2] << ", "
                       << (uint32_t)expectedColorUint8[3] << ") but got (" << (uint32_t)r << ", " << (uint32_t)g << ", "
                       << (uint32_t)b << ", " << (uint32_t)a << ")";
                return tcu::TestStatus::fail(stream.str());
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestCaseSecondary final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseSecondary(tcu::TestContext &testCtx, const std::string &name,
                                             const SecondaryTestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceSecondary(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    SecondaryTestParams m_params;
};

void DescriptorHeapTestCaseSecondary::initPrograms(vk::SourceCollections &programCollection) const
{
    if (m_params.queue == VK_QUEUE_COMPUTE_BIT)
    {
        const char *const computeShader = R"(#version 450
#extension GL_EXT_descriptor_heap : require
layout(descriptor_heap) uniform texture2D heapTextures[];
layout(descriptor_heap) uniform sampler heapSamplers[];
layout(descriptor_heap) buffer ssbo {
	vec4 data;
} heapBuffer[];
void main() {
	heapBuffer[0].data = texture(sampler2D(heapTextures[16], heapSamplers[29]), vec2(0.5f));
}
)";

        vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);
        programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << options;
    }
    else
    {
        const char *const vertexShader = R"(#version 450

layout(location = 0) out vec2 uv;

void main() {
    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
    gl_Position = vec4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    uv = pos;
}
)";

        const char *const fragmentShader = R"(#version 450
#extension GL_EXT_descriptor_heap : require
layout(descriptor_heap) uniform texture2D heapTextures[];
layout(descriptor_heap) uniform sampler heapSamplers[];

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    color = texture(sampler2D(heapTextures[16], heapSamplers[29]), uv);
}
)";

        vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);
        programCollection.glslSources.add("vertex") << glu::VertexSource(vertexShader) << options;
        programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentShader) << options;
    }

    if (m_params.testType == SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY ||
        m_params.testType == SecondaryCopyType::SAMPLER_HEAP_SHADER_COPY)
    {
        const char *const copyShader = R"(#version 450
#extension GL_EXT_descriptor_heap : require
layout(descriptor_heap, std430) buffer Buffer {
    uint data[];
} ssbos[];

layout(push_constant) uniform PushConstants {
    uint src;
    uint dst;
} pc;

void main() {
    ssbos[2].data[pc.dst + gl_GlobalInvocationID.x] = ssbos[1].data[pc.src + gl_GlobalInvocationID.x];
})";

        vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0);
        programCollection.glslSources.add("copy") << glu::ComputeSource(copyShader) << options;
    }
}

class DescriptorHeapTestInstanceShaderObjectInvariance final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceShaderObjectInvariance(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    Move<VkShaderEXT> compileShader(VkBorderColor borderColor);
    Move<VkShaderEXT> restoreShader(const std::vector<char> &binaryData);
    std::vector<char> getBinary(VkShaderEXT shader);
    tcu::UVec4 runShader(VkShaderEXT shader);

    TestParams m_params;
    std::unique_ptr<Buffer> m_samplerHeap;
};

class DescriptorHeapTestCaseShaderObjectInvariance final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseShaderObjectInvariance(tcu::TestContext &testCtx, const std::string &name,
                                                          const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceShaderObjectInvariance(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCaseShaderObjectInvariance::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
layout(local_size_x = 1) in;
layout(binding = 0) uniform usampler2D tex;
layout(binding = 1) buffer OutputBuffer {
    uvec4 result;
};

void main()
{
    result = textureLod(tex, vec2(-1.0, 0.5), 0);
}
)";
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader);
}

Move<VkShaderEXT> DescriptorHeapTestInstanceShaderObjectInvariance::compileShader(VkBorderColor borderColor)
{
    const auto &vkd = m_device.getDriver();

    const auto &spirvBinary = getShaderBinary("compute");

    VkSamplerCreateInfo samplerCreateInfo     = initVulkanStructure();
    samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias              = 0.0f;
    samplerCreateInfo.anisotropyEnable        = VK_FALSE;
    samplerCreateInfo.maxAnisotropy           = 1.0f;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                  = 0.0f;
    samplerCreateInfo.maxLod                  = 1000.0f;
    samplerCreateInfo.borderColor             = borderColor;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    std::array<VkDescriptorSetAndBindingMappingEXT, 2> mappings{};

    mappings[0]                                            = initVulkanStructure();
    mappings[0].bindingCount                               = 1;
    mappings[0].descriptorSet                              = 0;
    mappings[0].firstBinding                               = 0;
    mappings[0].resourceMask                               = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[0].source                                     = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[0].sourceData.constantOffset.pEmbeddedSampler = &samplerCreateInfo;

    mappings[1]                              = initVulkanStructure();
    mappings[1].bindingCount                 = 1;
    mappings[1].descriptorSet                = 0;
    mappings[1].firstBinding                 = 1;
    mappings[1].resourceMask                 = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[1].source                       = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    mappings[1].sourceData.pushAddressOffset = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = de::sizeU32(mappings);
    mappingInfo.pMappings                                     = mappings.data();

    VkShaderCreateInfoEXT shaderCreateInfo = initVulkanStructure();
    shaderCreateInfo.pNext                 = &mappingInfo;
    shaderCreateInfo.flags                 = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;
    shaderCreateInfo.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderCreateInfo.nextStage             = 0;
    shaderCreateInfo.codeType              = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    shaderCreateInfo.codeSize              = spirvBinary.getSize();
    shaderCreateInfo.pCode                 = spirvBinary.getBinary();
    shaderCreateInfo.pName                 = "main";

    return createShader(vkd, *m_device, shaderCreateInfo);
}

Move<VkShaderEXT> DescriptorHeapTestInstanceShaderObjectInvariance::restoreShader(const std::vector<char> &binaryData)
{
    const auto &vkd = m_device.getDriver();

    VkShaderCreateInfoEXT shaderCreateInfo = initVulkanStructure();
    shaderCreateInfo.flags                 = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;
    shaderCreateInfo.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderCreateInfo.nextStage             = 0;
    shaderCreateInfo.codeType              = VK_SHADER_CODE_TYPE_BINARY_EXT;
    shaderCreateInfo.codeSize              = binaryData.size();
    shaderCreateInfo.pCode                 = binaryData.data();
    shaderCreateInfo.pName                 = "main";

    return createShader(vkd, *m_device, shaderCreateInfo);
}

std::vector<char> DescriptorHeapTestInstanceShaderObjectInvariance::getBinary(VkShaderEXT shader)
{
    const auto &vkd = m_device.getDriver();

    size_t dataSize = 0;
    VK_CHECK(vkd.getShaderBinaryDataEXT(*m_device, shader, &dataSize, nullptr));

    std::vector<char> result(dataSize);
    VK_CHECK(vkd.getShaderBinaryDataEXT(*m_device, shader, &dataSize, result.data()));

    return result;
}

tcu::UVec4 DescriptorHeapTestInstanceShaderObjectInvariance::runShader(VkShaderEXT shader)
{
    const auto &vkd = m_device.getDriver();

    auto buffer = createBufferAndMemory(4 * sizeof(uint32_t), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkDeviceSize const userHeapSize =
        alignUp(m_descriptorHeapProperties.imageDescriptorSize, m_descriptorHeapProperties.resourceHeapAlignment);
    VkDeviceSize const totalHeapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto resourceHeap                = createBufferAndMemory(totalHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = VK_FORMAT_R32G32B32A32_UINT;
    imageCreateInfo.extent            = {1, 1, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

    auto image = createImageAndMemory(imageCreateInfo);

    VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
    imageViewCreateInfo.image                           = *image->image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R32G32B32A32_UINT;
    imageViewCreateInfo.components                      = makeComponentMappingRGBA();
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
    imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageDescriptorInfo.pView                    = &imageViewCreateInfo;

    VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
    resourceInfo.type                        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resourceInfo.data.pImage                 = &imageDescriptorInfo;

    VkHostAddressRangeEXT resourceHostRange{};
    resourceHostRange.address = resourceHeap->memory->getHostPtr();
    resourceHostRange.size    = static_cast<size_t>(m_descriptorHeapProperties.imageDescriptorSize);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &resourceHostRange));

    auto commandPool  = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBufferPtr = allocateCommandBuffer(vkd, *m_device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuffer    = cmdBufferPtr.get();

    VkCommandBufferBeginInfo cmdbufBeginInfo = initVulkanStructure();
    cmdbufBeginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImageMemoryBarrier2 setupImageMemoryBarrier           = initVulkanStructure();
    setupImageMemoryBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    setupImageMemoryBarrier.srcAccessMask                   = VK_ACCESS_2_NONE;
    setupImageMemoryBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    setupImageMemoryBarrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    setupImageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    setupImageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    setupImageMemoryBarrier.image                           = *image->image;
    setupImageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    setupImageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
    setupImageMemoryBarrier.subresourceRange.levelCount     = 1;
    setupImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    setupImageMemoryBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo setupDependencyInfo        = initVulkanStructure();
    setupDependencyInfo.imageMemoryBarrierCount = 1;
    setupDependencyInfo.pImageMemoryBarriers    = &setupImageMemoryBarrier;

    VkImageMemoryBarrier2 clearImageMemoryBarrier           = initVulkanStructure();
    clearImageMemoryBarrier.srcStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    clearImageMemoryBarrier.srcAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    clearImageMemoryBarrier.dstStageMask                    = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    clearImageMemoryBarrier.dstAccessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    clearImageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    clearImageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    clearImageMemoryBarrier.image                           = *image->image;
    clearImageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    clearImageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
    clearImageMemoryBarrier.subresourceRange.levelCount     = 1;
    clearImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    clearImageMemoryBarrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo clearDependencyInfo        = initVulkanStructure();
    clearDependencyInfo.imageMemoryBarrierCount = 1;
    clearDependencyInfo.pImageMemoryBarriers    = &clearImageMemoryBarrier;

    VkShaderStageFlagBits const stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = resourceHeap->address;
    bindHeapInfo.heapRange.size      = totalHeapSize;
    bindHeapInfo.reservedRangeOffset = userHeapSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerHeapInfo = initVulkanStructure();
    if (m_samplerHeap)
    {
        samplerHeapInfo.heapRange.address   = m_samplerHeap->address;
        samplerHeapInfo.heapRange.size      = m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;
        samplerHeapInfo.reservedRangeOffset = 0;
        samplerHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded;
    }

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.data.address      = &buffer->address;
    pushDataInfo.data.size         = sizeof(VkDeviceAddress);
    pushDataInfo.offset            = 0;

    VkClearColorValue clearColor = {};

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo postDependencyInfo   = initVulkanStructure();
    postDependencyInfo.memoryBarrierCount = 1;
    postDependencyInfo.pMemoryBarriers    = &memoryBarrier;

    vkd.beginCommandBuffer(cmdBuffer, &cmdbufBeginInfo);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &bindHeapInfo);
    vkd.cmdBindSamplerHeapEXT(cmdBuffer, &samplerHeapInfo);
    vkd.cmdBindShadersEXT(cmdBuffer, 1, &stageFlags, &shader);
    vkd.cmdPushDataEXT(cmdBuffer, &pushDataInfo);
    vkd.cmdPipelineBarrier2(cmdBuffer, &setupDependencyInfo);
    vkd.cmdClearColorImage(cmdBuffer, *image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                           &setupImageMemoryBarrier.subresourceRange);
    vkd.cmdPipelineBarrier2(cmdBuffer, &clearDependencyInfo);
    vkd.cmdDispatch(cmdBuffer, 1, 1, 1);
    vkd.cmdPipelineBarrier2(cmdBuffer, &postDependencyInfo);
    vkd.endCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.queueWaitIdle(m_queues.front()));

    tcu::UVec4 result{};
    deMemcpy(&result, buffer->memory->getHostPtr(), sizeof(result));
    return result;
}

tcu::TestStatus DescriptorHeapTestInstanceShaderObjectInvariance::iterate()
{
    if (m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded > 0)
    {
        m_samplerHeap = createBufferAndMemory(m_descriptorHeapProperties.minSamplerHeapReservedRangeWithEmbedded,
                                              VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                  VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    }

    std::vector<char> opaqueBlackBinary1;
    std::vector<char> opaqueWhiteBinary1;

    {
        auto opaqueBlackShader = compileShader(VK_BORDER_COLOR_INT_OPAQUE_BLACK);
        auto opaqueWhiteShader = compileShader(VK_BORDER_COLOR_INT_OPAQUE_WHITE);

        opaqueBlackBinary1 = getBinary(*opaqueBlackShader);
        opaqueWhiteBinary1 = getBinary(*opaqueWhiteShader);

        if (opaqueBlackBinary1 == opaqueWhiteBinary1)
        {
            return tcu::TestStatus::fail("Opaque black and white shader binaries match");
        }

        auto blackShaderResult = runShader(*opaqueBlackShader);
        auto whiteShaderResult = runShader(*opaqueWhiteShader);

        if (blackShaderResult != tcu::UVec4{0, 0, 0, 1})
        {
            std::stringstream message;
            message << "Basic opaque black shader returned incorrect result: " << blackShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
        if (whiteShaderResult != tcu::UVec4{1, 1, 1, 1})
        {
            std::stringstream message;
            message << "Basic opaque white shader returned incorrect result: " << whiteShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
    }
    {
        // Create shaders again in reverse order
        auto opaqueWhiteShader = compileShader(VK_BORDER_COLOR_INT_OPAQUE_WHITE);
        auto opaqueBlackShader = compileShader(VK_BORDER_COLOR_INT_OPAQUE_BLACK);

        std::vector<char> opaqueWhiteBinary2 = getBinary(*opaqueWhiteShader);
        std::vector<char> opaqueBlackBinary2 = getBinary(*opaqueBlackShader);

        if (opaqueWhiteBinary1 != opaqueWhiteBinary2)
        {
            return tcu::TestStatus::fail("Opaque white shader binaries do not match");
        }
        if (opaqueBlackBinary1 != opaqueBlackBinary2)
        {
            return tcu::TestStatus::fail("Opaque black shader binaries do not match");
        }

        auto blackShaderResult = runShader(*opaqueBlackShader);
        auto whiteShaderResult = runShader(*opaqueWhiteShader);

        if (blackShaderResult != tcu::UVec4{0, 0, 0, 1})
        {
            std::stringstream message;
            message << "Recreated opaque black shader returned incorrect result: " << blackShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
        if (whiteShaderResult != tcu::UVec4{1, 1, 1, 1})
        {
            std::stringstream message;
            message << "Recreated opaque white shader returned incorrect result: " << whiteShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
    }
    {
        // Restore shaders from binary in order (reversed of the previous operation)
        auto opaqueBlackShader = restoreShader(opaqueBlackBinary1);
        auto opaqueWhiteShader = restoreShader(opaqueWhiteBinary1);

        std::vector<char> opaqueWhiteBinary2 = getBinary(*opaqueWhiteShader);
        std::vector<char> opaqueBlackBinary2 = getBinary(*opaqueBlackShader);

        if (opaqueWhiteBinary1 != opaqueWhiteBinary2)
        {
            return tcu::TestStatus::fail("Restored white shader binary do not match");
        }
        if (opaqueBlackBinary1 != opaqueBlackBinary2)
        {
            return tcu::TestStatus::fail("Restored black shader binary do not match");
        }

        auto blackShaderResult = runShader(*opaqueBlackShader);
        auto whiteShaderResult = runShader(*opaqueWhiteShader);

        if (blackShaderResult != tcu::UVec4{0, 0, 0, 1})
        {
            std::stringstream message;
            message << "Restored opaque black shader returned incorrect result: " << blackShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
        if (whiteShaderResult != tcu::UVec4{1, 1, 1, 1})
        {
            std::stringstream message;
            message << "Restored opaque white shader returned incorrect result: " << whiteShaderResult;
            return tcu::TestStatus::fail(message.str());
        }
    }
    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstancePushDataAccess final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstancePushDataAccess(Context &context, const TestParams &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParams m_params;
};

class DescriptorHeapTestCasePushDataAccess final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCasePushDataAccess(tcu::TestContext &testCtx, const std::string &name,
                                                  const TestParams &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstancePushDataAccess(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParams m_params;
};

void DescriptorHeapTestCasePushDataAccess::initPrograms(vk::SourceCollections &programCollection) const
{
    const char *const source = R"(#version 450
#extension GL_EXT_descriptor_heap: require
layout(local_size_x = 1) in;

layout(descriptor_heap) buffer O { uint outputBuffer[]; } buffers[];

layout(constant_id = 0) const int PUSH_LENGTH = 1;

layout(push_constant, std430) uniform P {
    int pushData[PUSH_LENGTH];
};

void main() {
    for (int i = 0; i < PUSH_LENGTH; ++i) {
        buffers[0].outputBuffer[i] = pushData[i];
    }
}
)";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    programCollection.glslSources.add("compute") << glu::ComputeSource(source) << buildOptions;
}

tcu::TestStatus DescriptorHeapTestInstancePushDataAccess::iterate()
{
    const auto &vkd = m_device.getDriver();

    const uint32_t pushDataElements =
        static_cast<uint32_t>(m_descriptorHeapProperties.maxPushDataSize / sizeof(uint32_t));
    const VkDeviceSize pushDataSize = VkDeviceSize{pushDataElements} * VkDeviceSize{sizeof(uint32_t)};

    const VkDeviceSize userHeapSize = alignUp(
        alignUp(m_descriptorHeapProperties.bufferDescriptorSize, m_descriptorHeapProperties.bufferDescriptorAlignment),
        m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    auto resourceHeap           = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    auto outputBuffer = createBufferAndMemory(pushDataSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    deMemset(outputBuffer->memory->getHostPtr(), 0xcc, static_cast<size_t>(pushDataSize));

    VkDeviceAddressRangeEXT outputBufferAddressRange{};
    outputBufferAddressRange.address = outputBuffer->address;
    outputBufferAddressRange.size    = pushDataSize;

    VkResourceDescriptorInfoEXT resource = initVulkanStructure();
    resource.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resource.data.pAddressRange          = &outputBufferAddressRange;

    VkHostAddressRangeEXT hostDescriptor{};
    hostDescriptor.address = resourceHeap->memory->getHostPtr();
    hostDescriptor.size    = static_cast<size_t>(m_descriptorHeapProperties.bufferDescriptorSize);

    VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resource, &hostDescriptor));

    VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2CreateInfo = initVulkanStructure();
    pipelineCreateFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkSpecializationMapEntry mapEntry = {};
    mapEntry.constantID               = 0;
    mapEntry.offset                   = 0;
    mapEntry.size                     = sizeof(uint32_t);

    VkSpecializationInfo specializationInfo = {};
    specializationInfo.dataSize             = sizeof(pushDataElements);
    specializationInfo.pData                = &pushDataElements;
    specializationInfo.mapEntryCount        = 1;
    specializationInfo.pMapEntries          = &mapEntry;

    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkComputePipelineCreateInfo computePipelineCreateInfo = initVulkanStructure();
    computePipelineCreateInfo.pNext                       = &pipelineCreateFlags2CreateInfo;
    computePipelineCreateInfo.stage                       = initVulkanStructure();
    computePipelineCreateInfo.stage.pNext                 = nullptr;
    computePipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module                = *computeModule;
    computePipelineCreateInfo.stage.pName                 = "main";
    computePipelineCreateInfo.stage.pSpecializationInfo   = &specializationInfo;

    auto computePipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &computePipelineCreateInfo);

    de::Random rnd(m_params.seed);
    std::vector<uint32_t> pushData(pushDataElements);
    std::generate(pushData.begin(), pushData.end(), [&rnd] { return rnd.getUint32(); });

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = resourceHeap->address;
    bindHeapInfo.heapRange.size      = heapSize;
    bindHeapInfo.reservedRangeOffset = userHeapSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.data.address      = pushData.data();
    pushDataInfo.data.size         = static_cast<size_t>(pushDataSize);
    pushDataInfo.offset            = 0;

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;

    VkDependencyInfo dependencyInfo   = initVulkanStructure();
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers    = &memoryBarrier;

    auto commandPool      = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto commandBufferPtr = allocateCommandBuffer(vkd, *m_device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto cmdBuf           = commandBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuf);
    vkd.cmdBindResourceHeapEXT(cmdBuf, &bindHeapInfo);
    vkd.cmdPushDataEXT(cmdBuf, &pushDataInfo);
    vkd.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkd.cmdDispatch(cmdBuf, 1, 1, 1);
    vkd.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
    endCommandBuffer(vkd, cmdBuf);

    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuf;

    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    auto outputBufferData = static_cast<char *>(outputBuffer->memory->getHostPtr());

    for (uint32_t element = 0; element < pushDataElements; ++element)
    {
        uint32_t elementData = 0;
        deMemcpy(&elementData, outputBufferData + element * sizeof(uint32_t), sizeof(uint32_t));

        if (elementData != pushData[element])
        {
            std::stringstream message;
            message << "Mismatch at element " << element << ": expected " << pushData[element] << ", got "
                    << elementData;
            return tcu::TestStatus::fail(message.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestInstanceNonUniformAccess final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceNonUniformAccess(Context &context, const TestParamsWithDescriptorType &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsWithDescriptorType m_params{};
};

class DescriptorHeapTestCaseNonUniformAccess final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseNonUniformAccess(tcu::TestContext &testCtx, const std::string &name,
                                                    const TestParamsWithDescriptorType &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceNonUniformAccess(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsWithDescriptorType m_params{};
};

void DescriptorHeapTestCaseNonUniformAccess::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader = R"(#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions: require
#extension GL_EXT_descriptor_heap: require
layout(local_size_x = 64) in;
layout(binding = 0, std430) buffer OutputBuffer {
    uint result[];
};
)";

    switch (static_cast<int>(m_params.descriptorType))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        computeShader += "layout(descriptor_heap) uniform utexture1D descs[];\n";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        computeShader += "layout(descriptor_heap, r32ui) uniform readonly uimage1D descs[];\n";
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        computeShader += "layout(descriptor_heap) uniform utextureBuffer descs[];\n";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        computeShader += "layout(descriptor_heap, r32ui) uniform readonly uimageBuffer descs[];\n";
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        computeShader += "layout(descriptor_heap) uniform UBO { uint data; } descs[];\n";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        computeShader += "layout(descriptor_heap) buffer SSBO { uint data; } descs[];\n";
        break;
    }

    computeShader += R"(
void main()
{
    uint idx = gl_GlobalInvocationID.x;
    uint value = )";
    switch (static_cast<int>(m_params.descriptorType))
    {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        computeShader += "texelFetch(descs[idx], 0, 0).r;\n";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        computeShader += "imageLoad(descs[idx], 0).r;\n";
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        computeShader += "texelFetch(descs[idx], 0).r;\n";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        computeShader += "imageLoad(descs[idx], 0).r;\n";
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        computeShader += "descs[idx].data;\n";
        break;
    }
    computeShader += "    result[idx] = value;\n}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << buildOptions;
}

tcu::TestStatus DescriptorHeapTestInstanceNonUniformAccess::iterate()
{
    const auto &vkd = m_device.getDriver();

    const uint32_t descriptorCount = 64;

    // Get descriptor sizes
    VkDeviceSize descriptorSize      = 0;
    VkDeviceSize descriptorAlignment = 0;
    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        descriptorSize      = m_descriptorHeapProperties.bufferDescriptorSize;
        descriptorAlignment = m_descriptorHeapProperties.bufferDescriptorAlignment;
    }
    else
    {
        descriptorSize      = m_descriptorHeapProperties.imageDescriptorSize;
        descriptorAlignment = m_descriptorHeapProperties.imageDescriptorAlignment;
    }
    const VkDeviceSize descriptorStride = alignUp(descriptorSize, descriptorAlignment);

    // Calculate heap size
    const VkDeviceSize userHeapSize =
        alignUp(descriptorCount * descriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;

    // Create descriptor heap
    auto descriptorHeap = createBufferAndMemory(heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                              VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    // Create output buffer
    const VkDeviceSize outputBufferSize = descriptorCount * sizeof(uint32_t);
    auto outputBuffer                   = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    deMemset(outputBuffer->memory->getHostPtr(), 0, static_cast<size_t>(outputBufferSize));

    // Generate expected data and create resources
    de::Random rnd(m_params.seed);
    std::vector<uint32_t> expectedData(descriptorCount);
    std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<std::unique_ptr<Image>> images;

    const VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(m_instance.getDriver(), m_physDevice);
    const VkDeviceSize bufferSize =
        alignUp(alignUp(VkDeviceSize{sizeof(uint32_t)}, physDevProps.limits.minUniformBufferOffsetAlignment),
                physDevProps.limits.minStorageBufferOffsetAlignment);

    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        expectedData[i] = rnd.getUint32();

        if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            auto &buffer = buffers.emplace_back(createBufferAndMemory(
                bufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));
            deMemset(buffer->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));
            deMemcpy(buffer->memory->getHostPtr(), &expectedData[i], sizeof(uint32_t));
        }
        else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                 m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
            auto &buffer = buffers.emplace_back(createBufferAndMemory(
                bufferSize, VK_BUFFER_USAGE_2_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT |
                                VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));
            deMemset(buffer->memory->getHostPtr(), 0, static_cast<size_t>(bufferSize));
            deMemcpy(buffer->memory->getHostPtr(), &expectedData[i], sizeof(uint32_t));
        }
        else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                 m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            VkImageCreateInfo imageCreateInfo = initVulkanStructure();
            imageCreateInfo.imageType         = VK_IMAGE_TYPE_1D;
            imageCreateInfo.format            = VK_FORMAT_R32_UINT;
            imageCreateInfo.extent            = {1, 1, 1};
            imageCreateInfo.mipLevels         = 1;
            imageCreateInfo.arrayLayers       = 1;
            imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.usage =
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            images.emplace_back(createImageAndMemory(imageCreateInfo));
        }
    }

    // Write descriptors to heap
    auto heapHostPtr = static_cast<char *>(descriptorHeap->memory->getHostPtr());

    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        VkHostAddressRangeEXT hostRange{};
        hostRange.address = heapHostPtr + i * descriptorStride;
        hostRange.size    = static_cast<size_t>(descriptorSize);

        if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            VkDeviceAddressRangeEXT addressRange{};
            addressRange.address = buffers[i]->address;
            addressRange.size    = bufferSize;

            VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
            resourceInfo.type                        = m_params.descriptorType;
            resourceInfo.data.pAddressRange          = &addressRange;

            VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostRange));
        }
        else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                 m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
            VkTexelBufferDescriptorInfoEXT texelInfo = initVulkanStructure();
            texelInfo.format                         = VK_FORMAT_R32_UINT;
            texelInfo.addressRange.address           = buffers[i]->address;
            texelInfo.addressRange.size              = sizeof(uint32_t);

            VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
            resourceInfo.type                        = m_params.descriptorType;
            resourceInfo.data.pTexelBuffer           = &texelInfo;

            VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostRange));
        }
        else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                 m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            VkImageViewCreateInfo imageViewCreateInfo           = initVulkanStructure();
            imageViewCreateInfo.image                           = *images[i]->image;
            imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_1D;
            imageViewCreateInfo.format                          = VK_FORMAT_R32_UINT;
            imageViewCreateInfo.components                      = makeComponentMappingRGBA();
            imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
            imageViewCreateInfo.subresourceRange.levelCount     = 1;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.layerCount     = 1;

            VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
            imageDescriptorInfo.layout = (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ?
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                             VK_IMAGE_LAYOUT_GENERAL;
            imageDescriptorInfo.pView  = &imageViewCreateInfo;

            VkResourceDescriptorInfoEXT resourceInfo = initVulkanStructure();
            resourceInfo.type                        = m_params.descriptorType;
            resourceInfo.data.pImage                 = &imageDescriptorInfo;
            VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 1, &resourceInfo, &hostRange));
        }
    }

    // Setup descriptor mapping
    VkDescriptorSetAndBindingMappingEXT outputMapping = initVulkanStructure();
    outputMapping.descriptorSet                       = 0;
    outputMapping.firstBinding                        = 0;
    outputMapping.bindingCount                        = 1;
    outputMapping.resourceMask                        = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    outputMapping.source                              = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
    outputMapping.sourceData.pushAddressOffset        = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1;
    mappingInfo.pMappings                                     = &outputMapping;

    // Create compute pipeline
    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR createFlags2 = initVulkanStructure();
    createFlags2.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.pNext                       = &createFlags2;
    pipelineInfo.stage                       = initVulkanStructure();
    pipelineInfo.stage.pNext                 = &mappingInfo;
    pipelineInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module                = *computeModule;
    pipelineInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineInfo);

    // Record command buffer
    auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = descriptorHeap->address;
    bindHeapInfo.heapRange.size      = heapSize;
    bindHeapInfo.reservedRangeOffset = userHeapSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = 0;
    pushDataInfo.data.address      = &outputBuffer->address;
    pushDataInfo.data.size         = sizeof(VkDeviceAddress);

    VkMemoryBarrier2 memoryBarrier = initVulkanStructure();
    memoryBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_HOST_BIT;
    memoryBarrier.dstAccessMask    = VK_ACCESS_2_HOST_READ_BIT;

    VkDependencyInfo dependencyInfo   = initVulkanStructure();
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers    = &memoryBarrier;

    beginCommandBuffer(vkd, *cmdBuffer);

    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
    {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel            = 0;
        subresourceRange.levelCount              = 1;
        subresourceRange.baseArrayLayer          = 0;
        subresourceRange.layerCount              = 1;

        for (uint32_t i = 0; i < descriptorCount; ++i)
        {
            VkImageMemoryBarrier2 imageMemoryBarrier = initVulkanStructure();
            imageMemoryBarrier.srcStageMask          = VK_PIPELINE_STAGE_2_NONE;
            imageMemoryBarrier.srcAccessMask         = 0;
            imageMemoryBarrier.dstStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
            imageMemoryBarrier.dstAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.image                 = *images[i]->image;
            imageMemoryBarrier.subresourceRange      = subresourceRange;

            VkDependencyInfo barrierInfo        = initVulkanStructure();
            barrierInfo.imageMemoryBarrierCount = 1;
            barrierInfo.pImageMemoryBarriers    = &imageMemoryBarrier;
            vkd.cmdPipelineBarrier2(*cmdBuffer, &barrierInfo);
        }
        for (uint32_t i = 0; i < descriptorCount; ++i)
        {
            VkClearColorValue clearValue{};
            clearValue.uint32[0] = expectedData[i];
            vkd.cmdClearColorImage(*cmdBuffer, *images[i]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1,
                                   &subresourceRange);
        }
        for (uint32_t i = 0; i < descriptorCount; ++i)
        {
            VkImageMemoryBarrier2 imageMemoryBarrier = initVulkanStructure();
            imageMemoryBarrier.srcStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
            imageMemoryBarrier.srcAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstStageMask          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            imageMemoryBarrier.dstAccessMask         = (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ?
                                                           VK_ACCESS_2_SHADER_SAMPLED_READ_BIT :
                                                           VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout             = (m_params.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ?
                                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                                           VK_IMAGE_LAYOUT_GENERAL;
            imageMemoryBarrier.image                 = *images[i]->image;
            imageMemoryBarrier.subresourceRange      = subresourceRange;

            VkDependencyInfo barrierInfo        = initVulkanStructure();
            barrierInfo.imageMemoryBarrierCount = 1;
            barrierInfo.pImageMemoryBarriers    = &imageMemoryBarrier;
            vkd.cmdPipelineBarrier2(*cmdBuffer, &barrierInfo);
        }
    }

    vkd.cmdBindResourceHeapEXT(*cmdBuffer, &bindHeapInfo);

    vkd.cmdPushDataEXT(*cmdBuffer, &pushDataInfo);
    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(*cmdBuffer, 1, 1, 1);
    vkd.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

    endCommandBuffer(vkd, *cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo       = initVulkanStructure();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer.get();

    VK_CHECK(vkd.queueSubmit(m_queues.front(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkd.deviceWaitIdle(*m_device));

    // Verify results
    auto outputData = static_cast<uint32_t *>(outputBuffer->memory->getHostPtr());
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        if (outputData[i] != expectedData[i])
        {
            std::stringstream msg;
            msg << "At index " << i << ", expected 0x" << std::hex << expectedData[i] << " but got 0x" << outputData[i];
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

struct TestParamsZeroStride : TestParams
{
    VkDescriptorMappingSourceEXT mappingSource;
    bool useImageSampler = false;
};

class DescriptorHeapTestInstanceZeroStride final : public DescriptorHeapTestInstanceBase
{
public:
    explicit DescriptorHeapTestInstanceZeroStride(Context &context, const TestParamsZeroStride &params)
        : DescriptorHeapTestInstanceBase(context, params)
        , m_params{params}
    {
    }

    tcu::TestStatus iterate() override;

private:
    TestParamsZeroStride m_params;
};

tcu::TestStatus DescriptorHeapTestInstanceZeroStride::iterate()
{
    const auto &vkd       = m_device.getDriver();
    const VkDevice device = *m_device;
    const VkQueue queue   = m_queues[0];
    tcu::TestLog &log     = m_context.getTestContext().getLog();

    const bool useImageSampler                 = m_params.useImageSampler;
    const uint32_t pushOffset                  = sizeof(uint32_t) * 4;
    const uint32_t addressOffset               = sizeof(uint32_t) * 17;
    const uint32_t samplerPushOffset           = sizeof(uint32_t) * 21;
    const uint32_t samplerAddressOffset        = sizeof(uint32_t) * 27;
    const uint32_t descriptorCount             = 2u;
    const VkDeviceSize bufferDescriptorStride  = getBufferDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize imageDescriptorStride   = getImageDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize samplerDescriptorStride = getSamplerDescriptorStride(m_descriptorHeapProperties);
    const VkDeviceSize descriptorStride        = useImageSampler ? imageDescriptorStride : bufferDescriptorStride;

    const VkDeviceSize userHeapSize =
        alignUp(descriptorCount * descriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    const VkDeviceSize heapSize = userHeapSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
    const VkDeviceSize samplerUserHeapSize =
        alignUp(samplerDescriptorStride, m_descriptorHeapProperties.samplerHeapAlignment);
    const VkDeviceSize samplerHeapSize = samplerUserHeapSize + m_descriptorHeapProperties.minSamplerHeapReservedRange;
    const VkDeviceSize inputBufferSize = sizeof(uint32_t);

    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

    std::unique_ptr<Buffer> descriptorHeap = createBufferAndMemory(
        heapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

    std::unique_ptr<Buffer> samplerHeap;
    std::unique_ptr<Buffer> inputBuffer;
    std::unique_ptr<Image> sampledImage;

    const VkDeviceSize outputBufferSize = sizeof(uint32_t);
    auto outputBuffer                   = createBufferAndMemory(outputBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                                                                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    deMemset(outputBuffer->memory->getHostPtr(), 0, static_cast<size_t>(outputBufferSize));

    const VkDeviceSize indirectBufferSize = samplerAddressOffset + sizeof(uint32_t);
    auto indirectBuffer                   = createBufferAndMemory(
        indirectBufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
    uint8_t *indirectData = reinterpret_cast<uint8_t *>(indirectBuffer->memory->getHostPtr());
    deMemset(indirectData, 0, static_cast<size_t>(indirectBufferSize));
    uint32_t *indirectValue = reinterpret_cast<uint32_t *>(indirectData + addressOffset);
    indirectValue[0]        = 37;
    indirectValue[1]        = 71;

    if (useImageSampler)
    {
        samplerHeap = createBufferAndMemory(samplerHeapSize, VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                                 VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);

        VkImageCreateInfo imageCreateInfo = initVulkanStructure();
        imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format            = VK_FORMAT_R32_UINT;
        imageCreateInfo.extent            = {1, 1, 1};
        imageCreateInfo.mipLevels         = 1;
        imageCreateInfo.arrayLayers       = 1;
        imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        sampledImage                      = createImageAndMemory(imageCreateInfo);

        VkImageViewCreateInfo imageViewCreateInfo = initVulkanStructure();
        imageViewCreateInfo.image                 = *sampledImage->image;
        imageViewCreateInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format                = VK_FORMAT_R32_UINT;
        imageViewCreateInfo.components            = makeComponentMappingRGBA();
        imageViewCreateInfo.subresourceRange      = imageSubresourceRange;

        VkImageDescriptorInfoEXT imageDescriptorInfo = initVulkanStructure();
        imageDescriptorInfo.pView                    = &imageViewCreateInfo;
        imageDescriptorInfo.layout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDeviceAddressRangeEXT outputAddressRange{};
        outputAddressRange.address = outputBuffer->address;
        outputAddressRange.size    = outputBufferSize;

        VkResourceDescriptorInfoEXT resourceDescriptorInfo[2];
        resourceDescriptorInfo[0]                    = initVulkanStructure();
        resourceDescriptorInfo[0].type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        resourceDescriptorInfo[0].data.pImage        = &imageDescriptorInfo;
        resourceDescriptorInfo[1]                    = initVulkanStructure();
        resourceDescriptorInfo[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        resourceDescriptorInfo[1].data.pAddressRange = &outputAddressRange;

        VkHostAddressRangeEXT resourceDescriptor[2];
        resourceDescriptor[0].address = reinterpret_cast<char *>(descriptorHeap->memory->getHostPtr());
        resourceDescriptor[0].size    = static_cast<size_t>(descriptorStride);
        resourceDescriptor[1].address =
            reinterpret_cast<char *>(descriptorHeap->memory->getHostPtr()) + descriptorStride;
        resourceDescriptor[1].size = static_cast<size_t>(descriptorStride);
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 2, resourceDescriptorInfo, resourceDescriptor));

        VkSamplerCreateInfo samplerCreateInfo = makeDefaultSamplerCreateInfo();
        VkHostAddressRangeEXT samplerDescriptor{};
        samplerDescriptor.address = samplerHeap->memory->getHostPtr();
        samplerDescriptor.size    = static_cast<size_t>(getSamplerDescriptorStride(m_descriptorHeapProperties));
        VK_CHECK(vkd.writeSamplerDescriptorsEXT(*m_device, 1, &samplerCreateInfo, &samplerDescriptor));
    }
    else
    {
        inputBuffer         = createBufferAndMemory(inputBufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT |
                                                                         VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR);
        uint32_t *inputData = reinterpret_cast<uint32_t *>(inputBuffer->memory->getHostPtr());
        *inputData          = 1;

        VkDeviceAddressRangeEXT bufferDeviceAddressRange[2];
        bufferDeviceAddressRange[0].address = inputBuffer->address;
        bufferDeviceAddressRange[0].size    = inputBufferSize;
        bufferDeviceAddressRange[1].address = outputBuffer->address;
        bufferDeviceAddressRange[1].size    = outputBufferSize;

        VkResourceDescriptorInfoEXT bufferDescriptorInfo[2];
        bufferDescriptorInfo[0]                    = initVulkanStructure();
        bufferDescriptorInfo[0].type               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferDescriptorInfo[0].data.pAddressRange = &bufferDeviceAddressRange[0];
        bufferDescriptorInfo[1]                    = initVulkanStructure();
        bufferDescriptorInfo[1].type               = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferDescriptorInfo[1].data.pAddressRange = &bufferDeviceAddressRange[1];

        VkHostAddressRangeEXT bufferDescriptor[2];
        bufferDescriptor[0].address = reinterpret_cast<char *>(descriptorHeap->memory->getHostPtr());
        bufferDescriptor[0].size    = static_cast<size_t>(descriptorStride);
        bufferDescriptor[1].address = reinterpret_cast<char *>(descriptorHeap->memory->getHostPtr()) + descriptorStride;
        bufferDescriptor[1].size    = static_cast<size_t>(descriptorStride);
        VK_CHECK(vkd.writeResourceDescriptorsEXT(*m_device, 2, bufferDescriptorInfo, bufferDescriptor));
    }

    VkDescriptorSetAndBindingMappingEXT mappings[2];
    mappings[0]                                           = initVulkanStructure();
    mappings[0].descriptorSet                             = 0;
    mappings[0].firstBinding                              = 0;
    mappings[0].bindingCount                              = 64;
    mappings[0].resourceMask                              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[0].source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[0].sourceData.constantOffset                 = {};
    mappings[0].sourceData.constantOffset.heapOffset      = 0;
    mappings[0].sourceData.constantOffset.heapArrayStride = 0;
    mappings[0].sourceData.constantOffset.samplerHeapOffset      = 0;
    mappings[0].sourceData.constantOffset.samplerHeapArrayStride = 0;

    mappings[1]                                           = initVulkanStructure();
    mappings[1].descriptorSet                             = 1;
    mappings[1].firstBinding                              = 0;
    mappings[1].bindingCount                              = 64;
    mappings[1].resourceMask                              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mappings[1].source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
    mappings[1].sourceData.constantOffset                 = {};
    mappings[1].sourceData.constantOffset.heapOffset      = static_cast<uint32_t>(descriptorStride);
    mappings[1].sourceData.constantOffset.heapArrayStride = 0;
    mappings[1].sourceData.constantOffset.samplerHeapOffset      = 0;
    mappings[1].sourceData.constantOffset.samplerHeapArrayStride = 0;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 2;
    mappingInfo.pMappings                                     = mappings;
    auto computeModule = createShaderModule(vkd, *m_device, getShaderBinary("compute"));

    VkPipelineCreateFlags2CreateInfoKHR createFlags2 = initVulkanStructure();
    createFlags2.flags                               = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.pNext                       = &createFlags2;
    pipelineInfo.stage                       = initVulkanStructure();
    pipelineInfo.stage.pNext                 = &mappingInfo;
    pipelineInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module                = *computeModule;
    pipelineInfo.stage.pName                 = "main";

    auto pipeline = createComputePipeline(vkd, *m_device, VK_NULL_HANDLE, &pipelineInfo);

    auto cmdPool   = makeCommandPool(vkd, *m_device, m_queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vkd, *m_device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    uint32_t pushData[2]           = {1, 2};
    VkPushDataInfoEXT pushDataInfo = initVulkanStructure();
    pushDataInfo.offset            = sizeof(uint32_t);
    pushDataInfo.data.address      = pushData;
    pushDataInfo.data.size         = sizeof(uint32_t) * 2;

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = descriptorHeap->address;
    bindHeapInfo.heapRange.size      = heapSize;
    bindHeapInfo.reservedRangeOffset = userHeapSize;
    bindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;

    beginCommandBuffer(vkd, *cmdBuffer);

    if (useImageSampler)
    {
        VkImageMemoryBarrier2 preBarrier = initVulkanStructure();
        preBarrier.srcStageMask          = VK_PIPELINE_STAGE_2_NONE;
        preBarrier.srcAccessMask         = VK_ACCESS_2_NONE;
        preBarrier.dstStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        preBarrier.dstAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        preBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        preBarrier.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        preBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        preBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        preBarrier.image                 = *sampledImage->image;
        preBarrier.subresourceRange      = imageSubresourceRange;

        VkDependencyInfo depInfo        = initVulkanStructure();
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &preBarrier;
        vkd.cmdPipelineBarrier2(*cmdBuffer, &depInfo);

        VkClearColorValue clearColor{};
        clearColor.uint32[0] = 1;
        vkd.cmdClearColorImage(*cmdBuffer, *sampledImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                               &imageSubresourceRange);

        VkImageMemoryBarrier2 postBarrier = initVulkanStructure();
        postBarrier.srcStageMask          = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        postBarrier.srcAccessMask         = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        postBarrier.dstStageMask          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        postBarrier.dstAccessMask         = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        postBarrier.oldLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postBarrier.newLayout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        postBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        postBarrier.image                 = *sampledImage->image;
        postBarrier.subresourceRange      = imageSubresourceRange;

        depInfo.pImageMemoryBarriers = &postBarrier;
        vkd.cmdPipelineBarrier2(*cmdBuffer, &depInfo);
    }

    vkd.cmdPushDataEXT(*cmdBuffer, &pushDataInfo);
    if (m_params.mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT)
    {
        uint32_t pushIndex                  = 77;
        VkPushDataInfoEXT indexPushDataInfo = initVulkanStructure();
        indexPushDataInfo.offset            = pushOffset;
        indexPushDataInfo.data.address      = &pushIndex;
        indexPushDataInfo.data.size         = sizeof(uint32_t);
        vkd.cmdPushDataEXT(*cmdBuffer, &indexPushDataInfo);

        indexPushDataInfo.offset = samplerPushOffset;
        vkd.cmdPushDataEXT(*cmdBuffer, &indexPushDataInfo);
    }
    else if (m_params.mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT)
    {
        VkDeviceAddress indirectAddress       = indirectBuffer->address;
        VkPushDataInfoEXT addressPushDataInfo = initVulkanStructure();
        addressPushDataInfo.offset            = pushOffset;
        addressPushDataInfo.data.address      = &indirectAddress;
        addressPushDataInfo.data.size         = sizeof(VkDeviceAddress);
        vkd.cmdPushDataEXT(*cmdBuffer, &addressPushDataInfo);

        addressPushDataInfo.offset = samplerPushOffset;
        vkd.cmdPushDataEXT(*cmdBuffer, &addressPushDataInfo);
    }
    vkd.cmdBindResourceHeapEXT(*cmdBuffer, &bindHeapInfo);
    if (useImageSampler)
    {
        VkBindHeapInfoEXT samplerBindHeapInfo   = initVulkanStructure();
        samplerBindHeapInfo.heapRange.address   = samplerHeap->address;
        samplerBindHeapInfo.heapRange.size      = samplerHeapSize;
        samplerBindHeapInfo.reservedRangeOffset = samplerUserHeapSize;
        samplerBindHeapInfo.reservedRangeSize   = m_descriptorHeapProperties.minSamplerHeapReservedRange;
        vkd.cmdBindSamplerHeapEXT(*cmdBuffer, &samplerBindHeapInfo);
    }
    vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

    invalidateAlloc(vkd, *m_device, *outputBuffer->memory);

    uint32_t *outputData     = reinterpret_cast<uint32_t *>(outputBuffer->memory->getHostPtr());
    const uint32_t arraySize = 64u; // 64 is the array size in the shader
    uint32_t expectedResult  = pushData[1] + arraySize;
    if (!useImageSampler)
        expectedResult += pushData[0];

    if (outputData[0] != expectedResult)
    {
        log << tcu::TestLog::Message << "Expected result at index 0 was " << expectedResult << " but got "
            << outputData[0] << tcu::TestLog::EndMessage;
        if (!useImageSampler)
        {
            log << tcu::TestLog::Message << "Expected result is: pushData[0] (" << pushData[0] << ") + pushData[1] ("
                << pushData[1] << ") + arraySize (64) * ubo[0].data (1)." << tcu::TestLog::EndMessage;
        }
        else
        {
            log << tcu::TestLog::Message << "Expected result is: pushData[1] (" << pushData[1]
                << ") + arraySize (64) * tex[0].data (1)." << tcu::TestLog::EndMessage;
        }
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorHeapTestCaseZeroStride final : public DescriptorHeapTestCaseBase
{
public:
    explicit DescriptorHeapTestCaseZeroStride(tcu::TestContext &testCtx, const std::string &name,
                                              const TestParamsZeroStride &params)
        : DescriptorHeapTestCaseBase(testCtx, name, params)
        , m_params{params}
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DescriptorHeapTestInstanceZeroStride(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

private:
    TestParamsZeroStride m_params;
};

void DescriptorHeapTestCaseZeroStride::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string comp;
    if (m_params.useImageSampler)
    {
        comp = R"(#version 450
layout(set = 0, binding = 0) uniform usampler2D tex[64];
layout(set = 1, binding = 0) buffer Output {
    uint result;
} outputBuffer[64];

layout(push_constant) uniform PushConstant {
    uint value1;
    uint value2;
    uint value3;
};

void main(void) {
    uint result = value3;
    if (result == 0) {
        result += value2;
    }
    for (uint i = 0; i < 64; i++) {
        result += texture(tex[i], vec2(0.5)).r;
    }
    outputBuffer[63].result = result;
}
)";
    }
    else
    {
        comp = R"(#version 450
layout(set = 0, binding = 0) uniform UBO {
   uint data;
} d[64];
layout(set = 1, binding = 0) buffer Output {
    uint result;
} outputBuffer[64];

layout(push_constant) uniform PushConstant {
    uint value1;
    uint value2;
    uint value3;
};

void main(void) {
    uint result = value3;
    if (result > 0) {
        result += value2;
    }
    for (uint i = 0; i < 64; i++) {
        result += d[i].data;
    }
    outputBuffer[4].result = result;
}
)";
    }
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    programCollection.glslSources.add("compute") << glu::ComputeSource(comp) << buildOptions;
}

const char *getDescriptorTypeTestName(VkDescriptorType descriptorType)
{
    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return "sampler";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "sampled_image";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "storage_image";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "uniform_texel_buffer";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "storage_texel_buffer";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "uniform_buffer";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "storage_buffer";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "input_attachment";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return "acceleration_structure";
    default:
        DE_ASSERT(0);
        return "";
    }
}

const char *getMappingSourceTestName(VkDescriptorMappingSourceEXT mappingSource)
{
    switch (mappingSource)
    {
    case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
        return "heap_with_constant_offset";
    case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
        return "heap_with_push_index";
    case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
        return "heap_with_indirect_index";
    case VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT:
        return "resource_heap_data";
    case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT:
        return "push_data";
    case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT:
        return "push_address";
    case VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT:
        return "indirect_address";
    case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
        return "heap_with_shader_record_index";
    case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT:
        return "shader_record_data";
    case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT:
        return "shader_record_address";
    case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
        return "heap_with_indirect_index_array";
    default:
        DE_ASSERT(0);
        return "";
    }
}

bool isStorageDescriptorType(VkDescriptorType descriptorType)
{
    static constexpr std::array<VkDescriptorType, 3> storageDescriptorType = {
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };
    return std::find(storageDescriptorType.begin(), storageDescriptorType.end(), descriptorType) !=
           storageDescriptorType.end();
}

void populateLimitsTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(testCtx, "limit"));
    addFunctionCase(subGroup.get(), "limits", testLimits);
    topGroup->addChild(subGroup.release());
}

void populateBasicTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> basicGroup(new tcu::TestCaseGroup(testCtx, "basic"));
    const uint32_t basicGroupSeed = baseSeed ^ deStringHash("basic");

    static VkShaderStageFlagBits const shaderStages[] = {
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    };

    static VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const auto stage : shaderStages)
    {
        const char *stageGroupName = nullptr;
        VkQueueFlagBits queue{};
        bool rayTracingPipeline = false;

        switch (stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            stageGroupName = "fragment";
            queue          = VK_QUEUE_GRAPHICS_BIT;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            stageGroupName = "compute";
            queue          = VK_QUEUE_COMPUTE_BIT;
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            stageGroupName     = "raygen";
            queue              = VK_QUEUE_COMPUTE_BIT;
            rayTracingPipeline = true;
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stageGroupName));
        const uint32_t stageGroupHash = basicGroupSeed ^ deStringHash(stageGroupName);

        for (const auto descriptorType : descriptorTypes)
        {
            for (const auto customBorderColor : {false, true})
            {
                if ((stage != VK_SHADER_STAGE_FRAGMENT_BIT) && (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT))
                {
                    continue;
                }
                if ((descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER) && customBorderColor)
                {
                    continue;
                }

                std::string testName = getDescriptorTypeTestName(descriptorType);

                if (customBorderColor)
                {
                    testName += "_custom_border";
                }

                TestParamsBasic params{};
                params.stage                        = stage;
                params.queue                        = queue;
                params.enableRayTracing             = rayTracingPipeline;
                params.enableAccelerationStructures = rayTracingPipeline;
                params.inputAttachments             = (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
                params.enableCustomBorderColor      = customBorderColor;
                params.enableFragmentStoresAndAtomics =
                    stage == VK_SHADER_STAGE_FRAGMENT_BIT && isStorageDescriptorType(descriptorType);
                params.dimension = 1;

                ShaderBinding &binding        = params.bindings.emplace_back();
                binding.descriptorType        = descriptorType;
                binding.descriptorSet         = 0;
                binding.firstBinding          = 2;
                binding.mapping.descriptorSet = binding.descriptorSet;
                binding.mapping.firstBinding  = binding.firstBinding;
                binding.mapping.bindingCount  = 1;
                binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                binding.mapping.sourceData.constantOffset.heapOffset             = binding.firstBinding;
                binding.mapping.sourceData.constantOffset.heapArrayStride        = 1;
                binding.mapping.sourceData.constantOffset.pEmbeddedSampler       = nullptr;
                binding.mapping.sourceData.constantOffset.samplerHeapOffset      = 0;
                binding.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;

                if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                {
                    binding.imageBindingUid = 1;

                    ShaderBinding &sampledImage        = params.bindings.emplace_back();
                    sampledImage.descriptorType        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    sampledImage.descriptorSet         = 0;
                    sampledImage.firstBinding          = 3;
                    sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                    sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                    sampledImage.mapping.bindingCount  = 1;
                    sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                    sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                    sampledImage.mapping.sourceData.constantOffset.heapOffset             = sampledImage.firstBinding;
                    sampledImage.mapping.sourceData.constantOffset.heapArrayStride        = 1;
                    sampledImage.mapping.sourceData.constantOffset.pEmbeddedSampler       = nullptr;
                    sampledImage.mapping.sourceData.constantOffset.samplerHeapOffset      = 0;
                    sampledImage.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;
                }
                else if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {
                    params.enableRayQuery               = (params.stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR);
                    params.enableAccelerationStructures = true;
                }
                else
                {
                    // Nothing to do.
                }

                params.seed = stageGroupHash ^ deStringHash(testName.c_str());

                stageGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }
        }
        basicGroup->addChild(stageGroup.release());
    }

    topGroup->addChild(basicGroup.release());
}

void populateInvarianceTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();

    VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const bool captureReplay : {false, true})
    {
        const char *const groupName = captureReplay ? "capture_replay" : "invariance";

        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, groupName));

        for (const auto descriptorType : descriptorTypes)
        {
            const char *testName = getDescriptorTypeTestName(descriptorType);

            TestParamsWithDescriptorType params{};
            params.queue               = VK_QUEUE_GRAPHICS_BIT;
            params.descriptorType      = descriptorType;
            params.enableCaptureReplay = captureReplay;
            params.seed                = baseSeed;

            if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            {
                params.enableAccelerationStructures              = true;
                params.enableAccelerationStructuresCaptureReplay = true;
            }

            group->addChild(new DescriptorHeapTestCaseInvariance(testCtx, testName, params));

            if (captureReplay && descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
            {
                testName                            = "sampler_custom_border";
                params.enableSamplerYcbcrConversion = false;
                params.enableCustomBorderColor      = true;
                group->addChild(new DescriptorHeapTestCaseInvariance(testCtx, testName, params));
            }
        }

        topGroup->addChild(group.release());
    }
}

void populateDynamicIndexingTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> dynamicIndexingGroup(new tcu::TestCaseGroup(testCtx, "dynamic_indexing"));
    const uint32_t dynamicIndexingGroupSeed = baseSeed ^ deStringHash("dynamic_indexing");

    VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const auto descriptorType : descriptorTypes)
    {
        const char *testName = getDescriptorTypeTestName(descriptorType);

        TestParamsBasic params{};
        params.stage                        = VK_SHADER_STAGE_COMPUTE_BIT;
        params.queue                        = VK_QUEUE_COMPUTE_BIT;
        params.enableRayTracing             = false;
        params.enableAccelerationStructures = false;
        params.dimension                    = 32;

        ShaderBinding &binding        = params.bindings.emplace_back();
        binding.descriptorType        = descriptorType;
        binding.descriptorSet         = 0;
        binding.firstBinding          = 3;
        binding.arrayed               = true;
        binding.mapping.descriptorSet = binding.descriptorSet;
        binding.mapping.firstBinding  = binding.firstBinding;
        binding.mapping.bindingCount  = params.dimension;
        binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        binding.mapping.sourceData.constantOffset.heapOffset             = binding.firstBinding;
        binding.mapping.sourceData.constantOffset.heapArrayStride        = 1;
        binding.mapping.sourceData.constantOffset.pEmbeddedSampler       = nullptr;
        binding.mapping.sourceData.constantOffset.samplerHeapOffset      = 0;
        binding.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;

        if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
        {
            binding.imageBindingUid    = 1;
            binding.shiftSamplerResult = true;

            ShaderBinding &sampledImage        = params.bindings.emplace_back();
            sampledImage.descriptorType        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            sampledImage.descriptorSet         = 0;
            sampledImage.firstBinding          = 2;
            sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
            sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
            sampledImage.mapping.bindingCount  = 1;
            sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
            sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
            sampledImage.mapping.sourceData.constantOffset.heapOffset             = sampledImage.firstBinding;
            sampledImage.mapping.sourceData.constantOffset.heapArrayStride        = 1;
            sampledImage.mapping.sourceData.constantOffset.pEmbeddedSampler       = nullptr;
            sampledImage.mapping.sourceData.constantOffset.samplerHeapOffset      = 0;
            sampledImage.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            params.enableRayQuery               = true;
            params.enableAccelerationStructures = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        {
            params.enableSampledImageArrayNonUniformIndexing = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            params.enableStorageImageArrayNonUniformIndexing = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
        {
            params.enableUniformTexelBufferArrayNonUniformIndexing = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
            params.enableStorageTexelBufferArrayNonUniformIndexing = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            params.enableUniformBufferArrayNonUniformIndexing = true;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            params.enableStorageBufferArrayNonUniformIndexing = true;
        }
        else
        {
            // Nothing to do.
        }

        params.seed = dynamicIndexingGroupSeed ^ deStringHash(testName);

        dynamicIndexingGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
    }

    topGroup->addChild(dynamicIndexingGroup.release());
}

void populateBindingMappingTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> bindingMappingGroup(new tcu::TestCaseGroup(testCtx, "binding_mapping"));
    const uint32_t bindingMappingGroupHash = baseSeed ^ deStringHash("binding_mapping");

    VkDescriptorMappingSourceEXT const mappingSources[] = {
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT,
    };

    VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const VkDescriptorMappingSourceEXT mappingSource : mappingSources)
    {
        const char *mappingSourceName = getMappingSourceTestName(mappingSource);
        MovePtr<tcu::TestCaseGroup> mappingSourceGroup(new tcu::TestCaseGroup(testCtx, mappingSourceName));

        MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
        MovePtr<tcu::TestCaseGroup> fragmentGroup(new tcu::TestCaseGroup(testCtx, "fragment"));
        MovePtr<tcu::TestCaseGroup> raygenGroup(new tcu::TestCaseGroup(testCtx, "raygen"));

        const uint32_t mappingSourceGroupHash = bindingMappingGroupHash ^ deStringHash(mappingSourceName);

        for (const VkDescriptorType descriptorType : descriptorTypes)
        {
            if ((mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT) ||
                (mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT) ||
                (mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT))
            {
                if (descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    continue;
                }
            }

            if ((mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT) ||
                (mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT) ||
                (mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT))
            {
                if ((descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) &&
                    (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) &&
                    (descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR))
                {
                    continue;
                }
            }

            const char *testName = getDescriptorTypeTestName(descriptorType);

            TestParamsBasic params{};
            params.stage     = VK_SHADER_STAGE_COMPUTE_BIT;
            params.queue     = VK_QUEUE_COMPUTE_BIT;
            params.dimension = 8;
            params.seed      = mappingSourceGroupHash ^ deStringHash(testName);

            de::Random rng(~params.seed);

            ShaderBinding &binding = params.bindings.emplace_back();
            binding.descriptorType = descriptorType;
            binding.descriptorSet  = 1;
            binding.firstBinding   = rng.getInt(0, 15);
            binding.arrayed        = true;

            binding.mapping               = initVulkanStructure();
            binding.mapping.descriptorSet = binding.descriptorSet;
            binding.mapping.firstBinding  = binding.firstBinding;
            binding.mapping.source        = mappingSource;
            binding.mapping.bindingCount  = params.dimension;
            binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;

            binding.heapIndex = rng.getInt(8, kMaxDescriptor - params.dimension);

            bool shaderRecordMapping = false;

            switch (mappingSource)
            {
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
                binding.mapping.sourceData.constantOffset.heapOffset      = binding.heapIndex;
                binding.mapping.sourceData.constantOffset.heapArrayStride = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
                binding.mapping.sourceData.pushIndex.heapOffset      = 2;
                binding.mapping.sourceData.pushIndex.pushOffset      = rng.getInt(1, 8) * 4;
                binding.mapping.sourceData.pushIndex.heapIndexStride = 1;
                binding.mapping.sourceData.pushIndex.heapArrayStride = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
                binding.mapping.sourceData.indirectIndex.heapOffset      = 2;
                binding.mapping.sourceData.indirectIndex.pushOffset      = rng.getInt(1, 4) * 8;
                binding.mapping.sourceData.indirectIndex.addressOffset   = rng.getInt(1, 8) * 8;
                binding.mapping.sourceData.indirectIndex.heapIndexStride = 1;
                binding.mapping.sourceData.indirectIndex.heapArrayStride = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT:
                binding.mapping.bindingCount                   = 1;
                binding.mapping.sourceData.heapData.pushOffset = 48;
                binding.mapping.sourceData.heapData.heapOffset = 256;
                binding.arrayed                                = false;
                params.dimension                               = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT:
                binding.mapping.bindingCount              = 1;
                binding.mapping.sourceData.pushDataOffset = 12;
                binding.arrayed                           = false;
                params.dimension                          = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT:
                binding.mapping.bindingCount                 = 1;
                binding.mapping.sourceData.pushAddressOffset = 24;
                binding.arrayed                              = false;
                params.dimension                             = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT:
                binding.mapping.bindingCount                             = 1;
                binding.mapping.sourceData.indirectAddress.addressOffset = rng.getInt(1, 4) * 8;
                binding.mapping.sourceData.indirectAddress.pushOffset    = rng.getInt(1, 4) * 8;
                binding.arrayed                                          = false;
                params.dimension                                         = 1;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
                binding.mapping.sourceData.shaderRecordIndex.heapOffset         = 2;
                binding.mapping.sourceData.shaderRecordIndex.shaderRecordOffset = rng.getInt(1, 4) * 4;
                binding.mapping.sourceData.shaderRecordIndex.heapIndexStride    = 1;
                binding.mapping.sourceData.shaderRecordIndex.heapArrayStride    = 1;
                params.enableRayTracing                                         = true;
                params.enableAccelerationStructures                             = true;
                shaderRecordMapping                                             = true;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT:
                binding.mapping.bindingCount                      = 1;
                binding.mapping.sourceData.shaderRecordDataOffset = 64;
                binding.arrayed                                   = false;
                params.dimension                                  = 1;
                params.enableRayTracing                           = true;
                params.enableAccelerationStructures               = true;
                shaderRecordMapping                               = true;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT:
                binding.mapping.bindingCount                         = 1;
                binding.mapping.sourceData.shaderRecordAddressOffset = 48;
                binding.arrayed                                      = false;
                params.dimension                                     = 1;
                params.enableRayTracing                              = true;
                params.enableAccelerationStructures                  = true;
                shaderRecordMapping                                  = true;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
                binding.mapping.sourceData.indirectIndexArray.heapOffset      = 2;
                binding.mapping.sourceData.indirectIndexArray.pushOffset      = rng.getInt(1, 4) * 8;
                binding.mapping.sourceData.indirectIndexArray.addressOffset   = rng.getInt(1, 8) * 8;
                binding.mapping.sourceData.indirectIndexArray.heapIndexStride = 1;
                break;
            default:
                DE_ASSERT(0);
                break;
            }

            switch (static_cast<int>(descriptorType))
            {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                params.embeddedSamplers = true;

                binding.imageBindingUid    = 1;
                binding.shiftSamplerResult = true;

                ShaderBinding &sampledImage = params.bindings.emplace_back();
                sampledImage.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                sampledImage.descriptorSet  = 0;
                sampledImage.firstBinding   = 3;

                sampledImage.mapping               = initVulkanStructure();
                sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                sampledImage.mapping.bindingCount  = 1;
                sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                sampledImage.mapping.sourceData.constantOffset.heapOffset      = sampledImage.firstBinding;
                sampledImage.mapping.sourceData.constantOffset.heapArrayStride = 1;
                break;
            }
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                params.enableRayQuery               = true;
                params.enableAccelerationStructures = true;
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                params.enableSampledImageArrayNonUniformIndexing = binding.arrayed;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                params.enableStorageImageArrayNonUniformIndexing = binding.arrayed;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                params.enableUniformTexelBufferArrayNonUniformIndexing = binding.arrayed;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                params.enableStorageTexelBufferArrayNonUniformIndexing = binding.arrayed;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                params.enableUniformBufferArrayNonUniformIndexing = binding.arrayed;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                params.enableStorageBufferArrayNonUniformIndexing = binding.arrayed;
                break;
            }

            if (!shaderRecordMapping)
            {
                computeGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }

            params.dimension                      = 1;
            params.enableFragmentStoresAndAtomics = isStorageDescriptorType(descriptorType);
            params.queue                          = VK_QUEUE_GRAPHICS_BIT;
            params.stage                          = VK_SHADER_STAGE_FRAGMENT_BIT;
            if (!shaderRecordMapping)
            {
                fragmentGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }

            params.queue                          = VK_QUEUE_COMPUTE_BIT;
            params.stage                          = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            params.enableRayTracing               = true;
            params.enableAccelerationStructures   = true;
            params.enableRayQuery                 = false;
            params.enableFragmentStoresAndAtomics = false;
            raygenGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
        }
        mappingSourceGroup->addChild(computeGroup.release());
        mappingSourceGroup->addChild(fragmentGroup.release());
        mappingSourceGroup->addChild(raygenGroup.release());

        bindingMappingGroup->addChild(mappingSourceGroup.release());
    }

    topGroup->addChild(bindingMappingGroup.release());
}

void populateHighBindingTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> highBindingGroup(new tcu::TestCaseGroup(testCtx, "high_binding"));

    for (const uint32_t count : {4u, 0xFFFFFFFFu})
    {
        // High descriptor set values should also be tested, but glslang does not support them.
        for (const uint32_t descriptorSet : {15u})
        {
            // High binding values should also be tested, but glslang does not support them.
            for (const uint32_t bindingIndex : {300u})
            {
                std::stringstream stream;
                stream << std::hex;
                stream << "count_0x" << count << "_set_0x" << descriptorSet << "_binding_0x" << bindingIndex;
                std::string testName = stream.str();

                TestParamsBasic params{};
                params.stage     = VK_SHADER_STAGE_COMPUTE_BIT;
                params.queue     = VK_QUEUE_COMPUTE_BIT;
                params.dimension = 1;
                params.seed      = baseSeed ^ deStringHash(testName.c_str());

                ShaderBinding &binding = params.bindings.emplace_back();
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                binding.descriptorSet  = descriptorSet;
                binding.firstBinding   = bindingIndex;
                binding.heapIndex      = 5;

                binding.mapping               = initVulkanStructure();
                binding.mapping.descriptorSet = binding.descriptorSet;
                binding.mapping.firstBinding  = binding.firstBinding;
                binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                binding.mapping.bindingCount  = count;
                binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                binding.mapping.sourceData.constantOffset.heapOffset      = binding.heapIndex;
                binding.mapping.sourceData.constantOffset.heapArrayStride = 1;

                highBindingGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }
        }
    }

    topGroup->addChild(highBindingGroup.release());
}

void populateCombinedImageSamplerTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> combinedImageSamplersGroup(new tcu::TestCaseGroup(testCtx, "combined_image_samplers"));
    const uint32_t combinedImageSamplersGroupHash = baseSeed ^ deStringHash("combined_image_samplers");

    VkDescriptorMappingSourceEXT const mappingSources[] = {
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT,
    };

    for (const auto mappingSource : mappingSources)
    {
        const char *mappingSourceName = getMappingSourceTestName(mappingSource);
        MovePtr<tcu::TestCaseGroup> mappingSourceGroup(new tcu::TestCaseGroup(testCtx, mappingSourceName));

        const uint32_t mappingSourceGroupHash = combinedImageSamplersGroupHash ^ deStringHash(mappingSourceName);

        enum class Mode
        {
            Basic,
            Indexed,
            Combined,
            IndexedCombined,
        };

        static const std::pair<Mode, const char *> modes[] = {
            {Mode::Basic, "basic"},
            {Mode::Indexed, "indexed"},
            {Mode::Combined, "combined"},
            {Mode::IndexedCombined, "indexed_combined"},
        };

        for (const auto useEmbedded : {false, true})
        {
            const char *const embeddedName = useEmbedded ? "embedded" : "non_embedded";

            MovePtr<tcu::TestCaseGroup> embeddedGroup(new tcu::TestCaseGroup(testCtx, embeddedName));
            const uint32_t embeddedGroupHash = mappingSourceGroupHash ^ deStringHash(embeddedName);

            for (const auto &mode : modes)
            {
                if (useEmbedded && mode.first != Mode::Basic)
                {
                    continue;
                }

                const bool useCombined = (mode.first == Mode::Combined) || (mode.first == Mode::IndexedCombined);
                if (useCombined && (mappingSource == VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT))
                {
                    continue;
                }

                const char *const testName = mode.second;
                const bool isArrayed       = (mode.first == Mode::Indexed) || (mode.first == Mode::IndexedCombined);

                TestParamsBasic params{};
                params.stage                                     = VK_SHADER_STAGE_COMPUTE_BIT;
                params.queue                                     = VK_QUEUE_COMPUTE_BIT;
                params.enableRayTracing                          = false;
                params.enableAccelerationStructures              = false;
                params.enableSampledImageArrayNonUniformIndexing = isArrayed;
                params.dimension                                 = isArrayed ? 4 : 1;
                params.seed                                      = embeddedGroupHash ^ deStringHash(testName);
                params.embeddedSamplers                          = useEmbedded;
                params.bindSamplerHeap                           = !useEmbedded;

                de::Random rng(~params.seed);

                ShaderBinding &binding             = params.bindings.emplace_back();
                binding.descriptorType             = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                binding.descriptorSet              = 1;
                binding.firstBinding               = rng.getInt(0, 15);
                binding.arrayed                    = isArrayed;
                binding.combinedImageSamplerHandle = useCombined;

                binding.mapping               = initVulkanStructure();
                binding.mapping.descriptorSet = binding.descriptorSet;
                binding.mapping.firstBinding  = binding.firstBinding;
                binding.mapping.source        = mappingSource;
                binding.mapping.bindingCount  = params.dimension;
                binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;

                binding.heapIndex        = rng.getInt(8, kMaxDescriptor - params.dimension);
                binding.samplerHeapIndex = rng.getInt(8, 15);

                switch (mappingSource)
                {
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
                    DE_ASSERT(!binding.combinedImageSamplerHandle);

                    binding.mapping.sourceData.constantOffset.heapOffset      = binding.heapIndex;
                    binding.mapping.sourceData.constantOffset.heapArrayStride = 1;

                    binding.mapping.sourceData.constantOffset.samplerHeapOffset      = binding.samplerHeapIndex;
                    binding.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
                    if (binding.combinedImageSamplerHandle)
                    {
                        binding.mapping.sourceData.pushIndex.heapOffset                   = 0;
                        binding.mapping.sourceData.pushIndex.pushOffset                   = 4;
                        binding.mapping.sourceData.pushIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.pushIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.pushIndex.useCombinedImageSamplerIndex = VK_TRUE;
                        binding.mapping.sourceData.pushIndex.samplerHeapOffset            = 0;
                        binding.mapping.sourceData.pushIndex.samplerPushOffset            = 0xFFFF; // Unused
                        binding.mapping.sourceData.pushIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.pushIndex.samplerHeapArrayStride       = 0;

                        params.pushData.push_back({4, binding.heapIndex | (binding.samplerHeapIndex << 20)});
                    }
                    else
                    {
                        binding.mapping.sourceData.pushIndex.heapOffset                   = 2;
                        binding.mapping.sourceData.pushIndex.pushOffset                   = 4;
                        binding.mapping.sourceData.pushIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.pushIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.pushIndex.useCombinedImageSamplerIndex = VK_FALSE;
                        binding.mapping.sourceData.pushIndex.samplerHeapOffset            = 6;
                        binding.mapping.sourceData.pushIndex.samplerPushOffset            = 20;
                        binding.mapping.sourceData.pushIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.pushIndex.samplerHeapArrayStride       = 0;

                        params.pushData.emplace_back(binding.mapping.sourceData.pushIndex.pushOffset,
                                                     binding.heapIndex -
                                                         binding.mapping.sourceData.pushIndex.heapOffset);
                        params.pushData.emplace_back(binding.mapping.sourceData.pushIndex.samplerPushOffset,
                                                     binding.samplerHeapIndex -
                                                         binding.mapping.sourceData.pushIndex.samplerHeapOffset);
                    }
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
                    if (binding.combinedImageSamplerHandle)
                    {
                        binding.mapping.sourceData.indirectIndex.heapOffset                   = 5;
                        binding.mapping.sourceData.indirectIndex.pushOffset                   = 8;
                        binding.mapping.sourceData.indirectIndex.addressOffset                = 64;
                        binding.mapping.sourceData.indirectIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.indirectIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.indirectIndex.useCombinedImageSamplerIndex = VK_TRUE;
                        binding.mapping.sourceData.indirectIndex.samplerHeapOffset            = 5;
                        binding.mapping.sourceData.indirectIndex.samplerPushOffset            = 0xFFFF; // Unused
                        binding.mapping.sourceData.indirectIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.indirectIndex.samplerHeapArrayStride       = 0;
                    }
                    else
                    {
                        binding.mapping.sourceData.indirectIndex.heapOffset                   = 2;
                        binding.mapping.sourceData.indirectIndex.pushOffset                   = 8;
                        binding.mapping.sourceData.indirectIndex.addressOffset                = 64;
                        binding.mapping.sourceData.indirectIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.indirectIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.indirectIndex.useCombinedImageSamplerIndex = VK_FALSE;
                        binding.mapping.sourceData.indirectIndex.samplerHeapOffset            = 5;
                        binding.mapping.sourceData.indirectIndex.samplerPushOffset            = 16;
                        binding.mapping.sourceData.indirectIndex.samplerAddressOffset         = 80;
                        binding.mapping.sourceData.indirectIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.indirectIndex.samplerHeapArrayStride       = 0;
                    }
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
                {
                    params.enableRayTracing             = true;
                    params.enableAccelerationStructures = true;
                    params.stage                        = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    if (binding.combinedImageSamplerHandle)
                    {
                        binding.mapping.sourceData.shaderRecordIndex.heapOffset                   = 4;
                        binding.mapping.sourceData.shaderRecordIndex.shaderRecordOffset           = 12;
                        binding.mapping.sourceData.shaderRecordIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.shaderRecordIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.shaderRecordIndex.useCombinedImageSamplerIndex = VK_TRUE;
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapOffset            = 4;
                        binding.mapping.sourceData.shaderRecordIndex.samplerShaderRecordOffset    = 0xFFFF; // Unused
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapArrayStride       = 0;
                    }
                    else
                    {
                        binding.mapping.sourceData.shaderRecordIndex.heapOffset                   = 3;
                        binding.mapping.sourceData.shaderRecordIndex.shaderRecordOffset           = 4;
                        binding.mapping.sourceData.shaderRecordIndex.heapIndexStride              = 1;
                        binding.mapping.sourceData.shaderRecordIndex.heapArrayStride              = 1;
                        binding.mapping.sourceData.shaderRecordIndex.useCombinedImageSamplerIndex = VK_FALSE;
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapOffset            = 3;
                        binding.mapping.sourceData.shaderRecordIndex.samplerShaderRecordOffset    = 8;
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapIndexStride       = 1;
                        binding.mapping.sourceData.shaderRecordIndex.samplerHeapArrayStride       = 0;
                    }
                    break;
                }
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
                {
                    if (binding.combinedImageSamplerHandle)
                    {
                        binding.mapping.sourceData.indirectIndexArray.heapOffset                   = 4;
                        binding.mapping.sourceData.indirectIndexArray.pushOffset                   = 8;
                        binding.mapping.sourceData.indirectIndexArray.addressOffset                = 64;
                        binding.mapping.sourceData.indirectIndexArray.heapIndexStride              = 1;
                        binding.mapping.sourceData.indirectIndexArray.useCombinedImageSamplerIndex = VK_TRUE;
                        binding.mapping.sourceData.indirectIndexArray.samplerHeapOffset            = 4;
                        binding.mapping.sourceData.indirectIndexArray.samplerPushOffset            = 0xFFFF; // Unused
                        binding.mapping.sourceData.indirectIndexArray.samplerHeapIndexStride       = 1;
                    }
                    else
                    {
                        binding.mapping.sourceData.indirectIndexArray.heapOffset                   = 2;
                        binding.mapping.sourceData.indirectIndexArray.pushOffset                   = 8;
                        binding.mapping.sourceData.indirectIndexArray.addressOffset                = 16;
                        binding.mapping.sourceData.indirectIndexArray.heapIndexStride              = 1;
                        binding.mapping.sourceData.indirectIndexArray.useCombinedImageSamplerIndex = VK_FALSE;
                        binding.mapping.sourceData.indirectIndexArray.samplerHeapOffset            = 4;
                        binding.mapping.sourceData.indirectIndexArray.samplerPushOffset            = 16;
                        binding.mapping.sourceData.indirectIndexArray.samplerAddressOffset         = 64;
                        binding.mapping.sourceData.indirectIndexArray.samplerHeapIndexStride       = 1;
                    }
                    break;
                }
                default:
                    DE_ASSERT(0);
                    break;
                }

                embeddedGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }

            mappingSourceGroup->addChild(embeddedGroup.release());
        }

        combinedImageSamplersGroup->addChild(mappingSourceGroup.release());
    }

    topGroup->addChild(combinedImageSamplersGroup.release());
}

void populateReservedHeapTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> reservedHeapGroup(new tcu::TestCaseGroup(testCtx, "reserved_heap"));
    const uint32_t reservedHeapGroupHash = baseSeed ^ deStringHash("reserved_heap");

    static std::pair<CopyMethod, const char *> modes[] = {
        {CopyMethod::BufferToImage, "buffer_to_image"}, {CopyMethod::CopyImage, "copy_image"},
        {CopyMethod::ImageToBuffer, "image_to_buffer"}, {CopyMethod::ClearColorImage, "clear_color_image"},
        {CopyMethod::BlitImage, "blit_image"},
    };
    for (int mask = 3; mask >= 0; --mask)
    {
        const bool bindResourceHeap = (mask & 1) != 0;
        const bool bindSamplerHeap  = (mask & 2) != 0;

        const char *groupName = "";
        if (bindResourceHeap && bindSamplerHeap)
        {
            groupName = "both_heaps";
        }
        else if (bindResourceHeap)
        {
            groupName = "resource_heap";
        }
        else if (bindSamplerHeap)
        {
            groupName = "sampler_heap";
        }
        else
        {
            groupName = "no_heaps";
        }

        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, groupName));

        for (const auto &mode : modes)
        {
            const char *name    = mode.second;
            const uint32_t seed = reservedHeapGroupHash ^ deStringHash(name);

            TestParamsReservedHeap params;
            params.seed             = seed;
            params.copyMethod       = mode.first;
            params.imageExtent      = 32;
            params.bindResourceHeap = bindResourceHeap;
            params.bindSamplerHeap  = bindSamplerHeap;

            group->addChild(new DescriptorHeapTestCaseReservedHeap(testCtx, name, params));
        }
        reservedHeapGroup->addChild(group.release());
    }

    topGroup->addChild(reservedHeapGroup.release());
}

void populatePushDataTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> pushDataGroup(new tcu::TestCaseGroup(testCtx, "push_data"));
    const uint32_t pushDataSeed = baseSeed ^ deStringHash("push_data");

    static VkShaderStageFlagBits const shaderStages[] = {
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    };

    for (const auto stage : shaderStages)
    {
        const char *testName{};
        VkQueueFlagBits queue{};
        bool rayTracingPipeline{};

        switch (stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            testName = "fragment";
            queue    = VK_QUEUE_GRAPHICS_BIT;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            testName = "compute";
            queue    = VK_QUEUE_COMPUTE_BIT;
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            testName           = "raygen";
            queue              = VK_QUEUE_COMPUTE_BIT;
            rayTracingPipeline = true;
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        TestParamsBasic params{};
        params.stage                        = stage;
        params.queue                        = queue;
        params.enableRayTracing             = rayTracingPipeline;
        params.enableAccelerationStructures = rayTracingPipeline;
        params.dimension                    = 1;
        params.seed                         = pushDataSeed ^ deStringHash(testName);

        ShaderBinding &binding                    = params.bindings.emplace_back();
        binding.descriptorType                    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorSet                     = 1;
        binding.firstBinding                      = 24;
        binding.arrayed                           = false;
        binding.mapping                           = initVulkanStructure();
        binding.mapping.descriptorSet             = binding.descriptorSet;
        binding.mapping.firstBinding              = binding.firstBinding;
        binding.mapping.bindingCount              = 1;
        binding.mapping.resourceMask              = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        binding.mapping.source                    = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
        binding.mapping.sourceData.pushDataOffset = 12;

        pushDataGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
    }

    topGroup->addChild(pushDataGroup.release());
}

void populateNullDescriptorTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> nullDescriptorGroup(new tcu::TestCaseGroup(testCtx, "null_descriptor"));
    const uint32_t nullDescriptorSeed = baseSeed ^ deStringHash("null_descriptor");

    static VkShaderStageFlagBits const shaderStages[] = {
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    };

    static VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const auto stage : shaderStages)
    {
        const char *stageGroupName = nullptr;
        VkQueueFlagBits queue{};
        bool rayTracingPipeline = false;

        switch (stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            stageGroupName = "fragment";
            queue          = VK_QUEUE_GRAPHICS_BIT;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            stageGroupName = "compute";
            queue          = VK_QUEUE_COMPUTE_BIT;
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            stageGroupName     = "raygen";
            queue              = VK_QUEUE_COMPUTE_BIT;
            rayTracingPipeline = true;
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stageGroupName));
        const uint32_t stageGroupHash = nullDescriptorSeed ^ deStringHash(stageGroupName);

        for (const auto descriptorType : descriptorTypes)
        {
            const char *testName = getDescriptorTypeTestName(descriptorType);

            TestParamsBasic params{};
            params.stage                        = stage;
            params.queue                        = queue;
            params.enableRayTracing             = rayTracingPipeline;
            params.enableAccelerationStructures = rayTracingPipeline;
            params.enableNullDescriptor         = true;
            params.dimension                    = 1;
            params.enableFragmentStoresAndAtomics =
                stage == VK_SHADER_STAGE_FRAGMENT_BIT && isStorageDescriptorType(descriptorType);

            ShaderBinding &binding        = params.bindings.emplace_back();
            binding.descriptorType        = descriptorType;
            binding.descriptorSet         = 0;
            binding.firstBinding          = 2;
            binding.nullDescriptor        = true;
            binding.mapping.descriptorSet = binding.descriptorSet;
            binding.mapping.firstBinding  = binding.firstBinding;
            binding.mapping.bindingCount  = 1;
            binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
            binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
            binding.mapping.sourceData.constantOffset.heapOffset      = binding.firstBinding;
            binding.mapping.sourceData.constantOffset.heapArrayStride = 1;

            if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
            {
                binding.imageBindingUid = 1;

                ShaderBinding &sampledImage        = params.bindings.emplace_back();
                sampledImage.descriptorType        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                sampledImage.descriptorSet         = 0;
                sampledImage.firstBinding          = 3;
                sampledImage.samplerIsNull         = true;
                sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                sampledImage.mapping.bindingCount  = 1;
                sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                sampledImage.mapping.sourceData.constantOffset.heapOffset      = sampledImage.firstBinding;
                sampledImage.mapping.sourceData.constantOffset.heapArrayStride = 1;
            }
            else if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            {
                params.enableRayQuery               = (params.stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR);
                params.enableAccelerationStructures = true;
            }
            else
            {
                // Nothing to do.
            }

            params.seed = stageGroupHash ^ deStringHash(testName);

            stageGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
        }
        nullDescriptorGroup->addChild(stageGroup.release());
    }

    topGroup->addChild(nullDescriptorGroup.release());
}

void populateYcbcrTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> ycbcrGroup(new tcu::TestCaseGroup(testCtx, "ycbcr"));

    TestParams params;
    params.enableSamplerYcbcrConversion = true;
    params.seed                         = baseSeed ^ deStringHash("ycbcr");
    ycbcrGroup->addChild(new DescriptorHeapTestCaseYcbcr(testCtx, "ycbcr", params));
    topGroup->addChild(ycbcrGroup.release());
}

void populateDifferentMappingsPerShader(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, "different_mappings_per_shader"));

    TestParams params{};
    params.queue                          = VK_QUEUE_GRAPHICS_BIT;
    params.seed                           = baseSeed ^ deStringHash("different_mappings_per_shader");
    params.enableFragmentStoresAndAtomics = true;
    subgroup->addChild(
        new DescriptorHeapTestCaseDifferentMappingsPerShader(testCtx, "different_mappings_per_shader", params));
    topGroup->addChild(subgroup.release());
}

void populateGraphicsPipelineLibraryTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> gplGroup(new tcu::TestCaseGroup(testCtx, "graphics_pipeline_library"));

    // GPL case
    TestParamsGPL gplParams{};
    gplParams.queue                          = VK_QUEUE_GRAPHICS_BIT;
    gplParams.enableGraphicsPipelineLibrary  = true;
    gplParams.enableFragmentStoresAndAtomics = true;
    gplParams.seed                           = baseSeed ^ deStringHash("graphics_pipeline_library");
    gplGroup->addChild(new DescriptorHeapTestCaseGPL(testCtx, "graphics_pipeline_library", gplParams));

    // Shader object case
    TestParamsGPL shaderObjectParams{};
    shaderObjectParams.queue                          = VK_QUEUE_GRAPHICS_BIT;
    shaderObjectParams.enableShaderObject             = true;
    shaderObjectParams.enableDynamicRendering         = true;
    shaderObjectParams.enableFragmentStoresAndAtomics = true;
    shaderObjectParams.seed                           = baseSeed ^ deStringHash("shader_object");
    gplGroup->addChild(new DescriptorHeapTestCaseGPL(testCtx, "shader_object", shaderObjectParams));

    shaderObjectParams.seed                                 = baseSeed ^ deStringHash("shader_object_unbind_frag");
    shaderObjectParams.unbindFragShader                     = true;
    shaderObjectParams.enableVertexPipelineStoresAndAtomics = true;
    shaderObjectParams.enableFragmentStoresAndAtomics       = false;
    gplGroup->addChild(new DescriptorHeapTestCaseGPL(testCtx, "shader_object_unbind_frag", shaderObjectParams));
    topGroup->addChild(gplGroup.release());
}

void populateSwitchHeapsTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> switchHeapsGroup(new tcu::TestCaseGroup(testCtx, "switch_heaps"));

    for (const bool inheritance : {false, true})
    {
        for (const bool pushDescriptors : {false, true})
        {
            std::string name = "switch_heaps";
            if (pushDescriptors)
            {
                name += "_with_push_descriptors";
            }
            if (inheritance)
            {
                name += "_with_nv_inheritance";
            }

            TestParams params{};
            params.seed                             = baseSeed ^ deStringHash(name.c_str());
            params.enableNVCommandBufferInheritance = inheritance;
            params.enablePushDescriptors            = pushDescriptors;
            switchHeapsGroup->addChild(new DescriptorHeapTestCaseSwitchHeaps(testCtx, name, params));
        }
    }
    topGroup->addChild(switchHeapsGroup.release());
}

void populateConcurrentQueuesTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> concurrentQueuesGroup(new tcu::TestCaseGroup(testCtx, "concurrent_queues"));
    const uint32_t concurrentQueuesGroupSeed = baseSeed ^ deStringHash("concurrent_queues");

    static VkShaderStageFlagBits const shaderStages[] = {
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    };

    static VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const auto stage : shaderStages)
    {
        const char *stageGroupName = nullptr;
        VkQueueFlagBits queue{};
        bool rayTracingPipeline = false;

        switch (stage)
        {
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            stageGroupName = "fragment";
            queue          = VK_QUEUE_GRAPHICS_BIT;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            stageGroupName = "compute";
            queue          = VK_QUEUE_COMPUTE_BIT;
            break;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            stageGroupName     = "raygen";
            queue              = VK_QUEUE_COMPUTE_BIT;
            rayTracingPipeline = true;
            break;
        default:
            DE_ASSERT(0);
            break;
        }

        MovePtr<tcu::TestCaseGroup> stageGroup(new tcu::TestCaseGroup(testCtx, stageGroupName));
        const uint32_t stageGroupHash = concurrentQueuesGroupSeed ^ deStringHash(stageGroupName);

        for (const auto descriptorType : descriptorTypes)
        {
            for (const auto customBorderColor : {false, true})
            {
                if ((stage != VK_SHADER_STAGE_FRAGMENT_BIT) && (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT))
                {
                    continue;
                }
                if ((descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER) && customBorderColor)
                {
                    continue;
                }

                std::string testName = getDescriptorTypeTestName(descriptorType);

                if (customBorderColor)
                {
                    testName += "_custom_border";
                }

                TestParamsBasic params{};
                params.stage                        = stage;
                params.queue                        = queue;
                params.enableRayTracing             = rayTracingPipeline;
                params.enableAccelerationStructures = rayTracingPipeline;
                params.inputAttachments             = (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
                params.enableCustomBorderColor      = customBorderColor;
                params.enableFragmentStoresAndAtomics =
                    (stage == VK_SHADER_STAGE_FRAGMENT_BIT) && isStorageDescriptorType(descriptorType);
                params.dimension  = 1;
                params.queueCount = 2;

                for (uint32_t queueIndex = 0; queueIndex < params.queueCount; ++queueIndex)
                {
                    ShaderBinding &binding        = params.bindings.emplace_back();
                    binding.descriptorType        = descriptorType;
                    binding.descriptorSet         = 0;
                    binding.firstBinding          = 10;
                    binding.queue                 = queueIndex;
                    binding.mapping.descriptorSet = 0;
                    binding.mapping.firstBinding  = binding.firstBinding;
                    binding.mapping.bindingCount  = 1;
                    binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                    binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                    binding.mapping.sourceData.constantOffset.heapOffset             = binding.firstBinding;
                    binding.mapping.sourceData.constantOffset.heapArrayStride        = 1;
                    binding.mapping.sourceData.constantOffset.pEmbeddedSampler       = nullptr;
                    binding.mapping.sourceData.constantOffset.samplerHeapOffset      = 0;
                    binding.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;

                    if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                    {
                        binding.imageBindingUid = 1;

                        ShaderBinding &sampledImage        = params.bindings.emplace_back();
                        sampledImage.descriptorType        = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        sampledImage.descriptorSet         = 0;
                        sampledImage.firstBinding          = 20;
                        sampledImage.queue                 = queueIndex;
                        sampledImage.mapping.descriptorSet = 0;
                        sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                        sampledImage.mapping.bindingCount  = 1;
                        sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                        sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                        sampledImage.mapping.sourceData.constantOffset.heapOffset        = sampledImage.firstBinding;
                        sampledImage.mapping.sourceData.constantOffset.heapArrayStride   = 1;
                        sampledImage.mapping.sourceData.constantOffset.pEmbeddedSampler  = nullptr;
                        sampledImage.mapping.sourceData.constantOffset.samplerHeapOffset = 0;
                        sampledImage.mapping.sourceData.constantOffset.samplerHeapArrayStride = 0;
                    }
                }

                if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {
                    params.enableRayQuery               = (params.stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR);
                    params.enableAccelerationStructures = true;
                }

                params.seed = stageGroupHash ^ deStringHash(testName.c_str());

                stageGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }
        }
        concurrentQueuesGroup->addChild(stageGroup.release());
    }

    topGroup->addChild(concurrentQueuesGroup.release());
}

void populateConcurrentHeapSetTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> concurrentHeapSetGroup(new tcu::TestCaseGroup(testCtx, "concurrent_heap_set"));

    TestParams params{};
    params.seed       = baseSeed ^ deStringHash("concurrent_heap_set");
    params.queue      = VK_QUEUE_COMPUTE_BIT;
    params.queueCount = 2;

    concurrentHeapSetGroup->addChild(
        new DescriptorHeapTestCaseConcurrentHeapSet(testCtx, "concurrent_heap_set", params));
    topGroup->addChild(concurrentHeapSetGroup.release());
}

void populateStateInvalidationTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> stateInvalidationGroup(new tcu::TestCaseGroup(testCtx, "state_invalidation"));

    TestParams params{};
    params.seed  = baseSeed ^ deStringHash("state_invalidation");
    params.queue = VK_QUEUE_COMPUTE_BIT;

    stateInvalidationGroup->addChild(
        new DescriptorHeapTestCaseStateInvalidation(testCtx, "state_invalidation", params));
    topGroup->addChild(stateInvalidationGroup.release());
}

void populateWriteAfterRecordTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> writeAfterRecordGroup(new tcu::TestCaseGroup(testCtx, "write_after_record"));

    TestParams params{};
    params.seed  = baseSeed ^ deStringHash("write_after_record");
    params.queue = VK_QUEUE_COMPUTE_BIT;

    writeAfterRecordGroup->addChild(new DescriptorHeapTestCaseWriteAfterRecord(testCtx, "write_after_record", params));
    topGroup->addChild(writeAfterRecordGroup.release());
}

void populateSpirvTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> spirvGroup(new tcu::TestCaseGroup(testCtx, "spirv"));

    static constexpr std::pair<SpirvTestType, const char *> tests[] = {
        {SpirvTestType::SizeOf, "size_of"},
        {SpirvTestType::SizeOf64, "size_of_64"},
        {SpirvTestType::UntypedStorageBuffer, "untyped_storage_buffer"},
        {SpirvTestType::UntypedArrayLength, "untyped_array_length"},
        {SpirvTestType::SimpleStorageTexelBuffer, "simple_storage_texel_buffer"},
        {SpirvTestType::UntypedImageTexelPointer, "untyped_image_texel_pointer"},
        {SpirvTestType::SimpleSamplerHeap, "simple_sampler_heap"},
        {SpirvTestType::FunctionCallBinding, "function_call_binding"},
        {SpirvTestType::FunctionCallBindingForward, "function_call_binding_forward"},
        {SpirvTestType::StorageTexelBufferAtomic64, "storage_texel_buffer_atomic64"},
        {SpirvTestType::SimpleVariablePointers, "simple_variable_pointers"},
        {SpirvTestType::ArrayVariablePointers, "array_variable_pointers"},
        {SpirvTestType::AtomicImageWithinFunction, "atomic_image_within_function"},
    };
    for (const auto &test : tests)
    {
        TestParamsSpirv params{};
        params.queue                  = VK_QUEUE_COMPUTE_BIT;
        params.spirvTestType          = test.first;
        params.enableVariablePointers = (test.first == SpirvTestType::SimpleVariablePointers) ||
                                        (test.first == SpirvTestType::ArrayVariablePointers);
        params.shaderImageInt64Atomics      = params.spirvTestType == SpirvTestType::StorageTexelBufferAtomic64;
        params.enableShader64bitIndexing    = test.first == SpirvTestType::SizeOf64;
        params.enableRuntimeDescriptorArray = test.first == SpirvTestType::AtomicImageWithinFunction;

        spirvGroup->addChild(new DescriptorHeapTestCaseSpirv(testCtx, test.second, params));
    }

    topGroup->addChild(spirvGroup.release());
}

void populateResourceMaskingTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> resourceMaskingGroup(new tcu::TestCaseGroup(testCtx, "resource_masking"));

    TestParams params{};
    params.queue = VK_QUEUE_COMPUTE_BIT;
    resourceMaskingGroup->addChild(new DescriptorHeapTestCaseResourceMasking(testCtx, "resource_masking", params));

    topGroup->addChild(resourceMaskingGroup.release());
}

void populateNullImageQueriesTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> nullImageQueriesGroup(new tcu::TestCaseGroup(testCtx, "null_image_queries"));

    TestParams params{};
    params.queue                = VK_QUEUE_COMPUTE_BIT;
    params.enableNullDescriptor = true;
    nullImageQueriesGroup->addChild(new DescriptorHeapTestCaseNullImageQueries(testCtx, "null_image_queries", params));

    topGroup->addChild(nullImageQueriesGroup.release());
}

void populateGraphicsTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> graphicsGroup(new tcu::TestCaseGroup(testCtx, "graphics"));

    for (const bool secondary : {false, true})
    {
        for (const bool fragment : {false, true})
        {
            for (const bool tessellation : {false, true})
            {
                for (const bool geometry : {false, true})
                {
                    for (const bool vectors : {false, true})
                    {
                        std::string testName = "vertex";
                        if (tessellation)
                        {
                            testName += "_tessellation";
                        }
                        if (geometry)
                        {
                            testName += "_geometry";
                        }
                        if (fragment)
                        {
                            testName += "_fragment";
                        }
                        if (secondary)
                        {
                            testName += "_secondary_cmdbuf";
                        }
                        if (vectors)
                            testName += "_vectors";

                        TestParamsGraphics params{};
                        params.queue                                = VK_QUEUE_GRAPHICS_BIT;
                        params.enableVertexPipelineStoresAndAtomics = true;
                        params.enableTessellationShader             = tessellation;
                        params.enableGeometryShader                 = geometry;
                        params.enableFragmentStoresAndAtomics       = fragment;
                        params.useFragmentShader                    = fragment;
                        params.useSecondaryCommandBuffer            = secondary;
                        params.useVectors                           = vectors;
                        params.seed                                 = baseSeed ^ deStringHash(testName.c_str());
                        graphicsGroup->addChild(new DescriptorHeapTestCaseGraphics(testCtx, testName, params));
                    }
                }
            }
        }
        for (const bool fragment : {false, true})
        {
            for (const bool task : {false, true})
            {
                std::string testName = "";
                if (task)
                {
                    testName += "task_";
                }
                testName += "mesh";
                if (fragment)
                {
                    testName += "_fragment";
                }
                if (secondary)
                {
                    testName += "_secondary_cmdbuf";
                }

                TestParamsGraphics params{};
                params.queue                          = VK_QUEUE_GRAPHICS_BIT;
                params.enableFragmentStoresAndAtomics = fragment;
                params.useFragmentShader              = fragment;
                params.enableMeshShader               = true;
                params.enableTaskShader               = task;
                params.enableMaintenance4             = true;
                params.useSecondaryCommandBuffer      = secondary;
                params.seed                           = baseSeed ^ deStringHash(testName.c_str());
                graphicsGroup->addChild(new DescriptorHeapTestCaseGraphics(testCtx, testName, params));
            }
        }
    }

    topGroup->addChild(graphicsGroup.release());
}

void populateGraphicsAndComputeTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "graphics_and_compute"));
    TestParamsBasic params{};
    params.queue                                = VK_QUEUE_GRAPHICS_BIT;
    params.seed                                 = baseSeed ^ deStringHash("graphics_and_compute");
    params.enableVertexPipelineStoresAndAtomics = true;
    group->addChild(new DescriptorHeapTestCaseGraphicsAndCompute(testCtx, "graphics_and_compute", params));
    topGroup->addChild(group.release());
}

void populateDifferentMappingsSameShader(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "different_mappings_same_shader"));
    TestParamsBasic params{};
    params.queue = VK_QUEUE_COMPUTE_BIT;
    params.seed  = baseSeed ^ deStringHash("different_mappings_same_shader");
    group->addChild(
        new DescriptorHeapTestCaseDifferentMappingsSameShader(testCtx, "different_mappings_same_shader", params));
    topGroup->addChild(group.release());
}

void populateNonUniformMappings(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "non_uniform_mappings"));
    TestParams params{};
    params.queue                                      = VK_QUEUE_COMPUTE_BIT;
    params.enableStorageBufferArrayNonUniformIndexing = true;
    params.enableRuntimeDescriptorArray               = true;
    group->addChild(new DescriptorHeapTestCaseNonUniformMappings(testCtx, "non_uniform_mappings", params));
    topGroup->addChild(group.release());
}

void populateMSAAImageReadTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> msaaGroup(new tcu::TestCaseGroup(testCtx, "msaa_image_read"));

    TestParams params{};
    params.queue                   = VK_QUEUE_GRAPHICS_BIT;
    params.enableSampleRateShading = true;
    params.seed                    = baseSeed ^ deStringHash("msaa_image_read");

    msaaGroup->addChild(new DescriptorHeapTestCaseMSAAImageRead(testCtx, "msaa_image_read", params));
    topGroup->addChild(msaaGroup.release());
}

void populateResourceHeapAccessTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> resourceHeapAccessGroup(new tcu::TestCaseGroup(testCtx, "resource_heap_access"));

    for (const VkQueueFlagBits queue : {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT})
    {
        const char *const testName = (queue == VK_QUEUE_GRAPHICS_BIT) ? "graphics" : "compute";

        TestParams params{};
        params.queue = queue;
        params.seed  = baseSeed ^ deStringHash(testName);
        resourceHeapAccessGroup->addChild(new DescriptorHeapTestCaseResourceHeapAccess(testCtx, testName, params));
    }
    topGroup->addChild(resourceHeapAccessGroup.release());
}

void populateSamplerHeapAccessTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> samplerHeapAccessGroup(new tcu::TestCaseGroup(testCtx, "sampler_heap_access"));

    for (const VkQueueFlagBits queue : {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT})
    {
        const char *const testName = (queue == VK_QUEUE_GRAPHICS_BIT) ? "graphics" : "compute";

        TestParams params{};
        params.queue = queue;
        samplerHeapAccessGroup->addChild(new DescriptorHeapTestCaseSamplerHeapAccess(testCtx, testName, params));
    }
    topGroup->addChild(samplerHeapAccessGroup.release());
}

void populateSecondaryCommandBufferTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "secondary"));

    struct TestType
    {
        SecondaryCopyType type;
        const char *name;
    } testTypes[] = {
        {SecondaryCopyType::NONE, "none"},
        {SecondaryCopyType::RESOURCE_HEAP_COMMAND_COPY, "resource_heap_command_copy"},
        {SecondaryCopyType::RESOURCE_HEAP_SHADER_COPY, "resource_heap_shader_copy"},
        {SecondaryCopyType::SAMPLER_HEAP_COMMAND_COPY, "sampler_heap_command_copy"},
        {SecondaryCopyType::SAMPLER_HEAP_SHADER_COPY, "sampler_heap_shader_copy"},
    };

    for (const VkQueueFlagBits queue : {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT})
    {
        const std::string queueName = (queue == VK_QUEUE_GRAPHICS_BIT) ? "graphics" : "compute";

        for (const auto testType : testTypes)
        {
            for (const bool copyInSecondary : {false, true})
            {
                if ((testType.type == SecondaryCopyType::NONE || queue != VK_QUEUE_COMPUTE_BIT) && copyInSecondary)
                {
                    continue;
                }

                const std::string testName = queueName + "_" + testType.name + (copyInSecondary ? "_in_secondary" : "");

                SecondaryTestParams params{};
                params.queue           = queue;
                params.seed            = baseSeed ^ deStringHash(testName.c_str());
                params.testType        = testType.type;
                params.copyInSecondary = copyInSecondary;
                group->addChild(new DescriptorHeapTestCaseSecondary(testCtx, testName, params));
            }
        }
    }

    topGroup->addChild(group.release());
}

void populateShaderObjectInvariance(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "shader_object_invariance"));

    TestParams params{};
    params.queue              = VK_QUEUE_GRAPHICS_BIT;
    params.enableShaderObject = true;
    group->addChild(new DescriptorHeapTestCaseShaderObjectInvariance(testCtx, "shader_object_invariance", params));
    topGroup->addChild(group.release());
}

void populatePushDataAccessTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "push_data_access"));

    TestParams params{};
    params.queue = VK_QUEUE_COMPUTE_BIT;
    params.seed  = baseSeed ^ deStringHash("push_data_access");
    group->addChild(new DescriptorHeapTestCasePushDataAccess(testCtx, "push_data_access", params));
    topGroup->addChild(group.release());
}

void populateNonUniformAccessTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    static constexpr VkDescriptorType descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "non_uniform_access"));

    for (const VkDescriptorType descriptorType : descriptorTypes)
    {
        const char *const testName = getDescriptorTypeTestName(descriptorType);

        TestParamsWithDescriptorType params{};
        params.queue                        = VK_QUEUE_COMPUTE_BIT;
        params.descriptorType               = descriptorType;
        params.enableRuntimeDescriptorArray = true;
        params.seed                         = baseSeed ^ deStringHash(testName);
        params.descriptorType               = descriptorType;

        switch (descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            params.enableUniformTexelBufferArrayNonUniformIndexing    = true;
            params.enableShaderUniformTexelBufferArrayDynamicIndexing = true;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            params.enableStorageTexelBufferArrayNonUniformIndexing    = true;
            params.enableShaderStorageTexelBufferArrayDynamicIndexing = true;
            break;
        default:
            break;
        }

        group->addChild(new DescriptorHeapTestCaseNonUniformAccess(testCtx, testName, params));
    }
    topGroup->addChild(group.release());
}

void populateSpecialHeapTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> specialHeapGroup(new tcu::TestCaseGroup(testCtx, "special_heap"));
    const uint32_t specialHeapGroupHash = baseSeed ^ deStringHash("special_heap");

    VkDescriptorType const descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    for (const bool enableSparseHeap : {false, true})
    {
        for (const bool enableProtectedHeap : {false, true})
        {
            std::string specialModeName;

            if (enableSparseHeap && enableProtectedHeap)
                specialModeName = "sparse_and_protected";
            else if (enableSparseHeap)
                specialModeName = "sparse";
            else if (enableProtectedHeap)
                specialModeName = "protected";
            else
                continue;

            MovePtr<tcu::TestCaseGroup> specialModeGroup(new tcu::TestCaseGroup(testCtx, specialModeName.c_str()));

            const uint32_t specialModeHash = specialHeapGroupHash ^ deStringHash(specialModeName.c_str());

            for (const VkDescriptorType descriptorType : descriptorTypes)
            {
                const char *testName = getDescriptorTypeTestName(descriptorType);

                TestParamsBasic params{};
                params.stage               = VK_SHADER_STAGE_COMPUTE_BIT;
                params.queue               = VK_QUEUE_COMPUTE_BIT;
                params.dimension           = 8;
                params.seed                = specialModeHash ^ deStringHash(testName);
                params.enableSparseHeap    = enableSparseHeap;
                params.enableProtectedHeap = enableProtectedHeap;

                de::Random rng(~params.seed);

                ShaderBinding &binding = params.bindings.emplace_back();
                binding.descriptorType = descriptorType;
                binding.descriptorSet  = 1;
                binding.firstBinding   = rng.getInt(0, 15);
                binding.arrayed        = true;

                binding.mapping               = initVulkanStructure();
                binding.mapping.descriptorSet = binding.descriptorSet;
                binding.mapping.firstBinding  = binding.firstBinding;
                binding.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                binding.mapping.bindingCount  = params.dimension;
                binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;

                binding.heapIndex = rng.getInt(8, kMaxDescriptor - params.dimension);

                binding.mapping.sourceData.constantOffset.heapOffset      = binding.heapIndex;
                binding.mapping.sourceData.constantOffset.heapArrayStride = 1;

                switch (descriptorType)
                {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                {
                    params.embeddedSamplers = true;

                    binding.imageBindingUid    = 1;
                    binding.shiftSamplerResult = true;

                    ShaderBinding &sampledImage = params.bindings.emplace_back();
                    sampledImage.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    sampledImage.descriptorSet  = 0;
                    sampledImage.firstBinding   = 3;

                    sampledImage.mapping               = initVulkanStructure();
                    sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                    sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                    sampledImage.mapping.bindingCount  = 1;
                    sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                    sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                    sampledImage.mapping.sourceData.constantOffset.heapOffset      = sampledImage.firstBinding;
                    sampledImage.mapping.sourceData.constantOffset.heapArrayStride = 1;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    params.enableRayQuery               = true;
                    params.enableAccelerationStructures = true;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    params.enableSampledImageArrayNonUniformIndexing = binding.arrayed;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    params.enableStorageImageArrayNonUniformIndexing = binding.arrayed;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    params.enableUniformTexelBufferArrayNonUniformIndexing = binding.arrayed;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    params.enableStorageTexelBufferArrayNonUniformIndexing = binding.arrayed;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    params.enableUniformBufferArrayNonUniformIndexing = binding.arrayed;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    params.enableStorageBufferArrayNonUniformIndexing = binding.arrayed;
                    break;
                default:
                    break;
                }

                specialModeGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }

            specialHeapGroup->addChild(specialModeGroup.release());
        }
    }

    topGroup->addChild(specialHeapGroup.release());
}

static void populateNonPackedTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> nonPackedGroup(new tcu::TestCaseGroup(testCtx, "non_packed"));
    const uint32_t bindingMappingGroupHash = baseSeed ^ deStringHash("non_packed");

    VkDescriptorMappingSourceEXT const mappingSources[] = {
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT,
    };

    const uint32_t samplerMaxSize = 32;
    const uint32_t imageMaxSize   = 64;
    const uint32_t bufferMaxSize  = 128;

    std::pair<VkDescriptorType, uint32_t> const descriptorTypes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, imageMaxSize}, // Intentionally using imageMaxSize instead of samplerMaxSize.
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferMaxSize},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, bufferMaxSize},
    };

    for (const VkDescriptorMappingSourceEXT mappingSource : mappingSources)
    {
        const char *mappingSourceName = getMappingSourceTestName(mappingSource);
        MovePtr<tcu::TestCaseGroup> mappingSourceGroup(new tcu::TestCaseGroup(testCtx, mappingSourceName));

        const uint32_t mappingSourceGroupHash = bindingMappingGroupHash ^ deStringHash(mappingSourceName);

        for (const auto &pair : descriptorTypes)
        {
            for (const uint32_t factor : {1, 3, 4})
            {
                const VkDescriptorType descriptorType = pair.first;
                const uint32_t overrideStride         = pair.second * factor;

                std::string testName = getDescriptorTypeTestName(descriptorType);
                testName += "_stride_" + std::to_string(overrideStride);

                TestParamsBasic params{};
                params.stage                  = VK_SHADER_STAGE_COMPUTE_BIT;
                params.queue                  = VK_QUEUE_COMPUTE_BIT;
                params.dimension              = 8;
                params.seed                   = mappingSourceGroupHash ^ deStringHash(testName.c_str());
                params.overrideResourceStride = overrideStride;
                params.overrideSamplerStride  = samplerMaxSize * factor;

                de::Random rng(~params.seed);

                ShaderBinding &binding = params.bindings.emplace_back();
                binding.descriptorType = descriptorType;
                binding.descriptorSet  = 1;
                binding.firstBinding   = rng.getInt(0, 15);
                binding.arrayed        = true;

                binding.mapping               = initVulkanStructure();
                binding.mapping.descriptorSet = binding.descriptorSet;
                binding.mapping.firstBinding  = binding.firstBinding;
                binding.mapping.source        = mappingSource;
                binding.mapping.bindingCount  = params.dimension;
                binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;

                binding.heapIndex = rng.getInt(8, kMaxDescriptor - params.dimension);

                switch (mappingSource)
                {
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT:
                    binding.mapping.sourceData.constantOffset.heapOffset      = binding.heapIndex;
                    binding.mapping.sourceData.constantOffset.heapArrayStride = 1;
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
                    binding.mapping.sourceData.pushIndex.heapOffset      = 2;
                    binding.mapping.sourceData.pushIndex.pushOffset      = rng.getInt(1, 8) * 4;
                    binding.mapping.sourceData.pushIndex.heapIndexStride = 1;
                    binding.mapping.sourceData.pushIndex.heapArrayStride = 1;
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
                    binding.mapping.sourceData.indirectIndex.heapOffset      = 2;
                    binding.mapping.sourceData.indirectIndex.pushOffset      = rng.getInt(1, 4) * 8;
                    binding.mapping.sourceData.indirectIndex.addressOffset   = rng.getInt(1, 8) * 8;
                    binding.mapping.sourceData.indirectIndex.heapIndexStride = 1;
                    binding.mapping.sourceData.indirectIndex.heapArrayStride = 1;
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
                    binding.mapping.sourceData.shaderRecordIndex.heapOffset         = 2;
                    binding.mapping.sourceData.shaderRecordIndex.shaderRecordOffset = rng.getInt(1, 4) * 4;
                    binding.mapping.sourceData.shaderRecordIndex.heapIndexStride    = 1;
                    binding.mapping.sourceData.shaderRecordIndex.heapArrayStride    = 1;
                    params.enableRayTracing                                         = true;
                    params.enableAccelerationStructures                             = true;
                    params.stage                                                    = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
                    break;
                case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT:
                    binding.mapping.sourceData.indirectIndexArray.heapOffset      = 2;
                    binding.mapping.sourceData.indirectIndexArray.pushOffset      = rng.getInt(1, 4) * 8;
                    binding.mapping.sourceData.indirectIndexArray.addressOffset   = rng.getInt(1, 8) * 8;
                    binding.mapping.sourceData.indirectIndexArray.heapIndexStride = 1;
                    break;
                default:
                    DE_ASSERT(0);
                    break;
                }

                switch (descriptorType)
                {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                {
                    params.embeddedSamplers = true;

                    binding.imageBindingUid    = 1;
                    binding.shiftSamplerResult = true;

                    ShaderBinding &sampledImage = params.bindings.emplace_back();
                    sampledImage.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    sampledImage.descriptorSet  = 0;
                    sampledImage.firstBinding   = 3;

                    sampledImage.mapping               = initVulkanStructure();
                    sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                    sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                    sampledImage.mapping.bindingCount  = 1;
                    sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                    sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                    sampledImage.mapping.sourceData.constantOffset.heapOffset      = sampledImage.firstBinding;
                    sampledImage.mapping.sourceData.constantOffset.heapArrayStride = 1;
                    break;
                }
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    params.enableRayQuery               = true;
                    params.enableAccelerationStructures = true;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    params.enableSampledImageArrayNonUniformIndexing = true;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    params.enableStorageImageArrayNonUniformIndexing = true;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    params.enableUniformTexelBufferArrayNonUniformIndexing = true;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    params.enableStorageTexelBufferArrayNonUniformIndexing = true;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    params.enableUniformBufferArrayNonUniformIndexing = true;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    params.enableStorageBufferArrayNonUniformIndexing = true;
                    break;
                default:
                    break;
                }

                mappingSourceGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
            }
        }

        nonPackedGroup->addChild(mappingSourceGroup.release());
    }

    topGroup->addChild(nonPackedGroup.release());
}

static void populateUnalignedTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> unalignedGroup(new tcu::TestCaseGroup(testCtx, "unaligned"));
    const uint32_t bindingMappingGroupHash = baseSeed ^ deStringHash("unaligned");

    VkDescriptorMappingSourceEXT const mappingSources[] = {
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT,
    };

    const uint32_t imageMaxSize  = 64;
    const uint32_t bufferMaxSize = 128;

    std::pair<VkDescriptorType, uint32_t> const descriptorTypes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, imageMaxSize}, // Intentionally using imageMaxSize.
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, imageMaxSize},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferMaxSize},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferMaxSize},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, bufferMaxSize},
    };

    for (const VkDescriptorMappingSourceEXT mappingSource : mappingSources)
    {
        const char *mappingSourceName = getMappingSourceTestName(mappingSource);
        MovePtr<tcu::TestCaseGroup> mappingSourceGroup(new tcu::TestCaseGroup(testCtx, mappingSourceName));

        const uint32_t mappingSourceGroupHash = bindingMappingGroupHash ^ deStringHash(mappingSourceName);

        for (const auto &pair : descriptorTypes)
        {
            const VkDescriptorType descriptorType = pair.first;
            const uint32_t overrideStride         = pair.second;

            const char *const testName = getDescriptorTypeTestName(descriptorType);

            TestParamsBasic params{};
            params.stage                  = VK_SHADER_STAGE_COMPUTE_BIT;
            params.queue                  = VK_QUEUE_COMPUTE_BIT;
            params.dimension              = 8;
            params.seed                   = mappingSourceGroupHash ^ deStringHash(testName);
            params.scaledMappingStrides   = false;
            params.overrideResourceStride = overrideStride;
            params.overrideSamplerStride  = overrideStride;

            de::Random rng(~params.seed);

            ShaderBinding &binding = params.bindings.emplace_back();
            binding.descriptorType = descriptorType;
            binding.descriptorSet  = 1;
            binding.firstBinding   = rng.getInt(0, 15);
            binding.arrayed        = true;

            binding.mapping               = initVulkanStructure();
            binding.mapping.descriptorSet = binding.descriptorSet;
            binding.mapping.firstBinding  = binding.firstBinding;
            binding.mapping.source        = mappingSource;
            binding.mapping.bindingCount  = params.dimension;
            binding.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;

            binding.heapIndex = rng.getInt(8, kMaxDescriptor - params.dimension);

            switch (mappingSource)
            {
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT:
                binding.mapping.sourceData.pushIndex.heapOffset      = 2 * overrideStride;
                binding.mapping.sourceData.pushIndex.pushOffset      = rng.getInt(1, 8) * 4;
                binding.mapping.sourceData.pushIndex.heapIndexStride = 1;
                binding.mapping.sourceData.pushIndex.heapArrayStride = overrideStride;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT:
                binding.mapping.sourceData.indirectIndex.heapOffset      = 2 * overrideStride;
                binding.mapping.sourceData.indirectIndex.pushOffset      = rng.getInt(1, 4) * 8;
                binding.mapping.sourceData.indirectIndex.addressOffset   = rng.getInt(1, 8) * 8;
                binding.mapping.sourceData.indirectIndex.heapIndexStride = 1;
                binding.mapping.sourceData.indirectIndex.heapArrayStride = overrideStride;
                break;
            case VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT:
                binding.mapping.sourceData.shaderRecordIndex.heapOffset         = 2 * overrideStride;
                binding.mapping.sourceData.shaderRecordIndex.shaderRecordOffset = rng.getInt(1, 4) * 4;
                binding.mapping.sourceData.shaderRecordIndex.heapIndexStride    = 1;
                binding.mapping.sourceData.shaderRecordIndex.heapArrayStride    = overrideStride;
                params.enableRayTracing                                         = true;
                params.enableAccelerationStructures                             = true;
                params.stage                                                    = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
                break;
            default:
                DE_ASSERT(0);
                break;
            }

            switch (descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                params.embeddedSamplers = true;

                binding.imageBindingUid    = 1;
                binding.shiftSamplerResult = true;

                ShaderBinding &sampledImage = params.bindings.emplace_back();
                sampledImage.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                sampledImage.descriptorSet  = 0;
                sampledImage.firstBinding   = 3;

                sampledImage.mapping               = initVulkanStructure();
                sampledImage.mapping.descriptorSet = sampledImage.descriptorSet;
                sampledImage.mapping.firstBinding  = sampledImage.firstBinding;
                sampledImage.mapping.bindingCount  = 1;
                sampledImage.mapping.resourceMask  = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
                sampledImage.mapping.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
                sampledImage.mapping.sourceData.constantOffset.heapOffset = sampledImage.firstBinding * overrideStride;
                sampledImage.mapping.sourceData.constantOffset.heapArrayStride = overrideStride;
                break;
            }
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                params.enableRayQuery               = true;
                params.enableAccelerationStructures = true;
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                params.enableSampledImageArrayNonUniformIndexing = true;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                params.enableStorageImageArrayNonUniformIndexing = true;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                params.enableUniformTexelBufferArrayNonUniformIndexing = true;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                params.enableStorageTexelBufferArrayNonUniformIndexing = true;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                params.enableUniformBufferArrayNonUniformIndexing = true;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                params.enableStorageBufferArrayNonUniformIndexing = true;
                break;
            default:
                break;
            }

            mappingSourceGroup->addChild(new DescriptorHeapTestCaseBasic(testCtx, testName, params));
        }

        unalignedGroup->addChild(mappingSourceGroup.release());
    }

    topGroup->addChild(unalignedGroup.release());
}

void populateZeroStrideTests(tcu::TestCaseGroup *topGroup, uint32_t baseSeed)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "zero_stride"));

    const VkDescriptorMappingSourceEXT mappingSources[] = {
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT,
    };

    for (const auto mappingSource : mappingSources)
    {
        const std::string testName = "buffer_" + std::string(getMappingSourceTestName(mappingSource));
        TestParamsZeroStride params{};
        params.queue                                      = VK_QUEUE_COMPUTE_BIT;
        params.seed                                       = baseSeed ^ deStringHash(testName.c_str());
        params.enableUniformBufferArrayNonUniformIndexing = true;
        params.enableStorageBufferArrayNonUniformIndexing = true;
        params.mappingSource                              = mappingSource;
        group->addChild(new DescriptorHeapTestCaseZeroStride(testCtx, testName, params));
    }

    for (const auto mappingSource : mappingSources)
    {
        const std::string testName = "image_sampler_" + std::string(getMappingSourceTestName(mappingSource));
        TestParamsZeroStride params{};
        params.queue                                      = VK_QUEUE_COMPUTE_BIT;
        params.seed                                       = baseSeed ^ deStringHash(testName.c_str());
        params.enableSampledImageArrayNonUniformIndexing  = true;
        params.enableStorageBufferArrayNonUniformIndexing = true;
        params.mappingSource                              = mappingSource;
        params.useImageSampler                            = true;
        group->addChild(new DescriptorHeapTestCaseZeroStride(testCtx, testName, params));
    }

    topGroup->addChild(group.release());
}

void populateDescriptorHeapTests(tcu::TestCaseGroup *topGroup)
{
    tcu::TestContext &testCtx = topGroup->getTestContext();
    const uint32_t baseSeed   = static_cast<uint32_t>(testCtx.getCommandLine().getBaseSeed());

    populateLimitsTests(topGroup);
    populateBasicTests(topGroup, baseSeed);
    populateInvarianceTests(topGroup, baseSeed);
    populateDynamicIndexingTests(topGroup, baseSeed);
    populateBindingMappingTests(topGroup, baseSeed);
    populateHighBindingTests(topGroup, baseSeed);
    populateCombinedImageSamplerTests(topGroup, baseSeed);
    populateReservedHeapTests(topGroup, baseSeed);
    populatePushDataTests(topGroup, baseSeed);
    populateNullDescriptorTests(topGroup, baseSeed);
    populateYcbcrTests(topGroup, baseSeed);
    populateDifferentMappingsPerShader(topGroup, baseSeed);
    populateGraphicsPipelineLibraryTests(topGroup, baseSeed);
    populateSwitchHeapsTests(topGroup, baseSeed);
    populateConcurrentQueuesTests(topGroup, baseSeed);
    populateConcurrentHeapSetTests(topGroup, baseSeed);
    populateStateInvalidationTests(topGroup, baseSeed);
    populateWriteAfterRecordTests(topGroup, baseSeed);
    populateSpirvTests(topGroup);
    populateResourceMaskingTests(topGroup);
    populateNullImageQueriesTests(topGroup);
    populateGraphicsTests(topGroup, baseSeed);
    populateGraphicsAndComputeTests(topGroup, baseSeed);
    populateDifferentMappingsSameShader(topGroup, baseSeed);
    populateNonUniformMappings(topGroup);
    populateMSAAImageReadTests(topGroup, baseSeed);
    populateResourceHeapAccessTests(topGroup, baseSeed);
    populateSamplerHeapAccessTests(topGroup);
    populateShaderObjectInvariance(topGroup);
    populatePushDataAccessTests(topGroup, baseSeed);
    populateNonUniformAccessTests(topGroup, baseSeed);
    populateSpecialHeapTests(topGroup, baseSeed);
    populateNonPackedTests(topGroup, baseSeed);
    populateUnalignedTests(topGroup, baseSeed);
    populateSecondaryCommandBufferTests(topGroup, baseSeed);
    populateZeroStrideTests(topGroup, baseSeed);
}

} // namespace

tcu::TestCaseGroup *createDescriptorHeapTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "descriptor_heap", populateDescriptorHeapTests);
}

} // namespace BindingModel
} // namespace vkt
