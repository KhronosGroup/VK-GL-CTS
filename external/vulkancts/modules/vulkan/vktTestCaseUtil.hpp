#ifndef _VKTTESTCASEUTIL_HPP
#define _VKTTESTCASEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief TestCase utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

template<typename Arg0>
struct NoPrograms1
{
	void	init	(vk::SourceCollections&, Arg0) const {}
};

template<typename Instance, typename Arg0, typename Programs = NoPrograms1<Arg0> >
class InstanceFactory1 : public TestCase
{
public:
					InstanceFactory1	(tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& desc, const Arg0& arg0)
						: TestCase	(testCtx, type, name, desc)
						, m_progs	()
						, m_arg0	(arg0)
					{}

					InstanceFactory1	(tcu::TestContext& testCtx, tcu::TestNodeType type, const std::string& name, const std::string& desc, const Programs& progs, const Arg0& arg0)
						: TestCase	(testCtx, type, name, desc)
						, m_progs	(progs)
						, m_arg0	(arg0)
					{}

	void			initPrograms		(vk::SourceCollections& dst) const { m_progs.init(dst, m_arg0); }
	TestInstance*	createInstance		(Context& context) const { return new Instance(context, m_arg0); }

private:
	const Programs	m_progs;
	const Arg0		m_arg0;
};

class FunctionInstance0 : public TestInstance
{
public:
	typedef tcu::TestStatus	(*Function)	(Context& context);

					FunctionInstance0	(Context& context, Function function)
						: TestInstance	(context)
						, m_function	(function)
					{}

	tcu::TestStatus	iterate				(void) { return m_function(m_context); }

private:
	const Function	m_function;
};

template<typename Arg0>
class FunctionInstance1 : public TestInstance
{
public:
	typedef tcu::TestStatus	(*Function)	(Context& context, Arg0 arg0);

	struct Args
	{
		Args (Function func_, Arg0 arg0_) : func(func_), arg0(arg0_) {}

		Function	func;
		Arg0		arg0;
	};

					FunctionInstance1	(Context& context, const Args& args)
						: TestInstance	(context)
						, m_args		(args)
					{}

	tcu::TestStatus	iterate				(void) { return m_args.func(m_context, m_args.arg0); }

private:
	const Args		m_args;
};

class FunctionPrograms0
{
public:
	typedef void	(*Function)		(vk::SourceCollections& dst);

					FunctionPrograms0	(Function func)
						: m_func(func)
					{}

	void			init			(vk::SourceCollections& dst, FunctionInstance0::Function) const { m_func(dst); }

private:
	const Function	m_func;
};

template<typename Arg0>
class FunctionPrograms1
{
public:
	typedef void	(*Function)		(vk::SourceCollections& dst, Arg0 arg0);

					FunctionPrograms1	(Function func)
						: m_func(func)
					{}

	void			init			(vk::SourceCollections& dst, const typename FunctionInstance1<Arg0>::Args& args) const { m_func(dst, args.arg0); }

private:
	const Function	m_func;
};

// createFunctionCase

inline TestCase* createFunctionCase (tcu::TestContext&				testCtx,
									 tcu::TestNodeType				type,
									 const std::string&				name,
									 const std::string&				desc,
									 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1<FunctionInstance0, FunctionInstance0::Function>(testCtx, type, name, desc, testFunction);
}

inline TestCase* createFunctionCaseWithPrograms (tcu::TestContext&				testCtx,
												 tcu::TestNodeType				type,
												 const std::string&				name,
												 const std::string&				desc,
												 FunctionPrograms0::Function	initPrograms,
												 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1<FunctionInstance0, FunctionInstance0::Function, FunctionPrograms0>(
		testCtx, type, name, desc, FunctionPrograms0(initPrograms), testFunction);
}

template<typename Arg0>
TestCase* createFunctionCase (tcu::TestContext&								testCtx,
							  tcu::TestNodeType								type,
							  const std::string&							name,
							  const std::string&							desc,
							  typename FunctionInstance1<Arg0>::Function	testFunction,
							  Arg0											arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args>(
		testCtx, type, name, desc, typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

template<typename Arg0>
TestCase* createFunctionCaseWithPrograms (tcu::TestContext&								testCtx,
										  tcu::TestNodeType								type,
										  const std::string&							name,
										  const std::string&							desc,
										  typename FunctionPrograms1<Arg0>::Function	initPrograms,
										  typename FunctionInstance1<Arg0>::Function	testFunction,
										  Arg0											arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args, FunctionPrograms1<Arg0> >(
		testCtx, type, name, desc, FunctionPrograms1<Arg0>(initPrograms), typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

// addFunctionCase

inline void addFunctionCase (tcu::TestCaseGroup*			group,
							 const std::string&				name,
							 const std::string&				desc,
							 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCase(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, testFunc));
}

inline void addFunctionCaseWithPrograms (tcu::TestCaseGroup*			group,
										 const std::string&				name,
										 const std::string&				desc,
										 FunctionPrograms0::Function	initPrograms,
										 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCaseWithPrograms(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, initPrograms, testFunc));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  Arg0											arg0)
{
	group->addChild(createFunctionCase<Arg0>(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  tcu::TestNodeType								type,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  Arg0											arg0)
{
	group->addChild(createFunctionCase<Arg0>(group->getTestContext(), type, name, desc, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCaseWithPrograms (tcu::TestCaseGroup*							group,
								  const std::string&							name,
								  const std::string&							desc,
								  typename FunctionPrograms1<Arg0>::Function	initPrograms,
								  typename FunctionInstance1<Arg0>::Function	testFunc,
								  Arg0											arg0)
{
	group->addChild(createFunctionCaseWithPrograms<Arg0>(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, initPrograms, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCaseWithPrograms (tcu::TestCaseGroup*							group,
								  tcu::TestNodeType								type,
								  const std::string&							name,
								  const std::string&							desc,
								  typename FunctionPrograms1<Arg0>::Function	initPrograms,
								  typename FunctionInstance1<Arg0>::Function	testFunc,
								  Arg0											arg0)
{
	group->addChild(createFunctionCaseWithPrograms<Arg0>(group->getTestContext(), type, name, desc, initPrograms, testFunc, arg0));
}

} // vkt

#endif // _VKTTESTCASEUTIL_HPP
