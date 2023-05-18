/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google LLC.
 *
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
 * \brief Verify Depth/Stencil Write conditions
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"

#include "../pipeline/vktPipelineImageUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktTestCaseUtil.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace renderpass
{
namespace
{

using tcu::Vec4;
using std::vector;
using de::MovePtr;
using tcu::TextureLevel;

const int	WIDTH	= 64;
const int	HEIGHT	= 64;

enum DiscardType
{
	KILL = 0,
	TERMINATE,
	DEMOTE
};

enum BufferType
{
	DEPTH = 0,
	STENCIL
};

enum MutationMode
{
	WRITE = 0,
	INITIALIZE,
	INITIALIZE_WRITE
};

Move<VkBuffer> makeVertexBuffer (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkBufferCreateFlags  flags;
		1024u,									// VkDeviceSize         size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,		// VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode        sharingMode;
		1u,										// deUint32             queueFamilyIndexCount;
		&queueFamilyIndex						// const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer>			vertexBuffer		= createBuffer(vk, device, &vertexBufferParams);;
	return vertexBuffer;
}

class DepthStencilWriteConditionsInstance : public TestInstance
{
public:
					DepthStencilWriteConditionsInstance	(Context& context, const BufferType& bufferType, const VkFormat& m_bufferFormat);
	tcu::TestStatus	iterate								(void);
private:
	BufferType	m_bufferType;
	VkFormat	m_bufferFormat;
};

DepthStencilWriteConditionsInstance::DepthStencilWriteConditionsInstance (Context& context, const BufferType& bufferType, const VkFormat& bufferFormat)
	: TestInstance	(context), m_bufferType(bufferType), m_bufferFormat(bufferFormat)
{
}

template<typename T>
inline size_t sizeInBytes (const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

// A quad covering the whole framebuffer
vector<Vec4> genFullQuadVertices (void)
{
	vector<Vec4> vertices;
	vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 1.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f,  1.0f, 1.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 1.0f, 1.0f));

	return vertices;
}

struct Vertex
{
	Vertex(Vec4 vertices_) : vertices(vertices_) {}
	Vec4 vertices;

	static VkVertexInputBindingDescription				getBindingDescription		(void);
	static vector<VkVertexInputAttributeDescription>	getAttributeDescriptions	(void);
};

VkVertexInputBindingDescription Vertex::getBindingDescription (void)
{
	static const VkVertexInputBindingDescription desc =
	{
		0u,										// deUint32             binding;
		static_cast<deUint32>(sizeof(Vertex)),	// deUint32             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	return desc;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions (void)
{
	static const vector<VkVertexInputAttributeDescription> desc =
	{
		{
			0u,													// deUint32    location;
			0u,													// deUint32    binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,						// VkFormat    format;
			static_cast<deUint32>(offsetof(Vertex, vertices)),	// deUint32    offset;
		},
	};

	return desc;
}

tcu::TestStatus DepthStencilWriteConditionsInstance::iterate (void)
{
	const DeviceInterface&						vk						= m_context.getDeviceInterface();
	const VkDevice								device					= m_context.getDevice();
	Allocator&									allocator				= m_context.getDefaultAllocator();
	const VkQueue								queue					= m_context.getUniversalQueue();
	const deUint32								queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize							bufferSize				= 16 * 1024;

	const VkExtent2D							renderSize				= {deUint32(WIDTH), deUint32(HEIGHT)};
	const VkRect2D								renderArea				= makeRect2D(makeExtent3D(WIDTH, HEIGHT, 1u));
	const vector<VkRect2D>						scissors				(1u, renderArea);
	const vector<VkViewport>					viewports				(1u, makeViewport(makeExtent3D(WIDTH, HEIGHT, 1u)));

	const vector<Vec4>							vertices				= genFullQuadVertices();
	Move<VkBuffer>								vertexBuffer			= makeVertexBuffer(vk, device, queueFamilyIndex);
	MovePtr<Allocation>							vertexBufferAlloc		= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
	const VkDeviceSize							vertexBufferOffset		= 0ull;

	deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], sizeInBytes(vertices));
	flushAlloc(vk, device, *vertexBufferAlloc);

	const VkImageUsageFlags						colorImageUsage			=   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo						colorImageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType         sType;
		DE_NULL,								//  const void*             pNext;
		0,										//  VkImageCreateFlags      flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType             imageType;
		VK_FORMAT_R8G8B8A8_UNORM,				//  VkFormat                format;
		makeExtent3D(WIDTH, HEIGHT, 1u),		//  VkExtent3D              extent;
		1u,										//  deUint32                mipLevels;
		1u,										//  deUint32                arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits   samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling           tiling;
		colorImageUsage,						//  VkImageUsageFlags       usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode           sharingMode;
		0u,										//  deUint32                queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*         pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout           initialLayout;
	};
	const VkImageSubresourceRange				colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
	const ImageWithMemory						colorImage				(vk, device, m_context.getDefaultAllocator(), colorImageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>							colorImageView			= makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
																						colorSubresourceRange);

	// Depending on the type of the buffer, create a depth buffer or a stencil buffer.
	const VkImageUsageFlags						depthStencilUsage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	const VkImageCreateInfo						depthStencilBufferInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
		nullptr,								// const void*              pNext;
		0u,										// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType;
		m_bufferFormat,							// VkFormat                 format;
		makeExtent3D(WIDTH, HEIGHT, 1u),		// VkExtent3D               extent;
		1u,										// deUint32                 mipLevels;
		1u,										// deUint32                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling            tiling;
		depthStencilUsage,						// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
		0u,										// deUint32                 queueFamilyIndexCount;
		nullptr,								// const deUint32*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout            initialLayout;
	};

	const de::SharedPtr<Draw::Image>			depthStencilImage		= Draw::Image::createAndAlloc(vk, device, depthStencilBufferInfo, m_context.getDefaultAllocator(),
																									  m_context.getUniversalQueueFamilyIndex(), MemoryRequirement::Any);
	const VkImageAspectFlagBits					imageAspectFlagBits		= m_bufferType == BufferType::DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageSubresourceRange				subresourceRange		= makeImageSubresourceRange(imageAspectFlagBits, 0u, 1u, 0u, 1u);
	Move<VkImageView>							depthStencilImageView	= makeImageView(vk, device, depthStencilImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_bufferFormat, subresourceRange);

	const Move<VkCommandPool>					cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>					cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const auto									renderPass				= makeRenderPass(vk, device,VK_FORMAT_R8G8B8A8_UNORM, m_bufferFormat, VK_ATTACHMENT_LOAD_OP_CLEAR,
																						 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	const vector<VkImageView>					attachments				= {colorImageView.get(), depthStencilImageView.get()};
	const auto									framebuffer				= makeFramebuffer(vk, device, renderPass.get(), static_cast<deUint32>(attachments.size()),
																						  de::dataOrNull(attachments), renderSize.width, renderSize.height);

	const Move<VkShaderModule>					vertexModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	const Move<VkShaderModule>					fragmentModule			= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

	const Move<VkPipelineLayout>				pipelineLayout			= makePipelineLayout(vk, device, DE_NULL);

	const VkVertexInputBindingDescription		vtxBindingDescription	= Vertex::getBindingDescription();
	const auto									vtxAttrDescriptions		= Vertex::getAttributeDescriptions();

	const VkPipelineVertexInputStateCreateInfo	vtxInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		1u,															// deUint32                                    vertexBindingDescriptionCount;
		&vtxBindingDescription,										// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		static_cast<deUint32>(vtxAttrDescriptions.size()),			// deUint32                                    vertexAttributeDescriptionCount
		vtxAttrDescriptions.data(),									// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	// The value in the stencil buffer is replaced if the new value is greater than the previous value.
	const VkStencilOpState						stencilOp				= makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
																							 VK_COMPARE_OP_GREATER, 0xffu, 0xffu, 0u);

	const VkPipelineDepthStencilStateCreateInfo	depthStencilCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		nullptr,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		m_bufferType == BufferType::DEPTH ? VK_TRUE : VK_FALSE,		// VkBool32                                 depthTestEnable
		VK_TRUE,													// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_GREATER,										// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		m_bufferType == BufferType::STENCIL ? VK_TRUE : VK_FALSE,	// VkBool32                                 stencilTestEnable
		stencilOp,													// VkStencilOpState                         front
		stencilOp,													// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f,														// float                                    maxDepthBounds
	};

	const Move<VkPipeline>						graphicsPipeline		= makeGraphicsPipeline(vk, device, pipelineLayout.get(), vertexModule.get(),
																							   DE_NULL, DE_NULL, DE_NULL, fragmentModule.get(), renderPass.get(),
																							   viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
																							   0u, 0u, &vtxInputStateCreateInfo, DE_NULL, DE_NULL,
																							   &depthStencilCreateInfo, DE_NULL, DE_NULL);

	const VkBufferCreateInfo					resultBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Move<VkBuffer>								resultBuffer			= createBuffer(vk, device, &resultBufferCreateInfo);
	MovePtr<Allocation>							resultBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer),
																							 MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

	const vector<VkClearValue>					clearColors				=
	{
		makeClearValueColorF32(0.0f, 0.0f, 0.0f, 0.0f),
		makeClearValueDepthStencil(.1f, 0u),
	};

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

	beginRenderPass(vk, *cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(0, 0, WIDTH, HEIGHT), static_cast<deUint32>(clearColors.size()),
					de::dataOrNull(clearColors), VK_SUBPASS_CONTENTS_INLINE, DE_NULL);
	vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, *resultBufferMemory);

	de::MovePtr<tcu::TextureLevel>				attachment;

	if (m_bufferType == BufferType::DEPTH)
		attachment = pipeline::readDepthAttachment(vk, device, queue, queueFamilyIndex, allocator, depthStencilImage->object(),
												   m_bufferFormat, tcu::UVec2(WIDTH, HEIGHT), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	else
		attachment = pipeline::readStencilAttachment(vk, device, queue, queueFamilyIndex, allocator, depthStencilImage->object(),
													 m_bufferFormat, tcu::UVec2(WIDTH, HEIGHT), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	bool										result					= true;
	for (deUint32 y = 0; y < HEIGHT; y++)
	{
		for (deUint32 x = 0; x < WIDTH; x++)
		{
			if (m_bufferType == BufferType::STENCIL)
			{
				const auto stencilPixel = attachment->getAccess().getPixStencil(x, y, 0);
				if (static_cast<deUint32>(stencilPixel) != x % 2)
					result = false;
			}
			else
			{
				const auto depthPixel = attachment->getAccess().getPixDepth(x, y);
				if ((depthPixel < 0.09 || depthPixel > 0.11) && x % 2 == 0)
					result = false;
				if ((depthPixel < 0.19 || depthPixel > 0.21) && x % 2 == 1)
					result = false;
			}
		}
	}

	return result ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

class DepthStencilWriteConditionsTest : public TestCase
{
public:
						DepthStencilWriteConditionsTest (tcu::TestContext&	testCtx,
														 const std::string&	name,
														 const std::string&	description,
														 const BufferType	bufferType,
														 const DiscardType	discardType,
														 const MutationMode	mutationMode,
														 const VkFormat		bufferFormat);

	virtual void		checkSupport					 (Context&				context) const;
	void				initPrograms					 (SourceCollections&	programCollection) const;
	TestInstance*		createInstance					 (Context&				context) const;
private:
	BufferType		m_bufferType;
	DiscardType		m_discardType;
	MutationMode	m_mutationMode;
	VkFormat		m_bufferFormat;
};

DepthStencilWriteConditionsTest::DepthStencilWriteConditionsTest (tcu::TestContext&		testCtx,
																  const std::string&	name,
																  const std::string&	description,
																  const BufferType		bufferType,
																  const DiscardType		discardType,
																  const MutationMode	mutationMode,
																  const VkFormat		bufferFormat)
	: TestCase	(testCtx, name, description)
	, m_bufferType(bufferType)
	, m_discardType(discardType)
	, m_mutationMode(mutationMode)
	, m_bufferFormat(bufferFormat)
{
}

void DepthStencilWriteConditionsTest::initPrograms (SourceCollections& programCollection) const
{
	/*
	 * The fragment shader has been compiled from the following GLSL shader:
	 *
	 * layout(location = 0) out vec4 outColor;
	 * void main() {
	 *     if (int(gl_FragCoord.x) % 2 == 0)
	 *         discard;
	 *     outColor = vec4(1., 1., 1., 1.);
	 *     gl_FragDepth = 0.2;
	 * }
	 *
	 * If a stencil buffer is enabled, the shader writes to gl_FragStencilRefARB
	 * instead of gl_FragDepth.
	 *
	 * If the mutation mode is INITIALIZE or INITIALIZE_WRITE, the object that
	 * is written to the buffer is allocated with an initial value.
	 *
	 * Demote and terminate commands are used instead of discard if a corresponding
	 * DiscardType has been given.
	 */

	std::ostringstream	vertexSrc;
	vertexSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) in highp vec4 a_position;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = a_position;\n"
		<< "}\n";

	std::string			discardCommand	= "OpKill\n";
	std::string			extensions		= "";
	std::string			capabilities	= "";

	if (m_discardType == DiscardType::TERMINATE)
	{
		extensions = "OpExtension \"SPV_KHR_terminate_invocation\"\n";
		discardCommand = "OpTerminateInvocation\n";
	}
	else if (m_discardType == DiscardType::DEMOTE)
	{
		capabilities = "OpCapability DemoteToHelperInvocationEXT\n";
		extensions = "OpExtension \"SPV_EXT_demote_to_helper_invocation\"\n";
		discardCommand = "OpDemoteToHelperInvocationEXT\n";
	}

	if (m_bufferType == BufferType::STENCIL)
	{
		capabilities += "OpCapability StencilExportEXT\n";
		extensions += "OpExtension \"SPV_EXT_shader_stencil_export\"\n";
	}

	std::ostringstream	fragmentSrc;
	fragmentSrc
		<< "OpCapability Shader\n"
		<< capabilities
		<< extensions
		<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
		<< "OpMemoryModel Logical GLSL450\n";

	fragmentSrc
		<< "OpEntryPoint Fragment %4 \"main\" %9 %26 %30\n"
		<< "OpExecutionMode %4 OriginUpperLeft\n";

	if (m_bufferType == BufferType::DEPTH)
		fragmentSrc << "OpExecutionMode %4 DepthReplacing\n";

	fragmentSrc
		<< "OpDecorate %9 BuiltIn FragCoord\n"
		<< "OpDecorate %26 Location 0\n";

	if (m_bufferType == BufferType::DEPTH)
		fragmentSrc << "OpDecorate %30 BuiltIn FragDepth\n";
	else
		fragmentSrc << "OpDecorate %30 BuiltIn FragStencilRefEXT\n";

	fragmentSrc
		<< "%2 = OpTypeVoid\n"
		<< "%3 = OpTypeFunction %2\n"
		<< "%6 = OpTypeFloat 32\n"
		<< "%7 = OpTypeVector %6 4\n"
		<< "%8 = OpTypePointer Input %7\n"
		<< "%9 = OpVariable %8 Input\n"
		<< "%10 = OpTypeInt 32 0\n"
		<< "%11 = OpConstant %10 0\n"
		<< "%12 = OpTypePointer Input %6\n"
		<< "%15 = OpTypeInt 32 1\n"
		<< "%17 = OpConstant %15 2\n"
		<< "%19 = OpConstant %15 0\n"
		<< "%20 = OpTypeBool\n"
		<< "%25 = OpTypePointer Output %7\n"
		<< "%26 = OpVariable %25 Output\n"
		<< "%27 = OpConstant %6 1\n"
		<< "%28 = OpConstantComposite %7 %27 %27 %27 %27\n";
	if (m_bufferType == BufferType::DEPTH)
	{
		fragmentSrc << "%29 = OpTypePointer Output %6\n";

		if (m_mutationMode == MutationMode::INITIALIZE || m_mutationMode == MutationMode::INITIALIZE_WRITE)
		{
			// The value the depth buffer is initialized with.
			fragmentSrc << "%const_f32_02 = OpConstant %6 0.2\n";
			fragmentSrc << "%30 = OpVariable %29 Output %const_f32_02\n";
		}
		else
			fragmentSrc << "%30 = OpVariable %29 Output\n";

		// The value written to the depth buffer.
		fragmentSrc << "%31 = OpConstant %6 0.2\n";
	}
	else
	{
		fragmentSrc << "%29 = OpTypePointer Output %15\n";

		if (m_mutationMode == MutationMode::INITIALIZE || m_mutationMode == MutationMode::INITIALIZE_WRITE)
		{
			// The value the stencil buffer is initialized with.
			fragmentSrc << "%const_int_1 = OpConstant %15 1\n";
			fragmentSrc << "%30 = OpVariable %29 Output %const_int_1\n";
		}
		else
			fragmentSrc << "%30 = OpVariable %29 Output\n";

		// The value written to the stencil buffer.
		fragmentSrc << "%31 = OpConstant %15 1\n";
	}

	fragmentSrc
		<< "%4 = OpFunction %2 None %3\n"
		<< "%5 = OpLabel\n"
		<< "%13 = OpAccessChain %12 %9 %11\n"
		<< "%14 = OpLoad %6 %13\n"
		<< "%16 = OpConvertFToS %15 %14\n"
		<< "%18 = OpSMod %15 %16 %17\n"
		<< "%21 = OpIEqual %20 %18 %19\n"
		<< "OpSelectionMerge %23 None\n"
		<< "OpBranchConditional %21 %22 %23\n"
		<< "%22 = OpLabel\n"
		<< discardCommand;
	if (m_discardType == DiscardType::DEMOTE)
		fragmentSrc << "OpBranch %23\n";
	fragmentSrc
		<< "%23 = OpLabel\n"
		<< "OpStore %26 %28\n";

	if (m_mutationMode == MutationMode::WRITE || m_mutationMode == MutationMode::INITIALIZE_WRITE)
		fragmentSrc << "OpStore %30 %31\n";

	fragmentSrc
		<< "OpReturn\n"
		<< "OpFunctionEnd\n";

	programCollection.spirvAsmSources.add("frag") << fragmentSrc.str().c_str();
	programCollection.glslSources.add("vert") << glu::VertexSource(vertexSrc.str());
}

void DepthStencilWriteConditionsTest::checkSupport (Context& context) const
{
	if (m_discardType == DiscardType::DEMOTE)
		context.requireDeviceFunctionality("VK_EXT_shader_demote_to_helper_invocation");
	if (m_discardType == DiscardType::TERMINATE)
		context.requireDeviceFunctionality("VK_KHR_shader_terminate_invocation");
	if (m_bufferType == BufferType::STENCIL)
		context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

	std::string				formatName				= "VK_FORMAT_D32_SFLOAT_S8_UINT";
	if (m_bufferFormat == VK_FORMAT_D24_UNORM_S8_UINT)
		formatName = "VK_FORMAT_D24_UNORM_S8_UINT";
	if (m_bufferFormat == VK_FORMAT_X8_D24_UNORM_PACK32)
		formatName = "VK_FORMAT_X8_D24_UNORM_PACK32";
	if (m_bufferFormat == VK_FORMAT_D32_SFLOAT)
		formatName = "VK_FORMAT_D32_SFLOAT";

	const auto&				vki						= context.getInstanceInterface();
	const auto				physicalDevice			= context.getPhysicalDevice();
	const VkImageUsageFlags	depthStencilUsage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageFormatProperties	imageFormatProperties;
	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_bufferFormat, VK_IMAGE_TYPE_2D,
												   VK_IMAGE_TILING_OPTIMAL, depthStencilUsage, (VkImageCreateFlags)0,
												   &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, formatName + " not supported.");
}

TestInstance* DepthStencilWriteConditionsTest::createInstance (Context& context) const
{
	return new DepthStencilWriteConditionsInstance(context, m_bufferType, m_bufferFormat);
}

} // anonymous ns

tcu::TestCaseGroup* createDepthStencilWriteConditionsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "depth_stencil_write_conditions", "Depth/Stencil Write conditions tests"));

	const VkFormat	depthFormats[4]		= {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT};
	const VkFormat	stencilFormats[2]	= {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

	for (int i = 0; i < 4; i++)
	{
		VkFormat		format	= depthFormats[i];
		std::string		postfix	= "_d32sf_s8ui";
		if (format == VK_FORMAT_D24_UNORM_S8_UINT)
			postfix = "_d24unorm_s8ui";
		if (format == VK_FORMAT_X8_D24_UNORM_PACK32)
			postfix = "_d24_unorm";
		if (format == VK_FORMAT_D32_SFLOAT)
			postfix = "_d32sf";

		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_kill_write" + postfix, "", BufferType::DEPTH, DiscardType::KILL, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_kill_initialize" + postfix, "", BufferType::DEPTH, DiscardType::KILL, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_kill_write_initialize" + postfix, "", BufferType::DEPTH, DiscardType::KILL, MutationMode::INITIALIZE_WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_terminate_write" + postfix, "", BufferType::DEPTH, DiscardType::TERMINATE, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_terminate_initialize" + postfix, "", BufferType::DEPTH, DiscardType::TERMINATE, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_terminate_write_initialize" + postfix, "", BufferType::DEPTH, DiscardType::TERMINATE, MutationMode::INITIALIZE_WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_demote_write" + postfix, "", BufferType::DEPTH, DiscardType::DEMOTE, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_demote_initialize" + postfix, "", BufferType::DEPTH, DiscardType::DEMOTE, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "depth_demote_write_initialize" + postfix, "", BufferType::DEPTH, DiscardType::DEMOTE, MutationMode::INITIALIZE_WRITE, format));
	}

	for (int i = 0; i < 2; i++)
	{
		VkFormat		format	= stencilFormats[i];
		std::string		postfix	= "_d32sf_s8ui";
		if (format == VK_FORMAT_D24_UNORM_S8_UINT)
			postfix = "_d24unorm_s8ui";

		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_kill_write" + postfix, "", BufferType::STENCIL, DiscardType::KILL, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_kill_initialize" + postfix, "", BufferType::STENCIL, DiscardType::KILL, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_kill_write_initialize" + postfix, "", BufferType::STENCIL, DiscardType::KILL, MutationMode::INITIALIZE_WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_terminate_write" + postfix, "", BufferType::STENCIL, DiscardType::TERMINATE, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_terminate_initialize" + postfix, "", BufferType::STENCIL, DiscardType::TERMINATE, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_terminate_write_initialize" + postfix, "", BufferType::STENCIL, DiscardType::TERMINATE, MutationMode::INITIALIZE_WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_demote_write" + postfix, "", BufferType::STENCIL, DiscardType::DEMOTE, MutationMode::WRITE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_demote_initialize" + postfix, "", BufferType::STENCIL, DiscardType::DEMOTE, MutationMode::INITIALIZE, format));
		testGroup->addChild(new DepthStencilWriteConditionsTest(testCtx, "stencil_demote_write_initialize" + postfix, "", BufferType::STENCIL, DiscardType::DEMOTE, MutationMode::INITIALIZE_WRITE, format));
	}

	return testGroup.release();
}

} // renderpass
} // vkt
