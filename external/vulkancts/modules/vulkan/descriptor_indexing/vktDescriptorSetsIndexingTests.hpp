#ifndef _VKTDESCRIPTORSETSINDEXINGTESTS_HPP
#define _VKTDESCRIPTORSETSINDEXINGTESTS_HPP
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
* \brief Vulkan Descriptor Indexing Tests
*//*--------------------------------------------------------------------*/

#include <vector>
#include <fstream>
#include <iterator>
#include "deSharedPtr.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vktTestCase.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace DescriptorIndexing
{
using namespace vk;

namespace ut
{

struct FrameBuffer;
struct ImageHandleAlloc;
struct BufferHandleAlloc;

typedef de::SharedPtr<FrameBuffer>				FrameBufferSp;
typedef de::SharedPtr<BufferHandleAlloc>		BufferHandleAllocSp;
typedef de::SharedPtr<ImageHandleAlloc>			ImageHandleAllocSp;

typedef de::MovePtr<Allocation>					AllocMv;
typedef de::SharedPtr< Move<VkBufferView> >		BufferViewSp;
typedef de::SharedPtr< Move<VkImageView> >		ImageViewSp;
typedef de::SharedPtr< Move<VkSampler> >		SamplerSp;

static const deUint32 maxDeUint32 = static_cast<deUint32>(-1);

struct ImageHandleAlloc
{
	Move<VkImage>	image;
	AllocMv			alloc;
	VkExtent3D		extent;
	VkFormat		format;
	deUint32		levels;

	bool			usesMipMaps			(void) const { return levels > 0; }

					ImageHandleAlloc	(void)
						: image()
						, alloc() {}

					ImageHandleAlloc	(Move<VkImage>&		image_,
										 AllocMv&			alloc_,
										 const VkExtent3D&	extent_,
										 VkFormat			format_,
										 bool				usesMipMaps_ = false);
private:
					ImageHandleAlloc	(const ImageHandleAlloc&) {}
};

struct FrameBuffer
{
	ImageHandleAllocSp			image;
	Move<VkImageView>			attachment0;
	std::vector<VkImageView>	attachments;
	Move<VkFramebuffer>			buffer;

								FrameBuffer (void)
									: image			()
									, attachment0	()
									, attachments	()
									, buffer		() {}
private:
								FrameBuffer (const FrameBuffer&) {}
};

struct BufferHandleAlloc
{
	Move<VkBuffer>		buffer;
	AllocMv				alloc;

						BufferHandleAlloc	(void)
							: buffer	()
							, alloc		() {}

						BufferHandleAlloc	(Move<VkBuffer>&	buffer_,
											 AllocMv& alloc_)
							: buffer	(buffer_)
							, alloc		(alloc_) {}
private:
	BufferHandleAlloc(const BufferHandleAlloc&) {}
};

std::string				buildShaderName			(VkShaderStageFlagBits			stage,
												VkDescriptorType				descriptorType,
												deBool							updateAfterBind,
												bool							calculateInLoop,
												bool							performWritesInVertex);

std::vector<deUint32>	generatePrimes			(deUint32						limit);

deUint32				computePrimeCount		(deUint32						limit);

deUint32				computeImageSize		(const ImageHandleAllocSp&		image);

deUint32				computeMipMapCount		(const VkExtent3D&				extent);

deUint32				computeImageSize		(const VkExtent3D&				extent,
												 VkFormat						format,
												 bool							withMipMaps	= false,
												 deUint32						level = maxDeUint32);

std::vector<tcu::Vec4>	createVertices			(deUint32						width,
												 deUint32						height,
												 float&							xSize,
												 float&							ySize);

VkDeviceSize			createBufferAndBind		(ut::BufferHandleAllocSp&		output,
												 const vkt::Context&			ctx,
												 VkBufferUsageFlags				usage,
												 VkDeviceSize					desiredSize);

void					createImageAndBind		(ut::ImageHandleAllocSp&		output,
												 const vkt::Context&				ctx,
												 VkFormat						colorFormat,
												 const VkExtent3D&				extent,
												 VkImageLayout					initialLayout,
												 bool							withMipMaps = false,
												 VkImageType					imageType = VK_IMAGE_TYPE_2D);

void					createFrameBuffer		(ut::FrameBufferSp&				outputFB,
												 const vkt::Context&			context,
												 const VkExtent3D&				extent,
												 VkFormat						colorFormat,
												 VkRenderPass					renderpass,
												 deUint32						additionalAttachmentCount = 0u,
												 const VkImageView				additionalAttachments[] = DE_NULL);

void					recordCopyBufferToImage	(VkCommandBuffer				cmd,
												 const DeviceInterface&			interface,
												 VkPipelineStageFlagBits		srcStageMask,
												 VkPipelineStageFlagBits		dstStageMask,
												 const VkDescriptorBufferInfo&	bufferInfo,
												 VkImage						image,
												 const VkExtent3D&				imageExtent,
												 VkFormat						imageFormat,
												 VkImageLayout					oldImageLayout,
												 VkImageLayout					newImageLayout,
												 deUint32						mipLevelCount);

void					recordCopyImageToBuffer	(VkCommandBuffer				cmd,
												 const DeviceInterface&			interface,
												 VkPipelineStageFlagBits		srcStageMask,
												 VkPipelineStageFlagBits		dstStageMask,
												 VkImage						image,
												 const VkExtent3D&				imageExtent,
												 VkFormat						imageFormat,
												 VkImageLayout					oldimageLayout,
												 VkImageLayout					newImageLayout,
												 const VkDescriptorBufferInfo&	bufferInfo);

VkAccessFlags			pipelineAccessFromStage	(VkPipelineStageFlagBits		stage,
												bool							readORwrite);

bool					isDynamicDescriptor		(VkDescriptorType				descriptorType);

class DeviceProperties
{
	VkPhysicalDeviceDescriptorIndexingFeatures			m_descriptorIndexingFeatures;
	VkPhysicalDeviceFeatures2							m_features2;

	VkPhysicalDeviceDescriptorIndexingProperties		m_descriptorIndexingProperties;
	VkPhysicalDeviceProperties2							m_properties2;

public:
	DeviceProperties (const DeviceProperties& src);
	DeviceProperties (const vkt::Context& testContext);

	inline const VkPhysicalDeviceDescriptorIndexingFeatures&		descriptorIndexingFeatures	(void) const;
	inline const VkPhysicalDeviceProperties&						physicalDeviceProperties	(void) const;
	inline const VkPhysicalDeviceDescriptorIndexingProperties&		descriptorIndexingProperties(void) const;
	inline const VkPhysicalDeviceFeatures&							physicalDeviceFeatures		(void) const;

	deUint32 computeMaxPerStageDescriptorCount	(VkDescriptorType	descriptorType,
												 bool				enableUpdateAfterBind,
												 bool				reserveUniformTexelBuffer) const;
};

inline const VkPhysicalDeviceDescriptorIndexingFeatures& DeviceProperties::descriptorIndexingFeatures (void) const
{
	return m_descriptorIndexingFeatures;
}

inline const VkPhysicalDeviceProperties& DeviceProperties::physicalDeviceProperties (void) const
{
	return m_properties2.properties;
}

inline const VkPhysicalDeviceDescriptorIndexingProperties& DeviceProperties::descriptorIndexingProperties (void) const
{
	return m_descriptorIndexingProperties;
}

inline const VkPhysicalDeviceFeatures& DeviceProperties::physicalDeviceFeatures (void) const
{
	return m_features2.features;
}

template<VkFormat _Format> struct VkFormatName
{
	static const VkFormat value = _Format;
};
template<class T> struct mapType2vkFormat;
template<> struct mapType2vkFormat<deUint32>	: public VkFormatName<VK_FORMAT_R32_UINT>{};
template<> struct mapType2vkFormat<tcu::UVec2>	: public VkFormatName<VK_FORMAT_R32G32_UINT>{};
template<> struct mapType2vkFormat<tcu::UVec4>	: public VkFormatName<VK_FORMAT_R32G32B32A32_UINT>{};
template<> struct mapType2vkFormat<tcu::IVec4>	: public VkFormatName<VK_FORMAT_R32G32B32A32_SINT>{};
template<> struct mapType2vkFormat<tcu::Vec2>	: public VkFormatName<VK_FORMAT_R32G32_SFLOAT>{};
template<> struct mapType2vkFormat<tcu::Vec4>	: public VkFormatName<VK_FORMAT_R32G32B32A32_SFLOAT>{};

template<VkFormat _Format> struct mapVkFormat2Type;
template<> struct mapVkFormat2Type<VK_FORMAT_R32_UINT> : public VkFormatName<VK_FORMAT_R32_UINT>
{
	typedef deUint32 type;
};
template<> struct mapVkFormat2Type<VK_FORMAT_R32G32B32A32_SINT> : public VkFormatName<VK_FORMAT_R32G32B32A32_SINT>
{
	typedef tcu::IVec4 type;
};

struct UpdatablePixelBufferAccess : public tcu::PixelBufferAccess
{
	UpdatablePixelBufferAccess (const tcu::TextureFormat& format, const vk::VkExtent3D& extent, void* data)
		: PixelBufferAccess(format, extent.width, extent.height, extent.depth, data)
	{
	}
	virtual ~UpdatablePixelBufferAccess (void) { }
	virtual void invalidate (void) const = 0;
	virtual void fillColor (const tcu::Vec4& color) const = 0;
	static deUint32 calcTexSize (const tcu::TextureFormat& format, const vk::VkExtent3D& extent)
	{
		return extent.width * extent.height * extent.depth * format.getPixelSize();
	}
	static deUint32 calcTexSize (const tcu::TextureFormat& format, deUint32 width, deUint32 height, deUint32 depth)
	{
		return width * height * depth * format.getPixelSize();
	}
};

typedef de::SharedPtr<UpdatablePixelBufferAccess> UpdatablePixelBufferAccessPtr;

struct PixelBufferAccessBuffer : public UpdatablePixelBufferAccess
{
	const VkDevice								m_device;
	const DeviceInterface&						m_interface;
	de::SharedPtr< Move<VkBuffer> >				m_buffer;
	de::SharedPtr< de::MovePtr<Allocation> >	m_allocation;

	PixelBufferAccessBuffer (const VkDevice& device, const DeviceInterface& interface,
		const tcu::TextureFormat& format, const vk::VkExtent3D& extent,
		de::SharedPtr< Move<VkBuffer> > buffer, de::SharedPtr< de::MovePtr<Allocation> > allocation)
		: UpdatablePixelBufferAccess(format, extent, (*allocation)->getHostPtr())
		, m_device(device), m_interface(interface), m_buffer(buffer), m_allocation(allocation)
	{
	}
	void fillColor (const tcu::Vec4&) const { }
	void invalidate (void) const
	{
		const VkDeviceSize		bufferSize = calcTexSize(getFormat(), getWidth(), getHeight(), getDepth());
		vk::invalidateMappedMemoryRange(m_interface, m_device, (*m_allocation)->getMemory(), (*m_allocation)->getOffset(), bufferSize);
	}
};

struct PixelBufferAccessAllocation : public UpdatablePixelBufferAccess
{
	std::vector<unsigned char>					m_data;
	PixelBufferAccessAllocation (const tcu::TextureFormat& format, const VkExtent3D& extent)
		: UpdatablePixelBufferAccess(format, extent, (new unsigned char[calcTexSize(format, extent)]))
		, m_data(static_cast<unsigned char*>(getDataPtr()), (static_cast<unsigned char*>(getDataPtr()) + calcTexSize(format, extent)))
	{
	}
	void invalidate (void) const { /* intentionally empty, only for compability */ }
	void fillColor (const tcu::Vec4& color) const
	{
		tcu::clear(*this, color);
	}
};

template<class K, class V>
static std::ostream& operator<< (std::ostream& s, const std::pair<K, V>& p)
{
	s << "{ " << p.first << ", " << p.second << " } ";
	return s;
}

template<template<class, class> class TCont, class TItem, class TAlloc>
inline void printContainer (std::ostream& s, const std::string& header, const TCont<TItem, TAlloc>& cont)
{
	typename TCont<TItem, TAlloc>::const_iterator i, end = cont.end();
	s << header << '\n';
	for (i = cont.begin(); i != end; ++i)
	{
		s << *i;
	}
	s << '\n';
}

inline void printImage (std::ostream& s, const std::string& header, const tcu::PixelBufferAccess* pa, const deUint32& rgn = 4)
{
	if (header.length())
	{
		s << header << std::endl;
	}
	for (deUint32 r = 0; r < rgn; ++r)
	{
		for (deUint32 c = 0; c < rgn; ++c)
		{
			s << pa->getPixel(c, r) << " (" << r << "," << c << ")\n";
		}
	}
}

inline bool readFile (const std::string& fileName, std::string& content)
{
	bool result = false;
	std::ifstream file(fileName.c_str());

	if (file.is_open())
	{
		file >> std::noskipws;
		content.resize(static_cast<size_t>(file.tellg()));
		content.assign(std::istream_iterator<std::ifstream::char_type>(file),
			std::istream_iterator<std::ifstream::char_type>());
		result = true;
	}

	return result;
}

} // namespace ut
} // namespace DescriptorIndexing
} // namespace vkt

#endif // _VKTDESCRIPTORSETSINDEXINGTESTS_HPP
