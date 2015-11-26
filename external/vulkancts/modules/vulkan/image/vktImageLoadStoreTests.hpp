#ifndef _VKTIMAGELOADSTORETESTS_HPP
#define _VKTIMAGELOADSTORETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
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
 * \brief Image load/store Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "deUniquePtr.hpp"

namespace vkt
{
namespace image
{

tcu::TestCaseGroup*		createImageStoreTests				(tcu::TestContext& testCtx);
tcu::TestCaseGroup*		createImageLoadStoreTests			(tcu::TestContext& testCtx);
tcu::TestCaseGroup*		createImageFormatReinterpretTests	(tcu::TestContext& testCtx);

de::MovePtr<TestCase>	createImageQualifierRestrictCase	(tcu::TestContext& testCtx, const vk::ImageType imageType, const std::string& name);

} // image
} // vkt

#endif // _VKTIMAGELOADSTORETESTS_HPP
