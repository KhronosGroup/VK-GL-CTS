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
 * \brief Mesh Shader Smoke Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderSmokeTests.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"

#include <utility>
#include <vector>
#include <string>
#include <sstream>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

void checkTaskMeshShaderSupport (Context& context, bool requireTask, bool requireMesh)
{
	context.requireDeviceFunctionality("VK_NV_mesh_shader");

	DE_ASSERT(requireTask || requireMesh);

	const auto& meshFeatures = context.getMeshShaderFeatures();

	if (requireTask && !meshFeatures.taskShader)
		TCU_THROW(NotSupportedError, "Task shader not supported");

	if (requireMesh && !meshFeatures.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shader not supported");
}

std::string commonMeshFragShader ()
{
	std::string frag =
		"#version 450\n"
		"#extension GL_NV_mesh_shader : enable\n"
		"\n"
		"layout (location=0) in perprimitiveNV vec4 triangleColor;\n"
		"layout (location=0) out vec4 outColor;\n"
		"\n"
		"void main ()\n"
		"{\n"
		"	outColor = triangleColor;\n"
		"}\n"
		;
	return frag;
}

struct MeshTriangleRendererParams
{
	std::vector<tcu::Vec4>	vertexCoords;
	std::vector<uint32_t>	vertexIndices;
	uint32_t				taskCount;
	tcu::Vec4				expectedColor;

	MeshTriangleRendererParams (std::vector<tcu::Vec4> vertexCoords_, std::vector<uint32_t>	vertexIndices_, uint32_t taskCount_, const tcu::Vec4& expectedColor_)
		: vertexCoords	(std::move(vertexCoords_))
		, vertexIndices	(std::move(vertexIndices_))
		, taskCount		(taskCount_)
		, expectedColor	(expectedColor_)
	{}

	MeshTriangleRendererParams (MeshTriangleRendererParams&& other)
		: MeshTriangleRendererParams (std::move(other.vertexCoords), std::move(other.vertexIndices), other.taskCount, other.expectedColor)
	{}
};

class MeshOnlyTriangleCase : public vkt::TestCase
{
public:
					MeshOnlyTriangleCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description) : vkt::TestCase (testCtx, name, description) {}
	virtual			~MeshOnlyTriangleCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;
};

class MeshTaskTriangleCase : public vkt::TestCase
{
public:
					MeshTaskTriangleCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description) : vkt::TestCase (testCtx, name, description) {}
	virtual			~MeshTaskTriangleCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;
};

// Note: not actually task-only. The task shader will not emit mesh shader work groups.
class TaskOnlyTriangleCase : public vkt::TestCase
{
public:
					TaskOnlyTriangleCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description) : vkt::TestCase (testCtx, name, description) {}
	virtual			~TaskOnlyTriangleCase	(void) {}

	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;
	void			checkSupport			(Context& context) const override;
};

class MeshTriangleRenderer : public vkt::TestInstance
{
public:
						MeshTriangleRenderer	(Context& context, MeshTriangleRendererParams params) : vkt::TestInstance(context), m_params(std::move(params)) {}
	virtual				~MeshTriangleRenderer	(void) {}

	tcu::TestStatus		iterate					(void) override;

protected:
	MeshTriangleRendererParams	m_params;
};

void MeshOnlyTriangleCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupport(context, false, true);
}

void MeshTaskTriangleCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupport(context, true, true);
}

void TaskOnlyTriangleCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupport(context, true, true);
}

void MeshOnlyTriangleCase::initPrograms (SourceCollections& dst) const
{
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		// We will actually output a single triangle and most invocations will do no work.
		<< "layout(local_size_x=32) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		// Unique vertex coordinates.
		<< "layout (set=0, binding=0) uniform CoordsBuffer {\n"
		<< "    vec4 coords[3];\n"
		<< "} cb;\n"
		// Unique vertex indices.
		<< "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
		<< "    uint indices[3];\n"
		<< "} ib;\n"
		<< "\n"
		// Triangle color.
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 1u;\n"
		<< "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "\n"
		<< "    const uint vertex = gl_LocalInvocationIndex;\n"
		<< "    if (vertex < 3u)\n"
		<< "    {\n"
		<< "        const uint vertexIndex = ib.indices[vertex];\n"
		<< "        gl_PrimitiveIndicesNV[vertex] = vertexIndex;\n"
		<< "        gl_MeshVerticesNV[vertexIndex].gl_Position = cb.coords[vertexIndex];\n"
		<< "    }\n"
		<< "}\n"
		;
	dst.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader());
}

void MeshTaskTriangleCase::initPrograms (SourceCollections& dst) const
{
	std::string taskDataDecl =
		"taskNV TaskData {\n"
		"	uint triangleIndex;\n"
		"} td;\n"
		;

	std::ostringstream task;
	task
		// Each work group spawns 1 task each (2 in total) and each task will draw 1 triangle.
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=32) in;\n"
		<< "\n"
		<< "out " << taskDataDecl
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    if (gl_LocalInvocationIndex == 0u)\n"
		<< "    {\n"
		<< "        gl_TaskCountNV = 1u;\n"
		<< "        td.triangleIndex = gl_WorkGroupID.x;\n"
		<< "    }\n"
		<< "}\n"
		;
	dst.glslSources.add("task") << glu::TaskSource(task.str());

	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		// We will actually output a single triangle and most invocations will do no work.
		<< "layout(local_size_x=32) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		// Unique vertex coordinates.
		<< "layout (set=0, binding=0) uniform CoordsBuffer {\n"
		<< "    vec4 coords[4];\n"
		<< "} cb;\n"
		// Unique vertex indices.
		<< "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
		<< "    uint indices[6];\n"
		<< "} ib;\n"
		<< "\n"
		// Triangle color.
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "in " << taskDataDecl
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    if (gl_LocalInvocationIndex == 0u)\n"
		<< "    {\n"
		<< "        gl_PrimitiveCountNV = 1u;\n"
		<< "        triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "    }\n"
		<< "\n"
		// Each "active" invocation will copy one vertex.
		<< "    if (gl_LocalInvocationIndex < 3u)\n"
		<< "    {\n"
		<< "\n"
		<< "        const uint triangleVertex = gl_LocalInvocationIndex;\n"
		<< "        const uint coordsIndex    = ib.indices[td.triangleIndex * 3u + triangleVertex];\n"
		<< "\n"
		// Copy vertex coordinates.
		<< "        gl_MeshVerticesNV[triangleVertex].gl_Position = cb.coords[coordsIndex];\n"
		// Index renumbering: final indices will always be 0, 1, 2.
		<< "        gl_PrimitiveIndicesNV[triangleVertex] = triangleVertex;\n"
		<< "    }\n"
		<< "}\n"
		;
	dst.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader());
}

void TaskOnlyTriangleCase::initPrograms (SourceCollections& dst) const
{
	// The task shader does not spawn any mesh shader invocations.
	std::ostringstream task;
	task
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=1) in;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_TaskCountNV = 0u;\n"
		<< "}\n"
		;
	dst.glslSources.add("task") << glu::TaskSource(task.str());

	// Same shader as the mesh only case, but it should not be launched.
	std::ostringstream mesh;
	mesh
		<< "#version 450\n"
		<< "#extension GL_NV_mesh_shader : enable\n"
		<< "\n"
		<< "layout(local_size_x=32) in;\n"
		<< "layout(triangles) out;\n"
		<< "layout(max_vertices=256, max_primitives=256) out;\n"
		<< "\n"
		<< "layout (set=0, binding=0) uniform CoordsBuffer {\n"
		<< "    vec4 coords[3];\n"
		<< "} cb;\n"
		<< "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
		<< "    uint indices[3];\n"
		<< "} ib;\n"
		<< "\n"
		<< "layout (location=0) out perprimitiveNV vec4 triangleColor[];\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    gl_PrimitiveCountNV = 1u;\n"
		<< "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "\n"
		<< "    const uint vertex = gl_LocalInvocationIndex;\n"
		<< "    if (vertex < 3u)\n"
		<< "    {\n"
		<< "        const uint vertexIndex = ib.indices[vertex];\n"
		<< "        gl_PrimitiveIndicesNV[vertex] = vertexIndex;\n"
		<< "        gl_MeshVerticesNV[vertexIndex].gl_Position = cb.coords[vertexIndex];\n"
		<< "    }\n"
		<< "}\n"
		;
	dst.glslSources.add("mesh") << glu::MeshSource(mesh.str());

	dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader());
}

TestInstance* MeshOnlyTriangleCase::createInstance (Context& context) const
{
	const std::vector<tcu::Vec4>	vertexCoords	=
	{
		tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4(-1.0f,  3.0f, 0.0f, 1.0f),
		tcu::Vec4( 3.0f, -1.0f, 0.0f, 1.0f),
	};
	const std::vector<uint32_t>		vertexIndices	= { 0u, 1u, 2u };
	MeshTriangleRendererParams		params			(std::move(vertexCoords), std::move(vertexIndices), 1u, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

	return new MeshTriangleRenderer(context, std::move(params));
}

TestInstance* MeshTaskTriangleCase::createInstance (Context& context) const
{
	const std::vector<tcu::Vec4>	vertexCoords	=
	{
		tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
		tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f),
	};
	const std::vector<uint32_t>		vertexIndices	= { 2u, 0u, 1u, 1u, 3u, 2u };
	MeshTriangleRendererParams		params			(std::move(vertexCoords), std::move(vertexIndices), 2u, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

	return new MeshTriangleRenderer(context, std::move(params));
}

TestInstance* TaskOnlyTriangleCase::createInstance (Context& context) const
{
	const std::vector<tcu::Vec4>	vertexCoords	=
	{
		tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		tcu::Vec4(-1.0f,  3.0f, 0.0f, 1.0f),
		tcu::Vec4( 3.0f, -1.0f, 0.0f, 1.0f),
	};
	const std::vector<uint32_t>		vertexIndices	= { 0u, 1u, 2u };
	// Note we expect the clear color.
	MeshTriangleRendererParams		params			(std::move(vertexCoords), std::move(vertexIndices), 1u, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	return new MeshTriangleRenderer(context, std::move(params));
}

tcu::TestStatus MeshTriangleRenderer::iterate ()
{
	const auto&		vkd					= m_context.getDeviceInterface();
	const auto		device				= m_context.getDevice();
	auto&			alloc				= m_context.getDefaultAllocator();
	const auto		qIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto		queue				= m_context.getUniversalQueue();

	const auto		vertexBufferStages	= VK_SHADER_STAGE_MESH_BIT_NV;
	const auto		vertexBufferSize	= static_cast<VkDeviceSize>(de::dataSize(m_params.vertexCoords));
	const auto		vertexBufferUsage	= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const auto		vertexBufferLoc		= DescriptorSetUpdateBuilder::Location::binding(0u);
	const auto		vertexBufferType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	const auto		indexBufferStages	= VK_SHADER_STAGE_MESH_BIT_NV;
	const auto		indexBufferSize		= static_cast<VkDeviceSize>(de::dataSize(m_params.vertexIndices));
	const auto		indexBufferUsage	= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const auto		indexBufferLoc		= DescriptorSetUpdateBuilder::Location::binding(1u);
	const auto		indexBufferType		= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	// Vertex buffer.
	const auto			vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
	BufferWithMemory	vertexBuffer		(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
	auto&				vertexBufferAlloc	= vertexBuffer.getAllocation();
	void*				vertexBufferDataPtr	= vertexBufferAlloc.getHostPtr();

	deMemcpy(vertexBufferDataPtr, m_params.vertexCoords.data(), static_cast<size_t>(vertexBufferSize));
	flushAlloc(vkd, device, vertexBufferAlloc);

	// Index buffer.
	const auto			indexBufferInfo		= makeBufferCreateInfo(indexBufferSize, indexBufferUsage);
	BufferWithMemory	indexBuffer			(vkd, device, alloc, indexBufferInfo, MemoryRequirement::HostVisible);
	auto&				indexBufferAlloc	= indexBuffer.getAllocation();
	void*				indexBufferDataPtr	= indexBufferAlloc.getHostPtr();

	deMemcpy(indexBufferDataPtr, m_params.vertexIndices.data(), static_cast<size_t>(indexBufferSize));
	flushAlloc(vkd, device, indexBufferAlloc);

	// Color buffer.
	const auto	colorBufferFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	const auto	colorBufferExtent	= makeExtent3D(8u, 8u, 1u);
	const auto	colorBufferUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorBufferFormat,						//	VkFormat				format;
		colorBufferExtent,						//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorBufferUsage,						//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory colorBuffer(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);

	const auto colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorBufferView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, colorBufferFormat, colorSRR);

	// Render pass.
	const auto renderPass = makeRenderPass(vkd, device, colorBufferFormat);

	// Framebuffer.
	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(), colorBufferExtent.width, colorBufferExtent.height);

	// Set layout.
	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vertexBufferType, vertexBufferStages);
	layoutBuilder.addSingleBinding(indexBufferType, indexBufferStages);
	const auto setLayout = layoutBuilder.build(vkd, device);

	// Descriptor pool.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vertexBufferType);
	poolBuilder.addType(indexBufferType);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	DescriptorSetUpdateBuilder updateBuilder;
	const auto vertexBufferDescInfo	= makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBufferSize);
	const auto indexBufferDescInfo	= makeDescriptorBufferInfo(indexBuffer.get(), 0ull, indexBufferSize);
	updateBuilder.writeSingle(descriptorSet.get(), vertexBufferLoc, vertexBufferType, &vertexBufferDescInfo);
	updateBuilder.writeSingle(descriptorSet.get(), indexBufferLoc, indexBufferType, &indexBufferDescInfo);
	updateBuilder.update(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Shader modules.
	Move<VkShaderModule>	taskModule;
	const auto&				binaries = m_context.getBinaryCollection();

	if (binaries.contains("task"))
		taskModule = createShaderModule(vkd, device, binaries.get("task"), 0u);
	const auto meshModule = createShaderModule(vkd, device, binaries.get("mesh"), 0u);
	const auto fragModule = createShaderModule(vkd, device, binaries.get("frag"), 0u);

	// Graphics pipeline.
	std::vector<VkViewport>	viewports	(1u, makeViewport(colorBufferExtent));
	std::vector<VkRect2D>	scissors	(1u, makeRect2D(colorBufferExtent));
	const auto				pipeline	= makeGraphicsPipeline(vkd, device, pipelineLayout.get(), taskModule.get(), meshModule.get(), fragModule.get(), renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool			= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer		= cmdBufferPtr.get();

	// Output buffer.
	const auto	tcuFormat		= mapVkFormat(colorBufferFormat);
	const auto	outBufferSize	= static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * colorBufferExtent.width * colorBufferExtent.height);
	const auto	outBufferUsage	= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo	= makeBufferCreateInfo(outBufferSize, outBufferUsage);
	BufferWithMemory outBuffer (vkd, device, alloc, outBufferInfo, MemoryRequirement::HostVisible);
	auto&		outBufferAlloc	= outBuffer.getAllocation();
	void*		outBufferData	= outBufferAlloc.getHostPtr();

	// Draw triangle.
	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)/*clear color*/);
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksNV(cmdBuffer, m_params.taskCount, 0u);
	endRenderPass(vkd, cmdBuffer);

	// Copy color buffer to output buffer.
	const tcu::IVec3 imageDim	(static_cast<int>(colorBufferExtent.width), static_cast<int>(colorBufferExtent.height), static_cast<int>(colorBufferExtent.depth));
	const tcu::IVec2 imageSize	(imageDim.x(), imageDim.y());

	copyImageToBuffer(vkd, cmdBuffer, colorBuffer.get(), outBuffer.get(), imageSize);
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Invalidate alloc.
	invalidateAlloc(vkd, device, outBufferAlloc);
	tcu::ConstPixelBufferAccess outPixels(tcuFormat, imageDim, outBufferData);

	auto& log = m_context.getTestContext().getLog();
	const tcu::Vec4 threshold (0.0f); // The color can be represented exactly.

	if (!tcu::floatThresholdCompare(log, "Result", "", m_params.expectedColor, outPixels, threshold, tcu::COMPARE_LOG_EVERYTHING))
		return tcu::TestStatus::fail("Failed; check log for details");

	return tcu::TestStatus::pass("Pass");
}

}

tcu::TestCaseGroup* createMeshShaderSmokeTests (tcu::TestContext& testCtx)
{
	GroupPtr smokeTests (new tcu::TestCaseGroup(testCtx, "smoke", "Mesh Shader Smoke Tests"));

	smokeTests->addChild(new MeshOnlyTriangleCase(testCtx, "mesh_shader_triangle", ""));
	smokeTests->addChild(new MeshTaskTriangleCase(testCtx, "mesh_task_shader_triangle", ""));
	smokeTests->addChild(new TaskOnlyTriangleCase(testCtx, "task_only_shader_triangle", ""));

	return smokeTests.release();
}

} // MeshShader
} // vkt
