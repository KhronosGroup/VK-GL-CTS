#ifndef _VKTYPEUTIL_HPP
#define _VKTYPEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
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
 * \brief Utilities for creating commonly used composite types.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuVector.hpp"

namespace vk
{

#include "vkTypeUtil.inl"

inline VkClearValue makeClearValueColorF32 (float r, float g, float b, float a)
{
	VkClearValue v;
	v.color.float32[0] = r;
	v.color.float32[1] = g;
	v.color.float32[2] = b;
	v.color.float32[3] = a;
	return v;
}

inline VkClearValue makeClearValueColorU32 (deUint32 r, deUint32 g, deUint32 b, deUint32 a)
{
	VkClearValue v;
	v.color.uint32[0] = r;
	v.color.uint32[1] = g;
	v.color.uint32[2] = b;
	v.color.uint32[3] = a;
	return v;
}

inline VkClearValue makeClearValueColor (const tcu::Vec4& color)
{
	VkClearValue v;
	v.color.float32[0] = color[0];
	v.color.float32[1] = color[1];
	v.color.float32[2] = color[2];
	v.color.float32[3] = color[3];
	return v;
}

inline VkClearValue makeClearValueDepthStencil (float depth, deUint32 stencil)
{
	VkClearValue v;
	v.depthStencil.depth	= depth;
	v.depthStencil.stencil	= stencil;
	return v;
}

inline VkClearValue makeClearValue (VkClearColorValue color)
{
	VkClearValue v;
	v.color = color;
	return v;
}

inline VkComponentMapping makeComponentMappingRGBA (void)
{
	return makeComponentMapping(VK_COMPONENT_SWIZZLE_R,
								VK_COMPONENT_SWIZZLE_G,
								VK_COMPONENT_SWIZZLE_B,
								VK_COMPONENT_SWIZZLE_A);
}

inline VkExtent3D makeExtent3D(const tcu::IVec3& vec)
{
	return makeExtent3D((deUint32)vec.x(), (deUint32)vec.y(), (deUint32)vec.z());
}

inline VkExtent3D makeExtent3D(const tcu::UVec3& vec)
{
	return makeExtent3D(vec.x(), vec.y(), vec.z());
}

} // vk

#endif // _VKTYPEUTIL_HPP
