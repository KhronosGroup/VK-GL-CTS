#ifndef _VKTDRAWINDEXEDTEST_HPP
#define _VKTDRAWINDEXEDTEST_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Indexed Test
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

namespace vkt
{
namespace Draw
{
class DrawIndexedTests : public tcu::TestCaseGroup
{
public:
							DrawIndexedTests		(tcu::TestContext &testCtx, const SharedGroupParams groupParams);
							~DrawIndexedTests		(void);
	void					init					(void);

private:
	void					init					(bool useMaintenance5Ext);
	DrawIndexedTests								(const DrawIndexedTests &other);
	DrawIndexedTests&		operator=				(const DrawIndexedTests &other);

private:
	const SharedGroupParams	m_groupParams;
};
} // Draw
} // vkt

#endif // _VKTDRAWINDEXEDTEST_HPP
