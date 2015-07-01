/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
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

BuildStats buildPrograms (tcu::TestContext& testCtx, const std::string& dstPath, BuildMode mode)
{
	const UniquePtr<tcu::TestPackageRoot>	root		(createRoot(testCtx));
	tcu::DefaultHierarchyInflater			inflater	(testCtx);
	tcu::TestHierarchyIterator				iterator	(*root, inflater, testCtx.getCommandLine());
	const tcu::DirArchive					srcArchive	(dstPath.c_str());
	UniquePtr<vk::BinaryRegistryWriter>		writer		(mode == BUILDMODE_BUILD	? new vk::BinaryRegistryWriter(dstPath)			: DE_NULL);
	UniquePtr<vk::BinaryRegistryReader>		reader		(mode == BUILDMODE_VERIFY	? new vk::BinaryRegistryReader(srcArchive, "")	: DE_NULL);
	BuildStats								stats;

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
				try
				{
					const vk::ProgramIdentifier			progId		(casePath, progIter.getName());
					const UniquePtr<vk::ProgramBinary>	binary		(vk::buildProgram(progIter.getProgram(), vk::PROGRAM_FORMAT_SPIRV));

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

	parser << Option<opt::DstPath>	("d", "dst-path",	"Destination path",	".")
		   << Option<opt::Mode>		("m", "mode",		"Build mode",		s_modes,	"build");
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

		const vkt::BuildStats	stats			= vkt::buildPrograms(testCtx, cmdLine.getOption<opt::DstPath>(), cmdLine.getOption<opt::Mode>());

		tcu::print("DONE: %d passed, %d failed\n", stats.numSucceeded, stats.numFailed);

		return stats.numFailed == 0 ? 0 : -1;
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}
}
