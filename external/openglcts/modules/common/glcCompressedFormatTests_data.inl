/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 Google Inc.
 * Copyright (c) 2021 The Khronos Group Inc.
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
 */ /*!
 * \file glcCompressedFormatTests_data.inl
 * \brief Tests for OpenGL ES 3.1 and 3.2 compressed image formats
 */ /*-------------------------------------------------------------------*/
struct ImageData
{
	GLsizei			width;
	GLsizei			height;
	string			path;
};

const map<GLenum, vector<ImageData>> imageData =
{
	{
		GL_COMPRESSED_R11_EAC,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_240x240.bin"					// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_R_120x120.bin"					// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_240x240_ref.bin"				// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SIGNED_R11_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_signed_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_R_signed_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_signed_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RG11_EAC,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_240x240.bin"					// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RG_120x120.bin"					// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_240x240_ref.bin"				// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SIGNED_RG11_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_signed_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RG_signed_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_signed_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGB8_ETC2,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGB8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGB8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGB8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ETC2,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGB8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGB8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGB8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,						// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA1_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGBA1_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA1_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,					// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA8_ETC2_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGBA8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_4x4,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_4x4_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_4x4_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_4x4_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_5x4,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x4_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x4_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x4_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_5x5,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_5x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_6x5,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_6x6,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_6x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_8x5,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_8x6,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_8x8,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x8_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x8_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_8x8_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_10x5,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_10x6,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_10x8,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x8_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x8_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x8_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_10x10,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x10_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x10_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_10x10_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_12x10,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x10_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x10_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x10_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA_ASTC_12x12,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x12_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x12_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_RGBA_12x12_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_4x4_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_4x4_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_4x4_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x4_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x4_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x4_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_5x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_6x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x8_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x8_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_8x8_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x5_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x5_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x5_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x6_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x6_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x6_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x8_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x8_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x8_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x10_240x240.bin"		// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x10_120x120.bin"		// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_10x10_240x240_ref.bin"	// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x10_240x240.bin"		// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x10_120x120.bin"		// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x10_240x240_ref.bin"	// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12,							// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x12_240x240.bin"		// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x12_120x120.bin"		// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/astc_SRGBA_12x12_240x240_ref.bin"	// string				path
			},
		},
	},
	{
		GL_COMPRESSED_R11_EAC,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_240x240.bin"					// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_R_120x120.bin"					// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_240x240_ref.bin"				// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SIGNED_R11_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_signed_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_R_signed_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_R_signed_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RG11_EAC,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_240x240.bin"					// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RG_120x120.bin"					// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_240x240_ref.bin"				// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SIGNED_RG11_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_signed_240x240.bin"			// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RG_signed_120x120.bin"			// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RG_signed_240x240_ref.bin"		// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGB8_ETC2,											// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGB8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGB8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGB8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ETC2,										// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGB8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGB8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGB8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,						// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA1_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGBA1_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA1_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,					// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA1_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_RGBA8_ETC2_EAC,									// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_RGBA8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_RGBA8_240x240_ref.bin"			// string				path
			},
		},
	},
	{
		GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,								// GLenum				target
		{																// vector<ImageData>	images
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_240x240.bin"				// string				path
			},
			{
				120,														// GLsizei				width
				120,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_120x120.bin"				// string				path
			},
			{
				240,														// GLsizei				width
				240,														// GLsizei				height
				"compressed_texture/etc_sRGBA8_240x240_ref.bin"			// string				path
			},
		},
	},
};
