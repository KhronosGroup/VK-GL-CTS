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
#include "deSharedPtr.hpp"

#include <iostream>

using std::vector;
using std::string;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{

tcu::TestPackageRoot* createRoot (tcu::TestContext& testCtx)
{
	vector<tcu::TestNode*>	children;
	children.push_back(new TestPackage(testCtx));
	return new tcu::TestPackageRoot(testCtx, children);
}

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

void writeBuildLogs (const glu::ShaderProgramInfo& buildInfo, std::ostream& dst)
{
	for (size_t shaderNdx = 0; shaderNdx < buildInfo.shaders.size(); shaderNdx++)
	{
		const glu::ShaderInfo&	shaderInfo	= buildInfo.shaders[shaderNdx];
		const char* const		shaderName	= getShaderTypeName(shaderInfo.type);

		dst << shaderName << " source:\n"
			<< "---\n"
			<< shaderInfo.source << "\n"
			<< "---\n"
			<< shaderName << " compile log:\n"
			<< "---\n"
			<< shaderInfo.infoLog << "\n"
			<< "---\n";
	}

	dst << "link log:\n"
		<< "---\n"
		<< buildInfo.program.infoLog << "\n"
		<< "---\n";
}

void writeBuildLogs (const vk::SpirVProgramInfo& buildInfo, std::ostream& dst)
{
	dst << "source:\n"
		<< "---\n"
		<< buildInfo.source << "\n"
		<< "---\n";
}

vk::ProgramBinary* compileProgram (const glu::ProgramSources& source, std::ostream& buildLog)
{
	glu::ShaderProgramInfo	buildInfo;

	try
	{
		return vk::buildProgram(source, vk::PROGRAM_FORMAT_SPIRV, &buildInfo);
	}
	catch (const tcu::Exception&)
	{
		writeBuildLogs(buildInfo, buildLog);
		throw;
	}
}

vk::ProgramBinary* compileProgram (const vk::SpirVAsmSource& source, std::ostream& buildLog)
{
	vk::SpirVProgramInfo	buildInfo;

	try
	{
		return vk::assembleProgram(source, &buildInfo);
	}
	catch (const tcu::Exception&)
	{
		writeBuildLogs(buildInfo, buildLog);
		throw;
	}
}

struct BuiltProgram
{
	vk::ProgramIdentifier			id;
	bool							buildOk;
	UniquePtr<vk::ProgramBinary>	binary;		// Null if build failed
	std::string						buildLog;

	BuiltProgram (const vk::ProgramIdentifier&	id_,
				  bool							buildOk_,
				  MovePtr<vk::ProgramBinary>	binary_,
				  const std::string&			buildLog_)
		: id		(id_)
		, buildOk	(buildOk_)
		, binary	(binary_)
		, buildLog	(buildLog_)
	{
	}
};

typedef SharedPtr<BuiltProgram> BuiltProgramSp;

template<typename IteratorType>
BuiltProgramSp buildProgram (IteratorType progIter, const std::string& casePath)
{
	std::ostringstream			buildLog;
	MovePtr<vk::ProgramBinary>	programBinary;
	bool						buildOk			= false;

	try
	{
		programBinary	= MovePtr<vk::ProgramBinary>(compileProgram(progIter.getProgram(), buildLog));
		buildOk			= true;
	}
	catch (const std::exception&)
	{
		// Ignore, buildOk = false
		DE_ASSERT(!programBinary);
	}

	return BuiltProgramSp(new BuiltProgram(vk::ProgramIdentifier(casePath, progIter.getName()),
										   buildOk,
										   programBinary,
										   buildLog.str()));
}

} // anonymous

BuildStats buildPrograms (tcu::TestContext& testCtx, const std::string& dstPath, bool validateBinaries)
{
	const UniquePtr<tcu::TestPackageRoot>	root		(createRoot(testCtx));
	tcu::DefaultHierarchyInflater			inflater	(testCtx);
	tcu::TestHierarchyIterator				iterator	(*root, inflater, testCtx.getCommandLine());
	const tcu::DirArchive					srcArchive	(dstPath.c_str());
	UniquePtr<vk::BinaryRegistryWriter>		writer		(new vk::BinaryRegistryWriter(dstPath));
	BuildStats								stats;

	while (iterator.getState() != tcu::TestHierarchyIterator::STATE_FINISHED)
	{
		if (iterator.getState() == tcu::TestHierarchyIterator::STATE_ENTER_NODE &&
			tcu::isTestNodeTypeExecutable(iterator.getNode()->getNodeType()))
		{
			const TestCase* const		testCase	= dynamic_cast<TestCase*>(iterator.getNode());
			const string				casePath	= iterator.getNodePath();
			vk::SourceCollections		sourcePrograms;
			vector<BuiltProgramSp>		builtPrograms;

			tcu::print("%s\n", casePath.c_str());

			testCase->initPrograms(sourcePrograms);

			for (vk::GlslSourceCollection::Iterator progIter = sourcePrograms.glslSources.begin();
				 progIter != sourcePrograms.glslSources.end();
				 ++progIter)
			{
				builtPrograms.push_back(buildProgram(progIter, casePath));
			}

			for (vk::SpirVAsmCollection::Iterator progIter = sourcePrograms.spirvAsmSources.begin();
				 progIter != sourcePrograms.spirvAsmSources.end();
				 ++progIter)
			{
				builtPrograms.push_back(buildProgram(progIter, casePath));
			}

			// Process programs
			for (vector<BuiltProgramSp>::const_iterator progIter = builtPrograms.begin();
				 progIter != builtPrograms.end();
				 ++progIter)
			{
				const BuiltProgram&	program	= **progIter;

				if (program.buildOk)
				{
					std::ostringstream	validationLog;

					writer->storeProgram(program.id, *program.binary);

					if (validateBinaries &&
						!vk::validateProgram(*program.binary, &validationLog))
					{
						tcu::print("ERROR: validation failed for %s\n", program.id.programName.c_str());
						tcu::print("%s\n", validationLog.str().c_str());
						stats.numFailed += 1;
					}
					else
						stats.numSucceeded += 1;
				}
				else
				{
					tcu::print("ERROR: failed to build %s\n", program.id.programName.c_str());
					tcu::print("%s\n", program.buildLog.c_str());
					stats.numFailed += 1;
				}
			}
		}

		iterator.next();
	}

	writer->writeIndex();

	return stats;
}

} // vkt

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(DstPath,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(Cases,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(Validate,	bool);

} // opt

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;

	parser << Option<opt::DstPath>	("d", "dst-path",		"Destination path",	"out")
		   << Option<opt::Cases>	("n", "deqp-case",		"Case path filter (works as in test binaries)")
		   << Option<opt::Validate>	("v", "validate-spv",	"Validate generated SPIR-V binaries");
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
																	 cmdLine.getOption<opt::Validate>());

		tcu::print("DONE: %d passed, %d failed\n", stats.numSucceeded, stats.numFailed);

		return stats.numFailed == 0 ? 0 : -1;
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}
}
