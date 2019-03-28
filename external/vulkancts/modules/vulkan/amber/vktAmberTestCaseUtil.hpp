#ifndef _VKTAMBERTESTCASEUTIL_HPP
#define _VKTAMBERTESTCASEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace cts_amber
{

class AmberTestCase : public tcu::TestNode
{
};

AmberTestCase* createAmberTestCase (tcu::TestContext&	testCtx,
									const char*			name,
									const char*			description,
									const char*			category,
									const std::string&	filename);

} // cts_amber
} // vkt

#endif // _VKTAMBERTESTCASEUTIL_HPP
