#ifndef _VKTPIPELINEIMAGEUTIL_HPP
#define _VKTPIPELINEIMAGEUTIL_HPP
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
 * \brief Utilities for images.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vkDefs.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "tcuTexture.hpp"

namespace vkt
{
namespace pipeline
{

/*--------------------------------------------------------------------*//*!
 * Gets a tcu::TextureLevel initialized with data from a VK color
 * attachment.
 *
 * The VkImage must be non-multisampled and able to be used as a source
 * operand for transfer operations.
 *//*--------------------------------------------------------------------*/
de::MovePtr<tcu::TextureLevel> readColorAttachment (const vk::DeviceInterface&	vk,
													vk::VkDevice				device,
													vk::VkQueue					queue,
													deUint32					queueFamilyIndex,
													vk::Allocator&				allocator,
													vk::VkImage					image,
													vk::VkFormat				format,
													const tcu::IVec2&			renderSize);

} // pipeline
} // vkt

#endif // _VKTPIPELINEIMAGEUTIL_HPP
