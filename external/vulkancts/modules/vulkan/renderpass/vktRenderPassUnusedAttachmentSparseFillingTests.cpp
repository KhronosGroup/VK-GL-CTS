/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Tests sparse input attachments in VkSubpassDescription::pInputAttachments
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassUnusedAttachmentSparseFillingTests.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include <sstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

typedef de::SharedPtr<vk::Unique<vk::VkImage> >		VkImageSp;
typedef de::SharedPtr<vk::Unique<vk::VkImageView> >	VkImageViewSp;
typedef de::SharedPtr<vk::Unique<vk::VkBuffer> >	VkBufferSp;
typedef de::SharedPtr<vk::Allocation>				AllocationSp;

namespace vkt
{

namespace renderpass
{

using namespace vk;

template<typename T>
de::SharedPtr<T> safeSharedPtr(T* ptr)
{
	try
	{
		return de::SharedPtr<T>(ptr);
	}
	catch (...)
	{
		delete ptr;
		throw;
	}
}

static const deUint32		RENDER_SIZE		= 8u;
static const unsigned int	DEFAULT_SEED	= 31u;

namespace
{

struct TestParams
{
	SharedGroupParams	groupParams;
	deUint32			activeInputAttachmentCount;
};

struct Vertex
{
	tcu::Vec4 position;
	tcu::Vec4 uv;
};

std::vector<Vertex> createFullscreenTriangle (void)
{
	std::vector<Vertex>	vertices;

	for (deUint32 i = 0; i < 3; ++i)
	{
		float x = static_cast<float>((i << 1) & 2);
		float y = static_cast<float>(i & 2);
		vertices.push_back(Vertex{ tcu::Vec4(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f), tcu::Vec4(x,y,0.0f,0.0f) });
	}
	return vertices;
}

void generateInputAttachmentParams(deUint32 activeAttachmentCount, deUint32 allAttachmentCount, std::vector<deUint32>& attachmentIndices, std::vector<deUint32>& descriptorBindings)
{
	attachmentIndices.resize(allAttachmentCount);
	std::iota(begin(attachmentIndices), begin(attachmentIndices) + activeAttachmentCount, 0);
	std::fill(begin(attachmentIndices) + activeAttachmentCount, end(attachmentIndices), VK_ATTACHMENT_UNUSED);
	de::Random random(DEFAULT_SEED);
	random.shuffle(begin(attachmentIndices), end(attachmentIndices));

	descriptorBindings.resize(activeAttachmentCount+1);
	descriptorBindings[0] = VK_ATTACHMENT_UNUSED;
	for (deUint32 i = 0, lastBinding = 1; i < allAttachmentCount; ++i)
	{
		if (attachmentIndices[i] != VK_ATTACHMENT_UNUSED)
			descriptorBindings[lastBinding++] = i;
	}
}

VkImageLayout chooseInputImageLayout(const SharedGroupParams groupParams)
{
#ifndef CTS_USES_VULKANSC
	if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		// use general layout for local reads for some tests
		if (groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			return VK_IMAGE_LAYOUT_GENERAL;
		return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
	}
#else
	DE_UNREF(groupParams);
#endif
	return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

#ifndef CTS_USES_VULKANSC
void beginSecondaryCmdBuffer(const DeviceInterface&	vk,
							 VkCommandBuffer		secCmdBuffer,
							 deUint32				colorAttachmentsCount,
							 const void*			additionalInheritanceRenderingInfo)
{
	VkCommandBufferUsageFlags	usageFlags				(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	const std::vector<VkFormat>	colorAttachmentFormats	(colorAttachmentsCount, VK_FORMAT_R8G8B8A8_UNORM);

	const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
		additionalInheritanceRenderingInfo,										// const void*						pNext;
		0u,																		// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		colorAttachmentsCount,													// uint32_t							colorAttachmentCount;
		colorAttachmentFormats.data(),											// const VkFormat*					pColorAttachmentFormats;
		VK_FORMAT_UNDEFINED,													// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
		VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			rasterizationSamples;
	};
	const VkCommandBufferInheritanceInfo bufferInheritanceInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,						// VkStructureType					sType;
		&inheritanceRenderingInfo,												// const void*						pNext;
		DE_NULL,																// VkRenderPass						renderPass;
		0u,																		// deUint32							subpass;
		DE_NULL,																// VkFramebuffer					framebuffer;
		VK_FALSE,																// VkBool32							occlusionQueryEnable;
		(VkQueryControlFlags)0u,												// VkQueryControlFlags				queryFlags;
		(VkQueryPipelineStatisticFlags)0u										// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
	const VkCommandBufferBeginInfo commandBufBeginParams
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType							sType;
		DE_NULL,																// const void*								pNext;
		usageFlags,																// VkCommandBufferUsageFlags				flags;
		&bufferInheritanceInfo													// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(secCmdBuffer, &commandBufBeginParams));
}

VkRenderingInputAttachmentIndexInfoKHR getRenderingInputAttachmentIndexInfo(deUint32 activeAttachmentCount, std::vector<uint32_t>& inputAttachments)
{
	std::vector<deUint32> unnededIndices;
	generateInputAttachmentParams(activeAttachmentCount, 2u * activeAttachmentCount, unnededIndices, inputAttachments);

	return
	{
		VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
		DE_NULL,
		(deUint32)inputAttachments.size(),						// uint32_t					colorAttachmentCount
		inputAttachments.data(),								// const uint32_t*			pColorAttachmentInputIndices
		DE_NULL,												// uint32_t					depthInputAttachmentIndex
		DE_NULL,												// uint32_t					stencilInputAttachmentIndex
	};
}
#endif

class InputAttachmentSparseFillingTest : public vkt::TestCase
{
public:
										InputAttachmentSparseFillingTest	(tcu::TestContext&	testContext,
																			 const std::string&	name,
																			 const std::string&	description,
																			 const TestParams&	testParams);
	virtual								~InputAttachmentSparseFillingTest	(void) = default;
	virtual void						initPrograms						(SourceCollections&	sourceCollections) const;
	virtual TestInstance*				createInstance						(Context&			context) const;
	virtual void						checkSupport						(Context& context) const;

private:
	TestParams m_testParams;
};

class InputAttachmentSparseFillingTestInstance : public vkt::TestInstance
{
public:
										InputAttachmentSparseFillingTestInstance	(Context&			context,
																					 const TestParams&	testParams);
	virtual								~InputAttachmentSparseFillingTestInstance	(void) = default;
	virtual tcu::TestStatus				iterate										(void);

protected:
	template<typename RenderpassSubpass>
	void								createCommandBuffer							(const DeviceInterface&	vk,
																					 VkDevice				vkDevice);
	void								createCommandBufferDynamicRendering			(const DeviceInterface&	vk,
																					 VkDevice				vkDevice);
	void								preRenderCommands							(const DeviceInterface&	vk,
																					 VkCommandBuffer		cmdBuffer);
	void								drawCommands								(const DeviceInterface&	vk,
																					 VkCommandBuffer		cmdBuffer);
	void								postRenderCommands							(const DeviceInterface&	vk,
																					 VkCommandBuffer		cmdBuffer);

	template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
	Move<VkRenderPass>					createRenderPass							(const DeviceInterface&	vk,
																					 VkDevice				vkDevice);
private:
	tcu::TestStatus						verifyImage									(void);

	const tcu::UVec2					m_renderSize;
	std::vector<Vertex>					m_vertices;
	TestParams							m_testParams;

	std::vector<VkImageSp>				m_inputImages;
	std::vector<AllocationSp>			m_inputImageMemory;
	std::vector<VkImageViewSp>			m_inputImageViews;
	VkImageLayout						m_inputImageReadLayout;

	VkImageSp							m_outputImage;
	AllocationSp						m_outputImageMemory;
	VkImageViewSp						m_outputImageView;

	VkBufferSp							m_outputBuffer;
	AllocationSp						m_outputBufferMemory;

	Move<VkDescriptorSetLayout>			m_descriptorSetLayout;
	Move<VkDescriptorPool>				m_descriptorPool;
	Move<VkDescriptorSet>				m_descriptorSet;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	Move<VkShaderModule>				m_vertexShaderModule;
	Move<VkShaderModule>				m_fragmentShaderModule;

	Move<VkBuffer>						m_vertexBuffer;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_graphicsPipeline;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
	Move<VkCommandBuffer>				m_secCmdBuffer;
};

InputAttachmentSparseFillingTest::InputAttachmentSparseFillingTest (tcu::TestContext&	testContext,
																	const std::string&	name,
																	const std::string&	description,
																	const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, description), m_testParams(testParams)
{
}

void InputAttachmentSparseFillingTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream fragmentSource;

	sourceCollections.glslSources.add("vertex") << glu::VertexSource(
		"#version 450\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 uv;\n"
		"layout(location = 0) out vec4 outUV;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	outUV = uv;\n"
		"}\n");

	// We read from X input attachments randomly spread in input attachment array of size 2*X
	std::ostringstream str;
	str	<< "#version 450\n"
		<< "layout(location = 0) in vec4 inUV;\n"
		<< "layout(binding = 0, rg32ui) uniform uimage2D resultImage;\n";

	std::vector<deUint32> attachmentIndices, descriptorBindings;
	generateInputAttachmentParams(m_testParams.activeInputAttachmentCount, 2u * m_testParams.activeInputAttachmentCount, attachmentIndices, descriptorBindings);

	for (std::size_t i = 1; i < descriptorBindings.size(); ++i)
		str << "layout(binding = " << i << ", input_attachment_index = " << descriptorBindings[i] <<") uniform subpassInput attach" << i <<";\n";

	str << "void main (void)\n"
		<< "{\n"
		<< "	uvec4 result = uvec4(0);\n";

	for (std::size_t i = 1; i < descriptorBindings.size(); ++i)
	{
		str << "	result.x = result.x + 1;\n";
		str << "	if(subpassLoad(attach" << i << ").x > 0.0)\n";
		str << "		result.y = result.y + 1;\n";
	}

	str	<< "	imageStore(resultImage, ivec2(imageSize(resultImage) * inUV.xy), result);\n"
		<< "}\n";

	sourceCollections.glslSources.add("fragment") << glu::FragmentSource(str.str());
}

TestInstance* InputAttachmentSparseFillingTest::createInstance(Context& context) const
{
	return new InputAttachmentSparseFillingTestInstance(context, m_testParams);
}

void InputAttachmentSparseFillingTest::checkSupport(Context& context) const
{
	const InstanceInterface&				vki				= context.getInstanceInterface();
	vk::VkPhysicalDevice					physicalDevice	= context.getPhysicalDevice();
	const vk::VkPhysicalDeviceProperties	properties		= vk::getPhysicalDeviceProperties(vki, physicalDevice);
	const vk::VkPhysicalDeviceLimits&		limits			= properties.limits;

	if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
		context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
	else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
		if ((2u * m_testParams.activeInputAttachmentCount) > limits.maxColorAttachments)
			TCU_THROW(NotSupportedError, "Required number of color attachments not supported.");
	}

	if( 2u * m_testParams.activeInputAttachmentCount > limits.maxPerStageDescriptorInputAttachments )
		TCU_THROW(NotSupportedError, "Input attachment count including unused elements exceeds maxPerStageDescriptorInputAttachments");

	if ( 2u * m_testParams.activeInputAttachmentCount > limits.maxPerStageResources)
		TCU_THROW(NotSupportedError, "Input attachment count including unused elements exceeds maxPerStageResources");
}

InputAttachmentSparseFillingTestInstance::InputAttachmentSparseFillingTestInstance (Context& context, const TestParams& testParams)
	: vkt::TestInstance		(context)
	, m_renderSize			(RENDER_SIZE, RENDER_SIZE)
	, m_vertices			(createFullscreenTriangle())
	, m_testParams			(testParams)
	, m_inputImageReadLayout(chooseInputImageLayout(testParams.groupParams))
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						vkDevice				= m_context.getDevice();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator						memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping			componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	{
		const VkImageCreateInfo	inputImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			VK_FORMAT_R8G8B8A8_UNORM,												// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		VkImageViewCreateInfo inputAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,								// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageViewCreateFlags	flags;
			0,																		// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,													// VkImageViewType			viewType;
			VK_FORMAT_R8G8B8A8_UNORM,												// VkFormat					format;
			componentMappingRGBA,													// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }							// VkImageSubresourceRange	subresourceRange;
		};

		// Create input attachment images with image views
		for (deUint32 imageNdx = 0; imageNdx < m_testParams.activeInputAttachmentCount; ++imageNdx)
		{
			auto inputImage					= safeSharedPtr(new Unique<VkImage>(vk::createImage(vk, vkDevice, &inputImageParams)));

			auto inputImageAlloc			= safeSharedPtr(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, **inputImage), MemoryRequirement::Any).release());
			VK_CHECK(vk.bindImageMemory(vkDevice, **inputImage, inputImageAlloc->getMemory(), inputImageAlloc->getOffset()));

			inputAttachmentViewParams.image	= **inputImage;
			auto inputImageView				= safeSharedPtr(new Unique<VkImageView>(createImageView(vk, vkDevice, &inputAttachmentViewParams)));

			m_inputImages.push_back(inputImage);
			m_inputImageMemory.push_back(inputImageAlloc);
			m_inputImageViews.push_back(inputImageView);
		}
	}

	{
		const VkImageCreateInfo	outputImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			VK_FORMAT_R32G32_UINT,													// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,									// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		m_outputImage		= safeSharedPtr(new Unique<VkImage>(vk::createImage(vk, vkDevice, &outputImageParams)));
		m_outputImageMemory = safeSharedPtr(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, **m_outputImage), MemoryRequirement::Any).release());
		VK_CHECK(vk.bindImageMemory(vkDevice, **m_outputImage, m_outputImageMemory->getMemory(), m_outputImageMemory->getOffset()));

		VkImageViewCreateInfo inputAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,								// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageViewCreateFlags	flags;
			**m_outputImage,														// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,													// VkImageViewType			viewType;
			VK_FORMAT_R32G32_UINT,													// VkFormat					format;
			componentMappingRGBA,													// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }							// VkImageSubresourceRange	subresourceRange;
		};
		m_outputImageView = safeSharedPtr(new Unique<VkImageView>(createImageView(vk, vkDevice, &inputAttachmentViewParams)));
	}

	{
		const VkDeviceSize			outputBufferSizeBytes	= m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(VK_FORMAT_R32G32_UINT));
		const VkBufferCreateInfo	outputBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
			DE_NULL,									// pNext
			(VkBufferCreateFlags)0u,					// flags
			outputBufferSizeBytes,						// size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
			VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
			1u,											// queueFamilyIndexCount
			&queueFamilyIndex,							// pQueueFamilyIndices
		};
		m_outputBuffer			= safeSharedPtr(new Unique<VkBuffer>(createBuffer(vk, vkDevice, &outputBufferParams)));
		m_outputBufferMemory	= safeSharedPtr(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, **m_outputBuffer), MemoryRequirement::HostVisible).release());
		VK_CHECK(vk.bindBufferMemory(vkDevice, **m_outputBuffer, m_outputBufferMemory->getMemory(), m_outputBufferMemory->getOffset()));
	}

	// Create render pass
	if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
		m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice);
	else if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
		m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice);
	else
		m_renderPass = Move<VkRenderPass>();

	std::vector<VkDescriptorImageInfo>	descriptorImageInfos;
	std::vector<VkImageView> framebufferImageViews;
	descriptorImageInfos.push_back(
		VkDescriptorImageInfo{
			DE_NULL,				// VkSampleri		sampler;
			**m_outputImageView,	// VkImageView		imageView;
			VK_IMAGE_LAYOUT_GENERAL	// VkImageLayout	imageLayout;
		}
	);
	for (auto& inputImageView : m_inputImageViews)
	{
		framebufferImageViews.push_back(**inputImageView);
		descriptorImageInfos.push_back(
			VkDescriptorImageInfo{
				DE_NULL,									// VkSampleri		sampler;
				**inputImageView,							// VkImageView		imageView;
				m_inputImageReadLayout						// VkImageLayout	imageLayout;
			}
		);
	}

	// Create framebuffer if renderpass handle is valid
	if (*m_renderPass != DE_NULL)
	{
		const VkFramebufferCreateInfo	framebufferParams	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// VkStructureType			sType;
			DE_NULL,												// const void*				pNext;
			0u,														// VkFramebufferCreateFlags	flags;
			*m_renderPass,											// VkRenderPass				renderPass;
			static_cast<deUint32>(framebufferImageViews.size()),	// deUint32					attachmentCount;
			framebufferImageViews.data(),							// const VkImageView*		pAttachments;
			static_cast<deUint32>(m_renderSize.x()),				// deUint32					width;
			static_cast<deUint32>(m_renderSize.y()),				// deUint32					height;
			1u														// deUint32					layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		DescriptorSetLayoutBuilder	layoutBuilder;
		// add output image storage
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
		// add input attachments
		for (deUint32 imageNdx = 0; imageNdx < m_testParams.activeInputAttachmentCount; ++imageNdx)
			layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_descriptorSetLayout = layoutBuilder.build(vk, vkDevice);

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineLayoutCreateFlags		flags;
			1u,														// deUint32							setLayoutCount;
			&m_descriptorSetLayout.get(),							// const VkDescriptorSetLayout*		pSetLayouts;
			0u,														// deUint32							pushConstantRangeCount;
			DE_NULL													// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Update descriptor set
	{
		m_descriptorPool = DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,		1u)
			.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	m_testParams.activeInputAttachmentCount)
			.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		const VkDescriptorSetAllocateInfo	descriptorSetAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType					sType
			DE_NULL,											// const void*						pNext
			*m_descriptorPool,									// VkDescriptorPool					descriptorPool
			1u,													// deUint32							descriptorSetCount
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*		pSetLayouts
		};
		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

		DescriptorSetUpdateBuilder builder;
		builder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[0]);
		for( deUint32 i=1; i<static_cast<deUint32>(descriptorImageInfos.size()); ++i)
			builder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descriptorImageInfos[i]);
		builder.update(vk, vkDevice);
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vertex"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("fragment"), 0);

	// Create pipelines
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,								// deUint32					binding;
			sizeof(Vertex),					// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	inputRate;
		};

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescription	=
		{
			{
				0u,								// deUint32		location;
				0u,								// deUint32		binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat		format;
				0u								// deUint32		offset;
			},
			{
				1u,								// deUint32		location;
				0u,								// deUint32		binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat		format;
				DE_OFFSET_OF(Vertex, uv)		// deUint32		offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			static_cast<deUint32>(vertexInputAttributeDescription.size()),	// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescription.data()							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
		deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
		colorBlendAttachmentState.colorWriteMask = 0xF;

		deUint32 colorAttachmentsCount = (*m_renderPass == DE_NULL) ? m_testParams.activeInputAttachmentCount : 1u;
		const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(colorAttachmentsCount, colorBlendAttachmentState);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault = initVulkanStructure();
		colorBlendStateCreateInfoDefault.attachmentCount = deUint32(colorBlendAttachmentStates.size());
		colorBlendStateCreateInfoDefault.pAttachments = colorBlendAttachmentStates.data();

		const std::vector<VkViewport>	viewports	(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_renderSize));

		void* pNext = DE_NULL;
#ifndef CTS_USES_VULKANSC
		std::vector<deUint32> inputAttachments;
		const auto renderingInputAttachmentIndexInfo = getRenderingInputAttachmentIndexInfo(m_testParams.activeInputAttachmentCount, inputAttachments);

		const std::vector<VkFormat> colorAttachmentFormats(colorAttachmentsCount, VK_FORMAT_R8G8B8A8_UNORM);
		VkPipelineRenderingCreateInfo renderingCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			&renderingInputAttachmentIndexInfo,
			0u,
			(deUint32)colorAttachmentFormats.size(),
			colorAttachmentFormats.data(),
			VK_FORMAT_UNDEFINED,
			VK_FORMAT_UNDEFINED
		};

		if (*m_renderPass == DE_NULL)
			pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

		m_graphicsPipeline	= makeGraphicsPipeline(vk,										// const DeviceInterface&							vk
													vkDevice,								// const VkDevice									device
													*m_pipelineLayout,						// const VkPipelineLayout							pipelineLayout
													*m_vertexShaderModule,					// const VkShaderModule								vertexShaderModule
													DE_NULL,								// const VkShaderModule								tessellationControlModule
													DE_NULL,								// const VkShaderModule								tessellationEvalModule
													DE_NULL,								// const VkShaderModule								geometryShaderModule
													*m_fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
													*m_renderPass,							// const VkRenderPass								renderPass
													viewports,								// const std::vector<VkViewport>&					viewports
													scissors,								// const std::vector<VkRect2D>&						scissors
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
													0u,										// const deUint32									subpass
													0u,										// const deUint32									patchControlPoints
													&vertexInputStateParams,				// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
													DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
													DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
													DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
													&colorBlendStateCreateInfoDefault,		// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
													DE_NULL,								// const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo
													pNext);									// const void*										pNext
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,					// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			0u,														// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex) * m_vertices.size()),		// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,						// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode		sharingMode;
			1u,														// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex										// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Upload vertex data
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
		createCommandBuffer<RenderpassSubpass1>(vk, vkDevice);
	else if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
		createCommandBuffer<RenderpassSubpass2>(vk, vkDevice);
	else
		createCommandBufferDynamicRendering(vk, vkDevice);
}

template<typename RenderpassSubpass>
void InputAttachmentSparseFillingTestInstance::createCommandBuffer (const DeviceInterface&	vk,
																	VkDevice				vkDevice)
{
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	preRenderCommands(vk, *m_cmdBuffer);

	// Render pass does not use clear values - input images were prepared beforehand
	const VkRenderPassBeginInfo renderPassBeginInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
		DE_NULL,												// const void*			pNext;
		*m_renderPass,											// VkRenderPass			renderPass;
		*m_framebuffer,											// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),								// VkRect2D				renderArea;
		0,														// uint32_t				clearValueCount;
		DE_NULL													// const VkClearValue*	pClearValues;
	};
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	RenderpassSubpass::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);

	drawCommands(vk, *m_cmdBuffer);

	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo(DE_NULL);
	RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	postRenderCommands(vk, *m_cmdBuffer);

	endCommandBuffer(vk, *m_cmdBuffer);
}

void InputAttachmentSparseFillingTestInstance::createCommandBufferDynamicRendering(const DeviceInterface& vk, VkDevice vkDevice)
{
#ifndef CTS_USES_VULKANSC
	std::vector<VkRenderingAttachmentInfo> colorAttachments(m_testParams.activeInputAttachmentCount,
		{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,		// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			VK_NULL_HANDLE,										// VkImageView				imageView;
			m_inputImageReadLayout,								// VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode;
			DE_NULL,											// VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_LOAD,							// VkAttachmentLoadOp		loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		storeOp;
			makeClearValueColorU32(0, 0, 0, 0)					// VkClearValue				clearValue;
		});
	for (std::size_t i = 0; i < m_testParams.activeInputAttachmentCount; ++i)
		colorAttachments[i].imageView = **m_inputImageViews[i];

	std::vector<deUint32> inputAttachments;
	const auto renderingInputAttachmentIndexInfo = getRenderingInputAttachmentIndexInfo(m_testParams.activeInputAttachmentCount, inputAttachments);

	VkRenderingInfo renderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO,
		DE_NULL,
		0,														// VkRenderingFlagsKHR					flags;
		makeRect2D(m_renderSize),								// VkRect2D								renderArea;
		1u,														// deUint32								layerCount;
		0u,														// deUint32								viewMask;
		(deUint32)colorAttachments.size(),						// deUint32								colorAttachmentCount;
		colorAttachments.data(),								// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
	{
		m_secCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

		// record secondary command buffer
		beginSecondaryCmdBuffer(vk, *m_secCmdBuffer, m_testParams.activeInputAttachmentCount, &renderingInputAttachmentIndexInfo);
		vk.cmdBeginRendering(*m_secCmdBuffer, &renderingInfo);
		drawCommands(vk, *m_secCmdBuffer);
		vk.cmdEndRendering(*m_secCmdBuffer);
		endCommandBuffer(vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(vk, *m_cmdBuffer);
		preRenderCommands(vk, *m_cmdBuffer);
		vk.cmdSetRenderingInputAttachmentIndicesKHR(*m_cmdBuffer, &renderingInputAttachmentIndexInfo);
		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);
		postRenderCommands(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
	else
	{
		beginCommandBuffer(vk, *m_cmdBuffer, 0u);
		preRenderCommands(vk, *m_cmdBuffer);
		vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);

		vk.cmdSetRenderingInputAttachmentIndicesKHR(*m_cmdBuffer, &renderingInputAttachmentIndexInfo);
		drawCommands(vk, *m_cmdBuffer);

		vk.cmdEndRendering(*m_cmdBuffer);
		postRenderCommands(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}

#else
	DE_UNREF(vk);
	DE_UNREF(vkDevice);
#endif // CTS_USES_VULKANSC
}

void InputAttachmentSparseFillingTestInstance::preRenderCommands(const DeviceInterface& vk, VkCommandBuffer cmdBuffer)
{
	// clear output image (rg16ui) to (0,0), set image layout to VK_IMAGE_LAYOUT_GENERAL
	VkImageSubresourceRange		range					= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageMemoryBarrier	outputImageInitBarrier	= makeImageMemoryBarrier(
		0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, **m_outputImage, range);
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &outputImageInitBarrier);

	VkClearValue clearColor = makeClearValueColorU32(0, 0, 0, 0);
	vk.cmdClearColorImage(cmdBuffer, **m_outputImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

	VkMemoryBarrier memBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	// clear all input attachments (rgba8) to (1,1,1,1), set image layout to VK_IMAGE_LAYOUT_GENERAL
	clearColor = makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f);
	for (auto& inputImage : m_inputImages)
	{
		VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(
			0u, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, **inputImage, range);
		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

		vk.cmdClearColorImage(cmdBuffer, **inputImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
	}

	memBarrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
}

void InputAttachmentSparseFillingTestInstance::drawCommands(const DeviceInterface& vk, VkCommandBuffer cmdBuffer)
{
	const VkDeviceSize vertexBufferOffset = 0;
	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
	vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw(cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
}

void InputAttachmentSparseFillingTestInstance::postRenderCommands(const DeviceInterface& vk, VkCommandBuffer cmdBuffer)
{
	copyImageToBuffer(vk, cmdBuffer, **m_outputImage, **m_outputBuffer, tcu::IVec2(m_renderSize.x(), m_renderSize.y()), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> InputAttachmentSparseFillingTestInstance::createRenderPass (const DeviceInterface&	vk,
																			   VkDevice					vkDevice)
{
	const VkImageAspectFlags	aspectMask						= m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;
	std::vector<AttachmentDesc>	attachmentDescriptions;
	std::vector<AttachmentRef>	attachmentRefs;

	std::vector<deUint32>		attachmentIndices;
	std::vector<deUint32>		descriptorBindings;
	generateInputAttachmentParams(m_testParams.activeInputAttachmentCount, 2u * m_testParams.activeInputAttachmentCount, attachmentIndices, descriptorBindings);

	for (deUint32 i = 0; i < m_testParams.activeInputAttachmentCount; ++i)
	{
		attachmentDescriptions.push_back(
			AttachmentDesc(
				DE_NULL,									// const void*						pNext
				(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags
				VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat							format
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples
				VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp
				VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					initialLayout
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout					finalLayout
			)
		);
	}
	for (std::size_t i = 0; i < attachmentIndices.size(); ++i)
		attachmentRefs.push_back(
			AttachmentRef(
				DE_NULL,									// const void*			pNext
				attachmentIndices[i],						// deUint32				attachment
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout		layout
				aspectMask									// VkImageAspectFlags	aspectMask
			)
		);

	std::vector<SubpassDesc>		subpassDescriptions			=
	{
		SubpassDesc (
			DE_NULL,
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint
			0u,													// deUint32							viewMask
			static_cast<deUint32>(attachmentRefs.size()),		// deUint32							inputAttachmentCount
			attachmentRefs.data(),								// const VkAttachmentReference*		pInputAttachments
			0u,													// deUint32							colorAttachmentCount
			DE_NULL,											// const VkAttachmentReference*		pColorAttachments
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment
			0u,													// deUint32							preserveAttachmentCount
			DE_NULL												// const deUint32*					pPreserveAttachments
		),
	};
	std::vector<SubpassDep>		subpassDependencies =
	{
		SubpassDep (
			DE_NULL,
			0u,										// deUint32				srcPass
			VK_SUBPASS_EXTERNAL,					// deUint32				dstPass
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,	// VkPipelineStageFlags	srcStageMask
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,	// VkPipelineStageFlags	dstStageMask
			VK_ACCESS_SHADER_WRITE_BIT,				// VkAccessFlags		srcAccessMask
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT,	// VkAccessFlags		dstAccessMask
			0,										// VkDependencyFlags	flags
			0										// deInt32				viewOffset
		),
	};

	const RenderPassCreateInfo	renderPassInfo					(
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),		// deUint32							subpassCount
		subpassDescriptions.data(),								// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),		// deUint32							dependencyCount
		subpassDependencies.data(),								// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

tcu::TestStatus InputAttachmentSparseFillingTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus InputAttachmentSparseFillingTestInstance::verifyImage (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						vkDevice				= m_context.getDevice();

	invalidateAlloc(vk, vkDevice, *m_outputBufferMemory);
	const tcu::ConstPixelBufferAccess resultAccess(mapVkFormat(VK_FORMAT_R32G32_UINT), m_renderSize.x(), m_renderSize.y(), 1u, m_outputBufferMemory->getHostPtr());

	// Log result image
	m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "Result images")
		<< tcu::TestLog::Image("Rendered", "Rendered image", resultAccess)
		<< tcu::TestLog::EndImageSet;

	// Check the unused image data hasn't changed.
	for (int y = 0; y < resultAccess.getHeight(); y++)
		for (int x = 0; x < resultAccess.getWidth(); x++)
		{
			tcu::UVec4 color = resultAccess.getPixelUint(x, y);
			if( color.x() != m_testParams.activeInputAttachmentCount)
				return tcu::TestStatus::fail("Wrong attachment count");
			if( color.y() != m_testParams.activeInputAttachmentCount )
				return tcu::TestStatus::fail("Wrong active attachment count");
		}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createRenderPassUnusedAttachmentSparseFillingTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	de::MovePtr<tcu::TestCaseGroup>		unusedAttTests		(new tcu::TestCaseGroup(testCtx, "attachment_sparse_filling", "Unused attachment tests"));

	const std::vector<deUint32> activeInputAttachmentCount
	{
		1u,
		3u,
		7u,
		15u,
		31u,
		63u,
		127u
	};

	for (std::size_t attachmentNdx = 0; attachmentNdx < activeInputAttachmentCount.size(); ++attachmentNdx)
	{
		TestParams testParams{ groupParams, activeInputAttachmentCount[attachmentNdx] };
		unusedAttTests->addChild(new InputAttachmentSparseFillingTest(testCtx, std::string("input_attachment_") + de::toString(activeInputAttachmentCount[attachmentNdx]), "", testParams));
	}

	return unusedAttTests.release();
}

} // renderpass

} // vkt
