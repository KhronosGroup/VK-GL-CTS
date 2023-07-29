#ifndef _VKRAYTRACINGUTIL_HPP
#define _VKRAYTRACINGUTIL_HPP
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
 * \brief Vulkan ray tracing utility.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "deFloat16.h"

#include "tcuVector.hpp"
#include "tcuVectorType.hpp"

#include <vector>
#include <limits>

namespace vk
{
const VkTransformMatrixKHR identityMatrix3x4 = {
    {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};

template <typename T>
inline de::SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
    return de::SharedPtr<Move<T>>(new Move<T>(move));
}

template <typename T>
inline de::SharedPtr<de::MovePtr<T>> makeVkSharedPtr(de::MovePtr<T> movePtr)
{
    return de::SharedPtr<de::MovePtr<T>>(new de::MovePtr<T>(movePtr));
}

template <typename T>
inline const T *dataOrNullPtr(const std::vector<T> &v)
{
    return (v.empty() ? DE_NULL : v.data());
}

template <typename T>
inline T *dataOrNullPtr(std::vector<T> &v)
{
    return (v.empty() ? DE_NULL : v.data());
}

inline std::string updateRayTracingGLSL(const std::string &str)
{
    return str;
}

std::string getCommonRayGenerationShader(void);

const char *getRayTracingExtensionUsed(void);

class RaytracedGeometryBase
{
public:
    RaytracedGeometryBase()                                      = delete;
    RaytracedGeometryBase(const RaytracedGeometryBase &geometry) = delete;
    RaytracedGeometryBase(VkGeometryTypeKHR geometryType, VkFormat vertexFormat, VkIndexType indexType);
    virtual ~RaytracedGeometryBase();

    inline VkGeometryTypeKHR getGeometryType(void) const
    {
        return m_geometryType;
    }
    inline bool isTrianglesType(void) const
    {
        return m_geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    }
    inline VkFormat getVertexFormat(void) const
    {
        return m_vertexFormat;
    }
    inline VkIndexType getIndexType(void) const
    {
        return m_indexType;
    }
    inline bool usesIndices(void) const
    {
        return m_indexType != VK_INDEX_TYPE_NONE_KHR;
    }
    inline VkGeometryFlagsKHR getGeometryFlags(void) const
    {
        return m_geometryFlags;
    }
    inline void setGeometryFlags(const VkGeometryFlagsKHR geometryFlags)
    {
        m_geometryFlags = geometryFlags;
    }
    virtual uint32_t getVertexCount(void) const         = 0;
    virtual const uint8_t *getVertexPointer(void) const = 0;
    virtual VkDeviceSize getVertexStride(void) const    = 0;
    virtual size_t getVertexByteSize(void) const        = 0;
    virtual VkDeviceSize getAABBStride(void) const      = 0;
    virtual uint32_t getIndexCount(void) const          = 0;
    virtual const uint8_t *getIndexPointer(void) const  = 0;
    virtual VkDeviceSize getIndexStride(void) const     = 0;
    virtual size_t getIndexByteSize(void) const         = 0;
    virtual uint32_t getPrimitiveCount(void) const      = 0;
    virtual void addVertex(const tcu::Vec3 &vertex)     = 0;
    virtual void addIndex(const uint32_t &index)        = 0;

private:
    VkGeometryTypeKHR m_geometryType;
    VkFormat m_vertexFormat;
    VkIndexType m_indexType;
    VkGeometryFlagsKHR m_geometryFlags;
};

template <typename T>
inline T convertSatRte(float f)
{
    // \note Doesn't work for 64-bit types
    DE_STATIC_ASSERT(sizeof(T) < sizeof(uint64_t));
    DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

    int64_t minVal = std::numeric_limits<T>::min();
    int64_t maxVal = std::numeric_limits<T>::max();
    float q        = deFloatFrac(f);
    int64_t intVal = (int64_t)(f - q);

    // Rounding.
    if (q == 0.5f)
    {
        if (intVal % 2 != 0)
            intVal++;
    }
    else if (q > 0.5f)
        intVal++;
    // else Don't add anything

    // Saturate.
    intVal = de::max(minVal, de::min(maxVal, intVal));

    return (T)intVal;
}

inline int16_t deFloat32ToSNorm16(float src)
{
    const int16_t range  = (int32_t)((1u << 15) - 1u);
    const int16_t intVal = convertSatRte<int16_t>(src * (float)range);
    return de::clamp<int16_t>(intVal, -range, range);
}

typedef tcu::Vector<deFloat16, 2> Vec2_16;
typedef tcu::Vector<deFloat16, 4> Vec4_16;
typedef tcu::Vector<int16_t, 2> Vec2_16SNorm;
typedef tcu::Vector<int16_t, 4> Vec4_16SNorm;

template <typename V>
VkFormat vertexFormatFromType()
{
    TCU_THROW(TestError, "Unknown VkFormat");
}
template <>
inline VkFormat vertexFormatFromType<tcu::Vec2>()
{
    return VK_FORMAT_R32G32_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<tcu::Vec3>()
{
    return VK_FORMAT_R32G32B32_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec2_16>()
{
    return VK_FORMAT_R16G16_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec4_16>()
{
    return VK_FORMAT_R16G16B16A16_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec2_16SNorm>()
{
    return VK_FORMAT_R16G16_SNORM;
}
template <>
inline VkFormat vertexFormatFromType<Vec4_16SNorm>()
{
    return VK_FORMAT_R16G16B16A16_SNORM;
}

struct EmptyIndex
{
};
template <typename I>
VkIndexType indexTypeFromType()
{
    TCU_THROW(TestError, "Unknown VkIndexType");
}
template <>
inline VkIndexType indexTypeFromType<uint16_t>()
{
    return VK_INDEX_TYPE_UINT16;
}
template <>
inline VkIndexType indexTypeFromType<uint32_t>()
{
    return VK_INDEX_TYPE_UINT32;
}
template <>
inline VkIndexType indexTypeFromType<EmptyIndex>()
{
    return VK_INDEX_TYPE_NONE_KHR;
}

template <typename V>
V convertFloatTo(const tcu::Vec3 &vertex)
{
    DE_UNREF(vertex);
    TCU_THROW(TestError, "Unknown data format");
}
template <>
inline tcu::Vec2 convertFloatTo<tcu::Vec2>(const tcu::Vec3 &vertex)
{
    return tcu::Vec2(vertex.x(), vertex.y());
}
template <>
inline tcu::Vec3 convertFloatTo<tcu::Vec3>(const tcu::Vec3 &vertex)
{
    return vertex;
}
template <>
inline Vec2_16 convertFloatTo<Vec2_16>(const tcu::Vec3 &vertex)
{
    return Vec2_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y()));
}
template <>
inline Vec4_16 convertFloatTo<Vec4_16>(const tcu::Vec3 &vertex)
{
    return Vec4_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y()), deFloat32To16(vertex.z()),
                   deFloat32To16(0.0f));
}
template <>
inline Vec2_16SNorm convertFloatTo<Vec2_16SNorm>(const tcu::Vec3 &vertex)
{
    return Vec2_16SNorm(deFloat32ToSNorm16(vertex.x()), deFloat32ToSNorm16(vertex.y()));
}
template <>
inline Vec4_16SNorm convertFloatTo<Vec4_16SNorm>(const tcu::Vec3 &vertex)
{
    return Vec4_16SNorm(deFloat32ToSNorm16(vertex.x()), deFloat32ToSNorm16(vertex.y()), deFloat32ToSNorm16(vertex.z()),
                        deFloat32ToSNorm16(0.0f));
}

template <typename V>
V convertIndexTo(uint32_t index)
{
    DE_UNREF(index);
    TCU_THROW(TestError, "Unknown index format");
}
template <>
inline EmptyIndex convertIndexTo<EmptyIndex>(uint32_t index)
{
    DE_UNREF(index);
    TCU_THROW(TestError, "Cannot add empty index");
}
template <>
inline uint16_t convertIndexTo<uint16_t>(uint32_t index)
{
    return static_cast<uint16_t>(index);
}
template <>
inline uint32_t convertIndexTo<uint32_t>(uint32_t index)
{
    return index;
}

template <typename V, typename I>
class RaytracedGeometry : public RaytracedGeometryBase
{
public:
    RaytracedGeometry()                                  = delete;
    RaytracedGeometry(const RaytracedGeometry &geometry) = delete;
    RaytracedGeometry(VkGeometryTypeKHR geometryType);
    RaytracedGeometry(VkGeometryTypeKHR geometryType, const std::vector<V> &vertices,
                      const std::vector<I> &indices = std::vector<I>());

    uint32_t getVertexCount(void) const override;
    const uint8_t *getVertexPointer(void) const override;
    VkDeviceSize getVertexStride(void) const override;
    size_t getVertexByteSize(void) const override;
    VkDeviceSize getAABBStride(void) const override;
    uint32_t getIndexCount(void) const override;
    const uint8_t *getIndexPointer(void) const override;
    VkDeviceSize getIndexStride(void) const override;
    size_t getIndexByteSize(void) const override;
    uint32_t getPrimitiveCount(void) const override;

    void addVertex(const tcu::Vec3 &vertex) override;
    void addIndex(const uint32_t &index) override;

private:
    std::vector<V> m_vertices;
    std::vector<I> m_indices;
};

template <typename V, typename I>
RaytracedGeometry<V, I>::RaytracedGeometry(VkGeometryTypeKHR geometryType)
    : RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
{
}

template <typename V, typename I>
RaytracedGeometry<V, I>::RaytracedGeometry(VkGeometryTypeKHR geometryType, const std::vector<V> &vertices,
                                           const std::vector<I> &indices)
    : RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
    , m_vertices(vertices)
    , m_indices(indices)
{
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getVertexCount(void) const
{
    return static_cast<uint32_t>(isTrianglesType() ? m_vertices.size() : 0);
}

template <typename V, typename I>
const uint8_t *RaytracedGeometry<V, I>::getVertexPointer(void) const
{
    return reinterpret_cast<const uint8_t *>(m_vertices.empty() ? DE_NULL : m_vertices.data());
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getVertexStride(void) const
{
    return static_cast<VkDeviceSize>(sizeof(V));
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getAABBStride(void) const
{
    return static_cast<VkDeviceSize>(2 * sizeof(V));
}

template <typename V, typename I>
size_t RaytracedGeometry<V, I>::getVertexByteSize(void) const
{
    return static_cast<size_t>(m_vertices.size() * sizeof(V));
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getIndexCount(void) const
{
    return static_cast<uint32_t>(isTrianglesType() ? m_indices.size() : 0);
}

template <typename V, typename I>
const uint8_t *RaytracedGeometry<V, I>::getIndexPointer(void) const
{
    return reinterpret_cast<const uint8_t *>(m_indices.empty() ? DE_NULL : m_indices.data());
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getIndexStride(void) const
{
    return static_cast<VkDeviceSize>(sizeof(I));
}

template <typename V, typename I>
size_t RaytracedGeometry<V, I>::getIndexByteSize(void) const
{
    return static_cast<size_t>(m_indices.size() * sizeof(I));
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getPrimitiveCount(void) const
{
    return static_cast<uint32_t>(isTrianglesType() ? (usesIndices() ? m_indices.size() / 3 : m_vertices.size() / 3) :
                                                     (m_vertices.size() / 2));
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::addVertex(const tcu::Vec3 &vertex)
{
    m_vertices.push_back(convertFloatTo<V>(vertex));
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::addIndex(const uint32_t &index)
{
    m_indices.push_back(convertIndexTo<I>(index));
}

de::SharedPtr<RaytracedGeometryBase> makeRaytracedGeometry(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                                           VkIndexType indexType);

class SerialStorage
{
public:
    SerialStorage() = delete;
    SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                  const VkAccelerationStructureBuildTypeKHR buildType, const VkDeviceSize storageSize);

    VkDeviceOrHostAddressKHR getAddress(const DeviceInterface &vk, const VkDevice device);
    VkDeviceOrHostAddressConstKHR getAddressConst(const DeviceInterface &vk, const VkDevice device);

protected:
    VkAccelerationStructureBuildTypeKHR m_buildType;
    de::MovePtr<BufferWithMemory> m_buffer;
};

class BottomLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other) = delete;
    virtual ~BottomLevelAccelerationStructure();

    virtual void setGeometryData(const std::vector<tcu::Vec3> &geometryData, const bool triangles,
                                 const VkGeometryFlagsKHR geometryFlags = 0);
    virtual void setDefaultGeometryData(const VkShaderStageFlagBits testStage);
    virtual void setGeometryCount(const size_t geometryCount);
    virtual void addGeometry(de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry);
    virtual void addGeometry(const std::vector<tcu::Vec3> &geometryData, const bool triangles,
                             const VkGeometryFlagsKHR geometryFlags = 0);

    virtual void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType) = DE_NULL;
    virtual void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags)   = DE_NULL;
    virtual void setDeferredOperation(const bool deferredOperation)                = DE_NULL;
    virtual void setUseArrayOfPointers(const bool useArrayOfPointers)              = DE_NULL;
    virtual void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                            const uint32_t indirectBufferStride)   = DE_NULL;
    virtual VkBuildAccelerationStructureFlagsKHR getBuildFlags() const             = DE_NULL;

    // methods specific for each acceleration structure
    virtual void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                        VkDeviceAddress deviceAddress, VkDeviceSize compactCopySize)                      = DE_NULL;
    virtual void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer) = DE_NULL;
    virtual void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                          BottomLevelAccelerationStructure *accelerationStructure,
                          VkDeviceSize compactCopySize)                                                   = DE_NULL;

    virtual void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           SerialStorage *storage)   = DE_NULL;
    virtual void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                             SerialStorage *storage) = DE_NULL;

    // helper methods for typical acceleration structure creation tasks
    void createAndBuild(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                        Allocator &allocator, VkDeviceAddress deviceAddress = 0u);
    void createAndCopyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           Allocator &allocator, VkDeviceAddress deviceAddress = 0u,
                           BottomLevelAccelerationStructure *accelerationStructure = DE_NULL,
                           VkDeviceSize compactCopySize                            = 0u);
    void createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                  Allocator &allocator, VkDeviceAddress deviceAddress = 0u,
                                  SerialStorage *storage = DE_NULL);

    virtual const VkAccelerationStructureKHR *getPtr(void) const = DE_NULL;

protected:
    std::vector<de::SharedPtr<RaytracedGeometryBase>> m_geometriesData;
};

de::MovePtr<BottomLevelAccelerationStructure> makeBottomLevelAccelerationStructure();

struct InstanceData
{
    InstanceData(VkTransformMatrixKHR matrix_, uint32_t instanceCustomIndex_, uint32_t mask_,
                 uint32_t instanceShaderBindingTableRecordOffset_, VkGeometryInstanceFlagsKHR flags_)
        : matrix(matrix_)
        , instanceCustomIndex(instanceCustomIndex_)
        , mask(mask_)
        , instanceShaderBindingTableRecordOffset(instanceShaderBindingTableRecordOffset_)
        , flags(flags_)
    {
    }
    VkTransformMatrixKHR matrix;
    uint32_t instanceCustomIndex;
    uint32_t mask;
    uint32_t instanceShaderBindingTableRecordOffset;
    VkGeometryInstanceFlagsKHR flags;
};

class TopLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other) = delete;
    virtual ~TopLevelAccelerationStructure();

    virtual void setInstanceCount(const size_t instanceCount);
    virtual void addInstance(de::SharedPtr<BottomLevelAccelerationStructure> bottomLevelStructure,
                             const VkTransformMatrixKHR &matrix = identityMatrix3x4, uint32_t instanceCustomIndex = 0,
                             uint32_t mask = 0xFF, uint32_t instanceShaderBindingTableRecordOffset = 0,
                             VkGeometryInstanceFlagsKHR flags = VkGeometryInstanceFlagBitsKHR(0u));

    virtual void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType) = DE_NULL;
    virtual void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR flags)   = DE_NULL;
    virtual void setDeferredOperation(const bool deferredOperation)                = DE_NULL;
    virtual void setUseArrayOfPointers(const bool useArrayOfPointers)              = DE_NULL;
    virtual void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                            const uint32_t indirectBufferStride)   = DE_NULL;
    virtual VkBuildAccelerationStructureFlagsKHR getBuildFlags() const             = DE_NULL;

    // methods specific for each acceleration structure
    virtual void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                        VkDeviceAddress deviceAddress, VkDeviceSize compactCopySize)                          = DE_NULL;
    virtual void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer)     = DE_NULL;
    virtual void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                          TopLevelAccelerationStructure *accelerationStructure, VkDeviceSize compactCopySize) = DE_NULL;

    virtual void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           SerialStorage *storage)   = DE_NULL;
    virtual void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                             SerialStorage *storage) = DE_NULL;

    // helper methods for typical acceleration structure creation tasks
    void createAndBuild(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                        Allocator &allocator, VkDeviceAddress deviceAddress = 0u);
    void createAndCopyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           Allocator &allocator, VkDeviceAddress deviceAddress = 0u,
                           TopLevelAccelerationStructure *accelerationStructure = DE_NULL,
                           VkDeviceSize compactCopySize                         = 0u);
    void createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                  Allocator &allocator, VkDeviceAddress deviceAddress = 0u,
                                  SerialStorage *storage = DE_NULL);

    virtual const VkAccelerationStructureKHR *getPtr(void) const = DE_NULL;

protected:
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> m_bottomLevelInstances;
    std::vector<InstanceData> m_instanceData;
};

de::MovePtr<TopLevelAccelerationStructure> makeTopLevelAccelerationStructure();

bool queryAccelerationStructureSize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                    const std::vector<VkAccelerationStructureKHR> &accelerationStructureHandles,
                                    VkAccelerationStructureBuildTypeKHR buildType, const VkQueryPool queryPool,
                                    VkQueryType queryType, uint32_t firstQuery, std::vector<VkDeviceSize> &results);

class RayTracingPipeline
{
public:
    RayTracingPipeline();
    ~RayTracingPipeline();

    void addShader(VkShaderStageFlagBits shaderStage, Move<VkShaderModule> shaderModule, uint32_t group);
    void addLibrary(de::SharedPtr<de::MovePtr<RayTracingPipeline>> pipelineLibrary);
    Move<VkPipeline> createPipeline(const DeviceInterface &vk, const VkDevice device,
                                    const VkPipelineLayout pipelineLayout,
                                    const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries =
                                        std::vector<de::SharedPtr<Move<VkPipeline>>>());
    std::vector<de::SharedPtr<Move<VkPipeline>>> createPipelineWithLibraries(const DeviceInterface &vk,
                                                                             const VkDevice device,
                                                                             const VkPipelineLayout pipelineLayout);
    de::MovePtr<BufferWithMemory> createShaderBindingTable(
        const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline, Allocator &allocator,
        const uint32_t &shaderGroupHandleSize, const uint32_t shaderGroupBaseAlignment, const uint32_t &firstGroup,
        const uint32_t &groupCount, const VkBufferCreateFlags &additionalBufferCreateFlags = VkBufferCreateFlags(0u),
        const VkBufferUsageFlags &additionalBufferUsageFlags = VkBufferUsageFlags(0u),
        const MemoryRequirement &additionalMemoryRequirement = MemoryRequirement::Any,
        const VkDeviceAddress &opaqueCaptureAddress = 0u, const uint32_t shaderBindingTableOffset = 0u,
        const uint32_t shaderRecordSize = 0u);
    void setCreateFlags(const VkPipelineCreateFlags &pipelineCreateFlags);
    void setMaxRecursionDepth(const uint32_t &maxRecursionDepth);
    void setMaxPayloadSize(const uint32_t &maxPayloadSize);
    void setMaxAttributeSize(const uint32_t &maxAttributeSize);
    void setMaxCallableSize(const uint32_t &maxCallableSize);
    void setDeferredOperation(const bool deferredOperation);

protected:
    Move<VkPipeline> createPipelineKHR(const DeviceInterface &vk, const VkDevice device,
                                       const VkPipelineLayout pipelineLayout,
                                       const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries);

    std::vector<de::SharedPtr<Move<VkShaderModule>>> m_shadersModules;
    std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>> m_pipelineLibraries;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderCreateInfos;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shadersGroupCreateInfos;
    VkPipelineCreateFlags m_pipelineCreateFlags;
    uint32_t m_maxRecursionDepth;
    uint32_t m_maxPayloadSize;
    uint32_t m_maxAttributeSize;
    uint32_t m_maxCallableSize;
    bool m_deferredOperation;
};

class RayTracingProperties
{
protected:
    RayTracingProperties(){};

public:
    RayTracingProperties(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
    {
        DE_UNREF(vki);
        DE_UNREF(physicalDevice);
    };
    virtual ~RayTracingProperties(){};

    virtual uint32_t getShaderGroupHandleSize(void)                  = DE_NULL;
    virtual uint32_t getMaxRecursionDepth(void)                      = DE_NULL;
    virtual uint32_t getMaxShaderGroupStride(void)                   = DE_NULL;
    virtual uint32_t getShaderGroupBaseAlignment(void)               = DE_NULL;
    virtual uint64_t getMaxGeometryCount(void)                       = DE_NULL;
    virtual uint64_t getMaxInstanceCount(void)                       = DE_NULL;
    virtual uint64_t getMaxPrimitiveCount(void)                      = DE_NULL;
    virtual uint32_t getMaxDescriptorSetAccelerationStructures(void) = DE_NULL;
};

de::MovePtr<RayTracingProperties> makeRayTracingProperties(const InstanceInterface &vki,
                                                           const VkPhysicalDevice physicalDevice);

void cmdTraceRays(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                  const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                  const VkStridedBufferRegionKHR *callableShaderBindingTableRegion, uint32_t width, uint32_t height,
                  uint32_t depth);

void cmdTraceRaysIndirect(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                          const VkStridedBufferRegionKHR *raygenShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *missShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *hitShaderBindingTableRegion,
                          const VkStridedBufferRegionKHR *callableShaderBindingTableRegion, VkBuffer buffer,
                          VkDeviceSize offset);
} // namespace vk

#endif // _VKRAYTRACINGUTIL_HPP
