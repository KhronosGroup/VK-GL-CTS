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

#include <vector>
#include <string>
#include <thread>
#include <limits>

namespace vk
{

std::string getCommonRayGenerationShader(void)
{
    return "#version 460 core\n"
           "#extension GL_EXT_nonuniform_qualifier : enable\n"
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

const char *getRayTracingExtensionUsed(void)
{
    return "VK_KHR_ray_tracing";
}

RaytracedGeometryBase::RaytracedGeometryBase(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                             VkIndexType indexType)
    : m_geometryType(geometryType)
    , m_vertexFormat(vertexFormat)
    , m_indexType(indexType)
    , m_geometryFlags((VkGeometryFlagsKHR)0u)
{
}

RaytracedGeometryBase::~RaytracedGeometryBase()
{
}

de::SharedPtr<RaytracedGeometryBase> makeRaytracedGeometry(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                                           VkIndexType indexType)
{
    switch (vertexFormat)
    {
    case VK_FORMAT_R32G32_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec2, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec2, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec2, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    case VK_FORMAT_R32G32B32_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec3, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec3, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<tcu::Vec3, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    case VK_FORMAT_R16G16_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    case VK_FORMAT_R16G16_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16SNorm, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16SNorm, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec2_16SNorm, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    case VK_FORMAT_R16G16B16A16_SNORM:
        switch (indexType)
        {
        case VK_INDEX_TYPE_UINT16:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16SNorm, uint16_t>(geometryType));
        case VK_INDEX_TYPE_UINT32:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16SNorm, uint32_t>(geometryType));
        case VK_INDEX_TYPE_NONE_KHR:
            return de::SharedPtr<RaytracedGeometryBase>(new RaytracedGeometry<Vec4_16SNorm, EmptyIndex>(geometryType));
        default:
            TCU_THROW(InternalError, "Wrong index type");
        };
    default:
        TCU_THROW(InternalError, "Wrong vertex format");
    };
}

static inline VkDeviceOrHostAddressConstKHR makeDeviceOrHostAddressConstKHR(const void *hostAddress)
{
    // VS2015: Cannot create as a const due to cannot assign hostAddress due to it is a second field. Only assigning of first field supported.
    VkDeviceOrHostAddressConstKHR result;

    deMemset(&result, 0, sizeof(result));

    result.hostAddress = hostAddress;

    return result;
}

static inline VkDeviceOrHostAddressKHR makeDeviceOrHostAddressKHR(void *hostAddress)
{
    // VS2015: Cannot create as a const due to cannot assign hostAddress due to it is a second field. Only assigning of first field supported.
    VkDeviceOrHostAddressKHR result;

    deMemset(&result, 0, sizeof(result));

    result.hostAddress = hostAddress;

    return result;
}

static inline VkDeviceOrHostAddressConstKHR makeDeviceOrHostAddressConstKHR(const DeviceInterface &vk,
                                                                            const VkDevice device, VkBuffer buffer,
                                                                            VkDeviceSize offset)
{
    // VS2015: Cannot create as a const due to cannot assign hostAddress due to it is a second field. Only assigning of first field supported.
    VkDeviceOrHostAddressConstKHR result;

    deMemset(&result, 0, sizeof(result));

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, // VkStructureType  sType;
        DE_NULL,                                          // const void*  pNext;
        buffer,                                           // VkBuffer            buffer
    };
    result.deviceAddress = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo) + offset;

    return result;
}

static inline VkDeviceOrHostAddressKHR makeDeviceOrHostAddressKHR(const DeviceInterface &vk, const VkDevice device,
                                                                  VkBuffer buffer, VkDeviceSize offset)
{
    // VS2015: Cannot create as a const due to cannot assign hostAddress due to it is a second field. Only assigning of first field supported.
    VkDeviceOrHostAddressKHR result;

    deMemset(&result, 0, sizeof(result));

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, // VkStructureType  sType;
        DE_NULL,                                          // const void*  pNext;
        buffer,                                           // VkBuffer            buffer
    };
    result.deviceAddress = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo) + offset;

    return result;
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

static inline VkMemoryRequirements getAccelerationStructureMemoryRequirements(
    const DeviceInterface &vk, const VkDevice device, const VkAccelerationStructureKHR accelerationStructure,
    const VkAccelerationStructureMemoryRequirementsTypeKHR memoryRequirementsType,
    const VkAccelerationStructureBuildTypeKHR buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
{
    const VkAccelerationStructureMemoryRequirementsInfoKHR accelerationStructureMemoryRequirementsInfoKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR, //  VkStructureType sType;
        DE_NULL,                                                               //  const void* pNext;
        memoryRequirementsType, //  VkAccelerationStructureMemoryRequirementsTypeKHR type;
        buildType,              //  VkAccelerationStructureBuildTypeKHR buildType;
        accelerationStructure   //  VkAccelerationStructureKHR accelerationStructure;
    };
    VkMemoryRequirements2 memoryRequirements2 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, //  VkStructureType sType;
        DE_NULL,                                 //  void* pNext;
        {0, 0, 0}                                //  VkMemoryRequirements memoryRequirements;
    };

    vk.getAccelerationStructureMemoryRequirementsKHR(device, &accelerationStructureMemoryRequirementsInfoKHR,
                                                     &memoryRequirements2);

    return memoryRequirements2.memoryRequirements;
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

SerialStorage::SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                             const VkAccelerationStructureBuildTypeKHR buildType, const VkDeviceSize storageSize)
    : m_buildType(buildType)
{
    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
        storageSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator, bufferCreateInfo,
        MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
}

VkDeviceOrHostAddressKHR SerialStorage::getAddress(const DeviceInterface &vk, const VkDevice device)
{
    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        return makeDeviceOrHostAddressKHR(vk, device, m_buffer->get(), 0);
    else
        return makeDeviceOrHostAddressKHR(m_buffer->getAllocation().getHostPtr());
}

VkDeviceOrHostAddressConstKHR SerialStorage::getAddressConst(const DeviceInterface &vk, const VkDevice device)
{
    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        return makeDeviceOrHostAddressConstKHR(vk, device, m_buffer->get(), 0);
    else
        return makeDeviceOrHostAddressConstKHR(m_buffer->getAllocation().getHostPtr());
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure() : m_geometriesData()
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

void BottomLevelAccelerationStructure::setDefaultGeometryData(const VkShaderStageFlagBits testStage)
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
        geometryData.push_back(tcu::Vec3(-1.0f, +1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, +1.0f, z));
    }
    else
    {
        geometryData.reserve(2);

        geometryData.push_back(tcu::Vec3(-1.0f, -1.0f, z));
        geometryData.push_back(tcu::Vec3(+1.0f, +1.0f, z));
    }

    setGeometryCount(1u);

    addGeometry(geometryData, trianglesData);
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

void BottomLevelAccelerationStructure::addGeometry(const std::vector<tcu::Vec3> &geometryData, const bool triangles,
                                                   const VkGeometryFlagsKHR geometryFlags)
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
    addGeometry(geometry);
}

BufferWithMemory *createVertexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                     const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    DE_ASSERT(geometriesData.size() != 0);

    VkDeviceSize bufferSizeBytes = 0;
    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
        bufferSizeBytes += deAlignSize(geometriesData[geometryNdx]->getVertexByteSize(), 8);

    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
        bufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                    MemoryRequirement::DeviceAddress);
}

void updateVertexBuffer(const DeviceInterface &vk, const VkDevice device,
                        const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData,
                        BufferWithMemory *vertexBuffer)
{
    const Allocation &geometryAlloc = vertexBuffer->getAllocation();
    uint8_t *bufferStart            = static_cast<uint8_t *>(geometryAlloc.getHostPtr());
    VkDeviceSize bufferOffset       = 0;

    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
    {
        const void *geometryPtr      = geometriesData[geometryNdx]->getVertexPointer();
        const size_t geometryPtrSize = geometriesData[geometryNdx]->getVertexByteSize();

        deMemcpy(&bufferStart[bufferOffset], geometryPtr, geometryPtrSize);

        bufferOffset += deAlignSize(geometryPtrSize, 8);
    }

    flushMappedMemoryRange(vk, device, geometryAlloc.getMemory(), geometryAlloc.getOffset(), VK_WHOLE_SIZE);
}

BufferWithMemory *createIndexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                    const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData)
{
    DE_ASSERT(!geometriesData.empty());

    VkDeviceSize bufferSizeBytes = 0;
    for (size_t geometryNdx = 0; geometryNdx < geometriesData.size(); ++geometryNdx)
        if (geometriesData[geometryNdx]->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
            bufferSizeBytes += deAlignSize(geometriesData[geometryNdx]->getIndexByteSize(), 8);

    if (bufferSizeBytes == 0)
        return DE_NULL;

    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
        bufferSizeBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                    MemoryRequirement::DeviceAddress);
}

void updateIndexBuffer(const DeviceInterface &vk, const VkDevice device,
                       const std::vector<de::SharedPtr<RaytracedGeometryBase>> &geometriesData,
                       BufferWithMemory *indexBuffer)
{
    const Allocation &indexAlloc = indexBuffer->getAllocation();
    uint8_t *bufferStart         = static_cast<uint8_t *>(indexAlloc.getHostPtr());
    VkDeviceSize bufferOffset    = 0;

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

    flushMappedMemoryRange(vk, device, indexAlloc.getMemory(), indexAlloc.getOffset(), VK_WHOLE_SIZE);
}

class BottomLevelAccelerationStructureKHR : public BottomLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    BottomLevelAccelerationStructureKHR();
    BottomLevelAccelerationStructureKHR(const BottomLevelAccelerationStructureKHR &other) = delete;
    virtual ~BottomLevelAccelerationStructureKHR();

    void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType) override;
    void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags) override;
    void setDeferredOperation(const bool deferredOperation) override;
    void setUseArrayOfPointers(const bool useArrayOfPointers) override;
    void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                    const uint32_t indirectBufferStride) override;
    VkBuildAccelerationStructureFlagsKHR getBuildFlags() const override;

    void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, VkDeviceAddress deviceAddress,
                VkDeviceSize compactCopySize) override;
    void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer) override;
    void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                  BottomLevelAccelerationStructure *accelerationStructure, VkDeviceSize compactCopySize) override;

    void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                   SerialStorage *storage) override;
    void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                     SerialStorage *storage) override;

    const VkAccelerationStructureKHR *getPtr(void) const override;

protected:
    VkAccelerationStructureBuildTypeKHR m_buildType;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags;
    bool m_deferredOperation;
    bool m_useArrayOfPointers;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
    de::MovePtr<BufferWithMemory> m_indexBuffer;
    de::MovePtr<Allocation> m_accelerationStructureAlloc;
    de::MovePtr<BufferWithMemory> m_scratchBuffer;
    Move<VkAccelerationStructureKHR> m_accelerationStructureKHR;
    VkBuffer m_indirectBuffer;
    VkDeviceSize m_indirectBufferOffset;
    uint32_t m_indirectBufferStride;
};

uint32_t BottomLevelAccelerationStructureKHR::getRequiredAllocationCount(void)
{
    /*
        de::MovePtr<BufferWithMemory>                            m_geometryBuffer; // but only when m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR
        de::MovePtr<Allocation>                                    m_accelerationStructureAlloc;
        de::MovePtr<BufferWithMemory>                            m_scratchBuffer;
    */
    return 3u;
}

BottomLevelAccelerationStructureKHR::~BottomLevelAccelerationStructureKHR()
{
}

BottomLevelAccelerationStructureKHR::BottomLevelAccelerationStructureKHR()
    : BottomLevelAccelerationStructure()
    , m_buildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    , m_buildFlags(0u)
    , m_deferredOperation(false)
    , m_useArrayOfPointers(false)
    , m_vertexBuffer()
    , m_indexBuffer()
    , m_accelerationStructureAlloc()
    , m_scratchBuffer()
    , m_accelerationStructureKHR()
    , m_indirectBuffer(DE_NULL)
    , m_indirectBufferOffset(0)
    , m_indirectBufferStride(0)
{
}

void BottomLevelAccelerationStructureKHR::setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)
{
    m_buildType = buildType;
}

void BottomLevelAccelerationStructureKHR::setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags)
{
    m_buildFlags = flags;
}

void BottomLevelAccelerationStructureKHR::setDeferredOperation(const bool deferredOperation)
{
    m_deferredOperation = deferredOperation;
}

void BottomLevelAccelerationStructureKHR::setUseArrayOfPointers(const bool useArrayOfPointers)
{
    m_useArrayOfPointers = useArrayOfPointers;
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
                                                 VkDeviceAddress deviceAddress, VkDeviceSize compactCopySize)
{
    DE_ASSERT(!m_geometriesData.empty() != !(compactCopySize == 0)); // logical xor

    {
        std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> accelerationStructureCreateGeometryTypeInfosKHR(
            m_geometriesData.size());

        for (size_t geometryNdx = 0; geometryNdx < m_geometriesData.size(); ++geometryNdx)
        {
            de::SharedPtr<RaytracedGeometryBase> &geometryData = m_geometriesData[geometryNdx];
            const VkAccelerationStructureCreateGeometryTypeInfoKHR accelerationStructureCreateGeometryTypeInfoKHR = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR, //  VkStructureType sType;
                DE_NULL,                                                                //  const void* pNext;
                geometryData->getGeometryType(),   //  VkGeometryTypeKHR geometryType;
                geometryData->getPrimitiveCount(), //  uint32_t maxPrimitiveCount;
                geometryData->getIndexType(),      //  VkIndexType indexType;
                geometryData->getVertexCount(),    //  uint32_t maxVertexCount;
                geometryData->getVertexFormat(),   //  VkFormat vertexFormat;
                false                              //  VkBool32 allowsTransforms;
            };

            accelerationStructureCreateGeometryTypeInfosKHR[geometryNdx] =
                accelerationStructureCreateGeometryTypeInfoKHR;
        }

        const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                  //  const void* pNext;
            compactCopySize,                                          //  VkDeviceSize compactedSize;
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,          //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                             //  VkBuildAccelerationStructureFlagsKHR flags;
            static_cast<uint32_t>(
                accelerationStructureCreateGeometryTypeInfosKHR.size()), //  uint32_t maxGeometryCount;
            dataOrNullPtr(
                accelerationStructureCreateGeometryTypeInfosKHR), //  const VkAccelerationStructureCreateGeometryTypeInfoKHR* pGeometryInfos;
            deviceAddress                                         //  VkDeviceAddress deviceAddress;
        };

        m_accelerationStructureKHR =
            createAccelerationStructureKHR(vk, device, &accelerationStructureCreateInfoKHR, DE_NULL);
    }

    {
        const VkMemoryRequirements memoryRequirements = getAccelerationStructureMemoryRequirements(
            vk, device, m_accelerationStructureKHR.get(), VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR,
            m_buildType);

        m_accelerationStructureAlloc = allocator.allocate(memoryRequirements, vk::MemoryRequirement::Local);
    }

    {
        const VkBindAccelerationStructureMemoryInfoKHR bindAccelerationStructureMemoryInfoKHR = {
            VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                       //  const void* pNext;
            m_accelerationStructureKHR.get(),          //  VkAccelerationStructureKHR accelerationStructure;
            m_accelerationStructureAlloc->getMemory(), //  VkDeviceMemory memory;
            m_accelerationStructureAlloc->getOffset(), //  VkDeviceSize memoryOffset;
            0,                                         //  uint32_t deviceIndexCount;
            DE_NULL,                                   //  const uint32_t* pDeviceIndices;
        };

        VK_CHECK(vk.bindAccelerationStructureMemoryKHR(device, 1, &bindAccelerationStructureMemoryInfoKHR));
    }

    {
        const VkMemoryRequirements memoryRequirements = getAccelerationStructureMemoryRequirements(
            vk, device, m_accelerationStructureKHR.get(),
            VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR, m_buildType);
        if (memoryRequirements.size > 0u)
        {
            const VkBufferCreateInfo bufferCreateInfo =
                makeBufferCreateInfo(memoryRequirements.size,
                                     VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            m_scratchBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, bufferCreateInfo,
                MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
        }
    }

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR && !m_geometriesData.empty())
    {
        m_vertexBuffer = de::MovePtr<BufferWithMemory>(createVertexBuffer(vk, device, allocator, m_geometriesData));
        m_indexBuffer  = de::MovePtr<BufferWithMemory>(createIndexBuffer(vk, device, allocator, m_geometriesData));
    }
}

void BottomLevelAccelerationStructureKHR::build(const DeviceInterface &vk, const VkDevice device,
                                                const VkCommandBuffer cmdBuffer)
{
    DE_ASSERT(!m_geometriesData.empty());
    DE_ASSERT(m_accelerationStructureKHR.get() != DE_NULL);

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        updateVertexBuffer(vk, device, m_geometriesData, m_vertexBuffer.get());
        if (m_indexBuffer.get() != DE_NULL)
            updateIndexBuffer(vk, device, m_geometriesData, m_indexBuffer.get());
    }

    {
        std::vector<VkAccelerationStructureGeometryKHR> accelerationStructureGeometriesKHR(m_geometriesData.size());
        std::vector<VkAccelerationStructureGeometryKHR *> accelerationStructureGeometriesKHRPointers(
            m_geometriesData.size());
        std::vector<VkAccelerationStructureBuildOffsetInfoKHR> accelerationStructureBuildOffsetInfoKHR(
            m_geometriesData.size());
        VkDeviceSize vertexBufferOffset = 0, indexBufferOffset = 0;

        for (size_t geometryNdx = 0; geometryNdx < m_geometriesData.size(); ++geometryNdx)
        {
            de::SharedPtr<RaytracedGeometryBase> &geometryData = m_geometriesData[geometryNdx];
            VkDeviceOrHostAddressConstKHR vertexData, indexData;
            if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
            {
                vertexData = makeDeviceOrHostAddressConstKHR(vk, device, m_vertexBuffer->get(), vertexBufferOffset);
                vertexBufferOffset += deAlignSize(geometryData->getVertexByteSize(), 8);

                if (geometryData->getIndexType() != VK_INDEX_TYPE_NONE_KHR)
                {
                    indexData = makeDeviceOrHostAddressConstKHR(vk, device, m_indexBuffer->get(), indexBufferOffset);
                    indexBufferOffset += deAlignSize(geometryData->getIndexByteSize(), 8);
                }
                else
                    indexData = makeDeviceOrHostAddressConstKHR(DE_NULL);
            }
            else
            {
                vertexData = makeDeviceOrHostAddressConstKHR(geometryData->getVertexPointer());
                if (m_indexBuffer.get() != DE_NULL)
                    indexData = makeDeviceOrHostAddressConstKHR(geometryData->getIndexPointer());
                else
                    indexData = makeDeviceOrHostAddressConstKHR(DE_NULL);
            }

            const VkAccelerationStructureGeometryTrianglesDataKHR accelerationStructureGeometryTrianglesDataKHR = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, //  VkStructureType sType;
                DE_NULL,                                                              //  const void* pNext;
                geometryData->getVertexFormat(),                                      //  VkFormat vertexFormat;
                vertexData,                               //  VkDeviceOrHostAddressConstKHR vertexData;
                geometryData->getVertexStride(),          //  VkDeviceSize vertexStride;
                geometryData->getIndexType(),             //  VkIndexType indexType;
                indexData,                                //  VkDeviceOrHostAddressConstKHR indexData;
                makeDeviceOrHostAddressConstKHR(DE_NULL), //  VkDeviceOrHostAddressConstKHR transformData;
            };

            const VkAccelerationStructureGeometryAabbsDataKHR accelerationStructureGeometryAabbsDataKHR = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR, //  VkStructureType sType;
                DE_NULL,                                                          //  const void* pNext;
                vertexData,                   //  VkDeviceOrHostAddressConstKHR data;
                geometryData->getAABBStride() //  VkDeviceSize stride;
            };
            const VkAccelerationStructureGeometryDataKHR geometry =
                (geometryData->isTrianglesType()) ?
                    makeVkAccelerationStructureGeometryDataKHR(accelerationStructureGeometryTrianglesDataKHR) :
                    makeVkAccelerationStructureGeometryDataKHR(accelerationStructureGeometryAabbsDataKHR);
            const VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, //  VkStructureType sType;
                DE_NULL,                                               //  const void* pNext;
                geometryData->getGeometryType(),                       //  VkGeometryTypeKHR geometryType;
                geometry,                        //  VkAccelerationStructureGeometryDataKHR geometry;
                geometryData->getGeometryFlags() //  VkGeometryFlagsKHR flags;
            };

            const VkAccelerationStructureBuildOffsetInfoKHR accelerationStructureBuildOffsetInfosKHR = {
                geometryData->getPrimitiveCount(), //  uint32_t primitiveCount;
                0,                                 //  uint32_t primitiveOffset;
                0,                                 //  uint32_t firstVertex;
                0                                  //  uint32_t firstTransform;
            };

            accelerationStructureGeometriesKHR[geometryNdx]         = accelerationStructureGeometryKHR;
            accelerationStructureGeometriesKHRPointers[geometryNdx] = &accelerationStructureGeometriesKHR[geometryNdx];
            accelerationStructureBuildOffsetInfoKHR[geometryNdx]    = accelerationStructureBuildOffsetInfosKHR;
        }

        VkAccelerationStructureGeometryKHR *accelerationStructureGeometriesKHRPointer =
            accelerationStructureGeometriesKHR.data();
        VkAccelerationStructureGeometryKHR **accelerationStructureGeometry =
            (m_useArrayOfPointers) ? accelerationStructureGeometriesKHRPointers.data() :
                                     &accelerationStructureGeometriesKHRPointer;
        VkDeviceOrHostAddressKHR scratchData =
            (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                makeDeviceOrHostAddressKHR(vk, device, m_scratchBuffer->get(), 0) :
                makeDeviceOrHostAddressKHR(m_scratchBuffer->getAllocation().getHostPtr());

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                          //  const void* pNext;
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                  //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                    //  VkBuildAccelerationStructureFlagsKHR flags;
            false,                                           //  VkBool32 update;
            DE_NULL,                                         //  VkAccelerationStructureKHR srcAccelerationStructure;
            m_accelerationStructureKHR.get(),                //  VkAccelerationStructureKHR dstAccelerationStructure;
            (VkBool32)(m_useArrayOfPointers ? true : false), //  VkBool32 geometryArrayOfPointers;
            static_cast<uint32_t>(accelerationStructureGeometriesKHR.size()), //  uint32_t geometryCount;
            (const VkAccelerationStructureGeometryKHR **)
                accelerationStructureGeometry, //  const VkAccelerationStructureGeometryKHR** ppGeometries;
            scratchData                        //  VkDeviceOrHostAddressKHR scratchData;
        };
        VkAccelerationStructureBuildOffsetInfoKHR *accelerationStructureBuildOffsetInfoKHRPtr =
            accelerationStructureBuildOffsetInfoKHR.data();

        if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            if (m_indirectBuffer == DE_NULL)
                vk.cmdBuildAccelerationStructureKHR(
                    cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                    (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr);
            else
                vk.cmdBuildAccelerationStructureIndirectKHR(cmdBuffer, &accelerationStructureBuildGeometryInfoKHR,
                                                            m_indirectBuffer, m_indirectBufferOffset,
                                                            m_indirectBufferStride);
        }
        else if (!m_deferredOperation)
        {
            VK_CHECK(vk.buildAccelerationStructureKHR(
                device, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr));
        }
        else
        {
            VkDeferredOperationKHR deferredOperation = DE_NULL;

            VK_CHECK(vk.createDeferredOperationKHR(device, DE_NULL, &deferredOperation));

            VkDeferredOperationInfoKHR deferredOperationInfoKHR = {
                VK_STRUCTURE_TYPE_DEFERRED_OPERATION_INFO_KHR, //  VkStructureType sType;
                DE_NULL,                                       //  const void* pNext;
                deferredOperation                              //  VkDeferredOperationKHR operationHandle;
            };

            accelerationStructureBuildGeometryInfoKHR.pNext = &deferredOperationInfoKHR;

            VkResult result = vk.buildAccelerationStructureKHR(
                device, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr);

            DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                      result == VK_SUCCESS);
            DE_UNREF(result);

            VK_CHECK(finishDeferredOperation(vk, device, deferredOperation));

            accelerationStructureBuildGeometryInfoKHR.pNext = DE_NULL;
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
                                                   VkDeviceSize compactCopySize)
{
    VkCopyAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                // const void* pNext;
        *(accelerationStructure->getPtr()),                     // VkAccelerationStructureKHR src;
        *(getPtr()),                                            // VkAccelerationStructureKHR dst;
        compactCopySize > 0u ? VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR :
                               VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureKHR(device, &copyAccelerationStructureInfo));
    }
    else
    {
        VkDeferredOperationKHR deferredOperation = DE_NULL;

        VK_CHECK(vk.createDeferredOperationKHR(device, DE_NULL, &deferredOperation));

        VkDeferredOperationInfoKHR deferredOperationInfoKHR = {
            VK_STRUCTURE_TYPE_DEFERRED_OPERATION_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                       //  const void* pNext;
            deferredOperation                              //  VkDeferredOperationKHR operationHandle;
        };

        copyAccelerationStructureInfo.pNext = &deferredOperationInfoKHR;

        VkResult result = vk.copyAccelerationStructureKHR(device, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);
        DE_UNREF(result);

        VK_CHECK(finishDeferredOperation(vk, device, deferredOperation));
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
    VkCopyAccelerationStructureToMemoryInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                          // const void* pNext;
        *(getPtr()),                                                      // VkAccelerationStructureKHR src;
        storage->getAddress(vk, device),                                  // VkDeviceOrHostAddressKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR                 // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureToMemoryKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else
    {
        VK_CHECK(vk.copyAccelerationStructureToMemoryKHR(device, &copyAccelerationStructureInfo));
    }
    // There is no deferred operation for vkCopyAccelerationStructureToMemoryKHR
}

void BottomLevelAccelerationStructureKHR::deserialize(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    VkCopyMemoryToAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                          // const void* pNext;
        storage->getAddressConst(vk, device),                             // VkDeviceOrHostAddressConstKHR src;
        *(getPtr()),                                                      // VkAccelerationStructureKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR               // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyMemoryToAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else
    {
        VK_CHECK(vk.copyMemoryToAccelerationStructureKHR(device, &copyAccelerationStructureInfo));
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

uint32_t BottomLevelAccelerationStructure::getRequiredAllocationCount(void)
{
    return BottomLevelAccelerationStructureKHR::getRequiredAllocationCount();
}

void BottomLevelAccelerationStructure::createAndBuild(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                      VkDeviceAddress deviceAddress)
{
    create(vk, device, allocator, deviceAddress, 0u);
    build(vk, device, cmdBuffer);
}

void BottomLevelAccelerationStructure::createAndCopyFrom(const DeviceInterface &vk, const VkDevice device,
                                                         const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                         VkDeviceAddress deviceAddress,
                                                         BottomLevelAccelerationStructure *accelerationStructure,
                                                         VkDeviceSize compactCopySize)
{
    create(vk, device, allocator, deviceAddress, compactCopySize);
    copyFrom(vk, device, cmdBuffer, accelerationStructure, compactCopySize);
}

void BottomLevelAccelerationStructure::createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device,
                                                                const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                                VkDeviceAddress deviceAddress, SerialStorage *storage)
{
    create(vk, device, allocator, deviceAddress, 0u);
    deserialize(vk, device, cmdBuffer, storage);
}

de::MovePtr<BottomLevelAccelerationStructure> makeBottomLevelAccelerationStructure()
{
    return de::MovePtr<BottomLevelAccelerationStructure>(new BottomLevelAccelerationStructureKHR);
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure() : m_bottomLevelInstances()
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

void TopLevelAccelerationStructure::createAndBuild(const DeviceInterface &vk, const VkDevice device,
                                                   const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                   VkDeviceAddress deviceAddress)
{
    create(vk, device, allocator, deviceAddress, 0u);
    build(vk, device, cmdBuffer);
}

void TopLevelAccelerationStructure::createAndCopyFrom(const DeviceInterface &vk, const VkDevice device,
                                                      const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                      VkDeviceAddress deviceAddress,
                                                      TopLevelAccelerationStructure *accelerationStructure,
                                                      VkDeviceSize compactCopySize)
{
    create(vk, device, allocator, deviceAddress, compactCopySize);
    copyFrom(vk, device, cmdBuffer, accelerationStructure, compactCopySize);
}

void TopLevelAccelerationStructure::createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device,
                                                             const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                                             VkDeviceAddress deviceAddress, SerialStorage *storage)
{
    create(vk, device, allocator, deviceAddress, 0u);
    deserialize(vk, device, cmdBuffer, storage);
}

BufferWithMemory *createInstanceBuffer(
    const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> bottomLevelInstances,
    std::vector<InstanceData> instanceData)
{
    DE_ASSERT(bottomLevelInstances.size() != 0);
    DE_ASSERT(bottomLevelInstances.size() == instanceData.size());
    DE_UNREF(instanceData);

    const VkDeviceSize bufferSizeBytes = bottomLevelInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(
        bufferSizeBytes, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    return new BufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                MemoryRequirement::HostVisible | MemoryRequirement::Coherent |
                                    MemoryRequirement::DeviceAddress);
}

void updateInstanceBuffer(const DeviceInterface &vk, const VkDevice device,
                          std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> bottomLevelInstances,
                          std::vector<InstanceData> instanceData, BufferWithMemory *instanceBuffer,
                          VkAccelerationStructureBuildTypeKHR buildType)
{
    DE_ASSERT(bottomLevelInstances.size() != 0);
    DE_ASSERT(bottomLevelInstances.size() == instanceData.size());

    const Allocation &instancesAlloc = instanceBuffer->getAllocation();

    uint8_t *bufferStart      = static_cast<uint8_t *>(instancesAlloc.getHostPtr());
    VkDeviceSize bufferOffset = 0;

    for (size_t instanceNdx = 0; instanceNdx < bottomLevelInstances.size(); ++instanceNdx)
    {
        const BottomLevelAccelerationStructure &bottomLevelAccelerationStructure = *bottomLevelInstances[instanceNdx];
        const VkAccelerationStructureKHR accelerationStructureKHR = *bottomLevelAccelerationStructure.getPtr();

        // This part needs to be fixed once a new version of the VkAccelerationStructureInstanceKHR will be added to vkStructTypes.inl
        VkDeviceAddress accelerationStructureAddress;
        if (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        {
            VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, // VkStructureType sType;
                DE_NULL,                                                          // const void* pNext;
                accelerationStructureKHR // VkAccelerationStructureKHR accelerationStructure;
            };
            accelerationStructureAddress = vk.getAccelerationStructureDeviceAddressKHR(device, &asDeviceAddressInfo);
        }

        const uint64_t structureReference = (buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) ?
                                                uint64_t(accelerationStructureAddress) :
                                                uint64_t(&accelerationStructureKHR);

        VkAccelerationStructureInstanceKHR accelerationStructureInstanceKHR = makeVkAccelerationStructureInstanceKHR(
            instanceData[instanceNdx].matrix,              //  VkTransformMatrixKHR transform;
            instanceData[instanceNdx].instanceCustomIndex, //  uint32_t instanceCustomIndex:24;
            instanceData[instanceNdx].mask,                //  uint32_t mask:8;
            instanceData[instanceNdx]
                .instanceShaderBindingTableRecordOffset, //  uint32_t instanceShaderBindingTableRecordOffset:24;
            instanceData[instanceNdx].flags,             //  VkGeometryInstanceFlagsKHR flags:8;
            structureReference                           //  uint64_t accelerationStructureReference;
        );

        deMemcpy(&bufferStart[bufferOffset], &accelerationStructureInstanceKHR,
                 sizeof(VkAccelerationStructureInstanceKHR));

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
    void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags) override;
    void setDeferredOperation(const bool deferredOperation) override;
    void setUseArrayOfPointers(const bool useArrayOfPointers) override;
    void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                    const uint32_t indirectBufferStride) override;
    VkBuildAccelerationStructureFlagsKHR getBuildFlags() const override;

    void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, VkDeviceAddress deviceAddress,
                VkDeviceSize compactCopySize) override;
    void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer) override;
    void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                  TopLevelAccelerationStructure *accelerationStructure, VkDeviceSize compactCopySize) override;

    void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                   SerialStorage *storage) override;
    void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                     SerialStorage *storage) override;

    const VkAccelerationStructureKHR *getPtr(void) const override;

protected:
    VkAccelerationStructureBuildTypeKHR m_buildType;
    VkBuildAccelerationStructureFlagsKHR m_buildFlags;
    bool m_deferredOperation;
    bool m_useArrayOfPointers;
    de::MovePtr<BufferWithMemory> m_instanceBuffer;
    de::MovePtr<BufferWithMemory> m_instanceAddressBuffer;
    de::MovePtr<Allocation> m_accelerationStructureAlloc;
    de::MovePtr<BufferWithMemory> m_scratchBuffer;
    Move<VkAccelerationStructureKHR> m_accelerationStructureKHR;
    VkBuffer m_indirectBuffer;
    VkDeviceSize m_indirectBufferOffset;
    uint32_t m_indirectBufferStride;
};

uint32_t TopLevelAccelerationStructureKHR::getRequiredAllocationCount(void)
{
    /*
        de::MovePtr<BufferWithMemory>                            m_instanceBuffer;
        de::MovePtr<Allocation>                                    m_accelerationStructureAlloc;
        de::MovePtr<BufferWithMemory>                            m_scratchBuffer;
    */
    return 3u;
}

TopLevelAccelerationStructureKHR::TopLevelAccelerationStructureKHR()
    : TopLevelAccelerationStructure()
    , m_buildType(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    , m_buildFlags(0u)
    , m_deferredOperation(false)
    , m_useArrayOfPointers(false)
    , m_instanceBuffer()
    , m_instanceAddressBuffer()
    , m_accelerationStructureAlloc()
    , m_scratchBuffer()
    , m_accelerationStructureKHR()
    , m_indirectBuffer(DE_NULL)
    , m_indirectBufferOffset(0)
    , m_indirectBufferStride(0)
{
}

TopLevelAccelerationStructureKHR::~TopLevelAccelerationStructureKHR()
{
}

void TopLevelAccelerationStructureKHR::setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)
{
    m_buildType = buildType;
}

void TopLevelAccelerationStructureKHR::setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags)
{
    m_buildFlags = flags;
}

void TopLevelAccelerationStructureKHR::setDeferredOperation(const bool deferredOperation)
{
    m_deferredOperation = deferredOperation;
}

void TopLevelAccelerationStructureKHR::setUseArrayOfPointers(const bool useArrayOfPointers)
{
    m_useArrayOfPointers = useArrayOfPointers;
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

void TopLevelAccelerationStructureKHR::create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                              VkDeviceAddress deviceAddress, VkDeviceSize compactCopySize)
{
    DE_ASSERT(!m_bottomLevelInstances.empty() != !(compactCopySize == 0)); // logical xor

    {
        const VkAccelerationStructureCreateGeometryTypeInfoKHR accelerationStructureCreateGeometryTypeInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                                //  const void* pNext;
            VK_GEOMETRY_TYPE_INSTANCES_KHR,                                         //  VkGeometryTypeKHR geometryType;
            static_cast<uint32_t>(m_bottomLevelInstances.size()),                   //  uint32_t maxPrimitiveCount;
            VK_INDEX_TYPE_NONE_KHR,                                                 //  VkIndexType indexType;
            0u,                                                                     //  uint32_t maxVertexCount;
            VK_FORMAT_UNDEFINED,                                                    //  VkFormat vertexFormat;
            false                                                                   //  VkBool32 allowsTransforms;
        };
        const VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfoKHR = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                  //  const void* pNext;
            compactCopySize,                                          //  VkDeviceSize compactedSize;
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,             //  VkAccelerationStructureTypeKHR type;
            m_buildFlags,                                             //  VkBuildAccelerationStructureFlagsKHR flags;
            1u,                                                       //  uint32_t maxGeometryCount;
            &accelerationStructureCreateGeometryTypeInfoKHR, //  const VkAccelerationStructureCreateGeometryTypeInfoKHR* pGeometryInfos;
            deviceAddress                                    //  VkDeviceAddress deviceAddress;
        };

        m_accelerationStructureKHR =
            createAccelerationStructureKHR(vk, device, &accelerationStructureCreateInfoKHR, DE_NULL);
    }

    {
        const VkMemoryRequirements memoryRequirements = getAccelerationStructureMemoryRequirements(
            vk, device, m_accelerationStructureKHR.get(), VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR,
            m_buildType);

        m_accelerationStructureAlloc = allocator.allocate(memoryRequirements, vk::MemoryRequirement::Local);
    }

    {
        const VkBindAccelerationStructureMemoryInfoKHR bindAccelerationStructureMemoryInfoKHR = {
            VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                                       //  const void* pNext;
            m_accelerationStructureKHR.get(),          //  VkAccelerationStructureKHR accelerationStructure;
            m_accelerationStructureAlloc->getMemory(), //  VkDeviceMemory memory;
            m_accelerationStructureAlloc->getOffset(), //  VkDeviceSize memoryOffset;
            0,                                         //  uint32_t deviceIndexCount;
            DE_NULL,                                   //  const uint32_t* pDeviceIndices;
        };

        VK_CHECK(vk.bindAccelerationStructureMemoryKHR(device, 1, &bindAccelerationStructureMemoryInfoKHR));
    }

    {
        const VkMemoryRequirements memoryRequirements = getAccelerationStructureMemoryRequirements(
            vk, device, m_accelerationStructureKHR.get(),
            VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR, m_buildType);
        if (memoryRequirements.size > 0u)
        {
            const VkBufferCreateInfo bufferCreateInfo =
                makeBufferCreateInfo(memoryRequirements.size,
                                     VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            m_scratchBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, bufferCreateInfo,
                MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
        }
    }

    if (m_useArrayOfPointers)
    {
        const VkBufferCreateInfo bufferCreateInfo =
            makeBufferCreateInfo(m_bottomLevelInstances.size() * sizeof(VkDeviceOrHostAddressConstKHR),
                                 VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_instanceAddressBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, bufferCreateInfo,
            MemoryRequirement::HostVisible | MemoryRequirement::Coherent | MemoryRequirement::DeviceAddress));
    }

    if (!m_bottomLevelInstances.empty())
        m_instanceBuffer = de::MovePtr<BufferWithMemory>(
            createInstanceBuffer(vk, device, allocator, m_bottomLevelInstances, m_instanceData));
}

void TopLevelAccelerationStructureKHR::build(const DeviceInterface &vk, const VkDevice device,
                                             const VkCommandBuffer cmdBuffer)
{
    DE_ASSERT(!m_bottomLevelInstances.empty());
    DE_ASSERT(m_accelerationStructureKHR.get() != DE_NULL);

    updateInstanceBuffer(vk, device, m_bottomLevelInstances, m_instanceData, m_instanceBuffer.get(), m_buildType);

    VkDeviceOrHostAddressConstKHR instancesData;
    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        if (m_useArrayOfPointers)
        {
            uint8_t *bufferStart      = static_cast<uint8_t *>(m_instanceAddressBuffer->getAllocation().getHostPtr());
            VkDeviceSize bufferOffset = 0;
            VkDeviceOrHostAddressConstKHR firstInstance =
                makeDeviceOrHostAddressConstKHR(vk, device, m_instanceBuffer->get(), 0);
            for (size_t instanceNdx = 0; instanceNdx < m_bottomLevelInstances.size(); ++instanceNdx)
            {
                VkDeviceOrHostAddressConstKHR currentInstance;
                currentInstance.deviceAddress =
                    firstInstance.deviceAddress + instanceNdx * sizeof(VkAccelerationStructureInstanceKHR);

                deMemcpy(&bufferStart[bufferOffset], &currentInstance, sizeof(VkDeviceOrHostAddressConstKHR));
                bufferOffset += sizeof(VkDeviceOrHostAddressConstKHR);
            }
            flushMappedMemoryRange(vk, device, m_instanceAddressBuffer->getAllocation().getMemory(),
                                   m_instanceAddressBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

            instancesData = makeDeviceOrHostAddressConstKHR(vk, device, m_instanceAddressBuffer->get(), 0);
        }
        else
            instancesData = makeDeviceOrHostAddressConstKHR(vk, device, m_instanceBuffer->get(), 0);
    }
    else
    {
        if (m_useArrayOfPointers)
        {
            uint8_t *bufferStart      = static_cast<uint8_t *>(m_instanceAddressBuffer->getAllocation().getHostPtr());
            VkDeviceSize bufferOffset = 0;
            for (size_t instanceNdx = 0; instanceNdx < m_bottomLevelInstances.size(); ++instanceNdx)
            {
                VkDeviceOrHostAddressConstKHR currentInstance;
                currentInstance.hostAddress = (uint8_t *)m_instanceBuffer->getAllocation().getHostPtr() +
                                              instanceNdx * sizeof(VkAccelerationStructureInstanceKHR);

                deMemcpy(&bufferStart[bufferOffset], &currentInstance, sizeof(VkDeviceOrHostAddressConstKHR));
                bufferOffset += sizeof(VkDeviceOrHostAddressConstKHR);
            }
            instancesData = makeDeviceOrHostAddressConstKHR(m_instanceAddressBuffer->getAllocation().getHostPtr());
        }
        else
            instancesData = makeDeviceOrHostAddressConstKHR(m_instanceBuffer->getAllocation().getHostPtr());
    }

    VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesDataKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, //  VkStructureType sType;
        DE_NULL,                                                              //  const void* pNext;
        (VkBool32)(m_useArrayOfPointers ? true : false),                      //  VkBool32 arrayOfPointers;
        instancesData                                                         //  VkDeviceOrHostAddressConstKHR data;
    };

    VkAccelerationStructureGeometryKHR accelerationStructureGeometryKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, //  VkStructureType sType;
        DE_NULL,                                               //  const void* pNext;
        VK_GEOMETRY_TYPE_INSTANCES_KHR,                        //  VkGeometryTypeKHR geometryType;
        makeVkAccelerationStructureInstancesDataKHR(
            accelerationStructureGeometryInstancesDataKHR), //  VkAccelerationStructureGeometryDataKHR geometry;
        (VkGeometryFlagsKHR)0u                              //  VkGeometryFlagsKHR flags;
    };
    VkAccelerationStructureGeometryKHR *accelerationStructureGeometryKHRPointer = &accelerationStructureGeometryKHR;

    VkDeviceOrHostAddressKHR scratchData;
    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
        scratchData = makeDeviceOrHostAddressKHR(vk, device, m_scratchBuffer->get(), 0);
    else
        scratchData = makeDeviceOrHostAddressKHR(m_scratchBuffer->getAllocation().getHostPtr());

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfoKHR = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, //  VkStructureType sType;
        DE_NULL,                                                          //  const void* pNext;
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,                     //  VkAccelerationStructureTypeKHR type;
        m_buildFlags,                     //  VkBuildAccelerationStructureFlagsKHR flags;
        false,                            //  VkBool32 update;
        DE_NULL,                          //  VkAccelerationStructureKHR srcAccelerationStructure;
        m_accelerationStructureKHR.get(), //  VkAccelerationStructureKHR dstAccelerationStructure;
        false,                            //  VkBool32 geometryArrayOfPointers;
        1u,                               //  uint32_t geometryCount;
        (const VkAccelerationStructureGeometryKHR *
             *)&accelerationStructureGeometryKHRPointer, //  const VkAccelerationStructureGeometryKHR** ppGeometries;
        scratchData                                      //  VkDeviceOrHostAddressKHR scratchData;
    };

    VkAccelerationStructureBuildOffsetInfoKHR accelerationStructureBuildOffsetInfoKHR = {
        (uint32_t)m_bottomLevelInstances.size(), //  uint32_t primitiveCount;
        0,                                       //  uint32_t primitiveOffset;
        0,                                       //  uint32_t firstVertex;
        0                                        //  uint32_t firstTransform;
    };
    VkAccelerationStructureBuildOffsetInfoKHR *accelerationStructureBuildOffsetInfoKHRPtr =
        &accelerationStructureBuildOffsetInfoKHR;

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        if (m_indirectBuffer == DE_NULL)
            vk.cmdBuildAccelerationStructureKHR(
                cmdBuffer, 1u, &accelerationStructureBuildGeometryInfoKHR,
                (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr);
        else
            vk.cmdBuildAccelerationStructureIndirectKHR(cmdBuffer, &accelerationStructureBuildGeometryInfoKHR,
                                                        m_indirectBuffer, m_indirectBufferOffset,
                                                        m_indirectBufferStride);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.buildAccelerationStructureKHR(
            device, 1u, &accelerationStructureBuildGeometryInfoKHR,
            (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr));
    }
    else
    {
        VkDeferredOperationKHR deferredOperation = DE_NULL;

        VK_CHECK(vk.createDeferredOperationKHR(device, DE_NULL, &deferredOperation));

        VkDeferredOperationInfoKHR deferredOperationInfoKHR = {
            VK_STRUCTURE_TYPE_DEFERRED_OPERATION_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                       //  const void* pNext;
            deferredOperation                              //  VkDeferredOperationKHR operationHandle;
        };

        accelerationStructureBuildGeometryInfoKHR.pNext = &deferredOperationInfoKHR;

        VkResult result = vk.buildAccelerationStructureKHR(
            device, 1u, &accelerationStructureBuildGeometryInfoKHR,
            (const VkAccelerationStructureBuildOffsetInfoKHR **)&accelerationStructureBuildOffsetInfoKHRPtr);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);
        DE_UNREF(result);

        VK_CHECK(finishDeferredOperation(vk, device, deferredOperation));

        accelerationStructureBuildGeometryInfoKHR.pNext = DE_NULL;
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
                                                TopLevelAccelerationStructure *accelerationStructure,
                                                VkDeviceSize compactCopySize)
{
    VkCopyAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                // const void* pNext;
        *(accelerationStructure->getPtr()),                     // VkAccelerationStructureKHR src;
        *(getPtr()),                                            // VkAccelerationStructureKHR dst;
        compactCopySize > 0u ? VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR :
                               VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else if (!m_deferredOperation)
    {
        VK_CHECK(vk.copyAccelerationStructureKHR(device, &copyAccelerationStructureInfo));
    }
    else
    {
        VkDeferredOperationKHR deferredOperation = DE_NULL;

        VK_CHECK(vk.createDeferredOperationKHR(device, DE_NULL, &deferredOperation));

        VkDeferredOperationInfoKHR deferredOperationInfoKHR = {
            VK_STRUCTURE_TYPE_DEFERRED_OPERATION_INFO_KHR, //  VkStructureType sType;
            DE_NULL,                                       //  const void* pNext;
            deferredOperation                              //  VkDeferredOperationKHR operationHandle;
        };

        copyAccelerationStructureInfo.pNext = &deferredOperationInfoKHR;

        VkResult result = vk.copyAccelerationStructureKHR(device, &copyAccelerationStructureInfo);

        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);
        DE_UNREF(result);

        VK_CHECK(finishDeferredOperation(vk, device, deferredOperation));
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
    VkCopyAccelerationStructureToMemoryInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                          // const void* pNext;
        *(getPtr()),                                                      // VkAccelerationStructureKHR src;
        storage->getAddress(vk, device),                                  // VkDeviceOrHostAddressKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR                 // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyAccelerationStructureToMemoryKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else
    {
        VK_CHECK(vk.copyAccelerationStructureToMemoryKHR(device, &copyAccelerationStructureInfo));
    }
    // There is no deferred operation for vkCopyAccelerationStructureToMemoryKHR
}

void TopLevelAccelerationStructureKHR::deserialize(const DeviceInterface &vk, const VkDevice device,
                                                   const VkCommandBuffer cmdBuffer, SerialStorage *storage)
{
    VkCopyMemoryToAccelerationStructureInfoKHR copyAccelerationStructureInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                          // const void* pNext;
        storage->getAddressConst(vk, device),                             // VkDeviceOrHostAddressConstKHR src;
        *(getPtr()),                                                      // VkAccelerationStructureKHR dst;
        VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR               // VkCopyAccelerationStructureModeKHR mode;
    };

    if (m_buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
    {
        vk.cmdCopyMemoryToAccelerationStructureKHR(cmdBuffer, &copyAccelerationStructureInfo);
    }
    else
    {
        VK_CHECK(vk.copyMemoryToAccelerationStructureKHR(device, &copyAccelerationStructureInfo));
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

const VkAccelerationStructureKHR *TopLevelAccelerationStructureKHR::getPtr(void) const
{
    return &m_accelerationStructureKHR.get();
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
    vk.writeAccelerationStructuresPropertiesKHR(device, uint32_t(accelerationStructureHandles.size()),
                                                accelerationStructureHandles.data(), queryType, sizeof(VkDeviceSize),
                                                results.data(), sizeof(VkDeviceSize));
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
    , m_maxRecursionDepth(1U)
    , m_maxPayloadSize(0U)
    , m_maxAttributeSize(0U)
    , m_maxCallableSize(0U)
    , m_deferredOperation(false)
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

void RayTracingPipeline::addShader(VkShaderStageFlagBits shaderStage, Move<VkShaderModule> shaderModule, uint32_t group)
{
    if (group >= m_shadersGroupCreateInfos.size())
    {
        for (size_t groupNdx = m_shadersGroupCreateInfos.size(); groupNdx <= group; ++groupNdx)
        {
            VkRayTracingShaderGroupCreateInfoKHR shaderGroupCreateInfo = {
                VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, //  VkStructureType sType;
                DE_NULL,                                                    //  const void* pNext;
                VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR,              //  VkRayTracingShaderGroupTypeKHR type;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t generalShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t closestHitShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t anyHitShader;
                VK_SHADER_UNUSED_KHR,                                       //  uint32_t intersectionShader;
                DE_NULL, //  const void* pShaderGroupCaptureReplayHandle;
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
            DE_NULL,                                             //  const void* pNext;
            (VkPipelineShaderStageCreateFlags)0,                 //  VkPipelineShaderStageCreateFlags flags;
            shaderStage,                                         //  VkShaderStageFlagBits stage;
            *shaderModule,                                       //  VkShaderModule module;
            "main",                                              //  const char* pName;
            DE_NULL,                                             //  const VkSpecializationInfo* pSpecializationInfo;
        };

        m_shaderCreateInfos.push_back(shaderCreateInfo);
    }

    m_shadersModules.push_back(makeVkSharedPtr(shaderModule));
}

void RayTracingPipeline::addLibrary(de::SharedPtr<de::MovePtr<RayTracingPipeline>> pipelineLibrary)
{
    m_pipelineLibraries.push_back(pipelineLibrary);
}

Move<VkPipeline> RayTracingPipeline::createPipelineKHR(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries)
{
    for (size_t groupNdx = 0; groupNdx < m_shadersGroupCreateInfos.size(); ++groupNdx)
        DE_ASSERT(m_shadersGroupCreateInfos[groupNdx].sType ==
                  VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);

    DE_ASSERT(m_shaderCreateInfos.size() > 0);
    DE_ASSERT(m_shadersGroupCreateInfos.size() > 0);

    std::vector<VkPipeline> vkPipelineLibraries;
    for (auto it = begin(pipelineLibraries), eit = end(pipelineLibraries); it != eit; ++it)
        vkPipelineLibraries.push_back(it->get()->get());
    const VkPipelineLibraryCreateInfoKHR librariesCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, //  VkStructureType sType;
        DE_NULL,                                            //  const void* pNext;
        uint32_t(vkPipelineLibraries.size()),               //  uint32_t libraryCount;
        dataOrNullPtr(vkPipelineLibraries)                  //  VkPipeline* pLibraries;
    };
    const VkRayTracingPipelineInterfaceCreateInfoKHR pipelineInterfaceCreateInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR, //  VkStructureType sType;
        DE_NULL,                                                          //  const void* pNext;
        m_maxPayloadSize,                                                 //  uint32_t maxPayloadSize;
        m_maxAttributeSize,                                               //  uint32_t maxAttributeSize;
        m_maxCallableSize                                                 //  uint32_t maxCallableSize;
    };
    const bool addPipelineInterfaceCreateInfo =
        m_maxPayloadSize != 0 || m_maxAttributeSize != 0 || m_maxCallableSize != 0;
    const VkRayTracingPipelineInterfaceCreateInfoKHR *pipelineInterfaceCreateInfoPtr =
        addPipelineInterfaceCreateInfo ? &pipelineInterfaceCreateInfo : DE_NULL;
    Move<VkDeferredOperationKHR> deferredOperation =
        (m_deferredOperation ? createDeferredOperationKHR(vk, device) : Move<VkDeferredOperationKHR>());
    VkDeferredOperationInfoKHR deferredOperationInfoKHR = {
        VK_STRUCTURE_TYPE_DEFERRED_OPERATION_INFO_KHR, //  VkStructureType sType;
        DE_NULL,                                       //  const void* pNext;
        *deferredOperation                             //  VkDeferredOperationKHR operationHandle;
    };
    const VkDeferredOperationInfoKHR *deferredOperationInfoPtr =
        m_deferredOperation ? &deferredOperationInfoKHR : DE_NULL;
    const VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, //  VkStructureType sType;
        deferredOperationInfoPtr,                               //  const void* pNext;
        m_pipelineCreateFlags,                                  //  VkPipelineCreateFlags flags;
        (uint32_t)m_shaderCreateInfos.size(),                   //  uint32_t stageCount;
        m_shaderCreateInfos.data(),                             //  const VkPipelineShaderStageCreateInfo* pStages;
        (uint32_t)m_shadersGroupCreateInfos.size(),             //  uint32_t groupCount;
        m_shadersGroupCreateInfos.data(),                       //  const VkRayTracingShaderGroupCreateInfoKHR* pGroups;
        m_maxRecursionDepth,                                    //  uint32_t maxRecursionDepth;
        librariesCreateInfo,                                    //  VkPipelineLibraryCreateInfoKHR libraries;
        pipelineInterfaceCreateInfoPtr, //  VkRayTracingPipelineInterfaceCreateInfoKHR* pLibraryInterface;
        pipelineLayout,                 //  VkPipelineLayout layout;
        (VkPipeline)DE_NULL,            //  VkPipeline basePipelineHandle;
        0,                              //  int32_t basePipelineIndex;
    };
    VkPipeline object = DE_NULL;
    VkResult result   = vk.createRayTracingPipelinesKHR(device, DE_NULL, 1u, &pipelineCreateInfo, DE_NULL, &object);
    Move<VkPipeline> pipeline(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, DE_NULL));

    if (m_deferredOperation)
    {
        DE_ASSERT(result == VK_OPERATION_DEFERRED_KHR || result == VK_OPERATION_NOT_DEFERRED_KHR ||
                  result == VK_SUCCESS);
        DE_UNREF(result);

        VK_CHECK(finishDeferredOperation(vk, device, *deferredOperation));
    }

    return pipeline;
}

Move<VkPipeline> RayTracingPipeline::createPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries)
{
    return createPipelineKHR(vk, device, pipelineLayout, pipelineLibraries);
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

de::MovePtr<BufferWithMemory> RayTracingPipeline::createShaderBindingTable(
    const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline, Allocator &allocator,
    const uint32_t &shaderGroupHandleSize, const uint32_t shaderGroupBaseAlignment, const uint32_t &firstGroup,
    const uint32_t &groupCount, const VkBufferCreateFlags &additionalBufferCreateFlags,
    const VkBufferUsageFlags &additionalBufferUsageFlags, const MemoryRequirement &additionalMemoryRequirement,
    const VkDeviceAddress &opaqueCaptureAddress, const uint32_t shaderBindingTableOffset,
    const uint32_t shaderRecordSize)
{
    DE_ASSERT(shaderGroupBaseAlignment != 0u);
    DE_ASSERT((shaderBindingTableOffset % shaderGroupBaseAlignment) == 0);
    DE_UNREF(shaderGroupBaseAlignment);

    const uint32_t sbtSize = shaderBindingTableOffset +
                             groupCount * deAlign32(shaderGroupHandleSize + shaderRecordSize, shaderGroupHandleSize);
    const VkBufferUsageFlags sbtFlags =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | additionalBufferUsageFlags;
    VkBufferCreateInfo sbtCreateInfo = makeBufferCreateInfo(sbtSize, sbtFlags);
    sbtCreateInfo.flags |= additionalBufferCreateFlags;
    VkBufferOpaqueCaptureAddressCreateInfo sbtCaptureAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                     // const void* pNext;
        uint64_t(opaqueCaptureAddress)                               // uint64_t opaqueCaptureAddress;
    };
    if (opaqueCaptureAddress != 0u)
        sbtCreateInfo.pNext = &sbtCaptureAddressInfo;
    const MemoryRequirement sbtMemRequirements =
        MemoryRequirement::HostVisible | MemoryRequirement::Coherent | additionalMemoryRequirement;
    de::MovePtr<BufferWithMemory> sbtBuffer =
        de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, sbtCreateInfo, sbtMemRequirements));
    vk::Allocation &sbtAlloc = sbtBuffer->getAllocation();

    // collect shader group handles
    std::vector<uint8_t> shaderHandles(groupCount * shaderGroupHandleSize);
    VK_CHECK(getRayTracingShaderGroupHandles(vk, device, pipeline, firstGroup, groupCount,
                                             groupCount * shaderGroupHandleSize, shaderHandles.data()));

    // reserve place for ShaderRecordKHR after each shader handle ( ShaderRecordKHR size might be 0 ). Also take alignment into consideration
    uint8_t *shaderBegin = (uint8_t *)sbtAlloc.getHostPtr() + shaderBindingTableOffset;
    for (uint32_t idx = 0; idx < groupCount; ++idx)
    {
        uint8_t *shaderSrcPos = shaderHandles.data() + idx * shaderGroupHandleSize;
        uint8_t *shaderDstPos =
            shaderBegin + idx * deAlign32(shaderGroupHandleSize + shaderRecordSize, shaderGroupHandleSize);
        deMemcpy(shaderDstPos, shaderSrcPos, shaderGroupHandleSize);
    }

    flushMappedMemoryRange(vk, device, sbtAlloc.getMemory(), sbtAlloc.getOffset(), VK_WHOLE_SIZE);

    return sbtBuffer;
}

void RayTracingPipeline::setCreateFlags(const VkPipelineCreateFlags &pipelineCreateFlags)
{
    m_pipelineCreateFlags = pipelineCreateFlags;
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

void RayTracingPipeline::setMaxCallableSize(const uint32_t &maxCallableSize)
{
    m_maxCallableSize = maxCallableSize;
}

void RayTracingPipeline::setDeferredOperation(const bool deferredOperation)
{
    m_deferredOperation = deferredOperation;
}

class RayTracingPropertiesKHR : public RayTracingProperties
{
public:
    RayTracingPropertiesKHR() = delete;
    RayTracingPropertiesKHR(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice);
    virtual ~RayTracingPropertiesKHR();

    virtual uint32_t getShaderGroupHandleSize(void)
    {
        return m_rayTracingPropertiesKHR.shaderGroupHandleSize;
    };
    virtual uint32_t getMaxRecursionDepth(void)
    {
        return m_rayTracingPropertiesKHR.maxRecursionDepth;
    };
    virtual uint32_t getMaxShaderGroupStride(void)
    {
        return m_rayTracingPropertiesKHR.maxShaderGroupStride;
    };
    virtual uint32_t getShaderGroupBaseAlignment(void)
    {
        return m_rayTracingPropertiesKHR.shaderGroupBaseAlignment;
    };
    virtual uint64_t getMaxGeometryCount(void)
    {
        return m_rayTracingPropertiesKHR.maxGeometryCount;
    };
    virtual uint64_t getMaxInstanceCount(void)
    {
        return m_rayTracingPropertiesKHR.maxInstanceCount;
    };
    virtual uint64_t getMaxPrimitiveCount(void)
    {
        return m_rayTracingPropertiesKHR.maxPrimitiveCount;
    };
    virtual uint32_t getMaxDescriptorSetAccelerationStructures(void)
    {
        return m_rayTracingPropertiesKHR.maxDescriptorSetAccelerationStructures;
    };

protected:
    VkPhysicalDeviceRayTracingPropertiesKHR m_rayTracingPropertiesKHR;
};

RayTracingPropertiesKHR::~RayTracingPropertiesKHR()
{
}

RayTracingPropertiesKHR::RayTracingPropertiesKHR(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
    : RayTracingProperties(vki, physicalDevice)
{
    m_rayTracingPropertiesKHR = getPhysicalDeviceExtensionProperties(vki, physicalDevice);
}

de::MovePtr<RayTracingProperties> makeRayTracingProperties(const InstanceInterface &vki,
                                                           const VkPhysicalDevice physicalDevice)
{
    return de::MovePtr<RayTracingProperties>(new RayTracingPropertiesKHR(vki, physicalDevice));
}

static inline void cmdTraceRaysKHR(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                   const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                                   const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                                   const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                                   const VkStridedBufferRegionKHR *callableShaderBindingTableRegion, uint32_t width,
                                   uint32_t height, uint32_t depth)
{
    return vk.cmdTraceRaysKHR(commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                              hitShaderBindingTableRegion, callableShaderBindingTableRegion, width, height, depth);
}

void cmdTraceRays(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                  const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *callableShaderBindingTableRegion, uint32_t width, uint32_t height,
                  uint32_t depth)
{
    DE_ASSERT(raygenShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(missShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(hitShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(callableShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(width > 0);
    DE_ASSERT(height > 0);
    DE_ASSERT(depth > 0);

    return cmdTraceRaysKHR(vk, commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                           hitShaderBindingTableRegion, callableShaderBindingTableRegion, width, height, depth);
}

static inline void cmdTraceRaysIndirectKHR(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                                           const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                                           const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                                           const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                                           const VkStridedBufferRegionKHR *callableShaderBindingTableRegion,
                                           VkBuffer buffer, VkDeviceSize offset)
{
    DE_ASSERT(raygenShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(missShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(hitShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(callableShaderBindingTableRegion != DE_NULL);
    DE_ASSERT(buffer != DE_NULL);

    return vk.cmdTraceRaysIndirectKHR(commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                                      hitShaderBindingTableRegion, callableShaderBindingTableRegion, buffer, offset);
}

void cmdTraceRaysIndirect(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                          const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *callableShaderBindingTableRegion, VkBuffer buffer,
                          VkDeviceSize offset)
{
    return cmdTraceRaysIndirectKHR(vk, commandBuffer, raygenShaderBindingTableRegion, missShaderBindingTableRegion,
                                   hitShaderBindingTableRegion, callableShaderBindingTableRegion, buffer, offset);
}

} // namespace vk
