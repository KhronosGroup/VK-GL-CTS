/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Vulkan Test Package
 *//*--------------------------------------------------------------------*/

#include "vktTestPackage.hpp"

#include "qpDebugOut.h"
#include "qpInfo.h"

#include "tcuPlatform.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"

#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkBinaryRegistry.hpp"
#include "vkShaderToSpirV.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkRenderDocUtil.hpp"
#include "vkResourceInterface.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#ifdef CTS_USES_VULKANSC
	#include "deProcess.h"
	#include "vksClient.hpp"
	#include "vksIPC.hpp"
#endif // CTS_USES_VULKANSC

#include "vktTestGroupUtil.hpp"
#include "vktApiTests.hpp"
#include "vktPipelineTests.hpp"
#include "vktBindingModelTests.hpp"
#include "vktSpvAsmTests.hpp"
#include "vktShaderLibrary.hpp"
#include "vktRenderPassTests.hpp"
#include "vktMemoryTests.hpp"
#include "vktShaderRenderBuiltinVarTests.hpp"
#include "vktShaderRenderDerivateTests.hpp"
#include "vktShaderRenderDiscardTests.hpp"
#include "vktShaderRenderIndexingTests.hpp"
#include "vktShaderRenderInvarianceTests.hpp"
#include "vktShaderRenderLimitTests.hpp"
#include "vktShaderRenderLoopTests.hpp"
#include "vktShaderRenderMatrixTests.hpp"
#include "vktShaderRenderOperatorTests.hpp"
#include "vktShaderRenderReturnTests.hpp"
#include "vktShaderRenderStructTests.hpp"
#include "vktShaderRenderSwitchTests.hpp"
#include "vktShaderRenderTextureFunctionTests.hpp"
#include "vktShaderRenderTextureGatherTests.hpp"
#include "vktShaderBuiltinTests.hpp"
#include "vktOpaqueTypeIndexingTests.hpp"
#include "vktAtomicOperationTests.hpp"
#include "vktUniformBlockTests.hpp"
#include "vktDynamicStateTests.hpp"
#include "vktSSBOLayoutTests.hpp"
#include "vktQueryPoolTests.hpp"
#include "vktDrawTests.hpp"
#include "vktComputeTests.hpp"
#include "vktConditionalTests.hpp"
#include "vktImageTests.hpp"
#include "vktInfoTests.hpp"
#include "vktWsiTests.hpp"
#include "vktSynchronizationTests.hpp"
#include "vktSparseResourcesTests.hpp"
#include "vktTessellationTests.hpp"
#include "vktRasterizationTests.hpp"
#include "vktClippingTests.hpp"
#include "vktFragmentOperationsTests.hpp"
#include "vktTextureTests.hpp"
#include "vktGeometryTests.hpp"
#include "vktRobustnessTests.hpp"
#include "vktMultiViewTests.hpp"
#include "vktSubgroupsTests.hpp"
#include "vktYCbCrTests.hpp"
#include "vktProtectedMemTests.hpp"
#include "vktDeviceGroupTests.hpp"
#include "vktMemoryModelTests.hpp"
#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberGlslTests.hpp"
#include "vktAmberDepthTests.hpp"
#include "vktImagelessFramebufferTests.hpp"
#include "vktTransformFeedbackTests.hpp"
#include "vktDescriptorIndexingTests.hpp"
#include "vktImagelessFramebufferTests.hpp"
#include "vktFragmentShaderInterlockTests.hpp"
#include "vktShaderClockTests.hpp"
#include "vktModifiersTests.hpp"
#include "vktRayTracingTests.hpp"
#include "vktRayQueryTests.hpp"
#include "vktPostmortemTests.hpp"
#include "vktFragmentShadingRateTests.hpp"
#include "vktReconvergenceTests.hpp"
#include "vktMeshShaderTests.hpp"
#include "vktFragmentShadingBarycentricTests.hpp"
#include "vktVideoTests.hpp"
#ifdef CTS_USES_VULKANSC
#include "vktSafetyCriticalTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktVideoTests.hpp"
#include "vktShaderObjectTests.hpp"

#include <vector>
#include <sstream>
#include <fstream>
#include <thread>

namespace vkt
{

using std::vector;
using de::UniquePtr;
using de::SharedPtr;
using de::MovePtr;
using tcu::TestLog;

// TestCaseExecutor

#ifdef CTS_USES_VULKANSC
struct DetailedSubprocessTestCount
{
	std::string									testPattern;
	int											testCount;
};
#endif // CTS_USES_VULKANSC

class TestCaseExecutor : public tcu::TestCaseExecutor
{
public:
												TestCaseExecutor			(tcu::TestContext& testCtx);
												~TestCaseExecutor			(void);

	void										init						(tcu::TestCase* testCase, const std::string& path) override;
	void										deinit						(tcu::TestCase* testCase) override;

	tcu::TestNode::IterateResult				iterate						(tcu::TestCase* testCase) override;

	void										deinitTestPackage			(tcu::TestContext& testCtx) override;
	bool										usesLocalStatus				() override;
	void										updateGlobalStatus			(tcu::TestRunStatus& status) override;
	void										reportDurations				(tcu::TestContext& testCtx, const std::string& packageName, const deInt64& duration, const std::map<std::string, deUint64>& groupsDurationTime) override;
	int											getCurrentSubprocessCount	(const std::string& casePath, int defaultSubprocessCount);

private:
	void										logUnusedShaders		(tcu::TestCase* testCase);

	void										runTestsInSubprocess	(tcu::TestContext& testCtx);

	bool										spirvVersionSupported	(vk::SpirvVersion);

	vk::BinaryCollection						m_progCollection;
	vk::BinaryRegistryReader					m_prebuiltBinRegistry;

	const UniquePtr<vk::Library>				m_library;
	MovePtr<Context>							m_context;

	const UniquePtr<vk::RenderDocUtil>			m_renderDoc;
	SharedPtr<vk::ResourceInterface>			m_resourceInterface;
	vk::VkPhysicalDeviceProperties				m_deviceProperties;
	tcu::WaiverUtil								m_waiverMechanism;

	TestInstance*								m_instance;			//!< Current test case instance
	std::vector<std::string>					m_testsForSubprocess;
	tcu::TestRunStatus							m_status;

#ifdef CTS_USES_VULKANSC
	int											m_subprocessCount;

	std::unique_ptr<vksc_server::ipc::Parent>	m_parentIPC;
	std::vector<DetailedSubprocessTestCount>	m_detailedSubprocessTestCount;
#endif // CTS_USES_VULKANSC
};

#ifdef CTS_USES_VULKANSC
static deBool	supressedWrite			(int, const char*)								{ return false; }
static deBool	supressedWriteFtm		(int, const char*, va_list)						{ return false; }
static deBool	openWrite				(int type, const char* message)					{ DE_UNREF(type); DE_UNREF(message); return true; }
static deBool	openWriteFtm			(int type, const char* format, va_list args)	{ DE_UNREF(type); DE_UNREF(format); DE_UNREF(args); return true; }
static void		suppressStandardOutput	()												{ qpRedirectOut(supressedWrite, supressedWriteFtm); }
static void		restoreStandardOutput	()												{ qpRedirectOut(openWrite, openWriteFtm); }
#endif // CTS_USES_VULKANSC

static MovePtr<vk::Library> createLibrary (tcu::TestContext& testCtx)
{
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
	return MovePtr<vk::Library>(testCtx.getPlatform().getVulkanPlatform().createLibrary(vk::Platform::LIBRARY_TYPE_VULKAN, testCtx.getCommandLine().getVkLibraryPath()));
#else
	return MovePtr<vk::Library>(testCtx.getPlatform().getVulkanPlatform().createLibrary(testCtx.getCommandLine().getVkLibraryPath()));
#endif
}

static vk::VkPhysicalDeviceProperties getPhysicalDeviceProperties(vkt::Context& context)
{
	const vk::InstanceInterface&	vki				= context.getInstanceInterface();
	const vk::VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	vk::VkPhysicalDeviceProperties	properties;
	vki.getPhysicalDeviceProperties(physicalDevice, &properties);
	return properties;
}

std::string trim (const std::string& original)
{
	static const std::string whiteSigns = " \t";
	const auto beg = original.find_first_not_of(whiteSigns);
	if (beg == std::string::npos)
		return std::string();
	const auto end = original.find_last_not_of(whiteSigns);
	return original.substr(beg, end - beg + 1);
}

TestCaseExecutor::TestCaseExecutor (tcu::TestContext& testCtx)
	: m_prebuiltBinRegistry	(testCtx.getArchive(), "vulkan/prebuilt")
	, m_library				(createLibrary(testCtx))
	, m_renderDoc			(testCtx.getCommandLine().isRenderDocEnabled()
							 ? MovePtr<vk::RenderDocUtil>(new vk::RenderDocUtil())
							 : MovePtr<vk::RenderDocUtil>(DE_NULL))
#if defined CTS_USES_VULKANSC
	, m_resourceInterface	(new vk::ResourceInterfaceVKSC(testCtx))
#else
	, m_resourceInterface	(new vk::ResourceInterfaceStandard(testCtx))
#endif // CTS_USES_VULKANSC
	, m_instance			(DE_NULL)
#if defined CTS_USES_VULKANSC
	, m_subprocessCount		(0)
#endif // CTS_USES_VULKANSC
{
#ifdef CTS_USES_VULKANSC
	std::vector<int> caseFraction = testCtx.getCommandLine().getCaseFraction();
	std::string jsonFileName;
	int portOffset;
	if (caseFraction.empty())
	{
		jsonFileName	= "pipeline_data.txt";
		portOffset		= 0;
	}
	else
	{
		jsonFileName	= "pipeline_data_" + std::to_string(caseFraction[0]) + ".txt";
		portOffset		= caseFraction[0];
	}

	if (testCtx.getCommandLine().isSubProcess())
	{
		std::vector<deUint8> input = vksc_server::ipc::Child{portOffset}.GetFile(jsonFileName);
		m_resourceInterface->importData(input);
	}
	else
	{
		m_parentIPC.reset( new vksc_server::ipc::Parent{portOffset} );
	}

	// Load information about test tree branches that use subprocess test count other than default
	// Expected file format:
	if (!testCtx.getCommandLine().isSubProcess() && !std::string(testCtx.getCommandLine().getSubprocessConfigFile()).empty())
	{
		std::ifstream			iFile(testCtx.getCommandLine().getSubprocessConfigFile(), std::ios::in);
		if (!iFile)
			TCU_THROW(InternalError, (std::string("Missing config file defining number of tests: ") + testCtx.getCommandLine().getSubprocessConfigFile()).c_str());
		std::string line;
		while (std::getline(iFile, line))
		{
			if (line.empty())
				continue;
			std::size_t pos = line.find_first_of(',');
			if (pos == std::string::npos)
				continue;
			std::string testPattern, testNumber;
			std::copy(line.begin(), line.begin() + pos, std::back_inserter(testPattern));
			testPattern = trim(testPattern);
			std::copy(line.begin() + pos + 1, line.end(), std::back_inserter(testNumber));
			testNumber = trim(testNumber);
			if (testPattern.empty() || testNumber.empty())
				continue;
			std::istringstream is(testNumber);
			int testCount;
			if ((is >> testCount).fail())
				continue;
			m_detailedSubprocessTestCount.push_back(DetailedSubprocessTestCount{ testPattern, testCount });
		}
		// sort test patterns
		std::sort(m_detailedSubprocessTestCount.begin(), m_detailedSubprocessTestCount.end(), [](const DetailedSubprocessTestCount& lhs, const DetailedSubprocessTestCount& rhs)
			{
				return lhs.testCount < rhs.testCount;
			} );
	}

	// If we are provided with remote location
	if (!std::string(testCtx.getCommandLine().getServerAddress()).empty())
	{
		// Open connection with the server dedicated for standard output
		vksc_server::OpenRemoteStandardOutput(testCtx.getCommandLine().getServerAddress());
		restoreStandardOutput();
	}
#endif // CTS_USES_VULKANSC

	m_context			= MovePtr<Context>(new Context(testCtx, m_library->getPlatformInterface(), m_progCollection, m_resourceInterface));
	m_deviceProperties	= getPhysicalDeviceProperties(*m_context);

	tcu::SessionInfo sessionInfo(m_deviceProperties.vendorID,
								 m_deviceProperties.deviceID,
								 m_deviceProperties.deviceName,
								 testCtx.getCommandLine().getInitialCmdLine());
	m_waiverMechanism.setup(testCtx.getCommandLine().getWaiverFileName(),
							"dEQP-VK",
							m_deviceProperties.vendorID,
							m_deviceProperties.deviceID,
							sessionInfo);

#ifdef CTS_USES_VULKANSC
	if (!std::string(testCtx.getCommandLine().getServerAddress()).empty())
	{
		vksc_server::AppendRequest request;
		request.fileName = testCtx.getCommandLine().getLogFileName();

		std::ostringstream str;
		str << "#sessionInfo releaseName " << qpGetReleaseName() << std::endl;
		str << "#sessionInfo releaseId 0x" << std::hex << std::setw(8) << std::setfill('0') << qpGetReleaseId() << std::endl;
		str << "#sessionInfo targetName \"" << qpGetTargetName() << "\"" << std::endl;
		str << sessionInfo.get() << std::endl;
		str << "#beginSession" << std::endl;

		std::string output = str.str();
		request.data.assign(output.begin(), output.end());
		request.clear = true;
		vksc_server::StandardOutputServerSingleton()->SendRequest(request);
	}
	else
#endif // CTS_USES_VULKANSC
	{
		testCtx.getLog().writeSessionInfo(sessionInfo.get());
	}

#ifdef CTS_USES_VULKANSC
	m_resourceInterface->initApiVersion(m_context->getUsedApiVersion());

	// Real Vulkan SC tests are performed in subprocess.
	// Tests run in main process are only used to collect data required by Vulkan SC.
	// That's why we turn off any output in main process and copy output from subprocess when subprocess tests are performed
	if (!testCtx.getCommandLine().isSubProcess())
	{
		suppressStandardOutput();
		m_context->getTestContext().getLog().supressLogging(true);
	}
#endif // CTS_USES_VULKANSC
}

TestCaseExecutor::~TestCaseExecutor (void)
{
	delete m_instance;
}

void TestCaseExecutor::init (tcu::TestCase* testCase, const std::string& casePath)
{
	if (m_waiverMechanism.isOnWaiverList(casePath))
		throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

	TestCase*					vktCase						= dynamic_cast<TestCase*>(testCase);
	tcu::TestLog&				log							= m_context->getTestContext().getLog();
	const deUint32				usedVulkanVersion			= m_context->getUsedApiVersion();
	const vk::SpirvVersion		baselineSpirvVersion		= vk::getBaselineSpirvVersion(usedVulkanVersion);
	vk::ShaderBuildOptions		defaultGlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
	vk::ShaderBuildOptions		defaultHlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
	vk::SpirVAsmBuildOptions	defaultSpirvAsmBuildOptions	(usedVulkanVersion, baselineSpirvVersion);
	vk::SourceCollections		sourceProgs					(usedVulkanVersion, defaultGlslBuildOptions, defaultHlslBuildOptions, defaultSpirvAsmBuildOptions);
	const tcu::CommandLine&		commandLine					= m_context->getTestContext().getCommandLine();
	const bool					doShaderLog					= commandLine.isLogDecompiledSpirvEnabled() && log.isShaderLoggingEnabled();

	if (!vktCase)
		TCU_THROW(InternalError, "Test node not an instance of vkt::TestCase");

	{
#ifdef CTS_USES_VULKANSC
		int currentSubprocessCount = getCurrentSubprocessCount(casePath, m_context->getTestContext().getCommandLine().getSubprocessTestCount());
		if (m_subprocessCount && currentSubprocessCount != m_subprocessCount)
		{
			runTestsInSubprocess(m_context->getTestContext());

			// Clean up data after performing tests in subprocess and prepare system for another batch of tests
			m_testsForSubprocess.clear();
			const vk::DeviceInterface&				vkd = m_context->getDeviceInterface();
			const vk::DeviceDriverSC*				dds = dynamic_cast<const vk::DeviceDriverSC*>(&vkd);
			if (dds == DE_NULL)
				TCU_THROW(InternalError, "Undefined device driver for Vulkan SC");
			dds->reset();
			m_resourceInterface->resetObjects();

			suppressStandardOutput();
			m_context->getTestContext().getLog().supressLogging(true);
		}
		m_subprocessCount = currentSubprocessCount;
#endif // CTS_USES_VULKANSC
		m_testsForSubprocess.push_back(casePath);
	}

	m_resourceInterface->initTestCase(casePath);

	if (m_waiverMechanism.isOnWaiverList(casePath))
		throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

	vktCase->checkSupport(*m_context);

	vktCase->delayedInit();

	m_progCollection.clear();
	vktCase->initPrograms(sourceProgs);

	for (vk::GlslSourceCollection::Iterator progIter = sourceProgs.glslSources.begin(); progIter != sourceProgs.glslSources.end(); ++progIter)
	{
		if (!spirvVersionSupported(progIter.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		const vk::ProgramBinary* const binProg = m_resourceInterface->buildProgram<glu::ShaderProgramInfo, vk::GlslSourceCollection::Iterator>(casePath, progIter, m_prebuiltBinRegistry, &m_progCollection);

		if (doShaderLog)
		{
			try
			{
				std::ostringstream disasm;

				vk::disassembleProgram(*binProg, &disasm);

				log << vk::SpirVAsmSource(disasm.str());
			}
			catch (const tcu::NotSupportedError& err)
			{
				log << err;
			}
		}
	}

	for (vk::HlslSourceCollection::Iterator progIter = sourceProgs.hlslSources.begin(); progIter != sourceProgs.hlslSources.end(); ++progIter)
	{
		if (!spirvVersionSupported(progIter.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		const vk::ProgramBinary* const binProg = m_resourceInterface->buildProgram<glu::ShaderProgramInfo, vk::HlslSourceCollection::Iterator>(casePath, progIter, m_prebuiltBinRegistry, &m_progCollection);

		if (doShaderLog)
		{
			try
			{
				std::ostringstream disasm;

				vk::disassembleProgram(*binProg, &disasm);

				log << vk::SpirVAsmSource(disasm.str());
			}
			catch (const tcu::NotSupportedError& err)
			{
				log << err;
			}
		}
	}

	for (vk::SpirVAsmCollection::Iterator asmIterator = sourceProgs.spirvAsmSources.begin(); asmIterator != sourceProgs.spirvAsmSources.end(); ++asmIterator)
	{
		if (!spirvVersionSupported(asmIterator.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		m_resourceInterface->buildProgram<vk::SpirVProgramInfo, vk::SpirVAsmCollection::Iterator>(casePath, asmIterator, m_prebuiltBinRegistry, &m_progCollection);
	}

	if (m_renderDoc) m_renderDoc->startFrame(m_context->getInstance());

	DE_ASSERT(!m_instance);
	m_instance = vktCase->createInstance(*m_context);
	m_context->resultSetOnValidation(false);
}

void TestCaseExecutor::deinit (tcu::TestCase* testCase)
{
	delete m_instance;
	m_instance = DE_NULL;

	if (m_renderDoc) m_renderDoc->endFrame(m_context->getInstance());

	// Collect and report any debug messages
#ifndef CTS_USES_VULKANSC
	if (m_context->hasDebugReportRecorder())
		collectAndReportDebugMessages(m_context->getDebugReportRecorder(), *m_context);
#endif // CTS_USES_VULKANSC

	if (testCase != DE_NULL)
		logUnusedShaders(testCase);

#ifdef CTS_USES_VULKANSC
	if (!m_context->getTestContext().getCommandLine().isSubProcess())
	{
		int currentSubprocessCount = getCurrentSubprocessCount(m_context->getResourceInterface()->getCasePath(), m_context->getTestContext().getCommandLine().getSubprocessTestCount());
		if (m_testsForSubprocess.size() >= std::size_t(currentSubprocessCount))
		{
			runTestsInSubprocess(m_context->getTestContext());

			// Clean up data after performing tests in subprocess and prepare system for another batch of tests
			m_testsForSubprocess.clear();
			const vk::DeviceInterface&				vkd						= m_context->getDeviceInterface();
			const vk::DeviceDriverSC*				dds						= dynamic_cast<const vk::DeviceDriverSC*>(&vkd);
			if (dds == DE_NULL)
				TCU_THROW(InternalError, "Undefined device driver for Vulkan SC");
			dds->reset();
			m_resourceInterface->resetObjects();

			suppressStandardOutput();
			m_context->getTestContext().getLog().supressLogging(true);
		}
	}
	else
	{
		bool faultFail = false;
		std::lock_guard<std::mutex> lock(Context::m_faultDataMutex);

		if (Context::m_faultData.size() != 0)
		{
			for (uint32_t i = 0; i < Context::m_faultData.size(); ++i)
			{
				m_context->getTestContext().getLog() << TestLog::Message << "Fault recorded via fault callback: " << Context::m_faultData[i] << TestLog::EndMessage;
				if (Context::m_faultData[i].faultLevel != VK_FAULT_LEVEL_WARNING)
					faultFail = true;
			}
			Context::m_faultData.clear();
		}

		const vk::DeviceInterface&				vkd						= m_context->getDeviceInterface();
		VkBool32 unrecordedFaults = VK_FALSE;
		uint32_t faultCount = 0;
		VkResult result = vkd.getFaultData(m_context->getDevice(), VK_FAULT_QUERY_BEHAVIOR_GET_AND_CLEAR_ALL_FAULTS, &unrecordedFaults, &faultCount, DE_NULL);
		if (result != VK_SUCCESS)
		{
			m_context->getTestContext().getLog() << TestLog::Message << "vkGetFaultData returned error: " << getResultName(result) << TestLog::EndMessage;
			faultFail = true;
		}
		if (faultCount != 0)
		{
			std::vector<VkFaultData> faultData(faultCount);
			for (uint32_t i = 0; i < faultCount; ++i)
			{
				faultData[i] = {};
				faultData[i].sType = VK_STRUCTURE_TYPE_FAULT_DATA;
			}
			result = vkd.getFaultData(m_context->getDevice(), VK_FAULT_QUERY_BEHAVIOR_GET_AND_CLEAR_ALL_FAULTS, &unrecordedFaults, &faultCount, faultData.data());
			if (result != VK_SUCCESS)
			{
				m_context->getTestContext().getLog() << TestLog::Message << "vkGetFaultData returned error: " << getResultName(result) << TestLog::EndMessage;
				faultFail = true;
			}
			for (uint32_t i = 0; i < faultCount; ++i)
			{
				m_context->getTestContext().getLog() << TestLog::Message << "Fault recorded via vkGetFaultData: " << faultData[i] << TestLog::EndMessage;
				if (Context::m_faultData[i].faultLevel != VK_FAULT_LEVEL_WARNING)
					faultFail = true;
			}
		}
		if (faultFail)
			m_context->getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fault occurred");
	}
#endif // CTS_USES_VULKANSC
}

void TestCaseExecutor::logUnusedShaders (tcu::TestCase* testCase)
{
	const qpTestResult	testResult	= testCase->getTestContext().getTestResult();

	if (testResult == QP_TEST_RESULT_PASS || testResult == QP_TEST_RESULT_QUALITY_WARNING || testResult == QP_TEST_RESULT_COMPATIBILITY_WARNING)
	{
		bool	unusedShaders	= false;

		for (vk::BinaryCollection::Iterator it = m_progCollection.begin(); it != m_progCollection.end(); ++it)
		{
			if (!it.getProgram().getUsed())
			{
				unusedShaders = true;

				break;
			}
		}

		if (unusedShaders)
		{
			std::string message;

			for (vk::BinaryCollection::Iterator it = m_progCollection.begin(); it != m_progCollection.end(); ++it)
			{
				if (!it.getProgram().getUsed())
					message += it.getName() + ",";
			}

			message.resize(message.size() - 1);

			message = std::string("Unused shaders: ") + message;

			m_context->getTestContext().getLog() << TestLog::Message << message << TestLog::EndMessage;
		}
	}
}

tcu::TestNode::IterateResult TestCaseExecutor::iterate (tcu::TestCase*)
{
	DE_ASSERT(m_instance);

	const tcu::TestStatus	result	= m_instance->iterate();

	if (result.isComplete())
	{
		// Vulkan tests shouldn't set result directly except when using a debug report messenger to catch validation errors.
		DE_ASSERT(m_context->getTestContext().getTestResult() == QP_TEST_RESULT_LAST || m_context->resultSetOnValidation());

		// Override result if not set previously by a debug report messenger.
		if (!m_context->resultSetOnValidation())
			m_context->getTestContext().setTestResult(result.getCode(), result.getDescription().c_str());
		return tcu::TestNode::STOP;
	}
	else
		return tcu::TestNode::CONTINUE;
}

void TestCaseExecutor::deinitTestPackage (tcu::TestContext& testCtx)
{
#ifdef CTS_USES_VULKANSC
	if (!testCtx.getCommandLine().isSubProcess())
	{
		if (!m_testsForSubprocess.empty())
		{
			runTestsInSubprocess(testCtx);

			// Clean up data after performing tests in subprocess and prepare system for another batch of tests
			m_testsForSubprocess.clear();
			const vk::DeviceInterface&				vkd						= m_context->getDeviceInterface();
			const vk::DeviceDriverSC*				dds						= dynamic_cast<const vk::DeviceDriverSC*>(&vkd);
			if (dds == DE_NULL)
				TCU_THROW(InternalError, "Undefined device driver for Vulkan SC");
			dds->reset();
			m_resourceInterface->resetObjects();
		}

		// Tests are finished. Next tests ( if any ) will come from other test package and test executor
		restoreStandardOutput();
		m_context->getTestContext().getLog().supressLogging(false);
	}
	m_resourceInterface->resetPipelineCaches();
#else
	DE_UNREF(testCtx);
#endif // CTS_USES_VULKANSC
}

bool TestCaseExecutor::usesLocalStatus ()
{
#ifdef CTS_USES_VULKANSC
	return !m_context->getTestContext().getCommandLine().isSubProcess();
#else
	return false;
#endif
}

void TestCaseExecutor::updateGlobalStatus (tcu::TestRunStatus& status)
{
	status.numExecuted					+= m_status.numExecuted;
	status.numPassed					+= m_status.numPassed;
	status.numNotSupported				+= m_status.numNotSupported;
	status.numWarnings					+= m_status.numWarnings;
	status.numWaived					+= m_status.numWaived;
	status.numFailed					+= m_status.numFailed;
	m_status.clear();
}

void TestCaseExecutor::reportDurations(tcu::TestContext& testCtx, const std::string& packageName, const deInt64& duration, const std::map<std::string, deUint64>& groupsDurationTime)
{
#ifdef CTS_USES_VULKANSC
	// Send it to server to append to its log
	vksc_server::AppendRequest request;
	request.fileName = testCtx.getCommandLine().getLogFileName();

	std::ostringstream str;

	str << std::endl;
	str << "#beginTestsCasesTime" << std::endl;

	str << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
	str << "<TestsCasesTime>" << std::endl;

	str << " <Number Name=\"" << packageName << "\" Description=\"Total tests case duration in microseconds\" Tag=\"Time\" Unit=\"us\">" << duration << "</Number>" << std::endl;
	for (std::map<std::string, deUint64>::const_iterator it = groupsDurationTime.begin(); it != groupsDurationTime.end(); ++it)
		str << " <Number Name=\"" << it->first << "\" Description=\"The test group case duration in microseconds\" Tag=\"Time\" Unit=\"us\">" << it->second << "</Number>" << std::endl;
	str << "</TestsCasesTime>" << std::endl;
	str << std::endl;
	str << "#endTestsCasesTime" << std::endl;
	str << std::endl;
	str << "#endSession" << std::endl;

	std::string output = str.str();
	request.data.assign(output.begin(), output.end());
	vksc_server::StandardOutputServerSingleton()->SendRequest(request);
#else
	DE_UNREF(testCtx);
	DE_UNREF(packageName);
	DE_UNREF(duration);
	DE_UNREF(groupsDurationTime);
#endif // CTS_USES_VULKANSC

}

int TestCaseExecutor::getCurrentSubprocessCount(const std::string& casePath, int defaultSubprocessCount)
{
#ifdef CTS_USES_VULKANSC
	for (const auto& detailed : m_detailedSubprocessTestCount)
		if (tcu::matchWildcards(detailed.testPattern.begin(), detailed.testPattern.end(), casePath.begin(), casePath.end(), false))
			return detailed.testCount;
#else
	DE_UNREF(casePath);
#endif // CTS_USES_VULKANSC
	return defaultSubprocessCount;
}

void TestCaseExecutor::runTestsInSubprocess (tcu::TestContext& testCtx)
{
#ifdef CTS_USES_VULKANSC
	if (testCtx.getCommandLine().isSubProcess())
		TCU_THROW(InternalError, "Cannot run subprocess inside subprocess : ");

	if (m_testsForSubprocess.empty())
		return;

	std::vector<int>	caseFraction	= testCtx.getCommandLine().getCaseFraction();
	std::ostringstream	jsonFileName, qpaFileName, pipelineCompilerOutFileName, pipelineCompilerLogFileName, pipelineCompilerPrefix;
	if (caseFraction.empty())
	{
		jsonFileName				<< "pipeline_data.txt";
		qpaFileName					<< "sub.qpa";
		if (!std::string(testCtx.getCommandLine().getPipelineCompilerPath()).empty())
		{
			pipelineCompilerOutFileName << "pipeline_cache.bin";
			pipelineCompilerLogFileName << "compiler.log";
			pipelineCompilerPrefix << "";
		}
	}
	else
	{
		jsonFileName	<< "pipeline_data_" << caseFraction[0] << ".txt";
		qpaFileName		<< "sub_" << caseFraction[0] << ".qpa";
		if (!std::string(testCtx.getCommandLine().getPipelineCompilerPath()).empty())
		{
			pipelineCompilerOutFileName << "pipeline_cache_" << caseFraction[0] <<".bin";
			pipelineCompilerLogFileName << "compiler_" << caseFraction[0] << ".log";
			pipelineCompilerPrefix << "sub_" << caseFraction[0] << "_";
		}
	}

	// export data collected during statistics gathering to JSON file ( VkDeviceObjectReservationCreateInfo, SPIR-V shaders, pipelines )
	{
		m_resourceInterface->removeRedundantObjects();
		m_resourceInterface->finalizeCommandBuffers();
		std::vector<deUint8>					data					= m_resourceInterface->exportData();
		m_parentIPC->SetFile(jsonFileName.str(), data);
	}

	// collect current application name, add it to new commandline with subprocess parameters
	std::string								newCmdLine;
	{
		std::string appName = testCtx.getCommandLine().getApplicationName();
		if (appName.empty())
			TCU_THROW(InternalError, "Application name is not defined");
		// add --deqp-subprocess option to inform deqp-vksc process that it works as slave process
		newCmdLine = appName + " --deqp-subprocess=enable --deqp-log-filename=" + qpaFileName.str();

		// add offline pipeline compiler parameters if present
		if (!std::string(testCtx.getCommandLine().getPipelineCompilerPath()).empty())
		{
			newCmdLine += " --deqp-pipeline-compiler="		+ std::string(testCtx.getCommandLine().getPipelineCompilerPath());
			newCmdLine += " --deqp-pipeline-file="			+ pipelineCompilerOutFileName.str();
			if (!std::string(testCtx.getCommandLine().getPipelineCompilerDataDir()).empty())
				newCmdLine += " --deqp-pipeline-dir="			+ std::string(testCtx.getCommandLine().getPipelineCompilerDataDir());
			newCmdLine += " --deqp-pipeline-logfile="		+ pipelineCompilerLogFileName.str();
			if(!pipelineCompilerPrefix.str().empty())
				newCmdLine += " --deqp-pipeline-prefix="	+ pipelineCompilerPrefix.str();
			if (!std::string(testCtx.getCommandLine().getPipelineCompilerArgs()).empty())
				newCmdLine += " --deqp-pipeline-args=\""	+ std::string( testCtx.getCommandLine().getPipelineCompilerArgs() ) + "\"";
		}
	}

	// collect parameters, remove parameters associated with case filter and case fraction. We will provide our own case list
	{
		std::string							originalCmdLine		= testCtx.getCommandLine().getInitialCmdLine();

		// brave ( but working ) assumption that each CTS parameter starts with "--deqp"

		std::string							paramStr			("--deqp");
		std::vector<std::string>			skipElements		=
		{
			"--deqp-case",
			"--deqp-stdin-caselist",
			"--deqp-log-filename",
			"--deqp-pipeline-compiler",
			"--deqp-pipeline-dir",
			"--deqp-pipeline-args",
			"--deqp-pipeline-file",
			"--deqp-pipeline-logfile",
			"--deqp-pipeline-prefix"
		};

		std::size_t							pos = 0;
		std::vector<std::size_t>			argPos;
		while ((pos = originalCmdLine.find(paramStr, pos)) != std::string::npos)
			argPos.push_back(pos++);
		if (!argPos.empty())
			argPos.push_back(originalCmdLine.size());

		std::vector<std::string> args;
		for (std::size_t i = 0; i < argPos.size()-1; ++i)
		{
			std::string s = originalCmdLine.substr(argPos[i], argPos[i + 1] - argPos[i]);
			std::size_t found = s.find_last_not_of(' ');
			if (found != std::string::npos)
			{
				s.erase(found + 1);
				args.push_back(s);
			}
		}
		for (std::size_t i = 0; i < args.size(); ++i)
		{
			bool skipElement = false;
			for (const auto& elem : skipElements)
				if (args[i].find(elem) == 0)
				{
					skipElement = true;
					break;
				}
			if (skipElement)
				continue;
			newCmdLine = newCmdLine + " " + args[i];
		}
	}

	// create --deqp-case list from tests collected in m_testsForSubprocess
	std::string subprocessTestList;
	for (auto it = begin(m_testsForSubprocess); it != end(m_testsForSubprocess); ++it)
	{
		auto nit = it; ++nit;

		subprocessTestList += *it;
		if (nit != end(m_testsForSubprocess))
			subprocessTestList += "\n";
	}

	std::string caseListName	= "subcaselist" + (caseFraction.empty() ? std::string("") : de::toString(caseFraction[0])) + ".txt";

	deFile*		exportFile		= deFile_create(caseListName.c_str(), DE_FILEMODE_CREATE | DE_FILEMODE_OPEN | DE_FILEMODE_WRITE | DE_FILEMODE_TRUNCATE);
	deInt64		numWritten		= 0;
	deFile_write(exportFile, subprocessTestList.c_str(), subprocessTestList.size(), &numWritten);
	deFile_destroy(exportFile);
	newCmdLine = newCmdLine + " --deqp-caselist-file=" + caseListName;

	// restore cout and cerr
	restoreStandardOutput();

	// create subprocess which will perform real tests
	std::string subProcessExitCodeInfo;
	{
		deProcess*	process			= deProcess_create();
		if (deProcess_start(process, newCmdLine.c_str(), ".") != DE_TRUE)
		{
			std::string err = deProcess_getLastError(process);
			deProcess_destroy(process);
			process = DE_NULL;
			TCU_THROW(InternalError, "Error while running subprocess : " + err);
		}
		std::string whole;
		whole.reserve(1024 * 4);

		// create a separate thread that captures std::err output
		de::MovePtr<std::thread> errThread(new std::thread([&process]
		{
			deFile*		subErr = deProcess_getStdErr(process);
			char		errBuffer[128]	= { 0 };
			deInt64		errNumRead		= 0;
			while (deFile_read(subErr, errBuffer, sizeof(errBuffer) - 1, &errNumRead) == DE_FILERESULT_SUCCESS)
			{
				errBuffer[errNumRead] = 0;
			}
		}));

		deFile*		subOutput		= deProcess_getStdOut(process);
		char		outBuffer[128]	= { 0 };
		deInt64		numRead			= 0;
		while (deFile_read(subOutput, outBuffer, sizeof(outBuffer) - 1, &numRead) == DE_FILERESULT_SUCCESS)
		{
			outBuffer[numRead] = 0;
			qpPrint(outBuffer);
			whole += outBuffer;
		}
		errThread->join();
		if (deProcess_waitForFinish(process))
		{
			const int			exitCode = deProcess_getExitCode(process);
			std::stringstream	s;

			s << " Subprocess failed with exit code " << exitCode << "(" << std::hex << exitCode << ")";

			subProcessExitCodeInfo = s.str();
		}
		deProcess_destroy(process);

		vksc_server::RemoteWrite(0, whole.c_str());
	}

	// copy test information from sub.qpa to main log
	{
		std::ifstream	subQpa(qpaFileName.str(), std::ios::binary);
		std::string		subQpaText{std::istreambuf_iterator<char>(subQpa),
								   std::istreambuf_iterator<char>()};
		{
			std::string			beginText		("#beginTestCaseResult");
			std::string			endText			("#endTestCaseResult");
			std::size_t			beginPos		= subQpaText.find(beginText);
			std::size_t			endPos			= subQpaText.rfind(endText);
			if (beginPos == std::string::npos || endPos == std::string::npos)
				TCU_THROW(InternalError, "Couldn't match tags from " + qpaFileName.str() + subProcessExitCodeInfo);

			std::string		subQpaCopy = "\n" + std::string(subQpaText.begin() + beginPos, subQpaText.begin() + endPos + endText.size()) + "\n";

			if (!std::string(testCtx.getCommandLine().getServerAddress()).empty())
			{
				// Send it to server to append to its log
				vksc_server::AppendRequest request;
				request.fileName = testCtx.getCommandLine().getLogFileName();
				request.data.assign(subQpaCopy.begin(), subQpaCopy.end());
				vksc_server::StandardOutputServerSingleton()->SendRequest(request);
			}
			else
			{
				// Write it to parent's log
				try
				{
					testCtx.getLog().supressLogging(false);
					testCtx.getLog().writeRaw(subQpaCopy.c_str());
				}
				catch(...)
				{
					testCtx.getLog().supressLogging(true);
					throw;
				}
				testCtx.getLog().supressLogging(true);
			}
		}

		{
			std::string			beginStat		("#SubProcessStatus");
			std::size_t			beginPos		= subQpaText.find(beginStat);
			if (beginPos == std::string::npos)
				TCU_THROW(InternalError, "Couldn't match #SubProcessStatus tag from " + qpaFileName.str() + subProcessExitCodeInfo);

			std::string			subQpaStat		(subQpaText.begin() + beginPos + beginStat.size(), subQpaText.end());

			std::istringstream	str(subQpaStat);
			int					numExecuted, numPassed, numFailed, numNotSupported, numWarnings, numWaived;
			str >> numExecuted >> numPassed >> numFailed >> numNotSupported >> numWarnings >> numWaived;

			m_status.numExecuted				+= numExecuted;
			m_status.numPassed					+= numPassed;
			m_status.numNotSupported			+= numNotSupported;
			m_status.numWarnings				+= numWarnings;
			m_status.numWaived					+= numWaived;
			m_status.numFailed					+= numFailed;
		}

		deDeleteFile(qpaFileName.str().c_str());
	}
#else
	DE_UNREF(testCtx);
#endif // CTS_USES_VULKANSC
}

bool TestCaseExecutor::spirvVersionSupported (vk::SpirvVersion spirvVersion)
{
	if (spirvVersion <= vk::getMaxSpirvVersionForVulkan(m_context->getUsedApiVersion()))
		return true;

	if (spirvVersion <= vk::SPIRV_VERSION_1_4)
		return m_context->isDeviceFunctionalitySupported("VK_KHR_spirv_1_4");

	return false;
}

// GLSL shader tests

void createGlslTests (tcu::TestCaseGroup* glslTests)
{
	tcu::TestContext&	testCtx		= glslTests->getTestContext();

	// ShaderLibrary-based tests
	static const struct
	{
		const char*		name;
		const char*		description;
	} s_es310Tests[] =
	{
		{ "arrays",						"Arrays"					},
		{ "conditionals",				"Conditional statements"	},
		{ "constant_expressions",		"Constant expressions"		},
		{ "constants",					"Constants"					},
		{ "conversions",				"Type conversions"			},
		{ "functions",					"Functions"					},
		{ "linkage",					"Linking"					},
		{ "scoping",					"Scoping"					},
		{ "swizzles",					"Swizzles"					},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_es310Tests); ndx++)
		glslTests->addChild(createShaderLibraryGroup(testCtx,
													 s_es310Tests[ndx].name,
													 s_es310Tests[ndx].description,
													 std::string("vulkan/glsl/es310/") + s_es310Tests[ndx].name + ".test").release());

	static const struct
	{
		const char*		name;
		const char*		description;
	} s_440Tests[] =
	{
		{ "linkage",					"Linking"					},
	};

	de::MovePtr<tcu::TestCaseGroup> glsl440Tests = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "440", ""));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_440Tests); ndx++)
		glsl440Tests->addChild(createShaderLibraryGroup(testCtx,
													 s_440Tests[ndx].name,
													 s_440Tests[ndx].description,
													 std::string("vulkan/glsl/440/") + s_440Tests[ndx].name + ".test").release());

	glslTests->addChild(glsl440Tests.release());

	// ShaderRenderCase-based tests
	glslTests->addChild(sr::createDerivateTests			(testCtx));
	glslTests->addChild(sr::createDiscardTests			(testCtx));
#ifndef CTS_USES_VULKANSC
	glslTests->addChild(sr::createDemoteTests			(testCtx));
#endif // CTS_USES_VULKANSC
	glslTests->addChild(sr::createIndexingTests			(testCtx));
	glslTests->addChild(sr::createShaderInvarianceTests	(testCtx));
	glslTests->addChild(sr::createLimitTests			(testCtx));
	glslTests->addChild(sr::createLoopTests				(testCtx));
	glslTests->addChild(sr::createMatrixTests			(testCtx));
	glslTests->addChild(sr::createOperatorTests			(testCtx));
	glslTests->addChild(sr::createReturnTests			(testCtx));
	glslTests->addChild(sr::createStructTests			(testCtx));
	glslTests->addChild(sr::createSwitchTests			(testCtx));
	glslTests->addChild(sr::createTextureFunctionTests	(testCtx));
	glslTests->addChild(sr::createTextureGatherTests	(testCtx));
	glslTests->addChild(sr::createBuiltinVarTests		(testCtx));

	// ShaderExecutor-based tests
	glslTests->addChild(shaderexecutor::createBuiltinTests				(testCtx));
	glslTests->addChild(shaderexecutor::createOpaqueTypeIndexingTests	(testCtx));
	glslTests->addChild(shaderexecutor::createAtomicOperationTests		(testCtx));
	glslTests->addChild(shaderexecutor::createShaderClockTests			(testCtx));

#ifndef CTS_USES_VULKANSC
	// Amber GLSL tests.
	glslTests->addChild(cts_amber::createCombinedOperationsGroup		(testCtx));
	glslTests->addChild(cts_amber::createCrashTestGroup					(testCtx));
#endif // CTS_USES_VULKANSC
}

// TestPackage

BaseTestPackage::BaseTestPackage (tcu::TestContext& testCtx, const char* name, const char* desc)
	: tcu::TestPackage(testCtx, name, desc)
{
}

BaseTestPackage::~BaseTestPackage (void)
{
}

#ifdef CTS_USES_VULKAN

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: BaseTestPackage(testCtx, "dEQP-VK", "dEQP Vulkan Tests")
{
}

TestPackage::~TestPackage (void)
{
}

ExperimentalTestPackage::ExperimentalTestPackage (tcu::TestContext& testCtx)
	: BaseTestPackage(testCtx, "dEQP-VK-experimental", "dEQP Vulkan Experimental Tests")
{
}

ExperimentalTestPackage::~ExperimentalTestPackage (void)
{
}

#endif

#ifdef CTS_USES_VULKANSC

TestPackageSC::TestPackageSC (tcu::TestContext& testCtx)
	: BaseTestPackage(testCtx, "dEQP-VKSC", "dEQP Vulkan SC Tests")
{
}

TestPackageSC::~TestPackageSC (void)
{
}

#endif // CTS_USES_VULKANSC

tcu::TestCaseExecutor* BaseTestPackage::createExecutor (void) const
{
	return new TestCaseExecutor(m_testCtx);
}

tcu::TestCaseGroup* createGlslTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name, "GLSL shader execution tests", createGlslTests);
}

#ifdef CTS_USES_VULKAN

void TestPackage::init (void)
{
	addRootChild("info", m_caseListFilter,							info::createTests);
	addRootChild("api", m_caseListFilter,							api::createTests);
	addRootChild("memory", m_caseListFilter,						memory::createTests);
	addRootChild("pipeline", m_caseListFilter,						pipeline::createTests);
	addRootChild("binding_model", m_caseListFilter,					BindingModel::createTests);
	addRootChild("spirv_assembly", m_caseListFilter,				SpirVAssembly::createTests);
	addRootChild("glsl", m_caseListFilter,							createGlslTests);
	addRootChild("renderpass", m_caseListFilter,					createRenderPassTests);
	addRootChild("renderpass2", m_caseListFilter,					createRenderPass2Tests);
	addRootChild("dynamic_rendering", m_caseListFilter,				createDynamicRenderingTests);
	addRootChild("ubo", m_caseListFilter,							ubo::createTests);
	addRootChild("dynamic_state", m_caseListFilter,					DynamicState::createTests);
	addRootChild("ssbo", m_caseListFilter,							ssbo::createTests);
	addRootChild("query_pool", m_caseListFilter,					QueryPool::createTests);
	addRootChild("draw", m_caseListFilter,							Draw::createTests);
	addRootChild("compute", m_caseListFilter,						compute::createTests);
	addRootChild("image", m_caseListFilter,							image::createTests);
	addRootChild("wsi", m_caseListFilter,							wsi::createTests);
	addRootChild("synchronization", m_caseListFilter,				createSynchronizationTests);
	addRootChild("synchronization2", m_caseListFilter,				createSynchronization2Tests);
	addRootChild("sparse_resources", m_caseListFilter,				sparse::createTests);
	addRootChild("tessellation", m_caseListFilter,					tessellation::createTests);
	addRootChild("rasterization", m_caseListFilter,					rasterization::createTests);
	addRootChild("clipping", m_caseListFilter,						clipping::createTests);
	addRootChild("fragment_operations", m_caseListFilter,			FragmentOperations::createTests);
	addRootChild("texture", m_caseListFilter,						texture::createTests);
	addRootChild("geometry", m_caseListFilter,						geometry::createTests);
	addRootChild("robustness", m_caseListFilter,					robustness::createTests);
	addRootChild("multiview", m_caseListFilter,						MultiView::createTests);
	addRootChild("subgroups", m_caseListFilter,						subgroups::createTests);
	addRootChild("ycbcr", m_caseListFilter,							ycbcr::createTests);
	addRootChild("protected_memory", m_caseListFilter,				ProtectedMem::createTests);
	addRootChild("device_group", m_caseListFilter,					DeviceGroup::createTests);
	addRootChild("memory_model", m_caseListFilter,					MemoryModel::createTests);
	addRootChild("conditional_rendering", m_caseListFilter,			conditional::createTests);
	addRootChild("graphicsfuzz", m_caseListFilter,					cts_amber::createGraphicsFuzzTests);
	addRootChild("imageless_framebuffer", m_caseListFilter,			imageless::createTests);
	addRootChild("transform_feedback", m_caseListFilter,			TransformFeedback::createTests);
	addRootChild("descriptor_indexing", m_caseListFilter,			DescriptorIndexing::createTests);
	addRootChild("fragment_shader_interlock", m_caseListFilter,		FragmentShaderInterlock::createTests);
	addRootChild("drm_format_modifiers", m_caseListFilter,			modifiers::createTests);
	addRootChild("ray_tracing_pipeline", m_caseListFilter,			RayTracing::createTests);
	addRootChild("ray_query", m_caseListFilter,						RayQuery::createTests);
	addRootChild("fragment_shading_rate", m_caseListFilter,			FragmentShadingRate::createTests);
	addRootChild("reconvergence", m_caseListFilter,					Reconvergence::createTests);
	addRootChild("mesh_shader", m_caseListFilter,					MeshShader::createTests);
	addRootChild("fragment_shading_barycentric", m_caseListFilter,	FragmentShadingBarycentric::createTests);
	// Amber depth pipeline tests
	addRootChild("depth", m_caseListFilter,							cts_amber::createAmberDepthGroup);
	addRootChild("video", m_caseListFilter,							video::createTests);
	addRootChild("shader_object", m_caseListFilter,					ShaderObject::createTests);
}

void ExperimentalTestPackage::init (void)
{
	addRootChild("postmortem", m_caseListFilter,					postmortem::createTests);
	addRootChild("reconvergence", m_caseListFilter,					Reconvergence::createTestsExperimental);
}

#endif

#ifdef CTS_USES_VULKANSC

void TestPackageSC::init (void)
{
	addRootChild("info", m_caseListFilter,						info::createTests);
	addRootChild("api", m_caseListFilter,						api::createTests);
	addRootChild("memory", m_caseListFilter,					memory::createTests);
	addRootChild("pipeline", m_caseListFilter,					pipeline::createTests);
	addRootChild("binding_model", m_caseListFilter,				BindingModel::createTests);
	addRootChild("spirv_assembly", m_caseListFilter,			SpirVAssembly::createTests);
	addRootChild("glsl", m_caseListFilter,						createGlslTests);
	addRootChild("renderpass", m_caseListFilter,				createRenderPassTests);
	addRootChild("renderpass2", m_caseListFilter,				createRenderPass2Tests);
	addRootChild("ubo", m_caseListFilter,						ubo::createTests);
	addRootChild("dynamic_state", m_caseListFilter,				DynamicState::createTests);
	addRootChild("ssbo", m_caseListFilter,						ssbo::createTests);
	addRootChild("query_pool", m_caseListFilter,				QueryPool::createTests);
	addRootChild("draw", m_caseListFilter,						Draw::createTests);
	addRootChild("compute", m_caseListFilter,					compute::createTests);
	addRootChild("image", m_caseListFilter,						image::createTests);
//	addRootChild("wsi", m_caseListFilter,						wsi::createTests);
	addRootChild("synchronization", m_caseListFilter,			createSynchronizationTests);
	addRootChild("synchronization2", m_caseListFilter,			createSynchronization2Tests);
//	addRootChild("sparse_resources", m_caseListFilter,			sparse::createTests);
	addRootChild("tessellation", m_caseListFilter,				tessellation::createTests);
	addRootChild("rasterization", m_caseListFilter,				rasterization::createTests);
	addRootChild("clipping", m_caseListFilter,					clipping::createTests);
	addRootChild("fragment_operations", m_caseListFilter,		FragmentOperations::createTests);
	addRootChild("texture", m_caseListFilter,					texture::createTests);
	addRootChild("geometry", m_caseListFilter,					geometry::createTests);
	addRootChild("robustness", m_caseListFilter,				robustness::createTests);
	addRootChild("multiview", m_caseListFilter,					MultiView::createTests);
	addRootChild("subgroups", m_caseListFilter,					subgroups::createTests);
	addRootChild("ycbcr", m_caseListFilter,						ycbcr::createTests);
	addRootChild("protected_memory", m_caseListFilter,			ProtectedMem::createTests);
	addRootChild("device_group", m_caseListFilter,				DeviceGroup::createTests);
	addRootChild("memory_model", m_caseListFilter,				MemoryModel::createTests);
//	addRootChild("conditional_rendering", m_caseListFilter,		conditional::createTests);
//	addRootChild("graphicsfuzz", m_caseListFilter,				cts_amber::createGraphicsFuzzTests);
	addRootChild("imageless_framebuffer", m_caseListFilter,		imageless::createTests);
//	addRootChild("transform_feedback", m_caseListFilter,		TransformFeedback::createTests);
	addRootChild("descriptor_indexing", m_caseListFilter,		DescriptorIndexing::createTests);
	addRootChild("fragment_shader_interlock", m_caseListFilter,	FragmentShaderInterlock::createTests);
//	addRootChild("drm_format_modifiers", m_caseListFilter,		modifiers::createTests);
//	addRootChild("ray_tracing_pipeline", m_caseListFilter,		RayTracing::createTests);
//	addRootChild("ray_query", m_caseListFilter,					RayQuery::createTests);
	addRootChild("fragment_shading_rate", m_caseListFilter,		FragmentShadingRate::createTests);
	addChild(sc::createTests(m_testCtx));
}

#endif // CTS_USES_VULKANSC

} // vkt
