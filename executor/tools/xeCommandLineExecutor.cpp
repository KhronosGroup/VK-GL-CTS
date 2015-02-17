/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Command line test executor.
 *//*--------------------------------------------------------------------*/

#include "xeBatchExecutor.hpp"
#include "xeTestCaseListParser.hpp"
#include "xeTcpIpLink.hpp"
#include "xeLocalTcpIpLink.hpp"
#include "xeTestResultParser.hpp"
#include "xeTestLogWriter.hpp"
#include "deDirectoryIterator.hpp"
#include "deCommandLine.hpp"
#include "deString.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iostream>
#include <sstream>

// Command line arguments.
namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(StartServer,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(Host,			std::string);
DE_DECLARE_COMMAND_LINE_OPT(Port,			int);
DE_DECLARE_COMMAND_LINE_OPT(CaseListDir,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(TestSet,		std::vector<std::string>);
DE_DECLARE_COMMAND_LINE_OPT(ExcludeSet,		std::vector<std::string>);
DE_DECLARE_COMMAND_LINE_OPT(ContinueFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(TestLogFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(InfoLogFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(Summary,		bool);

// TargetConfiguration
DE_DECLARE_COMMAND_LINE_OPT(BinaryName,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(WorkingDir,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(CmdLineArgs,	std::string);

static void parseCommaSeparatedList (const char* src, std::vector<std::string>* dst)
{
	std::istringstream	inStr	(src);
	std::string			comp;

	while (std::getline(inStr, comp, ','))
		dst->push_back(comp);
}

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	static const NamedValue<bool> s_yesNo[] =
	{
		{ "yes",	true	},
		{ "no",		false	}
	};

	parser << Option<StartServer>	("s",		"start-server",	"Start local execserver",								"")
		   << Option<Host>			("c",		"connect",		"Connect to host",										"127.0.0.1")
		   << Option<Port>			("p",		"port",			"Select TCP port to use",								"50016")
		   << Option<CaseListDir>	("cd",		"caselistdir",	"Path to test case XML files",							".")
		   << Option<TestSet>		("t",		"testset",		"Test set",												parseCommaSeparatedList,	"")
		   << Option<ExcludeSet>	("e",		"exclude",		"Comma-separated list of exclude filters",				parseCommaSeparatedList,	"")
		   << Option<ContinueFile>	(DE_NULL,	"continue",		"Continue execution by initializing results from existing test log", "")
		   << Option<TestLogFile>	("o",		"out",			"Output test log filename",								"")
		   << Option<InfoLogFile>	("i",		"info",			"Output info log filename",								"")
		   << Option<Summary>		(DE_NULL,	"summary",		"Print summary at the end",								s_yesNo,	"yes")
		   << Option<BinaryName>	("b",		"binaryname",	"Test binary path, relative to working directory",		"")
		   << Option<WorkingDir>	("wd",		"workdir",		"Working directory for test execution",					"")
		   << Option<CmdLineArgs>	(DE_NULL,	"cmdline",		"Additional command line arguments for test binary",	"");
}

} // opt

using std::vector;
using std::string;

struct CommandLine
{
	CommandLine (void)
		: port		(0)
		, summary	(false)
	{
	}

	xe::TargetConfiguration		targetCfg;
	std::string					serverBin;
	std::string					host;
	int							port;
	std::string					caseListDir;
	std::vector<std::string>	testset;
	std::vector<std::string>	exclude;
	std::string					inFile;
	std::string					outFile;
	std::string					infoFile;
	bool						summary;
};

static bool parseCommandLine (CommandLine& cmdLine, int argc, const char* const* argv)
{
	de::cmdline::Parser			parser;
	de::cmdline::CommandLine	opts;

	XE_CHECK(argc >= 1);

	opt::registerOptions(parser);

	if (!parser.parse(argc-1, argv+1, &opts, std::cerr))
	{
		std::cout << argv[0] << " [options]\n";
		parser.help(std::cout);
		return false;
	}

	cmdLine.serverBin				= opts.getOption<opt::StartServer>();
	cmdLine.host					= opts.getOption<opt::Host>();
	cmdLine.port					= opts.getOption<opt::Port>();
	cmdLine.caseListDir				= opts.getOption<opt::CaseListDir>();
	cmdLine.testset					= opts.getOption<opt::TestSet>();
	cmdLine.exclude					= opts.getOption<opt::ExcludeSet>();
	cmdLine.inFile					= opts.getOption<opt::ContinueFile>();
	cmdLine.outFile					= opts.getOption<opt::TestLogFile>();
	cmdLine.infoFile				= opts.getOption<opt::InfoLogFile>();
	cmdLine.summary					= opts.getOption<opt::Summary>();
	cmdLine.targetCfg.binaryName	= opts.getOption<opt::BinaryName>();
	cmdLine.targetCfg.workingDir	= opts.getOption<opt::WorkingDir>();
	cmdLine.targetCfg.cmdLineArgs	= opts.getOption<opt::CmdLineArgs>();

	return true;
}

static bool checkCasePathPatternMatch (const char* pattern, const char* casePath, bool isTestGroup)
{
	int ptrnPos = 0;
	int casePos = 0;

	for (;;)
	{
		char c = casePath[casePos];
		char p = pattern[ptrnPos];

		if (p == '*')
		{
			/* Recurse to rest of positions. */
			int next = casePos;
			for (;;)
			{
				if (checkCasePathPatternMatch(pattern+ptrnPos+1, casePath+next, isTestGroup))
					return DE_TRUE;

				if (casePath[next] == 0)
					return DE_FALSE; /* No match found. */
				else
					next += 1;
			}
			DE_ASSERT(DE_FALSE);
		}
		else if (c == 0 && p == 0)
			return true;
		else if (c == 0)
		{
			/* Incomplete match is ok for test groups. */
			return isTestGroup;
		}
		else if (c != p)
			return false;

		casePos += 1;
		ptrnPos += 1;
	}

	DE_ASSERT(false);
	return false;
}

static void readCaseList (xe::TestGroup* root, const char* filename)
{
	xe::TestCaseListParser	caseListParser;
	std::ifstream			in				(filename, std::ios_base::binary);
	deUint8					buf[1024];

	XE_CHECK(in.good());

	caseListParser.init(root);

	for (;;)
	{
		in.read((char*)&buf[0], sizeof(buf));
		int numRead = (int)in.gcount();

		if (numRead > 0)
			caseListParser.parse(&buf[0], numRead);

		if (numRead < (int)sizeof(buf))
			break; // EOF
	}
}

static void readCaseLists (xe::TestRoot& root, const char* caseListDir)
{
	de::DirectoryIterator iter(caseListDir);

	for (; iter.hasItem(); iter.next())
	{
		de::FilePath item = iter.getItem();

		if (item.getType() == de::FilePath::TYPE_FILE)
		{
			std::string baseName = item.getBaseName();
			if (baseName.find("-cases.xml") == baseName.length()-10)
			{
				std::string		packageName	= baseName.substr(0, baseName.length()-10);
				xe::TestGroup*	package		= root.createGroup(packageName.c_str(), "");

				readCaseList(package, item.getPath());
			}
		}
	}
}

static void addMatchingCases (const xe::TestGroup& group, xe::TestSet& testSet, const char* filter)
{
	for (int childNdx = 0; childNdx < group.getNumChildren(); childNdx++)
	{
		const xe::TestNode* child		= group.getChild(childNdx);
		const bool			isGroup		= child->getNodeType() == xe::TESTNODETYPE_GROUP;
		const std::string	fullPath	= child->getFullPath();

		if (checkCasePathPatternMatch(filter, fullPath.c_str(), isGroup))
		{
			if (isGroup)
			{
				// Recurse into group.
				addMatchingCases(static_cast<const xe::TestGroup&>(*child), testSet, filter);
			}
			else
			{
				DE_ASSERT(child->getNodeType() == xe::TESTNODETYPE_TEST_CASE);
				testSet.add(child);
			}
		}
	}
}

static void removeMatchingCases (const xe::TestGroup& group, xe::TestSet& testSet, const char* filter)
{
	for (int childNdx = 0; childNdx < group.getNumChildren(); childNdx++)
	{
		const xe::TestNode* child		= group.getChild(childNdx);
		const bool			isGroup		= child->getNodeType() == xe::TESTNODETYPE_GROUP;
		const std::string	fullPath	= child->getFullPath();

		if (checkCasePathPatternMatch(filter, fullPath.c_str(), isGroup))
		{
			if (isGroup)
			{
				// Recurse into group.
				removeMatchingCases(static_cast<const xe::TestGroup&>(*child), testSet, filter);
			}
			else
			{
				DE_ASSERT(child->getNodeType() == xe::TESTNODETYPE_TEST_CASE);
				testSet.remove(child);
			}
		}
	}
}

class BatchResultHandler : public xe::TestLogHandler
{
public:
	BatchResultHandler (xe::BatchResult* batchResult)
		: m_batchResult(batchResult)
	{
	}

	void setSessionInfo (const xe::SessionInfo& sessionInfo)
	{
		m_batchResult->getSessionInfo() = sessionInfo;
	}

	xe::TestCaseResultPtr startTestCaseResult (const char* casePath)
	{
		// \todo [2012-11-01 pyry] What to do with duplicate results?
		if (m_batchResult->hasTestCaseResult(casePath))
			return m_batchResult->getTestCaseResult(casePath);
		else
			return m_batchResult->createTestCaseResult(casePath);
	}

	void testCaseResultUpdated (const xe::TestCaseResultPtr&)
	{
	}

	void testCaseResultComplete (const xe::TestCaseResultPtr&)
	{
	}

private:
	xe::BatchResult* m_batchResult;
};

static void readLogFile (xe::BatchResult* batchResult, const char* filename)
{
	std::ifstream		in		(filename, std::ifstream::binary|std::ifstream::in);
	BatchResultHandler	handler	(batchResult);
	xe::TestLogParser	parser	(&handler);
	deUint8				buf		[1024];
	int					numRead	= 0;

	for (;;)
	{
		in.read((char*)&buf[0], DE_LENGTH_OF_ARRAY(buf));
		numRead = (int)in.gcount();

		if (numRead <= 0)
			break;

		parser.parse(&buf[0], numRead);
	}

	in.close();
}

static void printBatchResultSummary (const xe::TestNode* root, const xe::TestSet& testSet, const xe::BatchResult& batchResult)
{
	int countByStatusCode[xe::TESTSTATUSCODE_LAST];
	std::fill(&countByStatusCode[0], &countByStatusCode[0]+DE_LENGTH_OF_ARRAY(countByStatusCode), 0);

	for (xe::ConstTestNodeIterator iter = xe::ConstTestNodeIterator::begin(root); iter != xe::ConstTestNodeIterator::end(root); ++iter)
	{
		const xe::TestNode* node = *iter;
		if (node->getNodeType() == xe::TESTNODETYPE_TEST_CASE && testSet.hasNode(node))
		{
			const xe::TestCase*				testCase		= static_cast<const xe::TestCase*>(node);
			std::string						fullPath;
			xe::TestStatusCode				statusCode		= xe::TESTSTATUSCODE_PENDING;
			testCase->getFullPath(fullPath);

			// Parse result data if such exists.
			if (batchResult.hasTestCaseResult(fullPath.c_str()))
			{
				xe::ConstTestCaseResultPtr	resultData	= batchResult.getTestCaseResult(fullPath.c_str());
				xe::TestCaseResult			result;
				xe::TestResultParser		parser;

				xe::parseTestCaseResultFromData(&parser, &result, *resultData.get());
				statusCode = result.statusCode;
			}

			countByStatusCode[statusCode] += 1;
		}
	}

	printf("\nTest run summary:\n");
	int totalCases = 0;
	for (int code = 0; code < xe::TESTSTATUSCODE_LAST; code++)
	{
		if (countByStatusCode[code] > 0)
			printf("  %20s: %5d\n", xe::getTestStatusCodeName((xe::TestStatusCode)code), countByStatusCode[code]);

		totalCases += countByStatusCode[code];
	}
	printf("  %20s: %5d\n", "Total", totalCases);
}

static void writeInfoLog (const xe::InfoLog& log, const char* filename)
{
	std::ofstream out(filename, std::ios_base::binary);
	XE_CHECK(out.good());
	out.write((const char*)log.getBytes(), log.getSize());
	out.close();
}

static xe::CommLink* createCommLink (const CommandLine& cmdLine)
{
	if (!cmdLine.serverBin.empty())
	{
		xe::LocalTcpIpLink* link = new xe::LocalTcpIpLink();
		try
		{
			link->start(cmdLine.serverBin.c_str(), DE_NULL, cmdLine.port);
			return link;
		}
		catch (...)
		{
			delete link;
			throw;
		}
	}
	else
	{
		de::SocketAddress address;
		address.setFamily(DE_SOCKETFAMILY_INET4);
		address.setProtocol(DE_SOCKETPROTOCOL_TCP);
		address.setHost(cmdLine.host.c_str());
		address.setPort(cmdLine.port);

		xe::TcpIpLink* link = new xe::TcpIpLink();
		try
		{
			link->connect(address);
			return link;
		}
		catch (...)
		{
			delete link;
			throw;
		}
	}
}

static void runExecutor (const CommandLine& cmdLine)
{
	xe::TestRoot root;

	// Read case list definitions.
	readCaseLists(root, cmdLine.caseListDir.c_str());

	// Build test set.
	xe::TestSet testSet;

	// Build test set.
	for (vector<string>::const_iterator filterIter = cmdLine.testset.begin(); filterIter != cmdLine.testset.end(); ++filterIter)
		addMatchingCases(root, testSet, filterIter->c_str());

	// Remove excluded cases.
	for (vector<string>::const_iterator filterIter = cmdLine.exclude.begin(); filterIter != cmdLine.exclude.end(); ++filterIter)
		removeMatchingCases(root, testSet, filterIter->c_str());

	// Initialize batch result.
	xe::BatchResult	batchResult;
	xe::InfoLog		infoLog;

	// Read existing results from input file (if supplied).
	if (!cmdLine.inFile.empty())
		readLogFile(&batchResult, cmdLine.inFile.c_str());

	// Initialize commLink.
	std::auto_ptr<xe::CommLink> commLink(createCommLink(cmdLine));

	xe::BatchExecutor executor(cmdLine.targetCfg, commLink.get(), &root, testSet, &batchResult, &infoLog);
	executor.run();

	commLink.reset();

	if (!cmdLine.outFile.empty())
	{
		xe::writeBatchResultToFile(batchResult, cmdLine.outFile.c_str());
		printf("Test log written to %s\n", cmdLine.outFile.c_str());
	}

	if (!cmdLine.infoFile.empty())
	{
		writeInfoLog(infoLog, cmdLine.infoFile.c_str());
		printf("Info log written to %s\n", cmdLine.infoFile.c_str());
	}

	if (cmdLine.summary)
		printBatchResultSummary(&root, testSet, batchResult);
}

int main (int argc, const char* const* argv)
{
	CommandLine cmdLine;

	if (!parseCommandLine(cmdLine, argc, argv))
		return -1;

	try
	{
		runExecutor(cmdLine);
	}
	catch (const std::exception& e)
	{
		printf("%s\n", e.what());
		return -1;
	}

	return 0;
}
