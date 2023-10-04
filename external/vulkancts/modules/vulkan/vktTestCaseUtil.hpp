#ifndef _VKTTESTCASEUTIL_HPP
#define _VKTTESTCASEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
#include "tcuResource.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

class ShaderSourceProvider
{
public:
	static std::string getSource (tcu::Archive& archive, const char* path)
	{
		de::UniquePtr<tcu::Resource> resource(archive.getResource(path));

		std::vector<deUint8> readBuffer(resource->getSize() + 1);
		resource->read(&readBuffer[0], resource->getSize());
		readBuffer[readBuffer.size() - 1] = 0;

		return std::string(reinterpret_cast<const char*>(&readBuffer[0]));
	}
};

template<typename Arg0>
struct NoPrograms1
{
	void	init	(vk::SourceCollections&, Arg0) const {}
};

template<typename Instance, typename Arg0, typename Programs = NoPrograms1<Arg0> >
class InstanceFactory1 : public TestCase
{
public:
					InstanceFactory1	(tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const Arg0& arg0)
						: TestCase	(testCtx, name, desc)
						, m_progs	()
						, m_arg0	(arg0)
					{}

					InstanceFactory1	(tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const Programs& progs, const Arg0& arg0)
						: TestCase	(testCtx, name, desc)
						, m_progs	(progs)
						, m_arg0	(arg0)
					{}

	void			initPrograms	(vk::SourceCollections& dst)	const { m_progs.init(dst, m_arg0); }
	TestInstance*	createInstance	(Context& context)				const { return new Instance(context, m_arg0); }
	void			checkSupport	(Context&)						const { }

private:
	const Programs	m_progs;
	const Arg0		m_arg0;
};

template<typename Instance, typename Arg0, typename Support, typename Programs = NoPrograms1<Arg0> >
class InstanceFactory1WithSupport : public TestCase
{
public:

					InstanceFactory1WithSupport	(tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const Arg0& arg0, const Support& support)
						: TestCase	(testCtx, name, desc)
						, m_progs	()
						, m_arg0	(arg0)
						, m_support	(support)
					{}

					InstanceFactory1WithSupport	(tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const Programs& progs, const Arg0& arg0, const Support& support)
						: TestCase	(testCtx, name, desc)
						, m_progs	(progs)
						, m_arg0	(arg0)
						, m_support	(support)
					{}

	void			initPrograms	(vk::SourceCollections& dst)	const { m_progs.init(dst, m_arg0); }
	TestInstance*	createInstance	(Context& context)				const { return new Instance(context, m_arg0); }
	void			checkSupport	(Context& context)				const { m_support.checkSupport(context); }

private:
	const Programs	m_progs;
	const Arg0		m_arg0;
	const Support	m_support;
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

struct NoSupport0
{
	void			checkSupport	(Context&) const {}
};

class FunctionSupport0
{
public:
	typedef void	(*Function)	(Context& context);

					FunctionSupport0 (Function function)
						: m_function(function)
					{}

	void			checkSupport (Context& context) const { m_function(context); }

private:
	const Function	m_function;
};

template<typename Arg0>
struct NoSupport1
{
	void			checkSupport	(Context&, Arg0) const {}
};

template<typename Arg0>
class FunctionSupport1
{
public:
	typedef void	(*Function)	(Context& context, Arg0 arg0);

	struct Args
	{
		Args (Function func_, Arg0 arg0_)
			: func(func_)
			, arg0(arg0_)
		{}

		Function	func;
		Arg0		arg0;
	};

					FunctionSupport1 (const Args& args)
						: m_args(args)
					{}

	void			checkSupport (Context& context) const { return m_args.func(context, m_args.arg0); }

private:
	const Args		m_args;
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
									 const std::string&				name,
									 const std::string&				desc,
									 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1<FunctionInstance0, FunctionInstance0::Function>(testCtx, name, desc, testFunction);
}

inline TestCase* createFunctionCase (tcu::TestContext&				testCtx,
									 const std::string&				name,
									 const std::string&				desc,
									 FunctionSupport0::Function		checkSupport,
									 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1WithSupport<FunctionInstance0, FunctionInstance0::Function, FunctionSupport0>(testCtx, name, desc, testFunction, checkSupport);
}

inline TestCase* createFunctionCaseWithPrograms (tcu::TestContext&				testCtx,
												 const std::string&				name,
												 const std::string&				desc,
												 FunctionPrograms0::Function	initPrograms,
												 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1<FunctionInstance0, FunctionInstance0::Function, FunctionPrograms0>(
		testCtx, name, desc, FunctionPrograms0(initPrograms), testFunction);
}

inline TestCase* createFunctionCaseWithPrograms (tcu::TestContext&				testCtx,
												 const std::string&				name,
												 const std::string&				desc,
												 FunctionSupport0::Function		checkSupport,
												 FunctionPrograms0::Function	initPrograms,
												 FunctionInstance0::Function	testFunction)
{
	return new InstanceFactory1WithSupport<FunctionInstance0, FunctionInstance0::Function, FunctionSupport0, FunctionPrograms0>(
		testCtx, name, desc, FunctionPrograms0(initPrograms), testFunction, checkSupport);
}

template<typename Arg0>
TestCase* createFunctionCase (tcu::TestContext&								testCtx,
							  const std::string&							name,
							  const std::string&							desc,
							  typename FunctionInstance1<Arg0>::Function	testFunction,
							  Arg0											arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args>(
		testCtx, name, desc, typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

template<typename Arg0>
TestCase* createFunctionCase (tcu::TestContext&								testCtx,
							  const std::string&							name,
							  const std::string&							desc,
							  typename FunctionSupport1<Arg0>::Function		checkSupport,
							  typename FunctionInstance1<Arg0>::Function	testFunction,
							  Arg0											arg0)
{
	return new InstanceFactory1WithSupport<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args, FunctionSupport1<Arg0> >(
		testCtx, name, desc, typename FunctionInstance1<Arg0>::Args(testFunction, arg0), typename FunctionSupport1<Arg0>::Args(checkSupport, arg0));
}

template<typename Arg0>
TestCase* createFunctionCaseWithPrograms (tcu::TestContext&								testCtx,
										  const std::string&							name,
										  const std::string&							desc,
										  typename FunctionPrograms1<Arg0>::Function	initPrograms,
										  typename FunctionInstance1<Arg0>::Function	testFunction,
										  Arg0											arg0)
{
	return new InstanceFactory1<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args, FunctionPrograms1<Arg0> >(
		testCtx, name, desc, FunctionPrograms1<Arg0>(initPrograms), typename FunctionInstance1<Arg0>::Args(testFunction, arg0));
}

template<typename Arg0>
TestCase* createFunctionCaseWithPrograms (tcu::TestContext&								testCtx,
										  const std::string&							name,
										  const std::string&							desc,
										  typename FunctionSupport1<Arg0>::Function		checkSupport,
										  typename FunctionPrograms1<Arg0>::Function	initPrograms,
										  typename FunctionInstance1<Arg0>::Function	testFunction,
										  Arg0											arg0)
{
	return new InstanceFactory1WithSupport<FunctionInstance1<Arg0>, typename FunctionInstance1<Arg0>::Args, FunctionSupport1<Arg0>, FunctionPrograms1<Arg0> >(
		testCtx, name, desc, FunctionPrograms1<Arg0>(initPrograms), typename FunctionInstance1<Arg0>::Args(testFunction, arg0), typename FunctionSupport1<Arg0>::Args(checkSupport, arg0));
}

// addFunctionCase

inline void addFunctionCase (tcu::TestCaseGroup*			group,
							 const std::string&				name,
							 const std::string&				desc,
							 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCase(group->getTestContext(), name, desc, testFunc));
}

inline void addFunctionCase (tcu::TestCaseGroup*			group,
							 const std::string&				name,
							 const std::string&				desc,
							 FunctionSupport0::Function		checkSupport,
							 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCase(group->getTestContext(), name, desc, checkSupport, testFunc));
}

inline void addFunctionCaseWithPrograms (tcu::TestCaseGroup*			group,
										 const std::string&				name,
										 const std::string&				desc,
										 FunctionPrograms0::Function	initPrograms,
										 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCaseWithPrograms(group->getTestContext(), name, desc, initPrograms, testFunc));
}

inline void addFunctionCaseWithPrograms (tcu::TestCaseGroup*			group,
										 const std::string&				name,
										 const std::string&				desc,
										 FunctionSupport0::Function		checkSupport,
										 FunctionPrograms0::Function	initPrograms,
										 FunctionInstance0::Function	testFunc)
{
	group->addChild(createFunctionCaseWithPrograms(group->getTestContext(), name, desc, checkSupport, initPrograms, testFunc));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  Arg0											arg0)
{
	group->addChild(createFunctionCase<Arg0>(group->getTestContext(), name, desc, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCase (tcu::TestCaseGroup*							group,
					  const std::string&							name,
					  const std::string&							desc,
					  typename FunctionSupport1<Arg0>::Function		checkSupport,
					  typename FunctionInstance1<Arg0>::Function	testFunc,
					  Arg0											arg0)
{
	group->addChild(createFunctionCase<Arg0>(group->getTestContext(), name, desc, checkSupport, testFunc, arg0));
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
	group->addChild(createFunctionCaseWithPrograms<Arg0>(group->getTestContext(), name, desc, initPrograms, testFunc, arg0));
}

template<typename Arg0>
void addFunctionCaseWithPrograms (tcu::TestCaseGroup*							group,
								  const std::string&							name,
								  const std::string&							desc,
								  typename FunctionSupport1<Arg0>::Function		checkSupport,
								  typename FunctionPrograms1<Arg0>::Function	initPrograms,
								  typename FunctionInstance1<Arg0>::Function	testFunc,
								  Arg0											arg0)
{
	group->addChild(createFunctionCaseWithPrograms<Arg0>(group->getTestContext(), name, desc, checkSupport, initPrograms, testFunc, arg0));
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
