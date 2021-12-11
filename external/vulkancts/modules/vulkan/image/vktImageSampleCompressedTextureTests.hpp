#ifndef _VKTIMAGESAMPLECOMPRESSEDTEXTURETESTS_HPP
#define _VKTIMAGESAMPLECOMPRESSEDTEXTURETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
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
 * \brief Tests that images using a block-compressed format are sampled
 * correctly
 *
 * These tests create a storage image using a 128-bit or a 64-bit
 * block-compressed image format and an ImageView using an uncompressed
 * format. Each test case then fills the storage image with compressed
 * color values in a compute shader and samples the storage image. If the
 * sampled values are pure blue, the test passes.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace image
{

tcu::TestCaseGroup* createImageSampleDrawnTextureTests	(tcu::TestContext& testCtx);

} // image
} // vkt

#endif // _VKTIMAGESAMPLECOMPRESSEDTEXTURETESTS_HPP
