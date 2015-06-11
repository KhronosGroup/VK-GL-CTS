#ifndef _VKTTESTCASEUTIL_HPP
#define _VKTTESTCASEUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief TestCase utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

template<typename Arg0>
struct NoPrograms1
{
	void	init	(vk::ProgramCollection<glu::ProgramSources>&, const Arg0&) const {}
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

	void			initPrograms		(vk::ProgramCollection<glu::ProgramSources>& dst) const { m_progs.init(dst, m_arg0); }
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
	typedef tcu::TestStatus	(*Function)	(Context& context, const Arg0& arg0);

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

template<typename Arg0>
class FunctionPrograms1
{
public:
	typedef void	(*Function)		(vk::ProgramCollection<glu::ProgramSources>& dst, const Arg0& arg0);

					FunctionPrograms1	(Function func)
						: m_func(func)
					{}

	void			init			(vk::ProgramCollection<glu::ProgramSources>& dst, const typename FunctionInstance1<Arg0>::Args& args) const { m_func(dst, args.arg0); }

private:
	const Function	m_func;
};

inline TestCase* createFunctionCase (tcu::TestContext&				testCtx,
									 tcu::TestNodeType				type,
									 const std::string&				name,
									 const std::string&				desc,
									 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1<FunctionInstance0, FunctionInstance0::Function>(testCtx, type, name, desc, testFunction);
}

template<typename Arg0>
TestCase* createFunctionCase (tcu::TestContext&								testCtx,
							  tcu::TestNodeType								type,
							  const std::string&							name,
							  const std::string&							desc,
							  typename FunctionInstance1<Arg0>::Function	testFunction,
							  const Arg0&									arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args>(
		testCtx, type, name, desc, typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

template<typename Arg0>
TestCase* createFunctionCase (tcu::TestContext&								testCtx,
							  tcu::TestNodeType								type,
							  const std::string&							name,
							  const std::string&							desc,
							  typename FunctionPrograms1<Arg0>::Function	initPrograms,
							  typename FunctionInstance1<Arg0>::Function	testFunction,
							  const Arg0&									arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args, FunctionPrograms1<Arg0> >(
		testCtx, type, name, desc, FunctionPrograms1<Arg0>(initPrograms), typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

inline void addFunctionCase (tcu::TestCaseGroup*			group,
							 const std::string&				name,
							 const std::string&				desc,
							 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCase(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, testFunc));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  const Arg0&									arg0)
{
	group->addChild(createFunctionCase(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  tcu::TestNodeType								type,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  const Arg0&									arg0)
{
	group->addChild(createFunctionCase(group->getTestContext(), type, name, desc, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionPrograms1<Arg0>::Function	initPrograms,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  const Arg0&									arg0)
{
	group->addChild(createFunctionCase(group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, initPrograms, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  tcu::TestNodeType								type,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionPrograms1<Arg0>::Function	initPrograms,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  const Arg0&									arg0)
{
	group->addChild(createFunctionCase(group->getTestContext(), type, name, desc, initPrograms, testFunc, arg0));
}

} // vkt

#endif // _VKTTESTCASEUTIL_HPP
