/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Utilities for creating commonly used Vulkan objects
 *//*--------------------------------------------------------------------*/

#include "vkRayTracingUtil.hpp"

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"

#include <vector>
#include <string>
#include <thread>
#include <limits>
#include <type_traits>
#include <map>

#include "SPIRV/spirv.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC

static const uint32_t WATCHDOG_INTERVAL = 16384; // Touch watchDog every N iterations.

struct DeferredThreadParams
{
    const DeviceInterface &vk;
    VkDevice device;
    VkDeferredOperationKHR deferredOperation;
    VkResult result;
};

std::string getFormatSimpleName(vk::VkFormat format)
{
    constexpr size_t kPrefixLen = 10; // strlen("VK_FORMAT_")
    return de::toLower(de::toString(format).substr(kPrefixLen));
}

bool pointInTriangle2D(const tcu::Vec3 &p, const tcu::Vec3 &p0, const tcu::Vec3 &p1, const tcu::Vec3 &p2)
{
    float s = p0.y() * p2.x() - p0.x() * p2.y() + (p2.y() - p0.y()) * p.x() + (p0.x() - p2.x()) * p.y();
    float t = p0.x() * p1.y() - p0.y() * p1.x() + (p0.y() - p1.y()) * p.x() + (p1.x() - p0.x()) * p.y();

    if ((s < 0) != (t < 0))
        return false;

    float a = -p1.y() * p2.x() + p0.y() * (p2.x() - p1.x()) + p0.x() * (p1.y() - p2.y()) + p1.x() * p2.y();

    return a < 0 ? (s <= 0 && s + t >= a) : (s >= 0 && s + t <= a);
}

// Returns true if VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR needs to be supported for the given format.
static bool isMandatoryAccelerationStructureVertexBufferFormat(vk::VkFormat format)
{
    bool mandatory = false;

    switch (format)
    {
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
        mandatory = true;
        break;
    default:
        break;
    }

    return mandatory;
}

void checkAccelerationStructureVertexBufferFormat(const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                                  vk::VkFormat format)
{
    const vk::VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);

    if ((formatProperties.bufferFeatures & vk::VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR) == 0u)
    {
        const std::string errorMsg = "Format not supported for acceleration structure vertex buffers";
        if (isMandatoryAccelerationStructureVertexBufferFormat(format))
            TCU_FAIL(errorMsg);
        TCU_THROW(NotSupportedError, errorMsg);
    }
}

std::string getCommonRayGenerationShader(void)
{
    return "#version 460 core\n"
           "#extension GL_EXT_ray_tracing : require\n"
           "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
           "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
           "\n"
           "void main()\n"
           "{\n"
           "  uint  rayFlags = 0;\n"
           "  uint  cullMask = 0xFF;\n"
           "  float tmin     = 0.0;\n"
           "  float tmax     = 9.0;\n"
           "  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), "
           "(float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
           "  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
           "  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
           "}\n";
}

RaytracedGeometryBase::RaytracedGeometryBase(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                             VkIndexType indexType)
    : m_geometryType(geometryType)
    , m_vertexFormat(vertexFormat)
    , m_indexType(indexType)
    , m_geometryFlags((VkGeometryFlagsKHR)0u)
    , m_hasOpacityMicromap(false)
{
    if (m_geometryType == VK_GEOMETRY_TYPE_AABBS_KHR)
        DE_ASSERT(m_vertexFormat == VK_FORMAT_R32G32B32_SFLOAT);
}

RaytracedGeometryBase::~RaytracedGeometryBase()
{
}

struct GeometryBuilderParams
{
    VkGeometryTypeKHR geometryType;
    bool usePadding;
};

template <typename V, typename I>
RaytracedGeometryBase *buildRaytracedGeometry(const GeometryBuilderParams &params)
{
    return new RaytracedGeometry<V, I>(params.geometryType, (params.usePadding ? 1u : 0u));
}

de::SharedPtr<RaytracedGeometryBase> makeRaytracedGeometry(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                                           VkIndexType indexType, bool padVertices)
{
    const GeometryBuilderParams builderParams{geometryType, padVertices};

    switch (vertexFormat)
    {
    case VK_FORMAT_R32G32_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec2, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec2, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec2, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R32G32B32_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec3, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec3, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec3, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec4, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec4, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::Vec4, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_16, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_16, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_16, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16B16_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_16, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_16, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_16, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_16, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_16, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_16, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_16SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_16SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(
                buildRaytracedGeometry<Vec2_16SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16B16_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_16SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_16SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(
                buildRaytracedGeometry<Vec3_16SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R16G16B16A16_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_16SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_16SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(
                buildRaytracedGeometry<Vec4_16SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R64G64_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec2, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec2, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec2, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R64G64B64_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec3, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec3, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec3, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec4, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec4, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<tcu::DVec4, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R8G8_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_8SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_8SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec2_8SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R8G8B8_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_8SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_8SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec3_8SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    case VK_FORMAT_R8G8B8A8_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_8SNorm, uint16_t>(builderParams));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_8SNorm, uint32_t>(builderParams));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(buildRaytracedGeometry<Vec4_8SNorm, EmptyIndex>(builderParams));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        }
    default:
        TCU_THROW(InternalError, "Wrong vertex format");
    }
}

VkDeviceAddress getBufferDeviceAddress(const DeviceInterface &vk, const VkDevice device, const VkBuffer buffer,
                                       VkDeviceSize offset)
{

    if (buffer == VK_NULL_HANDLE)
        return 0;

    VkBufferDeviceAddressInfo deviceAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType    sType
        nullptr,                                      // const void*        pNext
        buffer                                        // VkBuffer           buffer;
    };
    return vk.getBufferDeviceAddress(device, &deviceAddressInfo) + offset;
}

static inline Move<VkQueryPool> makeQueryPool(const DeviceInterface &vk, const VkDevice device,
                                              const VkQueryType queryType, uint32_t queryCount)
{
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        (VkQueryPoolCreateFlags)0,                // flags
        queryType,                                // queryType
        queryCount,                               // queryCount
        0u,                                       // pipelineStatistics
    };
    return createQueryPool(vk, device, &queryPoolCreateInfo);
}

static inline VkAccelerationStructureGeometryDataKHR makeVkAccelerationStructureGeometryDataKHR(
    const VkAccelerationStructureGeometryTrianglesDataKHR &triangles)
{
    VkAccelerationStructureGeometryDataKHR result;

    deMemset(&result, 0, sizeof(result));

    result.triangles = triangles;

    return result;
}

static inline VkAccelerationStructureGeometryDataKHR makeVkAccelerationStructureGeometryDataKHR(
    const VkAccelerationStructureGeometryAabbsDataKHR &aabbs)
{
    VkAccelerationStructureGeometryDataKHR result;

    deMemset(&result, 0, sizeof(result));

    result.aabbs = aabbs;

    return result;
}

static inline VkAccelerationStructureGeometryDataKHR makeVkAccelerationStructureInstancesDataKHR(
    const VkAccelerationStructureGeometryInstancesDataKHR &instances)
{
    VkAccelerationStructureGeometryDataKHR result;

    deMemset(&result, 0, sizeof(result));

    result.instances = instances;

    return result;
}

static inline VkAccelerationStructureInstanceKHR makeVkAccelerationStructureInstanceKHR(
    const VkTransformMatrixKHR &transform, uint32_t instanceCustomIndex, uint32_t mask,
    uint32_t instanceShaderBindingTableRecordOffset, VkGeometryInstanceFlagsKHR flags,
    uint64_t accelerationStructureReference)
{
    VkAccelerationStructureInstanceKHR instance     = {transform, 0, 0, 0, 0, accelerationStructureReference};
    instance.instanceCustomIndex                    = instanceCustomIndex & 0xFFFFFF;
    instance.mask                                   = mask & 0xFF;
    instance.instanceShaderBindingTableRecordOffset = instanceShaderBindingTableRecordOffset & 0xFFFFFF;
    instance.flags                                  = flags & 0xFF;
    return instance;
}

VkResult getRayTracingShaderGroupHandlesKHR(const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline,
                                            const uint32_t firstGroup, const uint32_t groupCount,
                                            const uintptr_t dataSize, void *pData)
{
    return vk.getRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult getRayTracingShaderGroupHandles(const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline,
                                         const uint32_t firstGroup, const uint32_t groupCount, const uintptr_t dataSize,
                                         void *pData)
{
    return getRayTracingShaderGroupHandlesKHR(vk, device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult getRayTracingCaptureReplayShaderGroupHandles(const DeviceInterface &vk, const VkDevice device,
                                                      const VkPipeline pipeline, const uint32_t firstGroup,
                                                      const uint32_t groupCount, const uintptr_t dataSize, void *pData)
{
    return vk.getRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize,
                                                              pData);
}

VkResult finishDeferredOperation(const DeviceInterface &vk, VkDevice device, VkDeferredOperationKHR deferredOperation)
{
    VkResult result = vk.deferredOperationJoinKHR(device, deferredOperation);

    while (result == VK_THREAD_IDLE_KHR)
    {
        std::this_thread::yield();
        result = vk.deferredOperationJoinKHR(device, deferredOperation);
    }

    switch (result)
    {
    case VK_SUCCESS:
    {
        // Deferred operation has finished. Query its result
        result = vk.getDeferredOperationResultKHR(device, deferredOperation);

        break;
    }

    case VK_THREAD_DONE_KHR:
    {
        // Deferred operation is being wrapped up by another thread
        // wait for that thread to finish
        do
        {
            std::this_thread::yield();
            result = vk.getDeferredOperationResultKHR(device, deferredOperation);
        } while (result == VK_NOT_READY);

        break;
    }

    default:
    {
        DE_ASSERT(false);

        break;
    }
    }

    return result;
}

void finishDeferredOperationThreaded(DeferredThreadParams *deferredThreadParams)
{
    deferredThreadParams->result = finishDeferredOperation(deferredThreadParams->vk, deferredThreadParams->device,
                                                           deferredThreadParams->deferredOperation);
}

void finishDeferredOperation(const DeviceInterface &vk, VkDevice device, VkDeferredOperationKHR deferredOperation,
                             const uint32_t workerThreadCount, const bool operationNotDeferred)
{

    if (operationNotDeferred)
    {
        // when the operation deferral returns VK_OPERATION_NOT_DEFERRED_KHR,
        // the deferred operation should act as if no command was deferred
        VK_CHECK(vk.getDeferredOperationResultKHR(device, deferredOperation));

        // there is not need to join any threads to the deferred operation,
        // so below can be skipped.
        return;
    }

    if (workerThreadCount == 0)
    {
        VK_CHECK(finishDeferredOperation(vk, device, deferredOperation));
    }
    else
    {
        const uint32_t maxThreadCountSupported =
            deMinu32(256u, vk.getDeferredOperationMaxConcurrencyKHR(device, deferredOperation));
        const uint32_t requestedThreadCount = workerThreadCount;
        const uint32_t testThreadCount      = requestedThreadCount == std::numeric_limits<uint32_t>::max() ?
                                                  maxThreadCountSupported :
                                                  requestedThreadCount;

        if (maxThreadCountSupported == 0)
            TCU_FAIL("vkGetDeferredOperationMaxConcurrencyKHR must not return 0");

        const DeferredThreadParams deferredThreadParams = {
            vk,                 //  const DeviceInterface& vk;
            device,             //  VkDevice device;
            deferredOperation,  //  VkDeferredOperationKHR deferredOperation;
            VK_RESULT_MAX_ENUM, //  VResult result;
        };
        std::vector<DeferredThreadParams> threadParams(testThreadCount, deferredThreadParams);
        std::vector<de::MovePtr<std::thread>> threads(testThreadCount);
        bool executionResult = false;

        DE_ASSERT(threads.size() > 0 && threads.size() == testThreadCount);

        for (uint32_t threadNdx = 0; threadNdx < testThreadCount; ++threadNdx)
            threads[threadNdx] =
                de::MovePtr<std::thread>(new std::thread(finishDeferredOperationThreaded, &threadParams[threadNdx]));

        for (uint32_t threadNdx = 0; threadNdx < testThreadCount; ++threadNdx)
            threads[threadNdx]->join();

        for (uint32_t threadNdx = 0; threadNdx < testThreadCount; ++threadNdx)
            if (threadParams[threadNdx].result == VK_SUCCESS)
                executionResult = true;

        if (!executionResult)
            TCU_FAIL("Neither reported VK_SUCCESS");
    }
}

SerialStorage::SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                             const VkAccelerationStructureBuildTypeKHR buildType, const VkDeviceSize storageSize)
    : m_buildType(buildType)
    , m_storageSize(storageSize)
    , m_serialInfo()
{
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(storageSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    try
    {
        m_buffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                 MemoryRequirement::Cached | MemoryRequirement::HostVisible |
                                     MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    }
    catch (const tcu::NotSupportedError &)
    {
        // retry without Cached flag
        m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, bufferCreateInfo,
            MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    }
}

SerialStorage::SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                             const VkAccelerationStructureBuildTypeKHR buildType, const SerialInfo &serialInfo)
    : m_buildType(buildType)
    , m_storageSize(serialInfo.sizes()[0]) // raise assertion if serialInfo is empty
    , m_serialInfo(serialInfo)
{
    DE_ASSERT(serialInfo.sizes().size() >= 2u);

    // create buffer for top-level acceleration structure
    {
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(m_storageSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, bufferCreateInfo,
            MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    }

    // create buffers for bottom-level acceleration structures
    {
        std::vector<uint64_t> addrs;

        for (std::size_t i = 1; i < serialInfo.addresses().size(); ++i)
        {
            const uint64_t &lookAddr = serialInfo.addresses()[i];
            auto end                 = addrs.end();
            auto match = std::find_if(addrs.begin(), end, [&](const uint64_t &item) { return item == lookAddr; });
            if (match == end)
            {
                addrs.emplace_back(lookAddr);
                m_bottoms.emplace_back(de::SharedPtr<SerialStorage>(
                    new SerialStorage(vk, device, allocator, buildType, serialInfo.sizes()[i])));
            }
        }
    }
}

VkDeviceOrHostAddressKHR SerialStorage::getAddress(const DeviceInterface &vk, const VkDevice device,
                                                   const VkAccelerationStructureBuildTypeKHR buildType)
{
    if (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        return makeDeviceOrHostAddressKHR(vk, device, m_buffer->get(), 0);
    else
        return makeDeviceOrHostAddressKHR(m_buffer->getAllocation().getHostPtr());
}

SerialStorage::AccelerationStructureHeader *SerialStorage::getASHeader()
{
    return reinterpret_cast<AccelerationStructureHeader *>(getHostAddress().hostAddress);
}

bool SerialStorage::hasDeepFormat() const
{
    return (m_serialInfo.sizes().size() >= 2u);
}

de::SharedPtr<SerialStorage> SerialStorage::getBottomStorage(uint32_t index) const
{
    return m_bottoms[index];
}

VkDeviceOrHostAddressKHR SerialStorage::getHostAddress(VkDeviceSize offset)
{
    DE_ASSERT(offset < m_storageSize);
    return makeDeviceOrHostAddressKHR(static_cast<uint8_t *>(m_buffer->getAllocation().getHostPtr()) + offset);
}

VkDeviceOrHostAddressConstKHR SerialStorage::getHostAddressConst(VkDeviceSize offset)
{
    return makeDeviceOrHostAddressConstKHR(static_cast<uint8_t *>(m_buffer->getAllocation().getHostPtr()) + offset);
}

VkDeviceOrHostAddressConstKHR SerialStorage::getAddressConst(const DeviceInterface &vk, const VkDevice device,
                                                             const VkAccelerationStructureBuildTypeKHR buildType)
{
    if (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        return makeDeviceOrHostAddressConstKHR(vk, device, m_buffer->get(), 0);
    else
        return getHostAddressConst();
}

inline VkDeviceSize SerialStorage::getStorageSize() const
{
    return m_storageSize;
}

inline const SerialInfo &SerialStorage::getSerialInfo() const
{
    return m_serialInfo;
}

uint64_t SerialStorage::getDeserializedSize()
{
    uint64_t result         = 0;
    const uint8_t *startPtr = static_cast<uint8_t *>(m_buffer->getAllocation().getHostPtr());

    DE_ASSERT(sizeof(result) == DESERIALIZED_SIZE_SIZE);

    deMemcpy(&result, startPtr + DESERIALIZED_SIZE_OFFSET, sizeof(result));

    return result;
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
    : m_structureSize(0u)
    , m_updateScratchSize(0u)
    , m_buildScratchSize(0u)
{
}

void BottomLevelAccelerationStructure::setGeometryData(const std::vector<tcu::Vec3> &geometryData, const bool triangles,
                                                       const VkGeometryFlagsKHR geometryFlags)
{
    if (triangles)
        DE_ASSERT((geometryData.size() % 3) == 0);
    else
        DE_ASSERT((geometryData.size() % 2) == 0);

    setGeometryCount(1u);

    addGeometry(geometryData, triangles, geometryFlags);
}

void BottomLevelAccelerationStructure::setDefaultGeometryData(const VkShaderStageFlagBits testStage,
                                                              const VkGeometryFlagsKHR geometryFlags)
{
    bool trianglesData = false;
    float z            = 0.0f;
    std::vector<tcu::Vec3> geometryData;

    switch (testStage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        z             = -1.0f;
        trianglesData = true;
        break;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        z             = -1.0f;
        trianglesData = true;
        break;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        z             = -1.0f;
        trianglesData = true;
        break;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        z             = -9.9f;
        trianglesData = true;
        break;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        z             = -1.0f;
        trianglesData = false;
        break;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        z             = -1.0f;
        trianglesData = true;
        break;
    default:
        TCU_THROW(InternalError, "Unacceptable stage");
    }

    if (trianglesData)
    {
        geometryData.reserve(6);

        geometryData.push_back(tcu::Vec3(-1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(-1.0f, +1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(-1.0f, +1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, +1.0f, z));
    }
    else
    {
        geometryData.reserve(2);

        geometryData.push_back(tcu::Vec3(-1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, +1.0f, z));
    }

    setGeometryCount(1u);

    addGeometry(geometryData, trianglesData, geometryFlags);
}

void BottomLevelAccelerationStructure::setGeometryCount(const size_t geometryCount)
{
    m_geometriesData.clear();

    m_geometriesData.reserve(geometryCount);
}

void BottomLevelAccelerationStructure::addGeometry(de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry)
{
    m_geometriesData.push_back(raytracedGeometry);
}

void BottomLevelAccelerationStructure::addGeometry(
    const std::vector<tcu::Vec3> &geometryData, const bool triangles, const VkGeometryFlagsKHR geometryFlags,
    const VkAccelerationStructureTrianglesOpacityMicromapEXT *opacityGeometryMicromap)
{
    DE_ASSERT(geometryData.size() > 0);
    DE_ASSERT((triangles && geometryData.size() % 3 == 0) || (!triangles && geometryData.size() % 2 == 0));

    if (!triangles)
        for (size_t posNdx = 0; posNdx < geometryData.size() / 2; ++posNdx)
        {
            DE_ASSERT(geometryData[2 * posNdx].x() <= geometryData[2 * posNdx + 1].x());
            DE_ASSERT(geometryData[2 * posNdx].y() <= geometryData[2 * posNdx + 1].y());
            DE_ASSERT(geometryData[2 * posNdx].z() <= geometryData[2 * posNdx + 1].z());
        }

    de::SharedPtr<RaytracedGeometryBase> geometry =
        makeRaytracedGeometry(triangles ? VK_GEOMETRY_TYPE_TRIANGLES_KHR : VK_GEOMETRY_TYPE_AABBS_KHR,
                              VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
    for (auto it = begin(geometryData), eit = end(geometryData); it != eit; ++it)
        geometry->addVertex(*it);

    geometry->setGeometryFlags(geometryFlags);
    if (opacityGeometryMicromap)
        geometry->setOpacityMicromap(opacityGeometryMicromap);
    addGeometry(geometry);
}

VkAccelerationStructureBuildSizesInfoKHR BottomLevelAccelerationStructure::getStructureBuildSizes() const
{
    return {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                       //  const void* pNext;
        m_structureSize,                                               //  VkDeviceSize accelerationStructureSize;
        m_updateScratchSize,                                           //  VkDeviceSize updateScratchSize;
        m_buildScratchSize                                             //  VkDeviceSize buildScratchSize;
    };
};

VkDeviceSize getVertexBufferSize(const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    DE_ASSERT(geometriesData.size() != 0);
    VkDeviceSize bufferSizeBytes = 0;
    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
        bufferSizeBytes += deAlignSize(geometriesData[geometryNdx]->getVertexByteSize(), 8);
    return bufferSizeBytes;
}

BufferWithMemory *createVertexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                     const VkDeviceSize bufferSizeBytes)
{
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                    MemoryRequirement::DeviceAddress);
}

BufferWithMemory *createVertexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                     const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    return createVertexBuffer(vk, device, allocator, getVertexBufferSize(geometriesData));
}

void updateVertexBuffer(const DeviceInterface &vk, const VkDevice device,
                        const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData,
                        BufferWithMemory *vertexBuffer, VkDeviceSize geometriesOffset = 0)
{
    const Allocation &geometryAlloc = vertexBuffer->getAllocation();
    uint8_t *bufferStart            = static_cast<uint8_t *>(geometryAlloc.getHostPtr());
    VkDeviceSize bufferOffset       = geometriesOffset;

    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
    {
        const void *geometryPtr      = geometriesData[geometryNdx]->getVertexPointer();
        const size_t geometryPtrSize = geometriesData[geometryNdx]->getVertexByteSize();

        deMemcpy(&bufferStart[bufferOffset], geometryPtr, geometryPtrSize);

        bufferOffset += deAlignSize(geometryPtrSize, 8);
    }

    // Flush the whole allocation. We could flush only the interesting range, but we'd need to be sure both the offset and size
    // align to VkPhysicalDeviceLimits::nonCoherentAtomSize, which we are not considering. Also note most code uses Coherent memory
    // for the vertex and index buffers, so flushing is actually not needed.
    flushAlloc(vk, device, geometryAlloc);
}

VkDeviceSize getIndexBufferSize(const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    DE_ASSERT(!geometriesData.empty());

    VkDeviceSize bufferSizeBytes = 0;
    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
        if (geometriesData[geometryNdx]->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
            bufferSizeBytes += deAlignSize(geometriesData[geometryNdx]->getIndexByteSize(), 8);
    return bufferSizeBytes;
}

BufferWithMemory *createIndexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                    const VkDeviceSize bufferSizeBytes)
{
    DE_ASSERT(bufferSizeBytes);
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                    MemoryRequirement::DeviceAddress);
}

BufferWithMemory *createIndexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                    const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    const VkDeviceSize bufferSizeBytes = getIndexBufferSize(geometriesData);
    return bufferSizeBytes ? createIndexBuffer(vk, device, allocator, bufferSizeBytes) : nullptr;
}

void updateIndexBuffer(const DeviceInterface &vk, const VkDevice device,
                       const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData,
                       BufferWithMemory *indexBuffer, VkDeviceSize geometriesOffset)
{
    const Allocation &indexAlloc = indexBuffer->getAllocation();
    uint8_t *bufferStart         = static_cast<uint8_t *>(indexAlloc.getHostPtr());
    VkDeviceSize bufferOffset    = geometriesOffset;

    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
    {
        if (geometriesData[geometryNdx]->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
        {
            const void *indexPtr      = geometriesData[geometryNdx]->getIndexPointer();
            const size_t indexPtrSize = geometriesData[geometryNdx]->getIndexByteSize();

            deMemcpy(&bufferStart[bufferOffset], indexPtr, indexPtrSize);

            bufferOffset += deAlignSize(indexPtrSize, 8);
        }
    }

    // Flush the whole allocation. We could flush only the interesting range, but we'd need to be sure both the offset and size
    // align to VkPhysicalDeviceLimits::nonCoherentAtomSize, which we are not considering. Also note most code uses Coherent memory
    // for the vertex and index buffers, so flushing is actually not needed.
    flushAlloc(vk, device, indexAlloc);
}

class BottomLevelAccelerationStructureKHR : public BottomLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    BottomLevelAccelerationStructureKHR();
    BottomLevelAccelerationStructureKHR(const BottomLevelAccelerationStructureKHR &other) = delete;
    virtual ~BottomLevelAccelerationStructureKHR();

    void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType) override;
    VkAccelerationStructureBuildTypeKHR getBuildType() const override;
    void setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags) override;
    void setCreateGeneric(bool createGeneric) override;
    void setCreationBufferUnbounded(bool creationBufferUnbounded) override;
    void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags) override;
    void setBuildWithoutGeometries(bool buildWithoutGeometries) override;
    void setBuildWithoutPrimitives(bool buildWithoutPrimitives) override;
    void setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount) override;
    void setUseArrayOfPointers(const bool useArrayOfPointers) override;
    void setUseMaintenance5(const bool useMaintenance5) override;
    void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                    const uint32_t indirectBufferStride) override;
    VkBuildAccelerationStructureFlagsKHR getBuildFlags() const override;

    void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, VkDeviceSize structureSize,
                VkDeviceAddress deviceAddress = 0u, const void *pNext = nullptr,
                const MemoryRequirement &addMemoryRequirement = MemoryRequirement::Any,
                const VkBuffer creationBuffer = VK_NULL_HANDLE, const VkDeviceSize creationBufferSize = 0u) override;
    void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
               BottomLevelAccelerationStructure *srcAccelerationStructure = nullptr) override;
    void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                  BottomLevelAccelerationStructure *accelerationStructure, bool compactCopy) override;

    void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                   SerialStorage *storage) override;
    void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                     SerialStorage *storage) override;

    const VkAccelerationStructureKHR *getPtr(void) const override;
    void updateGeometry(size_t geometryIndex, de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry) override;

protected:
    VkAccelerationStructureBuildTypeKHR m_buildType;
    VkAccelerationStructureCreateFlagsKHR m_createFlags;
    bool m_createGeneric;
    bool m_creationBufferUnbounded;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags;
    bool m_buildWithoutGeometries;
    bool m_buildWithoutPrimitives;
    bool m_deferredOperation;
    uint32_t m_workerThreadCount;
    bool m_useArrayOfPointers;
    bool m_useMaintenance5;
    de::MovePtr<BufferWithMemory> m_accelerationStructureBuffer;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
    de::MovePtr<BufferWithMemory> m_indexBuffer;
    de::MovePtr<BufferWithMemory> m_deviceScratchBuffer;
    de::UniquePtr<std::vector<uint8_t>> m_hostScratchBuffer;
    Move<VkAccelerationStructureKHR> m_accelerationStructureKHR;
    VkBuffer m_indirectBuffer;
    VkDeviceSize m_indirectBufferOffset;
    uint32_t m_indirectBufferStride;

    void prepareGeometries(
        const DeviceInterface &vk, const VkDevice device,
        std::vector<VkAccelerationStructureGeometryKHR> &accelerationStructureGeometriesKHR,
        std::vector<VkAccelerationStructureGeometryKHR *> &accelerationStructureGeometriesKHRPointers,
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> &accelerationStructureBuildRangeInfoKHR,
        std::vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> &accelerationStructureGeometryMicromapsEXT,
        std::vector<uint32_t> &maxPrimitiveCounts, VkDeviceSize vertexBufferOffset = 0,
        VkDeviceSize indexBufferOffset = 0) const;

    virtual BufferWithMemory *getAccelerationStructureBuffer() const
    {
        return m_accelerationStructureBuffer.get();
    }
    virtual BufferWithMemory *getDeviceScratchBuffer() const
    {
        return m_deviceScratchBuffer.get();
    }
    virtual std::vector<uint8_t> *getHostScratchBuffer() const
    {
        return m_hostScratchBuffer.get();
    }
    virtual BufferWithMemory *getVertexBuffer() const
    {
        return m_vertexBuffer.get();
    }
    virtual BufferWithMemory *getIndexBuffer() const
    {
        return m_indexBuffer.get();
    }

    virtual VkDeviceSize getAccelerationStructureBufferOffset() const
    {
        return 0;
    }
    virtual VkDeviceSize getDeviceScratchBufferOffset() const
    {
        return 0;
    }
    virtual VkDeviceSize getVertexBufferOffset() const
    {
        return 0;
    }
    virtual VkDeviceSize getIndexBufferOffset() const
    {
        return 0;
    }
};

uint32_t BottomLevelAccelerationStructureKHR::getRequiredAllocationCount(void)
{
    /*
        de::MovePtr<BufferWithMemory>                            m_geometryBuffer; // but only when m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR
        de::MovePtr<Allocation>                                    m_accelerationStructureAlloc;
        de::MovePtr<BufferWithMemory>                            m_deviceScratchBuffer;
    */
    return 3u;
}

BottomLevelAccelerationStructureKHR::~BottomLevelAccelerationStructureKHR()
{
}

BottomLevelAccelerationStructureKHR::BottomLevelAccelerationStructureKHR()
    : BottomLevelAccelerationStructure()
    , m_buildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    , m_createFlags(0u)
    , m_createGeneric(false)
    , m_creationBufferUnbounded(false)
    , m_buildFlags(0u)
    , m_buildWithoutGeometries(false)
    , m_buildWithoutPrimitives(false)
    , m_deferredOperation(false)
    , m_workerThreadCount(0)
    , m_useArrayOfPointers(false)
    , m_useMaintenance5(false)
    , m_accelerationStructureBuffer(VK_NULL_HANDLE)
    , m_vertexBuffer(VK_NULL_HANDLE)
    , m_indexBuffer(VK_NULL_HANDLE)
    , m_deviceScratchBuffer(VK_NULL_HANDLE)
    , m_hostScratchBuffer(new std::vector<uint8_t>)
    , m_accelerationStructureKHR()
    , m_indirectBuffer(VK_NULL_HANDLE)
    , m_indirectBufferOffset(0)
    , m_indirectBufferStride(0)
{
}

void BottomLevelAccelerationStructureKHR::setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)
{
    m_buildType = buildType;
}

VkAccelerationStructureBuildTypeKHR BottomLevelAccelerationStructureKHR::getBuildType() const
{
    return m_buildType;
}

void BottomLevelAccelerationStructureKHR::setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags)
{
    m_createFlags = createFlags;
}

void BottomLevelAccelerationStructureKHR::setCreateGeneric(bool createGeneric)
{
    m_createGeneric = createGeneric;
}

void BottomLevelAccelerationStructureKHR::setCreationBufferUnbounded(bool creationBufferUnbounded)
{
    m_creationBufferUnbounded = creationBufferUnbounded;
}

void BottomLevelAccelerationStructureKHR::setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags)
{
    m_buildFlags = buildFlags;
}

void BottomLevelAccelerationStructureKHR::setBuildWithoutGeometries(bool buildWithoutGeometries)
{
    m_buildWithoutGeometries = buildWithoutGeometries;
}

void BottomLevelAccelerationStructureKHR::setBuildWithoutPrimitives(bool buildWithoutPrimitives)
{
    m_buildWithoutPrimitives = buildWithoutPrimitives;
}

void BottomLevelAccelerationStructureKHR::setDeferredOperation(const bool deferredOperation,
                                                               const uint32_t workerThreadCount)
{
    m_deferredOperation = deferredOperation;
    m_workerThreadCount = workerThreadCount;
}

void BottomLevelAccelerationStructureKHR::setUseArrayOfPointers(const bool useArrayOfPointers)
{
    m_useArrayOfPointers = useArrayOfPointers;
}

void BottomLevelAccelerationStructureKHR::setUseMaintenance5(const bool useMaintenance5)
{
    m_useMaintenance5 = useMaintenance5;
}

void BottomLevelAccelerationStructureKHR::setIndirectBuildParameters(const VkBuffer indirectBuffer,
                                                                     const VkDeviceSize indirectBufferOffset,
                                                                     const uint32_t indirectBufferStride)
{
    m_indirectBuffer       = indirectBuffer;
    m_indirectBufferOffset = indirectBufferOffset;
    m_indirectBufferStride = indirectBufferStride;
}

VkBuildAccelerationStructureFlagsKHR BottomLevelAccelerationStructureKHR::getBuildFlags() const
{
    return m_buildFlags;
}

void BottomLevelAccelerationStructureKHR::create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                                 VkDeviceSize structureSize, VkDeviceAddress deviceAddress,
                                                 const void *pNext, const MemoryRequirement &addMemoryRequirement,
                                                 const VkBuffer creationBuffer, const VkDeviceSize creationBufferSize)
{
    // AS may be built from geometries using vkCmdBuildAccelerationStructuresKHR / vkBuildAccelerationStructuresKHR
    // or may be copied/compacted/deserialized from other AS ( in this case AS does not need geometries, but it needs to know its size before creation ).
    DE_ASSERT(!m_geometriesData.empty() != !(structureSize == 0)); // logical xor

    if (structureSize == 0)
    {
        std::vector<VkAccelerationStructureGeometryKHR> accelerationStructureGeometriesKHR;
        std::vector<VkAccelerationStructureGeometryKHR *> accelerationStructureGeometriesKHRPointers;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> accelerationStructureBuildRangeInfoKHR;
        std::vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> accelerationStructureGeometryMicromapsEXT;
        std::vector<uint32_t> maxPrimitiveCounts;
        prepareGeometries(vk, device, accelerationStructureGeometriesKHR, accelerationStructureGeometriesKHRPointers,
                          accelerationStructureBuildRangeInfoKHR, accelerationStructureGeometryMicromapsEXT,
                          maxPrimitiveCounts);

        const VkAccelerationStructureGeometryKHR *accelerationStructureGeometriesKHRPointer =
            accelerationStructureGeometriesKHR.data();
        const VkAccelerationStructureGeometryKHR *const *accelerationStructureGeometry =
            accelerationStructureGeometriesKHRPointers.data();

        const uint32_t geometryCount =
            (m_buildWithoutGeometries ? 0u : static_cast<uint32_t>(accelerationStructureGeometriesKHR.size()));
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                  //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                   //  VkBuildAccelerationStructureFlagsKHR flags;
            VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, //  VkBuildAccelerationStructureModeKHR mode;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR srcAccelerationStructure;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR dstAccelerationStructure;
            geometryCount,                                  //  uint32_t geometryCount;
            m_useArrayOfPointers ?
                nullptr :
                accelerationStructureGeometriesKHRPointer, //  const VkAccelerationStructureGeometryKHR* pGeometries;
            m_useArrayOfPointers ? accelerationStructureGeometry :
                                   nullptr,     //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
            makeDeviceOrHostAddressKHR(nullptr) //  VkDeviceOrHostAddressKHR scratchData;
        };
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                       //  const void* pNext;
            0,                                                             //  VkDeviceSize accelerationStructureSize;
            0,                                                             //  VkDeviceSize updateScratchSize;
            0                                                              //  VkDeviceSize buildScratchSize;
        };

        vk.getAccelerationStructureBuildSizesKHR(device, m_buildType, &accelerationStructureBuildGeometryInfoKHR,
                                                 maxPrimitiveCounts.data(), &sizeInfo);

        m_structureSize     = sizeInfo.accelerationStructureSize;
        m_updateScratchSize = sizeInfo.updateScratchSize;
        m_buildScratchSize  = sizeInfo.buildScratchSize;
    }
    else
    {
        m_structureSize     = structureSize;
        m_updateScratchSize = 0u;
        m_buildScratchSize  = 0u;
    }

    const bool externalCreationBuffer = (creationBuffer != VK_NULL_HANDLE);

    if (externalCreationBuffer)
    {
        DE_UNREF(creationBufferSize); // For release builds.
        DE_ASSERT(creationBufferSize >= m_structureSize);
    }

    if (!externalCreationBuffer)
    {
        VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(m_structureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();

        if (m_useMaintenance5)
        {
            bufferUsageFlags2.usage = VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
            bufferCreateInfo.pNext = &bufferUsageFlags2;
            bufferCreateInfo.usage = 0;
        }

        const MemoryRequirement memoryRequirement = addMemoryRequirement | MemoryRequirement::HostVisible |
                                                    MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress;
        const bool bindMemOnCreation = (!m_creationBufferUnbounded);

        try
        {
            m_accelerationStructureBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                     (MemoryRequirement::Cached | memoryRequirement), bindMemOnCreation));
        }
        catch (const tcu::NotSupportedError &)
        {
            // retry without Cached flag
            m_accelerationStructureBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo, memoryRequirement, bindMemOnCreation));
        }
    }

    const auto createInfoBuffer = (externalCreationBuffer ? creationBuffer : getAccelerationStructureBuffer()->get());
    const auto createInfoOffset =
        (externalCreationBuffer ? static_cast<VkDeviceSize>(0) : getAccelerationStructureBufferOffset());
    {
        const VkAccelerationStructureTypeKHR structureType =
            (m_createGeneric ? VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR :
                               VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
        const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfoKHR{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, //  VkStructureType sType;
            pNext,                                                    //  const void* pNext;
            m_createFlags,    //  VkAccelerationStructureCreateFlagsKHR createFlags;
            createInfoBuffer, //  VkBuffer buffer;
            createInfoOffset, //  VkDeviceSize offset;
            m_structureSize,  //  VkDeviceSize size;
            structureType,    //  VkAccelerationStructureTypeKHR type;
            deviceAddress     //  VkDeviceAddress deviceAddress;
        };

        m_accelerationStructureKHR =
            createAccelerationStructureKHR(vk, device, &accelerationStructureCreateInfoKHR, nullptr);

        // Make sure buffer memory is always bound after creation.
        if (!externalCreationBuffer)
            m_accelerationStructureBuffer->bindMemory();
    }

    if (m_buildScratchSize > 0u)
    {
        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
                m_buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            m_deviceScratchBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, bufferCreateInfo,
                MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
        }
        else
        {
            m_hostScratchBuffer->resize(static_cast<size_t>(m_buildScratchSize));
        }
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR && !m_geometriesData.empty())
    {
        VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(getVertexBufferSize(m_geometriesData),
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();

        if (m_useMaintenance5)
        {
            bufferUsageFlags2.usage = vk::VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                      VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
            bufferCreateInfo.pNext = &bufferUsageFlags2;
            bufferCreateInfo.usage = 0;
        }

        const vk::MemoryRequirement memoryRequirement =
            MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress;
        m_vertexBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, bufferCreateInfo, memoryRequirement));

        bufferCreateInfo.size = getIndexBufferSize(m_geometriesData);
        if (bufferCreateInfo.size)
            m_indexBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo, memoryRequirement));
        else
            m_indexBuffer = de::MovePtr<BufferWithMemory>(nullptr);
    }
}

void BottomLevelAccelerationStructureKHR::build(const DeviceInterface &vk, const VkDevice device,
                                                const VkCommandBuffer cmdBuffer,
                                                BottomLevelAccelerationStructure *srcAccelerationStructure)
{
    DE_ASSERT(!m_geometriesData.empty());
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(m_buildScratchSize != 0);

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        updateVertexBuffer(vk, device, m_geometriesData, getVertexBuffer(), getVertexBufferOffset());
        if (getIndexBuffer() != VK_NULL_HANDLE)
            updateIndexBuffer(vk, device, m_geometriesData, getIndexBuffer(), getIndexBufferOffset());
    }

    {
        std::vector<VkAccelerationStructureGeometryKHR> accelerationStructureGeometriesKHR;
        std::vector<VkAccelerationStructureGeometryKHR *> accelerationStructureGeometriesKHRPointers;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> accelerationStructureBuildRangeInfoKHR;
        std::vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> accelerationStructureGeometryMicromapsEXT;
        std::vector<uint32_t> maxPrimitiveCounts;

        prepareGeometries(vk, device, accelerationStructureGeometriesKHR, accelerationStructureGeometriesKHRPointers,
                          accelerationStructureBuildRangeInfoKHR, accelerationStructureGeometryMicromapsEXT,
                          maxPrimitiveCounts, getVertexBufferOffset(), getIndexBufferOffset());

        const VkAccelerationStructureGeometryKHR *accelerationStructureGeometriesKHRPointer =
            accelerationStructureGeometriesKHR.data();
        const VkAccelerationStructureGeometryKHR *const *accelerationStructureGeometry =
            accelerationStructureGeometriesKHRPointers.data();
        VkDeviceOrHostAddressKHR scratchData =
            (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                makeDeviceOrHostAddressKHR(vk, device, getDeviceScratchBuffer()->get(),
                                           getDeviceScratchBufferOffset()) :
                makeDeviceOrHostAddressKHR(getHostScratchBuffer()->data());
        const uint32_t geometryCount =
            (m_buildWithoutGeometries ? 0u : static_cast<uint32_t>(accelerationStructureGeometriesKHR.size()));

        VkAccelerationStructureKHR srcStructure =
            (srcAccelerationStructure != nullptr) ? *(srcAccelerationStructure->getPtr()) : VK_NULL_HANDLE;
        VkBuildAccelerationStructureModeKHR mode = (srcAccelerationStructure != nullptr) ?
                                                       VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR :
                                                       VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                  //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                     //  VkBuildAccelerationStructureFlagsKHR flags;
            mode,                             //  VkBuildAccelerationStructureModeKHR mode;
            srcStructure,                     //  VkAccelerationStructureKHR srcAccelerationStructure;
            m_accelerationStructureKHR.get(), //  VkAccelerationStructureKHR dstAccelerationStructure;
            geometryCount,                    //  uint32_t geometryCount;
            m_useArrayOfPointers ?
                nullptr :
                accelerationStructureGeometriesKHRPointer, //  const VkAccelerationStructureGeometryKHR* pGeometries;
            m_useArrayOfPointers ? accelerationStructureGeometry :
                                   nullptr, //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
            scratchData                     //  VkDeviceOrHostAddressKHR scratchData;
        };

        VkAccelerationStructureBuildRangeInfoKHR *accelerationStructureBuildRangeInfoKHRPtr =
            accelerationStructureBuildRangeInfoKHR.data();

        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            if (m_indirectBuffer == VK_NULL_HANDLE)
                vk.cmdBuildAccelerationStructuresKHR(
                    cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                    (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr);
            else
            {
                VkDeviceAddress indirectDeviceAddress =
                    getBufferDeviceAddress(vk, device, m_indirectBuffer, m_indirectBufferOffset);
                uint32_t *pMaxPrimitiveCounts = maxPrimitiveCounts.data();
                vk.cmdBuildAccelerationStructuresIndirectKHR(cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                                                             &indirectDeviceAddress, &m_indirectBufferStride,
                                                             &pMaxPrimitiveCounts);
            }
        }
        else if (!m_deferredOperation)
        {
            VK_CHECK(vk.buildAccelerationStructuresKHR(
                device, VK_NULL_HANDLE, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr));
        }
        else
        {
            const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
            const auto deferredOperation    = deferredOperationPtr.get();

            VkResult result = vk.buildAccelerationStructuresKHR(
                device, deferredOperation, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr);

            DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                      result == VK_SUCCESS);

            finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                    result == VK_OPERATION_NOT_DEFERRED_KHR);
        }
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

void BottomLevelAccelerationStructureKHR::copyFrom(const DeviceInterface &vk, const VkDevice device,
                                                   const VkCommandBuffer cmdBuffer,
                                                   BottomLevelAccelerationStructure *accelerationStructure,
                                                   bool compactCopy)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(accelerationStructure != nullptr);

    VkCopyAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        *(accelerationStructure->getPtr()),                     // VkAccelerationStructureKHR src;
        *(getPtr()),                                            // VkAccelerationStructureKHR dst;
        compactCopy ? VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR :
                      VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        VkResult result = vk.copyAccelerationStructureKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

void BottomLevelAccelerationStructureKHR::serialize(const DeviceInterface &vk, const VkDevice device,
                                                    const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(storage != nullptr);

    const VkCopyAccelerationStructureToMemoryInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        *(getPtr()),                                                      // VkAccelerationStructureKHR src;
        storage->getAddress(vk, device, m_buildType),                     // VkDeviceOrHostAddressKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR                 // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureToMemoryKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureToMemoryKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        const VkResult result =
            vk.copyAccelerationStructureToMemoryKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }
}

void BottomLevelAccelerationStructureKHR::deserialize(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(storage != nullptr);

    const VkCopyMemoryToAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        storage->getAddressConst(vk, device, m_buildType),                // VkDeviceOrHostAddressConstKHR src;
        *(getPtr()),                                                      // VkAccelerationStructureKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR               // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyMemoryToAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyMemoryToAccelerationStructureKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        const VkResult result =
            vk.copyMemoryToAccelerationStructureKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

const VkAccelerationStructureKHR *BottomLevelAccelerationStructureKHR::getPtr(void) const
{
    return &m_accelerationStructureKHR.get();
}

void BottomLevelAccelerationStructureKHR::prepareGeometries(
    const DeviceInterface &vk, const VkDevice device,
    std::vector<VkAccelerationStructureGeometryKHR> &accelerationStructureGeometriesKHR,
    std::vector<VkAccelerationStructureGeometryKHR *> &accelerationStructureGeometriesKHRPointers,
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> &accelerationStructureBuildRangeInfoKHR,
    std::vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> &accelerationStructureGeometryMicromapsEXT,
    std::vector<uint32_t> &maxPrimitiveCounts, VkDeviceSize vertexBufferOffset, VkDeviceSize indexBufferOffset) const
{
    accelerationStructureGeometriesKHR.resize(m_geometriesData.size());
    accelerationStructureGeometriesKHRPointers.resize(m_geometriesData.size());
    accelerationStructureBuildRangeInfoKHR.resize(m_geometriesData.size());
    accelerationStructureGeometryMicromapsEXT.resize(m_geometriesData.size());
    maxPrimitiveCounts.resize(m_geometriesData.size());

    for (size_t geometryNdx = 0; geometryNdx < m_geometriesData.size(); ++geometryNdx)
    {
        const de::SharedPtr<RaytracedGeometryBase> &geometryData = m_geometriesData[geometryNdx];
        VkDeviceOrHostAddressConstKHR vertexData, indexData;
        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            if (getVertexBuffer() != nullptr)
            {
                vertexData = makeDeviceOrHostAddressConstKHR(vk, device, getVertexBuffer()->get(), vertexBufferOffset);
                if (m_indirectBuffer == VK_NULL_HANDLE)
                {
                    vertexBufferOffset += deAlignSize(geometryData->getVertexByteSize(), 8);
                }
            }
            else
                vertexData = makeDeviceOrHostAddressConstKHR(nullptr);

            if (getIndexBuffer() != nullptr && geometryData->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
            {
                indexData = makeDeviceOrHostAddressConstKHR(vk, device, getIndexBuffer()->get(), indexBufferOffset);
                indexBufferOffset += deAlignSize(geometryData->getIndexByteSize(), 8);
            }
            else
                indexData = makeDeviceOrHostAddressConstKHR(nullptr);
        }
        else
        {
            vertexData = makeDeviceOrHostAddressConstKHR(geometryData->getVertexPointer());
            if (geometryData->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
                indexData = makeDeviceOrHostAddressConstKHR(geometryData->getIndexPointer());
            else
                indexData = makeDeviceOrHostAddressConstKHR(nullptr);
        }

        VkAccelerationStructureGeometryTrianglesDataKHR accelerationStructureGeometryTrianglesDataKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, //  VkStructureType sType;
            nullptr,                                                              //  const void* pNext;
            geometryData->getVertexFormat(),                                      //  VkFormat vertexFormat;
            vertexData,                                            //  VkDeviceOrHostAddressConstKHR vertexData;
            geometryData->getVertexStride(),                       //  VkDeviceSize vertexStride;
            static_cast<uint32_t>(geometryData->getVertexCount()), //  uint32_t maxVertex;
            geometryData->getIndexType(),                          //  VkIndexType indexType;
            indexData,                                             //  VkDeviceOrHostAddressConstKHR indexData;
            makeDeviceOrHostAddressConstKHR(nullptr),              //  VkDeviceOrHostAddressConstKHR transformData;
        };

        if (geometryData->getHasOpacityMicromap())
            accelerationStructureGeometryTrianglesDataKHR.pNext = &geometryData->getOpacityMicromap();

        const VkAccelerationStructureGeometryAabbsDataKHR accelerationStructureGeometryAabbsDataKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            vertexData,                                                       //  VkDeviceOrHostAddressConstKHR data;
            geometryData->getAABBStride()                                     //  VkDeviceSize stride;
        };
        const VkAccelerationStructureGeometryDataKHR geometry =
            (geometryData->isTrianglesType()) ?
                makeVkAccelerationStructureGeometryDataKHR(accelerationStructureGeometryTrianglesDataKHR) :
                makeVkAccelerationStructureGeometryDataKHR(accelerationStructureGeometryAabbsDataKHR);
        const VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, //  VkStructureType sType;
            nullptr,                                               //  const void* pNext;
            geometryData->getGeometryType(),                       //  VkGeometryTypeKHR geometryType;
            geometry,                                              //  VkAccelerationStructureGeometryDataKHR geometry;
            geometryData->getGeometryFlags()                       //  VkGeometryFlagsKHR flags;
        };

        const uint32_t primitiveCount = (m_buildWithoutPrimitives ? 0u : geometryData->getPrimitiveCount());

        const VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfosKHR = {
            primitiveCount, //  uint32_t primitiveCount;
            0,              //  uint32_t primitiveOffset;
            0,              //  uint32_t firstVertex;
            0               //  uint32_t firstTransform;
        };

        accelerationStructureGeometriesKHR[geometryNdx]         = accelerationStructureGeometryKHR;
        accelerationStructureGeometriesKHRPointers[geometryNdx] = &accelerationStructureGeometriesKHR[geometryNdx];
        accelerationStructureBuildRangeInfoKHR[geometryNdx]     = accelerationStructureBuildRangeInfosKHR;
        maxPrimitiveCounts[geometryNdx]                         = geometryData->getPrimitiveCount();
    }
}

uint32_t BottomLevelAccelerationStructure::getRequiredAllocationCount(void)
{
    return BottomLevelAccelerationStructureKHR::getRequiredAllocationCount();
}

void BottomLevelAccelerationStructure::createAndBuild(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                      VkDeviceAddress deviceAddress)
{
    create(vk, device, allocator, 0u, deviceAddress);
    build(vk, device, cmdBuffer);
}

void BottomLevelAccelerationStructure::createAndCopyFrom(const DeviceInterface &vk, const VkDevice device,
                                                         const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                         BottomLevelAccelerationStructure *accelerationStructure,
                                                         VkDeviceSize compactCopySize, VkDeviceAddress deviceAddress)
{
    DE_ASSERT(accelerationStructure != NULL);
    VkDeviceSize copiedSize = compactCopySize > 0u ?
                                  compactCopySize :
                                  accelerationStructure->getStructureBuildSizes().accelerationStructureSize;
    DE_ASSERT(copiedSize != 0u);

    create(vk, device, allocator, copiedSize, deviceAddress);
    copyFrom(vk, device, cmdBuffer, accelerationStructure, compactCopySize > 0u);
}

void BottomLevelAccelerationStructure::createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device,
                                                                const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                                SerialStorage *storage, VkDeviceAddress deviceAddress)
{
    DE_ASSERT(storage != NULL);
    DE_ASSERT(storage->getStorageSize() >= SerialStorage::SERIAL_STORAGE_SIZE_MIN);
    create(vk, device, allocator, storage->getDeserializedSize(), deviceAddress);
    deserialize(vk, device, cmdBuffer, storage);
}

void BottomLevelAccelerationStructureKHR::updateGeometry(size_t geometryIndex,
                                                         de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry)
{
    DE_ASSERT(geometryIndex < m_geometriesData.size());
    m_geometriesData[geometryIndex] = raytracedGeometry;
}

de::MovePtr<BottomLevelAccelerationStructure> makeBottomLevelAccelerationStructure()
{
    return de::MovePtr<BottomLevelAccelerationStructure>(new BottomLevelAccelerationStructureKHR);
}

// Forward declaration
struct BottomLevelAccelerationStructurePoolImpl;

class BottomLevelAccelerationStructurePoolMember : public BottomLevelAccelerationStructureKHR
{
public:
    friend class BottomLevelAccelerationStructurePool;

    BottomLevelAccelerationStructurePoolMember(BottomLevelAccelerationStructurePoolImpl &pool);
    BottomLevelAccelerationStructurePoolMember(const BottomLevelAccelerationStructurePoolMember &) = delete;
    BottomLevelAccelerationStructurePoolMember(BottomLevelAccelerationStructurePoolMember &&)      = delete;
    virtual ~BottomLevelAccelerationStructurePoolMember()                                          = default;

    virtual void create(const DeviceInterface &, const VkDevice, Allocator &, VkDeviceSize, VkDeviceAddress,
                        const void *, const MemoryRequirement &, const VkBuffer, const VkDeviceSize) override
    {
        DE_ASSERT(0); // Silent this method
    }
    virtual auto computeBuildSize(const DeviceInterface &vk, const VkDevice device, const VkDeviceSize strSize) const
        //              accStrSize,updateScratch, buildScratch, vertexSize,   indexSize
        -> std::tuple<VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize>;

protected:
    struct Info;
    virtual void preCreateSetSizesAndOffsets(const Info &info, const VkDeviceSize accStrSize,
                                             const VkDeviceSize updateScratchSize, const VkDeviceSize buildScratchSize);
    virtual void createAccellerationStructure(const DeviceInterface &vk, const VkDevice device,
                                              VkDeviceAddress deviceAddress);

    virtual BufferWithMemory *getAccelerationStructureBuffer() const override;
    virtual BufferWithMemory *getDeviceScratchBuffer() const override;
    virtual std::vector<uint8_t> *getHostScratchBuffer() const override;
    virtual BufferWithMemory *getVertexBuffer() const override;
    virtual BufferWithMemory *getIndexBuffer() const override;

    virtual VkDeviceSize getAccelerationStructureBufferOffset() const override
    {
        return m_info.accStrOffset;
    }
    virtual VkDeviceSize getDeviceScratchBufferOffset() const override
    {
        return m_info.buildScratchBuffOffset;
    }
    virtual VkDeviceSize getVertexBufferOffset() const override
    {
        return m_info.vertBuffOffset;
    }
    virtual VkDeviceSize getIndexBufferOffset() const override
    {
        return m_info.indexBuffOffset;
    }

    BottomLevelAccelerationStructurePoolImpl &m_pool;

    struct Info
    {
        uint32_t accStrIndex;
        VkDeviceSize accStrOffset;
        uint32_t vertBuffIndex;
        VkDeviceSize vertBuffOffset;
        uint32_t indexBuffIndex;
        VkDeviceSize indexBuffOffset;
        uint32_t buildScratchBuffIndex;
        VkDeviceSize buildScratchBuffOffset;
    } m_info;
};

template <class X>
inline X negz(const X &)
{
    return (~static_cast<X>(0));
}
template <class X>
inline bool isnegz(const X &x)
{
    return x == negz(x);
}
template <class Y>
inline auto make_unsigned(const Y &y) -> typename std::make_unsigned<Y>::type
{
    return static_cast<typename std::make_unsigned<Y>::type>(y);
}

BottomLevelAccelerationStructurePoolMember::BottomLevelAccelerationStructurePoolMember(
    BottomLevelAccelerationStructurePoolImpl &pool)
    : m_pool(pool)
    , m_info{}
{
}

struct BottomLevelAccelerationStructurePoolImpl
{
    BottomLevelAccelerationStructurePoolImpl(BottomLevelAccelerationStructurePoolImpl &&)      = delete;
    BottomLevelAccelerationStructurePoolImpl(const BottomLevelAccelerationStructurePoolImpl &) = delete;
    BottomLevelAccelerationStructurePoolImpl(BottomLevelAccelerationStructurePool &pool);

    BottomLevelAccelerationStructurePool &m_pool;
    std::vector<de::SharedPtr<BufferWithMemory>> m_accellerationStructureBuffers;
    de::SharedPtr<BufferWithMemory> m_deviceScratchBuffer;
    de::UniquePtr<std::vector<uint8_t>> m_hostScratchBuffer;
    std::vector<de::SharedPtr<BufferWithMemory>> m_vertexBuffers;
    std::vector<de::SharedPtr<BufferWithMemory>> m_indexBuffers;
};
BottomLevelAccelerationStructurePoolImpl::BottomLevelAccelerationStructurePoolImpl(
    BottomLevelAccelerationStructurePool &pool)
    : m_pool(pool)
    , m_accellerationStructureBuffers()
    , m_deviceScratchBuffer()
    , m_hostScratchBuffer(new std::vector<uint8_t>)
    , m_vertexBuffers()
    , m_indexBuffers()
{
}
BufferWithMemory *BottomLevelAccelerationStructurePoolMember::getAccelerationStructureBuffer() const
{
    BufferWithMemory *result = nullptr;
    if (m_pool.m_accellerationStructureBuffers.size())
    {
        DE_ASSERT(!isnegz(m_info.accStrIndex));
        result = m_pool.m_accellerationStructureBuffers[m_info.accStrIndex].get();
    }
    return result;
}
BufferWithMemory *BottomLevelAccelerationStructurePoolMember::getDeviceScratchBuffer() const
{
    DE_ASSERT(m_info.buildScratchBuffIndex == 0);
    return m_pool.m_deviceScratchBuffer.get();
}
std::vector<uint8_t> *BottomLevelAccelerationStructurePoolMember::getHostScratchBuffer() const
{
    return this->m_buildScratchSize ? m_pool.m_hostScratchBuffer.get() : nullptr;
}

BufferWithMemory *BottomLevelAccelerationStructurePoolMember::getVertexBuffer() const
{
    BufferWithMemory *result = nullptr;
    if (m_pool.m_vertexBuffers.size())
    {
        DE_ASSERT(!isnegz(m_info.vertBuffIndex));
        result = m_pool.m_vertexBuffers[m_info.vertBuffIndex].get();
    }
    return result;
}
BufferWithMemory *BottomLevelAccelerationStructurePoolMember::getIndexBuffer() const
{
    BufferWithMemory *result = nullptr;
    if (m_pool.m_indexBuffers.size())
    {
        DE_ASSERT(!isnegz(m_info.indexBuffIndex));
        result = m_pool.m_indexBuffers[m_info.indexBuffIndex].get();
    }
    return result;
}

struct BottomLevelAccelerationStructurePool::Impl : BottomLevelAccelerationStructurePoolImpl
{
    friend class BottomLevelAccelerationStructurePool;
    friend class BottomLevelAccelerationStructurePoolMember;

    Impl(BottomLevelAccelerationStructurePool &pool) : BottomLevelAccelerationStructurePoolImpl(pool)
    {
    }
};

BottomLevelAccelerationStructurePool::BottomLevelAccelerationStructurePool()
    : m_batchStructCount(4)
    , m_batchGeomCount(0)
    , m_infos()
    , m_structs()
    , m_createOnce(false)
    , m_tryCachedMemory(true)
    , m_structsBuffSize(0)
    , m_updatesScratchSize(0)
    , m_buildsScratchSize(0)
    , m_verticesSize(0)
    , m_indicesSize(0)
    , m_impl(new Impl(*this))
{
}

BottomLevelAccelerationStructurePool::~BottomLevelAccelerationStructurePool()
{
    delete m_impl;
}

void BottomLevelAccelerationStructurePool::batchStructCount(const uint32_t &value)
{
    DE_ASSERT(value >= 1);
    m_batchStructCount = value;
}

auto BottomLevelAccelerationStructurePool::add(VkDeviceSize structureSize, VkDeviceAddress deviceAddress)
    -> BottomLevelAccelerationStructurePool::BlasPtr
{
    // Prevent a programmer from calling this method after batchCreate(...) method has been called.
    if (m_createOnce)
        DE_ASSERT(0);

    auto blas = new BottomLevelAccelerationStructurePoolMember(*m_impl);
    m_infos.push_back({structureSize, deviceAddress});
    m_structs.emplace_back(blas);
    return m_structs.back();
}

void adjustBatchCount(const DeviceInterface &vkd, const VkDevice device,
                      const std::vector<BottomLevelAccelerationStructurePool::BlasPtr> &structs,
                      const std::vector<BottomLevelAccelerationStructurePool::BlasInfo> &infos,
                      const VkDeviceSize maxBufferSize, uint32_t (&result)[4])
{
    tcu::Vector<VkDeviceSize, 4> sizes(0);
    tcu::Vector<VkDeviceSize, 4> sums(0);
    tcu::Vector<uint32_t, 4> tmps(0);
    tcu::Vector<uint32_t, 4> batches(0);

    VkDeviceSize updateScratchSize = 0;
    static_cast<void>(updateScratchSize); // not used yet, disabled for future implementation

    auto updateIf = [&](uint32_t c)
    {
        if (sums[c] + sizes[c] <= maxBufferSize)
        {
            sums[c] += sizes[c];
            tmps[c] += 1;

            batches[c] = std::max(tmps[c], batches[c]);
        }
        else
        {
            sums[c] = 0;
            tmps[c] = 0;
        }
    };

    const uint32_t maxIter = static_cast<uint32_t>(structs.size());
    for (uint32_t i = 0; i < maxIter; ++i)
    {
        auto &str = *dynamic_cast<BottomLevelAccelerationStructurePoolMember *>(structs[i].get());
        std::tie(sizes[0], updateScratchSize, sizes[1], sizes[2], sizes[3]) =
            str.computeBuildSize(vkd, device, infos[i].structureSize);

        updateIf(0);
        updateIf(1);
        updateIf(2);
        updateIf(3);
    }

    result[0] = std::max(batches[0], 1u);
    result[1] = std::max(batches[1], 1u);
    result[2] = std::max(batches[2], 1u);
    result[3] = std::max(batches[3], 1u);
}

size_t BottomLevelAccelerationStructurePool::getAllocationCount() const
{
    return m_impl->m_accellerationStructureBuffers.size() + m_impl->m_vertexBuffers.size() +
           m_impl->m_indexBuffers.size() + 1 /* for scratch buffer */;
}

size_t BottomLevelAccelerationStructurePool::getAllocationCount(const DeviceInterface &vk, const VkDevice device,
                                                                const VkDeviceSize maxBufferSize) const
{
    DE_ASSERT(m_structs.size() != 0);

    std::map<uint32_t, VkDeviceSize> accStrSizes;
    std::map<uint32_t, VkDeviceSize> vertBuffSizes;
    std::map<uint32_t, VkDeviceSize> indexBuffSizes;
    std::map<uint32_t, VkDeviceSize> scratchBuffSizes;

    const uint32_t allStructsCount = structCount();

    uint32_t batchStructCount  = m_batchStructCount;
    uint32_t batchScratchCount = m_batchStructCount;
    uint32_t batchVertexCount  = m_batchGeomCount ? m_batchGeomCount : m_batchStructCount;
    uint32_t batchIndexCount   = batchVertexCount;

    if (!isnegz(maxBufferSize))
    {
        uint32_t batches[4];
        adjustBatchCount(vk, device, m_structs, m_infos, maxBufferSize, batches);
        batchStructCount  = batches[0];
        batchScratchCount = batches[1];
        batchVertexCount  = batches[2];
        batchIndexCount   = batches[3];
    }

    uint32_t iStr     = 0;
    uint32_t iScratch = 0;
    uint32_t iVertex  = 0;
    uint32_t iIndex   = 0;

    VkDeviceSize strSize           = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize buildScratchSize  = 0;
    VkDeviceSize vertexSize        = 0;
    VkDeviceSize indexSize         = 0;

    for (; iStr < allStructsCount; ++iStr)
    {
        auto &str = *dynamic_cast<BottomLevelAccelerationStructurePoolMember *>(m_structs[iStr].get());
        std::tie(strSize, updateScratchSize, buildScratchSize, vertexSize, indexSize) =
            str.computeBuildSize(vk, device, m_infos[iStr].structureSize);

        {
            const VkDeviceSize alignedStrSize = deAlign64(strSize, 256);
            const uint32_t accStrIndex        = (iStr / batchStructCount);
            accStrSizes[accStrIndex] += alignedStrSize;
        }

        if (buildScratchSize != 0)
        {
            const VkDeviceSize alignedBuilsScratchSize = deAlign64(buildScratchSize, 256);
            const uint32_t scratchBuffIndex            = (iScratch / batchScratchCount);
            scratchBuffSizes[scratchBuffIndex] += alignedBuilsScratchSize;
            iScratch += 1;
        }

        if (vertexSize != 0)
        {
            const VkDeviceSize alignedVertBuffSize = deAlign64(vertexSize, 8);
            const uint32_t vertBuffIndex           = (iVertex / batchVertexCount);
            vertBuffSizes[vertBuffIndex] += alignedVertBuffSize;
            iVertex += 1;
        }

        if (indexSize != 0)
        {
            const VkDeviceSize alignedIndexBuffSize = deAlign64(indexSize, 8);
            const uint32_t indexBuffIndex           = (iIndex / batchIndexCount);
            indexBuffSizes[indexBuffIndex] += alignedIndexBuffSize;
            iIndex += 1;
        }
    }

    return accStrSizes.size() + vertBuffSizes.size() + indexBuffSizes.size() + scratchBuffSizes.size();
}

tcu::Vector<VkDeviceSize, 4> BottomLevelAccelerationStructurePool::getAllocationSizes(const DeviceInterface &vk,
                                                                                      const VkDevice device) const
{
    if (m_structsBuffSize)
    {
        return tcu::Vector<VkDeviceSize, 4>(m_structsBuffSize, m_buildsScratchSize, m_verticesSize, m_indicesSize);
    }

    VkDeviceSize strSize           = 0;
    VkDeviceSize updateScratchSize = 0;
    static_cast<void>(updateScratchSize); // not used yet, disabled for future implementation
    VkDeviceSize buildScratchSize     = 0;
    VkDeviceSize vertexSize           = 0;
    VkDeviceSize indexSize            = 0;
    VkDeviceSize sumStrSize           = 0;
    VkDeviceSize sumUpdateScratchSize = 0;
    static_cast<void>(sumUpdateScratchSize); // not used yet, disabled for future implementation
    VkDeviceSize sumBuildScratchSize = 0;
    VkDeviceSize sumVertexSize       = 0;
    VkDeviceSize sumIndexSize        = 0;
    for (size_t i = 0; i < structCount(); ++i)
    {
        auto &str = *dynamic_cast<BottomLevelAccelerationStructurePoolMember *>(m_structs[i].get());
        std::tie(strSize, updateScratchSize, buildScratchSize, vertexSize, indexSize) =
            str.computeBuildSize(vk, device, m_infos[i].structureSize);
        sumStrSize += deAlign64(strSize, 256);
        //sumUpdateScratchSize    += deAlign64(updateScratchSize, 256);    not used yet, disabled for future implementation
        sumBuildScratchSize += deAlign64(buildScratchSize, 256);
        sumVertexSize += deAlign64(vertexSize, 8);
        sumIndexSize += deAlign64(indexSize, 8);
    }
    return tcu::Vector<VkDeviceSize, 4>(sumStrSize, sumBuildScratchSize, sumVertexSize, sumIndexSize);
}

void BottomLevelAccelerationStructurePool::batchCreate(const DeviceInterface &vkd, const VkDevice device,
                                                       Allocator &allocator)
{
    batchCreateAdjust(vkd, device, allocator, negz<VkDeviceSize>(0));
}

void BottomLevelAccelerationStructurePool::batchCreateAdjust(const DeviceInterface &vkd, const VkDevice device,
                                                             Allocator &allocator, const VkDeviceSize maxBufferSize)
{
    // Prevent a programmer from calling this method more than once.
    if (m_createOnce)
        DE_ASSERT(0);

    m_createOnce = true;
    DE_ASSERT(m_structs.size() != 0);

    auto createAccellerationStructureBuffer = [&](VkDeviceSize bufferSize) ->
        typename std::add_pointer<BufferWithMemory>::type
    {
        BufferWithMemory *res = nullptr;
        const VkBufferCreateInfo bci =
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

        if (m_tryCachedMemory)
            try
            {
                res = new BufferWithMemory(vkd, device, allocator, bci,
                                           MemoryRequirement::Cached | MemoryRequirement::HostVisible |
                                               MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress);
            }
            catch (const tcu::NotSupportedError &)
            {
                res = nullptr;
            }

        return (nullptr != res) ? res :
                                  (new BufferWithMemory(vkd, device, allocator, bci,
                                                        MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                                            MemoryRequirement::DeviceAddress));
    };

    auto createDeviceScratchBuffer = [&](VkDeviceSize bufferSize) -> de::SharedPtr<BufferWithMemory>
    {
        const VkBufferCreateInfo bci = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        BufferWithMemory *p          = new BufferWithMemory(vkd, device, allocator, bci,
                                                            MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                                                MemoryRequirement::DeviceAddress);
        return de::SharedPtr<BufferWithMemory>(p);
    };

    std::map<uint32_t, VkDeviceSize> accStrSizes;
    std::map<uint32_t, VkDeviceSize> vertBuffSizes;
    std::map<uint32_t, VkDeviceSize> indexBuffSizes;

    const uint32_t allStructsCount = structCount();
    uint32_t iterKey               = 0;

    uint32_t batchStructCount = m_batchStructCount;
    uint32_t batchVertexCount = m_batchGeomCount ? m_batchGeomCount : m_batchStructCount;
    uint32_t batchIndexCount  = batchVertexCount;

    if (!isnegz(maxBufferSize))
    {
        uint32_t batches[4];
        adjustBatchCount(vkd, device, m_structs, m_infos, maxBufferSize, batches);
        batchStructCount = batches[0];
        // batches[1]: batchScratchCount
        batchVertexCount = batches[2];
        batchIndexCount  = batches[3];
    }

    uint32_t iStr    = 0;
    uint32_t iVertex = 0;
    uint32_t iIndex  = 0;

    VkDeviceSize strSize             = 0;
    VkDeviceSize updateScratchSize   = 0;
    VkDeviceSize buildScratchSize    = 0;
    VkDeviceSize maxBuildScratchSize = 0;
    VkDeviceSize vertexSize          = 0;
    VkDeviceSize indexSize           = 0;

    VkDeviceSize strOffset    = 0;
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset  = 0;

    uint32_t hostStructCount   = 0;
    uint32_t deviceStructCount = 0;

    for (; iStr < allStructsCount; ++iStr)
    {
        BottomLevelAccelerationStructurePoolMember::Info info{};
        auto &str = *dynamic_cast<BottomLevelAccelerationStructurePoolMember *>(m_structs[iStr].get());
        std::tie(strSize, updateScratchSize, buildScratchSize, vertexSize, indexSize) =
            str.computeBuildSize(vkd, device, m_infos[iStr].structureSize);

        ++(str.getBuildType() == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR ? hostStructCount : deviceStructCount);

        {
            const VkDeviceSize alignedStrSize = deAlign64(strSize, 256);
            const uint32_t accStrIndex        = (iStr / batchStructCount);
            if (iStr != 0 && (iStr % batchStructCount) == 0)
            {
                strOffset = 0;
            }

            info.accStrIndex  = accStrIndex;
            info.accStrOffset = strOffset;
            accStrSizes[accStrIndex] += alignedStrSize;
            strOffset += alignedStrSize;
            m_structsBuffSize += alignedStrSize;
        }

        if (buildScratchSize != 0)
        {
            maxBuildScratchSize = std::max(maxBuildScratchSize, make_unsigned(deAlign64(buildScratchSize, 256u)));

            info.buildScratchBuffIndex  = 0;
            info.buildScratchBuffOffset = 0;
        }

        if (vertexSize != 0)
        {
            const VkDeviceSize alignedVertBuffSize = deAlign64(vertexSize, 8);
            const uint32_t vertBuffIndex           = (iVertex / batchVertexCount);
            if (iVertex != 0 && (iVertex % batchVertexCount) == 0)
            {
                vertexOffset = 0;
            }

            info.vertBuffIndex  = vertBuffIndex;
            info.vertBuffOffset = vertexOffset;
            vertBuffSizes[vertBuffIndex] += alignedVertBuffSize;
            vertexOffset += alignedVertBuffSize;
            m_verticesSize += alignedVertBuffSize;
            iVertex += 1;
        }

        if (indexSize != 0)
        {
            const VkDeviceSize alignedIndexBuffSize = deAlign64(indexSize, 8);
            const uint32_t indexBuffIndex           = (iIndex / batchIndexCount);
            if (iIndex != 0 && (iIndex % batchIndexCount) == 0)
            {
                indexOffset = 0;
            }

            info.indexBuffIndex  = indexBuffIndex;
            info.indexBuffOffset = indexOffset;
            indexBuffSizes[indexBuffIndex] += alignedIndexBuffSize;
            indexOffset += alignedIndexBuffSize;
            m_indicesSize += alignedIndexBuffSize;
            iIndex += 1;
        }

        str.preCreateSetSizesAndOffsets(info, strSize, updateScratchSize, buildScratchSize);
    }

    for (iterKey = 0; iterKey < static_cast<uint32_t>(accStrSizes.size()); ++iterKey)
    {
        m_impl->m_accellerationStructureBuffers.emplace_back(
            createAccellerationStructureBuffer(accStrSizes.at(iterKey)));
    }
    for (iterKey = 0; iterKey < static_cast<uint32_t>(vertBuffSizes.size()); ++iterKey)
    {
        m_impl->m_vertexBuffers.emplace_back(createVertexBuffer(vkd, device, allocator, vertBuffSizes.at(iterKey)));
    }
    for (iterKey = 0; iterKey < static_cast<uint32_t>(indexBuffSizes.size()); ++iterKey)
    {
        m_impl->m_indexBuffers.emplace_back(createIndexBuffer(vkd, device, allocator, indexBuffSizes.at(iterKey)));
    }

    if (maxBuildScratchSize)
    {
        if (hostStructCount)
            m_impl->m_hostScratchBuffer->resize(static_cast<size_t>(maxBuildScratchSize));
        if (deviceStructCount)
            m_impl->m_deviceScratchBuffer = createDeviceScratchBuffer(maxBuildScratchSize);

        m_buildsScratchSize = maxBuildScratchSize;
    }

    for (iterKey = 0; iterKey < allStructsCount; ++iterKey)
    {
        auto &str = *dynamic_cast<BottomLevelAccelerationStructurePoolMember *>(m_structs[iterKey].get());
        str.createAccellerationStructure(vkd, device, m_infos[iterKey].deviceAddress);
    }
}

void BottomLevelAccelerationStructurePool::batchBuild(const DeviceInterface &vk, const VkDevice device,
                                                      VkCommandBuffer cmdBuffer)
{
    for (const auto &str : m_structs)
    {
        str->build(vk, device, cmdBuffer);
    }
}

void BottomLevelAccelerationStructurePool::batchBuild(const DeviceInterface &vk, const VkDevice device,
                                                      VkCommandPool cmdPool, VkQueue queue, qpWatchDog *watchDog)
{
    const uint32_t limit = 10000u;
    const uint32_t count = structCount();
    std::vector<BlasPtr> buildingOnDevice;

    auto buildOnDevice = [&]() -> void
    {
        Move<VkCommandBuffer> cmd = allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *cmd, 0u);
        for (const auto &str : buildingOnDevice)
            str->build(vk, device, *cmd);
        endCommandBuffer(vk, *cmd);

        submitCommandsAndWait(vk, device, queue, *cmd);
        vk.resetCommandPool(device, cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    };

    buildingOnDevice.reserve(limit);
    for (uint32_t i = 0; i < count; ++i)
    {
        auto str = m_structs[i];

        if (str->getBuildType() == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR)
            str->build(vk, device, nullptr);
        else
            buildingOnDevice.emplace_back(str);

        if (buildingOnDevice.size() == limit || (count - 1) == i)
        {
            buildOnDevice();
            buildingOnDevice.clear();
        }

        if ((i % WATCHDOG_INTERVAL) == 0 && watchDog)
            qpWatchDog_touch(watchDog);
    }
}

auto BottomLevelAccelerationStructurePoolMember::computeBuildSize(const DeviceInterface &vk, const VkDevice device,
                                                                  const VkDeviceSize strSize) const
    //              accStrSize,updateScratch,buildScratch, vertexSize, indexSize
    -> std::tuple<VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize>
{
    DE_ASSERT(!m_geometriesData.empty() != !(strSize == 0)); // logical xor

    std::tuple<VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize, VkDeviceSize> result(deAlign64(strSize, 256), 0,
                                                                                            0, 0, 0);

    if (!m_geometriesData.empty())
    {
        std::vector<VkAccelerationStructureGeometryKHR> accelerationStructureGeometriesKHR;
        std::vector<VkAccelerationStructureGeometryKHR *> accelerationStructureGeometriesKHRPointers;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> accelerationStructureBuildRangeInfoKHR;
        std::vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> accelerationStructureGeometryMicromapsEXT;
        std::vector<uint32_t> maxPrimitiveCounts;
        prepareGeometries(vk, device, accelerationStructureGeometriesKHR, accelerationStructureGeometriesKHRPointers,
                          accelerationStructureBuildRangeInfoKHR, accelerationStructureGeometryMicromapsEXT,
                          maxPrimitiveCounts);

        const VkAccelerationStructureGeometryKHR *accelerationStructureGeometriesKHRPointer =
            accelerationStructureGeometriesKHR.data();
        const VkAccelerationStructureGeometryKHR *const *accelerationStructureGeometry =
            accelerationStructureGeometriesKHRPointers.data();

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                  //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                   //  VkBuildAccelerationStructureFlagsKHR flags;
            VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, //  VkBuildAccelerationStructureModeKHR mode;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR srcAccelerationStructure;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR dstAccelerationStructure;
            static_cast<uint32_t>(accelerationStructureGeometriesKHR.size()), //  uint32_t geometryCount;
            m_useArrayOfPointers ?
                nullptr :
                accelerationStructureGeometriesKHRPointer, //  const VkAccelerationStructureGeometryKHR* pGeometries;
            m_useArrayOfPointers ? accelerationStructureGeometry :
                                   nullptr,     //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
            makeDeviceOrHostAddressKHR(nullptr) //  VkDeviceOrHostAddressKHR scratchData;
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                       //  const void* pNext;
            0,                                                             //  VkDeviceSize accelerationStructureSize;
            0,                                                             //  VkDeviceSize updateScratchSize;
            0                                                              //  VkDeviceSize buildScratchSize;
        };

        vk.getAccelerationStructureBuildSizesKHR(device, m_buildType, &accelerationStructureBuildGeometryInfoKHR,
                                                 maxPrimitiveCounts.data(), &sizeInfo);

        std::get<0>(result) = sizeInfo.accelerationStructureSize;
        std::get<1>(result) = sizeInfo.updateScratchSize;
        std::get<2>(result) = sizeInfo.buildScratchSize;
        std::get<3>(result) = getVertexBufferSize(m_geometriesData);
        std::get<4>(result) = getIndexBufferSize(m_geometriesData);
    }

    return result;
}

void BottomLevelAccelerationStructurePoolMember::preCreateSetSizesAndOffsets(const Info &info,
                                                                             const VkDeviceSize accStrSize,
                                                                             const VkDeviceSize updateScratchSize,
                                                                             const VkDeviceSize buildScratchSize)
{
    m_info              = info;
    m_structureSize     = accStrSize;
    m_updateScratchSize = updateScratchSize;
    m_buildScratchSize  = buildScratchSize;
}

void BottomLevelAccelerationStructurePoolMember::createAccellerationStructure(const DeviceInterface &vk,
                                                                              const VkDevice device,
                                                                              VkDeviceAddress deviceAddress)
{
    const VkAccelerationStructureTypeKHR structureType =
        (m_createGeneric ? VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR :
                           VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
    const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfoKHR{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                  //  const void* pNext;
        m_createFlags,                                            //  VkAccelerationStructureCreateFlagsKHR createFlags;
        getAccelerationStructureBuffer()->get(),                  //  VkBuffer buffer;
        getAccelerationStructureBufferOffset(),                   //  VkDeviceSize offset;
        m_structureSize,                                          //  VkDeviceSize size;
        structureType,                                            //  VkAccelerationStructureTypeKHR type;
        deviceAddress                                             //  VkDeviceAddress deviceAddress;
    };

    m_accelerationStructureKHR =
        createAccelerationStructureKHR(vk, device, &accelerationStructureCreateInfoKHR, nullptr);
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
    : m_structureSize(0u)
    , m_updateScratchSize(0u)
    , m_buildScratchSize(0u)
{
}

void TopLevelAccelerationStructure::setInstanceCount(const size_t instanceCount)
{
    m_bottomLevelInstances.reserve(instanceCount);
    m_instanceData.reserve(instanceCount);
}

void TopLevelAccelerationStructure::addInstance(de::SharedPtr<BottomLevelAccelerationStructure> bottomLevelStructure,
                                                const VkTransformMatrixKHR &matrix, uint32_t instanceCustomIndex,
                                                uint32_t mask, uint32_t instanceShaderBindingTableRecordOffset,
                                                VkGeometryInstanceFlagsKHR flags)
{
    m_bottomLevelInstances.push_back(bottomLevelStructure);
    m_instanceData.push_back(
        InstanceData(matrix, instanceCustomIndex, mask, instanceShaderBindingTableRecordOffset, flags));
}

VkAccelerationStructureBuildSizesInfoKHR TopLevelAccelerationStructure::getStructureBuildSizes() const
{
    return {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                       //  const void* pNext;
        m_structureSize,                                               //  VkDeviceSize accelerationStructureSize;
        m_updateScratchSize,                                           //  VkDeviceSize updateScratchSize;
        m_buildScratchSize                                             //  VkDeviceSize buildScratchSize;
    };
}

void TopLevelAccelerationStructure::createAndBuild(const DeviceInterface &vk, const VkDevice device,
                                                   const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                   VkDeviceAddress deviceAddress)
{
    create(vk, device, allocator, 0u, deviceAddress);
    build(vk, device, cmdBuffer);
}

void TopLevelAccelerationStructure::createAndCopyFrom(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                      TopLevelAccelerationStructure *accelerationStructure,
                                                      VkDeviceSize compactCopySize, VkDeviceAddress deviceAddress)
{
    DE_ASSERT(accelerationStructure != NULL);
    VkDeviceSize copiedSize = compactCopySize > 0u ?
                                  compactCopySize :
                                  accelerationStructure->getStructureBuildSizes().accelerationStructureSize;
    DE_ASSERT(copiedSize != 0u);

    create(vk, device, allocator, copiedSize, deviceAddress);
    copyFrom(vk, device, cmdBuffer, accelerationStructure, compactCopySize > 0u);
}

void TopLevelAccelerationStructure::createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device,
                                                             const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                             SerialStorage *storage, VkDeviceAddress deviceAddress)
{
    DE_ASSERT(storage != NULL);
    DE_ASSERT(storage->getStorageSize() >= SerialStorage::SERIAL_STORAGE_SIZE_MIN);
    create(vk, device, allocator, storage->getDeserializedSize(), deviceAddress);
    if (storage->hasDeepFormat())
        createAndDeserializeBottoms(vk, device, cmdBuffer, allocator, storage);
    deserialize(vk, device, cmdBuffer, storage);
}

BufferWithMemory *createInstanceBuffer(
    const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> bottomLevelInstances,
    std::vector<InstanceData> instanceData, const bool tryCachedMemory)
{
    DE_ASSERT(bottomLevelInstances.size() != 0);
    DE_ASSERT(bottomLevelInstances.size() == instanceData.size());
    DE_UNREF(instanceData);

    BufferWithMemory *result           = nullptr;
    const VkDeviceSize bufferSizeBytes = bottomLevelInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if (tryCachedMemory)
        try
        {
            result = new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                          MemoryRequirement::Cached | MemoryRequirement::HostVisible |
                                              MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress);
        }
        catch (const tcu::NotSupportedError &)
        {
            result = nullptr;
        }
    return result ? result :
                    new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                         MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                             MemoryRequirement::DeviceAddress);
}

void updateSingleInstance(const DeviceInterface &vk, const VkDevice device,
                          const BottomLevelAccelerationStructure &bottomLevelAccelerationStructure,
                          const InstanceData &instanceData, uint8_t *bufferLocation,
                          VkAccelerationStructureBuildTypeKHR buildType, bool inactiveInstances)
{
    const VkAccelerationStructureKHR accelerationStructureKHR = *bottomLevelAccelerationStructure.getPtr();

    // This part needs to be fixed once a new version of the VkAccelerationStructureInstanceKHR will be added to vkStructTypes.inl
    VkDeviceAddress accelerationStructureAddress;
    if (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, // VkStructureType sType;
            nullptr,                                                          // const void* pNext;
            accelerationStructureKHR // VkAccelerationStructureKHR accelerationStructure;
        };
        accelerationStructureAddress = vk.getAccelerationStructureDeviceAddressKHR(device, &asDeviceAddressInfo);
    }

    uint64_t structureReference;
    if (inactiveInstances)
    {
        // Instances will be marked inactive by making their references VK_NULL_HANDLE or having address zero.
        structureReference = 0ull;
    }
    else
    {
        structureReference = (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                                 uint64_t(accelerationStructureAddress) :
                                 uint64_t(accelerationStructureKHR.getInternal());
    }

    VkAccelerationStructureInstanceKHR accelerationStructureInstanceKHR = makeVkAccelerationStructureInstanceKHR(
        instanceData.matrix,                                 //  VkTransformMatrixKHR transform;
        instanceData.instanceCustomIndex,                    //  uint32_t instanceCustomIndex:24;
        instanceData.mask,                                   //  uint32_t mask:8;
        instanceData.instanceShaderBindingTableRecordOffset, //  uint32_t instanceShaderBindingTableRecordOffset:24;
        instanceData.flags,                                  //  VkGeometryInstanceFlagsKHR flags:8;
        structureReference                                   //  uint64_t accelerationStructureReference;
    );

    deMemcpy(bufferLocation, &accelerationStructureInstanceKHR, sizeof(VkAccelerationStructureInstanceKHR));
}

void updateInstanceBuffer(const DeviceInterface &vk, const VkDevice device,
                          const std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> &bottomLevelInstances,
                          const std::vector<InstanceData> &instanceData, const BufferWithMemory *instanceBuffer,
                          VkAccelerationStructureBuildTypeKHR buildType, bool inactiveInstances)
{
    DE_ASSERT(bottomLevelInstances.size() != 0);
    DE_ASSERT(bottomLevelInstances.size() == instanceData.size());

    auto &instancesAlloc      = instanceBuffer->getAllocation();
    auto bufferStart          = reinterpret_cast<uint8_t *>(instancesAlloc.getHostPtr());
    VkDeviceSize bufferOffset = 0ull;

    for (size_t instanceNdx = 0; instanceNdx < bottomLevelInstances.size(); ++instanceNdx)
    {
        const auto &blas = *bottomLevelInstances[instanceNdx];
        updateSingleInstance(vk, device, blas, instanceData[instanceNdx], bufferStart + bufferOffset, buildType,
                             inactiveInstances);
        bufferOffset += sizeof(VkAccelerationStructureInstanceKHR);
    }

    flushMappedMemoryRange(vk, device, instancesAlloc.getMemory(), instancesAlloc.getOffset(), VK_WHOLE_SIZE);
}

class TopLevelAccelerationStructureKHR : public TopLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    TopLevelAccelerationStructureKHR();
    TopLevelAccelerationStructureKHR(const TopLevelAccelerationStructureKHR &other) = delete;
    virtual ~TopLevelAccelerationStructureKHR();

    void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType) override;
    void setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags) override;
    void setCreateGeneric(bool createGeneric) override;
    void setCreationBufferUnbounded(bool creationBufferUnbounded) override;
    void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags) override;
    void setBuildWithoutPrimitives(bool buildWithoutPrimitives) override;
    void setInactiveInstances(bool inactiveInstances) override;
    void setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount) override;
    void setUseArrayOfPointers(const bool useArrayOfPointers) override;
    void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                    const uint32_t indirectBufferStride) override;
    void setUsePPGeometries(const bool usePPGeometries) override;
    void setTryCachedMemory(const bool tryCachedMemory) override;
    VkBuildAccelerationStructureFlagsKHR getBuildFlags() const override;

    void getCreationSizes(const DeviceInterface &vk, const VkDevice device, const VkDeviceSize structureSize,
                          CreationSizes &sizes) override;
    void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, VkDeviceSize structureSize,
                VkDeviceAddress deviceAddress = 0u, const void *pNext = nullptr,
                const MemoryRequirement &addMemoryRequirement = MemoryRequirement::Any,
                const VkBuffer creationBuffer = VK_NULL_HANDLE, const VkDeviceSize creationBufferSize = 0u) override;
    void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
               TopLevelAccelerationStructure *srcAccelerationStructure = nullptr) override;
    void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                  TopLevelAccelerationStructure *accelerationStructure, bool compactCopy) override;
    void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                   SerialStorage *storage) override;
    void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                     SerialStorage *storage) override;

    std::vector<VkDeviceSize> getSerializingSizes(const DeviceInterface &vk, const VkDevice device, const VkQueue queue,
                                                  const uint32_t queueFamilyIndex) override;

    std::vector<uint64_t> getSerializingAddresses(const DeviceInterface &vk, const VkDevice device) const override;

    const VkAccelerationStructureKHR *getPtr(void) const override;

    void updateInstanceMatrix(const DeviceInterface &vk, const VkDevice device, size_t instanceIndex,
                              const VkTransformMatrixKHR &matrix) override;

protected:
    VkAccelerationStructureBuildTypeKHR m_buildType;
    VkAccelerationStructureCreateFlagsKHR m_createFlags;
    bool m_createGeneric;
    bool m_creationBufferUnbounded;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags;
    bool m_buildWithoutPrimitives;
    bool m_inactiveInstances;
    bool m_deferredOperation;
    uint32_t m_workerThreadCount;
    bool m_useArrayOfPointers;
    de::MovePtr<BufferWithMemory> m_accelerationStructureBuffer;
    de::MovePtr<BufferWithMemory> m_instanceBuffer;
    de::MovePtr<BufferWithMemory> m_instanceAddressBuffer;
    de::MovePtr<BufferWithMemory> m_deviceScratchBuffer;
    std::vector<uint8_t> m_hostScratchBuffer;
    Move<VkAccelerationStructureKHR> m_accelerationStructureKHR;
    VkBuffer m_indirectBuffer;
    VkDeviceSize m_indirectBufferOffset;
    uint32_t m_indirectBufferStride;
    bool m_usePPGeometries;
    bool m_tryCachedMemory;

    void prepareInstances(const DeviceInterface &vk, const VkDevice device,
                          VkAccelerationStructureGeometryKHR &accelerationStructureGeometryKHR,
                          std::vector<uint32_t> &maxPrimitiveCounts);

    void serializeBottoms(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                          SerialStorage *storage, VkDeferredOperationKHR deferredOperation);

    void createAndDeserializeBottoms(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                     Allocator &allocator, SerialStorage *storage) override;
};

uint32_t TopLevelAccelerationStructureKHR::getRequiredAllocationCount(void)
{
    /*
        de::MovePtr<BufferWithMemory>                            m_instanceBuffer;
        de::MovePtr<Allocation>                                    m_accelerationStructureAlloc;
        de::MovePtr<BufferWithMemory>                            m_deviceScratchBuffer;
    */
    return 3u;
}

TopLevelAccelerationStructureKHR::TopLevelAccelerationStructureKHR()
    : TopLevelAccelerationStructure()
    , m_buildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    , m_createFlags(0u)
    , m_createGeneric(false)
    , m_creationBufferUnbounded(false)
    , m_buildFlags(0u)
    , m_buildWithoutPrimitives(false)
    , m_inactiveInstances(false)
    , m_deferredOperation(false)
    , m_workerThreadCount(0)
    , m_useArrayOfPointers(false)
    , m_accelerationStructureBuffer(nullptr)
    , m_instanceBuffer(nullptr)
    , m_instanceAddressBuffer(nullptr)
    , m_deviceScratchBuffer(nullptr)
    , m_accelerationStructureKHR()
    , m_indirectBuffer(VK_NULL_HANDLE)
    , m_indirectBufferOffset(0)
    , m_indirectBufferStride(0)
    , m_usePPGeometries(false)
    , m_tryCachedMemory(true)
{
}

TopLevelAccelerationStructureKHR::~TopLevelAccelerationStructureKHR()
{
}

void TopLevelAccelerationStructureKHR::setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)
{
    m_buildType = buildType;
}

void TopLevelAccelerationStructureKHR::setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags)
{
    m_createFlags = createFlags;
}

void TopLevelAccelerationStructureKHR::setCreateGeneric(bool createGeneric)
{
    m_createGeneric = createGeneric;
}

void TopLevelAccelerationStructureKHR::setCreationBufferUnbounded(bool creationBufferUnbounded)
{
    m_creationBufferUnbounded = creationBufferUnbounded;
}

void TopLevelAccelerationStructureKHR::setInactiveInstances(bool inactiveInstances)
{
    m_inactiveInstances = inactiveInstances;
}

void TopLevelAccelerationStructureKHR::setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags)
{
    m_buildFlags = buildFlags;
}

void TopLevelAccelerationStructureKHR::setBuildWithoutPrimitives(bool buildWithoutPrimitives)
{
    m_buildWithoutPrimitives = buildWithoutPrimitives;
}

void TopLevelAccelerationStructureKHR::setDeferredOperation(const bool deferredOperation,
                                                            const uint32_t workerThreadCount)
{
    m_deferredOperation = deferredOperation;
    m_workerThreadCount = workerThreadCount;
}

void TopLevelAccelerationStructureKHR::setUseArrayOfPointers(const bool useArrayOfPointers)
{
    m_useArrayOfPointers = useArrayOfPointers;
}

void TopLevelAccelerationStructureKHR::setUsePPGeometries(const bool usePPGeometries)
{
    m_usePPGeometries = usePPGeometries;
}

void TopLevelAccelerationStructureKHR::setTryCachedMemory(const bool tryCachedMemory)
{
    m_tryCachedMemory = tryCachedMemory;
}

void TopLevelAccelerationStructureKHR::setIndirectBuildParameters(const VkBuffer indirectBuffer,
                                                                  const VkDeviceSize indirectBufferOffset,
                                                                  const uint32_t indirectBufferStride)
{
    m_indirectBuffer       = indirectBuffer;
    m_indirectBufferOffset = indirectBufferOffset;
    m_indirectBufferStride = indirectBufferStride;
}

VkBuildAccelerationStructureFlagsKHR TopLevelAccelerationStructureKHR::getBuildFlags() const
{
    return m_buildFlags;
}

VkDeviceSize TopLevelAccelerationStructure::CreationSizes::sum() const
{
    return structure + updateScratch + buildScratch + instancePointers + instancesBuffer;
}

void TopLevelAccelerationStructureKHR::getCreationSizes(const DeviceInterface &vk, const VkDevice device,
                                                        const VkDeviceSize structureSize, CreationSizes &sizes)
{
    // AS may be built from geometries using vkCmdBuildAccelerationStructureKHR / vkBuildAccelerationStructureKHR
    // or may be copied/compacted/deserialized from other AS ( in this case AS does not need geometries, but it needs to know its size before creation ).
    DE_ASSERT(!m_bottomLevelInstances.empty() != !(structureSize == 0)); // logical xor

    if (structureSize == 0)
    {
        VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR;
        const auto accelerationStructureGeometryKHRPtr = &accelerationStructureGeometryKHR;
        std::vector<uint32_t> maxPrimitiveCounts;
        prepareInstances(vk, device, accelerationStructureGeometryKHR, maxPrimitiveCounts);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,                     //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                   //  VkBuildAccelerationStructureFlagsKHR flags;
            VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, //  VkBuildAccelerationStructureModeKHR mode;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR srcAccelerationStructure;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR dstAccelerationStructure;
            1u,                                             //  uint32_t geometryCount;
            (m_usePPGeometries ?
                 nullptr :
                 &accelerationStructureGeometryKHR), //  const VkAccelerationStructureGeometryKHR* pGeometries;
            (m_usePPGeometries ? &accelerationStructureGeometryKHRPtr :
                                 nullptr),      //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
            makeDeviceOrHostAddressKHR(nullptr) //  VkDeviceOrHostAddressKHR scratchData;
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                       //  const void* pNext;
            0,                                                             //  VkDeviceSize accelerationStructureSize;
            0,                                                             //  VkDeviceSize updateScratchSize;
            0                                                              //  VkDeviceSize buildScratchSize;
        };

        vk.getAccelerationStructureBuildSizesKHR(device, m_buildType, &accelerationStructureBuildGeometryInfoKHR,
                                                 maxPrimitiveCounts.data(), &sizeInfo);

        sizes.structure     = sizeInfo.accelerationStructureSize;
        sizes.updateScratch = sizeInfo.updateScratchSize;
        sizes.buildScratch  = sizeInfo.buildScratchSize;
    }
    else
    {
        sizes.structure     = structureSize;
        sizes.updateScratch = 0u;
        sizes.buildScratch  = 0u;
    }

    sizes.instancePointers = 0u;
    if (m_useArrayOfPointers)
    {
        const size_t pointerSize = (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                                       sizeof(VkDeviceOrHostAddressConstKHR::deviceAddress) :
                                       sizeof(VkDeviceOrHostAddressConstKHR::hostAddress);
        sizes.instancePointers   = static_cast<VkDeviceSize>(m_bottomLevelInstances.size() * pointerSize);
    }

    sizes.instancesBuffer = m_bottomLevelInstances.empty() ?
                                0u :
                                m_bottomLevelInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
}

void TopLevelAccelerationStructureKHR::create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                              VkDeviceSize structureSize, VkDeviceAddress deviceAddress,
                                              const void *pNext, const MemoryRequirement &addMemoryRequirement,
                                              const VkBuffer creationBuffer, const VkDeviceSize creationBufferSize)
{
    // AS may be built from geometries using vkCmdBuildAccelerationStructureKHR / vkBuildAccelerationStructureKHR
    // or may be copied/compacted/deserialized from other AS ( in this case AS does not need geometries, but it needs to know its size before creation ).
    DE_ASSERT(!m_bottomLevelInstances.empty() != !(structureSize == 0)); // logical xor

    if (structureSize == 0)
    {
        VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR;
        const auto accelerationStructureGeometryKHRPtr = &accelerationStructureGeometryKHR;
        std::vector<uint32_t> maxPrimitiveCounts;
        prepareInstances(vk, device, accelerationStructureGeometryKHR, maxPrimitiveCounts);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,                     //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                   //  VkBuildAccelerationStructureFlagsKHR flags;
            VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, //  VkBuildAccelerationStructureModeKHR mode;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR srcAccelerationStructure;
            VK_NULL_HANDLE,                                 //  VkAccelerationStructureKHR dstAccelerationStructure;
            1u,                                             //  uint32_t geometryCount;
            (m_usePPGeometries ?
                 nullptr :
                 &accelerationStructureGeometryKHR), //  const VkAccelerationStructureGeometryKHR* pGeometries;
            (m_usePPGeometries ? &accelerationStructureGeometryKHRPtr :
                                 nullptr),      //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
            makeDeviceOrHostAddressKHR(nullptr) //  VkDeviceOrHostAddressKHR scratchData;
        };

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, //  VkStructureType sType;
            nullptr,                                                       //  const void* pNext;
            0,                                                             //  VkDeviceSize accelerationStructureSize;
            0,                                                             //  VkDeviceSize updateScratchSize;
            0                                                              //  VkDeviceSize buildScratchSize;
        };

        vk.getAccelerationStructureBuildSizesKHR(device, m_buildType, &accelerationStructureBuildGeometryInfoKHR,
                                                 maxPrimitiveCounts.data(), &sizeInfo);

        m_structureSize     = sizeInfo.accelerationStructureSize;
        m_updateScratchSize = sizeInfo.updateScratchSize;
        m_buildScratchSize  = sizeInfo.buildScratchSize;
    }
    else
    {
        m_structureSize     = structureSize;
        m_updateScratchSize = 0u;
        m_buildScratchSize  = 0u;
    }

    const bool externalCreationBuffer = (creationBuffer != VK_NULL_HANDLE);

    if (externalCreationBuffer)
    {
        DE_UNREF(creationBufferSize); // For release builds.
        DE_ASSERT(creationBufferSize >= m_structureSize);
    }

    if (!externalCreationBuffer)
    {
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(m_structureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        const MemoryRequirement memoryRequirement = addMemoryRequirement | MemoryRequirement::HostVisible |
                                                    MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress;
        const bool bindMemOnCreation = (!m_creationBufferUnbounded);

        try
        {
            m_accelerationStructureBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                     (MemoryRequirement::Cached | memoryRequirement), bindMemOnCreation));
        }
        catch (const tcu::NotSupportedError &)
        {
            // retry without Cached flag
            m_accelerationStructureBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo, memoryRequirement, bindMemOnCreation));
        }
    }

    const auto createInfoBuffer = (externalCreationBuffer ? creationBuffer : m_accelerationStructureBuffer->get());
    {
        const VkAccelerationStructureTypeKHR structureType =
            (m_createGeneric ? VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR :
                               VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);
        const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, //  VkStructureType sType;
            pNext,                                                    //  const void* pNext;
            m_createFlags,    //  VkAccelerationStructureCreateFlagsKHR createFlags;
            createInfoBuffer, //  VkBuffer buffer;
            0u,               //  VkDeviceSize offset;
            m_structureSize,  //  VkDeviceSize size;
            structureType,    //  VkAccelerationStructureTypeKHR type;
            deviceAddress     //  VkDeviceAddress deviceAddress;
        };

        m_accelerationStructureKHR =
            createAccelerationStructureKHR(vk, device, &accelerationStructureCreateInfoKHR, nullptr);

        // Make sure buffer memory is always bound after creation.
        if (!externalCreationBuffer)
            m_accelerationStructureBuffer->bindMemory();
    }

    if (m_buildScratchSize > 0u)
    {
        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
                m_buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            m_deviceScratchBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, bufferCreateInfo,
                MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
        }
        else
        {
            m_hostScratchBuffer.resize(static_cast<size_t>(m_buildScratchSize));
        }
    }

    if (m_useArrayOfPointers)
    {
        const size_t pointerSize = (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                                       sizeof(VkDeviceOrHostAddressConstKHR::deviceAddress) :
                                       sizeof(VkDeviceOrHostAddressConstKHR::hostAddress);
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(static_cast<VkDeviceSize>(m_bottomLevelInstances.size() * pointerSize),
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_instanceAddressBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, bufferCreateInfo,
            MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    }

    if (!m_bottomLevelInstances.empty())
        m_instanceBuffer = de::MovePtr<BufferWithMemory>(
            createInstanceBuffer(vk, device, allocator, m_bottomLevelInstances, m_instanceData, m_tryCachedMemory));
}

void TopLevelAccelerationStructureKHR::updateInstanceMatrix(const DeviceInterface &vk, const VkDevice device,
                                                            size_t instanceIndex, const VkTransformMatrixKHR &matrix)
{
    DE_ASSERT(instanceIndex < m_bottomLevelInstances.size());
    DE_ASSERT(instanceIndex < m_instanceData.size());

    const auto &blas          = *m_bottomLevelInstances[instanceIndex];
    auto &instanceData        = m_instanceData[instanceIndex];
    auto &instancesAlloc      = m_instanceBuffer->getAllocation();
    auto bufferStart          = reinterpret_cast<uint8_t *>(instancesAlloc.getHostPtr());
    VkDeviceSize bufferOffset = sizeof(VkAccelerationStructureInstanceKHR) * instanceIndex;

    instanceData.matrix = matrix;
    updateSingleInstance(vk, device, blas, instanceData, bufferStart + bufferOffset, m_buildType, m_inactiveInstances);
    flushMappedMemoryRange(vk, device, instancesAlloc.getMemory(), instancesAlloc.getOffset(), VK_WHOLE_SIZE);
}

void TopLevelAccelerationStructureKHR::build(const DeviceInterface &vk, const VkDevice device,
                                             const VkCommandBuffer cmdBuffer,
                                             TopLevelAccelerationStructure *srcAccelerationStructure)
{
    DE_ASSERT(!m_bottomLevelInstances.empty());
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(m_buildScratchSize != 0);

    updateInstanceBuffer(vk, device, m_bottomLevelInstances, m_instanceData, m_instanceBuffer.get(), m_buildType,
                         m_inactiveInstances);

    VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR;
    const auto accelerationStructureGeometryKHRPtr = &accelerationStructureGeometryKHR;
    std::vector<uint32_t> maxPrimitiveCounts;
    prepareInstances(vk, device, accelerationStructureGeometryKHR, maxPrimitiveCounts);

    VkDeviceOrHostAddressKHR scratchData = (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                                               makeDeviceOrHostAddressKHR(vk, device, m_deviceScratchBuffer->get(), 0) :
                                               makeDeviceOrHostAddressKHR(m_hostScratchBuffer.data());

    VkAccelerationStructureKHR srcStructure =
        (srcAccelerationStructure != nullptr) ? *(srcAccelerationStructure->getPtr()) : VK_NULL_HANDLE;
    VkBuildAccelerationStructureModeKHR mode = (srcAccelerationStructure != nullptr) ?
                                                   VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR :
                                                   VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                          //  const void* pNext;
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,                     //  VkAccelerationStructureTypeKHR type;
        m_buildFlags,                     //  VkBuildAccelerationStructureFlagsKHR flags;
        mode,                             //  VkBuildAccelerationStructureModeKHR mode;
        srcStructure,                     //  VkAccelerationStructureKHR srcAccelerationStructure;
        m_accelerationStructureKHR.get(), //  VkAccelerationStructureKHR dstAccelerationStructure;
        1u,                               //  uint32_t geometryCount;
        (m_usePPGeometries ?
             nullptr :
             &accelerationStructureGeometryKHR), //  const VkAccelerationStructureGeometryKHR* pGeometries;
        (m_usePPGeometries ? &accelerationStructureGeometryKHRPtr :
                             nullptr), //  const VkAccelerationStructureGeometryKHR* const* ppGeometries;
        scratchData                    //  VkDeviceOrHostAddressKHR scratchData;
    };

    const uint32_t primitiveCount =
        (m_buildWithoutPrimitives ? 0u : static_cast<uint32_t>(m_bottomLevelInstances.size()));

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfoKHR = {
        primitiveCount, //  uint32_t primitiveCount;
        0,              //  uint32_t primitiveOffset;
        0,              //  uint32_t firstVertex;
        0               //  uint32_t transformOffset;
    };
    VkAccelerationStructureBuildRangeInfoKHR *accelerationStructureBuildRangeInfoKHRPtr =
        &accelerationStructureBuildRangeInfoKHR;

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        if (m_indirectBuffer == VK_NULL_HANDLE)
            vk.cmdBuildAccelerationStructuresKHR(
                cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr);
        else
        {
            VkDeviceAddress indirectDeviceAddress =
                getBufferDeviceAddress(vk, device, m_indirectBuffer, m_indirectBufferOffset);
            uint32_t *pMaxPrimitiveCounts = maxPrimitiveCounts.data();
            vk.cmdBuildAccelerationStructuresIndirectKHR(cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                                                         &indirectDeviceAddress, &m_indirectBufferStride,
                                                         &pMaxPrimitiveCounts);
        }
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.buildAccelerationStructuresKHR(
            device, VK_NULL_HANDLE, 1u, &accelerationStructureBuildGeometryInfoKHR,
            (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        VkResult result = vk.buildAccelerationStructuresKHR(
            device, deferredOperation, 1u, &accelerationStructureBuildGeometryInfoKHR,
            (const VkAccelerationStructureBuildRangeInfoKHR **)&accelerationStructureBuildRangeInfoKHRPtr);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);

        accelerationStructureBuildGeometryInfoKHR.pNext = nullptr;
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

void TopLevelAccelerationStructureKHR::copyFrom(const DeviceInterface &vk, const VkDevice device,
                                                const VkCommandBuffer cmdBuffer,
                                                TopLevelAccelerationStructure *accelerationStructure, bool compactCopy)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(accelerationStructure != nullptr);

    VkCopyAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        *(accelerationStructure->getPtr()),                     // VkAccelerationStructureKHR src;
        *(getPtr()),                                            // VkAccelerationStructureKHR dst;
        compactCopy ? VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR :
                      VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        VkResult result = vk.copyAccelerationStructureKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

void TopLevelAccelerationStructureKHR::serialize(const DeviceInterface &vk, const VkDevice device,
                                                 const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(storage != nullptr);

    const VkCopyAccelerationStructureToMemoryInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        *(getPtr()),                                                      // VkAccelerationStructureKHR src;
        storage->getAddress(vk, device, m_buildType),                     // VkDeviceOrHostAddressKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR                 // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureToMemoryKHR(cmdBuffer, &copyAccelerationStructureInfo);
        if (storage->hasDeepFormat())
            serializeBottoms(vk, device, cmdBuffer, storage, VK_NULL_HANDLE);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureToMemoryKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
        if (storage->hasDeepFormat())
            serializeBottoms(vk, device, cmdBuffer, storage, VK_NULL_HANDLE);
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        const VkResult result =
            vk.copyAccelerationStructureToMemoryKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);
        if (storage->hasDeepFormat())
            serializeBottoms(vk, device, cmdBuffer, storage, deferredOperation);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }
}

void TopLevelAccelerationStructureKHR::deserialize(const DeviceInterface &vk, const VkDevice device,
                                                   const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    DE_ASSERT(m_accelerationStructureKHR.get() != VK_NULL_HANDLE);
    DE_ASSERT(storage != nullptr);

    const VkCopyMemoryToAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        storage->getAddressConst(vk, device, m_buildType),                // VkDeviceOrHostAddressConstKHR src;
        *(getPtr()),                                                      // VkAccelerationStructureKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR               // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyMemoryToAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyMemoryToAccelerationStructureKHR(device, VK_NULL_HANDLE, &copyAccelerationStructureInfo));
    }
    else
    {
        const auto deferredOperationPtr = createDeferredOperationKHR(vk, device);
        const auto deferredOperation    = deferredOperationPtr.get();

        const VkResult result =
            vk.copyMemoryToAccelerationStructureKHR(device, deferredOperation, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);

        finishDeferredOperation(vk, device, deferredOperation, m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        const VkAccessFlags accessMasks =
            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        const VkMemoryBarrier memBarrier = makeMemoryBarrier(accessMasks, accessMasks);

        cmdPipelineMemoryBarrier(vk, cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &memBarrier);
    }
}

void TopLevelAccelerationStructureKHR::serializeBottoms(const DeviceInterface &vk, const VkDevice device,
                                                        const VkCommandBuffer cmdBuffer, SerialStorage *storage,
                                                        VkDeferredOperationKHR deferredOperation)
{
    DE_UNREF(deferredOperation);
    DE_ASSERT(storage->hasDeepFormat());

    const std::vector<uint64_t> &addresses = storage->getSerialInfo().addresses();
    const std::size_t cbottoms             = m_bottomLevelInstances.size();

    uint32_t storageIndex = 0;
    std::vector<uint64_t> matches;

    for (std::size_t i = 0; i < cbottoms; ++i)
    {
        const uint64_t &lookAddr = addresses[i + 1];
        auto end                 = matches.end();
        auto match = std::find_if(matches.begin(), end, [&](const uint64_t &item) { return item == lookAddr; });
        if (match == end)
        {
            matches.emplace_back(lookAddr);
            m_bottomLevelInstances[i].get()->serialize(vk, device, cmdBuffer,
                                                       storage->getBottomStorage(storageIndex).get());
            storageIndex += 1;
        }
    }
}

void TopLevelAccelerationStructureKHR::createAndDeserializeBottoms(const DeviceInterface &vk, const VkDevice device,
                                                                   const VkCommandBuffer cmdBuffer,
                                                                   Allocator &allocator, SerialStorage *storage)
{
    DE_ASSERT(storage->hasDeepFormat());
    DE_ASSERT(m_bottomLevelInstances.size() == 0);

    const std::vector<uint64_t> &addresses = storage->getSerialInfo().addresses();
    const std::size_t cbottoms             = addresses.size() - 1;
    uint32_t storageIndex                  = 0;
    std::vector<std::pair<uint64_t, std::size_t>> matches;

    for (std::size_t i = 0; i < cbottoms; ++i)
    {
        const uint64_t &lookAddr = addresses[i + 1];
        auto end                 = matches.end();
        auto match               = std::find_if(matches.begin(), end,
                                                [&](const std::pair<uint64_t, std::size_t> &item) { return item.first == lookAddr; });
        if (match != end)
        {
            m_bottomLevelInstances.emplace_back(m_bottomLevelInstances[match->second]);
        }
        else
        {
            de::MovePtr<BottomLevelAccelerationStructure> blas = makeBottomLevelAccelerationStructure();
            blas->createAndDeserializeFrom(vk, device, cmdBuffer, allocator,
                                           storage->getBottomStorage(storageIndex).get());
            m_bottomLevelInstances.emplace_back(de::SharedPtr<BottomLevelAccelerationStructure>(blas.release()));
            matches.emplace_back(lookAddr, i);
            storageIndex += 1;
        }
    }

    std::vector<uint64_t> newAddresses = getSerializingAddresses(vk, device);
    DE_ASSERT(addresses.size() == newAddresses.size());

    SerialStorage::AccelerationStructureHeader *header = storage->getASHeader();
    DE_ASSERT(cbottoms == header->handleCount);

    // finally update bottom-level AS addresses before top-level AS deserialization
    for (std::size_t i = 0; i < cbottoms; ++i)
    {
        header->handleArray[i] = newAddresses[i + 1];
    }
}

std::vector<VkDeviceSize> TopLevelAccelerationStructureKHR::getSerializingSizes(const DeviceInterface &vk,
                                                                                const VkDevice device,
                                                                                const VkQueue queue,
                                                                                const uint32_t queueFamilyIndex)
{
    const uint32_t queryCount(uint32_t(m_bottomLevelInstances.size()) + 1);
    std::vector<VkAccelerationStructureKHR> handles(queryCount);
    std::vector<VkDeviceSize> sizes(queryCount);

    handles[0] = m_accelerationStructureKHR.get();

    for (uint32_t h = 1; h < queryCount; ++h)
        handles[h] = *m_bottomLevelInstances[h - 1].get()->getPtr();

    if (VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR == m_buildType)
        queryAccelerationStructureSize(vk, device, nullptr, handles, m_buildType, VK_NULL_HANDLE,
                                       VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, sizes);
    else
    {
        const Move<VkCommandPool> cmdPool = createCommandPool(vk, device, 0, queueFamilyIndex);
        const Move<VkCommandBuffer> cmdBuffer =
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const Move<VkQueryPool> queryPool =
            makeQueryPool(vk, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, queryCount);

        beginCommandBuffer(vk, *cmdBuffer);
        queryAccelerationStructureSize(vk, device, *cmdBuffer, handles, m_buildType, *queryPool,
                                       VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, sizes);
        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

        VK_CHECK(vk.getQueryPoolResults(device, *queryPool, 0u, queryCount, queryCount * sizeof(VkDeviceSize),
                                        sizes.data(), sizeof(VkDeviceSize),
                                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    }

    return sizes;
}

std::vector<uint64_t> TopLevelAccelerationStructureKHR::getSerializingAddresses(const DeviceInterface &vk,
                                                                                const VkDevice device) const
{
    std::vector<uint64_t> result(m_bottomLevelInstances.size() + 1);

    VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        VK_NULL_HANDLE, // VkAccelerationStructureKHR accelerationStructure;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        asDeviceAddressInfo.accelerationStructure = m_accelerationStructureKHR.get();
        result[0] = vk.getAccelerationStructureDeviceAddressKHR(device, &asDeviceAddressInfo);
    }
    else
    {
        result[0] = uint64_t(getPtr()->getInternal());
    }

    for (size_t instanceNdx = 0; instanceNdx < m_bottomLevelInstances.size(); ++instanceNdx)
    {
        const BottomLevelAccelerationStructure &bottomLevelAccelerationStructure = *m_bottomLevelInstances[instanceNdx];
        const VkAccelerationStructureKHR accelerationStructureKHR = *bottomLevelAccelerationStructure.getPtr();

        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            asDeviceAddressInfo.accelerationStructure = accelerationStructureKHR;
            result[instanceNdx + 1] = vk.getAccelerationStructureDeviceAddressKHR(device, &asDeviceAddressInfo);
        }
        else
        {
            result[instanceNdx + 1] = uint64_t(accelerationStructureKHR.getInternal());
        }
    }

    return result;
}

const VkAccelerationStructureKHR *TopLevelAccelerationStructureKHR::getPtr(void) const
{
    return &m_accelerationStructureKHR.get();
}

void TopLevelAccelerationStructureKHR::prepareInstances(
    const DeviceInterface &vk, const VkDevice device,
    VkAccelerationStructureGeometryKHR &accelerationStructureGeometryKHR, std::vector<uint32_t> &maxPrimitiveCounts)
{
    maxPrimitiveCounts.resize(1);
    maxPrimitiveCounts[0] = static_cast<uint32_t>(m_bottomLevelInstances.size());

    VkDeviceOrHostAddressConstKHR instancesData;
    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        if (m_instanceBuffer.get() != VK_NULL_HANDLE)
        {
            if (m_useArrayOfPointers)
            {
                uint8_t *bufferStart = static_cast<uint8_t *>(m_instanceAddressBuffer->getAllocation().getHostPtr());
                VkDeviceSize bufferOffset = 0;
                VkDeviceOrHostAddressConstKHR firstInstance =
                    makeDeviceOrHostAddressConstKHR(vk, device, m_instanceBuffer->get(), 0);
                for (size_t instanceNdx = 0; instanceNdx < m_bottomLevelInstances.size(); ++instanceNdx)
                {
                    VkDeviceOrHostAddressConstKHR currentInstance;
                    currentInstance.deviceAddress =
                        firstInstance.deviceAddress + instanceNdx * sizeof(VkAccelerationStructureInstanceKHR);

                    deMemcpy(&bufferStart[bufferOffset], &currentInstance,
                             sizeof(VkDeviceOrHostAddressConstKHR::deviceAddress));
                    bufferOffset += sizeof(VkDeviceOrHostAddressConstKHR::deviceAddress);
                }
                flushMappedMemoryRange(vk, device, m_instanceAddressBuffer->getAllocation().getMemory(),
                                       m_instanceAddressBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

                instancesData = makeDeviceOrHostAddressConstKHR(vk, device, m_instanceAddressBuffer->get(), 0);
            }
            else
                instancesData = makeDeviceOrHostAddressConstKHR(vk, device, m_instanceBuffer->get(), 0);
        }
        else
            instancesData = makeDeviceOrHostAddressConstKHR(nullptr);
    }
    else
    {
        if (m_instanceBuffer.get() != VK_NULL_HANDLE)
        {
            if (m_useArrayOfPointers)
            {
                uint8_t *bufferStart = static_cast<uint8_t *>(m_instanceAddressBuffer->getAllocation().getHostPtr());
                VkDeviceSize bufferOffset = 0;
                for (size_t instanceNdx = 0; instanceNdx < m_bottomLevelInstances.size(); ++instanceNdx)
                {
                    VkDeviceOrHostAddressConstKHR currentInstance;
                    currentInstance.hostAddress = (uint8_t *)m_instanceBuffer->getAllocation().getHostPtr() +
                                                  instanceNdx * sizeof(VkAccelerationStructureInstanceKHR);

                    deMemcpy(&bufferStart[bufferOffset], &currentInstance,
                             sizeof(VkDeviceOrHostAddressConstKHR::hostAddress));
                    bufferOffset += sizeof(VkDeviceOrHostAddressConstKHR::hostAddress);
                }
                instancesData = makeDeviceOrHostAddressConstKHR(m_instanceAddressBuffer->getAllocation().getHostPtr());
            }
            else
                instancesData = makeDeviceOrHostAddressConstKHR(m_instanceBuffer->getAllocation().getHostPtr());
        }
        else
            instancesData = makeDeviceOrHostAddressConstKHR(nullptr);
    }

    VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesDataKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, //  VkStructureType sType;
        nullptr,                                                              //  const void* pNext;
        (VkBool32)(m_useArrayOfPointers ? true : false),                      //  VkBool32 arrayOfPointers;
        instancesData                                                         //  VkDeviceOrHostAddressConstKHR data;
    };

    accelerationStructureGeometryKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, //  VkStructureType sType;
        nullptr,                                               //  const void* pNext;
        VK_GEOMETRY_TYPE_INSTANCES_KHR,                        //  VkGeometryTypeKHR geometryType;
        makeVkAccelerationStructureInstancesDataKHR(
            accelerationStructureGeometryInstancesDataKHR), //  VkAccelerationStructureGeometryDataKHR geometry;
        (VkGeometryFlagsKHR)0u                              //  VkGeometryFlagsKHR flags;
    };
}

uint32_t TopLevelAccelerationStructure::getRequiredAllocationCount(void)
{
    return TopLevelAccelerationStructureKHR::getRequiredAllocationCount();
}

de::MovePtr<TopLevelAccelerationStructure> makeTopLevelAccelerationStructure()
{
    return de::MovePtr<TopLevelAccelerationStructure>(new TopLevelAccelerationStructureKHR);
}

bool queryAccelerationStructureSizeKHR(const DeviceInterface &vk, const VkDevice device,
                                       const VkCommandBuffer cmdBuffer,
                                       const std::vector<VkAccelerationStructureKHR> &accelerationStructureHandles,
                                       VkAccelerationStructureBuildTypeKHR buildType, const VkQueryPool queryPool,
                                       VkQueryType queryType, uint32_t firstQuery, std::vector<VkDeviceSize> &results)
{
    DE_ASSERT(queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR ||
              queryType == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR);

    if (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        // queryPool must be large enough to contain at least (firstQuery + accelerationStructureHandles.size()) queries
        vk.cmdResetQueryPool(cmdBuffer, queryPool, firstQuery, uint32_t(accelerationStructureHandles.size()));
        vk.cmdWriteAccelerationStructuresPropertiesKHR(cmdBuffer, uint32_t(accelerationStructureHandles.size()),
                                                       accelerationStructureHandles.data(), queryType, queryPool,
                                                       firstQuery);
        // results cannot be retrieved to CPU at the moment - you need to do it using getQueryPoolResults after cmdBuffer is executed. Meanwhile function returns a vector of 0s.
        results.resize(accelerationStructureHandles.size(), 0u);
        return false;
    }
    // buildType != VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR
    results.resize(accelerationStructureHandles.size(), 0u);
    vk.writeAccelerationStructuresPropertiesKHR(
        device, uint32_t(accelerationStructureHandles.size()), accelerationStructureHandles.data(), queryType,
        sizeof(VkDeviceSize) * accelerationStructureHandles.size(), results.data(), sizeof(VkDeviceSize));
    // results will contain proper values
    return true;
}

bool queryAccelerationStructureSize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                    const std::vector<VkAccelerationStructureKHR> &accelerationStructureHandles,
                                    VkAccelerationStructureBuildTypeKHR buildType, const VkQueryPool queryPool,
                                    VkQueryType queryType, uint32_t firstQuery, std::vector<VkDeviceSize> &results)
{
    return queryAccelerationStructureSizeKHR(vk, device, cmdBuffer, accelerationStructureHandles, buildType, queryPool,
                                             queryType, firstQuery, results);
}

RayTracingPipeline::RayTracingPipeline()
    : m_shadersModules()
    , m_pipelineLibraries()
    , m_shaderCreateInfos()
    , m_shadersGroupCreateInfos()
    , m_pipelineCreateFlags(0U)
    , m_pipelineCreateFlags2(0U)
    , m_maxRecursionDepth(1U)
    , m_maxPayloadSize(0U)
    , m_maxAttributeSize(0U)
    , m_deferredOperation(false)
    , m_workerThreadCount(0)
{
}

RayTracingPipeline::~RayTracingPipeline()
{
}

#define CHECKED_ASSIGN_SHADER(SHADER, STAGE) \
    if (SHADER == VK_SHADER_UNUSED_KHR)      \
        SHADER = STAGE;                      \
    else                                     \
        TCU_THROW(InternalError, "Attempt to reassign shader")

void RayTracingPipeline::addShader(VkShaderStageFlagBits shaderStage, Move<VkShaderModule> shaderModule, uint32_t group,
                                   const VkSpecializationInfo *specializationInfo,
                                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags,
                                   const void *pipelineShaderStageCreateInfopNext)
{
    addShader(shaderStage, makeVkSharedPtr(shaderModule), group, specializationInfo, pipelineShaderStageCreateFlags,
              pipelineShaderStageCreateInfopNext);
}

void RayTracingPipeline::addShader(VkShaderStageFlagBits shaderStage, de::SharedPtr<Move<VkShaderModule>> shaderModule,
                                   uint32_t group, const VkSpecializationInfo *specializationInfoPtr,
                                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags,
                                   const void *pipelineShaderStageCreateInfopNext)
{
    addShader(shaderStage, **shaderModule, group, specializationInfoPtr, pipelineShaderStageCreateFlags,
              pipelineShaderStageCreateInfopNext);
    m_shadersModules.push_back(shaderModule);
}

void RayTracingPipeline::addShader(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, uint32_t group,
                                   const VkSpecializationInfo *specializationInfoPtr,
                                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags,
                                   const void *pipelineShaderStageCreateInfopNext)
{
    if (group >= m_shadersGroupCreateInfos.size())
    {
        for (size_t groupNdx = m_shadersGroupCreateInfos.size(); groupNdx <= group; ++groupNdx)
        {
            VkRayTracingShaderGroupCreateInfoKHR shaderGroupCreateInfo = {
                VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, //  VkStructureType sType;
                nullptr,                                                    //  const void* pNext;
                VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR,              //  VkRayTracingShaderGroupTypeKHR type;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t generalShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t closestHitShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t anyHitShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t intersectionShader;
                nullptr, //  const void* pShaderGroupCaptureReplayHandle;
            };

            m_shadersGroupCreateInfos.push_back(shaderGroupCreateInfo);
        }
    }

    const uint32_t shaderStageNdx                               = (uint32_t)m_shaderCreateInfos.size();
    VkRayTracingShaderGroupCreateInfoKHR &shaderGroupCreateInfo = m_shadersGroupCreateInfos[group];

    switch (shaderStage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.generalShader, shaderStageNdx);
        break;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.generalShader, shaderStageNdx);
        break;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.generalShader, shaderStageNdx);
        break;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.anyHitShader, shaderStageNdx);
        break;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.closestHitShader, shaderStageNdx);
        break;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        CHECKED_ASSIGN_SHADER(shaderGroupCreateInfo.intersectionShader, shaderStageNdx);
        break;
    default:
        TCU_THROW(InternalError, "Unacceptable stage");
    }

    switch (shaderStage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
    case VK_SHADER_STAGE_MISS_BIT_KHR:
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
    {
        DE_ASSERT(shaderGroupCreateInfo.type == VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR);
        shaderGroupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

        break;
    }

    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
    {
        DE_ASSERT(shaderGroupCreateInfo.type != VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
        shaderGroupCreateInfo.type = (shaderGroupCreateInfo.intersectionShader == VK_SHADER_UNUSED_KHR) ?
                                         VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR :
                                         VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;

        break;
    }

    default:
        TCU_THROW(InternalError, "Unacceptable stage");
    }

    {
        const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //  VkStructureType sType;
            pipelineShaderStageCreateInfopNext,                  //  const void* pNext;
            pipelineShaderStageCreateFlags,                      //  VkPipelineShaderStageCreateFlags flags;
            shaderStage,                                         //  VkShaderStageFlagBits stage;
            shaderModule,                                        //  VkShaderModule module;
            "main",                                              //  const char* pName;
            specializationInfoPtr,                               //  const VkSpecializationInfo* pSpecializationInfo;
        };

        m_shaderCreateInfos.push_back(shaderCreateInfo);
    }
}

void RayTracingPipeline::setGroupCaptureReplayHandle(uint32_t group, const void *pShaderGroupCaptureReplayHandle)
{
    DE_ASSERT(static_cast<size_t>(group) < m_shadersGroupCreateInfos.size());
    m_shadersGroupCreateInfos[group].pShaderGroupCaptureReplayHandle = pShaderGroupCaptureReplayHandle;
}

void RayTracingPipeline::addLibrary(de::SharedPtr<de::MovePtr<RayTracingPipeline>> pipelineLibrary)
{
    m_pipelineLibraries.push_back(pipelineLibrary);
}

uint32_t RayTracingPipeline::getShaderGroupCount(void)
{
    return de::sizeU32(m_shadersGroupCreateInfos);
}

uint32_t RayTracingPipeline::getFullShaderGroupCount(void)
{
    uint32_t totalCount = getShaderGroupCount();

    for (const auto &lib : m_pipelineLibraries)
        totalCount += lib->get()->getFullShaderGroupCount();

    return totalCount;
}

Move<VkPipeline> RayTracingPipeline::createPipelineKHR(const DeviceInterface &vk, const VkDevice device,
                                                       const VkPipelineLayout pipelineLayout,
                                                       const std::vector<VkPipeline> &pipelineLibraries,
                                                       const VkPipelineCache pipelineCache)
{
    for (size_t groupNdx = 0; groupNdx < m_shadersGroupCreateInfos.size(); ++groupNdx)
        DE_ASSERT(m_shadersGroupCreateInfos[groupNdx].sType ==
                  VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);

    VkPipelineLibraryCreateInfoKHR librariesCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                            //  const void* pNext;
        de::sizeU32(pipelineLibraries),                     //  uint32_t libraryCount;
        de::dataOrNull(pipelineLibraries)                   //  VkPipeline* pLibraries;
    };
    const VkRayTracingPipelineInterfaceCreateInfoKHR pipelineInterfaceCreateInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                          //  const void* pNext;
        m_maxPayloadSize,                                                 //  uint32_t maxPayloadSize;
        m_maxAttributeSize                                                //  uint32_t maxAttributeSize;
    };
    const bool addPipelineInterfaceCreateInfo = m_maxPayloadSize != 0 || m_maxAttributeSize != 0;
    const VkRayTracingPipelineInterfaceCreateInfoKHR *pipelineInterfaceCreateInfoPtr =
        addPipelineInterfaceCreateInfo ? &pipelineInterfaceCreateInfo : nullptr;
    const VkPipelineLibraryCreateInfoKHR *librariesCreateInfoPtr =
        (pipelineLibraries.empty() ? nullptr : &librariesCreateInfo);

    Move<VkDeferredOperationKHR> deferredOperation;
    if (m_deferredOperation)
        deferredOperation = createDeferredOperationKHR(vk, device);

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0,                                                    // VkPipelineDynamicStateCreateFlags flags;
        static_cast<uint32_t>(m_dynamicStates.size()),        // uint32_t dynamicStateCount;
        m_dynamicStates.data(),                               // const VkDynamicState* pDynamicStates;
    };

    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                                //  const void* pNext;
        m_pipelineCreateFlags,                                  //  VkPipelineCreateFlags flags;
        de::sizeU32(m_shaderCreateInfos),                       //  uint32_t stageCount;
        de::dataOrNull(m_shaderCreateInfos),                    //  const VkPipelineShaderStageCreateInfo* pStages;
        de::sizeU32(m_shadersGroupCreateInfos),                 //  uint32_t groupCount;
        de::dataOrNull(m_shadersGroupCreateInfos),              //  const VkRayTracingShaderGroupCreateInfoKHR* pGroups;
        m_maxRecursionDepth,                                    //  uint32_t maxRecursionDepth;
        librariesCreateInfoPtr,                                 //  VkPipelineLibraryCreateInfoKHR* pLibraryInfo;
        pipelineInterfaceCreateInfoPtr, //  VkRayTracingPipelineInterfaceCreateInfoKHR* pLibraryInterface;
        &dynamicStateCreateInfo,        //  const VkPipelineDynamicStateCreateInfo* pDynamicState;
        pipelineLayout,                 //  VkPipelineLayout layout;
        VK_NULL_HANDLE,                 //  VkPipeline basePipelineHandle;
        0,                              //  int32_t basePipelineIndex;
    };

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
    if (m_pipelineCreateFlags2)
    {
        pipelineFlags2CreateInfo.flags = m_pipelineCreateFlags2;
        pipelineCreateInfo.pNext       = &pipelineFlags2CreateInfo;
        pipelineCreateInfo.flags       = 0;
    }

    VkPipeline object = VK_NULL_HANDLE;
    VkResult result   = vk.createRayTracingPipelinesKHR(device, deferredOperation.get(), pipelineCache, 1u,
                                                        &pipelineCreateInfo, nullptr, &object);
    const bool allowCompileRequired =
        ((m_pipelineCreateFlags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT) != 0);

    if (m_deferredOperation)
    {
        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS || (allowCompileRequired && result == VK_PIPELINE_COMPILE_REQUIRED));
        finishDeferredOperation(vk, device, deferredOperation.get(), m_workerThreadCount,
                                result == VK_OPERATION_NOT_DEFERRED_KHR);
    }

    if (allowCompileRequired && result == VK_PIPELINE_COMPILE_REQUIRED)
        throw CompileRequiredError("createRayTracingPipelinesKHR returned VK_PIPELINE_COMPILE_REQUIRED");

    Move<VkPipeline> pipeline(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, nullptr));
    return pipeline;
}

Move<VkPipeline> RayTracingPipeline::createPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries)
{
    std::vector<VkPipeline> rawPipelines;
    rawPipelines.reserve(pipelineLibraries.size());
    for (const auto &lib : pipelineLibraries)
        rawPipelines.push_back(lib.get()->get());

    return createPipelineKHR(vk, device, pipelineLayout, rawPipelines);
}

Move<VkPipeline> RayTracingPipeline::createPipeline(const DeviceInterface &vk, const VkDevice device,
                                                    const VkPipelineLayout pipelineLayout,
                                                    const std::vector<VkPipeline> &pipelineLibraries,
                                                    const VkPipelineCache pipelineCache)
{
    return createPipelineKHR(vk, device, pipelineLayout, pipelineLibraries, pipelineCache);
}

std::vector<de::SharedPtr<Move<VkPipeline>>> RayTracingPipeline::createPipelineWithLibraries(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout)
{
    for (size_t groupNdx = 0; groupNdx < m_shadersGroupCreateInfos.size(); ++groupNdx)
        DE_ASSERT(m_shadersGroupCreateInfos[groupNdx].sType ==
                  VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);

    DE_ASSERT(m_shaderCreateInfos.size() > 0);
    DE_ASSERT(m_shadersGroupCreateInfos.size() > 0);

    std::vector<de::SharedPtr<Move<VkPipeline>>> result, allLibraries, firstLibraries;
    for (auto it = begin(m_pipelineLibraries), eit = end(m_pipelineLibraries); it != eit; ++it)
    {
        auto childLibraries = (*it)->get()->createPipelineWithLibraries(vk, device, pipelineLayout);
        DE_ASSERT(childLibraries.size() > 0);
        firstLibraries.push_back(childLibraries[0]);
        std::copy(begin(childLibraries), end(childLibraries), std::back_inserter(allLibraries));
    }
    result.push_back(makeVkSharedPtr(createPipeline(vk, device, pipelineLayout, firstLibraries)));
    std::copy(begin(allLibraries), end(allLibraries), std::back_inserter(result));
    return result;
}

std::vector<uint8_t> RayTracingPipeline::getShaderGroupHandles(const DeviceInterface &vk, const VkDevice device,
                                                               const VkPipeline pipeline,
                                                               const uint32_t shaderGroupHandleSize,
                                                               const uint32_t firstGroup,
                                                               const uint32_t groupCount) const
{
    const auto handleArraySizeBytes = groupCount * shaderGroupHandleSize;
    std::vector<uint8_t> shaderHandles(handleArraySizeBytes);

    VK_CHECK(getRayTracingShaderGroupHandles(vk, device, pipeline, firstGroup, groupCount,
                                             static_cast<uintptr_t>(shaderHandles.size()),
                                             de::dataOrNull(shaderHandles)));

    return shaderHandles;
}

std::vector<uint8_t> RayTracingPipeline::getShaderGroupReplayHandles(const DeviceInterface &vk, const VkDevice device,
                                                                     const VkPipeline pipeline,
                                                                     const uint32_t shaderGroupHandleReplaySize,
                                                                     const uint32_t firstGroup,
                                                                     const uint32_t groupCount) const
{
    const auto handleArraySizeBytes = groupCount * shaderGroupHandleReplaySize;
    std::vector<uint8_t> shaderHandles(handleArraySizeBytes);

    VK_CHECK(getRayTracingCaptureReplayShaderGroupHandles(vk, device, pipeline, firstGroup, groupCount,
                                                          static_cast<uintptr_t>(shaderHandles.size()),
                                                          de::dataOrNull(shaderHandles)));

    return shaderHandles;
}

de::MovePtr<BufferWithMemory> RayTracingPipeline::createShaderBindingTable(
    const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline, Allocator &allocator,
    const uint32_t &shaderGroupHandleSize, const uint32_t shaderGroupBaseAlignment, const uint32_t &firstGroup,
    const uint32_t &groupCount, const VkBufferCreateFlags &additionalBufferCreateFlags,
    const VkBufferUsageFlags &additionalBufferUsageFlags, const MemoryRequirement &additionalMemoryRequirement,
    const VkDeviceAddress &opaqueCaptureAddress, const uint32_t shaderBindingTableOffset,
    const uint32_t shaderRecordSize, const void **shaderGroupDataPtrPerGroup, const bool autoAlignRecords)
{
    const auto shaderHandles =
        getShaderGroupHandles(vk, device, pipeline, shaderGroupHandleSize, firstGroup, groupCount);
    return createShaderBindingTable(vk, device, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment,
                                    shaderHandles, additionalBufferCreateFlags, additionalBufferUsageFlags,
                                    additionalMemoryRequirement, opaqueCaptureAddress, shaderBindingTableOffset,
                                    shaderRecordSize, shaderGroupDataPtrPerGroup, autoAlignRecords);
}

de::MovePtr<BufferWithMemory> RayTracingPipeline::createShaderBindingTable(
    const DeviceInterface &vk, const VkDevice device, Allocator &allocator, const uint32_t shaderGroupHandleSize,
    const uint32_t shaderGroupBaseAlignment, const std::vector<uint8_t> &shaderHandles,
    const VkBufferCreateFlags additionalBufferCreateFlags, const VkBufferUsageFlags additionalBufferUsageFlags,
    const MemoryRequirement &additionalMemoryRequirement, const VkDeviceAddress opaqueCaptureAddress,
    const uint32_t shaderBindingTableOffset, const uint32_t shaderRecordSize, const void **shaderGroupDataPtrPerGroup,
    const bool autoAlignRecords)
{
    DE_ASSERT(shaderGroupBaseAlignment != 0u);
    DE_ASSERT((shaderBindingTableOffset % shaderGroupBaseAlignment) == 0);
    DE_UNREF(shaderGroupBaseAlignment);

    const auto groupCount = de::sizeU32(shaderHandles) / shaderGroupHandleSize;
    const auto totalEntrySize =
        (autoAlignRecords ? (deAlign32(shaderGroupHandleSize + shaderRecordSize, shaderGroupHandleSize)) :
                            (shaderGroupHandleSize + shaderRecordSize));
    const uint32_t sbtSize            = shaderBindingTableOffset + groupCount * totalEntrySize;
    const VkBufferUsageFlags sbtFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalBufferUsageFlags;
    VkBufferCreateInfo sbtCreateInfo = makeBufferCreateInfo(sbtSize, sbtFlags);
    sbtCreateInfo.flags |= additionalBufferCreateFlags;
    VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2           = vk::initVulkanStructure();
    VkBufferOpaqueCaptureAddressCreateInfo sbtCaptureAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        uint64_t(opaqueCaptureAddress)                               // uint64_t opaqueCaptureAddress;
    };

    // when maintenance5 is tested then m_pipelineCreateFlags2 is non-zero
    if (m_pipelineCreateFlags2)
    {
        bufferUsageFlags2.usage = (VkBufferUsageFlags2KHR)sbtFlags;
        sbtCreateInfo.pNext     = &bufferUsageFlags2;
        sbtCreateInfo.usage     = 0;
    }

    if (opaqueCaptureAddress != 0u)
    {
        sbtCreateInfo.pNext = &sbtCaptureAddressInfo;
        sbtCreateInfo.flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
    }
    const MemoryRequirement sbtMemRequirements = MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                                 MemoryRequirement::DeviceAddress | additionalMemoryRequirement;
    de::MovePtr<BufferWithMemory> sbtBuffer =
        de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, sbtCreateInfo, sbtMemRequirements));
    vk::Allocation &sbtAlloc = sbtBuffer->getAllocation();

    // Copy handles to table, leaving space for ShaderRecordKHR after each handle.
    uint8_t *shaderBegin = (uint8_t *)sbtAlloc.getHostPtr() + shaderBindingTableOffset;
    for (uint32_t idx = 0; idx < groupCount; ++idx)
    {
        const uint8_t *shaderSrcPos = shaderHandles.data() + idx * shaderGroupHandleSize;
        uint8_t *shaderDstPos       = shaderBegin + idx * totalEntrySize;
        deMemcpy(shaderDstPos, shaderSrcPos, shaderGroupHandleSize);

        if (shaderGroupDataPtrPerGroup != nullptr && shaderGroupDataPtrPerGroup[idx] != nullptr)
        {
            DE_ASSERT(sbtSize >= static_cast<uint32_t>(shaderDstPos - shaderBegin) + shaderGroupHandleSize);

            deMemcpy(shaderDstPos + shaderGroupHandleSize, shaderGroupDataPtrPerGroup[idx], shaderRecordSize);
        }
    }

    flushMappedMemoryRange(vk, device, sbtAlloc.getMemory(), sbtAlloc.getOffset(), VK_WHOLE_SIZE);

    return sbtBuffer;
}

void RayTracingPipeline::setCreateFlags(const VkPipelineCreateFlags &pipelineCreateFlags)
{
    m_pipelineCreateFlags = pipelineCreateFlags;
}

void RayTracingPipeline::setCreateFlags2(const VkPipelineCreateFlags2KHR &pipelineCreateFlags2)
{
    m_pipelineCreateFlags2 = pipelineCreateFlags2;
}

void RayTracingPipeline::setMaxRecursionDepth(const uint32_t &maxRecursionDepth)
{
    m_maxRecursionDepth = maxRecursionDepth;
}

void RayTracingPipeline::setMaxPayloadSize(const uint32_t &maxPayloadSize)
{
    m_maxPayloadSize = maxPayloadSize;
}

void RayTracingPipeline::setMaxAttributeSize(const uint32_t &maxAttributeSize)
{
    m_maxAttributeSize = maxAttributeSize;
}

void RayTracingPipeline::setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount)
{
    m_deferredOperation = deferredOperation;
    m_workerThreadCount = workerThreadCount;
}

void RayTracingPipeline::addDynamicState(const VkDynamicState &dynamicState)
{
    m_dynamicStates.push_back(dynamicState);
}

class RayTracingPropertiesKHR : public RayTracingProperties
{
public:
    RayTracingPropertiesKHR() = delete;
    RayTracingPropertiesKHR(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice);
    virtual ~RayTracingPropertiesKHR();

    uint32_t getShaderGroupHandleSize(void) override
    {
        return m_rayTracingPipelineProperties.shaderGroupHandleSize;
    }
    uint32_t getShaderGroupHandleAlignment(void) override
    {
        return m_rayTracingPipelineProperties.shaderGroupHandleAlignment;
    }
    uint32_t getShaderGroupHandleCaptureReplaySize(void) override
    {
        return m_rayTracingPipelineProperties.shaderGroupHandleCaptureReplaySize;
    }
    uint32_t getMaxRecursionDepth(void) override
    {
        return m_rayTracingPipelineProperties.maxRayRecursionDepth;
    }
    uint32_t getMaxShaderGroupStride(void) override
    {
        return m_rayTracingPipelineProperties.maxShaderGroupStride;
    }
    uint32_t getShaderGroupBaseAlignment(void) override
    {
        return m_rayTracingPipelineProperties.shaderGroupBaseAlignment;
    }
    uint64_t getMaxGeometryCount(void) override
    {
        return m_accelerationStructureProperties.maxGeometryCount;
    }
    uint64_t getMaxInstanceCount(void) override
    {
        return m_accelerationStructureProperties.maxInstanceCount;
    }
    uint64_t getMaxPrimitiveCount(void) override
    {
        return m_accelerationStructureProperties.maxPrimitiveCount;
    }
    uint32_t getMaxDescriptorSetAccelerationStructures(void) override
    {
        return m_accelerationStructureProperties.maxDescriptorSetAccelerationStructures;
    }
    uint32_t getMaxRayDispatchInvocationCount(void) override
    {
        return m_rayTracingPipelineProperties.maxRayDispatchInvocationCount;
    }
    uint32_t getMaxRayHitAttributeSize(void) override
    {
        return m_rayTracingPipelineProperties.maxRayHitAttributeSize;
    }
    uint32_t getMaxMemoryAllocationCount(void) override
    {
        return m_maxMemoryAllocationCount;
    }

protected:
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_accelerationStructureProperties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;
    uint32_t m_maxMemoryAllocationCount;
};

RayTracingPropertiesKHR::~RayTracingPropertiesKHR()
{
}

RayTracingPropertiesKHR::RayTracingPropertiesKHR(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
    : RayTracingProperties(vki, physicalDevice)
{
    m_accelerationStructureProperties = getPhysicalDeviceExtensionProperties(vki, physicalDevice);
    m_rayTracingPipelineProperties    = getPhysicalDeviceExtensionProperties(vki, physicalDevice);
    m_maxMemoryAllocationCount = getPhysicalDeviceProperties(vki, physicalDevice).limits.maxMemoryAllocationCount;
}

de::MovePtr<RayTracingProperties> makeRayTracingProperties(const InstanceInterface &vki,
                                                           const VkPhysicalDevice physicalDevice)
{
    return de::MovePtr<RayTracingProperties>(new RayTracingPropertiesKHR(vki, physicalDevice));
}

static inline void cmdTraceRaysKHR(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                   const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                                   const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                                   const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                                   const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion,
                                   uint32_t width, uint32_t height, uint32_t depth)
{
    return vk.cmdTraceRaysKHR(commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                              hitShaderBindingTableRegion, callableShaderBindingTableRegion, width, height, depth);
}

void cmdTraceRays(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                  const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion, uint32_t width,
                  uint32_t height, uint32_t depth)
{
    DE_ASSERT(raygenShaderBindingTableRegion != nullptr);
    DE_ASSERT(missShaderBindingTableRegion != nullptr);
    DE_ASSERT(hitShaderBindingTableRegion != nullptr);
    DE_ASSERT(callableShaderBindingTableRegion != nullptr);

    return cmdTraceRaysKHR(vk, commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                           hitShaderBindingTableRegion, callableShaderBindingTableRegion, width, height, depth);
}

static inline void cmdTraceRaysIndirectKHR(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                           const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                                           const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                                           const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                                           const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion,
                                           VkDeviceAddress indirectDeviceAddress)
{
    DE_ASSERT(raygenShaderBindingTableRegion != nullptr);
    DE_ASSERT(missShaderBindingTableRegion != nullptr);
    DE_ASSERT(hitShaderBindingTableRegion != nullptr);
    DE_ASSERT(callableShaderBindingTableRegion != nullptr);
    DE_ASSERT(indirectDeviceAddress != 0);

    return vk.cmdTraceRaysIndirectKHR(commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                                      hitShaderBindingTableRegion, callableShaderBindingTableRegion,
                                      indirectDeviceAddress);
}

void cmdTraceRaysIndirect(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                          const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion,
                          VkDeviceAddress indirectDeviceAddress)
{
    return cmdTraceRaysIndirectKHR(vk, commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                                   hitShaderBindingTableRegion, callableShaderBindingTableRegion,
                                   indirectDeviceAddress);
}

static inline void cmdTraceRaysIndirect2KHR(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                            VkDeviceAddress indirectDeviceAddress)
{
    DE_ASSERT(indirectDeviceAddress != 0);

    return vk.cmdTraceRaysIndirect2KHR(commandBuffer, indirectDeviceAddress);
}

void cmdTraceRaysIndirect2(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                           VkDeviceAddress indirectDeviceAddress)
{
    return cmdTraceRaysIndirect2KHR(vk, commandBuffer, indirectDeviceAddress);
}

constexpr uint32_t NO_INT_VALUE = spv::RayQueryCommittedIntersectionTypeMax;

void generateRayQueryShaders(SourceCollections &programCollection, RayQueryTestParams params, std::string rayQueryPart,
                             float max_t)
{
    std::stringstream genericMiss;
    genericMiss << "#version 460\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "#extension GL_EXT_ray_query : require\n"
                   "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
                   "void main()\n"
                   "{\n"
                   "  payload.x = 2000;\n"
                   "  payload.y = 2000;\n"
                   "  payload.z = 2000;\n"
                   "  payload.w = 2000;\n"
                   "}\n";

    std::stringstream genericIsect;
    genericIsect << "#version 460\n"
                    "#extension GL_EXT_ray_tracing : require\n"
                    "hitAttributeEXT uvec4 hitValue;\n"
                    "void main()\n"
                    "{\n"
                    "  reportIntersectionEXT(0.5f, 0);\n"
                    "}\n";

    std::stringstream rtChit;
    rtChit << "#version 460    \n"
              "#extension GL_EXT_ray_tracing : require\n"
              "#extension GL_EXT_ray_query : require\n"
              "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
              "void main()\n"
              "{\n"
              "  uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + (gl_LaunchIDEXT.y * "
              "gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
              "  payload.x = index;\n"
              "  payload.y = gl_HitTEXT;\n"
              "  payload.z = 1000;\n"
              "  payload.w = 1000;\n"
              "}\n";

    std::stringstream genericChit;
    genericChit << "#version 460    \n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "#extension GL_EXT_ray_query : require\n"
                   "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
                   "void main()\n"
                   "{\n"
                   "  payload.x = 1000;\n"
                   "  payload.y = 1000;\n"
                   "  payload.z = 1000;\n"
                   "  payload.w = 1000;\n"
                   "}\n";

    std::stringstream genericRayTracingSetResultsShader;
    genericRayTracingSetResultsShader << "#version 460    \n"
                                         "#extension GL_EXT_ray_tracing : require\n"
                                         "#extension GL_EXT_ray_query : require\n"
                                         "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
                                         "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                                         "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                                         "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                                      << params.shaderFunctions
                                      << "void main()\n"
                                         "{\n"
                                         "  uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) "
                                         "+ (gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                                      << rayQueryPart
                                      << "  payload.x = x;\n"
                                         "  payload.y = y;\n"
                                         "  payload.z = z;\n"
                                         "  payload.w = w;\n"
                                         "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, true);

    switch (params.pipelineType)
    {
    case RayQueryShaderSourcePipeline::COMPUTE:
    {
        std::ostringstream compute;
        compute << "#version 460\n"
                   "#extension GL_EXT_ray_tracing : enable\n"
                   "#extension GL_EXT_ray_query : require\n"
                   "\n"
                   "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                   "struct ResultType { float x; float y; float z; float w; };\n"
                   "layout(std430, set = 0, binding = 0) buffer Results { ResultType results[]; };\n"
                   "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                   "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                   "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                << params.shaderFunctions
                << "void main() {\n"
                   "   uint index = (gl_NumWorkGroups.x * gl_WorkGroupSize.x) * gl_GlobalInvocationID.y + "
                   "gl_GlobalInvocationID.x;\n"
                << rayQueryPart
                << "   results[index].x = x;\n"
                   "   results[index].y = y;\n"
                   "   results[index].z = z;\n"
                   "   results[index].w = w;\n"
                   "}";

        programCollection.glslSources.add("comp", &buildOptions) << glu::ComputeSource(compute.str());

        break;
    }
    case RayQueryShaderSourcePipeline::GRAPHICS:
    {
        std::ostringstream vertex;

        if (params.shaderSourceType == RayQueryShaderSourceType::VERTEX)
        {
            vertex << "#version 460\n"
                      "#extension GL_EXT_ray_tracing : enable\n"
                      "#extension GL_EXT_ray_query : require\n"
                      "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                      "layout(location = 0) in vec4 in_position;\n"
                      "layout(rgba32f, set = 0, binding = 0) uniform image3D resultImage;\n"
                      "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                      "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                   << params.shaderFunctions
                   << "void main(void)\n"
                      "{\n"
                      "  const int  vertId = int(gl_VertexIndex % 3);\n"
                      "  if (vertId == 0)\n"
                      "  {\n"
                      "    ivec3 sz = imageSize(resultImage);\n"
                      "    int index = int(in_position.z);\n"
                      "    int idx = int(index % sz.x);\n"
                      "    int idy = int(index / sz.y);\n"
                   << rayQueryPart
                   << "     imageStore(resultImage, ivec3(idx, idy, 0), vec4(x, y, z, w));\n"
                      "  }\n"
                      "}\n";
        }
        else
        {
            vertex << "#version 460\n"
                      "layout(location = 0) in highp vec3 position;\n"
                      "\n"
                      "out gl_PerVertex {\n"
                      "   vec4 gl_Position;\n"
                      "};\n"
                      "\n"
                      "void main (void)\n"
                      "{\n"
                      "    gl_Position = vec4(position, 1.0);\n"
                      "}\n";
        }

        programCollection.glslSources.add("vert", &buildOptions) << glu::VertexSource(vertex.str());

        if (params.shaderSourceType == RayQueryShaderSourceType::FRAGMENT)
        {
            std::ostringstream frag;
            frag << "#version 460\n"
                    "#extension GL_EXT_ray_tracing : enable\n"
                    "#extension GL_EXT_ray_query : require\n"
                    "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                    "layout(rgba32f, set = 0, binding = 0) uniform image3D resultImage;\n"
                    "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                    "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                 << params.shaderFunctions
                 << "void main() {\n"
                    "    ivec3 sz = imageSize(resultImage);\n"
                    "    uint index = uint(gl_FragCoord.x) + sz.x * uint(gl_FragCoord.y);\n"
                 << rayQueryPart
                 << "    imageStore(resultImage, ivec3(gl_FragCoord.xy, 0), vec4(x, y, z, w));\n"
                    "}";

            programCollection.glslSources.add("frag", &buildOptions) << glu::FragmentSource(frag.str());
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::GEOMETRY)
        {
            std::stringstream geom;
            geom << "#version 460\n"
                    "#extension GL_EXT_ray_tracing : enable\n"
                    "#extension GL_EXT_ray_query : require\n"
                    "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                    "layout(triangles) in;\n"
                    "layout (triangle_strip, max_vertices = 3) out;\n"
                    "layout(rgba32f, set = 0, binding = 0) uniform image3D resultImage;\n"
                    "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                    "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                    "\n"
                    "in gl_PerVertex {\n"
                    "  vec4  gl_Position;\n"
                    "} gl_in[];\n"
                    "out gl_PerVertex {\n"
                    "  vec4 gl_Position;\n"
                    "};\n"
                 << params.shaderFunctions
                 << "void main (void)\n"
                    "{\n"
                    "  ivec3 sz = imageSize(resultImage);\n"
                    "  int index = int(gl_in[0].gl_Position.z);\n"
                    "  int idx = int(index % sz.x);\n"
                    "  int idy = int(index / sz.y);\n"
                 << rayQueryPart
                 << "  imageStore(resultImage, ivec3(idx, idy, 0), vec4(x, y, z, w));\n"
                    "  for (int i = 0; i < gl_in.length(); ++i)\n"
                    "  {\n"
                    "        gl_Position      = gl_in[i].gl_Position;\n"
                    "        EmitVertex();\n"
                    "  }\n"
                    "  EndPrimitive();\n"
                    "}\n";

            programCollection.glslSources.add("geom", &buildOptions) << glu::GeometrySource(geom.str());
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_EVALUATION)
        {
            {
                std::stringstream tesc;
                tesc << "#version 460\n"
                        "#extension GL_EXT_tessellation_shader : require\n"
                        "in gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_in[];\n"
                        "layout(vertices = 4) out;\n"
                        "out gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_out[];\n"
                        "\n"
                        "void main (void)\n"
                        "{\n"
                        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                        "  gl_TessLevelInner[0] = 1;\n"
                        "  gl_TessLevelInner[1] = 1;\n"
                        "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
                        "}\n";
                programCollection.glslSources.add("tesc", &buildOptions) << glu::TessellationControlSource(tesc.str());
            }

            {
                std::ostringstream tese;
                tese << "#version 460\n"
                        "#extension GL_EXT_ray_tracing : enable\n"
                        "#extension GL_EXT_tessellation_shader : require\n"
                        "#extension GL_EXT_ray_query : require\n"
                        "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                        "layout(rgba32f, set = 0, binding = 0) uniform image3D resultImage;\n"
                        "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                        "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                        "layout(quads, equal_spacing, ccw) in;\n"
                        "in gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_in[];\n"
                     << params.shaderFunctions
                     << "void main(void)\n"
                        "{\n"
                        "  ivec3 sz = imageSize(resultImage);\n"
                        "  int index = int(gl_in[0].gl_Position.z);\n"
                        "  int idx = int(index % sz.x);\n"
                        "  int idy = int(index / sz.y);\n"
                     << rayQueryPart
                     << "  imageStore(resultImage, ivec3(idx, idy, 0), vec4(x, y, z, w));\n"
                        "  gl_Position = gl_in[0].gl_Position;\n"
                        "}\n";

                programCollection.glslSources.add("tese", &buildOptions)
                    << glu::TessellationEvaluationSource(tese.str());
            }
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::TESSELLATION_CONTROL)
        {
            {
                std::ostringstream tesc;
                tesc << "#version 460\n"
                        "#extension GL_EXT_ray_tracing : enable\n"
                        "#extension GL_EXT_tessellation_shader : require\n"
                        "#extension GL_EXT_ray_query : require\n"
                        "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                        "layout(rgba32f, set = 0, binding = 0) uniform image3D resultImage;\n"
                        "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                        "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                        "in gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_in[];\n"
                        "layout(vertices = 4) out;\n"
                        "out gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_out[];\n"
                        "\n"
                     << params.shaderFunctions
                     << "void main(void)\n"
                        "{\n"
                        "  ivec3 sz = imageSize(resultImage);\n"
                        "  int index = int(gl_in[0].gl_Position.z);\n"
                        "  int idx = int(index % sz.x);\n"
                        "  int idy = int(index / sz.y);\n"
                     << rayQueryPart
                     << "  imageStore(resultImage, ivec3(idx, idy, 0), vec4(x, y, z, w));\n"
                        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                        "  gl_TessLevelInner[0] = 1;\n"
                        "  gl_TessLevelInner[1] = 1;\n"
                        "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
                        "}\n";

                programCollection.glslSources.add("tesc", &buildOptions) << glu::TessellationControlSource(tesc.str());
            }

            {
                std::ostringstream tese;
                tese << "#version 460\n"
                        "#extension GL_EXT_tessellation_shader : require\n"
                        "layout(quads, equal_spacing, ccw) in;\n"
                        "in gl_PerVertex\n"
                        "{\n"
                        "  vec4 gl_Position;\n"
                        "} gl_in[];\n"
                        "\n"
                        "void main(void)\n"
                        "{\n"
                        "  gl_Position = gl_in[0].gl_Position;\n"
                        "}\n";

                programCollection.glslSources.add("tese", &buildOptions)
                    << glu::TessellationEvaluationSource(tese.str());
            }
        }

        break;
    }
    case RayQueryShaderSourcePipeline::RAYTRACING:
    {
        std::stringstream rayGen;

        if (params.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION_RT)
        {
            rayGen << "#version 460\n"
                      "#extension GL_EXT_ray_tracing : enable\n"
                      "#extension GL_EXT_ray_query : require\n"
                      "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                      "struct ResultType { float x; float y; float z; float w; };\n"
                      "layout(std430, set = 0, binding = 0) buffer Results { ResultType results[]; };\n"
                      "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                      "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                      "layout(location = 0) rayPayloadEXT vec4 payload;\n"
                   << params.shaderFunctions
                   << "void main() {\n"
                      "   payload = vec4("
                   << NO_INT_VALUE << "," << max_t * 2
                   << ",0,0);\n"
                      "   uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + "
                      "(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                   << rayQueryPart
                   << "   results[index].x = x;\n"
                      "   results[index].y = y;\n"
                      "   results[index].z = z;\n"
                      "   results[index].w = w;\n"
                      "}";

            programCollection.glslSources.add("isect_rt", &buildOptions)
                << glu::IntersectionSource(updateRayTracingGLSL(genericIsect.str()));
            programCollection.glslSources.add("chit_rt", &buildOptions) << glu::ClosestHitSource(rtChit.str());
            programCollection.glslSources.add("ahit_rt", &buildOptions) << glu::AnyHitSource(genericChit.str());
            programCollection.glslSources.add("miss_rt", &buildOptions) << glu::MissSource(genericMiss.str());
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::RAY_GENERATION)
        {
            rayGen << "#version 460\n"
                      "#extension GL_EXT_ray_tracing : enable\n"
                      "#extension GL_EXT_ray_query : require\n"
                      "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                      "struct ResultType { float x; float y; float z; float w; };\n"
                      "layout(std430, set = 0, binding = 0) buffer Results { ResultType results[]; };\n"
                      "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                      "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                   << params.shaderFunctions
                   << "void main() {\n"
                      "   uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + "
                      "(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                   << rayQueryPart
                   << "   results[index].x = x;\n"
                      "   results[index].y = y;\n"
                      "   results[index].z = z;\n"
                      "   results[index].w = w;\n"
                      "}";
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::CALLABLE)
        {
            rayGen << "#version 460\n"
                      "#extension GL_EXT_ray_tracing : require\n"
                      "struct CallValue\n{\n"
                      "  uint index;\n"
                      "  vec4 hitAttrib;\n"
                      "};\n"
                      "layout(location = 0) callableDataEXT CallValue param;\n"
                      "struct ResultType { float x; float y; float z; float w; };\n"
                      "layout(std430, set = 0, binding = 0) buffer Results { ResultType results[]; };\n"
                      "void main()\n"
                      "{\n"
                      "  uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + (gl_LaunchIDEXT.y "
                      "* gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                      "  param.index = index;\n"
                      "  param.hitAttrib = vec4(0, 0, 0, 0);\n"
                      "  executeCallableEXT(0, 0);\n"
                      "  results[index].x = param.hitAttrib.x;\n"
                      "  results[index].y = param.hitAttrib.y;\n"
                      "  results[index].z = param.hitAttrib.z;\n"
                      "  results[index].w = param.hitAttrib.w;\n"
                      "}\n";
        }
        else
        {
            rayGen << "#version 460\n"
                      "#extension GL_EXT_ray_tracing : require\n"
                      "#extension GL_EXT_ray_query : require\n"
                      "layout(location = 0) rayPayloadEXT vec4 payload;\n"
                      "struct ResultType { float x; float y; float z; float w; };\n"
                      "layout(std430, set = 0, binding = 0) buffer Results { ResultType results[]; };\n"
                      "layout(set = 0, binding = 3) uniform accelerationStructureEXT traceEXTAccel;\n"
                      "void main()\n"
                      "{\n"
                      "  payload = vec4("
                   << NO_INT_VALUE << "," << max_t * 2
                   << ",0,0);\n"
                      "  uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + (gl_LaunchIDEXT.y "
                      "* gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                      "  traceRayEXT(traceEXTAccel, 0, 0xFF, 0, 0, 0, vec3(0.1, 0.1, 0.0), 0.0, vec3(0.0, 0.0, 1.0), "
                      "500.0, 0);\n"
                      "  results[index].x = payload.x;\n"
                      "  results[index].y = payload.y;\n"
                      "  results[index].z = payload.z;\n"
                      "  results[index].w = payload.w;\n"
                      "}\n";
        }

        programCollection.glslSources.add("rgen", &buildOptions) << glu::RaygenSource(rayGen.str());

        if (params.shaderSourceType == RayQueryShaderSourceType::CLOSEST_HIT)
        {
            programCollection.glslSources.add("chit", &buildOptions)
                << glu::ClosestHitSource(genericRayTracingSetResultsShader.str());
            programCollection.glslSources.add("miss", &buildOptions) << glu::MissSource(genericMiss.str());
            programCollection.glslSources.add("isect", &buildOptions)
                << glu::IntersectionSource(updateRayTracingGLSL(genericIsect.str()));
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::ANY_HIT)
        {
            programCollection.glslSources.add("ahit", &buildOptions)
                << glu::AnyHitSource(genericRayTracingSetResultsShader.str());
            programCollection.glslSources.add("miss", &buildOptions) << glu::MissSource(genericMiss.str());
            programCollection.glslSources.add("isect", &buildOptions)
                << glu::IntersectionSource(updateRayTracingGLSL(genericIsect.str()));
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::MISS)
        {

            programCollection.glslSources.add("chit", &buildOptions) << glu::ClosestHitSource(genericChit.str());
            programCollection.glslSources.add("miss_1", &buildOptions)
                << glu::MissSource(genericRayTracingSetResultsShader.str());
            programCollection.glslSources.add("isect", &buildOptions)
                << glu::IntersectionSource(updateRayTracingGLSL(genericIsect.str()));
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::INTERSECTION)
        {
            {
                std::stringstream chit;
                chit << "#version 460    \n"
                        "#extension GL_EXT_ray_tracing : require\n"
                        "#extension GL_EXT_ray_query : require\n"
                        "layout(location = 0) rayPayloadInEXT vec4 payload;\n"
                        "hitAttributeEXT vec4 hitAttrib;\n"
                        "void main()\n"
                        "{\n"
                        "  payload = hitAttrib;\n"
                        "}\n";

                programCollection.glslSources.add("chit", &buildOptions) << glu::ClosestHitSource(chit.str());
            }

            programCollection.glslSources.add("miss", &buildOptions) << glu::MissSource(genericMiss.str());

            {
                std::stringstream isect;
                isect << "#version 460\n"
                         "#extension GL_EXT_ray_tracing : require\n"
                         "#extension GL_EXT_ray_query : require\n"
                         "hitAttributeEXT vec4 hitValue;\n"
                         "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                         "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                         "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                      << params.shaderFunctions
                      << "void main()\n"
                         "{\n"
                         "  uint index = (gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y) + "
                         "(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x) + gl_LaunchIDEXT.x;\n"
                      << rayQueryPart
                      << "  hitValue.x = x;\n"
                         "  hitValue.y = y;\n"
                         "  hitValue.z = z;\n"
                         "  hitValue.w = w;\n"
                         "  reportIntersectionEXT(0.5f, 0);\n"
                         "}\n";

                programCollection.glslSources.add("isect_1", &buildOptions)
                    << glu::IntersectionSource(updateRayTracingGLSL(isect.str()));
            }
        }
        else if (params.shaderSourceType == RayQueryShaderSourceType::CALLABLE)
        {
            {
                std::stringstream call;
                call << "#version 460\n"
                        "#extension GL_EXT_ray_tracing : require\n"
                        "#extension GL_EXT_ray_query : require\n"
                        "struct CallValue\n{\n"
                        "  uint index;\n"
                        "  vec4 hitAttrib;\n"
                        "};\n"
                        "layout(location = 0) callableDataInEXT CallValue result;\n"
                        "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n"
                        "layout(set = 0, binding = 1) uniform accelerationStructureEXT scene;\n"
                        "layout(std430, set = 0, binding = 2) buffer Rays { Ray rays[]; };\n"
                     << params.shaderFunctions
                     << "void main()\n"
                        "{\n"
                        "  uint index = result.index;\n"
                     << rayQueryPart
                     << "  result.hitAttrib.x = x;\n"
                        "  result.hitAttrib.y = y;\n"
                        "  result.hitAttrib.z = z;\n"
                        "  result.hitAttrib.w = w;\n"
                        "}\n";

                programCollection.glslSources.add("call", &buildOptions)
                    << glu::CallableSource(updateRayTracingGLSL(call.str()));
            }

            programCollection.glslSources.add("chit", &buildOptions) << glu::ClosestHitSource(genericChit.str());
            programCollection.glslSources.add("miss", &buildOptions) << glu::MissSource(genericMiss.str());
        }

        break;
    }
    default:
    {
        TCU_FAIL("Shader type not valid.");
    }
    }
}

#else

uint32_t rayTracingDefineAnything()
{
    return 0;
}

#endif // CTS_USES_VULKANSC

} // namespace vk
