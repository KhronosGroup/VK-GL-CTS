/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
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
 * \brief Test no-op image layout transitions in VK_KHR_synchronization2
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "tcuTestLog.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace synchronization
{
namespace
{

using tcu::Vec4;
using std::vector;
using de::MovePtr;
using tcu::TextureLevel;

const int			WIDTH	= 64;
const int			HEIGHT	= 64;
const VkFormat		FORMAT	= VK_FORMAT_R8G8B8A8_UNORM;

inline VkImageCreateInfo makeImageCreateInfo ()
{
	const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
										  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const VkImageCreateInfo	imageParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType         sType;
		DE_NULL,								//  const void*             pNext;
		0,										//  VkImageCreateFlags      flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType             imageType;
		FORMAT,									//  VkFormat                format;
		makeExtent3D(WIDTH, HEIGHT, 1u),		//  VkExtent3D              extent;
		1u,										//  deUint32                mipLevels;
		1u,										//  deUint32                arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits   samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling           tiling;
		usage,									//  VkImageUsageFlags       usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode           sharingMode;
		0u,										//  deUint32                queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*         pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout           initialLayout;
	};

	return imageParams;
}

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

class SynchronizationImageLayoutTransitionTestInstance : public TestInstance
{
public:
					SynchronizationImageLayoutTransitionTestInstance	(Context&			context);
	tcu::TestStatus	iterate												(void);
};

SynchronizationImageLayoutTransitionTestInstance::SynchronizationImageLayoutTransitionTestInstance (Context& context)
	: TestInstance	(context)
{
}

template<typename T>
inline size_t sizeInBytes (const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

// Draw a quad covering the whole framebuffer
vector<Vec4> genFullQuadVertices (void)
{
	vector<Vec4> vertices;
	vertices.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
	vertices.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));

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

tcu::TestStatus SynchronizationImageLayoutTransitionTestInstance::iterate (void)
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

	const VkImageCreateInfo						targetCreateInfo		= makeImageCreateInfo();
	const VkImageSubresourceRange				targetSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1, 0, 1);
	const ImageWithMemory						targetImage				(vk, device, m_context.getDefaultAllocator(), targetCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>							targetImageView			= makeImageView(vk, device, *targetImage, VK_IMAGE_VIEW_TYPE_2D, FORMAT, targetSubresourceRange);

	const Move<VkCommandPool>					cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>					cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Move<VkRenderPass>							renderPass				= makeRenderPass(vk, device, FORMAT, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
	Move<VkFramebuffer>							framebuffer				= makeFramebuffer(vk, device, *renderPass, targetImageView.get(), renderSize.width, renderSize.height);

	const Move<VkShaderModule>					vertexModule			= createShaderModule (vk, device, m_context.getBinaryCollection().get("vert1"), 0u);
	const Move<VkShaderModule>					fragmentModule			= createShaderModule (vk, device, m_context.getBinaryCollection().get("frag1"), 0u);

	const Move<VkPipelineLayout>				pipelineLayout			= makePipelineLayout (vk, device, DE_NULL);

	const VkPipelineColorBlendAttachmentState	clrBlendAttachmentState	=
	{
		VK_TRUE,								// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_SRC_ALPHA,				// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,						// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_MAX,						// VkBlendOp                alphaBlendOp;
		(VkColorComponentFlags)0xF				// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo	clrBlendStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType;
		DE_NULL,													// const void*                                   pNext;
		(VkPipelineColorBlendStateCreateFlags)0u,					// VkPipelineColorBlendStateCreateFlags          flags;
		VK_FALSE,													// VkBool32                                      logicOpEnable;
		VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp;
		1u,															// deUint32                                      attachmentCount;
		&clrBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }									// float                                         blendConstants[4];
	};

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

	const Move<VkPipeline>						graphicsPipeline		= makeGraphicsPipeline(vk, device, pipelineLayout.get(), vertexModule.get(), DE_NULL, DE_NULL,
																							   DE_NULL, fragmentModule.get(), renderPass.get(),
																							   viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
																							   0u, 0u, &vtxInputStateCreateInfo, DE_NULL,
																							   DE_NULL, DE_NULL, &clrBlendStateCreateInfo);

	const VkBufferCreateInfo					resultBufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Move<VkBuffer>								resultBuffer			= createBuffer(vk, device, &resultBufferCreateInfo);
	MovePtr<Allocation>							resultBufferMemory		= allocator.allocate(getBufferMemoryRequirements(vk, device, *resultBuffer), MemoryRequirement::HostVisible);
	MovePtr<TextureLevel>						resultImage				(new TextureLevel(mapVkFormat(FORMAT), renderSize.width, renderSize.height, 1));

	VK_CHECK(vk.bindBufferMemory(device, *resultBuffer, resultBufferMemory->getMemory(), resultBufferMemory->getOffset()));

	const Vec4									clearColor				(0.0f, 0.0f, 0.0f, 0.0f);

	clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
					targetImage.get(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1);

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), 0, DE_NULL);
	vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	// Define an execution dependency and skip the layout transition. This is allowed when oldLayout
	// and newLayout are both UNDEFINED. The test will fail if the driver discards the contents of
	// the image.
	const VkImageMemoryBarrier2KHR				imageMemoryBarrier2		= makeImageMemoryBarrier2(
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags2KHR    srcStageMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags2KHR           srcAccessMask
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags2KHR    dstStageMask
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,			// VkAccessFlags2KHR           dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout               oldLayout
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout               newLayout
		targetImage.get(),								// VkImage                     image
		targetSubresourceRange							// VkImageSubresourceRange     subresourceRange
	);
	VkDependencyInfoKHR							dependencyInfo			= makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
	vk.cmdPipelineBarrier2(cmdBuffer.get(), &dependencyInfo);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), 0, DE_NULL);
	vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	// Read the result buffer data
	copyImageToBuffer(vk, *cmdBuffer, *targetImage, *resultBuffer, tcu::IVec2(WIDTH, HEIGHT), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, *resultBufferMemory);

	tcu::clear(resultImage->getAccess(), tcu::IVec4(0));
	tcu::copy(resultImage->getAccess(), tcu::ConstPixelBufferAccess(resultImage.get()->getFormat(),
			  resultImage.get()->getSize(), resultBufferMemory->getHostPtr()));

	TextureLevel								textureLevel			(mapVkFormat(FORMAT), WIDTH, HEIGHT, 1);
	const tcu::PixelBufferAccess				expectedImage			= textureLevel.getAccess();

	const float									alpha					= 0.4f;
	const float									red						= (2.0f - alpha) * alpha;
	const float									green					= red;
	const float									blue					= 0;
	const Vec4									color					= Vec4(red, green, blue, alpha);

	for (int y = 0; y < HEIGHT; y++)
		for (int x = 0; x < WIDTH; x++)
			expectedImage.setPixel(color, x, y, 0);

	bool ok = tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Image comparison", "", expectedImage, resultImage->getAccess(), tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT);
	return ok ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

class SynchronizationImageLayoutTransitionTest : public TestCase
{
public:
						SynchronizationImageLayoutTransitionTest	(tcu::TestContext&	testCtx,
																	 const std::string&	name,
																	 const std::string&	description);

	virtual void		checkSupport								(Context&			context) const;
	void				initPrograms								(SourceCollections& programCollection) const;
	TestInstance*		createInstance								(Context&			context) const;
};

SynchronizationImageLayoutTransitionTest::SynchronizationImageLayoutTransitionTest (tcu::TestContext&	testCtx,
																					const std::string&	name,
																					const std::string&	description)
	: TestCase	(testCtx, name, description)
{
}

void SynchronizationImageLayoutTransitionTest::initPrograms (SourceCollections& programCollection) const
{
	std::ostringstream vertexSrc;
	vertexSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) in vec4 a_position;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = a_position;\n"
		<< "}\n";

	std::ostringstream fragmentSrc;
	fragmentSrc
		<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1., 1., 0., .4);\n"
		<< "}\n";

	programCollection.glslSources.add("vert1") << glu::VertexSource(vertexSrc.str());
	programCollection.glslSources.add("frag1") << glu::FragmentSource(fragmentSrc.str());
}

void SynchronizationImageLayoutTransitionTest::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

TestInstance* SynchronizationImageLayoutTransitionTest::createInstance (Context& context) const
{
	return new SynchronizationImageLayoutTransitionTestInstance(context);
}

} // anonymous ns

tcu::TestCaseGroup* createImageLayoutTransitionTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "layout_transition", "No-op image layout transition tests"));
	testGroup->addChild(new SynchronizationImageLayoutTransitionTest(testCtx, "no_op", ""));

	return testGroup.release();
}

} // synchronization
} // vkt
