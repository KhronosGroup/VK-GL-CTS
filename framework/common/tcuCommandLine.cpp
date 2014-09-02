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
 * \brief Command line parsing.
 *//*--------------------------------------------------------------------*/

#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "deFilePath.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "deInt32.h"
#include "deCommandLine.h"
#include "qpTestLog.h"
#include "qpDebugOut.h"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>

using std::string;
using std::vector;

// OOM tests are enabled by default only on platforms that don't do memory overcommit (Win32)
#if (DE_OS == DE_OS_WIN32)
#	define TEST_OOM_DEFAULT		"enable"
#else
#	define TEST_OOM_DEFAULT		"disable"
#endif

namespace tcu
{

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(CasePath,			std::string);
DE_DECLARE_COMMAND_LINE_OPT(CaseList,			std::string);
DE_DECLARE_COMMAND_LINE_OPT(CaseListFile,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(StdinCaseList,		bool);
DE_DECLARE_COMMAND_LINE_OPT(LogFilename,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(RunMode,			tcu::RunMode);
DE_DECLARE_COMMAND_LINE_OPT(WatchDog,			bool);
DE_DECLARE_COMMAND_LINE_OPT(CrashHandler,		bool);
DE_DECLARE_COMMAND_LINE_OPT(BaseSeed,			int);
DE_DECLARE_COMMAND_LINE_OPT(TestIterationCount,	int);
DE_DECLARE_COMMAND_LINE_OPT(Visibility,			WindowVisibility);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceWidth,		int);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceHeight,		int);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceType,		tcu::SurfaceType);
DE_DECLARE_COMMAND_LINE_OPT(ScreenRotation,		tcu::ScreenRotation);
DE_DECLARE_COMMAND_LINE_OPT(GLContextType,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(GLConfigID,			int);
DE_DECLARE_COMMAND_LINE_OPT(GLConfigName,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(GLContextFlags,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(CLPlatformID,		int);
DE_DECLARE_COMMAND_LINE_OPT(CLDeviceIDs,		std::vector<int>);
DE_DECLARE_COMMAND_LINE_OPT(CLBuildOptions,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLDisplayType,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLWindowType,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLPixmapType,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(LogImages,			bool);
DE_DECLARE_COMMAND_LINE_OPT(TestOOM,			bool);

static void parseIntList (const char* src, std::vector<int>* dst)
{
	std::istringstream	str	(src);
	std::string			val;

	while (std::getline(str, val, ','))
	{
		int intVal = 0;
		de::cmdline::parseType(val.c_str(), &intVal);
		dst->push_back(intVal);
	}
}

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	static const NamedValue<bool> s_enableNames[] =
	{
		{ "enable",		true	},
		{ "disable",	false	}
	};
	static const NamedValue<tcu::RunMode> s_runModes[] =
	{
		{ "execute",		RUNMODE_EXECUTE				},
		{ "xml-caselist",	RUNMODE_DUMP_XML_CASELIST	},
		{ "txt-caselist",	RUNMODE_DUMP_TEXT_CASELIST	}
	};
	static const NamedValue<WindowVisibility> s_visibilites[] =
	{
		{ "windowed",		WINDOWVISIBILITY_WINDOWED	},
		{ "fullscreen",		WINDOWVISIBILITY_FULLSCREEN	},
		{ "hidden",			WINDOWVISIBILITY_HIDDEN		}
	};
	static const NamedValue<tcu::SurfaceType> s_surfaceTypes[] =
	{
		{ "window",			SURFACETYPE_WINDOW				},
		{ "pixmap",			SURFACETYPE_OFFSCREEN_NATIVE	},
		{ "pbuffer",		SURFACETYPE_OFFSCREEN_GENERIC	},
		{ "fbo",			SURFACETYPE_FBO					}
	};
	static const NamedValue<tcu::ScreenRotation> s_screenRotations[] =
	{
		{ "0",				SCREENROTATION_0			},
		{ "90",				SCREENROTATION_90			},
		{ "180",			SCREENROTATION_180			},
		{ "270",			SCREENROTATION_270			}
	};

	parser
		<< Option<CasePath>				("n",		"deqp-case",					"Test case(s) to run, supports wildcards (e.g. dEQP-GLES2.info.*)")
		<< Option<CaseList>				(DE_NULL,	"deqp-caselist",				"Case list to run in trie format (e.g. {dEQP-GLES2{info{version,renderer}}})")
		<< Option<CaseListFile>			(DE_NULL,	"deqp-caselist-file",			"Read case list (in trie format) from given file")
		<< Option<StdinCaseList>		(DE_NULL,	"deqp-stdin-caselist",			"Read case list (in trie format) from stdin")
		<< Option<LogFilename>			(DE_NULL,	"deqp-log-filename",			"Write test results to given file",					"TestResults.qpa")
		<< Option<RunMode>				(DE_NULL,	"deqp-runmode",					"Execute tests, or write list of test cases into a file",
																					s_runModes, "execute")
		<< Option<WatchDog>				(DE_NULL,	"deqp-watchdog",				"Enable test watchdog",								s_enableNames,		"disable")
		<< Option<CrashHandler>			(DE_NULL,	"deqp-crashhandler",			"Enable crash handling",							s_enableNames,		"disable")
		<< Option<BaseSeed>				(DE_NULL,	"deqp-base-seed",				"Base seed for test cases that use randomization")
		<< Option<TestIterationCount>	(DE_NULL,	"deqp-test-iteration-count",	"Iteration count for cases that support variable number of iterations")
		<< Option<Visibility>			(DE_NULL,	"deqp-visibility",				"Default test window visibility",					s_visibilites,		"windowed")
		<< Option<SurfaceWidth>			(DE_NULL,	"deqp-surface-width",			"Use given surface width if possible",	"-1")
		<< Option<SurfaceHeight>		(DE_NULL,	"deqp-surface-height",			"Use given surface height if possible",	"-1")
		<< Option<SurfaceType>			(DE_NULL,	"deqp-surface-type",			"Use given surface type",							s_surfaceTypes,		"window")
		<< Option<ScreenRotation>		(DE_NULL,	"deqp-screen-rotation",			"Screen rotation for platforms that support it",	s_screenRotations,	"0")
		<< Option<GLContextType>		(DE_NULL,	"deqp-gl-context-type",			"OpenGL context type for platforms that support multiple")
		<< Option<GLConfigID>			(DE_NULL,	"deqp-gl-config-id",			"OpenGL (ES) render config ID (EGL config id on EGL platforms)",	"-1")
		<< Option<GLConfigName>			(DE_NULL,	"deqp-gl-config-name",			"Symbolic OpenGL (ES) render config name")
		<< Option<GLContextFlags>		(DE_NULL,	"deqp-gl-context-flags",		"OpenGL context flags (comma-separated, supports debug and robust)")
		<< Option<CLPlatformID>			(DE_NULL,	"deqp-cl-platform-id",			"Execute tests on given OpenCL platform (IDs start from 1)",		"1")
		<< Option<CLDeviceIDs>			(DE_NULL,	"deqp-cl-device-ids",			"Execute tests on given CL devices (comma-separated, IDs start from 1)",	parseIntList)
		<< Option<CLBuildOptions>		(DE_NULL,	"deqp-cl-build-options",		"Extra build options for OpenCL compiler")
		<< Option<EGLDisplayType>		(DE_NULL,	"deqp-egl-display-type",		"EGL native display type")
		<< Option<EGLWindowType>		(DE_NULL,	"deqp-egl-window-type",			"EGL native window type")
		<< Option<EGLPixmapType>		(DE_NULL,	"deqp-egl-pixmap-type",			"EGL native pixmap type")
		<< Option<LogImages>			(DE_NULL,	"deqp-log-images",				"Enable or disable logging of result images",		s_enableNames,		"enable")
		<< Option<TestOOM>				(DE_NULL,	"deqp-test-oom",				"Run tests that exhaust memory on purpose",			s_enableNames,		TEST_OOM_DEFAULT);
}

void registerLegacyOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;

	parser
		<< Option<GLConfigID>			(DE_NULL,	"deqp-egl-config-id",			"Legacy name for --deqp-gl-config-id",	"-1")
		<< Option<GLConfigName>			(DE_NULL,	"deqp-egl-config-name",			"Legacy name for --deqp-gl-config-name");
}

} // opt

// \todo [2014-02-13 pyry] This could be useful elsewhere as well.
class DebugOutStreambuf : public std::streambuf
{
public:
						DebugOutStreambuf	(void);
						~DebugOutStreambuf	(void);

protected:
	std::streamsize		xsputn				(const char* s, std::streamsize count);
	int					overflow			(int ch = -1);

private:
	void				flushLine			(void);

	std::ostringstream	m_curLine;
};

DebugOutStreambuf::DebugOutStreambuf (void)
{
}

DebugOutStreambuf::~DebugOutStreambuf (void)
{
	if (m_curLine.tellp() != std::streampos(0))
		flushLine();
}

std::streamsize DebugOutStreambuf::xsputn (const char* s, std::streamsize count)
{
	for (std::streamsize pos = 0; pos < count; pos++)
	{
		m_curLine.put(s[pos]);

		if (s[pos] == '\n')
			flushLine();
	}

	return count;
}

int DebugOutStreambuf::overflow (int ch)
{
	if (ch == -1)
		return -1;
	else
	{
		DE_ASSERT((ch & 0xff) == ch);
		const char chVal = (char)(deUint8)(ch & 0xff);
		return xsputn(&chVal, 1) == 1 ? ch : -1;
	}
}

void DebugOutStreambuf::flushLine (void)
{
	qpPrint(m_curLine.str().c_str());
	m_curLine.str("");
}

class CaseTreeNode
{
public:
										CaseTreeNode		(const std::string& name) : m_name(name) {}
										~CaseTreeNode		(void);

	void								addChild			(CaseTreeNode* child) { m_children.push_back(child); }

	const std::string&					getName				(void) const { return m_name;		}
	const std::vector<CaseTreeNode*>&	getChildren			(void) const { return m_children;	}

private:
										CaseTreeNode		(const CaseTreeNode&);
	CaseTreeNode&						operator=			(const CaseTreeNode&);

	std::string							m_name;
	std::vector<CaseTreeNode*>			m_children;
};

CaseTreeNode::~CaseTreeNode (void)
{
	for (vector<CaseTreeNode*>::const_iterator i = m_children.begin(); i != m_children.end(); ++i)
		delete *i;
}

static CaseTreeNode* parseCaseTree (std::istream& in)
{
	vector<CaseTreeNode*>	nodeStack;
	string					curName;

	if (in.get() != '{')
		throw std::invalid_argument("Malformed case tree");

	nodeStack.reserve(1);
	nodeStack.push_back(new CaseTreeNode(""));

	try
	{
		for (;;)
		{
			const int	curChr	= in.get();

			if (curChr == std::char_traits<char>::eof() || curChr == 0)
				break;

			if (nodeStack.empty())
				throw std::invalid_argument("Trailing characters at end of case tree");

			if (!curName.empty() && (curChr == '{' || curChr == ',' || curChr == '}'))
			{
				// Create child and push to stack.
				nodeStack.reserve(nodeStack.size()+1);
				nodeStack.push_back(new CaseTreeNode(curName));

				curName.clear();
			}

			if (curChr == ',' || curChr == '}')
			{
				// Attach to parent
				if (nodeStack.size() < 2)
					throw std::invalid_argument("Malformed case tree");

				(*(nodeStack.end()-2))->addChild(nodeStack.back());
				nodeStack.pop_back();
			}
			else if (curChr != '{')
				curName += (char)curChr;
		}

		if (nodeStack.size() != 1 || nodeStack[0]->getName() != "")
			throw std::invalid_argument("Unterminated case tree");
	}
	catch (...)
	{
		// Nodes in stack are not attached to any parents and must be deleted individually.
		for (vector<CaseTreeNode*>::const_iterator i = nodeStack.begin(); i != nodeStack.end(); ++i)
			delete *i;

		throw;
	}

	return nodeStack[0];
}

class CasePaths
{
public:
							CasePaths	(const string& pathList);
	bool					matches		(const string& caseName, bool allowPrefix=false) const;

private:
	const vector<string>	m_casePatterns;
};

CasePaths::CasePaths (const string& pathList)
	: m_casePatterns(de::splitString(pathList, ','))
{
}

// Match a single path component against a pattern component that may contain *-wildcards.
static bool matchWildcards(string::const_iterator	patternStart,
						   string::const_iterator	patternEnd,
						   string::const_iterator	pathStart,
						   string::const_iterator	pathEnd,
						   bool						allowPrefix)
{
	string::const_iterator	pattern	= patternStart;
	string::const_iterator	path	= pathStart;

	while (pattern != patternEnd && path != pathEnd && *pattern == *path)
	{
		++pattern;
		++path;
	}

	if (pattern == patternEnd)
		return (path == pathEnd);
	else if (*pattern == '*')
	{
		for (; path != pathEnd; ++path)
		{
			if (matchWildcards(pattern + 1, patternEnd, path, pathEnd, allowPrefix))
				return true;
		}

		if (matchWildcards(pattern + 1, patternEnd, pathEnd, pathEnd, allowPrefix))
			return true;
	}
	else if (path == pathEnd && allowPrefix)
		return true;

	return false;
}

#if defined(TCU_HIERARCHICAL_CASEPATHS)
// Match a list of pattern components to a list of path components. A pattern
// component may contain *-wildcards. A pattern component "**" matches zero or
// more whole path components.
static bool patternMatches(vector<string>::const_iterator	patternStart,
						   vector<string>::const_iterator	patternEnd,
						   vector<string>::const_iterator	pathStart,
						   vector<string>::const_iterator	pathEnd,
						   bool								allowPrefix)
{
	vector<string>::const_iterator	pattern	= patternStart;
	vector<string>::const_iterator	path	= pathStart;

	while (pattern != patternEnd && path != pathEnd && *pattern != "**" &&
		   (*pattern == *path || matchWildcards(pattern->begin(), pattern->end(),
												path->begin(), path->end(), false)))
	{
		++pattern;
		++path;
	}

	if (path == pathEnd && (allowPrefix || pattern == patternEnd))
		return true;
	else if (pattern != patternEnd && *pattern == "**")
	{
		for (; path != pathEnd; ++path)
			if (patternMatches(pattern + 1, patternEnd, path, pathEnd, allowPrefix))
				return true;
		if (patternMatches(pattern + 1, patternEnd, path, pathEnd, allowPrefix))
			return true;
	}

	return false;
}
#endif

bool CasePaths::matches (const string& caseName, bool allowPrefix) const
{
	const vector<string> components = de::splitString(caseName, '.');

	for (size_t ndx = 0; ndx < m_casePatterns.size(); ++ndx)
	{
#if defined(TCU_HIERARCHICAL_CASEPATHS)
		const vector<string> patternComponents = de::splitString(m_casePatterns[ndx], '.');

		if (patternMatches(patternComponents.begin(), patternComponents.end(),
						   components.begin(), components.end(), allowPrefix))
			return true;
#else
		if (matchWildcards(m_casePatterns[ndx].begin(), m_casePatterns[ndx].end(),
						   caseName.begin(), caseName.end(), allowPrefix))
			return true;
#endif
	}

	return false;
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line
 * \note CommandLine is not fully initialized until parse() has been called.
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine (void)
	: m_logFlags	(0)
	, m_caseTree	(DE_NULL)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line from standard argc, argv pair.
 *
 * Calls parse() with given arguments
 * \param argc Number of arguments
 * \param argv Command line arguments
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine (int argc, const char* const* argv)
	: m_logFlags	(0)
	, m_caseTree	(DE_NULL)
{
	if (!parse(argc, argv))
		throw Exception("Failed to parse command line");
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line from string.
 *
 * Calls parse() with given argument.
 * \param cmdLine Full command line string.
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine (const std::string& cmdLine)
	: m_logFlags	(0)
	, m_caseTree	(DE_NULL)
{
	if (!parse(cmdLine))
		throw Exception("Failed to parse command line");
}

CommandLine::~CommandLine (void)
{
	delete m_caseTree;
}

void CommandLine::clear (void)
{
	m_cmdLine.clear();
	m_logFlags = 0;

	delete m_caseTree;
	m_caseTree = DE_NULL;
}

/*--------------------------------------------------------------------*//*!
 * \brief Parse command line from standard argc, argv pair.
 * \note parse() must be called exactly once.
 * \param argc Number of arguments
 * \param argv Command line arguments
 *//*--------------------------------------------------------------------*/
bool CommandLine::parse (int argc, const char* const* argv)
{
	DebugOutStreambuf	sbuf;
	std::ostream		debugOut	(&sbuf);
	de::cmdline::Parser	parser;

	opt::registerOptions(parser);
	opt::registerLegacyOptions(parser);

	clear();

	if (!parser.parse(argc-1, argv+1, &m_cmdLine, std::cerr))
	{
		debugOut << "\n" << de::FilePath(argv[0]).getBaseName() << " [options]\n\n";
		parser.help(debugOut);

		clear();
		return false;
	}

	if (!m_cmdLine.getOption<opt::LogImages>())
		m_logFlags |= QP_TEST_LOG_EXCLUDE_IMAGES;

	if ((m_cmdLine.getOption<opt::CasePath>().empty()?0:1) +
		(m_cmdLine.getOption<opt::CaseList>().empty()?0:1) +
		(m_cmdLine.getOption<opt::CaseListFile>().empty()?0:1) +
		(m_cmdLine.getOption<opt::StdinCaseList>()?1:0) > 1)
	{
		debugOut << "ERROR: multiple test case list options given!\n" << std::endl;
		clear();
		return false;
	}

	try
	{
		if (!m_cmdLine.getOption<opt::CaseList>().empty())
		{
			std::istringstream str(m_cmdLine.getOption<opt::CaseList>());

			m_caseTree = parseCaseTree(str);
		}
		else if (!m_cmdLine.getOption<opt::CaseListFile>().empty())
		{
			std::ifstream in(m_cmdLine.getOption<opt::CaseListFile>().c_str(), std::ios_base::binary);

			if (!in.is_open() || !in.good())
				throw Exception("Failed to open case list file '" + m_cmdLine.getOption<opt::CaseListFile>() + "'");

			m_caseTree = parseCaseTree(in);
		}
		else if (m_cmdLine.getOption<opt::StdinCaseList>())
		{
			m_caseTree = parseCaseTree(std::cin);
		}
		else if (!m_cmdLine.getOption<opt::CasePath>().empty())
			m_casePaths = de::MovePtr<const CasePaths>(new CasePaths(m_cmdLine.getOption<opt::CasePath>()));
	}
	catch (const std::exception& e)
	{
		debugOut << "ERROR: Failed to parse test case list: " << e.what() << "\n";
		clear();
		return false;
	}

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Parse command line from string.
 * \note parse() must be called exactly once.
 * \param cmdLine Full command line string.
 *//*--------------------------------------------------------------------*/
bool CommandLine::parse (const std::string& cmdLine)
{
	deCommandLine* parsedCmdLine = deCommandLine_parse(cmdLine.c_str());
	if (!parsedCmdLine)
		throw std::bad_alloc();

	bool isOk = false;
	try
	{
		isOk = parse(parsedCmdLine->numArgs, parsedCmdLine->args);
	}
	catch (...)
	{
		deCommandLine_destroy(parsedCmdLine);
		throw;
	}

	deCommandLine_destroy(parsedCmdLine);
	return isOk;
}

const char*				CommandLine::getLogFileName				(void) const	{ return m_cmdLine.getOption<opt::LogFilename>().c_str();		}
deUint32				CommandLine::getLogFlags				(void) const	{ return m_logFlags;											}
RunMode					CommandLine::getRunMode					(void) const	{ return m_cmdLine.getOption<opt::RunMode>();					}
WindowVisibility		CommandLine::getVisibility				(void) const	{ return m_cmdLine.getOption<opt::Visibility>();				}
bool					CommandLine::isWatchDogEnabled			(void) const	{ return m_cmdLine.getOption<opt::WatchDog>();					}
bool					CommandLine::isCrashHandlingEnabled		(void) const	{ return m_cmdLine.getOption<opt::CrashHandler>();				}
int						CommandLine::getBaseSeed				(void) const	{ return m_cmdLine.getOption<opt::BaseSeed>();					}
int						CommandLine::getTestIterationCount		(void) const	{ return m_cmdLine.getOption<opt::TestIterationCount>();		}
int						CommandLine::getSurfaceWidth			(void) const	{ return m_cmdLine.getOption<opt::SurfaceWidth>();				}
int						CommandLine::getSurfaceHeight			(void) const	{ return m_cmdLine.getOption<opt::SurfaceHeight>();				}
SurfaceType				CommandLine::getSurfaceType				(void) const	{ return m_cmdLine.getOption<opt::SurfaceType>();				}
ScreenRotation			CommandLine::getScreenRotation			(void) const	{ return m_cmdLine.getOption<opt::ScreenRotation>();			}
int						CommandLine::getGLConfigId				(void) const	{ return m_cmdLine.getOption<opt::GLConfigID>();				}
int						CommandLine::getCLPlatformId			(void) const	{ return m_cmdLine.getOption<opt::CLPlatformID>();				}
const std::vector<int>&	CommandLine::getCLDeviceIds				(void) const	{ return m_cmdLine.getOption<opt::CLDeviceIDs>();				}
const char*				CommandLine::getEGLDisplayType			(void) const	{ return m_cmdLine.getOption<opt::EGLDisplayType>().c_str();	}
const char*				CommandLine::getEGLWindowType			(void) const	{ return m_cmdLine.getOption<opt::EGLWindowType>().c_str();		}
const char*				CommandLine::getEGLPixmapType			(void) const	{ return m_cmdLine.getOption<opt::EGLPixmapType>().c_str();		}
bool					CommandLine::isOutOfMemoryTestEnabled	(void) const	{ return m_cmdLine.getOption<opt::TestOOM>();					}

const char* CommandLine::getGLContextType (void) const
{
	if (!m_cmdLine.getOption<opt::GLContextType>().empty())
		return m_cmdLine.getOption<opt::GLContextType>().c_str();
	else
		return DE_NULL;
}
const char* CommandLine::getGLConfigName (void) const
{
	if (!m_cmdLine.getOption<opt::GLConfigName>().empty())
		return m_cmdLine.getOption<opt::GLConfigName>().c_str();
	else
		return DE_NULL;
}

const char* CommandLine::getGLContextFlags (void) const
{
	if (!m_cmdLine.getOption<opt::GLContextFlags>().empty())
		return m_cmdLine.getOption<opt::GLContextFlags>().c_str();
	else
		return DE_NULL;
}

const char* CommandLine::getCLBuildOptions (void) const
{
	if (!m_cmdLine.getOption<opt::CLBuildOptions>().empty())
		return m_cmdLine.getOption<opt::CLBuildOptions>().c_str();
	else
		return DE_NULL;
}

static bool checkTestGroupName (const CaseTreeNode* node, const char* groupName)
{
	for (vector<CaseTreeNode*>::const_iterator childIter = node->getChildren().begin(); childIter != node->getChildren().end(); ++childIter)
	{
		const CaseTreeNode* const child = *childIter;

		if (deStringBeginsWith(groupName, child->getName().c_str()))
		{
			const int prefixLen = (int)child->getName().length();

			if (groupName[prefixLen] == 0)
				return true;
			else if (groupName[prefixLen] == '.')
				return checkTestGroupName(child, groupName + prefixLen + 1);
		}
	}

	return false;
}

static bool checkTestCaseName (const CaseTreeNode* node, const char* caseName)
{
	for (vector<CaseTreeNode*>::const_iterator childIter = node->getChildren().begin(); childIter != node->getChildren().end(); ++childIter)
	{
		const CaseTreeNode* const child = *childIter;

		if (deStringBeginsWith(caseName, child->getName().c_str()))
		{
			const int prefixLen = (int)child->getName().length();

			if (caseName[prefixLen] == 0 && child->getChildren().empty())
				return true;
			else if (caseName[prefixLen] == '.')
				return checkTestCaseName(child, caseName + prefixLen + 1);
		}
	}

	return false;
}

bool CommandLine::checkTestGroupName (const char* groupName) const
{
	if (m_casePaths)
		return m_casePaths->matches(groupName, true);
	else if (m_caseTree)
		return groupName[0] == 0 || tcu::checkTestGroupName(m_caseTree, groupName);
	else
		return true;
}

bool CommandLine::checkTestCaseName (const char* caseName) const
{
	if (m_casePaths)
		return m_casePaths->matches(caseName, false);
	else if (m_caseTree)
		return tcu::checkTestCaseName(m_caseTree, caseName);
	else
		return true;
}

} // tcu
