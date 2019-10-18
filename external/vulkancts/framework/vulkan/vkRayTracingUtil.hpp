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

namespace vk
{
const VkTransformMatrixKHR identityMatrix3x4 = { { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } } };

template<typename T>
inline de::SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
	return de::SharedPtr<Move<T>>(new Move<T>(move));
}

template<typename T>
inline de::SharedPtr<de::MovePtr<T> > makeVkSharedPtr(de::MovePtr<T> movePtr)
{
	return de::SharedPtr<de::MovePtr<T> >(new de::MovePtr<T>(movePtr));
}

template<typename T>
inline const T* dataOrNullPtr(const std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : v.data());
}

template<typename T>
inline T* dataOrNullPtr(std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : v.data());
}

inline std::string updateRayTracingGLSL (const std::string& str)
{
	return str;
}

std::string getCommonRayGenerationShader (void);

const char* getRayTracingExtensionUsed (void);

class RaytracedGeometryBase
{
public:
								RaytracedGeometryBase			()										= delete;
								RaytracedGeometryBase			(const RaytracedGeometryBase& geometry)	= delete;
								RaytracedGeometryBase			(VkGeometryTypeKHR geometryType, VkFormat vertexFormat, VkIndexType indexType);
								virtual ~RaytracedGeometryBase	();

	inline VkGeometryTypeKHR	getGeometryType					(void) const								{ return m_geometryType; }
	inline bool					isTrianglesType					(void) const								{ return m_geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR; }
	inline VkFormat				getVertexFormat					(void) const								{ return m_vertexFormat; }
	inline VkIndexType			getIndexType					(void) const								{ return m_indexType; }
	inline bool					usesIndices						(void) const								{ return m_indexType != VK_INDEX_TYPE_NONE_KHR; }
	inline VkGeometryFlagsKHR	getGeometryFlags				(void) const								{ return m_geometryFlags; }
	inline void					setGeometryFlags				(const VkGeometryFlagsKHR geometryFlags)	{ m_geometryFlags = geometryFlags; }
	virtual deUint32			getVertexCount					(void) const								= 0;
	virtual const deUint8*		getVertexPointer				(void) const								= 0;
	virtual VkDeviceSize		getVertexStride					(void) const								= 0;
	virtual size_t				getVertexByteSize				(void) const								= 0;
	virtual VkDeviceSize		getAABBStride					(void) const								= 0;
	virtual deUint32			getIndexCount					(void) const								= 0;
	virtual const deUint8*		getIndexPointer					(void) const								= 0;
	virtual VkDeviceSize		getIndexStride					(void) const								= 0;
	virtual size_t				getIndexByteSize				(void) const								= 0;
	virtual deUint32			getPrimitiveCount				(void) const								= 0;
	virtual void				addVertex						(const tcu::Vec3& vertex)					= 0;
	virtual void				addIndex						(const deUint32& index)						= 0;
private:
	VkGeometryTypeKHR			m_geometryType;
	VkFormat					m_vertexFormat;
	VkIndexType					m_indexType;
	VkGeometryFlagsKHR			m_geometryFlags;
};

template <typename T>
inline T convertSatRte (float f)
{
	// \note Doesn't work for 64-bit types
	DE_STATIC_ASSERT(sizeof(T) < sizeof(deUint64));
	DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

	deInt64	minVal	= std::numeric_limits<T>::min();
	deInt64 maxVal	= std::numeric_limits<T>::max();
	float	q		= deFloatFrac(f);
	deInt64 intVal	= (deInt64)(f-q);

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

inline deInt16 deFloat32ToSNorm16 (float src)
{
	const deInt16	range = (deInt32)((1u << 15) - 1u);
	const deInt16	intVal = convertSatRte<deInt16>(src * (float)range);
	return de::clamp<deInt16>(intVal, -range, range);
}

typedef tcu::Vector<deFloat16, 2>			Vec2_16;
typedef tcu::Vector<deFloat16, 4>			Vec4_16;
typedef tcu::Vector<deInt16, 2>				Vec2_16SNorm;
typedef tcu::Vector<deInt16, 4>				Vec4_16SNorm;

template<typename V>	VkFormat			vertexFormatFromType				()							{ TCU_THROW(TestError, "Unknown VkFormat"); }
template<>				inline VkFormat		vertexFormatFromType<tcu::Vec2>		()							{ return VK_FORMAT_R32G32_SFLOAT; }
template<>				inline VkFormat		vertexFormatFromType<tcu::Vec3>		()							{ return VK_FORMAT_R32G32B32_SFLOAT; }
template<>				inline VkFormat		vertexFormatFromType<Vec2_16>		()							{ return VK_FORMAT_R16G16_SFLOAT; }
template<>				inline VkFormat		vertexFormatFromType<Vec4_16>		()							{ return VK_FORMAT_R16G16B16A16_SFLOAT; }
template<>				inline VkFormat		vertexFormatFromType<Vec2_16SNorm>	()							{ return VK_FORMAT_R16G16_SNORM; }
template<>				inline VkFormat		vertexFormatFromType<Vec4_16SNorm>	()							{ return VK_FORMAT_R16G16B16A16_SNORM; }

struct EmptyIndex {};
template<typename I>	VkIndexType			indexTypeFromType					()							{ TCU_THROW(TestError, "Unknown VkIndexType"); }
template<>				inline VkIndexType	indexTypeFromType<deUint16>			()							{ return VK_INDEX_TYPE_UINT16; }
template<>				inline VkIndexType	indexTypeFromType<deUint32>			()							{ return VK_INDEX_TYPE_UINT32; }
template<>				inline VkIndexType	indexTypeFromType<EmptyIndex>		()							{ return VK_INDEX_TYPE_NONE_KHR; }

template<typename V>	V					convertFloatTo						(const tcu::Vec3& vertex)	{ DE_UNREF(vertex); TCU_THROW(TestError, "Unknown data format"); }
template<>				inline tcu::Vec2	convertFloatTo<tcu::Vec2>			(const tcu::Vec3& vertex)	{ return tcu::Vec2(vertex.x(), vertex.y()); }
template<>				inline tcu::Vec3	convertFloatTo<tcu::Vec3>			(const tcu::Vec3& vertex)	{ return vertex; }
template<>				inline Vec2_16		convertFloatTo<Vec2_16>				(const tcu::Vec3& vertex)	{ return Vec2_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y())); }
template<>				inline Vec4_16		convertFloatTo<Vec4_16>				(const tcu::Vec3& vertex)	{ return Vec4_16(deFloat32To16(vertex.x()), deFloat32To16(vertex.y()), deFloat32To16(vertex.z()), deFloat32To16(0.0f)); }
template<>				inline Vec2_16SNorm	convertFloatTo<Vec2_16SNorm>		(const tcu::Vec3& vertex)	{ return Vec2_16SNorm(deFloat32ToSNorm16(vertex.x()), deFloat32ToSNorm16(vertex.y())); }
template<>				inline Vec4_16SNorm	convertFloatTo<Vec4_16SNorm>		(const tcu::Vec3& vertex)	{ return Vec4_16SNorm(deFloat32ToSNorm16(vertex.x()), deFloat32ToSNorm16(vertex.y()), deFloat32ToSNorm16(vertex.z()), deFloat32ToSNorm16(0.0f)); }

template<typename V>	V					convertIndexTo						(deUint32 index)			{ DE_UNREF(index); TCU_THROW(TestError, "Unknown index format"); }
template<>				inline EmptyIndex	convertIndexTo<EmptyIndex>			(deUint32 index)			{ DE_UNREF(index); TCU_THROW(TestError, "Cannot add empty index"); }
template<>				inline deUint16		convertIndexTo<deUint16>			(deUint32 index)			{ return static_cast<deUint16>(index); }
template<>				inline deUint32		convertIndexTo<deUint32>			(deUint32 index)			{ return index; }

template<typename V, typename I>
class RaytracedGeometry : public RaytracedGeometryBase
{
public:
						RaytracedGeometry			()									= delete;
						RaytracedGeometry			(const RaytracedGeometry& geometry)	= delete;
						RaytracedGeometry			(VkGeometryTypeKHR geometryType);
						RaytracedGeometry			(VkGeometryTypeKHR geometryType, const std::vector<V>& vertices, const std::vector<I>& indices = std::vector<I>());

	deUint32			getVertexCount				(void) const override;
	const deUint8*		getVertexPointer			(void) const override;
	VkDeviceSize		getVertexStride				(void) const override;
	size_t				getVertexByteSize			(void) const override;
	VkDeviceSize		getAABBStride				(void) const override;
	deUint32			getIndexCount				(void) const override;
	const deUint8*		getIndexPointer				(void) const override;
	VkDeviceSize		getIndexStride				(void) const override;
	size_t				getIndexByteSize			(void) const override;
	deUint32			getPrimitiveCount			(void) const override;

	void				addVertex					(const tcu::Vec3& vertex) override;
	void				addIndex					(const deUint32& index) override;
private:
	std::vector<V>		m_vertices;
	std::vector<I>		m_indices;
};

template<typename V, typename I>
RaytracedGeometry<V, I>::RaytracedGeometry (VkGeometryTypeKHR geometryType)
	: RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
{
}

template<typename V, typename I>
RaytracedGeometry<V,I>::RaytracedGeometry (VkGeometryTypeKHR geometryType, const std::vector<V>& vertices, const std::vector<I>& indices)
	: RaytracedGeometryBase(geometryType, vertexFormatFromType<V>(), indexTypeFromType<I>())
	, m_vertices(vertices)
	, m_indices(indices)
{
}

template<typename V, typename I>
deUint32 RaytracedGeometry<V,I>::getVertexCount (void) const
{
	return static_cast<deUint32>( isTrianglesType() ? m_vertices.size() : 0);
}

template<typename V, typename I>
const deUint8* RaytracedGeometry<V, I>::getVertexPointer (void) const
{
	return reinterpret_cast<const deUint8*>(m_vertices.empty() ? DE_NULL : m_vertices.data());
}

template<typename V, typename I>
VkDeviceSize RaytracedGeometry<V,I>::getVertexStride (void) const
{
	return static_cast<VkDeviceSize>(sizeof(V));
}

template<typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getAABBStride (void) const
{
	return static_cast<VkDeviceSize>(2 * sizeof(V));
}

template<typename V, typename I>
size_t RaytracedGeometry<V, I>::getVertexByteSize (void) const
{
	return static_cast<size_t>(m_vertices.size() * sizeof(V));
}

template<typename V, typename I>
deUint32 RaytracedGeometry<V, I>::getIndexCount (void) const
{
	return static_cast<deUint32>(isTrianglesType() ? m_indices.size() : 0);
}

template<typename V, typename I>
const deUint8* RaytracedGeometry<V, I>::getIndexPointer (void) const
{
	return reinterpret_cast<const deUint8*>(m_indices.empty() ? DE_NULL : m_indices.data());
}

template<typename V, typename I>
VkDeviceSize RaytracedGeometry<V, I>::getIndexStride (void) const
{
	return static_cast<VkDeviceSize>(sizeof(I));
}

template<typename V, typename I>
size_t RaytracedGeometry<V, I>::getIndexByteSize (void) const
{
	return static_cast<size_t>(m_indices.size() * sizeof(I));
}

template<typename V, typename I>
deUint32 RaytracedGeometry<V,I>::getPrimitiveCount (void) const
{
	return static_cast<deUint32>(isTrianglesType() ? (usesIndices() ? m_indices.size() / 3 : m_vertices.size() / 3) : (m_vertices.size() / 2));
}

template<typename V, typename I>
void RaytracedGeometry<V, I>::addVertex (const tcu::Vec3& vertex)
{
	m_vertices.push_back(convertFloatTo<V>(vertex));
}

template<typename V, typename I>
void RaytracedGeometry<V, I>::addIndex (const deUint32& index)
{
	m_indices.push_back(convertIndexTo<I>(index));
}

de::SharedPtr<RaytracedGeometryBase> makeRaytracedGeometry (VkGeometryTypeKHR geometryType, VkFormat vertexFormat, VkIndexType indexType);

class SerialStorage
{
public:
											SerialStorage		() = delete;
											SerialStorage		(const DeviceInterface&						vk,
																 const VkDevice								device,
																 Allocator&									allocator,
																 const VkAccelerationStructureBuildTypeKHR	buildType,
																 const VkDeviceSize							storageSize);

	VkDeviceOrHostAddressKHR				getAddress			(const DeviceInterface&	vk,
																 const VkDevice			device);
	VkDeviceOrHostAddressConstKHR			getAddressConst		(const DeviceInterface&	vk,
																 const VkDevice			device);
protected:
	VkAccelerationStructureBuildTypeKHR		m_buildType;
	de::MovePtr<BufferWithMemory>			m_buffer;

};

class BottomLevelAccelerationStructure
{
public:
	static deUint32										getRequiredAllocationCount				(void);

														BottomLevelAccelerationStructure		();
														BottomLevelAccelerationStructure		(const BottomLevelAccelerationStructure&	other) = delete;
	virtual												~BottomLevelAccelerationStructure		();

	virtual void										setGeometryData							(const std::vector<tcu::Vec3>&				geometryData,
																								 const bool									triangles,
																								 const VkGeometryFlagsKHR					geometryFlags = 0);
	virtual void										setDefaultGeometryData					(const VkShaderStageFlagBits				testStage);
	virtual void										setGeometryCount						(const size_t								geometryCount);
	virtual void										addGeometry								(de::SharedPtr<RaytracedGeometryBase>&		raytracedGeometry);
	virtual void										addGeometry								(const std::vector<tcu::Vec3>&				geometryData,
																								 const bool									triangles,
																								 const VkGeometryFlagsKHR					geometryFlags = 0);

	virtual void										setBuildType							(const VkAccelerationStructureBuildTypeKHR	buildType) = DE_NULL;
	virtual void										setBuildFlags							(const VkBuildAccelerationStructureFlagsKHR	flags) = DE_NULL;
	virtual void										setDeferredOperation					(const bool									deferredOperation) = DE_NULL;
	virtual void										setUseArrayOfPointers					(const bool									useArrayOfPointers) = DE_NULL;
	virtual void										setIndirectBuildParameters				(const VkBuffer								indirectBuffer,
																								 const VkDeviceSize							indirectBufferOffset,
																								 const deUint32								indirectBufferStride) = DE_NULL;
	virtual VkBuildAccelerationStructureFlagsKHR		getBuildFlags							() const = DE_NULL;

	// methods specific for each acceleration structure
	virtual void										create									(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 Allocator&									allocator,
																								 VkDeviceAddress							deviceAddress,
																								 VkDeviceSize								compactCopySize) = DE_NULL;
	virtual void										build									(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer) = DE_NULL;
	virtual void										copyFrom								(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 BottomLevelAccelerationStructure*			accelerationStructure,
																								 VkDeviceSize								compactCopySize) = DE_NULL;

	virtual void										serialize								(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 SerialStorage*								storage) = DE_NULL;
	virtual void										deserialize								(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 SerialStorage*								storage) = DE_NULL;

	// helper methods for typical acceleration structure creation tasks
	void												createAndBuild							(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 Allocator&									allocator,
																								 VkDeviceAddress							deviceAddress			= 0u );
	void												createAndCopyFrom						(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 Allocator&									allocator,
																								 VkDeviceAddress							deviceAddress			= 0u,
																								 BottomLevelAccelerationStructure*			accelerationStructure	= DE_NULL,
																								 VkDeviceSize								compactCopySize			= 0u);
	void												createAndDeserializeFrom				(const DeviceInterface&						vk,
																								 const VkDevice								device,
																								 const VkCommandBuffer						cmdBuffer,
																								 Allocator&									allocator,
																								 VkDeviceAddress							deviceAddress			= 0u,
																								 SerialStorage*								storage					= DE_NULL);

	virtual const VkAccelerationStructureKHR*			getPtr									(void) const = DE_NULL;
protected:
	std::vector<de::SharedPtr<RaytracedGeometryBase>>	m_geometriesData;
};

de::MovePtr<BottomLevelAccelerationStructure> makeBottomLevelAccelerationStructure ();

struct InstanceData
{
								InstanceData (VkTransformMatrixKHR							matrix_,
											  deUint32										instanceCustomIndex_,
											  deUint32										mask_,
											  deUint32										instanceShaderBindingTableRecordOffset_,
											  VkGeometryInstanceFlagsKHR					flags_)
									: matrix(matrix_), instanceCustomIndex(instanceCustomIndex_), mask(mask_), instanceShaderBindingTableRecordOffset(instanceShaderBindingTableRecordOffset_), flags(flags_)
								{
								}
	VkTransformMatrixKHR		matrix;
	deUint32					instanceCustomIndex;
	deUint32					mask;
	deUint32					instanceShaderBindingTableRecordOffset;
	VkGeometryInstanceFlagsKHR	flags;
};

class TopLevelAccelerationStructure
{
public:
	static deUint32													getRequiredAllocationCount			(void);

																	TopLevelAccelerationStructure		();
																	TopLevelAccelerationStructure		(const TopLevelAccelerationStructure&				other) = delete;
	virtual															~TopLevelAccelerationStructure		();

	virtual void													setInstanceCount					(const size_t										instanceCount);
	virtual void													addInstance							(de::SharedPtr<BottomLevelAccelerationStructure>	bottomLevelStructure,
																										 const VkTransformMatrixKHR&						matrix									= identityMatrix3x4,
																										 deUint32											instanceCustomIndex						= 0,
																										 deUint32											mask									= 0xFF,
																										 deUint32											instanceShaderBindingTableRecordOffset	= 0,
																										 VkGeometryInstanceFlagsKHR							flags									= VkGeometryInstanceFlagBitsKHR(0u)	);

	virtual void													setBuildType						(const VkAccelerationStructureBuildTypeKHR			buildType) = DE_NULL;
	virtual void													setBuildFlags						(const VkBuildAccelerationStructureFlagsKHR			flags) = DE_NULL;
	virtual void													setDeferredOperation				(const bool											deferredOperation) = DE_NULL;
	virtual void													setUseArrayOfPointers				(const bool											useArrayOfPointers) = DE_NULL;
	virtual void													setIndirectBuildParameters			(const VkBuffer										indirectBuffer,
																										 const VkDeviceSize									indirectBufferOffset,
																										 const deUint32										indirectBufferStride) = DE_NULL;
	virtual VkBuildAccelerationStructureFlagsKHR					getBuildFlags						() const = DE_NULL;

	// methods specific for each acceleration structure
	virtual void													create								(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 Allocator&									allocator,
																										 VkDeviceAddress							deviceAddress,
																										 VkDeviceSize								compactCopySize) = DE_NULL;
	virtual void													build								(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer) = DE_NULL;
	virtual void													copyFrom							(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 TopLevelAccelerationStructure*				accelerationStructure,
																										 VkDeviceSize								compactCopySize) = DE_NULL;

	virtual void													serialize							(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 SerialStorage*								storage) = DE_NULL;
	virtual void													deserialize							(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 SerialStorage*								storage) = DE_NULL;

	// helper methods for typical acceleration structure creation tasks
	void															createAndBuild						(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 Allocator&									allocator,
																										 VkDeviceAddress							deviceAddress			= 0u );
	void															createAndCopyFrom					(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 Allocator&									allocator,
																										 VkDeviceAddress							deviceAddress			= 0u,
																										 TopLevelAccelerationStructure*				accelerationStructure	= DE_NULL,
																										 VkDeviceSize								compactCopySize			= 0u);
	void															createAndDeserializeFrom			(const DeviceInterface&						vk,
																										 const VkDevice								device,
																										 const VkCommandBuffer						cmdBuffer,
																										 Allocator&									allocator,
																										 VkDeviceAddress							deviceAddress			= 0u,
																										 SerialStorage*								storage					= DE_NULL);

	virtual const VkAccelerationStructureKHR*						getPtr								(void) const = DE_NULL;

protected:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	m_bottomLevelInstances;
	std::vector<InstanceData>										m_instanceData;
};

de::MovePtr<TopLevelAccelerationStructure> makeTopLevelAccelerationStructure ();

bool queryAccelerationStructureSize (const DeviceInterface&							vk,
									 const VkDevice									device,
									 const VkCommandBuffer							cmdBuffer,
									 const std::vector<VkAccelerationStructureKHR>&	accelerationStructureHandles,
									 VkAccelerationStructureBuildTypeKHR			buildType,
									 const VkQueryPool								queryPool,
									 VkQueryType									queryType,
									 deUint32										firstQuery,
									 std::vector<VkDeviceSize>&						results);

class RayTracingPipeline
{
public:
																RayTracingPipeline			();
																~RayTracingPipeline			();

	void														addShader					(VkShaderStageFlagBits									shaderStage,
																							 Move<VkShaderModule>									shaderModule,
																							 deUint32												group);
	void														addLibrary					(de::SharedPtr<de::MovePtr<RayTracingPipeline>>			pipelineLibrary);
	Move<VkPipeline>											createPipeline				(const DeviceInterface&									vk,
																							 const VkDevice											device,
																							 const VkPipelineLayout									pipelineLayout,
																							 const std::vector<de::SharedPtr<Move<VkPipeline>>>&	pipelineLibraries			= std::vector<de::SharedPtr<Move<VkPipeline>>>());
	std::vector<de::SharedPtr<Move<VkPipeline>>>				createPipelineWithLibraries	(const DeviceInterface&									vk,
																							 const VkDevice											device,
																							 const VkPipelineLayout									pipelineLayout);
	de::MovePtr<BufferWithMemory>								createShaderBindingTable	(const DeviceInterface&									vk,
																							 const VkDevice											device,
																							 const VkPipeline										pipeline,
																							 Allocator&												allocator,
																							 const deUint32&										shaderGroupHandleSize,
																							 const deUint32											shaderGroupBaseAlignment,
																							 const deUint32&										firstGroup,
																							 const deUint32&										groupCount,
																							 const VkBufferCreateFlags&								additionalBufferCreateFlags	= VkBufferCreateFlags(0u),
																							 const VkBufferUsageFlags&								additionalBufferUsageFlags	= VkBufferUsageFlags(0u),
																							 const MemoryRequirement&								additionalMemoryRequirement	= MemoryRequirement::Any,
																							 const VkDeviceAddress&									opaqueCaptureAddress		= 0u,
																							 const deUint32											shaderBindingTableOffset	= 0u,
																							 const deUint32											shaderRecordSize			= 0u);
	void														setCreateFlags				(const VkPipelineCreateFlags&							pipelineCreateFlags);
	void														setMaxRecursionDepth		(const deUint32&										maxRecursionDepth);
	void														setMaxPayloadSize			(const deUint32&										maxPayloadSize);
	void														setMaxAttributeSize			(const deUint32&										maxAttributeSize);
	void														setMaxCallableSize			(const deUint32&										maxCallableSize);
	void														setDeferredOperation		(const bool												deferredOperation);

protected:
	Move<VkPipeline>											createPipelineKHR			(const DeviceInterface&									vk,
																							 const VkDevice											device,
																							 const VkPipelineLayout									pipelineLayout,
																							 const std::vector<de::SharedPtr<Move<VkPipeline>>>&	pipelineLibraries);

	std::vector<de::SharedPtr<Move<VkShaderModule> > >			m_shadersModules;
	std::vector<de::SharedPtr<de::MovePtr<RayTracingPipeline>>>	m_pipelineLibraries;
	std::vector<VkPipelineShaderStageCreateInfo>				m_shaderCreateInfos;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR>			m_shadersGroupCreateInfos;
	VkPipelineCreateFlags										m_pipelineCreateFlags;
	deUint32													m_maxRecursionDepth;
	deUint32													m_maxPayloadSize;
	deUint32													m_maxAttributeSize;
	deUint32													m_maxCallableSize;
	bool														m_deferredOperation;
};

class RayTracingProperties
{
protected:
									RayTracingProperties						() {};

public:
									RayTracingProperties						(const InstanceInterface&	vki,
																				 const VkPhysicalDevice		physicalDevice) { DE_UNREF(vki); DE_UNREF(physicalDevice); };
	virtual							~RayTracingProperties						() {};

	virtual deUint32				getShaderGroupHandleSize					(void)	= DE_NULL;
	virtual deUint32				getMaxRecursionDepth						(void)	= DE_NULL;
	virtual deUint32				getMaxShaderGroupStride						(void)	= DE_NULL;
	virtual deUint32				getShaderGroupBaseAlignment					(void)	= DE_NULL;
	virtual deUint64				getMaxGeometryCount							(void)	= DE_NULL;
	virtual deUint64				getMaxInstanceCount							(void)	= DE_NULL;
	virtual deUint64				getMaxPrimitiveCount						(void)	= DE_NULL;
	virtual deUint32				getMaxDescriptorSetAccelerationStructures	(void)	= DE_NULL;
};

de::MovePtr<RayTracingProperties> makeRayTracingProperties (const InstanceInterface&	vki,
															const VkPhysicalDevice		physicalDevice);

void cmdTraceRays	(const DeviceInterface&				vk,
					 VkCommandBuffer					commandBuffer,
					 const VkStridedBufferRegionKHR*	raygenShaderBindingTableRegion,
					 const VkStridedBufferRegionKHR*	missShaderBindingTableRegion,
					 const VkStridedBufferRegionKHR*	hitShaderBindingTableRegion,
					 const VkStridedBufferRegionKHR*	callableShaderBindingTableRegion,
					 deUint32							width,
					 deUint32							height,
					 deUint32							depth);

void cmdTraceRaysIndirect	(const DeviceInterface&				vk,
							 VkCommandBuffer					commandBuffer,
							 const VkStridedBufferRegionKHR*	raygenShaderBindingTableRegion,
							 const VkStridedBufferRegionKHR*	missShaderBindingTableRegion,
							 const VkStridedBufferRegionKHR*	hitShaderBindingTableRegion,
							 const VkStridedBufferRegionKHR*	callableShaderBindingTableRegion,
							 VkBuffer							buffer,
							 VkDeviceSize						offset);
} // vk

#endif // _VKRAYTRACINGUTIL_HPP
