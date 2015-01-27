/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
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
 * \brief Boolean State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es31fBooleanStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "tcuRenderTarget.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

using namespace gls::StateQueryUtil;

static const char* getVerifierSuffix (QueryType type)
{
	switch (type)
	{
		case QUERY_ISENABLED:	return "isenabled";
		case QUERY_BOOLEAN:		return "getboolean";
		case QUERY_INTEGER:		return "getinteger";
		case QUERY_INTEGER64:	return "getinteger64";
		case QUERY_FLOAT:		return "getfloat";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

class IsEnabledStateTestCase : public TestCase, private glu::CallLogWrapper
{
public:
	IsEnabledStateTestCase (Context& context, QueryType verifier, const char* name, const char* description, glw::GLenum targetName, bool initial)
		: TestCase				(context, name, description)
		, glu::CallLogWrapper	(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
		, m_targetName			(targetName)
		, m_initial				(initial)
		, m_verifier			(verifier)
	{
	}

	IterateResult iterate (void)
	{
		tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
		enableLogging(true);

		// check inital value
		verifyStateBoolean(result, *this, m_targetName, m_initial, m_verifier);

		// check toggle

		GLU_CHECK_CALL(glEnable(m_targetName));

		verifyStateBoolean(result, *this, m_targetName, true, m_verifier);

		GLU_CHECK_CALL(glDisable(m_targetName));

		verifyStateBoolean(result, *this, m_targetName, false, m_verifier);

		result.setTestContextResult(m_testCtx);
		return STOP;
	}

private:
	const glw::GLenum		m_targetName;
	const bool				m_initial;
	const QueryType			m_verifier;
};

} // anonymous

BooleanStateQueryTests::BooleanStateQueryTests (Context& context)
	: TestCaseGroup(context, "boolean", "Boolean State Query tests")
{
}

BooleanStateQueryTests::~BooleanStateQueryTests (void)
{
}

void BooleanStateQueryTests::init (void)
{
	static const QueryType isEnabledVerifiers[] =
	{
		QUERY_ISENABLED,
		QUERY_BOOLEAN,
		QUERY_INTEGER,
		QUERY_INTEGER64,
		QUERY_FLOAT
	};

#define FOR_EACH_VERIFIER(VERIFIERS, X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); ++verifierNdx)	\
	{																						\
		const char* verifierSuffix = getVerifierSuffix(VERIFIERS[verifierNdx]);				\
		const QueryType verifier = VERIFIERS[verifierNdx];									\
		this->addChild(X);																	\
	}

	struct StateBoolean
	{
		const char*		name;
		const char*		description;
		glw::GLenum		targetName;
		bool			value;
	};
	const StateBoolean isEnableds[] =
	{
		{ "sample_mask",	"SAMPLE_MASK",	GL_SAMPLE_MASK,	false},
	};

	for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(isEnableds); testNdx++)
	{
		FOR_EACH_VERIFIER(isEnabledVerifiers, new IsEnabledStateTestCase(m_context, verifier, (std::string(isEnableds[testNdx].name) + "_" + verifierSuffix).c_str(), isEnableds[testNdx].description, isEnableds[testNdx].targetName, isEnableds[testNdx].value));
	}

#undef FOR_EACH_VERIFIER
}

} // Functional
} // gles31
} // deqp
