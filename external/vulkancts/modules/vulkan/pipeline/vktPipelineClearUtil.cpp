/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Utilities for clear values.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineClearUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

tcu::Vec4 defaultClearColorFloat (const tcu::TextureFormat& format)
{
	const tcu::TextureFormatInfo	formatInfo	= tcu::getTextureFormatInfo(format);
	return (defaultClearColorUnorm() - formatInfo.lookupBias) / formatInfo.lookupScale;
}

tcu::IVec4 defaultClearColorInt (const tcu::TextureFormat& format)
{
	const tcu::TextureFormatInfo	formatInfo	= tcu::getTextureFormatInfo(format);
	const tcu::Vec4					color		= (defaultClearColorUnorm() - formatInfo.lookupBias) / formatInfo.lookupScale;

	const tcu::IVec4				result		((deInt32)deFloatRound(color.x()), (deInt32)deFloatRound(color.y()),
												 (deInt32)deFloatRound(color.z()), (deInt32)deFloatRound(color.w()));

	return result;
}

tcu::UVec4 defaultClearColorUint (const tcu::TextureFormat& format)
{
	const tcu::TextureFormatInfo	formatInfo	= tcu::getTextureFormatInfo(format);
	const tcu::Vec4					color		= (defaultClearColorUnorm() - formatInfo.lookupBias) / formatInfo.lookupScale;

	const	tcu::UVec4				result		((deUint32)deFloatRound(color.x()), (deUint32)deFloatRound(color.y()),
												 (deUint32)deFloatRound(color.z()), (deUint32)deFloatRound(color.w()));

	return result;
}

tcu::Vec4 defaultClearColorUnorm (void)
{
	return tcu::Vec4(0.39f, 0.58f, 0.93f, 1.0f);
}

float defaultClearDepth (void)
{
	return 1.0f;
}

deUint32 defaultClearStencil (void)
{
	return 0;
}

VkClearDepthStencilValue defaultClearDepthStencilValue (void)
{
	VkClearDepthStencilValue clearDepthStencilValue;
	clearDepthStencilValue.depth	= defaultClearDepth();
	clearDepthStencilValue.stencil	= defaultClearStencil();

	return clearDepthStencilValue;
}

VkClearValue defaultClearValue (VkFormat clearFormat)
{
	VkClearValue clearValue;

	if (isDepthStencilFormat(clearFormat))
	{
		const VkClearDepthStencilValue dsValue = defaultClearDepthStencilValue();
		clearValue.ds.stencil	= dsValue.stencil;
		clearValue.ds.depth		= dsValue.depth;
	}
	else
	{
		const tcu::TextureFormat tcuClearFormat = mapVkFormat(clearFormat);
		if (isUintFormat(clearFormat))
		{
			const tcu::UVec4 defaultColor	= defaultClearColorUint(tcuClearFormat);
			clearValue.color.u32[0]			= defaultColor.x();
			clearValue.color.u32[1]			= defaultColor.y();
			clearValue.color.u32[2]			= defaultColor.z();
			clearValue.color.u32[3]			= defaultColor.w();
		}
		else if (isIntFormat(clearFormat))
		{
			const tcu::IVec4 defaultColor	= defaultClearColorInt(tcuClearFormat);
			clearValue.color.s32[0]			= defaultColor.x();
			clearValue.color.s32[1]			= defaultColor.y();
			clearValue.color.s32[2]			= defaultColor.z();
			clearValue.color.s32[3]			= defaultColor.w();
		}
		else
		{
			const tcu::Vec4 defaultColor	= defaultClearColorFloat(tcuClearFormat);
			clearValue.color.f32[0]			= defaultColor.x();
			clearValue.color.f32[1]			= defaultColor.y();
			clearValue.color.f32[2]			= defaultColor.z();
			clearValue.color.f32[3]			= defaultColor.w();
		}
	}

	return clearValue;
}

} // pipeline
} // vkt
