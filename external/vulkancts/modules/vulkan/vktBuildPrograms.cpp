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

	vk::SpirvValidatorOptions	validatorOptions;

	explicit				Program		(const vk::ProgramIdentifier& id_, const vk::SpirvValidatorOptions& valOptions_)
								: id				(id_)
								, buildStatus		(STATUS_NOT_COMPLETED)
								, validationStatus	(STATUS_NOT_COMPLETED)
								, validatorOptions	(valOptions_)
							{}
							Program		(void)
								: id				("", "")
								, buildStatus		(STATUS_NOT_COMPLETED)
								, validationStatus	(STATUS_NOT_COMPLETED)
								, validatorOptions()
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
		: m_source		(source)
		, m_program		(program)
		, m_commandLine	(0)
	{}

	BuildHighLevelShaderTask (void) : m_program(DE_NULL) {}

	void setCommandline (const tcu::CommandLine &commandLine)
	{
		m_commandLine = &commandLine;
	}

	void execute (void)
	{
		glu::ShaderProgramInfo buildInfo;

		try
		{
			DE_ASSERT(m_source.buildOptions.targetVersion < vk::SPIRV_VERSION_LAST);
			DE_ASSERT(m_commandLine != DE_NULL);
			m_program->binary			= ProgramBinarySp(vk::buildProgram(m_source, &buildInfo, *m_commandLine));
			m_program->buildStatus		= Program::STATUS_PASSED;
			m_program->validatorOptions	= m_source.buildOptions.getSpirvValidatorOptions();
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
	Source					m_source;
	Program*				m_program;
	const tcu::CommandLine*	m_commandLine;
};

void writeBuildLogs (const vk::SpirVProgramInfo& buildInfo, std::ostream& dst)
{
	dst << "source:\n"
		<< "---\n"
		<< buildInfo.source << "\n"
		<< "---\n"
		<< buildInfo.infoLog << "\n"
		<< "---\n";
}

class BuildSpirVAsmTask : public Task
{
public:
	BuildSpirVAsmTask (const vk::SpirVAsmSource& source, Program* program)
		: m_source		(source)
		, m_program		(program)
		, m_commandLine	(0)
	{}

	BuildSpirVAsmTask (void) : m_program(DE_NULL), m_commandLine(0) {}

	void setCommandline (const tcu::CommandLine &commandLine)
	{
		m_commandLine = &commandLine;
	}

	void execute (void)
	{
		vk::SpirVProgramInfo buildInfo;

		try
		{
			DE_ASSERT(m_source.buildOptions.targetVersion < vk::SPIRV_VERSION_LAST);
			DE_ASSERT(m_commandLine != DE_NULL);
			m_program->binary		= ProgramBinarySp(vk::assembleProgram(m_source, &buildInfo, *m_commandLine));
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
	vk::SpirVAsmSource		m_source;
	Program*				m_program;
	const tcu::CommandLine*	m_commandLine;
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

		std::ostringstream			validationLogStream;

		if (vk::validateProgram(*m_program->binary, &validationLogStream, m_program->validatorOptions))
			m_program->validationStatus = Program::STATUS_PASSED;
		else
			m_program->validationStatus = Program::STATUS_FAILED;
		m_program->validationLog = validationLogStream.str();
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
						  const vk::SpirvVersion	baselineSpirvVersion,
						  const vk::SpirvVersion	maxSpirvVersion,
						  const bool				allowSpirV14)
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
					TestCase* const				testCase					= dynamic_cast<TestCase*>(iterator.getNode());
					const string				casePath					= iterator.getNodePath();
					vk::ShaderBuildOptions		defaultGlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
					vk::ShaderBuildOptions		defaultHlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
					vk::SpirVAsmBuildOptions	defaultSpirvAsmBuildOptions	(usedVulkanVersion, baselineSpirvVersion);
					vk::SourceCollections		sourcePrograms				(usedVulkanVersion, defaultGlslBuildOptions, defaultHlslBuildOptions, defaultSpirvAsmBuildOptions);

					try
					{
						testCase->delayedInit();
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
						// Unless this is SPIR-V 1.4 and is explicitly allowed.
						if (progIter.getProgram().buildOptions.targetVersion > maxSpirvVersion && !(allowSpirV14 && progIter.getProgram().buildOptions.supports_VK_KHR_spirv_1_4 && progIter.getProgram().buildOptions.targetVersion == vk::SPIRV_VERSION_1_4))
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.getSpirvValidatorOptions()));
						buildGlslTasks.pushBack(BuildHighLevelShaderTask<vk::GlslSource>(progIter.getProgram(), &programs.back()));
						buildGlslTasks.back().setCommandline(testCtx.getCommandLine());
						executor.submit(&buildGlslTasks.back());
					}

					for (vk::HlslSourceCollection::Iterator progIter = sourcePrograms.hlslSources.begin();
						 progIter != sourcePrograms.hlslSources.end();
						 ++progIter)
					{
						// Source program requires higher SPIR-V version than available: skip it to avoid fail
						// Unless this is SPIR-V 1.4 and is explicitly allowed.
						if (progIter.getProgram().buildOptions.targetVersion > maxSpirvVersion && !(allowSpirV14 && progIter.getProgram().buildOptions.supports_VK_KHR_spirv_1_4 && progIter.getProgram().buildOptions.targetVersion == vk::SPIRV_VERSION_1_4))
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.getSpirvValidatorOptions()));
						buildHlslTasks.pushBack(BuildHighLevelShaderTask<vk::HlslSource>(progIter.getProgram(), &programs.back()));
						buildHlslTasks.back().setCommandline(testCtx.getCommandLine());
						executor.submit(&buildHlslTasks.back());
					}

					for (vk::SpirVAsmCollection::Iterator progIter = sourcePrograms.spirvAsmSources.begin();
						 progIter != sourcePrograms.spirvAsmSources.end();
						 ++progIter)
					{
						// Source program requires higher SPIR-V version than available: skip it to avoid fail
						// Unless this is SPIR-V 1.4 and is explicitly allowed.
						if (progIter.getProgram().buildOptions.targetVersion > maxSpirvVersion && !(allowSpirV14 && progIter.getProgram().buildOptions.supports_VK_KHR_spirv_1_4 && progIter.getProgram().buildOptions.targetVersion == vk::SPIRV_VERSION_1_4))
							continue;

						programs.pushBack(Program(vk::ProgramIdentifier(casePath, progIter.getName()), progIter.getProgram().buildOptions.getSpirvValidatorOptions()));
						buildSpirvAsmTasks.pushBack(BuildSpirVAsmTask(progIter.getProgram(), &programs.back()));
						buildSpirvAsmTasks.back().setCommandline(testCtx.getCommandLine());
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

DE_DECLARE_COMMAND_LINE_OPT(DstPath,				std::string);
DE_DECLARE_COMMAND_LINE_OPT(Cases,					std::string);
DE_DECLARE_COMMAND_LINE_OPT(Validate,				bool);
DE_DECLARE_COMMAND_LINE_OPT(VulkanVersion,			deUint32);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCache,			bool);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCacheFilename,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCacheTruncate,	bool);
DE_DECLARE_COMMAND_LINE_OPT(SpirvOptimize,			bool);
DE_DECLARE_COMMAND_LINE_OPT(SpirvOptimizationRecipe,std::string);
DE_DECLARE_COMMAND_LINE_OPT(SpirvAllow14,			bool);

static const de::cmdline::NamedValue<bool> s_enableNames[] =
{
	{ "enable",		true },
	{ "disable",	false }
};

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	static const NamedValue<deUint32> s_vulkanVersion[] =
	{
		{ "1.0",	VK_MAKE_VERSION(1, 0, 0)	},
		{ "1.1",	VK_MAKE_VERSION(1, 1, 0)	},
		{ "1.2",	VK_MAKE_VERSION(1, 2, 0)	},
	};

	DE_STATIC_ASSERT(vk::SPIRV_VERSION_1_5 + 1 == vk::SPIRV_VERSION_LAST);

	parser << Option<opt::DstPath>("d", "dst-path", "Destination path", "out")
		<< Option<opt::Cases>("n", "deqp-case", "Case path filter (works as in test binaries)")
		<< Option<opt::Validate>("v", "validate-spv", "Validate generated SPIR-V binaries")
		<< Option<opt::VulkanVersion>("t", "target-vulkan-version", "Target Vulkan version", s_vulkanVersion, "1.2")
		<< Option<opt::ShaderCache>("s", "shadercache", "Enable or disable shader cache", s_enableNames, "enable")
		<< Option<opt::ShaderCacheFilename>("r", "shadercache-filename", "Write shader cache to given file", "shadercache.bin")
		<< Option<opt::ShaderCacheTruncate>("x", "shadercache-truncate", "Truncate shader cache before running", s_enableNames, "enable")
		<< Option<opt::SpirvOptimize>("o", "deqp-optimize-spirv", "Enable optimization for SPIR-V", s_enableNames, "disable")
		<< Option<opt::SpirvOptimizationRecipe>("p","deqp-optimization-recipe", "Shader optimization recipe")
		<< Option<opt::SpirvAllow14>("e","allow-spirv-14", "Allow SPIR-V 1.4 with Vulkan 1.1");
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

		if (cmdLine.hasOption<opt::ShaderCacheFilename>())
		{
			deqpArgv.push_back("--deqp-shadercache-filename");
			deqpArgv.push_back(cmdLine.getOption<opt::ShaderCacheFilename>().c_str());
		}

		if (cmdLine.hasOption<opt::ShaderCache>())
		{
			deqpArgv.push_back("--deqp-shadercache");
			if (cmdLine.getOption<opt::ShaderCache>())
				deqpArgv.push_back("enable");
			else
				deqpArgv.push_back("disable");
		}

		if (cmdLine.hasOption<opt::ShaderCacheTruncate>())
		{
			deqpArgv.push_back("--deqp-shadercache-truncate");
			if (cmdLine.getOption<opt::ShaderCacheTruncate>())
				deqpArgv.push_back("enable");
			else
				deqpArgv.push_back("disable");
		}

		if (cmdLine.hasOption<opt::SpirvOptimize>())
		{
			deqpArgv.push_back("--deqp-optimize-spirv");
			if (cmdLine.getOption<opt::SpirvOptimize>())
				deqpArgv.push_back("enable");
			 else
				deqpArgv.push_back("disable");
		}

		if (cmdLine.hasOption<opt::SpirvOptimizationRecipe>())
		{
			deqpArgv.push_back("--deqp-optimization-recipe");
			deqpArgv.push_back(cmdLine.getOption<opt::SpirvOptimizationRecipe>().c_str());
		}

		if (!deqpCmdLine.parse((int)deqpArgv.size(), &deqpArgv[0]))
			return -1;
	}

	try
	{
		tcu::DirArchive			archive					(".");
		tcu::TestLog			log						(deqpCmdLine.getLogFileName(), deqpCmdLine.getLogFlags());
		tcu::Platform			platform;
		tcu::TestContext		testCtx					(platform, archive, log, deqpCmdLine, DE_NULL);
		vk::SpirvVersion		baselineSpirvVersion	= vk::getBaselineSpirvVersion(cmdLine.getOption<opt::VulkanVersion>());
		vk::SpirvVersion		maxSpirvVersion			= vk::getMaxSpirvVersionForGlsl(cmdLine.getOption<opt::VulkanVersion>());

		testCtx.writeSessionInfo();

		tcu::print("SPIR-V versions: baseline: %s, max supported: %s\n",
					getSpirvVersionName(baselineSpirvVersion).c_str(),
					getSpirvVersionName(maxSpirvVersion).c_str());

		const vkt::BuildStats	stats		= vkt::buildPrograms(testCtx,
																 cmdLine.getOption<opt::DstPath>(),
																 cmdLine.getOption<opt::Validate>(),
																 cmdLine.getOption<opt::VulkanVersion>(),
																 baselineSpirvVersion,
																 maxSpirvVersion,
																 cmdLine.getOption<opt::SpirvAllow14>());

		tcu::print("DONE: %d passed, %d failed, %d not supported\n", stats.numSucceeded, stats.numFailed, stats.notSupported);

		return stats.numFailed == 0 ? 0 : -1;
	}
	catch (const std::exception& e)
	{
		tcu::die("%s", e.what());
	}
}
