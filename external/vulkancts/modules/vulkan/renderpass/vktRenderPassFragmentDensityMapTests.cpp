/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests fragment density map extension ( VK_EXT_fragment_density_map )
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassFragmentDensityMapTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "deMath.h"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include <sstream>
#include <vector>
#include <set>

// Each test generates an image with a color gradient where all colors should be unique when rendered without density map
// ( and for multi_view tests - the quantity of each color in a histogram should be 2 instead of 1 ).
// The whole density map has the same values defined by input fragment area ( one of the test input parameters ).
// With density map enabled - the number of each color in a histogram should be [ fragmentArea.x * fragmentArea.y ]
// ( that value will be doubled for multi_view case ).
//
// Additionally test checks if gl_FragSizeEXT shader variable has proper value ( as defined by fragmentArea input parameter ).
//
// Test variations:
// - multi_view tests check if density map also works when VK_KHR_multiview extension is in use
// - render_copy tests check if it's possible to copy results using input attachment descriptor ( this simulates deferred rendering behaviour )
// - non_divisible_density_size tests check if subsampled images work when its dimension is not divisible by minFragmentDensityTexelSize
// - N_samples tests check if multisampling works with VK_EXT_fragment_density_map extension
// - static_* tests use density map loaded from CPU during vkCmdBeginRenderPass.
// - dynamic_* tests use density map rendered on a GPU in a separate render pass
// - deffered_* tests use density map loaded from CPU during VkEndCommandBuffer.
// - *_nonsubsampled tests check if it's possible to use nonsubsampled images instead of subsampled ones

// There are 3 render passes performed during most of the tests:
//  - render pass that produces density map ( this rp is skipped when density map is static )
//  - render pass that produces subsampled image using density map and eventually copies results to different image ( render_copy )
//  - render pass that copies subsampled image to traditional image using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT flag.
//    ( because subsampled images cannot be retrieved to CPU in any other way ).
// There are few tests that use additional subpass that resamples subsampled image using diferent density map.

// Code of FragmentDensityMapTestInstance is also used to test subsampledLoads, subsampledCoarseReconstructionEarlyAccess,
// maxDescriptorSetSubsampledSamplers properties.

namespace vkt
{

namespace renderpass
{

using namespace vk;

namespace
{

struct TestParams
{
	bool					dynamicDensityMap;
	bool					deferredDensityMap;
	bool					nonSubsampledImages;
	bool					subsampledLoads;
	bool					coarseReconstruction;
	bool					imagelessFramebuffer;
	bool					useMemoryAccess;
	bool					useMaintenance5;
	deUint32				samplersCount;
	deUint32				viewCount;
	bool					multiViewport;
	bool					makeCopy;
	float					renderMultiplier;
	VkSampleCountFlagBits	colorSamples;
	tcu::UVec2				fragmentArea;
	tcu::UVec2				densityMapSize;
	VkFormat				densityMapFormat;
	const SharedGroupParams	groupParams;
};

struct Vertex4RGBA
{
	tcu::Vec4	position;
	tcu::Vec4	uv;
	tcu::Vec4	color;
};

de::SharedPtr<Move<vk::VkDevice>>	g_singletonDevice;

VkDevice getDevice(Context& context)
{
	if (!g_singletonDevice)
	{
		const float queuePriority = 1.0f;

		// Create a universal queue that supports graphics and compute
		const VkDeviceQueueCreateInfo queueParams
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkDeviceQueueCreateFlags		flags;
			context.getUniversalQueueFamilyIndex(),		// deUint32						queueFamilyIndex;
			1u,											// deUint32						queueCount;
			&queuePriority								// const float*					pQueuePriorities;
		};

		// \note Extensions in core are not explicitly enabled even though
		//		 they are in the extension list advertised to tests.
		const auto& extensionPtrs = context.getDeviceCreationExtensions();

		VkPhysicalDevicePortabilitySubsetFeaturesKHR	portabilitySubsetFeatures		= initVulkanStructure();
		VkPhysicalDeviceMultiviewFeatures				multiviewFeatures				= initVulkanStructure();
		VkPhysicalDeviceImagelessFramebufferFeatures	imagelessFramebufferFeatures	= initVulkanStructure();
		VkPhysicalDeviceDynamicRenderingFeatures		dynamicRenderingFeatures		= initVulkanStructure();
		VkPhysicalDeviceFragmentDensityMap2FeaturesEXT	fragmentDensityMap2Features		= initVulkanStructure();
		VkPhysicalDeviceFragmentDensityMapFeaturesEXT	fragmentDensityMapFeatures		= initVulkanStructure();
		VkPhysicalDeviceFeatures2						features2						= initVulkanStructure();

		const auto addFeatures = makeStructChainAdder(&features2);

		if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
			addFeatures(&portabilitySubsetFeatures);

		if (context.isDeviceFunctionalitySupported("VK_KHR_multiview"))
			addFeatures(&multiviewFeatures);

		if (context.isDeviceFunctionalitySupported("VK_KHR_imageless_framebuffer"))
			addFeatures(&imagelessFramebufferFeatures);

		if (context.isDeviceFunctionalitySupported("VK_KHR_dynamic_rendering"))
			addFeatures(&dynamicRenderingFeatures);

		if (context.isDeviceFunctionalitySupported("VK_EXT_fragment_density_map2"))
			addFeatures(&fragmentDensityMap2Features);

		addFeatures(&fragmentDensityMapFeatures);

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
		features2.features.robustBufferAccess = VK_FALSE;

		const VkDeviceCreateInfo deviceCreateInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//sType;
			&features2,														//pNext;
			0u,																//flags
			1,																//queueRecordCount;
			&queueParams,													//pRequestedQueues;
			0u,																//layerCount;
			nullptr,														//ppEnabledLayerNames;
			de::sizeU32(extensionPtrs),										// deUint32				enabledExtensionCount;
			de::dataOrNull(extensionPtrs),									// const char* const*	ppEnabledExtensionNames;
			nullptr,														//pEnabledFeatures;
		};

		Move<VkDevice> device = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceCreateInfo);
		g_singletonDevice = de::SharedPtr<Move<VkDevice>>(new Move<VkDevice>(device));
	}

	return g_singletonDevice->get();
}

std::vector<Vertex4RGBA> createFullscreenMesh(deUint32 viewCount, tcu::Vec2 redGradient, tcu::Vec2 greenGradient)
{
	DE_ASSERT(viewCount > 0);

	const auto&		r		= redGradient;
	const auto&		g		= greenGradient;
	const float		step	= 2.0f / static_cast<float>(viewCount);
	float			xStart	= -1.0f;

	std::vector<Vertex4RGBA> resultMesh;
	for (deUint32 viewIndex = 0; viewIndex < viewCount ; ++viewIndex)
	{
		const float		fIndex		= static_cast<float>(viewIndex);
		const deUint32	nextIndex	= viewIndex + 1;
		const float		xEnd		= (nextIndex == viewCount) ? 1.0f : (-1.0f + step * static_cast<float>(nextIndex));

		// quad vertex							position						uv								color
		const Vertex4RGBA lowerLeftVertex	= { { xStart,  1.0f, 0.0f, 1.0f },	{ 0.0f, 1.0f, fIndex, 1.0f },	{ r.x(), g.y(), 0.0f, 1.0f } };
		const Vertex4RGBA upperLeftVertex	= { { xStart, -1.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f, fIndex, 1.0f },	{ r.x(), g.x(), 0.0f, 1.0f } };
		const Vertex4RGBA lowerRightVertex	= { {   xEnd,  1.0f, 0.0f, 1.0f },	{ 1.0f, 1.0f, fIndex, 1.0f },	{ r.y(), g.y(), 0.0f, 1.0f } };
		const Vertex4RGBA upperRightVertex	= { {   xEnd, -1.0f, 0.0f, 1.0f },	{ 1.0f, 0.0f, fIndex, 1.0f },	{ r.y(), g.x(), 0.0f, 1.0f } };

		const std::vector<Vertex4RGBA> viewData
		{
			lowerLeftVertex, lowerRightVertex, upperLeftVertex,
			upperLeftVertex, lowerRightVertex, upperRightVertex
		};

		resultMesh.insert(resultMesh.end(), viewData.begin(), viewData.end());
		xStart = xEnd;
	}

	return resultMesh;
}

template <typename T>
void createVertexBuffer(const DeviceInterface&		vk,
						VkDevice					vkDevice,
						const deUint32&				queueFamilyIndex,
						SimpleAllocator&			memAlloc,
						const std::vector<T>&		vertices,
						Move<VkBuffer>&				vertexBuffer,
						de::MovePtr<Allocation>&	vertexAlloc)
{
	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,			// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		0u,												// VkBufferCreateFlags	flags;
		(VkDeviceSize)(sizeof(T) * vertices.size()),	// VkDeviceSize			size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,				// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode		sharingMode;
		1u,												// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex								// const deUint32*		pQueueFamilyIndices;
	};

	vertexBuffer	= createBuffer(vk, vkDevice, &vertexBufferParams);
	vertexAlloc		= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

	// Upload vertex data
	deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(T));
	flushAlloc(vk, vkDevice, *vertexAlloc);
}

void prepareImageAndImageView	(const DeviceInterface&			vk,
								 VkDevice						vkDevice,
								 SimpleAllocator&				memAlloc,
								 VkImageCreateFlags				imageCreateFlags,
								 VkFormat						format,
								 VkExtent3D						extent,
								 deUint32						arrayLayers,
								 VkSampleCountFlagBits			samples,
								 VkImageUsageFlags				usage,
								 deUint32						queueFamilyIndex,
								 VkImageViewCreateFlags			viewFlags,
								 VkImageViewType				viewType,
								 const VkComponentMapping&		channels,
								 const VkImageSubresourceRange&	subresourceRange,
								 Move<VkImage>&					image,
								 de::MovePtr<Allocation>&		imageAlloc,
								 Move<VkImageView>&				imageView)
{
	const VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		imageCreateFlags,							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
		format,										// VkFormat					format;
		extent,										// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		arrayLayers,								// deUint32					arrayLayers;
		samples,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
	};

	image = createImage(vk, vkDevice, &imageCreateInfo);

	// Allocate and bind color image memory
	imageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

	// create image view for subsampled image
	const VkImageViewCreateInfo imageViewCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		viewFlags,									// VkImageViewCreateFlags	flags;
		*image,										// VkImage					image;
		viewType,									// VkImageViewType			viewType;
		format,										// VkFormat					format;
		channels,									// VkChannelMapping			channels;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	imageView = createImageView(vk, vkDevice, &imageViewCreateInfo);
}

// Class that provides abstraction over renderpass and renderpass2.
class RenderPassWrapperBase
{
public:

	RenderPassWrapperBase() = default;
	virtual ~RenderPassWrapperBase() = default;

	virtual Move<VkRenderPass>	createRenderPassProduceDynamicDensityMap	(deUint32						viewMask) const = 0;
	virtual Move<VkRenderPass>	createRenderPassProduceSubsampledImage		(deUint32						viewMask,
																			 bool							makeCopySubpass,
																			 bool							resampleSubsampled) const = 0;
	virtual Move<VkRenderPass>	createRenderPassOutputSubsampledImage		() const = 0;

	virtual void				cmdBeginRenderPass							(VkCommandBuffer				cmdBuffer,
																			 const VkRenderPassBeginInfo*	pRenderPassBegin) const = 0;
	virtual void				cmdNextSubpass								(VkCommandBuffer				cmdBuffer) const = 0;
	virtual void				cmdEndRenderPass							(VkCommandBuffer				cmdBuffer) const = 0;
};

// Helper template that lets us define all used types basing on single enum value.
template <RenderingType>
struct RenderPassTraits;

template <> struct RenderPassTraits<RENDERING_TYPE_RENDERPASS_LEGACY>
{
	typedef AttachmentDescription1	AttachmentDesc;
	typedef AttachmentReference1	AttachmentRef;
	typedef SubpassDescription1		SubpassDesc;
	typedef SubpassDependency1		SubpassDep;
	typedef RenderPassCreateInfo1	RenderPassCreateInfo;
	typedef RenderpassSubpass1		RenderpassSubpass;
};

template <> struct RenderPassTraits<RENDERING_TYPE_RENDERPASS2>
{
	typedef AttachmentDescription2	AttachmentDesc;
	typedef AttachmentReference2	AttachmentRef;
	typedef SubpassDescription2		SubpassDesc;
	typedef SubpassDependency2		SubpassDep;
	typedef RenderPassCreateInfo2	RenderPassCreateInfo;
	typedef RenderpassSubpass2		RenderpassSubpass;
};

// Template that can be used to construct required
// renderpasses using legacy renderpass and renderpass2.
template <RenderingType RenderingTypeValue>
class RenderPassWrapper: public RenderPassWrapperBase
{
	typedef typename RenderPassTraits<RenderingTypeValue>::AttachmentDesc		AttachmentDesc;
	typedef typename RenderPassTraits<RenderingTypeValue>::AttachmentRef		AttachmentRef;
	typedef typename RenderPassTraits<RenderingTypeValue>::SubpassDesc			SubpassDesc;
	typedef typename RenderPassTraits<RenderingTypeValue>::SubpassDep			SubpassDep;
	typedef typename RenderPassTraits<RenderingTypeValue>::RenderPassCreateInfo	RenderPassCreateInfo;
	typedef typename RenderPassTraits<RenderingTypeValue>::RenderpassSubpass	RenderpassSubpass;

public:

	RenderPassWrapper(const DeviceInterface& vk, const VkDevice vkDevice, const TestParams& testParams);
	~RenderPassWrapper() = default;

	Move<VkRenderPass>	createRenderPassProduceDynamicDensityMap	(deUint32						viewMask) const override;
	Move<VkRenderPass>	createRenderPassProduceSubsampledImage		(deUint32						viewMask,
																	 bool							makeCopySubpass,
																	 bool							resampleSubsampled) const override;
	Move<VkRenderPass>	createRenderPassOutputSubsampledImage		() const override;

	void				cmdBeginRenderPass							(VkCommandBuffer				cmdBufferm,
																	 const VkRenderPassBeginInfo*	pRenderPassBegin) const override;
	void				cmdNextSubpass								(VkCommandBuffer				cmdBuffer) const override;
	void				cmdEndRenderPass							(VkCommandBuffer				cmdBuffer) const override;

private:

	const DeviceInterface&	m_vk;
	const VkDevice			m_vkDevice;
	const TestParams&		m_testParams;

	const typename RenderpassSubpass::SubpassBeginInfo	m_subpassBeginInfo;
	const typename RenderpassSubpass::SubpassEndInfo	m_subpassEndInfo;
};

template<RenderingType RenderingTypeValue>
RenderPassWrapper<RenderingTypeValue>::RenderPassWrapper(const DeviceInterface& vk, const VkDevice vkDevice, const TestParams& testParams)
	: RenderPassWrapperBase		()
	, m_vk						(vk)
	, m_vkDevice				(vkDevice)
	, m_testParams				(testParams)
	, m_subpassBeginInfo		(DE_NULL, testParams.groupParams->useSecondaryCmdBuffer ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE)
	, m_subpassEndInfo			(DE_NULL)
{
}

template<RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassProduceDynamicDensityMap(deUint32 viewMask) const
{
	DE_ASSERT(m_testParams.dynamicDensityMap);

	std::vector<AttachmentDesc> attachmentDescriptions
	{
		{
			DE_NULL,															// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			m_testParams.densityMapFormat,										// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT					// VkImageLayout					finalLayout
		}
	};

	std::vector<AttachmentRef> colorAttachmentRefs
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	std::vector<SubpassDesc> subpassDescriptions
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,										// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,									// VkPipelineBindPoint				pipelineBindPoint
			viewMask,															// deUint32							viewMask
			0u,																	// deUint32							inputAttachmentCount
			DE_NULL,															// const VkAttachmentReference*		pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs.size()),					// deUint32							colorAttachmentCount
			colorAttachmentRefs.data(),											// const VkAttachmentReference*		pColorAttachments
			DE_NULL,															// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,															// const VkAttachmentReference*		pDepthStencilAttachment
			0u,																	// deUint32							preserveAttachmentCount
			DE_NULL																// const deUint32*					pPreserveAttachments
		}
	};

	std::vector<SubpassDep> subpassDependencies
	{
		{
			DE_NULL,															// const void*				pNext
			0u,																	// uint32_t					srcSubpass
			VK_SUBPASS_EXTERNAL,												// uint32_t					dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,					// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,								// VkAccessFlags			srcAccessMask
			VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,						// VkAccessFlags			dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT,										// VkDependencyFlags		dependencyFlags
			0u																	// deInt32					viewOffset
		}
	};

	const RenderPassCreateInfo renderPassInfo(
		DE_NULL,																// const void*						pNext
		(VkRenderPassCreateFlags)0,												// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),					// deUint32							attachmentCount
		attachmentDescriptions.data(),											// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),						// deUint32							subpassCount
		subpassDescriptions.data(),												// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),						// deUint32							dependencyCount
		subpassDependencies.empty() ? DE_NULL : subpassDependencies.data(),		// const VkSubpassDependency*		pDependencies
		0u,																		// deUint32							correlatedViewMaskCount
		DE_NULL																	// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template<RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassProduceSubsampledImage(deUint32	viewMask,
																								 bool		makeCopySubpass,
																								 bool		resampleSubsampled) const
{
	const void* constNullPtr				= DE_NULL;
	deUint32	multisampleAttachmentIndex	= 0;
	deUint32	copyAttachmentIndex			= 0;
	deUint32	densityMapAttachmentIndex	= 0;

	// add color image
	VkAttachmentLoadOp			loadOp = resampleSubsampled ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
	std::vector<AttachmentDesc> attachmentDescriptions
	{
		// Output color attachment
		{
			DE_NULL,															// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,											// VkFormat							format
			m_testParams.colorSamples,											// VkSampleCountFlagBits			samples
			loadOp,																// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL							// VkImageLayout					finalLayout
		}
	};

	// add resolve image when we use more than one sample per fragment
	if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
	{
		multisampleAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());
		attachmentDescriptions.emplace_back(
			constNullPtr,														// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,											// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL							// VkImageLayout					finalLayout
		);
	}

	// add color image copy ( when render_copy is used )
	if (makeCopySubpass)
	{
		copyAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());
		attachmentDescriptions.emplace_back(
			constNullPtr,														// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,											// VkFormat							format
			m_testParams.colorSamples,											// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL							// VkImageLayout					finalLayout
		);
	}

	// add density map
	densityMapAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());
	attachmentDescriptions.emplace_back(
		constNullPtr,															// const void*						pNext
		(VkAttachmentDescriptionFlags)0,										// VkAttachmentDescriptionFlags		flags
		m_testParams.densityMapFormat,											// VkFormat							format
		VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			samples
		VK_ATTACHMENT_LOAD_OP_LOAD,												// VkAttachmentLoadOp				loadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,										// VkAttachmentStoreOp				storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,										// VkAttachmentLoadOp				stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,										// VkAttachmentStoreOp				stencilStoreOp
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,						// VkImageLayout					initialLayout
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT						// VkImageLayout					finalLayout
	);

	std::vector<AttachmentRef> colorAttachmentRefs0
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	// for multisampled scenario we need to add resolve attachment
	// (for makeCopy scenario it is used in second subpass)
	AttachmentRef*	pResolveAttachments		= DE_NULL;
	AttachmentRef	resolveAttachmentRef
	{
		DE_NULL,
		multisampleAttachmentIndex,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT
	};
	if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
		pResolveAttachments = &resolveAttachmentRef;

	std::vector<SubpassDesc> subpassDescriptions
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint			pipelineBindPoint
			viewMask,												// deUint32						viewMask
			0u,														// deUint32						inputAttachmentCount
			DE_NULL,												// const VkAttachmentReference*	pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs0.size()),		// deUint32						colorAttachmentCount
			colorAttachmentRefs0.data(),							// const VkAttachmentReference*	pColorAttachments
			makeCopySubpass ? DE_NULL : pResolveAttachments,		// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,												// const VkAttachmentReference*	pDepthStencilAttachment
			0u,														// deUint32						preserveAttachmentCount
			DE_NULL													// const deUint32*				pPreserveAttachments
		}
	};

	std::vector<AttachmentRef>	inputAttachmentRefs1
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};
	std::vector<AttachmentRef>	colorAttachmentRefs1
	{
		{ DE_NULL, copyAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};
	std::vector<SubpassDep>		subpassDependencies;

	if (makeCopySubpass)
	{
		subpassDescriptions.push_back({
			DE_NULL,
			(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint			pipelineBindPoint
			viewMask,												// deUint32						viewMask
			static_cast<deUint32>(inputAttachmentRefs1.size()),		// deUint32						inputAttachmentCount
			inputAttachmentRefs1.data(),							// const VkAttachmentReference*	pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs1.size()),		// deUint32						colorAttachmentCount
			colorAttachmentRefs1.data(),							// const VkAttachmentReference*	pColorAttachments
			pResolveAttachments,									// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,												// const VkAttachmentReference*	pDepthStencilAttachment
			0u,														// deUint32						preserveAttachmentCount
			DE_NULL													// const deUint32*				pPreserveAttachments
		});

		VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		if (m_testParams.viewCount > 1)
			dependencyFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;

		subpassDependencies.emplace_back(
			constNullPtr,																// const void*				pNext
			0u,																			// uint32_t					srcSubpass
			1u,																			// uint32_t					dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,								// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,										// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,										// VkAccessFlags			srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,										// VkAccessFlags			dstAccessMask
			dependencyFlags,															// VkDependencyFlags		dependencyFlags
			0u																			// deInt32					viewOffset
		);
	}

	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	// for coarse reconstruction we need to put barrier on vertex stage
	if (m_testParams.coarseReconstruction)
		dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

	subpassDependencies.emplace_back(
		constNullPtr,																	// const void*				pNext
		static_cast<deUint32>(subpassDescriptions.size())-1u,							// uint32_t					srcSubpass
		VK_SUBPASS_EXTERNAL,															// uint32_t					dstSubpass
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,									// VkPipelineStageFlags		srcStageMask
		dstStageMask,																	// VkPipelineStageFlags		dstStageMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,											// VkAccessFlags			srcAccessMask
		VK_ACCESS_SHADER_READ_BIT,														// VkAccessFlags			dstAccessMask
		VK_DEPENDENCY_BY_REGION_BIT,													// VkDependencyFlags		dependencyFlags
		0u																				// deInt32					viewOffset
	);

	VkRenderPassFragmentDensityMapCreateInfoEXT renderPassFragmentDensityMap
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
		DE_NULL,
		{ densityMapAttachmentIndex, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT }
	};

	void* renderPassInfoPNext = (void*)&renderPassFragmentDensityMap;

	const RenderPassCreateInfo	renderPassInfo(
		renderPassInfoPNext,														// const void*						pNext
		(VkRenderPassCreateFlags)0,													// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),						// deUint32							attachmentCount
		attachmentDescriptions.data(),												// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),							// deUint32							subpassCount
		subpassDescriptions.data(),													// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),							// deUint32							dependencyCount
		subpassDependencies.data(),													// const VkSubpassDependency*		pDependencies
		0u,																			// deUint32							correlatedViewMaskCount
		DE_NULL																		// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template<RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassOutputSubsampledImage() const
{
	// copy subsampled image to ordinary image - you cannot retrieve subsampled image to CPU in any way.
	// You must first convert it into plain image through rendering
	std::vector<AttachmentDesc> attachmentDescriptions
	{
		// output attachment
		{
			DE_NULL,											// const void*						pNext
			(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,							// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout
		},
	};

	std::vector<AttachmentRef> colorAttachmentRefs
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	std::vector<SubpassDesc> subpassDescriptions
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint
			0u,													// deUint32							viewMask
			0u,													// deUint32							inputAttachmentCount
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs.size()),	// deUint32							colorAttachmentCount
			colorAttachmentRefs.data(),							// const VkAttachmentReference*		pColorAttachments
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment
			0u,													// deUint32							preserveAttachmentCount
			DE_NULL												// const deUint32*					pPreserveAttachments
		}
	};

	const RenderPassCreateInfo	renderPassInfo(
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),		// deUint32							subpassCount
		subpassDescriptions.data(),								// const VkSubpassDescription*		pSubpasses
		0,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template<RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdBeginRenderPass(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin) const
{
	RenderpassSubpass::cmdBeginRenderPass(m_vk, cmdBuffer, pRenderPassBegin, &m_subpassBeginInfo);
}

template<RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdNextSubpass(VkCommandBuffer cmdBuffer) const
{
	RenderpassSubpass::cmdNextSubpass(m_vk, cmdBuffer, &m_subpassBeginInfo, &m_subpassEndInfo);
}

template<RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdEndRenderPass(VkCommandBuffer cmdBuffer) const
{
	RenderpassSubpass::cmdEndRenderPass(m_vk, cmdBuffer, &m_subpassEndInfo);
}

Move<VkFramebuffer> createImagelessFrameBuffer(const DeviceInterface& vk, VkDevice vkDevice, VkRenderPass renderPass, VkExtent3D size, const std::vector<VkFramebufferAttachmentImageInfo>& attachmentInfo)
{
	const deUint32 attachmentCount = static_cast<deUint32>(attachmentInfo.size());
	const VkFramebufferAttachmentsCreateInfo framebufferAttachmentsCreateInfo
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		attachmentCount,											// deUint32										attachmentImageInfoCount;
		&attachmentInfo[0]											// const VkFramebufferAttachmentImageInfo*		pAttachmentImageInfos;
	};

	const VkFramebufferCreateInfo framebufferParams
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,					// VkStructureType			sType;
		&framebufferAttachmentsCreateInfo,							// const void*				pNext;
		VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,						// VkFramebufferCreateFlags	flags;
		renderPass,													// VkRenderPass				renderPass;
		attachmentCount,											// deUint32					attachmentCount;
		DE_NULL,													// const VkImageView*		pAttachments;
		size.width,													// deUint32					width;
		size.height,												// deUint32					height;
		1u															// deUint32					layers;
	};

	return createFramebuffer(vk, vkDevice, &framebufferParams);
}

Move<VkFramebuffer> createFrameBuffer(const DeviceInterface& vk, VkDevice vkDevice, VkRenderPass renderPass, VkExtent3D size, const std::vector<VkImageView>& imageViews)
{
	return makeFramebuffer(vk, vkDevice, renderPass, static_cast<deUint32>(imageViews.size()), imageViews.data(), size.width, size.height);
}

void copyBufferToImage(const DeviceInterface&					vk,
					   VkDevice									device,
					   VkQueue									queue,
					   deUint32									queueFamilyIndex,
					   const VkBuffer&							buffer,
					   VkDeviceSize								bufferSize,
					   const VkExtent3D&						imageSize,
					   deUint32									arrayLayers,
					   VkImage									destImage)
{
	Move<VkCommandPool>		cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkFence>			fence					= createFence(vk, device);
	VkImageLayout			destImageLayout			= VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
	VkPipelineStageFlags	destImageDstStageFlags	= VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
	VkAccessFlags			finalAccessMask			= VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,		// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const VkBufferImageCopy copyRegion =
	{
		0,													// VkDeviceSize					bufferOffset
		0,													// deUint32						bufferRowLength
		0,													// deUint32						bufferImageHeight
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, arrayLayers },	// VkImageSubresourceLayers		imageSubresource
		{ 0, 0, 0 },										// VkOffset3D					imageOffset
		imageSize											// VkExtent3D					imageExtent
	};

	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier preBufferBarrier = makeBufferMemoryBarrier(
		VK_ACCESS_HOST_WRITE_BIT,							// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,						// VkAccessFlags	dstAccessMask;
		buffer,												// VkBuffer			buffer;
		0u,													// VkDeviceSize		offset;
		bufferSize											// VkDeviceSize		size;
	);

	const VkImageSubresourceRange subresourceRange
	{														// VkImageSubresourceRange	subresourceRange;
		VK_IMAGE_ASPECT_COLOR_BIT,							// VkImageAspectFlags		aspect;
		0u,													// deUint32					baseMipLevel;
		1u,													// deUint32					mipLevels;
		0u,													// deUint32					baseArraySlice;
		arrayLayers											// deUint32					arraySize;
	};

	const VkImageMemoryBarrier preImageBarrier = makeImageMemoryBarrier(
		0u,													// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout			newLayout;
		destImage,											// VkImage					image;
		subresourceRange									// VkImageSubresourceRange	subresourceRange;
	);

	const VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags			srcAccessMask;
		finalAccessMask,									// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout			oldLayout;
		destImageLayout,									// VkImageLayout			newLayout;
		destImage,											// VkImage					image;
		subresourceRange									// VkImageSubresourceRange	subresourceRange;
	);

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destImageDstStageFlags, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		DE_NULL,						// const void*					pNext;
		0u,								// deUint32						waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*			pWaitSemaphores;
		&pipelineStageFlags,			// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*		pCommandBuffers;
		0u,								// deUint32						signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*			pSignalSemaphores;
	};

	try
	{
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(device, 1, &fence.get(), true, ~(0ull) /* infinity */));
	}
	catch (...)
	{
		VK_CHECK(vk.deviceWaitIdle(device));
		throw;
	}
}

Move<VkPipeline> buildGraphicsPipeline(const DeviceInterface&						vk,
										const VkDevice								device,
										const VkPipelineLayout						pipelineLayout,
										const VkShaderModule						vertexShaderModule,
										const VkShaderModule						fragmentShaderModule,
										const VkRenderPass							renderPass,
										const std::vector<VkViewport>&				viewportVect,
										const std::vector<VkRect2D>&				scissorVect,
										const deUint32								subpass,
										const VkPipelineMultisampleStateCreateInfo*	multisampleStateCreateInfo,
										const void*									pNext,
										const bool									useDensityMapAttachment,
										const bool									useMaintenance5 = false)
{
	std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageParams(2,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkPipelineShaderStageCreateFlags		flags
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage
			vertexShaderModule,										// VkShaderModule						module
			"main",													// const char*							pName
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo
		});
	pipelineShaderStageParams[1].stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageParams[1].module	= fragmentShaderModule;

	const VkVertexInputBindingDescription vertexInputBindingDescription
	{
		0u,																// deUint32					binding;
		sizeof(Vertex4RGBA),											// deUint32					strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX										// VkVertexInputStepRate	inputRate;
	};

	std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions
	{
		{ 0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
		{ 1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 4) },
		{ 2u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 8) }
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		static_cast<deUint32>(vertexInputAttributeDescriptions.size()),	// deUint32									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions.data()							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		0u,																// VkPipelineInputAssemblyStateCreateFlags	flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology
		VK_FALSE														// VkBool32									primitiveRestartEnable
	};

	const VkPipelineViewportStateCreateInfo viewportStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags		flags
		(deUint32)viewportVect.size(),									// deUint32									viewportCount
		viewportVect.data(),											// const VkViewport*						pViewports
		(deUint32)scissorVect.size(),									// deUint32									scissorCount
		scissorVect.data()												// const VkRect2D*							pScissors
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		0u,																// VkPipelineRasterizationStateCreateFlags	flags
		VK_FALSE,														// VkBool32									depthClampEnable
		VK_FALSE,														// VkBool32									rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace
		VK_FALSE,														// VkBool32									depthBiasEnable
		0.0f,															// float									depthBiasConstantFactor
		0.0f,															// float									depthBiasClamp
		0.0f,															// float									depthBiasSlopeFactor
		1.0f															// float									lineWidth
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		0u,																// VkPipelineMultisampleStateCreateFlags	flags
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits					rasterizationSamples
		VK_FALSE,														// VkBool32									sampleShadingEnable
		1.0f,															// float									minSampleShading
		DE_NULL,														// const VkSampleMask*						pSampleMask
		VK_FALSE,														// VkBool32									alphaToCoverageEnable
		VK_FALSE														// VkBool32									alphaToOneEnable
	};

	const VkStencilOpState stencilOpState
	{
		VK_STENCIL_OP_KEEP,												// VkStencilOp		failOp
		VK_STENCIL_OP_KEEP,												// VkStencilOp		passOp
		VK_STENCIL_OP_KEEP,												// VkStencilOp		depthFailOp
		VK_COMPARE_OP_NEVER,											// VkCompareOp		compareOp
		0,																// deUint32			compareMask
		0,																// deUint32			writeMask
		0																// deUint32			reference
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType							sType
		DE_NULL,														// const void*								pNext
		0u,																// VkPipelineDepthStencilStateCreateFlags	flags
		VK_FALSE,														// VkBool32									depthTestEnable
		VK_FALSE,														// VkBool32									depthWriteEnable
		VK_COMPARE_OP_LESS_OR_EQUAL,									// VkCompareOp								depthCompareOp
		VK_FALSE,														// VkBool32									depthBoundsTestEnable
		VK_FALSE,														// VkBool32									stencilTestEnable
		stencilOpState,													// VkStencilOpState							front
		stencilOpState,													// VkStencilOpState							back
		0.0f,															// float									minDepthBounds
		1.0f,															// float									maxDepthBounds
	};

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState
	{
		VK_FALSE,														// VkBool32					blendEnable
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			dstColorBlendFactor
		VK_BLEND_OP_ADD,												// VkBlendOp				colorBlendOp
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor			dstAlphaBlendFactor
		VK_BLEND_OP_ADD,												// VkBlendOp				alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT										// VkColorComponentFlags	colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineColorBlendStateCreateFlags			flags
		VK_FALSE,														// VkBool32										logicOpEnable
		VK_LOGIC_OP_CLEAR,												// VkLogicOp									logicOp
		1u,																// deUint32										attachmentCount
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float										blendConstants[4]
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,													// VkStructureType									sType
		pNext,																								// const void*										pNext
		(useDensityMapAttachment ?
			deUint32(VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT) :
			0u),																							// VkPipelineCreateFlags							flags
		(deUint32)pipelineShaderStageParams.size(),															// deUint32											stageCount
		&pipelineShaderStageParams[0],																		// const VkPipelineShaderStageCreateInfo*			pStages
		&vertexInputStateCreateInfo,																		// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState
		&inputAssemblyStateCreateInfo,																		// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState
		DE_NULL,																							// const VkPipelineTessellationStateCreateInfo*		pTessellationState
		&viewportStateCreateInfo,																			// const VkPipelineViewportStateCreateInfo*			pViewportState
		&rasterizationStateCreateInfoDefault,																// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState
		multisampleStateCreateInfo ? multisampleStateCreateInfo: &multisampleStateCreateInfoDefault,		// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState
		&depthStencilStateCreateInfoDefault,																// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState
		&colorBlendStateCreateInfoDefault,																	// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState
		DE_NULL,																							// const VkPipelineDynamicStateCreateInfo*			pDynamicState
		pipelineLayout,																						// VkPipelineLayout									layout
		renderPass,																							// VkRenderPass										renderPass
		subpass,																							// deUint32											subpass
		DE_NULL,																							// VkPipeline										basePipelineHandle
		0																									// deInt32											basePipelineIndex;
	};

	VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo {};
	if (useDensityMapAttachment && useMaintenance5)
	{
		pipelineFlags2CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR;
		pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
		pipelineFlags2CreateInfo.pNext = pipelineCreateInfo.pNext;
		pipelineCreateInfo.pNext = &pipelineFlags2CreateInfo;
		pipelineCreateInfo.flags = 0;
	}

	return createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

class FragmentDensityMapTest : public vkt::TestCase
{
public:
							FragmentDensityMapTest	(tcu::TestContext&	testContext,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams&	testParams);
	virtual void			initPrograms			(SourceCollections&	sourceCollections) const;
	virtual TestInstance*	createInstance			(Context&			context) const;
	virtual void			checkSupport			(Context&			context) const;

private:
	const TestParams		m_testParams;
};

class FragmentDensityMapTestInstance : public vkt::TestInstance
{
public:
									FragmentDensityMapTestInstance			(Context&			context,
																			 const TestParams&	testParams);
	virtual tcu::TestStatus			iterate									(void);

private:

	typedef std::shared_ptr<RenderPassWrapperBase> RenderPassWrapperBasePtr;

	void							drawDynamicDensityMap					(VkCommandBuffer cmdBuffer);
	void							drawSubsampledImage						(VkCommandBuffer cmdBuffer);
	void							drawResampleSubsampledImage				(VkCommandBuffer cmdBuffer);
	void							drawCopySubsampledImage					(VkCommandBuffer cmdBuffer);
	void							createCommandBufferForRenderpass		(RenderPassWrapperBasePtr	renderPassWrapper,
																			 const VkExtent3D&			colorImageSize,
																			 const VkRect2D&			dynamicDensityMapRenderArea,
																			 const VkRect2D&			colorImageRenderArea,
																			 const VkRect2D&			outputRenderArea);
	void							createCommandBufferForDynamicRendering	(const VkRect2D&			dynamicDensityMapRenderArea,
																			 const VkRect2D&			colorImageRenderArea,
																			 const VkRect2D&			outputRenderArea,
																			 const VkDevice&			vkDevice);
	tcu::TestStatus					verifyImage								(void);

private:

	typedef de::SharedPtr<Unique<VkSampler> >	VkSamplerSp;
	typedef de::SharedPtr<Unique<VkImage> >		VkImageSp;
	typedef de::SharedPtr<Allocation>			AllocationSp;
	typedef de::SharedPtr<Unique<VkImageView> >	VkImageViewSp;

	TestParams						m_testParams;
	tcu::UVec2						m_renderSize;
	tcu::Vec2						m_densityValue;
	deUint32						m_viewMask;

	Move<VkCommandPool>				m_cmdPool;

	std::vector<VkImageSp>			m_densityMapImages;
	std::vector<AllocationSp>		m_densityMapImageAllocs;
	std::vector<VkImageViewSp>		m_densityMapImageViews;

	Move<VkImage>					m_colorImage;
	de::MovePtr<Allocation>			m_colorImageAlloc;
	Move<VkImageView>				m_colorImageView;

	Move<VkImage>					m_colorCopyImage;
	de::MovePtr<Allocation>			m_colorCopyImageAlloc;
	Move<VkImageView>				m_colorCopyImageView;

	Move<VkImage>					m_colorResolvedImage;
	de::MovePtr<Allocation>			m_colorResolvedImageAlloc;
	Move<VkImageView>				m_colorResolvedImageView;

	Move<VkImage>					m_outputImage;
	de::MovePtr<Allocation>			m_outputImageAlloc;
	Move<VkImageView>				m_outputImageView;

	std::vector<VkSamplerSp>		m_colorSamplers;

	Move<VkRenderPass>				m_renderPassProduceDynamicDensityMap;
	Move<VkRenderPass>				m_renderPassProduceSubsampledImage;
	Move<VkRenderPass>				m_renderPassUpdateSubsampledImage;
	Move<VkRenderPass>				m_renderPassOutputSubsampledImage;
	Move<VkFramebuffer>				m_framebufferProduceDynamicDensityMap;
	Move<VkFramebuffer>				m_framebufferProduceSubsampledImage;
	Move<VkFramebuffer>				m_framebufferUpdateSubsampledImage;
	Move<VkFramebuffer>				m_framebufferOutputSubsampledImage;

	Move<VkDescriptorSetLayout>		m_descriptorSetLayoutProduceSubsampled;

	Move<VkDescriptorSetLayout>		m_descriptorSetLayoutOperateOnSubsampledImage;
	Move<VkDescriptorPool>			m_descriptorPoolOperateOnSubsampledImage;
	Move<VkDescriptorSet>			m_descriptorSetOperateOnSubsampledImage;

	Move<VkDescriptorSetLayout>		m_descriptorSetLayoutOutputSubsampledImage;
	Move<VkDescriptorPool>			m_descriptorPoolOutputSubsampledImage;
	Move<VkDescriptorSet>			m_descriptorSetOutputSubsampledImage;

	Move<VkShaderModule>			m_vertexCommonShaderModule;
	Move<VkShaderModule>			m_fragmentShaderModuleProduceSubsampledImage;
	Move<VkShaderModule>			m_fragmentShaderModuleCopySubsampledImage;
	Move<VkShaderModule>			m_fragmentShaderModuleUpdateSubsampledImage;
	Move<VkShaderModule>			m_fragmentShaderModuleOutputSubsampledImage;

	std::vector<Vertex4RGBA>		m_verticesDDM;
	Move<VkBuffer>					m_vertexBufferDDM;
	de::MovePtr<Allocation>			m_vertexBufferAllocDDM;

	std::vector<Vertex4RGBA>		m_vertices;
	Move<VkBuffer>					m_vertexBuffer;
	de::MovePtr<Allocation>			m_vertexBufferAlloc;

	std::vector<Vertex4RGBA>		m_verticesOutput;
	Move<VkBuffer>					m_vertexBufferOutput;
	de::MovePtr<Allocation>			m_vertexBufferOutputAlloc;

	Move<VkPipelineLayout>			m_pipelineLayoutNoDescriptors;
	Move<VkPipelineLayout>			m_pipelineLayoutOperateOnSubsampledImage;
	Move<VkPipelineLayout>			m_pipelineLayoutOutputSubsampledImage;
	Move<VkPipeline>				m_graphicsPipelineProduceDynamicDensityMap;
	Move<VkPipeline>				m_graphicsPipelineProduceSubsampledImage;
	Move<VkPipeline>				m_graphicsPipelineCopySubsampledImage;
	Move<VkPipeline>				m_graphicsPipelineUpdateSubsampledImage;
	Move<VkPipeline>				m_graphicsPipelineOutputSubsampledImage;

	Move<VkCommandBuffer>			m_cmdBuffer;
	Move<VkCommandBuffer>			m_dynamicDensityMapSecCmdBuffer;
	Move<VkCommandBuffer>			m_subsampledImageSecCmdBuffer;
	Move<VkCommandBuffer>			m_resampleSubsampledImageSecCmdBuffer;
	Move<VkCommandBuffer>			m_copySubsampledImageSecCmdBuffer;
};

FragmentDensityMapTest::FragmentDensityMapTest (tcu::TestContext&	testContext,
												const std::string&	name,
												const std::string&	description,
												const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, description)
	, m_testParams	(testParams)
{
	DE_ASSERT(testParams.samplersCount > 0);
}

void FragmentDensityMapTest::initPrograms(SourceCollections& sourceCollections) const
{
	const std::string vertSourceTemplate(
		"#version 450\n"
		"#extension GL_EXT_multiview : enable\n"
		"${EXTENSIONS}"
		"layout(location = 0) in  vec4 inPosition;\n"
		"layout(location = 1) in  vec4 inUV;\n"
		"layout(location = 2) in  vec4 inColor;\n"
		"layout(location = 0) out vec4 outUV;\n"
		"layout(location = 1) out vec4 outColor;\n"
		"out gl_PerVertex\n"
		"{\n"
		"  vec4 gl_Position;\n"
		"};\n"
		"void main(void)\n"
		"{\n"
		"	gl_Position = inPosition;\n"
		"	outUV = inUV;\n"
		"	outColor = inColor;\n"
		"	${OPERATION}"
		"}\n");

	std::map<std::string, std::string> parameters
	{
		{"EXTENSIONS", ""},
		{"OPERATION", ""}
	};
	if (m_testParams.multiViewport)
	{
		parameters["EXTENSIONS"] = "#extension GL_ARB_shader_viewport_layer_array : enable\n";
		parameters["OPERATION"] = "gl_ViewportIndex = gl_ViewIndex;\n";
	}
	sourceCollections.glslSources.add("vert") << glu::VertexSource(tcu::StringTemplate(vertSourceTemplate).specialize(parameters));

	sourceCollections.glslSources.add("frag_produce_subsampled") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_fragment_invocation_density : enable\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
		"}\n"
	);

	sourceCollections.glslSources.add("frag_update_subsampled") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_fragment_invocation_density : enable\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	if (gl_FragCoord.y < 0.5)\n"
		"		discard;\n"
		"	fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
		"}\n"
	);

	sourceCollections.glslSources.add("frag_copy_subsampled") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_fragment_invocation_density : enable\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputAtt;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = subpassLoad(inputAtt);\n"
		"}\n"
	);

	sourceCollections.glslSources.add("frag_copy_subsampled_ms") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_fragment_invocation_density : enable\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInputMS inputAtt;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = subpassLoad(inputAtt, gl_SampleID);\n"
		"}\n"
	);

	const char* samplersDefTemplate =
		"layout(binding = ${BINDING})  uniform ${SAMPLER} subsampledImage${BINDING};\n";
	const char* sumColorsTemplate =
		"	fragColor += texture(subsampledImage${BINDING}, inUV.${COMPONENTS});\n";

	const char* densitymapOutputTemplate =
		"#version 450\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"${SAMPLERS_DEF}"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = vec4(0);\n"
		"${SUM_COLORS}"
		"	fragColor /= float(${COUNT});\n"
		"}\n";

	parameters =
	{
		{ "SAMPLER",		"" },
		{ "BINDING",		"" },
		{ "COMPONENTS",		"" },
		{ "COUNT",			std::to_string(m_testParams.samplersCount) },
		{ "SAMPLERS_DEF",	"" },
		{ "SUM_COLORS",		"" },
	};

	std::string sampler2dDefs;
	std::string sampler2dSumColors;
	std::string sampler2dArrayDefs;
	std::string sampler2dArraySumColors;
	for (deUint32 samplerIndex = 0; samplerIndex < m_testParams.samplersCount; ++samplerIndex)
	{
		parameters["BINDING"]		 = std::to_string(samplerIndex);

		parameters["COMPONENTS"]	 = "xy";
		parameters["SAMPLER"]		 = "sampler2D";
		sampler2dDefs				+= tcu::StringTemplate(samplersDefTemplate).specialize(parameters);
		sampler2dSumColors			+= tcu::StringTemplate(sumColorsTemplate).specialize(parameters);

		parameters["COMPONENTS"]	 = "xyz";
		parameters["SAMPLER"]		 = "sampler2DArray";
		sampler2dArrayDefs			+= tcu::StringTemplate(samplersDefTemplate).specialize(parameters);
		sampler2dArraySumColors		+= tcu::StringTemplate(sumColorsTemplate).specialize(parameters);
	}

	parameters["SAMPLERS_DEF"]	= sampler2dDefs;
	parameters["SUM_COLORS"]	= sampler2dSumColors;
	sourceCollections.glslSources.add("frag_output_2d")
		<< glu::FragmentSource(tcu::StringTemplate(densitymapOutputTemplate).specialize(parameters));

	parameters["SAMPLERS_DEF"]	= sampler2dArrayDefs;
	parameters["SUM_COLORS"]	= sampler2dArraySumColors;
	sourceCollections.glslSources.add("frag_output_2darray")
		<< glu::FragmentSource(tcu::StringTemplate(densitymapOutputTemplate).specialize(parameters));
}

TestInstance* FragmentDensityMapTest::createInstance(Context& context) const
{
	return new FragmentDensityMapTestInstance(context, m_testParams);
}

void FragmentDensityMapTest::checkSupport(Context& context) const
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const VkPhysicalDevice		vkPhysicalDevice	= context.getPhysicalDevice();

	context.requireDeviceFunctionality("VK_EXT_fragment_density_map");

	if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (m_testParams.imagelessFramebuffer)
		context.requireDeviceFunctionality("VK_KHR_imageless_framebuffer");

	if (m_testParams.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	VkPhysicalDeviceFragmentDensityMapFeaturesEXT		fragmentDensityMapFeatures		= initVulkanStructure();
	VkPhysicalDeviceFragmentDensityMap2FeaturesEXT		fragmentDensityMap2Features		= initVulkanStructure(&fragmentDensityMapFeatures);
	VkPhysicalDeviceFeatures2KHR						features2						= initVulkanStructure(&fragmentDensityMap2Features);

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	const auto& fragmentDensityMap2Properties	= context.getFragmentDensityMap2PropertiesEXT();

	if (!fragmentDensityMapFeatures.fragmentDensityMap)
		TCU_THROW(NotSupportedError, "fragmentDensityMap feature is not supported");
	if (m_testParams.dynamicDensityMap && !fragmentDensityMapFeatures.fragmentDensityMapDynamic)
		TCU_THROW(NotSupportedError, "fragmentDensityMapDynamic feature is not supported");
	if (m_testParams.nonSubsampledImages && !fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages)
		TCU_THROW(NotSupportedError, "fragmentDensityMapNonSubsampledImages feature is not supported");

	if (m_testParams.deferredDensityMap)
	{
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
		if (!fragmentDensityMap2Features.fragmentDensityMapDeferred)
			TCU_THROW(NotSupportedError, "fragmentDensityMapDeferred feature is not supported");
	}
	if (m_testParams.subsampledLoads)
	{
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
		if (!fragmentDensityMap2Properties.subsampledLoads)
			TCU_THROW(NotSupportedError, "subsampledLoads property is not supported");
	}
	if (m_testParams.coarseReconstruction)
	{
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
		if (!fragmentDensityMap2Properties.subsampledCoarseReconstructionEarlyAccess)
			TCU_THROW(NotSupportedError, "subsampledCoarseReconstructionEarlyAccess property is not supported");
	}

	if (m_testParams.viewCount > 1)
	{
		context.requireDeviceFunctionality("VK_KHR_multiview");
		if (!context.getMultiviewFeatures().multiview)
			TCU_THROW(NotSupportedError, "Implementation does not support multiview feature");

		if (m_testParams.viewCount > 2)
		{
			context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
			if (m_testParams.viewCount > fragmentDensityMap2Properties.maxSubsampledArrayLayers)
				TCU_THROW(NotSupportedError, "Maximum number of VkImageView array layers for usages supporting subsampled samplers is to small");
		}
	}

	if (m_testParams.multiViewport)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
		if (!context.getDeviceFeatures().multiViewport)
			TCU_THROW(NotSupportedError, "multiViewport not supported");
	}

	if (!m_testParams.nonSubsampledImages && (m_testParams.samplersCount > 1))
	{
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
		if (m_testParams.samplersCount > fragmentDensityMap2Properties.maxDescriptorSetSubsampledSamplers)
			TCU_THROW(NotSupportedError, "Required number of subsampled samplers is not supported");
	}

	vk::VkImageUsageFlags	colorImageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if (m_testParams.makeCopy)
		colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	deUint32				colorImageCreateFlags	= m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
	VkImageFormatProperties	imageFormatProperties	(getPhysicalDeviceImageFormatProperties(vki, vkPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, colorImageCreateFlags));

	if ((imageFormatProperties.sampleCounts & m_testParams.colorSamples) == 0)
		TCU_THROW(NotSupportedError, "Color image type not supported");

	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().multisampleArrayImage &&
		(m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT) && (m_testParams.viewCount != 1))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Implementation does not support image array with multiple samples per texel");
	}
}

FragmentDensityMapTestInstance::FragmentDensityMapTestInstance(Context&				context,
															   const TestParams&	testParams)
	: vkt::TestInstance	(context)
	, m_testParams		(testParams)
{
	m_renderSize		= tcu::UVec2(deFloorFloatToInt32(m_testParams.renderMultiplier * static_cast<float>(m_testParams.densityMapSize.x())),
									 deFloorFloatToInt32(m_testParams.renderMultiplier * static_cast<float>(m_testParams.densityMapSize.y())));
	m_densityValue		= tcu::Vec2(1.0f / static_cast<float>(m_testParams.fragmentArea.x()),
									1.0f / static_cast<float>(m_testParams.fragmentArea.y()));
	m_viewMask			= (m_testParams.viewCount > 1) ? ((1u << m_testParams.viewCount) - 1u) : 0u;

	const DeviceInterface&		vk							= m_context.getDeviceInterface();
	const VkDevice				vkDevice					= getDevice(m_context);
	const VkPhysicalDevice		vkPhysicalDevice			= m_context.getPhysicalDevice();
	const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue				queue						= getDeviceQueue(vk, vkDevice, queueFamilyIndex, 0);
	SimpleAllocator				memAlloc					(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), vkPhysicalDevice));
	const VkComponentMapping	componentMappingRGBA		= makeComponentMappingRGBA();
	RenderPassWrapperBasePtr	renderPassWrapper;

	// calculate all image sizes, image usage flags, view types etc.
	deUint32						densitiMapCount				= 1 + m_testParams.subsampledLoads;
	VkExtent3D						densityMapImageSize			{ m_testParams.densityMapSize.x(), m_testParams.densityMapSize.y(), 1 };
	deUint32						densityMapImageLayers		= m_testParams.viewCount;
	VkImageViewType					densityMapImageViewType		= (m_testParams.viewCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	vk::VkImageUsageFlags			densityMapImageUsage		= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageSubresourceRange	densityMapSubresourceRange	= { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, densityMapImageLayers };
	deUint32						densityMapImageViewFlags	= 0u;

	const VkFormat					colorImageFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	VkExtent3D						colorImageSize				{ m_renderSize.x() / m_testParams.viewCount, m_renderSize.y(), 1 };
	deUint32						colorImageLayers			= densityMapImageLayers;
	VkImageViewType					colorImageViewType			= densityMapImageViewType;
	vk::VkImageUsageFlags			colorImageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	deUint32						colorImageCreateFlags		= m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
	const VkImageSubresourceRange	colorSubresourceRange		= { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers };
	bool							isColorImageMultisampled	= m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
	bool							isDynamicRendering			= m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING;

	VkExtent3D						outputImageSize				{ m_renderSize.x(), m_renderSize.y(), 1 };
	const VkImageSubresourceRange	outputSubresourceRange		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };

	if (m_testParams.dynamicDensityMap)
	{
		DE_ASSERT(!m_testParams.subsampledLoads);

		densityMapImageUsage		= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		densityMapImageViewFlags	= (deUint32)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT;
	}
	else if (m_testParams.deferredDensityMap)
		densityMapImageViewFlags	= (deUint32)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DEFERRED_BIT_EXT;
	if (m_testParams.makeCopy)
		colorImageUsage				|= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	// Create subsampled color image
	prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat,
		colorImageSize, colorImageLayers, m_testParams.colorSamples,
		colorImageUsage, queueFamilyIndex, 0u, colorImageViewType,
		componentMappingRGBA, colorSubresourceRange, m_colorImage, m_colorImageAlloc, m_colorImageView);

	// Create subsampled color image for resolve operation ( when multisampling is used )
	if (isColorImageMultisampled)
	{
		prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat,
			colorImageSize, colorImageLayers, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u, colorImageViewType,
			componentMappingRGBA, colorSubresourceRange, m_colorResolvedImage, m_colorResolvedImageAlloc, m_colorResolvedImageView);
	}

	// Create subsampled image copy
	if (m_testParams.makeCopy)
	{
		prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat,
			colorImageSize, colorImageLayers, m_testParams.colorSamples,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u, colorImageViewType,
			componentMappingRGBA, colorSubresourceRange, m_colorCopyImage, m_colorCopyImageAlloc, m_colorCopyImageView);
	}

	// Create output image ( data from subsampled color image will be copied into it using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT )
	prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, colorImageFormat,
		outputImageSize, 1u, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, queueFamilyIndex, 0u, VK_IMAGE_VIEW_TYPE_2D,
		componentMappingRGBA, outputSubresourceRange, m_outputImage, m_outputImageAlloc, m_outputImageView);

	// Create density map image/images
	for (deUint32 mapIndex = 0; mapIndex < densitiMapCount; ++mapIndex)
	{
		Move<VkImage>			densityMapImage;
		de::MovePtr<Allocation>	densityMapImageAlloc;
		Move<VkImageView>		densityMapImageView;

		prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, m_testParams.densityMapFormat,
			densityMapImageSize, densityMapImageLayers, VK_SAMPLE_COUNT_1_BIT,
			densityMapImageUsage, queueFamilyIndex, densityMapImageViewFlags, densityMapImageViewType,
			componentMappingRGBA, densityMapSubresourceRange, densityMapImage, densityMapImageAlloc, densityMapImageView);

		m_densityMapImages.push_back(VkImageSp(new Unique<VkImage>(densityMapImage)));
		m_densityMapImageAllocs.push_back(AllocationSp(densityMapImageAlloc.release()));
		m_densityMapImageViews.push_back(VkImageViewSp(new Unique<VkImageView>(densityMapImageView)));
	}

	// Create and fill staging buffer, copy its data to density map image
	if (!m_testParams.dynamicDensityMap)
	{
		tcu::TextureFormat				densityMapTextureFormat = vk::mapVkFormat(m_testParams.densityMapFormat);
		VkDeviceSize					stagingBufferSize		= tcu::getPixelSize(densityMapTextureFormat) * densityMapImageSize.width * densityMapImageSize.height * densityMapImageLayers;
		const vk::VkBufferCreateInfo	stagingBufferCreateInfo
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			0u,									// flags
			stagingBufferSize,					// size
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,	// usage
			vk::VK_SHARING_MODE_EXCLUSIVE,		// sharingMode
			0u,									// queueFamilyCount
			DE_NULL,							// pQueueFamilyIndices
		};
		vk::Move<vk::VkBuffer>			stagingBuffer		= vk::createBuffer(vk, vkDevice, &stagingBufferCreateInfo);
		const vk::VkMemoryRequirements	stagingRequirements = vk::getBufferMemoryRequirements(vk, vkDevice, *stagingBuffer);
		de::MovePtr<vk::Allocation>		stagingAllocation	= memAlloc.allocate(stagingRequirements, MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *stagingBuffer, stagingAllocation->getMemory(), stagingAllocation->getOffset()));
		tcu::PixelBufferAccess			stagingBufferAccess	(densityMapTextureFormat, densityMapImageSize.width, densityMapImageSize.height, densityMapImageLayers, stagingAllocation->getHostPtr());
		tcu::Vec4						fragmentArea		(m_densityValue.x(), m_densityValue.y(), 0.0f, 1.0f);

		for (deUint32 mapIndex = 0; mapIndex < densitiMapCount; ++mapIndex)
		{
			// Fill staging buffer with one color
			tcu::clear(stagingBufferAccess, fragmentArea);
			flushAlloc(vk, vkDevice, *stagingAllocation);

			copyBufferToImage
			(
				vk, vkDevice, queue, queueFamilyIndex,
				*stagingBuffer, stagingBufferSize,
				densityMapImageSize, densityMapImageLayers, **m_densityMapImages[mapIndex]
			);

			std::swap(fragmentArea.m_data[0], fragmentArea.m_data[1]);
		}
	}

	deUint32 samplerCreateFlags = (deUint32)VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
	if (m_testParams.coarseReconstruction)
		samplerCreateFlags		|= (deUint32)VK_SAMPLER_CREATE_SUBSAMPLED_COARSE_RECONSTRUCTION_BIT_EXT;
	if (m_testParams.nonSubsampledImages)
		samplerCreateFlags		= 0u;

	const struct VkSamplerCreateInfo samplerInfo
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,			// sType
		DE_NULL,										// pNext
		(VkSamplerCreateFlags)samplerCreateFlags,		// flags
		VK_FILTER_NEAREST,								// magFilter
		VK_FILTER_NEAREST,								// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,					// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeW
		0.0f,											// mipLodBias
		VK_FALSE,										// anisotropyEnable
		1.0f,											// maxAnisotropy
		DE_FALSE,										// compareEnable
		VK_COMPARE_OP_ALWAYS,							// compareOp
		0.0f,											// minLod
		0.0f,											// maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,		// borderColor
		VK_FALSE,										// unnormalizedCoords
	};

	// Create a sampler that are able to read from subsampled image
	// (more than one sampler is needed only for 4 maxDescriptorSetSubsampledSamplers tests)
	for (deUint32 samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
		m_colorSamplers.push_back(VkSamplerSp(new Unique<VkSampler>(createSampler(vk, vkDevice, &samplerInfo))));

	if (!isDynamicRendering)
	{
		// Create render passes
		if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
			renderPassWrapper = RenderPassWrapperBasePtr(new RenderPassWrapper<RENDERING_TYPE_RENDERPASS_LEGACY>(vk, vkDevice, testParams));
		else
			renderPassWrapper = RenderPassWrapperBasePtr(new RenderPassWrapper<RENDERING_TYPE_RENDERPASS2>(vk, vkDevice, testParams));

		if (testParams.dynamicDensityMap)
			m_renderPassProduceDynamicDensityMap = renderPassWrapper->createRenderPassProduceDynamicDensityMap(m_viewMask);
		m_renderPassProduceSubsampledImage = renderPassWrapper->createRenderPassProduceSubsampledImage(m_viewMask, testParams.makeCopy, false);
		if (testParams.subsampledLoads)
			m_renderPassUpdateSubsampledImage = renderPassWrapper->createRenderPassProduceSubsampledImage(m_viewMask, false, true);
		m_renderPassOutputSubsampledImage = renderPassWrapper->createRenderPassOutputSubsampledImage();

		// Create framebuffers
		if (!testParams.imagelessFramebuffer)
		{
			if (testParams.dynamicDensityMap)
			{
				m_framebufferProduceDynamicDensityMap = createFrameBuffer(vk, vkDevice,
					*m_renderPassProduceDynamicDensityMap,
					densityMapImageSize,
					{ **m_densityMapImageViews[0] });
			}

			std::vector<VkImageView> imageViewsProduceSubsampledImage = { *m_colorImageView };
			if (isColorImageMultisampled)
				imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
			if (testParams.makeCopy)
				imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
			imageViewsProduceSubsampledImage.push_back(**m_densityMapImageViews[0]);

			m_framebufferProduceSubsampledImage = createFrameBuffer(vk, vkDevice,
				*m_renderPassProduceSubsampledImage,
				colorImageSize,
				imageViewsProduceSubsampledImage);

			if (testParams.subsampledLoads)
			{
				m_framebufferUpdateSubsampledImage = createFrameBuffer(vk, vkDevice,
					*m_renderPassUpdateSubsampledImage,
					colorImageSize,
					{ *m_colorImageView, **m_densityMapImageViews[1] });
			}

			m_framebufferOutputSubsampledImage = createFrameBuffer(vk, vkDevice,
				*m_renderPassOutputSubsampledImage,
				outputImageSize,
				{ *m_outputImageView });
		}
		else // create same framebuffers as above but with VkFramebufferAttachmentsCreateInfo instead of image views
		{
			// helper lambda used to create VkFramebufferAttachmentImageInfo structure and reduce code size
			auto createFramebufferAttachmentImageInfo = [](VkImageCreateFlags createFlags, VkImageUsageFlags usageFlags, VkExtent3D& extent, deUint32 layerCount, const VkFormat* format)
			{
				return VkFramebufferAttachmentImageInfo
				{
					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,	// VkStructureType		sType;
					DE_NULL,												// const void*			pNext;
					createFlags,											// VkImageCreateFlags	flags;
					usageFlags,												// VkImageUsageFlags	usage;
					extent.width,											// deUint32				width;
					extent.height,											// deUint32				height;
					layerCount,												// deUint32				layerCount;
					1u,														// deUint32				viewFormatCount;
					format													// const VkFormat*		pViewFormats;
				};
			};

			if (testParams.dynamicDensityMap)
			{
				m_framebufferProduceDynamicDensityMap = createImagelessFrameBuffer(vk, vkDevice,
					*m_renderPassProduceDynamicDensityMap,
					densityMapImageSize,
					{ createFramebufferAttachmentImageInfo(0u, densityMapImageUsage, densityMapImageSize, densityMapImageLayers, &m_testParams.densityMapFormat) });
			}

			std::vector<VkFramebufferAttachmentImageInfo> attachmentInfoProduceSubsampledImage;
			attachmentInfoProduceSubsampledImage.reserve(4);
			attachmentInfoProduceSubsampledImage.push_back(
				createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags,
					colorImageUsage,
					colorImageSize,
					colorImageLayers,
					&colorImageFormat));
			if (isColorImageMultisampled)
			{
				attachmentInfoProduceSubsampledImage.push_back(
					createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						colorImageSize,
						colorImageLayers,
						&colorImageFormat));
			}
			if (testParams.makeCopy)
			{
				attachmentInfoProduceSubsampledImage.push_back(
					createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						colorImageSize,
						colorImageLayers,
						&colorImageFormat));
			}
			attachmentInfoProduceSubsampledImage.push_back(
				createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags,
					colorImageUsage,
					colorImageSize,
					colorImageLayers,
					&colorImageFormat));

			m_framebufferProduceSubsampledImage = createImagelessFrameBuffer(vk, vkDevice,
				*m_renderPassProduceSubsampledImage,
				colorImageSize,
				attachmentInfoProduceSubsampledImage);

			if (testParams.subsampledLoads)
			{
				m_framebufferUpdateSubsampledImage = createImagelessFrameBuffer(vk, vkDevice,
					*m_renderPassUpdateSubsampledImage,
					colorImageSize,
					{
						createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags,
															 colorImageUsage,
															 colorImageSize,
															 colorImageLayers,
															 &colorImageFormat),
						createFramebufferAttachmentImageInfo(0u,
															 densityMapImageUsage,
															 densityMapImageSize,
															 densityMapImageLayers,
															 &m_testParams.densityMapFormat)
					});
			}

			m_framebufferOutputSubsampledImage = createImagelessFrameBuffer(vk, vkDevice,
				*m_renderPassOutputSubsampledImage,
				outputImageSize,
				{ createFramebufferAttachmentImageInfo(0u,
													   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
													   outputImageSize,
													   1u,
													   &colorImageFormat) });
		}
	}

	// Create pipeline layout for subpasses that do not use any descriptors
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			0u,												// deUint32						setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayoutNoDescriptors = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline layout for subpass that copies data or resamples subsampled image
	if (m_testParams.makeCopy || m_testParams.subsampledLoads)
	{
		m_descriptorSetLayoutOperateOnSubsampledImage =
			DescriptorSetLayoutBuilder()
			.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL)
			.build(vk, vkDevice);

		// Create and bind descriptor set
		m_descriptorPoolOperateOnSubsampledImage =
			DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
			.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		m_pipelineLayoutOperateOnSubsampledImage	= makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutOperateOnSubsampledImage);
		m_descriptorSetOperateOnSubsampledImage		= makeDescriptorSet(vk, vkDevice, *m_descriptorPoolOperateOnSubsampledImage, *m_descriptorSetLayoutOperateOnSubsampledImage);

		const VkDescriptorImageInfo inputImageInfo =
		{
			DE_NULL,											// VkSampleri		sampler;
			*m_colorImageView,									// VkImageView		imageView;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL			// VkImageLayout	imageLayout;
		};
		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSetOperateOnSubsampledImage, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &inputImageInfo)
			.update(vk, vkDevice);
	}

	// Create pipeline layout for last render pass (output subsampled image)
	{
		DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;
		DescriptorPoolBuilder		descriptorPoolBuilder;
		for (deUint32 samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
		{
			descriptorSetLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &(*m_colorSamplers[samplerIndex]).get());
			descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplerIndex + 1u);
		}

		m_descriptorSetLayoutOutputSubsampledImage	= descriptorSetLayoutBuilder.build(vk, vkDevice);
		m_descriptorPoolOutputSubsampledImage		= descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		m_pipelineLayoutOutputSubsampledImage		= makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutOutputSubsampledImage);
		m_descriptorSetOutputSubsampledImage		= makeDescriptorSet(vk, vkDevice, *m_descriptorPoolOutputSubsampledImage, *m_descriptorSetLayoutOutputSubsampledImage);

		VkImageView srcImageView = *m_colorImageView;
		if (isColorImageMultisampled)
			srcImageView = *m_colorResolvedImageView;
		else if (m_testParams.makeCopy)
			srcImageView = *m_colorCopyImageView;

		const VkDescriptorImageInfo inputImageInfo
		{
			DE_NULL,									// VkSampleri		sampler;
			srcImageView,								// VkImageView		imageView;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
		};

		DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
		for (deUint32 samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
			descriptorSetUpdateBuilder.writeSingle(*m_descriptorSetOutputSubsampledImage, DescriptorSetUpdateBuilder::Location::binding(samplerIndex), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &inputImageInfo);
		descriptorSetUpdateBuilder.update(vk, vkDevice);
	}

	// Load vertex and fragment shaders
	auto& bc = m_context.getBinaryCollection();
	m_vertexCommonShaderModule						= createShaderModule(vk, vkDevice, bc.get("vert"), 0);
	m_fragmentShaderModuleProduceSubsampledImage	= createShaderModule(vk, vkDevice, bc.get("frag_produce_subsampled"), 0);
	if (m_testParams.makeCopy)
	{
		const char* moduleName = isColorImageMultisampled ? "frag_copy_subsampled_ms" : "frag_copy_subsampled";
		m_fragmentShaderModuleCopySubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);
	}
	if (m_testParams.subsampledLoads)
	{
		const char* moduleName = "frag_update_subsampled";
		m_fragmentShaderModuleUpdateSubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);
	}
	const char* moduleName = (m_testParams.viewCount > 1) ? "frag_output_2darray" : "frag_output_2d";
	m_fragmentShaderModuleOutputSubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);

	const std::vector<VkRect2D>	dynamicDensityMapRenderArea	{ makeRect2D(densityMapImageSize.width, densityMapImageSize.height) };
	const std::vector<VkRect2D>	outputRenderArea			{ makeRect2D(outputImageSize.width, outputImageSize.height) };
	const VkRect2D				colorImageRect				= makeRect2D(colorImageSize.width, colorImageSize.height);
	std::vector<VkRect2D>		colorImageRenderArea		((m_testParams.multiViewport ? m_testParams.viewCount : 1u), colorImageRect);

	// Create pipelines
	{
		const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			(VkPipelineMultisampleStateCreateFlags)0u,					// VkPipelineMultisampleStateCreateFlags	flags
			(VkSampleCountFlagBits)m_testParams.colorSamples,			// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,													// VkBool32									sampleShadingEnable
			1.0f,														// float									minSampleShading
			DE_NULL,													// const VkSampleMask*						pSampleMask
			VK_FALSE,													// VkBool32									alphaToCoverageEnable
			VK_FALSE													// VkBool32									alphaToOneEnable
		};

		const std::vector<VkViewport>	viewportsProduceDynamicDensityMap	{ makeViewport(densityMapImageSize.width, densityMapImageSize.height) };
		const std::vector<VkViewport>	viewportsOutputSubsampledImage		{ makeViewport(outputImageSize.width, outputImageSize.height) };
		std::vector<VkViewport>			viewportsSubsampledImage			(colorImageRenderArea.size(), makeViewport(colorImageSize.width, colorImageSize.height));

		// test multiview in conjunction with multiViewport which specifies a different viewport per view
		if (m_testParams.multiViewport)
		{
			const deUint32 halfWidth	= colorImageSize.width / 2u;
			const float halfWidthFloat	= static_cast<float>(halfWidth);
			const float halfHeightFloat	= static_cast<float>(colorImageSize.height / 2u);
			for (deUint32 viewIndex = 0; viewIndex < m_testParams.viewCount; ++viewIndex)
			{
				// modify scissors/viewport for every other view
				bool isOdd = viewIndex % 2;

				auto& rect			= colorImageRenderArea[viewIndex];
				rect.extent.width	= halfWidth;
				rect.offset.x		= isOdd * halfWidth;

				auto& viewport		= viewportsSubsampledImage[viewIndex];
				viewport.width		= halfWidthFloat;
				viewport.height		= halfHeightFloat;
				viewport.y			= !isOdd * halfHeightFloat;
				viewport.x			= isOdd * halfWidthFloat;
			}
		}

		std::vector<vk::VkPipelineRenderingCreateInfoKHR> renderingCreateInfo(3,
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			DE_NULL,
			m_viewMask,
			1u,
			&m_testParams.densityMapFormat,
			vk::VK_FORMAT_UNDEFINED,
			vk::VK_FORMAT_UNDEFINED
		});
		renderingCreateInfo[1].pColorAttachmentFormats = &colorImageFormat;
		renderingCreateInfo[2].viewMask = 0;
		renderingCreateInfo[2].pColorAttachmentFormats = &colorImageFormat;

		const void* pNextForProduceDynamicDensityMap	= (isDynamicRendering ? &renderingCreateInfo[0] : DE_NULL);
		const void* pNextForeProduceSubsampledImage		= (isDynamicRendering ? &renderingCreateInfo[1] : DE_NULL);
		const void* pNextForeUpdateSubsampledImage		= (isDynamicRendering ? &renderingCreateInfo[1] : DE_NULL);
		const void* pNextForOutputSubsampledImage		= (isDynamicRendering ? &renderingCreateInfo[2] : DE_NULL);

		if (testParams.dynamicDensityMap)
			m_graphicsPipelineProduceDynamicDensityMap = buildGraphicsPipeline(vk,							// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutNoDescriptors,					// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceDynamicDensityMap,			// const VkRenderPass								renderPass
															viewportsProduceDynamicDensityMap,				// const std::vector<VkViewport>&					viewport
															dynamicDensityMapRenderArea,					// const std::vector<VkRect2D>&						scissor
															0u,												// const deUint32									subpass
															DE_NULL,										// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															pNextForProduceDynamicDensityMap,				// const void*										pNext
															isDynamicRendering,								// const bool										useDensityMapAttachment
															m_testParams.useMaintenance5);					// const bool										useMaintenance5

		m_graphicsPipelineProduceSubsampledImage = buildGraphicsPipeline(vk,								// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutNoDescriptors,					// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceSubsampledImage,			// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewport
															colorImageRenderArea,							// const std::vector<VkRect2D>&						scissor
															0u,												// const deUint32									subpass
															&multisampleStateCreateInfo,					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															pNextForeProduceSubsampledImage,				// const void*										pNext
															isDynamicRendering,								// const bool										useDensityMapAttachment
															m_testParams.useMaintenance5);					// const bool										useMaintenance5

		if(m_testParams.makeCopy)
			m_graphicsPipelineCopySubsampledImage = buildGraphicsPipeline(vk,								// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOperateOnSubsampledImage,		// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															*m_fragmentShaderModuleCopySubsampledImage,		// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceSubsampledImage,			// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewport
															colorImageRenderArea,							// const std::vector<VkRect2D>&						scissor
															1u,												// const deUint32									subpass
															&multisampleStateCreateInfo,					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															DE_NULL,										// const void*										pNext
															DE_FALSE);										// const bool										useDensityMapAttachment
		if (m_testParams.subsampledLoads)
			m_graphicsPipelineUpdateSubsampledImage = buildGraphicsPipeline(vk,								// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOperateOnSubsampledImage,		// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															*m_fragmentShaderModuleUpdateSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassUpdateSubsampledImage,				// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewport
															colorImageRenderArea,							// const std::vector<VkRect2D>&						scissor
															0u,												// const deUint32									subpass
															&multisampleStateCreateInfo,					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															pNextForeUpdateSubsampledImage,					// const void*										pNext
															isDynamicRendering,								// const bool										useDensityMapAttachment
															m_testParams.useMaintenance5);					// const bool										useMaintenance5


		m_graphicsPipelineOutputSubsampledImage = buildGraphicsPipeline(vk,									// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOutputSubsampledImage,			// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															*m_fragmentShaderModuleOutputSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassOutputSubsampledImage,				// const VkRenderPass								renderPass
															viewportsOutputSubsampledImage,					// const std::vector<VkViewport>&					viewport
															outputRenderArea,								// const std::vector<VkRect2D>&						scissor
															0u,												// const deUint32									subpass
															DE_NULL,										// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
															pNextForOutputSubsampledImage,					// const void*										pNext
															DE_FALSE);										// const bool										useDensityMapAttachment
	}

	// Create vertex buffers
	const tcu::Vec2 densityX(m_densityValue.x());
	const tcu::Vec2 densityY(m_densityValue.y());
	m_vertices			= createFullscreenMesh(1, {0.0f, 1.0f}, {0.0f, 1.0f});							// create fullscreen quad with gradient
	if (testParams.dynamicDensityMap)
		m_verticesDDM	= createFullscreenMesh(1, densityX, densityY);									// create fullscreen quad with single color
	m_verticesOutput	= createFullscreenMesh(m_testParams.viewCount, { 0.0f, 0.0f }, { 0.0f, 0.0f });	// create fullscreen mesh with black color

	createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_vertices, m_vertexBuffer, m_vertexBufferAlloc);
	if (testParams.dynamicDensityMap)
		createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesDDM, m_vertexBufferDDM, m_vertexBufferAllocDDM);
	createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesOutput, m_vertexBufferOutput, m_vertexBufferOutputAlloc);

	// Create command pool and command buffer
	m_cmdPool	= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	if (isDynamicRendering)
		createCommandBufferForDynamicRendering(dynamicDensityMapRenderArea[0], colorImageRect, outputRenderArea[0], vkDevice);
	else
		createCommandBufferForRenderpass(renderPassWrapper, colorImageSize, dynamicDensityMapRenderArea[0], colorImageRect, outputRenderArea[0]);
}

void FragmentDensityMapTestInstance::drawDynamicDensityMap(VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceDynamicDensityMap);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBufferDDM.get(), &vertexBufferOffset);
	vk.cmdDraw(cmdBuffer, (deUint32)m_verticesDDM.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawSubsampledImage(VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceSubsampledImage);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw(cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawResampleSubsampledImage(VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineUpdateSubsampledImage);
	vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0, 1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, DE_NULL);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw(cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawCopySubsampledImage(VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineOutputSubsampledImage);
	vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOutputSubsampledImage, 0, 1, &m_descriptorSetOutputSubsampledImage.get(), 0, DE_NULL);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBufferOutput.get(), &vertexBufferOffset);
	vk.cmdDraw(cmdBuffer, (deUint32)m_verticesOutput.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::createCommandBufferForRenderpass(RenderPassWrapperBasePtr	renderPassWrapper,
																	  const VkExtent3D&			colorImageSize,
																	  const VkRect2D&			dynamicDensityMapRenderArea,
																	  const VkRect2D&			colorImageRenderArea,
																	  const VkRect2D&			outputRenderArea)
{
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkDevice						vkDevice					= getDevice(m_context);
	const VkDeviceSize					vertexBufferOffset			= 0;
	const bool							isColorImageMultisampled	= m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
	const VkClearValue					attachmentClearValue		= makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
	const deUint32						attachmentCount				= 1 + m_testParams.makeCopy + isColorImageMultisampled;
	const std::vector<VkClearValue>		attachmentClearValues		(attachmentCount, attachmentClearValue);

	if (m_testParams.groupParams->useSecondaryCmdBuffer)
	{
		VkCommandBufferInheritanceInfo bufferInheritanceInfo
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,						// VkStructureType					sType;
			DE_NULL,																// const void*						pNext;
			*m_renderPassProduceDynamicDensityMap,									// VkRenderPass						renderPass;
			0,																		// uint32_t							subpass;
			*m_framebufferProduceDynamicDensityMap,									// VkFramebuffer					framebuffer;
			false,																	// VkBool32							occlusionQueryEnable;
			0u,																		// VkQueryControlFlags				queryFlags;
			DE_NULL,																// VkQueryPipelineStatisticFlags	pipelineStatistics;
		};
		const VkCommandBufferBeginInfo commandBufBeginParams
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,														// VkStructureType					sType;
			DE_NULL,																							// const void*						pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,		// VkCommandBufferUsageFlags		flags;
			&bufferInheritanceInfo
		};

		if (m_testParams.dynamicDensityMap)
		{
			m_dynamicDensityMapSecCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
			vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
			drawDynamicDensityMap(*m_dynamicDensityMapSecCmdBuffer);
			endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
		}

		bufferInheritanceInfo.renderPass = *m_renderPassProduceSubsampledImage;
		bufferInheritanceInfo.framebuffer = *m_framebufferProduceSubsampledImage;
		m_subsampledImageSecCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
		drawSubsampledImage(*m_subsampledImageSecCmdBuffer);
		if (m_testParams.makeCopy)
		{
			renderPassWrapper->cmdNextSubpass(*m_subsampledImageSecCmdBuffer);
			vk.cmdBindPipeline(*m_subsampledImageSecCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineCopySubsampledImage);
			vk.cmdBindDescriptorSets(*m_subsampledImageSecCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0, 1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, DE_NULL);
			vk.cmdBindVertexBuffers(*m_subsampledImageSecCmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_subsampledImageSecCmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
		}
		endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

		if (m_testParams.subsampledLoads)
		{
			bufferInheritanceInfo.renderPass = *m_renderPassUpdateSubsampledImage;
			bufferInheritanceInfo.framebuffer = *m_framebufferUpdateSubsampledImage;
			m_resampleSubsampledImageSecCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
			vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
			drawResampleSubsampledImage(*m_resampleSubsampledImageSecCmdBuffer);
			endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
		}

		bufferInheritanceInfo.renderPass = *m_renderPassOutputSubsampledImage;
		bufferInheritanceInfo.framebuffer = *m_framebufferOutputSubsampledImage;
		m_copySubsampledImageSecCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		vk.beginCommandBuffer(*m_copySubsampledImageSecCmdBuffer, &commandBufBeginParams);
		drawCopySubsampledImage(*m_copySubsampledImageSecCmdBuffer);
		endCommandBuffer(vk, *m_copySubsampledImageSecCmdBuffer);
	}

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// First render pass - render dynamic density map
	if (m_testParams.dynamicDensityMap)
	{
		std::vector<VkClearValue>	attachmentClearValuesDDM{ makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f) };

		const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,							// VkStructureType		sType;
			DE_NULL,																		// const void*			pNext;
			1u,																				// deUint32				attachmentCount;
			&**m_densityMapImageViews[0]													// const VkImageView*	pAttachments;
		};

		const VkRenderPassBeginInfo renderPassBeginInfoProduceDynamicDensityMap
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,										// VkStructureType		sType;
			m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : DE_NULL,	// const void*			pNext;
			*m_renderPassProduceDynamicDensityMap,											// VkRenderPass			renderPass;
			*m_framebufferProduceDynamicDensityMap,											// VkFramebuffer		framebuffer;
			dynamicDensityMapRenderArea,													// VkRect2D				renderArea;
			static_cast<deUint32>(attachmentClearValuesDDM.size()),							// uint32_t				clearValueCount;
			attachmentClearValuesDDM.data()													// const VkClearValue*	pClearValues;
		};

		renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoProduceDynamicDensityMap);

		if (m_testParams.groupParams->useSecondaryCmdBuffer)
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
		else
			drawDynamicDensityMap(*m_cmdBuffer);

		renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);
	}

	// Render subsampled image
	{
		std::vector<VkImageView> imageViewsProduceSubsampledImage = { *m_colorImageView };
		if (isColorImageMultisampled)
			imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
		if (m_testParams.makeCopy)
			imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
		imageViewsProduceSubsampledImage.push_back(**m_densityMapImageViews[0]);

		const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,								// VkStructureType		sType;
			DE_NULL,																			// const void*			pNext;
			static_cast<deUint32>(imageViewsProduceSubsampledImage.size()),						// deUint32				attachmentCount;
			imageViewsProduceSubsampledImage.data()												// const VkImageView*	pAttachments;
		};

		const VkRenderPassBeginInfo renderPassBeginInfoProduceSubsampledImage
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,											// VkStructureType		sType;
			m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : DE_NULL,		// const void*			pNext;
			*m_renderPassProduceSubsampledImage,												// VkRenderPass			renderPass;
			*m_framebufferProduceSubsampledImage,												// VkFramebuffer		framebuffer;
			colorImageRenderArea,																// VkRect2D				renderArea;
			static_cast<deUint32>(attachmentClearValues.size()),								// uint32_t				clearValueCount;
			attachmentClearValues.data()														// const VkClearValue*	pClearValues;
		};
		renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoProduceSubsampledImage);

		if (m_testParams.groupParams->useSecondaryCmdBuffer)
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
		else
		{
			drawSubsampledImage(*m_cmdBuffer);
			if (m_testParams.makeCopy)
			{
				renderPassWrapper->cmdNextSubpass(*m_cmdBuffer);
				vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineCopySubsampledImage);
				vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0, 1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, DE_NULL);
				vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
				vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
			}
		}

		renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);
	}

	// Resample subsampled image
	if (m_testParams.subsampledLoads)
	{
		VkImageView pAttachments[] = { *m_colorImageView, **m_densityMapImageViews[1] };
		const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,							// VkStructureType		sType;
			DE_NULL,																		// const void*			pNext;
			2u,																				// deUint32				attachmentCount;
			pAttachments																	// const VkImageView*	pAttachments;
		};

		const VkRenderPassBeginInfo renderPassBeginInfoUpdateSubsampledImage
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,										// VkStructureType		sType;
			m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : DE_NULL,	// const void*			pNext;
			*m_renderPassUpdateSubsampledImage,												// VkRenderPass			renderPass;
			*m_framebufferUpdateSubsampledImage,											// VkFramebuffer		framebuffer;
			makeRect2D(colorImageSize.width, colorImageSize.height),						// VkRect2D				renderArea;
			0u,																				// uint32_t				clearValueCount;
			DE_NULL																			// const VkClearValue*	pClearValues;
		};
		renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoUpdateSubsampledImage);

		if (m_testParams.groupParams->useSecondaryCmdBuffer)
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
		else
			drawResampleSubsampledImage(*m_cmdBuffer);

		renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);
	}

	// Copy subsampled image to normal image using sampler that is able to read from subsampled images
	// (subsampled image cannot be copied using vkCmdCopyImageToBuffer)
	const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,								// VkStructureType		sType;
		DE_NULL,																			// const void*			pNext;
		1u,																					// deUint32				attachmentCount;
		&*m_outputImageView																	// const VkImageView*	pAttachments;
	};

	const VkRenderPassBeginInfo renderPassBeginInfoOutputSubsampledImage
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,											// VkStructureType		sType;
		m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : DE_NULL,		// const void*			pNext;
		*m_renderPassOutputSubsampledImage,													// VkRenderPass			renderPass;
		*m_framebufferOutputSubsampledImage,												// VkFramebuffer		framebuffer;
		outputRenderArea,																	// VkRect2D				renderArea;
		static_cast<deUint32>(attachmentClearValues.size()),								// uint32_t				clearValueCount;
		attachmentClearValues.data()														// const VkClearValue*	pClearValues;
	};
	renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoOutputSubsampledImage);

	if (m_testParams.groupParams->useSecondaryCmdBuffer)
		vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_copySubsampledImageSecCmdBuffer);
	else
		drawCopySubsampledImage(*m_cmdBuffer);

	renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);

	endCommandBuffer(vk, *m_cmdBuffer);
}

void FragmentDensityMapTestInstance::createCommandBufferForDynamicRendering(const VkRect2D&		dynamicDensityMapRenderArea,
																			const VkRect2D&		colorImageRenderArea,
																			const VkRect2D&		outputRenderArea,
																			const VkDevice&     vkDevice)
{
	// no subpasses in dynamic rendering - makeCopy tests are not repeated for dynamic rendering
	DE_ASSERT (!m_testParams.makeCopy);

	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const bool							isColorImageMultisampled	= m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
	std::vector<VkClearValue>			attachmentClearValuesDDM	{ makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f) };
	const VkClearValue					attachmentClearValue		= makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
	const deUint32						attachmentCount				= 1 + m_testParams.makeCopy + isColorImageMultisampled;
	const std::vector<VkClearValue>		attachmentClearValues		(attachmentCount, attachmentClearValue);
	const VkImageSubresourceRange		dynamicDensitMapSubresourceRange	{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, m_testParams.viewCount, 0u, 1u };
	const VkImageSubresourceRange		colorSubresourceRange				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_testParams.viewCount };
	const VkImageSubresourceRange		outputSubresourceRange				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };

	const VkImageMemoryBarrier dynamicDensitMapBarrier = makeImageMemoryBarrier(
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT
									 : VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,		// VkAccessFlags			srcAccessMask;
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT
									 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,								// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout			newLayout;
		**m_densityMapImages[0],														// VkImage					image;
		dynamicDensitMapSubresourceRange												// VkImageSubresourceRange	subresourceRange;
	);

	const VkImageMemoryBarrier densityMapImageBarrier = makeImageMemoryBarrier(
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT
									 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask;
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT
									 : VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,		// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,								// VkImageLayout			newLayout;
		**m_densityMapImages[0],														// VkImage					image;
		colorSubresourceRange															// VkImageSubresourceRange	subresourceRange;
	);

	std::vector<VkImageMemoryBarrier> cbImageBarrier(2, makeImageMemoryBarrier(
		VK_ACCESS_NONE_KHR,																// VkAccessFlags			srcAccessMask;
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT
									 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout			newLayout;
		*m_colorImage,																	// VkImage					image;
		colorSubresourceRange															// VkImageSubresourceRange	subresourceRange;
	));
	cbImageBarrier[1].image = *m_colorResolvedImage;

	const VkImageMemoryBarrier subsampledImageBarrier = makeImageMemoryBarrier(
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT
									 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags						srcAccessMask;
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT
									 : VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags						dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						oldLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,										// VkImageLayout						newLayout;
		*m_colorImage,																	// VkImage								image;
		colorSubresourceRange															// VkImageSubresourceRange				subresourceRange;
	);

	const VkImageMemoryBarrier outputImageBarrier = makeImageMemoryBarrier(
		VK_ACCESS_NONE_KHR,																// VkAccessFlags						srcAccessMask;
		m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT
									 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags						dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout						oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						newLayout;
		*m_outputImage,																	// VkImage								image;
		outputSubresourceRange															// VkImageSubresourceRange				subresourceRange;
	);

	const VkRenderingFragmentDensityMapAttachmentInfoEXT densityMap0Attachment
	{
		VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,			// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		**m_densityMapImageViews[0],													// VkImageView							imageView;
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT								// VkImageLayout						imageLayout;
	};

	const VkRenderingFragmentDensityMapAttachmentInfoEXT densityMap1Attachment
	{
		VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,			// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		m_testParams.subsampledLoads ? **m_densityMapImageViews[1] : DE_NULL,			// VkImageView							imageView;
		VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT								// VkImageLayout						imageLayout;
	};

	const VkRenderingAttachmentInfoKHR dynamicDensityMapColorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		**m_densityMapImageViews[0],													// VkImageView							imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,															// VkResolveModeFlagBits				resolveMode;
		DE_NULL,																		// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,													// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp					storeOp;
		attachmentClearValuesDDM[0]														// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR dynamicDensityMapRenderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		0u,																				// VkRenderingFlagsKHR					flags;
		dynamicDensityMapRenderArea,													// VkRect2D								renderArea;
		m_testParams.viewCount,															// deUint32								layerCount;
		m_viewMask,																		// deUint32								viewMask;
		1u,																				// deUint32								colorAttachmentCount;
		&dynamicDensityMapColorAttachment,												// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	const VkRenderingAttachmentInfoKHR subsampledImageColorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		*m_colorImageView,																// VkImageView							imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						imageLayout;
		isColorImageMultisampled ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,	// VkResolveModeFlagBits				resolveMode;
		isColorImageMultisampled ? *m_colorResolvedImageView : DE_NULL,					// VkImageView							resolveImageView;
		isColorImageMultisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
									: VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,													// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp					storeOp;
		attachmentClearValues[0]														// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR subsampledImageRenderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		&densityMap0Attachment,
		0u,																				// VkRenderingFlagsKHR					flags;
		colorImageRenderArea,															// VkRect2D								renderArea;
		m_testParams.viewCount,															// deUint32								layerCount;
		m_viewMask,																		// deUint32								viewMask;
		1u,																				// deUint32								colorAttachmentCount;
		&subsampledImageColorAttachment,												// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	const VkRenderingAttachmentInfoKHR resampleSubsampledImageColorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		*m_colorImageView,																// VkImageView							imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,															// VkResolveModeFlagBits				resolveMode;
		DE_NULL,																		// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_LOAD,														// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp					storeOp;
		attachmentClearValues[0]														// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR resampleSubsampledImageRenderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		&densityMap1Attachment,
		0u,																				// VkRenderingFlagsKHR					flags;
		colorImageRenderArea,															// VkRect2D								renderArea;
		m_testParams.viewCount,															// deUint32								layerCount;
		m_viewMask,																		// deUint32								viewMask;
		1u,																				// deUint32								colorAttachmentCount;
		&resampleSubsampledImageColorAttachment,										// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	const VkRenderingAttachmentInfoKHR copySubsampledColorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		*m_outputImageView,																// VkImageView							imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,										// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,															// VkResolveModeFlagBits				resolveMode;
		DE_NULL,																		// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,													// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp					storeOp;
		attachmentClearValues[0]														// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR copySubsampledRenderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		0u,																				// VkRenderingFlagsKHR					flags;
		outputRenderArea,																// VkRect2D								renderArea;
		1u,																				// deUint32								layerCount;
		0u,																				// deUint32								viewMask;
		1u,																				// deUint32								colorAttachmentCount;
		&copySubsampledColorAttachment,													// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,																		// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	if (m_testParams.groupParams->useSecondaryCmdBuffer)
	{
		const VkFormat colorImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
		VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,			// VkStructureType						sType;
			DE_NULL,																	// const void*							pNext;
			0u,																			// VkRenderingFlagsKHR					flags;
			m_viewMask,																	// uint32_t								viewMask;
			1u,																			// uint32_t								colorAttachmentCount;
			&m_testParams.densityMapFormat,												// const VkFormat*						pColorAttachmentFormats;
			VK_FORMAT_UNDEFINED,														// VkFormat								depthAttachmentFormat;
			VK_FORMAT_UNDEFINED,														// VkFormat								stencilAttachmentFormat;
			VK_SAMPLE_COUNT_1_BIT														// VkSampleCountFlagBits				rasterizationSamples;
		};

		const VkCommandBufferInheritanceInfo	bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);
		VkCommandBufferBeginInfo				commandBufBeginParams
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
			DE_NULL,																// const void*						pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,							// VkCommandBufferUsageFlags		flags;
			&bufferInheritanceInfo
		};

		m_dynamicDensityMapSecCmdBuffer			= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		m_subsampledImageSecCmdBuffer			= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		m_resampleSubsampledImageSecCmdBuffer	= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		m_copySubsampledImageSecCmdBuffer		= allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

		// Record secondary command buffers
		if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			inheritanceRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

			if (m_testParams.dynamicDensityMap)
			{
				vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
				vk.cmdBeginRendering(*m_dynamicDensityMapSecCmdBuffer, &dynamicDensityMapRenderingInfo);
				drawDynamicDensityMap(*m_dynamicDensityMapSecCmdBuffer);
				vk.cmdEndRendering(*m_dynamicDensityMapSecCmdBuffer);
				endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
			}

			inheritanceRenderingInfo.pColorAttachmentFormats	= &colorImageFormat;
			inheritanceRenderingInfo.rasterizationSamples		= m_testParams.colorSamples;
			vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
			vk.cmdBeginRendering(*m_subsampledImageSecCmdBuffer, &subsampledImageRenderingInfo);
			drawSubsampledImage(*m_subsampledImageSecCmdBuffer);
			vk.cmdEndRendering(*m_subsampledImageSecCmdBuffer);
			endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

			if (m_testParams.subsampledLoads)
			{
				vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
				vk.cmdBeginRendering(*m_resampleSubsampledImageSecCmdBuffer, &resampleSubsampledImageRenderingInfo);
				drawResampleSubsampledImage(*m_resampleSubsampledImageSecCmdBuffer);
				vk.cmdEndRendering(*m_resampleSubsampledImageSecCmdBuffer);
				endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
			}

			inheritanceRenderingInfo.viewMask				= 0u;
			inheritanceRenderingInfo.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
			vk.beginCommandBuffer(*m_copySubsampledImageSecCmdBuffer, &commandBufBeginParams);
			vk.cmdBeginRendering(*m_copySubsampledImageSecCmdBuffer, &copySubsampledRenderingInfo);
			drawCopySubsampledImage(*m_copySubsampledImageSecCmdBuffer);
			vk.cmdEndRendering(*m_copySubsampledImageSecCmdBuffer);
			endCommandBuffer(vk, *m_copySubsampledImageSecCmdBuffer);
		}
		else
		{
			commandBufBeginParams.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

			if (m_testParams.dynamicDensityMap)
			{
				vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
				drawDynamicDensityMap(*m_dynamicDensityMapSecCmdBuffer);
				endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
			}

			inheritanceRenderingInfo.pColorAttachmentFormats	= &colorImageFormat;
			inheritanceRenderingInfo.rasterizationSamples		= m_testParams.colorSamples;
			vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
			drawSubsampledImage(*m_subsampledImageSecCmdBuffer);
			endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

			if (m_testParams.subsampledLoads)
			{
				vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
				drawResampleSubsampledImage(*m_resampleSubsampledImageSecCmdBuffer);
				endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
			}

			inheritanceRenderingInfo.viewMask				= 0u;
			inheritanceRenderingInfo.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
			vk.beginCommandBuffer(*m_copySubsampledImageSecCmdBuffer, &commandBufBeginParams);
			drawCopySubsampledImage(*m_copySubsampledImageSecCmdBuffer);
			endCommandBuffer(vk, *m_copySubsampledImageSecCmdBuffer);
		}

		// Record primary command buffer
		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		// Render dynamic density map
		if (m_testParams.dynamicDensityMap)
		{
			// change layout of density map - after filling it layout was changed
			// to density map optimal but here we want to render values to it
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, 0, DE_NULL, 0, DE_NULL, 1, &dynamicDensitMapBarrier);

			if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
			else
			{
				dynamicDensityMapRenderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
				vk.cmdBeginRendering(*m_cmdBuffer, &dynamicDensityMapRenderingInfo);
				vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
				vk.cmdEndRendering(*m_cmdBuffer);
			}

			// barrier that will change layout of density map
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
				0, 0, DE_NULL, 0, DE_NULL, 1, &densityMapImageBarrier);
		}

		// barrier that will change layout of color and resolve attachments
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, DE_NULL, 0, DE_NULL, 1 + isColorImageMultisampled, cbImageBarrier.data());

		// Render subsampled image
		if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
		else
		{
			subsampledImageRenderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
			vk.cmdBeginRendering(*m_cmdBuffer, &subsampledImageRenderingInfo);
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
			vk.cmdEndRendering(*m_cmdBuffer);
		}

		// Resample subsampled image
		if (m_testParams.subsampledLoads)
		{
			if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
			else
			{
				resampleSubsampledImageRenderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
				vk.cmdBeginRendering(*m_cmdBuffer, &resampleSubsampledImageRenderingInfo);
				vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
				vk.cmdEndRendering(*m_cmdBuffer);
			}
		}

		// barrier that ensures writing to colour image has completed.
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							  0, 0, DE_NULL, 0, DE_NULL, 1u, &subsampledImageBarrier);

		// barrier that will change layout of output image
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, DE_NULL, 0, DE_NULL, 1, &outputImageBarrier);

		if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_copySubsampledImageSecCmdBuffer);
		else
		{
			copySubsampledRenderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
			vk.cmdBeginRendering(*m_cmdBuffer, &copySubsampledRenderingInfo);
			vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_copySubsampledImageSecCmdBuffer);
			vk.cmdEndRendering(*m_cmdBuffer);
		}

		endCommandBuffer(vk, *m_cmdBuffer);
	}
	else
	{
		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		// First render pass - render dynamic density map
		if (m_testParams.dynamicDensityMap)
		{
			// change layout of density map - after filling it layout was changed
			// to density map optimal but here we want to render values to it
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								  0, 0, DE_NULL, 0, DE_NULL, 1, &dynamicDensitMapBarrier);

			vk.cmdBeginRendering(*m_cmdBuffer, &dynamicDensityMapRenderingInfo);
			drawDynamicDensityMap(*m_cmdBuffer);
			vk.cmdEndRendering(*m_cmdBuffer);

			// barrier that will change layout of density map
			vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
								  0, 0, DE_NULL, 0, DE_NULL, 1, &densityMapImageBarrier);
		}

		// barrier that will change layout of color and resolve attachments
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							  0, 0, DE_NULL, 0, DE_NULL, 1 + isColorImageMultisampled, cbImageBarrier.data());

		// Render subsampled image
		vk.cmdBeginRendering(*m_cmdBuffer, &subsampledImageRenderingInfo);
		drawSubsampledImage(*m_cmdBuffer);
		vk.cmdEndRendering(*m_cmdBuffer);

		// Resample subsampled image
		if (m_testParams.subsampledLoads)
		{
			vk.cmdBeginRendering(*m_cmdBuffer, &resampleSubsampledImageRenderingInfo);
			drawResampleSubsampledImage(*m_cmdBuffer);
			vk.cmdEndRendering(*m_cmdBuffer);
		}

		// barrier that ensures writing to colour image has completed.
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							  0, 0, DE_NULL, 0, DE_NULL, 1u, &subsampledImageBarrier);

		// barrier that will change layout of output image
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							  0, 0, DE_NULL, 0, DE_NULL, 1, &outputImageBarrier);

		vk.cmdBeginRendering(*m_cmdBuffer, &copySubsampledRenderingInfo);
		drawCopySubsampledImage(*m_cmdBuffer);
		vk.cmdEndRendering(*m_cmdBuffer);

		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

tcu::TestStatus FragmentDensityMapTestInstance::iterate (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= getDevice(m_context);
	const VkQueue			queue		= getDeviceQueue(vk, vkDevice, m_context.getUniversalQueueFamilyIndex(), 0);

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// approximations used when coarse reconstruction is specified are implementation defined
	if (m_testParams.coarseReconstruction)
		return tcu::TestStatus::pass("Pass");

	return verifyImage();
}

struct Vec4Sorter
{
	bool operator()(const tcu::Vec4& lhs, const tcu::Vec4& rhs) const
	{
		if (lhs.x() != rhs.x())
			return lhs.x() < rhs.x();
		if (lhs.y() != rhs.y())
			return lhs.y() < rhs.y();
		if (lhs.z() != rhs.z())
			return lhs.z() < rhs.z();
		return lhs.w() < rhs.w();
	}
};

tcu::TestStatus FragmentDensityMapTestInstance::verifyImage (void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						vkDevice			= getDevice(m_context);
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= getDeviceQueue(vk, vkDevice, queueFamilyIndex, 0);
	SimpleAllocator						memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	tcu::UVec2							renderSize			(m_renderSize.x(), m_renderSize.y());
	de::UniquePtr<tcu::TextureLevel>	outputImage			(pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_outputImage, VK_FORMAT_R8G8B8A8_UNORM, renderSize).release());
	const tcu::ConstPixelBufferAccess&	outputAccess		(outputImage->getAccess());
	tcu::TestLog&						log					(m_context.getTestContext().getLog());

	// Log images
	log << tcu::TestLog::ImageSet("Result", "Result images")
		<< tcu::TestLog::Image("Rendered", "Rendered output image", outputAccess)
		<< tcu::TestLog::EndImageSet;

	deInt32		noColorCount		= 0;
	deUint32	estimatedColorCount	= m_testParams.viewCount * m_testParams.fragmentArea.x() * m_testParams.fragmentArea.y();
	float		densityMult			= m_densityValue.x() * m_densityValue.y();

	// Create histogram of all image colors, check the value of inverted FragSizeEXT
	std::map<tcu::Vec4, deUint32, Vec4Sorter> colorCount;
	for (int y = 0; y < outputAccess.getHeight(); y++)
	{
		for (int x = 0; x < outputAccess.getWidth(); x++)
		{
			tcu::Vec4	outputColor		= outputAccess.getPixel(x, y);
			float		densityClamped	= outputColor.z() * outputColor.w();

			// for multiviewport cases we check only pixels to which we render
			if (m_testParams.multiViewport && outputColor.x() < 0.01f)
			{
				++noColorCount;
				continue;
			}

			if ((densityClamped + 0.01) < densityMult)
				return tcu::TestStatus::fail("Wrong value of FragSizeEXT variable");

			auto it = colorCount.find(outputColor);
			if (it == end(colorCount))
				it = colorCount.insert({ outputColor, 0u }).first;
			it->second++;
		}
	}

	// Check if color count is the same as estimated one
	for (const auto& color : colorCount)
	{
		if (color.second > estimatedColorCount)
			return tcu::TestStatus::fail("Wrong color count");
	}

	// For multiviewport cases ~75% of fragments should be black;
	// The margin of 100 fragments is used to compensate cases where
	// we can't fit all views in a same way to final 64x64 image
	// (64 can't be evenly divide for 6 views)
	deInt32 estimatedNoColorCount = m_renderSize.x() * m_renderSize.y() * 3 / 4;
	if (m_testParams.multiViewport && std::abs(noColorCount - estimatedNoColorCount) > 100)
		return tcu::TestStatus::fail("Wrong number of fragments with black color");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

static void createChildren (tcu::TestCaseGroup* fdmTests, const SharedGroupParams groupParams)
{
	tcu::TestContext&	testCtx		= fdmTests->getTestContext();

	const struct
	{
		std::string		name;
		deUint32		viewCount;
	} views[] =
	{
		{ "1_view",		1 },
		{ "2_views",	2 },
		{ "4_views",	4 },
		{ "6_views",	6 },
	};

	const struct
	{
		std::string			name;
		bool				makeCopy;
	} renders[] =
	{
		{ "render",			false },
		{ "render_copy",	true }
	};

	const struct
	{
		std::string		name;
		float			renderSizeToDensitySize;
	} sizes[] =
	{
		{ "divisible_density_size",		4.0f },
		{ "non_divisible_density_size",	3.75f }
	};

	const struct
	{
		std::string				name;
		VkSampleCountFlagBits	samples;
	} samples[] =
	{
		{ "1_sample",	VK_SAMPLE_COUNT_1_BIT },
		{ "2_samples",	VK_SAMPLE_COUNT_2_BIT },
		{ "4_samples",	VK_SAMPLE_COUNT_4_BIT },
		{ "8_samples",	VK_SAMPLE_COUNT_8_BIT }
	};

	std::vector<tcu::UVec2> fragmentArea
	{
		{ 1, 2 },
		{ 2, 1 },
		{ 2, 2 }
	};

	for (const auto& view : views)
	{
		if ((groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY) && view.viewCount > 1)
			continue;

		de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, view.name.c_str(), ""));
		for (const auto& render : renders)
		{
			if ((groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) && render.makeCopy)
				continue;

			de::MovePtr<tcu::TestCaseGroup> renderGroup(new tcu::TestCaseGroup(testCtx, render.name.c_str(), ""));
			for (const auto& size : sizes)
			{
				de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, size.name.c_str(), ""));
				for (const auto& sample : samples)
				{
					// Reduce number of tests for dynamic rendering cases where secondary command buffer is used
					if (groupParams->useSecondaryCmdBuffer && (sample.samples > VK_SAMPLE_COUNT_2_BIT))
						break;

					de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sample.name.c_str(), ""));
					for (const auto& area : fragmentArea)
					{
						std::stringstream str;
						str << "_" << area.x() << "_" << area.y();

						TestParams params
						{
							false,							// bool						dynamicDensityMap;
							false,							// bool						deferredDensityMap;
							false,							// bool						nonSubsampledImages;
							false,							// bool						subsampledLoads;
							false,							// bool						coarseReconstruction;
							false,							// bool						imagelessFramebuffer;
							false,							// bool						useMemoryAccess;
							false,							// bool						useMaintenance5;
							1,								// deUint32					samplersCount;
							view.viewCount,					// deUint32					viewCount;
							false,							// bool						multiViewport;
							render.makeCopy,				// bool						makeCopy;
							size.renderSizeToDensitySize,	// float					renderMultiplier;
							sample.samples,					// VkSampleCountFlagBits	colorSamples;
							area,							// tcu::UVec2				fragmentArea;
							{ 16, 16 },						// tcu::UVec2				densityMapSize;
							VK_FORMAT_R8G8_UNORM,			// VkFormat					densityMapFormat;
							groupParams						// SharedGroupParams		groupParams;
						};

						sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("static_subsampled") + str.str(), "", params));
						params.deferredDensityMap	= true;
						sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("deferred_subsampled") + str.str(), "", params));
						params.deferredDensityMap	= false;
						params.dynamicDensityMap	= true;
						sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("dynamic_subsampled") + str.str(), "", params));

						// generate nonsubsampled tests just for single view and double view cases
						if (view.viewCount < 3)
						{
							params.nonSubsampledImages	= true;
							params.dynamicDensityMap	= false;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("static_nonsubsampled") + str.str(), "", params));
							params.deferredDensityMap	= true;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("deferred_nonsubsampled") + str.str(), "", params));
							params.deferredDensityMap	= false;
							params.dynamicDensityMap	= true;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("dynamic_nonsubsampled") + str.str(), "", params));
						}

						// test multiviewport - each of views uses different viewport; limit number of cases to 2 samples
						if ((groupParams->renderingType == RENDERING_TYPE_RENDERPASS2) && (!render.makeCopy) &&
							(view.viewCount > 1) && (sample.samples == VK_SAMPLE_COUNT_2_BIT))
						{
							params.nonSubsampledImages	= false;
							params.dynamicDensityMap	= false;
							params.deferredDensityMap	= false;
							params.multiViewport		= true;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("static_subsampled") + str.str() + "_multiviewport", "", params));
						}
					}
					sizeGroup->addChild(sampleGroup.release());
				}
				renderGroup->addChild(sizeGroup.release());
			}
			viewGroup->addChild(renderGroup.release());
		}
		fdmTests->addChild(viewGroup.release());
	}

	const struct
	{
		std::string		name;
		deUint32		count;
	} subsampledSamplers[] =
	{
		{ "2_subsampled_samplers",	2 },
		{ "4_subsampled_samplers",	4 },
		{ "6_subsampled_samplers",	6 },
		{ "8_subsampled_samplers",	8 }
	};

	de::MovePtr<tcu::TestCaseGroup> propertiesGroup(new tcu::TestCaseGroup(testCtx, "properties", ""));
	for (const auto& sampler : subsampledSamplers)
	{
		TestParams params
		{
			false,							// bool						dynamicDensityMap;
			false,							// bool						deferredDensityMap;
			false,							// bool						nonSubsampledImages;
			false,							// bool						subsampledLoads;
			false,							// bool						coarseReconstruction;
			false,							// bool						imagelessFramebuffer;
			false,							// bool						useMemoryAccess;
			false,							// bool						useMaintenance5;
			sampler.count,					// deUint32					samplersCount;
			1,								// deUint32					viewCount;
			false,							// bool						multiViewport;
			false,							// bool						makeCopy;
			4.0f,							// float					renderMultiplier;
			VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits	colorSamples;
			{  2,  2 },						// tcu::UVec2				fragmentArea;
			{ 16, 16 },						// tcu::UVec2				densityMapSize;
			VK_FORMAT_R8G8_UNORM,			// VkFormat					densityMapFormat;
			groupParams						// SharedGroupParams		groupParams;
		};
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, sampler.name, "", params));

		// Reduce number of tests for dynamic rendering cases where secondary command buffer is used
		if (groupParams->useSecondaryCmdBuffer)
			break;
	}

	if ((groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) && (groupParams->useSecondaryCmdBuffer == false))
	{
		TestParams params
		{
			false,							// bool						dynamicDensityMap;
			false,							// bool						deferredDensityMap;
			false,							// bool						nonSubsampledImages;
			false,							// bool						subsampledLoads;
			false,							// bool						coarseReconstruction;
			false,							// bool						imagelessFramebuffer;
			false,							// bool						useMemoryAccess;
			true,							// bool						useMaintenance5;
			1,								// deUint32					samplersCount;
			1,								// deUint32					viewCount;
			false,							// bool						multiViewport;
			false,							// bool						makeCopy;
			4.0f,							// float					renderMultiplier;
			VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits	colorSamples;
			{  2,  2 },						// tcu::UVec2				fragmentArea;
			{ 16, 16 },						// tcu::UVec2				densityMapSize;
			VK_FORMAT_R8G8_UNORM,			// VkFormat					densityMapFormat;
			groupParams						// SharedGroupParams		groupParams;
		};
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "maintenance5", "", params));
	}

	if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
	{
		// interaction between fragment density map and imageless framebuffer

		const struct
		{
			std::string		name;
			bool			useSecondaryCmdBuffer;
		} commandBufferType[] =
		{
			{ "",						false},
			{ "secondary_cmd_buff_",	true}
		};

		for (const auto& cmdBuffType : commandBufferType)
		{
			TestParams params
			{
				false,									// bool						dynamicDensityMap;
				false,									// bool						deferredDensityMap;
				false,									// bool						nonSubsampledImages;
				false,									// bool						subsampledLoads;
				false,									// bool						coarseReconstruction;
				true,									// bool						imagelessFramebuffer;
				false,									// bool						useMemoryAccess;
				false,									// bool						useMaintenance5;
				1,										// deUint32					samplersCount;
				1,										// deUint32					viewCount;
				false,									// bool						multiViewport;
				false,									// bool						makeCopy;
				4.0f,									// float					renderMultiplier;
				VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	colorSamples;
				{  2,  2 },								// tcu::UVec2				fragmentArea;
				{ 16, 16 },								// tcu::UVec2				densityMapSize;
				VK_FORMAT_R8G8_UNORM,					// VkFormat					densityMapFormat;
				SharedGroupParams(new GroupParams		// SharedGroupParams		groupParams;
				{
					groupParams->renderingType,			//		RenderingType		renderingType;
					cmdBuffType.useSecondaryCmdBuffer,	//		bool				useSecondaryCmdBuffer;
					false,								//		bool				secondaryCmdBufferCompletelyContainsDynamicRenderpass;
				})
			};
			std::string namePrefix = cmdBuffType.name;

			params.deferredDensityMap = false;
			params.dynamicDensityMap = false;
			propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_static_subsampled", "", params));
			params.deferredDensityMap = true;
			propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_deferred_subsampled", "", params));
			params.deferredDensityMap = false;
			params.dynamicDensityMap = true;
			propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_dynamic_subsampled", "", params));
		}
	}

	if (groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
	{
		TestParams params
		{
			false,							// bool						dynamicDensityMap;
			false,							// bool						deferredDensityMap;
			false,							// bool						nonSubsampledImages;
			true,							// bool						subsampledLoads;
			false,							// bool						coarseReconstruction;
			false,							// bool						imagelessFramebuffer;
			false,							// bool						useMemoryAccess;
			false,							// bool						useMaintenance5;
			1,								// deUint32					samplersCount;
			2,								// deUint32					viewCount;
			false,							// bool						multiViewport;
			false,							// bool						makeCopy;
			4.0f,							// float					renderMultiplier;
			VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits	colorSamples;
			{  1,  2 },						// tcu::UVec2				fragmentArea;
			{ 16, 16 },						// tcu::UVec2				densityMapSize;
			VK_FORMAT_R8G8_UNORM,			// VkFormat					densityMapFormat;
			groupParams						// SharedGroupParams		groupParams;
		};
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_loads", "", params));
		params.subsampledLoads		= false;
		params.coarseReconstruction	= true;
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_coarse_reconstruction", "", params));
		params.useMemoryAccess		= true;
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "memory_access", "", params));
	}

	fdmTests->addChild(propertiesGroup.release());
}

static void cleanupGroup (tcu::TestCaseGroup* group, const SharedGroupParams)
{
	DE_UNREF(group);
	// Destroy singleton objects.
	g_singletonDevice.clear();
}

tcu::TestCaseGroup* createFragmentDensityMapTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "fragment_density_map", "VK_EXT_fragment_density_map and VK_EXT_fragment_density_map2 extensions tests", createChildren, groupParams, cleanupGroup);
}

} // renderpass

} // vkt
