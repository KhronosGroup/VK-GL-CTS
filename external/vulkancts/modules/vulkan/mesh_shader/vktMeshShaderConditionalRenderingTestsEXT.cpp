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
 * \brief Tests using VK_EXT_mesh_shader and VK_EXT_conditional_rendering
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderConditionalRenderingTestsEXT.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"

#include <vector>
#include <sstream>
#include <memory>

namespace vkt
{
namespace MeshShader
{

namespace
{

using namespace vk;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

enum class DrawType
{
	DRAW,
	DRAW_INDIRECT,
	DRAW_INDIRECT_WITH_COUNT,
};

enum class CmdBufferType
{
	PRIMARY,
	SECONDARY,
	SECONDARY_WITH_INHERITANCE,
};

static constexpr VkDeviceSize kBindOffset = 16u;

std::vector<uint32_t> getCondValues (void)
{
	const std::vector<uint32_t> values =
	{
		0x01000000u,
		0x00010000u,
		0x00000100u,
		0x00000001u,
		0x00000000u,
	};

	return values;
}

std::string paddedHex (uint32_t value)
{
	std::ostringstream repr;
	repr << "0x" << std::hex << std::setw(8u) << std::setfill('0') << value;
	return repr.str();
}

tcu::Vec4 getOutputColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

tcu::Vec4 getClearColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

struct TestParams
{
	DrawType		drawType;
	CmdBufferType	cmdBufferType;
	bool			bindWithOffset;
	bool			condWithOffset;
	uint32_t		condValue;
	bool			inverted;
	bool			useTask;

	bool needsSecondaryCmdBuffer (void) const
	{
		return (cmdBufferType != CmdBufferType::PRIMARY);
	}
};

class ConditionalRenderingCase : public vkt::TestCase
{
public:
					ConditionalRenderingCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~ConditionalRenderingCase	(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;

protected:
	const TestParams m_params;
};

class ConditionBuffer
{
public:
	ConditionBuffer (const DeviceInterface& vkd, VkDevice device, Allocator& alloc, uint32_t condValue, bool bindWithOffset, bool condWithOffset)
		: m_buffer		()
		, m_allocation	()
		, m_condOffset	(0ull)
	{
		// Create buffer with the desired size first.
		const auto	condSize			= static_cast<VkDeviceSize>(sizeof(condValue));
		const auto	condOffset			= (condWithOffset ? condSize : 0ull);
		const auto	bufferSize			= condSize + condOffset;
		const auto	bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
		auto		buffer				= createBuffer(vkd, device, &bufferCreateInfo);

		// Allocate memory taking bindWithOffset into account.
		const auto bufferMemReqs	= getBufferMemoryRequirements(vkd, device, buffer.get());
		const auto bindOffset		= (bindWithOffset ? de::roundUp(kBindOffset, bufferMemReqs.alignment) : 0ull);
		const auto allocSize		= bufferMemReqs.size + bindOffset;

		const auto	actualMemReqs	= makeMemoryRequirements(allocSize, bufferMemReqs.alignment, bufferMemReqs.memoryTypeBits);
		auto		allocation		= alloc.allocate(actualMemReqs, MemoryRequirement::HostVisible);
		vkd.bindBufferMemory(device, buffer.get(), allocation->getMemory(), bindOffset);

		// Fill buffer data.
		const uint32_t	fillValue	= ((condValue != 0u) ? 0u : 1u);
		uint8_t*		hostPtr		= reinterpret_cast<uint8_t*>(allocation->getHostPtr());

		deMemset(hostPtr,							0,			static_cast<size_t>(actualMemReqs.size));
		deMemcpy(hostPtr + bindOffset,				&fillValue,	sizeof(fillValue));
		deMemcpy(hostPtr + bindOffset + condOffset,	&condValue,	sizeof(condValue));

		m_buffer		= buffer;
		m_allocation	= allocation;
		m_condOffset	= condOffset;
	}

	VkDeviceSize getCondOffset (void) const
	{
		return m_condOffset;
	}

	VkBuffer getBuffer (void) const
	{
		return m_buffer.get();
	}

	// Cannot copy or assign this.
	ConditionBuffer (const ConditionBuffer&) = delete;
	ConditionBuffer& operator=(const ConditionBuffer&) = delete;

protected:
	Move<VkBuffer>			m_buffer;
	de::MovePtr<Allocation>	m_allocation;
	VkDeviceSize			m_condOffset;
};

class ConditionalRenderingInstance : public vkt::TestInstance
{
public:
							ConditionalRenderingInstance	(Context& context, const TestParams& params)
								: vkt::TestInstance			(context)
								, m_params					(params)
								, m_conditionBuffer			()
								, m_indirectDrawArgsBuffer	()
								, m_indirectDrawCountBuffer	()
								{}
	virtual					~ConditionalRenderingInstance	(void) {}

	tcu::TestStatus			iterate							(void) override;

protected:
	// Creates the indirect buffers that are needed according to the test parameters.
	void					initIndirectBuffers				(const DeviceInterface& vkd, const VkDevice device, Allocator& alloc);

	// Calls the appropriate drawing command depending on the test parameters.
	void					drawMeshTasks					(const DeviceInterface& vkd, const VkCommandBuffer cmdBuffer) const;

	const TestParams					m_params;
	std::unique_ptr<ConditionBuffer>	m_conditionBuffer;
	std::unique_ptr<BufferWithMemory>	m_indirectDrawArgsBuffer;
	std::unique_ptr<BufferWithMemory>	m_indirectDrawCountBuffer;
};

// Makes an indirect buffer with the specified contents.
template<class T>
std::unique_ptr<BufferWithMemory> makeIndirectBuffer (const DeviceInterface& vkd, const VkDevice device, Allocator& alloc, const T& data)
{
	const auto bufferSize		= static_cast<VkDeviceSize>(sizeof(data));
	const auto bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	std::unique_ptr<BufferWithMemory> buffer (new BufferWithMemory(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible));

	auto& allocation	= buffer->getAllocation();
	void* dataPtr		= allocation.getHostPtr();

	deMemcpy(dataPtr, &data, sizeof(data));
	flushAlloc(vkd, device, allocation);

	return buffer;
}

void ConditionalRenderingInstance::initIndirectBuffers (const DeviceInterface& vkd, const VkDevice device, Allocator& alloc)
{
	if (m_params.drawType != DrawType::DRAW)
	{
		const VkDrawMeshTasksIndirectCommandEXT drawArgs = { 1u, 1u, 1u };
		m_indirectDrawArgsBuffer = makeIndirectBuffer(vkd, device, alloc, drawArgs);
	}

	if (m_params.drawType == DrawType::DRAW_INDIRECT_WITH_COUNT)
	{
		const uint32_t drawCount = 1u;
		m_indirectDrawCountBuffer = makeIndirectBuffer(vkd, device, alloc, drawCount);
	}
}

void ConditionalRenderingInstance::drawMeshTasks (const DeviceInterface& vkd, const VkCommandBuffer cmdBuffer) const
{
	const auto stride = static_cast<uint32_t>(sizeof(VkDrawMeshTasksIndirectCommandEXT));

	if (m_params.drawType == DrawType::DRAW)
	{
		vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
	}
	else if (m_params.drawType == DrawType::DRAW_INDIRECT)
	{
		vkd.cmdDrawMeshTasksIndirectEXT(cmdBuffer, m_indirectDrawArgsBuffer->get(), 0ull, 1u, stride);
	}
	else if (m_params.drawType == DrawType::DRAW_INDIRECT_WITH_COUNT)
	{
		vkd.cmdDrawMeshTasksIndirectCountEXT(cmdBuffer, m_indirectDrawArgsBuffer->get(), 0ull, m_indirectDrawCountBuffer->get(), 0ull, 1u, stride);
	}
	else
		DE_ASSERT(false);
}

void ConditionalRenderingCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

	if (m_params.useTask)
	{
		std::ostringstream task;
		task
			<< "#version 460\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "\n"
			<< "void main (void) {\n"
			<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;
	}

	std::ostringstream mesh;
	mesh
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (triangles) out;\n"
		<< "layout (max_vertices=3, max_primitives=1) out;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    SetMeshOutputsEXT(3u, 1u);\n"
		<< "\n"
		<< "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
		<< "\n"
		<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
		<< "    gl_MeshVerticesEXT[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

	const auto outColor = getOutputColor();
	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "void main (void) {\n"
		<< "    outColor = vec4" << outColor << ";\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance* ConditionalRenderingCase::createInstance (Context& context) const
{
	return new ConditionalRenderingInstance(context, m_params);
}

void ConditionalRenderingCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportEXT(context, m_params.useTask/*requireTask*/, true/*requireMesh*/);

	context.requireDeviceFunctionality("VK_EXT_conditional_rendering");

	if (m_params.drawType == DrawType::DRAW_INDIRECT_WITH_COUNT)
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");

	if (m_params.cmdBufferType == CmdBufferType::SECONDARY_WITH_INHERITANCE)
	{
		const auto& condRenderingFeatures = context.getConditionalRenderingFeaturesEXT();
		if (!condRenderingFeatures.inheritedConditionalRendering)
			TCU_THROW(NotSupportedError, "inheritedConditionalRendering not supported");
	}
}

tcu::TestStatus ConditionalRenderingInstance::iterate ()
{
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto			queue			= m_context.getUniversalQueue();

	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			tcuFormat		= mapVkFormat(colorFormat);
	const auto			colorExtent		= makeExtent3D(4u, 4u, 1u);
	const tcu::IVec3	iExtent3D		(static_cast<int>(colorExtent.width), static_cast<int>(colorExtent.height), static_cast<int>(colorExtent.depth));
	const auto			clearColor		= getClearColor();
	const auto			drawColor		= getOutputColor();
	const auto			bindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	const auto			needsSecCmd		= m_params.needsSecondaryCmdBuffer();

	// Create color attachment.
	const VkImageCreateInfo colorAttCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		colorExtent,							//	VkExtent3D				extent;
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
	ImageWithMemory	colorAtt		(vkd, device, alloc, colorAttCreateInfo, MemoryRequirement::Any);
	const auto		colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto		colorAttView	= makeImageView(vkd, device, colorAtt.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Render pass and framebuffer.
	const auto renderPass	= makeRenderPass(vkd, device, colorFormat);
	const auto framebuffer	= makeFramebuffer(vkd, device, renderPass.get(), colorAttView.get(), colorExtent.width, colorExtent.height);

	// Verification buffer.
	const auto			verifBufferSize			= static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat) * iExtent3D.x() * iExtent3D.y() * iExtent3D.z());
	const auto			verifBufferCreateInfo	= makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verifBuffer				(vkd, device, alloc, verifBufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				verifBufferAlloc		= verifBuffer.getAllocation();
	void*				verifBufferData			= verifBufferAlloc.getHostPtr();

	// Create the condition buffer.
	m_conditionBuffer.reset(new ConditionBuffer(vkd, device, alloc, m_params.condValue, m_params.bindWithOffset, m_params.condWithOffset));

	// Create the indirect buffers if needed.
	initIndirectBuffers(vkd, device, alloc);

	// Pipeline.
	const auto	pipelineLayout	= makePipelineLayout(vkd, device);
	const auto&	binaries		= m_context.getBinaryCollection();
	const auto	taskModule		= (binaries.contains("task") ? createShaderModule(vkd, device, binaries.get("task")) : Move<VkShaderModule>());
	const auto	meshModule		= createShaderModule(vkd, device, binaries.get("mesh"));
	const auto	fragModule		= createShaderModule(vkd, device, binaries.get("frag"));

	const std::vector<VkViewport>	viewports	(1u, makeViewport(colorExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(colorExtent));

	const auto pipeline = makeGraphicsPipeline(
		vkd, device, pipelineLayout.get(),
		taskModule.get(), meshModule.get(), fragModule.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and command buffers.
	const auto cmdPool				= makeCommandPool(vkd, device, queueIndex);
	const auto primaryCmdBuffer		= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto secondaryCmdBuffer	= (needsSecCmd ? allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY) : Move<VkCommandBuffer>());
	const auto primary				= primaryCmdBuffer.get();
	const auto secondary			= secondaryCmdBuffer.get();

	// Common conditional rendering begin info.
	const auto									conditionalRenderingFlags	=	(m_params.inverted
																				? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT
																				: static_cast<VkConditionalRenderingFlagBitsEXT>(0));
	const VkConditionalRenderingBeginInfoEXT	conditionalRenderingBegin	=
	{
		VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,					//	VkStructureType					sType;
		nullptr,																//	const void*						pNext;
		m_conditionBuffer->getBuffer(),											//	VkBuffer						buffer;
		m_conditionBuffer->getCondOffset(),										//	VkDeviceSize					offset;
		conditionalRenderingFlags,												//	VkConditionalRenderingFlagsEXT	flags;
	};

	// Inheritance info for the secondary command buffer.
	const auto														conditionalRenderingEnable			= ((m_params.cmdBufferType == CmdBufferType::SECONDARY_WITH_INHERITANCE)
																										? VK_TRUE
																										: VK_FALSE);
	const vk::VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,	//	VkStructureType	sType;
		nullptr,																		//	const void*		pNext;
		conditionalRenderingEnable,														//	VkBool32		conditionalRenderingEnable;
	};

	const VkCommandBufferInheritanceInfo inheritanceInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	//	VkStructureType					sType;
		&conditionalRenderingInheritanceInfo,				//	const void*						pNext;
		renderPass.get(),									//	VkRenderPass					renderPass;
		0u,													//	uint32_t						subpass;
		framebuffer.get(),									//	VkFramebuffer					framebuffer;
		VK_FALSE,											//	VkBool32						occlusionQueryEnable;
		0u,													//	VkQueryControlFlags				queryFlags;
		0u,													//	VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const VkCommandBufferUsageFlags	cmdBufferUsageFlags	= (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
	const VkCommandBufferBeginInfo	secondaryBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//	VkStructureType							sType;
		nullptr,										//	const void*								pNext;
		cmdBufferUsageFlags,							//	VkCommandBufferUsageFlags				flags;
		&inheritanceInfo,								//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};

	beginCommandBuffer(vkd, primary);

	if (m_params.cmdBufferType == CmdBufferType::PRIMARY)
	{
		// Do everything in the primary command buffer.
		const auto cmdBuffer = primary;

		vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
		beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
		drawMeshTasks(vkd, cmdBuffer);
		endRenderPass(vkd, cmdBuffer);
		vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
	}
	else if (m_params.cmdBufferType == CmdBufferType::SECONDARY)
	{
		// Do everything in the secondary command buffer.
		// In addition, do the conditional rendering inside the render pass so it's a bit different from the primary case.
		beginRenderPass(vkd, primary, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		const auto cmdBuffer = secondaryCmdBuffer.get();

		vkd.beginCommandBuffer(secondary, &secondaryBeginInfo);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
		vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &conditionalRenderingBegin);
		drawMeshTasks(vkd, cmdBuffer);
		vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
		endCommandBuffer(vkd, cmdBuffer);

		vkd.cmdExecuteCommands(primary, 1u, &cmdBuffer);
		endRenderPass(vkd, primary);
	}
	else if (m_params.cmdBufferType == CmdBufferType::SECONDARY_WITH_INHERITANCE)
	{
		// Inherit everything in the secondary command buffer.
		vkd.beginCommandBuffer(secondary, &secondaryBeginInfo);
		vkd.cmdBindPipeline(secondary, bindPoint, pipeline.get());
		drawMeshTasks(vkd, secondary);
		endCommandBuffer(vkd, secondary);

		vkd.cmdBeginConditionalRenderingEXT(primary, &conditionalRenderingBegin);
		beginRenderPass(vkd, primary, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		vkd.cmdExecuteCommands(primary, 1u, &secondary);
		endRenderPass(vkd, primary);
		vkd.cmdEndConditionalRenderingEXT(primary);
	}
	else
		DE_ASSERT(false);

	// Transfer color attachment to the verification buffer.
	const auto postTransferBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const auto copyRegion			= makeBufferImageCopy(colorExtent, colorSRL);
	const auto preTranferBarrier	= makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAtt.get(), colorSRR);

	cmdPipelineImageMemoryBarrier(vkd, primary, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTranferBarrier);
	vkd.cmdCopyImageToBuffer(primary, colorAtt.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);
	cmdPipelineMemoryBarrier(vkd, primary, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postTransferBarrier);

	endCommandBuffer(vkd, primary);
	submitCommandsAndWait(vkd, device, queue, primary);

	invalidateAlloc(vkd, device, verifBufferAlloc);

	const tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, iExtent3D, verifBufferData);
	const bool							expectDraw		= ((m_params.condValue != 0u) != m_params.inverted);
	const auto							expectedColor	= (expectDraw ? drawColor : clearColor);
	const tcu::Vec4						threshold		(0.0f, 0.0f, 0.0f, 0.0f);

	auto& log = m_context.getTestContext().getLog();
	if (!tcu::floatThresholdCompare(log, "Result", "", expectedColor, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		TCU_FAIL("Check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createMeshShaderConditionalRenderingTestsEXT (tcu::TestContext& testCtx)
{
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "conditional_rendering", "Mesh Shader with Conditional Rendering"));

	const struct
	{
		DrawType		drawType;
		const char*		name;
	} drawTypeCases[] =
	{
		{	DrawType::DRAW,						"draw"					},
		{	DrawType::DRAW_INDIRECT,			"draw_indirect"			},
		{	DrawType::DRAW_INDIRECT_WITH_COUNT,	"draw_indirect_count"	},
	};

	const struct
	{
		CmdBufferType	cmdBufferType;
		const char*		name;
	} cmdBufferTypeCases[] =
	{
		{	CmdBufferType::PRIMARY,						"primary_cmd_buffer"				},
		{	CmdBufferType::SECONDARY,					"secondary_cmd_buffer"				},
		{	CmdBufferType::SECONDARY_WITH_INHERITANCE,	"secondary_cmd_buffer_inheritance"	},
	};

	const struct
	{
		bool		bindWithOffset;
		const char*	name;
	} bindWithOffsetCases[] =
	{
		{ false,	"bind_without_offset"	},
		{ true,		"bind_with_offset"		},
	};

	const struct
	{
		bool		condWithOffset;
		const char*	name;
	} condWithOffsetCases[] =
	{
		{ false,	"cond_without_offset"	},
		{ true,		"cond_with_offset"		},
	};

	const struct
	{
		bool		inverted;
		const char*	name;
	} inversionCases[] =
	{
		{ false,	"normal_cond"	},
		{ true,		"inverted_cond"	},
	};

	const struct
	{
		bool		useTask;
		const char*	name;
	} useTaskCases[] =
	{
		{ false,	"mesh_only"		},
		{ true,		"mesh_and_task"	},
	};

	const auto condValues = getCondValues();

	for (const auto& drawTypeCase : drawTypeCases)
	{
		GroupPtr drawTypeGroup (new tcu::TestCaseGroup(testCtx, drawTypeCase.name, ""));

		for (const auto& cmdBufferTypeCase : cmdBufferTypeCases)
		{
			GroupPtr cmdBufferTypeGroup (new tcu::TestCaseGroup(testCtx, cmdBufferTypeCase.name, ""));

			for (const auto& bindWithOffsetCase : bindWithOffsetCases)
			{
				GroupPtr bindWithOffsetGroup (new tcu::TestCaseGroup(testCtx, bindWithOffsetCase.name, ""));

				for (const auto& condWithOffsetCase : condWithOffsetCases)
				{
					GroupPtr condWithOffsetGroup (new tcu::TestCaseGroup(testCtx, condWithOffsetCase.name, ""));

					for (const auto& inversionCase : inversionCases)
					{
						GroupPtr inversionGroup (new tcu::TestCaseGroup(testCtx, inversionCase.name, ""));

						for (const auto& useTaskCase : useTaskCases)
						{
							GroupPtr useTaskGroup (new tcu::TestCaseGroup(testCtx, useTaskCase.name, ""));

							for (const auto& condValue : condValues)
							{
								const auto			testName	= "value_" + paddedHex(condValue);
								const TestParams	params		=
								{
									drawTypeCase.drawType,				//	DrawType		drawType;
									cmdBufferTypeCase.cmdBufferType,	//	CmdBufferType	cmdBufferType;
									bindWithOffsetCase.bindWithOffset,	//	bool			bindWithOffset;
									condWithOffsetCase.condWithOffset,	//	bool			condWithOffset;
									condValue,							//	uint32_t		condValue;
									inversionCase.inverted,				//	bool			inverted;
									useTaskCase.useTask,				//	bool			useTask;
								};
								useTaskGroup->addChild(new ConditionalRenderingCase(testCtx, testName, "", params));
							}

							inversionGroup->addChild(useTaskGroup.release());
						}

						condWithOffsetGroup->addChild(inversionGroup.release());
					}

					bindWithOffsetGroup->addChild(condWithOffsetGroup.release());
				}

				cmdBufferTypeGroup->addChild(bindWithOffsetGroup.release());
			}

			drawTypeGroup->addChild(cmdBufferTypeGroup.release());
		}

		mainGroup->addChild(drawTypeGroup.release());
	}

	return mainGroup.release();
}

} // MeshShader
} // vkt
