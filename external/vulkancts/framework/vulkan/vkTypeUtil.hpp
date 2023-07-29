#ifndef _VKTYPEUTIL_HPP
#define _VKTYPEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Utilities for creating commonly used composite types.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuVector.hpp"

namespace vk
{

#include "vkTypeUtil.inl"

inline VkClearValue makeClearValueColorF32(float r, float g, float b, float a)
{
    VkClearValue v;
    v.color.float32[0] = r;
    v.color.float32[1] = g;
    v.color.float32[2] = b;
    v.color.float32[3] = a;
    return v;
}

inline VkClearValue makeClearValueColorVec4(tcu::Vec4 vec)
{
    return makeClearValueColorF32(vec.x(), vec.y(), vec.z(), vec.w());
}

inline VkClearValue makeClearValueColorU32(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    VkClearValue v;
    v.color.uint32[0] = r;
    v.color.uint32[1] = g;
    v.color.uint32[2] = b;
    v.color.uint32[3] = a;
    return v;
}

inline VkClearValue makeClearValueColorI32(int32_t r, int32_t g, int32_t b, int32_t a)
{
    VkClearValue v;
    v.color.int32[0] = r;
    v.color.int32[1] = g;
    v.color.int32[2] = b;
    v.color.int32[3] = a;
    return v;
}

inline VkClearValue makeClearValueColor(const tcu::Vec4 &color)
{
    VkClearValue v;
    v.color.float32[0] = color[0];
    v.color.float32[1] = color[1];
    v.color.float32[2] = color[2];
    v.color.float32[3] = color[3];
    return v;
}

inline VkClearValue makeClearValueDepthStencil(float depth, uint32_t stencil)
{
    VkClearValue v;
    v.depthStencil.depth   = depth;
    v.depthStencil.stencil = stencil;
    return v;
}

inline VkClearValue makeClearValue(VkClearColorValue color)
{
    VkClearValue v;
    v.color = color;
    return v;
}

inline VkComponentMapping makeComponentMappingRGBA(void)
{
    return makeComponentMapping(VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                VK_COMPONENT_SWIZZLE_A);
}

inline VkExtent3D makeExtent3D(const tcu::IVec3 &vec)
{
    return makeExtent3D((uint32_t)vec.x(), (uint32_t)vec.y(), (uint32_t)vec.z());
}

inline VkExtent3D makeExtent3D(const tcu::UVec3 &vec)
{
    return makeExtent3D(vec.x(), vec.y(), vec.z());
}

inline VkRect2D makeRect2D(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    VkRect2D r;
    r.offset.x      = x;
    r.offset.y      = y;
    r.extent.width  = width;
    r.extent.height = height;

    return r;
}

inline VkRect2D makeRect2D(const tcu::IVec2 &vec)
{
    return makeRect2D(0, 0, vec.x(), vec.y());
}

inline VkRect2D makeRect2D(const tcu::IVec3 &vec)
{
    return makeRect2D(0, 0, vec.x(), vec.y());
}

inline VkRect2D makeRect2D(const tcu::UVec2 &vec)
{
    return makeRect2D(0, 0, vec.x(), vec.y());
}

inline VkRect2D makeRect2D(const VkExtent3D &extent)
{
    return makeRect2D(0, 0, extent.width, extent.height);
}

inline VkRect2D makeRect2D(const VkExtent2D &extent)
{
    return makeRect2D(0, 0, extent.width, extent.height);
}

inline VkRect2D makeRect2D(const uint32_t width, const uint32_t height)
{
    return makeRect2D(0, 0, width, height);
}

inline VkViewport makeViewport(const tcu::IVec2 &vec)
{
    return makeViewport(0.0f, 0.0f, (float)vec.x(), (float)vec.y(), 0.0f, 1.0f);
}

inline VkViewport makeViewport(const tcu::IVec3 &vec)
{
    return makeViewport(0.0f, 0.0f, (float)vec.x(), (float)vec.y(), 0.0f, 1.0f);
}

inline VkViewport makeViewport(const tcu::UVec2 &vec)
{
    return makeViewport(0.0f, 0.0f, (float)vec.x(), (float)vec.y(), 0.0f, 1.0f);
}

inline VkViewport makeViewport(const VkExtent3D &extent)
{
    return makeViewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f);
}

inline VkViewport makeViewport(const VkExtent2D &extent)
{
    return makeViewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f);
}

inline VkViewport makeViewport(const uint32_t width, const uint32_t height)
{
    return makeViewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
}

} // namespace vk

#endif // _VKTYPEUTIL_HPP
