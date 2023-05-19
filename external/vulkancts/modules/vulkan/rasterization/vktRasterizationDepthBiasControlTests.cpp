/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Test for VK_EXT_depth_bias_control.
 *//*--------------------------------------------------------------------*/

#include "vktRasterizationDepthBiasControlTests.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <string>
#include <cstring>

namespace vkt
{
namespace rasterization
{

namespace
{

using namespace vk;

using MaybeRepr = tcu::Maybe<VkDepthBiasRepresentationInfoEXT>;

VkDepthBiasRepresentationInfoEXT makeDepthBiasRepresentationInfo (const VkDepthBiasRepresentationEXT repr, const bool exact)
{
	VkDepthBiasRepresentationInfoEXT info	= initVulkanStructure();
	info.depthBiasRepresentation			= repr;
	info.depthBiasExact						= (exact ? VK_TRUE : VK_FALSE);
	return info;
}

std::string getFormatNameShort (const VkFormat format)
{
	static const size_t	prefixLen	= std::strlen("VK_FORMAT_");
	const auto			fullName	= getFormatName(format);
	const auto			shortName	= de::toLower(std::string(fullName).substr(prefixLen));
	return shortName;
}

inline tcu::IVec3			getExtent		(void) { return tcu::IVec3(1, 1, 1); }
inline VkImageUsageFlags	getColorUsage	(void) { return (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT); }
inline VkImageUsageFlags	getDepthUsage	(void) { return (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT); }

VkImageCreateInfo getImageCreateInfo (const VkFormat format, const VkExtent3D& extent, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		format,									//	VkFormat				format;
		extent,									//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		usage,									//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

using MinResolvDiff = std::pair<double, double>; // Min and Max values.

double calcPowerOf2 (int exponent)
{
	if (exponent >= 0)
		return static_cast<double>(1u << exponent);
	return (1.0 / static_cast<double>(1u << (-exponent)));
}

tcu::TextureChannelClass getChannelClass (const tcu::TextureFormat& format)
{
	const auto generalClass = getTextureChannelClass(format.type);
	// Fix for VK_FORMAT_X8_D24_UNORM_PACK32
	return ((generalClass == tcu::TEXTURECHANNELCLASS_LAST) ? tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT : generalClass);
}

// Returns a couple of numbers with the minimum and maximum values R (minimum resolvable difference) can have according to the spec.
// As explained there, this depends on the depth attachment format, the depth bias representation parameters and sometimes the
// geometry itself.
MinResolvDiff calcMinResolvableDiff (const tcu::TextureFormat& format, const VkDepthBiasRepresentationEXT repr, bool exact, float sampleDepth)
{
	MinResolvDiff	r				(0.0, 0.0);
	const auto		channelClass	= getChannelClass(format);

	switch (repr)
	{
	case VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT:
		if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
		{
			// Up to r = 2x2^(-n) where n is bit width.
			const auto		bitDepth	= getTextureFormatBitDepth(format);
			const double	minR		= calcPowerOf2(-bitDepth[0]);

			r.first		= minR;
			r.second	= (exact ? 1.0 : 2.0) * minR;
		}
		else if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
		{
			// r = 2^(e-n): e is max exponent in z values, n is mantissa bits.
			const tcu::Float32	value		(sampleDepth);
			const int			exponent	= value.exponent() - tcu::Float32::MANTISSA_BITS; // (e-n)
			const double		minR		= calcPowerOf2(exponent);

			r.first		= minR;
			r.second	= minR;
		}
		else
			DE_ASSERT(false);
		break;
	case VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT:
		{
			// Up to r = 2x2^(-n), where n is the bit width for fixed-point formats or the number of mantissa bits plus one for
			// floating-point formats.
			int n = 0;

			if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
				n = getTextureFormatBitDepth(format)[0];
			else if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
				n = tcu::Float32::MANTISSA_BITS + 1;
			else
				DE_ASSERT(false);

			DE_ASSERT(n > 0); // Make sure the bitwidth is positive.
			const double minR = calcPowerOf2(-n);

			r.first		= minR;
			r.second	= (exact ? 1.0 : 2.0) * minR;
		}
		break;
	case VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT:
		// r is always 1.
		r.first		= 1.0;
		r.second	= 1.0;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return r;
}

// Calculates error threshold when representing values in the given format. This is equivalent to calculating the minimum resolvable
// difference R according to the format, with exact precision.
double getDepthErrorThreshold (const tcu::TextureFormat& format, const float sampleDepth)
{
	const auto r = calcMinResolvableDiff(format, VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, true, sampleDepth);
	return r.first;
}

// Which depth bias factor will be used in the tests: focus on depthBiasSlopeFactor or depthBiasConstantFactor.
enum class UsedFactor	{ SLOPE = 0, CONSTANT = 1 };

// How are we going to set the depth bias parameters: statically or dynamically.
enum class SetMechanism	{ STATIC = 0, DYNAMIC_1 = 1 /*vkCmdSetDepthBias*/, DYNAMIC_2 = 2 /*vkCmdSetDepthBias2*/};

std::string getMechanismName (const SetMechanism m)
{
	switch (m)
	{
	case SetMechanism::STATIC:		return "Static";
	case SetMechanism::DYNAMIC_1:	return "vkCmdSetDepthBias";
	case SetMechanism::DYNAMIC_2:	return "vkCmdSetDepthBias2";
	default:						break;
	}

	DE_ASSERT(false);
	return "";
}

struct TestParams
{
	const VkFormat		attachmentFormat;	// Depth attachment format.
	const MaybeRepr		reprInfo;			// Representation info. We omit it for some cases.
	const SetMechanism	setMechanism;
	const float			targetBias;			// The bias we aim to get, before clamping. Based on this and R, we can calculate factors.
	const UsedFactor	usedFactor;
	const float			constantDepth;		// When using UsedFactor::CONSTANT.
	const float			depthBiasClamp;
	const bool			secondaryCmdBuffer;	// Use secondary command buffers or not.

	void log (tcu::TestLog& testLog) const
	{
		testLog << tcu::TestLog::Message << "Depth format: " << attachmentFormat << tcu::TestLog::EndMessage;

		if (!reprInfo)
			testLog << tcu::TestLog::Message << "No VkDepthBiasRepresentationInfoEXT extension structure" << tcu::TestLog::EndMessage;
		else
			testLog << tcu::TestLog::Message << *reprInfo << tcu::TestLog::EndMessage;

		testLog
			<< tcu::TestLog::Message << "Set mechanism: " << getMechanismName(setMechanism) << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Target bias: " << targetBias << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Used factor: " << ((usedFactor == UsedFactor::SLOPE) ? "depthBiasSlopeFactor" : "depthBiasConstantFactor") << tcu::TestLog::EndMessage
			;

		if (usedFactor == UsedFactor::SLOPE)
			testLog << tcu::TestLog::Message << "Maximum depth slope: " << 1.0f << tcu::TestLog::EndMessage;
		else
			testLog << tcu::TestLog::Message << "Constant depth: " << constantDepth << tcu::TestLog::EndMessage;

		testLog << tcu::TestLog::Message << "Depth bias clamp: " << depthBiasClamp << tcu::TestLog::EndMessage;
	}
};

class DepthBiasControlInstance : public vkt::TestInstance
{
public:
						DepthBiasControlInstance	(Context& context, const TestParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~DepthBiasControlInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	const TestParams	m_params;
};

class DepthBiasControlCase : public vkt::TestCase
{
public:
	static tcu::Vec4 kOutColor;

					DepthBiasControlCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	virtual			~DepthBiasControlCase		(void) {}

	void			initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override;
	void			checkSupport				(Context& context) const override;

protected:
	const TestParams m_params;
};

tcu::Vec4 DepthBiasControlCase::kOutColor (0.0f, 0.0f, 1.0f, 1.0f);

void DepthBiasControlCase::checkSupport (Context &context) const
{
	context.requireDeviceFunctionality("VK_EXT_depth_bias_control");

	if (m_params.reprInfo)
	{
		const auto& reprInfo	= m_params.reprInfo.get();
		const auto& dbcFeatures	= context.getDepthBiasControlFeaturesEXT();

		if (reprInfo.depthBiasExact && !dbcFeatures.depthBiasExact)
			TCU_THROW(NotSupportedError, "depthBiasExact not supported");

		if (reprInfo.depthBiasRepresentation == VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT
			&& !dbcFeatures.leastRepresentableValueForceUnormRepresentation)
		{
			TCU_THROW(NotSupportedError, "leastRepresentableValueForceUnormRepresentation not supported");
		}

		if (reprInfo.depthBiasRepresentation == VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT && !dbcFeatures.floatRepresentation)
			TCU_THROW(NotSupportedError, "floatRepresentation not supported");
	}

	// Check format support.
	const auto&	vki				= context.getInstanceInterface();
	const auto	physDev			= context.getPhysicalDevice();

	const auto	imageExtent		= makeExtent3D(getExtent());
	const auto	imageUsage		= getDepthUsage();
	const auto	imageCreateInfo	= getImageCreateInfo(m_params.attachmentFormat, imageExtent, imageUsage);

	VkImageFormatProperties formatProperties;
	const auto formatSupport = vki.getPhysicalDeviceImageFormatProperties(physDev, m_params.attachmentFormat, imageCreateInfo.imageType, imageCreateInfo.tiling, imageUsage, imageCreateInfo.flags, &formatProperties);
	if (formatSupport == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, getFormatNameShort(m_params.attachmentFormat) + " not supported");
}

TestInstance* DepthBiasControlCase::createInstance (Context& context) const
{
	return new DepthBiasControlInstance(context, m_params);
}

void DepthBiasControlCase::initPrograms (vk::SourceCollections &programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "layout (location=0) in vec4 inPos;\n"
		<< "void main (void) {\n"
		<< "    gl_Position = inPos;\n"
		<< "}\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main (void) {\n"
		<< "    outColor = vec4" << kOutColor << ";\n"
		<< "}\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus DepthBiasControlInstance::iterate (void)
{
	const auto			ctx					= m_context.getContextCommonData();
	const tcu::IVec3	fbExtent			(1, 1, 1);
	const auto			vkExtent			= makeExtent3D(fbExtent);
	const auto			colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorUsage			= getColorUsage();
	const auto			depthFormat			= m_params.attachmentFormat;
	const auto			depthUsage			= getDepthUsage();
	const auto			bindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	const auto			colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			depthSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	const auto			colorSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			depthSRL			= makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
	const auto			tcuDepthFormat		= getDepthCopyFormat(depthFormat);
	const auto			tcuColorFormat		= mapVkFormat(colorFormat);
	const bool			setStatically		= (m_params.setMechanism == SetMechanism::STATIC);
	auto&				log					= m_context.getTestContext().getLog();

	const auto			colorCreateInfo		= getImageCreateInfo(colorFormat, vkExtent, colorUsage);
	const auto			depthCreateInfo		= getImageCreateInfo(depthFormat, vkExtent, depthUsage);

	// Color buffer.
	ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, colorCreateInfo.imageType, colorSRR,
		colorCreateInfo.arrayLayers, colorCreateInfo.samples, colorCreateInfo.tiling, colorCreateInfo.mipLevels, colorCreateInfo.sharingMode);

	// Depth buffer.
	ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, depthFormat, depthUsage, depthCreateInfo.imageType, depthSRR,
		depthCreateInfo.arrayLayers, depthCreateInfo.samples, depthCreateInfo.tiling, depthCreateInfo.mipLevels, depthCreateInfo.sharingMode);

	// Vertices and vertex buffer.
	//
	// Generate two triangles as a triangle strip covering the whole framebuffer (4 vertices).
	// +--+
	// | /|
	// |/ |
	// +--+
	//
	// WHEN USING THE DEPTH SLOPE FACTOR:
	// If the framebuffer is 1x1, the delta-X and delta-Y accross the whole framebuffer is 1.
	// If we make the left-side vertices have a depth of 1.0 and the other 2 have 0.0, delta-Z is 1.
	// Using both alternative formulas for calculating M, M is 1. This means depthSlopeFactor applies directly.
	// The depth at the sampling point would be 0.5.
	//
	// WHEN USING THE CONSTANT FACTOR:
	// Generate geometry with a chosen constant depth, so M is zero and depthSlopeFactor never applies.
	// We will make depthSlopeFactor 0 in any case.
	// The constant depth value allows us to control the depth value exponent, which affects some calculations.
	const std::vector<tcu::Vec4> vertices
	{
		tcu::Vec4(-1.0f, -1.0f, ((m_params.usedFactor == UsedFactor::CONSTANT) ? m_params.constantDepth : 0.0f), 1.0f),
		tcu::Vec4(-1.0f,  1.0f, ((m_params.usedFactor == UsedFactor::CONSTANT) ? m_params.constantDepth : 0.0f), 1.0f),
		tcu::Vec4( 1.0f, -1.0f, ((m_params.usedFactor == UsedFactor::CONSTANT) ? m_params.constantDepth : 1.0f), 1.0f),
		tcu::Vec4( 1.0f,  1.0f, ((m_params.usedFactor == UsedFactor::CONSTANT) ? m_params.constantDepth : 1.0f), 1.0f),
	};
	const float sampleDepth = ((m_params.usedFactor == UsedFactor::CONSTANT) ? m_params.constantDepth : 0.5f);

	const auto			vertexBufferSize	= static_cast<VkDeviceSize>(de::dataSize(vertices));
	const auto			vertexBufferInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	BufferWithMemory	vertexBuffer		(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
	auto&				vertexAlloc			= vertexBuffer.getAllocation();
	const auto			vertexBufferOffset	= static_cast<VkDeviceSize>(0);

	deMemcpy(vertexAlloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
	flushAlloc(ctx.vkd, ctx.device, vertexAlloc);

	// Render pass with depth attachment.
	const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat, depthFormat);

	// Framebuffer.
	const std::vector<VkImageView> imageViews
	{
		colorBuffer.getImageView(),
		depthBuffer.getImageView(),
	};

	const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(),
											 de::sizeU32(imageViews), de::dataOrNull(imageViews),
											 vkExtent.width, vkExtent.height);

	// Pipeline.
	const auto&	binaries		= m_context.getBinaryCollection();
	const auto	vertModule		= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	fragModule		= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
	const auto	pipelineLayout	= makePipelineLayout(ctx.vkd, ctx.device);

	// Viewports and scissors.
	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	// Calculate depth bias parameters.
	const auto representation	= (m_params.reprInfo ? m_params.reprInfo->depthBiasRepresentation : VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT);
	const bool exactRepr		= (m_params.reprInfo ? m_params.reprInfo->depthBiasExact : false);
	const auto rValue			= calcMinResolvableDiff(tcuDepthFormat, representation, exactRepr, sampleDepth);

	// Calculate factors based on the target bias and the minimum resolvable difference.
	const float depthBiasConstantFactor	= ((m_params.usedFactor == UsedFactor::CONSTANT) ? static_cast<float>(static_cast<double>(m_params.targetBias) / rValue.first) : 0.0f);
	const float depthBiasSlopeFactor	= ((m_params.usedFactor == UsedFactor::SLOPE) ? m_params.targetBias : 0.0f); // Note M is 1.
	const float depthBiasClamp			= m_params.depthBiasClamp;
	{
		// Log some interesting test details, including computed factors.
		m_params.log(log);
		log
			<< tcu::TestLog::Message << "Rmin:                    " << rValue.first << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Rmax:                    " << rValue.second << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "depthBiasConstantFactor: " << depthBiasConstantFactor << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "depthBiasSlopeFactor:    " << depthBiasSlopeFactor << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "depthBiasClamp:          " << depthBiasClamp << tcu::TestLog::EndMessage
			;
	}

	const void* rasterizationPnext = ((setStatically && m_params.reprInfo) ? &m_params.reprInfo.get() : nullptr);

	const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		rasterizationPnext,											//	const void*								pNext;
		0u,															//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													//	VkBool32								depthClampEnable;
		VK_FALSE,													//	VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_BACK_BIT,										//	VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							//	VkFrontFace								frontFace;
		VK_TRUE,													//	VkBool32								depthBiasEnable;
		(setStatically ? depthBiasConstantFactor : 0.0f),			//	float									depthBiasConstantFactor;
		(setStatically ? depthBiasClamp : 0.0f),					//	float									depthBiasClamp;
		(setStatically ? depthBiasSlopeFactor : 0.0f),				//	float									depthBiasSlopeFactor;
		1.0f,														//	float									lineWidth;
	};

	const auto stencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0xFFu);

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													//	VkBool32								depthTestEnable;
		VK_TRUE,													//	VkBool32								depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								//	VkCompareOp								depthCompareOp;
		VK_FALSE,													//	VkBool32								depthBoundsTestEnable;
		VK_FALSE,													//	VkBool32								stencilTestEnable;
		stencilOp,													//	VkStencilOpState						front;
		stencilOp,													//	VkStencilOpState						back;
		0.0f,														//	float									minDepthBounds;
		1.0f,														//	float									maxDepthBounds;
	};

	std::vector<VkDynamicState> dynamicStates;
	if (!setStatically)
		dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		de::sizeU32(dynamicStates),								//	uint32_t							dynamicStateCount;
		de::dataOrNull(dynamicStates),							//	const VkDynamicState*				pDynamicStates;
	};

	const auto pipeline = makeGraphicsPipeline(
		ctx.vkd, ctx.device, pipelineLayout.get(),
		vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(),
		renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u,
		nullptr, &rasterizationStateCreateInfo, nullptr, &depthStencilStateCreateInfo, nullptr, &dynamicStateCreateInfo);

	// Command buffers.
	CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
	const auto primaryCmdBuffer = cmd.cmdBuffer.get();

	// Optional secondary command buffer
	const auto secondaryCmdBufferPtr	= (m_params.secondaryCmdBuffer
										? allocateCommandBuffer(ctx.vkd, ctx.device, cmd.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY)
										: Move<VkCommandBuffer>());
	const auto secondaryCmdBuffer		= (m_params.secondaryCmdBuffer ? secondaryCmdBufferPtr.get() : VK_NULL_HANDLE);
	const auto subpassContents			= (m_params.secondaryCmdBuffer ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);

	// For render pass contents.
	const auto rpCmdBuffer = (m_params.secondaryCmdBuffer ? secondaryCmdBuffer : primaryCmdBuffer);

	const std::vector<VkClearValue> clearValues
	{
		makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)),
		makeClearValueDepthStencil(1.0f, 0u),
	};

	beginCommandBuffer(ctx.vkd, primaryCmdBuffer);
	if (m_params.secondaryCmdBuffer)
		beginSecondaryCommandBuffer(ctx.vkd, secondaryCmdBuffer, renderPass.get(), framebuffer.get());
	beginRenderPass(ctx.vkd, primaryCmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues), subpassContents);

	// Render pass contents.
	ctx.vkd.cmdBindVertexBuffers(rpCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	ctx.vkd.cmdBindPipeline(rpCmdBuffer, bindPoint, pipeline.get());
	if (!setStatically)
	{
		if (m_params.setMechanism == SetMechanism::DYNAMIC_1)
		{
			DE_ASSERT(!m_params.reprInfo);
			ctx.vkd.cmdSetDepthBias(rpCmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
		}
		else if (m_params.setMechanism == SetMechanism::DYNAMIC_2)
		{
			const void* biasInfoPnext = (m_params.reprInfo ? &m_params.reprInfo.get() : nullptr);

			const VkDepthBiasInfoEXT depthBiasInfo =
			{
				VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT,	//	VkStructureType	sType;
				biasInfoPnext,							//	const void*		pNext;
				depthBiasConstantFactor,				//	float			depthBiasConstantFactor;
				depthBiasClamp,							//	float			depthBiasClamp;
				depthBiasSlopeFactor,					//	float			depthBiasSlopeFactor;
			};
			ctx.vkd.cmdSetDepthBias2EXT(rpCmdBuffer, &depthBiasInfo);
		}
		else
			DE_ASSERT(false);
	}
	ctx.vkd.cmdDraw(rpCmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);

	if (m_params.secondaryCmdBuffer)
	{
		endCommandBuffer(ctx.vkd, secondaryCmdBuffer);
		ctx.vkd.cmdExecuteCommands(primaryCmdBuffer, 1u, &secondaryCmdBuffer);
	}
	endRenderPass(ctx.vkd, primaryCmdBuffer);

	// Copy color and depth buffers to their verification buffers.
	{
		const std::vector<VkImageMemoryBarrier> preTransferBarriers
		{
			makeImageMemoryBarrier(
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				depthBuffer.getImage(), depthSRR),

			makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				colorBuffer.getImage(), colorSRR),
		};

		const auto preTransferStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		cmdPipelineImageMemoryBarrier(ctx.vkd, primaryCmdBuffer, preTransferStages, VK_PIPELINE_STAGE_TRANSFER_BIT, de::dataOrNull(preTransferBarriers), de::sizeU32(preTransferBarriers));

		const auto depthRegion = makeBufferImageCopy(vkExtent, depthSRL);
		const auto colorRegion = makeBufferImageCopy(vkExtent, colorSRL);

		ctx.vkd.cmdCopyImageToBuffer(primaryCmdBuffer, depthBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthBuffer.getBuffer(), 1u, &depthRegion);
		ctx.vkd.cmdCopyImageToBuffer(primaryCmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getBuffer(), 1u, &colorRegion);

		const auto transfer2Host = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &transfer2Host);
	}

	endCommandBuffer(ctx.vkd, primaryCmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, primaryCmdBuffer);

	// Invalidate allocations and verify contents.
	invalidateAlloc(ctx.vkd, ctx.device, depthBuffer.getBufferAllocation());
	invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

	// Depth reference.
	tcu::TextureLevel		depthReferenceLevel		(tcuDepthFormat, fbExtent.x(), fbExtent.y());
	tcu::PixelBufferAccess	depthReferenceAccess	(depthReferenceLevel.getAccess());
	const bool				noClamp					= (m_params.depthBiasClamp == 0.0f);
	const float				clampedBias				= std::min(m_params.targetBias, (noClamp ? m_params.targetBias : m_params.depthBiasClamp));
	const float				expectedDepth			= sampleDepth + clampedBias; // Must match vertex depth + actual bias.
	tcu::clearDepth(depthReferenceAccess, expectedDepth);

	// We calculated depth bias constant factors based on the most precise minimum resolvable diff, but the actual resolvable diff
	// may be bigger in some cases. We take that into account when calculating the error threshold for depth values, and we add the
	// format precision on top.
	const double			constantFactorD			= static_cast<double>(depthBiasConstantFactor);
	const double			constantBiasMin			= constantFactorD * rValue.first;
	const double			constantBiasMax			= constantFactorD * rValue.second;
	const double			constantBiasErrorThres	= constantBiasMax - constantBiasMin;
	const float				depthThreshold			= static_cast<float>(constantBiasErrorThres + getDepthErrorThreshold(tcuDepthFormat, expectedDepth));
	{
		log
			<< tcu::TestLog::Message << "Constant Bias Min:             " << constantBiasMin << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Constant Bias Max:             " << constantBiasMax << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Constant Bias Error Threshold: " << constantBiasErrorThres << tcu::TestLog::EndMessage
			;
	}

	// Color reference.
	const tcu::Vec4&	expectedColor	= DepthBiasControlCase::kOutColor;
	const tcu::Vec4		colorThreshold	(0.0f, 0.0f, 0.0f, 0.0f); // Expect exact result in color.

	// Result pixel buffer accesses.
	const tcu::ConstPixelBufferAccess depthResultAccess (tcuDepthFormat, fbExtent, depthBuffer.getBufferAllocation().getHostPtr());
	const tcu::ConstPixelBufferAccess colorResultAccess (tcuColorFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

	bool fail = false;

	if (!tcu::dsThresholdCompare(log, "DepthResult", "", depthReferenceAccess, depthResultAccess, depthThreshold, tcu::COMPARE_LOG_ON_ERROR))
	{
		log << tcu::TestLog::Message << "Depth buffer failed: expected " << expectedDepth << " (threshold " << depthThreshold
			<< ") and found " << depthResultAccess.getPixDepth(0, 0) << tcu::TestLog::EndMessage;
		fail = true;
	}

	if (!tcu::floatThresholdCompare(log, "ColorResult", "", expectedColor, colorResultAccess, colorThreshold, tcu::COMPARE_LOG_ON_ERROR))
	{
		log << tcu::TestLog::Message << "Color buffer failed: expected " << expectedColor << " (threshold " << colorThreshold
			<< ") and found " << depthResultAccess.getPixel(0, 0) << tcu::TestLog::EndMessage;
		fail = true;
	}

	if (fail)
		return tcu::TestStatus::fail("Failed -- check log for details");
	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createDepthBiasControlTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	const std::vector<VkFormat> attachmentFormats
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const struct
	{
		const UsedFactor	usedFactor;
		const char*			name;
	} usedFactorCases[] =
	{
		{ UsedFactor::SLOPE,	"slope"		},
		{ UsedFactor::CONSTANT,	"constant"	},
	};

	const struct
	{
		const MaybeRepr	reprInfo;
		const char*		name;
	} reprInfoCases[] =
	{
		{ tcu::Nothing,																										"no_repr_info"			},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, false),		"format_inexact"			},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT, true),			"format_exact"		},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT, false),	"force_unorm_inexact"		},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT, true),	"force_unorm_exact"	},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT, false),									"float_inexact"			},
		{ makeDepthBiasRepresentationInfo(VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT, true),									"float_exact"			},
	};

	struct ConstantDepthCase
	{
		const float constantDepth;
		const char* name;
	};
	using ConstantDepthCaseVec = std::vector<ConstantDepthCase>;

	const ConstantDepthCaseVec constantDepthSlopeCases // For these subcases, the constant depth is not used.
	{
		{ 0.0f,				"slope_depth_1_0"				},
	};
	const ConstantDepthCaseVec constantDepthConstantCases
	{
		{ 0.25f,			"constant_depth_0_25"			},
		{ 0.3125f,			"constant_depth_0_3125"			},
		{ 0.489742279053f,	"constant_depth_close_to_0_5"	},
		{ 0.625f,			"constant_depth_0_625"			},
		{ 0.125f,			"constant_depth_0_125"			},
	};

	const struct
	{
		const float targetBias;
		const char* name;
	} targetBiasCases[] =
	{
		{ 0.0625f,	"target_bias_0_0625"	},
		{ 0.125f,	"target_bias_0_125"		},
		{ 0.25f,	"target_bias_0_25"		},
	};

	const struct
	{
		const SetMechanism		setMechanism;
		const char*				name;
	} setMechanismCases[] =
	{
		{ SetMechanism::STATIC,		"static"			},
		{ SetMechanism::DYNAMIC_1,	"dynamic_set_1"		},
		{ SetMechanism::DYNAMIC_2,	"dynamic_set_2"		},
	};

	enum class ClampCase { ZERO = 0, LARGE = 1, SMALL = 2 };
	const struct
	{
		const ClampCase		clampCase;
		const char*			suffix;
	} clampValueCases[] =
	{
		{	ClampCase::ZERO,		"_no_clamp"				},
		{	ClampCase::LARGE,		"_no_effective_clamp"	},
		{	ClampCase::SMALL,		"_clamp_to_half"		},
	};

	const struct
	{
		const bool	secondaryCmdBuffer;
		const char*	suffix;
	} secondaryCmdBufferCases[] =
	{
		{ false,	""						},
		{ true,		"_secondary_cmd_buffer"	},
	};

	GroupPtr dbcGroup (new tcu::TestCaseGroup(testCtx, "depth_bias_control", "Tests for VK_EXT_depth_bias_control"));

	for (const auto& format : attachmentFormats)
	{
		const auto formatName = getFormatNameShort(format);

		GroupPtr formatGroup (new tcu::TestCaseGroup(testCtx, formatName.c_str(), ""));

		for (const auto& reprInfoCase : reprInfoCases)
		{
			GroupPtr reprInfoGroup (new tcu::TestCaseGroup(testCtx, reprInfoCase.name, ""));

			for (const auto& usedFactorCase : usedFactorCases)
			{
				GroupPtr usedFactorGroup (new tcu::TestCaseGroup(testCtx, usedFactorCase.name, ""));

				const bool constantFactor = (usedFactorCase.usedFactor == UsedFactor::CONSTANT);
				const ConstantDepthCaseVec& constantDepthCases = (constantFactor ? constantDepthConstantCases : constantDepthSlopeCases);

				for (const auto& constantDepthCase : constantDepthCases)
				{
					GroupPtr constantDepthGroup (new tcu::TestCaseGroup(testCtx, constantDepthCase.name, ""));

					for (const auto& targetBiasCase : targetBiasCases)
					{
						GroupPtr targetBiasGroup (new tcu::TestCaseGroup(testCtx, targetBiasCase.name, ""));

						for (const auto& setMechanismCase : setMechanismCases)
						{
							// We cannot use the representation info with vkCmdSetDepthBias.
							if (setMechanismCase.setMechanism == SetMechanism::DYNAMIC_1 && static_cast<bool>(reprInfoCase.reprInfo))
								continue;

							for (const auto& clampValueCase : clampValueCases)
							{
								float depthBiasClamp = 0.0f;
								switch (clampValueCase.clampCase)
								{
								case ClampCase::ZERO:			depthBiasClamp = 0.0f;									break;
								case ClampCase::LARGE:			depthBiasClamp = targetBiasCase.targetBias * 2.0f;		break;
								case ClampCase::SMALL:			depthBiasClamp = targetBiasCase.targetBias * 0.5f;		break;
								default:						DE_ASSERT(false);										break;
								}

								for (const auto& secondaryCmdBufferCase : secondaryCmdBufferCases)
								{
									// Some selected combinations will use secondary command buffers. Avoid applying this to all
									// combinations to keep the number of cases low.
									if (secondaryCmdBufferCase.secondaryCmdBuffer)
									{
										if (usedFactorCase.usedFactor != UsedFactor::CONSTANT)
											continue;

										if (setMechanismCase.setMechanism == SetMechanism::DYNAMIC_1)
											continue;

										if (clampValueCase.clampCase != ClampCase::ZERO)
											continue;

										if (!static_cast<bool>(reprInfoCase.reprInfo))
											continue;
									}

									const TestParams params
									{
										format,
										reprInfoCase.reprInfo,
										setMechanismCase.setMechanism,
										targetBiasCase.targetBias,
										usedFactorCase.usedFactor,
										constantDepthCase.constantDepth,
										depthBiasClamp,
										secondaryCmdBufferCase.secondaryCmdBuffer,
									};
									const std::string testName = std::string(setMechanismCase.name) + clampValueCase.suffix + secondaryCmdBufferCase.suffix;
									targetBiasGroup->addChild(new DepthBiasControlCase(testCtx, testName, "", params));
								}
							}
						}

						constantDepthGroup->addChild(targetBiasGroup.release());
					}

					usedFactorGroup->addChild(constantDepthGroup.release());
				}

				reprInfoGroup->addChild(usedFactorGroup.release());
			}

			formatGroup->addChild(reprInfoGroup.release());
		}

		dbcGroup->addChild(formatGroup.release());
	}

	return dbcGroup.release();
}

} // rasterization
} // vkt
