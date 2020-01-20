#ifndef _VKTIMAGEATOMICSPIRVSHADERS_HPP
#define _VKTIMAGEATOMICSPIRVSHADERS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Helper SPIR-V shaders for some image atomic operations.
 *//*--------------------------------------------------------------------*/
#include "vktImageTestsUtil.hpp"

#include <string>

namespace vkt
{
namespace image
{

// Test case variant, used when deciding which SPIR-V shader to get.
struct CaseVariant
{
	enum CheckType
	{
		CHECK_TYPE_INTERMEDIATE_RESULTS = 0,
		CHECK_TYPE_END_RESULTS,
	};

	ImageType			imageType;
	tcu::TextureFormat	textureFormat;
	CheckType			checkType;

	// Allows this struct to be used as key in maps.
	bool operator< (const CaseVariant& other) const;

	// Constructor.
	CaseVariant (ImageType imgtype, tcu::TextureFormat::ChannelOrder order, tcu::TextureFormat::ChannelType chtype, CheckType cktype);
};

// Gets the shader template for the appropriate case variant.
std::string getSpirvAtomicOpShader (const CaseVariant& caseVariant);

} // namespace image
} // namespace vkt

#endif // _VKTIMAGEATOMICSPIRVSHADERS_HPP
