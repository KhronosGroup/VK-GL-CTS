#ifndef _VKTDYNAMICSTATEINHERITANCETESTS_HPP
#define _VKTDYNAMICSTATEINHERITANCETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 NVIDIA Corporation
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
 * \brief VK_NV_inherited_viewport_scissor Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace DynamicState
{

class DynamicStateInheritanceTests : public tcu::TestCaseGroup
{
public:
	DynamicStateInheritanceTests (tcu::TestContext& testCtx);
	void init (void);
private:
	DynamicStateInheritanceTests (const DynamicStateInheritanceTests& other); // not implemented
	DynamicStateInheritanceTests& operator= (const DynamicStateInheritanceTests& other); // not implemented
};

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATEINHERITANCETESTS_HPP
