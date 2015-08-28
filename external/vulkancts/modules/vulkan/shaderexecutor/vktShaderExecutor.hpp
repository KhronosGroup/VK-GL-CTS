#ifndef _VKTSHADEREXECUTOR_HPP
#define _VKTSHADEREXECUTOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Vulkan ShaderExecutor
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vkPrograms.hpp"

#include "gluVarType.hpp"

#include <vector>

namespace vkt
{
namespace shaderexecutor
{

//! Shader input / output variable declaration.
struct Symbol
{
	std::string				name;		//!< Symbol name.
	glu::VarType			varType;	//!< Symbol type.


	Symbol (void) {}
	Symbol (const std::string& name_, const glu::VarType& varType_) : name(name_), varType(varType_) {}
};

//! Complete shader specification.
struct ShaderSpec
{
	std::vector<Symbol>		inputs;
	std::vector<Symbol>		outputs;
	std::string				globalDeclarations;	//!< These are placed into global scope. Can contain uniform declarations for example.
	std::string				source;				//!< Source snippet to be executed.

	ShaderSpec (void) {}
};

//! Base class for shader executor.
class ShaderExecutor
{
public:
	virtual					~ShaderExecutor		(void);

	//! Log executor details (program etc.).
	virtual void			log					(tcu::TestLog& log) const = 0;

	//! Execute
	virtual void			execute				(const Context& ctx, int numValues, const void* const* inputs, void* const* outputs) = 0;

	virtual void 			setShaderSources	(vk::SourceCollections& programCollection) const = 0;

protected:
							ShaderExecutor		(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);

	const ShaderSpec		m_shaderSpec;
	const glu::ShaderType	m_shaderType;
};

inline tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderExecutor* executor) { executor->log(log); return log; }
inline tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderExecutor& executor) { executor.log(log); return log; }

ShaderExecutor* createExecutor(glu::ShaderType shaderType, const ShaderSpec& shaderSpec);

} // shaderexecutor
} // vkt

#endif // _VKTSHADEREXECUTOR_HPP
