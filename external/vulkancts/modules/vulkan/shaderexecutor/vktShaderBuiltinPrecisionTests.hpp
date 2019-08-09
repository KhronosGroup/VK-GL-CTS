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

									BuiltinPrecisionTests				(const BuiltinPrecisionTests&) = delete;
	BuiltinPrecisionTests&			operator=							(const BuiltinPrecisionTests&) = delete;
};

class BuiltinPrecision16BitTests : public tcu::TestCaseGroup
{
public:
									BuiltinPrecision16BitTests			(tcu::TestContext& testCtx);
	virtual							~BuiltinPrecision16BitTests			(void);

	virtual void					init								(void);

									BuiltinPrecision16BitTests			(const BuiltinPrecision16BitTests&) = delete;
	BuiltinPrecision16BitTests&		operator=							(const BuiltinPrecision16BitTests&) = delete;
};

class BuiltinPrecision16Storage32BitTests : public tcu::TestCaseGroup
{
public:
												BuiltinPrecision16Storage32BitTests	(tcu::TestContext& testCtx);
	virtual										~BuiltinPrecision16Storage32BitTests	(void);

	virtual void								init(void);

												BuiltinPrecision16Storage32BitTests	(const BuiltinPrecision16Storage32BitTests&) = delete;
	BuiltinPrecision16Storage32BitTests&		operator=							(const BuiltinPrecision16Storage32BitTests&) = delete;
};

class BuiltinPrecisionDoubleTests : public tcu::TestCaseGroup
{
public:
									BuiltinPrecisionDoubleTests			(tcu::TestContext& testCtx);
	virtual							~BuiltinPrecisionDoubleTests		(void);

	virtual void					init								(void);

									BuiltinPrecisionDoubleTests			(const BuiltinPrecisionDoubleTests&) = delete;
	BuiltinPrecisionDoubleTests&	operator=							(const BuiltinPrecisionDoubleTests&) = delete;
};

} // shaderexecutor
} // vkt

#endif // _VKTSHADERBUILTINPRECISIONTESTS_HPP
