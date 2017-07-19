#ifndef _VKTSHADERBUILTINPRECISIONTESTS_HPP
#define _VKTSHADERBUILTINPRECISIONTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Precision and range tests for builtins and types.
 *
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace shaderexecutor
{

class BuiltinPrecisionTests : public tcu::TestCaseGroup
{
public:
									BuiltinPrecisionTests				(tcu::TestContext& testCtx);
	virtual							~BuiltinPrecisionTests				(void);

	virtual void					init								(void);

private:
									BuiltinPrecisionTests				(const BuiltinPrecisionTests&);		// not allowed!
	BuiltinPrecisionTests&			operator=							(const BuiltinPrecisionTests&);		// not allowed!
};

class BuiltinPrecision16BitTests : public tcu::TestCaseGroup
{
public:
									BuiltinPrecision16BitTests			(tcu::TestContext& testCtx);
	virtual							~BuiltinPrecision16BitTests			(void);

	virtual void					init								(void);

private:
									BuiltinPrecision16BitTests			(const BuiltinPrecisionTests&);		// not allowed!
	BuiltinPrecision16BitTests&		operator=							(const BuiltinPrecisionTests&);		// not allowed!
};

class BuiltinPrecision16Storage32BitTests : public tcu::TestCaseGroup
{
public:
												BuiltinPrecision16Storage32BitTests	(tcu::TestContext& testCtx);
	virtual										~BuiltinPrecision16Storage32BitTests	(void);

	virtual void								init(void);

private:
												BuiltinPrecision16Storage32BitTests	(const BuiltinPrecisionTests&);		// not allowed!
	BuiltinPrecision16Storage32BitTests&		operator=								(const BuiltinPrecisionTests&);		// not allowed!
};

} // shaderexecutor
} // vkt

#endif // _VKTSHADERBUILTINPRECISIONTESTS_HPP
