/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Tests for VK_EXT_multisampled_render_to_single_sampled
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampledRenderToSingleSampledTests.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "deMath.h"

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include <string>
#include <vector>


// For testing, logs
#define DEBUG_LOGS 0

#if DEBUG_LOGS
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif


namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;
using tcu::IVec2;
using tcu::UVec2;
using tcu::Vec2;
using tcu::Vec4;
using tcu::IVec4;
using tcu::UVec4;

VkImageAspectFlags getDepthStencilAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

inline bool isDepthFormat (const VkFormat format)
{
	return (getDepthStencilAspectFlags(format) & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
}

inline bool isStencilFormat (const VkFormat format)
{
	return (getDepthStencilAspectFlags(format) & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;
}

using PipelineSp = SharedPtr<Unique<VkPipeline>>;

// How many regions to render to in multi-pass tests
constexpr deUint32 RegionCount	= 4;

struct DrawPushConstants
{
	Vec4 color1Data[2];
	Vec4 color2Data[2];
	IVec4 color3Data[2];
	Vec2 depthData;
};

struct VerifyPushConstants
{
	Vec4 color1Data[2] = {Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0)};
	Vec4 color2Data[2] = {Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0)};
	IVec4 color3Data[2] = {IVec4(0, 0, 0, 0), IVec4(0, 0, 0, 0)};
	float depthData = 0;
	deUint32 stencilData = 0;
};

struct VerificationResults
{
	deUint32 color1Verification;
	deUint32 color2Verification;
	deUint32 color3Verification;
	deUint32 depthVerification;
	deUint32 stencilVerification;
};

struct VerifySingleFloatPushConstants
{
	UVec4 area;
	Vec4 color;
	deUint32 attachmentNdx;
};

struct VerifySingleIntPushConstants
{
	UVec4 area;
	IVec4 color;
	deUint32 attachmentNdx;
};

struct VerifySingleDepthPushConstants
{
	UVec4 area;
	float depthData;
};

struct VerifySingleStencilPushConstants
{
	UVec4 area;
	deUint32 stencilData;
};

//! The parameters that define a test case
struct TestParams
{
	VkSampleCountFlagBits	numFloatColor1Samples;		//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	VkSampleCountFlagBits	numFloatColor2Samples;		//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	VkSampleCountFlagBits	numIntColorSamples;			//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	VkSampleCountFlagBits	numDepthStencilSamples;		//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples

	VkFormat				floatColor1Format;			//!< Color attachment format
	VkFormat				floatColor2Format;			//!< Color attachment format
	VkFormat				intColorFormat;				//!< Color attachment format
	VkFormat				depthStencilFormat;			//!< D/S attachment format. Will test both aspects if it's a mixed format

	VkClearValue			clearValues[4];

	VerifyPushConstants verifyConstants[RegionCount];

	bool isMultisampledRenderToSingleSampled;			//!< Whether the test should use VK_EXT_multisampled_render_to_single_sampled or normal multisampling
	bool clearBeforeRenderPass;							//!< Whether loadOp=CLEAR should be used, or clear is done before render pass and loadOp=LOAD is used
	bool renderToWholeFramebuffer;						//!< Whether the test should render to the whole framebuffer.
	bool testBlendsColors;								//!< Whether the test blends colors or overwrites them.  Tests don't adapt to this automatically, it's informative for shader generation.
	bool dynamicRendering;								//!< Whether the test should use dynamic rendering.
	bool useGarbageAttachment;							//!< Whether the test uese garbage attachments.

	struct PerPass
	{
		VkSampleCountFlagBits	numSamples;				//!< Pipeline samples

		deInt32 floatColor1Location;
		deInt32 floatColor2Location;
		deInt32 intColorLocation;
		bool hasDepthStencil;

		bool resolveFloatColor1;
		bool resolveFloatColor2;
		bool resolveIntColor;
		bool resolveDepthStencil;

		VkResolveModeFlagBits depthStencilResolveMode;

		DrawPushConstants drawConstantsWithDepthWrite[RegionCount];
		DrawPushConstants drawConstantsWithDepthTest[RegionCount];
	};

	std::vector<PerPass> perPass;

	// Used to carry forward the rng seed from test generation to test run.
	deUint32 rngSeed;

	PipelineConstructionType pipelineConstructionType;

	TestParams ()
		: numFloatColor1Samples		{}
		, numFloatColor2Samples		{}
		, numIntColorSamples		{}
		, numDepthStencilSamples	{}
		, floatColor1Format			{}
		, floatColor2Format			{}
		, intColorFormat			{}
		, depthStencilFormat		{}
		, clearValues				{}
	{
	}

	bool	usesColor1InPass		(const size_t passNdx) const	{ return perPass[passNdx].floatColor1Location >= 0; }
	bool	usesColor2InPass		(const size_t passNdx) const	{ return perPass[passNdx].floatColor2Location >= 0; }
	bool	usesColor3InPass		(const size_t passNdx) const	{ return perPass[passNdx].intColorLocation >= 0; }
	bool	usesDepthStencilInPass	(const size_t passNdx) const	{ return perPass[passNdx].hasDepthStencil; }
};

struct Image
{
	Move<VkImage>				image;
	MovePtr<Allocation>			alloc;
	Move<VkImageView>			view;

	void allocate(const DeviceInterface&		vk,
				  const VkDevice				device,
				  const MovePtr<Allocator>&		allocator,
				  const VkFormat				format,
				  const UVec2&					size,
				  const VkSampleCountFlagBits	samples,
				  const VkImageUsageFlags		usage,
				  const VkImageAspectFlags		aspect,
				  const deUint32				layerCount,
				  const bool					usedForMSRTSS);
	Move<VkImageView> makeView(const DeviceInterface&		vk,
							   const VkDevice				device,
							   const VkFormat				format,
							   const VkImageAspectFlags		aspect,
							   const deUint32				layerCount);
};

//! Common data used by the test
struct WorkingData
{
	UVec2						framebufferSize;			//!< Size of the framebuffer
	UVec4						renderArea;					//!< Render area

	Move<VkBuffer>				vertexBuffer;				//!< Contains a fullscreen triangle
	MovePtr<Allocation>			vertexBufferAlloc;			//!< Storage for vertexBuffer
	Move<VkBuffer>				verificationBuffer;			//!< Buffer used for validation
	MovePtr<Allocation>			verificationBufferAlloc;	//!< Storage for verificationBuffer
	Move<VkBuffer>				singleVerificationBuffer;		//!< Buffer used for validation of attachments outside the render area
	MovePtr<Allocation>			singleVerificationBufferAlloc;	//!< Storage for singleVerificationBuffer

	//!< Color and depth/stencil attachments
	Image						floatColor1;
	Image						floatColor2;
	Image						intColor;
	Image						depthStencil;
	Move<VkImageView>			depthOnlyImageView;
	Move<VkImageView>			stencilOnlyImageView;

	//!< Resolve attachments
	Image						floatResolve1;
	Image						floatResolve2;
	Image						intResolve;
	Image						depthStencilResolve;
	Move<VkImageView>			depthOnlyResolveImageView;
	Move<VkImageView>			stencilOnlyResolveImageView;

	//!< Verification results for logging (an array of 5 to avoid hitting maxPerStageDescriptorStorageImages limit of 4.
	Image						verify;

	WorkingData (void) {}

	Move<VkImage>&			getResolvedFloatColorImage1(const TestParams& params)
	{
		return params.numFloatColor1Samples != VK_SAMPLE_COUNT_1_BIT ? floatResolve1.image : floatColor1.image;
	}
	Move<VkImage>&			getResolvedFloatColorImage2(const TestParams& params)
	{
		return params.numFloatColor2Samples != VK_SAMPLE_COUNT_1_BIT ? floatResolve2.image : floatColor2.image;
	}
	Move<VkImage>&			getResolvedIntColorImage(const TestParams& params)
	{
		return params.numIntColorSamples != VK_SAMPLE_COUNT_1_BIT ? intResolve.image : intColor.image;
	}
	Move<VkImage>&			getResolvedDepthStencilImage(const TestParams& params)
	{
		return params.numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT ? depthStencilResolve.image : depthStencil.image;
	}

	Move<VkImageView>&			getResolvedFloatColorImage1View(const TestParams& params)
	{
		return params.numFloatColor1Samples != VK_SAMPLE_COUNT_1_BIT ? floatResolve1.view : floatColor1.view;
	}
	Move<VkImageView>&			getResolvedFloatColorImage2View(const TestParams& params)
	{
		return params.numFloatColor2Samples != VK_SAMPLE_COUNT_1_BIT ? floatResolve2.view : floatColor2.view;
	}
	Move<VkImageView>&			getResolvedIntColorImageView(const TestParams& params)
	{
		return params.numIntColorSamples != VK_SAMPLE_COUNT_1_BIT ? intResolve.view : intColor.view;
	}
	Move<VkImageView>&			getResolvedDepthOnlyImageView(const TestParams& params)
	{
		// If no depth aspect, return the stencil view just to have something bound in the desc set
		if (!isDepthFormat(params.depthStencilFormat))
			return getResolvedStencilOnlyImageView(params);
		return params.numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT ? depthOnlyResolveImageView : depthOnlyImageView;
	}
	Move<VkImageView>&			getResolvedStencilOnlyImageView(const TestParams& params)
	{
		// If no stencil aspect, return the depth view just to have something bound in the desc set
		if (!isStencilFormat(params.depthStencilFormat))
			return getResolvedDepthOnlyImageView(params);
		return params.numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT ? stencilOnlyResolveImageView : stencilOnlyImageView;
	}
};

// Accumulate objects throughout the test to avoid them getting deleted before the command buffer is submitted and waited on.
// Speeds up the test by avoiding making multiple submissions and waits.
class TestObjects
{
public:
	TestObjects(Context& contextIn);

	void									beginCommandBuffer();
	void									submitCommandsAndWait();

	const Unique<VkCommandPool>							cmdPool;
	const Unique<VkCommandBuffer>						cmdBuffer;
	std::vector<PipelineSp>								computePipelines;
	std::vector<MovePtr<GraphicsPipelineWrapper>>		graphicsPipelines;
	std::vector<Move<VkDescriptorPool>>					descriptorPools;
	std::vector<Move<VkDescriptorSet>>					descriptorSets;
	std::vector<RenderPassWrapper>						renderPassFramebuffers;

private:
	Context&								context;
};

const VkImageUsageFlags	commonImageUsageFlags		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
const VkImageUsageFlags	colorImageUsageFlags		= commonImageUsageFlags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
const VkImageUsageFlags depthStencilImageUsageFlags	= commonImageUsageFlags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

Move<VkImage> makeImage (const DeviceInterface&			vk,
						 const VkDevice					device,
						 const VkFormat					format,
						 const UVec2&					size,
						 const deUint32					layerCount,
						 const VkSampleCountFlagBits	samples,
						 const VkImageUsageFlags		usage,
						 const bool						usedForMSRTSS)
{
	const VkImageCreateFlags createFlags = samples == VK_SAMPLE_COUNT_1_BIT && usedForMSRTSS ? VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT : 0;

	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		createFlags,									// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),			// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		layerCount,										// deUint32					arrayLayers;
		samples,										// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return createImage(vk, device, &imageParams);
}

void Image::allocate (const DeviceInterface&		vk,
					  const VkDevice				device,
					  const MovePtr<Allocator>&		allocator,
					  const VkFormat				format,
					  const UVec2&					size,
					  const VkSampleCountFlagBits	samples,
					  const VkImageUsageFlags		usage,
					  const VkImageAspectFlags		aspect,
					  const deUint32				layerCount,
					  const bool					usedForMSRTSS)
{
	image	= makeImage(vk, device, format, size, layerCount, samples, usage, usedForMSRTSS);
	alloc	= bindImage(vk, device, *allocator, *image, MemoryRequirement::Any);
	view	= makeView(vk, device, format, aspect, layerCount);
}

Move<VkImageView> Image::makeView(const DeviceInterface&	vk,
								  const VkDevice			device,
								  const VkFormat			format,
								  const VkImageAspectFlags	aspect,
								  const deUint32				layerCount)
{
	return makeImageView(vk, device, *image, layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, format, makeImageSubresourceRange(aspect, 0u, 1u, 0u, layerCount));
}

//! Create a test-specific MSAA pipeline
MovePtr<GraphicsPipelineWrapper> makeGraphicsPipeline (const InstanceInterface&					vki,
													   const DeviceInterface&					vk,
													   const VkPhysicalDevice					physicalDevice,
													   const VkDevice							device,
													   const std::vector<std::string>&			deviceExtensions,
													   const PipelineConstructionType			pipelineConstructionType,
													   const PipelineLayoutWrapper&				pipelineLayout,
													   const VkRenderPass						renderPass,
													   VkPipelineRenderingCreateInfoKHR*		pipelineRenderingCreateInfo,
													   const ShaderWrapper						vertexModule,
													   const ShaderWrapper						fragmentModule,
													   const bool								enableBlend,
													   const bool								enableDepthStencilWrite,
													   const bool								enableDepthTest,
													   const deUint32							intWriteMask,
													   const deUint32							subpassNdx,
													   const deInt32							integerAttachmentLocation,
													   const UVec4&								viewportIn,
													   const UVec4&								scissorIn,
													   const VkSampleCountFlagBits				numSamples,
													   const bool								garbageAttachment)
{
	std::vector<VkVertexInputBindingDescription>	vertexInputBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>	vertexInputAttributeDescriptions;

	// Vertex attributes: position
	vertexInputBindingDescriptions.push_back  (makeVertexInputBindingDescription  (0u, sizeof(Vec4), VK_VERTEX_INPUT_RATE_VERTEX));
	vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u));

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags	flags;
		static_cast<deUint32>(vertexInputBindingDescriptions.size()),	// uint32_t									vertexBindingDescriptionCount;
		dataOrNullPtr(vertexInputBindingDescriptions),					// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		static_cast<deUint32>(vertexInputAttributeDescriptions.size()),	// uint32_t									vertexAttributeDescriptionCount;
		dataOrNullPtr(vertexInputAttributeDescriptions),				// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const std::vector<VkViewport> viewports
	{{
		static_cast<float>(viewportIn.x()), static_cast<float>(viewportIn.y()),		// x, y
		static_cast<float>(viewportIn.z()), static_cast<float>(viewportIn.w()),		// width, height
		0.0f, 1.0f																	// minDepth, maxDepth
	}};

	const std::vector<VkRect2D> scissors =
	{{
		makeOffset2D(scissorIn.x(), scissorIn.y()),
		makeExtent2D(scissorIn.z(), scissorIn.w()),
	}};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthClampEnable;
		VK_FALSE,													// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
		VK_FALSE,													// VkBool32									depthBiasEnable;
		0.0f,														// float									depthBiasConstantFactor;
		0.0f,														// float									depthBiasClamp;
		0.0f,														// float									depthBiasSlopeFactor;
		1.0f,														// float									lineWidth;
	};

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		numSamples,													// VkSampleCountFlagBits					rasterizationSamples;
		VK_TRUE,													// VkBool32									sampleShadingEnable;
		1.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	// Simply increment the buffer
	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_KEEP,						// stencil fail
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,						// depth only fail
		VK_COMPARE_OP_ALWAYS,					// compare op
		~0u,									// compare mask
		~0u,									// write mask
		0u);									// reference

	// Enable depth write and test if needed
	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,			// VkStructureType							sType;
		DE_NULL,															// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,							// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,															// VkBool32									depthTestEnable;
		enableDepthStencilWrite,											// VkBool32									depthWriteEnable;
		enableDepthTest ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_ALWAYS,		// VkCompareOp								depthCompareOp;
		VK_FALSE,															// VkBool32									depthBoundsTestEnable;
		VK_TRUE,															// VkBool32									stencilTestEnable;
		stencilOpState,														// VkStencilOpState							front;
		stencilOpState,														// VkStencilOpState							back;
		0.0f,																// float									minDepthBounds;
		1.0f,																// float									maxDepthBounds;
	};

	// Always blend by addition.  This is used to verify the combination of multiple draw calls.
	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendAttachmentState defaultBlendAttachmentState =
	{
		enableBlend
			? VK_TRUE
			: VK_FALSE,			// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp				alphaBlendOp;
		colorComponentsAll,		// VkColorComponentFlags	colorWriteMask;
	};

	VkPipelineColorBlendAttachmentState blendAttachmentStates[4] =
	{
		defaultBlendAttachmentState,
		defaultBlendAttachmentState,
		defaultBlendAttachmentState,
		defaultBlendAttachmentState,
	};

	if (enableBlend && integerAttachmentLocation >= 0)
	{
		// Disable blend for the integer attachment unconditionally
		blendAttachmentStates[integerAttachmentLocation].blendEnable = VK_FALSE;
		// But emulate it by outputting to one channel only.
		blendAttachmentStates[integerAttachmentLocation].colorWriteMask =
			((intWriteMask & 1) != 0 ? VK_COLOR_COMPONENT_R_BIT : 0) |
			((intWriteMask & 2) != 0 ? VK_COLOR_COMPONENT_G_BIT : 0) |
			((intWriteMask & 4) != 0 ? VK_COLOR_COMPONENT_B_BIT : 0) |
			((intWriteMask & 8) != 0 ? VK_COLOR_COMPONENT_A_BIT : 0);
		DE_ASSERT(blendAttachmentStates[integerAttachmentLocation].colorWriteMask != 0);
	}

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		4u,															// deUint32										attachmentCount;
		blendAttachmentStates,										// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
	};

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfoWithGarbage;
	std::vector<VkFormat> garbageFormats;

	if (garbageAttachment)
	{
		DE_ASSERT(pipelineRenderingCreateInfo);

		for (int i = 0; i < 10; i++)
			garbageFormats.push_back(VK_FORMAT_UNDEFINED);

		pipelineRenderingCreateInfoWithGarbage = *pipelineRenderingCreateInfo;
		// Just set a bunch of VK_FORMAT_UNDEFINED for garbage_color_attachment tests to make the validation happy.
		pipelineRenderingCreateInfoWithGarbage.colorAttachmentCount		= static_cast<uint32_t>(garbageFormats.size());
		pipelineRenderingCreateInfoWithGarbage.pColorAttachmentFormats  = garbageFormats.data();
	}

	MovePtr<GraphicsPipelineWrapper> graphicsPipeline = MovePtr<GraphicsPipelineWrapper>(new GraphicsPipelineWrapper(vki, vk, physicalDevice, device, deviceExtensions, pipelineConstructionType, 0u));
	graphicsPipeline.get()->setMonolithicPipelineLayout(pipelineLayout)
			.setupVertexInputState(&vertexInputStateInfo,
								   &pipelineInputAssemblyStateInfo)
			.setupPreRasterizationShaderState(viewports,
											  scissors,
											  pipelineLayout,
											  renderPass,
											  subpassNdx,
											  vertexModule,
											  &pipelineRasterizationStateInfo,
											  ShaderWrapper(),
											  ShaderWrapper(),
											  ShaderWrapper(),
											  DE_NULL,
											  nullptr,
											  garbageAttachment ? &pipelineRenderingCreateInfoWithGarbage : pipelineRenderingCreateInfo)
			.setupFragmentShaderState(pipelineLayout,
									  renderPass,
									  subpassNdx,
									  fragmentModule,
									  &pipelineDepthStencilStateInfo,
									  &pipelineMultisampleStateInfo)
			.setRenderingColorAttachmentsInfo(pipelineRenderingCreateInfo)
			.setupFragmentOutputState(renderPass,
									  subpassNdx,
									  &pipelineColorBlendStateInfo,
									  &pipelineMultisampleStateInfo)
			.buildPipeline();

	return graphicsPipeline;
}

void logTestImages(Context&						context,
				   const TestParams&			params,
				   WorkingData&					wd,
				   const bool					drawsToColor1,
				   const bool					drawsToColor2,
				   const bool					drawsToColor3,
				   const bool					drawsToDepthStencil)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	MovePtr<Allocator>		allocator		= MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));
	tcu::TestLog&			log				= context.getTestContext().getLog();

	const VkDeviceSize				bufferSize[4]	=
	{
		wd.framebufferSize.x() * wd.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.floatColor1Format)),
		wd.framebufferSize.x() * wd.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.floatColor2Format)),
		wd.framebufferSize.x() * wd.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.intColorFormat)),
		wd.framebufferSize.x() * wd.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.depthStencilFormat)),
	};
	const Move<VkBuffer>			buffer[5]		=
	{
		makeBuffer(vk, device, bufferSize[0], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		makeBuffer(vk, device, bufferSize[1], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		makeBuffer(vk, device, bufferSize[2], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		makeBuffer(vk, device, bufferSize[3], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		makeBuffer(vk, device, bufferSize[3], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
	};
	const MovePtr<Allocation>		bufferAlloc[5]	=
	{
		bindBuffer(vk, device, *allocator, *buffer[0], MemoryRequirement::HostVisible),
		bindBuffer(vk, device, *allocator, *buffer[1], MemoryRequirement::HostVisible),
		bindBuffer(vk, device, *allocator, *buffer[2], MemoryRequirement::HostVisible),
		bindBuffer(vk, device, *allocator, *buffer[3], MemoryRequirement::HostVisible),
		bindBuffer(vk, device, *allocator, *buffer[4], MemoryRequirement::HostVisible),
	};

	for (deUint32 bufferNdx = 0; bufferNdx < 5; ++bufferNdx)
		invalidateAlloc(vk, device, *bufferAlloc[bufferNdx]);

	const Unique<VkCommandPool>		cmdPool				(createCommandPool  (vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	const IVec2	size	(wd.framebufferSize.x(), wd.framebufferSize.y());
	{
		if (drawsToColor1)
			copyImageToBuffer(vk, *cmdBuffer, *wd.getResolvedFloatColorImage1(params),	*buffer[0], size, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
		if (drawsToColor2)
			copyImageToBuffer(vk, *cmdBuffer, *wd.getResolvedFloatColorImage2(params),	*buffer[1], size, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
		if (drawsToColor3)
			copyImageToBuffer(vk, *cmdBuffer, *wd.getResolvedIntColorImage(params),		*buffer[2], size, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

		VkImageLayout depthStencilLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		if (drawsToDepthStencil && isDepthFormat(params.depthStencilFormat))
		{
			copyImageToBuffer(vk, *cmdBuffer, *wd.getResolvedDepthStencilImage(params),	*buffer[3], size, VK_ACCESS_SHADER_WRITE_BIT, depthStencilLayout, 1,
							  getDepthStencilAspectFlags(params.depthStencilFormat), VK_IMAGE_ASPECT_DEPTH_BIT);
			depthStencilLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
		if (drawsToDepthStencil && isStencilFormat(params.depthStencilFormat))
		{
			copyImageToBuffer(vk, *cmdBuffer, *wd.getResolvedDepthStencilImage(params),	*buffer[4], size, VK_ACCESS_SHADER_WRITE_BIT, depthStencilLayout, 1,
							  getDepthStencilAspectFlags(params.depthStencilFormat), VK_IMAGE_ASPECT_STENCIL_BIT);
		}
	}

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

	// For the D32 depth formats, we specify the texture format directly as tcu::getEffectiveDepthStencilAccess assumes stencil data is interleaved.
	// For the D24 format however, we have to use tcu::getEffectiveDepthStencilAccess to correctly account for the 8-bit padding.
	const tcu::TextureFormat		copiedDepthFormat = tcu::TextureFormat(tcu::TextureFormat::D,
																		   params.depthStencilFormat == VK_FORMAT_D16_UNORM	? tcu::TextureFormat::UNORM_INT16
																															: tcu::TextureFormat::FLOAT);
	const tcu::TextureFormat		copiedStencilFormat = tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);

	const tcu::ConstPixelBufferAccess	testImageData[5]	=
	{
		{ mapVkFormat(params.floatColor1Format),	size.x(), size.y(), 1, bufferAlloc[0]->getHostPtr() },
		{ mapVkFormat(params.floatColor2Format),	size.x(), size.y(), 1, bufferAlloc[1]->getHostPtr() },
		{ mapVkFormat(params.intColorFormat),		size.x(), size.y(), 1, bufferAlloc[2]->getHostPtr() },
		{ copiedDepthFormat,						size.x(), size.y(), 1, bufferAlloc[3]->getHostPtr() },
		{ copiedStencilFormat,						size.x(), size.y(), 1, bufferAlloc[4]->getHostPtr() },
	};

	const tcu::ConstPixelBufferAccess	testImageDataD24	= tcu::getEffectiveDepthStencilAccess(tcu::ConstPixelBufferAccess(mapVkFormat(params.depthStencilFormat),
																															  size.x(), size.y(), 1, bufferAlloc[3]->getHostPtr()),
																								  tcu::Sampler::MODE_DEPTH);

	log << tcu::TestLog::ImageSet("attachments", "attachments");
	if (drawsToColor1)
		log << tcu::TestLog::Image("Color attachment 1",	"Color attachment 1",		testImageData[0]);
	if (drawsToColor2)
		log << tcu::TestLog::Image("Color attachment 2",	"Color attachment 2",		testImageData[1]);
	if (drawsToColor3)
		log << tcu::TestLog::Image("Color attachment 3",	"Color attachment 3",		testImageData[2]);
	if (isDepthFormat(params.depthStencilFormat))
		log << tcu::TestLog::Image("Depth attachment",		"Depth attachment",			params.depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT ? testImageDataD24 : testImageData[3]);
	if (isStencilFormat(params.depthStencilFormat))
		log << tcu::TestLog::Image("Stencil attachment",	"Stencil attachment",		testImageData[4]);
	log << tcu::TestLog::EndImageSet;
}

void logVerifyImages(Context&						context,
					 const TestParams&				params,
					 WorkingData&					wd,
					 const bool						drawsToColor1,
					 const bool						drawsToColor2,
					 const bool						drawsToColor3,
					 const bool						drawsToDepthStencil)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	MovePtr<Allocator>		allocator		= MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));
	tcu::TestLog&			log				= context.getTestContext().getLog();

	const VkDeviceSize				bufferSize	= wd.framebufferSize.x() * wd.framebufferSize.y() * 5 * tcu::getPixelSize(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	const Move<VkBuffer>			buffer		= makeBuffer(vk, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const MovePtr<Allocation>		bufferAlloc	= bindBuffer(vk, device, *allocator, *buffer, MemoryRequirement::HostVisible);

	invalidateAlloc(vk, device, *bufferAlloc);

	const Unique<VkCommandPool>		cmdPool				(createCommandPool  (vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	copyImageToBuffer(vk, *cmdBuffer, *wd.verify.image, *buffer, IVec2(wd.framebufferSize.x(), wd.framebufferSize.y()), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, 5);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

	const tcu::ConstPixelBufferAccess	verifyImageData	(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), wd.framebufferSize.x(), wd.framebufferSize.y(), 5, bufferAlloc->getHostPtr());

	log << tcu::TestLog::ImageSet("attachment error mask", "attachment error mask");
	if (drawsToColor1)
		log << tcu::TestLog::Image("ErrorMask color attachment 1", "Error mask color attachment 1", tcu::getSubregion(verifyImageData, 0, 0, 0, wd.framebufferSize.x(), wd.framebufferSize.y(), 1));
	if (drawsToColor2)
		log << tcu::TestLog::Image("ErrorMask color attachment 2", "Error mask color attachment 2", tcu::getSubregion(verifyImageData, 0, 0, 1, wd.framebufferSize.x(), wd.framebufferSize.y(), 1));
	if (drawsToColor3)
		log << tcu::TestLog::Image("ErrorMask color attachment 3", "Error mask color attachment 3", tcu::getSubregion(verifyImageData, 0, 0, 2, wd.framebufferSize.x(), wd.framebufferSize.y(), 1));
	if (drawsToDepthStencil && isDepthFormat(params.depthStencilFormat))
		log << tcu::TestLog::Image("ErrorMask depth attachment", "Error mask depth attachment", tcu::getSubregion(verifyImageData, 0, 0, 3, wd.framebufferSize.x(), wd.framebufferSize.y(), 1));
	if (drawsToDepthStencil && isStencilFormat(params.depthStencilFormat))
		log << tcu::TestLog::Image("ErrorMask stencil attachment", "Error mask stencil attachment", tcu::getSubregion(verifyImageData, 0, 0, 4, wd.framebufferSize.x(), wd.framebufferSize.y(), 1));
	log << tcu::TestLog::EndImageSet;
}

bool checkAndReportError (Context&						context,
						  const deUint32				verifiedPixelCount,
						  const deUint32				expectedPixelCount,
						  const std::string&			attachment)
{
	tcu::TestLog&	log	= context.getTestContext().getLog();

	bool passed = verifiedPixelCount == expectedPixelCount;

	if (passed)
		log << tcu::TestLog::Message << "Verification passed for " << attachment << tcu::TestLog::EndMessage;
	else
		log << tcu::TestLog::Message << "Verification failed for " << attachment << " for " << (expectedPixelCount - verifiedPixelCount) << " pixel(s)" << tcu::TestLog::EndMessage;

	return passed;
}

void checkSampleRequirements (Context&						context,
							  const VkSampleCountFlagBits	numSamples,
							  const bool					checkColor,
							  const bool					checkDepth,
							  const bool					checkStencil)
{
	const VkPhysicalDeviceLimits& limits = context.getDeviceProperties().limits;

	if (checkColor && (limits.framebufferColorSampleCounts & numSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if (checkDepth && (limits.framebufferDepthSampleCounts & numSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferDepthSampleCounts: sample count not supported");

	if (checkStencil && (limits.framebufferStencilSampleCounts & numSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferStencilSampleCounts: sample count not supported");
}

void checkImageRequirements (Context&						context,
							 const VkFormat					format,
							 const VkFormatFeatureFlags		requiredFeatureFlags,
							 const VkImageUsageFlags		requiredUsageFlags,
							 const VkSampleCountFlagBits	requiredSampleCount,
							 VkImageFormatProperties&		imageProperties)
{
	const InstanceInterface&		vki				= context.getInstanceInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();

	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);

	if ((formatProperties.optimalTilingFeatures & requiredFeatureFlags) != requiredFeatureFlags)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": format features not supported").c_str());

	const VkImageCreateFlags createFlags = requiredSampleCount == VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT : 0;

	const VkResult result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, requiredUsageFlags, createFlags, &imageProperties);

	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": format not supported").c_str());

	if ((imageProperties.sampleCounts & requiredSampleCount) != requiredSampleCount)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": sample count not supported").c_str());
}

TestObjects::TestObjects(Context& contextIn)
	: cmdPool(createCommandPool(contextIn.getDeviceInterface(), contextIn.getDevice(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, contextIn.getUniversalQueueFamilyIndex()))
	, cmdBuffer(makeCommandBuffer(contextIn.getDeviceInterface(), contextIn.getDevice(), *cmdPool))
	, context(contextIn)
{
}

void TestObjects::beginCommandBuffer()
{
	const DeviceInterface&	vk				= context.getDeviceInterface();

	vk::beginCommandBuffer(vk, *cmdBuffer);
}

void TestObjects::submitCommandsAndWait()
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	vk::submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);
}

void initializeAttachments(const TestParams& params, WorkingData& wd, std::vector<VkImage>& images, std::vector<VkImageView>& attachments, const size_t passNdx, deInt32 attachmentNdxes[8])
{
	const bool	includeAll	= passNdx >= params.perPass.size();
	deInt32		currentNdx	= 0;

	// Output attachments
	if (includeAll || params.usesColor1InPass(passNdx))
	{
		images.push_back(wd.floatColor1.image.get());
		attachments.push_back(wd.floatColor1.view.get());
		attachmentNdxes[0] = currentNdx++;
	}
	if (includeAll || params.usesColor2InPass(passNdx))
	{
		images.push_back(wd.floatColor2.image.get());
		attachments.push_back(wd.floatColor2.view.get());
		attachmentNdxes[1] = currentNdx++;
	}
	if (includeAll || params.usesColor3InPass(passNdx))
	{
		images.push_back(wd.intColor.image.get());
		attachments.push_back(wd.intColor.view.get());
		attachmentNdxes[2] = currentNdx++;
	}
	if (includeAll || params.usesDepthStencilInPass(passNdx))
	{
		images.push_back(wd.depthStencil.image.get());
		attachments.push_back(wd.depthStencil.view.get());
		attachmentNdxes[3] = currentNdx++;
	}

	// Resolve attachments
	if (params.numFloatColor1Samples != VK_SAMPLE_COUNT_1_BIT && (includeAll || params.usesColor1InPass(passNdx)))
	{
		images.push_back(wd.floatResolve1.image.get());
		attachments.push_back(wd.floatResolve1.view.get());
		attachmentNdxes[4] = currentNdx++;
	}
	if (params.numFloatColor2Samples != VK_SAMPLE_COUNT_1_BIT && (includeAll || params.usesColor2InPass(passNdx)))
	{
		images.push_back(wd.floatResolve2.image.get());
		attachments.push_back(wd.floatResolve2.view.get());
		attachmentNdxes[5] = currentNdx++;
	}
	if (params.numIntColorSamples != VK_SAMPLE_COUNT_1_BIT && (includeAll || params.usesColor3InPass(passNdx)))
	{
		images.push_back(wd.intResolve.image.get());
		attachments.push_back(wd.intResolve.view.get());
		attachmentNdxes[6] = currentNdx++;
	}
	if (params.numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT && (includeAll || params.usesDepthStencilInPass(passNdx)))
	{
		images.push_back(wd.depthStencilResolve.image.get());
		attachments.push_back(wd.depthStencilResolve.view.get());
		attachmentNdxes[7] = currentNdx++;
	}
}

void initializeAttachmentDescriptions(const TestParams& params, std::vector<VkAttachmentDescription2>& descs,
		const bool preCleared, const deInt32 attachmentNdxes[8], deUint32& attachmentUseMask)
{
	// The attachments are either cleared already or should be cleared now.  If an attachment was used in a previous render pass,
	// it will override these values to always LOAD and use the SHADER_READ_ONLY layout.  It's SHADER_READ_ONLY because final layout
	// is always that for simplicity.
	const VkAttachmentLoadOp	loadOp			= preCleared ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
	const VkImageLayout			initialLayout	= preCleared ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;

	// Output attachments
	if (attachmentNdxes[0] >= 0)
	{
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.floatColor1Format,										// VkFormat							format;
			params.numFloatColor1Samples,									// VkSampleCountFlagBits			samples;
			(attachmentUseMask & (1 << 0)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,													// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			(attachmentUseMask & (1 << 0)) != 0
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: initialLayout,											// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});
		attachmentUseMask |= 1 << 0;
	}

	if (attachmentNdxes[1] >= 0)
	{
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.floatColor2Format,										// VkFormat							format;
			params.numFloatColor2Samples,									// VkSampleCountFlagBits			samples;
			(attachmentUseMask & (1 << 1)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,													// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			(attachmentUseMask & (1 << 1)) != 0
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: initialLayout,											// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});
		attachmentUseMask |= 1 << 1;
	}

	if (attachmentNdxes[2] >= 0)
	{
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.intColorFormat,											// VkFormat							format;
			params.numIntColorSamples,										// VkSampleCountFlagBits			samples;
			(attachmentUseMask & (1 << 2)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,													// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			(attachmentUseMask & (1 << 2)) != 0
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: initialLayout,											// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});
		attachmentUseMask |= 1 << 2;
	}

	if (attachmentNdxes[3] >= 0)
	{
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.depthStencilFormat,										// VkFormat							format;
			params.numDepthStencilSamples,									// VkSampleCountFlagBits			samples;
			(attachmentUseMask & (1 << 3)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,													// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			(attachmentUseMask & (1 << 3)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,													// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				stencilStoreOp;
			(attachmentUseMask & (1 << 3)) != 0
				? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
				: initialLayout,											// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL					// VkImageLayout					finalLayout;
		});
		attachmentUseMask |= 1 << 3;
	}

	// Resolve attachments
	if (attachmentNdxes[4] >= 0)
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.floatColor1Format,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});

	if (attachmentNdxes[5] >= 0)
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.floatColor2Format,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});

	if (attachmentNdxes[6] >= 0)
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.intColorFormat,											// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
		});

	if (attachmentNdxes[7] >= 0)
		descs.push_back(VkAttachmentDescription2{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,						// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
			params.depthStencilFormat,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL					// VkImageLayout					finalLayout;
		});
}

void initializeRenderingAttachmentInfos (const TestParams&							params,
										 WorkingData&								wd,
										 std::vector<VkRenderingAttachmentInfo>&	colorAttachmentInfos,
										 VkRenderingAttachmentInfo&					depthStencilAttachmentInfo,
										 std::vector<VkFormat>&						colorAttachmentFormats,
										 const deInt32								attachmentNdxes[8],
										 deUint32&									attachmentUseMask,
										 deUint32									passNdx)
{
	// The attachments are either cleared already or should be cleared now. If an attachment was used in a previous render pass,
	// it will override these values to always LOAD and use the SHADER_READ_ONLY layout. It's SHADER_READ_ONLY because final layout
	// is always that for simplicity.
	const VkAttachmentLoadOp	loadOp			= params.clearBeforeRenderPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
	const TestParams::PerPass&	perPass			= params.perPass[passNdx];

	const VkRenderingAttachmentInfo emptyRenderingAttachmentInfo = {
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,	// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		DE_NULL,										// VkImageView				imageView
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			imageLayout
		VK_RESOLVE_MODE_NONE,							// VkResolveModeFlagBits	resolveMode
		DE_NULL,										// VkImageView				resolveImageView
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			resolveImageLayout
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// VkAttachmentLoadOp		loadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,				// VkAttachmentStoreOp		storeOp
		params.clearValues[0]							// VkClearValue				clearValue
	};

	for (auto& colorAttachmentInfo : colorAttachmentInfos)
	{
		colorAttachmentInfo = emptyRenderingAttachmentInfo;
	}

	// Output attachments
	if (attachmentNdxes[0] >= 0)
	{
		VkRenderingAttachmentInfo renderingAttachmentInfo = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,	// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			wd.floatColor1.view.get(),						// VkImageView				imageView
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			imageLayout
			VK_RESOLVE_MODE_NONE,							// VkResolveModeFlagBits	resolveMode
			DE_NULL,										// VkImageView				resolveImageView
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			resolveImageLayout
			(attachmentUseMask & (1 << 0)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,									// VkAttachmentLoadOp		loadOp
			VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp		storeOp
			params.clearValues[0]							// VkClearValue				clearValue
		};

		// Enable resolve image if it's used.
		if (attachmentNdxes[4] >= 0)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_AVERAGE_BIT;
			renderingAttachmentInfo.resolveImageView	= wd.floatResolve1.view.get();
			renderingAttachmentInfo.resolveImageLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		else if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_AVERAGE_BIT;
		}

		colorAttachmentInfos[perPass.floatColor1Location]	= renderingAttachmentInfo;
		colorAttachmentFormats[perPass.floatColor1Location]	= params.floatColor1Format;
		attachmentUseMask |= 1 << 0;
	}

	if (attachmentNdxes[1] >= 0)
	{
		VkRenderingAttachmentInfo renderingAttachmentInfo = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,	// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			wd.floatColor2.view.get(),						// VkImageView				imageView
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			imageLayout
			VK_RESOLVE_MODE_NONE,							// VkResolveModeFlagBits	resolveMode
			DE_NULL,										// VkImageView				resolveImageView
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			resolveImageLayout
			(attachmentUseMask & (1 << 1)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,									// VkAttachmentLoadOp		loadOp
			VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp		storeOp
			params.clearValues[1]							// VkClearValue				clearValue
		};

		if (attachmentNdxes[5] >= 0)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_AVERAGE_BIT;
			renderingAttachmentInfo.resolveImageView	= wd.floatResolve2.view.get();
			renderingAttachmentInfo.resolveImageLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		else if (params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_AVERAGE_BIT;
		}

		colorAttachmentInfos[perPass.floatColor2Location] = renderingAttachmentInfo;
		colorAttachmentFormats[perPass.floatColor2Location] = params.floatColor2Format;
		attachmentUseMask |= 1 << 1;
	}

	if (attachmentNdxes[2] >= 0)
	{
		VkRenderingAttachmentInfo renderingAttachmentInfo = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,	// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			wd.intColor.view.get(),							// VkImageView				imageView
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			imageLayout
			VK_RESOLVE_MODE_NONE,							// VkResolveModeFlagBits	resolveMode
			DE_NULL,										// VkImageView				resolveImageView
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			resolveImageLayout
			(attachmentUseMask & (1 << 2)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,									// VkAttachmentLoadOp		loadOp
			VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp		storeOp
			params.clearValues[2]							// VkClearValue				clearValue
		};

		if (attachmentNdxes[6] >= 0)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
			renderingAttachmentInfo.resolveImageView	= wd.intResolve.view.get();
			renderingAttachmentInfo.resolveImageLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		else if (params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT)
		{
			renderingAttachmentInfo.resolveMode			= VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
		}

		colorAttachmentInfos[perPass.intColorLocation] = renderingAttachmentInfo;
		colorAttachmentFormats[perPass.intColorLocation] = params.intColorFormat;
		attachmentUseMask |= 1 << 2;
	}

	if (attachmentNdxes[3] >= 0)
	{

		VkRenderingAttachmentInfo renderingAttachmentInfo = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,		// VkStructureType			sType
			DE_NULL,											// const void*				pNext
			wd.depthStencil.view.get(),							// VkImageView				imageView
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			imageLayout
			VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits	resolveMode
			DE_NULL,											// VkImageView				resolveImageView
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			resolveImageLayout
			(attachmentUseMask & (1 << 3)) != 0
				? VK_ATTACHMENT_LOAD_OP_LOAD
				: loadOp,										// VkAttachmentLoadOp		loadOp
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		storeOp
			params.clearValues[3]								// VkClearValue				clearValue
		};

		if (attachmentNdxes[7] >= 0)
		{
			renderingAttachmentInfo.resolveMode			= params.perPass[passNdx].depthStencilResolveMode;
			renderingAttachmentInfo.resolveImageView	= wd.depthStencilResolve.view.get();
			renderingAttachmentInfo.resolveImageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else if (params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT)
		{
			renderingAttachmentInfo.resolveMode			= params.perPass[passNdx].depthStencilResolveMode;
		}

		depthStencilAttachmentInfo = renderingAttachmentInfo;
		attachmentUseMask |= 1 << 3;
	}
}

void initResolveImageLayouts (Context&				context,
							  const TestParams&		params,
							  WorkingData&			wd,
							  TestObjects&			testObjects)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkImageMemoryBarrier	imageBarrierTemplate	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType			sType
		DE_NULL,																// const void*				pNext
		0,																		// VkAccessFlags			srcAccessMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,						// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t					dstQueueFamilyIndex
		0,																		// VkImage					image
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),	// VkImageSubresourceRange	subresourceRange
	};

	std::vector<VkImageMemoryBarrier> barriers;

	if (wd.floatResolve1.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.floatResolve1.image;
	}

	if (wd.floatResolve2.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.floatResolve2.image;
	}

	if (wd.intResolve.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.intResolve.image;
	}

	if (wd.depthStencilResolve.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image						= *wd.depthStencilResolve.image;
		barriers.back().newLayout					= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers.back().subresourceRange.aspectMask	= getDepthStencilAspectFlags(params.depthStencilFormat);
	}

	if (!barriers.empty())
	{
		vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, static_cast<uint32_t>(barriers.size()), barriers.data());
	}
}

void preRenderingImageLayoutTransition (Context&				context,
										const TestParams&		params,
										WorkingData&			wd,
										TestObjects&			testObjects)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const bool preCleared = params.clearBeforeRenderPass;

	const VkImageMemoryBarrier	imageBarrierTemplate =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
		DE_NULL,																// const void*                pNext;
		preCleared ? VK_ACCESS_TRANSFER_WRITE_BIT : (VkAccessFlagBits)0,		// VkAccessFlags              srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,								// VkAccessFlags              dstAccessMask;
		preCleared
			? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			: VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout              oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout              newLayout;
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
		0,																		// VkImage                    image;
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),	// VkImageSubresourceRange    subresourceRange;
	};

	VkImageMemoryBarrier	barriers[4]		= { imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate };
	barriers[0].image						= *wd.floatColor1.image;
	barriers[1].image						= *wd.floatColor2.image;
	barriers[2].image						= *wd.intColor.image;
	barriers[3].image						= *wd.depthStencil.image;
	barriers[3].dstAccessMask				= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
														VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[3].newLayout					= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barriers[3].subresourceRange.aspectMask	= getDepthStencilAspectFlags(params.depthStencilFormat);

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer, preCleared ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
}

void postRenderingResolveImageLayoutTransition (Context&				context,
												const TestParams&		params,
												WorkingData&			wd,
												TestObjects&			testObjects)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkImageMemoryBarrier	imageBarrierTemplate	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType			sType
		DE_NULL,																// const void*				pNext
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,						// VkAccessFlags			srcAccessMask
		VK_ACCESS_SHADER_READ_BIT,												// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,								// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t					dstQueueFamilyIndex
		0,																		// VkImage					image
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),	// VkImageSubresourceRange	subresourceRange
	};

	std::vector<VkImageMemoryBarrier> barriers;

	if (wd.floatResolve1.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.floatResolve1.image;
	}

	if (wd.floatResolve2.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.floatResolve2.image;
	}

	if (wd.intResolve.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image = *wd.intResolve.image;
	}

	if (wd.depthStencilResolve.image)
	{
		barriers.push_back(imageBarrierTemplate);
		barriers.back().image						= *wd.depthStencilResolve.image;
		barriers.back().oldLayout					= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers.back().newLayout					= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		barriers.back().subresourceRange.aspectMask	= getDepthStencilAspectFlags(params.depthStencilFormat);
	}

	if (!barriers.empty())
	{
		vk.cmdPipelineBarrier(*testObjects.cmdBuffer,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
							  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT , 0u,
							  0u, DE_NULL, 0u, DE_NULL, static_cast<uint32_t>(barriers.size()), barriers.data());
	}
}

void preinitializeAttachmentReferences(std::vector<VkAttachmentReference2>& references, const deUint32 count)
{
	references.resize(count, VkAttachmentReference2{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,		// VkStructureType       sType;
		DE_NULL,										// const void*           pNext;
		VK_ATTACHMENT_UNUSED,							// uint32_t              attachment;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout         layout;
		0,												// VkImageAspectFlags    aspectMask;
	});
}

void initializeAttachmentReference(VkAttachmentReference2& reference, deUint32 attachment, const VkFormat depthStencilFormat, const bool isInputAttachment)
{
	const bool isColor	= depthStencilFormat == VK_FORMAT_UNDEFINED;

	reference.attachment	= attachment;
	reference.layout		= isInputAttachment
		? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		: isColor
			? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	reference.aspectMask	= isColor
		? VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
		: getDepthStencilAspectFlags(depthStencilFormat);
}

bool isInAttachmentReferences(const std::vector<VkAttachmentReference2>& references, const deInt32 attachment)
{
	for (const VkAttachmentReference2& reference: references)
		if (reference.attachment == static_cast<deUint32>(attachment))
			return true;

	return false;
}

void addSubpassDescription(const TestParams&										params,
						   const deUint32											passNdx,
						   std::vector<VkAttachmentReference2>&						attachmentReferences,
						   std::vector<VkAttachmentReference2>&						resolveAttachmentReferences,
						   VkSubpassDescriptionDepthStencilResolve&					depthStencilResolve,
						   std::vector<deUint32>*									preserveAttachments,
						   VkMultisampledRenderToSingleSampledInfoEXT&				msrtss,
						   std::vector<VkSubpassDescription2>&						subpasses,
						   const std::vector<VkAttachmentReference2>&				inputAttachmentReferences,
						   const deInt32											attachmentNdxes[8])
{
	const TestParams::PerPass&	perPass							= params.perPass[passNdx];
	bool						anySingleSampledAttachmentsUsed	= false;

	// Maximum 4 attachment references for color and 1 for depth
	preinitializeAttachmentReferences(attachmentReferences, 5);
	preinitializeAttachmentReferences(resolveAttachmentReferences, 5);

	if (perPass.floatColor1Location >= 0)
	{
		initializeAttachmentReference(attachmentReferences[perPass.floatColor1Location],
				attachmentNdxes[0], VK_FORMAT_UNDEFINED, false);
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT;
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[0]))
	{
		preserveAttachments->push_back(attachmentNdxes[0]);
	}
	if (perPass.floatColor2Location >= 0)
	{
		initializeAttachmentReference(attachmentReferences[perPass.floatColor2Location],
				attachmentNdxes[1], VK_FORMAT_UNDEFINED, false);
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT;
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[1]))
	{
		preserveAttachments->push_back(attachmentNdxes[1]);
	}
	if (perPass.intColorLocation >= 0)
	{
		initializeAttachmentReference(attachmentReferences[perPass.intColorLocation],
				attachmentNdxes[2], VK_FORMAT_UNDEFINED, false);
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT;
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[2]))
	{
		preserveAttachments->push_back(attachmentNdxes[2]);
	}
	if (perPass.hasDepthStencil)
	{
		initializeAttachmentReference(attachmentReferences.back(),
				attachmentNdxes[3], params.depthStencilFormat, false);
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT;
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[3]))
	{
		preserveAttachments->push_back(attachmentNdxes[3]);
	}

	// Resolve attachments
	if (perPass.resolveFloatColor1)
	{
		initializeAttachmentReference(resolveAttachmentReferences[perPass.floatColor1Location],
				attachmentNdxes[4], VK_FORMAT_UNDEFINED, false);
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[4]))
	{
		preserveAttachments->push_back(attachmentNdxes[4]);
	}
	if (perPass.resolveFloatColor2)
	{
		initializeAttachmentReference(resolveAttachmentReferences[perPass.floatColor2Location],
				attachmentNdxes[5], VK_FORMAT_UNDEFINED, false);
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[5]))
	{
		preserveAttachments->push_back(attachmentNdxes[5]);
	}
	if (perPass.resolveIntColor)
	{
		initializeAttachmentReference(resolveAttachmentReferences[perPass.intColorLocation],
				attachmentNdxes[6], VK_FORMAT_UNDEFINED, false);
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[6]))
	{
		preserveAttachments->push_back(attachmentNdxes[6]);
	}

	// Account for single-sampled attachments in input attachments as well.
	if (!inputAttachmentReferences.empty())
	{
		if (attachmentNdxes[0] >= 0 && isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[0]))
			anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT;
		if (attachmentNdxes[1] >= 0 && isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[1]))
			anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT;
		if (attachmentNdxes[2] >= 0 && isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[2]))
			anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT;
		if (attachmentNdxes[3] >= 0 && isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[3]))
			anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT;
	}

	const bool needsMsrtss = anySingleSampledAttachmentsUsed && perPass.numSamples != VK_SAMPLE_COUNT_1_BIT;
	const bool needsDepthStencilResolve = perPass.resolveDepthStencil || (needsMsrtss && params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT && perPass.hasDepthStencil);

	if (needsDepthStencilResolve)
	{
		if (perPass.resolveDepthStencil)
		{
			initializeAttachmentReference(resolveAttachmentReferences.back(),
					attachmentNdxes[7], params.depthStencilFormat, false);
		}
		depthStencilResolve = VkSubpassDescriptionDepthStencilResolve
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,	// VkStructureType                  sType;
			DE_NULL,														// const void*                      pNext;
			perPass.depthStencilResolveMode,								// VkResolveModeFlagBits            depthResolveMode;
			perPass.depthStencilResolveMode,								// VkResolveModeFlagBits            stencilResolveMode;
			perPass.resolveDepthStencil
				? &resolveAttachmentReferences.back()
				: nullptr,													// const VkAttachmentReference2*    pDepthStencilResolveAttachment;
		};
	}
	else if (preserveAttachments && !isInAttachmentReferences(inputAttachmentReferences, attachmentNdxes[7]))
	{
		preserveAttachments->push_back(attachmentNdxes[7]);
	}

	VkSubpassDescription2 subpassDescription =
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,					// VkStructureType                 sType;
		needsDepthStencilResolve
			? &depthStencilResolve
			: DE_NULL,												// const void*                     pNext;
		(VkSubpassDescriptionFlags)0,								// VkSubpassDescriptionFlags       flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,							// VkPipelineBindPoint             pipelineBindPoint;
		0u,															// uint32_t                        viewMask;
		static_cast<deUint32>(inputAttachmentReferences.size()),	// uint32_t                        inputAttachmentCount;
		dataOrNullPtr(inputAttachmentReferences),					// const VkAttachmentReference2*   pInputAttachments;
		4u,															// uint32_t                        colorAttachmentCount;
		dataOrNullPtr(attachmentReferences),						// const VkAttachmentReference2*   pColorAttachments;
		dataOrNullPtr(resolveAttachmentReferences),					// const VkAttachmentReference2*   pResolveAttachments;
		perPass.hasDepthStencil
			? &attachmentReferences.back()
			: DE_NULL,												// const VkAttachmentReference2*   pDepthStencilAttachment;
		preserveAttachments
			? static_cast<deUint32>(preserveAttachments->size())
			: 0,													// uint32_t                        preserveAttachmentCount;
		preserveAttachments
			? dataOrNullPtr(*preserveAttachments)
			: nullptr,												// const uint32_t*                 pPreserveAttachments;
	};

	// Append MSRTSS to subpass desc
	msrtss = VkMultisampledRenderToSingleSampledInfoEXT{
		VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,	// VkStructureType			sType
		subpassDescription.pNext,											// const void*				pNext
		VK_TRUE,															// VkBool32					multisampledRenderToSingleSampledEnable
		perPass.numSamples,													// VkSampleCountFlagBits	rasterizationSamples
	};
	if (needsMsrtss)
		subpassDescription.pNext = &msrtss;

	subpasses.push_back(subpassDescription);
}

void addSubpassDependency(const deUint32 subpassNdx, std::vector<VkSubpassDependency2>& subpassDependencies)
{
	subpassDependencies.push_back(VkSubpassDependency2{
		VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,					// VkStructureType         sType;
		DE_NULL,												// const void*             pNext;
		subpassNdx - 1,											// uint32_t                srcSubpass;
		subpassNdx,												// uint32_t                dstSubpass;

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,				// VkPipelineStageFlags    srcStageMask;

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,				// VkPipelineStageFlags    dstStageMask;

		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,			// VkAccessFlags           srcAccessMask;

		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,			// VkAccessFlags           dstAccessMask;

		VK_DEPENDENCY_BY_REGION_BIT,							// VkDependencyFlags       dependencyFlags;
		0,														// int32_t                 viewOffset;
	});
}

void createRenderPassAndFramebuffer(Context&			context,
		WorkingData&									wd,
		TestObjects&									testObjects,
		const PipelineConstructionType					pipelineConstructionType,
	    const std::vector<VkImage>&						images,
		const std::vector<VkImageView>&					attachments,
		const std::vector<VkAttachmentDescription2>&	attachmentDescriptions,
		const std::vector<VkSubpassDescription2>&		subpasses,
		const std::vector<VkSubpassDependency2>&		subpassDependencies)
{
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const VkDevice					device		= context.getDevice();

	const VkRenderPassCreateInfo2 renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
		dataOrNullPtr(attachmentDescriptions),					// const VkAttachmentDescription2*	pAttachments;
		static_cast<deUint32>(subpasses.size()),				// deUint32							subpassCount;
		dataOrNullPtr(subpasses),								// const VkSubpassDescription2*		pSubpasses;
		static_cast<deUint32>(subpassDependencies.size()),		// deUint32							dependencyCount;
		dataOrNullPtr(subpassDependencies),						// const VkSubpassDependency2*		pDependencies;
		0u,														// uint32_t                         correlatedViewMaskCount;
		DE_NULL,												// const uint32_t*                  pCorrelatedViewMasks;
	};

	testObjects.renderPassFramebuffers.emplace_back(RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo));
	testObjects.renderPassFramebuffers.back().createFramebuffer(vk, device, static_cast<deUint32>(attachments.size()), dataOrNullPtr(images), dataOrNullPtr(attachments), wd.framebufferSize.x(), wd.framebufferSize.y());
}

void createWorkingData (Context& context, const TestParams& params, WorkingData& wd)
{
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const VkDevice					device		= context.getDevice();
	MovePtr<Allocator>				allocator   = MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));

	// Create images
	{
		// TODO: change image types to be nonuniform, for example: mip 1 of 2D image, mip 2/level 3 of 2D array image, etc.
		wd.floatColor1.allocate	(vk, device, allocator, params.floatColor1Format,	wd.framebufferSize, params.numFloatColor1Samples,	colorImageUsageFlags,			VK_IMAGE_ASPECT_COLOR_BIT, 1, true);
		wd.floatColor2.allocate	(vk, device, allocator, params.floatColor2Format,	wd.framebufferSize, params.numFloatColor2Samples,	colorImageUsageFlags,			VK_IMAGE_ASPECT_COLOR_BIT, 1, true);
		wd.intColor.allocate	(vk, device, allocator, params.intColorFormat,		wd.framebufferSize, params.numIntColorSamples,		colorImageUsageFlags,			VK_IMAGE_ASPECT_COLOR_BIT, 1, true);
		wd.depthStencil.allocate(vk, device, allocator, params.depthStencilFormat,	wd.framebufferSize, params.numDepthStencilSamples,	depthStencilImageUsageFlags,	getDepthStencilAspectFlags(params.depthStencilFormat), 1, true);

		if (isDepthFormat(params.depthStencilFormat))
			wd.depthOnlyImageView	= wd.depthStencil.makeView(vk, device, params.depthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		if (isStencilFormat(params.depthStencilFormat))
			wd.stencilOnlyImageView	= wd.depthStencil.makeView(vk, device, params.depthStencilFormat, VK_IMAGE_ASPECT_STENCIL_BIT, 1);

		if (params.numFloatColor1Samples != VK_SAMPLE_COUNT_1_BIT)
		{
			wd.floatResolve1.allocate(vk, device, allocator, params.floatColor1Format, wd.framebufferSize, VK_SAMPLE_COUNT_1_BIT, colorImageUsageFlags, VK_IMAGE_ASPECT_COLOR_BIT, 1, false);
		}

		if (params.numFloatColor2Samples != VK_SAMPLE_COUNT_1_BIT)
		{
			wd.floatResolve2.allocate(vk, device, allocator, params.floatColor2Format, wd.framebufferSize, VK_SAMPLE_COUNT_1_BIT, colorImageUsageFlags, VK_IMAGE_ASPECT_COLOR_BIT, 1, false);
		}

		if (params.numIntColorSamples != VK_SAMPLE_COUNT_1_BIT)
		{
			wd.intResolve.allocate(vk, device, allocator, params.intColorFormat, wd.framebufferSize, VK_SAMPLE_COUNT_1_BIT, colorImageUsageFlags, VK_IMAGE_ASPECT_COLOR_BIT, 1, false);
		}

		if (params.numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT)
		{
			wd.depthStencilResolve.allocate(vk, device, allocator, params.depthStencilFormat, wd.framebufferSize, VK_SAMPLE_COUNT_1_BIT, depthStencilImageUsageFlags, getDepthStencilAspectFlags(params.depthStencilFormat), 1, false);

			if (isDepthFormat(params.depthStencilFormat))
				wd.depthOnlyResolveImageView	= wd.depthStencilResolve.makeView(vk, device, params.depthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

			if (isStencilFormat(params.depthStencilFormat))
				wd.stencilOnlyResolveImageView	= wd.depthStencilResolve.makeView(vk, device, params.depthStencilFormat, VK_IMAGE_ASPECT_STENCIL_BIT, 1);
		}

		wd.verify.allocate	(vk, device, allocator, VK_FORMAT_R8G8B8A8_UNORM, wd.framebufferSize, VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 5, false);
	}

	// Create vertex and verification buffers
	{
		// A fullscreen triangle
		const std::vector<Vec4> vertices =
		{
			Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			Vec4( 3.0f, -1.0f, 0.0f, 1.0f),
			Vec4(-1.0f,  3.0f, 0.0f, 1.0f),
		};

		const VkDeviceSize	vertexBufferSize = static_cast<VkDeviceSize>(sizeof(vertices[0]) * vertices.size());
		wd.vertexBuffer			= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		wd.vertexBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(wd.vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices), static_cast<std::size_t>(vertexBufferSize));
		flushMappedMemoryRange(vk, device, wd.vertexBufferAlloc->getMemory(), wd.vertexBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		// Initialize the verification data with 0.
		const VerificationResults results = {};

		wd.verificationBuffer		= makeBuffer(vk, device, sizeof(results), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		wd.verificationBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.verificationBuffer, MemoryRequirement::HostVisible);

		deMemcpy(wd.verificationBufferAlloc->getHostPtr(), &results, sizeof(results));
		flushMappedMemoryRange(vk, device, wd.verificationBufferAlloc->getMemory(), wd.verificationBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		wd.singleVerificationBuffer		= makeBuffer(vk, device, sizeof(results), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		wd.singleVerificationBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.singleVerificationBuffer, MemoryRequirement::HostVisible);

		deMemcpy(wd.singleVerificationBufferAlloc->getHostPtr(), &results, sizeof(results));
		flushMappedMemoryRange(vk, device, wd.singleVerificationBufferAlloc->getMemory(), wd.singleVerificationBufferAlloc->getOffset(), VK_WHOLE_SIZE);
	}
}

void checkRequirements (Context& context, TestParams params)
{
	const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const vk::InstanceInterface&	instanceInterface	= context.getInstanceInterface();

	checkPipelineConstructionRequirements(instanceInterface, physicalDevice, params.pipelineConstructionType);

	context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
	context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	if (params.dynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (params.isMultisampledRenderToSingleSampled)
	{
		context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_single_sampled");

		// Check extension feature
		{
			VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT	msrtssFeatures =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT,
				DE_NULL,
				VK_FALSE,
			};
			VkPhysicalDeviceFeatures2						physicalDeviceFeatures =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				&msrtssFeatures,
				{},
			};

			instanceInterface.getPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

			if (msrtssFeatures.multisampledRenderToSingleSampled != VK_TRUE)
			{
				TCU_THROW(NotSupportedError, "multisampledRenderToSingleSampled not supported");
			}
		}
	}

	// Check whether formats are supported with the requested usage and sample counts.
	{
		VkImageFormatProperties imageProperties;
		checkImageRequirements (context, params.floatColor1Format,
				VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
				colorImageUsageFlags, params.numFloatColor1Samples, imageProperties);
		if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			for (const TestParams::PerPass& perPass : params.perPass)
				if (perPass.floatColor1Location >= 0 && (imageProperties.sampleCounts & perPass.numSamples) != perPass.numSamples)
					TCU_THROW(NotSupportedError, (de::toString(params.floatColor1Format) + ": sample count not supported").c_str());
	}
	{
		VkImageFormatProperties imageProperties;
		checkImageRequirements (context, params.floatColor2Format,
				VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
				colorImageUsageFlags, params.numFloatColor2Samples, imageProperties);
		if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			for (const TestParams::PerPass& perPass : params.perPass)
				if (perPass.floatColor2Location >= 0 && (imageProperties.sampleCounts & perPass.numSamples) != perPass.numSamples)
					TCU_THROW(NotSupportedError, (de::toString(params.floatColor2Format) + ": sample count not supported").c_str());
	}
	{
		VkImageFormatProperties imageProperties;
		checkImageRequirements (context, params.intColorFormat,
				VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
				colorImageUsageFlags, params.numIntColorSamples, imageProperties);
		if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			for (const TestParams::PerPass& perPass : params.perPass)
				if (perPass.intColorLocation >= 0 && (imageProperties.sampleCounts & perPass.numSamples) != perPass.numSamples)
					TCU_THROW(NotSupportedError, (de::toString(params.intColorFormat) + ": sample count not supported").c_str());
	}
	{
		VkImageFormatProperties imageProperties;
		checkImageRequirements (context, params.depthStencilFormat,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
				depthStencilImageUsageFlags, params.numDepthStencilSamples, imageProperties);
		if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			for (const TestParams::PerPass& perPass : params.perPass)
				if (perPass.hasDepthStencil && (imageProperties.sampleCounts & perPass.numSamples) != perPass.numSamples)
					TCU_THROW(NotSupportedError, (de::toString(params.depthStencilFormat) + ": sample count not supported").c_str());
	}

	// Perform query to get supported depth/stencil resolve modes.
	VkPhysicalDeviceDepthStencilResolveProperties dsResolveProperties = {};
	dsResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

	VkPhysicalDeviceProperties2 deviceProperties = {};
	deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties.pNext = &dsResolveProperties;

	instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

	for (const TestParams::PerPass& perPass : params.perPass)
	{
		// Check whether sample counts used for rendering are acceptable
		const bool	checkColor		= perPass.floatColor1Location >= 0 || perPass.floatColor2Location >= 0 || perPass.intColorLocation >= 0;
		const bool	checkDepth		= perPass.hasDepthStencil && isDepthFormat(params.depthStencilFormat);
		const bool	checkStencil	= perPass.hasDepthStencil && isStencilFormat(params.depthStencilFormat);
		checkSampleRequirements(context, perPass.numSamples, checkColor, checkDepth, checkStencil);

		// Check whether depth/stencil resolve mode is supported
		if (perPass.depthStencilResolveMode != VK_RESOLVE_MODE_NONE &&
			((dsResolveProperties.supportedDepthResolveModes & perPass.depthStencilResolveMode) == 0 ||
			 (dsResolveProperties.supportedStencilResolveModes & perPass.depthStencilResolveMode) == 0))
		{
			TCU_THROW(NotSupportedError, "Depth/stencil resolve mode not supported");
		}
	}
}

void checkHasMsrtss (Context& context, VkFormat)
{
	context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_single_sampled");
}

void generateRandomClearValues(de::Random& rng, const TestParams& params, VkClearValue clearValues[4], bool smallValues)
{
	const bool		usesSignedIntFormat	= params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;

	const float		minFloatValue		= 0.05f;
	const float		maxFloatValue		= smallValues ? 0.1f : 0.95f;
	const deUint32	minIntValue			= smallValues ? 20 : 5000;
	const deUint32	maxIntValue			= smallValues ? 100 : 10000;
	const float		minDepthValue		= 0.05f;
	const float		maxDepthValue		= smallValues ? 0.1f : 0.5f;
	const deUint32	minStencilValue		= 0x10;
	const deUint32	maxStencilValue		= 0x20;

	clearValues[0].color.float32[0] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[0].color.float32[1] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[0].color.float32[2] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[0].color.float32[3] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[1].color.float32[0] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[1].color.float32[1] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[1].color.float32[2] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[1].color.float32[3] = rng.getFloat(minFloatValue, maxFloatValue);
	clearValues[2].color.int32[0] = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
	clearValues[2].color.int32[1] = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
	clearValues[2].color.int32[2] = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
	clearValues[2].color.int32[3] = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
	clearValues[3].depthStencil.depth = rng.getFloat(minDepthValue, maxDepthValue);
	clearValues[3].depthStencil.stencil = rng.getInt(minStencilValue, maxStencilValue);
}

void clearImagesBeforeDraw(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();

	const VkImageMemoryBarrier	imageBarrierTemplate =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
		DE_NULL,																// const void*                pNext;
		0,																		// VkAccessFlags              srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,											// VkAccessFlags              dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,									// VkImageLayout              newLayout;
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
		0,																		// VkImage                    image;
		makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),	// VkImageSubresourceRange    subresourceRange;
	};

	VkImageMemoryBarrier	preClearBarriers[4]		= { imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate };
	preClearBarriers[0].image						= *wd.floatColor1.image;
	preClearBarriers[1].image						= *wd.floatColor2.image;
	preClearBarriers[2].image						= *wd.intColor.image;
	preClearBarriers[3].image						= *wd.depthStencil.image;
	preClearBarriers[3].subresourceRange.aspectMask	= getDepthStencilAspectFlags(params.depthStencilFormat);

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(preClearBarriers), preClearBarriers);

	vk.cmdClearColorImage(*testObjects.cmdBuffer, *wd.floatColor1.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &params.clearValues[0].color, 1, &preClearBarriers[0].subresourceRange);
	vk.cmdClearColorImage(*testObjects.cmdBuffer, *wd.floatColor2.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &params.clearValues[1].color, 1, &preClearBarriers[1].subresourceRange);
	vk.cmdClearColorImage(*testObjects.cmdBuffer, *wd.intColor.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &params.clearValues[2].color, 1, &preClearBarriers[2].subresourceRange);
	vk.cmdClearDepthStencilImage(*testObjects.cmdBuffer, *wd.depthStencil.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &params.clearValues[3].depthStencil, 1, &preClearBarriers[3].subresourceRange);

	const VkMemoryBarrier postClearBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,										// VkStructureType    sType;
		DE_NULL,																// const void*        pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,											// VkAccessFlags      srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,						// VkAccessFlags      dstAccessMask;
	};

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			0u, 1u, &postClearBarrier, 0u, DE_NULL, 0u, DE_NULL);
}

void getDrawRegions(WorkingData& wd, UVec4 regions[RegionCount])
{
	static_assert(RegionCount == 4, "Update this function to generate the correct number of regions");

	UVec2 oneThirdRenderAreaSize(wd.renderArea.z() / 3, wd.renderArea.w() / 3);
	UVec2 twoThirdsRenderAreaSize(wd.renderArea.z() - oneThirdRenderAreaSize.x(), wd.renderArea.w() - oneThirdRenderAreaSize.y());
	UVec2 renderAreaSplit(wd.renderArea.x() + oneThirdRenderAreaSize.x(), wd.renderArea.y() + oneThirdRenderAreaSize.y());

	regions[0] = UVec4(wd.renderArea.x(), wd.renderArea.y(), oneThirdRenderAreaSize.x(), oneThirdRenderAreaSize.y());
	regions[1] = UVec4(renderAreaSplit.x(), wd.renderArea.y(), twoThirdsRenderAreaSize.x(), oneThirdRenderAreaSize.y());
	regions[2] = UVec4(wd.renderArea.x(), renderAreaSplit.y(), oneThirdRenderAreaSize.x(), twoThirdsRenderAreaSize.y());
	regions[3] = UVec4(renderAreaSplit.x(), renderAreaSplit.y(), twoThirdsRenderAreaSize.x(), twoThirdsRenderAreaSize.y());
}

void startRenderPass(Context& context, WorkingData&wd, TestObjects& testObjects, const deUint32 clearValueCount, const VkClearValue* clearValues)
{
	const DeviceInterface&	vk	= context.getDeviceInterface();

	const VkRect2D renderArea =
	{
		{ static_cast<deInt32>(wd.renderArea.x()), static_cast<deInt32>(wd.renderArea.y()) },
		{ wd.renderArea.z(), wd.renderArea.w() }
	};

	testObjects.renderPassFramebuffers.back().begin(vk, *testObjects.cmdBuffer, renderArea, clearValueCount, clearValues);
}

void startRendering (Context&									context,
					 const TestParams&							params,
					 WorkingData&								wd,
					 TestObjects&								testObjects,
					 uint32_t									colorAttachmentCount,
					 std::vector<VkRenderingAttachmentInfo>&	colorAttachmentInfos,
					 VkRenderingAttachmentInfo&					depthStencilAttachmentInfo,
					 uint32_t									renderPassNdx)
{
	const DeviceInterface&		vk			= context.getDeviceInterface();
	const TestParams::PerPass&	perPass		= params.perPass[renderPassNdx];

	bool anySingleSampledAttachmentsUsed = false;
	if (perPass.floatColor1Location >= 0)
	{
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT;
	}
	if (perPass.floatColor2Location >= 0)
	{
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT;
	}
	if (perPass.intColorLocation >= 0)
	{
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT;
	}
	if (perPass.hasDepthStencil)
	{
		anySingleSampledAttachmentsUsed	= anySingleSampledAttachmentsUsed || params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT;
	}

	// Append MSRTSS to subpass desc
	VkMultisampledRenderToSingleSampledInfoEXT msrtss =
	{
		VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,	// VkStructureType			sType
		DE_NULL,															// const void*				pNext
		VK_TRUE,															// VkBool32					multisampledRenderToSingleSampledEnable
		perPass.numSamples													// VkSampleCountFlagBits	rasterizationSamples
	};

	const VkRect2D renderArea =
	{
		{ static_cast<deInt32>(wd.renderArea.x()), static_cast<deInt32>(wd.renderArea.y()) },
		{ wd.renderArea.z(), wd.renderArea.w() }
	};

	const bool useDepthStencil = params.usesDepthStencilInPass(renderPassNdx);

	VkRenderingInfo renderingInfo =
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO,									// VkStructureType						sType
		DE_NULL,															// const void*							pNext
		(VkRenderingFlags) 0,												// VkRenderingFlags						flags
		renderArea,															// VkRect2D								renderArea
		1u,																	// uint32_t								layerCount
		0u,																	// uint32_t								viewMask
		colorAttachmentCount,												// uint32_t								colorAttachmentCount
		colorAttachmentInfos.data(),										// const VkRenderingAttachmentInfo*		pColorAttachments
		useDepthStencil && isDepthFormat(params.depthStencilFormat) ?
			&depthStencilAttachmentInfo : DE_NULL,							// const VkRenderingAttachmentInfo*		pDepthAttachment
		useDepthStencil && isStencilFormat(params.depthStencilFormat) ?
			&depthStencilAttachmentInfo : DE_NULL							// const VkRenderingAttachmentInfo*		pStencilAttachment
	};

	if (anySingleSampledAttachmentsUsed && perPass.numSamples != VK_SAMPLE_COUNT_1_BIT)
		renderingInfo.pNext = &msrtss;

	vk.cmdBeginRendering(*testObjects.cmdBuffer, &renderingInfo);
}

void postDrawBarrier(Context& context, TestObjects& testObjects)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();

	const VkMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,															// VkStructureType    sType;
		DE_NULL,																					// const void*        pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags      srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,																	// VkAccessFlags      dstAccessMask;
	};

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 1u, &barrier, 0u, DE_NULL, 0u, DE_NULL);
}

void setupVerifyDescriptorSetAndPipeline(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects, const VkPushConstantRange* pushConstantRange,
		Move<VkPipelineLayout>& verifyPipelineLayout)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	testObjects.descriptorPools.emplace_back(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5u)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	testObjects.descriptorSets.emplace_back(makeDescriptorSet(vk, device, *testObjects.descriptorPools.back(), *descriptorSetLayout));

	const VkDescriptorBufferInfo	resultBufferInfo	= makeDescriptorBufferInfo(*wd.verificationBuffer, 0ull, sizeof(VerificationResults));
	const VkDescriptorImageInfo		color1ImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.getResolvedFloatColorImage1View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		color2ImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.getResolvedFloatColorImage2View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		color3ImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.getResolvedIntColorImageView(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		depthImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.getResolvedDepthOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		stencilImageInfo	= makeDescriptorImageInfo(DE_NULL, *wd.getResolvedStencilOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		verifyImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.verify.view, VK_IMAGE_LAYOUT_GENERAL);

	DescriptorSetUpdateBuilder	builder;

	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color1ImageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color2ImageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color3ImageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(4u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &depthImageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(5u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &stencilImageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(6u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verifyImageInfo);

	builder.update(vk, device);

	const Unique<VkShaderModule>	verifyModule			(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0u));

	verifyPipelineLayout	= makePipelineLayout(vk, device, 1, &*descriptorSetLayout, 1, pushConstantRange);

	testObjects.computePipelines.push_back(PipelineSp(new Unique<VkPipeline>(makeComputePipeline(vk, device, *verifyPipelineLayout, *verifyModule))));

	vk.cmdBindPipeline(*testObjects.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **testObjects.computePipelines.back());
	vk.cmdBindDescriptorSets(*testObjects.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *verifyPipelineLayout, 0u, 1u, &testObjects.descriptorSets.back().get(), 0u, DE_NULL);
}

void postVerifyBarrier(Context& context, TestObjects& testObjects, const Move<VkBuffer>& verificationBuffer)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();

	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
		DE_NULL,										// const void*        pNext;
		VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags      srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
		*verificationBuffer,							// VkBuffer           buffer;
		0ull,											// VkDeviceSize       offset;
		VK_WHOLE_SIZE,									// VkDeviceSize       size;
	};

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
			0u, DE_NULL, 1u, &barrier, 0u, DE_NULL);
}

void dispatchVerifyConstantColor(Context&					context,
								 TestObjects&				testObjects,
								 const Move<VkImageView>&	imageView,
								 const VkImageLayout		layout,
								 const Move<VkImageView>&	verifyImageView,
								 const Move<VkBuffer>&		verificationBuffer,
								 const deUint32				pushConstantSize,
								 const void*				pushConstants,
								 const std::string&			shaderName)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	// Set up descriptor set
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,		VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	testObjects.descriptorPools.emplace_back(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	testObjects.descriptorSets.emplace_back(makeDescriptorSet(vk, device, *testObjects.descriptorPools.back(), *descriptorSetLayout));

	const VkDescriptorBufferInfo	resultBufferInfo	= makeDescriptorBufferInfo(*verificationBuffer, 0ull, sizeof(VerificationResults));
	const VkDescriptorImageInfo		imageInfo			= makeDescriptorImageInfo(DE_NULL, *imageView, layout);
	const VkDescriptorImageInfo		verifyImageInfo		= makeDescriptorImageInfo(DE_NULL, *verifyImageView, VK_IMAGE_LAYOUT_GENERAL);

	DescriptorSetUpdateBuilder	builder;

	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verifyImageInfo);

	builder.update(vk, device);

	// Setup pipeline
	const VkPushConstantRange& verifyPushConstantRange =
	{
		VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags    stageFlags;
		0,								// uint32_t              offset;
		pushConstantSize,				// uint32_t              size;
	};

	const Unique<VkShaderModule>	verifyModule			(createShaderModule(vk, device, context.getBinaryCollection().get(shaderName), 0u));
	const Unique<VkPipelineLayout>	verifyPipelineLayout	(makePipelineLayout(vk, device, 1, &*descriptorSetLayout, 1, &verifyPushConstantRange));

	testObjects.computePipelines.push_back(PipelineSp(new Unique<VkPipeline>(makeComputePipeline(vk, device, *verifyPipelineLayout, *verifyModule))));

	vk.cmdBindPipeline(*testObjects.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **testObjects.computePipelines.back());
	vk.cmdBindDescriptorSets(*testObjects.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *verifyPipelineLayout, 0u, 1u, &testObjects.descriptorSets.back().get(), 0u, DE_NULL);

	const VkMemoryBarrier preVerifyBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,		// VkStructureType    sType;
		DE_NULL,								// const void*        pNext;
		VK_ACCESS_SHADER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
		VK_ACCESS_SHADER_WRITE_BIT |
			VK_ACCESS_SHADER_READ_BIT,			// VkAccessFlags      dstAccessMask;
	};

	vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0,
			1u, &preVerifyBarrier, 0u, DE_NULL, 0u, DE_NULL);

	// Area is always the first uvec4
	const UVec4* area = static_cast<const UVec4*>(pushConstants);

	vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize, pushConstants);
	vk.cmdDispatch(*testObjects.cmdBuffer, (area->z() + 7) / 8, (area->w() + 7) / 8, 1);

	postVerifyBarrier(context, testObjects, verificationBuffer);
}

void testStart(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	de::Random	rng(params.rngSeed);

	wd.framebufferSize	= UVec2(rng.getInt(60, 80), rng.getInt(48, 64));
	wd.renderArea		= UVec4(0, 0, wd.framebufferSize.x(), wd.framebufferSize.y());
	if (!params.renderToWholeFramebuffer)
	{
		wd.renderArea.x() += rng.getInt(5, 15);
		wd.renderArea.y() += rng.getInt(5, 15);
		wd.renderArea.z() -= wd.renderArea.x() + rng.getInt(2, 12);
		wd.renderArea.w() -= wd.renderArea.y() + rng.getInt(2, 12);
	}

	createWorkingData(context, params, wd);

	testObjects.beginCommandBuffer();

	const DeviceInterface&	vk				= context.getDeviceInterface();

	// Clear verify image
	{
		VkImageMemoryBarrier	clearBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
			DE_NULL,																// const void*                pNext;
			0,																		// VkAccessFlags              srcAccessMask;
			VK_ACCESS_TRANSFER_WRITE_BIT,											// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,									// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
			*wd.verify.image,														// VkImage                    image;
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 5u),	// VkImageSubresourceRange    subresourceRange;
		};

		vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &clearBarrier);

		VkClearColorValue clearToBlack;
		clearToBlack.float32[0] = 0;
		clearToBlack.float32[1] = 0;
		clearToBlack.float32[2] = 0;
		clearToBlack.float32[3] = 1.0;
		vk.cmdClearColorImage(*testObjects.cmdBuffer, *wd.verify.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearToBlack, 1, &clearBarrier.subresourceRange);
	}

	// Transition it to GENERAL
	{
		VkImageMemoryBarrier	verifyBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
			DE_NULL,																// const void*                pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,											// VkAccessFlags              srcAccessMask;
			VK_ACCESS_SHADER_WRITE_BIT,												// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,									// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_GENERAL,												// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
			*wd.verify.image,														// VkImage                    image;
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 5u),	// VkImageSubresourceRange    subresourceRange;
		};

		vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &verifyBarrier);
	}
}

void testEnd(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	// If not rendering to the whole framebuffer and the images were cleared before the render pass, verify that the area outside the render pass is untouched.
	const bool	verifyOutsideRenderArea = params.clearBeforeRenderPass && !params.renderToWholeFramebuffer;
	if (verifyOutsideRenderArea)
	{
		const DeviceInterface&	vk				= context.getDeviceInterface();
		const VkDevice			device			= context.getDevice();

		const UVec4 verifyAreas[] = {
			UVec4(0, 0, wd.framebufferSize.x(), wd.renderArea.y()),
			UVec4(0, wd.renderArea.y(), wd.renderArea.x(), wd.renderArea.w()),
			UVec4(wd.renderArea.x() + wd.renderArea.z(), wd.renderArea.y(), wd.framebufferSize.x() - wd.renderArea.x() - wd.renderArea.z(), wd.renderArea.w()),
			UVec4(0, wd.renderArea.y() + wd.renderArea.w(), wd.framebufferSize.x(), wd.framebufferSize.y() - wd.renderArea.y() - wd.renderArea.w()),
		};

		for (deUint32 areaNdx = 0; areaNdx < DE_LENGTH_OF_ARRAY(verifyAreas); ++areaNdx)
		{
			if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			{
				const VerifySingleFloatPushConstants	verifyColor1 =
				{
					verifyAreas[areaNdx],
					Vec4(params.clearValues[0].color.float32[0], params.clearValues[0].color.float32[1], params.clearValues[0].color.float32[2], params.clearValues[0].color.float32[3]),
					0,
				};
				dispatchVerifyConstantColor(context, testObjects, wd.getResolvedFloatColorImage1View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.singleVerificationBuffer,
						static_cast<deUint32>(sizeof(verifyColor1)), &verifyColor1, "comp_singleFloat");
			}

			if (params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT)
			{
				const VerifySingleFloatPushConstants	verifyColor2 =
				{
					verifyAreas[areaNdx],
					Vec4(params.clearValues[1].color.float32[0], params.clearValues[1].color.float32[1], params.clearValues[1].color.float32[2], params.clearValues[1].color.float32[3]),
					1,
				};
				dispatchVerifyConstantColor(context, testObjects, wd.getResolvedFloatColorImage2View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.singleVerificationBuffer,
						static_cast<deUint32>(sizeof(verifyColor2)), &verifyColor2, "comp_singleFloat");
			}

			if (params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT)
			{
				const VerifySingleIntPushConstants	verifyColor3 =
				{
					verifyAreas[areaNdx],
					IVec4(params.clearValues[2].color.int32[0], params.clearValues[2].color.int32[1], params.clearValues[2].color.int32[2], params.clearValues[2].color.int32[3]),
					2,
				};
				dispatchVerifyConstantColor(context, testObjects, wd.getResolvedIntColorImageView(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.singleVerificationBuffer,
						static_cast<deUint32>(sizeof(verifyColor3)), &verifyColor3, "comp_singleInt");
			}

			if (params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT && isDepthFormat(params.depthStencilFormat))
			{
				const VerifySingleDepthPushConstants	verifyDepth =
				{
					verifyAreas[areaNdx],
					params.clearValues[3].depthStencil.depth,
				};
				dispatchVerifyConstantColor(context, testObjects, wd.getResolvedDepthOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, wd.verify.view, wd.singleVerificationBuffer,
						static_cast<deUint32>(sizeof(verifyDepth)), &verifyDepth, "comp_singleDepth");
			}

			if (params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT && isStencilFormat(params.depthStencilFormat))
			{
				const VerifySingleStencilPushConstants	verifyStencil =
				{
					verifyAreas[areaNdx],
					params.clearValues[3].depthStencil.stencil,
				};
				dispatchVerifyConstantColor(context, testObjects, wd.getResolvedStencilOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, wd.verify.view, wd.singleVerificationBuffer,
						static_cast<deUint32>(sizeof(verifyStencil)), &verifyStencil, "comp_singleStencil");
			}
		}

		invalidateAlloc(vk, device, *wd.singleVerificationBufferAlloc);
	}

	testObjects.submitCommandsAndWait();
}

tcu::TestStatus verify(Context& context, const TestParams& params, WorkingData& wd)
{
	bool drawsToColor1			= false;
	bool drawsToColor2			= false;
	bool drawsToColor3			= false;
	bool drawsToDepthStencil	= false;
	for (const TestParams::PerPass& perPass : params.perPass)
	{
		if (perPass.floatColor1Location >= 0)
			drawsToColor1		= true;
		if (perPass.floatColor2Location >= 0)
			drawsToColor2		= true;
		if (perPass.intColorLocation >= 0)
			drawsToColor3		= true;
		if (perPass.hasDepthStencil)
			drawsToDepthStencil	= true;
	}

	logTestImages(context, params, wd, drawsToColor1, drawsToColor2, drawsToColor3, drawsToDepthStencil);

	// Verify draw call results
	{
		const VerificationResults*	const	results		= static_cast<const VerificationResults*>(wd.verificationBufferAlloc->getHostPtr());
		const deUint32						totalPixels	= wd.renderArea.z() * wd.renderArea.w();
		bool								allOk		= true;
		const char*							errorDelim	= "";
		std::string							errorMsg	= "Incorrect multisampled rendering for ";

		if (drawsToColor1)
			if (!checkAndReportError(context, results->color1Verification, totalPixels, "color attachment 1"))
			{
				errorMsg	+= errorDelim;
				errorMsg	+= "color attachment 1";
				errorDelim	= ", ";
				allOk		= false;
			}

		if (drawsToColor2)
			if (!checkAndReportError(context, results->color2Verification, totalPixels, "color attachment 2"))
			{
				errorMsg	+= errorDelim;
				errorMsg	+= "color attachment 2";
				errorDelim	= ", ";
				allOk		= false;
			}

		if (drawsToColor3)
			if (!checkAndReportError(context, results->color3Verification, totalPixels, "color attachment 3"))
			{
				errorMsg	+= errorDelim;
				errorMsg	+= "color attachment 3";
				errorDelim	= ", ";
				allOk		= false;
			}

		if (drawsToDepthStencil && isDepthFormat(params.depthStencilFormat))
			if (!checkAndReportError(context, results->depthVerification, totalPixels, "depth attachment"))
			{
				errorMsg	+= errorDelim;
				errorMsg	+= "depth attachment";
				errorDelim	= ", ";
				allOk		= false;
			}

		if (drawsToDepthStencil && isStencilFormat(params.depthStencilFormat))
			if (!checkAndReportError(context, results->stencilVerification, totalPixels, "stencil attachment"))
			{
				errorMsg	+= errorDelim;
				errorMsg	+= "stencil attachment";
				errorDelim	= ", ";
				allOk		= false;
			}

		if (!allOk)
		{
			logVerifyImages(context, params, wd, drawsToColor1, drawsToColor2, drawsToColor3, drawsToDepthStencil);
			return tcu::TestStatus::fail(errorMsg);
		}
	}

	const bool	verifyOutsideRenderArea = params.clearBeforeRenderPass && !params.renderToWholeFramebuffer;
	if (verifyOutsideRenderArea)
	{
		const VerificationResults*	const	results		= static_cast<const VerificationResults*>(wd.singleVerificationBufferAlloc->getHostPtr());
		const deUint32						totalPixels	= wd.framebufferSize.x() * wd.framebufferSize.y() - wd.renderArea.z() * wd.renderArea.w();
		bool								allOk		= true;

		if (params.numFloatColor1Samples == VK_SAMPLE_COUNT_1_BIT)
			allOk = checkAndReportError(context, results->color1Verification, totalPixels, "color attachment 1 (outside render area)") && allOk;
		if (params.numFloatColor2Samples == VK_SAMPLE_COUNT_1_BIT)
			allOk = checkAndReportError(context, results->color2Verification, totalPixels, "color attachment 2 (outside render area)") && allOk;
		if (params.numIntColorSamples == VK_SAMPLE_COUNT_1_BIT)
			allOk = checkAndReportError(context, results->color3Verification, totalPixels, "color attachment 3 (outside render area)") && allOk;
		if (params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT && isDepthFormat(params.depthStencilFormat))
			allOk = checkAndReportError(context, results->depthVerification, totalPixels, "depth attachment (outside render area)") && allOk;
		if (params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT && isStencilFormat(params.depthStencilFormat))
			allOk = checkAndReportError(context, results->stencilVerification, totalPixels, "stencil attachment (outside render area)") && allOk;

		if (!allOk)
		{
			logVerifyImages(context, params, wd, drawsToColor1, drawsToColor2, drawsToColor3, drawsToDepthStencil);
			return tcu::TestStatus::fail("Detected corruption outside render area");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

void initConstantColorVerifyPrograms (SourceCollections& programCollection, const TestParams params)
{
	const bool	usesSignedIntFormat		= params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;

	// Compute shader - Verify outside render area is intact (float colors)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    vec4 color;\n"
			<< "    uint attachmentNdx;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform texture2D colorImage;\n"
			<< "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool v4matches(vec4 a, vec4 b, float error)\n"
			<< "{\n"
			<< "    return all(lessThan(abs(a - b), vec4(error)));\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "\n"
			<< "    vec4 result = vec4(1, 0, 0, 1);\n"
			<< "    vec4 color = texelFetch(colorImage, ivec2(coords), 0);\n"
			<< "    if (v4matches(color, params.color, 0.01))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[params.attachmentNdx], 1);\n"
			<< "        result = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, params.attachmentNdx), result);\n"
			<< "}\n";

		programCollection.glslSources.add("comp_singleFloat") << glu::ComputeSource(src.str());
	}

	// Compute shader - Verify outside render area is intact (int colors)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    ivec4 color;\n"
			<< "    uint attachmentNdx;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform " << (usesSignedIntFormat ? "i" : "u") << "texture2D colorImage;\n"
			<< "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool i4matches(ivec4 a, ivec4 b, int error)\n"
			<< "{\n"
			<< "    return all(lessThanEqual(abs(a - b), ivec4(error)));\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "\n"
			<< "    vec4 result = vec4(1, 0, 0, 1);\n"
			<< "    ivec4 color = ivec4(texelFetch(colorImage, ivec2(coords), 0));\n"
			<< "    if (i4matches(color, params.color, 0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[params.attachmentNdx], 1);\n"
			<< "        result = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, params.attachmentNdx), result);\n"
			<< "}\n";

		programCollection.glslSources.add("comp_singleInt") << glu::ComputeSource(src.str());
	}

	// Compute shader - Verify outside render area is intact (depth)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    float depthData;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform texture2D depthImage;\n"
			<< "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool fmatches(float a, float b, float error)\n"
			<< "{\n"
			<< "    return abs(a - b) < error;\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "\n"
			<< "    vec4 result = vec4(1, 0, 0, 1);\n"
			<< "    float depth  = texelFetch(depthImage, ivec2(coords), 0).r;\n"
			<< "    if (fmatches(depth, params.depthData, 0.01))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.depthVerification, 1);\n"
			<< "        result = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 3), result);\n"
			<< "}\n";

		programCollection.glslSources.add("comp_singleDepth") << glu::ComputeSource(src.str());
	}

	// Compute shader - Verify outside render area is intact (stencil)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    uint stencilData;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform utexture2D stencilImage;\n"
			<< "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool umatches(uint a, uint b, uint error)\n"
			<< "{\n"
			<< "    return abs(a - b) <= error;\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "\n"
			<< "    vec4 result = vec4(1, 0, 0, 1);\n"
			<< "    uint stencil = texelFetch(stencilImage, ivec2(coords), 0).r;\n"
			<< "    if (umatches(stencil, params.stencilData, 0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.stencilVerification, 1);\n"
			<< "        result = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 4), result);\n"
			<< "}\n";

		programCollection.glslSources.add("comp_singleStencil") << glu::ComputeSource(src.str());
	}
}

void initBasicPrograms (SourceCollections& programCollection, const TestParams params)
{
	// Vertex shader - position
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	const bool		usesSignedIntFormat	= params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;
	const char*     intTypePrefix		= usesSignedIntFormat ? "i" : "u";

	// The framebuffer contains four attachments with the same number of samples.
	// The fragment shader outputs a different color per sample (in a gradient) to verify that the multisampled image actually has that many samples:
	//
	// - For samples [4s, 4s+3), the shader outputs:
	//
	//     Vec4(0, v, v, v),
	//     Vec4(v, 0, v, v),
	//     Vec4(v, v, 0, v),
	//     Vec4(v, v, v, 0),
	//
	//   for float attachments where v = 1-s*0.2. For sample s, it outputs:
	//
	//     UVec4(v, v + 1, v + 2, v + 3),
	//
	//   for the int attachment where v = (s+1)*(s+1)*10.
	//
	// Additionally, the fragment shader outputs depth based on the sample index as well.  For sample s, it outputs 1 - (s^1)/16.
	// Note that ^1 ensures VK_RESOLVE_MODE_SAMPLE_ZERO_BIT and VK_RESOLVE_MODE_MAX_BIT produce different values.
	{
		const TestParams::PerPass &perPass = params.perPass[0];

		// The shader outputs up to 16 samples
		const deUint32	numSamples	= static_cast<deUint32>(perPass.numSamples);

		DE_ASSERT(numSamples <= 16);

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = " << perPass.floatColor1Location << ") out vec4 o_color1;\n"
			<< "layout(location = " << perPass.floatColor2Location << ") out vec4 o_color2;\n"
			<< "layout(location = " << perPass.intColorLocation << ") out " << intTypePrefix << "vec4 o_color3;\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "} params;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    vec2 uv = (gl_FragCoord.xy - vec2(params.area.xy)) / vec2(params.area.zw);\n";
		for (deUint32 sampleID = 0; sampleID < numSamples; ++sampleID)
		{
			const char*	uvComponent	= sampleID % 2 == 0 ? "uv.x" : "uv.y";

			const float		floatValue	= 1 - static_cast<float>(sampleID / 4) * 0.2f;
			const deUint32	intValue	= (sampleID + 1) * (sampleID + 1) * 10;
			const float		depthValue	= 1 - static_cast<float>(sampleID ^ 1) / 16.0f;

			const Vec4		floatChannels(sampleID % 4 == 0 ? 0 : floatValue,
										  sampleID % 4 == 1 ? 0 : floatValue,
										  sampleID % 4 == 2 ? 0 : floatValue,
										  sampleID % 4 == 3 ? 0 : floatValue);
			const UVec4		intChannels(intValue, intValue + 1, intValue + 2, intValue + 3);

			src << "    " << (sampleID == 0 ? "" : "else ") << "if (gl_SampleID == " << sampleID << ")\n"
				<< "    {\n"
				<< "        o_color1 = vec4(" << floatChannels.x() << ", " << floatChannels.y() << ", " << floatChannels.z() << ", " << floatChannels.w() << ") * " << uvComponent << ";\n"
				<< "        o_color2 = vec4(" << floatChannels.x() << ", " << floatChannels.y() << ", " << floatChannels.z() << ", " << floatChannels.w() << ") * " << uvComponent << ";\n"
				<< "        o_color3 = " << intTypePrefix << "vec4(vec4(" << intChannels.x() << ", " << intChannels.y() << ", " << intChannels.z() << ", " << intChannels.w() << ") * " << uvComponent << ");\n"
				<< "        gl_FragDepth = " << depthValue << ";\n"
				<< "    }\n";
		}
		src << "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	// Compute shader - verify the results of rendering
	//
	// Take the formulas used for the fragment shader.  Note the following:
	//
	//    n-1
	//    sum(1 - s*0.2)
	//     0                 n - (n*(n-1))/2 * 0.2
	//  ----------------- = ----------------------- = 1 - (n-1)*0.1
	//          n                    n
	//
	// When rendering is done to every sample and the attachment is resolved, we expect:
	//
	// - For float attachments, average of:
	//   * Horizontal gradient:
	//
	//       Vec4(0, 1, 1, 1)			if 2 samples
	//       Vec4(0.5v, v, 0.5v, v)		o.w. where v = 1 - (n - 1)*0.1 where n = floor(sampleCount / 4).
	//
	//   * Vertical gradient:
	//
	//       Vec4(1, 0, 1, 1)			if 2 samples
	//       Vec4(v, 0.5v, v, 0.5v)		o.w. where v = 1 - (n - 1)*0.1 where n = floor(sampleCount / 4).
	//
	// - For the int attachments, any of UVec4(v, v + 1, v + 2, v + 3) where v = (s+1)*(s+1)*10
	// - For the depth attachment, either 1 or 1-1/16 based on whether MAX or SAMPLE_ZERO resolve modes are selected respectively.
	// - For the stencil attachment, expect the clear value + 1.
	{
		const TestParams::PerPass &perPass = params.perPass[0];

		// The shader outputs up to 16 samples
		const deUint32	numSamples	= static_cast<deUint32>(perPass.numSamples);

		const float		floatValue	= 1 - static_cast<float>((numSamples / 4) - 1) * 0.1f;

		const Vec4		floatExpectHorizontal	= numSamples == 2 ? Vec4(0, 1, 1, 1)
																  : Vec4(0.5f * floatValue, floatValue, 0.5f * floatValue, floatValue);
		const Vec4		floatExpectVertical		= numSamples == 2 ? Vec4(1, 0, 1, 1)
																  : Vec4(floatValue, 0.5f * floatValue, floatValue, 0.5f * floatValue);

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    uint stencilExpect;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform texture2D color1Image;\n"
			<< "layout(set = 0, binding = 2) uniform texture2D color2Image;\n"
			<< "layout(set = 0, binding = 3) uniform " << (usesSignedIntFormat ? "i" : "u") << "texture2D color3Image;\n";
		if (isDepthFormat(params.depthStencilFormat))
			src << "layout(set = 0, binding = 4) uniform texture2D depthImage;\n";
		if (isStencilFormat(params.depthStencilFormat))
			src << "layout(set = 0, binding = 5) uniform utexture2D stencilImage;\n";
		src << "layout(set = 0, binding = 6, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool fmatches(float a, float b, float error)\n"
			<< "{\n"
			<< "    return abs(a - b) < error;\n"
			<< "}\n"
			<< "bool umatches(uint a, uint b, uint error)\n"
			<< "{\n"
			<< "    return abs(a - b) <= error;\n"
			<< "}\n"
			<< "bool v4matches(vec4 a, vec4 b, vec4 error)\n"
			<< "{\n"
			<< "    return all(lessThan(abs(a - b), error));\n"
			<< "}\n"
			<< "bool i4matchesEither(ivec4 a, ivec4 b, ivec4 c, int errorB, int errorC)\n"
			<< "{\n"
			<< "    return all(lessThanEqual(abs(a - b), ivec4(errorB))) || all(lessThanEqual(abs(a - c), ivec4(errorC)));\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(params.area.zw);\n"
			<< "\n"
			<< "    vec4 result1 = vec4(1, 0, 0, 1);\n"
			<< "    vec4 color1 = texelFetch(color1Image, ivec2(coords), 0);\n"
			<< "    vec4 expected1H = vec4(" << floatExpectHorizontal.x() << ", "
											 << floatExpectHorizontal.y() << ", "
											 << floatExpectHorizontal.z() << ", "
											 << floatExpectHorizontal.w() << ");\n"
			<< "    vec4 expected1V = vec4(" << floatExpectVertical.x() << ", "
											 << floatExpectVertical.y() << ", "
											 << floatExpectVertical.z() << ", "
											 << floatExpectVertical.w() << ");\n"
			<< "    vec4 expected1 = (expected1H * uv.x + expected1V * uv.y) / 2.0;\n"
			// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
			<< "    if (v4matches(color1, expected1, max(expected1H / float(params.area.z), expected1V / float(params.area.w)) + 2.0/255.0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[0], 1);\n"
			<< "        result1 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 0), result1);\n"
			<< "\n"
			<< "    vec4 result2 = vec4(1, 0, 0, 1);\n"
			<< "    vec4 color2 = texelFetch(color2Image, ivec2(coords), 0);\n"
			// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
			<< "    if (v4matches(color2, expected1, max(expected1H / float(params.area.z), expected1V / float(params.area.w)) + 2.0/1024.0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[1], 1);\n"
			<< "        result2 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 1), result2);\n"
			<< "\n"
			<< "    vec4 result3 = vec4(1, 0, 0, 1);\n"
			<< "    ivec4 color3 = ivec4(texelFetch(color3Image, ivec2(coords), 0));\n"
			<< "    if (";
		for (deUint32 sampleID = 0; sampleID < numSamples; ++sampleID)
		{
			const deUint32	intValue	= (sampleID + 1) * (sampleID + 1) * 10;
			const UVec4		intExpect(intValue, intValue + 1, intValue + 2, intValue + 3);

			src << (sampleID == 0 ? "" : "        || ")
				<< "i4matchesEither(color3, ivec4(vec4(" << intExpect.x() << ", " << intExpect.y() << ", " << intExpect.z() << ", " << intExpect.w() << ") * uv.x), "
										<< "ivec4(vec4(" << intExpect.x() << ", " << intExpect.y() << ", " << intExpect.z() << ", " << intExpect.w() << ") * uv.y), "
										// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
										<< intValue << " / int(params.area.z) + 1, "
										<< intValue << " / int(params.area.w) + 1)" << (sampleID == numSamples - 1 ? ")" : "") << "\n";
		}
		src << "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[2], 1);\n"
			<< "        result3 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 2), result3);\n"
			<< "\n";
		if (isDepthFormat(params.depthStencilFormat))
		{
			const float		expect	= perPass.depthStencilResolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT ? 1 - 1/16.0f : 1.0f;

			src << "    vec4 resultDepth = vec4(1, 0, 0, 1);\n"
				<< "    float depth  = texelFetch(depthImage, ivec2(coords), 0).r;\n"
				<< "    if (fmatches(depth, " << expect << ", 0.01))\n"
				<< "    {\n"
				<< "        atomicAdd(sb_out.depthVerification, 1);\n"
				<< "        resultDepth = vec4(0, 1, 0, 1);\n"
				<< "    }\n"
				<< "    imageStore(verify, ivec3(coords, 3), resultDepth);\n";
		}
		if (isStencilFormat(params.depthStencilFormat))
		{
			src << "    vec4 resultStencil = vec4(1, 0, 0, 1);\n"
				<< "    uint stencil = texelFetch(stencilImage, ivec2(coords), 0).r;\n"
				<< "    if (umatches(stencil, params.stencilExpect, 0))\n"
				<< "    {\n"
				<< "        atomicAdd(sb_out.stencilVerification, 1);\n"
				<< "        resultStencil = vec4(0, 1, 0, 1);\n"
				<< "    }\n"
				<< "    imageStore(verify, ivec3(coords, 4), resultStencil);\n";
		}
		src << "}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
	}

	// Always generate constant-color checks as they are used by vkCmdClearAttachments tests
	initConstantColorVerifyPrograms(programCollection, params);
}

void dispatchVerifyBasic(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	postDrawBarrier(context, testObjects);

	const VkPushConstantRange& verifyPushConstantRange =
	{
		VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlags    stageFlags;
		0,																// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(deUint32)),		// uint32_t              size;
	};

	Move<VkPipelineLayout> verifyPipelineLayout;
	setupVerifyDescriptorSetAndPipeline(context, params, wd, testObjects, &verifyPushConstantRange, verifyPipelineLayout);

	const deUint32	stencilExpect	= params.clearValues[3].depthStencil.stencil + 1;

	vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(UVec4), &wd.renderArea);
	vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UVec4), sizeof(deUint32), &stencilExpect);
	vk.cmdDispatch(*testObjects.cmdBuffer, (wd.renderArea.z() + 7) / 8, (wd.renderArea.w() + 7) / 8, 1);

	postVerifyBarrier(context, testObjects, wd.verificationBuffer);

	invalidateAlloc(vk, device, *wd.verificationBufferAlloc);
}

void drawBasic (Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const VkDevice				device			= context.getDevice();
	VkPipelineRenderingCreateInfo				pipelineRenderingCreateInfo;
	std::vector<VkFormat>						colorAttachmentFormats = { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
	std::vector<VkRenderingAttachmentInfo>		colorAttachmentInfos(4u);
	VkRenderingAttachmentInfo					depthStencilAttachmentInfo;

	DE_ASSERT(params.perPass.size() == 1);

	if (params.clearBeforeRenderPass)
	{
		clearImagesBeforeDraw(context, params, wd, testObjects);
	}

	if (params.dynamicRendering)
	{
		preRenderingImageLayoutTransition(context, params, wd, testObjects);
		initResolveImageLayouts(context, params, wd, testObjects);
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription2>							subpasses;
		std::vector<VkImage>										images;
		std::vector<VkImageView>									attachments;
		std::vector<VkAttachmentDescription2>						attachmentDescriptions;
		std::vector<VkAttachmentReference2>							attachmentReferences;
		std::vector<VkAttachmentReference2>							resolveAttachmentReferences;
		VkMultisampledRenderToSingleSampledInfoEXT					msrtss;
		VkSubpassDescriptionDepthStencilResolve						depthStencilResolve;
		deInt32														attachmentNdxes[8]	= {-1, -1, -1, -1,
																						   -1, -1, -1, -1};
		deUint32													attachmentUseMask	= 0;

		initializeAttachments(params, wd, images, attachments, 0, attachmentNdxes);

		if (params.dynamicRendering)
		{
			initializeRenderingAttachmentInfos(params,
											   wd,
											   colorAttachmentInfos,
											   depthStencilAttachmentInfo,
											   colorAttachmentFormats,
											   attachmentNdxes,
											   attachmentUseMask,
											   0u);

			pipelineRenderingCreateInfo = {
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,		// VkStructureType	sType
				DE_NULL,												// const void*		pNext
				0u,														// uint32_t			viewMask
				static_cast<uint32_t>(colorAttachmentFormats.size()),	// uint32_t			colorAttachmentCount
				colorAttachmentFormats.data(),							// const VkFormat*	pColorAttachmentFormats
				VK_FORMAT_UNDEFINED,									// VkFormat			depthAttachmentFormat
				VK_FORMAT_UNDEFINED										// VkFormat			stencilAttachmentFormat
			};

			if (params.usesDepthStencilInPass(0))
			{
				if (isDepthFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.depthAttachmentFormat = params.depthStencilFormat;
				if (isStencilFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.stencilAttachmentFormat = params.depthStencilFormat;
			}
		}
		else
		{
			initializeAttachmentDescriptions(params, attachmentDescriptions, params.clearBeforeRenderPass, attachmentNdxes, attachmentUseMask);

			addSubpassDescription(params,
								  0,
								  attachmentReferences,
								  resolveAttachmentReferences,
								  depthStencilResolve,
								  nullptr,
								  msrtss,
								  subpasses,
								  {},
								  attachmentNdxes);

			createRenderPassAndFramebuffer(context, wd, testObjects, params.pipelineConstructionType, images, attachments, attachmentDescriptions, subpasses, {});
		}
	}

	{
		const VkPushConstantRange& pushConstantRange =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlags    stageFlags;
			0,											// uint32_t              offset;
			static_cast<deUint32>(sizeof(UVec4)),		// uint32_t              size;
		};

		const ShaderWrapper				vertexModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
		const ShaderWrapper				fragmentModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));
		const PipelineLayoutWrapper		pipelineLayout	(params.pipelineConstructionType, vk, device, 0, DE_NULL, 1, &pushConstantRange);

		testObjects.graphicsPipelines.push_back(
			pipeline::makeGraphicsPipeline(vki,
										   vk,
										   physicalDevice,
										   device,
										   context.getDeviceExtensions(),
										   params.pipelineConstructionType,
										   pipelineLayout,
										   params.dynamicRendering ? DE_NULL : *testObjects.renderPassFramebuffers.back(),
										   params.dynamicRendering ? &pipelineRenderingCreateInfo : DE_NULL,
										   vertexModule,
										   fragmentModule,
										   false,
										   true,
										   false,
										   0,
										   0,
										   params.perPass[0].intColorLocation,
										   wd.renderArea,
										   wd.renderArea,
										   params.perPass[0].numSamples,
										   params.useGarbageAttachment));

		if (params.dynamicRendering)
		{
			startRendering(context, params, wd, testObjects, static_cast<uint32_t>(colorAttachmentFormats.size()), colorAttachmentInfos, depthStencilAttachmentInfo, 0u);
		}
		else
		{
			startRenderPass(context, wd, testObjects, DE_LENGTH_OF_ARRAY(params.clearValues), params.clearValues);
		}

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*testObjects.cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);

		vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &wd.renderArea);
		(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
		vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);

		if (params.dynamicRendering)
		{
			vk.cmdEndRendering(*testObjects.cmdBuffer);
		}
		else
		{
			testObjects.renderPassFramebuffers.back().end(vk, *testObjects.cmdBuffer);
		}
	}

	if (params.dynamicRendering)
	{
		postRenderingResolveImageLayoutTransition(context, params, wd, testObjects);
	}

	// Verify results
	dispatchVerifyBasic(context, params, wd, testObjects);
}

//! Verify multisampled rendering is done with the exact number of samples.
tcu::TestStatus testBasic (Context& context, const TestParams params)
{
	WorkingData wd;
	TestObjects testObjects(context);
	testStart(context, params, wd, testObjects);

	drawBasic (context, params, wd, testObjects);

	testEnd(context, params, wd, testObjects);
	return verify(context, params, wd);
}

void generateBasicTest (de::Random& rng, TestParams& params, const VkSampleCountFlagBits sampleCount, const VkResolveModeFlagBits resolveMode, const bool renderToWholeFramebuffer)
{
	params.perPass.resize(1);

	TestParams::PerPass& perPass	= params.perPass[0];

	// Set the sample count for attachments.
	if (params.isMultisampledRenderToSingleSampled)
	{
		params.numFloatColor1Samples	= VK_SAMPLE_COUNT_1_BIT;
		params.numFloatColor2Samples	= VK_SAMPLE_COUNT_1_BIT;
		params.numIntColorSamples		= VK_SAMPLE_COUNT_1_BIT;
		params.numDepthStencilSamples	= VK_SAMPLE_COUNT_1_BIT;

		perPass.resolveFloatColor1		= false;
		perPass.resolveFloatColor2		= false;
		perPass.resolveIntColor			= false;
		perPass.resolveDepthStencil		= false;
	}
	else
	{
		params.numFloatColor1Samples	= sampleCount;
		params.numFloatColor2Samples	= sampleCount;
		params.numIntColorSamples		= sampleCount;
		params.numDepthStencilSamples	= sampleCount;

		perPass.resolveFloatColor1		= true;
		perPass.resolveFloatColor2		= true;
		perPass.resolveIntColor			= true;
		perPass.resolveDepthStencil		= true;
	}
	perPass.depthStencilResolveMode	= resolveMode;

	perPass.numSamples				= sampleCount;

	// Set locations for the color attachments.
	perPass.floatColor1Location = 0;
	perPass.floatColor2Location = 1;
	perPass.intColorLocation = 2;

	// Depth/stencil is always used
	perPass.hasDepthStencil = true;

	// Always clear before render pass so outside render area can be verified.
	params.clearBeforeRenderPass	= true;
	params.renderToWholeFramebuffer	= renderToWholeFramebuffer;
	params.testBlendsColors			= false;

	// Set random clear values.
	generateRandomClearValues(rng, params, params.clearValues, false);

	params.rngSeed = rng.getUint32();
}

void dispatchVerifyClearAttachments(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects, const UVec4 regions[RegionCount], const VkClearValue clearValues[RegionCount - 1][4])
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	postDrawBarrier(context, testObjects);

	const VkPushConstantRange& verifyPushConstantRange =
	{
		VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlags    stageFlags;
		0,																// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(deUint32)),		// uint32_t              size;
	};

	Move<VkPipelineLayout> verifyPipelineLayout;
	setupVerifyDescriptorSetAndPipeline(context, params, wd, testObjects, &verifyPushConstantRange, verifyPipelineLayout);

	const deUint32	stencilExpect[2] =
	{
		// For region 0, there's a single draw that increments the cleared stencil
		params.clearValues[3].depthStencil.stencil + 1,
		// For region 1, there's a vkCmdClearAttachments followed by a draw that increments that stencil value
		clearValues[0][3].depthStencil.stencil + 1,
	};

	// Verify regions 0 and 1 have gradient colors.
	for (deUint32 regionNdx = 0; regionNdx < 2; ++regionNdx)
	{
		vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
		vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UVec4), sizeof(deUint32), &stencilExpect[regionNdx]);
		vk.cmdDispatch(*testObjects.cmdBuffer, (regions[regionNdx].z() + 7) / 8, (regions[regionNdx].w() + 7) / 8, 1);

		postVerifyBarrier(context, testObjects, wd.verificationBuffer);
	}

	// Verify the rest of the regions have clear values.  Note that clearValues[0] is unused as it's overriden with a draw call to region 1.
	for (deUint32 regionNdx = 2; regionNdx < RegionCount; ++regionNdx)
	{
		const VkClearValue*	regionClearValues	= clearValues[regionNdx - 1];
		const UVec4&		region				= regions[regionNdx];

		{
			const VerifySingleFloatPushConstants	verifyColor1 =
			{
				region,
				Vec4(regionClearValues[0].color.float32[0], regionClearValues[0].color.float32[1], regionClearValues[0].color.float32[2], regionClearValues[0].color.float32[3]),
				0,
			};
			dispatchVerifyConstantColor(context, testObjects, wd.getResolvedFloatColorImage1View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.verificationBuffer,
					static_cast<deUint32>(sizeof(verifyColor1)), &verifyColor1, "comp_singleFloat");
		}

		{
			const VerifySingleFloatPushConstants	verifyColor2 =
			{
				region,
				Vec4(regionClearValues[1].color.float32[0], regionClearValues[1].color.float32[1], regionClearValues[1].color.float32[2], regionClearValues[1].color.float32[3]),
				1,
			};
			dispatchVerifyConstantColor(context, testObjects, wd.getResolvedFloatColorImage2View(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.verificationBuffer,
					static_cast<deUint32>(sizeof(verifyColor2)), &verifyColor2, "comp_singleFloat");
		}

		{
			const VerifySingleIntPushConstants	verifyColor3 =
			{
				region,
				IVec4(regionClearValues[2].color.int32[0], regionClearValues[2].color.int32[1], regionClearValues[2].color.int32[2], regionClearValues[2].color.int32[3]),
				2,
			};
			dispatchVerifyConstantColor(context, testObjects, wd.getResolvedIntColorImageView(params), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, wd.verify.view, wd.verificationBuffer,
					static_cast<deUint32>(sizeof(verifyColor3)), &verifyColor3, "comp_singleInt");
		}

		if (isDepthFormat(params.depthStencilFormat))
		{
			const VerifySingleDepthPushConstants	verifyDepth =
			{
				region,
				regionClearValues[3].depthStencil.depth,
			};
			dispatchVerifyConstantColor(context, testObjects, wd.getResolvedDepthOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, wd.verify.view, wd.verificationBuffer,
					static_cast<deUint32>(sizeof(verifyDepth)), &verifyDepth, "comp_singleDepth");
		}

		if (isStencilFormat(params.depthStencilFormat))
		{
			const VerifySingleStencilPushConstants	verifyStencil =
			{
				region,
				regionClearValues[3].depthStencil.stencil,
			};
			dispatchVerifyConstantColor(context, testObjects, wd.getResolvedStencilOnlyImageView(params), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, wd.verify.view, wd.verificationBuffer,
					static_cast<deUint32>(sizeof(verifyStencil)), &verifyStencil, "comp_singleStencil");
		}
	}

	invalidateAlloc(vk, device, *wd.verificationBufferAlloc);
}

void drawClearAttachments (Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const VkDevice				device			= context.getDevice();
	VkPipelineRenderingCreateInfo				pipelineRenderingCreateInfo;
	std::vector<VkFormat>						colorAttachmentFormats = { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
	std::vector<VkRenderingAttachmentInfo>		colorAttachmentInfos(4u);
	VkRenderingAttachmentInfo					depthStencilAttachmentInfo;

	DE_ASSERT(params.perPass.size() == 1);

	if (params.clearBeforeRenderPass)
	{
		clearImagesBeforeDraw(context, params, wd, testObjects);
	}

	if (params.dynamicRendering)
	{
		preRenderingImageLayoutTransition(context, params, wd, testObjects);
		initResolveImageLayouts(context, params, wd, testObjects);
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription2>							subpasses;
		std::vector<VkImage>										images;
		std::vector<VkImageView>									attachments;
		std::vector<VkAttachmentDescription2>						attachmentDescriptions;
		std::vector<VkAttachmentReference2>							attachmentReferences;
		std::vector<VkAttachmentReference2>							resolveAttachmentReferences;
		VkMultisampledRenderToSingleSampledInfoEXT					msrtss;
		VkSubpassDescriptionDepthStencilResolve						depthStencilResolve;
		deInt32														attachmentNdxes[8]	= {-1, -1, -1, -1,
																						   -1, -1, -1, -1};
		deUint32													attachmentUseMask	= 0;

		initializeAttachments(params, wd, images, attachments, 0, attachmentNdxes);

		if (params.dynamicRendering)
		{
			initializeRenderingAttachmentInfos(params,
											   wd,
											   colorAttachmentInfos,
											   depthStencilAttachmentInfo,
											   colorAttachmentFormats,
											   attachmentNdxes,
											   attachmentUseMask,
											   0u);

			pipelineRenderingCreateInfo = {
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,		// VkStructureType	sType
				DE_NULL,												// const void*		pNext
				0u,														// uint32_t			viewMask
				static_cast<uint32_t>(colorAttachmentFormats.size()),	// uint32_t			colorAttachmentCount
				colorAttachmentFormats.data(),							// const VkFormat*	pColorAttachmentFormats
				VK_FORMAT_UNDEFINED,									// VkFormat			depthAttachmentFormat
				VK_FORMAT_UNDEFINED										// VkFormat			stencilAttachmentFormat
			};

			if (params.usesDepthStencilInPass(0))
			{
				if (isDepthFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.depthAttachmentFormat = params.depthStencilFormat;
				if (isStencilFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.stencilAttachmentFormat = params.depthStencilFormat;
			}
		}
		else
		{
			initializeAttachmentDescriptions(params, attachmentDescriptions, params.clearBeforeRenderPass, attachmentNdxes, attachmentUseMask);

			addSubpassDescription(params,
								  0,
								  attachmentReferences,
								  resolveAttachmentReferences,
								  depthStencilResolve,
								  nullptr,
								  msrtss,
								  subpasses,
								  {},
								  attachmentNdxes);

			createRenderPassAndFramebuffer(context, wd, testObjects, params.pipelineConstructionType, images, attachments, attachmentDescriptions, subpasses, {});
		}
	}

	UVec4	regions[RegionCount];
	getDrawRegions(wd, regions);

	VkClearValue	clearValues[RegionCount - 1][4];
	de::Random		rng(params.rngSeed);
	for (deUint32 regionNdx = 0; regionNdx < RegionCount - 1; ++regionNdx)
		generateRandomClearValues(rng, params, clearValues[regionNdx], false);

	{
		const VkPushConstantRange& pushConstantRange =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlags    stageFlags;
			0,											// uint32_t              offset;
			static_cast<deUint32>(sizeof(UVec4)),		// uint32_t              size;
		};

		const ShaderWrapper				vertexModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
		const ShaderWrapper				fragmentModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));
		const PipelineLayoutWrapper		pipelineLayout	(params.pipelineConstructionType, vk, device, 0, DE_NULL, 1, &pushConstantRange);

		if (params.dynamicRendering)
		{
			startRendering(context, params, wd, testObjects, static_cast<uint32_t>(colorAttachmentFormats.size()), colorAttachmentInfos, depthStencilAttachmentInfo, 0u);
		}
		else
		{
			startRenderPass(context, wd, testObjects, DE_LENGTH_OF_ARRAY(params.clearValues), params.clearValues);
		}

		// Draw to region[0]
		testObjects.graphicsPipelines.push_back(
			pipeline::makeGraphicsPipeline(vki,
										   vk,
										   physicalDevice,
										   device,
										   context.getDeviceExtensions(),
										   params.pipelineConstructionType,
										   pipelineLayout,
										   params.dynamicRendering ? DE_NULL : *testObjects.renderPassFramebuffers.back(),
										   params.dynamicRendering ? &pipelineRenderingCreateInfo : DE_NULL,
										   vertexModule,
										   fragmentModule,
										   false,
										   true,
										   false,
										   0,
										   0,
										   params.perPass[0].intColorLocation,
										   regions[0],
										   regions[0],
										   params.perPass[0].numSamples,
										   params.useGarbageAttachment));

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*testObjects.cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);

		vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[0]);
		(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
		vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);

		// Clear all regions except region 0
		{
			for (deUint32 regionNdx = 0; regionNdx < RegionCount - 1; ++regionNdx)
			{
				const VkClearAttachment attachments[4] = {
					{ VK_IMAGE_ASPECT_COLOR_BIT,	static_cast<deUint32>(params.perPass[0].floatColor1Location),	clearValues[regionNdx][0], },
					{ VK_IMAGE_ASPECT_COLOR_BIT,	static_cast<deUint32>(params.perPass[0].floatColor2Location),	clearValues[regionNdx][1], },
					{ VK_IMAGE_ASPECT_COLOR_BIT,	static_cast<deUint32>(params.perPass[0].intColorLocation),		clearValues[regionNdx][2], },
					{ getDepthStencilAspectFlags(params.depthStencilFormat),	0,									clearValues[regionNdx][3], },
				};
				const UVec4& region = regions[regionNdx + 1];
				const VkClearRect clearRegions =
				{
					{
						{static_cast<deInt32>(region.x()), static_cast<deInt32>(region.y())},
						{region.z(), region.w()}
					}, 0, 1
				};

				vk.cmdClearAttachments(*testObjects.cmdBuffer, 4, attachments, 1, &clearRegions);
			}
		}

		// Draw to region[1], overriding the clear value
		testObjects.graphicsPipelines.push_back(
			makeGraphicsPipeline(vki,
								 vk,
								 physicalDevice,
								 device,
								 context.getDeviceExtensions(),
								 params.pipelineConstructionType,
								 pipelineLayout,
								 params.dynamicRendering ? DE_NULL : *testObjects.renderPassFramebuffers.back(),
								 params.dynamicRendering ? &pipelineRenderingCreateInfo : DE_NULL,
								 vertexModule,
								 fragmentModule,
								 false,
								 true,
								 false,
								 0,
								 0,
								 params.perPass[0].intColorLocation,
								 regions[1],
								 regions[1],
								 params.perPass[0].numSamples,
								 params.useGarbageAttachment));

		vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[1]);
		(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
		vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);

		if (params.dynamicRendering)
		{
			vk.cmdEndRendering(*testObjects.cmdBuffer);
		}
		else
		{
			testObjects.renderPassFramebuffers.back().end(vk, *testObjects.cmdBuffer);
		}
	}

	if (params.dynamicRendering)
	{
		postRenderingResolveImageLayoutTransition(context, params, wd, testObjects);
	}

	// Verify results
	dispatchVerifyClearAttachments(context, params, wd, testObjects, regions, clearValues);
}

//! Verify vkCmdClearAttachments works.
tcu::TestStatus testClearAttachments (Context& context, const TestParams params)
{
	WorkingData wd;
	TestObjects testObjects(context);
	testStart(context, params, wd, testObjects);

	drawClearAttachments (context, params, wd, testObjects);

	testEnd(context, params, wd, testObjects);
	return verify(context, params, wd);
}

void drawOnePass(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects,
		const ShaderWrapper& vertexModule, const PipelineLayoutWrapper& pipelineLayout, const deUint32 passNdx,
		const deUint32 subpassNdx, UVec4 regions[RegionCount], VkPipelineRenderingCreateInfo* pipelineRenderingCreateInfo)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const VkDevice				device			= context.getDevice();

	const VkDeviceSize vertexBufferOffset = 0;
	vk.cmdBindVertexBuffers(*testObjects.cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);

	const TestParams::PerPass& perPass = params.perPass[passNdx];

	// Each subpass performs 4 sets of one or two draw calls.  Two if there is depth/stencil and one if not.
	// When depth/stencil is present, the first draw call writes to depth, while the second draw call does a depth test.
	// The four sets are draw calls with scissors dividing the render area in four:
	//
	// +--------+---------------+
	// |        |               |
	// |   1    |       2       |
	// |        |               |
	// +--------+---------------+
	// |        |               |
	// |        |               |
	// |   3    |       4       |
	// |        |               |
	// |        |               |
	// +--------+---------------+
	//

	std::ostringstream fragName;
	fragName << "frag_" << passNdx;
	const ShaderWrapper			fragmentModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get(fragName.str().c_str()), 0u));

	for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
	{
		testObjects.graphicsPipelines.push_back(
			pipeline::makeGraphicsPipeline(vki,
										   vk,
										   physicalDevice,
										   device,
										   context.getDeviceExtensions(),
										   params.pipelineConstructionType,
										   pipelineLayout,
										   params.dynamicRendering ? DE_NULL : *testObjects.renderPassFramebuffers.back(),
										   params.dynamicRendering ? pipelineRenderingCreateInfo : DE_NULL,
										   vertexModule,
										   fragmentModule,
										   true,
										   true,
										   false,
										   1 << passNdx,
										   subpassNdx,
										   perPass.intColorLocation,
										   regions[regionNdx],
										   regions[regionNdx],
										   perPass.numSamples,
										   params.useGarbageAttachment));

		vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
		vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UVec4), sizeof(perPass.drawConstantsWithDepthWrite[regionNdx]), &perPass.drawConstantsWithDepthWrite[regionNdx]);
		(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
		vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);

		if (perPass.hasDepthStencil)
		{
			testObjects.graphicsPipelines.push_back(
				pipeline::makeGraphicsPipeline(vki,
											   vk,
											   physicalDevice,
											   device,
											   context.getDeviceExtensions(),
											   params.pipelineConstructionType,
											   pipelineLayout,
											   params.dynamicRendering ? DE_NULL : *testObjects.renderPassFramebuffers.back(),
											   params.dynamicRendering ? pipelineRenderingCreateInfo : DE_NULL,
											   vertexModule,
											   fragmentModule,
											   true,
											   false,
											   true,
											   1 << passNdx,
											   subpassNdx,
											   perPass.intColorLocation,
											   regions[regionNdx],
											   regions[regionNdx],
											   perPass.numSamples,
											   params.useGarbageAttachment));

			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UVec4), sizeof(perPass.drawConstantsWithDepthTest[regionNdx]), &perPass.drawConstantsWithDepthTest[regionNdx]);
			(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
			vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);
		}
	}
}

void dispatchVerifyMultiPassRendering(Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects, UVec4 regions[RegionCount])
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();

	postDrawBarrier(context, testObjects);

	const VkPushConstantRange& verifyPushConstantRange =
	{
		VK_SHADER_STAGE_COMPUTE_BIT,											// VkShaderStageFlags    stageFlags;
		0,																		// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(VerifyPushConstants)),		// uint32_t              size;
	};

	Move<VkPipelineLayout> verifyPipelineLayout;
	setupVerifyDescriptorSetAndPipeline(context, params, wd, testObjects, &verifyPushConstantRange, verifyPipelineLayout);

	for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
	{
		if (regionNdx != 0)
		{
			const VkMemoryBarrier preVerifyBarrier =
			{
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,		// VkStructureType    sType;
				DE_NULL,								// const void*        pNext;
				VK_ACCESS_SHADER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
				VK_ACCESS_SHADER_WRITE_BIT |
					VK_ACCESS_SHADER_READ_BIT,			// VkAccessFlags      dstAccessMask;
			};

			vk.cmdPipelineBarrier(*testObjects.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0,
					1u, &preVerifyBarrier, 0u, DE_NULL, 0u, DE_NULL);
		}

		vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
		vk.cmdPushConstants(*testObjects.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UVec4), sizeof(params.verifyConstants[regionNdx]), &params.verifyConstants[regionNdx]);
		vk.cmdDispatch(*testObjects.cmdBuffer, (regions[regionNdx].z() + 7) / 8, (regions[regionNdx].w() + 7) / 8, 1);
	}

	postVerifyBarrier(context, testObjects, wd.verificationBuffer);

	invalidateAlloc(vk, device, *wd.verificationBufferAlloc);
}

void drawSingleRenderPass (Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	const deUint32			numSubpasses	= static_cast<deUint32>(params.perPass.size());

	if (params.clearBeforeRenderPass)
	{
		clearImagesBeforeDraw(context, params, wd, testObjects);
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription2>							subpasses;
		std::vector<VkImage>										images;
		std::vector<VkImageView>									attachments;
		std::vector<VkAttachmentDescription2>						attachmentDescriptions;
		std::vector<std::vector<VkAttachmentReference2>>			attachmentReferences(numSubpasses);
		std::vector<std::vector<VkAttachmentReference2>>			resolveAttachmentReferences(numSubpasses);
		std::vector<std::vector<deUint32>>							preserveAttachments(numSubpasses);
		std::vector<VkSubpassDependency2>							subpassDependencies;
		std::vector<VkMultisampledRenderToSingleSampledInfoEXT>		msrtss(numSubpasses);
		std::vector<VkSubpassDescriptionDepthStencilResolve>	    depthStencilResolve(numSubpasses);
		deInt32														attachmentNdxes[8]	= {-1, -1, -1, -1,
																						   -1, -1, -1, -1};
		deUint32													attachmentUseMask	= 0;

		initializeAttachments(params, wd, images, attachments, params.perPass.size(), attachmentNdxes);
		initializeAttachmentDescriptions(params, attachmentDescriptions, params.clearBeforeRenderPass, attachmentNdxes, attachmentUseMask);

		for (deUint32 passNdx = 0; passNdx < numSubpasses; ++passNdx)
		{
			addSubpassDescription(params,
								  passNdx,
								  attachmentReferences[passNdx],
								  resolveAttachmentReferences[passNdx],
								  depthStencilResolve[passNdx],
								  &preserveAttachments[passNdx],
								  msrtss[passNdx],
								  subpasses,
								  {},
								  attachmentNdxes);

			if (passNdx > 0)
				addSubpassDependency(passNdx, subpassDependencies);
		}

		createRenderPassAndFramebuffer(context, wd, testObjects, params.pipelineConstructionType, images, attachments, attachmentDescriptions, subpasses, subpassDependencies);
	}

	const VkPushConstantRange& pushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,											// VkShaderStageFlags    stageFlags;
		0,																		// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(DrawPushConstants)),		// uint32_t              size;
	};

	const ShaderWrapper				vertexModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const PipelineLayoutWrapper		pipelineLayout	(params.pipelineConstructionType, vk, device, 0, DE_NULL, 1, &pushConstantRange);

	UVec4	regions[RegionCount];
	getDrawRegions(wd, regions);

	startRenderPass(context, wd, testObjects, DE_LENGTH_OF_ARRAY(params.clearValues), params.clearValues);

	for (deUint32 passNdx = 0; passNdx < numSubpasses; ++passNdx)
	{
		if (passNdx != 0)
			testObjects.renderPassFramebuffers.back().nextSubpass(vk, *testObjects.cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		drawOnePass(context, params, wd, testObjects, vertexModule, pipelineLayout, passNdx, passNdx, regions, DE_NULL);
	}

	testObjects.renderPassFramebuffers.back().end(vk, *testObjects.cmdBuffer);

	// Verify results
	dispatchVerifyMultiPassRendering(context, params, wd, testObjects, regions);
}

//! Verify multisampled rendering in subpasses
tcu::TestStatus testSingleRenderPass (Context& context, const TestParams params)
{
	WorkingData wd;
	TestObjects testObjects(context);
	testStart(context, params, wd, testObjects);

	drawSingleRenderPass (context, params, wd, testObjects);

	testEnd(context, params, wd, testObjects);
	return verify(context, params, wd);
}

void drawMultiRenderPass (Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	const deUint32			numRenderPasses	= static_cast<deUint32>(params.perPass.size());

	if (params.clearBeforeRenderPass)
	{
		clearImagesBeforeDraw(context, params, wd, testObjects);
	}

	if (params.dynamicRendering)
	{
		preRenderingImageLayoutTransition(context, params, wd, testObjects);
		initResolveImageLayouts(context, params, wd, testObjects);
	}

	const VkPushConstantRange& pushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,											// VkShaderStageFlags    stageFlags;
		0,																		// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(DrawPushConstants)),		// uint32_t              size;
	};

	const ShaderWrapper				vertexModule	(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const PipelineLayoutWrapper		pipelineLayout	(params.pipelineConstructionType, vk, device, 0, DE_NULL, 1, &pushConstantRange);

	UVec4	regions[RegionCount];
	getDrawRegions(wd, regions);

	deUint32						attachmentUseMask	= 0;

	for (deUint32 renderPassNdx = 0; renderPassNdx < numRenderPasses; ++renderPassNdx)
	{
		// Create a render pass and a framebuffer
		std::vector<VkSubpassDescription2>				subpasses;
		std::vector<VkImage>							images;
		std::vector<VkImageView>						attachments;
		std::vector<VkAttachmentDescription2>			attachmentDescriptions;
		std::vector<VkAttachmentReference2>				attachmentReferences;
		std::vector<VkAttachmentReference2>				resolveAttachmentReferences;
		VkMultisampledRenderToSingleSampledInfoEXT		msrtss;
		VkSubpassDescriptionDepthStencilResolve			depthStencilResolve;
		deInt32											attachmentNdxes[8]	= {-1, -1, -1, -1,
																			   -1, -1, -1, -1};
		VkPipelineRenderingCreateInfo					pipelineRenderingCreateInfo;
		std::vector<VkFormat>							colorAttachmentFormats = { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
		std::vector<VkRenderingAttachmentInfo>			colorAttachmentInfos(4u);
		VkRenderingAttachmentInfo						depthStencilAttachmentInfo;

		std::vector<VkClearValue>						clearValues;

		initializeAttachments(params, wd, images, attachments, renderPassNdx, attachmentNdxes);
		if (params.dynamicRendering)
		{
			initializeRenderingAttachmentInfos(params,
											   wd,
											   colorAttachmentInfos,
											   depthStencilAttachmentInfo,
											   colorAttachmentFormats,
											   attachmentNdxes,
											   attachmentUseMask,
											   renderPassNdx);

			pipelineRenderingCreateInfo = {
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,		// VkStructureType	sType
				DE_NULL,												// const void*		pNext
				0u,														// uint32_t			viewMask
				static_cast<uint32_t>(colorAttachmentFormats.size()),	// uint32_t			colorAttachmentCount
				colorAttachmentFormats.data(),							// const VkFormat*	pColorAttachmentFormats
				VK_FORMAT_UNDEFINED,									// VkFormat			depthAttachmentFormat
				VK_FORMAT_UNDEFINED										// VkFormat			stencilAttachmentFormat
			};

			if (params.usesDepthStencilInPass(renderPassNdx))
			{
				if (isDepthFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.depthAttachmentFormat = params.depthStencilFormat;
				if (isStencilFormat(params.depthStencilFormat))
					pipelineRenderingCreateInfo.stencilAttachmentFormat = params.depthStencilFormat;
			}
		}
		else
		{
			initializeAttachmentDescriptions(params, attachmentDescriptions, params.clearBeforeRenderPass, attachmentNdxes, attachmentUseMask);

			addSubpassDescription(params,
								  renderPassNdx,
								  attachmentReferences,
								  resolveAttachmentReferences,
								  depthStencilResolve,
								  nullptr,
								  msrtss,
								  subpasses,
								  {},
								  attachmentNdxes);

			createRenderPassAndFramebuffer(context, wd, testObjects, params.pipelineConstructionType, images, attachments, attachmentDescriptions, subpasses, {});

			// Init clear values
			if (attachmentNdxes[0] >= 0)
				clearValues.push_back(params.clearValues[0]);
			if (attachmentNdxes[1] >= 0)
				clearValues.push_back(params.clearValues[1]);
			if (attachmentNdxes[2] >= 0)
				clearValues.push_back(params.clearValues[2]);
			if (attachmentNdxes[3] >= 0)
				clearValues.push_back(params.clearValues[3]);
		}

		if (renderPassNdx > 0)
		{
			const VkMemoryBarrier interRenderPassBarrier =
			{
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,										// VkStructureType    sType;
				DE_NULL,																// const void*        pNext;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,						// VkAccessFlags      srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,						// VkAccessFlags      dstAccessMask;
			};

			vk.cmdPipelineBarrier(*testObjects.cmdBuffer,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT	| VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
															  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					0u, 1u, &interRenderPassBarrier, 0u, DE_NULL, 0u, DE_NULL);
		}

		if (params.dynamicRendering)
		{
			startRendering(context, params, wd, testObjects, static_cast<uint32_t>(colorAttachmentFormats.size()), colorAttachmentInfos, depthStencilAttachmentInfo, renderPassNdx);
		}
		else
		{
			startRenderPass(context, wd, testObjects, static_cast<deUint32>(clearValues.size()), dataOrNullPtr(clearValues));
		}

		drawOnePass(context, params, wd, testObjects, vertexModule, pipelineLayout, renderPassNdx, 0, regions, &pipelineRenderingCreateInfo);

		if (params.dynamicRendering)
		{
			vk.cmdEndRendering(*testObjects.cmdBuffer);
		}
		else
		{
			testObjects.renderPassFramebuffers.back().end(vk, *testObjects.cmdBuffer);
		}
	}

	if (params.dynamicRendering)
	{
		postRenderingResolveImageLayoutTransition(context, params, wd, testObjects);
	}

	// Verify results
	dispatchVerifyMultiPassRendering(context, params, wd, testObjects, regions);
}

//! Verify multisampled rendering in multiple render passes
tcu::TestStatus testMultiRenderPass (Context& context, const TestParams params)
{
	WorkingData wd;
	TestObjects testObjects(context);
	testStart(context, params, wd, testObjects);

	drawMultiRenderPass (context, params, wd, testObjects);

	testEnd(context, params, wd, testObjects);
	return verify(context, params, wd);
}

void generateMultiPassTest (de::Random& rng, TestParams& params)
{
	const VkSampleCountFlagBits	sampleRange[] =
	{
		// 4x multisampling is always supported.  A higher chance is given to that to avoid too many tests being skipped.
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
	};

	const VkResolveModeFlagBits	depthStencilResolveModeRange[] =
	{
		// SAMPLE_ZERO is always supported, while MAX may not be.  A higher chance is given to SAMPLE_ZERO to avoid too many tests being skipped.
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
		VK_RESOLVE_MODE_MAX_BIT,
	};

	// Generate a random number of passes (either subpass or render pass)
	const deUint32	passCount		= rng.getInt(1, 4);

	params.perPass.resize(passCount);

	std::vector<deUint32> passAttachments;

	deUint32 usedAttachmentMask = 0;
	if (params.isMultisampledRenderToSingleSampled)
	{
		// Decide which attachments will be used in which pass.  This is a bit mask.
		for (deUint32 passNdx = 0; passNdx < passCount; ++passNdx)
		{
			passAttachments.push_back(rng.getInt(1, 15));
			usedAttachmentMask |= passAttachments.back();
		}
	}
	else
	{
		passAttachments.push_back(15);	// Make sure all attachments have the same sample count
		for (deUint32 passNdx = 1; passNdx < passCount; ++passNdx)
			passAttachments.push_back(rng.getInt(1, 15));
	}

	// Decide which attachments will be single-sampled.  This is a bit mask.
	// Include any attachment that is not used in any subpass just to make all attachments valid.
	const deUint32	singleSampledAttachmentsMask	= params.isMultisampledRenderToSingleSampled ? rng.getInt(1, 15) | (~usedAttachmentMask & 0xF) : 0;

	DBG("Generating test for %u passes\n", passCount);

	// Set the sample count for attachments.  Multisampled attachments that are used in the same pass will get the same number of samples.
	if ((singleSampledAttachmentsMask & 1) != 0)
		params.numFloatColor1Samples = VK_SAMPLE_COUNT_1_BIT;
	if ((singleSampledAttachmentsMask & 2) != 0)
		params.numFloatColor2Samples = VK_SAMPLE_COUNT_1_BIT;
	if ((singleSampledAttachmentsMask & 4) != 0)
		params.numIntColorSamples = VK_SAMPLE_COUNT_1_BIT;
	if ((singleSampledAttachmentsMask & 8) != 0)
		params.numDepthStencilSamples = VK_SAMPLE_COUNT_1_BIT;

	for (deUint32 passNdx = 0; passNdx < passCount; ++passNdx)
	{
		TestParams::PerPass& perPass = params.perPass[passNdx];

		const deUint32				multisampledAttachments	= passAttachments[passNdx] & ~singleSampledAttachmentsMask;
		const VkSampleCountFlagBits	randomSampleCount		= sampleRange[rng.getInt(0, DE_LENGTH_OF_ARRAY(sampleRange) - 1)];
		DBG("  + random samples: %d, multisampled attachments: %#x\n", randomSampleCount, multisampledAttachments);

		if (multisampledAttachments == 0)
		{
			// If all attachments are single-sampled, choose a random number of samples for the render pass.
			perPass.numSamples = randomSampleCount;
		}
		else
		{
			// Otherwise see if any of the attachments has already been decided what number of samples it has.
			VkSampleCountFlagBits	sampleCount = (multisampledAttachments & 1) != 0 && params.numFloatColor1Samples != 0 ?		params.numFloatColor1Samples
												: (multisampledAttachments & 2) != 0 && params.numFloatColor2Samples != 0 ?		params.numFloatColor2Samples
												: (multisampledAttachments & 4) != 0 && params.numIntColorSamples != 0 ?		params.numIntColorSamples
												: (multisampledAttachments & 8) != 0 && params.numDepthStencilSamples != 0 ?	params.numDepthStencilSamples
												// If none of the attachments already have a defined sample, generate a random sample count to use for all of them.
												: randomSampleCount;
			DBG("   + sample count from attachments or random: %d (already: %d %d %d %d)\n", sampleCount,
				params.numFloatColor1Samples, params.numFloatColor2Samples, params.numIntColorSamples, params.numDepthStencilSamples);

			perPass.numSamples = sampleCount;

			// Make all multisampled attachments used in the pass have the same number of samples.  Additionally, make all the multisampled attachments
			// used in conjunction with the these ones in future passes also have the same number of samples.
			for (deUint32 followingPassNdx = passNdx; followingPassNdx < passCount; ++followingPassNdx)
			{
				const deUint32				followingMultisampledAttachments	= passAttachments[followingPassNdx] & ~singleSampledAttachmentsMask;

				if ((followingMultisampledAttachments & 1) != 0)
					params.numFloatColor1Samples = sampleCount;
				if ((followingMultisampledAttachments & 2) != 0)
					params.numFloatColor2Samples = sampleCount;
				if ((followingMultisampledAttachments & 4) != 0)
					params.numIntColorSamples = sampleCount;
				if ((followingMultisampledAttachments & 8) != 0)
					params.numDepthStencilSamples = sampleCount;
			}
		}

		// Generate random locations for the color attachments.
		deInt32 locations[] = {0, 1, 2, 3};
		for (int i = 0; i < 3; ++i)
		{
			int j = rng.getInt(i, 3);
			std::swap(locations[i], locations[j]);
		}
		size_t nextLocation = 0;
		perPass.floatColor1Location = (passAttachments[passNdx] & 1) != 0 ? locations[nextLocation++] : -1;
		perPass.floatColor2Location = (passAttachments[passNdx] & 2) != 0 ? locations[nextLocation++] : -1;
		perPass.intColorLocation = (passAttachments[passNdx] & 4) != 0 ? locations[nextLocation++] : -1;

		// Specify if depth/stencil is used
		perPass.hasDepthStencil = (passAttachments[passNdx] & 8) != 0;

		perPass.resolveFloatColor1 = false;
		perPass.resolveFloatColor2 = false;
		perPass.resolveIntColor = false;
		perPass.resolveDepthStencil = false;
		perPass.depthStencilResolveMode = VK_RESOLVE_MODE_NONE;

		DBG(" - %u samples, locations: %d %d %d has D/S? %d\n", perPass.numSamples,
			perPass.floatColor1Location, perPass.floatColor2Location, perPass.intColorLocation, perPass.hasDepthStencil);
	}

	DBG(" Sample counts: %u %u %u %u\n", params.numFloatColor1Samples, params.numFloatColor2Samples, params.numIntColorSamples, params.numDepthStencilSamples);

	// Assert that generated passes are valid
	for (deUint32 passNdx = 0; passNdx < passCount; ++passNdx)
	{
		const VkSampleCountFlagBits	sampleCounts[4]				= {params.numFloatColor1Samples, params.numFloatColor2Samples, params.numIntColorSamples, params.numDepthStencilSamples};
		VkSampleCountFlagBits		subpassSampleCount			= VK_SAMPLE_COUNT_1_BIT;

		for (deUint32 attachmentNdx = 0; attachmentNdx < 4; ++attachmentNdx)
		{
			if ((passAttachments[passNdx] & (1 << attachmentNdx)) == 0)
				continue;

			const VkSampleCountFlagBits attachmentSampleCount	= sampleCounts[attachmentNdx] == VK_SAMPLE_COUNT_1_BIT
																		? params.perPass[passNdx].numSamples
																		: sampleCounts[attachmentNdx];

			if (subpassSampleCount == VK_SAMPLE_COUNT_1_BIT)
				subpassSampleCount = attachmentSampleCount;

			DE_ASSERT(subpassSampleCount == attachmentSampleCount);
		}
	}

	// Determine when multisampled attachments should resolve.
	deUint32	resolvedAttachmentsMask	= singleSampledAttachmentsMask;
	for (deUint32 passNdx = passCount; passNdx > 0; --passNdx)
	{
		TestParams::PerPass&	perPass		= params.perPass[passNdx - 1];
		const deUint32	unresolvedAttachments	= passAttachments[passNdx - 1] & ~resolvedAttachmentsMask;

		// Make every multisampled attachment resolve in the last pass it's used.
		if ((unresolvedAttachments & 1) != 0)
			perPass.resolveFloatColor1 = true;
		if ((unresolvedAttachments & 2) != 0)
			perPass.resolveFloatColor2 = true;
		if ((unresolvedAttachments & 4) != 0)
			perPass.resolveIntColor = true;
		if ((unresolvedAttachments & 8) != 0)
			perPass.resolveDepthStencil = true;

		if (perPass.resolveDepthStencil || params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT)
			perPass.depthStencilResolveMode	= depthStencilResolveModeRange[rng.getInt(0, DE_LENGTH_OF_ARRAY(depthStencilResolveModeRange) - 1)];

		resolvedAttachmentsMask |= unresolvedAttachments;

		DBG(" - Resolved 0x%x in pass %u\n", unresolvedAttachments, passNdx - 1);
	}

	// Decide whether clear should be done as part of the render pass.  Tests loadOp=CLEAR vs loadOp=LOAD.
	params.clearBeforeRenderPass	= rng.getBool();
	// Decide whether should render to the whole framebuffer or a subarea.
	params.renderToWholeFramebuffer	= rng.getBool();
	// These tests blend color so they can verify the results all at once at the end.
	params.testBlendsColors			= true;

	// Set random clear values.  Use small values as draw calls do additive blending.
	generateRandomClearValues(rng, params, params.clearValues, true);

	// Decide DrawPushConstants
	for (deUint32 passNdx = 0; passNdx < passCount; ++passNdx)
	{
		TestParams::PerPass& perPass = params.perPass[passNdx];

		for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
		{
			perPass.drawConstantsWithDepthWrite[regionNdx]	= DrawPushConstants{
				{Vec4(rng.getFloat(0.05f, 0.1f), 0, rng.getFloat(0.05f, 0.1f), 0), Vec4(0, rng.getFloat(0.05f, 0.1f), 0, rng.getFloat(0.05f, 0.1f))},
				{Vec4(rng.getFloat(0.05f, 0.1f), rng.getFloat(0.05f, 0.1f), 0, 0), Vec4(0, 0, rng.getFloat(0.05f, 0.1f), rng.getFloat(0.05f, 0.1f))},
				{IVec4(0, 0, 0, 0), IVec4(0, 0, 0, 0)},
				// Use quantized values to avoid values that are too close and may cause precision issues
				Vec2(0.1f * static_cast<float>(rng.getInt(2, 9)), 0.1f * static_cast<float>(rng.getInt(2, 9))),
			};

			perPass.drawConstantsWithDepthTest[regionNdx]	= DrawPushConstants{
				{Vec4(rng.getFloat(0.05f, 0.1f), 0, rng.getFloat(0.05f, 0.1f), 0), Vec4(0, rng.getFloat(0.05f, 0.1f), 0, rng.getFloat(0.05f, 0.1f))},
				{Vec4(rng.getFloat(0.05f, 0.1f), rng.getFloat(0.05f, 0.1f), 0, 0), Vec4(0, 0, rng.getFloat(0.05f, 0.1f), rng.getFloat(0.05f, 0.1f))},
				{IVec4(0, 0, 0, 0), IVec4(0, 0, 0, 0)},
				Vec2(0.1f * static_cast<float>(rng.getInt(2, 9)) + 0.05f, 0.1f * static_cast<float>(rng.getInt(2, 8)) + 0.05f),
			};

			// Integer resolve may choose any sample, so we modify only one channel per pass (hence the maximum of 4 passes).  This way, the verification
			// shader can accept two values per channel.
			perPass.drawConstantsWithDepthWrite[regionNdx].color3Data[0][passNdx]	= rng.getInt(1000, 5000);
			perPass.drawConstantsWithDepthWrite[regionNdx].color3Data[1][passNdx]	= rng.getInt(1000, 5000);
			perPass.drawConstantsWithDepthTest[regionNdx].color3Data[0][passNdx]	= rng.getInt(1000, 5000);
			perPass.drawConstantsWithDepthTest[regionNdx].color3Data[1][passNdx]	= rng.getInt(1000, 5000);
		}
	}

	// Calculate VerifyPushConstants.  Walk through the passes and emulate what the draw calls would produce.
	// Note: Color clear value is not applied and is added by the verification shader.  This is because the verification shader interpolates colors with black,
	// so the baseline (clear value) is added afterwards.
	Vec2 depthResult[RegionCount];
	UVec2 stencilResult[RegionCount];
	for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
	{
		depthResult[regionNdx]		= Vec2(params.clearValues[3].depthStencil.depth, params.clearValues[3].depthStencil.depth);
		stencilResult[regionNdx]	= UVec2(params.clearValues[3].depthStencil.stencil, params.clearValues[3].depthStencil.stencil);
	}

	for (deUint32 passNdx = 0; passNdx < passCount; ++passNdx)
	{
		TestParams::PerPass& perPass = params.perPass[passNdx];

		for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
		{
			// Apply the draw call output to enabled attachments.  Note that the tests always do additive blending, and when depth test succeeds, stencil is incremented.

			// First draw call overwrites depth and always succeeds.
			// Second draw call overwrites only the samples that pass the depth test (which is GREATER).
			const bool	evenSamplesPassDepthTest	= perPass.hasDepthStencil &&
				(!isDepthFormat(params.depthStencilFormat) || perPass.drawConstantsWithDepthTest[regionNdx].depthData[0] > perPass.drawConstantsWithDepthWrite[regionNdx].depthData[0]);
			const bool	oddSamplesPassDepthTest		= perPass.hasDepthStencil &&
				(!isDepthFormat(params.depthStencilFormat) || perPass.drawConstantsWithDepthTest[regionNdx].depthData[1] > perPass.drawConstantsWithDepthWrite[regionNdx].depthData[1]);

			if (perPass.floatColor1Location >= 0)
			{
				params.verifyConstants[regionNdx].color1Data[0] += perPass.drawConstantsWithDepthWrite[regionNdx].color1Data[0];
				params.verifyConstants[regionNdx].color1Data[1] += perPass.drawConstantsWithDepthWrite[regionNdx].color1Data[1];
				if (evenSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color1Data[0] += perPass.drawConstantsWithDepthTest[regionNdx].color1Data[0];
				if (oddSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color1Data[1] += perPass.drawConstantsWithDepthTest[regionNdx].color1Data[1];
			}
			if (perPass.floatColor2Location >= 0)
			{
				params.verifyConstants[regionNdx].color2Data[0] += perPass.drawConstantsWithDepthWrite[regionNdx].color2Data[0];
				params.verifyConstants[regionNdx].color2Data[1] += perPass.drawConstantsWithDepthWrite[regionNdx].color2Data[1];
				if (evenSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color2Data[0] += perPass.drawConstantsWithDepthTest[regionNdx].color2Data[0];
				if (oddSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color2Data[1] += perPass.drawConstantsWithDepthTest[regionNdx].color2Data[1];
			}
			if (perPass.intColorLocation >= 0)
			{
				// Note that integer formats don't blend, so always take the last value that's written.  Each pass writes to only one channel, and color mask is used
				// to emulate the effect of blending.
				if (evenSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color3Data[0] += perPass.drawConstantsWithDepthTest[regionNdx].color3Data[0];
				else
					params.verifyConstants[regionNdx].color3Data[0] += perPass.drawConstantsWithDepthWrite[regionNdx].color3Data[0];

				if (oddSamplesPassDepthTest)
					params.verifyConstants[regionNdx].color3Data[1] += perPass.drawConstantsWithDepthTest[regionNdx].color3Data[1];
				else
					params.verifyConstants[regionNdx].color3Data[1] += perPass.drawConstantsWithDepthWrite[regionNdx].color3Data[1];
			}
			if (perPass.hasDepthStencil)
			{
				depthResult[regionNdx] = perPass.drawConstantsWithDepthWrite[regionNdx].depthData;
				stencilResult[regionNdx] += UVec2(1);

				if (evenSamplesPassDepthTest)
					++stencilResult[regionNdx][0];
				if (oddSamplesPassDepthTest)
					++stencilResult[regionNdx][1];
			}

			// There is no need to resolve color attachments between passes.  For float formats, the additive nature of blend and resolve means we can continue adding to
			// the two color vectors and get the same result in the end, no matter when and how often resolve happens.  For the integer formats this is not true (because resolve
			// does not average), so the test makes sure every channel is written to in only one pass, which again means there's no need to perform a resolve in between passes.
			// Depth/stencil needs to resolve though, either if multisampled and requested or if it's single sampled.
			if (perPass.resolveDepthStencil || params.numDepthStencilSamples == VK_SAMPLE_COUNT_1_BIT)
			{
				DE_ASSERT(perPass.depthStencilResolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT || perPass.depthStencilResolveMode == VK_RESOLVE_MODE_MAX_BIT);
				if (perPass.depthStencilResolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
				{
					params.verifyConstants[regionNdx].depthData = depthResult[regionNdx][0];
					params.verifyConstants[regionNdx].stencilData = stencilResult[regionNdx][0];
				}
				else
				{
					params.verifyConstants[regionNdx].depthData = std::max(depthResult[regionNdx][0], depthResult[regionNdx][1]);
					params.verifyConstants[regionNdx].stencilData = std::max(stencilResult[regionNdx][0], stencilResult[regionNdx][1]);
				}

				// If depth/stencil is single-sampled, prepare the data for the next pass.  If multisampled, it will no longer be used after the resolve.
				depthResult[regionNdx][0]	= depthResult[regionNdx][1]		= params.verifyConstants[regionNdx].depthData;
				stencilResult[regionNdx][0]	= stencilResult[regionNdx][1]	= params.verifyConstants[regionNdx].stencilData;
			}
		}
	}

	params.rngSeed = rng.getUint32();

	// Note: formats are decided outside this function
}

void initMultipassPrograms (SourceCollections& programCollection, const TestParams params)
{
	// Vertex shader - position
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	const bool		usesSignedIntFormat	= params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;
	const char*     intTypePrefix		= usesSignedIntFormat ? "i" : "u";

	// Fragment shader - output color based on sample index and push constants
	for (size_t passNdx = 0; passNdx < params.perPass.size(); ++passNdx)
	{
		const TestParams::PerPass &perPass = params.perPass[passNdx];

		// The framebuffer contains four attachments with a mixture of samples.  A subpass can only contain a mixture of 1x and Nx samples with the pipelines configured at Nx multisampled rendering.
		// The fragment shader is adjusted based on which of these attachments are used in the subpass.  The output of the fragment shader is determined by push constants
		// as such (2 colors specified per output in uniform data):
		//
		// - For even samples, output color is interpolation of color 0 and transparent black from left to right
		// - For odd samples, output color is interpolation of color 1 and transparent black from top to bottom
		//
		// Additionally, the fragment shader outputs depth based on the sample index as well.
		//
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n";

		if (perPass.floatColor1Location >= 0)
			src << "layout(location = " << perPass.floatColor1Location << ") out vec4 o_color1;\n";
		if (perPass.floatColor2Location >= 0)
			src << "layout(location = " << perPass.floatColor2Location << ") out vec4 o_color2;\n";
		if (perPass.intColorLocation >= 0)
			src << "layout(location = " << perPass.intColorLocation << ") out " << intTypePrefix << "vec4 o_color3;\n";

		src << "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    vec4 color1Data[2];\n"
			<< "    vec4 color2Data[2];\n"
			<< "    ivec4 color3Data[2];\n"
			<< "    vec2 depthData;\n"
			<< "} params;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    vec2 uv = (gl_FragCoord.xy - vec2(params.area.xy)) / vec2(params.area.zw);\n"
			<< "    if (gl_SampleID % 2 == 0)\n"
			<< "    {\n";

		if (perPass.floatColor1Location >= 0)
			src << "        o_color1 = params.color1Data[0] * uv.x;\n";
		if (perPass.floatColor2Location >= 0)
			src << "        o_color2 = params.color2Data[0] * uv.x;\n";
		if (perPass.intColorLocation >= 0)
			src << "        o_color3 = " << intTypePrefix << "vec4(vec4(params.color3Data[0]) * uv.x);\n";
		if (perPass.hasDepthStencil)
			src << "        gl_FragDepth = params.depthData.x;\n";

		src << "    }\n"
			<< "    else\n"
			<< "    {\n";

		if (perPass.floatColor1Location >= 0)
			src << "        o_color1 = params.color1Data[1] * uv.y;\n";
		if (perPass.floatColor2Location >= 0)
			src << "        o_color2 = params.color2Data[1] * uv.y;\n";
		if (perPass.intColorLocation >= 0)
			src << "        o_color3 = " << intTypePrefix << "vec4(vec4(params.color3Data[1]) * uv.y);\n";
		if (perPass.hasDepthStencil)
			src << "        gl_FragDepth = params.depthData.y;\n";

		src << "    }\n"
			<< "}\n";

		std::ostringstream name;
		name << "frag_" << passNdx;

		programCollection.glslSources.add(name.str()) << glu::FragmentSource(src.str());
	}

	// Compute shader - verify the results of rendering
	{
		// The images are cleared and rendered to, possibly multiple times with blend, by blending between one color and black horizontally and another color and black vertically for every other sample.
		// Once resolved, the resulting image is verified by interpolating one color and black horizontally, another color and black vertically, averaging them and adding in the clear color.  For integer
		// formats, instead of averaging the two interpolated colors, either of the colors is accepted as integer resolves selects any sample.  A comparison threshold is used to avoid precision issues.
		// Each pixel that passes the test atomically increments an integer in the output buffer.  The test passes if the final number in the output buffer is the same as the number of pixels in the area being verified.

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_EXT_samplerless_texture_functions : require\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "    vec4 color1Data[2];\n"
			<< "    vec4 color2Data[2];\n"
			<< "    ivec4 color3Data[2];\n"
			<< "    float depthData;\n"
			<< "    uint stencilData;\n"
			<< "} params;\n"
			<< "\n"
			<< "layout(local_size_x = 8, local_size_y = 8) in;\n"
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint colorVerification[3];\n"
			<< "    uint depthVerification;\n"
			<< "    uint stencilVerification;\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1) uniform texture2D color1Image;\n"
			<< "layout(set = 0, binding = 2) uniform texture2D color2Image;\n"
			<< "layout(set = 0, binding = 3) uniform " << (usesSignedIntFormat ? "i" : "u") << "texture2D color3Image;\n";
		if (isDepthFormat(params.depthStencilFormat))
			src << "layout(set = 0, binding = 4) uniform texture2D depthImage;\n";
		if (isStencilFormat(params.depthStencilFormat))
			src << "layout(set = 0, binding = 5) uniform utexture2D stencilImage;\n";
		src << "layout(set = 0, binding = 6, rgba8) uniform writeonly image2DArray verify;\n"
			<< "\n"
			<< "bool fmatches(float a, float b, float error)\n"
			<< "{\n"
			<< "    return abs(a - b) < error;\n"
			<< "}\n"
			<< "bool umatches(uint a, uint b, uint error)\n"
			<< "{\n"
			<< "    return abs(a - b) <= error;\n"
			<< "}\n"
			<< "bool v4matches(vec4 a, vec4 b, vec4 error)\n"
			<< "{\n"
			<< "    return all(lessThan(abs(a - b), error));\n"
			<< "}\n"
			<< "bool i4matchesEither(ivec4 a, ivec4 b, ivec4 c, ivec4 errorB, ivec4 errorC)\n"
			<< "{\n"
			<< "    const bvec4 bMatches = lessThanEqual(abs(a - b), errorB);\n"
			<< "    const bvec4 cMatches = lessThanEqual(abs(a - c), errorC);\n"
			<< "    return all(bvec4(bMatches.x || cMatches.x, bMatches.y || cMatches.y, bMatches.z || cMatches.z, bMatches.w || cMatches.w));\n"
			<< "}\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
			<< "        return;\n"
			<< "\n"
			<< "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
			<< "    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(params.area.zw);\n"
			<< "\n"
			<< "    vec4 result1 = vec4(1, 0, 0, 1);\n"
			<< "    vec4 color1 = texelFetch(color1Image, ivec2(coords), 0);\n"
			<< "    vec4 expected1 = (params.color1Data[0] * uv.x + params.color1Data[1] * uv.y) / 2.0";
		if (params.testBlendsColors)
			src << " + vec4(" << params.clearValues[0].color.float32[0] << ", "
							  << params.clearValues[0].color.float32[1] << ", "
							  << params.clearValues[0].color.float32[2] << ", "
							  << params.clearValues[0].color.float32[3] << ")";
		src << ";\n"
			// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
			<< "    if (v4matches(color1, expected1, max(params.color1Data[0] / float(params.area.z), params.color1Data[1] / float(params.area.w)) + 2.0/255.0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[0], 1);\n"
			<< "        result1 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 0), result1);\n"
			<< "\n"
			<< "    vec4 result2 = vec4(1, 0, 0, 1);\n"
			<< "    vec4 color2 = texelFetch(color2Image, ivec2(coords), 0);\n"
			<< "    vec4 expected2 = (params.color2Data[0] * uv.x + params.color2Data[1] * uv.y) / 2.0";
		if (params.testBlendsColors)
			src << " + vec4(" << params.clearValues[1].color.float32[0] << ", "
							  << params.clearValues[1].color.float32[1] << ", "
							  << params.clearValues[1].color.float32[2] << ", "
							  << params.clearValues[1].color.float32[3] << ")";
		src << ";\n"
			// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
			<< "    if (v4matches(color2, expected2, max(params.color2Data[0] / float(params.area.z), params.color2Data[1] / float(params.area.w)) + 2.0/1024.0))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[1], 1);\n"
			<< "        result2 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 1), result2);\n"
			<< "\n"
			<< "    vec4 result3 = vec4(1, 0, 0, 1);\n"
			<< "    ivec4 color3 = ivec4(texelFetch(color3Image, ivec2(coords), 0));\n";
			// Note that integer formats don't blend, so clear values are discarded, except for channels that are never written to.  Each pass
			// outputs only to one channel.
		if (params.testBlendsColors)
			src << "    ivec4 clearValue3 = ivec4(" << (params.perPass[0].intColorLocation < 0 ? params.clearValues[2].color.int32[0] : 0) << ", "
													<< (params.perPass.size() < 2 || params.perPass[1].intColorLocation < 0 ? params.clearValues[2].color.int32[1] : 0) << ", "
													<< (params.perPass.size() < 3 || params.perPass[2].intColorLocation < 0 ? params.clearValues[2].color.int32[2] : 0) << ", "
													<< (params.perPass.size() < 4 || params.perPass[3].intColorLocation < 0 ? params.clearValues[2].color.int32[3] : 0) << ")" << ";\n";
		else
			src << "    ivec4 clearValue3 = ivec4(0);\n";
		src << "    ivec4 expected3_0 = ivec4(params.color3Data[0] * uv.x) + clearValue3;\n"
			<< "    ivec4 expected3_1 = ivec4(params.color3Data[1] * uv.y) + clearValue3;\n"
			// Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
			<< "    if (i4matchesEither(color3, expected3_0, expected3_1, params.color3Data[0] / int(params.area.z), params.color3Data[1] / int(params.area.w)))\n"
			<< "    {\n"
			<< "        atomicAdd(sb_out.colorVerification[2], 1);\n"
			<< "        result3 = vec4(0, 1, 0, 1);\n"
			<< "    }\n"
			<< "    imageStore(verify, ivec3(coords, 2), result3);\n"
			<< "\n";
		if (isDepthFormat(params.depthStencilFormat))
			src << "    vec4 resultDepth = vec4(1, 0, 0, 1);\n"
				<< "    float depth  = texelFetch(depthImage, ivec2(coords), 0).r;\n"
				<< "    if (fmatches(depth, params.depthData, 0.01))\n"
				<< "    {\n"
				<< "        atomicAdd(sb_out.depthVerification, 1);\n"
				<< "        resultDepth = vec4(0, 1, 0, 1);\n"
				<< "    }\n"
				<< "    imageStore(verify, ivec3(coords, 3), resultDepth);\n";
		if (isStencilFormat(params.depthStencilFormat))
			src << "    vec4 resultStencil = vec4(1, 0, 0, 1);\n"
				<< "    uint stencil = texelFetch(stencilImage, ivec2(coords), 0).r;\n"
				<< "    if (umatches(stencil, params.stencilData, 0))\n"
				<< "    {\n"
				<< "        atomicAdd(sb_out.stencilVerification, 1);\n"
				<< "        resultStencil = vec4(0, 1, 0, 1);\n"
				<< "    }\n"
				<< "    imageStore(verify, ivec3(coords, 4), resultStencil);\n";
		src << "}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
	}

	const bool	verifyOutsideRenderArea	= params.clearBeforeRenderPass && !params.renderToWholeFramebuffer;
	if (verifyOutsideRenderArea)
		initConstantColorVerifyPrograms(programCollection, params);
}

void drawInputAttachments (Context& context, const TestParams& params, WorkingData& wd, TestObjects& testObjects)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const VkDevice				device			= context.getDevice();
	const deUint32				numSubpasses	= static_cast<deUint32>(params.perPass.size());

	if (params.clearBeforeRenderPass)
	{
		clearImagesBeforeDraw(context, params, wd, testObjects);
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription2>							subpasses;
		std::vector<VkImage>										images;
		std::vector<VkImageView>									attachments;
		std::vector<VkAttachmentDescription2>						attachmentDescriptions;
		std::vector<std::vector<VkAttachmentReference2>>			attachmentReferences(numSubpasses);
		std::vector<std::vector<VkAttachmentReference2>>			resolveAttachmentReferences(numSubpasses);
		std::vector<std::vector<deUint32>>							preserveAttachments(numSubpasses);
		std::vector<VkAttachmentReference2>							inputAttachmentReferences;
		std::vector<VkSubpassDependency2>							subpassDependencies;
		std::vector<VkMultisampledRenderToSingleSampledInfoEXT>		msrtss(numSubpasses);
		std::vector<VkSubpassDescriptionDepthStencilResolve>		depthStencilResolve(numSubpasses);
		deInt32														attachmentNdxes[8]	= {-1, -1, -1, -1,
																						   -1, -1, -1, -1};
		deUint32													attachmentUseMask	= 0;

		initializeAttachments(params, wd, images, attachments, params.perPass.size(), attachmentNdxes);
		initializeAttachmentDescriptions(params, attachmentDescriptions, params.clearBeforeRenderPass, attachmentNdxes, attachmentUseMask);

		DE_ASSERT(numSubpasses == 2);
		inputAttachmentReferences.resize(2, VkAttachmentReference2{
				VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,		// VkStructureType       sType;
				DE_NULL,										// const void*           pNext;
				VK_ATTACHMENT_UNUSED,							// uint32_t              attachment;
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout         layout;
				0,												// VkImageAspectFlags    aspectMask;
			});
		// Color attachment 1 and depth/stencil attachment are used as input attachments in subpass 1.
		initializeAttachmentReference(inputAttachmentReferences[0], attachmentNdxes[0], VK_FORMAT_UNDEFINED, true);
		initializeAttachmentReference(inputAttachmentReferences[1], attachmentNdxes[3], params.depthStencilFormat, true);

		for (deUint32 passNdx = 0; passNdx < numSubpasses; ++passNdx)
		{
			const std::vector<VkAttachmentReference2> noInputAttachments;

			addSubpassDescription(params,
								  passNdx,
								  attachmentReferences[passNdx],
								  resolveAttachmentReferences[passNdx],
								  depthStencilResolve[passNdx],
								  &preserveAttachments[passNdx],
								  msrtss[passNdx],
								  subpasses,
								  passNdx == 0 ? noInputAttachments : inputAttachmentReferences,
								  attachmentNdxes);
		}

		subpassDependencies.push_back(VkSubpassDependency2{
			VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,					// VkStructureType         sType;
			DE_NULL,												// const void*             pNext;
			0,														// uint32_t                srcSubpass;
			1,														// uint32_t                dstSubpass;

			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,				// VkPipelineStageFlags    srcStageMask;

			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,					// VkPipelineStageFlags    dstStageMask;

			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,			// VkAccessFlags           srcAccessMask;

			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,					// VkAccessFlags           dstAccessMask;

			VK_DEPENDENCY_BY_REGION_BIT,							// VkDependencyFlags       dependencyFlags;
			0,														// int32_t                 viewOffset;
		});

		createRenderPassAndFramebuffer(context, wd, testObjects, params.pipelineConstructionType, images, attachments, attachmentDescriptions, subpasses, subpassDependencies);
	}

	const VkPushConstantRange& pushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,											// VkShaderStageFlags    stageFlags;
		0,																		// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4) + sizeof(DrawPushConstants)),		// uint32_t              size;
	};

	const ShaderWrapper	vertexModule					(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper	fragmentModule0					(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag_0"), 0u));
	const ShaderWrapper	fragmentModule1					(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag_1"), 0u));
	const PipelineLayoutWrapper pipelineLayout			(params.pipelineConstructionType, vk, device, 0, DE_NULL, 1, &pushConstantRange);

	// Descriptor set and layout for the draw call that uses input attachments
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,		VK_SHADER_STAGE_FRAGMENT_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,		VK_SHADER_STAGE_FRAGMENT_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,		VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	testObjects.descriptorPools.emplace_back(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	testObjects.descriptorSets.emplace_back(makeDescriptorSet(vk, device, *testObjects.descriptorPools.back(), *descriptorSetLayout));

	const VkDescriptorImageInfo		color1Info			= makeDescriptorImageInfo(DE_NULL, *wd.floatColor1.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		depthInfo			= makeDescriptorImageInfo(DE_NULL, isDepthFormat(params.depthStencilFormat) ? *wd.depthOnlyImageView : *wd.stencilOnlyImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const VkDescriptorImageInfo		stencilInfo			= makeDescriptorImageInfo(DE_NULL, isStencilFormat(params.depthStencilFormat) ? *wd.stencilOnlyImageView : *wd.depthOnlyImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	DescriptorSetUpdateBuilder	builder;

	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &color1Info);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &depthInfo);
	builder.writeSingle(*testObjects.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &stencilInfo);

	builder.update(vk, device);

	const VkPushConstantRange& inputPushConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,											// VkShaderStageFlags    stageFlags;
		0,																		// uint32_t              offset;
		static_cast<deUint32>(sizeof(UVec4)),									// uint32_t              size;
	};

	const ShaderWrapper				fragmentModuleIn	(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag_in"), 0u));
	const PipelineLayoutWrapper		inputPipelineLayout	(params.pipelineConstructionType, vk, device, 1, &*descriptorSetLayout, 1, &inputPushConstantRange);

	UVec4	regions[RegionCount];
	getDrawRegions(wd, regions);

	startRenderPass(context, wd, testObjects, DE_LENGTH_OF_ARRAY(params.clearValues), params.clearValues);

	{
		DE_ASSERT(numSubpasses == 2);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*testObjects.cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);

		// First draw call outputs to color attachment 1 and depth/stencil.  It doesn't blend with clear for simplicity of the verification code.
		for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
		{
			testObjects.graphicsPipelines.push_back(
				pipeline::makeGraphicsPipeline(vki, vk, physicalDevice, device, context.getDeviceExtensions(), params.pipelineConstructionType, pipelineLayout, *testObjects.renderPassFramebuffers.back(), DE_NULL, vertexModule, fragmentModule0, false, true, false, 0, 0,
									 params.perPass[0].intColorLocation, regions[regionNdx], regions[regionNdx], params.perPass[0].numSamples, params.useGarbageAttachment));

			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UVec4), sizeof(params.perPass[0].drawConstantsWithDepthWrite[regionNdx]), &params.perPass[0].drawConstantsWithDepthWrite[regionNdx]);
			(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
			vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);
		}

		// Next subpass initializes color attachments 2 and 3 from color attachment 1 and depth/stencil, then issues a draw call that modifies those attachments.
		testObjects.renderPassFramebuffers.back().nextSubpass(vk, *testObjects.cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
		{
			testObjects.graphicsPipelines.push_back(
				pipeline::makeGraphicsPipeline(vki, vk, physicalDevice, device, context.getDeviceExtensions(), params.pipelineConstructionType, inputPipelineLayout, *testObjects.renderPassFramebuffers.back(), DE_NULL, vertexModule, fragmentModuleIn, false, false, false, 0, 1,
									 params.perPass[1].intColorLocation, regions[regionNdx], regions[regionNdx], params.perPass[1].numSamples, params.useGarbageAttachment));

			vk.cmdPushConstants(*testObjects.cmdBuffer, *inputPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
			(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
			vk.cmdBindDescriptorSets(*testObjects.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *inputPipelineLayout, 0u, 1u, &testObjects.descriptorSets.back().get(), 0u, DE_NULL);
			vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);
		}

		for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
		{
			testObjects.graphicsPipelines.push_back(
				pipeline::makeGraphicsPipeline(vki, vk, physicalDevice, device, context.getDeviceExtensions(), params.pipelineConstructionType, pipelineLayout, *testObjects.renderPassFramebuffers.back(), DE_NULL, vertexModule, fragmentModule1, true, false, false, 0xC, 1,
									 params.perPass[1].intColorLocation, regions[regionNdx], regions[regionNdx], params.perPass[1].numSamples, params.useGarbageAttachment));

			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UVec4), &regions[regionNdx]);
			vk.cmdPushConstants(*testObjects.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UVec4), sizeof(params.perPass[1].drawConstantsWithDepthWrite[regionNdx]), &params.perPass[1].drawConstantsWithDepthWrite[regionNdx]);
			(*testObjects.graphicsPipelines.back()).bind(*testObjects.cmdBuffer);
			vk.cmdDraw(*testObjects.cmdBuffer, 3, 1u, 0u, 0u);
		}
	}

	testObjects.renderPassFramebuffers.back().end(vk, *testObjects.cmdBuffer);

	// Verify results
	dispatchVerifyMultiPassRendering(context, params, wd, testObjects, regions);
}

//! Verify input attachments and multisampled rendering interact correctly.
tcu::TestStatus testInputAttachments (Context& context, const TestParams params)
{
	WorkingData wd;
	TestObjects testObjects(context);
	testStart(context, params, wd, testObjects);

	drawInputAttachments (context, params, wd, testObjects);

	testEnd(context, params, wd, testObjects);
	return verify(context, params, wd);
}

void generateInputAttachmentsTest (de::Random& rng, TestParams& params, const VkSampleCountFlagBits sampleCount, const VkResolveModeFlagBits resolveMode, const bool renderToWholeFramebuffer)
{
	params.perPass.resize(2);

	// Set the sample count for attachments.
	if (params.isMultisampledRenderToSingleSampled)
	{
		params.numFloatColor1Samples				= VK_SAMPLE_COUNT_1_BIT;
		params.numFloatColor2Samples				= VK_SAMPLE_COUNT_1_BIT;
		params.numIntColorSamples					= VK_SAMPLE_COUNT_1_BIT;
		params.numDepthStencilSamples				= VK_SAMPLE_COUNT_1_BIT;

		params.perPass[0].resolveFloatColor1		= false;
		params.perPass[0].resolveDepthStencil		= false;

		params.perPass[1].resolveFloatColor2		= false;
		params.perPass[1].resolveIntColor			= false;
	}
	else
	{
		params.numFloatColor1Samples				= sampleCount;
		params.numFloatColor2Samples				= sampleCount;
		params.numIntColorSamples					= sampleCount;
		params.numDepthStencilSamples				= sampleCount;

		params.perPass[0].resolveFloatColor1		= true;
		params.perPass[0].resolveDepthStencil		= true;

		params.perPass[1].resolveFloatColor2		= true;
		params.perPass[1].resolveIntColor			= true;
	}

	// Subpass 0 renders to color1 and depth/stencil only.  They are resolved at the end of the pass.
	params.perPass[0].resolveFloatColor2		= false;
	params.perPass[0].resolveIntColor			= false;
	params.perPass[0].depthStencilResolveMode	= resolveMode;

	params.perPass[0].numSamples				= sampleCount;

	params.perPass[0].floatColor1Location = 0;
	params.perPass[0].floatColor2Location = -1;
	params.perPass[0].intColorLocation = -1;
	params.perPass[0].hasDepthStencil = true;

	// Subpass 1 uses color1 and depth/stencil as input attachments and outputs to color2 and color3.
	params.perPass[1].resolveFloatColor1		= false;
	params.perPass[1].resolveDepthStencil		= false;

	params.perPass[1].numSamples				= params.isMultisampledRenderToSingleSampled ? VK_SAMPLE_COUNT_1_BIT : sampleCount;

	params.perPass[1].floatColor1Location = -1;
	params.perPass[1].floatColor2Location = 3;
	params.perPass[1].intColorLocation = 2;
	params.perPass[1].hasDepthStencil = false;

	// Always clear before render pass so outside render area can be verified.
	params.clearBeforeRenderPass	= true;
	params.renderToWholeFramebuffer	= renderToWholeFramebuffer;
	params.testBlendsColors			= false;

	// Set random clear values.
	generateRandomClearValues(rng, params, params.clearValues, true);

	// Decide DrawPushConstants
	for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
	{
		// Subpass 0 writes to color 1, depth and stencil.
		params.perPass[0].drawConstantsWithDepthWrite[regionNdx]	= DrawPushConstants{
			{Vec4(rng.getFloat(0.2f, 0.4f), 0, rng.getFloat(0.2f, 0.4f), 0), Vec4(0, rng.getFloat(0.2f, 0.4f), 0, rng.getFloat(0.2f, 0.4f))},
			{Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0)},
			{IVec4(0, 0, 0, 0), IVec4(0, 0, 0, 0)},
			// Use quantized values to avoid values that are too close and may cause precision issues
			Vec2(0.025f * static_cast<float>(rng.getInt(2, 38)), 0.025f * static_cast<float>(rng.getInt(2, 38))),
		};

		// Subpass 1 writes to color 2 and color 3.
		params.perPass[1].drawConstantsWithDepthWrite[regionNdx]	= DrawPushConstants{
			{Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0)},
			{Vec4(rng.getFloat(0.2f, 0.4f), rng.getFloat(0.2f, 0.4f), 0, 0), Vec4(0, 0, rng.getFloat(0.2f, 0.4f), rng.getFloat(0.2f, 0.4f))},
			{IVec4(0, 0, 0, 0), IVec4(0, 0, 0, 0)},
			// Use quantized values to avoid values that are too close and may cause precision issues
			Vec2(0, 0),
		};

		// Integer resolve may choose any sample, so we modify only one channel.  This way, the verification
		// shader can accept two values per channel.
		params.perPass[0].drawConstantsWithDepthWrite[regionNdx].color3Data[0][0]	= rng.getInt(1000, 5000);
		params.perPass[0].drawConstantsWithDepthWrite[regionNdx].color3Data[1][1]	= rng.getInt(1000, 5000);
		params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color3Data[0][2]	= rng.getInt(1000, 5000);
		params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color3Data[1][3]	= rng.getInt(1000, 5000);
	}

	// Calculate VerifyPushConstants.  Walk through the passes and emulate what the draw calls would produce.
	for (deUint32 regionNdx = 0; regionNdx < RegionCount; ++regionNdx)
	{
		// First, subpass[0]'s data is written to every sample of color1 and depth/stencil.
		params.verifyConstants[regionNdx].color1Data[0] = params.perPass[0].drawConstantsWithDepthWrite[regionNdx].color1Data[0];
		params.verifyConstants[regionNdx].color1Data[1] = params.perPass[0].drawConstantsWithDepthWrite[regionNdx].color1Data[1];

		// Then depth/stencil is resolved
		DE_ASSERT(resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT || resolveMode == VK_RESOLVE_MODE_MAX_BIT);
		if (resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
		{
			params.verifyConstants[regionNdx].depthData	= params.perPass[0].drawConstantsWithDepthWrite[regionNdx].depthData[0];
		}
		else
		{
			params.verifyConstants[regionNdx].depthData	= std::max(params.perPass[0].drawConstantsWithDepthWrite[regionNdx].depthData[0], params.perPass[0].drawConstantsWithDepthWrite[regionNdx].depthData[1]);
		}
		params.verifyConstants[regionNdx].stencilData	= params.clearValues[3].depthStencil.stencil + 1;

		// Then subpass 1 initializes color2 and color3 based on the previous subpass' color1 and depth/stencil values.
		params.verifyConstants[regionNdx].color2Data[0] = params.verifyConstants[regionNdx].color1Data[0];
		params.verifyConstants[regionNdx].color2Data[1] = params.verifyConstants[regionNdx].color1Data[1];

		if (isDepthFormat(params.depthStencilFormat))
		{
			if (params.isMultisampledRenderToSingleSampled)
			{
				params.verifyConstants[regionNdx].color3Data[0][0]	= deInt32(10000 * params.verifyConstants[regionNdx].depthData);
				params.verifyConstants[regionNdx].color3Data[1][0]	= deInt32(10000 * params.verifyConstants[regionNdx].depthData);
			}
			else
			{
				params.verifyConstants[regionNdx].color3Data[0][0]	= deInt32(10000 * params.perPass[0].drawConstantsWithDepthWrite[regionNdx].depthData[0]);
				params.verifyConstants[regionNdx].color3Data[1][0]	= deInt32(10000 * params.perPass[0].drawConstantsWithDepthWrite[regionNdx].depthData[1]);
			}
		}

		if (isStencilFormat(params.depthStencilFormat))
		{
			params.verifyConstants[regionNdx].color3Data[0][1]	= 100 * params.verifyConstants[regionNdx].stencilData;
			params.verifyConstants[regionNdx].color3Data[1][1]	= 100 * params.verifyConstants[regionNdx].stencilData;
		}

		// Finally, a draw call in subpass 1 blends on top of those values.
		if (params.isMultisampledRenderToSingleSampled)
		{
			// If subpass 1 is single-sampled, there's only one sample to write to which is interpolated along X.  Additionally, there's no resolve.
			// The verification code expects the following:
			//
			//     color@uv = (color_even_samples*u + color_odd_samples*v) / 2
			//
			// In this case, we want color@uv to be color_even_samples*u.  We can have the verification shader arrive at this value
			// by providing color_even_samples twice what it should be and zero for color_odd_samples:
			//
			//     color@uv = (color_even_samples*2*u + 0*v) / 2 = color_even_samples*u
			params.verifyConstants[regionNdx].color2Data[0] += params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color2Data[0] * Vec4(2, 2, 2, 2);
		}
		else
		{
			params.verifyConstants[regionNdx].color2Data[0] += params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color2Data[0];
			params.verifyConstants[regionNdx].color2Data[1] += params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color2Data[1];
		}

		params.verifyConstants[regionNdx].color3Data[0] += params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color3Data[0];
		params.verifyConstants[regionNdx].color3Data[1] += params.perPass[1].drawConstantsWithDepthWrite[regionNdx].color3Data[1];
	}

	params.rngSeed = rng.getUint32();
}

void initInputAttachmentsPrograms (SourceCollections& programCollection, const TestParams params)
{
	// This test reuses the same programs as the multipass tests for rendering and verification.
	initMultipassPrograms(programCollection, params);

	const bool		usesSignedIntFormat	= params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;
	const char*     intTypePrefix		= usesSignedIntFormat ? "i" : "u";
	const char*		subpassInputSuffix	= params.perPass[1].numSamples == VK_SAMPLE_COUNT_1_BIT ? "" : "MS";
	const char*		subpassLoadParam	= params.perPass[1].numSamples == VK_SAMPLE_COUNT_1_BIT ? "" : ", gl_SampleID";

	// Fragment shader - initialize color attachments 2 and 3 with data from color attachments 1 and depth/stencil
	{
		const TestParams::PerPass &perPass = params.perPass[1];

		// Data from color attachment 1 is replicated in color attachment 2.  Data from the depth/stencil attachment is replicated in the red and green
		// channels of color attachment 3.  Depth is multiplied by 10000 and interpolated along x and stencil by 100 and interpolated along y.  This makes
		// the result look like the other draw calls that produce a gradient and simplifies the verification code.
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = " << perPass.floatColor2Location << ") out vec4 o_color2;\n"
			<< "layout(location = " << perPass.intColorLocation << ") out " << intTypePrefix << "vec4 o_color3;\n"
			<< "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput" << subpassInputSuffix << " i_color1;\n";
		if (isDepthFormat(params.depthStencilFormat))
			src << "layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput" << subpassInputSuffix << " i_depth;\n";
		if (isStencilFormat(params.depthStencilFormat))
			src << "layout(input_attachment_index = 1, set = 0, binding = 2) uniform usubpassInput" << subpassInputSuffix << " i_stencil;\n";
		src << "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    uvec4 area;\n"
			<< "} params;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    vec2 uv = (gl_FragCoord.xy - vec2(params.area.xy)) / vec2(params.area.zw);\n"
			<< "    o_color2 = subpassLoad(i_color1" << subpassLoadParam << ");\n"
			<< "    if (gl_SampleID % 2 != 0)\n"
			<< "        uv.xy = uv.yx;\n"
			<< "    uvec4 color3Value = uvec4(0);\n";
		if (isDepthFormat(params.depthStencilFormat))
			src << "    color3Value.x = uint(subpassLoad(i_depth" << subpassLoadParam << ").x * 10000 * uv.x);\n";
		if (isStencilFormat(params.depthStencilFormat))
			src << "    color3Value.y = uint(subpassLoad(i_stencil" << subpassLoadParam << ").x * 100 * uv.y);\n";
		src << "    o_color3 = " << intTypePrefix << "vec4(color3Value);\n"
			<< "}\n";

		programCollection.glslSources.add("frag_in") << glu::FragmentSource(src.str());
	}
}

//! Verify that subpass resolve perf query works.
tcu::TestStatus testPerfQuery (Context& context, VkFormat format)
{
	const InstanceInterface&			vki					= context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice		= context.getPhysicalDevice();
	VkFormatProperties2					formatProperties	= {};
	VkSubpassResolvePerformanceQueryEXT	perfQuery			= {};

	perfQuery.sType = VK_STRUCTURE_TYPE_SUBPASS_RESOLVE_PERFORMANCE_QUERY_EXT;
	perfQuery.optimal = 0xDEADBEEF;

	formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
	formatProperties.pNext = &perfQuery;

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

	// There is actually nothing to verify other than that the above query was successful.
	// Regardless of optimal resolve or not, the operations must succeed.  We'll just make sure
	// the driver did produce a valid response.
	if (perfQuery.optimal != VK_FALSE && perfQuery.optimal != VK_TRUE)
	{
		std::string errorMsg = "VkSubpassResolvePerformanceQueryEXT::optimal is not populated after query";
		return tcu::TestStatus::fail(errorMsg);
	}

	return tcu::TestStatus::pass("Pass");
}

std::string getFormatShortString (const VkFormat format)
{
	std::string s(de::toLower(getFormatName(format)));
	return s.substr(10);
}

std::string getFormatCaseName (const VkFormat color1Format,
							   const VkFormat color2Format,
							   const VkFormat color3Format,
							   const VkFormat depthStencilFormat)
{
	std::ostringstream str;
	str << getFormatShortString(color1Format)
		<< "_" << getFormatShortString(color2Format)
		<< "_" << getFormatShortString(color3Format)
		<< "_" << getFormatShortString(depthStencilFormat);
	return str.str();
}

std::string getSampleCountCaseName (const VkSampleCountFlagBits sampleCount)
{
	std::ostringstream str;
	str << sampleCount << "x";
	return str.str();
}

std::string getResolveModeCaseName (const VkResolveModeFlagBits resolveMode)
{
	std::ostringstream str;
	if (resolveMode == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
		str << "ds_resolve_sample_zero";
	else if (resolveMode == VK_RESOLVE_MODE_MAX_BIT)
		str << "ds_resolve_max";
	else
		DE_ASSERT(false);
	return str.str();
}

void createMultisampledTestsInGroup (tcu::TestCaseGroup*		rootGroup,
									 const bool					isMultisampledRenderToSingleSampled,
									 PipelineConstructionType	pipelineConstructionType,
									 const bool					dynamicRendering)
{
	// Color 1 is a float format
	const VkFormat	color1FormatRange[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
	};
	constexpr deUint32	color1FormatCount = DE_LENGTH_OF_ARRAY(color1FormatRange);

	// Color 2 is a float format
	const VkFormat	color2FormatRange[] =
	{
		VK_FORMAT_R16G16B16A16_SFLOAT,
	};
	constexpr deUint32	color2FormatCount = DE_LENGTH_OF_ARRAY(color2FormatRange);

	// Color 3 is an integer format
	const VkFormat	color3FormatRange[] =
	{
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
	};
	constexpr deUint32	color3FormatCount = DE_LENGTH_OF_ARRAY(color3FormatRange);

	// Test formats with only depth, only stencil or both
	const VkFormat	depthStencilFormatRange[] =
	{
		VK_FORMAT_D16_UNORM,				//!< Must be supported
		VK_FORMAT_S8_UINT,					//!< May not be supported
		VK_FORMAT_D24_UNORM_S8_UINT,		//!< Either this, or the next one must be supported
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};
	constexpr deUint32	depthStencilFormatCount = DE_LENGTH_OF_ARRAY(depthStencilFormatRange);

	const VkSampleCountFlagBits	sampleRange[] =
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
	};

	const VkResolveModeFlagBits	depthStencilResolveModeRange[] =
	{
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
		VK_RESOLVE_MODE_MAX_BIT,
	};

	const bool boolRange[] = { false, true };

	// Test 1: Simple tests that verify Nx multisampling actually uses N samples.
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "basic", "Tests that NxMSAA rendering actually renders to N samples"));

		de::Random	rng(0xDEADBEEF);

		for (const VkFormat color1Format		: color1FormatRange)
		for (const VkFormat color2Format		: color2FormatRange)
		for (const VkFormat color3Format		: color3FormatRange)
		for (const VkFormat depthStencilFormat	: depthStencilFormatRange)
		{
			MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(
				rootGroup->getTestContext(), getFormatCaseName(color1Format, color2Format, color3Format, depthStencilFormat).c_str(), "Combination of framebuffer attachment formats"));

			for (const VkSampleCountFlagBits sampleCount : sampleRange)
			{
				MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getSampleCountCaseName(sampleCount).c_str(), "Sample count"));

				for (const VkResolveModeFlagBits resolveMode : depthStencilResolveModeRange)
				{
					MovePtr<tcu::TestCaseGroup> resolveGroup(new tcu::TestCaseGroup(
						rootGroup->getTestContext(), getResolveModeCaseName(resolveMode).c_str(), "Depth/stencil resolve mode"));

					for (const bool renderToWholeFramebuffer : boolRange)
					{
						TestParams testParams;
						deMemset(&testParams, 0, sizeof(testParams));

						testParams.pipelineConstructionType				= pipelineConstructionType;
						testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
						testParams.floatColor1Format					= color1Format;
						testParams.floatColor2Format					= color2Format;
						testParams.intColorFormat						= color3Format;
						testParams.depthStencilFormat					= depthStencilFormat;
						testParams.dynamicRendering						= dynamicRendering;
						testParams.useGarbageAttachment					= false;

						generateBasicTest(rng, testParams, sampleCount, resolveMode, renderToWholeFramebuffer);

						addFunctionCaseWithPrograms(
								resolveGroup.get(),
								renderToWholeFramebuffer ? "whole_framebuffer" : "sub_framebuffer",
								"",
								checkRequirements,
								initBasicPrograms,
								testBasic,
								testParams);
					}

					sampleGroup->addChild(resolveGroup.release());
				}
				formatGroup->addChild(sampleGroup.release());
			}
			group->addChild(formatGroup.release());
		}

		rootGroup->addChild(group.release());
	}

	// Test 2: Test that vkCmdClearAttachments works.
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "clear_attachments", "Tests that vkCmdClearAttachments works"));

		de::Random	rng(0x0FEDCBA9);

		for (const VkFormat color1Format		: color1FormatRange)
		for (const VkFormat color2Format		: color2FormatRange)
		for (const VkFormat color3Format		: color3FormatRange)
		for (const VkFormat depthStencilFormat	: depthStencilFormatRange)
		{
			MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(
				rootGroup->getTestContext(), getFormatCaseName(color1Format, color2Format, color3Format, depthStencilFormat).c_str(), "Combination of framebuffer attachment formats"));

			for (const VkSampleCountFlagBits sampleCount : sampleRange)
			{
				MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getSampleCountCaseName(sampleCount).c_str(), "Sample count"));

				for (const VkResolveModeFlagBits resolveMode : depthStencilResolveModeRange)
				{
					MovePtr<tcu::TestCaseGroup> resolveGroup(new tcu::TestCaseGroup(
						rootGroup->getTestContext(), getResolveModeCaseName(resolveMode).c_str(), "Depth/stencil resolve mode"));

					for (const bool renderToWholeFramebuffer : boolRange)
					{
						TestParams testParams;
						deMemset(&testParams, 0, sizeof(testParams));

						testParams.pipelineConstructionType				= pipelineConstructionType;
						testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
						testParams.floatColor1Format					= color1Format;
						testParams.floatColor2Format					= color2Format;
						testParams.intColorFormat						= color3Format;
						testParams.depthStencilFormat					= depthStencilFormat;
						testParams.dynamicRendering						= dynamicRendering;
						testParams.useGarbageAttachment					= false;

						generateBasicTest(rng, testParams, sampleCount, resolveMode, renderToWholeFramebuffer);

						addFunctionCaseWithPrograms(
								resolveGroup.get(),
								renderToWholeFramebuffer ? "whole_framebuffer" : "sub_framebuffer",
								"",
								checkRequirements,
								initBasicPrograms,
								testClearAttachments,
								testParams);
					}
					sampleGroup->addChild(resolveGroup.release());
				}
				formatGroup->addChild(sampleGroup.release());
			}
			group->addChild(formatGroup.release());
		}

		rootGroup->addChild(group.release());
	}

	// Test 3: Tests with a single render pass, potentially with multiple subpasses.
	// Multiple subpasses can't be tested with dynamic rendering.
	if (!dynamicRendering)
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "multi_subpass", "Single render pass with multiple subpasses"));
		MovePtr<tcu::TestCaseGroup> formatGroup[color1FormatCount][color2FormatCount][color3FormatCount][depthStencilFormatCount];

		for (deUint32 color1FormatNdx = 0; color1FormatNdx < color1FormatCount; ++color1FormatNdx)
		for (deUint32 color2FormatNdx = 0; color2FormatNdx < color2FormatCount; ++color2FormatNdx)
		for (deUint32 color3FormatNdx = 0; color3FormatNdx < color3FormatCount; ++color3FormatNdx)
		for (deUint32 depthStencilFormatNdx = 0; depthStencilFormatNdx < depthStencilFormatCount; ++depthStencilFormatNdx)
		{
			formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx] = MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getFormatCaseName(color1FormatRange[color1FormatNdx],
																   color2FormatRange[color2FormatNdx],
																   color3FormatRange[color3FormatNdx],
																   depthStencilFormatRange[depthStencilFormatNdx]).c_str(),
					"Combination of framebuffer attachment formats"));
		}

		de::Random	rng(0x12345678);

		for (deUint32 iteration = 0; iteration < (isMultisampledRenderToSingleSampled ? 1000 : 250); ++iteration)
		{
			TestParams testParams;
			deMemset(&testParams, 0, sizeof(testParams));

			const deUint32 color1FormatNdx = iteration % color1FormatCount;
			const deUint32 color2FormatNdx = iteration % color2FormatCount;
			const deUint32 color3FormatNdx = iteration % color3FormatCount;
			const deUint32 depthStencilFormatNdx = iteration % depthStencilFormatCount;

			testParams.pipelineConstructionType				= pipelineConstructionType;
			testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
			testParams.floatColor1Format					= color1FormatRange[color1FormatNdx];
			testParams.floatColor2Format					= color2FormatRange[color2FormatNdx];
			testParams.intColorFormat						= color3FormatRange[color3FormatNdx];
			testParams.depthStencilFormat					= depthStencilFormatRange[depthStencilFormatNdx];
			testParams.dynamicRendering						= false;
			testParams.useGarbageAttachment					= false;

			generateMultiPassTest(rng, testParams);

			std::ostringstream name;
			name << "random_" << iteration;

			addFunctionCaseWithPrograms(
				formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx].get(),
				name.str().c_str(),
				"",
				checkRequirements,
				initMultipassPrograms,
				testSingleRenderPass,
				testParams);
		}

		for (deUint32 color1FormatNdx = 0; color1FormatNdx < color1FormatCount; ++color1FormatNdx)
		for (deUint32 color2FormatNdx = 0; color2FormatNdx < color2FormatCount; ++color2FormatNdx)
		for (deUint32 color3FormatNdx = 0; color3FormatNdx < color3FormatCount; ++color3FormatNdx)
		for (deUint32 depthStencilFormatNdx = 0; depthStencilFormatNdx < depthStencilFormatCount; ++depthStencilFormatNdx)
		{
			group->addChild(formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx].release());
		}

		rootGroup->addChild(group.release());
	}

	// Test 4: Tests with a multiple render passes, a single subpass each.
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "multi_renderpass", "Multiple render passes with a single subpass each"));
		MovePtr<tcu::TestCaseGroup> formatGroup[color1FormatCount][color2FormatCount][color3FormatCount][depthStencilFormatCount];

		for (deUint32 color1FormatNdx = 0; color1FormatNdx < color1FormatCount; ++color1FormatNdx)
		for (deUint32 color2FormatNdx = 0; color2FormatNdx < color2FormatCount; ++color2FormatNdx)
		for (deUint32 color3FormatNdx = 0; color3FormatNdx < color3FormatCount; ++color3FormatNdx)
		for (deUint32 depthStencilFormatNdx = 0; depthStencilFormatNdx < depthStencilFormatCount; ++depthStencilFormatNdx)
		{
			formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx] = MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getFormatCaseName(color1FormatRange[color1FormatNdx],
																   color2FormatRange[color2FormatNdx],
																   color3FormatRange[color3FormatNdx],
																   depthStencilFormatRange[depthStencilFormatNdx]).c_str(),
					"Combination of framebuffer attachment formats"));
		}

		de::Random	rng(0x87654321);

		for (deUint32 iteration = 0; iteration < (isMultisampledRenderToSingleSampled ? 1000 : 250); ++iteration)
		{
			TestParams testParams;
			deMemset(&testParams, 0, sizeof(testParams));

			const deUint32 color1FormatNdx = iteration % color1FormatCount;
			const deUint32 color2FormatNdx = iteration % color2FormatCount;
			const deUint32 color3FormatNdx = iteration % color3FormatCount;
			const deUint32 depthStencilFormatNdx = iteration % depthStencilFormatCount;

			testParams.pipelineConstructionType				= pipelineConstructionType;
			testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
			testParams.floatColor1Format					= color1FormatRange[color1FormatNdx];
			testParams.floatColor2Format					= color2FormatRange[color2FormatNdx];
			testParams.intColorFormat						= color3FormatRange[color3FormatNdx];
			testParams.depthStencilFormat					= depthStencilFormatRange[depthStencilFormatNdx];
			testParams.dynamicRendering						= dynamicRendering;
			testParams.useGarbageAttachment					= false;

			generateMultiPassTest(rng, testParams);

			std::ostringstream name;
			name << "random_" << iteration;

			addFunctionCaseWithPrograms(
				formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx].get(),
				name.str().c_str(),
				"",
				checkRequirements,
				initMultipassPrograms,
				testMultiRenderPass,
				testParams);
		}

		for (deUint32 color1FormatNdx = 0; color1FormatNdx < color1FormatCount; ++color1FormatNdx)
		for (deUint32 color2FormatNdx = 0; color2FormatNdx < color2FormatCount; ++color2FormatNdx)
		for (deUint32 color3FormatNdx = 0; color3FormatNdx < color3FormatCount; ++color3FormatNdx)
		for (deUint32 depthStencilFormatNdx = 0; depthStencilFormatNdx < depthStencilFormatCount; ++depthStencilFormatNdx)
		{
			group->addChild(formatGroup[color1FormatNdx][color2FormatNdx][color3FormatNdx][depthStencilFormatNdx].release());
		}

		rootGroup->addChild(group.release());
	}

	// Test 5: Tests multisampled rendering followed by use as input attachment.
	// These tests have two subpasses, so these can't be tested with dynamic rendering.
	if (!dynamicRendering && !vk::isConstructionTypeShaderObject(pipelineConstructionType))
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "input_attachments", "Tests that input attachment interaction with multisampled rendering works"));

		de::Random	rng(0x18273645);

		for (const VkFormat color1Format		: color1FormatRange)
		for (const VkFormat color2Format		: color2FormatRange)
		for (const VkFormat color3Format		: color3FormatRange)
		for (const VkFormat depthStencilFormat	: depthStencilFormatRange)
		{
			MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(
				rootGroup->getTestContext(), getFormatCaseName(color1Format, color2Format, color3Format, depthStencilFormat).c_str(), "Combination of framebuffer attachment formats"));

			for (const VkSampleCountFlagBits sampleCount : sampleRange)
			{
				MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getSampleCountCaseName(sampleCount).c_str(), "Sample count"));

				for (const VkResolveModeFlagBits resolveMode : depthStencilResolveModeRange)
				{
					MovePtr<tcu::TestCaseGroup> resolveGroup(new tcu::TestCaseGroup(
						rootGroup->getTestContext(), getResolveModeCaseName(resolveMode).c_str(), "Depth/stencil resolve mode"));

					for (const bool renderToWholeFramebuffer : boolRange)
					{
						TestParams testParams;
						deMemset(&testParams, 0, sizeof(testParams));

						testParams.pipelineConstructionType				= pipelineConstructionType;
						testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
						testParams.floatColor1Format					= color1Format;
						testParams.floatColor2Format					= color2Format;
						testParams.intColorFormat						= color3Format;
						testParams.depthStencilFormat					= depthStencilFormat;
						testParams.dynamicRendering						= false;
						testParams.useGarbageAttachment					= false;

						generateInputAttachmentsTest(rng, testParams, sampleCount, resolveMode, renderToWholeFramebuffer);

						addFunctionCaseWithPrograms(
								resolveGroup.get(),
								renderToWholeFramebuffer ? "whole_framebuffer" : "sub_framebuffer",
								"",
								checkRequirements,
								initInputAttachmentsPrograms,
								testInputAttachments,
								testParams);
					}
					sampleGroup->addChild(resolveGroup.release());
				}
				formatGroup->addChild(sampleGroup.release());
			}
			group->addChild(formatGroup.release());
		}

		rootGroup->addChild(group.release());
	}


	// Test 6: Tests subpass resolve efficiency query.
	// Efficiency query tests don't need to be tested with different pipeline construction types and with dynamic rendering.
	if (isMultisampledRenderToSingleSampled && pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC && !dynamicRendering)
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "subpass_resolve_efficiency_query", "Tests that subpass resolve efficiency performance query works"));

		for (const VkFormat format		: color1FormatRange)
		{
			addFunctionCase(
					group.get(),
					getFormatShortString(format),
					"",
					checkHasMsrtss,
					testPerfQuery,
					format);
		}

		for (const VkFormat format		: color2FormatRange)
		{
			addFunctionCase(
					group.get(),
					getFormatShortString(format),
					"",
					checkHasMsrtss,
					testPerfQuery,
					format);
		}

		for (const VkFormat format		: color3FormatRange)
		{
			addFunctionCase(
					group.get(),
					getFormatShortString(format),
					"",
					checkHasMsrtss,
					testPerfQuery,
					format);
		}

		for (const VkFormat format	: depthStencilFormatRange)
		{
			addFunctionCase(
					group.get(),
					getFormatShortString(format),
					"",
					checkHasMsrtss,
					testPerfQuery,
					format);
		}

		rootGroup->addChild(group.release());
	}

	// Test 7: Test that work with garbage color attachments
	if (dynamicRendering && pipelineConstructionType != vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "garbage_color_attachment", "Tests that work with garbage color attachments"));

		de::Random	rng(0x12348765);

		for (const VkFormat color1Format		: color1FormatRange)
		for (const VkFormat color2Format		: color2FormatRange)
		for (const VkFormat color3Format		: color3FormatRange)
		for (const VkFormat depthStencilFormat	: depthStencilFormatRange)
		{
			TestParams testParams;
			deMemset(&testParams, 0, sizeof(testParams));

			testParams.pipelineConstructionType				= pipelineConstructionType;
			testParams.isMultisampledRenderToSingleSampled	= isMultisampledRenderToSingleSampled;
			testParams.floatColor1Format					= color1Format;
			testParams.floatColor2Format					= color2Format;
			testParams.intColorFormat						= color3Format;
			testParams.depthStencilFormat					= depthStencilFormat;
			testParams.dynamicRendering						= dynamicRendering;
			testParams.useGarbageAttachment					= true;

			generateBasicTest(rng, testParams, VK_SAMPLE_COUNT_2_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, DE_TRUE);

			addFunctionCaseWithPrograms(
					group.get(),
					getFormatCaseName(color1Format, color2Format, color3Format, depthStencilFormat).c_str(),
					"Combination of framebuffer attachment formats",
					checkRequirements,
					initBasicPrograms,
					testBasic,
					testParams);
		}

		rootGroup->addChild(group.release());
	}
}

void createMultisampledRenderToSingleSampledTestsInGroup (tcu::TestCaseGroup* rootGroup, PipelineConstructionType pipelineConstructionType)
{
	createMultisampledTestsInGroup(rootGroup, true, pipelineConstructionType, false);

	MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "dynamic_rendering", "Multisampled rendering to single-sampled tests with dynamic rendering"));
	createMultisampledTestsInGroup(dynamicRenderingGroup.get(), true, pipelineConstructionType, true);
	rootGroup->addChild(dynamicRenderingGroup.release());
}

void createMultisampledMiscTestsInGroup (tcu::TestCaseGroup* rootGroup, PipelineConstructionType pipelineConstructionType)
{
	createMultisampledTestsInGroup(rootGroup, false, pipelineConstructionType, false);

	MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "dynamic_rendering", "Miscellaneous multisampled rendering tests with dynamic rendering"));
	createMultisampledTestsInGroup(dynamicRenderingGroup.get(), false, pipelineConstructionType, true);
	rootGroup->addChild(dynamicRenderingGroup.release());
}

} // anonymous ns

tcu::TestCaseGroup* createMultisampledRenderToSingleSampledTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	return createTestGroup(testCtx, "multisampled_render_to_single_sampled", "Test multisampled rendering to single-sampled framebuffer attachments", createMultisampledRenderToSingleSampledTestsInGroup, pipelineConstructionType);
}

tcu::TestCaseGroup* createMultisampledMiscTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	return createTestGroup(testCtx, "misc", "Miscellaneous multisampled rendering tests", createMultisampledMiscTestsInGroup, pipelineConstructionType);
}

} // pipeline
} // vkt
