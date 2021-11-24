#ifndef _VKTDRAWMULTIPLECLEARSWITHINRENDERPASS_HPP
#define _VKTDRAWMULTIPLECLEARSWITHINRENDERPASS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * -----------------------------
 *
 * Copyright (c) 2020 Google Inc.
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
 * \brief Test for multiple color or depth clears within a render pass
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace Draw
{
class MultipleClearsWithinRenderPassTests : public tcu::TestCaseGroup
{
public:
						MultipleClearsWithinRenderPassTests		(tcu::TestContext &testCtx, bool useDynamicRendering);
						~MultipleClearsWithinRenderPassTests	();
	void				init									();

private:
	MultipleClearsWithinRenderPassTests							(const MultipleClearsWithinRenderPassTests &other);
	MultipleClearsWithinRenderPassTests&	operator=			(const MultipleClearsWithinRenderPassTests &other);

private:
	const bool m_useDynamicRendering;
};
}
}
#endif // _VKTDRAWMULTIPLECLEARSWITHINRENDERPASS_HPP
