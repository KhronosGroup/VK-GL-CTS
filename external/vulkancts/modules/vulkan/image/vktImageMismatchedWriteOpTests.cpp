/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Image OpImageWrite tests.
 *//*--------------------------------------------------------------------*/

#include "vktImageMismatchedWriteOpTests.hpp"

#include "vktImageTexture.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"

#include <set>

#define EPSILON_COMPARE(a,b,e)	((de::max((a),(b))-de::min((a),(b)))<=(e))

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

using tcu::TextureFormat;
using tcu::StringTemplate;
using tcu::TextureChannelClass;
using strings = std::map<std::string, std::string>;

class MismatchedWriteOpTest : public TestCase
{
public:
	struct Params
	{
		VkFormat			vkFormat;
		int					textureWidth;
		int					textureHeight;
		VkFormat			spirvFormat;
	};
	typedef de::SharedPtr<Params> ParamsSp;

										MismatchedWriteOpTest		(tcu::TestContext&			testCtx,
																	 const std::string&			name,
																	 const std::string&			description,
																	 const ParamsSp				params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
	{
	}

	virtual void						checkSupport				(Context&					context)		const override;
	virtual TextureFormat				getBufferFormat				(void)										const;
	void								getProgramCodeAndVariables	(StringTemplate&			code,
																	 strings&					variables)		const;

	template<class TestParams> void getParams(TestParams&);

protected:
	const ParamsSp	m_params;
};

class MismatchedVectorSizesTest : public MismatchedWriteOpTest
{
public:
										MismatchedVectorSizesTest	(tcu::TestContext&			testCtx,
																	 const std::string&			name,
																	 const std::string&			description,
																	 const ParamsSp				params,
																	 const int					sourceWidth)
		: MismatchedWriteOpTest	(testCtx, name, description, params)
		, m_sourceWidth			(sourceWidth)
	{
		DE_ASSERT(getNumUsedChannels(params->vkFormat) <= sourceWidth);
	}

	virtual void						initPrograms				(SourceCollections&			programCollection)	const override;
	virtual TestInstance*				createInstance				(Context&					context)			const override;

private:
	const int	m_sourceWidth;
};

class MismatchedSignednessAndTypeTest : public MismatchedWriteOpTest
{
public:
									MismatchedSignednessAndTypeTest	(tcu::TestContext&			testCtx,
																	 const std::string&			name,
																	 const std::string&			description,
																	 const ParamsSp				params)
		: MismatchedWriteOpTest	(testCtx, name, description, params)
	{
	}

	virtual void						initPrograms				(SourceCollections&			programCollection)	const override;
	virtual TestInstance*				createInstance				(Context&					context)			const override;
};

class MismatchedWriteOpTestInstance : public TestInstance
{
public:
	using TestClass	= MismatchedWriteOpTest;
	using ParamsSp	= MismatchedWriteOpTest::ParamsSp;

							MismatchedWriteOpTestInstance			(Context&					context,
																	 const ParamsSp				params,
																	 const TestClass*			test)
		: TestInstance	(context)
		, m_params		(params)
		, m_test		(test)
	{
	}

	virtual tcu::TestStatus	iterate									(void)											override;
	virtual void			clear									(tcu::PixelBufferAccess&	data)				const;
	virtual void			populate								(tcu::PixelBufferAccess&	data)				const;
	virtual bool			compare									(tcu::PixelBufferAccess&	result,
																	 tcu::PixelBufferAccess&	reference)			const = 0;
protected:
	const ParamsSp		m_params;
	const TestClass*	m_test;
};

class MismatchedVectorSizesTestInstance : public MismatchedWriteOpTestInstance
{
public:
							MismatchedVectorSizesTestInstance		(Context&					context,
																	 const ParamsSp				params,
																	 const TestClass*			test)
		: MismatchedWriteOpTestInstance	(context, params, test)
	{
	}

	bool					compare									(tcu::PixelBufferAccess&	result,
																	 tcu::PixelBufferAccess&	reference)			const override;
};

class MismatchedSignednessAndTypeTestInstance : public MismatchedWriteOpTestInstance
{
public:
							MismatchedSignednessAndTypeTestInstance	(Context&					context,
																	 const ParamsSp				params,
																	 const TestClass*			test)
		: MismatchedWriteOpTestInstance	(context, params, test)
	{
	}

	bool					compare									(tcu::PixelBufferAccess&	result,
																	tcu::PixelBufferAccess&		reference)			const override;
};

namespace ut
{

class StorageBuffer2D
{
public:
									StorageBuffer2D		(Context&					context,
														 const tcu::TextureFormat&	format,
														 deUint32					width,
														 deUint32					height);

	VkBuffer						getBuffer			(void) const { return *m_buffer; }
	VkDeviceSize					getSize				(void) const { return m_bufferSize; }

	tcu::PixelBufferAccess&			getPixelAccess		(void) { return m_access[0]; }

	void							flush				(void) { flushAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory); }
	void							invalidate			(void) { invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory); }

protected:
	friend class StorageImage2D;
	Allocation&						getMemory			(void) const { return *m_bufferMemory; }

private:
	Context&							m_context;
	const tcu::TextureFormat			m_format;
	const deUint32						m_width;
	const deUint32						m_height;
	const VkDeviceSize					m_bufferSize;
	Move<VkBuffer>						m_buffer;
	de::MovePtr<Allocation>				m_bufferMemory;
	std::vector<tcu::PixelBufferAccess>	m_access;
};

class StorageImage2D
{
public:
									StorageImage2D	(Context&				context,
													 VkFormat				vkFormat,
													 const int				width,
													 const int				height,
													 bool					sparse = false);

	VkImageView						getView			(void) const	{ return *m_view; }

	tcu::PixelBufferAccess&			getPixelAccess	(void)			{ return m_buffer.getPixelAccess(); }

	void							flush			(void) { m_buffer.flush(); }
	void							invalidate		(void) { m_buffer.invalidate(); }

	void							upload			(const VkCommandBuffer	cmdBuffer);
	void							download		(const VkCommandBuffer	cmdBuffer);

private:
	Context&								m_context;
	const bool								m_sparse;
	const int								m_width;
	const int								m_height;
	const vk::VkFormat						m_vkFormat;
	const tcu::TextureFormat				m_texFormat;
	StorageBuffer2D							m_buffer;

	VkImageLayout							m_layout;
	Move<VkImage>							m_image;
	Move<VkImageView>						m_view;
	Move<VkSemaphore>						m_semaphore;
	std::vector<de::SharedPtr<Allocation>>	m_allocations;
	de::MovePtr<Allocation>					m_imageMemory;
};

StorageImage2D::StorageImage2D (Context& context, VkFormat vkFormat, const int width, const int height, bool sparse)
	: m_context		(context)
	, m_sparse		(sparse)
	, m_width		(width)
	, m_height		(height)
	, m_vkFormat	(vkFormat)
	, m_texFormat	(mapVkFormat(m_vkFormat))
	, m_buffer		(m_context, m_texFormat, m_width, m_height)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	// Create an image
	{
		VkImageCreateFlags				imageCreateFlags	= m_sparse? (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) : 0u;
		VkImageUsageFlags				imageUsageFlags		= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		const VkImageCreateInfo			imageCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			imageCreateFlags,								// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_vkFormat,										// VkFormat					format;
			{ deUint32(m_width), deUint32(m_height), 1u },	// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			imageUsageFlags,								// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,								// const deUint32*			pQueueFamilyIndices;
			(m_layout = VK_IMAGE_LAYOUT_UNDEFINED)			// VkImageLayout			initialLayout;
		};

		m_image		= createImage(vki, dev, &imageCreateInfo);

		if (m_sparse)
		{
			m_semaphore = createSemaphore(vki, dev);

			allocateAndBindSparseImage(	vki, dev, m_context.getPhysicalDevice(), m_context.getInstanceInterface(),
										imageCreateInfo, *m_semaphore, m_context.getSparseQueue(),
										allocator, m_allocations, mapVkFormat(m_vkFormat), *m_image	);
		}
		else
		{
			m_imageMemory = allocator.allocate(getImageMemoryRequirements(vki, dev, *m_image), MemoryRequirement::Any);
			VK_CHECK(vki.bindImageMemory(dev, *m_image, m_imageMemory->getMemory(), m_imageMemory->getOffset()));
		}

		VkImageSubresourceRange			subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		m_view	= makeImageView(vki, dev, *m_image, VK_IMAGE_VIEW_TYPE_2D, m_vkFormat, subresourceRange);
	}
}

void StorageImage2D::upload (const VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&			vki							= m_context.getDeviceInterface();
	const VkImageSubresourceRange	fullImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkBufferImageCopy			copyRegion					= makeBufferImageCopy(makeExtent3D(tcu::IVec3(m_width, m_height, 1)), 1u);

	{
		const VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			m_buffer.getBuffer(), 0ull, m_buffer.getSize());

		const VkImageMemoryBarrier beforeCopyBarrier = makeImageMemoryBarrier(
			(VkAccessFlagBits)0, VK_ACCESS_TRANSFER_WRITE_BIT,
			m_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			*m_image, fullImageSubresourceRange);

		vki.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
							   0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &beforeCopyBarrier);
	}

	vki.cmdCopyBufferToImage(cmdBuffer, m_buffer.getBuffer(), *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

	{
		const VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_TRANSFER_READ_BIT, (VkAccessFlags)0,
			m_buffer.getBuffer(), 0ull, m_buffer.getSize());

		m_layout = VK_IMAGE_LAYOUT_GENERAL;
		const VkImageMemoryBarrier afterCopyBarrier = makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_layout,
			*m_image, fullImageSubresourceRange);

		vki.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0,
							   0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &afterCopyBarrier);
	}
}

void StorageImage2D::download (const VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&			vki							= m_context.getDeviceInterface();
	const VkImageSubresourceRange	fullImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkBufferImageCopy			copyRegion					= makeBufferImageCopy(makeExtent3D(tcu::IVec3(m_width, m_height, 1)), 1u);

	{
		const VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
			(VkAccessFlags)0, VK_ACCESS_TRANSFER_WRITE_BIT,
			m_buffer.getBuffer(), 0ull, m_buffer.getSize());

		const VkImageMemoryBarrier beforeCopyBarrier = makeImageMemoryBarrier(
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			m_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			*m_image, fullImageSubresourceRange);

		vki.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
							   0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &beforeCopyBarrier);
	}

	vki.cmdCopyImageToBuffer(cmdBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer.getBuffer(), 1, &copyRegion);

	{
		const VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
			m_buffer.getBuffer(), 0ull, m_buffer.getSize());

		const VkImageMemoryBarrier afterCopyBarrier = makeImageMemoryBarrier(
			VK_ACCESS_TRANSFER_READ_BIT, (VkAccessFlags)0,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_layout,
			*m_image, fullImageSubresourceRange);

		vki.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
							   0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 1, &afterCopyBarrier);
	}

}

StorageBuffer2D::StorageBuffer2D (Context& context, const tcu::TextureFormat& format, deUint32 width, deUint32 height)
	: m_context		(context)
	, m_format		(format)
	, m_width		(width)
	, m_height		(height)
	, m_bufferSize	(m_width * m_height * m_format.getPixelSize())
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const VkBufferUsageFlags		bufferUsageFlags	= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	const VkBufferCreateInfo		bufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkBufferCreateFlags		flags;
		m_bufferSize,								// VkDeviceSize				size;
		bufferUsageFlags,							// VkBufferUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex							// const deUint32*			pQueueFamilyIndices;
	};

	m_buffer		= createBuffer(vki, dev, &bufferCreateInfo);

	m_bufferMemory	= allocator.allocate(getBufferMemoryRequirements(vki, dev, *m_buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vki.bindBufferMemory(dev, *m_buffer, m_bufferMemory->getMemory(), m_bufferMemory->getOffset()));

	m_access.emplace_back(m_format, tcu::IVec3(m_width, m_height, 1), m_bufferMemory->getHostPtr());
}

tcu::Vec4 gluePixels (const tcu::Vec4& a, const tcu::Vec4& b, const int pivot)
{
	tcu::Vec4 result;
	for (int i = 0; i < pivot; ++i) result[i] = a[i];
	for (int i = pivot; i < 4; ++i) result[i] = b[i];
	return result;
}

template<class T, int N>
bool comparePixels (const tcu::Vector<T,N>& res, const tcu::Vector<T,N>& ref, const int targetWidth, const T eps = {})
{
	bool		ok		= true;

	for (int i = 0; ok && i < targetWidth; ++i)
	{
		ok &= EPSILON_COMPARE(res[i], ref[i], eps);
	}

	return ok;
}

} // ut

TestInstance* MismatchedVectorSizesTest::createInstance (Context& context) const
{
	return (new MismatchedVectorSizesTestInstance(context, m_params, this));
}

TestInstance* MismatchedSignednessAndTypeTest::createInstance (Context& context) const
{
	return (new MismatchedSignednessAndTypeTestInstance(context, m_params, this));
}

const VkFormat allFormats[] =
{
	VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_FORMAT_R16G16_SFLOAT,
	VK_FORMAT_R16_SFLOAT,
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R16G16_UNORM,
	VK_FORMAT_R16_UNORM,
	VK_FORMAT_R16G16B16A16_SNORM,
	VK_FORMAT_R16G16_SNORM,
	VK_FORMAT_R16_SNORM,
	VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	VK_FORMAT_B10G11R11_UFLOAT_PACK32,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8_UNORM,
	VK_FORMAT_R8_UNORM,
	VK_FORMAT_R8G8B8A8_SNORM,
	VK_FORMAT_R8G8_SNORM,
	VK_FORMAT_R8_SNORM,
	VK_FORMAT_R32G32B32A32_SINT,
	VK_FORMAT_R32G32_SINT,
	VK_FORMAT_R32_SINT,
	VK_FORMAT_R16G16B16A16_SINT,
	VK_FORMAT_R16G16_SINT,
	VK_FORMAT_R16_SINT,
	VK_FORMAT_R8G8B8A8_SINT,
	VK_FORMAT_R8G8_SINT,
	VK_FORMAT_R8_SINT,
	VK_FORMAT_R32G32B32A32_UINT,
	VK_FORMAT_R32G32_UINT,
	VK_FORMAT_R32_UINT,
	VK_FORMAT_R16G16B16A16_UINT,
	VK_FORMAT_R16G16_UINT,
	VK_FORMAT_R16_UINT,
	VK_FORMAT_A2B10G10R10_UINT_PACK32,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R8G8_UINT,
	VK_FORMAT_R8_UINT,

	VK_FORMAT_R64_SINT,
	VK_FORMAT_R64_UINT,
};

std::vector<VkFormat> findFormatsByChannelClass(TextureChannelClass channelClass)
{
	std::vector<VkFormat> result;
	for (const VkFormat& f : allFormats)
	{
		if (getTextureChannelClass(mapVkFormat(f).type) == channelClass)
			result.emplace_back(f);
	}
	DE_ASSERT(!result.empty());
	return result;
}

const char* getChannelStr (const TextureFormat::ChannelType& type)
{
	switch (type)
	{
		case TextureFormat::FLOAT:				return "float";
		case TextureFormat::SIGNED_INT32:		return "sint";
		case TextureFormat::UNSIGNED_INT32:		return "uint";
		case TextureFormat::FLOAT64:			return "double";
		case TextureFormat::SIGNED_INT64:		return "slong";
		case TextureFormat::UNSIGNED_INT64:		return "ulong";
		default:								DE_ASSERT(DE_FALSE);
	}

	return nullptr;
}

TextureFormat::ChannelType makeChannelType (tcu::TextureChannelClass channelClass, bool doubled)
{
	auto	channelType	= TextureFormat::ChannelType::CHANNELTYPE_LAST;
	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			channelType	= doubled ? TextureFormat::ChannelType::SIGNED_INT64 : TextureFormat::ChannelType::SIGNED_INT32;
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			channelType	= doubled ? TextureFormat::ChannelType::UNSIGNED_INT64 : TextureFormat::ChannelType::UNSIGNED_INT32;
			break;
		default:
			channelType	= doubled ? TextureFormat::ChannelType::FLOAT64 : TextureFormat::ChannelType::FLOAT;
	}
	return channelType;
}

TextureFormat makeBufferFormat (tcu::TextureChannelClass channelClass, bool doubled)
{
	return TextureFormat(TextureFormat::ChannelOrder::RGBA, makeChannelType(channelClass, doubled));
}

void MismatchedWriteOpTest::checkSupport (Context& context) const
{
	// capabilities that may be used in the shader
	if (is64BitIntegerFormat(m_params->vkFormat))
	{
		const VkPhysicalDeviceFeatures deviceFeatures = getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());
		if(!deviceFeatures.shaderInt64)
		{
			TCU_THROW(NotSupportedError, "Device feature shaderInt64 is not supported");
		}
		context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");
	}

	// extensions used statically in the shader
	context.requireDeviceFunctionality("VK_KHR_variable_pointers");
	context.requireDeviceFunctionality("VK_KHR_storage_buffer_storage_class");

	VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), m_params->vkFormat);

	if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
	{
		TCU_THROW(NotSupportedError, "Creating storage image with this format is not supported");
	}
}

TextureFormat MismatchedWriteOpTest::getBufferFormat (void) const
{
	const TextureFormat texFormat	= mapVkFormat(m_params->vkFormat);
	return makeBufferFormat(getTextureChannelClass(texFormat.type), is64BitIntegerFormat(m_params->vkFormat));
}

void MismatchedWriteOpTest::getProgramCodeAndVariables (StringTemplate& code, strings& variables) const
{
	std::string shaderTemplate(R"(

							  OpCapability Shader
							  OpCapability StorageImageExtendedFormats

							  ${CAPABILITY_INT64}
							  OpExtension      "SPV_KHR_variable_pointers"
							  OpExtension      "SPV_KHR_storage_buffer_storage_class"
							  ${EXTENSIONS}

					%std450 = OpExtInstImport  "GLSL.std.450"
							  OpMemoryModel    Logical GLSL450

							  OpEntryPoint     GLCompute %main "main" %gid %image %buffer
							  OpExecutionMode  %main LocalSize 1 1 1

							  OpDecorate       %gid BuiltIn GlobalInvocationId

							  OpDecorate       %image DescriptorSet 0
							  OpDecorate       %image Binding 0

							  OpDecorate       %rta    ArrayStride ${ARRAY_STRIDE}
							  OpMemberDecorate %struct 0 Offset 0
							  OpDecorate       %struct Block
							  OpDecorate       %buffer DescriptorSet 0
							  OpDecorate       %buffer Binding 1

					  %void = OpTypeVoid
				   %fn_void = OpTypeFunction %void

					${TYPES_INT64}

					 %float = OpTypeFloat 32
					  %sint = OpTypeInt 32 1
					  %uint = OpTypeInt 32 0

				   %v4float = OpTypeVector %float 4
				   %v3float = OpTypeVector %float 3
				   %v2float = OpTypeVector %float 2

					%v4sint = OpTypeVector %sint 4
					%v3sint = OpTypeVector %sint 3
					%v2sint = OpTypeVector %sint 2

					%v4uint = OpTypeVector %uint 4
					%v3uint = OpTypeVector %uint 3
					%v2uint = OpTypeVector %uint 2

			 %v3uint_in_ptr = OpTypePointer Input %v3uint
					   %gid = OpVariable %v3uint_in_ptr Input

				%image_type = OpTypeImage %${SAMPLED_TYPE} 2D 0 0 0 2 ${SPIRV_IMAGE_FORMAT}
				 %image_ptr = OpTypePointer UniformConstant %image_type
					 %image = OpVariable %image_ptr UniformConstant

			   %image_width = OpConstant %sint ${IMAGE_WIDTH}
			  %image_height = OpConstant %sint ${IMAGE_HEIGHT}

				%rta_offset = OpConstant %uint 0
					   %rta = OpTypeRuntimeArray %v4${SAMPLED_TYPE}
					%struct = OpTypeStruct %rta
				  %ssbo_ptr = OpTypePointer StorageBuffer %struct
					%buffer = OpVariable %ssbo_ptr StorageBuffer

				%red_offset = OpConstant %uint 0
			  %green_offset = OpConstant %uint 1
			   %blue_offset = OpConstant %uint 2
			  %alpha_offset = OpConstant %uint 3

	   %${SAMPLED_TYPE}_PTR = OpTypePointer StorageBuffer %${SAMPLED_TYPE}
			  %var_sint_ptr = OpTypePointer Function %sint

				; Entry main procedure
					  %main = OpFunction %void None %fn_void
					 %entry = OpLabel

					 %index = OpVariable %var_sint_ptr Function

				; Transform gl_GlobalInvocationID.xyz to ivec2(gl_GlobalInvocationID.xy)
						%id = OpLoad %v3uint %gid

					%u_id_x = OpCompositeExtract %uint %id 0
					%s_id_x = OpBitcast %sint %u_id_x

					%u_id_y = OpCompositeExtract %uint %id 1
					%s_id_y = OpBitcast %sint %u_id_y

					 %id_xy = OpCompositeConstruct %v2sint %s_id_x %s_id_y

				; Calculate index in buffer
					   %mul = OpIMul %sint %s_id_y %image_width
					   %add = OpIAdd %sint %mul %s_id_x
							  OpStore %index %add

				; Final image variable used to read from or write to
					   %img = OpLoad %image_type %image

				; Accessors to buffer components
					   %idx = OpLoad %sint %index
			  %alpha_access = OpAccessChain %${SAMPLED_TYPE}_PTR %buffer %rta_offset %idx %alpha_offset
			   %blue_access = OpAccessChain %${SAMPLED_TYPE}_PTR %buffer %rta_offset %idx %blue_offset
			  %green_access = OpAccessChain %${SAMPLED_TYPE}_PTR %buffer %rta_offset %idx %green_offset
				%red_access = OpAccessChain %${SAMPLED_TYPE}_PTR %buffer %rta_offset %idx %red_offset

					   %red = OpLoad %${SAMPLED_TYPE} %red_access
					 %green = OpLoad %${SAMPLED_TYPE} %green_access
					  %blue = OpLoad %${SAMPLED_TYPE} %blue_access
					 %alpha = OpLoad %${SAMPLED_TYPE} %alpha_access

							  ${WRITE_TO_IMAGE}

							  OpReturn
							  OpFunctionEnd
	)");

	const std::string typesInt64(R"(
					 %slong = OpTypeInt 64 1
					 %ulong = OpTypeInt 64 0

				   %v4slong = OpTypeVector %slong 4
				   %v3slong = OpTypeVector %slong 3
				   %v2slong = OpTypeVector %slong 2

				   %v4ulong = OpTypeVector %ulong 4
				   %v3ulong = OpTypeVector %ulong 3
				   %v2ulong = OpTypeVector %ulong 2
	)");

	const tcu::TextureFormat			buffFormat	= getBufferFormat();

	variables["SPIRV_IMAGE_FORMAT"]					= getSpirvFormat(m_params->vkFormat);
	variables["CAPABILITY_INT64"]					= "";
	variables["EXTENSIONS"]							= "";
	variables["TYPES_INT64"]						= "";

	if (is64BitIntegerFormat(m_params->vkFormat))
	{
		variables["EXTENSIONS"]						= "OpExtension	   \"SPV_EXT_shader_image_int64\"";
		variables["CAPABILITY_INT64"]				=	"OpCapability Int64ImageEXT\n"
														"OpCapability Int64";
		variables["TYPES_INT64"]					= typesInt64;
	}

	variables["SAMPLED_TYPE"]						= getChannelStr(buffFormat.type);
	variables["IMAGE_WIDTH"]						= std::to_string(m_params->textureWidth);
	variables["IMAGE_HEIGHT"]						= std::to_string(m_params->textureHeight);
	variables["ARRAY_STRIDE"]						= std::to_string(tcu::getChannelSize(buffFormat.type) * tcu::getNumUsedChannels(buffFormat.order));

	code.setString(shaderTemplate);
}

void MismatchedVectorSizesTest::initPrograms (SourceCollections& programCollection) const
{
	strings				variables		{};
	StringTemplate		shaderTemplate	{};

	const StringTemplate writeFromSingleComponent	(R"(
					 OpImageWrite %img %id_xy %red
	)");
	const StringTemplate writeFromTwoComponents		(R"(
			   %rg = OpCompositeConstruct %v2${SAMPLED_TYPE} %red %green
					 OpImageWrite %img %id_xy %rg
	)");

	const StringTemplate writeFromThreeComponents	(R"(
			  %rgb = OpCompositeConstruct %v3${SAMPLED_TYPE} %red %green %blue
					 OpImageWrite %img %id_xy %rgb
	)");
	const StringTemplate writeFromFourComponents	(R"(
			 %rgba = OpCompositeConstruct %v4${SAMPLED_TYPE} %red %green %blue %alpha
					 OpImageWrite %img %id_xy %rgba
	)");

	getProgramCodeAndVariables(shaderTemplate, variables);

	variables["WRITE_TO_IMAGE"]						= (m_sourceWidth == 1
													   ? writeFromSingleComponent
													   : m_sourceWidth == 2
														 ? writeFromTwoComponents
														 : m_sourceWidth == 3
														   ? writeFromThreeComponents
														   : writeFromFourComponents).specialize(variables);
	programCollection.spirvAsmSources.add("comp")
			<< shaderTemplate.specialize(variables)
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

void MismatchedSignednessAndTypeTest::initPrograms (SourceCollections& programCollection) const
{
	strings				variables		{};
	StringTemplate		shaderTemplate	{};

	const StringTemplate writeToImage	(R"(
			%color = OpCompositeConstruct %v4${SAMPLED_TYPE} %red %green %blue %alpha
					 OpImageWrite %img %id_xy %color
	)");

	getProgramCodeAndVariables(shaderTemplate, variables);

	variables["WRITE_TO_IMAGE"]			= writeToImage.specialize(variables);

	programCollection.spirvAsmSources.add("comp")
			<< shaderTemplate.specialize(variables)
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

void MismatchedWriteOpTestInstance::clear (tcu::PixelBufferAccess& pixels) const
{
	const auto channelClass = tcu::getTextureChannelClass(mapVkFormat(m_params->vkFormat).type);
	switch (channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			tcu::clear(pixels, tcu::IVec4(-1, -2, -3, -4));
			break;

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			tcu::clear(pixels, tcu::UVec4(1, 2, 3, 4));
			break;

		default:
			tcu::clear(pixels, tcu::Vec4(0.2f, 0.3f, 0.4f, 0.5f));
	}
}

void MismatchedWriteOpTestInstance::populate (tcu::PixelBufferAccess& pixels) const
{
	const auto				texFormat			= mapVkFormat(m_params->vkFormat);
	const auto				bitDepth			= tcu::getTextureFormatBitDepth(texFormat);
	const auto				channelClass		= tcu::getTextureChannelClass(texFormat.type);

	const tcu::IVec4		signedMinValues		(bitDepth[0] ? deIntMinValue32(deMin32(bitDepth[0], 32)) : (-1),
												 bitDepth[1] ? deIntMinValue32(deMin32(bitDepth[1], 32)) : (-1),
												 bitDepth[2] ? deIntMinValue32(deMin32(bitDepth[2], 32)) : (-1),
												 bitDepth[3] ? deIntMinValue32(deMin32(bitDepth[3], 32)) : (-1));

	const tcu::IVec4		signedMaxValues		(bitDepth[0] ? deIntMaxValue32(deMin32(bitDepth[0], 32)) : 1,
												 bitDepth[1] ? deIntMaxValue32(deMin32(bitDepth[1], 32)) : 1,
												 bitDepth[2] ? deIntMaxValue32(deMin32(bitDepth[2], 32)) : 1,
												 bitDepth[3] ? deIntMaxValue32(deMin32(bitDepth[3], 32)) : 1);

	const tcu::UVec4		unsignedMinValues	(0u);

	const tcu::UVec4		unsignedMaxValues	(bitDepth[0] ? deUintMaxValue32(deMin32(bitDepth[0], 32)) : 1u,
												 bitDepth[1] ? deUintMaxValue32(deMin32(bitDepth[1], 32)) : 1u,
												 bitDepth[2] ? deUintMaxValue32(deMin32(bitDepth[2], 32)) : 1u,
												 bitDepth[3] ? deUintMaxValue32(deMin32(bitDepth[3], 32)) : 1u);

	auto					nextSigned			= [&](tcu::IVec4& color)
	{
		color[0] = (static_cast<deInt64>(color[0] + 2) < signedMaxValues[0]) ? (color[0] + 2) : signedMinValues[0];
		color[1] = (static_cast<deInt64>(color[1] + 3) < signedMaxValues[1]) ? (color[1] + 3) : signedMinValues[1];
		color[2] = (static_cast<deInt64>(color[2] + 5) < signedMaxValues[2]) ? (color[2] + 5) : signedMinValues[2];
		color[3] = (static_cast<deInt64>(color[3] + 7) < signedMaxValues[3]) ? (color[3] + 7) : signedMinValues[3];
	};

	auto					nextUnsigned		= [&](tcu::UVec4& color)
	{
		color[0] = (static_cast<deUint64>(color[0] + 2) < unsignedMaxValues[0]) ? (color[0] + 2) : unsignedMinValues[0];
		color[1] = (static_cast<deUint64>(color[1] + 3) < unsignedMaxValues[1]) ? (color[1] + 3) : unsignedMinValues[1];
		color[2] = (static_cast<deUint64>(color[2] + 5) < unsignedMaxValues[2]) ? (color[2] + 5) : unsignedMinValues[2];
		color[3] = (static_cast<deUint64>(color[3] + 7) < unsignedMaxValues[3]) ? (color[3] + 7) : unsignedMinValues[3];
	};

	deUint64				floatsData			[4];
	tcu::PixelBufferAccess	floatsAccess		(texFormat, 1, 1, 1, floatsData);
	tcu::Vec4				tmpFloats			(0.0f);

	const float				divider				(static_cast<float>(m_params->textureHeight));
	const tcu::Vec4			ufloatStep			(1.0f/(divider*1.0f), 1.0f/(divider*2.0f), 1.0f/(divider*3.0f), 1.0f/(divider*5.0f));
	const tcu::Vec4			sfloatStep			(2.0f/(divider*1.0f), 2.0f/(divider*2.0f), 2.0f/(divider*3.0f), 2.0f/(divider*5.0f));

	tcu::IVec4				signedColor			(0);
	tcu::UVec4				unsignedColor		(0u);
	tcu::Vec4				ufloatColor			(0.0f);
	tcu::Vec4				sfloatColor			(-1.0f);

	for (int y = 0; y < m_params->textureHeight; ++y)
	{
		for (int x = 0; x < m_params->textureWidth; ++x)
		{
			switch (channelClass)
			{
				case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
					pixels.setPixel(signedColor, x, y);
					break;

				case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
					pixels.setPixel(unsignedColor, x, y);
					break;

				case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
					floatsAccess.setPixel(sfloatColor, 0, 0);
					tmpFloats = ut::gluePixels(floatsAccess.getPixel(0, 0), sfloatColor, tcu::getNumUsedChannels(texFormat.order));
					pixels.setPixel(tmpFloats, x, y);
					break;

				// TEXTURECHANNELCLASS_FLOATING_POINT or
				// TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT
				default:
					floatsAccess.setPixel(ufloatColor, 0, 0);
					tmpFloats = ut::gluePixels(floatsAccess.getPixel(0, 0), ufloatColor, tcu::getNumUsedChannels(texFormat.order));
					pixels.setPixel(tmpFloats, x, y);
					break;
			}
		}

		nextSigned		(signedColor);
		nextUnsigned	(unsignedColor);
		sfloatColor +=	sfloatStep;
		ufloatColor +=	ufloatStep;
	}
}

tcu::TestStatus MismatchedWriteOpTestInstance::iterate (void)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	Move<VkCommandPool>				cmdPool				= createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkShaderModule>			shaderModule		= createShaderModule(vki, dev, m_context.getBinaryCollection().get("comp"), 0);

	Move<VkDescriptorSetLayout>		descriptorSetLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
															.build(vki, dev);
	Move<VkDescriptorPool>			descriptorPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vki, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vki, dev, *descriptorPool, *descriptorSetLayout);
	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vki, dev, *descriptorSetLayout);
	Move<VkPipeline>				pipeline			= makeComputePipeline(vki, dev, *pipelineLayout, *shaderModule);


	ut::StorageImage2D				image				(m_context, m_params->vkFormat, m_params->textureWidth, m_params->textureHeight);

	ut::StorageBuffer2D				buffer				(m_context, m_test->getBufferFormat(), m_params->textureWidth, m_params->textureHeight);

	VkDescriptorImageInfo			inputImageInfo		= makeDescriptorImageInfo(DE_NULL, image.getView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorBufferInfo			outputBufferInfo	= makeDescriptorBufferInfo(buffer.getBuffer(), 0u, buffer.getSize());

	DescriptorSetUpdateBuilder		builder;
	builder
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &inputImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferInfo)
		.update(vki, dev);

	populate	(buffer.getPixelAccess());
	clear		(image.getPixelAccess());

	beginCommandBuffer(vki, *cmdBuffer);
		image.upload(*cmdBuffer);
		vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vki.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vki.cmdDispatch(*cmdBuffer, m_params->textureWidth, m_params->textureHeight, 1);
		image.download(*cmdBuffer);
	endCommandBuffer(vki, *cmdBuffer);

	image.flush();
	buffer.flush();

	submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

	image.invalidate();
	buffer.invalidate();

	return compare(image.getPixelAccess(), buffer.getPixelAccess())
			? tcu::TestStatus::pass("")
			: tcu::TestStatus::fail("Pixel comparison failed");
}

bool MismatchedVectorSizesTestInstance::compare (tcu::PixelBufferAccess& result, tcu::PixelBufferAccess& reference) const
{
	const tcu::TextureFormat			texFormat		= mapVkFormat(m_params->vkFormat);
	const tcu::TextureChannelClass		channelClass	= tcu::getTextureChannelClass(texFormat.type);
	const int							targetWidth		= getNumUsedChannels(texFormat.order);

	bool								doContinue		= true;

	for (int y = 0; doContinue && y < m_params->textureHeight; ++y)
	{
		for (int x = 0; doContinue && x < m_params->textureWidth; ++x)
		{
			switch (channelClass)
			{
				case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
					doContinue	= ut::comparePixels(result.getPixelInt(x,y),  reference.getPixelInt(x,y), targetWidth );
					break;
				case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
					doContinue	= ut::comparePixels(result.getPixelUint(x,y), reference.getPixelUint(x,y), targetWidth );
					break;
				default:
					doContinue	= ut::comparePixels(result.getPixel(x,y),     reference.getPixel(x,y),	   targetWidth, 0.0005f);
					break;
			}
		}
	}

	return doContinue;
}

bool MismatchedSignednessAndTypeTestInstance::compare (tcu::PixelBufferAccess& result, tcu::PixelBufferAccess& reference) const
{
	DE_UNREF(result);
	DE_UNREF(reference);
	return true;
}

} // anonymous

tcu::TestCaseGroup* createImageWriteOpTests (tcu::TestContext& testCtx)
{
	std::stringstream ss;

	auto genVectorSizesTestName			= [&](const VkFormat& f, const int sourceWidth) -> std::string
	{
		ss.str(std::string());
		ss << de::toLower(getSpirvFormat(f)) << "_from";
		if (sourceWidth > 1)
			ss << "_vec" << sourceWidth;
		else ss << "_scalar";

		return ss.str();
	};

	auto testGroup						= new tcu::TestCaseGroup(testCtx, "mismatched_write_op",			"Test image OpImageWrite operation in various aspects.");
	auto testGroupMismatchedVectorSizes	= new tcu::TestCaseGroup(testCtx, "mismatched_vector_sizes",		"Case OpImageWrite operation on mismatched vector sizes.");
	auto testGroupMismatchedSignedness	= new tcu::TestCaseGroup(testCtx, "mismatched_signedness_and_type",	"Case OpImageWrite operation on mismatched signedness and values.");

	for (const VkFormat& f : allFormats)
	{
		const auto switchClass = getTextureChannelClass(mapVkFormat(f).type);
		auto compatibleFormats = findFormatsByChannelClass(switchClass);

		auto end	= compatibleFormats.cend();
		auto begin	= compatibleFormats.cbegin();
		for (auto i = begin; i != end; ++i)
		{
			if (is64BitIntegerFormat(i[0]) || is64BitIntegerFormat(f)) continue;

			const std::string testName = de::toLower(getSpirvFormat(i[0])) + "_from_" + de::toLower(getSpirvFormat(f));
			auto params	= new MismatchedWriteOpTest::Params { f, 12, 8*static_cast<int>(std::distance(begin,i)+1), i[0] };
			testGroupMismatchedSignedness->addChild(new MismatchedSignednessAndTypeTest(testCtx, testName, {}, MismatchedVectorSizesTest::ParamsSp(params)));
		}

		for (int sourceWidth = 4; sourceWidth > 0; --sourceWidth)
		{
			if (sourceWidth >= getNumUsedChannels(f))
			{
				auto params = new MismatchedWriteOpTest::Params { f, 12*sourceWidth, 8*(4-sourceWidth+1), f };
				testGroupMismatchedVectorSizes->addChild(
					new MismatchedVectorSizesTest(testCtx, genVectorSizesTestName(f, sourceWidth), {}, MismatchedVectorSizesTest::ParamsSp(params), sourceWidth));
			}
		}
	}

	testGroup->addChild(testGroupMismatchedVectorSizes);
	testGroup->addChild(testGroupMismatchedSignedness);

	return testGroup;
}

} // image
} // vkt
