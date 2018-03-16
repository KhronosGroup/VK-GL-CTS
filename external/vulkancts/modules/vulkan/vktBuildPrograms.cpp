/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
#include "deThread.hpp"
#include "deThreadSafeRingBuffer.hpp"
#include "dePoolArray.hpp"

#include <iostream>

using std::vector;
using std::string;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{

namespace // anonymous
{

typedef de::SharedPtr<glu::ProgramSources>	ProgramSourcesSp;
typedef de::SharedPtr<vk::SpirVAsmSource>	SpirVAsmSourceSp;
typedef de::SharedPtr<vk::ProgramBinary>	ProgramBinarySp;

class Task
{
public:
	virtual void	execute		(void) = 0;
};

typedef de::ThreadSafeRingBuffer<Task*>	TaskQueue;

class TaskExecutorThread : public de::Thread
{
public:
	TaskExecutorThread (TaskQueue& tasks)
		: m_tasks(tasks)
	{
		start();
	}

	void run (void)
	{
		for (;;)
		{
			Task* const	task	= m_tasks.popBack();

			if (task)
				task->execute();
			else
				break; // End of tasks - time to terminate
		}
	}

private:
	TaskQueue&	m_tasks;
};

class TaskExecutor
{
public:
								TaskExecutor		(deUint32 numThreads);
								~TaskExecutor		(void);

	void						submit				(Task* task);
	void						waitForComplete		(void);

private:
	typedef de::SharedPtr<TaskExecutorThread>	ExecThreadSp;

	std::vector<ExecThreadSp>	m_threads;
	TaskQueue					m_tasks;
};

TaskExecutor::TaskExecutor (deUint32 numThreads)
	: m_threads	(numThreads)
	, m_tasks	(m_threads.size() * 1024u)
{
	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		m_threads[ndx] = ExecThreadSp(new TaskExecutorThread(m_tasks));
}

TaskExecutor::~TaskExecutor (void)
{
	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		m_tasks.pushFront(DE_NULL);

	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		m_threads[ndx]->join();
}

void TaskExecutor::submit (Task* task)
{
	DE_ASSERT(task);
	m_tasks.pushFront(task);
}

class SyncTask : public Task
{
public:
	SyncTask (de::Semaphore* enterBarrier, de::Semaphore* inBarrier, de::Semaphore* leaveBarrier)
		: m_enterBarrier	(enterBarrier)
		, m_inBarrier		(inBarrier)
		, m_leaveBarrier	(leaveBarrier)
	{}

	SyncTask (void)
		: m_enterBarrier	(DE_NULL)
		, m_inBarrier		(DE_NULL)
		, m_leaveBarrier	(DE_NULL)
	{}

	void execute (void)
	{
		m_enterBarrier->increment();
		m_inBarrier->decrement();
		m_leaveBarrier->increment();
	}

private:
	de::Semaphore*	m_enterBarrier;
	de::Semaphore*	m_inBarrier;
	de::Semaphore*	m_leaveBarrier;
};

void TaskExecutor::waitForComplete (void)
{
	de::Semaphore			enterBarrier	(0);
	de::Semaphore			inBarrier		(0);
	de::Semaphore			leaveBarrier	(0);
	std::vector<SyncTask>	syncTasks		(m_threads.size());

	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
	{
		syncTasks[ndx] = SyncTask(&enterBarrier, &inBarrier, &leaveBarrier);
		submit(&syncTasks[ndx]);
	}

	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		enterBarrier.decrement();

	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		inBarrier.increment();

	for (size_t ndx = 0; ndx < m_threads.size(); ++ndx)
		leaveBarrier.decrement();
}

struct Program
{
	enum Status
	{
		STATUS_NOT_COMPLETED = 0,
		STATUS_FAILED,
		STATUS_PASSED,

		STATUS_LAST
	};

	vk::ProgramIdentifier	id;

	Status					buildStatus;
	std::string				buildLog;
	ProgramBinarySp			binary;

	Status					validationStatus;
	std::string				validationLog;

	vk::SpirvVersion		spirvVersion;

	explicit				Program		(const vk::ProgramIdentifier& id_, const vk::SpirvVersion spirvVersion_)
								: id				(id_)
								, buildStatus		(STATUS_NOT_COMPLETED)
								, validationStatus	(STATUS_NOT_COMPLETED)
								, spirvVersion		(spirvVersion_)
							{}
							Program		(void)
								: id				("", "")
								, buildStatus		(STATUS_NOT_COMPLETED)
								, validationStatus	(STATUS_NOT_COMPLETED)
								, spirvVersion		(vk::SPIRV_VERSION_LAST)
							{}
};

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

template <typename Source>
class BuildHighLevelShaderTask : public Task
{
public:

	BuildHighLevelShaderTask (const Source& source, Program* program)
		: m_source	(source)
		, m_program	(program)
	{}

	BuildHighLevelShaderTask (void) : m_program(DE_NULL) {}

	void execute (void)
	{
		glu::ShaderProgramInfo buildInfo;

		try
		{
			DE_ASSERT(m_source.buildOptions.targetVersion < vk::SPIRV_VERSION_LAST);

			m_program->binary		= ProgramBinarySp(vk::buildProgram(m_source, &buildInfo));
			m_program->buildStatus	= Program::STATUS_PASSED;
		}
		catch (const tcu::Exception&)
		{
			std::ostringstream log;

			writeBuildLogs(buildInfo, log);

			m_program->buildStatus	= Program::STATUS_FAILED;
			m_program->buildLog		= log.str();
		}
	}

private:
	Source		m_source;
	Program*	m_program;
};

void writeBuildLogs (const vk::SpirVProgramInfo& buildInfo, std::ostream& dst)
{
	dst << "source:\n"
		<< "---\n"
		<< buildInfo.source << "\n"
		<< "---\n";
}

class BuildSpirVAsmTask : public Task
{
public:
	BuildSpirVAsmTask (const vk::SpirVAsmSource& source, Program* program)
		: m_source	(source)
		, m_program	(program)
	{}

	BuildSpirVAsmTask (void) : m_program(DE_NULL) {}

	void execute (void)
	{
		vk::SpirVProgramInfo buildInfo;

		try
		{
			DE_ASSERT(m_source.buildOptions.targetVersion < vk::SPIRV_VERSION_LAST);

			m_program->binary		= ProgramBinarySp(vk::assembleProgram(m_source, &buildInfo));
			m_program->buildStatus	= Program::STATUS_PASSED;
		}
		catch (const tcu::Exception&)
		{
			std::ostringstream log;

			writeBuildLogs(buildInfo, log);

			m_program->buildStatus	= Program::STATUS_FAILED;
			m_program->buildLog		= log.str();
		}
	}

private:
	vk::SpirVAsmSource	m_source;
	Program*			m_program;
};

class ValidateBinaryTask : public Task
{
public:
	ValidateBinaryTask (Program* program)
		: m_program(program)
	{}

	void execute (void)
	{
		DE_ASSERT(m_program->buildStatus == Program::STATUS_PASSED);
		DE_ASSERT(m_program->binary->getFormat() == vk::PROGRAM_FORMAT_SPIRV);

		std::ostringstream			validationLog;
		const vk::ProgramBinary&	programBinary	= *(m_program->binary);
		const vk::SpirvVersion		spirvVersion	= vk::extractSpirvVersion(programBinary);

		if (vk::validateProgram(*m_program->binary, &validationLog, spirvVersion))
			m_program->validationStatus = Program::STATUS_PASSED;
		else
			m_program->validationStatus = Program::STATUS_FAILED;
	}

private:
	Program*	m_program;
};

tcu::TestPackageRoot* createRoot (tcu::TestContext& testCtx)
{
	vector<tcu::TestNode*>	children;
	children.push_back(new TestPackage(testCtx));
	return new tcu::TestPackageRoot(testCtx, children);
}

} // anonymous

struct BuildStats
{
	int		numSucceeded;
	int		numFailed;
	int		notSupported;

	BuildStats (void)
		: numSucceeded	(0)
		, numFailed		(0)
		, notSupported	(0)
	{
	}
};

BuildStats buildPrograms (tcu::TestContext&			testCtx,
						  const std::string&		dstPath,
						  const bool				validateBinaries,
						  const deUint32			usedVulkanVersion,
						  const vk::SpirvVersion	spirvVersionForGlsl,
						  const vk::SpirvVersion	spirvVersionForAsm)
{
	const deUint32						numThreads			= deGetNumAvailableLogicalCores();

	TaskExecutor						executor			(numThreads);

	// de::PoolArray<> is faster to build than std::vector
	de::MemPool							programPool;
	de::PoolArray<Program>				programs			(&programPool);
	int									notSupported		= 0;

	{
		de::MemPool							tmpPool;
		de::PoolArray<BuildHighLevelShaderTask<vk::GlslSource> >	buildGlslTasks		(&tmpPool);
		de::PoolArray<BuildHighLevelShaderTask<vk::HlslSource> >	buildHlslTasks		(&tmpPool);
		de::PoolArray<BuildSpirVAsmTask>	buildSpirvAsmTasks	(&tmpPool);

		// Collect build tasks
		{
			const UniquePtr<tcu::TestPackageRoot>	root			(createRoot(testCtx));
			tcu::DefaultHierarchyInflater			inflater		(testCtx);
			de::MovePtr<tcu::CaseListFilter>		caseListFilter	(testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));
			tcu::TestHierarchyIterator				iterator		(*root, inflater, *caseListFilter);

			while (iterator.getState() != tcu::TestHierarchyIterator::STATE_FINISHED)
			{
				if (iterator.getState() == tcu::TestHierarchyIterator::STATE_ENTER_NODE &&
					tcu::isTestNodeTypeExecutable(iterator.getNode()->getNodeType()))
				{
					const TestCase* const		testCase					= dynamic_cast<TestCase*>(iterator.getNode());
					const string				casePath					= iterator.getNodePath();
					vk::ShaderBuildOptions		defaultGlslBuildOptions		(spirvVersionForGlsl, 0u);
					vk::ShaderBuildOptions		defaultHlslBuildOptions		(spirvVersionForGlsl, 0u);
					vk::SpirVAsmBuildOptions	defaultSpirvAsmBuildOptions	(spirvVersionForAsm);
					vk::SourceCollections		sourcePrograms				(usedVulkanVersion, defaultGlslBuildOptions, defaultHlslBuildOptions, defaultSpirvAsmBuildOptions);

					try
					{
						testCase->initPrograms(sourcePrograms);
					}
					catch (const tcu::NotSupportedError& )
					{
						notSupported++;
						iterator.next();
						continue;
					}

					for (vk::GlslSourceCollection::Iterator progIter = sourcePrograms.glslSources.begin();
						 progIter != sourcePrograms.glslSources.end();
						 ++progIter)
					{
						// Source program requires higher SPIR-V version than available: skip it to avoid fail
						if (progIter.getProgram().buildOptions.targetVersion > spirvVersionForGlsl)
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.targetVersion));
						buildGlslTasks.pushBack(BuildHighLevelShaderTask<vk::GlslSource>(progIter.getProgram(), &programs.back()));
						executor.submit(&buildGlslTasks.back());
					}

					for (vk::HlslSourceCollection::Iterator progIter = sourcePrograms.hlslSources.begin();
						 progIter != sourcePrograms.hlslSources.end();
						 ++progIter)
					{
						// Source program requires higher SPIR-V version than available: skip it to avoid fail
						if (progIter.getProgram().buildOptions.targetVersion > spirvVersionForGlsl)
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.targetVersion));
						buildHlslTasks.pushBack(BuildHighLevelShaderTask<vk::HlslSource>(progIter.getProgram(), &programs.back()));
						executor.submit(&buildHlslTasks.back());
					}

					for (vk::SpirVAsmCollection::Iterator progIter = sourcePrograms.spirvAsmSources.begin();
						 progIter != sourcePrograms.spirvAsmSources.end();
						 ++progIter)
					{
						// Source program requires higher SPIR-V version than available: skip it to avoid fail
						if (progIter.getProgram().buildOptions.targetVersion > spirvVersionForAsm)
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.targetVersion));
						buildSpirvAsmTasks.pushBack(BuildSpirVAsmTask(progIter.getProgram(), &programs.back()));
						executor.submit(&buildSpirvAsmTasks.back());
					}
				}

				iterator.next();
			}
		}

		// Need to wait until tasks completed before freeing task memory
		executor.waitForComplete();
	}

	if (validateBinaries)
	{
		std::vector<ValidateBinaryTask>	validationTasks;

		validationTasks.reserve(programs.size());

		for (de::PoolArray<Program>::iterator progIter = programs.begin(); progIter != programs.end(); ++progIter)
		{
			if (progIter->buildStatus == Program::STATUS_PASSED)
			{
				validationTasks.push_back(ValidateBinaryTask(&*progIter));
				executor.submit(&validationTasks.back());
			}
		}

		executor.waitForComplete();
	}

	{
		vk::BinaryRegistryWriter	registryWriter		(dstPath);

		for (de::PoolArray<Program>::iterator progIter = programs.begin(); progIter != programs.end(); ++progIter)
		{
			if (progIter->buildStatus == Program::STATUS_PASSED)
				registryWriter.addProgram(progIter->id, *progIter->binary);
		}

		registryWriter.write();
	}

	{
		BuildStats	stats;
		stats.notSupported = notSupported;
		for (de::PoolArray<Program>::iterator progIter = programs.begin(); progIter != programs.end(); ++progIter)
		{
			const bool	buildOk			= progIter->buildStatus == Program::STATUS_PASSED;
			const bool	validationOk	= progIter->validationStatus != Program::STATUS_FAILED;

			if (buildOk && validationOk)
				stats.numSucceeded += 1;
			else
			{
				stats.numFailed += 1;
				tcu::print("ERROR: %s / %s: %s failed\n",
						   progIter->id.testCasePath.c_str(),
						   progIter->id.programName.c_str(),
						   (buildOk ? "validation" : "build"));
				tcu::print("%s\n", (buildOk ? progIter->validationLog.c_str() : progIter->buildLog.c_str()));
			}
		}

		return stats;
	}
}

} // vkt

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(DstPath,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(Cases,			std::string);
DE_DECLARE_COMMAND_LINE_OPT(Validate,		bool);
DE_DECLARE_COMMAND_LINE_OPT(VulkanVersion,	deUint32);

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	static const NamedValue<deUint32> s_vulkanVersion[] =
	{
		{ "1.0",	VK_MAKE_VERSION(1, 0, 0)	},
		{ "1.1",	VK_MAKE_VERSION(1, 1, 0)	},
	};

	DE_STATIC_ASSERT(vk::SPIRV_VERSION_1_3 + 1 == vk::SPIRV_VERSION_LAST);

	parser << Option<opt::DstPath>				("d", "dst-path",				"Destination path",	"out")
		   << Option<opt::Cases>				("n", "deqp-case",				"Case path filter (works as in test binaries)")
		   << Option<opt::Validate>				("v", "validate-spv",			"Validate generated SPIR-V binaries")
		   << Option<opt::VulkanVersion>		("t", "target-vulkan-version",	"Target Vulkan version", s_vulkanVersion, "1.1");
}

} // opt

int main (int argc, const char* argv[])
{
	de::cmdline::CommandLine	cmdLine;
	tcu::CommandLine			deqpCmdLine;

	{
		de::cmdline::Parser		parser;
		opt::registerOptions(parser);
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
		tcu::DirArchive			archive				(".");
		tcu::TestLog			log					(deqpCmdLine.getLogFileName(), deqpCmdLine.getLogFlags());
		tcu::Platform			platform;
		tcu::TestContext		testCtx				(platform, archive, log, deqpCmdLine, DE_NULL);
		vk::SpirvVersion		spirvVersionForGlsl	= vk::getSpirvVersionForGlsl(cmdLine.getOption<opt::VulkanVersion>());
		vk::SpirvVersion		spirvVersionForAsm	= vk::getSpirvVersionForAsm(cmdLine.getOption<opt::VulkanVersion>());

		tcu::print("SPIR-V versions: for GLSL sources: %s, for SPIR-V asm sources: %s\n",
					getSpirvVersionName(spirvVersionForGlsl).c_str(),
					getSpirvVersionName(spirvVersionForAsm).c_str());

		const vkt::BuildStats	stats		= vkt::buildPrograms(testCtx,
																 cmdLine.getOption<opt::DstPath>(),
																 cmdLine.getOption<opt::Validate>(),
																 cmdLine.getOption<opt::VulkanVersion>(),
																 spirvVersionForGlsl,
																 spirvVersionForAsm);

		tcu::print("DONE: %d passed, %d failed, %d not supported\n", stats.numSucceeded, stats.numFailed, stats.notSupported);

		return stats.numFailed == 0 ? 0 : -1;
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}
}
