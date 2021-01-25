/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Tests for descriptor updates.
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorUpdateTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <math.h>

namespace vkt
{
namespace BindingModel
{
namespace
{

// Test matches VkPositiveLayerTest.EmptyDescriptorUpdateTest
tcu::TestStatus EmptyDescriptorUpdateCase (Context& context)
{
	const vk::DeviceInterface&				vki					= context.getDeviceInterface();
	const vk::VkDevice						device				= context.getDevice();
	vk::Allocator&							allocator			= context.getDefaultAllocator();

	// Create layout with two uniform buffer descriptors w/ empty binding between them
	vk::DescriptorSetLayoutBuilder			builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL);
	builder.addBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, (vk::VkShaderStageFlags)0, DE_NULL);
	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_ALL);

	vk::Unique<vk::VkDescriptorSetLayout>	layout				(builder.build(vki, device, (vk::VkDescriptorSetLayoutCreateFlags)0));

	// Create descriptor pool
	vk::Unique<vk::VkDescriptorPool>		descriptorPool		(vk::DescriptorPoolBuilder().addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2).build(vki, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));

	// Create descriptor set
	const vk::VkDescriptorSetAllocateInfo	setAllocateInfo		=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		*descriptorPool,									// VkDescriptorPool				descriptorPool
		1,													// deUint32						descriptorSetCount
		&layout.get()										// const VkDescriptorSetLayout*	pSetLayouts
	};

	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(allocateDescriptorSet(vki, device, &setAllocateInfo));

	// Create a buffer to be used for update
	const vk::VkBufferCreateInfo			bufferCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,									// const void*			pNext
		(vk::VkBufferCreateFlags)DE_NULL,			// VkBufferCreateFlags	flags
		256,										// VkDeviceSize			size
		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,		// VkBufferUsageFlags	usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0,											// deUint32				queueFamilyIndexCount
		DE_NULL										// const deUint32*		pQueueFamilyIndices
	};

	vk::Unique<vk::VkBuffer>				buffer				(createBuffer(vki, device, &bufferCreateInfo));
	const vk::VkMemoryRequirements			requirements		= vk::getBufferMemoryRequirements(vki, device, *buffer);
	de::MovePtr<vk::Allocation>				allocation			= allocator.allocate(requirements, vk::MemoryRequirement::Any);

	VK_CHECK(vki.bindBufferMemory(device, *buffer, allocation->getMemory(), allocation->getOffset()));

	// Only update the descriptor at binding 2
	const vk::VkDescriptorBufferInfo		descriptorInfo		=
	{
		*buffer,		// VkBuffer		buffer
		0,				// VkDeviceSize	offset
		VK_WHOLE_SIZE	// VkDeviceSize	range
	};

	const vk::VkWriteDescriptorSet			descriptorWrite		=
	{
		vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureTypes					Type
		DE_NULL,									// const void*						pNext
		*descriptorSet,								// VkDescriptorSet					dstSet
		2,											// deUint32							dstBinding
		0,											// deUint32							dstArrayElement
		1,											// deUint32							descriptorCount
		vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// VkDescriptorType					descriptorType
		DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo
		&descriptorInfo,							// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL										// const VkBufferView*				pTexelBufferView
	};

	vki.updateDescriptorSets(device, 1, &descriptorWrite, 0, DE_NULL);

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}


tcu::TestCaseGroup* createEmptyDescriptorUpdateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "empty_descriptor", "Update last descriptor in a set that includes an empty binding"));

	addFunctionCase(group.get(), "uniform_buffer", "", EmptyDescriptorUpdateCase);

	return group.release();
}

enum class PointerCase
{
	ZERO = 0,
	ONE,
	DESTROYED,
};

struct SamplerlessParams
{
	vk::VkDescriptorType	type;
	PointerCase				pointer;
	deUint32				descriptorSet;
};

class SamplerlessDescriptorWriteTestCase : public vkt::TestCase
{
public:
								SamplerlessDescriptorWriteTestCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const SamplerlessParams& params);
	virtual						~SamplerlessDescriptorWriteTestCase	(void) {}

	virtual void				initPrograms						(vk::SourceCollections& programCollection) const;
	virtual vkt::TestInstance*	createInstance						(Context& context) const;
	virtual void				checkSupport						(Context& context) const;

	vk::VkFormatFeatureFlagBits	getMainImageFeature					(void) const;

	static const vk::VkFormat	kImageFormat						= vk::VK_FORMAT_R8G8B8A8_UNORM;
private:
	SamplerlessParams			m_params;
};

class SamplerlessDescriptorWriteTestInstance : public vkt::TestInstance
{
public:
								SamplerlessDescriptorWriteTestInstance	(Context& context, const SamplerlessParams& params);
	virtual						~SamplerlessDescriptorWriteTestInstance	(void) {}

	vk::VkSampler				getSamplerHandle						(void) const;
	virtual tcu::TestStatus		iterate									(void);

	vk::VkExtent3D				getMainImageExtent						(void) const;
	vk::VkImageUsageFlags		getMainImageUsage						(void) const;
	vk::VkImageLayout			getMainImageShaderLayout				(void) const;

	static const vk::VkFormat	kImageFormat							= SamplerlessDescriptorWriteTestCase::kImageFormat;
	static const vk::VkExtent3D	kFramebufferExtent;
	static const vk::VkExtent3D	kMinimumExtent;
	static const tcu::Vec4		kDescriptorColor;
private:
	SamplerlessParams			m_params;
};

const vk::VkExtent3D	SamplerlessDescriptorWriteTestInstance::kFramebufferExtent	= vk::makeExtent3D(64u, 64u, 1u);
const vk::VkExtent3D	SamplerlessDescriptorWriteTestInstance::kMinimumExtent		= vk::makeExtent3D(1u, 1u, 1u);
const tcu::Vec4			SamplerlessDescriptorWriteTestInstance::kDescriptorColor	{0.0f, 1.0f, 0.0f, 1.0f};

SamplerlessDescriptorWriteTestCase::SamplerlessDescriptorWriteTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const SamplerlessParams& params)
	: vkt::TestCase{testCtx, name, description}
	, m_params(params)
{
}

void SamplerlessDescriptorWriteTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string vertexShader =
		"#version 450\n"
		"layout(location=0) in vec4 position;\n"
		"void main() { gl_Position = position; }\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertexShader);

	std::string descriptorDecl;
	std::string readOp;
	std::string extensions;

	switch (m_params.type)
	{
	case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		extensions		= "#extension GL_EXT_samplerless_texture_functions : require\n";
		descriptorDecl	= "layout(set=" + std::to_string(m_params.descriptorSet) + ", binding=0) uniform texture2D img;";
		readOp			= "texelFetch(img, ivec2(0, 0), 0)";
		break;
	case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		descriptorDecl	= "layout(rgba8, set=" + std::to_string(m_params.descriptorSet) + ", binding=0) uniform image2D img;";
		readOp			= "imageLoad(img, ivec2(0, 0))";
		break;
	case vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		descriptorDecl	= "layout(input_attachment_index=0, set=" + std::to_string(m_params.descriptorSet) + ", binding=0) uniform subpassInput img;";
		readOp			= "subpassLoad(img)";
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	std::ostringstream fragmentShader;

	fragmentShader
		<< "#version 450\n"
		<< extensions
		<< descriptorDecl << "\n"
		<< "layout(location = 0) out vec4 color_out;\n"
		<< "void main()\n"
		<< "{\n"
		<< "    color_out = " << readOp << ";\n"
		<< "}\n"
		;

	programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentShader.str());
}

vk::VkFormatFeatureFlagBits SamplerlessDescriptorWriteTestCase::getMainImageFeature (void) const
{
	vk::VkFormatFeatureFlagBits feature = static_cast<vk::VkFormatFeatureFlagBits>(0);

	switch (m_params.type)
	{
	case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:		feature = vk::VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;		break;
	case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:		feature = vk::VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;		break;
	case vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:	feature = vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;	break;
	default:
		DE_ASSERT(false);
		break;
	}

	return feature;
}

void SamplerlessDescriptorWriteTestCase::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();
	const auto	mainFeature		= getMainImageFeature();

	const vk::VkFormatFeatureFlags features =
	(
		vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT		|	// For color clearing.
		vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT	|	// For the separate frame buffer image (uses the same format).
		mainFeature
	);

	const auto props = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kImageFormat);
	if ((props.optimalTilingFeatures & features) != features)
		TCU_THROW(NotSupportedError, "Image format does not support the required features");
}

vkt::TestInstance* SamplerlessDescriptorWriteTestCase::createInstance (Context& context) const
{
	return new SamplerlessDescriptorWriteTestInstance{context, m_params};
}

SamplerlessDescriptorWriteTestInstance::SamplerlessDescriptorWriteTestInstance (Context& context, const SamplerlessParams& params)
	: vkt::TestInstance{context}
	, m_params(params)
{
}

struct DestroyedSampler
{
	vk::VkSampler sampler;

	DestroyedSampler (Context& context)
		: sampler{DE_NULL}
	{
		const vk::VkSamplerCreateInfo createInfo =
		{
			vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType		sType;
			nullptr,										// const void*			pNext;
			0u,												// VkSamplerCreateFlags	flags;
			vk::VK_FILTER_NEAREST,							// VkFilter				magFilter;
			vk::VK_FILTER_NEAREST,							// VkFilter				minFilter;
			vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode	mipmapMode;
			vk::VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode	addressModeU;
			vk::VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode	addressModeV;
			vk::VK_SAMPLER_ADDRESS_MODE_REPEAT,				// VkSamplerAddressMode	addressModeW;
			0.0f,											// float				mipLodBias;
			VK_FALSE,										// VkBool32				anisotropyEnable;
			1.0f,											// float				maxAnisotropy;
			VK_FALSE,										// VkBool32				compareEnable;
			vk::VK_COMPARE_OP_NEVER,						// VkCompareOp			compareOp;
			0.0f,											// float				minLod;
			0.0f,											// float				maxLod;
			vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,	// VkBorderColor		borderColor;
			VK_FALSE,										// VkBool32				unnormalizedCoordinates;
		};
		const auto newSampler = vk::createSampler(context.getDeviceInterface(), context.getDevice(), &createInfo);
		sampler = newSampler.get();
		// newSampler will be destroyed here and sampler will hold the former handle.
	}
};

vk::VkSampler SamplerlessDescriptorWriteTestInstance::getSamplerHandle (void) const
{
	if (m_params.pointer == PointerCase::ZERO)	return vk::VkSampler{DE_NULL};
	if (m_params.pointer == PointerCase::ONE)	return vk::VkSampler{1};
	static const DestroyedSampler destroyedSampler{m_context};
	return destroyedSampler.sampler;
}

vk::VkExtent3D SamplerlessDescriptorWriteTestInstance::getMainImageExtent (void) const
{
	const vk::VkExtent3D* extent = nullptr;

	switch (m_params.type)
	{
	case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:		// fallthrough
	case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:		extent = &kMinimumExtent;		break;
	case vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:	extent = &kFramebufferExtent;	break;
	default:
		DE_ASSERT(false);
		break;
	}

	return *extent;
}

vk::VkImageUsageFlags SamplerlessDescriptorWriteTestInstance::getMainImageUsage (void) const
{
	vk::VkImageUsageFlags usage = vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;	// Used when clearing the image.

	switch (m_params.type)
	{
	case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:		usage |= vk::VK_IMAGE_USAGE_SAMPLED_BIT;			break;
	case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:		usage |= vk::VK_IMAGE_USAGE_STORAGE_BIT;			break;
	case vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:	usage |= vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;	break;
	default:
		DE_ASSERT(false);
		break;
	}

	return usage;
}

vk::VkImageLayout SamplerlessDescriptorWriteTestInstance::getMainImageShaderLayout (void) const
{
	vk::VkImageLayout layout = vk::VK_IMAGE_LAYOUT_UNDEFINED;

	switch (m_params.type)
	{
	case vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:		// fallthrough
	case vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:	layout = vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	break;
	case vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:		layout = vk::VK_IMAGE_LAYOUT_GENERAL;					break;
	default:
		DE_ASSERT(false);
		break;
	}

	return layout;
}


tcu::TestStatus SamplerlessDescriptorWriteTestInstance::iterate (void)
{
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		allocator	= m_context.getDefaultAllocator();
	const auto	queue		= m_context.getUniversalQueue();
	const auto	queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	tcuFormat	= vk::mapVkFormat(kImageFormat);

	const vk::VkImageCreateInfo mainImgCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		0u,											// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		kImageFormat,								// VkFormat					format;
		getMainImageExtent(),						// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		getMainImageUsage(),						// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueIndex,								// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	const vk::VkImageCreateInfo fbImgCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		0u,											// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		kImageFormat,								// VkFormat					format;
		kFramebufferExtent,							// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |	// VkImageUsageFlags		usage;
		 vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT),			// Used when verifying the image.
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueIndex,								// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	// Create main and framebuffer images.
	const vk::ImageWithMemory mainImage	{vkd, device, allocator, mainImgCreateInfo,	vk::MemoryRequirement::Any};
	const vk::ImageWithMemory fbImage	{vkd, device, allocator, fbImgCreateInfo,	vk::MemoryRequirement::Any};

	// Corresponding image views.
	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto mainView					= vk::makeImageView(vkd, device, mainImage.get(),	vk::VK_IMAGE_VIEW_TYPE_2D, kImageFormat, colorSubresourceRange);
	const auto fbView					= vk::makeImageView(vkd, device, fbImage.get(),		vk::VK_IMAGE_VIEW_TYPE_2D, kImageFormat, colorSubresourceRange);

	// Buffer to copy rendering result to.
	const vk::VkDeviceSize		resultsBufferSize	= static_cast<vk::VkDeviceSize>(static_cast<deUint32>(tcu::getPixelSize(tcuFormat)) * kFramebufferExtent.width * kFramebufferExtent.height * kFramebufferExtent.depth);
	const auto					resultsBufferInfo	= vk::makeBufferCreateInfo(resultsBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const vk::BufferWithMemory	resultsBuffer		{vkd, device, allocator, resultsBufferInfo, vk::MemoryRequirement::HostVisible};

	const std::vector<tcu::Vec4> fullScreenQuad =
	{
		{ -1.f, -1.f,	0.f, 1.f },
		{  1.f, -1.f,	0.f, 1.f },
		{ -1.f,  1.f,	0.f, 1.f },
		{ -1.f,  1.f,	0.f, 1.f },
		{  1.f, -1.f,	0.f, 1.f },
		{  1.f,  1.f,	0.f, 1.f },
	};

	// Vertex buffer.
	const vk::VkDeviceSize		vertexBufferSize	= static_cast<vk::VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
	const auto					vertexBufferInfo	= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const vk::BufferWithMemory	vertexBuffer		{vkd, device, allocator, vertexBufferInfo, vk::MemoryRequirement::HostVisible};

	// Copy data to vertex buffer.
	const auto&	vertexAlloc		= vertexBuffer.getAllocation();
	const auto	vertexDataPtr	= reinterpret_cast<char*>(vertexAlloc.getHostPtr()) + vertexAlloc.getOffset();
	deMemcpy(vertexDataPtr, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
	vk::flushAlloc(vkd, device, vertexAlloc);

	// Descriptor set layouts.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	std::vector<vk::Move<vk::VkDescriptorSetLayout>> descriptorSetLayouts;
	// Create layouts for required amount of empty descriptor sets before the one that is actually used.
	for (deUint32 descIdx = 0u; descIdx < m_params.descriptorSet; descIdx++)
	{
		descriptorSetLayouts.push_back(layoutBuilder.build(vkd, device));
	}
	// Create a layout for the descriptor set that is actually used.
	layoutBuilder.addSingleBinding(m_params.type, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	descriptorSetLayouts.push_back(layoutBuilder.build(vkd, device));

	// Descriptor pool.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(m_params.type);
	const auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, m_params.descriptorSet + 1);

	// Descriptor sets.
	std::vector<vk::Move<vk::VkDescriptorSet>> descriptorSets;
	for (deUint32 descIdx = 0u; descIdx < m_params.descriptorSet; descIdx++)
	{
		descriptorSets.push_back(vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayouts[descIdx].get()));
	}
	descriptorSets.push_back(vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayouts[m_params.descriptorSet].get()));

	// Update descriptor set with the descriptor.
	// IMPORTANT: the chosen sampler handle is used here.
	vk::DescriptorSetUpdateBuilder updateBuilder;
	const auto descriptorImageInfo = vk::makeDescriptorImageInfo(getSamplerHandle(), mainView.get(), getMainImageShaderLayout());
	updateBuilder.writeSingle(descriptorSets[m_params.descriptorSet].get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), m_params.type, &descriptorImageInfo);
	updateBuilder.update(vkd, device);

	// Shader modules.
	const auto vertexModule	= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragModule	= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Render pass.
	const vk::VkAttachmentDescription fbAttachment =
	{
		0u,												// VkAttachmentDescriptionFlags	flags;
		kImageFormat,									// VkFormat						format;
		vk::VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout				finalLayout;
	};

	std::vector<vk::VkAttachmentDescription> attachmentDescs;
	attachmentDescs.push_back(fbAttachment);

	if (m_params.type == vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
	{
		// Add it as a frame buffer attachment.
		const vk::VkAttachmentDescription inputAttachment =
		{
			0u,												// VkAttachmentDescriptionFlags	flags;
			kImageFormat,									// VkFormat						format;
			vk::VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples;
			vk::VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp			loadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			storeOp;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
			getMainImageShaderLayout(),						// VkImageLayout				initialLayout;
			getMainImageShaderLayout(),						// VkImageLayout				finalLayout;
		};

		attachmentDescs.push_back(inputAttachment);
	}

	std::vector<vk::VkAttachmentReference> inputAttachments;
	if (m_params.type == vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
	{
		const vk::VkAttachmentReference inputRef =
		{
			1u,												// deUint32			attachment;
			getMainImageShaderLayout(),						// VkImageLayout	layout;
		};

		inputAttachments.push_back(inputRef);
	}

	const vk::VkAttachmentReference colorRef =
	{
			0u,												// deUint32			attachment;
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout	layout;
	};
	const std::vector<vk::VkAttachmentReference> colorAttachments(1u, colorRef);

	const vk::VkSubpassDescription subpass =
	{
		0u,																// VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,							// VkPipelineBindPoint				pipelineBindPoint;
		static_cast<deUint32>(inputAttachments.size()),					// deUint32							inputAttachmentCount;
		(inputAttachments.empty() ? nullptr : inputAttachments.data()),	// const VkAttachmentReference*		pInputAttachments;
		static_cast<deUint32>(colorAttachments.size()),					// deUint32							colorAttachmentCount;
		colorAttachments.data(),										// const VkAttachmentReference*		pColorAttachments;
		0u,																// const VkAttachmentReference*		pResolveAttachments;
		nullptr,														// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,																// deUint32							preserveAttachmentCount;
		nullptr,														// const deUint32*					pPreserveAttachments;
	};
	const std::vector<vk::VkSubpassDescription> subpasses(1u, subpass);

	const vk::VkRenderPassCreateInfo renderPassInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		nullptr,										// const void*						pNext;
		0u,												// VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescs.size()),	// deUint32							attachmentCount;
		attachmentDescs.data(),							// const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpasses.size()),		// deUint32							subpassCount;
		subpasses.data(),								// const VkSubpassDescription*		pSubpasses;
		0u,												// deUint32							dependencyCount;
		nullptr,										// const VkSubpassDependency*		pDependencies;
	};
	const auto renderPass = vk::createRenderPass(vkd, device, &renderPassInfo);

	// Framebuffer.
	std::vector<vk::VkImageView> attachments;
	attachments.push_back(fbView.get());
	if (m_params.type == vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		attachments.push_back(mainView.get());
	const auto framebuffer = vk::makeFramebuffer(vkd, device, renderPass.get(), static_cast<deUint32>(attachments.size()), attachments.data(), kFramebufferExtent.width, kFramebufferExtent.height, kFramebufferExtent.depth);

	// Pipeline layout.
	const auto pipelineLayout = vk::makePipelineLayout(vkd, device, descriptorSetLayouts);

	// Graphics pipeline.
	const std::vector<vk::VkViewport>	viewports(1u, vk::makeViewport(kFramebufferExtent));
	const std::vector<vk::VkRect2D>		scissors(1u, vk::makeRect2D(kFramebufferExtent));

	const auto pipeline = vk::makeGraphicsPipeline(
		vkd, device, pipelineLayout.get(),
		vertexModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
		renderPass.get(), viewports, scissors);

	// Command pool and command buffer.
	const auto cmdPool		= vk::createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Draw quad.
	const vk::VkRect2D		renderArea			= vk::makeRect2D(kFramebufferExtent);
	const tcu::Vec4			clearFbColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const vk::VkDeviceSize	vertexBufferOffset	= 0ull;

	const auto vtxBufferBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, vertexBuffer.get(), 0ull, vertexBufferSize);
	const auto preClearBarrier	= vk::makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mainImage.get(), colorSubresourceRange);
	const auto postClearBarrier	= vk::makeImageMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
						vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, getMainImageShaderLayout(), mainImage.get(), colorSubresourceRange);
	const auto clearDescColor	= vk::makeClearValueColor(kDescriptorColor);

	vk::beginCommandBuffer(vkd, cmdBuffer);

	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u, 0u, nullptr, 1u, &vtxBufferBarrier, 0u, nullptr);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preClearBarrier);
	vkd.cmdClearColorImage(cmdBuffer, mainImage.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDescColor.color, 1u, &colorSubresourceRange);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &postClearBarrier);

	vk::beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea, clearFbColor);
	vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), m_params.descriptorSet, 1u, &descriptorSets[m_params.descriptorSet].get(), 0u, nullptr);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(fullScreenQuad.size()), 1u, 0u, 0u);
	vk::endRenderPass(vkd, cmdBuffer);

	const tcu::IVec2 copySize{static_cast<int>(kFramebufferExtent.width), static_cast<int>(kFramebufferExtent.height)};
	vk::copyImageToBuffer(vkd, cmdBuffer, fbImage.get(), resultsBuffer.get(), copySize);

	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check results.
	const auto& resultsBufferAlloc = resultsBuffer.getAllocation();
	vk::invalidateAlloc(vkd, device, resultsBufferAlloc);

	const auto							resultsBufferPtr	= reinterpret_cast<const char*>(resultsBufferAlloc.getHostPtr()) + resultsBufferAlloc.getOffset();
	const tcu::ConstPixelBufferAccess	resultPixels		{tcuFormat, copySize[0], copySize[1], 1, resultsBufferPtr};

	bool pass = true;
	for (int x = 0; pass && x < resultPixels.getWidth(); ++x)
	for (int y = 0; pass && y < resultPixels.getHeight(); ++y)
	for (int z = 0; pass && z < resultPixels.getDepth(); ++z)
	{
		const auto pixel = resultPixels.getPixel(x, y, z);
		pass = (pixel == kDescriptorColor);
	}

	tcu::TestStatus status = tcu::TestStatus::pass("Pass");
	if (!pass)
	{
		auto& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Image("color", "Rendered image", resultPixels);
		status = tcu::TestStatus::fail("Pixel mismatch; please check the rendered image");
	}

	return status;
}

tcu::TestCaseGroup* createSamplerlessWriteTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "samplerless", "Verify sampler unused with some descriptor image types"));

	const std::vector<std::pair<vk::VkDescriptorType, std::string>> descriptorTypes =
	{
		std::make_pair(vk::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	"sampled_img"),
		std::make_pair(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,	"storage_img"),
		std::make_pair(vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	"input_attachment"),
	};

	const std::vector<std::pair<PointerCase, std::string>> pointerCases =
	{
		std::make_pair(PointerCase::ZERO,		"sampler_zero"),
		std::make_pair(PointerCase::ONE,		"sampler_one"),
		std::make_pair(PointerCase::DESTROYED,	"sampler_destroyed"),
	};

	for (const auto& typeCase		: descriptorTypes)
	for (const auto& pointerCase	: pointerCases)
	for (deUint32 descriptorSet = 0u; descriptorSet < 2u; descriptorSet++)
	{
		std::string			caseName	= typeCase.second + "_" + pointerCase.second;
		SamplerlessParams	params		{typeCase.first, pointerCase.first, descriptorSet};
		if (descriptorSet > 0u)
		{
			caseName += "_set_" + std::to_string(descriptorSet);
		}

		group->addChild(new SamplerlessDescriptorWriteTestCase(testCtx, caseName, "", params));
	}

	return group.release();
}

class RandomDescriptorUpdateTestCase : public vkt::TestCase
{
public:
	RandomDescriptorUpdateTestCase				(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
	virtual	~RandomDescriptorUpdateTestCase		(void) {}

	virtual void				initPrograms	(vk::SourceCollections& programCollection) const;
	virtual vkt::TestInstance*	createInstance	(Context& context) const;

private:
};

class RandomDescriptorUpdateTestInstance : public vkt::TestInstance
{
public:
	RandomDescriptorUpdateTestInstance			(Context& context);
	virtual	~RandomDescriptorUpdateTestInstance	(void) {}

	virtual tcu::TestStatus		iterate			(void);

	static const vk::VkExtent3D	kFramebufferExtent;
	static const vk::VkFormat	kImageFormat;
	static const deUint32		kNumBuffers;
	static const deUint32		kNumOffsets;
	static const deUint32		kNumIterations;

private:
	deRandom					m_random;
};

const vk::VkExtent3D	RandomDescriptorUpdateTestInstance::kFramebufferExtent	= vk::makeExtent3D(64u, 64u, 1u);
const vk::VkFormat		RandomDescriptorUpdateTestInstance::kImageFormat		= vk::VK_FORMAT_R16G16B16A16_SFLOAT;
const deUint32			RandomDescriptorUpdateTestInstance::kNumBuffers			= 3u;
const deUint32			RandomDescriptorUpdateTestInstance::kNumOffsets			= 5u;
const deUint32			RandomDescriptorUpdateTestInstance::kNumIterations		= 1000u;

RandomDescriptorUpdateTestCase::RandomDescriptorUpdateTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
: vkt::TestCase(testCtx, name, description)
{
}

void RandomDescriptorUpdateTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string vertexShader =
			"#version 450\n"
			"layout(location=0) in vec4 position;\n"
			"void main() { gl_Position = position; }\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vertexShader);

	std::ostringstream fragmentShader;

	fragmentShader
			<< "#version 450\n"
			<< "layout(location = 0) out vec4 color_out;\n"
			<< "layout(set = 0, binding = 0) uniform buf\n"
			<< "{\n"
			<< "    vec4 data0;\n"
			<< "    vec4 data1;\n"
			<< "};\n"
			<< "void main()\n"
			<< "{\n"
			<< "    color_out = data0 + data1;\n"
			<< "}\n"
			;

	programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentShader.str());
}

vkt::TestInstance* RandomDescriptorUpdateTestCase::createInstance (Context& context) const
{
	return new RandomDescriptorUpdateTestInstance(context);
}

RandomDescriptorUpdateTestInstance::RandomDescriptorUpdateTestInstance(Context &context)
: vkt::TestInstance(context)
{
	deRandom_init(&m_random, 0);
}

tcu::TestStatus RandomDescriptorUpdateTestInstance::iterate()
{
	const auto&								vkd						= m_context.getDeviceInterface();
	const auto								device					= m_context.getDevice();
	auto&									allocator				= m_context.getDefaultAllocator();
	const auto								queue					= m_context.getUniversalQueue();
	const auto								queueIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto								tcuFormat				= vk::mapVkFormat(kImageFormat);
	vk::DescriptorSetLayoutBuilder			builder;

	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);

	vk::Unique<vk::VkDescriptorSetLayout>	layout					(builder.build(vkd, device, (vk::VkDescriptorSetLayoutCreateFlags)0));

	// Create descriptor pool
	vk::Unique<vk::VkDescriptorPool>		descriptorPool			(vk::DescriptorPoolBuilder().addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));

	// Create descriptor set
	const vk::VkDescriptorSetAllocateInfo	setAllocateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		*descriptorPool,									// VkDescriptorPool				descriptorPool
		1u,													// deUint32						descriptorSetCount
		&layout.get()										// const VkDescriptorSetLayout*	pSetLayouts
	};

	vk::Unique<vk::VkDescriptorSet>			descriptorSet			(allocateDescriptorSet(vkd, device, &setAllocateInfo));

	// The maximum allowed buffer offset alignment is 256 bytes. Meaningful data is placed at these offsets.
	const deUint32	bufferSize = 256u * kNumOffsets;

	float									bufferContents[kNumBuffers][bufferSize / 4];
	float									counter				= 1.0f;
	float									sign				= 1.0f;
	deUint32								offset				= 0;
	deUint32								channelSelector		= 0;

	// The buffers are filled with a running counter in one of the channels.
	// Both signed and unsigned values are used for each counter. Two vec4s
	// are initialized at offsets of 256 bytes (the maximum allowed alignment).
	// Everythin else is left as zero.
	for (deUint32 b = 0; b < kNumBuffers; b++)
	{
		deMemset(bufferContents[b], 0, bufferSize);

		for (deUint32 o = 0; o < kNumOffsets; o++)
		{
			offset = o * 64;

			// Two vectors at every offset.
			for (deUint32 v = 0; v < 2; v++)
			{
				// Only RGB channels are being tested.
				for (deUint32 c = 0; c < 3; c++)
				{
					if (c == channelSelector)
					{
						bufferContents[b][offset++] = sign * counter;
					}
					else
					{
						bufferContents[b][offset++] = 0.0f;
					}
				}
				// Keep alpha at one.
				bufferContents[b][offset++] = 1.0f;

				channelSelector = channelSelector + 1;

				// All three channels have been filled in. Switch a sign or increase the counter.
				if (channelSelector == 3)
				{
					channelSelector = 0;
					if (sign == 1.0f)
					{
						sign = -1.0f;
					}
					else
					{
						sign = 1.0f;
						counter += 1.0f;
					}
				}
			}
		}
	}

	const auto								bufferInfo				= vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	std::vector<std::shared_ptr<vk::BufferWithMemory>>	buffers;

	for (const auto& contents : bufferContents)
	{
		buffers.emplace_back(std::make_shared<vk::BufferWithMemory>(vkd, device, allocator, bufferInfo, vk::MemoryRequirement::HostVisible));
		const auto&	bufferAlloc	= buffers.back()->getAllocation();
		const auto	bufferPtr	= reinterpret_cast<char*>(bufferAlloc.getHostPtr()) + bufferAlloc.getOffset();
		deMemcpy(bufferPtr, contents, bufferSize);
		vk::flushAlloc(vkd, device, bufferAlloc);
	}

	// Create framebuffer image and view.
	const vk::VkImageCreateInfo				fbImgCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		kImageFormat,								// VkFormat					format
		kFramebufferExtent,							// VkExtent3D				extent
		1u,											// deUint32					mipLevels
		1u,											// deUint32					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |	// VkImageUsageFlags		usage
		 vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		1u,											// deUint32					queueFamilyIndexCount
		&queueIndex,								// const deUint32*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	const vk::ImageWithMemory				fbImage					(vkd, device, allocator, fbImgCreateInfo,	vk::MemoryRequirement::Any);
	const auto								colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto								fbView					= vk::makeImageView(vkd, device, fbImage.get(),		vk::VK_IMAGE_VIEW_TYPE_2D, kImageFormat, colorSubresourceRange);

	// Buffer to copy rendering result to.
	const vk::VkDeviceSize					resultsBufferSize		= static_cast<vk::VkDeviceSize>(static_cast<deUint32>(tcu::getPixelSize(tcuFormat)) * kFramebufferExtent.width * kFramebufferExtent.height * kFramebufferExtent.depth);
	const auto								resultsBufferInfo		= vk::makeBufferCreateInfo(resultsBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const vk::BufferWithMemory				resultsBuffer			(vkd, device, allocator, resultsBufferInfo, vk::MemoryRequirement::HostVisible);

	const std::vector<tcu::Vec4>			fullScreenQuad			=
	{
		{ -1.f, -1.f,	0.f, 1.f },
		{  1.f, -1.f,	0.f, 1.f },
		{ -1.f,  1.f,	0.f, 1.f },
		{ -1.f,  1.f,	0.f, 1.f },
		{  1.f, -1.f,	0.f, 1.f },
		{  1.f,  1.f,	0.f, 1.f }
	};

	// Vertex buffer.
	const vk::VkDeviceSize					vertexBufferSize		= static_cast<vk::VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
	const auto								vertexBufferInfo		= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const vk::BufferWithMemory				vertexBuffer			(vkd, device, allocator, vertexBufferInfo, vk::MemoryRequirement::HostVisible | vk::MemoryRequirement::Coherent);

	// Copy data to vertex buffer.
	const auto&								vertexAlloc				= vertexBuffer.getAllocation();
	const auto								vertexDataPtr			= reinterpret_cast<char*>(vertexAlloc.getHostPtr()) + vertexAlloc.getOffset();
	deMemcpy(vertexDataPtr, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
	vk::flushAlloc(vkd, device, vertexAlloc);

	// Shader modules.
	const auto								vertexModule			= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto								fragModule				= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Render pass.
	const vk::VkAttachmentDescription		fbAttachment			=
	{
		0u,												// VkAttachmentDescriptionFlags		flags
		kImageFormat,									// VkFormat							format
		vk::VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples
		vk::VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp
		vk::VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout
	};

	std::vector<vk::VkAttachmentDescription>	attachmentDescs;
	attachmentDescs.push_back(fbAttachment);

	const vk::VkAttachmentReference			colorRef				=
	{
		0u,												// deUint32			attachment
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout	layout
	};
	const std::vector<vk::VkAttachmentReference> colorAttachments(1u, colorRef);

	const vk::VkSubpassDescription				subpass				=
	{
		0u,												// VkSubpassDescriptionFlags		flags
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint				pipelineBindPoint
		0u,												// deUint32							inputAttachmentCount
		DE_NULL,										// const VkAttachmentReference*		pInputAttachments
		static_cast<deUint32>(colorAttachments.size()),	// deUint32							colorAttachmentCount
		colorAttachments.data(),						// const VkAttachmentReference*		pColorAttachments
		0u,												// const VkAttachmentReference*		pResolveAttachments
		DE_NULL,										// const VkAttachmentReference*		pDepthStencilAttachment
		0u,												// deUint32							preserveAttachmentCount
		DE_NULL											// const deUint32*					pPreserveAttachments
	};
	const std::vector<vk::VkSubpassDescription>	subpasses			(1u, subpass);

	const vk::VkRenderPassCreateInfo renderPassInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,										// const void*						pNext
		0u,												// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescs.size()),	// deUint32							attachmentCount
		attachmentDescs.data(),							// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpasses.size()),		// deUint32							subpassCount
		subpasses.data(),								// const VkSubpassDescription*		pSubpasses
		0u,												// deUint32							dependencyCount
		DE_NULL,										// const VkSubpassDependency*		pDependencies
	};
	const auto									renderPass			= vk::createRenderPass(vkd, device, &renderPassInfo);

	// Framebuffer.
	std::vector<vk::VkImageView>				attachments;
	attachments.push_back(fbView.get());
	const auto									framebuffer			= vk::makeFramebuffer(vkd, device, renderPass.get(), static_cast<deUint32>(attachments.size()), attachments.data(), kFramebufferExtent.width, kFramebufferExtent.height, kFramebufferExtent.depth);

	// Pipeline layout.
	const auto									pipelineLayout		= vk::makePipelineLayout(vkd, device, layout.get());

	// Graphics pipeline.
	const std::vector<vk::VkViewport>			viewports			(1u, vk::makeViewport(kFramebufferExtent));
	const std::vector<vk::VkRect2D>				scissors			(1u, vk::makeRect2D(kFramebufferExtent));

	// Use additive alpha blending to accumulate results from all iterations.
	const vk::VkPipelineColorBlendAttachmentState	colorBlendAttachmentState	=
	{
		VK_TRUE,						// VkBool32					blendEnable
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor			srcColorBlendFactor
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor			dstColorBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor			srcAlphaBlendFactor
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstAlphaBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
		vk::VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
		| vk::VK_COLOR_COMPONENT_G_BIT
		| vk::VK_COLOR_COMPONENT_B_BIT
		| vk::VK_COLOR_COMPONENT_A_BIT
	};

	const vk::VkPipelineColorBlendStateCreateInfo	colorBlendState	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineColorBlendStateCreateFlags			flags
		VK_FALSE,														// VkBool32										logicOpEnable
		vk::VK_LOGIC_OP_CLEAR,											// VkLogicOp									logicOp
		1u,																// deUint32										attachmentCount
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 1.0f, 1.0f, 1.0f, 1.0f }										// float										blendConstants[4]
	};

	const auto									pipeline			= vk::makeGraphicsPipeline(vkd, device, pipelineLayout.get(),
																							   vertexModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
																							   renderPass.get(), viewports, scissors, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
																							   0u, 0u, DE_NULL, DE_NULL, DE_NULL, DE_NULL, &colorBlendState);

	// Command pool and command buffer.
	const auto									cmdPool				= vk::createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
	const auto									cmdBufferPtr		= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto									cmdBuffer			= cmdBufferPtr.get();

	const vk::VkRect2D							renderArea			= vk::makeRect2D(kFramebufferExtent);
	const vk::VkDeviceSize						vertexBufferOffset	= 0ull;

	const auto									vtxBufferBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, vertexBuffer.get(), 0ull, vertexBufferSize);
	const auto									fbBarrier			= vk::makeImageMemoryBarrier(0u, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbImage.get(), colorSubresourceRange);

	vk::VkClearValue							clearValue;
	clearValue.color.float32[0] = 0.0f;
	clearValue.color.float32[1] = 0.0f;
	clearValue.color.float32[2] = 0.0f;
	clearValue.color.float32[3] = 1.0f;

	const vk::VkClearAttachment					clearAttachment		=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,								// uint32_t				colorAttachment
		clearValue						// VkClearValue			clearValue
	};

	const vk::VkClearRect						clearRect			=
	{
		vk::makeRect2D(kFramebufferExtent),	// VkRect2D	rect
		0u,									// uint32_t	baseArrayLayer
		1u									// uint32_t	layerCount
	};

	vk::beginCommandBuffer(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0u,
						   0u, DE_NULL, 1u, &vtxBufferBarrier, 0u, DE_NULL);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
						   0u, DE_NULL, 0u, DE_NULL, 1u, &fbBarrier);
	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	struct DescriptorWrite
	{
		deUint32			bufferId;	// Which buffer to use for the descriptor update.
		vk::VkDeviceSize	offset;		// The offset for the descriptor update.
		vk::VkDeviceSize	range;		// The range for the descriptor update.
	};

	// Each iteration operates on a descriptor mutation which decides the source of the descriptor update.
	struct DescriptorMutation
	{
		deBool							update;		// Determines if a descriptor update is performed.
		deUint32						numDraws;	// The number of consecutive draw calls in a loop.
		std::vector<DescriptorWrite>	writes;		// Multiple redundant writes can be performed.
		// Other ideas to implement:
		// - Sometimes also update the buffer contents.
		// - Multiple descriptor sets.
	};

	std::vector<DescriptorMutation>				descriptorMutations;

	// Keep track of the expected result while generating the mutations.
	tcu::Vec4									uboValue0;
	tcu::Vec4									uboValue1;
	tcu::Vec4									expectedColor		(0.0f, 0.0f, 0.0f, 1.0f);
	DescriptorWrite								descWrite			= { 0u, 0u, 32u };

	for (deUint32 i = 0; i < kNumIterations; i++)
	{
		while (true)
		{
			tcu::Vec4						val0		= uboValue0;
			tcu::Vec4						val1		= uboValue1;

			deUint32						numWrites	= 1u;

			// Sometimes do redundant descriptor writes.
			if (deRandom_getUint32(&m_random) % 10 == 0)
				numWrites = deRandom_getUint32(&m_random) % 20 + 1;

			std::vector<DescriptorWrite>	writes;

			for (deUint32 w = 0; w < numWrites; w++)
			{
				// The first half: Most of the times change the offset but sometimes the buffer.
				// The second half: Most of the times change the buffer but sometimes change the offset.
				bool	firstHalf	= i < kNumIterations / 2;
				bool	rare		= (deRandom_getUint32(&m_random) % 100u >= (firstHalf ? 98u : 80u));

				// firstHalf     rare      change
				// --------------------------------
				//     1          0        Offset
				//     1          1        Buffer
				//     0          0        Buffer
				//     0          1        Offset
				//
				// This has a XOR pattern

				if (firstHalf ^ rare)
					descWrite.offset = (deRandom_getUint32(&m_random) % kNumOffsets) * 256u;
				else
					descWrite.bufferId = deRandom_getUint32(&m_random) % kNumBuffers;

				writes.push_back(descWrite);
			}

			DescriptorMutation				mutation	= {i == 0 ? true : deRandom_getBool(&m_random),
										deRandom_getUint32(&m_random) % 10u, writes};

			const auto&						lastWrite	= mutation.writes.back();
			if (mutation.update)
			{
				for (int c = 0; c < 3; c++)
				{
					val0[c] = bufferContents[lastWrite.bufferId][lastWrite.offset / 4 + c];
					val1[c] = bufferContents[lastWrite.bufferId][lastWrite.offset / 4 + 4 + c];

					// Sanity check we are reading expected values.
					DE_ASSERT(val0[c] >= -counter && val0[c] <= counter);
					DE_ASSERT(val1[c] >= -counter && val1[c] <= counter);
				}
			}

			tcu::Vec4						color		= expectedColor + (val0 + val1) * tcu::Vec4(static_cast<float>(mutation.numDraws));

			// 16-bit float can precisely present integers from -2048..2048. Continue randomizing the mutation
			// until we stay in this range.
			if (color[0] >= -2048.0f && color[0] <= 2048.0f && color[1] >= -2048.0f && color[1] <= 2048.0f
				&& color[2] >= -2048.0f && color[2] <= 2048.0f)
			{
				descriptorMutations.push_back(mutation);
				uboValue0		= val0;
				uboValue1		= val1;
				expectedColor	= color;
				break;
			}
			else
			{
				// Randomize both buffer and offset for a better chance to hit a
				// mutation that pushes the values back to the desired range.
				descWrite.offset = (deRandom_getUint32(&m_random) % kNumOffsets) * 256u;
				descWrite.bufferId = deRandom_getUint32(&m_random) % kNumBuffers;
			}
		}
	}

	bool first = true;

	for (auto mutation : descriptorMutations)
	{
		if (mutation.update)
		{
			for (const auto &write : mutation.writes)
			{
				const vk::VkDescriptorBufferInfo descriptorInfo =
				{
					buffers[write.bufferId]->get(),	// VkBuffer		buffer
					write.offset,					// VkDeviceSize	offset
					write.range						// VkDeviceSize	range
				};

				const vk::VkWriteDescriptorSet descriptorWrite =
				{
					vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureTypes					Type
					DE_NULL,									// const void*						pNext
					*descriptorSet,								// VkDescriptorSet					dstSet
					0,											// deUint32							dstBinding
					0,											// deUint32							dstArrayElement
					1u,											// deUint32							descriptorCount
					vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// VkDescriptorType					descriptorType
					DE_NULL,									// const VkDescriptorImageInfo*		pImageInfo
					&descriptorInfo,							// const VkDescriptorBufferInfo*	pBufferInfo
					DE_NULL										// const VkBufferView*				pTexelBufferView
				};

				vkd.updateDescriptorSets(device, 1, &descriptorWrite, 0, DE_NULL);
			}
		}

		vk::beginCommandBuffer(vkd, cmdBuffer);

		vk::beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea);
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		// Clear the frame buffer during the first iteration.
		if (first)
		{
			vkd.cmdClearAttachments(cmdBuffer, 1u, &clearAttachment, 1u, &clearRect);
			first = false;
		}
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
								  &descriptorSet.get(), 0u, nullptr);
		vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		for (deUint32 i = 0u; i < mutation.numDraws; i++)
			vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(fullScreenQuad.size()), 1u, 0u, 0u);

		vk::endRenderPass(vkd, cmdBuffer);
		vk::endCommandBuffer(vkd, cmdBuffer);
		vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);
	}

	vk::beginCommandBuffer(vkd, cmdBuffer);
	const tcu::IVec2 copySize{static_cast<int>(kFramebufferExtent.width),
							  static_cast<int>(kFramebufferExtent.height)};
	vk::copyImageToBuffer(vkd, cmdBuffer, fbImage.get(), resultsBuffer.get(), copySize);
	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check results.
	const auto& resultsBufferAlloc = resultsBuffer.getAllocation();
	vk::invalidateAlloc(vkd, device, resultsBufferAlloc);

	const auto							resultsBufferPtr	= reinterpret_cast<const char*>(resultsBufferAlloc.getHostPtr()) + resultsBufferAlloc.getOffset();
	const tcu::ConstPixelBufferAccess	resultPixels		{tcuFormat, copySize[0], copySize[1], 1, resultsBufferPtr};

	// The test only operates on integers, so a tolerance of 0.5 should work.
	const float							tolerance			= 0.5f;

	bool pass = true;
	for (int x = 0; pass && x < resultPixels.getWidth(); ++x)
		for (int y = 0; pass && y < resultPixels.getHeight(); ++y)
			for (int z = 0; pass && z < resultPixels.getDepth(); ++z)
			{
				const auto pixel = resultPixels.getPixel(x, y, z);
				for (int c = 0; c < 3; c++)
					if (fabs(pixel[c] - expectedColor[c]) > tolerance)
						pass = false;
			}

	tcu::TestStatus status = tcu::TestStatus::pass("Pass");
	if (!pass)
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Image("color", "Rendered image", resultPixels);
		status = tcu::TestStatus::fail("Pixel mismatch; please check the rendered image");
	}

	return status;
}

tcu::TestCaseGroup* createRandomDescriptorUpdateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "random", "Update descriptors randomly between draws"));

	group->addChild(new RandomDescriptorUpdateTestCase(testCtx, "uniform_buffer", ""));
	return group.release();
}

} // anonymous

tcu::TestCaseGroup* createDescriptorUpdateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "descriptor_update", "Update descriptor sets"));

	group->addChild(createEmptyDescriptorUpdateTests(testCtx));
	group->addChild(createSamplerlessWriteTests(testCtx));
	group->addChild(createRandomDescriptorUpdateTests(testCtx));

	return group.release();
}

} // BindingModel
} // vkt
