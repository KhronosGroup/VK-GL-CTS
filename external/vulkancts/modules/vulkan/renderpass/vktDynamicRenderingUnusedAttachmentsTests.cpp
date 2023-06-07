/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation
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
 * \brief VK_EXT_dynamic_rendering_unused_attachments Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicRenderingUnusedAttachmentsTests.hpp"
#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <bitset>
#include <algorithm>
#include <iterator>
#include <iomanip>

namespace vkt
{
namespace renderpass
{

namespace
{

using namespace vk;

static constexpr	VkFormat	kColorFormat	= VK_FORMAT_R8G8B8A8_UINT;
static constexpr	VkFormat	kBadColorFormat	= VK_FORMAT_R32G32B32A32_UINT;

std::vector<VkFormat> getDSFormatList (void)
{
	// The spec mandates support for one of these two formats.
	static const VkFormat kDSFormatList[] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	return std::vector<VkFormat>(kDSFormatList, kDSFormatList + de::arrayLength(kDSFormatList));
}

// Find a suitable format for the depth/stencil buffer.
VkFormat chooseDepthStencilFormat (const InstanceInterface& vki, VkPhysicalDevice physDev)
{
	const auto candidates = getDSFormatList();

	for (const auto& format : candidates)
	{
		const auto properties = getPhysicalDeviceFormatProperties(vki, physDev, format);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u)
			return format;
	}

	TCU_FAIL("No suitable depth/stencil format found");
	return VK_FORMAT_UNDEFINED; // Unreachable.
}

// Return a different depth/stencil format from the one chosen.
VkFormat chooseAltDSFormat (VkFormat chosenFormat)
{
	const auto candidates = getDSFormatList();

	for (const auto& format : candidates)
	{
		if (format != chosenFormat)
			return format;
	}

	DE_ASSERT(false);
	return candidates.at(0u);
}

struct TestParams
{
	static constexpr uint32_t kMaxFragAttachments			= 8u;						// Based on real-world maxFragmentOutputAttachments values.
	static constexpr uint32_t kMaxFramebufferAttachments	= 2u * kMaxFragAttachments;	// Slightly arbitrary, based on the previous number.

	const uint32_t	pipeFBAttachmentCount;		// Number of attachments specified in the pipeline and framebuffer (VUID-vkCmdDraw-colorAttachmentCount-06179).
	const uint32_t	fragAttachmentCount;		// Frag shader outputs. Needs to be >= pipeFBAttachmentCount.

	const uint32_t	layerCount;					// Image layers.
	const uint32_t	layerMask;					// Which layers are going to be written to, either using viewMask or manual calls.
	const bool		multiView;					// Manual or "automatic" layer handling.

	const uint32_t	formatMask;					// Which attachments will have VK_FORMAT_UNDEFINED in the pipeline (0 for undefined, 1 for defined).
	const uint32_t	framebufferMask;			// Which attachments will be VK_NULL_HANDLE in the framebuffer (0 for null, 1 for valid handle).

	const bool		depthPresent;				// Create the pipeline with a depth attachment or not.
	const bool		depthDefined;				// Make the depth attachment have VK_FORMAT_UNDEFINED in the pipeline or not.
	const bool		depthValidHandle;			// Make the depth attachment be VK_NULL_HANDLE in the framebuffer or not.

	const bool		stencilPresent;				// Create the pipeline with a stencil attachment or not.
	const bool		stencilDefined;				// Make the stencil attachment have VK_FORMAT_UNDEFINED in the pipeline or not.
	const bool		stencilValidHandle;			// Make the stencil attachment be VK_NULL_HANDLE in the framebuffer or not.

	const bool		useSecondaries;				// Use secondary command buffers inside the render pass.
	const bool		wrongFormatWithNullViews;	// Use the wrong format value if the image view handle is VK_NULL_HANDLE.

	TestParams (uint32_t	pipeFBAttachmentCount_,
				uint32_t	fragAttachmentCount_,
				uint32_t	layerCount_,
				uint32_t	layerMask_,
				bool		multiView_,
				uint32_t	formatMask_,
				uint32_t	framebufferMask_,
				bool		depthPresent_,
				bool		depthDefined_,
				bool		depthValidHandle_,
				bool		stencilPresent_,
				bool		stencilDefined_,
				bool		stencilValidHandle_,
				bool		useSecondaries_,
				bool		wrongFormatWithNullViews_)
		: pipeFBAttachmentCount			(pipeFBAttachmentCount_)
		, fragAttachmentCount			(fragAttachmentCount_)
		, layerCount					(layerCount_)
		, layerMask						(layerMask_)
		, multiView						(multiView_)
		, formatMask					(formatMask_)
		, framebufferMask				(framebufferMask_)
		, depthPresent					(depthPresent_)
		, depthDefined					(depthDefined_)
		, depthValidHandle				(depthValidHandle_)
		, stencilPresent				(stencilPresent_)
		, stencilDefined				(stencilDefined_)
		, stencilValidHandle			(stencilValidHandle_)
		, useSecondaries				(useSecondaries_)
		, wrongFormatWithNullViews		(wrongFormatWithNullViews_)
	{
		DE_ASSERT(fragAttachmentCount <= kMaxFragAttachments);
		DE_ASSERT(pipeFBAttachmentCount <= kMaxFramebufferAttachments);
		DE_ASSERT(fragAttachmentCount >= pipeFBAttachmentCount);
		DE_ASSERT(layerCount >= 1u);
	}

private:
	inline const char* getFlagText (bool flag, const char* trueText, const char* falseText) const
	{
		return (flag ? trueText : falseText);
	}

	inline const char* getPresent	(bool flag) const	{ return getFlagText(flag, "yes", "no");		}
	inline const char* getDefined	(bool flag) const	{ return getFlagText(flag, "def", "undef");		}
	inline const char* getValid		(bool flag) const	{ return getFlagText(flag, "valid", "null");	}

public:
	std::string getTestName (void) const
	{
		// Yes, this is an awfully long string.
		std::ostringstream testName;
		testName
			<< "pipe_"			<< pipeFBAttachmentCount
			<< "_frag_"			<< fragAttachmentCount
			<< "_layers_"		<< layerCount
			<< "_mask_0x"		<< std::hex << std::setfill('0') << std::setw(2) << layerMask
			<< "_formats_0x"	<< std::hex << std::setfill('0') << std::setw(8) << formatMask
			<< "_handles_0x"	<< std::hex << std::setfill('0') << std::setw(8) << framebufferMask
			<< "_depth_"		<< getPresent(depthPresent) << "_" << getDefined(depthDefined) << "_" << getValid(depthValidHandle)
			<< "_stencil_"		<< getPresent(stencilPresent) << "_" << getDefined(stencilDefined) << "_" << getValid(stencilValidHandle)
			<< (multiView ? "_multiview" : "")
			//<< (wrongFormatWithNullViews ? "_bad_formats" : "")
			;
		return testName.str();
	}

	bool depthStencilNeeded (void) const
	{
		return (depthPresent || stencilPresent);
	}

	// Returns true if the vertex shader has to write to the Layer built-in.
	bool vertExportsLayer (void) const
	{
		return (!multiView && layerCount > 1u);
	}

protected:
	std::vector<VkFormat> getFormatVectorForMask (const VkFormat colorFormat, const uint32_t bitMask, const uint32_t attachmentCount) const
	{
		std::bitset<kMaxFragAttachments>	mask	(static_cast<unsigned long long>(bitMask));
		std::vector<VkFormat>				formats;

		formats.reserve(attachmentCount);
		for (uint32_t attIdx = 0u; attIdx < attachmentCount; ++attIdx)
			formats.push_back(mask[attIdx] ? colorFormat : VK_FORMAT_UNDEFINED);

		return formats;
	}

public:
	std::vector<VkFormat> getPipelineFormatVector (const VkFormat colorFormat) const
	{
		return getFormatVectorForMask(colorFormat, formatMask, pipeFBAttachmentCount);
	}

	std::vector<VkFormat> getInheritanceFormatVector (const VkFormat colorFormat) const
	{
		return getFormatVectorForMask(colorFormat, framebufferMask, pipeFBAttachmentCount);
	}

	inline VkFormat getPipelineDepthFormat (VkFormat dsFormat) const
	{
		return ((depthPresent && depthDefined) ? dsFormat : VK_FORMAT_UNDEFINED);
	}

	inline VkFormat getInheritanceDepthFormat (VkFormat dsFormat) const
	{
		return ((depthPresent && depthValidHandle) ? dsFormat : VK_FORMAT_UNDEFINED);
	}

	inline VkFormat getPipelineStencilFormat (VkFormat dsFormat) const
	{
		return ((stencilPresent && stencilDefined) ? dsFormat : VK_FORMAT_UNDEFINED);
	}

	inline VkFormat getInheritanceStencilFormat (VkFormat dsFormat) const
	{
		return ((stencilPresent && stencilValidHandle) ? dsFormat : VK_FORMAT_UNDEFINED);
	}

	static VkClearValue getClearValue (void)
	{
		VkClearValue clearValue;
		deMemset(&clearValue, 0, sizeof(clearValue));
		return clearValue;
	}

	std::vector<VkRenderingAttachmentInfo> getRenderingAttachmentInfos (const std::vector<VkImageView>& imageViews) const
	{
		DE_ASSERT(imageViews.size() == static_cast<size_t>(pipeFBAttachmentCount));

		std::bitset<kMaxFramebufferAttachments>	mask		(static_cast<unsigned long long>(framebufferMask));
		const auto								clearValue	= getClearValue();
		std::vector<VkRenderingAttachmentInfo>	infos;

		infos.reserve(pipeFBAttachmentCount);
		for (uint32_t attIdx = 0u; attIdx < pipeFBAttachmentCount; ++attIdx)
		{
			const auto imgView = (mask[attIdx] ? imageViews.at(attIdx) : VK_NULL_HANDLE);

			infos.push_back(VkRenderingAttachmentInfo{
				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,	//	VkStructureType			sType;
				nullptr,										//	const void*				pNext;
				imgView,										//	VkImageView				imageView;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//	VkImageLayout			imageLayout;
				VK_RESOLVE_MODE_NONE,							//	VkResolveModeFlagBits	resolveMode;
				VK_NULL_HANDLE,									//	VkImageView				resolveImageView;
				VK_IMAGE_LAYOUT_UNDEFINED,						//	VkImageLayout			resolveImageLayout;
				VK_ATTACHMENT_LOAD_OP_LOAD,						//	VkAttachmentLoadOp		loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,					//	VkAttachmentStoreOp		storeOp;
				clearValue,										//	VkClearValue			clearValue;
			});
		}

		return infos;
	}

	VkRenderingAttachmentInfo getDepthAttachmentInfo (const VkImageView imageView) const
	{
		const auto clearValue	= getClearValue();
		const auto attView		= ((depthPresent && depthValidHandle) ? imageView : VK_NULL_HANDLE);

		return VkRenderingAttachmentInfo{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,		//	VkStructureType			sType;
			nullptr,											//	const void*				pNext;
			attView,											//	VkImageView				imageView;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								//	VkResolveModeFlagBits	resolveMode;
			VK_NULL_HANDLE,										//	VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							//	VkImageLayout			resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_LOAD,							//	VkAttachmentLoadOp		loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp		storeOp;
			clearValue,											//	VkClearValue			clearValue;
		};
	}

	VkRenderingAttachmentInfo getStencilAttachmentInfo (const VkImageView imageView) const
	{
		const auto clearValue	= getClearValue();
		const auto attView		= ((stencilPresent && stencilValidHandle) ? imageView : VK_NULL_HANDLE);

		return VkRenderingAttachmentInfo{
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,		//	VkStructureType			sType;
			nullptr,											//	const void*				pNext;
			attView,											//	VkImageView				imageView;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	//	VkImageLayout			imageLayout;
			VK_RESOLVE_MODE_NONE,								//	VkResolveModeFlagBits	resolveMode;
			VK_NULL_HANDLE,										//	VkImageView				resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,							//	VkImageLayout			resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_LOAD,							//	VkAttachmentLoadOp		loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						//	VkAttachmentStoreOp		storeOp;
			clearValue,											//	VkClearValue			clearValue;
		};
	}
};

class DynamicUnusedAttachmentsInstance : public vkt::TestInstance
{
public:
					DynamicUnusedAttachmentsInstance	(Context& context, const TestParams& params)
						: vkt::TestInstance	(context)
						, m_params			(params)
						{}
	virtual			~DynamicUnusedAttachmentsInstance	(void) {}

	tcu::TestStatus	iterate								(void) override;

protected:
	const TestParams m_params;
};

class DynamicUnusedAttachmentsCase : public vkt::TestCase
{
public:
					DynamicUnusedAttachmentsCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}
	TestInstance*	createInstance					(Context& context) const override { return new DynamicUnusedAttachmentsInstance(context, m_params); }
	void			initPrograms					(vk::SourceCollections& programCollection) const override;
	void			checkSupport					(Context& context) const override;

protected:
	const TestParams m_params;
};

void DynamicUnusedAttachmentsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const bool vertExportsLayer = m_params.vertExportsLayer();

	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "#extension GL_ARB_shader_viewport_layer_array : enable\n"
		<< "layout (push_constant, std430) uniform PushConstantBlock { int layerIndex; } pc;\n"
		<< "vec2 positions[3] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),\n"
		<< "    vec2(-1.0,  3.0),\n"
		<< "    vec2( 3.0, -1.0)\n"
		<< ");\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4(positions[gl_VertexIndex % 3], 1.0, 1.0);\n"
		<< (vertExportsLayer ? "    gl_Layer = pc.layerIndex;\n" : "")
		<< "}\n"
		;
	{
		// This is required by the validation layers for the program to be correct. A SPIR-V 1.0 module that exports the Layer
		// built-in will use the ShaderViewportIndexLayerEXT capability, which is enabled by the VK_EXT_shader_viewport_index_layer
		// extension.
		//
		// However, in Vulkan 1.2+ the extension was promoted to core and that capability was replaced by the ShaderLayer and
		// ShaderViewportIndex capabilities, which are enabled by the shaderOutputViewportIndex and shaderOutputLayer features in
		// VkPhysicalDeviceVulkan12Features. In a Vulkan 1.2+ context, CTS will not enable VK_EXT_shader_viewport_index_layer as
		// that's part of the core extensions, and will enable the Vulkan 1.2 features instead. These will allow access to the
		// ShaderLayer and ShaderViewportIndex capabilities, but not the ShaderViewportIndexLayerEXT capability.
		//
		// When building the vertex module, glslang will, by default, target SPIR-V 1.0 and create a module that uses the
		// ShaderViewportIndexLayerEXT capability. When targetting SPIR-V 1.5 explicitly, glslang will generate a module that uses
		// the ShaderLayer capability.
		//
		// We cannot use a SPIR-V 1.0 module in a Vulkan 1.2+ context, because it will use the ShaderViewportIndexLayerEXT
		// capability, which will not be enabled. In that case, we must use a SPIR-V 1.5 module that depends on the ShaderLayer
		// capability.
		//
		// We cannot use a SPIR-V 1.5 module in a Vulkan <1.2 context, because it will use the ShaderLayer capability, which will
		// not be enabled. In these cases, we must use a SPIR-V 1.0 module that depends on the ShaderViewportIndexLayerEXT
		// capability.
		//
		// So we need both versions of the vertex shader and we need to choose at runtime.
		//
		const auto						src			= vert.str();
		const vk::ShaderBuildOptions	spv15Opts	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, false);

		programCollection.glslSources.add("vert-spv10") << glu::VertexSource(src);
		programCollection.glslSources.add("vert-spv15") << glu::VertexSource(src) << spv15Opts;
	}

	// Make sure the fragment shader does not write to any attachment which will have an undefined format in the pipeline.
	std::vector<bool>	fragAttachmentUsed	(m_params.fragAttachmentCount, true);
	const auto			pipelineFormats		= m_params.getPipelineFormatVector(kColorFormat);

	for (size_t i = 0; i < pipelineFormats.size(); ++i)
	{
		if (pipelineFormats[i] == VK_FORMAT_UNDEFINED)
			fragAttachmentUsed.at(i) = false;
	}

	std::ostringstream frag;

	frag
		<< "#version 460\n"
		<< "#extension " << (m_params.multiView ? "GL_EXT_multiview" : "GL_ARB_shader_viewport_layer_array") << " : enable\n";
		;

	// Color outputs.
	for (uint32_t i = 0u; i < m_params.fragAttachmentCount; ++i)
	{
		if (fragAttachmentUsed.at(i))
			frag << "layout (location=" << i << ") out uvec4 color" << i << ";\n";
	}

	const char* layerIndexExpr;

	if (m_params.multiView)
		layerIndexExpr = "uint(gl_ViewIndex)";
	else if (vertExportsLayer)
		layerIndexExpr = "uint(gl_Layer)";
	else
		layerIndexExpr = "0u";

	frag
		<< "void main (void) {\n"
		<< "    const uint layerIndex = " << layerIndexExpr << ";\n"
		;

	for (uint32_t i = 0u; i < m_params.fragAttachmentCount; ++i)
	{
		if (fragAttachmentUsed.at(i))
			frag << "    color" << i << " = uvec4(layerIndex, 255, " << i << ", 255);\n";
	}

	frag << "}\n";

	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void DynamicUnusedAttachmentsCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	context.requireDeviceFunctionality("VK_EXT_dynamic_rendering_unused_attachments");

	const auto& properties = context.getDeviceProperties();
	if (m_params.fragAttachmentCount > properties.limits.maxFragmentOutputAttachments)
		TCU_THROW(NotSupportedError, "Unsupported number of attachments");

	if (m_params.vertExportsLayer())
	{
		// This will check the right extension or Vulkan 1.2 features automatically.
		context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");

		// We also need geometry shader support to be able to use gl_Layer from frag shaders.
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	}

	if (m_params.multiView)
		context.requireDeviceFunctionality("VK_KHR_multiview");
}

tcu::TestStatus DynamicUnusedAttachmentsInstance::iterate (void)
{
	const auto			ctx			= m_context.getContextCommonData();
	const tcu::IVec3	fbDim		(1, 1, 1);
	const auto			fbExtent	= makeExtent3D(fbDim);
	const auto			fbSamples	= VK_SAMPLE_COUNT_1_BIT;
	const auto			colorUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto			dsUsage		= (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto			colorSRR	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_params.layerCount);
	const auto			dsSRR		= makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, m_params.layerCount);
	const auto			colorSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_params.layerCount);
	const auto			depthSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, m_params.layerCount);
	const auto			stencilSRL	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, m_params.layerCount);
	const auto			dsNeeded	= m_params.depthStencilNeeded();

	using ImageWithBufferPtr = std::unique_ptr<ImageWithBuffer>;

	// Allocate color attachments.
	std::vector<ImageWithBufferPtr> colorImages (m_params.pipeFBAttachmentCount);
	for (uint32_t i = 0; i < m_params.pipeFBAttachmentCount; ++i)
		colorImages[i].reset(new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent, kColorFormat, colorUsage, VK_IMAGE_TYPE_2D, colorSRR, m_params.layerCount));

	VkFormat							dsFormat = VK_FORMAT_UNDEFINED;
	std::unique_ptr<ImageWithMemory>	dsImage;
	Move<VkImageView>					dsImageView;
	tcu::TextureFormat					depthCopyFormat;
	tcu::TextureFormat					stencilCopyFormat;
	std::unique_ptr<BufferWithMemory>	depthVerificationBuffer;
	std::unique_ptr<BufferWithMemory>	stencilVerificationBuffer;

	if (dsNeeded)
		dsFormat = chooseDepthStencilFormat(ctx.vki, ctx.physicalDevice);

	if (dsNeeded)
	{
		const VkImageCreateInfo dsCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			dsFormat,								//	VkFormat				format;
			fbExtent,								//	VkExtent3D				extent;
			1u,										//	uint32_t				mipLevels;
			m_params.layerCount,					//	uint32_t				arrayLayers;
			fbSamples,								//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			dsUsage,								//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	uint32_t				queueFamilyIndexCount;
			nullptr,								//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		dsImage.reset(new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, MemoryRequirement::Any));

		const auto dsImageViewType	= (m_params.layerCount > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
		dsImageView					= makeImageView(ctx.vkd, ctx.device, dsImage->get(), dsImageViewType, dsFormat, dsSRR);
		depthCopyFormat				= getDepthCopyFormat(dsFormat);
		stencilCopyFormat			= getStencilCopyFormat(dsFormat);

		const auto depthVerificationBufferSize = static_cast<VkDeviceSize>(tcu::getPixelSize(depthCopyFormat) * fbExtent.width * fbExtent.height * fbExtent.depth * m_params.layerCount);
		const auto depthVerificationBufferInfo = makeBufferCreateInfo(depthVerificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		depthVerificationBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, depthVerificationBufferInfo, MemoryRequirement::HostVisible));

		const auto stencilVerificationBufferSize = static_cast<VkDeviceSize>(tcu::getPixelSize(stencilCopyFormat) * fbExtent.width * fbExtent.height * fbExtent.depth * m_params.layerCount);
		const auto stencilVerificationBufferInfo = makeBufferCreateInfo(stencilVerificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		stencilVerificationBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, stencilVerificationBufferInfo, MemoryRequirement::HostVisible));
	}

	const std::vector<VkViewport>	viewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vk12Support	= m_context.contextSupports(vk::ApiVersion(0u, 1u, 2u, 0u));
	const auto	vertModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get(vk12Support ? "vert-spv15" : "vert-spv10"));
	const auto	fragModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	const auto pcSize			= static_cast<uint32_t>(sizeof(int32_t));
	const auto pcStages			= VK_SHADER_STAGE_VERTEX_BIT;
	const auto pcRange			= makePushConstantRange(pcStages, 0u, pcSize);
	const auto pipelineLayout	= makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo	= initVulkanStructure();
	const auto									stencilOpState				= makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_GREATER_OR_EQUAL, 0xFFu, 0xFFu, 0xFFu);
	// If the depth or stencil test is enabled and the image view is not VK_NULL_HANDLE, the format cannot be UNDEFINED.
	const auto									depthEnabled				= (m_params.depthPresent	&& !(!m_params.depthDefined		&& m_params.depthValidHandle));
	const auto									stencilEnabled				= (m_params.stencilPresent	&& !(!m_params.stencilDefined	&& m_params.stencilValidHandle));
	const VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineDepthStencilStateCreateFlags	flags;
		depthEnabled,												//	VkBool32								depthTestEnable;
		depthEnabled,												//	VkBool32								depthWriteEnable;
		VK_COMPARE_OP_GREATER_OR_EQUAL,								//	VkCompareOp								depthCompareOp;
		VK_FALSE,													//	VkBool32								depthBoundsTestEnable;
		stencilEnabled,												//	VkBool32								stencilTestEnable;
		stencilOpState,												//	VkStencilOpState						front;
		stencilOpState,												//	VkStencilOpState						back;
		0.0f,														//	float									minDepthBounds;
		1.0f,														//	float									maxDepthBounds;
	};

	auto colorPipelineFormats	= m_params.getPipelineFormatVector(kColorFormat);
	auto depthPipelineFormat	= m_params.getPipelineDepthFormat(dsFormat);
	auto stencilPipelineFormat	= m_params.getPipelineStencilFormat(dsFormat);
	const auto viewMask			= (m_params.multiView ? m_params.layerMask : 0u);

	std::vector<VkImageView> rawColorViews;
	rawColorViews.reserve(colorImages.size());
	std::transform(begin(colorImages), end(colorImages), std::back_inserter(rawColorViews),
		[](const ImageWithBufferPtr& ib) { return (ib.get() ? ib->getImageView() : VK_NULL_HANDLE); });

	const auto renderingAttInfos = m_params.getRenderingAttachmentInfos(rawColorViews);

	using RenderingAttachmentInfoPtr = std::unique_ptr<VkRenderingAttachmentInfo>;
	RenderingAttachmentInfoPtr depthAttachmentPtr;
	RenderingAttachmentInfoPtr stencilAttachmentPtr;

	if (dsNeeded)
	{
		const auto& imgView = dsImageView.get();
		DE_ASSERT(imgView != VK_NULL_HANDLE);
		depthAttachmentPtr.reset(new VkRenderingAttachmentInfo(m_params.getDepthAttachmentInfo(imgView)));
		stencilAttachmentPtr.reset(new VkRenderingAttachmentInfo(m_params.getStencilAttachmentInfo(imgView)));
	}

	if (m_params.wrongFormatWithNullViews)
	{
		DE_ASSERT(renderingAttInfos.size() == colorPipelineFormats.size());

		// Use wrong formats when the image view is VK_NULL_HANDLE.
		for (size_t i = 0u; i < renderingAttInfos.size(); ++i)
		{
			if (renderingAttInfos[i].imageView == VK_NULL_HANDLE)
				colorPipelineFormats[i] = kBadColorFormat;
		}

		const auto badDSFormat = chooseAltDSFormat(dsFormat);

		if (depthAttachmentPtr.get() && depthAttachmentPtr->imageView == VK_NULL_HANDLE)
			depthPipelineFormat = badDSFormat;

		if (stencilAttachmentPtr.get() && stencilAttachmentPtr->imageView == VK_NULL_HANDLE)
			stencilPipelineFormat = badDSFormat;
	}

	const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,	//	VkStructureType	sType;
		nullptr,												//	const void*		pNext;
		viewMask,												//	uint32_t		viewMask;
		de::sizeU32(colorPipelineFormats),						//	uint32_t		colorAttachmentCount;
		de::dataOrNull(colorPipelineFormats),					//	const VkFormat*	pColorAttachmentFormats;
		depthPipelineFormat,									//	VkFormat		depthAttachmentFormat;
		stencilPipelineFormat,									//	VkFormat		stencilAttachmentFormat;
	};

	const auto colorWriteMask		= (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	const auto colorBlendAttState	= makePipelineColorBlendAttachmentState(VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, colorWriteMask);

	const std::vector<VkPipelineColorBlendAttachmentState> colorBlendStateVec (pipelineRenderingCreateInfo.colorAttachmentCount, colorBlendAttState);

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp;
		de::sizeU32(colorBlendStateVec),							//	uint32_t									attachmentCount;
		de::dataOrNull(colorBlendStateVec),							//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
	};

	const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(),
		vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(),
		VK_NULL_HANDLE, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
		&vertexInputStateCreateInfo, nullptr, nullptr, &depthStencilStateCreateInfo, &colorBlendStateCreateInfo, nullptr, &pipelineRenderingCreateInfo);

	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = cmd.cmdBuffer.get();
	Move<VkCommandBuffer> secondaryCmdBuffer;

	if (m_params.useSecondaries)
		secondaryCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, cmd.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	const auto rpCmdBuffer = (m_params.useSecondaries ? secondaryCmdBuffer.get() : cmdBuffer);

	const auto renderingFlags	= (m_params.useSecondaries
								? static_cast<VkRenderingFlags>(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT)
								: 0);

	const VkRenderingInfo renderingInfo =
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,				//	VkStructureType						sType;
		nullptr,											//	const void*							pNext;
		renderingFlags,										//	VkRenderingFlags					flags;
		scissors.at(0u),									//	VkRect2D							renderArea;
		(m_params.multiView ? 1u : m_params.layerCount),	//	uint32_t							layerCount;
		viewMask,											//	uint32_t							viewMask;
		de::sizeU32(renderingAttInfos),						//	uint32_t							colorAttachmentCount;
		de::dataOrNull(renderingAttInfos),					//	const VkRenderingAttachmentInfo*	pColorAttachments;
		depthAttachmentPtr.get(),							//	const VkRenderingAttachmentInfo*	pDepthAttachment;
		stencilAttachmentPtr.get(),							//	const VkRenderingAttachmentInfo*	pStencilAttachment;
	};

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	// Transition the layout of every image.
	{
		std::vector<VkImageMemoryBarrier> initialLayoutBarriers;

		for (const auto& img : colorImages)
		{
			const auto colorBarrier = makeImageMemoryBarrier(
				0u,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				img->getImage(), colorSRR);
			initialLayoutBarriers.push_back(colorBarrier);
		}
		if (dsNeeded)
		{
			const auto dsBarrier = makeImageMemoryBarrier(
				0u,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				dsImage->get(), dsSRR);
			initialLayoutBarriers.push_back(dsBarrier);
		}

		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0u, 0u, nullptr, 0u, nullptr,
			de::sizeU32(initialLayoutBarriers), de::dataOrNull(initialLayoutBarriers));
	}

	// Clear images.
	{
		const auto clearValue = TestParams::getClearValue();

		for (const auto& img : colorImages)
			ctx.vkd.cmdClearColorImage(cmdBuffer, img->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1u, &colorSRR);
		if (dsNeeded)
			ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, dsImage->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.depthStencil, 1u, &dsSRR);
	}

	// Transition the layout of every image.
	{
		std::vector<VkImageMemoryBarrier> initialLayoutBarriers;

		for (const auto& img : colorImages)
		{
			const auto colorBarrier = makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				img->getImage(), colorSRR);
			initialLayoutBarriers.push_back(colorBarrier);
		}
		if (dsNeeded)
		{
			const auto dsBarrier = makeImageMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT,
				(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				dsImage->get(), dsSRR);
			initialLayoutBarriers.push_back(dsBarrier);
		}

		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
			0u, 0u, nullptr, 0u, nullptr,
			de::sizeU32(initialLayoutBarriers), de::dataOrNull(initialLayoutBarriers));
	}

	ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);

	if (m_params.useSecondaries)
	{
		// The inheritance info and framebuffer attachments must match (null handle -> undefined format, non-null handle -> valid format).
		// The pipeline rendering info will later be able to selectively disable an attachment.
		const auto inheritanceColorFormats	= m_params.getInheritanceFormatVector(kColorFormat);
		const auto inheritanceDepthFormat	= m_params.getInheritanceDepthFormat(dsFormat);
		const auto inheritanceStencilFormat	= m_params.getInheritanceStencilFormat(dsFormat);

		const VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,	//	VkStructureType			sType;
			nullptr,														//	const void*				pNext;
			0u,																//	VkRenderingFlags		flags;
			viewMask,														//	uint32_t				viewMask;
			de::sizeU32(inheritanceColorFormats),							//	uint32_t				colorAttachmentCount;
			de::dataOrNull(inheritanceColorFormats),						//	const VkFormat*			pColorAttachmentFormats;
			inheritanceDepthFormat,											//	VkFormat				depthAttachmentFormat;
			inheritanceStencilFormat,										//	VkFormat				stencilAttachmentFormat;
			fbSamples,														//	VkSampleCountFlagBits	rasterizationSamples;
		};

		const VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	//	VkStructureType					sType;
			&inheritanceRenderingInfo,							//	const void*						pNext;
			VK_NULL_HANDLE,										//	VkRenderPass					renderPass;
			0u,													//	uint32_t						subpass;
			VK_NULL_HANDLE,										//	VkFramebuffer					framebuffer;
			VK_FALSE,											//	VkBool32						occlusionQueryEnable;
			0u,													//	VkQueryControlFlags				queryFlags;
			0u,													//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		const VkCommandBufferBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			//	VkStructureType							sType;
			nullptr,												//	const void*								pNext;
			VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,		//	VkCommandBufferUsageFlags				flags;
			&inheritanceInfo,										//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		ctx.vkd.beginCommandBuffer(secondaryCmdBuffer.get(), &beginInfo);
	}

	ctx.vkd.cmdBindPipeline(rpCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	{
		const auto iterCount = (m_params.multiView ? 1u : m_params.layerCount);
		for (uint32_t i = 0; i < iterCount; ++i)
		{
			// In non-multiview mode, we have to skip some layers manually.
			if (!m_params.multiView && ((m_params.layerMask & (1u << i)) == 0u))
				continue;

			ctx.vkd.cmdPushConstants(rpCmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &i);
			ctx.vkd.cmdDraw(rpCmdBuffer, 3u, 1u, 0u, 0u);
		}
	}

	if (m_params.useSecondaries)
	{
		endCommandBuffer(ctx.vkd, secondaryCmdBuffer.get());
		ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &secondaryCmdBuffer.get());
	}

	ctx.vkd.cmdEndRendering(cmdBuffer);

	// Transition the layout of all images again for verification.
	{
		std::vector<VkImageMemoryBarrier> preCopyLayoutBarriers;

		for (const auto& img : colorImages)
		{
			const auto colorBarrier = makeImageMemoryBarrier(
				(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				img->getImage(), colorSRR);
			preCopyLayoutBarriers.push_back(colorBarrier);
		}
		if (dsNeeded)
		{
			const auto dsBarrier = makeImageMemoryBarrier(
				(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dsImage->get(), dsSRR);
			preCopyLayoutBarriers.push_back(dsBarrier);
		}

		ctx.vkd.cmdPipelineBarrier(cmdBuffer,
			(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0u, 0u, nullptr, 0u, nullptr,
			de::sizeU32(preCopyLayoutBarriers), de::dataOrNull(preCopyLayoutBarriers));
	}

	// Copy all image contents to their verification buffers (note depth/stencil uses two buffers).
	for (const auto& img : colorImages)
	{
		const auto copyRegion = makeBufferImageCopy(fbExtent, colorSRL);
		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, img->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img->getBuffer(), 1u, &copyRegion);
	}
	if (dsNeeded)
	{
		const auto depthCopyRegion		= makeBufferImageCopy(fbExtent, depthSRL);
		const auto stencilCopyRegion	= makeBufferImageCopy(fbExtent, stencilSRL);

		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsImage->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthVerificationBuffer->get(), 1u, &depthCopyRegion);
		ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsImage->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilVerificationBuffer->get(), 1u, &stencilCopyRegion);
	}

	// Global barrier to synchronize verification buffers to host reads.
	{
		const auto transfer2HostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &transfer2HostBarrier);
	}

	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	// Invalidate all allocations.
	for (uint32_t i = 0; i < m_params.pipeFBAttachmentCount; ++i)
		invalidateAlloc(ctx.vkd, ctx.device, colorImages.at(i)->getBufferAllocation());
	if (dsNeeded)
	{
		invalidateAlloc(ctx.vkd, ctx.device, depthVerificationBuffer->getAllocation());
		invalidateAlloc(ctx.vkd, ctx.device, stencilVerificationBuffer->getAllocation());
	}

	// Verify all layers in all images.
	const auto colorTcuFormat = mapVkFormat(kColorFormat);
	const auto colorPixelSize = tcu::getPixelSize(colorTcuFormat);
	const auto colorLayerSize = static_cast<size_t>(fbDim.x() * fbDim.y() * fbDim.z() * colorPixelSize);

	const tcu::UVec4	threshold	(0u, 0u, 0u, 0u); // We expect exact results.
	auto&				log			= m_context.getTestContext().getLog();
	bool				failure		= false;

	for (size_t colorImgIdx = 0u; colorImgIdx < colorImages.size(); ++colorImgIdx)
	{
		const auto&	colorImg	= colorImages.at(colorImgIdx);
		const auto	dataPtr		= reinterpret_cast<const char*>(colorImg->getBufferAllocation().getHostPtr());
		const bool	imgWritten	= (colorImgIdx < colorPipelineFormats.size() && colorPipelineFormats.at(colorImgIdx) != VK_FORMAT_UNDEFINED
								   && colorImgIdx < renderingAttInfos.size() && renderingAttInfos.at(colorImgIdx).imageView != VK_NULL_HANDLE);

		for (uint32_t layerIdx = 0u; layerIdx < m_params.layerCount; ++layerIdx)
		{
			const bool							layerWritten	= imgWritten && ((m_params.layerMask & (1 << layerIdx)) != 0u);
			const auto							layerDataPtr	= dataPtr + colorLayerSize * layerIdx;
			const tcu::ConstPixelBufferAccess	layerAccess		(colorTcuFormat, fbDim, layerDataPtr);
			const tcu::UVec4					expectedColor	= (layerWritten
																? tcu::UVec4(layerIdx, 255u, static_cast<uint32_t>(colorImgIdx), 255u) // Needs to match frag shader.
																: tcu::UVec4(0u, 0u, 0u, 0u));
			const std::string					logImgName		= "ColorAttachment" + std::to_string(colorImgIdx) + "-Layer" + std::to_string(layerIdx);
			tcu::TextureLevel					refLevel		(colorTcuFormat, fbDim.x(), fbDim.y(), fbDim.z());
			tcu::PixelBufferAccess				refAccess		= refLevel.getAccess();

			tcu::clear(refAccess, expectedColor);
			if (!tcu::intThresholdCompare(log, logImgName.c_str(), "", refAccess, layerAccess, threshold, tcu::COMPARE_LOG_EVERYTHING))
				failure = true;
		}
	}

	if (dsNeeded)
	{
		const bool depthWritten		= (m_params.depthPresent && m_params.depthDefined && m_params.depthValidHandle);
		const bool stencilWritten	= (m_params.stencilPresent && m_params.stencilDefined && m_params.stencilValidHandle);

		// Depth.
		{
			const auto dataPtr			= reinterpret_cast<const char*>(depthVerificationBuffer->getAllocation().getHostPtr());
			const auto depthPixelSize	= tcu::getPixelSize(depthCopyFormat);
			const auto depthLayerSize	= static_cast<size_t>(fbDim.x() * fbDim.y() * fbDim.z() * depthPixelSize);
			const auto depthThreshold	= 0.0f; // We expect exact results.

			for (uint32_t layerIdx = 0u; layerIdx < m_params.layerCount; ++layerIdx)
			{
				const bool							layerWritten	= depthWritten && ((m_params.layerMask & (1 << layerIdx)) != 0u);
				const auto							layerDataPtr	= dataPtr + depthLayerSize * layerIdx;
				const tcu::ConstPixelBufferAccess	layerAccess		(depthCopyFormat, fbDim, layerDataPtr);
				const float							expectedDepth	= (layerWritten ? 1.0f : 0.0f); // Needs to match the vertex shader and depth/stencil config.
				const std::string					logImgName		= "DepthAttachment-Layer" + std::to_string(layerIdx);
				tcu::TextureLevel					refLevel		(depthCopyFormat, fbDim.x(), fbDim.y(), fbDim.z());
				tcu::PixelBufferAccess				refAccess		= refLevel.getAccess();

				tcu::clearDepth(refAccess, expectedDepth);
				if (!tcu::dsThresholdCompare(log, logImgName.c_str(), "", refAccess, layerAccess, depthThreshold, tcu::COMPARE_LOG_ON_ERROR))
					failure = true;
			}
		}

		// Stencil.
		{
			const auto dataPtr			= reinterpret_cast<const char*>(stencilVerificationBuffer->getAllocation().getHostPtr());
			const auto stencilPixelSize	= tcu::getPixelSize(stencilCopyFormat);
			const auto stencilLayerSize	= static_cast<size_t>(fbDim.x() * fbDim.y() * fbDim.z() * stencilPixelSize);
			const auto stencilThreshold	= 0.0f; // We expect exact results.

			for (uint32_t layerIdx = 0u; layerIdx < m_params.layerCount; ++layerIdx)
			{
				const bool							layerWritten	= stencilWritten && ((m_params.layerMask & (1 << layerIdx)) != 0u);
				const auto							layerDataPtr	= dataPtr + stencilLayerSize * layerIdx;
				const tcu::ConstPixelBufferAccess	layerAccess		(stencilCopyFormat, fbDim, layerDataPtr);
				const int							expectedStencil	= (layerWritten ? 0xFF : 0); // Needs to match the stencil op config.
				const std::string					logImgName		= "StencilAttachment-Layer" + std::to_string(layerIdx);
				tcu::TextureLevel					refLevel		(stencilCopyFormat, fbDim.x(), fbDim.y(), fbDim.z());
				tcu::PixelBufferAccess				refAccess		= refLevel.getAccess();

				tcu::clearStencil(refAccess, expectedStencil);
				if (!tcu::dsThresholdCompare(log, logImgName.c_str(), "", refAccess, layerAccess, stencilThreshold, tcu::COMPARE_LOG_ON_ERROR))
					failure = true;
			}
		}
	}

	if (failure)
		return tcu::TestStatus::fail("Invalid value found in verification buffers; check log for details");

	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous namespace

tcu::TestCaseGroup* createDynamicRenderingUnusedAttachmentsTests (tcu::TestContext& testCtx, bool useSecondaries)
{
	GroupPtr group (new tcu::TestCaseGroup(testCtx, "unused_attachments", "Tests for VK_EXT_dynamic_rendering_unused_attachments"));

	// Add a combination subgroup just in case we want to add more test cases later to another subgroup.
	GroupPtr combGroup	(new tcu::TestCaseGroup(testCtx, "comb", "VK_EXT_dynamic_rendering_unused_attachments with different combinations"));
	GroupPtr colorGroup	(new tcu::TestCaseGroup(testCtx, "color", ""));
	GroupPtr dsGroup	(new tcu::TestCaseGroup(testCtx, "depth_stencil", ""));
	GroupPtr badFmtGrp	(new tcu::TestCaseGroup(testCtx, "bad_formats", "Test using wrong formats when the handle is VK_NULL_HANDLE"));

	const uint32_t attachmentCounts[]	= { 1u, 4u, 8u, };
	const uint32_t layerCounts[]		= { 1u, 4u, };
	const uint32_t masksToTest[]		= { 0xFFFFFFFFu, 0x0u, 0x55555555u, 0xAAAAAAAAu, };

	{
		// Combinations of color attachment counts, no depth/stencil.
		for (const auto& pipeAtt : attachmentCounts)
			for (const auto& fragAtt : attachmentCounts)
			{
				if (fragAtt < pipeAtt)
					continue;

				for (const auto& layerCount : layerCounts)
					for (const auto& layerMask : masksToTest)
					{
						// Avoid duplicate cases.
						if (layerCount == 1u && layerMask != masksToTest[0] && layerMask != masksToTest[1])
							continue;

						for (const auto& formatMask : masksToTest)
							for (const auto& handleMask : masksToTest)
							{
								for (const auto multiview : { false, true })
								{
									const auto viewMask = (((1u << layerCount) - 1u) & layerMask);

									if (multiview && viewMask == 0u)
										continue;

									const TestParams params
									(
										pipeAtt, fragAtt, layerCount,
										viewMask,
										multiview,
										formatMask, handleMask,
										false, false, false,
										false, false, false,
										useSecondaries, false
									);
									colorGroup->addChild(new DynamicUnusedAttachmentsCase(testCtx, params.getTestName(), "", params));
								}
							}
					}
			}

		// Combinations of depth/stencil parameters, single color attachment.
		for (const auto depthPresent : { false, true })
			for (const auto depthDefined : { false, true })
				for (const auto depthValidHandle : { false, true })
				{
					if (!depthPresent && (depthDefined || depthValidHandle))
						continue;

					for (const auto stencilPresent : { false, true })
						for (const auto stencilDefined : { false, true })
							for (const auto stencilValidHandle : { false, true })
							{
								if (!stencilPresent && (stencilDefined || stencilValidHandle))
									continue;

								// Either both or none according to VUID-VkRenderingInfo-pDepthAttachment-06085
								if (depthValidHandle != stencilValidHandle)
									continue;

								// So far there is no VU that prevents only one of the depth/stencil formats from being
								// VK_FORMAT_UNDEFINED while the other one is not. However, that would mean disabling the
								// depth/stencil test (or at least make that aspect read-only, it's not clear) through a second
								// mechanism in the pipeline configuration.
								//
								// We can still test the VK_NULL_HANDLE/VK_FORMAT_UNDEFINED inconsistency, just not separately for
								// depth and stencil, which is one of the focus of these tests.
								if (depthDefined != stencilDefined)
									continue;

								for (const auto& layerCount : layerCounts)
									for (const auto& layerMask : masksToTest)
									{
										// Avoid duplicate cases.
										if (layerCount == 1u && layerMask != masksToTest[0] && layerMask != masksToTest[1])
											continue;

										for (const auto multiview : { false, true })
										{
											const auto viewMask = (((1u << layerCount) - 1u) & layerMask);

											if (multiview && viewMask == 0u)
												continue;

											const TestParams params
											(
												1u, 1u, layerCount,
												viewMask,
												multiview,
												0xFFFFFFFFu, 0xFFFFFFFFu,
												depthPresent, depthDefined, depthValidHandle,
												stencilPresent, stencilDefined, stencilValidHandle,
												useSecondaries, false
											);
											dsGroup->addChild(new DynamicUnusedAttachmentsCase(testCtx, params.getTestName(), "", params));
										}
									}
							}
				}

		combGroup->addChild(colorGroup.release());
		combGroup->addChild(dsGroup.release());
	}
	group->addChild(combGroup.release());

	// Bad format tests.
	{
		for (const auto& formatMask : masksToTest)
			for (const auto& handleMask : masksToTest)
			{
				if (handleMask == 0xFFFFFFFFu || formatMask == handleMask)
					continue;

				const TestParams params
				(
					4u, 4u, 1u,
					1u,
					false,
					formatMask, handleMask,
					true, true, false,
					true, true, false,
					useSecondaries, true
				);
				badFmtGrp->addChild(new DynamicUnusedAttachmentsCase(testCtx, params.getTestName(), "", params));
			}
	}
	group->addChild(badFmtGrp.release());

	return group.release();
}

} // renderpass
} // vkt
