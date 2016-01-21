#ifndef _VKTTESTGROUPUTIL_HPP
#define _VKTTESTGROUPUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief TestCaseGroup utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{

class TestGroupHelper0 : public tcu::TestCaseGroup
{
public:
	typedef void (*CreateChildrenFunc) (tcu::TestCaseGroup* testGroup);

								TestGroupHelper0	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 CreateChildrenFunc		createChildren);
								~TestGroupHelper0	(void);

	void						init				(void);

private:
	const CreateChildrenFunc	m_createChildren;
};

template<typename Arg0>
class TestGroupHelper1 : public tcu::TestCaseGroup
{
public:
	typedef void (*CreateChildrenFunc) (tcu::TestCaseGroup* testGroup, Arg0 arg0);

								TestGroupHelper1	(tcu::TestContext&		testCtx,
													 const std::string&		name,
													 const std::string&		description,
													 CreateChildrenFunc		createChildren,
													 const Arg0&			arg0)
									: tcu::TestCaseGroup	(testCtx, name.c_str(), description.c_str())
									, m_createChildren		(createChildren)
									, m_arg0				(arg0)
								{}

	void						init				(void) { m_createChildren(this, m_arg0); }

private:
	const CreateChildrenFunc	m_createChildren;
	const Arg0					m_arg0;
};

inline tcu::TestCaseGroup* createTestGroup (tcu::TestContext&						testCtx,
											const std::string&						name,
											const std::string&						description,
											TestGroupHelper0::CreateChildrenFunc	createChildren)
{
	return new TestGroupHelper0(testCtx, name, description, createChildren);
}

template<typename Arg0>
tcu::TestCaseGroup* createTestGroup (tcu::TestContext&										testCtx,
									 const std::string&										name,
									 const std::string&										description,
									 typename TestGroupHelper1<Arg0>::CreateChildrenFunc	createChildren,
									 Arg0													arg0)
{
	return new TestGroupHelper1<Arg0>(testCtx, name, description, createChildren, arg0);
}

} // vkt

#endif // _VKTTESTGROUPUTIL_HPP
