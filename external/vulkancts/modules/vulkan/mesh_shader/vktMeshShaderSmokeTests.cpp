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
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

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
	checkTaskMeshShaderSupportNV(context, false, true);
}

void MeshTaskTriangleCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportNV(context, true, true);
}

void TaskOnlyTriangleCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportNV(context, true, true);
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

	if (!tcu::floatThresholdCompare(log, "Result", "", m_params.expectedColor, outPixels, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Failed; check log for details");

	return tcu::TestStatus::pass("Pass");
}

VkExtent3D gradientImageExtent ()
{
	return makeExtent3D(256u, 256u, 1u);
}

void checkMeshSupport (Context& context, tcu::Maybe<FragmentSize> fragmentSize)
{
	DE_UNREF(fragmentSize);
	checkTaskMeshShaderSupportNV(context, false, true);
}

void initGradientPrograms (vk::SourceCollections& programCollection, tcu::Maybe<FragmentSize> fragmentSize)
{
	const auto extent = gradientImageExtent();

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout (location=0) in  vec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    outColor = inColor;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	const auto useFragmentSize = static_cast<bool>(fragmentSize);

	if (!useFragmentSize)
	{
		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=4) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=256, max_primitives=256) out;\n"
			<< "\n"
			<< "layout (location=0) out vec4 outColor[];\n"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 2u;\n"
			<< "\n"
			<< "    const uint vertex    = gl_LocalInvocationIndex;\n"
			<< "    const uint primitive = gl_LocalInvocationIndex;\n"
			<< "\n"
			<< "    const vec4 topLeft      = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    const vec4 botLeft      = vec4(-1.0,  1.0, 0.0, 1.0);\n"
			<< "    const vec4 topRight     = vec4( 1.0, -1.0, 0.0, 1.0);\n"
			<< "    const vec4 botRight     = vec4( 1.0,  1.0, 0.0, 1.0);\n"
			<< "    const vec4 positions[4] = vec4[](topLeft, botLeft, topRight, botRight);\n"
			<< "\n"
			// Green changes according to the width.
			// Blue changes according to the height.
			// Value 0 at the center of the first pixel and value 1 at the center of the last pixel.
			<< "    const float width      = " << extent.width << ";\n"
			<< "    const float height     = " << extent.height << ";\n"
			<< "    const float halfWidth  = (1.0 / (width - 1.0)) / 2.0;\n"
			<< "    const float halfHeight = (1.0 / (height - 1.0)) / 2.0;\n"
			<< "    const float minGreen   = -halfWidth;\n"
			<< "    const float maxGreen   = 1.0+halfWidth;\n"
			<< "    const float minBlue    = -halfHeight;\n"
			<< "    const float maxBlue    = 1.0+halfHeight;\n"
			<< "    const vec4  colors[4]  = vec4[](\n"
			<< "        vec4(0, minGreen, minBlue, 1.0),\n"
			<< "        vec4(0, minGreen, maxBlue, 1.0),\n"
			<< "        vec4(0, maxGreen, minBlue, 1.0),\n"
			<< "        vec4(0, maxGreen, maxBlue, 1.0)\n"
			<< "    );\n"
			<< "\n"
			<< "    const uint indices[6] = uint[](0, 1, 2, 1, 3, 2);\n"
			<< "\n"
			<< "    if (vertex < 4u)\n"
			<< "    {\n"
			<< "        gl_MeshVerticesNV[vertex].gl_Position = positions[vertex];\n"
			<< "        outColor[vertex] = colors[vertex];\n"
			<< "    }\n"
			<< "    if (primitive < 2u)\n"
			<< "    {\n"
			<< "        for (uint i = 0; i < 3; ++i) {\n"
			<< "            const uint arrayPos = 3u * primitive + i;\n"
			<< "            gl_PrimitiveIndicesNV[arrayPos] = indices[arrayPos];\n"
			<< "        }\n"
			<< "    }\n"
			<< "}\n"
			;
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}
	else
	{
		const int shadingRateVal = getSPVShadingRateValue(fragmentSize.get());
		DE_ASSERT(shadingRateVal != 0);

		// The following shader is largely equivalent to the GLSL below if it was accepted by glslang.
#if 0
		#version 450
		#extension GL_NV_mesh_shader : enable

		layout(local_size_x=4) in;
		layout(triangles) out;
		layout(max_vertices=256, max_primitives=256) out;

		layout (location=0) out vec4 outColor[];

		perprimitiveNV out gl_MeshPerPrimitiveNV {
			int gl_PrimitiveShadingRateEXT;
		} gl_MeshPrimitivesNV[];

		void main ()
		{
			gl_PrimitiveCountNV = 2u;

			const uint vertex    = gl_LocalInvocationIndex;
			const uint primitive = gl_LocalInvocationIndex;

			const vec4 topLeft      = vec4(-1.0, -1.0, 0.0, 1.0);
			const vec4 botLeft      = vec4(-1.0,  1.0, 0.0, 1.0);
			const vec4 topRight     = vec4( 1.0, -1.0, 0.0, 1.0);
			const vec4 botRight     = vec4( 1.0,  1.0, 0.0, 1.0);
			const vec4 positions[4] = vec4[](topLeft, botLeft, topRight, botRight);

			const float width      = IMAGE_WIDTH;
			const float height     = IMAGE_HEIGHT;
			const float halfWidth  = (1.0 / (width - 1.0)) / 2.0;
			const float halfHeight = (1.0 / (height - 1.0)) / 2.0;
			const float minGreen   = -halfWidth;
			const float maxGreen   = 1.0+halfWidth;
			const float minBlue    = -halfHeight;
			const float maxBlue    = 1.0+halfHeight;
			const vec4  colors[4]  = vec4[](
				vec4(0, minGreen, minBlue, 1.0),
				vec4(0, minGreen, maxBlue, 1.0),
				vec4(0, maxGreen, minBlue, 1.0),
				vec4(0, maxGreen, maxBlue, 1.0)
			);

			const uint indices[6] = uint[](0, 1, 2, 1, 3, 2);

			if (vertex < 4u)
			{
				gl_MeshVerticesNV[vertex].gl_Position = positions[vertex];
				outColor[vertex] = colors[vertex];
			}
			if (primitive < 2u)
			{
				gl_MeshPrimitivesNV[primitive].gl_PrimitiveShadingRateEXT = SHADING_RATE;
				for (uint i = 0; i < 3; ++i)
				{
					const uint arrayPos = 3u * primitive + i;
					gl_PrimitiveIndicesNV[arrayPos] = indices[arrayPos];
				}
			}
		}
#endif

#undef SPV_PRECOMPUTED_CONSTANTS
		std::ostringstream meshSPV;
		meshSPV

			<< "; SPIR-V\n"
			<< "; Version: 1.0\n"
			<< "; Generator: Khronos Glslang Reference Front End; 10\n"
			<< "; Bound: 145\n"
			<< "; Schema: 0\n"
			<< "               OpCapability MeshShadingNV\n"
			<< "               OpCapability FragmentShadingRateKHR\n"						// Added.
			<< "               OpExtension \"SPV_NV_mesh_shader\"\n"
			<< "               OpExtension \"SPV_KHR_fragment_shading_rate\"\n"				// Added.
			<< "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "               OpMemoryModel Logical GLSL450\n"
			<< "               OpEntryPoint MeshNV %4 \"main\" %8 %13 %74 %93 %106 %129\n"
			<< "               OpExecutionMode %4 LocalSize 4 1 1\n"
			<< "               OpExecutionMode %4 OutputVertices 256\n"
			<< "               OpExecutionMode %4 OutputPrimitivesNV 256\n"
			<< "               OpExecutionMode %4 OutputTrianglesNV\n"
			<< "               OpDecorate %8 BuiltIn PrimitiveCountNV\n"
			<< "               OpDecorate %13 BuiltIn LocalInvocationIndex\n"
			// These will be actual constants.
			//<< "               OpDecorate %21 SpecId 0\n"
			//<< "               OpDecorate %27 SpecId 1\n"
			<< "               OpMemberDecorate %70 0 BuiltIn Position\n"
			<< "               OpMemberDecorate %70 1 BuiltIn PointSize\n"
			<< "               OpMemberDecorate %70 2 BuiltIn ClipDistance\n"
			<< "               OpMemberDecorate %70 3 BuiltIn CullDistance\n"
			<< "               OpMemberDecorate %70 4 PerViewNV\n"
			<< "               OpMemberDecorate %70 4 BuiltIn PositionPerViewNV\n"
			<< "               OpMemberDecorate %70 5 PerViewNV\n"
			<< "               OpMemberDecorate %70 5 BuiltIn ClipDistancePerViewNV\n"
			<< "               OpMemberDecorate %70 6 PerViewNV\n"
			<< "               OpMemberDecorate %70 6 BuiltIn CullDistancePerViewNV\n"
			<< "               OpDecorate %70 Block\n"
			<< "               OpDecorate %93 Location 0\n"
			<< "               OpMemberDecorate %103 0 PerPrimitiveNV\n"
			<< "               OpMemberDecorate %103 0 BuiltIn PrimitiveShadingRateKHR\n"	// Replaced PrimitiveID.
			<< "               OpDecorate %103 Block\n"
			<< "               OpDecorate %129 BuiltIn PrimitiveIndicesNV\n"
			<< "               OpDecorate %144 BuiltIn WorkgroupSize\n"
			<< "          %2 = OpTypeVoid\n"
			<< "          %3 = OpTypeFunction %2\n"
			<< "          %6 = OpTypeInt 32 0\n"
			<< "          %7 = OpTypePointer Output %6\n"
			<< "          %8 = OpVariable %7 Output\n"
			<< "          %9 = OpConstant %6 2\n"
			<< "         %10 = OpTypePointer Function %6\n"
			<< "         %12 = OpTypePointer Input %6\n"
			<< "         %13 = OpVariable %12 Input\n"
			<< "         %17 = OpTypeFloat 32\n"
			<< "         %18 = OpTypePointer Function %17\n"
			<< "         %20 = OpConstant %17 1\n"
			<< "         %21 = OpConstant %17 " << extent.width << "\n"		// Made constant instead of spec constant.
			<< "         %24 = OpConstant %17 2\n"
			<< "         %27 = OpConstant %17 " << extent.height << "\n"	// Made constant instead of spec constant.
			<< "         %43 = OpTypeVector %17 4\n"
			<< "         %44 = OpConstant %6 4\n"
			<< "         %45 = OpTypeArray %43 %44\n"
			<< "         %46 = OpTypePointer Function %45\n"
			<< "         %48 = OpConstant %17 0\n"
			<< "         %63 = OpTypeBool\n"
			<< "         %67 = OpConstant %6 1\n"
			<< "         %68 = OpTypeArray %17 %67\n"
			<< "         %69 = OpTypeArray %68 %44\n"
			<< "         %70 = OpTypeStruct %43 %17 %68 %68 %45 %69 %69\n"
			<< "         %71 = OpConstant %6 256\n"
			<< "         %72 = OpTypeArray %70 %71\n"
			<< "         %73 = OpTypePointer Output %72\n"
			<< "         %74 = OpVariable %73 Output\n"
			<< "         %76 = OpTypeInt 32 1\n"
			<< "         %77 = OpConstant %76 0\n"
			<< "         %78 = OpConstant %17 -1\n"
			<< "         %79 = OpConstantComposite %43 %78 %78 %48 %20\n"
			<< "         %80 = OpConstantComposite %43 %78 %20 %48 %20\n"
			<< "         %81 = OpConstantComposite %43 %20 %78 %48 %20\n"
			<< "         %82 = OpConstantComposite %43 %20 %20 %48 %20\n"
			<< "         %83 = OpConstantComposite %45 %79 %80 %81 %82\n"
			<< "         %86 = OpTypePointer Function %43\n"
			<< "         %89 = OpTypePointer Output %43\n"
			<< "         %91 = OpTypeArray %43 %71\n"
			<< "         %92 = OpTypePointer Output %91\n"
			<< "         %93 = OpVariable %92 Output\n"
			<< "        %103 = OpTypeStruct %76\n"
			<< "        %104 = OpTypeArray %103 %71\n"
			<< "        %105 = OpTypePointer Output %104\n"
			<< "        %106 = OpVariable %105 Output\n"
			<< "        %108 = OpConstant %76 " << shadingRateVal << "\n"	// Used mask value here.
			<< "        %109 = OpTypePointer Output %76\n"
			<< "        %112 = OpConstant %6 0\n"
			<< "        %119 = OpConstant %6 3\n"
			<< "        %126 = OpConstant %6 768\n"
			<< "        %127 = OpTypeArray %6 %126\n"
			<< "        %128 = OpTypePointer Output %127\n"
			<< "        %129 = OpVariable %128 Output\n"
			<< "        %131 = OpConstant %6 6\n"
			<< "        %132 = OpTypeArray %6 %131\n"
			<< "        %133 = OpConstantComposite %132 %112 %67 %9 %67 %119 %9\n"
			<< "        %135 = OpTypePointer Function %132\n"
			<< "        %141 = OpConstant %76 1\n"
			<< "        %143 = OpTypeVector %6 3\n"
			<< "        %144 = OpConstantComposite %143 %44 %67 %67\n"
			<< "          %4 = OpFunction %2 None %3\n"
			<< "          %5 = OpLabel\n"
			<< "         %11 = OpVariable %10 Function\n"
			<< "         %15 = OpVariable %10 Function\n"
			<< "         %19 = OpVariable %18 Function\n"
			<< "         %26 = OpVariable %18 Function\n"
			<< "         %31 = OpVariable %18 Function\n"
			<< "         %34 = OpVariable %18 Function\n"
			<< "         %37 = OpVariable %18 Function\n"
			<< "         %40 = OpVariable %18 Function\n"
			<< "         %47 = OpVariable %46 Function\n"
			<< "         %85 = OpVariable %46 Function\n"
			<< "        %111 = OpVariable %10 Function\n"
			<< "        %121 = OpVariable %10 Function\n"
			<< "        %136 = OpVariable %135 Function\n"
			<< "               OpStore %8 %9\n"
			<< "         %14 = OpLoad %6 %13\n"
			<< "               OpStore %11 %14\n"
			<< "         %16 = OpLoad %6 %13\n"
			<< "               OpStore %15 %16\n"
			<< "         %22 = OpFSub %17 %21 %20\n"
			<< "         %23 = OpFDiv %17 %20 %22\n"
			<< "         %25 = OpFDiv %17 %23 %24\n"
			<< "               OpStore %19 %25\n"
			<< "         %28 = OpFSub %17 %27 %20\n"
			<< "         %29 = OpFDiv %17 %20 %28\n"
			<< "         %30 = OpFDiv %17 %29 %24\n"
			<< "               OpStore %26 %30\n"
			<< "         %32 = OpLoad %17 %19\n"
			<< "         %33 = OpFNegate %17 %32\n"
			<< "               OpStore %31 %33\n"
			<< "         %35 = OpLoad %17 %26\n"
			<< "         %36 = OpFNegate %17 %35\n"
			<< "               OpStore %34 %36\n"
			<< "         %38 = OpLoad %17 %19\n"
			<< "         %39 = OpFAdd %17 %20 %38\n"
			<< "               OpStore %37 %39\n"
			<< "         %41 = OpLoad %17 %26\n"
			<< "         %42 = OpFAdd %17 %20 %41\n"
			<< "               OpStore %40 %42\n"
			<< "         %49 = OpLoad %17 %31\n"
			<< "         %50 = OpLoad %17 %34\n"
			<< "         %51 = OpCompositeConstruct %43 %48 %49 %50 %20\n"
			<< "         %52 = OpLoad %17 %31\n"
			<< "         %53 = OpLoad %17 %40\n"
			<< "         %54 = OpCompositeConstruct %43 %48 %52 %53 %20\n"
			<< "         %55 = OpLoad %17 %37\n"
			<< "         %56 = OpLoad %17 %34\n"
			<< "         %57 = OpCompositeConstruct %43 %48 %55 %56 %20\n"
			<< "         %58 = OpLoad %17 %37\n"
			<< "         %59 = OpLoad %17 %40\n"
			<< "         %60 = OpCompositeConstruct %43 %48 %58 %59 %20\n"
			<< "         %61 = OpCompositeConstruct %45 %51 %54 %57 %60\n"
			<< "               OpStore %47 %61\n"
			<< "         %62 = OpLoad %6 %11\n"
			<< "         %64 = OpULessThan %63 %62 %44\n"
			<< "               OpSelectionMerge %66 None\n"
			<< "               OpBranchConditional %64 %65 %66\n"
			<< "         %65 = OpLabel\n"
			<< "         %75 = OpLoad %6 %11\n"
			<< "         %84 = OpLoad %6 %11\n"
			<< "               OpStore %85 %83\n"
			<< "         %87 = OpAccessChain %86 %85 %84\n"
			<< "         %88 = OpLoad %43 %87\n"
			<< "         %90 = OpAccessChain %89 %74 %75 %77\n"
			<< "               OpStore %90 %88\n"
			<< "         %94 = OpLoad %6 %11\n"
			<< "         %95 = OpLoad %6 %11\n"
			<< "         %96 = OpAccessChain %86 %47 %95\n"
			<< "         %97 = OpLoad %43 %96\n"
			<< "         %98 = OpAccessChain %89 %93 %94\n"
			<< "               OpStore %98 %97\n"
			<< "               OpBranch %66\n"
			<< "         %66 = OpLabel\n"
			<< "         %99 = OpLoad %6 %15\n"
			<< "        %100 = OpULessThan %63 %99 %9\n"
			<< "               OpSelectionMerge %102 None\n"
			<< "               OpBranchConditional %100 %101 %102\n"
			<< "        %101 = OpLabel\n"
			<< "        %107 = OpLoad %6 %15\n"
			<< "        %110 = OpAccessChain %109 %106 %107 %77\n"
			<< "               OpStore %110 %108\n"
			<< "               OpStore %111 %112\n"
			<< "               OpBranch %113\n"
			<< "        %113 = OpLabel\n"
			<< "               OpLoopMerge %115 %116 None\n"
			<< "               OpBranch %117\n"
			<< "        %117 = OpLabel\n"
			<< "        %118 = OpLoad %6 %111\n"
			<< "        %120 = OpULessThan %63 %118 %119\n"
			<< "               OpBranchConditional %120 %114 %115\n"
			<< "        %114 = OpLabel\n"
			<< "        %122 = OpLoad %6 %15\n"
			<< "        %123 = OpIMul %6 %119 %122\n"
			<< "        %124 = OpLoad %6 %111\n"
			<< "        %125 = OpIAdd %6 %123 %124\n"
			<< "               OpStore %121 %125\n"
			<< "        %130 = OpLoad %6 %121\n"
			<< "        %134 = OpLoad %6 %121\n"
			<< "               OpStore %136 %133\n"
			<< "        %137 = OpAccessChain %10 %136 %134\n"
			<< "        %138 = OpLoad %6 %137\n"
			<< "        %139 = OpAccessChain %7 %129 %130\n"
			<< "               OpStore %139 %138\n"
			<< "               OpBranch %116\n"
			<< "        %116 = OpLabel\n"
			<< "        %140 = OpLoad %6 %111\n"
			<< "        %142 = OpIAdd %6 %140 %141\n"
			<< "               OpStore %111 %142\n"
			<< "               OpBranch %113\n"
			<< "        %115 = OpLabel\n"
			<< "               OpBranch %102\n"
			<< "        %102 = OpLabel\n"
			<< "               OpReturn\n"
			<< "               OpFunctionEnd\n"
			;
		programCollection.spirvAsmSources.add("mesh") << meshSPV.str();
	}
}

std::string coordColorFormat (int x, int y, const tcu::Vec4& color)
{
	std::ostringstream msg;
	msg << "[" << x << ", " << y << "]=(" << color.x() << ", " << color.y() << ", " << color.z() << ", " << color.w() << ")";
	return msg.str();
}

tcu::TestStatus testFullscreenGradient (Context& context, tcu::Maybe<FragmentSize> fragmentSize)
{
	const auto&		vkd					= context.getDeviceInterface();
	const auto		device				= context.getDevice();
	auto&			alloc				= context.getDefaultAllocator();
	const auto		qIndex				= context.getUniversalQueueFamilyIndex();
	const auto		queue				= context.getUniversalQueue();
	const auto		useFragmentSize		= static_cast<bool>(fragmentSize);
	const auto		defaultFragmentSize	= FragmentSize::SIZE_1X1;
	const auto		rateSize			= getShadingRateSize(useFragmentSize ? fragmentSize.get() : defaultFragmentSize);

	// Color buffer.
	const auto	colorBufferFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	const auto	colorBufferExtent	= makeExtent3D(256u, 256u, 1u); // Big enough for a detailed gradient, small enough to get unique colors.
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
	const auto setLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Shader modules.
	Move<VkShaderModule>	taskModule;
	const auto&				binaries = context.getBinaryCollection();

	const auto meshModule = createShaderModule(vkd, device, binaries.get("mesh"), 0u);
	const auto fragModule = createShaderModule(vkd, device, binaries.get("frag"), 0u);

	using ShadingRateInfoPtr = de::MovePtr<VkPipelineFragmentShadingRateStateCreateInfoKHR>;
	ShadingRateInfoPtr pNext;
	if (useFragmentSize)
	{
		pNext = ShadingRateInfoPtr(new VkPipelineFragmentShadingRateStateCreateInfoKHR);
		*pNext = initVulkanStructure();

		pNext->fragmentSize		= getShadingRateSize(FragmentSize::SIZE_1X1); // 1x1 will not be used as the primitive rate in tests with fragment size.
		pNext->combinerOps[0]	= VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
		pNext->combinerOps[1]	= VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
	}

	// Graphics pipeline.
	std::vector<VkViewport>	viewports	(1u, makeViewport(colorBufferExtent));
	std::vector<VkRect2D>	scissors	(1u, makeRect2D(colorBufferExtent));
	const auto				pipeline	= makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors, 0u, nullptr, nullptr, nullptr, nullptr, nullptr, 0u, pNext.get());

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

	// Draw triangles.
	beginCommandBuffer(vkd, cmdBuffer);
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)/*clear color*/);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdDrawMeshTasksNV(cmdBuffer, 1u, 0u);
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

	// Create reference image.
	tcu::TextureLevel		refLevel	(tcuFormat, imageDim.x(), imageDim.y(), imageDim.z());
	tcu::PixelBufferAccess	refAccess	(refLevel);
	for (int y = 0; y < imageDim.y(); ++y)
	for (int x = 0; x < imageDim.x(); ++x)
	{
		const tcu::IVec4 color (0, x, y, 255);
		refAccess.setPixel(color, x, y);
	}

	const tcu::TextureFormat	maskFormat	(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
	tcu::TextureLevel			errorMask	(maskFormat, imageDim.x(), imageDim.y(), imageDim.z());
	tcu::PixelBufferAccess		errorAccess	(errorMask);
	const tcu::Vec4				green		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4				red			(1.0f, 0.0f, 0.0f, 1.0f);
	auto&						log			= context.getTestContext().getLog();

	// Each block needs to have the same color and be equal to one of the pixel colors of that block in the reference image.
	const auto blockWidth	= static_cast<int>(rateSize.width);
	const auto blockHeight	= static_cast<int>(rateSize.height);

	tcu::clear(errorAccess, green);
	bool globalFail = false;

	for (int y = 0; y < imageDim.y() / blockHeight; ++y)
	for (int x = 0; x < imageDim.x() / blockWidth; ++x)
	{
		bool					blockFail	= false;
		std::vector<tcu::Vec4>	candidates;

		candidates.reserve(rateSize.width * rateSize.height);

		const auto cornerY		= y * blockHeight;
		const auto cornerX		= x * blockWidth;
		const auto cornerColor	= outPixels.getPixel(cornerX, cornerY);

		for (int blockY = 0; blockY < blockHeight; ++blockY)
		for (int blockX = 0; blockX < blockWidth; ++blockX)
		{
			const auto absY		= cornerY + blockY;
			const auto absX		= cornerX + blockX;
			const auto resColor	= outPixels.getPixel(absX, absY);

			candidates.push_back(refAccess.getPixel(absX, absY));

			if (cornerColor != resColor)
			{
				std::ostringstream msg;
				msg << "Block not uniform: "
					<< coordColorFormat(cornerX, cornerY, cornerColor)
					<< " vs "
					<< coordColorFormat(absX, absY, resColor);
				log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

				blockFail = true;
			}
		}

		if (!de::contains(begin(candidates), end(candidates), cornerColor))
		{
			std::ostringstream msg;
			msg << "Block color does not match any reference color at [" << cornerX << ", " << cornerY << "]";
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
			blockFail = true;
		}

		if (blockFail)
		{
			const auto blockAccess = tcu::getSubregion(errorAccess, cornerX, cornerY, blockWidth, blockHeight);
			tcu::clear(blockAccess, red);
			globalFail = true;
		}
	}

	if (globalFail)
	{
		log << tcu::TestLog::Image("Result", "", outPixels);
		log << tcu::TestLog::Image("Reference", "", refAccess);
		log << tcu::TestLog::Image("ErrorMask", "", errorAccess);

		TCU_FAIL("Color mismatch; check log for more details");
	}

	return tcu::TestStatus::pass("Pass");
}

}

tcu::TestCaseGroup* createMeshShaderSmokeTests (tcu::TestContext& testCtx)
{
	GroupPtr smokeTests (new tcu::TestCaseGroup(testCtx, "smoke", "Mesh Shader Smoke Tests"));

	smokeTests->addChild(new MeshOnlyTriangleCase(testCtx, "mesh_shader_triangle", ""));
	smokeTests->addChild(new MeshTaskTriangleCase(testCtx, "mesh_task_shader_triangle", ""));
	smokeTests->addChild(new TaskOnlyTriangleCase(testCtx, "task_only_shader_triangle", ""));

	addFunctionCaseWithPrograms(smokeTests.get(), "fullscreen_gradient",		"", checkMeshSupport, initGradientPrograms, testFullscreenGradient, tcu::nothing<FragmentSize>());
	addFunctionCaseWithPrograms(smokeTests.get(), "fullscreen_gradient_fs2x2",	"", checkMeshSupport, initGradientPrograms, testFullscreenGradient, tcu::just(FragmentSize::SIZE_2X2));
	addFunctionCaseWithPrograms(smokeTests.get(), "fullscreen_gradient_fs2x1",	"", checkMeshSupport, initGradientPrograms, testFullscreenGradient, tcu::just(FragmentSize::SIZE_2X1));

	return smokeTests.release();
}

} // MeshShader
} // vkt
