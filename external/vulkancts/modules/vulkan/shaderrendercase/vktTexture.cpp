/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Texture utilities
 *//*--------------------------------------------------------------------*/

#include "vktTexture.hpp"

namespace vkt
{

vk::VkTexFilter getVkTexFilter (const tcu::Sampler::FilterMode& filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_FILTER_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_FILTER_NEAREST;
}

vk::VkTexMipmapMode getVkTexMipmapMode (const tcu::Sampler::FilterMode& filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_MIPMAP_MODE_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_MIPMAP_MODE_BASE;
}

vk::VkTexAddress getVkWrapMode (const tcu::Sampler::WrapMode& wrapMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(wrapMode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:		return vk::VK_TEX_ADDRESS_CLAMP;
		case tcu::Sampler::CLAMP_TO_BORDER:		return vk::VK_TEX_ADDRESS_CLAMP_BORDER;
		case tcu::Sampler::REPEAT_GL:			return vk::VK_TEX_ADDRESS_WRAP;
		case tcu::Sampler::REPEAT_CL:			return vk::VK_TEX_ADDRESS_WRAP;
		case tcu::Sampler::MIRRORED_REPEAT_GL:	return vk::VK_TEX_ADDRESS_MIRROR;
		case tcu::Sampler::MIRRORED_REPEAT_CL:	return vk::VK_TEX_ADDRESS_MIRROR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_ADDRESS_WRAP;
}

vk::VkCompareOp getVkCompareMode (const tcu::Sampler::CompareMode& mode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(mode)
	{
		case tcu::Sampler::COMPAREMODE_NONE:				return vk::VK_COMPARE_OP_NEVER;
		case tcu::Sampler::COMPAREMODE_LESS:				return vk::VK_COMPARE_OP_LESS;
		case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:		return vk::VK_COMPARE_OP_LESS_EQUAL;
		case tcu::Sampler::COMPAREMODE_GREATER:				return vk::VK_COMPARE_OP_GREATER;
		case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:	return vk::VK_COMPARE_OP_GREATER_EQUAL;
		case tcu::Sampler::COMPAREMODE_EQUAL:				return vk::VK_COMPARE_OP_EQUAL;
		case tcu::Sampler::COMPAREMODE_NOT_EQUAL:			return vk::VK_COMPARE_OP_NOT_EQUAL;
		case tcu::Sampler::COMPAREMODE_ALWAYS:				return vk::VK_COMPARE_OP_ALWAYS;
		case tcu::Sampler::COMPAREMODE_NEVER:				return vk::VK_COMPARE_OP_NEVER;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_COMPARE_OP_NEVER;
}

} // vkt
