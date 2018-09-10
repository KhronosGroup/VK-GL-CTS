/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2018 NVIDIA Corporation
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

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktDrawUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deDefs.h"
#include "deMath.h"
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
} DataType;

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
const VkFlags allPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

struct CaseDef
{
	CaseDef() : payloadMemLocal(true), guardMemLocal(true), coherent(true), mustpass11(true), atomicRMW(true), testType(TT_MP), payloadSC(SC_BUFFER), guardSC(SC_BUFFER), scope(SCOPE_DEVICE), syncType(ST_FENCE_FENCE), stage(STAGE_COMPUTE), dataType(DATA_TYPE_UINT) {}
	bool payloadMemLocal;
	bool guardMemLocal;
	bool coherent;
	bool mustpass11;
	bool atomicRMW;
	TestType testType;
	StorageClass payloadSC;
	StorageClass guardSC;
	Scope scope;
	SyncType syncType;
	Stage stage;
	DataType dataType;
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
	if (context.getUsedApiVersion() < VK_API_VERSION_1_1)
	{
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
	}

	if (!m_data.mustpass11)
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
	}
}


void MemoryModelTestCase::initPrograms (SourceCollections& programCollection) const
{
	Scope invocationMapping = m_data.scope;
	if ((m_data.scope == SCOPE_DEVICE || m_data.scope == SCOPE_QUEUEFAMILY) &&
		(m_data.payloadSC == SC_WORKGROUP || m_data.guardSC == SC_WORKGROUP))
	{
		invocationMapping = SCOPE_WORKGROUP;
	}

	const char *scopeStr;
	switch (m_data.scope)
	{
	case SCOPE_DEVICE:		scopeStr = "gl_ScopeDevice"; break;
	case SCOPE_QUEUEFAMILY:	scopeStr = "gl_ScopeQueueFamily"; break;
	case SCOPE_WORKGROUP:	scopeStr = "gl_ScopeWorkgroup"; break;
	case SCOPE_SUBGROUP:	scopeStr = "gl_ScopeSubgroup"; break;
	}

	const char *typeStr = m_data.dataType == DATA_TYPE_UINT64 ? "uint64_t" : "uint";

	// Construct storageSemantics strings. Both release and acquire
	// always have the payload storage class. They only include the
	// guard storage class if they're using FENCE for that side of the
	// sync.
	std::stringstream storageSemanticsRelease;
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
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
		DE_ASSERT(!m_data.mustpass11);
		semanticsRelease << " | gl_SemanticsMakeAvailable";
		semanticsAcquire << " | gl_SemanticsMakeVisible";
		semanticsAcquireRelease << " | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible";
	}

	std::stringstream css;
	css << "#version 450 core\n";
	if (!m_data.mustpass11)
	{
		css << "#pragma use_vulkan_memory_model\n";
	}
	css <<
		"#extension GL_KHR_shader_subgroup_basic : enable\n"
		"#extension GL_KHR_shader_subgroup_shuffle : enable\n"
		"#extension GL_KHR_shader_subgroup_ballot : enable\n"
		"#extension GL_KHR_memory_scope_semantics : enable\n"
		"#extension GL_ARB_gpu_shader_int64 : enable\n"
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
		if (m_data.mustpass11)
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
		DE_ASSERT(!m_data.mustpass11);
		memqual = "nonprivate";
	}

	// Declare payload, guard, and fail resources
	switch (m_data.payloadSC)
	{
	default: DE_ASSERT(0); // fall through
	case SC_BUFFER:		css << "layout(set=0, binding=0) " << memqual << " buffer Payload { " << typeStr << " x[]; } payload;\n"; break;
	case SC_IMAGE:		css << "layout(set=0, binding=0, r32ui) uniform " << memqual << " uimage2D payload;\n"; break;
	case SC_WORKGROUP:	css << "shared S payload;\n"; break;
	}
	if (m_data.syncType != ST_CONTROL_AND_MEMORY_BARRIER && m_data.syncType != ST_CONTROL_BARRIER)
	{
		// The guard variable is only accessed with atomics and need not be declared coherent.
		switch (m_data.guardSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_BUFFER:		css << "layout(set=0, binding=1) buffer Guard { " << typeStr << " x[]; } guard;\n"; break;
		case SC_IMAGE:		css << "layout(set=0, binding=1, r32ui) uniform uimage2D guard;\n"; break;
		case SC_WORKGROUP:	css << "shared S guard;\n"; break;
		}
	}

	css << "layout(set=0, binding=2) buffer Fail { uint x[]; } fail;\n";

	css <<
		"void main()\n"
		"{\n"
		"   bool pass = true;\n"
		"   bool skip = false;\n";

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
			"   ivec2 partnerImageCoord = subgroupShuffleXor(imageCoord, gl_SubgroupSize-1);\n";
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
			"   ivec2 partnerImageCoord = partnerGlobalId;\n";
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
		// Store payload
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
		case SC_BUFFER:		css << "   payload.x[bufferCoord] = bufferCoord + (payload.x[partnerBufferCoord]>>31);\n"; break;
		case SC_IMAGE:		css << "   imageStore(payload, imageCoord, uvec4(bufferCoord + (imageLoad(payload, partnerImageCoord).x>>31), 0, 0, 0));\n"; break;
		case SC_WORKGROUP:	css << "   payload.x[sharedCoord] = bufferCoord + (payload.x[partnerSharedCoord]>>31);\n"; break;
		}
	}
	else
	{
		DE_ASSERT(m_data.testType == TT_WAR);
		// Load payload
		switch (m_data.payloadSC)
		{
		default: DE_ASSERT(0); // fall through
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
			case SC_BUFFER:		css << "   atomicExchange(guard.x[bufferCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_IMAGE:		css << "   imageAtomicExchange(guard, imageCoord, (1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_WORKGROUP:	css << "   atomicExchange(guard.x[sharedCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			}
		}
		else
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
			case SC_BUFFER:		css << "   atomicStore(guard.x[bufferCoord], " << typeStr << "(1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
			case SC_IMAGE:		css << "   imageAtomicStore(guard, imageCoord, (1u), " << scopeStr << atomicReleaseSemantics.str() << ");\n"; break;
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
			case SC_BUFFER:		css << "   skip = atomicExchange(guard.x[partnerBufferCoord], 2u, " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_IMAGE:		css << "   skip = imageAtomicExchange(guard, partnerImageCoord, 2u, " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			case SC_WORKGROUP:	css << "   skip = atomicExchange(guard.x[partnerSharedCoord], 2u, " << scopeStr << atomicAcquireSemantics.str() << ") == 0;\n"; break;
			}
		} else
		{
			switch (m_data.guardSC)
			{
			default: DE_ASSERT(0); // fall through
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
		case SC_BUFFER:		css << "   " << typeStr << " r = payload.x[partnerBufferCoord];\n"; break;
		case SC_IMAGE:		css << "   " << typeStr << " r = imageLoad(payload, partnerImageCoord).x;\n"; break;
		case SC_WORKGROUP:	css << "   " << typeStr << " r = payload.x[partnerSharedCoord];\n"; break;
		}
		css <<
			"   if (!skip && r != partnerBufferCoord) { fail.x[bufferCoord] = 1; }\n"
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
		case SC_BUFFER:		css << "   payload.x[bufferCoord] = bufferCoord;\n"; break;
		case SC_IMAGE:		css << "   imageStore(payload, imageCoord, uvec4(bufferCoord, 0, 0, 0));\n"; break;
		case SC_WORKGROUP:	css << "   payload.x[sharedCoord] = bufferCoord;\n"; break;
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

	const vk::ShaderBuildOptions	buildOptions	(vk::SPIRV_VERSION_1_3, 0u);

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

TestInstance* MemoryModelTestCase::createInstance (Context& context) const
{
	return new MemoryModelTestInstance(context, m_data);
}

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize			bufferSize,
										 const VkBufferUsageFlags	usage)
{
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		(VkBufferCreateFlags)0,					// VkBufferCreateFlags	flags;
		bufferSize,								// VkDeviceSize			size;
		usage,									// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		pQueueFamilyIndices;
	};
	return bufferCreateInfo;
}

Move<VkDescriptorSet> makeDescriptorSet (const DeviceInterface&			vk,
										 const VkDevice					device,
										 const VkDescriptorPool			descriptorPool,
										 const VkDescriptorSetLayout	setLayout)
{
	const VkDescriptorSetAllocateInfo allocateParams =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		descriptorPool,										// VkDescriptorPool				descriptorPool;
		1u,													// deUint32						setLayoutCount;
		&setLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
	};
	return allocateDescriptorSet(vk, device, &allocateParams);
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
		size_t elementSize = m_data.dataType == DATA_TYPE_UINT64 ? sizeof(deUint64) : sizeof(deUint32);
		// buffer2 is the "fail" buffer, and is always uint
		if (i == 2)
			elementSize = sizeof(deUint32);
		bufferSizes[i] = NUM_INVOCATIONS * elementSize;

		bool local;
		switch (i)
		{
		default: DE_ASSERT(0); // fall through
		case 0:
			if (m_data.payloadSC != SC_BUFFER)
				continue;
			local = m_data.payloadMemLocal;
			break;
		case 1:
			if (m_data.guardSC != SC_BUFFER)
				continue;
			local = m_data.guardMemLocal;
			break;
		case 2: local = true; break;
		}

		try
		{
			buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
				vk, device, allocator, makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
				local ? MemoryRequirement::Local : MemoryRequirement::NonLocal));
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

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		  sType;
		DE_NULL,								// const void*			  pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags	   flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			  imageType;
		VK_FORMAT_R32_UINT,						// VkFormat				 format;
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
		VK_FORMAT_R32_UINT,										// VkFormat				   format;
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


	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		1,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
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

	const VkQueue				queue				= m_context.getUniversalQueue();
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer, 0u);

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

	VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	VkClearValue clearColor = makeClearValueColorU32(0,0,0,0);

	VkMemoryBarrier					memBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
		DE_NULL,							// pNext
		0u,									// srcAccessMask
		0u,									// dstAccessMask
	};

	for (deUint32 iters = 0; iters < 200; ++iters)
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

	const VkBufferCopy	copyParams =
	{
		(VkDeviceSize)0u,						// srcOffset
		(VkDeviceSize)0u,						// dstOffset
		bufferSizes[2]							// size
	};

	vk.cmdCopyBuffer(*cmdBuffer, **buffers[2], **copyBuffer, 1, &copyParams);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	tcu::TestLog& log = m_context.getTestContext().getLog();

	deUint32 *ptr = (deUint32 *)copyBuffer->getAllocation().getHostPtr();
	invalidateMappedMemoryRange(vk, device, copyBuffer->getAllocation().getMemory(), copyBuffer->getAllocation().getOffset(), bufferSizes[2]);
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

void createMemoryModelTests (tcu::TestCaseGroup* tests)
{
	tcu::TestContext& testCtx = tests->getTestContext();

	de::MovePtr<tcu::TestCaseGroup> mpGroup(new tcu::TestCaseGroup(
			testCtx, "message_passing", "Message passing test variants"));
	de::MovePtr<tcu::TestCaseGroup> warGroup(new tcu::TestCaseGroup(
			testCtx, "write_after_read", "Write-After-Read test variants"));

	for (deUint32 mustpass11 = 0; mustpass11 < 2; ++mustpass11) {
	for (deUint32 dt = 0; dt < 2; ++dt) {
	for (deUint32 coh = 0; coh < 2; ++coh) {
	for (deUint32 st = 0; st < 6; ++st) {
	for (deUint32 rmw = 0; rmw < 2; ++rmw) {
	for (deUint32 scope = 0; scope < 4; ++scope) {
	for (deUint32 pl = 0; pl < 2; ++pl) {
	for (deUint32 gl = 0; gl < 2; ++gl) {
	for (deUint32 psc = 0; psc < 3; ++psc) {
	for (deUint32 gsc = 0; gsc < 3; ++gsc) {
	for (deUint32 stage = 0; stage < 3; ++stage) {
		CaseDef c;
		std::string payloadStr = "_payload", guardStr = "_guard", scopeStr = "_scope", syncTypeStr, coherentStr, mp11, rmwStr, stageStr, typeStr;

		// Mustpass11 tests should only exercise things we expect to work on
		// existing implementations. Exclude noncoherent tests which require
		// new extensions, and assume atomic synchronization wouldn't work
		// (i.e. atomics may be implemented as relaxed atomics). Exclude
		// queuefamily scope which doesn't exist in Vulkan 1.1.
		if (mustpass11 == 1 &&
			(coh == 0 ||
			 st == ST_FENCE_ATOMIC ||
			 st == ST_ATOMIC_FENCE ||
			 st == ST_ATOMIC_ATOMIC ||
			 dt == DATA_TYPE_UINT64 ||
			 scope == SCOPE_QUEUEFAMILY))
		{
			continue;
		}

		if (stage != STAGE_COMPUTE &&
			scope == SCOPE_WORKGROUP)
		{
			continue;
		}

		// Don't exercise local and non-local for workgroup memory
		// Also don't exercise workgroup memory for non-compute stages
		if (psc == SC_WORKGROUP && (pl != 0 || stage != STAGE_COMPUTE))
		{
			continue;
		}
		if (gsc == SC_WORKGROUP && (gl != 0 || stage != STAGE_COMPUTE))
		{
			continue;
		}
		// Can't do control barrier with larger than workgroup scope, or non-compute stages
		if ((st == ST_CONTROL_BARRIER || st == ST_CONTROL_AND_MEMORY_BARRIER) && (scope == SCOPE_DEVICE || scope == SCOPE_QUEUEFAMILY || stage != STAGE_COMPUTE))
		{
			continue;
		}

		// Limit RMW atomics to ST_ATOMIC_ATOMIC, just to reduce # of test cases
		if (rmw && st != ST_ATOMIC_ATOMIC)
		{
			continue;
		}

		// uint64 testing is primarily for atomics, so only test it for ST_ATOMIC_ATOMIC
		if (dt == DATA_TYPE_UINT64 && st != ST_ATOMIC_ATOMIC)
		{
			continue;
		}

		// No 64-bit image types, so skip tests with both payload and guard in image memory
		if (dt == DATA_TYPE_UINT64 && psc == SC_IMAGE && gsc == SC_IMAGE)
		{
			continue;
		}

		// Control barrier tests don't use a guard variable, so only run them with gsc,gl==0
		if ((st == ST_CONTROL_BARRIER || st == ST_CONTROL_AND_MEMORY_BARRIER) &&
			(gsc != 0 || gl != 0))
		{
			continue;
		}

		c.atomicRMW = !!rmw;
		rmwStr = rmw ? "_rmw" : "";

		coherentStr = coh ? "coherent" : "noncoherent";
		c.coherent = !!coh;

		if (psc != SC_WORKGROUP)
		{
			c.payloadMemLocal = !!pl;
			payloadStr += c.payloadMemLocal ? "_local" : "_nonlocal";
		}
		if (gsc != SC_WORKGROUP)
		{
			c.guardMemLocal = !!gl;
			guardStr += c.guardMemLocal ? "_local" : "_nonlocal";
		}

		switch (st)
		{
		default: DE_ASSERT(0); // fall through
		case ST_FENCE_FENCE:				syncTypeStr = "_fence_fence"; break;
		case ST_FENCE_ATOMIC:				syncTypeStr = "_fence_atomic"; break;
		case ST_ATOMIC_FENCE:				syncTypeStr = "_atomic_fence"; break;
		case ST_ATOMIC_ATOMIC:				syncTypeStr = "_atomic_atomic"; break;
		case ST_CONTROL_BARRIER:			syncTypeStr = "_control_barrier"; break;
		case ST_CONTROL_AND_MEMORY_BARRIER:	syncTypeStr = "_control_and_memory_barrier"; break;
		}
		c.syncType = (SyncType)st;

		c.scope = (Scope)scope;
		switch (scope)
		{
		default: DE_ASSERT(0); // fall through
		case SCOPE_DEVICE:		scopeStr += "_device"; break;
		case SCOPE_QUEUEFAMILY:	scopeStr += "_queuefamily"; break;
		case SCOPE_WORKGROUP:	scopeStr += "_workgroup"; break;
		case SCOPE_SUBGROUP:	scopeStr += "_subgroup"; break;
		}

		c.payloadSC = StorageClass(psc);
		payloadStr += c.payloadSC == SC_BUFFER ? "_buffer" : c.payloadSC == SC_IMAGE ? "_image" : "_workgroup";
		c.guardSC = StorageClass(gsc);
		guardStr += c.guardSC == SC_BUFFER ? "_buffer" : c.guardSC == SC_IMAGE ? "_image" : "_workgroup";

		mp11 = mustpass11 ? "mustpass11_" : "";
		c.mustpass11 = !!mustpass11;

		c.stage = (Stage)stage;
		switch (stage)
		{
		default: DE_ASSERT(0); // fall through
		case STAGE_COMPUTE:		stageStr = "_comp"; break;
		case STAGE_VERTEX:		stageStr = "_vert"; break;
		case STAGE_FRAGMENT:	stageStr = "_frag"; break;
		}

		typeStr = dt == DATA_TYPE_UINT64 ? "_u64" : "";
		c.dataType = (DataType)dt;

		if (st == ST_CONTROL_BARRIER || st == ST_CONTROL_AND_MEMORY_BARRIER)
		{
			guardStr = "";
		}

		string name = mp11 + coherentStr + syncTypeStr + rmwStr + typeStr + payloadStr + guardStr + scopeStr + stageStr;

		c.testType = TT_MP;
		mpGroup->addChild(new MemoryModelTestCase(testCtx, name.c_str(), "", c));
		c.testType = TT_WAR;
		warGroup->addChild(new MemoryModelTestCase(testCtx, name.c_str(), "", c));
	}
	}
	}
	}
	}
	}
	}
	}
	}
	}
	}

	tests->addChild(mpGroup.release());
	tests->addChild(warGroup.release());
}

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "memory_model", "Exercise Vulkan Memory Model", createMemoryModelTests);
}

}	// MemoryModel
}	// vkt
