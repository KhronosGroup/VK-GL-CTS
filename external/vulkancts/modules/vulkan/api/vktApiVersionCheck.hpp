#ifndef _VKTAPIVERSIONCHECK_HPP
#define _VKTAPIVERSIONCHECK_HPP
/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2017 Khronos Group
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
* \brief API Version Check test - prints out version info
*//*--------------------------------------------------------------------*/

namespace tcu
{
class TestCaseGroup;
class TestContext;
}

namespace vkt
{
namespace api
{

tcu::TestCaseGroup* createVersionSanityCheckTests (tcu::TestContext& testCtx);

} // api
} // vkt

#endif // _VKTAPIVERSIONCHECK_HPP
