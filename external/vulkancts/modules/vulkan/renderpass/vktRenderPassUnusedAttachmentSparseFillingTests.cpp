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
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
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
	RenderPassType		renderPassType;
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

class InputAttachmentSparseFillingTest : public vkt::TestCase
{
public:
										InputAttachmentSparseFillingTest	(tcu::TestContext&	testContext,
																			 const std::string&	name,
																			 const std::string&	description,
																			 const TestParams&	testParams);
	virtual								~InputAttachmentSparseFillingTest	(void);
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
	virtual								~InputAttachmentSparseFillingTestInstance	(void);
	virtual tcu::TestStatus				iterate										(void);
	template<typename RenderpassSubpass>
	void								createCommandBuffer							(const DeviceInterface&	vk,
																					 VkDevice				vkDevice);

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
};

InputAttachmentSparseFillingTest::InputAttachmentSparseFillingTest (tcu::TestContext&	testContext,
																	const std::string&	name,
																	const std::string&	description,
																	const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, description), m_testParams(testParams)
{
}

InputAttachmentSparseFillingTest::~InputAttachmentSparseFillingTest	(void)
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
	if (m_testParams.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
		context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	const vk::VkPhysicalDeviceLimits limits = getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;

	if( 2u * m_testParams.activeInputAttachmentCount > limits.maxPerStageDescriptorInputAttachments )
		TCU_THROW(NotSupportedError, "Input attachment count including unused elements exceeds maxPerStageDescriptorInputAttachments");

	if ( 2u * m_testParams.activeInputAttachmentCount > limits.maxPerStageResources)
		TCU_THROW(NotSupportedError, "Input attachment count including unused elements exceeds maxPerStageResources");
}

InputAttachmentSparseFillingTestInstance::InputAttachmentSparseFillingTestInstance (Context& context, const TestParams& testParams)
	: vkt::TestInstance	(context)
	, m_renderSize		(RENDER_SIZE, RENDER_SIZE)
	, m_vertices		(createFullscreenTriangle())
	, m_testParams		(testParams)
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
	if (testParams.renderPassType == RENDERPASS_TYPE_LEGACY)
		m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice);
	else
		m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice);

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
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
			}
		);
	}

	// Create framebuffer
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

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			static_cast<deUint32>(vertexInputAttributeDescription.size()),	// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescription.data()							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>				viewports							(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>					scissors							(1, makeRect2D(m_renderSize));

		{
			m_graphicsPipeline	= makeGraphicsPipeline(vk,									// const DeviceInterface&						vk
													  vkDevice,								// const VkDevice								device
													  *m_pipelineLayout,					// const VkPipelineLayout						pipelineLayout
													  *m_vertexShaderModule,				// const VkShaderModule							vertexShaderModule
													  DE_NULL,								// const VkShaderModule							tessellationControlModule
													  DE_NULL,								// const VkShaderModule							tessellationEvalModule
													  DE_NULL,								// const VkShaderModule							geometryShaderModule
													  *m_fragmentShaderModule,				// const VkShaderModule							fragmentShaderModule
													  *m_renderPass,						// const VkRenderPass							renderPass
													  viewports,							// const std::vector<VkViewport>&				viewports
													  scissors,								// const std::vector<VkRect2D>&					scissors
													  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology					topology
													  0u,									// const deUint32								subpass
													  0u,									// const deUint32								patchControlPoints
													  &vertexInputStateParams);				// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
		}
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
	if (testParams.renderPassType == RENDERPASS_TYPE_LEGACY)
		createCommandBuffer<RenderpassSubpass1>(vk, vkDevice);
	else
		createCommandBuffer<RenderpassSubpass2>(vk, vkDevice);
}

InputAttachmentSparseFillingTestInstance::~InputAttachmentSparseFillingTestInstance (void)
{
}

template<typename RenderpassSubpass>
void InputAttachmentSparseFillingTestInstance::createCommandBuffer (const DeviceInterface&	vk,
																	VkDevice				vkDevice)
{
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// clear output image (rg16ui) to (0,0), set image layout to VK_IMAGE_LAYOUT_GENERAL
	VkImageSubresourceRange		range				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	{
		const VkImageMemoryBarrier	outputImageInitBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// VkStructureType			sType;
			DE_NULL,												// const void*				pNext;
			0u,														// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,							// VkAccessFlags			dstAcessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_GENERAL,								// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,								// deUint32					destQueueFamilyIndex;
			**m_outputImage,										// VkImage					image;
			range													// VkImageSubresourceRange	subresourceRange;
		};
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &outputImageInitBarrier);
		VkClearValue				clearColor = makeClearValueColorU32(0, 0, 0, 0);
		vk.cmdClearColorImage(*m_cmdBuffer, **m_outputImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
		VkMemoryBarrier					memBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,						// sType
			DE_NULL,												// pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,							// srcAccessMask
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT	// dstAccessMask
		};
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}
	// clear all input attachments (rgba8) to (1,1,1,1), set image layout to VK_IMAGE_LAYOUT_GENERAL
	for (auto& inputImage : m_inputImages)
	{
		const VkImageMemoryBarrier	inputImageInitBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkAccessFlags			srcAccessMask;
			VK_ACCESS_MEMORY_WRITE_BIT,							// VkAccessFlags			dstAcessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// deUint32					destQueueFamilyIndex;
			**inputImage,										// VkImage					image;
			range												// VkImageSubresourceRange	subresourceRange;
		};
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &inputImageInitBarrier);
		VkClearValue				clearColor = makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f);

		vk.cmdClearColorImage(*m_cmdBuffer, **inputImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

		VkMemoryBarrier					memBarrier =
		{
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,					// sType
			DE_NULL,											// pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,						// srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT					// dstAccessMask
		};
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}

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

	const VkDeviceSize			vertexBufferOffset = 0;
	vk.cmdBindPipeline			(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
	vk.cmdBindDescriptorSets	(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);
	vk.cmdBindVertexBuffers		(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw					(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo(DE_NULL);
	RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	copyImageToBuffer(vk, *m_cmdBuffer, **m_outputImage, **m_outputBuffer, tcu::IVec2(m_renderSize.x(), m_renderSize.y()), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

	endCommandBuffer(vk, *m_cmdBuffer);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> InputAttachmentSparseFillingTestInstance::createRenderPass (const DeviceInterface&	vk,
																			   VkDevice					vkDevice)
{
	const VkImageAspectFlags	aspectMask						= m_testParams.renderPassType == RENDERPASS_TYPE_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;
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

	const RenderPassCreateInfo	renderPassInfo					(
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),		// deUint32							subpassCount
		subpassDescriptions.data(),								// const VkSubpassDescription*		pSubpasses
		0u,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
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

tcu::TestCaseGroup* createRenderPassUnusedAttachmentSparseFillingTests (tcu::TestContext& testCtx, const RenderPassType renderPassType)
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
		TestParams testParams{ renderPassType, activeInputAttachmentCount[attachmentNdx] };
		unusedAttTests->addChild(new InputAttachmentSparseFillingTest(testCtx, std::string("input_attachment_") + de::toString(activeInputAttachmentCount[attachmentNdx]), "", testParams));
	}

	return unusedAttTests.release();
}

} // renderpass

} // vkt
