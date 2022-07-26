/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Ray Tracing Misc tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingMiscTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deRandom.hpp"
#include <algorithm>
#include <memory>
#include <sstream>

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace std;

enum class BaseType
{
	F32,
	F64,
	I8,
	I16,
	I32,
	I64,
	U8,
	U16,
	U32,
	U64,

	UNKNOWN
};

enum class GeometryType
{
	FIRST = 0,

	AABB		= FIRST,
	TRIANGLES,

	COUNT,

	AABB_AND_TRIANGLES, //< Only compatible with ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES AS layout.
};

enum class MatrixMajorOrder
{
	COLUMN_MAJOR,
	ROW_MAJOR,

	UNKNOWN
};

enum class ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,

	FIRST_CALLABLE_GROUP,
};

enum class TestType
{
	AABBS_AND_TRIS_IN_ONE_TL,
	AS_STRESS_TEST,
	CALLABLE_SHADER_STRESS_DYNAMIC_TEST,
	CALLABLE_SHADER_STRESS_TEST,
	CULL_MASK,
	MAX_RAY_HIT_ATTRIBUTE_SIZE,
	MAX_RT_INVOCATIONS_SUPPORTED,
	CULL_MASK_EXTRA_BITS,
	NO_DUPLICATE_ANY_HIT,
	REPORT_INTERSECTION_RESULT,
	RAY_PAYLOAD_IN,
	RECURSIVE_TRACES_0,
	RECURSIVE_TRACES_1,
	RECURSIVE_TRACES_2,
	RECURSIVE_TRACES_3,
	RECURSIVE_TRACES_4,
	RECURSIVE_TRACES_5,
	RECURSIVE_TRACES_6,
	RECURSIVE_TRACES_7,
	RECURSIVE_TRACES_8,
	RECURSIVE_TRACES_9,
	RECURSIVE_TRACES_10,
	RECURSIVE_TRACES_11,
	RECURSIVE_TRACES_12,
	RECURSIVE_TRACES_13,
	RECURSIVE_TRACES_14,
	RECURSIVE_TRACES_15,
	RECURSIVE_TRACES_16,
	RECURSIVE_TRACES_17,
	RECURSIVE_TRACES_18,
	RECURSIVE_TRACES_19,
	RECURSIVE_TRACES_20,
	RECURSIVE_TRACES_21,
	RECURSIVE_TRACES_22,
	RECURSIVE_TRACES_23,
	RECURSIVE_TRACES_24,
	RECURSIVE_TRACES_25,
	RECURSIVE_TRACES_26,
	RECURSIVE_TRACES_27,
	RECURSIVE_TRACES_28,
	RECURSIVE_TRACES_29,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5,
	SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5,
	SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6,
	SHADER_RECORD_BLOCK_SCALAR_1,
	SHADER_RECORD_BLOCK_SCALAR_2,
	SHADER_RECORD_BLOCK_SCALAR_3,
	SHADER_RECORD_BLOCK_SCALAR_4,
	SHADER_RECORD_BLOCK_SCALAR_5,
	SHADER_RECORD_BLOCK_SCALAR_6,
	SHADER_RECORD_BLOCK_STD430_1,
	SHADER_RECORD_BLOCK_STD430_2,
	SHADER_RECORD_BLOCK_STD430_3,
	SHADER_RECORD_BLOCK_STD430_4,
	SHADER_RECORD_BLOCK_STD430_5,
	SHADER_RECORD_BLOCK_STD430_6,
	IGNORE_ANY_HIT_STATICALLY,
	IGNORE_ANY_HIT_DYNAMICALLY,
	TERMINATE_ANY_HIT_STATICALLY,
	TERMINATE_ANY_HIT_DYNAMICALLY,
	TERMINATE_INTERSECTION_STATICALLY,
	TERMINATE_INTERSECTION_DYNAMICALLY,

	COUNT
};

enum class VariableType
{
	FIRST,

	FLOAT = FIRST,
	VEC2,
	VEC3,
	VEC4,

	MAT2,
	MAT2X2,
	MAT2X3,
	MAT2X4,
	MAT3,
	MAT3X2,
	MAT3X3,
	MAT3X4,
	MAT4,
	MAT4X2,
	MAT4X3,
	MAT4X4,

	INT,
	IVEC2,
	IVEC3,
	IVEC4,

	INT8,
	I8VEC2,
	I8VEC3,
	I8VEC4,

	INT16,
	I16VEC2,
	I16VEC3,
	I16VEC4,

	INT64,
	I64VEC2,
	I64VEC3,
	I64VEC4,

	UINT,
	UVEC2,
	UVEC3,
	UVEC4,

	UINT16,
	U16VEC2,
	U16VEC3,
	U16VEC4,

	UINT64,
	U64VEC2,
	U64VEC3,
	U64VEC4,

	UINT8,
	U8VEC2,
	U8VEC3,
	U8VEC4,

	DOUBLE,
	DVEC2,
	DVEC3,
	DVEC4,

	DMAT2,
	DMAT2X2,
	DMAT2X3,
	DMAT2X4,
	DMAT3,
	DMAT3X2,
	DMAT3X3,
	DMAT3X4,
	DMAT4,
	DMAT4X2,
	DMAT4X3,
	DMAT4X4,

	UNKNOWN,
	COUNT = UNKNOWN,
};

enum class AccelerationStructureLayout
{
	FIRST = 0,

	ONE_TL_ONE_BL_ONE_GEOMETRY    = FIRST,
	ONE_TL_ONE_BL_MANY_GEOMETRIES,
	ONE_TL_MANY_BLS_ONE_GEOMETRY,
	ONE_TL_MANY_BLS_MANY_GEOMETRIES,

	COUNT,

	ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES
};

static const VkFlags	ALL_RAY_TRACING_STAGES	= VK_SHADER_STAGE_RAYGEN_BIT_KHR
												| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
												| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
												| VK_SHADER_STAGE_MISS_BIT_KHR
												| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
												| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

struct CaseDef
{
	TestType					type;
	GeometryType				geometryType;
	AccelerationStructureLayout	asLayout;

	CaseDef()
		:	type			(TestType::COUNT),
			geometryType	(GeometryType::COUNT),
			asLayout		(AccelerationStructureLayout::COUNT)
	{
		/* Stub */
	}

	CaseDef(const TestType& inType)
		:	type			(inType),
			geometryType	(GeometryType::COUNT),
			asLayout		(AccelerationStructureLayout::COUNT)
	{
		/* Stub */
	}

	CaseDef(const TestType&						inType,
			const GeometryType&					inGeometryType,
			const AccelerationStructureLayout&	inAsLayout)
		:	type			(inType),
			geometryType	(inGeometryType),
			asLayout		(inAsLayout)
	{
		/* Stub */
	}
};

/* Helper global functions */
static const char* getSuffixForASLayout(const AccelerationStructureLayout& layout)
{
	const char* result = "?!";

	switch (layout)
	{
		case AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY:		result = "1TL1BL1G"; break;
		case AccelerationStructureLayout::ONE_TL_ONE_BL_MANY_GEOMETRIES:	result = "1TL1BLnG"; break;
		case AccelerationStructureLayout::ONE_TL_MANY_BLS_ONE_GEOMETRY:		result = "1TLnBL1G"; break;
		case AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES:	result = "1TLnBLnG"; break;

		default:
		{
			deAssertFail("This should never happen", __FILE__, __LINE__);
		}
	}

	return result;
}

static const char* getSuffixForGeometryType(const GeometryType& type)
{
	const char* result = "?!";

	switch (type)
	{
		case GeometryType::AABB:		result = "AABB"; break;
		case GeometryType::TRIANGLES:	result = "tri";  break;

		default:
		{
			deAssertFail("This should never happen", __FILE__, __LINE__);
		}
	}

	return result;
}

/* Instances and primitives in acceleration structures can have additional information assigned.
 *
 * By overriding functions of interest in this class, tests can further customize ASes generated by AS providers.
 */
class ASPropertyProvider
{
public:
	virtual ~ASPropertyProvider()
	{
		/* Stub */
	}

	virtual deUint8 getCullMask(const deUint32& nBL, const deUint32& nInstance) const
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		return 0xFF;
	}

	virtual deUint32 getInstanceCustomIndex(const deUint32& nBL, const deUint32& nInstance) const
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);
		return 0;
	}
};

class IGridASFeedback
{
public:
	virtual ~IGridASFeedback()
	{
		/* Stub */
	}

	virtual void onCullMaskAssignedToCell			(const tcu::UVec3& cellLocation, const deUint8&		cullMaskAssigned)		= 0;
	virtual void onInstanceCustomIndexAssignedToCell(const tcu::UVec3& cellLocation, const deUint32&	customIndexAssigned)	= 0;
};


/* Acceleration structure data providers.
 *
 * These are expected to be reused across different test cases.
 **/
class ASProviderBase
{
public:
	virtual ~ASProviderBase()
	{
		/* Stub */
	}

	virtual std::unique_ptr<TopLevelAccelerationStructure>	createTLAS(		Context&							context,
																			const AccelerationStructureLayout&	asLayout,
																			VkCommandBuffer						cmdBuffer,
																			const VkGeometryFlagsKHR&			bottomLevelGeometryFlags,
																			const ASPropertyProvider*			optAsPropertyProviderPtr	= nullptr,
																			IGridASFeedback*					optASFeedbackPtr			= nullptr)	const = 0;
	virtual deUint32										getNPrimitives()																			const = 0;

};

/* A 3D grid built of primitives. Size and distribution of the geometry can be configured both at creation time and at a later time. */
class GridASProvider : public ASProviderBase
{
public:
	GridASProvider(	const tcu::Vec3&			gridStartXYZ,
					const tcu::Vec3&			gridCellSizeXYZ,
					const tcu::UVec3&			gridSizeXYZ,
					const tcu::Vec3&			gridInterCellDeltaXYZ,
					const GeometryType&			geometryType)
		:m_geometryType			(geometryType),
		 m_gridCellSizeXYZ		(gridCellSizeXYZ),
		 m_gridInterCellDeltaXYZ(gridInterCellDeltaXYZ),
		 m_gridSizeXYZ			(gridSizeXYZ),
		 m_gridStartXYZ			(gridStartXYZ)
	{
		fillVertexVec();
	}

	std::unique_ptr<TopLevelAccelerationStructure> createTLAS(	Context&							context,
																const AccelerationStructureLayout&	asLayout,
																VkCommandBuffer						cmdBuffer,
																const VkGeometryFlagsKHR&			bottomLevelGeometryFlags,
																const ASPropertyProvider*			optASPropertyProviderPtr,
																IGridASFeedback*					optASFeedbackPtr) const final
	{
		Allocator&										allocator				= context.getDefaultAllocator		();
		const DeviceInterface&							deviceInterface			= context.getDeviceInterface		();
		const VkDevice									deviceVk				= context.getDevice					();
		const auto										nCells					= m_gridSizeXYZ.x() * m_gridSizeXYZ.y() * m_gridSizeXYZ.z();
		std::unique_ptr<TopLevelAccelerationStructure>	resultPtr;
		de::MovePtr<TopLevelAccelerationStructure>		tlPtr					= makeTopLevelAccelerationStructure ();

		DE_ASSERT(((asLayout == AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES) && (m_geometryType == GeometryType::AABB_AND_TRIANGLES)) ||
				  ((asLayout != AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES) && (m_geometryType != GeometryType::AABB_AND_TRIANGLES)) );

		switch (asLayout)
		{
			case AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY:
			{
				DE_ASSERT( (m_geometryType == GeometryType::AABB) || (m_geometryType == GeometryType::TRIANGLES) );

				const auto&	vertexVec				= (m_geometryType == GeometryType::AABB)	? m_aabbVertexVec
																								: m_triVertexVec;
				const auto	cullMask				= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getCullMask(0, 0)
																								: static_cast<deUint8>(0xFF);
				const auto	instanceCustomIndex		= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getInstanceCustomIndex(0, 0)
																								: 0;

				tlPtr->setInstanceCount(1);

				{
					de::MovePtr<BottomLevelAccelerationStructure> blPtr = makeBottomLevelAccelerationStructure();

					blPtr->setGeometryCount	(1u);
					blPtr->addGeometry		(vertexVec,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(	de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
										identityMatrix3x4,
										instanceCustomIndex,
										cullMask);
				}

				if (optASFeedbackPtr != nullptr)
				{
					for (auto	nCell = 0u;
								nCell < nCells;
								nCell++)
					{
						const auto cellX = (((nCell)												% m_gridSizeXYZ.x() ));
						const auto cellY = (((nCell / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
						const auto cellZ = (((nCell / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

						optASFeedbackPtr->onCullMaskAssignedToCell				(	tcu::UVec3(cellX, cellY, cellZ),
																					cullMask);
						optASFeedbackPtr->onInstanceCustomIndexAssignedToCell	(	tcu::UVec3(cellX, cellY, cellZ),
																					instanceCustomIndex);
					}
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_ONE_BL_MANY_GEOMETRIES:
			{
				DE_ASSERT( (m_geometryType == GeometryType::AABB) || (m_geometryType == GeometryType::TRIANGLES) );

				const auto&	vertexVec				= (m_geometryType == GeometryType::AABB)	? m_aabbVertexVec
																								: m_triVertexVec;
				const auto	nVerticesPerPrimitive	= (m_geometryType == GeometryType::AABB)	? 2u
																								: 12u /* tris */  * 3 /* verts */;
				const auto	cullMask				= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getCullMask(0, 0)
																								: static_cast<deUint8>(0xFF);
				const auto	instanceCustomIndex		= (optASPropertyProviderPtr	!= nullptr)		? optASPropertyProviderPtr->getInstanceCustomIndex(0, 0)
																								: 0;

				DE_ASSERT( (vertexVec.size() % nVerticesPerPrimitive) == 0);

				tlPtr->setInstanceCount(1);

				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr		= makeBottomLevelAccelerationStructure();
					const auto										nGeometries = vertexVec.size() / nVerticesPerPrimitive;

					blPtr->setGeometryCount	(nGeometries);

					for (deUint32 nGeometry = 0; nGeometry < nGeometries; ++nGeometry)
					{
						std::vector<tcu::Vec3> currentGeometry(nVerticesPerPrimitive);

						for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
						{
							currentGeometry.at(nVertex) = vertexVec.at(nGeometry * nVerticesPerPrimitive + nVertex);
						}

						blPtr->addGeometry	(currentGeometry,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);
					}

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(	de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
										identityMatrix3x4,
										instanceCustomIndex,
										cullMask);
				}

				if (optASFeedbackPtr != nullptr)
				{
					for (auto	nCell = 0u;
								nCell < nCells;
								nCell++)
					{
						const auto cellX = (((nCell)												% m_gridSizeXYZ.x() ));
						const auto cellY = (((nCell / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
						const auto cellZ = (((nCell / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

						optASFeedbackPtr->onCullMaskAssignedToCell				(	tcu::UVec3(cellX, cellY, cellZ),
																					cullMask);
						optASFeedbackPtr->onInstanceCustomIndexAssignedToCell	(	tcu::UVec3(cellX, cellY, cellZ),
																					instanceCustomIndex);
					}
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_MANY_BLS_ONE_GEOMETRY:
			{
				DE_ASSERT( (m_geometryType == GeometryType::AABB) || (m_geometryType == GeometryType::TRIANGLES) );

				const auto&	vertexVec				= (m_geometryType == GeometryType::AABB)	? m_aabbVertexVec
																								: m_triVertexVec;
				const auto	nVerticesPerPrimitive	= (m_geometryType == GeometryType::AABB)	? 2u
																								: 12u /* tris */  * 3 /* verts */;
				const auto	nInstances				= vertexVec.size() / nVerticesPerPrimitive;

				DE_ASSERT( (vertexVec.size() % nVerticesPerPrimitive) == 0);

				tlPtr->setInstanceCount(nInstances);

				for (deUint32 nInstance = 0; nInstance < nInstances; nInstance++)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr						= makeBottomLevelAccelerationStructure();
					const auto										cullMask					= (optASPropertyProviderPtr != nullptr)	? optASPropertyProviderPtr->getCullMask(0, nInstance)
																																		: static_cast<deUint8>(0xFF);
					std::vector<tcu::Vec3>							currentInstanceVertexVec;
					const auto										instanceCustomIndex			= (optASPropertyProviderPtr != nullptr)	? optASPropertyProviderPtr->getInstanceCustomIndex(0, nInstance)
																																		: 0;

					for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
					{
						currentInstanceVertexVec.push_back(vertexVec.at(nInstance * nVerticesPerPrimitive + nVertex) );
					}

					blPtr->setGeometryCount	(1u);
					blPtr->addGeometry		(currentInstanceVertexVec,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(	de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
										identityMatrix3x4,
										instanceCustomIndex,
										cullMask);


					if (optASFeedbackPtr != nullptr)
					{
						const auto cellX = (((nInstance)												% m_gridSizeXYZ.x() ));
						const auto cellY = (((nInstance / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
						const auto cellZ = (((nInstance / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

						optASFeedbackPtr->onCullMaskAssignedToCell(				tcu::UVec3(cellX, cellY, cellZ),
																				cullMask);
						optASFeedbackPtr->onInstanceCustomIndexAssignedToCell(	tcu::UVec3(cellX, cellY, cellZ),
																				instanceCustomIndex);
					}
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES:
			{
				DE_ASSERT( (m_geometryType == GeometryType::AABB) || (m_geometryType == GeometryType::TRIANGLES) );

				const auto&	vertexVec				= (m_geometryType == GeometryType::AABB)	? m_aabbVertexVec
																								: m_triVertexVec;
				const auto	nVerticesPerPrimitive	= (m_geometryType == GeometryType::AABB)	? 2u
																								: 12u /* tris */  * 3 /* verts */;
				const auto	nPrimitivesDefined		= static_cast<deUint32>(vertexVec.size() / nVerticesPerPrimitive);
				const auto	nPrimitivesPerBLAS		= 4;
				const auto	nBottomLevelASes		= nPrimitivesDefined / nPrimitivesPerBLAS;

				DE_ASSERT( (vertexVec.size()   % nVerticesPerPrimitive)	== 0);
				DE_ASSERT( (nPrimitivesDefined % nPrimitivesPerBLAS)	== 0);

				tlPtr->setInstanceCount(nBottomLevelASes);

				for (deUint32 nBottomLevelAS = 0; nBottomLevelAS < nBottomLevelASes; nBottomLevelAS++)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr				= makeBottomLevelAccelerationStructure();
					const auto										cullMask			= (optASPropertyProviderPtr != nullptr)	? optASPropertyProviderPtr->getCullMask(nBottomLevelAS, 0)
																																: static_cast<deUint8>(0xFF);
					const auto										instanceCustomIndex	= (optASPropertyProviderPtr != nullptr)	? optASPropertyProviderPtr->getInstanceCustomIndex(nBottomLevelAS, 0)
																																: 0;

					blPtr->setGeometryCount(nPrimitivesPerBLAS);

					for (deUint32 nGeometry = 0; nGeometry < nPrimitivesPerBLAS; nGeometry++)
					{
						std::vector<tcu::Vec3> currentVertexVec;

						for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
						{
							currentVertexVec.push_back(vertexVec.at((nBottomLevelAS * nPrimitivesPerBLAS + nGeometry) * nVerticesPerPrimitive + nVertex) );
						}

						blPtr->addGeometry(	currentVertexVec,
											(m_geometryType == GeometryType::TRIANGLES),
											bottomLevelGeometryFlags);
					}

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);
					tlPtr->addInstance(		de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
											identityMatrix3x4,
											instanceCustomIndex,
											cullMask);

					if (optASFeedbackPtr != nullptr)
					{
						for (deUint32 cellIndex = nPrimitivesPerBLAS * nBottomLevelAS; cellIndex < nPrimitivesPerBLAS * (nBottomLevelAS + 1); cellIndex++)
						{
							const auto cellX = (((cellIndex)												% m_gridSizeXYZ.x() ));
							const auto cellY = (((cellIndex / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
							const auto cellZ = (((cellIndex / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

							optASFeedbackPtr->onCullMaskAssignedToCell				(	tcu::UVec3(cellX, cellY, cellZ),
																						cullMask);
							optASFeedbackPtr->onInstanceCustomIndexAssignedToCell	(	tcu::UVec3(cellX, cellY, cellZ),
																						instanceCustomIndex);
						}
					}
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES:
			{
				DE_ASSERT(m_geometryType == GeometryType::AABB_AND_TRIANGLES);

				const auto	nCellsDefined		= m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
				const auto	nPrimitivesPerBLAS	= 1;
				const auto	nBottomLevelASes	= nCellsDefined / nPrimitivesPerBLAS;

				DE_ASSERT( (nCellsDefined % nPrimitivesPerBLAS) == 0);

				tlPtr->setInstanceCount(nBottomLevelASes);

				for (deUint32 nBottomLevelAS = 0; nBottomLevelAS < nBottomLevelASes; nBottomLevelAS++)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr					= makeBottomLevelAccelerationStructure();
					const auto										cullMask				= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getCullMask(nBottomLevelAS, 0)
																																		: static_cast<deUint8>(0xFF);
					const auto										instanceCustomIndex		= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getInstanceCustomIndex(nBottomLevelAS, 0)
																																		: 0;
					const bool										usesAABB				= (nBottomLevelAS % 2) == 0;
					const auto&										vertexVec				= (usesAABB)								? m_aabbVertexVec
																																		: m_triVertexVec;
					const auto										nVerticesPerPrimitive	= (usesAABB)								? 2u
																																		: 12u /* tris */  * 3 /* verts */;

					// For this case, AABBs use the first shader group and triangles use the second shader group in the table.
					const auto										instanceSBTOffset		= (usesAABB ? 0u : 1u);

					blPtr->setGeometryCount(nPrimitivesPerBLAS);

					for (deUint32 nGeometry = 0; nGeometry < nPrimitivesPerBLAS; nGeometry++)
					{
						DE_ASSERT( (vertexVec.size() % nVerticesPerPrimitive) == 0);

						std::vector<tcu::Vec3> currentVertexVec;

						for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
						{
							currentVertexVec.push_back(vertexVec.at((nBottomLevelAS * nPrimitivesPerBLAS + nGeometry) * nVerticesPerPrimitive + nVertex) );
						}

						blPtr->addGeometry(	currentVertexVec,
											!usesAABB,
											bottomLevelGeometryFlags);
					}

					blPtr->createAndBuild	(	deviceInterface,
												deviceVk,
												cmdBuffer,
												allocator);

					tlPtr->addInstance(	de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
										identityMatrix3x4,
										instanceCustomIndex,
										cullMask,
										instanceSBTOffset);

					if (optASFeedbackPtr != nullptr)
					{
						for (deUint32 cellIndex = nPrimitivesPerBLAS * nBottomLevelAS; cellIndex < nPrimitivesPerBLAS * (nBottomLevelAS + 1); cellIndex++)
						{
							const auto cellX = (((cellIndex)												% m_gridSizeXYZ.x() ));
							const auto cellY = (((cellIndex / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
							const auto cellZ = (((cellIndex / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

							optASFeedbackPtr->onCullMaskAssignedToCell				(	tcu::UVec3(cellX, cellY, cellZ),
																						cullMask);
							optASFeedbackPtr->onInstanceCustomIndexAssignedToCell	(	tcu::UVec3(cellX, cellY, cellZ),
																						instanceCustomIndex);
						}
					}
				}

				break;
			}

			default:
			{
				deAssertFail("This should never happen", __FILE__, __LINE__);
			}
		}

		tlPtr->createAndBuild(	deviceInterface,
								deviceVk,
								cmdBuffer,
								allocator);

		resultPtr = decltype(resultPtr)(tlPtr.release() );
		return resultPtr;
	}

	deUint32 getNPrimitives() const final
	{
		return m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
	}

	void setProperties(	const tcu::Vec3&		gridStartXYZ,
						const tcu::Vec3&		gridCellSizeXYZ,
						const tcu::UVec3&		gridSizeXYZ,
						const tcu::Vec3&		gridInterCellDeltaXYZ,
						const GeometryType&		geometryType)
	{
		m_gridStartXYZ			= gridStartXYZ;
		m_gridCellSizeXYZ		= gridCellSizeXYZ;
		m_gridSizeXYZ			= gridSizeXYZ;
		m_gridInterCellDeltaXYZ = gridInterCellDeltaXYZ;
		m_geometryType			= geometryType;

		fillVertexVec();
	}

private:
	void fillVertexVec()
	{
		const auto nCellsNeeded = m_gridSizeXYZ.x() * m_gridSizeXYZ.y() * m_gridSizeXYZ.z();

		m_aabbVertexVec.clear();
		m_triVertexVec.clear ();

		for (auto	nCell = 0u;
					nCell < nCellsNeeded;
					nCell++)
		{
			const auto cellX = (((nCell)												% m_gridSizeXYZ.x() ));
			const auto cellY = (((nCell / m_gridSizeXYZ.x() )							% m_gridSizeXYZ.y() ));
			const auto cellZ = (((nCell / m_gridSizeXYZ.x() )	/ m_gridSizeXYZ.y() )	% m_gridSizeXYZ.z() );

			const auto cellX1Y1Z1 = tcu::Vec3(	m_gridStartXYZ.x() + static_cast<float>(cellX) * m_gridInterCellDeltaXYZ.x(),
												m_gridStartXYZ.y() + static_cast<float>(cellY) * m_gridInterCellDeltaXYZ.y(),
												m_gridStartXYZ.z() + static_cast<float>(cellZ) * m_gridInterCellDeltaXYZ.z() );
			const auto cellX2Y2Z2 = tcu::Vec3(	m_gridStartXYZ.x() + static_cast<float>(cellX) * m_gridInterCellDeltaXYZ.x() + m_gridCellSizeXYZ.x(),
												m_gridStartXYZ.y() + static_cast<float>(cellY) * m_gridInterCellDeltaXYZ.y() + m_gridCellSizeXYZ.y(),
												m_gridStartXYZ.z() + static_cast<float>(cellZ) * m_gridInterCellDeltaXYZ.z() + m_gridCellSizeXYZ.z() );

			if (m_geometryType == GeometryType::AABB				||
				m_geometryType == GeometryType::AABB_AND_TRIANGLES)
			{
				/* Cell = AABB of the cell */
				m_aabbVertexVec.push_back(cellX1Y1Z1);
				m_aabbVertexVec.push_back(cellX2Y2Z2);
			}

			if (m_geometryType == GeometryType::AABB_AND_TRIANGLES ||
				m_geometryType == GeometryType::TRIANGLES)
			{
				/* Cell == Six triangles forming a cube
				 *
				 * Lower-case characters: vertices with Z == Z2
				 * Upper-case characters: vertices with Z == Z1


						g				h


				    C              D



						e				f

					A              B


				 */
				const auto A = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX1Y1Z1.y(),
											cellX1Y1Z1.z() );
				const auto B = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX1Y1Z1.y(),
											cellX1Y1Z1.z() );
				const auto C = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX2Y2Z2.y(),
											cellX1Y1Z1.z() );
				const auto D = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX2Y2Z2.y(),
											cellX1Y1Z1.z() );
				const auto E = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX1Y1Z1.y(),
											cellX2Y2Z2.z() );
				const auto F = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX1Y1Z1.y(),
											cellX2Y2Z2.z() );
				const auto G = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX2Y2Z2.y(),
											cellX2Y2Z2.z() );
				const auto H = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX2Y2Z2.y(),
											cellX2Y2Z2.z() );

				// Z = Z1 face
				m_triVertexVec.push_back(A);
				m_triVertexVec.push_back(C);
				m_triVertexVec.push_back(D);

				m_triVertexVec.push_back(D);
				m_triVertexVec.push_back(B);
				m_triVertexVec.push_back(A);

				// Z = Z2 face
				m_triVertexVec.push_back(E);
				m_triVertexVec.push_back(H);
				m_triVertexVec.push_back(G);

				m_triVertexVec.push_back(H);
				m_triVertexVec.push_back(E);
				m_triVertexVec.push_back(F);

				// X = X0 face
				m_triVertexVec.push_back(A);
				m_triVertexVec.push_back(G);
				m_triVertexVec.push_back(C);

				m_triVertexVec.push_back(G);
				m_triVertexVec.push_back(A);
				m_triVertexVec.push_back(E);

				// X = X1 face
				m_triVertexVec.push_back(B);
				m_triVertexVec.push_back(D);
				m_triVertexVec.push_back(H);

				m_triVertexVec.push_back(H);
				m_triVertexVec.push_back(F);
				m_triVertexVec.push_back(B);

				// Y = Y0 face
				m_triVertexVec.push_back(C);
				m_triVertexVec.push_back(H);
				m_triVertexVec.push_back(D);

				m_triVertexVec.push_back(H);
				m_triVertexVec.push_back(C);
				m_triVertexVec.push_back(G);

				// Y = y1 face
				m_triVertexVec.push_back(A);
				m_triVertexVec.push_back(B);
				m_triVertexVec.push_back(E);

				m_triVertexVec.push_back(B);
				m_triVertexVec.push_back(F);
				m_triVertexVec.push_back(E);
			}
		}
	}

	std::vector<tcu::Vec3> m_aabbVertexVec;
	std::vector<tcu::Vec3> m_triVertexVec;

	GeometryType	m_geometryType;
	tcu::Vec3		m_gridCellSizeXYZ;
	tcu::Vec3		m_gridInterCellDeltaXYZ;
	tcu::UVec3		m_gridSizeXYZ;
	tcu::Vec3		m_gridStartXYZ;
};

/* Provides an AS holding a single {(0, 0, 0), (-1, 1, 0), {1, 1, 0} tri. */
class TriASProvider : public ASProviderBase
{
public:
	TriASProvider()
	{
		/* Stub*/
	}

	std::unique_ptr<TopLevelAccelerationStructure> createTLAS(	Context&							context,
																const AccelerationStructureLayout&	/* asLayout */,
																VkCommandBuffer						cmdBuffer,
																const VkGeometryFlagsKHR&			bottomLevelGeometryFlags,
																const ASPropertyProvider*			optASPropertyProviderPtr,
																IGridASFeedback*					/* optASFeedbackPtr */) const final
	{
		Allocator&										allocator		= context.getDefaultAllocator		();
		const DeviceInterface&							deviceInterface	= context.getDeviceInterface		();
		const VkDevice									deviceVk		= context.getDevice					();
		std::unique_ptr<TopLevelAccelerationStructure>	resultPtr;
		de::MovePtr<TopLevelAccelerationStructure>		tlPtr			= makeTopLevelAccelerationStructure ();

		{

			const auto	cullMask				= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getCullMask(0, 0)
																							: static_cast<deUint8>(0xFF);
			const auto	instanceCustomIndex		= (optASPropertyProviderPtr != nullptr)		? optASPropertyProviderPtr->getInstanceCustomIndex(0, 0)
																							: 0;

			tlPtr->setInstanceCount(1);

			{
				de::MovePtr<BottomLevelAccelerationStructure>	blPtr		= makeBottomLevelAccelerationStructure();
				const std::vector<tcu::Vec3>					vertexVec	= {tcu::Vec3(0, 0, 0), tcu::Vec3(-1, 1, 0), tcu::Vec3(1, 1, 0) };

				blPtr->setGeometryCount	(1u);
				blPtr->addGeometry		(vertexVec,
										 true, /* triangles */
										 bottomLevelGeometryFlags);

				blPtr->createAndBuild(	deviceInterface,
										deviceVk,
										cmdBuffer,
										allocator);

				tlPtr->addInstance(	de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ),
									identityMatrix3x4,
									instanceCustomIndex,
									cullMask);
			}
		}

		tlPtr->createAndBuild(	deviceInterface,
								deviceVk,
								cmdBuffer,
								allocator);

		resultPtr = decltype(resultPtr)(tlPtr.release() );
		return resultPtr;
	}

	deUint32 getNPrimitives() const final
	{
		return 1;
	}
};

/* Test logic providers ==> */
class TestBase
{
public:
	virtual ~TestBase()
	{
		/* Stub */
	}

	virtual tcu::UVec3									getDispatchSize				()												const	= 0;
	virtual deUint32									getResultBufferSize			()												const	= 0;
	virtual std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind			()												const	= 0;
	virtual void										resetTLAS					()														= 0;
	virtual void										initAS						(	vkt::Context&			context,
																						RayTracingProperties*	rtPropertiesPtr,
																						VkCommandBuffer			commandBuffer)				= 0;
	virtual void										initPrograms				(	SourceCollections&		programCollection)	const	= 0;
	virtual bool										verifyResultBuffer			(	const void*				inBufferPtr)		const	= 0;

	virtual std::vector<std::string> getAHitShaderCollectionShaderNames() const
	{
		return {"ahit"};
	}

	virtual deUint32 getASBindingArraySize() const
	{
		return 1u;
	}

	virtual std::vector<std::string> getCallableShaderCollectionNames() const
	{
		return std::vector<std::string>{};
	}

	virtual std::vector<std::string> getCHitShaderCollectionShaderNames() const
	{
		return {"chit"};
	}

	virtual deUint32 getDynamicStackSize(deUint32 maxPipelineRayRecursionDepth) const
	{
		DE_ASSERT(false);

		DE_UNREF(maxPipelineRayRecursionDepth);

		return 0;
	}

	virtual std::vector<std::string> getIntersectionShaderCollectionShaderNames() const
	{
		return {"intersection"};
	}

	virtual deUint32 getMaxRecursionDepthUsed() const
	{
		return 1;
	}

	virtual std::vector<std::string> getMissShaderCollectionShaderNames() const
	{
		return {"miss"};
	}

	virtual deUint32 getNTraceRayInvocationsNeeded() const
	{
		return 1;
	}

	virtual Move<VkPipelineLayout> getPipelineLayout(const vk::DeviceInterface&	deviceInterface,
													 VkDevice					deviceVk,
													 VkDescriptorSetLayout		descriptorSetLayout)
	{
		return makePipelineLayout(	deviceInterface,
									deviceVk,
									descriptorSetLayout);
	}

	virtual std::vector<deUint8> getResultBufferStartData() const
	{
		return std::vector<deUint8>();
	}

	virtual const void* getShaderRecordData(const ShaderGroups& /* shaderGroup */) const
	{
		return nullptr;
	}

	virtual deUint32 getShaderRecordSize(const ShaderGroups& /* shaderGroup */) const
	{
		return 0;
	}

	virtual VkSpecializationInfo* getSpecializationInfoPtr(const VkShaderStageFlagBits& /* shaderStage */)
	{
		return nullptr;
	}

	virtual bool init(	vkt::Context&			/* context    */,
						RayTracingProperties*	/* rtPropsPtr */)
	{
		return true;
	}

	virtual void onBeforeCmdTraceRays(	const deUint32&		/* nDispatch      */,
										vkt::Context&		/* context        */,
										VkCommandBuffer		/* commandBuffer  */,
										VkPipelineLayout	/* pipelineLayout */)
	{
		/* Stub */
	}

	virtual void onShaderStackSizeDiscovered(	const VkDeviceSize& /* raygenShaderStackSize   */,
												const VkDeviceSize& /* ahitShaderStackSize     */,
												const VkDeviceSize& /* chitShaderStackSize     */,
												const VkDeviceSize& /* missShaderStackSize     */,
												const VkDeviceSize& /* callableShaderStackSize */,
												const VkDeviceSize& /* isectShaderStackSize    */)
	{
		/* Stub */
	}

	virtual bool usesDynamicStackSize() const
	{
		return false;
	}
};

class AABBTriTLTest :	public TestBase,
						public ASPropertyProvider
{
public:
	AABBTriTLTest(	const GeometryType&					geometryType,
					const AccelerationStructureLayout&	asStructureLayout)
		:	m_asStructureLayout				(asStructureLayout),
			m_geometryType					(geometryType),
			m_gridSize                      (tcu::UVec3(720, 1, 1) ),
			m_lastCustomInstanceIndexUsed	(0)
	{
	}

	~AABBTriTLTest()
	{
		/* Stub */
	}

	virtual std::vector<std::string> getAHitShaderCollectionShaderNames() const
	{
		return {"ahit", "ahit"};
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	deUint32 getInstanceCustomIndex(const deUint32& nBL, const deUint32& nInstance) const final
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		return ++m_lastCustomInstanceIndexUsed;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(m_gridSize[0], m_gridSize[1], m_gridSize[2]);
	}

	deUint32 getResultBufferSize() const final
	{
		return static_cast<deUint32>((2 /* nHits, nMisses */ + m_gridSize[0] * m_gridSize[1] * m_gridSize[2] * 1 /* hit instance custom indices */) * sizeof(deUint32) );
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return {m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		/* Each AS holds a single unit AABB / cube built of tris.
		 *
		 * Geometry in the zeroth acceleration structure starts at the origin. Subsequent ASes
		 * hold geometry that is positioned so that geometry formed by the union of all ASes never
		 * intersects.
		 *
		 * Each raygen shader invocation uses a unique origin+target pair for the traced ray, and
		 * only one AS is expected to hold geometry that the ray can find intersection for.
		 * The AS index is stored in the result buffer, which is later verified by the CPU.
		 *
		 * Due to the fact AccelerationStructureEXT array indexing must be dynamically uniform and
		 * it is not guaranteed we can determine workgroup size on VK 1.1-conformant platforms,
		 * we can only trace rays against the same AS in a single ray trace dispatch.
		 */
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0, 0, 0), /* gridStartXYZ          */
								tcu::Vec3 (1, 1, 1), /* gridCellSizeXYZ       */
								m_gridSize,
								tcu::Vec3 (3, 0, 0), /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = asProviderPtr->createTLAS(	context,
												m_asStructureLayout,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												this,		/* optASPropertyProviderPtr */
												nullptr);	/* optASFeedbackPtr			*/
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const char* hitPropsDefinition =
			"struct HitProps\n"
			"{\n"
			"    uint instanceCustomIndex;\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 dummyAttribute;\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT      uint   dummy;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint nHit = atomicAdd(nHitsRegistered, 1);\n"
				"\n"
				"    hits[nHit].instanceCustomIndex = gl_InstanceCustomIndexEXT;\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(set = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"layout(location = 0) rayPayloadInEXT uint rayIndex;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nMissesRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT uint               dummy;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = gl_RayFlagsCullBackFacingTrianglesEXT;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(nInvocation * 3.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result			= false;

		typedef struct
		{
			deUint32 instanceCustomIndex;
		} HitProperties;

		std::map<deUint32, deUint32>	customInstanceIndexToHitCountMap;
		const auto						nHitsReported						= *resultU32Ptr;
		const auto						nMissesReported						= *(resultU32Ptr + 1);

		if (nHitsReported != m_gridSize[0] * m_gridSize[1] * m_gridSize[2])
		{
			goto end;
		}

		if (nMissesReported != 0)
		{
			goto end;
		}

		for (deUint32 nHit = 0; nHit < nHitsReported; ++nHit)
		{
			const HitProperties* hitPropsPtr = reinterpret_cast<const HitProperties*>(resultU32Ptr + 2 /* preamble ints */) + nHit;

			customInstanceIndexToHitCountMap[hitPropsPtr->instanceCustomIndex]++;

			if (customInstanceIndexToHitCountMap[hitPropsPtr->instanceCustomIndex] > 1)
			{
				goto end;
			}
		}

		for (deUint32 nInstance = 0; nInstance < nHitsReported; ++nInstance)
		{
			if (customInstanceIndexToHitCountMap.find(1 + nInstance) == customInstanceIndexToHitCountMap.end() )
			{
				goto end;
			}
		}

		result = true;
end:
		return result;
	}

private:
	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	const tcu::UVec3								m_gridSize;
	mutable deUint32								m_lastCustomInstanceIndexUsed;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};

class ASStressTest :	public TestBase,
						public ASPropertyProvider
{
public:
	ASStressTest(	const GeometryType&					geometryType,
					const AccelerationStructureLayout&	asStructureLayout)
		:	m_asStructureLayout				(asStructureLayout),
			m_geometryType					(geometryType),
			m_lastCustomInstanceIndexUsed	(0),
			m_nASesToUse					(0),
			m_nMaxASToUse					(16u)
	{
	}

	~ASStressTest()
	{
		/* Stub */
	}

	deUint32 getASBindingArraySize() const final
	{
		DE_ASSERT(m_nASesToUse != 0);

		return m_nASesToUse;
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	deUint32 getInstanceCustomIndex(const deUint32& nBL, const deUint32& nInstance) const final
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		return ++m_lastCustomInstanceIndexUsed;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(1, 1, 1);
	}

	deUint32 getNTraceRayInvocationsNeeded() const final
	{
		return m_nMaxASToUse;
	}

	deUint32 getResultBufferSize() const final
	{
		return static_cast<deUint32>((2 /* nHits, nMisses */ + 2 * m_nMaxASToUse /* hit instance custom indices + AS index */) * sizeof(deUint32) );
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		std::vector<TopLevelAccelerationStructure*> resultVec;

		DE_ASSERT(m_tlPtrVec.size() != 0);

		for (auto& currentTLPtr : m_tlPtrVec)
		{
			resultVec.push_back(currentTLPtr.get() );
		}

		return resultVec;
	}

	void resetTLAS() final
	{
		for (auto& currentTLPtr : m_tlPtrVec)
		{
			currentTLPtr.reset();
		}
	}

	bool init(	vkt::Context&			/* context    */,
				RayTracingProperties*	rtPropertiesPtr) final
	{
		/* NOTE: We clamp the number below to a sensible value, in case the implementation has no restrictions on the number of
		 *		 ASes accessible to shaders.
		 */
		m_nASesToUse = std::min(rtPropertiesPtr->getMaxDescriptorSetAccelerationStructures(),
								m_nMaxASToUse);

		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		/* Each AS holds a single unit AABB / cube built of tris.
		 *
		 * Geometry in the zeroth acceleration structure starts at the origin. Subsequent ASes
		 * hold geometry that is positioned so that geometry formed by the union of all ASes never
		 * intersects.
		 *
		 * Each raygen shader invocation uses a unique origin+target pair for the traced ray, and
		 * only one AS is expected to hold geometry that the ray can find intersection for.
		 * The AS index is stored in the result buffer, which is later verified by the CPU.
		 *
		 * Due to the fact AccelerationStructureEXT array indexing must be dynamically uniform and
		 * it is not guaranteed we can determine workgroup size on VK 1.1-conformant platforms,
		 * we can only trace rays against the same AS in a single ray trace dispatch.
		 */
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0, 0, 0), /* gridStartXYZ          */
								tcu::Vec3 (1, 1, 1), /* gridCellSizeXYZ       */
								tcu::UVec3(1, 1, 1), /* gridSizeXYZ           */
								tcu::Vec3 (0, 0, 0), /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		for (deUint32 nAS = 0; nAS < m_nASesToUse; ++nAS)
		{
			const auto origin = tcu::Vec3(3.0f * static_cast<float>(nAS), 0.0f, 0.0f);

			asProviderPtr->setProperties(
				origin,
				tcu::Vec3(1, 1, 1),		/* gridCellSizeXYZ       */
				tcu::UVec3(1, 1, 1),	/* gridSizeXYZ           */
				tcu::Vec3(0, 0, 0),		/* gridInterCellDeltaXYZ */
				m_geometryType
			);

			auto tlPtr = asProviderPtr->createTLAS(	context,
													m_asStructureLayout,
													commandBuffer,
													VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
													this,		/* optASPropertyProviderPtr */
													nullptr);	/* optASFeedbackPtr			*/

			m_tlPtrVec.push_back(std::move(tlPtr) );
		}
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const char* hitPropsDefinition =
			"struct HitProps\n"
			"{\n"
			"    uint instanceCustomIndex;\n"
			"    uint nAS;\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 dummyAttribute;\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT      uint   nAS;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint nHit = atomicAdd(nHitsRegistered, 1);\n"
				"\n"
				"    hits[nHit].instanceCustomIndex = gl_InstanceCustomIndexEXT;\n"
				"    hits[nHit].nAS                 = nAS;\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(set = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"layout(location = 0) rayPayloadInEXT uint rayIndex;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nMissesRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(push_constant) uniform pcUB\n"
				"{\n"
				"    uint nAS;\n"
				"} ub;\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT uint               payload;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructures[" + de::toString(m_nMaxASToUse) + "];\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = gl_RayFlagsCullBackFacingTrianglesEXT;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(ub.nAS * 3.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    payload = ub.nAS;\n"
				"\n"
				"    traceRayEXT(accelerationStructures[ub.nAS], rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	Move<VkPipelineLayout> getPipelineLayout(	const vk::DeviceInterface&	deviceInterface,
												VkDevice					deviceVk,
												VkDescriptorSetLayout		descriptorSetLayout) final
	{
		VkPushConstantRange pushConstantRange;

		pushConstantRange.offset		= 0;
		pushConstantRange.size			= sizeof(deUint32);
		pushConstantRange.stageFlags	= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		return makePipelineLayout(	deviceInterface,
									deviceVk,
									1, /* setLayoutCount */
									&descriptorSetLayout,
									1, /* pushRangeCount */
									&pushConstantRange);
	}

	void onBeforeCmdTraceRays(	const deUint32&		nDispatch,
								vkt::Context&		context,
								VkCommandBuffer		commandBuffer,
								VkPipelineLayout	pipelineLayout) final
	{
		/* No need for a sync point in-between trace ray commands - all writes are atomic */
		VkMemoryBarrier memBarrier;

		memBarrier.dstAccessMask	= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		memBarrier.pNext			= nullptr;
		memBarrier.srcAccessMask	= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		memBarrier.sType			= VK_STRUCTURE_TYPE_MEMORY_BARRIER;

		context.getDeviceInterface().cmdPipelineBarrier(commandBuffer,
														VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,	/* srcStageMask       */
														VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,	/* dstStageMask       */
														0,												/* dependencyFlags    */
														1,												/* memoryBarrierCount */
														&memBarrier,
														0,												/* bufferMemoryBarrierCount */
														nullptr,										/* pBufferMemoryBarriers    */
														0,												/* imageMemoryBarrierCount  */
														nullptr);										/* pImageMemoryBarriers     */

		context.getDeviceInterface().cmdPushConstants(	commandBuffer,
														pipelineLayout,
														VK_SHADER_STAGE_RAYGEN_BIT_KHR,
														0, /* offset */
														sizeof(deUint32),
														&nDispatch);
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result			= false;

		typedef struct
		{
			deUint32 instanceCustomIndex;
			deUint32 nAS;
		} HitProperties;

		const auto		nHitsReported	= *resultU32Ptr;
		const auto		nMissesReported	= *(resultU32Ptr + 1);

		if (nHitsReported != m_nMaxASToUse)
		{
			goto end;
		}

		if (nMissesReported != 0)
		{
			goto end;
		}

		for (deUint32 nHit = 0; nHit < nHitsReported; ++nHit)
		{
			const HitProperties* hitPropsPtr = reinterpret_cast<const HitProperties*>(resultU32Ptr + 2 /* preamble ints */) + nHit;

			if (hitPropsPtr->instanceCustomIndex != (nHit + 1) )
			{
				goto end;
			}

			if (hitPropsPtr->nAS != nHit)
			{
				goto end;
			}
		}

		result = true;
end:
		return result;
	}

private:
	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	mutable deUint32												m_lastCustomInstanceIndexUsed;
	deUint32														m_nASesToUse;
	std::vector<std::unique_ptr<TopLevelAccelerationStructure> >	m_tlPtrVec;

	const deUint32 m_nMaxASToUse;
};

class CallableShaderStressTest: public TestBase
{
public:
	CallableShaderStressTest(	const GeometryType&					geometryType,
								const AccelerationStructureLayout&	asStructureLayout,
								const bool&							useDynamicStackSize)
		:	m_asStructureLayout				(asStructureLayout),
			m_geometryType					(geometryType),
			m_gridSizeXYZ					(tcu::UVec3 (128, 1, 1) ),
			m_nMaxCallableLevels			( (useDynamicStackSize)		? 8
																		: 2 /* as per spec */),
			m_useDynamicStackSize			(useDynamicStackSize),
			m_ahitShaderStackSize			(0),
			m_callableShaderStackSize		(0),
			m_chitShaderStackSize			(0),
			m_isectShaderStackSize			(0),
			m_missShaderStackSize			(0),
			m_raygenShaderStackSize			(0)
	{
	}

	~CallableShaderStressTest()
	{
		/* Stub */
	}

	std::vector<std::string> getCallableShaderCollectionNames() const final
	{
		std::vector<std::string> resultVec(m_nMaxCallableLevels);

		for (deUint32 nLevel = 0; nLevel < m_nMaxCallableLevels; nLevel++)
		{
			resultVec.at(nLevel) = "call" + de::toString(nLevel);
		}

		return resultVec;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return tcu::UVec3(m_gridSizeXYZ[0], m_gridSizeXYZ[1], m_gridSizeXYZ[2]);
	}

	deUint32 getDynamicStackSize(const deUint32 maxPipelineRayRecursionDepth) const final
	{
		deUint32	result									= 0;
		const auto	maxStackSpaceNeededForZerothTrace		= static_cast<deUint32>(de::max(de::max(m_chitShaderStackSize, m_missShaderStackSize), m_isectShaderStackSize + m_ahitShaderStackSize) );
		const auto	maxStackSpaceNeededForNonZerothTraces	= static_cast<deUint32>(de::max(m_chitShaderStackSize, m_missShaderStackSize) );

		DE_ASSERT(m_useDynamicStackSize);

		result =	static_cast<deUint32>(m_raygenShaderStackSize)														+
					de::min(1u, maxPipelineRayRecursionDepth)		* maxStackSpaceNeededForZerothTrace					+
					de::max(0u, maxPipelineRayRecursionDepth - 1)	* maxStackSpaceNeededForNonZerothTraces				+
					m_nMaxCallableLevels							* static_cast<deUint32>(m_callableShaderStackSize);

		DE_ASSERT(result != 0);
		return result;
	}

	deUint32 getResultBufferSize() const final
	{
		const auto nRaysTraced							= m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
		const auto nClosestHitShaderInvocationsExpected	= nRaysTraced / 2;
		const auto nMissShaderInvocationsExpected		= nRaysTraced / 2;
		const auto resultItemSize						= sizeof(deUint32) * 3 /* shaderStage, nOriginRay, nLevel */ + sizeof(float) * m_nMaxCallableLevels;

		DE_ASSERT((nRaysTraced % 2)		== 0);
		DE_ASSERT(m_nMaxCallableLevels	!= 0);
		DE_ASSERT(m_gridSizeXYZ[0]		!= 0);
		DE_ASSERT(m_gridSizeXYZ[1]		!= 0);
		DE_ASSERT(m_gridSizeXYZ[2]		!= 0);

		return static_cast<deUint32>(sizeof(deUint32) /* nItemsStored */ + (resultItemSize * m_nMaxCallableLevels) * (nRaysTraced + nMissShaderInvocationsExpected + nClosestHitShaderInvocationsExpected) );
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return {m_tlPtr.get() };
	}

	bool init(	vkt::Context&			/* context    */,
				RayTracingProperties*	rtPropertiesPtr) final
	{
		DE_UNREF(rtPropertiesPtr);
		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0, 0, 0), /* gridStartXYZ          */
								tcu::Vec3 (1, 1, 1), /* gridCellSizeXYZ       */
								m_gridSizeXYZ,
								tcu::Vec3 (6, 0, 0), /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = asProviderPtr->createTLAS(	context,
												m_asStructureLayout,
												commandBuffer,
												0,			/* bottomLevelGeometryFlags */
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFeedbackPtr			*/
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		std::vector<std::string>	callableDataDefinitions		(m_nMaxCallableLevels);
		std::vector<std::string>	callableDataInDefinitions	(m_nMaxCallableLevels);

		for (deUint32 nCallableDataLevel = 0; nCallableDataLevel < m_nMaxCallableLevels; ++nCallableDataLevel)
		{
			const auto locationsPerCallableData	= (3 /* uints */ + (nCallableDataLevel + 1) /* dataChunks */);
			const auto callableDataLocation		= locationsPerCallableData * nCallableDataLevel;

			callableDataDefinitions.at(nCallableDataLevel) =
				"layout (location = " + de::toString(callableDataLocation) + ") callableDataEXT struct\n"
				"{\n"
				"    uint  shaderStage;\n"
				"    uint  nOriginRay;\n"
				"    uint  nLevel;\n"
				"    float dataChunk[" + de::toString(nCallableDataLevel + 1) + "];\n"
				"} callableData" + de::toString(nCallableDataLevel) + ";\n";

			callableDataInDefinitions.at(nCallableDataLevel) =
				"layout(location = " + de::toString(callableDataLocation) + ") callableDataInEXT struct\n"
				"{\n"
				"    uint  shaderStage;\n"
				"    uint  nOriginRay;\n"
				"    uint  nLevel;\n"
				"    float dataChunk[" + de::toString(nCallableDataLevel + 1) + "];\n"
				"} inData;\n";

			m_callableDataLevelToCallableDataLocation[nCallableDataLevel] = callableDataLocation;
		}

		const auto resultBufferDefinition =
			"struct ResultData\n"
			"{\n"
			"    uint  shaderStage;\n"
			"    uint  nOriginRay;\n"
			"    uint  nLevel;\n"
			"    float dataChunk[" + de::toString(m_nMaxCallableLevels) + "];\n"
			"};\n"
			"\n"
			"layout(set = 0, binding = 0, std430) buffer result\n"
			"{\n"
			"    uint       nInvocationsRegistered;\n"
			"    ResultData resultData[];\n"
			"};\n";

		{
			std::stringstream css;

			/* NOTE: executeCallable() is unavailable in ahit stage */
			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 128) rayPayloadInEXT uint dummy;\n"
				"\n"
				"void main()\n"
				"{\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 128) rayPayloadInEXT uint rayIndex;\n"
				"\n"											+
				de::toString(callableDataDefinitions.at(0) )	+
				de::toString(resultBufferDefinition)			+
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    callableData0.shaderStage  = 3;\n"
				"    callableData0.nOriginRay   = nInvocation;\n"
				"    callableData0.nLevel       = 0;\n"
				"    callableData0.dataChunk[0] = float(nInvocation);\n"
				"\n"
				"    executeCallableEXT(0 /* sbtRecordIndex */, " + de::toString(m_callableDataLevelToCallableDataLocation.at(0) ) + ");\n"
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			/* NOTE: executeCallable() is unavailable in isect stage */
			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n" +
				de::toString(callableDataDefinitions.at(0) )	+
				de::toString(resultBufferDefinition)			+
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    callableData0.shaderStage  = 2;\n"
				"    callableData0.nOriginRay   = nInvocation;\n"
				"    callableData0.nLevel       = 0;\n"
				"    callableData0.dataChunk[0] = float(nInvocation);\n"
				"\n"
				"    executeCallableEXT(0 /* sbtRecordIndex */, " + de::toString(m_callableDataLevelToCallableDataLocation.at(0) ) + ");\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(callableDataDefinitions.at(0) ) +
				"layout(location = 128)            rayPayloadEXT uint               dummy;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(nInvocation * 3.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    callableData0.shaderStage  = 0;\n"
				"    callableData0.nOriginRay   = nInvocation;\n"
				"    callableData0.nLevel       = 0;\n"
				"    callableData0.dataChunk[0] = float(nInvocation);\n"
				"\n"
				"    executeCallableEXT(0 /* sbtRecordIndex */, " + de::toString(m_callableDataLevelToCallableDataLocation.at(0) ) + ");\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 128);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}

		for (deUint32 nCallableShader = 0; nCallableShader < m_nMaxCallableLevels; ++nCallableShader)
		{
			const bool        canInvokeExecutable = (nCallableShader != (m_nMaxCallableLevels - 1) );
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+	de::toString(resultBufferDefinition);

			if ((nCallableShader + 1) != m_nMaxCallableLevels)
			{
				css << de::toString(callableDataDefinitions.at(nCallableShader + 1) );
			}

			css <<
				callableDataInDefinitions[nCallableShader]	+
				"\n"
				"void main()\n"
				"{\n"
				"    uint nInvocation = atomicAdd(nInvocationsRegistered, 1);\n"
				"\n"
				"    resultData[nInvocation].shaderStage = inData.shaderStage;\n"
				"    resultData[nInvocation].nOriginRay  = inData.nOriginRay;\n"
				"    resultData[nInvocation].nLevel      = inData.nLevel;\n";

			for (deUint32 nLevel = 0; nLevel < nCallableShader + 1; ++nLevel)
			{
				css <<
					"    resultData[nInvocation].dataChunk[" + de::toString(nLevel) + "] = inData.dataChunk[" + de::toString(nLevel) + "];\n";
			}

			if (canInvokeExecutable)
			{
				css <<
					"\n"
					"    callableData" + de::toString(nCallableShader + 1) + ".shaderStage = 1;\n"
					"    callableData" + de::toString(nCallableShader + 1) + ".nOriginRay  = inData.nOriginRay;\n"
					"    callableData" + de::toString(nCallableShader + 1) + ".nLevel      = " + de::toString(nCallableShader) + ";\n"
					"\n";

				for (deUint32 nLevel = 0; nLevel <= nCallableShader + 1; ++nLevel)
				{
					css <<
						"    callableData" + de::toString(nCallableShader + 1) + ".dataChunk[" + de::toString(nLevel) + "] = float(inData.nOriginRay + " + de::toString(nLevel) + ");\n";
				}

				css <<
					"\n"
					"    executeCallableEXT(" + de::toString(nCallableShader + 1) + ", " + de::toString(m_callableDataLevelToCallableDataLocation[nCallableShader + 1]) + ");\n";
			}

			css <<
				"\n"
				"};\n";

			programCollection.glslSources.add("call" + de::toString(nCallableShader) ) << glu::CallableSource(css.str() ) << buildOptions;
		}
	}

	void onShaderStackSizeDiscovered(	const VkDeviceSize& raygenShaderStackSize,
										const VkDeviceSize& ahitShaderStackSize,
										const VkDeviceSize& chitShaderStackSize,
										const VkDeviceSize& missShaderStackSize,
										const VkDeviceSize& callableShaderStackSize,
										const VkDeviceSize& isectShaderStackSize) final
	{
		m_ahitShaderStackSize		= ahitShaderStackSize;
		m_callableShaderStackSize	= callableShaderStackSize;
		m_chitShaderStackSize		= chitShaderStackSize;
		m_isectShaderStackSize		= isectShaderStackSize;
		m_missShaderStackSize		= missShaderStackSize;
		m_raygenShaderStackSize		= raygenShaderStackSize;
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	bool usesDynamicStackSize() const final
	{
		return m_useDynamicStackSize;
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result			= false;
		const auto		nItemsStored	= *resultU32Ptr;

		/* Convert raw binary data into a human-readable vector representation */
		struct ResultItem
		{
			VkShaderStageFlagBits	shaderStage;
			deUint32				nLevel;
			std::vector<float>		dataChunk;

			ResultItem()
				:	shaderStage (VK_SHADER_STAGE_ALL),
					nLevel		(0)
			{
				/* Stub */
			}
		};

		std::map<deUint32, std::vector<ResultItem> > nRayToResultItemVecMap;

		for (deUint32 nItem = 0; nItem < nItemsStored; ++nItem)
		{
			const deUint32* itemDataPtr = resultU32Ptr + 1 /* nItemsStored */ + nItem * (3 /* preamble ints */ + m_nMaxCallableLevels /* received data */);
			ResultItem		item;
			const auto&		nOriginRay  = *(itemDataPtr + 1);

			item.dataChunk.resize(m_nMaxCallableLevels);

			switch (*itemDataPtr)
			{
				case 0: item.shaderStage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;		break;
				case 1: item.shaderStage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;	break;
				case 2: item.shaderStage = VK_SHADER_STAGE_MISS_BIT_KHR;		break;
				case 3: item.shaderStage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;	break;

				default:
				{
					deAssertFail("This should never happen", __FILE__, __LINE__);
				}
			}

			item.nLevel = *(itemDataPtr + 2);

			memcpy(	item.dataChunk.data(),
					itemDataPtr + 3,
					m_nMaxCallableLevels * sizeof(float) );

			nRayToResultItemVecMap[nOriginRay].push_back(item);
		}

		for (deUint32 nRay = 0; nRay < m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2]; ++nRay)
		{
			/* 1. Make sure each ray generated the anticipated number of stores */
			const bool		closestHitShaderInvoked			=	(nRay % 2) == 0;
			const bool		missShaderInvoked				=	(nRay % 2) != 0;
			const deUint32	nShaderStagesInvokingCallables	=	1										+ /* raygen */
																((closestHitShaderInvoked)	? 1 : 0)	+
																((missShaderInvoked)		? 1 : 0);
			auto			rayIterator						= nRayToResultItemVecMap.find(nRay);

			if (rayIterator == nRayToResultItemVecMap.end())
			{
				goto end;
			}

			if (rayIterator->second.size() != nShaderStagesInvokingCallables * m_nMaxCallableLevels)
			{
				goto end;
			}

			/* 2. Make sure each shader stage generated the anticipated number of result items */
			{
				deUint32 nCallableShaderStageItemsFound		= 0;
				deUint32 nClosestHitShaderStageItemsFound	= 0;
				deUint32 nMissShaderStageItemsFound			= 0;
				deUint32 nRaygenShaderStageItemsFound		= 0;

				for (const auto& currentItem : rayIterator->second)
				{
					if (currentItem.shaderStage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
					{
						nRaygenShaderStageItemsFound++;
					}
					else
					if (currentItem.shaderStage == VK_SHADER_STAGE_CALLABLE_BIT_KHR)
					{
						nCallableShaderStageItemsFound ++;
					}
					else
					if (currentItem.shaderStage == VK_SHADER_STAGE_MISS_BIT_KHR)
					{
						nMissShaderStageItemsFound ++;
					}
					else
					if (currentItem.shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
					{
						nClosestHitShaderStageItemsFound ++;
					}
					else
					{
						DE_ASSERT(false);
					}
				}

				if (nRaygenShaderStageItemsFound != 1)
				{
					goto end;
				}

				/* Even rays hit geometry. Odd ones don't */
				if (!missShaderInvoked)
				{
					if (nClosestHitShaderStageItemsFound == 0)
					{
						goto end;
					}

					if (nMissShaderStageItemsFound != 0)
					{
						goto end;
					}
				}
				else
				{
					if (nClosestHitShaderStageItemsFound != 0)
					{
						goto end;
					}

					if (nMissShaderStageItemsFound != 1)
					{
						goto end;
					}
				}

				if (nCallableShaderStageItemsFound != nShaderStagesInvokingCallables * (m_nMaxCallableLevels - 1) )
				{
					goto end;
				}
			}

			/* 3. Verify data chunk's correctness */
			{
				for (const auto& currentItem : rayIterator->second)
				{
					const auto nValidItemsRequired =	(currentItem.shaderStage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)			? 1
													:	(currentItem.shaderStage == VK_SHADER_STAGE_MISS_BIT_KHR)			? 1
													:	(currentItem.shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)	? 1
																															: (currentItem.nLevel + 1);

					for (deUint32 nItem = 0; nItem < nValidItemsRequired; ++nItem)
					{
						if (fabsf(currentItem.dataChunk.at(nItem) - static_cast<float>(nRay + nItem)) > 1e-3f)
						{
							goto end;
						}
					}
				}
			}

			/* 4. Verify all shader levels have been reported for relevant shader stages */
			{
				std::map<VkShaderStageFlagBits, std::vector<deUint32> > shaderStageToLevelVecReportedMap;

				for (const auto& currentItem : rayIterator->second)
				{
					shaderStageToLevelVecReportedMap[currentItem.shaderStage].push_back(currentItem.nLevel);
				}

				if (shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_RAYGEN_BIT_KHR).size()  != 1 ||
					shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_RAYGEN_BIT_KHR).at  (0) != 0)
				{
					goto end;
				}

				if (closestHitShaderInvoked)
				{
					if (shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR).size()  != 1 ||
						shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR).at  (0) != 0)
					{
						goto end;
					}
				}
				else
				{
					if (shaderStageToLevelVecReportedMap.find(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) != shaderStageToLevelVecReportedMap.end() )
					{
						goto end;
					}
				}

				if (missShaderInvoked)
				{
					if (shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_MISS_BIT_KHR).size()  != 1 ||
						shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_MISS_BIT_KHR).at  (0) != 0)
					{
						goto end;
					}
				}
				else
				{
					if (shaderStageToLevelVecReportedMap.find(VK_SHADER_STAGE_MISS_BIT_KHR) != shaderStageToLevelVecReportedMap.end() )
					{
						goto end;
					}
				}

				if (shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_CALLABLE_BIT_KHR).size() != nShaderStagesInvokingCallables * (m_nMaxCallableLevels - 1))
				{
					goto end;
				}

				for (deUint32 nLevel = 0; nLevel < m_nMaxCallableLevels - 1; ++nLevel)
				{
					const auto& vec			= shaderStageToLevelVecReportedMap.at(VK_SHADER_STAGE_CALLABLE_BIT_KHR);
					auto		vecIterator = std::find(vec.begin	(),
														vec.end		(),
														nLevel);

					if (vecIterator == vec.end())
					{
						goto end;
					}
				}
			}
		}

	result = true;
end:
	return result;
}

 private:

	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	const tcu::UVec3								m_gridSizeXYZ;
	const deUint32									m_nMaxCallableLevels;
	const bool										m_useDynamicStackSize;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;

	VkDeviceSize m_ahitShaderStackSize;
	VkDeviceSize m_callableShaderStackSize;
	VkDeviceSize m_chitShaderStackSize;
	VkDeviceSize m_isectShaderStackSize;
	VkDeviceSize m_missShaderStackSize;
	VkDeviceSize m_raygenShaderStackSize;

	mutable std::map<deUint32, deUint32>	m_callableDataLevelToCallableDataLocation;
};

class CullMaskTest :	public TestBase,
						public ASPropertyProvider
{
public:
	CullMaskTest(	const AccelerationStructureLayout&	asLayout,
					const GeometryType&					geometryType,
					const bool&							useExtraCullMaskBits)
		:m_asLayout						(asLayout),
		 m_geometryType					(geometryType),
		 m_nMaxHitsToRegister			(256),
		 m_nRaysPerInvocation			(4),
		 m_useExtraCullMaskBits			(useExtraCullMaskBits),
		 m_lastCustomInstanceIndexUsed	(0),
		 m_nCullMasksUsed				(1)
	{
		/* Stub */
	}

	~CullMaskTest()
	{
		/* Stub */
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	deUint8 getCullMask(const deUint32& nBL, const deUint32& nInstance) const final
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		deUint8 result = (m_nCullMasksUsed++) & 0xFF;

		DE_ASSERT(result != 0);
		return result;
	}

	deUint32 getInstanceCustomIndex(const deUint32& nBL, const deUint32& nInstance) const final
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		/* NOTE: The formula below generates a sequence of unique large values. */
		deUint32 result = (m_lastCustomInstanceIndexUsed * 7 + 153325) & ((1 << 24) - 1);

		if (m_instanceCustomIndexVec.size() <= nInstance)
		{
			m_instanceCustomIndexVec.resize(nInstance + 1);
		}

		m_instanceCustomIndexVec		[nInstance] = result;
		m_lastCustomInstanceIndexUsed				= result;

		return result;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		//< 3*5*17 == 255, which coincidentally is the maximum cull mask value the spec permits.
		//<
		//< This global WG size is excessively large if m_nRaysPerInvocation > 1 but the raygen shader has
		//< a guard condition check that drops extraneous invocations.
		return tcu::UVec3(3, 5, 17);
	}

	deUint32 getResultBufferSize() const final
	{
		return static_cast<deUint32>((1 + m_nMaxHitsToRegister * 2) * sizeof(deUint32) );
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const	final
	{
		return {m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		m_asProviderPtr.reset(
				new GridASProvider(	tcu::Vec3	(0,		0,		0),		/* gridStartXYZ          */
									tcu::Vec3	(1,		1,		1),		/* gridCellSizeXYZ       */
									tcu::UVec3	(3,		5,		17),	/* gridSizeXYZ           */
									tcu::Vec3	(2.0f,	2.0f,	2.0f),  /* gridInterCellDeltaXYZ */
									m_geometryType)
			);

		m_tlPtr  = m_asProviderPtr->createTLAS(	context,
												m_asLayout,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												this,		/* optASPropertyProviderPtr */
												nullptr);	/* optASFeedbackPtr         */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const char* hitPropsDefinition =
			"struct HitProps\n"
			"{\n"
			"    uint rayIndex;\n"
			"    uint instanceCustomIndex;\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 dummyAttribute;\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT      uint   nRay;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint nHit = atomicAdd(nHitsRegistered, 1);\n"
				"\n"
				"    if (nHit < " + de::toString(m_nMaxHitsToRegister) + ")\n"
				"    {\n"
				"        hits[nHit].rayIndex            = nRay;\n"
				"        hits[nHit].instanceCustomIndex = gl_InstanceCustomIndexEXT;\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"layout(location = 0) rayPayloadInEXT uint rayIndex;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nMissesRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT uint               rayIndex;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    const uint nRaysPerInvocation = " + de::toString(m_nRaysPerInvocation) + ";\n"
				"\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = gl_RayFlagsCullBackFacingTrianglesEXT;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 4.0;\n"
				"\n"
				"    if (nInvocation >= 256 / nRaysPerInvocation)\n"
				"    {\n"
				"        return;\n"
				"    }\n"
				"\n"
				"    for (uint nRay = 0; nRay < nRaysPerInvocation; ++nRay)\n"
				"    {\n"
				"        uint  cullMask     = 1 + nInvocation * nRaysPerInvocation + nRay;\n";

			if (m_useExtraCullMaskBits)
			{
				css << "cullMask |= 0x00FFFFFF;\n";
			}

			css <<
				"        uint  nCell        = nInvocation * nRaysPerInvocation + nRay;\n"
				"        uvec3 cellXYZ      = uvec3(nCell % gl_LaunchSizeEXT.x, (nCell / gl_LaunchSizeEXT.x) % gl_LaunchSizeEXT.y, (nCell / gl_LaunchSizeEXT.x / gl_LaunchSizeEXT.y) % gl_LaunchSizeEXT.z);\n"
				"        vec3  cellStartXYZ = vec3(cellXYZ) * vec3(2.0);\n"
				"        vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"        vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"        vec3  origin       = target - vec3(1, 1, 1);\n"
				"        vec3  direct       = normalize(target - origin);\n"
				"\n"
				"        if (nCell < 255)\n"
				"        {\n"
				"            rayIndex = nCell;"
				"\n"
				"            traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"        }\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		const auto		nHitsReported	= *resultU32Ptr;
		const auto		nMissesReported	= *(resultU32Ptr + 1);
		bool			result			= true;

		// For each traced ray:
		//
		// 1. Exactly one ahit invocation per ray should be reported.
		// 2. All hits reported for a ray R should point to a primitive with a valid custom instance index
		// 3. The reported custom instance indices must be valid.
		std::map<deUint32, std::vector<deUint32> > customInstanceIndexToRayIndexVecMap;
		std::map<deUint32, std::vector<deUint32> > rayIndexToCustomInstanceIndexVecMap;

		typedef struct
		{
			deUint32 rayIndex;
			deUint32 customInstanceHit;
		} HitProperties;

		if (nHitsReported != 0xFF)
		{
			result = false;

			goto end;
		}

		if (nMissesReported != 0)
		{
			result = false;

			goto end;
		}

		for (deUint32 nHit = 0; nHit < nHitsReported; ++nHit)
		{
			const HitProperties* hitPropsPtr = reinterpret_cast<const HitProperties*>(resultU32Ptr + 2 /* preamble ints */ + nHit * 2 /* ints per HitProperties item */);

			customInstanceIndexToRayIndexVecMap[hitPropsPtr->customInstanceHit].push_back(hitPropsPtr->rayIndex);
			rayIndexToCustomInstanceIndexVecMap[hitPropsPtr->rayIndex].push_back         (hitPropsPtr->customInstanceHit);
		}

		if (static_cast<deUint32>(customInstanceIndexToRayIndexVecMap.size()) != nHitsReported)
		{
			/* Invalid number of unique custom instance indices reported. */
			result = false;

			goto end;
		}

		if (static_cast<deUint32>(rayIndexToCustomInstanceIndexVecMap.size()) != nHitsReported)
		{
			/* Invalid ray indices reported by ahit invocations */
			result = false;

			goto end;
		}

		for (const auto& currentItem : customInstanceIndexToRayIndexVecMap)
		{
			if (currentItem.second.size() != 1)
			{
				/* More than one ray associated with the same custom instance index */
				result = false;

				goto end;
			}

			if (currentItem.second.at(0) > 255)
			{
				/* Invalid ray index associated with the instance index */
				result = false;

				goto end;
			}

			if (std::find(	m_instanceCustomIndexVec.begin	(),
							m_instanceCustomIndexVec.end	(),
							currentItem.first)					== m_instanceCustomIndexVec.end() )
			{
				/* Invalid custom instance index reported for the ray */
				result = false;

				goto end;
			}
		}

		end:
			return result;
	}

private:

	const AccelerationStructureLayout	m_asLayout;
	const GeometryType					m_geometryType;
	const deUint32						m_nMaxHitsToRegister;
	const deUint32						m_nRaysPerInvocation;
	const bool							m_useExtraCullMaskBits;

	mutable std::vector<deUint32>	m_instanceCustomIndexVec;
	mutable deUint32				m_lastCustomInstanceIndexUsed;
	mutable deUint32				m_nCullMasksUsed;

	std::unique_ptr<GridASProvider>					m_asProviderPtr;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};



class MAXRayHitAttributeSizeTest: public TestBase
{
public:
	MAXRayHitAttributeSizeTest(	const GeometryType&					geometryType,
							const AccelerationStructureLayout&	asStructureLayout)
	:	m_asStructureLayout	(asStructureLayout),
		m_geometryType		(geometryType),
		m_gridSizeXYZ		(tcu::UVec3 (512, 1, 1) ),
		m_nRayAttributeU32s	(0)
	{
	}

	~MAXRayHitAttributeSizeTest()
	{
	/* Stub */
	}

	tcu::UVec3 getDispatchSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return tcu::UVec3(m_gridSizeXYZ[0], m_gridSizeXYZ[1], m_gridSizeXYZ[2]);
	}

	deUint32 getResultBufferSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return static_cast<deUint32>((3 /* nAHits, nCHits, nMisses */ + m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2] * m_nRayAttributeU32s * 2 /* stages where result data is stored */) * sizeof(deUint32) );
	}

	VkSpecializationInfo* getSpecializationInfoPtr(const VkShaderStageFlagBits& shaderStage) final
	{
		VkSpecializationInfo* resultPtr = nullptr;

		if (shaderStage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR	||
			shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	||
			shaderStage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
		{
			resultPtr = &m_specializationInfo;
		}

		return resultPtr;
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return {m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	bool init(	vkt::Context&			/* context    */,
				RayTracingProperties*	rtPropertiesPtr) final
	{
		const auto maxRayHitAttributeSize = rtPropertiesPtr->getMaxRayHitAttributeSize();

		// TODO: If U8s are supported, we could cover the remaining space with these..
		m_nRayAttributeU32s = maxRayHitAttributeSize / static_cast<deUint32>(sizeof(deUint32) );
		DE_ASSERT(m_nRayAttributeU32s != 0);

		m_specializationInfoMapEntry.constantID = 1;
		m_specializationInfoMapEntry.offset		= 0;
		m_specializationInfoMapEntry.size		= sizeof(deUint32);

		m_specializationInfo.dataSize			= sizeof(deUint32);
		m_specializationInfo.mapEntryCount		= 1;
		m_specializationInfo.pData				= reinterpret_cast<const void*>(&m_nRayAttributeU32s);
		m_specializationInfo.pMapEntries		= &m_specializationInfoMapEntry;

		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0, 0, 0), /* gridStartXYZ          */
								tcu::Vec3 (1, 1, 1), /* gridCellSizeXYZ       */
								m_gridSizeXYZ,
								tcu::Vec3 (6, 0, 0), /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = asProviderPtr->createTLAS(	context,
												m_asStructureLayout,
												commandBuffer,
												0,			/* bottomLevelGeometryFlags */
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFeedbackPtr         */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const char* constantDefinitions =
			"layout(constant_id = 1) const uint N_UINTS_IN_HIT_ATTRIBUTE = 1;\n";

		const char* hitAttributeDefinition =
			"\n"
			"hitAttributeEXT block\n"
			"{\n"
			"    uint values[N_UINTS_IN_HIT_ATTRIBUTE];\n"
			"};\n"
			"\n";

		const char* resultBufferDefinition =
			"layout(set      = 0, binding = 0, std430) buffer result\n"
			"{\n"
			"    uint nAHitsRegistered;\n"
			"    uint nCHitsRegistered;\n"
			"    uint nMissesRegistered;\n"
			"    uint retrievedValues[N_UINTS_IN_HIT_ATTRIBUTE];\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(hitAttributeDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT uint dummy;\n"
				+ de::toString(resultBufferDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nAHitsRegistered, 1);\n"
				"\n"
				"    uint nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_HIT_ATTRIBUTE; ++nUint)\n"
				"    {\n"
				"        retrievedValues[(2 * nInvocation + 1) * N_UINTS_IN_HIT_ATTRIBUTE + nUint] = values[nUint];\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(hitAttributeDefinition) +
				  de::toString(resultBufferDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT uint rayIndex;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nCHitsRegistered, 1);\n"
				"\n"
				"    uint nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_HIT_ATTRIBUTE; ++nUint)\n"
				"    {\n"
				"        retrievedValues[(2 * nInvocation + 0) * N_UINTS_IN_HIT_ATTRIBUTE + nUint] = values[nUint];\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(hitAttributeDefinition) +
				  de::toString(resultBufferDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    uint nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_HIT_ATTRIBUTE; ++nUint)\n"
				"    {\n"
				"        values[nUint] = 1 + nInvocation + nUint;\n"
				"    }\n"
				"\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+	de::toString(constantDefinitions)
				+	de::toString(resultBufferDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nMissesRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT uint               dummy;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(nInvocation * 3.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result			= false;


		const auto	nAHitsReported		= *resultU32Ptr;
		const auto	nCHitsRegistered	= *(resultU32Ptr + 1);
		const auto	nMissesRegistered	= *(resultU32Ptr + 2);

		if (nAHitsReported != m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2] / 2)
		{
			goto end;
		}

		if (nCHitsRegistered != nAHitsReported)
		{
			goto end;
		}

		if (nMissesRegistered != nAHitsReported)
		{
			goto end;
		}

		for (deUint32 nHit = 0; nHit < nAHitsReported; ++nHit)
		{
			const deUint32* ahitValues		= resultU32Ptr + 3 /* preamble ints */ + (2 * nHit + 0) * m_nRayAttributeU32s;
			const deUint32* chitValues		= resultU32Ptr + 3 /* preamble ints */ + (2 * nHit + 1) * m_nRayAttributeU32s;
			const bool		missExpected	= (nHit % 2) != 0;

			for (deUint32 nValue = 0; nValue < m_nRayAttributeU32s; ++nValue)
			{
				if (!missExpected)
				{
					if (ahitValues[nValue] != 1 + nHit + nValue)
					{
						goto end;
					}

					if (chitValues[nValue] != 1 + nHit + nValue)
					{
						goto end;
					}
				}
				else
				{
					if (ahitValues[nValue] != 0)
					{
						goto end;
					}

					if (chitValues[nValue] != 0)
					{
						goto end;
					}
				}
			}
		}

		result = true;
end:
		return result;
	}

private:

	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	const tcu::UVec3								m_gridSizeXYZ;
	deUint32										m_nRayAttributeU32s;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;

	VkSpecializationInfo		m_specializationInfo;
	VkSpecializationMapEntry	m_specializationInfoMapEntry;
};

class MAXRTInvocationsSupportedTest :	public TestBase,
										public ASPropertyProvider,
										public IGridASFeedback
{
 public:
	MAXRTInvocationsSupportedTest(	const GeometryType&					geometryType,
									const AccelerationStructureLayout&	asStructureLayout)
	:	m_asStructureLayout				(asStructureLayout),
		m_geometryType					(geometryType),
		m_lastCustomInstanceIndexUsed	(0),
		m_nMaxCells						(8 * 8 * 8)
	{
	}

	~MAXRTInvocationsSupportedTest()
	{
		/* Stub */
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	deUint32 getInstanceCustomIndex(const deUint32& nBL, const deUint32& nInstance) const final
	{
		DE_UNREF(nBL);
		DE_UNREF(nInstance);

		return ++m_lastCustomInstanceIndexUsed;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return tcu::UVec3(m_gridSizeXYZ[0], m_gridSizeXYZ[1], m_gridSizeXYZ[2]);
	}

	deUint32 getResultBufferSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return static_cast<deUint32>((2 /* nHits, nMisses */ + m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2] * 1 /* hit instance custom index */) * sizeof(deUint32) );
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return {m_tlPtr.get() };
	}

	bool init(	vkt::Context&			context,
				RayTracingProperties*	rtPropertiesPtr) final
	{
		/* NOTE: In order to avoid running into a situation where the test attempts to create a buffer of size larger than permitted by Vulkan,
		 *       we limit the maximum number of testable invocations to 2^29 on 64bit CTS build and driver or to 2^27 on 32bit */
		const auto		maxComputeWorkGroupCount		= context.getDeviceProperties().limits.maxComputeWorkGroupCount;
		const auto		maxComputeWorkGroupSize			= context.getDeviceProperties().limits.maxComputeWorkGroupSize;
		const deUint64	maxGlobalRTWorkGroupSize[3]		= {	static_cast<deUint64>(maxComputeWorkGroupCount[0]) * static_cast<deUint64>(maxComputeWorkGroupSize[0]),
															static_cast<deUint64>(maxComputeWorkGroupCount[1]) * static_cast<deUint64>(maxComputeWorkGroupSize[1]),
															static_cast<deUint64>(maxComputeWorkGroupCount[2]) * static_cast<deUint64>(maxComputeWorkGroupSize[2]) };
		const auto		maxRayDispatchInvocationCount	= de::min(	static_cast<deUint64>(rtPropertiesPtr->getMaxRayDispatchInvocationCount() ),
#if (DE_PTR_SIZE == 4)
																	static_cast<deUint64>(1ULL << 27) );
#else
																	static_cast<deUint64>(1ULL << 29) );
#endif

		m_gridSizeXYZ[0] = de::max(	1u,
									static_cast<deUint32>((maxRayDispatchInvocationCount)										% maxGlobalRTWorkGroupSize[0]) );
		m_gridSizeXYZ[1] = de::max(	1u,
									static_cast<deUint32>((maxRayDispatchInvocationCount / m_gridSizeXYZ[0])					% maxGlobalRTWorkGroupSize[1]) );
		m_gridSizeXYZ[2] = de::max(	1u,
									static_cast<deUint32>((maxRayDispatchInvocationCount / m_gridSizeXYZ[0] / m_gridSizeXYZ[1])	% maxGlobalRTWorkGroupSize[2]) );

		/* TODO: The simple formulas above may need to be improved to handle your implementation correctly */
		DE_ASSERT(m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2] == maxRayDispatchInvocationCount);

		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0,   0, 0), /* gridStartXYZ          */
								tcu::Vec3 (1,   1, 1), /* gridCellSizeXYZ       */
								tcu::UVec3(512, 1, 1), /* gridSizeXYZ           */
								tcu::Vec3 (3,   0, 0), /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = asProviderPtr->createTLAS(	context,
												m_asStructureLayout,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												this,	/* optASPropertyProviderPtr */
												this);	/* optASFeedbackPtr			*/
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const char* hitPropsDefinition =
			"struct HitProps\n"
			"{\n"
			"    uint instanceCustomIndex;\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 dummyAttribute;\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(location = 0) rayPayloadInEXT      uint   dummy;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nHitsRegistered, 1);\n"
				"\n"
				"    uint nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    hits[nInvocation].instanceCustomIndex = gl_InstanceCustomIndexEXT;\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(hitPropsDefinition) +
				"\n"
				"layout(set = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint     nHitsRegistered;\n"
				"    uint     nMissesRegistered;\n"
				"    HitProps hits[];\n"
				"};\n"
				"\n"
				"layout(location = 0) rayPayloadInEXT uint rayIndex;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    atomicAdd(nMissesRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT uint               dummy;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 2.1;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3( (nInvocation % " + de::toString(m_nMaxCells) + ") * 3, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result			= false;

		typedef struct
		{
			deUint32 instanceCustomIndex;
		} HitProperties;

		const auto	nHitsReported	= *resultU32Ptr;
		const auto	nMissesReported	= *(resultU32Ptr + 1);

		if (nHitsReported != m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2])
		{
			goto end;
		}

		if (nMissesReported != 0)
		{
			goto end;
		}

		for (deUint32 nRay = 0; nRay < nHitsReported; ++nRay)
		{
			const HitProperties* hitPropsPtr = reinterpret_cast<const HitProperties*>(resultU32Ptr + 2 /* preamble ints */) + nRay;

			if (m_nRayToInstanceIndexExpected.at(nRay % m_nMaxCells) != hitPropsPtr->instanceCustomIndex)
			{
				goto end;
			}
		}

		result = true;
end:
		return result;
	}

private:
	void onCullMaskAssignedToCell(const tcu::UVec3& cellLocation, const deUint8& cullMaskAssigned)
	{
		/* Dont'care */
		DE_UNREF(cellLocation);
		DE_UNREF(cullMaskAssigned);
	}

	void onInstanceCustomIndexAssignedToCell(const tcu::UVec3& cellLocation, const deUint32& customIndexAssigned)
	{
		DE_ASSERT(cellLocation[1] == 0);
		DE_ASSERT(cellLocation[2] == 0);

		m_nRayToInstanceIndexExpected[cellLocation[0] ] = customIndexAssigned;
	}

	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	tcu::UVec3										m_gridSizeXYZ;
	mutable deUint32								m_lastCustomInstanceIndexUsed;
	const deUint32									m_nMaxCells;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;

	std::map<deUint32, deUint32> m_nRayToInstanceIndexExpected;
};

class NoDuplicateAnyHitTest : public TestBase
{
public:
	NoDuplicateAnyHitTest(	const AccelerationStructureLayout&	asLayout,
							const GeometryType&					geometryType)
		:m_asLayout			(asLayout),
		 m_geometryType		(geometryType),
		 m_gridSizeXYZ		(tcu::UVec3(4, 4, 4) ),
		 m_nRaysToTrace		(32)
	{
		/* Stub */
	}

	~NoDuplicateAnyHitTest()
	{
		/* Stub */
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(4, 4, m_nRaysToTrace / (4 * 4) + 1);
	}

	deUint32 getResultBufferSize() const final
	{
		const auto nPrimitives = m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];

		return static_cast<deUint32>((2 /* nHits, nMisses */ + 3 * nPrimitives /* instancePrimitiveIDPairsUsed */) * sizeof(deUint32) * m_nRaysToTrace);
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const	final
	{
		return {m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		m_asProviderPtr.reset(
			new GridASProvider(	tcu::Vec3	(0,		0,		0),		/* gridStartXYZ          */
								tcu::Vec3	(1,		1,		1),		/* gridCellSizeXYZ       */
								m_gridSizeXYZ,
								tcu::Vec3	(2.0f,	2.0f,	2.0f),  /* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = m_asProviderPtr->createTLAS(	context,
												m_asLayout,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFedbackPtr          */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const auto nTotalPrimitives			= m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
		const auto hitPropertiesDefinition	=	"struct HitProperties\n"
												"{\n"
												"    uint nHitsRegistered;\n"
												"	 uint nMissRegistered;\n"
												"    uint instancePrimitiveIDPairsUsed[3 * " + de::toString(nTotalPrimitives) + "];\n"
												"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 dummyAttribute;\n"
				"\n"
				+ hitPropertiesDefinition +
				"\n"
				"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    HitProperties rayToHitProps[" << de::toString(m_nRaysToTrace) << "];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint nRay            = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint nHitsRegistered = atomicAdd(rayToHitProps[nRay].nHitsRegistered, 1);\n"
				"\n"
				"    rayToHitProps[nRay].instancePrimitiveIDPairsUsed[3 * nHitsRegistered + 0] = 1 + gl_InstanceID;\n"
				"    rayToHitProps[nRay].instancePrimitiveIDPairsUsed[3 * nHitsRegistered + 1] = 1 + gl_PrimitiveID;\n"
				"    rayToHitProps[nRay].instancePrimitiveIDPairsUsed[3 * nHitsRegistered + 2] = 1 + gl_GeometryIndexEXT;\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ hitPropertiesDefinition +
				"layout(location = 0) rayPayloadInEXT      vec3   dummy;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    HitProperties rayToHitProps[" << de::toString(m_nRaysToTrace) << "];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint nRay = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"
				"    atomicAdd(rayToHitProps[nRay].nMissRegistered, 1);\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ hitPropertiesDefinition +
				"layout(location = 0)              rayPayloadEXT vec3                     dummy;\n"
				"layout(set      = 0, binding = 1) uniform       accelerationStructureEXT topLevelAS;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags    = 0;\n"
				"    uint  cullMask    = 0xFF;\n"
				"    float tmin        = 0.001;\n"
				"    float tmax        = 9.0;\n"
				"    vec3  origin      = vec3(4,                                  4,                                  4);\n"
				"    vec3  target      = vec3(float(gl_LaunchIDEXT.x * 2) + 0.5f, float(gl_LaunchIDEXT.y * 2) + 0.5f, float(gl_LaunchIDEXT.z * 2) + 0.5f);\n"
				"    vec3  direct      = normalize(target - origin);\n"
				"\n"
				"    if (nInvocation >= " << m_nRaysToTrace << ")\n"
				"    {\n"
				"        return;\n"
				"    }\n"
				"\n"
				"    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const auto	nTotalPrimitives	= m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
		bool		result				= true;

		for (deUint32 nRay = 0; nRay < m_nRaysToTrace; ++nRay)
		{
			std::vector<std::tuple<deUint32, deUint32, deUint32> >	tupleVec;
			const auto												rayProps	= reinterpret_cast<const deUint32*>(resultDataPtr) + (2 + 3 * nTotalPrimitives) * nRay;

			// 1. At least one ahit invocation must have been made.
			if (rayProps[0] == 0)
			{
				result = false;

				goto end;
			}

			// 2. It's OK for each ray to intersect many AABBs, but no AABB should have had >1 ahit invocation fired.
			for (deUint32 nPrimitive = 0; nPrimitive < nTotalPrimitives; nPrimitive++)
			{
				const auto instanceID    = rayProps[2 /* nHits, nMissesRegistered */ + 3 * nPrimitive + 0];
				const auto primitiveID   = rayProps[2 /* nHits, nMissesRegistered */ + 3 * nPrimitive + 1];
				const auto geometryIndex = rayProps[2 /* nHits, nMissesRegistered */ + 3 * nPrimitive + 2];

				const auto currentTuple = std::tuple<deUint32, deUint32, deUint32>(instanceID, primitiveID, geometryIndex);

				if (instanceID		!= 0 ||
					primitiveID		!= 0 ||
					geometryIndex	!= 0)
				{
					if (std::find(	tupleVec.begin(),
									tupleVec.end  (),
									currentTuple) != tupleVec.end() )
					{
						result = false;

						goto end;
					}

					tupleVec.push_back(currentTuple);
				}
			}

			// 3. None of the traced rays should have triggered the miss shader invocation.
			if (rayProps[1] != 0)
			{
				result = false;

				goto end;
			}
		}

		end:
			return result;
	}

private:
	const AccelerationStructureLayout	m_asLayout;
	const GeometryType					m_geometryType;
	const tcu::UVec3					m_gridSizeXYZ;
	const deUint32						m_nRaysToTrace;

	std::unique_ptr<GridASProvider>					m_asProviderPtr;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars1 =
{
	VariableType::FLOAT,
	VariableType::VEC2,
	VariableType::VEC3,
	VariableType::VEC4,

	VariableType::MAT2,
	VariableType::MAT2X2,
	VariableType::MAT2X3,
	VariableType::MAT2X4,
	VariableType::MAT3,
	VariableType::MAT3X2,
	VariableType::MAT3X3,
	VariableType::MAT3X4,
	VariableType::MAT4,
	VariableType::MAT4X2,
	VariableType::MAT4X3,
	VariableType::MAT4X4,

	VariableType::INT,
	VariableType::IVEC2,
	VariableType::IVEC3,
	VariableType::IVEC4,

	VariableType::UINT,
	VariableType::UVEC2,
	VariableType::UVEC3,
	VariableType::UVEC4,
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars2 =
{
	VariableType::DOUBLE,
	VariableType::DVEC2,
	VariableType::DVEC3,
	VariableType::DVEC4,

	VariableType::DMAT2,
	VariableType::DMAT2X2,
	VariableType::DMAT2X3,
	VariableType::DMAT2X4,
	VariableType::DMAT3,
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars3 =
{
	VariableType::DMAT3X2,
	VariableType::DMAT3X3,
	VariableType::DMAT3X4,
	VariableType::DMAT4,
	VariableType::DMAT4X2,
	VariableType::DMAT4X3,
	VariableType::DMAT4X4,
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars4 =
{
	VariableType::VEC3,
	VariableType::VEC4,

	VariableType::INT16,
	VariableType::I16VEC2,
	VariableType::I16VEC3,
	VariableType::I16VEC4,

	VariableType::MAT3X3,
	VariableType::MAT3X4,
	VariableType::MAT4X3,

	VariableType::UINT16,
	VariableType::U16VEC2,
	VariableType::U16VEC3,
	VariableType::U16VEC4,
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars5 =
{
	VariableType::VEC3,
	VariableType::VEC4,

	VariableType::INT64,
	VariableType::I64VEC2,
	VariableType::I64VEC3,
	VariableType::I64VEC4,

	VariableType::MAT3X3,
	VariableType::MAT3X4,
	VariableType::MAT4X3,

	VariableType::UINT64,
	VariableType::U64VEC2,
	VariableType::U64VEC3,
	VariableType::U64VEC4,
};

const std::vector<VariableType> g_ShaderRecordBlockTestVars6 =
{
	VariableType::VEC3,
	VariableType::VEC4,

	VariableType::INT8,
	VariableType::I8VEC2,
	VariableType::I8VEC3,
	VariableType::I8VEC4,

	VariableType::MAT3X3,
	VariableType::MAT3X4,
	VariableType::MAT4X3,

	VariableType::UINT8,
	VariableType::U8VEC2,
	VariableType::U8VEC3,
	VariableType::U8VEC4,
};

class ShaderRecordBlockTest : public TestBase
{
public:
	ShaderRecordBlockTest(	const TestType& testType, const std::vector<VariableType>& varTypesToTest)
		:	m_gridSizeXYZ		(tcu::UVec3(2, 2, 2) ),
			m_testType			(testType),
			m_varTypesToTest	(varTypesToTest),
			m_resultBufferSize	(0),
			m_shaderRecordSize	(0)
	{
		initTestItems();
	}

	~ShaderRecordBlockTest()
	{
		/* Stub */
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(3, 1, 1);
	}

	deUint32 getResultBufferSize() const final
	{
		return m_resultBufferSize;
	}

	const void* getShaderRecordData(const ShaderGroups& shaderGroup) const final
	{
		return		(shaderGroup == ShaderGroups::HIT_GROUP)		? m_shaderGroupToRecordDataMap.at(shaderGroup).data()
				:	(shaderGroup == ShaderGroups::MISS_GROUP)		? m_shaderGroupToRecordDataMap.at(shaderGroup).data()
																	: nullptr;
	}

	deUint32 getShaderRecordSize(const ShaderGroups& shaderGroup) const final
	{
		DE_ASSERT(m_shaderRecordSize != 0);

		return ((shaderGroup == ShaderGroups::HIT_GROUP)	||
				(shaderGroup == ShaderGroups::MISS_GROUP))	? m_shaderRecordSize
															: 0;
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const	final
	{
		return {m_tlPtr.get() };
	}

	static std::vector<VariableType> getVarsToTest(const TestType& testType)
	{
		return		((testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1) || (testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1) || (testType == TestType::SHADER_RECORD_BLOCK_SCALAR_1) || (testType == TestType::SHADER_RECORD_BLOCK_STD430_1))	? g_ShaderRecordBlockTestVars1
				:	((testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2) || (testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2) || (testType == TestType::SHADER_RECORD_BLOCK_SCALAR_2) || (testType == TestType::SHADER_RECORD_BLOCK_STD430_2))	? g_ShaderRecordBlockTestVars2
				:	((testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3) || (testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3) || (testType == TestType::SHADER_RECORD_BLOCK_SCALAR_3) || (testType == TestType::SHADER_RECORD_BLOCK_STD430_3))	? g_ShaderRecordBlockTestVars3
				:	((testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4) || (testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4) || (testType == TestType::SHADER_RECORD_BLOCK_SCALAR_4) || (testType == TestType::SHADER_RECORD_BLOCK_STD430_4))	? g_ShaderRecordBlockTestVars4
				:	((testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5) || (testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5) || (testType == TestType::SHADER_RECORD_BLOCK_SCALAR_5) || (testType == TestType::SHADER_RECORD_BLOCK_STD430_5))	? g_ShaderRecordBlockTestVars5
																																																																					: g_ShaderRecordBlockTestVars6;
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	bool init(	vkt::Context&			/* context */,
				RayTracingProperties*	/* rtPropsPtr */) final
	{
		// Cache required result buffer size.
		{
			deUint32					largestBaseTypeSizeUsed		= 0;
			const auto&					lastItem					= m_testItems.items.back();
			const deUint32				nResultBytesPerShaderStage	= lastItem.resultBufferProps.bufferOffset + lastItem.arraySize * lastItem.resultBufferProps.arrayStride;
			const VkShaderStageFlagBits shaderStages[]				=
			{
				VK_SHADER_STAGE_MISS_BIT_KHR,
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
				VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			};

			m_shaderRecordSize = lastItem.inputBufferProps.bufferOffset + lastItem.arraySize * lastItem.inputBufferProps.arrayStride;

			for (const auto& currentTestItem : m_testItems.items)
			{
				const auto baseType			= getBaseType			(currentTestItem.type);
				const auto componentSize	= getComponentSizeBytes	(baseType);

				largestBaseTypeSizeUsed = de::max(componentSize, largestBaseTypeSizeUsed);
			}

			for (const auto& currentShaderStage : shaderStages)
			{
				m_shaderStageToResultBufferOffset[currentShaderStage] = m_resultBufferSize;

				m_resultBufferSize  = de::roundUp(m_resultBufferSize, static_cast<deUint32>(sizeof(largestBaseTypeSizeUsed)) );
				m_resultBufferSize += nResultBytesPerShaderStage;
			}
		}

		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		m_asProviderPtr.reset(
			new GridASProvider(	tcu::Vec3	(0,		0,		0),		/* gridStartXYZ          */
								tcu::Vec3	(1,		1,		1),		/* gridCellSizeXYZ       */
								m_gridSizeXYZ,
								tcu::Vec3	(2.0f,	2.0f,	2.0f),  /* gridInterCellDeltaXYZ */
								GeometryType::AABB)
		);

		m_tlPtr  = m_asProviderPtr->createTLAS(	context,
												AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFedbackPtr          */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */


		const bool isSTD430Test					=	isExplicitSTD430OffsetTest	(m_testType) ||
													isSTD430LayoutTest			(m_testType);
		const bool requires16BitStorage			=	usesI16						(m_testType) ||
													usesU16						(m_testType);
		const bool requires8BitStorage			=	usesI8						(m_testType) ||
													usesU8						(m_testType);
		const bool requiresInt64				=	usesI64						(m_testType) ||
													usesU64						(m_testType);
		const bool usesExplicitOffsets			=	isExplicitScalarOffsetTest	(m_testType) ||
													isExplicitSTD430OffsetTest	(m_testType);
		const auto inputBlockVariablesGLSL		=	getGLSLForStructItem		(	m_testItems,
																					usesExplicitOffsets,
																					true	/* targetsInputBuffer			*/);
		const auto outputStructVariablesGLSL	=	getGLSLForStructItem		(	m_testItems,
																					false,	/* includeOffsetLayoutQualifier */
																					false	/* targetsInputBuffer			*/);

		const auto inputBufferGLSL				=	"layout (" + std::string((!isSTD430Test) ? "scalar, " : "std430, ") + "shaderRecordEXT) buffer ib\n"
													"{\n"						+
													inputBlockVariablesGLSL		+
													"} inputBuffer;\n";
		const auto outputBufferGLSL				=	"struct OutputData\n"
													"{\n"						+
													outputStructVariablesGLSL	+
													"};\n"
													"\n"
													"layout (std430, set = 0, binding = 0) buffer ob\n"
													"{\n"
													"    OutputData results[4];\n"
													"};\n";

		std::string preamble;

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n";

			if (!isSTD430Test)
			{
				css << "#extension GL_EXT_scalar_block_layout : require\n";
			}

			if (requires16BitStorage)
			{
				css << "#extension GL_EXT_shader_16bit_storage : require\n";
			}

			if (requires8BitStorage)
			{
				css << "#extension GL_EXT_shader_8bit_storage : require\n";
			}

			if (requiresInt64)
			{
				css << "#extension GL_ARB_gpu_shader_int64 : require\n";
			}

			preamble = css.str();
		}

		{
			std::stringstream css;

			css << preamble
				<<
				"\n"
				"                     hitAttributeEXT         vec3 dummyAttribute;\n"
				"layout(location = 0) rayPayloadInEXT dummy { vec3 dummyVec;};\n"
				"\n"				+
				inputBufferGLSL		+
				outputBufferGLSL	+
				"\n"
				"void main()\n"
				"{\n"								+
				getGLSLForSetters(m_testItems, 3)	+
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css << preamble
				<<
				"\n"
				"layout(location = 0) rayPayloadInEXT dummy { vec3 dummyVec;};\n"	+
				inputBufferGLSL														+
				outputBufferGLSL													+
				"\n"
				"void main()\n"
				"{\n"																+
				getGLSLForSetters(m_testItems, 1)									+
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css << preamble
				<<
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"				+
				inputBufferGLSL		+
				outputBufferGLSL	+
				"\n"
				"void main()\n"
				"{\n"								+
				getGLSLForSetters(m_testItems, 2)	+
				"\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css << preamble
				<<
				"\n"
				"layout(location = 0) rayPayloadInEXT vec3 dummy;\n"
				"\n"				+
				inputBufferGLSL		+
				outputBufferGLSL	+
				"\n"
				"void main()\n"
				"{\n"
				"    uint nRay = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"\n"								+
				getGLSLForSetters(m_testItems, 0)	+
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css << preamble
				<<
				"layout(location = 0)                      rayPayloadEXT vec3       dummy;\n"
				"layout(set      = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(nInvocation * 2.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	static bool isExplicitScalarOffsetTest(const TestType& testType)
	{
		return	(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6);
	}

	static bool isExplicitSTD430OffsetTest(const TestType& testType)
	{
		return	(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5) ||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6);
	}

	static bool isScalarLayoutTest(const TestType& testType)
	{
		return	(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_1) ||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_2) ||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_3) ||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_4) ||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_5) ||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_6);
	}

	static bool isSTD430LayoutTest(const TestType& testType)
	{
		return	(testType == TestType::SHADER_RECORD_BLOCK_STD430_1) ||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_2) ||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_3) ||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_4) ||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_5) ||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_6);
	}

	static bool isTest(const TestType& testType)
	{
		return	(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5)	||
				(testType == TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6)	||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_1)					||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_2)					||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_3)					||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_4)					||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_5)					||
				(testType == TestType::SHADER_RECORD_BLOCK_SCALAR_6)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_1)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_2)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_3)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_4)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_5)					||
				(testType == TestType::SHADER_RECORD_BLOCK_STD430_6);
	}

	static bool usesF64(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_f64			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DOUBLE) != tested_var_types.end();
		const bool has_f64vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DVEC2) != tested_var_types.end();
		const bool has_f64vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DVEC3) != tested_var_types.end();
		const bool has_f64vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DVEC4) != tested_var_types.end();
		const bool has_f64mat2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DMAT2) != tested_var_types.end();
		const bool has_f64mat3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DMAT3) != tested_var_types.end();
		const bool has_f64mat4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::DMAT4) != tested_var_types.end();

		return (has_f64 || has_f64vec2 || has_f64vec3 || has_f64vec4 || has_f64mat2 || has_f64mat3 || has_f64mat4);
	}

	static bool usesI8(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_i8			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::INT8) != tested_var_types.end();
		const bool has_i8vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I8VEC2) != tested_var_types.end();
		const bool has_i8vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I8VEC3) != tested_var_types.end();
		const bool has_i8vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I8VEC4) != tested_var_types.end();

		return (has_i8 || has_i8vec2 || has_i8vec3 || has_i8vec4);
	}

	static bool usesI16(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_i16			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::INT16) != tested_var_types.end();
		const bool has_i16vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I16VEC2) != tested_var_types.end();
		const bool has_i16vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I16VEC3) != tested_var_types.end();
		const bool has_i16vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I16VEC4) != tested_var_types.end();

		return (has_i16 || has_i16vec2 || has_i16vec3 || has_i16vec4);
	}

	static bool usesI64(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_i64			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::INT64) != tested_var_types.end();
		const bool has_i64vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I64VEC2) != tested_var_types.end();
		const bool has_i64vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I64VEC3) != tested_var_types.end();
		const bool has_i64vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::I64VEC4) != tested_var_types.end();

		return (has_i64 || has_i64vec2 || has_i64vec3 || has_i64vec4);
	}

	static bool usesU8(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_u8			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::UINT8) != tested_var_types.end();
		const bool has_u8vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U8VEC2) != tested_var_types.end();
		const bool has_u8vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U8VEC3) != tested_var_types.end();
		const bool has_u8vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U8VEC4) != tested_var_types.end();

		return (has_u8 || has_u8vec2 || has_u8vec3 || has_u8vec4);
	}

	static bool usesU16(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_u16			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::UINT16) != tested_var_types.end();
		const bool has_u16vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U16VEC2) != tested_var_types.end();
		const bool has_u16vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U16VEC3) != tested_var_types.end();
		const bool has_u16vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U16VEC4) != tested_var_types.end();

		return (has_u16 || has_u16vec2 || has_u16vec3 || has_u16vec4);
	}

	static bool usesU64(const TestType& testType)
	{
		const auto tested_var_types = getVarsToTest(testType);
		const bool has_u64			= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::UINT64) != tested_var_types.end();
		const bool has_u64vec2		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U64VEC2) != tested_var_types.end();
		const bool has_u64vec3		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U64VEC3) != tested_var_types.end();
		const bool has_u64vec4		= std::find	(	tested_var_types.begin	(),
													tested_var_types.end	(),
													VariableType::U64VEC4) != tested_var_types.end();

		return (has_u64 || has_u64vec2 || has_u64vec3 || has_u64vec4);
	}

	bool verifyResultBuffer (const void* resultBufferDataPtr) const final
	{
		bool result	= false;

		for (const auto& iterator : m_shaderStageToResultBufferOffset)
		{
			const auto currentShaderStage	= iterator.first;
			const auto shaderGroup			= (	(currentShaderStage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)			||
												(currentShaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)		||
												(currentShaderStage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR) )	? ShaderGroups::HIT_GROUP
																												: ShaderGroups::MISS_GROUP;
			const auto resultStartOffset	= iterator.second;

			if (currentShaderStage != VK_SHADER_STAGE_MISS_BIT_KHR) continue;

			for (const auto& currentItem : m_testItems.items)
			{
				const auto		baseDataType	= getBaseType								(currentItem.type);
				const auto		componentSize	= getComponentSizeBytes						(baseDataType);
				const auto&		expectedDataVec = currentItem.shaderGroupToRecordDataMap.at	(shaderGroup);
				auto			expectedDataPtr = reinterpret_cast<const deUint8*>			(expectedDataVec.data() );
				const auto		isMatrixType	= isMatrix									(currentItem.type);
				const auto		nComponents		= getNComponents							(currentItem.type);
				const deUint8*	resultDataPtr	= reinterpret_cast<const deUint8*>			(resultBufferDataPtr) + resultStartOffset + currentItem.resultBufferProps.bufferOffset;

				for (deUint32 nArrayItem = 0; nArrayItem < currentItem.arraySize; ++nArrayItem)
				{
					for (deUint32 nComponent = 0; nComponent < nComponents; ++nComponent)
					{
						const auto expectedComponentDataPtr	= expectedDataPtr	+ ((!isMatrixType)	? componentSize * nComponent
																									: currentItem.inputBufferProps.matrixElementStartOffsets.at(nComponent) );
						const auto resultComponentDataPtr	= resultDataPtr		+ ((!isMatrixType)	? componentSize * nComponent
																									: currentItem.resultBufferProps.matrixElementStartOffsets.at(nComponent) );

						switch (baseDataType)
						{
							case BaseType::F32:
							{
								if (fabs(*reinterpret_cast<const float*>(resultComponentDataPtr) - *reinterpret_cast<const float*>(expectedComponentDataPtr)) > 1e-3f)
								{
									goto end;
								}

								break;
							}

							case BaseType::F64:
							{
								if (fabs(*reinterpret_cast<const double*>(resultComponentDataPtr) - *reinterpret_cast<const double*>(expectedComponentDataPtr)) > 1e-3)
								{
									goto end;
								}

								break;
							}

							case BaseType::I8:
							{
								if (*reinterpret_cast<const deInt8*>(resultComponentDataPtr) != *reinterpret_cast<const deInt8*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::I16:
							{
								if (*reinterpret_cast<const deInt16*>(resultComponentDataPtr) != *reinterpret_cast<const deInt16*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::I32:
							{
								if (*reinterpret_cast<const deInt32*>(resultComponentDataPtr) != *reinterpret_cast<const deInt32*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::I64:
							{
								if (*reinterpret_cast<const deInt64*>(resultComponentDataPtr) != *reinterpret_cast<const deInt64*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::U8:
							{
								if (*reinterpret_cast<const deUint8*>(resultComponentDataPtr) != *reinterpret_cast<const deUint8*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::U16:
							{
								if (*reinterpret_cast<const deUint16*>(resultComponentDataPtr) != *reinterpret_cast<const deUint16*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::U32:
							{
								if (*reinterpret_cast<const deUint32*>(resultComponentDataPtr) != *reinterpret_cast<const deUint32*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							case BaseType::U64:
							{
								if (*reinterpret_cast<const deUint64*>(resultComponentDataPtr) != *reinterpret_cast<const deUint64*>(expectedComponentDataPtr))
								{
									goto end;
								}

								break;
							}

							default:
							{
								DE_ASSERT(false);
							}
						}
					}

					expectedDataPtr	+= currentItem.inputBufferProps.arrayStride;
					resultDataPtr	+= currentItem.resultBufferProps.arrayStride;
				}
			}
		}

		result = true;
end:
		return result;
	}

private:
	typedef struct Item
	{
		struct BufferProps
		{
			deUint32				arrayStride;
			deUint32				bufferOffset;
			std::vector<deUint32>	matrixElementStartOffsets; //< Holds offsets to consecutive matrix element values.

			BufferProps()
				:	arrayStride	(0),
					bufferOffset(0xFFFFFFFF)
			{
				/* Stub */
			}
		};

		BufferProps inputBufferProps;
		BufferProps resultBufferProps;

		deUint32				arraySize;
		MatrixMajorOrder		matrixOrder;
		std::string				name;
		VariableType			type;

		std::map<ShaderGroups, std::vector<deUint8> >	shaderGroupToRecordDataMap;


		Item()
			:arraySize		(0),
			 matrixOrder	(MatrixMajorOrder::UNKNOWN),
			 type			(VariableType::UNKNOWN)
		{
			/* Stub */
		}
	} Item;

	struct StructItem
	{
		std::vector<Item> items;
	};

	// Private functions
	BaseType	getBaseType(const VariableType& type) const
	{
		auto result = BaseType::UNKNOWN;

		switch (type)
		{
			case VariableType::FLOAT:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			case VariableType::MAT2X3:
			case VariableType::MAT2X4:
			case VariableType::MAT3:
			case VariableType::MAT3X2:
			case VariableType::MAT3X3:
			case VariableType::MAT3X4:
			case VariableType::MAT4:
			case VariableType::MAT4X2:
			case VariableType::MAT4X3:
			case VariableType::MAT4X4:
			case VariableType::VEC2:
			case VariableType::VEC3:
			case VariableType::VEC4:
			{
				result = BaseType::F32;

				break;
			}

			case VariableType::DOUBLE:
			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::DMAT2X3:
			case VariableType::DMAT2X4:
			case VariableType::DMAT3:
			case VariableType::DMAT3X2:
			case VariableType::DMAT3X3:
			case VariableType::DMAT3X4:
			case VariableType::DMAT4:
			case VariableType::DMAT4X2:
			case VariableType::DMAT4X3:
			case VariableType::DMAT4X4:
			case VariableType::DVEC2:
			case VariableType::DVEC3:
			case VariableType::DVEC4:
			{
				result = BaseType::F64;

				break;
			}

			case VariableType::INT16:
			case VariableType::I16VEC2:
			case VariableType::I16VEC3:
			case VariableType::I16VEC4:
			{
				result = BaseType::I16;

				break;
			}

			case VariableType::INT:
			case VariableType::IVEC2:
			case VariableType::IVEC3:
			case VariableType::IVEC4:
			{
				result = BaseType::I32;

				break;
			}

			case VariableType::INT64:
			case VariableType::I64VEC2:
			case VariableType::I64VEC3:
			case VariableType::I64VEC4:
			{
				result = BaseType::I64;

				break;
			}

			case VariableType::INT8:
			case VariableType::I8VEC2:
			case VariableType::I8VEC3:
			case VariableType::I8VEC4:
			{
				result = BaseType::I8;

				break;
			}

			case VariableType::UINT16:
			case VariableType::U16VEC2:
			case VariableType::U16VEC3:
			case VariableType::U16VEC4:
			{
				result = BaseType::U16;

				break;
			}

			case VariableType::UINT:
			case VariableType::UVEC2:
			case VariableType::UVEC3:
			case VariableType::UVEC4:
			{
				result = BaseType::U32;

				break;
			}

			case VariableType::UINT64:
			case VariableType::U64VEC2:
			case VariableType::U64VEC3:
			case VariableType::U64VEC4:
			{
				result = BaseType::U64;

				break;
			}

			case VariableType::UINT8:
			case VariableType::U8VEC2:
			case VariableType::U8VEC3:
			case VariableType::U8VEC4:
			{
				result = BaseType::U8;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	deUint32 getComponentSizeBytes(const BaseType& type) const
	{
		deUint32 result = 0;

		switch (type)
		{
			case BaseType::I8:
			case BaseType::U8:
			{
				result = 1;

				break;
			}

			case BaseType::I16:
			case BaseType::U16:
			{
				result = 2;

				break;
			}

			case BaseType::F32:
			case BaseType::I32:
			case BaseType::U32:
			{
				result = 4;

				break;
			}

			case BaseType::F64:
			case BaseType::I64:
			case BaseType::U64:
			{
				result = 8;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	std::string getGLSLForSetters(	const StructItem&	item,
									const deUint32&		nResultArrayItem) const
	{
		std::string result;

		for (const auto& currentItem : item.items)
		{
			if (currentItem.arraySize > 1)
			{
				result +=	"for (uint nArrayItem = 0; nArrayItem < " + de::toString(currentItem.arraySize) + "; ++nArrayItem)\n"
							"{\n";
			}

			result += "results[" + de::toString(nResultArrayItem) + "]." + currentItem.name;

			if (currentItem.arraySize > 1)
			{
				result += "[nArrayItem]";
			}

			result += " = inputBuffer." + currentItem.name;

			if (currentItem.arraySize > 1)
			{
				result += "[nArrayItem]";
			}

			result += ";\n";

			if (currentItem.arraySize > 1)
			{
				result += "}\n";
			}
		}

		return result;
	}

	std::string getGLSLForStructItem(	const StructItem&	item,
										const bool&			includeOffsetLayoutQualifier,
										const bool&			targetsInputBuffer) const
	{
		std::string result;

		for (const auto& currentItem : item.items)
		{
			const bool		needsMatrixOrderQualifier	= (currentItem.matrixOrder == MatrixMajorOrder::ROW_MAJOR);
			const auto		variableTypeGLSL			= getVariableTypeGLSLType	(currentItem.type);
			deUint32		nLayoutQualifiersUsed		= 0;
			const deUint32	nLayoutQualifierUses		=	((includeOffsetLayoutQualifier) ? 1 : 0) +
															((needsMatrixOrderQualifier)	? 1 : 0);
			const bool		usesLayoutQualifiers		= (nLayoutQualifierUses > 0);

			if (usesLayoutQualifiers)
			{
				result += "layout(";
			}

			if (includeOffsetLayoutQualifier)
			{
				result += "offset = " + de::toString((targetsInputBuffer)	? currentItem.inputBufferProps.bufferOffset
																			: currentItem.resultBufferProps.bufferOffset);

				if ( (++nLayoutQualifiersUsed) != nLayoutQualifierUses)
				{
					result += ", ";
				}
			}

			if (needsMatrixOrderQualifier)
			{
				result += ((currentItem.matrixOrder == MatrixMajorOrder::COLUMN_MAJOR)	? "column_major"
																						: "row_major");

				if ( (++nLayoutQualifiersUsed) != nLayoutQualifierUses)
				{
					result += ", ";
				}
			}

			if (usesLayoutQualifiers)
			{
				result += ") ";
			}

			result += variableTypeGLSL + std::string(" ") + currentItem.name;

			if (currentItem.arraySize != 1)
			{
				result += "[" + de::toString(currentItem.arraySize) + "]";
			}

			result += ";\n";
		}

		return result;
	}

	tcu::UVec2  getMatrixSize(const VariableType& type) const
	{
		auto result = tcu::UVec2();

		switch (type)
		{
			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			{
				result = tcu::UVec2(2, 2);

				break;
			}

			case VariableType::DMAT2X3:
			case VariableType::MAT2X3:
			{
				result = tcu::UVec2(2, 3);

				break;
			}

			case VariableType::DMAT2X4:
			case VariableType::MAT2X4:
			{
				result = tcu::UVec2(2, 4);

				break;
			}

			case VariableType::DMAT3:
			case VariableType::DMAT3X3:
			case VariableType::MAT3:
			case VariableType::MAT3X3:
			{
				result = tcu::UVec2(3, 3);

				break;
			}

			case VariableType::DMAT3X2:
			case VariableType::MAT3X2:
			{
				result = tcu::UVec2(3, 2);

				break;
			}

			case VariableType::DMAT3X4:
			case VariableType::MAT3X4:
			{
				result = tcu::UVec2(3, 4);

				break;
			}

			case VariableType::DMAT4:
			case VariableType::DMAT4X4:
			case VariableType::MAT4:
			case VariableType::MAT4X4:
			{
				result = tcu::UVec2(4, 4);

				break;
			}

			case VariableType::DMAT4X2:
			case VariableType::MAT4X2:
			{
				result = tcu::UVec2(4, 2);

				break;
			}

			case VariableType::DMAT4X3:
			case VariableType::MAT4X3:
			{
				result = tcu::UVec2(4, 3);

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

	deUint32	getNComponents(const VariableType& type) const
	{
		deUint32 result = 0;

		switch (type)
		{
			case VariableType::DOUBLE:
			case VariableType::FLOAT:
			case VariableType::INT8:
			case VariableType::INT16:
			case VariableType::INT64:
			case VariableType::INT:
			case VariableType::UINT:
			case VariableType::UINT8:
			case VariableType::UINT16:
			case VariableType::UINT64:
			{
				result = 1;

				break;
			}

			case VariableType::DVEC2:
			case VariableType::I8VEC2:
			case VariableType::I16VEC2:
			case VariableType::I64VEC2:
			case VariableType::IVEC2:
			case VariableType::U8VEC2:
			case VariableType::U16VEC2:
			case VariableType::U64VEC2:
			case VariableType::UVEC2:
			case VariableType::VEC2:
			{
				result = 2;

				break;
			}

			case VariableType::DVEC3:
			case VariableType::I8VEC3:
			case VariableType::I16VEC3:
			case VariableType::I64VEC3:
			case VariableType::IVEC3:
			case VariableType::U8VEC3:
			case VariableType::U16VEC3:
			case VariableType::U64VEC3:
			case VariableType::UVEC3:
			case VariableType::VEC3:
			{
				result = 3;

				break;
			}

			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::DVEC4:
			case VariableType::I8VEC4:
			case VariableType::I16VEC4:
			case VariableType::I64VEC4:
			case VariableType::IVEC4:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			case VariableType::U8VEC4:
			case VariableType::U16VEC4:
			case VariableType::U64VEC4:
			case VariableType::UVEC4:
			case VariableType::VEC4:
			{
				result = 4;

				break;
			}

			case VariableType::DMAT2X3:
			case VariableType::DMAT3X2:
			case VariableType::MAT2X3:
			case VariableType::MAT3X2:
			{
				result = 6;

				break;
			}

			case VariableType::DMAT2X4:
			case VariableType::DMAT4X2:
			case VariableType::MAT2X4:
			case VariableType::MAT4X2:
			{
				result = 8;

				break;
			}

			case VariableType::DMAT3:
			case VariableType::DMAT3X3:
			case VariableType::MAT3:
			case VariableType::MAT3X3:
			{
				result = 9;

				break;
			}

			case VariableType::DMAT3X4:
			case VariableType::DMAT4X3:
			case VariableType::MAT3X4:
			case VariableType::MAT4X3:
			{
				result = 12;

				break;
			}

			case VariableType::DMAT4:
			case VariableType::DMAT4X4:
			case VariableType::MAT4:
			case VariableType::MAT4X4:
			{
				result = 16;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	deUint32 getNMatrixColumns(const VariableType& type) const
	{
		deUint32 result = 0;

		switch (type)
		{
			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::DMAT2X3:
			case VariableType::DMAT2X4:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			case VariableType::MAT2X3:
			case VariableType::MAT2X4:
			{
				result = 2;

				break;
			}

			case VariableType::DMAT3:
			case VariableType::DMAT3X2:
			case VariableType::DMAT3X3:
			case VariableType::DMAT3X4:
			case VariableType::MAT3:
			case VariableType::MAT3X2:
			case VariableType::MAT3X4:
			case VariableType::MAT3X3:
			{
				result = 3;

				break;
			}

			case VariableType::DMAT4X2:
			case VariableType::MAT4X2:
			case VariableType::DMAT4X3:
			case VariableType::MAT4X3:
			case VariableType::DMAT4X4:
			case VariableType::DMAT4:
			case VariableType::MAT4X4:
			case VariableType::MAT4:
			{
				result = 4;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	deUint32 getNMatrixRows(const VariableType& type) const
	{
		deUint32 result = 0;

		switch (type)
		{
			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::DMAT3X2:
			case VariableType::DMAT4X2:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			case VariableType::MAT3X2:
			case VariableType::MAT4X2:
			{
				result = 2;

				break;
			}

			case VariableType::DMAT2X3:
			case VariableType::DMAT3:
			case VariableType::DMAT3X3:
			case VariableType::DMAT4X3:
			case VariableType::MAT2X3:
			case VariableType::MAT3:
			case VariableType::MAT3X3:
			case VariableType::MAT4X3:
			{
				result = 3;

				break;
			}

			case VariableType::DMAT2X4:
			case VariableType::DMAT3X4:
			case VariableType::DMAT4:
			case VariableType::DMAT4X4:
			case VariableType::MAT2X4:
			case VariableType::MAT3X4:
			case VariableType::MAT4:
			case VariableType::MAT4X4:
			{
				result = 4;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	const char* getVariableTypeGLSLType(const VariableType& type) const
	{
		const char* resultPtr = "!?";

		switch (type)
		{
			case VariableType::DOUBLE:	resultPtr = "double";	break;
			case VariableType::DMAT2:	resultPtr = "dmat2";	break;
			case VariableType::DMAT2X2:	resultPtr = "dmat2x2";	break;
			case VariableType::DMAT2X3:	resultPtr = "dmat2x3";	break;
			case VariableType::DMAT2X4:	resultPtr = "dmat2x4";	break;
			case VariableType::DMAT3:	resultPtr = "dmat3";	break;
			case VariableType::DMAT3X2:	resultPtr = "dmat3x2";	break;
			case VariableType::DMAT3X3:	resultPtr = "dmat3x3";	break;
			case VariableType::DMAT3X4:	resultPtr = "dmat3x4";	break;
			case VariableType::DMAT4:	resultPtr = "dmat4";	break;
			case VariableType::DMAT4X2:	resultPtr = "dmat4x2";	break;
			case VariableType::DMAT4X3:	resultPtr = "dmat4x3";	break;
			case VariableType::DMAT4X4:	resultPtr = "dmat4x4";	break;
			case VariableType::DVEC2:	resultPtr = "dvec2";	break;
			case VariableType::DVEC3:	resultPtr = "dvec3";	break;
			case VariableType::DVEC4:	resultPtr = "dvec4";	break;
			case VariableType::FLOAT:	resultPtr = "float";	break;
			case VariableType::INT16:	resultPtr = "int16_t";	break;
			case VariableType::INT64:	resultPtr = "int64_t";	break;
			case VariableType::INT8:	resultPtr = "int8_t";	break;
			case VariableType::INT:		resultPtr = "int";		break;
			case VariableType::I16VEC2:	resultPtr = "i16vec2";	break;
			case VariableType::I16VEC3:	resultPtr = "i16vec3";	break;
			case VariableType::I16VEC4:	resultPtr = "i16vec4";	break;
			case VariableType::I64VEC2:	resultPtr = "i64vec2";	break;
			case VariableType::I64VEC3:	resultPtr = "i64vec3";	break;
			case VariableType::I64VEC4:	resultPtr = "i64vec4";	break;
			case VariableType::I8VEC2:	resultPtr = "i8vec2";	break;
			case VariableType::I8VEC3:	resultPtr = "i8vec3";	break;
			case VariableType::I8VEC4:	resultPtr = "i8vec4";	break;
			case VariableType::IVEC2:	resultPtr = "ivec2";	break;
			case VariableType::IVEC3:	resultPtr = "ivec3";	break;
			case VariableType::IVEC4:	resultPtr = "ivec4";	break;
			case VariableType::MAT2:	resultPtr = "mat2";		break;
			case VariableType::MAT2X2:	resultPtr = "mat2x2";	break;
			case VariableType::MAT2X3:	resultPtr = "mat2x3";	break;
			case VariableType::MAT2X4:	resultPtr = "mat2x4";	break;
			case VariableType::MAT3:	resultPtr = "mat3";		break;
			case VariableType::MAT3X2:	resultPtr = "mat3x2";	break;
			case VariableType::MAT3X3:	resultPtr = "mat3x3";	break;
			case VariableType::MAT3X4:	resultPtr = "mat3x4";	break;
			case VariableType::MAT4:	resultPtr = "mat4";		break;
			case VariableType::MAT4X2:	resultPtr = "mat4x2";	break;
			case VariableType::MAT4X3:	resultPtr = "mat4x3";	break;
			case VariableType::MAT4X4:	resultPtr = "mat4x4";	break;
			case VariableType::UINT16:	resultPtr = "uint16_t";	break;
			case VariableType::UINT64:	resultPtr = "uint64_t";	break;
			case VariableType::UINT8:	resultPtr = "uint8_t";	break;
			case VariableType::UINT:	resultPtr = "uint";		break;
			case VariableType::U16VEC2:	resultPtr = "u16vec2";	break;
			case VariableType::U16VEC3:	resultPtr = "u16vec3";	break;
			case VariableType::U16VEC4:	resultPtr = "u16vec4";	break;
			case VariableType::U64VEC2:	resultPtr = "u64vec2";	break;
			case VariableType::U64VEC3:	resultPtr = "u64vec3";	break;
			case VariableType::U64VEC4:	resultPtr = "u64vec4";	break;
			case VariableType::U8VEC2:	resultPtr = "u8vec2";	break;
			case VariableType::U8VEC3:	resultPtr = "u8vec3";	break;
			case VariableType::U8VEC4:	resultPtr = "u8vec4";	break;
			case VariableType::UVEC2:	resultPtr = "uvec2";	break;
			case VariableType::UVEC3:	resultPtr = "uvec3";	break;
			case VariableType::UVEC4:	resultPtr = "uvec4";	break;
			case VariableType::VEC2:	resultPtr = "vec2";		break;
			case VariableType::VEC3:	resultPtr = "vec3";		break;
			case VariableType::VEC4:	resultPtr = "vec4";		break;

			default:
			{
				DE_ASSERT(false);
			}
		}

		return resultPtr;
	}

	void initTestItems()
	{
		de::Random		randomNumberGenerator(13567);
		const deUint32	testArraySizes[] =
		{
			3,
			7,
			5
		};

		const ShaderGroups shaderGroups[] =
		{
			ShaderGroups::HIT_GROUP,
			ShaderGroups::MISS_GROUP,
		};

		const auto nTestArraySizes	= sizeof(testArraySizes) / sizeof(testArraySizes[0]);

		for (const auto& currentVariableType : m_varTypesToTest)
		{
			const auto	currentArraySize = testArraySizes[static_cast<deUint32>(m_testItems.items.size() ) % nTestArraySizes];
			Item		newItem;

			newItem.arraySize	= currentArraySize;
			newItem.name		= "var" + de::toString(m_testItems.items.size() );
			newItem.type		= currentVariableType;

			// TODO: glslang issue.
			// newItem.matrixOrder = static_cast<MatrixMajorOrder>(static_cast<deUint32>(m_testItems.items.size() ) % static_cast<deUint32>(MatrixMajorOrder::UNKNOWN) );

			newItem.matrixOrder = MatrixMajorOrder::COLUMN_MAJOR;

			m_testItems.items.push_back(newItem);
		}

		// Determine start offsets for matrix elements.
		//
		// Note: result buffer aways uses std430 layout.
		setSTD430MatrixElementOffsets	(m_testItems, false /* updateInputBufferProps */);
		setSTD430ArrayStrides			(m_testItems, false /* updateInputBufferProps */);
		setSTD430BufferOffsets			(m_testItems, false /* updateInputBufferProps */);

		switch (m_testType)
		{
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6:
			{
				setExplicitScalarOffsetMatrixElementOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6:
			{
				setExplicitSTD430OffsetMatrixElementOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_SCALAR_1:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_2:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_3:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_4:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_5:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_6:
			{
				setScalarMatrixElementOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_STD430_1:
			case TestType::SHADER_RECORD_BLOCK_STD430_2:
			case TestType::SHADER_RECORD_BLOCK_STD430_3:
			case TestType::SHADER_RECORD_BLOCK_STD430_4:
			case TestType::SHADER_RECORD_BLOCK_STD430_5:
			case TestType::SHADER_RECORD_BLOCK_STD430_6:
			{
				setSTD430MatrixElementOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		// Configure array strides for the variables.
		switch (m_testType)
		{
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6:
			{
				setExplicitScalarOffsetArrayStrides(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6:
			{
				setExplicitSTD430OffsetArrayStrides(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_SCALAR_1:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_2:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_3:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_4:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_5:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_6:
			{
				setScalarArrayStrides(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_STD430_1:
			case TestType::SHADER_RECORD_BLOCK_STD430_2:
			case TestType::SHADER_RECORD_BLOCK_STD430_3:
			case TestType::SHADER_RECORD_BLOCK_STD430_4:
			case TestType::SHADER_RECORD_BLOCK_STD430_5:
			case TestType::SHADER_RECORD_BLOCK_STD430_6:
			{
				setSTD430ArrayStrides(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		// Configure buffer offsets for the variables.
		switch (m_testType)
		{
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6:
			{
				setExplicitScalarOffsetBufferOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5:
			case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6:
			{
				setExplicitSTD430OffsetBufferOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_SCALAR_1:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_2:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_3:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_4:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_5:
			case TestType::SHADER_RECORD_BLOCK_SCALAR_6:
			{
				setScalarBufferOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			case TestType::SHADER_RECORD_BLOCK_STD430_1:
			case TestType::SHADER_RECORD_BLOCK_STD430_2:
			case TestType::SHADER_RECORD_BLOCK_STD430_3:
			case TestType::SHADER_RECORD_BLOCK_STD430_4:
			case TestType::SHADER_RECORD_BLOCK_STD430_5:
			case TestType::SHADER_RECORD_BLOCK_STD430_6:
			{
				setSTD430BufferOffsets(m_testItems, true /* updateInputBufferProps */);

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		// Bake data to be used in the tested buffer.
		for (auto& currentTestItem : m_testItems.items)
		{
			const auto baseType				= getBaseType			(currentTestItem.type);
			const auto componentSizeBytes	= getComponentSizeBytes	(baseType);
			const bool isMatrixType			= isMatrix				(currentTestItem.type);
			const auto nComponents			= getNComponents		(currentTestItem.type);
			const auto nBytesNeeded			= currentTestItem.arraySize * currentTestItem.inputBufferProps.arrayStride;

			for (const auto& currentShaderGroup : shaderGroups)
			{
				auto& currentDataVec = currentTestItem.shaderGroupToRecordDataMap[currentShaderGroup];

				currentDataVec.resize(nBytesNeeded);

				for (deUint32 nArrayItem = 0; nArrayItem < currentTestItem.arraySize; ++nArrayItem)
				{
					deUint8* currentItemDataPtr = currentDataVec.data() + nArrayItem * currentTestItem.inputBufferProps.arrayStride;

					for (deUint32 nComponent = 0; nComponent < nComponents; ++nComponent)
					{
						switch (baseType)
						{
							case BaseType::F32:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(float) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<float*>(currentItemDataPtr) = randomNumberGenerator.getFloat();

								break;
							}

							case BaseType::F64:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(double) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<double*>(currentItemDataPtr) = randomNumberGenerator.getDouble();

								break;
							}

							case BaseType::I8:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deInt8) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deInt8*>(currentItemDataPtr) = static_cast<deInt8>(randomNumberGenerator.getInt(-128, 127) );

								break;
							}

							case BaseType::I16:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deInt16) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deInt16*>(currentItemDataPtr) = static_cast<deInt16>(randomNumberGenerator.getInt(-32768, 32767) );

								break;
							}

							case BaseType::I32:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deInt32) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deInt32*>(currentItemDataPtr) = randomNumberGenerator.getInt(static_cast<int>(-2147483648LL), static_cast<int>(2147483647LL) );

								break;
							}

							case BaseType::I64:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deInt64) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deInt64*>(currentItemDataPtr) = randomNumberGenerator.getInt64();

								break;
							}

							case BaseType::U8:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deUint8) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deUint8*>(currentItemDataPtr) = randomNumberGenerator.getUint8();

								break;
							}

							case BaseType::U16:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deUint16) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deUint16*>(currentItemDataPtr) = randomNumberGenerator.getUint16();

								break;
							}

							case BaseType::U32:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deUint32) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deUint32*>(currentItemDataPtr) = randomNumberGenerator.getUint32();

								break;
							}

							case BaseType::U64:
							{
								DE_ASSERT(currentItemDataPtr + sizeof(deUint64) <= currentDataVec.data() + currentDataVec.size() );

								*reinterpret_cast<deUint64*>(currentItemDataPtr) = randomNumberGenerator.getUint64();

								break;
							}

							default:
							{
								DE_ASSERT(false);
							}
						}

						if (isMatrixType)
						{
							if (nComponent != (nComponents - 1) )
							{
								const auto delta = currentTestItem.inputBufferProps.matrixElementStartOffsets.at(nComponent + 1) - currentTestItem.inputBufferProps.matrixElementStartOffsets.at(nComponent + 0);

								DE_ASSERT(delta >= componentSizeBytes);

								currentItemDataPtr += delta;
							}
						}
						else
						{
							currentItemDataPtr += componentSizeBytes;
						}
					}
				}
			}
		}

		// Merge individual member data into coalesced buffers.
		for (const auto& currentShaderGroup : shaderGroups)
		{
			auto& resultVec = m_shaderGroupToRecordDataMap[currentShaderGroup];

			{
				const auto& lastItem = m_testItems.items.back();

				resultVec.resize(lastItem.inputBufferProps.bufferOffset + lastItem.shaderGroupToRecordDataMap.at(currentShaderGroup).size() );
			}

			for (const auto& currentVariable : m_testItems.items)
			{
				const auto& currentVariableDataVec = currentVariable.shaderGroupToRecordDataMap.at(currentShaderGroup);

				DE_ASSERT(resultVec.size() >= currentVariable.inputBufferProps.bufferOffset + currentVariableDataVec.size());

				memcpy(	resultVec.data() + currentVariable.inputBufferProps.bufferOffset,
						currentVariableDataVec.data(),
						currentVariableDataVec.size() );
			}
		}
	}

	bool	isMatrix(const VariableType& type) const
	{
		bool result = false;

		switch (type)
		{
			case VariableType::DMAT2:
			case VariableType::DMAT2X2:
			case VariableType::DMAT2X3:
			case VariableType::DMAT2X4:
			case VariableType::DMAT3:
			case VariableType::DMAT3X2:
			case VariableType::DMAT3X3:
			case VariableType::DMAT3X4:
			case VariableType::DMAT4:
			case VariableType::DMAT4X2:
			case VariableType::DMAT4X3:
			case VariableType::DMAT4X4:
			case VariableType::MAT2:
			case VariableType::MAT2X2:
			case VariableType::MAT2X3:
			case VariableType::MAT2X4:
			case VariableType::MAT3:
			case VariableType::MAT3X2:
			case VariableType::MAT3X3:
			case VariableType::MAT3X4:
			case VariableType::MAT4:
			case VariableType::MAT4X2:
			case VariableType::MAT4X3:
			case VariableType::MAT4X4:
			{
				result = true;

				break;
			}

			case VariableType::DOUBLE:
			case VariableType::DVEC2:
			case VariableType::DVEC3:
			case VariableType::DVEC4:
			case VariableType::FLOAT:
			case VariableType::INT8:
			case VariableType::INT64:
			case VariableType::INT16:
			case VariableType::INT:
			case VariableType::I16VEC2:
			case VariableType::I16VEC3:
			case VariableType::I16VEC4:
			case VariableType::I64VEC2:
			case VariableType::I64VEC3:
			case VariableType::I64VEC4:
			case VariableType::I8VEC2:
			case VariableType::I8VEC3:
			case VariableType::I8VEC4:
			case VariableType::IVEC2:
			case VariableType::IVEC3:
			case VariableType::IVEC4:
			case VariableType::UINT8:
			case VariableType::UINT64:
			case VariableType::UINT16:
			case VariableType::UINT:
			case VariableType::U16VEC2:
			case VariableType::U16VEC3:
			case VariableType::U16VEC4:
			case VariableType::U64VEC2:
			case VariableType::U64VEC3:
			case VariableType::U64VEC4:
			case VariableType::U8VEC2:
			case VariableType::U8VEC3:
			case VariableType::U8VEC4:
			case VariableType::UVEC2:
			case VariableType::UVEC3:
			case VariableType::UVEC4:
			case VariableType::VEC2:
			case VariableType::VEC3:
			case VariableType::VEC4:
			{
				result = false;

				break;
			}

			default:
			{
				DE_ASSERT(false);
			}
		}

		return result;
	}

	void setExplicitScalarOffsetArrayStrides(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		return setScalarArrayStrides(inputStruct, updateInputBufferProps);
	}

	void setExplicitScalarOffsetBufferOffsets(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		deUint32 nBytesConsumed = 0;

		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);

			bufferProps.bufferOffset = de::roundUp(nBytesConsumed, componentSizeBytes * 2);

			if (isMatrixVariable)
			{
				nBytesConsumed = bufferProps.bufferOffset +  currentItem.arraySize * bufferProps.arrayStride;
			}
			else
			{
				nBytesConsumed = bufferProps.bufferOffset + currentItem.arraySize * componentSizeBytes * nComponents;
			}
		}
	}

	void setExplicitScalarOffsetElementOffsets(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		return setScalarMatrixElementOffsets(inputStruct, updateInputBufferProps);
	}

	void setExplicitScalarOffsetMatrixElementOffsets(	StructItem& inputStruct,
														const bool& updateInputBufferProps)
	{
		return setScalarMatrixElementOffsets(inputStruct, updateInputBufferProps);
	}

	void setExplicitSTD430OffsetArrayStrides(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		return setSTD430ArrayStrides(inputStruct, updateInputBufferProps);
	}

	void setExplicitSTD430OffsetBufferOffsets(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		deUint32 nBytesConsumed = 0;

		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);
			deUint32	requiredAlignment	= 0;

			deUint32 nMatrixRows	= 0;

			if (isMatrixVariable)
			{
				nMatrixRows	= getNMatrixRows(currentItem.type);

				if (nMatrixRows == 3)
				{
					nMatrixRows = 4;
				}

				requiredAlignment = nMatrixRows * componentSizeBytes;
			}
			else
			if (nComponents == 1)
			{
				DE_ASSERT(	(baseType == BaseType::F32) ||
							(baseType == BaseType::F64) ||
							(baseType == BaseType::I16) ||
							(baseType == BaseType::I32) ||
							(baseType == BaseType::I64) ||
							(baseType == BaseType::I8)  ||
							(baseType == BaseType::U16) ||
							(baseType == BaseType::U32) ||
							(baseType == BaseType::U64) ||
							(baseType == BaseType::U8) );

				requiredAlignment = componentSizeBytes;
			}
			else
			if (nComponents == 2)
			{
				requiredAlignment = 2 * componentSizeBytes;
			}
			else
			{
				requiredAlignment = 4 * componentSizeBytes;
			}

			bufferProps.bufferOffset = de::roundUp(nBytesConsumed, requiredAlignment * 2);

			if (isMatrixVariable)
			{
				nBytesConsumed = bufferProps.bufferOffset +  currentItem.arraySize * bufferProps.arrayStride;
			}
			else
			{
				nBytesConsumed = bufferProps.bufferOffset + currentItem.arraySize * componentSizeBytes * ((nComponents == 3) ? 4 : nComponents);
			}
		}
	}

	void setExplicitSTD430OffsetElementOffsets(	StructItem& inputStruct,
												const bool& updateInputBufferProps)
	{
		return setSTD430MatrixElementOffsets(inputStruct, updateInputBufferProps);
	}

	void setExplicitSTD430OffsetMatrixElementOffsets(	StructItem& inputStruct,
														const bool& updateInputBufferProps)
	{
		return setSTD430MatrixElementOffsets(inputStruct, updateInputBufferProps);
	}

	void setSTD430ArrayStrides(	StructItem& inputStruct,
								const bool& updateInputBufferProps)
	{
		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);
			deUint32	requiredStride		= 0;

			if (isMatrixVariable)
			{
				auto	nMatrixColumns	= getNMatrixColumns	(currentItem.type);
				auto	nMatrixRows		= getNMatrixRows	(currentItem.type);

				if (nMatrixRows == 3)
				{
					nMatrixRows = 4;
				}

				requiredStride = nMatrixRows * nMatrixColumns * componentSizeBytes;
			}
			else
			{
				requiredStride = componentSizeBytes * ((nComponents == 3)	? 4
																			: nComponents);
			}

			bufferProps.arrayStride = requiredStride;
		}
	}

	void setSTD430BufferOffsets(StructItem& inputStruct,
								const bool& updateInputBufferProps)
	{
		deUint32 nBytesConsumed = 0;

		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);
			deUint32	requiredAlignment	= 0;

			deUint32 nMatrixRows	= 0;

			if (isMatrixVariable)
			{
				nMatrixRows	= getNMatrixRows(currentItem.type);

				if (nMatrixRows == 3)
				{
					nMatrixRows = 4;
				}

				requiredAlignment = nMatrixRows * componentSizeBytes;
			}
			else
			if (nComponents == 1)
			{
				DE_ASSERT(	(baseType == BaseType::F32) ||
							(baseType == BaseType::F64) ||
							(baseType == BaseType::I16) ||
							(baseType == BaseType::I32) ||
							(baseType == BaseType::I64) ||
							(baseType == BaseType::I8)  ||
							(baseType == BaseType::U16) ||
							(baseType == BaseType::U32) ||
							(baseType == BaseType::U64) ||
							(baseType == BaseType::U8) );

				requiredAlignment = componentSizeBytes;
			}
			else
			if (nComponents == 2)
			{
				requiredAlignment = 2 * componentSizeBytes;
			}
			else
			{
				requiredAlignment = 4 * componentSizeBytes;
			}

			bufferProps.bufferOffset = de::roundUp(nBytesConsumed, requiredAlignment);

			if (isMatrixVariable)
			{
				nBytesConsumed = bufferProps.bufferOffset +  currentItem.arraySize * bufferProps.arrayStride;
			}
			else
			{
				nBytesConsumed = bufferProps.bufferOffset + currentItem.arraySize * componentSizeBytes * ((nComponents == 3) ? 4 : nComponents);
			}
		}
	}

	void setScalarArrayStrides(	StructItem& inputStruct,
								const bool& updateInputBufferProps)
	{
		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);

			if (isMatrixVariable)
			{
				auto	nMatrixColumns	= getNMatrixColumns	(currentItem.type);
				auto	nMatrixRows		= getNMatrixRows	(currentItem.type);

				bufferProps.arrayStride = nMatrixRows * nMatrixColumns * componentSizeBytes;
			}
			else
			{
				bufferProps.arrayStride = componentSizeBytes * nComponents;
			}
		}
	}

	void setScalarBufferOffsets(StructItem& inputStruct,
								const bool& updateInputBufferProps)
	{
		deUint32 nBytesConsumed = 0;

		for (auto& currentItem : inputStruct.items)
		{
			const auto	baseType			= getBaseType			(currentItem.type);
			auto&		bufferProps			= (updateInputBufferProps) ? currentItem.inputBufferProps : currentItem.resultBufferProps;
			const auto	componentSizeBytes	= getComponentSizeBytes	(baseType);
			const auto	isMatrixVariable	= isMatrix				(currentItem.type);
			const auto	nComponents			= getNComponents		(currentItem.type);

			bufferProps.bufferOffset = de::roundUp(nBytesConsumed, componentSizeBytes);

			if (isMatrixVariable)
			{
				nBytesConsumed = bufferProps.bufferOffset +  currentItem.arraySize * bufferProps.arrayStride;
			}
			else
			{
				nBytesConsumed = bufferProps.bufferOffset + currentItem.arraySize * componentSizeBytes * nComponents;
			}
		}
	}

	void setScalarMatrixElementOffsets(	StructItem& inputStruct,
										const bool& updateInputBufferProps)
	{
		for (auto& currentVariable : inputStruct.items)
		{
			if (isMatrix(currentVariable.type))
			{
				auto&		bufferProps					= (updateInputBufferProps) ? currentVariable.inputBufferProps : currentVariable.resultBufferProps;
				const auto	componentSizeBytes			= getComponentSizeBytes(getBaseType(currentVariable.type) );
				deUint32	currentMatrixElementOffset	= 0;
				const auto	nMatrixColumns				= getNMatrixColumns	(currentVariable.type);
				const auto	nMatrixRows					= getNMatrixRows	(currentVariable.type);

				for (deUint32 nMatrixColumn = 0; nMatrixColumn < nMatrixColumns; ++nMatrixColumn)
				{
					currentMatrixElementOffset = de::roundUp(	nMatrixRows * componentSizeBytes * nMatrixColumn,
																componentSizeBytes);

					for (deUint32 nMatrixRow = 0; nMatrixRow < nMatrixRows; ++nMatrixRow)
					{
						bufferProps.matrixElementStartOffsets.push_back(currentMatrixElementOffset);

						currentMatrixElementOffset += componentSizeBytes;
					}
				}
			}
		}
	}

	void setSTD430MatrixElementOffsets(	StructItem& inputStruct,
										const bool& updateInputBufferProps)
	{
		for (auto& currentVariable : inputStruct.items)
		{
			if (isMatrix(currentVariable.type))
			{
				auto&		bufferProps					= (updateInputBufferProps) ? currentVariable.inputBufferProps : currentVariable.resultBufferProps;
				const auto	componentSizeBytes			= getComponentSizeBytes(getBaseType(currentVariable.type) );
				deUint32	currentMatrixElementOffset	= 0;
				auto		nMatrixColumns				= getNMatrixColumns	(currentVariable.type);
				auto		nMatrixRows					= getNMatrixRows	(currentVariable.type);

				if (currentVariable.matrixOrder == MatrixMajorOrder::COLUMN_MAJOR)
				{
					for (deUint32 nMatrixColumn = 0; nMatrixColumn < nMatrixColumns; ++nMatrixColumn)
					{
						currentMatrixElementOffset = de::roundUp(	static_cast<deUint32>(nMatrixRows * componentSizeBytes * nMatrixColumn),
																	static_cast<deUint32>(((nMatrixRows == 3) ? 4 : nMatrixRows) * componentSizeBytes));

						for (deUint32 nMatrixRow = 0; nMatrixRow < nMatrixRows; ++nMatrixRow)
						{
							bufferProps.matrixElementStartOffsets.push_back(currentMatrixElementOffset);

							currentMatrixElementOffset += componentSizeBytes;
						}
					}
				}
				else
				{
					// TODO
					DE_ASSERT(false);
				}
			}
		}
	}

	// Private variables
	const tcu::UVec3				m_gridSizeXYZ;
	const TestType					m_testType;
	const std::vector<VariableType> m_varTypesToTest;

	deUint32	m_resultBufferSize;
	deUint32	m_shaderRecordSize;
	StructItem	m_testItems;

	std::map<ShaderGroups, std::vector<deUint8> >	m_shaderGroupToRecordDataMap;
	std::map<VkShaderStageFlagBits, deUint32>		m_shaderStageToResultBufferOffset;
	std::unique_ptr<GridASProvider>					m_asProviderPtr;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};

class RecursiveTracesTest : public TestBase
{
public:
	RecursiveTracesTest(	const GeometryType&					geometryType,
							const AccelerationStructureLayout&	asStructureLayout,
							const deUint32&						depthToUse)
		:	m_asStructureLayout				(asStructureLayout),
			m_geometryType					(geometryType),
			m_depthToUse					(depthToUse),
			m_nRaysToTest					(512),
			m_maxResultBufferSizePermitted	(512 * 1024768)
	{
		const auto nItemsExpectedPerRay			= static_cast<deUint32>((1 << (m_depthToUse + 0)) - 1);
		const auto nItemsExpectedPerRayInclRgen	= static_cast<deUint32>((1 << (m_depthToUse + 1)) - 1);

		m_nResultItemsExpected		= nItemsExpectedPerRayInclRgen	* m_nRaysToTest;
		m_nCHitInvocationsExpected	= nItemsExpectedPerRay			* m_nRaysToTest;
		m_nMissInvocationsExpected	= nItemsExpectedPerRay			* m_nRaysToTest;

		{
			const deUint32 nPreambleBytes = sizeof(deUint32) * 3;
			const deUint32 resultItemSize = sizeof(deUint32) * 4;

			m_nMaxResultItemsPermitted = (m_maxResultBufferSizePermitted - nPreambleBytes) / resultItemSize;
		}
	}

	~RecursiveTracesTest()
	{
		/* Stub */
	}

	std::vector<std::string> getAHitShaderCollectionShaderNames() const final
	{
		return m_ahitShaderNameVec;
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return m_chitShaderNameVec;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		DE_ASSERT(m_nRaysToTest != 0);

		return tcu::UVec3(m_nRaysToTest, 1u, 1u);
	}

	std::vector<std::string> getIntersectionShaderCollectionShaderNames() const final
	{
		const auto nIntersectionShaders =	(	(m_geometryType == GeometryType::AABB)					||
												(m_geometryType == GeometryType::AABB_AND_TRIANGLES) )	?	m_depthToUse
																										: 0;

		return std::vector<std::string>(nIntersectionShaders,
										{"intersection0"});
	}

	deUint32 getMaxRecursionDepthUsed() const final
	{
		return m_depthToUse;
	}

	std::vector<std::string> getMissShaderCollectionShaderNames() const final
	{
		return m_missShaderNameVec;
	}

	deUint32 getResultBufferSize() const final
	{
		DE_ASSERT(m_depthToUse  <  30); //< due to how nItemsExpectedPerRay is stored.
		DE_ASSERT(m_nRaysToTest	!= 0);

		/* NOTE: A single item is generated by rgen shader stage which is invoked once per each initial ray.
		 *
		 *       Each ray at level N generates two result items.
		 *
		 *       Thus, for a single initial traced ray, we need sum(2^depth)=2^(depth+1)-1 items.
		 */
		const auto nItemsExpectedPerRay	= static_cast<deUint32>((1 << (m_depthToUse + 1)) - 1);
		const auto nResultItemsExpected	= de::min(nItemsExpectedPerRay * m_nRaysToTest, m_nMaxResultItemsPermitted);
		const auto resultItemSize		= static_cast<deUint32>(sizeof(deUint32) * 4 /* nOriginRay, stage, depth, parentResultItem */);

		return static_cast<deUint32>(sizeof(deUint32) * 3 /* nItemsRegistered, nCHitInvocations, nMissInvocations */) + nResultItemsExpected * resultItemSize;
	}

	VkSpecializationInfo* getSpecializationInfoPtr(const VkShaderStageFlagBits& shaderStage) final
	{
		VkSpecializationInfo* resultPtr = nullptr;

		if (shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR ||
			shaderStage == VK_SHADER_STAGE_MISS_BIT_KHR)
		{
			resultPtr = &m_specializationInfo;
		}

		return resultPtr;
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return {m_tlPtr.get() };
	}

	bool init(	vkt::Context&			/* context    */,
				RayTracingProperties*	/* rtPropsPtr */) final
	{
		m_specializationEntry.constantID	= 1;
		m_specializationEntry.offset		= 0;
		m_specializationEntry.size			= sizeof(deUint32);

		m_specializationInfo.dataSize		= sizeof(deUint32);
		m_specializationInfo.mapEntryCount	= 1;
		m_specializationInfo.pData			= &m_depthToUse;
		m_specializationInfo.pMapEntries	= &m_specializationEntry;

		return true;
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(	tcu::Vec3 (0,					0,	0),				/* gridStartXYZ          */
								tcu::Vec3 (1,					1,	1),				/* gridCellSizeXYZ       */
								tcu::UVec3(1,					1,	1),
								tcu::Vec3 (2,					0,	2),				/* gridInterCellDeltaXYZ */
								m_geometryType)
		);

		m_tlPtr  = asProviderPtr->createTLAS(	context,
												m_asStructureLayout,
												commandBuffer,
												0,			/* bottomLevelGeometryFlags */
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFeedbackPtr         */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const auto nLocationsPerPayload = 3; /* 3 scalar uints */

		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		std::vector<std::string> rayPayloadDefinitionVec  (m_depthToUse);
		std::vector<std::string> rayPayloadInDefinitionVec(m_depthToUse);

		for (deUint32 nLevel = 0; nLevel < m_depthToUse; ++nLevel)
		{
			rayPayloadDefinitionVec.at(nLevel) =
				"layout(location = " + de::toString(nLocationsPerPayload * nLevel) + ") rayPayloadEXT block\n"
				"{\n"
				"    uint currentDepth;\n"
				"    uint currentNOriginRay;\n"
				"    uint currentResultItem;\n"
				"};\n";

			rayPayloadInDefinitionVec.at(nLevel) =
				"layout(location = " + de::toString(nLocationsPerPayload * nLevel) + ") rayPayloadInEXT block\n"
				"{\n"
				"    uint parentDepth;\n"
				"    uint parentNOriginRay;\n"
				"    uint parentResultItem;\n"
				"};\n";
		}

		const std::string constantVariableDefinition =
			"layout(constant_id = 1) const uint MAX_RECURSIVE_DEPTH = " + de::toString(m_depthToUse) + ";\n";

		const char* resultBufferDefinition =
			"struct ResultData\n"
			"{\n"
			"    uint nOriginRay;\n"
			"    uint shaderStage;\n"
			"    uint depth;\n"
			"    uint callerResultItem;\n"
			"};\n"
			"\n"
			"layout(set = 0, binding = 0, std430) buffer result\n"
			"{\n"
			"    uint       nItemsStored;\n"
			"    uint       nCHitInvocations;\n"
			"    uint       nMissInvocations;\n"
			"    ResultData resultItems[];\n"
			"};\n";

		{
			m_ahitShaderNameVec.resize(m_depthToUse);

			for (deUint32 nLevel = 0; nLevel < m_depthToUse; ++nLevel)
			{
				std::stringstream css;

				css <<
					"#version 460 core\n"
					"\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"\n"
					+ de::toString					(resultBufferDefinition)
					+ rayPayloadInDefinitionVec.at	(nLevel)					+
					"\n"
					"void main()\n"
					"{\n"
					/* Stub - don't care */
					"}\n";

				m_ahitShaderNameVec.at(nLevel) = std::string("ahit") + de::toString(nLevel);

				programCollection.glslSources.add(m_ahitShaderNameVec.at(nLevel) ) << glu::AnyHitSource(css.str() ) << buildOptions;
			}
		}

		{
			m_chitShaderNameVec.resize(m_depthToUse);

			for (deUint32 nLevel = 0; nLevel < m_depthToUse; ++nLevel)
			{
				std::stringstream	css;
				const bool			shouldTraceRays = (nLevel != (m_depthToUse - 1) );

				css <<
					"#version 460 core\n"
					"\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"\n"
					"layout(set = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
					"\n"
					+ constantVariableDefinition
					+ de::toString(resultBufferDefinition)
					+ de::toString(rayPayloadInDefinitionVec.at(nLevel) );

				if (shouldTraceRays)
				{
					css << rayPayloadDefinitionVec.at(nLevel + 1);
				}

				css <<
					"\n"
					"void main()\n"
					"{\n"
					"    uint nItem = atomicAdd(nItemsStored, 1);\n"
					"\n"
					"    atomicAdd(nCHitInvocations, 1);\n"
					"\n"
					"    if (nItem < " + de::toString(m_nMaxResultItemsPermitted) + ")\n"
					"    {\n"
					"        resultItems[nItem].callerResultItem = parentResultItem;\n"
					"        resultItems[nItem].depth            = parentDepth;\n"
					"        resultItems[nItem].nOriginRay       = parentNOriginRay;\n"
					"        resultItems[nItem].shaderStage      = 1;\n"
					"    }\n"
					"\n";

				if (shouldTraceRays)
				{
					css <<
						"    if (parentDepth < MAX_RECURSIVE_DEPTH - 1)\n"
						"    {\n"
						"        currentDepth      = parentDepth + 1;\n"
						"        currentNOriginRay = parentNOriginRay;\n"
						"        currentResultItem = nItem;\n"
						"\n"
						"        vec3  cellStartXYZ  = vec3(0.0, 0.0, 0.0);\n"
						"        vec3  cellEndXYZ    = cellStartXYZ + vec3(1.0);\n"
						"        vec3  targetHit     = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
						"        vec3  targetMiss    = targetHit + vec3(0, 10, 0);\n"
						"        vec3  origin        = targetHit - vec3(1, 0,  0);\n"
						"        vec3  directionHit  = normalize(targetHit  - origin);\n"
						"        vec3  directionMiss = normalize(targetMiss - origin);\n"
						"        uint  rayFlags      = 0;\n"
						"        uint  cullMask      = 0xFF;\n"
						"        float tmin          = 0.001;\n"
						"        float tmax          = 5.0;\n"
						"\n"
						"        traceRayEXT(accelerationStructure, rayFlags, cullMask, " + de::toString(nLevel + 1) + ", 0, 0, origin, tmin, directionHit,  tmax, " + de::toString(nLocationsPerPayload * (nLevel + 1) ) + ");\n"
						"        traceRayEXT(accelerationStructure, rayFlags, cullMask, " + de::toString(nLevel + 1) + ", 0, 0, origin, tmin, directionMiss, tmax, " + de::toString(nLocationsPerPayload * (nLevel + 1) ) + ");\n"
						"    }\n"
						"\n";
				}

				css << "}\n";

				m_chitShaderNameVec.at(nLevel) = std::string("chit") + de::toString(nLevel);

				programCollection.glslSources.add(m_chitShaderNameVec.at(nLevel) ) << glu::ClosestHitSource(css.str() ) << buildOptions;
			}
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			// There is stack caching code that assumes it knows which shader groups are what, but that doesn't apply to
			// this test. The other hit group shaders don't hit this issue because they don't use the canonical name, so
			// de-canonicalize the name to work around that
			programCollection.glslSources.add("intersection0") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			m_missShaderNameVec.resize(m_depthToUse);

			for (deUint32 nLevel = 0; nLevel < m_depthToUse; ++nLevel)
			{
				std::stringstream	css;
				const bool			shouldTraceRays = (nLevel != (m_depthToUse - 1) );

				css <<
					"#version 460 core\n"
					"\n"
					"#extension GL_EXT_ray_tracing : require\n"
					"\n"
					"layout(set = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
					"\n"
					+	constantVariableDefinition
					+	de::toString(resultBufferDefinition)
					+	de::toString(rayPayloadInDefinitionVec.at(nLevel) );

				if (shouldTraceRays)
				{
					css << rayPayloadDefinitionVec.at(nLevel + 1);
				}

				css <<
					"\n"
					"void main()\n"
					"{\n"
					"    uint nItem = atomicAdd(nItemsStored, 1);\n"
					"\n"
					"    atomicAdd(nMissInvocations, 1);\n"
					"\n"
					"    if (nItem < " + de::toString(m_nMaxResultItemsPermitted) + ")\n"
					"    {\n"
					"        resultItems[nItem].depth            = parentDepth;\n"
					"        resultItems[nItem].nOriginRay       = parentNOriginRay;\n"
					"        resultItems[nItem].callerResultItem = parentResultItem;\n"
					"        resultItems[nItem].shaderStage      = 2;\n"
					"    }\n"
					"\n";

				if (shouldTraceRays)
				{
					css <<
						"    if (parentDepth < MAX_RECURSIVE_DEPTH - 1)\n"
						"    {\n"
						"        currentDepth      = parentDepth + 1;\n"
						"        currentNOriginRay = parentNOriginRay;\n"
						"        currentResultItem = nItem;\n"
						"\n"
						"        vec3  cellStartXYZ  = vec3(0.0, 0.0, 0.0);\n"
						"        vec3  cellEndXYZ    = cellStartXYZ + vec3(1.0);\n"
						"        vec3  targetHit     = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
						"        vec3  targetMiss    = targetHit + vec3(0, 10, 0);\n"
						"        vec3  origin        = targetHit - vec3(1, 0,  0);\n"
						"        vec3  directionHit  = normalize(targetHit  - origin);\n"
						"        vec3  directionMiss = normalize(targetMiss - origin);\n"
						"\n"
						"        uint  rayFlags      = 0;\n"
						"        uint  cullMask      = 0xFF;\n"
						"        float tmin          = 0.001;\n"
						"        float tmax          = 5.0;\n"
						"\n"
						"        traceRayEXT(accelerationStructure, rayFlags, cullMask, " + de::toString(nLevel + 1) + ", 0, 0, origin, tmin, directionHit,  tmax, " + de::toString(nLocationsPerPayload * (nLevel + 1) ) + ");\n"
						"        traceRayEXT(accelerationStructure, rayFlags, cullMask, " + de::toString(nLevel + 1) + ", 0, 0, origin, tmin, directionMiss, tmax, " + de::toString(nLocationsPerPayload * (nLevel + 1) ) + ");\n"
						"    }\n";
				}

				css << "}\n";

				m_missShaderNameVec.at(nLevel) = "miss" + de::toString(nLevel);

				programCollection.glslSources.add(m_missShaderNameVec.at(nLevel)) << glu::MissSource(css.str() ) << buildOptions;
			}
		}

		{
			const std::string rayPayloadDefinition = ((m_depthToUse == 0u) ? "" : rayPayloadDefinitionVec.at(0));

			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				+	de::toString(resultBufferDefinition)
				+	rayPayloadDefinition +
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 9.0;\n"
				"\n"
				"    uint  cullMask      = 0xFF;\n"
				"    vec3  cellStartXYZ  = vec3(0.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ    = cellStartXYZ + vec3(1.0);\n"
				"    vec3  targetHit     = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  targetMiss    = targetHit + vec3(0, 10, 0);\n"
				"    vec3  origin        = targetHit - vec3(1, 0,  0);\n"
				"    vec3  directionHit  = normalize(targetHit  - origin);\n"
				"    vec3  directionMiss = normalize(targetMiss - origin);\n"
				"\n"
				"    uint nItem = atomicAdd(nItemsStored, 1);\n"
				"\n"
				"    if (nItem < " + de::toString(m_nMaxResultItemsPermitted) + ")\n"
				"    {\n"
				"        resultItems[nItem].callerResultItem = 0xFFFFFFFF;\n"
				"        resultItems[nItem].depth            = 0;\n"
				"        resultItems[nItem].nOriginRay       = nInvocation;\n"
				"        resultItems[nItem].shaderStage      = 3;\n"
				"    }\n"
				"\n"
				+ ((m_depthToUse == 0u) ? "" :
					"    currentDepth      = 0;\n"
					"    currentNOriginRay = nInvocation;\n"
					"    currentResultItem = nItem;\n"
					"\n"
					"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, directionHit,  tmax, 0);\n"
					"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, directionMiss, tmax, 0);\n"
				) +
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr		= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result				= false;
		auto			nItemsStored		= *resultU32Ptr;
		const auto		nCHitInvocations	= *(resultU32Ptr + 1);
		const auto		nMissInvocations	= *(resultU32Ptr + 2);
		const bool		doFullCheck			= (m_nResultItemsExpected < m_nMaxResultItemsPermitted);

		struct ResultItem
		{
			deUint32				depth;
			deUint32				nOriginRay;
			deUint32				nParentNode;

			VkShaderStageFlagBits	stage;

			ResultItem* childCHitNodePtr;
			ResultItem* childMissNodePtr;

			ResultItem()
				:	depth				(0xFFFFFFFFu),
					nOriginRay			(0xFFFFFFFFu),
					nParentNode			(0xFFFFFFFFu),
					stage				(VK_SHADER_STAGE_ALL),
					childCHitNodePtr	(nullptr),
					childMissNodePtr	(nullptr)
			{
				/* Stub */
			}
		};

		std::map<deUint32, ResultItem*>										nItemToResultItemPtrMap;
		std::map<deUint32, std::vector<std::unique_ptr<ResultItem> > >		nRayToResultItemPtrVecMap;
		std::map<deUint32, std::map<deUint32, std::vector<ResultItem*> > >	nRayToNLevelToResultItemPtrVecMap;

		if (doFullCheck)
		{
			if (nItemsStored != m_nResultItemsExpected)
			{
				goto end;
			}
		}
		else
		{
			// Test shaders always use an atomic add to obtain a unique index, at which they should write the result item.
			// Hence, the value we read back from the result buffer's preamble does not actually indicate how many items
			// are available for reading, since a partial (!= full) check implies our result buffer only contains a fraction
			// of all expected items (since more items would simply not fit in).
			//
			// Make sure to use a correct value in subsequent checks.
			if (nItemsStored < m_nResultItemsExpected)
			{
				goto end;
			}

			nItemsStored = m_nMaxResultItemsPermitted;
		}

		if (nCHitInvocations != m_nCHitInvocationsExpected)
		{
			goto end;
		}

		if (nMissInvocations != m_nMissInvocationsExpected)
		{
			goto end;
		}

		/* Convert an array of result items, stored in undefined order, to a representation we can easily verify */
		for (deUint32 nItem = 0; nItem < nItemsStored; ++nItem)
		{
			const deUint32*				currentItemU32Ptr = resultU32Ptr + 3 /* nItemsRegistered, nCHitInvocations, nMissInvocations*/ + 4 /* items per result item */ * nItem;
			std::unique_ptr<ResultItem>	resultItemPtr;

			resultItemPtr.reset(new ResultItem() );

			resultItemPtr->depth		= *(currentItemU32Ptr + 2);
			resultItemPtr->nOriginRay	= *(currentItemU32Ptr + 0);
			resultItemPtr->nParentNode	= *(currentItemU32Ptr + 3);

			switch (*(currentItemU32Ptr + 1) )
			{
				case 1: resultItemPtr->stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; break;
				case 2: resultItemPtr->stage = VK_SHADER_STAGE_MISS_BIT_KHR;		break;
				case 3: resultItemPtr->stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;		break;

				default:
				{
					/* This should never happen */
					DE_ASSERT(false);

					goto end;
				}
			}

			if (resultItemPtr->depth >= m_depthToUse && m_depthToUse > 0u)
			{
				DE_ASSERT(resultItemPtr->depth < m_depthToUse);

				goto end;
			}

			if (resultItemPtr->nOriginRay >= m_nRaysToTest)
			{
				DE_ASSERT(resultItemPtr->nOriginRay < m_nRaysToTest);

				goto end;
			}

			nItemToResultItemPtrMap[nItem]	= resultItemPtr.get();

			nRayToNLevelToResultItemPtrVecMap	[resultItemPtr->nOriginRay][resultItemPtr->depth].push_back	(resultItemPtr.get	() );
			nRayToResultItemPtrVecMap			[resultItemPtr->nOriginRay].push_back						(std::move			(resultItemPtr) );
		}

		if (doFullCheck)
		{
			for (const auto& iterator1 : nRayToNLevelToResultItemPtrVecMap)
			{
				const auto& currentNLevelToResultItemPtrVecMap	= iterator1.second;
				deUint32	nRayGenShaderResultItemsFound		= 0;

				for (const auto& iterator2 : currentNLevelToResultItemPtrVecMap)
				{
					const auto& currentResultItemPtrVec = iterator2.second;

					for (const auto& currentResultItemPtr : currentResultItemPtrVec)
					{
						if (currentResultItemPtr->stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
						{
							if (currentResultItemPtr->nParentNode != 0xFFFFFFFF)
							{
								DE_ASSERT(currentResultItemPtr->nParentNode == 0xFFFFFFFF);

								goto end;
							}

							nRayGenShaderResultItemsFound++;
						}
						else
						if (currentResultItemPtr->stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
						{
							DE_ASSERT(currentResultItemPtr->nParentNode < nItemsStored);

							auto parentNodePtr = nItemToResultItemPtrMap.at(currentResultItemPtr->nParentNode);

							if (parentNodePtr->childCHitNodePtr != nullptr)
							{
								DE_ASSERT(parentNodePtr->childCHitNodePtr == nullptr);

								goto end;
							}

							parentNodePtr->childCHitNodePtr = currentResultItemPtr;
						}
						else
						{
							DE_ASSERT(currentResultItemPtr->stage		==	VK_SHADER_STAGE_MISS_BIT_KHR);
							DE_ASSERT(currentResultItemPtr->nParentNode <	nItemsStored);

							auto parentNodePtr = nItemToResultItemPtrMap.at(currentResultItemPtr->nParentNode);

							if (parentNodePtr->childMissNodePtr != nullptr)
							{
								DE_ASSERT(parentNodePtr->childMissNodePtr == nullptr);

								goto end;
							}

							parentNodePtr->childMissNodePtr = currentResultItemPtr;
						}
					}
				}

				if (nRayGenShaderResultItemsFound != 1)
				{
					DE_ASSERT(nRayGenShaderResultItemsFound == 1);

					goto end;
				}
			}
		}

		// 1. Verify all nodes that are not leaves have both child nodes attached, and that leaf nodes do not have any children assigned.
		if (doFullCheck)
		{
			for (const auto& iterator1 : nRayToNLevelToResultItemPtrVecMap)
			{
				const auto& currentNLevelToResultItemPtrVecMap = iterator1.second;

				for (const auto& iterator2 : currentNLevelToResultItemPtrVecMap)
				{
					const auto& currentNLevel			= iterator2.first;
					const auto& currentResultItemPtrVec	= iterator2.second;

					for (const auto& currentResultItemPtr : currentResultItemPtrVec)
					{
						if (	currentResultItemPtr->stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR ||
								currentNLevel				!= m_depthToUse - 1)
						{
							if (currentResultItemPtr->childCHitNodePtr == nullptr && m_depthToUse > 0u)
							{
								DE_ASSERT(currentResultItemPtr->childCHitNodePtr != nullptr);

								goto end;
							}

							if (currentResultItemPtr->childMissNodePtr == nullptr && m_depthToUse > 0u)
							{
								DE_ASSERT(currentResultItemPtr->childMissNodePtr != nullptr);

								goto end;
							}
						}
						else
						{
							if (currentResultItemPtr->childCHitNodePtr != nullptr)
							{
								DE_ASSERT(currentResultItemPtr->childCHitNodePtr == nullptr);

								goto end;
							}

							if (currentResultItemPtr->childMissNodePtr != nullptr)
							{
								DE_ASSERT(currentResultItemPtr->childMissNodePtr == nullptr);

								goto end;
							}
						}
					}
				}
			}
		}

		// 2. Verify depth level is correct for each node.
		for (const auto& iterator1 : nRayToNLevelToResultItemPtrVecMap)
		{
			const auto& currentNLevelToResultItemPtrVecMap	= iterator1.second;

			for (const auto& iterator2 : currentNLevelToResultItemPtrVecMap)
			{
				const auto& currentNLevel			= iterator2.first;
				const auto& currentResultItemPtrVec	= iterator2.second;

				for (const auto& currentResultItemPtr : currentResultItemPtrVec)
				{
					if (currentResultItemPtr->stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
					{
						if (currentResultItemPtr->depth != 0)
						{
							DE_ASSERT(currentResultItemPtr->depth == 0);

							goto end;
						}
					}
					else
					if (currentResultItemPtr->depth != currentNLevel)
					{
						DE_ASSERT(currentResultItemPtr->depth == currentNLevel);

						goto end;
					}
				}
			}
		}

		// 3. Verify child node ptrs point to nodes that are assigned correct shader stage.
		for (const auto& iterator : nItemToResultItemPtrMap)
		{
			const auto& currentResultItemPtr = iterator.second;

			if (currentResultItemPtr->childCHitNodePtr			!= nullptr								&&
				currentResultItemPtr->childCHitNodePtr->stage	!= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
			{
				DE_ASSERT(currentResultItemPtr->childCHitNodePtr->stage	== VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

				goto end;
			}

			if (currentResultItemPtr->childMissNodePtr			!= nullptr						&&
				currentResultItemPtr->childMissNodePtr->stage	!= VK_SHADER_STAGE_MISS_BIT_KHR)
			{
				DE_ASSERT(currentResultItemPtr->childMissNodePtr->stage= VK_SHADER_STAGE_MISS_BIT_KHR);

				goto end;
			}
		}

		// 4. Verify nodes hold correct ray index.
		for (const auto& iterator : nRayToResultItemPtrVecMap)
		{
			const auto& currentNRay = iterator.first;

			for (const auto& currentResultItemPtr : iterator.second)
			{
				if (currentResultItemPtr->nOriginRay != currentNRay)
				{
					DE_ASSERT(currentResultItemPtr->nOriginRay == currentNRay);

					goto end;
				}
			}
		}

		// 5. Verify child nodes are assigned correct depth levels.
		for (const auto& iterator1 : nRayToNLevelToResultItemPtrVecMap)
		{
			const auto& currentNLevelToResultItemPtrVecMap	= iterator1.second;

			for (const auto& iterator2 : currentNLevelToResultItemPtrVecMap)
			{
				const auto& currentNLevel			= iterator2.first;
				const auto& currentResultItemPtrVec	= iterator2.second;

				for (const auto& currentResultItemPtr : currentResultItemPtrVec)
				{
					const auto expectedChildNodeDepth = (currentResultItemPtr->stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) ? 0
																														: currentResultItemPtr->depth + 1;

					if (currentResultItemPtr->depth != currentNLevel)
					{
						DE_ASSERT(currentResultItemPtr->depth == currentNLevel);

						goto end;
					}

					if (currentResultItemPtr->childCHitNodePtr			!= nullptr					&&
						currentResultItemPtr->childCHitNodePtr->depth	!= expectedChildNodeDepth)
					{
						DE_ASSERT(currentResultItemPtr->childCHitNodePtr->depth	== expectedChildNodeDepth);

						goto end;
					}

					if (currentResultItemPtr->childMissNodePtr			!= nullptr					&&
						currentResultItemPtr->childMissNodePtr->depth	!= expectedChildNodeDepth)
					{
						DE_ASSERT(currentResultItemPtr->childMissNodePtr->depth	== expectedChildNodeDepth);

						goto end;
					}
				}
			}
		}

		// 6. Verify that RT shader stages were invoked for all anticipated recursion levels.
		if (doFullCheck)
		{
			for (const auto& iterator1 : nRayToNLevelToResultItemPtrVecMap)
			{
				for (deUint32	nLevel = 0;
								nLevel < m_depthToUse;
								nLevel ++)
				{
					if (iterator1.second.find(nLevel) == iterator1.second.end())
					{
						DE_ASSERT(false);

						goto end;
					}
				}
			}
		}

		result = true;
end:
		return result;
	}

private:

	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	deUint32										m_depthToUse;
	deUint32										m_nMaxResultItemsPermitted;
	const deUint32									m_nRaysToTest;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;

	VkSpecializationInfo		m_specializationInfo;
	VkSpecializationMapEntry	m_specializationEntry;

	mutable std::vector<std::string> m_ahitShaderNameVec;
	mutable std::vector<std::string> m_chitShaderNameVec;
	mutable std::vector<std::string> m_missShaderNameVec;

	deUint32 m_nCHitInvocationsExpected;
	deUint32 m_nMissInvocationsExpected;
	deUint32 m_nResultItemsExpected;

	const deUint32 m_maxResultBufferSizePermitted;
};

// Test the return value of reportIntersectionEXT
class ReportIntersectionResultTest : public TestBase
{
public:
	ReportIntersectionResultTest(const AccelerationStructureLayout&	asLayout,
		const GeometryType&				geometryType)
		: m_asLayout(asLayout)
		, m_geometryType(geometryType)
		, m_gridSizeXYZ(tcu::UVec3(4, 4, 1))
		, m_nRaysToTrace(16)
	{
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return m_gridSizeXYZ;
	}

	deUint32 getResultBufferSize() const final
	{
		return static_cast<deUint32>(2u * sizeof(deUint32) * m_nRaysToTrace);
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const	final
	{
		return { m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
		RayTracingProperties*	/* rtPropertiesPtr */,
		VkCommandBuffer			commandBuffer) final
	{
		m_asProviderPtr.reset(
			new GridASProvider(tcu::Vec3(0, 0, 0),		// gridStartXYZ
				tcu::Vec3(1, 1, 1),						// gridCellSizeXYZ
				m_gridSizeXYZ,
				tcu::Vec3(2.0f, 2.0f, 2.0f),			// gridInterCellDeltaXYZ
				m_geometryType)
		);

		m_tlPtr = m_asProviderPtr->createTLAS(context,
			m_asLayout,
			commandBuffer,
			0u,
			nullptr,									// optASPropertyProviderPtr
			nullptr);									// optASFedbackPtr
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		const std::string				hitPropertiesDefinition =
			"struct HitProperties\n"
			"{\n"
			"    uint nHitsRejected;\n"
			"    uint nHitsAccepteded;\n"
			"};\n";
		const std::string hitPropertiesDeclaration =
			"layout(set = 0, binding = 0, std430) buffer result\n"
			"{\n"
			"    HitProperties rayToHitProps[" + de::toString(m_nRaysToTrace) + "];\n"
			"};\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(std::string() +
			"#version 460 core\n"
			"\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			"hitAttributeEXT vec3 dummyAttribute;\n"
			"\n"
			+ hitPropertiesDefinition +
			"\n"
			"layout(location = 0) rayPayloadInEXT dummy { vec3 dummyVec;};\n"
			+ hitPropertiesDeclaration +
			"\n"
			"void main()\n"
			"{\n"
			"    uint nRay = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
			"    if ((gl_RayTmaxEXT > 0.6) && (gl_RayTmaxEXT < 0.8))\n"
			"    {\n"
			"        atomicAdd(rayToHitProps[nRay].nHitsRejected, 1);\n"
			"        ignoreIntersectionEXT;\n"							// reportIntersectionEXT should return false
			"    }\n"
			"    else if ((gl_RayTmaxEXT > 0.1) && (gl_RayTmaxEXT < 0.3))\n"
			"    {\n"
			"        atomicAdd(rayToHitProps[nRay].nHitsAccepteded, 1);\n"
			"    }\n"
			"}\n")
			<< buildOptions;

		programCollection.glslSources.add("intersection") << glu::IntersectionSource(
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    bool resultThatShouldBeRejected = reportIntersectionEXT(0.7f, 0);\n"
			"    if (resultThatShouldBeRejected)\n"
			"        reportIntersectionEXT(0.7f, 0);\n"
			"    else\n"
			"    {\n"
			"         bool resultThatShouldBeAccepted = reportIntersectionEXT(0.2f, 0);\n"
			"         if (!resultThatShouldBeAccepted)\n"
			"             reportIntersectionEXT(0.2f, 0);\n"
			"    }\n"
			"}\n")
			<< buildOptions;

		programCollection.glslSources.add("miss") << glu::MissSource(std::string() +
			"#version 460 core\n"
			"\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			+ hitPropertiesDefinition +
			"layout(location = 0) rayPayloadInEXT vec3 dummy;\n"
			+ hitPropertiesDeclaration +
			"\n"
			"void main()\n"
			"{\n"
			"}\n")
			<< buildOptions;

		programCollection.glslSources.add("rgen") << glu::RaygenSource(
			"#version 460 core\n"
			"\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			+ hitPropertiesDefinition +
			"layout(location = 0)              rayPayloadEXT vec3                     dummy;\n"
			"layout(set      = 0, binding = 1) uniform       accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    uint  rayFlags    = 0;\n"
			"    uint  cullMask    = 0xFF;\n"
			"    float tmin        = 0.001;\n"
			"    float tmax        = 9.0;\n"
			"    vec3  origin      = vec3(4, 4, 4);\n"
			"    vec3  target      = vec3(float(gl_LaunchIDEXT.x * 2) + 0.5f, float(gl_LaunchIDEXT.y * 2) + 0.5f, float(gl_LaunchIDEXT.z * 2) + 0.5f);\n"
			"    vec3  direct      = normalize(target - origin);\n"
			"\n"
			"    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"}\n")
			<< buildOptions;
	}

	bool verifyResultBuffer(const void* resultDataPtr) const final
	{
		for (deUint32 nRay = 0; nRay < m_nRaysToTrace; ++nRay)
		{
			const deUint32* rayProps = reinterpret_cast<const deUint32*>(resultDataPtr) + 2 * nRay;
			if ((rayProps[0] != 1) || (rayProps[1] != 1))
				return false;
		}
		return true;
	}

private:
	const AccelerationStructureLayout	m_asLayout;
	const GeometryType					m_geometryType;
	const tcu::UVec3					m_gridSizeXYZ;
	const deUint32						m_nRaysToTrace;

	std::unique_ptr<GridASProvider>					m_asProviderPtr;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};

class RayPayloadInTest : public TestBase
{
public:
	RayPayloadInTest(const GeometryType&					geometryType,
		const AccelerationStructureLayout&	asStructureLayout)
		: m_asStructureLayout(asStructureLayout),
		m_geometryType(geometryType),
		m_gridSizeXYZ(tcu::UVec3(512, 1, 1)),
		m_nRayPayloadU32s(512)
	{
	}

	~RayPayloadInTest()
	{
		/* Stub */
	}

	tcu::UVec3 getDispatchSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);

		return tcu::UVec3(m_gridSizeXYZ[0], m_gridSizeXYZ[1], m_gridSizeXYZ[2]);
	}

	deUint32 getResultBufferSize() const final
	{
		DE_ASSERT(m_gridSizeXYZ[0] != 0);
		DE_ASSERT(m_gridSizeXYZ[1] != 0);
		DE_ASSERT(m_gridSizeXYZ[2] != 0);
		DE_ASSERT(m_nRayPayloadU32s != 0);

		const auto nRays = m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];

		DE_ASSERT(nRays != 0);
		DE_ASSERT((nRays % 2) == 0);

		const auto nMissShaderInvocationsExpected = nRays / 2;
		const auto nAHitShaderInvocationsExpected = nRays / 2;
		const auto nCHitShaderInvocationsExpected = nAHitShaderInvocationsExpected;
		const auto nResultStoresExpected = nMissShaderInvocationsExpected + nAHitShaderInvocationsExpected + nCHitShaderInvocationsExpected;

		return static_cast<deUint32>((1 /* nItems */ + m_nRayPayloadU32s * nResultStoresExpected) * sizeof(deUint32));
	}

	VkSpecializationInfo* getSpecializationInfoPtr(const VkShaderStageFlagBits& shaderStage) final
	{
		VkSpecializationInfo* resultPtr = nullptr;

		if (shaderStage == VK_SHADER_STAGE_MISS_BIT_KHR ||
			shaderStage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR ||
			shaderStage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR ||
			shaderStage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		{
			resultPtr = &m_specializationInfo;
		}

		return resultPtr;
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const final
	{
		DE_ASSERT(m_tlPtr != nullptr);

		return { m_tlPtr.get() };
	}

	bool init(	vkt::Context&			/* context	       */,
				RayTracingProperties*	/* rtPropertiesPtr */) final
	{
		m_specializationInfoMapEntry.constantID = 1;
		m_specializationInfoMapEntry.offset = 0;
		m_specializationInfoMapEntry.size = sizeof(deUint32);

		m_specializationInfo.dataSize = sizeof(deUint32);
		m_specializationInfo.mapEntryCount = 1;
		m_specializationInfo.pData = reinterpret_cast<const void*>(&m_nRayPayloadU32s);
		m_specializationInfo.pMapEntries = &m_specializationInfoMapEntry;

		return true;
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
		RayTracingProperties*	/* rtPropertiesPtr */,
		VkCommandBuffer			commandBuffer) final
	{
		std::unique_ptr<GridASProvider> asProviderPtr(
			new GridASProvider(tcu::Vec3(0, 0, 0), /* gridStartXYZ          */
				tcu::Vec3(1, 1, 1), /* gridCellSizeXYZ       */
				m_gridSizeXYZ,
				tcu::Vec3(6, 0, 0), /* gridInterCellDeltaXYZ */
				m_geometryType)
		);

		m_tlPtr = asProviderPtr->createTLAS(context,
			m_asStructureLayout,
			commandBuffer,
			VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
			nullptr,
			nullptr);
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion,
			vk::SPIRV_VERSION_1_4,
			0u,		/* flags        */
			true);	/* allowSpirv14 */

		const char* constantDefinitions =
			"layout(constant_id = 1) const uint N_UINTS_IN_RAY_PAYLOAD = 1;\n";

		const char* rayPayloadDefinition =
			"\n"
			"layout(location = 0) rayPayloadEXT block\n"
			"{\n"
			"    uint values[N_UINTS_IN_RAY_PAYLOAD];\n"
			"};\n"
			"\n";

		const char* rayPayloadInDefinition =
			"\n"
			"layout(location = 0) rayPayloadInEXT block\n"
			"{\n"
			"    uint values[N_UINTS_IN_RAY_PAYLOAD];\n"
			"};\n"
			"\n";

		const char* resultBufferDefinition =
			"layout(set      = 0, binding = 0, std430) buffer result\n"
			"{\n"
			"    uint nItemsStored;\n"
			"    uint resultValues[];\n"
			"};\n";

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(resultBufferDefinition)
				+ de::toString(rayPayloadInDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    uint nItem = atomicAdd(nItemsStored, 1);\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_RAY_PAYLOAD; ++nUint)\n"
				"    {\n"
				"        resultValues[nItem * N_UINTS_IN_RAY_PAYLOAD + nUint] = values[nUint];\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(resultBufferDefinition)
				+ de::toString(rayPayloadInDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    uint nItem = atomicAdd(nItemsStored, 1);\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_RAY_PAYLOAD; ++nUint)\n"
				"    {\n"
				"        resultValues[nItem * N_UINTS_IN_RAY_PAYLOAD + nUint] = values[nUint];\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"void main()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(resultBufferDefinition)
				+ de::toString(rayPayloadInDefinition) +
				"\n"
				"void main()\n"
				"{\n"
				"    uint nItem = atomicAdd(nItemsStored, 1);\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_RAY_PAYLOAD; ++nUint)\n"
				"    {\n"
				"        resultValues[nItem * N_UINTS_IN_RAY_PAYLOAD + nUint] = values[nUint];\n"
				"    }\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str()) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(set = 0, binding = 1) uniform accelerationStructureEXT accelerationStructure;\n"
				"\n"
				+ de::toString(constantDefinitions)
				+ de::toString(rayPayloadDefinition) +
				"void main()\n"
				"{\n"
				"    uint  nInvocation  = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags     = 0;\n"
				"    float tmin         = 0.001;\n"
				"    float tmax         = 2.1;\n"
				"\n"
				"    uint  cullMask     = 0xFF;\n"
				"    vec3  cellStartXYZ = vec3(nInvocation * 3.0, 0.0, 0.0);\n"
				"    vec3  cellEndXYZ   = cellStartXYZ + vec3(1.0);\n"
				"    vec3  target       = mix(cellStartXYZ, cellEndXYZ, vec3(0.5) );\n"
				"    vec3  origin       = target - vec3(0, 2, 0);\n"
				"    vec3  direct       = normalize(target - origin);\n"
				"\n"
				"    for (uint nUint = 0; nUint < N_UINTS_IN_RAY_PAYLOAD; ++nUint)\n"
				"    {\n"
				"        values[nUint] = (1 + nUint);\n"
				"    }\n"
				"\n"
				"    traceRayEXT(accelerationStructure, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
		}
	}

	bool verifyResultBuffer(const void* resultDataPtr) const final
	{
		const deUint32* resultU32Ptr = reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result = false;

		const auto nItemsStored = *resultU32Ptr;
		const auto nRays = m_gridSizeXYZ[0] * m_gridSizeXYZ[1] * m_gridSizeXYZ[2];
		const auto nMissShaderInvocationsExpected = nRays / 2;
		const auto nAHitShaderInvocationsExpected = nRays / 2;
		const auto nCHitShaderInvocationsExpected = nAHitShaderInvocationsExpected;
		const auto nResultStoresExpected = nMissShaderInvocationsExpected + nAHitShaderInvocationsExpected + nCHitShaderInvocationsExpected;

		if (nItemsStored != nResultStoresExpected)
		{
			goto end;
		}

		for (deUint32 nItem = 0; nItem < nItemsStored; ++nItem)
		{
			const auto resultItemDataPtr = resultU32Ptr + 1 /* nItemsStored */ + nItem * m_nRayPayloadU32s;

			for (deUint32 nValue = 0; nValue < m_nRayPayloadU32s; ++nValue)
			{
				if (resultItemDataPtr[nValue] != (1 + nValue))
				{
					goto end;
				}
			}
		}

		result = true;
	end:
		return result;
	}

private:

	const AccelerationStructureLayout	m_asStructureLayout;
	const GeometryType					m_geometryType;

	const tcu::UVec3								m_gridSizeXYZ;
	deUint32										m_nRayPayloadU32s;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;

	VkSpecializationInfo		m_specializationInfo;
	VkSpecializationMapEntry	m_specializationInfoMapEntry;
};

class TerminationTest : public TestBase
{
public:
	enum class Mode
	{
		IGNORE_ANY_HIT_STATICALLY,
		IGNORE_ANY_HIT_DYNAMICALLY,
		TERMINATE_ANY_HIT_STATICALLY,
		TERMINATE_ANY_HIT_DYNAMICALLY,
		TERMINATE_INTERSECTION_STATICALLY,
		TERMINATE_INTERSECTION_DYNAMICALLY,

		UNKNOWN
	};

	static Mode getModeFromTestType(const TestType& testType)
	{
		Mode result = Mode::UNKNOWN;

		switch (testType)
		{
			case TestType::IGNORE_ANY_HIT_DYNAMICALLY:			result = Mode::IGNORE_ANY_HIT_DYNAMICALLY;			break;
			case TestType::IGNORE_ANY_HIT_STATICALLY:			result = Mode::IGNORE_ANY_HIT_STATICALLY;			break;
			case TestType::TERMINATE_ANY_HIT_DYNAMICALLY:		result = Mode::TERMINATE_ANY_HIT_DYNAMICALLY;		break;
			case TestType::TERMINATE_ANY_HIT_STATICALLY:		result = Mode::TERMINATE_ANY_HIT_STATICALLY;		break;
			case TestType::TERMINATE_INTERSECTION_DYNAMICALLY:	result = Mode::TERMINATE_INTERSECTION_DYNAMICALLY;	break;
			case TestType::TERMINATE_INTERSECTION_STATICALLY:	result = Mode::TERMINATE_INTERSECTION_STATICALLY;	break;

			default:
			{
				DE_ASSERT(false && "This should never happen");
			}
		}

		return result;
	}

	TerminationTest(const Mode& mode)
		:m_mode(mode)
	{
		/* Stub */
	}

	~TerminationTest()
	{
		/* Stub */
	}

	std::vector<std::string> getCHitShaderCollectionShaderNames() const final
	{
		return {};
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(1, 1, 1);
	}

	std::vector<deUint8> getResultBufferStartData() const final
	{
		auto resultU8Vec		= std::vector<deUint8>			(getResultBufferSize	() );
		auto resultU32DataPtr	= reinterpret_cast<deUint32*>	(resultU8Vec.data		() );

		memset(	resultU8Vec.data(),
				0,
				resultU8Vec.size() );

		if (m_mode == Mode::IGNORE_ANY_HIT_DYNAMICALLY		||
			m_mode == Mode::TERMINATE_ANY_HIT_DYNAMICALLY)
		{
			resultU32DataPtr[2] = 1;
		}
		else
		if (m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY)
		{
			resultU32DataPtr[3] = 1;
		}

		return resultU8Vec;
	}

	deUint32 getResultBufferSize() const final
	{
		const deUint32 nExtraUints	= (	m_mode == Mode::IGNORE_ANY_HIT_DYNAMICALLY			||
										m_mode == Mode::TERMINATE_ANY_HIT_DYNAMICALLY		||
										m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY) ?	1
																							:	0;
		const deUint32 nResultUints	= (	m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY	||
										m_mode == Mode::TERMINATE_INTERSECTION_STATICALLY)	?	3
																							:	2;

		return static_cast<deUint32>(sizeof(deUint32) ) * (nExtraUints + nResultUints);
	}

	std::vector<TopLevelAccelerationStructure*>	getTLASPtrVecToBind() const	final
	{
		return {m_tlPtr.get() };
	}

	void resetTLAS() final
	{
		m_tlPtr.reset();
	}

	void initAS(vkt::Context&			context,
				RayTracingProperties*	/* rtPropertiesPtr */,
				VkCommandBuffer			commandBuffer) final
	{
		if (m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY ||
			m_mode == Mode::TERMINATE_INTERSECTION_STATICALLY)
		{
			const tcu::Vec3		gridCellSizeXYZ			= tcu::Vec3	( 2,  1,  1);
			const tcu::Vec3		gridInterCellDeltaXYZ	= tcu::Vec3	( 3,  3,  3);
			const tcu::UVec3	gridSizeXYZ				= tcu::UVec3( 1,  1,  1);
			const tcu::Vec3		gridStartXYZ			= tcu::Vec3	(-1, -1, -1);

			m_asProviderPtr.reset(
				new GridASProvider(	gridStartXYZ,
									gridCellSizeXYZ,
									gridSizeXYZ,
									gridInterCellDeltaXYZ,
									GeometryType::AABB)
			);
		}
		else
		{
			m_asProviderPtr.reset(
				new TriASProvider()
			);
		}

		m_tlPtr  = m_asProviderPtr->createTLAS(	context,
												AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY,
												commandBuffer,
												VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR,
												nullptr,	/* optASPropertyProviderPtr */
												nullptr);	/* optASFedbackPtr          */
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const std::string resultBufferSizeString = de::toString(getResultBufferSize() / sizeof(deUint32) );

		{
			std::string aHitShader;

			switch (m_mode)
			{
				case Mode::IGNORE_ANY_HIT_DYNAMICALLY:
				{
					aHitShader =
						"#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"hitAttributeEXT vec3 dummyAttribute;\n"
						"\n"
						"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
						"layout(set      = 0, binding = 0, std430) buffer result\n"
						"{\n"
						"    uint resultData[" + resultBufferSizeString + "];\n"
						"};\n"
						"\n"
						"void ignoreIntersectionWrapper()\n"
						"{\n"
						"    ignoreIntersectionEXT;\n"
						"}\n"
						"\n"
						"void main()\n"
						"{\n"
						"\n"
						"    if (resultData[2] == 1)\n"
						"    {\n"
						"        ignoreIntersectionWrapper();\n"
						"    }\n"
						"\n"
						"    resultData[0] = 1;\n"
						"}\n";

					break;
				}

				case Mode::IGNORE_ANY_HIT_STATICALLY:
				{
					aHitShader =
						"#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"hitAttributeEXT vec3 dummyAttribute;\n"
						"\n"
						"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
						"layout(set      = 0, binding = 0, std430) buffer result\n"
						"{\n"
						"    uint resultData[" + resultBufferSizeString + "];\n"
						"};\n"
						"\n"
						"void ignoreIntersectionWrapper()\n"
						"{\n"
						"    ignoreIntersectionEXT;\n"
						"}\n"
						"\n"
						"void main()\n"
						"{\n"
						"    ignoreIntersectionWrapper();\n"
						"\n"
						"    resultData[0] = 1;\n"
						"}\n";

					break;
				}

				case Mode::TERMINATE_ANY_HIT_DYNAMICALLY:
				{
					aHitShader =
						"#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"hitAttributeEXT vec3 dummyAttribute;\n"
						"\n"
						"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
						"layout(set      = 0, binding = 0, std430) buffer result\n"
						"{\n"
						"    uint resultData[" + resultBufferSizeString + "];\n"
						"};\n"
						"\n"
						"void terminateRayWrapper()\n"
						"{\n"
						"    terminateRayEXT;\n"
						"}\n"
						"\n"
						"void main()\n"
						"{\n"
						"    if (resultData[2] == 1)\n"
						"    {\n"
						"        terminateRayWrapper();\n"
						"    }\n"
						"\n"
						"    resultData[0] = 1;\n"
						"}\n";

					break;
				}

				case Mode::TERMINATE_ANY_HIT_STATICALLY:
				case Mode::TERMINATE_INTERSECTION_STATICALLY:
				{
					aHitShader =
						"#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"hitAttributeEXT vec3 dummyAttribute;\n"
						"\n"
						"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
						"layout(set      = 0, binding = 0, std430) buffer result\n"
						"{\n"
						"    uint resultData[" + resultBufferSizeString + "];\n"
						"};\n"
						"\n"
						"void terminateRayWrapper()\n"
						"{\n"
						"    terminateRayEXT;\n"
						"}\n"
						"\n"
						"void main()\n"
						"{\n"
						"    terminateRayWrapper();\n"
						"\n"
						"    resultData[0] = 1;\n"
						"}\n";

					break;
				}

				case Mode::TERMINATE_INTERSECTION_DYNAMICALLY:
				{
					aHitShader =
						"#version 460 core\n"
						"\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"\n"
						"hitAttributeEXT vec3 dummyAttribute;\n"
						"\n"
						"layout(location = 0) rayPayloadInEXT      dummy { vec3 dummyVec;};\n"
						"layout(set      = 0, binding = 0, std430) buffer result\n"
						"{\n"
						"    uint resultData[" + resultBufferSizeString + "];\n"
						"};\n"
						"\n"
						"void terminateRayWrapper()\n"
						"{\n"
						"    terminateRayEXT;\n"
						"}\n"
						"\n"
						"void main()\n"
						"{\n"
						"    if (resultData[3] == 1)\n"
						"    {\n"
						"        terminateRayWrapper();\n"
						"    }\n"
						"\n"
						"    resultData[0] = 1;\n"
						"}\n";

					break;
				}

				default:
				{
					DE_ASSERT(false);
				}
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(aHitShader) << buildOptions;
		}

		if (m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY ||
		    m_mode == Mode::TERMINATE_INTERSECTION_STATICALLY)
		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"hitAttributeEXT vec3 hitAttribute;\n"
				"\n"
				"layout(set = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint resultData[4];\n"
				"};\n"
				"\n"
				"void generateIntersection()\n"
				"{\n"
				"    reportIntersectionEXT(0.95f, 0);\n"
				"}\n"
				"\n"
				"void main()\n"
				"{\n";

			if (m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY)
			{
				css <<	"    if (resultData[3] == 1)\n"
						"    {\n";
			}

			css <<	"    generateIntersection();\n";

			if (m_mode == Mode::TERMINATE_INTERSECTION_DYNAMICALLY)
			{
				css <<	"    }\n";
			}

			css <<
				"\n"
				"    resultData[2] = 1;\n"
				"}\n";

			programCollection.glslSources.add("intersection") << glu::IntersectionSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0) rayPayloadInEXT      vec3   dummy;\n"
				"layout(set      = 0, binding = 0, std430) buffer result\n"
				"{\n"
				"    uint resultData[2];\n"
				"};\n"
				"\n"
				"void main()\n"
				"{\n"
				"    resultData[1] = 1;\n"
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(css.str() ) << buildOptions;
		}

		{
			std::stringstream css;

			css <<
				"#version 460 core\n"
				"\n"
				"#extension GL_EXT_ray_tracing : require\n"
				"\n"
				"layout(location = 0)              rayPayloadEXT vec3                     dummy;\n"
				"layout(set      = 0, binding = 1) uniform       accelerationStructureEXT topLevelAS;\n"
				"\n"
				"void main()\n"
				"{\n"
				"    uint  nInvocation = gl_LaunchIDEXT.z * gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;\n"
				"    uint  rayFlags    = 0;\n"
				"    uint  cullMask    = 0xFF;\n"
				"    float tmin        = 0.001;\n"
				"    float tmax        = 9.0;\n"
				"    vec3  origin      = vec3(-1,  -1,  -1);\n"
				"    vec3  target      = vec3(0.0, 0.5,  0);\n"
				"    vec3  direct      = normalize(target - origin);\n"
				"\n"
				"    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str() ) << buildOptions;
		}
	}

	bool verifyResultBuffer (const void* resultDataPtr) const final
	{
		const deUint32* resultU32DataPtr	= reinterpret_cast<const deUint32*>(resultDataPtr);
		bool			result				= false;

		switch (m_mode)
		{
			case Mode::IGNORE_ANY_HIT_DYNAMICALLY:
			case Mode::IGNORE_ANY_HIT_STATICALLY:
			{
				if (resultU32DataPtr[0] != 0 ||
					resultU32DataPtr[1] != 1)
				{
					goto end;
				}

				result = true;

				break;
			}

			case Mode::TERMINATE_ANY_HIT_DYNAMICALLY:
			case Mode::TERMINATE_ANY_HIT_STATICALLY:
			{
				if (resultU32DataPtr[0] != 0 ||
					resultU32DataPtr[1] != 0)
				{
					goto end;
				}

				result = true;

				break;
			}

			case Mode::TERMINATE_INTERSECTION_DYNAMICALLY:
			case Mode::TERMINATE_INTERSECTION_STATICALLY:
			{
				if (resultU32DataPtr[0] != 0 ||
					resultU32DataPtr[1] != 0 ||
					resultU32DataPtr[2] != 0)
				{
					goto end;
				}

				result = true;

				break;
			}

			default:
			{
				TCU_FAIL("This should never be reached");
			}
		}

end:
		return result;
	}

private:
	std::unique_ptr<ASProviderBase>					m_asProviderPtr;
	const Mode										m_mode;
	std::unique_ptr<TopLevelAccelerationStructure>	m_tlPtr;
};

/* Generic misc test instance */
class RayTracingMiscTestInstance : public TestInstance
{
public:
	 RayTracingMiscTestInstance (	Context&				context,
									const CaseDef&			data,
									TestBase*				testPtr);
	~RayTracingMiscTestInstance (	void);

	tcu::TestStatus	iterate	(void);

protected:
	void							checkSupport(void) const;
	de::MovePtr<BufferWithMemory>	runTest		(void);

private:
	CaseDef	m_data;

	de::MovePtr<RayTracingProperties>	m_rayTracingPropsPtr;
	TestBase*							m_testPtr;
};

RayTracingMiscTestInstance::RayTracingMiscTestInstance (Context&					context,
														const CaseDef&				data,
														TestBase*					testPtr)
	: vkt::TestInstance			(context)
	, m_data					(data)
	, m_rayTracingPropsPtr		(makeRayTracingProperties(context.getInstanceInterface(),
														  context.getPhysicalDevice()))
	, m_testPtr					(testPtr)
{
	m_testPtr->init(m_context, m_rayTracingPropsPtr.get());
 }

RayTracingMiscTestInstance::~RayTracingMiscTestInstance(void)
{
	/* Stub */
}

void RayTracingMiscTestInstance::checkSupport(void) const
{
	if (m_testPtr->getResultBufferSize() > m_context.getDeviceVulkan11Properties().maxMemoryAllocationSize)
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceVulkan11Properties::maxMemoryAllocationSize too small, allocation might fail");
}

de::MovePtr<BufferWithMemory> RayTracingMiscTestInstance::runTest(void)
{
	const DeviceInterface&		deviceInterface		= m_context.getDeviceInterface	();
	const VkDevice				deviceVk			= m_context.getDevice			();

	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue			queueVk				= m_context.getUniversalQueue			();
	Allocator&				allocator			= m_context.getDefaultAllocator			();

	de::MovePtr<BufferWithMemory>					resultBufferPtr;

	// Determine group indices
	const auto ahitCollectionShaderNameVec			= m_testPtr->getAHitShaderCollectionShaderNames			();
	const auto chitCollectionShaderNameVec			= m_testPtr->getCHitShaderCollectionShaderNames			();
	const auto intersectionCollectionShaderNameVec	= m_testPtr->getIntersectionShaderCollectionShaderNames	();
	const auto missCollectionShaderNameVec			= m_testPtr->getMissShaderCollectionShaderNames			();

	const deUint32 nRaygenGroups	= 1;
	const deUint32 nMissGroups		= static_cast<deUint32>(missCollectionShaderNameVec.size() );
	const deUint32 nHitGroups		= de::max(
											de::max(	static_cast<deUint32>(ahitCollectionShaderNameVec.size() ),
														static_cast<deUint32>(chitCollectionShaderNameVec.size() ) ),
											static_cast<deUint32>(intersectionCollectionShaderNameVec.size() ));

	const deUint32 raygenGroupIndex = 0;
	const deUint32 missGroupIndex   = nRaygenGroups;
	const deUint32 hitGroupIndex    = missGroupIndex + nMissGroups;

	const auto	callableShaderCollectionNames	= m_testPtr->getCallableShaderCollectionNames	();
	auto&		collection						= m_context.getBinaryCollection					();
	const auto resultBufferSize					= m_testPtr->getResultBufferSize				();


	const Move<VkDescriptorSetLayout>	descriptorSetLayoutPtr	= DescriptorSetLayoutBuilder()
																	.addSingleBinding(	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
																						ALL_RAY_TRACING_STAGES)
																	.addArrayBinding(	VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
																						m_testPtr->getASBindingArraySize(),
																						ALL_RAY_TRACING_STAGES)
																	.build			(	deviceInterface,
																						deviceVk);

	const Move<VkDescriptorPool>		descriptorPoolPtr		= DescriptorPoolBuilder()
																	.addType(	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																	.addType(	VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
																				m_testPtr->getASBindingArraySize() )
																	.build	(	deviceInterface,
																				deviceVk,
																				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
																				1u); /* maxSets */

	const Move<VkDescriptorSet>			descriptorSetPtr		= makeDescriptorSet(deviceInterface,
																					deviceVk,
																					*descriptorPoolPtr,
																					*descriptorSetLayoutPtr);

	const Move<VkPipelineLayout>		pipelineLayoutPtr		= m_testPtr->getPipelineLayout(	deviceInterface,
																								deviceVk,
																								descriptorSetLayoutPtr.get() );

	const Move<VkCommandPool>			cmdPoolPtr				= createCommandPool(deviceInterface,
																					deviceVk,
																					0, /* pCreateInfo */
																					queueFamilyIndex);

	const Move<VkCommandBuffer>			cmdBufferPtr			= allocateCommandBuffer(deviceInterface,
																						deviceVk,
																						*cmdPoolPtr,
																						VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Move<VkPipeline>					pipelineVkPtr;
	de::MovePtr<RayTracingPipeline>		rayTracingPipelinePtr	= de::newMovePtr<RayTracingPipeline>();

	{
		Move<VkShaderModule>	raygenShader		= createShaderModule(	deviceInterface,
																			deviceVk,
																			collection.get("rgen"),
																			0); /* flags */

		rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_RAYGEN_BIT_KHR,
											makeVkSharedPtr(raygenShader),
											raygenGroupIndex,
											m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_RAYGEN_BIT_KHR) );
	}

	{
		for (deUint32	nMissShaderName = 0;
						nMissShaderName < static_cast<deUint32>(missCollectionShaderNameVec.size() );
						nMissShaderName ++)
		{
			const auto&				currentMissShaderName	= missCollectionShaderNameVec.at(nMissShaderName);
			Move<VkShaderModule>	missShader				= createShaderModule(	deviceInterface,
																					deviceVk,
																					collection.get(currentMissShaderName),
																					0); /* flags */

			rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_MISS_BIT_KHR,
											makeVkSharedPtr(missShader),
											missGroupIndex + nMissShaderName,
											m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_MISS_BIT_KHR) );
		}
	}

	{
		for (deUint32	nAHitShaderName = 0;
						nAHitShaderName < static_cast<deUint32>(ahitCollectionShaderNameVec.size() );
						nAHitShaderName ++)
		{
			const auto&				currentAHitShaderName	= ahitCollectionShaderNameVec.at(nAHitShaderName);
			Move<VkShaderModule>	anyHitShader			= createShaderModule(	deviceInterface,
																					deviceVk,
																					collection.get(currentAHitShaderName),
																					0); /* flags */

			rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
												makeVkSharedPtr(anyHitShader),
												hitGroupIndex + nAHitShaderName,
												m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_ANY_HIT_BIT_KHR) );
		}

		for (deUint32	nCHitShaderName = 0;
						nCHitShaderName < static_cast<deUint32>(chitCollectionShaderNameVec.size() );
						nCHitShaderName ++)
		{
			const auto&				currentCHitShaderName	= chitCollectionShaderNameVec.at(nCHitShaderName);
			Move<VkShaderModule>	closestHitShader		= createShaderModule(	deviceInterface,
																					deviceVk,
																					collection.get(currentCHitShaderName),
																					0); /* flags */

			rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
												makeVkSharedPtr(closestHitShader),
												hitGroupIndex + nCHitShaderName,
												m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) );
		}

		if (m_data.geometryType == GeometryType::AABB				||
			m_data.geometryType == GeometryType::AABB_AND_TRIANGLES)
		{
			for (deUint32	nIntersectionShaderName = 0;
							nIntersectionShaderName < static_cast<deUint32>(intersectionCollectionShaderNameVec.size() );
							nIntersectionShaderName ++)
			{
				const auto&				currentIntersectionShaderName	= intersectionCollectionShaderNameVec.at(nIntersectionShaderName);
				Move<VkShaderModule>	intersectionShader				= createShaderModule(	deviceInterface,
																								deviceVk,
																								collection.get(currentIntersectionShaderName),
																								0); /* flags */

				rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
													makeVkSharedPtr(intersectionShader),
													hitGroupIndex + nIntersectionShaderName,
													m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_INTERSECTION_BIT_KHR) );
			}
		}

		for (deUint32 nCallableShader = 0; nCallableShader < static_cast<deUint32>(callableShaderCollectionNames.size() ); ++nCallableShader)
		{
			const auto&				currentCallableShaderName	= callableShaderCollectionNames.at	(	nCallableShader);
			Move<VkShaderModule>	callableShader				= createShaderModule				(	deviceInterface,
																										deviceVk,
																										collection.get(currentCallableShaderName),
																										0); /* flags */

			rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_CALLABLE_BIT_KHR,
												makeVkSharedPtr(callableShader),
												static_cast<deUint32>(ShaderGroups::FIRST_CALLABLE_GROUP) + nCallableShader,
												m_testPtr->getSpecializationInfoPtr(VK_SHADER_STAGE_CALLABLE_BIT_KHR) );
		}

		if (m_testPtr->usesDynamicStackSize() )
		{
			rayTracingPipelinePtr->addDynamicState(VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR);
		}

		rayTracingPipelinePtr->setMaxRecursionDepth(m_testPtr->getMaxRecursionDepthUsed() );

		pipelineVkPtr = rayTracingPipelinePtr->createPipeline(	deviceInterface,
																deviceVk,
																*pipelineLayoutPtr);
	}

	/* Cache shader stack size info */
	{
		VkDeviceSize ahitShaderStackSize		= 0;
		VkDeviceSize callableShaderStackSize	= 0;
		VkDeviceSize chitShaderStackSize		= 0;
		VkDeviceSize isectShaderStackSize		= 0;
		VkDeviceSize missShaderStackSize		= 0;
		VkDeviceSize raygenShaderStackSize		= 0;

		raygenShaderStackSize = deviceInterface.getRayTracingShaderGroupStackSizeKHR(deviceVk,
																					*pipelineVkPtr,
																					static_cast<deUint32>(ShaderGroups::RAYGEN_GROUP),
																					VK_SHADER_GROUP_SHADER_GENERAL_KHR);

		if (collection.contains("ahit"))
		{
			ahitShaderStackSize = deviceInterface.getRayTracingShaderGroupStackSizeKHR(	deviceVk,
																						*pipelineVkPtr,
																						static_cast<deUint32>(ShaderGroups::HIT_GROUP),
																						VK_SHADER_GROUP_SHADER_ANY_HIT_KHR);
		}

		if (collection.contains("chit") )
		{
			chitShaderStackSize = deviceInterface.getRayTracingShaderGroupStackSizeKHR(	deviceVk,
																						*pipelineVkPtr,
																						static_cast<deUint32>(ShaderGroups::HIT_GROUP),
																						VK_SHADER_GROUP_SHADER_CLOSEST_HIT_KHR);
		}

		if (m_data.geometryType == GeometryType::AABB				||
			m_data.geometryType == GeometryType::AABB_AND_TRIANGLES)
		{
			if (collection.contains("intersection") )
			{
				isectShaderStackSize = deviceInterface.getRayTracingShaderGroupStackSizeKHR(	deviceVk,
																								*pipelineVkPtr,
																								static_cast<deUint32>(ShaderGroups::HIT_GROUP),
																								VK_SHADER_GROUP_SHADER_INTERSECTION_KHR);
			}
		}

		if (nMissGroups > 0u)
		{
			missShaderStackSize = deviceInterface.getRayTracingShaderGroupStackSizeKHR(	deviceVk,
																						*pipelineVkPtr,
																						static_cast<deUint32>(ShaderGroups::MISS_GROUP),
																						VK_SHADER_GROUP_SHADER_GENERAL_KHR);
		}

		for (deUint32 nCallableShader = 0; nCallableShader < static_cast<deUint32>(callableShaderCollectionNames.size() ); ++nCallableShader)
		{
			callableShaderStackSize += deviceInterface.getRayTracingShaderGroupStackSizeKHR(	deviceVk,
																								*pipelineVkPtr,
																								static_cast<deUint32>(ShaderGroups::FIRST_CALLABLE_GROUP) + nCallableShader,
																								VK_SHADER_GROUP_SHADER_GENERAL_KHR);
		}

		m_testPtr->onShaderStackSizeDiscovered(	raygenShaderStackSize,
												ahitShaderStackSize,
												chitShaderStackSize,
												missShaderStackSize,
												callableShaderStackSize,
												isectShaderStackSize);
	}

	auto callableShaderBindingTablePtr = de::MovePtr<BufferWithMemory>();

	if (callableShaderCollectionNames.size() != 0)
	{
		callableShaderBindingTablePtr = rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																							deviceVk,
																							*pipelineVkPtr,
																							allocator,
																							m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																							m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																							static_cast<deUint32>								(ShaderGroups::FIRST_CALLABLE_GROUP),
																							static_cast<deUint32>								(callableShaderCollectionNames.size() ),	/* groupCount                  */
																							0u,																								/* additionalBufferCreateFlags */
																							0u,																								/* additionalBufferUsageFlags  */
																							MemoryRequirement::Any,
																							0u,																								/* opaqueCaptureAddress       */
																							0u,																								/* shaderBindingTableOffset   */
																							m_testPtr->getShaderRecordSize(ShaderGroups::FIRST_CALLABLE_GROUP) );
	}

	const auto	raygenShaderBindingTablePtr	= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																								deviceVk,
																								*pipelineVkPtr,
																								allocator,
																								m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																								m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																								raygenGroupIndex,
																								nRaygenGroups,																					/* groupCount                  */
																								0u,																								/* additionalBufferCreateFlags */
																								0u,																								/* additionalBufferUsageFlags  */
																								MemoryRequirement::Any,
																								0u,																								/* opaqueCaptureAddress        */
																								0u);																							/* shaderBindingTableOffset    */

	auto missShaderBindingTablePtr = de::MovePtr<BufferWithMemory>();
	if (nMissGroups > 0u)
	{
		const void*	missShaderBindingGroupShaderRecordDataPtr	= m_testPtr->getShaderRecordData(					ShaderGroups::MISS_GROUP);
		missShaderBindingTablePtr								= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																													deviceVk,
																													*pipelineVkPtr,
																													allocator,
																													m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																													m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																													missGroupIndex,
																													nMissGroups,																					/* groupCount                  */
																													0u,																								/* additionalBufferCreateFlags */
																													0u,																								/* additionalBufferUsageFlags  */
																													MemoryRequirement::Any,
																													0u,																								/* opaqueCaptureAddress       */
																													0u,																								/* shaderBindingTableOffset   */
																													m_testPtr->getShaderRecordSize(ShaderGroups::MISS_GROUP),
																													&missShaderBindingGroupShaderRecordDataPtr);
	}

	auto hitShaderBindingTablePtr = de::MovePtr<BufferWithMemory>();
	if (nHitGroups > 0u)
	{
		const void*	hitShaderBindingGroupShaderRecordDataPtr	= m_testPtr->getShaderRecordData(					ShaderGroups::HIT_GROUP);
		hitShaderBindingTablePtr								= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																													deviceVk,
																													*pipelineVkPtr,
																													allocator,
																													m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																													m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																													hitGroupIndex,
																													nHitGroups,																						/* groupCount                  */
																													0u,																								/* additionalBufferCreateFlags */
																													0u,																								/* additionalBufferUsageFlags  */
																													MemoryRequirement::Any,
																													0u,																								/* opaqueCaptureAddress       */
																													0u,																								/* shaderBindingTableOffset   */
																													m_testPtr->getShaderRecordSize(ShaderGroups::HIT_GROUP),
																													&hitShaderBindingGroupShaderRecordDataPtr);
	}

	{
		const auto resultBufferCreateInfo	= makeBufferCreateInfo					(	resultBufferSize,
																						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const auto resultBufferDataVec		= m_testPtr->getResultBufferStartData	();

		resultBufferPtr	= de::MovePtr<BufferWithMemory>(
			new BufferWithMemory(	deviceInterface,
									deviceVk,
									allocator,
									resultBufferCreateInfo,
									MemoryRequirement::HostVisible));

		if (resultBufferDataVec.size() > 0)
		{
			DE_ASSERT(static_cast<deUint32>(resultBufferDataVec.size() ) == resultBufferSize);

			memcpy(	resultBufferPtr->getAllocation().getHostPtr(),
					resultBufferDataVec.data(),
					resultBufferDataVec.size() );

			flushAlloc(deviceInterface, deviceVk, resultBufferPtr->getAllocation());
		}

	}

	beginCommandBuffer(	deviceInterface,
						*cmdBufferPtr,
						0u /* flags */);
	{
		m_testPtr->initAS(	m_context,
							m_rayTracingPropsPtr.get(),
							*cmdBufferPtr);

		std::vector<TopLevelAccelerationStructure*> tlasPtrVec = m_testPtr->getTLASPtrVecToBind();
		std::vector<VkAccelerationStructureKHR>		tlasVkVec;

		for (auto& currentTLASPtr : tlasPtrVec)
		{
			tlasVkVec.push_back(*currentTLASPtr->getPtr() );
		}

		if (m_testPtr->getResultBufferStartData().size() == 0)
		{
			deviceInterface.cmdFillBuffer(	*cmdBufferPtr,
											**resultBufferPtr,
											0,					/* dstOffset */
											VK_WHOLE_SIZE,
											0);					/* data */

			{
				const auto postFillBarrier = makeBufferMemoryBarrier(	VK_ACCESS_TRANSFER_WRITE_BIT,	/* srcAccessMask */
																		VK_ACCESS_SHADER_WRITE_BIT,		/* dstAccessMask */
																		**resultBufferPtr,
																		0, /* offset */
																		VK_WHOLE_SIZE);

				cmdPipelineBufferMemoryBarrier(	deviceInterface,
												*cmdBufferPtr,
												VK_PIPELINE_STAGE_TRANSFER_BIT,					/* srcStageMask */
												VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,	/* dstStageMask */
												&postFillBarrier);
			}
		}

		{
			VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet =
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
				DE_NULL,															//  const void*							pNext;
				static_cast<deUint32>(tlasVkVec.size() ),							//  deUint32							accelerationStructureCount;
				tlasVkVec.data(),													//  const VkAccelerationStructureKHR*	pAccelerationStructures;
			};

			const auto descriptorResultBufferInfo = makeDescriptorBufferInfo(	**resultBufferPtr,
																				0, /* offset */
																				resultBufferSize);

			DescriptorSetUpdateBuilder()
				.writeSingle(	*descriptorSetPtr,
								DescriptorSetUpdateBuilder::Location::binding(0u),
								VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								&descriptorResultBufferInfo)
				.writeArray(	*descriptorSetPtr,
								DescriptorSetUpdateBuilder::Location::binding(1u),
								VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								static_cast<deUint32>(tlasVkVec.size() ),
								&accelerationStructureWriteDescriptorSet)
				.update		(	deviceInterface,
								deviceVk);
		}

		deviceInterface.cmdBindDescriptorSets(	*cmdBufferPtr,
												VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
												*pipelineLayoutPtr,
												0, /* firstSet           */
												1, /* descriptorSetCount */
												&descriptorSetPtr.get(),
												0,        /* dynamicOffsetCount */
												DE_NULL); /* pDynamicOffsets    */

		deviceInterface.cmdBindPipeline(*cmdBufferPtr,
										VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
										*pipelineVkPtr);

		{
			const auto preTraceMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,	/* srcAccessMask */
																	VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);	/* dstAccessMask */

			cmdPipelineMemoryBarrier(	deviceInterface,
										*cmdBufferPtr,
										VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,	/* srcStageMask */
										VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,			/* dstStageMask */
										&preTraceMemoryBarrier);
		}

		{
			const auto	nTraceRaysInvocationsNeeded			= m_testPtr->getNTraceRayInvocationsNeeded();
			const auto	handleSize							= m_rayTracingPropsPtr->getShaderGroupHandleSize();
			const auto	missStride							= de::roundUp(handleSize + m_testPtr->getShaderRecordSize(ShaderGroups::MISS_GROUP), handleSize);
			const auto	hitStride							= de::roundUp(handleSize + m_testPtr->getShaderRecordSize(ShaderGroups::HIT_GROUP), handleSize);
			const auto	callStride							= de::roundUp(handleSize + m_testPtr->getShaderRecordSize(ShaderGroups::FIRST_CALLABLE_GROUP), handleSize);
			const auto	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																														deviceVk,
																														raygenShaderBindingTablePtr->get(),
																														0 /* offset */),
																								handleSize,
																								handleSize);
			const auto	missShaderBindingTableRegion		= ((nMissGroups > 0u)	?	makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																																					deviceVk,
																																					missShaderBindingTablePtr->get(),
																																					0 /* offset */),
																															missStride,
																															missStride * nMissGroups)
																					:	makeStridedDeviceAddressRegionKHR(DE_NULL,
																														  0, /* stride */
																														  0  /* size   */));
			const auto	hitShaderBindingTableRegion			= ((nHitGroups > 0u)	?	makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																																					deviceVk,
																																					hitShaderBindingTablePtr->get(),
																																					0 /* offset */),
																															hitStride,
																															hitStride * nHitGroups)
																					:	makeStridedDeviceAddressRegionKHR(DE_NULL,
																														  0, /* stride */
																														  0  /* size   */));

			const auto	callableShaderBindingTableRegion	=	(callableShaderCollectionNames.size() > 0)	? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																																										deviceVk,
																																										callableShaderBindingTablePtr->get(),
																																										0 /* offset */),
																																				callStride, /* stride */
																																				callStride * static_cast<deUint32>(callableShaderCollectionNames.size() ) )
																											: makeStridedDeviceAddressRegionKHR(DE_NULL,
																																				0, /* stride */
																																				0  /* size   */);

			if (m_testPtr->usesDynamicStackSize() )
			{
				deviceInterface.cmdSetRayTracingPipelineStackSizeKHR(	*cmdBufferPtr,
																		m_testPtr->getDynamicStackSize(m_testPtr->getMaxRecursionDepthUsed()) );
			}

			for (deUint32 nInvocation = 0; nInvocation < nTraceRaysInvocationsNeeded; ++nInvocation)
			{
				m_testPtr->onBeforeCmdTraceRays(nInvocation,
												m_context,
												*cmdBufferPtr,
												*pipelineLayoutPtr);

				cmdTraceRays(	deviceInterface,
								*cmdBufferPtr,
								&raygenShaderBindingTableRegion,
								&missShaderBindingTableRegion,
								&hitShaderBindingTableRegion,
								&callableShaderBindingTableRegion,
								m_testPtr->getDispatchSize()[0],
								m_testPtr->getDispatchSize()[1],
								m_testPtr->getDispatchSize()[2]);
			}
		}

		{
			const auto postTraceMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,	/* srcAccessMask */
																	VK_ACCESS_HOST_READ_BIT);	/* dstAccessMask */

			cmdPipelineMemoryBarrier(	deviceInterface,
										*cmdBufferPtr,
										VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,	/* srcStageMask */
										VK_PIPELINE_STAGE_HOST_BIT,						/* dstStageMask */
										&postTraceMemoryBarrier);
		}
	}
	endCommandBuffer(deviceInterface,
					*cmdBufferPtr);

	submitCommandsAndWait(	deviceInterface,
							deviceVk,
							queueVk,
							cmdBufferPtr.get() );

	invalidateMappedMemoryRange(deviceInterface,
								deviceVk,
								resultBufferPtr->getAllocation().getMemory(),
								resultBufferPtr->getAllocation().getOffset(),
								VK_WHOLE_SIZE);

	m_testPtr->resetTLAS();

	return resultBufferPtr;
}

tcu::TestStatus RayTracingMiscTestInstance::iterate (void)
{
	checkSupport();

	const de::MovePtr<BufferWithMemory>	bufferGPUPtr		= runTest();
	const deUint32*						bufferGPUDataPtr	= (deUint32*) bufferGPUPtr->getAllocation().getHostPtr();
	const bool							result				= m_testPtr->verifyResultBuffer(bufferGPUDataPtr);

	if (result)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

void nullMissSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
}

void nullMissPrograms (vk::SourceCollections& programCollection)
{
	const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream rgen;
	std::ostringstream chit;

	rgen
		<< "#version 460\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "layout(location=0) rayPayloadEXT vec3 unused;\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1) buffer OutputBuffer { float val; } outBuffer;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  uint  rayFlags = 0u;\n"
		<< "  uint  cullMask = 0xFFu;\n"
		<< "  float tmin     = 0.0;\n"
		<< "  float tmax     = 9.0;\n"
		<< "  vec3  origin   = vec3(0.0, 0.0, 0.0);\n"
		<< "  vec3  direct   = vec3(0.0, 0.0, 1.0);\n"
		<< "  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
		<< "}\n"
		;

	chit
		<< "#version 460\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "layout(location=0) rayPayloadInEXT vec3 unused;\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1) buffer OutputBuffer { float val; } outBuffer;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  outBuffer.val = 1.0;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;
	programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;
}

// Creates an empty shader binding table with a zeroed-out shader group handle.
de::MovePtr<BufferWithMemory> createEmptySBT (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, deUint32 shaderGroupHandleSize)
{
	const auto sbtSize	= static_cast<VkDeviceSize>(shaderGroupHandleSize);
	const auto sbtFlags	= (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	const auto sbtInfo	= makeBufferCreateInfo(sbtSize, sbtFlags);
	const auto sbtReqs	= (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

	auto	sbtBuffer	= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, alloc, sbtInfo, sbtReqs));
	auto&	sbtAlloc	= sbtBuffer->getAllocation();
	void*	sbtData		= sbtAlloc.getHostPtr();

	deMemset(sbtData, 0, static_cast<size_t>(sbtSize));
	flushAlloc(vkd, device, sbtAlloc);

	return sbtBuffer;
}

tcu::TestStatus nullMissInstance (Context& context)
{
	const auto&	vki		= context.getInstanceInterface();
	const auto	physDev	= context.getPhysicalDevice();
	const auto&	vkd		= context.getDeviceInterface();
	const auto	device	= context.getDevice();
	auto&		alloc	= context.getDefaultAllocator();
	const auto	qIndex	= context.getUniversalQueueFamilyIndex();
	const auto	queue	= context.getUniversalQueue();
	const auto	stages	= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	std::vector<tcu::Vec3> triangle;
	triangle.reserve(3u);
	triangle.emplace_back( 0.0f,  1.0f, 10.0f);
	triangle.emplace_back(-1.0f, -1.0f, 10.0f);
	triangle.emplace_back( 1.0f, -1.0f, 10.0f);
	bottomLevelAS->addGeometry(triangle, true/*triangles*/);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());
	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Create output buffer.
	const auto			bufferSize			= static_cast<VkDeviceSize>(sizeof(float));
	const auto			bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	buffer				(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				bufferAlloc			= buffer.getAllocation();

	// Fill output buffer with an initial value.
	deMemset(bufferAlloc.getHostPtr(), 0, sizeof(float));
	flushAlloc(vkd, device, bufferAlloc);

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);

	const auto setLayout		= setLayoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	{
		const VkWriteDescriptorSetAccelerationStructureKHR accelDescInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			topLevelAS.get()->getPtr(),
		};

		const auto bufferDescInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	auto rgenModule = createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0);
	auto chitModule = createShaderModule(vkd, device, context.getBinaryCollection().get("chit"), 0);

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physDev);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Create raytracing pipeline and shader binding tables.
	Move<VkPipeline>				pipeline;

	de::MovePtr<BufferWithMemory>	raygenSBT;
	de::MovePtr<BufferWithMemory>	missSBT;
	de::MovePtr<BufferWithMemory>	hitSBT;
	de::MovePtr<BufferWithMemory>	callableSBT;

	VkStridedDeviceAddressRegionKHR	raygenSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	missSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callableSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0u);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, 1u);

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenSBT		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);

		hitSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, 1u);
		hitSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);

		// Critical for the test: the miss shader binding table buffer is empty and contains a zero'ed out shader group handle.
		missSBT			= createEmptySBT(vkd, device, alloc, shaderGroupHandleSize);
		missSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0ull), shaderGroupHandleSize, shaderGroupHandleSize);
	}

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, 1u, 1u, 1u);

	// Barrier for the output buffer just in case (no writes should take place).
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read value back from the buffer. No write should have taken place.
	float bufferValue = 0.0f;
	invalidateAlloc(vkd, device, bufferAlloc);
	deMemcpy(&bufferValue, bufferAlloc.getHostPtr(), sizeof(bufferValue));

	if (bufferValue != 0.0f)
		TCU_FAIL("Unexpected value found in buffer: " + de::toString(bufferValue));

	return tcu::TestStatus::pass("Pass");
}

}	// anonymous


class RayTracingTestCase : public TestCase
{
	public:
							 RayTracingTestCase	(	tcu::TestContext&	context,
													const char*			name,
													const char*			desc,
													const CaseDef		data);
							~RayTracingTestCase	(	void);

	virtual void			checkSupport		(Context&			context)			const final;
	virtual TestInstance*	createInstance		(Context&			context)			const final;
	void					initPrograms		(SourceCollections& programCollection)	const final;

private:
	CaseDef								m_data;
	mutable std::unique_ptr<TestBase>	m_testPtr;
};

RayTracingTestCase::RayTracingTestCase (tcu::TestContext&	context,
										const char*			name,
										const char*			desc,
										const CaseDef		data)
	: vkt::TestCase	(	context,
						name,
						desc)
	, m_data		(	data)
{
	/* Stub */
}

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
	context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures	();
	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures		();
	const auto&												rayTracingPipelinePropertiesKHR		= context.getRayTracingPipelineProperties	();

	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
	{
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline is false");
	}

	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
	{
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure is false");
	}

	if (ShaderRecordBlockTest::isTest(m_data.type) )
	{
		if (ShaderRecordBlockTest::isExplicitScalarOffsetTest	(m_data.type) ||
			ShaderRecordBlockTest::isScalarLayoutTest			(m_data.type))
		{
			context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");
		}

		if (ShaderRecordBlockTest::usesF64(m_data.type))
		{
			context.requireDeviceCoreFeature(vkt::DeviceCoreFeature::DEVICE_CORE_FEATURE_SHADER_FLOAT64);
		}

		if (ShaderRecordBlockTest::usesI8(m_data.type) ||
			ShaderRecordBlockTest::usesU8(m_data.type) )
		{
			if (context.get8BitStorageFeatures().storageBuffer8BitAccess == VK_FALSE)
			{
				TCU_THROW(NotSupportedError, "storageBuffer8BitAccess feature is unavailable");
			}
		}

		if (ShaderRecordBlockTest::usesI16(m_data.type) ||
			ShaderRecordBlockTest::usesU16(m_data.type) )
		{
			context.requireDeviceCoreFeature(vkt::DeviceCoreFeature::DEVICE_CORE_FEATURE_SHADER_INT16);
		}

		if (ShaderRecordBlockTest::usesI64(m_data.type) ||
			ShaderRecordBlockTest::usesU64(m_data.type) )
		{
			context.requireDeviceCoreFeature(vkt::DeviceCoreFeature::DEVICE_CORE_FEATURE_SHADER_INT64);
		}
	}

	if (static_cast<deUint32>(m_data.type) >= static_cast<deUint32>(TestType::RECURSIVE_TRACES_1)  &&
		static_cast<deUint32>(m_data.type) <= static_cast<deUint32>(TestType::RECURSIVE_TRACES_29) )
	{
		const auto nLevels = static_cast<deUint32>(m_data.type) - static_cast<deUint32>(TestType::RECURSIVE_TRACES_1) + 1;

		if (rayTracingPipelinePropertiesKHR.maxRayRecursionDepth < nLevels)
		{
			TCU_THROW(NotSupportedError, "Cannot use an unsupported ray recursion depth.");
		}
	}
}

void RayTracingTestCase::initPrograms(SourceCollections& programCollection)	const
{
	switch (m_data.type)
	{
		case TestType::AABBS_AND_TRIS_IN_ONE_TL:
		{
			m_testPtr.reset(
				new AABBTriTLTest(m_data.geometryType, m_data.asLayout)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::AS_STRESS_TEST:
		{
			m_testPtr.reset(
				new ASStressTest(m_data.geometryType, m_data.asLayout)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST:
		case TestType::CALLABLE_SHADER_STRESS_TEST:
		{
			const bool useDynamicStackSize = (m_data.type == TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST);

			m_testPtr.reset(
				new CallableShaderStressTest(m_data.geometryType, m_data.asLayout, useDynamicStackSize)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::CULL_MASK:
		case TestType::CULL_MASK_EXTRA_BITS:
		{
			m_testPtr.reset(
				new CullMaskTest(m_data.asLayout, m_data.geometryType, (m_data.type == TestType::CULL_MASK_EXTRA_BITS) )
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::MAX_RAY_HIT_ATTRIBUTE_SIZE:
		{
			m_testPtr.reset(
				new MAXRayHitAttributeSizeTest(m_data.geometryType, m_data.asLayout)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::MAX_RT_INVOCATIONS_SUPPORTED:
		{
			m_testPtr.reset(
				new MAXRTInvocationsSupportedTest(m_data.geometryType, m_data.asLayout)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::NO_DUPLICATE_ANY_HIT:
		{
			m_testPtr.reset(
				new NoDuplicateAnyHitTest(m_data.asLayout, m_data.geometryType)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::RECURSIVE_TRACES_0:
		case TestType::RECURSIVE_TRACES_1:
		case TestType::RECURSIVE_TRACES_2:
		case TestType::RECURSIVE_TRACES_3:
		case TestType::RECURSIVE_TRACES_4:
		case TestType::RECURSIVE_TRACES_5:
		case TestType::RECURSIVE_TRACES_6:
		case TestType::RECURSIVE_TRACES_7:
		case TestType::RECURSIVE_TRACES_8:
		case TestType::RECURSIVE_TRACES_9:
		case TestType::RECURSIVE_TRACES_10:
		case TestType::RECURSIVE_TRACES_11:
		case TestType::RECURSIVE_TRACES_12:
		case TestType::RECURSIVE_TRACES_13:
		case TestType::RECURSIVE_TRACES_14:
		case TestType::RECURSIVE_TRACES_15:
		case TestType::RECURSIVE_TRACES_16:
		case TestType::RECURSIVE_TRACES_17:
		case TestType::RECURSIVE_TRACES_18:
		case TestType::RECURSIVE_TRACES_19:
		case TestType::RECURSIVE_TRACES_20:
		case TestType::RECURSIVE_TRACES_21:
		case TestType::RECURSIVE_TRACES_22:
		case TestType::RECURSIVE_TRACES_23:
		case TestType::RECURSIVE_TRACES_24:
		case TestType::RECURSIVE_TRACES_25:
		case TestType::RECURSIVE_TRACES_26:
		case TestType::RECURSIVE_TRACES_27:
		case TestType::RECURSIVE_TRACES_28:
		case TestType::RECURSIVE_TRACES_29:
		{
			const auto nLevels	= ((m_data.type == TestType::RECURSIVE_TRACES_0)
								? 0u
								: (static_cast<deUint32>(m_data.type) - static_cast<deUint32>(TestType::RECURSIVE_TRACES_1) + 1));

			m_testPtr.reset(
				new RecursiveTracesTest(m_data.geometryType, m_data.asLayout, nLevels)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::REPORT_INTERSECTION_RESULT:
		{
			m_testPtr.reset(
				new ReportIntersectionResultTest(m_data.asLayout, m_data.geometryType)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::RAY_PAYLOAD_IN:
		{
			m_testPtr.reset(
				new RayPayloadInTest(m_data.geometryType, m_data.asLayout)
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_1:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_2:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_3:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_4:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_5:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_6:
		case TestType::SHADER_RECORD_BLOCK_STD430_1:
		case TestType::SHADER_RECORD_BLOCK_STD430_2:
		case TestType::SHADER_RECORD_BLOCK_STD430_3:
		case TestType::SHADER_RECORD_BLOCK_STD430_4:
		case TestType::SHADER_RECORD_BLOCK_STD430_5:
		case TestType::SHADER_RECORD_BLOCK_STD430_6:
		{
			m_testPtr.reset(
				new ShaderRecordBlockTest(	m_data.type,
											ShaderRecordBlockTest::getVarsToTest(m_data.type) )
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		case TestType::IGNORE_ANY_HIT_DYNAMICALLY:
		case TestType::IGNORE_ANY_HIT_STATICALLY:
		case TestType::TERMINATE_ANY_HIT_DYNAMICALLY:
		case TestType::TERMINATE_ANY_HIT_STATICALLY:
		case TestType::TERMINATE_INTERSECTION_DYNAMICALLY:
		case TestType::TERMINATE_INTERSECTION_STATICALLY:
		{
			m_testPtr.reset(
				new TerminationTest(TerminationTest::getModeFromTestType(m_data.type) )
			);

			m_testPtr->initPrograms(programCollection);

			break;
		}

		default:
		{
			deAssertFail(	"This location should never be reached",
							__FILE__,
							__LINE__);
		}
	}
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	switch (m_data.type)
	{
		case TestType::AABBS_AND_TRIS_IN_ONE_TL:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new AABBTriTLTest(m_data.geometryType, m_data.asLayout)
				);
			}

			break;
		}

		case TestType::AS_STRESS_TEST:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new ASStressTest(m_data.geometryType, m_data.asLayout)
				);
			}

			break;
		}

		case TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST:
		case TestType::CALLABLE_SHADER_STRESS_TEST:
		{
			if (m_testPtr == nullptr)
			{
				const bool useDynamicStackSize = (m_data.type == TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST);

				m_testPtr.reset(
					new CallableShaderStressTest(m_data.geometryType, m_data.asLayout, useDynamicStackSize)
				);
			}

			break;
		}

		case TestType::CULL_MASK:
		case TestType::CULL_MASK_EXTRA_BITS:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new CullMaskTest(m_data.asLayout, m_data.geometryType, (m_data.type == TestType::CULL_MASK_EXTRA_BITS) )
				);
			}

			break;
		}

		case TestType::MAX_RAY_HIT_ATTRIBUTE_SIZE:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new MAXRayHitAttributeSizeTest(m_data.geometryType, m_data.asLayout)
				);
			}

			break;
		}

		case TestType::MAX_RT_INVOCATIONS_SUPPORTED:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new MAXRTInvocationsSupportedTest(m_data.geometryType, m_data.asLayout)
				);
			}

			break;
		}

		case TestType::NO_DUPLICATE_ANY_HIT:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new NoDuplicateAnyHitTest(m_data.asLayout, m_data.geometryType)
				);
			}

			break;
		}

		case TestType::RECURSIVE_TRACES_0:
		case TestType::RECURSIVE_TRACES_1:
		case TestType::RECURSIVE_TRACES_2:
		case TestType::RECURSIVE_TRACES_3:
		case TestType::RECURSIVE_TRACES_4:
		case TestType::RECURSIVE_TRACES_5:
		case TestType::RECURSIVE_TRACES_6:
		case TestType::RECURSIVE_TRACES_7:
		case TestType::RECURSIVE_TRACES_8:
		case TestType::RECURSIVE_TRACES_9:
		case TestType::RECURSIVE_TRACES_10:
		case TestType::RECURSIVE_TRACES_11:
		case TestType::RECURSIVE_TRACES_12:
		case TestType::RECURSIVE_TRACES_13:
		case TestType::RECURSIVE_TRACES_14:
		case TestType::RECURSIVE_TRACES_15:
		case TestType::RECURSIVE_TRACES_16:
		case TestType::RECURSIVE_TRACES_17:
		case TestType::RECURSIVE_TRACES_18:
		case TestType::RECURSIVE_TRACES_19:
		case TestType::RECURSIVE_TRACES_20:
		case TestType::RECURSIVE_TRACES_21:
		case TestType::RECURSIVE_TRACES_22:
		case TestType::RECURSIVE_TRACES_23:
		case TestType::RECURSIVE_TRACES_24:
		case TestType::RECURSIVE_TRACES_25:
		case TestType::RECURSIVE_TRACES_26:
		case TestType::RECURSIVE_TRACES_27:
		case TestType::RECURSIVE_TRACES_28:
		case TestType::RECURSIVE_TRACES_29:
		{
			const auto nLevels	= ((m_data.type == TestType::RECURSIVE_TRACES_0)
								? 0u
								: (static_cast<deUint32>(m_data.type) - static_cast<deUint32>(TestType::RECURSIVE_TRACES_1) + 1));

			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new RecursiveTracesTest(m_data.geometryType, m_data.asLayout, nLevels)
				);
			}

			break;
		}

		case TestType::REPORT_INTERSECTION_RESULT:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new ReportIntersectionResultTest(m_data.asLayout, m_data.geometryType)
				);
			}

			break;
		}

		case TestType::RAY_PAYLOAD_IN:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new RayPayloadInTest(m_data.geometryType, m_data.asLayout)
				);
			}

			break;
		}

		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5:
		case TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_1:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_2:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_3:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_4:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_5:
		case TestType::SHADER_RECORD_BLOCK_SCALAR_6:
		case TestType::SHADER_RECORD_BLOCK_STD430_1:
		case TestType::SHADER_RECORD_BLOCK_STD430_2:
		case TestType::SHADER_RECORD_BLOCK_STD430_3:
		case TestType::SHADER_RECORD_BLOCK_STD430_4:
		case TestType::SHADER_RECORD_BLOCK_STD430_5:
		case TestType::SHADER_RECORD_BLOCK_STD430_6:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new ShaderRecordBlockTest(	m_data.type,
												ShaderRecordBlockTest::getVarsToTest(m_data.type))
				);
			}

			break;
		}

		case TestType::IGNORE_ANY_HIT_DYNAMICALLY:
		case TestType::IGNORE_ANY_HIT_STATICALLY:
		case TestType::TERMINATE_ANY_HIT_DYNAMICALLY:
		case TestType::TERMINATE_ANY_HIT_STATICALLY:
		case TestType::TERMINATE_INTERSECTION_DYNAMICALLY:
		case TestType::TERMINATE_INTERSECTION_STATICALLY:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new TerminationTest(TerminationTest::getModeFromTestType(m_data.type) )
				);
			}

			break;
		}

		default:
		{
			deAssertFail(	"This location should never be reached",
							__FILE__,
							__LINE__);
		}
	}

	auto newTestInstancePtr = new RayTracingMiscTestInstance(	context,
																m_data,
																m_testPtr.get		() );

	return newTestInstancePtr;
}


tcu::TestCaseGroup*	createMiscTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> miscGroupPtr(
		new tcu::TestCaseGroup(
			testCtx,
			"misc",
			"Miscellaneous ray-tracing tests"));


	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		for (auto currentASLayout = AccelerationStructureLayout::FIRST; currentASLayout != AccelerationStructureLayout::COUNT; currentASLayout = static_cast<AccelerationStructureLayout>(static_cast<deUint32>(currentASLayout) + 1) )
		{
			for (deUint32 nIteration = 0; nIteration < 2; ++nIteration)
			{
				const auto			testType		=	(nIteration == 0)	? TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST
																			: TestType::CALLABLE_SHADER_STRESS_TEST;
				const std::string	newTestCaseName =	"callableshaderstress_"														+
														de::toString(getSuffixForASLayout(currentASLayout) )						+
														"_"																			+
														de::toString(getSuffixForGeometryType(currentGeometryType) )				+
														"_"																			+
														((testType == TestType::CALLABLE_SHADER_STRESS_DYNAMIC_TEST)	? "dynamic"
																														: "static");

				auto newTestCasePtr = new RayTracingTestCase(	testCtx,
																newTestCaseName.data(),
																"Verifies that the maximum ray hit attribute size property reported by the implementation is actually supported.",
																CaseDef{testType, currentGeometryType, currentASLayout});

				miscGroupPtr->addChild(newTestCasePtr);
			}
		}
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		const std::string newTestCaseName = "AS_stresstest_" + de::toString(getSuffixForGeometryType(currentGeometryType) );

		auto newTestCasePtr = new RayTracingTestCase(	testCtx,
														newTestCaseName.data(),
														"Verifies raygen shader invocations can simultaneously access as many AS instances as reported",
														CaseDef{TestType::AS_STRESS_TEST, currentGeometryType, AccelerationStructureLayout::ONE_TL_MANY_BLS_ONE_GEOMETRY});

		miscGroupPtr->addChild(newTestCasePtr);
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		for (int nUseExtraCullMaskBits = 0; nUseExtraCullMaskBits < 2 /* false, true */; ++nUseExtraCullMaskBits)
		{
			const std::string	newTestCaseName	= "cullmask_" + de::toString(getSuffixForGeometryType(currentGeometryType) ) + de::toString((nUseExtraCullMaskBits) ? "_extrabits" : "");
			const auto			testType		= (nUseExtraCullMaskBits == 0)	? TestType::CULL_MASK
																				: TestType::CULL_MASK_EXTRA_BITS;

			auto newTestCasePtr = new RayTracingTestCase(	testCtx,
															newTestCaseName.data(),
															"Verifies cull mask works as specified",
															CaseDef{testType, currentGeometryType, AccelerationStructureLayout::ONE_TL_MANY_BLS_ONE_GEOMETRY});

			miscGroupPtr->addChild(newTestCasePtr);
		}
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		const std::string newTestCaseName = "maxrtinvocations_" + de::toString(getSuffixForGeometryType(currentGeometryType) );

		auto newTestCasePtr = new RayTracingTestCase(	testCtx,
														newTestCaseName.data(),
														"Verifies top-level acceleration structures built of AABB and triangle bottom-level AS instances work as expected",
														CaseDef{TestType::MAX_RT_INVOCATIONS_SUPPORTED, currentGeometryType, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});

		miscGroupPtr->addChild(newTestCasePtr);
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		for (auto currentASLayout = AccelerationStructureLayout::FIRST; currentASLayout != AccelerationStructureLayout::COUNT; currentASLayout = static_cast<AccelerationStructureLayout>(static_cast<deUint32>(currentASLayout) + 1) )
		{
			const std::string newTestCaseName = "NO_DUPLICATE_ANY_HIT_" + de::toString(getSuffixForASLayout(currentASLayout) ) + "_" + de::toString(getSuffixForGeometryType(currentGeometryType) );

			auto newTestCasePtr = new RayTracingTestCase(	testCtx,
															newTestCaseName.data(),
															"Verifies the NO_DUPLICATE_ANY_HIT flag is adhered to when tracing rays",
															CaseDef{TestType::NO_DUPLICATE_ANY_HIT, currentGeometryType, currentASLayout});

			miscGroupPtr->addChild(newTestCasePtr);
		}
	}

	{
		auto newTestCasePtr = new RayTracingTestCase(	testCtx,
														"mixedPrimTL",
														"Verifies top-level acceleration structures built of AABB and triangle bottom-level AS instances work as expected",
														CaseDef{TestType::AABBS_AND_TRIS_IN_ONE_TL, GeometryType::AABB_AND_TRIANGLES, AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES_WITH_VARYING_PRIM_TYPES});

		miscGroupPtr->addChild(newTestCasePtr);
	}

	for (auto currentASLayout = AccelerationStructureLayout::FIRST; currentASLayout != AccelerationStructureLayout::COUNT; currentASLayout = static_cast<AccelerationStructureLayout>(static_cast<deUint32>(currentASLayout) + 1) )
	{
		const std::string newTestCaseName = "maxrayhitattributesize_" + de::toString(getSuffixForASLayout(currentASLayout) );

		auto newTestCasePtr = new RayTracingTestCase(	testCtx,
														newTestCaseName.data(),
														"Verifies that the maximum ray hit attribute size property reported by the implementation is actually supported.",
														CaseDef{TestType::MAX_RAY_HIT_ATTRIBUTE_SIZE, GeometryType::AABB, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});

		miscGroupPtr->addChild(newTestCasePtr);
	}

	{
		auto newTestCasePtr = new RayTracingTestCase(testCtx,
														"report_intersection_result",
														"Test the return value of reportIntersectionEXT",
														CaseDef{TestType::REPORT_INTERSECTION_RESULT, GeometryType::AABB, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});
		miscGroupPtr->addChild(newTestCasePtr);
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		const std::string newTestCaseName = "raypayloadin_" + de::toString(getSuffixForGeometryType(currentGeometryType) );

		auto newTestCasePtr = new RayTracingTestCase(	testCtx,
														newTestCaseName.data(),
														"Verifies that relevant shader stages can correctly read large ray payloads provided by raygen shader stage.",
														CaseDef{TestType::RAY_PAYLOAD_IN, currentGeometryType, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});
		miscGroupPtr->addChild(newTestCasePtr);
	}

		{
		auto newTestCaseSTD430_1Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_1",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_1) );
		auto newTestCaseSTD430_2Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_2",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_2) );
		auto newTestCaseSTD430_3Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_3",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_3) );
		auto newTestCaseSTD430_4Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_4",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_4) );
		auto newTestCaseSTD430_5Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_5",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_5) );
		auto newTestCaseSTD430_6Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordSTD430_6",
																"Tests usage of various variables inside a shader record block using std430 layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_STD430_6) );

		auto newTestCaseScalar_1Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_1",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_1) );
		auto newTestCaseScalar_2Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_2",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_2) );
		auto newTestCaseScalar_3Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_3",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_3) );
		auto newTestCaseScalar_4Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_4",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_4) );
		auto newTestCaseScalar_5Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_5",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_5) );
		auto newTestCaseScalar_6Ptr = new RayTracingTestCase(	testCtx,
																"shaderRecordScalar_6",
																"Tests usage of various variables inside a shader record block using scalar layout",
																CaseDef(TestType::SHADER_RECORD_BLOCK_SCALAR_6) );

		auto newTestCaseExplicitScalarOffset_1Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_1",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_1) );
		auto newTestCaseExplicitScalarOffset_2Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_2",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_2) );
		auto newTestCaseExplicitScalarOffset_3Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_3",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_3) );
		auto newTestCaseExplicitScalarOffset_4Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_4",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_4) );
		auto newTestCaseExplicitScalarOffset_5Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_5",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_5) );
		auto newTestCaseExplicitScalarOffset_6Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitScalarOffset_6",
																			"Tests usage of various variables inside a shader record block using scalar layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_SCALAR_OFFSET_6) );

		auto newTestCaseExplicitSTD430Offset_1Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_1",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_1) );
		auto newTestCaseExplicitSTD430Offset_2Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_2",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_2) );
		auto newTestCaseExplicitSTD430Offset_3Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_3",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_3) );
		auto newTestCaseExplicitSTD430Offset_4Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_4",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_4) );
		auto newTestCaseExplicitSTD430Offset_5Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_5",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_5) );
		auto newTestCaseExplicitSTD430Offset_6Ptr = new RayTracingTestCase(	testCtx,
																			"shaderRecordExplicitSTD430Offset_6",
																			"Tests usage of various variables inside a shader record block using std430 layout and explicit offset qualifiers",
																			CaseDef(TestType::SHADER_RECORD_BLOCK_EXPLICIT_STD430_OFFSET_6) );
		miscGroupPtr->addChild(newTestCaseSTD430_1Ptr);
		miscGroupPtr->addChild(newTestCaseSTD430_2Ptr);
		miscGroupPtr->addChild(newTestCaseSTD430_3Ptr);
		miscGroupPtr->addChild(newTestCaseSTD430_4Ptr);
		miscGroupPtr->addChild(newTestCaseSTD430_5Ptr);
		miscGroupPtr->addChild(newTestCaseSTD430_6Ptr);

		miscGroupPtr->addChild(newTestCaseScalar_1Ptr);
		miscGroupPtr->addChild(newTestCaseScalar_2Ptr);
		miscGroupPtr->addChild(newTestCaseScalar_3Ptr);
		miscGroupPtr->addChild(newTestCaseScalar_4Ptr);
		miscGroupPtr->addChild(newTestCaseScalar_5Ptr);
		miscGroupPtr->addChild(newTestCaseScalar_6Ptr);

		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_1Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_2Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_3Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_4Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_5Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitScalarOffset_6Ptr);

		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_1Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_2Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_3Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_4Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_5Ptr);
		miscGroupPtr->addChild(newTestCaseExplicitSTD430Offset_6Ptr);
	}

	for (auto currentGeometryType = GeometryType::FIRST; currentGeometryType != GeometryType::COUNT; currentGeometryType = static_cast<GeometryType>(static_cast<deUint32>(currentGeometryType) + 1) )
	{
		const std::string newTestCaseName = "recursiveTraces_" + de::toString(getSuffixForGeometryType(currentGeometryType) ) + "_";

		// 0 recursion levels.
		{
			auto newTestCasePtr = new RayTracingTestCase(	testCtx,
															(newTestCaseName + "0").data(),
															"Verifies that relevant shader stages can correctly read large ray payloads provided by raygen shader stage.",
															CaseDef{TestType::RECURSIVE_TRACES_0, currentGeometryType, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});

			miscGroupPtr->addChild(newTestCasePtr);
		}

		// TODO: for (deUint32 nLevels = 1; nLevels <= 29; ++nLevels)
		for (deUint32 nLevels = 1; nLevels <= 15; ++nLevels)
		{
			auto newTestCasePtr = new RayTracingTestCase(	testCtx,
															(newTestCaseName + de::toString(nLevels) ).data(),
															"Verifies that relevant shader stages can correctly read large ray payloads provided by raygen shader stage.",
															CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::RECURSIVE_TRACES_1) + (nLevels - 1) ), currentGeometryType, AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY});

			miscGroupPtr->addChild(newTestCasePtr);
		}
	}

	{
		auto newTestCase1Ptr = new RayTracingTestCase(	testCtx,
														"OpIgnoreIntersectionKHR_AnyHitStatically",
														"Verifies that OpIgnoreIntersectionKHR works as per spec (static invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::IGNORE_ANY_HIT_STATICALLY) ), GeometryType::TRIANGLES, AccelerationStructureLayout::COUNT});
		auto newTestCase2Ptr = new RayTracingTestCase(	testCtx,
														"OpIgnoreIntersectionKHR_AnyHitDynamically",
														"Verifies that OpIgnoreIntersectionKHR works as per spec (dynamic invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::IGNORE_ANY_HIT_DYNAMICALLY) ), GeometryType::TRIANGLES, AccelerationStructureLayout::COUNT});
		auto newTestCase3Ptr = new RayTracingTestCase(	testCtx,
														"OpTerminateRayKHR_AnyHitStatically",
														"Verifies that OpTerminateRayKHR works as per spec (static invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::TERMINATE_ANY_HIT_STATICALLY) ), GeometryType::TRIANGLES, AccelerationStructureLayout::COUNT});
		auto newTestCase4Ptr = new RayTracingTestCase(	testCtx,
														"OpTerminateRayKHR_AnyHitDynamically",
														"Verifies that OpTerminateRayKHR works as per spec (dynamic invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::TERMINATE_ANY_HIT_DYNAMICALLY) ), GeometryType::TRIANGLES, AccelerationStructureLayout::COUNT});
		auto newTestCase5Ptr = new RayTracingTestCase(	testCtx,
														"OpTerminateRayKHR_IntersectionStatically",
														"Verifies that OpTerminateRayKHR works as per spec (static invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::TERMINATE_INTERSECTION_STATICALLY) ), GeometryType::AABB, AccelerationStructureLayout::COUNT});
		auto newTestCase6Ptr = new RayTracingTestCase(	testCtx,
														"OpTerminateRayKHR_IntersectionDynamically",
														"Verifies that OpTerminateRayKHR works as per spec (dynamic invocations).",
														CaseDef{static_cast<TestType>(static_cast<deUint32>(TestType::TERMINATE_INTERSECTION_DYNAMICALLY) ), GeometryType::AABB, AccelerationStructureLayout::COUNT});

		miscGroupPtr->addChild(newTestCase1Ptr);
		miscGroupPtr->addChild(newTestCase2Ptr);
		miscGroupPtr->addChild(newTestCase3Ptr);
		miscGroupPtr->addChild(newTestCase4Ptr);
		miscGroupPtr->addChild(newTestCase5Ptr);
		miscGroupPtr->addChild(newTestCase6Ptr);
	}

	{
		addFunctionCaseWithPrograms(miscGroupPtr.get(), "null_miss", "", nullMissSupport, nullMissPrograms, nullMissInstance);
	}

	return miscGroupPtr.release();
}

}	// RayTracing
}	// vkt
