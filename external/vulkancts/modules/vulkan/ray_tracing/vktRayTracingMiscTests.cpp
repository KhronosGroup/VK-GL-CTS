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

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace std;

enum class GeometryType
{
	FIRST = 0,

	AABB		= FIRST,
	TRIANGLES,

	COUNT
};

enum class ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,
	GROUP_COUNT
};

enum class TestType
{
	NO_DUPLICATE_ANY_HIT
};

enum class AccelerationStructureLayout
{
	FIRST = 0,

	ONE_TL_ONE_BL_ONE_GEOMETRY    = FIRST,
	ONE_TL_ONE_BL_MANY_GEOMETRIES,
	ONE_TL_MANY_BLS_ONE_GEOMETRY,
	ONE_TL_MANY_BLS_MANY_GEOMETRIES,

	COUNT
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

	deUint32 nRaysToTrace;
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
																			const VkGeometryFlagsKHR&			bottomLevelGeometryFlags)	const = 0;
	virtual deUint32										getNPrimitives()																const = 0;

};

/* A 3D grid built of primitives. Size and distribution of the geometry can be configured at creation time. */
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
		const auto nCellsNeeded = gridSizeXYZ.x() * gridSizeXYZ.y() * gridSizeXYZ.z();

		for (auto	nCell = 0u;
					nCell < nCellsNeeded;
					nCell++)
		{
			const auto cellX = (((nCell)										% m_gridSizeXYZ.x() ));
			const auto cellY = (((nCell / gridSizeXYZ.x() )						% m_gridSizeXYZ.y() ));
			const auto cellZ = (((nCell / gridSizeXYZ.x() )	/ gridSizeXYZ.y() ) % m_gridSizeXYZ.z() );

			const auto cellX1Y1Z1 = tcu::Vec3(	m_gridStartXYZ.x() + static_cast<float>(cellX) * m_gridInterCellDeltaXYZ.x(),
												m_gridStartXYZ.y() + static_cast<float>(cellY) * m_gridInterCellDeltaXYZ.y(),
												m_gridStartXYZ.z() + static_cast<float>(cellZ) * m_gridInterCellDeltaXYZ.z() );
			const auto cellX2Y2Z2 = tcu::Vec3(	m_gridStartXYZ.x() + static_cast<float>(cellX) * m_gridInterCellDeltaXYZ.x() + m_gridCellSizeXYZ.x(),
												m_gridStartXYZ.y() + static_cast<float>(cellY) * m_gridInterCellDeltaXYZ.y() + m_gridCellSizeXYZ.y(),
												m_gridStartXYZ.z() + static_cast<float>(cellZ) * m_gridInterCellDeltaXYZ.z() + m_gridCellSizeXYZ.z() );

			if (m_geometryType == GeometryType::AABB)
			{
				/* Cell = AABB of the cell */
				m_vertexVec.push_back(cellX1Y1Z1);
				m_vertexVec.push_back(cellX2Y2Z2);
			}
			else
			{
				/* Cell == Plane that spans from top-left-front corner to bottom-right-back corner of the cell */
				const auto A = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX1Y1Z1.y(),
											cellX1Y1Z1.z() );
				const auto B = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX2Y2Z2.y(),
											cellX2Y2Z2.z() );
				const auto C = tcu::Vec3(	cellX1Y1Z1.x(),
											cellX2Y2Z2.y(),
											cellX1Y1Z1.z() );
				const auto D = tcu::Vec3(	cellX2Y2Z2.x(),
											cellX1Y1Z1.y(),
											cellX2Y2Z2.z() );

				m_vertexVec.push_back(A);
				m_vertexVec.push_back(C);
				m_vertexVec.push_back(B);

				m_vertexVec.push_back(A);
				m_vertexVec.push_back(B);
				m_vertexVec.push_back(D);
			}
		}
	}

	std::unique_ptr<TopLevelAccelerationStructure> createTLAS(	Context&							context,
																const AccelerationStructureLayout&	asLayout,
																VkCommandBuffer						cmdBuffer,
																const VkGeometryFlagsKHR&			bottomLevelGeometryFlags) const
	{
		Allocator&										allocator				= context.getDefaultAllocator		();
		const DeviceInterface&							deviceInterface			= context.getDeviceInterface		();
		const VkDevice									deviceVk				= context.getDevice					();
		std::unique_ptr<TopLevelAccelerationStructure>	resultPtr;
		de::MovePtr<TopLevelAccelerationStructure>		tlPtr					= makeTopLevelAccelerationStructure ();

		const auto	nVerticesPerPrimitive	= (m_geometryType == GeometryType::AABB)	? 2u
																						: 3u;

		switch (asLayout)
		{
			case AccelerationStructureLayout::ONE_TL_ONE_BL_ONE_GEOMETRY:
			{
				tlPtr->setInstanceCount(1);

				{
					de::MovePtr<BottomLevelAccelerationStructure> blPtr = makeBottomLevelAccelerationStructure();

					blPtr->setGeometryCount	(1u);
					blPtr->addGeometry		(m_vertexVec,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ) );
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_ONE_BL_MANY_GEOMETRIES:
			{
				DE_ASSERT( (m_vertexVec.size() % nVerticesPerPrimitive) == 0);

				tlPtr->setInstanceCount(1);

				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr		= makeBottomLevelAccelerationStructure();
					const auto										nGeometries = m_vertexVec.size() / nVerticesPerPrimitive;

					blPtr->setGeometryCount	(nGeometries);

					for (deUint32 nGeometry = 0; nGeometry < nGeometries; ++nGeometry)
					{
						std::vector<tcu::Vec3> currentGeometry(nVerticesPerPrimitive);

						for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
						{
							currentGeometry.at(nVertex) = m_vertexVec.at(nGeometry * nVerticesPerPrimitive + nVertex);
						}

						blPtr->addGeometry	(currentGeometry,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);
					}

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ) );
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_MANY_BLS_ONE_GEOMETRY:
			{
				DE_ASSERT( (m_vertexVec.size() % nVerticesPerPrimitive) == 0);

				const auto nInstances = m_vertexVec.size() / nVerticesPerPrimitive;

				tlPtr->setInstanceCount(nInstances);

				for (deUint32 nInstance = 0; nInstance < nInstances; nInstance++)
				{
					de::MovePtr<BottomLevelAccelerationStructure>	blPtr						= makeBottomLevelAccelerationStructure();
					std::vector<tcu::Vec3>							currentInstanceVertexVec;

					for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
					{
						currentInstanceVertexVec.push_back(m_vertexVec.at(nInstance * nVerticesPerPrimitive + nVertex) );
					}

					blPtr->setGeometryCount	(1u);
					blPtr->addGeometry		(currentInstanceVertexVec,
											 (m_geometryType == GeometryType::TRIANGLES),
											 bottomLevelGeometryFlags);

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);

					tlPtr->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ) );
				}

				break;
			}

			case AccelerationStructureLayout::ONE_TL_MANY_BLS_MANY_GEOMETRIES:
			{
				const auto nPrimitivesDefined	= static_cast<deUint32>(m_vertexVec.size() / nVerticesPerPrimitive);
				const auto nPrimitivesPerBLAS	= 4;
				const auto nBottomLevelASes		= nPrimitivesDefined / nPrimitivesPerBLAS;

				DE_ASSERT( (m_vertexVec.size() % nVerticesPerPrimitive)							== 0);
				DE_ASSERT( (nPrimitivesDefined % (nPrimitivesPerBLAS * nVerticesPerPrimitive))	== 0);

				tlPtr->setInstanceCount(nBottomLevelASes);

				for (deUint32 nBottomLevelAS = 0; nBottomLevelAS < nBottomLevelASes; nBottomLevelAS++)
				{
					de::MovePtr<BottomLevelAccelerationStructure> blPtr = makeBottomLevelAccelerationStructure();

					blPtr->setGeometryCount(nPrimitivesPerBLAS);

					for (deUint32 nGeometry = 0; nGeometry < nPrimitivesPerBLAS; nGeometry++)
					{
						std::vector<tcu::Vec3> currentVertexVec;

						for (deUint32 nVertex = 0; nVertex < nVerticesPerPrimitive; ++nVertex)
						{
							currentVertexVec.push_back(m_vertexVec.at((nBottomLevelAS * nPrimitivesPerBLAS + nGeometry) * nVerticesPerPrimitive + nVertex) );
						}

						blPtr->addGeometry(	currentVertexVec,
											(m_geometryType == GeometryType::TRIANGLES),
											bottomLevelGeometryFlags);
					}

					blPtr->createAndBuild(	deviceInterface,
											deviceVk,
											cmdBuffer,
											allocator);
					tlPtr->addInstance(		de::SharedPtr<BottomLevelAccelerationStructure>(blPtr.release() ) );
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

private:
	std::vector<tcu::Vec3> m_vertexVec;

	const GeometryType	m_geometryType;
	const tcu::Vec3		m_gridCellSizeXYZ;
	const tcu::Vec3		m_gridInterCellDeltaXYZ;
	const tcu::UVec3	m_gridSizeXYZ;
	const tcu::Vec3		m_gridStartXYZ;
};


/* Test logic providers ==> */
class TestBase
{
public:
	virtual ~TestBase()
	{
		/* Stub */
	}

	virtual VkGeometryFlagBitsKHR	getBottomLevelGeometryFlags	()										const = 0;
	virtual tcu::UVec3				getDispatchSize				()										const = 0;
	virtual deUint32				getResultBufferSize			()										const = 0;
	virtual void					initPrograms				(SourceCollections& programCollection)	const = 0;
	virtual bool					verifyResultBuffer			(const void*		inBufferPtr)		const = 0;
};

class NoDuplicateAnyHitTest : public TestBase
{
public:
	NoDuplicateAnyHitTest(	const deUint32& nRaysToTrace,
							const deUint32& nTotalPrimitives)
		:m_nRaysToTrace		(nRaysToTrace),
		 m_nTotalPrimitives	(nTotalPrimitives)
	{
		DE_ASSERT(nRaysToTrace		!= 0);
		DE_ASSERT(nTotalPrimitives	!= 0);
	}

	~NoDuplicateAnyHitTest()
	{
		/* Stub */
	}

	VkGeometryFlagBitsKHR getBottomLevelGeometryFlags() const final
	{
		return VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
	}

	tcu::UVec3 getDispatchSize() const final
	{
		return tcu::UVec3(4, 4, m_nRaysToTrace / (4 * 4) + 1);
	}

	deUint32 getResultBufferSize() const final
	{
		return static_cast<deUint32>((1 + 1 + 3 * m_nTotalPrimitives) * sizeof(deUint32) * m_nRaysToTrace);
	}

	void initPrograms(SourceCollections& programCollection) const final
	{
		const vk::ShaderBuildOptions	buildOptions(	programCollection.usedVulkanVersion,
														vk::SPIRV_VERSION_1_4,
														0u,		/* flags        */
														true);	/* allowSpirv14 */

		const auto hitPropertiesDefinition =	"struct HitProperties\n"
												"{\n"
												"    uint nHitsRegistered;\n"
												"	 uint nMissRegistered;\n"
												"    uint instancePrimitiveIDPairsUsed[3 * " + de::toString(m_nTotalPrimitives) + "];\n"
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
				"layout(location = 0) rayPayloadInEXT      dummy { vec4 dummyVec;};\n"
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
				"    vec3  target      = vec3(float(gl_LaunchIDEXT.x * 2) + 1.0f, float(gl_LaunchIDEXT.y * 2) + 1.0f, float(gl_LaunchIDEXT.z * 2) + 1.0f);\n"
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
		bool result = true;

		for (deUint32 nRay = 0; nRay < m_nRaysToTrace; ++nRay)
		{
			std::vector<std::tuple<deUint32, deUint32, deUint32> >	tupleVec;
			const auto												rayProps	= reinterpret_cast<const deUint32*>(resultDataPtr) + (2 + 3 * m_nTotalPrimitives) * nRay;

			// 1. At least one ahit invocation must have been made.
			if (rayProps[0] == 0)
			{
				result = false;

				goto end;
			}

			// 2. It's OK for each ray to intersect many AABBs, but no AABB should have had >1 ahit invocation fired.
			for (deUint32 nPrimitive = 0; nPrimitive < m_nTotalPrimitives; nPrimitive++)
			{
				const auto instanceID    = rayProps[2 + 3 * nPrimitive + 0];
				const auto primitiveID   = rayProps[2 + 3 * nPrimitive + 1];
				const auto geometryIndex = rayProps[2 + 3 * nPrimitive + 2];

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
	const deUint32 m_nRaysToTrace;
	const deUint32 m_nTotalPrimitives;
};


/* Generic misc test instance */
class RayTracingMiscTestInstance : public TestInstance
{
public:
	 RayTracingMiscTestInstance (	Context&				context,
									const CaseDef&			data,
									const ASProviderBase*	asProviderPtr,
									const TestBase*			testPtr);
	~RayTracingMiscTestInstance (	void);

	bool			init	(void);
	tcu::TestStatus	iterate	(void);

protected:
	void							checkSupport(void) const;
	de::MovePtr<BufferWithMemory>	runTest		(void);

private:
	CaseDef	m_data;

	const ASProviderBase*				m_asProviderPtr;
	de::MovePtr<RayTracingProperties>	m_rayTracingPropsPtr;
	const TestBase*						m_testPtr;
};

RayTracingMiscTestInstance::RayTracingMiscTestInstance (Context&					context,
														const CaseDef&				data,
														const ASProviderBase*		asProviderPtr,
														const TestBase*				testPtr)
	: vkt::TestInstance	(context)
	, m_data			(data)
	, m_asProviderPtr	(asProviderPtr)
	, m_testPtr			(testPtr)
{
	DE_ASSERT(m_asProviderPtr != nullptr);
}

RayTracingMiscTestInstance::~RayTracingMiscTestInstance(void)
{
	/* Stub */
}

bool RayTracingMiscTestInstance::init()
{
	const auto& instanceInterface = m_context.getInstanceInterface();
	const auto& physicalDeviceVk  = m_context.getPhysicalDevice   ();

	m_rayTracingPropsPtr = makeRayTracingProperties(instanceInterface,
													physicalDeviceVk);

	return true;
}

de::MovePtr<BufferWithMemory> RayTracingMiscTestInstance::runTest(void)
{
	const DeviceInterface&		deviceInterface		= m_context.getDeviceInterface	();
	const VkDevice				deviceVk			= m_context.getDevice			();

	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue			queueVk				= m_context.getUniversalQueue			();
	Allocator&				allocator			= m_context.getDefaultAllocator			();

	de::MovePtr<BufferWithMemory>					resultBufferPtr;
	const auto										resultBufferSize	= m_testPtr->getResultBufferSize();
	std::unique_ptr<TopLevelAccelerationStructure>	tlPtr;

	const Move<VkDescriptorSetLayout>	descriptorSetLayoutPtr	= DescriptorSetLayoutBuilder()
																	.addSingleBinding(	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
																						ALL_RAY_TRACING_STAGES)
																	.addSingleBinding(	VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
																						ALL_RAY_TRACING_STAGES)
																	.build			(	deviceInterface,
																						deviceVk);

	const Move<VkDescriptorPool>		descriptorPoolPtr		= DescriptorPoolBuilder()
																	.addType(	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																	.addType(	VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																	.build	(	deviceInterface,
																				deviceVk,
																				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
																				1u); /* maxSets */

	const Move<VkDescriptorSet>			descriptorSetPtr		= makeDescriptorSet(deviceInterface,
																					deviceVk,
																					*descriptorPoolPtr,
																					*descriptorSetLayoutPtr);

	const Move<VkPipelineLayout>		pipelineLayoutPtr		= makePipelineLayout(	deviceInterface,
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
		auto& collection = m_context.getBinaryCollection();

		Move<VkShaderModule>	raygenShader		= createShaderModule(	deviceInterface,
																			deviceVk,
																			collection.get("rgen"),
																			0); /* flags */
		Move<VkShaderModule>	hitShader			= createShaderModule(	deviceInterface,
																			deviceVk,
																			collection.get("ahit"),
																			0); /* flags */
		Move<VkShaderModule>	intersectionShader	= createShaderModule(	deviceInterface,
																			deviceVk,
																			collection.get("intersection"),
																			0); /* flags */
		Move<VkShaderModule>	missShader			= createShaderModule(	deviceInterface,
																			deviceVk,
																			collection.get("miss"),
																			0); /* flags */

		rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_RAYGEN_BIT_KHR,
											raygenShader,
											static_cast<deUint32>(ShaderGroups::RAYGEN_GROUP) );
		rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
											hitShader,
											static_cast<deUint32>(ShaderGroups::HIT_GROUP) );
		rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_MISS_BIT_KHR,
											missShader,
											static_cast<deUint32>(ShaderGroups::MISS_GROUP) );

		if (m_data.geometryType == GeometryType::AABB)
		{
			rayTracingPipelinePtr->addShader(	VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
												intersectionShader,
												static_cast<deUint32>(ShaderGroups::HIT_GROUP) );
		}

		pipelineVkPtr = rayTracingPipelinePtr->createPipeline(	deviceInterface,
																deviceVk,
																*pipelineLayoutPtr);
	}

	const auto raygenShaderBindingTablePtr	= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																								deviceVk,
																								*pipelineVkPtr,
																								allocator,
																								m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																								m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																								static_cast<deUint32>								(ShaderGroups::RAYGEN_GROUP),
																								1u); /* groupCount */
	const auto	missShaderBindingTablePtr	= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																								deviceVk,
																								*pipelineVkPtr,
																								allocator,
																								m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																								m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																								static_cast<deUint32>								(ShaderGroups::MISS_GROUP),
																								1u); /* groupCount */
	const auto	hitShaderBindingTablePtr	= rayTracingPipelinePtr->createShaderBindingTable(	deviceInterface,
																								deviceVk,
																								*pipelineVkPtr,
																								allocator,
																								m_rayTracingPropsPtr->getShaderGroupHandleSize		(),
																								m_rayTracingPropsPtr->getShaderGroupBaseAlignment	(),
																								static_cast<deUint32>								(ShaderGroups::HIT_GROUP),
																								1u); /* groupCount */

	{
		const auto resultBufferCreateInfo	= makeBufferCreateInfo(	resultBufferSize,
																	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		resultBufferPtr	= de::MovePtr<BufferWithMemory>(
			new BufferWithMemory(	deviceInterface,
									deviceVk,
									allocator,
									resultBufferCreateInfo,
									MemoryRequirement::HostVisible));
	}

	beginCommandBuffer(	deviceInterface,
						*cmdBufferPtr,
						0u /* flags */);
	{
		tlPtr = m_asProviderPtr->createTLAS(m_context,
											m_data.asLayout,
											*cmdBufferPtr,
											m_testPtr->getBottomLevelGeometryFlags() );

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

		{
			VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet =
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
				DE_NULL,															//  const void*							pNext;
				1u,																	//  deUint32							accelerationStructureCount;
				tlPtr->getPtr(),													//  const VkAccelerationStructureKHR*	pAccelerationStructures;
			};

			const auto descriptorResultBufferInfo = makeDescriptorBufferInfo(	**resultBufferPtr,
																				0, /* offset */
																				resultBufferSize);

			DescriptorSetUpdateBuilder()
				.writeSingle(	*descriptorSetPtr,
								DescriptorSetUpdateBuilder::Location::binding(0u),
								VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								&descriptorResultBufferInfo)
				.writeSingle(	*descriptorSetPtr,
								DescriptorSetUpdateBuilder::Location::binding(1u),
								VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
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
			const auto	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																														deviceVk,
																														raygenShaderBindingTablePtr->get(),
																														0 /* offset */),
																								0, /* stride */
																								m_rayTracingPropsPtr->getShaderGroupHandleSize() );
			const auto	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																														deviceVk,
																														missShaderBindingTablePtr->get(),
																														0 /* offset */),
																								0, /* stride */
																								m_rayTracingPropsPtr->getShaderGroupHandleSize() );
			const auto	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(	deviceInterface,
																														deviceVk,
																														hitShaderBindingTablePtr->get(),
																														0 /* offset */),
																								0, /* stride */
																								m_rayTracingPropsPtr->getShaderGroupHandleSize() );
			const auto	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL,
																								0, /* stride */
																								0  /* size   */);

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
								resultBufferSize);

	return resultBufferPtr;
}

tcu::TestStatus RayTracingMiscTestInstance::iterate (void)
{
	const de::MovePtr<BufferWithMemory>	bufferGPUPtr		= runTest();
	const deUint32*						bufferGPUDataPtr	= (deUint32*) bufferGPUPtr->getAllocation().getHostPtr();
	const bool							result				= m_testPtr->verifyResultBuffer(bufferGPUDataPtr);

	if (result)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
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
	mutable std::unique_ptr<ASProviderBase>	m_asProviderPtr;
	CaseDef									m_data;
	mutable std::unique_ptr<TestBase>		m_testPtr;
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
	switch (m_data.type)
	{
		case TestType::NO_DUPLICATE_ANY_HIT:
		{
			m_asProviderPtr.reset(
				new GridASProvider(	tcu::Vec3	(0,		0,		0),		/* gridStartXYZ          */
									tcu::Vec3	(1,		1,		1),		/* gridCellSizeXYZ       */
									tcu::UVec3	(4,		4,		4),		/* gridSizeXYZ           */
									tcu::Vec3	(2.0f,	2.0f,	2.0f),  /* gridInterCellDeltaXYZ */
									data.geometryType)
			);

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

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures	();
	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures		();

	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
	{
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline is false");
	}

	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
	{
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure is false");
	}
}

void RayTracingTestCase::initPrograms(SourceCollections& programCollection)	const
{
	switch (m_data.type)
	{
		case TestType::NO_DUPLICATE_ANY_HIT:
		{
			DE_ASSERT(m_asProviderPtr != nullptr);

			m_testPtr.reset(
				new NoDuplicateAnyHitTest(	m_data.nRaysToTrace,
											m_asProviderPtr->getNPrimitives() )
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
		case TestType::NO_DUPLICATE_ANY_HIT:
		{
			if (m_testPtr == nullptr)
			{
				m_testPtr.reset(
					new NoDuplicateAnyHitTest(	m_data.nRaysToTrace,
												m_asProviderPtr->getNPrimitives() )
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
																m_asProviderPtr.get	(),
																m_testPtr.get		() );

	newTestInstancePtr->init();

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
			const std::string newTestCaseName = "NO_DUPLICATE_ANY_HIT_" + de::toString(getSuffixForASLayout(currentASLayout) ) + "_" + de::toString(getSuffixForGeometryType(currentGeometryType) );

			auto newTestCasePtr = new RayTracingTestCase(	testCtx,
															newTestCaseName.data(),
															"Verifies the NO_DUPLICATE_ANY_HIT flag is adhered to when tracing rays",
															CaseDef{TestType::NO_DUPLICATE_ANY_HIT, GeometryType::AABB, currentASLayout, 32});

			miscGroupPtr->addChild(newTestCasePtr);
		}
	}

	return miscGroupPtr.release();
}

}	// RayTracing
}	// vkt
