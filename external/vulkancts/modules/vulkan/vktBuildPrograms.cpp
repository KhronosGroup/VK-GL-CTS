/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Utility for pre-compiling source programs to SPIR-V
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestHierarchyIterator.hpp"
#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vkBinaryRegistry.hpp"
#include "vktTestCase.hpp"
#include "vktTestPackage.hpp"
#include "deUniquePtr.hpp"
#include "deCommandLine.hpp"

#include <iostream>

namespace vkt
{

using std::vector;
using std::string;
using de::UniquePtr;

tcu::TestPackageRoot* createRoot (tcu::TestContext& testCtx)
{
	vector<tcu::TestNode*>	children;
	children.push_back(new TestPackage(testCtx));
	return new tcu::TestPackageRoot(testCtx, children);
}

enum BuildMode
{
	BUILDMODE_BUILD = 0,
	BUILDMODE_VERIFY,

	BUILDMODE_LAST
};

struct BuildStats
{
	int		numSucceeded;
	int		numFailed;

	BuildStats (void)
		: numSucceeded	(0)
		, numFailed		(0)
	{
	}
};

BuildStats buildPrograms (tcu::TestContext& testCtx, const std::string& dstPath, BuildMode mode, bool verbose)
{
	const UniquePtr<tcu::TestPackageRoot>	root		(createRoot(testCtx));
	tcu::DefaultHierarchyInflater			inflater	(testCtx);
	tcu::TestHierarchyIterator				iterator	(*root, inflater, testCtx.getCommandLine());
	const tcu::DirArchive					srcArchive	(dstPath.c_str());
	UniquePtr<vk::BinaryRegistryWriter>		writer		(mode == BUILDMODE_BUILD	? new vk::BinaryRegistryWriter(dstPath)			: DE_NULL);
	UniquePtr<vk::BinaryRegistryReader>		reader		(mode == BUILDMODE_VERIFY	? new vk::BinaryRegistryReader(srcArchive, "")	: DE_NULL);
	BuildStats								stats;
	const bool								printLogs	= verbose;

	while (iterator.getState() != tcu::TestHierarchyIterator::STATE_FINISHED)
	{
		if (iterator.getState() == tcu::TestHierarchyIterator::STATE_ENTER_NODE &&
			tcu::isTestNodeTypeExecutable(iterator.getNode()->getNodeType()))
		{
			const TestCase* const		testCase	= dynamic_cast<TestCase*>(iterator.getNode());
			const string				casePath	= iterator.getNodePath();
			vk::SourceCollection		progs;

			tcu::print("%s\n", casePath.c_str());

			testCase->initPrograms(progs);

			for (vk::SourceCollection::Iterator progIter = progs.begin(); progIter != progs.end(); ++progIter)
			{
				glu::ShaderProgramInfo	buildInfo;

				try
				{
					const vk::ProgramIdentifier			progId		(casePath, progIter.getName());
					const UniquePtr<vk::ProgramBinary>	binary		(vk::buildProgram(progIter.getProgram(), vk::PROGRAM_FORMAT_SPIRV, &buildInfo));

					if (mode == BUILDMODE_BUILD)
						writer->storeProgram(progId, *binary);
					else
					{
						DE_ASSERT(mode == BUILDMODE_VERIFY);

						const UniquePtr<vk::ProgramBinary>	storedBinary	(reader->loadProgram(progId));

						if (binary->getSize() != storedBinary->getSize())
							throw tcu::Exception("Binary size doesn't match");

						if (deMemCmp(binary->getBinary(), storedBinary->getBinary(), binary->getSize()))
							throw tcu::Exception("Binary contents don't match");
					}

					tcu::print("  OK: %s\n", progIter.getName().c_str());
					stats.numSucceeded += 1;
				}
				catch (const std::exception& e)
				{
					tcu::print("  ERROR: %s: %s\n", progIter.getName().c_str(), e.what());

					if (printLogs)
					{
						for (size_t shaderNdx = 0; shaderNdx < buildInfo.shaders.size(); shaderNdx++)
						{
							const glu::ShaderInfo&	shaderInfo	= buildInfo.shaders[shaderNdx];
							const char* const		shaderName	= getShaderTypeName(shaderInfo.type);

							tcu::print("%s source:\n---\n%s\n---\n", shaderName, shaderInfo.source.c_str());
							tcu::print("%s compile log:\n---\n%s\n---\n", shaderName, shaderInfo.infoLog.c_str());
						}
					}

					stats.numFailed += 1;
				}
			}
		}

		iterator.next();
	}

	return stats;
}

} // vkt

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(DstPath,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(Mode,		vkt::BuildMode);
DE_DECLARE_COMMAND_LINE_OPT(Verbose,	bool);

} // opt

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	static const NamedValue<vkt::BuildMode> s_modes[] =
	{
		{ "build",	vkt::BUILDMODE_BUILD	},
		{ "verify",	vkt::BUILDMODE_VERIFY	}
	};

	parser << Option<opt::DstPath>	("d", "dst-path",	"Destination path",	"out")
		   << Option<opt::Mode>		("m", "mode",		"Build mode",		s_modes,	"build")
		   << Option<opt::Verbose>	("v", "verbose",	"Verbose output");
}

int main (int argc, const char* argv[])
{
	de::cmdline::CommandLine	cmdLine;

	{
		de::cmdline::Parser		parser;
		registerOptions(parser);
		if (!parser.parse(argc, argv, &cmdLine, std::cerr))
		{
			parser.help(std::cout);
			return -1;
		}
	}

	try
	{
		const tcu::CommandLine	deqpCmdLine		("unused");
		tcu::DirArchive			archive			(".");
		tcu::TestLog			log				(deqpCmdLine.getLogFileName(), deqpCmdLine.getLogFlags());
		tcu::Platform			platform;
		tcu::TestContext		testCtx			(platform, archive, log, deqpCmdLine, DE_NULL);

		const vkt::BuildStats	stats			= vkt::buildPrograms(testCtx,
																	 cmdLine.getOption<opt::DstPath>(),
																	 cmdLine.getOption<opt::Mode>(),
																	 cmdLine.getOption<opt::Verbose>());

		tcu::print("DONE: %d passed, %d failed\n", stats.numSucceeded, stats.numFailed);

		return stats.numFailed == 0 ? 0 : -1;
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}
}
