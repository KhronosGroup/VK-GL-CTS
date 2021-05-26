#ifndef _VKTYCBCRUTIL_HPP
#define _VKTYCBCRUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief YCbCr Test Utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"

#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuFloat.hpp"
#include "tcuInterval.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuFloat.hpp"

#include <vector>

namespace vkt
{
namespace ycbcr
{

#define VK_YCBCR_FORMAT_FIRST	VK_FORMAT_G8B8G8R8_422_UNORM
#define VK_YCBCR_FORMAT_LAST	((vk::VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM+1))

typedef de::SharedPtr<vk::Allocation>				AllocationSp;
typedef de::SharedPtr<vk::Unique<vk::VkBuffer> >	VkBufferSp;

class MultiPlaneImageData
{
public:
										MultiPlaneImageData		(vk::VkFormat format, const tcu::UVec2& size);
										MultiPlaneImageData		(const MultiPlaneImageData&);
										~MultiPlaneImageData	(void);

	vk::VkFormat						getFormat				(void) const				{ return m_format;						}
	const vk::PlanarFormatDescription&	getDescription			(void) const				{ return m_description;					}
	const tcu::UVec2&					getSize					(void) const				{ return m_size;						}

	size_t								getPlaneSize			(deUint32 planeNdx) const	{ return m_planeData[planeNdx].size();	}
	void*								getPlanePtr				(deUint32 planeNdx)			{ return &m_planeData[planeNdx][0];		}
	const void*							getPlanePtr				(deUint32 planeNdx) const	{ return &m_planeData[planeNdx][0];		}

	tcu::PixelBufferAccess				getChannelAccess		(deUint32 channelNdx);
	tcu::ConstPixelBufferAccess			getChannelAccess		(deUint32 channelNdx) const;

private:
	MultiPlaneImageData&				operator=				(const MultiPlaneImageData&);

	const vk::VkFormat					m_format;
	const vk::PlanarFormatDescription	m_description;
	const tcu::UVec2					m_size;

	std::vector<deUint8>				m_planeData[vk::PlanarFormatDescription::MAX_PLANES];
};

void										checkImageSupport			(Context& context, vk::VkFormat format, vk::VkImageCreateFlags createFlags, vk::VkImageTiling tiling = vk::VK_IMAGE_TILING_OPTIMAL);

void										fillRandom					(de::Random* randomGen, MultiPlaneImageData* imageData, const vk::VkFormat format = vk::VK_FORMAT_UNDEFINED, bool noNan = false);
void										fillGradient				(MultiPlaneImageData* imageData, const tcu::Vec4& minVal, const tcu::Vec4& maxVal);
void										fillZero					(MultiPlaneImageData* imageData);

std::vector<de::SharedPtr<vk::Allocation> >	allocateAndBindImageMemory	(const vk::DeviceInterface&	vkd,
																		 vk::VkDevice				device,
																		 vk::Allocator&				allocator,
																		 vk::VkImage				image,
																		 vk::VkFormat				format,
																		 vk::VkImageCreateFlags		createFlags,
																		 vk::MemoryRequirement		requirement = vk::MemoryRequirement::Any);

void										uploadImage					(const vk::DeviceInterface&	vkd,
																		 vk::VkDevice				device,
																		 deUint32					queueFamilyNdx,
																		 vk::Allocator&				allocator,
																		 vk::VkImage				image,
																		 const MultiPlaneImageData&	imageData,
																		 vk::VkAccessFlags			nextAccess,
																		 vk::VkImageLayout			finalLayout,
																		 deUint32					arrayLayer = 0u);

void										fillImageMemory				(const vk::DeviceInterface&							vkd,
																		 vk::VkDevice										device,
																		 deUint32											queueFamilyNdx,
																		 vk::VkImage										image,
																		 const std::vector<de::SharedPtr<vk::Allocation> >&	memory,
																		 const MultiPlaneImageData&							imageData,
																		 vk::VkAccessFlags									nextAccess,
																		 vk::VkImageLayout									finalLayout,
																		 deUint32											arrayLayer = 0u);

void										downloadImage				(const vk::DeviceInterface&	vkd,
																		 vk::VkDevice				device,
																		 deUint32					queueFamilyNdx,
																		 vk::Allocator&				allocator,
																		 vk::VkImage				image,
																		 MultiPlaneImageData*		imageData,
																		 vk::VkAccessFlags			prevAccess,
																		 vk::VkImageLayout			initialLayout);

void										readImageMemory				(const vk::DeviceInterface&							vkd,
																		 vk::VkDevice										device,
																		 deUint32											queueFamilyNdx,
																		 vk::VkImage										image,
																		 const std::vector<de::SharedPtr<vk::Allocation> >&	memory,
																		 MultiPlaneImageData*								imageData,
																		 vk::VkAccessFlags									prevAccess,
																		 vk::VkImageLayout									initialLayout);

class ChannelAccess
{
public:
						ChannelAccess	(tcu::TextureChannelClass	channelClass,
										 deUint8					channelSize,
										 const tcu::IVec3&			size,
										 const tcu::IVec3&			bitPitch,
										 void*						data,
										 deUint32					bitOffset);

	const tcu::IVec3&	getSize			(void) const { return m_size; }
	const tcu::IVec3&	getBitPitch		(void) const { return m_bitPitch; }
	void*				getDataPtr		(void) const { return m_data; }

	tcu::Interval		getChannel		(const tcu::FloatFormat&	conversionFormat,
										 const tcu::IVec3&			pos) const;
	deUint32			getChannelUint	(const tcu::IVec3&			pos) const;
	float				getChannel		(const tcu::IVec3&			pos) const;
	void				setChannel		(const tcu::IVec3&			pos,
										 deUint32					x);
	void				setChannel		(const tcu::IVec3&			pos,
										 float						x);

private:
	const tcu::TextureChannelClass	m_channelClass;
	const deUint8					m_channelSize;
	const tcu::IVec3				m_size;
	const tcu::IVec3				m_bitPitch;
	void* const						m_data;
	const deInt32					m_bitOffset;
};

ChannelAccess					getChannelAccess			(ycbcr::MultiPlaneImageData&			data,
															 const vk::PlanarFormatDescription&		formatInfo,
															 const tcu::UVec2&						size,
															 int									channelNdx);

bool							isYChromaSubsampled			(vk::VkFormat							format);

bool							isXChromaSubsampled			(vk::VkFormat							format);

bool							areLsb6BitsDontCare			(vk::VkFormat							srcFormat,
															 vk::VkFormat							dstFormat);

bool							areLsb4BitsDontCare			(vk::VkFormat							srcFormat,
															 vk::VkFormat							dstFormat);

tcu::UVec4						getYCbCrBitDepth			(vk::VkFormat							format);

std::vector<tcu::FloatFormat>	getPrecision				(vk::VkFormat							format);

deUint32						getYCbCrFormatChannelCount	(vk::VkFormat							format);

int								wrap						(vk::VkSamplerAddressMode				addressMode,
															 int									coord,
															 int									size);

int								divFloor					(int									a,
															 int									b);

void							calculateBounds				(const ChannelAccess&					rPlane,
															 const ChannelAccess&					gPlane,
															 const ChannelAccess&					bPlane,
															 const ChannelAccess&					aPlane,
															 const tcu::UVec4&						bitDepth,
															 const std::vector<tcu::Vec2>&			sts,
															 const std::vector<tcu::FloatFormat>&	filteringFormat,
															 const std::vector<tcu::FloatFormat>&	conversionFormat,
															 const deUint32							subTexelPrecisionBits,
															 vk::VkFilter							filter,
															 vk::VkSamplerYcbcrModelConversion		colorModel,
															 vk::VkSamplerYcbcrRange				range,
															 vk::VkFilter							chromaFilter,
															 vk::VkChromaLocation					xChromaOffset,
															 vk::VkChromaLocation					yChromaOffset,
															 const vk::VkComponentMapping&			componentMapping,
															 bool									explicitReconstruction,
															 vk::VkSamplerAddressMode				addressModeU,
															 vk::VkSamplerAddressMode				addressModeV,
															 std::vector<tcu::Vec4>&				minBounds,
															 std::vector<tcu::Vec4>&				maxBounds,
															 std::vector<tcu::Vec4>&				uvBounds,
															 std::vector<tcu::IVec4>&				ijBounds);

} // ycbcr
} // vkt

#endif // _VKTYCBCRUTIL_HPP
