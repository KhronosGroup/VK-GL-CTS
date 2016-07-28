#ifndef _VKTSAMPLEVERIFIER_HPP
#define _VKTSAMPLEVERIFIER_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief GPU image sample verification
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "deUniquePtr.hpp"

#include "tcuTexture.hpp"
#include "tcuVector.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace vkt
{
namespace texture_filtering
{

struct SampleArguments
{
	tcu::Vec4	coord;
	tcu::Vec4	dPdx;
	tcu::Vec4	dPdy;
	float		layer;
	float		lod;
	float		lodBias;
	float		dRef;
};

enum LookupLodMode
{
	LOOKUP_LOD_MODE_DERIVATIVES = 0,
	LOOKUP_LOD_MODE_LOD,

	LOOKUP_LOD_MODE_LAST
};

struct SampleLookupSettings
{
	LookupLodMode	lookupLodMode;
	bool			hasLodBias;
	bool			isProjective;
};

enum WrappingMode
{
	WRAPPING_MODE_REPEAT = 0,
	WRAPPING_MODE_MIRRORED_REPEAT,
	WRAPPING_MODE_CLAMP_TO_EDGE,
	WRAPPING_MODE_CLAMP_TO_BORDER,
	WRAPPING_MODE_MIRROR_CLAMP_TO_EDGE,

	WRAPPING_MODE_LAST
};

struct SamplerParameters
{
	vk::VkFilter				magFilter;
	vk::VkFilter				minFilter;
	vk::VkSamplerMipmapMode		mipmapFilter;

	vk::VkSamplerAddressMode	wrappingModeU;
	vk::VkSamplerAddressMode	wrappingModeV;
	vk::VkSamplerAddressMode	wrappingModeW;

	vk::VkBorderColor			borderColor;

	float						lodBias;
	float						minLod;
	float						maxLod;

	bool						isUnnormalized;
	bool						isCompare;
};

enum ImgDim
{
	IMG_DIM_INVALID = 0,
	IMG_DIM_1D,
	IMG_DIM_2D,
	IMG_DIM_3D,
	IMG_DIM_CUBE,

	IMG_DIM_LAST
};

struct ImageViewParameters
{
	ImgDim			dim;
	vk::VkFormat	format;
	tcu::IVec3		size;
	deUint8			levels;

	bool			isArrayed;
	deUint32		arrayLayers;
};

class SampleVerifier
{
public:
	SampleVerifier						(const ImageViewParameters&							imParams,
										 const SamplerParameters&							samplerParams,
										 const SampleLookupSettings&						sampleLookupSettings,
										 int												coordBits,
										 int												mipmapBits,
										 const std::vector<tcu::ConstPixelBufferAccess>&	pba);

	bool verifySample					(const SampleArguments&								args,
										 const tcu::Vec4&									result) const;

	bool verifySampleReport				(const SampleArguments&								args,
										 const tcu::Vec4&									result,
										 std::string&										report) const;

private:
	bool verifySampleMipmapLinear		(tcu::Vec4											result,
										 tcu::Vec4											sampleHi,
										 tcu::Vec4											sampleLo,
										 deInt32											lodStepMin,
										 deInt32											lodStepMax,
										 deUint32											layer,
										 deUint8											levelHi) const;

	bool verifySampleFiltered			(const tcu::Vec4&									result,
										 const tcu::Vec3&									unnormalizedCoordHi,
										 const tcu::Vec3&									unnormalizedCoordLo,
										 deUint32											layer,
										 deUint8											levelHi,
										 const tcu::Vec2&									lodFracBounds,
										 vk::VkFilter										filter,
										 vk::VkSamplerMipmapMode							mipmapFilter,
										 std::ostream&										report) const;

	bool verifySampleUnnormalizedCoords (const SampleArguments&								args,
										 const tcu::Vec4&									result,
										 const tcu::Vec3&									unnormalizedCoord,
										 const tcu::Vec3&									unnormalizedCoordLo,
										 const tcu::Vec2&									lodBounds,
										 deUint8											level,
										 vk::VkSamplerMipmapMode							mipmapFilter,
										 std::ostream&										report) const;

	bool verifySampleMipmapLevel		(const SampleArguments&								args,
										 const tcu::Vec4&									result,
										 const tcu::Vec4&									coord,
										 const tcu::Vec2&									lodFracBounds,
										 deUint8											level,
										 std::ostream&										report) const;

	bool verifySampleCubemapFace		(const SampleArguments&								args,
										 const tcu::Vec4&									result,
										 const tcu::Vec4&									coord,
										 const tcu::Vec4&									dPdx,
										 const tcu::Vec4&									dPdy,
										 deUint8											face,
										 std::ostream&										report) const;

	bool verifySampleImpl				(const SampleArguments&								args,
										 const tcu::Vec4&									result,
										 std::ostream&										report) const;

	bool coordOutOfRange				(const tcu::IVec3&									coord,
										 int												compNdx,
										 int 												level) const;

	tcu::Vec4 fetchTexel				(const tcu::IVec3&									coordIn,
										 deUint32											layer,
										 deUint8											level,
										 vk::VkFilter										filter) const;

	tcu::Vec4 getFilteredSample1D		(const tcu::IVec3&									texelBase,
										 float												weight,
										 deUint32											layer,
										 deUint8											level) const;

	tcu::Vec4 getFilteredSample2D		(const tcu::IVec3&									texelBase,
										 const tcu::Vec2&									weights,
										 deUint32											layer,
										 deUint8											level) const;

	tcu::Vec4 getFilteredSample3D		(const tcu::IVec3&									texelBase,
										 const tcu::Vec3&									weights,
										 deUint32											layer,
										 deUint8											level) const;

	tcu::Vec4 getFilteredSample			(const tcu::IVec3&									texelBase,
										 const tcu::Vec3&									weights,
										 deUint32											layer,
										 deUint8											level) const;

	void getWeightStepBounds			(const tcu::Vec3&									unnormalizedCoord,
										 tcu::IVec3&										weightStepMin,
										 tcu::IVec3&										weightStepMax,
										 tcu::IVec3&										texelBase) const;

	void getMipmapStepBounds			(const tcu::Vec2&									lodFracBounds,
										 deInt32&											stepMin,
										 deInt32&											stepMax) const;

	const ImageViewParameters&						m_imParams;
	const SamplerParameters&						m_samplerParams;
	const SampleLookupSettings&						m_sampleLookupSettings;

    int												m_coordBits;
	int												m_mipmapBits;

	int												m_unnormalizedDim;

	const std::vector<tcu::ConstPixelBufferAccess>&	m_pba;
};

} // texture_filtering
} // vkt

#endif // _VKTSAMPLEVERIFIER_HPP
