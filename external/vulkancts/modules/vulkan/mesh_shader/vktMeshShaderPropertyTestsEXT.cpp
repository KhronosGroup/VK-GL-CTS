/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief Mesh Shader Property Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderPropertyTestsEXT.hpp"
#include "vktTestCase.hpp"
#include "vktMeshShaderUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <limits>

namespace vkt
{
namespace MeshShader
{

using namespace vk;

namespace
{

enum class PayLoadShMemSizeType
{
	PAYLOAD = 0,
	SHARED_MEMORY,
	BOTH,
};

struct PayloadShMemSizeParams
{
	PayLoadShMemSizeType testType;

	bool hasPayload			(void) const { return testType != PayLoadShMemSizeType::SHARED_MEMORY;	}
	bool hasSharedMemory	(void) const { return testType != PayLoadShMemSizeType::PAYLOAD;		}
};

using TaskPayloadShMemSizeParams	= PayloadShMemSizeParams;
using MeshPayloadShMemSizeParams	= PayloadShMemSizeParams;
using SpecConstVector				= std::vector<uint32_t>;

class TaskPayloadShMemSizeCase : public vkt::TestCase
{
public:
					TaskPayloadShMemSizeCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TaskPayloadShMemSizeParams& params)
						: vkt::TestCase				(testCtx, name, description)
						, m_params					(params)
						{}
	virtual			~TaskPayloadShMemSizeCase	(void) {}

	void			checkSupport				(Context& context) const override;
	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;

protected:
	// These depend on the context because we need the mesh shading properties to calculate them.
	struct ParamsFromContext
	{
		uint32_t payloadElements;
		uint32_t sharedMemoryElements;
	};

	ParamsFromContext getParamsFromContext (Context& context) const;

	const TaskPayloadShMemSizeParams m_params;

	static constexpr uint32_t kElementSize		= static_cast<uint32_t>(sizeof(uint32_t));
	static constexpr uint32_t kLocalInvocations	= 128u;
};

class SpecConstantInstance : public vkt::TestInstance
{
public:
											SpecConstantInstance	(Context& context, SpecConstVector&& vec)
												: vkt::TestInstance	(context)
												, m_specConstants	(std::move(vec))
												{}
	virtual									~SpecConstantInstance	(void) {}

protected:
	std::vector<VkSpecializationMapEntry>	makeSpecializationMap	(void) const;
	const SpecConstVector					m_specConstants;
};

std::vector<VkSpecializationMapEntry> SpecConstantInstance::makeSpecializationMap (void) const
{
	std::vector<VkSpecializationMapEntry> entryMap;
	entryMap.reserve(m_specConstants.size());

	const auto constantSize	= sizeof(uint32_t);
	const auto csU32		= static_cast<uint32_t>(constantSize);

	for (size_t i = 0u; i < m_specConstants.size(); ++i)
	{
		const auto id = static_cast<uint32_t>(i);

		const VkSpecializationMapEntry entry =
		{
			id,				//	uint32_t	constantID;
			(csU32 * id),	//	uint32_t	offset;
			constantSize,	//	size_t		size;
		};
		entryMap.push_back(entry);
	}

	return entryMap;
}

class PayloadShMemSizeInstance : public SpecConstantInstance
{
public:
						PayloadShMemSizeInstance	(Context& context, const TaskPayloadShMemSizeParams& params, SpecConstVector&& vec)
							: SpecConstantInstance	(context, std::move(vec))
							, m_params				(params)
							{}
	virtual				~PayloadShMemSizeInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	Move<VkRenderPass>						makeCustomRenderPass	(const DeviceInterface& vkd, VkDevice device);
	const TaskPayloadShMemSizeParams		m_params;
};

void TaskPayloadShMemSizeCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportEXT(context, true/*requireTask*/, true/*requireMesh*/);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

	const auto&	meshProperties	= context.getMeshShaderPropertiesEXT();
	const auto	minSize			= kLocalInvocations * kElementSize;

	// Note: the min required values for these properties in the spec would pass these checks.

	if (meshProperties.maxTaskPayloadSize < minSize)
		TCU_FAIL("Invalid maxTaskPayloadSize");

	if (meshProperties.maxTaskSharedMemorySize < minSize)
		TCU_FAIL("Invalid maxTaskSharedMemorySize");

	if (meshProperties.maxTaskPayloadAndSharedMemorySize < minSize)
		TCU_FAIL("Invalid maxTaskPayloadAndSharedMemorySize");

	if (meshProperties.maxMeshPayloadAndSharedMemorySize < minSize)
		TCU_FAIL("Invalid maxMeshPayloadAndSharedMemorySize");
}

TaskPayloadShMemSizeCase::ParamsFromContext TaskPayloadShMemSizeCase::getParamsFromContext (Context& context) const
{
	ParamsFromContext params;

	const auto&	meshProperties		= context.getMeshShaderPropertiesEXT();
	const auto	maxMeshPayloadSize	= std::min(meshProperties.maxMeshPayloadAndOutputMemorySize, meshProperties.maxMeshPayloadAndSharedMemorySize);
	const auto	maxPayloadElements	= std::min(meshProperties.maxTaskPayloadSize / kElementSize, maxMeshPayloadSize / kElementSize);
	const auto	maxShMemElements	= meshProperties.maxTaskSharedMemorySize / kElementSize;
	const auto	maxTotalElements	= meshProperties.maxTaskPayloadAndSharedMemorySize / kElementSize;

	if (m_params.testType == PayLoadShMemSizeType::PAYLOAD)
	{
		params.sharedMemoryElements	= 0u;
		params.payloadElements		= std::min(maxTotalElements, maxPayloadElements);
	}
	else if (m_params.testType == PayLoadShMemSizeType::SHARED_MEMORY)
	{
		params.payloadElements		= 0u;
		params.sharedMemoryElements	= std::min(maxTotalElements, maxShMemElements);
	}
	else
	{
		uint32_t*	minPtr;
		uint32_t	minVal;
		uint32_t*	maxPtr;
		uint32_t	maxVal;

		// Divide them as evenly as possible getting them as closest as possible to maxTotalElements.
		if (maxPayloadElements < maxShMemElements)
		{
			minPtr = &params.payloadElements;
			minVal = maxPayloadElements;

			maxPtr = &params.sharedMemoryElements;
			maxVal = maxShMemElements;
		}
		else
		{
			minPtr = &params.sharedMemoryElements;
			minVal = maxShMemElements;

			maxPtr = &params.payloadElements;
			maxVal = maxPayloadElements;
		}

		*minPtr = std::min(minVal, maxTotalElements / 2u);
		*maxPtr = std::min(maxTotalElements - (*minPtr), maxVal);
	}

	return params;
}

TestInstance* TaskPayloadShMemSizeCase::createInstance (Context &context) const
{
	const auto		ctxParams		= getParamsFromContext(context);
	SpecConstVector	specConstVec	{ ctxParams.payloadElements, ctxParams.sharedMemoryElements };

	return new PayloadShMemSizeInstance(context, m_params, std::move(specConstVec));
}

void TaskPayloadShMemSizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	const std::string scDecl =
		"layout (constant_id=0) const uint payloadElements = 1u;\n"
		"layout (constant_id=1) const uint sharedMemoryElements = 1u;\n"
		;

	const std::string dsDecl =
		"layout (set=0, binding=0, std430) buffer ResultBlock {\n"
		"    uint sharedOK;\n"
		"    uint payloadOK;\n"
		"} result;\n"
		;

	std::string taskData;
	std::string taskPayloadBody;
	std::string meshPayloadBody;

	if (m_params.hasPayload())
	{
		std::ostringstream taskDataStream;
		taskDataStream
			<< "struct TaskData {\n"
			<< "    uint elements[payloadElements];\n"
			<< "};\n"
			<< "taskPayloadSharedEXT TaskData td;\n"
			;
		taskData = taskDataStream.str();

		std::ostringstream taskBodyStream;
		taskBodyStream
			<< "    const uint payloadElementsPerInvocation = uint(ceil(float(payloadElements) / float(" << kLocalInvocations << ")));\n"
			<< "    for (uint i = 0u; i < payloadElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = payloadElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < payloadElements) {\n"
			<< "            td.elements[elemIdx] = elemIdx + 2000u;\n"
			<< "        }\n"
			<< "    }\n"
			<< "\n"
			;
		taskPayloadBody = taskBodyStream.str();

		std::ostringstream meshBodyStream;
		meshBodyStream
			<< "    bool allOK = true;\n"
			<< "    for (uint i = 0u; i < payloadElements; ++i) {\n"
			<< "        if (td.elements[i] != i + 2000u) {\n"
			<< "            allOK = false;\n"
			<< "            break;\n"
			<< "        }\n"
			<< "    }\n"
			<< "    result.payloadOK = (allOK ? 1u : 0u);\n"
			<< "\n"
			;
		meshPayloadBody = meshBodyStream.str();
	}
	else
	{
		meshPayloadBody = "    result.payloadOK = 1u;\n";
	}

	std::string sharedData;
	std::string taskSharedDataBody;

	if (m_params.hasSharedMemory())
	{
		sharedData = "shared uint sharedElements[sharedMemoryElements];\n";

		std::ostringstream bodyStream;
		bodyStream
			<< "    const uint shMemElementsPerInvocation = uint(ceil(float(sharedMemoryElements) / float(" << kLocalInvocations << ")));\n"
			<< "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < sharedMemoryElements) {\n"
			<< "            sharedElements[elemIdx] = elemIdx * 2u + 1000u;\n" // Write
			<< "        }\n"
			<< "    }\n"
			<< "    memoryBarrierShared();\n"
			<< "    barrier();\n"
			<< "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < sharedMemoryElements) {\n"
			<< "            const uint accessIdx = sharedMemoryElements - 1u - elemIdx;\n"
			<< "            sharedElements[accessIdx] += accessIdx;\n" // Read+Write a different element.
			<< "        }\n"
			<< "    }\n"
			<< "    memoryBarrierShared();\n"
			<< "    barrier();\n"
			<< "    if (gl_LocalInvocationIndex == 0u) {\n"
			<< "        bool allOK = true;\n"
			<< "        for (uint i = 0u; i < sharedMemoryElements; ++i) {\n"
			<< "            if (sharedElements[i] != i*3u + 1000u) {\n"
			<< "                allOK = false;\n"
			<< "                break;\n"
			<< "            }\n"
			<< "        }\n"
			<< "        result.sharedOK = (allOK ? 1u : 0u);\n"
			<< "    }\n"
			<< "\n"
			;
		taskSharedDataBody = bodyStream.str();
	}
	else
	{
		taskSharedDataBody =
			"    if (gl_LocalInvocationIndex == 0u) {\n"
			"        result.sharedOK = 1u;\n"
			"    }\n"
			;
	}

	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
		<< scDecl
		<< dsDecl
		<< taskData
		<< sharedData
		<< "\n"
		<< "void main () {\n"
		<< taskSharedDataBody
		<< taskPayloadBody
		<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< scDecl
		<< dsDecl
		<< taskData
		<< "\n"
		<< "void main () {\n"
		<< meshPayloadBody
		<< "    SetMeshOutputsEXT(0u, 0u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

Move<VkRenderPass> PayloadShMemSizeInstance::makeCustomRenderPass (const DeviceInterface& vkd, VkDevice device)
{
	const auto subpassDesc	= makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr);
	const auto dependency	= makeSubpassDependency(0u, 0u, VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, 0u);

	const VkRenderPassCreateInfo renderPassCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,									//	const void*						pNext;
		0u,											//	VkRenderPassCreateFlags			flags;
		0u,											//	uint32_t						attachmentCount;
		nullptr,									//	const VkAttachmentDescription*	pAttachments;
		1u,											//	uint32_t						subpassCount;
		&subpassDesc,								//	const VkSubpassDescription*		pSubpasses;
		1u,											//	uint32_t						dependencyCount;
		&dependency,								//	const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vkd, device, &renderPassCreateInfo);
}

tcu::TestStatus PayloadShMemSizeInstance::iterate (void)
{
	const auto&		vkd						= m_context.getDeviceInterface();
	const auto		device					= m_context.getDevice();
	auto&			alloc					= m_context.getDefaultAllocator();
	const auto		queueIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto		queue					= m_context.getUniversalQueue();
	const auto		framebufferExtent		= makeExtent2D(1u, 1u);
	const auto		pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;

	const auto			resultsBufferSize		= static_cast<VkDeviceSize>(sizeof(uint32_t) * 2u);
	const auto			resultsBufferDescType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto			resultsBufferUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const auto			resultsBufferStages		= (VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT);
	const auto			resultsBufferCreateInfo	= makeBufferCreateInfo(resultsBufferSize, resultsBufferUsage);
	BufferWithMemory	resultsBuffer			(vkd, device, alloc, resultsBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				resultsBufferAlloc		= resultsBuffer.getAllocation();
	void*				resultsBufferDataPtr	= resultsBufferAlloc.getHostPtr();

	deMemset(resultsBufferDataPtr, 0, static_cast<size_t>(resultsBufferSize));

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(resultsBufferDescType, resultsBufferStages);
	const auto setLayout		= layoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(resultsBufferDescType);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto resultsBufferDescInfo = makeDescriptorBufferInfo(resultsBuffer.get(), 0ull, resultsBufferSize);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), resultsBufferDescType, &resultsBufferDescInfo);
	updateBuilder.update(vkd, device);

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	hasTask		= binaries.contains("task");
	const auto	taskShader	= (hasTask ? createShaderModule(vkd, device, binaries.get("task")) : Move<VkShaderModule>());
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));

	const auto renderPass	= makeCustomRenderPass(vkd, device);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), 0u, nullptr, framebufferExtent.width, framebufferExtent.height);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(framebufferExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(framebufferExtent));

	const auto					specMap		= makeSpecializationMap();
	const VkSpecializationInfo	specInfo	=
	{
		static_cast<uint32_t>(specMap.size()),	//	uint32_t						mapEntryCount;
		de::dataOrNull(specMap),				//	const VkSpecializationMapEntry*	pMapEntries;
		de::dataSize(m_specConstants),			//	size_t							dataSize;
		de::dataOrNull(m_specConstants),		//	const void*						pData;
	};

	std::vector<VkPipelineShaderStageCreateInfo>	shaderStages;
	VkPipelineShaderStageCreateInfo					stageInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,						//	VkShaderStageFlagBits				stage;
		DE_NULL,												//	VkShaderModule						module;
		"main",													//	const char*							pName;
		&specInfo,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	if (hasTask)
	{
		stageInfo.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
		stageInfo.module = taskShader.get();
		shaderStages.push_back(stageInfo);
	}

	{
		stageInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
		stageInfo.module = meshShader.get();
		shaderStages.push_back(stageInfo);
	}

	const auto pipeline = makeGraphicsPipeline(vkd, device,
		DE_NULL, pipelineLayout.get(), 0u,
		shaderStages, renderPass.get(), viewports, scissors);

	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u));
	vkd.cmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);
	{
		const auto writeToHost = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		const auto writeStages = (VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT);
		cmdPipelineMemoryBarrier(vkd, cmdBuffer, writeStages, VK_PIPELINE_STAGE_HOST_BIT, &writeToHost);
	}
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, resultsBufferAlloc);
	struct
	{
		uint32_t sharedOK;
		uint32_t payloadOK;
	} resultData;
	deMemcpy(&resultData, resultsBufferDataPtr, sizeof(resultData));

	if (resultData.sharedOK != 1u)
		TCU_FAIL("Unexpected shared memory result: " + std::to_string(resultData.sharedOK));

	if (resultData.payloadOK != 1u)
		TCU_FAIL("Unexpected payload result: " + std::to_string(resultData.payloadOK));

	return tcu::TestStatus::pass("Pass");
}

class MaxViewIndexCase : public vkt::TestCase
{
public:
					MaxViewIndexCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description)
						{}
	virtual			~MaxViewIndexCase	(void) {}

	void			checkSupport	(Context& context) const override;
	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
};

class MaxViewIndexInstance : public vkt::TestInstance
{
public:
						MaxViewIndexInstance	(Context& context)
							: vkt::TestInstance (context)
							{}
	virtual				~MaxViewIndexInstance	(void) {}

	tcu::TestStatus		iterate					(void) override;
	Move<VkRenderPass>	makeCustomRenderPass	(const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format);

	static constexpr uint32_t kMaxViews = 32u;
};

void MaxViewIndexCase::checkSupport (Context &context) const
{
	checkTaskMeshShaderSupportEXT(context, false/*requireTask*/, true/*requireMesh*/);

	const auto& multiviewFeatures = context.getMultiviewFeatures();
	if (!multiviewFeatures.multiview)
		TCU_THROW(NotSupportedError, "Multiview not supported");

	const auto& meshFeatures = context.getMeshShaderFeaturesEXT();
	if (!meshFeatures.multiviewMeshShader)
		TCU_THROW(NotSupportedError, "Multiview not supported for mesh shaders");
}

void MaxViewIndexCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "#extension GL_EXT_multiview : enable\n"
		<< "\n"
		<< "layout (location=0) out uvec4 outColor;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    outColor = uvec4(uint(gl_ViewIndex) + 1u, 0, 0, 0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
}

TestInstance* MaxViewIndexCase::createInstance (Context& context) const
{
	return new MaxViewIndexInstance(context);
}

Move<VkRenderPass> MaxViewIndexInstance::makeCustomRenderPass (const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format)
{
	DE_ASSERT(layerCount > 0u);

	const VkAttachmentDescription colorAttachmentDescription =
	{
		0u,											// VkAttachmentDescriptionFlags    flags
		format,										// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference colorAttachmentRef = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const VkSubpassDescription subpassDescription =
	{
		0u,									// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		nullptr,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		nullptr,							// const VkAttachmentReference*    pResolveAttachments
		nullptr,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		nullptr								// const deUint32*                 pPreserveAttachments
	};

	const uint32_t viewMask = ((1u << layerCount) - 1u);
	const VkRenderPassMultiviewCreateInfo multiviewCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		1u,														//	uint32_t		subpassCount;
		&viewMask,												//	const uint32_t*	pViewMasks;
		0u,														//	uint32_t		dependencyCount;
		nullptr,												//	const int32_t*	pViewOffsets;
		1u,														//	uint32_t		correlationMaskCount;
		&viewMask,												//	const uint32_t*	pCorrelationMasks;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType                   sType
		&multiviewCreateInfo,									// const void*                       pNext
		0u,														// VkRenderPassCreateFlags           flags
		1u,														// deUint32                          attachmentCount
		&colorAttachmentDescription,							// const VkAttachmentDescription*    pAttachments
		1u,														// deUint32                          subpassCount
		&subpassDescription,									// const VkSubpassDescription*       pSubpasses
		0u,														// deUint32                          dependencyCount
		nullptr,												// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vkd, device, &renderPassInfo);
}

tcu::TestStatus MaxViewIndexInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();
	const auto&			meshProperties	= m_context.getMeshShaderPropertiesEXT();
	const auto			maxViews		= kMaxViews;
	const auto			numViews		= std::min(meshProperties.maxMeshMultiviewViewCount, maxViews);
	const auto			viewType		= ((numViews > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	const auto			colorFormat		= VK_FORMAT_R32_UINT;
	const auto			tcuColorFormat	= mapVkFormat(colorFormat);
	const auto			pixelSize		= static_cast<uint32_t>(tcu::getPixelSize(tcuColorFormat));
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			fbExtent		= makeExtent3D(8u, 8u, 1u);
	const tcu::IVec3	iExtent3D		(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(numViews));
	const tcu::UVec4	clearColor		(0u, 0u, 0u, 0u);

	// Create color attachment.
	const VkImageCreateInfo colorAttachmentCreatInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		fbExtent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		numViews,								//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorAttachment		(vkd, device, alloc, colorAttachmentCreatInfo, MemoryRequirement::Any);
	const auto		colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numViews);
	const auto		colorSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, numViews);
	const auto		colorAttachmentView	= makeImageView(vkd, device, colorAttachment.get(), viewType, colorFormat, colorSRR);

	// Verification buffer for the color attachment.
	DE_ASSERT(fbExtent.depth == 1u);
	const auto			verificationBufferUsage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto			verificationBufferSize			= static_cast<VkDeviceSize>(pixelSize * fbExtent.width * fbExtent.height * numViews);
	const auto			verificationBufferCreateInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);
	BufferWithMemory	verificationBuffer				(vkd, device, alloc, verificationBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc			= verificationBuffer.getAllocation();
	void*				verificationBufferData			= verificationBufferAlloc.getHostPtr();

	deMemset(verificationBufferData, 0, static_cast<size_t>(verificationBufferSize));

	const auto	pipelineLayout	= makePipelineLayout(vkd, device);
	const auto	renderPass		= makeCustomRenderPass(vkd, device, numViews, colorFormat);
	const auto	framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), fbExtent.width, fbExtent.height, 1u);

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		DE_NULL, meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);

	const auto preTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAttachment.get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

	const auto copyRegion = makeBufferImageCopy(fbExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

	const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, verificationBufferAlloc);
	tcu::ConstPixelBufferAccess resultAccess (tcuColorFormat, iExtent3D, verificationBufferData);

	for (int z = 0; z < iExtent3D.z(); ++z)
	{
		const tcu::UVec4 expectedPixel (static_cast<uint32_t>(z) + 1u, 0u, 0u, 1u);
		for (int y = 0; y < iExtent3D.y(); ++y)
			for (int x = 0; x < iExtent3D.x(); ++x)
			{
				const auto resultPixel = resultAccess.getPixelUint(x, y, z);
				if (resultPixel != expectedPixel)
				{
					std::ostringstream msg;
					msg
						<< "Unexpected pixel value at layer " << z << ": (" << x << ", " << y << ") is "
						<< resultPixel << " while expecting " << expectedPixel
						;
					TCU_FAIL(msg.str());
				}
			}
	}

	// QualityWarning if needed.
	if (meshProperties.maxMeshMultiviewViewCount > maxViews)
	{
		const auto maxViewsStr = std::to_string(maxViews);
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Test passed but maxMeshMultiviewViewCount greater than " + maxViewsStr);
	}

	return tcu::TestStatus::pass("Pass");
}

class MaxOutputLayersCase : public vkt::TestCase
{
public:
					MaxOutputLayersCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase (testCtx, name, description)
						{}
	virtual			~MaxOutputLayersCase	(void) {}

	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
};

class MaxOutputLayersInstance : public vkt::TestInstance
{
public:
						MaxOutputLayersInstance		(Context& context) : vkt::TestInstance(context) {}
	virtual				~MaxOutputLayersInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;
};

TestInstance* MaxOutputLayersCase::createInstance (Context& context) const
{
	return new MaxOutputLayersInstance(context);
}

void MaxOutputLayersCase::checkSupport (Context &context) const
{
	checkTaskMeshShaderSupportEXT(context, false/*requireTask*/, true/*requireMesh*/);
}

void MaxOutputLayersCase::initPrograms (vk::SourceCollections &programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
		<< "\n"
		<< "    gl_MeshPrimitivesEXT[0].gl_Layer = int(gl_WorkGroupID.x);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout (location=0) out uvec4 outColor;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    outColor = uvec4(uint(gl_Layer) + 1u, 0, 0, 0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus MaxOutputLayersInstance::iterate (void)
{
	const auto&			vki				= m_context.getInstanceInterface();
	const auto&			physicalDevice	= m_context.getPhysicalDevice();
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			fbFormat		= VK_FORMAT_R32_UINT;
	const auto			imageType		= VK_IMAGE_TYPE_2D;
	const auto			tiling			= VK_IMAGE_TILING_OPTIMAL;
	const auto			usage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			sampleCount		= VK_SAMPLE_COUNT_1_BIT;
	auto&				log				= m_context.getTestContext().getLog();

	// Find out how many layers we can actually use.
	const auto&	properties			= m_context.getDeviceProperties();
	const auto&	meshProperties		= m_context.getMeshShaderPropertiesEXT();
	const auto	formatProperties	= getPhysicalDeviceImageFormatProperties(vki, physicalDevice, fbFormat, imageType, tiling, usage, 0u);
	const auto	layerCount			= std::min({
		properties.limits.maxFramebufferLayers,
		meshProperties.maxMeshOutputLayers,
		formatProperties.maxArrayLayers,
		meshProperties.maxMeshWorkGroupCount[0],
		});

	// This is needed for iExtent3D below.
	DE_ASSERT(static_cast<uint64_t>(std::numeric_limits<int>::max()) >= static_cast<uint64_t>(layerCount));
	log << tcu::TestLog::Message << "Using " + std::to_string(layerCount) + " layers" << tcu::TestLog::EndMessage;

	const auto			viewType		= ((layerCount > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	const auto			tcuColorFormat	= mapVkFormat(fbFormat);
	const auto			pixelSize		= static_cast<uint32_t>(tcu::getPixelSize(tcuColorFormat));
	const auto			fbExtent		= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	iExtent3D		(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(layerCount));
	const tcu::UVec4	clearColor		(0u, 0u, 0u, 0u);

	// Create color attachment.
	const VkImageCreateInfo colorAttachmentCreatInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		imageType,								//	VkImageType				imageType;
		fbFormat,								//	VkFormat				format;
		fbExtent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		layerCount,								//	uint32_t				arrayLayers;
		sampleCount,							//	VkSampleCountFlagBits	samples;
		tiling,									//	VkImageTiling			tiling;
		usage,									//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorAttachment		(vkd, device, alloc, colorAttachmentCreatInfo, MemoryRequirement::Any);
	const auto		colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);
	const auto		colorSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, layerCount);
	const auto		colorAttachmentView	= makeImageView(vkd, device, colorAttachment.get(), viewType, fbFormat, colorSRR);

	// Verification buffer for the color attachment.
	DE_ASSERT(fbExtent.depth == 1u);
	const auto			verificationBufferUsage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto			verificationBufferSize			= static_cast<VkDeviceSize>(pixelSize * fbExtent.width * fbExtent.height * layerCount);
	const auto			verificationBufferCreateInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);
	BufferWithMemory	verificationBuffer				(vkd, device, alloc, verificationBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc			= verificationBuffer.getAllocation();
	void*				verificationBufferData			= verificationBufferAlloc.getHostPtr();

	deMemset(verificationBufferData, 0, static_cast<size_t>(verificationBufferSize));

	const auto	pipelineLayout	= makePipelineLayout(vkd, device);
	const auto	renderPass		= makeRenderPass(vkd, device, fbFormat);
	const auto	framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), fbExtent.width, fbExtent.height, layerCount);

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		DE_NULL, meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, layerCount, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);

	const auto preTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAttachment.get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

	const auto copyRegion = makeBufferImageCopy(fbExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

	const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, verificationBufferAlloc);
	tcu::ConstPixelBufferAccess resultAccess (tcuColorFormat, iExtent3D, verificationBufferData);

	for (int z = 0; z < iExtent3D.z(); ++z)
	{
		const tcu::UVec4 expectedPixel (static_cast<uint32_t>(z) + 1u, 0u, 0u, 1u);
		for (int y = 0; y < iExtent3D.y(); ++y)
			for (int x = 0; x < iExtent3D.x(); ++x)
			{
				const auto resultPixel = resultAccess.getPixelUint(x, y, z);
				if (resultPixel != expectedPixel)
				{
					std::ostringstream msg;
					msg
						<< "Unexpected pixel value at layer " << z << ": (" << x << ", " << y << ") is "
						<< resultPixel << " while expecting " << expectedPixel
						;
					TCU_FAIL(msg.str());
				}
			}
	}

	return tcu::TestStatus::pass("Pass");
}

enum class MaxPrimVertType
{
	PRIMITIVES,
	VERTICES,
};

struct MaxPrimVertParams
{
	MaxPrimVertType testType;
	uint32_t		itemCount;
};

class MaxMeshOutputPrimVertCase : public vkt::TestCase
{
public:
					MaxMeshOutputPrimVertCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const MaxPrimVertParams& params)
						: vkt::TestCase			(testCtx, name, description)
						, m_params				(params)
						{}
	virtual			~MaxMeshOutputPrimVertCase	(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;

protected:
	static constexpr uint32_t kLocalInvocations = 128u;

	const MaxPrimVertParams	m_params;
};

class MaxMeshOutputPrimVertInstance : public vkt::TestInstance
{
public:
						MaxMeshOutputPrimVertInstance	(Context& context, uint32_t shaderPrimitives, uint32_t fbWidth)
							: vkt::TestInstance			(context)
							, m_shaderPrimitives		(shaderPrimitives)
							, m_fbWidth					(fbWidth)
							{
								DE_ASSERT(m_shaderPrimitives > 0u);
								DE_ASSERT(m_fbWidth > 0u);
							}
	virtual				~MaxMeshOutputPrimVertInstance	(void) {}

	tcu::TestStatus		iterate							(void) override;

protected:
	const uint32_t		m_shaderPrimitives;
	const uint32_t		m_fbWidth;
};

TestInstance* MaxMeshOutputPrimVertCase::createInstance (Context &context) const
{
	const auto fbWidth = ((m_params.testType == MaxPrimVertType::PRIMITIVES) ? 1u : m_params.itemCount);
	return new MaxMeshOutputPrimVertInstance(context, m_params.itemCount, fbWidth);
}

void MaxMeshOutputPrimVertCase::checkSupport (Context &context) const
{
	checkTaskMeshShaderSupportEXT(context, false/*requireTask*/, true/*requireMesh*/);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

	// Note when testing vertices, due to our usage of points as the primitive type, we are also limited by the number of primitives.

	const auto	isVertices		= (m_params.testType == MaxPrimVertType::VERTICES);
	const auto&	meshProperties	= context.getMeshShaderPropertiesEXT();
	const auto&	itemLimit		= isVertices
								? std::min(meshProperties.maxMeshOutputVertices, meshProperties.maxMeshOutputPrimitives)
								: meshProperties.maxMeshOutputPrimitives;

	if (m_params.itemCount > itemLimit)
		TCU_THROW(NotSupportedError, "Implementation does not support the given amount of items");

	// Check memory limits just in case.
	uint32_t	totalBytes		= 0u;
	const auto	perVertexBytes	= static_cast<uint32_t>(sizeof(tcu::Vec4) + sizeof(float)); // gl_Position and gl_PointSize

	if (isVertices)
	{
		// No per-primitive data in this variant.
		const auto actualVertices		= de::roundUp(m_params.itemCount, meshProperties.meshOutputPerVertexGranularity);

		totalBytes = perVertexBytes * actualVertices;
	}
	else
	{
		// Single vertex, but using gl_PrimitiveID in each primitive.
		const auto perPrimitiveBytes	= static_cast<uint32_t>(sizeof(uint32_t)); // gl_PrimitiveID
		const auto actualVertices		= de::roundUp(1u, meshProperties.meshOutputPerVertexGranularity);
		const auto actualPrimitives		= de::roundUp(m_params.itemCount, meshProperties.meshOutputPerPrimitiveGranularity);

		totalBytes = perVertexBytes * actualVertices + perPrimitiveBytes * actualPrimitives;
	}

	if (totalBytes > meshProperties.maxMeshOutputMemorySize)
		TCU_THROW(NotSupportedError, "Not enough output memory for this test");
}

void MaxMeshOutputPrimVertCase::initPrograms (vk::SourceCollections &programCollection) const
{
	const auto buildOptions		= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const bool isPrimitives		= (m_params.testType == MaxPrimVertType::PRIMITIVES);
	const auto associatedVertex	= (isPrimitives ? "0u" : "primitiveID");
	const auto maxVertices		= (isPrimitives ? 1u : m_params.itemCount);
	const auto ssboIndex		= (isPrimitives ? "gl_PrimitiveID" : "uint(gl_FragCoord.x)");
	const auto xCoord			= (isPrimitives ? "0.0" : "(float(vertexID) + 0.5) / float(maxVertices) * 2.0 - 1.0");
	const auto maxPrimitives	= m_params.itemCount;

	// When testing vertices, we'll use a wide framebuffer, emit one vertex per pixel and use the fragment coords to index into the
	// SSBO. When testing primitives, we'll use a 1x1 framebuffer, emit one single vertex in the center and use the primitive id to
	// index into the SSBO.
	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout (set=0, binding=0, std430) buffer OutputBlock {\n"
		<< "    uint flags[];\n"
		<< "} ssbo;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    ssbo.flags[" << ssboIndex << "] = 1u;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
		<< "layout (points) out;\n"
		<< "layout (max_vertices=" << maxVertices << ", max_primitives=" << maxPrimitives << ") out;\n"
		<< "\n"
		<< "out gl_MeshPerVertexEXT {\n"
		<< "    vec4  gl_Position;\n"
		<< "    float gl_PointSize;\n"
		<< "} gl_MeshVerticesEXT[];\n"
		<< "\n"
		;

	if (isPrimitives)
	{
		mesh
			<< "perprimitiveEXT out gl_MeshPerPrimitiveEXT {\n"
			<< "    int gl_PrimitiveID;\n"
			<< "} gl_MeshPrimitivesEXT[];\n"
			<< "\n"
			;
	}

	mesh
		<< "void main (void) {\n"
		<< "    const uint localInvs = " << kLocalInvocations << "u;\n"
		<< "    const uint maxVertices = " << maxVertices << "u;\n"
		<< "    const uint maxPoints = " << maxPrimitives << "u;\n"
		<< "    const uint verticesPerInvocation = (maxVertices + localInvs - 1u) / localInvs;\n"
		<< "    const uint primitivesPerInvocation = (maxPoints + localInvs - 1u) / localInvs;\n"
		<< "\n"
		<< "    SetMeshOutputsEXT(maxVertices, maxPoints);\n"
		<< "\n"
		<< "    for (uint i = 0u; i < verticesPerInvocation; ++i) {\n"
		<< "        const uint vertexID = gl_LocalInvocationIndex * verticesPerInvocation + i;\n"
		<< "        if (vertexID >= maxVertices) {\n"
		<< "            break;\n"
		<< "        }\n"
		<< "        const float xCoord = " << xCoord << ";\n"
		<< "        gl_MeshVerticesEXT[vertexID].gl_Position = vec4(xCoord, 0.0, 0.0, 1.0);\n"
		<< "        gl_MeshVerticesEXT[vertexID].gl_PointSize = 1.0f;\n"
		<< "    }\n"
		<< "\n"
		<< "    for (uint i = 0u; i < primitivesPerInvocation; ++i) {\n"
		<< "        const uint primitiveID = gl_LocalInvocationIndex * primitivesPerInvocation + i;\n"
		<< "        if (primitiveID >= maxPoints) {\n"
		<< "            break;\n"
		<< "        }\n"
		<< (isPrimitives ? "        gl_MeshPrimitivesEXT[primitiveID].gl_PrimitiveID = int(primitiveID);\n" : "")
		<< "        gl_PrimitivePointIndicesEXT[primitiveID] = " << associatedVertex << ";\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

tcu::TestStatus MaxMeshOutputPrimVertInstance::iterate (void)
{
	const auto&		vkd				= m_context.getDeviceInterface();
	const auto		device			= m_context.getDevice();
	auto&			alloc			= m_context.getDefaultAllocator();
	const auto		queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto		queue			= m_context.getUniversalQueue();
	const auto		fbExtent		= makeExtent2D(m_fbWidth, 1u);
	const auto		bindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;

	const auto		ssboSize		= static_cast<VkDeviceSize>(sizeof(uint32_t) * m_shaderPrimitives);
	const auto		ssboUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const auto		ssboDescType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	const auto			ssboCreateInfo	= makeBufferCreateInfo(ssboSize, ssboUsage);
	BufferWithMemory	ssbo			(vkd, device, alloc, ssboCreateInfo, MemoryRequirement::HostVisible);
	auto&				ssboAlloc		= ssbo.getAllocation();
	void*				ssboData		= ssboAlloc.getHostPtr();
	const auto			ssboDescInfo	= makeDescriptorBufferInfo(ssbo.get(), 0ull, ssboSize);

	// Zero-out SSBO.
	deMemset(ssboData, 0, static_cast<size_t>(ssboSize));
	flushAlloc(vkd, device, ssboAlloc);

	// Descriptor set layout, pool, set and set update.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(ssboDescType, VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(ssboDescType);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), ssboDescType, &ssboDescInfo);
	updateBuilder.update(vkd, device);

	// Pipeline layout, render pass and pipeline.
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());
	const auto renderPass		= makeRenderPass(vkd, device);
	const auto framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), 0u, nullptr, fbExtent.width, fbExtent.height);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	meshShader	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragShader	= createShaderModule(vkd, device, binaries.get("frag"));
	const auto	pipeline	= makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		DE_NULL, meshShader.get(), fragShader.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u));
	vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, ssboAlloc);
	std::vector<uint32_t> outputFlags(m_shaderPrimitives, 0u);
	deMemcpy(outputFlags.data(), ssboData, de::dataSize(outputFlags));

	// Verify output SSBO.
	bool pass = true;
	auto& log = m_context.getTestContext().getLog();

	for (size_t i = 0u; i < outputFlags.size(); ++i)
	{
		if (outputFlags[i] != 1u)
		{
			std::ostringstream msg;
			msg << "Primitive ID " << i << " flag != 1: " << outputFlags[i];
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
			pass = false;
		}
	}

	if (!pass)
		TCU_FAIL("Check log for details");

	return tcu::TestStatus::pass("Pass");
}

class MaxMeshOutputComponentsCase : public vkt::TestCase
{
public:
					MaxMeshOutputComponentsCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description)
						: vkt::TestCase	(testCtx, name, description)
						{}

	virtual			~MaxMeshOutputComponentsCase	(void) {}

	void			initPrograms					(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance					(Context& context) const override;
	void			checkSupport					(Context& context) const override;

protected:
	struct ParamsFromContext
	{
		uint32_t maxLocations;
	};
	ParamsFromContext getParamsFromContext (Context& context) const;
};

class  MaxMeshOutputComponentsInstance : public SpecConstantInstance
{
public:
						MaxMeshOutputComponentsInstance		(Context& context, SpecConstVector&& scVector)
							: SpecConstantInstance(context, std::move(scVector))
							{}

	virtual				~MaxMeshOutputComponentsInstance	(void) {}

	tcu::TestStatus		iterate								(void) override;
};

MaxMeshOutputComponentsCase::ParamsFromContext MaxMeshOutputComponentsCase::getParamsFromContext (Context& context) const
{
	const uint32_t kLocationComponents	= 4u; // Each location can handle up to 4 32-bit components (and we'll be using uvec4).
	const uint32_t kUsedLocations		= 1u; // For gl_Position.
	const uint32_t maxLocations			= context.getMeshShaderPropertiesEXT().maxMeshOutputComponents / kLocationComponents - kUsedLocations;

	ParamsFromContext params { maxLocations };
	return params;
}

void MaxMeshOutputComponentsCase::checkSupport (Context &context) const
{
	checkTaskMeshShaderSupportEXT(context, false/*requireTask*/, true/*requireMesh*/);
}

TestInstance* MaxMeshOutputComponentsCase::createInstance (Context &context) const
{
	const auto		ctxParams		= getParamsFromContext(context);
	SpecConstVector	specConstVec	{ ctxParams.maxLocations };

	return new MaxMeshOutputComponentsInstance(context, std::move(specConstVec));
}

void MaxMeshOutputComponentsCase::initPrograms (vk::SourceCollections &programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	const std::string locationStructDecl =
		"layout (constant_id=0) const uint maxLocations = 1u;\n"
		"struct LocationStruct {\n"
		"    uvec4 location_var[maxLocations];\n"
		"};\n"
		;

	const std::string declOut =
		locationStructDecl +
		"layout (location=0) perprimitiveEXT flat out LocationStruct ls[];\n"
		;

	const std::string declIn =
		locationStructDecl +
		"layout (location=0) perprimitiveEXT flat in LocationStruct ls;\n"
		;

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "out gl_MeshPerVertexEXT {\n"
		<< "    vec4  gl_Position;\n"
		<< "} gl_MeshVerticesEXT[];\n"
		<< "\n"
		<< declOut
		<< "\n"
		<< "void main (void) {\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4( 0.0, -0.5, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(-0.5,  0.5, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4( 0.5,  0.5, 0.0, 1.0);\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		<< "\n"
		<< "    for (uint i = 0u; i < maxLocations; ++i) {\n"
		<< "        const uint baseVal = 10000u * (i + 1u);\n"
		<< "        const uvec4 expectedValue = uvec4(baseVal + 1u, baseVal + 2u, baseVal + 3u, baseVal + 4u);\n"
		<< "        ls[0].location_var[i] = expectedValue;\n"
		<< "    }\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< declIn
		<< "\n"
		<< "void main (void) {\n"
		<< "    bool success = true;\n"
		<< "    for (uint i = 0u; i < maxLocations; ++i) {\n"
		<< "        const uint baseVal = 10000u * (i + 1u);\n"
		<< "        const uvec4 expectedValue = uvec4(baseVal + 1u, baseVal + 2u, baseVal + 3u, baseVal + 4u);\n"
		<< "        success = success && (ls.location_var[i] == expectedValue);\n"
		<< "    }\n"
		<< "    outColor = (success ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0));\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
}

tcu::TestStatus MaxMeshOutputComponentsInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();

	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuColorFormat	= mapVkFormat(colorFormat);
	const auto			pixelSize		= static_cast<uint32_t>(tcu::getPixelSize(tcuColorFormat));
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			fbExtent		= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	iExtent3D		(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(fbExtent.depth));
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4		expectedColor	(0.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4		colorThreshold	(0.0f, 0.0f, 0.0f, 0.0f);

	// Create color attachment.
	const VkImageCreateInfo colorAttachmentCreatInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		fbExtent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorAttachment		(vkd, device, alloc, colorAttachmentCreatInfo, MemoryRequirement::Any);
	const auto		colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorAttachmentView	= makeImageView(vkd, device, colorAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Verification buffer for the color attachment.
	DE_ASSERT(fbExtent.depth == 1u);
	const auto			verificationBufferUsage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto			verificationBufferSize			= static_cast<VkDeviceSize>(pixelSize * fbExtent.width * fbExtent.height * fbExtent.depth);
	const auto			verificationBufferCreateInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);
	BufferWithMemory	verificationBuffer				(vkd, device, alloc, verificationBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc			= verificationBuffer.getAllocation();
	void*				verificationBufferData			= verificationBufferAlloc.getHostPtr();

	deMemset(verificationBufferData, 0, static_cast<size_t>(verificationBufferSize));

	const auto	pipelineLayout	= makePipelineLayout(vkd, device);
	const auto	renderPass		= makeRenderPass(vkd, device, colorFormat);
	const auto	framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), fbExtent.width, fbExtent.height, 1u);

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto					specMap		= makeSpecializationMap();
	const VkSpecializationInfo	specInfo	=
	{
		static_cast<uint32_t>(specMap.size()),	//	uint32_t						mapEntryCount;
		de::dataOrNull(specMap),				//	const VkSpecializationMapEntry*	pMapEntries;
		de::dataSize(m_specConstants),			//	size_t							dataSize;
		de::dataOrNull(m_specConstants),		//	const void*						pData;
	};

	std::vector<VkPipelineShaderStageCreateInfo>	shaderStages;
	VkPipelineShaderStageCreateInfo					stageInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,						//	VkShaderStageFlagBits				stage;
		DE_NULL,												//	VkShaderModule						module;
		"main",													//	const char*							pName;
		&specInfo,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	{
		stageInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
		stageInfo.module = meshModule.get();
		shaderStages.push_back(stageInfo);
	}

	{
		stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stageInfo.module = fragModule.get();
		shaderStages.push_back(stageInfo);
	}

	const auto pipeline = makeGraphicsPipeline(vkd, device,
		DE_NULL, pipelineLayout.get(), 0u,
		shaderStages, renderPass.get(), viewports, scissors);

	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);

	const auto preTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAttachment.get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

	const auto copyRegion = makeBufferImageCopy(fbExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

	const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, verificationBufferAlloc);
	tcu::ConstPixelBufferAccess resultAccess (tcuColorFormat, iExtent3D, verificationBufferData);

	auto& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::Message << "maxLocations value: " << m_specConstants.at(0u) << tcu::TestLog::EndMessage;
	if (!tcu::floatThresholdCompare(log, "Result", "", expectedColor, resultAccess, colorThreshold, tcu::COMPARE_LOG_ON_ERROR))
		TCU_FAIL("Check log for details");

	return tcu::TestStatus::pass("Pass");
}

class MeshPayloadShMemSizeCase : public vkt::TestCase
{
public:
					MeshPayloadShMemSizeCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const MeshPayloadShMemSizeParams& params)
						: vkt::TestCase				(testCtx, name, description)
						, m_params					(params)
						{}
	virtual			~MeshPayloadShMemSizeCase	(void) {}

	void			checkSupport				(Context& context) const override;
	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;

protected:
	struct ParamsFromContext
	{
		uint32_t payloadElements;
		uint32_t sharedMemoryElements;
	};
	ParamsFromContext getParamsFromContext		(Context& context) const;

	const MeshPayloadShMemSizeParams			m_params;

	static constexpr uint32_t kElementSize		= static_cast<uint32_t>(sizeof(uint32_t));
	static constexpr uint32_t kLocalInvocations	= 128u;
};

void MeshPayloadShMemSizeCase::checkSupport (Context& context) const
{
	const bool requireTask = m_params.hasPayload();

	checkTaskMeshShaderSupportEXT(context, requireTask, true/*requireMesh*/);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

	const auto&	meshProperties	= context.getMeshShaderPropertiesEXT();
	const auto	minSize			= kLocalInvocations * kElementSize;

	// Note: the min required values for these properties in the spec would pass these checks.

	if (requireTask)
	{
		if (meshProperties.maxTaskPayloadSize < minSize)
			TCU_FAIL("Invalid maxTaskPayloadSize");

		if (meshProperties.maxTaskPayloadAndSharedMemorySize < minSize)
			TCU_FAIL("Invalid maxTaskPayloadAndSharedMemorySize");
	}

	if (meshProperties.maxMeshSharedMemorySize < minSize)
		TCU_FAIL("Invalid maxMeshSharedMemorySize");

	if (meshProperties.maxMeshPayloadAndSharedMemorySize < minSize)
		TCU_FAIL("Invalid maxMeshPayloadAndSharedMemorySize");

	if (meshProperties.maxMeshPayloadAndOutputMemorySize < minSize)
		TCU_FAIL("Invalid maxMeshPayloadAndOutputMemorySize");
}

MeshPayloadShMemSizeCase::ParamsFromContext MeshPayloadShMemSizeCase::getParamsFromContext (Context& context) const
{
	ParamsFromContext params;

	const auto&	meshProperties		= context.getMeshShaderPropertiesEXT();
	const auto	maxTaskPayloadSize	= std::min(meshProperties.maxTaskPayloadAndSharedMemorySize, meshProperties.maxTaskPayloadSize);
	const auto	maxMeshPayloadSize	= std::min(meshProperties.maxMeshPayloadAndOutputMemorySize, meshProperties.maxMeshPayloadAndSharedMemorySize);
	const auto	maxPayloadElements	= std::min(maxTaskPayloadSize, maxMeshPayloadSize) / kElementSize;
	const auto	maxShMemElements	= meshProperties.maxMeshSharedMemorySize / kElementSize;
	const auto	maxTotalElements	= meshProperties.maxTaskPayloadAndSharedMemorySize / kElementSize;

	if (m_params.testType == PayLoadShMemSizeType::PAYLOAD)
	{
		params.sharedMemoryElements	= 0u;
		params.payloadElements		= std::min(maxTotalElements, maxPayloadElements);
	}
	else if (m_params.testType == PayLoadShMemSizeType::SHARED_MEMORY)
	{
		params.payloadElements		= 0u;
		params.sharedMemoryElements	= std::min(maxTotalElements, maxShMemElements);
	}
	else
	{
		uint32_t*	minPtr;
		uint32_t	minVal;
		uint32_t*	maxPtr;
		uint32_t	maxVal;

		// Divide them as evenly as possible getting them as closest as possible to maxTotalElements.
		if (maxPayloadElements < maxShMemElements)
		{
			minPtr = &params.payloadElements;
			minVal = maxPayloadElements;

			maxPtr = &params.sharedMemoryElements;
			maxVal = maxShMemElements;
		}
		else
		{
			minPtr = &params.sharedMemoryElements;
			minVal = maxShMemElements;

			maxPtr = &params.payloadElements;
			maxVal = maxPayloadElements;
		}

		*minPtr = std::min(minVal, maxTotalElements / 2u);
		*maxPtr = std::min(maxTotalElements - (*minPtr), maxVal);
	}

	return params;
}

TestInstance* MeshPayloadShMemSizeCase::createInstance (Context &context) const
{
	const auto		ctxParams	= getParamsFromContext(context);
	SpecConstVector	vec			{ ctxParams.payloadElements, ctxParams.sharedMemoryElements };

	return new PayloadShMemSizeInstance(context, m_params, std::move(vec));
}

void MeshPayloadShMemSizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	const std::string scDecl =
		"layout (constant_id=0) const uint payloadElements = 1u;\n"
		"layout (constant_id=1) const uint sharedMemoryElements = 1u;\n"
		;

	const std::string dsDecl =
		"layout (set=0, binding=0, std430) buffer ResultBlock {\n"
		"    uint sharedOK;\n"
		"    uint payloadOK;\n"
		"} result;\n"
		;

	std::string taskData;
	std::string taskPayloadBody;
	std::string meshPayloadBody;

	if (m_params.hasPayload())
	{
		std::ostringstream taskDataStream;
		taskDataStream
			<< "struct TaskData {\n"
			<< "    uint elements[payloadElements];\n"
			<< "};\n"
			<< "taskPayloadSharedEXT TaskData td;\n"
			;
		taskData = taskDataStream.str();

		std::ostringstream taskBodyStream;
		taskBodyStream
			<< "    const uint payloadElementsPerInvocation = uint(ceil(float(payloadElements) / float(" << kLocalInvocations << ")));\n"
			<< "    for (uint i = 0u; i < payloadElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = payloadElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < payloadElements) {\n"
			<< "            td.elements[elemIdx] = elemIdx + 2000u;\n"
			<< "        }\n"
			<< "    }\n"
			<< "\n"
			;
		taskPayloadBody = taskBodyStream.str();

		std::ostringstream meshBodyStream;
		meshBodyStream
			<< "    if (gl_LocalInvocationIndex == 0u) {\n"
			<< "        bool allOK = true;\n"
			<< "        for (uint i = 0u; i < payloadElements; ++i) {\n"
			<< "            if (td.elements[i] != i + 2000u) {\n"
			<< "                allOK = false;\n"
			<< "                break;\n"
			<< "            }\n"
			<< "        }\n"
			<< "        result.payloadOK = (allOK ? 1u : 0u);\n"
			<< "    }\n"
			<< "\n"
			;
		meshPayloadBody = meshBodyStream.str();
	}
	else
	{
		meshPayloadBody = "    result.payloadOK = 1u;\n";
	}

	std::string sharedData;
	std::string meshSharedDataBody;

	if (m_params.hasSharedMemory())
	{
		sharedData = "shared uint sharedElements[sharedMemoryElements];\n";

		std::ostringstream bodyStream;
		bodyStream
			<< "    const uint shMemElementsPerInvocation = uint(ceil(float(sharedMemoryElements) / float(" << kLocalInvocations << ")));\n"
			<< "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < sharedMemoryElements) {\n"
			<< "            sharedElements[elemIdx] = elemIdx * 2u + 1000u;\n" // Write
			<< "        }\n"
			<< "    }\n"
			<< "    memoryBarrierShared();\n"
			<< "    barrier();\n"
			<< "    for (uint i = 0u; i < shMemElementsPerInvocation; ++i) {\n"
			<< "        const uint elemIdx = shMemElementsPerInvocation * gl_LocalInvocationIndex + i;\n"
			<< "        if (elemIdx < sharedMemoryElements) {\n"
			<< "            const uint accessIdx = sharedMemoryElements - 1u - elemIdx;\n"
			<< "            sharedElements[accessIdx] += accessIdx;\n" // Read+Write a different element.
			<< "        }\n"
			<< "    }\n"
			<< "    memoryBarrierShared();\n"
			<< "    barrier();\n"
			<< "    if (gl_LocalInvocationIndex == 0u) {\n"
			<< "        bool allOK = true;\n"
			<< "        for (uint i = 0u; i < sharedMemoryElements; ++i) {\n"
			<< "            if (sharedElements[i] != i*3u + 1000u) {\n"
			<< "                allOK = false;\n"
			<< "                break;\n"
			<< "            }\n"
			<< "        }\n"
			<< "        result.sharedOK = (allOK ? 1u : 0u);\n"
			<< "    }\n"
			<< "\n"
			;
		meshSharedDataBody = bodyStream.str();
	}
	else
	{
		meshSharedDataBody =
			"    if (gl_LocalInvocationIndex == 0u) {\n"
			"        result.sharedOK = 1u;\n"
			"    }\n"
			;
	}

	if (m_params.hasPayload())
	{
		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
			<< scDecl
			<< dsDecl
			<< taskData
			<< "\n"
			<< "void main () {\n"
			<< taskPayloadBody
			<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< scDecl
		<< dsDecl
		<< taskData
		<< sharedData
		<< "\n"
		<< "void main () {\n"
		<< meshSharedDataBody
		<< meshPayloadBody
		<< "    SetMeshOutputsEXT(0u, 0u);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

enum class LocationType
{
	PER_VERTEX,
	PER_PRIMITIVE,
};

enum class ViewIndexType
{
	NO_VIEW_INDEX,
	VIEW_INDEX_FRAG,
	VIEW_INDEX_BOTH,
};

struct MaxMeshOutputParams
{
	bool			usePayload;
	LocationType	locationType;
	ViewIndexType	viewIndexType;

	bool isMultiView (void) const
	{
		return (viewIndexType != ViewIndexType::NO_VIEW_INDEX);
	}

	bool viewIndexInMesh (void) const
	{
		return (viewIndexType == ViewIndexType::VIEW_INDEX_BOTH);
	}
};

class MaxMeshOutputSizeCase : public vkt::TestCase
{
public:
					MaxMeshOutputSizeCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const MaxMeshOutputParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~MaxMeshOutputSizeCase	(void) {}

	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;

	// Small-ish numbers allow for more fine-grained control in the amount of memory, but it can't be too small or we hit the locations limit.
	static constexpr uint32_t				kMaxPoints			= 96u;
	static constexpr uint32_t				kNumViews			= 2u;	// For the multiView case.

protected:
	static constexpr uint32_t				kUvec4Size			= 16u;	// We'll use 4 scalars at a time in the form of a uvec4.
	static constexpr uint32_t				kUvec4Comp			= 4u;	// 4 components per uvec4.
	static constexpr uint32_t				kPayloadElementSize	= 4u;	// Each payload element will be a uint.

	struct ParamsFromContext
	{
		uint32_t payloadElements;
		uint32_t locationCount;
	};
	ParamsFromContext getParamsFromContext	(Context& context) const;

	const MaxMeshOutputParams				m_params;
};

class MaxMeshOutputSizeInstance : public SpecConstantInstance
{
public:
						MaxMeshOutputSizeInstance	(Context& context, SpecConstVector&& vec, uint32_t numViews)
							: SpecConstantInstance	(context, std::move(vec))
							, m_numViews			(numViews)
							{}
	virtual				~MaxMeshOutputSizeInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	Move<VkRenderPass>	makeCustomRenderPass		(const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format);

	const uint32_t		m_numViews;
};

void MaxMeshOutputSizeCase::checkSupport (Context &context) const
{
	checkTaskMeshShaderSupportEXT(context, m_params.usePayload/*requireTask*/, true/*requireMesh*/);

	if (m_params.isMultiView())
	{
		const auto& multiviewFeatures = context.getMultiviewFeatures();
		if (!multiviewFeatures.multiview)
			TCU_THROW(NotSupportedError, "Multiview not supported");

		const auto& meshFeatures = context.getMeshShaderFeaturesEXT();
		if (!meshFeatures.multiviewMeshShader)
			TCU_THROW(NotSupportedError, "Multiview not supported for mesh shaders");

		const auto& meshProperties = context.getMeshShaderPropertiesEXT();
		if (meshProperties.maxMeshMultiviewViewCount < kNumViews)
			TCU_THROW(NotSupportedError, "maxMeshMultiviewViewCount too low");
	}
}

MaxMeshOutputSizeCase::ParamsFromContext MaxMeshOutputSizeCase::getParamsFromContext (Context& context) const
{
	const auto&	meshProperties		= context.getMeshShaderPropertiesEXT();
	const auto	maxOutSize			= std::min(meshProperties.maxMeshOutputMemorySize, meshProperties.maxMeshPayloadAndOutputMemorySize);
	const auto	maxMeshPayloadSize	= std::min(meshProperties.maxMeshPayloadAndSharedMemorySize, meshProperties.maxMeshPayloadAndOutputMemorySize);
	const auto	maxTaskPayloadSize	= std::min(meshProperties.maxTaskPayloadSize, meshProperties.maxTaskPayloadAndSharedMemorySize);
	const auto	maxPayloadSize		= std::min(maxMeshPayloadSize, maxTaskPayloadSize);
	const auto	numViewFactor		= (m_params.viewIndexInMesh() ? kNumViews : 1u);

	uint32_t payloadSize;
	uint32_t outSize;

	if (m_params.usePayload)
	{
		const auto totalMax = maxOutSize + maxPayloadSize;

		if (totalMax <= meshProperties.maxMeshPayloadAndOutputMemorySize)
		{
			payloadSize	= maxPayloadSize;
			outSize		= maxOutSize;
		}
		else
		{
			payloadSize	= maxPayloadSize;
			outSize		= meshProperties.maxMeshPayloadAndOutputMemorySize - payloadSize;
		}
	}
	else
	{
		payloadSize	= 0u;
		outSize		= maxOutSize;
	}

	// This uses the equation in "Mesh Shader Output" spec section. Note per-vertex data already has gl_Position and gl_PointSize.
	// Also note gl_PointSize uses 1 effective location (4 scalar components) despite being a float.
	const auto granularity			= ((m_params.locationType == LocationType::PER_PRIMITIVE)
									? meshProperties.meshOutputPerPrimitiveGranularity
									: meshProperties.meshOutputPerVertexGranularity);
	const auto actualPoints			= de::roundUp(kMaxPoints, granularity);
	const auto sizeMultiplier		= actualPoints * kUvec4Size;
	const auto builtinDataSize		= (16u/*gl_Position*/ + 16u/*gl_PointSize*/) * actualPoints;
	const auto locationsDataSize	= (outSize - builtinDataSize) / numViewFactor;
	const auto maxTotalLocations	= meshProperties.maxMeshOutputComponents / kUvec4Comp - 2u; // gl_Position and gl_PointSize use 1 location each.
	const auto locationCount		= std::min(locationsDataSize / sizeMultiplier, maxTotalLocations);

	ParamsFromContext params;
	params.payloadElements	= payloadSize / kPayloadElementSize;
	params.locationCount	= locationCount;

	auto& log = context.getTestContext().getLog();
	{
		const auto actualOuputSize = builtinDataSize + locationCount * sizeMultiplier * numViewFactor;

		log << tcu::TestLog::Message << "Payload elements: " << params.payloadElements << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Location count: " << params.locationCount << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Max mesh payload and output size (bytes): " << meshProperties.maxMeshPayloadAndOutputMemorySize << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Max output size (bytes): " << maxOutSize << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Payload size (bytes): " << payloadSize << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Output data size (bytes): " << actualOuputSize << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Output + payload size (bytes): " << (payloadSize + actualOuputSize) << tcu::TestLog::EndMessage;
	}

	return params;
}

TestInstance* MaxMeshOutputSizeCase::createInstance (Context &context) const
{
	const auto		ctxParams		= getParamsFromContext(context);
	SpecConstVector	specConstVec	{ ctxParams.payloadElements, ctxParams.locationCount };
	const auto		numViews		= (m_params.isMultiView() ? kNumViews : 1u);

	return new MaxMeshOutputSizeInstance(context, std::move(specConstVec), numViews);
}

void MaxMeshOutputSizeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto			buildOptions		= getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
	const std::string	locationQualifier	= ((m_params.locationType == LocationType::PER_PRIMITIVE) ? "perprimitiveEXT" : "");
	const std::string	multiViewExtDecl	= "#extension GL_EXT_multiview : enable\n";

	const std::string scDecl =
		"layout (constant_id=0) const uint payloadElements = 1u;\n"
		"layout (constant_id=1) const uint locationCount = 1u;\n"
		;

	std::string taskPayload;
	std::string payloadVerification	= "    bool payloadOK = true;\n";
	std::string locStruct			=
		"struct LocationBlock {\n"
		"    uvec4 elements[locationCount];\n"
		"};\n"
		;

	if (m_params.usePayload)
	{
		taskPayload =
			"struct TaskData {\n"
			"    uint elements[payloadElements];\n"
			"};\n"
			"taskPayloadSharedEXT TaskData td;\n"
			;

		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< scDecl
			<< taskPayload
			<< "\n"
			<< "void main (void) {\n"
			<< "    for (uint i = 0; i < payloadElements; ++i) {\n"
			<< "        td.elements[i] = 1000000u + i;\n"
			<< "    }\n"
			<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

		payloadVerification +=
			"    for (uint i = 0; i < payloadElements; ++i) {\n"
			"        if (td.elements[i] != 1000000u + i) {\n"
			"            payloadOK = false;\n"
			"            break;\n"
			"        }\n"
			"    }\n"
			;
	}

	// Do values depend on view indices?
	const bool			valFromViewIndex	= m_params.viewIndexInMesh();
	const std::string	extraCompOffset		= (valFromViewIndex ? "(4u * uint(gl_ViewIndex))" : "0u");

	{
		const std::string multiViewExt = (valFromViewIndex ? multiViewExtDecl : "");

		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< multiViewExt
			<< "\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "layout (points) out;\n"
			<< "layout (max_vertices=" << kMaxPoints << ", max_primitives=" << kMaxPoints << ") out;\n"
			<< "\n"
			<< "out gl_MeshPerVertexEXT {\n"
			<< "    vec4  gl_Position;\n"
			<< "    float gl_PointSize;\n"
			<< "} gl_MeshVerticesEXT[];\n"
			<< "\n"
			<< scDecl
			<< taskPayload
			<< "\n"
			<< locStruct
			<< "layout (location=0) out " << locationQualifier << " LocationBlock loc[];\n"
			<< "\n"
			<< "void main (void) {\n"
			<< payloadVerification
			<< "\n"
			<< "    SetMeshOutputsEXT(" << kMaxPoints << ", " << kMaxPoints << ");\n"
			<< "    const uint payloadOffset = (payloadOK ? 10u : 0u);\n"
			<< "    const uint compOffset = " << extraCompOffset << ";\n"
			<< "    for (uint pointIdx = 0u; pointIdx < " << kMaxPoints << "; ++pointIdx) {\n"
			<< "        const float xCoord = ((float(pointIdx) + 0.5) / float(" << kMaxPoints << ")) * 2.0 - 1.0;\n"
			<< "        gl_MeshVerticesEXT[pointIdx].gl_Position = vec4(xCoord, 0.0, 0.0, 1.0);\n"
			<< "        gl_MeshVerticesEXT[pointIdx].gl_PointSize = 1.0f;\n"
			<< "        gl_PrimitivePointIndicesEXT[pointIdx] = pointIdx;\n"
			<< "        for (uint elemIdx = 0; elemIdx < locationCount; ++elemIdx) {\n"
			<< "            const uint baseVal = 200000000u + 100000u * pointIdx + 1000u * elemIdx + payloadOffset;\n"
			<< "            loc[pointIdx].elements[elemIdx] = uvec4(baseVal + 1u + compOffset, baseVal + 2u + compOffset, baseVal + 3u + compOffset, baseVal + 4u + compOffset);\n"
			<< "        }\n"
			<< "    }\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
	}

	{
		const std::string multiViewExt	= (m_params.isMultiView() ? multiViewExtDecl							: "");
		const std::string outColorMod	= (m_params.isMultiView() ? "    outColor.r += float(gl_ViewIndex);\n"	: "");

		std::ostringstream frag;
		frag
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< multiViewExt
			<< "\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< scDecl
			<< locStruct
			<< "layout (location=0) in flat " << locationQualifier << " LocationBlock loc;\n"
			<< "\n"
			<< "void main (void) {\n"
			<< "    bool pointOK = true;\n"
			<< "    const uint pointIdx = uint(gl_FragCoord.x);\n"
			<< "    const uint expectedPayloadOffset = 10u;\n"
			<< "    const uint compOffset = " << extraCompOffset << ";\n"
			<< "    for (uint elemIdx = 0; elemIdx < locationCount; ++elemIdx) {\n"
			<< "        const uint baseVal = 200000000u + 100000u * pointIdx + 1000u * elemIdx + expectedPayloadOffset;\n"
			<< "        const uvec4 expectedVal = uvec4(baseVal + 1u + compOffset, baseVal + 2u + compOffset, baseVal + 3u + compOffset, baseVal + 4u + compOffset);\n"
			<< "        if (loc.elements[elemIdx] != expectedVal) {\n"
			<< "            pointOK = false;\n"
			<< "            break;\n"
			<< "        }\n"
			<< "    }\n"
			<< "    const vec4 okColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
			<< "    const vec4 failColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "    outColor = (pointOK ? okColor : failColor);\n"
			<< outColorMod
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;
	}
}

Move<VkRenderPass> MaxMeshOutputSizeInstance::makeCustomRenderPass (const DeviceInterface& vkd, VkDevice device, uint32_t layerCount, VkFormat format)
{
	DE_ASSERT(layerCount > 0u);

	const VkAttachmentDescription colorAttachmentDescription =
	{
		0u,											// VkAttachmentDescriptionFlags    flags
		format,										// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference colorAttachmentRef = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const VkSubpassDescription subpassDescription =
	{
		0u,									// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		nullptr,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		nullptr,							// const VkAttachmentReference*    pResolveAttachments
		nullptr,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		nullptr								// const deUint32*                 pPreserveAttachments
	};

	const uint32_t viewMask = ((1u << layerCount) - 1u);
	const VkRenderPassMultiviewCreateInfo multiviewCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		1u,														//	uint32_t		subpassCount;
		&viewMask,												//	const uint32_t*	pViewMasks;
		0u,														//	uint32_t		dependencyCount;
		nullptr,												//	const int32_t*	pViewOffsets;
		1u,														//	uint32_t		correlationMaskCount;
		&viewMask,												//	const uint32_t*	pCorrelationMasks;
	};

	const void* pNext = ((layerCount > 1u) ? &multiviewCreateInfo : nullptr);

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType                   sType
		pNext,													// const void*                       pNext
		0u,														// VkRenderPassCreateFlags           flags
		1u,														// deUint32                          attachmentCount
		&colorAttachmentDescription,							// const VkAttachmentDescription*    pAttachments
		1u,														// deUint32                          subpassCount
		&subpassDescription,									// const VkSubpassDescription*       pSubpasses
		0u,														// deUint32                          dependencyCount
		nullptr,												// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vkd, device, &renderPassInfo);
}

tcu::TestStatus MaxMeshOutputSizeInstance::iterate (void)
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();

	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuColorFormat	= mapVkFormat(colorFormat);
	const auto			pixelSize		= static_cast<uint32_t>(tcu::getPixelSize(tcuColorFormat));
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			imageViewType	= ((m_numViews > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	const auto			fbExtent		= makeExtent3D(MaxMeshOutputSizeCase::kMaxPoints, 1u, 1u);
	const tcu::IVec3	iExtent3D		(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(m_numViews));
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4		expectedColor	(0.0f, 0.0f, 1.0f, 1.0f);
	const tcu::Vec4		colorThreshold	(0.0f, 0.0f, 0.0f, 0.0f);

	// Create color attachment.
	const VkImageCreateInfo colorAttachmentCreatInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		fbExtent,								//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		m_numViews,								//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorAttachment		(vkd, device, alloc, colorAttachmentCreatInfo, MemoryRequirement::Any);
	const auto		colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_numViews);
	const auto		colorSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_numViews);
	const auto		colorAttachmentView	= makeImageView(vkd, device, colorAttachment.get(), imageViewType, colorFormat, colorSRR);

	// Verification buffer for the color attachment.
	DE_ASSERT(fbExtent.depth == 1u);
	const auto			verificationBufferUsage			= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto			verificationBufferSize			= static_cast<VkDeviceSize>(pixelSize * fbExtent.width * fbExtent.height * m_numViews);
	const auto			verificationBufferCreateInfo	= makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);
	BufferWithMemory	verificationBuffer				(vkd, device, alloc, verificationBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc			= verificationBuffer.getAllocation();
	void*				verificationBufferData			= verificationBufferAlloc.getHostPtr();

	deMemset(verificationBufferData, 0, static_cast<size_t>(verificationBufferSize));

	const auto	pipelineLayout	= makePipelineLayout(vkd, device);
	const auto	renderPass		= makeCustomRenderPass(vkd, device, m_numViews, colorFormat);
	const auto	framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), colorAttachmentView.get(), fbExtent.width, fbExtent.height, 1u);

	const auto&	binaries	= m_context.getBinaryCollection();
	const bool	hasTask		= binaries.contains("task");
	const auto	taskModule	= (hasTask ? createShaderModule(vkd, device, binaries.get("task")) : Move<VkShaderModule>());
	const auto	meshModule	= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule	= createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto					specMap		= makeSpecializationMap();
	const VkSpecializationInfo	specInfo	=
	{
		static_cast<uint32_t>(specMap.size()),	//	uint32_t						mapEntryCount;
		de::dataOrNull(specMap),				//	const VkSpecializationMapEntry*	pMapEntries;
		de::dataSize(m_specConstants),			//	size_t							dataSize;
		de::dataOrNull(m_specConstants),		//	const void*						pData;
	};

	std::vector<VkPipelineShaderStageCreateInfo>	shaderStages;
	VkPipelineShaderStageCreateInfo					stageInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,						//	VkShaderStageFlagBits				stage;
		DE_NULL,												//	VkShaderModule						module;
		"main",													//	const char*							pName;
		&specInfo,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	if (hasTask)
	{
		stageInfo.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
		stageInfo.module = taskModule.get();
		shaderStages.push_back(stageInfo);
	}

	{
		stageInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
		stageInfo.module = meshModule.get();
		shaderStages.push_back(stageInfo);
	}

	{
		stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stageInfo.module = fragModule.get();
		shaderStages.push_back(stageInfo);
	}

	const auto pipeline = makeGraphicsPipeline(vkd, device,
		DE_NULL, pipelineLayout.get(), 0u,
		shaderStages, renderPass.get(), viewports, scissors);

	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	endRenderPass(vkd, cmdBuffer);

	const auto preTransferBarrier = makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAttachment.get(), colorSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

	const auto copyRegion = makeBufferImageCopy(fbExtent, colorSRL);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

	const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, verificationBufferAlloc);
	tcu::ConstPixelBufferAccess resultAccess	(tcuColorFormat, iExtent3D, verificationBufferData);
	tcu::TextureLevel			referenceLevel	(tcuColorFormat, iExtent3D.x(), iExtent3D.y(), iExtent3D.z());
	tcu::PixelBufferAccess		referenceAccess = referenceLevel.getAccess();

	for (int z = 0; z < iExtent3D.z(); ++z)
	{
		const auto layer = tcu::getSubregion(referenceAccess, 0, 0, z, iExtent3D.x(), iExtent3D.y(), 1);
		const tcu::Vec4 expectedLayerColor(static_cast<float>(z), expectedColor.y(), expectedColor.z(), expectedColor.w());
		tcu::clear(layer, expectedLayerColor);
	}

	auto& log = m_context.getTestContext().getLog();
	if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, colorThreshold, tcu::COMPARE_LOG_ON_ERROR))
		TCU_FAIL("Check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createMeshShaderPropertyTestsEXT (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "properties", "Tests checking mesh shading properties"));

	const struct
	{
		PayLoadShMemSizeType	testType;
		const char*				name;
	} taskPayloadShMemCases[] =
	{
		{ PayLoadShMemSizeType::PAYLOAD,		"task_payload_size"						},
		{ PayLoadShMemSizeType::SHARED_MEMORY,	"task_shared_memory_size"				},
		{ PayLoadShMemSizeType::BOTH,			"task_payload_and_shared_memory_size"	},
	};

	for (const auto& taskPayloadShMemCase : taskPayloadShMemCases)
	{
		const TaskPayloadShMemSizeParams params { taskPayloadShMemCase.testType };
		mainGroup->addChild(new TaskPayloadShMemSizeCase(testCtx, taskPayloadShMemCase.name, "", params));
	}

	mainGroup->addChild(new MaxViewIndexCase(testCtx, "max_view_index", ""));
	mainGroup->addChild(new MaxOutputLayersCase(testCtx, "max_output_layers", ""));

	const struct
	{
		MaxPrimVertType		limitPrimVertType;
		const char*			prefix;
	} limitPrimVertCases[] =
	{
		{ MaxPrimVertType::PRIMITIVES,	"max_mesh_output_primitives_"	},
		{ MaxPrimVertType::VERTICES,	"max_mesh_output_vertices_"		},
	};

	const uint32_t itemCounts[] = { 256u, 512u, 1024u, 2048u };

	for (const auto& primVertCase : limitPrimVertCases)
	{
		for (const auto& count : itemCounts)
		{
			const MaxPrimVertParams params { primVertCase.limitPrimVertType, count };
			mainGroup->addChild(new MaxMeshOutputPrimVertCase(testCtx, primVertCase.prefix + std::to_string(count), "", params));
		}
	}

	mainGroup->addChild(new MaxMeshOutputComponentsCase(testCtx, "max_mesh_output_components", ""));

	const struct
	{
		PayLoadShMemSizeType	testType;
		const char*				name;
	} meshPayloadShMemCases[] =
	{
		// No actual property for the first one, combines the two properties involving payload size.
		{ PayLoadShMemSizeType::PAYLOAD,		"mesh_payload_size"						},
		{ PayLoadShMemSizeType::SHARED_MEMORY,	"mesh_shared_memory_size"				},
		{ PayLoadShMemSizeType::BOTH,			"mesh_payload_and_shared_memory_size"	},
	};
	for (const auto& meshPayloadShMemCase : meshPayloadShMemCases)
	{
		const MeshPayloadShMemSizeParams params { meshPayloadShMemCase.testType };
		mainGroup->addChild(new MeshPayloadShMemSizeCase(testCtx, meshPayloadShMemCase.name, "", params));
	}

	const struct
	{
		bool			usePayload;
		const char*		suffix;
	} meshOutputPayloadCases[] =
	{
		{ false,	"_without_payload"	},
		{ true,		"_with_payload"		},
	};

	const struct
	{
		LocationType	locationType;
		const char*		suffix;
	} locationTypeCases[] =
	{
		{ LocationType::PER_PRIMITIVE,	"_per_primitive"	},
		{ LocationType::PER_VERTEX,		"_per_vertex"		},
	};

	const struct
	{
		ViewIndexType	viewIndexType;
		const char*		suffix;
	} multiviewCases[] =
	{
		{ ViewIndexType::NO_VIEW_INDEX,		"_no_view_index"				},
		{ ViewIndexType::VIEW_INDEX_FRAG,	"_view_index_in_frag"			},
		{ ViewIndexType::VIEW_INDEX_BOTH,	"_view_index_in_mesh_and_frag"	},
	};

	for (const auto& meshOutputPayloadCase : meshOutputPayloadCases)
	{
		for (const auto& locationTypeCase : locationTypeCases)
		{
			for (const auto& multiviewCase : multiviewCases)
			{
				const std::string			name	= std::string("max_mesh_output_size") + meshOutputPayloadCase.suffix + locationTypeCase.suffix + multiviewCase.suffix;
				const MaxMeshOutputParams	params	=
				{
					meshOutputPayloadCase.usePayload,	//	bool			usePayload;
					locationTypeCase.locationType,		//	LocationType	locationType;
					multiviewCase.viewIndexType,		//	ViewIndexType	viewIndexType;
				};

				mainGroup->addChild(new MaxMeshOutputSizeCase(testCtx, name, "", params));
			}
		}
	}

	return mainGroup.release();
}
} // MeshShader
} // vkt
