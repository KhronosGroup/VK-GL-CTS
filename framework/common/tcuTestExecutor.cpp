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
 * \brief Class for executing tests.
 *//*--------------------------------------------------------------------*/

#include "tcuTestExecutor.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestLog.hpp"

#include "deInt32.h"

#include <typeinfo>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using std::string;
using std::vector;

namespace tcu
{

TestExecutor::TestExecutor (TestContext& testCtx, const CommandLine& cmdLine)
	: m_testCtx				(testCtx)
	, m_cmdLine				(cmdLine)
	, m_rootNode			(DE_NULL)
	, m_testCaseWrapper		(DE_NULL)
	, m_testCaseListFile	(DE_NULL)
	, m_testCaseListWriter	(DE_NULL)
{
	m_abortSession	= false;
	m_isInTestCase	= false;

	// Create the root node.
	TestPackageRegistry*						packageRegistry	= TestPackageRegistry::getSingleton();
	vector<TestPackageRegistry::PackageInfo*>	packageInfos	= packageRegistry->getPackageInfos();
	vector<TestNode*>							testPackages;

	for (int i = 0; i < (int)packageInfos.size(); i++)
		testPackages.push_back(packageInfos[i]->createFunc(testCtx));

	m_rootNode = new TestPackageRoot(testCtx, testPackages);

	// Init traverse stack.
	NodeIter iter(m_rootNode);
	m_sessionStack.push_back(iter);
}

TestExecutor::~TestExecutor (void)
{
	if (m_testCaseListWriter)
		qpXmlWriter_destroy(m_testCaseListWriter);

	if (m_testCaseListFile)
		fclose(m_testCaseListFile);

	delete m_rootNode;
}

// Test sub-case iteration.
void TestExecutor::enterTestPackage (TestPackage* testPackage, const char* packageName)
{
	DE_ASSERT(testPackage && packageName);

	// Open file/writer for case dumping.
	const RunMode runMode = m_cmdLine.getRunMode();
	if (runMode == RUNMODE_DUMP_XML_CASELIST || runMode == RUNMODE_DUMP_TEXT_CASELIST)
	{
		const char* const	ext				= (runMode == RUNMODE_DUMP_XML_CASELIST) ? "xml" : "txt";
		const string		fileName		= string(packageName) + "-cases." + ext;

		print("Dumping all test case names in '%s' to file '%s'..\n", packageName, fileName.c_str());
		TCU_CHECK(m_testCaseListFile = fopen(fileName.c_str(), "wb"));

		if (runMode == RUNMODE_DUMP_XML_CASELIST)
		{
			TCU_CHECK(m_testCaseListWriter = qpXmlWriter_createFileWriter(m_testCaseListFile, DE_FALSE));

			qpXmlWriter_startDocument(m_testCaseListWriter);
			qpXmlWriter_startElement(m_testCaseListWriter, "TestCaseList", 0, DE_NULL);
		}
	}

	// Initialize package.
	testPackage->init();

	// Store test case wrapper
	m_testCaseWrapper = &testPackage->getTestCaseWrapper();
	DE_ASSERT(m_testCaseWrapper);

	// Set archive.
	m_testCtx.setCurrentArchive(testPackage->getArchive());
}

void TestExecutor::leaveTestPackage (TestPackage* testPackage)
{
	DE_ASSERT(testPackage);

	const RunMode runMode = m_cmdLine.getRunMode();
	if (runMode == RUNMODE_DUMP_XML_CASELIST)
	{
		qpXmlWriter_endElement(m_testCaseListWriter, "TestCaseList");
		qpXmlWriter_endDocument(m_testCaseListWriter);
		qpXmlWriter_destroy(m_testCaseListWriter);
		m_testCaseListWriter = DE_NULL;
	}

	if (runMode == RUNMODE_DUMP_TEXT_CASELIST || runMode == RUNMODE_DUMP_XML_CASELIST)
	{
		fclose(m_testCaseListFile);
		m_testCaseListFile = DE_NULL;
	}

	DE_ASSERT(!m_testCaseListWriter && !m_testCaseListFile);

	m_testCaseWrapper = DE_NULL;
	m_testCtx.setCurrentArchive(m_testCtx.getRootArchive());

	// Deinitialize package.
	testPackage->deinit();
}

void TestExecutor::enterGroupNode (TestCaseGroup* testGroup, const char* casePath)
{
	DE_UNREF(casePath);
	testGroup->init();
}

void TestExecutor::leaveGroupNode (TestCaseGroup* testGroup)
{
	testGroup->deinit();
}

static qpTestCaseType nodeTypeToTestCaseType (TestNodeType nodeType)
{
	switch (nodeType)
	{
		case NODETYPE_SELF_VALIDATE:	return QP_TEST_CASE_TYPE_SELF_VALIDATE;
		case NODETYPE_PERFORMANCE:		return QP_TEST_CASE_TYPE_PERFORMANCE;
		case NODETYPE_CAPABILITY:		return QP_TEST_CASE_TYPE_CAPABILITY;
		case NODETYPE_ACCURACY:			return QP_TEST_CASE_TYPE_ACCURACY;
		default:
			DE_ASSERT(DE_FALSE);
			return QP_TEST_CASE_TYPE_LAST;
	}
}

bool TestExecutor::enterTestCase (TestCase* testCase, const char* casePath)
{
	const RunMode			runMode		= m_cmdLine.getRunMode();
	const qpTestCaseType	caseType	= nodeTypeToTestCaseType(testCase->getNodeType());

	if (runMode == RUNMODE_EXECUTE)
	{
		print("\nTest case '%s'..\n", casePath);

		m_testCtx.getLog().startCase(casePath, caseType);
		m_isInTestCase = true;
		m_testCtx.setTestResult(QP_TEST_RESULT_LAST, "");

		if (!m_testCaseWrapper->initTestCase(testCase))
		{
			if (m_testCtx.getTestResult() == QP_TEST_RESULT_LAST)
				m_testCtx.setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, "Unexpected error in subcase init");
			return false;
		}
	}

	return true;
}

void TestExecutor::leaveTestCase (TestCase* testCase)
{
	const RunMode runMode = m_cmdLine.getRunMode();
	if (runMode == RUNMODE_EXECUTE)
	{
		// De-init case.
		const bool			deinitOk		= m_testCaseWrapper->deinitTestCase(testCase);
		const qpTestResult	testResult		= m_testCtx.getTestResult();
		const char* const	testResultDesc	= m_testCtx.getTestResultDesc();
		const bool			terminateAfter	= m_testCtx.getTerminateAfter();
		DE_ASSERT(testResult != QP_TEST_RESULT_LAST);

		m_isInTestCase = false;
		m_testCtx.getLog().endCase(testResult, testResultDesc);

		// Update statistics.
		print("  %s (%s)\n", qpGetTestResultName(testResult), testResultDesc);

		m_result.numExecuted += 1;
		switch (testResult)
		{
			case QP_TEST_RESULT_PASS:					m_result.numPassed			+= 1;	break;
			case QP_TEST_RESULT_NOT_SUPPORTED:			m_result.numNotSupported	+= 1;	break;
			case QP_TEST_RESULT_QUALITY_WARNING:		m_result.numWarnings		+= 1;	break;
			case QP_TEST_RESULT_COMPATIBILITY_WARNING:	m_result.numWarnings		+= 1;	break;
			default:									m_result.numFailed			+= 1;	break;
		}

		// terminateAfter, Resource error or any error in deinit means that execution should end
		if (terminateAfter || !deinitOk || testResult == QP_TEST_RESULT_RESOURCE_ERROR)
			m_abortSession = true;

		// \todo [2011-02-09 pyry] Disable watchdog temporarily?
		if (m_testCtx.getWatchDog())
			qpWatchDog_reset(m_testCtx.getWatchDog());
	}
}

// Return true while session should still continue, false otherwise.
bool TestExecutor::iterate (void)
{
	try
	{
		while (!m_sessionStack.empty())
		{
			// Get full path to node.
			string nodePath = "";
			for (int ndx = 0; ndx < (int)m_sessionStack.size(); ndx++)
			{
				NodeIter& iter = m_sessionStack[ndx];
				if (ndx > 1) // ignore root package
					nodePath += ".";
				nodePath += iter.node->getName();
			}

			// Handle the node.
			NodeIter& iter = m_sessionStack[m_sessionStack.size()-1];
			DE_ASSERT(iter.node != DE_NULL);
			TestNode*		node	= iter.node;
			bool			isLeaf	= isTestNodeTypeExecutable(node->getNodeType());

			switch (iter.getState())
			{
				case NodeIter::STATE_BEGIN:
				{
					// Return to parent if name doesn't match filter.
					if (!(isLeaf ? m_cmdLine.checkTestCaseName(nodePath.c_str()) : m_cmdLine.checkTestGroupName(nodePath.c_str())))
					{
						m_sessionStack.pop_back();
						break;
					}

					// Enter node.
					bool enterOk = true;
					switch (node->getNodeType())
					{
						case NODETYPE_ROOT:				/* nada */																	break;
						case NODETYPE_PACKAGE:			enterTestPackage(static_cast<TestPackage*>(node), nodePath.c_str());		break;
						case NODETYPE_GROUP:			enterGroupNode(static_cast<TestCaseGroup*>(node), nodePath.c_str());		break;
						case NODETYPE_PERFORMANCE:
						case NODETYPE_CAPABILITY:
						case NODETYPE_ACCURACY:			/* fall-trough */
						case NODETYPE_SELF_VALIDATE:	enterOk = enterTestCase(static_cast<TestCase*>(node), nodePath.c_str());	break;
						default: DE_ASSERT(false);
					}

					if (m_cmdLine.getRunMode() == RUNMODE_EXECUTE)
					{
						if (isLeaf)
						{
							if (enterOk)
								iter.setState(NodeIter::STATE_EXECUTE_TEST);
							else
								iter.setState(NodeIter::STATE_FINISH);
						}
						else
						{
							iter.setState(NodeIter::STATE_TRAVERSE_CHILDREN);
						}
					}
					else if (m_cmdLine.getRunMode() == RUNMODE_DUMP_XML_CASELIST)
					{
						if (node->getNodeType() != NODETYPE_ROOT && node->getNodeType() != NODETYPE_PACKAGE)
						{
							string			caseName	= iter.node->getName();
							string			description	= iter.node->getDescription();
							qpXmlAttribute	attribs[8];
							int				numAttribs = 0;
							const char*		caseType	= DE_NULL;

							switch (node->getNodeType())
							{
								case NODETYPE_SELF_VALIDATE:	caseType = "SelfValidate";	break;
								case NODETYPE_CAPABILITY:		caseType = "Capability";	break;
								case NODETYPE_ACCURACY:			caseType = "Accuracy";		break;
								case NODETYPE_PERFORMANCE:		caseType = "Performance";	break;
								default:						caseType = "TestGroup";		break;
							}

							attribs[numAttribs++] = qpSetStringAttrib("Name", caseName.c_str());
							attribs[numAttribs++] = qpSetStringAttrib("CaseType", caseType);
							attribs[numAttribs++] = qpSetStringAttrib("Description", description.c_str());
							qpXmlWriter_startElement(m_testCaseListWriter, "TestCase", numAttribs, attribs);
						}

						iter.setState(isLeaf ? NodeIter::STATE_FINISH : NodeIter::STATE_TRAVERSE_CHILDREN);
					}
					else if (m_cmdLine.getRunMode() == RUNMODE_DUMP_TEXT_CASELIST)
					{
						// \note Case list file is not open until we are in test package.
						if (isLeaf)
							fprintf(m_testCaseListFile, "TEST: %s\n", nodePath.c_str());
						else if (node->getNodeType() != NODETYPE_ROOT)
							fprintf(m_testCaseListFile, "GROUP: %s\n", nodePath.c_str());
						iter.setState(isLeaf ? NodeIter::STATE_FINISH : NodeIter::STATE_TRAVERSE_CHILDREN);
					}

					break;
				}

				case NodeIter::STATE_EXECUTE_TEST:
				{
					// Touch the watchdog.
					m_testCtx.touchWatchdog();

					// Iterate the sub-case.
					TestCase::IterateResult iterateResult = m_testCaseWrapper->iterateTestCase(static_cast<TestCase*>(node));

					if (iterateResult == TestCase::STOP)
						iter.setState(NodeIter::STATE_FINISH);

					return true; // return after each iteration (when another iteration follows).
				}

				case NodeIter::STATE_TRAVERSE_CHILDREN:
				{
					int numChildren = (int)iter.children.size();
					if (++iter.curChildNdx < numChildren)
					{
						// Push child to stack.
						TestNode* childNode = iter.children[iter.curChildNdx];
						m_sessionStack.push_back(NodeIter(childNode));
					}
					else
						iter.setState(NodeIter::STATE_FINISH);

					break;
				}

				case NodeIter::STATE_FINISH:
				{
					if (m_cmdLine.getRunMode() == RUNMODE_DUMP_XML_CASELIST)
					{
						if (node->getNodeType() != NODETYPE_ROOT && node->getNodeType() != NODETYPE_PACKAGE)
							qpXmlWriter_endElement(m_testCaseListWriter, "TestCase");
					}

					// Leave node.
					switch (node->getNodeType())
					{
						case NODETYPE_ROOT:				/* nada */											break;
						case NODETYPE_PACKAGE:			leaveTestPackage(static_cast<TestPackage*>(node));	break;
						case NODETYPE_GROUP:			leaveGroupNode(static_cast<TestCaseGroup*>(node));	break;
						case NODETYPE_ACCURACY:
						case NODETYPE_CAPABILITY:
						case NODETYPE_PERFORMANCE:		/* fall-thru */
						case NODETYPE_SELF_VALIDATE:	leaveTestCase(static_cast<TestCase*>(node));		break;
						default: DE_ASSERT(false);
					}

					m_sessionStack.pop_back();

					// Return if execution should abort.
					if (m_abortSession)
						return false;

					// Otherwise continue iterating.
					break;
				}

				default:
					DE_ASSERT(DE_FALSE);
					break;
			}
		}
	}
	catch (const std::exception& e)
	{
		print("TestExecutor::iterateSession(): Caught unhandled %s: %s\n", typeid(e).name(), e.what());
		throw;
	}

	m_result.isComplete = true;
	return false;
}

} // tcu
