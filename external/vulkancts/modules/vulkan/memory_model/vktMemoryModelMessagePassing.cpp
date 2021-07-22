/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
 * \brief Vulkan Memory Model tests
 *//*--------------------------------------------------------------------*/

#include "vktMemoryModelTests.hpp"
#include "vktMemoryModelPadding.hpp"
#include "vktMemoryModelSharedLayout.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace MemoryModel
{
namespace
{
using namespace vk;
using namespace std;

typedef enum
{
	TT_MP = 0,  // message passing
	TT_WAR, // write-after-read hazard
} TestType;

typedef enum
{
	ST_FENCE_FENCE = 0,
	ST_FENCE_ATOMIC,
	ST_ATOMIC_FENCE,
	ST_ATOMIC_ATOMIC,
	ST_CONTROL_BARRIER,
	ST_CONTROL_AND_MEMORY_BARRIER,
} SyncType;

typedef enum
{
	SC_BUFFER = 0,
	SC_IMAGE,
	SC_WORKGROUP,
	SC_PHYSBUFFER,
} StorageClass;

typedef enum
{
	SCOPE_DEVICE = 0,
	SCOPE_QUEUEFAMILY,
	SCOPE_WORKGROUP,
	SCOPE_SUBGROUP,
} Scope;

typedef enum
{
	STAGE_COMPUTE = 0,
	STAGE_VERTEX,
	STAGE_FRAGMENT,
} Stage;

typedef enum
{
	DATA_TYPE_UINT = 0,
	DATA_TYPE_UINT64,
	DATA_TYPE_FLOAT32,
	DATA_TYPE_FLOAT64,
} DataType;

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
const VkFlags allPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

struct CaseDef
{
	bool payloadMemLocal;
	bool guardMemLocal;
	bool coherent;
	bool core11;
	bool atomicRMW;
	TestType testType;
	StorageClass payloadSC;
	StorageClass guardSC;
	Scope scope;
	SyncType syncType;
	Stage stage;
	DataType dataType;
	bool transitive;
	bool transitiveVis;
};

class MemoryModelTestInstance : public TestInstance
{
public:
						MemoryModelTestInstance	(Context& context, const CaseDef& data);
						~MemoryModelTestInstance	(void);
	tcu::TestStatus		iterate				(void);
private:
	CaseDef			m_data;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

MemoryModelTestInstance::MemoryModelTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

MemoryModelTestInstance::~MemoryModelTestInstance (void)
{
}

class MemoryModelTestCase : public TestCase
{
	public:
								MemoryModelTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~MemoryModelTestCase	(void);
	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual	void				initProgramsTransitive(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance		(Context& context) const;
	virtual void				checkSupport		(Context& context) const;

private:
	CaseDef					m_data;
};

MemoryModelTestCase::MemoryModelTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

MemoryModelTestCase::~MemoryModelTestCase	(void)
{
}

void MemoryModelTestCase::checkSupport(Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(1, 1, 0)))
	{
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
	}

	if (!m_data.core11)
	{
		if (!context.getVulkanMemoryModelFeatures().vulkanMemoryModel)
		{
			TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");
		}

		if (m_data.scope == SCOPE_DEVICE && !context.getVulkanMemoryModelFeatures().vulkanMemoryModelDeviceScope)
		{
			TCU_THROW(NotSupportedError, "vulkanMemoryModelDeviceScope not supported");
		}
	}

	if (m_data.scope == SCOPE_SUBGROUP)
	{
		// Check for subgroup support for scope_subgroup tests.
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;
		subgroupProperties.supportedOperations = 0;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) ||
			!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) ||
			!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup features not supported");
		}

		VkShaderStageFlags stage= VK_SHADER_STAGE_COMPUTE_BIT;
		if (m_data.stage == STAGE_VERTEX)
		{
			stage = VK_SHADER_STAGE_VERTEX_BIT;
		}
		else if (m_data.stage == STAGE_COMPUTE)
		{
			stage = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		else if (m_data.stage == STAGE_FRAGMENT)
		{
			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if((subgroupProperties.supportedStages & stage)==0)
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}
	if (m_data.dataType == DATA_TYPE_UINT64)
	{
		if (!context.getDeviceFeatures().shaderInt64)
		{
			TCU_THROW(NotSupportedError, "64-bit integer in shaders not supported");
		}
		if (!context.getShaderAtomicInt64Features().shaderBufferInt64Atomics &&
			(m_data.guardSC == SC_BUFFER || m_data.guardSC == SC_PHYSBUFFER))
		{
			TCU_THROW(NotSupportedError, "64-bit integer buffer atomics not supported");
		}
		if (!context.getShaderAtomicInt64Features().shaderSharedInt64Atomics &&
			m_data.guardSC == SC_WORKGROUP)
		{
			TCU_THROW(NotSupportedError, "64-bit integer shared atomics not supported");
		}
	}

	if (m_data.dataType == DATA_TYPE_FLOAT32)
	{
		if (!context.isDeviceFunctionalitySupported("VK_EXT_shader_atomic_float"))
			TCU_THROW(NotSupportedError, "Missing extension: VK_EXT_shader_atomic_float");

		if ((m_data.guardSC == SC_BUFFER || m_data.guardSC == SC_PHYSBUFFER) &&
			(!context.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat32Atomics))
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point buffer atomic operations not supported");
		}

		if (m_data.guardSC == SC_IMAGE && (!context.getShaderAtomicFloatFeaturesEXT().shaderImageFloat32Atomics))
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point image atomic operations not supported");
		}

		if (m_data.guardSC == SC_WORKGROUP && (!context.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat32Atomics))
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point shared atomic operations not supported");
		}
	}

	if (m_data.dataType == DATA_TYPE_FLOAT64)
	{
		if (!context.isDeviceFunctionalitySupported("VK_EXT_shader_atomic_float"))
			TCU_THROW(NotSupportedError, "Missing extension: VK_EXT_shader_atomic_float");

		if ((m_data.guardSC == SC_BUFFER || m_data.guardSC == SC_PHYSBUFFER) &&
			(!context.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat64Atomics))
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point buffer atomic operations not supported");
		}

		if (m_data.guardSC == SC_IMAGE || m_data.payloadSC == SC_IMAGE)
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point image atomic operations not supported");
		}

		if (m_data.guardSC == SC_WORKGROUP && (!context.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat64Atomics))
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point shared atomic operations not supported");
		}
	}

	if (m_data.transitive &&
		!context.getVulkanMemoryModelFeatures().vulkanMemoryModelAvailabilityVisibilityChains)
		TCU_THROW(NotSupportedError, "vulkanMemoryModelAvailabilityVisibilityChains not supported");

	if ((m_data.payloadSC == SC_PHYSBUFFER || m_data.guardSC == SC_PHYSBUFFER) && !context.isBufferDeviceAddressSupported())
		TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");

	if (m_data.stage == STAGE_VERTEX)
	{
		if (!context.getDeviceFeatures().vertexPipelineStoresAndAtomics)
		{
			TCU_THROW(NotSupportedError, "vertexPipelineStoresAndAtomics not supported");
		}
	}
	if (m_data.stage == STAGE_FRAGMENT)
	{
		if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
		{
			TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");
		}
	}
}


void MemoryModelTestCase::initPrograms (SourceCollections& programCollection) const
{
	if (m_data.transitive)
	{
		initProgramsTransitive(programCollection);
		return;
	}
	DE_ASSERT(!m_data.transitiveVis);

	Scope invocationMapping = m_data.scope;
	if ((m_data.scope == SCOPE_DEVICE || m_data.scope == SCOPE_QUEUEFAMILY) &&
		(m_data.payloadSC == SC_WORKGROUP || m_data.guardSC == SC_WORKGROUP))
	{
		invocationMapping = SCOPE_WORKGROUP;
	}

	const char *scopeStr;
	switch (m_data.scope)
	{
	default: DE_ASSERT(0); // fall through
	case SCOPE_DEVICE:		scopeStr = "gl_ScopeDevice"; break;
	case SCOPE_QUEUEFAMILY:	scopeStr = "gl_ScopeQueueFamily"; break;
	case SCOPE_WORKGROUP:	scopeStr = "gl_ScopeWorkgroup"; break;
	case SCOPE_SUBGROUP:	scopeStr = "gl_ScopeSubgroup"; break;
	}

	const char *typeStr = (m_data.dataType == DATA_TYPE_UINT64) ? "uint64_t" : (m_data.dataType == DATA_TYPE_FLOAT32) ? "float" :
		(m_data.dataType == DATA_TYPE_FLOAT64) ? "double" : "uint";
	const bool intType = (m_data.dataType == DATA_TYPE_UINT || m_data.dataType == DATA_TYPE_UINT64);

	// Construct storageSemantics strings. Both release and acquire
	// always have the payload storage class. They only include the
	// guard storage class if they're using FENCE for that side of the
	// sync.
	std::stringstream storageSemanticsRelease;
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: // fall through
	case SC_BUFFER:		storageSemanticsRelease << "gl_StorageSemanticsBuffer"; break;
	case SC_IMAGE:		storageSemanticsRelease << "gl_StorageSemanticsImage"; break;
	case SC_WORKGROUP:	storageSemanticsRelease << "gl_StorageSemanticsShared"; break;
	}
	std::stringstream storageSemanticsAcquire;
	storageSemanticsAcquire << storageSemanticsRelease.str();
	if (m_data.syncType == ST_FENCE_ATOMIC || m_data.syncType == ST_FENCE_FENCE)
	{
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		storageSemanticsRelease << " | gl_StorageSemanticsBuffer"; break;
		case SC_IMAGE:		storageSemanticsRelease << " | gl_StorageSemanticsImage"; break;
		case SC_WORKGROUP:	storageSemanticsRelease << " | gl_StorageSemanticsShared"; break;
		}
	}
	if (m_data.syncType == ST_ATOMIC_FENCE || m_data.syncType == ST_FENCE_FENCE)
	{
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		storageSemanticsAcquire << " | gl_StorageSemanticsBuffer"; break;
		case SC_IMAGE:		storageSemanticsAcquire << " | gl_StorageSemanticsImage"; break;
		case SC_WORKGROUP:	storageSemanticsAcquire << " | gl_StorageSemanticsShared"; break;
		}
	}

	std::stringstream semanticsRelease, semanticsAcquire, semanticsAcquireRelease;

	semanticsRelease << "gl_SemanticsRelease";
	semanticsAcquire << "gl_SemanticsAcquire";
	semanticsAcquireRelease << "gl_SemanticsAcquireRelease";
	if (!m_data.coherent && m_data.testType != TT_WAR)
	{
		DE_ASSERT(!m_data.core11);
		semanticsRelease << " | gl_SemanticsMakeAvailable";
		semanticsAcquire << " | gl_SemanticsMakeVisible";
		semanticsAcquireRelease << " | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible";
	}

	std::stringstream css;
	css << "#version 450 core\n";
	if (!m_data.core11)
	{
		css << "#pragma use_vulkan_memory_model\n";
	}
	if (!intType)
	{
		css <<
			"#extension GL_EXT_shader_atomic_float : enable\n"
			"#extension GL_KHR_memory_scope_semantics : enable\n";
	}
	css <<
		"#extension GL_KHR_shader_subgroup_basic : enable\n"
		"#extension GL_KHR_shader_subgroup_shuffle : enable\n"
		"#extension GL_KHR_shader_subgroup_ballot : enable\n"
		"#extension GL_KHR_memory_scope_semantics : enable\n"
		"#extension GL_ARB_gpu_shader_int64 : enable\n"
		"#extension GL_EXT_buffer_reference : enable\n"
		"// DIM/NUM_WORKGROUP_EACH_DIM overriden by spec constants\n"
		"layout(constant_id = 0) const int DIM = 1;\n"
		"layout(constant_id = 1) const int NUM_WORKGROUP_EACH_DIM = 1;\n"
		"struct S { " << typeStr << " x[DIM*DIM]; };\n";

	if (m_data.stage == STAGE_COMPUTE)
	{
		css << "layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;\n";
	}

	const char *memqual = "";
	if (m_data.coherent)
	{
		if (m_data.core11)
		{
			// Vulkan 1.1 only has "coherent", use it regardless of scope
			memqual = "coherent";
		}
		else
		{
			switch (m_data.scope)
			{
			default: DE_ASSERT(0); // fall through
			case SCOPE_DEVICE:		memqual = "devicecoherent"; break;
			case SCOPE_QUEUEFAMILY:	memqual = "queuefamilycoherent"; break;
			case SCOPE_WORKGROUP:	memqual = "workgroupcoherent"; break;
			case SCOPE_SUBGROUP:	memqual = "subgroupcoherent"; break;
			}
		}
	}
	else
	{
		DE_ASSERT(!m_data.core11);
		memqual = "nonprivate";
	}

	stringstream pushConstMembers;

	// Declare payload, guard, and fail resources
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: css << "layout(buffer_reference) buffer PayloadRef { " << typeStr << " x[]; };\n";
						pushConstMembers << "   layout(offset = 0) PayloadRef payloadref;\n"; break;
	case SC_BUFFER:		css << "layout(set=0, binding=0) " << memqual << " buffer Payload { " << typeStr << " x[]; } payload;\n"; break;
	case SC_IMAGE:
		if (intType)
			css << "layout(set=0, binding=0, r32ui) uniform " << memqual << " uimage2D payload;\n";
		else
			css << "layout(set=0, binding=0, r32f) uniform " << memqual << " image2D payload;\n";
		break;
	case SC_WORKGROUP:	css << "shared S payload;\n"; break;
	}
	if (m_data.syncType != ST_CONTROL_AND_MEMORY_BARRIER && m_data.syncType != ST_CONTROL_BARRIER)
	{
		// The guard variable is only accessed with atomics and need not be declared coherent.
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: css << "layout(buffer_reference) buffer GuardRef { " << typeStr << " x[]; };\n";
							pushConstMembers << "layout(offset = 8) GuardRef guard;\n"; break;
		case SC_BUFFER:		css << "layout(set=0, binding=1) buffer Guard { " << typeStr << " x[]; } guard;\n"; break;
		case SC_IMAGE:
			if (intType)
				css << "layout(set=0, binding=1, r32ui) uniform " << memqual << " uimage2D guard;\n";
			else
				css << "layout(set=0, binding=1, r32f) uniform " << memqual << " image2D guard;\n";
			break;
		case SC_WORKGROUP:	css << "shared S guard;\n"; break;
		}
	}

	css << "layout(set=0, binding=2) buffer Fail { uint x[]; } fail;\n";

	if (pushConstMembers.str().size() != 0) {
		css << "layout (push_constant, std430) uniform PC {\n" << pushConstMembers.str() << "};\n";
	}

	css <<
		"void main()\n"
		"{\n"
		"   bool pass = true;\n"
		"   bool skip = false;\n";

	if (m_data.payloadSC == SC_PHYSBUFFER)
		css << "   " << memqual << " PayloadRef payload = payloadref;\n";

	if (m_data.stage == STAGE_FRAGMENT)
	{
		// Kill helper invocations so they don't load outside the bounds of the SSBO.
		// Helper pixels are also initially "active" and if a thread gets one as its
		// partner in SCOPE_SUBGROUP mode, it can't run the test.
		css << "   if (gl_HelperInvocation) { return; }\n";
	}

	// Compute coordinates based on the storage class and scope.
	// For workgroup scope, we pair up LocalInvocationID and DIM-1-LocalInvocationID.
	// For device scope, we pair up GlobalInvocationID and DIM*NUMWORKGROUPS-1-GlobalInvocationID.
	// For subgroup scope, we pair up LocalInvocationID and LocalInvocationID from subgroupId^(subgroupSize-1)
	switch (invocationMapping)
	{
	default: DE_ASSERT(0); // fall through
	case SCOPE_SUBGROUP:
		// If the partner invocation isn't active, the shuffle below will be undefined. Bail.
		css << "   uvec4 ballot = subgroupBallot(true);\n"
			   "   if (!subgroupBallotBitExtract(ballot, gl_SubgroupInvocationID^(gl_SubgroupSize-1))) { return; }\n";

		switch (m_data.stage)
		{
		default: DE_ASSERT(0); // fall through
		case STAGE_COMPUTE:
			css <<
			"   ivec2 localId           = ivec2(gl_LocalInvocationID.xy);\n"
			"   ivec2 partnerLocalId    = subgroupShuffleXor(localId, gl_SubgroupSize-1);\n"
			"   uint sharedCoord        = localId.y * DIM + localId.x;\n"
			"   uint partnerSharedCoord = partnerLocalId.y * DIM + partnerLocalId.x;\n"
			"   uint bufferCoord        = (gl_WorkGroupID.y * NUM_WORKGROUP_EACH_DIM + gl_WorkGroupID.x)*DIM*DIM + sharedCoord;\n"
			"   uint partnerBufferCoord = (gl_WorkGroupID.y * NUM_WORKGROUP_EACH_DIM + gl_WorkGroupID.x)*DIM*DIM + partnerSharedCoord;\n"
			"   ivec2 imageCoord        = ivec2(gl_WorkGroupID.xy * gl_WorkGroupSize.xy + localId);\n"
			"   ivec2 partnerImageCoord = ivec2(gl_WorkGroupID.xy * gl_WorkGroupSize.xy + partnerLocalId);\n";
			break;
		case STAGE_VERTEX:
			css <<
			"   uint bufferCoord        = gl_VertexIndex;\n"
			"   uint partnerBufferCoord = subgroupShuffleXor(gl_VertexIndex, gl_SubgroupSize-1);\n"
			"   ivec2 imageCoord        = ivec2(gl_VertexIndex % (DIM*NUM_WORKGROUP_EACH_DIM), gl_VertexIndex / (DIM*NUM_WORKGROUP_EACH_DIM));\n"
			"   ivec2 partnerImageCoord = subgroupShuffleXor(imageCoord, gl_SubgroupSize-1);\n"
			"   gl_PointSize            = 1.0f;\n"
			"   gl_Position             = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n\n";
			break;
		case STAGE_FRAGMENT:
			css <<
			"   ivec2 localId        = ivec2(gl_FragCoord.xy) % ivec2(DIM);\n"
			"   ivec2 groupId        = ivec2(gl_FragCoord.xy) / ivec2(DIM);\n"
			"   ivec2 partnerLocalId = subgroupShuffleXor(localId, gl_SubgroupSize-1);\n"
			"   ivec2 partnerGroupId = subgroupShuffleXor(groupId, gl_SubgroupSize-1);\n"
			"   uint sharedCoord     = localId.y * DIM + localId.x;\n"
			"   uint partnerSharedCoord = partnerLocalId.y * DIM + partnerLocalId.x;\n"
			"   uint bufferCoord     = (groupId.y * NUM_WORKGROUP_EACH_DIM + groupId.x)*DIM*DIM + sharedCoord;\n"
			"   uint partnerBufferCoord = (partnerGroupId.y * NUM_WORKGROUP_EACH_DIM + partnerGroupId.x)*DIM*DIM + partnerSharedCoord;\n"
			"   ivec2 imageCoord     = ivec2(groupId.xy * ivec2(DIM) + localId);\n"
			"   ivec2 partnerImageCoord = ivec2(partnerGroupId.xy * ivec2(DIM) + partnerLocalId);\n";
			break;
		}
		break;
	case SCOPE_WORKGROUP:
		css <<
		"   ivec2 localId           = ivec2(gl_LocalInvocationID.xy);\n"
		"   ivec2 partnerLocalId    = ivec2(DIM-1)-ivec2(gl_LocalInvocationID.xy);\n"
		"   uint sharedCoord        = localId.y * DIM + localId.x;\n"
		"   uint partnerSharedCoord = partnerLocalId.y * DIM + partnerLocalId.x;\n"
		"   uint bufferCoord        = (gl_WorkGroupID.y * NUM_WORKGROUP_EACH_DIM + gl_WorkGroupID.x)*DIM*DIM + sharedCoord;\n"
		"   uint partnerBufferCoord = (gl_WorkGroupID.y * NUM_WORKGROUP_EACH_DIM + gl_WorkGroupID.x)*DIM*DIM + partnerSharedCoord;\n"
		"   ivec2 imageCoord        = ivec2(gl_WorkGroupID.xy * gl_WorkGroupSize.xy + localId);\n"
		"   ivec2 partnerImageCoord = ivec2(gl_WorkGroupID.xy * gl_WorkGroupSize.xy + partnerLocalId);\n";
		break;
	case SCOPE_QUEUEFAMILY:
	case SCOPE_DEVICE:
		switch (m_data.stage)
		{
		default: DE_ASSERT(0); // fall through
		case STAGE_COMPUTE:
			css <<
			"   ivec2 globalId          = ivec2(gl_GlobalInvocationID.xy);\n"
			"   ivec2 partnerGlobalId   = ivec2(DIM*NUM_WORKGROUP_EACH_DIM-1) - ivec2(gl_GlobalInvocationID.xy);\n"
			"   uint bufferCoord        = globalId.y * DIM*NUM_WORKGROUP_EACH_DIM + globalId.x;\n"
			"   uint partnerBufferCoord = partnerGlobalId.y * DIM*NUM_WORKGROUP_EACH_DIM + partnerGlobalId.x;\n"
			"   ivec2 imageCoord        = globalId;\n"
			"   ivec2 partnerImageCoord = partnerGlobalId;\n";
			break;
		case STAGE_VERTEX:
			css <<
			"   ivec2 globalId          = ivec2(gl_VertexIndex % (DIM*NUM_WORKGROUP_EACH_DIM), gl_VertexIndex / (DIM*NUM_WORKGROUP_EACH_DIM));\n"
			"   ivec2 partnerGlobalId   = ivec2(DIM*NUM_WORKGROUP_EACH_DIM-1) - globalId;\n"
			"   uint bufferCoord        = globalId.y * DIM*NUM_WORKGROUP_EACH_DIM + globalId.x;\n"
			"   uint partnerBufferCoord = partnerGlobalId.y * DIM*NUM_WORKGROUP_EACH_DIM + partnerGlobalId.x;\n"
			"   ivec2 imageCoord        = globalId;\n"
			"   ivec2 partnerImageCoord = partnerGlobalId;\n"
			"   gl_PointSize            = 1.0f;\n"
			"   gl_Position             = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n\n";
			break;
		case STAGE_FRAGMENT:
			css <<
			"   ivec2 localId       = ivec2(gl_FragCoord.xy) % ivec2(DIM);\n"
			"   ivec2 groupId       = ivec2(gl_FragCoord.xy) / ivec2(DIM);\n"
			"   ivec2 partnerLocalId = ivec2(DIM-1)-localId;\n"
			"   ivec2 partnerGroupId = groupId;\n"
			"   uint sharedCoord    = localId.y * DIM + localId.x;\n"
			"   uint partnerSharedCoord = partnerLocalId.y * DIM + partnerLocalId.x;\n"
			"   uint bufferCoord    = (groupId.y * NUM_WORKGROUP_EACH_DIM + groupId.x)*DIM*DIM + sharedCoord;\n"
			"   uint partnerBufferCoord = (partnerGroupId.y * NUM_WORKGROUP_EACH_DIM + partnerGroupId.x)*DIM*DIM + partnerSharedCoord;\n"
			"   ivec2 imageCoord    = ivec2(groupId.xy * ivec2(DIM) + localId);\n"
			"   ivec2 partnerImageCoord = ivec2(partnerGroupId.xy * ivec2(DIM) + partnerLocalId);\n";
			break;
		}
		break;
	}

	// Initialize shared memory, followed by a barrier
	if (m_data.payloadSC == SC_WORKGROUP)
	{
		css << "   payload.x[sharedCoord] = 0;\n";
	}
	if (m_data.guardSC == SC_WORKGROUP)
	{
		css << "   guard.x[sharedCoord] = 0;\n";
	}
	if (m_data.payloadSC == SC_WORKGROUP || m_data.guardSC == SC_WORKGROUP)
	{
		switch (invocationMapping)
		{
		default: DE_ASSERT(0); // fall through
		case SCOPE_SUBGROUP:	css << "   subgroupBarrier();\n"; break;
		case SCOPE_WORKGROUP:	css << "   barrier();\n"; break;
		}
	}

	if (m_data.testType == TT_MP)
	{
		if (intType)
		{
			// Store payload
			switch (m_data.payloadSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER:		css << "   payload.x[bufferCoord] = bufferCoord + (payload.x[partnerBufferCoord]>>31);\n"; break;
			case SC_IMAGE:		css << "   imageStore(payload, imageCoord, uvec4(bufferCoord + (imageLoad(payload, partnerImageCoord).x>>31), 0, 0, 0));\n"; break;
			case SC_WORKGROUP:	css << "   payload.x[sharedCoord] = bufferCoord + (payload.x[partnerSharedCoord]>>31);\n"; break;
			}
		}
		else
		{
			// Store payload
			switch (m_data.payloadSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER:		css << "   payload.x[bufferCoord] = " << typeStr << "(bufferCoord) + ((floatBitsToInt(float(payload.x[partnerBufferCoord])))>>31);\n"; break;
			case SC_IMAGE:		css << "   imageStore(payload, imageCoord, vec4(" << typeStr << "(bufferCoord + (floatBitsToInt(float(imageLoad(payload, partnerImageCoord).x))>>31)), 0, 0, 0)); \n"; break;
			case SC_WORKGROUP:	css << "   payload.x[sharedCoord] = " << typeStr << "(bufferCoord) + ((floatBitsToInt(float(payload.x[partnerSharedCoord])))>>31);\n"; break;
			}
		}
	}
	else
	{
		DE_ASSERT(m_data.testType == TT_WAR);
		// Load payload
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "   " << typeStr << " r = payload.x[partnerBufferCoord];\n"; break;
		case SC_IMAGE:		css << "   " << typeStr << " r = imageLoad(payload, partnerImageCoord).x;\n"; break;
		case SC_WORKGROUP:	css << "   " << typeStr << " r = payload.x[partnerSharedCoord];\n"; break;
		}
	}
	if (m_data.syncType == ST_CONTROL_AND_MEMORY_BARRIER)
	{
		// Acquire and release separate from control barrier
		css << "   memoryBarrier(" << scopeStr << ", " << storageSemanticsRelease.str() << ", " << semanticsRelease.str() << ");\n"
			   "   controlBarrier(" << scopeStr << ", gl_ScopeInvocation, 0, 0);\n"
			   "   memoryBarrier(" << scopeStr << ", " << storageSemanticsAcquire.str() << ", " << semanticsAcquire.str() << ");\n";
	}
	else if (m_data.syncType == ST_CONTROL_BARRIER)
	{
		// Control barrier performs both acquire and release
		css << "   controlBarrier(" << scopeStr << ", " << scopeStr << ", "
									<< storageSemanticsRelease.str() << " | " << storageSemanticsAcquire.str() << ", "
									<< semanticsAcquireRelease.str() << ");\n";
	}
	else
	{
		// Don't type cast for 64 bit image atomics
		const char* typeCastStr = (m_data.dataType == DATA_TYPE_UINT64 || m_data.dataType == DATA_TYPE_FLOAT64) ? "" : typeStr;
		// Release barrier
		std::stringstream atomicReleaseSemantics;
		if (m_data.syncType == ST_FENCE_ATOMIC || m_data.syncType == ST_FENCE_FENCE)
		{
			css << "   memoryBarrier(" << scopeStr << ", " << storageSemanticsRelease.str() << ", " << semanticsRelease.str() << ");\n";
			atomicReleaseSemantics << ", 0, 0";
		}
		else
		{
			atomicReleaseSemantics << ", " << storageSemanticsRelease.str() << ", " << semanticsRelease.str();
		}
		// Atomic store guard
		if (m_data.atomicRMW)
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER:		css << "   atomicExchange(guard.x[bufferCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_IMAGE:		css << "   imageAtomicExchange(guard, imageCoord, " << typeCastStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_WORKGROUP:	css << "   atomicExchange(guard.x[sharedCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			}
		}
		else
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER:		css << "   atomicStore(guard.x[bufferCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_IMAGE:		css << "   imageAtomicStore(guard, imageCoord, " << typeCastStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_WORKGROUP:	css << "   atomicStore(guard.x[sharedCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			}
		}

		std::stringstream atomicAcquireSemantics;
		if (m_data.syncType == ST_ATOMIC_FENCE || m_data.syncType == ST_FENCE_FENCE)
		{
			atomicAcquireSemantics << ", 0, 0";
		}
		else
		{
			atomicAcquireSemantics << ", " << storageSemanticsAcquire.str() << ", " << semanticsAcquire.str();
		}
		// Atomic load guard
		if (m_data.atomicRMW)
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER: css << "   skip = atomicExchange(guard.x[partnerBufferCoord], " << typeStr << "(2u), " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_IMAGE:  css << "   skip = imageAtomicExchange(guard, partnerImageCoord, " << typeCastStr << "(2u), " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_WORKGROUP: css << "   skip = atomicExchange(guard.x[partnerSharedCoord], " << typeStr << "(2u), " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			}
		} else
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_PHYSBUFFER: // fall through
			case SC_BUFFER:		css << "   skip = atomicLoad(guard.x[partnerBufferCoord], " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_IMAGE:		css << "   skip = imageAtomicLoad(guard, partnerImageCoord, " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_WORKGROUP:	css << "   skip = atomicLoad(guard.x[partnerSharedCoord], " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			}
		}
		// Acquire barrier
		if (m_data.syncType == ST_ATOMIC_FENCE || m_data.syncType == ST_FENCE_FENCE)
		{
			css << "   memoryBarrier(" << scopeStr << ", " << storageSemanticsAcquire.str() << ", " << semanticsAcquire.str() << ");\n";
		}
	}
	if (m_data.testType == TT_MP)
	{
		// Load payload
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "   " << typeStr << " r = payload.x[partnerBufferCoord];\n"; break;
		case SC_IMAGE:		css << "   " << typeStr << " r = imageLoad(payload, partnerImageCoord).x;\n"; break;
		case SC_WORKGROUP:	css << "   " << typeStr << " r = payload.x[partnerSharedCoord];\n"; break;
		}
		css <<
			"   if (!skip && r != " << typeStr << "(partnerBufferCoord)) { fail.x[bufferCoord] = 1; }\n"
			"}\n";
	}
	else
	{
		DE_ASSERT(m_data.testType == TT_WAR);
		// Store payload, only if the partner invocation has already done its read
		css << "   if (!skip) {\n   ";
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "   payload.x[bufferCoord] = " << typeStr << "(bufferCoord);\n"; break;
		case SC_IMAGE:
			if (intType) {
				css << "   imageStore(payload, imageCoord, uvec4(bufferCoord, 0, 0, 0));\n";
			}
			else {
				css << "   imageStore(payload, imageCoord, vec4(" << typeStr << "(bufferCoord), 0, 0, 0));\n";
			}
			break;
		case SC_WORKGROUP:	css << "   payload.x[sharedCoord] = " << typeStr << "(bufferCoord);\n"; break;
		}
		css <<
			"   }\n"
			"   if (r != 0) { fail.x[bufferCoord] = 1; }\n"
			"}\n";
	}

	// Draw a fullscreen triangle strip based on gl_VertexIndex
	std::stringstream vss;
	vss <<
		"#version 450 core\n"
		"vec2 coords[4] = {ivec2(-1,-1), ivec2(-1, 1), ivec2(1, -1), ivec2(1, 1)};\n"
		"void main() { gl_Position = vec4(coords[gl_VertexIndex], 0, 1); }\n";

	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	switch (m_data.stage)
	{
	default: DE_ASSERT(0); // fall through
	case STAGE_COMPUTE:
		programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
		break;
	case STAGE_VERTEX:
		programCollection.glslSources.add("test") << glu::VertexSource(css.str()) << buildOptions;
		break;
	case STAGE_FRAGMENT:
		programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());
		programCollection.glslSources.add("test") << glu::FragmentSource(css.str()) << buildOptions;
		break;
	}
}


void MemoryModelTestCase::initProgramsTransitive (SourceCollections& programCollection) const
{
	Scope invocationMapping = m_data.scope;

	const char* typeStr = (m_data.dataType == DATA_TYPE_UINT64) ? "uint64_t" : (m_data.dataType == DATA_TYPE_FLOAT32) ? "float" :
		(m_data.dataType == DATA_TYPE_FLOAT64) ? "double" : "uint";
	const bool intType = (m_data.dataType == DATA_TYPE_UINT || m_data.dataType == DATA_TYPE_UINT64);

	// Construct storageSemantics strings. Both release and acquire
	// always have the payload storage class. They only include the
	// guard storage class if they're using FENCE for that side of the
	// sync.
	std::stringstream storageSemanticsPayload;
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: // fall through
	case SC_BUFFER:		storageSemanticsPayload << "gl_StorageSemanticsBuffer"; break;
	case SC_IMAGE:		storageSemanticsPayload << "gl_StorageSemanticsImage"; break;
	}
	std::stringstream storageSemanticsGuard;
	switch (m_data.guardSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: // fall through
	case SC_BUFFER:		storageSemanticsGuard << "gl_StorageSemanticsBuffer"; break;
	case SC_IMAGE:		storageSemanticsGuard << "gl_StorageSemanticsImage"; break;
	}
	std::stringstream storageSemanticsAll;
	storageSemanticsAll << storageSemanticsPayload.str() << " | " << storageSemanticsGuard.str();

	std::stringstream css;
	css << "#version 450 core\n";
	css << "#pragma use_vulkan_memory_model\n";
	if (!intType)
	{
		css <<
			"#extension GL_EXT_shader_atomic_float : enable\n"
			"#extension GL_KHR_memory_scope_semantics : enable\n";
	}
	css <<
		"#extension GL_KHR_shader_subgroup_basic : enable\n"
		"#extension GL_KHR_shader_subgroup_shuffle : enable\n"
		"#extension GL_KHR_shader_subgroup_ballot : enable\n"
		"#extension GL_KHR_memory_scope_semantics : enable\n"
		"#extension GL_ARB_gpu_shader_int64 : enable\n"
		"#extension GL_EXT_buffer_reference : enable\n"
		"// DIM/NUM_WORKGROUP_EACH_DIM overriden by spec constants\n"
		"layout(constant_id = 0) const int DIM = 1;\n"
		"layout(constant_id = 1) const int NUM_WORKGROUP_EACH_DIM = 1;\n"
		"shared bool sharedSkip;\n";

	css << "layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;\n";

	const char *memqual = "";
	const char *semAvail = "";
	const char *semVis = "";
	if (m_data.coherent)
	{
		memqual = "workgroupcoherent";
	}
	else
	{
		memqual = "nonprivate";
		semAvail = " | gl_SemanticsMakeAvailable";
		semVis = " | gl_SemanticsMakeVisible";
	}

	stringstream pushConstMembers;

	// Declare payload, guard, and fail resources
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: css << "layout(buffer_reference) buffer PayloadRef { " << typeStr << " x[]; };\n";
						pushConstMembers << "   layout(offset = 0) PayloadRef payloadref;\n"; break;
	case SC_BUFFER:		css << "layout(set=0, binding=0) " << memqual << " buffer Payload { " << typeStr << " x[]; } payload;\n"; break;
	case SC_IMAGE:
		if (intType)
			css << "layout(set=0, binding=0, r32ui) uniform " << memqual << " uimage2D payload;\n";
		else
			css << "layout(set=0, binding=0, r32f) uniform " << memqual << " image2D payload;\n";
		break;
	}
	// The guard variable is only accessed with atomics and need not be declared coherent.
	switch (m_data.guardSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: css << "layout(buffer_reference) buffer GuardRef { " << typeStr << " x[]; };\n";
						pushConstMembers << "layout(offset = 8) GuardRef guard;\n"; break;
	case SC_BUFFER:		css << "layout(set=0, binding=1) buffer Guard { " << typeStr << " x[]; } guard;\n"; break;
	case SC_IMAGE:
		if (intType)
			css << "layout(set=0, binding=1, r32ui) uniform " << memqual << " uimage2D guard;\n";
		else
			css << "layout(set=0, binding=1, r32f) uniform " << memqual << " image2D guard;\n";
		break;
	}

	css << "layout(set=0, binding=2) buffer Fail { uint x[]; } fail;\n";

	if (pushConstMembers.str().size() != 0) {
		css << "layout (push_constant, std430) uniform PC {\n" << pushConstMembers.str() << "};\n";
	}

	css <<
		"void main()\n"
		"{\n"
		"   bool pass = true;\n"
		"   bool skip = false;\n"
		"   sharedSkip = false;\n";

	if (m_data.payloadSC == SC_PHYSBUFFER)
		css << "   " << memqual << " PayloadRef payload = payloadref;\n";

	// Compute coordinates based on the storage class and scope.
	switch (invocationMapping)
	{
	default: DE_ASSERT(0); // fall through
	case SCOPE_DEVICE:
		css <<
		"   ivec2 globalId          = ivec2(gl_GlobalInvocationID.xy);\n"
		"   ivec2 partnerGlobalId   = ivec2(DIM*NUM_WORKGROUP_EACH_DIM-1) - ivec2(gl_GlobalInvocationID.xy);\n"
		"   uint bufferCoord        = globalId.y * DIM*NUM_WORKGROUP_EACH_DIM + globalId.x;\n"
		"   uint partnerBufferCoord = partnerGlobalId.y * DIM*NUM_WORKGROUP_EACH_DIM + partnerGlobalId.x;\n"
		"   ivec2 imageCoord        = globalId;\n"
		"   ivec2 partnerImageCoord = partnerGlobalId;\n"
		"   ivec2 globalId00          = ivec2(DIM) * ivec2(gl_WorkGroupID.xy);\n"
		"   ivec2 partnerGlobalId00   = ivec2(DIM) * (ivec2(NUM_WORKGROUP_EACH_DIM-1) - ivec2(gl_WorkGroupID.xy));\n"
		"   uint bufferCoord00        = globalId00.y * DIM*NUM_WORKGROUP_EACH_DIM + globalId00.x;\n"
		"   uint partnerBufferCoord00 = partnerGlobalId00.y * DIM*NUM_WORKGROUP_EACH_DIM + partnerGlobalId00.x;\n"
		"   ivec2 imageCoord00        = globalId00;\n"
		"   ivec2 partnerImageCoord00 = partnerGlobalId00;\n";
		break;
	}

	// Store payload
	if (intType)
	{
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "   payload.x[bufferCoord] = bufferCoord + (payload.x[partnerBufferCoord]>>31);\n"; break;
		case SC_IMAGE:		css << "   imageStore(payload, imageCoord, uvec4(bufferCoord + (imageLoad(payload, partnerImageCoord).x>>31), 0, 0, 0));\n"; break;
		}
	}
	else
	{
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:	css << "   payload.x[bufferCoord] = " << typeStr << "(bufferCoord) + ((floatBitsToInt(float(payload.x[partnerBufferCoord])))>>31);\n"; break;
		case SC_IMAGE:	css << "   imageStore(payload, imageCoord, vec4(" << typeStr << "(bufferCoord + (floatBitsToInt(float(imageLoad(payload, partnerImageCoord).x)>>31))), 0, 0, 0)); \n"; break;
		}
	}

	// Sync to other threads in the workgroup
	css << "   controlBarrier(gl_ScopeWorkgroup, "
							 "gl_ScopeWorkgroup, " <<
							  storageSemanticsPayload.str() << " | gl_StorageSemanticsShared, "
							 "gl_SemanticsAcquireRelease" << semAvail << ");\n";

	// Device-scope release/availability in invocation(0,0)
	css << "   if (all(equal(gl_LocalInvocationID.xy, ivec2(0,0)))) {\n";
	const char* typeCastStr = (m_data.dataType == DATA_TYPE_UINT64 || m_data.dataType == DATA_TYPE_FLOAT64) ? "" : typeStr;
	if (m_data.syncType == ST_ATOMIC_ATOMIC || m_data.syncType == ST_ATOMIC_FENCE) {
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "       atomicStore(guard.x[bufferCoord], " << typeStr << "(1u), gl_ScopeDevice, " << storageSemanticsPayload.str() << ", gl_SemanticsRelease | gl_SemanticsMakeAvailable);\n"; break;
		case SC_IMAGE:		css << "       imageAtomicStore(guard, imageCoord, " << typeCastStr << "(1u), gl_ScopeDevice, " << storageSemanticsPayload.str() << ", gl_SemanticsRelease | gl_SemanticsMakeAvailable);\n"; break;
		}
	} else {
		css << "       memoryBarrier(gl_ScopeDevice, " << storageSemanticsAll.str() << ", gl_SemanticsRelease | gl_SemanticsMakeAvailable);\n";
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "       atomicStore(guard.x[bufferCoord], " << typeStr << "(1u), gl_ScopeDevice, 0, 0);\n"; break;
		case SC_IMAGE:		css << "       imageAtomicStore(guard, imageCoord, " << typeCastStr << "(1u), gl_ScopeDevice, 0, 0);\n"; break;
		}
	}

	// Device-scope acquire/visibility either in invocation(0,0) or in every invocation
	if (!m_data.transitiveVis) {
		css << "   }\n";
	}
	if (m_data.syncType == ST_ATOMIC_ATOMIC || m_data.syncType == ST_FENCE_ATOMIC) {
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "       skip = atomicLoad(guard.x[partnerBufferCoord00], gl_ScopeDevice, " << storageSemanticsPayload.str() << ", gl_SemanticsAcquire | gl_SemanticsMakeVisible) == 0;\n"; break;
		case SC_IMAGE:		css << "       skip = imageAtomicLoad(guard, partnerImageCoord00, gl_ScopeDevice, " << storageSemanticsPayload.str() << ", gl_SemanticsAcquire | gl_SemanticsMakeVisible) == 0;\n"; break;
		}
	} else {
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_PHYSBUFFER: // fall through
		case SC_BUFFER:		css << "       skip = atomicLoad(guard.x[partnerBufferCoord00], gl_ScopeDevice, 0, 0) == 0;\n"; break;
		case SC_IMAGE:		css << "       skip = imageAtomicLoad(guard, partnerImageCoord00, gl_ScopeDevice, 0, 0) == 0;\n"; break;
		}
		css << "       memoryBarrier(gl_ScopeDevice, " << storageSemanticsAll.str() << ", gl_SemanticsAcquire | gl_SemanticsMakeVisible);\n";
	}

	// If invocation(0,0) did the acquire then store "skip" to shared memory and
	// synchronize with the workgroup
	if (m_data.transitiveVis) {
		css << "       sharedSkip = skip;\n";
		css << "   }\n";

		css << "   controlBarrier(gl_ScopeWorkgroup, "
								 "gl_ScopeWorkgroup, " <<
								  storageSemanticsPayload.str() << " | gl_StorageSemanticsShared, "
								 "gl_SemanticsAcquireRelease" << semVis << ");\n";
		css << "   skip = sharedSkip;\n";
	}

	// Load payload
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER: // fall through
	case SC_BUFFER:		css << "   " << typeStr << " r = payload.x[partnerBufferCoord];\n"; break;
	case SC_IMAGE:		css << "   " << typeStr << " r = imageLoad(payload, partnerImageCoord).x;\n"; break;
	}
	css <<
		"   if (!skip && r != " << typeStr << "(partnerBufferCoord)) { fail.x[bufferCoord] = 1; }\n"
		"}\n";

	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
}

TestInstance* MemoryModelTestCase::createInstance (Context& context) const
{
	return new MemoryModelTestInstance(context, m_data);
}

tcu::TestStatus MemoryModelTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = NULL;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties);

	deUint32 DIM = 31;
	deUint32 NUM_WORKGROUP_EACH_DIM = 8;
	// If necessary, shrink workgroup size to fit HW limits
	if (DIM*DIM > properties.properties.limits.maxComputeWorkGroupInvocations)
	{
		DIM = (deUint32)deFloatSqrt((float)properties.properties.limits.maxComputeWorkGroupInvocations);
	}
	deUint32 NUM_INVOCATIONS = (DIM * DIM * NUM_WORKGROUP_EACH_DIM * NUM_WORKGROUP_EACH_DIM);

	VkDeviceSize bufferSizes[3];
	de::MovePtr<BufferWithMemory> buffers[3];
	vk::VkDescriptorBufferInfo bufferDescriptors[3];
	de::MovePtr<BufferWithMemory> copyBuffer;

	for (deUint32 i = 0; i < 3; ++i)
	{
		size_t elementSize = (m_data.dataType == DATA_TYPE_UINT64 || m_data.dataType == DATA_TYPE_FLOAT64)? sizeof(deUint64) : sizeof(deUint32);
		// buffer2 is the "fail" buffer, and is always uint
		if (i == 2)
			elementSize = sizeof(deUint32);
		bufferSizes[i] = NUM_INVOCATIONS * elementSize;

		vk::VkFlags usageFlags = vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		bool memoryDeviceAddress = false;

		bool local;
		switch (i)
		{
		default: DE_ASSERT(0); // fall through
		case 0:
			if (m_data.payloadSC != SC_BUFFER && m_data.payloadSC != SC_PHYSBUFFER)
				continue;
			local = m_data.payloadMemLocal;
			if (m_data.payloadSC == SC_PHYSBUFFER)
			{
				usageFlags |= vk::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
				if (m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
					memoryDeviceAddress = true;
			}
			break;
		case 1:
			if (m_data.guardSC != SC_BUFFER && m_data.guardSC != SC_PHYSBUFFER)
				continue;
			local = m_data.guardMemLocal;
			if (m_data.guardSC == SC_PHYSBUFFER)
			{
				usageFlags |= vk::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
				if (m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
					memoryDeviceAddress = true;
			}
			break;
		case 2: local = true; break;
		}

		try
		{
			buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
				vk, device, allocator, makeBufferCreateInfo(bufferSizes[i], usageFlags),
				(memoryDeviceAddress ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any) |
				(local ? MemoryRequirement::Local : MemoryRequirement::NonLocal)));
		}
		catch (const tcu::NotSupportedError&)
		{
			if (!local)
			{
				TCU_THROW(NotSupportedError, "Test variant uses non-device-local memory, which is not supported");
			}
			throw;
		}
		bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, bufferSizes[i]);
	}

	// Try to use cached host memory for the buffer the CPU will read from, else fallback to host visible.
	try
	{
		copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(bufferSizes[2], VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible | MemoryRequirement::Cached));
	}
	catch (const tcu::NotSupportedError&)
	{
		copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(bufferSizes[2], VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));
	}

	VkFormat imageFormat;
	switch (m_data.dataType)
	{
	case DATA_TYPE_UINT:
	case DATA_TYPE_UINT64:
		imageFormat = VK_FORMAT_R32_UINT;
		break;
	case DATA_TYPE_FLOAT32:
	case DATA_TYPE_FLOAT64:
		imageFormat = VK_FORMAT_R32_SFLOAT;
		break;
	default:
		TCU_FAIL("Invalid data type.");
	}

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		imageFormat,							// VkFormat				format;
		{
			DIM*NUM_WORKGROUP_EACH_DIM,	// deUint32	width;
			DIM*NUM_WORKGROUP_EACH_DIM,	// deUint32	height;
			1u		// deUint32	depth;
		},										// VkExtent3D			   extent;
		1u,										// deUint32				 mipLevels;
		1u,										// deUint32				 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32				 queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		  pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};
	VkImageViewCreateInfo		imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	 flags;
		DE_NULL,									// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		imageFormat,								// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,	// VkComponentSwizzle	r;
			VK_COMPONENT_SWIZZLE_G,	// VkComponentSwizzle	g;
			VK_COMPONENT_SWIZZLE_B,	// VkComponentSwizzle	b;
			VK_COMPONENT_SWIZZLE_A	// VkComponentSwizzle	a;
		},											// VkComponentMapping		 components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32			  baseMipLevel;
			1u,							// deUint32			  levelCount;
			0u,							// deUint32			  baseArrayLayer;
			1u							// deUint32			  layerCount;
		}											// VkImageSubresourceRange	subresourceRange;
	};


	de::MovePtr<ImageWithMemory> images[2];
	Move<VkImageView> imageViews[2];
	vk::VkDescriptorImageInfo imageDescriptors[2];

	for (deUint32 i = 0; i < 2; ++i)
	{

		bool local;
		switch (i)
		{
		default: DE_ASSERT(0); // fall through
		case 0:
			if (m_data.payloadSC != SC_IMAGE)
				continue;
			local = m_data.payloadMemLocal;
			break;
		case 1:
			if (m_data.guardSC != SC_IMAGE)
				continue;
			local = m_data.guardMemLocal;
			break;
		}

		try
		{
			images[i] = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
				vk, device, allocator, imageCreateInfo, local ? MemoryRequirement::Local : MemoryRequirement::NonLocal));
		}
		catch (const tcu::NotSupportedError&)
		{
			if (!local)
			{
				TCU_THROW(NotSupportedError, "Test variant uses non-device-local memory, which is not supported");
			}
			throw;
		}
		imageViewCreateInfo.image = **images[i];
		imageViews[i] = createImageView(vk, device, &imageViewCreateInfo, NULL);

		imageDescriptors[i] = makeDescriptorImageInfo(DE_NULL, *imageViews[i], VK_IMAGE_LAYOUT_GENERAL);
	}

	vk::DescriptorSetLayoutBuilder layoutBuilder;

	switch (m_data.payloadSC)
	{
	default:
	case SC_BUFFER:	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages); break;
	case SC_IMAGE:	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, allShaderStages); break;
	}
	switch (m_data.guardSC)
	{
	default:
	case SC_BUFFER:	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages); break;
	case SC_IMAGE:	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, allShaderStages); break;
	}
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));

	vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	vk::DescriptorSetUpdateBuilder setUpdateBuilder;
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER:
	case SC_WORKGROUP:
		break;
	case SC_BUFFER:
		setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
		break;
	case SC_IMAGE:
		setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptors[0]);
		break;
	}
	switch (m_data.guardSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_PHYSBUFFER:
	case SC_WORKGROUP:
		break;
	case SC_BUFFER:
		setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
		break;
	case SC_IMAGE:
		setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptors[1]);
		break;
	}
	setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);

	setUpdateBuilder.update(vk, device);

	const VkPushConstantRange pushConstRange =
	{
		allShaderStages,		// VkShaderStageFlags	stageFlags
		0,						// deUint32				offset
		16						// deUint32				size
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		1,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		1u,															// pushConstantRangeCount
		&pushConstRange,											// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	Move<VkPipeline> pipeline;
	Move<VkRenderPass> renderPass;
	Move<VkFramebuffer> framebuffer;

	VkPipelineBindPoint bindPoint = m_data.stage == STAGE_COMPUTE ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	const deUint32 specData[2] = {DIM, NUM_WORKGROUP_EACH_DIM};

	const vk::VkSpecializationMapEntry entries[3] =
	{
		{0, sizeof(deUint32) * 0, sizeof(deUint32)},
		{1, sizeof(deUint32) * 1, sizeof(deUint32)},
	};

	const vk::VkSpecializationInfo specInfo =
	{
		2,						// mapEntryCount
		entries,				// pMapEntries
		sizeof(specData),		// dataSize
		specData				// pData
	};

	if (m_data.stage == STAGE_COMPUTE)
	{
		const Unique<VkShaderModule>	shader						(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,								// stage
			*shader,													// shader
			"main",
			&specInfo,													// pSpecializationInfo
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
			shaderCreateInfo,											// cs
			*pipelineLayout,											// layout
			(vk::VkPipeline)0,											// basePipelineHandle
			0u,															// basePipelineIndex
		};
		pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);
	}
	else
	{

		const vk::VkSubpassDescription		subpassDesc			=
		{
			(vk::VkSubpassDescriptionFlags)0,
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
			0u,														// inputCount
			DE_NULL,												// pInputAttachments
			0u,														// colorCount
			DE_NULL,												// pColorAttachments
			DE_NULL,												// pResolveAttachments
			DE_NULL,												// depthStencilAttachment
			0u,														// preserveCount
			DE_NULL,												// pPreserveAttachments

		};
		const vk::VkRenderPassCreateInfo	renderPassParams	=
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// sType
			DE_NULL,												// pNext
			(vk::VkRenderPassCreateFlags)0,
			0u,														// attachmentCount
			DE_NULL,												// pAttachments
			1u,														// subpassCount
			&subpassDesc,											// pSubpasses
			0u,														// dependencyCount
			DE_NULL,												// pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassParams);

		const vk::VkFramebufferCreateInfo	framebufferParams	=
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
			DE_NULL,										// pNext
			(vk::VkFramebufferCreateFlags)0,
			*renderPass,									// renderPass
			0u,												// attachmentCount
			DE_NULL,										// pAttachments
			DIM*NUM_WORKGROUP_EACH_DIM,						// width
			DIM*NUM_WORKGROUP_EACH_DIM,						// height
			1u,												// layers
		};

		framebuffer = createFramebuffer(vk, device, &framebufferParams);

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
			0u,															// deUint32									vertexBindingDescriptionCount;
			DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,															// deUint32									vertexAttributeDescriptionCount;
			DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			(m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology						topology;
			VK_FALSE														// VkBool32									primitiveRestartEnable;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClampEnable;
			(m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE,			// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType						  sType
			DE_NULL,													// const void*							  pNext
			0u,															// VkPipelineMultisampleStateCreateFlags	flags
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,													// VkBool32								 sampleShadingEnable
			1.0f,														// float									minSampleShading
			DE_NULL,													// const VkSampleMask*					  pSampleMask
			VK_FALSE,													// VkBool32								 alphaToCoverageEnable
			VK_FALSE													// VkBool32								 alphaToOneEnable
		};

		VkViewport viewport = makeViewport(DIM*NUM_WORKGROUP_EACH_DIM, DIM*NUM_WORKGROUP_EACH_DIM);
		VkRect2D scissor = makeRect2D(DIM*NUM_WORKGROUP_EACH_DIM, DIM*NUM_WORKGROUP_EACH_DIM);

		const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType							 sType
			DE_NULL,												// const void*								 pNext
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags		  flags
			1u,														// deUint32									viewportCount
			&viewport,												// const VkViewport*						   pViewports
			1u,														// deUint32									scissorCount
			&scissor												// const VkRect2D*							 pScissors
		};

		Move<VkShaderModule> fs;
		Move<VkShaderModule> vs;

		deUint32 numStages;
		if (m_data.stage == STAGE_VERTEX)
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0); // bogus
			numStages = 1u;
		}
		else
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			numStages = 2u;
		}

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo[2] = {
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,									// stage
				*vs,														// shader
				"main",
				&specInfo,													// pSpecializationInfo
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,								// stage
				*fs,														// shader
				"main",
				&specInfo,													// pSpecializationInfo
			}
		};

		const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			numStages,											// deUint32											stageCount;
			&shaderCreateInfo[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),								// VkPipelineLayout									layout;
			renderPass.get(),									// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			DE_NULL,											// VkPipeline										basePipelineHandle;
			0													// int												basePipelineIndex;
		};

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);
	}

	const VkQueue					queue					= m_context.getUniversalQueue();
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkBufferDeviceAddressInfo addrInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	// VkStructureType	sType;
			DE_NULL,										// const void*		 pNext;
			0,												// VkBuffer			buffer
		};

	VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	VkClearValue clearColor = makeClearValueColorU32(0,0,0,0);

	VkMemoryBarrier					memBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
			DE_NULL,							// pNext
			0u,									// srcAccessMask
			0u,									// dstAccessMask
		};

	const VkBufferCopy	copyParams =
		{
			(VkDeviceSize)0u,						// srcOffset
			(VkDeviceSize)0u,						// dstOffset
			bufferSizes[2]							// size
		};

	deUint32 NUM_SUBMITS = 4;

	for (deUint32 x = 0; x < NUM_SUBMITS; ++x)
	{
		beginCommandBuffer(vk, *cmdBuffer, 0u);

		if (x == 0)
			vk.cmdFillBuffer(*cmdBuffer, **buffers[2], 0, bufferSizes[2], 0);

		for (deUint32 i = 0; i < 2; ++i)
		{
			if (!images[i])
				continue;

			const VkImageMemoryBarrier imageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
				DE_NULL,											// const void*			pNext
				0u,													// VkAccessFlags		srcAccessMask
				VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
				**images[i],										// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					1u,										// uint32_t				mipLevels,
					0u,										// uint32_t				baseArray
					1u,										// uint32_t				arraySize
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
								 (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  0, (const VkBufferMemoryBarrier*)DE_NULL,
								  1, &imageBarrier);
		}

		vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

		if (m_data.payloadSC == SC_PHYSBUFFER)
		{
			const bool useKHR = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");
			addrInfo.buffer = **buffers[0];
			VkDeviceAddress addr;
			if (useKHR)
				addr = vk.getBufferDeviceAddress(device, &addrInfo);
			else
				addr = vk.getBufferDeviceAddressEXT(device, &addrInfo);
			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages,
								0, sizeof(VkDeviceSize), &addr);
		}
		if (m_data.guardSC == SC_PHYSBUFFER)
		{
			const bool useKHR = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");
			addrInfo.buffer = **buffers[1];
			VkDeviceAddress addr;
			if (useKHR)
				addr = vk.getBufferDeviceAddress(device, &addrInfo);
			else
				addr = vk.getBufferDeviceAddressEXT(device, &addrInfo);
			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages,
								8, sizeof(VkDeviceSize), &addr);
		}

		for (deUint32 iters = 0; iters < 50; ++iters)
		{
			for (deUint32 i = 0; i < 2; ++i)
			{
				if (buffers[i])
					vk.cmdFillBuffer(*cmdBuffer, **buffers[i], 0, bufferSizes[i], 0);
				if (images[i])
					vk.cmdClearColorImage(*cmdBuffer, **images[i], VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
			}

			memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

			if (m_data.stage == STAGE_COMPUTE)
			{
				vk.cmdDispatch(*cmdBuffer, NUM_WORKGROUP_EACH_DIM, NUM_WORKGROUP_EACH_DIM, 1);
			}
			else
			{
				beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
								makeRect2D(DIM*NUM_WORKGROUP_EACH_DIM, DIM*NUM_WORKGROUP_EACH_DIM),
								0, DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
				// Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
				if (m_data.stage == STAGE_VERTEX)
				{
					vk.cmdDraw(*cmdBuffer, DIM*DIM*NUM_WORKGROUP_EACH_DIM*NUM_WORKGROUP_EACH_DIM, 1u, 0u, 0u);
				}
				else
				{
					vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
				}
				endRenderPass(vk, *cmdBuffer);
			}

			memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
		}

		if (x == NUM_SUBMITS - 1)
		{
			vk.cmdCopyBuffer(*cmdBuffer, **buffers[2], **copyBuffer, 1, &copyParams);
			memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		vk.resetCommandBuffer(*cmdBuffer, 0x00000000);
	}

	tcu::TestLog& log = m_context.getTestContext().getLog();

	deUint32 *ptr = (deUint32 *)copyBuffer->getAllocation().getHostPtr();
	invalidateAlloc(vk, device, copyBuffer->getAllocation());
	qpTestResult res = QP_TEST_RESULT_PASS;

	deUint32 numErrors = 0;
	for (deUint32 i = 0; i < NUM_INVOCATIONS; ++i)
	{
		if (ptr[i] != 0)
		{
			if (numErrors < 256)
			{
				log << tcu::TestLog::Message << "Failed invocation: " << i << tcu::TestLog::EndMessage;
			}
			numErrors++;
			res = QP_TEST_RESULT_FAIL;
		}
	}

	if (numErrors)
	{
		log << tcu::TestLog::Message << "Total Errors: " << numErrors << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "memory_model", "Memory model tests"));

	typedef struct
	{
		deUint32				value;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	TestGroupCase ttCases[] =
	{
		{ TT_MP,	"message_passing",	"message passing"		},
		{ TT_WAR,	"write_after_read",	"write after read"		},
	};

	TestGroupCase core11Cases[] =
	{
		{ 1,	"core11",	"Supported by Vulkan1.1"							},
		{ 0,	"ext",		"Requires VK_KHR_vulkan_memory_model extension"		},
	};

	TestGroupCase dtCases[] =
	{
		{ DATA_TYPE_UINT,		"u32",	"uint32_t atomics"		},
		{ DATA_TYPE_UINT64,		"u64",	"uint64_t atomics"		},
		{ DATA_TYPE_FLOAT32,	"f32",	"float32 atomics"		},
		{ DATA_TYPE_FLOAT64,	"f64",	"float64 atomics"		},
	};

	TestGroupCase cohCases[] =
	{
		{ 1,	"coherent",		"coherent payload variable"			},
		{ 0,	"noncoherent",	"noncoherent payload variable"		},
	};

	TestGroupCase stCases[] =
	{
		{ ST_FENCE_FENCE,					"fence_fence",					"release fence, acquire fence"			},
		{ ST_FENCE_ATOMIC,					"fence_atomic",					"release fence, atomic acquire"			},
		{ ST_ATOMIC_FENCE,					"atomic_fence",					"atomic release, acquire fence"			},
		{ ST_ATOMIC_ATOMIC,					"atomic_atomic",				"atomic release, atomic acquire"		},
		{ ST_CONTROL_BARRIER,				"control_barrier",				"control barrier"						},
		{ ST_CONTROL_AND_MEMORY_BARRIER,	"control_and_memory_barrier",	"control barrier with release/acquire"	},
	};

	TestGroupCase rmwCases[] =
	{
		{ 0,	"atomicwrite",		"atomic write"		},
		{ 1,	"atomicrmw",		"atomic rmw"		},
	};

	TestGroupCase scopeCases[] =
	{
		{ SCOPE_DEVICE,			"device",		"device scope"			},
		{ SCOPE_QUEUEFAMILY,	"queuefamily",	"queuefamily scope"		},
		{ SCOPE_WORKGROUP,		"workgroup",	"workgroup scope"		},
		{ SCOPE_SUBGROUP,		"subgroup",		"subgroup scope"		},
	};

	TestGroupCase plCases[] =
	{
		{ 0,	"payload_nonlocal",		"payload variable in non-local memory"		},
		{ 1,	"payload_local",		"payload variable in local memory"			},
	};

	TestGroupCase pscCases[] =
	{
		{ SC_BUFFER,	"buffer",		"payload variable in buffer memory"			},
		{ SC_IMAGE,		"image",		"payload variable in image memory"			},
		{ SC_WORKGROUP,	"workgroup",	"payload variable in workgroup memory"		},
		{ SC_PHYSBUFFER,"physbuffer",	"payload variable in physical storage buffer memory"	},
	};

	TestGroupCase glCases[] =
	{
		{ 0,	"guard_nonlocal",		"guard variable in non-local memory"		},
		{ 1,	"guard_local",			"guard variable in local memory"			},
	};

	TestGroupCase gscCases[] =
	{
		{ SC_BUFFER,	"buffer",		"guard variable in buffer memory"			},
		{ SC_IMAGE,		"image",		"guard variable in image memory"			},
		{ SC_WORKGROUP,	"workgroup",	"guard variable in workgroup memory"		},
		{ SC_PHYSBUFFER,"physbuffer",	"guard variable in physical storage buffer memory"	},
	};

	TestGroupCase stageCases[] =
	{
		{ STAGE_COMPUTE,	"comp",		"compute shader"			},
		{ STAGE_VERTEX,		"vert",		"vertex shader"				},
		{ STAGE_FRAGMENT,	"frag",		"fragment shader"			},
	};


	for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
		for (int core11Ndx = 0; core11Ndx < DE_LENGTH_OF_ARRAY(core11Cases); core11Ndx++)
		{
			de::MovePtr<tcu::TestCaseGroup> core11Group(new tcu::TestCaseGroup(testCtx, core11Cases[core11Ndx].name, core11Cases[core11Ndx].description));
			for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
				for (int cohNdx = 0; cohNdx < DE_LENGTH_OF_ARRAY(cohCases); cohNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> cohGroup(new tcu::TestCaseGroup(testCtx, cohCases[cohNdx].name, cohCases[cohNdx].description));
					for (int stNdx = 0; stNdx < DE_LENGTH_OF_ARRAY(stCases); stNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> stGroup(new tcu::TestCaseGroup(testCtx, stCases[stNdx].name, stCases[stNdx].description));
						for (int rmwNdx = 0; rmwNdx < DE_LENGTH_OF_ARRAY(rmwCases); rmwNdx++)
						{
							de::MovePtr<tcu::TestCaseGroup> rmwGroup(new tcu::TestCaseGroup(testCtx, rmwCases[rmwNdx].name, rmwCases[rmwNdx].description));
							for (int scopeNdx = 0; scopeNdx < DE_LENGTH_OF_ARRAY(scopeCases); scopeNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> scopeGroup(new tcu::TestCaseGroup(testCtx, scopeCases[scopeNdx].name, scopeCases[scopeNdx].description));
								for (int plNdx = 0; plNdx < DE_LENGTH_OF_ARRAY(plCases); plNdx++)
								{
									de::MovePtr<tcu::TestCaseGroup> plGroup(new tcu::TestCaseGroup(testCtx, plCases[plNdx].name, plCases[plNdx].description));
									for (int pscNdx = 0; pscNdx < DE_LENGTH_OF_ARRAY(pscCases); pscNdx++)
									{
										de::MovePtr<tcu::TestCaseGroup> pscGroup(new tcu::TestCaseGroup(testCtx, pscCases[pscNdx].name, pscCases[pscNdx].description));
										for (int glNdx = 0; glNdx < DE_LENGTH_OF_ARRAY(glCases); glNdx++)
										{
											de::MovePtr<tcu::TestCaseGroup> glGroup(new tcu::TestCaseGroup(testCtx, glCases[glNdx].name, glCases[glNdx].description));
											for (int gscNdx = 0; gscNdx < DE_LENGTH_OF_ARRAY(gscCases); gscNdx++)
											{
												de::MovePtr<tcu::TestCaseGroup> gscGroup(new tcu::TestCaseGroup(testCtx, gscCases[gscNdx].name, gscCases[gscNdx].description));
												for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
												{
													CaseDef c =
													{
														!!plCases[plNdx].value,					// bool payloadMemLocal;
														!!glCases[glNdx].value,					// bool guardMemLocal;
														!!cohCases[cohNdx].value,				// bool coherent;
														!!core11Cases[core11Ndx].value,			// bool core11;
														!!rmwCases[rmwNdx].value,				// bool atomicRMW;
														(TestType)ttCases[ttNdx].value,			// TestType testType;
														(StorageClass)pscCases[pscNdx].value,	// StorageClass payloadSC;
														(StorageClass)gscCases[gscNdx].value,	// StorageClass guardSC;
														(Scope)scopeCases[scopeNdx].value,		// Scope scope;
														(SyncType)stCases[stNdx].value,			// SyncType syncType;
														(Stage)stageCases[stageNdx].value,		// Stage stage;
														(DataType)dtCases[dtNdx].value,			// DataType dataType;
														false,									// bool transitive;
														false,									// bool transitiveVis;
													};

													// Mustpass11 tests should only exercise things we expect to work on
													// existing implementations. Exclude noncoherent tests which require
													// new extensions, and assume atomic synchronization wouldn't work
													// (i.e. atomics may be implemented as relaxed atomics). Exclude
													// queuefamily scope which doesn't exist in Vulkan 1.1. Exclude
													// physical storage buffer which doesn't support the legacy decorations.
													if (c.core11 &&
														(c.coherent == 0 ||
														c.syncType == ST_FENCE_ATOMIC ||
														c.syncType == ST_ATOMIC_FENCE ||
														c.syncType == ST_ATOMIC_ATOMIC ||
														c.dataType == DATA_TYPE_UINT64 ||
														c.dataType == DATA_TYPE_FLOAT64 ||
														c.scope == SCOPE_QUEUEFAMILY ||
														c.payloadSC == SC_PHYSBUFFER ||
														c.guardSC == SC_PHYSBUFFER))
													{
														continue;
													}

													if (c.stage != STAGE_COMPUTE &&
														c.scope == SCOPE_WORKGROUP)
													{
														continue;
													}

													// Don't exercise local and non-local for workgroup memory
													// Also don't exercise workgroup memory for non-compute stages
													if (c.payloadSC == SC_WORKGROUP && (c.payloadMemLocal != 0 || c.stage != STAGE_COMPUTE))
													{
														continue;
													}
													if (c.guardSC == SC_WORKGROUP && (c.guardMemLocal != 0 || c.stage != STAGE_COMPUTE))
													{
														continue;
													}
													// Can't do control barrier with larger than workgroup scope, or non-compute stages
													if ((c.syncType == ST_CONTROL_BARRIER || c.syncType == ST_CONTROL_AND_MEMORY_BARRIER) &&
														(c.scope == SCOPE_DEVICE || c.scope == SCOPE_QUEUEFAMILY || c.stage != STAGE_COMPUTE))
													{
														continue;
													}

													// Limit RMW atomics to ST_ATOMIC_ATOMIC, just to reduce # of test cases
													if (c.atomicRMW && c.syncType != ST_ATOMIC_ATOMIC)
													{
														continue;
													}

													// uint64/float32/float64 testing is primarily for atomics, so only test it for ST_ATOMIC_ATOMIC
													const bool atomicTesting = (c.dataType == DATA_TYPE_UINT64 || c.dataType == DATA_TYPE_FLOAT32 || c.dataType == DATA_TYPE_FLOAT64);
													if (atomicTesting && c.syncType != ST_ATOMIC_ATOMIC)
													{
														continue;
													}

													// No 64-bit image types, so skip tests with both payload and guard in image memory
													if (c.dataType == DATA_TYPE_UINT64 && c.payloadSC == SC_IMAGE && c.guardSC == SC_IMAGE)
													{
														continue;
													}

													// No support for atomic operations on 64-bit floating point images
													if (c.dataType == DATA_TYPE_FLOAT64 && (c.payloadSC == SC_IMAGE || c.guardSC == SC_IMAGE))
													{
														continue;
													}
													// Control barrier tests don't use a guard variable, so only run them with gsc,gl==0
													if ((c.syncType == ST_CONTROL_BARRIER || c.syncType == ST_CONTROL_AND_MEMORY_BARRIER) &&
														(c.guardSC != 0 || c.guardMemLocal != 0))
													{
														continue;
													}

													gscGroup->addChild(new MemoryModelTestCase(testCtx, stageCases[stageNdx].name, stageCases[stageNdx].description, c));
												}
												glGroup->addChild(gscGroup.release());
											}
											pscGroup->addChild(glGroup.release());
										}
										plGroup->addChild(pscGroup.release());
									}
									scopeGroup->addChild(plGroup.release());
								}
								rmwGroup->addChild(scopeGroup.release());
							}
							stGroup->addChild(rmwGroup.release());
						}
						cohGroup->addChild(stGroup.release());
					}
					dtGroup->addChild(cohGroup.release());
				}
				core11Group->addChild(dtGroup.release());
			}
			ttGroup->addChild(core11Group.release());
		}
		group->addChild(ttGroup.release());
	}

	TestGroupCase transVisCases[] =
	{
		{ 0,	"nontransvis",		"destination invocation acquires"		},
		{ 1,	"transvis",			"invocation 0,0 acquires"				},
	};

	de::MovePtr<tcu::TestCaseGroup> transGroup(new tcu::TestCaseGroup(testCtx, "transitive", "transitive"));
	for (int cohNdx = 0; cohNdx < DE_LENGTH_OF_ARRAY(cohCases); cohNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> cohGroup(new tcu::TestCaseGroup(testCtx, cohCases[cohNdx].name, cohCases[cohNdx].description));
		for (int stNdx = 0; stNdx < DE_LENGTH_OF_ARRAY(stCases); stNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> stGroup(new tcu::TestCaseGroup(testCtx, stCases[stNdx].name, stCases[stNdx].description));
			for (int plNdx = 0; plNdx < DE_LENGTH_OF_ARRAY(plCases); plNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> plGroup(new tcu::TestCaseGroup(testCtx, plCases[plNdx].name, plCases[plNdx].description));
				for (int pscNdx = 0; pscNdx < DE_LENGTH_OF_ARRAY(pscCases); pscNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> pscGroup(new tcu::TestCaseGroup(testCtx, pscCases[pscNdx].name, pscCases[pscNdx].description));
					for (int glNdx = 0; glNdx < DE_LENGTH_OF_ARRAY(glCases); glNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> glGroup(new tcu::TestCaseGroup(testCtx, glCases[glNdx].name, glCases[glNdx].description));
						for (int gscNdx = 0; gscNdx < DE_LENGTH_OF_ARRAY(gscCases); gscNdx++)
						{
							de::MovePtr<tcu::TestCaseGroup> gscGroup(new tcu::TestCaseGroup(testCtx, gscCases[gscNdx].name, gscCases[gscNdx].description));
							for (int visNdx = 0; visNdx < DE_LENGTH_OF_ARRAY(transVisCases); visNdx++)
							{
								CaseDef c =
								{
									!!plCases[plNdx].value,					// bool payloadMemLocal;
									!!glCases[glNdx].value,					// bool guardMemLocal;
									!!cohCases[cohNdx].value,				// bool coherent;
									false,									// bool core11;
									false,									// bool atomicRMW;
									TT_MP,									// TestType testType;
									(StorageClass)pscCases[pscNdx].value,	// StorageClass payloadSC;
									(StorageClass)gscCases[gscNdx].value,	// StorageClass guardSC;
									SCOPE_DEVICE,							// Scope scope;
									(SyncType)stCases[stNdx].value,			// SyncType syncType;
									STAGE_COMPUTE,							// Stage stage;
									DATA_TYPE_UINT,							// DataType dataType;
									true,									// bool transitive;
									!!transVisCases[visNdx].value,			// bool transitiveVis;
								};
								if (c.payloadSC == SC_WORKGROUP || c.guardSC == SC_WORKGROUP)
								{
									continue;
								}
								if (c.syncType == ST_CONTROL_BARRIER || c.syncType == ST_CONTROL_AND_MEMORY_BARRIER)
								{
									continue;
								}
								gscGroup->addChild(new MemoryModelTestCase(testCtx, transVisCases[visNdx].name, transVisCases[visNdx].description, c));
							}
							glGroup->addChild(gscGroup.release());
						}
						pscGroup->addChild(glGroup.release());
					}
					plGroup->addChild(pscGroup.release());
				}
				stGroup->addChild(plGroup.release());
			}
			cohGroup->addChild(stGroup.release());
		}
		transGroup->addChild(cohGroup.release());
	}
	group->addChild(transGroup.release());

	// Padding tests.
	group->addChild(createPaddingTests(testCtx));
	// Shared memory layout tests.
	group->addChild(createSharedMemoryLayoutTests(testCtx));

	return group.release();
}

}	// MemoryModel
}	// vkt
