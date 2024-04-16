#ifndef _VKTDEMUXER_HPP
#define _VKTDEMUXER_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Igalia S. L
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
 */
/*!
 * \file
 * \brief Elementary stream demuxers
 */
/*--------------------------------------------------------------------*/

#include <vector>
#include <memory>

#include "deDefs.h"

#include "vkDefs.hpp"

#include "vktBufferedReader.hpp"

namespace vkt
{
namespace video
{
enum class ElementaryStreamFraming
{
	IVF,
	H26X_BYTE_STREAM,
	AV1_ANNEXB,

	UNKNOWN
};

class Demuxer
{
public:
	virtual ~Demuxer () {}

	struct Params
	{
		std::unique_ptr<BufferedReader>			data;
		vk::VkVideoCodecOperationFlagBitsKHR	codecOperation;
		ElementaryStreamFraming				framing;
	};

	static std::shared_ptr<Demuxer> create(Params&& params);

	vk::VkVideoCodecOperationFlagBitsKHR codecOperation() const { return m_params.codecOperation; };
	ElementaryStreamFraming framing() const { return m_params.framing; }

	virtual std::vector<uint8_t> nextPacket() = 0;

protected:
	Demuxer(Params&& params);
	Params m_params;
};

class H26XAnnexBDemuxer final : public Demuxer
{
public:
	H26XAnnexBDemuxer(Params&& params);

	virtual std::vector<uint8_t> nextPacket() override;

private:
	void readToNextStartCode(std::vector<uint8_t>* payload);
};

class DuckIVFDemuxer final : public Demuxer
{
public:
	DuckIVFDemuxer(Params&& params);
	virtual std::vector<uint8_t> nextPacket() override;

	DE_PACKED(Header {
		uint32_t signature;				// bytes 0-3	signature: 'DKIF'
		uint16_t version;				// bytes 4-5	version (should be 0)
		uint16_t hdrLength;				// bytes 6-7	length of header in bytes
		uint32_t fourcc;				// bytes 8-11	codec FourCC (e.g., 'VP80')
		uint16_t widthInPixels;			// bytes 12-13	width in pixels
		uint16_t heightInPixels;		// bytes 14-15	height in pixels
		uint32_t timeBaseDenominator;	// bytes 16-19	time base denominator
		uint32_t timeBaseNumerator;		// bytes 20-23	time base numerator
		uint32_t framesInFile;			// bytes 24-27	number of frames in file
		uint32_t padding;
	} Header);

private:
	void readHeader();

	Header m_hdr;

	int32_t m_frameNumber{ -1 };
	uint32_t m_numFrames{ 0 };
};

class AV1AnnexBDemuxer final : public Demuxer
{
public:
	AV1AnnexBDemuxer(Params&& params);

	virtual std::vector<uint8_t> nextPacket() override;

private:
	size_t		m_remainingBytesInTemporalUnit{ 0 };
	int32_t	m_frameNumber{ -1 };


	uint32_t	getUleb128(uint32_t *numBytes);
};


} // namespace video
} // namespace vkt

#endif // _VKTDEMUXER_HPP
