#ifndef _TCUTESTEXECUTOR_HPP
#define _TCUTESTEXECUTOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Base class for a test case.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "tcuTestContext.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestPackage.hpp"
#include "qpXmlWriter.h"

#include <vector>

namespace tcu
{

class CommandLine;

//! Test run summary.
class TestRunResult
{
public:
	TestRunResult (void) { clear(); }

	void clear (void)
	{
		numExecuted		= 0;
		numPassed		= 0;
		numFailed		= 0;
		numNotSupported	= 0;
		numWarnings		= 0;
		isComplete		= false;
	}

	int		numExecuted;		//!< Total number of cases executed.
	int		numPassed;			//!< Number of cases passed.
	int		numFailed;			//!< Number of cases failed.
	int		numNotSupported;	//!< Number of cases not supported.
	int		numWarnings;		//!< Number of QualityWarning / CompatibilityWarning results.
	bool	isComplete;			//!< Is run complete.
};

/*--------------------------------------------------------------------*//*!
 * \brief Test executor
 *
 * Test executor traverses TestNode hierarchy and executes the cases
 * included in current test case set. If no test case set is provided
 * all test cases in hierarchy are executed.
 *//*--------------------------------------------------------------------*/
class TestExecutor
{
public:
							TestExecutor		(TestContext& testCtx, const CommandLine& cmdLine);
							~TestExecutor		(void);

	bool					iterate				(void);

	const TestRunResult&	getResult			(void) const { return m_result;			}

	bool					isInTestCase		(void) const { return m_isInTestCase;	}

private:
	struct NodeIter
	{
		enum State
		{
			STATE_BEGIN = 0,
			STATE_TRAVERSE_CHILDREN,
			STATE_EXECUTE_TEST,
			STATE_FINISH,

			STATE_LAST
		};

		NodeIter (void)
			: node			(DE_NULL)
			, curChildNdx	(-1)
			, m_state		(STATE_LAST)
		{
		}

		NodeIter (TestNode* node_)
			: node			(node_)
			, curChildNdx	(-1)
			, m_state		(STATE_BEGIN)
		{
		}

		State getState (void) const
		{
			return m_state;
		}

		void setState (State newState)
		{
			switch (newState)
			{
				case STATE_TRAVERSE_CHILDREN:
					node->getChildren(children);
					curChildNdx = -1;
					break;

				default:
					// nada
					break;
			}

			m_state = newState;
		}

		TestNode*				node;
		std::vector<TestNode*>	children;
		int						curChildNdx;

	private:
		State					m_state;
	};

							TestExecutor		(const TestExecutor&);		// not allowed!
	TestExecutor&			operator=			(const TestExecutor&);		// not allowed!

	bool					matchFolderName		(const char* folderName) const;
	bool					matchCaseName		(const char* caseName) const;

	void					enterTestPackage	(TestPackage* testPackage, const char* packageName);
	void					leaveTestPackage	(TestPackage* testPackage);

	void					enterGroupNode		(TestCaseGroup* testGroup, const char* casePath);
	void					leaveGroupNode		(TestCaseGroup* testGroup);

	bool					enterTestCase		(TestCase* testCase, const char* casePath);
	void					leaveTestCase		(TestCase* testCase);

	// Member variables.
	TestContext&			m_testCtx;
	const CommandLine&		m_cmdLine;
	TestPackageRoot*		m_rootNode;

	TestCaseWrapper*		m_testCaseWrapper;

	FILE*					m_testCaseListFile;
	qpXmlWriter*			m_testCaseListWriter;

	// Current session state.
	std::vector<NodeIter>	m_sessionStack;
	bool					m_abortSession;
	bool					m_isInTestCase;

	TestRunResult			m_result;
};

} // tcu

#endif // _TCUTESTEXECUTOR_HPP
