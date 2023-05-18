#ifndef _EXTNVIDIAVIDEOPARSERIF_HPP
#define _EXTNVIDIAVIDEOPARSERIF_HPP

/*
 * Copyright 2021 NVIDIA Corporation.
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 /*
 * Copyright 2020 NVIDIA Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vkDefs.hpp"

#include <atomic>
#include <stdint.h>
#include <string.h>

namespace vkt
{
namespace video
{
#define DEBUGLOG(X)

using namespace vk;
using namespace std;

class NvidiaParserVideoRefCountBase
{
public:
	//! Increment the reference count by 1.
	virtual int32_t AddRef() = 0;

	//! Decrement the reference count by 1. When the reference count
	//! goes to 0 the object is automatically destroyed.
	virtual int32_t Release() = 0;

protected:
	virtual ~NvidiaParserVideoRefCountBase() { }
};

template<class NvidiaBaseObjType>
class NvidiaSharedBaseObj
{
public:
	NvidiaSharedBaseObj<NvidiaBaseObjType>& Reset (NvidiaBaseObjType* const newObjectPtr)
	{
		if (newObjectPtr != m_sharedObject)
		{
			int refCount = -1;

			if (m_sharedObject != nullptr)
			{
				refCount = m_sharedObject->Release();

				if (refCount < 0)
				{
					DE_ASSERT(0 && "RefCount less than zero");
				}
			}

			m_sharedObject = newObjectPtr;

			if (newObjectPtr != nullptr)
			{
				refCount = newObjectPtr->AddRef();

				if (!(refCount > 0))
				{
					DE_ASSERT(0 && "RefCount is smaller not greater than zero");
				}
			}
		}
		return *this;
	}

	// Constructors increment the refcount of the provided object if non-nullptr
	explicit NvidiaSharedBaseObj (NvidiaBaseObjType* const newObjectPtr = nullptr)
		: m_sharedObject(nullptr)
	{
		Reset(newObjectPtr);
	}

	NvidiaSharedBaseObj (const NvidiaSharedBaseObj<NvidiaBaseObjType>& newObject)
		: m_sharedObject(nullptr)
	{
		Reset(newObject.Get());
	}

	~NvidiaSharedBaseObj ()
	{
		Reset(nullptr);
	}

	// Assignment from another smart pointer maps to raw pointer assignment
	NvidiaSharedBaseObj<NvidiaBaseObjType>& operator= (const NvidiaSharedBaseObj<NvidiaBaseObjType>& sharedObject)
	{
		return Reset(sharedObject.Get());
	}

	NvidiaSharedBaseObj<NvidiaBaseObjType>& operator= (NvidiaBaseObjType* const newObjectPtr)
	{
		return Reset(newObjectPtr);
	}

	template <class VkBaseObjType2>
	const NvidiaSharedBaseObj<NvidiaBaseObjType>& operator= (const NvidiaSharedBaseObj<VkBaseObjType2>& otherSharedObject)
	{
		return Reset(otherSharedObject.Get());
	}

	// Comparison operators can be used with any compatible types
	inline bool operator== (const NvidiaSharedBaseObj<NvidiaBaseObjType>& otherObject)
	{
		return (this->Get() == otherObject.Get());
	}

	inline bool operator!= (const NvidiaSharedBaseObj<NvidiaBaseObjType>& otherObject)
	{
		return !(*this == otherObject);
	}

	bool operator! () const
	{
		return m_sharedObject == nullptr;
	}

	// Exchange
	void Swap (NvidiaSharedBaseObj<NvidiaBaseObjType>& sharedObject)
	{
		NvidiaSharedBaseObj<NvidiaBaseObjType> tmp(m_sharedObject);

		m_sharedObject = sharedObject.m_sharedObject;

		sharedObject.m_sharedObject = tmp;
	}

	// Non ref-counted access to the underlying object
	NvidiaBaseObjType* Get (void) const
	{
		return m_sharedObject;
	}

	// Cast to a raw object pointer
	operator NvidiaBaseObjType* () const
	{
		return m_sharedObject;
	}

	NvidiaBaseObjType* operator-> () const
	{
		return m_sharedObject;
	}

	NvidiaBaseObjType& operator* () const
	{
		return *m_sharedObject;
	}

private:
	NvidiaBaseObjType* m_sharedObject;
};

class INvidiaVulkanPicture
{
public:
	virtual void	AddRef() = 0;
	virtual void	Release() = 0;

	int32_t			decodeWidth;
	int32_t			decodeHeight;
	int32_t			decodeSuperResWidth;
	int32_t			reserved[16 - 3];

protected:
	virtual			~INvidiaVulkanPicture() { }
};

class NvidiaVulkanPictureBase : public INvidiaVulkanPicture
{
private:
	std::atomic<int32_t> m_refCount;

public:
	int32_t		m_picIdx;
	int32_t		m_displayOrder;
	int32_t		m_decodeOrder;
	uint64_t	m_timestamp;
	uint64_t	m_presentTime;

public:
	virtual void AddRef ()
	{
		DE_ASSERT(m_refCount >= 0);

		++m_refCount;
	}

	virtual void Release ()
	{
		DE_ASSERT(m_refCount > 0);

		int32_t ref = --m_refCount;

		if (ref == 0)
		{
			Reset();
		}
	}

public:
	NvidiaVulkanPictureBase ()
		: m_refCount		(0)
		, m_picIdx			(-1)
		, m_displayOrder	(-1)
		, m_decodeOrder		(-1)
		, m_timestamp		(0)
		, m_presentTime		(0)
	{
	}

	bool IsAvailable () const
	{
		DE_ASSERT(m_refCount >= 0);

		return (m_refCount == 0);
	}

	int32_t Reset ()
	{
		int32_t ref = m_refCount;

		m_picIdx		= -1;
		m_displayOrder	= -1;
		m_decodeOrder	= -1;
		m_timestamp		= 0;
		m_presentTime	= 0;
		m_refCount		= 0;

		return ref;
	}

	virtual ~NvidiaVulkanPictureBase ()
	{
		Reset();
	}
};

#define NV_VULKAN_VIDEO_PARSER_API_VERSION_0_9_7 VK_MAKE_VIDEO_STD_VERSION(0, 9, 7)

#define NV_VULKAN_VIDEO_PARSER_API_VERSION   NV_VULKAN_VIDEO_PARSER_API_VERSION_0_9_7

typedef uint32_t FrameRate; // Packed 18-bit numerator & 14-bit denominator

// Definitions for video_format
enum
{
	VideoFormatComponent = 0,
	VideoFormatPAL,
	VideoFormatNTSC,
	VideoFormatSECAM,
	VideoFormatMAC,
	VideoFormatUnspecified,
	VideoFormatReserved6,
	VideoFormatReserved7
};

// Definitions for color_primaries
enum
{
	ColorPrimariesForbidden		= 0,
	ColorPrimariesBT709			= 1,
	ColorPrimariesUnspecified	= 2,
	ColorPrimariesReserved		= 3,
	ColorPrimariesBT470M		= 4,
	ColorPrimariesBT470BG		= 5,
	ColorPrimariesSMPTE170M		= 6, // Also, ITU-R BT.601
	ColorPrimariesSMPTE240M		= 7,
	ColorPrimariesGenericFilm	= 8,
	ColorPrimariesBT2020		= 9,
	// below are defined in AOM standard
	ColorPrimariesXYZ			= 10, // SMPTE 428 (CIE 1921 XYZ)
	ColorPrimariesSMPTE431		= 11, // SMPTE RP 431-2
	ColorPrimariesSMPTE432		= 12, // SMPTE EG 432-1
	ColorPrimariesRESERVED13	= 13, // For future use (values 13 - 21)
	ColorPrimariesEBU3213		= 22, // EBU Tech. 3213-E
	ColorPrimariesRESERVED23	= 23  // For future use (values 23 - 255)
};

// Definitions for transfer_characteristics
enum
{
	TransferCharacteristicsForbidden	= 0,
	TransferCharacteristicsBT709		= 1,
	TransferCharacteristicsUnspecified	= 2,
	TransferCharacteristicsReserved		= 3,
	TransferCharacteristicsBT470M		= 4,
	TransferCharacteristicsBT470BG		= 5,
	TransferCharacteristicsSMPTE170M	= 6,
	TransferCharacteristicsSMPTE240M	= 7,
	TransferCharacteristicsLinear		= 8,
	TransferCharacteristicsLog100		= 9,
	TransferCharacteristicsLog316		= 10,
	TransferCharacteristicsIEC61966_2_4	= 11,
	TransferCharacteristicsBT1361		= 12,
	TransferCharacteristicsIEC61966_2_1	= 13,
	TransferCharacteristicsBT2020		= 14,
	TransferCharacteristicsBT2020_2		= 15,
	TransferCharacteristicsST2084		= 16,
	TransferCharacteristicsST428_1		= 17,
	// below are defined in AOM standard
	TransferCharacteristicsHLG			= 18, // BT.2100 HLG, ARIB STD-B67
	TransferCharacteristicsRESERVED19	= 19  // For future use (values 19-255)
};

// Definitions for matrix_coefficients
enum
{
	MatrixCoefficientsForbidden			= 0,
	MatrixCoefficientsBT709				= 1,
	MatrixCoefficientsUnspecified		= 2,
	MatrixCoefficientsReserved			= 3,
	MatrixCoefficientsFCC				= 4,
	MatrixCoefficientsBT470BG			= 5,
	MatrixCoefficientsSMPTE170M			= 6,
	MatrixCoefficientsSMPTE240M			= 7,
	MatrixCoefficientsYCgCo				= 8,
	MatrixCoefficientsBT2020_NCL		= 9,  // Non-constant luminance
	MatrixCoefficientsBT2020_CL			= 10, // Constant luminance
	// below are defined in AOM standard
	MatrixCoefficientsSMPTE2085			= 11, // SMPTE ST 2085 YDzDx
	MatrixCoefficientsCHROMAT_NCL		= 12, // Chromaticity-derived non-constant luminance
	MatrixCoefficientsCHROMAT_CL		= 13, // Chromaticity-derived constant luminance
	MatrixCoefficientsICTCP				= 14, // BT.2100 ICtCp
	MatrixCoefficientsRESERVED15		= 15
};

// Maximum raw sequence header length (all codecs) i.e. 1024 bytes
#define VK_MAX_SEQ_HDR_LEN (1024)

typedef struct NvidiaVulkanParserH264DpbEntry
{
	INvidiaVulkanPicture*	pNvidiaVulkanPicture;	// ptr to reference frame
	int32_t					FrameIdx;				// frame_num(short-term) or LongTermFrameIdx(long-term)
	int32_t					is_long_term;			// 0=short term reference, 1=long term reference
	int32_t					not_existing;			// non-existing reference frame (corresponding PicIdx should be set to -1)
	int32_t					used_for_reference;		// 0=unused, 1=top_field, 2=bottom_field, 3=both_fields
	int32_t					FieldOrderCnt[2];		// field order count of top and bottom fields
} NvidiaVulkanParserH264DpbEntry;

typedef struct NvidiaVulkanParserH264PictureData
{
	// SPS
	const struct vk::StdVideoH264SequenceParameterSet*	pStdSps;
	NvidiaParserVideoRefCountBase*						pSpsClientObject;

	// PPS
	const struct vk::StdVideoH264PictureParameterSet*	pStdPps;
	NvidiaParserVideoRefCountBase*						pPpsClientObject;

	uint8_t												pic_parameter_set_id;					// PPS ID
	uint8_t												seq_parameter_set_id;					// SPS ID
	uint8_t												vps_video_parameter_set_id;				// VPS ID
	int32_t												num_ref_idx_l0_active_minus1;
	int32_t												num_ref_idx_l1_active_minus1;
	int32_t												weighted_pred_flag;
	int32_t												weighted_bipred_idc;
	int32_t												pic_init_qp_minus26;
	int32_t												redundant_pic_cnt_present_flag;
	uint8_t												deblocking_filter_control_present_flag;
	uint8_t												transform_8x8_mode_flag;
	uint8_t												MbaffFrameFlag;
	uint8_t												constrained_intra_pred_flag;
	uint8_t												entropy_coding_mode_flag;
	uint8_t												pic_order_present_flag;
	int8_t												chroma_qp_index_offset;
	int8_t												second_chroma_qp_index_offset;
	int32_t												frame_num;
	int32_t												CurrFieldOrderCnt[2];
	uint8_t												fmo_aso_enable;
	uint8_t												num_slice_groups_minus1;
	uint8_t												slice_group_map_type;
	int8_t												pic_init_qs_minus26;
	uint32_t											slice_group_change_rate_minus1;
	const uint8_t*										pMb2SliceGroupMap;
	// DPB
	NvidiaVulkanParserH264DpbEntry						dpb[16 + 1]; // List of reference frames within the DPB

	// Quantization Matrices (raster-order)
	union
	{
		// MVC extension
		struct
		{
			int32_t		num_views_minus1;
			int32_t		view_id;
			uint8_t		inter_view_flag;
			uint8_t		num_inter_view_refs_l0;
			uint8_t		num_inter_view_refs_l1;
			uint8_t		MVCReserved8Bits;
			int32_t		InterViewRefsL0[16];
			int32_t		InterViewRefsL1[16];
		} mvcext;
		// SVC extension
		struct
		{
			uint8_t		profile_idc;
			uint8_t		level_idc;
			uint8_t		DQId;
			uint8_t		DQIdMax;
			uint8_t		disable_inter_layer_deblocking_filter_idc;
			uint8_t		ref_layer_chroma_phase_y_plus1;
			int8_t		inter_layer_slice_alpha_c0_offset_div2;
			int8_t		inter_layer_slice_beta_offset_div2;
			uint16_t	DPBEntryValidFlag;

			union
			{
				struct
				{
					uint8_t		inter_layer_deblocking_filter_control_present_flag : 1;
					uint8_t		extended_spatial_scalability_idc : 2;
					uint8_t		adaptive_tcoeff_level_prediction_flag : 1;
					uint8_t		slice_header_restriction_flag : 1;
					uint8_t		chroma_phase_x_plus1_flag : 1;
					uint8_t		chroma_phase_y_plus1 : 2;
					uint8_t		tcoeff_level_prediction_flag : 1;
					uint8_t		constrained_intra_resampling_flag : 1;
					uint8_t		ref_layer_chroma_phase_x_plus1_flag : 1;
					uint8_t		store_ref_base_pic_flag : 1;
					uint8_t		Reserved : 4;
				} f;
				uint8_t		ucBitFields[2];
			};

			union {
				int16_t		seq_scaled_ref_layer_left_offset;
				int16_t		scaled_ref_layer_left_offset;
			};
			union {
				int16_t		seq_scaled_ref_layer_top_offset;
				int16_t		scaled_ref_layer_top_offset;
			};
			union {
				int16_t		seq_scaled_ref_layer_right_offset;
				int16_t		scaled_ref_layer_right_offset;
			};
			union {
				int16_t		seq_scaled_ref_layer_bottom_offset;
				int16_t		scaled_ref_layer_bottom_offset;
			};
		} svcext;
	};
} NvidiaVulkanParserH264PictureData;

typedef struct NvidiaVulkanParserH265PictureData
{
	// VPS
	const StdVideoH265VideoParameterSet*				pStdVps;
	NvidiaParserVideoRefCountBase*						pVpsClientObject;

	// SPS
	const struct vk::StdVideoH265SequenceParameterSet*	pStdSps;
	NvidiaParserVideoRefCountBase*						pSpsClientObject;

	// PPS
	const struct vk::StdVideoH265PictureParameterSet*	pStdPps;
	NvidiaParserVideoRefCountBase*						pPpsClientObject;

	uint8_t												pic_parameter_set_id;				// PPS ID
	uint8_t												seq_parameter_set_id;				// SPS ID
	uint8_t												vps_video_parameter_set_id;			// VPS ID

	uint8_t												IrapPicFlag;
	uint8_t												IdrPicFlag;

	// RefPicSets
	int32_t												NumBitsForShortTermRPSInSlice;
	int32_t												NumDeltaPocsOfRefRpsIdx;
	int32_t												NumPocTotalCurr;
	int32_t												NumPocStCurrBefore;
	int32_t												NumPocStCurrAfter;
	int32_t												NumPocLtCurr;
	int32_t												CurrPicOrderCntVal;
	INvidiaVulkanPicture*								RefPics[16];
	int32_t												PicOrderCntVal[16];
	uint8_t												IsLongTerm[16]; // 1=long-term reference
	int8_t												RefPicSetStCurrBefore[8];
	int8_t												RefPicSetStCurrAfter[8];
	int8_t												RefPicSetLtCurr[8];

	// various profile related
	// 0 = invalid, 1 = Main, 2 = Main10, 3 = still picture, 4 = Main 12, 5 = MV-HEVC Main8
	uint8_t												ProfileLevel;
	uint8_t												ColorPrimaries; // ColorPrimariesBTXXXX enum
	uint8_t												bit_depth_luma_minus8;
	uint8_t												bit_depth_chroma_minus8;

	// MV-HEVC related fields
	uint8_t												mv_hevc_enable;
	uint8_t												nuh_layer_id;
	uint8_t												default_ref_layers_active_flag;
	uint8_t												NumDirectRefLayers;
	uint8_t												max_one_active_ref_layer_flag;
	uint8_t												poc_lsb_not_present_flag;
	uint8_t												pad0[2];

	int32_t												NumActiveRefLayerPics0;
	int32_t												NumActiveRefLayerPics1;
	int8_t												RefPicSetInterLayer0[8];
	int8_t												RefPicSetInterLayer1[8];
} NvidiaVulkanParserH265PictureData;

typedef struct NvidiaVulkanParserPictureData
{
	int32_t					PicWidthInMbs;			// Coded Frame Size
	int32_t					FrameHeightInMbs;		// Coded Frame Height
	INvidiaVulkanPicture*	pCurrPic;				// Current picture (output)
	int32_t					field_pic_flag;			// 0=frame picture, 1=field picture
	int32_t					bottom_field_flag;		// 0=top field, 1=bottom field (ignored if field_pic_flag=0)
	int32_t					second_field;			// Second field of a complementary field pair
	int32_t					progressive_frame;		// Frame is progressive
	int32_t					top_field_first;		// Frame pictures only
	int32_t					repeat_first_field;		// For 3:2 pulldown (number of additional fields, 2=frame doubling, 4=frame tripling)
	int32_t					ref_pic_flag;			// Frame is a reference frame
	int32_t					intra_pic_flag;			// Frame is entirely intra coded (no temporal dependencies)
	int32_t					chroma_format;			// Chroma Format (should match sequence info)
	int32_t					picture_order_count;	// picture order count (if known)
	uint8_t*				pbSideData;				// Encryption Info
	uint32_t				nSideDataLen;			// Encryption Info length

	// Bitstream data
	uint32_t				nBitstreamDataLen;		// Number of bytes in bitstream data buffer
	uint8_t*				pBitstreamData;			// Ptr to bitstream data for this picture (slice-layer)
	uint32_t				nNumSlices;				// Number of slices(tiles in case of AV1) in this picture
	const uint32_t*			pSliceDataOffsets;		// nNumSlices entries, contains offset of each slice within the bitstream data buffer

	// Codec-specific data
	union
	{
		NvidiaVulkanParserH264PictureData	h264;
		NvidiaVulkanParserH265PictureData	h265;
	} CodecSpecific;
} NvidiaVulkanParserPictureData;

// Packet input for parsing
typedef struct NvidiaVulkanParserBitstreamPacket
{
	const uint8_t*	pByteStream;		// Ptr to byte stream data
	int32_t			nDataLength;		// Data length for this packet
	int32_t			bEOS;				// true if this is an End-Of-Stream packet (flush everything)
	int32_t			bPTSValid;			// true if llPTS is valid (also used to detect frame boundaries for VC1 SP/MP)
	int32_t			bDiscontinuity;		// true if DecMFT is signalling a discontinuity
	int32_t			bPartialParsing;	// 0: parse entire packet, 1: parse until next decode/display event
	int64_t			llPTS;				// Presentation Time Stamp for this packet (clock rate specified at initialization)
	bool			bDisablePP;			// optional flag for VC1
	bool			bEOP;				// true if the packet in pByteStream is exactly one frame
	uint8_t*		pbSideData;			// Auxiliary encryption information
	int32_t			nSideDataLength;	// Auxiliary encrypton information length
} NvidiaVulkanParserBitstreamPacket;

// Sequence information
typedef struct NvidiaVulkanParserSequenceInfo
{
	vk::VkVideoCodecOperationFlagBitsKHR	eCodec;										// Compression Standard
	bool									isSVC;										// h.264 SVC
	FrameRate								frameRate;									// Frame Rate stored in the bitstream
	int32_t									bProgSeq;									// Progressive Sequence
	int32_t									nDisplayWidth;								// Displayed Horizontal Size
	int32_t									nDisplayHeight;								// Displayed Vertical Size
	int32_t									nCodedWidth;								// Coded Picture Width
	int32_t									nCodedHeight;								// Coded Picture Height
	int32_t									nMaxWidth;									// Max width within sequence
	int32_t									nMaxHeight;									// Max height within sequence
	uint8_t									nChromaFormat;								// Chroma Format (0=4:0:0, 1=4:2:0, 2=4:2:2, 3=4:4:4)
	uint8_t									uBitDepthLumaMinus8;						// Luma bit depth (0=8bit)
	uint8_t									uBitDepthChromaMinus8;						// Chroma bit depth (0=8bit)
	uint8_t									uVideoFullRange;							// 0=16-235, 1=0-255
	int32_t									lBitrate;									// Video bitrate (bps)
	int32_t									lDARWidth;									// Display Aspect Ratio = lDARWidth : lDARHeight
	int32_t									lDARHeight;									// Display Aspect Ratio = lDARWidth : lDARHeight
	int32_t									lVideoFormat;								// Video Format (VideoFormatXXX)
	int32_t									lColorPrimaries;							// Colour Primaries (ColorPrimariesXXX)
	int32_t									lTransferCharacteristics;					// Transfer Characteristics
	int32_t									lMatrixCoefficients;						// Matrix Coefficients
	int32_t									cbSequenceHeader;							// Number of bytes in SequenceHeaderData
	int32_t									nMinNumDpbSlots;							// Minimum number of DPB slots for correct decoding
	int32_t									nMinNumDecodeSurfaces;						// Minimum number of decode surfaces for correct decoding
	uint8_t									SequenceHeaderData[VK_MAX_SEQ_HDR_LEN];		// Raw sequence header data (codec-specific)
	uint8_t*								pbSideData;									// Auxiliary encryption information
	uint32_t								cbSideData;									// Auxiliary encryption information length
	uint32_t								codecProfile;								// Codec Profile IDC
} NvidiaVulkanParserSequenceInfo;

enum {
	VK_PARSER_CAPS_MVC = 0x01,
	VK_PARSER_CAPS_SVC = 0x02,
};

enum NvidiaParserPictureParametersUpdateType
{
	VK_PICTURE_PARAMETERS_UPDATE_H264_SPS = 0,
	VK_PICTURE_PARAMETERS_UPDATE_H264_PPS,
	VK_PICTURE_PARAMETERS_UPDATE_H265_VPS,
	VK_PICTURE_PARAMETERS_UPDATE_H265_SPS,
	VK_PICTURE_PARAMETERS_UPDATE_H265_PPS,
};

typedef struct NvidiaVulkanPictureParameters
{
	NvidiaParserPictureParametersUpdateType					updateType;
	union
	{
		const struct vk::StdVideoH264SequenceParameterSet*	pH264Sps;
		const struct vk::StdVideoH264PictureParameterSet*	pH264Pps;
		const struct vk::StdVideoH265VideoParameterSet*		pH265Vps;
		const struct vk::StdVideoH265SequenceParameterSet*	pH265Sps;
		const struct vk::StdVideoH265PictureParameterSet*	pH265Pps;
	};
	uint32_t												updateSequenceCount;
} NvidiaVulkanPictureParameters;

// Interface to allow decoder to communicate with the client
class NvidiaVulkanParserVideoDecodeClient
{
public:
	// callback
	virtual int32_t		BeginSequence							(const NvidiaVulkanParserSequenceInfo*					pnvsi)							= 0;	// Returns max number of reference frames (always at least 2 for MPEG-2)
	virtual bool		AllocPictureBuffer						(INvidiaVulkanPicture**									ppNvidiaVulkanPicture)			= 0;	// Returns a new INvidiaVulkanPicture interface
	virtual bool		DecodePicture							(NvidiaVulkanParserPictureData*							pNvidiaVulkanParserPictureData)	= 0;	// Called when a picture is ready to be decoded
	virtual bool		UpdatePictureParameters					(NvidiaVulkanPictureParameters*							pNvidiaVulkanPictureParameters,
																 NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>&	pictureParametersObject,
																 uint64_t												updateSequenceCount)			= 0;	// Called when a picture is ready to be decoded
	virtual bool		DisplayPicture							(INvidiaVulkanPicture*									pNvidiaVulkanPicture,
																 int64_t												llPTS)							= 0;
	virtual void		UnhandledNALU							(const uint8_t* pbData,
																 int32_t												cbData)							= 0;	// Called for custom NAL parsing (not required)
	virtual uint32_t	GetDecodeCaps							()																		{ return 0; }			// NVD_CAPS_XXX
	virtual int32_t		GetOperatingPoint						(void*													/* pOPInfo */)	{ return 0; }			// called from sequence header of av1 scalable video streams

protected:
	virtual				~NvidiaVulkanParserVideoDecodeClient	() {}
};

// Initialization parameters for decoder class
typedef struct NvidiaVulkanParserInitDecodeParameters
{
	uint32_t								interfaceVersion;
	NvidiaVulkanParserVideoDecodeClient*	pClient;						// should always be present if using parsing functionality
	uint64_t								lReferenceClockRate;			// ticks per second of PTS clock (0=default=10000000=10Mhz)
	int32_t									lErrorThreshold;				// threshold for deciding to bypass of picture (0=do not decode, 100=always decode)
	NvidiaVulkanParserSequenceInfo*			pExternalSeqInfo;				// optional external sequence header data from system layer
	bool									bOutOfBandPictureParameters;	// If set, Picture Parameters are going to be provided via UpdatePictureParameters callback
} NvidiaVulkanParserInitDecodeParameters;

// High-level interface to video decoder (Note that parsing and decoding functionality are decoupled from each other)
// Some unused parameters has been replaced with void*
class NvidiaVulkanVideoDecodeParser : public virtual NvidiaParserVideoRefCountBase
{
public:
	virtual VkResult		Initialize				(NvidiaVulkanParserInitDecodeParameters*	pVulkanParserInitDecodeParameters)	= 0;
	virtual bool			Deinitialize			()																				= 0;
	virtual bool			DecodePicture			(NvidiaVulkanParserPictureData*				pVulkanParserPictureData)			= 0;
	virtual bool			ParseByteStream			(const NvidiaVulkanParserBitstreamPacket*	pVulkanParserBitstreamPacket,
													 int32_t*									pParsedBytes = NULL)				= 0;
	virtual bool			DecodeSliceInfo			(void*										pVulkanParserSliceInfo,
													 const void*								pVulkanParserPictureData,
													 int32_t									iSlice)								= 0;
	virtual bool			GetDisplayMasteringInfo	(void*										pVulkanParserDisplayMasteringInfo)	= 0;
};

struct SpsVideoH264PictureParametersSet
{
	vk::StdVideoH264SequenceParameterSet    stdSps;
	vk::StdVideoH264SequenceParameterSetVui stdVui;
	vk::StdVideoH264ScalingLists            spsStdScalingLists;
};

struct PpsVideoH264PictureParametersSet
{
	vk::StdVideoH264PictureParameterSet     stdPps;
	vk::StdVideoH264ScalingLists            ppsStdScalingLists;
};

struct VpsVideoH265PictureParametersSet
{
	vk::StdVideoH265VideoParameterSet	    stdVps;
};

struct SpsVideoH265PictureParametersSet
{
	vk::StdVideoH265SequenceParameterSet    stdSps;
	vk::StdVideoH265SequenceParameterSetVui stdVui;
	vk::StdVideoH265ScalingLists            spsStdScalingLists;
};

struct PpsVideoH265PictureParametersSet
{
	vk::StdVideoH265PictureParameterSet     stdPps;
	vk::StdVideoH265ScalingLists            ppsStdScalingLists;
};

class StdVideoPictureParametersSet : public NvidiaParserVideoRefCountBase
{
public:

	void Update(NvidiaVulkanPictureParameters* pPictureParameters, uint32_t updateSequenceCount)
	{
		switch (pPictureParameters->updateType)
		{
			case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
			case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
			{

				if (pPictureParameters->updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_SPS)
				{
					m_data.h264Sps.stdSps = *pPictureParameters->pH264Sps;
					if (pPictureParameters->pH264Sps->pScalingLists)
					{
						m_data.h264Sps.spsStdScalingLists = *pPictureParameters->pH264Sps->pScalingLists;
						m_data.h264Sps.stdSps.pScalingLists = &m_data.h264Sps.spsStdScalingLists;
					}

					if (pPictureParameters->pH264Sps->pSequenceParameterSetVui)
					{
						m_data.h264Sps.stdVui = *pPictureParameters->pH264Sps->pSequenceParameterSetVui;
						m_data.h264Sps.stdSps.pSequenceParameterSetVui = &m_data.h264Sps.stdVui;
					}
				}
				else if (pPictureParameters->updateType == VK_PICTURE_PARAMETERS_UPDATE_H264_PPS)
				{
					m_data.h264Pps.stdPps = *pPictureParameters->pH264Pps;

					if (pPictureParameters->pH264Pps->pScalingLists)
					{
						m_data.h264Pps.ppsStdScalingLists = *pPictureParameters->pH264Pps->pScalingLists;
						m_data.h264Pps.stdPps.pScalingLists = &m_data.h264Pps.ppsStdScalingLists;
					}
				}

				break;
			}
			case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
			case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
			case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
			{
				if (pPictureParameters->updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_SPS)
				{
					m_data.h265Sps.stdSps = *pPictureParameters->pH265Sps;
					if (pPictureParameters->pH265Sps->pScalingLists)
					{
						m_data.h265Sps.spsStdScalingLists = *pPictureParameters->pH265Sps->pScalingLists;
						m_data.h265Sps.stdSps.pScalingLists = &m_data.h265Sps.spsStdScalingLists;
					}
					if (pPictureParameters->pH265Sps->pSequenceParameterSetVui)
					{
						m_data.h265Sps.stdVui = *pPictureParameters->pH265Sps->pSequenceParameterSetVui;
						m_data.h265Sps.stdSps.pSequenceParameterSetVui = &m_data.h265Sps.stdVui;
					}

				}
				else if (pPictureParameters->updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_PPS)
				{
					m_data.h265Pps.stdPps = *pPictureParameters->pH265Pps;

					if (pPictureParameters->pH265Pps->pScalingLists)
					{
						m_data.h265Pps.ppsStdScalingLists = *pPictureParameters->pH265Pps->pScalingLists;
						m_data.h265Pps.stdPps.pScalingLists = &m_data.h265Pps.ppsStdScalingLists;
					}
				}
				else if (pPictureParameters->updateType == VK_PICTURE_PARAMETERS_UPDATE_H265_VPS)
				{
					m_data.h265Vps.stdVps = *pPictureParameters->pH265Vps;
				}

				break;
			}
			default:
				DE_ASSERT(0 && "Invalid Parser format");
		}

		m_updateSequenceCount = updateSequenceCount;
	}

	int32_t GetVpsId(bool& isVps) const
	{
		switch (m_updateType)
		{
			case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
				return m_data.h264Sps.stdSps.seq_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
				return m_data.h264Pps.stdPps.seq_parameter_set_id;

			case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
				isVps = true;
				return m_data.h265Vps.stdVps.vps_video_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
				return m_data.h265Sps.stdSps.sps_seq_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
				return m_data.h265Pps.stdPps.pps_seq_parameter_set_id;

			default:
				DE_ASSERT(0 && "Invalid STD type");
		}

		return -1;
	}

	int32_t GetSpsId(bool& isSps) const
	{
		isSps = false;

		switch (m_updateType)
		{
			case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
				isSps = true;
				return m_data.h264Sps.stdSps.seq_parameter_set_id;

			case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
				return m_data.h264Pps.stdPps.seq_parameter_set_id;

			case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
				return m_data.h265Vps.stdVps.vps_video_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
				isSps = true;
				return m_data.h265Sps.stdSps.sps_seq_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
				return m_data.h265Pps.stdPps.pps_seq_parameter_set_id;

			default:
				DE_ASSERT(0 && "Invalid STD type");
		}

		return -1;
	}

	int32_t GetPpsId(bool& isPps) const
	{
		isPps = false;
		switch (m_updateType)
		{
			case VK_PICTURE_PARAMETERS_UPDATE_H264_SPS:
				break;
			case VK_PICTURE_PARAMETERS_UPDATE_H264_PPS:
				isPps = true;
				return m_data.h264Pps.stdPps.pic_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_SPS:
				break;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_PPS:
				isPps = true;
				return m_data.h265Pps.stdPps.pps_pic_parameter_set_id;
			case VK_PICTURE_PARAMETERS_UPDATE_H265_VPS:
				break;
			default:
				DE_ASSERT(0 && "Invalid STD type");
		}
		return -1;
	}

	static StdVideoPictureParametersSet* Create(NvidiaVulkanPictureParameters* pPictureParameters, uint64_t updateSequenceCount)
	{
		StdVideoPictureParametersSet* pNewSet = new StdVideoPictureParametersSet(pPictureParameters->updateType);

		pNewSet->Update(pPictureParameters, (uint32_t)updateSequenceCount);

		return pNewSet;
	}

	static StdVideoPictureParametersSet* StdVideoPictureParametersSetFromBase(NvidiaParserVideoRefCountBase* pBase)
	{
		if (!pBase)
			return DE_NULL;

		StdVideoPictureParametersSet*	result	= dynamic_cast<StdVideoPictureParametersSet*>(pBase);

		if (result)
			return result;

		TCU_THROW(InternalError, "Invalid StdVideoPictureParametersSet from pBase");
	}

	virtual int32_t AddRef()
	{
		return ++m_refCount;
	}

	virtual int32_t Release()
	{
		uint32_t ret = --m_refCount;

		// Destroy the device if refcount reaches zero
		if (ret == 0)
		{
			delete this;
		}

		return ret;
	}

private:
	std::atomic<int32_t>								m_refCount;

public:
	NvidiaParserPictureParametersUpdateType				m_updateType;
	union
	{
		SpsVideoH264PictureParametersSet h264Sps;
		PpsVideoH264PictureParametersSet h264Pps;
		VpsVideoH265PictureParametersSet h265Vps;
		SpsVideoH265PictureParametersSet h265Sps;
		PpsVideoH265PictureParametersSet h265Pps;
	} m_data;

	uint32_t											m_updateSequenceCount;
	NvidiaSharedBaseObj<NvidiaParserVideoRefCountBase>	m_vkObjectOwner; // NvidiaParserVideoPictureParameters
	vk::VkVideoSessionKHR								m_vkVideoDecodeSession;
private:

	StdVideoPictureParametersSet	(NvidiaParserPictureParametersUpdateType updateType)
		: m_refCount				(0)
		, m_updateType				(updateType)
		, m_data					()
		, m_updateSequenceCount		(0)
		, m_vkVideoDecodeSession	(DE_NULL)
	{
	}

	~StdVideoPictureParametersSet()
	{
		m_vkObjectOwner = nullptr;
		m_vkVideoDecodeSession = vk::VkVideoSessionKHR();
	}

};

struct VulkanParserDetectedVideoFormat
{
	vk::VkVideoCodecOperationFlagBitsKHR	codec;								// Compression format
	uint32_t								codecProfile;
	VkVideoComponentBitDepthFlagsKHR		lumaBitDepth;
	VkVideoComponentBitDepthFlagsKHR		chromaBitDepth;
	VkVideoChromaSubsamplingFlagBitsKHR		chromaSubsampling;
	uint32_t								frame_rate_numerator;				// Frame rate denominator (0 = unspecified or variable frame rate)
	uint32_t								frame_rate_denominator;
	uint8_t									sequenceUpdate : 1;					// if true, this is a sequence update and not the first time StartVideoSequence is being called.
	uint8_t									sequenceReconfigireFormat : 1;		// if true, this is a sequence update for the video format.
	uint8_t									sequenceReconfigireCodedExtent : 1;	// if true, this is a sequence update for the video coded extent.
	uint8_t									progressive_sequence : 1;			// false = interlaced, true = progressive
	uint8_t									bit_depth_luma_minus8;				// high bit depth luma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth
	uint8_t									bit_depth_chroma_minus8;			// high bit depth chroma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth
	uint8_t									reserved1;							// Reserved for future use
	uint32_t								coded_width;						// Coded frame width in pixels
	uint32_t								coded_height;						// Coded frame height in pixels

	struct
	{
		int32_t								left;								// left position of display rect
		int32_t								top;								// top position of display rect
		int32_t								right;								// right position of display rect
		int32_t								bottom;								// bottom position of display rect
	} display_area;

	uint32_t								bitrate;							// Video bitrate (bps, 0=unknown)
	int32_t									display_aspect_ratio_x;
	int32_t									display_aspect_ratio_y;
	uint32_t								minNumDecodeSurfaces;				// Minimum number of decode surfaces for correct decoding (NumRefFrames + 2;)
	uint32_t								maxNumDpbSlots;						// Can't be more than 16 + 1

	struct
	{
		uint8_t								video_format : 3;					// 0-Component, 1-PAL, 2-NTSC, 3-SECAM, 4-MAC, 5-Unspecified
		uint8_t								video_full_range_flag : 1;			// indicates the black level and luma and chroma range
		uint8_t								reserved_zero_bits : 4;				// Reserved bits
		uint8_t								color_primaries;					// Chromaticity coordinates of source primaries
		uint8_t								transfer_characteristics;			// Opto-electronic transfer characteristic of the source picture
		uint8_t								matrix_coefficients;				// used in deriving luma and chroma signals from RGB primaries
	} video_signal_description;													// Video Signal Description. Refer section E.2.1 (VUI parameters semantics) of H264 spec file
	uint32_t								seqhdr_data_length;					// Additional bytes following (NVVIDEOFORMATEX)
};

union VulkanParserFieldFlags
{
	struct
	{
		uint32_t progressiveFrame : 1;	// Frame is progressive
		uint32_t fieldPic : 1;			// 0 = frame picture, 1 = field picture
		uint32_t bottomField : 1;		// 0 = top field, 1 = bottom field (ignored if field_pic_flag=0)
		uint32_t secondField : 1;		// Second field of a complementary field pair
		uint32_t topFieldFirst : 1;		// Frame pictures only
		uint32_t unpairedField : 1;		// Incomplete (half) frame.
		uint32_t syncFirstReady : 1;	// Synchronize the second field to the first one.
		uint32_t syncToFirstField : 1;	// Synchronize the second field to the first one.
		uint32_t repeatFirstField : 3;	// For 3:2 pulldown (number of additional fields, 2 = frame doubling, 4 = frame tripling)
		uint32_t refPic : 1;			// Frame is a reference frame
	};
	uint32_t fieldFlags;
};

struct VulkanParserFrameSyncinfo
{
	uint32_t	unpairedField : 1;		// Generate a semaphore reference, do not return the semaphore.
	uint32_t	syncToFirstField : 1;	// Use the semaphore from the unpared field to wait on.
	void*		pDebugInterface;
};

struct VulkanParserDecodePictureInfo
{
	int32_t						displayWidth;
	int32_t						displayHeight;
	int32_t						pictureIndex;		// Index of the current picture
	VulkanParserFieldFlags		flags;
	VulkanParserFrameSyncinfo	frameSyncinfo;
	uint16_t					videoFrameType;		// VideoFrameType - use Vulkan codec specific type pd->CodecSpecific.h264.slice_type.
	uint16_t					viewId;				// from pictureInfoH264->ext.mvcext.view_id
};

struct PerFrameDecodeParameters
{
	enum
	{
		MAX_DPB_REF_SLOTS = 16 + 1
	};

	int									currPicIdx;							// Output index of the current picture
	StdVideoPictureParametersSet*		pCurrentPictureParameters;
	unsigned int						bitstreamDataLen;					// Number of bytes in bitstream data buffer
	const unsigned char*				pBitstreamData;						// ptr to bitstream data for this picture (slice-layer)
	vk::VkVideoDecodeInfoKHR			decodeFrameInfo;
	vk::VkVideoReferenceSlotInfoKHR		decodeBeginSlots[MAX_DPB_REF_SLOTS];
	int32_t								numGopReferenceSlots;
	int8_t								pGopReferenceImagesIndexes[MAX_DPB_REF_SLOTS];
	vk::VkVideoPictureResourceInfoKHR	pictureResources[MAX_DPB_REF_SLOTS];
};

} // video
} // vkt

#endif // _EXTNVIDIAVIDEOPARSERIF_HPP
