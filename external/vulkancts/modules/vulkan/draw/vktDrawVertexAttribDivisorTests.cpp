/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Google LLC
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
 * \brief Vertex attribute divisor Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawVertexAttribDivisorTests.hpp"

#include <climits>

#include "vktDrawCreateInfoUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "rrShaders.hpp"
#include "rrRenderer.hpp"
#include "tcuImageCompare.hpp"
#include "shader_object/vktShaderObjectCreateUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

enum Extension {
	EXT = 0,
	KHR,
};

enum PipelineType {
	STATIC_PIPELINE = 0,
	DYNAMIC_PIPELINE,
	SHADER_OBJECTS,
};

enum DrawFunction
{
	DRAW = 0,
	DRAW_INDEXED,
	DRAW_INDIRECT,
	DRAW_INDEXED_INDIRECT,
	DRAW_MULTI_EXT,
	DRAW_MULTI_INDEXED_EXT,
	DRAW_INDIRECT_BYTE_COUNT_EXT,
	DRAW_INDIRECT_COUNT,
	DRAW_INDEXED_INDIRECT_COUNT,

	FUNTION_LAST
};

bool isIndirectDraw (DrawFunction drawFunction)
{
	switch (drawFunction)
	{
		case DRAW_INDIRECT:
		case DRAW_INDEXED_INDIRECT:
		case DRAW_INDIRECT_BYTE_COUNT_EXT:
		case DRAW_INDIRECT_COUNT:
		case DRAW_INDEXED_INDIRECT_COUNT:
			return true;
		default:
			break;
	}
	return false;
}

bool isIndexedDraw (DrawFunction drawFunction)
{
	switch (drawFunction)
	{
		case DRAW_INDEXED:
		case DRAW_INDEXED_INDIRECT:
		case DRAW_MULTI_INDEXED_EXT:
		case DRAW_INDEXED_INDIRECT_COUNT:
			return true;
		default:
			break;
	}
	return false;
}

bool isCountDraw (DrawFunction drawFunction)
{
	switch (drawFunction)
	{
		case DRAW_INDIRECT_COUNT:
		case DRAW_INDEXED_INDIRECT_COUNT:
			return true;
		default:
			break;
	}
	return false;
}

struct TestParams
{
	Extension				extension;
	PipelineType			pipelineType;
	DrawFunction			function;
	SharedGroupParams		groupParams;
	bool					firstInstanceZero;
	deUint32				attribDivisor;
};

struct VertexPositionAndColor
{
				VertexPositionAndColor (tcu::Vec4 position_, tcu::Vec4 color_)
									   : position	(position_)
									   , color		(color_)
									   { }

	tcu::Vec4	position;
	tcu::Vec4	color;
};

template<typename T>
de::SharedPtr<Buffer> createAndUploadBuffer(const std::vector<T> data, const vk::DeviceInterface& vk, const Context& context, vk::VkBufferUsageFlags usage)
{
	const vk::VkDeviceSize dataSize = data.size() * sizeof(T);
	de::SharedPtr<Buffer> buffer = Buffer::createAndAlloc(vk, context.getDevice(), BufferCreateInfo(dataSize, usage), context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(buffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &data[0], static_cast<size_t>(dataSize));
	vk::flushAlloc(vk, context.getDevice(), buffer->getBoundMemory());

	return buffer;
}

class TestVertShader : public rr::VertexShader
{
public:
	TestVertShader (int numInstances, int firstInstance)
		: rr::VertexShader	(2, 1)
		, m_numInstances	(numInstances)
		, m_firstInstance	(firstInstance)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_inputs[1].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs,
						rr::VertexPacket* const* packets,
						const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			const int		instanceNdx		= packets[packetNdx]->instanceNdx + m_firstInstance;
			const tcu::Vec4	position		= rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx,	packets[packetNdx]->vertexNdx, m_firstInstance);
			const tcu::Vec4	color			= rr::readVertexAttribFloat(inputs[1], packets[packetNdx]->instanceNdx,	packets[packetNdx]->vertexNdx, m_firstInstance);
			const tcu::Vec4	color2			= rr::readVertexAttribFloat(inputs[2], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx, m_firstInstance);
			packets[packetNdx]->position	= position + tcu::Vec4((float)(packets[packetNdx]->instanceNdx * 2.0 / m_numInstances), 0.0, 0.0, 0.0);
			packets[packetNdx]->outputs[0]	= color + tcu::Vec4((float)instanceNdx / (float)m_numInstances, 0.0, 0.0, 1.0) + color2;
		}
	}

private:
	const int m_numInstances;
	const int m_firstInstance;
};

class TestFragShader : public rr::FragmentShader
{
public:
	TestFragShader (void)
		: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets,
						 const int numPackets,
						 const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];
			for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				const tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			}
		}
	}
};

class VertexAttributeDivisorInstance : public TestInstance
{
public:
												VertexAttributeDivisorInstance	(Context& context, TestParams params);
	virtual	tcu::TestStatus						iterate					(void);

private:
	void										prepareVertexData		(int instanceCount, int firstInstance, int instanceDivisor);
	void										preRenderCommands		(const vk::VkClearValue& clearColor);
	void										draw					(vk::VkCommandBuffer cmdBuffer,
																		 vk::VkBuffer vertexBuffer, vk::VkBuffer instancedVertexBuffer,
																		 de::SharedPtr<Buffer> indexBuffer, de::SharedPtr<Buffer> indirectBuffer,
																		 de::SharedPtr<Buffer> countBuffer, deUint32 firstInstance, deUint32 instanceCount);

#ifndef CTS_USES_VULKANSC
	void										beginSecondaryCmdBuffer	(vk::VkRenderingFlagsKHR renderingFlags = 0u);
#endif // CTS_USES_VULKANSC

private:
	vk::VkFormat								m_colorAttachmentFormat = vk::VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32								m_width					= 128u;
	const deUint32								m_height				= 128u;
	const deUint32								m_quadGridSize			= 8u;

	const TestParams							m_params;
	const vk::DeviceInterface&					m_vk;

#ifndef CTS_USES_VULKANSC
	vk::Move<vk::VkShaderEXT>					m_vertexShader;
	vk::Move<vk::VkShaderEXT>					m_fragmentShader;
#endif
	vk::Move<vk::VkPipeline>					m_pipeline;
	vk::Move<vk::VkPipelineLayout>				m_pipelineLayout;

	de::SharedPtr<Image>						m_colorTargetImage;
	vk::Move<vk::VkImageView>					m_colorTargetView;

	PipelineCreateInfo::VertexInputState		m_vertexInputState;

	vk::Move<vk::VkCommandPool>					m_cmdPool;
	vk::Move<vk::VkCommandBuffer>				m_cmdBuffer;
	vk::Move<vk::VkCommandBuffer>				m_secCmdBuffer;

	vk::Move<vk::VkFramebuffer>					m_framebuffer;
	vk::Move<vk::VkRenderPass>					m_renderPass;

	// Vertex data
	std::vector<VertexPositionAndColor>			m_data;
	std::vector<deUint32>						m_indexes;
	std::vector<tcu::Vec4>						m_instancedColor;
};

VertexAttributeDivisorInstance::VertexAttributeDivisorInstance (Context &context, TestParams params)
	: TestInstance				(context)
	, m_params					(params)
	, m_vk						(context.getDeviceInterface())
{
	const vk::VkDevice device				= m_context.getDevice();
	const deUint32 queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();

	const vk::VkPushConstantRange pushConstantRange = {
		vk::VK_SHADER_STAGE_VERTEX_BIT,				// VkShaderStageFlags    stageFlags;
		0u,											// uint32_t              offset;
		(deUint32)sizeof(float) * 2,				// uint32_t              size;
	};

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(0, DE_NULL, 1, &pushConstantRange);
	m_pipelineLayout						= vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

	const vk::VkExtent3D targetImageExtent	= { m_width, m_height, 1 };
	const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1u, 1u, vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_colorTargetImage						= Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	ImageSubresourceRange subresourceRange = ImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT);

	const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat, subresourceRange);
	m_colorTargetView						= vk::createImageView(m_vk, device, &colorTargetViewInfo);

	if (!m_params.groupParams->useDynamicRendering)
	{
		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_GENERAL,
																 vk::VK_IMAGE_LAYOUT_GENERAL));

		const vk::VkAttachmentReference colorAttachmentReference =
		{
			0,
			vk::VK_IMAGE_LAYOUT_GENERAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
														   0,
														   0,
														   DE_NULL,
														   1,
														   &colorAttachmentReference,
														   DE_NULL,
														   AttachmentReference(),
														   0,
														   DE_NULL));

		m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

		// create framebuffer
		std::vector<vk::VkImageView>	colorAttachments { *m_colorTargetView };
		const FramebufferCreateInfo		framebufferCreateInfo(*m_renderPass, colorAttachments, m_width, m_height, 1);
		m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
	}

	const vk::VkVertexInputBindingDescription vertexInputBindingDescription[2] =
	{
		{
			0u,
			(deUint32)sizeof(VertexPositionAndColor),
			vk::VK_VERTEX_INPUT_RATE_VERTEX,
		},
		{
			1u,
			(deUint32)sizeof(tcu::Vec4),
			vk::VK_VERTEX_INPUT_RATE_INSTANCE,
		},
	};

	const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},
		{
			1u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			(deUint32)sizeof(tcu::Vec4),
		},
		{
			2u,
			1u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			0,
		}
	};

	m_vertexInputState = PipelineCreateInfo::VertexInputState(2,
															  vertexInputBindingDescription,
															  DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
															  vertexInputAttributeDescriptions);

	const vk::VkVertexInputBindingDivisorDescriptionEXT vertexInputBindingDivisorDescription =
	{
		1u,
		m_params.attribDivisor,
	};

	m_vertexInputState.addDivisors(1, &vertexInputBindingDivisorDescription);

	const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
	m_cmdPool	= vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
	m_cmdBuffer	= vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	if (m_params.groupParams->useSecondaryCmdBuffer)
		m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	if (m_params.pipelineType == SHADER_OBJECTS)
	{
#ifndef CTS_USES_VULKANSC
		const auto& vertSrc = m_context.getBinaryCollection().get("vert");
		const vk::VkShaderCreateInfoEXT vertexCreateInfo = {
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	//	VkStructureType					sType;
			DE_NULL,										//	const void*						pNext;
			0u,												//	VkShaderCreateFlagsEXT			flags;
			vk::VK_SHADER_STAGE_VERTEX_BIT,					//	VkShaderStageFlagBits			stage;
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,				//	VkShaderStageFlags				nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				//	VkShaderCodeTypeEXT				codeType;
			vertSrc.getSize(),								//	size_t							codeSize;
			vertSrc.getBinary(),							//	const void*						pCode;
			"main",											//	const char*						pName;
			0u,												//	uint32_t						setLayoutCount;
			DE_NULL,										//	const VkDescriptorSetLayout*	pSetLayouts;
			0u,												//	uint32_t						pushConstantRangeCount;
			DE_NULL,										//	const VkPushConstantRange*		pPushConstantRanges;
			DE_NULL,										//	const VkSpecializationInfo*		pSpecializationInfo;
		};
		m_vertexShader = vk::createShader(m_vk, device, vertexCreateInfo);

		const auto& fragSrc = m_context.getBinaryCollection().get("frag");
		const vk::VkShaderCreateInfoEXT fragmentCreateInfo = {
			vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	//	VkStructureType					sType;
			DE_NULL,										//	const void*						pNext;
			0u,												//	VkShaderCreateFlagsEXT			flags;
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,				//	VkShaderStageFlagBits			stage;
			0u,												//	VkShaderStageFlags				nextStage;
			vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				//	VkShaderCodeTypeEXT				codeType;
			fragSrc.getSize(),								//	size_t							codeSize;
			fragSrc.getBinary(),							//	const void*						pCode;
			"main",											//	const char*						pName;
			0u,												//	uint32_t						setLayoutCount;
			DE_NULL,										//	const VkDescriptorSetLayout*	pSetLayouts;
			0u,												//	uint32_t						pushConstantRangeCount;
			DE_NULL,										//	const VkPushConstantRange*		pPushConstantRanges;
			DE_NULL,										//	const VkSpecializationInfo*		pSpecializationInfo;
		};
		m_fragmentShader = vk::createShader(m_vk, device, fragmentCreateInfo);
#endif
	}
	else
	{
		const vk::Unique<vk::VkShaderModule> vs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get("vert"), 0));
		const vk::Unique<vk::VkShaderModule> fs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get("frag"), 0));

		const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

		vk::VkViewport	viewport	= vk::makeViewport(m_width, m_height);
		vk::VkRect2D	scissor		= vk::makeRect2D(m_width, m_height);

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

		if (m_params.pipelineType == DYNAMIC_PIPELINE)
		{
			vk::VkDynamicState dynStates[] = { vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT };
			vk::VkPipelineDynamicStateCreateInfo dynamicState
			{
				vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				DE_NULL,
				0u,
				1u,
				dynStates
			};
			pipelineCreateInfo.addState(dynamicState);
		}
		else
		{
			pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
		}

	#ifndef CTS_USES_VULKANSC
		vk::VkPipelineRenderingCreateInfoKHR renderingFormatCreateInfo
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			DE_NULL,
			0u,
			1u,
			&m_colorAttachmentFormat,
			vk::VK_FORMAT_UNDEFINED,
			vk::VK_FORMAT_UNDEFINED
		};

		if (m_params.groupParams->useDynamicRendering)
		{
			pipelineCreateInfo.pNext = &renderingFormatCreateInfo;
		}
	#endif // CTS_USES_VULKANSC

		m_pipeline = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo);
	}
}

tcu::TestStatus VertexAttributeDivisorInstance::iterate()
{
	const vk::VkQueue		queue					= m_context.getUniversalQueue();
	const vk::VkDevice		device					= m_context.getDevice();
	tcu::TestLog&			log						= m_context.getTestContext().getLog();
	static const deUint32	instanceCounts[]		= { 0u, 1u, 2u, 4u, 20u };
	const vk::VkRect2D		renderArea				= vk::makeRect2D(m_width, m_height);
	qpTestResult			res						= QP_TEST_RESULT_PASS;

	std::vector<deUint32>	firstInstanceIndices;
	if (m_params.firstInstanceZero)
		firstInstanceIndices = { 0u };
	else
		firstInstanceIndices = { 1u, 3u, 4u, 20u };

	const vk::VkClearValue clearColor = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	for (int instanceCountNdx = 0; instanceCountNdx < DE_LENGTH_OF_ARRAY(instanceCounts); instanceCountNdx++)
	{
		const deUint32 instanceCount = instanceCounts[instanceCountNdx];
		for (size_t firstInstanceIndexNdx = 0u; firstInstanceIndexNdx < firstInstanceIndices.size(); firstInstanceIndexNdx++)
		{
			// Prepare vertex data for at least one instance
			const deUint32				prepareCount			= de::max(instanceCount, 1u);
			const deUint32				firstInstance			= firstInstanceIndices[firstInstanceIndexNdx];

			prepareVertexData(prepareCount, firstInstance, m_params.attribDivisor);
			const de::SharedPtr<Buffer>	vertexBuffer			= createAndUploadBuffer(m_data, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			const de::SharedPtr<Buffer>	instancedVertexBuffer	= createAndUploadBuffer(m_instancedColor, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

			de::SharedPtr<Buffer> indexBuffer;
			if (isIndexedDraw(m_params.function))
				indexBuffer = createAndUploadBuffer(m_indexes, m_vk, m_context, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

			de::SharedPtr<Buffer> indirectBuffer;
			if (isIndirectDraw(m_params.function))
			{
				if (!isIndexedDraw(m_params.function)) {
					std::vector<vk::VkDrawIndirectCommand> drawCommands;
					drawCommands.push_back({
						(deUint32)m_data.size(),	// uint32_t	vertexCount;
						instanceCount,				// uint32_t	instanceCount;
						0u,							// uint32_t	firstVertex;
						firstInstance				// uint32_t	firstInstance;
						});
					indirectBuffer = createAndUploadBuffer(drawCommands, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
				}
				else
				{
					std::vector<vk::VkDrawIndexedIndirectCommand> drawCommands;
					drawCommands.push_back({
						(deUint32)m_indexes.size(),	// uint32_t	indexCount;
						instanceCount,				// uint32_t	instanceCount;
						0u,							// uint32_t	firstIndex;
						0,							// int32_t	vertexOffset;
						firstInstance				// uint32_t	firstInstance;
						});
					indirectBuffer = createAndUploadBuffer(drawCommands, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
				}
			}
			de::SharedPtr<Buffer> countBuffer;
			if (isCountDraw(m_params.function)) {
				std::vector<deUint32> count = { 1 };
				countBuffer = createAndUploadBuffer(count, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
			}
			else if (m_params.function == DRAW_INDIRECT_BYTE_COUNT_EXT)
			{
				std::vector<deUint32> count = { (deUint32)m_data.size() };
				countBuffer = createAndUploadBuffer(count, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
			}

#ifndef CTS_USES_VULKANSC
			if (m_params.groupParams->useSecondaryCmdBuffer)
			{
				// record secondary command buffer
				if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				{
					beginSecondaryCmdBuffer(vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
					beginRendering(m_vk, *m_secCmdBuffer, *m_colorTargetView, renderArea, clearColor, vk::VK_IMAGE_LAYOUT_GENERAL,
								   vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, 1u, 0x0);
				}
				else
					beginSecondaryCmdBuffer();

				draw(*m_secCmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer, indirectBuffer, countBuffer, firstInstance, instanceCount);

				if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
					endRendering(m_vk, *m_secCmdBuffer);

				endCommandBuffer(m_vk, *m_secCmdBuffer);

				// record primary command buffer
				beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

				preRenderCommands(clearColor);

				if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				{
					beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR, 1u, 0x0);
				}

				m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

				if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
					endRendering(m_vk, *m_cmdBuffer);

				endCommandBuffer(m_vk, *m_cmdBuffer);
			}
			else if (m_params.groupParams->useDynamicRendering)
			{
				beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
				preRenderCommands(clearColor);

				beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor, vk::VK_IMAGE_LAYOUT_GENERAL,
							   vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, 1u, 0x0);
				draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer, indirectBuffer, countBuffer, firstInstance, instanceCount);
				endRendering(m_vk, *m_cmdBuffer);

				endCommandBuffer(m_vk, *m_cmdBuffer);
			}
#endif // CTS_USES_VULKANSC

			if (!m_params.groupParams->useDynamicRendering)
			{
				beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
				preRenderCommands(clearColor);

				beginRenderPass(m_vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);
				draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer, indirectBuffer, countBuffer, firstInstance, instanceCount);
				endRenderPass(m_vk, *m_cmdBuffer);

				endCommandBuffer(m_vk, *m_cmdBuffer);
			}

			submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
			m_context.resetCommandPoolForVKSC(device, *m_cmdPool);

			// Reference rendering
			std::vector<tcu::Vec4>	vertices;
			std::vector<tcu::Vec4>	colors;

			for (std::vector<VertexPositionAndColor>::const_iterator it = m_data.begin(); it != m_data.end(); ++it)
			{
				vertices.push_back(it->position);
				colors.push_back(it->color);
			}

			tcu::TextureLevel refImage (vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + m_width), (int)(0.5 + m_height));

			tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

			const TestVertShader					vertShader		(instanceCount, firstInstance);
			const TestFragShader					fragShader;
			const rr::Program						program			(&vertShader, &fragShader);
			const rr::MultisamplePixelBufferAccess	colorBuffer		= rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
			const rr::RenderTarget					renderTarget	(colorBuffer);
			const rr::RenderState					renderState		((rr::ViewportState(colorBuffer)), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
			const rr::Renderer						renderer;

			const rr::VertexAttrib	vertexAttribs[] =
			{
				rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertices[0]),
				rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0]),
				// The reference renderer treats a divisor of 0 as meaning per-vertex.  Use INT_MAX instead; it should work just as well.
				rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), m_params.attribDivisor == 0 ? INT_MAX : m_params.attribDivisor, &m_instancedColor[0])
			};

			if (!isIndexedDraw(m_params.function))
			{
				const rr::PrimitiveList	primitives = rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLE_STRIP, (int)vertices.size(), 0);
				const rr::DrawCommand	command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs), &vertexAttribs[0],
												primitives);
				renderer.drawInstanced(command, instanceCount);
			}
			else
			{
				const rr::DrawIndices indicies(m_indexes.data());

				const rr::PrimitiveList	primitives = rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLE_STRIP, (int)m_indexes.size(), indicies);
				const rr::DrawCommand	command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs), &vertexAttribs[0],
												primitives);
				renderer.drawInstanced(command, instanceCount);
			}

			const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
			const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
				vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, m_width, m_height, vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u);

			std::ostringstream resultDesc;
			resultDesc << "Instance count: " << instanceCount << " first instance index: " << firstInstance;

			if (!tcu::fuzzyCompare(log, "Result", resultDesc.str().c_str(), refImage.getAccess(), renderedFrame, 0.05f, tcu::COMPARE_LOG_RESULT))
				res = QP_TEST_RESULT_FAIL;
		}
	}
	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void VertexAttributeDivisorInstance::prepareVertexData(int instanceCount, int firstInstance, int instanceDivisor)
{
	m_data.clear();
	m_indexes.clear();
	m_instancedColor.clear();

	if (!isIndexedDraw(m_params.function))
	{
		for (deUint32 y = 0; y < m_quadGridSize; ++y)
		{
			for (deUint32 x = 0; x < m_quadGridSize; ++x)
			{
				const float fx0 = -1.0f + (float)(x + 0) / (float)m_quadGridSize * 2.0f / (float)instanceCount;
				const float fx1 = -1.0f + (float)(x + 1) / (float)m_quadGridSize * 2.0f / (float)instanceCount;
				const float fy0 = -1.0f + (float)(y + 0) / (float)m_quadGridSize * 2.0f;
				const float fy1 = -1.0f + (float)(y + 1) / (float)m_quadGridSize * 2.0f;

				// Vertices of a quad's lower-left triangle: (fx0, fy0), (fx1, fy0) and (fx0, fy1)
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

				// Vertices of a quad's upper-right triangle: (fx1, fy1), (fx0, fy1) and (fx1, fy0)
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
			}
		}
	}
	else
	{
		for (deUint32 y = 0; y < m_quadGridSize + 1; ++y)
		{
			for (deUint32 x = 0; x < m_quadGridSize + 1; ++x)
			{
				const float fx = -1.0f + (float)x / (float)m_quadGridSize * 2.0f / (float)instanceCount;
				const float fy = -1.0f + (float)y / (float)m_quadGridSize * 2.0f;

				m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx, fy, 1.0f, 1.0f),
														(y % 2 ? tcu::RGBA::blue().toVec() : tcu::RGBA::green().toVec())));
			}
		}

		for (deUint32 y = 0; y < m_quadGridSize; ++y)
		{
			for (deUint32 x = 0; x < m_quadGridSize; ++x)
			{
				const int ndx00 = y * (m_quadGridSize + 1) + x;
				const int ndx10 = y * (m_quadGridSize + 1) + x + 1;
				const int ndx01 = (y + 1)*(m_quadGridSize + 1) + x;
				const int ndx11 = (y + 1)*(m_quadGridSize + 1) + x + 1;

				// Lower-left triangle of a quad.
				m_indexes.push_back((deUint16)ndx00);
				m_indexes.push_back((deUint16)ndx10);
				m_indexes.push_back((deUint16)ndx01);

				// Upper-right triangle of a quad.
				m_indexes.push_back((deUint16)ndx11);
				m_indexes.push_back((deUint16)ndx01);
				m_indexes.push_back((deUint16)ndx10);
			}
		}
	}

	const int colorCount = instanceDivisor == 0 ? 1 : (instanceCount + firstInstance + instanceDivisor - 1) / instanceDivisor;
	for (int i = 0; i < instanceCount + firstInstance; i++)
	{
		m_instancedColor.push_back(tcu::Vec4(0.0, (float)(1.0 - i * 1.0 / colorCount) / 2, 0.0, 1.0));
	}
}

void VertexAttributeDivisorInstance::preRenderCommands (const vk::VkClearValue& clearColor)
{
	const ImageSubresourceRange subresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

	m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &subresourceRange);

	const vk::VkMemoryBarrier memBarrier
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
}

void VertexAttributeDivisorInstance::draw (vk::VkCommandBuffer cmdBuffer, vk::VkBuffer vertexBuffer, vk::VkBuffer instancedVertexBuffer,
										   de::SharedPtr<Buffer> indexBuffer, de::SharedPtr<Buffer> indirectBuffer,
										   de::SharedPtr<Buffer> countBuffer, deUint32 firstInstance, deUint32 instanceCount)
{
	if (m_params.pipelineType != PipelineType::SHADER_OBJECTS)
	{
		m_vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	}
	else
	{
#ifndef CTS_USES_VULKANSC
		vk::bindGraphicsShaders(m_vk, cmdBuffer, m_vertexShader.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, m_fragmentShader.get(), m_context.getMeshShaderFeatures().taskShader, m_context.getMeshShaderFeatures().meshShader);
		vk::setDefaultShaderObjectDynamicStates(m_vk, cmdBuffer, m_context.getDeviceExtensions());
		vk::bindNullMeshShaders(m_vk, cmdBuffer, m_context.getMeshShaderFeaturesEXT());

		vk::VkViewport	viewport	= vk::makeViewport(m_width, m_height);
		vk::VkRect2D	scissor		= vk::makeRect2D(m_width, m_height);
		m_vk.cmdSetViewportWithCount(cmdBuffer, 1u, &viewport);
		m_vk.cmdSetScissorWithCount(cmdBuffer, 1u, &scissor);
#endif
	}

	if (isIndexedDraw(m_params.function))
		m_vk.cmdBindIndexBuffer(cmdBuffer, indexBuffer->object(), 0, vk::VK_INDEX_TYPE_UINT32);

	const vk::VkBuffer		vertexBuffers[]			{ vertexBuffer,		instancedVertexBuffer	};
	const vk::VkDeviceSize	vertexBufferOffsets[]	{ 0,				0 };

	m_vk.cmdBindVertexBuffers(cmdBuffer, 0, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	const float pushConstants[] = { (float)firstInstance, (float)instanceCount };
	m_vk.cmdPushConstants(cmdBuffer, *m_pipelineLayout, vk::VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(pushConstants), pushConstants);

	if (m_params.pipelineType != PipelineType::STATIC_PIPELINE)
	{
		vk::VkVertexInputBindingDescription2EXT vertexBindingDescription[2]
		{
			{
				vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
				DE_NULL,
				0u,
				(deUint32)sizeof(VertexPositionAndColor),
				vk::VK_VERTEX_INPUT_RATE_VERTEX,
				1u
			},
			{
				vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
				DE_NULL,
				1u,
				(deUint32)sizeof(tcu::Vec4),
				vk::VK_VERTEX_INPUT_RATE_INSTANCE,
				m_params.attribDivisor
			},

		};
		vk::VkVertexInputAttributeDescription2EXT vertexAttributeDescription[3]
		{
			{
				vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
				DE_NULL,
				0u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				0u
			},
			{
				vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
				DE_NULL,
				1u,
				0u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				(deUint32)sizeof(tcu::Vec4),
			},
			{
				vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
				DE_NULL,
				2u,
				1u,
				vk::VK_FORMAT_R32G32B32A32_SFLOAT,
				0u,
			}
		};

		m_vk.cmdSetVertexInputEXT(cmdBuffer, 2u, vertexBindingDescription, 3u, vertexAttributeDescription);
	}

#ifndef CTS_USES_VULKANSC
	vk::VkMultiDrawInfoEXT multiDrawInfo;
	multiDrawInfo.firstVertex = 0u;
	multiDrawInfo.vertexCount = (deUint32)m_data.size();
	vk::VkMultiDrawIndexedInfoEXT multiDrawIndexedInfo;
	multiDrawIndexedInfo.firstIndex = 0u;
	multiDrawIndexedInfo.indexCount = (deUint32)m_indexes.size();
	multiDrawIndexedInfo.vertexOffset = 0u;
	deInt32 vertexOffset = 0u;
#endif

	switch (m_params.function)
	{
	case DrawFunction::DRAW:
		m_vk.cmdDraw(cmdBuffer, (deUint32)m_data.size(), instanceCount, 0u, firstInstance);
		break;
	case DrawFunction::DRAW_INDEXED:
		m_vk.cmdDrawIndexed(cmdBuffer, (deUint32)m_indexes.size(), instanceCount, 0u, 0u, firstInstance);
		break;
	case DrawFunction::DRAW_INDEXED_INDIRECT:
		m_vk.cmdDrawIndexedIndirect(cmdBuffer, indirectBuffer->object(), 0, 1u, 0u);
		break;
	case DrawFunction::DRAW_INDEXED_INDIRECT_COUNT:
		m_vk.cmdDrawIndexedIndirectCount(cmdBuffer, indirectBuffer->object(), 0, countBuffer->object(), 0u, 1u, sizeof(vk::VkDrawIndexedIndirectCommand));
		break;
	case DrawFunction::DRAW_INDIRECT:
		m_vk.cmdDrawIndirect(cmdBuffer, indirectBuffer->object(), 0, 1u, 0u);
		break;
	case DrawFunction::DRAW_INDIRECT_COUNT:
		m_vk.cmdDrawIndirectCount(cmdBuffer, indirectBuffer->object(), 0, countBuffer->object(), 0u, 1u, sizeof(vk::VkDrawIndirectCommand));
		break;
#ifndef CTS_USES_VULKANSC
	case DrawFunction::DRAW_INDIRECT_BYTE_COUNT_EXT:
		m_vk.cmdDrawIndirectByteCountEXT(cmdBuffer, instanceCount, firstInstance, countBuffer->object(), 0u, 0u, 1u);
		break;
	case DrawFunction::DRAW_MULTI_EXT:
		m_vk.cmdDrawMultiEXT(cmdBuffer, 1u, &multiDrawInfo, instanceCount, firstInstance, sizeof(vk::VkMultiDrawInfoEXT));
		break;
	case DrawFunction::DRAW_MULTI_INDEXED_EXT:
		m_vk.cmdDrawMultiIndexedEXT(cmdBuffer, 1u, &multiDrawIndexedInfo, instanceCount, firstInstance, sizeof(vk::VkMultiDrawIndexedInfoEXT), &vertexOffset);
		break;
#endif
	default:
		DE_ASSERT(false);
	}
}

#ifndef CTS_USES_VULKANSC
void VertexAttributeDivisorInstance::beginSecondaryCmdBuffer (vk::VkRenderingFlagsKHR renderingFlags)
{
	const vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&m_colorAttachmentFormat,												// const VkFormat*					pColorAttachmentFormats;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							depthAttachmentFormat;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							stencilAttachmentFormat;
		vk::VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			rasterizationSamples;
	};

	const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,					// VkStructureType					sType;
		&inheritanceRenderingInfo,												// const void*						pNext;
		DE_NULL,																// VkRenderPass						renderPass;
		0u,																		// deUint32							subpass;
		DE_NULL,																// VkFramebuffer					framebuffer;
		VK_FALSE,																// VkBool32							occlusionQueryEnable;
		(vk::VkQueryControlFlags)0u,											// VkQueryControlFlags				queryFlags;
		(vk::VkQueryPipelineStatisticFlags)0u									// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const vk::VkCommandBufferBeginInfo commandBufBeginParams
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,						// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	VK_CHECK(m_vk.beginCommandBuffer(*m_secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class VertexAttributeDivisorCase : public TestCase
{
public:
					VertexAttributeDivisorCase (tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const TestParams& params)
											   : TestCase	(testCtx, name, desc)
											   , m_params	(params)
											   {}

	virtual void	checkSupport	(Context& context) const override;
	virtual void	initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override { return new VertexAttributeDivisorInstance(context, m_params); }

private:
	const TestParams	m_params;
};


void VertexAttributeDivisorCase::checkSupport (Context& context) const
{
	const auto attributeDivisorFeatures = context.getVertexAttributeDivisorFeatures();
	if (m_params.extension == Extension::EXT)
	{
		context.requireDeviceFunctionality("VK_EXT_vertex_attribute_divisor");
	}
	else if (m_params.extension == Extension::KHR)
	{
		context.requireDeviceFunctionality("VK_KHR_vertex_attribute_divisor");
#ifndef CTS_USES_VULKANSC
		const vk::VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR& vertexAttributeDivisorProperties = context.getVertexAttributeDivisorProperties();
		if (!m_params.firstInstanceZero && !vertexAttributeDivisorProperties.supportsNonZeroFirstInstance)
			TCU_THROW(NotSupportedError, "supportsNonZeroFirstInstance not supported");
#endif
	}
	if (!m_params.firstInstanceZero && isIndirectDraw(m_params.function) && !context.getDeviceFeatures().drawIndirectFirstInstance) {
		TCU_THROW(NotSupportedError, "drawIndirectFirstInstancenot supported");
	}
	if (m_params.attribDivisor == 1u && !attributeDivisorFeatures.vertexAttributeInstanceRateDivisor)
		TCU_THROW(NotSupportedError, "vertexAttributeInstanceRateDivisor not supported");
	if (m_params.attribDivisor == 0u && !attributeDivisorFeatures.vertexAttributeInstanceRateZeroDivisor)
		TCU_THROW(NotSupportedError, "vertexAttributeInstanceRateZeroDivisor not supported");

	if (m_params.pipelineType == PipelineType::DYNAMIC_PIPELINE)
		context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
	else if (m_params.pipelineType == PipelineType::SHADER_OBJECTS)
		context.requireDeviceFunctionality("VK_EXT_shader_object");

	if (m_params.function == DrawFunction::DRAW_MULTI_EXT || m_params.function == DrawFunction::DRAW_MULTI_INDEXED_EXT)
		context.requireDeviceFunctionality("VK_EXT_multi_draw");
	if (isIndirectDraw(m_params.function))
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
	if (m_params.function == DrawFunction::DRAW_INDIRECT_BYTE_COUNT_EXT)
		context.requireDeviceFunctionality("VK_EXT_transform_feedback");

	if (m_params.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}


void VertexAttributeDivisorCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::string vertSrc = "#version 430\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in vec4 in_color;\n"
		"layout(location = 2) in vec4 in_color_2;\n"
		"layout(push_constant) uniform TestParams {\n"
		"	float firstInstance;\n"
		"	float instanceCount;\n"
		"} params;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"out gl_PerVertex {\n"
		"    vec4  gl_Position;\n"
		"    float gl_PointSize;\n"
		"};\n"
		"void main() {\n"
		"    gl_PointSize = 1.0;\n"
		"    gl_Position  = in_position + vec4(float(gl_InstanceIndex - params.firstInstance) * 2.0 / params.instanceCount, 0.0, 0.0, 0.0);\n"
		"    out_color    = in_color + vec4(float(gl_InstanceIndex) / params.instanceCount, 0.0, 0.0, 1.0) + in_color_2;\n"
		"}\n";

	std::string fragSrc = "#version 430\n"
		"layout(location = 0) in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"    out_color = in_color;\n"
		"}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc);
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc);
}

} // anonymus

tcu::TestCaseGroup* createVertexAttributeDivisorTests(tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	de::MovePtr<tcu::TestCaseGroup> vertexAttributeDivisorGroup(new tcu::TestCaseGroup(testCtx, "vertex_attribute_divisor", ""));

	const struct
	{
		Extension	extension;
		const char* name;
		const char* description;
	} extensionTests[] =
	{
		{ EXT,	"ext",	"Test VK_EXT_vertex_attribute_divisor" },
		{ KHR,	"khr",	"Test VK_KHR_vertex_attribute_divisor" },
	};

	const struct
	{
		PipelineType pipelineType;
		const char* name;
		const char* description;
	} pipelineTests[] = {
		{ STATIC_PIPELINE,	"static_pipeline",	"Use a pipeline without dynamic state" },
		{ DYNAMIC_PIPELINE, "dynamic_pipeline", "Use a pipeline with dynamic state" },
		{ SHADER_OBJECTS,	"shader_objects",	"Use shader objects" },
	};

	const struct
	{
		DrawFunction drawFunction;
		const char* name;
		const char* description;
	} drawTests[] = {
		{ DRAW,							"draw",							"Test vkCmdDraw"						},
		{ DRAW_INDEXED,					"draw_indexed",					"Test vkCmdDrawIndexed"					},
		{ DRAW_INDIRECT,				"draw_indirect",				"Test vkCmdDrawIndirect"				},
		{ DRAW_INDEXED_INDIRECT,		"draw_indexed_indirect",		"Test vkCmdDrawIndexedIndirect"			},
		{ DRAW_MULTI_EXT,				"draw_multi_ext",				"Test vkCmdDrawMultiEXT"				},
		{ DRAW_MULTI_INDEXED_EXT,		"draw_multi_indexed_ext",		"Test vkCmdDrawMultiIndexedEXT"			},
		{ DRAW_INDIRECT_COUNT,			"draw_indirect_count",			"Test vkCmdDrawIndirectCount"			},
		{ DRAW_INDEXED_INDIRECT_COUNT,	"draw_indexed_indirect_count",	"Test vkCmdDrawIndexedIndirectCount"	},
	};

	const struct
	{
		bool firstInstanceZero;
		const char* name;
		const char* description;
	} firstInstanceTests[] = {
		{ true,		"zero",			"First instance 0" },
		{ false,	"non_zero",		"First instance not 0" },
	};

	const struct
	{
		deUint32 attribDivisor;
		const char* name;
		const char* description;
	} vertexAttributeDivisorTests[] = {
		{ 0u,			"0",			"Vertex attribute divisor 0" },
		{ 1u,			"1",			"Vertex attribute divisor 1" },
		{ 2u,			"2",			"Vertex attribute divisor 2" },
		{ 16u,			"16",			"Vertex attribute divisor 16" },
	};

	for (const auto& extensionTest : extensionTests) {
		de::MovePtr<tcu::TestCaseGroup> extensionGroup(new tcu::TestCaseGroup(testCtx, extensionTest.name, extensionTest.description));

		for (const auto& pipelineTest : pipelineTests) {
			if (pipelineTest.pipelineType == PipelineType::SHADER_OBJECTS && !groupParams->useDynamicRendering)
				continue;

			de::MovePtr<tcu::TestCaseGroup> pipelineGroup(new tcu::TestCaseGroup(testCtx, pipelineTest.name, pipelineTest.description));

			for (const auto& drawTest : drawTests) {
				de::MovePtr<tcu::TestCaseGroup> drawGroup(new tcu::TestCaseGroup(testCtx, drawTest.name, drawTest.description));

				for (const auto& firstInstanceTest : firstInstanceTests) {
					de::MovePtr<tcu::TestCaseGroup> firstInstanceGroup(new tcu::TestCaseGroup(testCtx, firstInstanceTest.name, firstInstanceTest.description));

					for (const auto& vertexAttributeDivisorTest : vertexAttributeDivisorTests) {
						const TestParams params = {
							extensionTest.extension,						// Extension				extension;
							pipelineTest.pipelineType,						// PipelineType				pipelineType;
							drawTest.drawFunction,							// DrawFunction				drawFunction;
							groupParams,									// const SharedGroupParams	groupParams;
							firstInstanceTest.firstInstanceZero,			// bool						firstInstanceZero;
							vertexAttributeDivisorTest.attribDivisor,		// deUint32					attribDivisor;
						};

						firstInstanceGroup->addChild(new VertexAttributeDivisorCase(testCtx, vertexAttributeDivisorTest.name, vertexAttributeDivisorTest.description, params));
					}

					drawGroup->addChild(firstInstanceGroup.release());
				}

				pipelineGroup->addChild(drawGroup.release());
			}

			extensionGroup->addChild(pipelineGroup.release());
		}

		vertexAttributeDivisorGroup->addChild(extensionGroup.release());
	}

	return vertexAttributeDivisorGroup.release();
}

} // Draw
} // vkt
