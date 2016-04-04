/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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

using std::vector;
using std::string;
using de::UniquePtr;
using de::MovePtr;

namespace vkt
{

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

namespace // anonymous
{

vk::ProgramBinary* compileProgram (const glu::ProgramSources& source, glu::ShaderProgramInfo* buildInfo)
{
	return vk::buildProgram(source, vk::PROGRAM_FORMAT_SPIRV, buildInfo);
}

vk::ProgramBinary* compileProgram (const vk::SpirVAsmSource& source, vk::SpirVProgramInfo* buildInfo)
{
	return vk::assembleProgram(source, buildInfo);
}

void writeVerboseLogs (const glu::ShaderProgramInfo& buildInfo)
{
	for (size_t shaderNdx = 0; shaderNdx < buildInfo.shaders.size(); shaderNdx++)
	{
		const glu::ShaderInfo&	shaderInfo	= buildInfo.shaders[shaderNdx];
		const char* const		shaderName	= getShaderTypeName(shaderInfo.type);

		tcu::print("%s source:\n---\n%s\n---\n", shaderName, shaderInfo.source.c_str());
		tcu::print("%s compile log:\n---\n%s\n---\n", shaderName, shaderInfo.infoLog.c_str());
	}
}

void writeVerboseLogs (const vk::SpirVProgramInfo& buildInfo)
{
	tcu::print("source:\n---\n%s\n---\n", buildInfo.source->program.str().c_str());
	tcu::print("compile log:\n---\n%s\n---\n", buildInfo.infoLog.c_str());
}

template <typename InfoType, typename IteratorType>
void buildProgram (const std::string&			casePath,
				   bool							printLogs,
				   IteratorType					iter,
				   BuildMode					mode,
				   BuildStats*					stats,
				   vk::BinaryRegistryReader*	reader,
				   vk::BinaryRegistryWriter*	writer)
{
	InfoType							buildInfo;
	try
	{
		const vk::ProgramIdentifier			progId		(casePath, iter.getName());
		const UniquePtr<vk::ProgramBinary>	binary		(compileProgram(iter.getProgram(), &buildInfo));

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

		tcu::print("  OK: %s\n", iter.getName().c_str());
		stats->numSucceeded += 1;
	}
	catch (const std::exception& e)
	{
		tcu::print("  ERROR: %s: %s\n", iter.getName().c_str(), e.what());
		if (printLogs)
		{
			writeVerboseLogs(buildInfo);
		}
		stats->numFailed += 1;
	}
}

} // anonymous
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
			vk::SourceCollections		progs;

			tcu::print("%s\n", casePath.c_str());

			testCase->initPrograms(progs);

			for (vk::GlslSourceCollection::Iterator progIter = progs.glslSources.begin(); progIter != progs.glslSources.end(); ++progIter)
			{
				buildProgram<glu::ShaderProgramInfo, vk::GlslSourceCollection::Iterator>(casePath, printLogs, progIter, mode, &stats, reader.get(), writer.get());
			}

			for (vk::SpirVAsmCollection::Iterator progIter = progs.spirvAsmSources.begin(); progIter != progs.spirvAsmSources.end(); ++progIter)
			{
				buildProgram<vk::SpirVProgramInfo, vk::SpirVAsmCollection::Iterator>(casePath, printLogs, progIter, mode, &stats, reader.get(), writer.get());
			}
		}

		iterator.next();
	}

	if (mode == BUILDMODE_BUILD)
		writer->writeIndex();

	return stats;
}

} // vkt

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(DstPath,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(Mode,		vkt::BuildMode);
DE_DECLARE_COMMAND_LINE_OPT(Verbose,	bool);
DE_DECLARE_COMMAND_LINE_OPT(Cases,		std::string);

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
		   << Option<opt::Verbose>	("v", "verbose",	"Verbose output")
		   << Option<opt::Cases>	("n", "deqp-case",	"Case path filter (works as in test binaries)");
}

int main (int argc, const char* argv[])
{
	de::cmdline::CommandLine	cmdLine;
	tcu::CommandLine			deqpCmdLine;

	{
		de::cmdline::Parser		parser;
		registerOptions(parser);
		if (!parser.parse(argc, argv, &cmdLine, std::cerr))
		{
			parser.help(std::cout);
			return -1;
		}
	}

	{
		vector<const char*> deqpArgv;

		deqpArgv.push_back("unused");

		if (cmdLine.hasOption<opt::Cases>())
		{
			deqpArgv.push_back("--deqp-case");
			deqpArgv.push_back(cmdLine.getOption<opt::Cases>().c_str());
		}

		if (!deqpCmdLine.parse((int)deqpArgv.size(), &deqpArgv[0]))
			return -1;
	}

	try
	{
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
