/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google LLC
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Android Hardware Buffer Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawAhbTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "../util/vktExternalMemoryUtil.hpp"

#include "deDefs.h"
#include "deRandom.hpp"

#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"

#include "rrRenderer.hpp"

#include <string>

using namespace vkt::ExternalMemoryUtil;
using namespace vk;
using std::vector;

namespace vkt
{
namespace Draw
{
namespace
{

const deUint32 SEED = 0xc2a39fu;

struct DrawParams
{
	DrawParams (deUint32 numVertices, deUint32 numLayers)
	: m_numVertices(numVertices), m_numLayers(numLayers) {}

	deUint32					m_numVertices;
	deUint32					m_numLayers;
	vector<PositionColorVertex>	m_vertices;
};

// Reference renderer shaders
class PassthruVertShader : public rr::VertexShader
{
public:
	PassthruVertShader (void)
	: rr::VertexShader (2, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_inputs[1].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0],
																	 packets[packetNdx]->instanceNdx,
																	 packets[packetNdx]->vertexNdx);

			tcu::Vec4 color = rr::readVertexAttribFloat(inputs[1],
														packets[packetNdx]->instanceNdx,
														packets[packetNdx]->vertexNdx);

			packets[packetNdx]->outputs[0] = color;
		}
	}
};

class PassthruFragShader : public rr::FragmentShader
{
public:
	PassthruFragShader (void)
		: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];
			for (deUint32 fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			}
		}
	}
};

class AhbTestInstance : public TestInstance
{
public:
					AhbTestInstance		(Context& context, DrawParams data);
					~AhbTestInstance	(void);

	tcu::TestStatus	iterate				(void);

private:
	void			generateDrawData	(void);
	void			generateRefImage	(const tcu::PixelBufferAccess& access, const vector<tcu::Vec4>& vertices, const vector<tcu::Vec4>& colors) const;

	DrawParams							m_data;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

AhbTestInstance::AhbTestInstance (Context& context, DrawParams data)
	: vkt::TestInstance			(context)
	, m_data					(data)
{
	generateDrawData();
}

AhbTestInstance::~AhbTestInstance (void)
{
}

void AhbTestInstance::generateRefImage (const tcu::PixelBufferAccess& access, const vector<tcu::Vec4>& vertices, const vector<tcu::Vec4>& colors) const
{
	const PassthruVertShader				vertShader;
	const PassthruFragShader				fragShader;
	const rr::Program						program			(&vertShader, &fragShader);
	const rr::MultisamplePixelBufferAccess	colorBuffer		= rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(access);
	const rr::RenderTarget					renderTarget	(colorBuffer);
	const rr::RenderState					renderState		((rr::ViewportState(colorBuffer)), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	const rr::Renderer						renderer;

	const rr::VertexAttrib	vertexAttribs[] =
	{
		rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertices[0]),
		rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0])
	};

	renderer.draw(rr::DrawCommand(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
								  &vertexAttribs[0], rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, (deUint32)vertices.size(), 0)));
}

class AhbTestCase : public TestCase
{
	public:
									AhbTestCase			(tcu::TestContext& context, const char* name, const char* desc, const DrawParams data);
									~AhbTestCase		(void);
	virtual	void					initPrograms		(SourceCollections& programCollection) const;
	virtual void					initShaderSources	(void);
	virtual void					checkSupport		(Context& context) const;
	virtual TestInstance*			createInstance		(Context& context) const;

private:
	DrawParams											m_data;
	std::string											m_vertShaderSource;
	std::string											m_fragShaderSource;
};

AhbTestCase::AhbTestCase (tcu::TestContext& context, const char* name, const char* desc, const DrawParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
	initShaderSources();
}

AhbTestCase::~AhbTestCase	(void)
{
}

void AhbTestCase::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(m_vertShaderSource);
	programCollection.glslSources.add("frag") << glu::FragmentSource(m_fragShaderSource);
}

void AhbTestCase::checkSupport (Context& context) const
{
	const InstanceInterface&				vki				= context.getInstanceInterface();
	vk::VkPhysicalDevice					physicalDevice	= context.getPhysicalDevice();
	const vk::VkPhysicalDeviceProperties	properties		= vk::getPhysicalDeviceProperties(vki, physicalDevice);

	// Each layer is exposed as its own color attachment.
	if (m_data.m_numLayers > properties.limits.maxColorAttachments)
		TCU_THROW(NotSupportedError, "Required number of color attachments not supported.");
}

void AhbTestCase::initShaderSources (void)
{
	std::stringstream vertShader;
	vertShader	<< "#version 430\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "layout(location = 1) in vec4 in_color;\n"
				<< "layout(location = 0) out vec4 out_color;\n"

				<< "void main() {\n"
				<< "    gl_Position  = in_position;\n"
				<< "    out_color    = in_color;\n"
				<< "}\n";

	m_vertShaderSource = vertShader.str();

	std::stringstream fragShader;
	fragShader	<< "#version 430\n"
				<< "layout(location = 0) in vec4 in_color;\n"
				<< "layout(location = 0) out vec4 out_color;\n"

				<< "void main()\n"
				<< "{\n"
				<< "    out_color = in_color;\n"
				<< "}\n";

	m_fragShaderSource = fragShader.str();
}

TestInstance* AhbTestCase::createInstance (Context& context) const
{
	return new AhbTestInstance(context, m_data);
}

void AhbTestInstance::generateDrawData (void)
{
	de::Random rnd (SEED ^ m_data.m_numLayers ^ m_data.m_numVertices);

	for (deUint32 i = 0; i < m_data.m_numVertices; i++)
	{
		const float f0 = rnd.getFloat(-1.0f, 1.0f);
		const float f1 = rnd.getFloat(-1.0f, 1.0f);

		m_data.m_vertices.push_back(PositionColorVertex(
			tcu::Vec4(f0, f1, 1.0f, 1.0f),	// Coord
			tcu::randomVec4(rnd)));			// Color
	}
}

tcu::TestStatus AhbTestInstance::iterate (void)
{
	const DeviceInterface&						vk						= m_context.getDeviceInterface();
	const VkFormat								colorAttachmentFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	tcu::TestLog								&log					= m_context.getTestContext().getLog();
	const VkQueue								queue					= m_context.getUniversalQueue();
	const VkDevice								device					= m_context.getDevice();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const PipelineLayoutCreateInfo				pipelineLayoutCreateInfo;
	const Unique<VkPipelineLayout>				pipelineLayout			(createPipelineLayout(vk, device, &pipelineLayoutCreateInfo));
	vector<Move<VkBuffer>>						resultBuffers;
	vector<de::MovePtr<Allocation>>				resultBufferAllocations;

	for (deUint32 i = 0u; i < m_data.m_numLayers; i++)
	{
		const VkBufferUsageFlags	bufferUsage	(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkDeviceSize			pixelSize	= mapVkFormat(colorAttachmentFormat).getPixelSize();
		const VkBufferCreateInfo	createInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
			DE_NULL,								// const void*			pNext
			0u,										// VkBufferCreateFlags	flags
			WIDTH * HEIGHT * pixelSize,				// VkDeviceSize			size
			bufferUsage,							// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
			0u,										// uint32_t				queueFamilyIndexCount
			DE_NULL									// const uint32_t*		pQueueFamilyIndices
		};

		resultBuffers.push_back(createBuffer(vk, device, &createInfo));
		resultBufferAllocations.push_back(m_context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, device, *resultBuffers.back()), MemoryRequirement::HostVisible));
		VK_CHECK(vk.bindBufferMemory(device, *resultBuffers.back(), resultBufferAllocations.back()->getMemory(), resultBufferAllocations.back()->getOffset()));
	}

	const VkExtent3D							targetImageExtent		= { WIDTH, HEIGHT, 1 };
	const ImageCreateInfo						targetImageCreateInfo	(VK_IMAGE_TYPE_2D, colorAttachmentFormat, targetImageExtent, 1, m_data.m_numLayers, VK_SAMPLE_COUNT_1_BIT,
																	 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	// Enable this to use non-AHB images for color output.
#if 0
	const Unique<VkImage>						colorTargetImage		(createImage(vk, device, &targetImageCreateInfo, DE_NULL));
	de::MovePtr<Allocation>						m_colorImageAllocation	= m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, device, *colorTargetImage), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(device, *colorTargetImage, m_colorImageAllocation->getMemory(), m_colorImageAllocation->getOffset()));
#else
	AndroidHardwareBufferExternalApi*			ahbApi					= AndroidHardwareBufferExternalApi::getInstance();

	if (!ahbApi)
		TCU_THROW(NotSupportedError, "Android Hardware Buffer not supported");

	m_context.requireDeviceFunctionality("VK_ANDROID_external_memory_android_hardware_buffer");

	deUint64									requiredAhbUsage		= ahbApi->vkUsageToAhbUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	pt::AndroidHardwareBufferPtr ahb = ahbApi->allocate(WIDTH, HEIGHT, targetImageCreateInfo.arrayLayers, ahbApi->vkFormatToAhbFormat(colorAttachmentFormat), requiredAhbUsage);

	if (ahb.internal == DE_NULL)
		TCU_THROW(NotSupportedError, "Required number of layers for Android Hardware Buffer not supported");

	NativeHandle								nativeHandle(ahb);
	const Unique<VkImage>						colorTargetImage	(createExternalImage(vk, device, queueFamilyIndex, VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
																	 colorAttachmentFormat, WIDTH, HEIGHT, VK_IMAGE_TILING_OPTIMAL, 0u,
																	 targetImageCreateInfo.usage, targetImageCreateInfo.mipLevels, targetImageCreateInfo.arrayLayers));

	deUint32									ahbFormat			= 0;
	ahbApi->describe(nativeHandle.getAndroidHardwareBuffer(), DE_NULL, DE_NULL, DE_NULL, &ahbFormat, DE_NULL, DE_NULL);

	VkAndroidHardwareBufferPropertiesANDROID	ahbProperties		=
	{
		VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,	// VkStructureType    sType
		DE_NULL,														// void*              pNext
		0u,																// VkDeviceSize       allocationSize
		0u																// uint32_t           memoryTypeBits
	};

	vk.getAndroidHardwareBufferPropertiesANDROID(device, nativeHandle.getAndroidHardwareBuffer(), &ahbProperties);

	const VkImportAndroidHardwareBufferInfoANDROID	importInfo		=
	{
		VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,	// VkStructureType            sType
		DE_NULL,														// const void*                pNext
		nativeHandle.getAndroidHardwareBuffer()							// struct AHardwareBuffer*    buffer
	};

	const VkMemoryDedicatedAllocateInfo			dedicatedInfo		=
	{
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,	// VkStructureType    sType
		&importInfo,											// const void*        pNext
		*colorTargetImage,										// VkImage            image
		DE_NULL,												// VkBuffer           buffer
	};

	const VkMemoryAllocateInfo					allocateInfo		=
	{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,			// VkStructureType    sType
			(const void*)&dedicatedInfo,					// const void*        pNext
			ahbProperties.allocationSize,					// VkDeviceSize       allocationSize
			chooseMemoryType(ahbProperties.memoryTypeBits)	// uint32_t           memoryTypeIndex
	};

	const Unique<VkDeviceMemory>				colorImageMemory	(allocateMemory(vk, device, &allocateInfo));
	VK_CHECK(vk.bindImageMemory(device, *colorTargetImage, *colorImageMemory, 0u));
#endif

	vector<Move<VkImageView>>					imageViews;
	vector<VkImageView>							colorAttachments;
	RenderPassCreateInfo						renderPassCreateInfo;

	for (deUint32 i = 0u; i < m_data.m_numLayers; i++)
	{
		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags    aspectMask
			0u,							// uint32_t              baseMipLevel
			1u,							// uint32_t              levelCount
			i,							// uint32_t              baseArrayLayer
			1u,							// uint32_t              layerCount
		};

		const VkImageViewCreateInfo		imageViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType            sType
			DE_NULL,									// const void*                pNext
			0u,											// VkImageViewCreateFlags     flags
			*colorTargetImage,							// VkImage                    image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType            viewType
			colorAttachmentFormat,						// VkFormat                   format
			ComponentMapping(),							// VkComponentMapping         components
			subresourceRange							// VkImageSubresourceRange    subresourceRange
		};

		imageViews.push_back(createImageView(vk, device, &imageViewCreateInfo));
		colorAttachments.push_back(*imageViews.back());

		renderPassCreateInfo.addAttachment(AttachmentDescription(colorAttachmentFormat,
																 VK_SAMPLE_COUNT_1_BIT,
																 VK_ATTACHMENT_LOAD_OP_CLEAR,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));


		const VkAttachmentReference		colorAttachmentReference	=
		{
			i,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
														   0,
														   0,
														   DE_NULL,
														   1u,
														   &colorAttachmentReference,
														   DE_NULL,
														   AttachmentReference(),
														   0,
														   DE_NULL));
	}

	Unique<VkRenderPass>				renderPass					(createRenderPass(vk, device, &renderPassCreateInfo));

	const FramebufferCreateInfo			framebufferCreateInfo		(*renderPass, colorAttachments, WIDTH, HEIGHT, 1);
	Unique<VkFramebuffer>				framebuffer					(createFramebuffer(vk, device, &framebufferCreateInfo));

	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0,									// uint32_t             binding
		(deUint32)sizeof(tcu::Vec4) * 2,	// uint32_t             stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// VkVertexInputRate    inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{

		{
			0u,								// uint32_t    location
			0u,								// uint32_t    binding
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat    format
			0u								// uint32_t    offset
		},
		{
			1u,								// uint32_t    location
			0u,								// uint32_t    binding
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat    format
			(deUint32)(sizeof(float)* 4),	// uint32_t    offset
		}
	};

	PipelineCreateInfo::VertexInputState	vertexInputState		= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription, 2, vertexInputAttributeDescriptions);
	const VkDeviceSize						dataSize				= m_data.m_vertices.size() * sizeof(PositionColorVertex);
	de::SharedPtr<Buffer>					vertexBuffer			= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize,
																	  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
	deUint8*								ptr						= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());

	deMemcpy(ptr, &(m_data.m_vertices[0]), static_cast<size_t>(dataSize));
	flushAlloc(vk, device, vertexBuffer->getBoundMemory());

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkShaderModule>			vs						(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>			fs						(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));
	VkViewport								viewport				= makeViewport(WIDTH, HEIGHT);
	VkRect2D								scissor					= makeRect2D(WIDTH, HEIGHT);
	vector<Move<VkPipeline>>				pipelines;

	PipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *renderPass, 0, 0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	PipelineCreateInfo::ColorBlendState::Attachment	attachment;
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1u, &attachment));
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, vector<VkViewport>(1, viewport), vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

	for (deUint32 i = 0; i < m_data.m_numLayers; i++)
	{
		pipelineCreateInfo.subpass = i;
		pipelines.push_back(createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo));
	}

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	const VkImageMemoryBarrier				initialTransition		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType	sType
		DE_NULL,									// const void*		pNext
		0u,											// VkAccessFlags	srcAccessMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags	dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout	oldLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout	newLayout
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			destQueueFamilyIndex
		*colorTargetImage,							// VkImage			image
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, m_data.m_numLayers)	// VkImageSubresourceRange	subresourceRange
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL,
						  0, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1, &initialTransition);

	const VkRect2D							renderArea				= makeRect2D(WIDTH, HEIGHT);

	vector<VkClearValue>					clearColors				(m_data.m_numLayers, makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));

	const VkRenderPassBeginInfo				renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType
		DE_NULL,									// const void*             pNext
		*renderPass,								// VkRenderPass            renderPass
		*framebuffer,								// VkFramebuffer           framebuffer
		renderArea,									// VkRect2D                renderArea
		(deUint32)clearColors.size(),				// deUint32                clearValueCount
		clearColors.data(),							// const VkClearValue*     pClearValues
	};

	vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	const VkDeviceSize						vertexBufferOffset		= 0;
	const VkBuffer							vertexBufferObj			= vertexBuffer->object();

	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBufferObj, &vertexBufferOffset);
	for (deUint32 i = 0; i < m_data.m_numLayers; i++)
	{
		if (i != 0)
			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines[i]);
		vk.cmdDraw(*cmdBuffer, 9u, 1u, i * 9u, 0u);
	}

	endRenderPass(vk, *cmdBuffer);

	const VkImageMemoryBarrier				imageBarrier			=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex
		*colorTargetImage,							// VkImage					image
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, m_data.m_numLayers)	// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
						  0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	for (deUint32 i = 0; i < m_data.m_numLayers; i++)
	{
		const VkImageSubresourceLayers	subresource	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
			0u,							// deUint32				mipLevel
			i,							// deUint32				baseArrayLayer
			1u							// deUint32				layerCount
		};

		const VkBufferImageCopy			region		=
		{
			0ull,							// VkDeviceSize					bufferOffset
			0u,								// deUint32						bufferRowLength
			0u,								// deUint32						bufferImageHeight
			subresource,					// VkImageSubresourceLayers		imageSubresource
			makeOffset3D(0, 0, 0),			// VkOffset3D					imageOffset
			makeExtent3D(WIDTH, HEIGHT, 1u)	// VkExtent3D					imageExtent
		};

		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorTargetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *resultBuffers[i], 1u, &region);

		const VkBufferMemoryBarrier	bufferBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType
			DE_NULL,									// const void*		pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex
			*resultBuffers[i],							// VkBuffer			buffer
			0ull,										// VkDeviceSize		offset
			VK_WHOLE_SIZE								// VkDeviceSize		size
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
							  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
	}

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	qpTestResult res = QP_TEST_RESULT_PASS;

	for (deUint32 i = 0; i < m_data.m_numLayers; i++)
	{
		invalidateMappedMemoryRange(vk, m_context.getDevice(), resultBufferAllocations[i]->getMemory(),
									resultBufferAllocations[i]->getOffset(), VK_WHOLE_SIZE);

		tcu::TextureLevel					refImage(mapVkFormat(colorAttachmentFormat), WIDTH, HEIGHT);
		tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

		vector<tcu::Vec4>					vertices;
		vector<tcu::Vec4>					colors;

		for (int v = 0; v < 9; v++)
		{
			int idx = i * 9 + v;
			vertices.push_back(m_data.m_vertices[idx].position);
			colors.push_back(m_data.m_vertices[idx].color);
		}

		generateRefImage(refImage.getAccess(), vertices, colors);

		const tcu::TextureFormat			format			(mapVkFormat(colorAttachmentFormat));
		const void *const					ptrResult		(resultBufferAllocations[i]->getHostPtr());
		const tcu::ConstPixelBufferAccess	renderedFrame	(format, WIDTH, HEIGHT, 1, ptrResult);

		if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", refImage.getAccess(), renderedFrame, 0.053f, tcu::COMPARE_LOG_RESULT))
			res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void createAhbDrawTests (tcu::TestCaseGroup* testGroup)
{
	testGroup->addChild(new AhbTestCase(testGroup->getTestContext(), "triangle_list", "Draw triangle list to a single layer color buffer",
									 DrawParams(9u, 1u)));

	testGroup->addChild(new AhbTestCase(testGroup->getTestContext(), "triangle_list_layers_3", "Draw triangle list to a color buffer with three layers",
										DrawParams(9u * 3u, 3u)));

	testGroup->addChild(new AhbTestCase(testGroup->getTestContext(), "triangle_list_layers_5", "Draw triangle list to a color buffer with five layers",
										DrawParams(9u * 5u, 5u)));

	testGroup->addChild(new AhbTestCase(testGroup->getTestContext(), "triangle_list_layers_8", "Draw triangle list to a color buffer with eight layers",
										DrawParams(9u * 8u, 8u)));

}

}	// anonymous

tcu::TestCaseGroup*	createAhbTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "ahb", "Draw tests using Android Hardware Buffer", createAhbDrawTests);
}

}	// DrawTests
}	// vkt
