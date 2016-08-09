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

#include "vktSampleVerifier.hpp"

#include "deMath.h"
#include "tcuFloat.hpp"
#include "tcuTextureUtil.hpp"
#include "vkImageUtil.hpp"

#include <fstream>
#include <sstream>

namespace vkt
{
namespace texture_filtering
{

using namespace vk;
using namespace tcu;

namespace
{

bool isEqualRelEpsilon (const float a, const float b, const float epsilon)
{
	const float diff = de::abs(a - b);

	const float largest = de::max(de::abs(a), de::abs(b));

	return diff <= largest * epsilon;
}

template <int Size>
bool isEqualRelEpsilon (const Vector<float, Size>& a, const Vector<float, Size>& b, const float epsilon)
{
	for (deUint8 compNdx = 0; compNdx < Size; ++compNdx)
	{
		if (!isEqualRelEpsilon(a[compNdx], b[compNdx], epsilon))
		{
			return false;
		}
	}

	return true;
}

float calcRelEpsilon (VkFormat format, VkFilter filter, VkSamplerMipmapMode mipmapFilter)
{
	// This should take into account the format at some point, but doesn't now
	DE_UNREF(format);

	// fp16 format approximates the minimum precision for internal calculations mandated by spec
	const float fp16MachineEpsilon = 0.0009765625f;

	// \todo [2016-07-06 collinbaker] Pick the epsilon more
	// scientifically
	float relEpsilon = fp16MachineEpsilon;

	if (filter == VK_FILTER_LINEAR)
	{
		relEpsilon *= 3.0f;
	}

	if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
	{
		relEpsilon *= 2.0f;
	}

	return relEpsilon;
}

deInt32 mod (const deInt32 a, const deInt32 n)
{
	const deInt32 result = a % n;

	return (result < 0) ? result + n : result;
}

deInt32 mirror (const deInt32 n)
{
	if (n >= 0)
	{
		return n;
	}
	else
	{
		return -(1 + n);
	}
}

template <int Size>
Vector<float, Size> floor (const Vector<float, Size>& v)
{
	Vector<float, Size> result;

	for (int compNdx = 0; compNdx < Size; ++compNdx)
	{
		result[compNdx] = (float)deFloor(v[compNdx]);
	}

	return result;
}

template <int Size>
Vector<float, Size> ceil (const Vector<float, Size>& v)
{
	Vector<float, Size> result;

	for (int compNdx = 0; compNdx < Size; ++compNdx)
	{
		result[compNdx] = (float)deCeil(v[compNdx]);
	}

	return result;
}

template <int Size>
Vector<float, Size> abs (const Vector<float, Size>& v)
{
	Vector<float, Size> result;

	for (int compNdx = 0; compNdx < Size; ++compNdx)
	{
		result[compNdx] = de::abs(v[compNdx]);
	}

	return result;
}

Vec2 computeLevelLodBounds (const Vec2& lodBounds, deUint8 level)
{
	Vec2 levelLodBounds;

	if (lodBounds[0] <= 0.0f)
	{
		levelLodBounds[0] = lodBounds[0];
	}
	else
	{
		levelLodBounds[0] = de::max(lodBounds[0], (float) level);
	}

	levelLodBounds[1] = de::min(lodBounds[1], (float) level + 1.0f);

	return levelLodBounds;
}

float addUlp (float num, deInt32 ulp)
{
	// Note: adding positive ulp always moves float away from zero

	tcu::Float32 f(num);

	DE_ASSERT(!f.isNaN() && !f.isInf());
	DE_ASSERT(num > FLT_MIN * (float) ulp || num < FLT_MIN * (float) ulp);

	deUint32 bits = f.bits();
	bits += ulp;

	return tcu::Float32(bits).asFloat();
}

deInt32 wrapTexelCoord (const deInt32 coord,
						const deUint32 size,
						const VkSamplerAddressMode wrap)
{
	deInt32 wrappedCoord = 0;

	switch (wrap)
	{
		case VK_SAMPLER_ADDRESS_MODE_REPEAT:
			wrappedCoord = mod(coord, size);
			break;
		case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			wrappedCoord = (size - 1) - mirror(mod(coord, 2 * size) - size);
			break;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			wrappedCoord = de::clamp(coord, 0, (deInt32) size - 1);
			break;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			wrappedCoord = de::clamp(coord, -1, (deInt32) size);
			break;
		case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
			wrappedCoord = de::clamp(mirror(coord), 0, (deInt32) size - 1);
			break;
		default:
			DE_FATAL("Invalid VkSamplerAddressMode");
			break;
	}

	return wrappedCoord;
}

} // anonymous

SampleVerifier::SampleVerifier (const ImageViewParameters&						imParams,
								const SamplerParameters&						samplerParams,
								const SampleLookupSettings&						sampleLookupSettings,
								int												coordBits,
								int												mipmapBits,
								const std::vector<tcu::ConstPixelBufferAccess>&	pba)

	: m_imParams				(imParams)
	, m_samplerParams			(samplerParams)
	, m_sampleLookupSettings	(sampleLookupSettings)
	, m_coordBits				(coordBits)
	, m_mipmapBits				(mipmapBits)
	, m_pba						(pba)
{
	if (m_imParams.dim == IMG_DIM_1D)
	{
		m_unnormalizedDim = 1;
	}
	else if (m_imParams.dim == IMG_DIM_2D || m_imParams.dim == IMG_DIM_CUBE)
	{
		m_unnormalizedDim = 2;
	}
	else
	{
		m_unnormalizedDim = 3;
	}
}

bool SampleVerifier::coordOutOfRange (const IVec3& coord, int compNdx, int level) const
{
	DE_ASSERT(compNdx >= 0 && compNdx < 3);

	return coord[compNdx] < 0 || coord[compNdx] >= m_pba[level].getSize()[compNdx];
}

Vec4 SampleVerifier::fetchTexel (const IVec3& coordIn, deUint32 layer, deUint8 level, VkFilter filter) const
{
	IVec3 coord = coordIn;

	VkSamplerAddressMode wrappingModes[] =
	{
		m_samplerParams.wrappingModeU,
		m_samplerParams.wrappingModeV,
		m_samplerParams.wrappingModeW
	};

	const bool isSrgb = isSrgbFormat(m_imParams.format);

	// Wrapping operations

	if (m_imParams.dim == IMG_DIM_CUBE && filter == VK_FILTER_LINEAR)
	{
		const deUint32	arrayLayer = layer / 6;
		deUint8			arrayFace  = (deUint8) (layer % 6);

		// Cube map adjacent faces ordered clockwise from top

		// \todo [2016-07-07 collinbaker] Verify these are correct
		static const deUint8 adjacentFaces[6][4] =
		{
		    {3, 5, 2, 4},
			{3, 4, 2, 5},
			{4, 0, 5, 1},
			{5, 0, 4, 1},
			{3, 0, 2, 1},
			{3, 1, 2, 0}
		};

		static const deUint8 adjacentEdges[6][4] =
		{
			{1, 3, 1, 1},
			{3, 3, 3, 1},
			{2, 2, 2, 2},
			{0, 0, 0, 0},
			{2, 3, 0, 1},
			{0, 3, 2, 1}
		};

		static const deInt8 adjacentEdgeDirs[6][4] =
		{
			{-1, +1, +1, +1},
			{+1, +1, -1, +1},
			{+1, +1, -1, -1},
			{-1, -1, +1, +1},
			{+1, +1, +1, +1},
			{-1, +1, -1, +1}
		};

		static const deUint8 edgeComponent[4] = {0, 1, 0, 1};

		static const deUint8 edgeFactors[4][2] =
		{
			{0, 0},
			{1, 0},
			{0, 1},
			{0, 0}
		};

		if (coordOutOfRange(coord, 0, level) != coordOutOfRange(coord, 1, level))
		{
			// Handle edge

			deUint8 edgeNdx;

			if (coord[1] < 0)
			{
				edgeNdx = 0;
			}
			else if (coord[0] > 0)
			{
				edgeNdx = 1;
			}
			else if (coord[1] > 0)
			{
				edgeNdx = 2;
			}
			else
			{
				edgeNdx = 3;
			}

			const deUint8	adjacentEdgeNdx = adjacentEdges[arrayFace][edgeNdx];
			const IVec2		edgeFactor		= IVec2(edgeFactors[adjacentEdgeNdx][0],
													edgeFactors[adjacentEdgeNdx][1]);
			const IVec2		edgeOffset		= edgeFactor * (m_pba[level].getSize().swizzle(0, 1) - IVec2(1));

			IVec2 newCoord;

			if (adjacentEdgeDirs[arrayFace][edgeNdx] > 0)
			{
				newCoord[edgeComponent[adjacentEdgeNdx]] = coord[edgeComponent[edgeNdx]];
			}
			else
			{
				newCoord[edgeComponent[adjacentEdgeNdx]] =
					m_pba[level].getSize()[edgeComponent[edgeNdx]] - coord[edgeComponent[edgeNdx]] - 1;
			}

			newCoord[1 - edgeComponent[adjacentEdgeNdx]] = 0;
			coord.xy() = newCoord + edgeOffset;

			arrayFace = adjacentFaces[arrayFace][edgeNdx];
			layer = arrayLayer * 6 + arrayFace;
		}
		else if (coordOutOfRange(coord, 0, level) && coordOutOfRange(coord, 1, level))
		{
			// Handle corner; corners are numbered clockwise starting
			// from top left

			deUint8 cornerNdx;

			if (coord[0] < 0 && coord[1] < 0)
			{
				cornerNdx = 0;
			}
			else if (coord[0] > 0 && coord[1] < 0)
			{
				cornerNdx = 1;
			}
			else if (coord[0] > 0 && coord[1] > 0)
			{
				cornerNdx = 2;
			}
			else
			{
				cornerNdx = 3;
			}

			// Calculate faces and corners thereof adjacent to sampled corner

			const deUint8 cornerEdges[2] = {cornerNdx, (deUint8) ((cornerNdx + 3) % 4)};

		    const deUint8 faces[3]		 = {arrayFace,
											adjacentFaces[arrayFace][cornerEdges[0]],
											adjacentFaces[arrayFace][cornerEdges[1]]};

			deUint8 	  faceCorners[3] = {cornerNdx, 0, 0};

		    for (deUint8 edgeNdx = 0; edgeNdx < 2; ++edgeNdx)
			{
				const deUint8 faceEdge = adjacentEdges[arrayFace][cornerEdges[edgeNdx]];

				bool isFlipped = (adjacentEdgeDirs[arrayFace][cornerEdges[edgeNdx]]);

				if ((cornerEdges[edgeNdx] > 1) != (faceEdge > 1))
				{
					isFlipped = !isFlipped;
				}

				if (isFlipped)
				{
					faceCorners[edgeNdx + 1] = (deUint8) ((faceEdge + 1) % 4);
				}
				else
				{
					faceCorners[edgeNdx + 1] = faceEdge;
				}
			}

			// Compute average of corners and return

			Vec4 result(0.0f);

			for (deUint8 faceNdx = 0; faceNdx < 3; ++faceNdx)
			{
				IVec2 cornerFactor;

				switch (faceCorners[faceNdx])
				{
					case 0:
						cornerFactor = IVec2(0, 0);
						break;
					case 1:
						cornerFactor = IVec2(1, 0);
						break;
					case 2:
						cornerFactor = IVec2(1, 1);
						break;
					case 3:
						cornerFactor = IVec2(0, 1);
				}

				const IVec2 cornerCoord = cornerFactor * (m_pba[level].getSize().swizzle(0, 1) - IVec2(1));
				const deUint32 cornerLayer = arrayLayer * 6 + faces[faceNdx];

				if (isSrgb)
				{
					result += sRGBToLinear(m_pba[level].getPixel(cornerCoord[0], cornerCoord[1], cornerLayer));
				}
				else
				{
					result += m_pba[level].getPixel(cornerCoord[0], cornerCoord[1], cornerLayer);
				}
			}

			result = result / 3.0f;

			return result;
		}
	}
	else
	{
		if (m_imParams.dim == IMG_DIM_CUBE)
		{
			wrappingModes[0] = wrappingModes[1] = wrappingModes[2] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		}

		for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
		{
			const deUint32 size = m_pba[level].getSize()[compNdx];

			coord[compNdx] = wrapTexelCoord(coord[compNdx], size, wrappingModes[compNdx]);
		}
	}

	if (coordOutOfRange(coord, 0, level) ||
		coordOutOfRange(coord, 1, level) ||
		coordOutOfRange(coord, 2, level))
	{
		switch (m_samplerParams.borderColor)
		{
			case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
				return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
			case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
				return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
				return Vec4(1.0f, 1.0f, 1.0f, 1.0f);
			default:
				// \\ [2016-07-07 collinbaker] Handle
				// VK_BORDER_COLOR_INT_* borders
				DE_FATAL("Not implemented");
				break;
		}
	}
	else
	{
		Vec4 result;

	    if (m_imParams.dim == IMG_DIM_1D)
		{
			result = m_pba[level].getPixel(coord[0], layer, 0);
		}
		else if (m_imParams.dim == IMG_DIM_2D)
		{
			result = m_pba[level].getPixel(coord[0], coord[1], layer);
		}
		else
		{
			result = m_pba[level].getPixel(coord[0], coord[1], coord[2]);
		}

		// Do sRGB conversion if necessary

		if (isSrgb)
		{
			return sRGBToLinear(result);
		}
		else
		{
			return result;
		}
	}

	return Vec4(0.0f);
}

Vec4 SampleVerifier::getFilteredSample1D (const IVec3&	texelBase,
										  float			weight,
										  deUint32		layer,
										  deUint8		level) const
{
	Vec4 texels[2];

	for (deUint8 i = 0; i < 2; ++i)
	{
		texels[i] = fetchTexel(texelBase + IVec3(i, 0, 0), layer, level, VK_FILTER_LINEAR);
	}

	return (1.0f - weight) * texels[0] + weight * texels[1];
}


Vec4 SampleVerifier::getFilteredSample2D (const IVec3&	texelBase,
										  const Vec2&	weights,
										  deUint32		layer,
										  deUint8		level) const
{
	Vec4 texels[4];

	for (deUint8 i = 0; i < 2; ++i)
	{
		for (deUint8 j = 0; j < 2; ++j)
		{
			texels[2 * j + i] = fetchTexel(texelBase + IVec3(i, j, 0), layer, level, VK_FILTER_LINEAR);
		}
	}

	return (1.0f - weights[0]) * (1.0f - weights[1]) * texels[0]
		+ weights[0] * (1.0f - weights[1]) * texels[1]
		+ (1.0f - weights[0]) * weights[1] * texels[2]
		+ weights[0] * weights[1] * texels[3];
}

Vec4 SampleVerifier::getFilteredSample3D (const IVec3&	texelBase,
										  const Vec3&	weights,
										  deUint32		layer,
										  deUint8		level) const
{
	Vec4 texels[8];

	for (deUint8 i = 0; i < 2; ++i)
	{
		for (deUint8 j = 0; j < 2; ++j)
		{
			for (deUint8 k = 0; k < 2; ++k)
			{
				texels[4 * k + 2 * j + i] = fetchTexel(texelBase + IVec3(i, j, k), layer, level, VK_FILTER_LINEAR);
			}
		}
	}

	return (1.0f - weights[0]) * (1.0f - weights[1]) * (1.0f - weights[2]) * texels[0]
		+ weights[0] * (1.0f - weights[1]) * (1.0f - weights[2]) * texels[1]
		+ (1.0f - weights[0]) * weights[1] * (1.0f - weights[2]) * texels[2]
		+ weights[0] * weights[1] * (1.0f - weights[2]) * texels[3]
		+ (1.0f - weights[0]) * (1.0f - weights[1]) * weights[2] * texels[4]
		+ weights[0] * (1.0f - weights[1]) * weights[2] * texels[5]
		+ (1.0f - weights[0]) * weights[1] * weights[2] * texels[6]
		+ weights[0] * weights[1] * weights[3] * texels[7];
}

Vec4 SampleVerifier::getFilteredSample (const IVec3&	texelBase,
										const Vec3&		weights,
										deUint32		layer,
										deUint8			level) const
{
	DE_ASSERT(layer < m_imParams.arrayLayers);
	DE_ASSERT(level < m_imParams.levels);

	if (m_imParams.dim == IMG_DIM_1D)
	{
		return getFilteredSample1D(texelBase, weights.x(), layer, level);
	}
	else if (m_imParams.dim == IMG_DIM_2D || m_imParams.dim == IMG_DIM_CUBE)
	{
		return getFilteredSample2D(texelBase, weights.swizzle(0, 1), layer, level);
	}
	else
	{
		return getFilteredSample3D(texelBase, weights, layer, level);
	}
}

void SampleVerifier::getWeightStepBounds (const Vec3&	unnormalizedCoord,
										  IVec3&		weightStepMin,
										  IVec3&		weightStepMax,
										  IVec3&		texelBase) const
{
	DE_ASSERT(m_coordBits < 32);
	const deUint32 coordSteps = ((deUint32) 1) << m_coordBits;

    for (deUint8 compNdx = 0; compNdx < m_unnormalizedDim; ++compNdx)
	{
		const float component = unnormalizedCoord[compNdx];
		double intPart;

		float weight = (float) modf((double) component - 0.5, &intPart);

		if (weight < 0.0f)
		{
			weight = 1.0f + weight;
			intPart -= 1.0;
		}

		texelBase[compNdx] = (deInt32) intPart;

		weightStepMin[compNdx] = deCeilFloatToInt32 (weight * (float) coordSteps - 1.5f);
		weightStepMax[compNdx] = deFloorFloatToInt32(weight * (float) coordSteps + 1.5f);

		weightStepMin[compNdx] = de::max(weightStepMin[compNdx], (deInt32) 0);
		weightStepMax[compNdx] = de::min(weightStepMax[compNdx], (deInt32) coordSteps);
	}
}

void SampleVerifier::getMipmapStepBounds (const Vec2&	lodFracBounds,
										  deInt32&		stepMin,
										  deInt32& 		stepMax) const
{
	DE_ASSERT(m_mipmapBits < 32);
	const deUint32 mipmapSteps = ((deUint32) 1) << m_mipmapBits;

	stepMin = deFloorFloatToInt32(lodFracBounds[0] * (float) mipmapSteps);
	stepMax = deCeilFloatToInt32 (lodFracBounds[1] * (float) mipmapSteps);

	stepMin = de::max(stepMin, (deInt32) 0);
	stepMax = de::min(stepMax, (deInt32) mipmapSteps);
}

bool SampleVerifier::verifySampleFiltered (const Vec4&			result,
										   const Vec3&			unnormalizedCoordHi,
										   const Vec3&			unnormalizedCoordLo,
										   deUint32				layer,
										   deUint8				levelHi,
										   const Vec2&			lodFracBounds,
										   VkFilter				filter,
										   VkSamplerMipmapMode	mipmapFilter,
										   std::ostream&		report) const
{
	DE_ASSERT(layer < m_imParams.arrayLayers);
	DE_ASSERT(levelHi < m_imParams.levels);

	const float		epsilon	   = calcRelEpsilon(m_imParams.format, filter, mipmapFilter);

	const deUint32	coordSteps = ((deUint32) 1) << m_coordBits;
	const deUint32	lodSteps   = ((deUint32) 1) << m_mipmapBits;

	IVec3			texelBase[2];
	IVec3			weightStepsMin[2];
	IVec3			weightStepsMax[2];
	deInt32			lodStepsMin;
	deInt32			lodStepsMax;

	int				levels;
	int				levelLo;

	if (levelHi == m_imParams.levels - 1 || mipmapFilter == VK_SAMPLER_MIPMAP_MODE_NEAREST)
	{
		levels = 1;
		levelLo = levelHi;
		mipmapFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
	else
	{
		levels = 2;
		levelLo = 1 + levelHi;
	}

	getWeightStepBounds(unnormalizedCoordHi, weightStepsMin[0], weightStepsMax[0], texelBase[0]);

	if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
	{
		getWeightStepBounds(unnormalizedCoordLo, weightStepsMin[1], weightStepsMax[1], texelBase[1]);
		getMipmapStepBounds(lodFracBounds, lodStepsMin, lodStepsMax);
	}
	else
	{
		texelBase[1] = IVec3(0);
		weightStepsMin[1] = IVec3(0);
		weightStepsMax[1] = IVec3(0);
	}

	IVec3 weightSteps[2] = {weightStepsMin[0], weightStepsMin[1]};

	bool done = false;

	while (!done)
	{
		report << "Testing at base texel " << texelBase[0] << ", " << texelBase[1] << " with weight steps " << weightSteps[0] << ", " << weightSteps[1] << "\n";

		Vec4 idealSampleHi, idealSampleLo;

		// Get ideal samples at steps at each mipmap level

		if (filter == VK_FILTER_LINEAR)
		{
			Vec3 roundedWeightsHi, roundedWeightsLo;

			roundedWeightsHi = weightSteps[0].asFloat() / (float) coordSteps;
			roundedWeightsLo = weightSteps[1].asFloat() / (float) coordSteps;

			report << "Computed weights: " << roundedWeightsHi << ", " << roundedWeightsLo << "\n";

			idealSampleHi = getFilteredSample(texelBase[0], roundedWeightsHi, layer, levelHi);

			report << "Ideal hi sample: " << idealSampleHi << "\n";

			if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
			{
				idealSampleLo = getFilteredSample(texelBase[1], roundedWeightsLo, layer, (deUint8)levelLo);

				report << "Ideal lo sample: " << idealSampleLo << "\n";
			}
		}
		else
		{
			// \todo [2016-07-14 collinbaker] fix this because this is definitely wrong
			idealSampleHi = fetchTexel(floor(unnormalizedCoordHi).cast<deInt32>(), layer, levelHi, VK_FILTER_NEAREST);

			report << "Ideal hi sample: " << idealSampleHi << "\n";

			if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
			{
				idealSampleLo = fetchTexel(floor(unnormalizedCoordLo).cast<deInt32>(), layer, (deUint8)levelLo, VK_FILTER_NEAREST);

				report << "Ideal lo sample: " << idealSampleLo << "\n";
			}
		}

		// Test ideal samples based on mipmap filtering mode

		if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
		{
			for (deInt32 lodStep = lodStepsMin; lodStep <= lodStepsMax; ++lodStep)
			{
				float weight = (float) lodStep / (float) lodSteps;

				report << "Testing at mipmap weight " << weight << "\n";

				Vec4 idealSample = weight * idealSampleLo + (1.0f - weight) * idealSampleHi;

				report << "Ideal sample: " << idealSample << "\n";

				if (isEqualRelEpsilon(idealSample, result, epsilon))
				{
					return true;
				}
				else
				{
					report << "Failed comparison\n";
				}
			}
		}
		else if (filter == VK_FILTER_LINEAR)
		{
			if (isEqualRelEpsilon(idealSampleHi, result, epsilon))
			{
				return true;
			}
			else
			{
				report << "Failed comparison\n";
			}
		}
		else
		{
			if (idealSampleHi == result)
			{
				return true;
			}
		}

		// Increment step

		// Represents whether the increment at a position wraps and should "carry" to the next place
		bool carry = true;

		for (deUint8 levelNdx = 0; levelNdx < levels; ++levelNdx)
		{
			for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
			{
				if (carry)
				{
					carry = false;
					deInt32& n = weightSteps[levelNdx][compNdx];

					if (n++ == weightStepsMax[levelNdx][compNdx])
					{
						n = weightStepsMin[levelNdx][compNdx];
						carry = true;
					}
				}
				else
				{
					break;
				}
			}

			if (!carry)
			{
				break;
			}
		}

		if (carry)
		{
			done = true;
		}
	}

	report << "Failed comparison against all possible weights\n\n";

	return false;
}

bool SampleVerifier::verifySampleUnnormalizedCoords (const SampleArguments&	args,
													 const Vec4&			result,
													 const Vec3&			unnormalizedCoord,
													 const Vec3&			unnormalizedCoordLo,
													 const Vec2&			lodBounds,
													 deUint8				level,
													 VkSamplerMipmapMode	mipmapFilter,
													 std::ostream&			report) const
{
	const deUint8 layer = m_imParams.isArrayed ? (deUint8) deRoundEven(args.layer) : 0U;

	const bool canBeMinified = lodBounds[1] > 0.0f;
	const bool canBeMagnified = lodBounds[0] <= 0.0f;

	if (canBeMagnified)
	{
		report << "Trying magnification...\n";

		if (m_samplerParams.magFilter == VK_FILTER_NEAREST)
		{
			report << "Testing against nearest texel at " << floor(unnormalizedCoord).cast<deInt32>() << "\n";

			const Vec4 ideal = fetchTexel(floor(unnormalizedCoord).cast<deInt32>(), layer, level, VK_FILTER_NEAREST);

			if (result == ideal)
		    {
				return true;
			}
			else
			{
				report << "Failed against " << ideal << "\n";
			}
		}
		else
		{
			if  (verifySampleFiltered(result, unnormalizedCoord, Vec3(0.0f), layer, level, Vec2(0.0f, 0.0f), VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, report))
				return true;
		}
	}

	if (canBeMinified)
	{
		report << "Trying minification...\n";

		if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
		{
			const Vec2 lodFracBounds = lodBounds - Vec2(level);

			if (verifySampleFiltered(result, unnormalizedCoord, unnormalizedCoordLo, layer, level, lodFracBounds, m_samplerParams.minFilter, VK_SAMPLER_MIPMAP_MODE_LINEAR, report))
				return true;
		}
		else if (m_samplerParams.minFilter == VK_FILTER_LINEAR)
		{
		    if (verifySampleFiltered(result, unnormalizedCoord, Vec3(0.0f), layer, level, Vec2(0.0f, 0.0f), VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, report))
				return true;
		}
		else
		{
			report << "Testing against nearest texel at " << floor(unnormalizedCoord).cast<deInt32>() << "\n";

			Vec4 ideal = fetchTexel(floor(unnormalizedCoord).cast<deInt32>(), layer, level, VK_FILTER_NEAREST);

			if (result == ideal)
		    {
				return true;
			}
			else
			{
				report << "Failed against " << ideal << "\n";
			}
		}
	}

	return false;
}

bool SampleVerifier::verifySampleMipmapLevel (const SampleArguments&	args,
											  const Vec4&				result,
											  const Vec4&				coord,
											  const Vec2&				lodBounds,
											  deUint8					level,
											  std::ostream&				report) const
{
	DE_ASSERT(level < m_imParams.levels);

	VkSamplerMipmapMode mipmapFilter = m_samplerParams.mipmapFilter;

	if (level == m_imParams.levels - 1)
	{
		mipmapFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}

	Vector<double, 3> unnormalizedCoordHiDbl, unnormalizedCoordLoDbl;

	unnormalizedCoordHiDbl = coord.cast<double>().swizzle(0, 1, 2) * m_pba[level].getSize().cast<double>();

	if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
	{
		unnormalizedCoordLoDbl = coord.cast<double>().swizzle(0, 1, 2) * m_pba[level + 1].getSize().cast<double>();
	}

	// Check if result(s) will be rounded

	bool hiIsRounded[3] = {false};
	bool loIsRounded[3] = {false};

	for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
	{
		hiIsRounded[compNdx] = ((double) (float) unnormalizedCoordHiDbl[compNdx]) != unnormalizedCoordHiDbl[compNdx];

		if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
		{
			loIsRounded[compNdx] = ((double) (float) unnormalizedCoordLoDbl[compNdx]) != unnormalizedCoordLoDbl[compNdx];
		}
	}

	const deInt32 ulpEpsilon	= (deInt32) (2.0e-5f / FLT_EPSILON);
	const deInt32 ulpOffsets[3] = {0, -ulpEpsilon, ulpEpsilon};

	deUint8 roundTypesHi[3] = {0, 0, 0};
	deUint8 roundTypesLo[3] = {0, 0, 0};

	bool done = false;

	// Take into account different possible rounding modes by offsetting rounded result by ULPs
	// \todo [2016-07-15 collinbaker] This is not 100% correct; should simulate floating point arithmetic with possible rounding modes
    while (!done)
	{
		Vec3 unnormalizedCoordHi;
		Vec3 unnormalizedCoordLo;

		for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
		{
			float comp = coord[compNdx];
			float compHi;
			float compLo;

			if (roundTypesHi[compNdx] != 0 && comp > FLT_MIN * (float) ulpEpsilon)
			{
				compHi = addUlp(comp, ulpOffsets[roundTypesHi[compNdx]]);
			}
			else
			{
				compHi = comp;
			}

			if (roundTypesLo[compNdx] != 0 && comp > FLT_MIN * (float) ulpEpsilon)
			{
				compLo = addUlp(comp, ulpOffsets[roundTypesLo[compNdx]]);
			}
			else
			{
				compLo = comp;
			}

			unnormalizedCoordHi[compNdx] = compHi * (float) m_pba[level].getSize()[compNdx];

			if (mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
			{
			    unnormalizedCoordLo[compNdx] = compLo * (float) m_pba[level + 1].getSize()[compNdx];
			}
		}

		report << "Testing at " << unnormalizedCoordHi << ", " << unnormalizedCoordLo << "\n";

		if (verifySampleUnnormalizedCoords(args, result, unnormalizedCoordHi, unnormalizedCoordLo, lodBounds, level, mipmapFilter, report))
			return true;

		// Increment rounding types

		bool carry = true;

		for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
		{
			if (hiIsRounded[compNdx])
			{
				if (carry)
				{
					if (roundTypesHi[compNdx] == 2)
					{
						roundTypesHi[compNdx] = 0;
					}
					else
					{
						roundTypesHi[compNdx]++;
						carry = false;
					}
				}
				else
				{
					break;
				}
			}
		}

		for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
		{
			if (loIsRounded[compNdx])
			{
				if (carry)
				{
					if (roundTypesLo[compNdx] == 2)
					{
						roundTypesLo[compNdx] = 0;
					}
					else
					{
						roundTypesLo[compNdx]++;
						carry = false;
					}
				}
				else
				{
					break;
				}
			}
		}

		if (carry)
		{
			done = true;
		}
	}

	return false;
}

bool SampleVerifier::verifySampleCubemapFace (const SampleArguments&	args,
											  const Vec4&				result,
											  const Vec4&				coord,
											  const Vec4&				dPdx,
											  const Vec4&				dPdy,
											  deUint8					face,
											  std::ostream&				report) const
{
	// Will use this parameter once cubemapping is implemented completely
	DE_UNREF(face);

	Vec2 lodBounds;

	if (m_sampleLookupSettings.lookupLodMode == LOOKUP_LOD_MODE_DERIVATIVES)
	{
		const Vec3 mx = abs(dPdx.swizzle(0, 1, 2)) * m_imParams.size.asFloat();
		const Vec3 my = abs(dPdy.swizzle(0, 1, 2)) * m_imParams.size.asFloat();

		Vec2 scaleXBounds;
		Vec2 scaleYBounds;

		scaleXBounds[0] = de::max(de::abs(mx[0]), de::max(de::abs(mx[1]), de::abs(mx[2])));
		scaleYBounds[0] = de::max(de::abs(my[0]), de::max(de::abs(my[1]), de::abs(my[2])));

		scaleXBounds[1] = de::abs(mx[0]) + de::abs(mx[1]) + de::abs(mx[2]);
		scaleYBounds[1] = de::abs(my[0]) + de::abs(my[1]) + de::abs(my[2]);

		Vec2 scaleMaxBounds;

		for (deUint8 compNdx = 0; compNdx < 2; ++compNdx)
		{
			scaleMaxBounds[compNdx] = de::max(scaleXBounds[compNdx], scaleYBounds[compNdx]);
		}

		float lodBias = m_samplerParams.lodBias;

		if (m_sampleLookupSettings.hasLodBias)
			lodBias += args.lodBias;

		for (deUint8 compNdx = 0; compNdx < 2; ++compNdx)
		{
			lodBounds[compNdx] = deFloatLog2(scaleMaxBounds[compNdx]);
			lodBounds[compNdx] += lodBias;
			lodBounds[compNdx] = de::clamp(lodBounds[compNdx], m_samplerParams.minLod, m_samplerParams.maxLod);
		}
	}
	else
	{
		lodBounds[0] = lodBounds[1] = args.lod;
	}

	DE_ASSERT(lodBounds[0] <= lodBounds[1]);

    const float q = (float) (m_imParams.levels - 1);

	if (m_samplerParams.mipmapFilter == VK_SAMPLER_MIPMAP_MODE_NEAREST)
	{
		UVec2 levelBounds;

	    if (lodBounds[0] <= 0.5f)
		{
			levelBounds[0] = 0;
		}
		else if (lodBounds[0] < q + 0.5f)
		{
			levelBounds[0] = deCeilFloatToInt32(lodBounds[0] + 0.5f) - 1;
		}
		else
		{
			levelBounds[0] = deRoundFloatToInt32(q);
		}

		if (lodBounds[1] < 0.5f)
		{
			levelBounds[1] = 0;
		}
		else if (lodBounds[1] < q + 0.5f)
		{
			levelBounds[1] = deFloorFloatToInt32(lodBounds[1] + 0.5f);
		}
		else
		{
			levelBounds[1] = deRoundFloatToInt32(q);
		}

		for (deUint8 level = (deUint8) levelBounds[0]; level <= (deUint8) levelBounds[1]; ++level)
		{
			const Vec2 levelLodBounds = computeLevelLodBounds(lodBounds, level);

			if (verifySampleMipmapLevel(args, result, coord, levelLodBounds, level, report))
			{
				return true;
			}
		}
	}
	else
	{
		UVec2 levelBounds;

		for (deUint8 compNdx = 0; compNdx < 2; ++compNdx)
		{
			if (lodBounds[compNdx] >= q)
			{
				levelBounds[compNdx] = deRoundFloatToInt32(q);
			}
			else
			{
				levelBounds[compNdx] = lodBounds[compNdx] < 0.0f ? 0 : deFloorFloatToInt32(lodBounds[compNdx]);
			}
		}

		for (deUint8 level = (deUint8) levelBounds[0]; level <= (deUint8) levelBounds[1]; ++level)
		{
			const Vec2 levelLodBounds = computeLevelLodBounds(lodBounds, level);

			if (verifySampleMipmapLevel(args, result, coord, levelLodBounds, level, report))
			{
				return true;
			}
		}
	}

	return false;
}

bool SampleVerifier::verifySampleImpl (const SampleArguments&	args,
									   const Vec4&				result,
									   std::ostream&			report) const
{
	// \todo [2016-07-11 collinbaker] Handle depth and stencil formats
	// \todo [2016-07-06 collinbaker] Handle dRef
	DE_ASSERT(m_samplerParams.isCompare == false);

	Vec4 coord = args.coord;
	deUint8 coordSize = 0;

	if (m_imParams.dim == IMG_DIM_1D)
	{
		coordSize = 1;
	}
	else if (m_imParams.dim == IMG_DIM_2D)
	{
		coordSize = 2;
	}
	else if (m_imParams.dim == IMG_DIM_3D || m_imParams.dim == IMG_DIM_CUBE)
	{
		coordSize = 3;
	}

	// 15.6.1 Project operation

	if (m_sampleLookupSettings.isProjective)
	{
		DE_ASSERT(args.coord[coordSize] != 0.0f);
		const float proj = coord[coordSize];

		coord = coord / proj;
	}

	const Vec4 dPdx = (m_sampleLookupSettings.lookupLodMode == LOOKUP_LOD_MODE_DERIVATIVES) ? args.dPdx : Vec4(0);
	const Vec4 dPdy = (m_sampleLookupSettings.lookupLodMode == LOOKUP_LOD_MODE_DERIVATIVES) ? args.dPdy : Vec4(0);

	// 15.6.3 Cube Map Face Selection and Transformations

	if (m_imParams.dim == IMG_DIM_CUBE)
	{
		const Vec3 r = coord.swizzle(0, 1, 2);
		const Vec3 drdx = dPdx.swizzle(0, 1, 2);
		const Vec3 drdy = dPdy.swizzle(0, 1, 2);

		BVec3 isMajor(false);
		float rMax = de::abs(r[0]);

		for (deUint8 compNdx = 1; compNdx < 3; ++compNdx)
		{
			rMax = de::max(rMax, de::abs(r[compNdx]));
		}

		for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
		{
			if (de::abs(r[compNdx]) == rMax)
			{
				isMajor[compNdx] = true;
			}
		}

		DE_ASSERT(isMajor[0] || isMajor[1] || isMajor[2]);

		// We must test every possible disambiguation order

		for (deUint8 i = 0; i < 3; ++i)
		{
		    if (!isMajor[i])
			{
				continue;
			}

			const deUint8 faceNdx = (deUint8) (2U * i + (r[i] < 0.0f ? 1U : 0U));

			const deUint8 compMap[6][3] =
			{
				{2, 1, 0},
				{2, 1, 0},
				{0, 2, 1},
				{0, 2, 1},
				{0, 1, 2},
				{0, 1, 2}
			};

			const deInt8 signMap[6][3] =
			{
				{-1, -1, +1},
				{+1, -1, -1},
				{+1, +1, +1},
				{+1, -1, -1},
				{+1, -1, +1},
				{-1, -1, -1}
			};

			Vec3 coordC;
			Vec3 dPcdx;
			Vec3 dPcdy;

			for (deUint8 compNdx = 0; compNdx < 3; ++compNdx)
			{
				const deUint8	mappedComp = compMap[faceNdx][compNdx];
				const deInt8	mappedSign = signMap[faceNdx][compNdx];

				coordC[compNdx] = r[mappedComp]		* mappedSign;
				dPcdx[compNdx]	= drdx[mappedComp]	* mappedSign;
				dPcdy[compNdx]	= drdy[mappedComp]	* mappedSign;
			}

			coordC[2] = de::abs(coordC[2]);

			Vec4 coordFace;
			Vec4 dPdxFace;
			Vec4 dPdyFace;

			for (deUint8 compNdx = 0; compNdx < 2; ++compNdx)
			{
				coordFace[compNdx] = 0.5f * coordC[compNdx] / de::abs(coordC[2]) + 0.5f;

				dPdxFace[compNdx]  = 0.5f * (de::abs(coordC[2]) * dPcdx[compNdx] - coordC[compNdx] * dPcdx[2]) / (coordC[2] * coordC[2]);
				dPdyFace[compNdx]  = 0.5f * (de::abs(coordC[2]) * dPcdy[compNdx] - coordC[compNdx] * dPcdy[2]) / (coordC[2] * coordC[2]);
			}

			for (deUint8 compNdx = 2; compNdx < 4; ++compNdx)
			{
				coordFace[compNdx] = dPdxFace[compNdx] = dPdyFace[compNdx] = 0.0f;
			}

			if (verifySampleCubemapFace(args, result, coordFace, dPdxFace, dPdyFace, faceNdx, report))
				return true;
		}

		return false;
	}
	else
	{
		return verifySampleCubemapFace(args, result, coord, dPdx, dPdy, 0, report);
	}
}

bool SampleVerifier::verifySampleReport (const SampleArguments&	args,
										 const Vec4&			result,
										 std::string& 			report) const
{
	std::ostringstream reportStream;

	const bool isValid = verifySampleImpl(args, result, reportStream);

	report = reportStream.str();

    return isValid;
}

bool SampleVerifier::verifySample (const SampleArguments&	args,
								   const Vec4&				result) const
{
	// Create unopened ofstream to simulate "null" ostream
	std::ofstream nullStream;

	return verifySampleImpl(args, result, nullStream);
}

} // texture_filtering
} // vkt
