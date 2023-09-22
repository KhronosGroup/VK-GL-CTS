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
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief SPIR-V Assembly Tests for PhysicalStorageBuffer.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmPhysicalStorageBufferPointerTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include <iterator>

using namespace vk;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

enum class PassMethod
{
	PUSH_CONSTANTS,
	PUSH_CONSTANTS_FUNCTION,
	VERTEX_IN_OUT_IN,
	ADDRESSES_IN_SSBO
};

struct TestParams
{
	PassMethod	method;
	deUint32	elements;
};

typedef SharedPtr<const TestParams>	TestParamsPtr;

namespace ut
{

class Buffer
{
public:
					Buffer			(Context& ctx, VkBufferUsageFlags usage, VkDeviceSize size, bool address = false);
					Buffer			(const Buffer& src);

	VkBuffer		getBuffer		(void) const { return **m_buffer; }
	VkDeviceSize	getSize			(void) const { return m_size; }
	void*			getData			(void) const { return (*m_bufferMemory)->getHostPtr(); }
	deUint64		getDeviceAddress(void) const;
	void			zero			(bool flushAfter = false);
	void			flush			(void);
	void			invalidate		(void);

protected:
	Context&						m_context;
	const VkDeviceSize				m_size;
	const bool						m_address;
	SharedPtr<Move<VkBuffer>>		m_buffer;
	SharedPtr<MovePtr<Allocation>>	m_bufferMemory;
};

template<class X> class TypedBuffer : public Buffer
{
public:
				TypedBuffer		(Context&					ctx,
								 VkBufferUsageFlags			usage,
								 deUint32					nelements,
								 bool						address = false);
				TypedBuffer		(Context&					ctx,
								 VkBufferUsageFlags			usage,
								 std::initializer_list<X>	items,
								 bool						address = false);
				TypedBuffer		(const TypedBuffer&			src);
				TypedBuffer		(const Buffer&				src);

	deUint32	getElements		(void) const { return m_elements; }
	X*			getData			(void) const { return reinterpret_cast<X*>(Buffer::getData()); }
	void		iota			(X start, bool flushAfter = false);
	X&			operator[]		(deUint32 at);

	struct iterator;
	iterator	begin			() { return iterator(getData()); }
	iterator	end				() { return iterator(&getData()[m_elements]); }

private:
	const deUint32				m_elements;
};

class Image
{
public:
										Image				(Context&			ctx,
															 deUint32			width,
															 deUint32			height,
															 VkFormat			format);
										Image				(const Image&) = delete;
										Image				(Image&&) = delete;

	Move<VkRenderPass>					createRenderPass	(void) const;
	Move<VkFramebuffer>					createFramebuffer	(VkRenderPass		rp) const;

	template<class X> TypedBuffer<X>	getBuffer			(void);
	void								downloadAfterDraw	(VkCommandBuffer	cmdBuffer);

private:
	Context&				m_context;
	const deUint32			m_width;
	const deUint32			m_height;
	const VkFormat			m_format;
	VkImageLayout			m_layout;
	Buffer					m_buffer;

	Move<VkImage>			m_image;
	Move<VkImageView>		m_view;
	de::MovePtr<Allocation>	m_imageMemory;
};

template<class X> SharedPtr<Move<X>> makeShared(Move<X> move)
{
	return SharedPtr<Move<X>>(new Move<X>(move));
}

template<class X> SharedPtr<MovePtr<X>> makeShared(MovePtr<X> move)
{
	return SharedPtr<MovePtr<X>>(new MovePtr<X>(move));
}

Buffer::Buffer	(Context& ctx, VkBufferUsageFlags usage, VkDeviceSize size, bool address)
	: m_context		(ctx)
	, m_size		(size)
	, m_address		(address)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const VkBufferUsageFlags		bufferUsageFlags	= address ? (usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) : usage;
	const MemoryRequirement			requirements		= MemoryRequirement::Coherent | MemoryRequirement::HostVisible | (address ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any);

	const VkBufferCreateInfo		bufferCreateInfo
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkBufferCreateFlags		flags;
		size,											// VkDeviceSize				size;
		bufferUsageFlags,								// VkBufferUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		1u,												// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex								// const deUint32*			pQueueFamilyIndices;
	};

	m_buffer		= makeShared(createBuffer(vki, dev, &bufferCreateInfo));
	m_bufferMemory	= makeShared(allocator.allocate(getBufferMemoryRequirements(vki, dev, **m_buffer), requirements));

	VK_CHECK(vki.bindBufferMemory(dev, **m_buffer, (*m_bufferMemory)->getMemory(), (*m_bufferMemory)->getOffset()));
}

Buffer::Buffer (const Buffer& src)
	: m_context		(src.m_context)
	, m_size		(src.m_size)
	, m_address		(src.m_address)
	, m_buffer		(src.m_buffer)
	, m_bufferMemory(src.m_bufferMemory)
{
}

deUint64 Buffer::getDeviceAddress (void) const
{
	DE_ASSERT(m_address);

	const DeviceInterface&				vki	= m_context.getDeviceInterface();
	const VkDevice						dev	= m_context.getDevice();
	const VkBufferDeviceAddressInfo		info
	{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		**m_buffer										// VkBuffer			buffer;
	};

	return vki.getBufferDeviceAddress(dev, &info);
}

void Buffer::zero (bool flushAfter)
{
	deMemset(getData(), 0, static_cast<size_t>(m_size));
	if (flushAfter) flush();
}

void Buffer::flush (void)
{
	const DeviceInterface&	vki	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_context.getDevice();
	flushAlloc(vki, dev, **m_bufferMemory);
}

void Buffer::invalidate (void)
{
	const DeviceInterface&	vki	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_context.getDevice();
	invalidateAlloc(vki, dev, **m_bufferMemory);
}

template<class X> struct TypedBuffer<X>::iterator
{
	typedef std::forward_iterator_tag	iterator_category;
	typedef std::ptrdiff_t				difference_type;
	typedef X							value_type;
	typedef X&							reference;
	typedef X*							pointer;

				iterator	(pointer p) : m_p(p)			{ DE_ASSERT(p); }
	reference	operator*	()								{ return *m_p; }
	iterator&	operator++	()								{ ++m_p; return *this; }
	iterator	operator++	(int)							{ return iterator(m_p++); }
	bool		operator==	(const iterator& other) const	{ return (m_p == other.m_p); }
	bool		operator!=	(const iterator& other) const	{ return (m_p != other.m_p); }

private:
	pointer m_p;
};

template<class X> TypedBuffer<X>::TypedBuffer (Context& ctx, VkBufferUsageFlags usage, deUint32 nelements, bool address)
	: Buffer		(ctx, usage, (nelements * sizeof(X)), address)
	, m_elements	(nelements)
{
}

template<class X> TypedBuffer<X>::TypedBuffer (Context& ctx, VkBufferUsageFlags usage, std::initializer_list<X> items, bool address)
	: Buffer		(ctx, usage, (items.size() * sizeof(X)), address)
	, m_elements	(static_cast<deUint32>(items.size()))
{
	std::copy(items.begin(), items.end(), begin());
}

template<class X> TypedBuffer<X>::TypedBuffer (const TypedBuffer& src)
	: Buffer	(src)
	, m_elements(src.m_elements)
{
}

template<class X> TypedBuffer<X>::TypedBuffer (const Buffer& src)
	: Buffer	(src)
	, m_elements(static_cast<deUint32>(m_size/sizeof(X)))
{
}

template<class X> void TypedBuffer<X>::iota (X start, bool flushAfter)
{
	X* data = getData();
	for (deUint32 i = 0; i < m_elements; ++i)
		data[i] = start++;
	if (flushAfter) flush();
}

template<class X> X& TypedBuffer<X>::operator[] (deUint32 at)
{
	DE_ASSERT(at < m_elements);
	return getData()[at];
}

Image::Image (Context& ctx, deUint32 width, deUint32 height, VkFormat format)
	: m_context	(ctx)
	, m_width	(width)
	, m_height	(height)
	, m_format	(format)
	, m_layout	(VK_IMAGE_LAYOUT_UNDEFINED)
	, m_buffer	(ctx, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), (m_width * m_height * vk::mapVkFormat(m_format).getPixelSize()), false)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkImageUsageFlags			imageUsageFlags		= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const VkImageSubresourceRange	viewResourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const VkImageCreateInfo			imageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		m_format,										// VkFormat					format;
		{ m_width, m_height, 1u },						// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		imageUsageFlags,								// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		1u,												// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,								// const deUint32*			pQueueFamilyIndices;
		m_layout										// VkImageLayout			initialLayout;
	};

	m_image			= createImage(vki, dev, &imageCreateInfo);

	m_imageMemory	= allocator.allocate(getImageMemoryRequirements(vki, dev, *m_image), MemoryRequirement::Any);
	VK_CHECK(vki.bindImageMemory(dev, *m_image, m_imageMemory->getMemory(), m_imageMemory->getOffset()));

	m_view			= makeImageView(vki, dev, *m_image, VK_IMAGE_VIEW_TYPE_2D, m_format, viewResourceRange);
}

template<class X> TypedBuffer<X> Image::getBuffer (void)
{
	m_buffer.invalidate();
	return TypedBuffer<X>(m_buffer);
}

Move<VkRenderPass> Image::createRenderPass (void) const
{
	const DeviceInterface&			vki						= m_context.getDeviceInterface();
	const VkDevice					dev						= m_context.getDevice();

	const VkAttachmentDescription	attachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		m_format,									// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		m_layout,									// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference		attachmentReference		=
	{
		0u,											// deUint32							attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					layout
	};

	const VkSubpassDescription		subpassDescription		=
	{
		(VkSubpassDescriptionFlags)0,				// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint             pipelineBindPoint
		0u,											// deUint32                        inputAttachmentCount
		DE_NULL,									// const VkAttachmentReference*    pInputAttachments
		1u,											// deUint32                        colorAttachmentCount
		&attachmentReference,						// const VkAttachmentReference*    pColorAttachments
		DE_NULL,									// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,									// const VkAttachmentReference*    pDepthStencilAttachment
		0u,											// deUint32                        preserveAttachmentCount
		DE_NULL										// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo	renderPassInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		DE_NULL,									// const void*                       pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags           flags
		1u,											// deUint32                          attachmentCount
		&attachmentDescription,						// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		DE_NULL										// const VkSubpassDependency*        pDependencies
	};

	return vk::createRenderPass(vki, dev, &renderPassInfo);
}

Move<VkFramebuffer> Image::createFramebuffer (VkRenderPass	rp) const
{
	const DeviceInterface&	vki	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_context.getDevice();

	return makeFramebuffer(vki, dev, rp, 1u, &m_view.get(), m_width, m_height, 1u);
}

void Image::downloadAfterDraw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vki	= m_context.getDeviceInterface();
	vk::copyImageToBuffer(vki, cmdBuffer, *m_image, m_buffer.getBuffer(), { deInt32(m_width), deInt32(m_height) });
	m_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

} // ut

class SpvAsmPhysicalStorageBufferTestInstance : public TestInstance
{
public:
	SpvAsmPhysicalStorageBufferTestInstance	(Context&	ctx)
		: TestInstance	(ctx)
	{
	}
};

class SpvAsmPhysicalStorageBufferVertexInOutInTestInstance : public SpvAsmPhysicalStorageBufferTestInstance
{
public:
									SpvAsmPhysicalStorageBufferVertexInOutInTestInstance	(Context&				ctx,
																							 const TestParamsPtr	params)
										: SpvAsmPhysicalStorageBufferTestInstance	(ctx)
										, m_params									(params)
									{
									}
	tcu::TestStatus					iterate													(void);
	static void						initPrograms											(vk::SourceCollections&	programCollection,
																							 const TestParamsPtr	params);
	struct alignas(16) Attribute
	{
		tcu::Vec4	position;
		deUint64	address;
	};
	ut::TypedBuffer<tcu::Vec4>		prepareColorBuffer										(bool					flushAfter = true) const;
	ut::TypedBuffer<Attribute>		prepareVertexAttributes									(deUint64				address) const;
	Move<VkPipeline>				createGraphicsPipeline									(VkPipelineLayout		pipelineLayout,
																							 VkRenderPass			renderPass,
																							 VkShaderModule			vertexModule,
																							 VkShaderModule			fragmentModule) const;

private:
	const TestParamsPtr		m_params;
};

class SpvAsmPhysicalStorageBufferPushConstantsTestInstance : public SpvAsmPhysicalStorageBufferTestInstance
{
public:
						SpvAsmPhysicalStorageBufferPushConstantsTestInstance	(Context&				ctx,
																				 const TestParamsPtr	params)
							: SpvAsmPhysicalStorageBufferTestInstance	(ctx)
							, m_params									(params)
						{
						}
	tcu::TestStatus		iterate													(void);
	static void			initPrograms											(vk::SourceCollections&	programCollection,
																				 const TestParamsPtr	params);

private:
	const TestParamsPtr		m_params;
};

class SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance : public SpvAsmPhysicalStorageBufferTestInstance
{
public:
						SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance	(Context&				ctx,
																			 const TestParamsPtr	params)
							: SpvAsmPhysicalStorageBufferTestInstance	(ctx)
							, m_params									(params)
						{
						}
	tcu::TestStatus		iterate												(void);
	static void			initPrograms										(vk::SourceCollections&	programCollection,
																			 const TestParamsPtr	params);

private:
	const TestParamsPtr		m_params;
};

class SpvAsmPhysicalStorageBufferTestCase : public TestCase
{
public:
						SpvAsmPhysicalStorageBufferTestCase		(tcu::TestContext&		testCtx,
																 const std::string&		name,
																 const TestParamsPtr	params)
							: TestCase	(testCtx, name, std::string())
							, m_params	(params)
						{
						}
	void				checkSupport							(Context&				context) const;
	void				initPrograms							(vk::SourceCollections&	programCollection) const;
	TestInstance*		createInstance							(Context&				ctx) const;

private:
	const TestParamsPtr		m_params;
};

void SpvAsmPhysicalStorageBufferTestCase::checkSupport (Context& context) const
{
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

	if (!context.isBufferDeviceAddressSupported())
		TCU_THROW(NotSupportedError, "Request physical storage buffer feature not supported");

	if (m_params->method == PassMethod::ADDRESSES_IN_SSBO)
	{
		if (!context.getDeviceFeatures().shaderInt64)
			TCU_THROW(NotSupportedError, "Int64 not supported");
	}

	if (m_params->method == PassMethod::VERTEX_IN_OUT_IN)
	{
		if (!context.getDeviceFeatures().shaderInt64)
			TCU_THROW(NotSupportedError, "Int64 not supported");

		VkFormatProperties2 properties	{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, DE_NULL, {} };
		context.getInstanceInterface().getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), VK_FORMAT_R64_UINT, &properties);
		if ((properties.formatProperties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
			TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT not supported");
	}
}

TestInstance* SpvAsmPhysicalStorageBufferTestCase::createInstance (Context& ctx) const
{
	switch (m_params->method)
	{
		case PassMethod::PUSH_CONSTANTS:
		case PassMethod::PUSH_CONSTANTS_FUNCTION:
			return new SpvAsmPhysicalStorageBufferPushConstantsTestInstance(ctx, m_params);

		case PassMethod::VERTEX_IN_OUT_IN:
			return new SpvAsmPhysicalStorageBufferVertexInOutInTestInstance(ctx, m_params);

		case PassMethod::ADDRESSES_IN_SSBO:
			return new SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance(ctx, m_params);
	}

	DE_ASSERT(DE_FALSE);
	return DE_NULL;
}

void SpvAsmPhysicalStorageBufferTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	switch (m_params->method)
	{
		case PassMethod::PUSH_CONSTANTS:
		case PassMethod::PUSH_CONSTANTS_FUNCTION:
			SpvAsmPhysicalStorageBufferPushConstantsTestInstance::initPrograms(programCollection, m_params);
			break;

		case PassMethod::VERTEX_IN_OUT_IN:
			SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::initPrograms(programCollection, m_params);
			break;

		case PassMethod::ADDRESSES_IN_SSBO:
			SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::initPrograms(programCollection, m_params);
			break;
	}
}

void SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::initPrograms (SourceCollections &programCollection, const TestParamsPtr params)
{
	DE_UNREF(params);

	const std::string vert(R"(
		OpCapability Shader
		OpCapability PhysicalStorageBufferAddresses

		OpExtension "SPV_KHR_physical_storage_buffer"
		OpMemoryModel PhysicalStorageBuffer64 GLSL450

		OpEntryPoint Vertex %vert "main" %gl_PerVertex %in_pos %out_idx %gl_VertexIndex %in_addr %out_addr

		OpDecorate %PerVertex Block
		OpDecorate %gl_VertexIndex BuiltIn VertexIndex
		OpDecorate %in_pos Location 0
		OpDecorate %in_addr Location 1
		OpDecorate %in_addr RestrictPointerEXT
		OpDecorate %out_addr RestrictPointerEXT
		OpDecorate %out_idx Location 0
		OpDecorate %out_addr Location 1

		OpMemberDecorate %PerVertex 0 BuiltIn Position
		OpMemberDecorate %PerVertex 1 BuiltIn PointSize
		OpMemberDecorate %PerVertex 2 BuiltIn ClipDistance
		OpMemberDecorate %PerVertex 3 BuiltIn CullDistance

		OpDecorate %srta Block
		OpMemberDecorate %srta 0 Offset 0

		OpDecorate %rta ArrayStride 16

		%void		= OpTypeVoid
		%voidf		= OpTypeFunction %void

		%int		= OpTypeInt 32 1
		%flt		= OpTypeFloat 32
		%vec4		= OpTypeVector %flt 4
		%rta		= OpTypeRuntimeArray %vec4

		%zero		= OpConstant %int 0
		%one		= OpConstant %int 1

		%srta		= OpTypeStruct %rta
		%srta_psb	= OpTypePointer PhysicalStorageBuffer %srta
	%srta_psb_in	= OpTypePointer Input %srta_psb
	%srta_psb_out	= OpTypePointer Output %srta_psb
		%in_addr	= OpVariable %srta_psb_in Input
		%out_addr	= OpVariable %srta_psb_out Output

		%vec4_in	= OpTypePointer Input %vec4
		%vec4_out	= OpTypePointer Output %vec4
		%vec4_psb	= OpTypePointer PhysicalStorageBuffer %vec4
		%in_pos		= OpVariable %vec4_in Input

		%int_in		= OpTypePointer Input %int
		%int_out	= OpTypePointer Output %int
	%gl_VertexIndex	= OpVariable %int_in Input
		%out_idx	= OpVariable %int_out Output

		%flt_arr_1	= OpTypeArray %flt %one
		%PerVertex	= OpTypeStruct %vec4 %flt %flt_arr_1 %flt_arr_1
		%pv_out		= OpTypePointer Output %PerVertex
	%gl_PerVertex	= OpVariable %pv_out Output


		%vert		= OpFunction %void None %voidf
		%vert_begin	= OpLabel

		%vpos		= OpLoad %vec4 %in_pos
	%gl_Position	= OpAccessChain %vec4_out %gl_PerVertex %zero
					OpStore %gl_Position %vpos

		%vidx		= OpLoad %int %gl_VertexIndex
					OpStore %out_idx %vidx

		%vaddr		= OpLoad %srta_psb %in_addr Aligned 8
					OpStore %out_addr %vaddr

					OpReturn
					OpFunctionEnd
	)");

	const std::string frag(R"(
		OpCapability Shader
		OpCapability PhysicalStorageBufferAddresses

		OpExtension "SPV_KHR_physical_storage_buffer"
		OpMemoryModel PhysicalStorageBuffer64 GLSL450

		OpEntryPoint Fragment %frag "main" %in_idx %in_addr %dEQP_FragColor
		OpExecutionMode %frag OriginUpperLeft

		OpDecorate %in_idx Location 0
		OpDecorate %in_idx Flat
		OpDecorate %in_addr Location 1
		OpDecorate %in_addr AliasedPointerEXT
		OpDecorate %in_addr Flat
		OpDecorate %dEQP_FragColor Location 0

		OpDecorate %rta ArrayStride 16
		OpDecorate %vec4_psb ArrayStride 16
		OpDecorate %srta Block
		OpMemberDecorate %srta 0 Offset 0

		%void		= OpTypeVoid
		%voidf		= OpTypeFunction %void

		%int		= OpTypeInt 32 1
		%flt		= OpTypeFloat 32
		%vec4		= OpTypeVector %flt 4
		%rta		= OpTypeRuntimeArray %vec4

		%zero		= OpConstant %int 0

		%int_in		= OpTypePointer Input %int
		%in_idx		= OpVariable %int_in Input

		%vec4_out	= OpTypePointer Output %vec4
	%dEQP_FragColor	= OpVariable %vec4_out Output

		%srta		= OpTypeStruct %rta
		%srta_psb	= OpTypePointer PhysicalStorageBuffer %srta
	%srta_psb_in	= OpTypePointer Input %srta_psb
		%in_addr	= OpVariable %srta_psb_in Input
		%rta_psb	= OpTypePointer PhysicalStorageBuffer %rta
		%rta_in		= OpTypePointer Input %rta
		%vec4_psb	= OpTypePointer PhysicalStorageBuffer %vec4

		%frag		= OpFunction %void None %voidf
		%frag_begin	= OpLabel

		%vidx		= OpLoad %int %in_idx
		%vaddr		= OpLoad %srta_psb %in_addr
		%pcolor		= OpAccessChain %vec4_psb %vaddr %zero %vidx
		%color		= OpLoad %vec4 %pcolor Aligned 16
					OpStore %dEQP_FragColor %color
		OpReturn
		OpFunctionEnd
	)");

	programCollection.spirvAsmSources.add("vert")
			<< vert
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
	programCollection.spirvAsmSources.add("frag")
			<< frag
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

ut::TypedBuffer<tcu::Vec4> SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::prepareColorBuffer (bool flushAfter) const
{
	const deUint32	colorCount		= 21;
	tcu::Vec4		colors			[colorCount];
	tcu::Vec4		color			(-1.0f, +1.0f, +1.0f, -1.0f);

	for (deUint32 c = 0; c < colorCount; ++c)
	{
		colors[c]	= color;

		color[0]	+= 0.1f;
		color[1]	-= 0.1f;
		color[2]	-= 0.1f;
		color[3]	+= 0.1f;
	}

	ut::TypedBuffer<tcu::Vec4> buffer(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, (m_params->elements * m_params->elements), true);
	for (auto j = buffer.begin(), begin = j; j != buffer.end(); ++j)
	{
		 *j = colors[std::distance(begin, j) % colorCount];
	}

	if (flushAfter) buffer.flush();
	return  buffer;
}

ut::TypedBuffer<SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::Attribute>
	SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::prepareVertexAttributes (deUint64 address) const
{
	const float					xStep	= 2.0f / static_cast<float>(m_params->elements);
	const float					yStep	= 2.0f / static_cast<float>(m_params->elements);
	const float					xStart	= -1.0f + xStep / 2.0f;
	const float					yStart	= -1.0f + yStep / 2.0f;

	float						x		= xStart;
	float						y		= yStart;

	ut::TypedBuffer<Attribute>	attrs	(m_context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (m_params->elements * m_params->elements));

	for (deUint32 row = 0; row < m_params->elements; ++row)
	{
		for (deUint32 col = 0; col < m_params->elements; ++col)
		{
			Attribute& attr	= attrs[(row*m_params->elements)+col];
			attr.position	= tcu::Vec4(x, y, 0.0f, 1.0f);
			attr.address	= address;

			x += xStep;
		}
		y += yStep;
		x = xStart;
	}

	attrs.flush();

	return  attrs;
}

Move<VkPipeline> SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::createGraphicsPipeline (VkPipelineLayout	pipelineLayout,
																							   VkRenderPass		renderPass,
																							   VkShaderModule	vertexModule,
																							   VkShaderModule	fragmentModule) const
{
	const DeviceInterface&							vk					= m_context.getDeviceInterface();
	const VkDevice									device				= m_context.getDevice();
	const std::vector<VkRect2D>						scissors			(1, makeRect2D(m_params->elements, m_params->elements));
	const std::vector<VkViewport>					viewports			(1, makeViewport(m_params->elements, m_params->elements));

	const VkVertexInputBindingDescription		bindingDescriptions[]	=
	{
		{
			0u,													// binding
			sizeof(Attribute),									// stride
			VK_VERTEX_INPUT_RATE_VERTEX,						// inputRate
		},
	};

	const VkVertexInputAttributeDescription		attributeDescriptions[]	=
	{
		{
			0u,													// location
			0u,													// binding
			VK_FORMAT_R32G32B32A32_SFLOAT,						// format
			0u													// offset
		},
		{
			1u,													// location
			0u,													// binding
			VK_FORMAT_R64_UINT,									// format
			static_cast<deUint32>(sizeof(Attribute::position))	// offset
		},
	};

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0,	// flags
		DE_LENGTH_OF_ARRAY(bindingDescriptions),	// vertexBindingDescriptionCount
		bindingDescriptions,						// pVertexBindingDescriptions
		DE_LENGTH_OF_ARRAY(attributeDescriptions),	// vertexAttributeDescriptionCount
		attributeDescriptions						// pVertexAttributeDescriptions
	};

	return vk::makeGraphicsPipeline(
		vk,												// vk
		device,											// device
		pipelineLayout,									// pipelineLayout
		vertexModule,									// vertexShaderModule
		DE_NULL,										// tessellationControlModule
		DE_NULL,										// tessellationEvalModule
		DE_NULL,										// geometryShaderModule
		fragmentModule,									// fragmentShaderModule
		renderPass,										// renderPass
		viewports,										// viewports
		scissors,										// scissors
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,				// topology
		0U,												// subpass
		0U,												// patchControlPoints
		&vertexInputStateCreateInfo,					// vertexInputStateCreateInfo
		DE_NULL,										// rasterizationStateCreateInfo
		DE_NULL,										// multisampleStateCreateInfo
		DE_NULL,										// depthStencilStateCreateInfo
		DE_NULL,										// colorBlendStateCreateInfo
		DE_NULL);										// dynamicStateCreateInfo
}

tcu::TestStatus SpvAsmPhysicalStorageBufferVertexInOutInTestInstance::iterate (void)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkFormat					format				= VK_FORMAT_R32G32B32A32_SFLOAT;
	const VkRect2D					renderArea			= makeRect2D(m_params->elements, m_params->elements);

	Move<VkCommandPool>				cmdPool				= createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	ut::Image						image				(m_context, m_params->elements, m_params->elements, format);
	Move<VkRenderPass>				renderPass			= image.createRenderPass();
	Move<VkFramebuffer>				framebuffer			= image.createFramebuffer(*renderPass);

	Move<VkShaderModule>			vertexModule		= createShaderModule(vki, dev, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>			fragmentModule		= createShaderModule(vki, dev, m_context.getBinaryCollection().get("frag"), 0);
	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vki, dev, 0u, DE_NULL);
	Move<VkPipeline>				pipeline			= createGraphicsPipeline(*pipelineLayout, *renderPass, *vertexModule, *fragmentModule);

	ut::TypedBuffer<tcu::Vec4>		colorBuffer			= prepareColorBuffer();
	ut::TypedBuffer<Attribute>		attributes			= prepareVertexAttributes(colorBuffer.getDeviceAddress());
	const VkBuffer					vertexBuffers[]		= { attributes.getBuffer() };
	const VkDeviceSize				vertexOffsets[]		= { 0u };
	const tcu::Vec4					clearColor			(-1.0f);

	beginCommandBuffer(vki, *cmdBuffer);
		vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vki.cmdBindVertexBuffers(*cmdBuffer, 0, 1, vertexBuffers, vertexOffsets);
		beginRenderPass(vki, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
			vki.cmdDraw(*cmdBuffer, (m_params->elements * m_params->elements), 1u, 0u, 0u);
		endRenderPass(vki, *cmdBuffer);
		image.downloadAfterDraw(*cmdBuffer);
	endCommandBuffer(vki, *cmdBuffer);

	submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

	ut::TypedBuffer<tcu::Vec4>		resultBuffer		= image.getBuffer<tcu::Vec4>();

	return std::equal(resultBuffer.begin(), resultBuffer.end(), colorBuffer.begin()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

void SpvAsmPhysicalStorageBufferPushConstantsTestInstance::initPrograms (vk::SourceCollections& programCollection, const TestParamsPtr params)
{
	DE_UNREF(params);

	const std::string program(R"(
	OpCapability Shader
	OpCapability PhysicalStorageBufferAddresses

	OpExtension "SPV_KHR_physical_storage_buffer"
	OpMemoryModel PhysicalStorageBuffer64 GLSL450

	OpEntryPoint GLCompute %main "main" %id %str

	OpExecutionMode %main LocalSize 1 1 1
	OpSource GLSL 450
	OpName %main	"main"
	OpName %id		"gl_GlobalInvocationID"
	OpName %src		"source"
	OpName %dst		"destination"
	OpName %src_buf	"source"
	OpName %dst_buf	"destination"
	OpDecorate %id BuiltIn GlobalInvocationId

	OpDecorate %str_t Block
	OpMemberDecorate %str_t 0 Offset 0
	OpMemberDecorate %str_t 1 Offset 8
	OpMemberDecorate %str_t 2 Offset 16
	OpMemberDecorate %str_t 3 Offset 20

	OpDecorate %src_buf Restrict
	OpDecorate %dst_buf Restrict

	OpDecorate %int_arr ArrayStride 4

			%int = OpTypeInt 32 1
		%int_ptr = OpTypePointer PhysicalStorageBuffer %int
	   %int_fptr = OpTypePointer Function %int
		   %zero = OpConstant %int 0
			%one = OpConstant %int 1
			%two = OpConstant %int 2
		  %three = OpConstant %int 3

		   %uint = OpTypeInt 32 0
	   %uint_ptr = OpTypePointer Input %uint
	  %uint_fptr = OpTypePointer Function %uint
		  %uvec3 = OpTypeVector %uint 3
	  %uvec3ptr  = OpTypePointer Input %uvec3
		  %uzero = OpConstant %uint 0
			 %id = OpVariable %uvec3ptr Input

		%int_arr = OpTypeRuntimeArray %int

		%buf_ptr = OpTypePointer PhysicalStorageBuffer %int_arr
		  %str_t = OpTypeStruct %buf_ptr %buf_ptr %int %int
		%str_ptr = OpTypePointer PushConstant %str_t
			%str = OpVariable %str_ptr PushConstant
	%buf_ptr_fld = OpTypePointer PushConstant %buf_ptr
		%int_fld = OpTypePointer PushConstant %int

		   %bool = OpTypeBool
		   %void = OpTypeVoid
		  %voidf = OpTypeFunction %void
	   %cpbuffsf = OpTypeFunction %void %buf_ptr %buf_ptr %int

		%cpbuffs = OpFunction %void None %cpbuffsf
		%src_buf = OpFunctionParameter %buf_ptr
		%dst_buf = OpFunctionParameter %buf_ptr
	   %elements = OpFunctionParameter %int
	   %cp_begin = OpLabel
			  %j = OpVariable %int_fptr Function
				   OpStore %j %zero
				   OpBranch %for
			%for = OpLabel
			 %vj = OpLoad %int %j
			 %cj = OpULessThan %bool %vj %elements
				   OpLoopMerge %for_end %incj None
				   OpBranchConditional %cj %for_body %for_end
	   %for_body = OpLabel
	 %src_el_lnk = OpAccessChain %int_ptr %src_buf %vj
	 %dst_el_lnk = OpAccessChain %int_ptr %dst_buf %vj
		 %src_el = OpLoad %int %src_el_lnk Aligned 4
				   OpStore %dst_el_lnk %src_el Aligned 4
				   OpBranch %incj
		   %incj = OpLabel
			 %nj = OpIAdd %int %vj %one
				   OpStore %j %nj
				   OpBranch %for
		%for_end = OpLabel
				   OpReturn
				   OpFunctionEnd

		   %main = OpFunction %void None %voidf
		  %begin = OpLabel
			  %i = OpVariable %int_fptr Function
				   OpStore %i %zero
		%src_lnk = OpAccessChain %buf_ptr_fld %str %zero
		%dst_lnk = OpAccessChain %buf_ptr_fld %str %one
		%cnt_lnk = OpAccessChain %int_fld %str %two
	%use_fun_lnk = OpAccessChain %int_fld %str %three
			%src = OpLoad %buf_ptr %src_lnk
			%dst = OpLoad %buf_ptr %dst_lnk
			%cnt = OpLoad %int %cnt_lnk
		%use_fun = OpLoad %int %use_fun_lnk

			%cuf = OpINotEqual %bool %use_fun %zero
				   OpSelectionMerge %use_fun_end None
				   OpBranchConditional %cuf %copy %loop
		   %copy = OpLabel
		 %unused = OpFunctionCall %void %cpbuffs %src %dst %cnt
				   OpBranch %use_fun_end
		   %loop = OpLabel
			 %vi = OpLoad %int %i
			 %ci = OpSLessThan %bool %vi %cnt
				   OpLoopMerge %loop_end %inci None
				   OpBranchConditional %ci %loop_body %loop_end
	  %loop_body = OpLabel
	 %src_px_lnk = OpAccessChain %int_ptr %src %vi
	 %dst_px_lnk = OpAccessChain %int_ptr %dst %vi
		 %src_px = OpLoad %int %src_px_lnk Aligned 4
				   OpStore %dst_px_lnk %src_px Aligned 4
				   OpBranch %inci
		   %inci = OpLabel
			 %ni = OpIAdd %int %vi %one
				   OpStore %i %ni
				   OpBranch %loop
	   %loop_end = OpLabel
				   OpBranch %use_fun_end
	%use_fun_end = OpLabel

				   OpReturn
				   OpFunctionEnd
	)");

	programCollection.spirvAsmSources.add("comp")
			<< program
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

tcu::TestStatus SpvAsmPhysicalStorageBufferPushConstantsTestInstance::iterate (void)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	Move<VkCommandPool>				cmdPool				= createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkShaderModule>			shaderModule		= createShaderModule(vki, dev, m_context.getBinaryCollection().get("comp"), 0);

	struct PushConstant
	{
		deUint64	src;
		deUint64	dst;
		deInt32		cnt;
		deBool		use_fun;
	};

	VkPushConstantRange				pushConstantRange	=
	{
		VK_SHADER_STAGE_COMPUTE_BIT,	// VkShaderStageFlags	stageFlags;
		0,								// deUint32				offset;
		sizeof(PushConstant)			// deUint32				size;
	};

	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vki, dev, 0, DE_NULL, 1, &pushConstantRange);
	Move<VkPipeline>				pipeline			= makeComputePipeline(vki, dev, *pipelineLayout, *shaderModule);

	ut::TypedBuffer<deInt32>		src					(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);
	ut::TypedBuffer<deInt32>		dst					(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);

	src.iota(m_params->elements, true);
	dst.zero(true);

	const PushConstant				 pc					= { src.getDeviceAddress(), dst.getDeviceAddress(), deInt32(m_params->elements), m_params->method == PassMethod::PUSH_CONSTANTS_FUNCTION };

	beginCommandBuffer(vki, *cmdBuffer);
		vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vki.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vki.cmdDispatch(*cmdBuffer, 1, 1, 1);
	endCommandBuffer(vki, *cmdBuffer);

	submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

	dst.invalidate();

	return std::equal(src.begin(), src.end(), dst.begin()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

void SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::initPrograms (vk::SourceCollections& programCollection, const TestParamsPtr params)
{
	DE_UNREF(params);

	const std::string comp(R"(
	OpCapability Shader
	OpCapability Int64
	OpCapability PhysicalStorageBufferAddresses

	OpExtension "SPV_KHR_physical_storage_buffer"
	OpMemoryModel PhysicalStorageBuffer64 GLSL450

	OpEntryPoint GLCompute %comp "main" %id %ssbo

	OpExecutionMode %comp LocalSize 1 1 1
	OpDecorate %id BuiltIn GlobalInvocationId

	OpDecorate %sssbo Block
	OpMemberDecorate %sssbo 0 Offset 0
	OpMemberDecorate %sssbo 1 Offset 8
	OpMemberDecorate %sssbo 2 Offset 16
	OpMemberDecorate %sssbo 3 Offset 24

	OpDecorate %ssbo DescriptorSet 0
	OpDecorate %ssbo Binding 0

	OpDecorate %rta ArrayStride 4

	%bool	= OpTypeBool
	%int	= OpTypeInt 32 1
	%uint	= OpTypeInt 32 0
	%ulong	= OpTypeInt 64 0

	%zero	= OpConstant %int 0
	%one	= OpConstant %int 1
	%two	= OpConstant %int 2
	%three	= OpConstant %int 3

	%uvec3	= OpTypeVector %uint 3
	%rta	= OpTypeRuntimeArray %int

	%rta_psb	= OpTypePointer PhysicalStorageBuffer %rta
	%sssbo		= OpTypeStruct %rta_psb %ulong %rta_psb %ulong
	%sssbo_buf	= OpTypePointer StorageBuffer %sssbo
	%ssbo		= OpVariable %sssbo_buf StorageBuffer
	%rta_psb_sb	= OpTypePointer StorageBuffer %rta_psb
	%int_psb	= OpTypePointer PhysicalStorageBuffer %int
	%ulong_sb	= OpTypePointer StorageBuffer %ulong

	%uvec3_in	= OpTypePointer Input %uvec3
	%id			= OpVariable %uvec3_in Input
	%uint_in	= OpTypePointer Input %uint

	%void		= OpTypeVoid
	%voidf		= OpTypeFunction %void

	%comp = OpFunction %void None %voidf
	%comp_begin = OpLabel

		%pgid_x	= OpAccessChain %uint_in %id %zero
		%gid_x	= OpLoad %uint %pgid_x
		%mod2	= OpSMod %int %gid_x %two
		%even	= OpIEqual %bool %mod2 %zero

		%psrc_buff_p	= OpAccessChain %rta_psb_sb %ssbo %zero
		%pdst_buff_p	= OpAccessChain %rta_psb_sb %ssbo %two
		%src_buff_p		= OpLoad %rta_psb %psrc_buff_p
		%dst_buff_p		= OpLoad %rta_psb %pdst_buff_p

		%psrc_buff_u	= OpAccessChain %ulong_sb %ssbo %one
		%psrc_buff_v	= OpLoad %ulong %psrc_buff_u
		%src_buff_v		= OpConvertUToPtr %rta_psb %psrc_buff_v
		%pdst_buff_u	= OpAccessChain %ulong_sb %ssbo %three
		%pdst_buff_v	= OpLoad %ulong %pdst_buff_u
		%dst_buff_v		= OpConvertUToPtr %rta_psb %pdst_buff_v

		%src	= OpSelect %rta_psb %even %src_buff_p %src_buff_v
		%dst	= OpSelect %rta_psb %even %dst_buff_v %dst_buff_p

		%psrc_color	= OpAccessChain %int_psb %src %gid_x
		%src_color	= OpLoad %int %psrc_color Aligned 4
		%pdst_color	= OpAccessChain %int_psb %dst %gid_x
		OpStore %pdst_color %src_color Aligned 4

	OpReturn
	OpFunctionEnd
	)");

	programCollection.spirvAsmSources.add("comp")
			<< comp
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
}

/*
 Below test does not add anything new. The main purpose of this test is to show that both PhysicalStorageBuffer
 and 64-bit integer value can coexist in one array one next to the other. In the both cases, when the one address
 has its own dedicated storage class and the other is regular integer, the shader is responsible for how to interpret
 and use input addresses. Regardless of the shader, the application always passes them as 64-bit integers.
*/
tcu::TestStatus SpvAsmPhysicalStorageBufferAddrsInSSBOTestInstance::iterate (void)
{
	const DeviceInterface&			vki					= m_context.getDeviceInterface();
	const VkDevice					dev					= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	Move<VkCommandPool>				cmdPool				= createCommandPool(vki, dev, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vki, dev, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkShaderModule>			shaderModule		= createShaderModule(vki, dev, m_context.getBinaryCollection().get("comp"), 0);

	Move<VkDescriptorSetLayout>		descriptorSetLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
															.build(vki, dev);
	Move<VkDescriptorPool>			descriptorPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vki, dev, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vki, dev, *descriptorPool, *descriptorSetLayout);
	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vki, dev, 1u, &descriptorSetLayout.get());
	Move<VkPipeline>				pipeline			= makeComputePipeline(vki, dev, *pipelineLayout, *shaderModule);


	ut::TypedBuffer<deInt32>		src					(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);
	ut::TypedBuffer<deInt32>		dst					(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_params->elements, true);

	struct SSBO
	{
		deUint64	srcAsBuff;
		deUint64	srcAsUint;
		deUint64	dstAsBuff;
		deUint64	dstAsUint;
	};
	ut::TypedBuffer<SSBO>			ssbo				(m_context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {
															 {
																 src.getDeviceAddress(),
																 src.getDeviceAddress(),
																 dst.getDeviceAddress(),
																 dst.getDeviceAddress()
															 }
														 });
	VkDescriptorBufferInfo			ssboBufferInfo		= makeDescriptorBufferInfo(ssbo.getBuffer(), 0, ssbo.getSize());
	DescriptorSetUpdateBuilder		()					.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboBufferInfo)
															.update(vki, dev);

	src.iota(m_params->elements, true);
	dst.zero(true);

	beginCommandBuffer(vki, *cmdBuffer);
		vki.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
			vki.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vki.cmdDispatch(*cmdBuffer, m_params->elements, 1, 1);
	endCommandBuffer(vki, *cmdBuffer);

	submitCommandsAndWait(vki, dev, queue, *cmdBuffer);

	dst.invalidate();

	return std::equal(src.begin(), src.end(), dst.begin()) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

} // unnamed

tcu::TestCaseGroup*	createPhysicalStorageBufferTestGroup (tcu::TestContext& testCtx)
{
	struct
	{
		PassMethod	method;
		std::string	testName;
	}
	const				methods[]	=
	{
		{ PassMethod::PUSH_CONSTANTS,			"push_constants"			},
		{ PassMethod::PUSH_CONSTANTS_FUNCTION,	"push_constants_function"	},
		{ PassMethod::VERTEX_IN_OUT_IN,			"vertex_in_out_in"			},
		{ PassMethod::ADDRESSES_IN_SSBO,		"addrs_in_ssbo"				},
	};

	tcu::TestCaseGroup* group		= new tcu::TestCaseGroup(testCtx, "physical_storage_buffer", "Various methods of PhysicalStorageBuffer passing");

	for (const auto& method : methods)
	{
		group->addChild(new SpvAsmPhysicalStorageBufferTestCase(testCtx, method.testName, TestParamsPtr(new TestParams({method.method, 64}))));
	}

	return group;
}

} // SpirVAssembly
} // vkt

