/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2018 The Android Open Source Project
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
 * \brief Built-in function tests for uniform constants.
 *//*--------------------------------------------------------------------*/

#include "es31fShaderUniformIntegerFunctionTests.hpp"
#include "glsShaderExecUtil.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include <iostream>

namespace deqp
{
namespace gles31
{
namespace Functional
{

using std::vector;
using std::string;
using tcu::TestLog;
using namespace gls::ShaderExecUtil;

class UniformIntegerFunctionCase : public TestCase
{
public:
								UniformIntegerFunctionCase				(Context& context, const char* description, int inputValue, glu::Precision precision, glu::ShaderType shaderType);
								~UniformIntegerFunctionCase				(void);

	void						init									(void);
	void						deinit									(void);
	IterateResult				iterate									(void);
	virtual const char*			getFunctionName()						= 0;
	virtual int					computeExpectedResult(deInt32 value)	= 0;

protected:
								UniformIntegerFunctionCase				(const UniformIntegerFunctionCase& other);
	UniformIntegerFunctionCase&	operator=								(const UniformIntegerFunctionCase& other);

private:
	ShaderSpec					m_spec;
	glu::ShaderType				m_shaderType;
	int							m_input;
	int							m_value;
	ShaderExecutor*				m_executor;
};

static std::string getCaseName (glu::Precision precision, glu::ShaderType shaderType);

UniformIntegerFunctionCase::UniformIntegerFunctionCase(Context& context, const char* description, int inputValue, glu::Precision precision, glu::ShaderType shaderType)
	: TestCase(context, getCaseName(precision, shaderType).c_str(), description)
	, m_shaderType(shaderType)
	, m_input(inputValue)
	, m_value(0)
	, m_executor(DE_NULL)
{
	m_spec.version = glu::GLSL_VERSION_310_ES;

	std::ostringstream oss;
	glu::VarType varType(glu::TYPE_INT, precision);
	oss << "uniform " << glu::declare(varType, "value", 0) << ";\n";
	m_spec.globalDeclarations = oss.str();
	m_spec.outputs.push_back(Symbol("result", glu::VarType(glu::TYPE_INT, glu::PRECISION_LOWP)));
	m_spec.outputs.push_back(Symbol("comparison", glu::VarType(glu::TYPE_BOOL, glu::PRECISION_LAST)));
}

UniformIntegerFunctionCase::~UniformIntegerFunctionCase(void)
{
	UniformIntegerFunctionCase::deinit();
}

void UniformIntegerFunctionCase::deinit(void)
{
	delete m_executor;
	m_executor = DE_NULL;
}

void UniformIntegerFunctionCase::init(void)
{
	std::ostringstream oss;
	oss <<
		"result = " << getFunctionName() << "(value);\n"
		"comparison = (" << getFunctionName() << "(value) == " << computeExpectedResult(m_input) << ");\n";
	m_spec.source = oss.str();

	DE_ASSERT(!m_executor);
	m_executor = createExecutor(m_context.getRenderContext(), m_shaderType, m_spec);
	m_testCtx.getLog() << m_executor;

	if (!m_executor->isOk())
		throw tcu::TestError("Compile failed");

	m_value = m_context.getRenderContext().getFunctions().getUniformLocation(m_executor->getProgram(), "value");
}

tcu::TestNode::IterateResult UniformIntegerFunctionCase::iterate(void)
{
	deInt32					result;
	deBool					comparison;
	vector<void*>			outputPointers	(2);

	outputPointers[0]= &result;
	outputPointers[1] = &comparison;

	m_executor->useProgram();
	m_context.getRenderContext().getFunctions().uniform1i(m_value, m_input);
	m_executor->execute(1, DE_NULL, &outputPointers[0]);

	int expectedResult = computeExpectedResult(m_input);
	if (result != expectedResult) {
		m_testCtx.getLog() << TestLog::Message << "ERROR: comparison failed for " << getFunctionName() << "(" << m_input << ") == " << expectedResult << TestLog::EndMessage;
		m_testCtx.getLog() << TestLog::Message << "input: " << m_input << TestLog::EndMessage;
		m_testCtx.getLog() << TestLog::Message << "result: " << result << TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Result comparison failed");
		return STOP;
	} else if (!comparison) {
		m_testCtx.getLog() << TestLog::Message << "ERROR: result is as expected, but not when use in condition statement (" << getFunctionName() << "(" << m_input << ") == " << expectedResult << ") == true" << TestLog::EndMessage;
		m_testCtx.getLog() << TestLog::Message << "input:" << m_input << TestLog::EndMessage;
		m_testCtx.getLog() << TestLog::Message << "result: " << result << TestLog::EndMessage;
		m_testCtx.getLog() << TestLog::Message << "comparison: " << comparison << TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Result comparison failed");
		return STOP;
	}
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

static std::string getCaseName (glu::Precision precision, glu::ShaderType shaderType)
{
	return string(getPrecisionName(precision)) + getShaderTypePostfix(shaderType);
}

static int findMSB (deInt32 value)
{
	if (value > 0)
		return 31 - deClz32((deUint32)value);
	else if (value < 0)
		return 31 - deClz32(~(deUint32)value);
	else
		return -1;
}

class FindMSBEdgeCase : public UniformIntegerFunctionCase {
public:
	FindMSBEdgeCase(Context& context, int inputValue, glu::Precision precision, glu::ShaderType shaderType)
		: UniformIntegerFunctionCase(context, "findMSB", inputValue, precision, shaderType)
	{
	}

protected:
	const char* getFunctionName() {
		return "findMSB";
	}

	int computeExpectedResult(deInt32 input) {
		return findMSB(input);
	}
};

static int findLSB (deUint32 value)
{
	for (int i = 0; i < 32; i++)
	{
		if (value & (1u<<i))
			return i;
	}
	return -1;
}

class FindLSBEdgeCase : public UniformIntegerFunctionCase {
public:
	FindLSBEdgeCase(Context& context, int inputValue, glu::Precision precision, glu::ShaderType shaderType)
		: UniformIntegerFunctionCase(context, "findLSB", inputValue, precision, shaderType)
	{
	}

protected:
	const char* getFunctionName() {
		return "findLSB";
	}

	int computeExpectedResult(deInt32 input) {
		return findLSB(input);
	}
};


template<class TestClass>
static void addFunctionCases (TestCaseGroup* parent, const char* functionName, int input)
{
	tcu::TestCaseGroup* group = new tcu::TestCaseGroup(parent->getTestContext(), functionName, functionName);
	parent->addChild(group);
	for (int prec = glu::PRECISION_LOWP; prec <= glu::PRECISION_HIGHP; prec++)
	{
		for (int shaderTypeNdx = 0; shaderTypeNdx < glu::SHADERTYPE_LAST; shaderTypeNdx++)
		{
			if (executorSupported(glu::ShaderType(shaderTypeNdx)))
			{
				group->addChild(new TestClass(parent->getContext(), input, glu::Precision(prec), glu::ShaderType(shaderTypeNdx)));
			}
		}
	}
}

ShaderUniformIntegerFunctionTests::ShaderUniformIntegerFunctionTests(Context& context)
	: TestCaseGroup(context, "uniform", "Function on uniform")
{
}

ShaderUniformIntegerFunctionTests::~ShaderUniformIntegerFunctionTests()
{
}
void ShaderUniformIntegerFunctionTests::init()
{
	addFunctionCases<FindMSBEdgeCase>(this, "findMSBZero", 0);
	addFunctionCases<FindMSBEdgeCase>(this, "findMSBMinusOne", -1);
	addFunctionCases<FindLSBEdgeCase>(this, "findLSBZero", 0);
	addFunctionCases<FindLSBEdgeCase>(this, "findLSBMinusOne", -1);
}

}
}
}
