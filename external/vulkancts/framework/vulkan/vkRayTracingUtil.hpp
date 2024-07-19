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
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"

#include "deFloat16.h"

#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "tcuTexture.hpp"
#include "qpWatchDog.h"

#include <vector>
#include <map>
#include <limits>
#include <stdexcept>

namespace vk
{

#ifndef CTS_USES_VULKANSC

constexpr VkShaderStageFlags SHADER_STAGE_ALL_RAY_TRACING =
    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

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

inline std::string updateRayTracingGLSL(const std::string &str)
{
    return str;
}

std::string getCommonRayGenerationShader(void);

// Get lowercase version of the format name with no VK_FORMAT_ prefix.
std::string getFormatSimpleName(vk::VkFormat format);

// Test whether given poin p belons to the triangle (p0, p1, p2)
bool pointInTriangle2D(const tcu::Vec3 &p, const tcu::Vec3 &p0, const tcu::Vec3 &p1, const tcu::Vec3 &p2);

// Checks the given vertex buffer format is valid for acceleration structures.
// Note: VK_KHR_get_physical_device_properties2 and VK_KHR_acceleration_structure are supposed to be supported.
void checkAccelerationStructureVertexBufferFormat(const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                                  vk::VkFormat format);

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
    inline VkAccelerationStructureTrianglesOpacityMicromapEXT &getOpacityMicromap(void)
    {
        return m_opacityGeometryMicromap;
    }
    inline bool getHasOpacityMicromap(void) const
    {
        return m_hasOpacityMicromap;
    }
    inline void setOpacityMicromap(const VkAccelerationStructureTrianglesOpacityMicromapEXT *opacityGeometryMicromap)
    {
        m_hasOpacityMicromap      = true;
        m_opacityGeometryMicromap = *opacityGeometryMicromap;
    }
    virtual uint32_t getVertexCount(void) const         = 0;
    virtual const uint8_t *getVertexPointer(void) const = 0;
    virtual VkDeviceSize getVertexStride(void) const    = 0;
    virtual VkDeviceSize getAABBStride(void) const      = 0;
    virtual size_t getVertexByteSize(void) const        = 0;
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
    bool m_hasOpacityMicromap;
    VkAccelerationStructureTrianglesOpacityMicromapEXT m_opacityGeometryMicromap;
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

// Converts float to signed integer with variable width.
// Source float is assumed to be in the [-1, 1] range.
template <typename T>
inline T deFloat32ToSNorm(float src)
{
    DE_STATIC_ASSERT(std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed);
    const T range  = std::numeric_limits<T>::max();
    const T intVal = convertSatRte<T>(src * static_cast<float>(range));
    return de::clamp<T>(intVal, -range, range);
}

typedef tcu::Vector<deFloat16, 2> Vec2_16;
typedef tcu::Vector<deFloat16, 3> Vec3_16;
typedef tcu::Vector<deFloat16, 4> Vec4_16;
typedef tcu::Vector<int16_t, 2> Vec2_16SNorm;
typedef tcu::Vector<int16_t, 3> Vec3_16SNorm;
typedef tcu::Vector<int16_t, 4> Vec4_16SNorm;
typedef tcu::Vector<int8_t, 2> Vec2_8SNorm;
typedef tcu::Vector<int8_t, 3> Vec3_8SNorm;
typedef tcu::Vector<int8_t, 4> Vec4_8SNorm;

template <typename V>
VkFormat vertexFormatFromType();
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
inline VkFormat vertexFormatFromType<tcu::Vec4>()
{
    return VK_FORMAT_R32G32B32A32_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec2_16>()
{
    return VK_FORMAT_R16G16_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec3_16>()
{
    return VK_FORMAT_R16G16B16_SFLOAT;
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
inline VkFormat vertexFormatFromType<Vec3_16SNorm>()
{
    return VK_FORMAT_R16G16B16_SNORM;
}
template <>
inline VkFormat vertexFormatFromType<Vec4_16SNorm>()
{
    return VK_FORMAT_R16G16B16A16_SNORM;
}
template <>
inline VkFormat vertexFormatFromType<tcu::DVec2>()
{
    return VK_FORMAT_R64G64_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<tcu::DVec3>()
{
    return VK_FORMAT_R64G64B64_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<tcu::DVec4>()
{
    return VK_FORMAT_R64G64B64A64_SFLOAT;
}
template <>
inline VkFormat vertexFormatFromType<Vec2_8SNorm>()
{
    return VK_FORMAT_R8G8_SNORM;
}
template <>
inline VkFormat vertexFormatFromType<Vec3_8SNorm>()
{
    return VK_FORMAT_R8G8B8_SNORM;
}
template <>
inline VkFormat vertexFormatFromType<Vec4_8SNorm>()
{
    return VK_FORMAT_R8G8B8A8_SNORM;
}

struct EmptyIndex
{
};
template <typename I>
VkIndexType indexTypeFromType();
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
V convertFloatTo(const tcu::Vec3 &vertex);
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
inline tcu::Vec4 convertFloatTo<tcu::Vec4>(const tcu::Vec3 &vertex)
{
    return tcu::Vec4(vertex.x(), vertex.y(), vertex.z(), 0.0f);
}
template <>
inline Vec2_16 convertFloatTo<Vec2_16>(const tcu::Vec3 &vertex)
{
    return Vec2_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y()));
}
template <>
inline Vec3_16 convertFloatTo<Vec3_16>(const tcu::Vec3 &vertex)
{
    return Vec3_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y()), deFloat32To16(vertex.z()));
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
    return Vec2_16SNorm(deFloat32ToSNorm<int16_t>(vertex.x()), deFloat32ToSNorm<int16_t>(vertex.y()));
}
template <>
inline Vec3_16SNorm convertFloatTo<Vec3_16SNorm>(const tcu::Vec3 &vertex)
{
    return Vec3_16SNorm(deFloat32ToSNorm<int16_t>(vertex.x()), deFloat32ToSNorm<int16_t>(vertex.y()),
                        deFloat32ToSNorm<int16_t>(vertex.z()));
}
template <>
inline Vec4_16SNorm convertFloatTo<Vec4_16SNorm>(const tcu::Vec3 &vertex)
{
    return Vec4_16SNorm(deFloat32ToSNorm<int16_t>(vertex.x()), deFloat32ToSNorm<int16_t>(vertex.y()),
                        deFloat32ToSNorm<int16_t>(vertex.z()), deFloat32ToSNorm<int16_t>(0.0f));
}
template <>
inline tcu::DVec2 convertFloatTo<tcu::DVec2>(const tcu::Vec3 &vertex)
{
    return tcu::DVec2(static_cast<double>(vertex.x()), static_cast<double>(vertex.y()));
}
template <>
inline tcu::DVec3 convertFloatTo<tcu::DVec3>(const tcu::Vec3 &vertex)
{
    return tcu::DVec3(static_cast<double>(vertex.x()), static_cast<double>(vertex.y()),
                      static_cast<double>(vertex.z()));
}
template <>
inline tcu::DVec4 convertFloatTo<tcu::DVec4>(const tcu::Vec3 &vertex)
{
    return tcu::DVec4(static_cast<double>(vertex.x()), static_cast<double>(vertex.y()), static_cast<double>(vertex.z()),
                      0.0);
}
template <>
inline Vec2_8SNorm convertFloatTo<Vec2_8SNorm>(const tcu::Vec3 &vertex)
{
    return Vec2_8SNorm(deFloat32ToSNorm<int8_t>(vertex.x()), deFloat32ToSNorm<int8_t>(vertex.y()));
}
template <>
inline Vec3_8SNorm convertFloatTo<Vec3_8SNorm>(const tcu::Vec3 &vertex)
{
    return Vec3_8SNorm(deFloat32ToSNorm<int8_t>(vertex.x()), deFloat32ToSNorm<int8_t>(vertex.y()),
                       deFloat32ToSNorm<int8_t>(vertex.z()));
}
template <>
inline Vec4_8SNorm convertFloatTo<Vec4_8SNorm>(const tcu::Vec3 &vertex)
{
    return Vec4_8SNorm(deFloat32ToSNorm<int8_t>(vertex.x()), deFloat32ToSNorm<int8_t>(vertex.y()),
                       deFloat32ToSNorm<int8_t>(vertex.z()), deFloat32ToSNorm<int8_t>(0.0f));
}

template <typename V>
V convertIndexTo(uint32_t index);
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
    RaytracedGeometry(VkGeometryTypeKHR geometryType, uint32_t paddingBlocks = 0u);
    RaytracedGeometry(VkGeometryTypeKHR geometryType, const std::vector<V> &vertices,
                      const std::vector<I> &indices = std::vector<I>(), uint32_t paddingBlocks = 0u);

    uint32_t getVertexCount(void) const override;
    const uint8_t *getVertexPointer(void) const override;
    VkDeviceSize getVertexStride(void) const override;
    VkDeviceSize getAABBStride(void) const override;
    size_t getVertexByteSize(void) const override;
    uint32_t getIndexCount(void) const override;
    const uint8_t *getIndexPointer(void) const override;
    VkDeviceSize getIndexStride(void) const override;
    size_t getIndexByteSize(void) const override;
    uint32_t getPrimitiveCount(void) const override;

    void addVertex(const tcu::Vec3 &vertex) override;
    void addIndex(const uint32_t &index) override;

private:
    void init();                           // To be run in constructors.
    void checkGeometryType() const;        // Checks geometry type is valid.
    void calcBlockSize();                  // Calculates and saves vertex buffer block size.
    size_t getBlockSize() const;           // Return stored vertex buffer block size.
    void addNativeVertex(const V &vertex); // Adds new vertex in native format.

    // The implementation below stores vertices as byte blocks to take the requested padding into account. m_vertices is the array
    // of bytes containing vertex data.
    //
    // For triangles, the padding block has a size that is a multiple of the vertex size and each vertex is stored in a byte block
    // equivalent to:
    //
    //    struct Vertex
    //    {
    // V vertex;
    // uint8_t padding[m_paddingBlocks * sizeof(V)];
    // };
    //
    // For AABBs, the padding block has a size that is a multiple of kAABBPadBaseSize (see below) and vertices are stored in pairs
    // before the padding block. This is equivalent to:
    //
    //        struct VertexPair
    //        {
    // V vertices[2];
    // uint8_t padding[m_paddingBlocks * kAABBPadBaseSize];
    // };
    //
    // The size of each pseudo-structure above is saved to one of the correspoding union members below.
    union BlockSize
    {
        size_t trianglesBlockSize;
        size_t aabbsBlockSize;
    };

    const uint32_t m_paddingBlocks;
    size_t m_vertexCount;
    std::vector<uint8_t> m_vertices; // Vertices are stored as byte blocks.
    std::vector<I> m_indices;        // Indices are stored natively.
    BlockSize m_blockSize;           // For m_vertices.

    // Data sizes.
    static constexpr size_t kVertexSize      = sizeof(V);
    static constexpr size_t kIndexSize       = sizeof(I);
    static constexpr size_t kAABBPadBaseSize = 8; // As required by the spec.
};

template <typename V, typename I>
RaytracedGeometry<V, I>::RaytracedGeometry(VkGeometryTypeKHR geometryType, uint32_t paddingBlocks)
    : RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
    , m_paddingBlocks(paddingBlocks)
    , m_vertexCount(0)
{
    init();
}

template <typename V, typename I>
RaytracedGeometry<V, I>::RaytracedGeometry(VkGeometryTypeKHR geometryType, const std::vector<V> &vertices,
                                           const std::vector<I> &indices, uint32_t paddingBlocks)
    : RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
    , m_paddingBlocks(paddingBlocks)
    , m_vertexCount(0)
    , m_vertices()
    , m_indices(indices)
{
    init();
    for (const auto &vertex : vertices)
        addNativeVertex(vertex);
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getVertexCount(void) const
{
    return (isTrianglesType() ? static_cast<uint32_t>(m_vertexCount) : 0u);
}

template <typename V, typename I>
const uint8_t *RaytracedGeometry<V, I>::getVertexPointer(void) const
{
    DE_ASSERT(!m_vertices.empty());
    return reinterpret_cast<const uint8_t *>(m_vertices.data());
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getVertexStride(void) const
{
    return ((!isTrianglesType()) ? 0ull : static_cast<VkDeviceSize>(getBlockSize()));
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getAABBStride(void) const
{
    return (isTrianglesType() ? 0ull : static_cast<VkDeviceSize>(getBlockSize()));
}

template <typename V, typename I>
size_t RaytracedGeometry<V, I>::getVertexByteSize(void) const
{
    return m_vertices.size();
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getIndexCount(void) const
{
    return static_cast<uint32_t>(isTrianglesType() ? m_indices.size() : 0);
}

template <typename V, typename I>
const uint8_t *RaytracedGeometry<V, I>::getIndexPointer(void) const
{
    const auto indexCount = getIndexCount();
    DE_UNREF(indexCount); // For release builds.
    DE_ASSERT(indexCount > 0u);

    return reinterpret_cast<const uint8_t *>(m_indices.data());
}

template <typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getIndexStride(void) const
{
    return static_cast<VkDeviceSize>(kIndexSize);
}

template <typename V, typename I>
size_t RaytracedGeometry<V, I>::getIndexByteSize(void) const
{
    const auto indexCount = getIndexCount();
    DE_ASSERT(indexCount > 0u);

    return (indexCount * kIndexSize);
}

template <typename V, typename I>
uint32_t RaytracedGeometry<V, I>::getPrimitiveCount(void) const
{
    return static_cast<uint32_t>(isTrianglesType() ? (usesIndices() ? m_indices.size() / 3 : m_vertexCount / 3) :
                                                     (m_vertexCount / 2));
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::addVertex(const tcu::Vec3 &vertex)
{
    addNativeVertex(convertFloatTo<V>(vertex));
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::addNativeVertex(const V &vertex)
{
    const auto oldSize   = m_vertices.size();
    const auto blockSize = getBlockSize();

    if (isTrianglesType())
    {
        // Reserve new block, copy vertex at the beginning of the new block.
        m_vertices.resize(oldSize + blockSize, uint8_t{0});
        deMemcpy(&m_vertices[oldSize], &vertex, kVertexSize);
    }
    else // AABB
    {
        if (m_vertexCount % 2 == 0)
        {
            // New block needed.
            m_vertices.resize(oldSize + blockSize, uint8_t{0});
            deMemcpy(&m_vertices[oldSize], &vertex, kVertexSize);
        }
        else
        {
            // Insert in the second position of last existing block.
            //
            //                                                Vertex Size
            //                                                +-------+
            //    +-------------+------------+----------------------------------------+
            //    |             |            |      ...       | vertex vertex padding |
            //    +-------------+------------+----------------+-----------------------+
            //                                                +-----------------------+
            //                                                        Block Size
            //    +-------------------------------------------------------------------+
            //                            Old Size
            //
            deMemcpy(&m_vertices[oldSize - blockSize + kVertexSize], &vertex, kVertexSize);
        }
    }

    ++m_vertexCount;
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::addIndex(const uint32_t &index)
{
    m_indices.push_back(convertIndexTo<I>(index));
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::init()
{
    checkGeometryType();
    calcBlockSize();
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::checkGeometryType() const
{
    const auto geometryType = getGeometryType();
    DE_UNREF(geometryType); // For release builds.
    DE_ASSERT(geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR || geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
}

template <typename V, typename I>
void RaytracedGeometry<V, I>::calcBlockSize()
{
    if (isTrianglesType())
        m_blockSize.trianglesBlockSize = kVertexSize * static_cast<size_t>(1u + m_paddingBlocks);
    else
        m_blockSize.aabbsBlockSize = 2 * kVertexSize + m_paddingBlocks * kAABBPadBaseSize;
}

template <typename V, typename I>
size_t RaytracedGeometry<V, I>::getBlockSize() const
{
    return (isTrianglesType() ? m_blockSize.trianglesBlockSize : m_blockSize.aabbsBlockSize);
}

de::SharedPtr<RaytracedGeometryBase> makeRaytracedGeometry(VkGeometryTypeKHR geometryType, VkFormat vertexFormat,
                                                           VkIndexType indexType, bool padVertices = false);

VkDeviceAddress getBufferDeviceAddress(const DeviceInterface &vkd, const VkDevice device, const VkBuffer buffer,
                                       VkDeviceSize offset);

// type used for creating a deep serialization/deserialization of top-level acceleration structures
class SerialInfo
{
    std::vector<uint64_t> m_addresses;
    std::vector<VkDeviceSize> m_sizes;

public:
    SerialInfo() = default;

    // addresses: { (owner-top-level AS address) [, (first bottom_level AS address), (second bottom_level AS address), ...] }
    // sizes:     { (owner-top-level AS serial size) [, (first bottom_level AS serial size), (second bottom_level AS serial size), ...] }
    SerialInfo(const std::vector<uint64_t> &addresses, const std::vector<VkDeviceSize> &sizes)
        : m_addresses(addresses)
        , m_sizes(sizes)
    {
        DE_ASSERT(!addresses.empty() && addresses.size() == sizes.size());
    }

    const std::vector<uint64_t> &addresses() const
    {
        return m_addresses;
    }
    const std::vector<VkDeviceSize> &sizes() const
    {
        return m_sizes;
    }
};

class SerialStorage
{
public:
    enum
    {
        DE_SERIALIZED_FIELD(
            DRIVER_UUID, VK_UUID_SIZE), // VK_UUID_SIZE bytes of data matching VkPhysicalDeviceIDProperties::driverUUID
        DE_SERIALIZED_FIELD(
            COMPAT_UUID,
            VK_UUID_SIZE), // VK_UUID_SIZE bytes of data identifying the compatibility for comparison using vkGetDeviceAccelerationStructureCompatibilityKHR
        DE_SERIALIZED_FIELD(
            SERIALIZED_SIZE,
            sizeof(
                uint64_t)), // A 64-bit integer of the total size matching the value queried using VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR
        DE_SERIALIZED_FIELD(
            DESERIALIZED_SIZE,
            sizeof(
                uint64_t)), // A 64-bit integer of the deserialized size to be passed in to VkAccelerationStructureCreateInfoKHR::size
        DE_SERIALIZED_FIELD(
            HANDLES_COUNT,
            sizeof(
                uint64_t)), // A 64-bit integer of the count of the number of acceleration structure handles following. This will be zero for a bottom-level acceleration structure.
        SERIAL_STORAGE_SIZE_MIN
    };

    // An old fashion C-style structure that simplifies an access to the AS header
    struct alignas(16) AccelerationStructureHeader
    {
        union
        {
            struct
            {
                uint8_t driverUUID[VK_UUID_SIZE];
                uint8_t compactUUID[VK_UUID_SIZE];
            };
            uint8_t uuids[VK_UUID_SIZE * 2];
        };
        uint64_t serializedSize;
        uint64_t deserializedSize;
        uint64_t handleCount;
        VkDeviceAddress handleArray[1];
    };

    SerialStorage() = delete;
    SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                  const VkAccelerationStructureBuildTypeKHR buildType, const VkDeviceSize storageSize);
    // An additional constructor for creating a deep copy of top-level AS's.
    SerialStorage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                  const VkAccelerationStructureBuildTypeKHR buildType, const SerialInfo &SerialInfo);

    // below methods will return host addres if AS was build on cpu and device addres when it was build on gpu
    VkDeviceOrHostAddressKHR getAddress(const DeviceInterface &vk, const VkDevice device,
                                        const VkAccelerationStructureBuildTypeKHR buildType);
    VkDeviceOrHostAddressConstKHR getAddressConst(const DeviceInterface &vk, const VkDevice device,
                                                  const VkAccelerationStructureBuildTypeKHR buildType);

    // this methods retun host address regardless of where AS was built
    VkDeviceOrHostAddressKHR getHostAddress(VkDeviceSize offset = 0);
    VkDeviceOrHostAddressConstKHR getHostAddressConst(VkDeviceSize offset = 0);

    // works the similar way as getHostAddressConst() but returns more readable/intuitive object
    AccelerationStructureHeader *getASHeader();
    bool hasDeepFormat() const;
    de::SharedPtr<SerialStorage> getBottomStorage(uint32_t index) const;

    VkDeviceSize getStorageSize() const;
    const SerialInfo &getSerialInfo() const;
    uint64_t getDeserializedSize();

protected:
    const VkAccelerationStructureBuildTypeKHR m_buildType;
    const VkDeviceSize m_storageSize;
    const SerialInfo m_serialInfo;
    de::MovePtr<BufferWithMemory> m_buffer;
    std::vector<de::SharedPtr<SerialStorage>> m_bottoms;
};

class BottomLevelAccelerationStructure
{
public:
    static uint32_t getRequiredAllocationCount(void);

    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other) = delete;
    virtual ~BottomLevelAccelerationStructure();

    virtual void setGeometryData(const std::vector<tcu::Vec3> &geometryData, const bool triangles,
                                 const VkGeometryFlagsKHR geometryFlags = 0u);
    virtual void setDefaultGeometryData(const VkShaderStageFlagBits testStage,
                                        const VkGeometryFlagsKHR geometryFlags = 0u);
    virtual void setGeometryCount(const size_t geometryCount);
    virtual void addGeometry(de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry);
    virtual void addGeometry(
        const std::vector<tcu::Vec3> &geometryData, const bool triangles, const VkGeometryFlagsKHR geometryFlags = 0u,
        const VkAccelerationStructureTrianglesOpacityMicromapEXT *opacityGeometryMicromap = DE_NULL);

    virtual void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)                         = 0;
    virtual VkAccelerationStructureBuildTypeKHR getBuildType() const                                       = 0;
    virtual void setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags)                   = 0;
    virtual void setCreateGeneric(bool createGeneric)                                                      = 0;
    virtual void setCreationBufferUnbounded(bool creationBufferUnbounded)                                  = 0;
    virtual void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags)                      = 0;
    virtual void setBuildWithoutGeometries(bool buildWithoutGeometries)                                    = 0;
    virtual void setBuildWithoutPrimitives(bool buildWithoutPrimitives)                                    = 0;
    virtual void setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount = 0u) = 0;
    virtual void setUseArrayOfPointers(const bool useArrayOfPointers)                                      = 0;
    virtual void setUseMaintenance5(const bool useMaintenance5)                                            = 0;
    virtual void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                            const uint32_t indirectBufferStride)                           = 0;
    virtual VkBuildAccelerationStructureFlagsKHR getBuildFlags() const                                     = 0;
    VkAccelerationStructureBuildSizesInfoKHR getStructureBuildSizes() const;

    // methods specific for each acceleration structure
    virtual void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                        VkDeviceSize structureSize, VkDeviceAddress deviceAddress = 0u, const void *pNext = DE_NULL,
                        const MemoryRequirement &addMemoryRequirement = MemoryRequirement::Any,
                        const VkBuffer creationBuffer = VK_NULL_HANDLE, const VkDeviceSize creationBufferSize = 0u) = 0;
    virtual void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                       BottomLevelAccelerationStructure *srcAccelerationStructure = DE_NULL)                        = 0;
    virtual void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                          BottomLevelAccelerationStructure *accelerationStructure, bool compactCopy)                = 0;

    virtual void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           SerialStorage *storage)   = 0;
    virtual void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                             SerialStorage *storage) = 0;

    // helper methods for typical acceleration structure creation tasks
    void createAndBuild(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                        Allocator &allocator, VkDeviceAddress deviceAddress = 0u);
    void createAndCopyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           Allocator &allocator, BottomLevelAccelerationStructure *accelerationStructure,
                           VkDeviceSize compactCopySize = 0u, VkDeviceAddress deviceAddress = 0u);
    void createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                  Allocator &allocator, SerialStorage *storage, VkDeviceAddress deviceAddress = 0u);
    virtual const VkAccelerationStructureKHR *getPtr(void) const                                               = 0;
    virtual void updateGeometry(size_t geometryIndex, de::SharedPtr<RaytracedGeometryBase> &raytracedGeometry) = 0;

protected:
    std::vector<de::SharedPtr<RaytracedGeometryBase>> m_geometriesData;
    VkDeviceSize m_structureSize;
    VkDeviceSize m_updateScratchSize;
    VkDeviceSize m_buildScratchSize;
};

de::MovePtr<BottomLevelAccelerationStructure> makeBottomLevelAccelerationStructure();

/**
 * @brief Implements a pool of BottomLevelAccelerationStructure
 */
class BottomLevelAccelerationStructurePool
{
public:
    typedef de::SharedPtr<BottomLevelAccelerationStructure> BlasPtr;
    struct BlasInfo
    {
        VkDeviceSize structureSize;
        VkDeviceAddress deviceAddress;
    };

    BottomLevelAccelerationStructurePool();
    virtual ~BottomLevelAccelerationStructurePool();

    BlasPtr at(uint32_t index) const
    {
        return m_structs[index];
    }
    BlasPtr operator[](uint32_t index) const
    {
        return m_structs[index];
    }
    auto structures() const -> const std::vector<BlasPtr> &
    {
        return m_structs;
    }
    uint32_t structCount() const
    {
        return static_cast<uint32_t>(m_structs.size());
    }

    // defines how many structures will be packet in single buffer
    uint32_t batchStructCount() const
    {
        return m_batchStructCount;
    }
    void batchStructCount(const uint32_t &value);

    // defines how many geometries (vertices and/or indices) will be packet in single buffer
    uint32_t batchGeomCount() const
    {
        return m_batchGeomCount;
    }
    void batchGeomCount(const uint32_t &value)
    {
        m_batchGeomCount = value;
    }

    bool tryCachedMemory() const
    {
        return m_tryCachedMemory;
    }
    void tryCachedMemory(const bool cachedMemory)
    {
        m_tryCachedMemory = cachedMemory;
    }

    BlasPtr add(VkDeviceSize structureSize = 0, VkDeviceAddress deviceAddress = 0);
    /**
     * @brief Creates previously added bottoms at a time.
     * @note  All geometries must be known before call this method.
     */
    void batchCreate(const DeviceInterface &vkd, const VkDevice device, Allocator &allocator);
    void batchCreateAdjust(const DeviceInterface &vkd, const VkDevice device, Allocator &allocator,
                           const VkDeviceSize maxBufferSize);
    void batchBuild(const DeviceInterface &vk, const VkDevice device, VkCommandBuffer cmdBuffer);
    void batchBuild(const DeviceInterface &vk, const VkDevice device, VkCommandPool cmdPool, VkQueue queue,
                    qpWatchDog *watchDog);
    size_t getAllocationCount() const;
    size_t getAllocationCount(const DeviceInterface &vk, const VkDevice device, const VkDeviceSize maxBufferSize) const;
    auto getAllocationSizes(const DeviceInterface &vk, // (strBuff, scratchBuff, vertBuff, indexBuff)
                            const VkDevice device) const -> tcu::Vector<VkDeviceSize, 4>;

protected:
    uint32_t m_batchStructCount; // default is 4
    uint32_t m_batchGeomCount;   // default is 0, if zero then batchStructCount is used
    std::vector<BlasInfo> m_infos;
    std::vector<BlasPtr> m_structs;
    bool m_createOnce;
    bool m_tryCachedMemory;
    VkDeviceSize m_structsBuffSize;
    VkDeviceSize m_updatesScratchSize;
    VkDeviceSize m_buildsScratchSize;
    VkDeviceSize m_verticesSize;
    VkDeviceSize m_indicesSize;

protected:
    struct Impl;
    Impl *m_impl;
};

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
    struct CreationSizes
    {
        VkDeviceSize structure;
        VkDeviceSize updateScratch;
        VkDeviceSize buildScratch;
        VkDeviceSize instancePointers;
        VkDeviceSize instancesBuffer;
        VkDeviceSize sum() const;
    };

    static uint32_t getRequiredAllocationCount(void);

    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other) = delete;
    virtual ~TopLevelAccelerationStructure();

    virtual void setInstanceCount(const size_t instanceCount);
    virtual void addInstance(de::SharedPtr<BottomLevelAccelerationStructure> bottomLevelStructure,
                             const VkTransformMatrixKHR &matrix = identityMatrix3x4, uint32_t instanceCustomIndex = 0,
                             uint32_t mask = 0xFF, uint32_t instanceShaderBindingTableRecordOffset = 0,
                             VkGeometryInstanceFlagsKHR flags = VkGeometryInstanceFlagBitsKHR(0u));

    virtual void setBuildType(const VkAccelerationStructureBuildTypeKHR buildType)                         = 0;
    virtual void setCreateFlags(const VkAccelerationStructureCreateFlagsKHR createFlags)                   = 0;
    virtual void setCreateGeneric(bool createGeneric)                                                      = 0;
    virtual void setCreationBufferUnbounded(bool creationBufferUnbounded)                                  = 0;
    virtual void setBuildFlags(const VkBuildAccelerationStructureFlagsKHR buildFlags)                      = 0;
    virtual void setBuildWithoutPrimitives(bool buildWithoutPrimitives)                                    = 0;
    virtual void setInactiveInstances(bool inactiveInstances)                                              = 0;
    virtual void setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount = 0u) = 0;
    virtual void setUseArrayOfPointers(const bool useArrayOfPointers)                                      = 0;
    virtual void setIndirectBuildParameters(const VkBuffer indirectBuffer, const VkDeviceSize indirectBufferOffset,
                                            const uint32_t indirectBufferStride)                           = 0;
    virtual void setUsePPGeometries(const bool usePPGeometries)                                            = 0;
    virtual void setTryCachedMemory(const bool tryCachedMemory)                                            = 0;
    virtual VkBuildAccelerationStructureFlagsKHR getBuildFlags() const                                     = 0;
    VkAccelerationStructureBuildSizesInfoKHR getStructureBuildSizes() const;

    // methods specific for each acceleration structure
    virtual void getCreationSizes(const DeviceInterface &vk, const VkDevice device, const VkDeviceSize structureSize,
                                  CreationSizes &sizes)                                                             = 0;
    virtual void create(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                        VkDeviceSize structureSize = 0u, VkDeviceAddress deviceAddress = 0u,
                        const void *pNext                             = DE_NULL,
                        const MemoryRequirement &addMemoryRequirement = MemoryRequirement::Any,
                        const VkBuffer creationBuffer = VK_NULL_HANDLE, const VkDeviceSize creationBufferSize = 0u) = 0;
    virtual void build(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                       TopLevelAccelerationStructure *srcAccelerationStructure = DE_NULL)                           = 0;
    virtual void copyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                          TopLevelAccelerationStructure *accelerationStructure, bool compactCopy)                   = 0;

    virtual void serialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           SerialStorage *storage)   = 0;
    virtual void deserialize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                             SerialStorage *storage) = 0;

    virtual std::vector<VkDeviceSize> getSerializingSizes(const DeviceInterface &vk, const VkDevice device,
                                                          const VkQueue queue, const uint32_t queueFamilyIndex) = 0;

    virtual std::vector<uint64_t> getSerializingAddresses(const DeviceInterface &vk, const VkDevice device) const = 0;

    // helper methods for typical acceleration structure creation tasks
    void createAndBuild(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                        Allocator &allocator, VkDeviceAddress deviceAddress = 0u);
    void createAndCopyFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                           Allocator &allocator, TopLevelAccelerationStructure *accelerationStructure,
                           VkDeviceSize compactCopySize = 0u, VkDeviceAddress deviceAddress = 0u);
    void createAndDeserializeFrom(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                  Allocator &allocator, SerialStorage *storage, VkDeviceAddress deviceAddress = 0u);

    virtual const VkAccelerationStructureKHR *getPtr(void) const = 0;

    virtual void updateInstanceMatrix(const DeviceInterface &vk, const VkDevice device, size_t instanceIndex,
                                      const VkTransformMatrixKHR &matrix) = 0;

protected:
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> m_bottomLevelInstances;
    std::vector<InstanceData> m_instanceData;
    VkDeviceSize m_structureSize;
    VkDeviceSize m_updateScratchSize;
    VkDeviceSize m_buildScratchSize;

    virtual void createAndDeserializeBottoms(const DeviceInterface &vk, const VkDevice device,
                                             const VkCommandBuffer cmdBuffer, Allocator &allocator,
                                             SerialStorage *storage) = 0;
};

de::MovePtr<TopLevelAccelerationStructure> makeTopLevelAccelerationStructure();

template <class ASType>
de::MovePtr<ASType> makeAccelerationStructure();
template <>
inline de::MovePtr<BottomLevelAccelerationStructure> makeAccelerationStructure()
{
    return makeBottomLevelAccelerationStructure();
}
template <>
inline de::MovePtr<TopLevelAccelerationStructure> makeAccelerationStructure()
{
    return makeTopLevelAccelerationStructure();
}

bool queryAccelerationStructureSize(const DeviceInterface &vk, const VkDevice device, const VkCommandBuffer cmdBuffer,
                                    const std::vector<VkAccelerationStructureKHR> &accelerationStructureHandles,
                                    VkAccelerationStructureBuildTypeKHR buildType, const VkQueryPool queryPool,
                                    VkQueryType queryType, uint32_t firstQuery, std::vector<VkDeviceSize> &results);

class RayTracingPipeline
{
public:
    class CompileRequiredError : public std::runtime_error
    {
    public:
        CompileRequiredError(const std::string &error) : std::runtime_error(error)
        {
        }
    };

    RayTracingPipeline();
    ~RayTracingPipeline();

    void addShader(VkShaderStageFlagBits shaderStage, Move<VkShaderModule> shaderModule, uint32_t group,
                   const VkSpecializationInfo *specializationInfo = nullptr,
                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags =
                       static_cast<VkPipelineShaderStageCreateFlags>(0),
                   const void *pipelineShaderStageCreateInfopNext = nullptr);
    void addShader(VkShaderStageFlagBits shaderStage, de::SharedPtr<Move<VkShaderModule>> shaderModule, uint32_t group,
                   const VkSpecializationInfo *specializationInfoPtr = nullptr,
                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags =
                       static_cast<VkPipelineShaderStageCreateFlags>(0),
                   const void *pipelineShaderStageCreateInfopNext = nullptr);
    void addShader(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, uint32_t group,
                   const VkSpecializationInfo *specializationInfo = nullptr,
                   const VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags =
                       static_cast<VkPipelineShaderStageCreateFlags>(0),
                   const void *pipelineShaderStageCreateInfopNext = nullptr);
    void setGroupCaptureReplayHandle(uint32_t group, const void *pShaderGroupCaptureReplayHandle);
    void addLibrary(de::SharedPtr<de::MovePtr<RayTracingPipeline>> pipelineLibrary);
    uint32_t getShaderGroupCount(void);     // This pipeline only.
    uint32_t getFullShaderGroupCount(void); // This pipeline and its included pipeline libraries, recursively.
    Move<VkPipeline> createPipeline(const DeviceInterface &vk, const VkDevice device,
                                    const VkPipelineLayout pipelineLayout,
                                    const std::vector<de::SharedPtr<Move<VkPipeline>>> &pipelineLibraries =
                                        std::vector<de::SharedPtr<Move<VkPipeline>>>());
    Move<VkPipeline> createPipeline(const DeviceInterface &vk, const VkDevice device,
                                    const VkPipelineLayout pipelineLayout,
                                    const std::vector<VkPipeline> &pipelineLibraries,
                                    const VkPipelineCache pipelineCache);
    std::vector<de::SharedPtr<Move<VkPipeline>>> createPipelineWithLibraries(const DeviceInterface &vk,
                                                                             const VkDevice device,
                                                                             const VkPipelineLayout pipelineLayout);
    std::vector<uint8_t> getShaderGroupHandles(const DeviceInterface &vk, const VkDevice device,
                                               const VkPipeline pipeline, const uint32_t shaderGroupHandleSize,
                                               const uint32_t firstGroup, const uint32_t groupCount) const;
    std::vector<uint8_t> getShaderGroupReplayHandles(const DeviceInterface &vk, const VkDevice device,
                                                     const VkPipeline pipeline,
                                                     const uint32_t shaderGroupHandleReplaySize,
                                                     const uint32_t firstGroup, const uint32_t groupCount) const;
    de::MovePtr<BufferWithMemory> createShaderBindingTable(
        const DeviceInterface &vk, const VkDevice device, const VkPipeline pipeline, Allocator &allocator,
        const uint32_t &shaderGroupHandleSize, const uint32_t shaderGroupBaseAlignment, const uint32_t &firstGroup,
        const uint32_t &groupCount, const VkBufferCreateFlags &additionalBufferCreateFlags = VkBufferCreateFlags(0u),
        const VkBufferUsageFlags &additionalBufferUsageFlags = VkBufferUsageFlags(0u),
        const MemoryRequirement &additionalMemoryRequirement = MemoryRequirement::Any,
        const VkDeviceAddress &opaqueCaptureAddress = 0u, const uint32_t shaderBindingTableOffset = 0u,
        const uint32_t shaderRecordSize = 0u, const void **shaderGroupDataPtrPerGroup = nullptr,
        const bool autoAlignRecords = true);
    de::MovePtr<BufferWithMemory> createShaderBindingTable(
        const DeviceInterface &vk, const VkDevice device, Allocator &allocator, const uint32_t shaderGroupHandleSize,
        const uint32_t shaderGroupBaseAlignment, const std::vector<uint8_t> &shaderHandles,
        const VkBufferCreateFlags additionalBufferCreateFlags = VkBufferCreateFlags(0u),
        const VkBufferUsageFlags additionalBufferUsageFlags   = VkBufferUsageFlags(0u),
        const MemoryRequirement &additionalMemoryRequirement  = MemoryRequirement::Any,
        const VkDeviceAddress opaqueCaptureAddress = 0u, const uint32_t shaderBindingTableOffset = 0u,
        const uint32_t shaderRecordSize = 0u, const void **shaderGroupDataPtrPerGroup = nullptr,
        const bool autoAlignRecords = true);
    void setCreateFlags(const VkPipelineCreateFlags &pipelineCreateFlags);
    void setCreateFlags2(const VkPipelineCreateFlags2KHR &pipelineCreateFlags2);
    void setMaxRecursionDepth(const uint32_t &maxRecursionDepth);
    void setMaxPayloadSize(const uint32_t &maxPayloadSize);
    void setMaxAttributeSize(const uint32_t &maxAttributeSize);
    void setDeferredOperation(const bool deferredOperation, const uint32_t workerThreadCount = 0);
    void addDynamicState(const VkDynamicState &dynamicState);

protected:
    Move<VkPipeline> createPipelineKHR(const DeviceInterface &vk, const VkDevice device,
                                       const VkPipelineLayout pipelineLayout,
                                       const std::vector<VkPipeline> &pipelineLibraries,
                                       const VkPipelineCache pipelineCache = VK_NULL_HANDLE);

    std::vector<de::SharedPtr<Move<VkShaderModule>>> m_shadersModules;
    std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>> m_pipelineLibraries;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderCreateInfos;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shadersGroupCreateInfos;
    VkPipelineCreateFlags m_pipelineCreateFlags;
    VkPipelineCreateFlags2KHR m_pipelineCreateFlags2;
    uint32_t m_maxRecursionDepth;
    uint32_t m_maxPayloadSize;
    uint32_t m_maxAttributeSize;
    bool m_deferredOperation;
    uint32_t m_workerThreadCount;
    std::vector<VkDynamicState> m_dynamicStates;
};

class RayTracingProperties
{
protected:
    RayTracingProperties()
    {
    }

public:
    RayTracingProperties(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
    {
        DE_UNREF(vki);
        DE_UNREF(physicalDevice);
    }
    virtual ~RayTracingProperties()
    {
    }

    virtual uint32_t getShaderGroupHandleSize(void)                  = 0;
    virtual uint32_t getShaderGroupHandleAlignment(void)             = 0;
    virtual uint32_t getShaderGroupHandleCaptureReplaySize(void)     = 0;
    virtual uint32_t getMaxRecursionDepth(void)                      = 0;
    virtual uint32_t getMaxShaderGroupStride(void)                   = 0;
    virtual uint32_t getShaderGroupBaseAlignment(void)               = 0;
    virtual uint64_t getMaxGeometryCount(void)                       = 0;
    virtual uint64_t getMaxInstanceCount(void)                       = 0;
    virtual uint64_t getMaxPrimitiveCount(void)                      = 0;
    virtual uint32_t getMaxDescriptorSetAccelerationStructures(void) = 0;
    virtual uint32_t getMaxRayDispatchInvocationCount(void)          = 0;
    virtual uint32_t getMaxRayHitAttributeSize(void)                 = 0;
    virtual uint32_t getMaxMemoryAllocationCount(void)               = 0;
};

de::MovePtr<RayTracingProperties> makeRayTracingProperties(const InstanceInterface &vki,
                                                           const VkPhysicalDevice physicalDevice);

void cmdTraceRays(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                  const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                  const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion, uint32_t width,
                  uint32_t height, uint32_t depth);

void cmdTraceRaysIndirect(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                          const VkStridedDeviceAddressRegionKHR *raygenShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *missShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *hitShaderBindingTableRegion,
                          const VkStridedDeviceAddressRegionKHR *callableShaderBindingTableRegion,
                          VkDeviceAddress indirectDeviceAddress);

void cmdTraceRaysIndirect2(const DeviceInterface &vk, VkCommandBuffer commandBuffer,
                           VkDeviceAddress indirectDeviceAddress);

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

enum class RayQueryShaderSourcePipeline
{
    COMPUTE,
    GRAPHICS,
    RAYTRACING,
    INVALID_PIPELINE
};

enum class RayQueryShaderSourceType
{
    VERTEX,
    TESSELLATION_CONTROL,
    TESSELLATION_EVALUATION,
    GEOMETRY,
    FRAGMENT,
    COMPUTE,
    RAY_GENERATION_RT,
    RAY_GENERATION,
    INTERSECTION,
    ANY_HIT,
    CLOSEST_HIT,
    MISS,
    CALLABLE,
    INVALID
};

struct Ray
{
    Ray() : o(0.0f), tmin(0.0f), d(0.0f), tmax(0.0f)
    {
    }
    Ray(const tcu::Vec3 &io, float imin, const tcu::Vec3 &id, float imax) : o(io), tmin(imin), d(id), tmax(imax)
    {
    }
    tcu::Vec3 o;
    float tmin;
    tcu::Vec3 d;
    float tmax;
};

struct RayQueryTestParams
{
    uint32_t rayFlags;
    std::string name;
    std::string shaderFunctions;
    std::vector<Ray> rays;
    std::vector<std::vector<tcu::Vec3>> verts;
    std::vector<std::vector<tcu::Vec3>> aabbs;
    bool triangles;
    RayQueryShaderSourcePipeline pipelineType;
    RayQueryShaderSourceType shaderSourceType;
    VkTransformMatrixKHR transform;
};

struct RayQueryTestState
{
    RayQueryTestState(const vk::DeviceInterface &devInterface, vk::VkDevice dev,
                      const vk::InstanceInterface &instInterface, vk::VkPhysicalDevice pDevice,
                      uint32_t uQueueFamilyIndex)
        : deviceInterface(devInterface)
        , device(dev)
        , instanceInterface(instInterface)
        , physDevice(pDevice)
        , allocator(new SimpleAllocator(deviceInterface, device,
                                        getPhysicalDeviceMemoryProperties(instanceInterface, physDevice)))
        , cmdPool(createCommandPool(deviceInterface, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                    uQueueFamilyIndex))
    {
        pipelineBind = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    }

    const vk::DeviceInterface &deviceInterface;
    vk::VkDevice device;
    const vk::InstanceInterface &instanceInterface;
    vk::VkPhysicalDevice physDevice;
    const de::UniquePtr<vk::Allocator> allocator;
    const Unique<VkCommandPool> cmdPool;
    VkPipelineBindPoint pipelineBind;
};

static inline bool registerRayQueryShaderModule(const DeviceInterface &vkd, const VkDevice device,
                                                vk::BinaryCollection &binaryCollection,
                                                std::vector<de::SharedPtr<Move<VkShaderModule>>> &shaderModules,
                                                std::vector<VkPipelineShaderStageCreateInfo> &shaderCreateInfos,
                                                VkShaderStageFlagBits stage, const std::string &name)
{
    if (name.size() == 0)
        return false;

    shaderModules.push_back(de::SharedPtr<Move<VkShaderModule>>(
        new Move<VkShaderModule>(createShaderModule(vkd, device, binaryCollection.get(name), 0))));

    shaderCreateInfos.push_back({
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, DE_NULL, (VkPipelineShaderStageCreateFlags)0,
        stage,                       // stage
        shaderModules.back()->get(), // shader
        "main",
        DE_NULL, // pSpecializationInfo
    });

    return true;
}

static inline void initRayQueryAccelerationStructures(
    const vk::DeviceInterface &vkd, const vk::VkDevice &device, vk::Allocator &allocator, RayQueryTestParams testParams,
    VkCommandBuffer cmdBuffer,
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> &bottomAccelerationStructures,
    de::SharedPtr<vk::TopLevelAccelerationStructure> &topAccelerationStructure)
{
    uint32_t instanceCount = static_cast<uint32_t>(testParams.verts.size());

    const uint32_t instancesGroupCount = instanceCount;
    de::MovePtr<vk::TopLevelAccelerationStructure> rayQueryTopLevelAccelerationStructure =
        makeTopLevelAccelerationStructure();

    topAccelerationStructure =
        de::SharedPtr<vk::TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());
    topAccelerationStructure->setInstanceCount(instancesGroupCount);

    for (size_t instanceNdx = 0; instanceNdx < instancesGroupCount; ++instanceNdx)
    {
        de::MovePtr<BottomLevelAccelerationStructure> rayQueryBottomLevelAccelerationStructure =
            makeBottomLevelAccelerationStructure();

        bool triangles         = testParams.verts[instanceNdx].size() > 0;
        uint32_t geometryCount = (triangles) ? static_cast<uint32_t>(testParams.verts[instanceNdx].size()) / 3 :
                                               static_cast<uint32_t>(testParams.aabbs[instanceNdx].size()) / 2;
        std::vector<tcu::Vec3> geometryData;

        for (size_t geometryNdx = 0; geometryNdx < geometryCount; ++geometryNdx)
        {
            if (triangles)
            {
                tcu::Vec3 v0 = tcu::Vec3(testParams.verts[instanceNdx][geometryNdx * 3 + 0].x(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 0].y(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 0].z());
                tcu::Vec3 v1 = tcu::Vec3(testParams.verts[instanceNdx][geometryNdx * 3 + 1].x(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 1].y(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 1].z());
                tcu::Vec3 v2 = tcu::Vec3(testParams.verts[instanceNdx][geometryNdx * 3 + 2].x(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 2].y(),
                                         testParams.verts[instanceNdx][geometryNdx * 3 + 2].z());

                geometryData.push_back(v0);
                geometryData.push_back(v1);
                geometryData.push_back(v2);
            }
            else
            {
                tcu::Vec3 v0 = tcu::Vec3(testParams.aabbs[instanceNdx][geometryNdx * 2 + 0].x(),
                                         testParams.aabbs[instanceNdx][geometryNdx * 2 + 0].y(),
                                         testParams.aabbs[instanceNdx][geometryNdx * 2 + 0].z());
                tcu::Vec3 v1 = tcu::Vec3(testParams.aabbs[instanceNdx][geometryNdx * 2 + 1].x(),
                                         testParams.aabbs[instanceNdx][geometryNdx * 2 + 1].y(),
                                         testParams.aabbs[instanceNdx][geometryNdx * 2 + 1].z());

                geometryData.push_back(v0);
                geometryData.push_back(v1);
            }
        }

        rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, triangles);
        rayQueryBottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);

        bottomAccelerationStructures.push_back(
            de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));

        topAccelerationStructure->addInstance(bottomAccelerationStructures.back());
    }

    topAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
}

template <typename T>
std::vector<T> rayQueryRayTracingTestSetup(const vk::DeviceInterface &vkd, const vk::VkDevice &device,
                                           vk::Allocator &allocator, const vk::InstanceInterface &instanceInterface,
                                           vk::VkPhysicalDevice physDevice, vk::BinaryCollection &binaryCollection,
                                           vk::VkQueue universalQueue, uint32_t universalQueueFamilyIndex,
                                           const RayQueryTestParams params)
{
    RayQueryTestState state(vkd, device, instanceInterface, physDevice, universalQueueFamilyIndex);

    vk::Move<VkDescriptorPool> descriptorPool;
    vk::Move<VkDescriptorSetLayout> descriptorSetLayout;
    vk::Move<VkDescriptorSet> descriptorSet;
    vk::Move<VkPipelineLayout> pipelineLayout;
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> rayQueryBottomAccelerationStructures;
    de::SharedPtr<TopLevelAccelerationStructure> rayQueryTopAccelerationStructure;
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> traceBottomAccelerationStructures;
    de::MovePtr<TopLevelAccelerationStructure> traceAccelerationStructure;

    de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR = makeRayTracingProperties(instanceInterface, physDevice);
    uint32_t shaderGroupHandleSize                            = rayTracingPropertiesKHR->getShaderGroupHandleSize();
    uint32_t shaderGroupBaseAlignment                         = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();

    const VkBufferCreateInfo resultDataCreateInfo =
        makeBufferCreateInfo(params.rays.size() * sizeof(T), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> resultData = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, resultDataCreateInfo, MemoryRequirement::HostVisible));

    const uint32_t AllStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                               VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                               VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, AllStages)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, AllStages)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, AllStages)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, vk::VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .build(vkd, device);
    descriptorPool = DescriptorPoolBuilder()
                         .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                         .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                         .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                         .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                         .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

    const std::map<RayQueryShaderSourceType, std::vector<std::string>> shaderNames = {
        {RayQueryShaderSourceType::RAY_GENERATION_RT, {"rgen", "isect_rt", "ahit_rt", "chit_rt", "miss_rt", ""}},
        {RayQueryShaderSourceType::RAY_GENERATION, {"rgen", "", "", "", "", ""}},
        {RayQueryShaderSourceType::INTERSECTION, {"rgen", "isect_1", "", "chit", "miss", ""}},
        {RayQueryShaderSourceType::ANY_HIT, {"rgen", "isect", "ahit", "", "miss", ""}},
        {RayQueryShaderSourceType::CLOSEST_HIT, {"rgen", "isect", "", "chit", "miss", ""}},
        {RayQueryShaderSourceType::MISS, {"rgen", "isect", "", "chit", "miss_1", ""}},
        {RayQueryShaderSourceType::CALLABLE, {"rgen", "", "", "chit", "miss", "call"}}};

    auto shaderNameIt = shaderNames.find(params.shaderSourceType);
    if (shaderNameIt == end(shaderNames))
        TCU_THROW(InternalError, "Wrong shader source type");

    std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos;
    std::vector<de::SharedPtr<Move<VkShaderModule>>> shaderModules;
    bool rgen, isect, ahit, chit, miss, call;

    rgen  = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_RAYGEN_BIT_KHR, shaderNameIt->second[0]);
    isect = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderNameIt->second[1]);
    ahit  = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_ANY_HIT_BIT_KHR, shaderNameIt->second[2]);
    chit  = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderNameIt->second[3]);
    miss  = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_MISS_BIT_KHR, shaderNameIt->second[4]);
    call  = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_CALLABLE_BIT_KHR, shaderNameIt->second[5]);

    bool rgenRTTest = rgen && chit && ahit && miss && isect;
    bool isectTest  = rgen && isect && chit && miss && (shaderNameIt->second[1] == "isect_1");
    bool ahitTest   = rgen && ahit;
    bool chitTest =
        rgen && isect && chit && miss && (shaderNameIt->second[4] == "miss") && (shaderNameIt->second[1] == "isect");
    bool missTest = rgen && isect && chit && miss && (shaderNameIt->second[4] == "miss_1");
    bool callTest = rgen && chit && miss && call;

    de::MovePtr<RayTracingPipeline> rt_pipeline = de::newMovePtr<RayTracingPipeline>();

    int raygenGroup   = 0;
    int hitGroup      = -1;
    int missGroup     = -1;
    int callableGroup = -1;

    rt_pipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, shaderModules[0].get()->get(), raygenGroup);

    if (rgenRTTest)
    {
        hitGroup  = 1;
        missGroup = 2;
        rt_pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, shaderModules[2].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[3].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[4].get()->get(), missGroup);
    }
    else if (ahitTest)
    {
        hitGroup  = 1;
        missGroup = 2;
        rt_pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, shaderModules[2].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[3].get()->get(), missGroup);
    }
    else if (missTest)
    {
        hitGroup  = 1;
        missGroup = 2;
        rt_pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[2].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[3].get()->get(), missGroup);
    }
    else if (chitTest)
    {
        hitGroup  = 1;
        missGroup = 2;
        rt_pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[2].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[3].get()->get(), missGroup);
    }
    else if (isectTest)
    {
        hitGroup  = 1;
        missGroup = 2;
        rt_pipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[2].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[3].get()->get(), missGroup);
    }
    else if (callTest)
    {
        hitGroup      = 1;
        missGroup     = 2;
        callableGroup = 3;
        rt_pipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[1].get()->get(), hitGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[2].get()->get(), missGroup);
        rt_pipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, shaderModules[3].get()->get(), callableGroup);
    }

    Move<VkPipeline> pipeline = rt_pipeline->createPipeline(vkd, device, *pipelineLayout);

    de::MovePtr<BufferWithMemory> raygenShaderBindingTable = rt_pipeline->createShaderBindingTable(
        vkd, device, *pipeline, *state.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, raygenGroup, 1u);
    de::MovePtr<BufferWithMemory> missShaderBindingTable =
        missGroup > 0 ?
            rt_pipeline->createShaderBindingTable(vkd, device, *pipeline, *state.allocator, shaderGroupHandleSize,
                                                  shaderGroupBaseAlignment, missGroup, 1u) :
            de::MovePtr<BufferWithMemory>();
    de::MovePtr<BufferWithMemory> hitShaderBindingTable =
        hitGroup > 0 ?
            rt_pipeline->createShaderBindingTable(vkd, device, *pipeline, *state.allocator, shaderGroupHandleSize,
                                                  shaderGroupBaseAlignment, hitGroup, 1u) :
            de::MovePtr<BufferWithMemory>();
    de::MovePtr<BufferWithMemory> callableShaderBindingTable =
        callableGroup > 0 ?
            rt_pipeline->createShaderBindingTable(vkd, device, *pipeline, *state.allocator, shaderGroupHandleSize,
                                                  shaderGroupBaseAlignment, callableGroup, 1u) :
            de::MovePtr<BufferWithMemory>();

    VkStridedDeviceAddressRegionKHR raygenRegion =
        makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, (*raygenShaderBindingTable).get(), 0),
                                          shaderGroupHandleSize, shaderGroupHandleSize);
    VkStridedDeviceAddressRegionKHR missRegion =
        missGroup > 0 ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, (*missShaderBindingTable).get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            VkStridedDeviceAddressRegionKHR{0, 0, 0};
    VkStridedDeviceAddressRegionKHR hitRegion =
        hitGroup > 0 ?
            makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, (*hitShaderBindingTable).get(), 0),
                                              shaderGroupHandleSize, shaderGroupHandleSize) :
            VkStridedDeviceAddressRegionKHR{0, 0, 0};
    VkStridedDeviceAddressRegionKHR callableRegion =
        callableGroup > 0 ? makeStridedDeviceAddressRegionKHR(
                                getBufferDeviceAddress(vkd, device, (*callableShaderBindingTable).get(), 0),
                                shaderGroupHandleSize, shaderGroupHandleSize) :
                            VkStridedDeviceAddressRegionKHR{0, 0, 0};

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *state.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    de::MovePtr<BufferWithMemory> rayBuffer;

    if (params.rays.empty() == false)
    {
        const VkBufferCreateInfo rayBufferCreateInfo =
            makeBufferCreateInfo(params.rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        rayBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, allocator, rayBufferCreateInfo, MemoryRequirement::HostVisible));

        memcpy(rayBuffer->getAllocation().getHostPtr(), &params.rays[0], params.rays.size() * sizeof(Ray));
        flushMappedMemoryRange(vkd, device, rayBuffer->getAllocation().getMemory(),
                               rayBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
    }

    beginCommandBuffer(vkd, *cmdBuffer);

    // build acceleration structures for ray query
    initRayQueryAccelerationStructures(vkd, device, allocator, params, *cmdBuffer, rayQueryBottomAccelerationStructures,
                                       rayQueryTopAccelerationStructure);
    // build acceleration structures for trace
    std::vector<tcu::Vec3> geomData;
    switch (params.shaderSourceType)
    {
    case RayQueryShaderSourceType::MISS:
        geomData.push_back(tcu::Vec3(0, 0, -1));
        geomData.push_back(tcu::Vec3(1, 0, -1));
        geomData.push_back(tcu::Vec3(0, 1, -1));
        break;
    case RayQueryShaderSourceType::CLOSEST_HIT:
    case RayQueryShaderSourceType::CALLABLE:
        geomData.push_back(tcu::Vec3(0, 0, 1));
        geomData.push_back(tcu::Vec3(1, 0, 1));
        geomData.push_back(tcu::Vec3(0, 1, 1));
        break;
    case RayQueryShaderSourceType::ANY_HIT:
    case RayQueryShaderSourceType::INTERSECTION:
        geomData.push_back(tcu::Vec3(0, 0, 1));
        geomData.push_back(tcu::Vec3(0.5, 0.5, 1));
        break;
    default:
        break;
    }

    VkDescriptorBufferInfo resultBufferDesc = {(*resultData).get(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rayBufferDesc    = {(*rayBuffer).get(), 0, VK_WHOLE_SIZE};

    const TopLevelAccelerationStructure *rayQueryTopLevelAccelerationStructurePtr =
        rayQueryTopAccelerationStructure.get();
    VkWriteDescriptorSetAccelerationStructureKHR rayQueryAccelerationStructureWriteDescriptorSet = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
        DE_NULL,                                                           //  const void* pNext;
        1u,                                                                //  uint32_t accelerationStructureCount;
        rayQueryTopLevelAccelerationStructurePtr
            ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
    };

    VkWriteDescriptorSetAccelerationStructureKHR traceAccelerationStructureWriteDescriptorSet = {};
    if (geomData.size() > 0)
    {
        traceAccelerationStructure = makeTopLevelAccelerationStructure();
        traceAccelerationStructure->setInstanceCount(1);

        de::MovePtr<BottomLevelAccelerationStructure> traceBottomLevelAccelerationStructure =
            makeBottomLevelAccelerationStructure();

        traceBottomLevelAccelerationStructure->addGeometry(geomData, ((geomData.size() % 3) == 0), 0);
        traceBottomLevelAccelerationStructure->createAndBuild(vkd, device, *cmdBuffer, allocator);
        traceBottomAccelerationStructures.push_back(
            de::SharedPtr<BottomLevelAccelerationStructure>(traceBottomLevelAccelerationStructure.release()));
        traceAccelerationStructure->addInstance(traceBottomAccelerationStructures.back(), identityMatrix3x4, 0, 255U, 0,
                                                0);
        traceAccelerationStructure->createAndBuild(vkd, device, *cmdBuffer, allocator);

        const TopLevelAccelerationStructure *traceTopLevelAccelerationStructurePtr = traceAccelerationStructure.get();
        traceAccelerationStructureWriteDescriptorSet                               = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            DE_NULL, //  const void* pNext;
            1u,      //  uint32_t accelerationStructureCount;
            traceTopLevelAccelerationStructurePtr
                ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDesc)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                         &rayQueryAccelerationStructureWriteDescriptorSet)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayBufferDesc)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(3u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &traceAccelerationStructureWriteDescriptorSet)
            .update(vkd, device);
    }
    else
    {
        DescriptorSetUpdateBuilder()
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDesc)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                         &rayQueryAccelerationStructureWriteDescriptorSet)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayBufferDesc)
            .update(vkd, device);
    }

    VkDescriptorSet setHandle = descriptorSet.get();

    vkd.cmdBindPipeline(*cmdBuffer, state.pipelineBind, *pipeline);
    vkd.cmdBindDescriptorSets(*cmdBuffer, state.pipelineBind, *pipelineLayout, 0, 1, &setHandle, 0, DE_NULL);

    cmdTraceRays(vkd, *cmdBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
                 static_cast<uint32_t>(params.rays.size()), 1, 1);

    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, universalQueue, *cmdBuffer);

    invalidateMappedMemoryRange(vkd, device, resultData->getAllocation().getMemory(),
                                resultData->getAllocation().getOffset(), VK_WHOLE_SIZE);

    std::vector<T> results(params.rays.size());
    memcpy(&results[0], resultData->getAllocation().getHostPtr(), sizeof(T) * params.rays.size());

    rayQueryBottomAccelerationStructures.clear();
    rayQueryTopAccelerationStructure.clear();
    traceBottomAccelerationStructures.clear();
    traceAccelerationStructure.clear();

    return results;
}

template <typename T>
std::vector<T> rayQueryComputeTestSetup(const vk::DeviceInterface &vkd, const vk::VkDevice &device,
                                        vk::Allocator &allocator, const vk::InstanceInterface &instanceInterface,
                                        vk::VkPhysicalDevice physDevice, vk::BinaryCollection &binaryCollection,
                                        vk::VkQueue universalQueue, uint32_t universalQueueFamilyIndex,
                                        RayQueryTestParams params)
{
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> bottomAccelerationStructures;
    de::SharedPtr<TopLevelAccelerationStructure> topAccelerationStructure;

    RayQueryTestState state(vkd, device, instanceInterface, physDevice, universalQueueFamilyIndex);

    const DeviceInterface &vk = vkd;

    int power    = static_cast<int>(ceil(log2(params.rays.size())));
    power        = (power % 2 == 0) ? power : power + 1;
    const int sz = de::max<int>(static_cast<int>(pow(2, power)), 64);
    Ray ray      = Ray();

    for (int idx = static_cast<int>(params.rays.size()); idx < sz; ++idx)
    {
        params.rays.push_back(ray);
    }

    const VkBufferCreateInfo resultDataCreateInfo =
        makeBufferCreateInfo(params.rays.size() * sizeof(T), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> resultData = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, resultDataCreateInfo, MemoryRequirement::HostVisible));

    const Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device);
    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const Move<VkDescriptorSet> descriptorSet   = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
    const Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, descriptorSetLayout.get());

    const Unique<VkShaderModule> rayQueryModule(createShaderModule(vkd, device, binaryCollection.get("comp"), 0u));

    const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                             // const void* pNext;
        static_cast<VkPipelineShaderStageCreateFlags>(0u),   // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        *rayQueryModule,                                     // VkShaderModule module;
        "main",                                              // const char* pName;
        DE_NULL,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                        // const void* pNext;
        static_cast<VkPipelineCreateFlags>(0u),         // VkPipelineCreateFlags flags;
        pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
        *pipelineLayout,                                // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0,                                              // int32_t basePipelineIndex;
    };
    Move<VkPipeline> pipeline(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *state.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    de::MovePtr<BufferWithMemory> rayBuffer;

    if (params.rays.empty() == false)
    {
        const VkBufferCreateInfo rayBufferCreateInfo =
            makeBufferCreateInfo(params.rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        rayBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, allocator, rayBufferCreateInfo, MemoryRequirement::HostVisible));

        memcpy(rayBuffer->getAllocation().getHostPtr(), &params.rays[0], params.rays.size() * sizeof(Ray));
        flushMappedMemoryRange(vkd, device, rayBuffer->getAllocation().getMemory(),
                               rayBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
    }

    beginCommandBuffer(vk, *cmdBuffer);

    // build acceleration structures for ray query
    initRayQueryAccelerationStructures(vkd, device, allocator, params, *cmdBuffer, bottomAccelerationStructures,
                                       topAccelerationStructure);

    const TopLevelAccelerationStructure *rayQueryTopLevelAccelerationStructurePtr = topAccelerationStructure.get();
    VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
        DE_NULL,                                                           //  const void* pNext;
        1u,                                                                //  uint32_t accelerationStructureCount;
        rayQueryTopLevelAccelerationStructurePtr
            ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
    };

    VkDescriptorBufferInfo resultBufferDesc = {(*resultData).get(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rayBufferDesc    = {(*rayBuffer).get(), 0, VK_WHOLE_SIZE};

    DescriptorSetUpdateBuilder()
        .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDesc)
        .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
        .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayBufferDesc)
        .update(vk, device);

    VkDescriptorSet setHandle = descriptorSet.get();

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &setHandle, 0, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, static_cast<uint32_t>(params.rays.size()), 1, 1);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, universalQueue, *cmdBuffer);

    invalidateMappedMemoryRange(vk, device, resultData->getAllocation().getMemory(),
                                resultData->getAllocation().getOffset(), VK_WHOLE_SIZE);

    std::vector<T> results(params.rays.size());

    memcpy(&results[0], resultData->getAllocation().getHostPtr(), sizeof(T) * params.rays.size());

    topAccelerationStructure.clear();
    bottomAccelerationStructures.clear();

    return results;
}

template <typename T>
static std::vector<T> rayQueryGraphicsTestSetup(const DeviceInterface &vkd, const VkDevice device,
                                                const uint32_t queueFamilyIndex, Allocator &allocator,
                                                vk::BinaryCollection &binaryCollection, vk::VkQueue universalQueue,
                                                const vk::InstanceInterface &instanceInterface,
                                                vk::VkPhysicalDevice physDevice, RayQueryTestParams params)
{
    int width = static_cast<int>(params.rays.size());
    int power = static_cast<int>(ceil(log2(width)));
    power     = (power % 2 == 0) ? power : power + 1;
    int sz    = static_cast<int>(pow(2, power / 2));

    Ray ray           = Ray();
    const int totalSz = sz * sz;

    for (int idx = static_cast<int>(params.rays.size()); idx < totalSz; ++idx)
    {
        params.rays.push_back(ray);
    }

    const tcu::UVec2 renderSz = {static_cast<uint32_t>(sz), static_cast<uint32_t>(sz)};

    Move<VkDescriptorSetLayout> descriptorSetLayout;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;
    Move<VkPipeline> pipeline;
    std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> rayQueryBottomAccelerationStructures;
    de::SharedPtr<TopLevelAccelerationStructure> rayQueryTopAccelerationStructure;

    descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build(vkd, device);
    descriptorPool = DescriptorPoolBuilder()
                         .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                         .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                         .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                         .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    descriptorSet  = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());
    pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

    const std::map<RayQueryShaderSourceType, std::vector<std::string>> shaderNames = {
        //idx: 0                1                2                3                4
        //shader: vert, tesc, tese, geom, frag,
        {RayQueryShaderSourceType::VERTEX,
         {
             "vert",
             "",
             "",
             "",
             "",
         }},
        {RayQueryShaderSourceType::TESSELLATION_CONTROL,
         {
             "vert",
             "tesc",
             "tese",
             "",
             "",
         }},
        {RayQueryShaderSourceType::TESSELLATION_EVALUATION,
         {
             "vert",
             "tesc",
             "tese",
             "",
             "",
         }},
        {RayQueryShaderSourceType::GEOMETRY,
         {
             "vert",
             "",
             "",
             "geom",
             "",
         }},
        {RayQueryShaderSourceType::FRAGMENT,
         {
             "vert",
             "",
             "",
             "",
             "frag",
         }},
    };

    auto shaderNameIt = shaderNames.find(params.shaderSourceType);
    if (shaderNameIt == end(shaderNames))
        TCU_THROW(InternalError, "Wrong shader source type");

    std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos;
    std::vector<de::SharedPtr<Move<VkShaderModule>>> shaderModules;
    bool tescX, teseX, fragX;
    registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                 VK_SHADER_STAGE_VERTEX_BIT, shaderNameIt->second[0]);
    tescX = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderNameIt->second[1]);
    teseX = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderNameIt->second[2]);
    registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                 VK_SHADER_STAGE_GEOMETRY_BIT, shaderNameIt->second[3]);
    fragX = registerRayQueryShaderModule(vkd, device, binaryCollection, shaderModules, shaderCreateInfos,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, shaderNameIt->second[4]);

    const vk::VkSubpassDescription subpassDesc = {
        (vk::VkSubpassDescriptionFlags)0,
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
        0u,                                  // inputCount
        DE_NULL,                             // pInputAttachments
        0u,                                  // colorCount
        DE_NULL,                             // pColorAttachments
        DE_NULL,                             // pResolveAttachments
        DE_NULL,                             // depthStencilAttachment
        0u,                                  // preserveCount
        DE_NULL,                             // pPreserveAttachments
    };
    const vk::VkRenderPassCreateInfo renderPassParams = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (vk::VkRenderPassCreateFlags)0,
        0u,           // attachmentCount
        DE_NULL,      // pAttachments
        1u,           // subpassCount
        &subpassDesc, // pSubpasses
        0u,           // dependencyCount
        DE_NULL,      // pDependencies
    };

    renderPass = createRenderPass(vkd, device, &renderPassParams);

    const vk::VkFramebufferCreateInfo framebufferParams = {
        vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (vk::VkFramebufferCreateFlags)0,
        *renderPass, // renderPass
        0u,          // attachmentCount
        DE_NULL,     // pAttachments
        renderSz[0], // width
        renderSz[1], // height
        1u,          // layers
    };

    framebuffer = createFramebuffer(vkd, device, &framebufferParams);

    VkPrimitiveTopology testTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    std::vector<tcu::Vec3> vertices;

    switch (params.shaderSourceType)
    {
    case RayQueryShaderSourceType::TESSELLATION_CONTROL:
    case RayQueryShaderSourceType::TESSELLATION_EVALUATION:
    case RayQueryShaderSourceType::VERTEX:
    case RayQueryShaderSourceType::GEOMETRY:
    {
        if ((params.shaderSourceType == RayQueryShaderSourceType::VERTEX) ||
            (params.shaderSourceType == RayQueryShaderSourceType::GEOMETRY))
        {
            testTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
        else
        {
            testTopology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        }
        const int numTriangles = static_cast<int>(params.rays.size());
        const float halfStepSz = 1.f / (static_cast<float>(numTriangles) * 2.f);
        float startX           = 0.0;
        for (int index = 0; index < numTriangles; ++index)
        {
            vertices.push_back(tcu::Vec3(startX, 0.0, static_cast<float>(index)));
            startX += halfStepSz;
            vertices.push_back(tcu::Vec3(startX, 1.0, static_cast<float>(index)));
            startX += halfStepSz;
            vertices.push_back(tcu::Vec3(startX, 0.0, static_cast<float>(index)));
        }
        break;
    }
    case RayQueryShaderSourceType::FRAGMENT:
        vertices.push_back(tcu::Vec3(-1.0f, -1.0f, 0.0f));
        vertices.push_back(tcu::Vec3(1.0f, -1.0f, 0.0f));
        vertices.push_back(tcu::Vec3(-1.0f, 1.0f, 0.0f));
        vertices.push_back(tcu::Vec3(1.0f, 1.0f, 0.0f));
        break;
    default:
        TCU_THROW(InternalError, "Wrong shader source type");
    };

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t binding;
        sizeof(tcu::Vec3),           // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                         // uint32_t location;
        0u,                         // uint32_t binding;
        VK_FORMAT_R32G32B32_SFLOAT, // VkFormat format;
        0u,                         // uint32_t offset;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                   // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,                              // uint32_t vertexAttributeDescriptionCount;
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                     // const void* pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0,                  // VkPipelineInputAssemblyStateCreateFlags flags;
        testTopology,                                                // VkPrimitiveTopology topology;
        VK_FALSE                                                     // VkBool32 primitiveRestartEnable;
    };

    const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                   // const void* pNext;
        VkPipelineTessellationStateCreateFlags(0u),                // VkPipelineTessellationStateCreateFlags flags;
        3u                                                         // uint32_t patchControlPoints;
    };

    VkViewport viewport = makeViewport(renderSz[0], renderSz[1]);
    VkRect2D scissor    = makeRect2D(renderSz[0], renderSz[1]);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                                    sType
        DE_NULL,                               // const void*                                        pNext
        (VkPipelineViewportStateCreateFlags)0, // VkPipelineViewportStateCreateFlags                flags
        1u,                                    // uint32_t                                            viewportCount
        &viewport,                             // const VkViewport*                                pViewports
        1u,                                    // uint32_t                                            scissorCount
        &scissor                               // const VkRect2D*                                    pScissors
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                    // const void* pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        fragX ? VK_FALSE : VK_TRUE,                                 // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE,                                    // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f                                                        // float lineWidth;
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        DE_NULL,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        false,                                                    // VkBool32 logicOpEnable;
        VK_LOGIC_OP_CLEAR,                                        // VkLogicOp logicOp;
        0,                                                        // uint32_t attachmentCount;
        DE_NULL,                 // const VkPipelineColorBlendAttachmentState* pAttachments;
        {1.0f, 1.0f, 1.0f, 1.0f} // float blendConstants[4];
    };

    const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
        static_cast<uint32_t>(shaderCreateInfos.size()), // uint32_t stageCount;
        shaderCreateInfos.data(),                        // const VkPipelineShaderStageCreateInfo* pStages;
        &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
        &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
        (tescX || teseX) ? &tessellationStateCreateInfo :
                           DE_NULL,                 // const VkPipelineTessellationStateCreateInfo* pTessellationState;
        fragX ? &viewportStateCreateInfo : DE_NULL, // const VkPipelineViewportStateCreateInfo* pViewportState;
        &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
        fragX ? &multisampleStateCreateInfo : DE_NULL, // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
        DE_NULL, // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
        fragX ? &colorBlendStateCreateInfo : DE_NULL, // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
        DE_NULL,                                      // const VkPipelineDynamicStateCreateInfo* pDynamicState;
        pipelineLayout.get(),                         // VkPipelineLayout layout;
        renderPass.get(),                             // VkRenderPass renderPass;
        0u,                                           // uint32_t subpass;
        VK_NULL_HANDLE,                               // VkPipeline basePipelineHandle;
        0                                             // int basePipelineIndex;
    };

    pipeline = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);

    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                 // VkStructureType sType;
        DE_NULL,                                                              // const void* pNext;
        0u,                                                                   // VkBufferCreateFlags flags;
        VkDeviceSize(sizeof(tcu::Vec3) * vertices.size()),                    // VkDeviceSize size;
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                            // VkSharingMode sharingMode;
        1u,                                                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                                                     // const uint32_t* pQueueFamilyIndices;
    };

    Move<VkBuffer> vertexBuffer;
    de::MovePtr<Allocation> vertexAlloc;

    vertexBuffer = createBuffer(vkd, device, &vertexBufferParams);
    vertexAlloc =
        allocator.allocate(getBufferMemoryRequirements(vkd, device, *vertexBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vkd.bindBufferMemory(device, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

    // Upload vertex data
    deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(tcu::Vec3));
    flushAlloc(vkd, device, *vertexAlloc);

    RayQueryTestState state(vkd, device, instanceInterface, physDevice, queueFamilyIndex);

    de::MovePtr<BufferWithMemory> rayBuffer;

    if (params.rays.empty() == false)
    {
        const VkBufferCreateInfo rayBufferCreateInfo =
            makeBufferCreateInfo(params.rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        rayBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, allocator, rayBufferCreateInfo, MemoryRequirement::HostVisible));

        memcpy(rayBuffer->getAllocation().getHostPtr(), &params.rays[0], params.rays.size() * sizeof(Ray));
        flushMappedMemoryRange(vkd, device, rayBuffer->getAllocation().getMemory(),
                               rayBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
    }

    const VkQueue queue                     = universalQueue;
    const VkFormat imageFormat              = VK_FORMAT_R32G32B32A32_SFLOAT;
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,       // VkStructureType sType;
        DE_NULL,                                   // const void* pNext;
        (VkImageCreateFlags)0u,                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_3D,                          // VkImageType imageType;
        imageFormat,                               // VkFormat format;
        makeExtent3D(renderSz[0], renderSz[1], 1), // VkExtent3D extent;
        1u,                                        // uint32_t mipLevels;
        1u,                                        // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                     // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                   // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        DE_NULL,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    const Move<VkImageView> imageView =
        makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);

    const VkBufferCreateInfo resultBufferCreateInfo =
        makeBufferCreateInfo(renderSz[0] * renderSz[1] * 1 * 4 * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkImageSubresourceLayers resultBufferImageSubresourceLayers =
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const VkBufferImageCopy resultBufferImageRegion =
        makeBufferImageCopy(makeExtent3D(renderSz[0], renderSz[1], 1), resultBufferImageSubresourceLayers);
    de::MovePtr<BufferWithMemory> resultBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

    const VkDescriptorImageInfo resultImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);

    const Move<VkCommandPool> cmdPool = createCommandPool(vkd, device, 0, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkDescriptorBufferInfo rayBufferDescriptorInfo =
        makeDescriptorBufferInfo((*rayBuffer).get(), 0, VK_WHOLE_SIZE);

    beginCommandBuffer(vkd, *cmdBuffer, 0u);
    {
        const VkImageMemoryBarrier preImageBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);

        const VkClearValue clearValue = makeClearValueColorU32(0xFF, 0u, 0u, 0u);
        vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1,
                               &imageSubresourceRange);

        const VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);
        cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, SHADER_STAGE_ALL_RAY_TRACING,
                                      &postImageBarrier);

        // build acceleration structures for ray query
        initRayQueryAccelerationStructures(vkd, device, allocator, params, *cmdBuffer,
                                           rayQueryBottomAccelerationStructures, rayQueryTopAccelerationStructure);

        const TopLevelAccelerationStructure *rayQueryTopLevelAccelerationStructurePtr =
            rayQueryTopAccelerationStructure.get();
        VkWriteDescriptorSetAccelerationStructureKHR rayQueryAccelerationStructureWriteDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
            DE_NULL,                                                           //  const void* pNext;
            1u,                                                                //  uint32_t accelerationStructureCount;
            rayQueryTopLevelAccelerationStructurePtr
                ->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
        };

        DescriptorSetUpdateBuilder()
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                         &rayQueryAccelerationStructureWriteDescriptorSet)
            .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayBufferDescriptorInfo)
            .update(vkd, device);

        const VkRenderPassBeginInfo renderPassBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
            DE_NULL,                                  // const void* pNext;
            renderPass.get(),                         // VkRenderPass renderPass;
            framebuffer.get(),                        // VkFramebuffer framebuffer;
            makeRect2D(renderSz[0], renderSz[1]),     // VkRect2D renderArea;
            0u,                                       // uint32_t clearValueCount;
            DE_NULL                                   // const VkClearValue* pClearValues;
        };
        VkDeviceSize vertexBufferOffset = 0u;

        vkd.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &descriptorSet.get(), 0u, DE_NULL);
        vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
        vkd.cmdDraw(*cmdBuffer, uint32_t(vertices.size()), 1, 0, 0);
        vkd.cmdEndRenderPass(*cmdBuffer);

        const VkMemoryBarrier postTestMemoryBarrier =
            makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(vkd, *cmdBuffer, SHADER_STAGE_ALL_RAY_TRACING, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 &postTestMemoryBarrier);

        vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u,
                                 &resultBufferImageRegion);
    }
    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

    invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(),
                                resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

    rayQueryBottomAccelerationStructures.clear();
    rayQueryTopAccelerationStructure.clear();

    std::vector<T> results;
    const uint32_t depth = 1;

    // create result image
    tcu::TextureFormat imageFormatMapped = vk::mapVkFormat(imageFormat);
    tcu::ConstPixelBufferAccess resultAccess(imageFormatMapped, renderSz[0], renderSz[1], depth,
                                             resultBuffer->getAllocation().getHostPtr());

    for (uint32_t z = 0; z < depth; z++)
    {
        for (uint32_t y = 0; y < renderSz[1]; y++)
        {
            for (uint32_t x = 0; x < renderSz[0]; x++)
            {
                tcu::Vec4 pixel = resultAccess.getPixel(x, y, z);
                T resData       = {pixel[0], pixel[1], pixel[2], pixel[3]};
                results.push_back(resData);
                if (results.size() >= params.rays.size())
                {
                    return (results);
                }
            }
        }
    }

    return results;
}

void generateRayQueryShaders(SourceCollections &programCollection, RayQueryTestParams params, std::string rayQueryPart,
                             float max_t);

#else

uint32_t rayTracingDefineAnything();

#endif // CTS_USES_VULKANSC

} // namespace vk

#endif // _VKRAYTRACINGUTIL_HPP
