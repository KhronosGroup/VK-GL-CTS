/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Igalia S.L
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
#include "deMemory.h"

#include "vktVideoTestUtils.hpp"
#include "vktDemuxer.hpp"

#include <algorithm>
#include <limits>

namespace vkt
{
namespace video
{

std::unique_ptr<Demuxer> Demuxer::create(Params&& params)
{
	switch (params.codecOperation)
	{
	case vk::VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
	case vk::VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
	{
		return std::make_unique<H26XAnnexBDemuxer>(std::move(params));
	} break;
	case vk::VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
	{
		if (params.framing == ElementaryStreamFraming::AV1_ANNEXB)
			return std::make_unique<AV1AnnexBDemuxer>(std::move(params));
		else if (params.framing == ElementaryStreamFraming::IVF)
			return std::make_unique<DuckIVFDemuxer>(std::move(params));
		else
			TCU_THROW(InternalError, "unknown elementary stream framing");
	} break;
	default:
		TCU_THROW(InternalError, "Unknown codec operation");
	}
}

Demuxer::Demuxer(Params&& params)
	: m_params(std::move(params))
{
}

H26XAnnexBDemuxer::H26XAnnexBDemuxer(Params&& params)
	: Demuxer(std::move(params))
{
	readToNextStartCode(nullptr);
}

std::vector<uint8_t> H26XAnnexBDemuxer::nextPacket()
{
	std::vector<uint8_t> packet;
	readToNextStartCode(&packet);
	return packet;
}

// Very inefficient but simple algorithm, which is fine for the CTS
// since it can expect never to have to deal with inputs larger than a
// couple of megabytes.  A ~20x time boost would be mapping the
// bitstreams into memory and using the std::boyer_moore algorithm to
// find the start codes, at the cost of extra complexity handling
// corner cases in file mapping and low-memory environments.
void H26XAnnexBDemuxer::readToNextStartCode(std::vector<uint8_t>* payload)
{
	int zeroRunCount = 0;
	auto& reader = m_params.data;
	uint8_t byte;

	if (reader->isEof() || reader->isError())
		return;

	if (payload)
	{
		// TODO: Fix the sample parser's interface
		payload->push_back(0x0);
		payload->push_back(0x0);
		payload->push_back(0x0);
		payload->push_back(0x1);
	}

	while (true)
	{
		byte = reader->readByteChecked("failure looking for H26X start code");
		if  (reader->isEof() || reader->isError())
			return;

		if (byte == 0x01 && zeroRunCount >= 3)
		{
			return;
		}
		else if (byte == 0x00)
		{
			zeroRunCount++;
		}
		else
		{
			while (zeroRunCount > 0)
			{
				zeroRunCount--;
				payload->push_back(0x0);
			}
			payload->push_back(byte);
		}
	}
}

DuckIVFDemuxer::DuckIVFDemuxer(Params&& params)
	: Demuxer(std::move(params))
	, m_frameNumber(0)
{
	readHeader();
}

DE_PACKED(DuckIVFFrameHeader {
	uint32_t sizeOfFrame;				// bytes 0-3	size of frame in bytes (not including the 12-byte header)
	uint64_t presentationTimestamp;	// bytes 4-11	64-bit presentation timestamp
} DuckIVFFrameHeader);

std::vector<uint8_t> DuckIVFDemuxer::nextPacket()
{
	auto& reader = m_params.data;

	DuckIVFFrameHeader frameHdr;
	reader->readChecked((uint8_t *)&frameHdr, sizeof(DuckIVFFrameHeader), "error reading Duck IVF frame header");

	std::vector<uint8_t> packet(frameHdr.sizeOfFrame);
	reader->readChecked(packet.data(), frameHdr.sizeOfFrame, "error reading Duck IVF frame");

	m_frameNumber++;

	DE_ASSERT(packet.size() > 0);

	return packet;
}

void DuckIVFDemuxer::readHeader()
{
	auto& reader = m_params.data;

	DE_ASSERT(!reader->isError() && !reader->isEof());

	reader->readChecked((uint8_t *)&m_hdr, sizeof(DuckIVFDemuxer::Header), "invalid Duck IVF header");

	static uint8_t kDuckIVFSignature[4] = {'D', 'K', 'I', 'F'};
	if (deMemCmp(&m_hdr.signature, kDuckIVFSignature, sizeof(kDuckIVFSignature)) != 0)
		TCU_THROW(InternalError, "invalid Duck IVF signature");

	m_numFrames = m_hdr.framesInFile;
}

AV1AnnexBDemuxer::AV1AnnexBDemuxer(Params&& params)
	: Demuxer(std::move(params))
{
}

std::vector<uint8_t> AV1AnnexBDemuxer::nextPacket()
{
	std::vector<uint8_t> packet;
	auto& reader = m_params.data;

	DE_ASSERT(!reader->isError());
	if (reader->isEof())
		return packet;

	if (m_remainingBytesInTemporalUnit == 0)
	{
	    uint32_t tuUlebSize = 0;
	    uint32_t tuSize     = getUleb128(&tuUlebSize);
	    m_remainingBytesInTemporalUnit = tuSize;
	}

	uint32_t frameUlebSize = 0;
	uint32_t frameSize     = getUleb128(&frameUlebSize);

	packet.resize(frameSize);
	reader->read(packet);

	DE_ASSERT((frameSize + frameUlebSize) <= m_remainingBytesInTemporalUnit);
	m_remainingBytesInTemporalUnit -= (frameSize + frameUlebSize);

	m_frameNumber++;

	return packet;
}

uint32_t AV1AnnexBDemuxer::getUleb128(uint32_t *numBytes)
{
	auto&	 in		= m_params.data;
	uint64_t val		= 0;
	uint32_t i			= 0;
	uint32_t more		= 0;
	uint32_t bytesRead	= 0;

	do {
		const int v = in->readByteChecked("error reading uleb128 value");
		more = v & 0x80;
		val |= ((uint64_t)(v & 0x7F)) << i;
		bytesRead += 1;
		i += 7;
	} while (more && i < 56);

	if (val > std::numeric_limits<uint32_t>::max() || more)
		return 0;

	if (numBytes)
		*numBytes = bytesRead;

	return (uint32_t)val;
}

} // namespace video
} // namespace vkt
