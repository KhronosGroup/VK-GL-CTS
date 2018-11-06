#ifndef _VKTCONDITIONALCLEARATTACHMENTTESTS_HPP
#define _VKTCONDITIONALCLEARATTACHMENTTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdClearAttachments
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace conditional
{

class ConditionalClearAttachmentTests : public tcu::TestCaseGroup
{
public:
						ConditionalClearAttachmentTests	(tcu::TestContext &testCtx);
						~ConditionalClearAttachmentTests(void);
	void				init							(void);

private:
	ConditionalClearAttachmentTests						(const ConditionalClearAttachmentTests &other);
	ConditionalClearAttachmentTests&	operator=		(const ConditionalClearAttachmentTests &other);

};

} // conditional
} // vkt

#endif // _VKTCONDITIONALCLEARATTACHMENTTESTS_HPP
