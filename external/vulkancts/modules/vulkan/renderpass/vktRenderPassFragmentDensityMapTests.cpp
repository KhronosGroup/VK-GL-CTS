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

// set value of DRY_RUN_WITHOUT_FDM_EXTENSION to 1 for dummy run hat checks the correctness of the code without using VK_EXT_fragment_density_map extension
#define DRY_RUN_WITHOUT_FDM_EXTENSION 0

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
	deUint32				samplersCount;
	deUint32				viewCount;
	bool					makeCopy;
	float					renderMultiplier;
	VkSampleCountFlagBits	colorSamples;
	tcu::UVec2				fragmentArea;
	tcu::UVec2				densityMapSize;
	VkFormat				densityMapFormat;
};

struct Vertex4RGBA
{
	tcu::Vec4	position;
	tcu::Vec4	uv;
	tcu::Vec4	color;
};

de::SharedPtr<Move<vk::VkDevice>>	g_singletonDevice;

static std::vector<std::string> removeExtensions (const std::vector<std::string>& a, const std::vector<const char*>& b)
{
	std::vector<std::string>	res;
	std::set<std::string>		removeExts	(b.begin(), b.end());

	for (std::vector<std::string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
	{
		if (!de::contains(removeExts, *aIter))
			res.push_back(*aIter);
	}

	return res;
}

VkDevice getDevice(Context& context)
{
	if (!g_singletonDevice)
	{
		const float queuePriority = 1.0f;

		// Create a universal queue that supports graphics and compute
		const VkDeviceQueueCreateInfo	queueParams =
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
		std::vector<const char*>	extensionPtrs;
		std::vector<const char*>	coreExtensions;
		getCoreDeviceExtensions(context.getUsedApiVersion(), coreExtensions);
		std::vector<std::string>	nonCoreExtensions(removeExtensions(context.getDeviceExtensions(), coreExtensions));

		extensionPtrs.resize(nonCoreExtensions.size());

		for (size_t ndx = 0; ndx < nonCoreExtensions.size(); ++ndx)
			extensionPtrs[ndx] = nonCoreExtensions[ndx].c_str();

		VkPhysicalDeviceFragmentDensityMapFeaturesEXT	fragmentDensityMapFeatures	= initVulkanStructure();
		VkPhysicalDeviceFragmentDensityMap2FeaturesEXT	fragmentDensityMap2Features	= initVulkanStructure(&fragmentDensityMapFeatures);
		VkPhysicalDeviceFeatures2						features2					= initVulkanStructure(&fragmentDensityMap2Features);

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
		const VkPhysicalDeviceFeatures2 & feature2ptr = context.getDeviceFeatures2();

		const VkDeviceCreateInfo					deviceCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//sType;
			&feature2ptr,													//pNext;
			(VkDeviceCreateFlags)0u,										//flags
			1,																//queueRecordCount;
			&queueParams,													//pRequestedQueues;
			0,																//layerCount;
			DE_NULL,														//ppEnabledLayerNames;
			(deUint32)extensionPtrs.size(),			// deUint32				enabledExtensionCount;
			(extensionPtrs.empty() ? DE_NULL : &extensionPtrs[0]),			// const char* const*				ppEnabledExtensionNames;
			DE_NULL,														//pEnabledFeatures;
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

Move<VkRenderPass> createRenderPassProduceDynamicDensityMap(const DeviceInterface&	vk,
															VkDevice				vkDevice,
															deUint32				viewMask,
															const TestParams&		testParams)
{
	DE_ASSERT(testParams.dynamicDensityMap);

	typedef AttachmentDescription2	AttachmentDesc;
	typedef AttachmentReference2	AttachmentRef;
	typedef SubpassDescription2		SubpassDesc;
	typedef SubpassDependency2		SubpassDep;
	typedef RenderPassCreateInfo2	RenderPassCreateInfo;

	std::vector<AttachmentDesc> attachmentDescriptions
	{
		{
			DE_NULL,															// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			testParams.densityMapFormat,										// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
#if DRY_RUN_WITHOUT_FDM_EXTENSION
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL							// VkImageLayout					finalLayout
#else
			VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT					// VkImageLayout					finalLayout
#endif
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
#if DRY_RUN_WITHOUT_FDM_EXTENSION
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_TRANSFER_BIT,										// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,								// VkAccessFlags			srcAccessMask
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,								// VkAccessFlags			dstAccessMask
#else
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,					// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,								// VkAccessFlags			srcAccessMask
			VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,						// VkAccessFlags			dstAccessMask
#endif
			VK_DEPENDENCY_BY_REGION_BIT,										// VkDependencyFlags		dependencyFlags
			0u																	// deInt32					viewOffset
		}
	};

	vk::VkRenderPassMultiviewCreateInfo	renderPassMultiviewCreateInfo;
	void*								renderPassInfoPNext = DE_NULL;
	std::vector<deUint32>				viewMasks(subpassDescriptions.size(), viewMask);

	if (testParams.viewCount > 1)
	{
		renderPassMultiviewCreateInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
			renderPassInfoPNext,
			static_cast<deUint32>(viewMasks.size()),
			viewMasks.data(),
			0,
			DE_NULL,
			0U,
			DE_NULL
		};
		renderPassInfoPNext = (void*)&renderPassMultiviewCreateInfo;
	}

	const RenderPassCreateInfo renderPassInfo(
		renderPassInfoPNext,													// const void*						pNext
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

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

Move<VkRenderPass> createRenderPassProduceSubsampledImage(const DeviceInterface&	vk,
														  VkDevice					vkDevice,
														  deUint32					viewMask,
														  bool						makeCopySubpass,
														  bool						resampleSubsampled,
														  const TestParams&			testParams)
{
	typedef AttachmentDescription2	AttachmentDesc;
	typedef AttachmentReference2	AttachmentRef;
	typedef SubpassDescription2		SubpassDesc;
	typedef SubpassDependency2		SubpassDep;
	typedef RenderPassCreateInfo2	RenderPassCreateInfo;

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
			testParams.colorSamples,											// VkSampleCountFlagBits			samples
			loadOp,																// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL							// VkImageLayout					finalLayout
		}
	};

	// add resolve image when we use more than one sample per fragment
	if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
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
			testParams.colorSamples,											// VkSampleCountFlagBits			samples
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
		testParams.densityMapFormat,											// VkFormat							format
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
	if (testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
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
		if (testParams.viewCount > 1)
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
	if (testParams.coarseReconstruction)
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

	VkRenderPassFragmentDensityMapCreateInfoEXT renderPassFragmentDensityMap =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
		DE_NULL,
		{ densityMapAttachmentIndex, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT }
	};

	void* renderPassInfoPNext = (void*)&renderPassFragmentDensityMap;

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	// density map description is at the end - pop it from vector
	attachmentDescriptions.pop_back();
	renderPassInfoPNext = DE_NULL;
#endif

	vk::VkRenderPassMultiviewCreateInfo	renderPassMultiviewCreateInfo;
	std::vector<deUint32>				viewMasks		(subpassDescriptions.size(), viewMask);
	deInt32								pViewOffsets	(0);

	if (testParams.viewCount > 1)
	{
		renderPassMultiviewCreateInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
			renderPassInfoPNext,
			static_cast<deUint32>(viewMasks.size()),								// deUint32				subpassCount
			viewMasks.data(),														// const deUint32*		pViewMasks
			makeCopySubpass ? 1u : 0u,												// deUint32				dependencyCount
			makeCopySubpass ? &pViewOffsets : DE_NULL,								// const deInt32*		pViewOffsets
			0u,																		// deUint32				correlationMaskCount
			DE_NULL,																// const deUint32*		pCorrelationMasks
		};
		renderPassInfoPNext = (void*)&renderPassMultiviewCreateInfo;
	}

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

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

Move<VkRenderPass> createRenderPassOutputSubsampledImage(const DeviceInterface&	vk,
														 VkDevice				vkDevice)
{
	typedef AttachmentDescription2	AttachmentDesc;
	typedef AttachmentReference2	AttachmentRef;
	typedef SubpassDescription2		SubpassDesc;
	typedef RenderPassCreateInfo2	RenderPassCreateInfo;

	// copy subsampled image to ordinary image - you cannot retrieve subsampled image to CPU in any way.
	// You must first convert it into plain image through rendering
	std::vector<AttachmentDesc> attachmentDescriptions =
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

	std::vector<SubpassDesc> subpassDescriptions =
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

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

Move<VkFramebuffer> createFrameBuffer( const DeviceInterface& vk, VkDevice vkDevice, VkRenderPass renderPass, VkExtent3D size, const std::vector<VkImageView>& imageViews)
{
	const VkFramebufferCreateInfo framebufferParams =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkFramebufferCreateFlags	flags;
		renderPass,									// VkRenderPass				renderPass;
		static_cast<deUint32>(imageViews.size()),	// deUint32					attachmentCount;
		imageViews.data(),							// const VkImageView*		pAttachments;
		size.width,									// deUint32					width;
		size.height,								// deUint32					height;
		1u											// deUint32					layers;
	};

	return createFramebuffer(vk, vkDevice, &framebufferParams);
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

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	destImageLayout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	destImageDstStageFlags	= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	finalAccessMask			= VK_ACCESS_SHADER_READ_BIT;
#endif

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
									FragmentDensityMapTestInstance	(Context&			context,
																	 const TestParams&	testParams);
	virtual tcu::TestStatus			iterate							(void);

private:

	tcu::TestStatus					verifyImage						(void);

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
	sourceCollections.glslSources.add("vert") << glu::VertexSource(
		"#version 450\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in  vec4 inPosition;\n"
		"layout(location = 1) in  vec4 inUV;\n"
		"layout(location = 2) in  vec4 inColor;\n"
		"layout(location = 0) out vec4 outUV;\n"
		"layout(location = 1) out vec4 outColor;\n"
		"void main(void)\n"
		"{\n"
		"	gl_Position = inPosition;\n"
		"	outUV = inUV;\n"
		"	outColor = inColor;\n"
		"}\n"
	);

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	sourceCollections.glslSources.add("frag_produce_subsampled") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = vec4(inColor.x, inColor.y, 0.5, 0.5);\n"
		"}\n"
	);

	sourceCollections.glslSources.add("frag_update_subsampled") << glu::FragmentSource(
		"#version 450\n"
		"#extension GL_EXT_multiview : enable\n"
		"layout(location = 0) in vec4 inUV;\n"
		"layout(location = 1) in vec4 inColor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	if (gl_FragCoord.y < 0.5)\n"
		"		discard;\n"
		"	fragColor = vec4(inColor.x, inColor.y, 0.5, 0.5);\n"
		"}\n"
	);
#else
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
#endif
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

	std::map<std::string, std::string> parameters
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

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	if (m_testParams.viewCount > 1)
	{
		context.requireDeviceFunctionality("VK_KHR_multiview");
		if (!context.getMultiviewFeatures().multiview)
			TCU_THROW(NotSupportedError, "Implementation does not support multiview feature");
	}
#else
	context.requireDeviceFunctionality("VK_EXT_fragment_density_map");



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

	if (!m_testParams.nonSubsampledImages && (m_testParams.samplersCount > 1))
	{
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
		if (m_testParams.samplersCount > fragmentDensityMap2Properties.maxDescriptorSetSubsampledSamplers)
			TCU_THROW(NotSupportedError, "Required number of subsampled samplers is not supported");
	}
#endif

	vk::VkImageUsageFlags	colorImageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if (m_testParams.makeCopy)
		colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	deUint32				colorImageCreateFlags	= m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
	VkImageFormatProperties	imageFormatProperties	(getPhysicalDeviceImageFormatProperties(vki, vkPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, colorImageCreateFlags));

	if ((imageFormatProperties.sampleCounts & m_testParams.colorSamples) == 0)
		TCU_THROW(NotSupportedError, "Color image type not supported");
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

	// calculate all image sizes, image usage flags, view types etc.
	deUint32					densitiMapCount				= 1 + m_testParams.subsampledLoads;
	VkExtent3D					densityMapImageSize			{ m_testParams.densityMapSize.x(), m_testParams.densityMapSize.y(), 1 };
	deUint32					densityMapImageLayers		= m_testParams.viewCount;
	VkImageViewType				densityMapImageViewType		= (m_testParams.viewCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	vk::VkImageUsageFlags		densityMapImageUsage		= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	deUint32					densityMapImageViewFlags	= 0u;

	VkExtent3D					colorImageSize				{ m_renderSize.x() / m_testParams.viewCount, m_renderSize.y(), 1 };
	deUint32					colorImageLayers			= densityMapImageLayers;
	VkImageViewType				colorImageViewType			= densityMapImageViewType;
	vk::VkImageUsageFlags		colorImageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	deUint32					colorImageCreateFlags		= m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
	bool						isColorImageMultisampled	= m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;

	VkExtent3D					outputImageSize				{ m_renderSize.x(), m_renderSize.y(), 1 };

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

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	colorImageCreateFlags		= 0u;
	densityMapImageUsage		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	densityMapImageViewFlags	= 0u;
#endif

	// Create subsampled color image
	prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM,
		colorImageSize, colorImageLayers, m_testParams.colorSamples,
		colorImageUsage, queueFamilyIndex, 0u, colorImageViewType,
		componentMappingRGBA, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers },
		m_colorImage, m_colorImageAlloc, m_colorImageView);

	// Create subsampled color image for resolve operation ( when multisampling is used )
	if (isColorImageMultisampled)
	{
		prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM,
			colorImageSize, colorImageLayers, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u, colorImageViewType,
			componentMappingRGBA, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers },
			m_colorResolvedImage, m_colorResolvedImageAlloc, m_colorResolvedImageView);
	}

	// Create subsampled image copy
	if (m_testParams.makeCopy)
	{
		prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, VK_FORMAT_R8G8B8A8_UNORM,
			colorImageSize, colorImageLayers, m_testParams.colorSamples,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u, colorImageViewType,
			componentMappingRGBA, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers },
			m_colorCopyImage, m_colorCopyImageAlloc, m_colorCopyImageView);
	}

	// Create output image ( data from subsampled color image will be copied into it using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT )
	prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, VK_FORMAT_R8G8B8A8_UNORM,
		outputImageSize, 1u, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, queueFamilyIndex, 0u, VK_IMAGE_VIEW_TYPE_2D,
		componentMappingRGBA, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },
		m_outputImage, m_outputImageAlloc, m_outputImageView);

	// Create density map image/images
	for (deUint32 mapIndex = 0; mapIndex < densitiMapCount; ++mapIndex)
	{
		Move<VkImage>			densityMapImage;
		de::MovePtr<Allocation>	densityMapImageAlloc;
		Move<VkImageView>		densityMapImageView;

		prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, m_testParams.densityMapFormat,
			densityMapImageSize, densityMapImageLayers, VK_SAMPLE_COUNT_1_BIT,
			densityMapImageUsage, queueFamilyIndex, densityMapImageViewFlags, densityMapImageViewType,
			componentMappingRGBA, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, densityMapImageLayers },
			densityMapImage, densityMapImageAlloc, densityMapImageView);

		m_densityMapImages.push_back(VkImageSp(new Unique<VkImage>(densityMapImage)));
		m_densityMapImageAllocs.push_back(AllocationSp(densityMapImageAlloc.release()));
		m_densityMapImageViews.push_back(VkImageViewSp(new Unique<VkImageView>(densityMapImageView)));
	}

	// Create and fill staging buffer, copy its data to density map image
	if (!m_testParams.dynamicDensityMap)
	{
		tcu::TextureFormat				densityMapTextureFormat = vk::mapVkFormat(m_testParams.densityMapFormat);
		VkDeviceSize					stagingBufferSize		= tcu::getPixelSize(densityMapTextureFormat) * densityMapImageSize.width * densityMapImageSize.height * densityMapImageLayers;
		const vk::VkBufferCreateInfo	stagingBufferCreateInfo =
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

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	samplerCreateFlags = 0u;
#endif

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

	// Create render passes
	if (testParams.dynamicDensityMap)
		m_renderPassProduceDynamicDensityMap	= createRenderPassProduceDynamicDensityMap(vk, vkDevice, m_viewMask, testParams);
	m_renderPassProduceSubsampledImage			= createRenderPassProduceSubsampledImage(vk, vkDevice, m_viewMask, testParams.makeCopy, false, testParams);
	if (testParams.subsampledLoads)
		m_renderPassUpdateSubsampledImage		= createRenderPassProduceSubsampledImage(vk, vkDevice, m_viewMask, false, true, testParams);
	m_renderPassOutputSubsampledImage			= createRenderPassOutputSubsampledImage(vk, vkDevice);

	std::vector<VkImageView> imageViewsProduceSubsampledImage = { *m_colorImageView };
	if (isColorImageMultisampled)
		imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
	if (testParams.makeCopy)
		imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
	imageViewsProduceSubsampledImage.push_back(**m_densityMapImageViews[0]);

	std::vector<VkImageView> imageViewsUpdateSubsampledImage = { *m_colorImageView };
	if (testParams.subsampledLoads)
		imageViewsUpdateSubsampledImage.push_back(**m_densityMapImageViews[1]);

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	imageViewsProduceSubsampledImage.pop_back();
	imageViewsUpdateSubsampledImage.pop_back();
#endif

	// Create framebuffers
	if (testParams.dynamicDensityMap)
	{
		m_framebufferProduceDynamicDensityMap = createFrameBuffer(vk, vkDevice,
			*m_renderPassProduceDynamicDensityMap,
			densityMapImageSize,
			{ **m_densityMapImageViews[0] });
	}
	m_framebufferProduceSubsampledImage = createFrameBuffer(vk, vkDevice,
		*m_renderPassProduceSubsampledImage,
		colorImageSize,
		imageViewsProduceSubsampledImage);
	if (testParams.subsampledLoads)
	{
		m_framebufferUpdateSubsampledImage = createFrameBuffer(vk, vkDevice,
			*m_renderPassUpdateSubsampledImage,
			colorImageSize,
			imageViewsUpdateSubsampledImage);
	}
	m_framebufferOutputSubsampledImage = createFrameBuffer(vk, vkDevice,
		*m_renderPassOutputSubsampledImage,
		outputImageSize,
		{ *m_outputImageView });

	// Create pipeline layout for subpasses that do not use any descriptors
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
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

		const VkDescriptorImageInfo inputImageInfo =
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

	// Create pipelines
	{
		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,																// deUint32					binding;
			sizeof(Vertex4RGBA),											// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX										// VkVertexInputStepRate	inputRate;
		};

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions =
		{
			{ 0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
			{ 1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 4) },
			{ 2u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 8) }
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			static_cast<deUint32>(vertexInputAttributeDescriptions.size()),	// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions.data()							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

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
		const std::vector<VkRect2D>		scissorsProduceDynamicDensityMap	{ makeRect2D(densityMapImageSize.width, densityMapImageSize.height) };
		const std::vector<VkViewport>	viewportsSubsampledImage			{ makeViewport(colorImageSize.width, colorImageSize.height) };
		const std::vector<VkRect2D>		scissorsSubsampledImage				{ makeRect2D(colorImageSize.width, colorImageSize.height) };
		const std::vector<VkViewport>	viewportsOutputSubsampledImage		{ makeViewport(outputImageSize.width, outputImageSize.height) };
		const std::vector<VkRect2D>		scissorsOutputSubsampledImage		{ makeRect2D(outputImageSize.width, outputImageSize.height) };

		if (testParams.dynamicDensityMap)
			m_graphicsPipelineProduceDynamicDensityMap = makeGraphicsPipeline(vk,							// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutNoDescriptors,					// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															DE_NULL,										// const VkShaderModule								tessellationControlModule
															DE_NULL,										// const VkShaderModule								tessellationEvalModule
															DE_NULL,										// const VkShaderModule								geometryShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceDynamicDensityMap,			// const VkRenderPass								renderPass
															viewportsProduceDynamicDensityMap,				// const std::vector<VkViewport>&					viewports
															scissorsProduceDynamicDensityMap,				// const std::vector<VkRect2D>&						scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology						topology
															0u,												// const deUint32									subpass
															0u,												// const deUint32									patchControlPoints
															&vertexInputStateParams);						// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo

		m_graphicsPipelineProduceSubsampledImage = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutNoDescriptors,					// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															DE_NULL,										// const VkShaderModule								tessellationControlModule
															DE_NULL,										// const VkShaderModule								tessellationEvalModule
															DE_NULL,										// const VkShaderModule								geometryShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceSubsampledImage,			// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewports
															scissorsSubsampledImage,						// const std::vector<VkRect2D>&						scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology						topology
															0u,												// const deUint32									subpass
															0u,												// const deUint32									patchControlPoints
															&vertexInputStateParams,						// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
															DE_NULL,										// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
															&multisampleStateCreateInfo);					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
		if(m_testParams.makeCopy)
			m_graphicsPipelineCopySubsampledImage = makeGraphicsPipeline(vk,								// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOperateOnSubsampledImage,		// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															DE_NULL,										// const VkShaderModule								tessellationControlModule
															DE_NULL,										// const VkShaderModule								tessellationEvalModule
															DE_NULL,										// const VkShaderModule								geometryShaderModule
															*m_fragmentShaderModuleCopySubsampledImage,		// const VkShaderModule								fragmentShaderModule
															*m_renderPassProduceSubsampledImage,			// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewports
															scissorsSubsampledImage,						// const std::vector<VkRect2D>&						scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology						topology
															1u,												// const deUint32									subpass
															0u,												// const deUint32									patchControlPoints
															&vertexInputStateParams,						// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
															DE_NULL,										// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
															&multisampleStateCreateInfo);					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
		if (m_testParams.subsampledLoads)
			m_graphicsPipelineUpdateSubsampledImage = makeGraphicsPipeline(vk,								// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOperateOnSubsampledImage,		// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															DE_NULL,										// const VkShaderModule								tessellationControlModule
															DE_NULL,										// const VkShaderModule								tessellationEvalModule
															DE_NULL,										// const VkShaderModule								geometryShaderModule
															*m_fragmentShaderModuleUpdateSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassUpdateSubsampledImage,				// const VkRenderPass								renderPass
															viewportsSubsampledImage,						// const std::vector<VkViewport>&					viewports
															scissorsSubsampledImage,						// const std::vector<VkRect2D>&						scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology						topology
															0u,												// const deUint32									subpass
															0u,												// const deUint32									patchControlPoints
															&vertexInputStateParams,						// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
															DE_NULL,										// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
															&multisampleStateCreateInfo);					// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo

		m_graphicsPipelineOutputSubsampledImage = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
															vkDevice,										// const VkDevice									device
															*m_pipelineLayoutOutputSubsampledImage,			// const VkPipelineLayout							pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule								vertexShaderModule
															DE_NULL,										// const VkShaderModule								tessellationControlModule
															DE_NULL,										// const VkShaderModule								tessellationEvalModule
															DE_NULL,										// const VkShaderModule								geometryShaderModule
															*m_fragmentShaderModuleOutputSubsampledImage,	// const VkShaderModule								fragmentShaderModule
															*m_renderPassOutputSubsampledImage,				// const VkRenderPass								renderPass
															viewportsOutputSubsampledImage,					// const std::vector<VkViewport>&					viewports
															scissorsOutputSubsampledImage,					// const std::vector<VkRect2D>&						scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology						topology
															0u,												// const deUint32									subpass
															0u,												// const deUint32									patchControlPoints
															&vertexInputStateParams);						// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
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

	typedef RenderpassSubpass2 RPS2;
	const typename RPS2::SubpassBeginInfo	subpassBeginInfo		(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RPS2::SubpassEndInfo		subpassEndInfo			(DE_NULL);
	const VkDeviceSize						vertexBufferOffset		= 0;
	const VkClearValue						attachmentClearValue	= makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
	const deUint32							attachmentCount			= 1 + testParams.makeCopy + isColorImageMultisampled;
	const std::vector<VkClearValue>			attachmentClearValues	(attachmentCount, attachmentClearValue);

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// First render pass - render dynamic density map
	if (testParams.dynamicDensityMap)
	{
		std::vector<VkClearValue>	attachmentClearValuesDDM { makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f) };
		const VkRenderPassBeginInfo renderPassBeginInfoProduceDynamicDensityMap
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,							// VkStructureType		sType;
			DE_NULL,															// const void*			pNext;
			*m_renderPassProduceDynamicDensityMap,								// VkRenderPass			renderPass;
			*m_framebufferProduceDynamicDensityMap,								// VkFramebuffer		framebuffer;
			makeRect2D(densityMapImageSize.width, densityMapImageSize.height),	// VkRect2D				renderArea;
			static_cast<deUint32>(attachmentClearValuesDDM.size()),				// uint32_t				clearValueCount;
			attachmentClearValuesDDM.data()										// const VkClearValue*	pClearValues;
		};
		RPS2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceDynamicDensityMap, &subpassBeginInfo);
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceDynamicDensityMap);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBufferDDM.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_verticesDDM.size(), 1, 0, 0);
		RPS2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
	}

	// Render subsampled image
	const VkRenderPassBeginInfo renderPassBeginInfoProduceSubsampledImage
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,						// VkStructureType		sType;
		DE_NULL,														// const void*			pNext;
		*m_renderPassProduceSubsampledImage,							// VkRenderPass			renderPass;
		*m_framebufferProduceSubsampledImage,							// VkFramebuffer		framebuffer;
		makeRect2D(colorImageSize.width, colorImageSize.height),		// VkRect2D				renderArea;
		static_cast<deUint32>(attachmentClearValues.size()),			// uint32_t				clearValueCount;
		attachmentClearValues.data()									// const VkClearValue*	pClearValues;
	};
	RPS2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceSubsampledImage, &subpassBeginInfo);
	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceSubsampledImage);
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
	if (testParams.makeCopy)
	{
		RPS2::cmdNextSubpass(vk, *m_cmdBuffer, &subpassBeginInfo, &subpassEndInfo);
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineCopySubsampledImage);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0, 1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, DE_NULL);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
	}
	RPS2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	// Resample subsampled image
	if (testParams.subsampledLoads)
	{
		const VkRenderPassBeginInfo renderPassBeginInfoUpdateSubsampledImage
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,					// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			*m_renderPassUpdateSubsampledImage,							// VkRenderPass			renderPass;
			*m_framebufferUpdateSubsampledImage,						// VkFramebuffer		framebuffer;
			makeRect2D(colorImageSize.width, colorImageSize.height),	// VkRect2D				renderArea;
			0u,															// uint32_t				clearValueCount;
			DE_NULL														// const VkClearValue*	pClearValues;
		};
		RPS2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoUpdateSubsampledImage, &subpassBeginInfo);
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineUpdateSubsampledImage);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0, 1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, DE_NULL);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
		RPS2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
	}

	// Copy subsampled image to normal image using sampler that is able to read from subsampled images
	// (subsampled image cannot be copied using vkCmdCopyImageToBuffer)
	const VkRenderPassBeginInfo renderPassBeginInfoOutputSubsampledImage
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,							// VkStructureType		sType;
		DE_NULL,															// const void*			pNext;
		*m_renderPassOutputSubsampledImage,									// VkRenderPass			renderPass;
		*m_framebufferOutputSubsampledImage,								// VkFramebuffer		framebuffer;
		makeRect2D(outputImageSize.width, outputImageSize.height),			// VkRect2D				renderArea;
		static_cast<deUint32>(attachmentClearValues.size()),				// uint32_t				clearValueCount;
		attachmentClearValues.data()										// const VkClearValue*	pClearValues;
	};
	RPS2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoOutputSubsampledImage, &subpassBeginInfo);
	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineOutputSubsampledImage);
	vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOutputSubsampledImage, 0, 1, &m_descriptorSetOutputSubsampledImage.get(), 0, DE_NULL);
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBufferOutput.get(), &vertexBufferOffset);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_verticesOutput.size(), 1, 0, 0);
	RPS2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	endCommandBuffer(vk, *m_cmdBuffer);
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

	deUint32	estimatedColorCount	= m_testParams.viewCount * m_testParams.fragmentArea.x() * m_testParams.fragmentArea.y();
	float		densityMult			= m_densityValue.x() * m_densityValue.y();

#if DRY_RUN_WITHOUT_FDM_EXTENSION
	estimatedColorCount = m_testParams.viewCount + 2;
	densityMult			= 0.0f;
#endif

	// Create histogram of all image colors, check the value of inverted FragSizeEXT
	std::map<tcu::Vec4, deUint32, Vec4Sorter> colorCount;
	for (int y = 0; y < outputAccess.getHeight(); y++)
	{
		for (int x = 0; x < outputAccess.getWidth(); x++)
		{
			tcu::Vec4	outputColor		= outputAccess.getPixel(x, y);
			float		densityClamped	= outputColor.z() * outputColor.w();

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

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

static void createChildren (tcu::TestCaseGroup* fdmTests)
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
		de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, view.name.c_str(), ""));
		for (const auto& render : renders)
		{
			de::MovePtr<tcu::TestCaseGroup> renderGroup(new tcu::TestCaseGroup(testCtx, render.name.c_str(), ""));
			for (const auto& size : sizes)
			{
				de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, size.name.c_str(), ""));
				for (const auto& sample : samples)
				{
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
							1,								// deUint32					samplersCount;
							view.viewCount,					// deUint32					viewCount;
							render.makeCopy,				// bool						makeCopy;
							size.renderSizeToDensitySize,	// float					renderMultiplier;
							sample.samples,					// VkSampleCountFlagBits	colorSamples;
							area,							// tcu::UVec2				fragmentArea;
							{ 16, 16 },						// tcu::UVec2				densityMapSize;
							VK_FORMAT_R8G8_UNORM			// VkFormat					densityMapFormat;
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
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("static_nonsubsampled") + str.str(), "", params));
							params.deferredDensityMap	= true;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("deferred_nonsubsampled") + str.str(), "", params));
							params.deferredDensityMap	= false;
							params.dynamicDensityMap	= true;
							sampleGroup->addChild(new FragmentDensityMapTest(testCtx, std::string("dynamic_nonsubsampled") + str.str(), "", params));
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
			sampler.count,					// deUint32					samplersCount;
			1,								// deUint32					viewCount;
			false,							// bool						makeCopy;
			4.0f,							// float					renderMultiplier;
			VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits	colorSamples;
			{  2,  2 },						// tcu::UVec2				fragmentArea;
			{ 16, 16 },						// tcu::UVec2				densityMapSize;
			VK_FORMAT_R8G8_UNORM			// VkFormat					densityMapFormat;
		};
		propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, sampler.name, "", params));
	}
	TestParams params
	{
		false,							// bool						dynamicDensityMap;
		false,							// bool						deferredDensityMap;
		false,							// bool						nonSubsampledImages;
		true,							// bool						subsampledLoads;
		false,							// bool						coarseReconstruction;
		1,								// deUint32					samplersCount;
		2,								// deUint32					viewCount;
		false,							// bool						makeCopy;
		4.0f,							// float					renderMultiplier;
		VK_SAMPLE_COUNT_1_BIT,			// VkSampleCountFlagBits	colorSamples;
		{  1,  2 },						// tcu::UVec2				fragmentArea;
		{ 16, 16 },						// tcu::UVec2				densityMapSize;
		VK_FORMAT_R8G8_UNORM			// VkFormat					densityMapFormat;
	};
	propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_loads", "", params));
	params.subsampledLoads		= false;
	params.coarseReconstruction	= true;
	propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_coarse_reconstruction", "", params));
	fdmTests->addChild(propertiesGroup.release());
}

static void cleanupGroup (tcu::TestCaseGroup* group)
{
	DE_UNREF(group);
	// Destroy singleton objects.
	g_singletonDevice.clear();
}

tcu::TestCaseGroup* createFragmentDensityMapTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "fragment_density_map", "VK_EXT_fragment_density_map and VK_EXT_fragment_density_map2 extensions tests", createChildren, cleanupGroup);
}

} // renderpass

} // vkt
