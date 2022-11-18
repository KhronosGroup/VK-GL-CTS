/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Property Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderPropertyTests.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"

#include <vector>
#include <string>
#include <map>
#include <sstream>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr			= de::MovePtr<tcu::TestCaseGroup>;
using ReplacementsMap	= std::map<std::string, std::string>;

using namespace vk;

tcu::StringTemplate getTaskShaderTemplate ()
{
	return tcu::StringTemplate(
		"#version 460\n"
		"#extension GL_NV_mesh_shader : enable\n"
		"\n"
		"layout (local_size_x=${TASK_LOCAL_SIZE_X:default=1}) in;\n"
		"\n"
		"${TASK_GLOBAL_DECL:opt}"
		"\n"
		"${TASK_MESH_INTERFACE_OUT:opt}"
		"\n"
		"void main ()\n"
		"{\n"
		"    gl_TaskCountNV = ${TASK_TASK_COUNT:default=0};\n"
		"${TASK_BODY:opt}"
		"}\n");
}

tcu::StringTemplate getMeshShaderTemplate ()
{
	return tcu::StringTemplate(
		"#version 460\n"
		"#extension GL_NV_mesh_shader : enable\n"
		"\n"
		"layout (local_size_x=${MESH_LOCAL_SIZE_X:default=1}) in;\n"
		"layout (triangles) out;\n"
		"layout (max_vertices=3, max_primitives=1) out;\n"
		"\n"
		"${MESH_GLOBAL_DECL:opt}"
		"\n"
		"${TASK_MESH_INTERFACE_IN:opt}"
		"\n"
		"void main ()\n"
		"{\n"
		"    gl_PrimitiveCountNV = 0u;\n"
		"${MESH_BODY:opt}"
		"}\n");
}

std::string getCommonStorageBufferDecl ()
{
	return "layout (set=0, binding=0) buffer OutputBlock { uint values[]; } ov;\n";
}

void genericCheckSupport (Context& context, bool taskShaderNeeded)
{
	checkTaskMeshShaderSupportNV(context, taskShaderNeeded, true);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
}

struct InstanceParams
{
	uint32_t	bufferElements;
	uint32_t	taskCount;
};

class MeshShaderPropertyInstance : public vkt::TestInstance
{
public:
					MeshShaderPropertyInstance	(Context& context, const InstanceParams& params)
						: vkt::TestInstance	(context)
						, m_params			(params)
						{}
	virtual			~MeshShaderPropertyInstance	(void) {}

	tcu::TestStatus	iterate						() override;

protected:
	InstanceParams	m_params;
};

tcu::TestStatus MeshShaderPropertyInstance::iterate ()
{
	const auto&		vkd			= m_context.getDeviceInterface();
	const auto		device		= m_context.getDevice();
	auto&			alloc		= m_context.getDefaultAllocator();
	const auto		queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue		= m_context.getUniversalQueue();
	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		extent		= makeExtent3D(1u, 1u, 1u);
	const auto		bindPoint	= VK_PIPELINE_BIND_POINT_GRAPHICS;
	const auto		useTask		= binaries.contains("task");

	const auto		storageBufferSize	= static_cast<VkDeviceSize>(m_params.bufferElements) * static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto		storageBufferUsage	= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const auto		storageBufferType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto		storageBufferStages	= (VK_SHADER_STAGE_MESH_BIT_NV | (useTask ? VK_SHADER_STAGE_TASK_BIT_NV : 0));

	// Create storage buffer with the required space.
	const auto			storageBufferInfo		= makeBufferCreateInfo(storageBufferSize, storageBufferUsage);
	BufferWithMemory	storageBuffer			(vkd, device, alloc, storageBufferInfo, MemoryRequirement::HostVisible);
	auto&				storageBufferAlloc		= storageBuffer.getAllocation();
	void*				storageBufferDataPtr	= storageBufferAlloc.getHostPtr();
	const auto			storageBufferDescInfo	= makeDescriptorBufferInfo(storageBuffer.get(), 0ull, storageBufferSize);

	deMemset(storageBufferDataPtr, 0xFF, static_cast<size_t>(storageBufferSize));
	flushAlloc(vkd, device, storageBufferAlloc);

	// Descriptor pool.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(storageBufferType);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(storageBufferType, storageBufferStages);
	const auto setLayout		= layoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

	// Allocate and prepare descriptor set.
	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder setUpdateBuilder;
	setUpdateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), storageBufferType, &storageBufferDescInfo);
	setUpdateBuilder.update(vkd, device);

	// Create empty render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), 0u, nullptr, extent.width, extent.height);

	// Shader modules and pipeline.
	Move<VkShaderModule>		taskModule;
	Move<VkShaderModule>		meshModule;
	const Move<VkShaderModule>	fragModule;	// No fragment shader.

	if (useTask)
		taskModule = createShaderModule(vkd, device, binaries.get("task"));
	meshModule = createShaderModule(vkd, device, binaries.get("mesh"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(extent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(extent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Run the pipeline.
	beginCommandBuffer(vkd, cmdBuffer);

	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0));
	vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
	vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params.taskCount, 0u);
	endRenderPass(vkd, cmdBuffer);

	const auto shaderToHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &shaderToHostBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify the storage buffer has the expected results.
	invalidateAlloc(vkd, device, storageBufferAlloc);

	std::vector<uint32_t> bufferData (m_params.bufferElements);
	deMemcpy(bufferData.data(), storageBufferDataPtr, de::dataSize(bufferData));

	for (size_t idx = 0u; idx < bufferData.size(); ++idx)
	{
		const auto	expected	= static_cast<uint32_t>(idx);
		const auto&	bufferValue	= bufferData[idx];

		if (bufferValue != expected)
			TCU_FAIL("Unexpected value found in buffer position " + de::toString(idx) + ": " + de::toString(bufferValue));
	}

	return tcu::TestStatus::pass("Pass");
}

class MaxDrawMeshTasksCountCase : public vkt::TestCase
{
public:
	enum class TestType { TASK=0, MESH };

					MaxDrawMeshTasksCountCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, TestType testType)
						: vkt::TestCase	(testCtx, name, description)
						, m_testType	(testType)
						{}
	virtual			~MaxDrawMeshTasksCountCase	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t minLimit = 65535u;

protected:
	TestType		m_testType;
};

void MaxDrawMeshTasksCountCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, (m_testType == TestType::TASK));

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxDrawMeshTasksCount < minLimit)
		TCU_FAIL("maxDrawMeshTasksCount property below the minimum limit");
}

TestInstance* MaxDrawMeshTasksCountCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		minLimit,						//	uint32_t	bufferElements;
		minLimit,						//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxDrawMeshTasksCountCase::initPrograms (vk::SourceCollections& programCollection) const
{
	ReplacementsMap meshReplacements;
	ReplacementsMap taskReplacements;

	const auto meshTemplate = getMeshShaderTemplate();

	const std::string desc = getCommonStorageBufferDecl();
	const std::string body = "    ov.values[gl_WorkGroupID.x] = gl_WorkGroupID.x;\n";

	if (m_testType == TestType::TASK)
	{
		const auto taskTemplate = getTaskShaderTemplate();
		taskReplacements["TASK_GLOBAL_DECL"]	= desc;
		taskReplacements["TASK_BODY"]			= body;

		programCollection.glslSources.add("task") << glu::TaskSource(taskTemplate.specialize(taskReplacements));
	}
	else
	{
		meshReplacements["MESH_GLOBAL_DECL"]	= desc;
		meshReplacements["MESH_BODY"]			= body;
	}

	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));
}

class MaxTaskWorkGroupInvocationsCase : public vkt::TestCase
{
public:
					MaxTaskWorkGroupInvocationsCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description) {}
	virtual			~MaxTaskWorkGroupInvocationsCase (void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t minLimit = 32u;
};

void MaxTaskWorkGroupInvocationsCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, true/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxTaskWorkGroupInvocations < minLimit)
		TCU_FAIL("maxTaskWorkGroupInvocations property below the minimum limit");
}

TestInstance* MaxTaskWorkGroupInvocationsCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		minLimit,	//	uint32_t	bufferElements;
		1u,			//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxTaskWorkGroupInvocationsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const ReplacementsMap	meshReplacements;
	const auto				meshTemplate		= getMeshShaderTemplate();

	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));

	ReplacementsMap	taskReplacements;
	const auto		taskTemplate		= getTaskShaderTemplate();

	taskReplacements["TASK_GLOBAL_DECL"]	= getCommonStorageBufferDecl();
	taskReplacements["TASK_BODY"]			= "    ov.values[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n";
	taskReplacements["TASK_LOCAL_SIZE_X"]	= de::toString(uint32_t{minLimit});

	programCollection.glslSources.add("task") << glu::TaskSource(taskTemplate.specialize(taskReplacements));
}

// In the case of the NV extension, this is very similar to the test above. Added for completion.
class MaxTaskWorkGroupSizeCase : public MaxTaskWorkGroupInvocationsCase
{
public:
	MaxTaskWorkGroupSizeCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
		: MaxTaskWorkGroupInvocationsCase (testCtx, name, description) {}

	void checkSupport (Context& context) const override;

	static constexpr uint32_t minSizeX = 32u;
	static constexpr uint32_t minSizeY = 1u;
	static constexpr uint32_t minSizeZ = 1u;
};

void MaxTaskWorkGroupSizeCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, true/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxTaskWorkGroupSize[0] < minSizeX ||
		properties.maxTaskWorkGroupSize[1] < minSizeY ||
		properties.maxTaskWorkGroupSize[2] < minSizeZ)
	{
		TCU_FAIL("maxTaskWorkGroupSize property below the minimum limit");
	}
}

class MaxTaskOutputCountCase : public vkt::TestCase
{
public:
					MaxTaskOutputCountCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description) {}
	virtual			~MaxTaskOutputCountCase (void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t minLimit = 65535u;
};

void MaxTaskOutputCountCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, true/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxTaskOutputCount < minLimit)
		TCU_FAIL("maxTaskOutputCount property below the minimum limit");
}

TestInstance* MaxTaskOutputCountCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		minLimit,	//	uint32_t	bufferElements;
		1u,			//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxTaskOutputCountCase::initPrograms (vk::SourceCollections& programCollection) const
{
	ReplacementsMap	meshReplacements;
	ReplacementsMap	taskReplacements;
	const auto		meshTemplate		= getMeshShaderTemplate();
	const auto		taskTemplate		= getTaskShaderTemplate();

	taskReplacements["TASK_TASK_COUNT"]		= de::toString(uint32_t{minLimit});
	meshReplacements["MESH_GLOBAL_DECL"]	= getCommonStorageBufferDecl();
	meshReplacements["MESH_BODY"]			= "    ov.values[gl_WorkGroupID.x] = gl_WorkGroupID.x;\n";

	programCollection.glslSources.add("task") << glu::TaskSource(taskTemplate.specialize(taskReplacements));
	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));
}

class MaxMeshWorkGroupInvocationsCase : public vkt::TestCase
{
public:
					MaxMeshWorkGroupInvocationsCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description) {}
	virtual			~MaxMeshWorkGroupInvocationsCase (void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t minLimit = 32u;
};

void MaxMeshWorkGroupInvocationsCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, false/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxMeshWorkGroupInvocations < minLimit)
		TCU_FAIL("maxMeshWorkGroupInvocations property below the minimum limit");
}

TestInstance* MaxMeshWorkGroupInvocationsCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		minLimit,	//	uint32_t	bufferElements;
		1u,			//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxMeshWorkGroupInvocationsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	ReplacementsMap	meshReplacements;
	const auto		meshTemplate		= getMeshShaderTemplate();

	meshReplacements["MESH_LOCAL_SIZE_X"]	= de::toString(uint32_t{minLimit});
	meshReplacements["MESH_GLOBAL_DECL"]	= getCommonStorageBufferDecl();
	meshReplacements["MESH_BODY"]			= "    ov.values[gl_LocalInvocationID.x] = gl_LocalInvocationID.x;\n";

	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));
}

// In the case of the NV extension, this is very similar to the test above. Added for completion.
class MaxMeshWorkGroupSizeCase : public MaxMeshWorkGroupInvocationsCase
{
public:
	MaxMeshWorkGroupSizeCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
		: MaxMeshWorkGroupInvocationsCase (testCtx, name, description) {}

	void checkSupport (Context& context) const override;

	static constexpr uint32_t minSizeX = 32u;
	static constexpr uint32_t minSizeY = 1u;
	static constexpr uint32_t minSizeZ = 1u;
};

void MaxMeshWorkGroupSizeCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, false/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxMeshWorkGroupSize[0] < minSizeX ||
		properties.maxMeshWorkGroupSize[1] < minSizeY ||
		properties.maxMeshWorkGroupSize[2] < minSizeZ)
	{
		TCU_FAIL("maxMeshWorkGroupSize property below the minimum limit");
	}
}

std::string getSharedArrayDecl (uint32_t numElements)
{
	std::ostringstream decl;
	decl
		<< "const uint arrayElements = " << de::toString(numElements) << ";\n"
		<< "shared uint sharedArray[arrayElements];\n"
		;
	return decl.str();
}

std::string getSharedMemoryBody (uint32_t localSize)
{
	std::ostringstream body;
	body
		<< "\n"
		<< "    if (gl_LocalInvocationID.x == 0u)\n"
		<< "    {\n"
		<< "        for (uint i = 0; i < arrayElements; ++i)\n"
		<< "            sharedArray[i] = 0u;\n"
		<< "    }\n"
		<< "\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "\n"
		<< "    for (uint i = 0; i < arrayElements; ++i)\n"
		<< "        atomicAdd(sharedArray[i], 1u);\n"
		<< "\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "\n"
		<< "    uint allGood = 1u;\n"
		<< "    for (uint i = 0; i < arrayElements; ++i)\n"
		<< "    {\n"
		<< "        if (sharedArray[i] != " << localSize << ")\n"
		<< "        {\n"
		<< "            allGood = 0u;\n"
		<< "            break;\n"
		<< "        }\n"
		<< "    }\n"
		<< "\n"
		<< "    ov.values[gl_LocalInvocationID.x] = ((allGood == 1u) ? gl_LocalInvocationID.x : gl_WorkGroupSize.x);\n"
		;

	return body.str();
}

class MaxTaskTotalMemorySizeCase : public vkt::TestCase
{
public:
					MaxTaskTotalMemorySizeCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description) {}
	virtual			~MaxTaskTotalMemorySizeCase (void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t localSize	= 32u;
	static constexpr uint32_t minLimit	= 16384u;
};

TestInstance* MaxTaskTotalMemorySizeCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		localSize,	//	uint32_t	bufferElements;
		1u,			//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxTaskTotalMemorySizeCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, true/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxTaskTotalMemorySize < minLimit)
		TCU_FAIL("maxTaskTotalMemorySize property below the minimum limit");
}

void MaxTaskTotalMemorySizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const ReplacementsMap	meshReplacements;
	const auto				meshTemplate		= getMeshShaderTemplate();

	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));

	const auto taskTemplate		= getTaskShaderTemplate();
	const auto arrayElements	= minLimit / static_cast<uint32_t>(sizeof(uint32_t));

	const auto globalDecls		= getCommonStorageBufferDecl() + getSharedArrayDecl(arrayElements);
	const auto body				= getSharedMemoryBody(localSize);

	ReplacementsMap taskReplacements;
	taskReplacements["TASK_LOCAL_SIZE_X"]	= de::toString(uint32_t{localSize});
	taskReplacements["TASK_GLOBAL_DECL"]	= globalDecls;
	taskReplacements["TASK_BODY"]			= body;

	programCollection.glslSources.add("task") << glu::TaskSource(taskTemplate.specialize(taskReplacements));
}

// Very similar to the previous one in NV.
class MaxMeshTotalMemorySizeCase : public vkt::TestCase
{
public:
					MaxMeshTotalMemorySizeCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description) {}
	virtual			~MaxMeshTotalMemorySizeCase (void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

	static constexpr uint32_t localSize	= 32u;
	static constexpr uint32_t minLimit	= 16384u;
};

TestInstance* MaxMeshTotalMemorySizeCase::createInstance (Context& context) const
{
	const InstanceParams params =
	{
		localSize,	//	uint32_t	bufferElements;
		1u,			//	uint32_t	taskCount;
	};
	return new MeshShaderPropertyInstance(context, params);
}

void MaxMeshTotalMemorySizeCase::checkSupport (Context& context) const
{
	genericCheckSupport(context, false/*taskShaderNeeded*/);

	const auto& properties = context.getMeshShaderProperties();
	if (properties.maxMeshTotalMemorySize < minLimit)
		TCU_FAIL("maxMeshTotalMemorySize property below the minimum limit");
}

void MaxMeshTotalMemorySizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto meshTemplate		= getMeshShaderTemplate();
	const auto arrayElements	= minLimit / static_cast<uint32_t>(sizeof(uint32_t));

	const auto globalDecls		= getCommonStorageBufferDecl() + getSharedArrayDecl(arrayElements);
	const auto body				= getSharedMemoryBody(localSize);

	ReplacementsMap meshReplacements;
	meshReplacements["MESH_LOCAL_SIZE_X"]	= de::toString(uint32_t{localSize});
	meshReplacements["MESH_GLOBAL_DECL"]	= globalDecls;
	meshReplacements["MESH_BODY"]			= body;

	programCollection.glslSources.add("mesh") << glu::MeshSource(meshTemplate.specialize(meshReplacements));
}

}

tcu::TestCaseGroup* createMeshShaderPropertyTests (tcu::TestContext& testCtx)
{
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "property", "Mesh Shader Property Tests"));

	mainGroup->addChild(new MaxDrawMeshTasksCountCase		(testCtx, "max_draw_mesh_tasks_count_with_task",	"", MaxDrawMeshTasksCountCase::TestType::TASK));
	mainGroup->addChild(new MaxDrawMeshTasksCountCase		(testCtx, "max_draw_mesh_tasks_count_with_mesh",	"", MaxDrawMeshTasksCountCase::TestType::MESH));
	mainGroup->addChild(new MaxTaskWorkGroupInvocationsCase	(testCtx, "max_task_work_group_invocations",		""));
	mainGroup->addChild(new MaxTaskWorkGroupSizeCase		(testCtx, "max_task_work_group_size",				""));
	mainGroup->addChild(new MaxTaskOutputCountCase			(testCtx, "max_task_output_count",					""));
	mainGroup->addChild(new MaxMeshWorkGroupInvocationsCase	(testCtx, "max_mesh_work_group_invocations",		""));
	mainGroup->addChild(new MaxMeshWorkGroupSizeCase		(testCtx, "max_mesh_work_group_size",				""));
	mainGroup->addChild(new MaxTaskTotalMemorySizeCase		(testCtx, "max_task_total_memory_size",				""));
	mainGroup->addChild(new MaxMeshTotalMemorySizeCase		(testCtx, "max_mesh_total_memory_size",				""));

	return mainGroup.release();
}

} // MeshShader
} // vkt
