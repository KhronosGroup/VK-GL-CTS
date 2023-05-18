/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Tests using depth/stencil images as descriptors.
 *//*--------------------------------------------------------------------*/

#include "vktImageDepthStencilDescriptorTests.hpp"

#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuMaybe.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <iterator>
#include <vector>
#include <sstream>
#include <string>

namespace vkt
{
namespace image
{

namespace
{

using namespace vk;

VkExtent3D getExtent ()
{
	return makeExtent3D(8u, 8u, 1u);
}

VkFormat getColorBufferFormat ()
{
	return VK_FORMAT_R8G8B8A8_UNORM;
}

VkFormat getFloatStorageFormat ()
{
	return VK_FORMAT_R32_SFLOAT;
}

VkFormat getUintStorageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

tcu::Maybe<std::string> layoutExtension (VkImageLayout layout)
{
	std::string extension;

	switch (layout)
	{
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
		extension = "VK_KHR_maintenance2";
		break;
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
		// Note: we will not be using separate depth/stencil layouts. There's a separate group of tests for that.
		extension = "VK_KHR_separate_depth_stencil_layouts";
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	if (!extension.empty())
		return tcu::just(extension);
	return tcu::Nothing;
}

// Types of access for an image aspect.
enum class AspectAccess
{
	NONE	= 0,
	RO		= 1,	// Different subtypes, see below.
	RW		= 2,	// This always means a normal read/write depth/stencil attachment (NOT a storage image).
};

std::ostream& operator<< (std::ostream& stream, AspectAccess access)
{
	if		(access == AspectAccess::NONE)	stream << "none";
	else if	(access == AspectAccess::RO)	stream << "ro";
	else if	(access == AspectAccess::RW)	stream << "rw";
	else									DE_ASSERT(false);

	return stream;
}

// Types of read-only accesses.
enum class ReadOnlyAccess
{
	DS_ATTACHMENT		= 0,	// Depth/stencil attachment but read-only (writes not enabled).
	INPUT_ATTACHMENT	= 1,	// Input attachment in the set.
	SAMPLED				= 2,	// Sampled image.
};

std::ostream& operator<< (std::ostream& stream, ReadOnlyAccess access)
{
	if		(access == ReadOnlyAccess::DS_ATTACHMENT)		stream << "att";
	else if	(access == ReadOnlyAccess::INPUT_ATTACHMENT)	stream << "ia";
	else if	(access == ReadOnlyAccess::SAMPLED)				stream << "sampled";
	else													DE_ASSERT(false);

	return stream;
}

// A given layout gives different accesses to each aspect.
AspectAccess getLegalAccess (VkImageLayout layout, VkImageAspectFlagBits aspect)
{
	DE_ASSERT(aspect == VK_IMAGE_ASPECT_DEPTH_BIT || aspect == VK_IMAGE_ASPECT_STENCIL_BIT);

	if (layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
		return ((aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? AspectAccess::RW : AspectAccess::RO);
	else if (layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
		return ((aspect == VK_IMAGE_ASPECT_DEPTH_BIT) ? AspectAccess::RW : AspectAccess::RO);
	else if (layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)
		return ((aspect == VK_IMAGE_ASPECT_DEPTH_BIT) ? AspectAccess::RO : AspectAccess::NONE);
	else if (layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
		return ((aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? AspectAccess::RO : AspectAccess::NONE);

	DE_ASSERT(false);
	return AspectAccess::NONE; // Unreachable.
}

using ROAccessVec = std::vector<ReadOnlyAccess>;

std::ostream& operator<< (std::ostream& stream, const ROAccessVec& vec)
{
	for (size_t i = 0u; i < vec.size(); ++i)
		stream << ((i > 0u) ? "_" : "") << vec[i];
	return stream;
}

// We cannot access depth/stencil images both as a depth/stencil attachment and an input attachment at the same time if they have
// both aspects, because input attachments can only have one aspect.
bool incompatibleInputAttachmentAccess (AspectAccess depthAccess, const ROAccessVec* depthROAccesses, AspectAccess stencilAccess, const ROAccessVec* stencilROAccesses)
{
	const bool depthAsDSAttachment		= (depthAccess == AspectAccess::RW || (depthAccess == AspectAccess::RO && de::contains(begin(*depthROAccesses), end(*depthROAccesses), ReadOnlyAccess::DS_ATTACHMENT)));
	const bool stencilAsDSAttachment	= (stencilAccess == AspectAccess::RW || (stencilAccess == AspectAccess::RO && de::contains(begin(*stencilROAccesses), end(*stencilROAccesses), ReadOnlyAccess::DS_ATTACHMENT)));
	const bool depthAsInputAttachment	= (depthAccess == AspectAccess::RO && de::contains(begin(*depthROAccesses), end(*depthROAccesses), ReadOnlyAccess::INPUT_ATTACHMENT));
	const bool stencilAsInputAttachment	= (stencilAccess == AspectAccess::RO && de::contains(begin(*stencilROAccesses), end(*stencilROAccesses), ReadOnlyAccess::INPUT_ATTACHMENT));

	return ((depthAsDSAttachment && stencilAsInputAttachment) || (stencilAsDSAttachment && depthAsInputAttachment));
}

VkImageUsageFlags getReadOnlyUsageFlags (const ROAccessVec& readOnlyAccesses)
{
	VkImageUsageFlags usageFlags = 0u;

	for (const auto& access : readOnlyAccesses)
	{
		if (access == ReadOnlyAccess::DS_ATTACHMENT)
			usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else if (access == ReadOnlyAccess::INPUT_ATTACHMENT)
			usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else if (access == ReadOnlyAccess::SAMPLED)
			usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		else
			DE_ASSERT(false);
	}

	return usageFlags;
}

// Resources needed for an aspect that will be used as a descriptor in shaders (sampled or input attachment).
struct InputOutputDescriptor
{
	uint32_t				binding;
	tcu::Maybe<uint32_t>	inputAttachmentIndex;
	VkImageAspectFlagBits	aspect;
};

using IODescVec = std::vector<InputOutputDescriptor>;

// Test parameters.
struct TestParams
{
	VkFormat				format;				// Image format.
	VkImageLayout			layout;				// Layout being tested.
	AspectAccess			depthAccess;		// Type of access that will be used for depth (must be legal).
	AspectAccess			stencilAccess;		// Type of access that will be used for stencil (must be legal).

	tcu::Maybe<ROAccessVec>	depthROAccesses;	// Types of read-only accesses for depth (used when depthAccess is RO).
	tcu::Maybe<ROAccessVec>	stencilROAccesses;	// Types of read-only accesses for stencil (used when stencilAccess is RO).

	VkImageUsageFlags		getUsageFlags () const;

	// Get a list of descriptors needed according to the given test parameters.
	IODescVec				getDescriptors () const;

	// Does this case need a depth/stencil attachment?
	bool					dsAttachmentNeeded () const;

	// Does this case use the depth aspect as an input attachment?
	bool					depthAsInputAttachment () const;

	// Does this case use the stencil aspect as an input attachment?
	bool					stencilAsInputAttachment () const;

	// Does this case need an input attachment?
	bool					inputAttachmentNeeded () const;

	// Does this case need a depth/stencil attachment as a depth buffer?
	bool					depthBufferNeeded () const;

	// Does this case need the pipeline depth test enabled?
	bool					needsDepthTest () const;

	// Does this case need the stencil test enabled?
	bool					needsStencilTest () const;
};

VkImageUsageFlags TestParams::getUsageFlags () const
{
	VkImageUsageFlags usageFlags = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	if (depthAccess == AspectAccess::RW || stencilAccess == AspectAccess::RW)
		usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (depthAccess == AspectAccess::RO)
	{
		DE_ASSERT(static_cast<bool>(depthROAccesses));
		usageFlags |= getReadOnlyUsageFlags(*depthROAccesses);
	}

	if (stencilAccess == AspectAccess::RO)
	{
		DE_ASSERT(static_cast<bool>(stencilROAccesses));
		usageFlags |= getReadOnlyUsageFlags(*stencilROAccesses);
	}

	return usageFlags;
}

void addDescriptors (IODescVec& descriptors, uint32_t& inputAttachmentCount, const ROAccessVec& accesses, VkImageAspectFlagBits aspect)
{
	for (const auto& access : accesses)
	{
		// Get a new binding number and a new input attachment index if needed, then append the new descriptor to the list if
		// appropriate.

		InputOutputDescriptor descriptor =
		{
			static_cast<uint32_t>(descriptors.size()),	//	uint32_t				binding;
			tcu::Nothing,								//	tcu::Maybe<uint32_t>	inputAttachmentIndex;
			aspect,										//	VkImageAspectFlagBits	aspect;
		};

		if (access == ReadOnlyAccess::INPUT_ATTACHMENT)
			descriptor.inputAttachmentIndex = tcu::just(inputAttachmentCount++);

		if (access == ReadOnlyAccess::INPUT_ATTACHMENT || access == ReadOnlyAccess::SAMPLED)
			descriptors.push_back(descriptor);
	}
}

IODescVec TestParams::getDescriptors () const
{
	IODescVec descriptors;
	uint32_t inputAttachmentCount = 0u;

	if (static_cast<bool>(depthROAccesses))
		addDescriptors(descriptors, inputAttachmentCount, *depthROAccesses, VK_IMAGE_ASPECT_DEPTH_BIT);

	if (static_cast<bool>(stencilROAccesses))
		addDescriptors(descriptors, inputAttachmentCount, *stencilROAccesses, VK_IMAGE_ASPECT_STENCIL_BIT);

	return descriptors;
}

bool TestParams::dsAttachmentNeeded () const
{
	// The depth/stencil attachment is needed if the image is going to be used as a depth/stencil attachment or an input attachment.
	return (inputAttachmentNeeded() || depthBufferNeeded());
}

bool TestParams::depthAsInputAttachment () const
{
	return (depthAccess == AspectAccess::RO && de::contains(begin(*depthROAccesses), end(*depthROAccesses), ReadOnlyAccess::INPUT_ATTACHMENT));
}

bool TestParams::stencilAsInputAttachment () const
{
	return (stencilAccess == AspectAccess::RO && de::contains(begin(*stencilROAccesses), end(*stencilROAccesses), ReadOnlyAccess::INPUT_ATTACHMENT));
}

bool TestParams::inputAttachmentNeeded () const
{
	// An input attachment is needed if any of the depth or stencil aspects include a read-only access as an input attachment.
	return (depthAsInputAttachment() || stencilAsInputAttachment());
}

bool TestParams::depthBufferNeeded () const
{
	// The depth buffer is needed if any of the depth or stencil aspects include a read-write or read-only DS access.
	return (needsDepthTest() || needsStencilTest());
}

bool TestParams::needsDepthTest () const
{
	// The depth test is needed if the depth aspect includes a read-write or read-only DS access.
	return (depthAccess == AspectAccess::RW || (depthAccess == AspectAccess::RO && de::contains(begin(*depthROAccesses), end(*depthROAccesses), ReadOnlyAccess::DS_ATTACHMENT)));
}

bool TestParams::needsStencilTest () const
{
	// The stencil test is needed if the stencil aspect includes a read-write or read-only DS access.
	return (stencilAccess == AspectAccess::RW || (stencilAccess == AspectAccess::RO && de::contains(begin(*stencilROAccesses), end(*stencilROAccesses), ReadOnlyAccess::DS_ATTACHMENT)));
}

class DepthStencilDescriptorCase : public vkt::TestCase
{
public:
					DepthStencilDescriptorCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual			~DepthStencilDescriptorCase		(void) {}

	void			checkSupport	(Context& context) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			initPrograms	(vk::SourceCollections& programCollection) const override;

protected:
	TestParams		m_params;
};

class DepthStencilDescriptorInstance : public vkt::TestInstance
{
public:
					DepthStencilDescriptorInstance	(Context& context, const TestParams& params);
	virtual			~DepthStencilDescriptorInstance	(void) {}

	tcu::TestStatus	iterate() override;

protected:
	// Must match the shaders.
	struct PushConstantData
	{
		float colorR;
		float colorG;
		float colorB;
		float colorA;
		float depth;

		PushConstantData ()
			: colorR	(0.0f)
			, colorG	(0.0f)
			, colorB	(0.0f)
			, colorA	(0.0f)
			, depth		(0.0f)
		{}

		PushConstantData (const tcu::Vec4& color, float depth_)
			: colorR	(color.x())
			, colorG	(color.y())
			, colorB	(color.z())
			, colorA	(color.w())
			, depth		(depth_)
		{}
	};

	TestParams		m_params;
};

DepthStencilDescriptorCase::DepthStencilDescriptorCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void DepthStencilDescriptorCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	const auto requiredExtension = layoutExtension(m_params.layout);
	if (requiredExtension)
		context.requireDeviceFunctionality(*requiredExtension);

	// Check format support.
	const auto&		vki		= context.getInstanceInterface();
	const auto		physDev	= context.getPhysicalDevice();
	const auto		imgType	= VK_IMAGE_TYPE_2D;
	const auto		tiling	= VK_IMAGE_TILING_OPTIMAL;
	const auto		usage	= m_params.getUsageFlags();

	VkImageFormatProperties formatProperties;
	const auto res = vki.getPhysicalDeviceImageFormatProperties(physDev, m_params.format, imgType, tiling, usage, 0u, &formatProperties);
	if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Format does not support required properties");
	else if (res != VK_SUCCESS)
		TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned " + de::toString(res));
}

TestInstance* DepthStencilDescriptorCase::createInstance (Context& context) const
{
	return new DepthStencilDescriptorInstance(context, m_params);
}

void DepthStencilDescriptorCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 450\n"
		<< "\n"
		<< "layout(push_constant, std430) uniform PushConstantBlock {\n"
		<< "    float colorR;\n"
		<< "    float colorG;\n"
		<< "    float colorB;\n"
		<< "    float colorA;\n"
		<< "    float depth;\n"
		<< "} pc;\n"
		<< "\n"
		<< "vec2 vertexPositions[3] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),\n"
		<< "    vec2(-1.0,  3.0),\n"
		<< "    vec2( 3.0, -1.0));\n"
		<< "\n"
		<< "void main () {\n"
		<< "    gl_Position = vec4(vertexPositions[gl_VertexIndex], pc.depth, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	// When any of the image aspects is going to be used as an input attachment or sampled image, we need an input descriptor and an
	// output descriptor to verify reading from it.
	std::ostringstream	descriptorsDecl;
	std::ostringstream	descriptorsSideEffects;
	const auto			descriptors				= m_params.getDescriptors();

	// Samplers set (set number 2).
	descriptorsDecl
		<< "layout (set=2, binding=0) uniform sampler globalSampler;\n"		// Sampler with float border color (depth).
		<< "layout (set=2, binding=1) uniform sampler uglobalSampler;\n"	// Sampler with int border color (stencil).
		;

	for (const auto& descriptor : descriptors)
	{
		const auto			prefix = ((descriptor.aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? "u" : "");
		const auto			suffix = ((descriptor.aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? "ui" : "f");
		std::ostringstream	loadOp;

		// Input descriptor declaration.
		if (static_cast<bool>(descriptor.inputAttachmentIndex))
		{
			const auto& iaIndex = *descriptor.inputAttachmentIndex;
			descriptorsDecl << "layout (input_attachment_index=" << iaIndex << ", set=0, binding=" << descriptor.binding << ") uniform " << prefix << "subpassInput attachment" << descriptor.binding << ";\n";
			loadOp << "subpassLoad(attachment" << descriptor.binding << ")";
		}
		else
		{
			descriptorsDecl << "layout (set=0, binding=" << descriptor.binding << ") uniform " << prefix << "texture2D sampledImage" << descriptor.binding << ";\n";
			loadOp << "texture(" << prefix << "sampler2D(sampledImage" << descriptor.binding << ", " << prefix << "globalSampler), gl_FragCoord.xy)"; // This needs a sampler with unnormalizedCoordinates == VK_TRUE.
		}

		// Output descriptor declaration (output descriptors in set 1).
		descriptorsDecl << "layout (r32" << suffix << ", set=1, binding=" << descriptor.binding << ") uniform " << prefix << "image2D storage" << descriptor.binding << ";\n";

		// The corresponding side effect.
		descriptorsSideEffects << "    imageStore(storage" << descriptor.binding << ", ivec2(gl_FragCoord.xy), " << loadOp.str() << ");\n";
	}

	std::ostringstream frag;
	frag
		<< "#version 450\n"
		<< "\n"
		<< "layout(location=0) out vec4 outColor;\n"
		<< "layout(push_constant, std430) uniform PushConstantBlock {\n"
		<< "    float colorR;\n"
		<< "    float colorG;\n"
		<< "    float colorB;\n"
		<< "    float colorA;\n"
		<< "    float depth;\n"
		<< "} pc;\n"
		<< "\n"
		<< descriptorsDecl.str()
		<< "\n"
		<< "void main () {\n"
		<< descriptorsSideEffects.str()
		<< "    outColor = vec4(pc.colorR, pc.colorG, pc.colorB, pc.colorA);\n"
		<< "}\n"
		;

	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

DepthStencilDescriptorInstance::DepthStencilDescriptorInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

VkFormat getAspectStorageFormat (VkImageAspectFlagBits aspect)
{
	return ((aspect == VK_IMAGE_ASPECT_DEPTH_BIT) ? getFloatStorageFormat() : getUintStorageFormat());
}

VkDeviceSize getCopyBufferSize (const tcu::TextureFormat& format, const VkExtent3D& extent)
{
	return static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(format)) * extent.width * extent.height * extent.depth);
}

tcu::TestStatus	DepthStencilDescriptorInstance::iterate ()
{
	const auto&		vkd				= m_context.getDeviceInterface();
	const auto		device			= m_context.getDevice();
	auto&			alloc			= m_context.getDefaultAllocator();
	const auto		qIndex			= m_context.getUniversalQueueFamilyIndex();
	const auto		queue			= m_context.getUniversalQueue();
	const auto		extent			= getExtent();
	const auto		usage			= m_params.getUsageFlags();
	const auto		colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto		colorFormat		= getColorBufferFormat();
	const auto		tcuColorFormat	= mapVkFormat(colorFormat);
	const auto		storageUsage	= (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	const auto		stageFlags		= VK_SHADER_STAGE_FRAGMENT_BIT;
	const auto		tcuFormat		= mapVkFormat(m_params.format);
	const auto		hasDepth		= tcu::hasDepthComponent(tcuFormat.order);
	const auto		hasStencil		= tcu::hasStencilComponent(tcuFormat.order);
	const auto		colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto		outputImgLayout	= VK_IMAGE_LAYOUT_GENERAL;
	const auto		colorLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Copy formats.
	const auto		tcuDepthFormat		= (hasDepth ?	getDepthCopyFormat(m_params.format)		: tcu::TextureFormat());
	const auto		tcuStencilFormat	= (hasStencil ?	getStencilCopyFormat(m_params.format)	: tcu::TextureFormat());

	// These must match the depth test configuration when enabled.
	const float		depthClearValue	= 0.5f;
	const float		depthFailValue	= 1.0f;
	const float		depthPassValue	= 0.0f;

	// These must match the stencil test configuration when enabled.
	const uint32_t	stencilClearVal	= 100u;
	const uint32_t	stencilFailVal	= 200u;
	const uint32_t	stencilPassVal	= 10u;

	// For the color buffer.
	const tcu::Vec4	colorClearVal	(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	colorFailVal	(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4	colorPassVal	(0.0f, 1.0f, 0.0f, 1.0f);

	// Will the test update the depth/stencil buffer?
	const auto stencilWrites	= (m_params.stencilAccess == AspectAccess::RW);
	const auto depthWrites		= (m_params.depthAccess == AspectAccess::RW);

	// Create color attachment.
	const VkImageCreateInfo colorBufferInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		colorFormat,							//	VkFormat				format;
		extent,									//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorUsage,								//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorBuffer	(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
	const auto		colorView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Create depth/stencil image.
	const VkImageCreateInfo dsImageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		m_params.format,						//	VkFormat				format;
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
	ImageWithMemory dsImage(vkd, device, alloc, dsImageInfo, MemoryRequirement::Any);
	const auto depthStencilSRR	= makeImageSubresourceRange(getImageAspectFlags(tcuFormat), 0u, 1u, 0u, 1u);
	const auto depthSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	const auto stencilSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
	const auto dsImageView		= makeImageView(vkd, device, dsImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_params.format, depthStencilSRR);

	Move<VkImageView> depthOnlyView;
	Move<VkImageView> stencilOnlyView;

	if (hasDepth)
		depthOnlyView = makeImageView(vkd, device, dsImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_params.format, depthSRR);

	if (hasStencil)
		stencilOnlyView = makeImageView(vkd, device, dsImage.get(), VK_IMAGE_VIEW_TYPE_2D, m_params.format, stencilSRR);

	// Get expected descriptors and create output images for them.
	const auto descriptors = m_params.getDescriptors();

	std::vector<de::MovePtr<ImageWithMemory>>	outputImages;
	std::vector<Move<VkImageView>>				outputImageViews;

	outputImages.reserve(descriptors.size());
	outputImageViews.reserve(descriptors.size());

	for (const auto& desc : descriptors)
	{
		// Floating point images to copy the depth aspect and unsigned int images to copy the stencil aspect.
		const auto imageFormat = getAspectStorageFormat(desc.aspect);

		const VkImageCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			imageFormat,							//	VkFormat				format;
			extent,									//	VkExtent3D				extent;
			1u,										//	uint32_t				mipLevels;
			1u,										//	uint32_t				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			storageUsage,							//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	uint32_t				queueFamilyIndexCount;
			nullptr,								//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};

		outputImages.push_back(de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, alloc, createInfo, MemoryRequirement::Any)));
		outputImageViews.push_back(makeImageView(vkd, device, outputImages.back()->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR));
	}

	// Create samplers.
	Move<VkSampler> samplerFloat;
	Move<VkSampler> samplerInt;
	{
		VkSamplerCreateInfo samplerCreateInfo =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,						//	VkFilter				magFilter;
			VK_FILTER_NEAREST,						//	VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,			//	VkSamplerMipmapMode		mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeW;
			0.0f,									//	float					mipLodBias;
			VK_FALSE,								//	VkBool32				anisotropyEnable;
			0.0f,									//	float					maxAnisotropy;
			VK_FALSE,								//	VkBool32				compareEnable;
			VK_COMPARE_OP_LAST,						//	VkCompareOp				compareOp;
			0.0f,									//	float					minLod;
			0.0f,									//	float					maxLod;
			VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,		//	VkBorderColor			borderColor;
			VK_TRUE,								//	VkBool32				unnormalizedCoordinates;
		};
		// Note the samplers are created with unnormalizedCoordinates as per how they are used in shader code.
		samplerFloat = createSampler(vkd, device, &samplerCreateInfo);

		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInt = createSampler(vkd, device, &samplerCreateInfo);
	}

	// Create input and output descriptor set layouts.
	Move<VkDescriptorSetLayout> inputSetLayout;
	Move<VkDescriptorSetLayout> outputSetLayout;
	Move<VkDescriptorSetLayout> samplerSetLayout;

	{
		DescriptorSetLayoutBuilder inputLayoutBuilder;
		for (const auto& desc : descriptors)
		{
			if (static_cast<bool>(desc.inputAttachmentIndex))
				inputLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, stageFlags);
			else
				inputLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stageFlags);
		}
		inputSetLayout = inputLayoutBuilder.build(vkd, device);
	}
	{
		DescriptorSetLayoutBuilder outputLayoutBuilder;
		for (size_t i = 0; i < descriptors.size(); ++i)
			outputLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stageFlags);
		outputSetLayout = outputLayoutBuilder.build(vkd, device);
	}
	{
		DescriptorSetLayoutBuilder samplerLayoutBuilder;
		samplerLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags);
		samplerLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags);
		samplerSetLayout = samplerLayoutBuilder.build(vkd, device);
	}

	const std::vector<VkDescriptorSetLayout> setLayouts = { inputSetLayout.get(), outputSetLayout.get(), samplerSetLayout.get() };

	// Descriptor pool and descriptor sets.
	Move<VkDescriptorPool>	descriptorPool;
	Move<VkDescriptorSet>	inputSet;
	Move<VkDescriptorSet>	outputSet;
	Move<VkDescriptorSet>	samplerSet;
	{
		DescriptorPoolBuilder poolBuilder;

		// Input descriptors.
		for (const auto& desc : descriptors)
		{
			if (static_cast<bool>(desc.inputAttachmentIndex))
				poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
			else
				poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		}

		// Output descriptors.
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, static_cast<uint32_t>(descriptors.size()));

		// Global samplers.
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER, 2u);

		descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, static_cast<uint32_t>(setLayouts.size()));
	}

	inputSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), inputSetLayout.get());
	outputSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), outputSetLayout.get());
	samplerSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), samplerSetLayout.get());

	const std::vector<VkDescriptorSet> descriptorSets = { inputSet.get(), outputSet.get(), samplerSet.get() };

	// Update descriptor sets.
	{
		DescriptorSetUpdateBuilder inputUpdateBuilder;
		DescriptorSetUpdateBuilder outputUpdateBuilder;
		DescriptorSetUpdateBuilder samplerUpdateBuilder;

		std::vector<VkDescriptorImageInfo> inputImgInfos;
		std::vector<VkDescriptorImageInfo> outputImgInfos;
		std::vector<VkDescriptorImageInfo> samplerImgInfos;

		// Make sure addresses are stable (pushing elements back while taking their addresses).
		inputImgInfos.reserve(descriptors.size());
		outputImgInfos.reserve(descriptors.size());
		samplerImgInfos.reserve(2u);

		for (size_t descriptorIdx = 0u; descriptorIdx < descriptors.size(); ++descriptorIdx)
		{
			const auto& desc		= descriptors[descriptorIdx];
			const auto	isIA		= static_cast<bool>(desc.inputAttachmentIndex);
			const auto	location	= DescriptorSetUpdateBuilder::Location::binding(desc.binding);

			// Input descriptors.
			const auto inType	= (isIA ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
			const auto view		= ((desc.aspect == VK_IMAGE_ASPECT_DEPTH_BIT) ? depthOnlyView.get() : stencilOnlyView.get());
			inputImgInfos.push_back(makeDescriptorImageInfo(DE_NULL, view, m_params.layout));
			inputUpdateBuilder.writeSingle(inputSet.get(), location, inType, &inputImgInfos.back());

			// Output descriptors.
			outputImgInfos.push_back(makeDescriptorImageInfo(DE_NULL, outputImageViews[descriptorIdx].get(), outputImgLayout));
			outputUpdateBuilder.writeSingle(outputSet.get(), location, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputImgInfos.back());
		}

		inputUpdateBuilder.update(vkd, device);
		outputUpdateBuilder.update(vkd, device);

		// Samplers.
		samplerImgInfos.push_back(makeDescriptorImageInfo(samplerFloat.get(), DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED));
		samplerUpdateBuilder.writeSingle(samplerSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_SAMPLER, &samplerImgInfos.back());

		samplerImgInfos.push_back(makeDescriptorImageInfo(samplerInt.get(), DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED));
		samplerUpdateBuilder.writeSingle(samplerSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_SAMPLER, &samplerImgInfos.back());

		samplerUpdateBuilder.update(vkd, device);
	}

	PushConstantData	pcData;
	const auto			pcStages	= (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto			pcSize		= static_cast<uint32_t>(sizeof(pcData));
	const auto			pcRange		= makePushConstantRange(pcStages, 0u, pcSize);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, static_cast<uint32_t>(setLayouts.size()), de::dataOrNull(setLayouts), 1u, &pcRange);

	// Render pass.
	Move<VkRenderPass> renderPass;

	{
		std::vector<VkAttachmentDescription2>	attachmentDescriptions;
		std::vector<VkAttachmentReference2>		attachmentReferences;

		const VkAttachmentDescription2 colorAttachmentDesc =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,		//	VkStructureType					sType;
			nullptr,										//	const void*						pNext;
			0u,												//	VkAttachmentDescriptionFlags	flags;
			colorFormat,									//	VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,							//	VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,						//	VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,					//	VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//	VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,				//	VkAttachmentStoreOp				stencilStoreOp;
			colorLayout,									//	VkImageLayout					initialLayout;
			colorLayout,									//	VkImageLayout					finalLayout;
		};
		attachmentDescriptions.push_back(colorAttachmentDesc);

		const VkAttachmentReference2 colorAttachmentRef =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,	//	VkStructureType		sType;
			nullptr,									//	const void*			pNext;
			0u,											//	uint32_t			attachment;
			colorLayout,								//	VkImageLayout		layout;
			VK_IMAGE_ASPECT_COLOR_BIT,					//	VkImageAspectFlags	aspectMask;
		};
		attachmentReferences.push_back(colorAttachmentRef);

		const auto needsIA			= m_params.inputAttachmentNeeded();
		const auto needsDepthBuffer	= m_params.depthBufferNeeded();
		DE_ASSERT(!(needsIA && needsDepthBuffer));

		if (m_params.dsAttachmentNeeded())
		{
			const VkAttachmentDescription2 dsAttachmentDesc =
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				nullptr,
				0u,
				m_params.format,
				VK_SAMPLE_COUNT_1_BIT,
				(hasDepth						? VK_ATTACHMENT_LOAD_OP_LOAD	: VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				((hasDepth && depthWrites)		? VK_ATTACHMENT_STORE_OP_STORE	: VK_ATTACHMENT_STORE_OP_DONT_CARE),
				(hasStencil						? VK_ATTACHMENT_LOAD_OP_LOAD	: VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				((hasStencil && stencilWrites)	? VK_ATTACHMENT_STORE_OP_STORE	: VK_ATTACHMENT_STORE_OP_DONT_CARE),
				m_params.layout,
				m_params.layout,
			};
			attachmentDescriptions.push_back(dsAttachmentDesc);

			const VkAttachmentReference2 dsAttachmentRef =
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,	//	VkStructureType		sType;
				nullptr,									//	const void*			pNext;
				1u,											//	uint32_t			attachment;
				m_params.layout,							//	VkImageLayout		layout;
															//	VkImageAspectFlags	aspectMask;
				(	(m_params.depthAsInputAttachment()		? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)	: 0u)
				 |	(m_params.stencilAsInputAttachment()	? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_STENCIL_BIT)	: 0u)),
			};

			attachmentReferences.push_back(dsAttachmentRef);
		}

		const VkSubpassDescription2 subpassDesc =
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,					//	VkStructureType					sType;
			nullptr,													//	const void*						pNext;
			0u,															//	VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,							//	VkPipelineBindPoint				pipelineBindPoint;
			0u,															//	uint32_t						viewMask;
			(needsIA ? 1u : 0u),										//	uint32_t						inputAttachmentCount;
			(needsIA ? &attachmentReferences.at(1) : nullptr),			//	const VkAttachmentReference*	pInputAttachments;
			1u,															//	uint32_t						colorAttachmentCount;
			&attachmentReferences.at(0),								//	const VkAttachmentReference*	pColorAttachments;
			nullptr,													//	const VkAttachmentReference*	pResolveAttachments;
			(needsDepthBuffer ? &attachmentReferences.at(1) : nullptr),	//	const VkAttachmentReference*	pDepthStencilAttachment;
			0u,															//	uint32_t						preserveAttachmentCount;
			nullptr,													//	const uint32_t*					pPreserveAttachments;
		};

		const VkRenderPassCreateInfo2 renderPassCreateInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,			//	VkStructureType					sType;
			nullptr,												//	const void*						pNext;
			0u,														//	VkRenderPassCreateFlags			flags;
			static_cast<uint32_t>(attachmentDescriptions.size()),	//	uint32_t						attachmentCount;
			de::dataOrNull(attachmentDescriptions),					//	const VkAttachmentDescription*	pAttachments;
			1u,														//	uint32_t						subpassCount;
			&subpassDesc,											//	const VkSubpassDescription*		pSubpasses;
			0u,														//	uint32_t						dependencyCount;
			nullptr,												//	const VkSubpassDependency*		pDependencies;
			0u,														//	uint32_t						correlatedViewMaskCount;
			nullptr,												//	const uint32_t*					pCorrelatedViewMasks;
		};
		renderPass = createRenderPass2(vkd, device, &renderPassCreateInfo);
	}

	// Framebuffer.
	std::vector<VkImageView> framebufferViews;

	framebufferViews.push_back(colorView.get());
	if (m_params.dsAttachmentNeeded())
		framebufferViews.push_back(dsImageView.get());

	const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(framebufferViews.size()), de::dataOrNull(framebufferViews), extent.width, extent.height);

	// Pipeline.
	std::vector<Move<VkPipeline>> graphicsPipelines;
	{
		const auto vertModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
		const auto fragModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

		const VkPipelineVertexInputStateCreateInfo	vertexInputInfo		= initVulkanStructure();
		VkPipelineInputAssemblyStateCreateInfo		inputAssemblyInfo	= initVulkanStructure();
		VkPipelineViewportStateCreateInfo			viewportInfo		= initVulkanStructure();
		VkPipelineRasterizationStateCreateInfo		rasterizationInfo	= initVulkanStructure();
		VkPipelineMultisampleStateCreateInfo		multisampleInfo		= initVulkanStructure();
		VkPipelineDepthStencilStateCreateInfo		dsStateInfo			= initVulkanStructure();
		VkPipelineColorBlendStateCreateInfo			colorBlendInfo		= initVulkanStructure();
		VkPipelineColorBlendAttachmentState			colorBlendAttState	= {};

		// Topology.
		inputAssemblyInfo.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Viewports and scissors.
		const auto viewport			= makeViewport(extent);
		const auto scissor			= makeRect2D(extent);
		viewportInfo.viewportCount	= 1u;
		viewportInfo.pViewports		= &viewport;
		viewportInfo.scissorCount	= 1u;
		viewportInfo.pScissors		= &scissor;

		// Line width.
		rasterizationInfo.lineWidth	= 1.0f;

		// Multisample state.
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Depth/stencil state. This depends on the test parameters.
		if (m_params.needsDepthTest())
			dsStateInfo.depthTestEnable = VK_TRUE;
		if (depthWrites)
			dsStateInfo.depthWriteEnable = VK_TRUE;
		dsStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		if (m_params.needsStencilTest())
			dsStateInfo.stencilTestEnable = VK_TRUE;

		const auto stencilOpState	= makeStencilOpState(VK_STENCIL_OP_KEEP,												// failOp
														 (stencilWrites ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP),		// passOp
														 VK_STENCIL_OP_KEEP,												// depthFailOp
														 VK_COMPARE_OP_LESS,												// compareOp
														 0xFFu,																// compareMask
														 (stencilWrites ? 0xFFu : 0u),										// writeMask
														 stencilFailVal);													// reference
		dsStateInfo.front			= stencilOpState;
		dsStateInfo.back			= stencilOpState;

		colorBlendAttState.colorWriteMask	= (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
		colorBlendAttState.blendEnable		= VK_FALSE;
		colorBlendInfo.attachmentCount		= 1u;
		colorBlendInfo.pAttachments			= &colorBlendAttState;

		graphicsPipelines.push_back(makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
			vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(), // Shader modules.
			renderPass.get(), 0u /*subpass*/,
			&vertexInputInfo, &inputAssemblyInfo, nullptr, &viewportInfo, &rasterizationInfo, &multisampleInfo, &dsStateInfo, &colorBlendInfo, nullptr));

		// When the stencil test is enabled, we need a second pipeline changing the reference value so the stencil test passes the second time.
		if (m_params.needsStencilTest())
		{
			dsStateInfo.front.reference	= stencilPassVal;
			dsStateInfo.back.reference	= stencilPassVal;

			graphicsPipelines.push_back(makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
				vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(), // Shader modules.
				renderPass.get(), 0u /*subpass*/,
				&vertexInputInfo, &inputAssemblyInfo, nullptr, &viewportInfo, &rasterizationInfo, &multisampleInfo, &dsStateInfo, &colorBlendInfo, nullptr));
		}
	}

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();
	const auto renderArea	= makeRect2D(extent);

	// Output buffers to check the color attachment, depth/stencil attachment and output storage images.
	using BufferPtr = de::MovePtr<BufferWithMemory>;

	BufferPtr				colorVerifBuffer;
	BufferPtr				depthVerifBuffer;
	BufferPtr				stencilVerifBuffer;
	std::vector<BufferPtr>	storageVerifBuffers;

	{
		const auto colorVerifBufferSize	= getCopyBufferSize(tcuColorFormat, extent);
		const auto colorVerifBufferInfo	= makeBufferCreateInfo(colorVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		colorVerifBuffer = BufferPtr(new BufferWithMemory(vkd, device, alloc, colorVerifBufferInfo, MemoryRequirement::HostVisible));
	}

	if (hasDepth)
	{
		const auto depthVerifBufferSize	= getCopyBufferSize(tcuDepthFormat, extent);
		const auto depthVerifBufferInfo	= makeBufferCreateInfo(depthVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		depthVerifBuffer = BufferPtr(new BufferWithMemory(vkd, device, alloc, depthVerifBufferInfo, MemoryRequirement::HostVisible));
	}

	if (hasStencil)
	{
		const auto stencilVerifBufferSize	= getCopyBufferSize(tcuStencilFormat, extent);
		const auto stencilVerifBufferInfo	= makeBufferCreateInfo(stencilVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		stencilVerifBuffer = BufferPtr(new BufferWithMemory(vkd, device, alloc, stencilVerifBufferInfo, MemoryRequirement::HostVisible));
	}

	storageVerifBuffers.reserve(descriptors.size());
	for (const auto& desc : descriptors)
	{
		const auto storageFormat		= getAspectStorageFormat(desc.aspect);
		const auto tcuStorageFormat		= mapVkFormat(storageFormat);
		const auto descVerifBufferSize	= getCopyBufferSize(tcuStorageFormat, extent);
		const auto descVerifBufferInfo	= makeBufferCreateInfo(descVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		storageVerifBuffers.emplace_back(new BufferWithMemory(vkd, device, alloc, descVerifBufferInfo, MemoryRequirement::HostVisible));
	}

	beginCommandBuffer(vkd, cmdBuffer);

	// Transition layout for output images.
	std::vector<VkImageMemoryBarrier> outputImgBarriers;
	for (const auto& outputImg : outputImages)
		outputImgBarriers.push_back(makeImageMemoryBarrier(0u, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, outputImg->get(), colorSRR));
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, static_cast<uint32_t>(outputImgBarriers.size()), de::dataOrNull(outputImgBarriers));

	// Clear color and depth/stencil buffer.
	const auto colorPreTransferBarrier	= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.get(), colorSRR);
	const auto dsPreTransferBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dsImage.get(), depthStencilSRR);
	const std::vector<VkImageMemoryBarrier> preTransferBarriers = { colorPreTransferBarrier, dsPreTransferBarrier };

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr,
		static_cast<uint32_t>(preTransferBarriers.size()), de::dataOrNull(preTransferBarriers));

	const auto colorClearValue	= makeClearValueColorVec4(colorClearVal);
	const auto dsClearValue		= makeClearValueDepthStencil(depthClearValue, stencilClearVal);

	vkd.cmdClearColorImage(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colorClearValue.color, 1u, &colorSRR);
	vkd.cmdClearDepthStencilImage(cmdBuffer, dsImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dsClearValue.depthStencil, 1u, &depthStencilSRR);

	const auto graphicsAccesses			=	( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
											| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
											| VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
											| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
											| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );
	const auto colorPostTransferBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, graphicsAccesses, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorLayout, colorBuffer.get(), colorSRR);
	const auto dsPostTransferBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, graphicsAccesses, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_params.layout, dsImage.get(), depthStencilSRR);
	const std::vector<VkImageMemoryBarrier> postTransferBarriers = { colorPostTransferBarrier, dsPostTransferBarrier };

	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u, 0u, nullptr, 0u, nullptr,
		static_cast<uint32_t>(postTransferBarriers.size()), de::dataOrNull(postTransferBarriers));

	// Render pass.
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea);

	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
		0u, static_cast<uint32_t>(descriptorSets.size()), de::dataOrNull(descriptorSets), 0u, nullptr);

	const auto useSecondDraw = m_params.depthBufferNeeded();

	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines.at(0).get());
	{
		if (useSecondDraw)
		{
			// Two draws: the first draw will use the red color.
			pcData = PushConstantData(colorFailVal, (m_params.needsDepthTest() ? depthFailValue : depthPassValue));
		}
		else
		{
			// If there will be no more draws, the first one needs to pass and use the right color.
			pcData = PushConstantData(colorPassVal, depthPassValue);
		}

		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &pcData);
		vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	}
	if (useSecondDraw)
	{
		// The second draw, if used, always needs to pass and use the right color.
		if (m_params.needsStencilTest())
		{
			// Pipeline with a good stencil reference value.
			DE_ASSERT(graphicsPipelines.size() > 1);
			vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines.at(1).get());
		}
		pcData = PushConstantData(colorPassVal, depthPassValue);

		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &pcData);
		vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	}

	endRenderPass(vkd, cmdBuffer);

	// Copy color attachment.
	{
		const auto colorLayers				= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
		const auto copyRegion				= makeBufferImageCopy(extent, colorLayers);
		const auto colorPostWriteBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, colorLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), colorSRR);
		vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorPostWriteBarrier);
		vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorVerifBuffer->get(), 1u, &copyRegion);
	}

	// Copy aspects of DS attachment.
	{
		const auto dsPostWriteBarrier	= makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, m_params.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dsImage.get(), depthStencilSRR);
		const auto fragmentTestStages	= (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		vkd.cmdPipelineBarrier(cmdBuffer, fragmentTestStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &dsPostWriteBarrier);

		if (hasDepth)
		{
			const auto depthLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
			const auto copyRegion	= makeBufferImageCopy(extent, depthLayers);
			vkd.cmdCopyImageToBuffer(cmdBuffer, dsImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthVerifBuffer->get(), 1u, &copyRegion);
		}

		if (hasStencil)
		{
			const auto stencilLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u);
			const auto copyRegion		= makeBufferImageCopy(extent, stencilLayers);
			vkd.cmdCopyImageToBuffer(cmdBuffer, dsImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilVerifBuffer->get(), 1u, &copyRegion);
		}
	}

	// Copy storage images.
	{
		std::vector<VkImageMemoryBarrier> storagePostBarriers;
		storagePostBarriers.reserve(outputImages.size());

		for (const auto& outImg : outputImages)
			storagePostBarriers.push_back(makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outImg->get(), colorSRR));

		vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, static_cast<uint32_t>(storagePostBarriers.size()), de::dataOrNull(storagePostBarriers));

		const auto colorLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
		const auto copyRegion	= makeBufferImageCopy(extent, colorLayers);

		DE_ASSERT(outputImages.size() == storageVerifBuffers.size());
		for (size_t i = 0u; i < outputImages.size(); ++i)
			vkd.cmdCopyImageToBuffer(cmdBuffer, outputImages[i]->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, storageVerifBuffers[i]->get(), 1u, &copyRegion);
	}

	// Transfer to host barrier for buffers.
	const auto transferToHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &transferToHostBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify the different buffers.
	const tcu::IVec3 iExtent (static_cast<int>(extent.width), static_cast<int>(extent.height), static_cast<int>(extent.depth));
	auto& log = m_context.getTestContext().getLog();

	// Verify color buffer contents.
	{
		auto& verifAlloc = colorVerifBuffer->getAllocation();
		invalidateAlloc(vkd, device, verifAlloc);

		tcu::ConstPixelBufferAccess colorPixels(tcuColorFormat, iExtent, verifAlloc.getHostPtr());
		if (!tcu::floatThresholdCompare(log, "ColorResult", "", colorPassVal, colorPixels, tcu::Vec4(0.0f), tcu::COMPARE_LOG_ON_ERROR))
			TCU_FAIL("Unexpected color buffer contents; check log for details");
	}

	// Verify depth buffer contents.
	if (hasDepth)
	{
		auto& verifAlloc = depthVerifBuffer->getAllocation();
		invalidateAlloc(vkd, device, verifAlloc);

		tcu::TextureLevel	referenceDepth	(tcuDepthFormat, iExtent.x(), iExtent.y(), iExtent.z());
		auto				referenceAccess	= referenceDepth.getAccess();
		const auto			refDepthVal		= (depthWrites ? depthPassValue : depthClearValue);

		for (int z = 0; z < iExtent.z(); ++z)
		for (int y = 0; y < iExtent.y(); ++y)
		for (int x = 0; x < iExtent.x(); ++x)
			referenceAccess.setPixDepth(refDepthVal, x, y, z);

		tcu::ConstPixelBufferAccess depthPixels(tcuDepthFormat, iExtent, verifAlloc.getHostPtr());
		if (!tcu::dsThresholdCompare(log, "DepthResult", "", referenceAccess, depthPixels, 0.1f, tcu::COMPARE_LOG_ON_ERROR))
			TCU_FAIL("Unexpected value in depth buffer; check log for details");
	}

	// Verify stencil buffer contents.
	if (hasStencil)
	{
		auto& verifAlloc = stencilVerifBuffer->getAllocation();
		invalidateAlloc(vkd, device, verifAlloc);

		tcu::TextureLevel	referenceStencil	(tcuStencilFormat, iExtent.x(), iExtent.y(), iExtent.z());
		auto				referenceAccess		= referenceStencil.getAccess();
		const auto			refStencilVal		= static_cast<int>(stencilWrites ? stencilPassVal : stencilClearVal);

		for (int z = 0; z < iExtent.z(); ++z)
		for (int y = 0; y < iExtent.y(); ++y)
		for (int x = 0; x < iExtent.x(); ++x)
			referenceAccess.setPixStencil(refStencilVal, x, y, z);

		tcu::ConstPixelBufferAccess stencilPixels(tcuStencilFormat, iExtent, verifAlloc.getHostPtr());
		if (!tcu::dsThresholdCompare(log, "StencilResult", "", referenceAccess, stencilPixels, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
			TCU_FAIL("Unexpected value in stencil buffer; check log for details");
	}

	// Verify output images.
	for (size_t bufferIdx = 0u; bufferIdx < storageVerifBuffers.size(); ++bufferIdx)
	{
		const auto& verifBuffer = storageVerifBuffers[bufferIdx];
		auto& verifAlloc = verifBuffer->getAllocation();
		invalidateAlloc(vkd, device, verifAlloc);

		const auto					bufferFormat	= getAspectStorageFormat(descriptors.at(bufferIdx).aspect);
		const auto					tcuBufferFormat	= mapVkFormat(bufferFormat);
		tcu::ConstPixelBufferAccess	colorPixels		(tcuBufferFormat, iExtent, verifAlloc.getHostPtr());
		const std::string			resultName		= "Storage" + de::toString(bufferIdx);

		if (descriptors.at(bufferIdx).aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
		{
			if (!tcu::floatThresholdCompare(log, resultName.c_str(), "", tcu::Vec4(depthClearValue, 0.0f, 0.0f, 1.0f), colorPixels, tcu::Vec4(0.1f, 0.0f, 0.0f, 0.0f), tcu::COMPARE_LOG_ON_ERROR))
				TCU_FAIL("Unexpected value in depth storage buffer " + de::toString(bufferIdx) + "; check log for details");
		}
		else if (descriptors.at(bufferIdx).aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
		{
			tcu::TextureLevel stencilRef(tcuBufferFormat, iExtent.x(), iExtent.y(), iExtent.z());
			auto colorPIxels = stencilRef.getAccess();

			for (int z = 0; z < iExtent.z(); ++z)
			for (int y = 0; y < iExtent.y(); ++y)
			for (int x = 0; x < iExtent.x(); ++x)
				colorPIxels.setPixel(tcu::UVec4(stencilClearVal, 0u, 0u, 0u), x, y, z);

			if (!tcu::intThresholdCompare(log, resultName.c_str(), "", colorPIxels, colorPixels, tcu::UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
				TCU_FAIL("Unexpected value in stencil storage buffer " + de::toString(bufferIdx) + "; check log for details");
		}
		else
			DE_ASSERT(false);
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createImageDepthStencilDescriptorTests (tcu::TestContext& testCtx)
{
	using TestCaseGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	const VkFormat kDepthStencilFormats[] =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	// Layouts used in these tests as VkDescriptorImageInfo::imageLayout.
	const VkImageLayout kTestedLayouts[] =
	{
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
	};

	// Types of read-only combinations to test.
	ROAccessVec kReadOnlyDSAttachment		= { ReadOnlyAccess::DS_ATTACHMENT };
	ROAccessVec kReadOnlyInputAttachment	= { ReadOnlyAccess::INPUT_ATTACHMENT };
	ROAccessVec kReadOnlySampled			= { ReadOnlyAccess::SAMPLED };
	ROAccessVec kReadOnlyDSSampled			= { ReadOnlyAccess::DS_ATTACHMENT, ReadOnlyAccess::SAMPLED };
	ROAccessVec kReadOnlyInputSampled		= { ReadOnlyAccess::INPUT_ATTACHMENT, ReadOnlyAccess::SAMPLED };

	const ROAccessVec* kROAccessCases[] =
	{
		&kReadOnlyDSAttachment,
		&kReadOnlyInputAttachment,
		&kReadOnlySampled,
		&kReadOnlyDSSampled,
		&kReadOnlyInputSampled,
	};

	const auto kLayoutPrefixLen = std::string("VK_IMAGE_LAYOUT_").size();
	const auto kFormatPrefixLen = std::string("VK_FORMAT_").size();

	TestCaseGroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "depth_stencil_descriptor", "Tests using depth/stencil images as descriptors"));

	for (const auto& layout : kTestedLayouts)
	{
		const auto layoutStr		= de::toString(layout);
		const auto layoutGroupName	= de::toLower(layoutStr.substr(kLayoutPrefixLen));
		const auto layoutGroupDesc	= "Tests using the " + layoutStr + " layout";

		TestCaseGroupPtr layoutGroup(new tcu::TestCaseGroup(testCtx, layoutGroupName.c_str(), layoutGroupDesc.c_str()));

		for (const auto& format : kDepthStencilFormats)
		{
			const auto formatStr		= de::toString(format);
			const auto formatGroupName	= de::toLower(formatStr.substr(kFormatPrefixLen));
			const auto formatGroupDesc	= "Tests using the " + formatStr + " format";

			TestCaseGroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, formatGroupName.c_str(), formatGroupDesc.c_str()));

			const auto depthAccess		= getLegalAccess(layout, VK_IMAGE_ASPECT_DEPTH_BIT);
			const auto stencilAccess	= getLegalAccess(layout, VK_IMAGE_ASPECT_STENCIL_BIT);
			const auto tcuFormat		= mapVkFormat(format);

			const auto hasDepthAccess	= (depthAccess != AspectAccess::NONE);
			const auto hasStencilAccess	= (stencilAccess != AspectAccess::NONE);
			const auto hasDepth			= tcu::hasDepthComponent(tcuFormat.order);
			const auto hasStencil		= tcu::hasStencilComponent(tcuFormat.order);

			if (hasDepthAccess != hasDepth)
				continue;
			if (hasStencilAccess != hasStencil)
				continue;

			if (depthAccess == AspectAccess::RO)
			{
				for (const auto& depthROCase : kROAccessCases)
				{
					const std::string depthPart = "depth_" + de::toString(*depthROCase);
					if (stencilAccess == AspectAccess::RO)
					{
						for (const auto& stencilROCase : kROAccessCases)
						{
							if (incompatibleInputAttachmentAccess(depthAccess, depthROCase, stencilAccess, stencilROCase))
								continue;

							const std::string stencilPart = "_stencil_" + de::toString(*stencilROCase);
							const TestParams params =
							{
								format,						//	VkFormat				format;
								layout,						//	VkImageLayout			layout;
								depthAccess,				//	AspectAccess			depthAccess;
								stencilAccess,				//	AspectAccess			stencilAccess;
								tcu::just(*depthROCase),	//	tcu::Maybe<ROAccessVec>	depthROAccesses;
								tcu::just(*stencilROCase),	//	tcu::Maybe<ROAccessVec>	stencilROAccesses;
							};
							formatGroup->addChild(new DepthStencilDescriptorCase(testCtx, depthPart + stencilPart, "", params));
						}
					}
					else
					{
						if (incompatibleInputAttachmentAccess(depthAccess, depthROCase, stencilAccess, nullptr))
							continue;

						const std::string stencilPart = "_stencil_" + de::toString(stencilAccess);
						const TestParams params =
						{
							format,							//	VkFormat				format;
							layout,							//	VkImageLayout			layout;
							depthAccess,					//	AspectAccess			depthAccess;
							stencilAccess,					//	AspectAccess			stencilAccess;
							tcu::just(*depthROCase),		//	tcu::Maybe<ROAccessVec>	depthROAccesses;
							tcu::Nothing,					//	tcu::Maybe<ROAccessVec>	stencilROAccesses;
						};
						formatGroup->addChild(new DepthStencilDescriptorCase(testCtx, depthPart + stencilPart, "", params));
					}
				}
			}
			else
			{
				const std::string depthPart = "depth_" + de::toString(depthAccess);

				if (stencilAccess == AspectAccess::RO)
				{
					for (const auto& stencilROCase : kROAccessCases)
					{
						if (incompatibleInputAttachmentAccess(depthAccess, nullptr, stencilAccess, stencilROCase))
							continue;

						const std::string stencilPart = "_stencil_" + de::toString(*stencilROCase);
						const TestParams params =
						{
							format,							//	VkFormat				format;
							layout,							//	VkImageLayout			layout;
							depthAccess,					//	AspectAccess			depthAccess;
							stencilAccess,					//	AspectAccess			stencilAccess;
							tcu::Nothing,					//	tcu::Maybe<ROAccessVec>	depthROAccesses;
							tcu::just(*stencilROCase),		//	tcu::Maybe<ROAccessVec>	stencilROAccesses;
						};
						formatGroup->addChild(new DepthStencilDescriptorCase(testCtx, depthPart + stencilPart, "", params));
					}
				}
				else
				{
					if (incompatibleInputAttachmentAccess(depthAccess, nullptr, stencilAccess, nullptr))
						continue;

					const std::string stencilPart = "_stencil_" + de::toString(stencilAccess);
					const TestParams params =
					{
						format,							//	VkFormat				format;
						layout,							//	VkImageLayout			layout;
						depthAccess,					//	AspectAccess			depthAccess;
						stencilAccess,					//	AspectAccess			stencilAccess;
						tcu::Nothing,					//	tcu::Maybe<ROAccessVec>	depthROAccesses;
						tcu::Nothing,					//	tcu::Maybe<ROAccessVec>	stencilROAccesses;
					};
					formatGroup->addChild(new DepthStencilDescriptorCase(testCtx, depthPart + stencilPart, "", params));
				}
			}

			layoutGroup->addChild(formatGroup.release());
		}

		mainGroup->addChild(layoutGroup.release());
	}

	return mainGroup.release();
}

} // image
} // vkt
