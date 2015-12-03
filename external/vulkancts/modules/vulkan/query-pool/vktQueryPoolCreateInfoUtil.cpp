/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief CreateInfo utilities
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolCreateInfoUtil.hpp"

#include "vkImageUtil.hpp"

namespace vkt
{
namespace QueryPool
{

ImageSubresourceRange::ImageSubresourceRange (	vk::VkImageAspectFlags	_aspectMask,
												deUint32				_baseMipLevel,
												deUint32				_mipLevels,
												deUint32				_baseArrayLayer,
												deUint32				_arraySize)
{
	aspectMask		= _aspectMask;
	baseMipLevel	= _baseMipLevel;
	mipLevels		= _mipLevels;
	baseArrayLayer	= _baseArrayLayer;
	arraySize		= _arraySize;
}

ChannelMapping::ChannelMapping (vk::VkChannelSwizzle _r,
								vk::VkChannelSwizzle _g,
								vk::VkChannelSwizzle _b,
								vk::VkChannelSwizzle _a)
{
	r = _r;
	g = _g;
	b = _b;
	a = _a;
}

ImageViewCreateInfo::ImageViewCreateInfo (			vk::VkImage						_image,
													vk::VkImageViewType				_viewType,
													vk::VkFormat					_format,
											const	vk::VkImageSubresourceRange&	_subresourceRange,
											const	vk::VkChannelMapping&			_channels,
													vk::VkImageViewCreateFlags		_flags)
{
	sType = vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	pNext = DE_NULL;
	image				= _image;
	viewType			= _viewType;
	format				= _format;
	channels.r			= _channels.r;
	channels.g			= _channels.g;
	channels.b			= _channels.b;
	channels.a			= _channels.a;
	subresourceRange	= _subresourceRange;
	flags				= _flags;
}

ImageViewCreateInfo::ImageViewCreateInfo (			vk::VkImage					_image,
													vk::VkImageViewType			_viewType,
													vk::VkFormat				_format,
											const	vk::VkChannelMapping&		_channels,
													vk::VkImageViewCreateFlags	_flags)
{
	sType = vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	pNext = DE_NULL;
	image		= _image;
	viewType	= _viewType;
	format		= _format;
	channels.r	= _channels.r;
	channels.g	= _channels.g;
	channels.b	= _channels.b;
	channels.a	= _channels.a;

	vk::VkImageAspectFlags aspectFlags;
	const tcu::TextureFormat tcuFormat = vk::mapVkFormat(_format);

	switch (tcuFormat.order)
	{
		case tcu::TextureFormat::R:
		case tcu::TextureFormat::A:
		case tcu::TextureFormat::I:
		case tcu::TextureFormat::L:
		case tcu::TextureFormat::LA:
		case tcu::TextureFormat::RG:
		case tcu::TextureFormat::RA:
		case tcu::TextureFormat::RGB:
		case tcu::TextureFormat::RGBA:
		case tcu::TextureFormat::ARGB:
		case tcu::TextureFormat::BGRA:
		case tcu::TextureFormat::sR:
		case tcu::TextureFormat::sRG:
		case tcu::TextureFormat::sRGB:
		case tcu::TextureFormat::sRGBA:
			aspectFlags = vk::VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		case tcu::TextureFormat::D:
			aspectFlags = vk::VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		case tcu::TextureFormat::S:
			aspectFlags = vk::VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		case tcu::TextureFormat::DS:
			aspectFlags = vk::VK_IMAGE_ASPECT_STENCIL_BIT | vk::VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		default:
			TCU_FAIL("unhandled format");
	}

	subresourceRange = ImageSubresourceRange(aspectFlags);;
	flags = _flags;
}

BufferViewCreateInfo::BufferViewCreateInfo (vk::VkBuffer	_buffer,
											vk::VkFormat		_format,
											vk::VkDeviceSize _offset,
											vk::VkDeviceSize _range)
{
	sType = vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	pNext = DE_NULL;

	buffer	= _buffer;
	format	= _format;
	offset	= _offset;
	range	= _range;
}


BufferCreateInfo::BufferCreateInfo (		vk::VkDeviceSize		_size,
											vk::VkBufferUsageFlags	_usage,
											vk::VkSharingMode		_sharingMode,
											deUint32				_queueFamilyCount,
									const	deUint32*				_pQueueFamilyIndices,
											vk::VkBufferCreateFlags _flags)
{   
	sType = vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	pNext = DE_NULL;
	size				= _size;
	usage				= _usage;
	flags				= _flags;
	sharingMode			= _sharingMode;
	queueFamilyCount	= _queueFamilyCount;

	if (_queueFamilyCount)
	{
		m_queueFamilyIndices = std::vector<deUint32>(
			_pQueueFamilyIndices, _pQueueFamilyIndices + _queueFamilyCount);
		pQueueFamilyIndices = &m_queueFamilyIndices[0];
	} else
		pQueueFamilyIndices = _pQueueFamilyIndices;

   
}

BufferCreateInfo::BufferCreateInfo (const BufferCreateInfo &other)
{
	sType				= other.sType;
	pNext				= other.pNext;
	size				= other.size;
	usage				= other.usage;
	flags				= other.flags;
	sharingMode			= other.sharingMode;
	queueFamilyCount	= other.queueFamilyCount;

	m_queueFamilyIndices = other.m_queueFamilyIndices;
	DE_ASSERT(m_queueFamilyIndices.size() == queueFamilyCount);

	if (m_queueFamilyIndices.size())
	{
		pQueueFamilyIndices = &m_queueFamilyIndices[0];
	}
	else
	{
		pQueueFamilyIndices = DE_NULL;
	}
}

BufferCreateInfo & BufferCreateInfo::operator= (const BufferCreateInfo &other)
{
	sType					= other.sType;
	pNext					= other.pNext;
	size					= other.size;
	usage					= other.usage;
	flags					= other.flags;
	sharingMode				= other.sharingMode;
	queueFamilyCount		= other.queueFamilyCount;

	m_queueFamilyIndices	= other.m_queueFamilyIndices;

	DE_ASSERT(m_queueFamilyIndices.size() == queueFamilyCount);

	if (m_queueFamilyIndices.size())
	{
		pQueueFamilyIndices = &m_queueFamilyIndices[0];
	}
	else
	{
		pQueueFamilyIndices = DE_NULL;
	}

	return *this;
}

ImageCreateInfo::ImageCreateInfo (			vk::VkImageType			_imageType,
											vk::VkFormat			_format,
											vk::VkExtent3D			_extent,
											deUint32				_mipLevels,
											deUint32				_arraySize,
											deUint32				_samples,
											vk::VkImageTiling		_tiling,
											vk::VkImageUsageFlags	_usage,
											vk::VkSharingMode		_sharingMode,
											deUint32				_queueFamilyCount,
									const	deUint32*				_pQueueFamilyIndices,
											vk::VkImageCreateFlags	_flags, 
											vk::VkImageLayout		_initialLayout)
{
	if (_queueFamilyCount)
	{
		m_queueFamilyIndices = std::vector<deUint32>(_pQueueFamilyIndices, _pQueueFamilyIndices + _queueFamilyCount);
	}

	sType = vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	pNext = DE_NULL;
	imageType			= _imageType;
	format				= _format;
	extent				= _extent;
	mipLevels			= _mipLevels;
	arraySize			= _arraySize;
	samples				= _samples;
	tiling				= _tiling;
	usage				= _usage;
	sharingMode			= _sharingMode;
	queueFamilyCount	= _queueFamilyCount;

	if (m_queueFamilyIndices.size())
	{
		pQueueFamilyIndices = &m_queueFamilyIndices[0];
	}
	else
	{
		pQueueFamilyIndices = DE_NULL;
	}

	flags			= _flags;
	initialLayout	= _initialLayout;
}

FramebufferCreateInfo::FramebufferCreateInfo (			vk::VkRenderPass				_renderPass,
												const	std::vector<vk::VkImageView>&	atachments, 
														deUint32						_width,
														deUint32						_height,
														deUint32						_layers)
{
	sType = vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	pNext = DE_NULL;

	renderPass		= _renderPass;
	attachmentCount	= static_cast<deUint32>(atachments.size());

	if (attachmentCount)
	{
		pAttachments = const_cast<vk::VkImageView *>(&atachments[0]);
	}

	width	= _width;
	height	= _height;
	layers	= _layers;
}

RenderPassCreateInfo::RenderPassCreateInfo (const std::vector<vk::VkAttachmentDescription>&	attachments,
											const std::vector<vk::VkSubpassDescription>&	subpasses,
											const std::vector<vk::VkSubpassDependency>&		dependiences)

	: m_attachments			(attachments.begin(), attachments.end())
	, m_subpasses			(subpasses.begin(), subpasses.end())
	, m_dependiences		(dependiences.begin(), dependiences.end())
	, m_attachmentsStructs	(m_attachments.begin(), m_attachments.end())
	, m_subpassesStructs	(m_subpasses.begin(), m_subpasses.end())
	, m_dependiencesStructs	(m_dependiences.begin(), m_dependiences.end())
{
	sType = vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pNext = DE_NULL;

	attachmentCount = static_cast<deUint32>(m_attachments.size());
	pAttachments	= &m_attachmentsStructs[0];
	subpassCount	= static_cast<deUint32>(m_subpasses.size());
	pSubpasses		= &m_subpassesStructs[0];
	dependencyCount = static_cast<deUint32>(m_dependiences.size());
	pDependencies	= &m_dependiencesStructs[0];
}

RenderPassCreateInfo::RenderPassCreateInfo (		deUint32						_attachmentCount,
											const	vk::VkAttachmentDescription*	_pAttachments,
													deUint32						_subpassCount,
											const	vk::VkSubpassDescription*		_pSubpasses,
													deUint32						_dependencyCount,
											const	vk::VkSubpassDependency*		_pDependiences)
{

	m_attachments	= std::vector<AttachmentDescription>(_pAttachments, _pAttachments + _attachmentCount);
	m_subpasses		= std::vector<SubpassDescription>(_pSubpasses, _pSubpasses + _subpassCount);
	m_dependiences	= std::vector<SubpassDependency>(_pDependiences, _pDependiences + _dependencyCount);

	m_attachmentsStructs	= std::vector<vk::VkAttachmentDescription>	(m_attachments.begin(),		m_attachments.end());
	m_subpassesStructs		= std::vector<vk::VkSubpassDescription>		(m_subpasses.begin(),		m_subpasses.end());
	m_dependiencesStructs	= std::vector<vk::VkSubpassDependency>		(m_dependiences.begin(),	m_dependiences.end());

	sType = vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pNext = DE_NULL;

	attachmentCount = static_cast<deUint32>(m_attachments.size());

	if (attachmentCount) {
		pAttachments = &m_attachmentsStructs[0];
	}
	else
	{
		pAttachments = DE_NULL;
	}

	subpassCount = static_cast<deUint32>(m_subpasses.size());

	if (subpassCount) {
		pSubpasses = &m_subpassesStructs[0];
	}
	else
	{
		pSubpasses = DE_NULL;
	}

	dependencyCount = static_cast<deUint32>(m_dependiences.size());

	if (dependencyCount) {
		pDependencies = &m_dependiencesStructs[0];
	}
	else
	{
		pDependencies = DE_NULL;
	}
}

void
RenderPassCreateInfo::addAttachment (vk::VkAttachmentDescription attachment)
{

	m_attachments.push_back(attachment);
	m_attachmentsStructs	= std::vector<vk::VkAttachmentDescription>(m_attachments.begin(), m_attachments.end());
	attachmentCount			= static_cast<deUint32>(m_attachments.size());
	pAttachments			= &m_attachmentsStructs[0];
}

void
RenderPassCreateInfo::addSubpass (vk::VkSubpassDescription subpass)
{

	m_subpasses.push_back(subpass);
	m_subpassesStructs	= std::vector<vk::VkSubpassDescription>(m_subpasses.begin(), m_subpasses.end());
	subpassCount		= static_cast<deUint32>(m_subpasses.size());
	pSubpasses			= &m_subpassesStructs[0];
}

void
RenderPassCreateInfo::addDependency (vk::VkSubpassDependency dependency)
{

	m_dependiences.push_back(dependency);
	m_dependiencesStructs	= std::vector<vk::VkSubpassDependency>(m_dependiences.begin(), m_dependiences.end());

	dependencyCount			= static_cast<deUint32>(m_dependiences.size());
	pDependencies			= &m_dependiencesStructs[0];
}

RenderPassBeginInfo::RenderPassBeginInfo (			vk::VkRenderPass				_renderPass,
													vk::VkFramebuffer				_framebuffer,
													vk::VkRect2D					_renderArea,
											const	std::vector<vk::VkClearValue>&	_clearValues)
{

	m_clearValues	= _clearValues;

	sType			= vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pNext			= DE_NULL;
	renderPass		= _renderPass;
	framebuffer		= _framebuffer;
	renderArea		= _renderArea;
	clearValueCount = static_cast<deUint32>(m_clearValues.size());
	pClearValues	= m_clearValues.size() ? &m_clearValues[0] : DE_NULL;
}

CmdPoolCreateInfo::CmdPoolCreateInfo (deUint32 _queueFamilyIndex, unsigned int _flags)
{
	sType = vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO;
	pNext = DE_NULL;

	queueFamilyIndex = _queueFamilyIndex;
	flags			 = _flags;
}

CmdBufferCreateInfo::CmdBufferCreateInfo (vk::VkCmdPool _pool, vk::VkCmdBufferLevel _level, unsigned int _flags)
{
	sType	= vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO;
	pNext	= DE_NULL;
	cmdPool = _pool;
	level	= _level;
	flags	= _flags;
}

AttachmentDescription::AttachmentDescription (	vk::VkFormat			_format,
												deUint32				_samples,
												vk::VkAttachmentLoadOp	_loadOp,
												vk::VkAttachmentStoreOp _storeOp,
												vk::VkAttachmentLoadOp	_stencilLoadOp,
												vk::VkAttachmentStoreOp _stencilStoreOp,
												vk::VkImageLayout		_initialLayout,
												vk::VkImageLayout		_finalLayout)
{
	sType = vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION;
	pNext = DE_NULL;
	format			= _format;
	samples			= _samples;
	loadOp			= _loadOp;
	storeOp			= _storeOp;
	stencilLoadOp	= _stencilLoadOp;
	stencilStoreOp	= _stencilStoreOp;
	initialLayout	= _initialLayout;
	finalLayout		= _finalLayout;
}

AttachmentDescription::AttachmentDescription (const vk::VkAttachmentDescription &rhs)
{

	DE_ASSERT(rhs.sType == vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION);
	DE_ASSERT(rhs.pNext == DE_NULL);

	sType			= rhs.sType;
	pNext			= rhs.pNext;
	format			= rhs.format;
	samples			= rhs.samples;
	loadOp			= rhs.loadOp;
	storeOp			= rhs.storeOp;
	stencilLoadOp	= rhs.stencilLoadOp;
	stencilStoreOp	= rhs.stencilStoreOp;
	initialLayout	= rhs.initialLayout;
	finalLayout		= rhs.finalLayout;
}

AttachmentReference::AttachmentReference (deUint32 _attachment, vk::VkImageLayout _layout)
{
	attachment	= _attachment;
	layout		= _layout;
}

AttachmentReference::AttachmentReference (void)
{
	attachment = vk::VK_ATTACHMENT_UNUSED;
	layout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
}

SubpassDescription::SubpassDescription(			vk::VkPipelineBindPoint			_pipelineBindPoint,
												vk::VkSubpassDescriptionFlags	_flags,
												deUint32						_inputCount,
										const	vk::VkAttachmentReference*		_inputAttachments,
												deUint32						_colorCount,
										const	vk::VkAttachmentReference*		_colorAttachments,
										const	vk::VkAttachmentReference*		_resolveAttachments,
												vk::VkAttachmentReference		_depthStencilAttachment,
												deUint32						_preserveCount,
										const	vk::VkAttachmentReference*		_preserveAttachments)
{
	m_InputAttachments = std::vector<vk::VkAttachmentReference>(_inputAttachments, _inputAttachments + _inputCount);
	m_ColorAttachments = std::vector<vk::VkAttachmentReference>(_colorAttachments, _colorAttachments + _colorCount);

	if (_resolveAttachments)
	{
		m_ResolveAttachments = std::vector<vk::VkAttachmentReference>(_resolveAttachments, _resolveAttachments + _colorCount);
	}

	m_PreserveAttachments = std::vector<vk::VkAttachmentReference>(_preserveAttachments, _preserveAttachments + _preserveCount);

	sType = vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
	pNext = DE_NULL;
	pipelineBindPoint		= _pipelineBindPoint;
	flags					= _flags;
	inputCount				= _inputCount;
	pInputAttachments		= DE_NULL;
	colorCount				= _colorCount;
	pColorAttachments		= DE_NULL;
	pResolveAttachments		= DE_NULL;
	depthStencilAttachment	= _depthStencilAttachment;
	pPreserveAttachments	= DE_NULL;
	preserveCount			= _preserveCount;

	if (m_InputAttachments.size()) {
		pInputAttachments = &m_InputAttachments[0];
	}

	if (m_ColorAttachments.size()) {
		pColorAttachments = &m_ColorAttachments[0];
	}

	if (m_ResolveAttachments.size()) {
		pResolveAttachments = &m_ResolveAttachments[0];
	}

	if (m_PreserveAttachments.size()) {
		pPreserveAttachments = &m_PreserveAttachments[0];
	}
}

SubpassDescription::SubpassDescription (const vk::VkSubpassDescription &rhs)
{

	DE_ASSERT(rhs.sType == vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION);
	DE_ASSERT(rhs.pNext == DE_NULL);

	*static_cast<vk::VkSubpassDescription*>(this) = rhs;

	m_InputAttachments = std::vector<vk::VkAttachmentReference>(
		rhs.pInputAttachments, rhs.pInputAttachments + rhs.inputCount);

	m_ColorAttachments = std::vector<vk::VkAttachmentReference>(
		rhs.pColorAttachments, rhs.pColorAttachments + rhs.colorCount);

	if (rhs.pResolveAttachments) {
		m_ResolveAttachments = std::vector<vk::VkAttachmentReference>(
			rhs.pResolveAttachments, rhs.pResolveAttachments + rhs.colorCount);
	}
	m_PreserveAttachments = std::vector<vk::VkAttachmentReference>(
		rhs.pPreserveAttachments, rhs.pPreserveAttachments + rhs.preserveCount);

	if (m_InputAttachments.size()) {
		pInputAttachments = &m_InputAttachments[0];
	}
	if (m_ColorAttachments.size()) {
		pColorAttachments = &m_ColorAttachments[0];
	}

	if (m_ResolveAttachments.size()) {
		pResolveAttachments = &m_ResolveAttachments[0];
	}

	if (m_PreserveAttachments.size()) {
		pPreserveAttachments = &m_PreserveAttachments[0];
	}
}

SubpassDescription::SubpassDescription (const SubpassDescription &rhs) {
	*this = rhs;
}

SubpassDescription &SubpassDescription::operator= (const SubpassDescription &rhs)
{
	*static_cast<vk::VkSubpassDescription*>(this) = rhs;

	m_InputAttachments		= rhs.m_InputAttachments;
	m_ColorAttachments		= rhs.m_ColorAttachments;
	m_ResolveAttachments	= rhs.m_ResolveAttachments;
	m_PreserveAttachments	= rhs.m_PreserveAttachments;

	if (m_InputAttachments.size()) {
		pInputAttachments = &m_InputAttachments[0];
	}
	if (m_ColorAttachments.size()) {
		pColorAttachments = &m_ColorAttachments[0];
	}

	if (m_ResolveAttachments.size()) {
		pResolveAttachments = &m_ResolveAttachments[0];
	}

	if (m_PreserveAttachments.size()) {
		pPreserveAttachments = &m_PreserveAttachments[0];
	}

	return *this;
}

SubpassDependency::SubpassDependency (	deUint32					_srcSubpass,
										deUint32					_destSubpass,
										vk::VkPipelineStageFlags	_srcStageMask,
										vk::VkPipelineStageFlags	_destStageMask,
										vk::VkMemoryOutputFlags		_outputMask,
										vk::VkMemoryInputFlags		_inputMask,
										vk::VkBool32				_byRegion)
{

	sType = vk::VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY;
	pNext = DE_NULL;
	srcSubpass		= _srcSubpass;
	destSubpass		= _destSubpass;
	srcStageMask	= _srcStageMask;
	destStageMask	= _destStageMask;
	outputMask		= _outputMask;
	inputMask		= _inputMask;
	byRegion		= _byRegion;
}

SubpassDependency::SubpassDependency (const vk::VkSubpassDependency &rhs)
{

	DE_ASSERT(rhs.sType == vk::VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY);
	DE_ASSERT(rhs.pNext == DE_NULL);

	sType = rhs.sType;
	pNext = DE_NULL;
	srcSubpass		= rhs.srcSubpass;
	destSubpass		= rhs.destSubpass;
	srcStageMask	= rhs.srcStageMask;
	destStageMask	= rhs.destStageMask;
	outputMask		= rhs.outputMask;
	inputMask		= rhs.inputMask;
	byRegion		= rhs.byRegion;
}

CmdBufferBeginInfo::CmdBufferBeginInfo (vk::VkCmdBufferOptimizeFlags _flags)
{
	sType = vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO;
	pNext = DE_NULL;
	renderPass	= DE_NULL;
	subpass		= 0;
	framebuffer = DE_NULL;
	flags		= _flags;
}

CmdBufferBeginInfo::CmdBufferBeginInfo (vk::VkRenderPass				_renderPass,
										deUint32						_subpass,
										vk::VkFramebuffer				_framebuffer,
										vk::VkCmdBufferOptimizeFlags	_flags)
{

	sType = vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO;
	pNext = DE_NULL;
	renderPass	= _renderPass;
	subpass		= _subpass;
	framebuffer = _framebuffer;
	flags		= _flags;
}

DescriptorPoolCreateInfo::DescriptorPoolCreateInfo (const	std::vector<vk::VkDescriptorTypeCount>&	typeCounts,
															vk::VkDescriptorPoolUsage				_poolUsage,
															deUint32								_maxSets)
	: m_typeCounts(typeCounts)
{
	sType = vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pNext = DE_NULL;
	poolUsage	= _poolUsage;
	maxSets		= _maxSets;
	count		= static_cast<deUint32>(m_typeCounts.size());
	pTypeCount	= &m_typeCounts[0];
}

DescriptorPoolCreateInfo& DescriptorPoolCreateInfo::addDescriptors (vk::VkDescriptorType type, deUint32 count)
{
	vk::VkDescriptorTypeCount typeCount = { type, count };
	m_typeCounts.push_back(typeCount);

	count		= static_cast<deUint32>(m_typeCounts.size());
	pTypeCount	= &m_typeCounts[0];

	return *this;
}

DescriptorSetLayoutCreateInfo::DescriptorSetLayoutCreateInfo (deUint32 _count, const vk::VkDescriptorSetLayoutBinding* _pBinding)
{
	sType = vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	pNext = DE_NULL;
	count = _count;
	pBinding = _pBinding;
}


PipelineLayoutCreateInfo::PipelineLayoutCreateInfo (deUint32							_descriptorSetCount,
													const	vk::VkDescriptorSetLayout*	_pSetLayouts,
															deUint32					_pushConstantRangeCount,
													const	vk::VkPushConstantRange*	_pPushConstantRanges)
	: m_pushConstantRanges(_pPushConstantRanges, _pPushConstantRanges + _pushConstantRangeCount)
{
	for (unsigned int i = 0; i < _descriptorSetCount; i++)
	{
		m_setLayouts.push_back(_pSetLayouts[i]);
	}

	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pNext = DE_NULL;
	descriptorSetCount		= static_cast<deUint32>(m_setLayouts.size());
	pSetLayouts				= descriptorSetCount > 0 ? &m_setLayouts[0] : DE_NULL;
	pushConstantRangeCount	= static_cast<deUint32>(m_pushConstantRanges.size());

	if (m_pushConstantRanges.size()) {
		pPushConstantRanges = &m_pushConstantRanges[0];
	}
	else
	{
		pPushConstantRanges = DE_NULL;
	}
}

PipelineLayoutCreateInfo::PipelineLayoutCreateInfo (	const	std::vector<vk::VkDescriptorSetLayout>&	setLayouts,
																deUint32								_pushConstantRangeCount,
														const	vk::VkPushConstantRange*				_pPushConstantRanges)
	: m_setLayouts			(setLayouts)
	, m_pushConstantRanges	(_pPushConstantRanges, _pPushConstantRanges + _pushConstantRangeCount)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pNext = DE_NULL;

	descriptorSetCount = static_cast<deUint32>(m_setLayouts.size());

	if (descriptorSetCount)
	{
		pSetLayouts = &m_setLayouts[0];
	}
	else
	{
		pSetLayouts = DE_NULL;
	}

	pushConstantRangeCount = static_cast<deUint32>(m_pushConstantRanges.size());
	if (pushConstantRangeCount) {
		pPushConstantRanges = &m_pushConstantRanges[0];
	}
	else
	{
		pPushConstantRanges = DE_NULL;
	}
}


PipelineCreateInfo::PipelineShaderStage::PipelineShaderStage (vk::VkShader _shader, vk::VkShaderStage _stage)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pNext = DE_NULL;
	stage				= _stage;
	shader				= _shader;
	pSpecializationInfo = DE_NULL;
}

PipelineCreateInfo::VertexInputState::VertexInputState (		deUint32								_bindingCount,
														const	vk::VkVertexInputBindingDescription*	_pVertexBindingDescriptions,
																deUint32								_attributeCount,
														const	vk::VkVertexInputAttributeDescription*	_pVertexAttributeDescriptions)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pNext = DE_NULL;
	bindingCount					= _bindingCount;
	pVertexBindingDescriptions		= _pVertexBindingDescriptions;
	attributeCount					= _attributeCount;
	pVertexAttributeDescriptions	= _pVertexAttributeDescriptions;
}

PipelineCreateInfo::InputAssemblerState::InputAssemblerState (	vk::VkPrimitiveTopology _topology,
																vk::VkBool32			_primitiveRestartEnable)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pNext = DE_NULL;
	topology				= _topology;
	primitiveRestartEnable	= _primitiveRestartEnable;
}

PipelineCreateInfo::TesselationState::TesselationState (deUint32 _patchControlPoints)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	pNext = DE_NULL;
	patchControlPoints = _patchControlPoints;
}

PipelineCreateInfo::ViewportState::ViewportState (	deUint32					_viewportCount,
													std::vector<vk::VkViewport> _viewports,
													std::vector<vk::VkRect2D>	_scissors)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pNext = DE_NULL;
	viewportCount	= _viewportCount;
	scissorCount	= _viewportCount;
	
	if (!_viewports.size())
	{
		m_viewports.resize(viewportCount);
		deMemset(&m_viewports[0], 0, sizeof(m_viewports[0]) * m_viewports.size());
	}
	else
	{
		m_viewports = _viewports;
	}

	if (!_scissors.size())
	{
		m_scissors.resize(scissorCount);
		deMemset(&m_scissors[0], 0, sizeof(m_scissors[0]) * m_scissors.size());
	}
	else
	{
		m_scissors = _scissors;
	}

	pViewports	= &m_viewports[0];
	pScissors	= &m_scissors[0];
}

PipelineCreateInfo::ViewportState::ViewportState (const ViewportState &other)
{
	sType			= other.sType;
	pNext			= other.pNext;
	viewportCount	= other.viewportCount;
	scissorCount	= other.scissorCount;

	m_viewports = std::vector<vk::VkViewport>(other.pViewports, other.pViewports + viewportCount);
	m_scissors	= std::vector<vk::VkRect2D>(other.pScissors, other.pScissors + scissorCount);

	pViewports	= &m_viewports[0];
	pScissors	= &m_scissors[0];
}

PipelineCreateInfo::ViewportState & PipelineCreateInfo::ViewportState::operator= (const ViewportState &other)
{
	sType			= other.sType;
	pNext			= other.pNext;
	viewportCount	= other.viewportCount;
	scissorCount	= other.scissorCount;

	m_viewports		= std::vector<vk::VkViewport>(other.pViewports, other.pViewports + scissorCount);
	m_scissors		= std::vector<vk::VkRect2D>(other.pScissors, other.pScissors + scissorCount);

	pViewports		= &m_viewports[0];
	pScissors		= &m_scissors[0];
	return *this;
}

PipelineCreateInfo::RasterizerState::RasterizerState (	vk::VkBool32	_depthClipEnable,
														vk::VkBool32	_rasterizerDiscardEnable,
														vk::VkFillMode	_fillMode,
														vk::VkCullMode	_cullMode,
														vk::VkFrontFace _frontFace,
														vk::VkBool32	_depthBiasEnable,
														float			_depthBias,
														float			_depthBiasClamp,
														float			_slopeScaledDepthBias,
														float			_lineWidth)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO;
	pNext = DE_NULL;
	depthClipEnable			= _depthClipEnable;
	rasterizerDiscardEnable = _rasterizerDiscardEnable;
	fillMode				= _fillMode;
	cullMode				= _cullMode;
	frontFace				= _frontFace;

	depthBiasEnable			= _depthBiasEnable;
	depthBias				= _depthBias;
	depthBiasClamp			= _depthBiasClamp;
	slopeScaledDepthBias	= _slopeScaledDepthBias;
	lineWidth				= _lineWidth;
}

PipelineCreateInfo::MultiSampleState::MultiSampleState (		deUint32						_rasterSamples,
																vk::VkBool32					_sampleShadingEnable,
																float							_minSampleShading,
														const	std::vector<vk::VkSampleMask>&	_sampleMask)
	: m_sampleMask(_sampleMask)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pNext = DE_NULL;
	rasterSamples		= _rasterSamples;
	sampleShadingEnable = _sampleShadingEnable;
	minSampleShading	= _minSampleShading;
	pSampleMask			= &m_sampleMask[0];
}

PipelineCreateInfo::MultiSampleState::MultiSampleState (const MultiSampleState &other)
{
	sType				= other.sType;
	pNext				= other.pNext;
	rasterSamples		= other.rasterSamples;
	sampleShadingEnable	= other.sampleShadingEnable;
	minSampleShading	= other.minSampleShading;
	
	const size_t sampleMaskArrayLen = (sizeof(vk::VkSampleMask) * 8 + other.rasterSamples) / (sizeof(vk::VkSampleMask) * 8);

	m_sampleMask	= std::vector<vk::VkSampleMask>(other.pSampleMask, other.pSampleMask + sampleMaskArrayLen);
	pSampleMask		= &m_sampleMask[0];
}

PipelineCreateInfo::MultiSampleState& PipelineCreateInfo::MultiSampleState::operator= (const MultiSampleState & other)
{
	sType = other.sType;
	pNext = other.pNext;
	rasterSamples		= other.rasterSamples;
	sampleShadingEnable = other.sampleShadingEnable;
	minSampleShading	= other.minSampleShading;

	const size_t sampleMaskArrayLen = (sizeof(vk::VkSampleMask) * 8 + other.rasterSamples) / (sizeof(vk::VkSampleMask) * 8);

	m_sampleMask	= std::vector<vk::VkSampleMask>(other.pSampleMask, other.pSampleMask + sampleMaskArrayLen);
	pSampleMask		= &m_sampleMask[0];

	return *this;
}

PipelineCreateInfo::ColorBlendState::ColorBlendState (	const	std::vector<vk::VkPipelineColorBlendAttachmentState>&	_attachments,
																vk::VkBool32											_alphaToCoverageEnable,
																vk::VkBool32											_logicOpEnable,
																vk::VkLogicOp											_logicOp,
																vk::VkBool32											_alphaToOneEnable)
	: m_attachments(_attachments)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pNext = DE_NULL; 
	alphaToCoverageEnable	= _alphaToCoverageEnable;
	alphaToOneEnable		= _alphaToOneEnable;
	logicOpEnable			= _logicOpEnable;
	logicOp					= _logicOp;
	attachmentCount			= static_cast<deUint32>(m_attachments.size());
	pAttachments			= &m_attachments[0];
}

PipelineCreateInfo::ColorBlendState::ColorBlendState (			deUint32									_attachmentCount,
														const	vk::VkPipelineColorBlendAttachmentState*	_attachments,
																vk::VkBool32								_alphaToCoverageEnable,
																vk::VkBool32								_logicOpEnable,
																vk::VkLogicOp								_logicOp,
																vk::VkBool32								_alphaToOneEnable)
	: m_attachments(_attachments, _attachments + _attachmentCount)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pNext	= DE_NULL; 
	alphaToCoverageEnable	= _alphaToCoverageEnable;
	alphaToOneEnable		= _alphaToOneEnable;

	logicOpEnable			= _logicOpEnable;
	logicOp					= _logicOp;
	attachmentCount			= _attachmentCount;
	attachmentCount			= static_cast<deUint32>(m_attachments.size());
	pAttachments			= &m_attachments[0];
}

PipelineCreateInfo::ColorBlendState::ColorBlendState (const vk::VkPipelineColorBlendStateCreateInfo &createInfo)
	: m_attachments (createInfo.pAttachments, createInfo.pAttachments + createInfo.attachmentCount)
{
	sType = createInfo.sType;
	pNext = createInfo.pNext;
	alphaToCoverageEnable	= createInfo.alphaToCoverageEnable;
	logicOpEnable			= createInfo.logicOpEnable;
	logicOp					= createInfo.logicOp;
	attachmentCount			= static_cast<deUint32>(m_attachments.size());
	pAttachments			= &m_attachments[0];
}

PipelineCreateInfo::ColorBlendState::ColorBlendState (const ColorBlendState &createInfo, std::vector<float> _blendConst)
	: m_attachments (createInfo.pAttachments, createInfo.pAttachments + createInfo.attachmentCount)
{
	sType = createInfo.sType;
	pNext = createInfo.pNext;
	alphaToCoverageEnable	= createInfo.alphaToCoverageEnable;
	logicOpEnable			= createInfo.logicOpEnable;
	logicOp					= createInfo.logicOp;
	attachmentCount			= static_cast<deUint32>(m_attachments.size());
	pAttachments			= &m_attachments[0];
	deMemcpy(blendConst, &_blendConst[0], 4 * sizeof(float));
}

PipelineCreateInfo::ColorBlendState::Attachment::Attachment (	vk::VkBool32	_blendEnable,
																vk::VkBlend		_srcBlendColor,
																vk::VkBlend		_destBlendColor,
																vk::VkBlendOp	_blendOpColor,
																vk::VkBlend		_srcBlendAlpha,
																vk::VkBlend		_destBlendAlpha,
																vk::VkBlendOp	_blendOpAlpha,
																deUint8			_channelWriteMask)
{
	blendEnable			= _blendEnable;
	srcBlendColor		= _srcBlendColor;
	destBlendColor		= _destBlendColor;
	blendOpColor		= _blendOpColor;
	srcBlendAlpha		= _srcBlendAlpha;
	destBlendAlpha		= _destBlendAlpha;
	blendOpAlpha		= _blendOpAlpha;
	channelWriteMask	= _channelWriteMask;
}

PipelineCreateInfo::DepthStencilState::StencilOpState::StencilOpState (	vk::VkStencilOp _stencilFailOp,
																		vk::VkStencilOp _stencilPassOp,
																		vk::VkStencilOp _stencilDepthFailOp,
																		vk::VkCompareOp _stencilCompareOp,
																		deUint32		_stencilCompareMask,
																		deUint32		_stencilWriteMask,
																		deUint32		_stencilReference)
{
	stencilFailOp		= _stencilFailOp;
	stencilPassOp		= _stencilPassOp;
	stencilDepthFailOp	= _stencilDepthFailOp;
	stencilCompareOp	= _stencilCompareOp;

	stencilCompareMask	= _stencilCompareMask;
	stencilWriteMask	= _stencilWriteMask;
	stencilReference	= _stencilReference;
}

PipelineCreateInfo::DepthStencilState::DepthStencilState (vk::VkBool32 _depthTestEnable,
	vk::VkBool32 _depthWriteEnable,
	vk::VkCompareOp _depthCompareOp,
	vk::VkBool32 _depthBoundsTestEnable,
	vk::VkBool32 _stencilTestEnable,
	StencilOpState _front,
	StencilOpState _back,
	float _minDepthBounds,
	float _maxDepthBounds)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pNext = DE_NULL;
	depthTestEnable			= _depthTestEnable;
	depthWriteEnable		= _depthWriteEnable;
	depthCompareOp			= _depthCompareOp;
	depthBoundsTestEnable	= _depthBoundsTestEnable;
	stencilTestEnable		= _stencilTestEnable;
	front	= _front;
	back	= _back;

	minDepthBounds = _minDepthBounds;
	maxDepthBounds = _maxDepthBounds;
}

PipelineCreateInfo::DynamicState::DynamicState (const std::vector<vk::VkDynamicState>& _dynamicStates)
{
	sType = vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pNext = DE_NULL;

	if (!_dynamicStates.size())
	{
		for (size_t i = 0; i < vk::VK_DYNAMIC_STATE_LAST; ++i)
		{
			m_dynamicStates.push_back(static_cast<vk::VkDynamicState>(i));
		}
	}
	else
		m_dynamicStates = _dynamicStates;

	dynamicStateCount = static_cast<deUint32>(m_dynamicStates.size());
	pDynamicStates = &m_dynamicStates[0];
}

PipelineCreateInfo::DynamicState::DynamicState (const DynamicState &other)
{
	sType = other.sType;
	pNext = other.pNext;

	dynamicStateCount = other.dynamicStateCount;

	m_dynamicStates = std::vector<vk::VkDynamicState>(other.pDynamicStates, other.pDynamicStates + dynamicStateCount);
	pDynamicStates = &m_dynamicStates[0];
}

PipelineCreateInfo::DynamicState & PipelineCreateInfo::DynamicState::operator= (const DynamicState &other)
{
	sType = other.sType;
	pNext = other.pNext;

	dynamicStateCount = other.dynamicStateCount;

	m_dynamicStates = std::vector<vk::VkDynamicState>(other.pDynamicStates, other.pDynamicStates + dynamicStateCount);
	pDynamicStates = &m_dynamicStates[0];

	return *this;
}

PipelineCreateInfo::PipelineCreateInfo (vk::VkPipelineLayout		_layout,
										vk::VkRenderPass			_renderPass,
										int							_subpass,
										vk::VkPipelineCreateFlags	_flags)
{
	memset(static_cast<vk::VkGraphicsPipelineCreateInfo *>(this), 0,
		sizeof(vk::VkGraphicsPipelineCreateInfo));

	sType = vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pNext = DE_NULL;
	renderPass			= _renderPass;
	subpass				= _subpass;

	flags				= _flags;
	layout				= _layout;
	basePipelineHandle	= DE_NULL;
	basePipelineIndex	= 0;
	pDynamicState		= DE_NULL;
}

PipelineCreateInfo& PipelineCreateInfo::addShader (const vk::VkPipelineShaderStageCreateInfo &shader)
{
	m_shaders.push_back(shader);

	stageCount	= static_cast<deUint32>(m_shaders.size());
	pStages		= &m_shaders[0];

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineVertexInputStateCreateInfo& state)
{
	m_vertexInputState = de::SharedPtr<vk::VkPipelineVertexInputStateCreateInfo>(
		new vk::VkPipelineVertexInputStateCreateInfo(state));

	pVertexInputState = m_vertexInputState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineInputAssemblyStateCreateInfo& state)
{
	m_iputAssemblyState = de::SharedPtr<vk::VkPipelineInputAssemblyStateCreateInfo>(
		new vk::VkPipelineInputAssemblyStateCreateInfo(state));

	pInputAssemblyState = m_iputAssemblyState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineColorBlendStateCreateInfo& state)
{
	m_colorBlendStateAttachments = std::vector<vk::VkPipelineColorBlendAttachmentState>(
		state.pAttachments, state.pAttachments + state.attachmentCount);

	m_colorBlendState = de::SharedPtr<vk::VkPipelineColorBlendStateCreateInfo>(
		new vk::VkPipelineColorBlendStateCreateInfo(state));

	m_colorBlendState->pAttachments = &m_colorBlendStateAttachments[0];

	pColorBlendState = m_colorBlendState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineViewportStateCreateInfo& state)
{
	m_viewports = std::vector<vk::VkViewport>(state.pViewports, state.pViewports + state.viewportCount);
	m_scissors	= std::vector<vk::VkRect2D>(state.pScissors, state.pScissors + state.scissorCount);

	m_viewportState = de::SharedPtr<vk::VkPipelineViewportStateCreateInfo>(
		new vk::VkPipelineViewportStateCreateInfo(state));

	m_viewportState->pViewports = &m_viewports[0];
	m_viewportState->pScissors	= &m_scissors[0];

	pViewportState = m_viewportState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineDepthStencilStateCreateInfo& state)
{
	m_dynamicDepthStencilState = de::SharedPtr<vk::VkPipelineDepthStencilStateCreateInfo>(
		new vk::VkPipelineDepthStencilStateCreateInfo(state));

	pDepthStencilState = m_dynamicDepthStencilState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineTessellationStateCreateInfo& state)
{
	m_tessState = de::SharedPtr<vk::VkPipelineTessellationStateCreateInfo>(
		new vk::VkPipelineTessellationStateCreateInfo(state));

	pTessellationState = m_tessState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineRasterStateCreateInfo& state)
{
	m_rasterState	= de::SharedPtr<vk::VkPipelineRasterStateCreateInfo>(new vk::VkPipelineRasterStateCreateInfo(state));
	pRasterState	= m_rasterState.get();

	return *this;
}

PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineMultisampleStateCreateInfo& state)
{

	const size_t sampleMaskArrayLen = (sizeof(vk::VkSampleMask) * 8 + state.rasterSamples) / ( sizeof(vk::VkSampleMask) * 8 );
	m_multisampleStateSampleMask	= std::vector<vk::VkSampleMask>(state.pSampleMask, state.pSampleMask + sampleMaskArrayLen);

	m_multisampleState = de::SharedPtr<vk::VkPipelineMultisampleStateCreateInfo>(new vk::VkPipelineMultisampleStateCreateInfo(state));

	m_multisampleState->pSampleMask = &m_multisampleStateSampleMask[0];
	
	pMultisampleState = m_multisampleState.get();

	return *this;
}
PipelineCreateInfo& PipelineCreateInfo::addState (const vk::VkPipelineDynamicStateCreateInfo& state)
{
	m_dynamicStates = std::vector<vk::VkDynamicState>(state.pDynamicStates, state.pDynamicStates + state.dynamicStateCount);
	m_dynamicState	= de::SharedPtr<vk::VkPipelineDynamicStateCreateInfo>(new vk::VkPipelineDynamicStateCreateInfo(state));

	m_dynamicState->pDynamicStates = &m_dynamicStates[0];

	pDynamicState = m_dynamicState.get();

	return *this;
}

SamplerCreateInfo::SamplerCreateInfo (	vk::VkTexFilter			_magFilter,
										vk::VkTexFilter			_minFilter,
										vk::VkTexMipmapMode		_mipMode,
										vk::VkTexAddressMode	_addressModeU,
										vk::VkTexAddressMode	_addressModeV,
										vk::VkTexAddressMode	_addressModeW,
										float					_mipLodBias,
										float					_maxAnisotropy,
										vk::VkBool32			_compareEnable,
										vk::VkCompareOp			_compareOp,
										float					_minLod,
										float					_maxLod,
										vk::VkBorderColor		_borderColor,
										vk::VkBool32			_unnormalizedCoordinates)
{
	sType					= vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	pNext					= DE_NULL;
	magFilter				= _magFilter;
	minFilter				= _minFilter;
	mipMode					= _mipMode;
	addressModeU			= _addressModeU;
	addressModeV			= _addressModeV;
	addressModeW			= _addressModeW;
	mipLodBias				= _mipLodBias;
	maxAnisotropy			= _maxAnisotropy;
	compareEnable			= _compareEnable;
	compareOp				= _compareOp;
	minLod					= _minLod;
	maxLod					= _maxLod;
	borderColor				= _borderColor;
	unnormalizedCoordinates = _unnormalizedCoordinates;
}

} // DynamicState
} // vkt
