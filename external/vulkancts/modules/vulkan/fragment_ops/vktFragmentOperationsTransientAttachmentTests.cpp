/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Google Inc.
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
 * \brief Transient attachment tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsTransientAttachmentTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktFragmentOperationsMakeUtil.hpp"

#include "vkDefs.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace FragmentOperations
{
using namespace vk;
using de::UniquePtr;

namespace
{

enum class TestMode
{
	MODE_INVALID	= 1u << 0,
	MODE_COLOR		= 1u << 1,
	MODE_DEPTH		= 1u << 2,
	MODE_STENCIL	= 1u << 3
};

const char* memoryPropertyFlagBitToString(VkMemoryPropertyFlags flagBit)
{
	switch (flagBit)
	{
	case VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:
		return "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT";

	case VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:
		return "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT";

	case VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
		return "VK_MEMORY_PROPERTY_HOST_COHERENT_BIT";

	case VK_MEMORY_PROPERTY_HOST_CACHED_BIT:
		return "VK_MEMORY_PROPERTY_HOST_CACHED_BIT";

	case VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT:
		return "VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT";

	case VK_MEMORY_PROPERTY_PROTECTED_BIT:
		return "VK_MEMORY_PROPERTY_PROTECTED_BIT";

	case VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD:
		return "VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD";

	case VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD:
		return "VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD";

	case VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV:
		return "VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV";

	default:
		TCU_THROW(InternalError, "Unknown memory property flag bit");
	}
};

VkFormat getSupportedStencilFormat(const VkPhysicalDevice physDevice, const InstanceInterface& instanceInterface)
{
	static const VkFormat stencilFormats[] =
	{
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	for (const auto& sFormat : stencilFormats)
	{
		VkFormatProperties formatProps;
		instanceInterface.getPhysicalDeviceFormatProperties(physDevice, sFormat, &formatProps);

		if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			return sFormat;
		};
	}

	return VK_FORMAT_UNDEFINED;
}

std::vector<deUint32> getMemoryTypeIndices(VkMemoryPropertyFlags propertyFlag, const VkPhysicalDeviceMemoryProperties& pMemoryProperties)
{
	std::vector<deUint32> indices;
	for (deUint32 typeIndex = 0u; typeIndex < pMemoryProperties.memoryTypeCount; ++typeIndex)
	{
		if ((pMemoryProperties.memoryTypes[typeIndex].propertyFlags & propertyFlag) == propertyFlag)
			indices.push_back(typeIndex);
	}
	return indices;
}

VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const tcu::IVec2& size, VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),	// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};
	return imageParams;
}

VkAttachmentDescription makeAttachment (
	 const VkFormat				format,
	 const VkAttachmentLoadOp	loadOp,
	 const VkAttachmentStoreOp	storeOp,
	 const VkImageLayout		initialLayout,
	 const VkImageLayout		finalLayout)
{
	const tcu::TextureFormat		tcuFormat		= mapVkFormat(format);
	const bool						hasStencil		= (tcuFormat.order == tcu::TextureFormat::DS
													|| tcuFormat.order == tcu::TextureFormat::S);

	const VkAttachmentDescription	attachmentDesc	=
	{
		VkAttachmentDescriptionFlags(0),							// VkAttachmentDescriptionFlags	flags;
		format,														// VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		samples;
		loadOp,														// VkAttachmentLoadOp			loadOp;
		storeOp,													// VkAttachmentStoreOp			storeOp;
		hasStencil ? loadOp		: VK_ATTACHMENT_LOAD_OP_DONT_CARE,	// VkAttachmentLoadOp			stencilLoadOp;
		hasStencil ? storeOp	: VK_ATTACHMENT_STORE_OP_DONT_CARE,	// VkAttachmentStoreOp			stencilStoreOp;
		initialLayout,												// VkImageLayout				initialLayout;
		finalLayout													// VkImageLayout				finalLayout;
	};

	return attachmentDesc;
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface& vk, const VkDevice device, const std::vector<VkAttachmentDescription> attachmentDescriptions, const bool hasInputAttachment)
{
	const tcu::TextureFormat					tcuFormat				= mapVkFormat(attachmentDescriptions[0].format);
	const bool									hasDepthStencil			= (tcuFormat.order == tcu::TextureFormat::DS
																		|| tcuFormat.order == tcu::TextureFormat::S
																		|| tcuFormat.order == tcu::TextureFormat::D);

	std::vector< VkAttachmentReference>			testReferences;
	const deUint32								maxAttachmentIndex		= deUint32(attachmentDescriptions.size()) - 1u;

	for (deUint32 ref = 0; ref < attachmentDescriptions.size(); ref++)
	{
		testReferences.push_back({ ref, attachmentDescriptions[ref].finalLayout });
	}

	const VkSubpassDescription					subpassDescription		=
	{
		(VkSubpassDescriptionFlags)0,															// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,														// VkPipelineBindPoint				pipelineBindPoint
		hasInputAttachment ? 1u :0u,															// deUint32							inputAttachmentCount
		hasInputAttachment ? &testReferences[0] : DE_NULL,										// const VkAttachmentReference*		pInputAttachments
		!hasDepthStencil || hasInputAttachment ? 1u : 0u,										// deUint32							colorAttachmentCount
		!hasDepthStencil || hasInputAttachment ? &testReferences[maxAttachmentIndex] : DE_NULL,	// const VkAttachmentReference*		pColorAttachments
		DE_NULL,																				// const VkAttachmentReference*		pResolveAttachments
		hasDepthStencil && !hasInputAttachment ? &testReferences[0] : DE_NULL,					// const VkAttachmentReference*		pDepthStencilAttachment
		0u,																						// deUint32							preserveAttachmentCount
		DE_NULL																					// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo				renderPassInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags
		deUint32(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments
		1u,											// deUint32							subpassCount
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses
		0u,											// deUint32							dependencyCount
		DE_NULL																		// const VkSubpassDependency*		pDependencies
	};

	return createRenderPass(vk, device, &renderPassInfo, DE_NULL);
}

class TransientAttachmentTest : public TestCase
{
public:
						TransientAttachmentTest	(tcu::TestContext&				testCtx,
												 const std::string				name,
												 const TestMode					testMode,
												 const VkMemoryPropertyFlags	flags,
												 const tcu::IVec2				renderSize);

	void				initPrograms			(SourceCollections&				programCollection) const;
	TestInstance*		createInstance			(Context&						context) const;
	virtual void		checkSupport			(Context&						context) const;

private:
	const TestMode					m_testMode;
	const VkMemoryPropertyFlags		m_flags;
	const tcu::IVec2				m_renderSize;
};

TransientAttachmentTest::TransientAttachmentTest (
	tcu::TestContext&			testCtx,
	const std::string			name,
	const TestMode				testMode,
	const VkMemoryPropertyFlags	flags,
	const tcu::IVec2			renderSize)
	: TestCase		(testCtx, name, "")
	, m_testMode	(testMode)
	, m_flags		(flags)
	, m_renderSize	(renderSize)
{
}

void TransientAttachmentTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex shader
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(input_attachment_index = 0, binding = 0) uniform "	<< (m_testMode == TestMode::MODE_STENCIL	? "usubpassInput " : "subpassInput ") << "inputValue;\n"
			<< "\n"
			<< "layout(location = 0) out vec4 fragColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "	fragColor = " << (m_testMode == TestMode::MODE_COLOR					? "subpassLoad(inputValue);\n"
													: m_testMode == TestMode::MODE_DEPTH	? "vec4(subpassLoad(inputValue).r, 0.0, 0.0, 1.0);\n"
													: /*			TestMode::MODE_STENCIL	*/"vec4(0.0, 0.0, float(subpassLoad(inputValue).r) / 256.0, 1.0);\n")
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void TransientAttachmentTest::checkSupport (Context& context) const
{
	const InstanceInterface&				vki					= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	VkImageFormatProperties					formatProperties;
	const VkPhysicalDeviceMemoryProperties	pMemoryProperties	= getPhysicalDeviceMemoryProperties(vki,physicalDevice);
	const std::vector<deUint32>				memoryTypeIndices	= getMemoryTypeIndices(m_flags, pMemoryProperties);

	const VkFormat							testFormat			= m_testMode == TestMode::MODE_DEPTH
																? VK_FORMAT_D16_UNORM
																: m_testMode == TestMode::MODE_STENCIL
																? getSupportedStencilFormat(context.getPhysicalDevice(), context.getInstanceInterface())
																: VK_FORMAT_R8G8B8A8_UNORM;

	if (memoryTypeIndices.empty())
	{
		TCU_THROW(NotSupportedError, std::string(memoryPropertyFlagBitToString(m_flags)) + " is not supported by any memory type");
	}

	vki.getPhysicalDeviceImageFormatProperties	(physicalDevice, testFormat,
												 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
												 (m_testMode == TestMode::MODE_DEPTH || m_testMode == TestMode::MODE_STENCIL)
												? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT	| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
												: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT			| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
												 0u, &formatProperties);

	if (formatProperties.sampleCounts == 0 || testFormat == VK_FORMAT_UNDEFINED)
		TCU_THROW(NotSupportedError, de::toString(testFormat) + " not supported");
}

class TransientAttachmentTestInstance : public TestInstance
{
public:
					TransientAttachmentTestInstance	(Context&						context,
													 const TestMode					testMode,
													 const VkMemoryPropertyFlags	flags,
													 const tcu::IVec2				renderSize);

	tcu::TestStatus	iterate							(void);

private:
	const TestMode				m_testMode;
	const tcu::IVec2			m_renderSize;
	const VkImageAspectFlags	m_aspectFlags;
	const VkImageUsageFlags		m_usageFlags;
	const VkFormat				m_testFormat;
	const vk::MemoryRequirement	m_memReq;
};

TransientAttachmentTestInstance::TransientAttachmentTestInstance (Context& context, const TestMode testMode, const VkMemoryPropertyFlags flags, const tcu::IVec2 renderSize)
	: TestInstance	(context)
	, m_testMode	(testMode)
	, m_renderSize	(renderSize)
	, m_aspectFlags	(	testMode	==	TestMode::MODE_DEPTH			?	VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
					:	testMode	==	TestMode::MODE_STENCIL			?	VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT
					:														VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT)
	, m_usageFlags	(	testMode	==	TestMode::MODE_COLOR			?	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT			| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
					:														VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT	| VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
	, m_testFormat	(	testMode	==	TestMode::MODE_DEPTH			?	VK_FORMAT_D16_UNORM
					:	testMode	==	TestMode::MODE_STENCIL			?	getSupportedStencilFormat(m_context.getPhysicalDevice(), m_context.getInstanceInterface())
					:														VK_FORMAT_R8G8B8A8_UNORM)
	, m_memReq		(flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
																		?	MemoryRequirement::LazilyAllocated
																		:	MemoryRequirement::Local)
{
	DE_ASSERT(m_testMode != TestMode::MODE_INVALID);
}

tcu::TestStatus	TransientAttachmentTestInstance::iterate (void)
{
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkDevice						device						= m_context.getDevice();
	const VkQueue						queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	Allocator&							allocator					= m_context.getDefaultAllocator();
	const VkImageSubresourceRange		testSubresourceRange		= makeImageSubresourceRange(m_aspectFlags, 0u, 1u, 0u, 1u);
	const VkFormat						outputFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageAspectFlags			outputAspectFlags			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageUsageFlags				outputUsageFlags			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Test attachment
	const Unique<VkImage>				inputImage					(makeImage(vk, device, makeImageCreateInfo(m_testFormat, m_renderSize, m_usageFlags)));
	const UniquePtr<Allocation>			inputImageAllocator			(bindImage(vk, device, allocator, *inputImage, m_memReq));
	const Unique<VkImageView>			inputImageView				(makeImageView(vk, device, *inputImage, VK_IMAGE_VIEW_TYPE_2D, m_testFormat, testSubresourceRange));
	const VkImageView					firstAttachmentImages[]		= { *inputImageView };

	const VkImageSubresourceRange		outputSubresourceRange		= makeImageSubresourceRange(outputAspectFlags, 0u, 1u, 0u, 1u);
	const Unique<VkImage>				outputImage					(makeImage(vk, device, makeImageCreateInfo(outputFormat, m_renderSize, outputUsageFlags)));
	const UniquePtr<Allocation>			outputImageAllocator		(bindImage(vk, device, allocator, *outputImage, MemoryRequirement::Local));
	const Unique<VkImageView>			outputImageView				(makeImageView(vk, device, *outputImage, VK_IMAGE_VIEW_TYPE_2D, outputFormat, outputSubresourceRange));
	const VkImageView					secondAttachmentImages[]	= { *inputImageView, *outputImageView };

	const VkDeviceSize					resultBufferSizeBytes		= tcu::getPixelSize(mapVkFormat(outputFormat)) * m_renderSize.x() * m_renderSize.y();
	const Unique<VkBuffer>				resultBuffer				(makeBuffer(vk, device, resultBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			resultBufferAlloc			(bindBuffer(vk, device, allocator, *resultBuffer, MemoryRequirement::HostVisible));

	// Main vertex buffer
	const deUint32						numVertices					= 6;
	const VkDeviceSize					vertexBufferSizeBytes		= 256;
	const Unique<VkBuffer>				vertexBuffer				(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertexBufferAlloc			(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0]	= tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[1]	= tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f);
		pVertices[2]	= tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f);
		pVertices[3]	= tcu::Vec4(-1.0f,  1.0f, 1.0f, 1.0f);
		pVertices[4]	= tcu::Vec4( 1.0f,  1.0f, 1.0f, 1.0f);
		pVertices[5]	= tcu::Vec4( 1.0f, -1.0f, 1.0f, 1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// Shader modules
	const Unique<VkShaderModule>		vertexModule				(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		fragmentModule				(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

	// Descriptor pool and descriptor set
	DescriptorPoolBuilder				poolBuilder;
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u);
	}

	const auto							descriptorPool				= poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	DescriptorSetLayoutBuilder			layoutBuilderAttachments;
	{
		layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	const auto							inputAttachmentsSetLayout	= layoutBuilderAttachments.build(vk, device);
	const auto							descriptorSetAttachments	= makeDescriptorSet(vk, device, descriptorPool.get(), inputAttachmentsSetLayout.get());
	const std::vector<VkDescriptorSet>	descriptorSets				= { descriptorSetAttachments.get() };

	const VkDescriptorImageInfo			imageInfo					=
	{
		DE_NULL,									// VkSampler		sampler;
		*inputImageView,							// VkImageView		imageView;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
	};

	DescriptorSetUpdateBuilder			updater;
	{
		updater.writeSingle(descriptorSetAttachments.get(), DescriptorSetUpdateBuilder::Location::binding(static_cast<deUint32>(0)), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfo);
		updater.update(vk, device);
	}

	const tcu::TextureFormat			tcuFormat					= mapVkFormat(m_testFormat);
	VkImageLayout						inputLayout					= tcuFormat.order == tcu::TextureFormat::DS
																	? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
																	: tcuFormat.order == tcu::TextureFormat::D
																	? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
																	: tcuFormat.order == tcu::TextureFormat::S
																	? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
																	: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Renderpasses
	VkAttachmentDescription				clearPassAttachment			= makeAttachment(m_testFormat, VK_ATTACHMENT_LOAD_OP_CLEAR,		VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, inputLayout);
	VkAttachmentDescription				inputPassAttachment			= makeAttachment(m_testFormat, VK_ATTACHMENT_LOAD_OP_LOAD,		VK_ATTACHMENT_STORE_OP_STORE, inputLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkAttachmentDescription				outputPassAttachment		= makeAttachment(outputFormat, VK_ATTACHMENT_LOAD_OP_DONT_CARE,	VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const Unique<VkRenderPass>			renderPassOne				(makeRenderPass(vk, device, { clearPassAttachment }, false));
	const Unique<VkRenderPass>			renderPassTwo				(makeRenderPass(vk, device, { inputPassAttachment, outputPassAttachment }, true));

	const Unique<VkFramebuffer>			framebufferOne				(makeFramebuffer(vk, device, *renderPassOne, 1, firstAttachmentImages, m_renderSize.x(), m_renderSize.y()));
	const Unique<VkFramebuffer>			framebufferTwo				(makeFramebuffer(vk, device, *renderPassTwo, 2, secondAttachmentImages, m_renderSize.x(), m_renderSize.y()));

	// Pipeline
	const std::vector<VkViewport>		viewports					(1, makeViewport(m_renderSize));
	const std::vector<VkRect2D>			scissors					(1, makeRect2D(m_renderSize));
	const Unique<VkPipelineLayout>		pipelineLayout				(makePipelineLayout(vk, device, *inputAttachmentsSetLayout));
	const Unique<VkPipeline>			pipeline					(vk::makeGraphicsPipeline(vk,									// const DeviceInterface&						vk
																							  device,								// const VkDevice								device
																							  *pipelineLayout,						// const VkPipelineLayout						pipelineLayout
																							  *vertexModule,						// const VkShaderModule							vertexShaderModule
																							  DE_NULL,								// const VkShaderModule							essellationControlModule
																							  DE_NULL,								// const VkShaderModule							tessellationEvalModule
																							  DE_NULL,								// const VkShaderModule							geometryShaderModule
																							  *fragmentModule,						// const VkShaderModule							fragmentShaderModule
																							  *renderPassTwo,						// const VkRenderPass							renderPass
																							  viewports,							// const std::vector<VkViewport>&				viewports
																							  scissors,								// const std::vector<VkRect2D>&					scissors
																							  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology					topology
																							  0u,									// const deUint32								subpass
																							  0u));									// const VkPipelineDepthStencilStateCreateInfo*	depthStencilStateCreateInfo

	// Command buffer
	const Unique<VkCommandPool>			cmdPool						(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer					(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	{
		const VkDeviceSize	vertexBufferOffset	= 0ull;
		VkClearValue		clearValue;

		if		(m_testMode == TestMode::MODE_COLOR)	clearValue.color				= { { 1.0f, 1.0f, 0.0f, 1.0f } };
		else if	(m_testMode == TestMode::MODE_DEPTH)	clearValue.depthStencil.depth	= 0.5f;
		else if (m_testMode == TestMode::MODE_STENCIL)	clearValue.depthStencil			= { 0.0f, 128u };

		const VkRect2D		renderArea			=
		{
			makeOffset2D(0, 0),
			makeExtent2D(m_renderSize.x(), m_renderSize.y()),
		};

		beginCommandBuffer(vk, *cmdBuffer);

		// Clear attachment
		beginRenderPass(vk, *cmdBuffer, *renderPassOne, *framebufferOne, renderArea, clearValue);
		endRenderPass(vk, *cmdBuffer);

		// Draw with input attachment
		beginRenderPass(vk, *cmdBuffer, *renderPassTwo, *framebufferTwo, renderArea);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, static_cast<deUint32>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *outputImage, *resultBuffer, m_renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, outputPassAttachment.finalLayout, 1u, outputAspectFlags, outputAspectFlags);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Verify results
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const tcu::ConstPixelBufferAccess	imagePixelAccess(mapVkFormat(outputFormat), m_renderSize.x(), m_renderSize.y(), 1, resultBufferAlloc->getHostPtr());
		tcu::TextureLevel					referenceImage(mapVkFormat(outputFormat), m_renderSize.x(), m_renderSize.y());
		const tcu::Vec4						clearColor	= m_testMode == TestMode::MODE_COLOR
														? tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f)
														: m_testMode == TestMode::MODE_DEPTH ? tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f)
														: tcu::Vec4(0.0f, 0.0f, 0.5f, 1.0f);

		tcu::clear(referenceImage.getAccess(), clearColor);

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage.getAccess(), imagePixelAccess, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
		{
			return tcu::TestStatus::fail("Rendered color image is not correct");
		}
	}

	return tcu::TestStatus::pass("Success");
}

TestInstance* TransientAttachmentTest::createInstance (Context& context) const
{
	return new TransientAttachmentTestInstance(context, m_testMode, m_flags, m_renderSize);
}

} // anonymous

tcu::TestCaseGroup* createTransientAttachmentTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "transient_attachment_bit", "image usage transient attachment bit load and store op test"));

	{
		static const struct
		{
			std::string					caseName;
			TestMode					testMode;
			const VkMemoryPropertyFlags	flags;
		} cases[] =
		{
			{ "color_load_store_op_test_lazy_bit",		TestMode::MODE_COLOR	,	VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT},
			{ "depth_load_store_op_test_lazy_bit",		TestMode::MODE_DEPTH	,	VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT},
			{ "stencil_load_store_op_test_lazy_bit",	TestMode::MODE_STENCIL	,	VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT},
			{ "color_load_store_op_test_local_bit",		TestMode::MODE_COLOR	,	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
			{ "depth_load_store_op_test_local_bit",		TestMode::MODE_DEPTH	,	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
			{ "stencil_load_store_op_test_local_bit",	TestMode::MODE_STENCIL	,	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}
		};

		for (const auto& testCase: cases)
		{
			testGroup->addChild(new TransientAttachmentTest(testCtx, testCase.caseName, testCase.testMode, testCase.flags, tcu::IVec2(32, 32)));
		}
	}

	return testGroup.release();
}

} // FragmentOperations
} // vkt
