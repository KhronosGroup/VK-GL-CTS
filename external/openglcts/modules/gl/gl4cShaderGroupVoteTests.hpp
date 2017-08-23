#ifndef _GL4CSHADERGROUPVOTETESTS_HPP
#define _GL4CSHADERGROUPVOTETESTS_HPP
/*-------------------------------------------------------------------------
* OpenGL Conformance Test Suite
* -----------------------------
*
* Copyright (c) 2017 The Khronos Group Inc.
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
*/ /*!
* \file  gl4cShaderGroupVoteTests.hpp
* \brief Conformance tests for the ARB_shader_group_vote functionality.
*/ /*-------------------------------------------------------------------*/

#include "esextcTestCaseBase.hpp"
#include "glcTestCase.hpp"
#include "gluShaderProgram.hpp"

#include <string>

namespace gl4cts
{
class ShaderGroupVoteTestCaseBase : public glcts::TestCaseBase
{
public:
	class ComputeShader
	{
	private:
		/* Private members */
		std::string			m_name;
		std::string			m_shader;
		glu::ShaderProgram* m_program;
		tcu::Vec4			m_desiredColor;
		bool				m_compileOnly;

		/* Private methods */
		bool validateColor(tcu::Vec4 testedColor, tcu::Vec4 desiredColor);
		bool validateScreenPixels(deqp::Context& context, tcu::Vec4 desiredColor);

	public:
		/* Public methods */
		ComputeShader(const std::string& name, const std::string& shader);
		ComputeShader(const std::string& name, const std::string& shader, const tcu::Vec4& desiredColor);
		~ComputeShader();

		void create(deqp::Context& context);
		void execute(deqp::Context& context);
		void validate(deqp::Context& context);
	};

	/* Public methods */
	ShaderGroupVoteTestCaseBase(deqp::Context& context, const char* name, const char* description);

	void init();
	void deinit();

	tcu::TestNode::IterateResult iterate();

	typedef std::vector<ComputeShader*>::iterator ComputeShaderIter;

protected:
	/* Protected members */
	bool						m_extensionSupported;
	std::string					m_glslFunctionPostfix;
	std::vector<ComputeShader*> m_shaders;
};

/** Test verifies availability of new built-in functions and constants
**/
class ShaderGroupVoteAvailabilityTestCase : public ShaderGroupVoteTestCaseBase
{
public:
	/* Public methods */
	ShaderGroupVoteAvailabilityTestCase(deqp::Context& context);
};

class ShaderGroupVoteFunctionTestCaseBase : public ShaderGroupVoteTestCaseBase
{
protected:
	/* Protected members*/
	const char* m_shaderBase;

public:
	/* Public methods */
	ShaderGroupVoteFunctionTestCaseBase(deqp::Context& context, const char* name, const char* description);
};

/** Test verifies allInvocationsARB function calls
**/
class ShaderGroupVoteAllInvocationsTestCase : public ShaderGroupVoteFunctionTestCaseBase
{
public:
	/* Public methods */
	ShaderGroupVoteAllInvocationsTestCase(deqp::Context& context);
};

/** Test verifies anyInvocationARB function calls
**/
class ShaderGroupVoteAnyInvocationTestCase : public ShaderGroupVoteFunctionTestCaseBase
{
public:
	/* Public methods */
	ShaderGroupVoteAnyInvocationTestCase(deqp::Context& context);
};

/** Test verifies allInvocationsEqualARB function calls
**/
class ShaderGroupVoteAllInvocationsEqualTestCase : public ShaderGroupVoteFunctionTestCaseBase
{
public:
	/* Public methods */
	ShaderGroupVoteAllInvocationsEqualTestCase(deqp::Context& context);
};

/** Test group which encapsulates all ARB_shader_group_vote conformance tests */
class ShaderGroupVote : public deqp::TestCaseGroup
{
public:
	/* Public methods */
	ShaderGroupVote(deqp::Context& context);

	void init();

private:
	ShaderGroupVote(const ShaderGroupVote& other);
};

} /* glcts namespace */

#endif // _GL4CSHADERGROUPVOTETESTS_HPP
